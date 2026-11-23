/***************************************************************************
    File                 : PlotDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Custom curves dialog

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
#include "ApplicationWindow.h"
#include "PlotDialog.h"
#include "I18nJapanese.h"
#include "IconLoader.h"
#include "ColorButton.h"
#include "PenWidget.h"
#include "PlotStyleLimits.h"
#include "PatternBox.h"
#include "SymbolBox.h"
#include "Table.h"
#include "MyParser.h"
#include "QwtHistogram.h"
#include "VectorCurve.h"
#include "QwtErrorPlotCurve.h"
#include "BoxCurve.h"
#include "FunctionCurve.h"
#include "Spectrogram.h"
#include "QwtPieCurve.h"
#include "ColorMapEditor.h"
#include "QStringStdString.h"
#include "core/column/Column.h"

#include <QTreeWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>
#include <QWidget>
#include <QMessageBox>
#include <QComboBox>
#include <QWidgetList>
#include <QFileDialog>
#include <QGroupBox>
#include <QFontDialog>
#include <QShortcut>
#include <QKeySequence>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QDateTime>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSplitter>
#include <QFrame>
#include <QTableWidget>
#include <QLocale>
#include <QScrollArea>
#include <QTimer>
#include <QHideEvent>
#include <cmath>

namespace {
void registerPlotDialogTranslations()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Co&lor Palette..."), QString::fromUtf8("カラーパレット(&L)..."));
    struct Pair
    {
        const char *en;
        const char *ja;
    };
    static const Pair extra[] = {
        { "Log color scale (log10 z)", "対数カラースケール (log10 z)" },
        { "Cell blocks (no smoothing)", "セルブロック表示 (平滑化なし)" },
        { "Colours follow log10(z); cells with z <= 0 are transparent.",
          "色を log10(z) に従って割り当てます。z <= 0 のセルは透明になります。" },
        { "Draw each matrix cell as a flat block, like histogram bins.",
          "各セルをヒストグラムのビンのように平坦なブロックとして描きます。" },
        { "Tukey 1.5*IQR", "Tukey 1.5*IQR (四分位範囲の1.5倍)" },
        { "Edit the page size, unit, DPI, background, zoom and fitting in Page Setup.",
          "ページのサイズ・単位・DPI・背景色・ズーム・フィットは「ページ設定」で変更します。" },
    };
    for (const Pair &p : extra)
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(p.en),
                                                                  QString::fromUtf8(p.ja));
}

//! Opacity spin boxes show 0..100 %, while Qt colours store an 8-bit alpha.
int percentFromAlpha(int alpha)
{
    return qBound(PlotStyleLimits::kOpacityMinPercent,
                  qRound(alpha * 100.0 / 255.0), PlotStyleLimits::kOpacityMaxPercent);
}

int alphaFromPercent(int percent)
{
    return qBound(0, qRound(percent * 255.0 / 100.0), 255);
}

//! Alpha to write back: the loaded exact alpha when the percent shown is unchanged.
int alphaForCommit(int percent, int loadedAlpha)
{
    if (loadedAlpha >= 0 && percentFromAlpha(loadedAlpha) == percent)
        return loadedAlpha;
    return alphaFromPercent(percent);
}

constexpr double kTukeyWhiskerCoefficient = 1.5;

QFont clampedFontSize(QFont f)
{
    const double pt = PlotStyleLimits::effectiveFontPointSizeF(f);
    const double bounded = qBound(PlotStyleLimits::kFontSizeMin, pt, PlotStyleLimits::kFontSizeMax);
    if (std::fabs(bounded - pt) > 1e-6)
        f.setPointSizeF(bounded);
    return f;
}
} // namespace

PlotDialog::PlotDialog(bool showExtended, QWidget *parent, Qt::WindowFlags fl)
    : QDialog(parent, fl),
      titleFont("Arial", 12, QFont::Normal),
      legendFont("Arial", 12, QFont::Normal),
      axesFont("Arial", 12, QFont::Normal),
      numbersFont("Arial", 12, QFont::Normal),
      d_ml(0)
{
    Q_UNUSED(showExtended);
    registerPlotDialogTranslations();
    setWindowTitle(tr("Plot details"));
    setSizeGripEnabled(true);
    // The tab area lives in an internal QScrollArea (below), so the dialog may be made smaller than
    // its content (e.g. 911 x 472 usable screen); ToriUx must not wrap it a second time.
    setProperty("toriHasInternalScroll", true);
    setMinimumSize(520, 320);
    resize(780, 520);

    d_leftTreePanel = new QWidget(this);
    d_leftTreePanel->setMinimumWidth(210);
    QVBoxLayout *leftLayout = new QVBoxLayout(d_leftTreePanel);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->setSpacing(6);

    listBox = new QTreeWidget();
    listBox->setColumnCount(1);
    listBox->header()->hide();
    listBox->header()->setStretchLastSection(false);
    listBox->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    listBox->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    listBox->setTextElideMode(Qt::ElideNone);
    listBox->setIndentation(15);
    listBox->setMinimumWidth(205);
    leftLayout->addWidget(listBox, 1);

    QHBoxLayout *moveBtnLayout = new QHBoxLayout();
    moveBtnLayout->setContentsMargins(0, 0, 0, 0);
    moveBtnLayout->setSpacing(6);
    btnMoveCurveUp = new QPushButton(tr("Move Up"));
    btnMoveCurveDown = new QPushButton(tr("Move Down"));
    btnMoveCurveUp->setEnabled(false);
    btnMoveCurveDown->setEnabled(false);
    moveBtnLayout->addWidget(btnMoveCurveUp, 1);
    moveBtnLayout->addWidget(btnMoveCurveDown, 1);
    leftLayout->addLayout(moveBtnLayout);

    curvePlotTypeBox = new QWidget(d_leftTreePanel);
    QHBoxLayout *hb1 = new QHBoxLayout(curvePlotTypeBox);
    hb1->setContentsMargins(0, 2, 0, 0);
    hb1->setSpacing(6);
    hb1->addWidget(new QLabel(tr("Plot type")));
    boxPlotType = new QComboBox();
    boxPlotType->setEditable(false);
    boxPlotType->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hb1->addWidget(boxPlotType, 1);
    leftLayout->addWidget(curvePlotTypeBox);

    privateTabWidget = new QTabWidget(this);
    privateTabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    d_mainSplitter = new QSplitter(Qt::Horizontal, this);
    d_mainSplitter->setChildrenCollapsible(false);
    d_mainSplitter->addWidget(d_leftTreePanel);
    d_mainSplitter->addWidget(privateTabWidget);
    d_mainSplitter->setStretchFactor(0, 0);
    d_mainSplitter->setStretchFactor(1, 1);
    d_mainSplitter->setSizes({ 255, 525 });

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // Only the tree + tab area scrolls; the OK / Apply / Cancel row below stays visible.
    QScrollArea *contentScroll = new QScrollArea(this);
    contentScroll->setObjectName(QStringLiteral("PlotDialogContentScrollArea"));
    contentScroll->setWidgetResizable(true);
    contentScroll->setFrameShape(QFrame::NoFrame);
    contentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    contentScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    contentScroll->setWidget(d_mainSplitter);
    mainLayout->addWidget(contentScroll, 1);

    initAxesPage();
    initLinePage();
    initSymbolsPage();
    initHistogramPage();
    initErrorsPage();
    initSpacingPage();
    initVectPage();
    initBoxPage();
    initPercentilePage();
    initSpectrogramPage();
    initPiePage();
    initPrintPage(); // embedded in the Page tab, so it must exist before initPageTab()
    initPageTab();
    initLayerPage();
    initFontsPage();

    // pageTab only summarises the page (no Apply-committed controls); its print options are
    // tracked through printPage.
    for (QWidget *page : { axesPage, linePage, symbolPage, histogramPage, errorsPage, spacingPage,
                           vectPage, boxPage, percentilePage, spectrogramPage, piePage, layerPage,
                           fontsPage, printPage }) {
        connectTabDirtySignals(page);
    }

    clearTabWidget();

    d_externalSyncTimer = new QTimer(this);
    d_externalSyncTimer->setSingleShot(true);
    d_externalSyncTimer->setInterval(120);
    connect(d_externalSyncTimer, &QTimer::timeout, this, &PlotDialog::syncFromMultiLayer);

    QFrame *btnSep = new QFrame(this);
    btnSep->setFrameShape(QFrame::HLine);
    btnSep->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(btnSep);

    QHBoxLayout *hb2 = new QHBoxLayout();
    hb2->setContentsMargins(0, 2, 0, 0);
    hb2->setSpacing(8);

    // Keep hidden checked btnMore for legacy state checks while hiding cryptic 30px '<<' button
    btnMore = new QPushButton(this);
    btnMore->setCheckable(true);
    btnMore->setChecked(true);
    btnMore->hide();

    btnWorksheet = new QPushButton(tr("&Worksheet"));
    btnEditCurve = new QPushButton(tr("&Plot Associations..."));
    btnColorPalette = new QPushButton(tr("Co&lor Palette..."));
    hb2->addWidget(btnWorksheet);
    hb2->addWidget(btnEditCurve);
    hb2->addWidget(btnColorPalette);
    hb2->addStretch(1);

    buttonOk = new QPushButton(tr("&OK"));
    buttonOk->setDefault(true);
    buttonOk->setMinimumWidth(84);
    buttonApply = new QPushButton(tr("&Apply"));
    buttonApply->setMinimumWidth(84);
    buttonCancel = new QPushButton(tr("&Cancel"));
    buttonCancel->setMinimumWidth(84);
    hb2->addWidget(buttonOk);
    hb2->addWidget(buttonApply);
    hb2->addWidget(buttonCancel);
    mainLayout->addLayout(hb2);

    connect(btnMore, SIGNAL(toggled(bool)), this, SLOT(showAll(bool)));

    connect(buttonOk, SIGNAL(clicked()), this, SLOT(quit()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(buttonApply, SIGNAL(clicked()), this, SLOT(acceptParams()));
    connect(btnWorksheet, SIGNAL(clicked()), this, SLOT(showWorksheet()));
    connect(btnEditCurve, SIGNAL(clicked()), this, SLOT(editCurve()));
    connect(btnColorPalette, SIGNAL(clicked()), this, SLOT(openColorPaletteEditor()));
    connect(btnMoveCurveUp, SIGNAL(clicked()), this, SLOT(moveSelectedCurveUp()));
    connect(btnMoveCurveDown, SIGNAL(clicked()), this, SLOT(moveSelectedCurveDown()));
    connect(listBox, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this,
            SLOT(showPlotAssociations(QTreeWidgetItem *, int)));
    connect(listBox, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this,
            SLOT(updateTabWindow(QTreeWidgetItem *, QTreeWidgetItem *)));
    connect(listBox, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this,
            SLOT(onTreeItemChanged(QTreeWidgetItem *, int)));
    connect(listBox, SIGNAL(itemCollapsed(QTreeWidgetItem *)), this,
            SLOT(updateTreeWidgetItem(QTreeWidgetItem *)));
    connect(listBox, SIGNAL(itemExpanded(QTreeWidgetItem *)), this,
            SLOT(updateTreeWidgetItem(QTreeWidgetItem *)));
    connect(boxPlotType, SIGNAL(currentIndexChanged(int)), this, SLOT(changePlotType(int)));

    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    connect(shortcut, SIGNAL(activated()), this, SLOT(removeSelectedCurve()));

    togglePage(true);
}

void PlotDialog::showAll(bool all)
{
    Q_UNUSED(all);
    if (btnMore) {
        QSignalBlocker blockMore(btnMore);
        btnMore->setChecked(true);
        btnMore->hide();
    }
    if (d_leftTreePanel)
        d_leftTreePanel->show();
    if (listBox)
        listBox->show();

    QTreeWidgetItem *item = listBox ? listBox->currentItem() : nullptr;
    if (curvePlotTypeBox) {
        curvePlotTypeBox->setVisible(item && item->type() == CurveTreeItem::PlotCurveTreeItem);
    }
}

void PlotDialog::showPlotAssociations(QTreeWidgetItem *item, int)
{
    if (!item)
        return;

    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    if (!app)
        return;

    if (item->type() != CurveTreeItem::PlotCurveTreeItem)
        return;

    CurveTreeItem *ci = static_cast<CurveTreeItem *>(item);
    if (!ci->isCurveAlive())
        return;

    QwtPlotItem *it = const_cast<QwtPlotItem *>(ci->plotItem());
    if (!it)
        return;

    if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
        Spectrogram *sp = (Spectrogram *)it;
        if (sp->matrix())
            sp->matrix()->showMaximized();
        return;
    }

    if (((PlotCurve *)it)->type() == Graph::Function) {
        close();
        app->showFunctionDialog(ci->graph(), ci->plotItemIndex());
    } else {
#if QT_VERSION >= 0x050000
        close();
#endif
        app->showPlotAssociations(ci->plotItemIndex());
#if QT_VERSION < 0x050000
        close();
#endif
    }
}

void PlotDialog::editCurve()
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());

    QTreeWidgetItem *cur = listBox->currentItem();
    if (!cur || cur->type() != CurveTreeItem::PlotCurveTreeItem)
        return;

    CurveTreeItem *item = static_cast<CurveTreeItem *>(cur);
    if (!item->isCurveAlive())
        return;

    const QwtPlotItem *pi = item->plotItem();
    if (!pi || pi->rtti() != QwtPlotItem::Rtti_PlotCurve)
        return;

    int index = item->plotItemIndex();
    int curveType = static_cast<const PlotCurve *>(pi)->type();

#if QT_VERSION >= 0x050000
    close();
#endif

    if (app) {
        if (curveType == Graph::Function)
            app->showFunctionDialog(item->graph(), index);
        else
            app->showPlotAssociations(index);
    }

#if QT_VERSION < 0x050000
    close();
#endif
}

void PlotDialog::changePlotType(int index)
{
    if (d_loadingUi)
        return;

    CurveTreeItem *item = (CurveTreeItem *)listBox->currentItem();
    if (!item || item->type() != CurveTreeItem::PlotCurveTreeItem || !item->isCurveAlive())
        return;
    Graph *graph = item->graph();
    if (!graph)
        return;

    // The combo only offers what Graph::canConvertTo() accepts (see setPlotType()); a disabled or
    // single-entry combo (error bars, pie, histogram, box, vector, contour/image, function) never
    // converts.
    if (!boxPlotType->isEnabled() || boxPlotType->count() <= 1)
        return;
    const QVariant typeData = boxPlotType->itemData(index);
    if (!typeData.isValid())
        return;
    const int new_curve_type = typeData.toInt();
    const int old_curve_type = item->plotItemType();
    if (old_curve_type == new_curve_type)
        return;
    QwtPlotCurve *convCurve =
            dynamic_cast<QwtPlotCurve *>(const_cast<QwtPlotItem *>(item->plotItem()));
    if (!convCurve
        || !Graph::canConvertTo(convCurve, static_cast<Graph::CurveType>(new_curve_type)))
        return;

    // Immediate (Undo-able) structural change: first write the edits that are still waiting for
    // Apply, so they are neither lost nor mixed into this Undo step.
    flushPendingEdits();
    if (!item->isCurveAlive())
        return;

    MultiLayer *ml = d_mlGuard ? d_mlGuard.data() : graph->multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parentWidget());
    if (!app)
        app = qobject_cast<ApplicationWindow *>(window());
    if (app && ml)
        app->invalidateModelessGraphDialogs(ml, this);

    QScopedValueRollback<int> selfEdit(d_selfEditDepth, d_selfEditDepth + 1);
    {
        QScopedValueRollback<bool> guard(d_loadingUi, true);

        LayerItem *layerIt = dynamic_cast<LayerItem *>(item->parent());
        const int origIdx = item->plotItemIndex();
        int ebBefore = 0;
        if (DataCurve *dc =
                    dynamic_cast<DataCurve *>(const_cast<QwtPlotItem *>(item->plotItem()))) {
            const bool barTransition = ((new_curve_type == Graph::VerticalBars
                                         || new_curve_type == Graph::HorizontalBars)
                                        != (old_curve_type == Graph::VerticalBars
                                            || old_curve_type == Graph::HorizontalBars));
            if (barTransition) {
                for (DataCurve *eb : dc->errorBarsList()) {
                    const int ebi = graph->curveIndex(eb);
                    if (ebi >= 0 && ebi < origIdx)
                        ++ebBefore;
                }
            }
        }

        graph->setCurveType(origIdx, (Graph::CurveType)new_curve_type);
        const int resolvedIdx = qBound(0, origIdx - ebBefore, qMax(0, graph->curves() - 1));
        if (layerIt && (!item->isCurveAlive() || layerIt->childCount() != graph->curves())) {
            QSignalBlocker blocker(listBox);
            layerIt->rebuildCurvesList();
            if (resolvedIdx < layerIt->childCount()) {
                listBox->setCurrentItem(layerIt->child(resolvedIdx));
                item = dynamic_cast<CurveTreeItem *>(layerIt->child(resolvedIdx));
            } else {
                item = nullptr;
            }
        } else if (item && resolvedIdx < graph->curves()) {
            item->updatePlotItemPointer(graph->plotItem(resolvedIdx));
        }

        // Rebuild the tab pages and the widget values from the curve as it is now, so a type that
        // gained or lost symbols / fill shows the right pages (Line, Symbol, Pattern, Spacing...).
        if (item && item->isCurveAlive()) {
            int tabType = -1;
            const int shown = shownPlotType(item, &tabType);
            const int comboIdx = boxPlotType->findData(shown);
            if (comboIdx >= 0 && comboIdx != boxPlotType->currentIndex()) {
                QSignalBlocker typeBlocker(boxPlotType);
                boxPlotType->setCurrentIndex(comboIdx);
            }
            if (tabType >= 0)
                rebuildCurveTabs(tabType, false);
            setActiveCurve(item);
        } else {
            clearTabWidget();
            d_shownTabType = -1;
        }
        d_dirtyTabs.clear();
    }
    graph->replot();
    graph->notifyChanges();
    if (ml)
        ml->pushUndoSnapshot(tr("Plot Properties"), beforeSnap);
    updateCurveMoveButtons();
}

void PlotDialog::initFontsPage()
{
    QGroupBox *boxFonts = new QGroupBox();
    QGridLayout *fl = new QGridLayout(boxFonts);

    btnTitle = new QPushButton(tr("Titles"));
    btnAxesLabels = new QPushButton(tr("Axes Labels"));
    btnAxesNumbers = new QPushButton(tr("Axes Numbers"));
    btnLegend = new QPushButton(tr("Legends"));

    // One size box per font: kFontSizeMin..kFontSizeMax pt in 0.5 pt steps, the same range as the
    // inspector and AxesDialog (QFontDialog only offers whole points).
    auto newSizeBox = []() -> QDoubleSpinBox * {
        return new PlotStyleLimits::FractionalPtSpinBox();
    };
    boxTitleFontSize = newSizeBox();
    boxAxesLabelFontSize = newSizeBox();
    boxAxesNumberFontSize = newSizeBox();
    boxLegendFontSize = newSizeBox();

    fl->addWidget(btnTitle, 0, 0);
    fl->addWidget(boxTitleFontSize, 0, 1);
    fl->addWidget(btnAxesLabels, 1, 0);
    fl->addWidget(boxAxesLabelFontSize, 1, 1);
    fl->addWidget(btnAxesNumbers, 2, 0);
    fl->addWidget(boxAxesNumberFontSize, 2, 1);
    fl->addWidget(btnLegend, 3, 0);
    fl->addWidget(boxLegendFontSize, 3, 1);
    fl->setRowStretch(4, 1);
    fl->setColumnStretch(2, 1);

    fontsPage = new QWidget();
    QHBoxLayout *hl = new QHBoxLayout(fontsPage);
    hl->addWidget(boxFonts);
    privateTabWidget->addTab(fontsPage, tr("Fonts"));

    connect(btnTitle, SIGNAL(clicked()), this, SLOT(setTitlesFont()));
    connect(btnAxesLabels, SIGNAL(clicked()), this, SLOT(setAxesLabelsFont()));
    connect(btnAxesNumbers, SIGNAL(clicked()), this, SLOT(setAxesNumbersFont()));
    connect(btnLegend, SIGNAL(clicked()), this, SLOT(setLegendsFont()));

    auto bindSize = [this](QDoubleSpinBox *box, QFont *font) {
        connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, font](double pt) {
                    if (d_loadingUi)
                        return;
                    font->setPointSizeF(qBound(PlotStyleLimits::kFontSizeMin, pt,
                                               PlotStyleLimits::kFontSizeMax));
                });
    };
    bindSize(boxTitleFontSize, &titleFont);
    bindSize(boxAxesLabelFontSize, &axesFont);
    bindSize(boxAxesNumberFontSize, &numbersFont);
    bindSize(boxLegendFontSize, &legendFont);
    syncFontSizeBoxes();
}

void PlotDialog::syncFontSizeBoxes()
{
    struct Row
    {
        QDoubleSpinBox *box;
        QFont *font;
    };
    const Row rows[] = { { boxTitleFontSize, &titleFont },
                         { boxAxesLabelFontSize, &axesFont },
                         { boxAxesNumberFontSize, &numbersFont },
                         { boxLegendFontSize, &legendFont } };
    for (const Row &r : rows) {
        if (!r.box)
            continue;
        *r.font = clampedFontSize(*r.font);
        QSignalBlocker blocker(r.box);
        r.box->setValue(qBound(PlotStyleLimits::kFontSizeMin,
                               PlotStyleLimits::effectiveFontPointSizeF(*r.font),
                               PlotStyleLimits::kFontSizeMax));
    }
}

void PlotDialog::initPageTab()
{
    pageTab = new QWidget();

    // Page size, unit, DPI, background, view mode, zoom and the fit buttons used to be edited here
    // as well as in Page Setup. Page Setup is the single editor now; this tab shows a summary
    // and leads to it. The print options (printPage) are embedded below.
    QGroupBox *gbPage = new QGroupBox(tr("Page Setup"));
    QGridLayout *gl = new QGridLayout(gbPage);

    gl->addWidget(new QLabel(tr("Size")), 0, 0);
    labelPageSize = new QLabel();
    labelPageSize->setTextInteractionFlags(Qt::TextSelectableByMouse);
    gl->addWidget(labelPageSize, 0, 1);

    gl->addWidget(new QLabel(tr("Zoom (%)")), 1, 0);
    labelPageZoom = new QLabel();
    gl->addWidget(labelPageZoom, 1, 1);

    QLabel *hint = new QLabel(
            tr("Edit the page size, unit, DPI, background, zoom and fitting in Page Setup."));
    hint->setWordWrap(true);
    gl->addWidget(hint, 2, 0, 1, 2);

    btnOpenPageSetup = new QPushButton(tr("Page Setup..."));
    gl->addWidget(btnOpenPageSetup, 3, 0, 1, 2);
    gl->setRowStretch(4, 1);
    gl->setColumnStretch(1, 1);

    QHBoxLayout *hl = new QHBoxLayout(pageTab);
    hl->addWidget(gbPage, 1);
    if (printPage)
        hl->addWidget(printPage, 1);

    privateTabWidget->addTab(pageTab, tr("Page"));

    connect(btnOpenPageSetup, SIGNAL(clicked()), this, SLOT(openPageSetupFromDialog()));
}

void PlotDialog::initLayerPage()
{
    layerPage = new QWidget();

    QGroupBox *boxBkg = new QGroupBox(tr("Border"));
    QGridLayout *boxBkgLayout = new QGridLayout(boxBkg);

    boxBkgLayout->addWidget(new QLabel(tr("Background Color")), 0, 0);
    boxBackgroundColor = new ColorButton();
    boxBkgLayout->addWidget(boxBackgroundColor, 0, 1);
    boxBkgLayout->addWidget(new QLabel(tr("Opacity")), 0, 2);
    // Opacity is shown as 0..100 % everywhere (the colour's 8-bit alpha is converted on load/commit).
    boxBackgroundTransparency = new QSpinBox();
    boxBackgroundTransparency->setRange(PlotStyleLimits::kOpacityMinPercent,
                                        PlotStyleLimits::kOpacityMaxPercent);
    boxBackgroundTransparency->setSingleStep(5);
    boxBackgroundTransparency->setSuffix(QStringLiteral(" %"));
    boxBackgroundTransparency->setSpecialValueText(tr("Transparent"));
    boxBkgLayout->addWidget(boxBackgroundTransparency, 0, 3);

    boxBkgLayout->addWidget(new QLabel(tr("Canvas Color")), 1, 0);
    boxCanvasColor = new ColorButton();
    boxBkgLayout->addWidget(boxCanvasColor, 1, 1);
    boxBkgLayout->addWidget(new QLabel(tr("Opacity")), 1, 2);
    boxCanvasTransparency = new QSpinBox();
    boxCanvasTransparency->setRange(PlotStyleLimits::kOpacityMinPercent,
                                    PlotStyleLimits::kOpacityMaxPercent);
    boxCanvasTransparency->setSingleStep(5);
    boxCanvasTransparency->setSuffix(QStringLiteral(" %"));
    boxCanvasTransparency->setSpecialValueText(tr("Transparent"));
    boxBkgLayout->addWidget(boxCanvasTransparency, 1, 3);

    boxBkgLayout->addWidget(new QLabel(tr("Border Color")), 2, 0);
    boxBorderColor = new ColorButton();
    boxBkgLayout->addWidget(boxBorderColor, 2, 1);

    boxBkgLayout->addWidget(new QLabel(tr("Width")), 2, 2);
    boxBorderWidth = new QSpinBox();
    boxBkgLayout->addWidget(boxBorderWidth, 2, 3);
    boxBkgLayout->setRowStretch(4, 1);

    QGroupBox *box4 = new QGroupBox(tr("Options"));
    QGridLayout *box4Layout = new QGridLayout(box4);

    box4Layout->addWidget(new QLabel(tr("Margin")), 0, 0);
    boxMargin = new QSpinBox();
    boxMargin->setRange(0, 1000);
    boxMargin->setSingleStep(5);
    box4Layout->addWidget(boxMargin, 0, 1);

    boxAntialiasing = new QCheckBox(tr("Antialiasing"));
    box4Layout->addWidget(boxAntialiasing, 1, 0, 1, 2);

    boxAll = new QCheckBox(tr("Apply to all layers"));
    box4Layout->addWidget(boxAll, 2, 0, 1, 2);
    box4Layout->setRowStretch(3, 1);

    QGroupBox *boxGeom = new QGroupBox(tr("Geometry"));
    QGridLayout *geomLayout = new QGridLayout(boxGeom);
    geomLayout->addWidget(new QLabel(tr("Position")), 0, 0);
    boxLayerX = new QSpinBox();
    boxLayerX->setRange(-2000, 10000);
    boxLayerY = new QSpinBox();
    boxLayerY->setRange(-2000, 10000);
    geomLayout->addWidget(boxLayerX, 0, 1);
    geomLayout->addWidget(boxLayerY, 0, 2);

    geomLayout->addWidget(new QLabel(tr("Size")), 1, 0);
    boxLayerW = new QSpinBox();
    boxLayerW->setRange(20, 10000);
    boxLayerH = new QSpinBox();
    boxLayerH->setRange(20, 10000);
    geomLayout->addWidget(boxLayerW, 1, 1);
    geomLayout->addWidget(boxLayerH, 1, 2);

    boxKeepAspect = new QCheckBox(tr("Keep aspect ratio"));
    geomLayout->addWidget(boxKeepAspect, 2, 0, 1, 2);
    boxAspectRatio = new QDoubleSpinBox();
    boxAspectRatio->setRange(0.1, 10.0);
    boxAspectRatio->setDecimals(3);
    boxAspectRatio->setSingleStep(0.05);
    boxAspectRatio->setValue(4.0 / 3.0);
    geomLayout->addWidget(boxAspectRatio, 2, 2);

    geomLayout->addWidget(new QLabel(tr("Frame Presets")), 3, 0);
    boxFramePreset = new QComboBox();
    boxFramePreset->addItem(tr("Current"), -1);
    boxFramePreset->addItem(tr("4-Sided Box [Inward Ticks]"), (int)Graph::FrameBoxInwardTicks);
    boxFramePreset->addItem(tr("4-Sided Box [Plain Top/Right]"), (int)Graph::FrameBoxPlainOpposite);
    boxFramePreset->addItem(tr("Open L-Frame"), (int)Graph::FrameOpenL);
    boxFramePreset->addItem(tr("Zero-Crossed"), (int)Graph::FrameZeroCrossed);
    geomLayout->addWidget(boxFramePreset, 3, 1, 1, 2);
    geomLayout->setRowStretch(4, 1);

    QGroupBox *boxLink = new QGroupBox(tr("Link Axes Scales"));
    QGridLayout *linkLayout = new QGridLayout(boxLink);
    linkLayout->addWidget(new QLabel(tr("Parent Layer")), 0, 0);
    boxLinkParentLayer = new QComboBox();
    boxLinkParentLayer->addItem(tr("None"), -1);
    linkLayout->addWidget(boxLinkParentLayer, 0, 1, 1, 2);

    linkLayout->addWidget(new QLabel(tr("X Link Mode")), 1, 0);
    boxLinkXMode = new QComboBox();
    boxLinkXMode->addItem(tr("None"));
    boxLinkXMode->addItem(tr("Straight (1:1)"));
    boxLinkXMode->addItem(tr("Custom Formula"));
    linkLayout->addWidget(boxLinkXMode, 1, 1);
    editLinkXFormula = new QLineEdit("x");
    linkLayout->addWidget(editLinkXFormula, 1, 2);

    linkLayout->addWidget(new QLabel(tr("Y Link Mode")), 2, 0);
    boxLinkYMode = new QComboBox();
    boxLinkYMode->addItem(tr("None"));
    boxLinkYMode->addItem(tr("Straight (1:1)"));
    boxLinkYMode->addItem(tr("Custom Formula"));
    linkLayout->addWidget(boxLinkYMode, 2, 1);
    editLinkYFormula = new QLineEdit("y");
    linkLayout->addWidget(editLinkYFormula, 2, 2);
    linkLayout->setRowStretch(3, 1);

    QVBoxLayout *leftCol = new QVBoxLayout();
    leftCol->addWidget(boxBkg);
    leftCol->addWidget(box4);

    QVBoxLayout *rightCol = new QVBoxLayout();
    rightCol->addWidget(boxGeom);
    rightCol->addWidget(boxLink);

    QHBoxLayout *hl = new QHBoxLayout(layerPage);
    hl->addLayout(leftCol, 1);
    hl->addLayout(rightCol, 1);

    privateTabWidget->addTab(layerPage, tr("Layer"));

    connect(boxBackgroundTransparency, SIGNAL(valueChanged(int)), this,
            SLOT(updateBackgroundTransparency(int)));
    connect(boxCanvasTransparency, SIGNAL(valueChanged(int)), this,
            SLOT(updateCanvasTransparency(int)));
    connect(boxAntialiasing, SIGNAL(toggled(bool)), this, SLOT(updateAntialiasing(bool)));
    connect(boxMargin, SIGNAL(valueChanged(int)), this, SLOT(changeMargin(int)));
    connect(boxBorderColor, SIGNAL(changed(QColor)), this, SLOT(pickBorderColor(QColor)));
    connect(boxBackgroundColor, SIGNAL(changed(QColor)), this, SLOT(pickBackgroundColor(QColor)));
    connect(boxCanvasColor, SIGNAL(changed(QColor)), this, SLOT(pickCanvasColor(QColor)));
    connect(boxBorderWidth, SIGNAL(valueChanged(int)), this, SLOT(updateBorder(int)));
}

void PlotDialog::initPiePage()
{
    piePage = new QWidget();

    QGridLayout *gl1 = new QGridLayout();
    gl1->addWidget(new QLabel(tr("Color")), 0, 0);

    boxPieLineColor = new ColorButton();
    gl1->addWidget(boxPieLineColor, 0, 1);

    gl1->addWidget(new QLabel(tr("Style")), 1, 0);
    boxPieLineStyle = new QComboBox();
    boxPieLineStyle->addItem("_____");
    boxPieLineStyle->addItem("- - -");
    boxPieLineStyle->addItem(".....");
    boxPieLineStyle->addItem("_._._");
    boxPieLineStyle->addItem("_.._..");
    gl1->addWidget(boxPieLineStyle);

    gl1->addWidget(new QLabel(tr("Width")), 2, 0);
    boxPieLineWidth = new QSpinBox();
    gl1->addWidget(boxPieLineWidth, 2, 1);
    gl1->setRowStretch(3, 1);

    QGroupBox *gb1 = new QGroupBox(tr("Border"));
    gb1->setLayout(gl1);

    QGridLayout *gl2 = new QGridLayout();
    gl2->addWidget(new QLabel(tr("First color")), 0, 0);

    boxFirstColor = new ColorButton();
    gl2->addWidget(boxFirstColor, 0, 1);

    gl2->addWidget(new QLabel(tr("Pattern")), 1, 0);
    boxPiePattern = new PatternBox();
    gl2->addWidget(boxPiePattern, 1, 1);
    gl2->addWidget(new QLabel(tr("Pie radius")), 2, 0);

    boxRadius = new QSpinBox();
    boxRadius->setRange(0, 10000);
    boxRadius->setSingleStep(10);

    gl2->addWidget(boxRadius, 2, 1);
    gl2->setRowStretch(3, 1);

    QGroupBox *gb2 = new QGroupBox(tr("Fill"));
    gb2->setLayout(gl2);

    QHBoxLayout *hl = new QHBoxLayout();
    hl->addWidget(gb1);
    hl->addWidget(gb2);
    piePage->setLayout(hl);

    privateTabWidget->addTab(piePage, tr("Pie"));
}

void PlotDialog::initPrintPage()
{
    // Not a tab of its own any more: initPageTab() embeds this widget in the Page tab, which keeps
    // the three print/page/font groups of the Page node down to two tabs (Page, Fonts).
    QGroupBox *gb = new QGroupBox(tr("Print"));
    QVBoxLayout *vl = new QVBoxLayout(gb);
    boxScaleLayers = new QCheckBox(tr("&Scale layers to paper size"));
    vl->addWidget(boxScaleLayers);
    boxPrintCrops = new QCheckBox(tr("Print Crop &Marks"));
    vl->addWidget(boxPrintCrops);
    vl->addStretch();

    printPage = new QWidget();
    QHBoxLayout *hlayout = new QHBoxLayout(printPage);
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->addWidget(gb);
}

void PlotDialog::initAxesPage()
{
    QGroupBox *gb = new QGroupBox(tr("Attach curve to: "));
    QGridLayout *gl = new QGridLayout(gb);
    gl->addWidget(new QLabel(tr("x Axis")), 0, 0);
    boxXAxis = new QComboBox();
    boxXAxis->setEditable(false);
    boxXAxis->addItem(tr("Bottom"));
    boxXAxis->addItem(tr("Top"));
    gl->addWidget(boxXAxis, 0, 1);
    gl->addWidget(new QLabel(tr("y Axis")), 1, 0);
    boxYAxis = new QComboBox();
    boxYAxis->setEditable(false);
    boxYAxis->addItem(tr("Left"));
    boxYAxis->addItem(tr("Right"));
    gl->addWidget(boxYAxis, 1, 1);
    gl->setRowStretch(2, 1);
    gl->setColumnStretch(2, 1);

    axesPage = new QWidget();
    QHBoxLayout *hlayout = new QHBoxLayout(axesPage);
    hlayout->addWidget(gb);
    privateTabWidget->addTab(axesPage, tr("Axes"));
}

void PlotDialog::initLinePage()
{
    QGroupBox *gb = new QGroupBox();
    QGridLayout *gl1 = new QGridLayout(gb);
    gl1->addWidget(new QLabel(tr("Connect")), 0, 0);

    boxConnect = new QComboBox();
    boxConnect->setEditable(false);
    boxConnect->addItem(tr("No line"));
    boxConnect->addItem(tr("Lines"));
    boxConnect->addItem(tr("Sticks"));
    boxConnect->addItem(tr("Horizontal Steps"));
    boxConnect->addItem(tr("Dots"));
    boxConnect->addItem(tr("Spline"));
    boxConnect->addItem(tr("Vertical Steps"));
    gl1->addWidget(boxConnect, 0, 1);

    penWidget = new PenWidget(this, QPen());
    gl1->addWidget(penWidget, 1, 0, 1, 2);

    gl1->addWidget(new QLabel(tr("Opacity (%):")), 2, 0);
    boxLineOpacity = new QSpinBox();
    boxLineOpacity->setRange(PlotStyleLimits::kOpacityMinPercent,
                             PlotStyleLimits::kOpacityMaxPercent);
    boxLineOpacity->setSuffix(" %");
    boxLineOpacity->setValue(100);
    gl1->addWidget(boxLineOpacity, 2, 1);

    fillGroupBox = new QGroupBox(tr("Fill area under curve"));
    fillGroupBox->setCheckable(true);
    QGridLayout *gl2 = new QGridLayout(fillGroupBox);
    gl2->addWidget(new QLabel(tr("Fill color")), 0, 0);
    boxAreaColor = new ColorButton();
    gl2->addWidget(boxAreaColor, 0, 1);
    gl2->addWidget(new QLabel(tr("Pattern")), 1, 0);
    boxPattern = new PatternBox();
    gl2->addWidget(boxPattern, 1, 1);
    gl2->addWidget(new QLabel(tr("Opacity (%):")), 2, 0);
    boxAreaOpacity = new QSpinBox();
    boxAreaOpacity->setRange(PlotStyleLimits::kOpacityMinPercent,
                             PlotStyleLimits::kOpacityMaxPercent);
    boxAreaOpacity->setSuffix(" %");
    boxAreaOpacity->setValue(100);
    gl2->addWidget(boxAreaOpacity, 2, 1);
    gl2->setRowStretch(3, 1);

    linePage = new QWidget();
    QHBoxLayout *hlayout = new QHBoxLayout(linePage);
    hlayout->addWidget(gb);
    hlayout->addWidget(fillGroupBox);
    privateTabWidget->addTab(linePage, tr("Line"));

    connect(fillGroupBox, SIGNAL(toggled(bool)), this, SLOT(showAreaColor(bool)));
}

void PlotDialog::initSymbolsPage()
{
    QGroupBox *gb = new QGroupBox();
    QGridLayout *gl = new QGridLayout(gb);
    gl->addWidget(new QLabel(tr("Style")), 0, 0);
    boxSymbolStyle = new SymbolBox();
    gl->addWidget(boxSymbolStyle, 0, 1);
    gl->addWidget(new QLabel(tr("Size")), 1, 0);
    boxSymbolSize = new QSpinBox();
    boxSymbolSize->setRange(PlotStyleLimits::kSymbolSizeMin, PlotStyleLimits::kSymbolSizeMax);
    boxSymbolSize->setValue(9);
    gl->addWidget(boxSymbolSize, 1, 1);
    boxFillSymbol = new QCheckBox(tr("Fill Color"));
    gl->addWidget(boxFillSymbol, 2, 0);
    boxFillColor = new ColorButton();
    gl->addWidget(boxFillColor, 2, 1);
    boxHollowSymbol = new QCheckBox(tr("Hollow Symbol (White Fill)"));
    gl->addWidget(boxHollowSymbol, 3, 0, 1, 2);
    gl->addWidget(new QLabel(tr("Edge Color")), 4, 0);
    boxSymbolColor = new ColorButton();
    gl->addWidget(boxSymbolColor, 4, 1);
    gl->addWidget(new QLabel(tr("Edge Width")), 5, 0);
    boxPenWidth = new QDoubleSpinBox();
    boxPenWidth->setRange(PlotStyleLimits::kSymbolEdgeWidthMin,
                          PlotStyleLimits::kSymbolEdgeWidthMax);
    boxPenWidth->setDecimals(1);
    boxPenWidth->setSingleStep(0.5);
    boxPenWidth->setSuffix(QStringLiteral(" px"));
    boxPenWidth->setValue(1.0);
    gl->addWidget(boxPenWidth, 5, 1);
    gl->addWidget(new QLabel(tr("Opacity (%):")), 6, 0);
    boxSymbolOpacity = new QSpinBox();
    boxSymbolOpacity->setRange(PlotStyleLimits::kOpacityMinPercent,
                               PlotStyleLimits::kOpacityMaxPercent);
    boxSymbolOpacity->setSuffix(" %");
    boxSymbolOpacity->setValue(100);
    gl->addWidget(boxSymbolOpacity, 6, 1);
    gl->setRowStretch(7, 1);
    gl->setColumnStretch(2, 1);

    symbolPage = new QWidget();
    QHBoxLayout *hl = new QHBoxLayout(symbolPage);
    hl->addWidget(gb);

    privateTabWidget->addTab(symbolPage, tr("Symbol"));

    connect(boxFillSymbol, SIGNAL(clicked()), this, SLOT(fillSymbols()));
    connect(boxHollowSymbol, SIGNAL(toggled(bool)), this, SLOT(onHollowSymbolToggled(bool)));
    connect(boxFillColor, &ColorButton::changed, this, [this](QColor c) {
        if (d_loadingUi || !boxHollowSymbol)
            return;
        if (boxHollowSymbol->isChecked()
            && !(c.red() == 255 && c.green() == 255 && c.blue() == 255)) {
            QSignalBlocker blocker(boxHollowSymbol);
            boxHollowSymbol->setChecked(false);
        }
        markTabDirty(symbolPage);
    });
}

void PlotDialog::initBoxPage()
{
    QGroupBox *gb1 = new QGroupBox(tr("Box"));
    QGridLayout *gl1 = new QGridLayout(gb1);
    gl1->addWidget(new QLabel(tr("Type")), 0, 0);

    boxType = new QComboBox();
    boxType->setEditable(false);
    boxType->addItem(tr("No Box"));
    boxType->addItem(tr("Rectangle"));
    boxType->addItem(tr("Diamond"));
    boxType->addItem(tr("Perc 10, 25, 75, 90"));
    boxType->addItem(tr("Notch"));
    gl1->addWidget(boxType, 0, 1);

    boxRangeLabel = new QLabel(tr("Range"));
    gl1->addWidget(boxRangeLabel, 1, 0);
    boxRange = new QComboBox();
    boxRange->setEditable(false);
    boxRange->addItem(tr("Standard Deviation"));
    boxRange->addItem(tr("Standard Error"));
    boxRange->addItem(tr("Perc 25, 75"));
    boxRange->addItem(tr("Perc 10, 90"));
    boxRange->addItem(tr("Perc 5, 95"));
    boxRange->addItem(tr("Perc 1, 99"));
    boxRange->addItem(tr("Max-Min"));
    boxRange->addItem(tr("Constant"));
    gl1->addWidget(boxRange, 1, 1);

    boxCoeffLabel = new QLabel(tr("Percentile (%)"));
    gl1->addWidget(boxCoeffLabel, 2, 0);
    boxCoef = new QSpinBox();
    boxCoef->setRange(50, 100);
    boxCoef->setSingleStep(5);
    gl1->addWidget(boxCoef, 2, 1);

    boxCntLabel = new QLabel(tr("Coefficient"));
    gl1->addWidget(boxCntLabel, 3, 0);
    boxCnt = new QDoubleSpinBox();
    boxCnt->setRange(0.0, 100.0);
    boxCnt->setSingleStep(0.01);
    boxCnt->setValue(1.0);
    gl1->addWidget(boxCnt, 3, 1);

    gl1->addWidget(new QLabel(tr("Box Width")), 4, 0);
    boxWidth = new QSpinBox();
    boxWidth->setRange(0, 100);
    boxWidth->setSingleStep(5);
    gl1->addWidget(boxWidth, 4, 1);

    boxViolinMode = new QCheckBox(tr("Violin Plot (KDE Contour)"));
    gl1->addWidget(boxViolinMode, 5, 0, 1, 2);

    boxShowDataPoints = new QCheckBox(tr("Show Jittered Data Points"));
    gl1->addWidget(boxShowDataPoints, 6, 0, 1, 2);

    QGroupBox *gb2 = new QGroupBox(tr("Whiskers"));
    QGridLayout *gl2 = new QGridLayout(gb2);
    whiskerRangeLabel = new QLabel(tr("Range"));
    gl2->addWidget(whiskerRangeLabel, 0, 0);

    boxWhiskersRange = new QComboBox();
    boxWhiskersRange->setEditable(false);
    boxWhiskersRange->addItem(tr("No Whiskers"));
    boxWhiskersRange->addItem(tr("Standard Deviation"));
    boxWhiskersRange->addItem(tr("Standard Error"));
    boxWhiskersRange->addItem(tr("75-25"));
    boxWhiskersRange->addItem(tr("90-10"));
    boxWhiskersRange->addItem(tr("95-5"));
    boxWhiskersRange->addItem(tr("99-1"));
    boxWhiskersRange->addItem(tr("Max-Min"));
    boxWhiskersRange->addItem(tr("Constant"));
    // Item index == BoxCurve::Range value (None = 0 ... UserDef = 8, Tukey15IQR = 9).
    boxWhiskersRange->addItem(tr("Tukey 1.5*IQR"));
    gl2->addWidget(boxWhiskersRange, 0, 1);

    whiskerCoeffLabel = new QLabel(tr("Percentile (%)"));
    gl2->addWidget(whiskerCoeffLabel, 1, 0);
    boxWhiskersCoef = new QSpinBox();
    boxWhiskersCoef->setRange(50, 100);
    boxWhiskersCoef->setSingleStep(5);
    gl2->addWidget(boxWhiskersCoef, 1, 1);

    whiskerCntLabel = new QLabel(tr("Coef"));
    gl2->addWidget(whiskerCntLabel, 2, 0);
    whiskerCnt = new QDoubleSpinBox();
    whiskerCnt->setRange(0.0, 100.0);
    whiskerCnt->setSingleStep(0.01);
    whiskerCnt->setValue(1.0);
    gl2->addWidget(whiskerCnt, 2, 1);

    QVBoxLayout *vl1 = new QVBoxLayout();
    vl1->addWidget(gb1);
    vl1->addStretch();

    QVBoxLayout *vl2 = new QVBoxLayout();
    vl2->addWidget(gb2);
    vl2->addStretch();

    boxPage = new QWidget();
    QHBoxLayout *hl = new QHBoxLayout(boxPage);
    hl->addLayout(vl1);
    hl->addLayout(vl2);
    privateTabWidget->addTab(boxPage, tr("Box/Whiskers"));

    connect(boxType, SIGNAL(activated(int)), this, SLOT(setBoxType(int)));
    connect(boxRange, SIGNAL(activated(int)), this, SLOT(setBoxRangeType(int)));
    connect(boxWhiskersRange, SIGNAL(activated(int)), this, SLOT(setWhiskersRange(int)));
}

void PlotDialog::initPercentilePage()
{
    QGroupBox *gb1 = new QGroupBox(tr("Type"));
    QGridLayout *gl1 = new QGridLayout(gb1);
    gl1->addWidget(new QLabel(tr("Max")), 0, 0);

    boxMaxStyle = new SymbolBox();
    gl1->addWidget(boxMaxStyle, 0, 1);

    gl1->addWidget(new QLabel(tr("99%")), 1, 0);
    box99Style = new SymbolBox();
    gl1->addWidget(box99Style, 1, 1);

    gl1->addWidget(new QLabel(tr("Mean")), 2, 0);
    boxMeanStyle = new SymbolBox();
    gl1->addWidget(boxMeanStyle, 2, 1);

    gl1->addWidget(new QLabel(tr("1%")), 3, 0);
    box1Style = new SymbolBox();
    gl1->addWidget(box1Style, 3, 1);

    gl1->addWidget(new QLabel(tr("Min")), 4, 0);
    boxMinStyle = new SymbolBox();
    gl1->addWidget(boxMinStyle, 4, 1);
    gl1->setRowStretch(5, 1);

    QGroupBox *gb2 = new QGroupBox(tr("Symbol"));
    QGridLayout *gl2 = new QGridLayout(gb2);
    gl2->addWidget(new QLabel(tr("Size")), 0, 0);

    boxPercSize = new QSpinBox();
    boxPercSize->setMinimum(1);
    gl2->addWidget(boxPercSize, 0, 1);

    boxFillSymbols = new QCheckBox(tr("Fill Color"));
    gl2->addWidget(boxFillSymbols, 1, 0);
    boxPercFillColor = new ColorButton();
    gl2->addWidget(boxPercFillColor, 1, 1);

    gl2->addWidget(new QLabel(tr("Edge Color")), 2, 0);
    boxEdgeColor = new ColorButton();
    gl2->addWidget(boxEdgeColor, 2, 1);

    gl2->addWidget(new QLabel(tr("Edge Width")), 3, 0);
    boxEdgeWidth = new QDoubleSpinBox();
    boxEdgeWidth->setRange(PlotStyleLimits::kSymbolEdgeWidthMin,
                           PlotStyleLimits::kSymbolEdgeWidthMax);
    boxEdgeWidth->setDecimals(1);
    boxEdgeWidth->setSingleStep(0.5);
    boxEdgeWidth->setSuffix(QStringLiteral(" px"));
    boxEdgeWidth->setValue(1.0);
    gl2->addWidget(boxEdgeWidth, 3, 1);
    gl2->setRowStretch(4, 1);

    percentilePage = new QWidget();
    QHBoxLayout *hl = new QHBoxLayout(percentilePage);
    hl->addWidget(gb1);
    hl->addWidget(gb2);
    privateTabWidget->addTab(percentilePage, tr("Percentile"));

    connect(boxFillSymbols, SIGNAL(clicked()), this, SLOT(fillBoxSymbols()));
}

void PlotDialog::initSpectrogramPage()
{
    spectrogramPage = new QWidget();

    imageGroupBox = new QGroupBox(tr("Image"));
    imageGroupBox->setCheckable(true);

    QVBoxLayout *vl = new QVBoxLayout();
    vl->addWidget(new QLabel(tr("Palette Preset")));
    boxColorMapPreset = new QComboBox();
    boxColorMapPreset->addItem(tr("Default (Rainbow)"));
    boxColorMapPreset->addItem(tr("Gray Scale"));
    boxColorMapPreset->addItem(tr("Viridis"));
    boxColorMapPreset->addItem(tr("Magma"));
    boxColorMapPreset->addItem(tr("Inferno"));
    boxColorMapPreset->addItem(tr("Plasma"));
    boxColorMapPreset->addItem(tr("Cividis"));
    boxColorMapPreset->addItem(tr("Jet"));
    boxColorMapPreset->addItem(tr("Blue-White-Red"));
    boxColorMapPreset->addItem(tr("Cool-Warm"));
    boxColorMapPreset->addItem(tr("Custom Color Map"));
    connect(boxColorMapPreset, SIGNAL(currentIndexChanged(int)), this,
            SLOT(onColorMapPresetChanged(int)));
    vl->addWidget(boxColorMapPreset);

    boxReversePalette = new QCheckBox(tr("Reverse Palette"));
    connect(boxReversePalette, SIGNAL(toggled(bool)), this, SLOT(onReversePaletteToggled(bool)));
    vl->addWidget(boxReversePalette);

    // The Palette Preset combo is the only colour-map selector (the former hidden radio buttons
    // are gone). Spectrogram data options (E11b):
    boxLogColorScale = new QCheckBox(tr("Log color scale (log10 z)"));
    boxLogColorScale->setObjectName(QStringLiteral("PlotDialogLogColorScale"));
    boxLogColorScale->setToolTip(
            tr("Colours follow log10(z); cells with z <= 0 are transparent."));
    vl->addWidget(boxLogColorScale);

    boxCellBlocks = new QCheckBox(tr("Cell blocks (no smoothing)"));
    boxCellBlocks->setObjectName(QStringLiteral("PlotDialogCellBlocks"));
    boxCellBlocks->setToolTip(tr("Draw each matrix cell as a flat block, like histogram bins."));
    vl->addWidget(boxCellBlocks);
    vl->addStretch();

    QHBoxLayout *hl = new QHBoxLayout(imageGroupBox);
    colorMapEditor = new ColorMapEditor();
    hl->addLayout(vl);
    hl->addWidget(colorMapEditor);

    levelsGroupBox = new QGroupBox(tr("Contour Lines"));
    levelsGroupBox->setCheckable(true);

    QHBoxLayout *hl1 = new QHBoxLayout();
    hl1->addWidget(new QLabel(tr("Levels")));

    levelsBox = new QSpinBox();
    levelsBox->setRange(2, 1000);
    hl1->addWidget(levelsBox);
    hl1->addStretch();

    QVBoxLayout *vl1 = new QVBoxLayout();
    vl1->addLayout(hl1);

    autoContourBox = new QRadioButton(tr("Use &Color Map"));
    connect(autoContourBox, SIGNAL(toggled(bool)), this, SLOT(showDefaultContourLinesBox(bool)));
    vl1->addWidget(autoContourBox);

    defaultContourBox = new QRadioButton(tr("Use Default &Pen"));
    connect(defaultContourBox, SIGNAL(toggled(bool)), this, SLOT(showDefaultContourLinesBox(bool)));
    vl1->addWidget(defaultContourBox);

    QHBoxLayout *hl2 = new QHBoxLayout(levelsGroupBox);
    hl2->addLayout(vl1);

    defaultPenBox = new QGroupBox();
    QGridLayout *gl1 = new QGridLayout(defaultPenBox);
    gl1->addWidget(new QLabel(tr("Color")), 0, 0);

    levelsColorBox = new ColorButton(defaultPenBox);
    gl1->addWidget(levelsColorBox, 0, 1);

    gl1->addWidget(new QLabel(tr("Width")), 1, 0);
    contourWidthBox = new QSpinBox();
    gl1->addWidget(contourWidthBox, 1, 1);

    gl1->addWidget(new QLabel(tr("Style")), 2, 0);
    boxContourStyle = new QComboBox();
    boxContourStyle->setEditable(false);
    boxContourStyle->addItem("_____");
    boxContourStyle->addItem("_ _ _");
    boxContourStyle->addItem(".....");
    boxContourStyle->addItem("_._._");
    boxContourStyle->addItem("_.._..");
    gl1->addWidget(boxContourStyle, 2, 1);
    hl2->addWidget(defaultPenBox);

    axisScaleBox = new QGroupBox(tr("Color Bar Scale"));
    axisScaleBox->setCheckable(true);

    QGridLayout *gl2 = new QGridLayout(axisScaleBox);
    gl2->addWidget(new QLabel(tr("Axis")), 0, 0);

    colorScaleBox = new QComboBox();
    colorScaleBox->addItem(tr("Left"));
    colorScaleBox->addItem(tr("Right"));
    colorScaleBox->addItem(tr("Bottom"));
    colorScaleBox->addItem(tr("Top"));
    gl2->addWidget(colorScaleBox, 0, 1);

    gl2->addWidget(new QLabel(tr("Width")), 1, 0);
    colorScaleWidthBox = new QSpinBox();
    colorScaleWidthBox->setRange(2, 10000);
    gl2->addWidget(colorScaleWidthBox, 1, 1);

    gl2->addWidget(new QLabel(tr("Colorbar Title")), 2, 0);
    editColorBarTitle = new QLineEdit();
    gl2->addWidget(editColorBarTitle, 2, 1, 1, 3);

    boxCustomZRange = new QCheckBox(tr("Custom Z Range"));
    connect(boxCustomZRange, SIGNAL(toggled(bool)), this, SLOT(onCustomZRangeToggled(bool)));
    gl2->addWidget(boxCustomZRange, 3, 0, 1, 4);

    gl2->addWidget(new QLabel(tr("Z Min")), 4, 0);
    boxZMin = new QDoubleSpinBox();
    boxZMin->setRange(-1e300, 1e300);
    boxZMin->setDecimals(6);
    boxZMin->setEnabled(false);
    gl2->addWidget(boxZMin, 4, 1);

    gl2->addWidget(new QLabel(tr("Z Max")), 4, 2);
    boxZMax = new QDoubleSpinBox();
    boxZMax->setRange(-1e300, 1e300);
    boxZMax->setDecimals(6);
    boxZMax->setEnabled(false);
    gl2->addWidget(boxZMax, 4, 3);

    QVBoxLayout *vl2 = new QVBoxLayout(spectrogramPage);
    vl2->addWidget(imageGroupBox);
    vl2->addWidget(levelsGroupBox);
    vl2->addWidget(axisScaleBox);
    vl2->addStretch();

    privateTabWidget->addTab(spectrogramPage, tr("Contour") + " / " + tr("Image"));
}

void PlotDialog::fillBoxSymbols()
{
    boxPercFillColor->setEnabled(boxFillSymbols->isChecked());
    markTabDirty(percentilePage);
}

void PlotDialog::fillSymbols()
{
    boxFillColor->setEnabled(boxFillSymbol->isChecked());
    if (!boxFillSymbol->isChecked() && boxHollowSymbol && boxHollowSymbol->isChecked()) {
        QSignalBlocker blocker(boxHollowSymbol);
        boxHollowSymbol->setChecked(false);
    }
    markTabDirty(symbolPage);
}

void PlotDialog::onHollowSymbolToggled(bool checked)
{
    if (d_loadingUi)
        return;

    QScopedValueRollback<bool> guard(d_loadingUi, true);
    if (checked) {
        if (boxSymbolStyle->selectedSymbol() == QwtSymbol::NoSymbol)
            boxSymbolStyle->setCurrentIndex(1);

        QColor edgeCol = boxSymbolColor->color();
        if (!edgeCol.isValid() || (edgeCol.red() == 255 && edgeCol.green() == 255 && edgeCol.blue() == 255)) {
            QColor penCol = penWidget ? penWidget->pen().color() : QColor();
            if (!penCol.isValid()
                || (penCol.red() == 255 && penCol.green() == 255 && penCol.blue() == 255))
                penCol = QColor(Qt::black);
            boxSymbolColor->setColor(penCol);
        }
        if (boxPenWidth->value() < PlotStyleLimits::kSymbolEdgeWidthMin)
            boxPenWidth->setValue(1.0);
        boxFillSymbol->setChecked(true);
        boxFillColor->setEnabled(true);
        boxFillColor->setColor(QColor(255, 255, 255));
    } else {
        QColor edgeCol = boxSymbolColor->color();
        if (!edgeCol.isValid()
            || (edgeCol.red() == 255 && edgeCol.green() == 255 && edgeCol.blue() == 255)) {
            edgeCol = penWidget ? penWidget->pen().color() : QColor();
            if (!edgeCol.isValid()
                || (edgeCol.red() == 255 && edgeCol.green() == 255 && edgeCol.blue() == 255))
                edgeCol = QColor(Qt::black);
        }
        boxFillSymbol->setChecked(true);
        boxFillColor->setEnabled(true);
        boxFillColor->setColor(edgeCol);
    }
    markTabDirty(symbolPage);
}

void PlotDialog::initErrorsPage()
{
    QGroupBox *gb1 = new QGroupBox(tr("Direction"));

    QVBoxLayout *vl = new QVBoxLayout(gb1);
    plusBox = new QCheckBox(tr("Plus"));
    vl->addWidget(plusBox);
    minusBox = new QCheckBox(tr("Minus"));
    vl->addWidget(minusBox);
    xBox = new QCheckBox(tr("&X Error Bar"));
    vl->addWidget(xBox);
    vl->addStretch();

    QGroupBox *gb2 = new QGroupBox(tr("Style"));
    QGridLayout *gl = new QGridLayout(gb2);
    gl->addWidget(new QLabel(tr("Color")), 0, 0);

    colorBox = new ColorButton();
    gl->addWidget(colorBox, 0, 1);

    gl->addWidget(new QLabel(tr("Line Width")), 1, 0);
    widthBox = new QDoubleSpinBox();
    widthBox->setRange(PlotStyleLimits::kErrorBarWidthMin, PlotStyleLimits::kErrorBarWidthMax);
    widthBox->setDecimals(1);
    widthBox->setSingleStep(PlotStyleLimits::kErrorBarWidthStep);
    widthBox->setSuffix(QStringLiteral(" px"));
    widthBox->setValue(1.0);
    gl->addWidget(widthBox, 1, 1);

    gl->addWidget(new QLabel(tr("Cap Width")), 2, 0);
    capBox = new QComboBox();
    capBox->addItem(tr("8"));
    capBox->addItem(tr("10"));
    capBox->addItem(tr("12"));
    capBox->addItem(tr("16"));
    capBox->addItem(tr("20"));
    capBox->setEditable(true);
    gl->addWidget(capBox, 2, 1);

    throughBox = new QCheckBox(tr("Through Symbol"));
    gl->addWidget(throughBox, 3, 0);
    gl->setRowStretch(4, 1);

    errorsPage = new QWidget();
    QHBoxLayout *hl = new QHBoxLayout(errorsPage);
    hl->addWidget(gb1);
    hl->addWidget(gb2);
    privateTabWidget->addTab(errorsPage, tr("Error Bars"));

    connect(xBox, SIGNAL(clicked()), this, SLOT(changeErrorBarsType()));
    connect(plusBox, SIGNAL(clicked()), this, SLOT(changeErrorBarsPlus()));
    connect(minusBox, SIGNAL(clicked()), this, SLOT(changeErrorBarsMinus()));
    connect(throughBox, SIGNAL(clicked()), this, SLOT(changeErrorBarsThrough()));
    connect(colorBox, SIGNAL(changed(QColor)), this, SLOT(pickErrorBarsColor(QColor)));
}

void PlotDialog::initHistogramPage()
{
    QHBoxLayout *hl = new QHBoxLayout();
    automaticBox = new QCheckBox(tr("Automatic Binning"));
    hl->addWidget(automaticBox);
    hl->addStretch();
    buttonStatistics = new QPushButton(tr("&Show statistics"));
    hl->addWidget(buttonStatistics);

    GroupBoxH = new QGroupBox();
    QGridLayout *gl = new QGridLayout(GroupBoxH);
    gl->addWidget(new QLabel(tr("Bin Size")), 0, 0);
    binSizeBox = new QLineEdit();
    gl->addWidget(binSizeBox, 0, 1);
    gl->addWidget(new QLabel(tr("Begin")), 1, 0);
    histogramBeginBox = new QLineEdit();
    gl->addWidget(histogramBeginBox, 1, 1);
    gl->addWidget(new QLabel(tr("End")), 2, 0);
    histogramEndBox = new QLineEdit();
    gl->addWidget(histogramEndBox, 2, 1);

    histogramPage = new QWidget();
    QVBoxLayout *vl = new QVBoxLayout(histogramPage);
    vl->addLayout(hl);
    vl->addWidget(GroupBoxH);
    vl->addStretch();

    privateTabWidget->addTab(histogramPage, tr("Histogram Data"));

    connect(automaticBox, SIGNAL(clicked()), this, SLOT(setAutomaticBinning()));
    connect(buttonStatistics, SIGNAL(clicked()), this, SLOT(showStatistics()));
}

void PlotDialog::initSpacingPage()
{
    spacingPage = new QWidget();

    QGridLayout *gl = new QGridLayout(spacingPage);
    gl->addWidget(new QLabel(tr("Gap Between Bars (in %)")), 0, 0);
    gapBox = new QSpinBox();
    gapBox->setRange(0, 100);
    gapBox->setSingleStep(10);
    gl->addWidget(gapBox, 0, 1);
    gl->addWidget(new QLabel(tr("Offset (in %)")), 1, 0);
    offsetBox = new QSpinBox();
    offsetBox->setRange(-1000, 1000);
    offsetBox->setSingleStep(50);
    gl->addWidget(offsetBox, 1, 1);
    gl->setRowStretch(2, 1);

    privateTabWidget->addTab(spacingPage, tr("Spacing"));
}

void PlotDialog::initVectPage()
{
    QGroupBox *gb1 = new QGroupBox();
    QGridLayout *gl1 = new QGridLayout(gb1);
    gl1->addWidget(new QLabel(tr("Color")), 0, 0);
    vectColorBox = new ColorButton();
    gl1->addWidget(vectColorBox, 0, 1);
    gl1->addWidget(new QLabel(tr("Line Width")), 1, 0);
    vectWidthBox = new QSpinBox();
    vectWidthBox->setRange(0, 100);
    gl1->addWidget(vectWidthBox, 1, 1);

    QGroupBox *gb2 = new QGroupBox(tr("Arrowheads"));
    QGridLayout *gl2 = new QGridLayout(gb2);
    gl2->addWidget(new QLabel(tr("Length")), 0, 0);
    headLengthBox = new QSpinBox();
    headLengthBox->setRange(0, 100);
    gl2->addWidget(headLengthBox, 0, 1);
    gl2->addWidget(new QLabel(tr("Angle")), 1, 0);
    headAngleBox = new QSpinBox();
    headAngleBox->setRange(0, 85);
    headAngleBox->setSingleStep(5);
    gl2->addWidget(headAngleBox, 1, 1);
    filledHeadBox = new QCheckBox(tr("&Filled"));
    gl2->addWidget(filledHeadBox, 2, 0);
    gl2->setRowStretch(3, 1);

    GroupBoxVectEnd = new QGroupBox(tr("End Point"));
    QGridLayout *gl3 = new QGridLayout(GroupBoxVectEnd);
    labelXEnd = new QLabel(tr("X End"));
    gl3->addWidget(labelXEnd, 0, 0);
    xEndBox = new QComboBox();
    gl3->addWidget(xEndBox, 0, 1);

    labelYEnd = new QLabel(tr("Y End"));
    gl3->addWidget(labelYEnd, 1, 0);
    yEndBox = new QComboBox();
    gl3->addWidget(yEndBox, 1, 1);

    labelPosition = new QLabel(tr("Position"));
    gl3->addWidget(labelPosition, 2, 0);
    vectPosBox = new QComboBox();
    vectPosBox->addItem(tr("Tail"));
    vectPosBox->addItem(tr("Middle"));
    vectPosBox->addItem(tr("Head"));
    gl3->addWidget(vectPosBox, 2, 1);
    gl3->setRowStretch(3, 1);

    vectPage = new QWidget();

    QVBoxLayout *vl1 = new QVBoxLayout();
    vl1->addWidget(gb1);
    vl1->addWidget(gb2);

    QHBoxLayout *hl = new QHBoxLayout(vectPage);
    hl->addLayout(vl1);
    hl->addWidget(GroupBoxVectEnd);

    privateTabWidget->addTab(vectPage, tr("Vector"));
}

void PlotDialog::setMultiLayer(MultiLayer *ml)
{
    if (!ml)
        return;

    QScopedValueRollback<bool> guard(d_loadingUi, true);
    if (d_mlGuard && d_mlGuard.data() != ml)
        disconnect(d_mlGuard.data(), &MultiLayer::modifiedPlot, this,
                   &PlotDialog::onMultiLayerModified);
    d_ml = ml;
    d_mlGuard = ml;
    d_dirtyTabs.clear();
    d_externalSyncDeferred = false;
    // Edits made elsewhere (right dock, canvas, other dialogs) while this dialog is open.
    connect(ml, &MultiLayer::modifiedPlot, this, &PlotDialog::onMultiLayerModified,
            Qt::UniqueConnection);

    boxScaleLayers->setChecked(d_ml->scaleLayersOnPrint());
    boxPrintCrops->setChecked(d_ml->printCropmarksEnabled());
    syncPageTabFromMultiLayer();

    populateTreeFromMultiLayer(true);
    d_dirtyTabs.clear();
    updateCurveMoveButtons();
}

//! Fills the page / layer / curve tree (and the "parent layer" combo) from d_mlGuard.
//! \a selectActiveLayer: make the MultiLayer's active layer the current item (initial load).
void PlotDialog::populateTreeFromMultiLayer(bool selectActiveLayer)
{
    MultiLayer *ml = d_mlGuard.data();
    if (!ml)
        return;

    listBox->clear();

    boxLinkParentLayer->clear();
    boxLinkParentLayer->addItem(tr("None"), -1);
    QWidgetList plots = ml->graphPtrs();
    for (int i = 0; i < plots.count(); ++i)
        boxLinkParentLayer->addItem(tr("Layer") + " " + QString::number(i + 1), i);

    const QString pageLabel = ml->name().isEmpty()
            ? tr("Page")
            : (tr("Page") + " (" + ml->name() + ")");
    QTreeWidgetItem *item = new QTreeWidgetItem(listBox, QStringList(pageLabel));
    item->setIcon(0, IconLoader::duotoneIcon("folder-open"));
    item->setToolTip(0, pageLabel);
    listBox->addTopLevelItem(item);
    item->setExpanded(true);
    listBox->setCurrentItem(item);

    for (int i = 0; i < plots.count(); ++i) {
        Graph *g = (Graph *)plots.at(i);
        if (!g)
            continue;

        LayerItem *layer = new LayerItem(g, item, tr("Layer") + " " + QString::number(i + 1));
        item->addChild(layer);

        if (selectActiveLayer && g == ml->activeGraph()) {
            layer->setExpanded(true);
            layer->setActive(true);
            listBox->setCurrentItem(layer);
        }
    }
}

void PlotDialog::selectCurve(int index)
{
    LayerItem *layerItem = (LayerItem *)listBox->currentItem();
    if (!layerItem)
        return;
    if (layerItem->type() != LayerItem::LayerTreeItem)
        return;
    QTreeWidgetItem *item = layerItem->child(index);
    if (item) {
        ((CurveTreeItem *)item)->setActive(true);
        listBox->setCurrentItem(item);
    }
    updateCurveMoveButtons();
}

void PlotDialog::showStatistics()
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    if (!app)
        return;

    QTreeWidgetItem *it = listBox->currentItem();
    if (!it || it->type() != CurveTreeItem::PlotCurveTreeItem)
        return;

    CurveTreeItem *ci = static_cast<CurveTreeItem *>(it);
    if (!ci->isCurveAlive())
        return;

    QwtPlotItem *plotItem = const_cast<QwtPlotItem *>(ci->plotItem());
    if (!plotItem)
        return;

    QwtHistogram *h = dynamic_cast<QwtHistogram *>(plotItem);
    if (!h)
        return;

    QString tableName = app->generateUniqueName(tr("Bins"));
    Table *t = app->newTable(h->dataSize(), 4, tableName,
                             tr("Histogram and Probabilities for") + " " + h->title().text());
    if (t) {
        double h_sum = 0.0;
        for (int i = 0; i < h->dataSize(); i++)
            h_sum += h->y(i);

        double sum = 0.0;
        for (int i = 0; i < h->dataSize(); i++) {
            sum += h->y(i);
            t->column(0)->setValueAt(i, h->x(i));
            t->column(1)->setValueAt(i, h->y(i));
            t->column(2)->setValueAt(i, sum);
            t->column(3)->setValueAt(i, sum / h_sum * 100);
        }
        t->setHeader(QStringList() << tr("Bins") << tr("Quantity") << tr("Sum") << tr("Percent"));
        t->showMaximized();
    }

    QDateTime dt = QDateTime::currentDateTime();
    QString info = QLocale().toString(dt) + "\t" + tr("Histogram and Probabilities for") + " "
            + h->title().text() + "\n";
    info += tr("Mean") + " = " + QString::number(h->mean()) + "\t";
    info += tr("Standard Deviation") + " = " + QString::number(h->standardDeviation()) + "\n";
    info += tr("Minimum") + " = " + QString::number(h->minimum()) + "\t";
    info += tr("Maximum") + " = " + QString::number(h->maximum()) + "\t";
    info += tr("Bins") + " = " + QString::number(h->dataSize()) + "\n";
    info += "-------------------------------------------------------------\n";
    if (!info.isEmpty()) {
        app->logInfo += info;
        app->showResults(true);
    }

    close();
}

void PlotDialog::contextMenuEvent(QContextMenuEvent *e)
{
    QTreeWidgetItem *item = listBox->currentItem();
    if (!item || item->type() != CurveTreeItem::PlotCurveTreeItem)
        return;
    CurveTreeItem *ci = static_cast<CurveTreeItem *>(item);
    if (!ci->isCurveAlive())
        return;
    QwtPlotItem *it = const_cast<QwtPlotItem *>(ci->plotItem());
    if (!it)
        return;

    QPoint pos = listBox->viewport()->mapFromGlobal(QCursor::pos());
    QRect rect = listBox->visualItemRect(listBox->currentItem());
    if (rect.contains(pos)) {
        QMenu contextMenu(this);
        contextMenu.addAction(tr("&Delete"), this, SLOT(removeSelectedCurve()));

        if (it->rtti() == QwtPlotItem::Rtti_PlotCurve) {
            if (((PlotCurve *)it)->type() == Graph::Function)
                contextMenu.addAction(tr("&Edit..."), this, SLOT(editCurve()));
            else
                contextMenu.addAction(tr("&Plot Associations..."), this, SLOT(editCurve()));
        }
        contextMenu.exec(QCursor::pos());
    }
    e->accept();
}

void PlotDialog::removeSelectedCurve()
{
    QTreeWidgetItem *cur = listBox ? listBox->currentItem() : nullptr;
    if (!cur || cur->type() != CurveTreeItem::PlotCurveTreeItem)
        return;

    CurveTreeItem *item = static_cast<CurveTreeItem *>(cur);
    LayerItem *layerIt = dynamic_cast<LayerItem *>(item->parent());
    if (!item->isCurveAlive()) {
        d_dirtyTabs.clear();
        if (layerIt) {
            QScopedValueRollback<bool> guard(d_loadingUi, true);
            layerIt->rebuildCurvesList();
            listBox->setCurrentItem(layerIt);
        }
        updateCurveMoveButtons();
        return;
    }

    Graph *graph = item->graph();
    const int curveIdx = item->plotItemIndex();
    if (graph && curveIdx >= 0) {
        QScopedValueRollback<int> selfEdit(d_selfEditDepth, d_selfEditDepth + 1);
        MultiLayer *ml = d_mlGuard ? d_mlGuard.data() : graph->multiLayerParent();
        const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
        ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parentWidget());
        if (!app)
            app = qobject_cast<ApplicationWindow *>(window());
        if (app && ml)
            app->invalidateModelessGraphDialogs(ml, this);
        d_dirtyTabs.clear();
        graph->removeCurve(curveIdx);
        graph->updatePlot();

        if (layerIt) {
            QScopedValueRollback<bool> guard(d_loadingUi, true);
            {
                QSignalBlocker blocker(listBox);
                layerIt->rebuildCurvesList();
            }
            const int newCount = layerIt->childCount();
            if (newCount > 0)
                listBox->setCurrentItem(layerIt->child(qBound(0, curveIdx, newCount - 1)));
            else
                listBox->setCurrentItem(layerIt);
        }
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(tr("Remove Curve"), beforeSnap);
        updateCurveMoveButtons();
    }
}

void PlotDialog::changeErrorBarsPlus()
{
    markTabDirty(errorsPage);
}

void PlotDialog::changeErrorBarsMinus()
{
    markTabDirty(errorsPage);
}

void PlotDialog::changeErrorBarsThrough()
{
    markTabDirty(errorsPage);
}

void PlotDialog::changeErrorBarsType()
{
    markTabDirty(errorsPage);
}

void PlotDialog::pickErrorBarsColor(QColor)
{
    markTabDirty(errorsPage);
}

void PlotDialog::showAreaColor(bool show)
{
    boxAreaColor->setEnabled(show);
    boxPattern->setEnabled(show);
}

void PlotDialog::updateTabWindow(QTreeWidgetItem *currentItem, QTreeWidgetItem *previousItem)
{
    if (!currentItem) {
        updateCurveMoveButtons();
        return;
    }

    if (!d_loadingUi && previousItem && !d_dirtyTabs.isEmpty())
        commitParamsForItem(previousItem, false);

    QScopedValueRollback<bool> guard(d_loadingUi, true);
    d_dirtyTabs.clear();

    if (previousItem) {
        if (previousItem->type() == CurveTreeItem::PlotCurveTreeItem)
            ((CurveTreeItem *)previousItem)->setActive(false);
        else if (previousItem->type() == LayerItem::LayerTreeItem)
            ((LayerItem *)previousItem)->setActive(false);
    }

    boxPlotType->blockSignals(true);

    if (currentItem->type() == CurveTreeItem::PlotCurveTreeItem) {
        CurveTreeItem *curveItem = (CurveTreeItem *)currentItem;
        setActiveCurve(curveItem);

        if (!previousItem || previousItem->type() != CurveTreeItem::PlotCurveTreeItem
            || ((CurveTreeItem *)previousItem)->plotItemType() != curveItem->plotItemType()
            || d_shownTabType < 0) {
            clearTabWidget();
            int plot_type = setPlotType(curveItem);
            if (plot_type >= 0) {
                insertTabs(plot_type);
                d_shownTabType = plot_type;
            } else {
                d_shownTabType = -1;
            }
            if (!curvePlotTypeBox->isVisible())
                curvePlotTypeBox->show();
        } else {
            const int plot_type = setPlotType(curveItem);
            // A Line curve that gained / lost symbols needs different pages.
            if (plot_type >= 0 && plot_type != d_shownTabType)
                rebuildCurveTabs(plot_type, true);
            if (!curvePlotTypeBox->isVisible())
                curvePlotTypeBox->show();
        }
    } else if (currentItem->type() == LayerItem::LayerTreeItem) {
        if (!previousItem || previousItem->type() != LayerItem::LayerTreeItem) {
            clearTabWidget();
            privateTabWidget->addTab(layerPage, tr("Layer"));
            privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(layerPage));
        }
        d_shownTabType = -1;
        setActiveLayer((LayerItem *)currentItem);
    } else {
        clearTabWidget();
        d_shownTabType = -1;
        // Page node: Page (summary + print options) and Fonts.
        privateTabWidget->addTab(pageTab, tr("Page"));
        privateTabWidget->addTab(fontsPage, tr("Fonts"));
        privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(pageTab));
        syncPageTabFromMultiLayer();

        curvePlotTypeBox->hide();
        btnWorksheet->hide();
        btnEditCurve->hide();
    }
    boxPlotType->blockSignals(false);
    d_dirtyTabs.clear();
    updateCurveMoveButtons();
}

void PlotDialog::insertTabs(int plot_type)
{
    if (plot_type == Graph::Pie) {
        privateTabWidget->addTab(piePage, tr("Pie"));
        privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(piePage));
        btnEditCurve->hide();
        return;
    }

    privateTabWidget->addTab(axesPage, tr("Axes"));
    if (plot_type == Graph::Line) {
        boxConnect->setEnabled(true);
        privateTabWidget->addTab(linePage, tr("Line"));
        privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(linePage));
    } else if (plot_type == Graph::Scatter) {
        boxConnect->setEnabled(true);
        privateTabWidget->addTab(symbolPage, tr("Symbol"));
        privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(symbolPage));
    } else if (plot_type == Graph::LineSymbols) {
        boxConnect->setEnabled(true);
        privateTabWidget->addTab(linePage, tr("Line"));
        privateTabWidget->addTab(symbolPage, tr("Symbol"));
        privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(symbolPage));
    } else if (plot_type == Graph::VerticalBars || plot_type == Graph::HorizontalBars
               || plot_type == Graph::Histogram) {
        boxConnect->setEnabled(false);
        privateTabWidget->addTab(linePage, tr("Pattern"));
        privateTabWidget->addTab(spacingPage, tr("Spacing"));

        if (plot_type == Graph::Histogram) {
            privateTabWidget->addTab(histogramPage, tr("Histogram Data"));
            privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(histogramPage));
        } else
            privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(linePage));
    } else if (plot_type == Graph::VectXYXY || plot_type == Graph::VectXYAM) {
        boxConnect->setEnabled(true);
        privateTabWidget->addTab(vectPage, tr("Vector"));
        customVectorsPage(plot_type == Graph::VectXYAM);
        privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(vectPage));
    } else if (plot_type == Graph::ErrorBars) {
        privateTabWidget->addTab(errorsPage, tr("Error Bars"));
        privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(errorsPage));
    } else if (plot_type == Graph::Box) {
        boxConnect->setEnabled(false);
        privateTabWidget->addTab(linePage, tr("Pattern"));
        privateTabWidget->addTab(boxPage, tr("Box/Whiskers"));
        privateTabWidget->addTab(percentilePage, tr("Percentile"));
        privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(linePage));
    } else if (plot_type == Graph::ColorMap || plot_type == Graph::GrayMap
               || plot_type == Graph::ContourMap) {
        privateTabWidget->addTab(spectrogramPage, tr("Colors") + " / " + tr("Contour"));
        privateTabWidget->setCurrentIndex(privateTabWidget->indexOf(spectrogramPage));
    }
}

void PlotDialog::clearTabWidget()
{
    privateTabWidget->removeTab(privateTabWidget->indexOf(axesPage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(linePage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(symbolPage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(errorsPage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(histogramPage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(spacingPage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(vectPage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(boxPage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(percentilePage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(spectrogramPage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(piePage));

    privateTabWidget->removeTab(privateTabWidget->indexOf(pageTab));
    privateTabWidget->removeTab(privateTabWidget->indexOf(layerPage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(fontsPage));
    privateTabWidget->removeTab(privateTabWidget->indexOf(printPage));
}

void PlotDialog::quit()
{
    if (acceptParams())
        close();
}

void PlotDialog::showWorksheet()
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    if (!app)
        return;

    QTreeWidgetItem *cur = listBox ? listBox->currentItem() : nullptr;
    if (!cur || cur->type() != CurveTreeItem::PlotCurveTreeItem)
        return;

    CurveTreeItem *item = static_cast<CurveTreeItem *>(cur);
    if (!item->isCurveAlive())
        return;

    app->showCurveWorksheet(item->graph(), item->plotItemIndex());
    close();
}

int PlotDialog::setPlotType(CurveTreeItem *item)
{
    if (!item || !item->isCurveAlive())
        return -1;

    const int curveType = item->plotItemType();
    if (curveType < 0)
        return curveType;

    auto typeLabel = [this](int t) -> QString {
        switch (t) {
        case Graph::Line:
            return tr("Line");
        case Graph::Scatter:
            return tr("Scatter");
        case Graph::LineSymbols:
            return tr("Line + Symbol");
        case Graph::Spline:
            return tr("Spline");
        case Graph::HorizontalSteps:
            return tr("Horizontal Steps");
        case Graph::VerticalSteps:
            return tr("Vertical Steps");
        case Graph::VerticalDropLines:
            return tr("Vertical Drop Lines");
        case Graph::VerticalBars:
            return tr("Vertical Bars");
        case Graph::HorizontalBars:
            return tr("Horizontal Bars");
        case Graph::Area:
            return tr("Area");
        default:
            break;
        }
        return QString();
    };

    boxPlotType->clear();
    boxPlotType->setEnabled(true);

    // Types that Graph::canConvertTo() never converts: a single, disabled entry.
    QString fixedLabel;
    if (curveType == Graph::ErrorBars)
        fixedLabel = tr("Error Bars");
    else if (curveType == Graph::Pie)
        fixedLabel = tr("Pie");
    else if (curveType == Graph::Histogram)
        fixedLabel = tr("Histogram");
    else if (curveType == Graph::VectXYXY)
        fixedLabel = tr("Vector XYXY");
    else if (curveType == Graph::VectXYAM)
        fixedLabel = tr("Vector XYAM");
    else if (curveType == Graph::Box)
        fixedLabel = tr("Box");
    else if (curveType == Graph::ColorMap || curveType == Graph::GrayMap
             || curveType == Graph::ContourMap)
        fixedLabel = tr("Contour") + " / " + tr("Image");
    if (!fixedLabel.isEmpty()) {
        boxPlotType->addItem(fixedLabel, curveType);
        boxPlotType->setEnabled(false);
        return curveType;
    }

    const QwtPlotItem *pi = item->plotItem();
    if (!pi || pi->rtti() != QwtPlotItem::Rtti_PlotCurve)
        return -1;
    QwtPlotCurve *c = const_cast<QwtPlotCurve *>(static_cast<const QwtPlotCurve *>(pi));

    int tabType = -1;
    const int shown = shownPlotType(item, &tabType);

    // Offer exactly the types canConvertTo() accepts for this curve (the ten types of a data
    // curve: Line, Scatter, Line + Symbol, Spline, Horizontal / Vertical Steps, Vertical Drop
    // Lines, Vertical / Horizontal Bars, Area).
    static const Graph::CurveType kOffered[] = {
        Graph::Line,      Graph::Scatter,           Graph::LineSymbols,  Graph::Spline,
        Graph::HorizontalSteps, Graph::VerticalSteps, Graph::VerticalDropLines,
        Graph::VerticalBars, Graph::HorizontalBars, Graph::Area
    };
    for (Graph::CurveType t : kOffered) {
        if (Graph::canConvertTo(c, t))
            boxPlotType->addItem(typeLabel(t), int(t));
    }
    if (boxPlotType->count() == 0) {
        // Not convertible (e.g. a function curve): show what it currently looks like.
        boxPlotType->addItem(typeLabel(shown), shown);
        boxPlotType->setEnabled(false);
    } else {
        const int idx = boxPlotType->findData(shown);
        if (idx >= 0)
            boxPlotType->setCurrentIndex(idx);
    }
    return tabType;
}

int PlotDialog::shownPlotType(const CurveTreeItem *item, int *tabType) const
{
    if (tabType)
        *tabType = -1;
    if (!item || !item->isCurveAlive())
        return -1;
    const int curveType = item->plotItemType();
    const QwtPlotItem *pi = item->plotItem();
    if (!pi || pi->rtti() != QwtPlotItem::Rtti_PlotCurve) {
        if (tabType)
            *tabType = curveType;
        return curveType;
    }
    const QwtPlotCurve *c = static_cast<const QwtPlotCurve *>(pi);

    // Line / Scatter / Line + Symbol follow the real appearance (symbol and line style).
    const QwtSymbol s = c->symbol();
    int appearance;
    if (s.style() == QwtSymbol::NoSymbol)
        appearance = Graph::Line;
    else if (c->style() == QwtPlotCurve::NoCurve)
        appearance = Graph::Scatter;
    else
        appearance = Graph::LineSymbols;

    switch (curveType) {
    case Graph::VerticalBars:
    case Graph::HorizontalBars:
        // Bars have their own page set (Pattern / Spacing).
        if (tabType)
            *tabType = curveType;
        return curveType;
    case Graph::Spline:
    case Graph::HorizontalSteps:
    case Graph::VerticalSteps:
    case Graph::VerticalDropLines:
    case Graph::Area:
        // The combo names the exact type; the pages follow whether symbols are drawn.
        if (tabType)
            *tabType = appearance;
        return curveType;
    default:
        break;
    }
    if (tabType)
        *tabType = appearance;
    return appearance;
}

void PlotDialog::rebuildCurveTabs(int plotType, bool keepCurrentTab)
{
    QWidget *prevPage = keepCurrentTab ? privateTabWidget->currentWidget() : nullptr;
    clearTabWidget();
    insertTabs(plotType);
    d_shownTabType = plotType;
    if (prevPage && privateTabWidget->indexOf(prevPage) >= 0)
        privateTabWidget->setCurrentWidget(prevPage);
}

void PlotDialog::flushPendingEdits()
{
    if (d_dirtyTabs.isEmpty() || !listBox)
        return;
    commitParamsForItem(listBox->currentItem(), false);
}

void PlotDialog::setActiveLayer(LayerItem *item)
{
    if (!item)
        return;
    QScopedValueRollback<bool> guard(d_loadingUi, true);
    item->setActive(true);

    Graph *g = item->graph();
    if (!g)
        return;

    curvePlotTypeBox->hide();
    btnWorksheet->hide();
    btnEditCurve->hide();

    boxBackgroundTransparency->blockSignals(true);
    boxCanvasTransparency->blockSignals(true);
    boxBorderWidth->blockSignals(true);

    Plot *p = g->plotWidget();
    boxMargin->setValue(p->margin());
    boxBorderWidth->setValue(p->lineWidth());
    boxBorderColor->setColor(p->frameColor());

    QColor c = p->paletteBackgroundColor();
    d_bgAlphaLoaded = c.alpha();
    boxBackgroundTransparency->setValue(percentFromAlpha(c.alpha()));
    boxBackgroundColor->setEnabled(c.alpha());
    boxBackgroundColor->setColor(c);

    c = p->canvasBackground();
    d_canvasAlphaLoaded = c.alpha();
    boxCanvasTransparency->setValue(percentFromAlpha(c.alpha()));
    boxCanvasColor->setEnabled(c.alpha());
    boxCanvasColor->setColor(c);

    boxAntialiasing->setChecked(g->antialiasing());

    QRect geom = g->geometry();
    boxLayerX->setValue(geom.x());
    boxLayerY->setValue(geom.y());
    boxLayerW->setValue(qMax(20, geom.width()));
    boxLayerH->setValue(qMax(20, geom.height()));

    if (d_ml) {
        boxKeepAspect->setChecked(d_ml->keepAspectRatio());
        boxAspectRatio->setValue(d_ml->targetAspectRatio());
    }
    boxFramePreset->setCurrentIndex(0);

    int parentIdx = -1;
    if (d_ml && g->linkedParentLayer()) {
        QWidgetList plots = d_ml->graphPtrs();
        parentIdx = plots.indexOf(g->linkedParentLayer());
    }
    int comboIdx = boxLinkParentLayer->findData(parentIdx);
    boxLinkParentLayer->setCurrentIndex(comboIdx >= 0 ? comboIdx : 0);
    boxLinkXMode->setCurrentIndex(qBound(0, g->xLinkMode(), 2));
    boxLinkYMode->setCurrentIndex(qBound(0, g->yLinkMode(), 2));
    editLinkXFormula->setText(g->xLinkExpr().isEmpty() ? QString("x") : g->xLinkExpr());
    editLinkYFormula->setText(g->yLinkExpr().isEmpty() ? QString("y") : g->yLinkExpr());

    boxBackgroundTransparency->blockSignals(false);
    boxCanvasTransparency->blockSignals(false);
    boxBorderWidth->blockSignals(false);
}

void PlotDialog::setActiveCurve(CurveTreeItem *item)
{
    if (!item || !item->isCurveAlive())
        return;

    QScopedValueRollback<bool> guard(d_loadingUi, true);
    const QwtPlotItem *i = item->plotItem();
    if (!i)
        return;

    item->setActive(true);
    btnWorksheet->show();
    btnEditCurve->show();

    int curveType = item->plotItemType();
    if (curveType == Graph::Pie) {
        QwtPieCurve *pie = (QwtPieCurve *)i;
        boxRadius->setValue(pie->ray());
        boxPiePattern->setPattern(pie->pattern());
        boxPieLineWidth->setValue(pie->pen().width());
        boxPieLineColor->setColor(pie->pen().color());
        setPiePenStyle(pie->pen().style());
        boxFirstColor->setColor(pie->firstColor());
        return;
    }

    // axes page
    boxXAxis->setCurrentIndex(i->xAxis() - 2);
    boxYAxis->setCurrentIndex(i->yAxis());

    if (i->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
        btnEditCurve->hide();
        Spectrogram *sp = (Spectrogram *)i;

        imageGroupBox->setChecked(sp->testDisplayMode(QwtPlotSpectrogram::ImageMode));

        boxColorMapPreset->blockSignals(true);
        boxReversePalette->blockSignals(true);
        boxCustomZRange->blockSignals(true);
        boxColorMapPreset->setCurrentIndex(static_cast<int>(sp->colorMapPreset()));
        boxReversePalette->setChecked(sp->reversePalette());

        if (boxLogColorScale) {
            QSignalBlocker logBlocker(boxLogColorScale);
            boxLogColorScale->setChecked(sp->logColorScale());
        }
        if (boxCellBlocks) {
            QSignalBlocker cellBlocker(boxCellBlocks);
            boxCellBlocks->setChecked(sp->cellBlocks());
        }

        const QwtDoubleInterval rawR = sp->rawDataRange();
        const QwtDoubleInterval effR = sp->effectiveValueRange();
        boxCustomZRange->setChecked(sp->hasCustomValueRange());
        boxZMin->setValue(sp->hasCustomValueRange() ? sp->customZMin() : rawR.minValue());
        boxZMax->setValue(sp->hasCustomValueRange() ? sp->customZMax() : rawR.maxValue());
        boxZMin->setEnabled(sp->hasCustomValueRange());
        boxZMax->setEnabled(sp->hasCustomValueRange());

        boxColorMapPreset->blockSignals(false);
        boxReversePalette->blockSignals(false);
        boxCustomZRange->blockSignals(false);

        colorMapEditor->setRange(effR.minValue(), effR.maxValue());
        if (sp->colorMapPreset() == Spectrogram::PresetCustom) {
            colorMapEditor->setColorMap(sp->baseCustomColorMap());
            colorMapEditor->show();
        } else {
            colorMapEditor->setColorMap((const QwtLinearColorMap &)sp->colorMap());
            colorMapEditor->hide();
        }

        levelsGroupBox->setChecked(sp->testDisplayMode(QwtPlotSpectrogram::ContourMode));
        levelsBox->setValue(sp->levels());

        autoContourBox->setChecked(sp->defaultContourPen().style() == Qt::NoPen);
        defaultContourBox->setChecked(sp->defaultContourPen().style() != Qt::NoPen);

        levelsColorBox->setColor(sp->defaultContourPen().color());
        contourWidthBox->setValue(sp->defaultContourPen().width());
        if (sp->defaultContourPen().style() != Qt::NoPen)
            boxContourStyle->setCurrentIndex(sp->defaultContourPen().style() - 1);
        else
            boxContourStyle->setCurrentIndex(0);

        axisScaleBox->setChecked(sp->hasColorScale());
        colorScaleBox->setCurrentIndex((int)sp->colorScaleAxis());
        colorScaleWidthBox->setValue(sp->colorBarWidth());
        editColorBarTitle->setText(sp->colorBarTitle());
        return;
    }

    PlotCurve *c = (PlotCurve *)i;
    if (c->type() == Graph::Function)
        btnEditCurve->setText(tr("&Edit..."));
    else
        btnEditCurve->setText(tr("&Plot Associations..."));

    // line page
    int style = c->style();
    if (curveType == Graph::Spline)
        style = 5;
    else if (curveType == Graph::VerticalSteps)
        style = 6;
    boxConnect->setCurrentIndex(style);

    penWidget->blockSignals(true);
    penWidget->setPen(c->pen());
    penWidget->blockSignals(false);
    boxLineOpacity->blockSignals(true);
    boxLineOpacity->setValue(qRound(c->pen().color().alpha() * 100.0 / 255.0));
    boxLineOpacity->blockSignals(false);
    fillGroupBox->blockSignals(true);
    fillGroupBox->setChecked(c->brush().style() != Qt::NoBrush);
    fillGroupBox->blockSignals(false);
    boxAreaColor->setColor(c->brush().color());
    boxPattern->setPattern(c->brush().style());
    boxAreaOpacity->blockSignals(true);
    boxAreaOpacity->setValue(qRound(c->brush().color().alpha() * 100.0 / 255.0));
    boxAreaOpacity->blockSignals(false);

    // symbol page
    const QwtSymbol s = c->symbol();
    boxSymbolSize->setValue(qBound(1, s.size().width(), 100));
    boxSymbolStyle->setStyle(s.style());
    boxSymbolColor->setColor(s.pen().color());
    const double symPenW =
            s.pen().widthF() > 0.0 ? s.pen().widthF() : double(qMax(1, s.pen().width()));
    boxPenWidth->setValue(qBound(PlotStyleLimits::kSymbolEdgeWidthMin, symPenW,
                                 PlotStyleLimits::kSymbolEdgeWidthMax));
    boxFillSymbol->setChecked(s.brush() != Qt::NoBrush);
    boxFillColor->setEnabled(s.brush() != Qt::NoBrush);
    boxFillColor->setColor(s.brush().color());
    if (boxHollowSymbol) {
        QSignalBlocker hollowBlocker(boxHollowSymbol);
        boxHollowSymbol->setChecked(Graph::isSymbolWhiteHollow(s));
    }
    boxSymbolOpacity->blockSignals(true);
    boxSymbolOpacity->setValue(qRound(s.pen().color().alpha() * 100.0 / 255.0));
    boxSymbolOpacity->blockSignals(false);

    if (curveType == Graph::VerticalBars || curveType == Graph::HorizontalBars
        || curveType == Graph::Histogram) { // spacing page
        QwtBarCurve *b = (QwtBarCurve *)i;
        if (b) {
            gapBox->setValue(b->gap());
            offsetBox->setValue(b->offset());
        }
    }

    if (curveType == Graph::Histogram) { // Histogram page
        QwtHistogram *h = (QwtHistogram *)i;
        if (h) {
            automaticBox->setChecked(h->autoBinning());
            binSizeBox->setText(QString::number(h->binSize()));
            histogramBeginBox->setText(QString::number(h->begin()));
            histogramEndBox->setText(QString::number(h->end()));
            setAutomaticBinning();
        }
    }

    if (curveType == Graph::VectXYXY || curveType == Graph::VectXYAM) { // Vector page
        VectorCurve *v = (VectorCurve *)i;
        if (v) {
            vectColorBox->setColor(v->color());
            vectWidthBox->setValue(v->width());
            headLengthBox->setValue(v->headLength());
            headAngleBox->setValue(v->headAngle());
            filledHeadBox->setChecked(v->filledArrowHead());
            vectPosBox->setCurrentIndex(v->position());
            updateEndPointColumns(item->text(0));
        }
    }

    if (curveType == Graph::ErrorBars) {
        QwtErrorPlotCurve *err = (QwtErrorPlotCurve *)i;
        if (err) {
            xBox->setChecked(err->xErrors());
            const double errW =
                    err->pen().widthF() > 0.0 ? err->pen().widthF() : double(qMax(1, err->width()));
            widthBox->setValue(qBound(PlotStyleLimits::kErrorBarWidthMin, errW,
                                      PlotStyleLimits::kErrorBarWidthMax));
            capBox->setEditText(QString::number(err->capLength()));
            throughBox->setChecked(err->throughSymbol());
            plusBox->setChecked(err->plusSide());
            minusBox->setChecked(err->minusSide());
            colorBox->setColor(err->color());
        }
    }

    if (curveType == Graph::Box) {
        BoxCurve *b = (BoxCurve *)i;
        if (b) {
            boxMaxStyle->setStyle(b->maxStyle());
            boxMinStyle->setStyle(b->minStyle());
            boxMeanStyle->setStyle(b->meanStyle());
            box99Style->setStyle(b->p99Style());
            box1Style->setStyle(b->p1Style());

            boxPercSize->setValue(s.size().width() / 2);
            boxFillSymbols->setChecked(s.brush() != Qt::NoBrush);
            boxPercFillColor->setEnabled(s.brush() != Qt::NoBrush);
            boxPercFillColor->setColor(s.brush().color());
            boxEdgeColor->setColor(s.pen().color());
            const double boxPenW =
                    s.pen().widthF() > 0.0 ? s.pen().widthF() : double(qMax(1, s.pen().width()));
            boxEdgeWidth->setValue(qBound(PlotStyleLimits::kSymbolEdgeWidthMin, boxPenW,
                                          PlotStyleLimits::kSymbolEdgeWidthMax));

            boxRange->setCurrentIndex(b->boxRangeType() - 1);
            boxType->setCurrentIndex(b->boxStyle());
            boxWidth->setValue(b->boxWidth());
            setBoxRangeType(boxRange->currentIndex());
            setBoxType(boxType->currentIndex());
            if (b->boxRangeType() == BoxCurve::SD || b->boxRangeType() == BoxCurve::SE)
                boxCnt->setValue(b->boxRange());
            else
                boxCoef->setValue((int)b->boxRange());

            boxWhiskersRange->setCurrentIndex(b->whiskersRangeType());
            setWhiskersRange(boxWhiskersRange->currentIndex());
            if (b->whiskersRangeType() == BoxCurve::SD || b->whiskersRangeType() == BoxCurve::SE)
                whiskerCnt->setValue(b->whiskersRange());
            else if (b->whiskersRangeType() != BoxCurve::Tukey15IQR)
                boxWhiskersCoef->setValue((int)b->whiskersRange());

            boxViolinMode->setChecked(b->violinMode());
            boxShowDataPoints->setChecked(b->showDataPoints());
        }
    }
}

void PlotDialog::updateEndPointColumns(const QString &text)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList cols = text.split(",", Qt::SkipEmptyParts);
#else
    QStringList cols = text.split(",", QString::SkipEmptyParts);
#endif
    if (cols.size() < 4)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList aux = cols[0].split(":", Qt::SkipEmptyParts);
#else
    QStringList aux = cols[0].split(":", QString::SkipEmptyParts);
#endif
    if (aux.isEmpty())
        return;

    const QString table = aux[0].trimmed();
    ApplicationWindow *app = nullptr;
    for (QObject *p = this->parent(); p && !app; p = p->parent())
        app = qobject_cast<ApplicationWindow *>(p);
    const QStringList allTables = app ? app->tableWindows() : QStringList();

    const QString prefix = table + QLatin1Char('_');
    QStringList list;
    for (int i = 0; i < (int)columnNames.count(); i++) {
        const QString &s = columnNames[i];
        if (!s.startsWith(prefix))
            continue;
        bool matchedLongerTable = false;
        for (const QString &otherTable : allTables) {
            if (otherTable.size() > table.size() && s.startsWith(otherTable + QLatin1Char('_'))) {
                matchedLongerTable = true;
                break;
            }
        }
        if (!matchedLongerTable)
            list << s;
    }

    QString xColPart = cols[2].trimmed();
    xColPart.remove(QStringLiteral("(X)")).remove(QStringLiteral("(A)"));
    const QString xTarget = prefix + xColPart.trimmed();

    QString yColPart = cols[3].trimmed();
    yColPart.remove(QStringLiteral("(Y)")).remove(QStringLiteral("(M)"));
    const QString yTarget = prefix + yColPart.trimmed();

    {
        QSignalBlocker bx(xEndBox);
        xEndBox->clear();
        xEndBox->addItems(list);
        const int xIdx = xEndBox->findText(xTarget);
        if (xIdx >= 0)
            xEndBox->setCurrentIndex(xIdx);
    }

    {
        QSignalBlocker by(yEndBox);
        yEndBox->clear();
        yEndBox->addItems(list);
        const int yIdx = yEndBox->findText(yTarget);
        if (yIdx >= 0)
            yEndBox->setCurrentIndex(yIdx);
    }
}

void PlotDialog::markTabDirty(QWidget *tabPage)
{
    if (d_loadingUi || !tabPage)
        return;
    d_dirtyTabs.insert(tabPage);
}

void PlotDialog::connectTabDirtySignals(QWidget *tabPage)
{
    if (!tabPage)
        return;

    for (QSpinBox *sb : tabPage->findChildren<QSpinBox *>()) {
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, tabPage](int) { markTabDirty(tabPage); });
    }
    for (QDoubleSpinBox *dsb : tabPage->findChildren<QDoubleSpinBox *>()) {
        connect(dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, tabPage](double) { markTabDirty(tabPage); });
    }
    for (QComboBox *cb : tabPage->findChildren<QComboBox *>()) {
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, tabPage](int) { markTabDirty(tabPage); });
        connect(cb, &QComboBox::editTextChanged, this,
                [this, tabPage](const QString &) { markTabDirty(tabPage); });
    }
    for (QAbstractButton *ab : tabPage->findChildren<QAbstractButton *>()) {
        connect(ab, &QAbstractButton::toggled, this,
                [this, tabPage](bool) { markTabDirty(tabPage); });
    }
    for (QGroupBox *gb : tabPage->findChildren<QGroupBox *>()) {
        connect(gb, &QGroupBox::toggled, this,
                [this, tabPage](bool) { markTabDirty(tabPage); });
    }
    for (QLineEdit *le : tabPage->findChildren<QLineEdit *>()) {
        connect(le, &QLineEdit::textChanged, this,
                [this, tabPage](const QString &) { markTabDirty(tabPage); });
    }
    for (ColorButton *colBtn : tabPage->findChildren<ColorButton *>()) {
        connect(colBtn, &ColorButton::changed, this,
                [this, tabPage](QColor) { markTabDirty(tabPage); });
    }
    for (PenWidget *pw : tabPage->findChildren<PenWidget *>()) {
        connect(pw, &PenWidget::penChanged, this,
                [this, tabPage](const QPen &) { markTabDirty(tabPage); });
    }
    for (QTableWidget *tw : tabPage->findChildren<QTableWidget *>()) {
        connect(tw, &QTableWidget::cellChanged, this,
                [this, tabPage](int, int) { markTabDirty(tabPage); });
    }
    if (tabPage == spectrogramPage && colorMapEditor) {
        for (QPushButton *pb : colorMapEditor->findChildren<QPushButton *>()) {
            connect(pb, &QPushButton::clicked, this,
                    [this, tabPage]() { markTabDirty(tabPage); });
        }
    }
}

void PlotDialog::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (d_loadingUi || !item || column != 0)
        return;
    if (item->type() != CurveTreeItem::PlotCurveTreeItem)
        return;

    CurveTreeItem *ci = static_cast<CurveTreeItem *>(item);
    if (!ci->isCurveAlive())
        return;
    Graph *g = ci->graph();
    int idx = ci->plotItemIndex();
    if (!g || idx < 0)
        return;

    const bool wantVisible = (ci->checkState(0) == Qt::Checked);
    if (g->isCurveVisible(idx) == wantVisible)
        return;

    QScopedValueRollback<int> selfEdit(d_selfEditDepth, d_selfEditDepth + 1);
    MultiLayer *ml = d_mlGuard ? d_mlGuard.data() : g->multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    g->showCurve(idx, wantVisible);
    g->replot();
    g->notifyChanges();
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Toggle Curve Visibility"), beforeSnap);
}

void PlotDialog::updateCurveMoveButtons()
{
    if (!btnMoveCurveUp || !btnMoveCurveDown)
        return;

    QTreeWidgetItem *cur = listBox ? listBox->currentItem() : nullptr;
    if (!cur || cur->type() != CurveTreeItem::PlotCurveTreeItem) {
        btnMoveCurveUp->setEnabled(false);
        btnMoveCurveDown->setEnabled(false);
        return;
    }

    CurveTreeItem *ci = static_cast<CurveTreeItem *>(cur);
    if (!ci->isCurveAlive()) {
        btnMoveCurveUp->setEnabled(false);
        btnMoveCurveDown->setEnabled(false);
        return;
    }
    Graph *g = ci->graph();
    int idx = ci->plotItemIndex();
    int count = g ? g->curves() : 0;
    if (!g || idx < 0 || idx >= count) {
        btnMoveCurveUp->setEnabled(false);
        btnMoveCurveDown->setEnabled(false);
        return;
    }
    bool canUp = (idx > 0);
    bool canDown = (idx < count - 1);
    if (g->curveType(idx) != Graph::ErrorBars) {
        int nextIdx = idx + 1;
        while (nextIdx < count && g->curveType(nextIdx) == Graph::ErrorBars)
            ++nextIdx;
        canDown = (nextIdx < count);
    }
    btnMoveCurveUp->setEnabled(canUp);
    btnMoveCurveDown->setEnabled(canDown);
}

void PlotDialog::moveSelectedCurveUp()
{
    moveSelectedCurveBy(-1);
}

void PlotDialog::moveSelectedCurveDown()
{
    moveSelectedCurveBy(1);
}

void PlotDialog::moveSelectedCurveBy(int delta)
{
    if (delta == 0 || !listBox)
        return;

    QTreeWidgetItem *cur = listBox->currentItem();
    if (!cur || cur->type() != CurveTreeItem::PlotCurveTreeItem)
        return;

    CurveTreeItem *ci = static_cast<CurveTreeItem *>(cur);
    if (!ci->isCurveAlive())
        return;

    if (!d_dirtyTabs.isEmpty())
        commitParamsForItem(cur, false);

    if (!ci->isCurveAlive())
        return;

    LayerItem *layerIt = dynamic_cast<LayerItem *>(ci->parent());
    Graph *g = ci->graph();
    if (!g || !layerIt)
        return;

    const int nCurves = g->curves();
    const int fromIdx = ci->plotItemIndex();
    if (fromIdx < 0 || fromIdx >= nCurves)
        return;

    int toIdx = fromIdx + delta;
    if (g->curveType(fromIdx) != Graph::ErrorBars) {
        if (delta < 0) {
            toIdx = fromIdx - 1;
            while (toIdx > 0 && g->curveType(toIdx) == Graph::ErrorBars)
                --toIdx;
        } else {
            toIdx = fromIdx + 1;
            while (toIdx < nCurves && g->curveType(toIdx) == Graph::ErrorBars)
                ++toIdx;
            while (toIdx + 1 < nCurves && g->curveType(toIdx + 1) == Graph::ErrorBars)
                ++toIdx;
        }
    }
    if (toIdx < 0 || toIdx >= nCurves || toIdx == fromIdx)
        return;

    QScopedValueRollback<int> selfEdit(d_selfEditDepth, d_selfEditDepth + 1);
    const QwtPlotItem *movedItem = ci->plotItem();
    MultiLayer *ml = d_mlGuard ? d_mlGuard.data() : g->multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    if (!g->moveCurve(fromIdx, toIdx))
        return;

    {
        QScopedValueRollback<bool> guard(d_loadingUi, true);
        QSignalBlocker blocker(listBox);
        layerIt->rebuildCurvesList();
        for (int k = 0; k < layerIt->childCount(); ++k) {
            QTreeWidgetItem *ch = layerIt->child(k);
            if (ch && ch->type() == CurveTreeItem::PlotCurveTreeItem) {
                CurveTreeItem *cand = static_cast<CurveTreeItem *>(ch);
                if (cand->plotItem() == movedItem) {
                    cand->setActive(true);
                    listBox->setCurrentItem(cand);
                    break;
                }
            }
        }
    }

    g->replot();
    g->notifyChanges();
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Reorder Curve"), beforeSnap);
    updateCurveMoveButtons();
}

bool PlotDialog::acceptParams()
{
    QTreeWidgetItem *cur = listBox ? listBox->currentItem() : nullptr;
    if (cur && cur->type() == CurveTreeItem::PlotCurveTreeItem
        && !static_cast<CurveTreeItem *>(cur)->isCurveAlive()) {
        d_dirtyTabs.clear();
        return true;
    }
    return commitParamsForItem(cur, true);
}

bool PlotDialog::commitParamsForItem(QTreeWidgetItem *targetItem, bool explicitAccept)
{
    if (!d_ml || !d_mlGuard)
        return false;

    CurveTreeItem *curveItem = (targetItem && targetItem->type() == CurveTreeItem::PlotCurveTreeItem)
            ? static_cast<CurveTreeItem *>(targetItem)
            : nullptr;
    if (curveItem && !curveItem->isCurveAlive()) {
        d_dirtyTabs.clear();
        return explicitAccept;
    }

    QSet<QWidget *> tabsToCommit = d_dirtyTabs;
    if (tabsToCommit.isEmpty())
        return true;

    // Our own writes raise MultiLayer::modifiedPlot; they must not trigger the external reload.
    QScopedValueRollback<int> selfEdit(d_selfEditDepth, d_selfEditDepth + 1);

    QwtPlotItem *plotItem = curveItem ? const_cast<QwtPlotItem *>(curveItem->plotItem()) : nullptr;
    Graph *graph = curveItem ? curveItem->graph() : nullptr;

    if (curveItem && plotItem && graph && tabsToCommit.contains(histogramPage)
        && curveItem->plotItemType() == Graph::Histogram && !automaticBox->isChecked()) {
        if (explicitAccept && !validInput())
            return false;
    }

    const QString beforeSnap = d_mlGuard->captureUndoSnapshot();
    bool anyHandled = false;

    // (The Page tab no longer has Apply-committed controls: Page Setup applies on its own.)

    if (tabsToCommit.contains(fontsPage)) {
        titleFont = clampedFontSize(titleFont);
        axesFont = clampedFontSize(axesFont);
        numbersFont = clampedFontSize(numbersFont);
        legendFont = clampedFontSize(legendFont);
        d_mlGuard->setFonts(titleFont, axesFont, numbersFont, legendFont);
        anyHandled = true;
    }

    if (tabsToCommit.contains(printPage)) {
        d_mlGuard->setScaleLayersOnPrint(boxScaleLayers->isChecked());
        d_mlGuard->printCropmarks(boxPrintCrops->isChecked());
        anyHandled = true;
    }

    if (tabsToCommit.contains(layerPage)) {
        d_mlGuard->setTargetAspectRatio(boxAspectRatio->value());
        d_mlGuard->setKeepAspectRatio(boxKeepAspect->isChecked());

        int presetVal = boxFramePreset->currentData().toInt();
        QWidgetList allPlots = d_mlGuard->graphPtrs();

        LayerItem *selLayerIt = (targetItem && targetItem->type() == LayerItem::LayerTreeItem)
                ? static_cast<LayerItem *>(targetItem)
                : nullptr;
        Graph *selG = selLayerIt ? selLayerIt->graph() : nullptr;

        auto applyLayerVisuals = [&](Graph *g) {
            if (!g)
                return;
            g->setFrame(boxBorderWidth->value(), boxBorderColor->color());
            g->setMargin(boxMargin->value());

            QColor c = boxBackgroundColor->color();
            c.setAlpha(alphaForCommit(boxBackgroundTransparency->value(), d_bgAlphaLoaded));
            g->setBackgroundColor(c);

            c = boxCanvasColor->color();
            c.setAlpha(alphaForCommit(boxCanvasTransparency->value(), d_canvasAlphaLoaded));
            g->setCanvasBackground(c);

            g->setAntialiasing(boxAntialiasing->isChecked());
            if (presetVal >= 0)
                g->applyFramePreset(static_cast<Graph::FramePreset>(presetVal));
            g->replot();
        };

        if (selG) {
            QRect newRect(boxLayerX->value(), boxLayerY->value(), boxLayerW->value(),
                          boxLayerH->value());
            if (selG->geometry() != newRect)
                selG->setGeometry(newRect);

            if (!boxAll->isChecked())
                applyLayerVisuals(selG);

            int parentIdx = boxLinkParentLayer->currentData().toInt();
            Graph *parentG = (parentIdx >= 0 && parentIdx < allPlots.size())
                    ? qobject_cast<Graph *>(allPlots.at(parentIdx))
                    : nullptr;
            if (parentG == selG)
                parentG = nullptr;

            const QString xExpr = editLinkXFormula->text().trimmed().isEmpty()
                    ? QString("x")
                    : editLinkXFormula->text().trimmed();
            const QString yExpr = editLinkYFormula->text().trimmed().isEmpty()
                    ? QString("y")
                    : editLinkYFormula->text().trimmed();
            selG->setLinkedParentLayer(parentG, boxLinkXMode->currentIndex(), xExpr,
                                       boxLinkYMode->currentIndex(), yExpr);
        }

        if (boxAll->isChecked()) {
            for (int i = 0; i < allPlots.count(); i++)
                applyLayerVisuals(qobject_cast<Graph *>(allPlots.at(i)));
        }
        d_mlGuard->notifyChanges();
        anyHandled = true;
    }

    if (curveItem && plotItem && graph) {
        bool needReplot = false;
        bool lineOrSymbolCommitted = false;

        if (tabsToCommit.contains(axesPage) && curveItem->plotItemType() != Graph::Pie) {
            const int oldXAxis = plotItem->xAxis();
            const int oldYAxis = plotItem->yAxis();
            const int newXAxis = boxXAxis->currentIndex() + 2;
            const int newYAxis = boxYAxis->currentIndex();
            if (oldXAxis != newXAxis || oldYAxis != newYAxis) {
                plotItem->setAxis(newXAxis, newYAxis);
                if (oldXAxis != newXAxis) {
                    graph->enableAxis(newXAxis, true);
                    graph->setAutoScaleAxis(newXAxis);
                }
                if (oldYAxis != newYAxis) {
                    graph->enableAxis(newYAxis, true);
                    graph->setAutoScaleAxis(newYAxis);
                }
                needReplot = true;
                anyHandled = true;
            }
        }

        if (tabsToCommit.contains(spectrogramPage)
            && plotItem->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = static_cast<Spectrogram *>(plotItem);
            // Data flags first: each setter rebuilds the raster data, and the custom Z range below
            // is given in data units whatever the colour scale is.
            if (boxLogColorScale)
                sp->setLogColorScale(boxLogColorScale->isChecked());
            if (boxCellBlocks)
                sp->setCellBlocks(boxCellBlocks->isChecked());
            sp->setCustomValueRange(boxCustomZRange->isChecked(), boxZMin->value(),
                                    boxZMax->value());
            sp->setLevelsNumber(levelsBox->value());
            if (autoContourBox->isChecked())
                sp->setDefaultContourPen(Qt::NoPen);
            else
                sp->setDefaultContourPen(QPen(levelsColorBox->color(), contourWidthBox->value(),
                                              Graph::getPenStyle(boxContourStyle->currentIndex())));

            sp->setDisplayMode(QwtPlotSpectrogram::ContourMode, levelsGroupBox->isChecked());
            sp->setDisplayMode(QwtPlotSpectrogram::ImageMode, imageGroupBox->isChecked());

            const auto preset =
                    static_cast<Spectrogram::ColorMapPreset>(boxColorMapPreset->currentIndex());
            const bool rev = boxReversePalette->isChecked();
            if (preset == Spectrogram::PresetCustom) {
                sp->setReversePalette(rev);
                sp->setCustomColorMap(colorMapEditor->colorMap());
            } else {
                sp->setColorMapPreset(preset, rev);
                QScopedValueRollback<bool> guard(d_loadingUi, true);
                colorMapEditor->setColorMap((const QwtLinearColorMap &)sp->colorMap());
            }

            sp->setColorBarTitle(editColorBarTitle->text());
            sp->showColorScale((QwtPlot::Axis)colorScaleBox->currentIndex(),
                               axisScaleBox->isChecked());
            sp->setColorBarWidth(colorScaleWidthBox->value());
            sp->enforceColorBarArialFont();

            {
                QScopedValueRollback<bool> guard(d_loadingUi, true);
                boxXAxis->setCurrentIndex(sp->xAxis() - 2);
                boxYAxis->setCurrentIndex(sp->yAxis());
            }
            needReplot = true;
            anyHandled = true;
        }

        if (tabsToCommit.contains(linePage) && plotItem->rtti() == QwtPlotItem::Rtti_PlotCurve) {
            int index = curveItem->plotItemIndex();
            if (boxConnect->isEnabled())
                graph->setCurveStyle(index, boxConnect->currentIndex());

            QColor areaCol = boxAreaColor->color();
            areaCol.setAlpha(qRound(boxAreaOpacity->value() * 255.0 / 100.0));
            QBrush br = fillGroupBox->isChecked()
                    ? QBrush(areaCol, boxPattern->getSelectedPattern())
                    : QBrush();
            graph->setCurveBrush(index, br);

            QwtPlotCurve *curve = static_cast<QwtPlotCurve *>(plotItem);
            QPen pen = penWidget->pen();
            QColor penCol = pen.color();
            penCol.setAlpha(qRound(boxLineOpacity->value() * 255.0 / 100.0));
            pen.setColor(penCol);
            curve->setPen(pen);

            lineOrSymbolCommitted = true;
            needReplot = true;
            anyHandled = true;
        }

        if (tabsToCommit.contains(symbolPage) && plotItem->rtti() == QwtPlotItem::Rtti_PlotCurve) {
            QwtPlotCurve *curve = static_cast<QwtPlotCurve *>(plotItem);
            const int curveIdx = curveItem->plotItemIndex();
            const QwtSymbol::Style symStyle = boxSymbolStyle->selectedSymbol();
            if (symStyle == QwtSymbol::NoSymbol) {
                graph->setCurveSymbolHollow(curveIdx, false);
                QwtSymbol s = curve->symbol();
                s.setStyle(QwtSymbol::NoSymbol);
                curve->setSymbol(s);
            } else {
                const int size = qMax(1, boxSymbolSize->value());
                const bool wantHollow = boxHollowSymbol && boxHollowSymbol->isChecked();
                const int symAlpha = qRound(boxSymbolOpacity->value() * 255.0 / 100.0);
                QColor symEdgeCol = boxSymbolColor->color();
                if (!symEdgeCol.isValid()
                    || (wantHollow && symEdgeCol == QColor(Qt::white))) {
                    const QColor cp = curve->pen().color();
                    symEdgeCol = (cp.isValid() && (!wantHollow || cp != QColor(Qt::white)))
                            ? cp
                            : QColor(Qt::black);
                }
                symEdgeCol.setAlpha(symAlpha);
                QPen pen(symEdgeCol);
                pen.setStyle(Qt::SolidLine);
                pen.setWidthF(qBound(PlotStyleLimits::kSymbolEdgeWidthMin, boxPenWidth->value(),
                                     PlotStyleLimits::kSymbolEdgeWidthMax));

                QBrush br;
                if (wantHollow) {
                    br = QBrush(QColor(255, 255, 255, 255), Qt::SolidPattern);
                } else if (boxFillSymbol->isChecked()) {
                    QColor fillCol = boxFillColor->color();
                    if (!fillCol.isValid() || fillCol == QColor(Qt::white))
                        fillCol = symEdgeCol;
                    fillCol.setAlpha(symAlpha);
                    br = QBrush(fillCol, Qt::SolidPattern);
                } else {
                    br = QBrush(Qt::NoBrush);
                }
                graph->setCurveSymbolHollow(curveIdx, wantHollow);
                QwtSymbol s(symStyle, br, pen, QSize(size, size));
                curve->setSymbol(s);
            }
            lineOrSymbolCommitted = true;
            needReplot = true;
            anyHandled = true;
        }

        if (lineOrSymbolCommitted && boxPlotType->isEnabled() && boxPlotType->count() > 1
            && plotItem->rtti() == QwtPlotItem::Rtti_PlotCurve) {
            // Line / Scatter / Line + Symbol follow the symbol and line style just committed.
            QSignalBlocker blocker(boxPlotType);
            const int shown = shownPlotType(curveItem);
            const int comboIdx = boxPlotType->findData(shown);
            if (comboIdx >= 0)
                boxPlotType->setCurrentIndex(comboIdx);
        }

        if (tabsToCommit.contains(histogramPage) && curveItem->plotItemType() == Graph::Histogram) {
            QwtHistogram *h = static_cast<QwtHistogram *>(plotItem);
            if (h && (automaticBox->isChecked() || validInput())) {
                if (!(h->autoBinning() == automaticBox->isChecked()
                      && h->binSize() == binSizeBox->text().toDouble()
                      && h->begin() == histogramBeginBox->text().toDouble()
                      && h->end() == histogramEndBox->text().toDouble())) {
                    h->setBinning(automaticBox->isChecked(), binSizeBox->text().toDouble(),
                                  histogramBeginBox->text().toDouble(),
                                  histogramEndBox->text().toDouble());
                    h->loadData();
                    graph->updateScale();
                }
                needReplot = true;
                anyHandled = true;
            }
        }

        if (tabsToCommit.contains(spacingPage)) {
            graph->setBarsGap(curveItem->plotItemIndex(), gapBox->value(), offsetBox->value());
            needReplot = true;
            anyHandled = true;
        }

        if (tabsToCommit.contains(vectPage)
            && (curveItem->plotItemType() == Graph::VectXYXY
                || curveItem->plotItemType() == Graph::VectXYAM)) {
            int index = curveItem->plotItemIndex();
            ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
            QString xEndCol = xEndBox->currentText();
            QString yEndCol = yEndBox->currentText();
            Table *w = app ? app->table(xEndCol) : nullptr;
            if (app && w) {
                graph->updateVectorsLayout(index, vectColorBox->color(), vectWidthBox->value(),
                                           headLengthBox->value(), headAngleBox->value(),
                                           filledHeadBox->isChecked(), vectPosBox->currentIndex(),
                                           xEndCol, yEndCol);

                QString text = curveItem->text(0);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                QStringList t = text.split(": ", Qt::SkipEmptyParts);
#else
                QStringList t = text.split(": ", QString::SkipEmptyParts);
#endif
                if (t.size() >= 2) {
                    QString table = t[0];
                    const QString prefix = table + "_";
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                    QStringList cols = t[1].split(",", Qt::SkipEmptyParts);
#else
                    QStringList cols = t[1].split(",", QString::SkipEmptyParts);
#endif
                    if (xEndCol.startsWith(prefix))
                        xEndCol = xEndCol.mid(prefix.length());
                    if (yEndCol.startsWith(prefix))
                        yEndCol = yEndCol.mid(prefix.length());
                    if (graph->curveType(index) == Graph::VectXYXY) {
                        xEndCol += "(X)";
                        yEndCol += "(Y)";
                    } else {
                        xEndCol += "(A)";
                        yEndCol += "(M)";
                    }
                    if (cols.size() >= 4 && (cols[2] != xEndCol || cols[3] != yEndCol)) {
                        cols[2] = xEndCol;
                        cols[3] = yEndCol;
                        QScopedValueRollback<bool> guard(d_loadingUi, true);
                        const QString newCurveLabel = table + ": " + cols.join(",");
                        curveItem->setText(0, newCurveLabel);
                        curveItem->setToolTip(0, newCurveLabel);
                    }
                }
                needReplot = true;
                anyHandled = true;
            } else if (explicitAccept) {
                return false;
            }
        }

        if (tabsToCommit.contains(errorsPage) && curveItem->plotItemType() == Graph::ErrorBars) {
            auto *errCurve = static_cast<QwtErrorPlotCurve *>(plotItem);
            // Fractional widths (0.5 px steps) go straight into the pen; Graph::updateErrorBars()
            // only takes whole pixels, so the exact width is set on the pen right after it.
            double errWidthF = widthBox->value();
            if (!std::isfinite(errWidthF) || errWidthF <= 0.0)
                errWidthF = errCurve->pen().widthF() > 0.0 ? errCurve->pen().widthF() : 1.0;
            errWidthF = qBound(PlotStyleLimits::kErrorBarWidthMin, errWidthF,
                               PlotStyleLimits::kErrorBarWidthMax);

            const QString capTxt = capBox->currentText().trimmed();
            bool okCap = false;
            int capLen = capTxt.toInt(&okCap);
            if (!okCap) {
                double capD = QLocale().toDouble(capTxt, &okCap);
                if (!okCap)
                    capD = capTxt.toDouble(&okCap);
                capLen = okCap ? qMax(0, qRound(capD)) : errCurve->capLength();
            }

            const double prevWidthF = errCurve->pen().widthF();
            graph->updateErrorBars(errCurve, xBox->isChecked(), qMax(1, qRound(errWidthF)), capLen,
                                   colorBox->color(), plusBox->isChecked(), minusBox->isChecked(),
                                   throughBox->isChecked());
            QPen errPen = errCurve->pen();
            errPen.setColor(colorBox->color());
            errPen.setWidthF(errWidthF);
            errPen.setCosmetic(false);
            errCurve->setPen(errPen);
            if (std::fabs(prevWidthF - errWidthF) > 1e-6)
                graph->notifyChanges();
            needReplot = true;
            anyHandled = true;
        }

        if (tabsToCommit.contains(piePage) && curveItem->plotItemType() == Graph::Pie) {
            QwtPieCurve *pie = static_cast<QwtPieCurve *>(plotItem);
            pie->setPen(QPen(boxPieLineColor->color(), boxPieLineWidth->value(),
                             Graph::getPenStyle(boxPieLineStyle->currentIndex())));
            pie->setRay(boxRadius->value());
            pie->setBrushStyle(boxPiePattern->getSelectedPattern());
            pie->setFirstColor(boxFirstColor->colorIndex(boxFirstColor->color()));
            needReplot = true;
            anyHandled = true;
        }

        if (tabsToCommit.contains(percentilePage) && curveItem->plotItemType() == Graph::Box) {
            BoxCurve *b = static_cast<BoxCurve *>(plotItem);
            if (b) {
                b->setMaxStyle(boxMaxStyle->selectedSymbol());
                b->setP99Style(box99Style->selectedSymbol());
                b->setMeanStyle(boxMeanStyle->selectedSymbol());
                b->setP1Style(box1Style->selectedSymbol());
                b->setMinStyle(boxMinStyle->selectedSymbol());

                int size = 2 * boxPercSize->value() + 1;
                QBrush br = boxFillSymbols->isChecked()
                        ? QBrush(boxPercFillColor->color(), Qt::SolidPattern)
                        : QBrush();
                QPen edgePen(boxEdgeColor->color());
                edgePen.setStyle(Qt::SolidLine);
                edgePen.setWidthF(qBound(PlotStyleLimits::kSymbolEdgeWidthMin,
                                         boxEdgeWidth->value(),
                                         PlotStyleLimits::kSymbolEdgeWidthMax));
                QwtSymbol s(QwtSymbol::NoSymbol, br, edgePen, QSize(size, size));
                b->setSymbol(s);
                needReplot = true;
                anyHandled = true;
            }
        }

        if (tabsToCommit.contains(boxPage) && curveItem->plotItemType() == Graph::Box) {
            BoxCurve *b = static_cast<BoxCurve *>(plotItem);
            if (b) {
                b->setBoxWidth(boxWidth->value());
                b->setBoxStyle(boxType->currentIndex());
                if (boxCnt->isVisible())
                    b->setBoxRange(boxRange->currentIndex() + 1, boxCnt->value());
                else
                    b->setBoxRange(boxRange->currentIndex() + 1, (double)boxCoef->value());

                if (boxWhiskersRange->currentIndex() == BoxCurve::Tukey15IQR)
                    b->setWhiskersRange(BoxCurve::Tukey15IQR, kTukeyWhiskerCoefficient);
                else if (whiskerCnt->isVisible())
                    b->setWhiskersRange(boxWhiskersRange->currentIndex(), whiskerCnt->value());
                else
                    b->setWhiskersRange(boxWhiskersRange->currentIndex(),
                                        (double)boxWhiskersCoef->value());

                b->setViolinMode(boxViolinMode->isChecked());
                b->setShowDataPoints(boxShowDataPoints->isChecked());
                needReplot = true;
                anyHandled = true;
            }
        }

        if (needReplot) {
            graph->replot();
            graph->notifyChanges();
        }
    }

    d_dirtyTabs.clear();
    if (anyHandled && d_mlGuard && !beforeSnap.isEmpty())
        d_mlGuard->pushUndoSnapshot(tr("Plot Properties"), beforeSnap);
    // An external edit that arrived while these edits were pending is picked up now.
    if (d_externalSyncDeferred && d_externalSyncTimer)
        d_externalSyncTimer->start();
    return anyHandled || !explicitAccept || tabsToCommit.contains(axesPage);
}

void PlotDialog::openColorPaletteEditor()
{
    Graph *g = nullptr;
    QTreeWidgetItem *it = listBox->currentItem();
    if (it) {
        if (it->type() == CurveTreeItem::PlotCurveTreeItem)
            g = ((CurveTreeItem *)it)->graph();
        else if (it->type() == LayerItem::LayerTreeItem)
            g = ((LayerItem *)it)->graph();
    }
    if (!g && d_ml)
        g = d_ml->activeGraph();

    if (Graph::showColorPaletteEditor(this, g)) {
        if (it && it->type() == CurveTreeItem::PlotCurveTreeItem)
            setActiveCurve((CurveTreeItem *)it);
    }
}

void PlotDialog::setAutomaticBinning()
{
    GroupBoxH->setEnabled(!automaticBox->isChecked());
}

bool PlotDialog::validInput()
{
    QString from = histogramBeginBox->text();
    QString to = histogramEndBox->text();
    QString step = binSizeBox->text();
    QRegExp nonDigit("\\D");

    if (histogramBeginBox->text().isEmpty()) {
        QMessageBox::critical(this, tr("Input error"), tr("Please enter a valid start limit!"));
        histogramBeginBox->setFocus();
        return false;
    }

    if (histogramEndBox->text().isEmpty()) {
        QMessageBox::critical(this, tr("Input error"), tr("Please enter a valid end limit!"));
        histogramEndBox->setFocus();
        return false;
    }

    if (binSizeBox->text().isEmpty()) {
        QMessageBox::critical(this, tr("Input error"), tr("Please enter a valid bin size value!"));
        binSizeBox->setFocus();
        return false;
    }

    from = from.remove(".");
    to = to.remove(".");
    step = step.remove(".");

    int pos = from.indexOf("-", 0);
    if (pos == 0)
        from = from.replace(pos, 1, "");

    pos = to.indexOf("-", 0);
    if (pos == 0)
        to = to.replace(pos, 1, "");

    double start, end, stp;
    // bool error = false;
    if (from.contains(nonDigit)) {
        try {
            MyParser parser;
            parser.SetExpr(histogramBeginBox->text());
            start = parser.Eval();
        } catch (mu::ParserError &e) {
            QMessageBox::critical(this, tr("Start limit error"), QStringFromString(e.GetMsg()));
            histogramBeginBox->setFocus();
            // error = true;
            return false;
        }
    } else
        start = histogramBeginBox->text().toDouble();

    if (to.contains(nonDigit)) {
        try {
            MyParser parser;
            parser.SetExpr(histogramEndBox->text());
            end = parser.Eval();
        } catch (mu::ParserError &e) {
            QMessageBox::critical(this, tr("End limit error"), QStringFromString(e.GetMsg()));
            histogramEndBox->setFocus();
            // error=true;
            return false;
        }
    } else
        end = histogramEndBox->text().toDouble();

    if (start >= end) {
        QMessageBox::critical(this, tr("Input error"),
                              tr("Please enter limits that satisfy: begin < end!"));
        histogramEndBox->setFocus();
        return false;
    }

    if (step.contains(nonDigit)) {
        try {
            MyParser parser;
            parser.SetExpr(binSizeBox->text());
            stp = parser.Eval();
        } catch (mu::ParserError &e) {
            QMessageBox::critical(this, tr("Bin size input error"), QStringFromString(e.GetMsg()));
            binSizeBox->setFocus();
            // error=true;
            return false;
        }
    } else
        stp = binSizeBox->text().toDouble();

    if (stp <= 0) {
        QMessageBox::critical(this, tr("Bin size input error"),
                              tr("Please enter a positive bin size value!"));
        binSizeBox->setFocus();
        return false;
    }

    return true;
}
void PlotDialog::setPiePenStyle(const Qt::PenStyle &style)
{
    if (style == Qt::SolidLine)
        boxPieLineStyle->setCurrentIndex(0);
    if (style == Qt::DashLine)
        boxPieLineStyle->setCurrentIndex(1);
    if (style == Qt::DotLine)
        boxPieLineStyle->setCurrentIndex(2);
    if (style == Qt::DashDotLine)
        boxPieLineStyle->setCurrentIndex(3);
    if (style == Qt::DashDotDotLine)
        boxPieLineStyle->setCurrentIndex(4);
}

void PlotDialog::setBoxType(int index)
{
    boxCoeffLabel->hide();
    boxRangeLabel->hide();
    boxRange->hide();
    boxCoef->hide();
    boxCntLabel->hide();
    boxCnt->hide();

    if (index != BoxCurve::NoBox && index != BoxCurve::WindBox) {
        boxRange->show();
        boxRangeLabel->show();
        int id = boxRange->currentIndex() + 1;
        if (id == BoxCurve::UserDef) {
            boxCoef->show();
            boxCoeffLabel->show();
        } else if (id == BoxCurve::SD || id == BoxCurve::SE) {
            boxCntLabel->show();
            boxCnt->show();
        }
    }
}

void PlotDialog::setBoxRangeType(int index)
{
    boxCoeffLabel->hide();
    boxCoef->hide();
    boxCntLabel->hide();
    boxCnt->hide();

    index++;
    if (index == BoxCurve::UserDef) {
        boxCoeffLabel->show();
        boxCoef->show();
    } else if (index == BoxCurve::SD || index == BoxCurve::SE) {
        boxCntLabel->show();
        boxCnt->show();
    }
}

void PlotDialog::setWhiskersRange(int index)
{
    whiskerCoeffLabel->hide();
    boxWhiskersCoef->hide();
    whiskerCntLabel->hide();
    whiskerCnt->hide();

    // Tukey 1.5*IQR has a fixed multiplier, so it shows no coefficient input.
    if (index == BoxCurve::UserDef) {
        whiskerCoeffLabel->show();
        boxWhiskersCoef->show();
    } else if (index == BoxCurve::SD || index == BoxCurve::SE) {
        whiskerCntLabel->show();
        whiskerCnt->show();
    }
}

void PlotDialog::customVectorsPage(bool angleMag)
{
    if (angleMag) {
        GroupBoxVectEnd->setTitle(tr("Vector Data"));
        labelXEnd->setText(tr("Angle"));
        labelYEnd->setText(tr("Magnitude"));
        labelPosition->show();
        vectPosBox->show();
    } else {
        GroupBoxVectEnd->setTitle(tr("End Point"));
        labelXEnd->setText(tr("X End"));
        labelYEnd->setText(tr("Y End"));
        labelPosition->hide();
        vectPosBox->hide();
    }
}

void PlotDialog::onColorMapPresetChanged(int index)
{
    const auto preset = static_cast<Spectrogram::ColorMapPreset>(index);
    if (preset == Spectrogram::PresetCustom) {
        colorMapEditor->show();
        colorMapEditor->setFocus();
    } else {
        colorMapEditor->setColorMap(
                Spectrogram::presetColorMap(preset, boxReversePalette->isChecked()));
        colorMapEditor->hide();
    }
}

void PlotDialog::onReversePaletteToggled(bool checked)
{
    const auto preset =
            static_cast<Spectrogram::ColorMapPreset>(boxColorMapPreset->currentIndex());
    if (preset != Spectrogram::PresetCustom)
        colorMapEditor->setColorMap(Spectrogram::presetColorMap(preset, checked));
}

void PlotDialog::onCustomZRangeToggled(bool checked)
{
    boxZMin->setEnabled(checked);
    boxZMax->setEnabled(checked);
    if (checked) {
        colorMapEditor->setRange(boxZMin->value(), boxZMax->value());
    } else if (listBox->currentItem()
               && listBox->currentItem()->type() == CurveTreeItem::PlotCurveTreeItem) {
        const QwtPlotItem *pi = ((CurveTreeItem *)listBox->currentItem())->plotItem();
        if (pi && pi->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            const QwtDoubleInterval rawR = ((const Spectrogram *)pi)->rawDataRange();
            boxZMin->setValue(rawR.minValue());
            boxZMax->setValue(rawR.maxValue());
            colorMapEditor->setRange(rawR.minValue(), rawR.maxValue());
        }
    }
}

void PlotDialog::showDefaultContourLinesBox(bool)
{
    if (autoContourBox->isChecked())
        defaultPenBox->hide();
    else
        defaultPenBox->show();
}

void PlotDialog::updateTreeWidgetItem(QTreeWidgetItem *item)
{
    if (item->type() != QTreeWidgetItem::Type)
        return;

    if (item->isExpanded())
        item->setIcon(0, IconLoader::duotoneIcon("folder-open"));
    else
        item->setIcon(0, IconLoader::duotoneIcon("folder-closed"));
}

// The opacity spin boxes hold 0..100 %; \a percent here is that value, the colours keep 8-bit alpha.
void PlotDialog::updateBackgroundTransparency(int percent)
{
    boxBackgroundColor->setEnabled(percent > 0);
    const int alpha = alphaForCommit(percent, d_bgAlphaLoaded);
    if (alpha != boxBackgroundColor->color().alpha()) {
        QSignalBlocker blocker(boxBackgroundColor);
        QColor c = boxBackgroundColor->color();
        c.setAlpha(alpha);
        boxBackgroundColor->setColor(c);
    }
    markTabDirty(layerPage);
}

void PlotDialog::updateCanvasTransparency(int percent)
{
    boxCanvasColor->setEnabled(percent > 0);
    const int alpha = alphaForCommit(percent, d_canvasAlphaLoaded);
    if (alpha != boxCanvasColor->color().alpha()) {
        QSignalBlocker blocker(boxCanvasColor);
        QColor c = boxCanvasColor->color();
        c.setAlpha(alpha);
        boxCanvasColor->setColor(c);
    }
    markTabDirty(layerPage);
}

void PlotDialog::pickCanvasColor(QColor c)
{
    const int percent = percentFromAlpha(c.alpha());
    if (percent != boxCanvasTransparency->value()) {
        QSignalBlocker blocker(boxCanvasTransparency);
        boxCanvasTransparency->setValue(percent);
        d_canvasAlphaLoaded = c.alpha();
    }
    markTabDirty(layerPage);
}

void PlotDialog::pickBackgroundColor(QColor c)
{
    const int percent = percentFromAlpha(c.alpha());
    if (percent != boxBackgroundTransparency->value()) {
        QSignalBlocker blocker(boxBackgroundTransparency);
        boxBackgroundTransparency->setValue(percent);
        d_bgAlphaLoaded = c.alpha();
    }
    markTabDirty(layerPage);
}

void PlotDialog::pickBorderColor(QColor)
{
    markTabDirty(layerPage);
}

void PlotDialog::updateAntialiasing(bool)
{
    markTabDirty(layerPage);
}

void PlotDialog::updateBorder(int)
{
    markTabDirty(layerPage);
}

void PlotDialog::changeMargin(int)
{
    markTabDirty(layerPage);
}

void PlotDialog::setTitlesFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, titleFont, this);
    if (ok) {
        if (font.family().trimmed().isEmpty())
            font.setFamily("Arial");
        titleFont = PlotStyleLimits::preserveFractionalPtAfterFontDialog(titleFont, font);
        syncFontSizeBoxes();
        markTabDirty(fontsPage);
    }
}

void PlotDialog::setAxesLabelsFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, axesFont, this);
    if (ok) {
        if (font.family().trimmed().isEmpty())
            font.setFamily("Arial");
        axesFont = PlotStyleLimits::preserveFractionalPtAfterFontDialog(axesFont, font);
        syncFontSizeBoxes();
        markTabDirty(fontsPage);
    }
}

void PlotDialog::setAxesNumbersFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, numbersFont, this);
    if (ok) {
        if (font.family().trimmed().isEmpty())
            font.setFamily("Arial");
        numbersFont = PlotStyleLimits::preserveFractionalPtAfterFontDialog(numbersFont, font);
        syncFontSizeBoxes();
        markTabDirty(fontsPage);
    }
}

void PlotDialog::setLegendsFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, legendFont, this);
    if (ok) {
        if (font.family().trimmed().isEmpty())
            font.setFamily("Arial");
        legendFont = PlotStyleLimits::preserveFractionalPtAfterFontDialog(legendFont, font);
        syncFontSizeBoxes();
        markTabDirty(fontsPage);
    }
}

void PlotDialog::initFonts(const QFont &titlefont, const QFont &axesfont, const QFont &numbersfont,
                           const QFont &legendfont)
{
    axesFont = axesfont;
    if (axesFont.family().isEmpty())
        axesFont.setFamily("Arial");
    titleFont = titlefont;
    if (titleFont.family().isEmpty())
        titleFont.setFamily("Arial");
    numbersFont = numbersfont;
    if (numbersFont.family().isEmpty())
        numbersFont.setFamily("Arial");
    legendFont = legendfont;
    if (legendFont.family().isEmpty())
        legendFont.setFamily("Arial");
    // Same size range as every other font-size control (kFontSizeMin..kFontSizeMax pt).
    syncFontSizeBoxes();
}

void PlotDialog::syncPageTabFromMultiLayer()
{
    if (!d_ml || !d_mlGuard)
        return;

    QString unitName;
    switch (d_mlGuard->pageUnit()) {
    case MultiLayer::UnitMm:
        unitName = QStringLiteral("mm");
        break;
    case MultiLayer::UnitCm:
        unitName = QStringLiteral("cm");
        break;
    case MultiLayer::UnitInch:
        unitName = tr("inch");
        break;
    default:
        unitName = QStringLiteral("px");
        break;
    }
    if (labelPageSize)
        labelPageSize->setText(QStringLiteral("%1 x %2 %3, %4 dpi")
                                       .arg(d_mlGuard->pageWidthPhysical(), 0, 'f', 2)
                                       .arg(d_mlGuard->pageHeightPhysical(), 0, 'f', 2)
                                       .arg(unitName)
                                       .arg(d_mlGuard->pageDpi()));
    if (labelPageZoom)
        labelPageZoom->setText(QStringLiteral("%1 %").arg(qRound(d_mlGuard->pageZoom() * 100.0)));

    QScopedValueRollback<bool> guard(d_loadingUi, true);
    if (boxScaleLayers)
        boxScaleLayers->setChecked(d_mlGuard->scaleLayersOnPrint());
    if (boxPrintCrops)
        boxPrintCrops->setChecked(d_mlGuard->printCropmarksEnabled());
}

void PlotDialog::openPageSetupFromDialog()
{
    if (!d_ml || !d_mlGuard)
        return;
    // Page Setup applies and records its own Undo step; its edits must not reload this dialog
    // underneath it.
    {
        QScopedValueRollback<int> selfEdit(d_selfEditDepth, d_selfEditDepth + 1);
        d_mlGuard->showPageSetupDialog();
    }
    if (!d_mlGuard)
        return;
    syncPageTabFromMultiLayer();
}

void PlotDialog::closeEvent(QCloseEvent *e)
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    if (app)
        app->d_extended_plot_dialog = btnMore->isChecked();
    if (d_externalSyncTimer)
        d_externalSyncTimer->stop();

    e->accept();
}

void PlotDialog::hideEvent(QHideEvent *e)
{
    if (d_externalSyncTimer)
        d_externalSyncTimer->stop();
    QDialog::hideEvent(e);
}

void PlotDialog::reject()
{
    if (d_externalSyncTimer)
        d_externalSyncTimer->stop();
    if (testAttribute(Qt::WA_DeleteOnClose)) {
        // Call close() while visible so closeEvent() and WA_DeleteOnClose cleanup run,
        // then QDialog::reject() sets QDialog::Rejected and emits rejected()/finished().
        QPointer<PlotDialog> self(this);
        close();
        if (self)
            QDialog::reject();
        return;
    }
    QDialog::reject();
}

/*****************************************************************************
 *
 * Synchronisation with edits made outside this dialog
 *
 * Policy (the dialog used to mix two models):
 *  - Property edits (the tab pages) are Apply-deferred and recorded as ONE Undo step by
 *    commitParamsForItem().
 *  - Structural operations (plot type, delete, move up/down, visibility check box) are applied
 *    immediately, each as its own Undo step, but only after flushPendingEdits() wrote the
 *    pending property edits, so nothing is lost or merged into the wrong Undo step.
 *  - MultiLayer::modifiedPlot from elsewhere (right dock, canvas, other dialogs, Undo / Redo)
 *    re-reads the dialog after a short delay. Pending property edits are never overwritten:
 *    the reload waits until they are applied or the selection changes. The current layer /
 *    curve selection and the current tab are kept.
 *
 *****************************************************************************/

