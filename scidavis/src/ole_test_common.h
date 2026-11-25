#ifndef TORIGRAPH_OLE_TEST_COMMON_H
#define TORIGRAPH_OLE_TEST_COMMON_H

#include "ApplicationWindow.h"
#include "MultiLayer.h"
#include "Graph.h"
#include "Plot.h"
#include "Legend.h"
#include "Table.h"
#include "MultiPeakFit.h"
#include "NonLinearFit.h"
#include "ExponentialFit.h"
#include "fit_gsl.h"
#include "BoxCurve.h"
#include "PlotCurve.h"
#include "EmfPaintDevice.h"
#include "OleServer.h"
#include "core/column/Column.h"
#include "OriginGadgets.h"
#include "DataPickerTool.h"
#include "PolynomialFit.h"
#include "CurveMathTools.h"
#include "AdvancedSignalProc.h"
#include "SmoothFilter.h"
#include "Differentiation.h"
#include "GraphStatsAndSurfaceFit.h"
#include "RecalcEngine.h"
#include "Matrix.h"
#include "FunctionCurve.h"
#include "I18nJapanese.h"
#include "ConfigDialog.h"
#include "AxesDialog.h"
#include "PlotDialog.h"
#include "LayerDialog.h"
#include "FitDialog.h"
#include "GraphMiniToolbar.h"
#include "PiecewiseAndRobustFit.h"
#include "GraphDigitizer.h"
#include "ImageExportDialog.h"
#include "SelectionMoveResizer.h"
#include "AssociationsDialog.h"
#include "Spectrogram.h"
#include "ArrowMarker.h"
#include "ImageMarker.h"
#include "CurveRangeDialog.h"
#include "ExtraPlotTypes.h"
#include "QwtErrorPlotCurve.h"
#include <gsl/gsl_cdf.h>
#include <qwt_scale_engine.h>

#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_div.h>
#include <qwt_scale_map.h>
#include <qwt_scale_widget.h>
#include <qwt_text_label.h>
#include <QApplication>
#include <QJsonDocument>
#include <QScrollArea>
#include <QScrollBar>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QPointer>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUndoStack>
#include "core/Project.h"
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include <QKeyEvent>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QWheelEvent>
#include <cmath>
#include <functional>
#include <iostream>
#include <wrl/client.h>
#include "SettingsTransfer.h"
#include "PptxExport.h"
#include "ZipReader.h"
#include "ZipWriter.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QRadioButton>
#include <QXmlStreamReader>
#include <QAction>
#include <QSettings>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using Microsoft::WRL::ComPtr;

#include "CrashHandler.h"
#include "Graph3D.h"
#include "MyParser.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "ModernRibbonAndHud.h"
#include "AutoUpdater.h"
#include "PythonAutomationServer.h"
#include "Grid.h"
#include "CurvesDialog.h"
#include "Folder.h"
#include "Note.h"
#include "GraphInspectorDock.h"
#include "CanvasInlineTextEditor.h"
#include "ErrDialog.h"
#include "ScalePicker.h"
#include "ScaleDraw.h"
#include "ScriptEdit.h"
#include "QwtHistogram.h"
#include "ImportOPJU.h"
#include "ImportASCIIDialog.h"
#include "PlotStyleLimits.h"
#include "UxNavigation.h"
#include "XlsxImport.h"
#include "Workbook.h"
#include "TextDialog.h"
#include "ColorButton.h"
#include "future/table/AsciiTableImportFilter.h"
#include "future/table/future_Table.h"
#include "future/core/datatypes/DateTime2StringFilter.h"
#include "future/matrix/future_Matrix.h"

#include <QAbstractButton>
#include <QCompleter>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFocusEvent>
#include <QGroupBox>
#include <QJsonArray>
#include <QListWidgetItem>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPlainTextEdit>
#include <QProcess>
#include <QSet>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextCodec>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVBoxLayout>
#include <qwt_layout_metrics.h>
#include <qwt_painter.h>
#include <qwt_symbol.h>
#ifdef Q_OS_WIN
#include <objidl.h>
#include <gdiplus.h>
#endif

struct EmfRecordStats {
    int totalRecords = 0;
    int vectorPathOrLineRecords = 0;
    int textRecords = 0;
    int bitmapRecords = 0;
};

