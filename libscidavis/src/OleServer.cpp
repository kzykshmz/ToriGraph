#include "OleServer.h"

#include "I18nJapanese.h"

#include <QObject>

namespace ToriGraphOleUi {

void registerJapaneseStrings()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                  QString::fromUtf8(ja));
    };
    reg("Copy for PowerPoint", "PowerPoint 用にコピー");
    reg("Copy Graph for Power&Point", "グラフを PowerPoint 用にコピー(&P)");
    reg("Copy the graph for PowerPoint / Word as an editable object. After pasting, double-click "
        "it on the slide to edit it again in ToriGraph.",
        "グラフを PowerPoint・Word 用に、編集できるオブジェクトとしてコピーします。"
        "貼り付け後、スライド上でダブルクリックすると再編集できます。");
    reg("Embed Whole Project in PowerPoint Copies",
        "PowerPoint 用コピーにプロジェクト全体を埋め込む");
    reg("Repair PowerPoint Link (OLE Registration)...",
        "PowerPoint 連携を修復(OLE 登録)...");
    reg("Select a graph window first. \"Copy for PowerPoint\" copies the active graph.",
        "グラフを選んでください。「PowerPoint 用にコピー」は、選択中のグラフをコピーします。");
    reg("Copied %1 for PowerPoint (%2 x %3 mm). Paste with Ctrl+V; double-click it on the slide "
        "to edit it again.",
        "%1 を PowerPoint 用にコピーしました(%2 × %3 mm)。Ctrl+V で貼り付け、"
        "スライド上でダブルクリックすると再編集できます。");
    reg("Embedded: %1.", "埋め込んだウィンドウ: %1。");
    reg("Embedded: the whole project.", "埋め込み: プロジェクト全体。");
    reg("Could not copy %1 as an editable object for PowerPoint.",
        "%1 を PowerPoint 用の編集できるオブジェクトとしてコピーできませんでした。");
    reg("A picture of the graph (EMF vector and %1 dpi PNG) was placed on the clipboard "
        "instead. It can be pasted, but not edited again from PowerPoint.",
        "代わりにグラフの画像(EMF ベクターと %1 dpi の PNG)をクリップボードに載せました。"
        "貼り付けはできますが、PowerPoint から再編集はできません。");
    reg("Nothing was placed on the clipboard.", "クリップボードには何も載せていません。");
    reg("A picture of the graph (bitmap only) was placed on the clipboard instead. It can be "
        "pasted, but not edited again from PowerPoint.",
        "代わりにグラフの画像 (ビットマップのみ) をクリップボードに載せました。貼り付けはできますが、"
        "PowerPoint から再編集はできません。");
    reg("The clipboard is in use by another program. Wait a moment and try again.",
        "クリップボードを他のプログラムが使用中です。少し待ってからもう一度お試しください。");
    reg("OLE could not be initialized in this ToriGraph session. Restart ToriGraph and try again.",
        "この ToriGraph では OLE を初期化できませんでした。ToriGraph を再起動してからもう一度お試しください。");
    reg("The graph could not be drawn as a vector picture (EMF).",
        "グラフをベクター画像(EMF)として描画できませんでした。");
    reg("The graph could not be drawn as a picture.", "グラフを画像として描画できませんでした。");
    reg("The embedded object could not be created.", "埋め込みオブジェクトを作成できませんでした。");
    reg("The PowerPoint link of ToriGraph is not registered, or points to a program that no "
        "longer exists (%1). Pasting works, but double-clicking the graph in PowerPoint will "
        "not open ToriGraph.",
        "ToriGraph の PowerPoint 連携が登録されていないか、存在しないプログラム(%1)を指しています。"
        "貼り付けはできますが、PowerPoint でダブルクリックしても ToriGraph は開きません。");
    reg("The PowerPoint link currently points to another ToriGraph installation (%1) instead of "
        "this program (%2). Double-clicking the graph in PowerPoint will open that copy.",
        "PowerPoint 連携が別の場所の ToriGraph (%1)を指しています(現在のプログラム: %2)。"
        "PowerPoint でダブルクリックするとそちらの ToriGraph が開きます。");
    reg("The embedded project data exceeds the 128 MB PowerPoint limit (%1 MB).",
        "埋め込みデータが PowerPoint 連携の上限 128 MB を超えています(%1 MB)。");
    reg("To fix this, click \"Repair PowerPoint Link\" (also in the Help menu).",
        "直すには「PowerPoint 連携を修復」を押してください(ヘルプメニューにもあります)。");
    reg("Repair PowerPoint Link", "PowerPoint 連携を修復");
    reg("PowerPoint link repaired. Graphs pasted into PowerPoint will open in:\n%1",
        "PowerPoint 連携を修復しました。PowerPoint に貼ったグラフは次のプログラムで開きます:\n%1");
    reg("The PowerPoint link could not be registered: %1",
        "PowerPoint 連携を登録できませんでした: %1");
    reg("Repair PowerPoint Link (OLE Registration)", "PowerPoint 連携を修復(OLE 登録)");
    reg("ToriGraph could not open the graph embedded in this slide (%1).\n\nEditing was cancelled "
        "and the object on the slide was not changed.",
        "このスライドに埋め込まれたグラフを ToriGraph で開けませんでした(%1)。\n\n"
        "編集を中止しました。スライド上のオブジェクトは変更していません。");
    reg("the embedded project could not be read", "埋め込まれたプロジェクトを読み込めません");
    reg("the graph \"%1\" was not found in the embedded project",
        "埋め込まれたプロジェクトにグラフ「%1」がありません");
    reg("Edit Embedded Graph", "埋め込みグラフの編集");
    reg("The embedded graph window (\"%1\") was closed or deleted.",
        "埋め込み対象のグラフウィンドウ(「%1」)が閉じられたか削除されています。");
    reg("The embedded graph window was closed or deleted.",
        "埋め込み対象のグラフウィンドウが閉じられたか削除されています。");
    reg("Could not serialize the embedded project data.",
        "埋め込みプロジェクトのデータを書き出せませんでした。");
    reg("The embedded project data exceeds the 128 MB PowerPoint limit.",
        "埋め込みデータが PowerPoint 連携の上限 128 MB を超えています。");
    reg("PowerPoint could not save the updated graph because the presentation is read-only or "
        "locked (0x%1).",
        "プレゼンテーションが読み取り専用またはロックされているため、PowerPoint に更新後のグラフを"
        "保存できませんでした (0x%1)。");
    reg("The connection to PowerPoint was lost (the presentation may have been closed) (0x%1).",
        "PowerPoint との接続が失われました(プレゼンテーションが閉じられた可能性があります)(0x%1)。");
    reg("PowerPoint could not save the updated graph into the slide (0x%1).",
        "更新後のグラフを PowerPoint のスライドへ保存できませんでした (0x%1)。");
    reg("ToriGraph - Embedded Graph Preview Unavailable",
        "ToriGraph - 埋め込みグラフのプレビューを表示できません");
    reg("The embedded project data could not be read.",
        "埋め込まれたプロジェクトデータを読み込めませんでした。");
    reg("Graph \"%1\" was not found in the embedded project.",
        "埋め込まれたプロジェクト内にグラフ「%1」が見つかりませんでした。");
    reg("No graph could be rendered from the embedded project.",
        "埋め込まれたプロジェクトからグラフを描画できませんでした。");
}

QString copyForPowerPointLabel()
{
    registerJapaneseStrings();
    return QObject::tr("Copy for PowerPoint");
}

QString copyForPowerPointMenuText()
{
    registerJapaneseStrings();
    return QObject::tr("Copy Graph for Power&Point");
}

QString copyForPowerPointToolTip()
{
    registerJapaneseStrings();
    return QObject::tr("Copy the graph for PowerPoint / Word as an editable object. After "
                       "pasting, double-click it on the slide to edit it again in ToriGraph.");
}

QString embedWholeProjectMenuText()
{
    registerJapaneseStrings();
    return QObject::tr("Embed Whole Project in PowerPoint Copies");
}

QString repairRegistrationMenuText()
{
    registerJapaneseStrings();
    return QObject::tr("Repair PowerPoint Link (OLE Registration)...");
}

QString selectGraphFirstMessage()
{
    registerJapaneseStrings();
    return QObject::tr(
            "Select a graph window first. \"Copy for PowerPoint\" copies the active graph.");
}

} // namespace ToriGraphOleUi

#ifdef Q_OS_WIN

#include "EmfPaintDevice.h"
#include "ApplicationWindow.h"
#include "GraphInspectorDock.h"
#include "MultiLayer.h"
#include "Graph.h"
#include "Graph3D.h"
#include "Plot3DDialog.h"
#include "Table.h"
#include "Matrix.h"
#include "MyWidget.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "Spectrogram.h"


#include <QApplication>
#include <QBuffer>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QTimer>
#include <QVector>

extern "C" HRESULT STDAPICALLTYPE SHCreateStdEnumFmtEtc(UINT cfmt, const FORMATETC afmt[], IEnumFORMATETC **ppenumFormatEtc);
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

using std::max;
using std::min;
#include <objidl.h>
#include <gdiplus.h>

using Microsoft::WRL::ComPtr;

namespace SciDAVisOle {

namespace {

class SciDAVisGraphObject;

bool g_oleInitialized = false;
bool g_embeddedServerMode = false;
bool g_normalSessionLockHeld = false;
bool g_everHadServerActivity = false;
bool g_oleSuppressedQuitOnLastWindow = false;
bool g_hadVisibleNormalWindowDuringSuppression = false;
DWORD g_factoryCookie = 0;
ComPtr<IDataObject> g_clipboardDataObject;
//! GetClipboardSequenceNumber() right after the last successful OleSetClipboard
DWORD g_lastOleSetClipboardSeq = 0;
QVector<SciDAVisGraphObject *> g_liveGraphObjects;

CLIPFORMAT embedFormat()
{
    static CLIPFORMAT cf = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"Embed Source"));
    return cf;
}

CLIPFORMAT objectFormat()
{
    static CLIPFORMAT cf = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"Embedded Object"));
    return cf;
}

CLIPFORMAT descriptorFormat()
{
    static CLIPFORMAT cf = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"Object Descriptor"));
    return cf;
}

void addRefServerProcess()
{
    g_everHadServerActivity = true;
    CoAddRefServerProcess();
}

void releaseServerProcess()
{
    if (CoReleaseServerProcess() == 0) {
        if (g_embeddedServerMode && qApp) {
            bool hasNormalVisibleApp = false;
            for (QWidget *tlw : QApplication::topLevelWidgets()) {
                auto *aw = qobject_cast<ApplicationWindow *>(tlw);
                if (aw && aw->isVisible() && !aw->isOleEmbeddedMode() && !aw->isNonInteractive()) {
                    hasNormalVisibleApp = true;
                    break;
                }
            }
            if (hasNormalVisibleApp) {
                g_embeddedServerMode = false;
                if (auto *guiApp = qobject_cast<QApplication *>(qApp))
                    guiApp->setQuitOnLastWindowClosed(true);
            } else {
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            }
        }
    }
}

HRESULT writeStreamBytes(IStorage *stg, const wchar_t *name, const QByteArray &bytes)
{
    if (!stg || !name)
        return E_POINTER;
    ComPtr<IStream> stream;
    HRESULT hr = stg->CreateStream(name, STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stream);
    if (FAILED(hr))
        return hr;
    ULONG written = 0;
    if (!bytes.isEmpty()) {
        hr = stream->Write(bytes.constData(), static_cast<ULONG>(bytes.size()), &written);
        if (FAILED(hr))
            return hr;
        if (written != static_cast<ULONG>(bytes.size()))
            return STG_E_WRITEFAULT;
    }
    return S_OK;
}

HRESULT readStreamBytes(IStorage *stg, const wchar_t *name, QByteArray *outBytes)
{
    if (!stg || !name || !outBytes)
        return E_POINTER;
    outBytes->clear();
    ComPtr<IStream> stream;
    HRESULT hr = stg->OpenStream(name, nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stream);
    if (FAILED(hr))
        return hr;
    STATSTG stat{};
    hr = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hr))
        return hr;
    if (stat.cbSize.QuadPart > 128000000ULL)
        return STG_E_INSUFFICIENTMEMORY;
    const ULONG sz = static_cast<ULONG>(stat.cbSize.QuadPart);
    if (sz == 0)
        return S_OK;
    QByteArray buf(static_cast<int>(sz), '\0');
    ULONG read = 0;
    hr = stream->Read(buf.data(), sz, &read);
    if (FAILED(hr))
        return hr;
    if (read != sz)
        return STG_E_READFAULT;
    *outBytes = std::move(buf);
    return S_OK;
}

QString normalizeOleWindowKey(QString s)
{
    s = s.trimmed().toLower();
    s.replace(QLatin1Char('_'), QLatin1Char('-'));
    return s;
}

MultiLayer *findTargetMultiLayer(ApplicationWindow *app, const QString &targetName,
                                 bool allowFallback = true)
{
    if (!app)
        return nullptr;
    const QList<MyWidget *> windows = app->windowsList();
    const QString wantNorm = normalizeOleWindowKey(targetName);
    MultiLayer *firstMl = nullptr;
    MultiLayer *normMatch = nullptr;
    MultiLayer *labelMatch = nullptr;
    for (MyWidget *w : windows) {
        if (w && w->inherits("MultiLayer")) {
            MultiLayer *ml = static_cast<MultiLayer *>(w);
            if (!firstMl)
                firstMl = ml;
            if (!targetName.isEmpty()) {
                if (ml->name() == targetName)
                    return ml;
                if (!normMatch && normalizeOleWindowKey(ml->name()) == wantNorm)
                    normMatch = ml;
                if (!labelMatch
                    && (!ml->windowLabel().isEmpty()
                        && (ml->windowLabel() == targetName
                            || normalizeOleWindowKey(ml->windowLabel()) == wantNorm))) {
                    labelMatch = ml;
                }
            }
        }
    }
    if (normMatch)
        return normMatch;
    if (labelMatch)
        return labelMatch;
    return allowFallback ? firstMl : nullptr;
}

