/***************************************************************************
    File                 : GraphInspectorDock_Panels.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Copyright            : (C) 2026 ToriGraph Project
    Description          : Inspector page builders (Page/Layer, Axis, Curve, Legend)
                           (split translation unit)
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

#include "GraphInspectorDock.h"
#include "GraphInspectorDock_Internal.h"

#include "ApplicationWindow.h"
#include "ArrowMarker.h"
#include "AxesDialog.h"
#include "FunctionCurve.h"
#include "GraphMiniToolbar.h"
#include "Grid.h"
#include "I18nJapanese.h"
#include "IconLoader.h"
#include "ImageMarker.h"
#include "Legend.h"
#include "ModernRibbonAndHud.h"
#include "ModernUiTheme.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "CanvasInlineTextEditor.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"
#include "ScaleDraw.h"
#include "Spectrogram.h"
#include "PlotStyleLimits.h"
#include "SymbolBox.h"
#include "Table.h"
#include "ImportASCIIDialog.h"
#include "Workbook.h"
#include "XlsxImport.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTextCursor>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyleOptionComboBox>
#include <QStylePainter>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVector>
#include <QWidgetAction>

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>

using namespace GraphInspectorInternal;

namespace {

// Font size box: whole sizes read "14 pt", fractional ones "12.9 pt" (the lab slide preset).
class InspectorPtSpinBox : public QDoubleSpinBox
{
public:
    explicit InspectorPtSpinBox(QWidget *parent) : QDoubleSpinBox(parent)
    {
        setDecimals(1);
        setSingleStep(1.0);
    }

protected:
    QString textFromValue(double v) const override
    {
        const double r = std::round(v * 10.0) / 10.0;
        const bool whole = std::fabs(r - std::round(r)) < 1e-9;
        return locale().toString(r, 'f', whole ? 0 : 1);
    }
};

class ElidedPresetComboBox : public QComboBox
{
public:
    using QComboBox::QComboBox;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter painter(this);
        painter.setPen(palette().color(QPalette::Text));

        QStyleOptionComboBox opt;
        initStyleOption(&opt);
        painter.drawComplexControl(QStyle::CC_ComboBox, opt);

        const QRect textRect = style()->subControlRect(QStyle::CC_ComboBox, &opt,
                                                      QStyle::SC_ComboBoxEditField, this);
        if (textRect.isValid()) {
            const int idx = currentIndex();
            const int availW = qMax(12, textRect.width() - 2);
            QString label = opt.currentText;
            if (idx >= 0 && fontMetrics().horizontalAdvance(label) > availW) {
                const QString compact = itemData(idx, Qt::UserRole + 1).toString();
                if (!compact.isEmpty())
                    label = compact;
            }
            opt.currentText = fontMetrics().elidedText(label, Qt::ElideRight, availW);
        }
        painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
    }
};

static QLabel *makeSectionHeaderLabel(QWidget *parent)
{
    QLabel *lbl = new QLabel(parent);
    lbl->setObjectName(QStringLiteral("InspectorSectionHeader"));
    return lbl;
}

} // namespace

QWidget *GraphInspectorDock::buildPageLayerInspector()
{
    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 12, 10);
    lay->setSpacing(6);

    // 1) Style Presets & 70% Square Frame
    d_sec_presets_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_presets_lbl);

    QHBoxLayout *presetQuickRow = new QHBoxLayout();
    presetQuickRow->setContentsMargins(0, 0, 0, 0);
    presetQuickRow->setSpacing(4);
    d_btn_preset_isij = new QPushButton(page);
    d_btn_preset_isij->setIcon(IconLoader::duotoneIcon(QStringLiteral("preset-isij"), 14));
    d_btn_preset_isij->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(d_btn_preset_isij, &QPushButton::clicked, this, [this]() {
        if (!d_target_ml)
            return;
        d_target_ml->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        if (d_app)
            emit d_app->modified();
        refreshAll();
    });
    presetQuickRow->addWidget(d_btn_preset_isij, 1);

    d_btn_preset_slide28 = new QPushButton(page);
    d_btn_preset_slide28->setIcon(IconLoader::duotoneIcon(QStringLiteral("preset-slide"), 14));
    d_btn_preset_slide28->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(d_btn_preset_slide28, &QPushButton::clicked, this, [this]() {
        if (!d_target_ml)
            return;
        d_target_ml->applyStylePresetById(QStringLiteral("lab_slide_83mm"));
        if (d_app)
            emit d_app->modified();
        refreshAll();
    });
    presetQuickRow->addWidget(d_btn_preset_slide28, 1);
    lay->addLayout(presetQuickRow);

    QHBoxLayout *customPresetRow = new QHBoxLayout();
    customPresetRow->setSpacing(3);
    d_cb_user_presets = new ElidedPresetComboBox(page);
    d_cb_user_presets->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    d_cb_user_presets->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    d_cb_user_presets->setMinimumContentsLength(10);
    d_cb_user_presets->setStyleSheet(
            QStringLiteral("QComboBox { font-size: 8.5pt; padding: 2px 18px 2px 6px; }"));
    if (QAbstractItemView *pv = d_cb_user_presets->view())
        pv->setTextElideMode(Qt::ElideNone);
    connect(d_cb_user_presets, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                if (!d_cb_user_presets)
                    return;
                const QString fullTip = (idx >= 0)
                        ? d_cb_user_presets->itemData(idx, Qt::ToolTipRole).toString()
                        : QString();
                d_cb_user_presets->setToolTip(
                        fullTip.isEmpty() ? d_cb_user_presets->currentText() : fullTip);
            });
    d_btn_apply_user_preset = new QPushButton(page);
    d_btn_apply_user_preset->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    d_btn_apply_user_preset->setMaximumWidth(54);
    connect(d_btn_apply_user_preset, &QPushButton::clicked, this, [this]() {
        if (!d_target_ml || !d_cb_user_presets || d_cb_user_presets->currentIndex() < 0)
            return;
        const QString pid = d_cb_user_presets->currentData().toString();
        if (!pid.isEmpty()) {
            d_target_ml->applyStylePresetById(pid);
            refreshAll();
        }
    });
    d_btn_manage_presets = new QToolButton(page);
    d_btn_manage_presets->setObjectName(QStringLiteral("InspectorSymbolChipBtn"));
    d_btn_manage_presets->setIcon(IconLoader::duotoneIcon(QStringLiteral("page-setup"), 14));
    d_btn_manage_presets->setFixedSize(26, 26);
    d_btn_manage_presets->setCursor(Qt::PointingHandCursor);
    connect(d_btn_manage_presets, &QToolButton::clicked, this, [this]() {
        MultiLayer::showStylePresetManagerDialog(d_app, d_target_ml);
        refreshAll();
    });
    customPresetRow->addWidget(d_cb_user_presets, 1);
    customPresetRow->addWidget(d_btn_apply_user_preset, 0);
    customPresetRow->addWidget(d_btn_manage_presets, 0);
    lay->addLayout(customPresetRow);

    d_btn_square_frame70 = new QPushButton(page);
    d_btn_square_frame70->setIcon(IconLoader::duotoneIcon(QStringLiteral("aspect-square"), 15));
    connect(d_btn_square_frame70, &QPushButton::clicked, this, [this]() {
        if (!d_target_ml)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->applySquareCanvasFrame70Percent();
        d_target_ml->pushUndoSnapshot(tr("Square Frame (70%)"), before);
        syncPageLayerControls();
    });
    lay->addWidget(d_btn_square_frame70);

    // 1b) Aspect Ratio Lock & Quick Ratio Presets
    d_sec_aspect_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_aspect_lbl);

    d_chk_keep_aspect = new QCheckBox(page);
    d_chk_keep_aspect->setObjectName(QStringLiteral("InspectorKeepAspectCheck"));
    connect(d_chk_keep_aspect, &QCheckBox::toggled, this, [this](bool checked) {
        if (d_syncing_props || !d_target_ml)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->setKeepAspectRatio(checked);
        d_target_ml->pushUndoSnapshot(tr("Toggle Aspect Ratio Lock"), before);
        if (d_app && d_app->modernRibbonBar())
            d_app->modernRibbonBar()->syncCheckableStates();
        syncPageLayerControls();
    });
    lay->addWidget(d_chk_keep_aspect);

    QHBoxLayout *ratioRow = new QHBoxLayout();
    ratioRow->setContentsMargins(0, 0, 0, 0);
    ratioRow->setSpacing(3);

    auto makeRatioBtn = [&](double ratio, bool lock) -> QPushButton * {
        QPushButton *b = new QPushButton(page);
        b->setObjectName(QStringLiteral("InspectorCompactRatioBtn"));
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, ratio, lock]() {
            if (!d_target_ml)
                return;
            const QString before = d_target_ml->captureUndoSnapshot();
            if (lock)
                d_target_ml->applyAspectRatioPreset(ratio, true);
            else
                d_target_ml->setKeepAspectRatio(false);
            d_target_ml->pushUndoSnapshot(
                    lock ? tr("Apply Aspect Ratio Preset") : tr("Toggle Aspect Ratio Lock"),
                    before);
            if (d_app && d_app->modernRibbonBar())
                d_app->modernRibbonBar()->syncCheckableStates();
            syncPageLayerControls();
        });
        return b;
    };

    d_btn_ratio_1_1 = makeRatioBtn(1.0, true);
    d_btn_ratio_4_3 = makeRatioBtn(4.0 / 3.0, true);
    d_btn_ratio_16_9 = makeRatioBtn(16.0 / 9.0, true);
    d_btn_ratio_3_4 = makeRatioBtn(3.0 / 4.0, true);
    d_btn_ratio_free = makeRatioBtn(0.0, false);

    ratioRow->addWidget(d_btn_ratio_1_1, 1);
    ratioRow->addWidget(d_btn_ratio_4_3, 1);
    ratioRow->addWidget(d_btn_ratio_16_9, 1);
    ratioRow->addWidget(d_btn_ratio_3_4, 1);
    ratioRow->addWidget(d_btn_ratio_free, 1);
    lay->addLayout(ratioRow);

    // 2) Physical Page Size & Zoom
    d_sec_page_size_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_page_size_lbl);

    QGridLayout *sizeGrid = new QGridLayout();
    sizeGrid->setContentsMargins(0, 0, 0, 0);
    sizeGrid->setHorizontalSpacing(6);
    sizeGrid->setVerticalSpacing(4);
    sizeGrid->setColumnStretch(1, 1);

    d_lbl_page_w = new QLabel(page);
    d_spin_page_w_mm = new QDoubleSpinBox(page);
    d_spin_page_w_mm->setRange(20.0, 1000.0);
    d_spin_page_w_mm->setDecimals(1);
    d_spin_page_w_mm->setSingleStep(5.0);
    d_spin_page_w_mm->setSuffix(QStringLiteral(" mm"));
    d_spin_page_w_mm->setMinimumWidth(76);
    d_spin_page_w_mm->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    d_lbl_page_h = new QLabel(page);
    d_spin_page_h_mm = new QDoubleSpinBox(page);
    d_spin_page_h_mm->setRange(20.0, 1000.0);
    d_spin_page_h_mm->setDecimals(1);
    d_spin_page_h_mm->setSingleStep(5.0);
    d_spin_page_h_mm->setSuffix(QStringLiteral(" mm"));
    d_spin_page_h_mm->setMinimumWidth(76);
    d_spin_page_h_mm->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // B1-7 + A4-5: include px (MultiLayer::UnitPx) and give Unit its own 2-column row
    // so 252px dock width never clips Width/Height spinboxes.
    d_lbl_page_unit = new QLabel(page);
    d_combo_page_unit = new QComboBox(page);
    d_combo_page_unit->addItem(QStringLiteral("mm"), int(MultiLayer::UnitMm));
    d_combo_page_unit->addItem(QStringLiteral("cm"), int(MultiLayer::UnitCm));
    d_combo_page_unit->addItem(QStringLiteral("in"), int(MultiLayer::UnitInch));
    d_combo_page_unit->addItem(QStringLiteral("px"), int(MultiLayer::UnitPx));
    d_combo_page_unit->setObjectName(QStringLiteral("inspectorPageUnitCombo"));

    d_lbl_page_dpi = new QLabel(page);
    d_spin_page_dpi = new QSpinBox(page);
    // E7: same upper limit as the export dialog and the preset editor.
    d_spin_page_dpi->setRange(72, 2400);
    d_spin_page_dpi->setSingleStep(50);
    d_spin_page_dpi->setValue(300);

    sizeGrid->addWidget(d_lbl_page_w, 0, 0);
    sizeGrid->addWidget(d_spin_page_w_mm, 0, 1);
    sizeGrid->addWidget(d_lbl_page_h, 1, 0);
    sizeGrid->addWidget(d_spin_page_h_mm, 1, 1);
    sizeGrid->addWidget(d_lbl_page_unit, 2, 0);
    sizeGrid->addWidget(d_combo_page_unit, 2, 1);
    sizeGrid->addWidget(d_lbl_page_dpi, 3, 0);
    sizeGrid->addWidget(d_spin_page_dpi, 3, 1);

    auto applyPageSize = [this]() {
        if (d_syncing_props || !d_target_ml)
            return;
        const double unitF =
                pageUnitToMmFactor(static_cast<MultiLayer::PageUnit>(inspectorPageUnit()));
        d_spin_page_w_mm->setProperty("_mmValue", d_spin_page_w_mm->value() * unitF);
        d_spin_page_h_mm->setProperty("_mmValue", d_spin_page_h_mm->value() * unitF);
        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->setPageSizePhysical(d_spin_page_w_mm->value(), d_spin_page_h_mm->value(),
                                         static_cast<MultiLayer::PageUnit>(inspectorPageUnit()),
                                         d_spin_page_dpi->value());
        d_target_ml->pushUndoSnapshot(tr("Resize Page"), before, kMergePageDims);
    };
    connect(d_combo_page_unit, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, applyPageSize](int) {
                if (d_syncing_props)
                    return;
                const int newUnit = inspectorPageUnit();
                // Keep the physical size: convert the shown values to the new unit.
                const double wMm = d_spin_page_w_mm->property("_mmValue").toDouble();
                const double hMm = d_spin_page_h_mm->property("_mmValue").toDouble();
                configurePageSizeSpins(newUnit, wMm, hMm);
                applyPageSize();
            });
    connect(d_spin_page_w_mm, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [applyPageSize](double) { applyPageSize(); });
    connect(d_spin_page_h_mm, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [applyPageSize](double) { applyPageSize(); });
    connect(d_spin_page_dpi, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [applyPageSize](int) { applyPageSize(); });

    d_chk_page_view = new QCheckBox(page);
    connect(d_chk_page_view, &QCheckBox::toggled, this, [this](bool checked) {
        if (d_syncing_props || !d_target_ml)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->setPageViewMode(checked);
        d_target_ml->pushUndoSnapshot(tr("Toggle Page View"), before);
        if (d_app)
            d_app->syncGraphPageAndToolbarMenuState();
    });
    sizeGrid->addWidget(d_chk_page_view, 4, 0, 1, 2);
    lay->addLayout(sizeGrid);

    // 2b) Inset Layer Controls (B5-8: visible only when active layer is an inset)
    d_inset_wrap = new QWidget(page);
    QVBoxLayout *insetLay = new QVBoxLayout(d_inset_wrap);
    insetLay->setContentsMargins(0, 2, 0, 0);
    insetLay->setSpacing(4);
    d_sec_inset_lbl = makeSectionHeaderLabel(d_inset_wrap);
    insetLay->addWidget(d_sec_inset_lbl);

    QGridLayout *insetGrid = new QGridLayout();
    insetGrid->setContentsMargins(0, 0, 0, 0);
    insetGrid->setHorizontalSpacing(4);
    insetGrid->setVerticalSpacing(4);
    d_lbl_inset_xmin = new QLabel(d_inset_wrap);
    d_edit_inset_xmin = new QLineEdit(d_inset_wrap);
    d_lbl_inset_xmax = new QLabel(d_inset_wrap);
    d_edit_inset_xmax = new QLineEdit(d_inset_wrap);
    d_lbl_inset_ymin = new QLabel(d_inset_wrap);
    d_edit_inset_ymin = new QLineEdit(d_inset_wrap);
    d_lbl_inset_ymax = new QLabel(d_inset_wrap);
    d_edit_inset_ymax = new QLineEdit(d_inset_wrap);
    for (QLineEdit *ed : { d_edit_inset_xmin, d_edit_inset_xmax, d_edit_inset_ymin, d_edit_inset_ymax }) {
        wirePendingEditCue(ed);
        connect(ed, &QLineEdit::editingFinished, this, [this]() { applyInsetRoiFromUi(true); });
    }
    insetGrid->addWidget(d_lbl_inset_xmin, 0, 0);
    insetGrid->addWidget(d_edit_inset_xmin, 0, 1);
    insetGrid->addWidget(d_lbl_inset_xmax, 1, 0);
    insetGrid->addWidget(d_edit_inset_xmax, 1, 1);
    insetGrid->addWidget(d_lbl_inset_ymin, 2, 0);
    insetGrid->addWidget(d_edit_inset_ymin, 2, 1);
    insetGrid->addWidget(d_lbl_inset_ymax, 3, 0);
    insetGrid->addWidget(d_edit_inset_ymax, 3, 1);
    insetLay->addLayout(insetGrid);

    d_chk_inset_connectors = new QCheckBox(d_inset_wrap);
    connect(d_chk_inset_connectors, &QCheckBox::toggled, this, [this](bool on) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g || !d_target_ml->isInsetLayer(g))
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->setInsetShowConnectors(g, on);
        d_target_ml->pushUndoSnapshot(tr("Toggle Inset Connectors"), before);
    });
    insetLay->addWidget(d_chk_inset_connectors);
    d_inset_wrap->hide();
    lay->addWidget(d_inset_wrap);

    // 3) Frame, Linked Axes & Panel Labels
    d_sec_layer_frame_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_layer_frame_lbl);

    d_btn_box_inward = new QPushButton(page);
    d_btn_box_inward->setIcon(IconLoader::duotoneIcon(QStringLiteral("frame-box"), 15));
    connect(d_btn_box_inward, &QPushButton::clicked, this, [this]() {
        const QVector<Graph *> targets = targetLayersForScope();
        if (!d_target_ml || targets.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (Graph *lg : targets) {
            if (lg)
                lg->applyFramePreset(Graph::FrameBoxInwardTicks);
        }
        d_target_ml->pushUndoSnapshot(tr("Box Frame + Inward Ticks"), before);
        syncAxisControls();
    });
    lay->addWidget(d_btn_box_inward);

    d_btn_link_right_y = new QPushButton(page);
    d_btn_link_right_y->setIcon(IconLoader::duotoneIcon(QStringLiteral("plot-double-y"), 15));
    connect(d_btn_link_right_y, &QPushButton::clicked, this, [this]() {
        // B1-5: Enable the primary layer's yRight cleanly instead of spawning an overlapping
        // linked right-Y overlay layer (unless a linked right-Y overlay already exists, in which
        // case focus that existing layer without duplicating yRight).
        Graph *parentLayer = activeLayer();
        if (!d_target_ml || !parentLayer)
            return;
        if (parentLayer->linkedParentLayer())
            parentLayer = parentLayer->linkedParentLayer();

        Graph *existingRightOverlay = nullptr;
        for (Graph *cand : d_target_ml->layersList()) {
            if (!cand || cand->linkedParentLayer() != parentLayer || !cand->plotWidget())
                continue;
            Plot *cp = cand->plotWidget();
            const QwtScaleDraw *sdRight = cp->axisScaleDraw(QwtPlot::yRight);
            const bool rightHasLabels =
                    sdRight && sdRight->hasComponent(QwtAbstractScaleDraw::Labels);
            if (cp->axisEnabled(QwtPlot::yRight) && rightHasLabels) {
                existingRightOverlay = cand;
                break;
            }
        }

        if (existingRightOverlay) {
            d_target_ml->setActiveGraph(existingRightOverlay);
            d_active_axis = QwtPlot::yRight;
            switchToInspectorPage(PageTabAxis);
            refreshAll();
            return;
        }

        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->setActiveGraph(parentLayer);
        parentLayer->enableAxis(QwtPlot::yRight, true);
        parentLayer->setAxisBackboneVisible(QwtPlot::yRight, true);
        if (Plot *pp = parentLayer->plotWidget()) {
            if (QwtScaleDraw *sdRight = pp->axisScaleDraw(QwtPlot::yRight))
                sdRight->enableComponent(QwtAbstractScaleDraw::Labels, true);
            pp->setMajorTicksType(QwtPlot::yRight, ScaleDraw::In);
            pp->setMinorTicksType(QwtPlot::yRight, ScaleDraw::In);
        }
        parentLayer->replot();
        emit parentLayer->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Enable Right-Y Axis"), before);
        d_active_axis = QwtPlot::yRight;
        switchToInspectorPage(PageTabAxis);
        refreshAll();
    });
    lay->addWidget(d_btn_link_right_y);

    d_btn_link_top_x = new QToolButton(page);
    d_btn_link_top_x->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    d_btn_link_top_x->setPopupMode(QToolButton::InstantPopup);
    d_btn_link_top_x->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_link_top_x->setIcon(IconLoader::duotoneIcon(QStringLiteral("axis-scale"), 15));
    d_menu_link_top_x = new QMenu(d_btn_link_top_x);
    d_btn_link_top_x->setMenu(d_menu_link_top_x);

    auto addTopXFormulaLink = [this](const QString &formula, const QString &defaultTitle) {
        Graph *parentLayer = activeLayer();
        const QString trimmedFormula = formula.trimmed();
        if (!d_target_ml || !parentLayer || trimmedFormula.isEmpty())
            return;
        if (parentLayer->linkedParentLayer())
            parentLayer = parentLayer->linkedParentLayer();
        const QString before = d_target_ml->captureUndoSnapshot();

        QString normExpr = trimmedFormula.toLower();
        normExpr.remove(QLatin1Char(' '));
        const bool singularAtZero = normExpr.contains(QStringLiteral("/x"))
                || normExpr.contains(QStringLiteral("/(x)"));
        const bool singularAtAbsZeroC = normExpr.contains(QStringLiteral("/(x+273.15)"));

        if (Plot *pp = parentLayer->plotWidget()) {
            if (singularAtZero || singularAtAbsZeroC) {
                const double pole = singularAtZero ? 0.0 : -273.15;
                if (const QwtScaleDiv *xDiv = pp->axisScaleDiv(QwtPlot::xBottom)) {
                    const double rawLo = xDiv->lowerBound();
                    const double rawHi = xDiv->upperBound();
                    const double lo = qMin(rawLo, rawHi);
                    const double hi = qMax(rawLo, rawHi);
                    if (lo <= pole && hi > pole) {
                        const double spanAbovePole = hi - pole;
                        double clampedLo = pole
                                + ((spanAbovePole >= 100.0) ? qMax(1.0, spanAbovePole * 0.05)
                                                            : (0.1 * spanAbovePole));
                        double minDataAbovePole = std::numeric_limits<double>::infinity();
                        for (int ci = 0; ci < parentLayer->curves(); ++ci) {
                            if (parentLayer->curveType(ci) == Graph::ErrorBars)
                                continue;
                            QwtPlotCurve *cv = parentLayer->curve(ci);
                            if (!cv)
                                continue;
                            for (int p = 0; p < cv->dataSize(); ++p) {
                                const double xv = cv->x(p);
                                if (std::isfinite(xv) && xv > pole && xv < minDataAbovePole)
                                    minDataAbovePole = xv;
                            }
                        }
                        if (std::isfinite(minDataAbovePole) && minDataAbovePole < clampedLo) {
                            clampedLo = pole + 0.9 * (minDataAbovePole - pole);
                        }
                        int scaleType = 0;
                        bool inv = (rawLo > rawHi);
                        if (const QwtScaleEngine *eng = pp->axisScaleEngine(QwtPlot::xBottom)) {
                            inv = eng->testAttribute(QwtScaleEngine::Inverted);
                            if (QwtScaleTransformation *xf = eng->transformation()) {
                                if (xf->type() == QwtScaleTransformation::Log10)
                                    scaleType = 1;
                                delete xf;
                            }
                        }
                        parentLayer->enableAutoscaling(false);
                        parentLayer->setScale(QwtPlot::xBottom, clampedLo, hi,
                                              parentLayer->axisStep(QwtPlot::xBottom),
                                              pp->axisMaxMajor(QwtPlot::xBottom),
                                              pp->axisMaxMinor(QwtPlot::xBottom), scaleType, inv);
                    }
                }
            }
            if (QwtScaleDraw *psdTop = pp->axisScaleDraw(QwtPlot::xTop))
                psdTop->enableComponent(QwtAbstractScaleDraw::Labels, false);
            pp->setMajorTicksType(QwtPlot::xTop, ScaleDraw::None);
            pp->setMinorTicksType(QwtPlot::xTop, ScaleDraw::None);
        }

        Graph *existingChild = nullptr;
        for (Graph *cand : d_target_ml->layersList()) {
            if (!cand || cand->linkedParentLayer() != parentLayer || !cand->plotWidget())
                continue;
            Plot *cp = cand->plotWidget();
            const QwtScaleDraw *sdTop = cp->axisScaleDraw(QwtPlot::xTop);
            const QwtScaleDraw *sdRight = cp->axisScaleDraw(QwtPlot::yRight);
            const bool topHasLabels = sdTop && sdTop->hasComponent(QwtAbstractScaleDraw::Labels);
            const bool rightHasLabels =
                    sdRight && sdRight->hasComponent(QwtAbstractScaleDraw::Labels);
            if (cp->axisEnabled(QwtPlot::xTop)
                && (!cp->axisEnabled(QwtPlot::yRight) || topHasLabels || !rightHasLabels
                    || cand->xLinkMode() == 2)) {
                existingChild = cand;
                break;
            }
        }

        Graph *child = existingChild;
        const int xMode =
                (trimmedFormula.compare(QStringLiteral("x"), Qt::CaseInsensitive) == 0
                 || trimmedFormula.compare(QStringLiteral("x1"), Qt::CaseInsensitive) == 0)
                ? 1
                : 2;
        if (child) {
            const int yMode = (child->yLinkMode() > 0) ? child->yLinkMode() : 1;
            const QString yExpr = child->yLinkExpr().trimmed().isEmpty()
                    ? QStringLiteral("y")
                    : child->yLinkExpr();
            child->setLinkedParentLayer(parentLayer, xMode, trimmedFormula, yMode, yExpr);
            d_target_ml->setActiveGraph(child);
        } else {
            child = d_target_ml->addLinkedLayer(parentLayer, true, true, trimmedFormula,
                                                QStringLiteral("y"));
        }
        if (child && child->plotWidget()) {
            Plot *cp = child->plotWidget();
            if (QwtScaleDraw *sdTop = cp->axisScaleDraw(QwtPlot::xTop))
                sdTop->enableComponent(QwtAbstractScaleDraw::Labels, true);
            if (QwtScaleDraw *sdRight = cp->axisScaleDraw(QwtPlot::yRight))
                sdRight->enableComponent(QwtAbstractScaleDraw::Labels, false);
            cp->setMajorTicksType(QwtPlot::xTop, ScaleDraw::In);
            cp->setMinorTicksType(QwtPlot::xTop, ScaleDraw::In);
            cp->setMajorTicksType(QwtPlot::yRight, ScaleDraw::None);
            cp->setMinorTicksType(QwtPlot::yRight, ScaleDraw::None);
            if (!defaultTitle.isEmpty()) {
                QFont tf = parentLayer->axisTitleFont(QwtPlot::xBottom);
                if (tf.family().trimmed().isEmpty())
                    tf.setFamily(QStringLiteral("Arial"));
                tf.setBold(false);
                child->setAxisTitleFont(QwtPlot::xTop, tf);
                child->setAxisTitleAuto(QwtPlot::xTop, false);
                child->setTopAxisTitle(defaultTitle);
            } else if (xMode == 1) {
                child->setTopAxisTitle(QString());
            }
            child->syncLinkedAxesFromParent();
            double targetAspect = d_target_ml->targetAspectRatio();
            if (!d_target_ml->keepAspectRatio() || targetAspect <= 0.05) {
                const QSize cSz = (parentLayer->plotWidget() && parentLayer->plotWidget()->canvas())
                        ? parentLayer->plotWidget()->canvas()->size()
                        : QSize();
                if (cSz.width() > 20 && cSz.height() > 20) {
                    targetAspect = double(cSz.width()) / double(cSz.height());
                } else {
                    const double cellW =
                            d_target_ml->pageWidthPhysical() / qMax(1, d_target_ml->getCols());
                    const double cellH =
                            d_target_ml->pageHeightPhysical() / qMax(1, d_target_ml->getRows());
                    if (cellH > 1e-6)
                        targetAspect = cellW / cellH;
                }
            }
            if (d_target_ml->keepAspectRatio() && qAbs(targetAspect - 1.0) < 0.05) {
                d_target_ml->applySquareCanvasFrame70Percent();
            } else if (d_target_ml->keepAspectRatio()) {
                d_target_ml->fitLayersToPage();
            } else {
                cp->updateLayout();
                d_target_ml->syncChildOverlayAndInsetGeometries();
            }
        }
        d_target_ml->pushUndoSnapshot(tr("Link Top-X Axis"), before);
        refreshAll();
    };

    connect(d_menu_link_top_x, &QMenu::aboutToShow, this, [this, addTopXFormulaLink]() {
        d_menu_link_top_x->clear();
        d_menu_link_top_x->addAction(tr("1:1 Linear Link (x)"), this, [addTopXFormulaLink]() {
            addTopXFormulaLink(QStringLiteral("x"), QString());
        });
        d_menu_link_top_x->addAction(
                tr("Arrhenius: K -> 1000/T"), this, [addTopXFormulaLink]() {
                    addTopXFormulaLink(QStringLiteral("1000.0/x"),
                                       QStringLiteral("1000 / <i>T</i> (K<sup>-1</sup>)"));
                });
        d_menu_link_top_x->addAction(
                tr("Arrhenius: °C -> 1000/T"), this, [addTopXFormulaLink]() {
                    addTopXFormulaLink(QStringLiteral("1000.0/(x+273.15)"),
                                       QStringLiteral("1000 / <i>T</i> (K<sup>-1</sup>)"));
                });
        d_menu_link_top_x->addAction(
                tr("Inverse Arrhenius: 1000/T -> T (K)"), this, [this, addTopXFormulaLink]() {
                    addTopXFormulaLink(QStringLiteral("1000.0/x"),
                                       tr("Temperature (K)"));
                });
        d_menu_link_top_x->addAction(
                tr("Inverse Arrhenius: 1000/T -> °C"), this, [this, addTopXFormulaLink]() {
                    addTopXFormulaLink(QStringLiteral("(1000.0/x)-273.15"),
                                       tr("Temperature (°C)"));
                });
        d_menu_link_top_x->addSeparator();
        d_menu_link_top_x->addAction(tr("Custom Formula..."), this, [this, addTopXFormulaLink]() {
            bool ok = false;
            const QString expr = QInputDialog::getText(
                    this, tr("Link Top-X Formula"),
                    tr("Formula in terms of bottom X (variable 'x'):"), QLineEdit::Normal,
                    QStringLiteral("1000.0/x"), &ok);
            if (ok && !expr.trimmed().isEmpty())
                addTopXFormulaLink(expr.trimmed(), QString());
        });
    });
    lay->addWidget(d_btn_link_top_x);

    QHBoxLayout *panelRow = new QHBoxLayout();
    panelRow->setSpacing(4);
    d_chk_panel_labels = new QCheckBox(page);
    d_combo_panel_pos = new QComboBox(page);
    d_combo_panel_pos->addItem(QString(), int(MultiLayer::PanelOutsideTopLeft));
    d_combo_panel_pos->addItem(QString(), int(MultiLayer::PanelInsideTopLeft));
    panelRow->addWidget(d_chk_panel_labels, 1);
    panelRow->addWidget(d_combo_panel_pos);
    lay->addLayout(panelRow);

    auto applyPanelLabelsFromUi = [this]() {
        if (d_syncing_props || !d_target_ml)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        const auto pos = static_cast<MultiLayer::PanelLabelPlacement>(
                d_combo_panel_pos->currentData().toInt());
        d_target_ml->setPanelLabels(d_chk_panel_labels->isChecked(), pos,
                                    d_target_ml->panelLabelPattern(),
                                    d_target_ml->panelLabelFontPt());
        d_target_ml->pushUndoSnapshot(tr("Panel Labels (a), (b)"), before);
    };
    connect(d_chk_panel_labels, &QCheckBox::toggled, this,
            [applyPanelLabelsFromUi](bool) { applyPanelLabelsFromUi(); });
    connect(d_combo_panel_pos, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [applyPanelLabelsFromUi](int) { applyPanelLabelsFromUi(); });

    lay->addStretch(1);
    return page;
}

QWidget *GraphInspectorDock::buildAxisInspector()
{
    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 12, 10);
    lay->setSpacing(6);

    // Axis Selector: 2x2 grid ([Bottom X][Left Y] / [Top X][Right Y]) so labels never clip at 252px (A4-5)
    QFrame *axisTrack = new QFrame(page);
    axisTrack->setObjectName(QStringLiteral("InspectorSegmentedTrack"));
    QGridLayout *axisSelGrid = new QGridLayout(axisTrack);
    axisSelGrid->setContentsMargins(2, 2, 2, 2);
    axisSelGrid->setHorizontalSpacing(2);
    axisSelGrid->setVerticalSpacing(2);
    d_axis_sel_group = new QButtonGroup(this);
    d_axis_sel_group->setExclusive(true);

    auto makeAxisBtn = [&](int qwtAxis, int row, int col) -> QToolButton * {
        QToolButton *b = new QToolButton(axisTrack);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setFixedHeight(24);
        b->setObjectName(QStringLiteral("InspectorAxisPillBtn"));
        d_axis_sel_group->addButton(b, qwtAxis);
        axisSelGrid->addWidget(b, row, col);
        return b;
    };

    d_btn_axis_bottom = makeAxisBtn(QwtPlot::xBottom, 0, 0);
    d_btn_axis_left = makeAxisBtn(QwtPlot::yLeft, 0, 1);
    d_btn_axis_top = makeAxisBtn(QwtPlot::xTop, 1, 0);
    d_btn_axis_right = makeAxisBtn(QwtPlot::yRight, 1, 1);
    d_btn_axis_bottom->setChecked(true);

    connect(d_axis_sel_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this,
            [this](int qwtAxis) {
                if (d_axis_title_timer && d_axis_title_timer->isActive())
                    flushPendingAxisTitleEdit();
                d_active_axis = qBound(0, qwtAxis, 3);
                if (Graph *g = activeLayer()) {
                    if (GraphMiniToolbar *mt = GraphMiniToolbar::instance())
                        mt->cancelScheduledAndHide();
                    Graph::GraphSelection sel;
                    sel.kind = Graph::GraphSelection::Axis;
                    sel.primaryId = d_active_axis;
                    g->setCurrentSelection(sel, false);
                }
                syncAxisControls();
            });
    lay->addWidget(axisTrack);

    d_chk_axis_visible = new QCheckBox(page);
    connect(d_chk_axis_visible, &QCheckBox::toggled, this, [this](bool vis) {
        const QVector<Graph *> targets = targetLayersForScope();
        if (d_syncing_props || !d_target_ml || targets.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (Graph *lg : targets) {
            if (!lg)
                continue;
            lg->enableAxis(d_active_axis, vis);
            lg->setAxisBackboneVisible(d_active_axis, vis);
            lg->replot();
        }
        d_target_ml->pushUndoSnapshot(tr("Toggle Axis Visibility"), before);
        syncAxisControls();
    });

    d_chk_tick_labels = new QCheckBox(page);
    connect(d_chk_tick_labels, &QCheckBox::toggled, this, [this](bool on) {
        const QVector<Graph *> targets = targetLayersForScope();
        if (d_syncing_props || !d_target_ml || targets.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (Graph *lg : targets) {
            if (!lg)
                continue;
            lg->setTickLabelsOnAxis(d_active_axis, on);
            lg->replot();
            emit lg->modifiedGraph();
        }
        d_target_ml->pushUndoSnapshot(tr("Toggle Tick Numbers"), before);
        syncAxisControls();
    });

    QHBoxLayout *visRow = new QHBoxLayout();
    visRow->setContentsMargins(0, 0, 0, 0);
    visRow->setSpacing(8);
    visRow->addWidget(d_chk_axis_visible);
    visRow->addWidget(d_chk_tick_labels);
    visRow->addStretch(1);
    lay->addLayout(visRow);

    // Axis Title & Scientific Rich-Text Toolbar + Quick Materials Symbol Chips (kept at top)
    d_sec_axis_title_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_axis_title_lbl);

    d_edit_axis_title = new QLineEdit(page);
    d_axis_title_timer = new QTimer(this);
    d_axis_title_timer->setSingleShot(true);
    d_axis_title_timer->setInterval(300);
    connect(d_axis_title_timer, &QTimer::timeout, this,
            &GraphInspectorDock::flushPendingAxisTitleEdit);
    connect(d_edit_axis_title, &QLineEdit::textEdited, this, [this](const QString &txt) {
        updateAxisTitlePreview(txt);
        if (d_syncing_props)
            return;
        Graph *g = activeLayer();
        if (!g)
            return;
        if (!d_axis_title_timer->isActive()) {
            d_axis_title_edit_layer = g;
            d_axis_title_edit_axis = d_active_axis;
        }
        d_axis_title_timer->start();
    });
    connect(d_edit_axis_title, &QLineEdit::editingFinished, this, [this]() {
        if (d_syncing_props)
            return;
        if (d_axis_title_timer && d_axis_title_timer->isActive()) {
            flushPendingAxisTitleEdit();
            return;
        }
        if (!d_edit_axis_title->isModified()
            && d_edit_axis_title->text() == d_edit_axis_title->property("_syncedText").toString()) {
            return;
        }
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        d_axis_title_edit_layer = g;
        d_axis_title_edit_axis = d_active_axis;
        flushPendingAxisTitleEdit();
    });
    lay->addWidget(d_edit_axis_title);

    // Rich-text live preview label directly below d_edit_axis_title (claude_review_1 #4, A3-3)
    d_lbl_axis_title_preview = new QLabel(page);
    d_lbl_axis_title_preview->setObjectName(QStringLiteral("InspectorAxisTitlePreview"));
    d_lbl_axis_title_preview->setTextFormat(Qt::RichText);
    d_lbl_axis_title_preview->setWordWrap(true);
    lay->addWidget(d_lbl_axis_title_preview);

    d_axis_sym_bar = buildRichTextBar(page, RichTargetMode::AxisTitle);
    lay->addWidget(d_axis_sym_bar);

    // Range & Scale
    d_sec_axis_range_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_axis_range_lbl);

    QGridLayout *rngGrid = new QGridLayout();
    rngGrid->setContentsMargins(0, 0, 0, 0);
    rngGrid->setHorizontalSpacing(6);
    rngGrid->setVerticalSpacing(4);

    d_lbl_axis_min = new QLabel(page);
    d_edit_axis_min = new QLineEdit(page);
    d_lbl_axis_max = new QLabel(page);
    d_edit_axis_max = new QLineEdit(page);
    d_lbl_axis_step = new QLabel(page);
    d_edit_axis_step = new QLineEdit(page);
    wirePendingEditCue(d_edit_axis_min);
    wirePendingEditCue(d_edit_axis_max);
    wirePendingEditCue(d_edit_axis_step);

    rngGrid->addWidget(d_lbl_axis_min, 0, 0);
    rngGrid->addWidget(d_edit_axis_min, 0, 1);
    rngGrid->addWidget(d_lbl_axis_max, 1, 0);
    rngGrid->addWidget(d_edit_axis_max, 1, 1);
    rngGrid->addWidget(d_lbl_axis_step, 2, 0);
    rngGrid->addWidget(d_edit_axis_step, 2, 1);

    connect(d_edit_axis_min, &QLineEdit::editingFinished, this,
            [this]() { applyAxisScaleFromUi(kMergeAxisScale, true); });
    connect(d_edit_axis_max, &QLineEdit::editingFinished, this,
            [this]() { applyAxisScaleFromUi(kMergeAxisScale, true); });
    connect(d_edit_axis_step, &QLineEdit::editingFinished, this,
            [this]() { applyAxisScaleFromUi(kMergeAxisScale, true); });

    d_btn_axis_autoscale = new QPushButton(page);
    d_btn_axis_autoscale->setMinimumHeight(28);
    d_btn_axis_autoscale->setIcon(IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 14));
    connect(d_btn_axis_autoscale, &QPushButton::clicked, this, [this]() {
        const QVector<Graph *> targets = targetLayersForScope();
        if (!d_target_ml || targets.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (Graph *lg : targets) {
            if (!lg)
                continue;
            lg->setAxisUserLocked(d_active_axis, false);
            lg->setAutoScaleAxis(d_active_axis);
            syncLinkedChildrenOf(d_target_ml, lg);
        }
        d_target_ml->pushUndoSnapshot(tr("Auto Range Axis"), before);
        syncAxisControls();
    });
    rngGrid->addWidget(d_btn_axis_autoscale, 3, 0, 1, 2);

    d_lbl_axis_scale_type = new QLabel(page);
    d_combo_axis_scale = new QComboBox(page);
    d_combo_axis_scale->addItem(QString(), 0); // Linear
    d_combo_axis_scale->addItem(QString(), 1); // Log10
    connect(d_combo_axis_scale, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (!d_syncing_props)
                    applyAxisScaleFromUi(-1);
            });
    rngGrid->addWidget(d_lbl_axis_scale_type, 4, 0);
    rngGrid->addWidget(d_combo_axis_scale, 4, 1);

    d_lbl_minor_subdiv = new QLabel(page);
    d_combo_minor_subdiv = new QComboBox(page);
    // C1: the choices follow the axis type (linear 0/1/4/9, log 0/2/4/8 as in AxesDialog) and
    // any other number can be typed in.
    d_combo_minor_subdiv->setObjectName(QStringLiteral("InspectorMinorTicksCombo"));
    d_combo_minor_subdiv->setEditable(true);
    d_combo_minor_subdiv->setInsertPolicy(QComboBox::NoInsert);
    d_combo_minor_subdiv->lineEdit()->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("^\\s*\\d{1,3}(\\s.*)?$")), d_combo_minor_subdiv));
    fillMinorTickCombo(d_combo_minor_subdiv, false, 0);
    connect(d_combo_minor_subdiv, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (!d_syncing_props)
                    applyAxisScaleFromUi(-1);
            });
    connect(d_combo_minor_subdiv->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
        if (d_syncing_props || !d_combo_minor_subdiv)
            return;
        Graph *g = activeLayer();
        if (!g || !g->plotWidget())
            return;
        const int typed = minorTickComboValue(d_combo_minor_subdiv, -1);
        if (typed >= 0 && typed != g->plotWidget()->axisMaxMinor(d_active_axis))
            applyAxisScaleFromUi(-1);
    });
    rngGrid->addWidget(d_lbl_minor_subdiv, 5, 0);
    rngGrid->addWidget(d_combo_minor_subdiv, 5, 1);

    d_chk_axis_reverse = new QCheckBox(page);
    connect(d_chk_axis_reverse, &QCheckBox::toggled, this, [this](bool) {
        if (!d_syncing_props)
            applyAxisScaleFromUi(-1);
    });
    rngGrid->addWidget(d_chk_axis_reverse, 6, 0, 1, 2);
    lay->addLayout(rngGrid);

    // Axis Break (Start/End fields collapsed when unchecked)
    d_chk_axis_break = new QCheckBox(page);
    lay->addWidget(d_chk_axis_break);

    QGridLayout *brkGrid = new QGridLayout();
    brkGrid->setContentsMargins(0, 0, 0, 0);
    brkGrid->setHorizontalSpacing(6);
    brkGrid->setVerticalSpacing(4);
    d_lbl_break_from = new QLabel(page);
    d_edit_break_from = new QLineEdit(page);
    d_lbl_break_to = new QLabel(page);
    d_edit_break_to = new QLineEdit(page);
    wirePendingEditCue(d_edit_break_from);
    wirePendingEditCue(d_edit_break_to);
    d_lbl_break_scale_before = new QLabel(page);
    d_combo_break_scale_before = new QComboBox(page);
    d_combo_break_scale_before->addItem(QString(), 0); // Linear
    d_combo_break_scale_before->addItem(QString(), 1); // Log10
    d_lbl_break_scale_after = new QLabel(page);
    d_combo_break_scale_after = new QComboBox(page);
    d_combo_break_scale_after->addItem(QString(), 0); // Linear
    d_combo_break_scale_after->addItem(QString(), 1); // Log10
    d_lbl_break_from->setVisible(false);
    d_edit_break_from->setVisible(false);
    d_lbl_break_to->setVisible(false);
    d_edit_break_to->setVisible(false);
    d_lbl_break_scale_before->setVisible(false);
    d_combo_break_scale_before->setVisible(false);
    d_lbl_break_scale_after->setVisible(false);
    d_combo_break_scale_after->setVisible(false);
    brkGrid->addWidget(d_lbl_break_from, 0, 0);
    brkGrid->addWidget(d_edit_break_from, 0, 1);
    brkGrid->addWidget(d_lbl_break_to, 1, 0);
    brkGrid->addWidget(d_edit_break_to, 1, 1);
    brkGrid->addWidget(d_lbl_break_scale_before, 2, 0);
    brkGrid->addWidget(d_combo_break_scale_before, 2, 1);
    brkGrid->addWidget(d_lbl_break_scale_after, 3, 0);
    brkGrid->addWidget(d_combo_break_scale_after, 3, 1);
    lay->addLayout(brkGrid);

    connect(d_chk_axis_break, &QCheckBox::toggled, this, [this](bool on) {
        d_lbl_break_from->setVisible(on);
        d_edit_break_from->setVisible(on);
        d_lbl_break_to->setVisible(on);
        d_edit_break_to->setVisible(on);
        d_lbl_break_scale_before->setVisible(on);
        d_combo_break_scale_before->setVisible(on);
        d_lbl_break_scale_after->setVisible(on);
        d_combo_break_scale_after->setVisible(on);
        d_edit_break_from->setEnabled(on);
        d_edit_break_to->setEnabled(on);
        d_combo_break_scale_before->setEnabled(on);
        d_combo_break_scale_after->setEnabled(on);
        if (!d_syncing_props)
            applyAxisBreakFromUi(false);
    });
    connect(d_edit_break_from, &QLineEdit::editingFinished, this,
            [this]() { applyAxisBreakFromUi(true); });
    connect(d_edit_break_to, &QLineEdit::editingFinished, this,
            [this]() { applyAxisBreakFromUi(true); });
    connect(d_combo_break_scale_before, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (!d_syncing_props)
                    applyAxisBreakFromUi(false);
            });
    connect(d_combo_break_scale_after, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (!d_syncing_props)
                    applyAxisBreakFromUi(false);
            });

    // Reference Lines (y = 0, x = 0, + Add Reference Line...)
    d_sec_ref_lines_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_ref_lines_lbl);

    QHBoxLayout *refRow = new QHBoxLayout();
    refRow->setContentsMargins(0, 0, 0, 0);
    refRow->setSpacing(4);
    d_btn_ref_zero_y = new QPushButton(QStringLiteral("y = 0"), page);
    d_btn_ref_zero_y->setCheckable(true);
    d_btn_ref_zero_y->setCursor(Qt::PointingHandCursor);
    connect(d_btn_ref_zero_y, &QPushButton::clicked, this, [this]() {
        if (Graph *g = activeLayer()) {
            toggleZeroReferenceLine(g, 0);
            refreshAll();
        }
    });
    d_btn_ref_zero_x = new QPushButton(QStringLiteral("x = 0"), page);
    d_btn_ref_zero_x->setCheckable(true);
    d_btn_ref_zero_x->setCursor(Qt::PointingHandCursor);
    connect(d_btn_ref_zero_x, &QPushButton::clicked, this, [this]() {
        if (Graph *g = activeLayer()) {
            toggleZeroReferenceLine(g, 1);
            refreshAll();
        }
    });
    d_btn_ref_add_custom = new QPushButton(page);
    d_btn_ref_add_custom->setIcon(IconLoader::duotoneIcon(QStringLiteral("curve-add"), 14));
    d_btn_ref_add_custom->setCursor(Qt::PointingHandCursor);
    connect(d_btn_ref_add_custom, &QPushButton::clicked, this, [this]() {
        if (Graph *g = activeLayer()) {
            const int defOrient =
                    (d_active_axis == QwtPlot::yLeft || d_active_axis == QwtPlot::yRight) ? 0 : 1;
            addReferenceLineInteractive(g, defOrient);
            refreshAll();
        }
    });
    refRow->addWidget(d_btn_ref_zero_y, 1);
    refRow->addWidget(d_btn_ref_zero_x, 1);
    refRow->addWidget(d_btn_ref_add_custom, 1);
    lay->addLayout(refRow);

    // Ticks, Notation & Grids
    d_sec_axis_ticks_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_axis_ticks_lbl);

    QGridLayout *tickGrid = new QGridLayout();
    tickGrid->setContentsMargins(0, 0, 0, 0);
    tickGrid->setHorizontalSpacing(6);
    tickGrid->setVerticalSpacing(4);

    d_lbl_tick_dir = new QLabel(page);
    d_combo_tick_dir = new QComboBox(page);
    d_combo_tick_dir->addItem(QString(), int(ScaleDraw::In));
    d_combo_tick_dir->addItem(QString(), int(ScaleDraw::Out));
    d_combo_tick_dir->addItem(QString(), int(ScaleDraw::Both));
    d_combo_tick_dir->addItem(QString(), int(ScaleDraw::None));
    connect(d_combo_tick_dir, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                const QVector<Graph *> targets = targetLayersForScope();
                if (d_syncing_props || !d_target_ml || targets.isEmpty() || idx < 0)
                    return;
                const int style = d_combo_tick_dir->itemData(idx).toInt();
                const QString before = d_target_ml->captureUndoSnapshot();
                for (Graph *lg : targets) {
                    if (!lg)
                        continue;
                    lg->setAxisTicksLength(d_active_axis, style, style, lg->minorTickLength(),
                                           lg->majorTickLength());
                    lg->replot();
                }
                d_target_ml->pushUndoSnapshot(tr("Change Tick Direction"), before);
            });
    tickGrid->addWidget(d_lbl_tick_dir, 0, 0);
    tickGrid->addWidget(d_combo_tick_dir, 0, 1);

    d_lbl_tick_format = new QLabel(page);
    d_combo_tick_format = new QComboBox(page);
    d_combo_tick_format->addItem(QString(), int(Plot::Automatic));    // 0: Automatic
    d_combo_tick_format->addItem(QString(), int(Plot::Decimal));      // 1: Decimal (0.00)
    d_combo_tick_format->addItem(QString(), int(Plot::Superscripts)); // 3: Scientific (10^x)
    d_combo_tick_format->addItem(QString(), int(Plot::Scientific));   // 2: Scientific (1E3)
    d_combo_tick_format->addItem(QString(), 4);                       // 4: Engineering
    connect(d_combo_tick_format, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (!d_syncing_props)
                    applyTickFormatFromUi(-1);
            });
    tickGrid->addWidget(d_lbl_tick_format, 1, 0);
    tickGrid->addWidget(d_combo_tick_format, 1, 1);

    d_lbl_tick_prec = new QLabel(page);
    d_spin_tick_prec = new QSpinBox(page);
    d_spin_tick_prec->setRange(-1, 9);
    d_spin_tick_prec->setValue(-1);
    connect(d_spin_tick_prec, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!d_syncing_props)
            applyTickFormatFromUi(kMergeTickPrec);
    });
    tickGrid->addWidget(d_lbl_tick_prec, 2, 0);
    tickGrid->addWidget(d_spin_tick_prec, 2, 1);

    d_lbl_tick_divide = new QLabel(page);
    d_combo_tick_divide = new QComboBox(page);
    d_combo_tick_divide->setEditable(true);
    d_combo_tick_divide->addItems(QStringList()
                                  << QStringLiteral("1") << QStringLiteral("1000")
                                  << QStringLiteral("1e6") << QStringLiteral("0.01"));
    connect(d_combo_tick_divide, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        if (!d_syncing_props)
            applyTickDivideFromUi(false);
    });
    if (QLineEdit *divEdit = d_combo_tick_divide->lineEdit()) {
        wirePendingEditCue(divEdit);
        connect(divEdit, &QLineEdit::editingFinished, this, [this]() {
            if (!d_syncing_props)
                applyTickDivideFromUi(true);
        });
    }
    tickGrid->addWidget(d_lbl_tick_divide, 3, 0);
    tickGrid->addWidget(d_combo_tick_divide, 3, 1);

    d_lbl_category_col = new QLabel(page);
    d_combo_category_col = new QComboBox(page);
    connect(d_combo_category_col, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        if (!d_syncing_props)
            applyCategoryColumnFromUi();
    });
    tickGrid->addWidget(d_lbl_category_col, 4, 0);
    tickGrid->addWidget(d_combo_category_col, 4, 1);

    d_chk_major_grid = new QCheckBox(page);
    d_chk_minor_grid = new QCheckBox(page);
    auto applyGridToggle = [this]() {
        const QVector<Graph *> targets = targetLayersForScope();
        if (d_syncing_props || !d_target_ml || targets.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        const bool isX = (d_active_axis == QwtPlot::xBottom || d_active_axis == QwtPlot::xTop);
        for (Graph *lg : targets) {
            if (!lg || !lg->grid())
                continue;
            Grid *gr = lg->grid();
            if (isX) {
                gr->enableX(d_chk_major_grid->isChecked());
                gr->enableXMin(d_chk_minor_grid->isChecked());
            } else {
                gr->enableY(d_chk_major_grid->isChecked());
                gr->enableYMin(d_chk_minor_grid->isChecked());
            }
            lg->replot();
            emit lg->modifiedGraph();
        }
        d_target_ml->pushUndoSnapshot(tr("Toggle Axis Grid"), before);
    };
    connect(d_chk_major_grid, &QCheckBox::toggled, this,
            [applyGridToggle](bool) { applyGridToggle(); });
    connect(d_chk_minor_grid, &QCheckBox::toggled, this,
            [applyGridToggle](bool) { applyGridToggle(); });

    tickGrid->addWidget(d_chk_major_grid, 5, 0);
    tickGrid->addWidget(d_chk_minor_grid, 5, 1);
    lay->addLayout(tickGrid);

    // Unit Format & Arial Font Sizes (Axis title & symbol chips are placed at the top)
    {
        QHBoxLayout *ufRow = new QHBoxLayout();
        ufRow->setContentsMargins(0, 0, 0, 0);
        ufRow->setSpacing(6);
        d_lbl_unit_format = new QLabel(page);
        d_combo_unit_format = new QComboBox(page);
        d_combo_unit_format->addItem(QString(), 0); // Name (Unit)
        d_combo_unit_format->addItem(QString(), 1); // Name / Unit (ISIJ)
        d_combo_unit_format->addItem(QString(), 2); // Name [Unit]
        connect(d_combo_unit_format, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int idx) {
                    const QVector<Graph *> targets = targetLayersForScope();
                    if (d_syncing_props || !d_target_ml || targets.isEmpty() || idx < 0)
                        return;
                    const int mode = d_combo_unit_format->itemData(idx).toInt();
                    const QString before = d_target_ml->captureUndoSnapshot();
                    static const QRegularExpression rxParen(
                            QStringLiteral("^(.*\\S)\\s+\\(([^()\\[\\]]+)\\)\\s*$"));
                    static const QRegularExpression rxBracket(
                            QStringLiteral("^(.*\\S)\\s+\\[([^\\[\\]]+)\\]\\s*$"));
                    auto matchSlashOutsideTags = [](const QString &raw, QString &base,
                                                    QString &unit) -> bool {
                        int parenDepth = 0;
                        int bracketDepth = 0;
                        int bestSlash = -1;
                        for (int i = 0; i < raw.size(); ++i) {
                            const QChar c = raw.at(i);
                            if (c == QLatin1Char('<')) {
                                const int closeIdx = raw.indexOf(QLatin1Char('>'), i + 1);
                                if (closeIdx > i + 1) {
                                    const QChar nextCh = raw.at(i + 1);
                                    if (nextCh == QLatin1Char('/') || nextCh.isLetter()) {
                                        i = closeIdx;
                                        continue;
                                    }
                                }
                            }
                            if (c == QLatin1Char('('))
                                ++parenDepth;
                            else if (c == QLatin1Char(')') && parenDepth > 0)
                                --parenDepth;
                            else if (c == QLatin1Char('['))
                                ++bracketDepth;
                            else if (c == QLatin1Char(']') && bracketDepth > 0)
                                --bracketDepth;
                            else if (c == QLatin1Char('/') && parenDepth == 0 && bracketDepth == 0
                                     && i > 0 && raw.at(i - 1).isSpace()
                                     && i + 1 < raw.size() && raw.at(i + 1).isSpace()) {
                                bestSlash = i;
                            }
                        }
                        if (bestSlash <= 0)
                            return false;
                        base = raw.left(bestSlash).trimmed();
                        unit = raw.mid(bestSlash + 1).trimmed();
                        return !base.isEmpty() && !unit.isEmpty();
                    };
                    auto rewriteManualTitle = [&](const QString &raw, int dstMode) -> QString {
                        QString base, unit;
                        QRegularExpressionMatch m = rxParen.match(raw);
                        if (m.hasMatch()) {
                            base = m.captured(1);
                            unit = m.captured(2);
                        } else if ((m = rxBracket.match(raw)).hasMatch()) {
                            base = m.captured(1);
                            unit = m.captured(2);
                        } else if (!matchSlashOutsideTags(raw, base, unit)) {
                            return raw;
                        }
                        const QString uTrim = unit.trimmed();
                        if (dstMode == 1) {
                            if (uTrim == QLatin1String("-")
                                || uTrim == QString::fromUtf8("\xE2\x80\x94")
                                || uTrim.compare(QLatin1String("a.u."), Qt::CaseInsensitive) == 0
                                || uTrim.compare(QLatin1String("arb. units"), Qt::CaseInsensitive)
                                        == 0) {
                                return base + QStringLiteral(" (") + uTrim + QStringLiteral(")");
                            }
                            return base + QStringLiteral(" / ") + uTrim;
                        }
                        if (dstMode == 2)
                            return base + QStringLiteral(" [") + uTrim + QStringLiteral("]");
                        return base + QStringLiteral(" (") + uTrim + QStringLiteral(")");
                    };
                    const Column::UnitFormat targetFmt = (mode == 1)
                            ? Column::UnitFormat::ISIJSlash
                            : (mode == 2 ? Column::UnitFormat::Bracket
                                         : Column::UnitFormat::Paren);
                    for (Graph *lg : targets) {
                        if (!lg)
                            continue;
                        // A3-8 / E1-4: use Column::UnitFormat::Bracket natively without any
                        // dynamic property hack, and also rewrite manually edited titles.
                        lg->setAxisUnitFormat(targetFmt);
                        lg->refreshAutoAxisTitlesFromColumns();
                        for (int a = 0; a < 4; ++a) {
                            if (!lg->isAxisTitleAuto(a)) {
                                const QString cur = lg->axisTitle(a);
                                const QString rew = rewriteManualTitle(cur, mode);
                                if (rew != cur)
                                    lg->setAxisTitle(a, rew);
                            }
                        }
                        lg->replot();
                        emit lg->modifiedGraph();
                    }
                    d_target_ml->pushUndoSnapshot(tr("Change Unit Format"), before);
                    syncAxisControls();
                });
        ufRow->addWidget(d_lbl_unit_format);
        ufRow->addWidget(d_combo_unit_format, 1);
        lay->addLayout(ufRow);
    }

    d_chk_lock_all_axes_font = new QCheckBox(page);
    d_chk_lock_all_axes_font->setChecked(true); // Default checked per OriginPro ergonomics!
    lay->addWidget(d_chk_lock_all_axes_font);

    QGridLayout *fntGrid = new QGridLayout();
    fntGrid->setContentsMargins(0, 0, 0, 0);
    fntGrid->setHorizontalSpacing(6);
    fntGrid->setVerticalSpacing(4);

    d_lbl_title_pt = new QLabel(page);
    d_spin_axis_title_pt = new InspectorPtSpinBox(page);
    d_spin_axis_title_pt->setRange(PlotStyleLimits::kFontSizeMin, PlotStyleLimits::kFontSizeMax);
    d_spin_axis_title_pt->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_axis_title_pt, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double pt) {
                Graph *g = activeLayer();
                const QVector<Graph *> targets = targetLayersForScope();
                if (d_syncing_props || !d_target_ml || !g || targets.isEmpty())
                    return;
                const bool lockAll4 = d_chk_lock_all_axes_font && d_chk_lock_all_axes_font->isChecked();
                const double oldTitlePt = qMax(
                        PlotStyleLimits::kFontSizeMin,
                        g->axisTitleFont(d_active_axis).pointSizeF() > 0.0
                                ? g->axisTitleFont(d_active_axis).pointSizeF()
                                : 14.0);
                const double oldTickPt = qMax(
                        PlotStyleLimits::kFontSizeMin,
                        g->axisFont(d_active_axis).pointSizeF() > 0.0
                                ? g->axisFont(d_active_axis).pointSizeF()
                                : 12.0);
                const double scaledTickPt = qBound(
                        PlotStyleLimits::kFontSizeMin,
                        std::round(oldTickPt * pt / oldTitlePt * 10.0) / 10.0,
                        PlotStyleLimits::kFontSizeMax);

                const QString before = d_target_ml->captureUndoSnapshot();
                auto applyLayerFonts = [&](Graph *lg) {
                    if (!lg)
                        return;
                    if (lockAll4) {
                        for (int a = 0; a < 4; ++a) {
                            QFont tf = lg->axisTitleFont(a);
                            if (tf.family().trimmed().isEmpty())
                                tf.setFamily(QStringLiteral("Arial"));
                            tf.setPointSizeF(pt);
                            lg->setAxisTitleFont(a, tf);

                            QFont nf = lg->axisFont(a);
                            if (nf.family().trimmed().isEmpty())
                                nf.setFamily(QStringLiteral("Arial"));
                            nf.setPointSizeF(scaledTickPt);
                            lg->setAxisFont(a, nf);
                        }
                        for (Graph *cand : d_target_ml->layersList()) {
                            if (cand && cand->linkedParentLayer() == lg) {
                                for (int a = 0; a < 4; ++a) {
                                    QFont ctf = cand->axisTitleFont(a);
                                    if (ctf.family().trimmed().isEmpty())
                                        ctf.setFamily(QStringLiteral("Arial"));
                                    ctf.setPointSizeF(pt);
                                    cand->setAxisTitleFont(a, ctf);

                                    QFont cnf = cand->axisFont(a);
                                    if (cnf.family().trimmed().isEmpty())
                                        cnf.setFamily(QStringLiteral("Arial"));
                                    cnf.setPointSizeF(scaledTickPt);
                                    cand->setAxisFont(a, cnf);
                                }
                                cand->replot();
                            }
                        }
                    } else {
                        QFont tf = lg->axisTitleFont(d_active_axis);
                        if (tf.family().trimmed().isEmpty())
                            tf.setFamily(QStringLiteral("Arial"));
                        tf.setPointSizeF(pt);
                        lg->setAxisTitleFont(d_active_axis, tf);
                    }
                    lg->replot();
                };
                for (Graph *lg : targets)
                    applyLayerFonts(lg);

                if (lockAll4 && d_spin_axis_tick_pt) {
                    QSignalBlocker bTick(d_spin_axis_tick_pt);
                    d_spin_axis_tick_pt->setValue(scaledTickPt);
                }
                d_target_ml->pushUndoSnapshot(tr("Axis Title Font Size"), before,
                                              kMergeAxisTitlePt);
            });

    d_lbl_tick_pt = new QLabel(page);
    d_spin_axis_tick_pt = new InspectorPtSpinBox(page);
    d_spin_axis_tick_pt->setRange(PlotStyleLimits::kFontSizeMin, PlotStyleLimits::kFontSizeMax);
    d_spin_axis_tick_pt->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_axis_tick_pt, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double pt) {
                const QVector<Graph *> targets = targetLayersForScope();
                if (d_syncing_props || !d_target_ml || targets.isEmpty())
                    return;
                const bool lockAll4 = d_chk_lock_all_axes_font && d_chk_lock_all_axes_font->isChecked();
                const QString before = d_target_ml->captureUndoSnapshot();
                for (Graph *lg : targets) {
                    if (!lg)
                        continue;
                    if (lockAll4) {
                        for (int a = 0; a < 4; ++a) {
                            QFont f = lg->axisFont(a);
                            if (f.family().trimmed().isEmpty())
                                f.setFamily(QStringLiteral("Arial"));
                            f.setPointSizeF(pt);
                            lg->setAxisFont(a, f);
                        }
                        for (Graph *cand : d_target_ml->layersList()) {
                            if (cand && cand->linkedParentLayer() == lg) {
                                for (int a = 0; a < 4; ++a) {
                                    QFont cf = cand->axisFont(a);
                                    if (cf.family().trimmed().isEmpty())
                                        cf.setFamily(QStringLiteral("Arial"));
                                    cf.setPointSizeF(pt);
                                    cand->setAxisFont(a, cf);
                                }
                                cand->replot();
                            }
                        }
                    } else {
                        QFont f = lg->axisFont(d_active_axis);
                        if (f.family().trimmed().isEmpty())
                            f.setFamily(QStringLiteral("Arial"));
                        f.setPointSizeF(pt);
                        lg->setAxisFont(d_active_axis, f);
                    }
                    lg->replot();
                }
                d_target_ml->pushUndoSnapshot(tr("Axis Tick Font Size"), before, kMergeAxisTickPt);
            });

    fntGrid->addWidget(d_lbl_title_pt, 0, 0);
    fntGrid->addWidget(d_spin_axis_title_pt, 0, 1);
    fntGrid->addWidget(d_lbl_tick_pt, 1, 0);
    fntGrid->addWidget(d_spin_axis_tick_pt, 1, 1);
    lay->addLayout(fntGrid);

    lay->addStretch(1);
    return page;
}

QWidget *GraphInspectorDock::buildCurveInspector()
{
    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 12, 10);
    lay->setSpacing(6);

    d_lbl_active_curve_name = new QLabel(page);
    d_lbl_active_curve_name->setObjectName(QStringLiteral("InspectorSubTitle"));
    d_lbl_active_curve_name->setWordWrap(true);
    lay->addWidget(d_lbl_active_curve_name);

    // =========================================================================
    // Regular 2D Curve / ErrorBar / Group Auto-Style Container
    // =========================================================================
    d_curve_regular_wrap = new QWidget(page);
    QVBoxLayout *regLay = new QVBoxLayout(d_curve_regular_wrap);
    regLay->setContentsMargins(0, 0, 0, 0);
    regLay->setSpacing(6);

    QGridLayout *styleGrid = new QGridLayout();
    styleGrid->setContentsMargins(0, 0, 0, 0);
    styleGrid->setHorizontalSpacing(6);
    styleGrid->setVerticalSpacing(4);

    d_lbl_plot_style = new QLabel(d_curve_regular_wrap);
    d_combo_curve_style = new QComboBox(d_curve_regular_wrap);
    d_combo_curve_style->addItem(QString(), int(Graph::Line));
    d_combo_curve_style->addItem(QString(), int(Graph::Scatter));
    d_combo_curve_style->addItem(QString(), int(Graph::LineSymbols));
    d_combo_curve_style->addItem(QString(), int(Graph::Spline));
    d_combo_curve_style->addItem(QString(), int(Graph::HorizontalSteps));
    d_combo_curve_style->addItem(QString(), int(Graph::VerticalSteps));
    d_combo_curve_style->addItem(QString(), int(Graph::VerticalDropLines));
    d_combo_curve_style->addItem(QString(), int(Graph::VerticalBars));
    d_combo_curve_style->addItem(QString(), int(Graph::HorizontalBars));
    d_combo_curve_style->addItem(QString(), int(Graph::Area));
    connect(d_combo_curve_style, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g || idx < 0)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                if (ci < 0 || ci >= g->curves())
                    return;
                QwtPlotCurve *cv = g->curve(ci);
                if (!cv)
                    return;

                const auto targetType =
                        static_cast<Graph::CurveType>(d_combo_curve_style->itemData(idx).toInt());
                const QString before = d_target_ml->captureUndoSnapshot();
                const QPen savedPen = cv->pen();
                const QwtSymbol savedSym = cv->symbol();
                const int savedYAxis = cv->yAxis();

                int resolvedCi = ci;
                if (Graph::canConvertTo(cv, targetType)) {
                    if (d_app)
                        d_app->invalidateModelessGraphDialogs(d_target_ml);
                    int ebBefore = 0;
                    if (DataCurve *dc = dynamic_cast<DataCurve *>(cv)) {
                        const int oldType = g->curveType(ci);
                        const bool barTransition =
                                ((targetType == Graph::VerticalBars
                                  || targetType == Graph::HorizontalBars)
                                 != (oldType == Graph::VerticalBars
                                     || oldType == Graph::HorizontalBars));
                        if (barTransition) {
                            for (DataCurve *eb : dc->errorBarsList()) {
                                const int ebi = g->curveIndex(eb);
                                if (ebi >= 0 && ebi < ci)
                                    ++ebBefore;
                            }
                        }
                    }
                    g->setCurveType(ci, targetType, true);
                    resolvedCi = qBound(0, ci - ebBefore, qMax(0, g->curves() - 1));
                    Graph::GraphSelection sel;
                    sel.kind = Graph::GraphSelection::Curve;
                    sel.primaryId = resolvedCi;
                    g->setCurrentSelection(sel, false);
                    cv = g->curve(resolvedCi);
                }
                if (cv) {
                    cv->setYAxis(savedYAxis);
                    const bool usesLine =
                            (targetType == Graph::Line || targetType == Graph::LineSymbols
                             || targetType == Graph::Spline || targetType == Graph::HorizontalSteps
                             || targetType == Graph::VerticalSteps
                             || targetType == Graph::VerticalDropLines
                             || targetType == Graph::Area);
                    if (usesLine) {
                        QPen p = savedPen;
                        if (p.style() == Qt::NoPen)
                            p.setStyle(Qt::SolidLine);
                        if (p.width() <= 0)
                            p.setWidth(2);
                        cv->setPen(p);
                    }
                    if (targetType == Graph::Scatter || targetType == Graph::LineSymbols
                        || targetType == Graph::Spline) {
                        if (targetType == Graph::Scatter || targetType == Graph::LineSymbols) {
                            QwtSymbol s = savedSym;
                            if (s.style() == QwtSymbol::NoSymbol) {
                                const QColor col = savedPen.color().isValid() ? savedPen.color()
                                                                              : QColor(Qt::black);
                                s.setStyle(QwtSymbol::Ellipse);
                                s.setSize(QSize(8, 8));
                                s.setPen(QPen(col, qMax(1, savedPen.width())));
                                s.setBrush(d_chk_sym_hollow->isChecked() ? QBrush(QColor(Qt::white))
                                                                         : QBrush(col));
                            }
                            cv->setSymbol(s);
                        } else if (savedSym.style() != QwtSymbol::NoSymbol) {
                            cv->setSymbol(savedSym);
                        }
                    } else if (targetType == Graph::Line || targetType == Graph::HorizontalSteps
                               || targetType == Graph::VerticalSteps
                               || targetType == Graph::VerticalDropLines) {
                        QwtSymbol s = cv->symbol();
                        s.setStyle(QwtSymbol::NoSymbol);
                        cv->setSymbol(s);
                    }
                    if (targetType == Graph::Area) {
                        QColor fillCol = cv->pen().color();
                        fillCol.setAlpha(65);
                        cv->setBrush(QBrush(fillCol));
                    }
                }
                g->reconstructLegend();
                g->updatePlot();
                d_target_ml->pushUndoSnapshot(tr("Change Curve Plot Style"), before);
                refreshObjectTree();
                syncCurveControls();
            });
    styleGrid->addWidget(d_lbl_plot_style, 0, 0);
    styleGrid->addWidget(d_combo_curve_style, 0, 1);
    regLay->addLayout(styleGrid);

    // Line Appearance
    d_sec_line_style_lbl = makeSectionHeaderLabel(d_curve_regular_wrap);
    regLay->addWidget(d_sec_line_style_lbl);

    QGridLayout *lineGrid = new QGridLayout();
    lineGrid->setContentsMargins(0, 0, 0, 0);
    lineGrid->setHorizontalSpacing(6);
    lineGrid->setVerticalSpacing(4);

    d_lbl_line_color = new QLabel(d_curve_regular_wrap);
    d_btn_line_color = new QPushButton(d_curve_regular_wrap);
    updateColorSwatchButton(d_btn_line_color, QColor(Qt::black));
    connect(d_btn_line_color, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;
        const bool hasLine =
                (cv->style() != QwtPlotCurve::NoCurve && cv->pen().style() != Qt::NoPen);
        const QColor cur = (hasLine && cv->pen().color().isValid()) ? cv->pen().color()
                                                                    : cv->symbol().pen().color();
        QColor chosen = pickColorInteractive(this, cur, tr("Select Series Color"));
        if (!chosen.isValid() || chosen == cur)
            return;

        if (d_spin_curve_opacity && chosen.alpha() == 255 && d_spin_curve_opacity->value() < 100)
            chosen.setAlpha(qRound(d_spin_curve_opacity->value() * 2.55));

        const QString before = d_target_ml->captureUndoSnapshot();
        QPen lp = cv->pen();
        lp.setColor(chosen);
        cv->setPen(lp);

        const bool linkSym = !d_chk_link_sym_color || d_chk_link_sym_color->isChecked();
        if (linkSym) {
            QwtSymbol sym = cv->symbol();
            const bool wasHollow = Graph::isSymbolWhiteHollow(sym)
                    || (d_chk_sym_hollow && d_chk_sym_hollow->isChecked());
            QPen sp = sym.pen();
            QColor edgeCol = chosen;
            edgeCol.setAlpha(chosen.alpha());
            sp.setColor(edgeCol);
            sym.setPen(sp);
            if (wasHollow)
                sym.setBrush(QBrush(QColor(Qt::white)));
            else if (sym.brush().style() != Qt::NoBrush)
                sym.setBrush(QBrush(chosen));
            cv->setSymbol(sym);
        }

        if (cv->brush().style() != Qt::NoBrush) {
            QColor areaCol = chosen;
            const int curAlpha = cv->brush().color().alpha();
            areaCol.setAlpha(curAlpha > 0 && curAlpha < 255 ? curAlpha : 65);
            cv->setBrush(QBrush(areaCol));
        }
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Series Color"), before);
        refreshObjectTree();
        syncCurveControls();
    });

    d_lbl_line_width = new QLabel(d_curve_regular_wrap);
    d_spin_line_width = new QDoubleSpinBox(d_curve_regular_wrap);
    d_spin_line_width->setRange(0.5, double(PlotStyleLimits::kLineWidthMax));
    d_spin_line_width->setDecimals(1);
    d_spin_line_width->setSingleStep(0.5);
    d_spin_line_width->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_line_width, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double w) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
                if (!cv)
                    return;
                const QString before = d_target_ml->captureUndoSnapshot();
                QPen lp = cv->pen();
                lp.setWidthF(w);
                cv->setPen(lp);
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Line Width"), before, kMergeCurveWidth);
                refreshObjectTree();
            });

    d_lbl_line_pen = new QLabel(d_curve_regular_wrap);
    d_combo_line_pen = new QComboBox(d_curve_regular_wrap);
    d_combo_line_pen->addItem(QString(), int(Qt::SolidLine));
    d_combo_line_pen->addItem(QString(), int(Qt::DashLine));
    d_combo_line_pen->addItem(QString(), int(Qt::DotLine));
    d_combo_line_pen->addItem(QString(), int(Qt::DashDotLine));
    d_combo_line_pen->addItem(QString(), int(Qt::NoPen));
    connect(d_combo_line_pen, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g || idx < 0)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
                if (!cv)
                    return;
                const auto ps = static_cast<Qt::PenStyle>(d_combo_line_pen->itemData(idx).toInt());
                const QString before = d_target_ml->captureUndoSnapshot();
                QPen lp = cv->pen();
                lp.setStyle(ps);
                cv->setPen(lp);
                if (ps == Qt::NoPen)
                    cv->setStyle(QwtPlotCurve::NoCurve);
                else if (cv->style() == QwtPlotCurve::NoCurve)
                    cv->setStyle(QwtPlotCurve::Lines);
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Line Pen Style"), before);
                refreshObjectTree();
            });

    d_lbl_curve_opacity = new QLabel(d_curve_regular_wrap);
    d_spin_curve_opacity = new QSpinBox(d_curve_regular_wrap);
    d_spin_curve_opacity->setRange(0, 100);
    d_spin_curve_opacity->setSingleStep(5);
    d_spin_curve_opacity->setValue(100);
    d_spin_curve_opacity->setSuffix(QStringLiteral("%"));
    connect(d_spin_curve_opacity, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int pct) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
                if (!cv)
                    return;
                const int alpha = qBound(0, qRound(pct * 2.55), 255);
                const QString before = d_target_ml->captureUndoSnapshot();
                QPen lp = cv->pen();
                QColor lc = lp.color();
                lc.setAlpha(alpha);
                lp.setColor(lc);
                cv->setPen(lp);

                if (cv->brush().style() != Qt::NoBrush) {
                    QColor bc = cv->brush().color();
                    bc.setAlpha(alpha);
                    cv->setBrush(QBrush(bc));
                }

                if (!d_chk_link_sym_color || d_chk_link_sym_color->isChecked()) {
                    QwtSymbol sym = cv->symbol();
                    QPen sp = sym.pen();
                    QColor sc = sp.color();
                    sc.setAlpha(alpha);
                    sp.setColor(sc);
                    sym.setPen(sp);
                    if (!Graph::isSymbolWhiteHollow(sym) && sym.brush().style() != Qt::NoBrush) {
                        QColor sfc = sym.brush().color();
                        sfc.setAlpha(alpha);
                        sym.setBrush(QBrush(sfc));
                    }
                    cv->setSymbol(sym);
                }
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Curve Opacity"), before,
                                              kMergeCurveOpacity);
                refreshObjectTree();
            });

    lineGrid->addWidget(d_lbl_line_color, 0, 0);
    lineGrid->addWidget(d_btn_line_color, 0, 1);
    lineGrid->addWidget(d_lbl_line_width, 1, 0);
    lineGrid->addWidget(d_spin_line_width, 1, 1);
    lineGrid->addWidget(d_lbl_line_pen, 2, 0);
    lineGrid->addWidget(d_combo_line_pen, 2, 1);
    lineGrid->addWidget(d_lbl_curve_opacity, 3, 0);
    lineGrid->addWidget(d_spin_curve_opacity, 3, 1);
    regLay->addLayout(lineGrid);

    // Symbol Appearance & Hollow + Independent Edge/Fill Colors + Right-Y Binding
    d_sec_symbol_style_lbl = makeSectionHeaderLabel(d_curve_regular_wrap);
    regLay->addWidget(d_sec_symbol_style_lbl);

    QGridLayout *symGrid = new QGridLayout();
    symGrid->setContentsMargins(0, 0, 0, 0);
    symGrid->setHorizontalSpacing(6);
    symGrid->setVerticalSpacing(4);

    d_lbl_sym_shape = new QLabel(d_curve_regular_wrap);
    d_combo_sym_shape = new QComboBox(d_curve_regular_wrap);
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Ellipse));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Rect));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Diamond));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::UTriangle));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::DTriangle));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::LTriangle));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::RTriangle));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Cross));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::XCross));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Star1));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Hexagon));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::NoSymbol));
    connect(d_combo_sym_shape, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g || idx < 0)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
                if (!cv)
                    return;
                const auto st =
                        static_cast<QwtSymbol::Style>(d_combo_sym_shape->itemData(idx).toInt());
                const QString before = d_target_ml->captureUndoSnapshot();
                QwtSymbol sym = cv->symbol();
                const bool hasLine =
                        (cv->style() != QwtPlotCurve::NoCurve && cv->pen().style() != Qt::NoPen);
                const QColor baseCol = (hasLine && cv->pen().color().isValid())
                        ? cv->pen().color()
                        : sym.pen().color();
                sym.setStyle(st);
                if (st != QwtSymbol::NoSymbol) {
                    if (sym.size().width() <= 0)
                        sym.setSize(QSize(d_spin_sym_size->value(), d_spin_sym_size->value()));
                    QPen sp = sym.pen();
                    if (!sp.color().isValid())
                        sp.setColor(baseCol);
                    if (sp.widthF() <= 0.0)
                        sp.setWidth(qMax(1, cv->pen().width()));
                    sym.setPen(sp);
                    if (d_chk_sym_hollow->isChecked()) {
                        sym.setBrush(QBrush(QColor(Qt::white)));
                    } else if (!d_chk_link_sym_color || d_chk_link_sym_color->isChecked()
                               || sym.brush().style() == Qt::NoBrush
                               || Graph::isSymbolWhiteHollow(sym)) {
                        sym.setBrush(QBrush(sp.color()));
                    }
                }
                cv->setSymbol(sym);
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Symbol Shape"), before);
                refreshObjectTree();
            });

    d_lbl_sym_size = new QLabel(d_curve_regular_wrap);
    d_spin_sym_size = new QSpinBox(d_curve_regular_wrap);
    d_spin_sym_size->setRange(PlotStyleLimits::kSymbolSizeMin, PlotStyleLimits::kSymbolSizeMax);
    d_spin_sym_size->setValue(8);
    d_spin_sym_size->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_sym_size, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int sz) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        QwtSymbol sym = cv->symbol();
        sym.setSize(QSize(sz, sz));
        cv->setSymbol(sym);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Symbol Size"), before, kMergeSymSize);
        refreshObjectTree();
    });

    d_lbl_sym_skip = new QLabel(d_curve_regular_wrap);
    d_spin_sym_skip = new QSpinBox(d_curve_regular_wrap);
    d_spin_sym_skip->setRange(0, 500);
    d_spin_sym_skip->setValue(0);
    connect(d_spin_sym_skip, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int skip) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        PlotCurve *pc =
                (ci >= 0 && ci < g->curves()) ? dynamic_cast<PlotCurve *>(g->curve(ci)) : nullptr;
        if (!pc || pc->symbolSkip() == skip)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        pc->setSymbolSkip(skip);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Symbol Skip"), before, kMergeSymSkip);
    });

    symGrid->addWidget(d_lbl_sym_shape, 0, 0);
    symGrid->addWidget(d_combo_sym_shape, 0, 1);
    symGrid->addWidget(d_lbl_sym_size, 1, 0);
    symGrid->addWidget(d_spin_sym_size, 1, 1);
    symGrid->addWidget(d_lbl_sym_skip, 2, 0);
    symGrid->addWidget(d_spin_sym_skip, 2, 1);
    regLay->addLayout(symGrid);

    d_chk_sym_hollow = new QCheckBox(d_curve_regular_wrap);
    connect(d_chk_sym_hollow, &QCheckBox::toggled, this, [this](bool hollow) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        if (ci < 0 || ci >= g->curves())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        g->setCurveSymbolHollow(ci, hollow);
        d_target_ml->pushUndoSnapshot(tr("Toggle Hollow Symbol"), before);
        refreshObjectTree();
        syncCurveControls();
    });
    regLay->addWidget(d_chk_sym_hollow);

    d_chk_link_sym_color = new QCheckBox(d_curve_regular_wrap);
    d_chk_link_sym_color->setChecked(true);
    regLay->addWidget(d_chk_link_sym_color);

    d_sym_custom_colors_wrap = new QWidget(d_curve_regular_wrap);
    QGridLayout *symColGrid = new QGridLayout(d_sym_custom_colors_wrap);
    symColGrid->setContentsMargins(8, 2, 0, 2);
    symColGrid->setHorizontalSpacing(6);
    symColGrid->setVerticalSpacing(4);

    d_lbl_sym_edge_color = new QLabel(d_sym_custom_colors_wrap);
    d_btn_sym_edge_color = new QPushButton(d_sym_custom_colors_wrap);
    updateColorSwatchButton(d_btn_sym_edge_color, QColor(Qt::black));
    connect(d_btn_sym_edge_color, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;
        QwtSymbol sym = cv->symbol();
        const QColor cur = sym.pen().color().isValid() ? sym.pen().color() : cv->pen().color();
        const QColor chosen = pickColorInteractive(this, cur, tr("Select Symbol Edge Color"));
        if (!chosen.isValid() || chosen == cur)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        QPen sp = sym.pen();
        sp.setColor(chosen);
        sym.setPen(sp);
        cv->setSymbol(sym);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Symbol Edge Color"), before);
        refreshObjectTree();
        syncCurveControls();
    });

    d_lbl_sym_fill_color = new QLabel(d_sym_custom_colors_wrap);
    d_btn_sym_fill_color = new QPushButton(d_sym_custom_colors_wrap);
    updateColorSwatchButton(d_btn_sym_fill_color, QColor(Qt::white));
    connect(d_btn_sym_fill_color, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;
        QwtSymbol sym = cv->symbol();
        const QColor cur =
                (sym.brush().style() != Qt::NoBrush && sym.brush().color().isValid())
                ? sym.brush().color()
                : QColor(Qt::white);
        const QColor chosen = pickColorInteractive(this, cur, tr("Select Symbol Fill Color"));
        if (!chosen.isValid() || chosen == cur)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        sym.setBrush(QBrush(chosen));
        cv->setSymbol(sym);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Symbol Fill Color"), before);
        refreshObjectTree();
        syncCurveControls();
    });

    d_lbl_sym_edge_width = new QLabel(d_sym_custom_colors_wrap);
    d_spin_sym_edge_width = new QDoubleSpinBox(d_sym_custom_colors_wrap);
    d_spin_sym_edge_width->setRange(PlotStyleLimits::kSymbolEdgeWidthMin,
                                    PlotStyleLimits::kSymbolEdgeWidthMax);
    d_spin_sym_edge_width->setSingleStep(0.5);
    d_spin_sym_edge_width->setDecimals(1);
    d_spin_sym_edge_width->setValue(1.5);
    d_spin_sym_edge_width->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_sym_edge_width, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double w) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
                if (!cv)
                    return;
                const QString before = d_target_ml->captureUndoSnapshot();
                QwtSymbol sym = cv->symbol();
                QPen sp = sym.pen();
                sp.setWidthF(w);
                sym.setPen(sp);
                cv->setSymbol(sym);
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Symbol Edge Width"), before,
                                              kMergeSymEdgeWidth);
                refreshObjectTree();
            });

    symColGrid->addWidget(d_lbl_sym_edge_color, 0, 0);
    symColGrid->addWidget(d_btn_sym_edge_color, 0, 1);
    symColGrid->addWidget(d_lbl_sym_fill_color, 1, 0);
    symColGrid->addWidget(d_btn_sym_fill_color, 1, 1);
    symColGrid->addWidget(d_lbl_sym_edge_width, 2, 0);
    symColGrid->addWidget(d_spin_sym_edge_width, 2, 1);
    d_sym_custom_colors_wrap->hide();
    regLay->addWidget(d_sym_custom_colors_wrap);

    connect(d_chk_link_sym_color, &QCheckBox::toggled, this, [this](bool linked) {
        if (d_sym_custom_colors_wrap)
            d_sym_custom_colors_wrap->setVisible(!linked);
        if (d_syncing_props || !linked)
            return;
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv || !cv->pen().color().isValid())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        QwtSymbol sym = cv->symbol();
        const bool wasHollow = Graph::isSymbolWhiteHollow(sym)
                || (d_chk_sym_hollow && d_chk_sym_hollow->isChecked());
        QPen sp = sym.pen();
        sp.setColor(cv->pen().color());
        sym.setPen(sp);
        sym.setBrush(wasHollow ? QBrush(QColor(Qt::white)) : QBrush(cv->pen().color()));
        cv->setSymbol(sym);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Link Symbol Color"), before);
        refreshObjectTree();
        syncCurveControls();
    });

    d_chk_curve_right_y = new QCheckBox(d_curve_regular_wrap);
    connect(d_chk_curve_right_y, &QCheckBox::toggled, this, [this](bool rightY) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;

        // B1-5: guard against duplicating right-Y axis ticks when a linked overlay layer
        // already owns the right-Y axis for this primary layer.
        Graph *rightOverlay = nullptr;
        if (rightY && !g->linkedParentLayer()) {
            for (Graph *cand : d_target_ml->layersList()) {
                if (!cand || cand->linkedParentLayer() != g || !cand->plotWidget())
                    continue;
                Plot *cp = cand->plotWidget();
                const QwtScaleDraw *sdRight = cp->axisScaleDraw(QwtPlot::yRight);
                if (cp->axisEnabled(QwtPlot::yRight) && sdRight
                    && sdRight->hasComponent(QwtAbstractScaleDraw::Labels)) {
                    rightOverlay = cand;
                    break;
                }
            }
            if (rightOverlay && rightOverlay->curves() > 0) {
                QSignalBlocker blk(d_chk_curve_right_y);
                d_chk_curve_right_y->setChecked(false);
                if (d_app && d_app->statusBar()) {
                    d_app->statusBar()->showMessage(
                            tr("A linked Right-Y overlay layer already owns the right Y axis. "
                               "Add or edit curves on that layer instead."),
                            5000);
                }
                return;
            }
        }

        const QString before = d_target_ml->captureUndoSnapshot();
        if (rightY && rightOverlay && rightOverlay->curves() == 0 && rightOverlay->plotWidget()) {
            // Empty linked right-Y overlay: silence its duplicate right-Y ticks/labels cleanly.
            Plot *cp = rightOverlay->plotWidget();
            if (QwtScaleDraw *sd = cp->axisScaleDraw(QwtPlot::yRight))
                sd->enableComponent(QwtAbstractScaleDraw::Labels, false);
            cp->setMajorTicksType(QwtPlot::yRight, ScaleDraw::None);
            cp->setMinorTicksType(QwtPlot::yRight, ScaleDraw::None);
            rightOverlay->replot();
        }
        const int targetAxis = rightY ? QwtPlot::yRight : QwtPlot::yLeft;
        cv->setYAxis(targetAxis);
        if (DataCurve *dc = dynamic_cast<DataCurve *>(cv)) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->setYAxis(targetAxis);
            }
        }
        if (rightY) {
            g->enableAxis(QwtPlot::yRight, true);
            g->setAxisBackboneVisible(QwtPlot::yRight, true);
            if (Plot *pw = g->plotWidget()) {
                if (QwtScaleDraw *sd = pw->axisScaleDraw(QwtPlot::yRight))
                    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                pw->setMajorTicksType(QwtPlot::yRight, ScaleDraw::In);
                pw->setMinorTicksType(QwtPlot::yRight, ScaleDraw::In);
            }
        }
        if (!g->isAxisUserLocked(targetAxis))
            g->setAutoScaleAxis(targetAxis);
        g->refreshAutoAxisTitlesFromColumns();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Assign Curve to Right-Y"), before);
        refreshObjectTree();
        syncCurveControls();
    });
    d_btn_match_right_y_color = new QPushButton(d_curve_regular_wrap);
    d_btn_match_right_y_color->setIcon(
            IconLoader::duotoneIcon(QStringLiteral("color-palette"), 13));
    connect(d_btn_match_right_y_color, &QPushButton::clicked, this,
            &GraphInspectorDock::matchRightYAxisColorToCurve);
    regLay->addWidget(d_chk_curve_right_y);
    regLay->addWidget(d_btn_match_right_y_color);

    // Error Bars + Live Error Bar Styling
    d_sec_err_bars_lbl = makeSectionHeaderLabel(d_curve_regular_wrap);
    regLay->addWidget(d_sec_err_bars_lbl);

    QHBoxLayout *errBtnRow = new QHBoxLayout();
    errBtnRow->setContentsMargins(0, 0, 0, 0);
    errBtnRow->setSpacing(4);
    d_btn_err_adjacent = new QPushButton(d_curve_regular_wrap);
    d_btn_err_dialog = new QPushButton(d_curve_regular_wrap);
    d_btn_err_adjacent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    d_btn_err_dialog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(d_btn_err_adjacent, &QPushButton::clicked, this,
            &GraphInspectorDock::applyAdjacentErrorBars);
    connect(d_btn_err_dialog, &QPushButton::clicked, this,
            &GraphInspectorDock::openErrorBarsDialog);
    errBtnRow->addWidget(d_btn_err_adjacent, 1);
    errBtnRow->addWidget(d_btn_err_dialog, 1);
    regLay->addLayout(errBtnRow);

    d_err_style_wrap = new QWidget(d_curve_regular_wrap);
    QVBoxLayout *errStyleLay = new QVBoxLayout(d_err_style_wrap);
    errStyleLay->setContentsMargins(0, 2, 0, 0);
    errStyleLay->setSpacing(4);

    d_sec_err_style_lbl = makeSectionHeaderLabel(d_err_style_wrap);
    errStyleLay->addWidget(d_sec_err_style_lbl);

    QGridLayout *errGrid = new QGridLayout();
    errGrid->setContentsMargins(0, 0, 0, 0);
    errGrid->setHorizontalSpacing(6);
    errGrid->setVerticalSpacing(4);

    // C2-5 + claude_review_1 #3: QDoubleSpinBox with PlotStyleLimits::kErrorBarWidthMin/Max/Step and pt suffix
    d_lbl_err_width = new QLabel(d_err_style_wrap);
    d_spin_err_width = new QDoubleSpinBox(d_err_style_wrap);
    d_spin_err_width->setRange(PlotStyleLimits::kErrorBarWidthMin,
                               PlotStyleLimits::kErrorBarWidthMax);
    d_spin_err_width->setDecimals(1);
    d_spin_err_width->setSingleStep(PlotStyleLimits::kErrorBarWidthStep);
    d_spin_err_width->setValue(1.0);
    d_spin_err_width->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_err_width, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double w) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g)
                    return;
                const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
                if (errs.isEmpty())
                    return;
                const QString before = d_target_ml->captureUndoSnapshot();
                for (QwtErrorPlotCurve *er : errs)
                    er->setWidthF(w);
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Error Bar Width"), before, kMergeErrWidth);
            });

    d_lbl_err_cap = new QLabel(d_err_style_wrap);
    d_spin_err_cap = new QSpinBox(d_err_style_wrap);
    d_spin_err_cap->setRange(0, 32);
    d_spin_err_cap->setSingleStep(2);
    d_spin_err_cap->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_err_cap, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int cap) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
        if (errs.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (QwtErrorPlotCurve *er : errs)
            er->setCapLength(cap);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Error Bar Cap"), before, kMergeErrCap);
    });

    d_lbl_err_dir = new QLabel(d_err_style_wrap);
    d_combo_err_dir = new QComboBox(d_err_style_wrap);
    d_combo_err_dir->addItem(QString(), 0); // Both (±)
    d_combo_err_dir->addItem(QString(), 1); // Plus (+)
    d_combo_err_dir->addItem(QString(), 2); // Minus (-)
    connect(d_combo_err_dir, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g || idx < 0)
                    return;
                const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
                if (errs.isEmpty())
                    return;
                const int mode = d_combo_err_dir->itemData(idx).toInt();
                const QString before = d_target_ml->captureUndoSnapshot();
                for (QwtErrorPlotCurve *er : errs) {
                    er->drawPlusSide(mode != 2);
                    er->drawMinusSide(mode != 1);
                }
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Error Bar Direction"), before);
            });

    d_lbl_err_color = new QLabel(d_err_style_wrap);
    QHBoxLayout *errColRow = new QHBoxLayout();
    errColRow->setContentsMargins(0, 0, 0, 0);
    errColRow->setSpacing(4);
    d_btn_err_color = new QPushButton(d_err_style_wrap);
    updateColorSwatchButton(d_btn_err_color, QColor(Qt::black));
    connect(d_btn_err_color, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
        if (errs.isEmpty())
            return;
        const QColor cur = errs.first()->color();
        const QColor chosen = pickColorInteractive(this, cur, tr("Select Error Bar Color"));
        if (!chosen.isValid() || chosen == cur)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (QwtErrorPlotCurve *er : errs)
            er->setColor(chosen);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Error Bar Color"), before);
        syncCurveControls();
    });
    d_btn_err_match_curve = new QPushButton(d_err_style_wrap);
    connect(d_btn_err_match_curve, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
        if (errs.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (QwtErrorPlotCurve *er : errs) {
            if (DataCurve *mc = er->masterCurve()) {
                const bool hasL =
                        (mc->style() != QwtPlotCurve::NoCurve && mc->pen().style() != Qt::NoPen);
                const QColor mcCol =
                        (hasL && mc->pen().color().isValid()) ? mc->pen().color()
                                                              : mc->symbol().pen().color();
                if (mcCol.isValid())
                    er->setColor(mcCol);
            }
        }
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Error Bar Color"), before);
        syncCurveControls();
    });
    errColRow->addWidget(d_btn_err_color);
    errColRow->addWidget(d_btn_err_match_curve, 1);

    d_chk_err_band = new QCheckBox(d_err_style_wrap);
    connect(d_chk_err_band, &QCheckBox::toggled, this, [this](bool on) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
        if (errs.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (QwtErrorPlotCurve *er : errs)
            er->setBand(on);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Toggle Error Band"), before);
        syncCurveControls();
    });

    d_lbl_err_band_opacity = new QLabel(d_err_style_wrap);
    d_spin_err_band_opacity = new QSpinBox(d_err_style_wrap);
    d_spin_err_band_opacity->setRange(5, 100);
    d_spin_err_band_opacity->setSingleStep(5);
    d_spin_err_band_opacity->setValue(25);
    d_spin_err_band_opacity->setSuffix(QStringLiteral("%"));
    connect(d_spin_err_band_opacity, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int pct) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g)
                    return;
                const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
                if (errs.isEmpty())
                    return;
                const QString before = d_target_ml->captureUndoSnapshot();
                const int alpha = qBound(10, qRound(double(pct) * 2.55), 255);
                for (QwtErrorPlotCurve *er : errs)
                    er->setBandAlpha(alpha);
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Error Band Opacity"), before,
                                              kMergeErrBandOpacity);
            });

    errGrid->addWidget(d_lbl_err_width, 0, 0);
    errGrid->addWidget(d_spin_err_width, 0, 1);
    errGrid->addWidget(d_lbl_err_cap, 1, 0);
    errGrid->addWidget(d_spin_err_cap, 1, 1);
    errGrid->addWidget(d_lbl_err_dir, 2, 0);
    errGrid->addWidget(d_combo_err_dir, 2, 1);
    errGrid->addWidget(d_lbl_err_color, 3, 0);
    errGrid->addLayout(errColRow, 3, 1);
    errGrid->addWidget(d_chk_err_band, 4, 0, 1, 2);
    errGrid->addWidget(d_lbl_err_band_opacity, 5, 0);
    errGrid->addWidget(d_spin_err_band_opacity, 5, 1);
    errStyleLay->addLayout(errGrid);
    d_err_style_wrap->hide();
    regLay->addWidget(d_err_style_wrap);

    // Color-Mapped Scatter (PointColorMap) Live Controls
    d_point_cmap_wrap = new QWidget(d_curve_regular_wrap);
    QVBoxLayout *pcmLay = new QVBoxLayout(d_point_cmap_wrap);
    pcmLay->setContentsMargins(0, 2, 0, 0);
    pcmLay->setSpacing(4);
    d_sec_pcm_lbl = makeSectionHeaderLabel(d_point_cmap_wrap);
    pcmLay->addWidget(d_sec_pcm_lbl);

    QGridLayout *pcmGrid = new QGridLayout();
    pcmGrid->setContentsMargins(0, 0, 0, 0);
    pcmGrid->setHorizontalSpacing(6);
    pcmGrid->setVerticalSpacing(4);

    d_lbl_pcm_cmap = new QLabel(d_point_cmap_wrap);
    d_combo_pcm_cmap = new QComboBox(d_point_cmap_wrap);
    d_combo_pcm_cmap->addItem(QString(), int(Spectrogram::PresetLegacyDefault));
    d_combo_pcm_cmap->addItem(QStringLiteral("Viridis"), int(Spectrogram::PresetViridis));
    d_combo_pcm_cmap->addItem(QStringLiteral("Magma"), int(Spectrogram::PresetMagma));
    d_combo_pcm_cmap->addItem(QStringLiteral("Inferno"), int(Spectrogram::PresetInferno));
    d_combo_pcm_cmap->addItem(QStringLiteral("Plasma"), int(Spectrogram::PresetPlasma));
    d_combo_pcm_cmap->addItem(QStringLiteral("Cividis"), int(Spectrogram::PresetCividis));
    d_combo_pcm_cmap->addItem(QString(), int(Spectrogram::PresetCoolWarm));
    d_combo_pcm_cmap->addItem(QString(), int(Spectrogram::PresetBlueWhiteRed));
    d_combo_pcm_cmap->addItem(QStringLiteral("Jet"), int(Spectrogram::PresetJet));
    d_combo_pcm_cmap->addItem(QString(), int(Spectrogram::PresetGrayScale));

    d_chk_pcm_reverse = new QCheckBox(d_point_cmap_wrap);
    d_chk_pcm_colorbar = new QCheckBox(d_point_cmap_wrap);
    d_lbl_pcm_cb_title = new QLabel(d_point_cmap_wrap);
    d_edit_pcm_cb_title = new QLineEdit(d_point_cmap_wrap);
    wirePendingEditCue(d_edit_pcm_cb_title);
    d_chk_pcm_custom_z = new QCheckBox(d_point_cmap_wrap);
    d_lbl_pcm_zmin = new QLabel(d_point_cmap_wrap);
    d_edit_pcm_zmin = new QLineEdit(d_point_cmap_wrap);
    d_lbl_pcm_zmax = new QLabel(d_point_cmap_wrap);
    d_edit_pcm_zmax = new QLineEdit(d_point_cmap_wrap);
    wirePendingEditCue(d_edit_pcm_zmin);
    wirePendingEditCue(d_edit_pcm_zmax);

    auto applyPointColorMapFromUi = [this](const QString &undoLabel, int mergeId = -1,
                                           bool fromZEdits = false,
                                           bool fromCbTitleEdit = false) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        if (fromCbTitleEdit && d_edit_pcm_cb_title) {
            const bool isMod = d_edit_pcm_cb_title->isModified();
            const bool isDiff = (d_edit_pcm_cb_title->text().trimmed()
                                 != d_edit_pcm_cb_title->property("_syncedText").toString());
            if (!isMod && !isDiff) {
                setLineEditPendingCue(d_edit_pcm_cb_title, false);
                return;
            }
        }
        if (fromZEdits && d_edit_pcm_zmin && d_edit_pcm_zmax) {
            const bool anyMod = d_edit_pcm_zmin->isModified() || d_edit_pcm_zmax->isModified();
            const bool anyDiff = (d_edit_pcm_zmin->text().trimmed()
                                  != d_edit_pcm_zmin->property("_syncedText").toString())
                    || (d_edit_pcm_zmax->text().trimmed()
                        != d_edit_pcm_zmax->property("_syncedText").toString());
            if (!anyMod && !anyDiff) {
                setLineEditPendingCue(d_edit_pcm_zmin, false);
                setLineEditPendingCue(d_edit_pcm_zmax, false);
                return;
            }
            if (!anyDiff) {
                d_edit_pcm_zmin->setModified(false);
                d_edit_pcm_zmax->setModified(false);
                setLineEditPendingCue(d_edit_pcm_zmin, false);
                setLineEditPendingCue(d_edit_pcm_zmax, false);
                return;
            }
        }
        const int ci = effectiveSelectedCurveIndex(g, true);
        DataCurve *dc = (ci >= 0 && ci < g->curves()) ? dynamic_cast<DataCurve *>(g->curve(ci))
                                                      : nullptr;
        if (!dc || !dc->hasPointColorMap())
            return;
        DataCurve::PointColorMap pcm = dc->pointColorMap();
        if (d_combo_pcm_cmap && d_combo_pcm_cmap->currentIndex() >= 0)
            pcm.preset = d_combo_pcm_cmap->currentData().toInt();
        pcm.reverse = d_chk_pcm_reverse && d_chk_pcm_reverse->isChecked();
        pcm.showColorBar = !d_chk_pcm_colorbar || d_chk_pcm_colorbar->isChecked();
        if (d_edit_pcm_cb_title) {
            const QString nextCbTitle =
                    CanvasInlineTextEditor::normalizeOriginRichText(d_edit_pcm_cb_title->text());
            const QString displayedCbTitle = pcm.colorBarTitle.isEmpty()
                    ? g->axisTitle(QwtPlot::yRight)
                    : pcm.colorBarTitle;
            if (fromCbTitleEdit && nextCbTitle == displayedCbTitle) {
                d_edit_pcm_cb_title->setModified(false);
                setLineEditPendingCue(d_edit_pcm_cb_title, false);
                return;
            }
            const bool cbChanged = d_edit_pcm_cb_title->isModified()
                    || (nextCbTitle.trimmed()
                        != d_edit_pcm_cb_title->property("_syncedText").toString());
            if (cbChanged && nextCbTitle != displayedCbTitle)
                pcm.colorBarTitle = nextCbTitle;
        }
        const bool customZ = d_chk_pcm_custom_z && d_chk_pcm_custom_z->isChecked();
        pcm.autoRange = !customZ;
        if (customZ && d_edit_pcm_zmin && d_edit_pcm_zmax) {
            bool okLo = false, okHi = false;
            double lo = QLocale::c().toDouble(d_edit_pcm_zmin->text().trimmed(), &okLo);
            if (!okLo)
                lo = QLocale().toDouble(d_edit_pcm_zmin->text().trimmed(), &okLo);
            double hi = QLocale::c().toDouble(d_edit_pcm_zmax->text().trimmed(), &okHi);
            if (!okHi)
                hi = QLocale().toDouble(d_edit_pcm_zmax->text().trimmed(), &okHi);
            if (okLo && okHi && std::isfinite(lo) && std::isfinite(hi) && qAbs(hi - lo) > 1e-15) {
                pcm.zMin = qMin(lo, hi);
                pcm.zMax = qMax(lo, hi);
            }
        }
        const QString before = d_target_ml->captureUndoSnapshot();
        dc->setPointColorMap(pcm);
        g->syncPointColorBar();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(undoLabel, before, mergeId);
        syncCurveControls();
    };

    connect(d_combo_pcm_cmap, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [applyPointColorMapFromUi](int) {
                applyPointColorMapFromUi(tr("Change Scatter Colormap"));
            });
    connect(d_chk_pcm_reverse, &QCheckBox::toggled, this, [applyPointColorMapFromUi](bool) {
        applyPointColorMapFromUi(tr("Change Scatter Colormap"));
    });
    connect(d_chk_pcm_colorbar, &QCheckBox::toggled, this, [applyPointColorMapFromUi](bool) {
        applyPointColorMapFromUi(tr("Toggle Colorbar"));
    });
    connect(d_edit_pcm_cb_title, &QLineEdit::editingFinished, this,
            [applyPointColorMapFromUi]() {
                applyPointColorMapFromUi(tr("Edit Colorbar Title"), kMergePcmCbTitle, false, true);
            });
    connect(d_chk_pcm_custom_z, &QCheckBox::toggled, this, [applyPointColorMapFromUi](bool) {
        applyPointColorMapFromUi(tr("Change Scatter Z Range"));
    });
    connect(d_edit_pcm_zmin, &QLineEdit::editingFinished, this, [applyPointColorMapFromUi]() {
        applyPointColorMapFromUi(tr("Change Scatter Z Range"), -1, true);
    });
    connect(d_edit_pcm_zmax, &QLineEdit::editingFinished, this, [applyPointColorMapFromUi]() {
        applyPointColorMapFromUi(tr("Change Scatter Z Range"), -1, true);
    });

    pcmGrid->addWidget(d_lbl_pcm_cmap, 0, 0);
    pcmGrid->addWidget(d_combo_pcm_cmap, 0, 1);
    pcmGrid->addWidget(d_chk_pcm_reverse, 1, 0, 1, 2);
    pcmGrid->addWidget(d_chk_pcm_colorbar, 2, 0, 1, 2);
    pcmGrid->addWidget(d_lbl_pcm_cb_title, 3, 0);
    pcmGrid->addWidget(d_edit_pcm_cb_title, 3, 1);
    pcmGrid->addWidget(d_chk_pcm_custom_z, 4, 0, 1, 2);
    pcmGrid->addWidget(d_lbl_pcm_zmin, 5, 0);
    pcmGrid->addWidget(d_edit_pcm_zmin, 5, 1);
    pcmGrid->addWidget(d_lbl_pcm_zmax, 6, 0);
    pcmGrid->addWidget(d_edit_pcm_zmax, 6, 1);
    pcmLay->addLayout(pcmGrid);
    d_point_cmap_wrap->hide();
    regLay->addWidget(d_point_cmap_wrap);

    // Group Series Style Scheme (OriginPro Palette + Symbol Shape Cycle + Paired Hollow)
    d_sec_series_scheme_lbl = makeSectionHeaderLabel(d_curve_regular_wrap);
    regLay->addWidget(d_sec_series_scheme_lbl);

    d_combo_series_scheme = new QComboBox(d_curve_regular_wrap);
    refreshPaletteSchemeCombo();
    // Only a choice made by the user pins the combo; otherwise it follows the default palette.
    connect(d_combo_series_scheme, QOverload<int>::of(&QComboBox::activated), this,
            [this](int) { d_series_scheme_user_chosen = true; });
    regLay->addWidget(d_combo_series_scheme);

    d_chk_cycle_sym_shapes = new QCheckBox(d_curve_regular_wrap);
    d_chk_cycle_sym_shapes->setChecked(true);
    regLay->addWidget(d_chk_cycle_sym_shapes);

    d_chk_paired_hollow = new QCheckBox(d_curve_regular_wrap);
    d_chk_paired_hollow->setChecked(false);
    regLay->addWidget(d_chk_paired_hollow);

    d_btn_apply_scheme_all = new QPushButton(d_curve_regular_wrap);
    d_btn_apply_scheme_all->setIcon(
            IconLoader::duotoneIcon(QStringLiteral("color-palette"), 14));
    connect(d_btn_apply_scheme_all, &QPushButton::clicked, this,
            &GraphInspectorDock::applyGroupAutoStyleFromUi);
    regLay->addWidget(d_btn_apply_scheme_all);

    // Non-Destructive Y-Offset Stack
    d_sec_y_stack_lbl = makeSectionHeaderLabel(d_curve_regular_wrap);
    regLay->addWidget(d_sec_y_stack_lbl);

    QHBoxLayout *stkGapRow = new QHBoxLayout();
    d_lbl_stack_gap = new QLabel(d_curve_regular_wrap);
    d_spin_stack_gap = new QDoubleSpinBox(d_curve_regular_wrap);
    d_spin_stack_gap->setRange(0.0, 200.0);
    d_spin_stack_gap->setDecimals(0);
    d_spin_stack_gap->setSingleStep(5.0);
    d_spin_stack_gap->setValue(15.0);
    d_spin_stack_gap->setSuffix(QStringLiteral("%"));
    stkGapRow->addWidget(d_lbl_stack_gap);
    stkGapRow->addWidget(d_spin_stack_gap);
    regLay->addLayout(stkGapRow);

    QHBoxLayout *stkBtnRow = new QHBoxLayout();
    stkBtnRow->setSpacing(4);
    d_btn_apply_stack = new QPushButton(d_curve_regular_wrap);
    connect(d_btn_apply_stack, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        g->setStackOffsets(1 /*AutoGap*/, d_spin_stack_gap->value(), 0.0, false);
        d_target_ml->pushUndoSnapshot(tr("Apply Y-Offset Stack"), before);
        syncAxisControls();
    });
    d_btn_reset_stack = new QPushButton(d_curve_regular_wrap);
    connect(d_btn_reset_stack, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        g->resetStackOffsets();
        d_target_ml->pushUndoSnapshot(tr("Reset Stack Offsets"), before);
        syncAxisControls();
    });
    stkBtnRow->addWidget(d_btn_apply_stack);
    stkBtnRow->addWidget(d_btn_reset_stack);
    regLay->addLayout(stkBtnRow);

    lay->addWidget(d_curve_regular_wrap);

    // =========================================================================
    // Spectrogram / Heatmap Live Controls Container
    // =========================================================================
    d_spectrogram_wrap = new QWidget(page);
    QVBoxLayout *specLay = new QVBoxLayout(d_spectrogram_wrap);
    specLay->setContentsMargins(0, 0, 0, 0);
    specLay->setSpacing(6);

    d_sec_spec_lbl = makeSectionHeaderLabel(d_spectrogram_wrap);
    specLay->addWidget(d_sec_spec_lbl);

    QGridLayout *specGrid = new QGridLayout();
    specGrid->setContentsMargins(0, 0, 0, 0);
    specGrid->setHorizontalSpacing(6);
    specGrid->setVerticalSpacing(4);

    d_lbl_spec_cmap = new QLabel(d_spectrogram_wrap);
    d_combo_spec_cmap = new QComboBox(d_spectrogram_wrap);
    d_combo_spec_cmap->addItem(QString(), int(Spectrogram::PresetLegacyDefault));
    d_combo_spec_cmap->addItem(QStringLiteral("Viridis"), int(Spectrogram::PresetViridis));
    d_combo_spec_cmap->addItem(QStringLiteral("Magma"), int(Spectrogram::PresetMagma));
    d_combo_spec_cmap->addItem(QStringLiteral("Inferno"), int(Spectrogram::PresetInferno));
    d_combo_spec_cmap->addItem(QStringLiteral("Plasma"), int(Spectrogram::PresetPlasma));
    d_combo_spec_cmap->addItem(QStringLiteral("Cividis"), int(Spectrogram::PresetCividis));
    d_combo_spec_cmap->addItem(QString(), int(Spectrogram::PresetCoolWarm));
    d_combo_spec_cmap->addItem(QString(), int(Spectrogram::PresetBlueWhiteRed));
    d_combo_spec_cmap->addItem(QStringLiteral("Jet"), int(Spectrogram::PresetJet));
    d_combo_spec_cmap->addItem(QString(), int(Spectrogram::PresetGrayScale));

    d_chk_spec_reverse = new QCheckBox(d_spectrogram_wrap);

    auto applySpecColormap = [this]() {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const auto preset =
                static_cast<Spectrogram::ColorMapPreset>(d_combo_spec_cmap->currentData().toInt());
        const bool rev = d_chk_spec_reverse->isChecked();
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setColorMapPreset(preset, rev);
        sp->syncColorScale();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Heatmap Colormap"), before);
        refreshObjectTree();
    };
    connect(d_combo_spec_cmap, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [applySpecColormap](int) { applySpecColormap(); });
    connect(d_chk_spec_reverse, &QCheckBox::toggled, this,
            [applySpecColormap](bool) { applySpecColormap(); });

    specGrid->addWidget(d_lbl_spec_cmap, 0, 0);
    specGrid->addWidget(d_combo_spec_cmap, 0, 1);
    specGrid->addWidget(d_chk_spec_reverse, 1, 0, 1, 2);

    // B2-7: Log color scale & Cell blocks checkboxes for Spectrogram / 2D Density
    d_chk_spec_log_scale = new QCheckBox(d_spectrogram_wrap);
    connect(d_chk_spec_log_scale, &QCheckBox::toggled, this, [this](bool on) {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setLogColorScale(on);
        sp->syncColorScale();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Toggle Log Color Scale"), before);
        syncCurveControls();
    });
    specGrid->addWidget(d_chk_spec_log_scale, 2, 0, 1, 2);

    d_chk_spec_cell_blocks = new QCheckBox(d_spectrogram_wrap);
    connect(d_chk_spec_cell_blocks, &QCheckBox::toggled, this, [this](bool on) {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setCellBlocks(on);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Toggle Cell Blocks"), before);
        syncCurveControls();
    });
    specGrid->addWidget(d_chk_spec_cell_blocks, 3, 0, 1, 2);

    d_chk_spec_colorbar = new QCheckBox(d_spectrogram_wrap);
    connect(d_chk_spec_colorbar, &QCheckBox::toggled, this, [this](bool on) {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        const int axis = (sp->colorScaleAxis() >= 0 && sp->colorScaleAxis() < 4)
                ? sp->colorScaleAxis()
                : int(QwtPlot::yRight);
        sp->showColorScale(axis, on);
        if (on)
            sp->enforceColorBarArialFont();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Toggle Colorbar"), before);
        syncCurveControls();
    });
    specGrid->addWidget(d_chk_spec_colorbar, 4, 0, 1, 2);

    d_lbl_spec_cb_title = new QLabel(d_spectrogram_wrap);
    d_edit_spec_cb_title = new QLineEdit(d_spectrogram_wrap);
    connect(d_edit_spec_cb_title, &QLineEdit::editingFinished, this, [this]() {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const QString nextTitle =
                CanvasInlineTextEditor::normalizeOriginRichText(d_edit_spec_cb_title->text());
        if (sp->colorBarTitle() == nextTitle)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setColorBarTitle(nextTitle);
        sp->enforceColorBarArialFont();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Edit Colorbar Title"), before, kMergeSpecCbTitle);
    });
    d_spec_cb_sym_bar = buildRichTextBar(d_spectrogram_wrap, RichTargetMode::SpectrogramCbTitle);
    specGrid->addWidget(d_lbl_spec_cb_title, 5, 0);
    specGrid->addWidget(d_edit_spec_cb_title, 5, 1);
    specGrid->addWidget(d_spec_cb_sym_bar, 6, 0, 1, 2);

    d_lbl_spec_cb_width = new QLabel(d_spectrogram_wrap);
    d_spin_spec_cb_width = new QSpinBox(d_spectrogram_wrap);
    d_spin_spec_cb_width->setRange(6, 64);
    d_spin_spec_cb_width->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_spec_cb_width, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int w) {
                Graph *g = activeLayer();
                Spectrogram *sp = effectiveSelectedSpectrogram(g);
                if (d_syncing_props || !d_target_ml || !g || !sp)
                    return;
                const QString before = d_target_ml->captureUndoSnapshot();
                sp->setColorBarWidth(w);
                g->replot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Colorbar Width"), before,
                                              kMergeSpecCbWidth);
            });
    specGrid->addWidget(d_lbl_spec_cb_width, 7, 0);
    specGrid->addWidget(d_spin_spec_cb_width, 7, 1);

    d_chk_spec_contours = new QCheckBox(d_spectrogram_wrap);
    connect(d_chk_spec_contours, &QCheckBox::toggled, this, [this](bool on) {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setDisplayMode(QwtPlotSpectrogram::ContourMode, on);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Toggle Contour Lines"), before);
        syncCurveControls();
    });
    specGrid->addWidget(d_chk_spec_contours, 8, 0, 1, 2);

    d_lbl_spec_levels = new QLabel(d_spectrogram_wrap);
    d_spin_spec_levels = new QSpinBox(d_spectrogram_wrap);
    d_spin_spec_levels->setRange(2, 1000);
    connect(d_spin_spec_levels, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int lv) {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setLevelsNumber(lv);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Contour Levels"), before, kMergeSpecLevels);
    });
    specGrid->addWidget(d_lbl_spec_levels, 9, 0);
    specGrid->addWidget(d_spin_spec_levels, 9, 1);

    d_chk_spec_custom_z = new QCheckBox(d_spectrogram_wrap);
    d_lbl_spec_zmin = new QLabel(d_spectrogram_wrap);
    d_edit_spec_zmin = new QLineEdit(d_spectrogram_wrap);
    d_lbl_spec_zmax = new QLabel(d_spectrogram_wrap);
    d_edit_spec_zmax = new QLineEdit(d_spectrogram_wrap);
    wirePendingEditCue(d_edit_spec_zmin);
    wirePendingEditCue(d_edit_spec_zmax);

    auto applySpecZRange = [this](bool fromZEdits) {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        if (fromZEdits) {
            const bool anyMod = d_edit_spec_zmin->isModified() || d_edit_spec_zmax->isModified();
            const bool anyDiff = (d_edit_spec_zmin->text().trimmed()
                                  != d_edit_spec_zmin->property("_syncedText").toString())
                    || (d_edit_spec_zmax->text().trimmed()
                        != d_edit_spec_zmax->property("_syncedText").toString());
            if (!anyMod && !anyDiff) {
                setLineEditPendingCue(d_edit_spec_zmin, false);
                setLineEditPendingCue(d_edit_spec_zmax, false);
                return;
            }
            if (!anyDiff) {
                d_edit_spec_zmin->setModified(false);
                d_edit_spec_zmax->setModified(false);
                setLineEditPendingCue(d_edit_spec_zmin, false);
                setLineEditPendingCue(d_edit_spec_zmax, false);
                return;
            }
        }
        const bool custom = d_chk_spec_custom_z->isChecked();
        d_edit_spec_zmin->setEnabled(custom);
        d_edit_spec_zmax->setEnabled(custom);
        bool okMin = false, okMax = false;
        double zMin = d_edit_spec_zmin->text().trimmed().toDouble(&okMin);
        double zMax = d_edit_spec_zmax->text().trimmed().toDouble(&okMax);
        if (custom && (!okMin || !okMax || !std::isfinite(zMin) || !std::isfinite(zMax)))
            return;
        if (custom && qAbs(zMax - zMin) < 1e-30)
            zMax = zMin + 1.0;
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setCustomValueRange(custom, zMin, zMax);
        sp->syncColorScale();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Heatmap Z Range"), before);
        syncCurveControls();
    };
    connect(d_chk_spec_custom_z, &QCheckBox::toggled, this,
            [applySpecZRange](bool) { applySpecZRange(false); });
    connect(d_edit_spec_zmin, &QLineEdit::editingFinished, this,
            [applySpecZRange]() { applySpecZRange(true); });
    connect(d_edit_spec_zmax, &QLineEdit::editingFinished, this,
            [applySpecZRange]() { applySpecZRange(true); });

    specGrid->addWidget(d_chk_spec_custom_z, 10, 0, 1, 2);
    specGrid->addWidget(d_lbl_spec_zmin, 11, 0);
    specGrid->addWidget(d_edit_spec_zmin, 11, 1);
    specGrid->addWidget(d_lbl_spec_zmax, 12, 0);
    specGrid->addWidget(d_edit_spec_zmax, 12, 1);

    specLay->addLayout(specGrid);
    d_spectrogram_wrap->hide();
    lay->addWidget(d_spectrogram_wrap);

    lay->addStretch(1);
    return page;
}