inline int CALLBACK enumEmfProc(HDC, HANDLETABLE *, const ENHMETARECORD *rec, int, LPARAM data)
{
    auto *stats = reinterpret_cast<EmfRecordStats *>(data);
    if (!stats || !rec)
        return 0;
    stats->totalRecords++;
    switch (rec->iType) {
    case EMR_LINETO:
    case EMR_MOVETOEX:
    case EMR_POLYLINE:
    case EMR_POLYLINE16:
    case EMR_POLYGON:
    case EMR_POLYGON16:
    case EMR_POLYPOLYGON:
    case EMR_POLYPOLYGON16:
    case EMR_RECTANGLE:
    case EMR_ELLIPSE:
    case EMR_BEGINPATH:
    case EMR_ENDPATH:
    case EMR_FILLPATH:
    case EMR_STROKEPATH:
    case EMR_STROKEANDFILLPATH:
        stats->vectorPathOrLineRecords++;
        break;
    case EMR_EXTTEXTOUTW:
    case EMR_EXTTEXTOUTA:
        stats->textRecords++;
        break;
    case EMR_STRETCHDIBITS:
    case EMR_BITBLT:
        stats->bitmapRecords++;
        break;
    default:
        break;
    }
    return 1;
}

#define CHECK_COND(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl; \
            return 1; \
        } \
    } while (0)

inline bool relErrWithin(double actual, double expected, double tolRatio = 0.02)
{
    double denom = std::max(std::fabs(expected), 1e-9);
    return std::fabs(actual - expected) / denom <= tolRatio;
}

// -------------------------------------------------------------------------------------------
// Isolation from other processes. The system clipboard is shared with every program on the
// desktop (other test runs, other ToriGraph windows, clipboard managers), so a check that reads
// it back may see someone else's data. The helpers below either verify what was put there or
// repeat the copy until the data on the clipboard is ours.
// -------------------------------------------------------------------------------------------
inline QString normalizedClipboardText(QString s)
{
    s.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    return s;
}

//! Put \a text on the clipboard and read it back until it is really there
inline bool setClipboardTextVerified(const QString &text)
{
    const QString want = normalizedClipboardText(text);
    for (int k = 0; k < 40; ++k) {
        QApplication::clipboard()->setText(text);
        QCoreApplication::processEvents();
        if (normalizedClipboardText(QApplication::clipboard()->text()) == want)
            return true;
        Sleep(25 + 5 * k);
    }
    return false;
}

//! Is the clipboard still holding \a text (nobody replaced it meanwhile)?
inline bool clipboardStillHolds(const QString &text)
{
    return normalizedClipboardText(QApplication::clipboard()->text()) == normalizedClipboardText(text);
}

//! OleGetClipboard with a few retries (another process may have the clipboard open)
inline HRESULT oleGetClipboardRetry(ComPtr<IDataObject> &out)
{
    HRESULT hr = E_FAIL;
    for (int k = 0; k < 40; ++k) {
        QCoreApplication::processEvents();
        out.Reset();
        hr = OleGetClipboard(&out);
        if (SUCCEEDED(hr) && out)
            return hr;
        Sleep(25);
    }
    return FAILED(hr) ? hr : E_FAIL;
}

//! Clipboard sequence number right after this process put its data on the clipboard
inline DWORD g_clipboardSeqAfterCopy = 0;
//! Process that owned the clipboard when markClipboardAfterCopy() was called
inline DWORD g_clipboardOwnerAtMark = 0;

inline DWORD currentClipboardOwnerPid()
{
    DWORD pid = 0;
    if (HWND owner = GetClipboardOwner())
        GetWindowThreadProcessId(owner, &pid);
    return pid;
}

//! Call right after the copy under test: remembers the clipboard's sequence number and owner
inline void markClipboardAfterCopy()
{
    g_clipboardSeqAfterCopy = GetClipboardSequenceNumber();
    g_clipboardOwnerAtMark = currentClipboardOwnerPid();
}

