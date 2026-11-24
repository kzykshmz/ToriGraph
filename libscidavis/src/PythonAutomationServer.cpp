/***************************************************************************
    File                 : PythonAutomationServer.cpp
    Project              : ToriGraph (SciDAVis)
    --------------------------------------------------------------------
    Description          : Headless JSON-RPC 2.0 Automation Server (TCP &
                           Named Pipe) and In-App Python Automation Console
 ***************************************************************************/
#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "PythonAutomationServer.h"
#include "PythonAutomationServer_Internal.h"

#ifdef _MSC_VER
#pragma warning(disable : 4267 4373)
#endif

#include "AdvancedSignalProc.h"
#include "ApplicationWindow.h"
#include "ArrowMarker.h"
#include "AutoUpdater.h"
#include "BoxCurve.h"
#include "CurveMathTools.h"
#include "ExtraPlotTypes.h"
#include "Differentiation.h"
#include "FFT.h"
#include "FFTFilter.h"
#include "Folder.h"
#include "FunctionCurve.h"
#include "Graph.h"
#include "Graph3D.h"
#include "GraphDigitizer.h"
#include "GraphInspectorDock.h"
#include "GraphStatsAndSurfaceFit.h"
#include "Grid.h"
#include "ImportOPJU.h"
#include "XlsxImport.h"
#include "Workbook.h"
#include "Integration.h"
#include "Interpolation.h"
#include "Legend.h"
#include "Matrix.h"
#include "ModernRibbonAndHud.h"
#include "MultiLayer.h"
#include "MultiPeakFit.h"
#include "MyParser.h"
#include "NonLinearFit.h"
#include "Note.h"
#include "OriginGadgets.h"
#include "PiecewiseAndRobustFit.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "PolynomialFit.h"
#include "QwtBarCurve.h"
#include "QwtErrorPlotCurve.h"
#include "QwtHistogram.h"
#include "QwtPieCurve.h"
#include "RecalcEngine.h"
#include "ScaleDraw.h"
#include "SmoothFilter.h"
#include "Spectrogram.h"
#include "Table.h"
#include "VectorCurve.h"
#include "globals.h"

#include "core/column/Column.h"
#include "core/datatypes/Double2StringFilter.h"
#include "lib/Interval.h"
#include "matrix/future_Matrix.h"
#include "table/AsciiTableImportFilter.h"
#include "table/future_Table.h"

#ifdef Q_OS_WIN
#include "EmfPaintDevice.h"
#include "OleServer.h"
#include <ole2.h>
#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>
#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#endif
#endif

#ifdef ORIGIN_IMPORT
#include "importOPJ.h"
#endif

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>

#include <gsl/gsl_cdf.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_statistics.h>

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QMimeData>
#include <QMetaMethod>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableView>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTextCharFormat>
#include <QTextCodec>
#include <QTextCursor>
#include <QTextDecoder>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <vector>

using namespace rpc_internal;

namespace {

int parseStrictPositiveInt(const QJsonObject &params, const QString &key, int defaultVal)
{
    if (!params.contains(key) || params.value(key).isUndefined() || params.value(key).isNull())
        return defaultVal;
    const int v = parseStrictJsonInt(params.value(key), key);
    if (v <= 0) {
        throwInvalidParams(
                QStringLiteral("Parameter '%1' must be a positive integer (>= 1), got %2.")
                        .arg(key)
                        .arg(v));
    }
    return v;
}

QJsonValue sanitizeJsonValueSurrogates(const QJsonValue &v)
{
    if (v.isString())
        return QJsonValue(sanitizeUtf16Surrogates(v.toString()));
    if (v.isArray()) {
        const QJsonArray src = v.toArray();
        QJsonArray dst;
        for (const QJsonValue &it : src)
            dst.append(sanitizeJsonValueSurrogates(it));
        return dst;
    }
    if (v.isObject()) {
        const QJsonObject src = v.toObject();
        QJsonObject dst;
        for (auto it = src.begin(); it != src.end(); ++it)
            dst.insert(sanitizeUtf16Surrogates(it.key()), sanitizeJsonValueSurrogates(it.value()));
        return dst;
    }
    return v;
}

// The same size as Python source text for to_python_script ("13" or "12.9").
QString fontPtPy(const QFont &f, double minPt)
{
    double pt = f.pointSizeF();
    if (!(pt > 0.0))
        pt = f.pointSize();
    pt = std::max(minPt, std::round(pt * 100.0) / 100.0);
    return QString::number(pt, 'g', 6);
}

QByteArray gzipCompressBytes(const QByteArray &raw)
{
    z_stream zs{};
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY)
        != Z_OK) {
        throwRpcRuntime(QStringLiteral("zlib deflateInit2 failed while compressing project."));
    }
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(raw.constData()));
    zs.avail_in = static_cast<uInt>(raw.size());

    QByteArray out;
    out.reserve(std::max(128, raw.size() / 2));
    char chunk[32768];
    int ret = Z_OK;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(chunk);
        zs.avail_out = sizeof(chunk);
        ret = deflate(&zs, zs.avail_in > 0 ? Z_NO_FLUSH : Z_FINISH);
        if (ret == Z_STREAM_ERROR || ret == Z_MEM_ERROR) {
            deflateEnd(&zs);
            throwRpcRuntime(QStringLiteral("zlib deflate failed while compressing project."));
        }
        const int have = static_cast<int>(sizeof(chunk) - zs.avail_out);
        if (have > 0)
            out.append(chunk, have);
    } while (ret != Z_STREAM_END);

    deflateEnd(&zs);
    return out;
}

QJsonArray parseCompactParamsSignature(const char *sigCstr)
{
    QJsonArray arr;
    const QString sig = QString::fromUtf8(sigCstr ? sigCstr : "").trimmed();
    if (sig.isEmpty())
        return arr;
    const QStringList parts = sig.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &rawPart : parts) {
        const QString item = rawPart.trimmed();
        const int colonPos = item.indexOf(QLatin1Char(':'));
        if (colonPos <= 0)
            continue;
        const QString pName = item.left(colonPos).trimmed();
        QString rest = item.mid(colonPos + 1).trimmed();

        QStringList enumVals;
        const int brOpen = rest.indexOf(QLatin1Char('['));
        const int brClose = rest.lastIndexOf(QLatin1Char(']'));
        if (brOpen >= 0 && brClose > brOpen) {
            const QString inside = rest.mid(brOpen + 1, brClose - brOpen - 1);
            if (inside.contains(QLatin1Char('|'))) {
                enumVals = inside.split(QLatin1Char('|'), Qt::SkipEmptyParts);
                rest.remove(brOpen, brClose - brOpen + 1);
                rest = rest.trimmed();
            }
        }

        bool required = false;
        QString defStr;
        bool hasDefault = false;
        const int eqPos = rest.indexOf(QLatin1Char('='));
        QString pType;
        if (eqPos >= 0) {
            pType = rest.left(eqPos).trimmed();
            defStr = rest.mid(eqPos + 1).trimmed();
            if (defStr.size() >= 2 && defStr.startsWith(QLatin1Char('\''))
                && defStr.endsWith(QLatin1Char('\''))) {
                defStr = defStr.mid(1, defStr.size() - 2);
            }
            hasDefault = true;
        } else {
            pType = rest;
        }
        if (pType.endsWith(QLatin1Char('*'))) {
            required = true;
            pType.chop(1);
            pType = pType.trimmed();
        }

        QJsonObject pObj;
        pObj[QStringLiteral("name")] = pName;
        pObj[QStringLiteral("type")] = pType;
        pObj[QStringLiteral("required")] = required;
        if (hasDefault && !required) {
            if (defStr == QStringLiteral("null"))
                pObj[QStringLiteral("default")] = QJsonValue::Null;
            else if (defStr == QStringLiteral("true"))
                pObj[QStringLiteral("default")] = true;
            else if (defStr == QStringLiteral("false"))
                pObj[QStringLiteral("default")] = false;
            else {
                bool okInt = false;
                const int iv = defStr.toInt(&okInt);
                if (okInt) {
                    pObj[QStringLiteral("default")] = iv;
                } else {
                    bool okDbl = false;
                    const double dv = defStr.toDouble(&okDbl);
                    if (okDbl && std::isfinite(dv))
                        pObj[QStringLiteral("default")] = dv;
                    else
                        pObj[QStringLiteral("default")] = defStr;
                }
            }
        }
        if (!enumVals.isEmpty())
            pObj[QStringLiteral("enum")] = toJsonArray(enumVals);
        arr.append(pObj);
    }
    return arr;
}

QString pyEscapeStr(const QString &raw)
{
    const QString s = sanitizeUtf16Surrogates(raw);
    QString out;
    out.reserve(s.size() + 8);
    out.append(QLatin1Char('\''));
    for (int i = 0; i < s.size(); ++i) {
        const ushort u = s.at(i).unicode();
        if (u == '\\')
            out.append(QStringLiteral("\\\\"));
        else if (u == '\'')
            out.append(QStringLiteral("\\'"));
        else if (u == '\n')
            out.append(QStringLiteral("\\n"));
        else if (u == '\r')
            out.append(QStringLiteral("\\r"));
        else if (u == '\t')
            out.append(QStringLiteral("\\t"));
        else if (u < 0x20 || u == 0x7F || (u >= 0x80 && u <= 0x9F))
            out.append(QStringLiteral("\\x%1").arg(u, 2, 16, QLatin1Char('0')));
        else if (u == 0x2028 || u == 0x2029)
            out.append(QStringLiteral("\\u%1").arg(u, 4, 16, QLatin1Char('0')));
        else
            out.append(s.at(i));
    }
    out.append(QLatin1Char('\''));
    return out;
}

QString pyFormatNum(double v)
{
    if (!std::isfinite(v))
        return QStringLiteral("None");
    if (v == 0.0)
        return QStringLiteral("0.0");
    return QString::number(v, 'g', 12);
}

} // namespace

namespace rpc_internal {

[[noreturn]] void throwInvalidParamsWithAvailable(const QString &msg,
                                                  const QStringList &available,
                                                  const QJsonObject &extraData)
{
    QJsonArray availArr;
    for (const QString &s : available)
        availArr.append(s);
    QJsonObject dataObj = extraData;
    dataObj[QStringLiteral("available")] = availArr;
    QString fullMsg = msg;
    if (!available.isEmpty()) {
        const QStringList preview = available.mid(0, 24);
        QString joined = preview.join(QStringLiteral(", "));
        if (available.size() > preview.size())
            joined += QStringLiteral(", ...");
        fullMsg += QStringLiteral(" Available: [%1]").arg(joined);
    } else {
        fullMsg += QStringLiteral(" Available: []");
    }
    throw RpcDispatchException(-32602, fullMsg, dataObj);
}

QString sanitizeUtf16Surrogates(const QString &in)
{
    bool needsFix = false;
    for (int i = 0; i < in.size(); ++i) {
        const QChar ch = in.at(i);
        if (ch.isHighSurrogate()) {
            if (i + 1 < in.size() && in.at(i + 1).isLowSurrogate()) {
                ++i;
            } else {
                needsFix = true;
                break;
            }
        } else if (ch.isLowSurrogate()) {
            needsFix = true;
            break;
        }
    }
    if (!needsFix)
        return in;

    QString out;
    out.reserve(in.size());
    for (int i = 0; i < in.size(); ++i) {
        const QChar ch = in.at(i);
        if (ch.isHighSurrogate()) {
            if (i + 1 < in.size() && in.at(i + 1).isLowSurrogate()) {
                out.append(ch);
                out.append(in.at(i + 1));
                ++i;
            } else {
                out.append(QChar::ReplacementCharacter);
            }
        } else if (ch.isLowSurrogate()) {
            out.append(QChar::ReplacementCharacter);
        } else {
            out.append(ch);
        }
    }
    return out;
}

BatchExecutionContext &rpcBatchContext()
{
    static BatchExecutionContext s_ctx;
    return s_ctx;
}

QColor parseColorValue(const QJsonValue &val, const QColor &fallback)
{
    if (val.isUndefined() || val.isNull())
        return fallback;
    if (val.isString()) {
        const QString s = val.toString().trimmed();
        if (s.isEmpty())
            return fallback;
        QColor c(s);
        if (c.isValid())
            return c;
        throwInvalidParams(QStringLiteral("Invalid color specification '%1'.").arg(s));
    }
    if (val.isArray()) {
        const QJsonArray a = val.toArray();
        if (a.size() < 3 || a.size() > 4) {
            throwInvalidParams(
                    QStringLiteral("Color array must have 3 ([r, g, b]) or 4 ([r, g, b, a]) numeric elements."));
        }
        for (int i = 0; i < a.size(); ++i) {
            if (!a[i].isDouble() || a[i].isBool() || !std::isfinite(a[i].toDouble())) {
                throwInvalidParams(
                        QStringLiteral("Color array elements must be finite numbers in [0, 255]."));
            }
        }
        const int r = std::clamp(qRound(a[0].toDouble()), 0, 255);
        const int g = std::clamp(qRound(a[1].toDouble()), 0, 255);
        const int b = std::clamp(qRound(a[2].toDouble()), 0, 255);
        const int al = (a.size() >= 4) ? std::clamp(qRound(a[3].toDouble()), 0, 255) : 255;
        return QColor(r, g, b, al);
    }
    throwInvalidParams(
            QStringLiteral("Color parameter must be a color string or [r, g, b] array."));
}

Qt::PenStyle parsePenStyleValue(const QJsonValue &val, Qt::PenStyle fallback)
{
    if (val.isUndefined() || val.isNull())
        return fallback;
    if (val.isDouble() && !val.isBool()) {
        const int s = parseStrictJsonInt(val, QStringLiteral("line_style"));
        if (s >= int(Qt::NoPen) && s <= int(Qt::DashDotDotLine))
            return static_cast<Qt::PenStyle>(s);
        throwInvalidParamsWithAvailable(
                QStringLiteral("Unknown integer line_style %1.").arg(s),
                { QStringLiteral("solid"), QStringLiteral("dash"), QStringLiteral("dot"),
                  QStringLiteral("dashdot"), QStringLiteral("dashdotdot"),
                  QStringLiteral("none") });
    }
    if (val.isString()) {
        const QString raw = val.toString().trimmed();
        if (raw.isEmpty())
            return fallback;
        const QString s = raw.toLower();
        if (s == QStringLiteral("none") || s == QStringLiteral("nopen")
            || s == QStringLiteral("off") || s == QStringLiteral("blank"))
            return Qt::NoPen;
        if (s == QStringLiteral("-") || s == QStringLiteral("solid")
            || s == QStringLiteral("solidline") || s == QStringLiteral("solid_line"))
            return Qt::SolidLine;
        if (s == QStringLiteral("--") || s == QStringLiteral("dash")
            || s == QStringLiteral("dashed") || s == QStringLiteral("dashline")
            || s == QStringLiteral("dash_line"))
            return Qt::DashLine;
        if (s == QStringLiteral(":") || s == QStringLiteral("dot") || s == QStringLiteral("dotted")
            || s == QStringLiteral("dotline") || s == QStringLiteral("dot_line"))
            return Qt::DotLine;
        if (s == QStringLiteral("-.") || s == QStringLiteral("dashdot")
            || s == QStringLiteral("dash_dot") || s == QStringLiteral("dash-dot")
            || s == QStringLiteral("dashdotline"))
            return Qt::DashDotLine;
        if (s == QStringLiteral("..-") || s == QStringLiteral("-..")
            || s == QStringLiteral("dashdotdot") || s == QStringLiteral("dash_dot_dot")
            || s == QStringLiteral("dash-dot-dot") || s == QStringLiteral("dashdotdotline"))
            return Qt::DashDotDotLine;
        static const QStringList kPenStyles = {
            QStringLiteral("-"),          QStringLiteral("--"),      QStringLiteral(":"),
            QStringLiteral("-."),         QStringLiteral("solid"),   QStringLiteral("dash"),
            QStringLiteral("dot"),        QStringLiteral("dashdot"), QStringLiteral("dashdotdot"),
            QStringLiteral("none")
        };
        throwInvalidParamsWithAvailable(
                QStringLiteral("Unknown line style '%1'.").arg(raw), kPenStyles);
    }
    throwInvalidParams(QStringLiteral("Line style must be a string or integer code."));
}

QString windowTypeString(MyWidget *w)
{
    if (!w)
        return QStringLiteral("unknown");
    if (w->inherits("Table"))
        return QStringLiteral("Table");
    if (w->inherits("Matrix"))
        return QStringLiteral("Matrix");
    if (w->inherits("MultiLayer"))
        return QStringLiteral("MultiLayer");
    if (w->inherits("Graph3D"))
        return QStringLiteral("Graph3D");
    if (w->inherits("Note"))
        return QStringLiteral("Note");
    return QString::fromLatin1(w->metaObject()->className());
}

QStringList collectWindowNames(ApplicationWindow *app, const char *inheritsClass)
{
    QStringList out;
    if (!app)
        return out;
    for (MyWidget *w : app->windowsList()) {
        if (!w)
            continue;
        if (!inheritsClass || w->inherits(inheritsClass))
            out.append(w->name());
    }
    return out;
}

MyWidget *findWindowByName(ApplicationWindow *app, const QString &name)
{
    if (!app)
        return nullptr;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return nullptr;
    const QList<MyWidget *> windows = app->windowsList();
    for (MyWidget *w : windows) {
        if (w && w->name() == trimmed)
            return w;
    }
    for (MyWidget *w : windows) {
        if (w && w->name().compare(trimmed, Qt::CaseInsensitive) == 0)
            return w;
    }
    for (MyWidget *w : windows) {
        if (w && !w->windowLabel().trimmed().isEmpty()
            && w->windowLabel().trimmed() == trimmed)
            return w;
    }
    for (MyWidget *w : windows) {
        if (w && !w->windowLabel().trimmed().isEmpty()
            && w->windowLabel().trimmed().compare(trimmed, Qt::CaseInsensitive) == 0)
            return w;
    }
    QString hyphenNorm = trimmed;
    hyphenNorm.replace(QLatin1Char('_'), QLatin1Char('-'));
    if (hyphenNorm != trimmed) {
        for (MyWidget *w : windows) {
            if (w && (w->name() == hyphenNorm
                      || w->name().compare(hyphenNorm, Qt::CaseInsensitive) == 0)) {
                return w;
            }
        }
    }
    return nullptr;
}

Table *resolveTable(ApplicationWindow *app, const QJsonObject &params,
                    const QString &key, bool required)
{
    if (!app)
        throwRpcRuntime(QStringLiteral("ApplicationWindow is null."));
    QString name = optionalJsonString(params, key).trimmed();
    if (name.isEmpty() && key != QStringLiteral("name"))
        name = optionalJsonString(params, QStringLiteral("window")).trimmed();
    if (!name.isEmpty()) {
        if (Table *t = app->table(name))
            return t;
        if (MyWidget *mw = findWindowByName(app, name)) {
            if (mw->inherits("Table"))
                return static_cast<Table *>(mw);
        }
        if (required) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Table not found: '%1'.").arg(name),
                    collectWindowNames(app, "Table"));
        }
        return nullptr;
    }
    if (QWidget *act = app->d_workspace.activeSubWindow()) {
        if (act->inherits("Table"))
            return static_cast<Table *>(act);
    }
    const QList<MyWidget *> windows = app->windowsList();
    for (int i = windows.size() - 1; i >= 0; --i) {
        if (windows[i] && windows[i]->inherits("Table"))
            return static_cast<Table *>(windows[i]);
    }
    if (required) {
        throwInvalidParamsWithAvailable(QStringLiteral("No Table window available."),
                                        collectWindowNames(app, "Table"));
    }
    return nullptr;
}

Matrix *resolveMatrix(ApplicationWindow *app, const QJsonObject &params,
                      const QString &key, bool required)
{
    if (!app)
        throwRpcRuntime(QStringLiteral("ApplicationWindow is null."));
    QString name = optionalJsonString(params, key).trimmed();
    if (name.isEmpty() && key != QStringLiteral("name"))
        name = optionalJsonString(params, QStringLiteral("window")).trimmed();
    if (!name.isEmpty()) {
        if (Matrix *m = app->matrix(name))
            return m;
        if (MyWidget *mw = findWindowByName(app, name)) {
            if (mw->inherits("Matrix"))
                return static_cast<Matrix *>(mw);
        }
        if (required) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Matrix not found: '%1'.").arg(name),
                    collectWindowNames(app, "Matrix"));
        }
        return nullptr;
    }
    if (QWidget *act = app->d_workspace.activeSubWindow()) {
        if (act->inherits("Matrix"))
            return static_cast<Matrix *>(act);
    }
    const QList<MyWidget *> windows = app->windowsList();
    for (int i = windows.size() - 1; i >= 0; --i) {
        if (windows[i] && windows[i]->inherits("Matrix"))
            return static_cast<Matrix *>(windows[i]);
    }
    if (required) {
        throwInvalidParamsWithAvailable(QStringLiteral("No Matrix window available."),
                                        collectWindowNames(app, "Matrix"));
    }
    return nullptr;
}

MultiLayer *resolveMultiLayer(ApplicationWindow *app, const QJsonObject &params,
                              const QString &key, bool required)
{
    if (!app)
        throwRpcRuntime(QStringLiteral("ApplicationWindow is null."));
    QString name = optionalJsonString(params, key).trimmed();
    if (name.isEmpty())
        name = optionalJsonString(params, QStringLiteral("graph_name")).trimmed();
    if (name.isEmpty() && key != QStringLiteral("name"))
        name = optionalJsonString(params, QStringLiteral("window")).trimmed();
    if (!name.isEmpty()) {
        if (MyWidget *mw = findWindowByName(app, name)) {
            if (mw->inherits("MultiLayer"))
                return static_cast<MultiLayer *>(mw);
        }
        if (required) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("MultiLayer graph not found: '%1'.").arg(name),
                    collectWindowNames(app, "MultiLayer"));
        }
        return nullptr;
    }
    if (QWidget *act = app->d_workspace.activeSubWindow()) {
        if (act->inherits("MultiLayer"))
            return static_cast<MultiLayer *>(act);
    }
    const QList<MyWidget *> windows = app->windowsList();
    for (int i = windows.size() - 1; i >= 0; --i) {
        if (windows[i] && windows[i]->inherits("MultiLayer"))
            return static_cast<MultiLayer *>(windows[i]);
    }
    if (required) {
        throwInvalidParamsWithAvailable(
                QStringLiteral("No MultiLayer graph window available."),
                collectWindowNames(app, "MultiLayer"));
    }
    return nullptr;
}

Graph *resolveGraphLayer(MultiLayer *ml, const QJsonObject &params, bool required)
{
    if (!ml) {
        if (required)
            throwInvalidParams(QStringLiteral("MultiLayer graph is null."));
        return nullptr;
    }
    const QList<Graph *> layers = ml->layersList();
    if (params.contains(QStringLiteral("layer")) && !params.value(QStringLiteral("layer")).isNull()
        && !params.value(QStringLiteral("layer")).isUndefined()) {
        const int idx = parseStrictJsonInt(params.value(QStringLiteral("layer")),
                                           QStringLiteral("layer"));
        if (idx >= 0 && idx < layers.size())
            return layers[idx];
        if (required) {
            QStringList avail;
            for (int i = 0; i < layers.size(); ++i)
                avail.append(QString::number(i));
            throwInvalidParamsWithAvailable(
                    QStringLiteral("0-based layer index %1 out of range for graph '%2' (layer_count=%3).")
                            .arg(idx)
                            .arg(ml->name())
                            .arg(layers.size()),
                    avail);
        }
        return nullptr;
    }
    if (Graph *act = ml->activeGraph())
        return act;
    if (!layers.isEmpty())
        return layers.first();
    if (required)
        throwInvalidParams(QStringLiteral("Graph window '%1' has no layers.").arg(ml->name()));
    return nullptr;
}

int resolveColumnIndex(Table *t, const QJsonValue &colSpec, bool required)
{
    if (!t) {
        if (required)
            throwInvalidParams(QStringLiteral("Table is null."));
        return -1;
    }
    auto availColLabels = [&]() -> QStringList {
        QStringList cols;
        for (int c = 0; c < t->numCols(); ++c)
            cols.append(t->colLabel(c));
        return cols;
    };
    if (colSpec.isDouble() && !colSpec.isBool()) {
        const int idx = parseStrictJsonInt(colSpec, QStringLiteral("col"));
        if (idx >= 0 && idx < t->numCols())
            return idx;
        if (required) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Column index %1 out of range in table '%2' (cols=%3).")
                            .arg(idx)
                            .arg(t->name())
                            .arg(t->numCols()),
                    availColLabels());
        }
        return -1;
    }
    if (colSpec.isString()) {
        const QString raw = colSpec.toString().trimmed();
        if (raw.isEmpty()) {
            if (required) {
                throwInvalidParamsWithAvailable(
                        QStringLiteral("Column name is empty in table '%1'.").arg(t->name()),
                        availColLabels());
            }
            return -1;
        }
        // 1. Exact case-sensitive short column label match first (handles numeric labels like "0", "100")
        for (int c = 0; c < t->numCols(); ++c) {
            if (t->colLabel(c) == raw)
                return c;
        }
        // 2. Exact case-sensitive full column name match ("Table_Col")
        for (int c = 0; c < t->numCols(); ++c) {
            if (t->colName(c) == raw)
                return c;
        }
        // 3. Case-insensitive short column label match
        for (int c = 0; c < t->numCols(); ++c) {
            if (t->colLabel(c).compare(raw, Qt::CaseInsensitive) == 0)
                return c;
        }
        // 4. Case-insensitive full name or long name match
        for (int c = 0; c < t->numCols(); ++c) {
            if (t->colName(c).compare(raw, Qt::CaseInsensitive) == 0
                || t->colLongName(c).compare(raw, Qt::CaseInsensitive) == 0)
                return c;
        }
        int idx = t->colIndex(raw);
        if (idx >= 0 && idx < t->numCols())
            return idx;
        const QString full = QString(t->name()) + QStringLiteral("_") + raw;
        idx = t->colIndex(full);
        if (idx >= 0 && idx < t->numCols())
            return idx;
        bool okInt = false;
        int asInt = raw.toInt(&okInt);
        if (okInt && asInt >= 0 && asInt < t->numCols())
            return asInt;
        if (required) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Column '%1' not found in table '%2'.").arg(raw, t->name()),
                    availColLabels());
        }
        return -1;
    }
    if (required) {
        throwInvalidParamsWithAvailable(
                QStringLiteral("Column specification must be a 0-based integer index or column name."),
                availColLabels());
    }
    return -1;
}

void enforceArialGraphFonts(Graph *g)
{
    if (!g)
        return;
    BatchExecutionContext &bctx = rpcBatchContext();
    if (bctx.active && bctx.deferReplot) {
        bctx.markDirtyGraph(g);
        if (MultiLayer *ml = g->multiLayerParent())
            bctx.markDirtyMultiLayer(ml);
        return;
    }
    for (int ax = 0; ax < QwtPlot::axisCnt; ++ax) {
        QFont nf = g->axisFont(ax);
        nf.setFamily(QStringLiteral("Arial"));
        nf.setBold(false);
        nf.setHintingPreference(QFont::PreferNoHinting);
        g->setAxisFont(ax, nf);

        QFont tf = g->axisTitleFont(ax);
        tf.setFamily(QStringLiteral("Arial"));
        tf.setBold(false);
        tf.setHintingPreference(QFont::PreferNoHinting);
        g->setAxisTitleFont(ax, tf);
    }
    if (Legend *leg = g->legend()) {
        QFont lf = leg->font();
        lf.setFamily(QStringLiteral("Arial"));
        lf.setBold(false);
        lf.setHintingPreference(QFont::PreferNoHinting);
        leg->setFont(lf);
    }
    for (int tid : g->textMarkerKeys()) {
        if (Legend *tm = g->textMarker(tid)) {
            QFont tmf = tm->font();
            tmf.setFamily(QStringLiteral("Arial"));
            tmf.setHintingPreference(QFont::PreferNoHinting);
            tm->setFont(tmf);
        }
    }
    if (g->plotWidget()) {
        QwtText t = g->plotWidget()->title();
        QFont f = t.font();
        f.setFamily(QStringLiteral("Arial"));
        f.setBold(false);
        f.setHintingPreference(QFont::PreferNoHinting);
        t.setFont(f);
        g->plotWidget()->setTitle(t);

        const QwtPlotItemList &items = g->plotWidget()->itemList();
        for (QwtPlotItem *it : items) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                if (auto *spec = dynamic_cast<Spectrogram *>(it))
                    spec->enforceColorBarArialFont();
            }
        }
    }
}

bool resolveStylePresetForRpc(const QString &rawPreset, MultiLayer::GraphStylePresetSpec *outSpec,
                              bool throwOnUnknown)
{
    const QString trimmed = rawPreset.trimmed();
    if (trimmed.isEmpty())
        return false;
    const QString lower = trimmed.toLower();
    MultiLayer::GraphStylePresetSpec spec;
    bool resolved = false;
    if (lower == QStringLiteral("journal"))
        resolved = MultiLayer::findStylePreset(QStringLiteral("isij_85mm"), &spec);
    else
        resolved = MultiLayer::findStylePreset(trimmed, &spec);
    if (!resolved) {
        if (!throwOnUnknown)
            return false;
        QStringList avail;
        for (const auto &s : MultiLayer::listStylePresets())
            avail << s.id;
        throwInvalidParamsWithAvailable(
                QStringLiteral("Unknown style preset '%1'.").arg(trimmed), avail);
    }
    if (outSpec)
        *outSpec = spec;
    return true;
}

void applyResolvedStylePreset(MultiLayer *ml, const MultiLayer::GraphStylePresetSpec &spec)
{
    if (!ml)
        return;
    ml->applyStylePresetSpec(spec);
    if (spec.fontFamily.compare(QStringLiteral("Arial"), Qt::CaseInsensitive) == 0
        && !spec.boldTitle && !spec.boldTick && !spec.boldLegend) {
        for (Graph *lg : ml->layersList())
            enforceArialGraphFonts(lg);
    }
}

void replotGraphOrDefer(Graph *g)
{
    if (!g)
        return;
    BatchExecutionContext &bctx = rpcBatchContext();
    if (bctx.active && bctx.deferReplot) {
        bctx.markDirtyGraph(g);
        if (MultiLayer *ml = g->multiLayerParent())
            bctx.markDirtyMultiLayer(ml);
        return;
    }
    g->replot();
}

void enforceArialGraph3DFonts(Graph3D *g3d)
{
    if (!g3d)
        return;
    QFont xf = g3d->xAxisLabelFont();
    xf.setFamily(QStringLiteral("Arial"));
    xf.setBold(false);
    g3d->setXAxisLabelFont(xf);

    QFont yf = g3d->yAxisLabelFont();
    yf.setFamily(QStringLiteral("Arial"));
    yf.setBold(false);
    g3d->setYAxisLabelFont(yf);

    QFont zf = g3d->zAxisLabelFont();
    zf.setFamily(QStringLiteral("Arial"));
    zf.setBold(false);
    g3d->setZAxisLabelFont(zf);

    QFont nf = g3d->numbersFont();
    nf.setFamily(QStringLiteral("Arial"));
    nf.setBold(false);
    g3d->setNumbersFont(nf);

    QFont tf = g3d->titleFont();
    tf.setFamily(QStringLiteral("Arial"));
    tf.setBold(false);
    g3d->setTitleFont(tf);
}