Graph3D *findTargetGraph3D(ApplicationWindow *app, const QString &targetName,
                           bool allowFallback = false)
{
    if (!app)
        return nullptr;
    const QList<MyWidget *> windows = app->windowsList();
    const QString wantNorm = normalizeOleWindowKey(targetName);
    Graph3D *firstG3d = nullptr;
    Graph3D *normMatch = nullptr;
    for (MyWidget *w : windows) {
        if (w && w->inherits("Graph3D")) {
            Graph3D *g3d = static_cast<Graph3D *>(w);
            if (!firstG3d)
                firstG3d = g3d;
            if (!targetName.isEmpty()) {
                if (g3d->name() == targetName)
                    return g3d;
                if (!normMatch && normalizeOleWindowKey(g3d->name()) == wantNorm)
                    normMatch = g3d;
            }
        }
    }
    if (normMatch)
        return normMatch;
    return allowFallback ? firstG3d : nullptr;
}

QImage captureGraph3DOleImage(Graph3D *g3d)
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
        QTemporaryFile tmpPng(
                QDir::temp().filePath(QStringLiteral("torigraph_ole3d_XXXXXX.png")));
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

QByteArray synthesizeDibFromEmf(const QByteArray &emfBytes, QSize fallbackPx)
{
    HENHMETAFILE hemf = SciDAVisEmf::createHemfFromBytes(emfBytes);
    if (!hemf)
        return {};
    const int w = std::clamp(fallbackPx.width() > 0 ? fallbackPx.width() * 2 : 1000, 100, 2400);
    const int h = std::clamp(fallbackPx.height() > 0 ? fallbackPx.height() * 2 : 1000, 100, 2400);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = h; // bottom-up for standard CF_DIB compatibility
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = static_cast<DWORD>(w * h * 4);
    bmi.bmiHeader.biXPelsPerMeter = 11811; // 300 DPI
    bmi.bmiHeader.biYPelsPerMeter = 11811;
    void *bits = nullptr;
    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    HBITMAP hBmp = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (screenDc)
        ReleaseDC(nullptr, screenDc);
    QByteArray out;
    if (memDc && hBmp && bits) {
        std::memset(bits, 0xFF, static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
        HGDIOBJ oldObj = SelectObject(memDc, hBmp);
        RECT rc{ 0, 0, w, h };
        PlayEnhMetaFile(memDc, hemf, &rc);
        GdiFlush();
        SelectObject(memDc, oldObj);
        out.resize(static_cast<int>(sizeof(BITMAPINFOHEADER)) + w * h * 4);
        std::memcpy(out.data(), &bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
        std::memcpy(out.data() + sizeof(BITMAPINFOHEADER), bits,
                    static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    }
    if (hBmp)
        DeleteObject(hBmp);
    if (memDc)
        DeleteDC(memDc);
    DeleteEnhMetaFile(hemf);
    return out;
}

// Tells the user that an embedded graph could not be opened for editing. The text is also kept
// on qApp (property "torigraph_last_ole_open_error") so that tests can check it.
void reportEmbeddedOpenFailure(const QString &reason)
{
    ToriGraphOleUi::registerJapaneseStrings();
    const QString msg = QObject::tr("ToriGraph could not open the graph embedded in this slide "
                                    "(%1).\n\nEditing was cancelled and the object on the slide "
                                    "was not changed.")
                                .arg(reason);
    if (qApp)
        qApp->setProperty("torigraph_last_ole_open_error", msg);
    if (ApplicationWindow::isNonInteractiveContext(nullptr))
        return;
    QMessageBox box(QMessageBox::Critical, QObject::tr("Edit Embedded Graph"), msg,
                    QMessageBox::Ok);
    box.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    box.exec();
}

// Hidden message-only window that owns the clipboard for the static (non-OLE) fallback:
// SetClipboardData fails when the clipboard was opened without an owner window.
HWND clipboardOwnerWindow()
{
    static HWND s_hwnd = nullptr;
    if (!s_hwnd || !IsWindow(s_hwnd)) {
        s_hwnd = CreateWindowExW(0, L"STATIC", L"ToriGraphClipboard", 0, 0, 0, 0, 0,
                                 HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    }
    return s_hwnd;
}

HGLOBAL bytesToHGlobal(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return nullptr;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(bytes.size()));
    if (!hMem)
        return nullptr;
    void *ptr = GlobalLock(hMem);
    if (!ptr) {
        GlobalFree(hMem);
        return nullptr;
    }
    std::memcpy(ptr, bytes.constData(), static_cast<size_t>(bytes.size()));
    GlobalUnlock(hMem);
    return hMem;
}

// Places a static picture of the graph (EMF vector, PNG and DIB) on the clipboard with the
// plain Win32 API. Used when the editable OLE object cannot be offered.
bool placeStaticGraphOnClipboard(const QByteArray &emfBytes, const QByteArray &pngBytes,
                                 const QByteArray &dibBytes)
{
    if (emfBytes.isEmpty() && pngBytes.isEmpty() && dibBytes.isEmpty())
        return false;
    HWND owner = clipboardOwnerWindow();
    bool opened = false;
    // Another program (a clipboard manager such as the Windows clipboard history service, or
    // Office) may hold the clipboard for a moment. It may be waiting for this thread to render a
    // delayed format of the previous copy, so keep dispatching messages while waiting.
    for (int attempt = 0; attempt < 80 && !opened; ++attempt) {
        opened = OpenClipboard(owner) != FALSE;
        if (!opened) {
            const ULONGLONG until = GetTickCount64() + 25;
            do {
                MSG msg{};
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                Sleep(5);
            } while (GetTickCount64() < until);
        }
    }
    if (!opened)
        return false;
    EmptyClipboard();
    int placed = 0;
    if (!emfBytes.isEmpty()) {
        if (HENHMETAFILE hemf = SciDAVisEmf::createHemfFromBytes(emfBytes)) {
            if (SetClipboardData(CF_ENHMETAFILE, hemf))
                ++placed;
            else
                DeleteEnhMetaFile(hemf);
        }
    }
    if (HGLOBAL hPng = bytesToHGlobal(pngBytes)) {
        if (SetClipboardData(pngClipboardFormat(), hPng))
            ++placed;
        else
            GlobalFree(hPng);
    }
    if (HGLOBAL hDib = bytesToHGlobal(dibBytes)) {
        if (SetClipboardData(CF_DIB, hDib))
            ++placed;
        else
            GlobalFree(hDib);
    }
    CloseClipboard();
    return placed > 0;
}

ApplicationWindow *createSampleAppWindow()
{
    ApplicationWindow *mw = new ApplicationWindow();
    mw->setHeadlessMode(true);
    mw->setOleEmbeddedMode(true);
    mw->applyUserSettings();
    Table *t = mw->newTable("Table1", 6, 2);
    if (t) {
        for (int r = 0; r < 6; ++r) {
            double x = double(r + 1);
            double y = x * 1.5 + 0.5;
            t->setText(r, 0, QString::number(x));
            t->setText(r, 1, QString::number(y));
        }
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_2"), Graph::LineSymbols);
        if (ml) {
            ml->resize(520, 400);
            ml->arrangeLayers(false, false);
        }
    }
    mw->savedProject();
    return mw;
}

ApplicationWindow *openAppWindowFromBytes(const QByteArray &projectBytes)
{
    if (projectBytes.isEmpty())
        return nullptr;

    // Unpredictable, exclusively created temp file that is removed automatically (also when
    // loadProject() throws or the process unwinds), instead of a guessable name in %TEMP%.
    QTemporaryFile tempProject(QDir::temp().filePath(QStringLiteral("torigraph_ole_XXXXXX.sciprj")));
    tempProject.setAutoRemove(true);
    if (!tempProject.open())
        return nullptr;
    const QString tempPath = tempProject.fileName();
    if (tempProject.write(projectBytes) != projectBytes.size() || !tempProject.flush()) {
        tempProject.close();
        return nullptr;
    }
    tempProject.close();

    std::unique_ptr<ApplicationWindow> mw(new ApplicationWindow());
    mw->setHeadlessMode(true);
    mw->setOleEmbeddedMode(true);
    mw->applyUserSettings();
    const QStringList prevRecent = mw->recentProjects;
    bool ok = mw->loadProject(tempPath);
    tempProject.remove();
    mw->recentProjects = prevRecent;
    mw->projectname = "untitled";
    mw->updateRecentProjectsList();
    if (!ok) {
        mw->setOleEmbeddedMode(true);
        return nullptr;
    }
    mw->savedProject();
    return mw.release();
}

QImage renderOlePlaceholderImage(const QString &reason)
{
    ToriGraphOleUi::registerJapaneseStrings();
    if (qApp)
        qApp->setProperty("torigraph_last_ole_placeholder_reason", reason);
    qWarning("OLE embedded preview fell back to explicit placeholder: %s", qPrintable(reason));

    constexpr int w = 1000;
    constexpr int h = 750;
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(QColor(248, 250, 252)); // #F8FAFC
    // 192 dpi -> ~132 x 99 mm physical box on the slide
    const int dpm = static_cast<int>(std::lround(192.0 / 0.0254));
    img.setDotsPerMeterX(dpm);
    img.setDotsPerMeterY(dpm);

    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);

    QPen borderPen(QColor(148, 163, 184), 4.0, Qt::DashLine);
    p.setPen(borderPen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(24.0, 24.0, w - 48.0, h - 48.0), 16.0, 16.0);

    QFont titleFont(QStringLiteral("Segoe UI"), 18, QFont::Bold);
    p.setFont(titleFont);
    p.setPen(QColor(30, 41, 59)); // #1E293B
    const QString title = QObject::tr("ToriGraph - Embedded Graph Preview Unavailable");
    p.drawText(QRect(64, h / 2 - 110, w - 128, 80), Qt::AlignHCenter | Qt::AlignBottom | Qt::TextWordWrap,
               title);

    QFont bodyFont(QStringLiteral("Segoe UI"), 13);
    p.setFont(bodyFont);
    p.setPen(QColor(71, 85, 105)); // #475569
    p.drawText(QRect(80, h / 2 - 12, w - 160, 180), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
               reason);
    p.end();
    return img;
}

void ensureRenderedPayload(OleGraphPayload *payload)
{
    if (!payload)
        return;
    if (!payload->emfBytes.isEmpty() && payload->extentHm.cx > 0 && payload->extentHm.cy > 0)
        return;

    ToriGraphOleUi::registerJapaneseStrings();
    QString placeholderReason;
    ApplicationWindow *mw = openAppWindowFromBytes(payload->projectBytes);
    // A sample graph is only acceptable for a brand-new object without project data. When the
    // embedded project cannot be read, never render (and later save) a different graph in its
    // place; the explicit placeholder below is used instead.
    if (!mw && payload->projectBytes.isEmpty())
        mw = createSampleAppWindow();
    else if (!mw)
        placeholderReason = QObject::tr("The embedded project data could not be read.");

    if (mw) {
        if (payload->projectBytes.isEmpty())
            payload->projectBytes = mw->saveProjectToByteArray();

        MultiLayer *ml = findTargetMultiLayer(mw, payload->targetWindow, false);
        Graph3D *g3d = !ml ? findTargetGraph3D(mw, payload->targetWindow, false) : nullptr;
        if (!ml && !g3d && payload->targetWindow.isEmpty()) {
            ml = findTargetMultiLayer(mw, payload->targetWindow, true);
            if (!ml)
                g3d = findTargetGraph3D(mw, payload->targetWindow, true);
        }
        if (ml) {
            if (payload->targetWindow.isEmpty())
                payload->targetWindow = ml->name();
            for (Graph *lg : ml->layersList()) {
                if (lg) {
                    lg->updatePlot();
                    lg->replot();
                }
            }
            Graph *single = (payload->targetLayer >= 1 && payload->targetLayer <= ml->layers())
                    ? ml->layer(payload->targetLayer)
                    : nullptr;
            OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml, single, false);
            payload->emfBytes = rg.emfBytes;
            payload->dibBytes = rg.dibBytes;
            payload->pixelSize = rg.pixelSize;
            if (payload->extentHm.cx <= 0 || payload->extentHm.cy <= 0)
                payload->extentHm = rg.extentHm;
        } else if (g3d) {
            if (payload->targetWindow.isEmpty())
                payload->targetWindow = g3d->name();
            OleRenderedGraph rg = SciDAVisEmf::renderFromImage(captureGraph3DOleImage(g3d));
            payload->emfBytes = rg.emfBytes;
            payload->dibBytes = rg.dibBytes;
            payload->pixelSize = rg.pixelSize;
            if (payload->extentHm.cx <= 0 || payload->extentHm.cy <= 0)
                payload->extentHm = rg.extentHm;
        } else if (!payload->targetWindow.isEmpty()) {
            placeholderReason = QObject::tr("Graph \"%1\" was not found in the embedded project.")
                                        .arg(payload->targetWindow);
        }
        mw->setOleEmbeddedMode(true);
        mw->close();
        delete mw;
    }

    if (payload->emfBytes.isEmpty()) {
        if (placeholderReason.isEmpty())
            placeholderReason = QObject::tr("No graph could be rendered from the embedded project.");
        OleRenderedGraph rg = SciDAVisEmf::renderFromImage(renderOlePlaceholderImage(placeholderReason));
        payload->emfBytes = rg.emfBytes;
        payload->dibBytes = rg.dibBytes;
        payload->pixelSize = rg.pixelSize;
        payload->extentHm = rg.extentHm;
    }
}

SIZEL naturalPayloadExtentHm(const OleGraphPayload &payload)
{
    if (payload.emfBytes.size() >= static_cast<int>(sizeof(ENHMETAHEADER))) {
        ENHMETAHEADER hdr{};
        std::memcpy(&hdr, payload.emfBytes.constData(), sizeof(hdr));
        const LONG cx = hdr.rclFrame.right - hdr.rclFrame.left;
        const LONG cy = hdr.rclFrame.bottom - hdr.rclFrame.top;
        if (hdr.iType == EMR_HEADER && cx > 0 && cy > 0)
            return SIZEL{ cx, cy };
    }
    if (payload.pixelSize.width() > 0 && payload.pixelSize.height() > 0) {
        const LONG cx = std::max<LONG>(
                100, static_cast<LONG>(std::lround(payload.pixelSize.width() * 2540.0 / 96.0)));
        const LONG cy = std::max<LONG>(
                100, static_cast<LONG>(std::lround(payload.pixelSize.height() * 2540.0 / 96.0)));
        return SIZEL{ cx, cy };
    }
    if (payload.extentHm.cx > 0 && payload.extentHm.cy > 0)
        return payload.extentHm;
    return SIZEL{ 0, 0 };
}

void updateOleQuitGuard();

QString formatOleSaveError(HRESULT hr, const QString &detail)
{
    ToriGraphOleUi::registerJapaneseStrings();
    if (!detail.isEmpty())
        return detail;
    if (hr == STG_E_INSUFFICIENTMEMORY || hr == E_OUTOFMEMORY)
        return QObject::tr("The embedded project data exceeds the 128 MB PowerPoint limit.");
    const QString hex = QStringLiteral("%1").arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0'));
    if (hr == STG_E_ACCESSDENIED || hr == STG_E_SHAREVIOLATION || hr == STG_E_LOCKVIOLATION) {
        return QObject::tr("PowerPoint could not save the updated graph because the presentation "
                           "is read-only or locked (0x%1).")
                .arg(hex);
    }
    if (hr == RPC_E_DISCONNECTED || hr == CO_E_OBJNOTCONNECTED
        || hr == HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE)) {
        return QObject::tr("The connection to PowerPoint was lost (the presentation may have been "
                           "closed) (0x%1).")
                .arg(hex);
    }
    return QObject::tr("PowerPoint could not save the updated graph into the slide (0x%1).")
            .arg(hex);
}

