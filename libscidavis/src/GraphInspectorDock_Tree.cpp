/***************************************************************************
    File                 : GraphInspectorDock_Tree.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Copyright            : (C) 2026 ToriGraph Project
    Description          : Object Manager tree, reference lines, and curve/object edits
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

enum TreeItemRoles {
    RoleSelKind = Qt::UserRole + 1,
    RolePrimaryId = Qt::UserRole + 2,
    RoleSecondaryId = Qt::UserRole + 3,
    RoleLayerIndex = Qt::UserRole + 4,
    RoleIsGroupHeader = Qt::UserRole + 5
};

// Object Manager clipboard state for B1-9 (Copy/Paste Axis Scale Range & Curve Color/Symbol).
struct CopiedSingleAxisScale {
    bool valid = false;
    double lo = 0.0;
    double hi = 1.0;
    double step = 0.0;
    int maxMajor = 8;
    int maxMinor = 5;
    int scaleType = 0; // 0 = Linear, 1 = Log10
    bool inverted = false;
    Graph::AxisBreakSpec brk;
};

struct CopiedAxisScaleClipboard {
    bool valid = false;
    bool allAxes = true;
    int sourceAxis = QwtPlot::xBottom;
    CopiedSingleAxisScale axes[4];
};

struct CopiedCurveStyleClipboard {
    bool valid = false;
    QColor lineColor;
    QwtSymbol symbol;
    bool hasBrush = false;
    QBrush brush;
};

CopiedAxisScaleClipboard s_copiedAxisScale;
CopiedCurveStyleClipboard s_copiedCurveStyle;

static QString formatColumnLabelWithMeta(Table *t, int c, const QString &roleTag)
{
    if (!t || c < 0 || c >= t->numCols())
        return QString();
    const QString colShort = stripHtmlToCleanUnicode(t->colLabel(c));
    const QString longName = stripHtmlToCleanUnicode(t->colLongName(c)).trimmed();
    const QString units = stripHtmlToCleanUnicode(t->colUnits(c)).trimmed();
    QString meta;
    if (!longName.isEmpty() && longName != colShort && !units.isEmpty())
        meta = QStringLiteral(" [%1 (%2)]").arg(longName, units);
    else if (!longName.isEmpty() && longName != colShort)
        meta = QStringLiteral(" [%1]").arg(longName);
    else if (!units.isEmpty())
        meta = QStringLiteral(" [(%1)]").arg(units);
    return QStringLiteral("%1%2 %3").arg(colShort, meta, roleTag);
}

} // namespace

namespace {
QIcon buildReferenceLineSwatchIcon(const Graph::ReferenceLineSpec &spec)
{
    QPixmap px(22, 14);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor col = spec.color.isValid() ? spec.color : QColor(0x33, 0x41, 0x55);
    const Qt::PenStyle ps = (spec.style == Qt::NoPen) ? Qt::DashLine : spec.style;
    QPen pen(col, qBound(1.2, spec.width, 2.6), ps);
    p.setPen(pen);
    if (spec.orientation == 1)
        p.drawLine(QPointF(11.0, 1.5), QPointF(11.0, 12.5));
    else
        p.drawLine(QPointF(1.5, 7.0), QPointF(20.5, 7.0));
    p.end();
    return QIcon(px);
}

QString formatReferenceLineTreeLabel(const Graph::ReferenceLineSpec &spec)
{
    const QString statMode = spec.statMode.trimmed();
    const QString lbl = stripHtmlToCleanUnicode(spec.label).trimmed();
    const QString axisVar = (spec.orientation == 1) ? QStringLiteral("x") : QStringLiteral("y");
    const QString valTxt = QLocale::c().toString(spec.value, 'g', 5);
    QString base = QStringLiteral("%1 = %2").arg(axisVar, valTxt);
    if (!statMode.isEmpty() && statMode.compare(QLatin1String("Constant"), Qt::CaseInsensitive) != 0)
        base += QStringLiteral(" (%1)").arg(statMode);
    if (!lbl.isEmpty() && lbl != base)
        base += QStringLiteral(" - %1").arg(lbl);
    return base;
}
} // namespace

bool GraphInspectorDock::hasZeroReferenceLine(const Graph *g, int orientation)
{
    if (!g)
        return false;
    const int wantOrient = (orientation == 1) ? 1 : 0;
    const QVector<Graph::ReferenceLineSpec> lines = g->referenceLines();
    for (const Graph::ReferenceLineSpec &spec : lines) {
        const QString sm = spec.statMode.trimmed();
        const bool isConst = sm.isEmpty()
                || sm.compare(QLatin1String("Constant"), Qt::CaseInsensitive) == 0;
        if (spec.visible && spec.orientation == wantOrient && isConst
            && std::fabs(spec.value) <= 1e-12 && spec.style != Qt::NoPen) {
            return true;
        }
    }
    return false;
}

void GraphInspectorDock::toggleZeroReferenceLine(Graph *g, int orientation)
{
    if (!g)
        return;
    const int wantOrient = (orientation == 1) ? 1 : 0;
    MultiLayer *ml = g->multiLayerParent();
    const QVector<Graph::ReferenceLineSpec> lines = g->referenceLines();
    int foundIdx = -1;
    for (int i = 0; i < lines.size(); ++i) {
        const Graph::ReferenceLineSpec &spec = lines[i];
        const QString sm = spec.statMode.trimmed();
        const bool isConst = sm.isEmpty()
                || sm.compare(QLatin1String("Constant"), Qt::CaseInsensitive) == 0;
        if (spec.orientation == wantOrient && isConst && std::fabs(spec.value) <= 1e-12) {
            foundIdx = i;
            break;
        }
    }

    if (foundIdx >= 0) {
        if (lines[foundIdx].visible && lines[foundIdx].style != Qt::NoPen) {
            g->removeReferenceLine(foundIdx);
        } else {
            const QString before = ml ? ml->captureUndoSnapshot() : QString();
            Graph::ReferenceLineSpec spec = lines[foundIdx];
            spec.visible = true;
            if (spec.style == Qt::NoPen)
                spec.style = Qt::DashLine;
            g->updateReferenceLine(foundIdx, spec);
            if (ml && !before.isEmpty()) {
                ml->pushUndoSnapshot(wantOrient == 0 ? tr("Add y = 0 Reference Line")
                                                     : tr("Add x = 0 Reference Line"),
                                     before);
            }
        }
        return;
    }

    const QString before = ml ? ml->captureUndoSnapshot() : QString();
    Graph::ReferenceLineSpec spec;
    spec.orientation = wantOrient;
    spec.value = 0.0;
    spec.statMode.clear();
    spec.color = QColor(0x33, 0x41, 0x55);
    spec.style = Qt::DashLine;
    spec.width = 1.2;
    spec.label.clear();
    spec.fillToAxis = false;
    g->addReferenceLine(spec);
    if (ml && !before.isEmpty()) {
        ml->pushUndoSnapshot(wantOrient == 0 ? tr("Add y = 0 Reference Line")
                                             : tr("Add x = 0 Reference Line"),
                             before);
    }
}

void GraphInspectorDock::addReferenceLineInteractive(Graph *g, int defaultOrientation)
{
    if (!g)
        return;
    QPointer<Graph> safeG(g);
    QPointer<MultiLayer> safeMl(g->multiLayerParent());
    const int defOrient = (defaultOrientation == 1) ? 1 : 0;

    if (ApplicationWindow::isNonInteractiveContext(g)) {
        const QString before = safeMl ? safeMl->captureUndoSnapshot() : QString();
        Graph::ReferenceLineSpec spec;
        spec.orientation = defOrient;
        spec.value = 0.0;
        spec.color = QColor(0x33, 0x41, 0x55);
        spec.style = Qt::DashLine;
        spec.width = 1.2;
        safeG->addReferenceLine(spec);
        if (safeMl && !before.isEmpty())
            safeMl->pushUndoSnapshot(tr("Add Reference Line"), before);
        return;
    }

    QDialog dlg(g->window());
    dlg.setWindowTitle(tr("Add Reference Line"));
    dlg.setModal(true);

    QVBoxLayout *mainLay = new QVBoxLayout(&dlg);
    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);

    QComboBox *orientBox = new QComboBox(&dlg);
    orientBox->addItem(QCoreApplication::translate("Graph", "Horizontal (Y = const)"), 0);
    orientBox->addItem(QCoreApplication::translate("Graph", "Vertical (X = const)"), 1);
    orientBox->setCurrentIndex(defOrient);

    QComboBox *modeBox = new QComboBox(&dlg);
    modeBox->addItem(tr("Constant Value"), QString());
    modeBox->addItem(tr("Mean"), QStringLiteral("Mean"));
    modeBox->addItem(tr("Median"), QStringLiteral("Median"));
    modeBox->addItem(tr("Mean + SD"), QStringLiteral("Mean+SD"));
    modeBox->addItem(tr("Mean - SD"), QStringLiteral("Mean-SD"));

    QLineEdit *valEdit = new QLineEdit(QStringLiteral("0"), &dlg);
    connect(modeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [modeBox, valEdit](int idx) {
                valEdit->setEnabled(modeBox->itemData(idx).toString().isEmpty());
            });

    QLineEdit *labelEdit = new QLineEdit(&dlg);
    labelEdit->setPlaceholderText(
            QCoreApplication::translate("Graph", "Optional label (e.g. Y = 0)"));

    QComboBox *styleBox = new QComboBox(&dlg);
    styleBox->addItem(QCoreApplication::translate("Graph", "Solid"), int(Qt::SolidLine));
    styleBox->addItem(QCoreApplication::translate("Graph", "Dash"), int(Qt::DashLine));
    styleBox->addItem(QCoreApplication::translate("Graph", "Dot"), int(Qt::DotLine));
    styleBox->addItem(QCoreApplication::translate("Graph", "Dash-Dot"), int(Qt::DashDotLine));
    styleBox->setCurrentIndex(1);

    QDoubleSpinBox *widthSpin = new QDoubleSpinBox(&dlg);
    widthSpin->setRange(0.5, 12.0);
    widthSpin->setSingleStep(0.5);
    widthSpin->setDecimals(1);
    widthSpin->setValue(1.2);
    widthSpin->setSuffix(QStringLiteral(" pt"));

    QColor chosenColor(0x33, 0x41, 0x55);
    QPushButton *colorBtn =
            new QPushButton(QCoreApplication::translate("Graph", "Choose Color..."), &dlg);
    auto updateColorBtnStyle = [colorBtn](const QColor &c) {
        colorBtn->setStyleSheet(
                QStringLiteral("QPushButton { background-color: %1; color: %2; padding: 4px 10px; "
                               "border: 1px solid #888; border-radius: 3px; }")
                        .arg(c.name(),
                             c.lightness() < 140 ? QStringLiteral("#ffffff")
                                                 : QStringLiteral("#111111")));
    };
    updateColorBtnStyle(chosenColor);
    connect(colorBtn, &QPushButton::clicked, &dlg, [&dlg, &chosenColor, updateColorBtnStyle]() {
        const QColor c = pickColorInteractive(
                &dlg, chosenColor, QCoreApplication::translate("Graph", "Reference Line Color"));
        if (c.isValid()) {
            chosenColor = c;
            updateColorBtnStyle(chosenColor);
        }
    });

    QCheckBox *fillCheck = new QCheckBox(
            QCoreApplication::translate("Graph", "Shade to zero / axis baseline"), &dlg);

    grid->addWidget(new QLabel(QCoreApplication::translate("Graph", "Orientation:"), &dlg), 0, 0);
    grid->addWidget(orientBox, 0, 1);
    grid->addWidget(new QLabel(tr("Position Mode:"), &dlg), 1, 0);
    grid->addWidget(modeBox, 1, 1);
    grid->addWidget(new QLabel(QCoreApplication::translate("Graph", "Value:"), &dlg), 2, 0);
    grid->addWidget(valEdit, 2, 1);
    grid->addWidget(new QLabel(QCoreApplication::translate("Graph", "Label:"), &dlg), 3, 0);
    grid->addWidget(labelEdit, 3, 1);
    grid->addWidget(new QLabel(QCoreApplication::translate("Graph", "Line Style:"), &dlg), 4, 0);
    grid->addWidget(styleBox, 4, 1);
    grid->addWidget(new QLabel(QCoreApplication::translate("Graph", "Line Width:"), &dlg), 5, 0);
    grid->addWidget(widthSpin, 5, 1);
    grid->addWidget(new QLabel(QCoreApplication::translate("Graph", "Color:"), &dlg), 6, 0);
    grid->addWidget(colorBtn, 6, 1);
    grid->addWidget(fillCheck, 7, 0, 1, 2);
    mainLay->addLayout(grid);

    QDialogButtonBox *bbox =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    mainLay->addWidget(bbox);

    if (dlg.exec() != QDialog::Accepted || !safeG)
        return;

    const QString statMode = modeBox->currentData().toString();
    double val = 0.0;
    if (statMode.isEmpty()) {
        bool ok = false;
        val = QLocale::c().toDouble(valEdit->text().trimmed(), &ok);
        if (!ok)
            val = QLocale().toDouble(valEdit->text().trimmed(), &ok);
        if (!ok || !std::isfinite(val))
            return;
    }

    if (!safeMl)
        safeMl = safeG->multiLayerParent();
    const QString before = safeMl ? safeMl->captureUndoSnapshot() : QString();
    Graph::ReferenceLineSpec spec;
    spec.orientation = orientBox->currentData().toInt();
    spec.value = val;
    spec.statMode = statMode;
    spec.label = labelEdit->text().trimmed();
    spec.style = static_cast<Qt::PenStyle>(styleBox->currentData().toInt());
    spec.width = widthSpin->value();
    spec.color = chosenColor;
    spec.fillToAxis = fillCheck->isChecked();
    QColor fillCol = chosenColor;
    fillCol.setAlpha(32);
    spec.fillColor = fillCol;
    safeG->addReferenceLine(spec);
    const int addedIdx = safeG->referenceLines().size() - 1;
    if (addedIdx >= 0)
        safeG->setCurrentSelection({ Graph::GraphSelection::ReferenceLine, addedIdx, -1 });
    if (safeMl && !before.isEmpty())
        safeMl->pushUndoSnapshot(tr("Add Reference Line"), before);
    if (ApplicationWindow *app = qobject_cast<ApplicationWindow *>(safeG->window())) {
        if (GraphInspectorDock *dock = app->graphInspectorDock())
            dock->refreshAll();
    }
}

void GraphInspectorDock::refreshAll(bool allowTabSwitch)
{
    if (d_coalesce_refresh_timer && d_coalesce_refresh_timer->isActive())
        d_coalesce_refresh_timer->stop();

    const bool hasGraph = !d_target_ml.isNull();
    if (d_header_target_lbl) {
        if (!hasGraph) {
            d_header_target_lbl->setText(tr("(No Active Graph)"));
            d_header_target_lbl->setToolTip(QString());
        } else {
            const QString winLbl = d_target_ml->windowLabel().trimmed();
            const QString winName = d_target_ml->name();
            const bool hasExtraLbl = !winLbl.isEmpty() && winLbl != winName && winLbl.length() <= 48
                    && !winLbl.contains(QLatin1Char('/')) && !winLbl.contains(QLatin1Char('\\'));
            d_header_target_lbl->setText(winName);
            d_header_target_lbl->setToolTip(
                    hasExtraLbl ? QStringLiteral("%1 — %2").arg(winName, winLbl) : winName);
        }
    }
    if (d_scope_strip) {
        const bool multiPanel = hasGraph && d_target_ml->layers() > 1;
        d_scope_strip->setVisible(multiPanel);
        if (d_btn_scope_active && d_btn_scope_all) {
            QSignalBlocker b1(d_btn_scope_active);
            QSignalBlocker b2(d_btn_scope_all);
            d_btn_scope_active->setChecked(!d_scope_all_panels);
            d_btn_scope_all->setChecked(d_scope_all_panels);
        }
    }
    if (d_content_stack) {
        d_content_stack->setCurrentIndex(hasGraph ? 1 : 0);
        d_content_stack->updateGeometry();
    }
    if (d_prop_stack)
        d_prop_stack->setEnabled(hasGraph);
    if (d_tree_header_strip)
        d_tree_header_strip->setEnabled(hasGraph);

    if (!hasGraph)
        updateQuickStartPlotButtonState();

    refreshPaletteSchemeCombo();
    refreshObjectTree();
    syncInspectorFromSelection(allowTabSwitch);
}

void GraphInspectorDock::refreshObjectTree()
{
    if (!d_object_tree)
        return;

    if (d_scope_strip)
        d_scope_strip->setVisible(d_target_ml && d_target_ml->layers() > 1);

    d_syncing_tree = true;
    QSignalBlocker blocker(d_object_tree);
    d_object_tree->clear();

    if (!d_target_ml) {
        d_object_tree->hide();
        if (d_empty_hint_lbl)
            d_empty_hint_lbl->show();
        d_syncing_tree = false;
        return;
    }

    if (d_empty_hint_lbl)
        d_empty_hint_lbl->hide();
    d_object_tree->show();

    Graph *activeG = activeLayer();
    const Graph::GraphSelection activeSel =
            activeG ? activeG->currentSelection() : Graph::GraphSelection();
    const int activeMasterCurveIdx =
            (activeG && (activeSel.kind == Graph::GraphSelection::Curve
                         || activeSel.kind == Graph::GraphSelection::ErrorBars))
            ? resolveMasterCurveIndex(activeG, activeSel.primaryId)
            : -1;

    QTreeWidgetItem *pageItem = new QTreeWidgetItem(d_object_tree);
    pageItem->setText(0, tr("Page: %1").arg(d_target_ml->name()));
    pageItem->setIcon(0, IconLoader::duotoneIcon(QStringLiteral("page-setup"), 14));
    pageItem->setData(0, RoleSelKind, int(Graph::GraphSelection::Page));
    pageItem->setData(0, RoleLayerIndex, 0);
    pageItem->setExpanded(true);

    QTreeWidgetItem *itemToSelect = nullptr;
    if (activeSel.kind == Graph::GraphSelection::Page)
        itemToSelect = pageItem;

    const QList<Graph *> layers = d_target_ml->layersList();
    for (int li = 0; li < layers.size(); ++li) {
        Graph *g = layers[li];
        if (!g)
            continue;
        bindLayerSignals(g);

        QString layerTitle = tr("Layer %1").arg(li + 1);
        if (d_target_ml->isInsetLayer(g))
            layerTitle += QStringLiteral(" ") + tr("[Inset]");
        else if (g->linkedParentLayer())
            layerTitle += QStringLiteral(" ") + tr("[Linked]");
        QTreeWidgetItem *layerItem = new QTreeWidgetItem(pageItem);
        layerItem->setText(0, layerTitle);
        layerItem->setIcon(0, IconLoader::duotoneIcon(
                d_target_ml->isInsetLayer(g) ? QStringLiteral("layer-inset")
                                             : QStringLiteral("layer-arrange"),
                14));
        layerItem->setData(0, RoleSelKind, int(Graph::GraphSelection::Layer));
        layerItem->setData(0, RoleLayerIndex, li + 1);
        layerItem->setExpanded(true);
        if (g == activeG) {
            QFont bf = layerItem->font(0);
            bf.setBold(true);
            layerItem->setFont(0, bf);
            if (!itemToSelect
                && (activeSel.kind == Graph::GraphSelection::None
                    || activeSel.kind == Graph::GraphSelection::Layer)) {
                itemToSelect = layerItem;
            }
        }

        // Plot Title item (if non-empty) so users can select & edit it directly (A3-5)
        if (g->plotWidget()) {
            const QString rawPlotTitle = g->plotWidget()->title().text().trimmed();
            if (!rawPlotTitle.isEmpty()) {
                QString cleanTitle = stripHtmlToCleanUnicode(rawPlotTitle).simplified();
                if (cleanTitle.size() > 24)
                    cleanTitle = cleanTitle.left(22) + QStringLiteral("...");
                QTreeWidgetItem *ptItem = new QTreeWidgetItem(layerItem);
                ptItem->setText(0, tr("Plot Title: %1").arg(cleanTitle));
                ptItem->setIcon(0, IconLoader::duotoneIcon(QStringLiteral("text-label"), 14));
                ptItem->setData(0, RoleSelKind, int(Graph::GraphSelection::PlotTitle));
                ptItem->setData(0, RolePrimaryId, 0);
                ptItem->setData(0, RoleLayerIndex, li + 1);
                if (g == activeG && activeSel.kind == Graph::GraphSelection::PlotTitle)
                    itemToSelect = ptItem;
            }
        }

        // Curves Group
        int dataCurveCount = 0;
        for (int c = 0; c < g->curves(); ++c) {
            if (g->curveType(c) != Graph::ErrorBars)
                ++dataCurveCount;
        }
        QTreeWidgetItem *curvesGroup = new QTreeWidgetItem(layerItem);
        curvesGroup->setText(0, tr("Curves (%1)").arg(dataCurveCount));
        curvesGroup->setData(0, RoleSelKind, int(Graph::GraphSelection::Curve));
        curvesGroup->setData(0, RolePrimaryId, 0);
        curvesGroup->setData(0, RoleLayerIndex, li + 1);
        curvesGroup->setData(0, RoleIsGroupHeader, true);
        curvesGroup->setExpanded(true);

        for (int ci = 0; ci < g->curves(); ++ci) {
            if (auto *sp = dynamic_cast<Spectrogram *>(g->plotItem(ci))) {
                QTreeWidgetItem *spItem = new QTreeWidgetItem(curvesGroup);
                const QString matName = stripHtmlToCleanUnicode(
                        sp->matrix() ? QString(sp->matrix()->name())
                                     : sp->title().text().trimmed());
                spItem->setText(0, matName.isEmpty() ? tr("Heatmap")
                                                     : tr("Heatmap: %1").arg(matName));
                spItem->setIcon(0, buildCurveSwatchIcon(g, ci));
                spItem->setFlags(spItem->flags() | Qt::ItemIsUserCheckable);
                spItem->setCheckState(0, sp->isVisible() ? Qt::Checked : Qt::Unchecked);
                spItem->setData(0, RoleSelKind, int(Graph::GraphSelection::Spectrogram));
                spItem->setData(0, RolePrimaryId, ci);
                spItem->setData(0, RoleLayerIndex, li + 1);
                if (g == activeG && activeSel.kind == Graph::GraphSelection::Spectrogram
                    && activeSel.primaryId == ci) {
                    itemToSelect = spItem;
                }
                continue;
            }
            if (g->curveType(ci) == Graph::ErrorBars)
                continue;
            QTreeWidgetItem *cvItem = new QTreeWidgetItem(curvesGroup);
            QString cvLabel = friendlyCurveDisplayName(g, ci);
            const PlotCurve *pc = dynamic_cast<const PlotCurve *>(g->plotItem(ci));
            if (g->curveType(ci) == Graph::Function || (pc && pc->type() == Graph::Function)
                || dynamic_cast<const FunctionCurve *>(g->plotItem(ci)) != nullptr)
                cvLabel += tr("  [fx]");
            cvItem->setText(0, cvLabel);
            cvItem->setIcon(0, buildCurveSwatchIcon(g, ci));
            cvItem->setFlags(cvItem->flags() | Qt::ItemIsUserCheckable);
            cvItem->setCheckState(0, g->isCurveVisible(ci) ? Qt::Checked : Qt::Unchecked);
            cvItem->setData(0, RoleSelKind, int(Graph::GraphSelection::Curve));
            cvItem->setData(0, RolePrimaryId, ci);
            cvItem->setData(0, RoleLayerIndex, li + 1);
            if (g == activeG && activeSel.kind == Graph::GraphSelection::Curve
                && ci == activeMasterCurveIdx) {
                itemToSelect = cvItem;
            }

            // Show attached QwtErrorPlotCurve items under their parent curve
            if (auto *dc = dynamic_cast<DataCurve *>(g->curve(ci))) {
                for (DataCurve *eb : dc->errorBarsList()) {
                    auto *er = dynamic_cast<QwtErrorPlotCurve *>(eb);
                    const int ebIdx = er ? g->curveIndex(er) : -1;
                    if (!er || ebIdx < 0)
                        continue;
                    QTreeWidgetItem *ebItem = new QTreeWidgetItem(cvItem);
                    QString colShort = er->yColumnName().trimmed();
                    if (er->table()) {
                        const QString pfx = QString(er->table()->name()) + QStringLiteral("_");
                        if (colShort.startsWith(pfx) && colShort.size() > pfx.size())
                            colShort = colShort.mid(pfx.size());
                    }
                    if (colShort.isEmpty())
                        colShort = er->title().text();
                    ebItem->setText(0, (er->direction() == QwtErrorPlotCurve::Horizontal
                                                ? tr("±X Err: %1")
                                                : tr("±Y Err: %1"))
                                               .arg(colShort));
                    ebItem->setIcon(0, buildCurveSwatchIcon(g, ebIdx));
                    ebItem->setFlags(ebItem->flags() | Qt::ItemIsUserCheckable);
                    ebItem->setCheckState(0, g->isCurveVisible(ebIdx) ? Qt::Checked
                                                                      : Qt::Unchecked);
                    ebItem->setData(0, RoleSelKind, int(Graph::GraphSelection::ErrorBars));
                    ebItem->setData(0, RolePrimaryId, ebIdx);
                    ebItem->setData(0, RoleLayerIndex, li + 1);
                    cvItem->setExpanded(true);
                    if (g == activeG && activeSel.kind == Graph::GraphSelection::ErrorBars
                        && activeSel.primaryId == ebIdx) {
                        itemToSelect = ebItem;
                    }
                }
            }
        }

        // Legend Item
        QTreeWidgetItem *legItem = new QTreeWidgetItem(layerItem);
        legItem->setText(0, tr("Legend"));
        legItem->setIcon(0, IconLoader::duotoneIcon(QStringLiteral("legend"), 14));
        legItem->setFlags(legItem->flags() | Qt::ItemIsUserCheckable);
        const bool legVis = g->hasLegend() && g->isLegendVisible();
        legItem->setCheckState(0, legVis ? Qt::Checked : Qt::Unchecked);
        legItem->setData(0, RoleSelKind, int(Graph::GraphSelection::LegendBox));
        legItem->setData(0, RoleLayerIndex, li + 1);
        if (g == activeG && activeSel.kind == Graph::GraphSelection::LegendBox)
            itemToSelect = legItem;

        // Reference Lines
        const QVector<Graph::ReferenceLineSpec> refLines = g->referenceLines();
        if (!refLines.isEmpty()) {
            QTreeWidgetItem *refGroup = new QTreeWidgetItem(layerItem);
            refGroup->setText(0, tr("Reference Lines (%1)").arg(refLines.size()));
            refGroup->setData(0, RoleSelKind, int(Graph::GraphSelection::ReferenceLine));
            refGroup->setData(0, RoleLayerIndex, li + 1);
            refGroup->setData(0, RoleIsGroupHeader, true);
            refGroup->setExpanded(true);

            for (int ri = 0; ri < refLines.size(); ++ri) {
                const Graph::ReferenceLineSpec &rspec = refLines[ri];
                QTreeWidgetItem *rItem = new QTreeWidgetItem(refGroup);
                rItem->setText(0, formatReferenceLineTreeLabel(rspec));
                rItem->setIcon(0, buildReferenceLineSwatchIcon(rspec));
                rItem->setFlags(rItem->flags() | Qt::ItemIsUserCheckable);
                rItem->setCheckState(0, (rspec.visible && rspec.style != Qt::NoPen) ? Qt::Checked
                                                                                    : Qt::Unchecked);
                rItem->setData(0, RoleSelKind, int(Graph::GraphSelection::ReferenceLine));
                rItem->setData(0, RolePrimaryId, ri);
                rItem->setData(0, RoleLayerIndex, li + 1);
                if (g == activeG && activeSel.kind == Graph::GraphSelection::ReferenceLine
                    && activeSel.primaryId == ri) {
                    itemToSelect = rItem;
                }
            }
        }

        // Annotations (Text, Arrow, Image)
        const Legend *mainLegend = g->legend();
        QVector<int> textIds;
        for (int tid : g->textMarkerKeys()) {
            Legend *tm = g->textMarker(tid);
            if (tm && tm != mainLegend)
                textIds.append(tid);
        }
        const QVector<int> arrowIds = g->lineMarkerKeys();
        const QVector<int> imageIds = g->imageMarkerKeys();
        const int totalAnn = textIds.size() + arrowIds.size() + imageIds.size();

        if (totalAnn > 0) {
            QTreeWidgetItem *annGroup = new QTreeWidgetItem(layerItem);
            annGroup->setText(0, tr("Annotations (%1)").arg(totalAnn));
            annGroup->setData(0, RoleSelKind, int(Graph::GraphSelection::LegendBox));
            annGroup->setData(0, RoleLayerIndex, li + 1);
            annGroup->setData(0, RoleIsGroupHeader, true);
            annGroup->setExpanded(true);

            for (int tid : textIds) {
                Legend *tm = g->textMarker(tid);
                QString snippet = tm ? stripHtmlToCleanUnicode(tm->text()).simplified() : QString();
                if (snippet.size() > 22)
                    snippet = snippet.left(20) + QStringLiteral("...");
                QTreeWidgetItem *tItem = new QTreeWidgetItem(annGroup);
                tItem->setText(0, snippet.isEmpty() ? tr("Text")
                                                    : tr("Text: %1").arg(snippet));
                tItem->setData(0, RoleSelKind, int(Graph::GraphSelection::TextAnnotation));
                tItem->setData(0, RolePrimaryId, tid);
                tItem->setData(0, RoleLayerIndex, li + 1);
                if (g == activeG && activeSel.kind == Graph::GraphSelection::TextAnnotation
                    && activeSel.primaryId == tid) {
                    itemToSelect = tItem;
                }
            }
            for (int k = 0; k < arrowIds.size(); ++k) {
                const int aid = arrowIds[k];
                QTreeWidgetItem *aItem = new QTreeWidgetItem(annGroup);
                aItem->setText(0, tr("Arrow %1").arg(k + 1));
                aItem->setData(0, RoleSelKind, int(Graph::GraphSelection::ArrowAnnotation));
                aItem->setData(0, RolePrimaryId, aid);
                aItem->setData(0, RoleLayerIndex, li + 1);
                if (g == activeG && activeSel.kind == Graph::GraphSelection::ArrowAnnotation
                    && activeSel.primaryId == aid) {
                    itemToSelect = aItem;
                }
            }
            for (int k = 0; k < imageIds.size(); ++k) {
                const int iid = imageIds[k];
                ImageMarker *im = g->imageMarker(iid);
                const QString base = im ? QFileInfo(im->fileName()).fileName() : QString();
                QTreeWidgetItem *iItem = new QTreeWidgetItem(annGroup);
                iItem->setText(0, base.isEmpty() ? tr("Image %1").arg(k + 1)
                                                 : tr("Image: %1").arg(base));
                iItem->setData(0, RoleSelKind, int(Graph::GraphSelection::ImageAnnotation));
                iItem->setData(0, RolePrimaryId, iid);
                iItem->setData(0, RoleLayerIndex, li + 1);
                if (g == activeG && activeSel.kind == Graph::GraphSelection::ImageAnnotation
                    && activeSel.primaryId == iid) {
                    itemToSelect = iItem;
                }
            }
        }
    }

    if (itemToSelect)
        d_object_tree->setCurrentItem(itemToSelect);

    d_syncing_tree = false;
}

void GraphInspectorDock::onTreeItemClicked(QTreeWidgetItem *item, int)
{
    if (d_syncing_tree || !item || !d_target_ml)
        return;

    // Extract all needed fields from `item` BEFORE flushing pending edits, because
    // flushPendingAnnotationEdit() calls refreshObjectTree() -> d_object_tree->clear(),
    // freeing every QTreeWidgetItem* including `item`.
    const int layerOneBased = item->data(0, RoleLayerIndex).toInt();
    const auto kind =
            static_cast<Graph::GraphSelection::Kind>(item->data(0, RoleSelKind).toInt());
    const int primaryId = item->data(0, RolePrimaryId).toInt();
    const bool isGroupHeader = item->data(0, RoleIsGroupHeader).toBool();

    // E5-2: flush any pending debounced edits before switching selection or layer.
    const bool flushedAnnot = d_annot_commit_timer && d_annot_commit_timer->isActive();
    if (flushedAnnot)
        flushPendingAnnotationEdit();
    if (d_axis_title_timer && d_axis_title_timer->isActive())
        flushPendingAxisTitleEdit();

    if (!d_target_ml)
        return;

    if (GraphMiniToolbar *mt = GraphMiniToolbar::instance())
        mt->cancelScheduledAndHide();

    if (layerOneBased >= 1) {
        if (Graph *targetG = d_target_ml->layer(layerOneBased)) {
            if (d_target_ml->activeGraph() != targetG)
                d_target_ml->setActiveGraph(targetG);
        }
    }

    Graph *g = activeLayer();

    if (g && !isGroupHeader) {
        Graph::GraphSelection sel;
        sel.kind = kind;
        sel.primaryId = primaryId;
        g->setCurrentSelection(sel, false);
    }

    if (flushedAnnot && !isGroupHeader)
        refreshObjectTree();

    switch (kind) {
    case Graph::GraphSelection::Page:
    case Graph::GraphSelection::Layer:
        switchToInspectorPage(PageTabPageLayer);
        syncPageLayerControls();
        break;
    case Graph::GraphSelection::Axis:
    case Graph::GraphSelection::AxisTitle:
        d_active_axis = qBound(0, primaryId, 3);
        switchToInspectorPage(PageTabAxis);
        syncAxisControls();
        break;
    case Graph::GraphSelection::ReferenceLine:
        if (g && !isGroupHeader && primaryId >= 0 && primaryId < g->referenceLines().size()) {
            const int orient = g->referenceLines()[primaryId].orientation;
            d_active_axis = (orient == 1) ? int(QwtPlot::xBottom) : int(QwtPlot::yLeft);
        }
        switchToInspectorPage(PageTabAxis);
        syncAxisControls();
        break;
    case Graph::GraphSelection::Curve:
    case Graph::GraphSelection::ErrorBars:
    case Graph::GraphSelection::Spectrogram:
        switchToInspectorPage(PageTabCurve);
        syncCurveControls();
        break;
    case Graph::GraphSelection::PlotTitle:
    case Graph::GraphSelection::LegendBox:
    case Graph::GraphSelection::TextAnnotation:
    case Graph::GraphSelection::ArrowAnnotation:
    case Graph::GraphSelection::ImageAnnotation:
        switchToInspectorPage(PageTabLegend);
        syncLegendControls();
        break;
    default:
        syncInspectorFromSelection();
        break;
    }
}

bool GraphInspectorDock::ensureTargetMultiLayerActive(QPointer<MultiLayer> &safeMl)
{
    if (!d_app || !safeMl)
        return false;
    if (d_app->d_workspace.activeSubWindow() != safeMl.data()) {
        d_app->d_workspace.setActiveSubWindow(safeMl.data());
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    return !safeMl.isNull() && d_app && d_app->d_workspace.activeSubWindow() == safeMl.data();
}

void GraphInspectorDock::onTreeItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (d_syncing_tree || !item || !d_target_ml || !d_app
        || ApplicationWindow::isNonInteractiveContext(d_app)) {
        return;
    }

    // Extract all needed fields from `item` BEFORE onTreeItemClicked(), which may flush
    // pending edits and call refreshObjectTree() -> d_object_tree->clear(), freeing `item`.
    const auto kind =
            static_cast<Graph::GraphSelection::Kind>(item->data(0, RoleSelKind).toInt());
    const int primaryId = item->data(0, RolePrimaryId).toInt();
    const bool isGroupHeader = item->data(0, RoleIsGroupHeader).toBool();

    onTreeItemClicked(item, column);

    QPointer<MultiLayer> safeMl(d_target_ml);
    QPointer<Graph> safeG(activeLayer());
    if (!ensureTargetMultiLayerActive(safeMl))
        return;
    Graph *g = safeG ? safeG.data() : activeLayer();

    // Same rules as a double-click on the canvas (CanvasPicker): text-like elements (axis
    // title, legend, text) are edited in place; curves, axes and the layer open their dialog.
    auto globalRectOf = [](QWidget *w, const QRect &local) {
        return QRect(w->mapToGlobal(local.topLeft()), local.size());
    };

    switch (kind) {
    case Graph::GraphSelection::Page:
        d_app->showPageSetupDialog();
        break;
    case Graph::GraphSelection::Layer:
        d_app->showGeneralPlotDialog();
        break;
    case Graph::GraphSelection::PlotTitle:
        d_app->showTitleDialog();
        break;
    case Graph::GraphSelection::Axis:
        openAxesDialogForQwtAxis(g, qBound(0, primaryId, 3), true);
        break;
    case Graph::GraphSelection::ReferenceLine:
        if (!isGroupHeader && g && primaryId >= 0 && primaryId < g->referenceLines().size()) {
            QPointer<GraphInspectorDock> self(this);
            if (g->editReferenceLineDialog(primaryId) && self)
                self->refreshAll();
        }
        break;
    case Graph::GraphSelection::AxisTitle:
        if (g && g->plotWidget()) {
            if (QwtScaleWidget *sw = g->plotWidget()->axisWidget(qBound(0, primaryId, 3))) {
                CanvasInlineTextEditor::openForAxisTitle(g, qBound(0, primaryId, 3),
                                                         globalRectOf(sw, sw->rect()));
                break;
            }
        }
        openAxesDialogForQwtAxis(g, qBound(0, primaryId, 3), true);
        break;
    case Graph::GraphSelection::Curve:
    case Graph::GraphSelection::ErrorBars:
    case Graph::GraphSelection::Spectrogram:
        if (!isGroupHeader)
            d_app->showPlotDialog(g ? int(g->curveKey(primaryId)) : primaryId);
        else
            d_app->showCurvesDialog();
        break;
    case Graph::GraphSelection::LegendBox:
        if (!isGroupHeader && g && g->legend() && g->plotWidget()) {
            long legendKey = -1;
            for (int k : g->textMarkerKeys()) {
                if (g->textMarker(k) == g->legend()) {
                    legendKey = k;
                    break;
                }
            }
            if (legendKey >= 0) {
                QWidget *canvas = g->plotWidget()->canvas();
                CanvasInlineTextEditor::openForLegendOrText(
                        g, legendKey, -1, globalRectOf(canvas, g->legend()->rect()));
            }
        }
        break;
    case Graph::GraphSelection::TextAnnotation:
        if (g && g->plotWidget()) {
            if (Legend *tm = g->textMarker(primaryId)) {
                QWidget *canvas = g->plotWidget()->canvas();
                CanvasInlineTextEditor::openForLegendOrText(g, primaryId, -1,
                                                            globalRectOf(canvas, tm->rect()));
            }
        }
        break;
    case Graph::GraphSelection::ArrowAnnotation:
        if (g) {
            g->setSelectedMarker(primaryId);
            d_app->showLineDialog();
        }
        break;
    case Graph::GraphSelection::ImageAnnotation:
        if (g) {
            g->setSelectedMarker(primaryId);
            d_app->showImageDialog();
        }
        break;
    default:
        break;
    }
}

void GraphInspectorDock::openDetailsDialogForPage()
{
    if (!d_app || !d_target_ml || ApplicationWindow::isNonInteractiveContext(d_app))
        return;
    QPointer<MultiLayer> safeMl(d_target_ml);
    QPointer<Graph> safeG(activeLayer());
    const int page = d_prop_stack ? d_prop_stack->currentIndex() : int(PageTabPageLayer);
    if (!ensureTargetMultiLayerActive(safeMl))
        return;
    Graph *g = safeG ? safeG.data() : activeLayer();
    const Graph::GraphSelection sel = g ? g->currentSelection() : Graph::GraphSelection();
    switch (page) {
    case PageTabAxis:
        if (g && sel.kind == Graph::GraphSelection::ReferenceLine && sel.primaryId >= 0
            && sel.primaryId < g->referenceLines().size()) {
            QPointer<GraphInspectorDock> self(this);
            if (g->editReferenceLineDialog(sel.primaryId) && self)
                self->refreshAll();
        } else {
            openAxesDialogForQwtAxis(g, qBound(0, d_active_axis, 3), true);
        }
        break;
    case PageTabCurve: {
        int idx = -1;
        if (g) {
            if ((sel.kind == Graph::GraphSelection::Curve
                 || sel.kind == Graph::GraphSelection::ErrorBars
                 || sel.kind == Graph::GraphSelection::Spectrogram)
                && sel.primaryId >= 0 && sel.primaryId < g->curves())
                idx = sel.primaryId;
            else if (g->curves() > 0)
                idx = 0;
        }
        d_app->showPlotDialog((g && idx >= 0) ? int(g->curveKey(idx)) : -1);
        break;
    }
    case PageTabLegend:
        if (g && sel.kind == Graph::GraphSelection::PlotTitle) {
            d_app->showTitleDialog();
        } else if (g && sel.kind == Graph::GraphSelection::ArrowAnnotation && sel.primaryId >= 0) {
            g->setSelectedMarker(sel.primaryId);
            d_app->showLineDialog();
        } else if (g && sel.kind == Graph::GraphSelection::ImageAnnotation && sel.primaryId >= 0) {
            g->setSelectedMarker(sel.primaryId);
            d_app->showImageDialog();
        } else if (g && sel.kind == Graph::GraphSelection::TextAnnotation && sel.primaryId >= 0) {
            g->setSelectedMarker(sel.primaryId);
            d_app->showTextDialog();
        } else if (g && g->legend()) {
            for (int k : g->textMarkerKeys()) {
                if (g->textMarker(k) == g->legend()) {
                    g->setSelectedMarker(k);
                    break;
                }
            }
            d_app->showTextDialog();
        } else if (g) {
            const QString before = safeMl->captureUndoSnapshot();
            g->newLegend();
            if (g->legend()) {
                safeMl->pushUndoSnapshot(tr("Add Legend"), before);
                for (int k : g->textMarkerKeys()) {
                    if (g->textMarker(k) == g->legend()) {
                        g->setSelectedMarker(k);
                        break;
                    }
                }
                d_app->showTextDialog();
            }
        }
        break;
    case PageTabPageLayer:
    default:
        if (sel.kind == Graph::GraphSelection::Layer)
            d_app->showGeneralPlotDialog();
        else
            d_app->showPageSetupDialog();
        break;
    }
}

void GraphInspectorDock::onObjectTreeContextMenu(const QPoint &pos)
{
    if (!d_object_tree || !d_target_ml || !d_app
        || ApplicationWindow::isNonInteractiveContext(d_app)) {
        return;
    }

    QTreeWidgetItem *item = d_object_tree->itemAt(pos);
    if (!item)
        return;

    // E5-3: capture ONLY value-based descriptors + QPointer guards BEFORE onTreeItemClicked()
    // (which may flush pending edits and call refreshObjectTree() -> d_object_tree->clear(),
    // freeing `item`) and before menu.exec() — never dereference or capture raw
    // QTreeWidgetItem* or raw Graph* afterward.
    const int layerOneBased = item->data(0, RoleLayerIndex).toInt();
    const auto kind =
            static_cast<Graph::GraphSelection::Kind>(item->data(0, RoleSelKind).toInt());
    const int primaryId = item->data(0, RolePrimaryId).toInt();
    const bool isGroupHeader = item->data(0, RoleIsGroupHeader).toBool();
    const bool isCheckable = !isGroupHeader && (item->flags() & Qt::ItemIsUserCheckable);
    const bool isVis = isCheckable && (item->checkState(0) == Qt::Checked);

    d_object_tree->setCurrentItem(item);
    onTreeItemClicked(item, 0);

    QPointer<MultiLayer> safeMl(d_target_ml);
    QPointer<Graph> safeG(
            (layerOneBased >= 1 && safeMl) ? safeMl->layer(layerOneBased) : activeLayer());

    auto resolveLayer = [safeMl, safeG, layerOneBased]() -> Graph * {
        if (!safeMl)
            return nullptr;
        if (safeG && safeG->multiLayerParent() == safeMl.data())
            return safeG.data();
        if (layerOneBased >= 1)
            return safeMl->layer(layerOneBased);
        return safeMl->activeGraph() ? safeMl->activeGraph() : safeMl->layer(1);
    };

    auto captureSingleAxis = [](Graph *lg, int ax, CopiedSingleAxisScale &out) {
        out = CopiedSingleAxisScale();
        if (!lg || !lg->plotWidget() || ax < 0 || ax > 3)
            return;
        Plot *pw = lg->plotWidget();
        const QwtScaleDiv *scDiv = pw->axisScaleDiv(ax);
        if (!scDiv)
            return;
        out.lo = qMin(scDiv->lowerBound(), scDiv->upperBound());
        out.hi = qMax(scDiv->lowerBound(), scDiv->upperBound());
        out.step = lg->axisStep(ax);
        out.maxMajor = pw->axisMaxMajor(ax);
        out.maxMinor = pw->axisMaxMinor(ax);
        out.inverted = (scDiv->lowerBound() > scDiv->upperBound());
        out.brk = lg->axisBreak(ax);
        const QwtScaleEngine *eng = pw->axisScaleEngine(ax);
        if (eng)
            out.inverted = eng->testAttribute(QwtScaleEngine::Inverted);
        const bool isLog10 = out.brk.enabled ? (out.brk.log10Before && out.brk.log10After)
                                             : Plot::isLog10ScaleEngine(eng);
        out.scaleType = isLog10 ? 1 : 0;
        out.valid = std::isfinite(out.lo) && std::isfinite(out.hi) && qAbs(out.hi - out.lo) > 1e-30;
    };

    auto applySingleAxis = [](MultiLayer *ml, Graph *lg, int ax, const CopiedSingleAxisScale &in) {
        if (!lg || !in.valid || ax < 0 || ax > 3)
            return;
        lg->enableAutoscaling(false);
        lg->setScale(ax, in.lo, in.hi, in.step, in.maxMajor, in.maxMinor, in.scaleType,
                     in.inverted);
        if (in.brk.enabled)
            lg->setAxisBreak(ax, in.brk);
        else if (lg->axisBreak(ax).enabled)
            lg->clearAxisBreak(ax);
        lg->setAxisUserLocked(ax, true);
        if (ml)
            syncLinkedChildrenOf(ml, lg);
    };

    QMenu menu;
    if (isCheckable) {
        QAction *actVis = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("visibility"), 14),
                isVis ? tr("Hide Object") : tr("Show Object"));
        connect(actVis, &QAction::triggered, this,
                [this, safeMl, resolveLayer, kind, primaryId, nextVis = !isVis]() {
                    Graph *lg = resolveLayer();
                    if (!safeMl || !lg)
                        return;
                    if (kind == Graph::GraphSelection::Curve
                        || kind == Graph::GraphSelection::ErrorBars) {
                        if (primaryId >= 0 && primaryId < lg->curves()
                            && lg->isCurveVisible(primaryId) != nextVis) {
                            const QString before = safeMl->captureUndoSnapshot();
                            lg->showCurve(primaryId, nextVis);
                            safeMl->pushUndoSnapshot(
                                    kind == Graph::GraphSelection::ErrorBars
                                            ? tr("Toggle Error Bar Visibility")
                                            : tr("Toggle Curve Visibility"),
                                    before);
                            refreshAll();
                        }
                    } else if (kind == Graph::GraphSelection::Spectrogram) {
                        if (primaryId >= 0 && primaryId < lg->curves()) {
                            if (auto *sp = dynamic_cast<Spectrogram *>(lg->plotItem(primaryId))) {
                                if (sp->isVisible() != nextVis) {
                                    const QString before = safeMl->captureUndoSnapshot();
                                    sp->setVisible(nextVis);
                                    lg->replot();
                                    emit lg->modifiedGraph();
                                    safeMl->pushUndoSnapshot(tr("Toggle Heatmap Visibility"),
                                                             before);
                                    refreshAll();
                                }
                            }
                        }
                    } else if (kind == Graph::GraphSelection::LegendBox) {
                        const QString before = safeMl->captureUndoSnapshot();
                        if (nextVis && !lg->hasLegend())
                            lg->newLegend();
                        lg->setLegendVisible(nextVis);
                        safeMl->pushUndoSnapshot(tr("Toggle Legend Visibility"), before);
                        refreshAll();
                    } else if (kind == Graph::GraphSelection::ReferenceLine) {
                        if (primaryId >= 0 && primaryId < lg->referenceLines().size()) {
                            Graph::ReferenceLineSpec spec = lg->referenceLines()[primaryId];
                            const QString before = safeMl->captureUndoSnapshot();
                            spec.visible = nextVis;
                            if (nextVis && spec.style == Qt::NoPen)
                                spec.style = Qt::DashLine;
                            lg->updateReferenceLine(primaryId, spec);
                            safeMl->pushUndoSnapshot(tr("Toggle Reference Line Visibility"),
                                                     before);
                            refreshAll();
                        }
                    }
                });
    }

    if (!isGroupHeader
        && (kind == Graph::GraphSelection::Curve || kind == Graph::GraphSelection::ErrorBars
            || kind == Graph::GraphSelection::Spectrogram)) {
        QAction *actEdit = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("settings"), 14),
                tr("Edit Curve Style..."));
        connect(actEdit, &QAction::triggered, this, [this, safeMl, resolveLayer, primaryId]() {
            QPointer<MultiLayer> ml = safeMl;
            QPointer<Graph> lg(resolveLayer());
            if (!ensureTargetMultiLayerActive(ml) || !lg)
                return;
            ml->setActiveGraph(lg.data());
            d_app->showPlotDialog((primaryId >= 0 && primaryId < lg->curves())
                                          ? int(lg->curveKey(primaryId))
                                          : primaryId);
        });

        // B1-9: Edit X/Y Data Columns / Range + Copy/Paste Curve Color & Symbol
        if (kind == Graph::GraphSelection::Curve) {
            QAction *actEditCols = menu.addAction(
                    IconLoader::duotoneIcon(QStringLiteral("plot-line-symbol"), 14),
                    tr("Edit X/Y Data Columns..."));
            QPointer<GraphInspectorDock> self(this);
            connect(actEditCols, &QAction::triggered, this,
                    [self, safeMl, resolveLayer, primaryId]() {
                        Graph *lg = resolveLayer();
                        if (!self || !self->d_app || !safeMl || !lg)
                            return;
                        safeMl->setActiveGraph(lg);
                        if (self->d_app->activeWindow() != safeMl.data())
                            self->d_app->activateSubWindow(safeMl.data());
                        if (primaryId >= 0 && primaryId < lg->curves()
                            && dynamic_cast<DataCurve *>(lg->curve(primaryId)) != nullptr) {
                            self->d_app->showPlotAssociations(primaryId);
                        } else {
                            self->d_app->showCurvesDialog();
                        }
                        if (self)
                            self->refreshAll();
                    });

            if (safeG && primaryId >= 0 && primaryId < safeG->curves()
                && dynamic_cast<DataCurve *>(safeG->curve(primaryId)) != nullptr) {
                QAction *actEditRange = menu.addAction(tr("Edit Data Row Range..."));
                connect(actEditRange, &QAction::triggered, this,
                        [this, safeMl, resolveLayer, primaryId]() {
                            Graph *lg = resolveLayer();
                            if (!d_app || !safeMl || !lg)
                                return;
                            if (primaryId >= 0 && primaryId < lg->curves()
                                && dynamic_cast<DataCurve *>(lg->curve(primaryId)) != nullptr) {
                                safeMl->setActiveGraph(lg);
                                d_app->showCurveRangeDialog(lg, primaryId);
                            }
                        });
            }

            menu.addSeparator();
            QAction *actCopyStyle = menu.addAction(
                    IconLoader::duotoneIcon(QStringLiteral("copy"), 14),
                    tr("Copy Curve Color && Symbol"));
            connect(actCopyStyle, &QAction::triggered, this, [resolveLayer, primaryId]() {
                Graph *lg = resolveLayer();
                if (!lg || primaryId < 0 || primaryId >= lg->curves())
                    return;
                QwtPlotCurve *cv = lg->curve(primaryId);
                if (!cv)
                    return;
                const bool hasLine =
                        (cv->style() != QwtPlotCurve::NoCurve && cv->pen().style() != Qt::NoPen);
                s_copiedCurveStyle.lineColor = (hasLine && cv->pen().color().isValid())
                        ? cv->pen().color()
                        : cv->symbol().pen().color();
                s_copiedCurveStyle.symbol = cv->symbol();
                s_copiedCurveStyle.hasBrush = (cv->brush().style() != Qt::NoBrush);
                s_copiedCurveStyle.brush = cv->brush();
                s_copiedCurveStyle.valid = true;
            });

            QAction *actPasteStyle = menu.addAction(
                    IconLoader::duotoneIcon(QStringLiteral("paste"), 14),
                    tr("Paste Curve Color && Symbol"));
            actPasteStyle->setEnabled(s_copiedCurveStyle.valid);
            connect(actPasteStyle, &QAction::triggered, this,
                    [this, safeMl, resolveLayer, primaryId]() {
                        Graph *lg = resolveLayer();
                        if (!safeMl || !lg || !s_copiedCurveStyle.valid || primaryId < 0
                            || primaryId >= lg->curves()) {
                            return;
                        }
                        QwtPlotCurve *cv = lg->curve(primaryId);
                        if (!cv)
                            return;
                        const QString before = safeMl->captureUndoSnapshot();
                        if (s_copiedCurveStyle.lineColor.isValid()) {
                            QPen lp = cv->pen();
                            lp.setColor(s_copiedCurveStyle.lineColor);
                            cv->setPen(lp);
                            if (cv->brush().style() != Qt::NoBrush) {
                                QColor fc = s_copiedCurveStyle.lineColor;
                                fc.setAlpha(cv->brush().color().alpha() > 0
                                                    ? cv->brush().color().alpha()
                                                    : 65);
                                cv->setBrush(QBrush(fc));
                            }
                            if (auto *dc = dynamic_cast<DataCurve *>(cv)) {
                                for (DataCurve *eb : dc->errorBarsList()) {
                                    if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(eb))
                                        er->setColor(s_copiedCurveStyle.lineColor);
                                }
                            }
                        }
                        cv->setSymbol(s_copiedCurveStyle.symbol);
                        lg->reconstructLegend();
                        lg->updatePlot();
                        emit lg->modifiedGraph();
                        safeMl->pushUndoSnapshot(tr("Paste Curve Style"), before);
                        refreshAll();
                    });
        }
    } else if (isGroupHeader && kind == Graph::GraphSelection::Curve) {
        QAction *actCurvesDlg = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("settings"), 14),
                tr("Add/Remove Curves..."));
        connect(actCurvesDlg, &QAction::triggered, this, [this, safeMl, resolveLayer]() {
            Graph *lg = resolveLayer();
            if (!d_app || !safeMl || !lg)
                return;
            safeMl->setActiveGraph(lg);
            d_app->showCurvesDialog();
        });
    } else if (!isGroupHeader && kind == Graph::GraphSelection::ReferenceLine) {
        QAction *actEditRef = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("settings"), 14),
                tr("Edit Reference Line..."));
        QPointer<GraphInspectorDock> self(this);
        connect(actEditRef, &QAction::triggered, this, [self, resolveLayer, primaryId]() {
            Graph *lg = resolveLayer();
            if (lg && primaryId >= 0 && primaryId < lg->referenceLines().size()) {
                if (lg->editReferenceLineDialog(primaryId) && self)
                    self->refreshAll();
            }
        });
    } else if (!isGroupHeader
               && (kind == Graph::GraphSelection::Axis
                   || kind == Graph::GraphSelection::AxisTitle)) {
        QAction *actEditAxis = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("axis-scale"), 14),
                tr("Edit Axis Properties..."));
        connect(actEditAxis, &QAction::triggered, this, [this, resolveLayer, primaryId]() {
            if (Graph *lg = resolveLayer())
                openAxesDialogForQwtAxis(lg, qBound(0, primaryId, 3), true);
        });
    } else if (kind == Graph::GraphSelection::PlotTitle) {
        QAction *actTitle = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("text-label"), 14),
                tr("Plot Title"));
        connect(actTitle, &QAction::triggered, this, [this, safeMl]() {
            QPointer<MultiLayer> ml = safeMl;
            if (ensureTargetMultiLayerActive(ml))
                d_app->showTitleDialog();
        });
    } else if (kind == Graph::GraphSelection::Page) {
        QAction *actPage = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("page-setup"), 14),
                tr("Page Setup..."));
        connect(actPage, &QAction::triggered, this, [this, safeMl]() {
            QPointer<MultiLayer> ml = safeMl;
            if (ensureTargetMultiLayerActive(ml))
                d_app->showPageSetupDialog();
        });
    } else if (kind == Graph::GraphSelection::Layer) {
        QAction *actProp = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("settings"), 14),
                tr("Layer / Plot Properties..."));
        connect(actProp, &QAction::triggered, this, [this, safeMl]() {
            QPointer<MultiLayer> ml = safeMl;
            if (ensureTargetMultiLayerActive(ml))
                d_app->showGeneralPlotDialog();
        });
    }

    // B1-9: Copy/Paste Axis Scale Range on Layer, Page, Axis, or AxisTitle
    if (kind == Graph::GraphSelection::Layer || kind == Graph::GraphSelection::Page
        || kind == Graph::GraphSelection::Axis || kind == Graph::GraphSelection::AxisTitle) {
        const bool singleAxisMode =
                (kind == Graph::GraphSelection::Axis || kind == Graph::GraphSelection::AxisTitle);
        menu.addSeparator();
        QAction *actCopyScale = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("copy"), 14),
                tr("Copy Axis Scale Range"));
        connect(actCopyScale, &QAction::triggered, this,
                [resolveLayer, captureSingleAxis, singleAxisMode, primaryId]() {
                    Graph *lg = resolveLayer();
                    if (!lg)
                        return;
                    CopiedAxisScaleClipboard clip;
                    clip.allAxes = !singleAxisMode;
                    clip.sourceAxis = singleAxisMode ? qBound(0, primaryId, 3) : int(QwtPlot::xBottom);
                    bool anyValid = false;
                    for (int a = 0; a < 4; ++a) {
                        captureSingleAxis(lg, a, clip.axes[a]);
                        if (clip.axes[a].valid)
                            anyValid = true;
                    }
                    clip.valid = anyValid;
                    if (anyValid)
                        s_copiedAxisScale = clip;
                });

        QAction *actPasteScale = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("paste"), 14),
                tr("Paste Axis Scale Range"));
        actPasteScale->setEnabled(s_copiedAxisScale.valid);
        connect(actPasteScale, &QAction::triggered, this,
                [this, safeMl, resolveLayer, applySingleAxis, singleAxisMode, primaryId]() {
                    Graph *lg = resolveLayer();
                    if (!safeMl || !lg || !s_copiedAxisScale.valid)
                        return;
                    const QString before = safeMl->captureUndoSnapshot();
                    if (singleAxisMode) {
                        const int dstAx = qBound(0, primaryId, 3);
                        const int srcAx = s_copiedAxisScale.allAxes
                                ? dstAx
                                : qBound(0, s_copiedAxisScale.sourceAxis, 3);
                        applySingleAxis(safeMl.data(), lg, dstAx, s_copiedAxisScale.axes[srcAx]);
                    } else {
                        for (int a = 0; a < 4; ++a) {
                            if (s_copiedAxisScale.axes[a].valid)
                                applySingleAxis(safeMl.data(), lg, a, s_copiedAxisScale.axes[a]);
                        }
                    }
                    lg->replot();
                    emit lg->modifiedGraph();
                    safeMl->pushUndoSnapshot(tr("Paste Axis Scale Range"), before);
                    refreshAll();
                });
    }

    menu.addSeparator();
    QAction *actAdd = menu.addAction(
            IconLoader::duotoneIcon(QStringLiteral("curve-add"), 14),
            tr("Add Curve(s)..."));
    connect(actAdd, &QAction::triggered, this, &GraphInspectorDock::openAddCurvePopover);

    if (safeG && (kind == Graph::GraphSelection::LegendBox || kind == Graph::GraphSelection::Layer)) {
        QAction *actAutoLeg = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("legend"), 14),
                tr("Auto-Place Legend"));
        connect(actAutoLeg, &QAction::triggered, this, [this, safeMl, resolveLayer]() {
            Graph *lg = resolveLayer();
            if (!safeMl || !lg)
                return;
            const QString before = safeMl->captureUndoSnapshot();
            if (!lg->hasLegend())
                lg->newLegend();
            lg->setLegendVisible(true);
            lg->autoPlaceLegend();
            lg->replot();
            safeMl->pushUndoSnapshot(tr("Auto-Place Legend"), before);
            refreshAll();
        });
    }

    if (!isGroupHeader && kind == Graph::GraphSelection::Curve) {
        menu.addSeparator();
        QAction *actUp = menu.addAction(tr("Move Curve Up"));
        connect(actUp, &QAction::triggered, this, [this]() { moveSelectedCurve(-1); });
        QAction *actDown = menu.addAction(tr("Move Curve Down"));
        connect(actDown, &QAction::triggered, this, [this]() { moveSelectedCurve(+1); });
    }

    if (!isGroupHeader
        && (kind == Graph::GraphSelection::Curve || kind == Graph::GraphSelection::ErrorBars
            || kind == Graph::GraphSelection::ReferenceLine
            || kind == Graph::GraphSelection::LegendBox
            || kind == Graph::GraphSelection::TextAnnotation
            || kind == Graph::GraphSelection::ArrowAnnotation
            || kind == Graph::GraphSelection::ImageAnnotation)) {
        menu.addSeparator();
        QAction *actRem = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("delete"), 14),
                tr("Remove Object"));
        connect(actRem, &QAction::triggered, this, &GraphInspectorDock::removeSelectedObject);
    }

    if (!menu.isEmpty())
        menu.exec(d_object_tree->viewport()->mapToGlobal(pos));
}

void GraphInspectorDock::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (d_syncing_tree || column != 0 || !item || !d_target_ml)
        return;

    const int layerOneBased = item->data(0, RoleLayerIndex).toInt();
    Graph *g = (layerOneBased >= 1) ? d_target_ml->layer(layerOneBased) : activeLayer();
    if (!g)
        return;

    const auto kind =
            static_cast<Graph::GraphSelection::Kind>(item->data(0, RoleSelKind).toInt());
    const bool checked = (item->checkState(0) == Qt::Checked);

    if ((kind == Graph::GraphSelection::Curve || kind == Graph::GraphSelection::ErrorBars)
        && !item->data(0, RoleIsGroupHeader).toBool()) {
        const int ci = item->data(0, RolePrimaryId).toInt();
        if (ci >= 0 && ci < g->curves() && g->isCurveVisible(ci) != checked) {
            const QString before = d_target_ml->captureUndoSnapshot();
            g->showCurve(ci, checked);
            d_target_ml->pushUndoSnapshot(kind == Graph::GraphSelection::ErrorBars
                                                  ? tr("Toggle Error Bar Visibility")
                                                  : tr("Toggle Curve Visibility"),
                                          before);
        }
    } else if (kind == Graph::GraphSelection::Spectrogram) {
        const int ci = item->data(0, RolePrimaryId).toInt();
        if (auto *sp = dynamic_cast<Spectrogram *>(g->plotItem(ci))) {
            if (sp->isVisible() != checked) {
                const QString before = d_target_ml->captureUndoSnapshot();
                sp->setVisible(checked);
                g->replot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Toggle Heatmap Visibility"), before);
            }
        }
    } else if (kind == Graph::GraphSelection::LegendBox) {
        const QString before = d_target_ml->captureUndoSnapshot();
        if (checked && !g->hasLegend())
            g->newLegend();
        g->setLegendVisible(checked);
        d_target_ml->pushUndoSnapshot(tr("Toggle Legend Visibility"), before);
        syncLegendControls();
    } else if (kind == Graph::GraphSelection::ReferenceLine
               && !item->data(0, RoleIsGroupHeader).toBool()) {
        const int ri = item->data(0, RolePrimaryId).toInt();
        if (ri >= 0 && ri < g->referenceLines().size()) {
            Graph::ReferenceLineSpec spec = g->referenceLines()[ri];
            const bool curVis = (spec.visible && spec.style != Qt::NoPen);
            if (curVis != checked) {
                const QString before = d_target_ml->captureUndoSnapshot();
                spec.visible = checked;
                if (checked && spec.style == Qt::NoPen)
                    spec.style = Qt::DashLine;
                g->updateReferenceLine(ri, spec);
                d_target_ml->pushUndoSnapshot(tr("Toggle Reference Line Visibility"), before);
                syncAxisControls();
            }
        }
    }
}

void GraphInspectorDock::onGraphSelectionChanged(MultiLayer *page, Graph *layer,
                                                 const Graph::GraphSelection &sel)
{
    if (page != d_target_ml)
        return;

    // E5-2: flush pending debounced text edits if selection moved away from the edited object.
    if (d_annot_commit_timer && d_annot_commit_timer->isActive()) {
        const bool sameAnnotTarget = d_annot_edit_layer
                && (layer == d_annot_edit_layer.data())
                && (sel.kind == Graph::GraphSelection::TextAnnotation)
                && (sel.primaryId == d_annot_edit_id);
        if (!sameAnnotTarget)
            flushPendingAnnotationEdit();
    }
    if (d_axis_title_timer && d_axis_title_timer->isActive()) {
        const bool sameAxisTarget = d_axis_title_edit_layer
                && (layer == d_axis_title_edit_layer.data())
                && (sel.kind == Graph::GraphSelection::Axis
                    || sel.kind == Graph::GraphSelection::AxisTitle)
                && (qBound(0, sel.primaryId, 3) == d_axis_title_edit_axis);
        if (!sameAxisTarget)
            flushPendingAxisTitleEdit();
    }

    if (layer && d_target_ml && d_target_ml->activeGraph() != layer)
        d_target_ml->setActiveGraph(layer);

    if (sel.kind == Graph::GraphSelection::Axis || sel.kind == Graph::GraphSelection::AxisTitle)
        d_active_axis = qBound(0, sel.primaryId, 3);
    else if (sel.kind == Graph::GraphSelection::ReferenceLine && layer && sel.primaryId >= 0
             && sel.primaryId < layer->referenceLines().size()) {
        d_active_axis = (layer->referenceLines()[sel.primaryId].orientation == 1)
                ? int(QwtPlot::xBottom)
                : int(QwtPlot::yLeft);
    }

    refreshObjectTree();
    syncInspectorFromSelection();
}

void GraphInspectorDock::syncTabPage(InspectorPageIndex pageIdx)
{
    const int idx = qBound(0, int(pageIdx), 3);
    d_tab_dirty[idx] = false;
    switch (pageIdx) {
    case PageTabPageLayer:
        syncPageLayerControls();
        break;
    case PageTabAxis:
        syncAxisControls();
        break;
    case PageTabCurve:
        syncCurveControls();
        break;
    case PageTabLegend:
        syncLegendControls();
        break;
    }
    QTimer::singleShot(0, this, &GraphInspectorDock::updatePropScrollCue);
}

void GraphInspectorDock::syncInspectorFromSelection(bool allowTabSwitch)
{
    if (!d_target_ml)
        return;

    for (bool &dirty : d_tab_dirty)
        dirty = true;

    Graph *g = activeLayer();
    const Graph::GraphSelection sel = g ? g->currentSelection() : Graph::GraphSelection();

    if (sel.kind == Graph::GraphSelection::Axis || sel.kind == Graph::GraphSelection::AxisTitle)
        d_active_axis = qBound(0, sel.primaryId, 3);

    if (allowTabSwitch) {
        switch (sel.kind) {
        case Graph::GraphSelection::Axis:
        case Graph::GraphSelection::AxisTitle:
        case Graph::GraphSelection::ReferenceLine:
            switchToInspectorPage(PageTabAxis);
            break;
        case Graph::GraphSelection::Curve:
        case Graph::GraphSelection::ErrorBars:
        case Graph::GraphSelection::Spectrogram:
            switchToInspectorPage(PageTabCurve);
            break;
        case Graph::GraphSelection::PlotTitle:
        case Graph::GraphSelection::LegendBox:
        case Graph::GraphSelection::TextAnnotation:
        case Graph::GraphSelection::ArrowAnnotation:
        case Graph::GraphSelection::ImageAnnotation:
            switchToInspectorPage(PageTabLegend);
            break;
        case Graph::GraphSelection::Page:
        case Graph::GraphSelection::Layer:
            switchToInspectorPage(PageTabPageLayer);
            break;
        case Graph::GraphSelection::None:
            break;
        }
    }

    // Always keep lightweight Page, Axis, and Curve fields in sync so programmatic inspections
    // and tests (e.g. Suite 63e InspectorMinorTicksCombo) never read stale values off-tab; only
    // defer the heavier up-to-24-row Legend widget rebuild when another tab is active.
    const InspectorPageIndex curPage = currentInspectorPage();
    syncPageLayerControls();
    syncAxisControls();
    syncCurveControls();
    d_tab_dirty[int(PageTabPageLayer)] = false;
    d_tab_dirty[int(PageTabAxis)] = false;
    d_tab_dirty[int(PageTabCurve)] = false;
    if (curPage == PageTabLegend) {
        syncLegendControls();
        d_tab_dirty[int(PageTabLegend)] = false;
    } else {
        d_tab_dirty[int(PageTabLegend)] = true;
    }
    QTimer::singleShot(0, this, &GraphInspectorDock::updatePropScrollCue);
}

void GraphInspectorDock::openAddCurvePopover()
{
    if (!d_app || !d_target_ml)
        return;
    Graph *g = activeLayer();
    if (!g)
        return;

    QList<MyWidget *> *tblList = d_app->tableList();
    QVector<QPointer<Table>> tables;
    if (tblList) {
        for (MyWidget *w : *tblList) {
            if (!isUsableInspectorTable(d_app, w))
                continue;
            if (Table *t = qobject_cast<Table *>(w))
                tables.append(QPointer<Table>(t));
        }
    }
    if (tables.isEmpty())
        return;

    QMenu menu;
    QWidget *panel = new QWidget(&menu);
    panel->setMinimumWidth(292);
    QVBoxLayout *v = new QVBoxLayout(panel);
    v->setContentsMargins(10, 8, 10, 8);
    v->setSpacing(6);

    QLabel *hdr = new QLabel(tr("Add Curve(s) to Layer"), panel);
    QFont hf = hdr->font();
    hf.setBold(true);
    hdr->setFont(hf);
    v->addWidget(hdr);

    QGridLayout *topGrid = new QGridLayout();
    topGrid->setHorizontalSpacing(6);
    topGrid->setVerticalSpacing(4);

    topGrid->addWidget(new QLabel(tr("Table:"), panel), 0, 0);
    QComboBox *comboTable = new QComboBox(panel);
    int defaultTblIdx = 0;
    if (g->curves() > 0) {
        if (DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(0))) {
            for (int ti = 0; ti < tables.size(); ++ti) {
                if (tables[ti].data() == dc->table()) {
                    defaultTblIdx = ti;
                    break;
                }
            }
        }
    }
    for (const QPointer<Table> &tp : tables)
        comboTable->addItem(tp ? tp->name() : QString());
    comboTable->setCurrentIndex(defaultTblIdx);
    topGrid->addWidget(comboTable, 0, 1);

    topGrid->addWidget(new QLabel(tr("X Column:"), panel), 1, 0);
    QComboBox *comboXCol = new QComboBox(panel);
    topGrid->addWidget(comboXCol, 1, 1);

    topGrid->addWidget(new QLabel(tr("Plot Style:"), panel), 2, 0);
    QComboBox *comboStyle = new QComboBox(panel);
    comboStyle->addItem(tr("Line + Symbol"), int(Graph::LineSymbols));
    comboStyle->addItem(tr("Line"), int(Graph::Line));
    comboStyle->addItem(tr("Scatter"), int(Graph::Scatter));
    comboStyle->addItem(tr("Vertical Bars"), int(Graph::VerticalBars));
    comboStyle->addItem(tr("Horizontal Bars"), int(Graph::HorizontalBars));
    comboStyle->addItem(tr("Area"), int(Graph::Area));
    topGrid->addWidget(comboStyle, 2, 1);
    v->addLayout(topGrid);

    v->addWidget(new QLabel(tr("Y Columns (multi-select):"), panel));
    QListWidget *yChecklist = new QListWidget(panel);
    yChecklist->setMinimumHeight(130);
    yChecklist->setMaximumHeight(190);
    v->addWidget(yChecklist);

    QCheckBox *chkRightY = new QCheckBox(tr("Assign to Right-Y Axis"), panel);
    v->addWidget(chkRightY);

    QPushButton *btnAddNow = new QPushButton(tr("Add Selected Curve(s)"), panel);
    v->addWidget(btnAddNow);

    auto populateColumnsForTable = [&](int tblIdx) {
        comboXCol->clear();
        yChecklist->clear();
        if (tblIdx < 0 || tblIdx >= tables.size())
            return;
        Table *t = tables[tblIdx].data();
        if (!t)
            return;
        comboXCol->addItem(tr("(Default Table X)"), QString());
        for (int c = 0; c < t->numCols(); ++c) {
            const QString colShort = t->colLabel(c);
            const QString fullName = QString(t->name()) + QStringLiteral("_") + colShort;
            const int desig = t->colPlotDesignation(c);
            QString roleTag = QStringLiteral("[Y]");
            if (desig == SciDAVis::X)
                roleTag = QStringLiteral("[X]");
            else if (desig == SciDAVis::Z)
                roleTag = QStringLiteral("[Z]");
            else if (desig == SciDAVis::xErr)
                roleTag = QStringLiteral("[xEr]");
            else if (desig == SciDAVis::yErr)
                roleTag = QStringLiteral("[yEr]");
            else if (desig == SciDAVis::noDesignation)
                roleTag = QStringLiteral("[-]");

            const QString disp = formatColumnLabelWithMeta(t, c, roleTag);
            comboXCol->addItem(disp, fullName);
            comboXCol->setItemData(comboXCol->count() - 1, disp, Qt::ToolTipRole);
            if (desig != SciDAVis::X) {
                QListWidgetItem *it = new QListWidgetItem(disp, yChecklist);
                it->setToolTip(disp);
                it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
                it->setCheckState(Qt::Unchecked);
                it->setData(Qt::UserRole, fullName);
            }
        }
    };

    populateColumnsForTable(defaultTblIdx);
    QObject::connect(comboTable, QOverload<int>::of(&QComboBox::currentIndexChanged), panel,
                     populateColumnsForTable);

    QPointer<GraphInspectorDock> self(this);
    QPointer<MultiLayer> safeMl(d_target_ml);
    QPointer<Graph> safeG(g);
    QObject::connect(btnAddNow, &QPushButton::clicked, &menu, [&]() {
        if (!self || !safeMl || !safeG) {
            menu.close();
            return;
        }
        const int tblIdx = comboTable->currentIndex();
        if (tblIdx < 0 || tblIdx >= tables.size())
            return;
        Table *t = tables[tblIdx].data();
        if (!t) {
            menu.close();
            return;
        }
        const QString xOverride = comboXCol->currentData().toString();
        const int plotStyle = comboStyle->currentData().toInt();
        const bool toRightY = chkRightY->isChecked();

        QStringList selectedYCols;
        for (int i = 0; i < yChecklist->count(); ++i) {
            QListWidgetItem *it = yChecklist->item(i);
            if (it && it->checkState() == Qt::Checked)
                selectedYCols.append(it->data(Qt::UserRole).toString());
        }
        if (selectedYCols.isEmpty())
            return;

        const QString before = safeMl->captureUndoSnapshot();
        int addedCount = 0;
        for (const QString &yFull : selectedYCols) {
            const int prevCount = safeG->curves();
            int dataCurveOrder = 0;
            for (int ci = 0; ci < prevCount; ++ci) {
                if (safeG->curveType(ci) != Graph::ErrorBars)
                    ++dataCurveOrder;
            }
            bool ok = false;
            if (!xOverride.isEmpty())
                ok = safeG->insertCurve(t, xOverride, yFull, plotStyle);
            else
                ok = safeG->insertCurve(t, yFull, plotStyle);
            if (ok && safeG->curves() > prevCount) {
                ++addedCount;
                const int newIdx = safeG->curves() - 1;
                CurveLayout cl = safeG->initCurveLayout(
                        static_cast<Graph::CurveType>(plotStyle), dataCurveOrder);
                if (self->d_app) {
                    cl.sSize = self->d_app->defaultSymbolSize;
                    cl.lWidth = self->d_app->defaultCurveLineWidth;
                }
                safeG->updateCurveLayout(newIdx, &cl);
                if (toRightY) {
                    if (QwtPlotCurve *cv = safeG->curve(newIdx))
                        cv->setYAxis(QwtPlot::yRight);
                }
            }
        }

        if (addedCount > 0) {
            if (toRightY) {
                safeG->enableAxis(QwtPlot::yRight, true);
                safeG->setAxisBackboneVisible(QwtPlot::yRight, true);
                if (Plot *pw = safeG->plotWidget()) {
                    if (QwtScaleDraw *sd = pw->axisScaleDraw(QwtPlot::yRight))
                        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                    pw->setMajorTicksType(QwtPlot::yRight, ScaleDraw::In);
                    pw->setMinorTicksType(QwtPlot::yRight, ScaleDraw::In);
                }
                if (!safeG->isAxisUserLocked(QwtPlot::yRight))
                    safeG->setAutoScaleAxis(QwtPlot::yRight);
            }
            if (!safeG->isAxisUserLocked(QwtPlot::xBottom))
                safeG->setAutoScaleAxis(QwtPlot::xBottom);
            if (!toRightY && !safeG->isAxisUserLocked(QwtPlot::yLeft))
                safeG->setAutoScaleAxis(QwtPlot::yLeft);
            safeG->reconstructLegend();
            safeG->refreshAutoAxisTitlesFromColumns();
            safeG->updatePlot();
            safeMl->pushUndoSnapshot(tr("Add Curve(s)"), before);
        }
        menu.close();
        if (self)
            self->refreshAll();
    });

    QWidgetAction *wa = new QWidgetAction(&menu);
    wa->setDefaultWidget(panel);
    menu.addAction(wa);
    menu.exec(d_btn_add_curve->mapToGlobal(QPoint(0, d_btn_add_curve->height() + 2)));
}

void GraphInspectorDock::moveSelectedCurve(int delta)
{
    if (!d_target_ml || !d_object_tree || delta == 0)
        return;

    Graph *g = activeLayer();
    int curIdx = -1;
    if (QTreeWidgetItem *cur = d_object_tree->currentItem()) {
        if (!cur->data(0, RoleIsGroupHeader).toBool()) {
            const auto kind =
                    static_cast<Graph::GraphSelection::Kind>(cur->data(0, RoleSelKind).toInt());
            if (kind == Graph::GraphSelection::Curve || kind == Graph::GraphSelection::ErrorBars) {
                const int layerOneBased = cur->data(0, RoleLayerIndex).toInt();
                if (layerOneBased >= 1) {
                    if (Graph *lg = d_target_ml->layer(layerOneBased))
                        g = lg;
                }
                curIdx = resolveMasterCurveIndex(g, cur->data(0, RolePrimaryId).toInt());
            }
        }
    }
    if (curIdx < 0 && g)
        curIdx = effectiveSelectedCurveIndex(g, false);
    if (!g || curIdx < 0 || curIdx >= g->curves() || g->curveType(curIdx) == Graph::ErrorBars)
        return;

    QVector<int> dataIndices;
    for (int i = 0; i < g->curves(); ++i) {
        if (g->curveType(i) != Graph::ErrorBars)
            dataIndices.append(i);
    }
    const int pos = dataIndices.indexOf(curIdx);
    if (pos < 0 || (delta < 0 && pos <= 0) || (delta > 0 && pos >= dataIndices.size() - 1))
        return;

    QwtPlotItem *movedCurvePtr = g->curve(curIdx);
    if (!movedCurvePtr)
        return;

    const int neighborMasterIdx = dataIndices[pos + (delta < 0 ? -1 : 1)];
    int toIdx = neighborMasterIdx;
    if (delta > 0) {
        while (toIdx + 1 < g->curves() && g->curveType(toIdx + 1) == Graph::ErrorBars) {
            auto *er = dynamic_cast<QwtErrorPlotCurve *>(g->curve(toIdx + 1));
            if (er && er->masterCurve() == movedCurvePtr)
                break;
            ++toIdx;
        }
    }

    const QString before = d_target_ml->captureUndoSnapshot();
    if (g->moveCurve(curIdx, toIdx)) {
        const int newActualIdx = g->curveIndex(static_cast<QwtPlotCurve *>(movedCurvePtr));
        Graph::GraphSelection sel;
        sel.kind = Graph::GraphSelection::Curve;
        sel.primaryId = (newActualIdx >= 0) ? newActualIdx : toIdx;
        g->setCurrentSelection(sel, false);
        d_target_ml->pushUndoSnapshot(tr("Reorder Curve"), before);
        refreshObjectTree();
        syncCurveControls();
    }
}

void GraphInspectorDock::removeSelectedObject()
{
    if (!d_target_ml || !d_object_tree)
        return;
    QTreeWidgetItem *cur = d_object_tree->currentItem();
    if (!cur || cur->data(0, RoleIsGroupHeader).toBool())
        return;

    const int layerOneBased = cur->data(0, RoleLayerIndex).toInt();
    Graph *g = (layerOneBased >= 1) ? d_target_ml->layer(layerOneBased) : activeLayer();
    if (!g)
        return;

    const auto kind =
            static_cast<Graph::GraphSelection::Kind>(cur->data(0, RoleSelKind).toInt());
    const int primaryId = cur->data(0, RolePrimaryId).toInt();

    if (kind == Graph::GraphSelection::ErrorBars) {
        if (primaryId >= 0 && primaryId < g->curves()) {
            if (d_app && d_target_ml)
                d_app->invalidateModelessGraphDialogs(d_target_ml);
            const QString before = d_target_ml->captureUndoSnapshot();
            g->removeCurve(primaryId);
            g->updatePlot();
            d_target_ml->pushUndoSnapshot(tr("Remove Error Bars"), before);
            refreshAll();
        }
    } else if (kind == Graph::GraphSelection::Curve
               || kind == Graph::GraphSelection::Spectrogram) {
        const int ci = (kind == Graph::GraphSelection::Spectrogram)
                ? primaryId
                : resolveMasterCurveIndex(g, primaryId);
        if (ci >= 0 && ci < g->curves()) {
            if (d_app && d_target_ml)
                d_app->invalidateModelessGraphDialogs(d_target_ml);
            const QString before = d_target_ml->captureUndoSnapshot();
            g->removeCurve(ci);
            g->updatePlot();
            d_target_ml->pushUndoSnapshot(tr("Remove Curve"), before);
            refreshAll();
        }
    } else if (kind == Graph::GraphSelection::ReferenceLine
               && !cur->data(0, RoleIsGroupHeader).toBool()) {
        if (primaryId >= 0 && primaryId < g->referenceLines().size()) {
            g->removeReferenceLine(primaryId);
            refreshAll();
        }
    } else if (kind == Graph::GraphSelection::LegendBox) {
        if (g->hasLegend()) {
            const QString before = d_target_ml->captureUndoSnapshot();
            g->removeLegend();
            g->replot();
            d_target_ml->pushUndoSnapshot(tr("Remove Legend"), before);
            refreshAll();
        }
    } else if (kind == Graph::GraphSelection::TextAnnotation
               || kind == Graph::GraphSelection::ArrowAnnotation
               || kind == Graph::GraphSelection::ImageAnnotation) {
        const QString before = d_target_ml->captureUndoSnapshot();
        g->setSelectedMarker(primaryId, false);
        g->removeMarker();
        g->replot();
        d_target_ml->pushUndoSnapshot(tr("Remove Annotation"), before);
        refreshAll();
    } else if (kind == Graph::GraphSelection::Layer && d_target_ml->layers() > 1) {
        if (d_app && d_target_ml)
            d_app->invalidateModelessGraphDialogs(d_target_ml);
        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->setActiveGraph(g);
        d_target_ml->removeLayer();
        d_target_ml->pushUndoSnapshot(tr("Remove Layer"), before);
        refreshAll();
    }
}