void PlotDialog::onMultiLayerModified()
{
    if (!isVisible() || d_loadingUi || d_selfEditDepth > 0 || !d_mlGuard
        || !d_externalSyncTimer)
        return;
    if (!d_externalSyncTimer->isActive())
        d_externalSyncTimer->start();
}

void PlotDialog::syncFromMultiLayer()
{
    if (!isVisible() || !d_mlGuard || !listBox || !d_externalSyncTimer)
        return;
    // Busy (this dialog is itself editing / a nested modal dialog runs the event loop, or an
    // Undo / Redo is rebuilding the layers): try again shortly.
    if (d_loadingUi || d_selfEditDepth > 0 || d_mlGuard->isRestoringSnapshot()) {
        d_externalSyncTimer->start();
        return;
    }
    if (!d_dirtyTabs.isEmpty()) {
        d_externalSyncDeferred = true;
        return;
    }
    d_externalSyncDeferred = false;

    if (treeMatchesMultiLayer())
        reloadCurrentSelection();
    else
        rebuildTreeFromMultiLayer();
}

bool PlotDialog::treeMatchesMultiLayer() const
{
    MultiLayer *ml = d_mlGuard.data();
    if (!ml || !listBox)
        return false;
    QTreeWidgetItem *pageItem = listBox->topLevelItem(0);
    if (!pageItem || listBox->topLevelItemCount() != 1)
        return false;

    const QWidgetList plots = ml->graphPtrs();
    if (pageItem->childCount() != plots.count())
        return false;
    for (int i = 0; i < plots.count(); ++i) {
        LayerItem *li = dynamic_cast<LayerItem *>(pageItem->child(i));
        Graph *g = qobject_cast<Graph *>(plots.at(i));
        if (!li || !g || li->graph() != g)
            return false;
        if (li->childCount() != g->curves())
            return false;
        for (int k = 0; k < li->childCount(); ++k) {
            CurveTreeItem *ci = dynamic_cast<CurveTreeItem *>(li->child(k));
            if (!ci || ci->plotItem() == nullptr || ci->plotItem() != g->plotItem(k))
                return false;
        }
    }
    return true;
}

