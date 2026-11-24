#include "ole_test_common.h"
#include "Graph3D.h"
#include "GraphInspectorDock.h"
#include "ImportOPJU.h"
#include "Plot3DDialog.h"
#include "PythonAutomationServer.h"
#include "ScaleDraw.h"
#include "core/Project.h"
#include "future/table/TableModel.h"
#include "future/table/TableView.h"

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMdiArea>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUndoStack>
#include <QUndoView>

#include <cstring>
#include <functional>

namespace {

struct EnvVarGuard80
{
    QByteArray name;
    QByteArray prevVal;
    bool hadPrev = false;

    explicit EnvVarGuard80(const char *varName, const QByteArray &newVal)
        : name(varName), hadPrev(qEnvironmentVariableIsSet(varName))
    {
        if (hadPrev)
            prevVal = qgetenv(varName);
        qputenv(name.constData(), newVal);
    }
    ~EnvVarGuard80()
    {
        if (hadPrev)
            qputenv(name.constData(), prevVal);
        else
            qunsetenv(name.constData());
    }
    EnvVarGuard80(const EnvVarGuard80 &) = delete;
    EnvVarGuard80 &operator=(const EnvVarGuard80 &) = delete;
};

struct LocaleGuard80
{
    QLocale prevLocale;
    explicit LocaleGuard80(const QLocale &loc) : prevLocale(QLocale())
    {
        QLocale::setDefault(loc);
    }
    ~LocaleGuard80() { QLocale::setDefault(prevLocale); }
    LocaleGuard80(const LocaleGuard80 &) = delete;
    LocaleGuard80 &operator=(const LocaleGuard80 &) = delete;
};

static void pumpEventsMs80(int ms)
{
    QElapsedTimer t;
    t.start();
    const int minPasses = qMax(2, ms / 20);
    for (int pass = 0; pass < minPasses || t.elapsed() < ms; ++pass) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (t.elapsed() < ms)
            QThread::msleep(10);
    }
}

template <typename Pred>
static void waitUntilMs80(int timeoutMs, Pred &&pred)
{
    QElapsedTimer t;
    t.start();
    for (int pass = 0;; ++pass) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (pred() || (pass >= 2 && t.elapsed() >= timeoutMs))
            break;
        QThread::msleep(15);
    }
}

#ifdef Q_OS_WIN
struct MockDeadClientSite80 final : public IOleClientSite, public IAdviseSink
{
    ULONG refs = 1;
    bool containerDead = false;
    int saveObjectCount = 0;
    HRESULT saveObjectHr = S_OK;
    std::function<void()> onSaveObject;
    std::function<void(BOOL)> onShowWindow;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (id == IID_IUnknown || id == IID_IOleClientSite)
            *out = static_cast<IOleClientSite *>(this);
        else if (id == IID_IAdviseSink)
            *out = static_cast<IAdviseSink *>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; }

    HRESULT STDMETHODCALLTYPE SaveObject() override
    {
        ++saveObjectCount;
        if (auto cb = onSaveObject)
            cb();
        return saveObjectHr;
    }
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker **m) override
    {
        if (m)
            *m = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer **c) override
    {
        if (c)
            *c = nullptr;
        return containerDead ? RPC_E_SERVER_DIED : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE ShowObject() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL fShow) override
    {
        if (auto cb = onShowWindow)
            cb(fShow);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() override { return S_OK; }

    void STDMETHODCALLTYPE OnDataChange(FORMATETC *, STGMEDIUM *) override {}
    void STDMETHODCALLTYPE OnViewChange(DWORD, LONG) override {}
    void STDMETHODCALLTYPE OnRename(IMoniker *) override {}
    void STDMETHODCALLTYPE OnSave() override {}
    void STDMETHODCALLTYPE OnClose() override {}
};

static SciDAVisOle::OleGraphPayload makeOlePayload80(ApplicationWindow *srcApp, const char *tblName)
{
    Table *t = srcApp->newTable(4, 2, tblName, "Ole80");
    for (int r = 0; r < 4; ++r) {
        t->setCell(r, 0, double(r));
        t->setCell(r, 1, double(r * 10 + 5));
    }
    MultiLayer *ml = srcApp->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
    const OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml, nullptr, false);
    SciDAVisOle::OleGraphPayload pay;
    pay.projectBytes =
            SciDAVisOle::buildEmbeddedProjectBytes(srcApp, ml, SciDAVisOle::EmbedScope::GraphOnly);
    pay.targetWindow = ml->name();
    pay.targetLayer = 0;
    pay.emfBytes = rg.emfBytes;
    pay.extentHm = rg.extentHm;
    pay.pixelSize = rg.pixelSize;
    return pay;
}

static QPointer<ApplicationWindow> findNewOleEditor80(const QSet<ApplicationWindow *> &existing)
{
    for (QWidget *tlw : QApplication::topLevelWidgets()) {
        if (auto *cand = qobject_cast<ApplicationWindow *>(tlw)) {
            if (!existing.contains(cand) && cand->isVisible() && cand->isOleEmbeddedMode())
                return QPointer<ApplicationWindow>(cand);
        }
    }
    return QPointer<ApplicationWindow>();
}

struct EmbeddedModeGuard80
{
    bool prev = SciDAVisOle::isEmbeddedServerModeForTesting();
    explicit EmbeddedModeGuard80(bool on) { SciDAVisOle::setEmbeddedServerModeForTesting(on); }
    ~EmbeddedModeGuard80() { SciDAVisOle::setEmbeddedServerModeForTesting(prev); }
    EmbeddedModeGuard80(const EmbeddedModeGuard80 &) = delete;
    EmbeddedModeGuard80 &operator=(const EmbeddedModeGuard80 &) = delete;
};
#endif

struct AxesDialogProbe80 : public AxesDialog
{
    void setScaleBoundsAndUpdate(const QString &startTxt, const QString &endTxt)
    {
        generalDialog.setCurrentWidget(scalesPage);
        axesList->setCurrentRow(0);
        boxStart->setText(startTxt);
        boxEnd->setText(endTxt);
        accept();
    }

    void setTickFormulaAndUpdate(const QString &formulaTxt)
    {
        generalDialog.setCurrentWidget(axesPage);
        axesList->setCurrentRow(0);
        axesTitlesList->setCurrentRow(0);
        if (!boxShowFormula->isChecked())
            boxShowFormula->click();
        boxFormula->setPlainText(formulaTxt);
        accept();
    }
};

static QByteArray encodeCpyuaZeroRle80(const QByteArray &raw)
{
    QByteArray out;
    int i = 0;
    while (i < raw.size()) {
        if (raw[i] == '\0') {
            int run = 0;
            while (i + run < raw.size() && raw[i + run] == '\0' && run < 127)
                ++run;
            out.append(static_cast<char>(0x80U | static_cast<unsigned char>(run)));
            i += run;
        } else {
            int run = 0;
            while (i + run < raw.size() && raw[i + run] != '\0' && run < 127)
                ++run;
            out.append(static_cast<char>(static_cast<unsigned char>(run)));
            out.append(raw.constData() + i, run);
            i += run;
        }
    }
    return out;
}

static void appendU16LE80(QByteArray &buf, quint16 v)
{
    buf.append(static_cast<char>(v & 0xFFU));
    buf.append(static_cast<char>((v >> 8) & 0xFFU));
}

static void appendU32LE80(QByteArray &buf, quint32 v)
{
    for (int b = 0; b < 4; ++b)
        buf.append(static_cast<char>((v >> (8 * b)) & 0xFFU));
}

static void appendU64LE80(QByteArray &buf, quint64 v)
{
    for (int b = 0; b < 8; ++b)
        buf.append(static_cast<char>((v >> (8 * b)) & 0xFFU));
}

static QByteArray buildSyntheticCpyuaSkippedBuffer80()
{
    // Minimal CPYUA container:
    //   Dataset 1: Book80_A ([1,2,3], oneBasedId=1)
    //   Dataset 2: Book80_Skip (non-0x05 skipped payload, oneBasedId=2)
    //   Dataset 3: Book80_B ([10,20,30], oneBasedId=3)
    //   Workbook:  Book80 (rleLen >= 64, winDec == 540B)
    //   Column 1:  dsId=2, name="A", desig=0x00(Y) -> hits skipped ds 2 -> resyncs to ds 1 (seqShift=1)
    //   Column 2:  dsId=3, name="B", desig=0x03(X) -> 3-seqShift=2 hits skipped ds 2 -> resyncs to ds 3
    QByteArray buf("CPYUA  2024\0", 12);
    buf.append(QByteArray(52, '\0'));
    buf.append("\x7c\x00\x00\x00\x0a", 5);
    buf.append(QByteArray(0x7C + 1 + 5, '\0'));

    auto makeNumPay = [](std::initializer_list<double> vals) {
        QByteArray numPay;
        numPay.append('\x05');
        numPay.append(static_cast<char>(vals.size())); // LEB128 totalRows
        for (double val : vals) {
            numPay.append('\x00');
            numPay.append('\x00');
            numPay.append('\x50'); // full 8-byte IEEE754 literal
            quint64 bits = 0;
            std::memcpy(&bits, &val, sizeof(double));
            appendU64LE80(numPay, bits);
        }
        numPay.append('\xCE');
        numPay.append('\xFA');
        return numPay;
    };

    auto appendDatasetBlock = [&](const QByteArray &fullName, const QByteArray &payload) {
        QByteArray hdr(114, '\0');
        hdr[2] = '\x01';
        hdr[3] = '\xFA';
        const int nameOff = hdr.size() - 53;
        const int copyLen = std::min(fullName.size(), 21);
        for (int i = 0; i < copyLen; ++i)
            hdr[nameOff + i] = fullName[i];
        const QByteArray rleHdr = encodeCpyuaZeroRle80(hdr);
        buf.append('\x80');
        buf.append(static_cast<char>(rleHdr.size()));
        buf.append(rleHdr);
        buf.append('\x18');
        appendU64LE80(buf, static_cast<quint64>(payload.size()));
        appendU64LE80(buf, static_cast<quint64>(payload.size()));
        buf.append(QByteArray(8, '\0'));
        buf.append('\x0A');
        buf.append(payload);
        appendU32LE80(buf, 0U);
    };

    appendDatasetBlock(QByteArrayLiteral("Book80_A"), makeNumPay({ 1.0, 2.0, 3.0 }));
    appendDatasetBlock(QByteArrayLiteral("Book80_Skip"),
                       QByteArray("\x09\x01\x02\x03\x04\x05\x06\x07", 8));
    appendDatasetBlock(QByteArrayLiteral("Book80_B"), makeNumPay({ 10.0, 20.0, 30.0 }));

    buf.append("\x00\x00\x00\x00\x00\x0A", 6);

    // Workbook header (winDec=540B, rleLen >= 64)
    QByteArray winRaw(540, '\0');
    winRaw[2] = 'B';
    winRaw[3] = 'o';
    winRaw[4] = 'o';
    winRaw[5] = 'k';
    winRaw[6] = '8';
    winRaw[7] = '0';
    winRaw[57] = 'O';
    winRaw[58] = 'r';
    winRaw[59] = 'i';
    winRaw[60] = 'g';
    winRaw[61] = 'i';
    winRaw[62] = 'n';
    for (int k = 120; k < 210; ++k)
        winRaw[k] = static_cast<char>('A' + (k % 26));
    const QByteArray rleWin = encodeCpyuaZeroRle80(winRaw);
    buf.append('\x80');
    buf.append(static_cast<char>(rleWin.size()));
    buf.append(rleWin);

    auto appendColMeta = [&](quint16 dsId, unsigned char desigByte, char colCh) {
        QByteArray colRaw(468, '\0');
        colRaw[0] = '\x10';
        colRaw[1] = static_cast<char>(dsId & 0xFFU);
        colRaw[2] = static_cast<char>((dsId >> 8) & 0xFFU);
        colRaw[8] = static_cast<char>(desigByte);
        colRaw[9] = colCh;
        for (int k = 300; k < 325; ++k)
            colRaw[k] = 'P'; // ensure 16 <= rleCol.size() <= 200 (ImportOPJU.cpp:3548, 3611)
        QByteArray rleCol;
        rleCol.append('\x01'); // 1-byte literal header required by ImportOPJU.cpp:3553-3554
        rleCol.append('\x10');
        rleCol.append(encodeCpyuaZeroRle80(colRaw.mid(1)));
        buf.append('\x80');
        buf.append(static_cast<char>(rleCol.size()));
        buf.append(rleCol);
    };
    appendColMeta(2U, 0x00U, 'A'); // SciDAVis::Y, conflicts with skipped Dataset 2 -> resyncs to A
    appendColMeta(3U, 0x03U, 'B'); // SciDAVis::X, 3-seqShift=2 conflicts with skipped -> resyncs to B
    return buf;
}

} // namespace