class SciDAVisGraphObject final : public IOleObject,
                                  public IPersistStorage,
                                  public IDataObject,
                                  public IViewObject2,
                                  public IPersistFile
{
    ULONG m_refs = 1;
    OleGraphPayload m_payload;
    bool m_dirty = true;
    bool m_pending = false;
    bool m_sameSave = false;
    bool m_handsOff = false;
    size_t m_revision = 1;
    size_t m_savedRevision = 0;
    bool m_closing = false;
    bool m_stopped = false;
    bool m_visible = false;
    size_t m_editorGeneration = 0;
    size_t m_activeEditorRefGen = 0;
    SIZEL m_extent{ 15000, 11250 };
    SIZEL m_initialRenderExtentHm{ 0, 0 };
    QPointer<ApplicationWindow> m_editor;
    QPointer<MultiLayer> m_targetMl;
    QPointer<Graph3D> m_targetG3d;
    std::wstring m_filename;

    ComPtr<IOleClientSite> m_site;
    ComPtr<IStorage> m_storage;
    ComPtr<IOleAdviseHolder> m_oleAdvises;
    ComPtr<IDataAdviseHolder> m_dataAdvises;
    ComPtr<IAdviseSink> m_viewSink;
    DWORD m_viewFlags = 0;
    QByteArray m_pngCache;    // PNG rendering of m_pngCacheEmf
    QByteArray m_pngCacheEmf; // EMF the cached PNG was rendered from
    QMetaObject::Connection m_editorDestroyedConn;
    std::shared_ptr<std::function<void()>> m_rearmHolder;

    void disconnectEditorDestroyedConn()
    {
        if (m_editorDestroyedConn) {
            QObject::disconnect(m_editorDestroyedConn);
            m_editorDestroyedConn = QMetaObject::Connection();
        }
        if (m_rearmHolder) {
            *m_rearmHolder = nullptr;
            m_rearmHolder.reset();
        }
    }

    const QByteArray &pngBytes()
    {
        if (m_pngCache.isEmpty() || m_pngCacheEmf != m_payload.emfBytes) {
            m_pngCache = renderEmfToPng(m_payload.emfBytes, naturalPayloadExtentHm(m_payload));
            m_pngCacheEmf = m_payload.emfBytes;
        }
        return m_pngCache;
    }

    void finishClose()
    {
        if (m_stopped)
            return;
        m_closing = true;
        m_stopped = true;

        auto d = m_dataAdvises;
        if (d)
            d->SendOnDataChange(this, 0, ADVF_DATAONSTOP);

        bool wasShown = m_visible;
        m_visible = false;
        auto s = m_site;
        if (wasShown && s)
            s->OnShowWindow(FALSE);

        auto a = m_oleAdvises;
        if (a)
            a->SendOnClose();

        CoDisconnectObject(static_cast<IOleObject *>(this), 0);
        m_closing = false;
        updateOleQuitGuard();
    }

    void notifyViewAndData()
    {
        ComPtr<IOleObject> alive(this);
        auto v = m_viewSink;
        if (v) {
            if (m_viewFlags & ADVF_ONLYONCE)
                m_viewSink.Reset();
            v->OnViewChange(DVASPECT_CONTENT, -1);
        }
        auto d = m_dataAdvises;
        if (d)
            d->SendOnDataChange(this, 0, 0);
    }

    void markChanged()
    {
        m_dirty = true;
        ++m_revision;
        notifyViewAndData();
    }

    void releaseEditorRefOnce(size_t gen)
    {
        if (gen != 0 && m_activeEditorRefGen == gen) {
            m_activeEditorRefGen = 0;
            Release();
        }
    }

    HRESULT syncFromEditor(QString *errOut = nullptr)
    {
        if (errOut)
            errOut->clear();
        if (!m_editor)
            return S_FALSE;

        ToriGraphOleUi::registerJapaneseStrings();
        MultiLayer *ml = m_targetMl
                ? m_targetMl.data()
                : findTargetMultiLayer(m_editor, m_payload.targetWindow, false);
        Graph3D *g3d = m_targetG3d
                ? m_targetG3d.data()
                : (!ml ? findTargetGraph3D(m_editor, m_payload.targetWindow, false) : nullptr);
        if (!ml && !g3d) {
            if (!m_payload.targetWindow.isEmpty()) {
                if (errOut) {
                    *errOut = QObject::tr("The embedded graph window (\"%1\") was closed or "
                                          "deleted.")
                                      .arg(m_payload.targetWindow);
                }
                return OLE_E_BLANK;
            }
            ml = findTargetMultiLayer(m_editor, m_payload.targetWindow, true);
            if (!ml)
                g3d = findTargetGraph3D(m_editor, m_payload.targetWindow, true);
            if (!ml && !g3d) {
                if (errOut)
                    *errOut = QObject::tr("The embedded graph window was closed or deleted.");
                return OLE_E_BLANK;
            }
        }

        if (ml) {
            for (Graph *lg : ml->layersList()) {
                if (lg) {
                    lg->updatePlot();
                    lg->replot();
                }
            }
        }

        QByteArray newProj = m_editor->saveProjectToByteArray();
        if (newProj.isEmpty()) {
            if (errOut)
                *errOut = QObject::tr("Could not serialize the embedded project data.");
            return STG_E_WRITEFAULT;
        }
        QByteArray newEmf = m_payload.emfBytes;
        QByteArray newDib = m_payload.dibBytes;
        QSize newPx = m_payload.pixelSize;
        SIZEL nextExtent = m_extent;
        SIZEL nextInitRender = m_initialRenderExtentHm;

        auto updateExtentPreservingSlideBox = [&](SIZEL renderedHm, QSize pxSz) {
            if ((renderedHm.cx <= 0 || renderedHm.cy <= 0) && pxSz.width() > 0 && pxSz.height() > 0) {
                renderedHm.cx = std::max<LONG>(
                        100, static_cast<LONG>(std::lround(pxSz.width() * 2540.0 / 96.0)));
                renderedHm.cy = std::max<LONG>(
                        100, static_cast<LONG>(std::lround(pxSz.height() * 2540.0 / 96.0)));
            }
            if (renderedHm.cx > 0 && renderedHm.cy > 0) {
                if (nextInitRender.cx <= 0 || nextInitRender.cy <= 0) {
                    nextInitRender = renderedHm;
                    if (nextExtent.cx <= 0 || nextExtent.cy <= 0)
                        nextExtent = renderedHm;
                } else if (renderedHm.cx != nextInitRender.cx
                           || renderedHm.cy != nextInitRender.cy) {
                    const double oldAr = double(nextInitRender.cx)
                            / std::max(1.0, double(nextInitRender.cy));
                    const double newAr =
                            double(renderedHm.cx) / std::max(1.0, double(renderedHm.cy));
                    if (nextExtent.cx > 0 && nextExtent.cy > 0) {
                        if (std::fabs(newAr - oldAr) / std::max(1e-6, oldAr) > 0.02) {
                            nextExtent.cx = std::clamp<LONG>(
                                    static_cast<LONG>(std::lround(double(nextExtent.cy) * newAr)),
                                    100, 1000000);
                        }
                    } else {
                        nextExtent = renderedHm;
                    }
                    nextInitRender = renderedHm;
                }
            }
        };

        if (ml) {
            Graph *single = (m_payload.targetLayer >= 1 && m_payload.targetLayer <= ml->layers())
                    ? ml->layer(m_payload.targetLayer)
                    : nullptr;
            OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml, single, false);
            if (!rg.emfBytes.isEmpty()) {
                newEmf = rg.emfBytes;
                newDib = rg.dibBytes;
                newPx = rg.pixelSize;
                updateExtentPreservingSlideBox(rg.extentHm, rg.pixelSize);
            }
        } else if (g3d) {
            OleRenderedGraph rg = SciDAVisEmf::renderFromImage(captureGraph3DOleImage(g3d));
            if (!rg.emfBytes.isEmpty()) {
                newEmf = rg.emfBytes;
                newDib = rg.dibBytes;
                newPx = rg.pixelSize;
                updateExtentPreservingSlideBox(rg.extentHm, rg.pixelSize);
            }
        }

        if (newEmf.isEmpty()) {
            if (errOut)
                *errOut = QObject::tr("The graph could not be drawn as a vector picture (EMF).");
            return E_FAIL;
        }
        if (newProj.size() > kMaxOleStreamBytes || newEmf.size() > kMaxOleStreamBytes) {
            const qint64 maxSz = std::max<qint64>(newProj.size(), newEmf.size());
            if (errOut) {
                *errOut = QObject::tr("The embedded project data exceeds the 128 MB PowerPoint "
                                      "limit (%1 MB).")
                                  .arg(double(maxSz) / 1.0e6, 0, 'f', 1);
            }
            return STG_E_INSUFFICIENTMEMORY;
        }

        if (ml)
            m_payload.targetWindow = ml->name();
        else if (g3d)
            m_payload.targetWindow = g3d->name();
        m_extent = nextExtent;
        m_initialRenderExtentHm = nextInitRender;

        const bool changed = (newProj != m_payload.projectBytes) || (newEmf != m_payload.emfBytes)
                || (m_extent.cx != m_payload.extentHm.cx) || (m_extent.cy != m_payload.extentHm.cy);
        m_payload.projectBytes = std::move(newProj);
        m_payload.emfBytes = std::move(newEmf);
        m_payload.dibBytes = std::move(newDib);
        m_payload.pixelSize = newPx;
        m_payload.extentHm = m_extent;

        if (changed) {
            m_dirty = true;
            ++m_revision;
        }
        return S_OK;
    }

    HRESULT checkFormatEtc(FORMATETC *f) const
    {
        if (!f)
            return E_POINTER;
        if (f->dwAspect != DVASPECT_CONTENT)
            return DV_E_DVASPECT;
        if (f->lindex != -1)
            return DV_E_LINDEX;
        if (f->ptd)
            return DV_E_DVTARGETDEVICE;
        if (f->cfFormat == CF_ENHMETAFILE)
            return (f->tymed & TYMED_ENHMF) ? S_OK : DV_E_TYMED;
        if (f->cfFormat == CF_DIB)
            return (f->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_TYMED;
        if (f->cfFormat == embedFormat() || f->cfFormat == objectFormat())
            return (f->tymed & TYMED_ISTORAGE) ? S_OK : DV_E_TYMED;
        if (f->cfFormat == descriptorFormat())
            return (f->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_TYMED;
        if (f->cfFormat == pngClipboardFormat())
            return (f->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_TYMED;
        return DV_E_FORMATETC;
    }

public:
    explicit SciDAVisGraphObject(OleGraphPayload payload)
        : m_payload(std::move(payload))
    {
        ensureRenderedPayload(&m_payload);
        if (m_payload.extentHm.cx > 0 && m_payload.extentHm.cy > 0)
            m_extent = m_payload.extentHm;
        m_initialRenderExtentHm = naturalPayloadExtentHm(m_payload);
        if (m_initialRenderExtentHm.cx <= 0 || m_initialRenderExtentHm.cy <= 0)
            m_initialRenderExtentHm = m_extent;
        g_liveGraphObjects.push_back(this);
        addRefServerProcess();
        CreateOleAdviseHolder(&m_oleAdvises);
        CreateDataAdviseHolder(&m_dataAdvises);
    }

    ~SciDAVisGraphObject()
    {
        disconnectEditorDestroyedConn();
        g_liveGraphObjects.removeAll(this);
        updateOleQuitGuard();
        releaseServerProcess();
    }

    bool hasActiveContainerOrEditor() const
    {
        return !m_editor.isNull() || (m_site != nullptr && !m_stopped);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (id == IID_IUnknown || id == IID_IOleObject)
            *out = static_cast<IOleObject *>(this);
        else if (id == IID_IPersist || id == IID_IPersistStorage)
            *out = static_cast<IPersistStorage *>(this);
        else if (id == IID_IPersistFile)
            *out = static_cast<IPersistFile *>(this);
        else if (id == IID_IDataObject)
            *out = static_cast<IDataObject *>(this);
        else if (id == IID_IViewObject || id == IID_IViewObject2)
            *out = static_cast<IViewObject2 *>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refs; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG n = --m_refs;
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE SetClientSite(IOleClientSite *site) override
    {
        m_site = site;
        updateOleQuitGuard();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetClientSite(IOleClientSite **site) override
    {
        return m_site.CopyTo(site);
    }

    HRESULT STDMETHODCALLTYPE SetHostNames(LPCOLESTR, LPCOLESTR) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE Close(DWORD option) override
    {
        ComPtr<IOleObject> alive(this);
        if (option > OLECLOSE_PROMPTSAVE)
            return E_INVALIDARG;
        if (m_closing || m_stopped)
            return S_OK;

        m_closing = true;
        if (option != OLECLOSE_NOSAVE) {
            try {
                syncFromEditor();
            } catch (...) {
            }
            auto s = m_site;
            if (m_dirty && s) {
                const HRESULT hrSave = s->SaveObject();
                if (hrSave == OLE_E_PROMPTSAVECANCELLED) {
                    m_closing = false;
                    return hrSave;
                }
            }
        }

        if (m_editor) {
            disconnectEditorDestroyedConn();
            const size_t closingGen = m_activeEditorRefGen;
            ++m_editorGeneration;
            QPointer<ApplicationWindow> ed = m_editor;
            m_editor = nullptr;
            m_targetMl = nullptr;
            m_targetG3d = nullptr;
            if (ed) {
                ed->setOleEmbeddedCallbacks({}, {});
                ed->setHeadlessMode(true);
                ed->setOleEmbeddedMode(true);
                ed->setAttribute(Qt::WA_DeleteOnClose, false);
                // Reject any open child/modal dialogs before hiding and deferring deletion so an
                // incoming COM STA Close() inside a nested QDialog::exec() never destroys the
                // parent ApplicationWindow while the dialog's stack frame is still unwinding.
                auto belongsToEditor = [ed](QWidget *w) {
                    for (QWidget *cur = w; cur; cur = cur->parentWidget()) {
                        if (cur == ed.data())
                            return true;
                    }
                    return false;
                };
                if (QWidget *modal = QApplication::activeModalWidget()) {
                    if (auto *dlg = qobject_cast<QDialog *>(modal)) {
                        if (belongsToEditor(dlg))
                            dlg->reject();
                    }
                }
                const QList<QDialog *> childDlgs = ed->findChildren<QDialog *>();
                for (QDialog *dlg : childDlgs) {
                    if (dlg && dlg->isVisible())
                        dlg->reject();
                }
                ed->hide();
                QTimer::singleShot(0, ed.data(), [ed]() {
                    if (ed) {
                        ed->close();
                        ed->deleteLater();
                    }
                });
            }
            releaseEditorRefOnce(closingGen);
        }

        finishClose();
        m_closing = false;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetMoniker(DWORD, IMoniker *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker **p) override
    {
        if (p)
            *p = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE InitFromData(IDataObject *, BOOL, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetClipboardData(DWORD, IDataObject **p) override
    {
        return QueryInterface(IID_IDataObject, reinterpret_cast<void **>(p));
    }

    HRESULT STDMETHODCALLTYPE DoVerb(LONG verb, LPMSG, IOleClientSite *s, LONG, HWND, LPCRECT) override
    {
        if (m_handsOff || m_closing)
            return E_UNEXPECTED;

        if (verb == OLEIVERB_HIDE) {
            updateOleQuitGuard();
            if (m_editor)
                m_editor->hide();
            m_visible = false;
            if (m_site)
                m_site->OnShowWindow(FALSE);
            updateOleQuitGuard();
            return S_OK;
        }

        if (verb != OLEIVERB_PRIMARY && verb != OLEIVERB_OPEN && verb != OLEIVERB_SHOW
            && verb != OLEIVERB_INPLACEACTIVATE && verb != OLEIVERB_UIACTIVATE
            && verb != 0 && verb != 1) {
            return OLEOBJ_E_NOVERBS;
        }

        if (s)
            m_site = s;
        updateOleQuitGuard();

        if (m_editor) {
            m_editor->showNormal();
            m_editor->raise();
            m_editor->activateWindow();
            SetForegroundWindow(reinterpret_cast<HWND>(m_editor->winId()));
            if (!m_visible) {
                m_visible = true;
                if (m_site)
                    m_site->OnShowWindow(TRUE);
            }
            updateOleQuitGuard();
            return S_OK;
        }

        const bool prevStopped = m_stopped;
        m_stopped = false;
        const size_t generation = ++m_editorGeneration;
        m_activeEditorRefGen = generation;
        AddRef();
        QPointer<ApplicationWindow> mwGuard;

        try {
            ApplicationWindow *mw = openAppWindowFromBytes(m_payload.projectBytes);
            if (!mw) {
                if (!m_payload.projectBytes.isEmpty()) {
                    // Never substitute a sample graph for an embedding we could not read: closing
                    // that editor would overwrite the user's graph on the slide.
                    reportEmbeddedOpenFailure(
                            QObject::tr("the embedded project could not be read"));
                    m_stopped = prevStopped;
                    releaseEditorRefOnce(generation);
                    return E_FAIL;
                }
                mw = createSampleAppWindow();
            }
            mwGuard = mw;

            MultiLayer *ml = findTargetMultiLayer(mw, m_payload.targetWindow, false);
            Graph3D *g3d = !ml ? findTargetGraph3D(mw, m_payload.targetWindow, false) : nullptr;
            if (!ml && !g3d && !m_payload.targetWindow.isEmpty()) {
                // The project loaded, but the graph this object shows is not in it. Editing
                // another graph would replace the picture on the slide, so stop here.
                mw->setOleEmbeddedMode(true);
                mw->close();
                delete mw;
                reportEmbeddedOpenFailure(
                        QObject::tr("the graph \"%1\" was not found in the embedded project")
                                .arg(m_payload.targetWindow));
                m_stopped = prevStopped;
                releaseEditorRefOnce(generation);
                return E_FAIL;
            }
            if (!ml && !g3d) {
                ml = findTargetMultiLayer(mw, m_payload.targetWindow, true);
                if (!ml)
                    g3d = findTargetGraph3D(mw, m_payload.targetWindow, true);
            }
            m_targetMl = ml;
            m_targetG3d = g3d;
            m_initialRenderExtentHm = naturalPayloadExtentHm(m_payload);
            if (ml) {
                m_payload.targetWindow = ml->name();
                if (ml->pageUnit() == MultiLayer::UnitPx && !ml->pageViewMode())
                    ml->setPageViewMode(true);
                mw->activateSubWindow(ml);
                Graph *single = nullptr;
                if (m_payload.targetLayer >= 1 && m_payload.targetLayer <= ml->layers()) {
                    single = ml->layer(m_payload.targetLayer);
                    ml->setActiveGraph(single);
                }
                for (Graph *lg : ml->layersList()) {
                    if (lg) {
                        lg->updatePlot();
                        lg->replot();
                    }
                }
                OleRenderedGraph initRg = SciDAVisEmf::renderMultiLayer(ml, single, false);
                if (initRg.extentHm.cx > 0 && initRg.extentHm.cy > 0)
                    m_initialRenderExtentHm = initRg.extentHm;
                else if (initRg.pixelSize.width() > 0 && initRg.pixelSize.height() > 0) {
                    m_initialRenderExtentHm.cx = std::max<LONG>(
                            100, static_cast<LONG>(std::lround(initRg.pixelSize.width() * 2540.0 / 96.0)));
                    m_initialRenderExtentHm.cy = std::max<LONG>(
                            100, static_cast<LONG>(std::lround(initRg.pixelSize.height() * 2540.0 / 96.0)));
                }
            } else if (g3d) {
                m_payload.targetWindow = g3d->name();
                mw->activateSubWindow(g3d);
            }

            QString titleSuffix = m_payload.targetWindow.isEmpty()
                    ? QObject::tr("Embedded Graph")
                    : m_payload.targetWindow;
            mw->setWindowTitle(QObject::tr("ToriGraph - %1 [PowerPoint OLE]").arg(titleSuffix));
            const bool appNonInteractive =
                    qApp && qApp->property("torigraph_noninteractive").toBool();
            mw->setHeadlessMode(appNonInteractive);
            mw->setOleEmbeddedMode(true);
            // OLE editor windows are owned by the embedding container, never by the standalone
            // user GUI session: hiding (OLEIVERB_HIDE) or closing them must not trigger Qt's
            // lastWindowClosed -> quit() on its own.
            mw->setAttribute(Qt::WA_QuitOnClose, false);

            auto rearmHolder = std::make_shared<std::function<void()>>();
            m_rearmHolder = rearmHolder;
            *rearmHolder = [this, mwGuard, generation, rearmHolder]() {
                if (!mwGuard)
                    return;
                auto makeUpdateCb = [this, mwGuard]() {
                    ComPtr<IOleObject> alive(this);
                    QString errMsg;
                    HRESULT hrSync = E_FAIL;
                    try {
                        hrSync = syncFromEditor(&errMsg);
                    } catch (...) {
                        hrSync = E_FAIL;
                    }
                    bool ok = SUCCEEDED(hrSync) && (hrSync != S_FALSE);
                    if (!ok) {
                        errMsg = formatOleSaveError(hrSync, errMsg);
                    } else {
                        markChanged();
                        auto site = m_site;
                        if (site) {
                            const HRESULT hrSave = site->SaveObject();
                            if (FAILED(hrSave)) {
                                ok = false;
                                errMsg = formatOleSaveError(hrSave, QString());
                            } else if (!m_stopped) {
                                site->ShowObject();
                            }
                        }
                    }
                    if (ok)
                        errMsg.clear();
                    if (mwGuard) {
                        mwGuard->setProperty("_toriLastOleSaveOk", ok);
                        mwGuard->setProperty("_toriLastOleSaveError", errMsg);
                    }
                };
                mwGuard->setOleEmbeddedCallbacks(
                    makeUpdateCb,
                    [this, mwGuard, generation, rearmHolder]() {
                        ComPtr<IOleObject> alive(this);
                        bool ok = true;
                        QString errMsg;
                        if (generation == m_editorGeneration) {
                            HRESULT hrSync = E_FAIL;
                            try {
                                hrSync = syncFromEditor(&errMsg);
                            } catch (...) {
                                hrSync = E_FAIL;
                            }
                            ok = SUCCEEDED(hrSync) && (hrSync != S_FALSE);
                            if (!ok) {
                                errMsg = formatOleSaveError(hrSync, errMsg);
                            } else if (m_dirty) {
                                notifyViewAndData();
                                if (m_site) {
                                    const HRESULT hrSave = m_site->SaveObject();
                                    if (FAILED(hrSave)) {
                                        ok = false;
                                        errMsg = formatOleSaveError(hrSave, QString());
                                    }
                                }
                            }
                            if (ok) {
                                errMsg.clear();
                                disconnectEditorDestroyedConn();
                                m_editor = nullptr;
                                m_targetMl = nullptr;
                                m_targetG3d = nullptr;
                                finishClose();
                                releaseEditorRefOnce(generation);
                            } else if (mwGuard && rearmHolder && *rearmHolder) {
                                // Re-arm callbacks in case ApplicationWindow::closeEvent moved
                                // d_ole_on_close out before invoking it and then cancels closing
                                // (ce->ignore()) so the user can retry or Save As.
                                (*rearmHolder)();
                            }
                        }
                        if (mwGuard) {
                            mwGuard->setProperty("_toriLastOleSaveOk", ok);
                            mwGuard->setProperty("_toriLastOleSaveError", errMsg);
                        }
                    });
            };
            (*rearmHolder)();
            // Break the shared_ptr cycle once mw is destroyed, and ensure clean COM teardown if
            // mwGuard is ultimately destroyed without a successful onClose.
            disconnectEditorDestroyedConn();
            m_editorDestroyedConn = QObject::connect(
                    mw, &QObject::destroyed,
                    [this, alive = ComPtr<IOleObject>(this), generation, rearmHolder]() {
                        *rearmHolder = nullptr;
                        m_editorDestroyedConn = QMetaObject::Connection();
                        if (generation == m_editorGeneration) {
                            m_editor = nullptr;
                            m_targetMl = nullptr;
                            m_targetG3d = nullptr;
                            finishClose();
                        }
                        releaseEditorRefOnce(generation);
                    });

            m_editor = mw;
            updateOleQuitGuard();
            mw->show();
            if (ml) {
                mw->activateSubWindow(ml);
                if (GraphInspectorDock *dock = mw->graphInspectorDock())
                    dock->setTargetMultiLayer(ml, true);
            } else if (g3d) {
                mw->activateSubWindow(g3d);
            }
            mw->updateGraphInspectorResponsiveness();
            mw->savedProject();
            mw->raise();
            mw->activateWindow();
            SetForegroundWindow(reinterpret_cast<HWND>(mw->winId()));

            m_visible = true;
            if (m_site)
                m_site->OnShowWindow(TRUE);
            return S_OK;
        } catch (...) {
            if (mwGuard) {
                if (m_editor == mwGuard)
                    m_editor = nullptr;
                m_targetMl = nullptr;
                m_targetG3d = nullptr;
                mwGuard->setOleEmbeddedCallbacks({}, {});
                mwGuard->setHeadlessMode(true);
                mwGuard->setOleEmbeddedMode(true);
                mwGuard->close();
            }
            m_stopped = prevStopped;
            releaseEditorRefOnce(generation);
            return E_FAIL;
        }
    }

    HRESULT STDMETHODCALLTYPE EnumVerbs(IEnumOLEVERB **p) override
    {
        if (p)
            *p = nullptr;
        return OLE_S_USEREG;
    }

    HRESULT STDMETHODCALLTYPE Update() override
    {
        syncFromEditor();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE IsUpToDate() override { return S_OK; }

    HRESULT STDMETHODCALLTYPE GetUserClassID(CLSID *p) override
    {
        if (!p)
            return E_POINTER;
        *p = graph_clsid;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetUserType(DWORD, LPOLESTR *p) override
    {
        if (!p)
            return E_POINTER;
        const wchar_t text[] = L"ToriGraph Graph";
        *p = static_cast<LPOLESTR>(CoTaskMemAlloc(sizeof(text)));
        if (!*p)
            return E_OUTOFMEMORY;
        std::memcpy(*p, text, sizeof(text));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetExtent(DWORD aspect, SIZEL *s) override
    {
        if (aspect != DVASPECT_CONTENT)
            return DV_E_DVASPECT;
        if (!s || s->cx <= 0 || s->cy <= 0 || s->cx > 1000000 || s->cy > 1000000)
            return E_INVALIDARG;
        m_extent = *s;
        m_payload.extentHm = *s;
        markChanged();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetExtent(DWORD aspect, SIZEL *s) override
    {
        if (!s)
            return E_POINTER;
        if (aspect != DVASPECT_CONTENT)
            return DV_E_DVASPECT;
        *s = m_extent;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Advise(IAdviseSink *s, DWORD *c) override
    {
        return m_oleAdvises ? m_oleAdvises->Advise(s, c) : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE Unadvise(DWORD c) override
    {
        return m_oleAdvises ? m_oleAdvises->Unadvise(c) : OLE_E_NOCONNECTION;
    }

    HRESULT STDMETHODCALLTYPE EnumAdvise(IEnumSTATDATA **e) override
    {
        return m_oleAdvises ? m_oleAdvises->EnumAdvise(e) : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE GetMiscStatus(DWORD aspect, DWORD *s) override
    {
        if (!s)
            return E_POINTER;
        if (aspect != DVASPECT_CONTENT)
            return DV_E_DVASPECT;
        *s = OLEMISC_RECOMPOSEONRESIZE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetColorScheme(LOGPALETTE *) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE GetClassID(CLSID *c) override { return GetUserClassID(c); }

    HRESULT STDMETHODCALLTYPE IsDirty() override
    {
        try {
            syncFromEditor();
        } catch (...) {
            return S_OK;
        }
        return m_dirty ? S_OK : S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE InitNew(IStorage *s) override
    {
        if (!s)
            return E_POINTER;
        m_storage = s;
        m_dirty = true;
        m_pending = false;
        m_handsOff = false;
        ensureRenderedPayload(&m_payload);
        m_extent = m_payload.extentHm;
        m_initialRenderExtentHm = naturalPayloadExtentHm(m_payload);
        if (m_initialRenderExtentHm.cx <= 0 || m_initialRenderExtentHm.cy <= 0)
            m_initialRenderExtentHm = m_extent;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Load(IStorage *s) override
    {
        if (!s)
            return E_POINTER;
        OleGraphPayload loaded;
        HRESULT hr = loadPayloadFromStorage(s, &loaded);
        if (FAILED(hr))
            return hr;
        m_payload = std::move(loaded);
        m_extent = m_payload.extentHm;
        m_initialRenderExtentHm = naturalPayloadExtentHm(m_payload);
        m_storage = s;
        m_dirty = false;
        m_pending = false;
        m_handsOff = false;
        ++m_revision;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Save(IStorage *s, BOOL same) override
    {
        ComPtr<IOleObject> alive(this);
        if (!s)
            return E_POINTER;
        if (m_pending || m_handsOff)
            return E_UNEXPECTED;

        try {
            const HRESULT hrSync = syncFromEditor();
            if (FAILED(hrSync))
                return hrSync;
            m_payload.extentHm = m_extent;
            HRESULT hr = savePayloadToStorage(s, m_payload);
            if (FAILED(hr))
                return hr;
            m_pending = true;
            m_sameSave = (same != FALSE);
            m_savedRevision = m_revision;
            return S_OK;
        } catch (...) {
            return STG_E_WRITEFAULT;
        }
    }

    HRESULT STDMETHODCALLTYPE SaveCompleted(IStorage *s) override
    {
        ComPtr<IOleObject> alive(this);
        if (m_handsOff && !s)
            return E_INVALIDARG;
        if (!m_pending && !m_handsOff)
            return E_UNEXPECTED;

        const bool actualSave = m_pending && (m_sameSave || s != nullptr);
        if (s)
            m_storage = s;
        if (actualSave && m_savedRevision == m_revision)
            m_dirty = false;
        m_pending = false;
        m_handsOff = false;
        if (actualSave && m_oleAdvises)
            m_oleAdvises->SendOnSave();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE HandsOffStorage() override
    {
        m_storage.Reset();
        m_handsOff = true;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Load(LPCOLESTR path, DWORD) override
    {
        if (!path)
            return E_POINTER;
        QString qpath = QString::fromWCharArray(path);
        QFile f(qpath);
        if (!f.open(QIODevice::ReadOnly))
            return STG_E_FILENOTFOUND;
        QByteArray bytes = f.readAll();
        f.close();
        if (bytes.isEmpty())
            return STG_E_INVALIDHEADER;

        OleGraphPayload loaded;
        loaded.projectBytes = bytes;
        ensureRenderedPayload(&loaded);
        m_payload = std::move(loaded);
        m_extent = m_payload.extentHm;
        m_initialRenderExtentHm = naturalPayloadExtentHm(m_payload);
        if (m_initialRenderExtentHm.cx <= 0 || m_initialRenderExtentHm.cy <= 0)
            m_initialRenderExtentHm = m_extent;
        m_filename = path;
        m_dirty = false;
        ++m_revision;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Save(LPCOLESTR path, BOOL remember) override
    {
        syncFromEditor();
        std::wstring target = path ? path : m_filename;
        if (target.empty())
            return STG_E_INVALIDNAME;
        QSaveFile f(QString::fromWCharArray(target.c_str()));
        if (!f.open(QIODevice::WriteOnly)
            || f.write(m_payload.projectBytes) != m_payload.projectBytes.size()
            || !f.commit()) {
            return STG_E_WRITEFAULT;
        }
        if (remember || !path) {
            m_filename = target;
            if (!m_site)
                m_dirty = false;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SaveCompleted(LPCOLESTR) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE GetCurFile(LPOLESTR *out) override
    {
        if (!out)
            return E_POINTER;
        std::wstring s = m_filename.empty() ? L"*.sciprj" : m_filename;
        *out = static_cast<LPOLESTR>(CoTaskMemAlloc((s.size() + 1) * sizeof(wchar_t)));
        if (!*out)
            return E_OUTOFMEMORY;
        std::memcpy(*out, s.c_str(), (s.size() + 1) * sizeof(wchar_t));
        return m_filename.empty() ? S_FALSE : S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC *f, STGMEDIUM *m) override
    {
        if (!m)
            return E_POINTER;
        *m = {};
        HRESULT hr = checkFormatEtc(f);
        if (FAILED(hr))
            return hr;

        try {
            syncFromEditor();
            ensureRenderedPayload(&m_payload);

            if (f->cfFormat == CF_ENHMETAFILE) {
                HENHMETAFILE hemf = SciDAVisEmf::createHemfFromBytes(m_payload.emfBytes);
                if (!hemf)
                    return E_FAIL;
                m->tymed = TYMED_ENHMF;
                m->hEnhMetaFile = hemf;
                return S_OK;
            }
            if (f->cfFormat == CF_DIB) {
                if (m_payload.dibBytes.isEmpty() && !m_payload.emfBytes.isEmpty()) {
                    m_payload.dibBytes =
                            synthesizeDibFromEmf(m_payload.emfBytes, m_payload.pixelSize);
                }
                if (m_payload.dibBytes.isEmpty())
                    return E_FAIL;
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(m_payload.dibBytes.size()));
                if (!hMem)
                    return E_OUTOFMEMORY;
                void *ptr = GlobalLock(hMem);
                if (!ptr) {
                    GlobalFree(hMem);
                    return E_OUTOFMEMORY;
                }
                std::memcpy(ptr, m_payload.dibBytes.constData(), static_cast<size_t>(m_payload.dibBytes.size()));
                GlobalUnlock(hMem);
                m->tymed = TYMED_HGLOBAL;
                m->hGlobal = hMem;
                return S_OK;
            }
            if (f->cfFormat == pngClipboardFormat()) {
                const QByteArray &png = pngBytes();
                if (png.isEmpty())
                    return E_FAIL;
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(png.size()));
                if (!hMem)
                    return E_OUTOFMEMORY;
                void *ptr = GlobalLock(hMem);
                if (!ptr) {
                    GlobalFree(hMem);
                    return E_OUTOFMEMORY;
                }
                std::memcpy(ptr, png.constData(), static_cast<size_t>(png.size()));
                GlobalUnlock(hMem);
                m->tymed = TYMED_HGLOBAL;
                m->hGlobal = hMem;
                return S_OK;
            }
            if (f->cfFormat == descriptorFormat()) {
                const wchar_t userType[] = L"ToriGraph Graph";
                const wchar_t srcName[] = L"ToriGraph";
                const DWORD cbType = sizeof(userType);
                const DWORD cbSrc = sizeof(srcName);
                const DWORD totalSize = sizeof(OBJECTDESCRIPTOR) + cbType + cbSrc;

                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, totalSize);
                if (!hMem)
                    return E_OUTOFMEMORY;
                auto *d = static_cast<OBJECTDESCRIPTOR *>(GlobalLock(hMem));
                if (!d) {
                    GlobalFree(hMem);
                    return E_OUTOFMEMORY;
                }
                d->cbSize = totalSize;
                d->clsid = graph_clsid;
                d->dwDrawAspect = DVASPECT_CONTENT;
                d->sizel = m_extent;
                d->dwStatus = OLEMISC_RECOMPOSEONRESIZE;
                d->dwFullUserTypeName = sizeof(OBJECTDESCRIPTOR);
                d->dwSrcOfCopy = sizeof(OBJECTDESCRIPTOR) + cbType;
                BYTE *base = reinterpret_cast<BYTE *>(d);
                std::memcpy(base + d->dwFullUserTypeName, userType, cbType);
                std::memcpy(base + d->dwSrcOfCopy, srcName, cbSrc);
                GlobalUnlock(hMem);

                m->tymed = TYMED_HGLOBAL;
                m->hGlobal = hMem;
                return S_OK;
            }

            ComPtr<ILockBytes> lockBytes;
            hr = CreateILockBytesOnHGlobal(nullptr, TRUE, &lockBytes);
            if (FAILED(hr))
                return hr;
            ComPtr<IStorage> stg;
            hr = StgCreateDocfileOnILockBytes(
                    lockBytes.Get(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
            if (FAILED(hr))
                return hr;
            m_payload.extentHm = m_extent;
            hr = savePayloadToStorage(stg.Get(), m_payload);
            if (FAILED(hr))
                return hr;
            hr = stg->Commit(STGC_DEFAULT);
            if (FAILED(hr))
                return hr;
            m->tymed = TYMED_ISTORAGE;
            m->pstg = stg.Detach();
            return S_OK;
        } catch (...) {
            return E_FAIL;
        }
    }

    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *f, STGMEDIUM *m) override
    {
        ComPtr<IOleObject> alive(this);
        if (!f || !m)
            return E_POINTER;
        HRESULT hr = checkFormatEtc(f);
        if (FAILED(hr))
            return hr;
        if ((f->cfFormat != embedFormat() && f->cfFormat != objectFormat())
            || m->tymed != TYMED_ISTORAGE || !m->pstg) {
            return DV_E_TYMED;
        }
        try {
            syncFromEditor();
            m_payload.extentHm = m_extent;
            return savePayloadToStorage(m->pstg, m_payload);
        } catch (...) {
            return STG_E_WRITEFAULT;
        }
    }

    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *f) override { return checkFormatEtc(f); }

    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC *, FORMATETC *o) override
    {
        if (!o)
            return E_POINTER;
        o->ptd = nullptr;
        return DATA_S_SAMEFORMATETC;
    }

    HRESULT STDMETHODCALLTYPE SetData(FORMATETC *, STGMEDIUM *, BOOL) override { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction, IEnumFORMATETC **out) override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (direction != DATADIR_GET)
            return E_NOTIMPL;
        FORMATETC fmts[] = {
            { embedFormat(), nullptr, DVASPECT_CONTENT, -1, TYMED_ISTORAGE },
            { objectFormat(), nullptr, DVASPECT_CONTENT, -1, TYMED_ISTORAGE },
            { descriptorFormat(), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL },
            { CF_ENHMETAFILE, nullptr, DVASPECT_CONTENT, -1, TYMED_ENHMF },
            { pngClipboardFormat(), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL },
            { CF_DIB, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }
        };
        return SHCreateStdEnumFmtEtc(static_cast<UINT>(sizeof(fmts) / sizeof(fmts[0])), fmts,
                                     out);
    }

    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *f, DWORD flags, IAdviseSink *sink, DWORD *c) override
    {
        return m_dataAdvises ? m_dataAdvises->Advise(this, f, flags, sink, c) : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD c) override
    {
        return m_dataAdvises ? m_dataAdvises->Unadvise(c) : OLE_E_NOCONNECTION;
    }

    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **p) override
    {
        return m_dataAdvises ? m_dataAdvises->EnumAdvise(p) : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE Draw(DWORD aspect, LONG index, void *, DVTARGETDEVICE *, HDC, HDC dc,
                                   LPCRECTL b, LPCRECTL, BOOL(CALLBACK *cont)(ULONG_PTR),
                                   ULONG_PTR token) override
    {
        if (aspect != DVASPECT_CONTENT)
            return DV_E_DVASPECT;
        if (index != -1)
            return DV_E_LINDEX;
        if (!dc || !b)
            return E_POINTER;
        if (cont && !cont(token))
            return E_ABORT;

        ensureRenderedPayload(&m_payload);
        HENHMETAFILE hemf = SciDAVisEmf::createHemfFromBytes(m_payload.emfBytes);
        if (!hemf)
            return E_FAIL;
        RECT rc{ b->left, b->top, b->right, b->bottom };
        BOOL ok = SciDAVisEmf::drawHemfWithGdiplus(dc, hemf, rc) ? TRUE : PlayEnhMetaFile(dc, hemf, &rc);
        DeleteEnhMetaFile(hemf);
        return ok ? S_OK : E_FAIL;
    }

    HRESULT STDMETHODCALLTYPE GetColorSet(DWORD, LONG, void *, DVTARGETDEVICE *, HDC,
                                          LOGPALETTE **p) override
    {
        if (p)
            *p = nullptr;
        return S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE Freeze(DWORD, LONG, void *, DWORD *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Unfreeze(DWORD) override { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE SetAdvise(DWORD aspect, DWORD flags, IAdviseSink *sink) override
    {
        if (aspect != DVASPECT_CONTENT)
            return DV_E_DVASPECT;
        m_viewSink = sink;
        m_viewFlags = flags;
        if (sink && (flags & ADVF_PRIMEFIRST)) {
            ComPtr<IAdviseSink> s = sink;
            if (flags & ADVF_ONLYONCE)
                m_viewSink.Reset();
            s->OnViewChange(DVASPECT_CONTENT, -1);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetAdvise(DWORD *aspects, DWORD *flags, IAdviseSink **sink) override
    {
        if (aspects)
            *aspects = DVASPECT_CONTENT;
        if (flags)
            *flags = m_viewFlags;
        return sink ? m_viewSink.CopyTo(sink) : S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetExtent(DWORD a, LONG i, DVTARGETDEVICE *, LPSIZEL s) override
    {
        if (i != -1)
            return DV_E_LINDEX;
        return GetExtent(a, s);
    }
};

bool hasVisibleNormalAppWindow()
{
    if (!qApp)
        return false;
    for (QWidget *tlw : QApplication::topLevelWidgets()) {
        auto *aw = qobject_cast<ApplicationWindow *>(tlw);
        if (aw && aw->isVisible() && !aw->isOleEmbeddedMode() && !aw->isNonInteractive())
            return true;
    }
    return false;
}

void updateOleQuitGuard()
{
    auto *guiApp = qobject_cast<QApplication *>(qApp);
    if (!guiApp)
        return;

    bool activeContainer = false;
    for (const SciDAVisGraphObject *obj : g_liveGraphObjects) {
        if (obj && obj->hasActiveContainerOrEditor()) {
            activeContainer = true;
            break;
        }
    }

    const bool normalVisible = hasVisibleNormalAppWindow();
    if (activeContainer) {
        if (normalVisible)
            g_hadVisibleNormalWindowDuringSuppression = true;
        if (guiApp->quitOnLastWindowClosed()) {
            guiApp->setQuitOnLastWindowClosed(false);
            g_oleSuppressedQuitOnLastWindow = true;
        }
    } else if (g_oleSuppressedQuitOnLastWindow) {
        const bool hadNormal = g_hadVisibleNormalWindowDuringSuppression;
        g_oleSuppressedQuitOnLastWindow = false;
        g_hadVisibleNormalWindowDuringSuppression = false;
        guiApp->setQuitOnLastWindowClosed(true);
        // If a standalone GUI window was visible while an OLE session was active (or hidden via
        // OLEIVERB_HIDE) and the user closed that standalone window in the meantime, finish
        // exiting once the last OLE container session has also closed.
        if (hadNormal && !normalVisible && !ApplicationWindow::isNonInteractiveContext(nullptr)) {
            QTimer::singleShot(0, guiApp, &QCoreApplication::quit);
        }
    }
}

class SciDAVisClassFactory final : public IClassFactory
{
    ULONG m_refs = 1;

public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **p) override
    {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (id != IID_IUnknown && id != IID_IClassFactory)
            return E_NOINTERFACE;
        *p = static_cast<IClassFactory *>(this);
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refs; }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG n = --m_refs;
        if (!n)
            delete this;
        return n;
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer, REFIID id, void **p) override
    {
        if (outer)
            return CLASS_E_NOAGGREGATION;
        OleGraphPayload emptyPayload;
        return createGraphObject(emptyPayload, id, p);
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override
    {
        if (lock)
            addRefServerProcess();
        else
            releaseServerProcess();
        return S_OK;
    }
};

bool setRegDefaultString(HKEY root, const std::wstring &subkey, const std::wstring &val)
{
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return false;
    const DWORD cb = static_cast<DWORD>((val.size() + 1) * sizeof(wchar_t));
    LSTATUS st = RegSetValueExW(hKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE *>(val.c_str()), cb);
    RegCloseKey(hKey);
    return st == ERROR_SUCCESS;
}

bool setRegNamedString(HKEY root, const std::wstring &subkey, const std::wstring &name, const std::wstring &val)
{
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return false;
    const DWORD cb = static_cast<DWORD>((val.size() + 1) * sizeof(wchar_t));
    LSTATUS st = RegSetValueExW(hKey, name.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE *>(val.c_str()), cb);
    RegCloseKey(hKey);
    return st == ERROR_SUCCESS;
}

} // namespace

HRESULT savePayloadToStorage(IStorage *stg, const OleGraphPayload &payload)
{
    if (!stg)
        return E_POINTER;
    if (payload.projectBytes.size() > kMaxOleStreamBytes
        || payload.emfBytes.size() > kMaxOleStreamBytes) {
        return STG_E_INSUFFICIENTMEMORY;
    }

    HRESULT hr = WriteClassStg(stg, graph_clsid);
    if (FAILED(hr))
        return hr;
    hr = WriteFmtUserTypeStg(stg, embedFormat(), const_cast<LPOLESTR>(L"ToriGraph Graph"));
    if (FAILED(hr))
        return hr;

    hr = writeStreamBytes(stg, L"SciDAVisProject", payload.projectBytes);
    if (FAILED(hr))
        return hr;

    QJsonObject meta;
    meta[QStringLiteral("targetWindow")] = payload.targetWindow;
    meta[QStringLiteral("targetLayer")] = payload.targetLayer;
    meta[QStringLiteral("widthHm")] = static_cast<qint64>(payload.extentHm.cx);
    meta[QStringLiteral("heightHm")] = static_cast<qint64>(payload.extentHm.cy);
    meta[QStringLiteral("widthPx")] = payload.pixelSize.width();
    meta[QStringLiteral("heightPx")] = payload.pixelSize.height();
    QByteArray metaBytes = QJsonDocument(meta).toJson(QJsonDocument::Compact);

    hr = writeStreamBytes(stg, L"SciDAVisMeta", metaBytes);
    if (FAILED(hr))
        return hr;

    if (!payload.emfBytes.isEmpty()) {
        hr = writeStreamBytes(stg, L"SciDAVisEmf", payload.emfBytes);
        if (FAILED(hr))
            return hr;
    }
    return S_OK;
}

HRESULT loadPayloadFromStorage(IStorage *stg, OleGraphPayload *outPayload)
{
    if (!stg || !outPayload)
        return E_POINTER;

    OleGraphPayload p;
    HRESULT hr = readStreamBytes(stg, L"SciDAVisProject", &p.projectBytes);
    if (FAILED(hr))
        return STG_E_INVALIDHEADER;

    QByteArray metaBytes;
    if (SUCCEEDED(readStreamBytes(stg, L"SciDAVisMeta", &metaBytes)) && !metaBytes.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(metaBytes);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            p.targetWindow = obj.value(QStringLiteral("targetWindow")).toString();
            p.targetLayer = obj.value(QStringLiteral("targetLayer")).toInt(0);
            LONG wHm = static_cast<LONG>(obj.value(QStringLiteral("widthHm")).toInt(15000));
            LONG hHm = static_cast<LONG>(obj.value(QStringLiteral("heightHm")).toInt(11250));
            if (wHm > 0 && hHm > 0 && wHm <= 1000000 && hHm <= 1000000)
                p.extentHm = SIZEL{ wHm, hHm };
            int wPx = obj.value(QStringLiteral("widthPx")).toInt(500);
            int hPx = obj.value(QStringLiteral("heightPx")).toInt(375);
            if (wPx > 0 && hPx > 0)
                p.pixelSize = QSize(wPx, hPx);
        }
    }

    readStreamBytes(stg, L"SciDAVisEmf", &p.emfBytes);
    readStreamBytes(stg, L"SciDAVisDib", &p.dibBytes);

    if (p.emfBytes.isEmpty()) {
        ensureRenderedPayload(&p);
    }
    *outPayload = std::move(p);
    return S_OK;
}

HRESULT createGraphObject(const OleGraphPayload &payload, REFIID riid, void **ppvObj)
{
    if (!ppvObj)
        return E_POINTER;
    *ppvObj = nullptr;
    try {
        auto *obj = new SciDAVisGraphObject(payload);
        HRESULT hr = obj->QueryInterface(riid, ppvObj);
        obj->Release();
        return hr;
    } catch (...) {
        return E_OUTOFMEMORY;
    }
}

namespace {

const wchar_t kClsidKey[] = L"CLSID\\{8E6B9541-3C74-4D28-A915-7D84E5A12001}\\LocalServer32";

QString selfServerExecutable()
{
    QString candidateExe = QCoreApplication::applicationDirPath() + QStringLiteral("/ToriGraph.exe");
    if (!QFileInfo::exists(candidateExe))
        candidateExe = QCoreApplication::applicationDirPath() + QStringLiteral("/scidavis.exe");
    if (!QFileInfo::exists(candidateExe))
        candidateExe = QCoreApplication::applicationFilePath();
    return candidateExe;
}

QString readRegString(HKEY root, const wchar_t *subkey, const wchar_t *name)
{
    DWORD cb = 0;
    if (RegGetValueW(root, subkey, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, nullptr,
                     &cb)
                != ERROR_SUCCESS
        || cb < sizeof(wchar_t)) {
        return QString();
    }
    std::wstring buf(cb / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(root, subkey, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr,
                     buf.data(), &cb)
        != ERROR_SUCCESS) {
        return QString();
    }
    return QString::fromWCharArray(buf.c_str()).trimmed();
}

// Extracts the executable from a LocalServer32 command line such as
// "C:\Program Files\ToriGraph\ToriGraph.exe" -Embedding
QString executableFromCommand(const QString &cmd)
{
    const QString c = cmd.trimmed();
    if (c.isEmpty())
        return QString();
    if (c.startsWith(QLatin1Char('"'))) {
        const int end = c.indexOf(QLatin1Char('"'), 1);
        return end > 1 ? c.mid(1, end - 1) : c.mid(1);
    }
    const int exePos = c.indexOf(QLatin1String(".exe"), 0, Qt::CaseInsensitive);
    if (exePos > 0)
        return c.left(exePos + 4);
    const int space = c.indexOf(QLatin1Char(' '));
    return space > 0 ? c.left(space) : c;
}

bool envFlagSet(const char *name)
{
    const QByteArray v = qgetenv(name).trimmed();
    return !v.isEmpty() && v != "0" && v.toLower() != "false" && v.toLower() != "no";
}

} // namespace

RegistrationInfo queryRegistration()
{
    RegistrationInfo info;
    // HKEY_CLASSES_ROOT is the merged per-user / per-machine view that COM itself uses.
    QString exe = readRegString(HKEY_CLASSES_ROOT, kClsidKey, L"ServerExecutable");
    if (exe.isEmpty())
        exe = executableFromCommand(readRegString(HKEY_CLASSES_ROOT, kClsidKey, nullptr));
    if (exe.isEmpty()) {
        info.state = RegistrationInfo::NotRegistered;
        return info;
    }
    info.executable = QDir::fromNativeSeparators(exe);
    const QFileInfo fi(info.executable);
    info.state = (fi.exists() && fi.isFile()) ? RegistrationInfo::Valid
                                              : RegistrationInfo::MissingExecutable;
    const QFileInfo self(selfServerExecutable());
    info.pointsToSelf = fi.exists()
            && QDir::cleanPath(fi.canonicalFilePath())
                    .compare(QDir::cleanPath(self.canonicalFilePath()), Qt::CaseInsensitive)
                    == 0;
    return info;
}

bool ensureRegistrationAtStartup(QString *errMsg)
{
    if (envFlagSet("TORIGRAPH_NO_OLE_REGISTER"))
        return true;
    if (queryRegistration().state == RegistrationInfo::Valid)
        return true; // a working installation is never redirected to this executable
    return ensureRegistryRegistered(errMsg);
}

bool ensureRegistryRegistered(QString *errMsg)
{
    const QString candidateExe = selfServerExecutable();
    std::wstring exePath = QDir::toNativeSeparators(candidateExe).toStdWString();
    std::wstring serverCmd = L"\"" + exePath + L"\" -Embedding";
    std::wstring openCmd = L"\"" + exePath + L"\" \"%1\"";
    std::wstring iconVal = L"\"" + exePath + L"\",0";

    const std::wstring clsidStr = L"{8E6B9541-3C74-4D28-A915-7D84E5A12001}";
    const std::wstring clsidBase = L"Software\\Classes\\CLSID\\" + clsidStr;
    const std::wstring progBase = L"Software\\Classes\\ToriGraph.Graph.1";
    const std::wstring legacyProgBase = L"Software\\Classes\\SciDAVis.Graph.1";

    bool ok = true;
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase, L"ToriGraph Graph");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\LocalServer32", serverCmd);
    ok = ok && setRegNamedString(HKEY_CURRENT_USER, clsidBase + L"\\LocalServer32", L"ServerExecutable", exePath);
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\ProgID", L"ToriGraph.Graph.1");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\VersionIndependentProgID", L"ToriGraph.Graph");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\InprocHandler32", L"ole32.dll");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\DefaultIcon", iconVal);
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\Verb\\0", L"\u7de8\u96c6(&E),0,2");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\Verb\\1", L"\u958b\u304f(&O),0,2");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\DataFormats\\GetSet\\0", L"Embed Source,1,8,1");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\DataFormats\\GetSet\\1", L"Embedded Object,1,8,1");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\DataFormats\\GetSet\\2", L"Object Descriptor,1,1,1");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\DataFormats\\GetSet\\3", L"14,1,64,1");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\MiscStatus", L"0");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\MiscStatus\\1", L"1");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\AuxUserType\\2", L"ToriGraph");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\AuxUserType\\3", L"ToriGraph");
    ok = ok && setRegDefaultString(HKEY_CURRENT_USER, clsidBase + L"\\Insertable", L"");

    for (const auto &pBase : { progBase, legacyProgBase }) {
        ok = ok && setRegDefaultString(HKEY_CURRENT_USER, pBase, L"ToriGraph Graph");
        ok = ok && setRegDefaultString(HKEY_CURRENT_USER, pBase + L"\\CLSID", clsidStr);
        ok = ok && setRegDefaultString(HKEY_CURRENT_USER, pBase + L"\\Insertable", L"");
        ok = ok && setRegDefaultString(HKEY_CURRENT_USER, pBase + L"\\shell\\open\\command", openCmd);
    }

    if (!ok && errMsg)
        *errMsg = QStringLiteral("Failed to write HKCU OLE registration keys.");
    return ok;
}

bool initializeOleClipboardOnly()
{
    if (!g_oleInitialized) {
        HRESULT hr = OleInitialize(nullptr);
        if (FAILED(hr))
            return false;
        g_oleInitialized = true;
    }
    return true;
}

bool initializeOleServer(bool embeddedMode)
{
    if (!g_oleInitialized) {
        HRESULT hr = OleInitialize(nullptr);
        if (FAILED(hr))
            return false;
        g_oleInitialized = true;
    }

    g_embeddedServerMode = embeddedMode;
    if (embeddedMode) {
        g_everHadServerActivity = false;
    } else if (!g_normalSessionLockHeld) {
        CoAddRefServerProcess();
        g_normalSessionLockHeld = true;
    }

    if (g_factoryCookie == 0) {
        auto *factory = new SciDAVisClassFactory();
        HRESULT hr = CoRegisterClassObject(
                graph_clsid, factory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &g_factoryCookie);
        factory->Release();
        if (FAILED(hr))
            return false;
    }

    if (embeddedMode && qApp) {
        // Exit zombie -Embedding server processes if no object creation or lock occurs within 30s
        QTimer::singleShot(30000, qApp, []() {
            if (!g_embeddedServerMode || g_everHadServerActivity)
                return;
            for (QWidget *tlw : QApplication::topLevelWidgets()) {
                if (tlw && tlw->isVisible())
                    return;
            }
            QCoreApplication::quit();
        });
    }
    return true;
}

static void pumpClipboardMessages(int durationMs = 50)
{
    ULONGLONG endTick = GetTickCount64() + static_cast<ULONGLONG>(durationMs);
    do {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (GetTickCount64() < endTick)
            Sleep(10);
    } while (GetTickCount64() < endTick);
}

static HRESULT setOleClipboardWithRetry(IDataObject *dataObj)
{
    HRESULT hr = E_FAIL;
    for (int attempt = 0; attempt < 20; ++attempt) {
        pumpClipboardMessages(15);
        hr = OleSetClipboard(dataObj);
        if (SUCCEEDED(hr)) {
            g_lastOleSetClipboardSeq = GetClipboardSequenceNumber();
            pumpClipboardMessages(150);
            return hr;
        }
        Sleep(25);
    }
    return hr;
}

static void flushOleClipboardWithRetry(IDataObject *dataObj)
{
    if (!dataObj)
        return;
    for (int attempt = 0; attempt < 20; ++attempt) {
        pumpClipboardMessages(20);
        if (OleIsCurrentClipboard(dataObj) != S_OK)
            return;
        HRESULT hr = OleFlushClipboard();
        if (SUCCEEDED(hr)) {
            pumpClipboardMessages(20);
            return;
        }
        Sleep(25);
    }
}

DWORD lastCopyClipboardSequence()
{
    return g_clipboardDataObject ? g_lastOleSetClipboardSeq : 0;
}

bool lastCopyStillOnClipboard()
{
    return g_clipboardDataObject && OleIsCurrentClipboard(g_clipboardDataObject.Get()) == S_OK;
}

void shutdownOleServer()
{
    g_oleSuppressedQuitOnLastWindow = false;
    g_hadVisibleNormalWindowDuringSuppression = false;
    if (g_clipboardDataObject) {
        flushOleClipboardWithRetry(g_clipboardDataObject.Get());
        g_clipboardDataObject.Reset();
    }
    if (g_factoryCookie != 0) {
        CoRevokeClassObject(g_factoryCookie);
        g_factoryCookie = 0;
    }
    if (g_normalSessionLockHeld) {
        CoReleaseServerProcess();
        g_normalSessionLockHeld = false;
    }
    if (g_oleInitialized) {
        OleUninitialize();
        g_oleInitialized = false;
    }
}

CLIPFORMAT pngClipboardFormat()
{
    static CLIPFORMAT cf = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"PNG"));
    return cf;
}

namespace {

// Resolution of the PNG offered with a copied graph: 600 dpi of the physical size, lowered in
// steps (but not below 300 dpi) so that the picture stays within about 24 million pixels.
// Only pages too large for 300 dpi within that budget go below 300 dpi.
int choosePngDpi(SIZEL extentHm)
{
    if (extentHm.cx <= 0 || extentHm.cy <= 0)
        return 0;
    const double wIn = double(extentHm.cx) / 2540.0;
    const double hIn = double(extentHm.cy) / 2540.0;
    constexpr double kMaxPixels = 24.0e6;
    constexpr double kMaxSide = 16000.0;
    auto fits = [&](int dpi) {
        const double w = wIn * dpi;
        const double h = hIn * dpi;
        return w * h <= kMaxPixels && w <= kMaxSide && h <= kMaxSide;
    };
    int dpi = 600;
    while (dpi > 300 && !fits(dpi))
        dpi -= 25;
    while (dpi > 72 && !fits(dpi))
        dpi -= 12;
    return dpi;
}

} // namespace

QByteArray renderEmfToPng(const QByteArray &emfBytes, SIZEL extentHm, int *outDpi)
{
    if (outDpi)
        *outDpi = 0;
    HENHMETAFILE hemf = SciDAVisEmf::createHemfFromBytes(emfBytes);
    if (!hemf)
        return {};
    if (extentHm.cx <= 0 || extentHm.cy <= 0) {
        ENHMETAHEADER hdr{};
        if (GetEnhMetaFileHeader(hemf, sizeof(hdr), &hdr) != 0) {
            extentHm = SIZEL{ hdr.rclFrame.right - hdr.rclFrame.left,
                              hdr.rclFrame.bottom - hdr.rclFrame.top };
        }
    }
    const int dpi = choosePngDpi(extentHm);
    if (dpi <= 0) {
        DeleteEnhMetaFile(hemf);
        return {};
    }
    const int w = std::max(16, static_cast<int>(std::lround(extentHm.cx / 2540.0 * dpi)));
    const int h = std::max(16, static_cast<int>(std::lround(extentHm.cy / 2540.0 * dpi)));
    const int dpm = static_cast<int>(std::lround(dpi / 0.0254));

    // Render onto a transparent 32-bit premultiplied ARGB surface via GDI+ so graphs whose page
    // background has alpha < 255 (e.g. transparent background, alpha == 0) retain transparency in
    // the PNG instead of being flattened onto opaque white.
    QByteArray png;
    if (SciDAVisEmf::ensureGdiplusInitialized()) {
        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        bool okGdip = false;
        {
            Gdiplus::Bitmap bmp(w, h, img.bytesPerLine(), PixelFormat32bppPARGB, img.bits());
            Gdiplus::Graphics gfx(&bmp);
            Gdiplus::Metafile meta(hemf, FALSE);
            if (bmp.GetLastStatus() == Gdiplus::Ok && gfx.GetLastStatus() == Gdiplus::Ok
                && meta.GetLastStatus() == Gdiplus::Ok) {
                gfx.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
                gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                gfx.SetPixelOffsetMode(Gdiplus::PixelOffsetModeDefault);
                gfx.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                gfx.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
                gfx.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
                if (gfx.DrawImage(&meta, Gdiplus::Rect(0, 0, w, h)) == Gdiplus::Ok) {
                    gfx.Flush(Gdiplus::FlushIntentionSync);
                    okGdip = true;
                }
            }
        }
        if (okGdip) {
            img.setDotsPerMeterX(dpm);
            img.setDotsPerMeterY(dpm);
            QBuffer buf(&png);
            if (buf.open(QIODevice::WriteOnly))
                img.save(&buf, "PNG");
        }
    }

    if (png.isEmpty()) {
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top-down rows, as QImage expects
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void *bits = nullptr;
        HDC screenDc = GetDC(nullptr);
        HDC memDc = CreateCompatibleDC(screenDc);
        HBITMAP hBmp = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (screenDc)
            ReleaseDC(nullptr, screenDc);

        if (memDc && hBmp && bits) {
            std::memset(bits, 0xFF, static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
            HGDIOBJ oldObj = SelectObject(memDc, hBmp);
            RECT rc{ 0, 0, w, h };
            if (!SciDAVisEmf::drawHemfWithGdiplus(memDc, hemf, rc))
                PlayEnhMetaFile(memDc, hemf, &rc);
            GdiFlush();
            SelectObject(memDc, oldObj);
            QImage img(static_cast<uchar *>(bits), w, h, w * 4, QImage::Format_RGB32);
            img.setDotsPerMeterX(dpm);
            img.setDotsPerMeterY(dpm);
            QBuffer buf(&png);
            if (buf.open(QIODevice::WriteOnly))
                img.save(&buf, "PNG");
        }
        if (hBmp)
            DeleteObject(hBmp);
        if (memDc)
            DeleteDC(memDc);
    }
    DeleteEnhMetaFile(hemf);
    if (outDpi && !png.isEmpty())
        *outDpi = dpi;
    return png;
}

EmbedScope embedScopeSetting()
{
    const QString v = ApplicationWindow::getSettings()
                              .value(QStringLiteral("/OLE/EmbedScope"), QStringLiteral("graph"))
                              .toString()
                              .trimmed()
                              .toLower();
    return v == QLatin1String("project") ? EmbedScope::WholeProject : EmbedScope::GraphOnly;
}

void setEmbedScopeSetting(EmbedScope scope)
{
    ApplicationWindow::getSettings().setValue(
            QStringLiteral("/OLE/EmbedScope"),
            scope == EmbedScope::WholeProject ? QStringLiteral("project")
                                              : QStringLiteral("graph"));
}

QStringList dataDependenciesForWindow(ApplicationWindow *app, MyWidget *w)
{
    QStringList names;
    if (!app || !w)
        return names;
    auto add = [&](const QString &n) {
        if (!n.isEmpty() && n != w->name() && !names.contains(n))
            names << n;
    };
    // Curve titles and column names have the form "<table>_<column>"; table() resolves both.
    auto addTableOf = [&](const QString &spec) {
        if (spec.isEmpty())
            return;
        if (Table *t = app->table(spec))
            add(t->name());
    };

    if (w->inherits("MultiLayer")) {
        MultiLayer *ml = static_cast<MultiLayer *>(w);
        for (Graph *g : ml->layersList()) {
            if (!g)
                continue;
            const int n = g->curves();
            for (int i = 0; i < n; ++i) {
                QwtPlotItem *it = g->plotItem(i);
                if (!it)
                    continue;
                if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                    if (Matrix *m = static_cast<Spectrogram *>(it)->matrix())
                        add(m->name());
                    continue;
                }
                if (auto *dc = dynamic_cast<DataCurve *>(it)) {
                    if (dc->table())
                        add(dc->table()->name());
                    addTableOf(dc->xColumnName());
                }
            }
            for (const QString &c : g->curvesList())
                addTableOf(c);
            if (Plot *pl = g->plotWidget()) {
                const QStringList extraTags = pl->property("_toriExtraPlotTags").toStringList();
                static const QRegularExpression extraAttrRx(
                        QStringLiteral("(table|matrix|sourceMatrix)=\"([^\"]*)\""));
                auto unescXmlAttr = [](QString v) {
                    v.replace(QLatin1String("&#10;"), QLatin1String("\n"));
                    v.replace(QLatin1String("&quot;"), QLatin1String("\""));
                    v.replace(QLatin1String("&lt;"), QLatin1String("<"));
                    v.replace(QLatin1String("&gt;"), QLatin1String(">"));
                    v.replace(QLatin1String("&amp;"), QLatin1String("&"));
                    return v.trimmed();
                };
                for (const QString &rawTag : extraTags) {
                    const QString trimmedTag = rawTag.trimmed();
                    if (!trimmedTag.startsWith(QLatin1String("<ToriDensity2D"))
                        && !trimmedTag.startsWith(QLatin1String("<ToriPoleFigure"))) {
                        continue;
                    }
                    auto mit = extraAttrRx.globalMatch(trimmedTag);
                    while (mit.hasNext()) {
                        const QRegularExpressionMatch m = mit.next();
                        const QString key = m.captured(1);
                        const QString val = unescXmlAttr(m.captured(2));
                        if (val.isEmpty())
                            continue;
                        if (key == QLatin1String("table")) {
                            if (Table *t = app->table(val))
                                add(t->name());
                            else
                                add(val);
                        } else {
                            if (Matrix *mat = app->matrix(val))
                                add(mat->name());
                            else
                                add(val);
                        }
                    }
                }
            }
        }
    } else if (w->inherits("Graph3D")) {
        Graph3D *g3 = static_cast<Graph3D *>(w);
        if (Matrix *m = g3->matrix())
            add(m->name());
        if (Table *t = g3->getTable())
            add(t->name());
        // Table association of a 3D plot: "Table1_x(X),Table1_y(Y)[,Table1_z(Z)]".
        static const QRegularExpression assocRx(QStringLiteral("([^,]+)\\((X|Y|Z)\\)"));
        auto it = assocRx.globalMatch(g3->formula());
        while (it.hasNext())
            addTableOf(it.next().captured(1).trimmed());
    }

    // Column formulas may read other tables (col("Table2/B"), tablecol("Table2", "B"),
    // cell("Table2/B", i) ...). Those tables are needed to recalculate the embedded copy, so
    // follow the references (transitively) from every table collected so far.
    auto exactTable = [&](const QString &name) -> Table * {
        if (name.isEmpty())
            return nullptr;
        Table *t = app->table(name);
        return (t && t->name() == name) ? t : nullptr;
    };
    static const QRegularExpression quotedRx(QStringLiteral("\"((?:[^\"\\\\]|\\\\.)*)\""));
    QStringList queue;
    for (const QString &n : names)
        if (exactTable(n))
            queue << n;
    if (w->inherits("Table"))
        queue << w->name();
    QStringList visited;
    while (!queue.isEmpty()) {
        const QString tn = queue.takeFirst();
        if (visited.contains(tn))
            continue;
        visited << tn;
        Table *t = exactTable(tn);
        if (!t)
            continue;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!col)
                continue;
            for (const Interval<int> &iv : col->formulaIntervals()) {
                const QString formula = col->formula(iv.start());
                auto qit = quotedRx.globalMatch(formula);
                while (qit.hasNext()) {
                    // "\/" is an escaped slash inside a table or column name
                    QString arg = qit.next().captured(1);
                    arg.replace(QStringLiteral("\\/"), QString(QChar(0x1F)));
                    QStringList candidates;
                    candidates << arg; // tablecol("Table2", ...), column__("Table2", ...)
                    const QStringList parts = arg.split(QLatin1Char('/'));
                    if (parts.size() >= 2)
                        candidates << parts.at(parts.size() - 2); // "[folder/]Table2/B"
                    for (QString cand : candidates) {
                        cand.replace(QChar(0x1F), QLatin1Char('/'));
                        if (Table *ref = exactTable(cand)) {
                            add(ref->name());
                            if (!visited.contains(ref->name()))
                                queue << ref->name();
                        }
                    }
                }
            }
        }
    }
    return names;
}

QByteArray buildEmbeddedProjectBytes(ApplicationWindow *app, MyWidget *target, EmbedScope scope,
                                     QStringList *embeddedWindows)
{
    if (embeddedWindows)
        embeddedWindows->clear();
    if (!app)
        return {};
    if (scope == EmbedScope::WholeProject || !target)
        return app->saveProjectToByteArray();

    QList<MyWidget *> windows;
    for (const QString &n : dataDependenciesForWindow(app, target)) {
        MyWidget *dep = nullptr;
        if (Table *t = app->table(n))
            dep = (t->name() == n) ? t : nullptr;
        if (!dep) {
            if (Matrix *m = app->matrix(n))
                dep = m;
        }
        if (dep && dep != target && !windows.contains(dep))
            windows << dep;
    }
    windows << target; // data first, so that the graph finds its curves when loaded
    if (embeddedWindows) {
        for (MyWidget *w : windows)
            *embeddedWindows << w->name();
    }
    return app->saveWindowsToByteArray(windows);
}

namespace {

ApplicationWindow *resolveApplicationWindow(ApplicationWindow *app, QWidget *w)
{
    if (app)
        return app;
    QWidget *p = w ? w->parentWidget() : nullptr;
    while (p && !qobject_cast<ApplicationWindow *>(p))
        p = p->parentWidget();
    if (auto *found = qobject_cast<ApplicationWindow *>(p))
        return found;
    for (QWidget *top : QApplication::topLevelWidgets()) {
        if (auto *cand = qobject_cast<ApplicationWindow *>(top))
            return cand;
    }
    return nullptr;
}

bool isProductionToriGraphExe()
{
    const QString selfName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    return selfName.compare(QStringLiteral("ToriGraph.exe"), Qt::CaseInsensitive) == 0;
}

QString registrationProblemText(const RegistrationInfo &reg)
{
    if (reg.state == RegistrationInfo::Valid && !reg.pointsToSelf) {
        return QObject::tr("The PowerPoint link currently points to another ToriGraph installation "
                           "(%1) instead of this program (%2). Double-clicking the graph in "
                           "PowerPoint will open that copy.")
                .arg(QDir::toNativeSeparators(reg.executable),
                     QDir::toNativeSeparators(selfServerExecutable()));
    }
    return QObject::tr("The PowerPoint link of ToriGraph is not registered, or points to a "
                       "program that no longer exists (%1). Pasting works, but double-clicking "
                       "the graph in PowerPoint will not open ToriGraph.")
            .arg(reg.executable.isEmpty() ? QStringLiteral("-")
                                          : QDir::toNativeSeparators(reg.executable));
}

// Puts the editable object on the clipboard; if that is impossible, puts a static picture
// (EMF + PNG + DIB) there instead and records why.
void offerPayloadOnClipboard(OleGraphPayload &payload, OleCopyResult &r)
{
    QString reason;
    if (qApp && qApp->property("torigraph_test_force_ole_copy_failure").toBool()) {
        // Regression tests exercise the static fallback without breaking the real clipboard.
        reason = QStringLiteral("forced failure (test)");
    } else if (payload.projectBytes.size() > kMaxOleStreamBytes
               || payload.emfBytes.size() > kMaxOleStreamBytes) {
        const qint64 maxSz = std::max<qint64>(payload.projectBytes.size(), payload.emfBytes.size());
        reason = QObject::tr("The embedded project data exceeds the 128 MB PowerPoint limit (%1 MB).")
                         .arg(double(maxSz) / 1.0e6, 0, 'f', 1);
    } else if (!g_oleInitialized) {
        reason = QObject::tr("OLE could not be initialized in this ToriGraph session. Restart "
                             "ToriGraph and try again.");
    } else {
        ComPtr<IDataObject> dataObj;
        HRESULT hr = createGraphObject(payload, IID_PPV_ARGS(&dataObj));
        if (FAILED(hr) || !dataObj) {
            reason = QObject::tr("The embedded object could not be created.");
        } else {
            hr = setOleClipboardWithRetry(dataObj.Get());
            if (FAILED(hr)) {
                reason = QObject::tr("The clipboard is in use by another program. Wait a moment "
                                     "and try again.");
                if (hr != CLIPBRD_E_CANT_OPEN)
                    reason += QStringLiteral(" (0x%1)").arg(static_cast<quint32>(hr), 8, 16,
                                                            QLatin1Char('0'));
            } else {
                g_clipboardDataObject = dataObj;
                r.ok = true;
            }
        }
    }

    const RegistrationInfo reg = queryRegistration();
    QString regProblemMsg;
    r.registrationProblem = hasRegistrationProblem(reg, &regProblemMsg);

    if (r.ok) {
        r.pngDpi = choosePngDpi(naturalPayloadExtentHm(payload));
        if (r.registrationProblem)
            r.errorReason = regProblemMsg;
        return;
    }

    r.errorReason = reason;
    int dpi = 0;
    const QByteArray png = renderEmfToPng(payload.emfBytes, naturalPayloadExtentHm(payload), &dpi);
    QByteArray dib;
    if (!png.isEmpty())
        dib = SciDAVisEmf::imageToDibBytes(QImage::fromData(png, "PNG"));
    r.staticFallbackOnClipboard = placeStaticGraphOnClipboard(payload.emfBytes, png, dib);
    if (r.staticFallbackOnClipboard) {
        r.pngDpi = dpi;
        g_clipboardDataObject.Reset();
    }
}

} // namespace

OleCopyResult copyGraphForPowerPoint(ApplicationWindow *app, MultiLayer *ml, Graph *singleLayer)
{
    ToriGraphOleUi::registerJapaneseStrings();
    OleCopyResult r;
    r.scope = embedScopeSetting();
    if (!ml) {
        r.errorReason = ToriGraphOleUi::selectGraphFirstMessage();
        return r;
    }
    r.windowName = ml->name();
    app = resolveApplicationWindow(app, ml);

    for (Graph *lg : ml->layersList()) {
        if (lg) {
            lg->updatePlot();
            lg->replot();
        }
    }

    OleGraphPayload payload;
    payload.targetWindow = ml->name();
    payload.targetLayer = 0;
    if (singleLayer) {
        for (int i = 1; i <= ml->layers(); ++i) {
            if (ml->layer(i) == singleLayer) {
                payload.targetLayer = i;
                break;
            }
        }
    }

    OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml, singleLayer, false);
    payload.emfBytes = rg.emfBytes;
    payload.dibBytes = rg.dibBytes;
    payload.extentHm = rg.extentHm;
    payload.pixelSize = rg.pixelSize;
    r.widthMm = rg.extentHm.cx / 100.0;
    r.heightMm = rg.extentHm.cy / 100.0;
    if (payload.emfBytes.isEmpty()) {
        r.errorReason = QObject::tr("The graph could not be drawn as a vector picture (EMF).");
        return r;
    }

    if (!app) {
        r.errorReason = QObject::tr("The embedded object could not be created.");
        int dpi = 0;
        const QByteArray png = renderEmfToPng(payload.emfBytes, payload.extentHm, &dpi);
        r.staticFallbackOnClipboard = placeStaticGraphOnClipboard(
                payload.emfBytes, png,
                png.isEmpty() ? QByteArray()
                              : SciDAVisEmf::imageToDibBytes(QImage::fromData(png, "PNG")));
        r.pngDpi = r.staticFallbackOnClipboard ? dpi : 0;
        return r;
    }

    payload.projectBytes = buildEmbeddedProjectBytes(app, ml, r.scope, &r.embeddedWindows);
    if (payload.projectBytes.size() > kMaxOleStreamBytes
        && r.scope == EmbedScope::WholeProject) {
        payload.projectBytes =
                buildEmbeddedProjectBytes(app, ml, EmbedScope::GraphOnly, &r.embeddedWindows);
        r.scope = EmbedScope::GraphOnly;
    }
    offerPayloadOnClipboard(payload, r);
    return r;
}

OleCopyResult copyImageForPowerPoint(ApplicationWindow *app, const QImage &img,
                                     const QString &windowName)
{
    ToriGraphOleUi::registerJapaneseStrings();
    OleCopyResult r;
    r.scope = embedScopeSetting();
    r.windowName = windowName;
    if (img.isNull()) {
        r.errorReason = QObject::tr("The graph could not be drawn as a picture.");
        return r;
    }
    app = resolveApplicationWindow(app, nullptr);

    OleGraphPayload payload;
    payload.targetWindow = windowName;
    payload.targetLayer = 0;
    OleRenderedGraph rg = SciDAVisEmf::renderFromImage(img);
    payload.emfBytes = rg.emfBytes;
    payload.dibBytes = rg.dibBytes;
    payload.extentHm = rg.extentHm;
    payload.pixelSize = rg.pixelSize;
    r.widthMm = rg.extentHm.cx / 100.0;
    r.heightMm = rg.extentHm.cy / 100.0;
    if (payload.emfBytes.isEmpty()) {
        r.errorReason = QObject::tr("The graph could not be drawn as a picture.");
        return r;
    }

    if (app) {
        MyWidget *target = nullptr;
        for (MyWidget *w : app->windowsList()) {
            if (w && w->name() == windowName) {
                target = w;
                break;
            }
        }
        payload.projectBytes = buildEmbeddedProjectBytes(app, target, r.scope, &r.embeddedWindows);
        if (!target) {
            r.scope = EmbedScope::WholeProject;
        } else if (payload.projectBytes.size() > kMaxOleStreamBytes
                   && r.scope == EmbedScope::WholeProject) {
            payload.projectBytes = buildEmbeddedProjectBytes(app, target, EmbedScope::GraphOnly,
                                                             &r.embeddedWindows);
            r.scope = EmbedScope::GraphOnly;
        }
    }
    offerPayloadOnClipboard(payload, r);
    return r;
}

void reportCopyResult(ApplicationWindow *app, const OleCopyResult &r, bool showDialog)
{
    ToriGraphOleUi::registerJapaneseStrings();
    QString statusMsg;
    QString dialogMsg;
    if (r.ok) {
        statusMsg = QObject::tr("Copied %1 for PowerPoint (%2 x %3 mm). Paste with Ctrl+V; "
                                "double-click it on the slide to edit it again.")
                            .arg(r.windowName)
                            .arg(r.widthMm, 0, 'f', 1)
                            .arg(r.heightMm, 0, 'f', 1);
        statusMsg += QLatin1Char(' ');
        statusMsg += (r.scope == EmbedScope::WholeProject)
                ? QObject::tr("Embedded: the whole project.")
                : QObject::tr("Embedded: %1.").arg(r.embeddedWindows.join(QStringLiteral(", ")));
        if (r.registrationProblem)
            dialogMsg = statusMsg + QStringLiteral("\n\n") + r.errorReason;
    } else {
        const QString head = r.windowName.isEmpty()
                ? r.errorReason
                : QObject::tr("Could not copy %1 as an editable object for PowerPoint.")
                                  .arg(r.windowName)
                        + QLatin1Char(' ') + r.errorReason;
        // pngDpi <= 0: the caller fell back to a plain bitmap copy (3D graphs).
        const QString tail = !r.staticFallbackOnClipboard
                ? QObject::tr("Nothing was placed on the clipboard.")
                : (r.pngDpi <= 0
                           ? QObject::tr("A picture of the graph (bitmap only) was placed on the "
                                         "clipboard instead. It can be pasted, but not edited "
                                         "again from PowerPoint.")
                           : QObject::tr("A picture of the graph (EMF vector and %1 dpi PNG) was "
                                         "placed on the clipboard instead. It can be pasted, but "
                                         "not edited again from PowerPoint.")
                                     .arg(r.pngDpi));
        statusMsg = head + QLatin1Char(' ') + tail;
        dialogMsg = head + QStringLiteral("\n\n") + tail;
        if (r.registrationProblem && !r.windowName.isEmpty()) {
            const RegistrationInfo reg = queryRegistration();
            dialogMsg += QStringLiteral("\n\n") + registrationProblemText(reg);
        }
    }
    if (r.registrationProblem && !r.windowName.isEmpty())
        dialogMsg += QStringLiteral("\n\n")
                + QObject::tr("To fix this, click \"Repair PowerPoint Link\" (also in the Help "
                              "menu).");

    if (qApp) {
        qApp->setProperty("torigraph_last_ole_copy_message", statusMsg);
        qApp->setProperty("torigraph_last_ole_copy_dialog", dialogMsg);
    }
    if (app && app->statusBar())
        app->statusBar()->showMessage(statusMsg, r.ok ? 12000 : 20000);

    if (!showDialog || dialogMsg.isEmpty() || ApplicationWindow::isNonInteractiveContext(app))
        return;
    QMessageBox box(r.ok ? QMessageBox::Information : QMessageBox::Warning,
                    ToriGraphOleUi::copyForPowerPointLabel(), dialogMsg, QMessageBox::NoButton,
                    app);
    QPushButton *repair = nullptr;
    if (r.registrationProblem)
        repair = box.addButton(QObject::tr("Repair PowerPoint Link"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.exec();
    if (repair && box.clickedButton() == repair)
        repairRegistrationInteractive(app);
}

bool hasRegistrationProblem(const RegistrationInfo &reg, QString *problemTextOut)
{
    ToriGraphOleUi::registerJapaneseStrings();
    if (problemTextOut)
        problemTextOut->clear();
    const bool mismatchOnProdExe =
            (reg.state == RegistrationInfo::Valid && !reg.pointsToSelf && isProductionToriGraphExe());
    const bool problem = (reg.state != RegistrationInfo::Valid) || mismatchOnProdExe;
    if (problem && problemTextOut)
        *problemTextOut = registrationProblemText(reg);
    return problem;
}

bool repairRegistrationInteractive(ApplicationWindow *app)
{
    ToriGraphOleUi::registerJapaneseStrings();
    QString err;
    const bool ok = ensureRegistryRegistered(&err);
    const QString msg = ok
            ? QObject::tr("PowerPoint link repaired. Graphs pasted into PowerPoint will open "
                          "in:\n%1")
                      .arg(QDir::toNativeSeparators(selfServerExecutable()))
            : QObject::tr("The PowerPoint link could not be registered: %1").arg(err);
    if (qApp)
        qApp->setProperty("torigraph_last_ole_repair_message", msg);
    if (app && app->statusBar())
        app->statusBar()->showMessage(QString(msg).replace(QLatin1Char('\n'), QLatin1Char(' ')),
                                      12000);
    if (!ApplicationWindow::isNonInteractiveContext(app)) {
        QMessageBox::information(app, QObject::tr("Repair PowerPoint Link (OLE Registration)"),
                                 msg);
    }
    return ok;
}

bool copyGraphAsOle(ApplicationWindow *app, MultiLayer *ml, Graph *singleLayer)
{
    return copyGraphForPowerPoint(app, ml, singleLayer).ok;
}

bool copyImageAsOle(ApplicationWindow *app, const QImage &img, const QString &windowName)
{
    return copyImageForPowerPoint(app, img, windowName).ok;
}

} // namespace SciDAVisOle

#endif // Q_OS_WIN