//! Has the clipboard changed since markClipboardAfterCopy() and does it now belong to another
//! process? (A change made by this process - or no change at all - means the data under test is
//! this process's own, so a failed check is a real failure.)
//! Another program may also take the clipboard between the copy and markClipboardAfterCopy():
//! the copy does some work (registry lookup, result bookkeeping) after OleSetClipboard. The
//! sequence number then does not change after the mark, but the owner recorded at the mark is
//! already the other process - that data was never ours either.
inline bool clipboardTakenByOtherProcess(DWORD *ownerPid = nullptr)
{
    const DWORD self = GetCurrentProcessId();
    if (GetClipboardSequenceNumber() == g_clipboardSeqAfterCopy) {
        if (ownerPid)
            *ownerPid = g_clipboardOwnerAtMark;
        // (no owner at all: someone else emptied the clipboard; ours always has an owner window)
        return g_clipboardOwnerAtMark != self;
    }
    const DWORD pid = currentClipboardOwnerPid();
    if (ownerPid)
        *ownerPid = pid;
    return pid != self;
}

//! Run \a attempt (copy, markClipboardAfterCopy(), check) and return its error or nullptr. The
//! attempt is repeated only when its check failed AND the clipboard changed after the copy and
//! now belongs to another process; any other failure is reported at once, so that an occasional
//! failure of the product itself is not hidden by the retries.
inline const char *retryClipboardStep(const std::function<const char *()> &attempt,
                                      const char *what, int attempts = 8)
{
    const char *err = nullptr;
    for (int i = 0; i < attempts; ++i) {
        err = attempt();
        if (!err)
            return nullptr;
        DWORD ownerPid = 0;
        if (!clipboardTakenByOtherProcess(&ownerPid))
            return err;
        if (i + 1 < attempts) {
            std::cout << "[INFO] " << what << ": " << err
                      << "; the clipboard changed after the copy and is now owned by process "
                      << ownerPid << ", copying again (attempt " << (i + 2) << ")\n";
            QCoreApplication::processEvents();
            Sleep(100 * (i + 1));
        }
    }
    return err;
}

// -------------------------------------------------------------------------------------------
// Modal watchdog: a modal dialog that nobody answers would stop the whole run (and with it every
// later suite). Dialogs the suites open on purpose are answered within a second or two; anything
// still open after kModalWatchdogMs is reported and dismissed so the run goes on. Such a dialog
// is unexpected (e.g. an error box a regression brought up), so main() fails the run when the
// watchdog had to close one, unless its title is on the allow list below.
// Progress dialogs are left alone: they close themselves, and closing one would cancel the work.
// -------------------------------------------------------------------------------------------
inline int g_modalWatchdogDismissals = 0;

//! Titles of dialogs the watchdog may close without failing the run. Empty at present: every
//! dialog a suite opens on purpose is answered by the suite itself.
inline QStringList modalWatchdogAllowedTitles()
{
    return QStringList();
}

class ModalWatchdog : public QObject
{
public:
    static constexpr int kModalWatchdogMs = 20000;
    explicit ModalWatchdog(QObject *parent = nullptr) : QObject(parent)
    {
        m_timer.setInterval(500);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this]() { poll(); });
        m_timer.start();
    }

private:
    void poll()
    {
        QWidget *w = QApplication::activeModalWidget();
        if (w != m_seen) {
            m_seen = w;
            m_since.start();
            return;
        }
        if (!w || qobject_cast<QProgressDialog *>(w) || m_since.elapsed() < kModalWatchdogMs)
            return;
        const bool allowed = modalWatchdogAllowedTitles().contains(w->windowTitle());
        if (!allowed)
            ++g_modalWatchdogDismissals;
        std::cerr << (allowed ? "[INFO]" : "[WARN]") << " modal watchdog: dismissing a dialog left open for "
                  << m_since.elapsed() / 1000 << " s: \"" << w->windowTitle().toStdString() << "\"";
        if (auto *box = qobject_cast<QMessageBox *>(w))
            std::cerr << " - " << box->text().left(200).toStdString();
        std::cerr << std::endl;
        if (auto *box = qobject_cast<QMessageBox *>(w)) {
            if (QAbstractButton *esc = box->escapeButton())
                esc->click();
            else
                box->reject();
        } else if (auto *dlg = qobject_cast<QDialog *>(w)) {
            dlg->reject();
        } else {
            w->close();
        }
        m_since.start();
    }

    QTimer m_timer;
    QPointer<QWidget> m_seen;
    QElapsedTimer m_since;
};