void PlotDialog::rebuildTreeFromMultiLayer()
{
    if (!d_mlGuard || !listBox)
        return;

    // Remember what is selected and expanded: layer index, curve index (and the curve itself, so
    // a reordered curve keeps the selection), and the tab page that is showing.
    int selLayer = -1, selCurve = -1;
    const QwtPlotItem *selPlotItem = nullptr;
    QVector<bool> layerExpanded;
    QWidget *prevTab = privateTabWidget ? privateTabWidget->currentWidget() : nullptr;
    if (QTreeWidgetItem *pageItem = listBox->topLevelItem(0)) {
        for (int i = 0; i < pageItem->childCount(); ++i)
            layerExpanded.append(pageItem->child(i)->isExpanded());
        QTreeWidgetItem *cur = listBox->currentItem();
        if (cur && cur != pageItem) {
            if (cur->type() == LayerItem::LayerTreeItem) {
                selLayer = pageItem->indexOfChild(cur);
            } else if (cur->type() == CurveTreeItem::PlotCurveTreeItem && cur->parent()) {
                selLayer = pageItem->indexOfChild(cur->parent());
                selCurve = cur->parent()->indexOfChild(cur);
                selPlotItem = static_cast<CurveTreeItem *>(cur)->plotItem();
            }
        }
    }

    QTreeWidgetItem *target = nullptr;
    {
        QScopedValueRollback<bool> guard(d_loadingUi, true);
        QSignalBlocker blocker(listBox);
        d_dirtyTabs.clear();
        populateTreeFromMultiLayer(false);

        QTreeWidgetItem *pageItem = listBox->topLevelItem(0);
        target = pageItem;
        if (pageItem) {
            for (int i = 0; i < pageItem->childCount() && i < layerExpanded.size(); ++i)
                pageItem->child(i)->setExpanded(layerExpanded.at(i));
            if (selLayer >= 0 && selLayer < pageItem->childCount()) {
                QTreeWidgetItem *layerIt = pageItem->child(selLayer);
                target = layerIt;
                if (selCurve >= 0 && layerIt->childCount() > 0) {
                    int pick = qBound(0, selCurve, layerIt->childCount() - 1);
                    for (int k = 0; k < layerIt->childCount(); ++k) {
                        CurveTreeItem *ci = dynamic_cast<CurveTreeItem *>(layerIt->child(k));
                        if (selPlotItem && ci && ci->plotItem() == selPlotItem) {
                            pick = k;
                            break;
                        }
                    }
                    layerIt->setExpanded(true);
                    target = layerIt->child(pick);
                } else if (selCurve >= 0) {
                    target = layerIt; // its curves are gone: fall back to the layer
                }
            }
        }
        if (target)
            listBox->setCurrentItem(target);
    }

    // The tree signals were blocked, so load the page for the new current item explicitly
    // (same path as a normal selection change; there is no previous item to commit).
    if (target) {
        updateTabWindow(target, nullptr);
        if (prevTab && privateTabWidget->indexOf(prevTab) >= 0)
            privateTabWidget->setCurrentWidget(prevTab);
    }
}