// =========================================================================================
// Suite 80: Round 4 P0/P1 Regression Gate
// =========================================================================================
int runSuite80Round4P0P1RegressionGate(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 80: Round 4 P0/P1 Regression Gate...\n";

    struct NonInteractiveQuitGuard80
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard80()
            : prevNonInteractive(qApp->property("torigraph_noninteractive"))
            , prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard80()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard80;

    struct AppWinGuard80
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard80(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard80() { reset(); }
        AppWinGuard80(const AppWinGuard80 &) = delete;
        AppWinGuard80 &operator=(const AppWinGuard80 &) = delete;
        void reset(ApplicationWindow *next = nullptr)
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            win = next;
            QApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        ApplicationWindow *get() const { return win; }
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    QTemporaryDir tmp80;
    CHECK_COND(tmp80.isValid(), "80: temp dir creation failed");

    AppWinGuard80 w80(new ApplicationWindow());
    w80->applyUserSettings();
    w80->confirmCloseTable = false;
    w80->confirmClosePlot2D = false;
    w80->confirmClosePlot3D = false;
    w80->confirmCloseFolder = false;
    w80->setHeadlessMode(true);

    // -------------------------------------------------------------------------
    // 80a: OLE dead-container watchdog UAF & saved-standalone survival
    // -------------------------------------------------------------------------
#ifdef Q_OS_WIN
    {
        AppWinGuard80 oleSrc(new ApplicationWindow());
        oleSrc->setHeadlessMode(true);
        oleSrc->setOleEmbeddedMode(true);
        oleSrc->applyUserSettings();
        const SciDAVisOle::OleGraphPayload pay = makeOlePayload80(oleSrc.get(), "Ole80SrcTbl");
        CHECK_COND(!pay.projectBytes.isEmpty() && !pay.emfBytes.isEmpty(),
                   "80a: failed to build OLE test payload");

        auto snapshotTopApps = []() {
            QSet<ApplicationWindow *> s;
            for (QWidget *tlw : QApplication::topLevelWidgets()) {
                if (auto *aw = qobject_cast<ApplicationWindow *>(tlw))
                    s.insert(aw);
            }
            return s;
        };

        // Subtest 1: Dirty embedded window converts to standalone once and survives subsequent
        // watchdog ticks even after the user saves it (saved == true).
        {
            MockDeadClientSite80 site1;
            ComPtr<IOleObject> oleObj1;
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&oleObj1)))
                               && oleObj1,
                       "80a: createGraphObject(oleObj1) failed");
            CHECK_COND(SUCCEEDED(oleObj1->SetClientSite(&site1)),
                       "80a: SetClientSite(site1) failed");

            const QSet<ApplicationWindow *> beforeOpen = snapshotTopApps();
            CHECK_COND(SUCCEEDED(oleObj1->DoVerb(OLEIVERB_OPEN, nullptr, &site1, 0, nullptr,
                                                 nullptr)),
                       "80a: DoVerb(OLEIVERB_OPEN, oleObj1) failed");
            QApplication::processEvents();
            QPointer<ApplicationWindow> ed1 = findNewOleEditor80(beforeOpen);
            CHECK_COND(!ed1.isNull() && ed1->isVisible() && ed1->isOleEmbeddedMode(),
                       "80a: OLE editor 1 did not open cleanly");

            ed1->saved = false;
            site1.containerDead = true;
            SciDAVisOle::runContainerWatchdogTickForTesting();
            QApplication::setQuitOnLastWindowClosed(false);
            pumpEventsMs80(20);

            CHECK_COND(!ed1.isNull() && ed1->isVisible(),
                       "80a: dirty standalone editor was closed on container exit");
            CHECK_COND(!ed1->isOleEmbeddedMode(),
                       "80a: converted standalone editor still in OLE embedded mode");
            CHECK_COND(ed1->property("_tg_ole_container_died_handled").toBool(),
                       "80a: missing _tg_ole_container_died_handled marker");

            // Simulate user saving standalone window, then fire another watchdog tick
            ed1->savedProject();
            CHECK_COND(ed1->saved, "80a: savedProject did not set saved==true");
            SciDAVisOle::runContainerWatchdogTickForTesting();
            pumpEventsMs80(20);
            CHECK_COND(!ed1.isNull() && ed1->isVisible(),
                       "80a: second watchdog tick closed saved standalone window!");

            ed1->savedProject();
            ed1->setAttribute(Qt::WA_DeleteOnClose, true);
            ed1->close();
            pumpEventsMs80(20);
            CHECK_COND(ed1.isNull(), "80a: manual close of converted standalone ed1 did not destroy");
            oleObj1.Reset();
            pumpEventsMs80(10);
        }

        // Subtest 2: Clean embedded window closed on watchdog tick while objA is held ONLY by
        // its editor ref (no local ComPtr ref) alongside another live object objB (ASan UAF check).
        {
            const int baseLive = SciDAVisOle::liveGraphObjectCountForTesting();
            MockDeadClientSite80 siteA;
            MockDeadClientSite80 siteB;
            ComPtr<IOleObject> objA;
            ComPtr<IOleObject> objB;
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&objA))) && objA,
                       "80a: createGraphObject(objA) failed");
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&objB))) && objB,
                       "80a: createGraphObject(objB) failed");
            CHECK_COND(SUCCEEDED(objA->SetClientSite(&siteA))
                               && SUCCEEDED(objB->SetClientSite(&siteB)),
                       "80a: SetClientSite A/B failed");
            CHECK_COND(SciDAVisOle::liveGraphObjectCountForTesting() == baseLive + 2,
                       "80a: unexpected live graph object count before watchdog");

            const QSet<ApplicationWindow *> beforeOpen = snapshotTopApps();
            CHECK_COND(SUCCEEDED(objA->DoVerb(OLEIVERB_OPEN, nullptr, &siteA, 0, nullptr, nullptr)),
                       "80a: DoVerb(objA) failed");
            QApplication::processEvents();
            QPointer<ApplicationWindow> edA = findNewOleEditor80(beforeOpen);
            CHECK_COND(!edA.isNull() && edA->isVisible() && edA->saved,
                       "80a: clean OLE editor edA did not open in clean saved state");

            // Drop caller's reference so only the active editor ref keeps objA alive
            objA.Reset();
            siteA.containerDead = true;
            siteB.containerDead = true;
            SciDAVisOle::runContainerWatchdogTickForTesting();
            QApplication::setQuitOnLastWindowClosed(false);
            pumpEventsMs80(25);
            objB.Reset();
            pumpEventsMs80(25);

            CHECK_COND(edA.isNull(), "80a: clean edA was not closed/destroyed after container exit");
            CHECK_COND(SciDAVisOle::liveGraphObjectCountForTesting() == baseLive,
                       "80a: live graph object leaked after watchdog teardown (got "
                               << SciDAVisOle::liveGraphObjectCountForTesting() << ", expected "
                               << baseLive << ")");
        }

        // Subtest 3 (P1-3 + rejectEditorModalDialogs): Detaching one dirty OLE object to
        // standalone must NOT disable g_embeddedServerMode while another live embedded object
        // exists, and closing an embedded object with an active child modal QDialog must reject
        // the dialog and tear down cleanly under ASan.
        {
            EmbeddedModeGuard80 embGuard(true);
            MockDeadClientSite80 site1;
            MockDeadClientSite80 site2;
            ComPtr<IOleObject> obj1;
            ComPtr<IOleObject> obj2;
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&obj1))) && obj1,
                       "80a-3: createGraphObject(obj1) failed");
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&obj2))) && obj2,
                       "80a-3: createGraphObject(obj2) failed");
            CHECK_COND(SUCCEEDED(obj1->SetClientSite(&site1))
                               && SUCCEEDED(obj2->SetClientSite(&site2)),
                       "80a-3: SetClientSite failed");

            const QSet<ApplicationWindow *> before1 = snapshotTopApps();
            CHECK_COND(SUCCEEDED(obj1->DoVerb(OLEIVERB_OPEN, nullptr, &site1, 0, nullptr, nullptr)),
                       "80a-3: DoVerb(obj1) failed");
            QApplication::processEvents();
            QPointer<ApplicationWindow> ed1 = findNewOleEditor80(before1);
            CHECK_COND(!ed1.isNull() && ed1->isOleEmbeddedMode(), "80a-3: ed1 open failed");

            const QSet<ApplicationWindow *> before2 = snapshotTopApps();
            CHECK_COND(SUCCEEDED(obj2->DoVerb(OLEIVERB_OPEN, nullptr, &site2, 0, nullptr, nullptr)),
                       "80a-3: DoVerb(obj2) failed");
            QApplication::processEvents();
            QPointer<ApplicationWindow> ed2 = findNewOleEditor80(before2);
            CHECK_COND(!ed2.isNull() && ed2->isOleEmbeddedMode(), "80a-3: ed2 open failed");

            QPointer<QDialog> childDlg = new QDialog(ed2.data());
            childDlg->setModal(true);
            childDlg->open();
            QApplication::processEvents();
            CHECK_COND(!childDlg.isNull() && childDlg->isVisible(),
                       "80a-3: child modal QDialog failed to open");

            // Container 1 dies while ed1 is dirty -> ed1 converts to standalone, but obj2 keeps
            // embedded server mode alive!
            ed1->saved = false;
            site1.containerDead = true;
            SciDAVisOle::runContainerWatchdogTickForTesting();
            QApplication::setQuitOnLastWindowClosed(false);
            pumpEventsMs80(25);
            CHECK_COND(!ed1.isNull() && !ed1->isOleEmbeddedMode(),
                       "80a-3: ed1 did not convert to standalone");
            CHECK_COND(SciDAVisOle::isEmbeddedServerModeForTesting(),
                       "80a-3: P1-3 regression! g_embeddedServerMode disabled globally while obj2 active");

            // Container 2 dies -> rejects child modal dialog and destroys clean ed2 safely
            site2.containerDead = true;
            SciDAVisOle::runContainerWatchdogTickForTesting();
            QApplication::setQuitOnLastWindowClosed(false);
            pumpEventsMs80(35);
            CHECK_COND(childDlg.isNull() || !childDlg->isVisible(),
                       "80a-3: rejectEditorModalDialogs did not reject/close child dialog");
            CHECK_COND(ed2.isNull(), "80a-3: clean ed2 not destroyed after container exit");

            ed1->savedProject();
            ed1->setAttribute(Qt::WA_DeleteOnClose, true);
            ed1->close();
            obj1.Reset();
            obj2.Reset();
            pumpEventsMs80(25);
        }

        // Subtest 4 (P1 OleServer::onClose rearmHolder closure UAF): closing a dirty embedded
        // editor when SaveObject() returns E_FAIL must keep the editor open, set
        // _toriLastOleSaveOk=false, and re-arm d_ole_on_close cleanly so a retry with S_OK closes
        // with zero ASan UAF.
        {
            MockDeadClientSite80 site4;
            ComPtr<IOleObject> obj4;
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&obj4))) && obj4,
                       "80a-4: createGraphObject(obj4) failed");
            CHECK_COND(SUCCEEDED(obj4->SetClientSite(&site4)), "80a-4: SetClientSite failed");

            const QSet<ApplicationWindow *> before4 = snapshotTopApps();
            CHECK_COND(SUCCEEDED(obj4->DoVerb(OLEIVERB_OPEN, nullptr, &site4, 0, nullptr, nullptr)),
                       "80a-4: DoVerb(obj4) failed");
            QApplication::processEvents();
            QPointer<ApplicationWindow> ed4 = findNewOleEditor80(before4);
            CHECK_COND(!ed4.isNull() && ed4->isOleEmbeddedMode(), "80a-4: ed4 open failed");

            ed4->modifiedProject();
            const int savesBeforeClose = site4.saveObjectCount;
            site4.saveObjectHr = E_FAIL;
            ed4->close();
            pumpEventsMs80(25);
            CHECK_COND(!ed4.isNull() && !ed4->property("_toriLastOleSaveOk").toBool(),
                       "80a-4: editor closed or failed to record _toriLastOleSaveOk=false on E_FAIL");
            CHECK_COND(site4.saveObjectCount == savesBeforeClose + 1,
                       "80a-4: expected SaveObject count==" << (savesBeforeClose + 1)
                                                            << " on first failed close (got "
                                                            << site4.saveObjectCount << ")");

            site4.saveObjectHr = S_OK;
            ed4->close();
            pumpEventsMs80(25);
            CHECK_COND(site4.saveObjectCount >= savesBeforeClose + 2,
                       "80a-4: re-armed d_ole_on_close did not fire SaveObject on retry (count="
                               << site4.saveObjectCount << ", expected >= "
                               << (savesBeforeClose + 2) << ")");
            CHECK_COND(ed4.isNull(),
                       "80a-4: embedded editor failed to close after re-armed S_OK retry");
            obj4.Reset();
            pumpEventsMs80(25);
        }

        // Subtest 5 (Round 8 + Round 9 OLE teardown & re-entrant SaveObject -> Close(OLECLOSE_NOSAVE)):
        // Re-entrant COM IOleObject::Close() inside SaveObject() during ed5->saveProject() (even
        // with SaveObject() returning E_FAIL afterward) must NOT UAF `ed5`, must NOT show an
        // interactive save-failure modal warning, and must return `true` before deferred teardown.
        {
            const int baseLive5 = SciDAVisOle::liveGraphObjectCountForTesting();
            MockDeadClientSite80 site5;
            site5.saveObjectHr = E_FAIL;
            ComPtr<IOleObject> obj5;
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&obj5))) && obj5,
                       "80a-5: createGraphObject(obj5) failed");
            CHECK_COND(SUCCEEDED(obj5->SetClientSite(&site5)), "80a-5: SetClientSite failed");

            const QSet<ApplicationWindow *> before5 = snapshotTopApps();
            CHECK_COND(SUCCEEDED(obj5->DoVerb(OLEIVERB_OPEN, nullptr, &site5, 0, nullptr, nullptr)),
                       "80a-5: DoVerb(obj5) failed");
            QApplication::processEvents();
            QPointer<ApplicationWindow> ed5 = findNewOleEditor80(before5);
            CHECK_COND(!ed5.isNull() && ed5->isOleEmbeddedMode(), "80a-5: ed5 open failed");

            ed5->modifiedProject();
            bool closedOnce5 = false;
            site5.onSaveObject = [&]() {
                if (closedOnce5)
                    return;
                closedOnce5 = true;
                obj5->Close(OLECLOSE_NOSAVE);
            };
            bool sawWarning5 = false;
            bool saved5 = false;
            bool propOkBeforePump5 = false;
            {
                const QVariant prevProp5 = qApp->property("torigraph_noninteractive");
                const bool prevHeadless5 = ed5->isHeadlessMode();
                qApp->setProperty("torigraph_noninteractive", false);
                ed5->setHeadlessMode(false);
                QTimer warnTimer5;
                warnTimer5.setInterval(10);
                QObject::connect(&warnTimer5, &QTimer::timeout, [&]() {
                    if (auto *mb = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                        sawWarning5 = true;
                        mb->accept();
                    }
                });
                warnTimer5.start();
                saved5 = ed5->saveProject();
                warnTimer5.stop();
                propOkBeforePump5 =
                        !ed5.isNull() && ed5->property("_toriLastOleSaveOk").toBool();
                if (ed5)
                    ed5->setHeadlessMode(prevHeadless5);
                qApp->setProperty("torigraph_noninteractive", prevProp5);
            }
            CHECK_COND(closedOnce5 && saved5 && !sawWarning5 && propOkBeforePump5,
                       "80a-5: saveProject flagged failure or skipped re-entrant Close (closed="
                               << closedOnce5 << ", saved5=" << saved5 << ", sawWarn="
                               << sawWarning5 << ", propOk=" << propOkBeforePump5 << ")");

            QApplication::setQuitOnLastWindowClosed(false);
            pumpEventsMs80(35);
            obj5.Reset();
            pumpEventsMs80(25);

            CHECK_COND(ed5.isNull(), "80a-5: ed5 was not deferred-destroyed after Close()");
            CHECK_COND(SciDAVisOle::liveGraphObjectCountForTesting() == baseLive5,
                       "80a-5: OLE object leaked after re-entrant Close (got "
                               << SciDAVisOle::liveGraphObjectCountForTesting() << ", expected "
                               << baseLive5 << ")");
        }

        // Subtest 6 (Round 9): (6a) re-entrant Close during closeEvent()/d_ole_on_close with E_FAIL
        // must NOT show save-failure warning; (6b/6c) crash-recovery detachment during saveProject/
        // closeEvent must preserve the detached unsaved standalone window instead of marking saved
        // or closing it with unsaved edits.
        {
            const int baseLive6 = SciDAVisOle::liveGraphObjectCountForTesting();
            MockDeadClientSite80 site6;
            site6.saveObjectHr = E_FAIL;
            ComPtr<IOleObject> obj6;
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&obj6))) && obj6,
                       "80a-6a: createGraphObject(obj6) failed");
            CHECK_COND(SUCCEEDED(obj6->SetClientSite(&site6)), "80a-6a: SetClientSite failed");
            const QSet<ApplicationWindow *> before6 = snapshotTopApps();
            CHECK_COND(SUCCEEDED(obj6->DoVerb(OLEIVERB_OPEN, nullptr, &site6, 0, nullptr, nullptr)),
                       "80a-6a: DoVerb(obj6) failed");
            QApplication::processEvents();
            QPointer<ApplicationWindow> ed6 = findNewOleEditor80(before6);
            CHECK_COND(!ed6.isNull() && ed6->isOleEmbeddedMode(), "80a-6a: ed6 open failed");
            ed6->modifiedProject();
            bool closedOnce6 = false;
            site6.onSaveObject = [&]() {
                if (closedOnce6)
                    return;
                closedOnce6 = true;
                obj6->Close(OLECLOSE_NOSAVE);
            };
            bool sawWarning6 = false;
            {
                const QVariant prevProp6 = qApp->property("torigraph_noninteractive");
                const bool prevHeadless6 = ed6->isHeadlessMode();
                qApp->setProperty("torigraph_noninteractive", false);
                ed6->setHeadlessMode(false);
                QTimer warnTimer6;
                warnTimer6.setInterval(10);
                QObject::connect(&warnTimer6, &QTimer::timeout, [&]() {
                    if (auto *mb = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                        sawWarning6 = true;
                        mb->accept();
                    }
                });
                warnTimer6.start();
                ed6->close();
                warnTimer6.stop();
                const bool okProp6a =
                        !ed6.isNull() && ed6->property("_toriLastOleSaveOk").toBool();
                if (ed6)
                    ed6->setHeadlessMode(prevHeadless6);
                qApp->setProperty("torigraph_noninteractive", prevProp6);
                CHECK_COND(okProp6a,
                           "80a-6a: _toriLastOleSaveOk was false after re-entrant Close during closeEvent");
            }
            QApplication::setQuitOnLastWindowClosed(false);
            pumpEventsMs80(35);
            obj6.Reset();
            pumpEventsMs80(25);
            CHECK_COND(closedOnce6 && !sawWarning6 && ed6.isNull(),
                       "80a-6a: re-entrant Close in closeEvent failed (closed="
                               << closedOnce6 << ", sawWarn=" << sawWarning6
                               << ", null=" << ed6.isNull() << ")");

            // 6b: Watchdog crash-recovery detachment inside saveProject() must NOT mark saved
            MockDeadClientSite80 site6b;
            ComPtr<IOleObject> obj6b;
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&obj6b))) && obj6b,
                       "80a-6b: createGraphObject(obj6b) failed");
            CHECK_COND(SUCCEEDED(obj6b->SetClientSite(&site6b)), "80a-6b: SetClientSite failed");
            const QSet<ApplicationWindow *> before6b = snapshotTopApps();
            CHECK_COND(SUCCEEDED(obj6b->DoVerb(OLEIVERB_OPEN, nullptr, &site6b, 0, nullptr, nullptr)),
                       "80a-6b: DoVerb(obj6b) failed");
            QApplication::processEvents();
            QPointer<ApplicationWindow> ed6b = findNewOleEditor80(before6b);
            CHECK_COND(!ed6b.isNull() && ed6b->isOleEmbeddedMode(), "80a-6b: ed6b open failed");
            ed6b->modifiedProject();
            site6b.containerDead = true;
            bool ticked6b = false;
            site6b.onSaveObject = [&]() {
                if (ticked6b)
                    return;
                ticked6b = true;
                SciDAVisOle::runContainerWatchdogTickForTesting();
            };
            const bool saved6b = ed6b->saveProject();
            QApplication::setQuitOnLastWindowClosed(false);
            pumpEventsMs80(25);
            CHECK_COND(ticked6b && !saved6b && !ed6b.isNull() && !ed6b->isOleEmbeddedMode()
                               && !ed6b->saved,
                       "80a-6b: detached dirty standalone marked saved or closed (ticked="
                               << ticked6b << ", saved6b=" << saved6b << ", null=" << ed6b.isNull()
                               << ", emb=" << (ed6b ? ed6b->isOleEmbeddedMode() : true)
                               << ", saved=" << (ed6b ? ed6b->saved : true) << ")");
            ed6b->savedProject();
            ed6b->setAttribute(Qt::WA_DeleteOnClose, true);
            ed6b->close();
            obj6b.Reset();
            pumpEventsMs80(25);

            // 6c: Watchdog crash-recovery detachment inside closeEvent() must ignore close & keep window
            MockDeadClientSite80 site6c;
            ComPtr<IOleObject> obj6c;
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&obj6c))) && obj6c,
                       "80a-6c: createGraphObject(obj6c) failed");
            CHECK_COND(SUCCEEDED(obj6c->SetClientSite(&site6c)), "80a-6c: SetClientSite failed");
            const QSet<ApplicationWindow *> before6c = snapshotTopApps();
            CHECK_COND(SUCCEEDED(obj6c->DoVerb(OLEIVERB_OPEN, nullptr, &site6c, 0, nullptr, nullptr)),
                       "80a-6c: DoVerb(obj6c) failed");
            QApplication::processEvents();
            QPointer<ApplicationWindow> ed6c = findNewOleEditor80(before6c);
            CHECK_COND(!ed6c.isNull() && ed6c->isOleEmbeddedMode(), "80a-6c: ed6c open failed");
            ed6c->modifiedProject();
            site6c.containerDead = true;
            bool ticked6c = false;
            site6c.onSaveObject = [&]() {
                if (ticked6c)
                    return;
                ticked6c = true;
                SciDAVisOle::runContainerWatchdogTickForTesting();
            };
            ed6c->close();
            QApplication::setQuitOnLastWindowClosed(false);
            pumpEventsMs80(35);
            CHECK_COND(ticked6c && !ed6c.isNull() && ed6c->isVisible() && !ed6c->isOleEmbeddedMode()
                               && !ed6c->saved,
                       "80a-6c: detached dirty standalone closed during closeEvent (ticked="
                               << ticked6c << ", null=" << ed6c.isNull()
                               << ", vis=" << (ed6c ? ed6c->isVisible() : false)
                               << ", emb=" << (ed6c ? ed6c->isOleEmbeddedMode() : true)
                               << ", saved=" << (ed6c ? ed6c->saved : true) << ")");
            ed6c->savedProject();
            ed6c->setAttribute(Qt::WA_DeleteOnClose, true);
            ed6c->close();
            obj6c.Reset();
            pumpEventsMs80(25);
            CHECK_COND(SciDAVisOle::liveGraphObjectCountForTesting() == baseLive6,
                       "80a-6: OLE object leaked (got "
                               << SciDAVisOle::liveGraphObjectCountForTesting() << ", expected "
                               << baseLive6 << ")");
        }

        // Subtest 7 (Round 9 + 10 ASan): COM Update(), IsDirty(), IPersistFile::Save(), and
        // IDataObject::GetData() must hold `ComPtr<IOleObject> alive(this)` across Graph3D's
        // captureGraph3DOleImage processEvents() pump even when all external COM refs & editor drop.
        {
            AppWinGuard80 oleSrc3d(new ApplicationWindow());
            oleSrc3d->setHeadlessMode(true);
            oleSrc3d->setOleEmbeddedMode(true);
            oleSrc3d->applyUserSettings();
            oleSrc3d->confirmClosePlot3D = false;
            Graph3D *srcG3d = oleSrc3d->newPlot3D();
            CHECK_COND(srcG3d != nullptr, "80a-7: failed to create Graph3D source");
            srcG3d->addFunction("cos(x)*cos(y)", -3.0, 3.0, -3.0, 3.0, -1.2, 1.2);
            QCoreApplication::processEvents();
            SciDAVisOle::OleGraphPayload pay3d;
            pay3d.projectBytes = SciDAVisOle::buildEmbeddedProjectBytes(
                    oleSrc3d.get(), srcG3d, SciDAVisOle::EmbedScope::GraphOnly);
            pay3d.targetWindow = srcG3d->name();
            pay3d.emfBytes = pay.emfBytes;
            pay3d.extentHm = pay.extentHm;
            pay3d.pixelSize = pay.pixelSize;
            CHECK_COND(!pay3d.projectBytes.isEmpty(), "80a-7: failed to build 3D OLE payload");

            const int baseLive7 = SciDAVisOle::liveGraphObjectCountForTesting();
            for (int op = 0; op < 4; ++op) {
                MockDeadClientSite80 site7;
                ComPtr<IOleObject> obj7;
                CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay3d, IID_PPV_ARGS(&obj7)))
                                   && obj7,
                           "80a-7: createGraphObject failed on op " << op);
                CHECK_COND(SUCCEEDED(obj7->SetClientSite(&site7)),
                           "80a-7: SetClientSite failed on op " << op);

                const QSet<ApplicationWindow *> before7 = snapshotTopApps();
                CHECK_COND(SUCCEEDED(obj7->DoVerb(OLEIVERB_OPEN, nullptr, &site7, 0, nullptr,
                                                  nullptr)),
                           "80a-7: DoVerb failed on op " << op);
                QApplication::processEvents();
                QPointer<ApplicationWindow> ed7 = findNewOleEditor80(before7);
                CHECK_COND(!ed7.isNull() && ed7->isOleEmbeddedMode(),
                           "80a-7: embedded 3D editor failed to open on op " << op);

                ComPtr<IPersistStorage> ipStg7;
                ComPtr<IPersistFile> ipFile7;
                ComPtr<IDataObject> iData7;
                CHECK_COND(SUCCEEDED(obj7.As(&ipStg7)) && SUCCEEDED(obj7.As(&ipFile7))
                                   && SUCCEEDED(obj7.As(&iData7)),
                           "80a-7: COM QI failed on op " << op);

                bool fired7 = false;
                QTimer::singleShot(0, qApp, [&]() {
                    fired7 = true;
                    if (obj7)
                        obj7->Close(OLECLOSE_NOSAVE);
                    ipStg7.Reset();
                    ipFile7.Reset();
                    iData7.Reset();
                    obj7.Reset();
                });

                if (op == 0) {
                    obj7->Update();
                } else if (op == 1) {
                    ipStg7->IsDirty();
                } else if (op == 2) {
                    const std::wstring wPath = QDir::toNativeSeparators(
                                                       tmp80.filePath(QStringLiteral("ole3d_7.sciprj")))
                                                       .toStdWString();
                    ipFile7->Save(wPath.c_str(), FALSE);
                } else {
                    FORMATETC fmt7{ CF_ENHMETAFILE, nullptr, DVASPECT_CONTENT, -1, TYMED_ENHMF };
                    STGMEDIUM med7{};
                    if (SUCCEEDED(iData7->GetData(&fmt7, &med7)))
                        ReleaseStgMedium(&med7);
                }

                QApplication::setQuitOnLastWindowClosed(false);
                pumpEventsMs80(35);
                CHECK_COND(fired7 && ed7.isNull(),
                           "80a-7: re-entrant release failed on op " << op << " (fired=" << fired7
                                                                     << ", edNull=" << ed7.isNull()
                                                                     << ")");
            }

            // Op 4 (Round 11 ASan): IPersistFile::Load on Graph3D payload must hold alive(this)
            {
                const QString loadPath = tmp80.filePath(QStringLiteral("ole3d_load_7.sciprj"));
                QFile lf(loadPath);
                CHECK_COND(lf.open(QIODevice::WriteOnly)
                                   && lf.write(pay3d.projectBytes) == pay3d.projectBytes.size(),
                           "80a-7: failed to write ole3d_load_7.sciprj");
                lf.close();

                ComPtr<IOleObject> objLoad;
                CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&objLoad)))
                                   && objLoad,
                           "80a-7: createGraphObject failed on op 4 (IPersistFile::Load)");
                ComPtr<IPersistFile> ipFileLoad;
                CHECK_COND(SUCCEEDED(objLoad.As(&ipFileLoad)) && ipFileLoad,
                           "80a-7: IPersistFile QI failed on op 4");

                bool firedLoadFile = false;
                QTimer::singleShot(0, qApp, [&]() {
                    firedLoadFile = true;
                    ipFileLoad.Reset();
                    objLoad.Reset();
                });
                const std::wstring wLoadPath = QDir::toNativeSeparators(loadPath).toStdWString();
                const HRESULT hrLoadFile = ipFileLoad->Load(wLoadPath.c_str(), STGM_READ);
                const bool firedDuringLoadFile = firedLoadFile;
                CHECK_COND(SUCCEEDED(hrLoadFile) && firedDuringLoadFile,
                           "80a-7: IPersistFile::Load alive(this) guard failed mid-Load (hr=0x"
                                   << std::hex << hrLoadFile << std::dec
                                   << ", firedDuring=" << firedDuringLoadFile << ")");
                QApplication::setQuitOnLastWindowClosed(false);
                pumpEventsMs80(35);
            }

            // Op 5 (Round 11 ASan): IPersistStorage::Load on Graph3D storage with SciDAVisEmf
            // omitted must hold alive(this) across ensureRenderedPayload processEvents() pump.
            {
                ComPtr<ILockBytes> lbLoad;
                ComPtr<IStorage> stgLoad;
                CHECK_COND(SUCCEEDED(CreateILockBytesOnHGlobal(nullptr, TRUE, &lbLoad))
                                   && SUCCEEDED(StgCreateDocfileOnILockBytes(
                                           lbLoad.Get(),
                                           STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0,
                                           &stgLoad))
                                   && stgLoad,
                           "80a-7: StgCreateDocfileOnILockBytes failed on op 5");
                {
                    ComPtr<IStream> strmProj;
                    CHECK_COND(SUCCEEDED(stgLoad->CreateStream(
                                       L"SciDAVisProject",
                                       STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0,
                                       &strmProj))
                                       && strmProj,
                               "80a-7: CreateStream(SciDAVisProject) failed on op 5");
                    ULONG written = 0;
                    const HRESULT hrWr = strmProj->Write(
                            pay3d.projectBytes.constData(),
                            static_cast<ULONG>(pay3d.projectBytes.size()), &written);
                    CHECK_COND(SUCCEEDED(hrWr)
                                       && written == static_cast<ULONG>(pay3d.projectBytes.size()),
                               "80a-7: IStream::Write failed on op 5");
                }
                CHECK_COND(SUCCEEDED(stgLoad->Commit(STGC_DEFAULT)),
                           "80a-7: IStorage::Commit failed on op 5");

                ComPtr<IOleObject> objStgLoad;
                CHECK_COND(
                        SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&objStgLoad)))
                                && objStgLoad,
                        "80a-7: createGraphObject failed on op 5 (IPersistStorage::Load)");
                ComPtr<IPersistStorage> ipStgLoad;
                CHECK_COND(SUCCEEDED(objStgLoad.As(&ipStgLoad)) && ipStgLoad,
                           "80a-7: IPersistStorage QI failed on op 5");

                bool firedLoadStg = false;
                QTimer::singleShot(0, qApp, [&]() {
                    firedLoadStg = true;
                    ipStgLoad.Reset();
                    objStgLoad.Reset();
                });
                const HRESULT hrLoadStg = ipStgLoad->Load(stgLoad.Get());
                const bool firedDuringLoadStg = firedLoadStg;
                CHECK_COND(SUCCEEDED(hrLoadStg) && firedDuringLoadStg,
                           "80a-7: IPersistStorage::Load alive(this) guard failed mid-Load (hr=0x"
                                   << std::hex << hrLoadStg << std::dec
                                   << ", firedDuring=" << firedDuringLoadStg << ")");
                QApplication::setQuitOnLastWindowClosed(false);
                pumpEventsMs80(35);
            }

            // Op 6 (Round 12 ASan): IOleObject::DoVerb(OLEIVERB_HIDE) must hold alive(this)
            // and copy m_site locally if OnShowWindow clears site & drops last COM ref.
            {
                MockDeadClientSite80 siteVerb;
                ComPtr<IOleObject> objVerb;
                CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&objVerb)))
                                   && objVerb,
                           "80a-7: createGraphObject failed on op 6 (DoVerb)");
                CHECK_COND(SUCCEEDED(objVerb->SetClientSite(&siteVerb)),
                           "80a-7: SetClientSite failed on op 6");
                bool firedOnShowVerb = false;
                siteVerb.onShowWindow = [&](BOOL) {
                    firedOnShowVerb = true;
                    if (objVerb)
                        objVerb->SetClientSite(nullptr);
                    objVerb.Reset();
                };
                const HRESULT hrVerb =
                        objVerb->DoVerb(OLEIVERB_HIDE, nullptr, &siteVerb, 0, nullptr, nullptr);
                CHECK_COND(SUCCEEDED(hrVerb) && firedOnShowVerb && !objVerb,
                           "80a-7: DoVerb(OLEIVERB_HIDE) alive/site guard failed (hr=0x"
                                   << std::hex << hrVerb << std::dec
                                   << ", fired=" << firedOnShowVerb << ")");
                QApplication::setQuitOnLastWindowClosed(false);
                pumpEventsMs80(25);
            }

            // Op 7 (Round 12 ASan): IViewObject2::Draw must hold alive(this) if cont() drops
            // the last external COM reference mid-Draw().
            {
                struct DrawContCtx80
                {
                    bool fired = false;
                    ComPtr<IViewObject2> iView;
                    ComPtr<IOleObject> obj;
                    static BOOL CALLBACK onCont(ULONG_PTR token)
                    {
                        auto *ctx = reinterpret_cast<DrawContCtx80 *>(token);
                        if (ctx) {
                            ctx->fired = true;
                            ctx->iView.Reset();
                            ctx->obj.Reset();
                        }
                        return TRUE;
                    }
                } drawCtx;
                CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&drawCtx.obj)))
                                   && drawCtx.obj,
                           "80a-7: createGraphObject failed on op 7 (Draw)");
                CHECK_COND(SUCCEEDED(drawCtx.obj.As(&drawCtx.iView)) && drawCtx.iView,
                           "80a-7: IViewObject2 QI failed on op 7");

                HDC hdcDraw = CreateCompatibleDC(nullptr);
                HBITMAP hbmDraw = hdcDraw ? CreateCompatibleBitmap(hdcDraw, 64, 64) : nullptr;
                HGDIOBJ oldBmDraw = (hdcDraw && hbmDraw) ? SelectObject(hdcDraw, hbmDraw) : nullptr;
                RECTL boundsDraw{ 0, 0, 64, 64 };
                const HRESULT hrDraw = drawCtx.iView->Draw(
                        DVASPECT_CONTENT, -1, nullptr, nullptr, nullptr, hdcDraw, &boundsDraw,
                        nullptr, &DrawContCtx80::onCont, reinterpret_cast<ULONG_PTR>(&drawCtx));
                if (hdcDraw && oldBmDraw)
                    SelectObject(hdcDraw, oldBmDraw);
                if (hbmDraw)
                    DeleteObject(hbmDraw);
                if (hdcDraw)
                    DeleteDC(hdcDraw);
                CHECK_COND(SUCCEEDED(hrDraw) && drawCtx.fired && !drawCtx.obj && !drawCtx.iView,
                           "80a-7: IViewObject2::Draw alive(this) guard failed (hr=0x"
                                   << std::hex << hrDraw << std::dec << ", fired=" << drawCtx.fired
                                   << ")");
                QApplication::setQuitOnLastWindowClosed(false);
                pumpEventsMs80(25);
            }

            CHECK_COND(SciDAVisOle::liveGraphObjectCountForTesting() == baseLive7,
                       "80a-7: COM object leaked after alive(this) teardown (got "
                               << SciDAVisOle::liveGraphObjectCountForTesting() << ", expected "
                               << baseLive7 << ")");
        }
    }