inline bool isUntranslatedAsciiUiLabel(const QString &raw)
{
    QString s = raw.trimmed();
    s.remove('&');
    if (s.isEmpty())
        return false;
    for (const QChar &ch : s) {
        if (ch.unicode() >= 0x3000)
            return false;
    }
    static const QSet<QString> kAllowedTokens = {
        "X", "Y", "Z", "W", "H", "mm", "cm", "in", "px", "pt", "DPI", "%", "K", "nm", "eV",
        "I", "B", "U", "x_2", "x^2", "(a)", "(%a)", "100%", "50%", "75%", "125%", "150%", "200%",
        "ISIJ", "ISIJ 85mm", "ISIJ (85 mm)", "Nature (89 mm)", "Log10", "Linear", "ToriGraph",
        "Viridis", "Magma", "Inferno", "Plasma", "Cividis", "Jet",
        "α / μ / σ", "x₂", "x²", "°C", "μm", "MPa", "GPa", "mass%", "at.%", "wt.ppm", "kJ/mol",
        "[fx]", "s⁻¹", "10⁻⁴ s⁻¹", "mass ppm·s⁻¹", "σ (MPa)", "σ₀.₂ (MPa)", "2θ (deg.)",
        "ε (%)", "dσ/dε (GPa)", "1000/T (K⁻¹)",
        "σ / MPa", "σ₀.₂ / MPa", "2θ / deg", "2θ / deg.", "ε / %", "dσ/dε / GPa", "dσ/dε / MPa",
        "1000/T / K⁻¹", "1:1", "4:3", "16:9", "3:4"
    };
    if (kAllowedTokens.contains(s))
        return false;
    int alphaRun = 0;
    int maxAlphaRun = 0;
    for (const QChar &ch : s) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
            ++alphaRun;
            maxAlphaRun = std::max(maxAlphaRun, alphaRun);
        } else {
            alphaRun = 0;
        }
    }
    return maxAlphaRun >= 3;
}

inline quint32 suite52Crc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<unsigned char>(data[i]);
        for (int b = 0; b < 8; ++b)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFu;
}

inline void suite52AppendU16LE(QByteArray &out, quint16 v)
{
    out.append(static_cast<char>(v & 0xFFu));
    out.append(static_cast<char>((v >> 8) & 0xFFu));
}

inline void suite52AppendU32LE(QByteArray &out, quint32 v)
{
    out.append(static_cast<char>(v & 0xFFu));
    out.append(static_cast<char>((v >> 8) & 0xFFu));
    out.append(static_cast<char>((v >> 16) & 0xFFu));
    out.append(static_cast<char>((v >> 24) & 0xFFu));
}

inline bool suite59WriteFile(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    const bool ok = f.write(bytes) == bytes.size();
    f.close();
    return ok;
}

inline QByteArray suite59ReadAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