QString sanitizeWindowNameForSciDAVis(const QString &raw,
                                      const QString &fallbackPrefix,
                                      bool preserveUnderscores)
{
    QString s = sanitizeUtf16Surrogates(raw).trimmed();
    if (!preserveUnderscores) {
        s.replace(QLatin1Char('-'), QLatin1Char('_'));
        s.replace(QRegExp(QStringLiteral("\\W+")), QStringLiteral("_"));
        while (s.startsWith(QLatin1Char('_')))
            s.remove(0, 1);
        while (s.endsWith(QLatin1Char('_')))
            s.chop(1);
        if (s.isEmpty())
            s = fallbackPrefix;
        s.replace(QLatin1Char('_'), QLatin1Char('-'));
        return s;
    }

    QString out;
    out.reserve(s.size());
    bool prevSep = false;
    for (int i = 0; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        const bool isAsciiWordOrDash = (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
                || (ch >= QLatin1Char('a') && ch <= QLatin1Char('z'))
                || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
                || ch == QLatin1Char('_') || ch == QLatin1Char('-');
        if (isAsciiWordOrDash) {
            out.append(ch);
            prevSep = false;
        } else if (!prevSep && !out.isEmpty()) {
            out.append(QLatin1Char('_'));
            prevSep = true;
        }
    }
    while (out.endsWith(QLatin1Char('_')) || out.endsWith(QLatin1Char('-')))
        out.chop(1);
    while (out.startsWith(QLatin1Char('_')) || out.startsWith(QLatin1Char('-')))
        out.remove(0, 1);
    if (out.isEmpty())
        out = fallbackPrefix;
    return out;
}

QString makeUniqueValidWindowName(ApplicationWindow *app, const QString &sanitizedBase,
                                  MyWidget *targetWidget)
{
    auto hasConflict = [&](const QString &cand) -> bool {
        if (app && app->alreadyUsedName(cand))
            return true;
        if (targetWidget && targetWidget->inherits("Table")) {
            if (static_cast<Table *>(targetWidget)->colNames().contains(cand))
                return true;
        }
        return false;
    };
    if (!hasConflict(sanitizedBase))
        return sanitizedBase;
    for (int suffix = 2; suffix < 100000; ++suffix) {
        const QString cand = QStringLiteral("%1-%2").arg(sanitizedBase).arg(suffix);
        if (!hasConflict(cand))
            return cand;
    }
    return app->generateUniqueName(sanitizedBase + QStringLiteral("-"), false);
}

void applyDefaultNumericFormat6g(Table *t, int colIdx)
{
    if (!t || colIdx < 0 || colIdx >= t->numCols())
        return;
    Column *col = t->column(colIdx);
    if (!col || col->columnMode() != SciDAVis::ColumnMode::Numeric)
        return;
    if (auto *f = qobject_cast<Double2StringFilter *>(col->outputFilter())) {
        f->setNumericFormat('g');
        f->setNumDigits(6);
    }
}

Table *createUniqueTableHeadless(ApplicationWindow *app, int rows, int cols, const QString &reqName)
{
    rows = std::max(1, rows);
    cols = std::max(1, cols);
    const QString trimmedReq = reqName.trimmed();
    QString finalName;
    if (trimmedReq.isEmpty()) {
        finalName = app->generateUniqueName(QStringLiteral("Table"), true);
    } else {
        const QString base = sanitizeWindowNameForSciDAVis(trimmedReq, QStringLiteral("Table"), true);
        finalName = makeUniqueValidWindowName(app, base, nullptr);
    }
    Table *t = app->newTable(rows, cols, finalName, QString());
    if (!t)
        throwRpcRuntime(QStringLiteral("Failed to create Table '%1'.").arg(finalName));
    if (!trimmedReq.isEmpty() && trimmedReq != finalName)
        t->setWindowLabel(trimmedReq);
    for (int c = 0; c < t->numCols(); ++c)
        applyDefaultNumericFormat6g(t, c);
    if (RecalcManager *rm = RecalcManager::instance(app))
        t->setAutoRecalcFormulas(rm->recalcMode() == RecalcManager::Auto);
    return t;
}

QVariant &rpcPrevNonInteractiveSlot()
{
    static QVariant v; // value of "torigraph_noninteractive" before the current RPC call
    return v;
}

QString pageUnitToString(MultiLayer::PageUnit u)
{
    switch (u) {
    case MultiLayer::UnitMm:
        return QStringLiteral("mm");
    case MultiLayer::UnitCm:
        return QStringLiteral("cm");
    case MultiLayer::UnitInch:
        return QStringLiteral("inch");
    default:
        return QStringLiteral("px");
    }
}

extern const ApiMethodCatalogRow kApiCatalogSpecs[] = {
    // 1. App / Project / GUI / Introspection
    { "ping", "app", "Session.ping()", "Health check returning pong, endpoint, and server PID.", "",
      "app.ping()" },
    { "app.info", "app", "Session.info()",
      "Return ToriGraph version, PID, language, active endpoint, window count, and method count.",
      "", "app.info()" },
    { "app.supported_methods", "app", "Session.supported_methods()",
      "Return the complete ordered list of supported JSON-RPC 2.0 method names.", "",
      "app.supported_methods()" },
    { "app.describe_api", "app", "Session.describe_api(method=None, *, domain=None)",
      "Self-discovery catalog returning structured schemas, parameter types/enums, and Python SDK examples for one method, a domain, or all methods.",
      "method:str=''; domain:str=''", "app.describe_api(domain='graph')" },
    { "app.new_project", "app", "Session.new_project(*, force=None)",
      "Reset the workspace in-place to a clean empty project.", "force:bool|null=null",
      "app.new_project()" },
    { "app.batch", "app",
      "Session.batch(*, atomic=True, stop_on_error=True, defer_replot=True)",
      "Execute an atomic pipeline of RPC commands with '$prev.<key>' and '$steps[k].<key>' token interpolation, coalesced undo/replot, and automatic workspace rollback on error.",
      "commands:list[dict]*; stop_on_error:bool=true; atomic:bool=true; defer_replot:bool=true",
      "with app.batch(atomic=True) as b:\n    t = app.create_table(rows=10, cols=2)" },
    { "app.open_project", "app", "Session.open_project(path, *, append=False, force=None)",
      "Open a .sciprj/.sciprj.gz project (or append it as a subfolder when append=True).",
      "path:str*; append:bool=false; force:bool|null=null", "app.open_project('study.sciprj')" },
    { "app.save_project", "app", "Session.save_project(path, *, compress=False, snapshot=False)",
      "Save the current project to .sciprj or gzip-compressed .sciprj.gz.",
      "path:str*; compress:bool=false; snapshot:bool=false", "app.save_project('study.sciprj')" },
    { "app.import_csv", "app",
      "Session.import_csv(path, *, separator=None, encoding='auto', header=True, units_row=None, skip_lines=0, numeric=True, name=None)",
      "Import CSV/TSV/ASCII, headerless .xy/.xye, or Rigaku .ras data with UTF-8/UTF-16/Shift-JIS auto-decoding, RFC 4180 quotes, 2-row [Name, Units] auto-promotion, and 'Name (Unit)' header parsing.",
      "path:str*; separator:str=''; encoding:str='auto'[auto|utf-8|shift_jis|utf-16]; header:bool=true; units_row:int|bool|null=null; skip_lines:int=0; strip_spaces:bool=true; simplify_spaces:bool=false; numeric:bool=true; with_comments:bool=false; name:str=''",
      "t = app.import_csv('tensile_sjis.csv', encoding='auto')" },
    { "app.import_opj", "app", "Session.import_opj(path, *, codec=None)",
      "Import an Origin .opj or Origin 2018+ .opju (ZIP or CPYUA) project file into the workspace.",
      "path:str*; codec:str=''", "app.import_opj('project.opju')" },
    { "app.import_xlsx", "app", "Session.import_xlsx(path, *, sheet=None, workbook=True)",
      "Import an Excel workbook (.xlsx/.xlsm) without Excel: every sheet (or only `sheet`) becomes a worksheet named <book>_<sheet>; a text first row becomes Long Name, a unit second row becomes Units, date-formatted cells become date/time columns.",
      "path:str*; sheet:str=''; workbook:bool=true", "tables = app.import_xlsx('tensile.xlsx')" },
    { "app.list_sample_datasets", "app", "Session.list_sample_datasets(category=None)",
      "List built-in curated Materials Science & physics sample datasets (Arrhenius, DP980 tensile, FCC Cu XRD, TiO2 Tauc, LiFePO4 EIS, DSC Kissinger, Si bands).",
      "category:str=''", "app.list_sample_datasets()" },
    { "app.load_sample_dataset", "app",
      "Session.load_sample_dataset(dataset_id='arrhenius_diffusion', *, auto_plot=True, preset=None)",
      "Load a built-in curated scientific sample dataset into a worksheet/matrix and optionally create a publication-styled plot.",
      "id:str='arrhenius_diffusion'; auto_plot:bool=true; preset:str=''",
      "res = app.load_sample_dataset('tensile_alznmg', auto_plot=True)" },
    { "app.list_windows", "app", "Session.list_windows(window_type=None)",
      "List open workspace windows (optionally filtered by Table, Matrix, MultiLayer, Graph3D, Note).",
      "type:str=''", "app.list_windows('MultiLayer')" },
    { "app.window_info", "app", "Session.window_info(name)",
      "Return detailed serialized metadata for a named window.", "name:str*",
      "app.window_info('Graph1')" },
    { "app.activate_window", "app", "Session.activate_window(name)",
      "Activate the specified MDI window by name.", "name:str*", "app.activate_window('Table1')" },
    { "app.rename_window", "app", "Session.rename_window(name, new_name, *, old_name=None)",
      "Rename an existing window (ASCII letters, digits, '-' and '_').",
      "name:str=''; old_name:str=''; new_name:str*", "app.rename_window('Table1', 'Tensile-Raw')" },
    { "app.close_window", "app", "Session.close_window(name)",
      "Close and delete a named window without confirmation prompts.", "name:str*",
      "app.close_window('Table1')" },
    { "app.clone_window", "app", "Session.clone_window(name, *, new_name=None)",
      "Duplicate an existing Table, Matrix, or Graph window.",
      "name:str*; new_name:str=''", "app.clone_window('Graph1', new_name='Graph1-Copy')" },
    { "app.set_language", "app", "Session.set_language(language=None, *, lang=None)",
      "Switch UI/translation language between Japanese ('ja') and English ('en').",
      "language:str='ja'[ja|en]; lang:str=''", "app.set_language('en')" },
    { "app.get_language", "app", "Session.get_language()",
      "Return active application language ('ja' or 'en').", "", "app.get_language()" },
    { "app.set_recalc_mode", "app", "Session.set_recalc_mode(mode)",
      "Set column/analysis recalculation mode ('auto', 'manual', or 'disabled').",
      "mode:str*[auto|manual|disabled]", "app.set_recalc_mode('auto')" },
    { "app.recalculate", "app", "Session.recalculate()",
      "Evaluate all dirty column formulas and registered analysis links.", "", "app.recalculate()" },
    { "app.undo", "app", "Session.undo(window=None, *, graph=None, table=None, matrix=None)",
      "Undo the last operation on a specified graph/table/matrix window or the most recently modified stack.",
      "window:str=''; graph:str=''; table:str=''; matrix:str=''", "app.undo(table=tbl)" },
    { "app.redo", "app", "Session.redo(window=None, *, graph=None, table=None, matrix=None)",
      "Redo the last undone operation on a specified graph/table/matrix window or the most recently modified stack.",
      "window:str=''; graph:str=''; table:str=''; matrix:str=''", "app.redo(table=tbl)" },
    { "app.set_focus_hud", "app", "Session.set_focus_hud(enabled=None)",
      "Enable, disable, or toggle the floating canvas Focus HUD.", "enabled:bool=true",
      "app.set_focus_hud(False)" },
    { "app.get_focus_hud", "app", "Session.get_focus_hud()",
      "Query whether the Focus HUD is enabled.", "", "app.get_focus_hud()" },
    { "app.set_workspace_mode", "app", "Session.set_workspace_mode(mode)",
      "Switch workspace layout mode ('tabs', 'split', or 'mdi').",
      "mode:str*[tabs|split|mdi]", "app.set_workspace_mode('tabs')" },
    { "app.check_updates", "app", "Session.check_updates()",
      "Check release feed metadata for available ToriGraph updates.", "", "app.check_updates()" },
    { "app.list_actions", "app", "Session.list_actions(filter_text=None)",
      "List registered QAction commands and shortcuts.", "filter:str=''",
      "app.list_actions('grid')" },
    { "app.trigger_action", "app", "Session.trigger_action(name, *, checked=None)",
      "Trigger a safe view/navigation/toggle QAction or 0-arg slot from a fixed allow-list (destructive commands and modal dialogs are rejected with -32602).",
      "name:str*; checked:bool=null", "app.trigger_action('actionNextWindow')" },
    { "app.grab_gui_diagnostic", "app", "Session.call(method='app.grab_gui_diagnostic', path='')",
      "Capture main-window screenshot and ribbon/dock/table geometry diagnostics.",
      "path:str=''; width:int=0; height:int=0; ribbon_tab:int=0; show_explorer:bool=null; expand_inspector:bool=null; inspector_tab:int=null; scroll_inspector:bool|int=null; focus_hud:bool=null; workspace_mode:str=''\x5btabs|split|mdi]; dock_explorer:str=''\x5bleft|right|bottom]; explorer_width:int=null; focus_table_drawer:bool=null; check_all_tabs:bool=true",
      "app.call('app.grab_gui_diagnostic', path='ui.png')" },

    // 1b. Workbooks (several worksheets as sheet tabs of one window)
    { "workbook.create", "workbook", "Session.new_workbook(name=None, sheets=1, *, tables=None)",
      "Create a workbook with new empty sheets, or group existing worksheets (tables) into one.",
      "name:str=''; sheets:int=1; tables:list=[]", "wb = app.new_workbook('Specimens', sheets=3)" },
    { "workbook.list", "workbook", "Session.list_workbooks()",
      "List workbooks with their sheets (table names, tab labels) and the visible sheet.", "",
      "app.list_workbooks()" },
    { "workbook.add_sheet", "workbook", "WorkbookProxy.add_sheet(name=None, *, index=-1)",
      "Add a new empty sheet (worksheet table '<book>_<name>') to a workbook.",
      "book:str*; name:str=''; index:int=-1; activate:bool=true", "t = wb.add_sheet('S2')" },
    { "workbook.attach_sheet", "workbook", "WorkbookProxy.attach(table, *, index=-1)",
      "Move an existing stand-alone worksheet of the same folder into a workbook.",
      "book:str*; table:str*; index:int=-1", "wb.attach('Table1')" },
    { "workbook.detach_sheet", "workbook", "Session.call('workbook.detach_sheet', table=...)",
      "Take a sheet out of its workbook; it becomes a stand-alone worksheet.",
      "table:str*", "app.call('workbook.detach_sheet', table='Book1_Sheet2')" },
    { "workbook.set_current", "workbook", "WorkbookProxy.set_current(table)",
      "Show the given sheet in the workbook window (the sheet tab is selected).", "table:str*",
      "wb.set_current('Book1_Sheet2')" },
    { "workbook.move_sheet", "workbook", "WorkbookProxy.move_sheet(table, index)",
      "Move a sheet tab to a new position (0 = first).", "table:str*; index:int*",
      "wb.move_sheet('Book1_Sheet3', 0)" },
    { "workbook.rename_sheet", "workbook", "WorkbookProxy.rename_sheet(table, name)",
      "Rename a sheet (graphs and column formulas follow the new table name).",
      "table:str*; name:str*", "wb.rename_sheet('Book1_Sheet1', 'Raw')" },
    { "workbook.rename", "workbook", "WorkbookProxy.rename(new_name)",
      "Rename a workbook (the sheets keep their table names, so references stay valid).", "book:str*; new_name:str*",
      "wb.rename('Specimens')" },
    { "workbook.ungroup", "workbook", "WorkbookProxy.ungroup()",
      "Dissolve a workbook: every sheet becomes a stand-alone worksheet.", "book:str*",
      "wb.ungroup()" },

    // 2. Worksheet (table.*)
    { "table.create", "table", "Session.create_table(rows=30, cols=2, *, name=None, columns=None)",
      "Create a new worksheet Table (numeric columns default to 'g' 6-digit display; non-ASCII names preserved in windowLabel).",
      "rows:int=30; cols:int=2; name:str=''; columns:list=[]; designations:list|dict=null",
      "tbl = app.create_table(rows=20, cols=2, name='Tensile')" },
    { "table.get_info", "table", "TableProxy.info()",
      "Return worksheet dimensions, column names, modes, plot designations, long names, and units.",
      "table:str=''", "tbl.info()" },
    { "table.resize", "table", "TableProxy.resize(rows=None, cols=None)",
      "Resize worksheet row count and/or column count.",
      "table:str=''; rows:int=null; cols:int=null", "tbl.resize(rows=100, cols=4)" },
    { "table.add_column", "table",
      "TableProxy.add_column(name=None, *, plot_designation='Y', mode='numeric', units=None, long_name=None, data=None)",
      "Append a new column with optional metadata and initial values.",
      "table:str=''; name:str=''; plot_designation:str='Y'[X|Y|Z|xErr|yErr|Label|None]; mode:str='numeric'[numeric|text|datetime]; units:str=''; long_name:str=''; data:list=null",
      "tbl.add_column('TrueStress', units='MPa', data=[100.0, 210.0])" },
    { "table.remove_column", "table", "TableProxy.remove_column(col)",
      "Remove a column by 0-based index or name.", "table:str=''; col:int|str*",
      "tbl.remove_column('TempCol')" },
    { "table.set_column_data", "table",
      "TableProxy.set_column(col, data, *, mode=None, units=None, long_name=None, plot_designation=None)",
      "Write an array of numeric or text values (None/NaN stored as invalid cells) and optional metadata.",
      "table:str=''; col:int|str*; data:list*; mode:str=''; units:str=''; long_name:str=''; plot_designation:str=''",
      "tbl.set_column('Strain', [0.0, 0.2, 0.5], units='%', plot_designation='X')" },
    { "table.get_column_data", "table",
      "TableProxy.get_column(col, *, compact=False, include_masked=True)",
      "Read column values (null for invalid/NaN cells) and sparse/dense row mask metadata.",
      "table:str=''; col:int|str*; compact:bool=false; include_masked:bool=true",
      "vals = tbl.get_column('Stress')" },
    { "table.set_data_dict", "table",
      "TableProxy.set_data_dict(data, *, units=None, long_names=None, designations=None, comments=None)",
      "Populate multiple named columns in one call with units, long_names, and plot designations.",
      "table:str=''; data:dict*; units:dict=null; long_names:dict=null; comments:dict=null; designations:dict=null",
      "tbl.set_data_dict({'Strain': [0, 1], 'Stress': [0, 210]}, units={'Strain': '%', 'Stress': 'MPa'})" },
    { "table.paste_tsv", "table",
      "TableProxy.paste_tsv(text=None, *, header=True, separator=None, units_row=None, start_row=0, start_col=0)",
      "Paste TSV/CSV text (or system clipboard) into a worksheet with automatic separator, 2-row [Name, Units], and 'Name (Unit)' header promotion.",
      "table:str=''; text:str=''; tsv:str=''; header:bool=true; separator:str=''; units_row:int|bool|null=null; start_row:int=0; start_col:int=0; name:str=''",
      "tbl.paste_tsv('Strain (%)\\tStress (MPa)\\n0.0\\t0.0\\n0.2\\t410.0')" },
    { "table.set_cell", "table", "TableProxy.set_cell(row, col, value)",
      "Set a single cell value (None/'NaN'/'' on a numeric column clears the cell to NaN without flipping column mode to text).",
      "table:str=''; row:int*; col:int|str*; value:float|str|null*",
      "tbl.set_cell(0, 'Stress', 412.5)" },
    { "table.get_cell", "table", "TableProxy.get_cell(row, col)",
      "Read a single cell's value, formatted text, invalid flag, and mask state.",
      "table:str=''; row:int*; col:int|str*", "tbl.get_cell(0, 'Stress')" },
    { "table.set_column_meta", "table",
      "TableProxy.set_meta(col, *, name=None, long_name=None, units=None, comment=None, plot_designation=None, mode=None)",
      "Update column name, Long Name, Units, Comment, plot designation, mode, or numeric format.",
      "table:str=''; col:int|str*; name:str=''; long_name:str=''; units:str=''; comment:str=''; plot_designation:str=''; mode:str=''",
      "tbl.set_meta('Stress', long_name='Engineering Stress', units='MPa')" },
    { "table.set_col_metadata", "table",
      "TableProxy.set_meta(col, *, long_name=None, units=None)",
      "Alias of table.set_column_meta.",
      "table:str=''; col:int|str*; name:str=''; long_name:str=''; units:str=''; comment:str=''; plot_designation:str=''; mode:str=''",
      "tbl.set_meta(1, units='MPa')" },
    { "table.get_column_meta", "table", "TableProxy.get_meta(col)",
      "Return column metadata (name, mode, plot_designation, long_name, units, comment, formula).",
      "table:str=''; col:int|str*", "tbl.get_meta('Stress')" },
    { "table.set_formula", "table", "TableProxy.set_formula(col, formula, *, recalculate=True)",
      "Assign a muParser column formula (e.g. 'col(\"Stress\")*(1+col(\"Strain\")/100)') and evaluate it.",
      "table:str=''; col:int|str*; formula:str*; recalculate:bool=true",
      "tbl.set_formula('TrueStress', 'col(\"Stress\")*(1+col(\"Strain\")/100)')" },
    { "table.recalculate", "table", "TableProxy.recalculate(col=None)",
      "Recalculate one or all formula columns in the worksheet.",
      "table:str=''; col:int|str=null", "tbl.recalculate()" },
    { "table.mask_rows", "table", "TableProxy.mask_rows(rows=None, *, col=None, start_row=None, end_row=None)",
      "Mask outlier rows so fits and statistics exclude them without deleting data.",
      "table:str=''; rows:list[int]=null; col:int|str=null; start_row:int=null; end_row:int=null",
      "tbl.mask_rows([5, 6], col='Stress')" },
    { "table.unmask_rows", "table", "TableProxy.unmask_rows(rows=None, *, col=None)",
      "Unmask specified rows (or all rows if omitted).",
      "table:str=''; rows:list[int]=null; col:int|str=null", "tbl.unmask_rows()" },
    { "table.clear_masks", "table", "TableProxy.clear_masks(col=None)",
      "Clear all row masks on one column or the entire table.",
      "table:str=''; col:int|str=null", "tbl.clear_masks()" },
    { "table.get_masked_rows", "table", "TableProxy.get_masked_rows(col=None)",
      "Return 0-based indices of masked rows in a column.",
      "table:str=''; col:int|str=null", "tbl.get_masked_rows('Stress')" },
    { "table.sort", "table", "TableProxy.sort(by_col=0, *, ascending=True, only_column=False)",
      "Sort the entire worksheet (or a single column) by a leading column.",
      "table:str=''; by:int|str=0; ascending:bool=true; only_column:bool=false",
      "tbl.sort('Strain', ascending=True)" },
    { "table.column_stats", "table", "TableProxy.stats(col=None, *, ignore_masked=True)",
      "Compute descriptive statistics (n, mean, sd, se, min, q1, median, q3, iqr, max, sum) per column.",
      "table:str=''; col:int|str=null; ignore_masked:bool=true", "tbl.stats('Stress')" },
    { "table.row_stats", "table",
      "TableProxy.row_stats(rows=None, *, start_row=None, end_row=None)",
      "Compute descriptive statistics across numeric columns for each row.",
      "table:str=''; rows:list[int]=null; start_row:int=null; end_row:int=null",
      "tbl.row_stats([0, 1, 2])" },
    { "table.export_csv", "table", "TableProxy.export_csv(path, *, separator=',', header=True, with_comments=False)",
      "Export worksheet contents to an RFC 4180 UTF-8 CSV or TSV file.",
      "table:str=''; path:str*; separator:str=','; header:bool=true; with_comments:bool=false",
      "tbl.export_csv('results.csv')" },
    { "table.to_matrix", "table", "TableProxy.to_matrix(name=None)",
      "Convert numeric worksheet cells into a Matrix window (preserving invalid cells as NaN).",
      "table:str=''; name:str=''", "m = tbl.to_matrix('GridMat')" },

    // 3. Matrix (matrix.*)
    { "matrix.create", "matrix",
      "Session.create_matrix(rows=32, cols=32, *, name=None, x_min=None, x_max=None, y_min=None, y_max=None)",
      "Create a 2D numeric Matrix window with optional physical X/Y coordinate bounds.",
      "rows:int=32; cols:int=32; name:str=''; x_min:float=null; x_max:float=null; y_min:float=null; y_max:float=null",
      "m = app.create_matrix(64, 64, name='Field', x_min=-1, x_max=1, y_min=-1, y_max=1)" },
    { "matrix.get_info", "matrix", "MatrixProxy.info()",
      "Return matrix dimensions, physical coordinate bounds, and value range.",
      "matrix:str=''", "m.info()" },
    { "matrix.set_coordinates", "matrix", "MatrixProxy.set_coordinates(x_min, x_max, y_min, y_max)",
      "Set physical X and Y coordinate bounds of the matrix.",
      "matrix:str=''; x_min:float*; x_max:float*; y_min:float*; y_max:float*",
      "m.set_coordinates(0.0, 10.0, 0.0, 10.0)" },
    { "matrix.set_data", "matrix", "MatrixProxy.set_data(rows_2d)",
      "Populate matrix cells from a 2D row-major array (null/NaN preserved as quiet_NaN).",
      "matrix:str=''; data:list[list[float]]*", "m.set_data([[1.0, 2.0], [3.0, 4.0]])" },
    { "matrix.get_data", "matrix", "MatrixProxy.get_data()",
      "Read matrix cells as a 2D row-major array (null for NaN).", "matrix:str=''",
      "grid = m.get_data()" },
    { "matrix.set_formula", "matrix", "MatrixProxy.set_formula(formula)",
      "Evaluate a muParser expression f(x, y, i, j) over all matrix cells.",
      "matrix:str=''; formula:str*", "m.set_formula('exp(-(x*x+y*y))')" },
    { "matrix.transpose", "matrix", "MatrixProxy.transpose()", "Transpose the matrix in-place.",
      "matrix:str=''", "m.transpose()" },
    { "matrix.invert", "matrix", "MatrixProxy.invert()",
      "Invert a square non-singular finite matrix in-place (throws -32602 if non-square or NaN/Inf).",
      "matrix:str=''", "m.invert()" },
    { "matrix.determinant", "matrix", "MatrixProxy.determinant()",
      "Compute the determinant of a square finite matrix (throws -32602 if non-square or NaN/Inf).",
      "matrix:str=''", "det = m.determinant()" },
    { "matrix.flip", "matrix", "MatrixProxy.flip(mode='h')",
      "Mirror or rotate matrix ('h', 'v', 'rot90_cw', 'rot90_ccw').",
      "matrix:str=''; mode:str*[h|v|rot90_cw|rot90_ccw]", "m.flip('h')" },
    { "matrix.fft2d", "matrix", "MatrixProxy.fft2d(*, inverse=False, shift_center=True)",
      "Compute 2D FFT (or inverse 2D FFT) returning a new Matrix.",
      "matrix:str=''; inverse:bool=false; shift_center:bool=true", "spec = m.fft2d()" },
    { "matrix.filter2d", "matrix",
      "MatrixProxy.filter2d(method='gaussian', *, sigma=1.5, radius=1, in_place=False, plot_heatmap=False)",
      "Apply 2D image/surface filter ('gaussian', 'median', 'laplacian', 'sobel', 'planar_tilt').",
      "matrix:str=''; method:str='gaussian'[gaussian|median|laplacian|sobel|planar_tilt]; sigma:float=1.5; radius:int=1; in_place:bool=false; plot_heatmap:bool=false",
      "m.filter2d('gaussian', sigma=1.2, in_place=True)" },
    { "matrix.analyze_2d_peaks", "matrix",
      "MatrixProxy.analyze_2d_peaks(*, max_peaks=10, prominence=0.1, graph=None)",
      "Find 2D local maxima and fit 2D Gaussian centers, amplitudes, FWHMs, and volumes.",
      "matrix:str=''; max_peaks:int=10; prominence:float=0.1; graph:str=''",
      "pks = m.analyze_2d_peaks(max_peaks=5)" },
    { "matrix.to_table", "matrix", "MatrixProxy.to_table(name=None)",
      "Convert matrix into a numeric worksheet Table (preserving NaN cells as invalid).",
      "matrix:str=''; name:str=''", "tbl = m.to_table('FromMat')" },

    // 4. Plotting (plot.*)
    { "plot.xy", "plot",
      "Session.plot_xy(table=None, x_col=None, y_cols=None, *, style='line_symbol', y_err_cols=None, graph_name=None, title=None, xlabel=None, ylabel=None, xlim=None, ylim=None, color=None, lw=None, ls=None, marker=None, ms=None, symbol_skip=None)",
      "Create a 2D XY plot (line, scatter, line_symbol, spline, area, vertical_bars, steps, stem; auto-applies category labels when X column is text) with optional per-series error bars and inline curve styling.",
      "table:str=''; x_col:int|str=null; y_cols:list|str=null; y_err_col:int|str=null; y_err_cols:list=null; style:str='line_symbol'[line|scatter|line_symbol|spline|vertical_bars|horizontal_bars|area|vertical_steps|horizontal_steps|drop_lines]; bar_gap:int=null; graph_name:str=''; title:str=''; xlabel:str=''; ylabel:str=''; xlim:list[float]=null; ylim:list[float]=null; color:str=''; lw:float=null; ls:str=''; marker:str=''; ms:int=null; symbol_skip:int=null",
      "g = app.plot_xy(tbl, 'Strain', ['Stress'], style='line', lw=2.0, color='#0072B2')" },
    { "plot.stacked_bars", "plot",
      "Session.plot_stacked_bars(table=None, x_col=None, y_cols=None, *, horizontal=False, percent_100=False)",
      "Create a stacked vertical or horizontal bar/column chart (supports text category X columns and percent_100=True 0..100% normalization).",
      "table:str=''; x_col:int|str=null; y_cols:list=null; horizontal:bool=false; percent_100:bool=false; graph_name:str=''",
      "g = app.plot_stacked_bars(tbl, 'Sample', ['PhaseA', 'PhaseB'], percent_100=True)" },
    { "plot.add_curve", "plot",
      "GraphProxy.add_curve(table, y_col, *, x_col=None, style='line_symbol', layer=None, right_axis=False, color=None, lw=None, ls=None, marker=None, ms=None, symbol_skip=None)",
      "Add a data curve onto an existing graph layer (supports inline matplotlib style kwargs).",
      "graph:str=''; table:str=''; y_col:int|str*; x_col:int|str=null; style:str='line_symbol'; layer:int=null; right_axis:bool=false; color:str=''; lw:float=null; ls:str=''; marker:str=''; ms:int=null; symbol_skip:int=null",
      "g.add_curve(tbl, 'Stress_300C', x_col='Strain', ls='--', color='#D55E00')" },
    { "plot.remove_curve", "plot", "GraphProxy.remove_curve(curve=0, *, layer=None)",
      "Remove a curve from a graph layer by title or 0-based index.",
      "graph:str=''; curve:int|str=0; layer:int=null", "g.remove_curve(0)" },
    { "plot.add_error_bars", "plot",
      "GraphProxy.add_error_bars(err_col, *, curve=None, table=None, direction='y', layer=None)",
      "Attach X or Y error bars from a worksheet column to an existing curve.",
      "graph:str=''; err_col:int|str*; curve:int|str=null; table:str=''; direction:str='y'[x|y]; layer:int=null",
      "g.add_error_bars('Stress_SD', curve='Stress')" },
    { "plot.double_y", "plot",
      "Session.plot_double_y(table=None, x_col=None, y_left_col=None, y_right_col=None, *, graph_name=None)",
      "Create a dual-Y-axis plot with left and right Y curves.",
      "table:str=''; x_col:int|str=null; y_left_col:int|str=null; y_right_col:int|str=null; graph_name:str=''",
      "g = app.plot_double_y(tbl, 'Time', 'Temp', 'Load')" },
    { "plot.waterfall", "plot",
      "Session.plot_waterfall(table=None, *, x_col=None, y_cols=None, offset_x=0.0, offset_y=15.0)",
      "Create an offset waterfall multi-curve plot (e.g. XRD / Raman temperature series, top-to-bottom legend order).",
      "table:str=''; x_col:int|str=null; y_cols:list=null; offset_x:float=0.0; offset_y:float=15.0; x_offset_pct:float=0.0; y_offset_pct:float=15.0; normalize:bool=false; graph_name:str=''",
      "g = app.plot_waterfall(tbl, x_col='TwoTheta', y_cols=['300K', '500K', '700K'])" },
    { "plot.histogram", "plot",
      "Session.plot_histogram(table=None, col=None, *, bins=None, bin_size=None, stacked=False, overlay_pdf=False, graph_name=None)",
      "Plot a frequency histogram from one or more numeric columns with optional custom bins, stacked panels, and Normal PDF overlay.",
      "table:str=''; col:int|str=null; cols:list=null; bins:int=null; bin_size:float=null; bin_min:float=null; bin_max:float=null; stacked:bool=false; overlay_pdf:bool|str=false; graph_name:str=''",
      "g = app.plot_histogram(tbl, 'GrainSize_um', bins=20, overlay_pdf=True)" },
    { "plot.box", "plot",
      "Session.plot_box(table=None, cols=None, *, notched=False, whisker_mode='1.5iqr', show_points=False, graph_name=None)",
      "Create a statistical box-and-whisker plot across one or more columns.",
      "table:str=''; cols:list=null; notched:bool=false; whisker_mode:str='1.5iqr'[1.5iqr|sd|se|minmax|p5_95|p10_90|p1_99]; show_points:bool=false; graph_name:str=''",
      "g = app.plot_box(tbl, ['BatchA', 'BatchB'])" },
    { "plot.violin", "plot",
      "Session.plot_violin(table=None, cols=None, *, show_points=False, graph_name=None)",
      "Create a kernel-density violin plot across numeric columns.",
      "table:str=''; cols:list=null; show_points:bool=false; graph_name:str=''",
      "g = app.plot_violin(tbl, ['BatchA', 'BatchB'])" },
    { "plot.pareto", "plot",
      "Session.plot_pareto(table=None, category_col=None, value_col=None, *, graph_name=None)",
      "Create a sorted Pareto bar + cumulative percentage curve chart with category X labels.",
      "table:str=''; category_col:int|str=null; value_col:int|str=null; graph_name:str=''",
      "g = app.plot_pareto(tbl, 'DefectType', 'Count')" },
    { "plot.qq", "plot", "Session.plot_qq(table=None, col=None, *, graph_name=None)",
      "Create a Normal Q-Q probability plot for a numeric column.",
      "table:str=''; col:int|str=null; graph_name:str=''", "g = app.plot_qq(tbl, 'Strength')" },
    { "plot.pie", "plot", "Session.plot_pie(table=None, col=None, *, label_col=None, graph_name=None)",
      "Create a 2D pie chart from positive numeric values.",
      "table:str=''; col:int|str=null; label_col:int|str=null; graph_name:str=''",
      "g = app.plot_pie(tbl, 'Fraction')" },
    { "plot.vectors", "plot",
      "Session.plot_vectors(table=None, x_col=None, y_col=None, u_col=None, v_col=None, *, mode='xyxy')",
      "Create a 2D vector field plot ('xyxy' endpoints or 'xyam' angle/magnitude).",
      "table:str=''; x_col:int|str=null; y_col:int|str=null; u_col:int|str=null; v_col:int|str=null; mode:str='xyxy'[xyxy|xyam]; color:str=''; lw:float=null; head_length:int=null; head_angle:int=null; graph_name:str=''",
      "g = app.plot_vectors(tbl, 'X', 'Y', 'X2', 'Y2', mode='xyxy')" },
    { "plot.ternary", "plot",
      "Session.plot_ternary(table=None, a_col=None, b_col=None, c_col=None, *, graph_name=None)",
      "Create a ternary composition plot from three columns (A + B + C).",
      "table:str=''; a_col:int|str=null; b_col:int|str=null; c_col:int|str=null; graph_name:str=''",
      "g = app.plot_ternary(tbl, 'Fe', 'Cr', 'Ni')" },
    { "plot.polar", "plot",
      "Session.plot_polar(table=None, theta_col=0, r_cols=None, *, theta_in_radians=False, graph_name=None)",
      "Create a 2D polar coordinate plot (theta, r) with circular radial & angular grid rings.",
      "table:str=''; theta_col:int|str=null; r_col:int|str=null; r_cols:list=null; theta:list[float]=null; r:list[float]=null; theta_in_radians:bool=false; radians:bool=false; style:str='line_symbol'; color:str=''; lw:float=null; graph_name:str=''",
      "g = app.plot_polar(tbl, 'Angle_deg', 'Gain')" },
    { "plot.function", "plot",
      "Session.plot_function(formula, *, x_min=0.0, x_max=10.0, points=200, type='normal')",
      "Plot an analytical 2D function f(x), parametric [x(t), y(t)], or polar r(t).",
      "formula:str|list*; x_min:float=0.0; x_max:float=10.0; points:int=200; type:str='normal'[normal|parametric|polar]; variable:str='x'; graph:str=''; graph_name:str=''",
      "g = app.plot_function('sin(x)/x', x_min=0.1, x_max=20.0)" },
    { "plot.matrix", "plot",
      "Session.plot_matrix(matrix=None, *, style='colormap', palette='viridis', colormap=None, levels=None)",
      "Plot a 2D Matrix as a colormap heatmap, contour map, or grayscale image.",
      "matrix:str=''; style:str='colormap'[colormap|contour|gray|image]; palette:str='viridis'; colormap:str=''; levels:int=null; use_colormap_pen:bool=null; show_colorbar:bool=true; colorbar_title:str=''; graph_name:str=''",
      "g = app.plot_matrix(m, style='colormap', palette='viridis')" },
    { "plot.3d_matrix", "plot", "Session.plot_3d_matrix(matrix=None, *, style='surface')",
      "Create an interactive 3D surface/mesh/bars plot from a Matrix.",
      "matrix:str=''; style:str='surface'[surface|wireframe|hidden_line|bars|scatter]",
      "g3 = app.plot_3d_matrix(m, style='surface')" },
    { "plot.3d_xyz", "plot",
      "Session.plot_3d_xyz(table=None, x_col=None, y_col=None, z_col=None, *, style='scatter')",
      "Create a 3D XYZ scatter, trajectory, bar, or ribbon plot from worksheet columns.",
      "table:str=''; x_col:int|str=null; y_col:int|str=null; z_col:int|str=null; style:str='scatter'[scatter|trajectory|bars|ribbon]",
      "g3 = app.plot_3d_xyz(tbl, 'X', 'Y', 'Z')" },
    { "plot.3d_function", "plot",
      "Session.plot_3d_function(formula, *, x_min=-5, x_max=5, y_min=-5, y_max=5)",
      "Create a 3D analytical surface z = f(x, y).",
      "formula:str*; x_min:float=-5; x_max:float=5; y_min:float=-5; y_max:float=5",
      "g3 = app.plot_3d_function('sin(x)*cos(y)')" },

    // 5. Graph Layout, Inspection, Styling & Export (graph.*)
    { "graph.get_info", "graph", "GraphProxy.info()",
      "Return semantic summary of graph page, layers, axes, curves, spectrograms, and reference lines.",
      "graph:str=''", "g.info()" },
    { "graph.inspect", "graph", "GraphProxy.inspect(*, include_ticks=True, diagnose=True)",
      "Return full semantic + pixel geometry state (layer/canvas/legend/marker bboxes, rendered ticks, curve bounds/clipping, symbol_skip) and 7 deterministic layout warnings.",
      "graph:str=''; include_ticks:bool=true; diagnose:bool=true",
      "snap = g.inspect(); print(snap['warnings'])" },
    { "graph.add_layer", "graph", "GraphProxy.add_layer(*, x=0, y=0, width=0, height=0)",
      "Add a new 2D plot layer to a MultiLayer graph window (returns 0-based layer index).",
      "graph:str=''; x:int=0; y:int=0; width:int=0; height:int=0", "idx = g.add_layer()" },
    { "graph.remove_layer", "graph", "GraphProxy.remove_layer(layer)",
      "Remove a 0-based layer from a MultiLayer graph (requires >= 2 layers).",
      "graph:str=''; layer:int*", "g.remove_layer(1)" },
    { "graph.add_linked_layer", "graph",
      "GraphProxy.add_linked_layer(*, parent_layer=0, link_x=True, link_y=False, x_formula='x', y_formula='y')",
      "Add a transparent linked secondary-axis overlay layer (e.g. top-X Celsius / 1000/T).",
      "graph:str=''; parent_layer:int=0; link_x:bool=true; link_y:bool=false; x_formula:str='x'; y_formula:str='y'",
      "g.add_linked_layer(link_x=True, x_formula='1000/x - 273.15')" },
    { "graph.add_inset_layer", "graph",
      "GraphProxy.add_inset(*, source_layer=0, x_min=None, x_max=None, y_min=None, y_max=None, rel_x=0.55, rel_y=0.10, rel_w=0.38, rel_h=0.38, show_connectors=True)",
      "Add a zoomed inset layer over a parent ROI while keeping parent active_layer intact.",
      "graph:str=''; source_layer:int=0; x_min:float=null; x_max:float=null; y_min:float=null; y_max:float=null; rel_x:float=0.55; rel_y:float=0.10; rel_w:float=0.38; rel_h:float=0.38; show_connectors:bool=true",
      "g.add_inset(x_min=0.0, x_max=0.5, y_min=0.0, y_max=450.0, show_connectors=True)" },
    { "graph.add_inset", "graph",
      "GraphProxy.add_inset(*, source_layer=0, x_min=None, x_max=None, y_min=None, y_max=None, show_connectors=True)",
      "Alias of graph.add_inset_layer.",
      "graph:str=''; source_layer:int=0; x_min:float=null; x_max:float=null; y_min:float=null; y_max:float=null; show_connectors:bool=true",
      "g.add_inset(x_min=0.0, x_max=0.5)" },
    { "graph.arrange_layers", "graph",
      "GraphProxy.arrange_layers(rows=1, cols=1, *, row_spacing=None, col_spacing=None, share_x=False, share_y=False)",
      "Arrange primary layers into a rows x cols multi-panel grid.",
      "graph:str=''; rows:int=1; cols:int=1; row_spacing:int=null; col_spacing:int=null; left_margin:int=null; right_margin:int=null; top_margin:int=null; bottom_margin:int=null; share_x:bool=false; share_y:bool=false; fit:bool=false",
      "g.arrange_layers(rows=2, cols=2)" },
    { "graph.merge_graphs", "graph",
      "Session.merge_graphs(graphs, *, rows=0, cols=0, name=None, panel_labels=True)",
      "Combine multiple open 2D MultiLayer graphs into a single multi-panel figure preserving linked overlays.",
      "graphs:list[str]*; rows:int=0; cols:int=0; name:str=''; panel_labels:bool=true; close_sources:bool=false",
      "merged = app.merge_graphs([g1, g2], rows=1, cols=2)" },
    { "graph.set_page_size", "graph",
      "GraphProxy.set_page_size(width, height, *, unit='mm', dpi=300, page_view=True, zoom=1.0)",
      "Set physical journal figure dimensions (mm, cm, inch, or px) and export DPI.",
      "graph:str=''; width:float*; height:float*; unit:str='mm'[mm|cm|inch|px]; dpi:int=300; page_view:bool=true; zoom:float=1.0",
      "g.set_page_size(85, 65, unit='mm', dpi=600)" },
    { "graph.fit_page_to_layers", "graph", "GraphProxy.fit_page_to_layers(margin_px=14)",
      "Resize the page to tightly wrap all layers.", "graph:str=''; margin_px:int=14",
      "g.fit_page_to_layers()" },
    { "graph.fit_layers_to_page", "graph", "GraphProxy.fit_layers_to_page()",
      "Refit all primary layers into the current physical page dimensions.", "graph:str=''",
      "g.fit_layers_to_page()" },
    { "graph.apply_style_preset", "graph", "GraphProxy.apply_style_preset(preset='isij_85mm')",
      "Apply one-click journal/slide typography & sizing preset ('isij_85mm', 'lab_slide_83mm', 'slide_28pt', 'journal_2col_175mm', or custom user preset).",
      "graph:str=''; preset:str='isij_85mm'[isij_85mm|lab_slide_83mm|slide_28pt|journal_2col_175mm]",
      "g.apply_style_preset('isij_85mm')" },
    { "graph.apply_frame_preset", "graph", "GraphProxy.apply_frame_preset(preset='box_inward', *, layer=None)",
      "Apply 4-wall box + inward ticks ('box_inward'/'box'), plain box ('box_plain'), open L-axes ('open_l'/'top_right_off'), or origin-crossed ('zero_crossed').",
      "graph:str=''; preset:str='box_inward'[box_inward|box|box_plain|open_l|top_right_off|zero_crossed]; layer:int=null",
      "g.apply_frame_preset('box_inward')" },
    { "graph.set_aspect_ratio", "graph", "GraphProxy.set_aspect_ratio(ratio=1.0, *, keep=True)",
      "Lock or set target canvas width/height aspect ratio.",
      "graph:str=''; ratio:float=1.0; keep:bool=true", "g.set_aspect_ratio(1.0, keep=True)" },
    { "graph.copy_format", "graph", "GraphProxy.copy_format(layer=None)",
      "Copy page, axis, frame, font, and curve style formatting to the RPC format clipboard.",
      "graph:str=''; layer:int=null", "g.copy_format()" },
    { "graph.paste_format", "graph", "GraphProxy.paste_format(layer=None)",
      "Paste copied graph formatting onto another graph window.", "graph:str=''; layer:int=null",
      "g2.paste_format()" },
    { "graph.batch_clone", "graph", "GraphProxy.batch_clone(tables, *, x_col=None, y_cols=None)",
      "Clone a formatted template graph across multiple data tables.",
      "graph:str=''; tables:list[str]*; x_col:int|str=null; y_cols:list=null",
      "g.batch_clone(['Tbl2', 'Tbl3'])" },
    { "graph.set_title", "graph", "GraphProxy.set_title(title='', *, font_size=None, color=None, layer=None)",
      "Set or clear the layer plot title (enforcing non-bold Arial).",
      "graph:str=''; title:str=''; font_size:float=null; color:str=''; layer:int=null",
      "g.set_title('SUS304 Room-Temperature Tensile')" },
    { "graph.set_axis", "graph",
      "GraphProxy.set_axis(axis='x', *, title=None, min=None, max=None, lim=None, step=None, scale=None, title_font_size=None, numbers_font_size=None, show_labels=None, show_ticks=None, ticks_inward=None, unit_format=None, tick_format=None, precision=None, label_rotation=None, categories=None, layer=None)",
      "Configure axis title, limits (min/max or lim), linear/log10 scale, title/tick font sizes, tick/label visibility, unit format ('paren'|'slash'|'bracket'), numeric tick format, precision, rotation, and category labels.",
      "graph:str=''; axis:str='x'[x|y|top|right]; title:str=null; min:float=null; max:float=null; lim:list[float]=null; step:float=null; minor_ticks:int=null; scale:str=null[linear|log10]; inverted:bool=null; enabled:bool=null; backbone_visible:bool=null; show_backbone:bool=null; color:str=null; axis_color:str=null; title_color:str=null; numbers_color:str=null; font_family:str=null; font_size:float=null; title_font_size:float=null; tick_font_size:float=null; numbers_font_size:float=null; show_labels:bool=null; show_ticks:bool=null; ticks_inward:bool=null; unit_format:str=null[paren|slash|bracket]; tick_format:str|int=null[auto|f|e|superscript]; precision:int=null; label_rotation:float=null; rotation:float=null; categories:list[str]=null; layer:int=null",
      "g.set_axis('x', title='Strain (%)', lim=[0, 25], unit_format='paren')" },
    { "graph.set_category_labels", "graph",
      "GraphProxy.set_category_labels(labels, *, axis='x', layer=None)",
      "Assign discrete text category tick labels at integer coordinates 1..N on a graph axis.",
      "graph:str=''; axis:str='x'[x|y|top|right]; labels:list[str]*; layer:int=null",
      "g.set_category_labels(['SUS304', 'DP980', 'Ti-6Al-4V'], axis='x')" },
    { "graph.sync_axis_color", "graph", "GraphProxy.sync_axis_color(axis='right', *, curve=None, reset=False, layer=None)",
      "Colorize an axis backbone, ticks, numbers, and title to match a curve (typical for double-Y plots).",
      "graph:str=''; axis:str='right'[x|y|top|right]; curve:int|str=null; reset:bool=false; layer:int=null",
      "g.sync_axis_color('right', curve=1)" },
    { "graph.set_axis_break", "graph",
      "GraphProxy.set_axis_break(axis='x', *, enabled=True, start=None, end=None, position_pct=50.0, log_before=None, log_after=None, layer=None)",
      "Enable or disable a discontinuous scale break on an axis.",
      "graph:str=''; axis:str='x'[x|y|top|right]; enabled:bool=true; start:float=null; end:float=null; position_pct:float=50.0; log_before:bool=null; log_after:bool=null; layer:int=null",
      "g.set_axis_break('x', start=5.0, end=45.0)" },
    { "graph.step_zoom_history", "graph", "GraphProxy.step_zoom_history(direction='back', *, layer=None)",
      "Step backward or forward through interactive/RPC axis zoom history.",
      "graph:str=''; direction:str='back'[back|forward]; layer:int=null",
      "g.step_zoom_history('back')" },
    { "graph.add_reference_line", "graph",
      "GraphProxy.add_reference_line(*, orientation='horizontal', value=0.0, label='', color='#444444', style='dash', width=1.2, layer=None)",
      "Add a horizontal or vertical constant/statistical reference line.",
      "graph:str=''; orientation:str='horizontal'[horizontal|vertical]; value:float=0.0; stat_mode:str=''; label:str=''; color:str='#444444'; style:str='dash'[solid|dash|dot|dashdot]; width:float=1.2; layer:int=null",
      "g.add_reference_line(orientation='horizontal', value=410.0, label='0.2% Proof Stress')" },
    { "graph.remove_reference_line", "graph", "GraphProxy.remove_reference_line(index, *, layer=None)",
      "Remove a single reference line by 0-based index from a layer.",
      "graph:str=''; index:int*; layer:int=null", "g.remove_reference_line(0)" },
    { "graph.clear_reference_lines", "graph", "GraphProxy.clear_reference_lines(*, layer=None)",
      "Clear all reference lines on a graph layer.", "graph:str=''; layer:int=null",
      "g.clear_reference_lines()" },
    { "graph.add_shaded_band", "graph",
      "GraphProxy.add_shaded_band(x, y_low, y_high, *, color='#4C78A8', alpha=55, label='', layer=None)",
      "Add a translucent shaded confidence/tolerance band polygon.",
      "graph:str=''; x:list[float]*; y_low:list[float]*; y_high:list[float]*; color:str='#4C78A8'; alpha:float=55; label:str=''; layer:int=null",
      "g.add_shaded_band([0, 10], [95, 95], [105, 105], label='95% CI')" },
    { "graph.set_grid", "graph", "GraphProxy.set_grid(*, x_major=False, y_major=False, x_minor=False, y_minor=False, layer=None)",
      "Configure major and minor X/Y grid lines.",
      "graph:str=''; x_major:bool=false; y_major:bool=false; x_minor:bool=false; y_minor:bool=false; color:str=''; layer:int=null",
      "g.set_grid(x_major=True, y_major=True)" },
    { "graph.set_curve_style", "graph",
      "GraphProxy.set_curve_style(curve=0, *, color=None, line_width=None, lw=None, line_style=None, ls=None, symbol_style=None, marker=None, symbol_size=None, ms=None, symbol_hollow=None, symbol_skip=None, alpha=None, layer=None)",
      "Set curve color, line width/style ('-','--',':','-.'), marker shape ('o','s','^','v','D','x','+','none'), size, hollow fill, and N-th symbol decimation (symbol_skip).",
      "graph:str=''; curve:int|str=0; color:str=''; line_width:float=null; lw:float=null; line_style:str|int=null[-|--|:|-.|solid|dash|dot|dashdot|none]; ls:str|int=null; symbol_style:str|int=null[o|s|^|v|<|>|D|x|+|*|h|none]; marker:str|int=null; symbol_size:float=null; ms:float=null; symbol_hollow:bool=null; symbol_skip:int=null; alpha:float=null; layer:int=null",
      "g.set_curve_style(0, color='#0072B2', lw=2.0, ls='-', marker='o', ms=6, symbol_skip=10)" },
    { "graph.set_curve_visible", "graph", "GraphProxy.set_curve_visible(curve, visible=True, *, layer=None)",
      "Show or hide a curve without deleting it.",
      "graph:str=''; curve:int|str*; visible:bool=true; layer:int=null",
      "g.set_curve_visible(0, False)" },
    { "graph.move_curve", "graph", "GraphProxy.move_curve(curve, direction='up', *, layer=None)",
      "Reorder curve Z-order ('up', 'down', 'top', 'bottom').",
      "graph:str=''; curve:int|str*; direction:str='up'[up|down|top|bottom]; layer:int=null",
      "g.move_curve(1, 'top')" },
    { "graph.set_stack_offsets", "graph", "GraphProxy.set_stack_offsets(*, y_offset=0.0, x_offset=0.0, auto_waterfall=False, layer=None)",
      "Apply vertical/horizontal stack offsets across curves (XRD / spectroscopy waterfall).",
      "graph:str=''; y_offset:float=0.0; x_offset:float=0.0; auto_waterfall:bool=false; layer:int=null",
      "g.set_stack_offsets(auto_waterfall=True)" },
    { "graph.apply_series_scheme", "graph", "GraphProxy.apply_series_scheme(scheme='okabe_ito', *, group_size=2, layer=None)",
      "Apply multi-curve publication styling scheme ('okabe_ito', 'bw_hollow_filled', 'blue_red_hollow', 'muted_categorical', 'cool_to_warm', 'viridis', 'origin_classic').",
      "graph:str=''; scheme:str='okabe_ito'[okabe_ito|bw_hollow_filled|blue_red_hollow|muted_categorical|cool_to_warm|viridis|origin_classic]; group_size:int=2; layer:int=null",
      "g.apply_series_scheme('okabe_ito')" },
    { "graph.apply_palette", "graph",
      "GraphProxy.apply_palette(palette='okabe_ito', *, sequential=False, apply_to_symbols=True, layer=None)",
      "Apply a named colorblind-safe or journal color palette across curves.",
      "graph:str=''; palette:str='okabe_ito'[okabe_ito|Okabe-Ito|Paul Tol Bright|Paul Tol Muted|ISIJ Monochrome|Nature NPG|Science AAAS|Viridis|Plasma|Inferno|Cividis|CoolWarm]; sequential:bool=false; apply_to_symbols:bool=true; layer:int=null",
      "g.apply_palette('okabe_ito')" },
    { "graph.set_colorbar", "graph",
      "GraphProxy.set_colorbar(*, palette='viridis', reverse=None, z_min=None, z_max=None, title=None, levels=None, show_contour_lines=None, use_colormap_pen=None, log=None, cell_blocks=None, layer=None)",
      "Configure spectrogram colormap, log color scale, discrete cell blocks, Z range, contour levels, colormap contour pen, and colorbar title.",
      "graph:str=''; palette:str='viridis'[viridis|plasma|inferno|magma|cividis|jet|coolwarm|bwr|grayscale]; reverse:bool=null; z_min:float=null; z_max:float=null; title:str=null; levels:int=null; show_contour_lines:bool=null; use_colormap_pen:bool=null; log:bool=null; cell_blocks:bool=null; layer:int=null",
      "g.set_colorbar(palette='viridis', title='Intensity (a.u.)')" },
    { "graph.set_legend", "graph",
      "GraphProxy.set_legend(*, visible=None, position=None, columns=None, font_size=None, frame=None, framed=None, text=None, layer=None)",
      "Configure legend visibility, anchor ('auto', 'top_right', 'top_left', 'bottom_right', 'bottom_left', 'outside_right'), columns, font size, and frame.",
      "graph:str=''; visible:bool=null; position:str=null[auto|avoid_data|top_right|top_left|bottom_right|bottom_left|top_center|bottom_center|outside_right]; columns:int=null; font_size:float=null; frame:bool|int=null; framed:bool|int=null; x:float=null; y:float=null; text:str=null; layer:int=null",
      "g.set_legend(position='auto', frame=False)" },
    { "graph.auto_place_legend", "graph", "GraphProxy.auto_place_legend(layer=None)",
      "Automatically place the legend at the lowest-density anchor avoiding plotted curves.",
      "graph:str=''; layer:int=null", "g.auto_place_legend()" },
    { "graph.set_panel_labels", "graph",
      "GraphProxy.set_panel_labels(enabled=True, *, placement='outside_top_left', pattern='(%a)', font_pt=0)",
      "Enable or configure automatic multi-panel labels (a), (b), (c)...",
      "graph:str=''; enabled:bool=true; placement:str='outside_top_left'[outside_top_left|inside_top_left]; pattern:str='(%a)'; font_pt:int=0",
      "g.set_panel_labels(True, pattern='(%a)')" },
    { "graph.apply_square_frame", "graph", "GraphProxy.apply_square_frame()",
      "Normalize primary plot canvases to the clean 70% square journal frame.", "graph:str=''",
      "g.apply_square_frame()" },
    { "graph.drop_columns", "graph", "Session.drop_columns(graph, table, cols, *, layer=None)",
      "Plot worksheet columns onto an existing graph layer (equivalent to Explorer drag-and-drop).",
      "graph:str=''; table:str=''; cols:list*; layer:int=null",
      "app.drop_columns(g, tbl, ['Stress_2'])" },
    { "graph.add_text", "graph",
      "GraphProxy.add_text(text, x, y, *, font_size=11, color='#000000', frame=0, angle=0, layer=None)",
      "Add an Arial text callout/annotation at data coordinates (x, y) returning marker id.",
      "graph:str=''; text:str*; x:float=0.0; y:float=0.0; font_size:float=11; color:str='#000000'; frame:bool|int=0; angle:float=0; layer:int=null",
      "m = g.add_text('Yield Point', x=0.22, y=415.0)" },
    { "graph.add_arrow", "graph",
      "GraphProxy.add_arrow(x1, y1, x2, y2, *, color='#000000', width=1.0, end_arrow=True, start_arrow=False, head_length=9, head_angle=28, layer=None)",
      "Add a line/arrow marker between data coordinates (x1, y1) and (x2, y2) returning marker id.",
      "graph:str=''; x1:float*; y1:float*; x2:float*; y2:float*; color:str='#000000'; width:float=1.0; style:str='solid'; end_arrow:bool=true; start_arrow:bool=false; head_length:int=9; head_angle:int=28; layer:int=null",
      "a = g.add_arrow(0.5, 450, 0.22, 412)" },
    { "graph.remove_marker", "graph",
      "GraphProxy.remove_marker(marker_id=None, *, kind=None, index=None, clear_all=False, layer=None)",
      "Remove a text or arrow marker by marker_id, 0-based index (+ kind='text'|'arrow'|'line'|'all'), or clear_all=True.",
      "graph:str=''; marker_id:int=null; kind:str=null[text|arrow|line|all]; index:int=null; all:bool=false; layer:int=null",
      "g.remove_marker(m['id'])" },
    { "graph.mask_region", "graph",
      "GraphProxy.mask_region(x_min, x_max, *, y_min=None, y_max=None, curve=None, unmask=False, layer=None)",
      "Mask (or unmask) data points falling inside a rectangular data ROI (optional y_min/y_max default to full Y span).",
      "graph:str=''; x_min:float*; x_max:float*; y_min:float=null; y_max:float=null; curve:int|str=null; unmask:bool=false; layer:int=null",
      "g.mask_region(4.8, 5.2)" },
    { "graph.clear_masks", "graph", "GraphProxy.clear_masks(curve=None, *, layer=None)",
      "Clear all masked points on one curve or all curves on a layer.",
      "graph:str=''; curve:int|str=null; layer:int=null", "g.clear_masks()" },
    { "graph.export", "graph", "GraphProxy.export(path, *, dpi=300, transparent=False)",
      "Export 2D or 3D graph to PDF, SVG, EPS, EMF, PNG, or TIFF.",
      "graph:str=''; path:str*; dpi:int=300; transparent:bool=false; format:str=''",
      "g.export('figure1.pdf', dpi=600)" },
    { "graph.copy_image", "graph", "GraphProxy.copy_image(dpi=300)",
      "Copy rendered graph raster image to the system clipboard.", "graph:str=''; dpi:int=300",
      "g.copy_image(dpi=300)" },
    { "graph.copy_ole", "graph", "GraphProxy.copy_ole(*, preset=None, active_layer_only=False)",
      "Place editable ToriGraph OLE object + vector EMF on Windows clipboard for Word/PowerPoint.",
      "graph:str=''; preset:str=''; active_layer_only:bool=false", "g.copy_ole()" },
    { "graph.export_ole", "graph",
      "GraphProxy.export_ole(*, emf_path=None, ole_path=None, dpi=300, preset=None, active_layer_only=False, include_base64=True, skip_project_stream=False, embed_scope='graph')",
      "Export editable ToriGraph OLE Compound Document (.bin), crisp vector EMF preview, .sciprj XML, and physical dimensions (mm/hm) for zero-COM PowerPoint (.pptx) embedding.",
      "graph:str=''; emf_path:str=''; ole_path:str=''; dpi:int=300; preset:str=''; active_layer_only:bool=false; include_base64:bool=true; skip_project_stream:bool=false; embed_scope:str='graph'[graph|project]",
      "pkg = g.export_ole(dpi=300)" },
    { "graph.auto_layout", "graph",
      "GraphProxy.auto_layout(*, pad_fraction=0.05, allow_page_resize=True, allow_tick_rotation=True, allow_outside_legend=True, max_passes=3, dry_run=False)",
      "Deterministic 5-stage self-healing layout solver fixing disabled axes, clipped curves, multi-panel canvas mismatch, tick collisions, and legend/callout overlaps.",
      "graph:str=''; pad_fraction:float=0.05; allow_page_resize:bool=true; allow_tick_rotation:bool=true; allow_outside_legend:bool=true; max_passes:int=3; dry_run:bool=false",
      "res = g.auto_layout()" },
    { "graph.fix_layout", "graph",
      "GraphProxy.auto_layout(*, pad_fraction=0.05, allow_page_resize=True, allow_tick_rotation=True, allow_outside_legend=True, max_passes=3, dry_run=False)",
      "Alias of graph.auto_layout.",
      "graph:str=''; pad_fraction:float=0.05; allow_page_resize:bool=true; allow_tick_rotation:bool=true; allow_outside_legend:bool=true; max_passes:int=3; dry_run:bool=false",
      "res = g.auto_layout()" },
    { "graph.to_python", "graph",
      "GraphProxy.to_python_script(path=None, *, embed_data='inline', standalone=True)",
      "Generate a clean, runnable 'import torigraph as tg' Python script reproducing the target graph's data, styles, axes, layers, insets, reference lines, and annotations.",
      "graph:str=''; embed_data:str='inline'[inline|csv|reference]; standalone:bool=true; path:str=''",
      "code = g.to_python_script(embed_data='inline', standalone=True)" },
    { "graph.set_3d_options", "graph",
      "GraphProxy.set_3d_options(*, title=None, x_rotation=None, y_rotation=None, z_rotation=None, zoom=None, orthogonal=None)",
      "Configure 3D Graph camera angles, zoom, projection, resolution, and axes labels.",
      "graph:str=''; title:str=null; x_rotation:float=null; y_rotation:float=null; z_rotation:float=null; zoom:float=null; orthogonal:bool=null; resolution:int=null",
      "g3.set_3d_options(x_rotation=30, z_rotation=45)" },

    // 6. Curve Fitting & Peak Analysis (analysis.*)
    { "analysis.list_fit_models", "analysis", "Session.list_fit_models(category=None)",
      "List built-in Origin-class nonlinear fit models, formulas, and parameter names.",
      "category:str=''", "app.list_fit_models()" },
    { "analysis.fit_linear", "analysis",
      "Session.fit_linear(*, graph=None, curve=None, table=None, x_col=None, y_col=None, x_min=None, x_max=None, plot_curve=True, fit_label=None)",
      "Perform weighted/unweighted linear regression returning slope, intercept, errors, R2, Pearson r, and ANOVA p-value.",
      "graph:str=''; curve:int|str=null; table:str=''; x_col:int|str=null; y_col:int|str=null; x_min:float=null; x_max:float=null; plot_curve:bool=true; fit_label:str=''",
      "res = app.fit_linear(graph=g, x_min=0.0, x_max=0.2)" },
    { "analysis.fit_york", "analysis",
      "Session.fit_york(table=None, x_col=None, y_col=None, x_err_col=None, y_err_col=None)",
      "Perform York bivariate errors-in-variables linear regression.",
      "graph:str=''; table:str=''; x_col:int|str=null; y_col:int|str=null; x_err_col:int|str=''; y_err_col:int|str=''",
      "app.fit_york(tbl, 'X', 'Y', 'sX', 'sY')" },
    { "analysis.fit_polynomial", "analysis",
      "Session.fit_polynomial(*, order=2, graph=None, curve=None, table=None, x_col=None, y_col=None, plot_curve=True, fit_label=None)",
      "Fit an N-th order polynomial returning coefficients, errors, R2, and optional fit curve.",
      "order:int=2; graph:str=''; curve:int|str=null; table:str=''; x_col:int|str=null; y_col:int|str=null; plot_curve:bool=true; fit_label:str=''",
      "res = app.fit_polynomial(graph=g, order=3)" },
    { "analysis.fit_nonlinear", "analysis",
      "Session.fit_nonlinear(*, model=None, formula=None, graph=None, curve=None, init_params=None, bounds=None, fixed=None, max_iter=1000, plot_curve=True, fit_label=None)",
      "Fit a built-in nonlinear model ('Gauss'/'Gaussian', 'GaussAmp', 'Lorentz'/'Lorentzian', 'PseudoVoigt1', 'Boltzmann', 'Arrhenius', 'Hollomon', 'Voce') or custom muParser formula.",
      "model:str=''; formula:str=''; param_names:list=null; graph:str=''; curve:int|str=null; table:str=''; x_col:int|str=null; y_col:int|str=null; init_params:dict=null; bounds:dict=null; fixed:list=null; max_iter:int=1000; tolerance:float=1e-7; points:int=200; plot_curve:bool=true; fit_label:str=''",
      "res = app.fit_nonlinear(graph=g, model='Gaussian')" },
    { "analysis.fit_global", "analysis",
      "Session.fit_global(*, graph=None, curves=None, model=None, formula=None, shared_params=None, max_iter=200)",
      "Simultaneously fit multiple curves sharing global physical parameters.",
      "graph:str=''; curves:list=null; model:str=''; formula:str=''; param_names:list=null; shared_params:list=null; max_iter:int=200; tolerance:float=0.0001",
      "app.fit_global(graph=g, model='Arrhenius', shared_params=['Ea'])" },
    { "analysis.fit_piecewise", "analysis", "Session.fit_piecewise(*, segments=2, graph=None, curve=None)",
      "Fit continuous piecewise-linear segments with automatic breakpoint optimization.",
      "segments:int=2; graph:str=''; curve:int|str=null; table:str=''",
      "app.fit_piecewise(graph=g, segments=2)" },
    { "analysis.fit_robust", "analysis", "Session.fit_robust(*, method='huber', graph=None, curve=None)",
      "Perform outlier-resistant linear regression ('huber', 'bisquare', 'theil_sen').",
      "method:str='huber'[huber|bisquare|theil_sen]; graph:str=''; curve:int|str=null",
      "app.fit_robust(graph=g, method='theil_sen')" },
    { "analysis.fit_quantile", "analysis", "Session.fit_quantile(*, tau=0.5, graph=None, curve=None)",
      "Perform quantile regression for conditional quantile tau in (0, 1).",
      "tau:float=0.5; graph:str=''; curve:int|str=null", "app.fit_quantile(graph=g, tau=0.5)" },
    { "analysis.fit_logistic", "analysis", "Session.fit_logistic(table=None, x_col=None, y_col=None)",
      "Fit binary logistic regression P(Y=1|X) returning odds ratio, AUC, and p-values.",
      "table:str=''; x_col:int|str=null; y_col:int|str*", "app.fit_logistic(tbl, 'Dose', 'Response')" },
    { "analysis.fit_mlr", "analysis", "Session.fit_mlr(table=None, x_cols=None, y_col=None)",
      "Fit multiple linear regression across multiple predictor columns with VIF diagnostics.",
      "table:str=''; x_cols:list=null; y_col:int|str*", "app.fit_mlr(tbl, ['C_pct', 'Mn_pct'], 'UTS')" },
    { "analysis.fit_implicit_odr", "analysis",
      "Session.fit_implicit_odr(table=None, x_col=None, y_col=None, *, model='circle')",
      "Fit implicit geometric orthogonal distance regression ('circle', 'ellipse', or custom F(x,y)=0).",
      "graph:str=''; table:str=''; x_col:int|str=null; y_col:int|str=null; model:str='circle'[circle|ellipse|superellipse|custom]",
      "app.fit_implicit_odr(tbl, 'X', 'Y', model='circle')" },
    { "analysis.fit_surface_3d", "analysis", "Session.fit_surface_3d(*, matrix=None, model='gaussian2d')",
      "Fit a 3D surface z = f(x, y) ('plane', 'paraboloid', 'gaussian2d', 'lorentzian2d', or custom).",
      "matrix:str=''; table:str=''; model:str='gaussian2d'[plane|paraboloid|gaussian2d|lorentzian2d|custom]",
      "app.fit_surface_3d(matrix=m, model='gaussian2d')" },
    { "analysis.compare_models", "analysis",
      "Session.compare_models(*, models=None, graph=None, curve=None)",
      "Rank candidate nonlinear models by AICc, BIC, and F-test.",
      "models:list=null; graph:str=''; curve:int|str=null",
      "app.compare_models(graph=g, models=['Gauss', 'Lorentz', 'PseudoVoigt1'])" },
    { "analysis.compare_datasets", "analysis", "Session.compare_datasets(*, graph=None, model='ExpDec1')",
      "Test parameter invariance across multiple curves using nested global F-test.",
      "graph:str=''; model:str='ExpDec1'; curves:list=null", "app.compare_datasets(graph=g, model='Gauss')" },
    { "analysis.analyze_peaks", "analysis",
      "Session.analyze_peaks(*, graph=None, curve=None, profile='PseudoVoigt', peak_count=None, baseline='als', plot_components=True)",
      "Baseline-subtract and deconvolve multi-peak spectra (XRD, XPS, Raman, TDS) with baseline-aware component & envelope curves.",
      "graph:str=''; curve:int|str=null; table:str=''; profile:str='PseudoVoigt'[Gauss|GaussAmp|Lorentz|PseudoVoigt|Voigt|PearsonVII|BiGaussian|EMG|LogNormal|DoniachSunjic]; peak_count:int=null; baseline:str='als'[none|constant|linear|als|anchors]; plot_components:bool=true",
      "res = app.analyze_peaks(graph=g, profile='PseudoVoigt', baseline='als')" },
    { "analysis.batch_peaks", "analysis",
      "Session.batch_peaks(table=None, x_col=None, y_cols=None, *, profile='PseudoVoigt')",
      "Run automated peak deconvolution across a batch of Y columns.",
      "graph:str=''; table:str=''; x_col:int|str=null; y_cols:list=null; profile:str='PseudoVoigt'; baseline:str='als'",
      "app.batch_peaks(tbl, 'TwoTheta', ['S1', 'S2', 'S3'])" },

    // 7. Interactive Gadgets (gadget.*)
    { "gadget.attach", "gadget", "Session.attach_gadget(graph=None, gadget_type='roi_stats', *, x_min=None, x_max=None)",
      "Attach a live ROI/cursor gadget ('roi_stats', 'quick_fit', 'quick_peaks', 'integrate', 'delta_cursor', 'vertical_cursor', 'image_profile').",
      "graph:str=''; type:str='roi_stats'; curve:int|str=null; x_min:float=null; x_max:float=null",
      "app.attach_gadget(g, 'roi_stats', x_min=1.0, x_max=5.0)" },
    { "gadget.set_roi", "gadget", "Session.set_gadget_roi(graph=None, *, x_min=None, x_max=None)",
      "Move an active gadget's ROI or cursor coordinates and recalculate.",
      "graph:str=''; x_min:float=null; x_max:float=null; x1:float=null; x2:float=null",
      "app.set_gadget_roi(g, x_min=2.0, x_max=6.0)" },
    { "gadget.query", "gadget", "Session.query_gadget(graph=None, *, create_table=False)",
      "Read live metrics from the active gadget on a graph.",
      "graph:str=''; create_table:bool=false", "app.query_gadget(g)" },
    { "gadget.clear", "gadget", "Session.clear_gadgets(graph=None)",
      "Detach and remove the active gadget from a graph.", "graph:str=''", "app.clear_gadgets(g)" },

    // 8. Signal Processing (signal.*)
    { "signal.smooth", "signal", "Session.smooth(*, method='savgol', window=7, poly_order=2, graph=None, curve=None)",
      "Smooth noisy curve data ('savgol', 'fft', 'average', 'median', 'lowess', 'whittaker', 'gaussian').",
      "method:str='savgol'[savgol|fft|average|median|lowess|whittaker|gaussian]; window:int=7; poly_order:int=2; span:float=0.2; lambda:float=100.0; sigma:float=null; graph:str=''; curve:int|str=null",
      "app.smooth(graph=g, method='savgol', window=9, poly_order=3)" },
    { "signal.savgol_derivative", "signal", "Session.savgol_derivative(*, order=1, window=9, poly_order=3, graph=None)",
      "Compute regularized Savitzky-Golay 1st/2nd/3rd derivative curve.",
      "order:int=1; window:int=9; poly_order:int=3; graph:str=''; curve:int|str=null",
      "app.savgol_derivative(graph=g, order=1, window=11)" },
    { "signal.fft", "signal", "Session.fft(*, graph=None, table=None, sampling=None, inverse=False)",
      "Compute 1D forward or inverse Fast Fourier Transform.",
      "graph:str=''; curve:int|str=null; table:str=''; real_col:int|str=null; sampling:float=null; inverse:bool=false",
      "app.fft(graph=g)" },
    { "signal.fft_filter", "signal", "Session.fft_filter(*, filter_type='lowpass', cutoff=1.0, graph=None)",
      "Apply FFT frequency-domain filter ('lowpass', 'highpass', 'bandpass', 'bandblock').",
      "filter_type:str='lowpass'[lowpass|highpass|bandpass|bandblock]; cutoff:float=1.0; high_cutoff:float=null; graph:str=''",
      "app.fft_filter(graph=g, filter_type='lowpass', cutoff=50.0)" },
    { "signal.iir_filter", "signal", "Session.iir_filter(*, design='butterworth', mode='lowpass', cutoff=10.0, order=4, graph=None)",
      "Apply zero-phase forward-backward IIR digital filter (Butterworth/Bessel/Chebyshev).",
      "design:str='butterworth'[butterworth|chebyshev1|bessel]; mode:str='lowpass'[lowpass|highpass|bandpass|bandstop]; order:int=4; cutoff:float=10.0; graph:str=''",
      "app.iir_filter(graph=g, cutoff=25.0, order=4)" },
    { "signal.stft", "signal", "Session.stft(*, window_size=64, overlap=32, graph=None)",
      "Compute Short-Time Fourier Transform spectrogram matrix.",
      "window_size:int=64; overlap:int=32; hop_size:int=null; graph:str=''", "app.stft(graph=g, window_size=128)" },
    { "signal.envelope", "signal", "Session.envelope(*, graph=None, curve=None)",
      "Compute upper/lower Hilbert analytic signal envelope.", "graph:str=''; curve:int|str=null",
      "app.envelope(graph=g)" },
    { "signal.wavelet_cwt", "signal", "Session.wavelet_cwt(*, scales=32, graph=None)",
      "Compute Morlet Continuous Wavelet Transform scalogram.",
      "scales:int=32; wavelet:str='morlet'[morlet|ricker]; graph:str=''",
      "app.wavelet_cwt(graph=g, scales=32)" },
    { "signal.wavelet_denoise", "signal", "Session.wavelet_denoise(*, levels=4, graph=None)",
      "Perform Haar/Daubechies wavelet soft-threshold denoising.", "levels:int=4; graph:str=''",
      "app.wavelet_denoise(graph=g, levels=4)" },
    { "signal.welch_psd", "signal", "Session.welch_psd(*, segment_len=128, graph=None)",
      "Estimate power spectral density via Welch overlapped periodograms.",
      "segment_len:int=128; graph:str=''", "app.welch_psd(graph=g)" },
    { "signal.coherence", "signal",
      "Session.coherence(table=None, x_col=None, y1_col=None, y2_col=None, *, graph=None, curve_a=None, curve_b=None, segment_len=128)",
      "Compute cross-correlation and magnitude-squared coherence between two signals (from table columns or graph curves).",
      "table:str=''; x_col:int|str=null; y1_col:int|str=null; y2_col:int|str=null; graph:str=''; curve_a:int|str=null; curve_b:int|str=null; segment_len:int=128",
      "app.coherence(tbl, 'Time', 'Sig1', 'Sig2')" },
    { "signal.decimate", "signal", "Session.decimate(*, factor=4, graph=None)",
      "Anti-aliased signal downsampling/decimation by an integer factor.",
      "factor:int=4; graph:str=''", "app.decimate(graph=g, factor=5)" },
    { "signal.fsd", "signal", "Session.fsd(*, gamma=5.0, smoothing=0.2, graph=None)",
      "Resolution-enhancing Fourier Self-Deconvolution for overlapping IR/Raman bands.",
      "gamma:float=5.0; smoothing:float=0.2; graph:str=''",
      "app.fsd(graph=g, gamma=2.5)" },

    // 9. Curve Math & Materials Science (curvemath.*)
    { "curvemath.eval", "curvemath", "Session.curve_eval(graph=None, curve_a=0, curve_b=1, *, op='sub')",
      "Binary math (+, -, *, /) between two curves interpolated onto a common X grid.",
      "graph:str=''; curve_a:int|str=0; curve_b:int|str=1; op:str='sub'[add|sub|mul|div]",
      "app.curve_eval(g, 'Sample', 'Blank', op='sub')" },
    { "curvemath.average_curves", "curvemath", "Session.average_curves(graph=None, *, curves=None, points=200)",
      "Compute mean curve and +/- SD shaded band across replicate curves.",
      "graph:str=''; curves:list=null; points:int=200; sd_band:bool=true", "app.average_curves(g)" },
    { "curvemath.normalize", "curvemath", "Session.normalize_curve(*, mode='minmax', graph=None, curve=None)",
      "Normalize curve intensities ('max', 'minmax', 'area', 'zscore', 'ref_x').",
      "mode:str='minmax'[max|minmax|area|zscore|ref_x]; ref_x:float=0.0; graph:str=''; curve:int|str=null",
      "app.normalize_curve(graph=g, mode='max')" },
    { "curvemath.integrate", "curvemath",
      "Session.integrate_curve(*, graph=None, curve=None, x_min=None, x_max=None, method='linear', plot_curve=False)",
      "Numerically integrate curve area over [x_min, x_max] (strictly distinct X; optional cumulative integral curve when plot_curve=True).",
      "graph:str=''; curve:int|str=null; x_min:float=null; x_max:float=null; method:str='linear'[linear|cubic|akima]; plot_curve:bool=false",
      "res = app.integrate_curve(graph=g, x_min=0.0, x_max=10.0)" },
    { "curvemath.differentiate", "curvemath", "Session.differentiate_curve(*, graph=None, curve=None, x_min=None, x_max=None)",
      "Compute central-difference derivative dY/dX (>= 4 valid points) returning new graph, table, and output_curve.",
      "graph:str=''; curve:int|str=null; x_min:float=null; x_max:float=null",
      "res = app.differentiate_curve(graph=g)" },
    { "curvemath.interpolate", "curvemath",
      "Session.interpolate_curve(*, method='spline', points=200, x_min=None, x_max=None, graph=None, curve=None)",
      "Interpolate curve onto a uniform grid ('linear' >=2 pts, 'spline'/'cubic' >=4 pts, 'akima' >=5 pts).",
      "method:str='spline'[linear|spline|akima]; points:int=200; x_min:float=null; x_max:float=null; graph:str=''; curve:int|str=null",
      "app.interpolate_curve(graph=g, method='akima', points=300)" },
    { "curvemath.subtract_line", "curvemath",
      "Session.subtract_line(*, x1=0.0, y1=0.0, x2=1.0, y2=0.0, graph=None)",
      "Subtract a two-point linear baseline from a curve.",
      "x1:float*; y1:float*; x2:float*; y2:float*; graph:str=''; curve:int|str=null; in_place:bool=false",
      "app.subtract_line(graph=g, x1=0, y1=10, x2=100, y2=15)" },
    { "curvemath.intersections", "curvemath", "Session.intersections(graph=None, curve_a=0, curve_b=1)",
      "Find all (x, y) intersection points between two curves.",
      "graph:str=''; curve_a:int|str=0; curve_b:int|str=1; mark:bool=true",
      "pts = app.intersections(g, 0, 1)" },
    { "curvemath.lookup_xy", "curvemath", "Session.lookup_xy(*, x=None, y=None, graph=None, curve=None)",
      "Interpolate Y at given X (or solve all X roots where curve crosses Y).",
      "x:float=null; y:float=null; graph:str=''; curve:int|str=null",
      "app.lookup_xy(graph=g, x=0.2)" },
    { "curvemath.polygon_area", "curvemath", "Session.polygon_area(*, graph=None, curve=None)",
      "Compute enclosed hysteresis/cyclic loop polygon area via Shoelace theorem.",
      "graph:str=''; curve:int|str=null", "app.polygon_area(graph=g)" },
    { "curvemath.fill_between", "curvemath",
      "Session.fill_between(graph=None, curve_a=0, curve_b=1, *, color='#4C78A8', alpha=55)",
      "Shade the region between two curves on a graph layer.",
      "graph:str=''; curve_a:int|str=0; curve_b:int|str=1; color:str='#4C78A8'; alpha:float=55",
      "app.fill_between(g, 0, 1)" },
    { "curvemath.offset_yield", "curvemath",
      "Session.analyze_tensile(graph=None, curve=None, *, offset=0.002, strain_in_percent=None, elastic_min=None, elastic_max=None, plot_construction=True, add_callout=True, create_true_stress_table=False)",
      "Materials science tensile test analyzer: Young's modulus E, elastic R2, 0.2% offset proof stress (sigma_0.2), UTS, fracture point, dashed offset construction line, and optional true stress-strain table.",
      "graph:str=''; curve:int|str=null; table:str=''; x_col:int|str=null; y_col:int|str=null; offset:float=0.002; strain_in_percent:bool|null=null; elastic_min:float=null; elastic_max:float=null; plot_construction:bool=true; add_callout:bool=true; create_true_stress_table:bool=false",
      "res = g.analyze_tensile(offset=0.002, plot_construction=True)" },
    { "curvemath.arrhenius", "curvemath",
      "Session.arrhenius(table=None, temp_col=0, rate_col=1, *, temp_in_celsius=False)",
      "Build an Arrhenius plot (ln k vs 1000/T with linked top-T axis) and extract activation energy Ea.",
      "table:str=''; temp_col:int|str=0; rate_col:int|str=1; temp_in_celsius:bool=false; use_1000_over_t:bool=true",
      "res = app.arrhenius(tbl, 'Temp_K', 'Rate_k')" },
    { "curvemath.tauc", "curvemath",
      "Session.tauc(table=None, x_col=0, y_col=1, *, direct=True, x_is_wavelength_nm=True)",
      "Construct a Tauc optical bandgap plot and extrapolate Eg in eV.",
      "table:str=''; x_col:int|str=0; y_col:int|str=1; direct:bool=true; x_is_wavelength_nm:bool=true",
      "res = app.tauc(tbl, 'Energy_eV', 'Alpha', direct=True, x_is_wavelength_nm=False)" },
    { "curvemath.eis_fit", "curvemath",
      "Session.eis_fit(table=None, freq_col=0, z_re_col=1, z_im_col=2, *, model='Randles_CPE')",
      "Fit electrochemical impedance Nyquist & Bode data to Randles or Randles-CPE equivalent circuits.",
      "table:str=''; freq_col:int|str=0; z_re_col:int|str=1; z_im_col:int|str=2; model:str='Randles_CPE'[Randles|Randles_CPE|RandlesWarburg]",
      "res = app.eis_fit(tbl, 'Freq', 'Z_re', 'Z_im')" },
    { "curvemath.kissinger", "curvemath",
      "Session.analyze_kissinger(table=None, rate_col=None, temp_col=None, *, heating_rates=None, peak_temps=None, rate_in_k_per_hour=False, temp_in_celsius=True, top_axis_in_celsius=True, temp_min=None, temp_max=None)",
      "Kissinger DSC/TDS thermal activation analysis (ln(beta/Tp^2) vs 1000/Tp with linked top Celsius axis and optional [temp_min, temp_max] peak window) returning Ea in kJ/mol and eV.",
      "heating_rates:list[float]=null; peak_temps:list[float]=null; y_cols:list=null; table:str=''; rate_col:int|str=0; temp_col:int|str|list=1; rate_in_k_per_hour:bool=false; temp_in_celsius:bool=true; top_axis_in_celsius:bool=true; top_axis_celsius:bool=true; temp_min:float=null; temp_max:float=null; plot:bool=true",
      "res = app.analyze_kissinger(heating_rates=[5, 10, 20, 40], peak_temps=[285, 298, 312, 327])" },
    { "curvemath.kocks_mecking", "curvemath",
      "Session.analyze_work_hardening(*, graph=None, curve=None, table=None, smooth_window=11, create_plot=True, plot_mode='considere')",
      "Kocks-Mecking work-hardening rate (theta = d_sigma_t / d_epsilon_t vs true strain or Stage-III true stress), Considere necking instability, Hollomon (K, n), and Voce saturation stress.",
      "graph:str=''; curve:int|str=null; table:str=''; x_col:int|str=null; y_col:int|str=null; smooth_window:int=11; create_plot:bool=true; plot_mode:str='considere'[considere|kocks_mecking]",
      "res = app.analyze_work_hardening(graph=g, smooth_window=11)" },
    { "curvemath.xrd_analyze", "curvemath",
      "Session.analyze_xrd(*, graph=None, curve=None, wavelength=1.540598, crystal_system='fcc', c_over_a=1.633, strip_kalpha2=False)",
      "Automated powder XRD analyzer: Rachinger K-alpha2 stripping, Scherrer crystallite size, Williamson-Hall microstrain, FCC/BCC/SC/HCP (hkl) indexing, lattice parameter a (and c), and linked top d-spacing axis.",
      "graph:str=''; curve:int|str=null; table:str=''; x_col:int|str=null; y_col:int|str=null; wavelength:float=1.540598; xray_energy_kev:float=null; radiation:str=''; instr_fwhm:float=0.05; scherrer_k:float=0.90; crystal_system:str='fcc'[fcc|bcc|sc|hcp|none]; c_over_a:float=1.633; strip_kalpha2:bool=false; add_top_d_axis:bool=true; create_wh_plot:bool=true",
      "res = app.analyze_xrd(graph=g, crystal_system='fcc')" },
    { "curvemath.bragg_convert", "curvemath",
      "Session.bragg_convert(*, mode='2theta_to_d', wavelength=1.540598, values=None, table=None, col=0, graph=None)",
      "Convert diffraction coordinates between 2theta (deg), d-spacing (Angstrom), and scattering vector q (Angstrom^-1) on arrays, worksheet columns, or linked top axes.",
      "mode:str='2theta_to_d'[2theta_to_d|d_to_2theta|2theta_to_q|q_to_2theta|d_to_q|q_to_d]; wavelength:float=1.540598; xray_energy_kev:float=null; radiation:str=''; values:list[float]=null; table:str=''; col:int|str=0; out_col:str=''; graph:str=''; add_top_axis:bool=false",
      "res = app.bragg_convert(mode='2theta_to_d', values=[43.32, 50.45, 74.13])" },
    { "curvemath.vasp_dos", "curvemath",
      "Session.plot_vasp_dos(table=None, energy_col=0, dos_cols=None, *, doscar_path=None, down_dos_cols=None, fermi_energy=0.0, vertical_energy=False)",
      "Plot spin-resolved or total VASP electronic Density of States (E - E_F, Fermi reference line, bandgap & d-band center).",
      "table:str=''; doscar_path:str=''; energy_col:int|str=0; dos_cols:list=null; down_dos_cols:list=null; fermi_energy:float=0.0; vertical_energy:bool=false",
      "res = app.plot_vasp_dos(tbl, 'Energy', ['TDOS'], fermi_energy=5.42)" },
    { "curvemath.vasp_bands", "curvemath",
      "Session.plot_vasp_bands(table=None, k_col=0, band_cols=None, *, eigenval_path=None, k_ticks=None, k_labels=None, fermi_energy=0.0, e_min=-6.0, e_max=6.0)",
      "Plot VASP electronic band structure with high-symmetry k-point dividers, Gamma symbol formatting, Fermi level, and bandgap extraction.",
      "table:str=''; eigenval_path:str=''; k_col:int|str=0; band_cols:list=null; k_ticks:list[float]=null; k_labels:list[str]=null; fermi_energy:float=0.0; e_min:float=-6.0; e_max:float=6.0",
      "res = app.plot_vasp_bands(tbl, 'k_dist', k_ticks=[0.0, 1.0, 1.8], k_labels=['L', 'G', 'X'])" },
    { "curvemath.vasp_band_dos", "curvemath",
      "Session.plot_vasp_band_dos(band_table=None, dos_table=None, *, eigenval_path=None, doscar_path=None, k_col=0, band_cols=None, energy_col=0, dos_cols=None, down_dos_cols=None, k_ticks=None, k_labels=None, fermi_energy=0.0, e_min=-6.0, e_max=6.0)",
      "Plot coupled 2-panel VASP Band Structure (left) + vertical-energy Density of States (right) sharing the E - E_F energy axis.",
      "band_table:str=''; dos_table:str=''; eigenval_path:str=''; doscar_path:str=''; k_col:int|str=0; band_cols:list=null; energy_col:int|str=0; dos_cols:list=null; down_dos_cols:list=null; k_ticks:list[float]=null; k_labels:list[str]=null; fermi_energy:float=0.0; e_min:float=-6.0; e_max:float=6.0",
      "res = app.plot_vasp_band_dos(band_table=t_band, dos_table=t_dos, fermi_energy=5.42)" },

    // 10. Statistical Analysis (stats.*)
    { "stats.normality", "stats", "Session.normality(table=None, col=0)",
      "Run Shapiro-Wilk / Anderson-Darling / Kolmogorov-Smirnov normality tests.",
      "table:str=''; col:int|str=0; graph:str=''", "app.normality(tbl, 'Strength')" },
    { "stats.outlier", "stats",
      "Session.outlier(table=None, col=0, *, method='grubbs', alpha=0.05, mask_outliers=False)",
      "Run Grubbs / GESD / Dixon Q outlier detection with optional automatic row masking.",
      "table:str=''; col:int|str=0; method:str='grubbs'[grubbs|gesd|dixon]; alpha:float=0.05; max_outliers:int=5; mask_outliers:bool=false",
      "app.outlier(tbl, 'Strength', mask_outliers=True)" },
    { "stats.anova_oneway", "stats", "Session.anova_oneway(table=None, cols=None, *, posthoc='tukey', graph=None, add_brackets=None)",
      "Run one-way ANOVA with Tukey HSD / Holm / Games-Howell / Dunnett pairwise comparisons and optional significance brackets.",
      "table:str=''; cols:list=null; posthoc:str='tukey'[holm|tukey|games_howell|dunnett]; control:int|str=null; graph:str=''; add_brackets:bool=null",
      "app.anova_oneway(tbl, ['A', 'B', 'C'])" },
    { "stats.anova_twoway", "stats",
      "Session.anova_twoway(table=None, factor_a_col=0, factor_b_col=1, response_col=2, *, plot=True)",
      "Run balanced/unbalanced two-way ANOVA with interaction effect.",
      "table:str=''; factor_a_col:int|str=0; factor_b_col:int|str=1; response_col:int|str=2; plot:bool=true",
      "app.anova_twoway(tbl, 'Alloy', 'Temp', 'Yield')" },
    { "stats.nonparametric", "stats",
      "Session.nonparametric(table=None, cols=None, *, test='mann_whitney', graph=None, add_brackets=None)",
      "Run Mann-Whitney U, Wilcoxon signed-rank, Kruskal-Wallis (+ Dunn post-hoc brackets), or Friedman test.",
      "table:str=''; cols:list=null; test:str='mann_whitney'[mann_whitney|wilcoxon|kruskal_wallis|friedman]; graph:str=''; add_brackets:bool=null",
      "app.nonparametric(tbl, ['Before', 'After'], test='wilcoxon')" },
    { "stats.paired_plot", "stats", "Session.paired_plot(table=None, col_a=0, col_b=1)",
      "Create a Gardner-Altman paired estimation plot with mean difference CI.",
      "table:str=''; col_a:int|str=0; col_b:int|str=1; wilcoxon:bool=false",
      "app.paired_plot(tbl, 'Pre', 'Post')" },
    { "stats.bland_altman", "stats", "Session.bland_altman(table=None, col_a=0, col_b=1)",
      "Create a Bland-Altman method-agreement plot with bias and +/-1.96 SD limits of agreement.",
      "table:str=''; col_a:int|str=0; col_b:int|str=1", "app.bland_altman(tbl, 'MethodA', 'MethodB')" },
    { "stats.passing_bablok", "stats", "Session.passing_bablok(table=None, x_col=0, y_col=1)",
      "Run Passing-Bablok / Deming method-comparison regression.",
      "table:str=''; x_col:int|str=0; y_col:int|str=1; lambda:float=1.0; plot:bool=true",
      "app.passing_bablok(tbl, 'Ref', 'Test')" },
    { "stats.weibull", "stats", "Session.weibull(table=None, col=0, *, censor_col=None, three_param=False, mle=False, plot=True)",
      "Fit 2-parameter or 3-parameter Weibull modulus/strength distribution via rank regression or right-censored MLE (ceramics & fatigue).",
      "table:str=''; col:int|str=0; censor_col:int|str=null; censor_coding:int=0; three_param:bool=false; mle:bool=false; plot:bool=true",
      "app.weibull(tbl, 'FractureMPa', three_param=True)" },
    { "stats.probability_plot", "stats",
      "Session.probability_plot(table=None, col=0, *, distribution='lognormal', position=None, return_period=0, plot=True)",
      "Lognormal or Gumbel (largest extreme value) probability plot by rank plotting positions with a least-squares line (e.g. CT maximum defect size).",
      "table:str=''; col:int|str=0; distribution:str='lognormal'[lognormal|gumbel]; position:str=''[median|mean|hazen|blom]; return_period:float=0; plot:bool=true",
      "app.probability_plot(tbl, 'sqrt_area_max', distribution='gumbel', return_period=500)" },
    { "plot.color_scatter", "plot",
      "Session.plot_color_scatter(table=None, x=0, y=1, z=2, *, size=None, palette='viridis', reverse=False)",
      "Scatter plot whose symbol colour follows a Z column (optionally size from a 4th column) with a colour bar on the right axis.",
      "table:str=''; x:int|str=0; y:int|str=1; z:int|str=2; size:int|str=null; palette:str='viridis'; reverse:bool=false",
      "g = app.plot_color_scatter(tbl, 'x', 'y', 'strain')" },
    { "plot.histogram_cumulative", "plot",
      "Session.plot_histogram_cumulative(table=None, col=0, *, bin_width=0, begin=0)",
      "Frequency histogram with the cumulative percentage (0-100 %) on the right axis.",
      "table:str=''; col:int|str=0; bin_width:float=0; begin:float=0",
      "app.plot_histogram_cumulative(tbl, 'DefectSize', bin_width=5)" },
    { "graph.add_error_band", "graph",
      "GraphProxy.add_error_band(curve, err, *, err_minus=None, table=None, alpha=64, layer=None)",
      "Shade Y - error ... Y + error of a data curve from error column(s) (asymmetric with err_minus); saved with the project.",
      "graph:str=''; curve:int|str*; err:int|str*; err_minus:int|str=null; table:str=''; alpha:float=64; layer:int=null",
      "g.add_error_band('Mean', 'SD')" },
    { "plot.density2d", "plot",
      "Session.plot_density2d(table=None, x=0, y=1, *, bins_x=0, bins_y=0, width_x=0, width_y=0, log=False, palette='viridis', plot=True)",
      "2D density (2D histogram): count X-Y points on a grid, keep the count matrix and show it as a heat map with a colour bar (fast for 100,000+ points).",
      "table:str=''; x:int|str=0; y:int|str=1; bins_x:int=0; bins_y:int=0; width_x:float=0; width_y:float=0; log:bool=false; palette:str='viridis'; plot:bool=true",
      "app.plot_density2d(tbl, 'x_um', 'y_um', width_x=5, width_y=5, log=True)" },
    { "graph.rebuild_density2d", "graph",
      "GraphProxy.rebuild_density2d()",
      "Re-count a 2D density graph from its recorded source columns and bin settings.",
      "graph:str=''; layer:int=null",
      "g.rebuild_density2d()" },
    { "plot.pole_figure", "plot",
      "Session.plot_pole_figure(table=None, alpha=0, beta=1, intensity=2, *, matrix=None, projection='schmidt', mrd=True, alpha_from_rim=False, beta_zero_top=False, clockwise=False, rd='top', grid=201, levels=8, palette='viridis', label='', plot=True)",
      "XRD pole figure from alpha (tilt), beta (rotation), intensity columns or an alpha x beta matrix: equal-area (Schmidt) or equal-angle (Wulff) projection, contours, colour bar and MRD normalisation.",
      "table:str=''; alpha:int|str=0; beta:int|str=1; intensity:int|str=2; matrix:str=''; projection:str='schmidt'[schmidt|wulff|equal-area|equal-angle]; mrd:bool=true; alpha_from_rim:bool=false; beta_zero_top:bool=false; clockwise:bool=false; rd:str='top'[top|right]; grid:int=201; levels:int=8; palette:str='viridis'; label:str=''; plot:bool=true",
      "app.plot_pole_figure(tbl, 'alpha', 'beta', 'I', projection='schmidt', label='(111)')" },
    { "stats.kaplan_meier", "stats",
      "Session.kaplan_meier(table=None, time_col=0, event_col=1, *, group_col=None)",
      "Compute Kaplan-Meier survival curves and log-rank test.",
      "table:str=''; time_col:int|str=0; event_col:int|str=1; group_col:int|str=null",
      "app.kaplan_meier(tbl, 'Cycles', 'Failed')" },
    { "stats.roc", "stats",
      "Session.roc(table=None, state_col=0, score_cols=None, *, plot=True)",
      "Compute ROC curve, AUC, Hanley-McNeil SE, and Youden optimal threshold.",
      "table:str=''; state_col:int|str=0; score_cols:list=null; score_col:int|str=null; plot:bool=true",
      "app.roc(tbl, 'Class', ['Score'])" },
    { "stats.pca", "stats", "Session.pca(table=None, cols=None, *, standardize=True, biplot=True)",
      "Run Principal Component Analysis returning eigenvalues, explained variance, loadings, and scores.",
      "table:str=''; cols:list=null; standardize:bool=true; biplot:bool=true",
      "app.pca(tbl, ['F1', 'F2', 'F3', 'F4'])" },
    { "stats.correlation", "stats",
      "Session.correlation(table=None, cols=None, *, method='pearson', plot_heatmap=True)",
      "Compute Pearson or Spearman correlation & p-value matrix.",
      "table:str=''; cols:list=null; method:str='pearson'[pearson|spearman]; plot_heatmap:bool=true",
      "app.correlation(tbl, method='pearson')" },
    { "stats.clustering", "stats",
      "Session.clustering(table=None, cols=None, *, k=3, method='kmeans', linkage='ward')",
      "Perform K-Means or hierarchical clustering across numeric columns.",
      "table:str=''; cols:list=null; k:int=3; method:str='kmeans'[kmeans|hierarchical]; linkage:str='ward'[ward|complete|average]; plot:bool=true",
      "app.clustering(tbl, ['X', 'Y'], k=3)" },
    { "stats.confidence_ellipse", "stats",
      "Session.confidence_ellipse(graph=None, *, curve=None, level=0.95, prediction=False)",
      "Add a bivariate covariance confidence/prediction ellipse onto a 2D scatter plot.",
      "graph:str=''; curve:int|str=null; level:float=0.95; prediction:bool=false",
      "app.confidence_ellipse(g, level=0.95)" },

    // 11. Plot Digitizer (digitizer.*)
    { "digitizer.extract", "digitizer",
      "Session.digitize(image_path, *, cal_points=None, color='#DC1E1E', tolerance=50, mode='auto_curve')",
      "Calibrate axes on a bitmap figure image (4 axis points, 3 L-axis points [Origin, X2, Y2], or 2 corners) and extract calibrated (X, Y) curve data.",
      "image_path:str*; cal_points:list=null; color:str='#DC1E1E'; tolerance:int=50; samples:int=100; mode:str='auto_curve'[auto_curve]; plot:bool=false; table_name:str=''",
      "app.digitize('paper_fig.png', cal_points=[[50, 350, 0.0], [450, 350, 10.0], [50, 350, 0.0], [50, 50, 100.0]])" }
};

extern const int kApiCatalogCount =
        static_cast<int>(sizeof(kApiCatalogSpecs) / sizeof(kApiCatalogSpecs[0]));

QJsonObject serializeCatalogRow(const ApiMethodCatalogRow &row)
{
    QJsonObject m;
    m[QStringLiteral("method")] = QString::fromUtf8(row.method);
    m[QStringLiteral("domain")] = QString::fromUtf8(row.domain);
    m[QStringLiteral("sdk_method")] = QString::fromUtf8(row.sdkMethod);
    m[QStringLiteral("summary")] = QString::fromUtf8(row.summary);
    m[QStringLiteral("params")] = parseCompactParamsSignature(row.paramsSig);
    m[QStringLiteral("example")] = QString::fromUtf8(row.example);
    return m;
}

void validateParamsAgainstCatalogSchema(const QString &method, const QJsonObject &params)
{
    if (params.isEmpty())
        return;

    static const QHash<QString, QHash<QString, QString>> s_schemaMap = []() {
        QHash<QString, QHash<QString, QString>> out;
        out.reserve(kApiCatalogCount);
        for (int i = 0; i < kApiCatalogCount; ++i) {
            const QString mName = QString::fromUtf8(kApiCatalogSpecs[i].method);
            const QJsonArray pArr = parseCompactParamsSignature(kApiCatalogSpecs[i].paramsSig);
            QHash<QString, QString> pMap;
            for (const QJsonValue &pv : pArr) {
                const QJsonObject po = pv.toObject();
                const QString pn = po.value(QStringLiteral("name")).toString();
                const QString pt = po.value(QStringLiteral("type")).toString().trimmed().toLower();
                if (!pn.isEmpty() && !pt.isEmpty())
                    pMap.insert(pn, pt);
            }
            out.insert(mName, pMap);
        }
        return out;
    }();

    const auto mIt = s_schemaMap.constFind(method);
    if (mIt == s_schemaMap.constEnd())
        return;
    const QHash<QString, QString> &methodParams = mIt.value();

    auto isBoolLiteralWord = [](const QString &raw) -> bool {
        const QString s = raw.trimmed().toLower();
        return s == QStringLiteral("true") || s == QStringLiteral("1")
                || s == QStringLiteral("yes") || s == QStringLiteral("on")
                || s == QStringLiteral("false") || s == QStringLiteral("0")
                || s == QStringLiteral("no") || s == QStringLiteral("off");
    };

    auto isColorParamName = [](const QString &pn) -> bool {
        return pn == QStringLiteral("color") || pn.endsWith(QStringLiteral("_color"));
    };

    auto isSingleStringListShorthand = [](const QString &pn) -> bool {
        return pn == QStringLiteral("cols") || pn == QStringLiteral("y_cols")
                || pn == QStringLiteral("x_cols") || pn == QStringLiteral("r_cols")
                || pn == QStringLiteral("dos_cols") || pn == QStringLiteral("down_dos_cols")
                || pn == QStringLiteral("band_cols") || pn == QStringLiteral("score_cols")
                || pn == QStringLiteral("curves") || pn == QStringLiteral("models")
                || pn == QStringLiteral("tables") || pn == QStringLiteral("graphs")
                || pn == QStringLiteral("shared_params");
    };

    auto isSingleIntIndexListShorthand = [](const QString &pn) -> bool {
        return pn == QStringLiteral("cols") || pn == QStringLiteral("y_cols")
                || pn == QStringLiteral("x_cols") || pn == QStringLiteral("r_cols")
                || pn == QStringLiteral("dos_cols") || pn == QStringLiteral("down_dos_cols")
                || pn == QStringLiteral("band_cols") || pn == QStringLiteral("score_cols")
                || pn == QStringLiteral("curves");
    };

    auto isJsonExactInt = [](const QJsonValue &v) -> bool {
        if (!v.isDouble() || v.isBool())
            return false;
        const double d = v.toDouble();
        return std::isfinite(d) && std::floor(d) == d
                && d >= double(std::numeric_limits<int>::min())
                && d <= double(std::numeric_limits<int>::max());
    };

    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        const QJsonValue &val = it.value();
        if (val.isNull() || val.isUndefined())
            continue;
        const QString &pName = it.key();
        const auto pIt = methodParams.constFind(pName);
        if (pIt == methodParams.constEnd())
            continue;
        const QString &typeSpec = pIt.value();
        if (typeSpec.isEmpty() || typeSpec.contains(QStringLiteral("any")))
            continue;

        const QStringList tokens = typeSpec.split(QLatin1Char('|'), Qt::SkipEmptyParts);
        const bool unionHasInt = typeSpec.contains(QStringLiteral("int"));
        const bool unionHasStr = typeSpec.contains(QStringLiteral("str"));
        bool matched = false;
        for (const QString &rawTok : tokens) {
            const QString tok = rawTok.trimmed();
            if (tok == QStringLiteral("null")) {
                if (val.isNull()) {
                    matched = true;
                    break;
                }
            } else if (tok == QStringLiteral("str") || tok == QStringLiteral("string")) {
                if (val.isString()) {
                    matched = true;
                    break;
                }
                if (isColorParamName(pName) && val.isArray()) {
                    const QJsonArray ca = val.toArray();
                    if (ca.size() >= 3 && ca.size() <= 4) {
                        matched = true;
                        break;
                    }
                }
            } else if (tok == QStringLiteral("bool") || tok == QStringLiteral("boolean")) {
                if (val.isBool()) {
                    matched = true;
                    break;
                }
                if (!unionHasInt && val.isDouble() && !val.isBool()) {
                    const double d = val.toDouble();
                    if (d == 0.0 || d == 1.0) {
                        matched = true;
                        break;
                    }
                }
                if (!unionHasStr && val.isString() && isBoolLiteralWord(val.toString())) {
                    matched = true;
                    break;
                }
            } else if (tok == QStringLiteral("int") || tok == QStringLiteral("integer")) {
                if (val.isDouble() && !val.isBool()) {
                    const double d = val.toDouble();
                    if (std::isfinite(d) && std::floor(d) == d
                        && d >= double(std::numeric_limits<int>::min())
                        && d <= double(std::numeric_limits<int>::max())) {
                        matched = true;
                        break;
                    }
                }
            } else if (tok == QStringLiteral("float") || tok == QStringLiteral("number")
                       || tok == QStringLiteral("double")) {
                if (val.isDouble() && !val.isBool() && std::isfinite(val.toDouble())) {
                    matched = true;
                    break;
                }
            } else if (tok.startsWith(QStringLiteral("list"))
                       || tok.startsWith(QStringLiteral("array"))) {
                if (val.isArray()
                    || (isSingleStringListShorthand(pName) && val.isString())
                    || (isSingleIntIndexListShorthand(pName) && isJsonExactInt(val))) {
                    matched = true;
                    break;
                }
            } else if (tok.startsWith(QStringLiteral("dict"))
                       || tok.startsWith(QStringLiteral("object"))) {
                if (val.isObject()) {
                    matched = true;
                    break;
                }
            }
        }

        if (!matched) {
            throwInvalidParams(
                    QStringLiteral("Invalid type for parameter '%1' in '%2' (expected %3).")
                            .arg(pName, method, typeSpec));
        }
    }
}

