#include "ole_test_common.h"

#include "GraphInspectorDock.h"
#include "CanvasInlineTextEditor.h"
#include "ErrDialog.h"
#include "ScalePicker.h"
#include "ArrowMarker.h"
#include "SelectionMoveResizer.h"
#include "QwtErrorPlotCurve.h"
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFocusEvent>
#include <QMenu>
#include <QStackedWidget>
#include <QTextEdit>
#include <QUndoStack>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>

namespace {

struct Suite47AxesDialogProbe : public AxesDialog
{
    void stageCrossTabEditsAndAccept()
    {
        generalDialog.setCurrentWidget(scalesPage);
        axesList->setCurrentRow(0); // Bottom X
        boxStart->setText("-15");
        boxEnd->setText("275");
        // Switch row on Scales page without clicking Apply (used to wipe uncommitted edits!)
        axesList->setCurrentRow(2); // Left Y
        // Switch tab to Axes page and edit Bottom X title
        generalDialog.setCurrentWidget(axesPage);
        axesTitlesList->setCurrentRow(0); // Bottom X
        if (boxAutoAxisTitle)
            boxAutoAxisTitle->setChecked(false);
        boxTitle->setText("True Strain, <i>&epsilon;</i> (%)");
        accept();
    }
};

} // namespace