void PlotDialog::reloadCurrentSelection()
{
    if (!listBox)
        return;
    QTreeWidgetItem *cur = listBox->currentItem();
    if (!cur)
        return;

    QScopedValueRollback<bool> guard(d_loadingUi, true);
    d_dirtyTabs.clear();

    // Visibility check marks may have been changed by the inspector.
    if (QTreeWidgetItem *pageItem = listBox->topLevelItem(0)) {
        for (int i = 0; i < pageItem->childCount(); ++i) {
            LayerItem *li = dynamic_cast<LayerItem *>(pageItem->child(i));
            Graph *g = li ? li->graph() : nullptr;
            if (!g)
                continue;
            for (int k = 0; k < li->childCount(); ++k) {
                CurveTreeItem *ci = dynamic_cast<CurveTreeItem *>(li->child(k));
                if (!ci || !ci->isCurveAlive())
                    continue;
                const int idx = ci->plotItemIndex();
                if (idx < 0)
                    continue;
                const Qt::CheckState want = g->isCurveVisible(idx) ? Qt::Checked : Qt::Unchecked;
                if (ci->checkState(0) != want)
                    ci->setCheckState(0, want);
            }
        }
    }

    QSignalBlocker typeBlocker(boxPlotType);
    if (cur->type() == CurveTreeItem::PlotCurveTreeItem) {
        CurveTreeItem *ci = static_cast<CurveTreeItem *>(cur);
        if (!ci->isCurveAlive())
            return;
        setActiveCurve(ci);
        const int plotType = setPlotType(ci);
        // e.g. the inspector turned a Line into a Line + Symbol curve: other pages are needed.
        if (plotType >= 0 && plotType != d_shownTabType)
            rebuildCurveTabs(plotType, true);
    } else if (cur->type() == LayerItem::LayerTreeItem) {
        setActiveLayer(static_cast<LayerItem *>(cur));
    } else {
        syncPageTabFromMultiLayer();
    }
    d_dirtyTabs.clear();
    updateCurveMoveButtons();
}