QString generatePythonScriptInternal(ApplicationWindow *app, MyWidget *window, bool standalone,
                                     const QString &embedDataRaw, const QString &csvSidecarDir,
                                     QStringList *outTables, QStringList *outMatrices)
{
    if (outTables)
        outTables->clear();
    if (outMatrices)
        outMatrices->clear();
    if (!app || !window)
        return QString();

    QString embedMode = embedDataRaw.trimmed().toLower();
    if (embedMode.isEmpty())
        embedMode = QStringLiteral("inline");

    QVector<Table *> refTables;
    QVector<Matrix *> refMatrices;
    auto addTableRef = [&](Table *t) {
        if (t && !refTables.contains(t)) {
            refTables.append(t);
            if (outTables)
                outTables->append(t->name());
        }
    };
    auto addMatrixRef = [&](Matrix *m) {
        if (m && !refMatrices.contains(m)) {
            refMatrices.append(m);
            if (outMatrices)
                outMatrices->append(m->name());
        }
    };

    auto parseXmlTagAttrs = [](const QString &tag) -> QMap<QString, QString> {
        QMap<QString, QString> out;
        static const QRegularExpression rxAttr(QStringLiteral("([A-Za-z0-9_]+)=\"([^\"]*)\""));
        auto it = rxAttr.globalMatch(tag);
        while (it.hasNext()) {
            const auto m = it.next();
            QString val = m.captured(2);
            val.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
            val.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
            val.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
            val.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
            val.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
            out.insert(m.captured(1), val);
        }
        return out;
    };

    if (window->inherits("Table")) {
        addTableRef(static_cast<Table *>(window));
    } else if (window->inherits("Matrix")) {
        addMatrixRef(static_cast<Matrix *>(window));
    } else if (window->inherits("Graph3D")) {
        auto *g3d = static_cast<Graph3D *>(window);
        if (g3d->matrix())
            addMatrixRef(g3d->matrix());
        if (g3d->getTable())
            addTableRef(g3d->getTable());
    } else if (window->inherits("MultiLayer")) {
        auto *ml = static_cast<MultiLayer *>(window);
        for (Graph *lg : ml->layersList()) {
            if (!lg)
                continue;
            bool poleHasSource = false;
            if (lg->isPoleFigurePlot()) {
                const QString pfTag =
                        ExtraPlotTypes::graphTag(lg, QStringLiteral("ToriPoleFigure"));
                if (!pfTag.isEmpty()) {
                    const QMap<QString, QString> pfa = parseXmlTagAttrs(pfTag);
                    if (Table *pt = app->table(pfa.value(QStringLiteral("table")))) {
                        if (pt->colIndex(pfa.value(QStringLiteral("a"))) >= 0
                            && pt->colIndex(pfa.value(QStringLiteral("b"))) >= 0
                            && pt->colIndex(pfa.value(QStringLiteral("i"))) >= 0) {
                            addTableRef(pt);
                            poleHasSource = true;
                        }
                    } else if (Matrix *pm =
                                       app->matrix(pfa.value(QStringLiteral("sourceMatrix")))) {
                        addMatrixRef(pm);
                        poleHasSource = true;
                    }
                }
            }
            for (int ci = 0; ci < lg->curves(); ++ci) {
                if (auto *dc = dynamic_cast<DataCurve *>(lg->curve(ci))) {
                    addTableRef(dc->table());
                    for (DataCurve *eb : dc->errorBarsList()) {
                        if (eb)
                            addTableRef(eb->table());
                    }
                }
            }
            if (!poleHasSource && lg->plotWidget()) {
                for (QwtPlotItem *it : lg->plotWidget()->itemList()) {
                    if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                        if (auto *sp = dynamic_cast<Spectrogram *>(it))
                            addMatrixRef(sp->matrix());
                    }
                }
            }
        }
    }

    const bool useJa = app && app->appLanguage.startsWith(QLatin1String("ja"), Qt::CaseInsensitive);
    auto pyTr = [useJa](const char *enText) -> QString {
        if (!useJa)
            return QString::fromUtf8(enText);
        return QObject::tr(enText);
    };

    auto sanitizePyCommentText = [](const QString &raw) -> QString {
        const QString s = sanitizeUtf16Surrogates(raw);
        QString out;
        out.reserve(s.size());
        for (int i = 0; i < s.size(); ++i) {
            const QChar ch = s.at(i);
            const ushort u = ch.unicode();
            if (u == '\r' || u == '\n' || u == '\v' || u == '\f' || u == 0x0085
                || u == 0x2028 || u == 0x2029) {
                if (u == '\n' && i > 0 && s.at(i - 1) == QLatin1Char('\r'))
                    continue;
                out.append(QLatin1Char(' '));
            } else if (u < 0x20 || u == 0x7F || (u >= 0x80 && u <= 0x9F)) {
                if (u == '\t')
                    out.append(QLatin1Char(' '));
            } else {
                out.append(ch);
            }
        }
        return out;
    };

    QStringList lines;
    lines << QStringLiteral("# ") + sanitizePyCommentText(pyTr("Auto-generated reproducible ToriGraph script"));
    lines << QStringLiteral("# ")
                    + sanitizePyCommentText(pyTr("Reproduces the visual appearance (data, curves, axes, annotations). Analysis and fit computations are not re-run."));
    lines << QStringLiteral("import torigraph as tg");
    lines << QString();
    // Elements this generator cannot (yet) rebuild are listed as "# Not reproduced: ..." / "# 未再現: ..." comments
    // right after the import line, so the reader sees them before running the script.
    QStringList unreproduced;
    auto noteUnreproduced = [&unreproduced, &pyTr, &sanitizePyCommentText](const QString &what) {
        const QString body = sanitizePyCommentText(pyTr("Not reproduced: %1").arg(sanitizePyCommentText(what)));
        const QString ln = QStringLiteral("# ") + body;
        if (!unreproduced.contains(ln))
            unreproduced << ln;
    };
    auto finishScript = [&lines, &unreproduced, &pyTr, &sanitizePyCommentText]() -> QString {
        QStringList out = lines;
        if (!unreproduced.isEmpty()) {
            QStringList block;
            block << QStringLiteral("# ")
                            + sanitizePyCommentText(pyTr("Warning: the following elements are not reproduced by this script (adjust their appearance manually)."));
            block << unreproduced;
            block << QString();
            for (int i = 0; i < block.size(); ++i)
                out.insert(4 + i, block.at(i));
        }
        return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
    };
    if (standalone) {
        lines << QStringLiteral("app = tg.connect_or_launch()");
        lines << QString();
    }

    if (embedMode == QStringLiteral("csv") && !refTables.isEmpty()) {
        lines << QStringLiteral("import os");
        lines << QStringLiteral(
                "_script_dir = os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else '.'");
        lines << QString();
    }

    QHash<Table *, QString> tblVarMap;
    for (int ti = 0; ti < refTables.size(); ++ti) {
        Table *t = refTables[ti];
        const QString varName =
                (refTables.size() == 1) ? QStringLiteral("tbl")
                                        : QStringLiteral("tbl_%1").arg(ti + 1);
        tblVarMap.insert(t, varName);

        if (embedMode == QStringLiteral("reference")) {
            lines << QStringLiteral("%1 = app.table(%2)").arg(varName, pyEscapeStr(t->name()));
            continue;
        }
        if (embedMode == QStringLiteral("csv")) {
            const QString csvName = QStringLiteral("%1.csv").arg(t->name());
            if (!csvSidecarDir.isEmpty()) {
                const QString fullCsv = QDir(csvSidecarDir).filePath(csvName);
                t->exportASCII(fullCsv, QStringLiteral(","), true, false);
            } else {
                noteUnreproduced(pyTr(
                        "embed_data='csv' requires an output directory; CSV file (%1) was not written")
                                         .arg(csvName));
            }
            lines << QStringLiteral("%1 = app.import_csv(os.path.join(_script_dir, %2), name=%3)")
                             .arg(varName, pyEscapeStr(csvName), pyEscapeStr(t->name()));
            continue;
        }

        // Inline data emission
        lines << QStringLiteral("%1 = app.create_table(rows=%2, cols=%3, name=%4)")
                         .arg(varName)
                         .arg(t->numRows())
                         .arg(t->numCols())
                         .arg(pyEscapeStr(t->name()));
        QStringList dataEntries, unitEntries, longEntries, desigEntries;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            const QString cLbl = t->colLabel(c);
            QStringList vals;
            const int nR = t->numRows();
            for (int r = 0; r < nR; ++r) {
                if (!col || col->isInvalid(r)) {
                    vals << QStringLiteral("None");
                } else if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    vals << pyFormatNum(col->valueAt(r));
                } else {
                    vals << pyEscapeStr(col->textAt(r));
                }
            }
            dataEntries << QStringLiteral("        %1: [%2]").arg(pyEscapeStr(cLbl),
                                                                  vals.join(QStringLiteral(", ")));
            if (!t->colUnits(c).trimmed().isEmpty()) {
                unitEntries << QStringLiteral("%1: %2").arg(pyEscapeStr(cLbl),
                                                            pyEscapeStr(t->colUnits(c).trimmed()));
            }
            if (!t->colLongName(c).trimmed().isEmpty()) {
                longEntries << QStringLiteral("%1: %2").arg(
                        pyEscapeStr(cLbl), pyEscapeStr(t->colLongName(c).trimmed()));
            }
            const int pd = t->colPlotDesignation(c);
            if (pd == SciDAVis::X)
                desigEntries << QStringLiteral("%1: 'X'").arg(pyEscapeStr(cLbl));
            else if (pd == SciDAVis::Y)
                desigEntries << QStringLiteral("%1: 'Y'").arg(pyEscapeStr(cLbl));
            else if (pd == SciDAVis::Z)
                desigEntries << QStringLiteral("%1: 'Z'").arg(pyEscapeStr(cLbl));
            else if (pd == SciDAVis::xErr)
                desigEntries << QStringLiteral("%1: 'xErr'").arg(pyEscapeStr(cLbl));
            else if (pd == SciDAVis::yErr)
                desigEntries << QStringLiteral("%1: 'yErr'").arg(pyEscapeStr(cLbl));
        }
        lines << QStringLiteral("%1.set_data_dict(").arg(varName);
        lines << QStringLiteral("    {\n%1\n    },").arg(dataEntries.join(QStringLiteral(",\n")));
        if (!unitEntries.isEmpty())
            lines << QStringLiteral("    units={%1},").arg(unitEntries.join(QStringLiteral(", ")));
        if (!longEntries.isEmpty()) {
            lines << QStringLiteral("    long_names={%1},")
                             .arg(longEntries.join(QStringLiteral(", ")));
        }
        if (!desigEntries.isEmpty()) {
            lines << QStringLiteral("    designations={%1},")
                             .arg(desigEntries.join(QStringLiteral(", ")));
        }
        lines << QStringLiteral(")");

        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!col)
                continue;
            const QString fStr = col->formula(0).trimmed();
            if (!fStr.isEmpty()) {
                lines << QStringLiteral("%1.set_formula(%2, %3)")
                                 .arg(varName, pyEscapeStr(t->colLabel(c)), pyEscapeStr(fStr));
            }
            QStringList maskedIdx;
            for (int r = 0; r < t->numRows(); ++r) {
                if (col->isMasked(r))
                    maskedIdx << QString::number(r);
            }
            if (!maskedIdx.isEmpty()) {
                lines << QStringLiteral("%1.mask_rows([%2], col=%3)")
                                 .arg(varName, maskedIdx.join(QStringLiteral(", ")),
                                      pyEscapeStr(t->colLabel(c)));
            }
        }
        lines << QString();
    }

    QHash<Matrix *, QString> matVarMap;
    for (int mi = 0; mi < refMatrices.size(); ++mi) {
        Matrix *m = refMatrices[mi];
        const QString varName =
                (refMatrices.size() == 1) ? QStringLiteral("mat")
                                          : QStringLiteral("mat_%1").arg(mi + 1);
        matVarMap.insert(m, varName);
        if (embedMode == QStringLiteral("reference")) {
            lines << QStringLiteral("%1 = app.matrix(%2)").arg(varName, pyEscapeStr(m->name()));
            continue;
        }
        lines << QStringLiteral(
                         "%1 = app.create_matrix(rows=%2, cols=%3, name=%4, x_min=%5, x_max=%6, y_min=%7, y_max=%8)")
                         .arg(varName)
                         .arg(m->numRows())
                         .arg(m->numCols())
                         .arg(pyEscapeStr(m->name()))
                         .arg(pyFormatNum(m->xStart()))
                         .arg(pyFormatNum(m->xEnd()))
                         .arg(pyFormatNum(m->yStart()))
                         .arg(pyFormatNum(m->yEnd()));
        if (!m->formula().trimmed().isEmpty()) {
            lines << QStringLiteral("%1.set_formula(%2)")
                             .arg(varName, pyEscapeStr(m->formula().trimmed()));
        } else {
            QStringList rowLiterals;
            for (int r = 0; r < m->numRows(); ++r) {
                QStringList cellLiterals;
                for (int c = 0; c < m->numCols(); ++c)
                    cellLiterals << pyFormatNum(m->cell(r, c));
                rowLiterals << QStringLiteral("    [%1]").arg(
                        cellLiterals.join(QStringLiteral(", ")));
            }
            lines << QStringLiteral("%1.set_data([\n%2\n])")
                             .arg(varName, rowLiterals.join(QStringLiteral(",\n")));
        }
        lines << QString();
    }

    if (window->inherits("Graph3D")) {
        auto *g3d = static_cast<Graph3D *>(window);
        bool emittedG3 = false;
        if (g3d->matrix() && matVarMap.contains(g3d->matrix())) {
            lines << QStringLiteral("g3 = app.plot_3d_matrix(%1.name)")
                             .arg(matVarMap.value(g3d->matrix()));
            emittedG3 = true;
        } else if (g3d->getTable() && tblVarMap.contains(g3d->getTable())) {
            Table *t3 = g3d->getTable();
            const QString c0 = (t3->numCols() > 0) ? t3->colLabel(0) : QStringLiteral("1");
            const QString c1 = (t3->numCols() > 1) ? t3->colLabel(1) : c0;
            const QString c2 = (t3->numCols() > 2) ? t3->colLabel(2) : c1;
            lines << QStringLiteral("g3 = app.plot_3d_xyz(%1.name, %2, %3, %4)")
                             .arg(tblVarMap.value(t3), pyEscapeStr(c0), pyEscapeStr(c1),
                                  pyEscapeStr(c2));
            emittedG3 = true;
        } else if (!g3d->formula().trimmed().isEmpty()) {
            lines << QStringLiteral(
                             "g3 = app.plot_3d_function(%1, x_min=%2, x_max=%3, y_min=%4, y_max=%5)")
                             .arg(pyEscapeStr(g3d->formula().trimmed()),
                                  pyFormatNum(g3d->xStart()), pyFormatNum(g3d->xStop()),
                                  pyFormatNum(g3d->yStart()), pyFormatNum(g3d->yStop()));
            emittedG3 = true;
        }
        if (!emittedG3) {
            noteUnreproduced(pyTr(
                    "3D graph data source (could not identify Matrix, Table, or formula; 3D graph cannot be reproduced)"));
            return finishScript();
        }
        lines << QStringLiteral(
                         "g3.set_3d_options(title=%1, x_rotation=%2, y_rotation=%3, z_rotation=%4, orthogonal=%5)")
                         .arg(pyEscapeStr(g3d->plotTitle()), pyFormatNum(g3d->xRotation()),
                              pyFormatNum(g3d->yRotation()), pyFormatNum(g3d->zRotation()),
                              g3d->isOrthogonal() ? QStringLiteral("True")
                                                  : QStringLiteral("False"));
        noteUnreproduced(pyTr(
                "3D graph colors (colormap), axis ranges, zoom, and drawing style (only title, rotation, and projection are reproduced)"));
        return finishScript();
    }

    if (!window->inherits("MultiLayer"))
        return finishScript();

    auto *ml = static_cast<MultiLayer *>(window);
    const QList<Graph *> layers = ml->layersList();
    if (layers.isEmpty())
        return finishScript();

    auto curveStyleStr = [](Graph *lg, int ci) -> QString {
        const int ct = lg->curveType(ci);
        if (ct == Graph::Line)
            return QStringLiteral("line");
        if (ct == Graph::Scatter)
            return QStringLiteral("scatter");
        if (ct == Graph::Spline)
            return QStringLiteral("spline");
        if (ct == Graph::VerticalBars)
            return QStringLiteral("vertical_bars");
        if (ct == Graph::HorizontalBars)
            return QStringLiteral("horizontal_bars");
        if (ct == Graph::Area)
            return QStringLiteral("area");
        if (ct == Graph::VerticalSteps)
            return QStringLiteral("vertical_steps");
        if (ct == Graph::HorizontalSteps)
            return QStringLiteral("horizontal_steps");
        if (ct == Graph::VerticalDropLines)
            return QStringLiteral("drop_lines");
        return QStringLiteral("line_symbol");
    };

    auto penStyleStr = [](Qt::PenStyle ps) -> QString {
        if (ps == Qt::NoPen)
            return QStringLiteral("none");
        if (ps == Qt::DashLine)
            return QStringLiteral("--");
        if (ps == Qt::DotLine)
            return QStringLiteral(":");
        if (ps == Qt::DashDotLine || ps == Qt::DashDotDotLine)
            return QStringLiteral("-.");
        return QStringLiteral("-");
    };

    auto symStyleStr = [](QwtSymbol::Style st) -> QString {
        switch (st) {
        case QwtSymbol::Ellipse:
            return QStringLiteral("o");
        case QwtSymbol::Rect:
            return QStringLiteral("s");
        case QwtSymbol::UTriangle:
        case QwtSymbol::Triangle:
            return QStringLiteral("^");
        case QwtSymbol::DTriangle:
            return QStringLiteral("v");
        case QwtSymbol::LTriangle:
            return QStringLiteral("<");
        case QwtSymbol::RTriangle:
            return QStringLiteral(">");
        case QwtSymbol::Diamond:
            return QStringLiteral("D");
        case QwtSymbol::Cross:
            return QStringLiteral("+");
        case QwtSymbol::XCross:
            return QStringLiteral("x");
        case QwtSymbol::Star1:
        case QwtSymbol::Star2:
            return QStringLiteral("*");
        case QwtSymbol::Hexagon:
            return QStringLiteral("h");
        default:
            return QStringLiteral("none");
        }
    };

    auto colorMapPresetPyName = [](int presetInt) -> QString {
        switch (static_cast<Spectrogram::ColorMapPreset>(presetInt)) {
        case Spectrogram::PresetGrayScale:
            return QStringLiteral("grayscale");
        case Spectrogram::PresetViridis:
            return QStringLiteral("viridis");
        case Spectrogram::PresetMagma:
            return QStringLiteral("magma");
        case Spectrogram::PresetInferno:
            return QStringLiteral("inferno");
        case Spectrogram::PresetPlasma:
            return QStringLiteral("plasma");
        case Spectrogram::PresetCividis:
            return QStringLiteral("cividis");
        case Spectrogram::PresetJet:
            return QStringLiteral("jet");
        case Spectrogram::PresetBlueWhiteRed:
            return QStringLiteral("bwr");
        case Spectrogram::PresetCoolWarm:
            return QStringLiteral("coolwarm");
        default:
            return QString();
        }
    };

    auto spectrogramStyleStr = [](Spectrogram *sp) -> QString {
        if (!sp)
            return QStringLiteral("colormap");
        if (!sp->testDisplayMode(QwtPlotSpectrogram::ImageMode)
            && sp->testDisplayMode(QwtPlotSpectrogram::ContourMode))
            return QStringLiteral("contour");
        if (sp->colorMapPreset() == Spectrogram::PresetGrayScale)
            return QStringLiteral("grayscale");
        return QStringLiteral("colormap");
    };

    auto emitSpectrogramColorbar = [&](Spectrogram *sp, const QString &layerKw) {
        if (!sp)
            return;
        QStringList cbArgs;
        const QString palName = colorMapPresetPyName(int(sp->colorMapPreset()));
        if (!palName.isEmpty())
            cbArgs << QStringLiteral("palette=%1").arg(pyEscapeStr(palName));
        else if (sp->colorMapPreset() == Spectrogram::PresetCustom)
            noteUnreproduced(pyTr("custom colormap palette (defaults to the standard palette)"));
        if (sp->reversePalette())
            cbArgs << QStringLiteral("reverse=True");
        const QwtDoubleInterval zInt = sp->colorAxisRange();
        if (std::isfinite(zInt.minValue()) && std::isfinite(zInt.maxValue())
            && zInt.maxValue() > zInt.minValue()) {
            cbArgs << QStringLiteral("z_min=%1").arg(pyFormatNum(zInt.minValue()));
            cbArgs << QStringLiteral("z_max=%1").arg(pyFormatNum(zInt.maxValue()));
        }
        cbArgs << QStringLiteral("title=%1").arg(pyEscapeStr(sp->colorBarTitle()));
        cbArgs << QStringLiteral("levels=%1").arg(sp->levels());
        const bool showContours = sp->testDisplayMode(QwtPlotSpectrogram::ContourMode);
        cbArgs << QStringLiteral("show_contour_lines=%1")
                          .arg(showContours ? QStringLiteral("True") : QStringLiteral("False"));
        if (showContours && sp->defaultContourPen().style() == Qt::NoPen)
            cbArgs << QStringLiteral("use_colormap_pen=True");
        if (sp->logColorScale())
            cbArgs << QStringLiteral("log=True");
        if (sp->cellBlocks())
            cbArgs << QStringLiteral("cell_blocks=True");
        if (!sp->hasColorScale())
            cbArgs << QStringLiteral("visible=False");
        lines << QStringLiteral("g.set_colorbar(%1%2)")
                         .arg(cbArgs.join(QStringLiteral(", ")), layerKw);
    };

    auto boxWhiskerExtra = [](BoxCurve *bc) -> QString {
        if (!bc)
            return QString();
        const int wType = bc->whiskersRangeType();
        if (wType == BoxCurve::Tukey15IQR)
            return QStringLiteral(", whisker_mode='1.5iqr'");
        if (wType == BoxCurve::MinMax)
            return QStringLiteral(", whisker_mode='minmax'");
        if (wType == BoxCurve::SD)
            return QStringLiteral(", whisker_mode='sd'");
        if (wType == BoxCurve::SE)
            return QStringLiteral(", whisker_mode='se'");
        if (wType == BoxCurve::r5_95)
            return QStringLiteral(", whisker_mode='p5_95'");
        if (wType == BoxCurve::r10_90)
            return QStringLiteral(", whisker_mode='p10_90'");
        if (wType == BoxCurve::r1_99)
            return QStringLiteral(", whisker_mode='p1_99'");
        return QString();
    };

    auto firstSpectrogramOn = [](Graph *lg) -> Spectrogram * {
        if (!lg || !lg->plotWidget())
            return nullptr;
        for (QwtPlotItem *it : lg->plotWidget()->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                if (auto *sp = dynamic_cast<Spectrogram *>(it))
                    return sp;
            }
        }
        return nullptr;
    };

    auto emitPoleFigureForLayer = [&](Graph *lg, int li, bool isBaseLayer,
                                      const QString &layerKw) -> bool {
        if (!lg || !lg->isPoleFigurePlot())
            return false;
        const QString pfTag = ExtraPlotTypes::graphTag(lg, QStringLiteral("ToriPoleFigure"));
        if (pfTag.isEmpty())
            return false;
        const QMap<QString, QString> pfa = parseXmlTagAttrs(pfTag);
        Spectrogram *sp = firstSpectrogramOn(lg);

        QStringList args;
        bool hasSrc = false;
        if (Table *pt = app->table(pfa.value(QStringLiteral("table")))) {
            const int aIdx = pt->colIndex(pfa.value(QStringLiteral("a")));
            const int bIdx = pt->colIndex(pfa.value(QStringLiteral("b")));
            const int iIdx = pt->colIndex(pfa.value(QStringLiteral("i")));
            if (tblVarMap.contains(pt) && aIdx >= 0 && bIdx >= 0 && iIdx >= 0) {
                args << QStringLiteral("table=%1.name").arg(tblVarMap.value(pt));
                args << QStringLiteral("alpha=%1").arg(pyEscapeStr(pt->colLabel(aIdx)));
                args << QStringLiteral("beta=%1").arg(pyEscapeStr(pt->colLabel(bIdx)));
                args << QStringLiteral("intensity=%1").arg(pyEscapeStr(pt->colLabel(iIdx)));
                hasSrc = true;
            }
        }
        if (!hasSrc) {
            if (Matrix *pm = app->matrix(pfa.value(QStringLiteral("sourceMatrix")))) {
                if (matVarMap.contains(pm)) {
                    args << QStringLiteral("matrix=%1.name").arg(matVarMap.value(pm));
                    hasSrc = true;
                }
            }
        }
        if (!hasSrc) {
            noteUnreproduced(pyTr(
                    "pole figure stereographic frame (source data not found; reproduced as a matrix heatmap)"));
            return false;
        }
        const QString projStr =
                (pfa.value(QStringLiteral("proj")).toInt() == int(ExtraPlotTypes::EqualAngle))
                ? QStringLiteral("wulff")
                : QStringLiteral("schmidt");
        args << QStringLiteral("projection=%1").arg(pyEscapeStr(projStr));
        args << QStringLiteral("mrd=%1")
                        .arg(pfa.value(QStringLiteral("mrd"), QStringLiteral("1")).toInt() != 0
                                     ? QStringLiteral("True")
                                     : QStringLiteral("False"));
        args << QStringLiteral("alpha_from_rim=%1")
                        .arg(pfa.value(QStringLiteral("rim"), QStringLiteral("0")).toInt() != 0
                                     ? QStringLiteral("True")
                                     : QStringLiteral("False"));
        args << QStringLiteral("beta_zero_top=%1")
                        .arg(pfa.value(QStringLiteral("zeroTop"), QStringLiteral("0")).toInt() != 0
                                     ? QStringLiteral("True")
                                     : QStringLiteral("False"));
        args << QStringLiteral("clockwise=%1")
                        .arg(pfa.value(QStringLiteral("cw"), QStringLiteral("0")).toInt() != 0
                                     ? QStringLiteral("True")
                                     : QStringLiteral("False"));
        args << QStringLiteral("rd=%1")
                        .arg(pyEscapeStr(
                                pfa.value(QStringLiteral("rdTop"), QStringLiteral("1")).toInt() != 0
                                        ? QStringLiteral("top")
                                        : QStringLiteral("right")));
        if (sp) {
            args << QStringLiteral("levels=%1").arg(sp->levels());
            const QString palName = colorMapPresetPyName(int(sp->colorMapPreset()));
            if (!palName.isEmpty())
                args << QStringLiteral("palette=%1").arg(pyEscapeStr(palName));
        }
        const QString lbl = pfa.value(QStringLiteral("label")).trimmed();
        if (!lbl.isEmpty())
            args << QStringLiteral("label=%1").arg(pyEscapeStr(lbl));
        const QString ax1 = pfa.value(QStringLiteral("ax1")).trimmed();
        if (!ax1.isEmpty())
            args << QStringLiteral("axis1_label=%1").arg(pyEscapeStr(ax1));
        const QString ax2 = pfa.value(QStringLiteral("ax2")).trimmed();
        if (!ax2.isEmpty())
            args << QStringLiteral("axis2_label=%1").arg(pyEscapeStr(ax2));
        if (pfa.value(QStringLiteral("rings")).toInt() != 0)
            args << QStringLiteral("tilt_circles=True");

        if (isBaseLayer) {
            args << QStringLiteral("graph_name=%1").arg(pyEscapeStr(ml->name()));
            lines << QStringLiteral("_pf_res = app.plot_pole_figure(%1)")
                             .arg(args.join(QStringLiteral(", ")));
            lines << QStringLiteral("g = app.graph(_pf_res['graph'])");
        } else {
            lines << QStringLiteral("app.call('plot.pole_figure', graph=g.name, layer=%1, %2)")
                             .arg(li)
                             .arg(args.join(QStringLiteral(", ")));
        }
        if (sp)
            emitSpectrogramColorbar(sp, layerKw);
        return true;
    };

    auto emitColorScatterCurve = [&](DataCurve *dc, int li, bool isBaseLayer) -> bool {
        if (!dc || !dc->hasPointColorMap() || !dc->table() || !tblVarMap.contains(dc->table()))
            return false;
        Table *ct = dc->table();
        const DataCurve::PointColorMap &pm = dc->pointColorMap();
        const int xIdx = ct->colIndex(dc->xColumnName());
        const int yIdx = ct->colIndex(dc->yColumnName());
        const int zIdx = ct->colIndex(pm.zColumn);
        if (xIdx < 0 || yIdx < 0 || zIdx < 0)
            return false;
        const int sIdx = pm.sizeColumn.isEmpty() ? -1 : ct->colIndex(pm.sizeColumn);
        QStringList args;
        args << QStringLiteral("x=%1").arg(pyEscapeStr(ct->colLabel(xIdx)));
        args << QStringLiteral("y=%1").arg(pyEscapeStr(ct->colLabel(yIdx)));
        args << QStringLiteral("z=%1").arg(pyEscapeStr(ct->colLabel(zIdx)));
        if (sIdx >= 0)
            args << QStringLiteral("size=%1").arg(pyEscapeStr(ct->colLabel(sIdx)));
        const QString palName = colorMapPresetPyName(pm.preset);
        if (!palName.isEmpty())
            args << QStringLiteral("palette=%1").arg(pyEscapeStr(palName));
        if (pm.reverse)
            args << QStringLiteral("reverse=True");
        if (!pm.autoRange && std::isfinite(pm.zMin) && std::isfinite(pm.zMax) && pm.zMax > pm.zMin) {
            args << QStringLiteral("z_min=%1").arg(pyFormatNum(pm.zMin));
            args << QStringLiteral("z_max=%1").arg(pyFormatNum(pm.zMax));
        }
        if (!pm.colorBarTitle.isEmpty())
            args << QStringLiteral("colorbar_title=%1").arg(pyEscapeStr(pm.colorBarTitle));
        if (!pm.showColorBar)
            args << QStringLiteral("show_colorbar=False");
        if (isBaseLayer) {
            lines << QStringLiteral("g = app.plot_color_scatter(%1.name, %2, graph_name=%3)")
                             .arg(tblVarMap.value(ct), args.join(QStringLiteral(", ")),
                                  pyEscapeStr(ml->name()));
        } else {
            lines << QStringLiteral(
                             "app.call('plot.color_scatter', graph=g.name, layer=%1, table=%2.name, %3)")
                             .arg(li)
                             .arg(tblVarMap.value(ct), args.join(QStringLiteral(", ")));
        }
        return true;
    };

    // Emit Layer 0 base plot
    Graph *g0 = layers.first();
    bool emittedBasePlot = false;
    bool l0HandledAllCurves = false;
    int firstEmittedCurveOnL0 = -1;
    QSet<int> preCreatedPrimaryLayers;
    QSet<QPair<int, int>> preHandledLayerCurves;
    const QString l0LayerKw =
            (layers.size() > 1) ? QStringLiteral(", layer=0") : QString();
    if (g0 && g0->isPoleFigurePlot()) {
        if (emitPoleFigureForLayer(g0, 0, true, l0LayerKw)) {
            emittedBasePlot = true;
            l0HandledAllCurves = true;
        }
    }
    if (!emittedBasePlot && g0) {
        if (Spectrogram *sp = firstSpectrogramOn(g0)) {
            if (sp->matrix() && matVarMap.contains(sp->matrix())) {
                lines << QStringLiteral("g = app.plot_matrix(%1.name, style=%2, graph_name=%3)")
                                 .arg(matVarMap.value(sp->matrix()),
                                      pyEscapeStr(spectrogramStyleStr(sp)),
                                      pyEscapeStr(ml->name()));
                emitSpectrogramColorbar(sp, l0LayerKw);
                emittedBasePlot = true;
            }
        }
    }
    if (!emittedBasePlot && g0 && g0->isTernaryPlot()) {
        QString lblA = QStringLiteral("A");
        QString lblB = QStringLiteral("B");
        QString lblC = QStringLiteral("C");
        {
            const QString savedG0 = g0->saveToString();
            static const QRegularExpression rxAttrT(
                    QStringLiteral("([A-Za-z0-9_]+)=\"([^\"]*)\""));
            for (const QString &rawLn : savedG0.split(QLatin1Char('\n'))) {
                const QString ln = rawLn.trimmed();
                if (!ln.startsWith(QStringLiteral("<ToriTernary")))
                    continue;
                auto ait = rxAttrT.globalMatch(ln);
                while (ait.hasNext()) {
                    const auto m = ait.next();
                    if (m.captured(1) == QStringLiteral("a") && !m.captured(2).isEmpty())
                        lblA = m.captured(2);
                    else if (m.captured(1) == QStringLiteral("b") && !m.captured(2).isEmpty())
                        lblB = m.captured(2);
                    else if (m.captured(1) == QStringLiteral("c") && !m.captured(2).isEmpty())
                        lblC = m.captured(2);
                }
            }
            if (lblB == lblA)
                lblB = lblB + QStringLiteral("_B");
            if (lblC == lblA || lblC == lblB)
                lblC = lblC + QStringLiteral("_C");
        }
        QStringList aVals, bVals, cVals;
        const double invH = 2.0 / std::sqrt(3.0);
        for (int ci = 0; ci < g0->curves(); ++ci) {
            QwtPlotCurve *c = g0->curve(ci);
            if (!c || g0->curveType(ci) == Graph::ErrorBars)
                continue;
            auto *dc = dynamic_cast<DataCurve *>(c);
            const bool checkMasksT = (dc != nullptr) && dc->hasAnyMaskedPoints();
            Column *maskXColT = nullptr;
            Column *maskYColT = nullptr;
            if (checkMasksT)
                dc->resolveColumns(maskXColT, maskYColT);
            for (int k = 0; k < static_cast<int>(c->dataSize()); ++k) {
                if (checkMasksT && dc->isPointMaskedFast(k, maskXColT, maskYColT))
                    continue;
                const double xv = c->x(k);
                const double yv = c->y(k);
                if (!std::isfinite(xv) || !std::isfinite(yv))
                    continue;
                const double cn = yv * invH;
                const double bn = xv - 0.5 * cn;
                const double an = 1.0 - bn - cn;
                aVals << pyFormatNum(100.0 * an);
                bVals << pyFormatNum(100.0 * bn);
                cVals << pyFormatNum(100.0 * cn);
            }
            break;
        }
        if (!aVals.isEmpty()) {
            lines << QStringLiteral("_tbl_tern = app.create_table(rows=%1, cols=3, name='TernaryComp')")
                             .arg(aVals.size());
            lines << QStringLiteral("_tbl_tern.set_data_dict({%1: [%2], %3: [%4], %5: [%6]})")
                             .arg(pyEscapeStr(lblA), aVals.join(QStringLiteral(", ")),
                                  pyEscapeStr(lblB), bVals.join(QStringLiteral(", ")),
                                  pyEscapeStr(lblC), cVals.join(QStringLiteral(", ")));
            lines << QStringLiteral("g = app.plot_ternary(_tbl_tern.name, %1, %2, %3, graph_name=%4)")
                             .arg(pyEscapeStr(lblA), pyEscapeStr(lblB), pyEscapeStr(lblC),
                                  pyEscapeStr(ml->name()));
            emittedBasePlot = true;
            l0HandledAllCurves = true;
        }
    }
    if (!emittedBasePlot && g0 && invokeGraphIsPolarPlot(g0)) {
        // Check if Curve 0 is an analytical FunctionCurve::Polar first
        FunctionCurve *fc0 = (g0->curves() > 0) ? dynamic_cast<FunctionCurve *>(g0->curve(0))
                                                : nullptr;
        if (!fc0) {
            QStringList thVals;
            QStringList rColEntries;
            QStringList rColNames;
            int maxLen = 0;
            constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
            for (int ci = 0; ci < g0->curves(); ++ci) {
                QwtPlotCurve *c = g0->curve(ci);
                if (!c || g0->curveType(ci) == Graph::ErrorBars)
                    continue;
                QStringList curR, curTh;
                for (int k = 0; k < static_cast<int>(c->dataSize()); ++k) {
                    const double xv = c->x(k);
                    const double yv = c->y(k);
                    if (!std::isfinite(xv) || !std::isfinite(yv))
                        continue;
                    const double rv = std::hypot(xv, yv);
                    double deg = std::atan2(yv, xv) * kRadToDeg;
                    if (deg < -1e-9)
                        deg += 360.0;
                    curTh << pyFormatNum(deg);
                    curR << pyFormatNum(rv);
                }
                if (curR.isEmpty())
                    continue;
                if (thVals.isEmpty()) {
                    thVals = curTh;
                    maxLen = thVals.size();
                } else {
                    while (curR.size() < maxLen)
                        curR << QStringLiteral("None");
                    while (curR.size() > maxLen)
                        curR.removeLast();
                }
                QString rName = c->title().text().trimmed();
                if (rName.isEmpty() || rName == QStringLiteral("Theta")
                    || rColNames.contains(rName)) {
                    rName = QStringLiteral("R_%1").arg(rColNames.size() + 1);
                }
                rColNames << rName;
                rColEntries << QStringLiteral("%1: [%2]").arg(
                        pyEscapeStr(rName), curR.join(QStringLiteral(", ")));
            }
            if (!thVals.isEmpty() && !rColNames.isEmpty()) {
                QStringList qRNames;
                for (const QString &rn : rColNames)
                    qRNames << pyEscapeStr(rn);
                lines << QStringLiteral("_tbl_polar = app.create_table(rows=%1, cols=%2, name='PolarData')")
                                 .arg(maxLen)
                                 .arg(1 + rColNames.size());
                lines << QStringLiteral("_tbl_polar.set_data_dict({'Theta': [%1], %2})")
                                 .arg(thVals.join(QStringLiteral(", ")),
                                      rColEntries.join(QStringLiteral(", ")));
                lines << QStringLiteral(
                                 "g = app.plot_polar(_tbl_polar.name, theta_col='Theta', r_cols=[%1], theta_in_radians=False, graph_name=%2)")
                                 .arg(qRNames.join(QStringLiteral(", ")), pyEscapeStr(ml->name()));
                emittedBasePlot = true;
                l0HandledAllCurves = true;
            }
        }
    }
    if (!emittedBasePlot && g0 && g0->isPiePlot()) {
        for (int ci = 0; ci < g0->curves(); ++ci) {
            auto *dc = dynamic_cast<DataCurve *>(g0->curve(ci));
            Table *ct = dc ? dc->table() : nullptr;
            const int yIdx = ct ? ct->colIndex(dc->yColumnName()) : -1;
            if (ct && tblVarMap.contains(ct) && yIdx >= 0) {
                int lblIdx = -1;
                for (int c = 0; c < ct->numCols(); ++c) {
                    if (c != yIdx && ct->column(c)
                        && ct->column(c)->columnMode() == SciDAVis::ColumnMode::Text) {
                        lblIdx = c;
                        break;
                    }
                }
                const QString lblKw = (lblIdx >= 0)
                        ? QStringLiteral(", label_col=%1").arg(pyEscapeStr(ct->colLabel(lblIdx)))
                        : QString();
                lines << QStringLiteral("g = app.plot_pie(%1.name, col=%2%3, graph_name=%4)")
                                 .arg(tblVarMap.value(ct), pyEscapeStr(ct->colLabel(yIdx)), lblKw,
                                      pyEscapeStr(ml->name()));
                emittedBasePlot = true;
                l0HandledAllCurves = true;
                break;
            }
        }
    }
    if (!emittedBasePlot && g0) {
        BoxCurve *bc0 = nullptr;
        for (int ci = 0; ci < g0->curves(); ++ci) {
            if (auto *bc = dynamic_cast<BoxCurve *>(g0->curve(ci))) {
                bc0 = bc;
                break;
            }
        }
        Table *ctBox = bc0 ? bc0->table() : nullptr;
        if (bc0 && ctBox && tblVarMap.contains(ctBox)) {
            QStringList qBoxCols;
            for (int ci = 0; ci < g0->curves(); ++ci) {
                if (auto *bc = dynamic_cast<BoxCurve *>(g0->curve(ci))) {
                    if (bc->table() == ctBox) {
                        const int yIdx = ctBox->colIndex(bc->yColumnName());
                        if (yIdx >= 0)
                            qBoxCols << pyEscapeStr(ctBox->colLabel(yIdx));
                    }
                }
            }
            if (!qBoxCols.isEmpty()) {
                if (bc0->violinMode()) {
                    lines << QStringLiteral(
                                     "g = app.plot_violin(%1.name, cols=[%2], show_points=%3, graph_name=%4)")
                                     .arg(tblVarMap.value(ctBox),
                                          qBoxCols.join(QStringLiteral(", ")),
                                          bc0->showDataPoints() ? QStringLiteral("True")
                                                                : QStringLiteral("False"),
                                          pyEscapeStr(ml->name()));
                } else {
                    const QString wExtra = boxWhiskerExtra(bc0);
                    lines << QStringLiteral(
                                     "g = app.plot_box(%1.name, cols=[%2], notched=%3, show_points=%4%5, graph_name=%6)")
                                     .arg(tblVarMap.value(ctBox),
                                          qBoxCols.join(QStringLiteral(", ")),
                                          (bc0->boxStyle() == BoxCurve::Notch)
                                                  ? QStringLiteral("True")
                                                  : QStringLiteral("False"),
                                          bc0->showDataPoints() ? QStringLiteral("True")
                                                                : QStringLiteral("False"),
                                          wExtra, pyEscapeStr(ml->name()));
                }
                emittedBasePlot = true;
                l0HandledAllCurves = true;
            }
        }
    }
    if (!emittedBasePlot && g0) {
        QwtHistogram *hc0 = nullptr;
        for (int ci = 0; ci < g0->curves(); ++ci) {
            if (auto *hc = dynamic_cast<QwtHistogram *>(g0->curve(ci))) {
                hc0 = hc;
                break;
            }
        }
        Table *ctHist = hc0 ? hc0->table() : nullptr;
        if (hc0 && ctHist && tblVarMap.contains(ctHist)) {
            QVector<int> primLayerIdxs;
            for (int li = 0; li < layers.size(); ++li) {
                Graph *lg = layers[li];
                if (lg && !ml->isInsetLayer(lg) && !lg->linkedParentLayer())
                    primLayerIdxs.append(li);
            }
            int l0HistCount = 0;
            for (int ci = 0; ci < g0->curves(); ++ci) {
                if (dynamic_cast<QwtHistogram *>(g0->curve(ci)))
                    ++l0HistCount;
            }
            bool isMultiPanelStackedHist = (primLayerIdxs.size() >= 2 && l0HistCount == 1);
            QStringList stackedHistCols;
            QVector<QPair<int, int>> stackedHistPairs;
            for (int pLi : primLayerIdxs) {
                if (!isMultiPanelStackedHist)
                    break;
                Graph *pLg = layers[pLi];
                int panelHists = 0;
                int panelHistCi = -1;
                QwtHistogram *panelHc = nullptr;
                for (int ci = 0; ci < pLg->curves(); ++ci) {
                    if (auto *hc = dynamic_cast<QwtHistogram *>(pLg->curve(ci))) {
                        ++panelHists;
                        panelHistCi = ci;
                        panelHc = hc;
                    }
                }
                const int yIdx = (panelHists == 1 && panelHc && panelHc->table() == ctHist)
                        ? ctHist->colIndex(panelHc->yColumnName())
                        : -1;
                if (yIdx < 0) {
                    isMultiPanelStackedHist = false;
                    break;
                }
                stackedHistCols << pyEscapeStr(ctHist->colLabel(yIdx));
                stackedHistPairs.append(qMakePair(pLi, panelHistCi));
            }
            QString binKw;
            if (!hc0->autoBinning() && hc0->binSize() > 0.0 && std::isfinite(hc0->binSize())) {
                binKw = QStringLiteral(", bin_size=%1, bin_min=%2, bin_max=%3")
                                .arg(pyFormatNum(hc0->binSize()), pyFormatNum(hc0->begin()),
                                     pyFormatNum(hc0->end()));
            }
            if (isMultiPanelStackedHist && stackedHistCols.size() >= 2) {
                lines << QStringLiteral(
                                 "g = app.plot_histogram(%1.name, cols=[%2], stacked=True%3, graph_name=%4)")
                                 .arg(tblVarMap.value(ctHist),
                                      stackedHistCols.join(QStringLiteral(", ")), binKw,
                                      pyEscapeStr(ml->name()));
                for (int pLi : primLayerIdxs)
                    preCreatedPrimaryLayers.insert(pLi);
                for (const auto &pr : stackedHistPairs)
                    preHandledLayerCurves.insert(pr);
                emittedBasePlot = true;
            } else {
                QStringList qHistCols;
                for (int ci = 0; ci < g0->curves(); ++ci) {
                    if (auto *hc = dynamic_cast<QwtHistogram *>(g0->curve(ci))) {
                        if (hc->table() == ctHist) {
                            const int yIdx = ctHist->colIndex(hc->yColumnName());
                            if (yIdx >= 0) {
                                qHistCols << pyEscapeStr(ctHist->colLabel(yIdx));
                                preHandledLayerCurves.insert(qMakePair(0, ci));
                            }
                        }
                    }
                }
                if (!qHistCols.isEmpty()) {
                    const QString colsArg = (qHistCols.size() == 1)
                            ? qHistCols.first()
                            : QStringLiteral("[%1]").arg(qHistCols.join(QStringLiteral(", ")));
                    lines << QStringLiteral("g = app.plot_histogram(%1.name, cols=%2%3, graph_name=%4)")
                                     .arg(tblVarMap.value(ctHist), colsArg, binKw,
                                          pyEscapeStr(ml->name()));
                    emittedBasePlot = true;
                }
            }
        }
    }
    if (!emittedBasePlot && g0) {
        if (DataCurve *cmDc = g0->pointColorMapCurve()) {
            if (emitColorScatterCurve(cmDc, 0, true)) {
                emittedBasePlot = true;
                for (int ci = 0; ci < g0->curves(); ++ci) {
                    if (g0->curve(ci) == cmDc) {
                        firstEmittedCurveOnL0 = ci;
                        break;
                    }
                }
            }
        }
    }
    if (!emittedBasePlot && g0) {
        for (int ci = 0; ci < g0->curves(); ++ci) {
            if (g0->curveType(ci) == Graph::ErrorBars)
                continue;
            if (auto *dc = dynamic_cast<DataCurve *>(g0->curve(ci))) {
                Table *ct = dc->table();
                const int xIdx = ct ? ct->colIndex(dc->xColumnName()) : -1;
                const int yIdx = ct ? ct->colIndex(dc->yColumnName()) : -1;
                if (ct && tblVarMap.contains(ct) && xIdx >= 0 && yIdx >= 0) {
                    const QString xLbl = ct->colLabel(xIdx);
                    const QString yLbl = ct->colLabel(yIdx);
                    lines << QStringLiteral(
                                     "g = app.plot_xy(%1.name, x_col=%2, y_cols=[%3], style=%4, graph_name=%5)")
                                     .arg(tblVarMap.value(ct), pyEscapeStr(xLbl),
                                          pyEscapeStr(yLbl), pyEscapeStr(curveStyleStr(g0, ci)),
                                          pyEscapeStr(ml->name()));
                    emittedBasePlot = true;
                    firstEmittedCurveOnL0 = ci;
                    break;
                }
            } else if (auto *fc = dynamic_cast<FunctionCurve *>(g0->curve(ci))) {
                const QStringList fList = fc->formulas();
                if (!fList.isEmpty()) {
                    const QString tStr = (fc->functionType() == FunctionCurve::Polar)
                            ? QStringLiteral("polar")
                            : ((fc->functionType() == FunctionCurve::Parametric)
                                       ? QStringLiteral("parametric")
                                       : QStringLiteral("normal"));
                    QString fArg = (fList.size() == 1)
                            ? pyEscapeStr(fList.first())
                            : QStringLiteral("[%1, %2]").arg(pyEscapeStr(fList[0]),
                                                             pyEscapeStr(fList[1]));
                    const QString vStr = fc->variable().trimmed().isEmpty()
                            ? QStringLiteral("x")
                            : fc->variable().trimmed();
                    lines << QStringLiteral(
                                     "g = app.plot_function(%1, x_min=%2, x_max=%3, points=%4, type=%5, variable=%6, graph_name=%7)")
                                     .arg(fArg, pyFormatNum(fc->startRange()),
                                          pyFormatNum(fc->endRange()))
                                     .arg(std::max(2, static_cast<int>(fc->dataSize())))
                                     .arg(pyEscapeStr(tStr), pyEscapeStr(vStr),
                                          pyEscapeStr(ml->name()));
                    emittedBasePlot = true;
                    firstEmittedCurveOnL0 = ci;
                    break;
                }
            }
        }
    }
    if (!emittedBasePlot) {
        noteUnreproduced(pyTr(
                "base plot of layer 1 could not be reconstructed; emitting an empty graph"));
        lines << QStringLiteral("g = app.plot_function('0', x_min=0.0, x_max=1.0, graph_name=%1)")
                         .arg(pyEscapeStr(ml->name()));
        lines << QStringLiteral("g.remove_curve(0)");
    }

    bool wantStackedHistSquareFrame = false;
    if (preCreatedPrimaryLayers.size() >= 2) {
        bool allSquarePanels =
                (ml->keepAspectRatio() && std::abs(ml->targetAspectRatio() - 1.0) <= 0.05);
        if (!allSquarePanels) {
            allSquarePanels = true;
            for (int pLi : preCreatedPrimaryLayers) {
                Graph *pLg = (pLi >= 0 && pLi < layers.size()) ? layers[pLi] : nullptr;
                Plot *pPlt = pLg ? pLg->plotWidget() : nullptr;
                if (!pPlt || !pPlt->plotLayout()) {
                    allSquarePanels = false;
                    break;
                }
                pPlt->updateLayout();
                const QRect cr = pPlt->plotLayout()->canvasRect();
                if (cr.width() < 20 || cr.height() < 20
                    || std::abs(cr.width() - cr.height()) > 4) {
                    allSquarePanels = false;
                    break;
                }
            }
        }
        wantStackedHistSquareFrame = allSquarePanels;
    }

    QString pageSizeLine;
    if (ml->pageViewMode() || ml->pageUnit() != MultiLayer::UnitPx) {
        pageSizeLine = QStringLiteral("g.set_page_size(%1, %2, unit=%3, dpi=%4, page_view=%5)")
                               .arg(pyFormatNum(ml->pageWidthPhysical()),
                                    pyFormatNum(ml->pageHeightPhysical()),
                                    pyEscapeStr(pageUnitToString(ml->pageUnit())))
                               .arg(ml->pageDpi())
                               .arg(ml->pageViewMode() ? QStringLiteral("True")
                                                       : QStringLiteral("False"));
        lines << pageSizeLine;
    }
    if (wantStackedHistSquareFrame)
        lines << QStringLiteral("g.apply_square_frame()");

    // Create additional layers (primary, linked, inset)
    int nPrimaryLayers = 0;
    for (int li = 0; li < layers.size(); ++li) {
        Graph *lg = layers[li];
        if (!lg)
            continue;
        const bool isInset = ml->isInsetLayer(lg);
        const bool isLinked = (!isInset && lg->linkedParentLayer() != nullptr);
        if (!isInset && !isLinked)
            ++nPrimaryLayers;
        if (li == 0)
            continue;
        if (isLinked) {
            const int pIdx = std::max(0, layers.indexOf(lg->linkedParentLayer()));
            lines << QStringLiteral(
                             "g.add_linked_layer(parent_layer=%1, link_x=%2, link_y=%3, x_formula=%4, y_formula=%5)")
                             .arg(pIdx)
                             .arg(lg->xLinkMode() != 0 ? QStringLiteral("True")
                                                       : QStringLiteral("False"))
                             .arg(lg->yLinkMode() != 0 ? QStringLiteral("True")
                                                       : QStringLiteral("False"))
                             .arg(pyEscapeStr(lg->xLinkExpr()), pyEscapeStr(lg->yLinkExpr()));
        } else if (isInset) {
            const int srcIdx = std::max(0, layers.indexOf(ml->insetSourceLayer(lg)));
            const QRectF nr = ml->insetNormGeo(lg);
            const QRectF zr = ml->insetZoomDataRect(lg);
            if (zr.isValid() && zr.width() > 0.0 && zr.height() > 0.0) {
                lines << QStringLiteral(
                                 "g.add_inset(source_layer=%1, x_min=%2, x_max=%3, y_min=%4, y_max=%5, rel_x=%6, rel_y=%7, rel_w=%8, rel_h=%9, show_connectors=%10)")
                                 .arg(srcIdx)
                                 .arg(pyFormatNum(zr.left()), pyFormatNum(zr.right()),
                                      pyFormatNum(zr.top()), pyFormatNum(zr.bottom()),
                                      pyFormatNum(nr.x()), pyFormatNum(nr.y()),
                                      pyFormatNum(nr.width()), pyFormatNum(nr.height()),
                                      ml->insetShowConnectors(lg) ? QStringLiteral("True")
                                                                  : QStringLiteral("False"));
            } else {
                lines << QStringLiteral(
                                 "g.add_inset(source_layer=%1, rel_x=%2, rel_y=%3, rel_w=%4, rel_h=%5, show_connectors=%6)")
                                 .arg(srcIdx)
                                 .arg(pyFormatNum(nr.x()), pyFormatNum(nr.y()),
                                      pyFormatNum(nr.width()), pyFormatNum(nr.height()),
                                      ml->insetShowConnectors(lg) ? QStringLiteral("True")
                                                                  : QStringLiteral("False"));
            }
        } else if (!preCreatedPrimaryLayers.contains(li)) {
            lines << QStringLiteral("g.add_layer()");
        }
    }

    if (nPrimaryLayers >= 2) {
        if (preCreatedPrimaryLayers.size() < nPrimaryLayers) {
            lines << QStringLiteral("g.arrange_layers(rows=%1, cols=%2, fit=False)")
                             .arg(std::max(1, ml->getRows()))
                             .arg(std::max(1, ml->getCols()));
        }
        if (ml->hasPanelLabels()) {
            const QString plStr =
                    (ml->panelLabelPlacement() == MultiLayer::PanelInsideTopLeft)
                    ? QStringLiteral("inside_top_left")
                    : QStringLiteral("outside_top_left");
            lines << QStringLiteral("g.set_panel_labels(True, placement=%1, pattern=%2)")
                             .arg(pyEscapeStr(plStr), pyEscapeStr(ml->panelLabelPattern()));
        }
    }

    // Style preset: applied once all layers exist, as the base look; the explicit page,
    // axis, curve and legend settings emitted afterwards keep the graph's current values.
    const QString stylePresetId = ml->property("toriAppliedStylePreset").toString().trimmed();
    if (!stylePresetId.isEmpty()) {
        MultiLayer::GraphStylePresetSpec presetSpec;
        if (MultiLayer::findStylePreset(stylePresetId, &presetSpec)) {
            lines << QStringLiteral("g.apply_style_preset(%1)").arg(pyEscapeStr(stylePresetId));
            if (!pageSizeLine.isEmpty())
                lines << pageSizeLine;
        } else {
            noteUnreproduced(pyTr("style preset '%1' (not found on this PC)")
                                     .arg(sanitizePyCommentText(stylePresetId)));
        }
    }

    // Detect what the per-layer pass below cannot rebuild.
    for (int li = 0; li < layers.size(); ++li) {
        Graph *lg = layers[li];
        if (!lg)
            continue;
        if (!lg->fitCurvesList().isEmpty())
            noteUnreproduced(pyTr(
                    "fit curve (reproduced as numeric data, but fit conditions and fitted parameters are not reproduced)"));
        if (DataCurve *cmDc = lg->pointColorMapCurve()) {
            Table *ct = cmDc->table();
            const DataCurve::PointColorMap &pm = cmDc->pointColorMap();
            const bool canReproduce = ct && tblVarMap.contains(ct)
                    && ct->colIndex(cmDc->xColumnName()) >= 0
                    && ct->colIndex(cmDc->yColumnName()) >= 0
                    && ct->colIndex(pm.zColumn) >= 0;
            if (!canReproduce)
                noteUnreproduced(pyTr("value-mapped scatter coloring and colorbar"));
        }
        for (int ax = 0; ax < QwtPlot::axisCnt; ++ax) {
            if (lg->hasWeibullProbabilityAxis(ax) || lg->hasCustomTickLabels(ax)) {
                noteUnreproduced(pyTr(
                        "custom or probability tick labels (probability plots fall back to standard ticks)"));
            }
        }
        bool hasStackOffset = false;
        for (int ci = 0; ci < lg->curves(); ++ci) {
            QwtPlotCurve *c = lg->curve(ci);
            if (!c)
                continue;
            const int ct = lg->curveType(ci);
            if (ct == Graph::VectXYXY || ct == Graph::VectXYAM)
                noteUnreproduced(pyTr("vector plot arrows (drawn as connected line segments instead)"));
            if (auto *bar = dynamic_cast<QwtBarCurve *>(c)) {
                if (bar->isStacked())
                    noteUnreproduced(pyTr(
                            "stacked bar stacking (bars overlap; recreate with app.plot_stacked_bars)"));
                else if (bar->offset() != 0)
                    noteUnreproduced(pyTr("grouped bar horizontal offset (bar offset amount)"));
            }
            if (auto *pc = dynamic_cast<PlotCurve *>(c)) {
                if (pc->stackOffsetX() != 0.0 || pc->stackOffsetY() != 0.0 || pc->stackNormalize01())
                    hasStackOffset = true;
            }
            if (li == 0 && ci == firstEmittedCurveOnL0 && c->yAxis() == QwtPlot::yRight)
                noteUnreproduced(pyTr(
                        "first curve assigned to right Y axis (drawn on left Y axis)"));
        }
        if (hasStackOffset)
            noteUnreproduced(pyTr(
                    "stack offset / waterfall curve offsets (re-apply with g.set_stack_offsets)"));
    }

    // Per-layer curves, styles, error bars, axes, reference lines, shaded bands, annotations
    for (int li = 0; li < layers.size(); ++li) {
        Graph *lg = layers[li];
        Plot *plt = lg ? lg->plotWidget() : nullptr;
        if (!lg || !plt)
            continue;
        const bool isInset = ml->isInsetLayer(lg);
        const bool isNonCartesianLayer = invokeGraphIsPolarPlot(lg) || lg->isTernaryPlot()
                || lg->isPiePlot() || lg->isPoleFigurePlot();
        const QString layerKw =
                (layers.size() > 1) ? QStringLiteral(", layer=%1").arg(li) : QString();

        // Non-L0 PoleFigure / Spectrogram / BoxCurve / Violin reproduction
        if (li >= 1 && !isInset) {
            bool layerHandledSpec = false;
            if (lg->isPoleFigurePlot()) {
                layerHandledSpec = emitPoleFigureForLayer(lg, li, false, layerKw);
            }
            if (!layerHandledSpec) {
                if (Spectrogram *sp = firstSpectrogramOn(lg)) {
                    if (sp->matrix() && matVarMap.contains(sp->matrix())) {
                        lines << QStringLiteral(
                                         "app.call('plot.matrix', matrix=%1.name, style=%2, graph=g.name, layer=%3)")
                                         .arg(matVarMap.value(sp->matrix()),
                                              pyEscapeStr(spectrogramStyleStr(sp)))
                                         .arg(li);
                        emitSpectrogramColorbar(sp, layerKw);
                    }
                }
            }
            // Group any unhandled BoxCurve / Violin curves on this layer by source table.
            for (int ci = 0; ci < lg->curves(); ++ci) {
                if (preHandledLayerCurves.contains(qMakePair(li, ci)))
                    continue;
                auto *bc0 = dynamic_cast<BoxCurve *>(lg->curve(ci));
                Table *ctBox = bc0 ? bc0->table() : nullptr;
                if (!bc0 || !ctBox || !tblVarMap.contains(ctBox))
                    continue;
                const bool wantViolin = bc0->violinMode();
                QStringList qBoxCols;
                for (int cj = ci; cj < lg->curves(); ++cj) {
                    if (preHandledLayerCurves.contains(qMakePair(li, cj)))
                        continue;
                    if (auto *bc = dynamic_cast<BoxCurve *>(lg->curve(cj))) {
                        if (bc->table() == ctBox && bc->violinMode() == wantViolin) {
                            const int yIdx = ctBox->colIndex(bc->yColumnName());
                            if (yIdx >= 0) {
                                qBoxCols << pyEscapeStr(ctBox->colLabel(yIdx));
                                preHandledLayerCurves.insert(qMakePair(li, cj));
                            }
                        }
                    }
                }
                if (!qBoxCols.isEmpty()) {
                    if (wantViolin) {
                        lines << QStringLiteral(
                                         "app.call('plot.violin', graph=g.name, layer=%1, table=%2.name, cols=[%3], show_points=%4)")
                                         .arg(li)
                                         .arg(tblVarMap.value(ctBox),
                                              qBoxCols.join(QStringLiteral(", ")),
                                              bc0->showDataPoints() ? QStringLiteral("True")
                                                                    : QStringLiteral("False"));
                    } else {
                        lines << QStringLiteral(
                                         "app.call('plot.box', graph=g.name, layer=%1, table=%2.name, cols=[%3], notched=%4, show_points=%5%6)")
                                         .arg(li)
                                         .arg(tblVarMap.value(ctBox),
                                              qBoxCols.join(QStringLiteral(", ")),
                                              (bc0->boxStyle() == BoxCurve::Notch)
                                                      ? QStringLiteral("True")
                                                      : QStringLiteral("False"),
                                              bc0->showDataPoints() ? QStringLiteral("True")
                                                                    : QStringLiteral("False"),
                                              boxWhiskerExtra(bc0));
                    }
                }
            }
        }

        // Additional curves on this layer (skip inset auto-cloned curves)
        int nonErrIdx = 0;
        int dataCurveIdx = 0;
        if (!isInset) {
            for (int ci = 0; ci < lg->curves(); ++ci) {
                QwtPlotCurve *c = lg->curve(ci);
                if (!c || lg->curveType(ci) == Graph::ErrorBars
                    || dynamic_cast<QwtErrorPlotCurve *>(c) != nullptr) {
                    continue;
                }
                auto *boxC = dynamic_cast<BoxCurve *>(c);
                const bool isPieOrHist = lg->isPiePlot()
                        || lg->curveType(ci) == Graph::Pie
                        || lg->curveType(ci) == Graph::Histogram
                        || dynamic_cast<QwtPieCurve *>(c) != nullptr
                        || dynamic_cast<QwtHistogram *>(c) != nullptr;
                auto *dc = dynamic_cast<DataCurve *>(c);
                auto *fc = dynamic_cast<FunctionCurve *>(c);
                auto *pc = dynamic_cast<PlotCurve *>(c);
                // index among the layer's table-backed data curves (graph.add_error_band's 'curve')
                const int thisDataCurveIdx = (dc && dc->table()) ? dataCurveIdx++ : -1;
                bool curvePresent = (li == 0 && (ci == firstEmittedCurveOnL0 || l0HandledAllCurves))
                        || preHandledLayerCurves.contains(qMakePair(li, ci));
                if (!curvePresent) {
                    if (dc && dc->hasPointColorMap() && emitColorScatterCurve(dc, li, false)) {
                        curvePresent = true;
                    } else if (dc && dc->table() && tblVarMap.contains(dc->table())) {
                        Table *ct = dc->table();
                        const int xIdx = ct->colIndex(dc->xColumnName());
                        const int yIdx = ct->colIndex(dc->yColumnName());
                        if (xIdx >= 0 && yIdx >= 0) {
                            const QString xLbl = ct->colLabel(xIdx);
                            const QString yLbl = ct->colLabel(yIdx);
                            const bool rightY = (c->yAxis() == QwtPlot::yRight);
                            lines << QStringLiteral(
                                             "g.add_curve(%1.name, %2, x_col=%3, style=%4, right_axis=%5%6)")
                                             .arg(tblVarMap.value(ct), pyEscapeStr(yLbl),
                                                  pyEscapeStr(xLbl),
                                                  pyEscapeStr(curveStyleStr(lg, ci)),
                                                  rightY ? QStringLiteral("True")
                                                         : QStringLiteral("False"),
                                                  layerKw);
                            curvePresent = true;
                        }
                    } else if (fc && !fc->formulas().isEmpty()) {
                        const QStringList fList = fc->formulas();
                        const QString tStr = (fc->functionType() == FunctionCurve::Polar)
                                ? QStringLiteral("polar")
                                : ((fc->functionType() == FunctionCurve::Parametric)
                                           ? QStringLiteral("parametric")
                                           : QStringLiteral("normal"));
                        const QString fArg = (fList.size() == 1)
                                ? pyEscapeStr(fList.first())
                                : QStringLiteral("[%1, %2]").arg(pyEscapeStr(fList[0]),
                                                                 pyEscapeStr(fList[1]));
                        const QString vStr = fc->variable().trimmed().isEmpty()
                                ? QStringLiteral("x")
                                : fc->variable().trimmed();
                        lines << QStringLiteral(
                                         "app.call('plot.function', graph=g.name, layer=%1, formula=%2, variable=%3, x_min=%4, x_max=%5, points=%6, type=%7, title=%8)")
                                         .arg(li)
                                         .arg(fArg, pyEscapeStr(vStr),
                                              pyFormatNum(fc->startRange()),
                                              pyFormatNum(fc->endRange()))
                                         .arg(std::max(2, static_cast<int>(fc->dataSize())))
                                         .arg(pyEscapeStr(tStr), pyEscapeStr(c->title().text()));
                        curvePresent = true;
                    }
                }
                if (!curvePresent)
                    continue;
                if (isPieOrHist) {
                    ++nonErrIdx;
                    continue;
                }

                const double lw = c->pen().widthF() > 0.0 ? c->pen().widthF()
                                                          : double(std::max(1, c->pen().width()));
                const int skip = pc ? std::max(1, pc->symbolSkip()) : 1;
                if (boxC) {
                    QString boxStyleExtra;
                    if (c->brush().style() != Qt::NoBrush) {
                        const QColor fillCol = c->brush().color();
                        const QString fcHex = (fillCol.alpha() < 255)
                                ? fillCol.name(QColor::HexArgb)
                                : fillCol.name(QColor::HexRgb);
                        boxStyleExtra = QStringLiteral(", fill_area=True, fill_color=%1")
                                                .arg(pyEscapeStr(fcHex));
                    } else {
                        boxStyleExtra = QStringLiteral(", fill_area=False");
                    }
                    lines << QStringLiteral("g.set_curve_style(%1, color=%2, lw=%3%4%5)")
                                     .arg(nonErrIdx)
                                     .arg(pyEscapeStr(c->pen().color().name(QColor::HexRgb)),
                                          pyFormatNum(lw), boxStyleExtra, layerKw);
                } else if (dc && dc->hasPointColorMap()) {
                    lines << QStringLiteral(
                                     "g.set_curve_style(%1, marker=%2, ms=%3, symbol_skip=%4%5)")
                                     .arg(nonErrIdx)
                                     .arg(pyEscapeStr(symStyleStr(c->symbol().style())))
                                     .arg(std::max(1, c->symbol().size().width()))
                                     .arg(skip)
                                     .arg(layerKw);
                } else {
                    lines << QStringLiteral(
                                     "g.set_curve_style(%1, color=%2, lw=%3, ls=%4, marker=%5, ms=%6, symbol_hollow=%7, symbol_skip=%8%9)")
                                     .arg(nonErrIdx)
                                     .arg(pyEscapeStr(c->pen().color().name(QColor::HexRgb)),
                                          pyFormatNum(lw), pyEscapeStr(penStyleStr(c->pen().style())),
                                          pyEscapeStr(symStyleStr(c->symbol().style())))
                                     .arg(std::max(1, c->symbol().size().width()))
                                     .arg(Graph::isSymbolWhiteHollow(c->symbol())
                                                  ? QStringLiteral("True")
                                                  : QStringLiteral("False"))
                                     .arg(skip)
                                     .arg(layerKw);
                }
                if (!lg->isCurveVisible(ci)) {
                    lines << QStringLiteral("g.set_curve_visible(%1, False%2)")
                                     .arg(nonErrIdx)
                                     .arg(layerKw);
                }
                if (dc) {
                    // Error bands (E11): one symmetric band curve, or a plus-only and a
                    // minus-only band curve together (asymmetric band).
                    QString bandPlus, bandMinus, bandBoth;
                    Table *bandTable = nullptr;
                    int bandAlpha = 64;
                    for (DataCurve *ebBase : dc->errorBarsList()) {
                        auto *ec = dynamic_cast<QwtErrorPlotCurve *>(ebBase);
                        if (!ec || !ec->isBand() || !ec->table()
                            || !tblVarMap.contains(ec->table()))
                            continue;
                        const int eIdx = ec->table()->colIndex(ec->yColumnName());
                        if (eIdx < 0)
                            continue;
                        const QString eLbl = ec->table()->colLabel(eIdx);
                        bandTable = ec->table();
                        bandAlpha = ec->bandAlpha();
                        if (ec->plusSide() && !ec->minusSide())
                            bandPlus = eLbl;
                        else if (ec->minusSide() && !ec->plusSide())
                            bandMinus = eLbl;
                        else
                            bandBoth = eLbl;
                    }
                    if (bandTable && thisDataCurveIdx >= 0) {
                        const QString bandTblArg = tblVarMap.value(bandTable);
                        if (!bandBoth.isEmpty()) {
                            lines << QStringLiteral(
                                             "g.add_error_band(%1, %2, table=%3.name, alpha=%4%5)")
                                             .arg(thisDataCurveIdx)
                                             .arg(pyEscapeStr(bandBoth), bandTblArg)
                                             .arg(bandAlpha)
                                             .arg(layerKw);
                        }
                        if (!bandPlus.isEmpty() || !bandMinus.isEmpty()) {
                            const QString plusLbl = bandPlus.isEmpty() ? bandMinus : bandPlus;
                            const QString minusLbl = bandMinus.isEmpty() ? bandPlus : bandMinus;
                            lines << QStringLiteral(
                                             "g.add_error_band(%1, %2, err_minus=%3, table=%4.name, alpha=%5%6)")
                                             .arg(thisDataCurveIdx)
                                             .arg(pyEscapeStr(plusLbl), pyEscapeStr(minusLbl),
                                                  bandTblArg)
                                             .arg(bandAlpha)
                                             .arg(layerKw);
                        }
                    }
                    for (DataCurve *ebBase : dc->errorBarsList()) {
                        if (auto *ec = dynamic_cast<QwtErrorPlotCurve *>(ebBase)) {
                            if (ec->isBand())
                                continue;
                            if (ec->table() && tblVarMap.contains(ec->table())) {
                                Table *et = ec->table();
                                const int eIdx = et->colIndex(ec->yColumnName());
                                if (eIdx >= 0) {
                                    const QString eLbl = et->colLabel(eIdx);
                                    const QString dir = ec->xErrors() ? QStringLiteral("x")
                                                                      : QStringLiteral("y");
                                    lines << QStringLiteral(
                                                     "g.add_error_bars(%1, curve=%2, table=%3.name, direction=%4%5)")
                                                     .arg(pyEscapeStr(eLbl))
                                                     .arg(nonErrIdx)
                                                     .arg(tblVarMap.value(et), pyEscapeStr(dir),
                                                          layerKw);
                                }
                            }
                        }
                    }
                }
                ++nonErrIdx;
            }
        }

        if (!plt->title().text().trimmed().isEmpty()) {
            lines << QStringLiteral("g.set_title(%1, font_size=%2%3)")
                             .arg(pyEscapeStr(plt->title().text()))
                             .arg(fontPtPy(plt->title().font(), 8.0))
                             .arg(layerKw);
        }

        // Axes & category labels & breaks (skip hidden Cartesian scaffolding on Polar/Ternary/Pie)
        // Pass 1: emit all g.set_axis(...) and g.set_category_labels(...) calls BEFORE Pass 2
        // (g.set_axis_break(...)), so g.set_axis('right', ...) / g.set_axis('top', ...) never
        // clobbers a synced opposite-axis PiecewiseBreakScaleEngine.
        static const int kAxes[4] = { QwtPlot::xBottom, QwtPlot::yLeft, QwtPlot::xTop,
                                      QwtPlot::yRight };
        static const char *const kAxNames[4] = { "x", "y", "top", "right" };
        const QList<int> majTickStyles = plt->getMajorTicksType();
        for (int ai = 0; !isNonCartesianLayer && ai < 4; ++ai) {
            const int ax = kAxes[ai];
            const bool enabled = plt->axisEnabled(ax);
            if (!enabled && (ax == QwtPlot::xTop || ax == QwtPlot::yRight))
                continue;
            const QwtScaleDiv *sdiv = plt->axisScaleDiv(ax);
            if (!sdiv)
                continue;
            const double rawLo = sdiv->lowerBound();
            const double rawHi = sdiv->upperBound();
            const double lo = std::min(rawLo, rawHi);
            const double hi = std::max(rawLo, rawHi);
            if (!std::isfinite(hi - lo) || hi <= lo)
                continue;
            const bool isLog =
                    (dynamic_cast<const QwtLog10ScaleEngine *>(plt->axisScaleEngine(ax)) != nullptr);
            const bool inv = (rawLo > rawHi);
            const QStringList cats = invokeGraphCategoryLabels(lg, ax);
            const Graph::AxisBreakSpec brk = lg->axisBreak(ax);
            QString axArgs = QStringLiteral("min=%1, max=%2, scale=%3, enabled=%4")
                                     .arg(pyFormatNum(lo), pyFormatNum(hi),
                                          pyEscapeStr(isLog ? QStringLiteral("log10")
                                                            : QStringLiteral("linear")),
                                          enabled ? QStringLiteral("True")
                                                  : QStringLiteral("False"));
            if (inv)
                axArgs += QStringLiteral(", inverted=True");
            double effStep = lg->axisStep(ax);
            if (effStep <= 0.0 && !isLog && !brk.enabled && cats.isEmpty()
                && !lg->hasWeibullProbabilityAxis(ax)) {
                const QwtValueList majTicks = sdiv->ticks(QwtScaleDiv::MajorTick);
                if (majTicks.size() >= 2) {
                    const double d0 = std::abs(majTicks[1] - majTicks[0]);
                    bool uniform = (d0 > 0.0 && std::isfinite(d0) && (hi - lo) / d0 <= 200.0);
                    for (int ti = 2; uniform && ti < majTicks.size(); ++ti) {
                        const double di = std::abs(majTicks[ti] - majTicks[ti - 1]);
                        if (std::abs(di - d0) > 1e-4 * std::max(1.0, d0))
                            uniform = false;
                    }
                    if (uniform)
                        effStep = d0;
                }
            }
            if (effStep > 0.0)
                axArgs += QStringLiteral(", step=%1").arg(pyFormatNum(effStep));
            const QString tTxt = lg->axisTitle(ax);
            if (!tTxt.isEmpty())
                axArgs += QStringLiteral(", title=%1").arg(pyEscapeStr(tTxt));
            const QFont titleFnt = lg->axisTitleFont(ax);
            if (titleFnt.pointSizeF() >= 4.0)
                axArgs += QStringLiteral(", title_font_size=%1").arg(fontPtPy(titleFnt, 4.0));
            const QFont tickFnt = plt->axisFont(ax);
            if (tickFnt.pointSizeF() >= 4.0)
                axArgs += QStringLiteral(", tick_font_size=%1").arg(fontPtPy(tickFnt, 4.0));
            if (enabled && !lg->hasTickLabelsOnAxis(ax))
                axArgs += QStringLiteral(", show_labels=False");
            const int majStyle = majTickStyles.value(ax, int(ScaleDraw::Out));
            if (majStyle == int(ScaleDraw::None)) {
                axArgs += QStringLiteral(", show_ticks=False");
            } else {
                axArgs += QStringLiteral(", ticks_inward=%1")
                                  .arg(majStyle == int(ScaleDraw::In) ? QStringLiteral("True")
                                                                      : QStringLiteral("False"));
            }
            if (lg->labelsRotation(ax) != 0)
                axArgs += QStringLiteral(", label_rotation=%1").arg(lg->labelsRotation(ax));
            lines << QStringLiteral("g.set_axis(%1, %2%3)")
                             .arg(pyEscapeStr(QLatin1String(kAxNames[ai])), axArgs, layerKw);

            if (!cats.isEmpty()) {
                QStringList qCats;
                for (const QString &cs : cats)
                    qCats << pyEscapeStr(cs);
                lines << QStringLiteral("g.set_category_labels([%1], axis=%2%3)")
                                 .arg(qCats.join(QStringLiteral(", ")),
                                      pyEscapeStr(QLatin1String(kAxNames[ai])), layerKw);
            }
        }
        // Pass 2: emit axis breaks AFTER all g.set_axis(...) calls
        for (int ai = 0; !isNonCartesianLayer && ai < 4; ++ai) {
            const int ax = kAxes[ai];
            const Graph::AxisBreakSpec brk = lg->axisBreak(ax);
            if (brk.enabled) {
                lines << QStringLiteral(
                                 "g.set_axis_break(%1, enabled=True, start=%2, end=%3, position_pct=%4, log_before=%5, log_after=%6%7)")
                                 .arg(pyEscapeStr(QLatin1String(kAxNames[ai])),
                                      pyFormatNum(brk.breakFrom), pyFormatNum(brk.breakTo),
                                      pyFormatNum(brk.positionPct * 100.0),
                                      brk.log10Before ? QStringLiteral("True")
                                                      : QStringLiteral("False"),
                                      brk.log10After ? QStringLiteral("True")
                                                     : QStringLiteral("False"),
                                      layerKw);
            }
        }

        if (Grid *grid = lg->grid()) {
            if (!isNonCartesianLayer
                && (grid->xEnabled() || grid->yEnabled() || grid->xMinEnabled()
                    || grid->yMinEnabled())) {
                lines << QStringLiteral(
                                 "g.set_grid(x_major=%1, y_major=%2, x_minor=%3, y_minor=%4%5)")
                                 .arg(grid->xEnabled() ? QStringLiteral("True")
                                                       : QStringLiteral("False"),
                                      grid->yEnabled() ? QStringLiteral("True")
                                                       : QStringLiteral("False"),
                                      grid->xMinEnabled() ? QStringLiteral("True")
                                                          : QStringLiteral("False"),
                                      grid->yMinEnabled() ? QStringLiteral("True")
                                                          : QStringLiteral("False"),
                                      layerKw);
            }
        }

        // Reference lines
        for (const Graph::ReferenceLineSpec &rs : lg->referenceLines()) {
            if (!rs.visible)
                continue;
            const QString orient = (rs.orientation == 1) ? QStringLiteral("vertical")
                                                         : QStringLiteral("horizontal");
            lines << QStringLiteral(
                             "g.add_reference_line(orientation=%1, value=%2, label=%3, color=%4, width=%5%6)")
                             .arg(pyEscapeStr(orient), pyFormatNum(rs.value),
                                  pyEscapeStr(rs.label),
                                  pyEscapeStr(rs.color.name(QColor::HexRgb)),
                                  pyFormatNum(rs.width), layerKw);
        }

        // Shaded bands (parsed cleanly from Graph::saveToString line-anchored <ToriRefLine kind="band">)
        {
            const QString savedG = lg->saveToString();
            static const QRegularExpression rxAttr(
                    QStringLiteral("([A-Za-z0-9_]+)=\"([^\"]*)\""));
            for (const QString &rawLn : savedG.split(QLatin1Char('\n'))) {
                const QString ln = rawLn.trimmed();
                if (!ln.startsWith(QStringLiteral("<ToriRefLine"))
                    || !ln.contains(QStringLiteral("kind=\"band\""))) {
                    continue;
                }
                QMap<QString, QString> am;
                auto ait = rxAttr.globalMatch(ln);
                while (ait.hasNext()) {
                    const auto m = ait.next();
                    am.insert(m.captured(1), m.captured(2));
                }
                if (am.value(QStringLiteral("kind")) != QStringLiteral("band"))
                    continue;
                const QString xCsv = am.value(QStringLiteral("x"));
                const QString y1Csv = am.contains(QStringLiteral("yLow"))
                        ? am.value(QStringLiteral("yLow"))
                        : am.value(QStringLiteral("y1"));
                const QString y2Csv = am.contains(QStringLiteral("yHigh"))
                        ? am.value(QStringLiteral("yHigh"))
                        : am.value(QStringLiteral("y2"));
                auto sanitizeBandNumCsv = [](const QString &rawCsv, QString *outPyList) -> bool {
                    const QStringList parts =
                            rawCsv.split(QLatin1Char(','), Qt::SkipEmptyParts);
                    if (parts.isEmpty())
                        return false;
                    QStringList formatted;
                    formatted.reserve(parts.size());
                    for (const QString &tok : parts) {
                        bool ok = false;
                        const double v = tok.trimmed().toDouble(&ok);
                        if (!ok || !std::isfinite(v))
                            return false;
                        formatted << pyFormatNum(v);
                    }
                    *outPyList = formatted.join(QStringLiteral(", "));
                    return true;
                };
                QString xSafe, y1Safe, y2Safe;
                if (sanitizeBandNumCsv(xCsv, &xSafe) && sanitizeBandNumCsv(y1Csv, &y1Safe)
                    && sanitizeBandNumCsv(y2Csv, &y2Safe)) {
                    const QColor bc(am.value(QStringLiteral("color")));
                    lines << QStringLiteral(
                                     "g.add_shaded_band([%1], [%2], [%3], color=%4, alpha=%5, label=%6%7)")
                                     .arg(xSafe, y1Safe, y2Safe,
                                          pyEscapeStr(bc.isValid() ? bc.name(QColor::HexRgb)
                                                                   : QStringLiteral("#4C78A8")))
                                     .arg(bc.isValid() ? bc.alpha() : 55)
                                     .arg(pyEscapeStr(am.value(QStringLiteral("title"))), layerKw);
                } else if (!xCsv.isEmpty() || !y1Csv.isEmpty() || !y2Csv.isEmpty()) {
                    noteUnreproduced(pyTr("shaded band numeric data (skipped because of non-numeric entries)"));
                }
            }
        }

        // Legend visibility & styling
        Legend *leg = lg->legend();
        if (!lg->isLegendVisible()) {
            lines << QStringLiteral("g.set_legend(visible=False%1)").arg(layerKw);
        } else if (leg) {
            const QString legPt = fontPtPy(leg->font(), 6.0);
            const bool legFramed = (leg->frameStyle() != 0);
            QString legExtra;
            if (leg->numColumns() > 1)
                legExtra += QStringLiteral(", columns=%1").arg(leg->numColumns());
            if (lg->property("_outsideRightMargin").toInt() > 0) {
                legExtra += QStringLiteral(", position='outside_right'");
            } else if (lg->isPiePlot()) {
                legExtra += QStringLiteral(", position='top_center'");
                if (!leg->text().trimmed().isEmpty())
                    legExtra += QStringLiteral(", text=%1").arg(pyEscapeStr(leg->text()));
            } else if (invokeGraphIsPolarPlot(lg)) {
                legExtra += QStringLiteral(", position='top_right'");
            } else if (std::isfinite(leg->xValue()) && std::isfinite(leg->yValue())) {
                legExtra += QStringLiteral(", x=%1, y=%2")
                                    .arg(pyFormatNum(leg->xValue()), pyFormatNum(leg->yValue()));
            }
            if (!lg->isPiePlot() && preHandledLayerCurves.contains(qMakePair(li, 0))
                && lg->curves() >= 2 && !leg->text().trimmed().isEmpty()) {
                legExtra += QStringLiteral(", text=%1").arg(pyEscapeStr(leg->text()));
            }
            lines << QStringLiteral("g.set_legend(visible=True, font_size=%1, framed=%2%3%4)")
                             .arg(legPt)
                             .arg(legFramed ? QStringLiteral("True") : QStringLiteral("False"))
                             .arg(legExtra, layerKw);
        }

        // Text callouts & arrows
        for (int tid : lg->textMarkerKeys()) {
            Legend *tm = lg->textMarker(tid);
            if (!tm || tm == leg || tm->text().trimmed().isEmpty())
                continue;
            lines << QStringLiteral(
                             "g.add_text(%1, %2, %3, font_size=%4, color=%5, frame=%6, angle=%7%8)")
                             .arg(pyEscapeStr(tm->text()), pyFormatNum(tm->xValue()),
                                  pyFormatNum(tm->yValue()))
                             .arg(fontPtPy(tm->font(), 6.0))
                             .arg(pyEscapeStr(tm->textColor().name(QColor::HexRgb)))
                             .arg(tm->frameStyle())
                             .arg(tm->angle())
                             .arg(layerKw);
        }
        for (int aid : lg->lineMarkerKeys()) {
            ArrowMarker *am = lg->arrow(aid);
            if (!am)
                continue;
            const QwtDoublePoint p1 = am->startPointCoord();
            const QwtDoublePoint p2 = am->endPointCoord();
            lines << QStringLiteral(
                             "g.add_arrow(%1, %2, %3, %4, color=%5, width=%6, end_arrow=%7, start_arrow=%8%9)")
                             .arg(pyFormatNum(p1.x()), pyFormatNum(p1.y()), pyFormatNum(p2.x()),
                                  pyFormatNum(p2.y()),
                                  pyEscapeStr(am->color().name(QColor::HexRgb)))
                             .arg(std::max(1, am->width()))
                             .arg(am->hasEndArrow() ? QStringLiteral("True")
                                                    : QStringLiteral("False"),
                                  am->hasStartArrow() ? QStringLiteral("True")
                                                      : QStringLiteral("False"),
                                  layerKw);
        }
    }
    if (wantStackedHistSquareFrame)
        lines << QStringLiteral("g.apply_square_frame()");

    return finishScript();
}

