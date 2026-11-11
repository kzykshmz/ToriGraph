#include "OleServer.h"

#ifdef Q_OS_WIN

#include "EmfPaintDevice.h"
#include "ApplicationWindow.h"
#include "MultiLayer.h"
#include "Graph.h"
#include "Table.h"


#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTemporaryFile>
#include <QTimer>

extern "C" HRESULT STDAPICALLTYPE SHCreateStdEnumFmtEtc(UINT cfmt, const FORMATETC afmt[], IEnumFORMATETC **ppenumFormatEtc);
#include <wrl/client.h>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

namespace SciDAVisOle {

namespace {

bool g_oleInitialized = false;
bool g_embeddedServerMode = false;
bool g_normalSessionLockHeld = false;
DWORD g_factoryCookie = 0;
ComPtr<IDataObject> g_clipboardDataObject;

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
    CoAddRefServerProcess();
}

void releaseServerProcess()
{
    if (CoReleaseServerProcess() == 0) {
        if (g_embeddedServerMode && qApp) {
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
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

MultiLayer *findTargetMultiLayer(ApplicationWindow *app, const QString &targetName)
{
    if (!app)
        return nullptr;
    QList<MyWidget *> windows = app->windowsList();
    MultiLayer *firstMl = nullptr;
    for (MyWidget *w : windows) {
        if (w && w->inherits("MultiLayer")) {
            MultiLayer *ml = static_cast<MultiLayer *>(w);
            if (!firstMl)
                firstMl = ml;
            if (!targetName.isEmpty() && ml->name() == targetName)
                return ml;
        }
    }
    return firstMl;
}

ApplicationWindow *createSampleAppWindow()
{
    ApplicationWindow *mw = new ApplicationWindow();
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

    static std::atomic<unsigned long long> s_oleTempSeq{ 0 };
    QString tempPath = QDir::tempPath()
            + QString("/torigraph_ole_%1_%2.sciprj")
                      .arg(GetTickCount64())
                      .arg(++s_oleTempSeq);
    {
        QFile f(tempPath);
        if (!f.open(QIODevice::WriteOnly))
            return nullptr;
        f.write(projectBytes);
        f.flush();
        f.close();
    }

    std::unique_ptr<ApplicationWindow> mw(new ApplicationWindow());
    mw->applyUserSettings();
    bool ok = mw->loadProject(tempPath);
    QFile::remove(tempPath);
    if (!ok) {
        mw->setOleEmbeddedMode(true);
        return nullptr;
    }
    mw->savedProject();
    return mw.release();
}

void ensureRenderedPayload(OleGraphPayload *payload)
{
    if (!payload)
        return;
    if (!payload->emfBytes.isEmpty() && payload->extentHm.cx > 0 && payload->extentHm.cy > 0)
        return;

    ApplicationWindow *mw = openAppWindowFromBytes(payload->projectBytes);
    if (!mw)
        mw = createSampleAppWindow();

    if (mw) {
        if (payload->projectBytes.isEmpty())
            payload->projectBytes = mw->saveProjectToByteArray();

        MultiLayer *ml = findTargetMultiLayer(mw, payload->targetWindow);
        if (ml) {
            if (payload->targetWindow.isEmpty())
                payload->targetWindow = ml->name();
            Graph *single = (payload->targetLayer >= 1 && payload->targetLayer <= ml->layers())
                    ? ml->layer(payload->targetLayer)
                    : nullptr;
            OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml, single);
            payload->emfBytes = rg.emfBytes;
            payload->dibBytes = rg.dibBytes;
            payload->pixelSize = rg.pixelSize;
            if (payload->extentHm.cx <= 0 || payload->extentHm.cy <= 0)
                payload->extentHm = rg.extentHm;
        }
        mw->setOleEmbeddedMode(true);
        mw->close();
        delete mw;
    }

    if (payload->emfBytes.isEmpty()) {
        OleRenderedGraph rg = SciDAVisEmf::renderFromImage(QImage());
        payload->emfBytes = rg.emfBytes;
        payload->dibBytes = rg.dibBytes;
        payload->pixelSize = rg.pixelSize;
        payload->extentHm = rg.extentHm;
    }
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
    SIZEL m_extent{ 15000, 11250 };
    SIZEL m_initialRenderExtentHm{ 0, 0 };
    QPointer<ApplicationWindow> m_editor;
    std::wstring m_filename;

    ComPtr<IOleClientSite> m_site;
    ComPtr<IStorage> m_storage;
    ComPtr<IOleAdviseHolder> m_oleAdvises;
    ComPtr<IDataAdviseHolder> m_dataAdvises;
    ComPtr<IAdviseSink> m_viewSink;
    DWORD m_viewFlags = 0;

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

    bool syncFromEditor()
    {
        if (!m_editor)
            return false;

        QByteArray newProj = m_editor->saveProjectToByteArray();
        MultiLayer *ml = findTargetMultiLayer(m_editor, m_payload.targetWindow);
        QByteArray newEmf = m_payload.emfBytes;
        QByteArray newDib = m_payload.dibBytes;
        QSize newPx = m_payload.pixelSize;

        if (ml) {
            m_payload.targetWindow = ml->name();
            Graph *single = (m_payload.targetLayer >= 1 && m_payload.targetLayer <= ml->layers())
                    ? ml->layer(m_payload.targetLayer)
                    : nullptr;
            OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml, single);
            if (!rg.emfBytes.isEmpty()) {
                newEmf = rg.emfBytes;
                newDib = rg.dibBytes;
                newPx = rg.pixelSize;
                if (rg.extentHm.cx > 0 && rg.extentHm.cy > 0
                    && m_initialRenderExtentHm.cx > 0 && m_initialRenderExtentHm.cy > 0
                    && (rg.extentHm.cx != m_initialRenderExtentHm.cx
                        || rg.extentHm.cy != m_initialRenderExtentHm.cy)) {
                    m_extent = rg.extentHm;
                    m_initialRenderExtentHm = rg.extentHm;
                }
            }
        }

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
        return changed;
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
        return DV_E_FORMATETC;
    }

public:
    explicit SciDAVisGraphObject(OleGraphPayload payload)
        : m_payload(std::move(payload))
    {
        ensureRenderedPayload(&m_payload);
        if (m_payload.extentHm.cx > 0 && m_payload.extentHm.cy > 0)
            m_extent = m_payload.extentHm;
        addRefServerProcess();
        CreateOleAdviseHolder(&m_oleAdvises);
        CreateDataAdviseHolder(&m_dataAdvises);
    }

    ~SciDAVisGraphObject()
    {
        releaseServerProcess();
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
                s->SaveObject();
            }
        }

        if (m_editor) {
            QPointer<ApplicationWindow> ed = m_editor;
            m_editor = nullptr;
            ed->setOleEmbeddedCallbacks({}, {});
            ed->setOleEmbeddedMode(true);
            ed->close();
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
            if (m_editor)
                m_editor->hide();
            m_visible = false;
            if (m_site)
                m_site->OnShowWindow(FALSE);
            return S_OK;
        }

        if (verb != OLEIVERB_PRIMARY && verb != OLEIVERB_OPEN && verb != OLEIVERB_SHOW
            && verb != OLEIVERB_INPLACEACTIVATE && verb != OLEIVERB_UIACTIVATE
            && verb != 0 && verb != 1) {
            return OLEOBJ_E_NOVERBS;
        }

        if (s)
            m_site = s;

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
            return S_OK;
        }