#endif

    // -------------------------------------------------------------------------
    // 80b: RecalcManager::Auto undo, MultiLayer redo isolation, P0-1/P0-2/P1-1
    // -------------------------------------------------------------------------
    {
        Table *tRec = w80->newTable(5, 2, "T80Rec", "Rec80");
        for (int r = 0; r < 5; ++r) {
            tRec->setCell(r, 0, double(r));
            tRec->setCell(r, 1, double(2 * r));
        }
        MultiLayer *mlRec = w80->multilayerPlot(tRec, QStringList() << tRec->colName(1),
                                                Graph::Scatter);
        Graph *gRec = mlRec ? mlRec->activeGraph() : nullptr;
        CHECK_COND(gRec != nullptr && w80->project() && w80->project()->undoStack(),
                   "80b: failed to setup RecalcManager test objects");
        RecalcManager *rm = RecalcManager::instance(w80);
        CHECK_COND(rm != nullptr, "80b: RecalcManager::instance null");
        rm->setRecalcMode(RecalcManager::Auto);
        const int lid = rm->registerLink(w80, gRec, tRec, tRec->colName(1),
                                         QStringLiteral("Rec80Fit"), RecalcManager::OpLinearFit);
        CHECK_COND(lid > 0, "80b: registerLink failed");
        QCoreApplication::processEvents();

        QUndoStack *ustack = w80->project()->undoStack();
        w80->d_workspace.setActiveSubWindow(tRec);
        tRec->column(1)->setValueAt(4, 24.0);
        QCoreApplication::processEvents();

        QwtPlotCurve *fitEdited = gRec->curve(QStringLiteral("Rec80Fit"));
        CHECK_COND(fitEdited != nullptr && fitEdited->dataSize() >= 2,
                   "80b: Rec80Fit curve missing after cell edit");
        const double yEndEdited = fitEdited->y(fitEdited->dataSize() - 1);
        CHECK_COND(ustack->canUndo(), "80b: Project undoStack cannot undo after edit");

        ustack->undo();
        QCoreApplication::processEvents();
        CHECK_COND(ustack->canRedo(),
                   "80b: RecalcManager destroyed Project Redo stack during Undo replay!");
        fitEdited = gRec->curve(QStringLiteral("Rec80Fit"));
        CHECK_COND(fitEdited != nullptr && fitEdited->dataSize() > 0 && !rm->hasDirtyLinks(gRec),
                   "80b: dirty link or empty curve after Undo");
        const double yEndUndone = fitEdited->y(fitEdited->dataSize() - 1);
        CHECK_COND(std::fabs(yEndUndone - 8.0) < 1e-5,
                   "80b: fit curve did not revert on Undo (got " << yEndUndone << ")");

        ustack->redo();
        QCoreApplication::processEvents();
        fitEdited = gRec->curve(QStringLiteral("Rec80Fit"));
        CHECK_COND(fitEdited != nullptr && fitEdited->dataSize() > 0 && !rm->hasDirtyLinks(gRec),
                   "80b: dirty link or empty curve after Redo");
        const double yEndRedone = fitEdited->y(fitEdited->dataSize() - 1);
        CHECK_COND(std::fabs(yEndRedone - yEndEdited) < 1e-5,
                   "80b: fit curve did not replay on Redo (got " << yEndRedone << " expected "
                                                                 << yEndEdited << ")");
        rm->removeLinksForGraph(gRec);

        // mem_ole_recalc P1: Auto recalc fit must suppress MultiLayer undo stack so table edits
        // and graph-mask Undo do not push spurious "Fit Curve" entries or wipe ml->canRedo().
        {
            Table *tFitMl = w80->newTable(5, 2, "T80FitML", "FitML80");
            for (int r = 0; r < 5; ++r) {
                tFitMl->setCell(r, 0, double(r));
                tFitMl->setCell(r, 1, double(3 * r + 1));
            }
            MultiLayer *mlFit =
                    w80->multilayerPlot(tFitMl, QStringList() << tFitMl->colName(1), Graph::Scatter);
            Graph *gFit = mlFit ? mlFit->activeGraph() : nullptr;
            DataCurve *dcFitSrc = gFit ? dynamic_cast<DataCurve *>(gFit->curve(0)) : nullptr;
            CHECK_COND(gFit && dcFitSrc && mlFit->undoStack(), "80b-ml: setup failed");
            CHECK_COND(rm->registerLink(w80, gFit, tFitMl, tFitMl->colName(1),
                                        QStringLiteral("Rec80MLFit"), RecalcManager::OpLinearFit)
                               > 0,
                       "80b-ml: registerLink failed");
            mlFit->undoStack()->clear();
            gFit->runWithColumnMaskUndo(QStringLiteral("Mask Pt 1"),
                                        QList<DataCurve *>{ dcFitSrc },
                                        [&]() { dcFitSrc->maskPoint(1, true); });
            CHECK_COND(mlFit->undoStack()->count() == 1,
                       "80b-ml: expected 1 mask undo entry (got "
                               << mlFit->undoStack()->count() << ")");
            tFitMl->column(1)->setValueAt(4, 35.0);
            QCoreApplication::processEvents();
            CHECK_COND(mlFit->undoStack()->count() == 1,
                       "80b-ml: Auto recalc pushed spurious Fit Curve onto MultiLayer undoStack!");

            w80->d_workspace.setActiveSubWindow(mlFit);
            w80->undo();
            QCoreApplication::processEvents();
            CHECK_COND(mlFit->undoStack()->canRedo(),
                       "80b-ml: Auto recalc during graph mask Undo wiped MultiLayer Redo!");
            rm->removeLinksForGraph(gFit);
        }

        // P0-1 + P0-2 ASan regression: Auto-recalc OpIIRFilter alongside ROI Mask Region & Clear
        // All Masks + Project Undo after IIR output table cell edit & Auto recalc retirement.
        {
            Table *tIir = w80->newTable(24, 2, "T80IIR", "IIR80");
            for (int r = 0; r < 24; ++r) {
                tIir->setCell(r, 0, double(r) * 0.01);
                tIir->setCell(r, 1, double(r % 5) + 1.0);
            }
            MultiLayer *mlIir =
                    w80->multilayerPlot(tIir, QStringList() << tIir->colName(1), Graph::Line);
            Graph *gIir = mlIir ? mlIir->activeGraph() : nullptr;
            CHECK_COND(gIir && gIir->curve(0), "80b-iir: graph setup failed");
            rm->setRecalcMode(RecalcManager::Auto);
            QwtPlotCurve *iirCurve = AdvancedSignalProc::iirFilter(
                    w80, gIir, gIir->curve(0)->title().text(), AdvancedSignalProc::Butterworth,
                    AdvancedSignalProc::LowPass, 4, 10.0, 25.0, true, 1.0);
            CHECK_COND(iirCurve != nullptr && rm->activeLinkCount(gIir) == 1,
                       "80b-iir: iirFilter or link registration failed");

            // P0-1 ASan: ROI Mask Region over all visible curves [source S + Auto-linked IIR F]
            // triggers recalc retiring F; must NOT UAF F on next iteration!
            RoiGadgetTool *roi = RoiGadgetTool::attachGadget(w80, gIir, RoiGadgetTool::MaskRegion);
            CHECK_COND(roi != nullptr, "80b-iir: attachGadget failed");
            roi->setRoiRect(QRectF(-0.05, -2.0, 0.30, 10.0));
            const int maskedCnt = roi->applyClusterMask(true);
            QCoreApplication::processEvents();
            CHECK_COND(maskedCnt > 0 && tIir->column(1)->isMasked(0),
                       "80b-iir: ROI applyClusterMask failed (count=" << maskedCnt << ")");
            RoiGadgetTool::clearGadget(gIir);

            // P0-1 ASan: Clear All Masks on Graph while source & Auto-linked IIR curve share graph
            w80->d_workspace.setActiveSubWindow(mlIir);
            w80->clearAllMasksOnGraph();
            QCoreApplication::processEvents();
            CHECK_COND(!tIir->column(1)->isMasked(0), "80b-iir: clearAllMasksOnGraph failed");

            // P0-2 ASan: Edit a cell in retired IIR output table -> edit source (Auto recalc
            // suppresses undo and retires output table) -> press Undo twice (touches column on
            // retired table) -> Redo + undoStack->clear() without UAF!
            DataCurve *iirDc = nullptr;
            for (int i = 0; i < gIir->curves(); ++i) {
                auto *dc = dynamic_cast<DataCurve *>(gIir->curve(i));
                if (dc && dc->table() && dc->table() != tIir) {
                    iirDc = dc;
                    break;
                }
            }
            CHECK_COND(iirDc && iirDc->table(), "80b-iir: could not find current IIR output table");
            QPointer<Table> retiredOutTbl(iirDc->table());
            QTableView *tvOut = retiredOutTbl->findChild<QTableView *>();
            CHECK_COND(tvOut && tvOut->model(), "80b-iir: IIR output table view/model null");
            w80->project()->undoStack()->clear();
            CHECK_COND(tvOut->model()->setData(tvOut->model()->index(2, 1),
                                               QStringLiteral("77.5"), Qt::EditRole),
                       "80b-iir: failed to edit cell in IIR output table");

            QTableView *tvSrc = tIir->findChild<QTableView *>();
            CHECK_COND(tvSrc && tvSrc->model(), "80b-iir: source table view/model null");
            CHECK_COND(tvSrc->model()->setData(tvSrc->model()->index(3, 1),
                                               QStringLiteral("12.3"), Qt::EditRole),
                       "80b-iir: failed to edit cell in source table");
            QCoreApplication::processEvents();

            w80->d_workspace.setActiveSubWindow(tIir);
            w80->undo(); // undoes source edit
            QCoreApplication::processEvents();
            CHECK_COND(std::fabs(tIir->cell(3, 1) - 4.0) < 1e-6,
                       "80b-iir: first Undo did not restore source cell");
            w80->undo(); // P0-2 critical step: undoes cell edit in retired IIR output table!
            QCoreApplication::processEvents();
            w80->redo();
            QCoreApplication::processEvents();
            w80->project()->undoStack()->clear();
            QCoreApplication::processEvents();
            CHECK_COND(retiredOutTbl.isNull(),
                       "80b-iir: retired output table leaked after Project::undoStack()->clear()");
            rm->removeLinksForGraph(gIir);
        }

        // P1-1: Table::scheduleOrRunDependentRecalc runs immediately when Project undo is suppressed
        {
            Table *tForm = w80->newTable(3, 2, "T80FormSupp", "FormSupp80");
            tForm->setColName(0, QStringLiteral("X"));
            tForm->setColName(1, QStringLiteral("Y"));
            tForm->setCell(0, 0, 2.0);
            tForm->setCell(1, 0, 5.0);
            tForm->setCell(2, 0, 7.0);
            tForm->setCommand(1, QStringLiteral("col(\"X\") * 3.0"));
            tForm->recalculate(1, false);
            CHECK_COND(std::fabs(tForm->cell(1, 1) - 15.0) < 1e-9, "80b-form: initial formula failed");

            {
                ScopedProjectUndoSuppressor suppress(w80->project());
                tForm->column(0)->setValueAt(1, 10.0);
                QCoreApplication::processEvents();
                CHECK_COND(std::fabs(tForm->cell(1, 1) - 30.0) < 1e-9,
                           "80b-form: P1-1! formula column stalled while Project undo suppressed (got "
                                   << tForm->cell(1, 1) << ")");
            }
            // Subsequent edit after suppressor exits must still recalculate (flag not stuck!)
            tForm->column(0)->setValueAt(2, 11.0);
            QCoreApplication::processEvents();
            CHECK_COND(std::fabs(tForm->cell(2, 1) - 33.0) < 1e-9,
                       "80b-form: subsequent edit stalled after suppressed recalc (got "
                               << tForm->cell(2, 1) << ")");
        }

        // P0 (`Table::closeEvent`): synchronous Table destruction inside d_future_table->remove()
        // when Project undo is suppressed and undoStack is empty must NOT UAF this->d_future_table.
        {
            w80->project()->undoStack()->clear();
            QPointer<Table> tCloseGuard;
            {
                ScopedProjectUndoSuppressor suppressClose(w80->project());
                Table *tClose = w80->newTable(3, 2, "T80CloseUaf", "CloseUaf");
                CHECK_COND(tClose != nullptr, "80b-close-uaf: newTable failed");
                tClose->askOnCloseEvent(false);
                tCloseGuard = tClose;
                tClose->close();
                QCoreApplication::processEvents();
            }
            CHECK_COND(tCloseGuard.isNull()
                               && w80->table(QStringLiteral("T80CloseUaf")) == nullptr,
                       "80b-close-uaf: suppressed table did not close cleanly");

            // Also verify normal undo-enabled table close still retains for Project Undo
            Table *tUndoClose = w80->newTable(3, 2, "T80UndoClose", "UndoClose");
            CHECK_COND(tUndoClose != nullptr, "80b-close-uaf: undo newTable failed");
            tUndoClose->askOnCloseEvent(false);
            tUndoClose->setCell(0, 1, 42.5);
            tUndoClose->close();
            QCoreApplication::processEvents();
            CHECK_COND(w80->table(QStringLiteral("T80UndoClose")) == nullptr,
                       "80b-close-uaf: closed table still listed before Undo");
            w80->project()->undoStack()->undo();
            QCoreApplication::processEvents();
            Table *restoredClose = w80->table(QStringLiteral("T80UndoClose"));
            CHECK_COND(restoredClose != nullptr
                               && std::fabs(restoredClose->cell(0, 1) - 42.5) < 1e-9,
                       "80b-close-uaf: Undo did not restore closed table");
        }

        // Round 8 P0 (`ApplicationWindow::showHistory`): stepping forward inside QUndoView modal
        // exec() over insertRows() must run under ScopedUndoReplayGuard so handleRowsAboutToBeInserted
        // does not push a competing command onto d_project->undoStack() mid-setIndex().
        {
            Table *tHist = w80->newTable(3, 2, "T80Hist", "Hist80");
            CHECK_COND(tHist != nullptr && tHist->d_future_table != nullptr,
                       "80b-history: newTable failed");
            w80->d_workspace.setActiveSubWindow(tHist);
            QCoreApplication::processEvents();

            w80->project()->undoStack()->clear();
            tHist->d_future_table->insertRows(0, 2);
            QCoreApplication::processEvents();
            CHECK_COND(tHist->numRows() == 5 && w80->project()->undoStack()->count() == 1,
                       "80b-history: insertRows setup failed (rows=" << tHist->numRows() << ")");

            w80->project()->undoStack()->undo();
            QCoreApplication::processEvents();
            CHECK_COND(tHist->numRows() == 3 && w80->project()->undoStack()->canRedo(),
                       "80b-history: pre-showHistory undo failed (rows=" << tHist->numRows() << ")");

            struct HistInteractiveGuard
            {
                ApplicationWindow *app = nullptr;
                QVariant prevProp;
                bool prevHeadless = true;
                explicit HistInteractiveGuard(ApplicationWindow *w)
                    : app(w)
                    , prevProp(qApp->property("torigraph_noninteractive"))
                    , prevHeadless(w ? w->isHeadlessMode() : true)
                {
                    qApp->setProperty("torigraph_noninteractive", false);
                    if (app)
                        app->setHeadlessMode(false);
                }
                ~HistInteractiveGuard()
                {
                    qApp->setProperty("torigraph_noninteractive", prevProp);
                    if (app)
                        app->setHeadlessMode(prevHeadless);
                }
            } histInteractive(w80);

            bool sawHistDialog = false;
            bool replayingInsideDialog = false;
            int rowsAfterRedoInsideDialog = -1;
            int countAfterRedoInsideDialog = -1;
            QTimer histTimer;
            histTimer.setInterval(15);
            QObject::connect(&histTimer, &QTimer::timeout, [&]() {
                if (sawHistDialog)
                    return;
                QDialog *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dlg)
                    return;
                if (dlg->windowTitle().contains(QStringLiteral("History"))
                    || dlg->findChild<QUndoView *>() != nullptr) {
                    sawHistDialog = true;
                    replayingInsideDialog = Table::isUndoRedoReplaying();
                    QUndoStack *stk = w80->project()->undoStack();
                    stk->setIndex(stk->count());
                    rowsAfterRedoInsideDialog = tHist->numRows();
                    countAfterRedoInsideDialog = stk->count();
                    dlg->reject();
                }
            });
            histTimer.start();
            w80->showHistory();
            histTimer.stop();
            QCoreApplication::processEvents();

            CHECK_COND(sawHistDialog && replayingInsideDialog,
                       "80b-history: showHistory dialog/replay guard inactive (saw="
                               << sawHistDialog << ", replaying=" << replayingInsideDialog << ")");
            CHECK_COND(rowsAfterRedoInsideDialog == 5 && countAfterRedoInsideDialog == 1,
                       "80b-history: mid-redo inside showHistory pushed extra command or wrong rows (rows="
                               << rowsAfterRedoInsideDialog << ", count=" << countAfterRedoInsideDialog
                               << ")");
            CHECK_COND(!Table::isUndoRedoReplaying(),
                       "80b-history: ScopedUndoReplayGuard leaked replaying flag after showHistory");
            CHECK_COND(tHist->numRows() == 3 && w80->project()->undoStack()->canRedo()
                               && w80->project()->undoStack()->count() == 1,
                       "80b-history: Cancel did not cleanly restore row count & redo history (rows="
                               << tHist->numRows()
                               << ", canRedo=" << w80->project()->undoStack()->canRedo() << ")");
        }
    }

    // -------------------------------------------------------------------------
    // 80c: Split View divider resize invariance, Focus HUD focus restore, Viewport auto-fit
    // -------------------------------------------------------------------------
    {
        AppWinGuard80 wVis(new ApplicationWindow());
        wVis->applyUserSettings();
        wVis->confirmCloseTable = false;
        wVis->confirmClosePlot2D = false;
        wVis->confirmClosePlot3D = false;
        wVis->confirmCloseFolder = false;
        wVis->resize(1920, 1080);
        wVis->show();
        QCoreApplication::processEvents();

        Table *tSp = wVis->newTable(5, 2, "T80Split", "Sp80");
        for (int r = 0; r < 5; ++r) {
            tSp->setCell(r, 0, double(r));
            tSp->setCell(r, 1, double(r * 5 + 1));
        }
        MultiLayer *mlSp =
                wVis->multilayerPlot(tSp, QStringList() << tSp->colName(1), Graph::LineSymbols);
        CHECK_COND(mlSp != nullptr && wVis->modernRibbonBar() != nullptr,
                   "80c: failed to create split test widgets");

        ModernRibbonBar *rib = wVis->modernRibbonBar();
        rib->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
        rib->setSplitLeftRatio(0.40);
        CHECK_COND(mlSp->applyStylePresetById(QStringLiteral("isij_85mm")),
                   "80c: applyStylePresetById(isij_85mm) failed");
        pumpEventsMs80(50);
        CHECK_COND(std::fabs(rib->splitLeftRatio() - 0.40) < 0.02,
                   "80c: splitLeftRatio not 0.40 after setup");
        const double zoomLarge = mlSp->pageZoom();

        // Programmatic window resize (no mouse left button pressed) preserves 0.40 ratio
        // AND shrinks unconstrained Split viewport pageZoom when !d_user_zoomed.
        wVis->resize(1366, 768);
        pumpEventsMs80(40);
        CHECK_COND(std::fabs(rib->splitLeftRatio() - 0.40) < 0.02,
                   "80c: programmatic resize (1366x768) drifted splitLeftRatio to "
                           << rib->splitLeftRatio());

        wVis->resize(760, 420);
        waitUntilMs80(2000, [&]() { return mlSp->pageZoom() < zoomLarge - 0.10; });
        CHECK_COND(std::fabs(rib->splitLeftRatio() - 0.40) < 0.02,
                   "80c: programmatic shrink drifted splitLeftRatio to "
                           << rib->splitLeftRatio());
        const double zoomShrunk = mlSp->pageZoom();
        CHECK_COND(zoomShrunk < zoomLarge - 0.10,
                   "80c: Split MultiLayer shrink did not auto-fit pageZoom (large="
                           << zoomLarge << " shrunk=" << zoomShrunk << ")");

        mlSp->setPageZoom(1.35);
        pumpEventsMs80(20);
        wVis->resize(900, 480);
        // Poll across 120ms so any accidental auto-fit overwrite fails immediately while
        // confirming user zoom 1.35 holds after the coalesced 20ms viewport auto-fit window.
        waitUntilMs80(120, [&]() { return std::fabs(mlSp->pageZoom() - 1.35) > 1e-4; });
        CHECK_COND(std::fabs(mlSp->pageZoom() - 1.35) < 1e-4,
                   "80c: Split viewport resize overwrote explicit user zoom 1.35 with "
                           << mlSp->pageZoom());

        // Focus HUD exit must preserve previously active window
        rib->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
        wVis->activateSubWindow(mlSp);
        pumpEventsMs80(10);
        rib->setFocusHudMode(true);
        pumpEventsMs80(10);
        rib->setFocusHudMode(false);
        pumpEventsMs80(10);
        CHECK_COND(wVis->d_workspace.currentSubWindow() == mlSp,
                   "80c: Focus HUD exit did not preserve active MultiLayer window");
    }

    // -------------------------------------------------------------------------
    // 80d: Mask X+Y clear, graph mask undo/redo, removeLegend() safety
    // -------------------------------------------------------------------------
    {
        Table *tMask = w80->newTable(5, 2, "T80Mask", "Mask80");
        for (int r = 0; r < 5; ++r) {
            tMask->setCell(r, 0, double(r + 1));
            tMask->setCell(r, 1, double((r + 1) * 3));
        }
        MultiLayer *mlMask =
                w80->multilayerPlot(tMask, QStringList() << tMask->colName(1), Graph::Scatter);
        Graph *gMask = mlMask ? mlMask->activeGraph() : nullptr;
        DataCurve *dc = gMask ? dynamic_cast<DataCurve *>(gMask->curve(0)) : nullptr;
        CHECK_COND(gMask != nullptr && dc != nullptr, "80d: mask test graph/curve null");

        tMask->column(0)->setMasked(1, true);
        dc->maskPoint(1, true);
        CHECK_COND(tMask->column(0)->isMasked(1) && tMask->column(1)->isMasked(1),
                   "80d: failed to mask X and Y on row 1");
        dc->clearMasks();
        CHECK_COND(!tMask->column(0)->isMasked(1) && !tMask->column(1)->isMasked(1),
                   "80d: DataCurve::clearMasks() left X or Y mask behind");

        dc->maskPoint(1, true);
        dc->maskPoint(3, true);
        CHECK_COND(dc->maskedPointCount() == 2, "80d: expected 2 masked points");
        w80->project()->undoStack()->clear();
        if (mlMask->undoStack())
            mlMask->undoStack()->clear();
        w80->d_workspace.setActiveSubWindow(mlMask);
        w80->clearAllMasksOnGraph();
        QCoreApplication::processEvents();
        CHECK_COND(dc->maskedPointCount() == 0,
                   "80d: clearAllMasksOnGraph() did not clear all masks");
        QUndoStack *mlUndo = mlMask->undoStack();
        CHECK_COND(mlUndo && mlUndo->canUndo(),
                   "80d: clearAllMasksOnGraph() did not push Undo command to MultiLayer undoStack");
        CHECK_COND(w80->project()->undoStack()->count() == 0,
                   "80d: clearAllMasksOnGraph() leaked duplicate command onto Project undoStack");
        w80->undo();
        QCoreApplication::processEvents();
        CHECK_COND(dc->maskedPointCount() == 2 && mlUndo->canRedo(),
                   "80d: Undo of clearAllMasksOnGraph() did not restore 2 masks cleanly");
        CHECK_COND(w80->project()->undoStack()->count() == 0,
                   "80d: Undo of graph masks dirtied Project undoStack");
        w80->redo();
        QCoreApplication::processEvents();
        CHECK_COND(dc->maskedPointCount() == 0,
                   "80d: Redo of clearAllMasksOnGraph() did not re-clear masks");
        CHECK_COND(w80->project()->undoStack()->count() == 0,
                   "80d: Redo of graph masks dirtied Project undoStack");

        // removeLegend() safety under multi-select (selectedMarker != legendMarkerID) and repeat
        gMask->newLegend();
        CHECK_COND(gMask->hasLegend(), "80d: failed to add Legend");
        long legId = -1;
        for (int k : gMask->textMarkerKeys()) {
            if (gMask->plotWidget() && gMask->plotWidget()->marker(k) == gMask->legend()) {
                legId = k;
                break;
            }
        }
        CHECK_COND(legId >= 0, "80d: could not locate legend marker ID in textMarkerKeys");
        Legend note(gMask->plotWidget());
        note.setText(QStringLiteral("Note80"));
        const long noteId = gMask->insertTextMarker(&note);
        CHECK_COND(noteId >= 0, "80d: insertTextMarker failed");
        gMask->setSelectedMarker(legId, false);
        gMask->setSelectedMarker(noteId, true);
        gMask->removeLegend();
        gMask->removeLegend();
        pumpEventsMs80(10);
        gMask->deselectMarker();
        CHECK_COND(!gMask->hasLegend(), "80d: removeLegend() did not remove legend safely");

        // P1-2: Delta-only Graph mask Undo/Redo preserves concurrent Table masks AND tracks row deletion
        dc->clearMasks();
        if (mlMask->undoStack())
            mlMask->undoStack()->clear();
        gMask->runWithColumnMaskUndo(QStringLiteral("Mask Row 1"), QList<DataCurve *>{ dc },
                                     [&]() { dc->maskPoint(1, true); });
        CHECK_COND(tMask->column(1)->isMasked(1) && mlMask->undoStack()->count() == 1,
                   "80d-delta: failed to mask row 1 via graph mask undo");
        // Unrelated mask added directly on row 4 from the Table view
        tMask->column(1)->setMasked(4, true);
        CHECK_COND(tMask->column(1)->isMasked(4), "80d-delta: failed to mask row 4 on Table");

        mlMask->undoStack()->undo();
        QCoreApplication::processEvents();
        CHECK_COND(!tMask->column(1)->isMasked(1) && tMask->column(1)->isMasked(4),
                   "80d-delta: P1-2! Graph mask Undo wiped unrelated Table mask on row 4!");

        mlMask->undoStack()->redo();
        QCoreApplication::processEvents();
        CHECK_COND(tMask->column(1)->isMasked(1) && tMask->column(1)->isMasked(4),
                   "80d-delta: Graph mask Redo did not restore row 1 while keeping row 4");

        // Deleting row 0 shifts old row 1 -> row 0 and old row 4 -> row 3; Graph Undo must track index shift!
        tMask->d_future_table->removeRows(0, 1);
        QCoreApplication::processEvents();
        CHECK_COND(tMask->column(1)->isMasked(0) && tMask->column(1)->isMasked(3),
                   "80d-delta: expected masks at shifted rows 0 and 3 before Undo");
        mlMask->undoStack()->undo();
        QCoreApplication::processEvents();
        CHECK_COND(!tMask->column(1)->isMasked(0) && tMask->column(1)->isMasked(3),
                   "80d-delta: P1-2! Graph mask Undo after row deletion unmasked wrong row!");

        // 80d-unmask-click (Round 10): DataPickerTool::Unmask click directly on a masked point
        // must hit it via Plot::closestDataPoint(..., includeMasked=true, maskedOnly=true),
        // unmask it cleanly, and push a MultiLayer Undo command.
        {
            Table *tUnmask = w80->newTable(5, 2, "T80UnmaskClk", "UnmaskClk80");
            for (int r = 0; r < 5; ++r) {
                tUnmask->setCell(r, 0, double(r + 1));
                tUnmask->setCell(r, 1, double((r + 1) * 5));
            }
            MultiLayer *mlUnmask = w80->multilayerPlot(
                    tUnmask, QStringList() << tUnmask->colName(1), Graph::Scatter);
            Graph *gUnmask = mlUnmask ? mlUnmask->activeGraph() : nullptr;
            DataCurve *dcUnmask = gUnmask ? dynamic_cast<DataCurve *>(gUnmask->curve(0)) : nullptr;
            CHECK_COND(gUnmask && gUnmask->plotWidget() && gUnmask->plotWidget()->canvas()
                               && dcUnmask && dcUnmask->dataSize() == 5,
                       "80d-unmask-click: setup failed");

            const bool wasVisible = w80->isVisible();
            w80->show();
            w80->resize(1024, 768);
            w80->d_workspace.setActiveSubWindow(mlUnmask);
            mlUnmask->show();
            mlUnmask->resize(440, 340);
            gUnmask->updatePlot();
            pumpEventsMs80(30);

            dcUnmask->maskPoint(2, true);
            CHECK_COND(dcUnmask->isPointMasked(2), "80d-unmask-click: pre-mask point 2 failed");

            const QPoint pt2(gUnmask->plotWidget()->transform(dcUnmask->xAxis(), dcUnmask->x(2)),
                             gUnmask->plotWidget()->transform(dcUnmask->yAxis(), dcUnmask->y(2)));
            int dDef = 99999, vDef = -1;
            const int cDef = gUnmask->plotWidget()->closestDataPoint(pt2, dDef, vDef);
            CHECK_COND(!(cDef >= 0 && vDef == 2),
                       "80d-unmask-click: default closestDataPoint unexpectedly hit masked point 2");

            int dUmk = 99999, vUmk = -1;
            const int cUmk =
                    gUnmask->plotWidget()->closestDataPoint(pt2, dUmk, vUmk, true, true);
            CHECK_COND(cUmk >= 0 && vUmk == 2 && dUmk < 10,
                       "80d-unmask-click: maskedOnly closestDataPoint missed point 2 (c="
                               << cUmk << ", v=" << vUmk << ", d=" << dUmk << ")");

            DataPickerTool *unmaskTool =
                    new DataPickerTool(gUnmask, w80, DataPickerTool::Unmask);
            gUnmask->setActiveTool(unmaskTool);
            if (mlUnmask->undoStack())
                mlUnmask->undoStack()->clear();

            QWidget *cv = gUnmask->plotWidget()->canvas();
            QMouseEvent pressEv(QEvent::MouseButtonPress, pt2, Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier);
            QMouseEvent releaseEv(QEvent::MouseButtonRelease, pt2, Qt::LeftButton, Qt::NoButton,
                                  Qt::NoModifier);
            QApplication::sendEvent(cv, &pressEv);
            QApplication::sendEvent(cv, &releaseEv);
            QCoreApplication::processEvents();

            CHECK_COND(!dcUnmask->isPointMasked(2),
                       "80d-unmask-click: clicking masked point in DataPickerTool::Unmask did not unmask point 2");
            CHECK_COND(mlUnmask->undoStack() && mlUnmask->undoStack()->canUndo(),
                       "80d-unmask-click: DataPickerTool::Unmask did not push MultiLayer Undo");
            mlUnmask->undoStack()->undo();
            QCoreApplication::processEvents();
            CHECK_COND(dcUnmask->isPointMasked(2),
                       "80d-unmask-click: Undo did not restore mask on point 2");
            gUnmask->setActiveTool(nullptr);
            if (!wasVisible)
                w80->hide();
        }
    }

    // -------------------------------------------------------------------------
    // 80e: Locale RejectGroupSeparator, ScaleDraw, Dialogs, P0-3/P1-4 Log-axis Fit
    // -------------------------------------------------------------------------
    {
        w80->switchToLanguage(QStringLiteral("en"));
        QCoreApplication::processEvents();

        // num_locale_fit P1: en_US TableModel::setData + String2DoubleFilter (default
        // OmitGroupSeparator) and MyParser::Eval multi-expression rejection (nRes != 1).
        {
            QLocale enOmit(QLocale::English, QLocale::UnitedStates);
            enOmit.setNumberOptions(enOmit.numberOptions() | QLocale::OmitGroupSeparator);
            LocaleGuard80 enUsLocale(enOmit);
            Table *tEn = w80->newTable(2, 1, "T80EnGroup", "EnGroup80");
            tEn->setCell(0, 0, 10.0);
            QTableView *tvEn = tEn->findChild<QTableView *>();
            CHECK_COND(tvEn != nullptr && tvEn->model() != nullptr, "80e-en: TableView/model null");
            QAbstractItemModel *tmEn = tvEn->model();
            tmEn->setData(tmEn->index(1, 0), QStringLiteral("1,250"), Qt::EditRole);
            CHECK_COND(tEn->column(0)->isInvalid(1),
                       "80e-en: P1 regression! TableModel accepted '1,250' in en_US as "
                               << tEn->cell(1, 0) << " instead of marking invalid");

            // P1: String2DoubleFilter (Text -> Numeric conversion) must reject "1,250" under
            // OmitGroupSeparator in en_US.
            Table *tTxtEn = w80->newTable(2, 1, "T80EnFilter", "EnFilter80");
            Column *txtCol = tTxtEn->column(0);
            CHECK_COND(txtCol != nullptr, "80e-en: tTxtEn column null");
            txtCol->setColumnMode(SciDAVis::ColumnMode::Text);
            txtCol->setTextAt(0, QStringLiteral("10"));
            txtCol->setTextAt(1, QStringLiteral("1,250"));
            txtCol->setColumnMode(SciDAVis::ColumnMode::Numeric);
            CHECK_COND(!txtCol->isInvalid(0) && std::fabs(txtCol->valueAt(0) - 10.0) < 1e-9,
                       "80e-en: String2DoubleFilter failed on valid '10'");
            CHECK_COND(txtCol->isInvalid(1),
                       "80e-en: P1 regression! String2DoubleFilter accepted '1,250' under en_US OmitGroupSeparator as "
                               << txtCol->valueAt(1));

            // P1 (80e-muparser): reject top-level comma-separated multi-expressions across
            // ScaleDraw, PlotDialog, AxesDialog, and FitDialog while preserving multi-arg funcs.
            {
                ScaleDraw sdCommaRej;
                sdCommaRej.setFormulaString(QStringLiteral("x,5"));
                CHECK_COND(std::fabs(sdCommaRej.transformValue(10.0)) < 1e-12,
                           "80e-muparser: ScaleDraw failed to reject 'x,5' (got "
                                   << sdCommaRej.transformValue(10.0) << ")");
                ScaleDraw sdMinOk;
                sdMinOk.setFormulaString(QStringLiteral("min(x, 5)"));
                CHECK_COND(std::fabs(sdMinOk.transformValue(10.0) - 5.0) < 1e-9,
                           "80e-muparser: ScaleDraw multi-arg min(x, 5) failed (got "
                                   << sdMinOk.transformValue(10.0) << ")");

                PlotDialog pdHistEn(false, w80);
                QGroupBox *grpHistEn = nullptr;
                for (QGroupBox *gb : pdHistEn.findChildren<QGroupBox *>()) {
                    if (auto *gl = qobject_cast<QGridLayout *>(gb->layout())) {
                        auto *it0 = gl->itemAtPosition(0, 1);
                        auto *it1 = gl->itemAtPosition(1, 1);
                        auto *it2 = gl->itemAtPosition(2, 1);
                        if (gl->rowCount() >= 3 && gl->columnCount() >= 2 && it0 && it1 && it2
                            && qobject_cast<QLineEdit *>(it0->widget())
                            && qobject_cast<QLineEdit *>(it1->widget())
                            && qobject_cast<QLineEdit *>(it2->widget())) {
                            grpHistEn = gb;
                            break;
                        }
                    }
                }
                CHECK_COND(grpHistEn != nullptr,
                           "80e-muparser: PlotDialog histogram QGroupBox not found");
                auto *glEn = qobject_cast<QGridLayout *>(grpHistEn->layout());
                auto *binLeEn = qobject_cast<QLineEdit *>(glEn->itemAtPosition(0, 1)->widget());
                auto *begLeEn = qobject_cast<QLineEdit *>(glEn->itemAtPosition(1, 1)->widget());
                auto *endLeEn = qobject_cast<QLineEdit *>(glEn->itemAtPosition(2, 1)->widget());
                binLeEn->setText(QStringLiteral("1,250"));
                begLeEn->setText(QStringLiteral("0"));
                endLeEn->setText(QStringLiteral("10"));
                double hbEn = 0.0, heEn = 0.0, hbwEn = 0.0;
                bool histOkEn = true;
                const bool invEnOk = QMetaObject::invokeMethod(
                        &pdHistEn, "evaluateHistogramInputs", Qt::DirectConnection,
                        Q_RETURN_ARG(bool, histOkEn), Q_ARG(double *, &hbEn),
                        Q_ARG(double *, &heEn), Q_ARG(double *, &hbwEn), Q_ARG(bool, false));
                CHECK_COND(invEnOk && !histOkEn,
                           "80e-muparser: PlotDialog::evaluateHistogramInputs accepted '1,250' as "
                                   << hbwEn);

                Table *tMu = w80->newTable(5, 2, "T80MuEn", "MuEn80");
                for (int r = 0; r < 5; ++r) {
                    tMu->setCell(r, 0, double(r));
                    tMu->setCell(r, 1, double(2 * r + 1));
                }
                MultiLayer *mlMu =
                        w80->multilayerPlot(tMu, QStringList() << tMu->colName(1), Graph::Scatter);
                Graph *gMu = mlMu ? mlMu->activeGraph() : nullptr;
                CHECK_COND(gMu && gMu->curve(0), "80e-muparser: gMu setup failed");

                auto installModalBoxAutoAccept = [](QTimer &timer, bool &sawBox) {
                    sawBox = false;
                    timer.setInterval(10);
                    QObject::connect(&timer, &QTimer::timeout, [&sawBox]() {
                        if (auto *mb = qobject_cast<QMessageBox *>(
                                    QApplication::activeModalWidget())) {
                            sawBox = true;
                            mb->accept();
                            return;
                        }
                        for (QWidget *tlw : QApplication::topLevelWidgets()) {
                            if (auto *mb = qobject_cast<QMessageBox *>(tlw)) {
                                if (mb->isVisible()) {
                                    sawBox = true;
                                    mb->accept();
                                    return;
                                }
                            }
                        }
                    });
                    timer.start();
                };

                {
                    bool sawAxMsgBox = false;
                    QTimer axBoxTimer;
                    installModalBoxAutoAccept(axBoxTimer, sawAxMsgBox);
                    AxesDialogProbe80 axEnDlg;
                    axEnDlg.setGraph(gMu);
                    axEnDlg.setScaleBoundsAndUpdate(QStringLiteral("0"), QStringLiteral("1,250"));
                    axBoxTimer.stop();
                    const QwtScaleDiv *sdMuX = gMu->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
                    CHECK_COND(sawAxMsgBox && sdMuX && sdMuX->upperBound() < 200.0,
                               "80e-muparser: AxesDialog failed to reject '1,250' (sawBox="
                                       << sawAxMsgBox << " upper="
                                       << (sdMuX ? sdMuX->upperBound() : -1.0) << ")");
                }

                // Round 7 P1 coverage: AxesDialog::updatePlot() tick-formula nRes != 1 rejection
                {
                    bool sawFormMsgBox = false;
                    QTimer formBoxTimer;
                    installModalBoxAutoAccept(formBoxTimer, sawFormMsgBox);
                    AxesDialogProbe80 axFormDlg;
                    axFormDlg.setGraph(gMu);
                    axFormDlg.setTickFormulaAndUpdate(QStringLiteral("x,5"));
                    formBoxTimer.stop();
                    CHECK_COND(sawFormMsgBox
                                       && gMu->getAxesFormulas().value(QwtPlot::xBottom)
                                               != QStringLiteral("x,5"),
                               "80e-muparser: AxesDialog tick formula failed to reject 'x,5' (sawBox="
                                       << sawFormMsgBox << ")");
                }

                {
                    EnvVarGuard80 fitEnIniGuard(
                            "TORIGRAPH_SETTINGS_INI",
                            QFile::encodeName(tmp80.filePath(QStringLiteral("fit_en_mu_80.ini"))));
                    FitDialog fdEn(w80);
                    fdEn.setGraph(gMu);
                    CHECK_COND(fdEn.selectRecentModel(QStringLiteral("Polynomial")),
                               "80e-muparser: selectRecentModel(Polynomial) failed");
                    fdEn.showFitPage();
                    QLineEdit *leFromEn = nullptr;
                    QLineEdit *leToEn = nullptr;
                    for (QGroupBox *gb : fdEn.findChildren<QGroupBox *>()) {
                        if (auto *gl = qobject_cast<QGridLayout *>(gb->layout())) {
                            for (int row = 0; row < gl->rowCount(); ++row) {
                                auto *lblItem = gl->itemAtPosition(row, 0);
                                auto *edItem = gl->itemAtPosition(row, 1);
                                auto *lbl = lblItem ? qobject_cast<QLabel *>(lblItem->widget())
                                                    : nullptr;
                                auto *le = edItem ? qobject_cast<QLineEdit *>(edItem->widget())
                                                  : nullptr;
                                if (!lbl || !le)
                                    continue;
                                if (lbl->text().startsWith(QLatin1String("From x")))
                                    leFromEn = le;
                                else if (lbl->text().startsWith(QLatin1String("To x")))
                                    leToEn = le;
                            }
                        }
                    }
                    CHECK_COND(leFromEn && leToEn,
                               "80e-muparser: FitDialog boxFrom/boxTo not found");
                    leFromEn->setText(QStringLiteral("0"));
                    leToEn->setText(QStringLiteral("1,250"));
                    const int preCurvesEn = gMu->curves();
                    bool sawFitMsgBox = false;
                    QTimer fitBoxTimer;
                    installModalBoxAutoAccept(fitBoxTimer, sawFitMsgBox);
                    fdEn.accept();
                    fitBoxTimer.stop();
                    QCoreApplication::processEvents();
                    CHECK_COND(sawFitMsgBox && gMu->curves() == preCurvesEn,
                               "80e-muparser: FitDialog failed to reject '1,250' (sawBox="
                                       << sawFitMsgBox << " curves=" << gMu->curves() << "/"
                                       << preCurvesEn << ")");
                }

                // Round 7 P0 ASan regression: destroying FitDialog during Filter::isDataAcceptable
                // (Sub-check A) or FitDialog::accept() (Sub-check B) modal QMessageBox must NOT
                // double-free or UAF d_fitter / FitDialog (`this`) / child QMessageBox.
                {
                    struct InteractiveGuard80
                    {
                        ApplicationWindow *app = nullptr;
                        QVariant prevNonInteractive;
                        bool prevHeadless = true;
                        explicit InteractiveGuard80(ApplicationWindow *w)
                            : app(w)
                            , prevNonInteractive(qApp->property("torigraph_noninteractive"))
                            , prevHeadless(w ? w->isHeadlessMode() : true)
                        {
                            qApp->setProperty("torigraph_noninteractive", false);
                            if (app)
                                app->setHeadlessMode(false);
                        }
                        ~InteractiveGuard80()
                        {
                            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
                            if (app)
                                app->setHeadlessMode(prevHeadless);
                        }
                    };

                    EnvVarGuard80 fitDeadIniGuard(
                            "TORIGRAPH_SETTINGS_INI",
                            QFile::encodeName(tmp80.filePath(QStringLiteral("fit_dead_80.ini"))));

                    auto runFitSelfDestructModalCheck =
                            [&](const char *label, Table *tbl, const QString &modelName,
                                const QString &expectedMsgFragment) -> int {
                                MultiLayer *mlFitDead = w80->multilayerPlot(
                                        tbl, QStringList() << tbl->colName(1), Graph::Scatter);
                                Graph *gFitDead = mlFitDead ? mlFitDead->activeGraph() : nullptr;
                                CHECK_COND(gFitDead && gFitDead->curve(0),
                                           label << ": gFitDead setup failed");

                                QPointer<FitDialog> fdDead = new FitDialog(w80);
                                fdDead->setAttribute(Qt::WA_DeleteOnClose, true);
                                fdDead->setGraph(gFitDead);
                                CHECK_COND(fdDead->selectRecentModel(modelName),
                                           label << ": selectRecentModel("
                                                 << modelName.toStdString() << ") failed");
                                fdDead->showFitPage();

                                bool sawFitDeadBox = false;
                                bool reenteredAccept = false;
                                QString capturedBoxText;
                                QTimer fitDeadTimer;
                                fitDeadTimer.setInterval(10);
                                QObject::connect(&fitDeadTimer, &QTimer::timeout, [&]() {
                                    QMessageBox *mb = qobject_cast<QMessageBox *>(
                                            QApplication::activeModalWidget());
                                    if (!mb) {
                                        for (QWidget *tlw : QApplication::topLevelWidgets()) {
                                            if (auto *cand = qobject_cast<QMessageBox *>(tlw)) {
                                                if (cand->isVisible()) {
                                                    mb = cand;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    if (!mb)
                                        return;
                                    sawFitDeadBox = true;
                                    capturedBoxText = mb->text();
                                    if (!reenteredAccept && fdDead) {
                                        reenteredAccept = true;
                                        fdDead->accept();
                                    }
                                    if (fdDead) {
                                        QPointer<FitDialog> target = fdDead;
                                        target->close();
                                        if (target)
                                            delete target.data();
                                    }
                                    mb->accept();
                                });
                                InteractiveGuard80 interactive80(w80);
                                fitDeadTimer.start();
                                fdDead->accept();
                                fitDeadTimer.stop();
                                pumpEventsMs80(25);
                                CHECK_COND(sawFitDeadBox && fdDead.isNull()
                                                   && (expectedMsgFragment.isEmpty()
                                                       || capturedBoxText.contains(
                                                               expectedMsgFragment)),
                                           label << ": FitDialog self-destruction during modal QMessageBox failed (sawBox="
                                                 << sawFitDeadBox << ", null=" << fdDead.isNull()
                                                 << ", text='" << capturedBoxText.toStdString()
                                                 << "')");
                                return 0;
                            };

                    // Sub-check A: 2 points with ExpDecay1 (d_min_points=3) passes start < end and
                    // hits Filter::isDataAcceptable() (`Filter.cpp:222`) modal QMessageBox.
                    Table *tFitDeadA = w80->newTable(2, 2, "T80FitDeadA", "FitDeadA80");
                    tFitDeadA->setCell(0, 0, 1.0);
                    tFitDeadA->setCell(0, 1, 2.0);
                    tFitDeadA->setCell(1, 0, 2.0);
                    tFitDeadA->setCell(1, 1, 5.0);
                    if (runFitSelfDestructModalCheck("80e-fit-self-destruct-A", tFitDeadA,
                                                     QStringLiteral("ExpDecay1"),
                                                     QStringLiteral("at least 3 points"))
                        != 0)
                        return 1;

                    // Sub-check B: 1 point with Polynomial (start >= end) hits FitDialog's own
                    // parentWidget()-parented modal QMessageBox (`FitDialog.cpp`).
                    Table *tFitDeadB = w80->newTable(1, 2, "T80FitDeadB", "FitDeadB80");
                    tFitDeadB->setCell(0, 0, 1.0);
                    tFitDeadB->setCell(0, 1, 2.0);
                    if (runFitSelfDestructModalCheck("80e-fit-self-destruct-B", tFitDeadB,
                                                     QStringLiteral("Polynomial"),
                                                     QStringLiteral("from < end"))
                        != 0)
                        return 1;

                    // 80e-fit-silent (Round 9): Under interactive mode (`torigraph_noninteractive
                    // == false` and headless == false), calling FitDialog::showFitPage() and
                    // enabling Live Preview (updateLivePreview()) on a 1-point ExpDecay1 curve must
                    // suppress Filter error dialogs via setSilent(true) -> zero modal popups.
                    {
                        Table *tFitSil = w80->newTable(1, 2, "T80FitSilent", "FitSilent80");
                        tFitSil->setCell(0, 0, 1.0);
                        tFitSil->setCell(0, 1, 2.0);
                        MultiLayer *mlFitSil = w80->multilayerPlot(
                                tFitSil, QStringList() << tFitSil->colName(1), Graph::Scatter);
                        Graph *gFitSil = mlFitSil ? mlFitSil->activeGraph() : nullptr;
                        CHECK_COND(gFitSil && gFitSil->curve(0),
                                   "80e-fit-silent: gFitSil setup failed");

                        InteractiveGuard80 interactiveSil(w80);
                        bool sawUnexpectedModal = false;
                        QTimer silTimer;
                        silTimer.setInterval(10);
                        QObject::connect(&silTimer, &QTimer::timeout, [&]() {
                            if (auto *mb =
                                        qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                                sawUnexpectedModal = true;
                                mb->accept();
                            }
                        });
                        silTimer.start();

                        FitDialog fdSil(w80);
                        fdSil.setGraph(gFitSil);
                        CHECK_COND(fdSil.selectRecentModel(QStringLiteral("ExpDecay1")),
                                   "80e-fit-silent: selectRecentModel(ExpDecay1) failed");
                        fdSil.showFitPage();
                        QCoreApplication::processEvents();

                        QCheckBox *chkPreview = nullptr;
                        for (QCheckBox *cb : fdSil.findChildren<QCheckBox *>()) {
                            if (cb->text().contains(QStringLiteral("Preview"))) {
                                chkPreview = cb;
                                break;
                            }
                        }
                        CHECK_COND(chkPreview != nullptr,
                                   "80e-fit-silent: Live Preview QCheckBox not found");
                        chkPreview->setChecked(true);
                        QCoreApplication::processEvents();
                        silTimer.stop();
                        CHECK_COND(!sawUnexpectedModal,
                                   "80e-fit-silent: showFitPage()/updateLivePreview() opened modal popup under interactive mode!");
                    }
                }
            }
        }

        {
            QLocale deOmit(QLocale::German, QLocale::Germany);
            deOmit.setNumberOptions(deOmit.numberOptions() | QLocale::OmitGroupSeparator);
            LocaleGuard80 deLocale(deOmit);
            Table *tDe = w80->newTable(6, 2, "T80De", "De80");
            const double xDe[6] = { 1.0, 1.3, 1.6, 1.9, 2.2, 3.0 };
            for (int r = 0; r < 6; ++r) {
                tDe->setCell(r, 0, xDe[r]);
                tDe->setCell(r, 1, 2.0 * xDe[r] * xDe[r] + 1.0);
            }
            QTableView *tvDe = tDe->findChild<QTableView *>();
            CHECK_COND(tvDe != nullptr && tvDe->model() != nullptr, "80e: TableView/model null");
            QAbstractItemModel *tm = tvDe->model();
            tm->setData(tm->index(0, 1), QStringLiteral("1.250"), Qt::EditRole);
            CHECK_COND(!tDe->column(1)->isInvalid(0) && std::fabs(tDe->cell(0, 1) - 1.25) < 1e-9,
                       "80e: de_DE RejectGroupSeparator failed — '1.250' parsed as "
                               << tDe->cell(0, 1) << " instead of C-decimal 1.25");
            tm->setData(tm->index(0, 1), QStringLiteral("1,250"), Qt::EditRole);
            CHECK_COND(!tDe->column(1)->isInvalid(0) && std::fabs(tDe->cell(0, 1) - 1.25) < 1e-9,
                       "80e: de_DE decimal comma '1,250' did not parse as 1.25 (got "
                               << tDe->cell(0, 1) << ")");
            tDe->setCell(0, 1, 3.0);

            ScaleDraw sdComma;
            sdComma.setFormulaString(QStringLiteral("x*1,5"));
            CHECK_COND(std::fabs(sdComma.transformValue(10.0) - 15.0) < 1e-9,
                       "80e: ScaleDraw comma-decimal formula x*1,5 failed (got "
                               << sdComma.transformValue(10.0) << ")");

            ScaleDraw sdAvg;
            sdAvg.setFormulaString(QStringLiteral("avg(x,20)"));
            CHECK_COND(std::fabs(sdAvg.transformValue(10.0) - 15.0) < 1e-9,
                       "80e: ScaleDraw multi-arg avg(x,20) corrupted commas (got "
                               << sdAvg.transformValue(10.0) << ")");

            // Dialog locale checks under de_DE: AxesDialog, PlotDialog histogram, FitDialog
            MultiLayer *mlDe =
                    w80->multilayerPlot(tDe, QStringList() << tDe->colName(1), Graph::Scatter);
            Graph *gDe = mlDe ? mlDe->activeGraph() : nullptr;
            CHECK_COND(gDe != nullptr && gDe->curve(0) != nullptr, "80e-dlg: gDe null");

            {
                AxesDialogProbe80 axDlg;
                axDlg.setGraph(gDe);
                axDlg.setScaleBoundsAndUpdate(QStringLiteral("1,250"), QStringLiteral("2.500"));
                const QwtScaleDiv *sdX = gDe->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
                CHECK_COND(sdX && std::fabs(sdX->lowerBound() - 1.25) < 1e-6
                                   && std::fabs(sdX->upperBound() - 2.5) < 1e-6,
                           "80e-dlg: AxesDialog de_DE scale parsing wrong ("
                                   << (sdX ? sdX->lowerBound() : 0.0) << ", "
                                   << (sdX ? sdX->upperBound() : 0.0) << ")");
            }

            {
                PlotDialog pdHist(false, w80);
                QGroupBox *grpHist = nullptr;
                for (QGroupBox *gb : pdHist.findChildren<QGroupBox *>()) {
                    if (auto *gl = qobject_cast<QGridLayout *>(gb->layout())) {
                        auto *it0 = gl->itemAtPosition(0, 1);
                        auto *it1 = gl->itemAtPosition(1, 1);
                        auto *it2 = gl->itemAtPosition(2, 1);
                        if (gl->rowCount() >= 3 && gl->columnCount() >= 2 && it0 && it1 && it2
                            && qobject_cast<QLineEdit *>(it0->widget())
                            && qobject_cast<QLineEdit *>(it1->widget())
                            && qobject_cast<QLineEdit *>(it2->widget())) {
                            grpHist = gb;
                            break;
                        }
                    }
                }
                CHECK_COND(grpHist != nullptr, "80e-dlg: PlotDialog histogram QGroupBox not found");
                auto *gl = qobject_cast<QGridLayout *>(grpHist->layout());
                auto *binLe = qobject_cast<QLineEdit *>(gl->itemAtPosition(0, 1)->widget());
                auto *begLe = qobject_cast<QLineEdit *>(gl->itemAtPosition(1, 1)->widget());
                auto *endLe = qobject_cast<QLineEdit *>(gl->itemAtPosition(2, 1)->widget());
                binLe->setText(QStringLiteral("0,5"));
                begLe->setText(QStringLiteral("1,250"));
                endLe->setText(QStringLiteral("2.500"));
                double hb = 0.0, he = 0.0, hbw = 0.0;
                bool histOk = false;
                const bool invOk = QMetaObject::invokeMethod(
                        &pdHist, "evaluateHistogramInputs", Qt::DirectConnection,
                        Q_RETURN_ARG(bool, histOk), Q_ARG(double *, &hb), Q_ARG(double *, &he),
                        Q_ARG(double *, &hbw), Q_ARG(bool, false));
                CHECK_COND(invOk && histOk && std::fabs(hb - 1.25) < 1e-9
                                   && std::fabs(he - 2.5) < 1e-9
                                   && std::fabs(hbw - 0.5) < 1e-9,
                           "80e-dlg: PlotDialog::evaluateHistogramInputs de_DE parsing failed ("
                                   << hb << ", " << he << ", " << hbw << ")");
            }

            {
                EnvVarGuard80 fitIniGuard(
                        "TORIGRAPH_SETTINGS_INI",
                        QFile::encodeName(tmp80.filePath(QStringLiteral("fit_de_80.ini"))));
                FitDialog fdDe(w80);
                fdDe.setGraph(gDe);
                CHECK_COND(fdDe.selectRecentModel(QStringLiteral("Polynomial")),
                           "80e-dlg: selectRecentModel(Polynomial) failed");
                fdDe.showFitPage();
                QLineEdit *leFrom = nullptr;
                QLineEdit *leTo = nullptr;
                for (QGroupBox *gb : fdDe.findChildren<QGroupBox *>()) {
                    if (auto *gl = qobject_cast<QGridLayout *>(gb->layout())) {
                        for (int row = 0; row < gl->rowCount(); ++row) {
                            auto *lblItem = gl->itemAtPosition(row, 0);
                            auto *edItem = gl->itemAtPosition(row, 1);
                            auto *lbl = lblItem ? qobject_cast<QLabel *>(lblItem->widget()) : nullptr;
                            auto *le = edItem ? qobject_cast<QLineEdit *>(edItem->widget()) : nullptr;
                            if (!lbl || !le)
                                continue;
                            if (lbl->text().startsWith(QLatin1String("From x")))
                                leFrom = le;
                            else if (lbl->text().startsWith(QLatin1String("To x")))
                                leTo = le;
                        }
                    }
                }
                CHECK_COND(leFrom && leTo, "80e-dlg: FitDialog boxFrom/boxTo not found");
                leFrom->setText(QStringLiteral("1,250"));
                leTo->setText(QStringLiteral("2.500"));
                const int preCurves = gDe->curves();
                fdDe.accept();
                QCoreApplication::processEvents();
                CHECK_COND(gDe->curves() > preCurves,
                           "80e-dlg: FitDialog::accept under de_DE ('1,250'..'2.500') failed to fit");
            }
        }

        // P0-3 + P1-4 + sdk_tests_hygiene 80e: Apparent log-Y LinearFit with excluded y = -5
        {
            Table *tLogY = w80->newTable(5, 2, "T80LogYExcl", "LogYExcl80");
            const double xsY[5] = { 1.0, 2.0, 3.0, 3.5, 4.0 };
            const double ysY[5] = { 1.1, 9.5, 105.0, -5.0, 980.0 };
            for (int r = 0; r < 5; ++r) {
                tLogY->setCell(r, 0, xsY[r]);
                tLogY->setCell(r, 1, ysY[r]);
            }
            MultiLayer *mlLogY =
                    w80->multilayerPlot(tLogY, QStringList() << tLogY->colName(1), Graph::Scatter);
            Graph *gLogY = mlLogY ? mlLogY->activeGraph() : nullptr;
            CHECK_COND(gLogY && gLogY->curve(0), "80e-logy: setup failed");
            gLogY->setScale(QwtPlot::xBottom, 0.5, 5.0, 0.0, 5, 5, 0);
            gLogY->setScale(QwtPlot::yLeft, 0.5, 5000.0, 0.0, 5, 5, 1);

            LinearFit lfEx(w80, gLogY, gLogY->curve(0)->title().text());
            lfEx.setApparentFit(true);
            lfEx.setSilent(true);
            lfEx.fit();
            CHECK_COND(std::isfinite(lfEx.rss()) && lfEx.rss() >= 0.0 && lfEx.rss() < 10000.0,
                       "80e-logy: LinearFit::rss() invalid for excluded negative Y: " << lfEx.rss());
            QwtPlotCurve *lfCurve = gLogY->curve(gLogY->curves() - 1);
            CHECK_COND(lfCurve && lfCurve->dataSize() >= 2, "80e-logy: LinearFit curve missing");
            for (int i = 0; i < lfCurve->dataSize(); ++i) {
                CHECK_COND(lfCurve->x(i) > 0.0 && std::isfinite(lfCurve->y(i)),
                           "80e-logy: LinearFit curve non-positive X or non-finite Y at " << i);
            }

            // P0-3: createResidualsTable must mark excluded row 3 invalid and compute finite
            // non-zero Standardized_Residual & Cooks_Distance on active rows {0,1,2,4}.
            Table *resTbl = lfEx.createResidualsTable();
            CHECK_COND(resTbl && resTbl->numRows() == 5 && resTbl->numCols() >= 7,
                       "80e-logy: createResidualsTable failed");
            for (int col : { 2, 3, 4, 5, 6 }) {
                CHECK_COND(resTbl->column(col)->isInvalid(3),
                           "80e-logy: P0-3! excluded row 3 col " << col << " not marked invalid");
            }
            bool anyNonZeroStdRes = false;
            double manualActiveRss = 0.0;
            for (int r : { 0, 1, 2, 4 }) {
                CHECK_COND(!resTbl->column(2)->isInvalid(r) && !resTbl->column(4)->isInvalid(r)
                                   && !resTbl->column(6)->isInvalid(r),
                           "80e-logy: active row " << r << " unexpectedly marked invalid");
                CHECK_COND(std::isfinite(resTbl->cell(r, 4)) && std::isfinite(resTbl->cell(r, 6)),
                           "80e-logy: active row " << r << " has non-finite residual/Cook metric");
                if (std::fabs(resTbl->cell(r, 4)) > 1e-6)
                    anyNonZeroStdRes = true;
                const double diff = resTbl->cell(r, 1) - resTbl->cell(r, 2);
                manualActiveRss += diff * diff;
            }
            CHECK_COND(anyNonZeroStdRes,
                       "80e-logy: P0-3! Standardized_Residual was identically 0.0 on all active rows");
            CHECK_COND(std::fabs(lfEx.rss() - manualActiveRss) < 1e-6,
                       "80e-logy: rss()=" << lfEx.rss() << " != manualActiveRss=" << manualActiveRss);

            // P0-3: anovaFValue must match active-only TSS/RSS (not polluted by y=-5)
            double activeMean = 0.0;
            for (int r : { 0, 1, 2, 4 })
                activeMean += ysY[r];
            activeMean /= 4.0;
            double activeTss = 0.0;
            for (int r : { 0, 1, 2, 4 })
                activeTss += (ysY[r] - activeMean) * (ysY[r] - activeMean);
            const double expectedF =
                    std::max(0.0, activeTss - lfEx.rss()) / (lfEx.rss() / 2.0);
            const double gotF = lfEx.anovaFValue();
            CHECK_COND(std::isfinite(gotF) && std::fabs(gotF - expectedF) <= 1e-4 * (1.0 + expectedF),
                       "80e-logy: P0-3! anovaFValue mismatch (got " << gotF << " expected "
                                                                     << expectedF << ")");

            // P1-4: Residual Bootstrap (mode == 0) must succeed and preserve rSquare()
            const double r2BeforeBoot = lfEx.rSquare();
            const Fit::BootstrapResult bootRes = lfEx.runBootstrapCI(25, 0.95, 0);
            CHECK_COND(bootRes.valid && bootRes.nSuccess > 0,
                       "80e-logy: P1-4! residual bootstrap failed on apparent log fit with exclusion");
            CHECK_COND(std::fabs(lfEx.rSquare() - r2BeforeBoot) < 1e-12,
                       "80e-logy: runBootstrapCI mutated custom rSquare() from "
                               << r2BeforeBoot << " to " << lfEx.rSquare());
        }

        // Log-X PolynomialFit with unsorted X + non-positive leading/trailing X
        {
            Table *tLogX = w80->newTable(5, 2, "T80LogXUnsort", "LogXUnsort80");
            const double xsX[5] = { -2.0, 10.0, 1.0, 100.0, 0.0 };
            const double ysX[5] = { 5.0, 10.0, 2.0, 100.0, 3.0 };
            for (int r = 0; r < 5; ++r) {
                tLogX->setCell(r, 0, xsX[r]);
                tLogX->setCell(r, 1, ysX[r]);
            }
            MultiLayer *mlLogX =
                    w80->multilayerPlot(tLogX, QStringList() << tLogX->colName(1), Graph::Scatter);
            Graph *gLogX = mlLogX ? mlLogX->activeGraph() : nullptr;
            CHECK_COND(gLogX && gLogX->curve(0), "80e-logx: setup failed");
            gLogX->setScale(QwtPlot::xBottom, 0.5, 200.0, 0.0, 5, 5, 1);
            gLogX->setScale(QwtPlot::yLeft, 0.5, 200.0, 0.0, 5, 5, 0);

            QString unsortCurve = gLogX->curve(0)->title().text();
            PolynomialFit pfUnsort(w80, gLogX, unsortCurve, 1, false);
            pfUnsort.setApparentFit(true);
            pfUnsort.setSilent(true);
            pfUnsort.fit();
            CHECK_COND(std::isfinite(pfUnsort.rss()) && pfUnsort.rss() >= 0.0,
                       "80e-logx: PolynomialFit::rss() not finite: " << pfUnsort.rss());
            CHECK_COND(std::isfinite(pfUnsort.anovaFValue()),
                       "80e-logx: PolynomialFit::anovaFValue() not finite");
            const Fit::BootstrapResult bootPoly = pfUnsort.runBootstrapCI(20, 0.95, 0);
            CHECK_COND(bootPoly.valid && bootPoly.nSuccess > 0,
                       "80e-logx: PolynomialFit residual bootstrap failed");
            QwtPlotCurve *fitC = gLogX->curve(gLogX->curves() - 1);
            CHECK_COND(fitC != nullptr && fitC->dataSize() >= 2, "80e-logx: fit curve empty");
            double minCx = std::numeric_limits<double>::infinity();
            double maxCx = -std::numeric_limits<double>::infinity();
            for (int i = 0; i < fitC->dataSize(); ++i) {
                CHECK_COND(fitC->x(i) > 0.0 && std::isfinite(fitC->y(i)),
                           "80e-logx: PolynomialFit curve non-positive X at " << i);
                minCx = std::min(minCx, fitC->x(i));
                maxCx = std::max(maxCx, fitC->x(i));
            }
            CHECK_COND(minCx >= 0.99 && maxCx <= 100.01 && maxCx > 50.0,
                       "80e-logx: curve X range [" << minCx << ", " << maxCx
                                                   << "] did not match positive data [1, 100]");
        }
    }

    // -------------------------------------------------------------------------
    // 80f: TORIGRAPH_SETTINGS_INI isolation, Ctrl+A scope, Graph3D QPointer, i18n
    // -------------------------------------------------------------------------
    {
        // TORIGRAPH_SETTINGS_INI isolates CustomColorPalettes
        const QString customIniPath = tmp80.filePath(QStringLiteral("custom_palettes_80.ini"));
        {
            EnvVarGuard80 iniGuard("TORIGRAPH_SETTINGS_INI", QFile::encodeName(customIniPath));
            CHECK_COND(Graph::saveCustomPalette(QStringLiteral("Suite80Pal"),
                                                QList<QColor>{ QColor(Qt::red), QColor(Qt::blue) }),
                       "80f: saveCustomPalette failed");
            ApplicationWindow::getSettings().sync();
            QFile iniFile(customIniPath);
            CHECK_COND(iniFile.exists() && iniFile.open(QIODevice::ReadOnly),
                       "80f: TORIGRAPH_SETTINGS_INI file not written at "
                               << customIniPath.toStdString());
            const QString iniText = QString::fromUtf8(iniFile.readAll());
            CHECK_COND(iniText.contains(QStringLiteral("Suite80Pal")),
                       "80f: CustomColorPalettes did not isolate into TORIGRAPH_SETTINGS_INI");
            Graph::removeCustomPalette(QStringLiteral("Suite80Pal"));
        }

        // TableView Ctrl+A uses Qt::WidgetWithChildrenShortcut even with multiple tables open
        Table *tSc1 = w80->newTable(2, 2, "T80Sc1", "Sc1");
        Table *tSc2 = w80->newTable(2, 2, "T80Sc2", "Sc2");
        CHECK_COND(tSc1 != nullptr && tSc2 != nullptr, "80f: shortcut test tables null");
        auto findCtrlAShortcuts = [](Table *t) {
            QList<QShortcut *> matched;
            for (QShortcut *sc : t->findChildren<QShortcut *>()) {
                if (sc && sc->key() == QKeySequence(QStringLiteral("Ctrl+A")))
                    matched.append(sc);
            }
            return matched;
        };
        const QList<QShortcut *> scList1 = findCtrlAShortcuts(tSc1);
        const QList<QShortcut *> scList2 = findCtrlAShortcuts(tSc2);
        CHECK_COND(!scList1.isEmpty() && !scList2.isEmpty(),
                   "80f: missing TableView Ctrl+A QShortcut");
        for (QShortcut *sc : scList1 + scList2) {
            CHECK_COND(sc->context() == Qt::WidgetWithChildrenShortcut,
                       "80f: TableView Ctrl+A QShortcut context is not WidgetWithChildrenShortcut");
        }

        // Graph3D QPointer<Table> nulls out safely when source Table closes
        Table *t3d = w80->newTable(4, 3, "T80G3d", "G3d");
        t3d->setColPlotDesignation(0, SciDAVis::X);
        t3d->setColPlotDesignation(1, SciDAVis::Y);
        t3d->setColPlotDesignation(2, SciDAVis::Z);
        for (int r = 0; r < 4; ++r) {
            t3d->setCell(r, 0, double(r));
            t3d->setCell(r, 1, double(r * 2));
            t3d->setCell(r, 2, double(r * 3));
        }
        Graph3D *g3d = w80->dataPlotXYZ(t3d, t3d->colName(2), Graph3D::Scatter);
        CHECK_COND(g3d != nullptr && g3d->getTable() == t3d,
                   "80f: Graph3D source Table not attached");
        t3d->askOnCloseEvent(false);
        t3d->close();
        pumpEventsMs80(20);
        CHECK_COND(g3d->getTable() == nullptr,
                   "80f: Graph3D::getTable() dangled non-null after Table close");
        g3d->updateScales(0.0, 5.0, 0.0, 10.0, 0.0, 15.0);

        // I18n canonical Export PDF + exact Paste Colors & Symbols Only + EN quickGuideUrl
        const QString jaExportPdf =
                ToriGraphFullJapaneseTranslator::lookupJapanese(QStringLiteral("Export PDF"));
        CHECK_COND(jaExportPdf
                           == QString::fromUtf8("PDF\xE3\x82\xA8\xE3\x82\xAF\xE3\x82\xB9"
                                                "\xE3\x83\x9D\xE3\x83\xBC\xE3\x83\x88"),
                   "80f: Export PDF JA translation mismatch");
        const QString jaPasteStyle = ToriGraphFullJapaneseTranslator::lookupJapanese(
                QStringLiteral("Paste Colors & Symbols Only"));
        CHECK_COND(jaPasteStyle
                           == QString::fromUtf8(
                                   "\xE8\x89\xB2\xE3\x83\xBB\xE3\x82\xB7\xE3\x83\xB3\xE3\x83\x9C"
                                   "\xE3\x83\xAB\xE3\x81\xAE\xE3\x81\xBF\xE8\xB2\xBC\xE3\x82\x8A"
                                   "\xE4\xBB\x98\xE3\x81\x91"),
                   "80f: Paste Colors & Symbols Only exact JA mismatch (got "
                           << jaPasteStyle.toUtf8().constData() << ")");

        // Edit menu + Edit -> Paste Format Special submenu Alt mnemonic uniqueness in EN & JA
        auto checkMenuNoDupMnemonics = [](const QMenu *menu, const QString &label) {
            if (!menu)
                return std::make_pair(false, QStringLiteral("null menu: ") + label);
            QMap<QChar, QString> seen;
            static const QRegularExpression reEn(QStringLiteral("(?:^|[^&])&([A-Za-z])"));
            static const QRegularExpression reJa(QStringLiteral("\\(&([A-Za-z])\\)"));
            for (const QAction *act : menu->actions()) {
                if (!act || act->isSeparator())
                    continue;
                const QString txt = act->text();
                QStringList letters;
                auto itJa = reJa.globalMatch(txt);
                while (itJa.hasNext())
                    letters << itJa.next().captured(1).toUpper();
                if (letters.isEmpty()) {
                    auto itEn = reEn.globalMatch(txt);
                    while (itEn.hasNext())
                        letters << itEn.next().captured(1).toUpper();
                }
                for (const QString &l : letters) {
                    const QChar ch = l.at(0);
                    if (seen.contains(ch)) {
                        return std::make_pair(
                                false,
                                QStringLiteral("%1: duplicate Alt+%2 between '%3' and '%4'")
                                        .arg(label, ch, seen.value(ch), txt));
                    }
                    seen.insert(ch, txt);
                }
            }
            return std::make_pair(true, QString());
        };
        for (const QString &lang : QStringList{ QStringLiteral("en"), QStringLiteral("ja") }) {
            w80->switchToLanguage(lang);
            QCoreApplication::processEvents();
            QMenu *editMenu = nullptr;
            if (w80->menuBar()) {
                for (QAction *topAct : w80->menuBar()->actions()) {
                    if (topAct && topAct->menu()
                        && (topAct->text().contains(QLatin1String("Edit"), Qt::CaseInsensitive)
                            || topAct->text().contains(QStringLiteral("\xE7\xB7\xA8\xE9\x9B\x86")))) {
                        editMenu = topAct->menu();
                        break;
                    }
                }
            }
            CHECK_COND(editMenu != nullptr, "80f: could not locate Edit menu for lang=" << lang.toStdString());
            const auto editRes = checkMenuNoDupMnemonics(editMenu, QStringLiteral("Edit/") + lang);
            CHECK_COND(editRes.first, "80f-mnem: " << editRes.second.toUtf8().constData());
            QMenu *pasteSpecial = nullptr;
            for (QAction *ea : editMenu->actions()) {
                if (ea && ea->menu()) {
                    pasteSpecial = ea->menu();
                    break;
                }
            }
            CHECK_COND(pasteSpecial != nullptr,
                       "80f: could not locate Paste Format Special submenu for lang=" << lang.toStdString());
            const auto subRes =
                    checkMenuNoDupMnemonics(pasteSpecial, QStringLiteral("PasteSpecial/") + lang);
            CHECK_COND(subRes.first, "80f-mnem: " << subRes.second.toUtf8().constData());
        }

        // GraphDigitizer calibration labels must have buddies so '&&' renders as single '&'
        w80->switchToLanguage(QStringLiteral("en"));
        QCoreApplication::processEvents();
        {
            int anchorChecked = 0;
            bool buddyOk = true;
            QTimer probeTimer(w80);
            probeTimer.setInterval(20);
            int ticks = 0;
            QObject::connect(&probeTimer, &QTimer::timeout, w80, [&]() {
                ++ticks;
                if (auto *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                    const QList<QLabel *> lbls = dlg->findChildren<QLabel *>();
                    for (QLabel *lbl : lbls) {
                        if (lbl && lbl->text().contains(QLatin1String("Anchor"))) {
                            ++anchorChecked;
                            if (!lbl->buddy() || !lbl->text().contains(QLatin1String("&&")))
                                buddyOk = false;
                        }
                    }
                    probeTimer.stop();
                    dlg->reject();
                } else if (ticks > 60) {
                    probeTimer.stop();
                }
            });
            probeTimer.start();
            GraphDigitizer::showDigitizerDialog(w80);
            probeTimer.stop();
            CHECK_COND(anchorChecked == 4 && buddyOk,
                       "80f-dig: GraphDigitizer calibration Anchor labels missing buddies (checked="
                               << anchorChecked << " buddyOk=" << buddyOk << ")");
        }

        // Parse CPYUA skipped dataset resync regression (ImportOPJU D4-8 + cand.skipped)
        {
            const QByteArray cpyuaBytes = buildSyntheticCpyuaSkippedBuffer80();
            const QString cpyuaPath = tmp80.filePath(QStringLiteral("skipped_resync_80.opju"));
            {
                QFile cf(cpyuaPath);
                CHECK_COND(cf.open(QIODevice::WriteOnly), "80f-opju: write failed");
                cf.write(cpyuaBytes);
            }
            CHECK_COND(ImportOPJU::isCpyuaOpjuFile(cpyuaPath),
                       "80f-opju: synthetic buffer not recognized as CPYUA");
            AppWinGuard80 wOpju(new ApplicationWindow());
            wOpju->applyUserSettings();
            wOpju->setHeadlessMode(true);
            QString opjuErr;
            ImportOPJU::ImportStats opjuStats;
            CHECK_COND(ImportOPJU::importFile(wOpju.get(), cpyuaPath, &opjuErr, &opjuStats),
                       "80f-opju: importFile failed: " << opjuErr.toStdString());
            Table *impTbl = wOpju->table(QStringLiteral("Book80"));
            CHECK_COND(impTbl != nullptr && impTbl->numCols() == 2 && impTbl->numRows() == 3,
                       "80f-opju: Book80 table missing or wrong dimensions");
            CHECK_COND(impTbl->column(0)->name() == QStringLiteral("A")
                               && impTbl->column(0)->plotDesignation() == SciDAVis::Y,
                       "80f-opju: Book80_A did not get Y designation via cand.skipped resync!");
            CHECK_COND(impTbl->column(1)->name() == QStringLiteral("B")
                               && impTbl->column(1)->plotDesignation() == SciDAVis::X,
                       "80f-opju: skipped Dataset 2 broke X designation resync onto Dataset 3 'B'!");
            CHECK_COND(std::fabs(impTbl->cell(0, 1) - 10.0) < 1e-6
                               && std::fabs(impTbl->cell(2, 1) - 30.0) < 1e-6,
                       "80f-opju: wrong numeric values decoded after skipped dataset");
        }

        // Visible window tests: QuickStart & Split grab_gui_diagnostic + Inspector Tree Key_Delete
        {
            AppWinGuard80 wDiag(new ApplicationWindow());
            wDiag->applyUserSettings();
            wDiag->confirmCloseTable = false;
            wDiag->confirmClosePlot2D = false;
            wDiag->confirmClosePlot3D = false;
            wDiag->confirmCloseFolder = false;
            wDiag->resize(1366, 768);
            wDiag->show();
            QCoreApplication::processEvents();
            PythonAutomationServer *srv80 = wDiag->pythonAutomationServer();
            CHECK_COND(srv80 != nullptr, "80f-diag: pythonAutomationServer null");

            // (1) QuickStart + expand_inspector (NO scroll_inspector) -> inspector_clipped_bottom == false
            const QString qsPng = tmp80.filePath(QStringLiteral("qs_80f.png"));
            QJsonObject reqQs;
            reqQs["jsonrpc"] = "2.0";
            reqQs["id"] = 8001;
            reqQs["method"] = "app.grab_gui_diagnostic";
            reqQs["params"] = QJsonObject{
                { "path", qsPng },
                { "width", 1366 },
                { "height", 768 },
                { "expand_inspector", true },
            };
            const QJsonObject resQs = srv80->handleRpcRequest(reqQs).value("result").toObject();
            CHECK_COND(resQs.value("ok").toBool()
                               && !resQs.value("inspector_clipped_bottom").toBool(),
                       "80f-diag: QuickStart expand_inspector misreported inspector_clipped_bottom!");

            // (2) Split mode resize + 45ms settle + GraphInspectorDock d_object_tree Key_Delete
            Table *tDiag = wDiag->newTable(5, 2, "T80Diag", "Diag80");
            for (int r = 0; r < 5; ++r) {
                tDiag->setCell(r, 0, double(r));
                tDiag->setCell(r, 1, double(r * 2 + 1));
            }
            MultiLayer *mlDiag =
                    wDiag->multilayerPlot(tDiag, QStringList() << tDiag->colName(1), Graph::Scatter);
            Graph *gDiag = mlDiag ? mlDiag->activeGraph() : nullptr;
            CHECK_COND(gDiag != nullptr && gDiag->curves() == 1, "80f-diag: mlDiag setup failed");
            CHECK_COND(mlDiag->applyStylePresetById(QStringLiteral("isij_85mm")),
                       "80f-diag: applyStylePresetById(isij_85mm) failed");
            wDiag->resize(1920, 1080);
            QCoreApplication::processEvents();
            if (wDiag->modernRibbonBar())
                wDiag->modernRibbonBar()->setSplitLeftRatio(0.40);
            mlDiag->fitPageZoomToViewport();
            QCoreApplication::processEvents();
            const double zoomBeforeSplitResize = mlDiag->pageZoom();
            const QString splitPng = tmp80.filePath(QStringLiteral("split_80f.png"));
            QJsonObject reqSplit;
            reqSplit["jsonrpc"] = "2.0";
            reqSplit["id"] = 8002;
            reqSplit["method"] = "app.grab_gui_diagnostic";
            reqSplit["params"] = QJsonObject{
                { "path", splitPng },
                { "width", 900 },
                { "height", 520 },
                { "workspace_mode", "split" },
            };
            const QJsonObject resSplit =
                    srv80->handleRpcRequest(reqSplit).value("result").toObject();
            CHECK_COND(resSplit.value("ok").toBool() && QFileInfo(splitPng).size() > 1000,
                       "80f-diag: Split grab_gui_diagnostic failed");
            const double expectedSplitFitZoom = qBound(0.35, mlDiag->fitPageZoomFactor(), 1.6);
            // Allow up to ~0.10 variance for ~18 px QScrollArea scrollbar hide hysteresis on the
            // 85 mm (321 px) canvas (within MultiLayer's 24 px autofit viewport deadband), while
            // proving zoom dropped > 0.50 during grab_gui_diagnostic's 45 ms debounce settle.
            CHECK_COND(zoomBeforeSplitResize > 1.15 && expectedSplitFitZoom < 0.95
                               && mlDiag->pageZoom() < 0.90
                               && (zoomBeforeSplitResize - mlDiag->pageZoom()) > 0.50
                               && std::fabs(mlDiag->pageZoom() - expectedSplitFitZoom) < 0.10,
                       "80f-diag: Split resize 45ms settle did not converge pageZoom (before="
                               << zoomBeforeSplitResize << ", after=" << mlDiag->pageZoom()
                               << ", expected=" << expectedSplitFitZoom << ")");

            // GraphInspectorDock object tree Qt::Key_Delete deletes selected curve
            GraphInspectorDock *dock80 = wDiag->graphInspectorDock();
            CHECK_COND(dock80 != nullptr && dock80->objectTree() != nullptr,
                       "80f-insp: dock/objectTree null");
            dock80->show();
            dock80->setCollapsedRail(false);
            dock80->setTargetMultiLayer(mlDiag, true);
            QCoreApplication::processEvents();

            // Round 7 + Round 8 P1: with a Curve actively selected (would otherwise snap to
            // PageTabCurve), resizing/auto-fitting page zoom emits pageZoomChanged (syncing
            // StatusBarPageZoomBtn) WITHOUT emitting spurious modifiedPlot(), AND explicit
            // modifiedPlot() coalesced refresh preserves the user's chosen PageTabPageLayer.
            gDiag->setCurrentSelection({ Graph::GraphSelection::Curve, 0, -1 });
            QCoreApplication::processEvents();
            CHECK_COND(gDiag->currentSelection().kind == Graph::GraphSelection::Curve
                               && dock80->propertyStack()
                               && dock80->propertyStack()->currentIndex()
                                          == GraphInspectorDock::PageTabCurve,
                       "80f-page-tab-preserve: failed initial Curve selection setup");

            dock80->switchToInspectorPage(GraphInspectorDock::PageTabPageLayer);
            QCoreApplication::processEvents();
            CHECK_COND(dock80->propertyStack()->currentIndex()
                               == GraphInspectorDock::PageTabPageLayer,
                       "80f-page-tab-preserve: failed to switch to PageTabPageLayer");

            int modCount = 0;
            int zoomSigCount = 0;
            QObject::connect(mlDiag, &MultiLayer::modifiedPlot, mlDiag, [&]() { ++modCount; });
            QObject::connect(mlDiag, &MultiLayer::pageZoomChanged, mlDiag,
                             [&](double) { ++zoomSigCount; });

            wDiag->resize(1366, 768);
            pumpEventsMs80(90);

            QToolButton *statusZoomBtn =
                    wDiag->findChild<QToolButton *>(QStringLiteral("StatusBarPageZoomBtn"));
            const QString expectedZoomText =
                    QStringLiteral("%1%").arg(qMax(10, qRound(mlDiag->pageZoom() * 100.0)));
            CHECK_COND(zoomSigCount >= 1 && modCount == 0,
                       "80f-page-tab-preserve: expected pageZoomChanged>=1 & modCount==0 on resize (zoomSig="
                               << zoomSigCount << ", modCount=" << modCount << ")");
            CHECK_COND(statusZoomBtn != nullptr && statusZoomBtn->text() == expectedZoomText,
                       "80f-page-tab-preserve: StatusBarPageZoomBtn out of sync with pageZoom (btn='"
                               << (statusZoomBtn ? statusZoomBtn->text().toStdString() : "<null>")
                               << "', expected='" << expectedZoomText.toStdString() << "')");
            CHECK_COND(dock80->propertyStack()->currentIndex()
                               == GraphInspectorDock::PageTabPageLayer,
                       "80f-page-tab-preserve: P1 regression! resize snapped Inspector away from Page tab to index "
                               << dock80->propertyStack()->currentIndex());

            QMetaObject::invokeMethod(mlDiag, "modifiedPlot", Qt::DirectConnection);
            pumpEventsMs80(60);
            CHECK_COND(modCount >= 1
                               && dock80->propertyStack()->currentIndex()
                                          == GraphInspectorDock::PageTabPageLayer,
                       "80f-page-tab-preserve: modifiedPlot coalesced refresh snapped away from Page tab (index="
                               << dock80->propertyStack()->currentIndex() << ")");

            // Round 9: language switch (changeEvent -> LanguageChange -> refreshAll(false)),
            // same-target setTargetMultiLayer, and collapsed-rail Page click must preserve Page tab
            wDiag->switchToLanguage(QStringLiteral("ja"));
            QCoreApplication::processEvents();
            CHECK_COND(dock80->propertyStack()->currentIndex()
                               == GraphInspectorDock::PageTabPageLayer,
                       "80f-page-tab-preserve: switchToLanguage(ja) snapped Inspector away from Page tab (index="
                               << dock80->propertyStack()->currentIndex() << ")");
            wDiag->switchToLanguage(QStringLiteral("en"));
            QCoreApplication::processEvents();
            CHECK_COND(dock80->propertyStack()->currentIndex()
                               == GraphInspectorDock::PageTabPageLayer,
                       "80f-page-tab-preserve: switchToLanguage(en) snapped Inspector away from Page tab (index="
                               << dock80->propertyStack()->currentIndex() << ")");

            dock80->setTargetMultiLayer(mlDiag, false);
            QCoreApplication::processEvents();
            CHECK_COND(dock80->propertyStack()->currentIndex()
                               == GraphInspectorDock::PageTabPageLayer,
                       "80f-page-tab-preserve: setTargetMultiLayer(mlDiag, false) snapped away from Page tab");

            dock80->setTargetMultiLayer(mlDiag, true);
            QCoreApplication::processEvents();
            CHECK_COND(dock80->propertyStack()->currentIndex()
                               == GraphInspectorDock::PageTabPageLayer,
                       "80f-page-tab-preserve: setTargetMultiLayer(mlDiag, true) snapped away from Page tab");

            dock80->switchToInspectorPage(GraphInspectorDock::PageTabCurve);
            dock80->setCollapsedRail(true);
            QCoreApplication::processEvents();
            QToolButton *railPageBtn =
                    dock80->findChild<QToolButton *>(QStringLiteral("InspectorRailPageBtn"));
            CHECK_COND(railPageBtn != nullptr,
                       "80f-page-tab-preserve: InspectorRailPageBtn not found");
            railPageBtn->click();
            QCoreApplication::processEvents();
            CHECK_COND(!dock80->isCollapsedRail()
                               && dock80->propertyStack()->currentIndex()
                                          == GraphInspectorDock::PageTabPageLayer,
                       "80f-page-tab-preserve: collapsed-rail Page click snapped away (collapsed="
                               << dock80->isCollapsedRail() << ", index="
                               << dock80->propertyStack()->currentIndex() << ")");

            // Positive control: explicit syncInspectorFromSelection(true) still switches to Curve
            dock80->syncInspectorFromSelection(true);
            QCoreApplication::processEvents();
            CHECK_COND(dock80->propertyStack()->currentIndex() == GraphInspectorDock::PageTabCurve,
                       "80f-page-tab-preserve: positive control syncInspectorFromSelection(true) did not switch to Curve");

            QTreeWidget *tree = dock80->objectTree();
            QTreeWidgetItem *curveItem = nullptr;
            const int curveKind = static_cast<int>(Graph::GraphSelection::Curve);
            for (QTreeWidgetItemIterator it(tree); *it; ++it) {
                QTreeWidgetItem *item = *it;
                const int itemKind = item->data(0, Qt::UserRole + 1).toInt();
                const int itemPid = item->data(0, Qt::UserRole + 2).toInt();
                if (item->childCount() == 0 && itemKind == curveKind && itemPid == 0) {
                    curveItem = item;
                    break;
                }
                if (!curveItem && item->childCount() == 0
                    && item->text(0).contains(gDiag->curve(0)->title().text())) {
                    curveItem = item;
                }
            }
            if (!curveItem) {
                for (QTreeWidgetItemIterator it(tree); *it; ++it) {
                    if ((*it)->childCount() == 0
                        && (*it)->text(0).contains(QLatin1String("Diag80"))) {
                        curveItem = *it;
                        break;
                    }
                }
            }
            CHECK_COND(curveItem != nullptr, "80f-insp: curve tree item not found");
            tree->setCurrentItem(curveItem);
            tree->setFocus(Qt::ShortcutFocusReason);
            QCoreApplication::processEvents();
            QKeyEvent delEv(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
            QCoreApplication::sendEvent(tree, &delEv);
            QCoreApplication::processEvents();
            CHECK_COND(gDiag->curves() == 0,
                       "80f-insp: Key_Delete on GraphInspectorDock objectTree did not delete curve (curves="
                               << gDiag->curves() << ")");

            // Round 7 P1 ASan regression: deleting the target curve mid-modal pickColorInteractive
            // QMenu::exec() must cleanly bail via QPointer/curveKey re-lookup with zero UAF.
            {
                Table *tCol = wDiag->newTable(4, 2, "T80ColUaf", "ColUaf80");
                for (int r = 0; r < 4; ++r) {
                    tCol->setCell(r, 0, double(r));
                    tCol->setCell(r, 1, double(r * 3 + 2));
                }
                MultiLayer *mlColTest = wDiag->multilayerPlot(
                        tCol, QStringList() << tCol->colName(1), Graph::Scatter);
                Graph *gColTest = mlColTest ? mlColTest->activeGraph() : nullptr;
                CHECK_COND(gColTest && gColTest->curves() == 1 && gColTest->curve(0),
                           "80f-insp-color-uaf: mlColTest setup failed");
                const QString initHex =
                        gColTest->curve(0)->pen().color().name(QColor::HexRgb).toUpper();
                dock80->setTargetMultiLayer(mlColTest, true);
                dock80->switchToInspectorPage(GraphInspectorDock::PageTabCurve);
                QCoreApplication::processEvents();

                QPushButton *btnCol = nullptr;
                for (QPushButton *cand :
                     dock80->findChildren<QPushButton *>(QStringLiteral("InspectorColorSwatchBtn"))) {
                    if (cand && cand->isVisible()) {
                        btnCol = cand;
                        break;
                    }
                }
                CHECK_COND(btnCol != nullptr,
                           "80f-insp-color-uaf: visible InspectorColorSwatchBtn not found");

                struct NonInteractivePropGuard80
                {
                    QVariant prev;
                    explicit NonInteractivePropGuard80(bool on)
                        : prev(qApp->property("torigraph_noninteractive"))
                    {
                        qApp->setProperty("torigraph_noninteractive", on);
                    }
                    ~NonInteractivePropGuard80()
                    {
                        qApp->setProperty("torigraph_noninteractive", prev);
                    }
                } interactiveScope(false);

                bool sawColorPopup = false;
                bool clickedColorSwatch = false;
                QTimer popupTimer;
                popupTimer.setInterval(15);
                QObject::connect(&popupTimer, &QTimer::timeout, [&]() {
                    auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                    if (!popup || sawColorPopup)
                        return;
                    sawColorPopup = true;
                    if (gColTest && gColTest->curves() > 0)
                        gColTest->removeCurve(0);
                    for (QToolButton *sw : popup->findChildren<QToolButton *>()) {
                        const QString tip = sw->toolTip();
                        if (tip.startsWith(QLatin1Char('#')) && tip != initHex) {
                            clickedColorSwatch = true;
                            sw->click();
                            return;
                        }
                    }
                    popup->close();
                });
                popupTimer.start();
                btnCol->click();
                popupTimer.stop();
                QCoreApplication::processEvents();
                CHECK_COND(sawColorPopup && clickedColorSwatch && gColTest->curves() == 0,
                           "80f-insp-color-uaf: modal color popup curve-deletion test failed (sawPopup="
                                   << sawColorPopup << ", clicked=" << clickedColorSwatch
                                   << ", curves=" << gColTest->curves() << ")");

                // Subtest 2 (Round 8): destroying the parent GraphInspectorDock mid-modal
                // pickColorInteractive QMenu::exec() must NOT destroy stack-allocated popup QMenu.
                {
                    Table *tCol2 = wDiag->newTable(4, 2, "T80ColDockDead", "ColDockDead80");
                    for (int r = 0; r < 4; ++r) {
                        tCol2->setCell(r, 0, double(r));
                        tCol2->setCell(r, 1, double(r * 2 + 1));
                    }
                    MultiLayer *mlColDead = wDiag->multilayerPlot(
                            tCol2, QStringList() << tCol2->colName(1), Graph::Scatter);
                    Graph *gColDead = mlColDead ? mlColDead->activeGraph() : nullptr;
                    CHECK_COND(gColDead && gColDead->curve(0),
                               "80f-insp-color-uaf-parent: mlColDead setup failed");

                    QPointer<GraphInspectorDock> tmpDock = new GraphInspectorDock(wDiag);
                    tmpDock->setFloating(true);
                    tmpDock->show();
                    tmpDock->setCollapsedRail(false);
                    tmpDock->setTargetMultiLayer(mlColDead, true);
                    tmpDock->switchToInspectorPage(GraphInspectorDock::PageTabCurve);
                    QCoreApplication::processEvents();

                    QPushButton *btnTmp = nullptr;
                    for (QPushButton *cand : tmpDock->findChildren<QPushButton *>(
                                 QStringLiteral("InspectorColorSwatchBtn"))) {
                        if (cand && cand->isVisible()) {
                            btnTmp = cand;
                            break;
                        }
                    }
                    CHECK_COND(btnTmp != nullptr,
                               "80f-insp-color-uaf-parent: visible InspectorColorSwatchBtn not found");

                    bool sawParentDeadPopup = false;
                    QTimer parentDeadTimer;
                    parentDeadTimer.setInterval(15);
                    QObject::connect(&parentDeadTimer, &QTimer::timeout, [&]() {
                        auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                        if (!popup || sawParentDeadPopup)
                            return;
                        sawParentDeadPopup = true;
                        if (tmpDock)
                            delete tmpDock.data();
                        for (QToolButton *sw : popup->findChildren<QToolButton *>()) {
                            if (sw->toolTip().startsWith(QLatin1Char('#'))) {
                                sw->click();
                                return;
                            }
                        }
                        popup->close();
                    });
                    parentDeadTimer.start();
                    btnTmp->click();
                    parentDeadTimer.stop();
                    QCoreApplication::processEvents();
                    CHECK_COND(sawParentDeadPopup && tmpDock.isNull(),
                               "80f-insp-color-uaf-parent: destroying parent dock mid-QMenu::exec failed (saw="
                                       << sawParentDeadPopup << ", null=" << tmpDock.isNull() << ")");
                }

                // 80f-insp-modal-uaf (Round 9 + 10 ASan): parentless QMenu & QPointer modal
                // destruction checks for onObjectTreeContextMenu, openAddCurvePopover,
                // Graph::editReferenceLineDialog, and d_btn_ref_add_custom.
                {
                    struct HeadlessScope80f
                    {
                        ApplicationWindow *app = nullptr;
                        bool prev = true;
                        explicit HeadlessScope80f(ApplicationWindow *w, bool on)
                            : app(w), prev(w ? w->isHeadlessMode() : true)
                        {
                            if (app)
                                app->setHeadlessMode(on);
                        }
                        ~HeadlessScope80f()
                        {
                            if (app)
                                app->setHeadlessMode(prev);
                        }
                    } nonHeadless80f(wDiag, false);

                    Table *tModal = wDiag->newTable(4, 2, "T80InspModal", "InspModal80");
                    for (int r = 0; r < 4; ++r) {
                        tModal->setCell(r, 0, double(r));
                        tModal->setCell(r, 1, double(r * 2 + 1));
                    }
                    MultiLayer *mlModal = wDiag->multilayerPlot(
                            tModal, QStringList() << tModal->colName(1), Graph::Scatter);
                    Graph *gModal = mlModal ? mlModal->activeGraph() : nullptr;
                    CHECK_COND(gModal && gModal->curve(0),
                               "80f-insp-modal-uaf: mlModal setup failed");

                    // Subtest A: destroy floating GraphInspectorDock during onObjectTreeContextMenu
                    {
                        QPointer<GraphInspectorDock> tmpDockA = new GraphInspectorDock(wDiag);
                        tmpDockA->setFloating(true);
                        tmpDockA->show();
                        tmpDockA->setCollapsedRail(false);
                        tmpDockA->setTargetMultiLayer(mlModal, true);
                        QCoreApplication::processEvents();

                        QTreeWidget *trA = tmpDockA->objectTree();
                        QTreeWidgetItem *cvItemA = nullptr;
                        const int curveKindA = static_cast<int>(Graph::GraphSelection::Curve);
                        if (trA) {
                            for (QTreeWidgetItemIterator it(trA); *it; ++it) {
                                QTreeWidgetItem *cand = *it;
                                if (cand->childCount() == 0
                                    && cand->data(0, Qt::UserRole + 1).toInt() == curveKindA
                                    && cand->data(0, Qt::UserRole + 2).toInt() == 0) {
                                    cvItemA = cand;
                                    break;
                                }
                            }
                        }
                        CHECK_COND(trA != nullptr && cvItemA != nullptr,
                                   "80f-insp-modal-uaf-A: curve item not found");
                        trA->expandAll();
                        trA->scrollToItem(cvItemA);
                        QCoreApplication::processEvents();
                        const QPoint posA = trA->visualItemRect(cvItemA).center();

                        bool sawTreePopupA = false;
                        QTimer timerA;
                        timerA.setInterval(15);
                        QObject::connect(&timerA, &QTimer::timeout, [&]() {
                            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                            if (!popup || sawTreePopupA)
                                return;
                            sawTreePopupA = true;
                            if (tmpDockA)
                                delete tmpDockA.data();
                            popup->close();
                        });
                        timerA.start();
                        QMetaObject::invokeMethod(trA, "customContextMenuRequested",
                                                  Qt::DirectConnection, Q_ARG(QPoint, posA));
                        timerA.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawTreePopupA && tmpDockA.isNull(),
                                   "80f-insp-modal-uaf-A: parentless objectTree QMenu teardown failed (saw="
                                           << sawTreePopupA << ", null=" << tmpDockA.isNull()
                                           << ")");
                    }

                    // Subtest B: destroy floating dock during openAddCurvePopover and click btnAddNow
                    {
                        QPointer<GraphInspectorDock> tmpDockB = new GraphInspectorDock(wDiag);
                        tmpDockB->setFloating(true);
                        tmpDockB->show();
                        tmpDockB->setCollapsedRail(false);
                        tmpDockB->setTargetMultiLayer(mlModal, true);
                        QCoreApplication::processEvents();

                        QToolButton *btnAddB = nullptr;
                        for (QToolButton *tb : tmpDockB->findChildren<QToolButton *>()) {
                            if (tb && tb->isVisible() && tb->text().contains(QStringLiteral("+ Add"))) {
                                btnAddB = tb;
                                break;
                            }
                        }
                        CHECK_COND(btnAddB != nullptr,
                                   "80f-insp-modal-uaf-B: '+ Add' tool button not found");

                        bool sawAddPopupB = false;
                        bool clickedAddNowB = false;
                        QTimer timerB;
                        timerB.setInterval(15);
                        QObject::connect(&timerB, &QTimer::timeout, [&]() {
                            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                            if (!popup || sawAddPopupB)
                                return;
                            sawAddPopupB = true;
                            if (tmpDockB)
                                delete tmpDockB.data();
                            for (QPushButton *pb : popup->findChildren<QPushButton *>()) {
                                if (pb->text().contains(QStringLiteral("Add Selected Curve"))) {
                                    clickedAddNowB = true;
                                    pb->click();
                                    return;
                                }
                            }
                            popup->close();
                        });
                        timerB.start();
                        btnAddB->click();
                        timerB.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawAddPopupB && clickedAddNowB && tmpDockB.isNull(),
                                   "80f-insp-modal-uaf-B: openAddCurvePopover self-destruction guard failed (saw="
                                           << sawAddPopupB << ", clk=" << clickedAddNowB
                                           << ", null=" << tmpDockB.isNull() << ")");
                    }

                    // Subtest C: destroy owning MultiLayer/Graph mid-modal Graph::editReferenceLineDialog
                    {
                        QPointer<MultiLayer> mlRefDead = wDiag->multilayerPlot(
                                tModal, QStringList() << tModal->colName(1), Graph::Scatter);
                        QPointer<Graph> gRefDead = mlRefDead ? mlRefDead->activeGraph() : nullptr;
                        CHECK_COND(mlRefDead && gRefDead,
                                   "80f-insp-modal-uaf-C: mlRefDead setup failed");
                        mlRefDead->askOnCloseEvent(false);
                        Graph::ReferenceLineSpec refSpec;
                        refSpec.orientation = 0;
                        refSpec.value = 3.0;
                        gRefDead->addReferenceLine(refSpec);

                        bool sawRefDlgC = false;
                        QTimer timerC;
                        timerC.setInterval(15);
                        QObject::connect(&timerC, &QTimer::timeout, [&]() {
                            QDialog *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                            if (!dlg || sawRefDlgC)
                                return;
                            sawRefDlgC = true;
                            if (mlRefDead) {
                                mlRefDead->close();
                                if (mlRefDead)
                                    delete mlRefDead.data();
                            }
                            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                            dlg->accept();
                        });
                        timerC.start();
                        const bool editResC = gRefDead->editReferenceLineDialog(0);
                        timerC.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawRefDlgC && !editResC && gRefDead.isNull(),
                                   "80f-insp-modal-uaf-C: Graph::editReferenceLineDialog mid-modal destruction failed (saw="
                                           << sawRefDlgC << ", res=" << editResC
                                           << ", null=" << gRefDead.isNull() << ")");
                    }

                    // Subtest D: destroy floating dock during d_btn_ref_add_custom -> addReferenceLineInteractive
                    {
                        QPointer<GraphInspectorDock> tmpDockD = new GraphInspectorDock(wDiag);
                        tmpDockD->setFloating(true);
                        tmpDockD->show();
                        tmpDockD->setCollapsedRail(false);
                        tmpDockD->setTargetMultiLayer(mlModal, true);
                        tmpDockD->switchToInspectorPage(GraphInspectorDock::PageTabAxis);
                        QCoreApplication::processEvents();

                        QPushButton *btnRefD = nullptr;
                        for (QPushButton *pb : tmpDockD->findChildren<QPushButton *>()) {
                            if (pb && pb->isVisible() && pb->text().contains(QStringLiteral("+ Add..."))) {
                                btnRefD = pb;
                                break;
                            }
                        }
                        CHECK_COND(btnRefD != nullptr,
                                   "80f-insp-modal-uaf-D: '+ Add...' reference line button not found");

                        bool sawAddRefDlgD = false;
                        QTimer timerD;
                        timerD.setInterval(15);
                        QObject::connect(&timerD, &QTimer::timeout, [&]() {
                            QDialog *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                            if (!dlg || sawAddRefDlgD)
                                return;
                            sawAddRefDlgD = true;
                            if (tmpDockD)
                                delete tmpDockD.data();
                            dlg->reject();
                        });
                        timerD.start();
                        btnRefD->click();
                        timerD.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawAddRefDlgD && tmpDockD.isNull(),
                                   "80f-insp-modal-uaf-D: d_btn_ref_add_custom self guard failed (saw="
                                           << sawAddRefDlgD << ", null=" << tmpDockD.isNull()
                                           << ")");
                    }

                    // Subtest E (Round 11 ASan): pickColorInteractive -> Custom Color...
                    // QColorDialog must NOT be parented to GraphInspectorDock.
                    {
                        QPointer<GraphInspectorDock> tmpDockE = new GraphInspectorDock(wDiag);
                        tmpDockE->setFloating(true);
                        tmpDockE->show();
                        tmpDockE->setCollapsedRail(false);
                        tmpDockE->setTargetMultiLayer(mlModal, true);
                        tmpDockE->switchToInspectorPage(GraphInspectorDock::PageTabCurve);
                        QCoreApplication::processEvents();

                        QPushButton *btnSwatchE = nullptr;
                        for (QPushButton *cand : tmpDockE->findChildren<QPushButton *>(
                                     QStringLiteral("InspectorColorSwatchBtn"))) {
                            if (cand && cand->isVisible()) {
                                btnSwatchE = cand;
                                break;
                            }
                        }
                        CHECK_COND(btnSwatchE != nullptr,
                                   "80f-insp-modal-uaf-E: visible InspectorColorSwatchBtn not found");

                        int ticksE = 0;
                        bool sawPopupE = false;
                        bool sawColorDlgE = false;
                        QTimer timerE;
                        timerE.setInterval(15);
                        QObject::connect(&timerE, &QTimer::timeout, [&]() {
                            ++ticksE;
                            if (ticksE > 80) {
                                timerE.stop();
                                if (auto *p = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget()))
                                    p->close();
                                if (auto *w = QApplication::activeModalWidget())
                                    w->close();
                                return;
                            }
                            if (!sawPopupE) {
                                if (auto *popup = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget())) {
                                    sawPopupE = true;
                                    for (QPushButton *pb : popup->findChildren<QPushButton *>()) {
                                        if (pb && pb->isVisible()) {
                                            pb->click();
                                            return;
                                        }
                                    }
                                    popup->close();
                                    return;
                                }
                            } else if (!sawColorDlgE) {
                                if (auto *cdlg = qobject_cast<QColorDialog *>(
                                            QApplication::activeModalWidget())) {
                                    sawColorDlgE = true;
                                    if (tmpDockE)
                                        delete tmpDockE.data();
                                    cdlg->reject();
                                }
                            }
                        });
                        timerE.start();
                        btnSwatchE->click();
                        timerE.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawPopupE && sawColorDlgE && tmpDockE.isNull(),
                                   "80f-insp-modal-uaf-E: Custom Color QColorDialog dock teardown failed (popup="
                                           << sawPopupE << ", dlg=" << sawColorDlgE
                                           << ", null=" << tmpDockE.isNull() << ")");
                    }

                    // Subtest F (Round 11 ASan): RichTextBarEntry::menuGreek InstantPopup must
                    // survive deleting the owning floating GraphInspectorDock mid-QMenu::exec().
                    {
                        QPointer<GraphInspectorDock> tmpDockF = new GraphInspectorDock(wDiag);
                        tmpDockF->setFloating(true);
                        tmpDockF->show();
                        tmpDockF->setCollapsedRail(false);
                        tmpDockF->setTargetMultiLayer(mlModal, true);
                        tmpDockF->switchToInspectorPage(GraphInspectorDock::PageTabAxis);
                        QCoreApplication::processEvents();

                        QToolButton *btnGreekF = nullptr;
                        for (QToolButton *tb : tmpDockF->findChildren<QToolButton *>()) {
                            if (tb && tb->isVisible()
                                && tb->popupMode() == QToolButton::InstantPopup
                                && tb->menu() != nullptr
                                && tb->property("_tg_tip_fmt").toString()
                                        == QLatin1String("Insert Greek Symbol")) {
                                btnGreekF = tb;
                                break;
                            }
                        }
                        CHECK_COND(btnGreekF != nullptr,
                                   "80f-insp-modal-uaf-F: visible Greek InstantPopup button not found");

                        int ticksF = 0;
                        bool sawGreekPopupF = false;
                        QTimer timerF;
                        timerF.setInterval(15);
                        QObject::connect(&timerF, &QTimer::timeout, [&]() {
                            ++ticksF;
                            if (ticksF > 80) {
                                timerF.stop();
                                if (auto *p = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget()))
                                    p->close();
                                return;
                            }
                            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                            if (!popup || sawGreekPopupF)
                                return;
                            sawGreekPopupF = true;
                            if (tmpDockF)
                                delete tmpDockF.data();
                            if (auto *ap = QApplication::activePopupWidget())
                                ap->close();
                        });
                        timerF.start();
                        btnGreekF->showMenu();
                        timerF.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawGreekPopupF && tmpDockF.isNull(),
                                   "80f-insp-modal-uaf-F: RichTextBar menuGreek teardown failed (saw="
                                           << sawGreekPopupF << ", null=" << tmpDockF.isNull()
                                           << ")");
                    }

                    // Subtest G (Round 11 ASan): Graph::showAxisContextMenu and
                    // Graph::showTitleContextMenu must use parentless QMenu across .exec().
                    {
                        QPointer<MultiLayer> mlCtxG = wDiag->multilayerPlot(
                                tModal, QStringList() << tModal->colName(1), Graph::Scatter);
                        QPointer<Graph> gCtxG = mlCtxG ? mlCtxG->activeGraph() : nullptr;
                        CHECK_COND(mlCtxG && gCtxG, "80f-insp-modal-uaf-G1: setup failed");
                        mlCtxG->askOnCloseEvent(false);

                        int ticksG1 = 0;
                        bool sawCtxPopupG1 = false;
                        QTimer timerG1;
                        timerG1.setInterval(15);
                        QObject::connect(&timerG1, &QTimer::timeout, [&]() {
                            ++ticksG1;
                            if (ticksG1 > 80) {
                                timerG1.stop();
                                if (auto *p = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget()))
                                    p->close();
                                return;
                            }
                            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                            if (!popup || sawCtxPopupG1)
                                return;
                            sawCtxPopupG1 = true;
                            if (mlCtxG) {
                                mlCtxG->close();
                                if (mlCtxG)
                                    delete mlCtxG.data();
                            }
                            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                            popup->close();
                        });
                        timerG1.start();
                        gCtxG->showAxisContextMenu(QwtScaleDraw::BottomScale);
                        timerG1.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawCtxPopupG1 && mlCtxG.isNull() && gCtxG.isNull(),
                                   "80f-insp-modal-uaf-G1: showAxisContextMenu parentless teardown failed (saw="
                                           << sawCtxPopupG1 << ", mlNull=" << mlCtxG.isNull()
                                           << ", gNull=" << gCtxG.isNull() << ")");

                        QPointer<MultiLayer> mlTitleG = wDiag->multilayerPlot(
                                tModal, QStringList() << tModal->colName(1), Graph::Scatter);
                        QPointer<Graph> gTitleG = mlTitleG ? mlTitleG->activeGraph() : nullptr;
                        CHECK_COND(mlTitleG && gTitleG, "80f-insp-modal-uaf-G2: setup failed");
                        mlTitleG->askOnCloseEvent(false);

                        int ticksG2 = 0;
                        bool sawTitlePopupG2 = false;
                        QTimer timerG2;
                        timerG2.setInterval(15);
                        QObject::connect(&timerG2, &QTimer::timeout, [&]() {
                            ++ticksG2;
                            if (ticksG2 > 80) {
                                timerG2.stop();
                                if (auto *p = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget()))
                                    p->close();
                                return;
                            }
                            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                            if (!popup || sawTitlePopupG2)
                                return;
                            sawTitlePopupG2 = true;
                            if (mlTitleG) {
                                mlTitleG->close();
                                if (mlTitleG)
                                    delete mlTitleG.data();
                            }
                            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                            popup->close();
                        });
                        timerG2.start();
                        gTitleG->showTitleContextMenu();
                        timerG2.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawTitlePopupG2 && mlTitleG.isNull() && gTitleG.isNull(),
                                   "80f-insp-modal-uaf-G2: showTitleContextMenu parentless teardown failed (saw="
                                           << sawTitlePopupG2 << ", mlNull=" << mlTitleG.isNull()
                                           << ", gNull=" << gTitleG.isNull() << ")");

                        // Subtest G3 (Round 12 ASan): Graph::showAxisTitleMenu parentless QMenu
                        QPointer<MultiLayer> mlAxisTitleG = wDiag->multilayerPlot(
                                tModal, QStringList() << tModal->colName(1), Graph::Scatter);
                        QPointer<Graph> gAxisTitleG =
                                mlAxisTitleG ? mlAxisTitleG->activeGraph() : nullptr;
                        CHECK_COND(mlAxisTitleG && gAxisTitleG, "80f-insp-modal-uaf-G3: setup failed");
                        mlAxisTitleG->askOnCloseEvent(false);

                        int ticksG3 = 0;
                        bool sawAxisTitlePopupG3 = false;
                        QTimer timerG3;
                        timerG3.setInterval(15);
                        QObject::connect(&timerG3, &QTimer::timeout, [&]() {
                            ++ticksG3;
                            if (ticksG3 > 80) {
                                timerG3.stop();
                                if (auto *p = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget()))
                                    p->close();
                                return;
                            }
                            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                            if (!popup || sawAxisTitlePopupG3)
                                return;
                            sawAxisTitlePopupG3 = true;
                            if (mlAxisTitleG) {
                                mlAxisTitleG->close();
                                if (mlAxisTitleG)
                                    delete mlAxisTitleG.data();
                            }
                            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                            popup->close();
                        });
                        timerG3.start();
                        gAxisTitleG->showAxisTitleMenu(QwtScaleDraw::BottomScale);
                        timerG3.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawAxisTitlePopupG3 && mlAxisTitleG.isNull()
                                           && gAxisTitleG.isNull(),
                                   "80f-insp-modal-uaf-G3: showAxisTitleMenu parentless teardown failed (saw="
                                           << sawAxisTitlePopupG3
                                           << ", mlNull=" << mlAxisTitleG.isNull()
                                           << ", gNull=" << gAxisTitleG.isNull() << ")");
                    }

                    // Subtest F2 (Round 12 + 13 ASan): RichTextBarEntry::menuUnits synchronous
                    // submenu deletion on LanguageChange when closed, and safe deferred close
                    // + rebuild when LanguageChange fires while popped visible, plus direct
                    // dock destruction mid-open menuUnits.
                    {
                        wDiag->switchToLanguage(QStringLiteral("en"));
                        QCoreApplication::processEvents();

                        QPointer<GraphInspectorDock> tmpDockF2 = new GraphInspectorDock(wDiag);
                        tmpDockF2->setFloating(true);
                        tmpDockF2->show();
                        tmpDockF2->setCollapsedRail(false);
                        tmpDockF2->setTargetMultiLayer(mlModal, true);
                        tmpDockF2->switchToInspectorPage(GraphInspectorDock::PageTabAxis);
                        QCoreApplication::processEvents();

                        QToolButton *btnUnitF2 = nullptr;
                        for (QToolButton *tb : tmpDockF2->findChildren<QToolButton *>()) {
                            if (tb && tb->isVisible()
                                && tb->popupMode() == QToolButton::InstantPopup
                                && tb->menu() != nullptr
                                && tb->property("_tg_tip_fmt").toString()
                                        == QLatin1String(
                                                "Insert Symbol, Unit, or Journal Axis Template")) {
                                btnUnitF2 = tb;
                                break;
                            }
                        }
                        CHECK_COND(btnUnitF2 != nullptr && btnUnitF2->menu() != nullptr,
                                   "80f-insp-modal-uaf-F2: visible Units InstantPopup button not found");

                        // Closed check: send ONLY LanguageChange (do NOT flush DeferredDelete).
                        // Synchronous `delete sub` must immediately null oldSubBeforeF2 and keep count == 2.
                        const QList<QMenu *> subsBeforeF2 = btnUnitF2->menu()->findChildren<QMenu *>(
                                QString(), Qt::FindDirectChildrenOnly);
                        CHECK_COND(subsBeforeF2.size() == 2,
                                   "80f-insp-modal-uaf-F2: expected 2 Units submenus before LanguageChange (got "
                                           << subsBeforeF2.size() << ")");
                        QPointer<QMenu> oldSubBeforeF2(subsBeforeF2.first());

                        wDiag->switchToLanguage(QStringLiteral("ja"));
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
                        const QList<QMenu *> subsAfterJaF2 = btnUnitF2->menu()->findChildren<QMenu *>(
                                QString(), Qt::FindDirectChildrenOnly);
                        CHECK_COND(oldSubBeforeF2.isNull() && subsAfterJaF2.size() == 2,
                                   "80f-insp-modal-uaf-F2: closed Units submenu did not synchronously delete old submenu (null="
                                           << oldSubBeforeF2.isNull()
                                           << ", count=" << subsAfterJaF2.size() << ")");

                        wDiag->switchToLanguage(QStringLiteral("en"));
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
                        QCoreApplication::processEvents();

                        // Visible popup check: deliver LanguageChange WHILE menuUnits is popped visible.
                        const QList<QMenu *> subsVisiblePreF2 = btnUnitF2->menu()->findChildren<QMenu *>(
                                QString(), Qt::FindDirectChildrenOnly);
                        CHECK_COND(subsVisiblePreF2.size() == 2,
                                   "80f-insp-modal-uaf-F2: expected 2 Units submenus before visible popup test");
                        QPointer<QMenu> oldSubVisibleF2(subsVisiblePreF2.first());

                        int ticksF2 = 0;
                        bool sawUnitsPopupF2 = false;
                        bool rebuiltWhileVisibleF2 = false;
                        bool oldSubDeferredF2 = false;
                        QTimer timerF2;
                        timerF2.setInterval(15);
                        QObject::connect(&timerF2, &QTimer::timeout, [&]() {
                            ++ticksF2;
                            if (ticksF2 > 80) {
                                timerF2.stop();
                                if (auto *p = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget()))
                                    p->close();
                                return;
                            }
                            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                            if (!popup || sawUnitsPopupF2)
                                return;
                            sawUnitsPopupF2 = true;
                            wDiag->switchToLanguage(QStringLiteral("ja"));
                            QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
                            const QList<QMenu *> subsNow = popup->findChildren<QMenu *>(
                                    QString(), Qt::FindDirectChildrenOnly);
                            oldSubDeferredF2 = !oldSubVisibleF2.isNull();
                            rebuiltWhileVisibleF2 = (!popup->isVisible() && subsNow.size() >= 2);
                            if (tmpDockF2)
                                delete tmpDockF2.data();
                            if (auto *ap = QApplication::activePopupWidget())
                                ap->close();
                        });
                        timerF2.start();
                        btnUnitF2->showMenu();
                        timerF2.stop();
                        wDiag->switchToLanguage(QStringLiteral("en"));
                        QCoreApplication::processEvents();
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                        CHECK_COND(sawUnitsPopupF2 && rebuiltWhileVisibleF2 && oldSubDeferredF2
                                           && oldSubVisibleF2.isNull() && tmpDockF2.isNull(),
                                   "80f-insp-modal-uaf-F2: active menuUnits LanguageChange + dock teardown failed (saw="
                                           << sawUnitsPopupF2 << ", rebuilt=" << rebuiltWhileVisibleF2
                                           << ", deferred=" << oldSubDeferredF2
                                           << ", subNullAfter=" << oldSubVisibleF2.isNull()
                                           << ", dockNull=" << tmpDockF2.isNull() << ")");

                        // Direct dock destruction mid-open menuUnits (without prior LanguageChange)
                        QPointer<GraphInspectorDock> tmpDockF2b = new GraphInspectorDock(wDiag);
                        tmpDockF2b->setFloating(true);
                        tmpDockF2b->show();
                        tmpDockF2b->setCollapsedRail(false);
                        tmpDockF2b->setTargetMultiLayer(mlModal, true);
                        tmpDockF2b->switchToInspectorPage(GraphInspectorDock::PageTabAxis);
                        QCoreApplication::processEvents();
                        QToolButton *btnUnitF2b = nullptr;
                        for (QToolButton *tb : tmpDockF2b->findChildren<QToolButton *>()) {
                            if (tb && tb->isVisible()
                                && tb->popupMode() == QToolButton::InstantPopup
                                && tb->menu() != nullptr
                                && tb->property("_tg_tip_fmt").toString()
                                        == QLatin1String(
                                                "Insert Symbol, Unit, or Journal Axis Template")) {
                                btnUnitF2b = tb;
                                break;
                            }
                        }
                        CHECK_COND(btnUnitF2b != nullptr,
                                   "80f-insp-modal-uaf-F2b: visible Units button not found");
                        int ticksF2b = 0;
                        bool sawUnitsPopupF2b = false;
                        QTimer timerF2b;
                        timerF2b.setInterval(15);
                        QObject::connect(&timerF2b, &QTimer::timeout, [&]() {
                            ++ticksF2b;
                            if (ticksF2b > 80) {
                                timerF2b.stop();
                                if (auto *p = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget()))
                                    p->close();
                                return;
                            }
                            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                            if (!popup || sawUnitsPopupF2b)
                                return;
                            sawUnitsPopupF2b = true;
                            if (tmpDockF2b)
                                delete tmpDockF2b.data();
                            if (auto *ap = QApplication::activePopupWidget())
                                ap->close();
                        });
                        timerF2b.start();
                        btnUnitF2b->showMenu();
                        timerF2b.stop();
                        QCoreApplication::processEvents();
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                        CHECK_COND(sawUnitsPopupF2b && tmpDockF2b.isNull(),
                                   "80f-insp-modal-uaf-F2b: direct menuUnits dock destruction failed (saw="
                                           << sawUnitsPopupF2b << ", null=" << tmpDockF2b.isNull()
                                           << ")");
                    }

                    // Subtest H (Round 12 ASan): GraphMiniToolbar parentless QMenu & non-self
                    // QColorDialog/QMessageBox with mid-exec GraphMiniToolbar destruction.
                    {
                        struct MiniToolbarEnableGuard80
                        {
                            bool prev = GraphMiniToolbar::isMiniToolbarEnabled();
                            explicit MiniToolbarEnableGuard80(bool on)
                            {
                                GraphMiniToolbar::setMiniToolbarEnabled(on);
                            }
                            ~MiniToolbarEnableGuard80()
                            {
                                GraphMiniToolbar::setMiniToolbarEnabled(prev);
                            }
                        } miniEn80(true);

                        MultiLayer *mlMt = wDiag->multilayerPlot(
                                tModal, QStringList() << tModal->colName(1), Graph::Scatter);
                        Graph *gMt = mlMt ? mlMt->activeGraph() : nullptr;
                        CHECK_COND(mlMt && gMt && gMt->curve(0), "80f-insp-modal-uaf-H: setup failed");
                        mlMt->askOnCloseEvent(false);
                        if (!gMt->legend())
                            gMt->newLegend();
                        CHECK_COND(gMt->legend() != nullptr,
                                   "80f-insp-modal-uaf-H: legend setup failed");
                        gMt->legend()->setText(QStringLiteral("Custom Legend Text 80H3"));

                        // H1: onCurvePickLineWidth parentless QMenu + mid-exec destruction
                        QPointer<GraphMiniToolbar> mt1 = new GraphMiniToolbar(wDiag);
                        mt1->popupForCurve(gMt, 0, QPoint(200, 200));
                        int ticksH1 = 0;
                        bool sawMtMenu1 = false;
                        QTimer timerH1;
                        timerH1.setInterval(15);
                        QObject::connect(&timerH1, &QTimer::timeout, [&]() {
                            ++ticksH1;
                            if (ticksH1 > 80) {
                                timerH1.stop();
                                if (auto *p = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget()))
                                    p->close();
                                return;
                            }
                            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                            if (!popup || sawMtMenu1)
                                return;
                            sawMtMenu1 = true;
                            if (mt1)
                                delete mt1.data();
                            popup->close();
                        });
                        timerH1.start();
                        QMetaObject::invokeMethod(mt1.data(), "onCurvePickLineWidth",
                                                  Qt::DirectConnection);
                        timerH1.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawMtMenu1 && mt1.isNull(),
                                   "80f-insp-modal-uaf-H1: MiniToolbar onCurvePickLineWidth teardown failed (saw="
                                           << sawMtMenu1 << ", null=" << mt1.isNull() << ")");

                        // H2: onCurvePickColor -> Custom Color QColorDialog with mt2 destroyed
                        QPointer<GraphMiniToolbar> mt2 = new GraphMiniToolbar(wDiag);
                        mt2->popupForCurve(gMt, 0, QPoint(200, 200));
                        int ticksH2 = 0;
                        bool sawMtPopup2 = false;
                        bool sawMtColorDlg2 = false;
                        QTimer timerH2;
                        timerH2.setInterval(15);
                        QObject::connect(&timerH2, &QTimer::timeout, [&]() {
                            ++ticksH2;
                            if (ticksH2 > 80) {
                                timerH2.stop();
                                if (auto *p = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget()))
                                    p->close();
                                if (auto *w = QApplication::activeModalWidget())
                                    w->close();
                                return;
                            }
                            if (!sawMtPopup2) {
                                if (auto *popup = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget())) {
                                    sawMtPopup2 = true;
                                    const QList<QAction *> acts = popup->actions();
                                    if (!acts.isEmpty()) {
                                        QAction *customAct = acts.last();
                                        popup->setActiveAction(customAct);
                                        QKeyEvent kp(QEvent::KeyPress, Qt::Key_Return,
                                                     Qt::NoModifier);
                                        QApplication::sendEvent(popup, &kp);
                                        QKeyEvent kr(QEvent::KeyRelease, Qt::Key_Return,
                                                     Qt::NoModifier);
                                        QApplication::sendEvent(popup, &kr);
                                        return;
                                    }
                                    popup->close();
                                }
                            } else if (!sawMtColorDlg2) {
                                if (auto *cdlg = qobject_cast<QColorDialog *>(
                                            QApplication::activeModalWidget())) {
                                    sawMtColorDlg2 = true;
                                    if (mt2)
                                        delete mt2.data();
                                    cdlg->reject();
                                }
                            }
                        });
                        timerH2.start();
                        QMetaObject::invokeMethod(mt2.data(), "onCurvePickColor",
                                                  Qt::DirectConnection);
                        timerH2.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawMtPopup2 && sawMtColorDlg2 && mt2.isNull(),
                                   "80f-insp-modal-uaf-H2: MiniToolbar Custom Color teardown failed (popup="
                                           << sawMtPopup2 << ", dlg=" << sawMtColorDlg2
                                           << ", null=" << mt2.isNull() << ")");

                        // H3: onLegendReconstruct QMessageBox::question with mt3 destroyed
                        QPointer<GraphMiniToolbar> mt3 = new GraphMiniToolbar(wDiag);
                        mt3->popupForLegend(gMt, gMt->legend(), QPoint(200, 200));
                        int ticksH3 = 0;
                        bool sawMtMsg3 = false;
                        QTimer timerH3;
                        timerH3.setInterval(15);
                        QObject::connect(&timerH3, &QTimer::timeout, [&]() {
                            ++ticksH3;
                            if (ticksH3 > 80) {
                                timerH3.stop();
                                if (auto *w = QApplication::activeModalWidget())
                                    w->close();
                                return;
                            }
                            if (auto *mb = qobject_cast<QMessageBox *>(
                                        QApplication::activeModalWidget())) {
                                if (!sawMtMsg3) {
                                    sawMtMsg3 = true;
                                    if (mt3)
                                        delete mt3.data();
                                    mb->reject();
                                }
                            }
                        });
                        timerH3.start();
                        QMetaObject::invokeMethod(mt3.data(), "onLegendReconstruct",
                                                  Qt::DirectConnection);
                        timerH3.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawMtMsg3 && mt3.isNull(),
                                   "80f-insp-modal-uaf-H3: MiniToolbar onLegendReconstruct teardown failed (saw="
                                           << sawMtMsg3 << ", null=" << mt3.isNull() << ")");
                    }

                    // Subtest I (Round 12 ASan): AxesDialog non-self modal QMessageBox parent
                    // and unified QPointer<Graph> d_graph safety after Graph teardown.
                    {
                        // I1: destroy heap AxesDialog while QMessageBox::critical is open
                        QPointer<AxesDialogProbe80> ad1 = new AxesDialogProbe80();
                        ad1->setParent(wDiag, Qt::Dialog);
                        ad1->setAttribute(Qt::WA_DeleteOnClose, true);
                        ad1->setGraph(gModal);

                        int ticksI1 = 0;
                        bool sawAxesMsg1 = false;
                        QTimer timerI1;
                        timerI1.setInterval(15);
                        QObject::connect(&timerI1, &QTimer::timeout, [&]() {
                            ++ticksI1;
                            if (ticksI1 > 80) {
                                timerI1.stop();
                                if (auto *w = QApplication::activeModalWidget())
                                    w->close();
                                return;
                            }
                            if (auto *mb = qobject_cast<QMessageBox *>(
                                        QApplication::activeModalWidget())) {
                                if (!sawAxesMsg1) {
                                    sawAxesMsg1 = true;
                                    if (ad1)
                                        delete ad1.data();
                                    mb->reject();
                                }
                            }
                        });
                        timerI1.start();
                        ad1->setScaleBoundsAndUpdate(QStringLiteral("10"), QStringLiteral("1"));
                        timerI1.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawAxesMsg1 && ad1.isNull(),
                                   "80f-insp-modal-uaf-I1: AxesDialog mid-QMessageBox destruction failed (saw="
                                           << sawAxesMsg1 << ", null=" << ad1.isNull() << ")");

                        // I2: QPointer<Graph> d_graph nulls cleanly across slots after Graph teardown
                        QPointer<MultiLayer> mlAxesDead = wDiag->multilayerPlot(
                                tModal, QStringList() << tModal->colName(1), Graph::Scatter);
                        QPointer<Graph> gAxesDead =
                                mlAxesDead ? mlAxesDead->activeGraph() : nullptr;
                        CHECK_COND(mlAxesDead && gAxesDead, "80f-insp-modal-uaf-I2: setup failed");
                        mlAxesDead->askOnCloseEvent(false);

                        AxesDialogProbe80 ad2;
                        ad2.setParent(wDiag, Qt::Dialog);
                        ad2.setGraph(gAxesDead);

                        mlAxesDead->close();
                        if (mlAxesDead)
                            delete mlAxesDead.data();
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                        CHECK_COND(gAxesDead.isNull(),
                                   "80f-insp-modal-uaf-I2: Graph did not teardown cleanly");

                        ad2.showAxisFormatOptions(0);
                        ad2.showAxisFormula(0);
                        ad2.setAxisType(0);
                        ad2.updateLabelsFormat(0);
                        const bool updI2 = ad2.updatePlot();
                        CHECK_COND(!updI2,
                                   "80f-insp-modal-uaf-I2: updatePlot succeeded on destroyed Graph");
                    }

                    // Subtest J (Round 13 ASan): PlotDialog SelfEditDepthGuard + non-self modal
                    // QMessageBox/QDialog/parentless QMenu + unified QPointer<MultiLayer>/<Graph>.
                    {
                        Table *tHistJ = wDiag->newTable(6, 2, "T80PlotHistJ", "HistJ80");
                        for (int r = 0; r < 6; ++r) {
                            tHistJ->setCell(r, 0, double(r));
                            tHistJ->setCell(r, 1, double(r % 3 + 1));
                        }
                        MultiLayer *mlHistJ = wDiag->multilayerPlot(
                                tHistJ, QStringList() << tHistJ->colName(1), Graph::Histogram);
                        CHECK_COND(mlHistJ && mlHistJ->activeGraph(),
                                   "80f-insp-modal-uaf-J1: mlHistJ setup failed");
                        mlHistJ->askOnCloseEvent(false);

                        // J1: commitParamsForItem + evaluateHistogramInputs + quit() mid-QMessageBox
                        QPointer<PlotDialog> pd1 = new PlotDialog(true, wDiag);
                        pd1->setAttribute(Qt::WA_DeleteOnClose, true);
                        pd1->setMultiLayer(mlHistJ);
                        pd1->show();
                        pd1->selectCurve(0);
                        QCoreApplication::processEvents();

                        QCheckBox *chkAutoJ = nullptr;
                        for (QCheckBox *cb : pd1->findChildren<QCheckBox *>()) {
                            if (cb && cb->text().contains(QStringLiteral("Automatic Binning"))) {
                                chkAutoJ = cb;
                                break;
                            }
                        }
                        CHECK_COND(chkAutoJ != nullptr,
                                   "80f-insp-modal-uaf-J1: Automatic Binning checkbox not found");
                        QWidget *histPageJ = chkAutoJ->parentWidget();
                        if (QTabWidget *twJ = pd1->findChild<QTabWidget *>())
                            twJ->setCurrentWidget(histPageJ);
                        chkAutoJ->setChecked(false);
                        if (QGroupBox *grpHJ = histPageJ ? histPageJ->findChild<QGroupBox *>() : nullptr) {
                            for (QLineEdit *le : grpHJ->findChildren<QLineEdit *>()) {
                                if (le)
                                    le->setText(QString());
                            }
                        }
                        QCoreApplication::processEvents();

                        int ticksJ1 = 0;
                        bool sawPdHistMsg = false;
                        QTimer timerJ1;
                        timerJ1.setInterval(15);
                        QObject::connect(&timerJ1, &QTimer::timeout, [&]() {
                            ++ticksJ1;
                            if (ticksJ1 > 80) {
                                timerJ1.stop();
                                if (auto *w = QApplication::activeModalWidget())
                                    w->close();
                                return;
                            }
                            if (auto *mb = qobject_cast<QMessageBox *>(
                                        QApplication::activeModalWidget())) {
                                if (!sawPdHistMsg) {
                                    sawPdHistMsg = true;
                                    if (pd1)
                                        delete pd1.data();
                                    mb->reject();
                                }
                            }
                        });
                        timerJ1.start();
                        QMetaObject::invokeMethod(pd1.data(), "quit", Qt::DirectConnection);
                        timerJ1.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawPdHistMsg && pd1.isNull(),
                                   "80f-insp-modal-uaf-J1: PlotDialog commitParamsForItem mid-QMessageBox UAF guard failed (saw="
                                           << sawPdHistMsg << ", null=" << pd1.isNull() << ")");

                        // J2a: openColorPaletteEditor with pd2 destroyed mid-QDialog
                        MultiLayer *mlPdJ = wDiag->multilayerPlot(
                                tModal, QStringList() << tModal->colName(1), Graph::Scatter);
                        CHECK_COND(mlPdJ && mlPdJ->activeGraph(),
                                   "80f-insp-modal-uaf-J2: mlPdJ setup failed");
                        mlPdJ->askOnCloseEvent(false);

                        QPointer<PlotDialog> pd2 = new PlotDialog(true, wDiag);
                        pd2->setAttribute(Qt::WA_DeleteOnClose, true);
                        pd2->setMultiLayer(mlPdJ);
                        pd2->selectCurve(0);
                        int ticksJ2a = 0;
                        bool sawPalDlg = false;
                        QTimer timerJ2a;
                        timerJ2a.setInterval(15);
                        QObject::connect(&timerJ2a, &QTimer::timeout, [&]() {
                            ++ticksJ2a;
                            if (ticksJ2a > 80) {
                                timerJ2a.stop();
                                if (auto *w = QApplication::activeModalWidget())
                                    w->close();
                                return;
                            }
                            if (auto *dlg = qobject_cast<QDialog *>(
                                        QApplication::activeModalWidget())) {
                                if (dlg != pd2.data() && !sawPalDlg) {
                                    sawPalDlg = true;
                                    if (pd2)
                                        delete pd2.data();
                                    dlg->reject();
                                }
                            }
                        });
                        timerJ2a.start();
                        QMetaObject::invokeMethod(pd2.data(), "openColorPaletteEditor",
                                                  Qt::DirectConnection);
                        timerJ2a.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawPalDlg && pd2.isNull(),
                                   "80f-insp-modal-uaf-J2a: openColorPaletteEditor teardown failed (saw="
                                           << sawPalDlg << ", null=" << pd2.isNull() << ")");

                        // J2b: openPageSetupFromDialog with pd3 destroyed mid-QDialog
                        QPointer<PlotDialog> pd3 = new PlotDialog(true, wDiag);
                        pd3->setAttribute(Qt::WA_DeleteOnClose, true);
                        pd3->setMultiLayer(mlPdJ);
                        int ticksJ2b = 0;
                        bool sawPsDlg = false;
                        QTimer timerJ2b;
                        timerJ2b.setInterval(15);
                        QObject::connect(&timerJ2b, &QTimer::timeout, [&]() {
                            ++ticksJ2b;
                            if (ticksJ2b > 80) {
                                timerJ2b.stop();
                                if (auto *w = QApplication::activeModalWidget())
                                    w->close();
                                return;
                            }
                            if (auto *dlg = qobject_cast<QDialog *>(
                                        QApplication::activeModalWidget())) {
                                if (dlg != pd3.data() && !sawPsDlg) {
                                    sawPsDlg = true;
                                    if (pd3)
                                        delete pd3.data();
                                    dlg->reject();
                                }
                            }
                        });
                        timerJ2b.start();
                        QMetaObject::invokeMethod(pd3.data(), "openPageSetupFromDialog",
                                                  Qt::DirectConnection);
                        timerJ2b.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawPsDlg && pd3.isNull(),
                                   "80f-insp-modal-uaf-J2b: openPageSetupFromDialog teardown failed (saw="
                                           << sawPsDlg << ", null=" << pd3.isNull() << ")");

                        // J2c: removeSelectedCurve with pd4 destroyed mid-QMessageBox
                        QPointer<PlotDialog> pd4 = new PlotDialog(true, wDiag);
                        pd4->setAttribute(Qt::WA_DeleteOnClose, true);
                        pd4->setMultiLayer(mlPdJ);
                        pd4->show();
                        pd4->selectCurve(0);
                        QCoreApplication::processEvents();
                        int ticksJ2c = 0;
                        bool sawRemMsg = false;
                        QTimer timerJ2c;
                        timerJ2c.setInterval(15);
                        QObject::connect(&timerJ2c, &QTimer::timeout, [&]() {
                            ++ticksJ2c;
                            if (ticksJ2c > 80) {
                                timerJ2c.stop();
                                if (auto *w = QApplication::activeModalWidget())
                                    w->close();
                                return;
                            }
                            if (auto *mb = qobject_cast<QMessageBox *>(
                                        QApplication::activeModalWidget())) {
                                if (!sawRemMsg) {
                                    sawRemMsg = true;
                                    if (pd4)
                                        delete pd4.data();
                                    mb->reject();
                                }
                            }
                        });
                        timerJ2c.start();
                        QMetaObject::invokeMethod(pd4.data(), "removeSelectedCurve",
                                                  Qt::DirectConnection);
                        timerJ2c.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawRemMsg && pd4.isNull(),
                                   "80f-insp-modal-uaf-J2c: removeSelectedCurve teardown failed (saw="
                                           << sawRemMsg << ", null=" << pd4.isNull() << ")");

                        // J2d: contextMenuEvent parentless QMenu with pd5 destroyed mid-exec()
                        QPointer<PlotDialog> pd5 = new PlotDialog(true, wDiag);
                        pd5->setAttribute(Qt::WA_DeleteOnClose, true);
                        pd5->setMultiLayer(mlPdJ);
                        pd5->showAll(true);
                        pd5->move(80, 80);
                        pd5->resize(760, 500);
                        pd5->show();
                        pd5->selectCurve(0);
                        QCoreApplication::processEvents();
                        QTreeWidget *lb5 = pd5->findChild<QTreeWidget *>();
                        CHECK_COND(lb5 && lb5->currentItem(),
                                   "80f-insp-modal-uaf-J2d: listBox/currentItem not found");
                        lb5->expandAll();
                        lb5->scrollToItem(lb5->currentItem());
                        QCoreApplication::processEvents();
                        const QRect r5 = lb5->visualItemRect(lb5->currentItem());
                        CHECK_COND(r5.isValid() && !r5.isEmpty(),
                                   "80f-insp-modal-uaf-J2d: curve item rect not visible");
                        const QPoint gPos5 = lb5->viewport()->mapToGlobal(r5.center());
                        int ticksJ2d = 0;
                        bool sawCtxMenu5 = false;
                        QTimer timerJ2d;
                        timerJ2d.setInterval(15);
                        QObject::connect(&timerJ2d, &QTimer::timeout, [&]() {
                            ++ticksJ2d;
                            if (ticksJ2d > 80) {
                                timerJ2d.stop();
                                if (auto *p = qobject_cast<QMenu *>(
                                            QApplication::activePopupWidget()))
                                    p->close();
                                return;
                            }
                            if (auto *popup = qobject_cast<QMenu *>(
                                        QApplication::activePopupWidget())) {
                                if (!sawCtxMenu5) {
                                    sawCtxMenu5 = true;
                                    if (pd5)
                                        delete pd5.data();
                                    popup->close();
                                }
                            }
                        });
                        timerJ2d.start();
                        QContextMenuEvent ctxEv5(QContextMenuEvent::Mouse,
                                                 pd5->mapFromGlobal(gPos5), gPos5);
                        QApplication::sendEvent(pd5.data(), &ctxEv5);
                        timerJ2d.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawCtxMenu5 && pd5.isNull(),
                                   "80f-insp-modal-uaf-J2d: contextMenuEvent parentless QMenu teardown failed (saw="
                                           << sawCtxMenu5 << ", null=" << pd5.isNull() << ")");

                        // J2e: LayerItem::graph() and PlotDialog::d_ml nulling after MultiLayer teardown
                        QPointer<MultiLayer> mlPdDead = wDiag->multilayerPlot(
                                tModal, QStringList() << tModal->colName(1), Graph::Scatter);
                        CHECK_COND(mlPdDead != nullptr, "80f-insp-modal-uaf-J2e: setup failed");
                        mlPdDead->askOnCloseEvent(false);
                        PlotDialog pd6(true, wDiag);
                        pd6.setMultiLayer(mlPdDead);
                        QTreeWidget *lb6 = pd6.findChild<QTreeWidget *>();
                        LayerItem *layerIt6 = nullptr;
                        if (lb6) {
                            for (QTreeWidgetItemIterator it6(lb6); *it6; ++it6) {
                                if (auto *li = dynamic_cast<LayerItem *>(*it6)) {
                                    layerIt6 = li;
                                    break;
                                }
                            }
                        }
                        CHECK_COND(layerIt6 != nullptr && layerIt6->graph() != nullptr,
                                   "80f-insp-modal-uaf-J2e: LayerItem not found");

                        mlPdDead->close();
                        if (mlPdDead)
                            delete mlPdDead.data();
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                        CHECK_COND(mlPdDead.isNull() && layerIt6->graph() == nullptr,
                                   "80f-insp-modal-uaf-J2e: LayerItem::graph() did not null after MultiLayer teardown");

                        bool acceptResJ2e = false;
                        QMetaObject::invokeMethod(&pd6, "acceptParams", Qt::DirectConnection,
                                                  Q_RETURN_ARG(bool, acceptResJ2e));
                        QMetaObject::invokeMethod(&pd6, "openPageSetupFromDialog",
                                                  Qt::DirectConnection);
                        CHECK_COND(!acceptResJ2e,
                                   "80f-insp-modal-uaf-J2e: acceptParams succeeded on destroyed MultiLayer");
                    }

                    // Subtest K (Round 13 ASan): Plot3DDialog QPointer<Graph3D> d_plot and
                    // non-self modal QColorDialog/QMessageBox with mid-exec Plot3DDialog deletion.
                    {
                        const bool prevConfirm3d = wDiag->confirmClosePlot3D;
                        wDiag->confirmClosePlot3D = false;
                        QPointer<Graph3D> g3dK = wDiag->newPlot3D();
                        CHECK_COND(g3dK != nullptr, "80f-insp-modal-uaf-K: newPlot3D failed");
                        g3dK->addFunction("cos(x)*cos(y)", -2.0, 2.0, -2.0, 2.0, -1.0, 1.0);
                        QCoreApplication::processEvents();

                        // K1a: pickFromColor mid-QColorDialog destruction of Plot3DDialog
                        QPointer<Plot3DDialog> p3d1 = new Plot3DDialog(wDiag);
                        p3d1->setAttribute(Qt::WA_DeleteOnClose, true);
                        p3d1->setPlot(g3dK);
                        int ticksK1a = 0;
                        bool sawP3dColor = false;
                        QTimer timerK1a;
                        timerK1a.setInterval(15);
                        QObject::connect(&timerK1a, &QTimer::timeout, [&]() {
                            ++ticksK1a;
                            if (ticksK1a > 80) {
                                timerK1a.stop();
                                if (auto *w = QApplication::activeModalWidget())
                                    w->close();
                                return;
                            }
                            if (auto *cdlg = qobject_cast<QColorDialog *>(
                                        QApplication::activeModalWidget())) {
                                if (!sawP3dColor) {
                                    sawP3dColor = true;
                                    if (p3d1)
                                        delete p3d1.data();
                                    cdlg->reject();
                                }
                            }
                        });
                        timerK1a.start();
                        p3d1->pickFromColor();
                        timerK1a.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawP3dColor && p3d1.isNull(),
                                   "80f-insp-modal-uaf-K1a: Plot3DDialog pickFromColor teardown failed (saw="
                                           << sawP3dColor << ", null=" << p3d1.isNull() << ")");

                        // K1b: accept() / updatePlot() mid-QMessageBox destruction of Plot3DDialog
                        QPointer<Plot3DDialog> p3d2 = new Plot3DDialog(wDiag);
                        p3d2->setAttribute(Qt::WA_DeleteOnClose, true);
                        p3d2->setPlot(g3dK);
                        p3d2->setScales(QStringList{ QStringLiteral("10"), QStringLiteral("1"),
                                                     QStringLiteral("5"), QStringLiteral("5"),
                                                     QStringLiteral("0"), QStringLiteral("0"),
                                                     QStringLiteral("10"), QStringLiteral("5"),
                                                     QStringLiteral("5"), QStringLiteral("0"),
                                                     QStringLiteral("0"), QStringLiteral("10"),
                                                     QStringLiteral("5"), QStringLiteral("5"),
                                                     QStringLiteral("0") });
                        int ticksK1b = 0;
                        bool sawP3dMsg = false;
                        QTimer timerK1b;
                        timerK1b.setInterval(15);
                        QObject::connect(&timerK1b, &QTimer::timeout, [&]() {
                            ++ticksK1b;
                            if (ticksK1b > 80) {
                                timerK1b.stop();
                                if (auto *w = QApplication::activeModalWidget())
                                    w->close();
                                return;
                            }
                            if (auto *mb = qobject_cast<QMessageBox *>(
                                        QApplication::activeModalWidget())) {
                                if (!sawP3dMsg) {
                                    sawP3dMsg = true;
                                    if (p3d2)
                                        delete p3d2.data();
                                    mb->reject();
                                }
                            }
                        });
                        timerK1b.start();
                        p3d2->accept();
                        timerK1b.stop();
                        QCoreApplication::processEvents();
                        CHECK_COND(sawP3dMsg && p3d2.isNull(),
                                   "80f-insp-modal-uaf-K1b: Plot3DDialog accept/updatePlot teardown failed (saw="
                                           << sawP3dMsg << ", null=" << p3d2.isNull() << ")");

                        // K2: QPointer<Graph3D> d_plot safety after Graph3D teardown
                        Plot3DDialog p3d3(wDiag);
                        p3d3.setPlot(g3dK);
                        g3dK->close();
                        if (g3dK)
                            delete g3dK.data();
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                        CHECK_COND(g3dK.isNull(),
                                   "80f-insp-modal-uaf-K2: Graph3D did not teardown cleanly");
                        p3d3.pickDataColorMap();
                        wDiag->confirmClosePlot3D = prevConfirm3d;
                    }
                }
            }
        }

        w80->switchToLanguage(QStringLiteral("en"));
        QCoreApplication::processEvents();
        const QUrl enGuide = w80->quickGuideUrl();
        const QString enGuideStr = enGuide.toString();
        CHECK_COND(enGuideStr.contains(QStringLiteral("index.en.html"))
                           || enGuideStr.endsWith(QStringLiteral("README.md")),
                   "80f: EN quickGuideUrl resolved to wrong target: " << enGuideStr.toStdString());

        // 80f-i18n-restore-size (Round 9): QMdiSubWindow "Restore"/"Size" translations must be
        // context-scoped so global "Restore" -> "復元する" and "Size" -> "サイズ" are preserved.
        CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese(QStringLiteral("Restore"))
                           == QString::fromUtf8("復元する"),
                   "80f-i18n-restore-size: global 'Restore' translation overwritten");
        CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese(QStringLiteral("Size"))
                           == QString::fromUtf8("サイズ"),
                   "80f-i18n-restore-size: global 'Size' translation overwritten");
        CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapaneseInContext(
                           QStringLiteral("QMdiSubWindow"), QStringLiteral("Restore"))
                           == QString::fromUtf8("元のサイズに戻す"),
                   "80f-i18n-restore-size: QMdiSubWindow context 'Restore' translation missing");
        CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapaneseInContext(
                           QStringLiteral("QMdiSubWindow"), QStringLiteral("Size"))
                           == QString::fromUtf8("サイズ変更"),
                   "80f-i18n-restore-size: QMdiSubWindow context 'Size' translation missing");
    }

    w80->switchToLanguage(QStringLiteral("en"));
    QCoreApplication::processEvents();
    w80.reset();
    std::cout << "[PASS] Suite 80 passed.\n";
    return 0;
}