inline void suite59Drop(ApplicationWindow *mw, const QStringList &paths)
{
    QMimeData mime;
    QList<QUrl> urls;
    for (const QString &p : paths)
        urls << QUrl::fromLocalFile(p);
    mime.setUrls(urls);
    QDropEvent ev(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    mw->dropEvent(&ev);
    QApplication::processEvents();
}

inline QString cleanMnemonicForSuite62(const QString &t)
{
    QString out;
    for (int i = 0; i < t.size(); ++i) {
        if (t.at(i) == QLatin1Char('&')) {
            if (i + 1 < t.size() && t.at(i + 1) == QLatin1Char('&'))
                out.append(t.at(++i));
            continue;
        }
        out.append(t.at(i));
    }
    return out.trimmed();
}

struct Suite64ZipEntry
{
    QByteArray name;
    QByteArray data;
    bool deflate = false;
};

inline QByteArray suite64BuildZip(const QList<Suite64ZipEntry> &entries)
{
    QByteArray zip;
    QByteArray cd;
    for (const Suite64ZipEntry &e : entries) {
        const quint32 crc = suite52Crc32(e.data);
        QByteArray payload = e.data;
        quint16 method = 0;
        if (e.deflate) {
            const QByteArray z = qCompress(e.data, 9);
            payload = z.mid(6, z.size() - 10);
            method = 8;
        }
        const quint32 offset = static_cast<quint32>(zip.size());
        suite52AppendU32LE(zip, 0x04034B50u);
        suite52AppendU16LE(zip, 20);
        suite52AppendU16LE(zip, 0);
        suite52AppendU16LE(zip, method);
        suite52AppendU16LE(zip, 0);
        suite52AppendU16LE(zip, 0);
        suite52AppendU32LE(zip, crc);
        suite52AppendU32LE(zip, static_cast<quint32>(payload.size()));
        suite52AppendU32LE(zip, static_cast<quint32>(e.data.size()));
        suite52AppendU16LE(zip, static_cast<quint16>(e.name.size()));
        suite52AppendU16LE(zip, 0);
        zip.append(e.name);
        zip.append(payload);

        suite52AppendU32LE(cd, 0x02014B50u);
        suite52AppendU16LE(cd, 20);
        suite52AppendU16LE(cd, 20);
        suite52AppendU16LE(cd, 0);
        suite52AppendU16LE(cd, method);
        suite52AppendU16LE(cd, 0);
        suite52AppendU16LE(cd, 0);
        suite52AppendU32LE(cd, crc);
        suite52AppendU32LE(cd, static_cast<quint32>(payload.size()));
        suite52AppendU32LE(cd, static_cast<quint32>(e.data.size()));
        suite52AppendU16LE(cd, static_cast<quint16>(e.name.size()));
        suite52AppendU16LE(cd, 0);
        suite52AppendU16LE(cd, 0);
        suite52AppendU16LE(cd, 0);
        suite52AppendU16LE(cd, 0);
        suite52AppendU32LE(cd, 0);
        suite52AppendU32LE(cd, offset);
        cd.append(e.name);
    }
    const quint32 cdOffset = static_cast<quint32>(zip.size());
    zip.append(cd);
    suite52AppendU32LE(zip, 0x06054B50u);
    suite52AppendU16LE(zip, 0);
    suite52AppendU16LE(zip, 0);
    suite52AppendU16LE(zip, static_cast<quint16>(entries.size()));
    suite52AppendU16LE(zip, static_cast<quint16>(entries.size()));
    suite52AppendU32LE(zip, static_cast<quint32>(cd.size()));
    suite52AppendU32LE(zip, cdOffset);
    suite52AppendU16LE(zip, 0);
    return zip;
}

inline double suite67PoleIntensity(double aDeg, double bDeg)
{
    const double d2r = 3.14159265358979323846 / 180.0;
    const double a = aDeg * d2r, b = bDeg * d2r, a0 = 45.0 * d2r, b0 = 30.0 * d2r;
    const double c = std::sin(a) * std::cos(b) * std::sin(a0) * std::cos(b0)
            + std::sin(a) * std::sin(b) * std::sin(a0) * std::sin(b0) + std::cos(a) * std::cos(a0);
    const double dDeg = std::acos(qBound(-1.0, c, 1.0)) / d2r;
    return 100.0 + 900.0 * std::exp(-dDeg * dDeg / (2.0 * 10.0 * 10.0));
}

// Modular suite entry points (Suites 36..78)
int runSuite36Round3SurgicalZero(ApplicationWindow *mw);
int runSuite37DuotoneIconsAndTheme(ApplicationWindow *mw);
int runSuite38RibbonSplitPaletteAndFocusHud(ApplicationWindow *mw);
int runSuite39GitHubAutoUpdaterEngine(ApplicationWindow *mw);
int runSuite40PythonJsonRpcServerDispatch(ApplicationWindow *mw);
int runSuite41Phase6JapaneseZeroMissAndArialGate(ApplicationWindow *mw);
int runSuite42Round2CriticRegressionGate(ApplicationWindow *mw);
int runSuite43Round3CriticRegressionGate(ApplicationWindow *mw);
int runSuite44Round4UnifiedRegressionGate(ApplicationWindow *mw);
int runSuite45Round5ZeroDefectGate(ApplicationWindow *mw);
int runSuite46OleAntialiasingAndSubpixelSmoothnessGate(ApplicationWindow *mw);
int runSuite47Phase7GraphGuiUsabilityAndZeroLeakGate(ApplicationWindow *mw);
int runSuite48Phase8RibbonDockWorksheetAndGraphErgonomicsGate(ApplicationWindow *mw);
int runSuite49PhaseBUxRenderMatSciAndApiGate(ApplicationWindow *mw);
int runSuite50MegaWave1PlotsMatSciAutonomyUndoPerf(ApplicationWindow *mw);
int runSuite51Campaign6FinalEndToEndInvariants(ApplicationWindow *mw);
int runSuite52Phase12OpjuApiGraphMathAndGuiGate(ApplicationWindow *mw);
int runSuite53SaveSafetyUndoAndRecovery(ApplicationWindow *mw);
int runSuite55TensileAndWorkHardeningCorrectness(ApplicationWindow *mw);
int runSuite57NumericalRobustnessXrdWeibullKissinger(ApplicationWindow *mw);
int runSuite56FitConvergenceAndStatistics(ApplicationWindow *mw);
int runSuite59DataImportAndWorksheet(ApplicationWindow *mw);
int runSuite63JapaneseUiAccessibilityGate(ApplicationWindow *mw);
int runSuite61DistributionVersionAndUpdaterSafety(ApplicationWindow *mw);
int runSuite60PowerPointOleLink(ApplicationWindow *mw);
int runSuite62BeginnerNavigationMenusAndScreenFit(ApplicationWindow *mw);
int runSuite58FigureDefaultsPresetsAndExport(ApplicationWindow *mw);
int runSuite64XlsxImport(ApplicationWindow *mw);
int runSuite65E11ExtraGraphTypes(ApplicationWindow *mw);
int runSuite70IconCleanupAndLightTheme(ApplicationWindow *mw);
int runSuite72ToriGraphStandardDefaultPalette(ApplicationWindow *mw);
int runSuite67Density2DAndPoleFigure(ApplicationWindow *mw);
int runSuite68RibbonFoldingRecentModelsAndMiniToolbarKeys(ApplicationWindow *mw);
int runSuite66Workbooks(ApplicationWindow *mw);
int runSuite69SettingsTransferAndScriptCoverage(ApplicationWindow *mw);
int runSuite74LegacyUiHidden(ApplicationWindow *mw);
int runSuite71LabSlideTextAndZoomFollowsText(ApplicationWindow *mw);
int runSuite73SendToPowerPoint(ApplicationWindow *mw);
int runSuite75CopyAndExportKeepGraphState(ApplicationWindow *mw);
int runSuite76ReviewPolishAndEdgeCases(ApplicationWindow *mw);
int runSuite77Nov22NightReviewRegressions(ApplicationWindow *mw);
int runSuite78ZeroBaseUxAndQualityGate(ApplicationWindow *mw);
int runSuite79ZeroBaseOverhaulV2Gate(ApplicationWindow *mw);
int runSuite80Round4P0P1RegressionGate(ApplicationWindow *mw);
int runSuite81NumericalTransformCorrectness(ApplicationWindow *mw);
int runSuite82JsonRpcParamValidation(ApplicationWindow *mw);
int runSuite83ImportRobustness(ApplicationWindow *mw);
int runSuite84ProjectSerializationFidelity(ApplicationWindow *mw);
int runSuite85TableMatrixCoreIntegrity(ApplicationWindow *mw);
int runSuite86AxisScaleCorrectness(ApplicationWindow *mw);
int runSuite87Graph3DMatrixOrientation(ApplicationWindow *mw);
int runSuite88ExportAndFftSampling(ApplicationWindow *mw);
int runSuite89CurveDataMapping(ApplicationWindow *mw);
int runSuite90FftWindowTeardown(ApplicationWindow *mw);
int runSuite91GraphCopyStackTransforms(ApplicationWindow *mw);
int runSuite92RenamePropagation(ApplicationWindow *mw);
int runSuite93WorksheetSortAndBlankRows(ApplicationWindow *mw);
int runSuite94RpcWorksheetDataFidelity(ApplicationWindow *mw);
int runSuite95FitDataDriven(ApplicationWindow *mw);
int runSuite96XrdPeaksAndUnsortedDiff(ApplicationWindow *mw);
int runSuite97PlotDataFidelity(ApplicationWindow *mw);
int runSuite98RpcImportRobustness(ApplicationWindow *mw);
int runSuite99DateTimeParsingAndWideTables(ApplicationWindow *mw);
int runSuite100NumericsRound2(ApplicationWindow *mw);
int runSuite101WorksheetRound2(ApplicationWindow *mw);
int runSuite102PlotRound2(ApplicationWindow *mw);
int runSuite103ImportRound2(ApplicationWindow *mw);
int runSuite104ImportRound3(ApplicationWindow *mw);
int runSuite105RpcImportColumnTypes(ApplicationWindow *mw);

#endif // TORIGRAPH_OLE_TEST_COMMON_H