        m_stopped = false;
        const size_t generation = ++m_editorGeneration;
        AddRef();

        try {
            ApplicationWindow *mw = openAppWindowFromBytes(m_payload.projectBytes);
            if (!mw)
                mw = createSampleAppWindow();

            MultiLayer *ml = findTargetMultiLayer(mw, m_payload.targetWindow);
            m_initialRenderExtentHm = SIZEL{ 0, 0 };
            if (ml) {
                m_payload.targetWindow = ml->name();
                mw->activateSubWindow(ml);
                Graph *single = nullptr;
                if (m_payload.targetLayer >= 1 && m_payload.targetLayer <= ml->layers()) {
                    single = ml->layer(m_payload.targetLayer);
                    ml->setActiveGraph(single);
                }
                OleRenderedGraph initRg = SciDAVisEmf::renderMultiLayer(ml, single);
                if (initRg.extentHm.cx > 0 && initRg.extentHm.cy > 0)
                    m_initialRenderExtentHm = initRg.extentHm;
            }

            QString titleSuffix = m_payload.targetWindow.isEmpty() ? QStringLiteral("Embedded Graph") : m_payload.targetWindow;
            mw->setWindowTitle(QStringLiteral("ToriGraph - %1 [PowerPoint OLE]").arg(titleSuffix));
            mw->setOleEmbeddedMode(true);

            mw->setOleEmbeddedCallbacks(
                [this]() {
                    ComPtr<IOleObject> alive(this);
                    syncFromEditor();
                    markChanged();
                    auto site = m_site;
                    if (site) {
                        site->SaveObject();
                        if (!m_stopped)
                            site->ShowObject();
                    }
                },
                [this, generation]() {
                    ComPtr<IOleObject> alive(this);
                    if (generation == m_editorGeneration) {
                        syncFromEditor();
                        if (m_dirty) {
                            notifyViewAndData();
                            if (m_site)
                                m_site->SaveObject();
                        }
                        m_editor = nullptr;
                        finishClose();
                    }
                    Release();
                });

            m_editor = mw;
            mw->show();
            mw->raise();
            mw->activateWindow();
            SetForegroundWindow(reinterpret_cast<HWND>(mw->winId()));

            m_visible = true;
            if (m_site)
                m_site->OnShowWindow(TRUE);
            return S_OK;
        } catch (...) {
            Release();
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
            syncFromEditor();
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
        QFile f(QString::fromWCharArray(target.c_str()));
        if (!f.open(QIODevice::WriteOnly))
            return STG_E_WRITEFAULT;
        f.write(m_payload.projectBytes);
        f.close();
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
                if (m_payload.dibBytes.isEmpty())
                    return E_FAIL;
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(m_payload.dibBytes.size()));
                if (!hMem)
                    return E_OUTOFMEMORY;
                void *ptr = GlobalLock(hMem);
                std::memcpy(ptr, m_payload.dibBytes.constData(), static_cast<size_t>(m_payload.dibBytes.size()));
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
            { CF_DIB, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }
        };
        return SHCreateStdEnumFmtEtc(5, fmts, out);
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
        BOOL ok = PlayEnhMetaFile(dc, hemf, &rc);
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
    if (!payload.dibBytes.isEmpty()) {
        hr = writeStreamBytes(stg, L"SciDAVisDib", payload.dibBytes);
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

bool ensureRegistryRegistered(QString *errMsg)
{
    QString candidateExe = QCoreApplication::applicationDirPath() + QStringLiteral("/ToriGraph.exe");
    if (!QFileInfo::exists(candidateExe))
        candidateExe = QCoreApplication::applicationDirPath() + QStringLiteral("/scidavis.exe");
    if (!QFileInfo::exists(candidateExe))
        candidateExe = QCoreApplication::applicationFilePath();
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

bool initializeOleServer(bool embeddedMode)
{
    if (!g_oleInitialized) {
        HRESULT hr = OleInitialize(nullptr);
        if (FAILED(hr))
            return false;
        g_oleInitialized = true;
    }

    g_embeddedServerMode = embeddedMode;
    if (!embeddedMode && !g_normalSessionLockHeld) {
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

void shutdownOleServer()
{
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

bool copyGraphAsOle(ApplicationWindow *app, MultiLayer *ml, Graph *singleLayer)
{
    if (!ml)
        return false;
    if (!app) {
        QWidget *p = ml->parentWidget();
        while (p && !qobject_cast<ApplicationWindow *>(p))
            p = p->parentWidget();
        app = qobject_cast<ApplicationWindow *>(p);
    }
    if (!app) {
        for (QWidget *top : QApplication::topLevelWidgets()) {
            if (auto *cand = qobject_cast<ApplicationWindow *>(top)) {
                app = cand;
                break;
            }
        }
    }
    if (!app)
        return false;

    OleGraphPayload payload;
    payload.projectBytes = app->saveProjectToByteArray();
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

    OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml, singleLayer);
    payload.emfBytes = rg.emfBytes;
    payload.dibBytes = rg.dibBytes;
    payload.extentHm = rg.extentHm;
    payload.pixelSize = rg.pixelSize;

    ComPtr<IDataObject> dataObj;
    HRESULT hr = createGraphObject(payload, IID_PPV_ARGS(&dataObj));
    if (FAILED(hr) || !dataObj)
        return false;

    hr = setOleClipboardWithRetry(dataObj.Get());
    if (FAILED(hr))
        return false;

    g_clipboardDataObject = dataObj;
    return true;
}

bool copyImageAsOle(ApplicationWindow *app, const QImage &img, const QString &windowName)
{
    if (img.isNull())
        return false;
    OleGraphPayload payload;
    if (!app) {
        for (QWidget *top : QApplication::topLevelWidgets()) {
            if (auto *cand = qobject_cast<ApplicationWindow *>(top)) {
                app = cand;
                break;
            }
        }
    }
    if (app)
        payload.projectBytes = app->saveProjectToByteArray();
    payload.targetWindow = windowName;
    payload.targetLayer = 0;

    OleRenderedGraph rg = SciDAVisEmf::renderFromImage(img);
    payload.emfBytes = rg.emfBytes;
    payload.dibBytes = rg.dibBytes;
    payload.extentHm = rg.extentHm;
    payload.pixelSize = rg.pixelSize;

    ComPtr<IDataObject> dataObj;
    HRESULT hr = createGraphObject(payload, IID_PPV_ARGS(&dataObj));
    if (FAILED(hr) || !dataObj)
        return false;

    hr = setOleClipboardWithRetry(dataObj.Get());
    if (FAILED(hr))
        return false;

    g_clipboardDataObject = dataObj;
    return true;
}

} // namespace SciDAVisOle

#endif // Q_OS_WIN