bool dispatchCoreAndGraphMethod(PythonAutomationServer *server, ApplicationWindow *d_app,
                                bool &focusHudFallback, QString &workspaceModeFallback,
                                QString &lastUndoDomain, QString &lastUndoGraph,
                                const QString &method, const QJsonObject &params,
                                QJsonObject &outResult)
{
    if (dispatchAppTableMatrixMethod(server, d_app, focusHudFallback, workspaceModeFallback,
                                     lastUndoDomain, lastUndoGraph, method, params, outResult)) {
        return true;
    }
    if (dispatchPlotAndLayerMethod(d_app, method, params, outResult))
        return true;
    if (dispatchGraphDetailAndExportMethod(d_app, method, params, outResult))
        return true;
    return false;
}

bool dispatchAnalysisAndScienceMethod(ApplicationWindow *d_app, const QString &method,
                                      const QJsonObject &params, QJsonObject &outResult)
{
    if (dispatchAnalysisAndSignalMethod(d_app, method, params, outResult))
        return true;
    if (dispatchCurveMathAndStatsMethod(d_app, method, params, outResult))
        return true;
    return false;
}

} // namespace rpc_internal

// ============================================================================
// PythonAutomationServer — Transports & JSON-RPC 2.0 Framing
// ============================================================================

namespace {

QString generateRandom256BitHexToken()
{
    QByteArray rawBytes;
    rawBytes.reserve(32);
    for (int i = 0; i < 8; ++i) {
        const quint32 v = QRandomGenerator::system()->generate();
        rawBytes.append(static_cast<char>((v >> 24) & 0xFF));
        rawBytes.append(static_cast<char>((v >> 16) & 0xFF));
        rawBytes.append(static_cast<char>((v >> 8) & 0xFF));
        rawBytes.append(static_cast<char>(v & 0xFF));
    }
    return QString::fromLatin1(rawBytes.toHex()).toLower();
}

bool constantTimeTokenEquals(const QString &provided, const QString &expected)
{
    const QByteArray a = provided.toUtf8();
    const QByteArray b = expected.toUtf8();
    if (b.isEmpty())
        return false;
    const int lenA = a.size();
    const int lenB = b.size();
    volatile unsigned int diff = static_cast<unsigned int>(lenA ^ lenB);
    const int n = std::max(lenA, lenB);
    for (int i = 0; i < n; ++i) {
        const unsigned char ca = (i < lenA) ? static_cast<unsigned char>(a.at(i)) : 0u;
        const unsigned char cb = (i < lenB) ? static_cast<unsigned char>(b.at(i)) : 0u;
        diff |= static_cast<unsigned int>(ca ^ cb);
    }
    return diff == 0u;
}

QString rpcTokenDirectoryPath()
{
    QString base = qEnvironmentVariable("LOCALAPPDATA").trimmed();
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty())
        base = QDir::tempPath();
    return QDir(base).filePath(QStringLiteral("ToriGraph/rpc"));
}