QWidget *GraphInspectorDock::buildLegendInspector()
{
    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 12, 10);
    lay->setSpacing(6);

    d_lbl_legend_target = new QLabel(page);
    d_lbl_legend_target->setObjectName(QStringLiteral("InspectorSubTitle"));
    lay->addWidget(d_lbl_legend_target);

    // =========================================================================
    // 1) Primary Legend & Text Annotation Container
    // =========================================================================
    d_leg_text_wrap = new QWidget(page);
    QVBoxLayout *legWrapLay = new QVBoxLayout(d_leg_text_wrap);
    legWrapLay->setContentsMargins(0, 0, 0, 0);
    legWrapLay->setSpacing(6);

    d_sec_legend_pos_lbl = makeSectionHeaderLabel(d_leg_text_wrap);
    legWrapLay->addWidget(d_sec_legend_pos_lbl);

    // A4-5: 2x3 grid so placement labels (Top-Center, Outside Right) never clip at 252px width
    QGridLayout *snapGrid = new QGridLayout();
    snapGrid->setContentsMargins(0, 0, 0, 0);
    snapGrid->setHorizontalSpacing(4);
    snapGrid->setVerticalSpacing(4);

    auto makeSnapBtn = [&](Graph::LegendAnchor anchor, int row, int col) -> QPushButton * {
        QPushButton *b = new QPushButton(d_leg_text_wrap);
        connect(b, &QPushButton::clicked, this, [this, anchor]() {
            Graph *g = activeLayer();
            if (!d_target_ml || !g)
                return;
            const QString before = d_target_ml->captureUndoSnapshot();
            if (!g->hasLegend())
                g->newLegend();
            g->setLegendVisible(true);
            g->placeLegendAnchor(anchor);
            d_target_ml->pushUndoSnapshot(tr("Place Legend"), before);
            refreshObjectTree();
            syncLegendControls();
        });
        snapGrid->addWidget(b, row, col);
        return b;
    };

    d_btn_leg_tl = makeSnapBtn(Graph::LegendTopLeft, 0, 0);
    d_btn_leg_tr = makeSnapBtn(Graph::LegendTopRight, 0, 1);
    d_btn_leg_bl = makeSnapBtn(Graph::LegendBottomLeft, 1, 0);
    d_btn_leg_br = makeSnapBtn(Graph::LegendBottomRight, 1, 1);
    d_btn_leg_tc = makeSnapBtn(Graph::LegendTopCenter, 2, 0);
    d_btn_leg_out_r = makeSnapBtn(Graph::LegendOutsideRight, 2, 1);
    legWrapLay->addLayout(snapGrid);

    d_btn_leg_auto_avoid = new QPushButton(d_leg_text_wrap);
    d_btn_leg_auto_avoid->setIcon(IconLoader::duotoneIcon(QStringLiteral("legend"), 15));
    connect(d_btn_leg_auto_avoid, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        if (!g->hasLegend())
            g->newLegend();
        g->setLegendVisible(true);
        g->placeLegendAnchor(Graph::LegendAutoAvoidData);
        d_target_ml->pushUndoSnapshot(tr("Place Legend in Empty Space"), before);
        refreshObjectTree();
        syncLegendControls();
    });
    legWrapLay->addWidget(d_btn_leg_auto_avoid);

    d_btn_leg_rebuild = new QPushButton(d_leg_text_wrap);
    d_btn_leg_rebuild->setObjectName(QStringLiteral("InspectorRebuildLegendBtn"));
    d_btn_leg_rebuild->setIcon(IconLoader::duotoneIcon(QStringLiteral("recalc-now"), 15));
    connect(d_btn_leg_rebuild, &QPushButton::clicked, this, [this]() {
        QPointer<GraphInspectorDock> self(this);
        QPointer<MultiLayer> mlGuard(d_target_ml);
        QPointer<Graph> gGuard(activeLayer());
        if (!mlGuard || !gGuard)
            return;
        // A3-4: confirm before overwriting custom legend text (auto-confirmed in non-interactive mode)
        if (gGuard->hasLegend() && gGuard->legend()
            && !ApplicationWindow::isNonInteractiveContext(d_app)) {
            const QString curTxt = gGuard->legend()->text().trimmed();
            const QString autoTxt =
                    (gGuard->isPiePlot() ? gGuard->pieLegendText() : gGuard->legendText())
                            .trimmed();
            if (!curTxt.isEmpty() && curTxt != autoTxt) {
                const auto ans = QMessageBox::question(
                        this, tr("Rebuild Legend"),
                        tr("Rebuilding the legend will overwrite your custom legend labels and "
                           "header. Continue?"),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (ans != QMessageBox::Yes || !self || !mlGuard || !gGuard)
                    return;
            }
        }
        if (!self || !mlGuard || !gGuard)
            return;
        const QString before = mlGuard->captureUndoSnapshot();
        gGuard->reconstructLegend();
        gGuard->setLegendVisible(true);
        gGuard->replot();
        mlGuard->pushUndoSnapshot(tr("Reconstruct Legend"), before);
        self->refreshObjectTree();
        self->syncLegendControls();
    });
    legWrapLay->addWidget(d_btn_leg_rebuild);

    d_sec_legend_style_lbl = makeSectionHeaderLabel(d_leg_text_wrap);
    legWrapLay->addWidget(d_sec_legend_style_lbl);

    QGridLayout *legGrid = new QGridLayout();
    legGrid->setContentsMargins(0, 0, 0, 0);
    legGrid->setHorizontalSpacing(6);
    legGrid->setVerticalSpacing(4);

    // Frame modes:
    //  0 -> Borderless (Legend::None, transparent bg)
    //  1 -> Black Box + White Fill (Legend::Line, solid white bg)
    //  2 -> Transparent Background + Box (Legend::Line, transparent bg)
    d_lbl_leg_frame = new QLabel(d_leg_text_wrap);
    d_combo_leg_frame = new QComboBox(d_leg_text_wrap);
    d_combo_leg_frame->addItem(QString(), 0);
    d_combo_leg_frame->addItem(QString(), 1);
    d_combo_leg_frame->addItem(QString(), 2);
    connect(d_combo_leg_frame, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g || idx < 0)
                    return;
                Legend *target = nullptr;
                const auto selKind = g->currentSelection().kind;
                if (selKind == Graph::GraphSelection::TextAnnotation)
                    target = g->textMarker(g->currentSelection().primaryId);
                else if (selKind != Graph::GraphSelection::ArrowAnnotation
                         && selKind != Graph::GraphSelection::ImageAnnotation
                         && selKind != Graph::GraphSelection::PlotTitle)
                    target = g->legend();
                if (!target)
                    return;

                const int mode = d_combo_leg_frame->itemData(idx).toInt();
                const QString before = d_target_ml->captureUndoSnapshot();
                if (mode == 0) {
                    target->setFrameStyle(Legend::None);
                    target->setBackgroundColor(QColor(255, 255, 255, 0));
                } else if (mode == 1) {
                    target->setFrameStyle(Legend::Line);
                    target->setBackgroundColor(QColor(255, 255, 255, 255));
                } else {
                    target->setFrameStyle(Legend::Line);
                    target->setBackgroundColor(QColor(255, 255, 255, 0));
                }
                g->replot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Legend Frame"), before);
            });

    d_lbl_leg_header = new QLabel(d_leg_text_wrap);
    d_edit_leg_header = new QLineEdit(d_leg_text_wrap);
    d_edit_leg_header->installEventFilter(this);
    wirePendingEditCue(d_edit_leg_header);
    connect(d_edit_leg_header, &QLineEdit::editingFinished, this, [this]() {
        if (d_syncing_props || !d_edit_leg_header)
            return;
        const bool isMod = d_edit_leg_header->isModified();
        const bool isDiff = (d_edit_leg_header->text().trimmed()
                             != d_edit_leg_header->property("_syncedText").toString());
        if (!isMod && !isDiff) {
            setLineEditPendingCue(d_edit_leg_header, false);
            return;
        }
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;

        const auto selKind = g->currentSelection().kind;
        Legend *leg = nullptr;
        if (selKind == Graph::GraphSelection::TextAnnotation)
            leg = g->textMarker(g->currentSelection().primaryId);
        else if (selKind != Graph::GraphSelection::ArrowAnnotation
                 && selKind != Graph::GraphSelection::ImageAnnotation
                 && selKind != Graph::GraphSelection::PlotTitle)
            leg = g->legend();

        const bool isPrimaryLegend = (g && leg && leg == g->legend())
                || (g && !leg && selKind == Graph::GraphSelection::LegendBox);

        if (!isPrimaryLegend) {
            if (!leg)
                return;
            const QString nextHdr =
                    CanvasInlineTextEditor::normalizeOriginRichText(d_edit_leg_header->text());
            QStringList lines = leg->text().split('\n');
            if (lines.isEmpty())
                lines.append(nextHdr);
            else
                lines[0] = nextHdr;
            const QString nextTxt = lines.join('\n');
            d_edit_leg_header->setProperty("_syncedText", nextHdr.trimmed());
            d_edit_leg_header->setModified(false);
            setLineEditPendingCue(d_edit_leg_header, false);
            if (leg->text() == nextTxt)
                return;
            const QString before = d_target_ml->captureUndoSnapshot();
            leg->setText(nextTxt);
            g->replot();
            emit g->modifiedGraph();
            d_target_ml->pushUndoSnapshot(tr("Edit Annotation Text"), before, kMergeLegendHeader);
            refreshObjectTree();
            return;
        }

        if (!g->hasLegend())
            g->newLegend();
        leg = g->legend();
        if (!leg)
            return;

        QStringList lines = leg->text().split('\n');
        while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
            lines.removeLast();

        const QString hdr =
                CanvasInlineTextEditor::normalizeOriginRichText(d_edit_leg_header->text().trimmed());
        if (!lines.isEmpty() && !isLegendEntryLine(lines.first())) {
            if (hdr.isEmpty())
                lines.removeFirst();
            else
                lines[0] = hdr;
        } else if (!hdr.isEmpty()) {
            lines.prepend(hdr);
        }

        const QString updated = lines.join(QStringLiteral("\n"));
        d_edit_leg_header->setProperty("_syncedText", hdr);
        d_edit_leg_header->setModified(false);
        setLineEditPendingCue(d_edit_leg_header, false);
        if (leg->text() == updated)
            return;

        const QString before = d_target_ml->captureUndoSnapshot();
        leg->setText(updated);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Edit Legend Header"), before, kMergeLegendHeader);
    });

    d_lbl_leg_cols = new QLabel(d_leg_text_wrap);
    d_spin_leg_cols = new QSpinBox(d_leg_text_wrap);
    d_spin_leg_cols->setRange(1, 4);
    d_spin_leg_cols->setValue(1);
    connect(d_spin_leg_cols, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int cols) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        Legend *leg = g->legend();
        if (!leg)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        leg->setNumColumns(cols);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Legend Columns"), before, kMergeLegendCols);
    });

    d_lbl_leg_font_pt = new QLabel(d_leg_text_wrap);
    d_spin_leg_font_pt = new InspectorPtSpinBox(d_leg_text_wrap);
    d_spin_leg_font_pt->setRange(PlotStyleLimits::kFontSizeMin, PlotStyleLimits::kFontSizeMax);
    d_spin_leg_font_pt->setValue(12);
    d_spin_leg_font_pt->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_leg_font_pt, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double pt) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g)
                    return;
                Legend *target = nullptr;
                const auto selKind = g->currentSelection().kind;
                if (selKind == Graph::GraphSelection::TextAnnotation)
                    target = g->textMarker(g->currentSelection().primaryId);
                else if (selKind != Graph::GraphSelection::ArrowAnnotation
                         && selKind != Graph::GraphSelection::ImageAnnotation
                         && selKind != Graph::GraphSelection::PlotTitle)
                    target = g->legend();
                if (!target)
                    return;
                const QString before = d_target_ml->captureUndoSnapshot();
                QFont f = target->font();
                if (f.family().trimmed().isEmpty())
                    f.setFamily(QStringLiteral("Arial"));
                f.setPointSizeF(pt);
                target->setFont(f);
                g->replot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Legend Font Size"), before,
                                              kMergeLegendFontPt);
            });

    // C16 + E5-2: multi-line text annotation editor (flushes on 450ms timer, FocusOut,
    // target/selection switch, or closeEvent using g->multiLayerParent()).
    d_edit_annot_text = new QPlainTextEdit(d_leg_text_wrap);
    d_edit_annot_text->setObjectName(QStringLiteral("InspectorAnnotationTextEdit"));
    d_edit_annot_text->setTabChangesFocus(true);
    d_edit_annot_text->setFixedHeight(78);
    d_edit_annot_text->setHidden(true);
    d_edit_annot_text->installEventFilter(this);
    d_annot_commit_timer = new QTimer(this);
    d_annot_commit_timer->setSingleShot(true);
    d_annot_commit_timer->setInterval(450);
    connect(d_edit_annot_text, &QPlainTextEdit::textChanged, this, [this]() {
        if (d_edit_annot_text && !d_edit_annot_text->isHidden())
            updateRichPreviewLabel(d_lbl_leg_preview, d_edit_annot_text->toPlainText(), false);
        if (d_syncing_props)
            return;
        Graph *g = activeLayer();
        if (!d_annot_commit_timer->isActive() && g
            && g->currentSelection().kind == Graph::GraphSelection::TextAnnotation) {
            d_annot_edit_layer = g;
            d_annot_edit_id = g->currentSelection().primaryId;
        }
        if (d_annot_edit_layer && d_annot_edit_id >= 0)
            d_annot_commit_timer->start();
    });
    connect(d_annot_commit_timer, &QTimer::timeout, this,
            &GraphInspectorDock::flushPendingAnnotationEdit);

    d_lbl_leg_preview = new QLabel(d_leg_text_wrap);
    d_lbl_leg_preview->setObjectName(QStringLiteral("InspectorRichPreviewLabel"));
    d_lbl_leg_preview->setTextFormat(Qt::RichText);
    d_lbl_leg_preview->setWordWrap(true);
    d_lbl_leg_preview->hide();
    connect(d_edit_leg_header, &QLineEdit::textChanged, this, [this](const QString &txt) {
        if (d_edit_leg_header && !d_edit_leg_header->isHidden())
            updateRichPreviewLabel(d_lbl_leg_preview, txt, false);
    });

    d_leg_sym_bar = buildRichTextBar(d_leg_text_wrap, RichTargetMode::LegendOrAnnot);

    legGrid->addWidget(d_lbl_leg_frame, 0, 0);
    legGrid->addWidget(d_combo_leg_frame, 0, 1);
    legGrid->addWidget(d_lbl_leg_header, 1, 0);
    legGrid->addWidget(d_edit_leg_header, 1, 1);
    legGrid->addWidget(d_edit_annot_text, 1, 1);
    legGrid->addWidget(d_lbl_leg_preview, 2, 0, 1, 2);
    legGrid->addWidget(d_leg_sym_bar, 3, 0, 1, 2);
    legGrid->addWidget(d_lbl_leg_cols, 4, 0);
    legGrid->addWidget(d_spin_leg_cols, 4, 1);
    legGrid->addWidget(d_lbl_leg_font_pt, 5, 0);
    legGrid->addWidget(d_spin_leg_font_pt, 5, 1);
    legWrapLay->addLayout(legGrid);

    // A3-4: Per-curve legend label editor (compact swatch + QLineEdit per curve, primary legend only)
    d_leg_entries_wrap = new QWidget(d_leg_text_wrap);
    QVBoxLayout *entriesOuter = new QVBoxLayout(d_leg_entries_wrap);
    entriesOuter->setContentsMargins(0, 2, 0, 0);
    entriesOuter->setSpacing(4);
    d_sec_leg_entries_lbl = makeSectionHeaderLabel(d_leg_entries_wrap);
    entriesOuter->addWidget(d_sec_leg_entries_lbl);
    d_leg_entries_layout = new QVBoxLayout();
    d_leg_entries_layout->setContentsMargins(0, 0, 0, 0);
    d_leg_entries_layout->setSpacing(3);
    entriesOuter->addLayout(d_leg_entries_layout);
    legWrapLay->addWidget(d_leg_entries_wrap);

    lay->addWidget(d_leg_text_wrap);

    // =========================================================================
    // 2) Arrow / Line Annotation Editor (A3-5, C2-3)
    // =========================================================================
    d_arrow_editor_wrap = new QWidget(page);
    QVBoxLayout *arrowLay = new QVBoxLayout(d_arrow_editor_wrap);
    arrowLay->setContentsMargins(0, 0, 0, 0);
    arrowLay->setSpacing(6);
    d_sec_arrow_lbl = makeSectionHeaderLabel(d_arrow_editor_wrap);
    arrowLay->addWidget(d_sec_arrow_lbl);

    QGridLayout *arrowGrid = new QGridLayout();
    arrowGrid->setContentsMargins(0, 0, 0, 0);
    arrowGrid->setHorizontalSpacing(6);
    arrowGrid->setVerticalSpacing(4);

    d_lbl_arrow_color = new QLabel(d_arrow_editor_wrap);
    d_btn_arrow_color = new QPushButton(d_arrow_editor_wrap);
    updateColorSwatchButton(d_btn_arrow_color, QColor(Qt::black));
    connect(d_btn_arrow_color, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const auto sel = g->currentSelection();
        if (sel.kind != Graph::GraphSelection::ArrowAnnotation)
            return;
        ArrowMarker *am = g->arrow(sel.primaryId);
        if (!am)
            return;
        const QColor cur = am->color();
        const QColor chosen = pickColorInteractive(this, cur, tr("Select Arrow Color"));
        if (!chosen.isValid() || chosen == cur)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        am->setColor(chosen);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Arrow Style"), before);
        syncLegendControls();
    });

    d_lbl_arrow_width = new QLabel(d_arrow_editor_wrap);
    d_spin_arrow_width = new QDoubleSpinBox(d_arrow_editor_wrap);
    d_spin_arrow_width->setRange(0.5, 20.0);
    d_spin_arrow_width->setDecimals(1);
    d_spin_arrow_width->setSingleStep(0.5);
    d_spin_arrow_width->setValue(1.5);
    d_spin_arrow_width->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_arrow_width, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { applyArrowControlsFromUi(tr("Change Arrow Style"), kMergeArrowWidth); });

    d_lbl_arrow_style = new QLabel(d_arrow_editor_wrap);
    d_combo_arrow_style = new QComboBox(d_arrow_editor_wrap);
    d_combo_arrow_style->addItem(QString(), int(Qt::SolidLine));
    d_combo_arrow_style->addItem(QString(), int(Qt::DashLine));
    d_combo_arrow_style->addItem(QString(), int(Qt::DotLine));
    d_combo_arrow_style->addItem(QString(), int(Qt::DashDotLine));
    connect(d_combo_arrow_style, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { applyArrowControlsFromUi(tr("Change Arrow Style")); });

    d_chk_arrow_start = new QCheckBox(d_arrow_editor_wrap);
    connect(d_chk_arrow_start, &QCheckBox::toggled, this,
            [this](bool) { applyArrowControlsFromUi(tr("Change Arrow Style")); });

    d_chk_arrow_end = new QCheckBox(d_arrow_editor_wrap);
    connect(d_chk_arrow_end, &QCheckBox::toggled, this,
            [this](bool) { applyArrowControlsFromUi(tr("Change Arrow Style")); });

    d_lbl_arrow_head_len = new QLabel(d_arrow_editor_wrap);
    d_spin_arrow_head_len = new QSpinBox(d_arrow_editor_wrap);
    d_spin_arrow_head_len->setRange(1, 60);
    d_spin_arrow_head_len->setValue(10);
    d_spin_arrow_head_len->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_arrow_head_len, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { applyArrowControlsFromUi(tr("Change Arrow Style"), kMergeArrowHead); });

    d_lbl_arrow_head_angle = new QLabel(d_arrow_editor_wrap);
    d_spin_arrow_head_angle = new QSpinBox(d_arrow_editor_wrap);
    d_spin_arrow_head_angle->setRange(5, 85);
    d_spin_arrow_head_angle->setSingleStep(5);
    d_spin_arrow_head_angle->setValue(30);
    d_spin_arrow_head_angle->setSuffix(QString::fromUtf8("\xC2\xB0"));
    connect(d_spin_arrow_head_angle, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { applyArrowControlsFromUi(tr("Change Arrow Style"), kMergeArrowHead); });

    d_chk_arrow_filled = new QCheckBox(d_arrow_editor_wrap);
    connect(d_chk_arrow_filled, &QCheckBox::toggled, this,
            [this](bool) { applyArrowControlsFromUi(tr("Change Arrow Style")); });

    arrowGrid->addWidget(d_lbl_arrow_color, 0, 0);
    arrowGrid->addWidget(d_btn_arrow_color, 0, 1);
    arrowGrid->addWidget(d_lbl_arrow_width, 1, 0);
    arrowGrid->addWidget(d_spin_arrow_width, 1, 1);
    arrowGrid->addWidget(d_lbl_arrow_style, 2, 0);
    arrowGrid->addWidget(d_combo_arrow_style, 2, 1);
    arrowGrid->addWidget(d_chk_arrow_start, 3, 0, 1, 2);
    arrowGrid->addWidget(d_chk_arrow_end, 4, 0, 1, 2);
    arrowGrid->addWidget(d_lbl_arrow_head_len, 5, 0);
    arrowGrid->addWidget(d_spin_arrow_head_len, 5, 1);
    arrowGrid->addWidget(d_lbl_arrow_head_angle, 6, 0);
    arrowGrid->addWidget(d_spin_arrow_head_angle, 6, 1);
    arrowGrid->addWidget(d_chk_arrow_filled, 7, 0, 1, 2);
    arrowLay->addLayout(arrowGrid);
    d_arrow_editor_wrap->hide();
    lay->addWidget(d_arrow_editor_wrap);

    // =========================================================================
    // 3) Image Annotation Editor (A3-5, C2-3)
    // =========================================================================
    d_image_editor_wrap = new QWidget(page);
    QVBoxLayout *imgLay = new QVBoxLayout(d_image_editor_wrap);
    imgLay->setContentsMargins(0, 0, 0, 0);
    imgLay->setSpacing(6);
    d_sec_img_lbl = makeSectionHeaderLabel(d_image_editor_wrap);
    imgLay->addWidget(d_sec_img_lbl);

    d_lbl_img_filename = new QLabel(d_image_editor_wrap);
    d_lbl_img_filename->setWordWrap(true);
    imgLay->addWidget(d_lbl_img_filename);

    QGridLayout *imgGrid = new QGridLayout();
    imgGrid->setContentsMargins(0, 0, 0, 0);
    imgGrid->setHorizontalSpacing(6);
    imgGrid->setVerticalSpacing(4);

    auto makeImgSpin = [&](int minV, int maxV) -> QSpinBox * {
        QSpinBox *sb = new QSpinBox(d_image_editor_wrap);
        sb->setRange(minV, maxV);
        sb->setSuffix(QStringLiteral(" px"));
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this](int) { applyImageGeometryFromUi(); });
        return sb;
    };
    d_lbl_img_x = new QLabel(d_image_editor_wrap);
    d_spin_img_x = makeImgSpin(-2000, 4000);
    d_lbl_img_y = new QLabel(d_image_editor_wrap);
    d_spin_img_y = makeImgSpin(-2000, 4000);
    d_lbl_img_w = new QLabel(d_image_editor_wrap);
    d_spin_img_w = makeImgSpin(4, 4000);
    d_lbl_img_h = new QLabel(d_image_editor_wrap);
    d_spin_img_h = makeImgSpin(4, 4000);

    imgGrid->addWidget(d_lbl_img_x, 0, 0);
    imgGrid->addWidget(d_spin_img_x, 0, 1);
    imgGrid->addWidget(d_lbl_img_y, 1, 0);
    imgGrid->addWidget(d_spin_img_y, 1, 1);
    imgGrid->addWidget(d_lbl_img_w, 2, 0);
    imgGrid->addWidget(d_spin_img_w, 2, 1);
    imgGrid->addWidget(d_lbl_img_h, 3, 0);
    imgGrid->addWidget(d_spin_img_h, 3, 1);
    imgLay->addLayout(imgGrid);

    d_btn_img_replace = new QPushButton(d_image_editor_wrap);
    d_btn_img_replace->setIcon(IconLoader::duotoneIcon(QStringLiteral("export-image"), 14));
    connect(d_btn_img_replace, &QPushButton::clicked, this, [this]() {
        QPointer<GraphInspectorDock> self(this);
        QPointer<MultiLayer> mlGuard(d_target_ml);
        QPointer<Graph> gGuard(activeLayer());
        if (!mlGuard || !gGuard || ApplicationWindow::isNonInteractiveContext(d_app))
            return;
        const auto sel = gGuard->currentSelection();
        if (sel.kind != Graph::GraphSelection::ImageAnnotation)
            return;
        const int oldId = sel.primaryId;
        ImageMarker *oldIm = gGuard->imageMarker(oldId);
        if (!oldIm)
            return;
        const QString startDir = !oldIm->fileName().isEmpty()
                ? QFileInfo(oldIm->fileName()).absolutePath()
                : (d_app ? d_app->workingDir : QString());
        const QString fn = QFileDialog::getOpenFileName(
                this, tr("Select Replacement Image"), startDir,
                tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.gif *.svg);;All Files (*)"));
        if (!self || !mlGuard || !gGuard || fn.isEmpty() || !QFile::exists(fn))
            return;

        oldIm = gGuard->imageMarker(oldId);
        if (!oldIm)
            return;

        const double oldLeft = oldIm->xValue();
        const double oldTop = oldIm->yValue();
        const double oldRight = oldIm->right();
        const double oldBottom = oldIm->bottom();
        const QRect oldPx = oldIm->plot() ? oldIm->rect() : QRect();

        const QString before = mlGuard->captureUndoSnapshot();
        ImageMarker *newIm = gGuard->addImage(fn);
        if (!newIm)
            return;
        if (qAbs(oldRight - oldLeft) > 1e-12 && qAbs(oldBottom - oldTop) > 1e-12)
            newIm->setBoundingRect(oldLeft, oldTop, oldRight, oldBottom);
        else if (oldPx.isValid() && !oldPx.isEmpty())
            newIm->setRect(oldPx.x(), oldPx.y(), oldPx.width(), oldPx.height());

        const QVector<int> keys = gGuard->imageMarkerKeys();
        const int newId = keys.isEmpty() ? -1 : keys.last();

        gGuard->setSelectedMarker(oldId, false);
        gGuard->removeMarker();
        if (newId >= 0) {
            Graph::GraphSelection nextSel;
            nextSel.kind = Graph::GraphSelection::ImageAnnotation;
            nextSel.primaryId = newId;
            gGuard->setCurrentSelection(nextSel, false);
        }
        gGuard->replot();
        emit gGuard->modifiedGraph();
        mlGuard->pushUndoSnapshot(tr("Replace Image"), before);
        self->refreshAll();
    });
    imgLay->addWidget(d_btn_img_replace);
    d_image_editor_wrap->hide();
    lay->addWidget(d_image_editor_wrap);

    // =========================================================================
    // 4) Plot Title Editor (A3-5)
    // =========================================================================
    d_plot_title_wrap = new QWidget(page);
    QVBoxLayout *ptLay = new QVBoxLayout(d_plot_title_wrap);
    ptLay->setContentsMargins(0, 0, 0, 0);
    ptLay->setSpacing(6);
    d_sec_plot_title_lbl = makeSectionHeaderLabel(d_plot_title_wrap);
    ptLay->addWidget(d_sec_plot_title_lbl);

    QGridLayout *ptGrid = new QGridLayout();
    ptGrid->setContentsMargins(0, 0, 0, 0);
    ptGrid->setHorizontalSpacing(6);
    ptGrid->setVerticalSpacing(4);

    d_lbl_plot_title_text = new QLabel(d_plot_title_wrap);
    d_edit_plot_title = new QLineEdit(d_plot_title_wrap);
    wirePendingEditCue(d_edit_plot_title);
    connect(d_edit_plot_title, &QLineEdit::editingFinished, this, [this]() {
        if (d_syncing_props || !d_edit_plot_title)
            return;
        const bool isMod = d_edit_plot_title->isModified();
        const bool isDiff = (d_edit_plot_title->text().trimmed()
                             != d_edit_plot_title->property("_syncedText").toString());
        if (!isMod && !isDiff) {
            setLineEditPendingCue(d_edit_plot_title, false);
            return;
        }
        applyPlotTitleFromUi(tr("Edit Plot Title"), kMergePlotTitle);
    });
    d_lbl_plot_title_preview = new QLabel(d_plot_title_wrap);
    d_lbl_plot_title_preview->setObjectName(QStringLiteral("InspectorRichPreviewLabel"));
    d_lbl_plot_title_preview->setTextFormat(Qt::RichText);
    d_lbl_plot_title_preview->setWordWrap(true);
    d_lbl_plot_title_preview->hide();
    connect(d_edit_plot_title, &QLineEdit::textChanged, this, [this](const QString &txt) {
        updateRichPreviewLabel(d_lbl_plot_title_preview, txt, false);
    });
    d_plot_title_sym_bar = buildRichTextBar(d_plot_title_wrap, RichTargetMode::PlotTitle);

    d_lbl_plot_title_pt = new QLabel(d_plot_title_wrap);
    d_spin_plot_title_pt = new InspectorPtSpinBox(d_plot_title_wrap);
    d_spin_plot_title_pt->setRange(PlotStyleLimits::kFontSizeMin, PlotStyleLimits::kFontSizeMax);
    d_spin_plot_title_pt->setValue(16.0);
    d_spin_plot_title_pt->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_plot_title_pt, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) {
                applyPlotTitleFromUi(tr("Change Plot Title Font"), kMergePlotTitlePt);
            });

    d_chk_plot_title_bold = new QCheckBox(d_plot_title_wrap);
    connect(d_chk_plot_title_bold, &QCheckBox::toggled, this,
            [this](bool) { applyPlotTitleFromUi(tr("Change Plot Title Font")); });

    d_lbl_plot_title_color = new QLabel(d_plot_title_wrap);
    d_btn_plot_title_color = new QPushButton(d_plot_title_wrap);
    updateColorSwatchButton(d_btn_plot_title_color, QColor(Qt::black));
    connect(d_btn_plot_title_color, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g || !g->plotWidget())
            return;
        const QColor cur = g->plotWidget()->title().color();
        const QColor chosen = pickColorInteractive(this, cur, tr("Select Plot Title Color"));
        if (!chosen.isValid() || chosen == cur)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        g->setTitleColor(chosen);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Plot Title Color"), before);
        syncLegendControls();
    });

    ptGrid->addWidget(d_lbl_plot_title_text, 0, 0);
    ptGrid->addWidget(d_edit_plot_title, 0, 1);
    ptGrid->addWidget(d_lbl_plot_title_preview, 1, 0, 1, 2);
    ptGrid->addWidget(d_plot_title_sym_bar, 2, 0, 1, 2);
    ptGrid->addWidget(d_lbl_plot_title_pt, 3, 0);
    ptGrid->addWidget(d_spin_plot_title_pt, 3, 1);
    ptGrid->addWidget(d_chk_plot_title_bold, 4, 0, 1, 2);
    ptGrid->addWidget(d_lbl_plot_title_color, 5, 0);
    ptGrid->addWidget(d_btn_plot_title_color, 5, 1);
    ptLay->addLayout(ptGrid);
    d_plot_title_wrap->hide();
    lay->addWidget(d_plot_title_wrap);

    lay->addStretch(1);
    return page;
}