/*****************************************************************************
 *
 * Class LayerItem
 *
 *****************************************************************************/

LayerItem::LayerItem(Graph *g, QTreeWidgetItem *parent, const QString &s)
    : QTreeWidgetItem(parent, QStringList(s), LayerTreeItem), d_graph(g), d_graphGuard(g)
{
    setIcon(0, IconLoader::duotoneIcon("layer-add"));
    setToolTip(0, s);
    if (g)
        insertCurvesList();
}

void LayerItem::setActive(bool on)
{
    if (on)
        setIcon(0, IconLoader::duotoneIcon("layer-add"));
    else
        setIcon(0, IconLoader::duotoneIcon("layer-add"));
}

void LayerItem::rebuildCurvesList()
{
    while (childCount() > 0)
        delete takeChild(0);
    insertCurvesList();
}

void LayerItem::insertCurvesList()
{
    Graph *g = graph();
    if (!g)
        return;
    for (int i = 0; i < g->curves(); i++) {
        QString plotAssociation = QString();
        const QwtPlotItem *it = (QwtPlotItem *)g->plotItem(i);
        if (!it)
            continue;

        if (it->rtti() == QwtPlotItem::Rtti_PlotCurve) {
            PlotCurve *c = (PlotCurve *)it;
            if (c->type() != Graph::Function) {
                QString s = ((DataCurve *)it)->plotAssociation();
                Table *tbl = ((DataCurve *)it)->table();
                QString table = tbl ? tbl->name() : QString();
                if (table.isEmpty()) {
                    plotAssociation = s;
                } else {
                    const QString prefix = table + "_";
                    QStringList parts = s.split(",");
                    for (int k = 0; k < parts.size(); ++k) {
                        QString tok = parts[k].trimmed();
                        if (tok.startsWith(prefix))
                            tok = tok.mid(prefix.length());
                        parts[k] = tok;
                    }
                    plotAssociation = table + ": " + parts.join(",");
                }
            } else
                plotAssociation = it->title().text();
        } else
            plotAssociation = it->title().text();

        addChild(new CurveTreeItem(it, this, plotAssociation));
    }
}

