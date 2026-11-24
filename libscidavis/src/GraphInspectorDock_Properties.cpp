/***************************************************************************
    File                 : GraphInspectorDock_Properties.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Copyright            : (C) 2026 ToriGraph Project
    Description          : Live property sync/apply, rich-text toolbar, and series style
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

//! Escape arbitrary user text while preserving safe scientific markup tags for Qt::RichText preview.
static bool hasRichTextMarkup(const QString &raw)
{
    static const QRegularExpression rxHtml(
            QStringLiteral("</?(?:i|b|u|sub|sup)>"),
            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rxOrigin(QStringLiteral("\\\\[ib\\-\\+]\\("));
    return raw.contains(rxHtml) || raw.contains(rxOrigin);
}

QString renderSafeScientificPreviewHtml(const QString &raw)
{
    QString esc = CanvasInlineTextEditor::normalizeOriginRichText(raw).toHtmlEscaped();
    static const QRegularExpression rxAllowed(
            QStringLiteral("&lt;(/?(?:i|b|u|sub|sup))&gt;"),
            QRegularExpression::CaseInsensitiveOption);
    esc.replace(rxAllowed, QStringLiteral("<\\1>"));
    esc.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return esc;
}

void syncLineEditClean(QLineEdit *ed, const QString &text)
{
    if (!ed)
        return;
    QSignalBlocker blk(ed);
    if (ed->text() != text)
        ed->setText(text);
    ed->setProperty("_syncedText", text.trimmed());
    ed->setModified(false);
    setLineEditPendingCue(ed, false);
}

static int findSmartAdjacentYErrColumn(Table *tbl, int colIdx)
{
    if (!tbl || colIdx < 0 || colIdx >= tbl->numCols())
        return -1;
    if (colIdx + 1 < tbl->numCols() && tbl->colPlotDesignation(colIdx + 1) == SciDAVis::yErr)
        return colIdx + 1;

    for (int c = colIdx + 1; c < tbl->numCols(); ++c) {
        const int desig = tbl->colPlotDesignation(c);
        if (desig == SciDAVis::Y)
            break;
        if (desig == SciDAVis::yErr)
            return c;
    }

    bool hasAnyYErr = false;
    for (int c = 0; c < tbl->numCols(); ++c) {
        if (tbl->colPlotDesignation(c) == SciDAVis::yErr) {
            hasAnyYErr = true;
            break;
        }
    }
    if (!hasAnyYErr && colIdx + 1 < tbl->numCols())
        return colIdx + 1;
    return -1;
}

static double physicalValueToMm(double val, MultiLayer::PageUnit unit)
{
    switch (unit) {
    case MultiLayer::UnitPx:
        return (val / 96.0) * 25.4;
    case MultiLayer::UnitMm:
        return val;
    case MultiLayer::UnitCm:
        return val * 10.0;
    case MultiLayer::UnitInch:
        return val * 25.4;
    }
    return val;
}

} // namespace

int GraphInspectorDock::inspectorPageUnit() const
{
    if (!d_combo_page_unit)
        return int(MultiLayer::UnitMm);
    return d_combo_page_unit->currentData().toInt();
}

void GraphInspectorDock::configurePageSizeSpins(int unitInt, double wMm, double hMm)
{
    const MultiLayer::PageUnit unit = static_cast<MultiLayer::PageUnit>(unitInt);
    if (!d_spin_page_w_mm || !d_spin_page_h_mm)
        return;
    const double f = pageUnitToMmFactor(unit);
    QString suffix = QStringLiteral(" mm");
    int decimals = 1;
    double step = 5.0;
    double minVal = 20.0 / f;
    double maxVal = 1000.0 / f;
    if (unit == MultiLayer::UnitCm) {
        suffix = QStringLiteral(" cm");
        decimals = 2;
        step = 0.5;
    } else if (unit == MultiLayer::UnitInch) {
        suffix = QStringLiteral(" in");
        decimals = 3;
        step = 0.1;
    } else if (unit == MultiLayer::UnitPx) {
        suffix = QStringLiteral(" px");
        decimals = 0;
        step = 20.0;
        minVal = 50.0;
        maxVal = 10000.0;
    }
    for (QDoubleSpinBox *sb : { d_spin_page_w_mm, d_spin_page_h_mm }) {
        QSignalBlocker blk(sb);
        sb->setDecimals(decimals);
        sb->setSingleStep(step);
        sb->setSuffix(suffix);
        sb->setRange(minVal, maxVal);
    }
    {
        QSignalBlocker bw(d_spin_page_w_mm);
        QSignalBlocker bh(d_spin_page_h_mm);
        d_spin_page_w_mm->setValue(wMm / f);
        d_spin_page_h_mm->setValue(hMm / f);
    }
    d_spin_page_w_mm->setProperty("_mmValue", wMm);
    d_spin_page_h_mm->setProperty("_mmValue", hMm);
}

void GraphInspectorDock::syncPageLayerControls()
{
    if (!d_target_ml)
        return;
    d_syncing_props = true;

    const double wMm =
            physicalValueToMm(d_target_ml->pageWidthPhysical(), d_target_ml->pageUnit());
    const double hMm =
            physicalValueToMm(d_target_ml->pageHeightPhysical(), d_target_ml->pageUnit());

    {
        QSignalBlocker b1(d_spin_page_w_mm);
        QSignalBlocker b2(d_spin_page_h_mm);
        QSignalBlocker b3(d_spin_page_dpi);
        QSignalBlocker b4(d_chk_page_view);
        QSignalBlocker b6(d_chk_panel_labels);
        QSignalBlocker b7(d_combo_panel_pos);
        QSignalBlocker b8(d_chk_keep_aspect);

        if (d_chk_keep_aspect)
            d_chk_keep_aspect->setChecked(d_target_ml->keepAspectRatio());

        // B1-7: preserve UnitPx directly instead of silently converting to mm
        const MultiLayer::PageUnit shownUnit = d_target_ml->pageUnit();
        if (d_combo_page_unit) {
            QSignalBlocker bu(d_combo_page_unit);
            const int uIdx = d_combo_page_unit->findData(int(shownUnit));
            d_combo_page_unit->setCurrentIndex(uIdx >= 0 ? uIdx : 0);
        }
        configurePageSizeSpins(int(shownUnit), wMm, hMm);
        d_spin_page_dpi->setValue(d_target_ml->pageDpi());
        d_chk_page_view->setChecked(d_target_ml->pageViewMode());

        d_chk_panel_labels->setChecked(d_target_ml->hasPanelLabels());
        const int pIdx = d_combo_panel_pos->findData(int(d_target_ml->panelLabelPlacement()));
        if (pIdx >= 0)
            d_combo_panel_pos->setCurrentIndex(pIdx);
    }

    d_syncing_props = false;
    syncInsetControls();
}

void GraphInspectorDock::syncInsetControls()
{
    if (!d_inset_wrap)
        return;
    Graph *g = activeLayer();
    const bool isInset = (d_target_ml && g
                          && (d_target_ml->isInsetLayer(g)
                              || d_target_ml->insetZoomDataRect(g).isValid()));
    d_inset_wrap->setVisible(isInset);
    if (!isInset)
        return;

    const bool prevSync = d_syncing_props;
    d_syncing_props = true;

    QRectF roi = d_target_ml->insetZoomDataRect(g);
    if (!roi.isValid() && g->plotWidget()) {
        const QwtScaleDiv *xDiv = g->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
        const QwtScaleDiv *yDiv = g->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
        if (xDiv && yDiv) {
            const double x0 = qMin(xDiv->lowerBound(), xDiv->upperBound());
            const double x1 = qMax(xDiv->lowerBound(), xDiv->upperBound());
            const double y0 = qMin(yDiv->lowerBound(), yDiv->upperBound());
            const double y1 = qMax(yDiv->lowerBound(), yDiv->upperBound());
            roi = QRectF(QPointF(x0, y0), QPointF(x1, y1));
        }
    }

    if (roi.isValid()) {
        const double xMin = qMin(roi.left(), roi.right());
        const double xMax = qMax(roi.left(), roi.right());
        const double yMin = qMin(roi.top(), roi.bottom());
        const double yMax = qMax(roi.top(), roi.bottom());
        syncLineEditClean(d_edit_inset_xmin, QString::number(xMin, 'g', 6));
        syncLineEditClean(d_edit_inset_xmax, QString::number(xMax, 'g', 6));
        syncLineEditClean(d_edit_inset_ymin, QString::number(yMin, 'g', 6));
        syncLineEditClean(d_edit_inset_ymax, QString::number(yMax, 'g', 6));
    }
    if (d_chk_inset_connectors) {
        QSignalBlocker bc(d_chk_inset_connectors);
        d_chk_inset_connectors->setChecked(d_target_ml->insetShowConnectors(g));
    }

    d_syncing_props = prevSync;
}

void GraphInspectorDock::applyInsetRoiFromUi(bool fromEdits)
{
    Graph *g = activeLayer();
    if (d_syncing_props || !d_target_ml || !g || !d_edit_inset_xmin || !d_edit_inset_xmax
        || !d_edit_inset_ymin || !d_edit_inset_ymax) {
        return;
    }
    if (fromEdits) {
        const bool anyDiff = (d_edit_inset_xmin->text().trimmed()
                              != d_edit_inset_xmin->property("_syncedText").toString())
                || (d_edit_inset_xmax->text().trimmed()
                    != d_edit_inset_xmax->property("_syncedText").toString())
                || (d_edit_inset_ymin->text().trimmed()
                    != d_edit_inset_ymin->property("_syncedText").toString())
                || (d_edit_inset_ymax->text().trimmed()
                    != d_edit_inset_ymax->property("_syncedText").toString());
        if (!anyDiff) {
            for (QLineEdit *le :
                 { d_edit_inset_xmin, d_edit_inset_xmax, d_edit_inset_ymin, d_edit_inset_ymax }) {
                le->setModified(false);
                setLineEditPendingCue(le, false);
            }
            return;
        }
    }

    auto parseNum = [](QLineEdit *le, bool *okOut) -> double {
        const QString s = le->text().trimmed();
        bool ok = false;
        double v = QLocale::c().toDouble(s, &ok);
        if (!ok)
            v = QLocale().toDouble(s, &ok);
        if (okOut)
            *okOut = ok;
        return v;
    };
    bool okX0 = false, okX1 = false, okY0 = false, okY1 = false;
    const double x0 = parseNum(d_edit_inset_xmin, &okX0);
    const double x1 = parseNum(d_edit_inset_xmax, &okX1);
    const double y0 = parseNum(d_edit_inset_ymin, &okY0);
    const double y1 = parseNum(d_edit_inset_ymax, &okY1);
    if (!okX0 || !okX1 || !okY0 || !okY1 || !std::isfinite(x0) || !std::isfinite(x1)
        || !std::isfinite(y0) || !std::isfinite(y1) || qAbs(x1 - x0) < 1e-15
        || qAbs(y1 - y0) < 1e-15) {
        return;
    }

    const QRectF roi(QPointF(qMin(x0, x1), qMin(y0, y1)), QPointF(qMax(x0, x1), qMax(y0, y1)));
    const QRectF cur = d_target_ml->insetZoomDataRect(g);
    if (cur.isValid() && qAbs(cur.left() - roi.left()) <= 1e-12
        && qAbs(cur.right() - roi.right()) <= 1e-12 && qAbs(cur.top() - roi.top()) <= 1e-12
        && qAbs(cur.bottom() - roi.bottom()) <= 1e-12) {
        syncInsetControls();
        return;
    }

    const QString before = d_target_ml->captureUndoSnapshot();
    const bool invoked = QMetaObject::invokeMethod(
            d_target_ml.data(), "setInsetZoomDataRect", Qt::DirectConnection, Q_ARG(Graph *, g),
            Q_ARG(QRectF, roi));
    if (!invoked && g->plotWidget()) {
        Plot *pw = g->plotWidget();
        g->enableAutoscaling(false);
        g->setScale(QwtPlot::xBottom, roi.left(), roi.right(), 0.0,
                    pw->axisMaxMajor(QwtPlot::xBottom), pw->axisMaxMinor(QwtPlot::xBottom));
        g->setScale(QwtPlot::yLeft, roi.top(), roi.bottom(), 0.0,
                    pw->axisMaxMajor(QwtPlot::yLeft), pw->axisMaxMinor(QwtPlot::yLeft));
        g->replot();
    }
    emit g->modifiedGraph();
    d_target_ml->pushUndoSnapshot(tr("Edit Inset Zoom Region"), before, kMergeInsetRoi);
    syncInsetControls();
}

void GraphInspectorDock::syncAxisControls()
{
    Graph *g = activeLayer();
    if (!g || !g->plotWidget())
        return;

    const int ax = qBound(0, d_active_axis, 3);
    if (d_axis_title_timer && d_axis_title_timer->isActive()
        && (d_axis_title_edit_layer.data() != g || d_axis_title_edit_axis != ax)) {
        flushPendingAxisTitleEdit();
    }

    d_syncing_props = true;
    Plot *pw = g->plotWidget();

    if (d_axis_sel_group) {
        if (QAbstractButton *ab = d_axis_sel_group->button(ax)) {
            QSignalBlocker b(ab);
            ab->setChecked(true);
        }
    }

    {
        QSignalBlocker bVis(d_chk_axis_visible);
        QSignalBlocker bTickLbl(d_chk_tick_labels);
        QSignalBlocker bUFmt(d_combo_unit_format);
        QSignalBlocker bMin(d_edit_axis_min);
        QSignalBlocker bMax(d_edit_axis_max);
        QSignalBlocker bStep(d_edit_axis_step);
        QSignalBlocker bScl(d_combo_axis_scale);
        QSignalBlocker bRev(d_chk_axis_reverse);
        QSignalBlocker bMinSub(d_combo_minor_subdiv);
        QSignalBlocker bBrk(d_chk_axis_break);
        QSignalBlocker bBrkF(d_edit_break_from);
        QSignalBlocker bBrkT(d_edit_break_to);
        QSignalBlocker bTDir(d_combo_tick_dir);
        QSignalBlocker bTFmt(d_combo_tick_format);
        QSignalBlocker bTPrec(d_spin_tick_prec);
        QSignalBlocker bTDiv(d_combo_tick_divide);
        QSignalBlocker bCat(d_combo_category_col);
        QSignalBlocker bMajG(d_chk_major_grid);
        QSignalBlocker bMinG(d_chk_minor_grid);
        QSignalBlocker bTit(d_edit_axis_title);
        QSignalBlocker bTitPt(d_spin_axis_title_pt);
        QSignalBlocker bTckPt(d_spin_axis_tick_pt);

        d_chk_axis_visible->setChecked(pw->axisEnabled(ax));
        if (d_chk_tick_labels) {
            d_chk_tick_labels->setChecked(g->hasTickLabelsOnAxis(ax));
            d_chk_tick_labels->setEnabled(pw->axisEnabled(ax));
        }
        if (d_combo_unit_format) {
            // A3-8 / E1-4: read Column::UnitFormat directly (Bracket is native)
            int ufMode = 0;
            if (g->axisUnitFormat() == Column::UnitFormat::ISIJSlash)
                ufMode = 1;
            else if (g->axisUnitFormat() == Column::UnitFormat::Bracket)
                ufMode = 2;
            const int ufIdx = d_combo_unit_format->findData(ufMode);
            if (ufIdx >= 0)
                d_combo_unit_format->setCurrentIndex(ufIdx);
        }

        const QwtScaleDiv *scDiv = pw->axisScaleDiv(ax);
        const double lo = scDiv ? qMin(scDiv->lowerBound(), scDiv->upperBound()) : 0.0;
        const double hi = scDiv ? qMax(scDiv->lowerBound(), scDiv->upperBound()) : 1.0;
        syncLineEditClean(d_edit_axis_min, QString::number(lo, 'g', 8));
        syncLineEditClean(d_edit_axis_max, QString::number(hi, 'g', 8));
        const double stp = g->axisStep(ax);
        syncLineEditClean(d_edit_axis_step,
                          stp > 0.0 ? QString::number(stp, 'g', 8) : QStringLiteral("0"));

        bool isLog10 = false;
        bool isInverted = (scDiv && scDiv->lowerBound() > scDiv->upperBound());
        if (const QwtScaleEngine *eng = pw->axisScaleEngine(ax)) {
            isInverted = eng->testAttribute(QwtScaleEngine::Inverted);
            QwtScaleTransformation *xform = eng->transformation();
            isLog10 = (xform && xform->type() == QwtScaleTransformation::Log10);
            delete xform;
        }
        d_combo_axis_scale->setCurrentIndex(isLog10 ? 1 : 0);
        if (d_chk_axis_reverse)
            d_chk_axis_reverse->setChecked(isInverted);

        if (d_combo_minor_subdiv)
            fillMinorTickCombo(d_combo_minor_subdiv, isLog10, qMax(0, pw->axisMaxMinor(ax)));

        const Graph::AxisBreakSpec brk = g->axisBreak(ax);
        d_chk_axis_break->setChecked(brk.enabled);
        d_lbl_break_from->setVisible(brk.enabled);
        d_edit_break_from->setVisible(brk.enabled);
        d_lbl_break_to->setVisible(brk.enabled);
        d_edit_break_to->setVisible(brk.enabled);
        d_edit_break_from->setEnabled(brk.enabled);
        d_edit_break_to->setEnabled(brk.enabled);
        if (d_lbl_break_scale_before && d_combo_break_scale_before
            && d_lbl_break_scale_after && d_combo_break_scale_after) {
            QSignalBlocker bSb(d_combo_break_scale_before);
            QSignalBlocker bSa(d_combo_break_scale_after);
            d_lbl_break_scale_before->setVisible(brk.enabled);
            d_combo_break_scale_before->setVisible(brk.enabled);
            d_lbl_break_scale_after->setVisible(brk.enabled);
            d_combo_break_scale_after->setVisible(brk.enabled);
            d_combo_break_scale_before->setEnabled(brk.enabled);
            d_combo_break_scale_after->setEnabled(brk.enabled);
            d_combo_break_scale_before->setCurrentIndex(brk.log10Before ? 1 : 0);
            d_combo_break_scale_after->setCurrentIndex(brk.log10After ? 1 : 0);
        }
        const double span = hi - lo;
        const double defFrom = brk.enabled ? brk.breakFrom : (lo + 0.35 * span);
        const double defTo = brk.enabled ? brk.breakTo : (lo + 0.65 * span);
        syncLineEditClean(d_edit_break_from, QString::number(defFrom, 'g', 6));
        syncLineEditClean(d_edit_break_to, QString::number(defTo, 'g', 6));

        if (d_btn_ref_zero_y) {
            QSignalBlocker bRy(d_btn_ref_zero_y);
            d_btn_ref_zero_y->setChecked(hasZeroReferenceLine(g, 0));
        }
        if (d_btn_ref_zero_x) {
            QSignalBlocker bRx(d_btn_ref_zero_x);
            d_btn_ref_zero_x->setChecked(hasZeroReferenceLine(g, 1));
        }

        int majStyle = int(ScaleDraw::In);
        if (const ScaleDraw *sd = dynamic_cast<const ScaleDraw *>(pw->axisScaleDraw(ax)))
            majStyle = sd->majorTicksStyle();
        const int tdIdx = d_combo_tick_dir->findData(majStyle);
        if (tdIdx >= 0)
            d_combo_tick_dir->setCurrentIndex(tdIdx);

        if (d_combo_tick_format) {
            const int fmt = pw->axisLabelFormat(ax);
            const int fmtIdx = d_combo_tick_format->findData(fmt);
            if (fmtIdx >= 0)
                d_combo_tick_format->setCurrentIndex(fmtIdx);
        }
        if (d_spin_tick_prec) {
            const int fmt = pw->axisLabelFormat(ax);
            const int prec = pw->axisLabelPrecision(ax);
            if (fmt == Plot::Automatic && (prec == 4 || prec == 6))
                d_spin_tick_prec->setValue(-1);
            else
                d_spin_tick_prec->setValue(qBound(-1, prec, 9));
        }
        if (d_combo_tick_divide) {
            const double div = g->tickDivideFactor(ax);
            const QString divTxt = QString::number(div, 'g', 8);
            const int foundDiv = d_combo_tick_divide->findText(divTxt);
            if (foundDiv >= 0)
                d_combo_tick_divide->setCurrentIndex(foundDiv);
            else
                d_combo_tick_divide->setEditText(divTxt);
            if (QLineEdit *divEdit = d_combo_tick_divide->lineEdit()) {
                divEdit->setProperty("_syncedText", divEdit->text().trimmed());
                divEdit->setModified(false);
                setLineEditPendingCue(divEdit, false);
            }
        }
        refreshCategoryColumnCombo();

        if (Grid *gr = g->grid()) {
            const bool isX = (ax == QwtPlot::xBottom || ax == QwtPlot::xTop);
            d_chk_major_grid->setChecked(isX ? gr->xEnabled() : gr->yEnabled());
            d_chk_minor_grid->setChecked(isX ? gr->xMinEnabled() : gr->yMinEnabled());
        }

        const bool editingSameAxisTitle =
                (d_axis_title_timer && d_axis_title_timer->isActive()
                 && d_axis_title_edit_layer.data() == g && d_axis_title_edit_axis == ax);
        if (!editingSameAxisTitle) {
            const QString axisTitle = g->axisTitle(ax);
            const bool keepFocusedRaw = d_edit_axis_title->hasFocus()
                    && CanvasInlineTextEditor::normalizeOriginRichText(d_edit_axis_title->text())
                            == axisTitle;
            if (!keepFocusedRaw && d_edit_axis_title->text() != axisTitle)
                d_edit_axis_title->setText(axisTitle);
            d_edit_axis_title->setProperty("_syncedText", axisTitle);
            d_edit_axis_title->setModified(false);
        }
        updateAxisTitlePreview(d_edit_axis_title->text());

        const QFont tf = g->axisTitleFont(ax);
        d_spin_axis_title_pt->setValue(tf.pointSizeF() > 0.0 ? tf.pointSizeF() : 14.0);
        const QFont nf = g->axisFont(ax);
        d_spin_axis_tick_pt->setValue(nf.pointSizeF() > 0.0 ? nf.pointSizeF() : 12.0);
    }

    d_syncing_props = false;
}

void GraphInspectorDock::syncCurveControls()
{
    Graph *g = activeLayer();
    if (!g)
        return;

    d_syncing_props = true;

    // 1) Check if a Spectrogram / Heatmap is selected (or sole item on layer)
    if (Spectrogram *sp = effectiveSelectedSpectrogram(g)) {
        if (d_curve_regular_wrap)
            d_curve_regular_wrap->hide();
        if (d_spectrogram_wrap)
            d_spectrogram_wrap->show();

        const QString matName = stripHtmlToCleanUnicode(
                                        sp->matrix() ? QString(sp->matrix()->name())
                                                     : sp->title().text())
                                        .trimmed();
        d_lbl_active_curve_name->setText(matName.isEmpty() ? tr("Heatmap")
                                                           : tr("Heatmap: %1").arg(matName));

        QSignalBlocker bs1(d_combo_spec_cmap);
        QSignalBlocker bs2(d_chk_spec_reverse);
        QSignalBlocker bsLog(d_chk_spec_log_scale);
        QSignalBlocker bsCells(d_chk_spec_cell_blocks);
        QSignalBlocker bs3(d_chk_spec_colorbar);
        QSignalBlocker bsTitle(d_edit_spec_cb_title);
        QSignalBlocker bs4(d_spin_spec_cb_width);
        QSignalBlocker bs5(d_chk_spec_contours);
        QSignalBlocker bs6(d_spin_spec_levels);
        QSignalBlocker bs7(d_chk_spec_custom_z);
        QSignalBlocker bs8(d_edit_spec_zmin);
        QSignalBlocker bs9(d_edit_spec_zmax);

        const int cmIdx = d_combo_spec_cmap->findData(int(sp->colorMapPreset()));
        if (cmIdx >= 0)
            d_combo_spec_cmap->setCurrentIndex(cmIdx);
        d_chk_spec_reverse->setChecked(sp->reversePalette());
        if (d_chk_spec_log_scale)
            d_chk_spec_log_scale->setChecked(sp->logColorScale());
        if (d_chk_spec_cell_blocks)
            d_chk_spec_cell_blocks->setChecked(sp->cellBlocks());

        const bool hasCb = sp->hasColorScale();
        d_chk_spec_colorbar->setChecked(hasCb);
        if (d_edit_spec_cb_title) {
            d_edit_spec_cb_title->setEnabled(hasCb);
            d_edit_spec_cb_title->setText(sp->colorBarTitle());
        }
        if (d_spec_cb_sym_bar)
            d_spec_cb_sym_bar->setEnabled(hasCb);
        d_spin_spec_cb_width->setEnabled(hasCb);
        d_spin_spec_cb_width->setValue(qBound(6, sp->colorBarWidth(), 64));

        const bool hasContours = sp->testDisplayMode(QwtPlotSpectrogram::ContourMode);
        d_chk_spec_contours->setChecked(hasContours);
        d_spin_spec_levels->setEnabled(hasContours);
        d_spin_spec_levels->setValue(qBound(2, sp->levels(), 1000));

        const bool customZ = sp->hasCustomValueRange();
        d_chk_spec_custom_z->setChecked(customZ);
        d_edit_spec_zmin->setEnabled(customZ);
        d_edit_spec_zmax->setEnabled(customZ);
        const QwtDoubleInterval effRange = sp->effectiveValueRange();
        const double zLo = customZ ? sp->customZMin() : effRange.minValue();
        const double zHi = customZ ? sp->customZMax() : effRange.maxValue();
        syncLineEditClean(d_edit_spec_zmin, QString::number(zLo, 'g', 6));
        syncLineEditClean(d_edit_spec_zmax, QString::number(zHi, 'g', 6));

        d_syncing_props = false;
        return;
    }

    if (d_spectrogram_wrap)
        d_spectrogram_wrap->hide();
    if (d_curve_regular_wrap)
        d_curve_regular_wrap->show();

    const int ci = effectiveSelectedCurveIndex(g, true);
    QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
    const bool hasCurve = (cv != nullptr);

    d_combo_curve_style->setEnabled(hasCurve);
    d_btn_line_color->setEnabled(hasCurve);
    d_spin_line_width->setEnabled(hasCurve);
    d_combo_line_pen->setEnabled(hasCurve);
    if (d_spin_curve_opacity)
        d_spin_curve_opacity->setEnabled(hasCurve);
    d_combo_sym_shape->setEnabled(hasCurve);
    d_spin_sym_size->setEnabled(hasCurve);
    if (d_spin_sym_skip)
        d_spin_sym_skip->setEnabled(hasCurve);
    d_chk_sym_hollow->setEnabled(hasCurve);
    if (d_chk_link_sym_color)
        d_chk_link_sym_color->setEnabled(hasCurve);
    d_chk_curve_right_y->setEnabled(hasCurve);
    if (d_btn_match_right_y_color)
        d_btn_match_right_y_color->setEnabled(hasCurve);
    d_btn_err_adjacent->setEnabled(hasCurve);
    if (d_btn_err_dialog)
        d_btn_err_dialog->setEnabled(hasCurve);

    if (!cv) {
        if (d_err_style_wrap)
            d_err_style_wrap->hide();
        if (d_point_cmap_wrap)
            d_point_cmap_wrap->hide();
        d_lbl_active_curve_name->setText(tr("(No Curve Selected)"));
        d_syncing_props = false;
        return;
    }

    d_lbl_active_curve_name->setText(friendlyCurveDisplayName(g, ci));

    {
        QSignalBlocker b1(d_combo_curve_style);
        QSignalBlocker b2(d_spin_line_width);
        QSignalBlocker b3(d_combo_line_pen);
        QSignalBlocker bOp(d_spin_curve_opacity);
        QSignalBlocker b4(d_combo_sym_shape);
        QSignalBlocker b5(d_spin_sym_size);
        QSignalBlocker b6(d_chk_sym_hollow);
        QSignalBlocker bLink(d_chk_link_sym_color);
        QSignalBlocker bEdgeW(d_spin_sym_edge_width);
        QSignalBlocker b7(d_chk_curve_right_y);

        while (d_combo_curve_style->count() > 10)
            d_combo_curve_style->removeItem(10);
        const int cType = g->curveType(ci);
        const int ctIdx = d_combo_curve_style->findData(cType);
        const bool canConvertStandard =
                (ctIdx >= 0 && !g->isPolarPlot() && !g->isTernaryPlot()
                 && (cType == Graph::Line || Graph::canConvertTo(cv, Graph::Line)));
        if (canConvertStandard) {
            d_combo_curve_style->setEnabled(true);
            d_combo_curve_style->setCurrentIndex(ctIdx);
        } else {
            QString specLabel;
            if (g->isPolarPlot())
                specLabel = tr("Polar");
            else if (g->isTernaryPlot())
                specLabel = tr("Ternary Plot");
            else if (cType == Graph::Histogram)
                specLabel = tr("Histogram");
            else if (cType == Graph::Box)
                specLabel = tr("Box Plot");
            else if (cType == Graph::Pie)
                specLabel = tr("Pie");
            else if (cType == Graph::VectXYXY || cType == Graph::VectXYAM)
                specLabel = tr("Vectors");
            else if (cType == Graph::ErrorBars)
                specLabel = tr("Error Bars");
            else
                specLabel = tr("Function");
            d_combo_curve_style->addItem(specLabel, cType);
            d_combo_curve_style->setCurrentIndex(10);
            d_combo_curve_style->setEnabled(false);
        }

        const QPen lp = cv->pen();
        const QwtSymbol sym = cv->symbol();
        const bool hasLine = (cv->style() != QwtPlotCurve::NoCurve && lp.style() != Qt::NoPen);
        const QColor col = (hasLine && lp.color().isValid()) ? lp.color() : sym.pen().color();
        updateColorSwatchButton(d_btn_line_color, col);

        const double lw = lp.widthF() > 0.0 ? lp.widthF() : double(qMax(1, lp.width()));
        d_spin_line_width->setValue(qBound(0.5, lw, double(PlotStyleLimits::kLineWidthMax)));
        const int penIdx = d_combo_line_pen->findData(int(lp.style()));
        if (penIdx >= 0)
            d_combo_line_pen->setCurrentIndex(penIdx);

        if (d_spin_curve_opacity) {
            const int alpha = col.isValid() ? col.alpha() : 255;
            d_spin_curve_opacity->setValue(qBound(0, qRound(alpha * 100.0 / 255.0), 100));
        }

        const int symIdx = d_combo_sym_shape->findData(int(sym.style()));
        if (symIdx >= 0)
            d_combo_sym_shape->setCurrentIndex(symIdx);
        d_spin_sym_size->setValue(sym.size().width() > 0 ? sym.size().width() : 8);
        if (d_spin_sym_skip) {
            QSignalBlocker bSkip(d_spin_sym_skip);
            const PlotCurve *pc = dynamic_cast<const PlotCurve *>(cv);
            d_spin_sym_skip->setValue(pc ? qBound(0, pc->symbolSkip(), 500) : 0);
        }
        const bool isHollow = Graph::isSymbolWhiteHollow(sym);
        d_chk_sym_hollow->setChecked(isHollow);

        if (d_chk_link_sym_color) {
            const quintptr curPtr = reinterpret_cast<quintptr>(cv);
            const quintptr prevPtr = d_chk_link_sym_color->property("boundCurvePtr").value<quintptr>();
            const bool edgeDiffers = (sym.style() != QwtSymbol::NoSymbol && hasLine
                                      && lp.color().isValid() && sym.pen().color().isValid()
                                      && sym.pen().color().rgb() != lp.color().rgb());
            const bool fillDiffers = (sym.style() != QwtSymbol::NoSymbol && !isHollow
                                      && sym.brush().style() != Qt::NoBrush
                                      && sym.brush().color().isValid() && sym.pen().color().isValid()
                                      && sym.brush().color().rgb() != sym.pen().color().rgb());
            bool linked = true;
            if (edgeDiffers || fillDiffers)
                linked = false;
            else if (curPtr == prevPtr)
                linked = d_chk_link_sym_color->isChecked();
            d_chk_link_sym_color->setChecked(linked);
            d_chk_link_sym_color->setProperty("boundCurvePtr", QVariant::fromValue<quintptr>(curPtr));
            if (d_sym_custom_colors_wrap)
                d_sym_custom_colors_wrap->setVisible(!linked);
        }

        updateColorSwatchButton(d_btn_sym_edge_color,
                                sym.pen().color().isValid() ? sym.pen().color() : col);
        updateColorSwatchButton(
                d_btn_sym_fill_color,
                (sym.brush().style() != Qt::NoBrush && sym.brush().color().isValid())
                        ? sym.brush().color()
                        : QColor(Qt::white));
        if (d_spin_sym_edge_width) {
            const double ew = sym.pen().widthF() > 0.0 ? sym.pen().widthF()
                                                       : double(qMax(1, sym.pen().width()));
            d_spin_sym_edge_width->setValue(
                    qBound(PlotStyleLimits::kSymbolEdgeWidthMin, ew,
                           PlotStyleLimits::kSymbolEdgeWidthMax));
        }

        d_chk_curve_right_y->setChecked(cv->yAxis() == QwtPlot::yRight);
    }

    // 1b) Sync Color-Mapped Scatter (PointColorMap) controls
    const DataCurve *dc = dynamic_cast<const DataCurve *>(cv);
    const bool hasPcm = (dc && dc->hasPointColorMap());
    if (d_point_cmap_wrap) {
        d_point_cmap_wrap->setVisible(hasPcm);
        if (hasPcm) {
            const DataCurve::PointColorMap &pcm = dc->pointColorMap();
            QSignalBlocker bp1(d_combo_pcm_cmap);
            QSignalBlocker bp2(d_chk_pcm_reverse);
            QSignalBlocker bp3(d_chk_pcm_colorbar);
            QSignalBlocker bp4(d_edit_pcm_cb_title);
            QSignalBlocker bp5(d_chk_pcm_custom_z);
            QSignalBlocker bp6(d_edit_pcm_zmin);
            QSignalBlocker bp7(d_edit_pcm_zmax);

            const int pIdx = d_combo_pcm_cmap->findData(pcm.preset);
            if (pIdx >= 0)
                d_combo_pcm_cmap->setCurrentIndex(pIdx);
            d_chk_pcm_reverse->setChecked(pcm.reverse);

            const bool cbVisible = pcm.showColorBar;
            d_chk_pcm_colorbar->setChecked(cbVisible);
            d_edit_pcm_cb_title->setEnabled(cbVisible);
            syncLineEditClean(d_edit_pcm_cb_title,
                              pcm.colorBarTitle.isEmpty() ? g->axisTitle(QwtPlot::yRight)
                                                          : pcm.colorBarTitle);

            const bool customZ = !pcm.autoRange;
            d_chk_pcm_custom_z->setChecked(customZ);
            d_edit_pcm_zmin->setEnabled(customZ);
            d_edit_pcm_zmax->setEnabled(customZ);
            double zLo = pcm.zMin, zHi = pcm.zMax;
            if (pcm.autoRange)
                dc->pointColorRange(&zLo, &zHi);
            syncLineEditClean(d_edit_pcm_zmin, QString::number(zLo, 'g', 6));
            syncLineEditClean(d_edit_pcm_zmax, QString::number(zHi, 'g', 6));
        }
    }
    if (hasPcm) {
        d_chk_sym_hollow->setEnabled(false);
        if (d_chk_link_sym_color)
            d_chk_link_sym_color->setEnabled(false);
        if (d_sym_custom_colors_wrap)
            d_sym_custom_colors_wrap->hide();
    }

    // 2) Sync attached or selected ErrorBar controls
    const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
    if (d_err_style_wrap) {
        d_err_style_wrap->setVisible(!errs.isEmpty());
        if (!errs.isEmpty()) {
            QwtErrorPlotCurve *er = errs.first();
            QSignalBlocker be1(d_spin_err_width);
            QSignalBlocker be2(d_spin_err_cap);
            QSignalBlocker be3(d_combo_err_dir);
            const double ew = er->widthF() > 0.0 ? er->widthF() : double(qMax(1, er->width()));
            d_spin_err_width->setValue(
                    qBound(PlotStyleLimits::kErrorBarWidthMin, ew,
                           PlotStyleLimits::kErrorBarWidthMax));
            d_spin_err_cap->setValue(qBound(0, er->capLength(), 32));
            int dirMode = 0;
            if (er->plusSide() && !er->minusSide())
                dirMode = 1;
            else if (!er->plusSide() && er->minusSide())
                dirMode = 2;
            const int dIdx = d_combo_err_dir->findData(dirMode);
            if (dIdx >= 0)
                d_combo_err_dir->setCurrentIndex(dIdx);
            updateColorSwatchButton(d_btn_err_color, er->color());
            if (d_chk_err_band && d_lbl_err_band_opacity && d_spin_err_band_opacity) {
                QSignalBlocker beBand(d_chk_err_band);
                QSignalBlocker beBandOp(d_spin_err_band_opacity);
                const bool isBand = er->isBand();
                d_chk_err_band->setChecked(isBand);
                d_lbl_err_band_opacity->setVisible(isBand);
                d_spin_err_band_opacity->setVisible(isBand);
                d_spin_err_band_opacity->setEnabled(isBand);
                d_spin_err_band_opacity->setValue(
                        qBound(5, qRound(double(er->bandAlpha()) * 100.0 / 255.0), 100));
            }
        }
    }

    d_syncing_props = false;
}

void GraphInspectorDock::flushPendingAnnotationEdit()
{
    if (d_annot_commit_timer)
        d_annot_commit_timer->stop();
    Graph *g = d_annot_edit_layer.data();
    const int id = d_annot_edit_id;
    d_annot_edit_layer = nullptr;
    d_annot_edit_id = -1;
    // E5-2: resolve the owning MultiLayer from g itself so edits flush reliably even when
    // switching graphs or closing a graph window (when d_target_ml may already be null or changed).
    if (!g || id < 0 || !d_edit_annot_text)
        return;
    MultiLayer *ml = g->multiLayerParent();
    Legend *leg = g->textMarker(id);
    if (!leg)
        return;
    const QString nextTxt =
            CanvasInlineTextEditor::normalizeOriginRichText(d_edit_annot_text->toPlainText());
    if (leg->text() == nextTxt)
        return;
    const QString before = ml ? ml->captureUndoSnapshot() : QString();
    leg->setText(nextTxt);
    g->replot();
    emit g->modifiedGraph();
    if (ml && !before.isEmpty())
        ml->pushUndoSnapshot(tr("Edit Annotation Text"), before, kMergeLegendHeader);
    if (ml && ml == d_target_ml.data())
        refreshObjectTree();
}

void GraphInspectorDock::rebuildLegendCurveEntryRows(Graph *g, Legend *leg)
{
    if (!d_leg_entries_wrap || !d_leg_entries_layout)
        return;

    static const QRegularExpression rxLegendEntry(
            QStringLiteral("^\\\\(?:c\\{(\\d+)\\}|l\\((\\d+)\\))\\s?(.*)$"));

    if (!g || g->isPiePlot()) {
        while (QLayoutItem *it = d_leg_entries_layout->takeAt(0)) {
            if (QWidget *w = it->widget()) {
                w->hide();
                w->setParent(nullptr);
                w->deleteLater();
            }
            delete it;
        }
        d_leg_curve_rows.clear();
        d_leg_rows_graph = nullptr;
        d_leg_entries_wrap->hide();
        return;
    }

    const QStringList existingLines = leg ? leg->text().split(QLatin1Char('\n')) : QStringList();
    QHash<int, QString> parsedByNum;
    for (const QString &ln : existingLines) {
        const QRegularExpressionMatch m = rxLegendEntry.match(ln.trimmed());
        if (!m.hasMatch())
            continue;
        bool ok = false;
        const int n = !m.captured(1).isEmpty() ? m.captured(1).toInt(&ok)
                                               : m.captured(2).toInt(&ok);
        if (ok && !parsedByNum.contains(n))
            parsedByNum.insert(n, m.captured(3));
    }

    struct CurveEntrySpec {
        int ci = -1;
        QString label;
    };
    QVector<CurveEntrySpec> specs;
    const int maxRows = 24;
    for (int ci = 0; ci < g->curves() && specs.size() < maxRows; ++ci) {
        QwtPlotCurve *cv = g->curve(ci);
        if (!cv || cv->rtti() == QwtPlotItem::Rtti_PlotSpectrogram
            || g->curveType(ci) == Graph::ErrorBars
            || !cv->testItemAttribute(QwtPlotItem::Legend)) {
            continue;
        }

        const int num = ci + 1;
        const QString autoMacro = QStringLiteral("%(%1)").arg(num);
        QString autoLbl = g->resolvedAutoCurveLabel(ci).trimmed();
        if (autoLbl.isEmpty())
            autoLbl = cv->title().text().trimmed();

        QString currentLbl = autoLbl;
        const auto itParsed = parsedByNum.constFind(num);
        if (itParsed != parsedByNum.constEnd()) {
            const QString &tail = itParsed.value();
            if (!tail.isEmpty() && tail.trimmed() != autoMacro)
                currentLbl = tail;
        }
        specs.append({ ci, currentLbl });
    }

    bool canReuse = (d_leg_rows_graph == g && d_leg_curve_rows.size() == specs.size());
    for (int i = 0; canReuse && i < specs.size(); ++i) {
        if (d_leg_curve_rows[i].curveIndex != specs[i].ci || !d_leg_curve_rows[i].edit
            || !d_leg_curve_rows[i].swatchLabel || !d_leg_curve_rows[i].previewLabel) {
            canReuse = false;
        }
    }
    if (canReuse) {
        for (int i = 0; i < specs.size(); ++i) {
            LegendCurveRow &row = d_leg_curve_rows[i];
            row.swatchLabel->setPixmap(buildCurveSwatchIcon(g, specs[i].ci).pixmap(22, 14));
            if (!row.edit->hasFocus() && !row.edit->isModified())
                syncLineEditClean(row.edit, specs[i].label);
            updateRichPreviewLabel(row.previewLabel, row.edit->text(), false);
            if (row.resetBtn)
                row.resetBtn->setToolTip(tr("Reset to automatic curve name"));
        }
        d_leg_entries_wrap->setVisible(!specs.isEmpty());
        return;
    }

    while (QLayoutItem *it = d_leg_entries_layout->takeAt(0)) {
        if (QWidget *w = it->widget()) {
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete it;
    }
    d_leg_curve_rows.clear();
    d_leg_rows_graph = g;

    for (const CurveEntrySpec &spec : specs) {
        const int ci = spec.ci;
        QWidget *rowW = new QWidget(d_leg_entries_wrap);
        QVBoxLayout *rowWrapLay = new QVBoxLayout(rowW);
        rowWrapLay->setContentsMargins(0, 0, 0, 0);
        rowWrapLay->setSpacing(2);

        QHBoxLayout *rowL = new QHBoxLayout();
        rowL->setContentsMargins(0, 0, 0, 0);
        rowL->setSpacing(4);

        QLabel *swLbl = new QLabel(rowW);
        swLbl->setPixmap(buildCurveSwatchIcon(g, ci).pixmap(22, 14));
        swLbl->setFixedSize(24, 16);

        QLineEdit *edit = new QLineEdit(rowW);
        edit->installEventFilter(this);
        syncLineEditClean(edit, spec.label);
        wirePendingEditCue(edit);

        QLabel *previewLbl = new QLabel(rowW);
        previewLbl->setObjectName(QStringLiteral("InspectorRichPreviewLabel"));
        previewLbl->setTextFormat(Qt::RichText);
        previewLbl->setWordWrap(true);
        updateRichPreviewLabel(previewLbl, spec.label, false);

        QToolButton *resetBtn = new QToolButton(rowW);
        resetBtn->setIcon(IconLoader::duotoneIcon(QStringLiteral("undo"), 12));
        resetBtn->setFixedSize(22, 22);
        resetBtn->setAutoRaise(true);
        resetBtn->setFocusPolicy(Qt::NoFocus);
        resetBtn->setCursor(Qt::PointingHandCursor);
        resetBtn->setToolTip(tr("Reset to automatic curve name"));

        const QPointer<Graph> gp(g);
        auto commitCurveLabel = [this, gp, ci](const QString &newLbl) {
            if (d_syncing_props || !gp || !d_target_ml)
                return;
            if (!gp->hasLegend())
                gp->newLegend();
            Legend *curLeg = gp->legend();
            if (!curLeg)
                return;

            const int curveNum = ci + 1;
            QStringList lines = curLeg->text().split(QLatin1Char('\n'));
            while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
                lines.removeLast();

            const QString normLbl = CanvasInlineTextEditor::normalizeOriginRichText(newLbl);
            const QString newLine = QStringLiteral("\\c{%1}%2").arg(curveNum).arg(normLbl);
            bool replaced = false;
            for (int li = 0; li < lines.size(); ++li) {
                const QRegularExpressionMatch m = rxLegendEntry.match(lines[li].trimmed());
                if (!m.hasMatch())
                    continue;
                bool ok = false;
                const int parsedNum = !m.captured(1).isEmpty() ? m.captured(1).toInt(&ok)
                                                               : m.captured(2).toInt(&ok);
                if (ok && parsedNum == curveNum) {
                    lines[li] = newLine;
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
                lines.append(newLine);

            const QString updated = lines.join(QStringLiteral("\n"));
            if (curLeg->text() == updated)
                return;

            const QString before = d_target_ml->captureUndoSnapshot();
            curLeg->setText(updated);
            gp->replot();
            emit gp->modifiedGraph();
            d_target_ml->pushUndoSnapshot(tr("Edit Legend Label"), before, kMergeLegendHeader);
        };

        connect(edit, &QLineEdit::textChanged, this, [this, previewLbl](const QString &txt) {
            updateRichPreviewLabel(previewLbl, txt, false);
        });
        connect(edit, &QLineEdit::editingFinished, this, [edit, commitCurveLabel]() {
            const QString rawTrimmed = edit->text().trimmed();
            const QString normTrimmed =
                    CanvasInlineTextEditor::normalizeOriginRichText(rawTrimmed).trimmed();
            const QString synced = edit->property("_syncedText").toString();
            const bool isDiff = (normTrimmed != synced);
            if (!edit->isModified() && !isDiff) {
                setLineEditPendingCue(edit, false);
                return;
            }
            edit->setProperty("_syncedText", normTrimmed);
            edit->setModified(false);
            setLineEditPendingCue(edit, false);
            if (!isDiff)
                return;
            commitCurveLabel(edit->text());
        });

        connect(resetBtn, &QToolButton::clicked, this, [gp, ci, edit, commitCurveLabel]() {
            if (!gp || ci < 0 || ci >= gp->curves())
                return;
            QwtPlotCurve *c = gp->curve(ci);
            QString resetLbl = gp->resolvedAutoCurveLabel(ci).trimmed();
            if (resetLbl.isEmpty() && c)
                resetLbl = c->title().text().trimmed();
            syncLineEditClean(edit, resetLbl);
            commitCurveLabel(resetLbl);
        });

        rowL->addWidget(swLbl, 0);
        rowL->addWidget(edit, 1);
        rowL->addWidget(resetBtn, 0);
        rowWrapLay->addLayout(rowL);
        rowWrapLay->addWidget(previewLbl);
        d_leg_entries_layout->addWidget(rowW);
        d_leg_curve_rows.append({ ci, rowW, swLbl, edit, previewLbl, resetBtn });
    }

    d_leg_entries_wrap->setVisible(!specs.isEmpty());
}

void GraphInspectorDock::applyArrowControlsFromUi(const QString &undoLabel, int mergeId)
{
    Graph *g = activeLayer();
    if (d_syncing_props || !d_target_ml || !g)
        return;
    const auto sel = g->currentSelection();
    if (sel.kind != Graph::GraphSelection::ArrowAnnotation)
        return;
    ArrowMarker *am = g->arrow(sel.primaryId);
    if (!am)
        return;

    const QString before = d_target_ml->captureUndoSnapshot();
    if (d_spin_arrow_width)
        am->setWidthF(d_spin_arrow_width->value());
    if (d_combo_arrow_style && d_combo_arrow_style->currentIndex() >= 0)
        am->setStyle(static_cast<Qt::PenStyle>(d_combo_arrow_style->currentData().toInt()));
    if (d_chk_arrow_start)
        am->drawStartArrow(d_chk_arrow_start->isChecked());
    if (d_chk_arrow_end)
        am->drawEndArrow(d_chk_arrow_end->isChecked());
    if (d_spin_arrow_head_len)
        am->setHeadLength(d_spin_arrow_head_len->value());
    if (d_spin_arrow_head_angle)
        am->setHeadAngle(d_spin_arrow_head_angle->value());
    if (d_chk_arrow_filled)
        am->fillArrowHead(d_chk_arrow_filled->isChecked());

    const bool anyHead = am->hasStartArrow() || am->hasEndArrow();
    if (d_spin_arrow_head_len)
        d_spin_arrow_head_len->setEnabled(anyHead);
    if (d_spin_arrow_head_angle)
        d_spin_arrow_head_angle->setEnabled(anyHead);
    if (d_chk_arrow_filled)
        d_chk_arrow_filled->setEnabled(anyHead);

    g->replot();
    emit g->modifiedGraph();
    d_target_ml->pushUndoSnapshot(undoLabel, before, mergeId);
}

void GraphInspectorDock::applyImageGeometryFromUi()
{
    Graph *g = activeLayer();
    if (d_syncing_props || !d_target_ml || !g)
        return;
    const auto sel = g->currentSelection();
    if (sel.kind != Graph::GraphSelection::ImageAnnotation)
        return;
    ImageMarker *im = g->imageMarker(sel.primaryId);
    if (!im || !im->plot() || !d_spin_img_x || !d_spin_img_y || !d_spin_img_w || !d_spin_img_h)
        return;

    const int nx = d_spin_img_x->value();
    const int ny = d_spin_img_y->value();
    const int nw = qMax(4, d_spin_img_w->value());
    const int nh = qMax(4, d_spin_img_h->value());
    const QRect cur = im->rect();
    if (cur.x() == nx && cur.y() == ny && cur.width() == nw && cur.height() == nh)
        return;

    const QString before = d_target_ml->captureUndoSnapshot();
    im->setRect(nx, ny, nw, nh);
    g->replot();
    emit g->modifiedGraph();
    d_target_ml->pushUndoSnapshot(tr("Change Image Geometry"), before, kMergeImageGeom);
}

void GraphInspectorDock::applyPlotTitleFromUi(const QString &undoLabel, int mergeId)
{
    Graph *g = activeLayer();
    if (d_syncing_props || !d_target_ml || !g || !g->plotWidget())
        return;

    const QwtText curTitle = g->plotWidget()->title();
    const QString nextTxt = d_edit_plot_title
            ? CanvasInlineTextEditor::normalizeOriginRichText(d_edit_plot_title->text())
            : curTitle.text();
    QFont nextFont = curTitle.font();
    if (nextFont.family().trimmed().isEmpty())
        nextFont.setFamily(QStringLiteral("Arial"));
    if (d_spin_plot_title_pt)
        nextFont.setPointSizeF(d_spin_plot_title_pt->value());
    if (d_chk_plot_title_bold)
        nextFont.setBold(d_chk_plot_title_bold->isChecked());

    if (d_edit_plot_title) {
        d_edit_plot_title->setProperty("_syncedText", nextTxt.trimmed());
        d_edit_plot_title->setModified(false);
        setLineEditPendingCue(d_edit_plot_title, false);
    }

    if (curTitle.text() == nextTxt && curTitle.font() == nextFont)
        return;

    const QString before = d_target_ml->captureUndoSnapshot();
    g->setTitle(nextTxt);
    g->setTitleFont(nextFont);
    g->replot();
    emit g->modifiedGraph();
    d_target_ml->pushUndoSnapshot(undoLabel, before, mergeId);
    refreshObjectTree();
}

void GraphInspectorDock::syncLegendControls()
{
    // An edit still waiting for its debounce belongs to the annotation selected when typing
    // began; write it before the editor is refilled from the (possibly new) selection.
    if (d_annot_commit_timer && d_annot_commit_timer->isActive())
        flushPendingAnnotationEdit();

    Graph *g = activeLayer();
    if (!g)
        return;

    d_syncing_props = true;

    const auto sel = g->currentSelection();
    const auto selKind = sel.kind;
    const bool isArrowAnn = (selKind == Graph::GraphSelection::ArrowAnnotation);
    const bool isImageAnn = (selKind == Graph::GraphSelection::ImageAnnotation);
    const bool isPlotTitle = (selKind == Graph::GraphSelection::PlotTitle);
    const bool isTextAnn = (selKind == Graph::GraphSelection::TextAnnotation);

    if (d_arrow_editor_wrap)
        d_arrow_editor_wrap->setVisible(isArrowAnn);
    if (d_image_editor_wrap)
        d_image_editor_wrap->setVisible(isImageAnn);
    if (d_plot_title_wrap)
        d_plot_title_wrap->setVisible(isPlotTitle);
    if (d_leg_text_wrap)
        d_leg_text_wrap->setVisible(!isArrowAnn && !isImageAnn && !isPlotTitle);

    // Keep Suite 63e invariant: d_edit_annot_text is ALWAYS hidden unless TextAnnotation is selected.
    if (d_edit_annot_text)
        d_edit_annot_text->setHidden(!isTextAnn);
    if (d_edit_leg_header)
        d_edit_leg_header->setHidden(isTextAnn && d_edit_annot_text);

    // A3-5 / C2-3: Arrow / Line Annotation editor
    if (isArrowAnn) {
        d_lbl_legend_target->setText(tr("Arrow / Line Annotation"));
        ArrowMarker *am = g->arrow(sel.primaryId);
        const bool hasAm = (am != nullptr);
        if (d_btn_arrow_color)
            d_btn_arrow_color->setEnabled(hasAm);
        if (d_spin_arrow_width)
            d_spin_arrow_width->setEnabled(hasAm);
        if (d_combo_arrow_style)
            d_combo_arrow_style->setEnabled(hasAm);
        if (d_chk_arrow_start)
            d_chk_arrow_start->setEnabled(hasAm);
        if (d_chk_arrow_end)
            d_chk_arrow_end->setEnabled(hasAm);
        if (am) {
            QSignalBlocker bw(d_spin_arrow_width);
            QSignalBlocker bs(d_combo_arrow_style);
            QSignalBlocker bSt(d_chk_arrow_start);
            QSignalBlocker bEn(d_chk_arrow_end);
            QSignalBlocker bHl(d_spin_arrow_head_len);
            QSignalBlocker bHa(d_spin_arrow_head_angle);
            QSignalBlocker bFl(d_chk_arrow_filled);

            updateColorSwatchButton(d_btn_arrow_color, am->color());
            const double aw = am->widthF() > 0.0 ? am->widthF() : double(qMax(1, am->width()));
            d_spin_arrow_width->setValue(qBound(0.5, aw, 20.0));
            const int stIdx = d_combo_arrow_style->findData(int(am->style()));
            d_combo_arrow_style->setCurrentIndex(stIdx >= 0 ? stIdx : 0);
            d_chk_arrow_start->setChecked(am->hasStartArrow());
            d_chk_arrow_end->setChecked(am->hasEndArrow());
            d_spin_arrow_head_len->setValue(qBound(1, am->headLength(), 60));
            d_spin_arrow_head_angle->setValue(qBound(5, am->headAngle(), 85));
            d_chk_arrow_filled->setChecked(am->filledArrowHead());
            const bool anyHead = am->hasStartArrow() || am->hasEndArrow();
            d_spin_arrow_head_len->setEnabled(anyHead);
            d_spin_arrow_head_angle->setEnabled(anyHead);
            d_chk_arrow_filled->setEnabled(anyHead);
        } else {
            if (d_spin_arrow_head_len)
                d_spin_arrow_head_len->setEnabled(false);
            if (d_spin_arrow_head_angle)
                d_spin_arrow_head_angle->setEnabled(false);
            if (d_chk_arrow_filled)
                d_chk_arrow_filled->setEnabled(false);
        }
        d_syncing_props = false;
        return;
    }

    // A3-5 / C2-3: Image Annotation editor
    if (isImageAnn) {
        d_lbl_legend_target->setText(tr("Image Annotation"));
        ImageMarker *im = g->imageMarker(sel.primaryId);
        const bool hasIm = (im != nullptr);
        if (d_spin_img_x)
            d_spin_img_x->setEnabled(hasIm);
        if (d_spin_img_y)
            d_spin_img_y->setEnabled(hasIm);
        if (d_spin_img_w)
            d_spin_img_w->setEnabled(hasIm);
        if (d_spin_img_h)
            d_spin_img_h->setEnabled(hasIm);
        if (d_btn_img_replace)
            d_btn_img_replace->setEnabled(hasIm);
        if (im) {
            const QString base = QFileInfo(im->fileName()).fileName();
            if (d_lbl_img_filename) {
                d_lbl_img_filename->setText(base.isEmpty() ? tr("File: (none)")
                                                           : tr("File: %1").arg(base));
                d_lbl_img_filename->setToolTip(im->fileName());
            }
            const QRect r = im->plot() ? im->rect() : QRect();
            QSignalBlocker bx(d_spin_img_x);
            QSignalBlocker by(d_spin_img_y);
            QSignalBlocker bw(d_spin_img_w);
            QSignalBlocker bh(d_spin_img_h);
            d_spin_img_x->setValue(r.x());
            d_spin_img_y->setValue(r.y());
            d_spin_img_w->setValue(qMax(4, r.width()));
            d_spin_img_h->setValue(qMax(4, r.height()));
        } else if (d_lbl_img_filename) {
            d_lbl_img_filename->setText(tr("File: (none)"));
            d_lbl_img_filename->setToolTip(QString());
        }
        d_syncing_props = false;
        return;
    }

    // A3-5: Plot Title editor
    if (isPlotTitle) {
        d_lbl_legend_target->setText(tr("Plot Title"));
        if (g->plotWidget()) {
            const QwtText t = g->plotWidget()->title();
            QSignalBlocker bt(d_edit_plot_title);
            QSignalBlocker bp(d_spin_plot_title_pt);
            QSignalBlocker bb(d_chk_plot_title_bold);
            syncLineEditClean(d_edit_plot_title, t.text());
            updateRichPreviewLabel(d_lbl_plot_title_preview, t.text(), false);
            const QFont tf = t.font();
            d_spin_plot_title_pt->setValue(
                    qBound(PlotStyleLimits::kFontSizeMin,
                           tf.pointSizeF() > 0.0 ? tf.pointSizeF() : 16.0,
                           PlotStyleLimits::kFontSizeMax));
            d_chk_plot_title_bold->setChecked(tf.bold());
            updateColorSwatchButton(d_btn_plot_title_color,
                                    t.color().isValid() ? t.color() : QColor(Qt::black));
        }
        d_syncing_props = false;
        return;
    }

    Legend *leg = isTextAnn ? g->textMarker(sel.primaryId) : g->legend();
    const bool isPrimaryLegend = (g && leg && leg == g->legend())
            || (g && !leg && selKind == Graph::GraphSelection::LegendBox);
    const bool canEditMarker = isPrimaryLegend || (isTextAnn && leg != nullptr);

    d_lbl_legend_target->setText(isPrimaryLegend ? tr("Graph Legend") : tr("Text Annotation"));
    d_lbl_leg_header->setText(isPrimaryLegend ? tr("Header:") : tr("Text:"));

    if (d_btn_leg_tl)
        d_btn_leg_tl->setEnabled(isPrimaryLegend);
    if (d_btn_leg_tc)
        d_btn_leg_tc->setEnabled(isPrimaryLegend);
    if (d_btn_leg_tr)
        d_btn_leg_tr->setEnabled(isPrimaryLegend);
    if (d_btn_leg_bl)
        d_btn_leg_bl->setEnabled(isPrimaryLegend);
    if (d_btn_leg_br)
        d_btn_leg_br->setEnabled(isPrimaryLegend);
    if (d_btn_leg_out_r)
        d_btn_leg_out_r->setEnabled(isPrimaryLegend);
    if (d_btn_leg_auto_avoid)
        d_btn_leg_auto_avoid->setEnabled(isPrimaryLegend);
    if (d_btn_leg_rebuild)
        d_btn_leg_rebuild->setEnabled(isPrimaryLegend);
    if (d_spin_leg_cols)
        d_spin_leg_cols->setEnabled(isPrimaryLegend);
    if (d_combo_leg_frame)
        d_combo_leg_frame->setEnabled(canEditMarker);
    if (d_edit_leg_header)
        d_edit_leg_header->setEnabled(canEditMarker);
    if (d_edit_annot_text)
        d_edit_annot_text->setEnabled(canEditMarker);
    if (d_leg_sym_bar)
        d_leg_sym_bar->setEnabled(canEditMarker);
    if (d_spin_leg_font_pt)
        d_spin_leg_font_pt->setEnabled(canEditMarker);

    if (leg) {
        QSignalBlocker b1(d_combo_leg_frame);
        QSignalBlocker b2(d_edit_leg_header);
        QSignalBlocker b3(d_spin_leg_cols);
        QSignalBlocker b4(d_spin_leg_font_pt);

        int frameMode = 0;
        if (leg->frameStyle() != Legend::None) {
            frameMode = (leg->backgroundColor().alpha() > 10) ? 1 : 2;
        }
        const int fmIdx = d_combo_leg_frame->findData(frameMode);
        if (fmIdx >= 0)
            d_combo_leg_frame->setCurrentIndex(fmIdx);

        if (!isPrimaryLegend) {
            const QStringList lines = leg->text().split('\n');
            syncLineEditClean(d_edit_leg_header, lines.isEmpty() ? QString() : lines.first());
            // Refill only when the text differs, so the cursor stays put while typing.
            if (d_edit_annot_text && d_edit_annot_text->toPlainText() != leg->text()) {
                const bool keepFocusedRaw = d_edit_annot_text->hasFocus()
                        && CanvasInlineTextEditor::normalizeOriginRichText(
                                   d_edit_annot_text->toPlainText())
                                == leg->text();
                if (!keepFocusedRaw) {
                    QSignalBlocker bAnn(d_edit_annot_text);
                    d_edit_annot_text->setPlainText(leg->text());
                }
            }
        } else {
            const QStringList lines = leg->text().split('\n');
            if (!lines.isEmpty() && !lines.first().trimmed().isEmpty()
                && !isLegendEntryLine(lines.first())) {
                syncLineEditClean(d_edit_leg_header, lines.first().trimmed());
            } else {
                syncLineEditClean(d_edit_leg_header, QString());
            }
            d_spin_leg_cols->setValue(qBound(1, leg->numColumns(), 4));
        }

        const QFont lf = leg->font();
        d_spin_leg_font_pt->setValue(lf.pointSizeF() > 0.0 ? lf.pointSizeF() : 12.0);
    } else {
        QSignalBlocker b(d_edit_leg_header);
        syncLineEditClean(d_edit_leg_header, QString());
        if (d_edit_annot_text) {
            QSignalBlocker bAnn(d_edit_annot_text);
            d_edit_annot_text->clear();
        }
    }

    updateRichPreviewLabel(
            d_lbl_leg_preview,
            (isTextAnn && d_edit_annot_text) ? d_edit_annot_text->toPlainText()
                                             : (d_edit_leg_header ? d_edit_leg_header->text()
                                                                  : QString()),
            false);

    if (isPrimaryLegend)
        rebuildLegendCurveEntryRows(g, leg);
    else if (d_leg_entries_wrap)
        d_leg_entries_wrap->hide();

    d_syncing_props = false;
}

void GraphInspectorDock::applyAxisScaleFromUi(int mergeId, bool fromRangeEdits)
{
    if (!d_edit_axis_min || !d_edit_axis_max || !d_edit_axis_step || !d_combo_axis_scale)
        return;

    Graph *g = activeLayer();
    const QVector<Graph *> targets = targetLayersForScope();
    if (d_syncing_props || !d_target_ml || !g || !g->plotWidget() || targets.isEmpty())
        return;

    // E5-6: ignore no-op focus-out on Min/Max/Step so simply clicking or tabbing through the
    // line edits never disables AutoScale, locks the axis, or pushes a phantom Undo entry.
    if (fromRangeEdits) {
        const bool anyDiff = (d_edit_axis_min->text().trimmed()
                              != d_edit_axis_min->property("_syncedText").toString())
                || (d_edit_axis_max->text().trimmed()
                    != d_edit_axis_max->property("_syncedText").toString())
                || (d_edit_axis_step->text().trimmed()
                    != d_edit_axis_step->property("_syncedText").toString());
        if (!anyDiff) {
            for (QLineEdit *le : { d_edit_axis_min, d_edit_axis_max, d_edit_axis_step }) {
                le->setModified(false);
                setLineEditPendingCue(le, false);
            }
            return;
        }
    }

    bool okMin = false, okMax = false, okStep = false;
    double lo = d_edit_axis_min->text().trimmed().toDouble(&okMin);
    double hi = d_edit_axis_max->text().trimmed().toDouble(&okMax);
    double stp = d_edit_axis_step->text().trimmed().toDouble(&okStep);
    if (!okMin || !okMax || !std::isfinite(lo) || !std::isfinite(hi))
        return;
    if (!okStep || !std::isfinite(stp) || stp < 0.0)
        stp = 0.0;

    const int scaleType = d_combo_axis_scale->currentData().toInt(); // 0=Linear, 1=Log10
    if (scaleType == 1) {
        if (lo <= 0.0 && hi <= 0.0) {
            lo = 0.1;
            hi = 100.0;
        } else if (lo <= 0.0) {
            lo = qMax(1e-6, hi * 1e-4);
        }
    }
    if (qAbs(hi - lo) < 1e-30)
        hi = lo + 1.0;

    Plot *pw = g->plotWidget();
    if (fromRangeEdits && targets.size() == 1) {
        const QwtScaleDiv *scDiv = pw->axisScaleDiv(d_active_axis);
        const double curLo = scDiv ? qMin(scDiv->lowerBound(), scDiv->upperBound()) : 0.0;
        const double curHi = scDiv ? qMax(scDiv->lowerBound(), scDiv->upperBound()) : 1.0;
        const double curStp = g->axisStep(d_active_axis);
        const double eps = qMax(1e-12, qAbs(curHi - curLo) * 1e-9);
        if (qAbs(lo - curLo) <= eps && qAbs(hi - curHi) <= eps
            && qAbs(stp - qMax(0.0, curStp)) <= eps) {
            syncAxisControls();
            return;
        }
    }

    const QwtScaleEngine *eng = pw->axisScaleEngine(d_active_axis);
    const bool inv = d_chk_axis_reverse
            ? d_chk_axis_reverse->isChecked()
            : (eng ? eng->testAttribute(QwtScaleEngine::Inverted) : false);
    const int minorSubdiv = minorTickComboValue(d_combo_minor_subdiv, pw->axisMaxMinor(d_active_axis));

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg || !lg->plotWidget())
            continue;
        Plot *lpw = lg->plotWidget();
        lg->enableAutoscaling(false);
        lg->setScale(d_active_axis, lo, hi, stp, lpw->axisMaxMajor(d_active_axis), minorSubdiv,
                     scaleType, inv);
        lg->setAxisUserLocked(d_active_axis, true);
        syncLinkedChildrenOf(d_target_ml, lg);
        lg->replot();
    }
    d_target_ml->pushUndoSnapshot(tr("Modify Axis Scale"), before, mergeId);
    syncAxisControls();
}

void GraphInspectorDock::applyAxisBreakFromUi(bool fromBreakEdits)
{
    Graph *g = activeLayer();
    const QVector<Graph *> targets = targetLayersForScope();
    if (d_syncing_props || !d_target_ml || !g || targets.isEmpty())
        return;

    const bool enabled = d_chk_axis_break->isChecked();
    if (fromBreakEdits && d_edit_break_from && d_edit_break_to) {
        const bool anyDiff = (d_edit_break_from->text().trimmed()
                              != d_edit_break_from->property("_syncedText").toString())
                || (d_edit_break_to->text().trimmed()
                    != d_edit_break_to->property("_syncedText").toString());
        if (!anyDiff) {
            d_edit_break_from->setModified(false);
            d_edit_break_to->setModified(false);
            setLineEditPendingCue(d_edit_break_from, false);
            setLineEditPendingCue(d_edit_break_to, false);
            return;
        }
    }

    if (!enabled) {
        const QString before = d_target_ml->captureUndoSnapshot();
        for (Graph *lg : targets) {
            if (lg)
                lg->clearAxisBreak(d_active_axis);
        }
        d_target_ml->pushUndoSnapshot(tr("Clear Axis Break"), before, kMergeAxisBreak);
        syncAxisControls();
        return;
    }

    bool okFrom = false, okTo = false;
    const double bf = d_edit_break_from->text().trimmed().toDouble(&okFrom);
    const double bt = d_edit_break_to->text().trimmed().toDouble(&okTo);
    if (!okFrom || !okTo || !std::isfinite(bf) || !std::isfinite(bt) || qAbs(bt - bf) < 1e-12)
        return;

    if (fromBreakEdits && targets.size() == 1) {
        const Graph::AxisBreakSpec cur = g->axisBreak(d_active_axis);
        if (cur.enabled && qAbs(cur.breakFrom - bf) <= 1e-12 && qAbs(cur.breakTo - bt) <= 1e-12) {
            syncAxisControls();
            return;
        }
    }

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg)
            continue;
        Graph::AxisBreakSpec spec = lg->axisBreak(d_active_axis);
        spec.enabled = true;
        spec.breakFrom = bf;
        spec.breakTo = bt;
        if (d_combo_break_scale_before)
            spec.log10Before = (d_combo_break_scale_before->currentData().toInt() == 1);
        if (d_combo_break_scale_after)
            spec.log10After = (d_combo_break_scale_after->currentData().toInt() == 1);
        if (spec.positionPct < 0.10 || spec.positionPct > 0.90)
            spec.positionPct = 0.50;
        lg->setAxisBreak(d_active_axis, spec);
    }
    d_target_ml->pushUndoSnapshot(tr("Set Axis Break"), before, kMergeAxisBreak);
    syncAxisControls();
}

void GraphInspectorDock::applyTickFormatFromUi(int mergeId)
{
    Graph *g = activeLayer();
    const QVector<Graph *> targets = targetLayersForScope();
    if (d_syncing_props || !d_target_ml || !g || !g->plotWidget() || targets.isEmpty())
        return;

    const int rawFmt = d_combo_tick_format ? d_combo_tick_format->currentData().toInt() : 0;
    const int uiPrec = d_spin_tick_prec ? d_spin_tick_prec->value() : -1;

    int qwtFmt = rawFmt;
    bool applyEngineeringFactor = false;
    double engFactor = 1.0;
    if (rawFmt == 4) { // Engineering (power-of-1000 divide factor)
        qwtFmt = int(Plot::Automatic);
        applyEngineeringFactor = true;
        if (const QwtScaleDiv *scDiv = g->plotWidget()->axisScaleDiv(d_active_axis)) {
            const double maxAbs =
                    qMax(std::fabs(scDiv->lowerBound()), std::fabs(scDiv->upperBound()));
            if (std::isfinite(maxAbs) && maxAbs > 0.0) {
                const int exp3 = int(std::floor(std::log10(maxAbs) / 3.0)) * 3;
                if (exp3 != 0)
                    engFactor = std::pow(10.0, double(exp3));
            }
        }
    }

    int effPrec = uiPrec;
    if (effPrec < 0) {
        effPrec = (qwtFmt == int(Plot::Decimal)) ? 2 : ((qwtFmt == int(Plot::Automatic)) ? 6 : 2);
    }

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg)
            continue;
        if (applyEngineeringFactor) {
            lg->setTickLabelTransform(d_active_axis, engFactor, lg->tickPrefix(d_active_axis),
                                      lg->tickSuffix(d_active_axis));
        }
        const QString formula = lg->getAxesFormulas().value(d_active_axis);
        lg->setLabelsNumericFormat(d_active_axis, qwtFmt, effPrec, formula);
        lg->replot();
        emit lg->modifiedGraph();
    }
    if (applyEngineeringFactor && d_combo_tick_divide) {
        QSignalBlocker bDiv(d_combo_tick_divide);
        d_combo_tick_divide->setEditText(QString::number(engFactor, 'g', 8));
    }
    d_target_ml->pushUndoSnapshot(tr("Change Tick Format"), before, mergeId);
}

void GraphInspectorDock::applyTickDivideFromUi(bool fromLineEdit)
{
    const QVector<Graph *> targets = targetLayersForScope();
    if (d_syncing_props || !d_target_ml || targets.isEmpty() || !d_combo_tick_divide)
        return;

    QLineEdit *divEdit = d_combo_tick_divide->lineEdit();
    if (fromLineEdit && divEdit) {
        const bool isDiff =
                (divEdit->text().trimmed() != divEdit->property("_syncedText").toString());
        if (!isDiff) {
            divEdit->setModified(false);
            setLineEditPendingCue(divEdit, false);
            return;
        }
    }

    bool ok = false;
    const double factor = d_combo_tick_divide->currentText().trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(factor) || std::fabs(factor) <= 1e-300)
        return;

    if (fromLineEdit && targets.size() == 1 && targets.first()
        && std::fabs(targets.first()->tickDivideFactor(d_active_axis) - factor) <= 1e-12) {
        if (divEdit) {
            divEdit->setProperty("_syncedText", divEdit->text().trimmed());
            divEdit->setModified(false);
            setLineEditPendingCue(divEdit, false);
        }
        return;
    }

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg)
            continue;
        lg->setTickLabelTransform(d_active_axis, factor, lg->tickPrefix(d_active_axis),
                                  lg->tickSuffix(d_active_axis));
        lg->replot();
        emit lg->modifiedGraph();
    }
    if (divEdit) {
        divEdit->setProperty("_syncedText", divEdit->text().trimmed());
        divEdit->setModified(false);
        setLineEditPendingCue(divEdit, false);
    }
    d_target_ml->pushUndoSnapshot(tr("Change Tick Divide Factor"), before);
}

void GraphInspectorDock::refreshCategoryColumnCombo()
{
    if (!d_combo_category_col)
        return;

    QSignalBlocker blocker(d_combo_category_col);
    d_combo_category_col->clear();
    d_combo_category_col->addItem(tr("(Numeric Ticks)"), QString());

    Graph *g = activeLayer();
    QString currentFmtInfo;
    if (g && d_active_axis >= 0 && d_active_axis < 4) {
        const QList<Graph::AxisType> types = g->axesType();
        if (d_active_axis < types.size() && types[d_active_axis] == Graph::AxisType::Txt)
            currentFmtInfo = g->axisFormatInfo(d_active_axis).trimmed();
    }

    int selectedIdx = 0;
    if (d_app) {
        QList<MyWidget *> windows = d_app->windowsList();
        for (MyWidget *w : windows) {
            if (!isUsableInspectorTable(d_app, w))
                continue;
            Table *tbl = qobject_cast<Table *>(w);
            if (!tbl)
                continue;
            const int nCols = tbl->numCols();
            for (int c = 0; c < nCols; ++c) {
                const QString shortCol = tbl->colLabel(c);
                const QString fullCol = tbl->name() + QLatin1Char('_') + shortCol;
                QString display = fullCol;
                if (Column *colPtr = tbl->column(c)) {
                    if (colPtr->columnMode() == SciDAVis::ColumnMode::Text)
                        display += QStringLiteral(" [Text]");
                }
                const int itemIdx = d_combo_category_col->count();
                d_combo_category_col->addItem(display, fullCol);
                if (!currentFmtInfo.isEmpty() && fullCol == currentFmtInfo)
                    selectedIdx = itemIdx;
            }
        }
    }

    if (selectedIdx == 0 && g && !g->categoryLabels(d_active_axis).isEmpty()) {
        const QStringList cats = g->categoryLabels(d_active_axis);
        const QString customLabel =
                tr("Custom (%1 labels)").arg(cats.size());
        d_combo_category_col->addItem(customLabel, QStringLiteral("__custom__"));
        selectedIdx = d_combo_category_col->count() - 1;
    }

    d_combo_category_col->setCurrentIndex(selectedIdx);
}

void GraphInspectorDock::applyCategoryColumnFromUi()
{
    const QVector<Graph *> targets = targetLayersForScope();
    if (d_syncing_props || !d_target_ml || targets.isEmpty() || !d_combo_category_col || !d_app)
        return;

    const QString fullCol = d_combo_category_col->currentData().toString();
    if (fullCol == QLatin1String("__custom__"))
        return;

    Table *srcTable = nullptr;
    Column *srcCol = nullptr;
    QString shortColName;
    QStringList labels;

    if (!fullCol.isEmpty()) {
        srcTable = d_app->table(fullCol);
        if (!srcTable) {
            const int us = fullCol.lastIndexOf(QLatin1Char('_'));
            if (us > 0)
                srcTable = d_app->table(fullCol.left(us));
        }
        if (!srcTable)
            return;
        const QString prefix = srcTable->name() + QLatin1Char('_');
        shortColName = fullCol.startsWith(prefix) ? fullCol.mid(prefix.size()) : fullCol;
        const int colIdx = srcTable->colIndex(fullCol);
        srcCol = (colIdx >= 0) ? srcTable->column(colIdx) : srcTable->column(shortColName);
        if (!srcCol)
            return;

        int lastNonEmpty = -1;
        const int nRows = srcCol->rowCount();
        for (int r = 0; r < nRows; ++r) {
            const QString txt = srcTable->text(r, srcTable->colIndex(srcCol->name())).trimmed();
            if (!txt.isEmpty())
                lastNonEmpty = r;
        }
        for (int r = 0; r <= lastNonEmpty; ++r) {
            QString txt;
            if (srcCol->columnMode() == SciDAVis::ColumnMode::Text)
                txt = srcCol->textAt(r).trimmed();
            else
                txt = srcTable->text(r, srcTable->colIndex(srcCol->name())).trimmed();
            labels.append(txt);
        }
    }

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg)
            continue;
        if (fullCol.isEmpty()) {
            lg->setCategoryLabels(d_active_axis, QStringList());
            const QString formula = lg->getAxesFormulas().value(d_active_axis);
            lg->setLabelsNumericFormat(d_active_axis, Plot::Automatic, 6, formula);
        } else if (srcTable && srcCol) {
            lg->setLabelsTextFormat(d_active_axis, srcCol, 0, qMax(0, labels.size() - 1));
            if (!labels.isEmpty())
                lg->setCategoryLabels(d_active_axis, labels);
        }
        lg->replot();
        emit lg->modifiedGraph();
    }
    d_target_ml->pushUndoSnapshot(tr("Change Category Labels"), before);
}

void GraphInspectorDock::updateRichPreviewLabel(QLabel *lbl, const QString &raw, bool showWhenPlain)
{
    if (!lbl)
        return;
    if (raw.trimmed().isEmpty()) {
        if (showWhenPlain) {
            lbl->setText(tr("Preview: (empty)"));
            lbl->show();
        } else {
            lbl->clear();
            lbl->hide();
        }
        QTimer::singleShot(0, this, &GraphInspectorDock::updatePropScrollCue);
        return;
    }
    if (!showWhenPlain && !hasRichTextMarkup(raw)) {
        lbl->clear();
        lbl->hide();
        QTimer::singleShot(0, this, &GraphInspectorDock::updatePropScrollCue);
        return;
    }
    const QString rendered = renderSafeScientificPreviewHtml(raw);
    lbl->setText(showWhenPlain ? tr("Preview: %1").arg(rendered) : rendered);
    lbl->show();
    QTimer::singleShot(0, this, &GraphInspectorDock::updatePropScrollCue);
}

void GraphInspectorDock::updateAxisTitlePreview(const QString &raw)
{
    updateRichPreviewLabel(d_lbl_axis_title_preview, raw, true);
}

void GraphInspectorDock::flushPendingAxisTitleEdit()
{
    if (d_axis_title_timer)
        d_axis_title_timer->stop();
    Graph *g = d_axis_title_edit_layer.data();
    const int ax = d_axis_title_edit_axis;
    d_axis_title_edit_layer = nullptr;
    d_axis_title_edit_axis = -1;
    if (!g || ax < 0 || ax >= 4 || !d_edit_axis_title)
        return;

    MultiLayer *ml = g->multiLayerParent();
    QVector<Graph *> targets;
    if (ml && ml == d_target_ml.data() && d_scope_all_panels)
        targets = targetLayersForScope();
    if (targets.isEmpty())
        targets.append(g);

    const QString newTitle =
            CanvasInlineTextEditor::normalizeOriginRichText(d_edit_axis_title->text());
    bool anyDiff = false;
    for (Graph *lg : targets) {
        if (lg && lg->axisTitle(ax) != newTitle) {
            anyDiff = true;
            break;
        }
    }
    d_edit_axis_title->setProperty("_syncedText", newTitle);
    d_edit_axis_title->setModified(false);
    if (!anyDiff)
        return;

    const QString before = ml ? ml->captureUndoSnapshot() : QString();
    for (Graph *lg : targets) {
        if (!lg)
            continue;
        lg->setAxisTitle(ax, newTitle);
        lg->replot();
        emit lg->modifiedGraph();
    }
    if (ml && !before.isEmpty())
        ml->pushUndoSnapshot(tr("Edit Axis Title"), before, kMergeAxisTitle);
}

void GraphInspectorDock::resolveRichTarget(RichTargetMode mode, QLineEdit *&outLine,
                                           QPlainTextEdit *&outPlain) const
{
    outLine = nullptr;
    outPlain = nullptr;
    if (mode == RichTargetMode::AxisTitle) {
        outLine = d_edit_axis_title;
        return;
    }
    if (mode == RichTargetMode::PlotTitle) {
        outLine = d_edit_plot_title;
        return;
    }
    if (mode == RichTargetMode::SpectrogramCbTitle) {
        outLine = d_edit_spec_cb_title;
        return;
    }
    if (d_edit_annot_text && !d_edit_annot_text->isHidden() && d_edit_annot_text->isEnabled()) {
        outPlain = d_edit_annot_text;
        return;
    }
    QWidget *scopeRoot = d_leg_text_wrap ? d_leg_text_wrap
                                         : const_cast<GraphInspectorDock *>(this);
    if (d_last_legend_focus_edit && d_last_legend_focus_edit->isEnabled()
        && d_last_legend_focus_edit->isVisibleTo(scopeRoot)) {
        outLine = d_last_legend_focus_edit.data();
        return;
    }
    outLine = d_edit_leg_header;
}

void GraphInspectorDock::applyRichMarkupToTarget(RichTargetMode mode, const QString &openTag,
                                                 const QString &closeTag)
{
    QLineEdit *le = nullptr;
    QPlainTextEdit *pe = nullptr;
    resolveRichTarget(mode, le, pe);
    if (pe && pe->isEnabled()) {
        QTextCursor c = pe->textCursor();
        // QTextCursor uses U+2029 between paragraphs inside a selection.
        const QString sel = c.selectedText().replace(QChar(0x2029), QLatin1Char('\n'));
        c.insertText(openTag + sel + closeTag);
        if (sel.isEmpty())
            c.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, closeTag.length());
        pe->setTextCursor(c);
        pe->setFocus();
        return;
    }
    if (!le || !le->isEnabled())
        return;
    QString txt = le->text();
    if (le->hasSelectedText()) {
        const int start = le->selectionStart();
        const QString sel = le->selectedText();
        txt.replace(start, sel.length(), openTag + sel + closeTag);
        le->setText(txt);
        le->setSelection(start + openTag.length(), sel.length());
    } else {
        const int pos = le->cursorPosition();
        txt.insert(pos, openTag + closeTag);
        le->setText(txt);
        le->setCursorPosition(pos + openTag.length());
    }
    le->setModified(true);
    if (le == d_edit_axis_title)
        updateAxisTitlePreview(le->text());
    le->setFocus();
    emit le->editingFinished();
}

void GraphInspectorDock::insertRichTokenToTarget(RichTargetMode mode, const QString &token,
                                                 bool isJournalTemplate, bool isUnitOnly)
{
    if (token.isEmpty())
        return;
    QLineEdit *le = nullptr;
    QPlainTextEdit *pe = nullptr;
    resolveRichTarget(mode, le, pe);
    if (pe && pe->isEnabled()) {
        QTextCursor c = pe->textCursor();
        if (isJournalTemplate && isUnitOnly && !pe->toPlainText().trimmed().isEmpty())
            c.insertText(QStringLiteral(" (") + token + QStringLiteral(")"));
        else
            c.insertText(token);
        pe->setTextCursor(c);
        pe->setFocus();
        return;
    }
    if (!le || !le->isEnabled())
        return;
    if (isJournalTemplate) {
        if (le->hasSelectedText()) {
            const int start = le->selectionStart();
            const int len = le->selectedText().length();
            QString txt = le->text();
            txt.replace(start, len, token);
            le->setText(txt);
            le->setCursorPosition(start + token.length());
        } else if (isUnitOnly && !le->text().trimmed().isEmpty()) {
            const int pos = le->cursorPosition();
            const QString wrapped = QStringLiteral(" (") + token + QStringLiteral(")");
            QString txt = le->text();
            txt.insert(pos, wrapped);
            le->setText(txt);
            le->setCursorPosition(pos + wrapped.length());
        } else {
            le->setText(token);
            le->setCursorPosition(token.length());
        }
    } else if (le->hasSelectedText()) {
        const int start = le->selectionStart();
        const int len = le->selectedText().length();
        QString txt = le->text();
        txt.replace(start, len, token);
        le->setText(txt);
        le->setCursorPosition(start + token.length());
    } else {
        const int pos = le->cursorPosition();
        QString txt = le->text();
        txt.insert(pos, token);
        le->setText(txt);
        le->setCursorPosition(pos + token.length());
    }
    le->setModified(true);
    if (le == d_edit_axis_title)
        updateAxisTitlePreview(le->text());
    le->setFocus();
    emit le->editingFinished();
}

QWidget *GraphInspectorDock::buildRichTextBar(QWidget *parent, RichTargetMode mode)
{
    QWidget *wrap = new QWidget(parent);
    QVBoxLayout *vlay = new QVBoxLayout(wrap);
    vlay->setContentsMargins(0, 1, 0, 1);
    vlay->setSpacing(2);

    QHBoxLayout *fmtRow = new QHBoxLayout();
    fmtRow->setContentsMargins(0, 0, 0, 0);
    fmtRow->setSpacing(2);

    auto makeFmtBtn = [&](const QString &label, bool compact) -> QToolButton * {
        QToolButton *b = new QToolButton(wrap);
        b->setObjectName(compact ? QStringLiteral("InspectorSymbolChipBtn")
                                 : QStringLiteral("InspectorMenuChipBtn"));
        b->setFocusPolicy(Qt::NoFocus);
        b->setText(label);
        b->setFixedHeight(24);
        if (compact) {
            b->setFixedWidth(25);
            b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            fmtRow->addWidget(b, 0);
        } else {
            b->setMinimumWidth(56);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            fmtRow->addWidget(b, 1);
        }
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };

    RichTextBarEntry entry;
    entry.targetMode = mode;

    entry.btnItalic = makeFmtBtn(QStringLiteral("I"), true);
    entry.btnItalic->setProperty("_tg_tip_fmt", QStringLiteral("Italic (slanted)"));
    QFont itFont = entry.btnItalic->font();
    itFont.setItalic(true);
    itFont.setBold(true);
    entry.btnItalic->setFont(itFont);
    connect(entry.btnItalic, &QToolButton::clicked, this, [this, mode]() {
        applyRichMarkupToTarget(mode, QStringLiteral("<i>"), QStringLiteral("</i>"));
    });

    entry.btnSub = makeFmtBtn(QString::fromUtf8("X\xE2\x82\x82"), true);
    entry.btnSub->setProperty("_tg_tip_fmt", QString::fromUtf8("Subscript (X\xE2\x82\x82)"));
    connect(entry.btnSub, &QToolButton::clicked, this, [this, mode]() {
        applyRichMarkupToTarget(mode, QStringLiteral("<sub>"), QStringLiteral("</sub>"));
    });

    entry.btnSup = makeFmtBtn(QString::fromUtf8("X\xC2\xB2"), true);
    entry.btnSup->setProperty("_tg_tip_fmt", QString::fromUtf8("Superscript (X\xC2\xB2)"));
    connect(entry.btnSup, &QToolButton::clicked, this, [this, mode]() {
        applyRichMarkupToTarget(mode, QStringLiteral("<sup>"), QStringLiteral("</sup>"));
    });

    entry.btnGreek = makeFmtBtn(tr("Greek"), false);
    entry.btnGreek->setProperty("_tg_tip_fmt", QStringLiteral("Insert Greek Symbol"));
    entry.btnGreek->setPopupMode(QToolButton::InstantPopup);
    entry.menuGreek = new QMenu(d_app ? static_cast<QWidget *>(d_app.data()) : nullptr);
    rebuildGreekSymbolMenu(entry.menuGreek, mode);
    entry.btnGreek->setMenu(entry.menuGreek);

    entry.btnUnit = makeFmtBtn(tr("Units"), false);
    entry.btnUnit->setProperty("_tg_tip_fmt",
                              QStringLiteral("Insert Symbol, Unit, or Journal Axis Template"));
    entry.btnUnit->setPopupMode(QToolButton::InstantPopup);
    entry.menuUnits = new QMenu(d_app ? static_cast<QWidget *>(d_app.data()) : nullptr);
    rebuildUnitsSymbolMenu(entry.menuUnits, mode);
    entry.btnUnit->setMenu(entry.menuUnits);
    vlay->addLayout(fmtRow);

    QHBoxLayout *matSymRow = new QHBoxLayout();
    matSymRow->setContentsMargins(0, 0, 0, 0);
    matSymRow->setSpacing(3);
    const QList<QString> quickSyms = {
        QString::fromUtf8("\xCF\x83"),     // σ
        QString::fromUtf8("\xCE\xB5"),     // ε
        QString::fromUtf8("2\xCE\xB8"),    // 2θ
        QString::fromUtf8("\xCE\xBC" "m"), // μm
        QString::fromUtf8("\xC2\xB0" "C"), // °C
        QString::fromUtf8("\xC3\x85"),     // Å
    };
    for (const QString &sym : quickSyms) {
        QToolButton *b = new QToolButton(wrap);
        b->setObjectName(QStringLiteral("InspectorSymbolChipBtn"));
        b->setFocusPolicy(Qt::NoFocus);
        b->setText(sym);
        b->setToolTip(QCoreApplication::translate("QObject", "Insert %1").arg(sym));
        b->setProperty("_tg_tip_fmt", QStringLiteral("Insert %1"));
        b->setProperty("_tg_tip_arg", sym);
        b->setFixedHeight(22);
        b->setMinimumWidth(24);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(b, &QToolButton::clicked, this,
                [this, mode, sym]() { insertRichTokenToTarget(mode, sym); });
        matSymRow->addWidget(b);
    }
    vlay->addLayout(matSymRow);

    d_rich_bars.append(entry);
    return wrap;
}

void GraphInspectorDock::rebuildRichTextBarMenus()
{
    for (const RichTextBarEntry &entry : d_rich_bars) {
        if (entry.btnGreek)
            entry.btnGreek->setText(tr("Greek"));
        if (entry.btnUnit)
            entry.btnUnit->setText(tr("Units"));
        rebuildGreekSymbolMenu(entry.menuGreek, entry.targetMode);
        rebuildUnitsSymbolMenu(entry.menuUnits, entry.targetMode);
    }
}

void GraphInspectorDock::rebuildGreekSymbolMenu(QMenu *menu, RichTargetMode mode)
{
    if (!menu)
        return;
    menu->clear();
    const QList<QPair<QString, QString>> greekList = {
        { tr("\xCE\xB1 (alpha)"), QString::fromUtf8("\xCE\xB1") },
        { tr("\xCE\xB2 (beta)"), QString::fromUtf8("\xCE\xB2") },
        { tr("\xCE\xB3 (gamma)"), QString::fromUtf8("\xCE\xB3") },
        { tr("\xCE\xB4 (delta)"), QString::fromUtf8("\xCE\xB4") },
        { tr("\xCE\x94 (Delta)"), QString::fromUtf8("\xCE\x94") },
        { tr("\xCE\xB5 (epsilon)"), QString::fromUtf8("\xCE\xB5") },
        { tr("\xCE\xB8 (theta)"), QString::fromUtf8("\xCE\xB8") },
        { tr("\xCE\xBB (lambda)"), QString::fromUtf8("\xCE\xBB") },
        { tr("\xCE\xBC (mu)"), QString::fromUtf8("\xCE\xBC") },
        { tr("\xCF\x81 (rho)"), QString::fromUtf8("\xCF\x81") },
        { tr("\xCF\x83 (sigma)"), QString::fromUtf8("\xCF\x83") },
        { tr("\xCF\x84 (tau)"), QString::fromUtf8("\xCF\x84") },
        { tr("\xCF\x86 (phi)"), QString::fromUtf8("\xCF\x86") },
        { tr("\xCE\xA9 (Omega)"), QString::fromUtf8("\xCE\xA9") }
    };
    for (const auto &pr : greekList) {
        const QString sym = pr.second;
        menu->addAction(pr.first, this, [this, mode, sym]() { insertRichTokenToTarget(mode, sym); });
    }
}

void GraphInspectorDock::rebuildUnitsSymbolMenu(QMenu *menu, RichTargetMode mode)
{
    if (!menu)
        return;
    for (QMenu *sub : menu->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly)) {
        sub->close();
        sub->deleteLater();
    }
    menu->clear();

    // 1) Materials-science symbols
    QMenu *symSub = menu->addMenu(tr("Symbols"));
    const QList<QPair<QString, QString>> matSymbols = {
        { tr("\xCF\x83 (sigma)"), QString::fromUtf8("\xCF\x83") },
        { tr("\xCE\xB5 (epsilon)"), QString::fromUtf8("\xCE\xB5") },
        { tr("\xCE\x94 (Delta)"), QString::fromUtf8("\xCE\x94") },
        { tr("\xC2\xB1 (plus/minus)"), QString::fromUtf8("\xC2\xB1") },
        { tr("\xC3\x97 (times)"), QString::fromUtf8("\xC3\x97") },
        { tr("\xC2\xB7 (cdot)"), QString::fromUtf8("\xC2\xB7") },
        { tr("\xC2\xB0 (degree)"), QString::fromUtf8("\xC2\xB0") },
        { tr("\xC3\x85 (Angstrom)"), QString::fromUtf8("\xC3\x85") },
        { tr("\xCE\xBC (mu)"), QString::fromUtf8("\xCE\xBC") },
        { tr("\xCE\xB8 (theta)"), QString::fromUtf8("\xCE\xB8") }
    };
    for (const auto &pr : matSymbols) {
        const QString tok = pr.second;
        symSub->addAction(pr.first, this, [this, mode, tok]() { insertRichTokenToTarget(mode, tok); });
    }

    // 2) 1-click Journal Axis Templates
    menu->addSeparator();
    const QList<QPair<QString, QString>> journalTemplates = {
        { QString::fromUtf8("\xCF\x83 (MPa)"), QString::fromUtf8("\xCF\x83 (MPa)") },
        { QString::fromUtf8("\xCF\x83\xE2\x82\x80.\xE2\x82\x82 (MPa)"),
          QString::fromUtf8("\xCF\x83<sub>0.2</sub> (MPa)") },
        { QString::fromUtf8("\xCE\xB5 (%)"), QString::fromUtf8("\xCE\xB5 (%)") },
        { QString::fromUtf8("d\xCF\x83/d\xCE\xB5 (GPa)"),
          QString::fromUtf8("d\xCF\x83/d\xCE\xB5 (GPa)") },
        { QString::fromUtf8("2\xCE\xB8 (deg.)"), QString::fromUtf8("2\xCE\xB8 (deg.)") },
        { QString::fromUtf8("d\xE2\x81\xBB\xC2\xB9/\xC2\xB2 (\xCE\xBC" "m\xE2\x81\xBB\xC2\xB9/\xC2\xB2)"),
          QString::fromUtf8("<i>d</i><sup>-1/2</sup> (\xCE\xBC" "m<sup>-1/2</sup>)") },
        { QString::fromUtf8("10\xE2\x81\xBB\xE2\x81\xB4 s\xE2\x81\xBB\xC2\xB9"),
          QStringLiteral("10<sup>-4</sup> s<sup>-1</sup>") },
        { QString::fromUtf8("mass ppm\xC2\xB7s\xE2\x81\xBB\xC2\xB9"),
          QString::fromUtf8("mass ppm\xC2\xB7s<sup>-1</sup>") },
        { QString::fromUtf8("1000/T (K\xE2\x81\xBB\xC2\xB9)"),
          QStringLiteral("1000/<i>T</i> (K<sup>-1</sup>)") }
    };
    for (int i = 0; i < journalTemplates.size(); ++i) {
        const QString disp = journalTemplates[i].first;
        const QString tmpl = journalTemplates[i].second;
        const bool isUnitOnly = (i == 6 || i == 7);
        menu->addAction(disp, this, [this, mode, tmpl, isUnitOnly]() {
            insertRichTokenToTarget(mode, tmpl, true, isUnitOnly);
        });
    }

    // 3) Common Units submenu
    menu->addSeparator();
    QMenu *unitSub = menu->addMenu(tr("Common Units"));
    const QList<QPair<QString, QString>> unitList = {
        { QStringLiteral("MPa"), QStringLiteral(" (MPa)") },
        { QStringLiteral("GPa"), QStringLiteral(" (GPa)") },
        { QStringLiteral("%"), QStringLiteral(" (%)") },
        { QStringLiteral("K"), QStringLiteral(" (K)") },
        { QString::fromUtf8("\xC2\xB0" "C"), QString::fromUtf8(" (\xC2\xB0" "C)") },
        { QString::fromUtf8("s\xE2\x81\xBB\xC2\xB9"), QStringLiteral(" (s<sup>-1</sup>)") },
        { QStringLiteral("mm"), QStringLiteral(" (mm)") },
        { QString::fromUtf8("\xCE\xBC" "m"), QString::fromUtf8(" (\xCE\xBC" "m)") },
        { QStringLiteral("nm"), QStringLiteral(" (nm)") },
        { QString::fromUtf8("\xC3\x85"), QString::fromUtf8(" (\xC3\x85)") },
        { QString::fromUtf8("cm\xE2\x81\xBB\xC2\xB9"), QStringLiteral(" (cm<sup>-1</sup>)") },
        { QStringLiteral("wt.ppm"), QStringLiteral(" (wt.ppm)") },
        { QStringLiteral("mass%"), QStringLiteral(" (mass%)") },
        { QStringLiteral("at.%"), QStringLiteral(" (at.%)") },
        { QStringLiteral("kJ/mol"), QStringLiteral(" (kJ/mol)") },
        { QStringLiteral("eV"), QStringLiteral(" (eV)") }
    };
    for (const auto &pr : unitList) {
        const QString tok = pr.second;
        unitSub->addAction(pr.first, this,
                           [this, mode, tok]() { insertRichTokenToTarget(mode, tok); });
    }
}

void GraphInspectorDock::matchRightYAxisColorToCurve()
{
    Graph *g = activeLayer();
    if (!d_target_ml || !g)
        return;
    const int ci = effectiveSelectedCurveIndex(g);
    QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
    if (!cv)
        return;

    const bool hasLine = (cv->style() != QwtPlotCurve::NoCurve && cv->pen().style() != Qt::NoPen);
    QColor col = (hasLine && cv->pen().color().isValid()) ? cv->pen().color()
                                                          : cv->symbol().pen().color();
    if (!col.isValid())
        return;
    col.setAlpha(255);

    const QString before = d_target_ml->captureUndoSnapshot();
    auto colorizeRightY = [&](Graph *targetG) {
        if (!targetG || !targetG->plotWidget())
            return;
        targetG->enableAxis(QwtPlot::yRight, true);
        targetG->setAxisBackboneVisible(QwtPlot::yRight, true);
        targetG->setRightAxisTitleColor(col);

        QStringList axCols = targetG->axesColors();
        while (axCols.size() < 4)
            axCols.append(QStringLiteral("#000000"));
        axCols[QwtPlot::yRight] = col.name();
        targetG->setAxesColors(axCols);

        QStringList numCols = targetG->axesNumColors();
        while (numCols.size() < 4)
            numCols.append(QStringLiteral("#000000"));
        numCols[QwtPlot::yRight] = col.name();
        targetG->setAxesNumColors(numCols);

        if (QwtScaleWidget *sw = targetG->plotWidget()->axisWidget(QwtPlot::yRight)) {
            QPalette pal = sw->palette();
            pal.setColor(QPalette::WindowText, col);
            pal.setColor(QPalette::Text, col);
            sw->setPalette(pal);
        }
        targetG->replot();
        emit targetG->modifiedGraph();
    };

    colorizeRightY(g);
    for (Graph *cand : d_target_ml->layersList()) {
        if (cand && cand->linkedParentLayer() == g && cand->plotWidget()
            && cand->plotWidget()->axisEnabled(QwtPlot::yRight)) {
            colorizeRightY(cand);
        }
    }

    d_target_ml->pushUndoSnapshot(tr("Match Right-Y Axis Color"), before);
}

void GraphInspectorDock::refreshPaletteSchemeCombo()
{
    if (!d_combo_series_scheme)
        return;
    QSignalBlocker b(d_combo_series_scheme);
    const QString prevName = d_combo_series_scheme->currentData().toString();
    d_combo_series_scheme->clear();
    const QStringList names = Graph::allPaletteNames();
    for (const QString &nm : names)
        d_combo_series_scheme->addItem(tr(nm.toUtf8().constData()), nm);

    // Until the user picks a scheme here, show the palette new curves use (so "apply" keeps
    // their colours, and a changed default is followed); a user's pick is kept.
    int targetIdx = (d_series_scheme_user_chosen && !prevName.isEmpty())
            ? d_combo_series_scheme->findData(prevName)
            : d_combo_series_scheme->findData(Graph::defaultCurvePaletteName());
    if (targetIdx < 0)
        targetIdx = 0;
    if (d_combo_series_scheme->count() > 0)
        d_combo_series_scheme->setCurrentIndex(targetIdx);
}

void GraphInspectorDock::syncSeriesSchemeToDefaultPalette()
{
    refreshPaletteSchemeCombo();
}

QString GraphInspectorDock::seriesSchemePaletteName() const
{
    return d_combo_series_scheme ? d_combo_series_scheme->currentData().toString() : QString();
}

void GraphInspectorDock::applyGroupAutoStyleFromUi()
{
    const QVector<Graph *> targets = targetLayersForScope();
    if (!d_target_ml || targets.isEmpty() || !d_combo_series_scheme)
        return;

    const QString palName = d_combo_series_scheme->currentData().toString().isEmpty()
            ? d_combo_series_scheme->currentText()
            : d_combo_series_scheme->currentData().toString();
    QList<QColor> anchors = Graph::paletteColors(palName);
    if (anchors.isEmpty())
        anchors = { QColor(Qt::black) };

    const bool cycleShapes = d_chk_cycle_sym_shapes && d_chk_cycle_sym_shapes->isChecked();
    const bool pairedHollow = d_chk_paired_hollow && d_chk_paired_hollow->isChecked();
    const QString canonPal = Graph::canonicalPaletteName(palName);
    const bool isSequential =
            (canonPal.compare(QStringLiteral("Viridis"), Qt::CaseInsensitive) == 0
             || canonPal.compare(QStringLiteral("Plasma"), Qt::CaseInsensitive) == 0
             || canonPal.compare(QStringLiteral("Inferno"), Qt::CaseInsensitive) == 0
             || canonPal.compare(QStringLiteral("CoolWarm"), Qt::CaseInsensitive) == 0);

    static const QVector<QwtSymbol::Style> kShapeCycle = {
        QwtSymbol::Ellipse,   QwtSymbol::Rect,    QwtSymbol::UTriangle, QwtSymbol::DTriangle,
        QwtSymbol::Diamond,   QwtSymbol::Hexagon, QwtSymbol::Star1
    };

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg)
            continue;
        const QList<QwtPlotCurve *> fitCurves = lg->fitCurvesList();
        QVector<int> eligibleIndices;
        for (int ci = 0; ci < lg->curves(); ++ci) {
            if (lg->curveType(ci) == Graph::ErrorBars || lg->curveType(ci) == Graph::Function)
                continue;
            QwtPlotCurve *cv = lg->curve(ci);
            if (!cv || fitCurves.contains(cv)
                || dynamic_cast<FunctionCurve *>(lg->plotItem(ci)) != nullptr)
                continue;
            eligibleIndices.append(ci);
        }
        if (eligibleIndices.isEmpty())
            continue;

        const QList<QColor> effColors = (isSequential && eligibleIndices.size() > 1)
                ? Graph::interpolatePalette(anchors, eligibleIndices.size())
                : anchors;

        for (int k = 0; k < eligibleIndices.size(); ++k) {
            const int ci = eligibleIndices[k];
            QwtPlotCurve *cv = lg->curve(ci);
            if (!cv)
                continue;
            const QColor col = effColors[k % effColors.size()];
            const bool makeHollow = pairedHollow ? ((k % 2) == 1)
                                                 : Graph::isSymbolWhiteHollow(cv->symbol());

            QPen lp = cv->pen();
            lp.setColor(col);
            cv->setPen(lp);

            QwtSymbol sym = cv->symbol();
            if (cycleShapes && sym.style() != QwtSymbol::NoSymbol) {
                const int shapeIdx = (pairedHollow ? (k / 2) : k) % kShapeCycle.size();
                sym.setStyle(kShapeCycle[shapeIdx]);
            }
            if (sym.style() != QwtSymbol::NoSymbol) {
                QPen sp = sym.pen();
                sp.setColor(col);
                if (sp.width() <= 0)
                    sp.setWidth(qMax(1, lp.width()));
                sym.setPen(sp);
                sym.setBrush(makeHollow ? QBrush(QColor(Qt::white)) : QBrush(col));
                cv->setSymbol(sym);
            }

            if (cv->brush().style() != Qt::NoBrush) {
                QColor fc = col;
                fc.setAlpha(cv->brush().color().alpha() > 0 ? cv->brush().color().alpha() : 65);
                cv->setBrush(QBrush(fc));
            }

            if (auto *dc = dynamic_cast<DataCurve *>(cv)) {
                for (DataCurve *eb : dc->errorBarsList()) {
                    if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(eb))
                        er->setColor(col);
                }
            }
        }
        lg->reconstructLegend();
        lg->updatePlot();
    }

    d_target_ml->pushUndoSnapshot(tr("Apply Series Style Scheme"), before);
    refreshObjectTree();
    syncCurveControls();
}

void GraphInspectorDock::applyAdjacentErrorBars()
{
    Graph *g = activeLayer();
    if (!d_app || !d_target_ml || !g)
        return;
    const int ci = effectiveSelectedCurveIndex(g);
    DataCurve *dc = (ci >= 0 && ci < g->curves()) ? dynamic_cast<DataCurve *>(g->curve(ci))
                                                 : nullptr;
    if (!dc || !dc->table() || !dc->table()->d_future_table) {
        if (d_app->statusBar()) {
            d_app->statusBar()->showMessage(
                    tr("This feature is not available for user defined function curves!"), 4000);
        }
        syncCurveControls();
        return;
    }

    Table *tbl = dc->table();
    const QString curveName = dc->title().text();
    MultiLayer *ml = g->multiLayerParent();
    if (!ml)
        ml = d_target_ml.data();

    auto removeVerticalErrorBarsOnCurve = [&](DataCurve *master, const QString &onlyMatchingCol) -> int {
        int removed = 0;
        const QList<DataCurve *> errs = master->errorBarsList();
        for (DataCurve *eb : errs) {
            auto *er = dynamic_cast<QwtErrorPlotCurve *>(eb);
            if (!er || er->direction() != QwtErrorPlotCurve::Vertical)
                continue;
            if (!onlyMatchingCol.isEmpty() && er->title().text() != onlyMatchingCol)
                continue;
            const int eIdx = g->curveIndex(er);
            if (eIdx >= 0) {
                if (d_app && ml)
                    d_app->invalidateModelessGraphDialogs(ml);
                g->removeCurve(eIdx);
                ++removed;
            }
        }
        return removed;
    };

    const int colIdx = tbl->colIndex(dc->yColumnName());
    const int errColIdx = findSmartAdjacentYErrColumn(tbl, colIdx);
    if (errColIdx < 0) {
        if (d_app->statusBar()) {
            d_app->statusBar()->showMessage(
                    tr("No adjacent column exists to the right of the selected curve column."),
                    4000);
        }
        syncCurveControls();
        return;
    }

    const QString errFull =
            QString(tbl->name()) + QStringLiteral("_") + tbl->colLabel(errColIdx);
    const QString before = ml ? ml->captureUndoSnapshot() : QString();
    bool changed = false;

    if (removeVerticalErrorBarsOnCurve(dc, errFull) > 0) {
        changed = true;
    } else {
        removeVerticalErrorBarsOnCurve(dc, QString());
        if (g->addErrorBars(dc->xColumnName(), curveName, tbl, errFull,
                            QwtErrorPlotCurve::Vertical)) {
            changed = true;
        }
    }
    if (changed) {
        g->updatePlot();
        if (ml)
            ml->pushUndoSnapshot(tr("Toggle Error Bars"), before);
        refreshAll();
    } else {
        syncCurveControls();
    }
}

void GraphInspectorDock::openErrorBarsDialog()
{
    QPointer<MultiLayer> safeMl(d_target_ml);
    QPointer<Graph> safeG(activeLayer());
    if (!d_app || !safeMl || !safeG)
        return;
    if (!ensureTargetMultiLayerActive(safeMl) || !safeG)
        return;
    safeMl->setActiveGraph(safeG.data());
    d_app->addErrorBars();
}