void lockDownWindowsOwnerOnlyAcl(const QString &targetPath)
{
    QFile::setPermissions(targetPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#ifdef Q_OS_WIN
    QString userSidClause;
    HANDLE hTok = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hTok) && hTok) {
        DWORD needed = 0;
        GetTokenInformation(hTok, TokenUser, nullptr, 0, &needed);
        if (needed > 0) {
            std::vector<BYTE> tokBuf(needed, 0);
            if (GetTokenInformation(hTok, TokenUser, tokBuf.data(), needed, &needed)) {
                auto *tu = reinterpret_cast<TOKEN_USER *>(tokBuf.data());
                LPWSTR sidStr = nullptr;
                if (tu->User.Sid && ConvertSidToStringSidW(tu->User.Sid, &sidStr) && sidStr) {
                    userSidClause = QStringLiteral("(A;;FA;;;%1)")
                                            .arg(QString::fromWCharArray(sidStr));
                    LocalFree(sidStr);
                }
            }
        }
        CloseHandle(hTok);
    }

    const QString sddl = QStringLiteral("D:P(A;;FA;;;SY)(A;;FA;;;OW)") + userSidClause;
    const std::wstring wSddl = sddl.toStdWString();
    PSECURITY_DESCRIPTOR pSD = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                wSddl.c_str(), SDDL_REVISION_1, &pSD, nullptr)) {
        // Fallback to strict D:P(A;;FA;;;SY)(A;;FA;;;OW)
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                    L"D:P(A;;FA;;;SY)(A;;FA;;;OW)", SDDL_REVISION_1, &pSD, nullptr)) {
            return;
        }
    }
    PACL pDacl = nullptr;
    BOOL daclPresent = FALSE;
    BOOL daclDefaulted = FALSE;
    if (GetSecurityDescriptorDacl(pSD, &daclPresent, &pDacl, &daclDefaulted) && daclPresent) {
        std::wstring wPath = QDir::toNativeSeparators(targetPath).toStdWString();
        SetNamedSecurityInfoW(wPath.data(), SE_FILE_OBJECT,
                              DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                              nullptr, nullptr, pDacl, nullptr);
    }
    LocalFree(pSD);