/*****************************************************************************
 *
 * Class CurveTreeItem
 *
 *****************************************************************************/

CurveTreeItem::CurveTreeItem(const QwtPlotItem *curve, LayerItem *parent, const QString &s)
    : QTreeWidgetItem(parent, QStringList(s), PlotCurveTreeItem), d_curve(curve)
{
    setIcon(0, IconLoader::duotoneIcon("new-graph"));
    setToolTip(0, s);
    setFlags(flags() | Qt::ItemIsUserCheckable);
    Graph *g = graph();
    int idx = plotItemIndex();
    const bool vis = (g && idx >= 0) ? g->isCurveVisible(idx) : (curve ? curve->isVisible() : true);
    setCheckState(0, vis ? Qt::Checked : Qt::Unchecked);
}

bool CurveTreeItem::isCurveAlive() const
{
    if (!d_curve)
        return false;
    Graph *g = graph();
    if (!g || !g->plotWidget())
        return false;
    return g->plotWidget()->itemList().contains(const_cast<QwtPlotItem *>(d_curve));
}

const QwtPlotItem *CurveTreeItem::plotItem() const
{
    if (!isCurveAlive())
        return nullptr;
    return d_curve;
}

void CurveTreeItem::setActive(bool on)
{
    if (!isCurveAlive())
        return;
    if (on)
        setIcon(0, IconLoader::duotoneIcon("new-graph"));
    else
        setIcon(0, IconLoader::duotoneIcon("new-graph"));
}

int CurveTreeItem::plotItemIndex() const
{
    if (!isCurveAlive())
        return -1;
    Graph *g = graph();
    if (!g)
        return -1;

    return g->plotItemIndex(const_cast<QwtPlotItem *>(d_curve));
}

int CurveTreeItem::plotItemType() const
{
    if (!isCurveAlive())
        return -1;
    Graph *g = graph();
    if (!g)
        return -1;

    int index = g->plotItemIndex(const_cast<QwtPlotItem *>(d_curve));
    return index >= 0 ? g->curveType(index) : -1;
}

int CurveTreeItem::plotItemStyle() const
{
    if (!isCurveAlive())
        return -1;
    if (d_curve->rtti() == QwtPlotItem::Rtti_PlotCurve)
        return static_cast<const QwtPlotCurve *>(d_curve)->style();
    return -1;
}
