#include "ole_test_common.h"
#include "Graph3D.h"
#include "GraphInspectorDock.h"
#include "ImportOPJU.h"
#include "PythonAutomationServer.h"
#include "ScaleDraw.h"
#include "core/Project.h"
#include "future/table/TableModel.h"
#include "future/table/TableView.h"

#include <QAbstractItemModel>
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
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUndoStack>

#include <cstring>

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
    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL) override { return S_OK; }
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

            // Round 7 P1 (07_inspector_page_ja): switching to Overall/Page tab before viewport
            // auto-fit resize must NOT snap back to Curve tab via modifiedPlot coalesced refresh.
            dock80->switchToInspectorPage(GraphInspectorDock::PageTabPageLayer);
            QCoreApplication::processEvents();
            CHECK_COND(dock80->propertyStack() && dock80->propertyStack()->currentIndex() == 0,
                       "80f-page-tab-preserve: failed to switch to PageTabPageLayer");
            wDiag->resize(1366, 768);
            pumpEventsMs80(90);
            CHECK_COND(dock80->propertyStack()->currentIndex() == 0,
                       "80f-page-tab-preserve: P1 regression! resize snapped Inspector away from Page tab to index "
                               << dock80->propertyStack()->currentIndex());

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
            }
        }

        w80->switchToLanguage(QStringLiteral("en"));
        QCoreApplication::processEvents();
        const QUrl enGuide = w80->quickGuideUrl();
        const QString enGuideStr = enGuide.toString();
        CHECK_COND(enGuideStr.contains(QStringLiteral("index.en.html"))
                           || enGuideStr.endsWith(QStringLiteral("README.md")),
                   "80f: EN quickGuideUrl resolved to wrong target: " << enGuideStr.toStdString());
    }

    w80->switchToLanguage(QStringLiteral("en"));
    QCoreApplication::processEvents();
    w80.reset();
    std::cout << "[PASS] Suite 80 passed.\n";
    return 0;
}
