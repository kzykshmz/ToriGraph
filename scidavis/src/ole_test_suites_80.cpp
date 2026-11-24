#include "ole_test_common.h"
#include "Graph3D.h"
#include "ScaleDraw.h"
#include "future/table/TableModel.h"
#include "future/table/TableView.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QKeySequence>
#include <QLocale>
#include <QMdiArea>
#include <QPointer>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>
#include <QUndoStack>

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
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

#ifdef Q_OS_WIN
struct MockDeadClientSite80 final : public IOleClientSite, public IAdviseSink
{
    ULONG refs = 1;
    bool containerDead = false;
    int saveObjectCount = 0;

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
        return S_OK;
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
#endif

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
    }
#endif

    // -------------------------------------------------------------------------
    // 80b: RecalcManager::Auto preserves Project Undo/Redo stack
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
        CHECK_COND(fitEdited != nullptr && !rm->hasDirtyLinks(gRec),
                   "80b: dirty link remained after Undo");
        const double yEndUndone = fitEdited->y(fitEdited->dataSize() - 1);
        CHECK_COND(std::fabs(yEndUndone - 8.0) < 1e-5,
                   "80b: fit curve did not revert on Undo (got " << yEndUndone << ")");

        ustack->redo();
        QCoreApplication::processEvents();
        fitEdited = gRec->curve(QStringLiteral("Rec80Fit"));
        CHECK_COND(fitEdited != nullptr && !rm->hasDirtyLinks(gRec),
                   "80b: dirty link remained after Redo");
        const double yEndRedone = fitEdited->y(fitEdited->dataSize() - 1);
        CHECK_COND(std::fabs(yEndRedone - yEndEdited) < 1e-5,
                   "80b: fit curve did not replay on Redo (got " << yEndRedone << " expected "
                                                                 << yEndEdited << ")");
        rm->removeLinksForGraph(gRec);
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
        pumpEventsMs80(60);
        CHECK_COND(std::fabs(rib->splitLeftRatio() - 0.40) < 0.02,
                   "80c: programmatic shrink drifted splitLeftRatio to "
                           << rib->splitLeftRatio());
        const double zoomShrunk = mlSp->pageZoom();
        CHECK_COND(zoomShrunk < zoomLarge - 0.10,
                   "80c: Split MultiLayer shrink did not auto-fit pageZoom (large="
                           << zoomLarge << " shrunk=" << zoomShrunk << ")");

        mlSp->setPageZoom(1.35);
        pumpEventsMs80(10);
        wVis->resize(900, 480);
        pumpEventsMs80(60);
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
        Legend *note = new Legend(gMask->plotWidget());
        note->setText(QStringLiteral("Note80"));
        const long noteId = gMask->insertTextMarker(note);
        CHECK_COND(noteId >= 0, "80d: insertTextMarker failed");
        gMask->setSelectedMarker(legId, false);
        gMask->setSelectedMarker(noteId, true);
        gMask->removeLegend();
        gMask->removeLegend();
        pumpEventsMs80(10);
        gMask->deselectMarker();
        CHECK_COND(!gMask->hasLegend(), "80d: removeLegend() did not remove legend safely");
    }

    // -------------------------------------------------------------------------
    // 80e: Locale RejectGroupSeparator, ScaleDraw comma ticks, log-axis Fit RSS/X
    // -------------------------------------------------------------------------
    {
        {
            LocaleGuard80 deLocale(QLocale(QLocale::German, QLocale::Germany));
            Table *tDe = w80->newTable(2, 1, "T80De", "De80");
            tDe->setCell(0, 0, 10.0);
            QTableView *tvDe = tDe->findChild<QTableView *>();
            CHECK_COND(tvDe != nullptr && tvDe->model() != nullptr, "80e: TableView/model null");
            QAbstractItemModel *tm = tvDe->model();
            tm->setData(tm->index(1, 0), QStringLiteral("1.250"), Qt::EditRole);
            CHECK_COND(!tDe->column(0)->isInvalid(1) && std::fabs(tDe->cell(1, 0) - 1.25) < 1e-9,
                       "80e: de_DE RejectGroupSeparator failed — '1.250' parsed as "
                               << tDe->cell(1, 0) << " instead of C-decimal 1.25");
            tm->setData(tm->index(1, 0), QStringLiteral("1,250"), Qt::EditRole);
            CHECK_COND(!tDe->column(0)->isInvalid(1) && std::fabs(tDe->cell(1, 0) - 1.25) < 1e-9,
                       "80e: de_DE decimal comma '1,250' did not parse as 1.25 (got "
                               << tDe->cell(1, 0) << ")");

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
        }

        // Log-axis PolynomialFit / LinearFit finite rss() & positive X curve
        Table *tLog = w80->newTable(4, 2, "T80LogFit", "LogFit80");
        for (int r = 0; r < 4; ++r) {
            tLog->setCell(r, 0, std::pow(2.0, double(r)));
            tLog->setCell(r, 1, std::pow(10.0, double(r + 1)));
        }
        MultiLayer *mlLog =
                w80->multilayerPlot(tLog, QStringList() << tLog->colName(1), Graph::Scatter);
        Graph *gLog = mlLog ? mlLog->activeGraph() : nullptr;
        CHECK_COND(gLog != nullptr && gLog->curve(0) != nullptr, "80e: gLog null");
        gLog->setScale(QwtPlot::xBottom, 0.5, 16.0, 0.0, 5, 5, 1);
        gLog->setScale(QwtPlot::yLeft, 1.0, 100000.0, 0.0, 5, 5, 1);

        LinearFit lf(w80, gLog, gLog->curve(0)->title().text());
        lf.setApparentFit(true);
        lf.setSilent(true);
        lf.fit();
        CHECK_COND(std::isfinite(lf.rss()) && lf.rss() >= 0.0,
                   "80e: apparent log LinearFit::rss() not finite: " << lf.rss());

        QString curveName = gLog->curve(0)->title().text();
        PolynomialFit pf(w80, gLog, curveName, 2, false);
        pf.setApparentFit(true);
        pf.setSilent(true);
        pf.fit();
        CHECK_COND(std::isfinite(pf.rss()) && pf.rss() >= 0.0,
                   "80e: apparent log PolynomialFit::rss() not finite: " << pf.rss());
        CHECK_COND(gLog->curves() >= 2, "80e: fit curve not added to gLog");
        QwtPlotCurve *fitC = gLog->curve(gLog->curves() - 1);
        CHECK_COND(fitC != nullptr && fitC->dataSize() >= 2, "80e: fit curve empty");
        for (int i = 0; i < fitC->dataSize(); ++i) {
            CHECK_COND(fitC->x(i) > 0.0 && std::isfinite(fitC->y(i)),
                       "80e: log-axis fit curve produced non-positive X or non-finite Y at " << i);
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

        // I18n canonical Export PDF + Paste Colors & Symbols Only + EN quickGuideUrl
        const QString jaExportPdf =
                ToriGraphFullJapaneseTranslator::lookupJapanese(QStringLiteral("Export PDF"));
        CHECK_COND(jaExportPdf
                           == QString::fromUtf8("PDF\xE3\x82\xA8\xE3\x82\xAF\xE3\x82\xB9"
                                                "\xE3\x83\x9D\xE3\x83\xBC\xE3\x83\x88"),
                   "80f: Export PDF JA translation mismatch");
        const QString jaPasteStyle = ToriGraphFullJapaneseTranslator::lookupJapanese(
                QStringLiteral("Paste Colors & Symbols Only"));
        CHECK_COND(!jaPasteStyle.isEmpty(),
                   "80f: missing JA translation for Paste Colors & Symbols Only");

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