#endif
}

} // namespace

PythonAutomationServer::PythonAutomationServer(ApplicationWindow *app, QObject *parent)
    : QObject(parent), d_app(app)
{
    gsl_set_error_handler_off();
}

PythonAutomationServer::~PythonAutomationServer()
{
    stop();
}

void PythonAutomationServer::setTcpAuthToken(const QString &token)
{
    d_tcp_auth_token = token.trimmed();
    if (isTcpListening() && d_tcp_port > 0 && !d_tcp_auth_token.isEmpty())
        writeTcpTokenFile(d_tcp_port);
}

QString PythonAutomationServer::tcpAuthToken() const
{
    return d_tcp_auth_token;
}

void PythonAutomationServer::writeTcpTokenFile(quint16 port)
{
    if (port == 0 || d_tcp_auth_token.isEmpty())
        return;
    const QString rpcDir = rpcTokenDirectoryPath();
    QDir().mkpath(rpcDir);
    const QString filePath =
            QDir(rpcDir).filePath(QStringLiteral("tcp_%1.token").arg(port));
    if (!d_tcp_token_file_path.isEmpty() && d_tcp_token_file_path != filePath)
        removeTcpTokenFile();

    QFile tf(filePath);
    if (tf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        tf.write(d_tcp_auth_token.toUtf8());
        tf.flush();
        tf.close();
        lockDownWindowsOwnerOnlyAcl(filePath);
        d_tcp_token_file_path = filePath;
    }
}