int runSuite47Phase7GraphGuiUsabilityAndZeroLeakGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 47: Phase 7 Graph Inspector Dock, Direct Canvas UX, Dialog Data-Loss Fixes, Column DnD, Per-MultiLayer Undo & Export Zero-Chrome Gate...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 47");
    QTemporaryDir tmpDir;
    CHECK_COND(tmpDir.isValid(), "Failed to create QTemporaryDir in Suite 47");

    ApplicationWindow *w47 = new ApplicationWindow();
    w47->applyUserSettings();
    w47->resize(1380, 880);
    w47->show();
    w47->switchToLanguage("ja");
    QApplication::processEvents();

    // =========================================================================
    // 1. GraphInspectorDock Existence, Responsive Collapse & 100% Japanese Audit
    // =========================================================================
    GraphInspectorDock *dock = w47->graphInspectorDock();
    CHECK_COND(dock != nullptr, "w47->graphInspectorDock() is null!");

    Table *t47 = w47->newTable(8, 5, "Al_Zn_Mg_1", "Underscore Alloy Data");
    CHECK_COND(t47 != nullptr, "Failed to create Al_Zn_Mg_1 table");
    t47->setColName(0, "Strain");
    t47->setColName(1, "Stress_T6");
    t47->setColName(2, "Stress_Err");
    t47->setColName(3, "Stress_RRA");
    t47->setColName(4, "Hardness_HV");
    t47->setColPlotDesignation(0, SciDAVis::X);
    t47->setColPlotDesignation(1, SciDAVis::Y);
    t47->setColPlotDesignation(2, SciDAVis::yErr);
    t47->setColPlotDesignation(3, SciDAVis::Y);
    t47->setColPlotDesignation(4, SciDAVis::Y);
    for (int r = 0; r < 8; ++r) {
        const double eps = double(r) * 2.5;
        t47->setCell(r, 0, eps);
        t47->setCell(r, 1, 420.0 + 18.0 * eps);
        t47->setCell(r, 2, 8.0 + 0.4 * eps);
        t47->setCell(r, 3, 380.0 + 15.0 * eps);
        t47->setCell(r, 4, 165.0 + 2.0 * eps);
    }

    MultiLayer *ml47 = w47->multilayerPlot(
            t47, QStringList() << "Al_Zn_Mg_1_Stress_T6" << "Al_Zn_Mg_1_Stress_RRA",
            Graph::LineSymbols);
    CHECK_COND(ml47 != nullptr && ml47->activeGraph() != nullptr, "Failed to create ml47");
    ml47->setName("Phase7Graph1");
    w47->activateSubWindow(ml47);
    ml47->resize(620, 560);
    ml47->show();
    QApplication::processEvents();

    Graph *g47 = ml47->activeGraph();
    CHECK_COND(dock->targetMultiLayer() == ml47,
               "GraphInspectorDock did not bind to active MultiLayer ml47!");

    // Audit Japanese localization across all 4 Inspector pages & header/rail widgets
    {
        int jaAsciiMisses = 0;
        auto checkText = [&](const QString &txt, const char *where) {
            if (txt.contains("Phase7Graph1") || txt.contains("Al_Zn_Mg_1")
                || txt.contains("Stress_") || txt.contains("Hardness_"))
                return;
            if (isUntranslatedAsciiUiLabel(txt)) {
                ++jaAsciiMisses;
                std::cerr << "[JA-MISS Suite47] " << where << ": '"
                          << txt.toUtf8().constData() << "'\n";
            }
        };

        for (int p = 0; p < 4; ++p) {
            dock->switchToInspectorPage(static_cast<GraphInspectorDock::InspectorPageIndex>(p));
            QApplication::processEvents();
        }
        for (QLabel *lbl : dock->findChildren<QLabel *>()) {
            if (lbl)
                checkText(lbl->text(), "GraphInspectorDock::QLabel");
        }
        for (QPushButton *pb : dock->findChildren<QPushButton *>()) {
            if (pb) {
                checkText(pb->text(), "GraphInspectorDock::QPushButton");
                checkText(pb->toolTip(), "GraphInspectorDock::QPushButton::toolTip");
            }
        }
        for (QCheckBox *cb : dock->findChildren<QCheckBox *>()) {
            if (cb)
                checkText(cb->text(), "GraphInspectorDock::QCheckBox");
        }
        for (QToolButton *tb : dock->findChildren<QToolButton *>()) {
            if (tb) {
                checkText(tb->text(), "GraphInspectorDock::QToolButton");
                checkText(tb->toolTip(), "GraphInspectorDock::QToolButton::toolTip");
            }
        }
        for (QComboBox *cmb : dock->findChildren<QComboBox *>()) {
            if (!cmb)
                continue;
            for (int i = 0; i < cmb->count(); ++i)
                checkText(cmb->itemText(i), "GraphInspectorDock::QComboBox::item");
        }
        for (QMenu *mnu : dock->findChildren<QMenu *>()) {
            if (!mnu)
                continue;
            for (QAction *act : mnu->actions()) {
                if (act && !act->isSeparator())
                    checkText(act->text(), "GraphInspectorDock::QMenu::QAction");
            }
        }
        CHECK_COND(jaAsciiMisses == 0,
                   "GraphInspectorDock contains untranslated English ASCII UI strings in 'ja' mode!");
    }

    // Verify Responsive Collapse (< 1080 px width, LayoutSplitTableGraph, and OLE Embedded)
    {
        ModernRibbonBar *rib47 = w47->modernRibbonBar();
        CHECK_COND(rib47 != nullptr, "rib47 is null");
        rib47->setWorkspaceLayoutMode(ModernRibbonBar::LayoutTabbed);
        w47->resize(1380, 880);
        w47->updateGraphInspectorResponsiveness();
        QApplication::processEvents();
        CHECK_COND(dock->isVisible() && !dock->isCollapsedRail(),
                   "GraphInspectorDock should be expanded at 1380px in LayoutTabbed!");

        w47->resize(960, 760);
        w47->updateGraphInspectorResponsiveness();
        QApplication::processEvents();
        CHECK_COND(dock->isCollapsedRail(),
                   "GraphInspectorDock failed to auto-collapse to 32px rail below 1080px width!");

        w47->resize(1380, 880);
        w47->updateGraphInspectorResponsiveness();
        QApplication::processEvents();
        CHECK_COND(!dock->isCollapsedRail(),
                   "GraphInspectorDock did not re-expand when mainWindow returned >= 1080px!");

        rib47->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
        w47->updateGraphInspectorResponsiveness();
        QApplication::processEvents();
        CHECK_COND(dock->isCollapsedRail(),
                   "GraphInspectorDock failed to auto-collapse in LayoutSplitTableGraph!");

        rib47->setWorkspaceLayoutMode(ModernRibbonBar::LayoutTabbed);
        w47->activateSubWindow(ml47);
        w47->updateGraphInspectorResponsiveness();
        QApplication::processEvents();

        ApplicationWindow *oleCheck = new ApplicationWindow();
        oleCheck->setOleEmbeddedMode(true);
        oleCheck->show();
        QApplication::processEvents();
        CHECK_COND(oleCheck->graphInspectorDock() != nullptr
                           && oleCheck->graphInspectorDock()->isVisible(),
                   "GraphInspectorDock MUST stay available in interactive OLE embedded mode (parity with standalone exe)!");
        oleCheck->savedProject();
        delete oleCheck;
    }

    w47->switchToLanguage("en");
    QApplication::processEvents();

    // =========================================================================
    // 2. Non-Destructive Curve Visibility (with ErrorBars) & Z-Order Reordering
    // =========================================================================
    {
        CHECK_COND(g47->addErrorBars("Al_Zn_Mg_1_Stress_T6", t47, "Al_Zn_Mg_1_Stress_Err"),
                   "Failed to add error bars to Al_Zn_Mg_1_Stress_T6");
        CHECK_COND(g47->curves() == 3, "Expected 3 plot curves (2 data + 1 error bar)");
        if (!g47->legend())
            g47->newLegend();
        g47->legend()->setText("\\c{1}Peak-Aged T6\n\\c{2}Over-Aged RRA");
        g47->replot();

        // Hide curve 0 -> curve 0 AND its attached error bar must hide non-destructively
        g47->showCurve(0, false);
        g47->replot();
        CHECK_COND(!g47->isCurveVisible(0), "Curve 0 should be hidden after showCurve(0, false)");
        bool errHiddenToo = true;
        for (int i = 0; i < g47->curves(); ++i) {
            if (auto *eb = dynamic_cast<QwtErrorPlotCurve *>(g47->curve(i))) {
                if (eb->isVisible())
                    errHiddenToo = false;
            }
        }
        CHECK_COND(errHiddenToo, "Hiding master curve 0 did not hide attached QwtErrorPlotCurve!");

        // Re-show curve 0 -> custom legend text "Peak-Aged T6" must still be intact!
        g47->showCurve(0, true);
        g47->replot();
        CHECK_COND(g47->isCurveVisible(0) && g47->legend()->text().contains("Peak-Aged T6"),
                   "Re-enabling curve 0 lost custom legend label 'Peak-Aged T6'!");

        // Reorder curve 0 and curve 1 (the two data curves: find index of Stress_RRA)
        const int rraIdxBefore = g47->curveIndex("Al_Zn_Mg_1_Stress_RRA");
        CHECK_COND(rraIdxBefore > 0, "Expected Al_Zn_Mg_1_Stress_RRA at index > 0");
        CHECK_COND(g47->moveCurve(rraIdxBefore, 0), "Graph::moveCurve(rraIdxBefore, 0) returned false");
        CHECK_COND(g47->curve(0)->title().text() == "Al_Zn_Mg_1_Stress_RRA",
                   "moveCurve did not move Al_Zn_Mg_1_Stress_RRA to index 0!");
        const int t6IdxAfter = g47->curveIndex("Al_Zn_Mg_1_Stress_T6");
        CHECK_COND(t6IdxAfter > 0 && g47->curve(t6IdxAfter)->z() > g47->curve(0)->z(),
                   "moveCurve did not synchronize QwtPlotItem::z() draw order!");
        // Restore original order
        CHECK_COND(g47->moveCurve(0, t6IdxAfter), "Failed to move curve back to original order");
    }

    // =========================================================================
    // 3. Direct Canvas Manipulation: 1-Motion Legend Drag, Arrow Delete vs Layer Delete,
    //    Inline Scale Bound Editor (No MiniToolbar Flash!) & Inline Text Editor
    // =========================================================================
    {
        QWidget *canvas = g47->plotWidget()->canvas();
        CHECK_COND(canvas != nullptr && g47->legend() != nullptr, "Canvas or Legend null in Suite 47");
        g47->legend()->setOrigin(QPoint(70, 50));
        g47->replot();
        QApplication::processEvents();

        g47->deselectMarker();
        QApplication::processEvents();
        CHECK_COND(!g47->markerSelected(), "Precondition: Legend must start unselected");

        const int initLegPt = g47->legend()->font().pointSize();
        const QPoint initLegTopLeft = g47->legend()->rect().topLeft();
        const QPoint pressPt = g47->legend()->rect().center();
        const QPoint midPt(pressPt.x() + 12, pressPt.y() + 10);
        const QPoint releasePt(pressPt.x() + 42, pressPt.y() + 28);

        QMouseEvent evPress(QEvent::MouseButtonPress, pressPt, canvas->mapToGlobal(pressPt),
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent evMove1(QEvent::MouseMove, midPt, canvas->mapToGlobal(midPt),
                            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent evMove2(QEvent::MouseMove, releasePt, canvas->mapToGlobal(releasePt),
                            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent evRel(QEvent::MouseButtonRelease, releasePt, canvas->mapToGlobal(releasePt),
                          Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(canvas, &evPress);
        QCoreApplication::sendEvent(canvas, &evMove1);
        QCoreApplication::sendEvent(canvas, &evMove2);
        QCoreApplication::sendEvent(canvas, &evRel);
        QApplication::processEvents();

        const QPoint movedLegTopLeft = g47->legend()->rect().topLeft();
        CHECK_COND(movedLegTopLeft != initLegTopLeft,
                   "1-motion click-and-drag on unselected Legend did NOT move it in one gesture!");
        CHECK_COND(g47->legend()->font().pointSize() == initLegPt
                           && g47->legend()->font().family() == "Arial",
                   "Dragging Legend mutated its Arial font family or point size!");
        g47->deselectMarker();

        // ArrowMarker selection retention & safe deletion (must NOT delete Graph layer!)
        ArrowMarker arr;
        arr.setStartPoint(5.0, 460.0);
        arr.setEndPoint(15.0, 620.0);
        arr.setColor(Qt::black);
        arr.setWidth(2);
        arr.drawEndArrow(true);
        const QString snapBeforeArrow = ml47->captureUndoSnapshot();
        g47->addArrow(&arr);
        ml47->pushUndoSnapshot("Add Arrow", snapBeforeArrow);
        g47->replot();
        g47->deselectMarker();
        QApplication::processEvents();
        CHECK_COND(g47->lineMarkerKeys().size() == 1, "Expected 1 ArrowMarker on g47");

        ArrowMarker *insertedArr = g47->arrow(g47->lineMarkerKeys().first());
        CHECK_COND(insertedArr != nullptr, "Inserted ArrowMarker lookup returned nullptr");
        const QPoint arrMidPx = (insertedArr->startPoint() + insertedArr->endPoint()) / 2;
        QMouseEvent arrClick(QEvent::MouseButtonPress, arrMidPx, canvas->mapToGlobal(arrMidPx),
                             Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent arrRel(QEvent::MouseButtonRelease, arrMidPx, canvas->mapToGlobal(arrMidPx),
                           Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(canvas, &arrClick);
        QCoreApplication::sendEvent(canvas, &arrRel);
        QApplication::processEvents();
        CHECK_COND(g47->markerSelected() && g47->arrowMarkerSelected(),
                   "Clicking ArrowMarker did not retain selectedMarker / arrowMarkerSelected()!");

        const QString snapBeforeDelArr = ml47->captureUndoSnapshot();
        g47->removeMarker();
        ml47->pushUndoSnapshot("Remove Arrow", snapBeforeDelArr);
        QApplication::processEvents();
        CHECK_COND(ml47->layers() == 1 && g47->lineMarkerKeys().isEmpty(),
                   "Removing selected ArrowMarker failed or deleted the Graph layer!");

        // Undo removes deletion -> restores ArrowMarker!
        w47->undo();
        QApplication::processEvents();
        g47 = ml47->activeGraph();
        CHECK_COND(g47 != nullptr && g47->lineMarkerKeys().size() == 1,
                   "Per-MultiLayer Undo failed to restore deleted ArrowMarker!");
        // Clean up arrow via Undo of Add Arrow
        w47->undo();
        QApplication::processEvents();
        g47 = ml47->activeGraph();
        CHECK_COND(g47 != nullptr && g47->lineMarkerKeys().isEmpty(),
                   "Second Undo failed to revert Add Arrow!");

        // Inline Axis Max Scale Bound Editor via double-click on xBottom scale end
        QwtScaleWidget *xBottomSw = g47->plotWidget()->axisWidget(QwtPlot::xBottom);
        CHECK_COND(xBottomSw != nullptr, "xBottom QwtScaleWidget is null");
        const QRect ticksRc = g47->scalePicker->scaleRect(xBottomSw);
        const QPoint maxEndPt(ticksRc.right() - 8, ticksRc.center().y());
        QMouseEvent dblScale(QEvent::MouseButtonDblClick, maxEndPt, xBottomSw->mapToGlobal(maxEndPt),
                             Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(xBottomSw, &dblScale);
        QApplication::processEvents();

        QLineEdit *inlineScaleEdit = xBottomSw->findChild<QLineEdit *>();
        CHECK_COND(inlineScaleEdit != nullptr && inlineScaleEdit->isVisible(),
                   "Double-clicking xBottom max tick end did not open inline scale QLineEdit!");
        for (GraphMiniToolbar *mtb : w47->findChildren<GraphMiniToolbar *>()) {
            CHECK_COND(!mtb->isVisible(),
                       "GraphMiniToolbar flashed during axis double-click!");
        }

        inlineScaleEdit->setText("320");
        QKeyEvent enterKey(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QCoreApplication::sendEvent(inlineScaleEdit, &enterKey);
        QApplication::processEvents();
        g47 = ml47->activeGraph();
        const double xUpperAfterInline =
                g47->plotWidget()->axisScaleDiv(QwtPlot::xBottom)->upperBound();
        CHECK_COND(std::fabs(xUpperAfterInline - 320.0) < 1e-6,
                   "Inline scale QLineEdit Enter did not commit upperBound = 320.0!");

        w47->undo();
        QApplication::processEvents();
        g47 = ml47->activeGraph();
        CHECK_COND(std::fabs(g47->plotWidget()->axisScaleDiv(QwtPlot::xBottom)->upperBound() - 320.0) > 1.0,
                   "Undo did not revert inline scale bound edit!");

        // Round 1 Regression: Inline Axis Scale Bound Editor MUST also commit on FocusOut (clicking away)
        xBottomSw = g47->plotWidget()->axisWidget(QwtPlot::xBottom);
        CHECK_COND(xBottomSw != nullptr, "xBottom QwtScaleWidget is null before FocusOut check");
        QCoreApplication::sendEvent(xBottomSw, &dblScale);
        QApplication::processEvents();
        inlineScaleEdit = xBottomSw->findChild<QLineEdit *>();
        CHECK_COND(inlineScaleEdit != nullptr && inlineScaleEdit->isVisible(),
                   "Second double-click on xBottom max tick end did not open inline scale QLineEdit!");
        inlineScaleEdit->setText("345");
        QFocusEvent focusOutEv(QEvent::FocusOut, Qt::MouseFocusReason);
        QCoreApplication::sendEvent(inlineScaleEdit, &focusOutEv);
        QApplication::processEvents();
        g47 = ml47->activeGraph();
        CHECK_COND(std::fabs(g47->plotWidget()->axisScaleDiv(QwtPlot::xBottom)->upperBound() - 345.0) < 1e-6,
                   "Inline scale QLineEdit FocusOut discarded typed bound 345.0 instead of committing!");
        w47->undo();
        QApplication::processEvents();
        g47 = ml47->activeGraph();

        // CanvasInlineTextEditor preserving \c{1} token & Arial font
        if (!g47->legend())
            g47->newLegend();
        long legKey = -1;
        if (g47->legend() && g47->plotWidget()) {
            for (int mk : g47->plotWidget()->markerKeys()) {
                if (g47->plotWidget()->marker(mk) == g47->legend()) {
                    legKey = mk;
                    break;
                }
            }
        }
        if (legKey >= 0) {
            CanvasInlineTextEditor *inEd = CanvasInlineTextEditor::openForLegendOrText(
                    g47, legKey, 0, QRect(200, 200, 120, 28));
            CHECK_COND(inEd != nullptr, "CanvasInlineTextEditor::openForLegendOrText returned null");
            QLineEdit *le = inEd->findChild<QLineEdit *>();
            if (le) {
                le->setText("T6 Peak-Aged (120 °C)");
                QKeyEvent retEv(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(le, &retEv);
                QApplication::processEvents();
                g47 = ml47->activeGraph();
                CHECK_COND(g47->legend() != nullptr
                                   && g47->legend()->text().contains("\\c{1}")
                                   && g47->legend()->text().contains("T6 Peak-Aged")
                                   && g47->legend()->font().family() == "Arial",
                           "CanvasInlineTextEditor stripped \\c{1} symbol token or broke Arial font!");
            }
        }
    }

    // =========================================================================
    // 4. Dialog Silent-Data-Loss Fixes (AxesDialog & PlotDialog) + Underscore ErrDialog
    // =========================================================================
    {
        g47 = ml47->activeGraph();
        Suite47AxesDialogProbe axProbe;
        axProbe.setGraph(g47);
        axProbe.stageCrossTabEditsAndAccept();
        QApplication::processEvents();

        g47 = ml47->activeGraph();
        const double xMinAx = g47->plotWidget()->axisScaleDiv(QwtPlot::xBottom)->lowerBound();
        const double xMaxAx = g47->plotWidget()->axisScaleDiv(QwtPlot::xBottom)->upperBound();
        CHECK_COND(std::fabs(xMinAx - (-15.0)) < 1e-6 && std::fabs(xMaxAx - 275.0) < 1e-6,
                   "AxesDialog silently lost Bottom scale edits when switching axis row & tab before OK!");
        CHECK_COND(g47->axisTitle(QwtPlot::xBottom).contains("True Strain")
                           && g47->axisTitleFont(QwtPlot::xBottom).family() == "Arial",
                   "AxesDialog failed to commit Bottom axis title alongside scale edit or broke Arial!");

        // PlotDialog cross-tab Line width + Hollow Symbol commit
        PlotDialog pd(true, w47);
        pd.initFonts(QFont("Arial", 14), QFont("Arial", 14), QFont("Arial", 12), QFont("Arial", 12));
        pd.setMultiLayer(ml47);
        pd.selectCurve(0);
        QApplication::processEvents();

        // Set curve 0 symbol to Ellipse first so hollow brush check applies to a visible symbol
        g47->setCurveSymbol(0, QwtSymbol(QwtSymbol::Ellipse, QBrush(Qt::black),
                                         QPen(Qt::black, 1), QSize(9, 9)));
        pd.selectCurve(0);
        QApplication::processEvents();

        QTabWidget *pdTabs = pd.findChild<QTabWidget *>();
        CHECK_COND(pdTabs != nullptr, "PlotDialog QTabWidget not found");
        bool editedLineWidth = false;
        bool checkedHollow = false;
        for (int ti = 0; ti < pdTabs->count(); ++ti) {
            QWidget *pg = pdTabs->widget(ti);
            pdTabs->setCurrentIndex(ti);
            QApplication::processEvents();
            for (QCheckBox *cb : pg->findChildren<QCheckBox *>()) {
                if (cb && cb->text().contains("Hollow", Qt::CaseInsensitive)) {
                    cb->setChecked(true);
                    checkedHollow = true;
                }
            }
            if (!editedLineWidth && pg->findChildren<QCheckBox *>().isEmpty()) {
                // Line tab has boxPenWidth spinbox
                for (QSpinBox *sb : pg->findChildren<QSpinBox *>()) {
                    if (sb && sb->minimum() >= 1 && sb->maximum() <= 100 && sb->value() <= 5) {
                        sb->setValue(4);
                        editedLineWidth = true;
                        break;
                    }
                }
            }
        }
        // Leave currentWidget on the first tab (NOT Symbol tab!) and invoke acceptParams()
        pdTabs->setCurrentIndex(0);
        bool invokeOk = false;
        QMetaObject::invokeMethod(&pd, "acceptParams", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, invokeOk));
        QApplication::processEvents();
        g47 = ml47->activeGraph();
        CHECK_COND(checkedHollow && Graph::isSymbolWhiteHollow(g47->curve(0)->symbol()),
                   "PlotDialog lost Hollow Symbol (#FFFFFF) edit when OK was pressed from another tab!");

        // Underscore Table Name ("Al_Zn_Mg_1") in ErrDialog
        ErrDialog errDlg(w47);
        errDlg.setSrcTables(w47->tableList());
        errDlg.setCurveNames(g47->analysableCurvesList());
        QApplication::processEvents();
        bool foundStressErrCol = false;
        for (QComboBox *cmb : errDlg.findChildren<QComboBox *>()) {
            for (int i = 0; i < cmb->count(); ++i) {
                if (cmb->itemText(i).contains("Stress_Err"))
                    foundStressErrCol = true;
            }
        }
        CHECK_COND(foundStressErrCol,
                   "ErrDialog failed to populate columns for underscore-named table 'Al_Zn_Mg_1'!");
    }

    // =========================================================================
    // 5. Worksheet Column MIME Drag-and-Drop + Per-MultiLayer Undo Stack Isolation
    // =========================================================================
    {
        Table *bgTbl = w47->newTable(4, 2, "BgUndoIsolationTbl", "Background Worksheet");
        CHECK_COND(bgTbl != nullptr, "Failed to create BgUndoIsolationTbl");
        bgTbl->setCell(0, 0, 777.0);
        CHECK_COND(std::fabs(bgTbl->cell(0, 0) - 777.0) < 1e-9, "bgTbl initial cell mismatch");

        w47->activateSubWindow(ml47);
        QApplication::processEvents();
        g47 = ml47->activeGraph();
        const int curvesBeforeDrop = g47->curves();

        QMimeData *mime = new QMimeData();
        QJsonObject payload;
        payload["table"] = QStringLiteral("Al_Zn_Mg_1");
        payload["columns"] = QJsonArray{ QStringLiteral("Hardness_HV") };
        mime->setData(QLatin1String(MultiLayer::kColumnMimeType),
                      QJsonDocument(payload).toJson(QJsonDocument::Compact));

        const QPoint dropPos = ml47->rect().center();
        QDragEnterEvent dragEnter(dropPos, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
        ml47->dragEnterEvent(&dragEnter);
        CHECK_COND(dragEnter.isAccepted(),
                   "MultiLayer::dragEnterEvent rejected application/x-torigraph-columns!");

        QDropEvent dropEv(dropPos, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
        ml47->dropEvent(&dropEv);
        delete mime;
        QApplication::processEvents();

        g47 = ml47->activeGraph();
        CHECK_COND(g47->curves() == curvesBeforeDrop + 1,
                   "Dropping column MIME onto MultiLayer did not add Hardness_HV curve!");

        // Undo while ml47 is active MUST revert the dropped curve WITHOUT touching bgTbl!
        w47->undo();
        QApplication::processEvents();
        g47 = ml47->activeGraph();
        CHECK_COND(g47->curves() == curvesBeforeDrop,
                   "Undo after column MIME drop did not revert MultiLayer curve count!");
        CHECK_COND(std::fabs(bgTbl->cell(0, 0) - 777.0) < 1e-9,
                   "Ctrl+Z on active MultiLayer corrupted background Table cell!");

        // Redo restores the dropped curve!
        w47->redo();
        QApplication::processEvents();
        g47 = ml47->activeGraph();
        CHECK_COND(g47->curves() == curvesBeforeDrop + 1,
                   "Redo did not re-apply column MIME drop on MultiLayer!");
    }

    // =========================================================================
    // 6. Presets (70% Square Frame, Smart Legend, (a)(b) Panel Labels) &
    //    100% Zero-Chrome Export + PowerPoint OLE Metafile Gate
    // =========================================================================
    {
        ml47->applySquareCanvasFrame70Percent();
        QApplication::processEvents();
        g47 = ml47->activeGraph();
        const QRect sqCanvas = g47->plotWidget()->plotLayout()->canvasRect();
        CHECK_COND(std::abs(sqCanvas.width() - sqCanvas.height()) <= 2,
                   "applySquareCanvasFrame70Percent did not produce a 1:1 square plot canvas!");

        double overlapScore = -1.0;
        const QString anchorChosen = g47->placeLegendAnchor(Graph::LegendAutoAvoidData, &overlapScore);
        CHECK_COND(!anchorChosen.isEmpty() && overlapScore >= 0.0,
                   "placeLegendAnchor(LegendAutoAvoidData) failed!");

        ml47->setPanelLabels(true, MultiLayer::PanelOutsideTopLeft, "(%a)", 14);
        CHECK_COND(ml47->hasPanelLabels(), "MultiLayer::setPanelLabels(true) getter mismatch");

        // Dedicated pure-black-on-white graph with ALL interactive chrome active simultaneously
        Table *tClean = w47->newTable(20, 2, "CleanExportTbl", "Zero Chrome Gate");
        for (int r = 0; r < 20; ++r) {
            const double x = double(r);
            tClean->setCell(r, 0, x);
            tClean->setCell(r, 1, 10.0 + 3.0 * x);
        }
        MultiLayer *mlClean = w47->multilayerPlot(
                tClean, QStringList() << "CleanExportTbl_2", Graph::Line);
        CHECK_COND(mlClean != nullptr && mlClean->activeGraph() != nullptr, "mlClean null");
        mlClean->setPageSizePhysical(85.0, 85.0, MultiLayer::UnitMm, 300);
        Graph *gClean = mlClean->activeGraph();
        gClean->setCurvePen(0, QPen(Qt::black, 2));
        gClean->removeLegend();
        gClean->setXAxisTitle("Strain (%)");
        gClean->setYAxisTitle("Stress (MPa)");
        gClean->replot();
        mlClean->show();
        QApplication::processEvents();

        // Activate ALL 4 interactive chrome sources:
        // (a) Curve selection halo (#2563EB)
        gClean->plotWidget()->setHighlightedCurveKey(gClean->curveKey(0));
        // (b) Axis title selection (used to mutate QwtText::setBackgroundPen(Qt::blue)!)
        QwtScaleWidget *cleanBottomSw = gClean->plotWidget()->axisWidget(QwtPlot::xBottom);
        if (cleanBottomSw)
            gClean->scalePicker->selectTitle(cleanBottomSw, true);
        // (c) Editable ArrowMarker (used to draw 10x10 black endpoint squares in print/EMF!)
        ArrowMarker *cleanArr = new ArrowMarker();
        cleanArr->setStartPoint(3.0, 25.0);
        cleanArr->setEndPoint(12.0, 55.0);
        cleanArr->setColor(Qt::black);
        cleanArr->setWidth(1);
        gClean->addArrow(cleanArr);
        cleanArr->setEditable(true);
        // (d) RoiGadgetTool (used to paint yellow/blue ROI box & HUD into exports!)
        RoiGadgetTool *cleanRoi = RoiGadgetTool::attachGadget(w47, gClean, RoiGadgetTool::RegionStats);
        CHECK_COND(cleanRoi != nullptr, "Failed to attach RoiGadgetTool on gClean");
        cleanRoi->setRoiRect(QRectF(4.0, 15.0, 10.0, 45.0));
        gClean->replot();
        QApplication::processEvents();

        auto countChromaticLeakPixels = [](const QImage &img) -> int {
            int leaks = 0;
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const QRgb p = img.pixel(x, y);
                    const int r = qRed(p), g = qGreen(p), b = qBlue(p);
                    // Pure black-on-white graph has r == g == b everywhere; any chromatic pixel
                    // (blue #2563EB halo/border, Qt::blue frame, or yellow ROI shading) has max-min >= 35!
                    const int chroma = std::max({ r, g, b }) - std::min({ r, g, b });
                    if (chroma >= 35)
                        ++leaks;
                }
            }
            return leaks;
        };

        // Check 300-DPI PNG export
        const QString cleanPngPath = tmpDir.filePath("suite47_zero_chrome.png");
        mlClean->exportImage(cleanPngPath, -1, false, 300);
        QImage exportedPng(cleanPngPath);
        CHECK_COND(!exportedPng.isNull() && exportedPng.width() > 600,
                   "Failed to export 300-DPI suite47_zero_chrome.png");
        const int pngChromaLeaks = countChromaticLeakPixels(exportedPng);
        std::cout << "[INFO] Suite 47 300-DPI PNG chromatic leak pixels = " << pngChromaLeaks << "\n";
        CHECK_COND(pngChromaLeaks == 0,
                   "Selection halo, blue title border, or ROI gadget leaked into exported PNG!");

        // Check PowerPoint OLE EMF+ renderMultiLayer playback
        OleRenderedGraph cleanOle = SciDAVisEmf::renderMultiLayer(mlClean);
        HENHMETAFILE hCleanEmf = SciDAVisEmf::createHemfFromBytes(cleanOle.emfBytes);
        CHECK_COND(hCleanEmf != nullptr, "createHemfFromBytes failed on mlClean");

        const int playSide = 900;
        QImage olePlayImg(playSide, playSide, QImage::Format_RGB32);
        olePlayImg.fill(Qt::white);
        {
            HDC sdc = GetDC(nullptr);
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = playSide;
            bmi.bmiHeader.biHeight = -playSide;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            void *bits = nullptr;
            HBITMAP hbm = CreateDIBSection(sdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
            HDC mdc = CreateCompatibleDC(sdc);
            HGDIOBJ old = SelectObject(mdc, hbm);
            if (bits) {
                auto *px = static_cast<quint32 *>(bits);
                for (int i = 0; i < playSide * playSide; ++i)
                    px[i] = 0xFFFFFFFFu;
                Gdiplus::Metafile mf(hCleanEmf, FALSE);
                Gdiplus::Graphics gfx(mdc);
                gfx.DrawImage(&mf, 0, 0, playSide, playSide);
                GdiFlush();
                std::memcpy(olePlayImg.bits(), bits, playSide * playSide * 4);
            }
            SelectObject(mdc, old);
            DeleteObject(hbm);
            DeleteDC(mdc);
            ReleaseDC(nullptr, sdc);
        }
        DeleteEnhMetaFile(hCleanEmf);

        const int oleChromaLeaks = countChromaticLeakPixels(olePlayImg);
        std::cout << "[INFO] Suite 47 OLE EMF+ playback chromatic leak pixels = " << oleChromaLeaks << "\n";
        CHECK_COND(oleChromaLeaks == 0,
                   "Selection halo, blue title border, or ROI gadget leaked into PowerPoint OLE EMF+!");

        RoiGadgetTool::clearGadget(gClean);
        cleanArr->setEditable(false);

        // Round 1 Regression A: RoiGadgetTool::keepFittedCurveOnGraph() must survive MultiLayer Undo/Redo
        const int curvesBeforeKeepFit = gClean->curves();
        RoiGadgetTool *fitRoi = RoiGadgetTool::attachGadget(w47, gClean, RoiGadgetTool::QuickFit);
        CHECK_COND(fitRoi != nullptr, "Failed to attach QuickFit RoiGadgetTool");
        fitRoi->setRoiRect(QRectF(2.0, 10.0, 14.0, 60.0));
        fitRoi->keepFittedCurveOnGraph();
        RoiGadgetTool::clearGadget(gClean);
        QApplication::processEvents();
        gClean = mlClean->activeGraph();
        CHECK_COND(gClean->curves() == curvesBeforeKeepFit + 1,
                   "keepFittedCurveOnGraph() did not add fitted DataCurve!");
        // Perform an unrelated undoable edit on mlClean, then Undo it -> kept fit curve MUST survive!
        const QString snapBeforeTitleEdit = mlClean->captureUndoSnapshot();
        gClean->setXAxisTitle("Temporary Strain Label");
        mlClean->pushUndoSnapshot("Edit X Title", snapBeforeTitleEdit);
        w47->activateSubWindow(mlClean);
        w47->undo();
        QApplication::processEvents();
        gClean = mlClean->activeGraph();
        CHECK_COND(gClean != nullptr && gClean->curves() == curvesBeforeKeepFit + 1
                           && gClean->curve(gClean->curves() - 1)->dataSize() >= 2,
                   "Kept ROI fitted curve was wiped out when MultiLayer Undo restored snapshot!");

        // Round 1 Regression B: Reciprocal Arrhenius FormulaLinkedScaleEngine on Top-X
        gClean->setScale(QwtPlot::xBottom, 2.0, 3.5);
        Graph *childTopX = mlClean->addLinkedLayer(gClean, true, false, "(1000/x)-273.15");
        CHECK_COND(childTopX != nullptr && mlClean->activeGraph() == gClean,
                   "addLinkedLayer failed or stole activeGraph() away from parent data layer!");
        gClean->replot();
        childTopX->replot();
        QApplication::processEvents();
        const double topLower = childTopX->plotWidget()->axisScaleDiv(QwtPlot::xTop)->lowerBound();
        const double topUpper = childTopX->plotWidget()->axisScaleDiv(QwtPlot::xTop)->upperBound();
        CHECK_COND(topLower > 200.0 && topUpper < 30.0 && topLower > topUpper,
                   "Arrhenius linked Top-X (1000/x)-273.15 failed to invert bounds (left high T -> right low T)!");

        // Round 2/2.5 Regression C: <ToriAxisLocks> preservation across MultiLayer Undo/Redo +
        // safe ScalePicker::cancelInlineScaleEditor() when Undo fires mid-inline-edit!
        gClean->setAxisUserLocked(QwtPlot::xBottom, true);
        CHECK_COND(gClean->isAxisUserLocked(QwtPlot::xBottom), "setAxisUserLocked(xBottom, true) failed");
        const QString snapWithAxisLock = mlClean->captureUndoSnapshot();
        gClean->setYAxisTitle("Temp Stress Label");
        mlClean->pushUndoSnapshot("Edit Y Title After Lock", snapWithAxisLock);

        // Open an inline scale editor on gClean xBottom with unconfirmed text "9999", then Undo!
        QwtScaleWidget *cleanSwX = gClean->plotWidget()->axisWidget(QwtPlot::xBottom);
        if (cleanSwX && gClean->scalePicker) {
            const QRect cleanTicksRc = gClean->scalePicker->scaleRect(cleanSwX);
            const QPoint cleanMaxPt(cleanTicksRc.right() - 6, cleanTicksRc.center().y());
            QMouseEvent dblClean(QEvent::MouseButtonDblClick, cleanMaxPt,
                                 cleanSwX->mapToGlobal(cleanMaxPt), Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(cleanSwX, &dblClean);
            QApplication::processEvents();
            if (QLineEdit *inEdClean = cleanSwX->findChild<QLineEdit *>())
                inEdClean->setText("9999");
        }
        w47->activateSubWindow(mlClean);
        w47->undo();
        QApplication::processEvents();
        gClean = mlClean->activeGraph();
        CHECK_COND(gClean != nullptr && gClean->isAxisUserLocked(QwtPlot::xBottom),
                   "<ToriAxisLocks> lost user axis lock across MultiLayer Undo!");
        CHECK_COND(std::fabs(gClean->plotWidget()->axisScaleDiv(QwtPlot::xBottom)->upperBound() - 3.5) < 1e-4,
                   "Mid-edit Undo committed unconfirmed inline scale text '9999' instead of cancelling cleanly!");

        // Round 2/2.5 Regression D: [関数] badge in JA mode + Japanese LinearFit naming without ASCII space
        QList<double> fnRange;
        fnRange << 2.0 << 3.5;
        CHECK_COND(gClean->addFunctionCurve(w47, FunctionCurve::Normal,
                                            QStringList() << "20.0*x", "x", fnRange, 50, "RefFunc"),
                   "Failed to add FunctionCurve RefFunc to gClean");
        w47->switchToLanguage("ja");
        QApplication::processEvents();

        bool sawFuncBadgeJa = false;
        bool sawRawFuncBadgeEn = false;
        if (auto *insp47 = w47->findChild<GraphInspectorDock *>()) {
            insp47->setTargetMultiLayer(mlClean, true);
            insp47->refreshAll();
            QApplication::processEvents();
            if (QTreeWidget *trw = insp47->objectTree()) {
                std::function<void(QTreeWidgetItem *)> scanBadge = [&](QTreeWidgetItem *it) {
                    if (!it)
                        return;
                    const QString txt = it->text(0);
                    if (txt.contains(QString::fromUtf8("[関数]")))
                        sawFuncBadgeJa = true;
                    if (txt.contains("[Func]"))
                        sawRawFuncBadgeEn = true;
                    for (int ci = 0; ci < it->childCount(); ++ci)
                        scanBadge(it->child(ci));
                };
                for (int ti = 0; ti < trw->topLevelItemCount(); ++ti)
                    scanBadge(trw->topLevelItem(ti));
            }
        }
        CHECK_COND(!sawRawFuncBadgeEn, "Untranslated '[Func]' badge leaked in Japanese mode!");
        CHECK_COND(sawFuncBadgeJa, "GraphInspectorDock missing Japanese '[関数]' badge on FunctionCurve!");

        LinearFit *jaLinFit = new LinearFit(w47, gClean, "CleanExportTbl_2");
        jaLinFit->fit();
        delete jaLinFit;
        QApplication::processEvents();
        bool sawCleanJaFitName = false;
        for (int ci = 0; ci < gClean->curves(); ++ci) {
            const QString ct = gClean->curve(ci)->title().text();
            CHECK_COND(!ct.contains(QString::fromUtf8("線形 フィット")),
                       "LinearFit inserted unwanted ASCII space in Japanese '線形 フィット'!");
            if (ct.contains(QString::fromUtf8("線形フィット")))
                sawCleanJaFitName = true;
        }
        CHECK_COND(sawCleanJaFitName, "LinearFit in Japanese mode did not produce '線形フィット' curve!");

        w47->switchToLanguage("en");
        QApplication::processEvents();

        // Round 2 Regression E: PlotDialog switching between curves must NOT corrupt QComboBox item labels
        {
            PlotDialog pdComboCheck(true, w47);
            pdComboCheck.initFonts(QFont("Arial", 14), QFont("Arial", 14), QFont("Arial", 12),
                                   QFont("Arial", 12));
            pdComboCheck.setMultiLayer(ml47);
            pdComboCheck.selectCurve(0);
            QApplication::processEvents();
            pdComboCheck.selectCurve(1);
            QApplication::processEvents();
            pdComboCheck.selectCurve(0);
            QApplication::processEvents();
            for (QComboBox *cmb : pdComboCheck.findChildren<QComboBox *>()) {
                QSet<QString> seenItems;
                int dupColItems = 0;
                for (int i = 0; i < cmb->count(); ++i) {
                    const QString itemTxt = cmb->itemText(i);
                    if (itemTxt.startsWith("Al_Zn_Mg_1_")) {
                        if (seenItems.contains(itemTxt))
                            ++dupColItems;
                        seenItems.insert(itemTxt);
                    }
                }
                CHECK_COND(dupColItems == 0,
                           "PlotDialog::setActiveCurve corrupted QComboBox items with duplicate column names!");
            }
        }

        // Round 3 Regression F: Graph::setCurveType polymorphic bar conversion on DataCurve fitted curves
        // must NOT crash out-of-bounds on size()==1 or overwrite slot [1] on size()==2
        gClean->deleteFitCurves();
        CHECK_COND(gClean->fitCurvesList().isEmpty(),
                   "deleteFitCurves() failed to clear initial FunctionCurve fit on gClean!");

        LinearFit *dataFit0 = new LinearFit(w47, gClean, "CleanExportTbl_2");
        dataFit0->generateFunction(false, 20);
        dataFit0->fit();
        delete dataFit0;
        QApplication::processEvents();
        CHECK_COND(gClean->fitCurvesList().size() == 1,
                   "Expected exactly 1 DataCurve fitted curve on gClean!");
        const int fitCurveIdx0 = gClean->curveIndex(gClean->fitCurvesList().at(0));
        CHECK_COND(fitCurveIdx0 >= 0, "Failed to resolve fitted DataCurve index on gClean!");
        gClean->setCurveType(fitCurveIdx0, Graph::VerticalBars, true);
        CHECK_COND(gClean->curveType(fitCurveIdx0) == Graph::VerticalBars
                           && gClean->fitCurvesList().size() == 1
                           && gClean->fitCurvesList().at(0) == gClean->curve(fitCurveIdx0),
                   "Graph::setCurveType(VerticalBars) corrupted d_fit_curves[0]!");
        gClean->setCurveType(fitCurveIdx0, Graph::Line, true);
        CHECK_COND(gClean->curveType(fitCurveIdx0) == Graph::Line
                           && gClean->fitCurvesList().size() == 1
                           && gClean->fitCurvesList().at(0) == gClean->curve(fitCurveIdx0),
                   "Graph::setCurveType(Line) corrupted d_fit_curves[0]!");

        LinearFit *dataFit1 = new LinearFit(w47, gClean, "CleanExportTbl_2");
        dataFit1->generateFunction(false, 20);
        dataFit1->fit();
        delete dataFit1;
        QApplication::processEvents();
        CHECK_COND(gClean->fitCurvesList().size() == 2,
                   "Expected 2 DataCurve fitted curves on gClean for two-fit precedence test!");
        QwtPlotCurve *preservedFit1 = gClean->fitCurvesList().at(1);
        gClean->setCurveType(fitCurveIdx0, Graph::VerticalBars, true);
        CHECK_COND(gClean->fitCurvesList().at(0) == gClean->curve(fitCurveIdx0)
                           && gClean->fitCurvesList().at(1) == preservedFit1,
                   "Graph::setCurveType overwrote d_fit_curves[1] instead of replacing [0]!");
        gClean->setCurveType(fitCurveIdx0, Graph::Line, true);
        CHECK_COND(gClean->fitCurvesList().at(0) == gClean->curve(fitCurveIdx0)
                           && gClean->fitCurvesList().at(1) == preservedFit1,
                   "Graph::setCurveType restore overwrote d_fit_curves[1]!");

        // Round 3 Regression G: invalidateModelessGraphDialogs(ml, excludeDlg) keeps excludeDlg
        // alive while invalidating sibling CurveRangeDialog on polymorphic style switch
        QPointer<CurveRangeDialog> crdGuard = new CurveRangeDialog(w47, Qt::Window);
        crdGuard->setAttribute(Qt::WA_DeleteOnClose);
        crdGuard->setProperty("targetMultiLayerPtr",
                              QVariant::fromValue(static_cast<QObject *>(ml47)));
        crdGuard->setCurveToModify(g47, 0);
        crdGuard->show();
        QPointer<PlotDialog> pdKeepGuard = new PlotDialog(true, w47, Qt::Window);
        pdKeepGuard->setAttribute(Qt::WA_DeleteOnClose);
        pdKeepGuard->setProperty("targetMultiLayerPtr",
                                 QVariant::fromValue(static_cast<QObject *>(ml47)));
        pdKeepGuard->initFonts(QFont("Arial", 14), QFont("Arial", 14), QFont("Arial", 12),
                               QFont("Arial", 12));
        pdKeepGuard->setMultiLayer(ml47);
        pdKeepGuard->selectCurve(0);
        pdKeepGuard->show();
        QApplication::processEvents();

        w47->invalidateModelessGraphDialogs(ml47, pdKeepGuard.data());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();
        CHECK_COND(crdGuard.isNull(),
                   "Sibling CurveRangeDialog was not invalidated by invalidateModelessGraphDialogs!");
        CHECK_COND(!pdKeepGuard.isNull() && pdKeepGuard->isVisible(),
                   "Excluded PlotDialog was wrongly destroyed by invalidateModelessGraphDialogs!");
        delete pdKeepGuard.data();
    }

    w47->savedProject();
    w47->setOleEmbeddedMode(true);
    delete w47;
    QApplication::processEvents();

    std::cout << "[INFO] Suite 47 Phase 7 Graph Usability & Zero-Leak Gate passed!\n";
    return 0;
}

#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>
#include <QToolBar>

int runSuite48Phase8RibbonDockWorksheetAndGraphErgonomicsGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 48: Phase 8 Full-Width Ribbon Dock Geometry, Zero-Clipped Group Captions, Worksheet Quick Bar & Paste Header Promotion, Catch-22 Metadata Legend, Zoom History, Safe Format Paste & Full-Page Multi-Panel OLE Gate...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 48");

    ApplicationWindow *w48 = new ApplicationWindow();
    w48->applyUserSettings();
    w48->resize(1380, 860);
    w48->show();
    w48->switchToLanguage("ja");
    w48->explorerWindow.setFloating(false);
    w48->addDockWidget(Qt::LeftDockWidgetArea, &w48->explorerWindow);
    w48->explorerWindow.show();
    for (int pass = 0; pass < 4; ++pass) {
        QCoreApplication::sendPostedEvents();
        QApplication::processEvents();
    }

    // 1. Dock Layout Below Full-Width Top Ribbon + Zero Clipped Group Captions Across All 7 Tabs
    ModernRibbonBar *rib48 = w48->modernRibbonBar();
    CHECK_COND(rib48 != nullptr && rib48->isVisible(), "ModernRibbonBar is null or hidden in Suite 48!");
    QToolBar *ribTb = qobject_cast<QToolBar *>(rib48->parentWidget());
    CHECK_COND(ribTb != nullptr && ribTb->isVisible(),
               "ModernRibbonBar must be hosted inside full-width top QToolBar so docks sit below it!");

    const int ribbonBotY = ribTb->mapTo(w48, QPoint(0, ribTb->height())).y();
    const int explorerTopY = w48->explorerWindow.mapTo(w48, QPoint(0, 0)).y();
    CHECK_COND(explorerTopY >= ribbonBotY - 1,
               "explorerWindow dock intruded vertically into ModernRibbonBar row!");

    GraphInspectorDock *insp48 = w48->graphInspectorDock();
    if (insp48 && insp48->isVisible()) {
        const int inspTopY = insp48->mapTo(w48, QPoint(0, 0)).y();
        CHECK_COND(inspTopY >= ribbonBotY - 1,
                   "GraphInspectorDock intruded vertically into ModernRibbonBar row!");
    }

    // Verify across all 7 Ribbon tabs at 1380px, 1280px, and narrow 1040px:
    // (a) ZERO visible RibbonGroupCaption labels exist in ModernRibbonBar,
    // (b) ALL visible QToolButtons have zero vertical clipping (height >= fontMetrics.height & inside viewport),
    // (c) Tab 2 ("Plot 2D/3D" / "2D/3Dプロット") has accessible "Linked Layer" and "Zoom Inset" buttons
    //     with zero horizontal overflow clipping at 1380px,
    // (d) At 1280px width, Tabs 1, 2, 4, and 6 (indices 0, 1, 3, 5) have ZERO horizontal scroll overflow.
    for (int testW : { 1380, 1280, 1040 }) {
        w48->resize(testW, 820);
        for (int ti = 0; ti < rib48->tabCount(); ++ti) {
            rib48->setCurrentTabIndex(ti);
            for (int pass = 0; pass < 3; ++pass) {
                QCoreApplication::sendPostedEvents();
                QApplication::processEvents();
            }
            if (testW == 1280 && (ti == 0 || ti == 1 || ti == 3 || ti == 5)) {
                for (QScrollArea *sa : rib48->findChildren<QScrollArea *>()) {
                    if (!sa || !sa->isVisible())
                        continue;
                    if (QScrollBar *hb = sa->horizontalScrollBar()) {
                        CHECK_COND(hb->maximum() <= hb->minimum(),
                                   QString("Suite 48(d): Ribbon Tab %1 overflows horizontally at 1280px (hb max=%2 > min=%3)!")
                                           .arg(ti + 1)
                                           .arg(hb->maximum())
                                           .arg(hb->minimum())
                                           .toStdString()
                                           .c_str());
                    }
                }
            }
            for (QLabel *lbl : rib48->findChildren<QLabel *>()) {
                if (!lbl || !lbl->isVisible())
                    continue;
                CHECK_COND(lbl->property("class").toString() != QStringLiteral("RibbonGroupCaption"),
                           "Suite 48(a): Visible RibbonGroupCaption found in ModernRibbonBar (must be removed)!");
            }

            bool foundLinkedLayer = false;
            bool foundZoomInset = false;
            for (QToolButton *btn : rib48->findChildren<QToolButton *>()) {
                if (!btn || !btn->isVisible())
                    continue;
                if (qobject_cast<QTabBar *>(btn->parentWidget())
                    || qobject_cast<QScrollArea *>(btn->parentWidget()))
                    continue; // ignore Qt internal QTabBar & RibbonHorizontalScrollArea scroll chevrons
                CHECK_COND(btn->height() + 1 >= btn->fontMetrics().height(),
                           QString("Suite 48(b): Ribbon QToolButton '%1' vertically clipped (h=%2 < fm=%3) at W=%4 tab=%5!")
                                   .arg(btn->text())
                                   .arg(btn->height())
                                   .arg(btn->fontMetrics().height())
                                   .arg(testW)
                                   .arg(ti)
                                   .toStdString()
                                   .c_str());

                QScrollArea *saAnc = nullptr;
                for (QWidget *anc = btn->parentWidget(); anc; anc = anc->parentWidget()) {
                    if (auto *sa = qobject_cast<QScrollArea *>(anc)) {
                        saAnc = sa;
                        break;
                    }
                }
                if (saAnc && saAnc->viewport() && saAnc->viewport()->isAncestorOf(btn)) {
                    const int botVp = btn->mapTo(saAnc->viewport(), QPoint(0, btn->height())).y();
                    CHECK_COND(botVp <= saAnc->viewport()->height() + 1,
                               QString("Suite 48(b): Ribbon QToolButton '%1' clipped vertically by QScrollArea viewport (%2 > %3) at W=%4 tab=%5!")
                                       .arg(btn->text())
                                       .arg(botVp)
                                       .arg(saAnc->viewport()->height())
                                       .arg(testW)
                                       .arg(ti)
                                       .toStdString()
                                       .c_str());
                }

                if (testW == 1380 && ti == 1) {
                    const QString txt = btn->text();
                    const bool isLinked = txt.contains(QStringLiteral("Linked Layer"), Qt::CaseInsensitive)
                            || txt.contains(QString::fromUtf8("\xE9\x80\xA3\xE5\x8B\x95\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC"));
                    const bool isInset = txt.contains(QStringLiteral("Zoom Inset"), Qt::CaseInsensitive)
                            || txt.contains(QString::fromUtf8("\xE6\x8B\xA1\xE5\xA4\xA7\xE3\x82\xA4\xE3\x83\xB3\xE3\x82\xBB\xE3\x83\x83\xE3\x83\x88"));
                    if (isLinked || isInset) {
                        if (saAnc && saAnc->viewport() && saAnc->viewport()->isAncestorOf(btn)) {
                            const int leftVp = btn->mapTo(saAnc->viewport(), QPoint(0, 0)).x();
                            const int rightVp = btn->mapTo(saAnc->viewport(), QPoint(btn->width(), 0)).x();
                            CHECK_COND(leftVp >= -1 && rightVp <= saAnc->viewport()->width() + 1,
                                       QString("Suite 48(c): Tab 2 button '%1' clipped horizontally at 1380px (left=%2, right=%3, vpW=%4)!")
                                               .arg(txt)
                                               .arg(leftVp)
                                               .arg(rightVp)
                                               .arg(saAnc->viewport()->width())
                                               .toStdString()
                                               .c_str());
                        }
                        if (isLinked)
                            foundLinkedLayer = true;
                        if (isInset)
                            foundZoomInset = true;
                    }
                }
            }
            if (testW == 1380 && ti == 1) {
                CHECK_COND(foundLinkedLayer && foundZoomInset,
                           "Suite 48(c): Tab 2 ('Plot 2D/3D') missing visible 'Linked Layer' and/or 'Zoom Inset' QToolButton at 1380px!");
            }
        }
    }
    w48->resize(1380, 860);
    QApplication::processEvents();

    // 2. TableView Default Collapsed Side Panel + Worksheet Quick Bar + Smart Paste Header Promotion
    Table *t48 = w48->newTable(6, 3, "Tensile_Sheet", "Al-Zn-Mg Tensile");
    CHECK_COND(t48 != nullptr, "Failed to create Tensile_Sheet in Suite 48");
    t48->show();
    QApplication::processEvents();
    CHECK_COND(!t48->isControlTabBarVisible(),
               "TableView::isControlTabBarVisible() must default to collapsed (false) on new tables!");
    CHECK_COND(t48->findChild<QWidget *>("WorksheetQuickBar") != nullptr,
               "TableView missing #WorksheetQuickBar top toolbar!");

    // Smart paste with Long Name + Units rows. The text goes straight to pasteText() (what
    // pasteSelection() does with the clipboard text), so another program using the clipboard
    // cannot change what is pasted; Suite 59a covers the clipboard path.
    CHECK_COND(t48->pasteText("Nominal strain\tNominal stress\tNominal stress\n"
                              "%\tMPa\tMPa\n"
                              "0.0\t0.0\t0.0\n"
                              "2.0\t480.0\t472.0\n"
                              "4.0\t518.0\t505.0\n"),
               "Suite 48: Table::pasteText failed");
    QApplication::processEvents();
    CHECK_COND(t48->column(0)->longName().contains("Nominal strain")
                       && t48->column(0)->units() == "%"
                       && std::fabs(t48->cell(1, 1) - 480.0) < 1e-6,
               "Table::pasteText failed to promote header/unit rows instead of inserting NaNs!");

    // 3. Catch-22 Metadata Resolution: Shared Long Name -> Y-Axis Title; Distinct Comments -> Legend
    t48->setColPlotDesignation(0, SciDAVis::X);
    t48->setColPlotDesignation(1, SciDAVis::Y);
    t48->setColPlotDesignation(2, SciDAVis::Y);
    t48->column(1)->setComment("Uncharged");
    t48->column(2)->setComment("H-charged");

    MultiLayer *ml48 = w48->multilayerPlot(
            t48, QStringList() << "Tensile_Sheet_2" << "Tensile_Sheet_3", Graph::LineSymbols);
    CHECK_COND(ml48 != nullptr && ml48->activeGraph() != nullptr, "ml48 null in Suite 48");
    Graph *g48 = ml48->activeGraph();
    CHECK_COND(g48->axisTitle(QwtPlot::yLeft).contains("Nominal stress")
                       && !g48->axisTitle(QwtPlot::yLeft).contains("Uncharged"),
               "Auto Y-axis title failed to use shared Long Name cleanly!");
    CHECK_COND(g48->legend() != nullptr
                       && g48->legend()->text().contains("Uncharged")
                       && g48->legend()->text().contains("H-charged"),
               "Multi-curve Legend failed to disambiguate identical Long Names via Column Comments!");

    // 4. Zoom History (Alt+Z / Shift+Alt+Z) & Right-Y Axis Color Sync
    g48->setScale(QwtPlot::xBottom, 0.0, 5.0, 1.0);
    g48->recordZoomHistorySnapshot();
    g48->setScale(QwtPlot::xBottom, 1.0, 3.0, 0.5);
    g48->recordZoomHistorySnapshot();
    CHECK_COND(g48->stepZoomHistory(-1), "Graph::stepZoomHistory(-1) returned false!");
    const QwtScaleDiv *divAfterUndo = g48->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
    CHECK_COND(divAfterUndo != nullptr && std::fabs(divAfterUndo->upperBound() - 5.0) < 1e-6,
               "stepZoomHistory(-1) did not restore prior X scale [0..5]!");
    CHECK_COND(g48->stepZoomHistory(+1), "Graph::stepZoomHistory(+1) returned false!");

    g48->setCurvePen(1, QPen(QColor("#DC2626"), 2));
    g48->syncAxisColorToCurve(QwtPlot::yRight, 1);
    CHECK_COND(g48->axisColor(QwtPlot::yRight).name().compare("#dc2626", Qt::CaseInsensitive) == 0,
               "Graph::syncAxisColorToCurve failed to color Right-Y axis!");

    // 5. Safe Copy/Paste Format (Zero Foreign Axis-Break / Reference-Line Leak)
    Graph::AxisBreakSpec brk;
    brk.enabled = true;
    brk.breakFrom = 1.5;
    brk.breakTo = 2.5;
    brk.positionPct = 0.5;
    g48->setAxisBreak(QwtPlot::xBottom, brk);
    Graph::ReferenceLineSpec rl;
    rl.orientation = 0;
    rl.value = 250.0;
    rl.label = "Yield";
    g48->addReferenceLine(rl);
    ml48->applyStylePreset(MultiLayer::StylePresentationSlide28pt);

    w48->activateSubWindow(ml48);
    w48->copyGraphFormat();

    MultiLayer *mlDst = w48->multilayerPlot(t48, QStringList() << "Tensile_Sheet_2", Graph::Line);
    CHECK_COND(mlDst != nullptr && mlDst->activeGraph() != nullptr, "mlDst null in Suite 48");
    w48->activateSubWindow(mlDst);
    w48->pasteGraphFormat(); // FormatDefaultSafe
    Graph *gDst = mlDst->activeGraph();
    CHECK_COND(gDst->axisTitleFont(QwtPlot::xBottom).pointSize() == 28,
               "pasteGraphFormat failed to copy 28pt Slide font size!");
    CHECK_COND(!gDst->axisBreak(QwtPlot::xBottom).enabled,
               "Default pasteGraphFormat leaked foreign Axis Break onto target graph!");
    CHECK_COND(gDst->referenceLines().isEmpty(),
               "Default pasteGraphFormat leaked foreign Reference Lines onto target graph!");

    // 6. Full-Page Multi-Panel OLE EMF+ Rendering (ml, nullptr) Preserves All Panels & Labels
    Graph *gSecond = mlDst->addLayer(0, 0, 200, 200);
    CHECK_COND(gSecond != nullptr, "Failed to add second panel to mlDst");
    gSecond->insertCurve(t48, "Tensile_Sheet_1", "Tensile_Sheet_3", Graph::Line);
    mlDst->setRows(1);
    mlDst->setCols(2);
    mlDst->setPageSizePhysical(175.0, 85.0, MultiLayer::UnitMm, 300);
    mlDst->setPageViewMode(true);
    mlDst->fitLayersToPage();
    mlDst->setPanelLabels(true, MultiLayer::PanelOutsideTopLeft, "(%a)", 12);

    const OleRenderedGraph oleFull = SciDAVisEmf::renderMultiLayer(mlDst, nullptr);
    const QByteArray &emfFull = oleFull.emfBytes;
    CHECK_COND(emfFull.size() > 1024,
               "Full-page multi-panel SciDAVisEmf::renderMultiLayer(mlDst, nullptr) produced empty EMF+!");
    HENHMETAFILE hemf = SetEnhMetaFileBits(static_cast<UINT>(emfFull.size()),
                                           reinterpret_cast<const BYTE *>(emfFull.constData()));
    CHECK_COND(hemf != nullptr, "SetEnhMetaFileBits failed on multi-panel full-page EMF+!");
    ENHMETAHEADER hdr{};
    hdr.nSize = sizeof(hdr);
    CHECK_COND(GetEnhMetaFileHeader(hemf, sizeof(hdr), &hdr) > 0,
               "GetEnhMetaFileHeader failed on multi-panel EMF+!");
    const double frameAspect = double(hdr.rclFrame.right - hdr.rclFrame.left)
            / std::max(1.0, double(hdr.rclFrame.bottom - hdr.rclFrame.top));
    DeleteEnhMetaFile(hemf);
    CHECK_COND(frameAspect > 1.5,
               "Full-page 175x85 mm 1x2 multi-panel EMF+ lost wide 2-panel aspect ratio!");

    w48->savedProject();
    w48->setOleEmbeddedMode(true);
    delete w48;
    QApplication::processEvents();

    std::cout << "[INFO] Suite 48 Phase 8 Ribbon/Dock/Worksheet/Graph Ergonomics Gate passed!\n";
    return 0;
}

#include "ScaleDraw.h"
#include "future/table/AsciiTableImportFilter.h"
#include "future/table/future_Table.h"
#include <QFileInfo>
#include <QTextCodec>
#include "ScriptEdit.h"

int runSuite49PhaseBUxRenderMatSciAndApiGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 49: Phase B 600-DPI Curve Thickness & Lossless PNG/PDF Export, Lab CSV/Shift-JIS/Instron/Rigaku .ras Importer, Tensile 0.2% Proof Stress & Non-Bold Arial Arrhenius, Nonlinear 1/(x*x) Linked Scale & Comma-Free Ticks, Dual EMF+ Translucent Pill & .sciprj/Snapshot Legend+Tab Gate...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 49");

    QTemporaryDir tmpDir;
    CHECK_COND(tmpDir.isValid(), "Failed to create QTemporaryDir in Suite 49");

    ApplicationWindow *w49 = new ApplicationWindow();
    w49->applyUserSettings();
    w49->resize(1280, 820);
    w49->show();
    QApplication::processEvents();

    // -------------------------------------------------------------------------
    // 49a & 49b. 600-DPI MultiLayer::exportImage curve line thickness (> 5 px,
    //            NOT 2px hairline) & lossless PNG (< 2 MB for 2008x2008), plus
    //            MultiLayer::exportVector .pdf at res=600 (no double-scaling)
    // -------------------------------------------------------------------------
    {
        Table *tExp = w49->newTable(21, 2, "Suite49ExportTbl", "600DPI Check");
        CHECK_COND(tExp != nullptr, "Failed to create Suite49ExportTbl");
        for (int i = 0; i <= 20; ++i) {
            const double x = 0.5 * i; // 0 .. 10
            tExp->setCell(i, 0, x);
            tExp->setCell(i, 1, 5.0); // Horizontal line at Y = 5.0
        }
        MultiLayer *mlExp = w49->multilayerPlot(tExp, QStringList() << "Suite49ExportTbl_2", Graph::Line);
        CHECK_COND(mlExp != nullptr && mlExp->activeGraph() != nullptr, "mlExp null in Suite 49");
        mlExp->setPageSizePhysical(85.0, 85.0, MultiLayer::UnitMm, 300);
        mlExp->setPageViewMode(true);
        mlExp->fitLayersToPage();
        Graph *gExp = mlExp->activeGraph();
        gExp->setScale(QwtPlot::xBottom, 0.0, 10.0, 2.0);
        gExp->setScale(QwtPlot::yLeft, 0.0, 10.0, 2.0);
        gExp->setCurvePen(0, QPen(QColor(0, 80, 220), 2));
        gExp->replot();
        QApplication::processEvents();

        const QString png600Path = tmpDir.filePath("suite49_600dpi_isij85mm.png");
        mlExp->exportImage(png600Path, 100, false, 600);
        QFileInfo pngFi(png600Path);
        CHECK_COND(pngFi.exists() && pngFi.size() > 4096,
                   "Suite 49a: 600-DPI PNG export missing or empty!");
        std::cout << "[INFO] Suite 49a: 2008x2008 600-DPI lossless PNG size = "
                  << pngFi.size() << " bytes\n";
        CHECK_COND(pngFi.size() < 2LL * 1024LL * 1024LL,
                   "Suite 49a: 600-DPI PNG exceeded 2 MB (Z_NO_COMPRESSION bloat regression)!");

        QImage img600(png600Path);
        CHECK_COND(!img600.isNull() && std::abs(img600.width() - 2008) <= 2
                           && std::abs(img600.height() - 2008) <= 2,
                   "Suite 49a: 85x85 mm @ 600 DPI did not produce ~2008x2008 PNG!");

        QVector<int> colStrokes;
        const int xStart = int(img600.width() * 0.40);
        const int xEnd = int(img600.width() * 0.60);
        const int yStart = int(img600.height() * 0.20);
        const int yEnd = int(img600.height() * 0.80);
        for (int x = xStart; x <= xEnd; x += 4) {
            int bluePx = 0;
            for (int y = yStart; y <= yEnd; ++y) {
                const QRgb c = img600.pixel(x, y);
                if (qBlue(c) - qRed(c) >= 60)
                    ++bluePx;
            }
            if (bluePx > 0)
                colStrokes.push_back(bluePx);
        }
        CHECK_COND(!colStrokes.isEmpty(), "Suite 49a: failed to locate blue curve in 600-DPI PNG!");
        std::sort(colStrokes.begin(), colStrokes.end());
        const int medianStrokePx = colStrokes[colStrokes.size() / 2];
        std::cout << "[INFO] Suite 49a: 600-DPI curve median stroke thickness = "
                  << medianStrokePx << " px\n";
        CHECK_COND(medianStrokePx > 5,
                   "Suite 49a: 600-DPI curve rendered as <=5 px cosmetic hairline instead of scaled pen!");

        // 49b. MultiLayer::exportVector .pdf at res=600
        const QString pdf600Path = tmpDir.filePath("suite49_res600_isij85mm.pdf");
        mlExp->exportVector(pdf600Path, 600, true, true);
        QFile pdfFile(pdf600Path);
        CHECK_COND(pdfFile.exists() && pdfFile.open(QIODevice::ReadOnly),
                   "Suite 49b: MultiLayer::exportVector(.pdf, res=600) failed to create PDF!");
        const QByteArray pdfBytes = pdfFile.readAll();
        pdfFile.close();
        std::cout << "[INFO] Suite 49b: res=600 vector PDF size = " << pdfBytes.size() << " bytes\n";
        CHECK_COND(pdfBytes.size() > 1500 && pdfBytes.startsWith("%PDF-"),
                   "Suite 49b: exported PDF is empty or lacks %PDF- header!");
        QRegularExpression mediaBoxRe(
                QStringLiteral("/MediaBox\\s*\\[\\s*0(?:\\.0+)?\\s+0(?:\\.0+)?\\s+([0-9.]+)\\s+([0-9.]+)\\s*\\]"));
        const QString pdfLatin1 = QString::fromLatin1(pdfBytes.constData(), pdfBytes.size());
        const QRegularExpressionMatch mbMatch = mediaBoxRe.match(pdfLatin1);
        CHECK_COND(mbMatch.hasMatch(), "Suite 49b: PDF missing /MediaBox!");
        const double mbW = mbMatch.captured(1).toDouble();
        const double mbH = mbMatch.captured(2).toDouble();
        // 85 mm = 240.945 pt; double-scaling blow-up would distort page size or coordinates
        CHECK_COND(mbW > 225.0 && mbW < 258.0 && mbH > 225.0 && mbH < 258.0,
                   "Suite 49b: PDF /MediaBox dimensions do not match 85x85 mm (~241x241 pt)!");
    }

    // -------------------------------------------------------------------------
    // 49c. AsciiTableImportFilter::splitHeaderNameAndUnits & importLabTextData
    //      (Shift-JIS CSV, 2-row [Name + Units] Instron CSV, Rigaku .ras)
    // -------------------------------------------------------------------------
    {
        QString shortN, longN, unitN;
        AsciiTableImportFilter::splitHeaderNameAndUnits(
                QString::fromUtf8("Nominal stress, \xCF\x83 (MPa)"), shortN, longN, unitN);
        CHECK_COND(longN == QString::fromUtf8("Nominal stress, \xCF\x83")
                           && unitN == QLatin1String("MPa")
                           && !shortN.isEmpty() && !shortN.contains(' ') && !shortN.contains('('),
                   "Suite 49c: splitHeaderNameAndUnits(\"Nominal stress, σ (MPa)\") failed!");

        // Descriptive parens like "(H-charged)" must NOT be split as units
        QString sDesc, lDesc, uDesc;
        AsciiTableImportFilter::splitHeaderNameAndUnits(
                QStringLiteral("Stress (H-charged)"), sDesc, lDesc, uDesc);
        CHECK_COND(uDesc.isEmpty() && lDesc == QLatin1String("Stress (H-charged)"),
                   "Suite 49c: splitHeaderNameAndUnits wrongly split '(H-charged)' as unit!");

        // Round 2: ASCII micro-prefix unit "(um)" must be recognized as scientific unit
        QString sUm, lUm, uUm;
        AsciiTableImportFilter::splitHeaderNameAndUnits(
                QStringLiteral("Displacement (um)"), sUm, lUm, uUm);
        CHECK_COND(lUm == QLatin1String("Displacement") && uUm == QLatin1String("um"),
                   "Suite 49c: splitHeaderNameAndUnits(\"Displacement (um)\") failed to extract units == \"um\"!");

        // Shift-JIS Shimadzu CSV
        QTextCodec *sjisCodec = QTextCodec::codecForName("Shift-JIS");
        CHECK_COND(sjisCodec != nullptr, "Shift-JIS QTextCodec unavailable");
        const QString sjisSrc = QString::fromUtf8(
                "# Shimadzu Autograph Tensile\r\n"
                "ひずみ (%),公称応力 (MPa)\r\n"
                "0.0,0.0\r\n"
                "0.2,485.5\r\n"
                "2.0,532.0\r\n");
        const QByteArray sjisBytes = sjisCodec->fromUnicode(sjisSrc);
        std::unique_ptr<future::Table> sjisTbl(AsciiTableImportFilter::importLabTextData(
                sjisBytes, "SjisTensile", "shimadzu_sjis.csv"));
        CHECK_COND(sjisTbl != nullptr && sjisTbl->columnCount() == 2 && sjisTbl->rowCount() == 3,
                   "Suite 49c: Shift-JIS CSV importLabTextData wrong dimensions!");
        CHECK_COND(sjisTbl->column(0)->longName() == QString::fromUtf8("ひずみ")
                           && sjisTbl->column(0)->units() == QLatin1String("%")
                           && sjisTbl->column(1)->longName() == QString::fromUtf8("公称応力")
                           && sjisTbl->column(1)->units() == QLatin1String("MPa")
                           && std::fabs(sjisTbl->column(1)->valueAt(1) - 485.5) < 1e-6,
                   "Suite 49c: Shift-JIS CSV metadata or cell values corrupted (Mojibake)!");

        // 2-row [Name + Units] Instron CSV with Text specimen column & missing '--' sentinel
        const QByteArray instronBytes =
                "# Instron Bluehill Export\n"
                "Specimen,Tensile strain,Tensile stress\n"
                ",(%),(MPa)\n"
                "SP-01,0.0,0.0\n"
                "SP-01,0.25,492.5\n"
                "SP-01,1.50,--\n";
        std::unique_ptr<future::Table> insTbl(AsciiTableImportFilter::importLabTextData(
                instronBytes, "InstronTensile", "instron_2row.csv"));
        CHECK_COND(insTbl != nullptr && insTbl->columnCount() == 3 && insTbl->rowCount() == 3,
                   "Suite 49c: 2-row Instron CSV failed auto unit-row promotion (rowCount != 3)!");
        CHECK_COND(insTbl->column(0)->textAt(0) == QLatin1String("SP-01")
                           && insTbl->column(1)->longName() == QLatin1String("Tensile strain")
                           && insTbl->column(1)->units() == QLatin1String("%")
                           && insTbl->column(2)->longName() == QLatin1String("Tensile stress")
                           && insTbl->column(2)->units() == QLatin1String("MPa")
                           && std::fabs(insTbl->column(2)->valueAt(1) - 492.5) < 1e-6
                           && insTbl->column(2)->isInvalid(2),
                   "Suite 49c: 2-row Instron CSV units, specimen ID, or '--' invalid cell failed!");

        // Rigaku SmartLab .ras XRD file
        const QByteArray rasBytes =
                "*RAS_DATA_START\n"
                "*RAS_HEADER_START\n"
                "*MEAS_SCAN_UNIT_X \"deg.\"\n"
                "*MEAS_SCAN_UNIT_Y \"cps\"\n"
                "*RAS_HEADER_END\n"
                "*RAS_INT_START\n"
                "20.0000 125.0 1.0000\n"
                "20.0200 1480.5 1.0000\n"
                "20.0400 310.0 1.0000\n"
                "*RAS_INT_END\n"
                "*RAS_DATA_END\n";
        std::unique_ptr<future::Table> rasTbl(AsciiTableImportFilter::importLabTextData(
                rasBytes, "RigakuXrd", "al_zn_mg.ras"));
        CHECK_COND(rasTbl != nullptr && rasTbl->columnCount() >= 2 && rasTbl->rowCount() == 3,
                   "Suite 49c: Rigaku .ras importLabTextData wrong dimensions!");
        CHECK_COND(rasTbl->column(0)->longName() == QString::fromUtf8("2\xCE\xB8")
                           && rasTbl->column(0)->units() == QLatin1String("deg.")
                           && rasTbl->column(1)->units() == QLatin1String("cps")
                           && std::fabs(rasTbl->column(1)->valueAt(1) - 1480.5) < 1e-6,
                   "Suite 49c: Rigaku .ras metadata or intensity values mismatch!");
    }

    // -------------------------------------------------------------------------
    // 49d. CurveMathTools::analyzeTensile (0.2% proof stress offsetYieldY,
    //      youngsModulus, utsY) & createArrheniusPlot (100% Arial !bold(),
    //      linked Top-X layer)
    // -------------------------------------------------------------------------
    {
        // Analytical tensile curve: E = 70,000 MPa (700 MPa/%), elastic up to 0.60% (420 MPa),
        // work-hardening crossing 0.2% offset line near ~490 MPa, UTS = 560 MPa at 8.0%
        const QVector<double> epsPct = {
            0.00, 0.10, 0.20, 0.30, 0.40, 0.50, 0.60,
            0.70, 0.80, 0.90, 1.00, 1.50, 2.50, 4.00, 6.00, 8.00, 10.00
        };
        const QVector<double> sigMPa = {
            0.0,  70.0, 140.0, 210.0, 280.0, 350.0, 420.0,
            458.0, 480.0, 492.0, 500.0, 518.0, 534.0, 548.0, 556.0, 560.0, 525.0
        };
        Table *tTen = w49->newTable(epsPct.size(), 2, "Suite49TensileTbl", "Al-7075-T6");
        CHECK_COND(tTen != nullptr, "Failed to create Suite49TensileTbl");
        tTen->column(0)->setLongName("Nominal strain");
        tTen->column(0)->setUnits("%");
        tTen->column(1)->setLongName("Nominal stress");
        tTen->column(1)->setUnits("MPa");
        for (int i = 0; i < epsPct.size(); ++i) {
            tTen->setCell(i, 0, epsPct[i]);
            tTen->setCell(i, 1, sigMPa[i]);
        }
        MultiLayer *mlTen = w49->multilayerPlot(
                tTen, QStringList() << "Suite49TensileTbl_2", Graph::Line);
        CHECK_COND(mlTen != nullptr && mlTen->activeGraph() != nullptr, "mlTen null in Suite 49d");
        Graph *gTen = mlTen->activeGraph();

        const double nanVal = std::numeric_limits<double>::quiet_NaN();
        const CurveMathTools::TensileResult tres = CurveMathTools::analyzeTensile(
                w49, gTen, QString(), 0.002, 1, nanVal, nanVal, true, true, true);
        std::cout << "[INFO] Suite 49d Tensile: E=" << tres.youngsModulus
                  << " MPa, Rp0.2=" << tres.offsetYieldY
                  << " MPa @ " << tres.offsetYieldX
                  << "%, UTS=" << tres.utsY << " MPa\n";
        CHECK_COND(tres.valid, "Suite 49d: analyzeTensile returned valid == false!");
        CHECK_COND(std::fabs(tres.youngsModulus - 70000.0) < 1500.0,
                   "Suite 49d: analyzeTensile youngsModulus deviated from 70,000 MPa!");
        // At 0.90% strain: offset line 700*(0.90 - 0.20) = 490 MPa vs curve 492 MPa -> crossing just above 0.90% (~493 MPa)
        CHECK_COND(tres.offsetYieldY > 485.0 && tres.offsetYieldY < 502.0,
                   "Suite 49d: analyzeTensile 0.2% proof stress offsetYieldY out of expected ~493 MPa range!");
        CHECK_COND(std::fabs(tres.utsY - 560.0) < 1e-4,
                   "Suite 49d: analyzeTensile utsY != 560.0 MPa!");
        CHECK_COND(tres.stressUnit == QLatin1String("MPa"),
                   "Suite 49d: analyzeTensile failed to populate stressUnit == \"MPa\"!");
        CHECK_COND(tres.trueStressTable != nullptr && tres.trueStressTable->numRows() == 16,
                   "Suite 49d: analyzeTensile TrueStressStrain table must stop at uniform elongation utsIdx (16 rows)!");

        // Round 2: Purely linear-elastic brittle fracture curve (E = 300 GPa = 300,000 MPa,
        // dropping vertically from 900 -> 0 MPa at x = 0.30..0.31%) must return valid == false!
        const QVector<double> britEpsPct = { 0.00, 0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.31 };
        const QVector<double> britSigMPa = { 0.0, 150.0, 300.0, 450.0, 600.0, 750.0, 900.0, 0.0 };
        Table *tBrit = w49->newTable(britEpsPct.size(), 2, "Suite49BrittleTbl", "SiC-Ceramic");
        CHECK_COND(tBrit != nullptr, "Failed to create Suite49BrittleTbl");
        tBrit->column(0)->setLongName("Strain");
        tBrit->column(0)->setUnits("%");
        tBrit->column(1)->setLongName("Stress");
        tBrit->column(1)->setUnits("MPa");
        for (int i = 0; i < britEpsPct.size(); ++i) {
            tBrit->setCell(i, 0, britEpsPct[i]);
            tBrit->setCell(i, 1, britSigMPa[i]);
        }
        MultiLayer *mlBrit = w49->multilayerPlot(
                tBrit, QStringList() << "Suite49BrittleTbl_2", Graph::Line);
        CHECK_COND(mlBrit != nullptr && mlBrit->activeGraph() != nullptr, "mlBrit null in Suite 49d");
        const CurveMathTools::TensileResult britRes = CurveMathTools::analyzeTensile(
                w49, mlBrit->activeGraph(), QString(), 0.002, 1, nanVal, nanVal, false, false, false);
        std::cout << "[INFO] Suite 49d Brittle ceramic valid=" << britRes.valid
                  << ", err='" << britRes.error.toStdString() << "'\n";
        CHECK_COND(!britRes.valid && !britRes.error.isEmpty(),
                   "Suite 49d: Purely linear-elastic brittle fracture curve falsely reported valid 0.2% proof stress!");

        // Arrhenius plot: Ea = 50.0 kJ/mol, R = 8.314462618 J/(mol*K)
        Table *tArr = w49->newTable(5, 2, "Suite49ArrTbl", "Diffusion");
        CHECK_COND(tArr != nullptr, "Failed to create Suite49ArrTbl");
        tArr->setColName(0, "T_K");
        tArr->setColName(1, "Rate_k");
        const double tempsK[5] = { 300.0, 325.0, 355.0, 390.0, 430.0 };
        const double eaJ = 50000.0;
        const double rGas = 8.314462618;
        for (int i = 0; i < 5; ++i) {
            tArr->setCell(i, 0, tempsK[i]);
            tArr->setCell(i, 1, 1.0e8 * std::exp(-eaJ / (rGas * tempsK[i])));
        }
        const CurveMathTools::ArrheniusResult ares = CurveMathTools::createArrheniusPlot(
                w49, tArr, "T_K", "Rate_k", false, true, false);
        CHECK_COND(ares.valid && ares.plot != nullptr,
                   "Suite 49d: createArrheniusPlot failed!");
        CHECK_COND(std::fabs(ares.activationEnergyKJmol - 50.0) < 0.25,
                   "Suite 49d: createArrheniusPlot Ea deviated from 50.0 kJ/mol!");
        CHECK_COND(ares.plot->layers() >= 2,
                   "Suite 49d: createArrheniusPlot missing linked Top-X temperature layer!");

        bool sawTopLinked = false;
        for (int li = 1; li <= ares.plot->layers(); ++li) {
            Graph *gl = ares.plot->layer(li);
            if (!gl || !gl->plotWidget())
                continue;
            if (li >= 2 && gl->plotWidget()->axisEnabled(QwtPlot::xTop))
                sawTopLinked = true;
            CHECK_COND(!gl->plotWidget()->title().font().bold(),
                       "Suite 49d: Arrhenius plot title font is bold!");
            for (int ax = 0; ax < QwtPlot::axisCnt; ++ax) {
                const QFont tf = gl->axisTitleFont(ax);
                const QFont nf = gl->plotWidget()->axisFont(ax);
                CHECK_COND(tf.family().compare("Arial", Qt::CaseInsensitive) == 0 && !tf.bold(),
                           "Suite 49d: Arrhenius axis title font is not non-bold Arial!");
                CHECK_COND(nf.family().compare("Arial", Qt::CaseInsensitive) == 0 && !nf.bold(),
                           "Suite 49d: Arrhenius axis tick font is not non-bold Arial!");
            }
            if (Legend *lg = gl->legend()) {
                CHECK_COND(lg->font().family().compare("Arial", Qt::CaseInsensitive) == 0
                                   && !lg->font().bold(),
                           "Suite 49d: Arrhenius legend font is not non-bold Arial!");
            }
        }
        CHECK_COND(sawTopLinked,
                   "Suite 49d: Arrhenius linked Top-X temperature axis not enabled on overlay layer!");

        // Round 2: Raw createArrheniusPlot must align parent and topLayer canvasRect().top()
        // within <= 2 px immediately without requiring isij_style()!
        QApplication::processEvents();
        Graph *arrParent = ares.plot->layer(1);
        Graph *arrTopLayer = nullptr;
        for (int li = 2; li <= ares.plot->layers(); ++li) {
            Graph *cand = ares.plot->layer(li);
            if (cand && cand->plotWidget() && cand->plotWidget()->axisEnabled(QwtPlot::xTop)) {
                arrTopLayer = cand;
                break;
            }
        }
        CHECK_COND(arrParent != nullptr && arrTopLayer != nullptr,
                   "Suite 49d: Missing arrParent or arrTopLayer for raw canvas alignment check!");
        const int parentCanvasTopMl =
                arrParent->plotWidget()->canvas()->mapTo(ares.plot->canvasWidget(), QPoint(0, 0)).y();
        const int topLayerCanvasTopMl =
                arrTopLayer->plotWidget()->canvas()->mapTo(ares.plot->canvasWidget(), QPoint(0, 0)).y();
        const int topDeltaPx = std::abs(parentCanvasTopMl - topLayerCanvasTopMl);
        std::cout << "[INFO] Suite 49d Raw Arrhenius canvasTop: parent=" << parentCanvasTopMl
                  << ", topLayer=" << topLayerCanvasTopMl << ", delta=" << topDeltaPx << " px\n";
        CHECK_COND(topDeltaPx <= 2,
                   "Suite 49d: Raw createArrheniusPlot parent & topLayer canvasRect().top() misaligned (> 2 px)!");
    }

    // -------------------------------------------------------------------------
    // 49e. FormulaLinkedScaleEngine nonlinear 1.0/(x*x) tick spread (zero
    //      left-edge pileup) & ScaleDraw comma-free labels ("1000", not "1,000")
    // -------------------------------------------------------------------------
    {
        Table *tHp = w49->newTable(5, 2, "Suite49HallPetch", "Hall-Petch");
        CHECK_COND(tHp != nullptr, "Failed to create Suite49HallPetch");
        const double invSqrtD[5] = { 0.08, 0.15, 0.25, 0.35, 0.48 };
        for (int i = 0; i < 5; ++i) {
            tHp->setCell(i, 0, invSqrtD[i]);
            tHp->setCell(i, 1, 120.0 + 650.0 * invSqrtD[i]);
        }
        MultiLayer *mlHp = w49->multilayerPlot(
                tHp, QStringList() << "Suite49HallPetch_2", Graph::LineSymbols);
        CHECK_COND(mlHp != nullptr && mlHp->activeGraph() != nullptr, "mlHp null in Suite 49e");
        Graph *gHp = mlHp->activeGraph();
        gHp->setScale(QwtPlot::xBottom, 0.05, 0.50, 0.10);
        gHp->setScale(QwtPlot::yLeft, 0.0, 10000.0, 2000.0);
        Graph *gHpTop = mlHp->addLinkedLayer(gHp, true, false, "1.0/(x*x)");
        CHECK_COND(gHpTop != nullptr, "Suite 49e: addLinkedLayer(1.0/(x*x)) returned null!");
        gHp->replot();
        gHpTop->replot();
        QApplication::processEvents();

        const QwtScaleDiv *topDiv = gHpTop->plotWidget()->axisScaleDiv(QwtPlot::xTop);
        CHECK_COND(topDiv != nullptr, "Suite 49e: Top-X scaleDiv null!");
        const QwtValueList majTicks = topDiv->ticks(QwtScaleDiv::MajorTick);
        CHECK_COND(majTicks.size() >= 3 && majTicks.size() <= 8,
                   "Suite 49e: Nonlinear 1.0/(x*x) linked axis should select 3..8 major ticks!");

        const QwtScaleMap topMap = gHpTop->plotWidget()->canvasMap(QwtPlot::xTop);
        const double p1 = topMap.p1();
        const double p2 = topMap.p2();
        const double pSpan = std::fabs(p2 - p1);
        CHECK_COND(pSpan > 50.0, "Suite 49e: Top-X canvas map span too small!");

        QVector<double> normPos;
        for (int i = 0; i < majTicks.size(); ++i) {
            const double px = topMap.xTransform(majTicks[i]);
            normPos.push_back((px - std::min(p1, p2)) / pSpan);
        }
        std::sort(normPos.begin(), normPos.end());
        double minAdjGap = 1.0;
        for (int i = 0; i + 1 < normPos.size(); ++i)
            minAdjGap = std::min(minAdjGap, normPos[i + 1] - normPos[i]);
        const double totalSpread = normPos.back() - normPos.front();
        std::cout << "[INFO] Suite 49e: 1.0/(x*x) linked majorTicks=" << majTicks.size()
                  << ", minAdjGap=" << minAdjGap << ", totalSpread=" << totalSpread << "\n";
        CHECK_COND(minAdjGap >= 0.10 && totalSpread >= 0.35,
                   "Suite 49e: Nonlinear 1.0/(x*x) linked ticks piled up at one edge!");

        const ScaleDraw *ySd =
                dynamic_cast<const ScaleDraw *>(gHp->plotWidget()->axisScaleDraw(QwtPlot::yLeft));
        CHECK_COND(ySd != nullptr, "Suite 49e: yLeft ScaleDraw is null!");
        const QString lbl1000 = ySd->label(1000.0).text();
        const QString lbl10000 = ySd->label(10000.0).text();
        CHECK_COND(lbl1000 == QLatin1String("1000") && lbl10000 == QLatin1String("10000")
                           && !lbl1000.contains(',') && !lbl10000.contains(','),
                   "Suite 49e: ScaleDraw emitted thousands commas or scientific flip on 1000/10000!");
    }

    // -------------------------------------------------------------------------
    // 49f. EmfPaintDevice translucent pill QColor(255, 255, 255, 205) played
    //      back via GDI+ Graphics::DrawImage(Metafile*) -> near-white blended
    //      background (NOT RGB(205,205,205) gray or black border) + PlayEnhMetaFile
    // -------------------------------------------------------------------------
    {
        Table *tPill = w49->newTable(11, 3, "Suite49PillTbl", "PillCheck");
        for (int i = 0; i <= 10; ++i) {
            tPill->setCell(i, 0, double(i));
            tPill->setCell(i, 1, 5.5); // Opaque red curve crossing behind lower part of pill
            tPill->setCell(i, 2, 2.5); // Horizontal translucent curve (y1 == y2 == 2.5)
        }
        MultiLayer *mlPill = w49->multilayerPlot(
                tPill, QStringList() << "Suite49PillTbl_2" << "Suite49PillTbl_3", Graph::Line);
        CHECK_COND(mlPill != nullptr && mlPill->activeGraph() != nullptr, "mlPill null in Suite 49f");
        mlPill->setPageSizePhysical(85.0, 85.0, MultiLayer::UnitMm, 300);
        mlPill->setPageViewMode(true);
        mlPill->fitLayersToPage();
        Graph *gPill = mlPill->activeGraph();
        gPill->setScale(QwtPlot::xBottom, 0.0, 10.0, 2.0);
        gPill->setScale(QwtPlot::yLeft, 0.0, 10.0, 2.0);
        gPill->setCurvePen(0, QPen(QColor(220, 25, 25), 5));
        gPill->setCurvePen(1, QPen(QColor(220, 38, 38, 195), 3));
        if (gPill->legend())
            gPill->removeLegend();

        // Frameless translucent white pill QColor(255, 255, 255, 205) centered away from axes,
        // using red text matching the red curve so zero neutral gray/black pixels exist unless
        // EmfPaintDevice leaks premultiplied RGB(205,205,205) or a black frame pen!
        Legend *pill = gPill->newLegend("Pill");
        CHECK_COND(pill != nullptr, "Suite 49f: newLegend failed!");
        pill->setFrameStyle(Legend::None);
        pill->setBackgroundColor(QColor(255, 255, 255, 205));
        pill->setTextColor(QColor(220, 25, 25));
        pill->setFont(QFont("Arial", 11));
        pill->setOriginCoord(3.8, 6.2);
        gPill->replot();
        QApplication::processEvents();

        // Map pill canvas rect into MultiLayer page coordinates -> 900x900 playback coordinates
        const QRect pillCanvasRc = pill->rect();
        const QPoint canvasInMl =
                gPill->plotWidget()->canvas()->mapTo(mlPill->canvasWidget(), QPoint(0, 0));
        const QRect pillMlRc(pillCanvasRc.topLeft() + canvasInMl, pillCanvasRc.size());
        const QSize mlCanvasSz = mlPill->canvasWidget()->size();
        CHECK_COND(pillMlRc.width() > 20 && pillMlRc.height() > 10 && mlCanvasSz.width() > 100,
                   "Suite 49f: invalid pill geometry on MultiLayer canvas!");

        OleRenderedGraph pillOle = SciDAVisEmf::renderMultiLayer(mlPill);
        HENHMETAFILE hPillEmf = SciDAVisEmf::createHemfFromBytes(pillOle.emfBytes);
        CHECK_COND(hPillEmf != nullptr, "Suite 49f: createHemfFromBytes failed!");

        const int playSide = 900;
        auto playEmfSurface = [&](bool useGdiplus) -> QImage {
            QImage out(playSide, playSide, QImage::Format_RGB32);
            out.fill(Qt::white);
            HDC sdc = GetDC(nullptr);
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = playSide;
            bmi.bmiHeader.biHeight = -playSide;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            void *bits = nullptr;
            HBITMAP hbm = CreateDIBSection(sdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
            HDC mdc = CreateCompatibleDC(sdc);
            HGDIOBJ old = SelectObject(mdc, hbm);
            if (bits) {
                auto *px = static_cast<quint32 *>(bits);
                for (int i = 0; i < playSide * playSide; ++i)
                    px[i] = 0xFFFFFFFFu;
                if (useGdiplus) {
                    Gdiplus::Metafile mf(hPillEmf, FALSE);
                    Gdiplus::Graphics gfx(mdc);
                    gfx.DrawImage(&mf, 0, 0, playSide, playSide);
                } else {
                    RECT rcPlay{ 0, 0, playSide, playSide };
                    PlayEnhMetaFile(mdc, hPillEmf, &rcPlay);
                }
                GdiFlush();
                std::memcpy(out.bits(), bits, playSide * playSide * 4);
            }
            SelectObject(mdc, old);
            DeleteObject(hbm);
            DeleteDC(mdc);
            ReleaseDC(nullptr, sdc);
            return out;
        };

        const QImage gdiplusImg = playEmfSurface(true);
        const QImage gdiImg = playEmfSurface(false);
        DeleteEnhMetaFile(hPillEmf);

        const double sx = double(playSide) / double(mlCanvasSz.width());
        const double sy = double(playSide) / double(mlCanvasSz.height());
        const QRect roi = QRect(qRound(pillMlRc.left() * sx) - 3,
                                qRound(pillMlRc.top() * sy) - 3,
                                qRound(pillMlRc.width() * sx) + 6,
                                qRound(pillMlRc.height() * sy) + 6)
                                  .intersected(QRect(0, 0, playSide, playSide));

        int gray205Px = 0;
        int darkBorderPx = 0;
        int nearWhitePx = 0;
        int pastelRedBlendPx = 0;
        for (int y = roi.top(); y <= roi.bottom(); ++y) {
            for (int x = roi.left(); x <= roi.right(); ++x) {
                const QRgb c = gdiplusImg.pixel(x, y);
                const int r = qRed(c), g = qGreen(c), b = qBlue(c);
                const int maxC = std::max({ r, g, b });
                const int minC = std::min({ r, g, b });
                // Neutral gray plateau RGB(205,205,205) from unblended premultiplied bytes
                if (maxC - minC <= 5 && r >= 188 && r <= 225)
                    ++gray205Px;
                // Unintended black/dark neutral border around frameless pill
                if (maxC < 95)
                    ++darkBorderPx;
                if (r >= 250 && g >= 250 && b >= 250)
                    ++nearWhitePx;
                // Red line (220,25,25) attenuated beneath QColor(255,255,255,205) -> ~(248, 210, 210)
                if (r >= 235 && g >= 180 && g <= 235 && (r - g) >= 12 && std::abs(g - b) <= 8)
                    ++pastelRedBlendPx;
            }
        }
        std::cout << "[INFO] Suite 49f GDI+ Metafile pill ROI: nearWhite=" << nearWhitePx
                  << ", gray205=" << gray205Px << ", darkBorder=" << darkBorderPx
                  << ", pastelRedBlend=" << pastelRedBlendPx << "\n";
        CHECK_COND(gray205Px == 0,
                   "Suite 49f: GDI+ Metafile rendered translucent pill as RGB(205,205,205) gray!");
        CHECK_COND(darkBorderPx == 0,
                   "Suite 49f: GDI+ Metafile drew unintended dark/black border around frameless pill!");
        CHECK_COND(nearWhitePx > 200 && pastelRedBlendPx > 15,
                   "Suite 49f: GDI+ Metafile failed near-white + translucent curve alpha blend!");

        // Confirm down-level Win32 PlayEnhMetaFile also blends without RGB(205,205,205) gray fill
        int gdiGray205 = 0;
        for (int y = roi.top(); y <= roi.bottom(); ++y) {
            for (int x = roi.left(); x <= roi.right(); ++x) {
                const QRgb c = gdiImg.pixel(x, y);
                const int r = qRed(c), g = qGreen(c), b = qBlue(c);
                if (std::max({ r, g, b }) - std::min({ r, g, b }) <= 5 && r >= 188 && r <= 225)
                    ++gdiGray205;
            }
        }
        CHECK_COND(gdiGray205 == 0,
                   "Suite 49f: Down-level PlayEnhMetaFile rendered RGB(205,205,205) gray pill!");

        // Round 2: Horizontal translucent line (y1 == y2 == 2.5, QColor(220, 38, 38, 195))
        // must NOT be dropped by empty rawBr intersection in EmfPaintDevice::blendTranslucentPath!
        const QwtScaleMap xMapPill = gPill->plotWidget()->canvasMap(QwtPlot::xBottom);
        const QwtScaleMap yMapPill = gPill->plotWidget()->canvasMap(QwtPlot::yLeft);
        const int hLineX1 = qRound((canvasInMl.x() + xMapPill.transform(2.0)) * sx);
        const int hLineX2 = qRound((canvasInMl.x() + xMapPill.transform(8.0)) * sx);
        const int hLineY = qRound((canvasInMl.y() + yMapPill.transform(2.5)) * sy);
        const QRect hLineRoi = QRect(std::min(hLineX1, hLineX2), hLineY - 8,
                                     std::abs(hLineX2 - hLineX1), 17)
                                       .intersected(QRect(0, 0, playSide, playSide));
        int horizTransRedPx = 0;
        for (int y = hLineRoi.top(); y <= hLineRoi.bottom(); ++y) {
            for (int x = hLineRoi.left(); x <= hLineRoi.right(); ++x) {
                const QRgb c = gdiplusImg.pixel(x, y);
                const int r = qRed(c), g = qGreen(c), b = qBlue(c);
                // QColor(220,38,38,195) over white blends to ~(228, 89, 89)
                if (r >= 200 && g <= 185 && b <= 185 && (r - g) >= 50)
                    ++horizTransRedPx;
            }
        }
        std::cout << "[INFO] Suite 49f Horizontal translucent line (y1==y2) GDI+ red px = "
                  << horizTransRedPx << "\n";
        CHECK_COND(horizTransRedPx >= 150,
                   "Suite 49f: Horizontal translucent line (y1==y2, alpha=195) dropped in GDI+ EMF playback!");
    }

    // -------------------------------------------------------------------------
    // 49g. .sciprj & restoreSnapshotInPlace / Undo with <text> containing
    //      "Legend" and axis/plot/window titles containing '\t'
    // -------------------------------------------------------------------------
    {
        ApplicationWindow *writer = new ApplicationWindow();
        writer->applyUserSettings();
        Table *tPrj = writer->newTable(4, 2, "Suite49PrjTbl", "Tab\tTable");
        for (int i = 0; i < 4; ++i) {
            tPrj->setCell(i, 0, double(i));
            tPrj->setCell(i, 1, double(i * 10));
        }
        MultiLayer *mlPrj = writer->multilayerPlot(
                tPrj, QStringList() << "Suite49PrjTbl_2", Graph::LineSymbols);
        CHECK_COND(mlPrj != nullptr && mlPrj->activeGraph() != nullptr, "mlPrj null in Suite 49g");
        mlPrj->setName("Suite49LegendTabGraph");
        mlPrj->setWindowLabel("Tensile\tSummary");

        Graph *gPrj = mlPrj->activeGraph();
        gPrj->setTitle("Specimen A\tRT");
        gPrj->setXAxisTitle("Nominal strain\t(%)");
        gPrj->setYAxisTitle("Nominal stress\t(MPa)");
        CHECK_COND(gPrj->legend() != nullptr && gPrj->legend()->text().contains("\\c{1}"),
                   "Suite 49g: initial curve legend missing!");

        Legend *noteWithLegendWord = gPrj->newLegend("See Legend in Table 1 for heat treatment");
        CHECK_COND(noteWithLegendWord != nullptr, "Suite 49g: failed to add text marker!");

        // 1) Snapshot / Undo round-trip in place
        const QString snap49 = mlPrj->captureUndoSnapshot();
        gPrj->setXAxisTitle("Overwritten X");
        CHECK_COND(mlPrj->restoreSnapshotInPlace(writer, snap49),
                   "Suite 49g: restoreSnapshotInPlace returned false!");
        gPrj = mlPrj->activeGraph();
        CHECK_COND(gPrj != nullptr, "Suite 49g: activeGraph null after restoreSnapshotInPlace!");

        auto verifyGraphLegendAndTabs = [](MultiLayer *mlCheck, const char *ctx) -> bool {
            if (!mlCheck || !mlCheck->activeGraph() || !mlCheck->activeGraph()->plotWidget()) {
                std::cerr << "[FAIL] " << ctx << ": null MultiLayer/Graph\n";
                return false;
            }
            Graph *gc = mlCheck->activeGraph();
            if (mlCheck->windowLabel() != QLatin1String("Tensile Summary")) {
                std::cerr << "[FAIL] " << ctx << ": windowLabel mismatch: '"
                          << mlCheck->windowLabel().toStdString() << "'\n";
                return false;
            }
            if (gc->plotWidget()->title().text() != QLatin1String("Specimen A\tRT")) {
                std::cerr << "[FAIL] " << ctx << ": plotTitle lost '\\t': '"
                          << gc->plotWidget()->title().text().toStdString() << "'\n";
                return false;
            }
            if (gc->axisTitle(QwtPlot::xBottom) != QLatin1String("Nominal strain\t(%)")
                || gc->axisTitle(QwtPlot::yLeft) != QLatin1String("Nominal stress\t(MPa)")) {
                std::cerr << "[FAIL] " << ctx << ": axisTitle lost '\\t': X='"
                          << gc->axisTitle(QwtPlot::xBottom).toStdString() << "' Y='"
                          << gc->axisTitle(QwtPlot::yLeft).toStdString() << "'\n";
                return false;
            }
            if (!gc->legend() || !gc->legend()->text().contains(QLatin1String("\\c{1}"))) {
                std::cerr << "[FAIL] " << ctx
                          << ": primary curve legend clobbered by <text> containing 'Legend'!\n";
                return false;
            }
            bool foundNote = false;
            for (QwtPlotItem *it : gc->plotWidget()->itemList()) {
                if (auto *lg = dynamic_cast<Legend *>(it)) {
                    if (lg != gc->legend()
                        && lg->text().contains(QLatin1String("See Legend in Table 1"))) {
                        foundNote = true;
                        break;
                    }
                }
            }
            if (!foundNote) {
                std::cerr << "[FAIL] " << ctx
                          << ": <text> annotation containing 'Legend' lost on reload!\n";
                return false;
            }
            return true;
        };

        CHECK_COND(verifyGraphLegendAndTabs(mlPrj, "Suite 49g restoreSnapshotInPlace"),
                   "Suite 49g: restoreSnapshotInPlace corrupted <text> 'Legend' or '\\t' titles!");

        // 2) Full .sciprj project serialization & reload round-trip
        const QByteArray prjBytes = writer->saveProjectToByteArray();
        const QString prjPath = tmpDir.filePath("suite49_legend_tab.sciprj");
        {
            QFile pf(prjPath);
            CHECK_COND(pf.open(QIODevice::WriteOnly), "Suite 49g: failed to open temp .sciprj");
            pf.write(prjBytes);
            pf.close();
        }
        writer->savedProject();
        writer->setOleEmbeddedMode(true);
        delete writer;

        ApplicationWindow *reader = new ApplicationWindow();
        reader->applyUserSettings();
        const bool okLoad = reader->loadProject(prjPath);
        CHECK_COND(okLoad, "Suite 49g: loadProject failed on suite49_legend_tab.sciprj!");
        MultiLayer *mlRel =
                qobject_cast<MultiLayer *>(reader->window("Suite49LegendTabGraph"));
        CHECK_COND(verifyGraphLegendAndTabs(mlRel, "Suite 49g .sciprj loadProject"),
                   "Suite 49g: .sciprj reload corrupted <text> 'Legend' or '\\t' titles!");

        reader->savedProject();
        reader->setOleEmbeddedMode(true);
        delete reader;
    }

    w49->savedProject();
    w49->setOleEmbeddedMode(true);
    delete w49;
    QApplication::processEvents();

    // -------------------------------------------------------------------------
    // 49h. Round 3 [P2-CORE-R3-1]: tableMenu / matrixMenu child QMenu & QAction
    //      count stability across repeated activations/language switches +
    //      ScriptEdit runtime en <-> ja QAction retranslation
    // -------------------------------------------------------------------------
    {
        mw->switchToLanguage("en");
        ApplicationWindow *wMenu = new ApplicationWindow();
        wMenu->applyUserSettings();
        wMenu->show();
        wMenu->switchToLanguage("en");
        QApplication::processEvents();

        Table *tMenuA = wMenu->newTable(4, 2, "Suite49MenuTblA", "A");
        Table *tMenuB = wMenu->newTable(4, 2, "Suite49MenuTblB", "B");
        Matrix *mMenuA = wMenu->newMatrix(4, 4);
        CHECK_COND(tMenuA != nullptr && tMenuB != nullptr && mMenuA != nullptr,
                   "Suite 49h: failed to create Table/Matrix windows!");

        // Populate both tableMenu and matrixMenu once to establish steady-state baseline
        wMenu->activateSubWindow(mMenuA);
        wMenu->customMenu(mMenuA);
        wMenu->activateSubWindow(tMenuA);
        wMenu->customMenu(tMenuA);
        QApplication::processEvents();

        const int baseMenuCount = wMenu->findChildren<QMenu *>().size();
        const int baseActionCount = wMenu->findChildren<QAction *>().size();

        ScriptEdit *se = tMenuA->findChild<ScriptEdit *>();
        CHECK_COND(se != nullptr, "Suite 49h: Table missing embedded ScriptEdit!");
        const QList<QAction *> seActions = se->findChildren<QAction *>(QString(), Qt::FindDirectChildrenOnly);
        CHECK_COND(seActions.size() >= 6, "Suite 49h: ScriptEdit missing context-menu QActions!");

        bool sawEnExecute = false;
        for (QAction *act : seActions) {
            if (act && act->text() == QLatin1String("E&xecute"))
                sawEnExecute = true;
        }
        CHECK_COND(sawEnExecute, "Suite 49h: ScriptEdit missing 'E&xecute' in English mode!");

        // Cycle across Table/Matrix activations and en <-> ja switches 4 times
        for (int cyc = 0; cyc < 4; ++cyc) {
            wMenu->activateSubWindow(tMenuB);
            wMenu->customMenu(tMenuB);
            wMenu->switchToLanguage("ja");
            wMenu->activateSubWindow(mMenuA);
            wMenu->customMenu(mMenuA);
            wMenu->activateSubWindow(tMenuA);
            wMenu->customMenu(tMenuA);
            if (cyc < 3)
                wMenu->switchToLanguage("en");
            QApplication::processEvents();
        }

        // Now in 'ja' with tMenuA active: verify zero leaked child QMenus / QActions
        const int afterMenuCount = wMenu->findChildren<QMenu *>().size();
        const int afterActionCount = wMenu->findChildren<QAction *>().size();
        std::cout << "[INFO] Suite 49h Menu/Action stability: menus " << baseMenuCount << " -> "
                  << afterMenuCount << ", actions " << baseActionCount << " -> " << afterActionCount
                  << "\n";
        CHECK_COND(afterMenuCount == baseMenuCount,
                   "Suite 49h: tableMenu/matrixMenu leaked detached child QMenus across activations & language switches!");
        CHECK_COND(afterActionCount == baseActionCount,
                   "Suite 49h: QAction count grew across repeated Table/Matrix activations & language switches!");

        // Verify ScriptEdit runtime en -> ja retranslation of all 6 context-menu QActions
        const QStringList rawEnTexts = {
            QStringLiteral("E&xecute"),
            QStringLiteral("Execute &All"),
            QStringLiteral("&Evaluate Expression"),
            QStringLiteral("&Print"),
            QStringLiteral("&Import"),
            QStringLiteral("&Export")
        };
        int jaNonAsciiCount = 0;
        for (QAction *act : seActions) {
            if (!act)
                continue;
            const QString txt = act->text();
            CHECK_COND(!rawEnTexts.contains(txt),
                       "Suite 49h: ScriptEdit action stayed untranslated English after runtime switchToLanguage('ja')!");
            bool hasNonAscii = false;
            for (QChar ch : txt) {
                if (ch.unicode() > 127) {
                    hasNonAscii = true;
                    break;
                }
            }
            if (hasNonAscii)
                ++jaNonAsciiCount;
        }
        CHECK_COND(jaNonAsciiCount >= 6,
                   "Suite 49h: ScriptEdit did not retranslate all 6 actions into Japanese on QEvent::LanguageChange!");

        // Switch back ja -> en and verify bidirectional restoration
        wMenu->switchToLanguage("en");
        QApplication::processEvents();
        bool restoredEnExecute = false;
        for (QAction *act : seActions) {
            if (act && act->text() == QLatin1String("E&xecute"))
                restoredEnExecute = true;
        }
        CHECK_COND(restoredEnExecute,
                   "Suite 49h: ScriptEdit failed bidirectional ja -> en retranslation!");

        wMenu->savedProject();
        wMenu->setOleEmbeddedMode(true);
        delete wMenu;
        QApplication::processEvents();
    }

    std::cout << "[INFO] Suite 49 Phase B UX/Render/MatSci/API Regression Gate passed!\n";
    return 0;
}

#include <QTemporaryDir>

int runSuite50MegaWave1PlotsMatSciAutonomyUndoPerf(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 50: MegaWave1_Plots_MatSci_Autonomy_Undo_Perf...\n";
    Q_UNUSED(mw);

    QTemporaryDir tmpDir;
    CHECK_COND(tmpDir.isValid(), "Suite 50: failed to create temporary directory!");

    ApplicationWindow *w50 = new ApplicationWindow();
    w50->setHeadlessMode(true);
    w50->applyUserSettings();
    w50->switchToLanguage("en");
    QApplication::processEvents();

    auto verifyArialNormalGraph = [](MultiLayer *mlCheck, const char *ctx) -> bool {
        if (!mlCheck || mlCheck->layersList().isEmpty()) {
            std::cerr << "[FAIL] Suite 50 (" << ctx << "): null or empty MultiLayer!\n";
            return false;
        }
        for (Graph *gl : mlCheck->layersList()) {
            if (!gl || !gl->plotWidget())
                continue;
            const QFont tf = gl->plotWidget()->title().font();
            if (tf.family().compare(QLatin1String("Arial"), Qt::CaseInsensitive) != 0 || tf.bold()) {
                std::cerr << "[FAIL] Suite 50 (" << ctx << "): plot title font='"
                          << tf.family().toStdString() << "' bold=" << tf.bold() << "\n";
                return false;
            }
            for (int ax = 0; ax < QwtPlot::axisCnt; ++ax) {
                const QFont atf = gl->axisTitleFont(ax);
                const QFont tkf = gl->axisFont(ax);
                if (atf.family().compare(QLatin1String("Arial"), Qt::CaseInsensitive) != 0 || atf.bold()) {
                    std::cerr << "[FAIL] Suite 50 (" << ctx << "): axisTitleFont(" << ax << ")='"
                              << atf.family().toStdString() << "' bold=" << atf.bold() << "\n";
                    return false;
                }
                if (tkf.family().compare(QLatin1String("Arial"), Qt::CaseInsensitive) != 0 || tkf.bold()) {
                    std::cerr << "[FAIL] Suite 50 (" << ctx << "): axisFont(" << ax << ")='"
                              << tkf.family().toStdString() << "' bold=" << tkf.bold() << "\n";
                    return false;
                }
            }
            for (QwtPlotItem *it : gl->plotWidget()->itemList()) {
                if (auto *lg = dynamic_cast<Legend *>(it)) {
                    const QFont lf = lg->font();
                    if (lf.family().compare(QLatin1String("Arial"), Qt::CaseInsensitive) != 0 || lf.bold()) {
                        std::cerr << "[FAIL] Suite 50 (" << ctx << "): Legend font='"
                                  << lf.family().toStdString() << "' bold=" << lf.bold() << "\n";
                        return false;
                    }
                }
            }
        }
        return true;
    };

    // -------------------------------------------------------------------------
    // 50a. Box/Violin category labels (plotBoxDiagram sets 1-based category
    //      labels, clears "X Axis Title", skips NaN & masked points)
    // -------------------------------------------------------------------------
    {
        Table *tBox = w50->newTable(10, 3, "S50BoxTbl", "Temper Hardness");
        CHECK_COND(tBox != nullptr, "Suite 50a: failed to create S50BoxTbl");
        tBox->setColName(0, "T4");
        tBox->setColName(1, "T6");
        tBox->setColName(2, "T73");
        tBox->setColPlotDesignation(0, SciDAVis::Y);
        tBox->setColPlotDesignation(1, SciDAVis::Y);
        tBox->setColPlotDesignation(2, SciDAVis::Y);

        for (int c = 0; c < 3; ++c) {
            for (int r = 0; r < 8; ++r)
                tBox->setCell(r, c, 120.0 + 25.0 * double(c) + 2.5 * double(r));
            // Row 8: NaN / invalid cell
            tBox->column(c)->setInvalid(8, true);
            // Row 9: extreme outlier 99999.0 that is masked
            tBox->setCell(9, c, 99999.0);
            tBox->column(c)->setMasked(9, true);
        }

        MultiLayer *mlBox = w50->newGraph("S50BoxGraph");
        CHECK_COND(mlBox != nullptr && mlBox->activeGraph() != nullptr, "Suite 50a: mlBox null");
        Graph *gBox = mlBox->activeGraph();
        gBox->setXAxisTitle("X Axis Title");
        gBox->plotBoxDiagram(
                tBox, QStringList() << "S50BoxTbl_T4" << "S50BoxTbl_T6" << "S50BoxTbl_T73");

        const QStringList boxCats = gBox->categoryLabels(QwtPlot::xBottom);
        CHECK_COND(boxCats.size() == 3 && boxCats[0].contains("T4")
                           && boxCats[1].contains("T6") && boxCats[2].contains("T73"),
                   "Suite 50a: plotBoxDiagram failed to set 1-based category labels T4/T6/T73!");
        CHECK_COND(gBox->axisTitle(QwtPlot::xBottom).trimmed().isEmpty(),
                   "Suite 50a: plotBoxDiagram failed to clear 'X Axis Title' placeholder!");
        const QwtScaleDiv *xDivBox = gBox->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
        CHECK_COND(xDivBox != nullptr && std::fabs(xDivBox->lowerBound() - 0.5) < 1e-4
                           && std::fabs(xDivBox->upperBound() - 3.5) < 1e-4,
                   "Suite 50a: Box plot X-axis category bounds not locked to [0.5, 3.5]!");

        CHECK_COND(gBox->curves() == 3, "Suite 50a: expected 3 BoxCurve items!");
        for (int k = 0; k < 3; ++k) {
            auto *bc = dynamic_cast<BoxCurve *>(gBox->curve(k));
            CHECK_COND(bc != nullptr, "Suite 50a: curve is not BoxCurve!");
            CHECK_COND(bc->dataSize() == 8 && bc->boundingRect().bottom() < 1000.0,
                       "Suite 50a: BoxCurve::loadData failed to skip NaN/invalid or masked 99999.0 point!");
            if (k == 0)
                bc->setViolinMode(true);
        }
        gBox->replot();
        QImage violinImg(480, 360, QImage::Format_ARGB32);
        violinImg.fill(Qt::white);
        {
            QPainter vp(&violinImg);
            gBox->exportPainter(vp, false, violinImg.rect());
        }
    }

    // -------------------------------------------------------------------------
    // 50b. Grouped Bars (--headless non-zero dataOffset(), 0.0 baseline in
    //      boundingRect()) + Category X-axis [0.5, K+0.5] across applyStylePreset
    // -------------------------------------------------------------------------
    {
        Table *tBar = w50->newTable(4, 3, "S50BarTbl", "Grouped Bars");
        CHECK_COND(tBar != nullptr, "Suite 50b: failed to create S50BarTbl");
        for (int r = 0; r < 4; ++r) {
            tBar->setCell(r, 0, double(r + 1));
            tBar->setCell(r, 1, 120.0 + 20.0 * double(r)); // min = 120 > 0
            tBar->setCell(r, 2, 95.0 + 18.0 * double(r));  // min = 95 > 0
        }
        // Keep w50 & mlBar hidden (plotWidget()->isVisible() == false, matching --headless!)
        MultiLayer *mlBar = w50->multilayerPlot(
                tBar, QStringList() << "S50BarTbl_2" << "S50BarTbl_3", Graph::VerticalBars);
        CHECK_COND(mlBar != nullptr && mlBar->activeGraph() != nullptr, "Suite 50b: mlBar null");
        Graph *gBar = mlBar->activeGraph();
        CHECK_COND(!gBar->plotWidget()->isVisible(),
                   "Suite 50b: expected hidden/headless Plot widget during dataOffset check!");

        auto *bar0 = dynamic_cast<QwtBarCurve *>(gBar->curve(0));
        auto *bar1 = dynamic_cast<QwtBarCurve *>(gBar->curve(1));
        CHECK_COND(bar0 != nullptr && bar1 != nullptr, "Suite 50b: bar0/bar1 null!");
        const double off0 = bar0->dataOffset();
        const double off1 = bar1->dataOffset();
        CHECK_COND(std::fabs(off0) > 1e-6 && std::fabs(off1) > 1e-6
                           && std::fabs(off0 - off1) > 1e-4,
                   "Suite 50b: headless Grouped Bars returned zero or unstaggered dataOffset()!");
        const QwtDoubleRect br0 = bar0->boundingRect();
        CHECK_COND(std::fabs(br0.top() - 0.0) < 1e-9 && br0.bottom() >= 180.0,
                   "Suite 50b: QwtBarCurve::boundingRect() missed 0.0 baseline!");

        const QStringList cat4 = { "Base", "Aged-1h", "Aged-8h", "Overaged" };
        gBar->setCategoryLabels(QwtPlot::xBottom, cat4);
        mlBar->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        mlBar->applyStylePreset(MultiLayer::StylePresentationSlide28pt);
        const QwtScaleDiv *xDivBar = gBar->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
        CHECK_COND(xDivBar != nullptr && std::fabs(xDivBar->lowerBound() - 0.5) < 1e-4
                           && std::fabs(xDivBar->upperBound() - 4.5) < 1e-4
                           && gBar->categoryLabels(QwtPlot::xBottom) == cat4,
                   "Suite 50b: Category X-axis [0.5, 4.5] clobbered by applyStylePreset!");
    }

    // -------------------------------------------------------------------------
    // 50c. Ternary (Al-Zn-Mg + applyStylePreset safety) & Polar plot
    //      (setupPolarPlot, isPolarPlot, .sciprj round-trip)
    // -------------------------------------------------------------------------
    {
        Table *tTern = w50->newTable(4, 2, "S50TernTbl", "Al-Zn-Mg");
        const double compA[4] = { 90.0, 80.0, 70.0, 85.0 };
        const double compB[4] = { 6.0, 12.0, 18.0, 10.0 };
        const double compC[4] = { 4.0, 8.0, 12.0, 5.0 };
        for (int i = 0; i < 4; ++i) {
            const QPointF xy = Graph::ternaryToCartesian(compA[i], compB[i], compC[i]);
            tTern->setCell(i, 0, xy.x());
            tTern->setCell(i, 1, xy.y());
        }
        MultiLayer *mlTern = w50->multilayerPlot(
                tTern, QStringList() << "S50TernTbl_2", Graph::Scatter);
        CHECK_COND(mlTern != nullptr && mlTern->activeGraph() != nullptr, "Suite 50c: mlTern null");
        Graph *gTern = mlTern->activeGraph();
        gTern->setupTernaryPlot("Al", "Zn", "Mg");
        mlTern->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        CHECK_COND(gTern->isTernaryPlot() && gTern->ternaryLabel(0) == QLatin1String("Al")
                           && gTern->ternaryLabel(1) == QLatin1String("Zn")
                           && gTern->ternaryLabel(2) == QLatin1String("Mg")
                           && !gTern->plotWidget()->axisEnabled(QwtPlot::xBottom)
                           && !gTern->plotWidget()->axisEnabled(QwtPlot::yLeft),
                   "Suite 50c: Ternary Al-Zn-Mg corrupted or Cartesian axes re-enabled by applyStylePreset!");

        // Polar plot + applyStylePreset + .sciprj round-trip
        ApplicationWindow *polWriter = new ApplicationWindow();
        polWriter->applyUserSettings();
        MultiLayer *mlPol = polWriter->newGraph("S50PolarGraph");
        CHECK_COND(mlPol != nullptr && mlPol->activeGraph() != nullptr, "Suite 50c: mlPol null");
        const QString polGraphName = mlPol->name();
        Graph *gPol = mlPol->activeGraph();
        QVector<double> thDeg, radVal;
        for (int k = 0; k <= 36; ++k) {
            const double deg = 10.0 * double(k);
            thDeg.push_back(deg);
            radVal.push_back(1.0 + 0.5 * std::cos(2.0 * deg * M_PI / 180.0));
        }
        gPol->setupPolarPlot(thDeg, radVal, "RollingTexture", false);
        mlPol->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        CHECK_COND(gPol->isPolarPlot() && gPol->polarRadiusMax() >= 1.5
                           && !gPol->plotWidget()->axisEnabled(QwtPlot::xBottom)
                           && !gPol->plotWidget()->axisEnabled(QwtPlot::yLeft),
                   "Suite 50c: Polar plot Cartesian axes leaked after applyStylePreset!");

        const QString polPrjPath = tmpDir.filePath("suite50_polar.sciprj");
        {
            QFile pf(polPrjPath);
            CHECK_COND(pf.open(QIODevice::WriteOnly), "Suite 50c: failed to open polar .sciprj");
            pf.write(polWriter->saveProjectToByteArray());
            pf.close();
        }
        polWriter->savedProject();
        polWriter->setOleEmbeddedMode(true);
        delete polWriter;

        ApplicationWindow *polReader = new ApplicationWindow();
        polReader->applyUserSettings();
        CHECK_COND(polReader->loadProject(polPrjPath), "Suite 50c: loadProject failed on polar .sciprj!");
        MultiLayer *mlPolRel = qobject_cast<MultiLayer *>(polReader->window(polGraphName));
        CHECK_COND(mlPolRel != nullptr && mlPolRel->activeGraph() != nullptr
                           && mlPolRel->activeGraph()->isPolarPlot()
                           && !mlPolRel->activeGraph()->plotWidget()->axisEnabled(QwtPlot::xBottom),
                   "Suite 50c: Polar plot lost isPolarPlot() state across .sciprj round-trip!");
        polReader->savedProject();
        polReader->setOleEmbeddedMode(true);
        delete polReader;
    }

    // -------------------------------------------------------------------------
    // 50d. ylog_zero piecewise log break preserved across setScale, +-1e308
    //      scale overflow guard, and QwtHistogram sub-micron [0.015, 0.35]
    //      auto-binning (> 3 bins)
    // -------------------------------------------------------------------------
    {
        Table *tBrk = w50->newTable(6, 2, "S50YLogZeroTbl", "ylog_zero");
        const double xv[6] = { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0 };
        const double yv[6] = { 0.0, 1e-4, 1e-3, 1e-2, 1e-1, 10.0 };
        for (int i = 0; i < 6; ++i) {
            tBrk->setCell(i, 0, xv[i]);
            tBrk->setCell(i, 1, yv[i]);
        }
        MultiLayer *mlBrk = w50->multilayerPlot(
                tBrk, QStringList() << "S50YLogZeroTbl_2", Graph::LineSymbols);
        CHECK_COND(mlBrk != nullptr && mlBrk->activeGraph() != nullptr, "Suite 50d: mlBrk null");
        Graph *gBrk = mlBrk->activeGraph();

        Graph::AxisBreakSpec ylogZero;
        ylogZero.enabled = true;
        ylogZero.breakFrom = 1e-5;
        ylogZero.breakTo = 1e-4;
        ylogZero.positionPct = 0.18;
        ylogZero.log10Before = false;
        ylogZero.log10After = true;
        gBrk->setAxisBreak(QwtPlot::yLeft, ylogZero);
        gBrk->setScale(QwtPlot::yLeft, 0.0, 100.0, 0.0, 5, 9, 0 /*Linear outer bounds*/, false);

        const Graph::AxisBreakSpec afterSetScale = gBrk->axisBreak(QwtPlot::yLeft);
        CHECK_COND(afterSetScale.enabled && !afterSetScale.log10Before && afterSetScale.log10After
                           && std::fabs(afterSetScale.breakTo - 1e-4) < 1e-12,
                   "Suite 50d: Graph::setScale wiped ylog_zero piecewise linear+log axis break!");

        // +-1e308 extreme scale overflow guard (no hang / OOM / inf span)
        QElapsedTimer ovTimer;
        ovTimer.start();
        gBrk->setScale(QwtPlot::xBottom, -1e308, 1e308, 0.0, 5, 5, 0 /*Linear*/, false);
        gBrk->replot();
        const qint64 ovMs = ovTimer.elapsed();
        const QwtScaleDiv *ovDiv = gBrk->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
        CHECK_COND(ovMs < 500 && ovDiv != nullptr && std::isfinite(ovDiv->lowerBound())
                           && std::isfinite(ovDiv->upperBound())
                           && std::fabs(ovDiv->upperBound()) <= 1.01e300,
                   "Suite 50d: +-1e308 scale overflow guard failed or hung!");

        // QwtHistogram sub-micron [0.015, 0.35] um auto-binning (> 3 bins)
        Table *tHist = w50->newTable(40, 1, "S50SubMicronHist", "Precipitate Radius (um)");
        tHist->setColPlotDesignation(0, SciDAVis::Y);
        for (int i = 0; i < 40; ++i) {
            const double val = 0.015 + (0.35 - 0.015) * (double(i) / 39.0);
            tHist->setCell(i, 0, val);
        }
        MultiLayer *mlHist = w50->newGraph("S50HistGraph");
        Graph *gHist = mlHist->activeGraph();
        CHECK_COND(gHist != nullptr && gHist->plotHistogram(tHist, QStringList() << "S50SubMicronHist_1"),
                   "Suite 50d: plotHistogram failed on sub-micron table!");
        auto *hc = dynamic_cast<QwtHistogram *>(gHist->curve(0));
        CHECK_COND(hc != nullptr && hc->autoBinning() && hc->dataSize() > 3 && hc->binSize() < 0.2,
                   "Suite 50d: QwtHistogram collapsed sub-micron [0.015, 0.35] data into <= 3 bins!");
    }

    // -------------------------------------------------------------------------
    // 50e. Matrix::determinant() & Matrix::invert() on 32x32 all-zero matrix
    //      (0 crash, clean singular handling) + invertible sanity check
    // -------------------------------------------------------------------------
    {
        Matrix *mZero = w50->newMatrix("S50ZeroMatrix32", 32, 32);
        CHECK_COND(mZero != nullptr, "Suite 50e: failed to create 32x32 zero matrix!");
        QVector<qreal> zeros(32 * 32, 0.0);
        mZero->setCells(zeros);

        const double detZero = mZero->determinant();
        CHECK_COND(std::isfinite(detZero) && std::fabs(detZero) < 1e-12,
                   "Suite 50e: 32x32 all-zero Matrix::determinant() did not return 0.0!");
        mZero->invert(); // Must NOT segfault or corrupt cells with NaN/Inf
        for (int r = 0; r < 32; ++r) {
            for (int c = 0; c < 32; ++c) {
                CHECK_COND(std::isfinite(mZero->cell(r, c)) && std::fabs(mZero->cell(r, c)) < 1e-12,
                           "Suite 50e: singular 32x32 Matrix::invert() corrupted cell values!");
            }
        }

        Matrix *mDiag = w50->newMatrix("S50DiagMatrix4", 4, 4);
        QVector<qreal> diag(16, 0.0);
        diag[0] = 2.0;
        diag[5] = 4.0;
        diag[10] = 5.0;
        diag[15] = 10.0;
        mDiag->setCells(diag);
        CHECK_COND(std::fabs(mDiag->determinant() - 400.0) < 1e-8,
                   "Suite 50e: 4x4 diagonal Matrix::determinant() != 400.0!");
        mDiag->invert();
        CHECK_COND(std::fabs(mDiag->cell(0, 0) - 0.5) < 1e-9
                           && std::fabs(mDiag->cell(3, 3) - 0.1) < 1e-9,
                   "Suite 50e: 4x4 diagonal Matrix::invert() inaccurate!");
    }

    // -------------------------------------------------------------------------
    // 50f & 50g. All 20 Sample Datasets & Project Packs + Kissinger, Work Hardening
    //            (Considere), XRD (Scherrer + W-H), VASP DOS & Bands + 100% Arial Normal
    // -------------------------------------------------------------------------
    {
        const QVector<CurveMathTools::SampleDatasetInfo> dsList = CurveMathTools::listSampleDatasets();
        CHECK_COND(dsList.size() == 20, "Suite 50f: listSampleDatasets() count != 20!");

        const QStringList requiredIds = {
            "tensile_alznmg",
            "tensile_temper_compare",
            "tensile_hydrogen_embrittlement",
            "fatigue_sn_curve",
            "fatigue_crack_growth_paris",
            "hall_petch_grain_size",
            "creep_curve_9cr1mo",
            "charpy_dbtt_curve",
            "vickers_age_hardening",
            "nanoindentation_load_depth",
            "cyclic_hysteresis_bauschinger",
            "fracture_toughness_jr_curve",
            "strain_rate_flow_stress",
            "tds_hydrogen_3rates",
            "xrd_aged_alloy",
            "arrhenius_diffusion",
            "ct_local_strain_map",
            "weibull_ceramics",
            "project_metal_mechanics_suite",
            "project_alznmg_he_research"
        };

        MultiLayer *mlTensileSample = nullptr;
        MultiLayer *mlXrdSample = nullptr;

        for (const QString &dsId : requiredIds) {
            Table *outTbl = nullptr;
            Matrix *outMtx = nullptr;
            MultiLayer *mlSample = CurveMathTools::loadSampleDataset(w50, dsId, true, &outTbl, &outMtx);
            CHECK_COND(mlSample != nullptr && mlSample->activeGraph() != nullptr,
                       "Suite 50f: loadSampleDataset returned null MultiLayer!");
            if (dsId == QLatin1String("ct_local_strain_map")) {
                CHECK_COND(outMtx != nullptr && outMtx->numRows() == 128,
                           "Suite 50f: ct_local_strain_map did not populate 128x128 Matrix!");
            } else {
                CHECK_COND(outTbl != nullptr && outTbl->numRows() >= 6,
                           "Suite 50f: loadSampleDataset did not populate Table!");
            }
            CHECK_COND(verifyArialNormalGraph(mlSample, dsId.toUtf8().constData()),
                       "Suite 50f: sample dataset graph violated 100% Arial QFont::Normal!");
            if (dsId == QLatin1String("tensile_alznmg"))
                mlTensileSample = mlSample;
            else if (dsId == QLatin1String("xrd_aged_alloy"))
                mlXrdSample = mlSample;
        }

        // Kissinger TDS: exact synthetic Ea = 45.0 kJ/mol -> ln(beta / Tp^2) = C - Ea / (R * Tp)
        constexpr double kR = 8.314462618;
        constexpr double eaTrueJ = 45000.0;
        const QVector<double> tpK = { 420.0, 450.0, 480.0, 510.0 };
        QVector<double> betaKmin, tpCelsius;
        for (double tk : tpK) {
            const double b = (tk * tk) * std::exp(14.0 - eaTrueJ / (kR * tk));
            betaKmin.push_back(b);
            tpCelsius.push_back(tk - 273.15);
        }
        const CurveMathTools::KissingerResult kis =
                CurveMathTools::analyzeKissinger(w50, betaKmin, tpCelsius, false, true, true);
        std::cout << "[INFO] Suite 50g Kissinger Ea=" << kis.activationEnergyKJmol
                  << " kJ/mol (true 45.0), R^2=" << kis.rSquare << "\n";
        CHECK_COND(kis.valid && std::fabs(kis.activationEnergyKJmol - 45.0) < 0.05
                           && kis.rSquare > 0.9999 && kis.plot != nullptr,
                   "Suite 50g: analyzeKissinger Ea accuracy or R^2 check failed!");
        CHECK_COND(verifyArialNormalGraph(kis.plot, "analyzeKissinger"),
                   "Suite 50g: Kissinger graph violated 100% Arial QFont::Normal!");

        // Work Hardening / Kocks-Mecking Considere necking root on AA7075-T6 sample
        CHECK_COND(mlTensileSample != nullptr, "Suite 50g: mlTensileSample null");
        const CurveMathTools::WorkHardeningResult wh = CurveMathTools::analyzeWorkHardening(
                w50, mlTensileSample->activeGraph(), QString(), 11, true);
        std::cout << "[INFO] Suite 50g WorkHardening Considere eps_t=" << wh.considereTrueStrain
                  << ", sigma_t=" << wh.considereTrueStress << " MPa, n=" << wh.hollomonN << "\n";
        CHECK_COND(wh.valid && wh.considereTrueStrain > 0.05 && wh.considereTrueStrain < 0.20
                           && wh.considereTrueStress > 500.0 && wh.hollomonN > 0.02
                           && wh.plot != nullptr,
                   "Suite 50g: analyzeWorkHardening Considere root out of range!");
        CHECK_COND(verifyArialNormalGraph(wh.plot, "analyzeWorkHardening"),
                   "Suite 50g: WorkHardening graph violated 100% Arial QFont::Normal!");

        // XRD Scherrer + Williamson-Hall on Peak-Aged Al-Zn-Mg sample
        CHECK_COND(mlXrdSample != nullptr, "Suite 50g: mlXrdSample null");
        const CurveMathTools::XRDAnalysisResult xrd = CurveMathTools::analyzeXRD(
                w50, mlXrdSample->activeGraph(), QString(), 1.540598, 0.05, 0.90,
                QStringLiteral("fcc"), false, true, true);
        std::cout << "[INFO] Suite 50g XRD peaks=" << xrd.peaks.size()
                  << ", Scherrer D=" << xrd.meanScherrerSizeNm
                  << " nm, W-H D=" << xrd.whSizeNm
                  << " nm, strain=" << xrd.whMicrostrain
                  << ", a0=" << xrd.latticeParamAngstrom << " A\n";
        CHECK_COND(xrd.valid && xrd.peaks.size() >= 4 && xrd.meanScherrerSizeNm > 5.0
                           && xrd.whSizeNm > 5.0 && xrd.latticeParamAngstrom > 3.9
                           && xrd.latticeParamAngstrom < 4.2 && xrd.whPlot != nullptr,
                   "Suite 50g: analyzeXRD Scherrer / Williamson-Hall / lattice parameter failed!");
        CHECK_COND(verifyArialNormalGraph(mlXrdSample, "analyzeXRD_parent")
                           && verifyArialNormalGraph(xrd.whPlot, "analyzeXRD_WH"),
                   "Suite 50g: XRD / W-H graph violated 100% Arial QFont::Normal!");

        // VASP DOS & Band Structure
        Table *tDos = w50->newTable(81, 3, "S50VaspDosSrc", "DOSCAR");
        tDos->setColName(0, "Energy");
        tDos->setColName(1, "TotalDOS");
        tDos->setColName(2, "d_DOS");
        for (int i = 0; i < 81; ++i) {
            const double e = -6.0 + 0.15 * double(i);
            const double dBand = 4.0 * std::exp(-((e + 1.8) * (e + 1.8)) / 1.2);
            const double tot = dBand + ((std::fabs(e) > 0.6) ? 1.2 : 0.0);
            tDos->setCell(i, 0, e);
            tDos->setCell(i, 1, tot);
            tDos->setCell(i, 2, dBand);
        }
        const CurveMathTools::VaspPlotResult vDos = CurveMathTools::plotVaspDos(
                w50, tDos, "Energy", QStringList() << "TotalDOS" << "d_DOS", QStringList(), 0.0, false);
        CHECK_COND(vDos.valid && vDos.plot != nullptr && std::isfinite(vDos.dBandCenterEV)
                           && std::fabs(vDos.dBandCenterEV - (-1.8)) < 0.25,
                   "Suite 50g: plotVaspDos failed or d-band center inaccurate!");
        CHECK_COND(verifyArialNormalGraph(vDos.plot, "plotVaspDos"),
                   "Suite 50g: VASP DOS graph violated 100% Arial QFont::Normal!");

        Table *tBand = w50->newTable(41, 3, "S50VaspBandSrc", "EIGENVAL");
        tBand->setColName(0, "k_dist");
        tBand->setColName(1, "VB");
        tBand->setColName(2, "CB");
        for (int i = 0; i < 41; ++i) {
            const double k = 2.0 * double(i) / 40.0;
            const double c = std::cos(M_PI * k);
            tBand->setCell(i, 0, k);
            tBand->setCell(i, 1, -1.6 + 1.0 * c); // VBM = -0.6 eV at k=0, 2
            tBand->setCell(i, 2, 1.6 - 1.0 * c);  // CBM = +0.6 eV at k=0, 2 -> Eg = 1.2 eV
        }
        const QVector<double> kTicks = { 0.0, 1.0, 2.0 };
        const QStringList kLabels = { QString::fromUtf8("\xCE\x93"), "X", "M" };
        const CurveMathTools::VaspPlotResult vBand = CurveMathTools::plotVaspBands(
                w50, tBand, "k_dist", QStringList() << "VB" << "CB", kTicks, kLabels, 0.0, -4.0, 4.0);
        CHECK_COND(vBand.valid && vBand.plot != nullptr && std::fabs(vBand.bandgapEV - 1.2) < 0.05,
                   "Suite 50g: plotVaspBands failed or bandgap != 1.2 eV!");
        CHECK_COND(verifyArialNormalGraph(vBand.plot, "plotVaspBands"),
                   "Suite 50g: VASP Band Structure graph violated 100% Arial QFont::Normal!");
    }

    // -------------------------------------------------------------------------
    // 50h. 200,000-point PlotCurve render < 500 ms, FocusCanvasHudOverlay
    //      visible in LayoutSplitTableGraph with 0 eager QTableWidgetItems when
    //      drawer is hidden, and Table 12-edit + formula Undo/Redo (12/12 Redo)
    // -------------------------------------------------------------------------
    {
        ApplicationWindow *wPerf = new ApplicationWindow();
        wPerf->applyUserSettings();
        wPerf->resize(1280, 800);
        wPerf->show();
        QApplication::processEvents();

        constexpr int N200K = 200000;
        Table *t200k = wPerf->newTable(N200K, 2, "S50Big200kTbl", "200k Stream");
        CHECK_COND(t200k != nullptr, "Suite 50h: failed to allocate 200k Table!");
        QVector<qreal> vx(N200K), vy(N200K);
        for (int i = 0; i < N200K; ++i) {
            const double x = 0.001 * double(i);
            vx[i] = x;
            vy[i] = std::sin(x) + 0.25 * std::cos(17.0 * x);
        }
        t200k->column(0)->replaceValues(0, vx);
        t200k->column(1)->replaceValues(0, vy);

        MultiLayer *ml200k = wPerf->multilayerPlot(
                t200k, QStringList() << "S50Big200kTbl_2", Graph::Line);
        CHECK_COND(ml200k != nullptr && ml200k->activeGraph() != nullptr, "Suite 50h: ml200k null");
        Graph *g200k = ml200k->activeGraph();
        QImage renderTarget(900, 600, QImage::Format_ARGB32);
        renderTarget.fill(Qt::white);

        QElapsedTimer renderTimer;
        renderTimer.start();
        {
            QPainter rp(&renderTarget);
            g200k->exportPainter(rp, false, renderTarget.rect());
        }
        const qint64 renderMs = renderTimer.elapsed();
        std::cout << "[INFO] Suite 50h 200,000-point PlotCurve render time = " << renderMs << " ms\n";
        CHECK_COND(renderMs < 500,
                   "Suite 50h: 200,000-point PlotCurve render exceeded 500 ms budget!");

        // FocusCanvasHudOverlay visible in LayoutSplitTableGraph + 0 eager QTableWidgetItems when drawer hidden
        ModernRibbonBar *ribbon = wPerf->modernRibbonBar();
        CHECK_COND(ribbon != nullptr, "Suite 50h: ModernRibbonBar null on wPerf");
        ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
        QApplication::processEvents();
        ribbon->setFocusHudMode(true);
        QApplication::processEvents();

        FocusCanvasHudOverlay *hud = ribbon->focusHudOverlay();
        FocusTableDrawer *drawer = ribbon->focusTableDrawer();
        CHECK_COND(hud != nullptr && drawer != nullptr, "Suite 50h: HUD or Drawer null");
        CHECK_COND(hud->isOverlayActive() && !hud->isHidden() && hud->isVisibleTo(wPerf)
                           && hud->geometry().width() > 100 && hud->geometry().top() >= 0,
                   "Suite 50h: FocusCanvasHudOverlay not visible in LayoutSplitTableGraph!");
        CHECK_COND(drawer->isHidden(), "Suite 50h: FocusTableDrawer should start hidden!");

        int eagerWidgetItems = 0;
        for (QTableWidget *tw : drawer->findChildren<QTableWidget *>()) {
            if (!tw)
                continue;
            for (int r = 0; r < tw->rowCount(); ++r) {
                for (int c = 0; c < tw->columnCount(); ++c) {
                    if (tw->item(r, c) != nullptr)
                        ++eagerWidgetItems;
                }
            }
        }
        CHECK_COND(eagerWidgetItems == 0,
                   "Suite 50h: FocusTableDrawer allocated eager QTableWidgetItems while hidden!");

        ribbon->setFocusHudMode(false);
        QApplication::processEvents();

        // Table 12-edit + dependent formula Undo/Redo preserving 12/12 Redo steps
        Table *tUndo = wPerf->newTable(6, 2, "S50UndoFormulaTbl", "12-Step Undo/Redo");
        CHECK_COND(tUndo != nullptr && tUndo->d_future_table != nullptr, "Suite 50h: tUndo null");
        for (int r = 0; r < 6; ++r)
            tUndo->setCell(r, 0, double(r + 1)); // 1..6
        tUndo->setCommand(1, "col(\"1\")*3.0 + 5.0");
        CHECK_COND(tUndo->recalculate(1, false), "Suite 50h: initial formula recalculate failed!");
        CHECK_COND(std::fabs(tUndo->cell(0, 1) - 8.0) < 1e-9,
                   "Suite 50h: initial formula cell(0, 1) != 8.0!");

        QUndoStack *us = tUndo->d_future_table->undoStack();
        CHECK_COND(us != nullptr && us->undoLimit() >= 50,
                   "Suite 50h: Table QUndoStack null or undoLimit < 50!");
        us->clear();

        for (int step = 0; step < 12; ++step) {
            const int row = step % 6;
            const double newX = 10.0 + double(step);
            tUndo->setCell(row, 0, newX);
            CHECK_COND(std::fabs(tUndo->cell(row, 1) - (3.0 * newX + 5.0)) < 1e-9,
                       "Suite 50h: dependent formula column did not auto-recalculate on edit!");
        }
        CHECK_COND(us->count() == 12,
                   "Suite 50h: 12 Table edits + formula recalc did not produce exactly 12 undo macros!");

        int undoCount = 0;
        for (int step = 0; step < 12; ++step) {
            CHECK_COND(us->canUndo(), "Suite 50h: canUndo() false during 12-step undo!");
            us->undo();
            ++undoCount;
            CHECK_COND(us->canRedo() && us->count() == 12,
                       "Suite 50h: Undo wiped Redo stack or pushed spurious formula QUndoCommand!");
        }
        CHECK_COND(undoCount == 12, "Suite 50h: undoCount != 12!");
        for (int r = 0; r < 6; ++r) {
            const double expX = double(r + 1);
            CHECK_COND(std::fabs(tUndo->cell(r, 0) - expX) < 1e-9
                               && std::fabs(tUndo->cell(r, 1) - (3.0 * expX + 5.0)) < 1e-9,
                       "Suite 50h: 12-step Undo did not restore initial X and formula Y values!");
        }

        int redoCount = 0;
        for (int step = 0; step < 12; ++step) {
            CHECK_COND(us->canRedo(), "Suite 50h: canRedo() false before completing 12/12 Redo steps!");
            us->redo();
            ++redoCount;
        }
        CHECK_COND(redoCount == 12 && !us->canRedo(),
                   "Suite 50h: failed to complete all 12/12 Redo steps!");
        // After steps 6..11, row r has X = 10.0 + (6 + r) = 16.0 + r
        for (int r = 0; r < 6; ++r) {
            const double expX = 16.0 + double(r);
            CHECK_COND(std::fabs(tUndo->cell(r, 0) - expX) < 1e-9
                               && std::fabs(tUndo->cell(r, 1) - (3.0 * expX + 5.0)) < 1e-9,
                       "Suite 50h: 12/12 Redo did not restore post-edit X and formula Y values!");
        }

        wPerf->savedProject();
        wPerf->setOleEmbeddedMode(true);
        delete wPerf;
        QApplication::processEvents();
    }

    // -------------------------------------------------------------------------
    // 50i. Mega-Wave 2 Surgical Regression Gate:
    //      - setupPolarPlot + newTable / column rename / clone / removeCurve (0 crash)
    //      - Graph3D::exportImage non-cropped 4-quadrant coverage in --headless mode
    //      - QwtBarCurve::setStacked(true) (offset == 0) & UserCurve preservation
    //      - HorizontalBars with ColumnMode::Text X (yLeft categories, xBottom numeric)
    //      - setCustomTickLabels (Gamma, X, M, Gamma) & setWeibullProbabilityAxis
    //        .sciprj round-trip + applyStylePreset survival
    //      - HCP analyzeXRD(..., "hcp", ..., 1.587) + analyzeWorkHardening(..., true)
    // -------------------------------------------------------------------------
    {
        // 1) setupPolarPlot followed by newTable / column rename / clone / removeCurve
        MultiLayer *mlPolSafe = w50->newGraph("S50iPolarSafeGraph");
        CHECK_COND(mlPolSafe != nullptr && mlPolSafe->activeGraph() != nullptr,
                   "Suite 50i: mlPolSafe null");
        Graph *gPolSafe = mlPolSafe->activeGraph();
        QVector<double> polTh, polR;
        for (int k = 0; k <= 24; ++k) {
            const double deg = 15.0 * double(k);
            polTh.push_back(deg);
            polR.push_back(50.0 + 20.0 * std::sin(2.0 * deg * M_PI / 180.0));
        }
        gPolSafe->setupPolarPlot(polTh, polR, "PolarTextureSafe", false);
        Table *tAfterPol = w50->newTable(6, 2, "S50iAfterPolTbl", "AfterPolar");
        CHECK_COND(tAfterPol != nullptr, "Suite 50i: failed to create table after setupPolarPlot");
        tAfterPol->setColName(1, "RenamedCol");
        w50->updateColNames("S50iAfterPolTbl_2", "S50iAfterPolTbl_RenamedCol");
        w50->updateCurves(tAfterPol, "S50iAfterPolTbl_RenamedCol");
        MultiLayer *mlPolClone = qobject_cast<MultiLayer *>(w50->clone(mlPolSafe));
        CHECK_COND(mlPolClone != nullptr && mlPolClone->activeGraph() != nullptr
                           && mlPolClone->activeGraph()->isPolarPlot(),
                   "Suite 50i: cloning Polar MultiLayer failed or lost isPolarPlot()!");
        if (gPolSafe->curves() > 0)
            gPolSafe->removeCurve(0);
        w50->removeCurves("S50iAfterPolTbl_RenamedCol");

        // 2) Graph3D::exportImage non-cropped coverage across all 4 quadrants in headless mode
        Graph3D *g3d = w50->newPlot3D();
        CHECK_COND(g3d != nullptr && w50->isHeadlessMode(),
                   "Suite 50i: failed to create headless Graph3D!");
        g3d->addFunction("cos(x)*cos(y)", -3.0, 3.0, -3.0, 3.0, -1.2, 1.2);
        const QString g3dPng = tmpDir.filePath("suite50i_headless_3d.png");
        g3d->exportImage(g3dPng, 100, false);
        QImage img3d(g3dPng);
        CHECK_COND(!img3d.isNull() && img3d.width() >= 320 && img3d.height() >= 240,
                   "Suite 50i: headless Graph3D::exportImage produced null or tiny PNG!");
        int qPx[4] = { 0, 0, 0, 0 };
        const int midX = img3d.width() / 2;
        const int midY = img3d.height() / 2;
        for (int y = 0; y < img3d.height(); y += 2) {
            for (int x = 0; x < img3d.width(); x += 2) {
                const QRgb px = img3d.pixel(x, y);
                if (qRed(px) < 250 || qGreen(px) < 250 || qBlue(px) < 250) {
                    const int qIdx = ((y >= midY) ? 2 : 0) + ((x >= midX) ? 1 : 0);
                    ++qPx[qIdx];
                }
            }
        }
        std::cout << "[INFO] Suite 50i Headless 3D PNG (" << img3d.width() << "x" << img3d.height()
                  << ") quadrant non-white px = [" << qPx[0] << ", " << qPx[1] << ", " << qPx[2]
                  << ", " << qPx[3] << "]\n";
        CHECK_COND(qPx[0] >= 50 && qPx[1] >= 50 && qPx[2] >= 50 && qPx[3] >= 50,
                   "Suite 50i: headless Graph3D::exportImage cropped 3D plot out of one or more quadrants!");

        // 3) QwtBarCurve::setStacked(true) (offset == 0) & UserCurve preservation
        Table *tStk = w50->newTable(4, 3, "S50iStackTbl", "StackedBars");
        for (int r = 0; r < 4; ++r) {
            tStk->setCell(r, 0, double(r + 1));
            tStk->setCell(r, 1, 30.0 + 10.0 * double(r));
            tStk->setCell(r, 2, 70.0 - 10.0 * double(r));
        }
        MultiLayer *mlStk = w50->multilayerPlot(
                tStk, QStringList() << "S50iStackTbl_2" << "S50iStackTbl_3", Graph::VerticalBars);
        CHECK_COND(mlStk != nullptr && mlStk->activeGraph() != nullptr, "Suite 50i: mlStk null");
        Graph *gStk = mlStk->activeGraph();
        auto *barS0 = dynamic_cast<QwtBarCurve *>(gStk->curve(0));
        auto *barS1 = dynamic_cast<QwtBarCurve *>(gStk->curve(1));
        CHECK_COND(barS0 != nullptr && barS1 != nullptr, "Suite 50i: barS0/barS1 null");
        barS0->setStacked(true);
        barS1->setStacked(true);
        gStk->setBarsGap(0, 30, 0);
        gStk->setBarsGap(1, 30, 0);
        CurveLayout clBar = gStk->initCurveLayout(Graph::VerticalBars, 2);
        gStk->updateCurveLayout(0, &clBar);
        CHECK_COND(barS0->isStacked() && barS1->isStacked()
                           && barS0->offset() == 0 && barS1->offset() == 0
                           && std::fabs(barS0->dataOffset()) < 1e-9
                           && std::fabs(barS1->dataOffset()) < 1e-9
                           && barS0->style() == QwtPlotCurve::UserCurve,
                   "Suite 50i: QwtBarCurve::setStacked(true) offset!=0 or updateCurveLayout lost UserCurve!");

        // 4) HorizontalBars with ColumnMode::Text X (yLeft categories, xBottom numeric)
        Table *tHBar = w50->newTable(4, 2, "S50iHBarTxtTbl", "HorizTextBars");
        tHBar->setColumnType(0, SciDAVis::ColumnMode::Text);
        tHBar->setColPlotDesignation(0, SciDAVis::X);
        tHBar->setColPlotDesignation(1, SciDAVis::Y);
        const char *hCats[4] = { "Alloy-A", "Alloy-B", "Alloy-C", "Alloy-D" };
        const double hVals[4] = { 410.0, 520.0, 485.0, 610.0 };
        for (int r = 0; r < 4; ++r) {
            tHBar->setText(r, 0, QString::fromLatin1(hCats[r]));
            tHBar->setCell(r, 1, hVals[r]);
        }
        MultiLayer *mlHBar = w50->multilayerPlot(
                tHBar, QStringList() << "S50iHBarTxtTbl_2", Graph::HorizontalBars);
        CHECK_COND(mlHBar != nullptr && mlHBar->activeGraph() != nullptr, "Suite 50i: mlHBar null");
        Graph *gHBar = mlHBar->activeGraph();
        const QStringList yLeftCats = gHBar->categoryLabels(QwtPlot::yLeft);
        const QwtScaleDiv *hBarXDiv = gHBar->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
        CHECK_COND(yLeftCats.size() == 4 && yLeftCats[0] == QLatin1String("Alloy-A")
                           && gHBar->categoryLabels(QwtPlot::xBottom).isEmpty()
                           && hBarXDiv != nullptr && hBarXDiv->upperBound() >= 600.0,
                   "Suite 50i: HorizontalBars with Text X failed to place categories on yLeft & numeric on xBottom!");

        // 5) setCustomTickLabels (Gamma, X, M, Gamma) & setWeibullProbabilityAxis
        //    .sciprj round-trip + applyStylePreset survival
        {
            ApplicationWindow *axWriter = new ApplicationWindow();
            axWriter->setHeadlessMode(true);
            axWriter->applyUserSettings();
            Table *tCw = axWriter->newTable(4, 2, "S50iCwTbl", "CustomWeibull");
            for (int r = 0; r < 4; ++r) {
                tCw->setCell(r, 0, double(r));
                tCw->setCell(r, 1, -4.0 + 1.8 * double(r));
            }
            MultiLayer *mlCw = axWriter->multilayerPlot(
                    tCw, QStringList() << "S50iCwTbl_2", Graph::LineSymbols);
            CHECK_COND(mlCw != nullptr && mlCw->activeGraph() != nullptr, "Suite 50i: mlCw null");
            mlCw->setName("S50iCustomWeibullGraph");
            Graph *gCw = mlCw->activeGraph();

            const QVector<double> kPos = { 0.0, 1.0, 2.0, 3.0 };
            const QString gammaSym = QString::fromUtf8("\xCE\x93");
            const QStringList kLbl = { gammaSym, "X", "M", gammaSym };
            gCw->setScale(QwtPlot::xBottom, 0.0, 3.0);
            gCw->setCustomTickLabels(QwtPlot::xBottom, kPos, kLbl, 0.0);
            gCw->plotWidget()->enableAxis(QwtPlot::yRight, true);
            gCw->setScale(QwtPlot::yRight, -6.9, 1.6);
            gCw->setWeibullProbabilityAxis(QwtPlot::yRight, true);

            mlCw->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
            CHECK_COND(gCw->hasCustomTickLabels(QwtPlot::xBottom)
                               && gCw->hasWeibullProbabilityAxis(QwtPlot::yRight)
                               && gCw->plotWidget()->axisScaleDraw(QwtPlot::xBottom)->label(0.0).text() == gammaSym
                               && gCw->plotWidget()->axisScaleDraw(QwtPlot::xBottom)->label(2.0).text() == QLatin1String("M")
                               && gCw->plotWidget()->axisScaleDraw(QwtPlot::yRight)->label(0.0).text().contains("63.2"),
                       "Suite 50i: setCustomTickLabels or setWeibullProbabilityAxis wiped by applyStylePreset!");

            const QString cwPrjPath = tmpDir.filePath("suite50i_custom_weibull.sciprj");
            {
                QFile pf(cwPrjPath);
                CHECK_COND(pf.open(QIODevice::WriteOnly), "Suite 50i: failed to write cw .sciprj");
                pf.write(axWriter->saveProjectToByteArray());
                pf.close();
            }
            axWriter->savedProject();
            axWriter->setOleEmbeddedMode(true);
            delete axWriter;

            ApplicationWindow *axReader = new ApplicationWindow();
            axReader->setHeadlessMode(true);
            axReader->applyUserSettings();
            CHECK_COND(axReader->loadProject(cwPrjPath), "Suite 50i: loadProject failed on cw .sciprj!");
            MultiLayer *mlCwRel =
                    qobject_cast<MultiLayer *>(axReader->window("S50iCustomWeibullGraph"));
            CHECK_COND(mlCwRel != nullptr && mlCwRel->activeGraph() != nullptr,
                       "Suite 50i: S50iCustomWeibullGraph missing after reload!");
            Graph *gCwRel = mlCwRel->activeGraph();
            CHECK_COND(gCwRel->hasCustomTickLabels(QwtPlot::xBottom)
                               && gCwRel->hasWeibullProbabilityAxis(QwtPlot::yRight)
                               && gCwRel->plotWidget()->axisScaleDraw(QwtPlot::xBottom)->label(0.0).text() == gammaSym
                               && gCwRel->plotWidget()->axisScaleDraw(QwtPlot::xBottom)->label(1.0).text() == QLatin1String("X")
                               && gCwRel->plotWidget()->axisScaleDraw(QwtPlot::yRight)->label(0.0).text().contains("63.2"),
                       "Suite 50i: setCustomTickLabels or setWeibullProbabilityAxis lost across .sciprj reload!");
            axReader->savedProject();
            axReader->setOleEmbeddedMode(true);
            delete axReader;
        }

        // 6) HCP analyzeXRD(..., "hcp", ..., 1.587) + analyzeWorkHardening(..., true)
        {
            constexpr double lamCu = 1.540598;
            constexpr double aHcp = 2.9508; // alpha-Ti HCP: a = 2.9508 A, c/a = 1.587
            constexpr double caRatio = 1.587;
            struct HcpPeakSpec { int h, k, l; double amp; };
            const HcpPeakSpec specs[5] = {
                { 1, 0, 0, 280.0 },
                { 0, 0, 2, 310.0 },
                { 1, 0, 1, 1000.0 },
                { 1, 0, 2, 190.0 },
                { 1, 1, 0, 220.0 }
            };
            QVector<double> pkTwoTheta, pkAmp;
            for (const auto &sp : specs) {
                const double mVal = (4.0 / 3.0) * double(sp.h * sp.h + sp.h * sp.k + sp.k * sp.k)
                        + double(sp.l * sp.l) / (caRatio * caRatio);
                const double dHkl = aHcp / std::sqrt(mVal);
                const double th = std::asin(lamCu / (2.0 * dHkl));
                pkTwoTheta.push_back(2.0 * th * 180.0 / M_PI);
                pkAmp.push_back(sp.amp);
            }

            constexpr int NHCP = 1001;
            Table *tHcp = w50->newTable(NHCP, 2, "S50iTiHcpXrd", "alpha-Ti HCP");
            for (int i = 0; i < NHCP; ++i) {
                const double tt = 30.0 + 0.05 * double(i); // 30 .. 80 deg
                double inten = 15.0;
                for (int p = 0; p < pkTwoTheta.size(); ++p) {
                    const double u = (tt - pkTwoTheta[p]) / (0.26 * 0.5);
                    inten += pkAmp[p] * (0.65 * std::exp(-0.693147 * u * u) + 0.35 / (1.0 + u * u));
                }
                tHcp->setCell(i, 0, tt);
                tHcp->setCell(i, 1, inten);
            }
            MultiLayer *mlHcp = w50->multilayerPlot(
                    tHcp, QStringList() << "S50iTiHcpXrd_2", Graph::Line);
            CHECK_COND(mlHcp != nullptr && mlHcp->activeGraph() != nullptr, "Suite 50i: mlHcp null");
            const CurveMathTools::XRDAnalysisResult xrdHcp = CurveMathTools::analyzeXRD(
                    w50, mlHcp->activeGraph(), QString(), lamCu, 0.05, 0.90,
                    QStringLiteral("hcp"), false, true, true, caRatio);
            bool saw002 = false, saw101 = false;
            for (const auto &pe : xrdHcp.peaks) {
                if (pe.hklLabel.contains("002"))
                    saw002 = true;
                if (pe.hklLabel.contains("101"))
                    saw101 = true;
            }
            std::cout << "[INFO] Suite 50i HCP XRD peaks=" << xrdHcp.peaks.size()
                      << ", a=" << xrdHcp.latticeParamAngstrom << " A (true " << aHcp
                      << "), saw002=" << saw002 << ", saw101=" << saw101 << "\n";
            CHECK_COND(xrdHcp.valid && xrdHcp.peaks.size() >= 4 && saw002 && saw101
                               && std::fabs(xrdHcp.latticeParamAngstrom - aHcp) < 0.03
                               && xrdHcp.whPlot != nullptr
                               && verifyArialNormalGraph(xrdHcp.whPlot, "analyzeXRD_HCP"),
                       "Suite 50i: HCP analyzeXRD(..., 'hcp', ..., 1.587) indexing or a-parameter failed!");

            Table *tKmSrc = nullptr;
            MultiLayer *mlKmSrc =
                    CurveMathTools::loadSampleDataset(w50, "tensile_alznmg", true, &tKmSrc, nullptr);
            CHECK_COND(mlKmSrc != nullptr && mlKmSrc->activeGraph() != nullptr,
                       "Suite 50i: mlKmSrc null");
            const CurveMathTools::WorkHardeningResult whKm = CurveMathTools::analyzeWorkHardening(
                    w50, mlKmSrc->activeGraph(), QString(), 11, true, true /*kocksMeckingPlot*/);
            CHECK_COND(whKm.valid && whKm.plot != nullptr && whKm.plot->activeGraph() != nullptr
                               && verifyArialNormalGraph(whKm.plot, "analyzeWorkHardening_KM"),
                       "Suite 50i: analyzeWorkHardening(..., kocksMeckingPlot=true) failed!");
        }
    }

    // -------------------------------------------------------------------------
    // 50j. Mega-Wave 3 Surgical Regression Gate:
    //      (a) 12-op Table + formula + row-resize (68) + removeCol 12x undo -> 12x redo
    //      (b) 40-cycle 256x256 Matrix create/close (d_matrix_private freed cleanly)
    //      (c) 200,000-pt high-frequency ultrasonic sine sin(i*0.05) render < 250 ms
    //      (d) Weibull .sciprj reload Right-Y 1%..63.2%..99% distinct vertical positions
    //      (e) <ToriStackedBars> .sciprj reload preserving gap & offset==0 across K=3
    // -------------------------------------------------------------------------
    {
        // (a) 12-op Table + formula + setNumRows(68) + addCol + removeCol: 12x undo -> 12x redo
        {
            Table *t50j = w50->newTable(20, 3, "S50jUndo68Tbl", "Row68+RemoveCol");
            CHECK_COND(t50j != nullptr && t50j->d_future_table != nullptr, "Suite 50j(a): t50j null");
            for (int r = 0; r < 20; ++r)
                t50j->setCell(r, 0, double(r + 1));
            t50j->setCommand(1, "col(\"1\")*2.0 + 1.0");
            CHECK_COND(t50j->recalculate(1, false), "Suite 50j(a): formula init failed");

            QUndoStack *us = t50j->d_future_table->undoStack();
            CHECK_COND(us != nullptr, "Suite 50j(a): QUndoStack null");
            us->clear();
            w50->activateSubWindow(t50j);

            // Ops 1..4: 4 cell edits with dependent formula auto-recalc
            for (int s = 0; s < 4; ++s)
                t50j->setCell(s, 0, 50.0 + double(s));
            // Op 5: resize rows 20 -> 68
            t50j->setNumRows(68);
            // Ops 6..8: 3 cell edits in newly added rows 50..52
            for (int s = 0; s < 3; ++s)
                t50j->setCell(50 + s, 0, 120.0 + double(s));
            // Op 9: addCol (cols 3 -> 4)
            t50j->addCol(SciDAVis::Y);
            // Op 10: edit row 65 on newly added column 3
            t50j->setCell(65, 3, 42.0);
            // Op 11: removeCol on column 2 (cols 4 -> 3; former col 3 shifts to col 2)
            t50j->removeCol(QStringList() << t50j->colName(2));
            // Op 12: edit row 67 on col 0 -> formula col 1 becomes 601.0
            t50j->setCell(67, 0, 300.0);

            if (!(us->count() == 12 && t50j->numRows() == 68 && t50j->numCols() == 3
                  && std::fabs(t50j->cell(67, 1) - 601.0) < 1e-9)) {
                std::cout << "[DIAG] 50j(a) us->count=" << us->count()
                          << " rows=" << t50j->numRows() << " cols=" << t50j->numCols()
                          << " cell(67,0)=" << t50j->cell(67, 0)
                          << " cell(67,1)=" << t50j->cell(67, 1) << "\n";
            }
            CHECK_COND(us->count() == 12 && t50j->numRows() == 68 && t50j->numCols() == 3
                               && std::fabs(t50j->cell(67, 1) - 601.0) < 1e-9,
                       "Suite 50j(a): 12-op setup count or formula state mismatch!");

            for (int step = 0; step < 12; ++step) {
                CHECK_COND(us->canUndo(), "Suite 50j(a): canUndo() false during 12x undo!");
                w50->undo();
                CHECK_COND(us->canRedo() && us->count() == 12,
                           "Suite 50j(a): undo() corrupted Redo stack or pushed nested command!");
            }
            CHECK_COND(t50j->numRows() == 20 && t50j->numCols() == 3
                               && std::fabs(t50j->cell(0, 0) - 1.0) < 1e-9
                               && std::fabs(t50j->cell(0, 1) - 3.0) < 1e-9,
                       "Suite 50j(a): 12x undo did not restore initial 20x3 Table state!");

            for (int step = 0; step < 12; ++step) {
                CHECK_COND(us->canRedo(), "Suite 50j(a): canRedo() false during 12x redo!");
                w50->redo(); // Must NEVER segfault or push nested TableSetNumberOfRowsCmd!
            }
            CHECK_COND(!us->canRedo() && us->count() == 12
                               && t50j->numRows() == 68 && t50j->numCols() == 3
                               && std::fabs(t50j->cell(67, 0) - 300.0) < 1e-9
                               && std::fabs(t50j->cell(67, 1) - 601.0) < 1e-9
                               && std::fabs(t50j->cell(65, 2) - 42.0) < 1e-9,
                       "Suite 50j(a): 12x redo failed to restore 68x3 Table + formula + col state!");

            // Second undo->redo cycle to prove TableSetNumberOfRowsCmd::d_old_rows stayed 20 across redo!
            for (int step = 0; step < 12; ++step)
                w50->undo();
            CHECK_COND(t50j->numRows() == 20 && t50j->numCols() == 3,
                       "Suite 50j(a): second 12x undo after redo failed to restore 20x3 Table shape!");
            for (int step = 0; step < 12; ++step)
                w50->redo();
            CHECK_COND(t50j->numRows() == 68 && t50j->numCols() == 3
                               && std::fabs(t50j->cell(67, 1) - 601.0) < 1e-9,
                       "Suite 50j(a): second 12x redo failed!");
        }

        // (b) 40-cycle 256x256 Matrix create/close (d_matrix_private & QObjects freed cleanly)
        {
            QVector<qreal> matPayload(256 * 256, 1.25);
            for (int cyc = 0; cyc < 40; ++cyc) {
                Matrix *m256 = w50->newMatrix(QString("S50jMtx256_%1").arg(cyc), 256, 256);
                CHECK_COND(m256 != nullptr && m256->d_future_matrix != nullptr,
                           "Suite 50j(b): failed to create 256x256 Matrix!");
                m256->setCells(matPayload);
                QPointer<Matrix> pMtx(m256);
                QPointer<future::Matrix> pFut(m256->d_future_matrix);
                m256->askOnCloseEvent(false);
                w50->closeWindow(m256);
                w50->savedProject();
                QApplication::processEvents();
                CHECK_COND(pMtx.isNull() && pFut.isNull(),
                           "Suite 50j(b): 256x256 Matrix or future::Matrix leaked after closeWindow!");
            }
        }

        // (c) 200,000-pt high-frequency ultrasonic sine sin(i * 0.05) render < 250 ms
        {
            constexpr int NULTRA = 200000;
            Table *tUltra = w50->newTable(NULTRA, 2, "S50jUltra200kTbl", "Ultrasonic Sine");
            CHECK_COND(tUltra != nullptr, "Suite 50j(c): failed to allocate 200k ultrasonic table!");
            QVector<qreal> ux(NULTRA), uy(NULTRA);
            for (int i = 0; i < NULTRA; ++i) {
                ux[i] = double(i);
                uy[i] = std::sin(double(i) * 0.05);
            }
            tUltra->column(0)->replaceValues(0, ux);
            tUltra->column(1)->replaceValues(0, uy);

            MultiLayer *mlUltra = w50->multilayerPlot(
                    tUltra, QStringList() << "S50jUltra200kTbl_2", Graph::Line);
            CHECK_COND(mlUltra != nullptr && mlUltra->activeGraph() != nullptr,
                       "Suite 50j(c): mlUltra null");
            Graph *gUltra = mlUltra->activeGraph();
            QImage ultraImg(900, 600, QImage::Format_ARGB32);
            ultraImg.fill(Qt::white);

            QElapsedTimer ultraTimer;
            ultraTimer.start();
            {
                QPainter up(&ultraImg);
                gUltra->exportPainter(up, false, ultraImg.rect());
            }
            const qint64 ultraMs = ultraTimer.elapsed();
            std::cout << "[INFO] Suite 50j(c) 200,000-pt high-freq sin(i*0.05) render time = "
                      << ultraMs << " ms\n";
            CHECK_COND(ultraMs < 250,
                       "Suite 50j(c): 200,000-pt high-frequency sine render exceeded 250 ms budget!");
        }

        // (d) Weibull .sciprj reload Right-Y 1%..63.2%..99% distinct vertical positions
        {
            ApplicationWindow *wbWriter = new ApplicationWindow();
            wbWriter->setHeadlessMode(true);
            wbWriter->applyUserSettings();
            Table *tWbSample = nullptr;
            MultiLayer *mlWb = CurveMathTools::loadSampleDataset(
                    wbWriter, "weibull_ceramics", true, &tWbSample, nullptr);
            CHECK_COND(mlWb != nullptr, "Suite 50j(d): loadSampleDataset('weibull_ceramics') null");
            mlWb->setName("S50jWeibullPrjGraph");

            const QString wbPrjPath = tmpDir.filePath("suite50j_weibull_reload.sciprj");
            {
                QFile pf(wbPrjPath);
                CHECK_COND(pf.open(QIODevice::WriteOnly), "Suite 50j(d): failed to open wb .sciprj");
                pf.write(wbWriter->saveProjectToByteArray());
                pf.close();
            }
            wbWriter->savedProject();
            wbWriter->setOleEmbeddedMode(true);
            delete wbWriter;

            ApplicationWindow *wbReader = new ApplicationWindow();
            wbReader->setHeadlessMode(true);
            wbReader->applyUserSettings();
            CHECK_COND(wbReader->loadProject(wbPrjPath),
                       "Suite 50j(d): loadProject failed on suite50j_weibull_reload.sciprj!");
            MultiLayer *mlWbRel =
                    qobject_cast<MultiLayer *>(wbReader->window("S50jWeibullPrjGraph"));
            CHECK_COND(mlWbRel != nullptr, "Suite 50j(d): S50jWeibullPrjGraph missing on reload!");

            Graph *gWbRight = nullptr;
            for (Graph *cand : mlWbRel->layersList()) {
                if (cand && cand->hasWeibullProbabilityAxis(QwtPlot::yRight)) {
                    gWbRight = cand;
                    break;
                }
            }
            CHECK_COND(gWbRight != nullptr && gWbRight->plotWidget() != nullptr,
                       "Suite 50j(d): no layer retained hasWeibullProbabilityAxis(yRight) after reload!");

            QImage wbRender(640, 520, QImage::Format_ARGB32);
            wbRender.fill(Qt::white);
            {
                QPainter wp(&wbRender);
                gWbRight->exportPainter(wp, false, wbRender.rect());
            }
            gWbRight->plotWidget()->resize(560, 440);
            gWbRight->plotWidget()->updateLayout();

            const QwtScaleMap yRightMap = gWbRight->plotWidget()->canvasMap(QwtPlot::yRight);
            const double w01 = std::log(-std::log(1.0 - 0.01));
            const double w632 = 0.0;
            const double w99 = std::log(-std::log(1.0 - 0.99));
            const double py01 = yRightMap.xTransform(w01);
            const double py632 = yRightMap.xTransform(w632);
            const double py99 = yRightMap.xTransform(w99);
            std::cout << "[INFO] Suite 50j(d) Reloaded Weibull Right-Y py(1%, 63.2%, 99%) = ("
                      << py01 << ", " << py632 << ", " << py99 << ")\n";
            CHECK_COND(py01 > py632 + 40.0 && py632 > py99 + 18.0,
                       "Suite 50j(d): Reloaded Weibull Right-Y 1%..63.2%..99% ticks piled up instead of spanning canvas!");

            wbReader->savedProject();
            wbReader->setOleEmbeddedMode(true);
            delete wbReader;
        }

        // (e) <ToriStackedBars> .sciprj reload preserving identical gap & offset==0 across K=3
        {
            ApplicationWindow *sbWriter = new ApplicationWindow();
            sbWriter->setHeadlessMode(true);
            sbWriter->applyUserSettings();
            Table *tSb3 = sbWriter->newTable(4, 4, "S50jSb3Tbl", "Stacked3");
            for (int r = 0; r < 4; ++r) {
                tSb3->setCell(r, 0, double(r + 1));
                tSb3->setCell(r, 1, 25.0 + 5.0 * double(r));
                tSb3->setCell(r, 2, 50.0 + 10.0 * double(r));
                tSb3->setCell(r, 3, 85.0 + 15.0 * double(r));
            }
            MultiLayer *mlSb3 = sbWriter->multilayerPlot(
                    tSb3, QStringList() << "S50jSb3Tbl_2" << "S50jSb3Tbl_3" << "S50jSb3Tbl_4",
                    Graph::VerticalBars);
            CHECK_COND(mlSb3 != nullptr && mlSb3->activeGraph() != nullptr, "Suite 50j(e): mlSb3 null");
            mlSb3->setName("S50jStacked3Graph");
            Graph *gSb3 = mlSb3->activeGraph();
            CHECK_COND(gSb3->curves() == 3, "Suite 50j(e): expected 3 bar series!");
            for (int ci = 0; ci < 3; ++ci) {
                auto *bc = dynamic_cast<QwtBarCurve *>(gSb3->curve(ci));
                CHECK_COND(bc != nullptr, "Suite 50j(e): curve is not QwtBarCurve!");
                bc->setStacked(true);
                gSb3->setBarsGap(ci, 28, 0);
            }
            gSb3->replot();

            const QString sbPrjPath = tmpDir.filePath("suite50j_stacked3.sciprj");
            {
                QFile pf(sbPrjPath);
                CHECK_COND(pf.open(QIODevice::WriteOnly), "Suite 50j(e): failed to open sb3 .sciprj");
                pf.write(sbWriter->saveProjectToByteArray());
                pf.close();
            }
            sbWriter->savedProject();
            sbWriter->setOleEmbeddedMode(true);
            delete sbWriter;

            ApplicationWindow *sbReader = new ApplicationWindow();
            sbReader->setHeadlessMode(true);
            sbReader->applyUserSettings();
            CHECK_COND(sbReader->loadProject(sbPrjPath),
                       "Suite 50j(e): loadProject failed on suite50j_stacked3.sciprj!");
            MultiLayer *mlSbRel =
                    qobject_cast<MultiLayer *>(sbReader->window("S50jStacked3Graph"));
            CHECK_COND(mlSbRel != nullptr && mlSbRel->activeGraph() != nullptr
                               && mlSbRel->activeGraph()->curves() == 3,
                       "Suite 50j(e): S50jStacked3Graph missing or curve count != 3 after reload!");
            Graph *gSbRel = mlSbRel->activeGraph();
            for (int ci = 0; ci < 3; ++ci) {
                auto *bc = dynamic_cast<QwtBarCurve *>(gSbRel->curve(ci));
                CHECK_COND(bc != nullptr && bc->isStacked() && bc->gap() == 28 && bc->offset() == 0
                                   && std::fabs(bc->dataOffset()) < 1e-9,
                           "Suite 50j(e): <ToriStackedBars> .sciprj reload failed to preserve gap==28 & offset==0 across K=3!");
            }

            sbReader->savedProject();
            sbReader->setOleEmbeddedMode(true);
            delete sbReader;
        }
    }

    // -------------------------------------------------------------------------
    // 50k. Mega-Wave 4 Surgical Regression Gate:
    //      (a) Plot::removeCurve / closing Table before Graph & Matrix before Heatmap
    //          deletes DataCurve & Spectrogram (0 leaked curve items)
    //      (b) 200,000-pt sin(i * 0.05) 600-DPI QImage export via MultiLayer::exportImage < 300 ms
    //      (c) Pie + applyStylePreset(StyleISIJJournal85mm) keeps legend & pie non-overlapping
    // -------------------------------------------------------------------------
    {
        // (a) Deterministic C++ destructor verification on Plot::removeCurve & source close
        {
            struct ProbeDataCurve : public DataCurve {
                int *destroyedCount = nullptr;
                ProbeDataCurve(Table *t, const QString &xCol, const QString &yCol, int *cnt)
                    : DataCurve(t, xCol, yCol, 0, -1), destroyedCount(cnt)
                {
                    setType(Graph::Line);
                    loadData();
                }
                ~ProbeDataCurve() override
                {
                    if (destroyedCount)
                        ++(*destroyedCount);
                }
            };
            struct ProbeSpectrogram : public Spectrogram {
                int *destroyedCount = nullptr;
                ProbeSpectrogram(Matrix *m, int *cnt) : Spectrogram(m), destroyedCount(cnt) {}
                ~ProbeSpectrogram() override
                {
                    if (destroyedCount)
                        ++(*destroyedCount);
                }
            };

            Table *tRem = w50->newTable(8, 2, "S50kRemTbl", "CurveDelete");
            CHECK_COND(tRem != nullptr, "Suite 50k(a): tRem null");
            for (int r = 0; r < 8; ++r) {
                tRem->setCell(r, 0, double(r));
                tRem->setCell(r, 1, double(r * r));
            }
            MultiLayer *mlRem = w50->multilayerPlot(
                    tRem, QStringList() << "S50kRemTbl_2", Graph::Line);
            CHECK_COND(mlRem != nullptr && mlRem->activeGraph() != nullptr, "Suite 50k(a): mlRem null");
            Graph *gRem = mlRem->activeGraph();

            int directDeleted = 0;
            auto *pDirect = new ProbeDataCurve(tRem, tRem->colName(0), tRem->colName(1), &directDeleted);
            const int directKey = gRem->plotWidget()->insertCurve(pDirect);
            gRem->plotWidget()->removeCurve(directKey);
            CHECK_COND(directDeleted == 1,
                       "Suite 50k(a): Plot::removeCurve failed to delete detached QwtPlotItem!");

            int tblCloseDeleted = 0;
            auto *pOnTblClose =
                    new ProbeDataCurve(tRem, tRem->colName(0), tRem->colName(1), &tblCloseDeleted);
            gRem->insertPlotItem(pOnTblClose, Graph::Line);
            CHECK_COND(gRem->curves() == 2, "Suite 50k(a): expected 2 curves before closing Table");

            tRem->askOnCloseEvent(false);
            w50->closeWindow(tRem);
            CHECK_COND(tblCloseDeleted == 1 && gRem->curves() == 0,
                       "Suite 50k(a): closing Table before Graph leaked DataCurve!");
            mlRem->askOnCloseEvent(false);
            w50->closeWindow(mlRem);

            Matrix *mRem = w50->newMatrix("S50kRemMtx", 16, 16);
            CHECK_COND(mRem != nullptr, "Suite 50k(a): mRem null");
            for (int r = 0; r < 16; ++r)
                for (int c = 0; c < 16; ++c)
                    mRem->setCell(r, c, double(r + c));
            MultiLayer *mlHm = w50->plotSpectrogram(mRem, Graph::ColorMap);
            CHECK_COND(mlHm != nullptr && mlHm->activeGraph() != nullptr, "Suite 50k(a): mlHm null");
            Graph *gHm = mlHm->activeGraph();

            int mtxCloseDeleted = 0;
            auto *pSpOnClose = new ProbeSpectrogram(mRem, &mtxCloseDeleted);
            gHm->insertPlotItem(pSpOnClose, Graph::ColorMap);
            CHECK_COND(gHm->curves() == 2, "Suite 50k(a): expected 2 spectrograms before closing Matrix");

            mRem->askOnCloseEvent(false);
            w50->closeWindow(mRem);
            int remainingSpecItems = 0;
            for (QwtPlotItem *it : gHm->plotWidget()->itemList()) {
                if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
                    ++remainingSpecItems;
            }
            CHECK_COND(mtxCloseDeleted == 1 && gHm->curves() == 0 && remainingSpecItems == 0,
                       "Suite 50k(a): closing Matrix before Heatmap leaked Spectrogram!");
            mlHm->askOnCloseEvent(false);
            w50->closeWindow(mlHm);
        }

        // (b) 200,000-pt sin(i * 0.05) 600-DPI QImage export via MultiLayer::exportImage < 300 ms
        {
            constexpr int NU600 = 200000;
            Table *tU600 = w50->newTable(NU600, 2, "S50kUltra600Tbl", "Ultra600DPI");
            CHECK_COND(tU600 != nullptr, "Suite 50k(b): tU600 null");
            QVector<qreal> ux(NU600), uy(NU600);
            for (int i = 0; i < NU600; ++i) {
                ux[i] = double(i);
                uy[i] = std::sin(double(i) * 0.05);
            }
            tU600->column(0)->replaceValues(0, ux);
            tU600->column(1)->replaceValues(0, uy);

            MultiLayer *mlU600 = w50->multilayerPlot(
                    tU600, QStringList() << "S50kUltra600Tbl_2", Graph::Line);
            CHECK_COND(mlU600 != nullptr, "Suite 50k(b): mlU600 null");
            mlU600->applyStylePreset(MultiLayer::StyleISIJJournal85mm);

            const QString exp600Path = tmpDir.filePath("suite50k_ultra_600dpi.png");
            QElapsedTimer expTimer;
            expTimer.start();
            mlU600->exportImage(exp600Path, 100, false, 600);
            const qint64 exp600Ms = expTimer.elapsed();
            std::cout << "[INFO] Suite 50k(b) 200,000-pt sin(i*0.05) 600-DPI exportImage time = "
                      << exp600Ms << " ms\n";
            QImage expImg(exp600Path);
            CHECK_COND(!expImg.isNull() && expImg.width() >= 1800 && expImg.height() >= 1800,
                       "Suite 50k(b): 600-DPI exported PNG invalid or wrong dimensions!");
            CHECK_COND(exp600Ms < 300,
                       "Suite 50k(b): 200,000-pt 600-DPI MultiLayer::exportImage exceeded 300 ms budget!");
        }

        // (c) Pie + applyStylePreset(StyleISIJJournal85mm) keeps legend & pie non-overlapping
        {
            Table *tPie = w50->newTable(4, 2, "S50kPieTbl", "Phases");
            CHECK_COND(tPie != nullptr, "Suite 50k(c): tPie null");
            tPie->setColumnType(0, SciDAVis::ColumnMode::Text);
            const char *phaseNames[4] = { "Alpha-Al Matrix", "MgZn2 Precipitate", "Al3Zr Dispersoid", "S-Phase" };
            const double phasePcts[4] = { 56.0, 24.0, 12.0, 8.0 };
            for (int r = 0; r < 4; ++r) {
                tPie->setText(r, 0, QString::fromLatin1(phaseNames[r]));
                tPie->setCell(r, 1, phasePcts[r]);
            }
            MultiLayer *mlPie = w50->multilayerPlot(
                    tPie, QStringList() << "S50kPieTbl_2", Graph::Pie);
            CHECK_COND(mlPie != nullptr && mlPie->activeGraph() != nullptr, "Suite 50k(c): mlPie null");
            mlPie->resize(360, 360);
            mlPie->activeGraph()->plotWidget()->resize(322, 322);
            mlPie->applyStylePreset(MultiLayer::StyleISIJJournal85mm);

            Graph *gPie = mlPie->activeGraph();
            if (gPie->plotWidget()->width() < 200 || gPie->plotWidget()->height() < 200)
                gPie->plotWidget()->resize(322, 322);
            gPie->plotWidget()->updateLayout();
            gPie->replot();

            QImage pieImg(gPie->plotWidget()->size(), QImage::Format_ARGB32);
            pieImg.fill(Qt::white);
            {
                QPainter pp(&pieImg);
                gPie->exportPainter(pp, false, pieImg.rect());
            }
            // QwtPlot::print() invalidates plotLayout() on exit; reactivate layout before reading canvasRect()
            gPie->plotWidget()->updateLayout();

            Legend *legPie = gPie->legend();
            CHECK_COND(legPie != nullptr && gPie->isLegendVisible(),
                       "Suite 50k(c): Pie legend missing or hidden!");
            const QRect cr = gPie->plotWidget()->plotLayout()->canvasRect();
            QRect lr = legPie->rect();
            if (lr.width() <= 4 || lr.height() <= 4) {
                QwtText qtLeg(legPie->text());
                qtLeg.setFont(legPie->font());
                const QSize tsz = qtLeg.textSize();
                const QwtScaleMap xm = gPie->plotWidget()->canvasMap(legPie->xAxis());
                const QwtScaleMap ym = gPie->plotWidget()->canvasMap(legPie->yAxis());
                const int lx = qMax(0, qRound(xm.xTransform(legPie->xValue())) - cr.left());
                const int ly = qMax(0, qRound(ym.xTransform(legPie->yValue())) - cr.top());
                lr = QRect(lx, ly, qMax(24, tsz.width() + 28), qMax(16, tsz.height() + 8));
            }
            CHECK_COND(cr.width() > 80 && cr.height() > 80 && lr.width() > 20 && lr.height() > 10,
                       "Suite 50k(c): invalid Pie canvasRect or legend rect!");

            const double sCw = double(cr.width());
            const double sCh = double(cr.height());
            const QVariant ancProp = gPie->plotWidget()->property("_toriLegendAnchor");
            const bool topBandLegend =
                    (ancProp.isValid() && ancProp.toInt() == int(Graph::LegendTopCenter))
                    || (double(lr.width()) / sCw > 0.34);
            double radius = 0.0, pieCx = 0.0, pieCy = 0.0;
            if (topBandLegend) {
                const double legBottomFrac =
                        qBound(0.08, double(qMax(4, lr.top()) + lr.height()) / sCh, 0.52);
                const double legBottom = legBottomFrac * sCh;
                const double pad = 0.028 * sCh;
                const double availH = qMax(0.20 * sCh, sCh - legBottom - 2.0 * pad);
                radius = qMax(10.0, qMin(0.31 * sCw, 0.44 * availH));
                pieCx = 0.5 * sCw;
                pieCy = legBottom + pad + 0.5 * availH;
            } else {
                radius = 0.285 * qMin(sCw, sCh);
                pieCx = 0.36 * sCw;
                pieCy = 0.52 * sCh;
            }
            const QRectF pieDiscInCanvas(pieCx - radius, pieCy - radius, 2.0 * radius, 2.0 * radius);
            CHECK_COND(!QRectF(lr).intersects(pieDiscInCanvas),
                       "Suite 50k(c): Pie legend rect intersects Pie disc after StyleISIJJournal85mm!");

            // Verify rendered Pie disc contains slice color pixels while legend side-bands stay clear
            int pieDiscChromaticPx = 0;
            const int discCxPx = qBound(0, cr.left() + qRound(pieCx), pieImg.width() - 1);
            const int discCyPx = qBound(0, cr.top() + qRound(pieCy), pieImg.height() - 1);
            for (int dy = -qRound(0.5 * radius); dy <= qRound(0.5 * radius); ++dy) {
                const int py = qBound(0, discCyPx + dy, pieImg.height() - 1);
                for (int dx = -qRound(0.5 * radius); dx <= qRound(0.5 * radius); ++dx) {
                    const int px = qBound(0, discCxPx + dx, pieImg.width() - 1);
                    const QColor c = pieImg.pixelColor(px, py);
                    if (qMax(c.red(), qMax(c.green(), c.blue()))
                                - qMin(c.red(), qMin(c.green(), c.blue()))
                        > 35) {
                        ++pieDiscChromaticPx;
                    }
                }
            }
            CHECK_COND(pieDiscChromaticPx > 200,
                       "Suite 50k(c): rendered ISIJ Pie disc had insufficient slice pixels!");
        }
    }

    // -------------------------------------------------------------------------
    // 50l. Mega-Wave 5 Surgical Regression Gate:
    //      (a) Table::colLabel(-1) & colName(-1) return "" without crash
    //      (b) PythonAutomationServer::generatePythonScriptForWindow on Pie, Box,
    //          and Histogram windows returns non-empty valid Python scripts (0 crash)
    //      (c) Reloaded <Pie> .sciprj has 0 outer canvas frame
    // -------------------------------------------------------------------------
    {
        // (a) Negative & out-of-bounds Table column-index helper guards
        Table *t50l = w50->newTable(12, 3, "S50lSpecTbl", "SpecialtyCodegen");
        CHECK_COND(t50l != nullptr, "Suite 50l(a): t50l null");
        CHECK_COND(t50l->colLabel(-1).isEmpty() && t50l->colLabel(99).isEmpty()
                           && t50l->colName(-1).isEmpty() && t50l->colName(99).isEmpty(),
                   "Suite 50l(a): Table::colLabel(-1)/colName(-1) failed to return empty QString!");

        t50l->setColumnType(0, SciDAVis::ColumnMode::Text);
        for (int r = 0; r < 12; ++r) {
            t50l->setText(r, 0, QString("Cat_%1").arg(r + 1));
            t50l->setCell(r, 1, 15.0 + 3.5 * double(r));
            t50l->setCell(r, 2, 22.0 + 2.0 * double((r * 5) % 7));
        }

        // (b) generatePythonScriptForWindow on Pie, Box, and Histogram (single-column DataCurves)
        MultiLayer *mlPie50l = w50->multilayerPlot(
                t50l, QStringList() << "S50lSpecTbl_2", Graph::Pie);
        CHECK_COND(mlPie50l != nullptr, "Suite 50l(b): mlPie50l null");
        mlPie50l->setName("S50lPieWin");
        const QString pyPie =
                PythonAutomationServer::generatePythonScriptForWindow(w50, mlPie50l, true, "inline");
        CHECK_COND(!pyPie.trimmed().isEmpty() && pyPie.contains("plot_pie"),
                   "Suite 50l(b): generatePythonScriptForWindow failed or omitted plot_pie on Pie!");

        MultiLayer *mlBox50l = w50->multilayerPlot(
                t50l, QStringList() << "S50lSpecTbl_2" << "S50lSpecTbl_3", Graph::Box);
        CHECK_COND(mlBox50l != nullptr, "Suite 50l(b): mlBox50l null");
        mlBox50l->setName("S50lBoxWin");
        const QString pyBox =
                PythonAutomationServer::generatePythonScriptForWindow(w50, mlBox50l, true, "inline");
        CHECK_COND(!pyBox.trimmed().isEmpty() && pyBox.contains("plot_box"),
                   "Suite 50l(b): generatePythonScriptForWindow failed or omitted plot_box on Box!");

        MultiLayer *mlHist50l = w50->multilayerPlot(
                t50l, QStringList() << "S50lSpecTbl_2", Graph::Histogram);
        CHECK_COND(mlHist50l != nullptr, "Suite 50l(b): mlHist50l null");
        mlHist50l->setName("S50lHistWin");
        const QString pyHist =
                PythonAutomationServer::generatePythonScriptForWindow(w50, mlHist50l, true, "inline");
        CHECK_COND(!pyHist.trimmed().isEmpty() && pyHist.contains("plot_histogram"),
                   "Suite 50l(b): generatePythonScriptForWindow failed or omitted plot_histogram on Histogram!");

        // (c) Reloaded <Pie> .sciprj has 0 outer canvas frame
        {
            ApplicationWindow *pieWriter = new ApplicationWindow();
            pieWriter->setHeadlessMode(true);
            pieWriter->applyUserSettings();
            Table *tPW = pieWriter->newTable(4, 2, "S50lPiePrjTbl", "PieFrame");
            tPW->setColumnType(0, SciDAVis::ColumnMode::Text);
            for (int r = 0; r < 4; ++r) {
                tPW->setText(r, 0, QString("Phase%1").arg(r + 1));
                tPW->setCell(r, 1, 10.0 * double(r + 1));
            }
            MultiLayer *mlPW = pieWriter->multilayerPlot(
                    tPW, QStringList() << "S50lPiePrjTbl_2", Graph::Pie);
            CHECK_COND(mlPW != nullptr, "Suite 50l(c): mlPW null");
            mlPW->setName("S50lPieReloadGraph");

            const QString piePrjPath = tmpDir.filePath("suite50l_pie_frame.sciprj");
            {
                QFile pf(piePrjPath);
                CHECK_COND(pf.open(QIODevice::WriteOnly), "Suite 50l(c): open pie .sciprj failed");
                pf.write(pieWriter->saveProjectToByteArray());
                pf.close();
            }
            pieWriter->savedProject();
            pieWriter->setOleEmbeddedMode(true);
            delete pieWriter;

            ApplicationWindow *pieReader = new ApplicationWindow();
            pieReader->setHeadlessMode(true);
            pieReader->applyUserSettings();
            CHECK_COND(pieReader->loadProject(piePrjPath),
                       "Suite 50l(c): loadProject failed on suite50l_pie_frame.sciprj!");
            MultiLayer *mlPR =
                    qobject_cast<MultiLayer *>(pieReader->window("S50lPieReloadGraph"));
            CHECK_COND(mlPR != nullptr && mlPR->activeGraph() != nullptr,
                       "Suite 50l(c): S50lPieReloadGraph missing after reload!");
            Graph *gPR = mlPR->activeGraph();
            CHECK_COND(gPR->isPiePlot() && !gPR->framed() && gPR->canvasFrameWidth() == 0
                               && !gPR->axesBackbones()
                               && gPR->plotWidget()->lineWidth() == 0,
                       "Suite 50l(c): Reloaded <Pie> .sciprj retained outer canvas/plot frame!");

            pieReader->savedProject();
            pieReader->setOleEmbeddedMode(true);
            delete pieReader;
        }
    }

    w50->savedProject();
    w50->setOleEmbeddedMode(true);
    delete w50;
    QApplication::processEvents();

    std::cout << "[INFO] Suite 50 MegaWave1_Plots_MatSci_Autonomy_Undo_Perf passed!\n";
    return 0;
}

int runSuite51Campaign6FinalEndToEndInvariants(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 51: Campaign6_Final_EndToEnd_Invariants...\n";
    Q_UNUSED(mw);

    ApplicationWindow *w51 = new ApplicationWindow();
    w51->setHeadlessMode(true);
    w51->applyUserSettings();
    w51->switchToLanguage("en");
    QApplication::processEvents();

    // -------------------------------------------------------------------------
    // 51a. EmfPaintDevice rendering "Minimum creep rate dε/dt / s^-1" (both -90 deg
    //      Y-axis title and horizontal legend) produces valid non-empty EMF+ bytes
    // -------------------------------------------------------------------------
    {
        Table *tCreep = w51->newTable(6, 2, "S51CreepTbl", "NortonCreep");
        CHECK_COND(tCreep != nullptr, "Suite 51a: tCreep null");
        for (int r = 0; r < 6; ++r) {
            tCreep->setCell(r, 0, 120.0 + 30.0 * double(r));
            tCreep->setCell(r, 1, 1.0e-7 * std::pow(10.0, 0.45 * double(r)));
        }
        MultiLayer *mlCreep = w51->multilayerPlot(
                tCreep, QStringList() << "S51CreepTbl_2", Graph::LineSymbols);
        CHECK_COND(mlCreep != nullptr && mlCreep->activeGraph() != nullptr, "Suite 51a: mlCreep null");
        mlCreep->resize(420, 420);
        Graph *gCreep = mlCreep->activeGraph();
        gCreep->plotWidget()->resize(340, 340);
        const QString creepYTitle =
                QString::fromUtf8("Minimum creep rate d\xCE\xB5/dt / s^-1");
        gCreep->setAxisTitle(QwtPlot::yLeft, creepYTitle);
        gCreep->setAxisTitle(QwtPlot::xBottom, QStringLiteral("Applied stress / MPa"));
        if (gCreep->legend())
            gCreep->legend()->setText(
                    QString::fromUtf8("\\c{1}Minimum creep rate d\xCE\xB5/dt"));
        mlCreep->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        gCreep->replot();

        const OleRenderedGraph emfOut = SciDAVisEmf::renderMultiLayer(mlCreep, nullptr, false);
        CHECK_COND(emfOut.emfBytes.size() > 1024 && emfOut.emfBytes.contains("EMF+"),
                   "Suite 51a: EmfPaintDevice failed to emit non-empty Dual EMF+ bytes for 'Minimum creep rate dε/dt / s^-1'!");
        HENHMETAFILE hemf = SciDAVisEmf::createHemfFromBytes(emfOut.emfBytes);
        CHECK_COND(hemf != nullptr, "Suite 51a: createHemfFromBytes returned null HENHMETAFILE!");

        QImage gdiPlay(480, 480, QImage::Format_ARGB32);
        gdiPlay.fill(Qt::white);
        {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = 480;
            bmi.bmiHeader.biHeight = -480;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            void *bits = nullptr;
            HDC screenDc = GetDC(nullptr);
            HDC memDc = CreateCompatibleDC(screenDc);
            HBITMAP hDib = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
            ReleaseDC(nullptr, screenDc);
            if (memDc && hDib && bits) {
                HGDIOBJ oldBmp = SelectObject(memDc, hDib);
                HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
                RECT rc = { 0, 0, 480, 480 };
                FillRect(memDc, &rc, whiteBrush);
                DeleteObject(whiteBrush);
                CHECK_COND(SciDAVisEmf::drawHemfWithGdiplus(memDc, hemf, rc),
                           "Suite 51a: GDI+ playback of 'Minimum creep rate dε/dt / s^-1' EMF+ failed!");
                std::memcpy(gdiPlay.bits(), bits, 480 * 480 * 4);
                SelectObject(memDc, oldBmp);
                DeleteObject(hDib);
                DeleteDC(memDc);
            }
        }
        DeleteEnhMetaFile(hemf);

        int darkInkPx = 0;
        for (int y = 0; y < gdiPlay.height(); ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(gdiPlay.constScanLine(y));
            for (int x = 0; x < gdiPlay.width(); ++x) {
                if (qRed(line[x]) < 120 && qGreen(line[x]) < 120 && qBlue(line[x]) < 120)
                    ++darkInkPx;
            }
        }
        CHECK_COND(darkInkPx > 300,
                   "Suite 51a: EMF+ GDI+ playback produced insufficient dark text/axis ink pixels!");

        // Verify [P1-05]: <sub>/<sup> runs in "<i>E</i> - <i>E</i><sub>F</sub> / eV" (28 pt)
        // and "ln(<i>β</i> / <i>T</i><sub>p</sub><sup>2</sup>)" (11 pt) @ 300 DPI emit
        // EmfPlusFont emSize in [0.58, 0.80]x parent emSize (never >= 0.90x!)
        auto extractEmfPlusDrawnFontSizes =
                [](const QByteArray &bytes) -> QVector<QPair<QString, float>> {
            QVector<QPair<QString, float>> runs;
            QMap<int, float> fontEmBySlot;
            const uchar *raw = reinterpret_cast<const uchar *>(bytes.constData());
            const size_t total = static_cast<size_t>(bytes.size());
            size_t pos = 0;
            while (pos + 8 <= total) {
                quint32 iType = 0, nSize = 0;
                std::memcpy(&iType, raw + pos, 4);
                std::memcpy(&nSize, raw + pos + 4, 4);
                if (nSize < 8 || pos + nSize > total)
                    break;
                if (iType == 70 && nSize >= 16) { // EMR_COMMENT
                    quint32 cbData = 0, ident = 0;
                    std::memcpy(&cbData, raw + pos + 8, 4);
                    std::memcpy(&ident, raw + pos + 12, 4);
                    if (ident == 0x2B464D45u) { // "EMF+"
                        const size_t epEnd =
                                std::min<size_t>(pos + nSize, pos + 12 + size_t(cbData));
                        size_t ep = pos + 16;
                        while (ep + 12 <= epEnd) {
                            quint16 recType = 0, recFlags = 0;
                            quint32 recSize = 0, dataSize = 0;
                            std::memcpy(&recType, raw + ep, 2);
                            std::memcpy(&recFlags, raw + ep + 2, 2);
                            std::memcpy(&recSize, raw + ep + 4, 4);
                            std::memcpy(&dataSize, raw + ep + 8, 4);
                            if (recSize < 12 || ep + recSize > epEnd
                                || ep + 12 + size_t(dataSize) > ep + recSize)
                                break;
                            if (recType == 0x4008 && (recFlags & 0x8000) == 0
                                && ((recFlags >> 8) & 0x7F) == 0x06 && dataSize >= 24) {
                                float emSz = 0.0f;
                                std::memcpy(&emSz, raw + ep + 12 + 4, sizeof(float));
                                fontEmBySlot[int(recFlags & 0xFF)] = emSz;
                            } else if (recType == 0x401C && dataSize >= 28) {
                                quint32 strLen = 0;
                                std::memcpy(&strLen, raw + ep + 12 + 8, 4);
                                if (28u + size_t(strLen) * 2u <= size_t(dataSize)) {
                                    const QString s = QString::fromUtf16(
                                            reinterpret_cast<const ushort *>(raw + ep + 12 + 28),
                                            int(strLen));
                                    runs.append(qMakePair(
                                            s, fontEmBySlot.value(int(recFlags & 0xFF), 0.0f)));
                                }
                            }
                            ep += recSize;
                        }
                    }
                }
                pos += nSize;
            }
            return runs;
        };

        mlCreep->applyStylePreset(MultiLayer::StylePresentationSlide28pt);
        mlCreep->setPageSizePhysical(160.0, 120.0, MultiLayer::UnitMm, 300);
        gCreep->setLegendVisible(false);
        gCreep->setAxisTitle(QwtPlot::xBottom,
                             QStringLiteral("<i>E</i> - <i>E</i><sub>F</sub> / eV"));
        gCreep->setAxisTitle(QwtPlot::yLeft, QString());
        gCreep->replot();
        const OleRenderedGraph emfSlide28 =
                SciDAVisEmf::renderMultiLayer(mlCreep, nullptr, false);
        const auto runs28 = extractEmfPlusDrawnFontSizes(emfSlide28.emfBytes);
        double ratioSubF28 = -1.0;
        for (int i = 0; i + 1 < runs28.size(); ++i) {
            if (runs28[i].first.trimmed() == QLatin1String("E")
                && runs28[i + 1].first.trimmed() == QLatin1String("F")
                && runs28[i].second > 1.0f) {
                ratioSubF28 = double(runs28[i + 1].second) / double(runs28[i].second);
                break;
            }
        }
        std::cout << "[INFO] Suite 51a 28pt 300-DPI EmfPlusFont ratio(F/E) = " << ratioSubF28
                  << "\n";
        CHECK_COND(ratioSubF28 >= 0.58 && ratioSubF28 <= 0.80,
                   "Suite 51a: 28pt 300-DPI '<i>E</i> - <i>E</i><sub>F</sub> / eV' EmfPlusFont emSize ratio for <sub>F</sub> outside [0.58, 0.80]!");

        mlCreep->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        mlCreep->setPageSizePhysical(85.0, 85.0, MultiLayer::UnitMm, 300);
        gCreep->setLegendVisible(false);
        gCreep->setAxisTitle(
                QwtPlot::yLeft,
                QString::fromUtf8("ln(<i>\xCE\xB2</i> / <i>T</i><sub>p</sub><sup>2</sup>)"));
        gCreep->setAxisTitle(QwtPlot::xBottom, QString());
        gCreep->replot();
        const OleRenderedGraph emfIsij11 =
                SciDAVisEmf::renderMultiLayer(mlCreep, nullptr, false);
        const auto runs11 = extractEmfPlusDrawnFontSizes(emfIsij11.emfBytes);
        double ratioSubP11 = -1.0;
        double ratioSup211 = -1.0;
        for (int i = 0; i + 2 < runs11.size(); ++i) {
            if (runs11[i].first.trimmed() == QLatin1String("T")
                && runs11[i + 1].first.trimmed() == QLatin1String("p")
                && runs11[i + 2].first.trimmed() == QLatin1String("2")
                && runs11[i].second > 1.0f) {
                ratioSubP11 = double(runs11[i + 1].second) / double(runs11[i].second);
                ratioSup211 = double(runs11[i + 2].second) / double(runs11[i].second);
                break;
            }
        }
        std::cout << "[INFO] Suite 51a 11pt 300-DPI EmfPlusFont ratio(p/T)=" << ratioSubP11
                  << ", ratio(2/T)=" << ratioSup211 << "\n";
        CHECK_COND(ratioSubP11 >= 0.58 && ratioSubP11 <= 0.80
                           && ratioSup211 >= 0.58 && ratioSup211 <= 0.80,
                   "Suite 51a: 11pt 300-DPI 'ln(<i>β</i> / <i>T</i><sub>p</sub><sup>2</sup>)' EmfPlusFont emSize ratio for <sub>p</sub>/<sup>2</sup> outside [0.58, 0.80]!");
    }

    // -------------------------------------------------------------------------
    // 51b. HorizontalBars 2-column legend wider than canvas automatically
    //      downgrades leg->numColumns() == 1
    // -------------------------------------------------------------------------
    {
        Table *tHB = w51->newTable(4, 3, "S51HorizBarTbl", "FractureModes");
        CHECK_COND(tHB != nullptr, "Suite 51b: tHB null");
        tHB->setColumnType(0, SciDAVis::ColumnMode::Text);
        tHB->setColPlotDesignation(0, SciDAVis::X);
        tHB->setColName(1, "SeriesA");
        tHB->setColName(2, "SeriesB");
        tHB->setColLongName(1, "Interface debonding fracture");
        tHB->setColLongName(2, "Transgranular cleavage fracture");
        const char *tempers[4] = { "As-Q", "T6", "T73", "RRA" };
        for (int r = 0; r < 4; ++r) {
            tHB->setText(r, 0, QString::fromLatin1(tempers[r]));
            tHB->setCell(r, 1, 24.0 + 6.0 * double(r));
            tHB->setCell(r, 2, 31.0 + 5.0 * double(r));
        }

        MultiLayer *mlHB = w51->multilayerPlot(
                tHB, QStringList() << "S51HorizBarTbl_SeriesA" << "S51HorizBarTbl_SeriesB",
                Graph::HorizontalBars);
        CHECK_COND(mlHB != nullptr && mlHB->activeGraph() != nullptr, "Suite 51b: mlHB null");
        mlHB->resize(340, 340);
        Graph *gHB = mlHB->activeGraph();
        gHB->plotWidget()->resize(280, 280);
        gHB->plotWidget()->updateLayout();
        Legend *legHB = gHB->legend();
        CHECK_COND(legHB != nullptr, "Suite 51b: HorizontalBars legend null!");
        legHB->setText("\\c{1}Interface debonding fracture\n\\c{2}Transgranular cleavage fracture");
        legHB->setNumColumns(2);
        CHECK_COND(legHB->numColumns() == 2, "Suite 51b: precondition numColumns == 2 failed");

        gHB->placeLegendAnchor(Graph::LegendTopRight);
        CHECK_COND(legHB->numColumns() == 1,
                   "Suite 51b: HorizontalBars 2-column legend wider than canvas failed to auto-downgrade to 1 column!");
    }

    // -------------------------------------------------------------------------
    // 51c. Graph::refreshAutoAxisTitlesFromColumns preserves concise colShort
    //      legend on a narrow < 190 px canvas when colLongName would overflow
    // -------------------------------------------------------------------------
    {
        Table *tNar = w51->newTable(20, 2, "S51NarrowHistTbl", "Precipitates");
        CHECK_COND(tNar != nullptr, "Suite 51c: tNar null");
        tNar->setColName(0, "Idx");
        tNar->setColName(1, "d_T6");
        tNar->setColLongName(1, "Precipitate equivalent diameter d_eq (Peak-Aged T6 Condition)");
        tNar->setColUnits(1, "nm");
        for (int r = 0; r < 20; ++r) {
            tNar->setCell(r, 0, double(r));
            tNar->setCell(r, 1, 8.5 + 0.7 * double(r % 7));
        }

        MultiLayer *mlNar = w51->multilayerPlot(
                tNar, QStringList() << "S51NarrowHistTbl_d_T6", Graph::Line);
        CHECK_COND(mlNar != nullptr && mlNar->activeGraph() != nullptr, "Suite 51c: mlNar null");
        mlNar->resize(230, 230);
        Graph *gNar = mlNar->activeGraph();
        gNar->plotWidget()->resize(210, 210);
        gNar->plotWidget()->updateLayout();
        const QRect narCr = gNar->plotWidget()->plotLayout()->canvasRect();
        CHECK_COND(narCr.width() > 0 && narCr.width() < 190,
                   "Suite 51c: test setup canvas width should be < 190 px!");

        Legend *legNar = gNar->legend();
        CHECK_COND(legNar != nullptr, "Suite 51c: narrow graph legend null!");
        legNar->setFont(QFont("Arial", 9));
        legNar->setText("\\c{1}d_T6");

        gNar->refreshAutoAxisTitlesFromColumns();
        const QString postLegText = legNar->text();
        CHECK_COND(postLegText.contains("d_T6")
                           && !postLegText.contains("Precipitate equivalent diameter"),
                   "Suite 51c: refreshAutoAxisTitlesFromColumns expanded concise 'd_T6' into overflowing colLongName on < 190 px canvas!");
    }

    // -------------------------------------------------------------------------
    // 51d. ArrowMarker::updateBoundingRect preserves exact data-space
    //      (x1, y1, x2, y2) across a 500x400 -> 220x220 canvas resize ([P1-01])
    // -------------------------------------------------------------------------
    {
        Table *tBrk = w51->newTable(3, 2, "S51ArrowBrkTbl", "AnovaGroups");
        CHECK_COND(tBrk != nullptr, "Suite 51d: tBrk null");
        for (int r = 0; r < 3; ++r) {
            tBrk->setCell(r, 0, double(r + 1));
            tBrk->setCell(r, 1, 480.0 + 35.0 * double(r));
        }
        MultiLayer *mlBrk = w51->multilayerPlot(
                tBrk, QStringList() << "S51ArrowBrkTbl_2", Graph::Scatter);
        CHECK_COND(mlBrk != nullptr && mlBrk->activeGraph() != nullptr, "Suite 51d: mlBrk null");
        mlBrk->resize(560, 460);
        Graph *gBrk = mlBrk->activeGraph();
        gBrk->plotWidget()->resize(500, 400);
        gBrk->setScale(QwtPlot::xBottom, 0.5, 3.5);
        gBrk->setScale(QwtPlot::yLeft, 420.0, 660.0);
        gBrk->plotWidget()->updateLayout();
        gBrk->replot();

        ArrowMarker proto;
        proto.drawStartArrow(false);
        proto.drawEndArrow(false);
        proto.setBoundingRect(1.0, 625.0, 3.0, 625.0);
        gBrk->addArrow(&proto);
        CHECK_COND(!gBrk->lineMarkerKeys().isEmpty(), "Suite 51d: lineMarkerKeys empty after addArrow");
        ArrowMarker *brk = gBrk->arrow(gBrk->lineMarkerKeys().back());
        CHECK_COND(brk != nullptr, "Suite 51d: attached ArrowMarker null");

        const QPoint pxBeforeStart = brk->startPoint();
        const QPoint pxBeforeEnd = brk->endPoint();

        // Resize canvas from 500x400 down to 220x220 and trigger updateMarkersBoundingRect + ISIJ preset
        mlBrk->resize(260, 260);
        gBrk->plotWidget()->resize(220, 220);
        gBrk->plotWidget()->updateLayout();
        gBrk->replot();
        gBrk->updateMarkersBoundingRect();
        brk->updateBoundingRect();

        CHECK_COND(std::abs(brk->startPointCoord().x() - 1.0) < 1e-9
                           && std::abs(brk->startPointCoord().y() - 625.0) < 1e-9
                           && std::abs(brk->endPointCoord().x() - 3.0) < 1e-9
                           && std::abs(brk->endPointCoord().y() - 625.0) < 1e-9,
                   "Suite 51d: ArrowMarker::updateBoundingRect corrupted data-space (x1, y1, x2, y2) across 500x400 -> 220x220 resize!");
        CHECK_COND(brk->startPoint() != pxBeforeStart || brk->endPoint() != pxBeforeEnd,
                   "Suite 51d: ArrowMarker::updateBoundingRect failed to refresh pixel start/end cache for 220x220 canvas!");

        mlBrk->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        gBrk->replot();
        CHECK_COND(std::abs(brk->startPointCoord().x() - 1.0) < 1e-9
                           && std::abs(brk->startPointCoord().y() - 625.0) < 1e-9
                           && std::abs(brk->endPointCoord().x() - 3.0) < 1e-9
                           && std::abs(brk->endPointCoord().y() - 625.0) < 1e-9,
                   "Suite 51d: StyleISIJJournal85mm corrupted ArrowMarker data-space coordinates!");
    }

    // -------------------------------------------------------------------------
    // 51e. PowerPoint OLE Container Simulation: Multi-Graph DoVerb Target
    //      Activation ("Fig_2" -> "Fig-2"), Slide-Box SetExtent Preservation
    //      Across Slide 28pt Preset, Compact Storage & On-Demand CF_DIB, and
    //      Full-Page Ctrl+C (copyActiveLayer) OLE Clipboard Verification
    // -------------------------------------------------------------------------
    {
        struct MockPptClientSite final : public IOleClientSite, public IAdviseSink {
            ULONG refs = 1;
            int saveObjectCount = 0;
            int showWindowCount = 0;
            int viewChangeCount = 0;

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

            // IOleClientSite
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
                return E_NOINTERFACE;
            }
            HRESULT STDMETHODCALLTYPE ShowObject() override { return S_OK; }
            HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL) override
            {
                ++showWindowCount;
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() override { return S_OK; }

            // IAdviseSink
            void STDMETHODCALLTYPE OnDataChange(FORMATETC *, STGMEDIUM *) override {}
            void STDMETHODCALLTYPE OnViewChange(DWORD, LONG) override { ++viewChangeCount; }
            void STDMETHODCALLTYPE OnRename(IMoniker *) override {}
            void STDMETHODCALLTYPE OnSave() override {}
            void STDMETHODCALLTYPE OnClose() override {}
        };

        ApplicationWindow *oleSrcApp = new ApplicationWindow();
        oleSrcApp->setHeadlessMode(true);
        oleSrcApp->setOleEmbeddedMode(true);
        oleSrcApp->applyUserSettings();

        Table *tOle = oleSrcApp->newTable(5, 3, "OleMultiTbl", "OleMultiData");
        CHECK_COND(tOle != nullptr, "Suite 51e: tOle null");
        for (int r = 0; r < 5; ++r) {
            tOle->setCell(r, 0, double(r + 1));
            tOle->setCell(r, 1, double(r + 1) * 10.0);
            tOle->setCell(r, 2, double(r + 1) * 25.0);
        }

        MultiLayer *mlFig1 = oleSrcApp->multilayerPlot(
                tOle, QStringList() << "OleMultiTbl_2", Graph::LineSymbols);
        MultiLayer *mlFig2 = oleSrcApp->multilayerPlot(
                tOle, QStringList() << "OleMultiTbl_3", Graph::LineSymbols);
        MultiLayer *mlFig3 = oleSrcApp->multilayerPlot(
                tOle, QStringList() << "OleMultiTbl_2", Graph::Line);
        CHECK_COND(mlFig1 && mlFig2 && mlFig3, "Suite 51e: failed to create 3 graphs");
        oleSrcApp->renameWindow(mlFig1, "Fig_1");
        oleSrcApp->renameWindow(mlFig2, "Fig_2");
        oleSrcApp->renameWindow(mlFig3, "Fig_3");
        // Verify MyWidget::setName normalized "_" to "-"
        CHECK_COND(mlFig2->name() == QStringLiteral("Fig-2"),
                   "Suite 51e: expected Fig_2 canonical name to be Fig-2");
        mlFig2->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        mlFig2->activeGraph()->setXAxisTitle("Strain, <i>e</i> (%)");
        mlFig2->activeGraph()->setYAxisTitle("Stress, <i>&sigma;</i> / MPa");

        const OleRenderedGraph rg2 = SciDAVisEmf::renderMultiLayer(mlFig2, nullptr, false);
        CHECK_COND(!rg2.emfBytes.isEmpty(), "Suite 51e: renderMultiLayer(mlFig2) empty");

        SciDAVisOle::OleGraphPayload pay2;
        pay2.projectBytes = oleSrcApp->saveProjectToByteArray();
        // Intentionally pass un-normalized "Fig_2" to verify underscore/hyphen normalization in DoVerb & syncFromEditor!
        pay2.targetWindow = QStringLiteral("Fig_2");
        pay2.targetLayer = 0;
        pay2.emfBytes = rg2.emfBytes;
        pay2.extentHm = rg2.extentHm;
        pay2.pixelSize = rg2.pixelSize;

        ComPtr<IOleObject> oleObj2;
        CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay2, IID_PPV_ARGS(&oleObj2))) && oleObj2,
                   "Suite 51e: createGraphObject failed");

        MockPptClientSite mockSite;
        CHECK_COND(SUCCEEDED(oleObj2->SetClientSite(&mockSite)), "Suite 51e: SetClientSite failed");
        ComPtr<IViewObject2> viewObj2;
        CHECK_COND(SUCCEEDED(oleObj2.As(&viewObj2)) && viewObj2, "Suite 51e: QI(IViewObject2) failed");
        CHECK_COND(SUCCEEDED(viewObj2->SetAdvise(DVASPECT_CONTENT, 0, &mockSite)),
                   "Suite 51e: IViewObject2::SetAdvise failed");

        // Simulate PowerPoint resizing the OLE box on the slide to 12.0 x 12.0 cm (12000 x 12000 HIMETRIC)
        SIZEL slideBoxHm{ 12000, 12000 };
        CHECK_COND(SUCCEEDED(oleObj2->SetExtent(DVASPECT_CONTENT, &slideBoxHm)),
                   "Suite 51e: IOleObject::SetExtent(12000x12000) failed");

        // Simulate PowerPoint double-click (DoVerb OLEIVERB_PRIMARY)
        CHECK_COND(SUCCEEDED(oleObj2->DoVerb(OLEIVERB_PRIMARY, nullptr, &mockSite, 0, nullptr, nullptr)),
                   "Suite 51e: IOleObject::DoVerb(OLEIVERB_PRIMARY) failed");
        QApplication::processEvents();

        // Find the opened OLE editor ApplicationWindow and verify:
        // (1) Fig-2 (NOT Fig-3, which was the last window in the project!) is the active subwindow after show()
        // (2) Apply Slide 28pt preset + edit worksheet Table while Table is active, then close editor!
        ApplicationWindow *openedOleEditor = nullptr;
        for (QWidget *tlw : QApplication::topLevelWidgets()) {
            if (auto *cand = qobject_cast<ApplicationWindow *>(tlw)) {
                if (cand != mw && cand != w51 && cand != oleSrcApp
                    && cand->windowTitle().contains(QStringLiteral("PowerPoint OLE"))) {
                    openedOleEditor = cand;
                    break;
                }
            }
        }
        CHECK_COND(openedOleEditor != nullptr,
                   "Suite 51e: DoVerb did not open ToriGraph [PowerPoint OLE] editor window!");
        // currentSubWindow(), not activeSubWindow(): QMdiArea reports an active subwindow only
        // while its top-level window is the active (foreground) window, which Windows may refuse
        // when another program has the focus (another test run, an editor). The current subwindow
        // is the one DoVerb selected in either case.
        MyWidget *activeInEditor = qobject_cast<MyWidget *>(openedOleEditor->d_workspace.currentSubWindow());
        CHECK_COND(activeInEditor != nullptr && activeInEditor->name() == QStringLiteral("Fig-2"),
                   "Suite 51e: DoVerb in multi-graph project failed to activate target 'Fig-2' after show()!");

        MultiLayer *editorFig2 = qobject_cast<MultiLayer *>(activeInEditor);
        CHECK_COND(editorFig2 != nullptr && editorFig2->activeGraph() != nullptr,
                   "Suite 51e: active subwindow in OLE editor is not MultiLayer Fig-2!");
        editorFig2->applyStylePreset(MultiLayer::StylePresentationSlide28pt);
        editorFig2->activeGraph()->setXAxisTitle("Updated Strain, <i>e</i> (%)");

        // Switch active tab in editor to Table ("OleMultiTbl") and modify a cell before closing,
        // verifying that syncFromEditor still renders Fig-2 (with updated plot data) rather than failing!
        for (MyWidget *ew : openedOleEditor->windowsList()) {
            if (Table *et = qobject_cast<Table *>(ew)) {
                openedOleEditor->activateSubWindow(et);
                et->setCell(4, 2, 180.0);
                break;
            }
        }
        QApplication::processEvents();

        // Close via IOleObject::Close(OLECLOSE_SAVEIFDIRTY) (simulating PowerPoint closing/saving OLE session)
        CHECK_COND(SUCCEEDED(oleObj2->Close(OLECLOSE_SAVEIFDIRTY)),
                   "Suite 51e: IOleObject::Close(OLECLOSE_SAVEIFDIRTY) failed");
        QApplication::processEvents();

        CHECK_COND(mockSite.saveObjectCount >= 1,
                   "Suite 51e: IOleClientSite::SaveObject was not invoked on OLE editor update/close!");

        // Verify slide box extent (12000 x 12000) was preserved without jumping to 18500 x 18500!
        SIZEL postEditExtent{};
        CHECK_COND(SUCCEEDED(viewObj2->GetExtent(DVASPECT_CONTENT, -1, nullptr, &postEditExtent)),
                   "Suite 51e: GetExtent after DoVerb close failed");
        CHECK_COND(postEditExtent.cx == 12000 && postEditExtent.cy == 12000,
                   "Suite 51e: Slide box extent (12000x12000) jumped after applying Slide 28pt preset in OLE editor!");

        // Save to IStorage and verify:
        // (a) No bloated SciDAVisDib stream is written to persistent storage
        // (b) On-demand GetData(CF_DIB) still succeeds via synthesizeDibFromEmf
        // (c) Reloaded project contains "Updated Strain, <i>e</i> (%)" at 28 pt Arial
        ComPtr<ILockBytes> lb51;
        CHECK_COND(SUCCEEDED(CreateILockBytesOnHGlobal(nullptr, TRUE, &lb51)) && lb51,
                   "Suite 51e: CreateILockBytesOnHGlobal failed");
        ComPtr<IStorage> stg51;
        CHECK_COND(SUCCEEDED(StgCreateDocfileOnILockBytes(
                           lb51.Get(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg51))
                           && stg51,
                   "Suite 51e: StgCreateDocfileOnILockBytes failed");
        ComPtr<IPersistStorage> persist51;
        CHECK_COND(SUCCEEDED(oleObj2.As(&persist51)) && persist51, "Suite 51e: QI(IPersistStorage) failed");
        CHECK_COND(SUCCEEDED(persist51->Save(stg51.Get(), FALSE)), "Suite 51e: IPersistStorage::Save failed");
        CHECK_COND(SUCCEEDED(persist51->SaveCompleted(stg51.Get())), "Suite 51e: SaveCompleted failed");

        ComPtr<IStream> dibStreamCheck;
        const HRESULT hrDibStm = stg51->OpenStream(
                L"SciDAVisDib", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &dibStreamCheck);
        CHECK_COND(FAILED(hrDibStm),
                   "Suite 51e: Persistent OLE storage must not contain bloated uncompressed SciDAVisDib stream!");

        SciDAVisOle::OleGraphPayload reloadedPay;
        CHECK_COND(SUCCEEDED(SciDAVisOle::loadPayloadFromStorage(stg51.Get(), &reloadedPay)),
                   "Suite 51e: loadPayloadFromStorage failed");
        CHECK_COND(reloadedPay.targetWindow == QStringLiteral("Fig-2"),
                   "Suite 51e: reloaded targetWindow was not updated to canonical 'Fig-2'!");
        CHECK_COND(QString::fromUtf8(reloadedPay.projectBytes).contains(QStringLiteral("Updated Strain")),
                   "Suite 51e: reloaded OLE projectBytes missing updated axis title from DoVerb edit!");

        ComPtr<IDataObject> reloadedDataObj;
        CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(reloadedPay, IID_PPV_ARGS(&reloadedDataObj)))
                           && reloadedDataObj,
                   "Suite 51e: createGraphObject(reloadedPay) failed");
        FORMATETC fmtDib51{ CF_DIB, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medDib51{};
        CHECK_COND(SUCCEEDED(reloadedDataObj->GetData(&fmtDib51, &medDib51))
                           && medDib51.tymed == TYMED_HGLOBAL && medDib51.hGlobal != nullptr,
                   "Suite 51e: On-demand GetData(CF_DIB) failed when SciDAVisDib stream was omitted from storage!");
        ReleaseStgMedium(&medDib51);

        // Also verify full-page Ctrl+C (copyActiveLayer) on a 2-layer graph preserves targetLayer == 0 (full page)
        Graph *gOverlay = mlFig2->addLayer(0, 0, 200, 200);
        CHECK_COND(gOverlay != nullptr, "Suite 51e: failed to add second layer to mlFig2");
        oleSrcApp->activateSubWindow(mlFig2);
        oleSrcApp->copyActiveLayer();
        ComPtr<IDataObject> clip51;
        HRESULT hrClip = E_FAIL;
        for (int k = 0; k < 30; ++k) {
            QCoreApplication::processEvents();
            hrClip = OleGetClipboard(&clip51);
            if (SUCCEEDED(hrClip) && clip51)
                break;
            Sleep(20);
        }
        CHECK_COND(SUCCEEDED(hrClip) && clip51, "Suite 51e: OleGetClipboard after copyActiveLayer failed");

        oleSrcApp->savedProject();
        oleSrcApp->setOleEmbeddedMode(true);
        delete oleSrcApp;
        QApplication::processEvents();
    }

    // -------------------------------------------------------------------------
    // 51f. UI Font Size (>= 10 pt Antigravity parity) & On-Screen Plot Frame
    //      4-Corner Continuity Gate (Zero Disconnected Corner Pixels)
    // -------------------------------------------------------------------------
    {
        CHECK_COND(w51->appFont.pointSize() >= 12 && w51->tableTextFont.pointSize() >= 12
                           && w51->tableHeaderFont.pointSize() >= 12
                           && qApp->font().pointSize() >= 12,
                   "Suite 51f: UI default font pointSize must be >= 12 pt (Antigravity readability parity)!");

        Table *tCorner = w51->newTable(5, 2, "CornerFrameTbl", "");
        CHECK_COND(tCorner != nullptr, "Suite 51f: failed to create CornerFrameTbl");
        for (int r = 0; r < 5; ++r) {
            tCorner->setCell(r, 0, double(r));
            tCorner->setCell(r, 1, double(r * 2));
        }
        MultiLayer *mlCorner = w51->multilayerPlot(
                tCorner, QStringList() << QStringLiteral("CornerFrameTbl_2"), Graph::Line);
        CHECK_COND(mlCorner != nullptr && mlCorner->activeGraph() != nullptr,
                   "Suite 51f: failed to create mlCorner");
        mlCorner->show();
        Graph *gCorner = mlCorner->activeGraph();
        gCorner->setAxesLinewidth(2);
        gCorner->replot();
        QApplication::processEvents();

        QWidget *cv = gCorner->plotWidget() ? gCorner->plotWidget()->canvas() : nullptr;
        CHECK_COND(cv != nullptr && cv->width() > 20 && cv->height() > 20,
                   "Suite 51f: plotWidget()->canvas() invalid");
        const QImage cvImg = cv->grab().toImage();
        CHECK_COND(!cvImg.isNull() && cvImg.width() >= 20 && cvImg.height() >= 20,
                   "Suite 51f: canvas()->grab() returned null image");
        const int cw = cvImg.width();
        const int ch = cvImg.height();
        const int gTL = qGray(cvImg.pixel(0, 0));
        const int gTR = qGray(cvImg.pixel(cw - 1, 0));
        const int gBL = qGray(cvImg.pixel(0, ch - 1));
        const int gBR = qGray(cvImg.pixel(cw - 1, ch - 1));
        CHECK_COND(gTL < 60 && gTR < 60 && gBL < 60 && gBR < 60,
                   QString("Suite 51f: Plot frame corners disconnected on screen! Corner grays=(%1,%2,%3,%4)")
                           .arg(gTL)
                           .arg(gTR)
                           .arg(gBL)
                           .arg(gBR)
                           .toStdString()
                           .c_str());
    }

    // -------------------------------------------------------------------------
    // 51g. Phase 11 ISIJ Auto Axis Titles, Default 1:1 Lock + Free Wide/Tall
    //      Stretching, and GraphInspectorDock Empty-State Quick Start Panel
    // -------------------------------------------------------------------------
    {
        w51->resize(1280, 880);
        w51->show();
        QApplication::processEvents();

        Table *t51g = w51->newTable(6, 2, "Phase11IsijTbl", "Phase 11 ISIJ");
        CHECK_COND(t51g != nullptr, "Suite 51g: failed to create Phase11IsijTbl");
        t51g->setColName(0, "Strain");
        t51g->setColPlotDesignation(0, SciDAVis::X);
        t51g->column(0)->setLongName(QString::fromUtf8("Nominal strain, \xCE\xB5"));
        t51g->column(0)->setUnits(QStringLiteral("%"));
        t51g->setColName(1, "Stress");
        t51g->setColPlotDesignation(1, SciDAVis::Y);
        t51g->column(1)->setLongName(QString::fromUtf8("Nominal stress, \xCF\x83"));
        t51g->column(1)->setUnits(QStringLiteral("MPa"));
        for (int r = 0; r < 6; ++r) {
            t51g->setCell(r, 0, double(r) * 1.5);
            t51g->setCell(r, 1, double(r) * 95.0);
        }

        // (a) Auto axis title uses ISIJ format: "Nominal stress, σ / MPa"
        MultiLayer *ml51g = w51->multilayerPlot(
                t51g, QStringList() << (t51g->name() + "_Stress"), Graph::LineSymbols);
        CHECK_COND(ml51g != nullptr && ml51g->activeGraph() != nullptr,
                   "Suite 51g: failed to create ml51g");
        Graph *g51g = ml51g->activeGraph();
        const QString xTitle51g = g51g->plotWidget()->axisTitle(QwtPlot::xBottom).text();
        const QString yTitle51g = g51g->plotWidget()->axisTitle(QwtPlot::yLeft).text();
        CHECK_COND(xTitle51g == QString::fromUtf8("Nominal strain, \xCE\xB5 / %"),
                   QString("Suite 51g(a): expected ISIJ X title 'Nominal strain, ε / %', got '%1'")
                           .arg(xTitle51g)
                           .toStdString()
                           .c_str());
        CHECK_COND(yTitle51g == QString::fromUtf8("Nominal stress, \xCF\x83 / MPa"),
                   QString("Suite 51g(a): expected ISIJ Y title 'Nominal stress, σ / MPa', got '%1'")
                           .arg(yTitle51g)
                           .toStdString()
                           .c_str());

        // (b) Default keepAspectRatio() == true; unlocking allows free wide (700x360) and tall (360x700) stretching
        CHECK_COND(ml51g->keepAspectRatio(),
                   "Suite 51g(b): new MultiLayer graph must default to keepAspectRatio() == true!");
        ml51g->show();
        QApplication::processEvents();

        ml51g->setKeepAspectRatio(false);
        CHECK_COND(!ml51g->keepAspectRatio(),
                   "Suite 51g(b): setKeepAspectRatio(false) did not unlock aspect ratio!");

        ml51g->resize(700, 360);
        if (ml51g->canvasWidget())
            ml51g->canvasWidget()->resize(680, 310);
        ml51g->arrangeLayers(false, false);
        for (int pass = 0; pass < 3; ++pass) {
            QCoreApplication::sendPostedEvents();
            QApplication::processEvents();
        }
        const QRect wideRect = g51g->plotWidget()->plotLayout()->canvasRect();
        const double wideRatio = double(wideRect.width()) / double(qMax(1, wideRect.height()));
        CHECK_COND(wideRatio > 1.4,
                   QString("Suite 51g(b): unlocked wide resize (700x360) canvas W/H=%1 (expected > 1.4, rect=%2x%3)")
                           .arg(wideRatio, 0, 'f', 3)
                           .arg(wideRect.width())
                           .arg(wideRect.height())
                           .toStdString()
                           .c_str());

        ml51g->resize(360, 700);
        if (ml51g->canvasWidget())
            ml51g->canvasWidget()->resize(340, 650);
        ml51g->arrangeLayers(false, false);
        for (int pass = 0; pass < 3; ++pass) {
            QCoreApplication::sendPostedEvents();
            QApplication::processEvents();
        }
        const QRect tallRect = g51g->plotWidget()->plotLayout()->canvasRect();
        const double tallRatio = double(tallRect.width()) / double(qMax(1, tallRect.height()));
        CHECK_COND(tallRatio < 0.75,
                   QString("Suite 51g(b): unlocked tall resize (360x700) canvas W/H=%1 (expected < 0.75, rect=%2x%3)")
                           .arg(tallRatio, 0, 'f', 3)
                           .arg(tallRect.width())
                           .arg(tallRect.height())
                           .toStdString()
                           .c_str());

        // (c) GraphInspectorDock shows Quick Start empty-state panel when no graph is active
        w51->setHeadlessMode(false);
        w51->resize(1280, 820);
        w51->show();
        QApplication::processEvents();
        GraphInspectorDock *insp51g = w51->graphInspectorDock();
        CHECK_COND(insp51g != nullptr, "Suite 51g(c): w51->graphInspectorDock() is null!");
        insp51g->setCollapsedRail(false);
        insp51g->show();
        insp51g->setTargetMultiLayer(ml51g, true);
        QApplication::processEvents();
        CHECK_COND(!insp51g->isShowingQuickStart(),
                   "Suite 51g(c): GraphInspectorDock must NOT show Quick Start when a graph is active!");

        w51->activateSubWindow(t51g);
        insp51g->setTargetMultiLayer(nullptr, true);
        for (int pass = 0; pass < 3; ++pass) {
            QCoreApplication::sendPostedEvents();
            QApplication::processEvents();
        }
        QWidget *qsPanel = insp51g->findChild<QWidget *>(QStringLiteral("InspectorQuickStartPanel"));
        CHECK_COND(insp51g->isShowingQuickStart() && qsPanel != nullptr && qsPanel->isVisible(),
                   "Suite 51g(c): GraphInspectorDock did not display visible InspectorQuickStartPanel when no graph is active!");
        w51->setHeadlessMode(true);
    }

    w51->savedProject();
    w51->setOleEmbeddedMode(true);
    delete w51;
    QApplication::processEvents();

    std::cout << "[INFO] Suite 51 Campaign6_Final_EndToEnd_Invariants passed!\n";
    return 0;
}

#include "ImportOPJU.h"

namespace {
static QByteArray suite52BuildSingleEntryStoreZip(const QByteArray &entryName,
                                                  const QByteArray &entryData)
{
    const quint32 crc = suite52Crc32(entryData);
    const quint32 sz = static_cast<quint32>(entryData.size());
    const quint16 nameLen = static_cast<quint16>(entryName.size());

    QByteArray zip;
    // Local File Header
    suite52AppendU32LE(zip, 0x04034B50u);
    suite52AppendU16LE(zip, 20);      // version needed
    suite52AppendU16LE(zip, 0);       // flags
    suite52AppendU16LE(zip, 0);       // method = STORE
    suite52AppendU16LE(zip, 0);       // mod time
    suite52AppendU16LE(zip, 0);       // mod date
    suite52AppendU32LE(zip, crc);
    suite52AppendU32LE(zip, sz);
    suite52AppendU32LE(zip, sz);
    suite52AppendU16LE(zip, nameLen);
    suite52AppendU16LE(zip, 0);       // extra len
    zip.append(entryName);
    zip.append(entryData);

    const quint32 cdOffset = static_cast<quint32>(zip.size());
    // Central Directory Header
    suite52AppendU32LE(zip, 0x02014B50u);
    suite52AppendU16LE(zip, 20);      // version made by
    suite52AppendU16LE(zip, 20);      // version needed
    suite52AppendU16LE(zip, 0);       // flags
    suite52AppendU16LE(zip, 0);       // method = STORE
    suite52AppendU16LE(zip, 0);       // mod time
    suite52AppendU16LE(zip, 0);       // mod date
    suite52AppendU32LE(zip, crc);
    suite52AppendU32LE(zip, sz);
    suite52AppendU32LE(zip, sz);
    suite52AppendU16LE(zip, nameLen);
    suite52AppendU16LE(zip, 0);       // extra len
    suite52AppendU16LE(zip, 0);       // comment len
    suite52AppendU16LE(zip, 0);       // disk number start
    suite52AppendU16LE(zip, 0);       // internal attrs
    suite52AppendU32LE(zip, 0);       // external attrs
    suite52AppendU32LE(zip, 0);       // local header offset
    zip.append(entryName);

    const quint32 cdSize = static_cast<quint32>(zip.size()) - cdOffset;
    // End of Central Directory Record
    suite52AppendU32LE(zip, 0x06054B50u);
    suite52AppendU16LE(zip, 0);       // disk number
    suite52AppendU16LE(zip, 0);       // cd start disk
    suite52AppendU16LE(zip, 1);       // entries on disk
    suite52AppendU16LE(zip, 1);       // total entries
    suite52AppendU32LE(zip, cdSize);
    suite52AppendU32LE(zip, cdOffset);
    suite52AppendU16LE(zip, 0);       // comment len
    return zip;
}
} // namespace

int runSuite52Phase12OpjuApiGraphMathAndGuiGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 52: Phase 12 .opju Importer, Graph/Math Bug-Fix Regression & Modern UI/Ribbon Polish Gate...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 52");

    QTemporaryDir tmpDir52;
    CHECK_COND(tmpDir52.isValid(), "Suite 52: Failed to create QTemporaryDir");

    // =========================================================================
    // 52a. Native .opju Importer (Synthetic ZIP XML + ApplicationWindow::importOPJ
    //      + Corrupt .opju Rejection + Optional Real OriginPro CPYUA .opju)
    // =========================================================================
    {
        const QByteArray originProjXml =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<OriginProject version=\"2024\">\n"
                "  <Folder name=\"AlloyStudy\">\n"
                "    <Workbook name=\"TensileBook\" label=\"Al-Mg-Si Tensile\">\n"
                "      <Worksheet name=\"Sheet1\">\n"
                "        <Column name=\"A\" designation=\"X\" longName=\"Strain\" units=\"%\" comment=\"Engineering strain\">\n"
                "          <Data><v>0.0</v><v>1.0</v><v>2.0</v><v>3.0</v><v>4.0</v></Data>\n"
                "        </Column>\n"
                "        <Column name=\"B\" designation=\"Y\" longName=\"Stress\" units=\"MPa\" comment=\"Al-Mg-Si\">\n"
                "          <Data><v>0.0</v><v>210.5</v><v>NANUM</v><v>385.0</v><v>412.25</v></Data>\n"
                "        </Column>\n"
                "        <Column name=\"C\" designation=\"YErr\" longName=\"StdDev\" units=\"MPa\" comment=\"Replicate SD\">\n"
                "          <Data><v>0.0</v><v>4.5</v><v>5.0</v><v>6.2</v><v>7.1</v></Data>\n"
                "        </Column>\n"
                "      </Worksheet>\n"
                "    </Workbook>\n"
                "    <Graph name=\"StressStrainGraph\" label=\"Stress-Strain Curve\">\n"
                "      <Layer title=\"Tensile\">\n"
                "        <Plot table=\"TensileBook\" xCol=\"A\" yCol=\"B\" errCol=\"C\" type=\"lineSymbol\" />\n"
                "      </Layer>\n"
                "    </Graph>\n"
                "  </Folder>\n"
                "</OriginProject>\n";

        const QString synOpjuPath = tmpDir52.filePath(QStringLiteral("synthetic_tensile.opju"));
        {
            QFile f(synOpjuPath);
            CHECK_COND(f.open(QIODevice::WriteOnly), "Suite 52a: Failed to write synthetic .opju");
            f.write(suite52BuildSingleEntryStoreZip(QByteArrayLiteral("OriginProj.xml"), originProjXml));
            f.close();
        }

        CHECK_COND(ImportOPJU::isZipOpjuFile(synOpjuPath),
                   "Suite 52a: ImportOPJU::isZipOpjuFile returned false for synthetic ZIP .opju!");
        CHECK_COND(ImportOPJU::isSupportedOpjuFile(synOpjuPath),
                   "Suite 52a: ImportOPJU::isSupportedOpjuFile returned false for synthetic ZIP .opju!");
        CHECK_COND(!ImportOPJU::isCpyuaOpjuFile(synOpjuPath),
                   "Suite 52a: ImportOPJU::isCpyuaOpjuFile falsely matched ZIP .opju!");

        ApplicationWindow *wOpju = new ApplicationWindow();
        wOpju->applyUserSettings();
        wOpju->setHeadlessMode(true);

        QString opjuErr;
        ImportOPJU::ImportStats stats;
        const bool okDirect = ImportOPJU::importFile(wOpju, synOpjuPath, &opjuErr, &stats);
        CHECK_COND(okDirect,
                   QString("Suite 52a: ImportOPJU::importFile failed: %1").arg(opjuErr).toStdString().c_str());
        CHECK_COND(stats.foldersImported >= 1 && stats.tablesImported >= 1 && stats.graphsImported >= 1,
                   QString("Suite 52a: Unexpected ImportStats (folders=%1, tables=%2, graphs=%3)")
                           .arg(stats.foldersImported)
                           .arg(stats.tablesImported)
                           .arg(stats.graphsImported)
                           .toStdString()
                           .c_str());

        auto collectAllFolderWindows = [](Folder *root) {
            QList<MyWidget *> out;
            std::function<void(Folder *)> visit = [&](Folder *f) {
                if (!f)
                    return;
                for (MyWidget *w : f->windowsList()) {
                    if (w && !out.contains(w))
                        out.append(w);
                }
                for (Folder *sub : f->folders())
                    visit(sub);
            };
            visit(root);
            return out;
        };

        Table *tblOpju = nullptr;
        MultiLayer *mlOpju = nullptr;
        for (MyWidget *w : collectAllFolderWindows(wOpju->projectFolder())) {
            if (!tblOpju) {
                if (Table *t = qobject_cast<Table *>(w)) {
                    if (t->name().contains(QStringLiteral("TensileBook")))
                        tblOpju = t;
                }
            }
            if (!mlOpju) {
                if (MultiLayer *ml = qobject_cast<MultiLayer *>(w)) {
                    if (ml->name().contains(QStringLiteral("StressStrainGraph")))
                        mlOpju = ml;
                }
            }
        }
        CHECK_COND(tblOpju != nullptr, "Suite 52a: Imported TensileBook Table not found!");
        CHECK_COND(tblOpju->numCols() == 3 && tblOpju->numRows() >= 5,
                   "Suite 52a: Imported TensileBook dimensions mismatch!");
        CHECK_COND(tblOpju->column(0)->plotDesignation() == SciDAVis::X
                           && tblOpju->column(1)->plotDesignation() == SciDAVis::Y
                           && tblOpju->column(2)->plotDesignation() == SciDAVis::yErr,
                   "Suite 52a: Imported column plot designations (X/Y/YErr) mismatch!");
        CHECK_COND(tblOpju->column(0)->longName() == QStringLiteral("Strain")
                           && tblOpju->column(0)->units() == QStringLiteral("%")
                           && tblOpju->column(0)->comment() == QStringLiteral("Engineering strain")
                           && tblOpju->column(1)->longName() == QStringLiteral("Stress")
                           && tblOpju->column(1)->units() == QStringLiteral("MPa")
                           && tblOpju->column(1)->comment() == QStringLiteral("Al-Mg-Si")
                           && tblOpju->column(2)->longName() == QStringLiteral("StdDev")
                           && tblOpju->column(2)->units() == QStringLiteral("MPa"),
                   "Suite 52a: Imported column Long Name / Units / Comments metadata mismatch!");
        CHECK_COND(std::fabs(tblOpju->cell(1, 1) - 210.5) < 1e-6
                           && std::fabs(tblOpju->cell(3, 1) - 385.0) < 1e-6
                           && std::fabs(tblOpju->cell(4, 1) - 412.25) < 1e-6,
                   "Suite 52a: Imported numeric cell values mismatch!");
        CHECK_COND(tblOpju->column(1)->isInvalid(2) || tblOpju->text(2, 1).trimmed().isEmpty(),
                   "Suite 52a: NANUM missing value at row 2 col 1 was not preserved as invalid/empty!");
        CHECK_COND(mlOpju != nullptr && mlOpju->activeGraph() != nullptr
                           && mlOpju->activeGraph()->curves() >= 1,
                   "Suite 52a: Imported 2D Graph 'StressStrainGraph' missing or has zero curves!");

        // Also test ApplicationWindow::importOPJ routing for .opju
        ApplicationWindow *wViaImportOpj = wOpju->importOPJ(synOpjuPath);
        CHECK_COND(wViaImportOpj != nullptr,
                   "Suite 52a: ApplicationWindow::importOPJ(synOpjuPath) returned null!");
        bool foundTblViaApp = false;
        bool foundGraphViaApp = false;
        for (MyWidget *w : collectAllFolderWindows(wViaImportOpj->projectFolder())) {
            if (Table *t = qobject_cast<Table *>(w)) {
                if (t->name().contains(QStringLiteral("TensileBook")))
                    foundTblViaApp = true;
            } else if (MultiLayer *ml = qobject_cast<MultiLayer *>(w)) {
                if (ml->name().contains(QStringLiteral("StressStrainGraph"))
                    && ml->activeGraph() && ml->activeGraph()->curves() >= 1) {
                    foundGraphViaApp = true;
                }
            }
        }
        CHECK_COND(foundTblViaApp && foundGraphViaApp,
                   "Suite 52a: ApplicationWindow::importOPJ(.opju) did not populate Table & Graph!");
        if (wViaImportOpj != wOpju) {
            wViaImportOpj->savedProject();
            wViaImportOpj->setOleEmbeddedMode(true);
            delete wViaImportOpj;
        }

        // Corrupt / non-ZIP .opju must return false with non-empty error and zero crashes
        const QString corruptPath = tmpDir52.filePath(QStringLiteral("corrupt_invalid.opju"));
        {
            QFile cf(corruptPath);
            CHECK_COND(cf.open(QIODevice::WriteOnly), "Suite 52a: Failed to write corrupt .opju");
            cf.write("NOT_A_VALID_OPJU_OR_ZIP_HEADER_1234567890");
            cf.close();
        }
        CHECK_COND(!ImportOPJU::isSupportedOpjuFile(corruptPath),
                   "Suite 52a: isSupportedOpjuFile falsely returned true for corrupt .opju!");
        QString corruptErr;
        const bool okCorrupt = ImportOPJU::importFile(wOpju, corruptPath, &corruptErr);
        CHECK_COND(!okCorrupt && !corruptErr.trimmed().isEmpty(),
                   "Suite 52a: Corrupt .opju must return false with a non-empty error message!");

        wOpju->savedProject();
        wOpju->setOleEmbeddedMode(true);
        delete wOpju;

        // Optional real OriginPro CPYUA .opju verification (skip cleanly if absent)
        const QStringList cpyuaCandidates = {
            QStringLiteral("D:/Claude/py4ct/origin_check/ctod/out/ctod_steps_origin.opju"),
            QStringLiteral("D:/Claude/py4ct/origin_check/ctod/out/step1/ctod_step1_profile_origin.opju")
        };
        QString realCpyuaPath;
        for (const QString &cand : cpyuaCandidates) {
            if (QFileInfo::exists(cand)) {
                realCpyuaPath = cand;
                break;
            }
        }
        if (!realCpyuaPath.isEmpty()) {
            CHECK_COND(ImportOPJU::isCpyuaOpjuFile(realCpyuaPath)
                               && ImportOPJU::isSupportedOpjuFile(realCpyuaPath),
                       "Suite 52a: Real OriginPro CPYUA .opju failed isCpyuaOpjuFile/isSupportedOpjuFile!");
            ApplicationWindow *wCpyua = new ApplicationWindow();
            wCpyua->applyUserSettings();
            wCpyua->setHeadlessMode(true);
            QString cpyuaErr;
            ImportOPJU::ImportStats cpyuaStats;
            const bool okCpyua = ImportOPJU::importFile(wCpyua, realCpyuaPath, &cpyuaErr, &cpyuaStats);
            CHECK_COND(okCpyua && cpyuaStats.tablesImported >= 1,
                       QString("Suite 52a: Real CPYUA .opju import failed (ok=%1, tables=%2, err=%3)")
                               .arg(okCpyua)
                               .arg(cpyuaStats.tablesImported)
                               .arg(cpyuaErr)
                               .toStdString()
                               .c_str());
            std::cout << "[INFO] Suite 52a: Verified real CPYUA .opju (" << realCpyuaPath.toStdString()
                      << "): tables=" << cpyuaStats.tablesImported
                      << ", graphs=" << cpyuaStats.graphsImported << "\n";
            wCpyua->savedProject();
            wCpyua->setOleEmbeddedMode(true);
            delete wCpyua;
        } else {
            std::cout << "[INFO] Suite 52a: No local real CPYUA .opju sample present; skipping optional CPYUA check.\n";
        }
    }

    // =========================================================================
    // 52b. Graph & Math Bug Fixes (Phase 12 P1 Regression Gate)
    // =========================================================================
    {
        ApplicationWindow *w52 = new ApplicationWindow();
        w52->applyUserSettings();
        w52->setHeadlessMode(true);

        // 52b-1: Notched Box Plot on small-N outlier group [0.9, 1.0, 1.1, 1.2, 100.0]
        //        produces valid non-inverted notches (no cat-ear spikes extending toward 100.0)
        Table *tBox52 = w52->newTable(5, 1, "NotchOutlier52", "Small-N Outlier");
        const double outlierVals[5] = { 0.9, 1.0, 1.1, 1.2, 100.0 };
        for (int r = 0; r < 5; ++r)
            tBox52->setCell(r, 0, outlierVals[r]);
        MultiLayer *mlBox52 = w52->multilayerPlot(tBox52, QStringList() << "NotchOutlier52_1", Graph::Box);
        CHECK_COND(mlBox52 != nullptr && mlBox52->activeGraph() != nullptr,
                   "Suite 52b-1: Failed to create Notched Box Plot!");
        Graph *gBox52 = mlBox52->activeGraph();
        BoxCurve *bc52 = dynamic_cast<BoxCurve *>(gBox52->curve(0));
        CHECK_COND(bc52 != nullptr, "Suite 52b-1: Curve 0 is not a BoxCurve!");
        bc52->setBoxStyle(BoxCurve::Notch);
        bc52->setWhiskersRange(BoxCurve::None, 0.0);
        bc52->setBrush(QBrush(QColor(220, 38, 38)));
        bc52->setPen(QPen(QColor(220, 38, 38), 1));
        bc52->setMinStyle(QwtSymbol::NoSymbol);
        bc52->setMaxStyle(QwtSymbol::NoSymbol);
        bc52->setMeanStyle(QwtSymbol::NoSymbol);
        bc52->setP1Style(QwtSymbol::NoSymbol);
        bc52->setP99Style(QwtSymbol::NoSymbol);
        gBox52->setScale(QwtPlot::yLeft, 0.0, 110.0);
        gBox52->setScale(QwtPlot::xBottom, 0.0, 2.0);
        gBox52->replot();

        QImage boxImg(400, 300, QImage::Format_RGB32);
        boxImg.fill(Qt::white);
        {
            QPainter p(&boxImg);
            gBox52->plotWidget()->print(&p, QRect(0, 0, 400, 300));
        }
        int redSpikePx = 0;
        for (int py = 105; py <= 195; ++py) { // middle band y_data ~ [35..75], strictly above Q3 & below 100.0
            for (int px = 80; px <= 360; ++px) {
                const QRgb c = boxImg.pixel(px, py);
                if (qRed(c) > 180 && qGreen(c) < 90 && qBlue(c) < 90)
                    ++redSpikePx;
            }
        }
        CHECK_COND(redSpikePx == 0,
                   QString("Suite 52b-1: Notched Box Plot drew inverted cat-ear spike into mid-range (redSpikePx=%1)!")
                           .arg(redSpikePx)
                           .toStdString()
                           .c_str());

        // 52b-2: Tensile Rp0.2 on noisy Ramberg-Osgood steel curve (+8 MPa noise at k=0)
        //        succeeds cleanly instead of falsely aborting at k=0!
        const int nTen = 81;
        Table *tTen52 = w52->newTable(nTen, 2, "NoisyTensile52", "Noisy Steel Rp0.2");
        tTen52->column(0)->setLongName(QStringLiteral("Strain"));
        tTen52->column(0)->setUnits(QStringLiteral("%"));
        tTen52->column(1)->setLongName(QStringLiteral("Stress"));
        tTen52->column(1)->setUnits(QStringLiteral("MPa"));
        for (int i = 0; i < nTen; ++i) {
            const double stressClean = 560.0 * double(i) / double(nTen - 1);
            const double strainPct = 100.0
                    * (stressClean / 200000.0 + 0.002 * std::pow(stressClean / 400.0, 10.0));
            double stressNoisy = stressClean;
            if (i == 0)
                stressNoisy = 8.0; // triggers >50% drop to i=1 (3.0 MPa) near zero stress
            else if (i == 1)
                stressNoisy = 3.0;
            else
                stressNoisy = std::max(1.0, stressClean + 8.0 * std::sin(double(i) * 1.7));
            tTen52->setCell(i, 0, strainPct);
            tTen52->setCell(i, 1, stressNoisy);
        }
        MultiLayer *mlTen52 = w52->multilayerPlot(tTen52, QStringList() << "NoisyTensile52_2", Graph::Line);
        CHECK_COND(mlTen52 != nullptr && mlTen52->activeGraph() != nullptr,
                   "Suite 52b-2: Failed to create NoisyTensile52 graph!");
        const double qnan = std::numeric_limits<double>::quiet_NaN();
        const CurveMathTools::TensileResult tenRes = CurveMathTools::analyzeTensile(
                w52, mlTen52->activeGraph(), QString(), 0.002, 1, qnan, qnan, false, false, false);
        CHECK_COND(tenRes.valid && std::isfinite(tenRes.offsetYieldY)
                           && tenRes.offsetYieldY > 320.0 && tenRes.offsetYieldY < 480.0,
                   QString("Suite 52b-2: Noisy Ramberg-Osgood Rp0.2 failed (valid=%1, Rp0.2=%2, err='%3')!")
                           .arg(tenRes.valid)
                           .arg(tenRes.offsetYieldY)
                           .arg(tenRes.error)
                           .toStdString()
                           .c_str());

        // 52b-3: LinearFit on collinear X = const (X=[2,2,2,2], Y=[1,2,3,4]) fails cleanly
        //        (no garbage f_value=1e15 / aic=-2762)
        Table *tCol52 = w52->newTable(4, 2, "CollinearX52", "Collinear X");
        for (int r = 0; r < 4; ++r) {
            tCol52->setCell(r, 0, 2.0);
            tCol52->setCell(r, 1, double(r + 1));
        }
        MultiLayer *mlCol52 = w52->multilayerPlot(tCol52, QStringList() << "CollinearX52_2", Graph::Scatter);
        CHECK_COND(mlCol52 != nullptr && mlCol52->activeGraph() != nullptr,
                   "Suite 52b-3: Failed to create CollinearX52 graph!");
        {
            LinearFit lfCol52(w52, mlCol52->activeGraph(), QStringLiteral("CollinearX52_2"));
            lfCol52.generateFunction(false);
            lfCol52.fit();
            CHECK_COND(!std::isfinite(lfCol52.rss()) && !std::isfinite(lfCol52.aic())
                               && !std::isfinite(lfCol52.bic()) && !std::isfinite(lfCol52.anovaFValue()),
                       QString("Suite 52b-3: Collinear X LinearFit returned finite garbage stats (rss=%1, aic=%2, bic=%3, F=%4)!")
                               .arg(lfCol52.rss())
                               .arg(lfCol52.aic())
                               .arg(lfCol52.bic())
                               .arg(lfCol52.anovaFValue())
                               .toStdString()
                               .c_str());
        }

        // 52b-4: Inset Layer .sciprj Save/Reload preserves cleared axis titles ("") on inset layer
        {
            ApplicationWindow *wInSave = new ApplicationWindow();
            wInSave->applyUserSettings();
            wInSave->setHeadlessMode(true);
            Table *tIn52 = wInSave->newTable(6, 2, "InsetTbl52", "Inset Axis Persistence");
            tIn52->column(0)->setLongName(QStringLiteral("Time"));
            tIn52->column(0)->setUnits(QStringLiteral("s"));
            tIn52->column(1)->setLongName(QStringLiteral("Stress"));
            tIn52->column(1)->setUnits(QStringLiteral("MPa"));
            for (int r = 0; r < 6; ++r) {
                tIn52->setCell(r, 0, double(r));
                tIn52->setCell(r, 1, 10.0 * double(r));
            }
            MultiLayer *mlIn52 = wInSave->multilayerPlot(tIn52, QStringList() << "InsetTbl52_2", Graph::Line);
            CHECK_COND(mlIn52 != nullptr && mlIn52->activeGraph() != nullptr,
                       "Suite 52b-4: Failed to create InsetGraph52!");
            mlIn52->setName(QStringLiteral("InsetGraph52"));
            Graph *gIn52 = mlIn52->addInsetLayer(mlIn52->activeGraph(), QRectF(1.0, 10.0, 3.0, 30.0));
            CHECK_COND(gIn52 != nullptr && mlIn52->layersList().size() == 2,
                       "Suite 52b-4: addInsetLayer failed!");
            gIn52->setAxisTitle(QwtPlot::xBottom, QString());
            gIn52->setAxisTitle(QwtPlot::yLeft, QString());
            CHECK_COND(gIn52->axisTitle(QwtPlot::xBottom).trimmed().isEmpty()
                               && gIn52->axisTitle(QwtPlot::yLeft).trimmed().isEmpty(),
                       "Suite 52b-4: Inset axis titles not empty before save!");

            const QString insetProjPath = tmpDir52.filePath(QStringLiteral("inset_axis_roundtrip.sciprj"));
            const QByteArray projBytes = wInSave->saveProjectToByteArray();
            CHECK_COND(!projBytes.isEmpty(), "Suite 52b-4: saveProjectToByteArray returned empty!");
            {
                QFile pf(insetProjPath);
                CHECK_COND(pf.open(QIODevice::WriteOnly), "Suite 52b-4: Failed to write .sciprj!");
                pf.write(projBytes);
                pf.close();
            }
            wInSave->savedProject();
            wInSave->setOleEmbeddedMode(true);
            delete wInSave;

            ApplicationWindow *wInLoad = new ApplicationWindow();
            wInLoad->applyUserSettings();
            wInLoad->setHeadlessMode(true);
            CHECK_COND(wInLoad->loadProject(insetProjPath), "Suite 52b-4: loadProject failed!");
            MultiLayer *mlInReload = qobject_cast<MultiLayer *>(wInLoad->window(QStringLiteral("InsetGraph52")));
            CHECK_COND(mlInReload != nullptr && mlInReload->layersList().size() == 2,
                       "Suite 52b-4: Reloaded InsetGraph52 missing or lost inset layer!");
            Graph *gInReload = mlInReload->layersList().at(1);
            CHECK_COND(gInReload->axisTitle(QwtPlot::xBottom).trimmed().isEmpty()
                               && gInReload->axisTitle(QwtPlot::yLeft).trimmed().isEmpty(),
                       QString("Suite 52b-4: .sciprj reload regenerated cleared inset axis titles (X='%1', Y='%2')!")
                               .arg(gInReload->axisTitle(QwtPlot::xBottom))
                               .arg(gInReload->axisTitle(QwtPlot::yLeft))
                               .toStdString()
                               .c_str());
            wInLoad->savedProject();
            wInLoad->setOleEmbeddedMode(true);
            delete wInLoad;
        }

        // 52b-5: Constant-Z Spectrogram (Z=3.0 everywhere) has non-degenerate colorbar scale
        Matrix *matConst52 = w52->newMatrix(8, 8);
        CHECK_COND(matConst52 != nullptr, "Suite 52b-5: newMatrix(8, 8) returned null!");
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c)
                matConst52->setCell(r, c, 3.0);
        MultiLayer *mlConst52 = w52->plotSpectrogram(matConst52, Graph::ColorMap);
        CHECK_COND(mlConst52 != nullptr && mlConst52->activeGraph() != nullptr,
                   "Suite 52b-5: plotSpectrogram failed on constant-Z matrix!");
        Graph *gConst52 = mlConst52->activeGraph();
        Spectrogram *spec52 = nullptr;
        for (QwtPlotItem *it : gConst52->plotWidget()->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                spec52 = static_cast<Spectrogram *>(it);
                break;
            }
        }
        CHECK_COND(spec52 != nullptr, "Suite 52b-5: Spectrogram item not found on ColorMap graph!");
        const QwtScaleDiv *cbDiv52 = gConst52->plotWidget()->axisScaleDiv(spec52->colorScaleAxis());
        CHECK_COND(cbDiv52 != nullptr && std::isfinite(cbDiv52->lowerBound())
                           && std::isfinite(cbDiv52->upperBound())
                           && cbDiv52->upperBound() > cbDiv52->lowerBound() + 1e-6,
                   QString("Suite 52b-5: Constant-Z Spectrogram colorbar scale is degenerate ([%1, %2])!")
                           .arg(cbDiv52 ? cbDiv52->lowerBound() : 0.0)
                           .arg(cbDiv52 ? cbDiv52->upperBound() : 0.0)
                           .toStdString()
                           .c_str());

        w52->savedProject();
        w52->setOleEmbeddedMode(true);
        delete w52;
    }

    // =========================================================================
    // 52c. Modern UI Theme & Ribbon Polish (QListWidget + Arial/No-QwtPlotCanvas
    //      + Always-Visible RibbonCommandSearchPill at 1280px)
    // =========================================================================
    {
        const QString qss52 = ModernUiTheme::modernStyleSheet();
        CHECK_COND(!qss52.isEmpty() && qss52.contains(QStringLiteral("QListWidget"))
                           && !qss52.contains(QStringLiteral("QwtPlotCanvas")),
                   "Suite 52c: ModernUiTheme::modernStyleSheet() must style QListWidget and must not override QwtPlotCanvas!");

        ApplicationWindow *w52Gui = new ApplicationWindow();
        w52Gui->applyUserSettings();
        ModernUiTheme::applyModernTheme(qApp);
        CHECK_COND(w52Gui->plotAxesFont.family() == QStringLiteral("Arial")
                           && w52Gui->plotNumbersFont.family() == QStringLiteral("Arial")
                           && w52Gui->plotLegendFont.family() == QStringLiteral("Arial")
                           && w52Gui->plotTitleFont.family() == QStringLiteral("Arial"),
                   "Suite 52c: ModernUiTheme clobbered Suite 37 Arial plot font invariants!");

        w52Gui->resize(1280, 820);
        w52Gui->show();
        for (int pass = 0; pass < 4; ++pass) {
            QCoreApplication::sendPostedEvents();
            QApplication::processEvents();
        }
        ModernRibbonBar *rib52 = w52Gui->modernRibbonBar();
        CHECK_COND(rib52 != nullptr && rib52->isVisible(),
                   "Suite 52c: ModernRibbonBar null or hidden at 1280px!");
        QToolButton *searchPill = rib52->findChild<QToolButton *>(QStringLiteral("RibbonCommandSearchPill"));
        CHECK_COND(searchPill != nullptr && searchPill->isVisible()
                           && !searchPill->text().trimmed().isEmpty(),
                   "Suite 52c: ModernRibbonBar missing visible RibbonCommandSearchPill with non-empty text at 1280px!");

        w52Gui->savedProject();
        w52Gui->setOleEmbeddedMode(true);
        delete w52Gui;
        QApplication::processEvents();
    }

    // =========================================================================
    // 52d. Wide MDI SubWindow Resize Grab Margin (>= 7px) & Last Graph Window
    //      Geometry + Settings Persistence Across Application Restart
    // =========================================================================
    {
        const QString persistIni =
                QDir::tempPath()
                + QStringLiteral("/torigraph_suite52d_persist_%1.ini")
                          .arg(qint64(GetCurrentProcessId()));
        QFile::remove(persistIni);
        const QByteArray prevIniEnv = qgetenv("TORIGRAPH_SETTINGS_INI");
        qputenv("TORIGRAPH_SETTINGS_INI", persistIni.toLocal8Bit());

        // Session 1: verify wide resize grab margin + customize graph geometry & settings, then save
        ApplicationWindow *wP1 = new ApplicationWindow();
        wP1->applyUserSettings();
        ModernUiTheme::applyModernTheme(qApp);
        wP1->resize(1400, 900);
        wP1->show();
        QApplication::processEvents();

        ModernRibbonBar *rib1 = wP1->modernRibbonBar();
        CHECK_COND(rib1 != nullptr, "Suite 52d: ModernRibbonBar null on wP1");
        rib1->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
        rib1->setSplitLeftRatio(0.34);
        QApplication::processEvents();

        Table *t1 = wP1->newTable("PersistTbl1", 5, 2);
        wP1->initTable(t1);
        for (int r = 0; r < 5; ++r) {
            t1->setCell(r, 0, double(r));
            t1->setCell(r, 1, double(r * r + 1));
        }
        MultiLayer *ml1 = wP1->multilayerPlot(t1, QStringList() << "PersistTbl1_2", Graph::Line);
        CHECK_COND(ml1 != nullptr && ml1->activeGraph() != nullptr,
                   "Suite 52d: Failed to create MultiLayer on wP1");
        QApplication::processEvents();

        const int subFrameW_tbl = t1->style()->pixelMetric(QStyle::PM_MdiSubWindowFrameWidth, nullptr, t1);
        const int subFrameW_ml = ml1->style()->pixelMetric(QStyle::PM_MdiSubWindowFrameWidth, nullptr, ml1);
        const int splitW = wP1->style()->pixelMetric(QStyle::PM_SplitterWidth, nullptr, wP1);
        std::cout << "[INFO] Suite 52d resize grab metrics: Table frame=" << subFrameW_tbl
                  << "px, Graph frame=" << subFrameW_ml << "px, Splitter=" << splitW << "px\n";
        CHECK_COND(subFrameW_tbl >= 7 && subFrameW_ml >= 7 && splitW >= 7,
                   QString("Suite 52d: Resize grab zone too narrow (table=%1, graph=%2, splitter=%3)!")
                           .arg(subFrameW_tbl)
                           .arg(subFrameW_ml)
                           .arg(splitW)
                           .toStdString()
                           .c_str());

        ml1->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        ml1->resize(680, 560);
        ml1->move(56, 38);
        QApplication::processEvents();

        wP1->saveSettings();
        wP1->savedProject();
        wP1->setOleEmbeddedMode(true);
        delete wP1;
        QApplication::processEvents();

        // Session 2: verify restored graph window geometry, settings, and workspace mode
        ApplicationWindow *wP2 = new ApplicationWindow();
        wP2->applyUserSettings();
        wP2->resize(1400, 900);
        wP2->show();
        QApplication::processEvents();

        ModernRibbonBar *rib2 = wP2->modernRibbonBar();
        CHECK_COND(rib2 != nullptr
                           && rib2->workspaceLayoutMode() == ModernRibbonBar::LayoutFreeMdi
                           && std::fabs(rib2->splitLeftRatio() - 0.34) < 0.02,
                   "Suite 52d: Failed to restore workspaceLayoutMode / splitLeftRatio on Session 2!");

        Table *t2 = wP2->newTable("PersistTbl2", 5, 2);
        wP2->initTable(t2);
        for (int r = 0; r < 5; ++r) {
            t2->setCell(r, 0, double(r));
            t2->setCell(r, 1, double(r * 2 + 1));
        }
        MultiLayer *ml2 = wP2->multilayerPlot(t2, QStringList() << "PersistTbl2_2", Graph::Line);
        QApplication::processEvents();
        CHECK_COND(ml2 != nullptr && ml2->activeGraph() != nullptr,
                   "Suite 52d: Failed to create MultiLayer on wP2");
        Graph *g2 = ml2->activeGraph();
        std::cout << "[INFO] Suite 52d restored Graph geom=(" << ml2->x() << "," << ml2->y()
                  << "," << ml2->width() << "x" << ml2->height() << "), page="
                  << ml2->pageWidthPhysical() << "x" << ml2->pageHeightPhysical()
                  << " dpi=" << ml2->pageDpi() << " zoom=" << ml2->pageZoom()
                  << " titlePt=" << g2->axisTitleFont(QwtPlot::xBottom).pointSize() << "\n";
        CHECK_COND(qAbs(ml2->width() - 680) <= 4 && qAbs(ml2->height() - 560) <= 4
                           && qAbs(ml2->x() - 56) <= 4 && qAbs(ml2->y() - 38) <= 4,
                   "Suite 52d: Restored Graph window size/position mismatch!");
        CHECK_COND(ml2->pageUnit() == MultiLayer::UnitMm
                           && std::fabs(ml2->pageWidthPhysical() - 85.0) < 0.5
                           && std::fabs(ml2->pageHeightPhysical() - 85.0) < 0.5
                           && ml2->pageDpi() == 600
                           && std::fabs(ml2->pageZoom() - 1.6) < 0.05
                           && ml2->keepAspectRatio(),
                   "Suite 52d: Restored Graph page/DPI/zoom/aspect settings mismatch!");
        CHECK_COND(g2->axisUnitFormat() == Column::UnitFormat::ISIJSlash
                           && g2->legend() != nullptr
                           && g2->legend()->frameStyle() == Legend::None
                           && g2->axisTitleFont(QwtPlot::xBottom).pointSize() == 11,
                   "Suite 52d: Restored Graph font/legend/unitFormat settings mismatch!");

        // Also test Split View workspace mode persistence
        rib2->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
        wP2->saveSettings();
        wP2->savedProject();
        wP2->setOleEmbeddedMode(true);
        delete wP2;
        QApplication::processEvents();

        ApplicationWindow *wP3 = new ApplicationWindow();
        CHECK_COND(wP3->modernRibbonBar() != nullptr
                           && wP3->modernRibbonBar()->workspaceLayoutMode()
                                   == ModernRibbonBar::LayoutSplitTableGraph,
                   "Suite 52d: Failed to restore LayoutSplitTableGraph workspace mode!");
        wP3->savedProject();
        wP3->setOleEmbeddedMode(true);
        delete wP3;
        QApplication::processEvents();

        qputenv("TORIGRAPH_SETTINGS_INI", prevIniEnv);
        QFile::remove(persistIni);
    }

    std::cout << "[INFO] Suite 52 Phase 12 .opju Importer, Graph/Math Bug Fixes & Modern UI/Ribbon Polish Gate passed!\n";
    return 0;
}