void PythonAutomationServer::removeTcpTokenFile()
{
    if (!d_tcp_token_file_path.isEmpty()) {
        QFile::remove(d_tcp_token_file_path);
        d_tcp_token_file_path.clear();
    }
}

bool PythonAutomationServer::startTcp(quint16 port, const QHostAddress &bindAddress)
{
    if (d_tcp_server) {
        if (d_tcp_server->isListening()
            && (port == 0 || d_tcp_server->serverPort() == port)
            && d_bind_address == bindAddress) {
            d_tcp_port = d_tcp_server->serverPort();
            if (d_tcp_auth_token.isEmpty())
                d_tcp_auth_token = generateRandom256BitHexToken();
            writeTcpTokenFile(d_tcp_port);
            d_prefer_local_pipe = false;
            return true;
        }
        for (auto it = d_tcp_buffers.begin(); it != d_tcp_buffers.end(); ++it) {
            if (it.key()) {
                it.key()->disconnect(this);
                it.key()->disconnectFromHost();
                it.key()->deleteLater();
            }
        }
        d_tcp_buffers.clear();
        d_authed_tcp_sockets.clear();
        removeTcpTokenFile();
        d_tcp_server->close();
        delete d_tcp_server;
        d_tcp_server = nullptr;
        d_tcp_port = 0;
    }

    if (d_tcp_auth_token.isEmpty())
        d_tcp_auth_token = generateRandom256BitHexToken();

    d_tcp_server = new QTcpServer(this);
    connect(d_tcp_server, &QTcpServer::newConnection, this,
            &PythonAutomationServer::onNewTcpConnection);

    if (!d_tcp_server->listen(bindAddress, port)) {
        delete d_tcp_server;
        d_tcp_server = nullptr;
        d_tcp_port = 0;
        return false;
    }

    d_bind_address = bindAddress;
    d_tcp_port = d_tcp_server->serverPort();
    writeTcpTokenFile(d_tcp_port);
    d_prefer_local_pipe = false;
    return true;
}

quint16 PythonAutomationServer::tcpPort() const
{
    return (d_tcp_server && d_tcp_server->isListening()) ? d_tcp_server->serverPort() : d_tcp_port;
}

bool PythonAutomationServer::isTcpListening() const
{
    return d_tcp_server && d_tcp_server->isListening();
}

bool PythonAutomationServer::startLocalPipe(const QString &pipeName)
{
    QString targetName = pipeName.trimmed();
    if (targetName.isEmpty()) {
        targetName = QStringLiteral("torigraph_rpc_%1")
                             .arg(QCoreApplication::applicationPid());
    }

    if (d_local_server) {
        if (d_local_server->isListening() && d_local_pipe_name == targetName) {
            d_prefer_local_pipe = true;
            return true;
        }
        for (auto it = d_local_buffers.begin(); it != d_local_buffers.end(); ++it) {
            if (it.key()) {
                it.key()->disconnect(this);
                it.key()->disconnectFromServer();
                it.key()->deleteLater();
            }
        }
        d_local_buffers.clear();
        d_local_server->close();
        delete d_local_server;
        d_local_server = nullptr;
        d_local_pipe_name.clear();
    }

    QLocalServer::removeServer(targetName);
    d_local_server = new QLocalServer(this);
    d_local_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(d_local_server, &QLocalServer::newConnection, this,
            &PythonAutomationServer::onNewLocalConnection);

    if (!d_local_server->listen(targetName)) {
        delete d_local_server;
        d_local_server = nullptr;
        return false;
    }

    d_local_pipe_name = targetName;
    d_prefer_local_pipe = true;
    return true;
}

QString PythonAutomationServer::localPipeName() const
{
    return d_local_pipe_name;
}

bool PythonAutomationServer::isLocalPipeListening() const
{
    return d_local_server && d_local_server->isListening();
}

void PythonAutomationServer::stop()
{
    if (d_tcp_server) {
        for (auto it = d_tcp_buffers.begin(); it != d_tcp_buffers.end(); ++it) {
            if (it.key()) {
                it.key()->disconnect(this);
                it.key()->close();
                it.key()->deleteLater();
            }
        }
        d_tcp_buffers.clear();
        d_authed_tcp_sockets.clear();
        d_tcp_server->close();
        delete d_tcp_server;
        d_tcp_server = nullptr;
        d_tcp_port = 0;
    }
    removeTcpTokenFile();
    d_tcp_auth_token.clear();

    if (d_local_server) {
        for (auto it = d_local_buffers.begin(); it != d_local_buffers.end(); ++it) {
            if (it.key()) {
                it.key()->disconnect(this);
                it.key()->close();
                it.key()->deleteLater();
            }
        }
        d_local_buffers.clear();
        const QString savedName = d_local_pipe_name;
        d_local_server->close();
        delete d_local_server;
        d_local_server = nullptr;
        d_local_pipe_name.clear();
        if (!savedName.isEmpty())
            QLocalServer::removeServer(savedName);
    }
    d_prefer_local_pipe = false;
}

QString PythonAutomationServer::endpointString() const
{
    if (d_prefer_local_pipe && d_local_server && d_local_server->isListening()) {
        return QStringLiteral("pipe://%1").arg(d_local_pipe_name);
    }
    if (d_tcp_server && d_tcp_server->isListening()) {
        QString host = d_bind_address.toString();
        if (host.isEmpty() || d_bind_address == QHostAddress::Any
            || d_bind_address == QHostAddress::AnyIPv4) {
            host = QStringLiteral("127.0.0.1");
        }
        return QStringLiteral("tcp://%1:%2").arg(host).arg(d_tcp_server->serverPort());
    }
    if (d_local_server && d_local_server->isListening()) {
        return QStringLiteral("pipe://%1").arg(d_local_pipe_name);
    }
    return QString();
}

bool PythonAutomationServer::writeEndpointPortFile(const QString &filePath) const
{
    const QString ep = endpointString();
    if (filePath.trimmed().isEmpty() || ep.isEmpty())
        return false;
    QFileInfo fi(filePath);
    if (fi.dir().exists() == false)
        QDir().mkpath(fi.dir().absolutePath());

    QSaveFile sf(filePath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray payload = (ep + QStringLiteral("\n")).toUtf8();
    if (sf.write(payload) != payload.size())
        return false;
    return sf.commit();
}

void PythonAutomationServer::setCustomCommandHook(CustomCommandHook hook)
{
    d_custom_hook = std::move(hook);
}

void PythonAutomationServer::onNewTcpConnection()
{
    if (!d_tcp_server)
        return;
    while (QTcpSocket *sock = d_tcp_server->nextPendingConnection()) {
        d_tcp_buffers.insert(sock, QByteArray());
        connect(sock, &QTcpSocket::readyRead, this, &PythonAutomationServer::onTcpReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &PythonAutomationServer::onTcpDisconnected);
    }
}

void PythonAutomationServer::onTcpReadyRead()
{
    static constexpr int kMaxRpcFrameBytes = 64 * 1024 * 1024;
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock || !d_tcp_buffers.contains(sock))
        return;

    QByteArray &buf = d_tcp_buffers[sock];
    const QByteArray incoming = sock->readAll();
    if (qint64(buf.size()) + qint64(incoming.size()) > kMaxRpcFrameBytes) {
        buf.clear();
        QJsonObject errObj;
        errObj[QStringLiteral("code")] = -32600;
        errObj[QStringLiteral("message")] =
                QStringLiteral("Invalid Request: JSON-RPC frame exceeds 64 MiB limit.");
        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = QJsonValue::Null;
        resp[QStringLiteral("error")] = errObj;
        sock->write(QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n");
        sock->flush();
        sock->disconnectFromHost();
        return;
    }
    buf.append(incoming);

    while (true) {
        if (!d_tcp_buffers.contains(sock))
            break;
        int nlPos = buf.indexOf('\n');
        if (nlPos < 0)
            break;
        QByteArray line = buf.left(nlPos);
        buf.remove(0, nlPos + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.trimmed().isEmpty())
            continue;

        if (!d_authed_tcp_sockets.contains(sock)) {
            QJsonParseError authParseErr;
            const QJsonDocument authDoc = QJsonDocument::fromJson(line, &authParseErr);
            QJsonValue idVal = QJsonValue::Null;
            bool authOk = false;
            bool hasId = false;
            QString authMethod;

            if (authParseErr.error == QJsonParseError::NoError && authDoc.isObject()) {
                const QJsonObject reqObj = authDoc.object();
                hasId = reqObj.contains(QStringLiteral("id"));
                if (hasId) {
                    const QJsonValue rawId = reqObj.value(QStringLiteral("id"));
                    if (rawId.isNull() || rawId.isString()
                        || (rawId.isDouble() && !rawId.isBool()
                            && std::isfinite(rawId.toDouble()))) {
                        idVal = rawId.isString() ? QJsonValue(sanitizeUtf16Surrogates(rawId.toString()))
                                                 : rawId;
                    }
                }
                authMethod = reqObj.value(QStringLiteral("method")).toString().trimmed();
                if (authMethod == QStringLiteral("rpc.auth")
                    || authMethod == QStringLiteral("auth")) {
                    const QJsonObject paramsObj = reqObj.value(QStringLiteral("params")).toObject();
                    const QString providedToken =
                            paramsObj.value(QStringLiteral("token")).toString().trimmed();
                    if (constantTimeTokenEquals(providedToken, d_tcp_auth_token))
                        authOk = true;
                }
            }

            if (!authOk) {
                buf.clear();
                emit rpcRequestHandled(
                        authMethod.isEmpty() ? QStringLiteral("rpc.auth") : authMethod, false);
                QJsonObject errObj;
                errObj[QStringLiteral("code")] = -32001;
                errObj[QStringLiteral("message")] =
                        QStringLiteral("Unauthorized: rpc.auth required");
                QJsonObject errResp;
                errResp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
                errResp[QStringLiteral("id")] = idVal;
                errResp[QStringLiteral("error")] = errObj;
                sock->write(QJsonDocument(errResp).toJson(QJsonDocument::Compact) + "\n");
                sock->flush();
                sock->disconnectFromHost();
                return;
            }

            d_authed_tcp_sockets.insert(sock);
            emit rpcRequestHandled(authMethod, true);
            if (hasId && d_tcp_buffers.contains(sock)
                && sock->state() == QAbstractSocket::ConnectedState) {
                QJsonObject resObj;
                resObj[QStringLiteral("ok")] = true;
                resObj[QStringLiteral("authenticated")] = true;
                QJsonObject okResp;
                okResp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
                okResp[QStringLiteral("id")] = idVal;
                okResp[QStringLiteral("result")] = resObj;
                sock->write(QJsonDocument(okResp).toJson(QJsonDocument::Compact) + "\n");
                sock->flush();
            }
            continue;
        }

        QByteArray resp = handleRpcLine(line);
        if (!resp.isEmpty() && d_tcp_buffers.contains(sock)
            && sock->state() == QAbstractSocket::ConnectedState) {
            if (!resp.endsWith('\n'))
                resp.append('\n');
            sock->write(resp);
            sock->flush();
        }
    }
}

void PythonAutomationServer::onTcpDisconnected()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;
    d_tcp_buffers.remove(sock);
    d_authed_tcp_sockets.remove(sock);
    sock->deleteLater();
}

void PythonAutomationServer::onNewLocalConnection()
{
    if (!d_local_server)
        return;
    while (QLocalSocket *sock = d_local_server->nextPendingConnection()) {
        d_local_buffers.insert(sock, QByteArray());
        connect(sock, &QLocalSocket::readyRead, this, &PythonAutomationServer::onLocalReadyRead);
        connect(sock, &QLocalSocket::disconnected, this,
                &PythonAutomationServer::onLocalDisconnected);
    }
}

void PythonAutomationServer::onLocalReadyRead()
{
    static constexpr int kMaxRpcFrameBytes = 64 * 1024 * 1024;
    auto *sock = qobject_cast<QLocalSocket *>(sender());
    if (!sock || !d_local_buffers.contains(sock))
        return;

    QByteArray &buf = d_local_buffers[sock];
    const QByteArray incoming = sock->readAll();
    if (qint64(buf.size()) + qint64(incoming.size()) > kMaxRpcFrameBytes) {
        buf.clear();
        QJsonObject errObj;
        errObj[QStringLiteral("code")] = -32600;
        errObj[QStringLiteral("message")] =
                QStringLiteral("Invalid Request: JSON-RPC frame exceeds 64 MiB limit.");
        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = QJsonValue::Null;
        resp[QStringLiteral("error")] = errObj;
        sock->write(QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n");
        sock->flush();
        sock->disconnectFromServer();
        return;
    }
    buf.append(incoming);

    while (true) {
        if (!d_local_buffers.contains(sock))
            break;
        int nlPos = buf.indexOf('\n');
        if (nlPos < 0)
            break;
        QByteArray line = buf.left(nlPos);
        buf.remove(0, nlPos + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.trimmed().isEmpty())
            continue;

        QByteArray resp = handleRpcLine(line);
        if (!resp.isEmpty() && d_local_buffers.contains(sock)
            && sock->state() == QLocalSocket::ConnectedState) {
            if (!resp.endsWith('\n'))
                resp.append('\n');
            sock->write(resp);
            sock->flush();
        }
    }
}

void PythonAutomationServer::onLocalDisconnected()
{
    auto *sock = qobject_cast<QLocalSocket *>(sender());
    if (!sock)
        return;
    d_local_buffers.remove(sock);
    sock->deleteLater();
}

QByteArray PythonAutomationServer::handleRpcLine(const QByteArray &line)
{
    static constexpr int kMaxRpcFrameBytes = 64 * 1024 * 1024;
    if (line.size() > kMaxRpcFrameBytes) {
        QJsonObject errObj;
        errObj[QStringLiteral("code")] = -32600;
        errObj[QStringLiteral("message")] =
                QStringLiteral("Invalid Request: JSON-RPC frame exceeds 64 MiB limit.");
        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = QJsonValue::Null;
        resp[QStringLiteral("error")] = errObj;
        return QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n";
    }
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        QJsonObject errObj;
        errObj[QStringLiteral("code")] = -32700;
        errObj[QStringLiteral("message")] =
                QStringLiteral("Parse error: %1").arg(parseErr.errorString());

        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = QJsonValue::Null;
        resp[QStringLiteral("error")] = errObj;
        return QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n";
    }

    if (doc.isArray()) {
        const QJsonArray reqArr = doc.array();
        if (reqArr.isEmpty()) {
            QJsonObject errObj;
            errObj[QStringLiteral("code")] = -32600;
            errObj[QStringLiteral("message")] = QStringLiteral("Invalid Request: empty batch.");
            QJsonObject resp;
            resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            resp[QStringLiteral("id")] = QJsonValue::Null;
            resp[QStringLiteral("error")] = errObj;
            return QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n";
        }
        QJsonArray respArr;
        for (const QJsonValue &item : reqArr) {
            if (!item.isObject()) {
                QJsonObject errObj;
                errObj[QStringLiteral("code")] = -32600;
                errObj[QStringLiteral("message")] = QStringLiteral("Invalid Request");
                QJsonObject r;
                r[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
                r[QStringLiteral("id")] = QJsonValue::Null;
                r[QStringLiteral("error")] = errObj;
                respArr.append(r);
            } else {
                const QJsonObject r = handleRpcRequest(item.toObject());
                if (!r.isEmpty())
                    respArr.append(r);
            }
        }
        if (respArr.isEmpty())
            return QByteArray();
        return QJsonDocument(respArr).toJson(QJsonDocument::Compact) + "\n";
    }

    if (!doc.isObject()) {
        QJsonObject errObj;
        errObj[QStringLiteral("code")] = -32600;
        errObj[QStringLiteral("message")] = QStringLiteral("Invalid Request: root must be object.");
        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = QJsonValue::Null;
        resp[QStringLiteral("error")] = errObj;
        return QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n";
    }

    const QJsonObject resp = handleRpcRequest(doc.object());
    if (resp.isEmpty())
        return QByteArray();
    return QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n";
}

QJsonObject PythonAutomationServer::handleRpcRequest(const QJsonObject &req)
{
    const bool hasId = req.contains(QStringLiteral("id"));
    QJsonValue idVal = hasId ? req.value(QStringLiteral("id")) : QJsonValue::Null;
    bool validId = true;
    if (hasId) {
        if (idVal.isNull()) {
            validId = true;
        } else if (idVal.isString()) {
            idVal = sanitizeUtf16Surrogates(idVal.toString());
        } else if (idVal.isDouble() && !idVal.isBool() && std::isfinite(idVal.toDouble())) {
            validId = true;
        } else {
            validId = false;
            idVal = QJsonValue::Null;
        }
    }

    auto makeError = [&](int code, const QString &message,
                         const QJsonValue &errData = QJsonValue()) -> QJsonObject {
        QJsonObject errObj;
        errObj[QStringLiteral("code")] = code;
        errObj[QStringLiteral("message")] = message;
        if (!errData.isUndefined() && !errData.isNull())
            errObj[QStringLiteral("data")] = errData;

        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = idVal;
        resp[QStringLiteral("error")] = errObj;
        return resp;
    };

    if (!validId) {
        return makeError(-32600,
                         QStringLiteral("Invalid Request: 'id' must be string, number, or null."));
    }

    if (req.contains(QStringLiteral("jsonrpc"))) {
        const QJsonValue jv = req.value(QStringLiteral("jsonrpc"));
        if (!jv.isString() || jv.toString() != QStringLiteral("2.0")) {
            return makeError(-32600,
                             QStringLiteral("Invalid Request: 'jsonrpc' must be '2.0'."));
        }
    }

    if (!req.value(QStringLiteral("method")).isString()) {
        return makeError(-32600, QStringLiteral("Invalid Request: missing string 'method'."));
    }

    const QString method =
            sanitizeUtf16Surrogates(req.value(QStringLiteral("method")).toString()).trimmed();
    if (method.isEmpty()) {
        return makeError(-32600, QStringLiteral("Invalid Request: empty 'method'."));
    }

    QJsonObject params;
    if (req.contains(QStringLiteral("params")) && !req.value(QStringLiteral("params")).isNull()) {
        const QJsonValue pv = req.value(QStringLiteral("params"));
        if (pv.isObject()) {
            params = sanitizeJsonValueSurrogates(pv).toObject();
        } else {
            return makeError(-32602, QStringLiteral("Invalid params: params must be a JSON object."));
        }
    }

    try {
        const QJsonObject result = dispatchMethod(method, params);
        emit rpcRequestHandled(method, true);
        if (!hasId)
            return QJsonObject(); // JSON-RPC 2.0 notification

        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = idVal;
        resp[QStringLiteral("result")] = result;
        return resp;
    } catch (const RpcDispatchException &ex) {
        emit rpcRequestHandled(method, false);
        if (!hasId)
            return QJsonObject();
        return makeError(ex.code, QString::fromUtf8(ex.what()), ex.data);
    } catch (const std::exception &ex) {
        emit rpcRequestHandled(method, false);
        if (!hasId)
            return QJsonObject();
        return makeError(-32000, QString::fromUtf8(ex.what()));
    } catch (...) {
        emit rpcRequestHandled(method, false);
        if (!hasId)
            return QJsonObject();
        return makeError(-32603, QStringLiteral("Internal error during RPC execution."));
    }
}

QString PythonAutomationServer::resolvePythonExecutable(bool *explicitInvalidOut)
{
    if (explicitInvalidOut)
        *explicitInvalidOut = false;

    const QString envPy = qEnvironmentVariable("TORIGRAPH_PYTHON_EXECUTABLE").trimmed();
    if (!envPy.isEmpty()) {
        if (QFileInfo::exists(envPy))
            return envPy;
        const QString foundEnv = QStandardPaths::findExecutable(envPy);
        if (!foundEnv.isEmpty())
            return foundEnv;
        if (explicitInvalidOut)
            *explicitInvalidOut = true;
        return envPy;
    }

    // Search PATH ourselves so that Microsoft Store "WindowsApps" app-execution-alias stubs
    // (python.exe / python3.exe that only open the Store) are skipped. On Windows the "py"
    // launcher (always started with -3, see pythonLauncherPrefixArgs()) is preferred.
    auto findOnPathSkippingStubs = [](const QString &name) -> QString {
        const QStringList dirs =
                qEnvironmentVariable("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
        for (const QString &dir : dirs) {
            const QString hit = QStandardPaths::findExecutable(name, QStringList() << dir);
            if (hit.isEmpty())
                continue;
            if (hit.contains(QStringLiteral("WindowsApps"), Qt::CaseInsensitive)
                && name != QStringLiteral("py"))
                continue;
            return hit;
        }
        return QString();
    };

    QStringList order;
#ifdef Q_OS_WIN
    order << QStringLiteral("py") << QStringLiteral("python") << QStringLiteral("python3");
#else
    order << QStringLiteral("python3") << QStringLiteral("python") << QStringLiteral("py");
#endif
    for (const QString &name : order) {
        const QString found = findOnPathSkippingStubs(name);
        if (!found.isEmpty())
            return found;
    }
#ifdef Q_OS_WIN
    {
        const QString winDir = qEnvironmentVariable("WINDIR", QStringLiteral("C:\\Windows"));
        const QString pyLauncher = QDir(winDir).filePath(QStringLiteral("py.exe"));
        if (QFileInfo(pyLauncher).isExecutable())
            return pyLauncher;
    }
#endif
    return QStringLiteral("python");
}

QStringList PythonAutomationServer::pythonLauncherPrefixArgs(const QString &exe)
{
    const QString base = QFileInfo(exe).completeBaseName().toLower();
    if (base == QStringLiteral("py"))
        return QStringList() << QStringLiteral("-3");
    return QStringList();
}

QString PythonAutomationServer::resolvePythonExecutable()
{
    return resolvePythonExecutable(nullptr);
}

QString PythonAutomationServer::resolvePythonSdkDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    candidates << QDir(appDir).filePath(QStringLiteral("python"))
               << QDir(appDir).filePath(QStringLiteral("../python"))
               << QDir(appDir).filePath(QStringLiteral("../../python"))
               << QDir(appDir).filePath(QStringLiteral("../../../python"))
               << QDir::current().filePath(QStringLiteral("python"));

    for (const QString &cand : candidates) {
        QFileInfo fi(QDir(cand).filePath(QStringLiteral("torigraph/__init__.py")));
        if (fi.exists())
            return QFileInfo(cand).canonicalFilePath();
    }
    return QFileInfo(QDir(appDir).filePath(QStringLiteral("python"))).absoluteFilePath();
}

namespace {

QProcess *startPythonScriptProcessImpl(PythonAutomationServer *server, const QString &scriptPath,
                                       const QStringList &extraArgs, QObject *parent,
                                       const QString &workingDir,
                                       const std::function<void(QProcess *)> &beforeStart)
{
    if (!server)
        return nullptr;
    if (!server->isTcpListening() && !server->isLocalPipeListening()) {
        if (!server->startLocalPipe())
            server->startTcp(0, QHostAddress::LocalHost);
    }

    auto *proc = new QProcess(parent ? parent : static_cast<QObject *>(server));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TORIGRAPH_RPC_ENDPOINT"), server->endpointString());
    if (server->isTcpListening() && !server->tcpAuthToken().isEmpty())
        env.insert(QStringLiteral("TORIGRAPH_RPC_TOKEN"), server->tcpAuthToken());
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    env.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));

    const QString sdkDir = PythonAutomationServer::resolvePythonSdkDir();
    const QString sep =
#ifdef Q_OS_WIN
            QStringLiteral(";");
#else
            QStringLiteral(":");
#endif
    const QString existingPyPath = env.value(QStringLiteral("PYTHONPATH"));
    const QString newPyPath = existingPyPath.isEmpty()
            ? sdkDir
            : (sdkDir + sep + existingPyPath);
    env.insert(QStringLiteral("PYTHONPATH"), newPyPath);
    proc->setProcessEnvironment(env);

    // The working directory and signal connections must be set before start().
    const QFileInfo wdi(workingDir);
    if (!workingDir.isEmpty() && wdi.isDir()) {
        proc->setWorkingDirectory(wdi.absoluteFilePath());
    } else {
        const QFileInfo sfi(scriptPath);
        if (sfi.exists())
            proc->setWorkingDirectory(sfi.absolutePath());
    }

    if (beforeStart)
        beforeStart(proc);

    const QString pyExe = PythonAutomationServer::resolvePythonExecutable();
    QStringList args = PythonAutomationServer::pythonLauncherPrefixArgs(pyExe);
    args << QStringLiteral("-u") << scriptPath;
    args.append(extraArgs);
    proc->start(pyExe, args);
    return proc;
}

} // namespace

QProcess *PythonAutomationServer::startPythonScriptProcess(const QString &scriptPath,
                                                           const QStringList &extraArgs,
                                                           QObject *parent,
                                                           const QString &workingDir)
{
    return startPythonScriptProcessImpl(this, scriptPath, extraArgs, parent, workingDir, {});
}

QStringList PythonAutomationServer::supportedMethods()
{
    QStringList out;
    out.reserve(kApiCatalogCount);
    for (int i = 0; i < kApiCatalogCount; ++i)
        out.append(QString::fromUtf8(kApiCatalogSpecs[i].method));
    return out;
}

QJsonObject PythonAutomationServer::dispatchMethod(const QString &method,
                                                   const QJsonObject &params)
{
    // Re-entrancy guard: nested event loops (e.g. app.check_updates waiting on the network,
    // modal dialogs opened by a handler) can deliver another socket request while a handler is
    // still running. Executing it would interleave two handlers on the UI thread.
    if (d_rpc_dispatching) {
        throw RpcDispatchException(
                -32003,
                QStringLiteral("Server busy (re-entrant RPC): another RPC request is still being "
                               "processed. Retry after the previous call completes."));
    }
    struct ScopedDispatchFlag
    {
        bool &flag;
        explicit ScopedDispatchFlag(bool &f) : flag(f) { flag = true; }
        ~ScopedDispatchFlag() { flag = false; }
    } dispatchFlag(d_rpc_dispatching);

    struct ScopedCallerInteractiveTracker
    {
        bool &slot;
        bool prev;
        ScopedCallerInteractiveTracker(bool &s, bool cur) : slot(s), prev(s) { slot = cur; }
        ~ScopedCallerInteractiveTracker() { slot = prev; }
    } callerInteractiveGuard(d_caller_noninteractive,
                             ApplicationWindow::isNonInteractiveContext(d_app));

    // torigraph_noninteractive is application-global; it is only forced to true for the
    // duration of this call and handlers that spin a nested event loop must restore the
    // previous value around it (see RpcInteractiveWindow).
    struct ScopedNonInteractiveGuard
    {
        QCoreApplication *appInstance;
        QVariant prevProp;
        explicit ScopedNonInteractiveGuard(QCoreApplication *qa)
            : appInstance(qa),
              prevProp(qa ? qa->property("torigraph_noninteractive") : QVariant())
        {
            rpcPrevNonInteractiveSlot() = prevProp;
            if (appInstance)
                appInstance->setProperty("torigraph_noninteractive", true);
        }
        ~ScopedNonInteractiveGuard()
        {
            if (appInstance)
                appInstance->setProperty("torigraph_noninteractive", prevProp);
            rpcPrevNonInteractiveSlot() = QVariant();
        }
    } nonInteractiveGuard(QCoreApplication::instance());

    validateParamsAgainstCatalogSchema(method, params);

    if (d_custom_hook) {
        bool handled = false;
        const QJsonObject hookedRes = d_custom_hook(method, params, &handled);
        if (handled)
            return hookedRes;
    }

    auto findProjUndoIdx = [&]() -> int {
        if (!d_app)
            return -1;
        for (MyWidget *w : d_app->windowsList()) {
            if (!w)
                continue;
            if (w->inherits("Table")) {
                auto *t = static_cast<Table *>(w);
                if (t->d_future_table && t->d_future_table->undoStack())
                    return t->d_future_table->undoStack()->index();
            } else if (w->inherits("Matrix")) {
                auto *m = static_cast<Matrix *>(w);
                if (m->d_future_matrix && m->d_future_matrix->undoStack())
                    return m->d_future_matrix->undoStack()->index();
            }
        }
        return -1;
    };
    const int projIdxBefore = findProjUndoIdx();

    auto updateLastUndoTracking = [&](const QJsonObject &res) {
        if (method == QStringLiteral("app.undo") || method == QStringLiteral("app.redo"))
            return;
        if (method == QStringLiteral("app.new_project")
            || method == QStringLiteral("app.open_project")) {
            d_last_undo_domain.clear();
            d_last_undo_graph.clear();
            return;
        }
        const int projIdxAfter = findProjUndoIdx();
        const bool isReadOnlyGraph = (method == QStringLiteral("graph.get_info")
                                      || method == QStringLiteral("graph.inspect")
                                      || method == QStringLiteral("graph.export")
                                      || method == QStringLiteral("graph.copy_image")
                                      || method == QStringLiteral("graph.copy_ole")
                                      || method == QStringLiteral("graph.export_ole")
                                      || method == QStringLiteral("graph.to_python")
                                      || ((method == QStringLiteral("graph.auto_layout")
                                           || method == QStringLiteral("graph.fix_layout"))
                                          && params.value(QStringLiteral("dry_run")).toBool(false))
                                      || method == QStringLiteral("gadget.query"));
        if (!isReadOnlyGraph
            && (method.startsWith(QStringLiteral("plot."))
                || method.startsWith(QStringLiteral("graph."))
                || (res.contains(QStringLiteral("graph"))
                    && !res.value(QStringLiteral("graph")).toString().trimmed().isEmpty()))) {
            d_last_undo_domain = QStringLiteral("graph");
            const QString gName = res.value(QStringLiteral("graph")).toString().trimmed();
            if (!gName.isEmpty())
                d_last_undo_graph = gName;
            else if (!params.value(QStringLiteral("graph")).toString().trimmed().isEmpty())
                d_last_undo_graph = params.value(QStringLiteral("graph")).toString().trimmed();
        } else if ((projIdxBefore >= 0 && projIdxAfter != projIdxBefore)
                   || method.startsWith(QStringLiteral("table.set_"))
                   || method == QStringLiteral("table.add_column")
                   || method == QStringLiteral("table.remove_column")
                   || method == QStringLiteral("table.resize")
                   || method == QStringLiteral("table.paste_tsv")
                   || method == QStringLiteral("matrix.set_data")
                   || method == QStringLiteral("matrix.set_formula")
                   || method == QStringLiteral("matrix.transpose")
                   || method == QStringLiteral("matrix.invert")
                   || method == QStringLiteral("matrix.flip")) {
            d_last_undo_domain = QStringLiteral("project");
        }
    };

    QJsonObject result;
    if (dispatchCoreAndGraphMethod(this, d_app, d_focus_hud_fallback, d_workspace_mode_fallback,
                                   d_last_undo_domain, d_last_undo_graph, method, params,
                                   result)) {
        if (!result.contains(QStringLiteral("ok")))
            result[QStringLiteral("ok")] = true;
        updateLastUndoTracking(result);
        return result;
    }
    if (dispatchAnalysisAndScienceMethod(d_app, method, params, result)) {
        if (!result.contains(QStringLiteral("ok")))
            result[QStringLiteral("ok")] = true;
        updateLastUndoTracking(result);
        return result;
    }

    QJsonObject errData;
    errData[QStringLiteral("available")] = toJsonArray(supportedMethods());
    throw RpcDispatchException(-32601, QStringLiteral("Method not found: %1").arg(method), errData);
}

QString PythonAutomationServer::generatePythonScriptForWindow(ApplicationWindow *app,
                                                              MyWidget *window, bool standalone,
                                                              const QString &embedData)
{
    if (!app)
        return QString();
    MyWidget *target = window;
    if (!target) {
        if (QWidget *actW = app->d_workspace.activeSubWindow())
            target = qobject_cast<MyWidget *>(actW);
        if (!target) {
            for (MyWidget *w : app->windowsList()) {
                if (w && (w->inherits("MultiLayer") || w->inherits("Graph3D")))
                    target = w;
            }
        }
    }
    if (!target)
        return QString();
    return generatePythonScriptInternal(app, target, standalone, embedData, QString(), nullptr,
                                        nullptr);
}


// ============================================================================
// In-App Modeless Python Automation Console (PythonConsoleDialog)
// ============================================================================

QPointer<PythonConsoleDialog> PythonConsoleDialog::s_instance;

PythonConsoleDialog::PythonConsoleDialog(ApplicationWindow *app, PythonAutomationServer *server,
                                         QWidget *parent)
    : QDialog(parent ? parent : reinterpret_cast<QWidget *>(app)), d_app(app), d_server(server)
{
    setObjectName(QStringLiteral("PythonConsoleDialog"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowModality(Qt::NonModal);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(840, 640);

    buildUi();
    retranslateUi();
    refreshEndpointDisplay();
}

PythonConsoleDialog::~PythonConsoleDialog()
{
    stopRunningScript();
    cleanupTempScript();
    delete d_stdout_decoder;
    d_stdout_decoder = nullptr;
    delete d_stderr_decoder;
    d_stderr_decoder = nullptr;
}

void PythonConsoleDialog::resetStreamDecoders()
{
    delete d_stdout_decoder;
    d_stdout_decoder = nullptr;
    delete d_stderr_decoder;
    d_stderr_decoder = nullptr;
    if (QTextCodec *utf8 = QTextCodec::codecForName("UTF-8")) {
        d_stdout_decoder = utf8->makeDecoder(QTextCodec::IgnoreHeader);
        d_stderr_decoder = utf8->makeDecoder(QTextCodec::IgnoreHeader);
    }
}

void PythonConsoleDialog::closeEvent(QCloseEvent *event)
{
    stopRunningScript();
    QDialog::closeEvent(event);
}

void PythonConsoleDialog::reject()
{
    stopRunningScript();
    QDialog::reject();
}

PythonConsoleDialog *PythonConsoleDialog::showDialog(ApplicationWindow *app,
                                                     PythonAutomationServer *server)
{
    if (server && !server->isTcpListening() && !server->isLocalPipeListening()) {
        if (!server->startLocalPipe())
            server->startTcp(0, QHostAddress::LocalHost);
    }

    if (!s_instance) {
        s_instance = new PythonConsoleDialog(app, server, reinterpret_cast<QWidget *>(app));
    } else {
        s_instance->d_app = app;
        s_instance->d_server = server;
        s_instance->retranslateUi();
        s_instance->refreshEndpointDisplay();
    }

    s_instance->show();
    s_instance->raise();
    s_instance->activateWindow();
    return s_instance.data();
}

void PythonConsoleDialog::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(8);

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);

    // Top row: Endpoint + Example Selector
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    d_endpoint_label = new QLabel(this);
    d_endpoint_edit = new QLineEdit(this);
    d_endpoint_edit->setReadOnly(true);
    d_endpoint_edit->setFont(monoFont);
    d_endpoint_edit->setMinimumWidth(210);

    d_example_label = new QLabel(this);
    d_example_combo = new QComboBox(this);
    for (int i = 0; i < 7; ++i)
        d_example_combo->addItem(QString());
    d_example_combo->setMinimumWidth(260);

    topRow->addWidget(d_endpoint_label);
    topRow->addWidget(d_endpoint_edit, 1);
    topRow->addSpacing(8);
    topRow->addWidget(d_example_label);
    topRow->addWidget(d_example_combo, 1);
    rootLayout->addLayout(topRow);

    // Button bar
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);

    d_run_btn = new QPushButton(this);
    d_run_btn->setDefault(true);
    d_stop_btn = new QPushButton(this);
    d_stop_btn->setEnabled(false);
    d_open_btn = new QPushButton(this);
    d_save_btn = new QPushButton(this);
    d_clear_btn = new QPushButton(this);
    d_close_btn = new QPushButton(this);

    btnRow->addWidget(d_run_btn);
    btnRow->addWidget(d_stop_btn);
    btnRow->addSpacing(8);
    btnRow->addWidget(d_open_btn);
    btnRow->addWidget(d_save_btn);
    btnRow->addWidget(d_clear_btn);
    btnRow->addStretch(1);
    btnRow->addWidget(d_close_btn);
    rootLayout->addLayout(btnRow);

    // Vertical splitter: Editor (top) + Output (bottom)
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);

    auto *editorPane = new QWidget(splitter);
    auto *editorLayout = new QVBoxLayout(editorPane);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(4);
    d_editor_label = new QLabel(editorPane);
    d_editor = new QPlainTextEdit(editorPane);
    d_editor->setFont(monoFont);
    d_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    d_editor->setTabStopDistance(32.0);
#endif
    d_editor->setPlainText(exampleScriptCode(1));
    editorLayout->addWidget(d_editor_label);
    editorLayout->addWidget(d_editor, 1);

    auto *outputPane = new QWidget(splitter);
    auto *outputLayout = new QVBoxLayout(outputPane);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(4);
    d_output_label = new QLabel(outputPane);
    d_output = new QPlainTextEdit(outputPane);
    d_output->setReadOnly(true);
    d_output->setFont(monoFont);
    outputLayout->addWidget(d_output_label);
    outputLayout->addWidget(d_output, 1);

    splitter->addWidget(editorPane);
    splitter->addWidget(outputPane);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    rootLayout->addWidget(splitter, 1);

    // Connections
    connect(d_run_btn, &QPushButton::clicked, this, &PythonConsoleDialog::runCurrentScript);
    connect(d_stop_btn, &QPushButton::clicked, this, &PythonConsoleDialog::stopRunningScript);
    connect(d_open_btn, &QPushButton::clicked, this, &PythonConsoleDialog::openScriptDialog);
    connect(d_save_btn, &QPushButton::clicked, this, &PythonConsoleDialog::saveScriptDialog);
    connect(d_clear_btn, &QPushButton::clicked, this, &PythonConsoleDialog::clearOutput);
    connect(d_close_btn, &QPushButton::clicked, this, &QDialog::close);
    connect(d_example_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PythonConsoleDialog::onExampleSelected);

    auto *scReturn = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return), this);
    connect(scReturn, &QShortcut::activated, this, &PythonConsoleDialog::runCurrentScript);
    auto *scEnter = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Enter), this);
    connect(scEnter, &QShortcut::activated, this, &PythonConsoleDialog::runCurrentScript);
}

void PythonConsoleDialog::retranslateUi()
{
    setWindowTitle(tr("Python Automation Console"));

    if (d_endpoint_label)
        d_endpoint_label->setText(tr("RPC Endpoint:"));
    if (d_endpoint_edit) {
        d_endpoint_edit->setToolTip(
                tr("Active loopback JSON-RPC 2.0 endpoint used by torigraph.connect()"));
    }
    if (d_example_label)
        d_example_label->setText(tr("Examples:"));

    if (d_example_combo && d_example_combo->count() >= 7) {
        const bool oldBlock = d_example_combo->blockSignals(true);
        d_example_combo->setItemText(0, tr("Select Example Workflow..."));
        d_example_combo->setItemText(1, tr("1. ISIJ 85mm Stress-Strain Plot"));
        d_example_combo->setItemText(2, tr("2. Pseudo-Voigt TDS Peak Deconvolution"));
        d_example_combo->setItemText(3, tr("3. EIS Randles Nyquist Fit"));
        d_example_combo->setItemText(4, tr("4. Weibull 3P Reliability & ANOVA"));
        d_example_combo->setItemText(5, tr("5. 2D Matrix Filter & Heatmap"));
        d_example_combo->setItemText(6, tr("6. Lab Slide (18pt) & PowerPoint Export"));
        d_example_combo->blockSignals(oldBlock);
    }

    if (d_run_btn) {
        d_run_btn->setText(tr("Run (Ctrl+Enter)"));
        d_run_btn->setToolTip(tr("Execute the Python script against this ToriGraph instance"));
    }
    if (d_stop_btn) {
        d_stop_btn->setText(tr("Stop"));
        d_stop_btn->setToolTip(tr("Terminate the currently running Python script"));
    }
    if (d_open_btn)
        d_open_btn->setText(tr("Open .py..."));
    if (d_save_btn)
        d_save_btn->setText(tr("Save .py..."));
    if (d_clear_btn)
        d_clear_btn->setText(tr("Clear Output"));
    if (d_close_btn)
        d_close_btn->setText(tr("Close"));

    if (d_editor_label)
        d_editor_label->setText(tr("Python Script (import torigraph as tg):"));
    if (d_output_label)
        d_output_label->setText(tr("Output (stdout / stderr):"));

    if (d_editor) {
        d_editor->setPlaceholderText(tr(
                "Write Python code using 'import torigraph as tg' and press Ctrl+Enter to run..."));
    }
    if (d_output)
        d_output->setPlaceholderText(tr("Script output will appear here."));

    refreshEndpointDisplay();
}

void PythonConsoleDialog::changeEvent(QEvent *event)
{
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void PythonConsoleDialog::refreshEndpointDisplay()
{
    if (!d_endpoint_edit)
        return;
    if (d_server && !d_server->endpointString().isEmpty())
        d_endpoint_edit->setText(d_server->endpointString());
    else
        d_endpoint_edit->setText(tr("Not listening"));
}

bool PythonConsoleDialog::loadScriptFile(const QString &pyFilePath)
{
    QFile f(pyFilePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendOutput(tr("[Error] Failed to read script file: %1\n").arg(pyFilePath), true);
        return false;
    }
    const QString code = QString::fromUtf8(f.readAll());
    f.close();
    d_current_file_path = QFileInfo(pyFilePath).absoluteFilePath();
    if (d_editor)
        d_editor->setPlainText(code);
    return true;
}

bool PythonConsoleDialog::saveScriptFile(const QString &pyFilePath)
{
    QSaveFile f(pyFilePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendOutput(tr("[Error] Failed to save script file: %1\n").arg(pyFilePath), true);
        return false;
    }
    const QByteArray bytes = d_editor ? d_editor->toPlainText().toUtf8() : QByteArray();
    f.write(bytes);
    if (!f.commit()) {
        appendOutput(tr("[Error] Failed to save script file: %1\n").arg(pyFilePath), true);
        return false;
    }
    d_current_file_path = QFileInfo(pyFilePath).absoluteFilePath();
    appendOutput(tr("[Saved] %1\n").arg(d_current_file_path), false);
    return true;
}

void PythonConsoleDialog::loadAndRunScriptFile(const QString &pyFilePath)
{
    if (loadScriptFile(pyFilePath))
        runCurrentScript();
}

void PythonConsoleDialog::openScriptDialog()
{
    const QString startDir = d_current_file_path.isEmpty()
            ? QDir::homePath()
            : QFileInfo(d_current_file_path).absolutePath();
    const QString chosen = QFileDialog::getOpenFileName(
            this, tr("Open Python Automation Script"), startDir,
            tr("Python Scripts (*.py);;All Files (*)"));
    if (!chosen.isEmpty())
        loadScriptFile(chosen);
}

void PythonConsoleDialog::saveScriptDialog()
{
    const QString startPath = d_current_file_path.isEmpty()
            ? QDir(QDir::homePath()).filePath(QStringLiteral("torigraph_script.py"))
            : d_current_file_path;
    const QString chosen = QFileDialog::getSaveFileName(
            this, tr("Save Python Automation Script"), startPath,
            tr("Python Scripts (*.py);;All Files (*)"));
    if (!chosen.isEmpty())
        saveScriptFile(chosen);
}

void PythonConsoleDialog::clearOutput()
{
    if (d_output)
        d_output->clear();
}

void PythonConsoleDialog::runCurrentScript()
{
    if (d_process && d_process->state() != QProcess::NotRunning) {
        appendOutput(tr("[Info] A Python script is already running.\n"), false);
        return;
    }
    if (!d_server) {
        appendOutput(tr("[Error] PythonAutomationServer is not available.\n"), true);
        return;
    }
    if (!d_server->isTcpListening() && !d_server->isLocalPipeListening()) {
        if (!d_server->startLocalPipe() && !d_server->startTcp(0, QHostAddress::LocalHost)) {
            appendOutput(tr("[Error] Failed to start loopback RPC listener.\n"), true);
            return;
        }
    }
    refreshEndpointDisplay();
    cleanupTempScript();

    bool invalidEnvPy = false;
    const QString pyExe = PythonAutomationServer::resolvePythonExecutable(&invalidEnvPy);
    if (invalidEnvPy) {
        appendOutput(
                tr("[Process Error] Failed to start Python interpreter (%1). Set TORIGRAPH_PYTHON_EXECUTABLE if needed.\n")
                        .arg(pyExe),
                true);
        return;
    }

    QTemporaryFile tmp(QDir::temp().filePath(QStringLiteral("torigraph_console_XXXXXX.py")));
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        appendOutput(tr("[Error] Failed to create temporary Python script file.\n"), true);
        return;
    }
    d_temp_script_path = tmp.fileName();
    tmp.write(d_editor ? d_editor->toPlainText().toUtf8() : QByteArray());
    tmp.close();

    appendOutput(tr("[Running] %1 (Endpoint: %2)\n").arg(pyExe, d_server->endpointString()), false);

    if (d_run_btn)
        d_run_btn->setEnabled(false);
    if (d_stop_btn)
        d_stop_btn->setEnabled(true);

    if (d_process) {
        d_process->disconnect(this);
        d_process->deleteLater();
        d_process = nullptr;
    }

    resetStreamDecoders();
    // Working directory and signal connections must be set before QProcess::start();
    // unsaved scripts fall back to the application's working directory rather than %TEMP%.
    QString scriptDir;
    if (!d_current_file_path.isEmpty() && QFileInfo::exists(d_current_file_path))
        scriptDir = QFileInfo(d_current_file_path).absolutePath();
    else if (d_app && !d_app->workingDir.isEmpty() && QFileInfo(d_app->workingDir).isDir())
        scriptDir = QFileInfo(d_app->workingDir).absoluteFilePath();

    startPythonScriptProcessImpl(
            d_server.data(), d_temp_script_path, QStringList(), this, scriptDir,
            [this](QProcess *proc) {
                d_process = proc;
                connect(proc, &QProcess::readyReadStandardOutput, this,
                        &PythonConsoleDialog::onProcessReadyReadStdout);
                connect(proc, &QProcess::readyReadStandardError, this,
                        &PythonConsoleDialog::onProcessReadyReadStderr);
                connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                        [this](int code, QProcess::ExitStatus st) {
                            onProcessFinished(code, int(st));
                        });
                connect(proc, &QProcess::errorOccurred, this,
                        [this](QProcess::ProcessError err) { onProcessErrorOccurred(int(err)); });
            });
}

void PythonConsoleDialog::stopRunningScript()
{
    if (!d_process || d_process->state() == QProcess::NotRunning)
        return;

    d_process->disconnect(this);
    d_process->kill();
    d_process->waitForFinished(1500);
    d_process->deleteLater();
    d_process = nullptr;

    cleanupTempScript();
    if (d_run_btn)
        d_run_btn->setEnabled(true);
    if (d_stop_btn)
        d_stop_btn->setEnabled(false);
    appendOutput(tr("[Stopped] Python script terminated by user.\n"), true);
}

void PythonConsoleDialog::onExampleSelected(int index)
{
    if (index <= 0)
        return;
    const QString code = exampleScriptCode(index);
    if (!code.isEmpty() && d_editor)
        d_editor->setPlainText(code);
}

void PythonConsoleDialog::onProcessReadyReadStdout()
{
    if (!d_process)
        return;
    const QByteArray raw = d_process->readAllStandardOutput();
    if (!raw.isEmpty()) {
        const QString decoded = d_stdout_decoder ? d_stdout_decoder->toUnicode(raw)
                                                 : QString::fromUtf8(raw);
        appendOutput(decoded, false);
    }
}

void PythonConsoleDialog::onProcessReadyReadStderr()
{
    if (!d_process)
        return;
    const QByteArray raw = d_process->readAllStandardError();
    if (!raw.isEmpty()) {
        const QString decoded = d_stderr_decoder ? d_stderr_decoder->toUnicode(raw)
                                                 : QString::fromUtf8(raw);
        appendOutput(decoded, true);
    }
}

void PythonConsoleDialog::onProcessFinished(int exitCode, int exitStatus)
{
    onProcessReadyReadStdout();
    onProcessReadyReadStderr();
    cleanupTempScript();

    if (d_run_btn)
        d_run_btn->setEnabled(true);
    if (d_stop_btn)
        d_stop_btn->setEnabled(false);

    if (exitStatus == int(QProcess::NormalExit)) {
        appendOutput(tr("[Finished] Exit code: %1\n").arg(exitCode), exitCode != 0);
    } else {
        appendOutput(tr("[Crashed] Python process terminated abnormally (code %1).\n")
                             .arg(exitCode),
                     true);
    }
}

void PythonConsoleDialog::onProcessErrorOccurred(int processError)
{
    if (processError == int(QProcess::FailedToStart)) {
        cleanupTempScript();
        if (d_run_btn)
            d_run_btn->setEnabled(true);
        if (d_stop_btn)
            d_stop_btn->setEnabled(false);
        appendOutput(
                tr("[Process Error] Failed to start Python interpreter (%1). Set TORIGRAPH_PYTHON_EXECUTABLE if needed.\n")
                        .arg(PythonAutomationServer::resolvePythonExecutable()),
                true);
    } else {
        appendOutput(tr("[Process Error] Python process error (code %1).\n").arg(processError),
                     true);
    }
}

void PythonConsoleDialog::appendOutput(const QString &text, bool isError)
{
    if (!d_output || text.isEmpty())
        return;
    d_output->moveCursor(QTextCursor::End);
    QTextCursor cur = d_output->textCursor();
    QTextCharFormat fmt;
    if (isError) {
        // Pick a red that stays readable on both light and dark editor backgrounds.
        const bool darkBase = d_output->palette().base().color().lightness() < 128;
        fmt.setForeground(darkBase ? QColor(255, 120, 120) : QColor(200, 30, 30));
    } else {
        // Explicitly reset so a preceding error run does not bleed its colour.
        fmt.setForeground(d_output->palette().text());
    }
    cur.insertText(text, fmt);
    d_output->setTextCursor(cur);
    d_output->moveCursor(QTextCursor::End);
    if (QScrollBar *sb = d_output->verticalScrollBar())
        sb->setValue(sb->maximum());
}

void PythonConsoleDialog::cleanupTempScript()
{
    if (!d_temp_script_path.isEmpty()) {
        QFile::remove(d_temp_script_path);
        d_temp_script_path.clear();
    }
}

QString PythonConsoleDialog::exampleScriptCode(int index) const
{
    switch (index) {
    case 1:
        return QStringLiteral(
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "strain = [0.0005 * i for i in range(25)]\n"
                "stress = [205000.0 * e if e <= 0.002 else 410.0 + 18000.0 * (e - 0.002) for e in strain]\n\n"
                "tbl = app.create_table(rows=len(strain), cols=2, name='TensileSS')\n"
                "tbl.set_data_dict(\n"
                "    {'Strain': strain, 'Stress': stress},\n"
                "    units={'Strain': '-', 'Stress': 'MPa'},\n"
                "    long_names={'Strain': 'Engineering Strain', 'Stress': 'Engineering Stress'},\n"
                ")\n\n"
                "g = app.plot_xy(tbl.name, x_col='Strain', y_cols=['Stress'], style='line_symbol')\n"
                "g.apply_style_preset('isij_85mm')\n"
                "g.apply_frame_preset('box_inward')\n"
                "elastic = app.fit_linear(graph=g.name, x_min=0.0, x_max=0.002)\n"
                "print(f\"Young's Modulus E = {elastic['slope']:.1f} MPa (R2 = {elastic['r2']:.5f})\")\n");

    case 2:
        return QStringLiteral(
                "import math\n"
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "temp = [300.0 + 2.5 * i for i in range(161)]\n"
                "flux = [\n"
                "    0.04 + 0.0001 * (t - 300.0)\n"
                "    + 1.8 * math.exp(-((t - 420.0) / 24.0) ** 2)\n"
                "    + 1.1 / (1.0 + ((t - 535.0) / 20.0) ** 2)\n"
                "    for t in temp\n"
                "]\n\n"
                "tbl = app.create_table(rows=len(temp), cols=2, name='TDS_Data')\n"
                "tbl.set_data_dict({'Temp_K': temp, 'Desorption': flux}, units={'Temp_K': 'K', 'Desorption': 'arb. u.'})\n"
                "g = app.plot_xy(tbl.name, x_col='Temp_K', y_cols=['Desorption'], style='scatter')\n"
                "res = app.analyze_peaks(\n"
                "    graph=g.name, profile='PseudoVoigt', peak_count=2, baseline='linear', plot_components=True\n"
                ")\n"
                "for idx, pk in enumerate(res.get('peaks', []), 1):\n"
                "    print(f\"Peak {idx}: T_c = {pk['center']:.2f} K, Area = {pk['area']:.2f}, FWHM = {pk['fwhm']:.2f} K\")\n");

    case 3:
        return QStringLiteral(
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "rs, rct, cdl = 25.0, 480.0, 2.2e-5\n"
                "freqs = [10.0 ** (-1.0 + 0.25 * i) for i in range(25)]\n"
                "z_re, z_im = [], []\n"
                "for f in freqs:\n"
                "    w = 2.0 * 3.141592653589793 * f\n"
                "    denom = 1.0 + (w * rct * cdl) ** 2\n"
                "    z_re.append(rs + rct / denom)\n"
                "    z_im.append(-(w * (rct ** 2) * cdl) / denom)\n\n"
                "tbl = app.create_table(rows=len(freqs), cols=3, name='EIS_Randles')\n"
                "tbl.set_data_dict({'Freq': freqs, 'Z_re': z_re, 'Z_im': z_im})\n"
                "fit = app.eis_fit(table=tbl.name, freq_col='Freq', z_re_col='Z_re', z_im_col='Z_im', model='Randles')\n"
                "print('EIS Randles Fit:', fit.get('params', fit))\n");

    case 4:
        return QStringLiteral(
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "batch_a = [410.0, 435.0, 448.0, 462.0, 475.0, 489.0, 502.0, 518.0, 535.0, 558.0]\n"
                "batch_b = [465.0, 488.0, 504.0, 519.0, 532.0, 546.0, 561.0, 578.0, 595.0, 615.0]\n\n"
                "tbl = app.create_table(rows=len(batch_a), cols=2, name='CeramicsStrength')\n"
                "tbl.set_data_dict({'Batch_A': batch_a, 'Batch_B': batch_b}, units={'Batch_A': 'MPa', 'Batch_B': 'MPa'})\n"
                "anova = app.anova_oneway(table=tbl.name, cols=['Batch_A', 'Batch_B'])\n"
                "wb = app.weibull(table=tbl.name, col='Batch_A', three_param=True, plot=True)\n"
                "print(f\"ANOVA p = {anova['p_value']:.4e}, Weibull beta = {wb['beta']:.2f}, eta = {wb['eta']:.1f} MPa\")\n");

    case 5:
        return QStringLiteral(
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "m = app.create_matrix(rows=64, cols=64, name='BeamProfile', x_min=-2.0, x_max=2.0, y_min=-2.0, y_max=2.0)\n"
                "m.set_formula('exp(-(x*x + y*y)) + 0.55*exp(-((x-0.85)*(x-0.85) + (y+0.65)*(y+0.65))/0.18)')\n"
                "m.filter2d('gaussian', sigma=1.1, in_place=True)\n"
                "g = app.plot_matrix(m.name, style='colormap')\n"
                "g.set_colorbar(palette='viridis', show_contour_lines=True, levels=8, title='Intensity (a.u.)')\n"
                "pks = m.analyze_2d_peaks(prominence=0.15, max_peaks=4)\n"
                "print(f\"Detected {pks['peak_count']} 2D peaks on {g.name}\")\n");

    case 6:
        return QStringLiteral(
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "x = [0.0, 0.2, 0.4, 0.6, 0.8, 1.0]\n"
                "y = [0.0, 42.0, 86.0, 128.0, 165.0, 198.0]\n"
                "tbl = app.create_table({'Strain': x, 'Stress': y}, units={'Strain': '%', 'Stress': 'MPa'}, name='LabSlideData')\n"
                "g = app.plot_xy(tbl.name, x_col='Strain', y_cols=['Stress'], style='line_symbol')\n"
                "g.lab_slide_style()  # 83.3 mm / 12.9 pt -> placed at 140% (18 pt) in PPTX\n"
                "res = app.to_pptx('lab_slide_demo.pptx', [g], per_slide=2, overwrite=True,\n"
                "                  titles=['Al-Zn-Mg Tensile Response'])\n"
                "print('Exported PPTX:', res['path'], 'effective_scales:', res.get('effective_scales'))\n");

    default:
        return QString();
    }
}

QString PythonAutomationServer::matchPaletteName(const QString &raw, const QStringList &palettes,
                                                 QString *normalizedKey)
{
    auto normKey = [](QString s) -> QString {
        s = s.trimmed().toLower();
        s.remove(QLatin1Char('-'));
        s.remove(QLatin1Char('_'));
        s.remove(QLatin1Char(' '));
        return s;
    };
    QString wantNorm = normKey(raw);
    if (wantNorm == QStringLiteral("okabeito") || wantNorm == QStringLiteral("okabe"))
        wantNorm = QStringLiteral("okabeito");
    else if (wantNorm == QStringLiteral("npg") || wantNorm == QStringLiteral("naturenpg"))
        wantNorm = QStringLiteral("nature");
    else if (wantNorm == QStringLiteral("aaas") || wantNorm == QStringLiteral("scienceaaas"))
        wantNorm = QStringLiteral("science");
    else if (wantNorm == QStringLiteral("isij") || wantNorm == QStringLiteral("isijsteel")
             || wantNorm == QStringLiteral("default") || wantNorm == QStringLiteral("origin"))
        wantNorm = QStringLiteral("isij");
    else if (wantNorm == QStringLiteral("tolbright") || wantNorm == QStringLiteral("paultolbright"))
        wantNorm = QStringLiteral("tolbright");
    else if (wantNorm == QStringLiteral("tolmuted") || wantNorm == QStringLiteral("paultolmuted"))
        wantNorm = QStringLiteral("tolmuted");
    else if (wantNorm == QStringLiteral("grayscale") || wantNorm == QStringLiteral("grey")
             || wantNorm == QStringLiteral("gray") || wantNorm == QStringLiteral("monochrome"))
        wantNorm = QStringLiteral("monochrome");
    if (normalizedKey)
        *normalizedKey = wantNorm;
    if (wantNorm.isEmpty())
        return QString();

    // Strongest match first, so a short key such as "orig" is not caught by a palette that
    // merely contains it ("ToriGraph Standard"): exact name, alias, prefix, word prefix,
    // and only then a substring.
    for (const QString &cand : palettes) {
        if (cand.compare(raw.trimmed(), Qt::CaseInsensitive) == 0 || normKey(cand) == wantNorm)
            return cand;
    }
    const QString canon = Graph::canonicalPaletteName(raw);
    for (const QString &cand : palettes) {
        if (cand == canon)
            return cand;
    }
    for (const QString &cand : palettes) {
        if (normKey(cand).startsWith(wantNorm))
            return cand;
    }
    static const QRegularExpression kWordSplit(QStringLiteral("[^A-Za-z0-9]+"));
    for (const QString &cand : palettes) {
        // word prefix: "tab" -> "Matplotlib (tab10)", "colorblind" -> "Okabe-Ito (Colorblind ...)"
        const QStringList words = cand.split(kWordSplit, Qt::SkipEmptyParts);
        for (int w = 0; w < words.size(); ++w) {
            QString tail;
            for (int k = w; k < words.size(); ++k)
                tail += words.at(k).toLower();
            if (tail.startsWith(wantNorm))
                return cand;
        }
    }
    if (wantNorm.size() >= 4) {
        for (const QString &cand : palettes) {
            if (normKey(cand).contains(wantNorm))
                return cand;
        }
    }
    return QString();
}
