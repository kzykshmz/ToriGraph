#include "OleServer.h"
#include "MultiLayer.h"
/***************************************************************************
    File                 : Graph_Curves.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Graph curves, plot types (pie, histogram, vector, box, function, spectrogram, ternary, polar), and stack offsets

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

#include <QVarLengthArray>

#include "Graph.h"
#include "ExtraPlotTypes.h"
#include "Grid.h"
#include "CanvasPicker.h"
#include "CanvasInlineTextEditor.h"
#include "QwtErrorPlotCurve.h"
#include "Legend.h"
#include "ArrowMarker.h"
#include "ScalePicker.h"
#include "TitlePicker.h"
#include "QwtPieCurve.h"
#include "ImageMarker.h"
#include "QwtBarCurve.h"
#include "BoxCurve.h"
#include "QwtHistogram.h"
#include "VectorCurve.h"
#include "ScaleDraw.h"
#include "ColorButton.h"
#include "PatternBox.h"
#include "SymbolBox.h"
#include "FunctionCurve.h"
#include "Spectrogram.h"
#include "SelectionMoveResizer.h"
#include "DataPickerTool.h"
#include "RangeSelectorTool.h"
#include "PlotCurve.h"
#include "ApplicationWindow.h"
#include "I18nJapanese.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "core/Project.h"
#include "core/column/Column.h"
#include "MultiLayer.h"
#include "EmfPaintDevice.h"
#include "MyParser.h"
#include <algorithm>
#include <limits>
#include <mutex>

#include <QApplication>
#include <QFontMetricsF>
#include <QBitmap>
#include <QClipboard>
#include <QCursor>
#include <QFile>
#include <QImage>
#include <QMessageBox>
#include <QPainterPath>
#include <QPixmap>
#include <QPainter>
#include <QMenu>
#include <QTextStream>
#include <QLocale>
#include <QPrintDialog>
#include <QImageWriter>
#include <QFileInfo>
#include <QRegExp>
#include <QRegularExpression>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QMainWindow>
#include <QStatusBar>
#include <QTextDocument>
#include <QUndoCommand>
#include <QUndoStack>
#include <utility>

#if QT_VERSION >= 0x040300
#include <QSvgGenerator>
#endif

#include <qwt_painter.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_zoomer.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_text.h>
#include <qwt_text_label.h>
#include <qwt_color_map.h>

#include <cmath>
#include <memory>
#include <stdexcept>

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
using namespace std;

#include "Graph_Internal.h"
using namespace GraphInternal;

QStringList Graph::analysableCurvesList()
{
    QStringList cList;
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotCurve *c = d_plot->curve(c_keys[i]);
        if (c && c_type[i] != ErrorBars)
            cList << c->title().text();
    }
    return cList;
}

QStringList Graph::curvesList()
{
    QStringList cList;
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotCurve *c = d_plot->curve(c_keys[i]);
        if (c)
            cList << c->title().text();
    }
    return cList;
}

QStringList Graph::plotItemsList() const
{
    QStringList cList;
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (it)
            cList << it->title().text();
    }
    return cList;
}

int Graph::selectedCurveID()
{
    if (d_range_selector && d_range_selector->selectedCurve())
        return curveKey(curveIndex(d_range_selector->selectedCurve()));
    if ((d_selection.kind == GraphSelection::Curve
         || d_selection.kind == GraphSelection::ErrorBars)
        && d_selection.primaryId >= 0 && d_selection.primaryId < n_curves) {
        return curveKey(d_selection.primaryId);
    }
    return -1;
}

QString Graph::selectedCurveTitle()
{
    if (d_range_selector && d_range_selector->selectedCurve())
        return d_range_selector->selectedCurve()->title().text();
    if ((d_selection.kind == GraphSelection::Curve
         || d_selection.kind == GraphSelection::ErrorBars)
        && d_selection.primaryId >= 0 && d_selection.primaryId < n_curves) {
        if (const QwtPlotCurve *c = curve(d_selection.primaryId))
            return c->title().text();
    }
    return {};
}

void Graph::updateCurvesData(Table *w, const QString &colName)
{
    int updated_curves = 0;
    // Updating a curve may remove it (and its error bars) from the graph, which shifts the
    // positions in c_keys; walk a snapshot of the (never reused) curve keys instead, so no
    // curve is skipped and removed ones are simply not found any more.
    const QVector<int> keys = c_keys.mid(0, qMin(n_curves, c_keys.size()));
    QList<int> to_remove; // keys of curves which changed too much to be kept
    for (int key : keys) {
        QwtPlotItem *it = d_plot->plotItem(key);
        if (!it)
            continue;
        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(it);
        if (!pc || pc->type() == Function)
            continue;
        DataCurve *c = dynamic_cast<DataCurve *>(pc);
        if (!c)
            continue;
        if (c->table() == w && (c->xColumnName() == colName || c->yColumnName() == colName)) {
            Column *changedCol = w ? w->column(colName) : nullptr;
            if (!changedCol) {
                if (c->updateData(w, colName))
                    updated_curves++;
                continue;
            }
            auto colType = changedCol->columnMode();
            AxisType atype;
            if (c->xColumnName() == colName) {
                if (c->type() == HorizontalBars)
                    atype = (AxisType)axisType[QwtPlot::yLeft];
                else
                    atype = (AxisType)axisType[QwtPlot::xBottom];
            } else {
                if (c->type() == HorizontalBars)
                    atype = (AxisType)axisType[QwtPlot::xBottom];
                else
                    atype = (AxisType)axisType[QwtPlot::yLeft];
            }
            // compare setLabels*Format() calls in insertCurve()
            if ((colType == SciDAVis::ColumnMode::Text && atype != AxisType::Txt)
                || (colType == SciDAVis::ColumnMode::DateTime
                    && ((atype != AxisType::Time) && (atype != AxisType::Date)
                        && (atype != AxisType::DateTime))))
                to_remove << key;
            else if (c->updateData(w, colName))
                updated_curves++;
        } else if (c->updateData(w, colName))
            updated_curves++;
    }
    foreach (int key, to_remove) {
        const int idx = d_plot->plotItem(key) ? c_keys.indexOf(key) : -1;
        if (idx < 0)
            continue;
        removeCurve(idx);
        updated_curves++;
    }
    if (updated_curves) {
        if (d_point_colorbar_active)
            syncPointColorBar();
        updateStatReferenceLines();
        if (isPiePlot())
            updatePlot();
        else {
            if (m_autoscale) {
                for (int i = 0; i < QwtPlot::axisCnt; i++) {
                    if (!isToriAxisUserLocked(d_plot, i))
                        d_plot->setAxisAutoScale(i);
                }
                applyLogAutoScalePositiveRange();
            }
            if (d_linked_parent)
                syncLinkedAxesFromParent();
            d_plot->replot();
            syncChildLinkedLayers();
        }
    }
}

double Graph::selectedXStartValue()
{
    if (d_range_selector)
        return d_range_selector->minXValue();
    else
        return 0;
}

double Graph::selectedXEndValue()
{
    if (d_range_selector)
        return d_range_selector->maxXValue();
    else
        return 0;
}

QwtPlotItem *Graph::plotItem(int index)
{
    if (!n_curves || index >= n_curves || index < 0)
        return 0;

    return d_plot->plotItem(c_keys[index]);
}

int Graph::plotItemIndex(QwtPlotItem *it) const
{
    for (int i = 0; i < n_curves; i++) {
        if (d_plot->plotItem(c_keys[i]) == it)
            return i;
    }
    return -1;
}

QwtPlotCurve *Graph::curve(int index) const
{
    if (!n_curves || index >= n_curves || index < 0)
        return 0;

    return d_plot->curve(c_keys[index]);
}

int Graph::curveIndex(QwtPlotCurve *c) const
{
    return plotItemIndex(c);
}

int Graph::range(const int index, double& start, double& end) const
{
    if (d_range_selector && d_range_selector->selectedCurve() == curve(index)) {
        start = d_range_selector->minXValue();
        end = d_range_selector->maxXValue();
        return d_range_selector->dataSize();
    } else {
    QwtPlotCurve *c = curve(index);
        if (!c) return 0;
        start = c->minXValue();
        end = c->maxXValue();
        return c->dataSize();
    }
}

CurveLayout Graph::initCurveLayout()
{
    CurveLayout cl;
    cl.connectType = 1;
    cl.lStyle = 0;
    cl.lWidth = 2;
    cl.sSize = 9;
    cl.sType = defaultCurveSymbolIndex(0);
    cl.filledArea = 0;
    cl.aCol = 0;
    cl.aStyle = 0;
    const QList<QColor> defPalette = paletteColors(defaultCurvePaletteName());
    const QRgb firstCol = defPalette.isEmpty() ? QColor(Qt::black).rgba() : defPalette.first().rgba();
    cl.lCol = firstCol;
    cl.penWidth = 2;
    cl.symCol = firstCol;
    cl.fillCol = firstCol;
    cl.symbolFill = true;
    return cl;
}

CurveLayout Graph::initCurveLayout(int style, int curves)
{
    int i = n_curves - 1;

    CurveLayout cl = initCurveLayout();
    int color;
    guessUniqueCurveLayout(color, cl.sType);

    cl.lCol = color;
    cl.symCol = color;
    cl.fillCol = color;

    const QList<QColor> defPalette = paletteColors(defaultCurvePaletteName());
    const QColor okCol = defPalette.isEmpty() ? QColor(Qt::black)
                                              : defPalette.at(qMax(0, i) % defPalette.size());

    if (style == Graph::Line)
        cl.sType = 0;
    else if (style == Graph::VerticalDropLines)
        cl.connectType = 2;
    else if (style == Graph::HorizontalSteps || style == Graph::VerticalSteps) {
        cl.connectType = 3;
        cl.sType = 0;
    } else if (style == Graph::Spline)
        cl.connectType = 5;
    else if (style == Graph::VerticalBars || style == Graph::HorizontalBars) {
        cl.filledArea = 1;
        cl.lWidth = 1;
        cl.lCol = okCol.darker(140).rgba();
        cl.aCol = okCol.rgba();
        cl.fillCol = okCol.rgba();
        cl.aStyle = 0;
        cl.sType = 0;
        const int effBarCount = qMax(1, curves);
        QwtBarCurve *b = dynamic_cast<QwtBarCurve *>(curve(i));
        if (b) {
            if (b->isStacked() || effBarCount <= 1) {
                b->setGap(35);
                b->setOffset(0);
            } else {
                b->setGap(qBound(30, qRound(100.0 * (1.0 - 0.74 / double(effBarCount))), 92));
                b->setOffset(qRound((double(i) - 0.5 * double(effBarCount - 1)) * 106.0));
            }
        }
    } else if (style == Graph::Histogram) {
        cl.filledArea = 1;
        cl.lWidth = 1;
        cl.lCol = okCol.darker(140).rgba();
        QColor histFill = okCol;
        histFill.setAlpha(165);
        cl.aCol = histFill.rgba();
        cl.fillCol = histFill.rgba();
        cl.aStyle = 0;
        cl.sType = 0;
    } else if (style == Graph::Area) {
        cl.filledArea = 1;
        cl.aCol = color;
        cl.sType = 0;
    }
    return cl;
}

bool Graph::canConvertTo(QwtPlotCurve *c, CurveType type)
{
    if (!c)
        return false;
    // conversion between VectXYXY and VectXYAM is possible, but not implemented
    if (dynamic_cast<VectorCurve *>(c))
        return false;
    // conversion between Pie, Histogram and Box should be possible (all of them take one input
    // column), but lots of special-casing in ApplicationWindow and Graph makes this very difficult
    if (dynamic_cast<QwtPieCurve *>(c) || dynamic_cast<QwtHistogram *>(c)
        || dynamic_cast<BoxCurve *>(c))
        return false;
    // converting error bars doesn't make sense
    if (dynamic_cast<QwtErrorPlotCurve *>(c))
        return false;
    // line/symbol, area and bar curves can be converted to each other
    if (dynamic_cast<DataCurve *>(c))
        return type == Line || type == Scatter || type == LineSymbols || type == VerticalBars
                || type == HorizontalBars || type == HorizontalSteps || type == VerticalSteps
                || type == Area || type == VerticalDropLines || type == Spline;
    return false;
}

void Graph::setCurveType(int curve_index, CurveType type, bool update)
{
    if (curve_index < 0 || curve_index >= n_curves || curve_index >= c_type.size())
        return;
    CurveType old_type = static_cast<CurveType>(c_type[curve_index]);
    // nothing changed
    if (type == old_type)
        return;
    // A function curve read back from a project is inserted as Line (insertFunctionCurve, so a
    // fit curve keeps the Line type it was created with); restoring a saved Function type only
    // re-tags it, the item already is a FunctionCurve.
    if (type == Function) {
        const PlotCurve *pc = dynamic_cast<const PlotCurve *>(curve(curve_index));
        if (old_type == Line && pc && pc->type() == Function)
            c_type[curve_index] = Function;
        return;
    }
    // not all types can be modified cleanly
    if (type != Line && type != Scatter && type != LineSymbols && type != VerticalBars
        && type != Area && type != VerticalDropLines && type != Spline && type != HorizontalSteps
        && type != HorizontalBars && type != VerticalSteps)
        return;
    if (old_type != Line && old_type != Scatter && old_type != LineSymbols
        && old_type != VerticalBars && old_type != Area && old_type != VerticalDropLines
        && old_type != Spline && old_type != HorizontalSteps && old_type != HorizontalBars
        && old_type != VerticalSteps)
        return;

    if ((type == VerticalBars || type == HorizontalBars)
        != (old_type == VerticalBars || old_type == HorizontalBars)) {
        // bar curves are represented by a different class than the other curve types, which makes
        // changing from/to horizontal or vertical bars more difficult
        PlotCurve *base_curve = dynamic_cast<PlotCurve *>(curve(curve_index));
        if (!base_curve || base_curve->type() == Function)
            return;
        DataCurve *old_curve = dynamic_cast<DataCurve *>(base_curve);
        if (!old_curve)
            return;
        DataCurve *new_curve = nullptr;
        switch (type) {
        case Histogram:
        case Box:
        case Pie:
            return;
        case VerticalBars:
            new_curve = new QwtBarCurve(QwtBarCurve::Vertical, old_curve->table(),
                                        old_curve->xColumnName(), old_curve->yColumnName(),
                                        old_curve->startRow(), old_curve->endRow());
            new_curve->setStyle(QwtPlotCurve::UserCurve);
            break;
        case HorizontalBars:
            new_curve = new QwtBarCurve(QwtBarCurve::Horizontal, old_curve->table(),
                                        old_curve->xColumnName(), old_curve->yColumnName(),
                                        old_curve->startRow(), old_curve->endRow());
            new_curve->setStyle(QwtPlotCurve::UserCurve);
            break;
        default:
            new_curve = new DataCurve(old_curve->table(), old_curve->xColumnName(),
                                      old_curve->yColumnName(), old_curve->startRow(),
                                      old_curve->endRow());
            break;
        }

        const bool wasVisible = old_curve->isVisible();
        // 曲線そのものが持つ状態（c_type や表には無いもの）を新しい曲線へ写す。写さないと
        // 縦棒・横棒との行き来で、独自ラベル（凡例の名前）、右軸・上軸への割り当て、
        // 色分け、記号の間引き、積み重ねのずらし・0-1 正規化が消える。ずらしと正規化は
        // 下の loadData() が読み直した点に掛け直す。点と表の行の対応も loadData() が作る。
        new_curve->setCustomLabel(old_curve->customLabel());
        new_curve->setAxis(old_curve->xAxis(), old_curve->yAxis());
        new_curve->setPointColorMap(old_curve->pointColorMap());
        new_curve->setSymbolSkip(old_curve->symbolSkip());
        new_curve->setStackOffset(old_curve->stackOffsetX(), old_curve->stackOffsetY());
        new_curve->setStackNormalize01(old_curve->stackNormalize01());
        old_curve->clearErrorBars();
        curve_index = plotItemIndex(old_curve);
        if (curve_index < 0 || curve_index >= n_curves || curve_index >= c_type.size()) {
            delete new_curve;
            return;
        }

        const int fitIdx = d_fit_curves.indexOf(old_curve);
        if (fitIdx >= 0)
            d_fit_curves.replace(fitIdx, new_curve);

        if (d_active_tool) {
            if (DataPickerTool *dpt = dynamic_cast<DataPickerTool *>(d_active_tool)) {
                if (dpt->selectedCurve() == old_curve)
                    setActiveTool(NULL);
            }
        }
        if (d_range_selector && old_curve == d_range_selector->selectedCurve())
            d_range_selector->setSelectedCurve(new_curve);

        const long oldKey = c_keys[curve_index];
        const bool wasHighlighted = (d_plot->highlightedCurveKey() == oldKey);
        d_plot->removeCurve(oldKey);
        old_curve = nullptr;
        c_keys[curve_index] = d_plot->insertCurve(new_curve);
        if (wasHighlighted)
            d_plot->setHighlightedCurveKey(c_keys[curve_index]);
        new_curve->setVisible(wasVisible);
        new_curve->loadData();
    }

    c_type[curve_index] = type;
    if (!update)
        return;

    CurveLayout cl = initCurveLayout(type, 1);
    updateCurveLayout(curve_index, &cl);
    updatePlot();
}

void Graph::updateCurveLayout(int index, const CurveLayout *cL)
{
    PlotCurve *c = dynamic_cast<PlotCurve *>(this->curve(index));
    if (!c)
        return;
    if (index < 0 || c_type.isEmpty() || (int)c_type.size() <= index)
        return;

    QPen pen = QPen(QBrush(ColorButton::color(cL->symCol)), cL->penWidth, Qt::SolidLine);
    if (cL->symbolFill)
        c->setSymbol(QwtSymbol(SymbolBox::style(cL->sType), QBrush(ColorButton::color(cL->fillCol)),
                               pen, QSize(cL->sSize, cL->sSize)));
    else
        c->setSymbol(
                QwtSymbol(SymbolBox::style(cL->sType), QBrush(), pen, QSize(cL->sSize, cL->sSize)));

    // custom dash pattern
    pen = QPen(QBrush(ColorButton::color(cL->lCol)), cL->lWidth, getPenStyle(cL->lStyle));
    pen.setCapStyle(static_cast<Qt::PenCapStyle>(cL->lCapStyle));
    pen.setJoinStyle(static_cast<Qt::PenJoinStyle>(cL->lJoinStyle));
    QVector<qreal> customDash;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    for (auto v : (cL->lCustomDash).split(" ", Qt::SkipEmptyParts))
#else
    for (auto v : (cL->lCustomDash).split(" ", QString::SkipEmptyParts))
#endif
        customDash << v.toDouble();
    if (pen.style() == Qt::CustomDashLine)
        pen.setDashPattern(customDash);
    c->setPen(pen);

    int style = c_type[index];
    if (style == Scatter || style == VectXYXY || style == VectXYAM)
        c->setStyle(QwtPlotCurve::NoCurve);
    else if (style == VerticalBars || style == HorizontalBars || style == Histogram || style == Box
             || style == Pie)
        c->setStyle(QwtPlotCurve::UserCurve);
    else if (style == Spline) {
        c->setStyle(QwtPlotCurve::Lines);
        c->setCurveAttribute(QwtPlotCurve::Fitted, true);
    } else if (style == VerticalSteps) {
        c->setStyle(QwtPlotCurve::Steps);
        c->setCurveAttribute(QwtPlotCurve::Inverted, true);
    } else
        c->setStyle((QwtPlotCurve::CurveStyle)cL->connectType);

    QBrush brush = QBrush(ColorButton::color(cL->aCol));
    if (cL->filledArea)
        brush.setStyle(getBrushStyle(cL->aStyle));
    else
        brush.setStyle(Qt::NoBrush);
    c->setBrush(brush);
}

void Graph::updateErrorBars(QwtErrorPlotCurve *er, bool xErr, int width, int cap, const QColor &c,
                            bool plus, bool minus, bool through)
{
    if (!er)
        return;

    if (er->width() == width && er->capLength() == cap && er->color() == c && er->plusSide() == plus
        && er->minusSide() == minus && er->throughSymbol() == through && er->xErrors() == xErr)
        return;

    er->setWidth(width);
    er->setCapLength(cap);
    er->setColor(c);
    er->setXErrors(xErr);
    er->drawThroughSymbol(through);
    er->drawPlusSide(plus);
    er->drawMinusSide(minus);

    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::addErrorBars(const QString &yColName, Table *errTable, const QString &errColName,
                         int type, int width, int cap, const QColor &color, bool through,
                         bool minus, bool plus)
{
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        DataCurve *c = dynamic_cast<DataCurve *>(d_plot->curve(c_keys[i]));
        if (c && c->title().text() == yColName && c_type[i] != ErrorBars) {
            return addErrorBars(c->xColumnName(), yColName, errTable, errColName, type, width, cap,
                                color, through, minus, plus);
        }
    }
    return false;
}

bool Graph::addErrorBars(const QString &xColName, const QString &yColName, Table *errTable,
                         const QString &errColName, int type, int width, int cap,
                         const QColor &color, bool through, bool minus, bool plus)
{
    DataCurve *master_curve = masterCurve(xColName, yColName);
    if (!master_curve)
        return false;

    QwtErrorPlotCurve *er = new QwtErrorPlotCurve(type, errTable, errColName);
    er->setMasterCurve(master_curve);
    er->setCapLength(cap);
    er->setColor(color);
    er->setWidth(width);
    er->drawPlusSide(plus);
    er->drawMinusSide(minus);
    er->drawThroughSymbol(through);
    er->setVisible(master_curve->isVisible());

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = ErrorBars;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(er);
    syncCurveZOrder();

    updatePlot();
    return true;
}

void Graph::plotPie(Table *w, const QString &name, const QPen &pen, int brush, int size,
                    int firstColor, int startRow, int endRow, bool visible)
{
    if (endRow < 0)
        endRow = w->numRows() - 1;

    QwtPieCurve *pieCurve = new QwtPieCurve(w, name, startRow, endRow);
    pieCurve->loadData();
    pieCurve->setPen(pen);
    pieCurve->setRay(size);
    pieCurve->setFirstColor(firstColor);
    pieCurve->setBrushStyle(getBrushStyle(brush));
    pieCurve->setVisible(visible);

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(pieCurve);

    c_type.resize(n_curves);
    c_type[n_curves - 1] = Pie;

    for (int a = 0; a < QwtPlot::axisCnt; ++a)
        enableAxis(a, false);
    drawAxesBackbones(false);
    drawCanvasFrame(false, 0);
    setFrame(0, Qt::black);
}

void Graph::plotPie(Table *w, const QString &name, int startRow, int endRow)
{
    const MarkerAxisState axesBefore = markerAxisState();
    int ycol = w->colIndex(name);
    int size = 0;
    double sum = 0.0;

    Column *y_col_ptr = w->column(ycol);
    auto yColType = w->columnType(ycol);

    if (endRow < 0)
        endRow = w->numRows() - 1;

    for (int i = 0; i < QwtPlot::axisCnt; i++)
        d_plot->enableAxis(i, false);
    scalePicker->refresh();

    d_plot->setTitle(QString());
    drawCanvasFrame(false, 0);
    setFrame(0, Qt::black);

    QVarLengthArray<double> Y(abs(endRow - startRow) + 1);
    QStringList sliceNames;
    int labelCol = -1;
    for (int c = 0; c < w->numCols(); ++c) {
        if (c == ycol || !w->column(c))
            continue;
        if (w->column(c)->columnMode() == SciDAVis::ColumnMode::Text
            || w->colPlotDesignation(c) == SciDAVis::X) {
            labelCol = c;
            break;
        }
    }
    Column *lblColPtr = (labelCol >= 0) ? w->column(labelCol) : nullptr;

    for (int row = startRow; row <= endRow && row < y_col_ptr->rowCount(); row++) {
        if (!y_col_ptr->isInvalid(row) && !y_col_ptr->isMasked(row)) {
            double val = 0.0;
            if (yColType == SciDAVis::ColumnMode::Text) {
                QString yval = y_col_ptr->textAt(row);
                bool valid_data = true;
                val = QLocale().toDouble(yval, &valid_data);
                if (!valid_data)
                    continue;
            } else {
                val = y_col_ptr->valueAt(row);
            }
            if (!std::isfinite(val) || val <= 0.0)
                continue;

            Y[size] = val;
            sum += val;
            QString sLbl;
            if (lblColPtr && row < lblColPtr->rowCount() && !lblColPtr->isInvalid(row)) {
                sLbl = (lblColPtr->columnMode() == SciDAVis::ColumnMode::Text)
                        ? lblColPtr->textAt(row).trimmed()
                        : QLocale::c().toString(lblColPtr->valueAt(row), 'g', 6);
            }
            if (sLbl.isEmpty())
                sLbl = QString::number(size + 1);
            sliceNames << sLbl;
            size++;
        }
    }
    if (!size || sum <= 0.0)
        return;
    Y.resize(size);

    QwtPieCurve *pieCurve = new QwtPieCurve(w, name, startRow, endRow);
    pieCurve->setData(Y.data(), Y.data(), size);

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(pieCurve);

    c_type.resize(n_curves);
    c_type[n_curves - 1] = Pie;

    double maxPieVal = 1.0;
    for (int i = 0; i < size; ++i) {
        if (std::isfinite(Y[i]) && Y[i] > maxPieVal)
            maxPieVal = Y[i];
    }
    m_autoscale = false;
    setScale(QwtPlot::xBottom, -2.0 * maxPieVal, 3.0 * maxPieVal, 0.0, 5, 0, 0, false);
    setScale(QwtPlot::yLeft, -1.2 * maxPieVal, 3.8 * maxPieVal, 0.0, 5, 0, 0, false);
    for (int a = 0; a < QwtPlot::axisCnt; ++a)
        enableAxis(a, false);
    drawAxesBackbones(false);
    drawCanvasFrame(false, 0);

    d_plot->updateLayout();
    QRect canvas_rect = d_plot->plotLayout()->canvasRect();
    if (canvas_rect.width() < 60 || canvas_rect.height() < 60)
        canvas_rect = QRect(0, 0, 420, 420);

    bool legendShowsPct = false;
    int estLegW = 0;
    int estLegH = 0;
    if (legendMarkerID >= 0) {
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk) {
            QString text;
            for (int i = 0; i < size; i++) {
                text += "\\p{" + QString::number(i + 1) + "} " + sliceNames.value(i) + "\n";
            }
            mrk->setText(text);
            legendShowsPct = text.contains(QLatin1Char('%'));
            QwtText qtLeg(text);
            qtLeg.setFont(mrk->font());
            const QSize lsz = qtLeg.textSize();
            estLegW = lsz.width() + 28;
            estLegH = lsz.height() + 10;
        }
    }

    const bool hasVisPieLeg = (legendMarkerID >= 0) && isLegendVisible();
    const bool topBandPieLeg =
            hasVisPieLeg && (estLegW > int(0.34 * double(canvas_rect.width())));
    double pieRadius = 0.285 * qMin(canvas_rect.width(), canvas_rect.height());
    double pieCx = double(canvas_rect.center().x());
    double pieCy = double(canvas_rect.center().y());
    if (topBandPieLeg) {
        const double legBottom =
                qBound(0.08 * double(canvas_rect.height()), double(4 + estLegH),
                       0.52 * double(canvas_rect.height()));
        const double pad = 0.028 * double(canvas_rect.height());
        const double availH =
                qMax(0.20 * double(canvas_rect.height()),
                     double(canvas_rect.height()) - legBottom - 2.0 * pad);
        pieRadius = qMax(14.0,
                         qMin((legendShowsPct ? 0.31 : 0.255) * double(canvas_rect.width()),
                              (legendShowsPct ? 0.44 : 0.34) * availH));
        pieCx = double(canvas_rect.center().x());
        pieCy = double(canvas_rect.top()) + legBottom + pad + 0.5 * availH;
    } else if (hasVisPieLeg) {
        pieRadius = (legendShowsPct ? 0.285 : 0.24)
                * qMin(canvas_rect.width(), canvas_rect.height());
        pieCx = double(canvas_rect.left()) + 0.36 * double(canvas_rect.width());
        pieCy = double(canvas_rect.top()) + 0.52 * double(canvas_rect.height());
    }
    const double radius = 1.22 * pieRadius;

    const double PI = 4.0 * atan(1.0);
    double angle = 90.0;
    QFont pctFont(QStringLiteral("Arial"), 10, QFont::Normal, false);
    pctFont.setBold(false);
    pctFont.setHintingPreference(QFont::PreferNoHinting);

    QVector<QRect> placedPctBoxes;
    for (int i = 0; i < size; i++) {
        const double pctVal = Y[i] / sum * 100.0;
        const double value = Y[i] / sum * 360.0;
        const double alabel = (angle - value * 0.5) * PI / 180.0;
        const QString pctStr =
                QLocale::c().toString(pctVal, 'f', 1) + QStringLiteral("%");

        if (!legendShowsPct) {
            Legend *aux = new Legend(d_plot);
            aux->setFrameStyle(0);
            aux->setBackgroundColor(QColor(255, 255, 255, 0));
            aux->setFont(pctFont);
            aux->setText(pctStr);

            int texts = d_texts.size();
            d_texts.resize(++texts);
            d_texts[texts - 1] = d_plot->insertMarker(aux);

            QwtText qtPct(pctStr);
            qtPct.setFont(pctFont);
            const QSize pctSz = qtPct.textSize();
            const int boxW = pctSz.width() + 8;
            const int boxH = pctSz.height() + 4;
            int ox = qBound(2, qRound(pieCx + radius * cos(alabel) - 0.5 * double(boxW)),
                            qMax(2, canvas_rect.width() - boxW - 2));
            int oy = qBound(2, qRound(pieCy - radius * sin(alabel) - 0.5 * double(boxH)),
                            qMax(2, canvas_rect.height() - boxH - 2));
            QRect candBox(ox, oy, boxW, boxH);
            bool collides = false;
            for (const QRect &prev : placedPctBoxes) {
                if (candBox.intersects(prev)) {
                    collides = true;
                    break;
                }
            }
            if (collides) {
                const int dirY = (sin(alabel) >= 0.0) ? -1 : 1;
                for (int step = 1; step <= 4 && collides; ++step) {
                    const int tryY = qBound(2, oy + dirY * step * (boxH - 2),
                                            qMax(2, canvas_rect.height() - boxH - 2));
                    const QRect tryBox(ox, tryY, boxW, boxH);
                    bool hitAny = false;
                    for (const QRect &prev : placedPctBoxes) {
                        if (tryBox.intersects(prev)) {
                            hitAny = true;
                            break;
                        }
                    }
                    if (!hitAny) {
                        oy = tryY;
                        candBox = tryBox;
                        collides = false;
                    }
                }
            }
            if (collides && pctVal < 6.0) {
                aux->setText(QString());
                aux->setVisible(false);
            } else {
                aux->setOrigin(QPoint(ox, oy));
                placedPctBoxes.append(candBox);
            }
        }

        angle -= value;
    }

    if (legendMarkerID >= 0) {
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk)
            placeLegendAnchor(topBandPieLeg ? LegendTopCenter : LegendTopRight);
    }

    d_plot->replot();
    updateScale(axesBefore);
}

bool Graph::plotHistogram(Table *w, QStringList names, int startRow, int endRow)
{
    if (!w)
        return false;
    if (endRow < 0 || endRow >= w->numRows())
        endRow = w->numRows() - 1;

    bool success = false;
    foreach (QString col, names) {
        Column *col_ptr = w->column(col);
        if (!col_ptr || col_ptr->columnMode() != SciDAVis::ColumnMode::Numeric)
            continue;

        QwtHistogram *c = new QwtHistogram(w, col, startRow, endRow);
        c->loadData();
        c->setStyle(QwtPlotCurve::UserCurve);

        c_type.resize(++n_curves);
        c_type[n_curves - 1] = Histogram;
        c_keys.resize(n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(c);
        syncCurveZOrder();

        CurveLayout cl = initCurveLayout(Histogram, names.size());
        updateCurveLayout(n_curves - 1, &cl);
        QBrush hb = c->brush();
        QColor hc = hb.color();
        hc.setAlpha(165);
        hb.setColor(hc);
        hb.setStyle(Qt::SolidPattern);
        c->setBrush(hb);

        addLegendItem(col);

        success = true;
    }
    if (success && !d_refreshing_auto_titles)
        refreshAutoAxisTitlesFromColumns();

    return success;
}

void Graph::insertPlotItem(QwtPlotItem *i, int type)
{
    c_type.resize(++n_curves);
    c_type[n_curves - 1] = type;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(i);
    syncCurveZOrder();

    if (i->rtti() != QwtPlotItem::Rtti_PlotSpectrogram)
        addLegendItem(i->title().text());
}

bool Graph::insertCurvesList(Table *w, const QStringList &names, int style, int lWidth, int sSize,
                             int startRow, int endRow, const QString &xColName)
{
    if (style == Graph::Pie)
        plotPie(w, names[0], startRow, endRow);
    else if (style == Box)
        plotBoxDiagram(w, names, startRow, endRow);
    else if (style == Histogram)
        plotHistogram(w, names, startRow, endRow);
    else if (style == Graph::VectXYXY || style == Graph::VectXYAM) {
        const int prevCurves = n_curves;
        plotVectorCurve(w, names, style, startRow, endRow);
        if (n_curves <= prevCurves)
            return false;
    } else {
        QStringList errorCurves, otherCurves;
        foreach (QString col, names) {
            int colIndex = w->colIndex(col);
            if (colIndex < 0)
                continue;
            switch (w->colPlotDesignation(colIndex)) {
            case SciDAVis::xErr:
            case SciDAVis::yErr:
                errorCurves << col;
                break;
            default:
                otherCurves << col;
                break;
            }
        }
        QStringList lst = otherCurves + errorCurves;

        for (int i = 0; i < lst.size(); i++) {
            CurveType type_of_i;
            int j = w->colIndex(lst[i]);
            if (j < 0)
                continue;
            bool ok = false;
            if (i >= otherCurves.size()) {
                type_of_i = ErrorBars;
                int ycol = -1;
                for (int k = j - 1; k >= 0; k--) {
                    if (w->colPlotDesignation(k) == SciDAVis::Y) {
                        ycol = k;
                        break;
                    }
                }
                if (ycol < 0)
                    ycol = w->colY(w->colIndex(lst[i]));
                if (ycol < 0)
                    return false;

                if (w->colPlotDesignation(j) == SciDAVis::xErr)
                    ok = addErrorBars(w->colName(ycol), w, lst[i],
                                      (int)QwtErrorPlotCurve::Horizontal);
                else
                    ok = addErrorBars(w->colName(ycol), w, lst[i]);
            } else {
                type_of_i = (CurveType)style;
                // 呼び出し側が X 列を名指ししたときはそれを使う。表の X 指定列から探すと
                // Y 列の左にある別の X 指定列に結び付いてしまう（plot.xy の x_col が無視された）。
                if (!xColName.isEmpty() && w->colIndex(xColName) >= 0) {
                    ok = insertCurve(w, xColName, lst[i], style, startRow, endRow);
                    if (ok)
                        emit modifiedGraph();
                } else {
                    ok = insertCurve(w, lst[i], style, startRow, endRow);
                }
            }

            if (ok) {
                CurveLayout cl = initCurveLayout(type_of_i, otherCurves.size());
                cl.sSize = sSize;
                cl.lWidth = lWidth;
                updateCurveLayout(i, &cl);
            } else
                return false;
        }
    }
    updatePlot();
    return true;
}

bool Graph::insertCurve(Table *w, const QString &name, int style, int startRow, int endRow)
{ // provided for convenience
    int ycol = w->colIndex(name);
    int xcol = w->colX(ycol);

    bool succes = insertCurve(w, w->colName(xcol), w->colName(ycol), style, startRow, endRow);
    if (succes)
        emit modifiedGraph();
    return succes;
}

bool Graph::insertCurve(Table *w, int xcol, const QString &name, int style)
{
    return insertCurve(w, w->colName(xcol), w->colName(w->colIndex(name)), style);
}

bool Graph::insertCurve(Table *w, const QString &xColName, const QString &yColName, int style,
                        int startRow, int endRow, bool keepEmpty)
{
    if (!w)
        return false;
    DataCurve *c = 0;

    switch (style) {
    case Histogram:
    case Box:
    case Pie:
        return false;
    case VerticalBars:
        c = new QwtBarCurve(QwtBarCurve::Vertical, w, xColName, yColName, startRow, endRow);
        c->setStyle(QwtPlotCurve::UserCurve);
        break;
    case HorizontalBars:
        c = new QwtBarCurve(QwtBarCurve::Horizontal, w, xColName, yColName, startRow, endRow);
        c->setStyle(QwtPlotCurve::UserCurve);
        break;
    default:
        c = new DataCurve(w, xColName, yColName, startRow, endRow);
        break;
    };

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = style;
    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(c);
    syncCurveZOrder();

    const QList<QColor> barPalette = paletteColors(defaultCurvePaletteName());
    if (style == VerticalBars || style == HorizontalBars) {
        QVector<QwtBarCurve *> allStyleBars;
        QVector<QwtBarCurve *> groupedBars;
        for (int bi = 0; bi < n_curves; ++bi) {
            if (bi < c_type.size() && c_type[bi] == style) {
                if (QwtBarCurve *bc = dynamic_cast<QwtBarCurve *>(curve(bi))) {
                    allStyleBars.append(bc);
                    if (!bc->isStacked())
                        groupedBars.append(bc);
                    else
                        bc->setOffset(0);
                }
            }
        }
        const int barIdx = qMax(0, allStyleBars.size() - 1);
        const QColor fillCol = barPalette.isEmpty()
                ? QColor(Qt::black)
                : barPalette.at(barIdx % barPalette.size());
        QPen barPen(fillCol.darker(140), 1, Qt::SolidLine);
        barPen.setCosmetic(false);
        c->setPen(barPen);
        c->setBrush(QBrush(fillCol, Qt::SolidPattern));

        const int K = groupedBars.size();
        for (int ord = 0; ord < K; ++ord) {
            if (K <= 1) {
                groupedBars[ord]->setGap(35);
                groupedBars[ord]->setOffset(0);
            } else {
                groupedBars[ord]->setGap(
                        qBound(30, qRound(100.0 * (1.0 - 0.74 / double(K))), 92));
                groupedBars[ord]->setOffset(
                        qRound((double(ord) - 0.5 * double(K - 1)) * 106.0));
            }
        }
    } else {
        c->setPen(QPen(Qt::black, widthLine));
    }

    if (!c->loadData())
        return false;
    // loadData() は有効な点が無くなった既存の曲線を空のまま残す。新しく作図する曲線は従来どおり、
    // 有効な点が 1 つも無ければ作らない（空の列で作図したときはエラーになる）。プロジェクトの
    // 読み込み（keepEmpty）では、保存したときに空だった曲線も体裁ごと戻す。
    if (c->dataSize() <= 0 && !keepEmpty) {
        c->remove();
        return false;
    }

    if (style == HorizontalBars) {
        bool anyHorizTextCurve = false;
        for (int bi = 0; bi < n_curves; ++bi) {
            if (bi < c_type.size() && c_type[bi] != HorizontalBars && c_type[bi] != ErrorBars) {
                if (DataCurve *odc = dynamic_cast<DataCurve *>(curve(bi))) {
                    if (odc->table()) {
                        Column *oxc = odc->table()->column(odc->xColumnName());
                        if (oxc && oxc->columnMode() == SciDAVis::ColumnMode::Text)
                            anyHorizTextCurve = true;
                    }
                }
            }
        }
        if (!anyHorizTextCurve
            && (axisType[QwtPlot::xBottom] == AxisType::Txt
                || dynamic_cast<QwtTextScaleDraw *>(d_plot->axisScaleDraw(QwtPlot::xBottom)))) {
            d_custom_category_labels[QwtPlot::xBottom].clear();
            axesFormatInfo[QwtPlot::xBottom].clear();
            setLabelsNumericFormat(QwtPlot::xBottom, Plot::Automatic, 4);
            setToriAxisUserLocked(d_plot, QwtPlot::xBottom, false);
            d_user_step[QwtPlot::xBottom] = 0.0;
            d_plot->setAxisAutoScale(QwtPlot::xBottom);
        }
    }

    addLegendItem(yColName);
    if (!d_refreshing_auto_titles)
        refreshAutoAxisTitlesFromColumns();
    updatePlot();

    return true;
}

void Graph::setCurveAxes(int curveIndex, int xAxis, int yAxis)
{
    if (!d_plot || curveIndex < 0 || curveIndex >= n_curves)
        return;
    if (xAxis != QwtPlot::xBottom && xAxis != QwtPlot::xTop)
        xAxis = QwtPlot::xBottom;
    if (yAxis != QwtPlot::yLeft && yAxis != QwtPlot::yRight)
        yAxis = QwtPlot::yLeft;

    QwtPlotItem *it = plotItem(curveIndex);
    if (!it)
        return;
    it->setAxis(xAxis, yAxis);
    if (DataCurve *dc = dynamic_cast<DataCurve *>(it)) {
        for (DataCurve *eb : dc->errorBarsList()) {
            if (eb)
                eb->setAxis(xAxis, yAxis);
        }
    }

    auto ensureOppositeAxisVisible = [&](int secAxis, int primAxis) {
        enableAxis(secAxis, true);
        setAxisBackboneVisible(secAxis, true);
        if (QwtScaleDraw *sd = d_plot->axisScaleDraw(secAxis))
            sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        if (ScaleDraw *s = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(secAxis))) {
            const ScaleDraw *primSd = dynamic_cast<const ScaleDraw *>(d_plot->axisScaleDraw(primAxis));
            const int targetMaj = (s->majorTicksStyle() == ScaleDraw::None)
                    ? (primSd && primSd->majorTicksStyle() != ScaleDraw::None
                               ? primSd->majorTicksStyle()
                               : int(ScaleDraw::In))
                    : s->majorTicksStyle();
            const int targetMin = (s->minorTicksStyle() == ScaleDraw::None && primSd)
                    ? primSd->minorTicksStyle()
                    : s->minorTicksStyle();
            d_plot->setMajorTicksType(secAxis, targetMaj);
            d_plot->setMinorTicksType(secAxis, targetMin);
        }
        if (!isAxisUserLocked(secAxis))
            d_plot->setAxisAutoScale(secAxis);
    };

    if (yAxis == QwtPlot::yRight)
        ensureOppositeAxisVisible(QwtPlot::yRight, QwtPlot::yLeft);
    if (xAxis == QwtPlot::xTop)
        ensureOppositeAxisVisible(QwtPlot::xTop, QwtPlot::xBottom);

    if (m_autoscale) {
        if (!isAxisUserLocked(QwtPlot::yLeft))
            d_plot->setAxisAutoScale(QwtPlot::yLeft);
        if (!isAxisUserLocked(QwtPlot::xBottom))
            d_plot->setAxisAutoScale(QwtPlot::xBottom);
    }

    if (!d_refreshing_auto_titles)
        refreshAutoAxisTitlesFromColumns();
    if (scalePicker)
        scalePicker->refresh();
    updatePlot();
    emit modifiedGraph();
}

void Graph::plotVectorCurve(Table *w, const QStringList &colList, int style, int startRow,
                            int endRow)
{
    if (!w || !w->d_future_table || colList.count() != 4)
        return;

    auto resolveColIdx = [w](const QString &spec) -> int {
        if (spec.isEmpty())
            return -1;
        const int numCols = w->numCols();
        const QString tblPrefix = QString(w->name()) + QLatin1Char('_');
        if (spec.startsWith(tblPrefix)) {
            const QString shortName = spec.mid(tblPrefix.size());
            for (int i = 0; i < numCols; ++i) {
                if (const Column *col = w->column(i)) {
                    if (col->name() == shortName)
                        return i;
                }
            }
        }
        for (int i = 0; i < numCols; ++i) {
            if (const Column *col = w->column(i)) {
                if (col->name() == spec)
                    return i;
            }
        }
        const int fallback = w->colIndex(spec);
        return (fallback >= 0 && fallback < numCols && w->column(fallback) != nullptr) ? fallback
                                                                                       : -1;
    };

    QStringList canonicalCols;
    canonicalCols.reserve(4);
    for (int k = 0; k < 4; ++k) {
        const int idx = resolveColIdx(colList[k]);
        if (idx < 0)
            return;
        canonicalCols.append(w->colName(idx));
    }

    if (endRow < 0)
        endRow = w->numRows() - 1;

    VectorCurve *v = 0;
    if (style == VectXYAM)
        v = new VectorCurve(VectorCurve::XYAM, w, canonicalCols[0], canonicalCols[1],
                            canonicalCols[2], canonicalCols[3], startRow, endRow);
    else
        v = new VectorCurve(VectorCurve::XYXY, w, canonicalCols[0], canonicalCols[1],
                            canonicalCols[2], canonicalCols[3], startRow, endRow);

    if (!v)
        return;

    if (!v->loadData()) {
        delete v;
        return;
    }
    v->setStyle(QwtPlotCurve::NoCurve);

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = style;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(v);

    addLegendItem(canonicalCols[1]);
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QString plainT = unwrapNoWrapAxisTitle(axisTitle(a)).trimmed();
        if (isAxisPlaceholderTitle(plainT)) {
            QwtText emptyT = d_plot->axisTitle(a);
            emptyT.setText(QString());
            d_plot->setAxisTitle(a, emptyT);
        }
    }
    if (!d_refreshing_auto_titles)
        refreshAutoAxisTitlesFromColumns();
    updatePlot();
}

void Graph::updateVectorsLayout(int curve, const QColor &color, int width, int arrowLength,
                                int arrowAngle, bool filled, int position,
                                const QString &xEndColName, const QString &yEndColName)
{
    VectorCurve *vect = (VectorCurve *)this->curve(curve);
    if (!vect)
        return;

    vect->setColor(color);
    vect->setWidth(width);
    vect->setHeadLength(arrowLength);
    vect->setHeadAngle(arrowAngle);
    vect->fillArrowHead(filled);
    vect->setPosition(position);

    if (!xEndColName.isEmpty() && !yEndColName.isEmpty())
        vect->setVectorEnd(xEndColName, yEndColName);

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::updatePlot()
{
    const MarkerAxisState axesBefore = markerAxisState();
    if (d_is_ternary) {
        d_plot->setAxisScale(QwtPlot::xBottom, -0.24, 1.24, 0.2);
        d_plot->setAxisScale(QwtPlot::yLeft, -0.25, 1.23, 0.2);
        for (int i = 0; i < QwtPlot::axisCnt; i++)
            d_plot->enableAxis(i, false);
        drawAxesBackbones(false);
    } else if (d_is_polar) {
        double xLo = 0.0, xHi = 0.0, yLo = 0.0, yHi = 0.0;
        if (d_polar_view_set) {
            xLo = d_polar_view[0];
            xHi = d_polar_view[1];
            yLo = d_polar_view[2];
            yHi = d_polar_view[3];
        } else {
            polarDefaultViewRange(&xLo, &xHi, &yLo, &yHi);
        }
        d_plot->setAxisScale(QwtPlot::xBottom, xLo, xHi);
        d_plot->setAxisScale(QwtPlot::yLeft, yLo, yHi);
        for (int i = 0; i < QwtPlot::axisCnt; i++)
            d_plot->enableAxis(i, false);
        drawAxesBackbones(false);
    } else if (isPoleFigurePlot()) {
        const double lim = ExtraPlotTypes::poleFigureScaleLimit(this);
        d_plot->setAxisScale(QwtPlot::xBottom, -lim, lim);
        d_plot->setAxisScale(QwtPlot::yLeft, -lim, lim);
        d_plot->enableAxis(QwtPlot::xBottom, false);
        d_plot->enableAxis(QwtPlot::yLeft, false);
        d_plot->enableAxis(QwtPlot::xTop, false);
        drawCanvasFrame(false, 0);
        drawAxesBackbones(false);
    } else if (m_autoscale && !zoomOn() && d_active_tool == NULL) {
        for (int i = 0; i < QwtPlot::axisCnt; i++) {
            if (!isToriAxisUserLocked(d_plot, i) && !d_weibull_prob_axis[i]
                && d_custom_tick_positions[i].isEmpty())
                d_plot->setAxisAutoScale(i);
        }
        applyZeroSnapOnAutoAxes();
    }

    // Lock discrete category axes (AxisType::Txt) to [minKey - 0.5, maxKey + 0.5, step = 1.0]
    // (preserving explicit category headroom when user-locked) with 0 minor ticks.
    if (!d_is_ternary && !d_is_polar && !isPoleFigurePlot() && !isPiePlot()) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            if (const QwtTextScaleDraw *tsd =
                        dynamic_cast<const QwtTextScaleDraw *>(d_plot->axisScaleDraw(a))) {
                const QMap<int, QString> catMap = tsd->labelsMap();
                if (!catMap.isEmpty() && !zoomOn()) {
                    const double minKey = double(catMap.firstKey());
                    const double maxKey = double(catMap.lastKey());
                    double cLo = minKey - 0.5;
                    double cHi = maxKey + 0.5;
                    if (isToriAxisUserLocked(d_plot, a)) {
                        if (const QwtScaleDiv *curDiv = d_plot->axisScaleDiv(a)) {
                            const double curLo = qMin(curDiv->lowerBound(), curDiv->upperBound());
                            const double curHi = qMax(curDiv->lowerBound(), curDiv->upperBound());
                            if (std::isfinite(curLo) && std::isfinite(curHi) && curHi > curLo
                                && curLo >= minKey - 1.5 && curLo <= minKey
                                && curHi >= maxKey && curHi <= maxKey + 4.5) {
                                cLo = qMin(cLo, curLo);
                                cHi = qMax(cHi, curHi);
                            }
                        }
                    }
                    const bool isInvCat = d_plot->axisScaleEngine(a)
                            && d_plot->axisScaleEngine(a)->testAttribute(
                                    QwtScaleEngine::Inverted);
                    QwtValueList catMaj;
                    for (auto it = catMap.constBegin(); it != catMap.constEnd(); ++it)
                        catMaj += double(it.key());
                    QwtValueList emptyTicks;
                    QwtValueList catTicks[QwtScaleDiv::NTickTypes];
                    catTicks[QwtScaleDiv::MinorTick] = emptyTicks;
                    catTicks[QwtScaleDiv::MediumTick] = emptyTicks;
                    catTicks[QwtScaleDiv::MajorTick] = catMaj;
                    QwtScaleEngine *catEng = new QwtLinearScaleEngine();
                    QwtScaleDiv catDiv(cLo, cHi, catTicks);
                    if (isInvCat) {
                        catEng->setAttribute(QwtScaleEngine::Inverted);
                        catDiv.invert();
                    }
                    d_plot->setAxisMaxMinor(a, 0);
                    d_plot->setMinorTicksType(a, ScaleDraw::None);
                    d_plot->setAxisMaxMajor(a, qMax(8, catMap.size() + 2));
                    d_plot->setAxisScale(a, isInvCat ? cHi : cLo, isInvCat ? cLo : cHi, 1.0);
                    d_plot->setAxisScaleEngine(a, catEng);
                    d_plot->setAxisScaleDiv(a, catDiv);

                    const int opp = (a == QwtPlot::xBottom) ? QwtPlot::xTop
                            : (a == QwtPlot::yLeft)         ? QwtPlot::yRight
                                                            : -1;
                    if (opp >= 0 && !d_plot->hasVisibleItemOnAxis(opp)
                        && !hasColorScaleOnAxis(opp) && !d_weibull_prob_axis[opp]
                        && d_custom_tick_positions[opp].isEmpty()) {
                        if (QwtScaleDraw *oppSd = d_plot->axisScaleDraw(opp))
                            oppSd->enableComponent(QwtAbstractScaleDraw::Labels, false);
                        d_plot->setMinorTicksType(opp, ScaleDraw::None);
                        d_plot->setMajorTicksType(opp, ScaleDraw::None);
                    }
                }
            }
        }
    }

    // Clamp extreme ±1e308 auto-scaled spans before QwtLinearScaleEngine::divideScale can spin
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QwtScaleDiv *sd = d_plot->axisScaleDiv(a);
        if (!sd)
            continue;
        const double lo = sd->lowerBound();
        const double hi = sd->upperBound();
        if (!std::isfinite(lo) || !std::isfinite(hi) || !std::isfinite(hi - lo)
            || std::fabs(lo) > 1.0e300 || std::fabs(hi) > 1.0e300) {
            const double cLo = std::isfinite(lo) ? qBound(-1.0e150, lo, 1.0e150) : -1.0;
            const double cHi = std::isfinite(hi) ? qBound(-1.0e150, hi, 1.0e150) : 1.0;
            d_plot->setAxisScale(a, cLo, (std::fabs(cHi - cLo) > 1e-300) ? cHi : (cLo + 1.0));
        }
    }

    if (!d_is_ternary && !d_is_polar && !isPoleFigurePlot() && !isPiePlot()) {
        syncCustomAndWeibullAxes();
    }

    d_plot->updateAxes();
    if (!d_is_ternary && !d_is_polar && !isPoleFigurePlot()) {
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
    }

    if (isPiePlot()) {
        drawCanvasFrame(false, 0);
        setFrame(0, Qt::black);
        drawAxesBackbones(false);
        if (QwtPieCurve *c = (QwtPieCurve *)curve(0))
            c->updateBoundingRect();
    }

    updateMarkersBoundingRect(axesBefore);
    d_zoomer[0]->setZoomBase(false);
    d_zoomer[1]->setZoomBase(false);
    updateStatReferenceLines();
    if (d_linked_parent)
        syncLinkedAxesFromParent();
    syncChildLinkedLayers();
}

void Graph::updateScale()
{
    updateScale(markerAxisState());
}

void Graph::updateScale(const MarkerAxisState &axesBefore)
{
    // With autoscale off, freeze the current xBottom/yLeft ranges. The step handed back to
    // setAxisScale() is only meaningful for a uniform linear scale: a log engine reads it in
    // decades (1..1e4 would get step 9), and break / formula-linked / custom-tick / probability
    // axes have non-uniform majors. Those keep their current ScaleDiv unchanged.
    auto freezeAxis = [this](int axis) {
        const QwtScaleDiv *scDiv = d_plot->axisScaleDiv(axis);
        if (!scDiv)
            return;
        const QwtScaleDiv keep = *scDiv;
        const double lo = keep.lowerBound();
        const double hi = keep.upperBound();
        bool uniformLinear = !d_axis_breaks[axis].enabled && !d_weibull_prob_axis[axis]
                && d_custom_tick_positions[axis].isEmpty();
        if (uniformLinear) {
            const QwtScaleEngine *se = d_plot->axisScaleEngine(axis);
            std::unique_ptr<QwtScaleTransformation> xf(se ? se->transformation() : nullptr);
            uniformLinear = xf && xf->type() == QwtScaleTransformation::Linear;
        }
        if (!uniformLinear) {
            d_plot->setAxisScale(axis, lo, hi, 0.0);
            d_plot->setAxisScaleDiv(axis, keep);
            return;
        }
        const QwtValueList lst = keep.ticks(QwtScaleDiv::MajorTick);
        const double step = (lst.count() >= 2) ? fabs(lst[1] - lst[0]) : 0.0;
        d_plot->setAxisScale(axis, lo, hi, step);
    };

    if (!m_autoscale) {
        freezeAxis(QwtPlot::xBottom);
        freezeAxis(QwtPlot::yLeft);
    }

    d_plot->updateAxes();
    updateSecondaryAxis(QwtPlot::xTop);
    updateSecondaryAxis(QwtPlot::yRight);
    updateMarkersBoundingRect(axesBefore);
    syncChildLinkedLayers();
}

void Graph::setBarsGap(int curve, int gapPercent, int offset)
{
    QwtBarCurve *bars = dynamic_cast<QwtBarCurve *>(this->curve(curve));
    if (!bars)
        return;

    if (bars->isStacked()) {
        offset = 0;
    } else if (std::abs(offset) > 10) {
        int groupedCount = 0;
        for (int bi = 0; bi < n_curves; ++bi) {
            if (QwtBarCurve *bc = dynamic_cast<QwtBarCurve *>(this->curve(bi))) {
                if (!bc->isStacked() && bc->orientation() == bars->orientation())
                    ++groupedCount;
            }
        }
        if (groupedCount > 1 && gapPercent < 100 - qRound(88.0 / double(groupedCount))) {
            gapPercent = qBound(
                    25,
                    100 - qRound((100.0 - double(qBound(0, gapPercent, 90))) / double(groupedCount)),
                    94);
        }
    }

    if (bars->gap() == gapPercent && bars->offset() == offset)
        return;

    bars->setGap(gapPercent);
    bars->setOffset(offset);
}

void Graph::removePie()
{
    if (legendMarkerID >= 0) {
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk)
            mrk->setText({});
    }

    d_plot->removeCurve(c_keys[0]);
    d_plot->replot();

    c_keys.resize(0);
    c_type.resize(0);
    n_curves = 0;
    emit modifiedGraph();
}

bool Graph::curveUsesColumn(int index, const QString &s)
{
    QwtPlotItem *it = plotItem(index);
    if (!it)
        return false;

    if (it->title().text() == s)
        return true;

    if (it->rtti() != QwtPlotItem::Rtti_PlotCurve)
        return false;
    PlotCurve *pc = dynamic_cast<PlotCurve *>(it);
    if (!pc || pc->type() == Function)
        return false;
    DataCurve *dc = dynamic_cast<DataCurve *>(pc);
    if (!dc)
        return false;

    return dc->plotAssociation().contains(QRegExp("\\b" + QRegExp::escape(s) + "\\b"))
            || dc->plotAssociation().contains(QRegExp("\b" + s + "\b"));
}

Graph::TakenCurves Graph::takeCurvesUsingColumns(const QStringList &names)
{
    TakenCurves tc;
    QList<QwtPlotItem *> items;
    for (int i = 0; i < n_curves; ++i) {
        for (const QString &n : names) {
            if (curveUsesColumn(i, n)) {
                items << plotItem(i);
                break;
            }
        }
    }
    // the error bars of a taken curve go with it, so its links stay whole
    const QList<QwtPlotItem *> matched = items;
    for (QwtPlotItem *it : matched) {
        DataCurve *dc = dynamic_cast<DataCurve *>(it);
        if (!dc || dc->type() == ErrorBars)
            continue;
        for (DataCurve *er : dc->errorBarsList()) {
            if (er && !items.contains(er) && plotItemIndex(er) >= 0)
                items << er;
        }
    }
    if (!items.isEmpty())
        takeCurveItems(items, tc);
    return tc;
}

int Graph::curveIndexOfKey(int key, const QwtPlotItem *item) const
{
    if (key < 0 || !item || d_plot->plotItem(key) != item)
        return -1;
    const int idx = c_keys.indexOf(key);
    return (idx >= 0 && idx < n_curves) ? idx : -1;
}

void Graph::takeCurveItems(const QList<QwtPlotItem *> &items, TakenCurves &tc)
{
    tc.curves.clear();
    tc.order.clear();
    for (int i = 0; i < n_curves && i < c_keys.size(); ++i)
        tc.order << c_keys[i];
    Legend *legBefore = (legendMarkerID >= 0) ? (Legend *)d_plot->marker(legendMarkerID) : nullptr;
    tc.legendBefore = legBefore ? legBefore->text() : QString();
    tc.legendAfter.clear();
    tc.curvesAfter = -1;

    QList<QPair<int, QwtPlotItem *>> byIndex;
    for (QwtPlotItem *it : items) {
        const int idx = plotItemIndex(it);
        if (idx >= 0)
            byIndex << qMakePair(idx, it);
    }
    std::sort(byIndex.begin(), byIndex.end(),
              [](const QPair<int, QwtPlotItem *> &a, const QPair<int, QwtPlotItem *> &b) {
                  return a.first < b.first;
              });

    // The key of each error bars' curve, read while every item is still in the layer: a curve
    // freed later can leave its address to a new one, but its key is never given out again.
    QHash<QwtPlotItem *, int> masterKeys;
    for (const auto &bi : byIndex) {
        if (QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(bi.second)) {
            const int mi = er->masterCurve() ? plotItemIndex(er->masterCurve()) : -1;
            masterKeys.insert(bi.second, mi >= 0 ? c_keys[mi] : -1);
        }
    }

    // From the last one down, so the indices and legend numbers of the rest stay valid.
    for (int k = byIndex.size() - 1; k >= 0; --k) {
        QwtPlotItem *it = byIndex[k].second;
        const int idx = plotItemIndex(it);
        if (idx < 0)
            continue;
        TakenCurve t;
        t.item = it;
        t.key = c_keys[idx];
        t.masterKey = masterKeys.value(it, -1);
        t.type = c_type[idx];
        t.index = idx;
        t.fitCurve = d_fit_curves.contains(dynamic_cast<QwtPlotCurve *>(it));
        Legend *leg = (legendMarkerID >= 0) ? (Legend *)d_plot->marker(legendMarkerID) : nullptr;
        if (leg && t.type != ErrorBars && !isPiePlot()) {
            const QString token = "\\c{" + QString::number(idx + 1) + "}";
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            const QStringList lines = leg->text().split("\n", Qt::SkipEmptyParts);
#else
            const QStringList lines = leg->text().split("\n", QString::SkipEmptyParts);
#endif
            for (const QString &ln : lines) {
                if (ln.contains(token)) {
                    t.legendLine = ln;
                    break;
                }
            }
        }
        // error bars whose curve stays: unlink them from it (restoreTakenCurves() links back)
        if (QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(it)) {
            if (er->masterCurve() && !items.contains(er->masterCurve()))
                er->detachFromMasterCurve();
        }
        if (removeCurveAt(idx, false) == it)
            tc.curves.prepend(t);
    }
    tc.out = !tc.curves.isEmpty();
    if (legBefore && legendMarkerID >= 0 && d_plot->marker(legendMarkerID) == legBefore) {
        tc.legendAfter = legBefore->text();
        tc.curvesAfter = n_curves;
    }
    d_plot->replot();
}

QString Graph::curveIdentity(int index)
{
    if (index < 0 || index >= n_curves)
        return QString();
    QwtPlotItem *it = plotItem(index);
    QString id = QString::number(c_type.value(index)) + QLatin1Char('|')
            + (it ? it->title().text() : QString());
    if (auto *dc = dynamic_cast<DataCurve *>(it))
        id += QLatin1Char('|') + dc->plotAssociation();
    return id;
}

int Graph::lastCurveKey() const
{
    return d_plot ? d_plot->lastCurveKey() : 0;
}

void Graph::adoptCurveKeys(const QVector<int> &keys, int reserved)
{
    QList<QwtPlotItem *> items;
    QHash<QwtPlotItem *, int> want;
    for (int i = 0; i < n_curves && i < c_keys.size(); ++i) {
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        items << it;
        if (it && keys.value(i, -1) > 0)
            want.insert(it, keys[i]);
    }
    d_plot->rekeyCurves(want, reserved);
    const QMap<int, QwtPlotItem *> now = d_plot->curves();
    for (int i = 0; i < items.size(); ++i) {
        if (items[i])
            c_keys[i] = now.key(items[i], c_keys[i]);
    }
}

void Graph::restoreTakenCurves(TakenCurves &tc)
{
    if (!tc.out)
        return;
    tc.out = false;

    // The items held here. An error bars' curve not among them may be gone (its layer was
    // rebuilt by the graph's Undo / Redo): its address is only compared, never followed.
    QSet<QwtPlotItem *> held;
    for (const TakenCurve &t : tc.curves) {
        if (t.item)
            held.insert(t.item);
    }
    // held curves the layer has again (deleted at the end, after their error bars are seen to)
    QList<QwtPlotItem *> recreated;
    QList<QPair<int, QwtPlotItem *>> restored; // key, item
    // keys a curve could not get back (taken by another item): its error bars find it under the
    // new one
    QHash<int, int> rekeyed;
    // the legend as the take left it, with as many curves: if every curve goes back to its
    // place, the legend text from before the take is put back as it was
    Legend *legNow = (legendMarkerID >= 0) ? (Legend *)d_plot->marker(legendMarkerID) : nullptr;
    bool legendAsBefore = legNow && tc.curvesAfter >= 0 && n_curves == tc.curvesAfter
            && legNow->text() == tc.legendAfter;
    // the curves first, then the error bars, which need their curve back in the layer
    for (int pass = 0; pass < 2; ++pass) {
        for (TakenCurve &t : tc.curves) {
            if (!t.item || (pass == 0) == (t.type == ErrorBars))
                continue;
            QwtErrorPlotCurve *er = (t.type == ErrorBars)
                    ? dynamic_cast<QwtErrorPlotCurve *>(t.item)
                    : nullptr;
            DataCurve *was = er ? er->masterCurve() : nullptr;
            auto unlinkFromHeldCurve = [&]() {
                if (was && held.contains(was))
                    was->removeErrorBars(er);
            };
            // A curve under its own key is in the layer again: the graph's Undo / Redo rebuilt
            // the layer with it (a rebuilt layer keeps the keys, and a key is never given to
            // another curve). That one stays (and reads its columns below); the held copy goes.
            if (QwtPlotItem *here = d_plot->plotItem(t.key)) {
                legendAsBefore = false;
                unlinkFromHeldCurve();
                recreated << t.item;
                t.item = nullptr;
                restored << qMakePair(t.key, here);
                continue;
            }
            // Its curve is the one under the key it hung on: the very one, or the same curve in
            // a layer rebuilt meanwhile. The address alone is not enough, since a curve deleted
            // meanwhile can leave it to a new one, which has a new key.
            DataCurve *master = nullptr;
            if (er) {
                t.masterKey = rekeyed.value(t.masterKey, t.masterKey);
                QwtPlotItem *mi = (t.masterKey >= 0) ? d_plot->plotItem(t.masterKey) : nullptr;
                const int mIdx = mi ? c_keys.indexOf(t.masterKey) : -1;
                if (mIdx >= 0 && mIdx < n_curves && c_type[mIdx] != ErrorBars)
                    master = dynamic_cast<DataCurve *>(mi);
            }
            if (t.type == ErrorBars && (!er || !master)) {
                // its curve is no longer in this layer: nothing to attach the error bars to
                legendAsBefore = false;
                unlinkFromHeldCurve();
                delete t.item;
                t.item = nullptr;
                continue;
            }
            if (er && was != master) {
                unlinkFromHeldCurve();
                er->setMasterCurve(master); // the same curve, rebuilt: hang on it
            }

            // after the item that preceded it when it was taken, if that one is still here
            // (keys are never given to another item, so a key found is that same item)
            int pos = 0;
            for (int p = tc.order.indexOf(t.key) - 1; p >= 0; --p) {
                const int k = tc.order.at(p);
                const int cur = d_plot->plotItem(k) ? c_keys.indexOf(k) : -1;
                if (cur >= 0 && cur < n_curves) {
                    pos = cur + 1;
                    break;
                }
            }
            pos = qBound(0, pos, n_curves);
            legendAsBefore = legendAsBefore && pos == t.index;

            // shift the curve numbers in the legend and the per-index state from pos on
            Legend *leg =
                    (legendMarkerID >= 0) ? (Legend *)d_plot->marker(legendMarkerID) : nullptr;
            if (leg) {
                QVector<int> oldToNew(n_curves);
                for (int i = 0; i < n_curves; ++i)
                    oldToNew[i] = (i < pos) ? i : i + 1;
                QString text = remapLegendCurveTokens(leg->text(), oldToNew);
                if (!t.legendLine.isEmpty()) {
                    QString line = t.legendLine;
                    line.replace("\\c{" + QString::number(t.index + 1) + "}",
                                 "\\c{" + QString::number(pos + 1) + "}");
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                    QStringList lines = text.split("\n", Qt::SkipEmptyParts);
#else
                    QStringList lines = text.split("\n", QString::SkipEmptyParts);
#endif
                    int at = -1;
                    const QString prevTok = "\\c{" + QString::number(pos) + "}";
                    const QString nextTok = "\\c{" + QString::number(pos + 2) + "}";
                    for (int li = 0; pos > 0 && li < lines.size() && at < 0; ++li) {
                        if (lines[li].contains(prevTok))
                            at = li + 1;
                    }
                    for (int li = 0; li < lines.size() && at < 0; ++li) {
                        if (lines[li].contains(nextTok))
                            at = li;
                    }
                    if (at < 0)
                        at = lines.size();
                    lines.insert(at, line);
                    text = lines.join("\n") + "\n";
                }
                if (text != leg->text())
                    leg->setText(text);
            }
            {
                QMap<int, QString> shiftedLabels;
                for (auto lit = d_last_auto_curve_labels.constBegin();
                     lit != d_last_auto_curve_labels.constEnd(); ++lit)
                    shiftedLabels.insert(lit.key() < pos ? lit.key() : lit.key() + 1,
                                         lit.value());
                d_last_auto_curve_labels = shiftedLabels;
            }
            if ((d_selection.kind == GraphSelection::Curve
                 || d_selection.kind == GraphSelection::ErrorBars
                 || d_selection.kind == GraphSelection::Spectrogram)
                && d_selection.primaryId >= pos)
                d_selection.primaryId++;

            // back under its own key, so that it is known by that key again (retakeCurves(),
            // the error bars of other taken sets that hang on it)
            const int key = d_plot->insertCurve(t.item, t.key);
            if (key != t.key)
                rekeyed.insert(t.key, key);
            t.key = key;
            c_keys.insert(pos, key);
            c_type.insert(pos, t.type);
            ++n_curves;
            if (t.fitCurve)
                if (QwtPlotCurve *fc = dynamic_cast<QwtPlotCurve *>(t.item))
                    d_fit_curves << fc;
            if (er && !master->errorBarsList().contains(er))
                master->addErrorBars(er);
            restored << qMakePair(key, t.item);
        }
    }
    qDeleteAll(recreated);
    if (legendAsBefore && legendMarkerID >= 0 && d_plot->marker(legendMarkerID) == legNow
        && legNow->text() != tc.legendBefore)
        legNow->setText(tc.legendBefore);

    syncCurveZOrder();
    // The columns are back: read their present values. loadData() drops a curve whose column
    // is (still) missing, so each one is looked up again first.
    for (const auto &ri : restored) {
        QwtPlotItem *it = ri.second;
        if (curveIndexOfKey(ri.first, it) < 0)
            continue;
        if (QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(it))
            er->loadData();
        else if (DataCurve *dc = dynamic_cast<DataCurve *>(it)) {
            if (dc->type() != Function)
                dc->loadData();
        }
    }
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::retakeCurves(TakenCurves &tc)
{
    if (tc.out)
        return;
    QList<QwtPlotItem *> items;
    // The curve under the key it was put back with (or, for one the layer had again, the key
    // it had): the same item, or the same curve in a layer rebuilt by the graph's Undo / Redo,
    // which keeps the keys. One the user has deleted since may have left its address to a new
    // curve, which has a new key and stays.
    for (const TakenCurve &t : tc.curves) {
        QwtPlotItem *now = (t.key >= 0) ? d_plot->plotItem(t.key) : nullptr;
        const int idx = now ? c_keys.indexOf(t.key) : -1;
        if (idx < 0 || idx >= n_curves || items.contains(now))
            continue;
        if (now == t.item || c_type[idx] == t.type)
            items << now;
    }
    tc.curves.clear();
    if (!items.isEmpty())
        takeCurveItems(items, tc);
}

void Graph::discardTakenCurves(TakenCurves &tc)
{
    if (tc.out) {
        for (TakenCurve &t : tc.curves) {
            delete t.item;
            t.item = nullptr;
        }
    }
    tc.curves.clear();
    tc.out = false;
}

void Graph::removeCurves(const QString &s)
{
    for (int i = n_curves - 1; i >= 0; --i) {
        if (i >= n_curves)
            continue;
        if (curveUsesColumn(i, s))
            removeCurve(i);
    }
    d_plot->replot();
}

void Graph::removeCurve(const QString &s)
{
    removeCurve(plotItemsList().indexOf(s));
}

void Graph::removeCurve(int index)
{
    removeCurveAt(index, true);
}

QwtPlotItem *Graph::removeCurveAt(int index, bool deleteItem)
{
    if (index < 0 || index >= n_curves)
        return nullptr;

    QwtPlotItem *it = plotItem(index);
    if (!it)
        return nullptr;

    removeLegendItem(index);

    if (it->rtti() != QwtPlotItem::Rtti_PlotSpectrogram) {
        PlotCurve *pc = dynamic_cast<PlotCurve *>(it);
        if (!deleteItem) {
            // taken out whole: the caller keeps the error bar links (takeCurvesUsingColumns)
        } else if (pc && pc->type() == ErrorBars) {
            if (QwtErrorPlotCurve *epc = dynamic_cast<QwtErrorPlotCurve *>(pc)) {
                if (epc->masterCurve())
                    epc->detachFromMasterCurve();
            }
        } else if (pc && pc->type() != Function) {
            if (DataCurve *dc = dynamic_cast<DataCurve *>(pc))
                dc->clearErrorBars();
        }

        if (d_fit_curves.contains((QwtPlotCurve *)it)) {
            int i = d_fit_curves.indexOf((QwtPlotCurve *)it);
            if (i >= 0 && i < d_fit_curves.size())
                d_fit_curves.removeAt(i);
        }
    }

    // Re-resolve index in case clearErrorBars() removed items before it
    index = plotItemIndex(it);
    if (index < 0 || index >= n_curves)
        return nullptr;

    QwtPlotCurve *removedCurve = curve(index);
    if (d_active_tool) {
        if (DataPickerTool *dpt = dynamic_cast<DataPickerTool *>(d_active_tool)) {
            if (!removedCurve || dpt->selectedCurve() == removedCurve)
                setActiveTool(NULL);
        } else if (d_active_tool->rtti() == PlotToolInterface::TranslateCurve
                   || d_active_tool->rtti() == PlotToolInterface::MultiPeak) {
            setActiveTool(NULL);
        }
    }

    if (d_range_selector && (!removedCurve || removedCurve == d_range_selector->selectedCurve())) {
        QwtPlotCurve *replacement = nullptr;
        for (int delta = 1; delta < n_curves && !replacement; ++delta) {
            const int candIndices[2] = { index - delta, index + delta };
            for (int ci : candIndices) {
                if (ci < 0 || ci >= n_curves || ci == index)
                    continue;
                if (c_type[ci] == ErrorBars)
                    continue;
                QwtPlotCurve *cand = curve(ci);
                if (cand && cand->rtti() == QwtPlotItem::Rtti_PlotCurve && cand->dataSize() > 0) {
                    replacement = cand;
                    break;
                }
            }
        }
        if (replacement)
            d_range_selector->setSelectedCurve(replacement);
        else
            delete d_range_selector;
    }

    if (d_plot->highlightedCurveKey() == c_keys[index])
        d_plot->setHighlightedCurveKey(-1);

    QwtPlotItem *taken = nullptr;
    if (deleteItem)
        d_plot->removeCurve(c_keys[index]);
    else
        taken = d_plot->takeCurve(c_keys[index]);
    n_curves--;

    for (int i = index; i < n_curves; i++) {
        c_type[i] = c_type[i + 1];
        c_keys[i] = c_keys[i + 1];
    }
    c_type.resize(n_curves);
    c_keys.resize(n_curves);
    if (d_point_colorbar_active) // E11: drop the colour bar with its scatter curve
        syncPointColorBar();

    {
        QMap<int, QString> shiftedLabels;
        for (auto lit = d_last_auto_curve_labels.constBegin();
             lit != d_last_auto_curve_labels.constEnd(); ++lit) {
            if (lit.key() < index)
                shiftedLabels.insert(lit.key(), lit.value());
            else if (lit.key() > index)
                shiftedLabels.insert(lit.key() - 1, lit.value());
        }
        d_last_auto_curve_labels = shiftedLabels;
    }

    if (d_selection.kind == GraphSelection::Curve
        || d_selection.kind == GraphSelection::ErrorBars
        || d_selection.kind == GraphSelection::Spectrogram) {
        if (d_selection.primaryId == index)
            setCurrentSelection({ GraphSelection::None, -1, -1 });
        else if (d_selection.primaryId > index)
            d_selection.primaryId--;
    }

    syncCurveZOrder();
    emit modifiedGraph();
    return taken;
}

bool Graph::modifyFunctionCurve(ApplicationWindow *parent, int curve, int type,
                                const QStringList &formulas, const QString &var,
                                QList<double> &ranges, int points)
{
    FunctionCurve *c = (FunctionCurve *)this->curve(curve);
    if (!c)
        return false;

    QStringList effFormulas = formulas;
    if (type == FunctionCurve::Polar && effFormulas.size() == 1)
        effFormulas << (var.trimmed().isEmpty() ? QStringLiteral("t") : var.trimmed());

    if (c->functionType() == type && c->variable() == var && c->formulas() == effFormulas
        && c->startRange() == ranges[0] && c->endRange() == ranges[1] && c->dataSize() == points)
        return true;

    FunctionCurve backup(parent);
    backup.copy(c);

    c->setFunctionType((FunctionCurve::FunctionType)type);
    c->setRange(ranges[0], ranges[1]);
    c->setFormulas(effFormulas);
    c->setVariable(var);
    if (!c->loadData(points)) {
        c->copy(&backup);
        c->loadData(points);
        return false;
    }

    if (type == FunctionCurve::Polar) {
        double maxR = 0.0;
        for (int k = 0; k < c->dataSize(); ++k) {
            const double rk = std::hypot(c->x(k), c->y(k));
            if (std::isfinite(rk) && rk > maxR)
                maxR = rk;
        }
        setupPolarPlot(QVector<double>(), QVector<double>(), QString(), true);
        if (maxR > 0.0) {
            d_polar_rmax = qMax(d_polar_rmax, maxR);
            const double rBound = 1.32 * d_polar_rmax;
            setScale(QwtPlot::xBottom, -rBound, rBound, 0.0, 5, 0, 0, false);
            setScale(QwtPlot::yLeft, -rBound, rBound, 0.0, 5, 0, 0, false);
        }
    }

    if (legendMarkerID >= 0) { // update the legend marker
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk) {
            QString text = (mrk->text()).replace(backup.legend(), c->legend());
            mrk->setText(text);
        }
    }
    updatePlot();
    emit modifiedGraph();
    return true;
}

QString Graph::generateFunctionName(const QString &name)
{
    int index = 1;
    QString newName = name + QString::number(index);

    QStringList lst;
    for (int i = 0; i < n_curves; i++) {
        PlotCurve *c = (PlotCurve *)this->curve(i);
        if (!c)
            continue;

        if (c->type() == Function)
            lst << c->title().text();
    }

    while (lst.contains(newName)) {
        newName = name + QString::number(++index);
    }
    return newName;
}

bool Graph::addFunctionCurve(ApplicationWindow *parent, int type, const QStringList &formulas,
                             const QString &var, QList<double> &ranges, int points,
                             const QString &title)
{
    QString name;
    if (!title.isEmpty())
        name = title;
    else
        name = generateFunctionName();

    QStringList effFormulas = formulas;
    const QString effVar = var.trimmed().isEmpty() ? QStringLiteral("t") : var.trimmed();
    if (type == FunctionCurve::Polar && effFormulas.size() == 1)
        effFormulas << effVar;

    FunctionCurve *c = new FunctionCurve(parent, (FunctionCurve::FunctionType)type, name);
    QPen fPen(QColor(0, 114, 178), qMax(2, widthLine));
    fPen.setCosmetic(false);
    c->setPen(fPen);
    c->setRange(ranges[0], ranges[1]);
    c->setFormulas(effFormulas);
    c->setVariable(effVar);
    if (!c->loadData(points)) {
        delete c;
        return false;
    }

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = Function;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(c);

    if (type == FunctionCurve::Polar) {
        double maxR = 0.0;
        for (int k = 0; k < c->dataSize(); ++k) {
            const double rk = std::hypot(c->x(k), c->y(k));
            if (std::isfinite(rk) && rk > maxR)
                maxR = rk;
        }
        setupPolarPlot(QVector<double>(), QVector<double>(), QString(), true);
        if (maxR > 0.0) {
            double rLo = 0.0;
            double rHi = maxR;
            double rStep = 0.0;
            QwtLinearScaleEngine rse;
            rse.autoScale(4, rLo, rHi, rStep);
            d_polar_rmax = qMax(d_polar_rmax, (rHi >= maxR && std::isfinite(rHi)) ? rHi : maxR);
            const int extraRows = qMax(0, qMax(int(d_polar_series.size()), n_curves) - 1);
            const double yLo = -1.44 * d_polar_rmax;
            const double yHi = (1.64 + 0.36 * double(extraRows)) * d_polar_rmax;
            const double halfX = 0.5 * (yHi - yLo);
            setScale(QwtPlot::xBottom, -halfX, halfX, 0.0, 5, 0, 0, false);
            setScale(QwtPlot::yLeft, yLo, yHi, 0.0, 5, 0, 0, false);
        }
    }

    const QString legLabel = (type == FunctionCurve::Polar && !formulas.isEmpty())
            ? QStringLiteral("r(%1) = %2").arg(effVar, formulas[0].trimmed())
            : c->legend();
    addLegendItem(legLabel);
    if (type == FunctionCurve::Polar && hasLegend())
        placeLegendAnchor(LegendTopRight);
    if (parent) {
        auto ml = qobject_cast<MultiLayer *>(parent->d_workspace.activeSubWindow());
        if (!ml || ml->status() != MyWidget::Minimized) {
            updatePlot();
            emit modifiedGraph();
        }
    } else {
        updatePlot();
        emit modifiedGraph();
    }
    return true;
}

bool Graph::insertFunctionCurve(ApplicationWindow *parent, const QStringList &func_spec, int points,
                                int fileVersion)
{
    int type = FunctionCurve::Normal;
    QStringList formulas;
    QString var, name;
    QList<double> ranges;

    QStringList curve = func_spec[0].split(",");
    if (fileVersion >= 0x000105) {
        // SciDAVis 0.1.4 and 0.2.0 crash when trying to save a function curve;
        // thus, it's safe to assume every version from 0.1.5 on uses the new format,
        // even though 0.2.0 doesn't actually contain the revised code yet
        if (curve.size() < 5)
            return false;
        type = curve[0].toInt();
        name = curve.mid(1, curve.size() - 4).join(",");
        var = curve[curve.size() - 3];
        ranges += curve[curve.size() - 2].toDouble();
        ranges += curve[curve.size() - 1].toDouble();

        if (func_spec.size() > 1)
            formulas << func_spec[1];
        else
            formulas << QString();
        if (type != FunctionCurve::Normal)
            formulas << (func_spec.size() > 2 ? func_spec[2] : QString());

        FunctionCurve *c = new FunctionCurve(parent, (FunctionCurve::FunctionType)type, name);
        c->setSuppressScriptErrors(true);
        c->setPen(QPen(QColor(Qt::black), widthLine));
        c->setRange(ranges[0], ranges[1]);
        c->setFormulas(formulas);
        c->setVariable(var);
        if (!c->loadData(points)) {
            delete c;
            return false;
        }
        c->setSuppressScriptErrors(false);

        c_type.resize(++n_curves);
        c_type[n_curves - 1] = Line;

        c_keys.resize(n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(c);

        addLegendItem(c->legend());
        auto ml = parent ? qobject_cast<MultiLayer *>(parent->d_workspace.activeSubWindow()) : nullptr;
        if (ml && ml->status() != MyWidget::Minimized) {
            updatePlot();
            emit modifiedGraph();
        }
        return true;

    } else if (fileVersion < 87) {
        if (curve[0][0] == 'f') {
            type = FunctionCurve::Normal;
            formulas += curve[0].section('=', 1, 1);
            var = curve[1];
            ranges += curve[2].toDouble();
            ranges += curve[3].toDouble();
        } else if (curve[0][0] == 'X') {
            type = FunctionCurve::Parametric;
            formulas += curve[0].section('=', 1, 1);
            formulas += curve[1].section('=', 1, 1);
            var = curve[2];
            ranges += curve[3].toDouble();
            ranges += curve[4].toDouble();
        } else if (curve[0][0] == 'R') {
            type = FunctionCurve::Polar;
            formulas += curve[0].section('=', 1, 1);
            formulas += curve[1].section('=', 1, 1);
            var = curve[2];
            ranges += curve[3].toDouble();
            ranges += curve[4].toDouble();
        }
    } else {
        type = curve[0].toInt();
        name = curve[1];

        if (type == FunctionCurve::Normal) {
            formulas << curve[2];
            var = curve[3];
            ranges += curve[4].toDouble();
            ranges += curve[5].toDouble();
        } else if (type == FunctionCurve::Polar || type == FunctionCurve::Parametric) {
            formulas << curve[2];
            formulas << curve[3];
            var = curve[4];
            ranges += curve[5].toDouble();
            ranges += curve[6].toDouble();
        }
    }
    return addFunctionCurve(parent, type, formulas, var, ranges, points, name);
}

Qt::BrushStyle Graph::getBrushStyle(int style)
{
    Qt::BrushStyle brushStyle;
    switch (style) {
    case 0:
        brushStyle = Qt::SolidPattern;
        break;
    case 1:
        brushStyle = Qt::HorPattern;
        break;
    case 2:
        brushStyle = Qt::VerPattern;
        break;
    case 3:
        brushStyle = Qt::CrossPattern;
        break;
    case 4:
        brushStyle = Qt::BDiagPattern;
        break;
    case 5:
        brushStyle = Qt::FDiagPattern;
        break;
    case 6:
        brushStyle = Qt::DiagCrossPattern;
        break;
    case 7:
        brushStyle = Qt::Dense1Pattern;
        break;
    case 8:
        brushStyle = Qt::Dense2Pattern;
        break;
    case 9:
        brushStyle = Qt::Dense3Pattern;
        break;
    case 10:
        brushStyle = Qt::Dense4Pattern;
        break;
    case 11:
        brushStyle = Qt::Dense5Pattern;
        break;
    case 12:
        brushStyle = Qt::Dense6Pattern;
        break;
    case 13:
        brushStyle = Qt::Dense7Pattern;
        break;
    default:
        throw runtime_error("invalid brush style");
    }
    return brushStyle;
}

QString Graph::penStyleName(Qt::PenStyle style)
{
    if (style == Qt::SolidLine)
        return "SolidLine";
    else if (style == Qt::DashLine)
        return "DashLine";
    else if (style == Qt::DotLine)
        return "DotLine";
    else if (style == Qt::DashDotLine)
        return "DashDotLine";
    else if (style == Qt::DashDotDotLine)
        return "DashDotDotLine";
    else if (style == Qt::CustomDashLine)
        return "CustomDashLine";
    else
        return "SolidLine";
}

Qt::PenStyle Graph::getPenStyle(int style)
{
    Qt::PenStyle linePen;
    switch (style) {
    case 0:
        linePen = Qt::SolidLine;
        break;
    case 1:
        linePen = Qt::DashLine;
        break;
    case 2:
        linePen = Qt::DotLine;
        break;
    case 3:
        linePen = Qt::DashDotLine;
        break;
    case 4:
        linePen = Qt::DashDotDotLine;
        break;
    case 5:
        linePen = Qt::CustomDashLine;
        break;
    default:
        throw runtime_error("invalid pen style");
    }
    return linePen;
}

Qt::PenStyle Graph::getPenStyle(const QString &s)
{
    Qt::PenStyle style = Qt::SolidLine;
    if (s == "SolidLine")
        style = Qt::SolidLine;
    else if (s == "DashLine")
        style = Qt::DashLine;
    else if (s == "DotLine")
        style = Qt::DotLine;
    else if (s == "DashDotLine")
        style = Qt::DashDotLine;
    else if (s == "DashDotDotLine")
        style = Qt::DashDotDotLine;
    else if (s == "CustomDashLine")
        style = Qt::CustomDashLine;
    return style;
}

int Graph::obsoleteSymbolStyle(int type)
{
    if (type <= 4)
        return type + 1;
    else
        return type + 2;
}

int Graph::curveType(int curveIndex)
{
    if (curveIndex < (int)c_type.size() && curveIndex >= 0)
        return c_type[curveIndex];
    else
        return -1;
}

void Graph::plotBoxDiagram(Table *w, const QStringList &names, int startRow, int endRow)
{
    if (!w || names.isEmpty())
        return;
    if (endRow < 0 || endRow >= w->numRows())
        endRow = w->numRows() - 1;

    const QList<QColor> boxPalette = paletteColors(defaultCurvePaletteName());

    const QString tblPrefix = QString(w->name()) + QLatin1Char('_');
    QStringList catLabels;
    catLabels.reserve(names.count());

    int plottedOrd = 0;
    for (int j = 0; j < (int)names.count(); j++) {
        Column *colPtr = w->column(names[j]);
        if (!colPtr && names[j].startsWith(tblPrefix))
            colPtr = w->column(names[j].mid(tblPrefix.size()));

        BoxCurve *c = new BoxCurve(w, names[j], startRow, endRow);
        c->setData(QwtSingleArrayData(double(plottedOrd + 1), QwtArray<double>(), 0));
        c->loadData();
        if (c->dataSize() <= 0) {
            delete c;
            continue;
        }
        // A box is drawn by BoxCurve::draw, not as connected points: the same UserCurve style
        // that updateCurveLayout gives it on reopen (the Qwt default Lines was saved as "1" and
        // came back as "100", so the first and second save of a project differed).
        c->setStyle(QwtPlotCurve::UserCurve);

        c_keys.resize(++n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(c);
        c_type.resize(n_curves);
        c_type[n_curves - 1] = Box;

        const QColor col = boxPalette.isEmpty() ? QColor(Qt::black)
                                                : boxPalette.at(plottedOrd % boxPalette.size());
        QPen boxPen(col.darker(120), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
        boxPen.setCosmetic(false);
        c->setPen(boxPen);
        QColor fillTint = col;
        fillTint.setAlpha(75);
        c->setBrush(QBrush(fillTint, Qt::SolidPattern));
        QPen symPen(col.darker(135), 1.4, Qt::SolidLine);
        symPen.setCosmetic(false);
        c->setSymbol(QwtSymbol(QwtSymbol::NoSymbol, QBrush(Qt::white, Qt::SolidPattern), symPen,
                               QSize(8, 8)));

        QString catName;
        if (colPtr) {
            catName = firstNonEmptyCommentLine(colPtr->comment());
            if (catName.isEmpty())
                catName = colPtr->name().trimmed();
        }
        if (catName.isEmpty()) {
            catName = names[j];
            if (catName.startsWith(tblPrefix))
                catName = catName.mid(tblPrefix.size());
        }
        catLabels << catName;
        ++plottedOrd;
    }

    if (catLabels.isEmpty())
        return;

    syncCurveZOrder();
    setCategoryLabels(QwtPlot::xBottom, catLabels);
    if (QwtScaleDraw *topSd = d_plot->axisScaleDraw(QwtPlot::xTop))
        topSd->enableComponent(QwtAbstractScaleDraw::Labels, false);
    d_plot->setMajorTicksType(QwtPlot::xTop, ScaleDraw::None);
    d_plot->setMinorTicksType(QwtPlot::xTop, ScaleDraw::None);
    axesFormatInfo[QwtPlot::xBottom] = w->name();
    axesFormatInfo[QwtPlot::xTop] = w->name();

    {
        const QString curBotPlain = unwrapNoWrapAxisTitle(axisTitle(QwtPlot::xBottom)).trimmed();
        if (isAxisPlaceholderTitle(curBotPlain)) {
            QwtText emptyT = d_plot->axisTitle(QwtPlot::xBottom);
            emptyT.setText(QString());
            d_plot->setAxisTitle(QwtPlot::xBottom, emptyT);
        }
    }
    if (!d_refreshing_auto_titles)
        refreshAutoAxisTitlesFromColumns();

    Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
    if (mrk)
        mrk->setText(legendText());
}

void Graph::setCurveStyle(int index, int s)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    if (s == 5) // ancient spline style in Qwt 4.2.0
    {
        s = QwtPlotCurve::Lines;
        c->setCurveAttribute(QwtPlotCurve::Fitted, true);
        c_type[index] = Spline;
    } else if (s == QwtPlotCurve::Lines)
        c->setCurveAttribute(QwtPlotCurve::Fitted, false);
    else if (s == 6) // Vertical Steps
    {
        s = QwtPlotCurve::Steps;
        c->setCurveAttribute(QwtPlotCurve::Inverted, true);
        c_type[index] = VerticalSteps;
    } else if (s == QwtPlotCurve::Steps) // Horizontal Steps
    {
        c->setCurveAttribute(QwtPlotCurve::Inverted, false);
        c_type[index] = HorizontalSteps;
    }
    c->setStyle((QwtPlotCurve::CurveStyle)s);
}

void Graph::setCurveSymbol(int index, const QwtSymbol &s)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    c->setSymbol(s);
}

void Graph::setCurvePen(int index, const QPen &p)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    c->setPen(p);
}

void Graph::setCurveBrush(int index, const QBrush &b)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    c->setBrush(b);
}

void Graph::openBoxDiagram(Table *w, const QStringList &l, int fileVersion)
{
    if (!w)
        return;

    int s_offset = 0;
    int startRow = 0;
    int endRow = w->numRows() - 1;
    if (fileVersion >= 90) {
        startRow = l[l.count() - 3].toInt();
        endRow = l[l.count() - 2].toInt();
    }

    BoxCurve *c = new BoxCurve(w, l[2], startRow, endRow);
    c->setData(QwtSingleArrayData(l[1].toDouble(), QwtArray<double>(), 0));
    c->setData(QwtSingleArrayData(l[1].toDouble(), QwtArray<double>(), 0));
    c->loadData();

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(c);
    c_type.resize(n_curves);
    c_type[n_curves - 1] = Box;
    // The category axis of a box plot refers to the box table, as plotBoxDiagram sets it. The
    // AxisType line keeps it ("1;<table>"), but its reader only takes "<table>_<column>", so the
    // reference was lost on reopen and the next save wrote "1;".
    axesFormatInfo[QwtPlot::xBottom] = w->name();
    axesFormatInfo[QwtPlot::xTop] = w->name();

    if (fileVersion >= 0x011800) // 1.24.0
    {
        s_offset = 3;
    }
    c->setMaxStyle(SymbolBox::style(l[16 + s_offset].toInt()));
    c->setP99Style(SymbolBox::style(l[17 + s_offset].toInt()));
    c->setMeanStyle(SymbolBox::style(l[18 + s_offset].toInt()));
    c->setP1Style(SymbolBox::style(l[19 + s_offset].toInt()));
    c->setMinStyle(SymbolBox::style(l[20 + s_offset].toInt()));

    c->setBoxStyle(l[21 + s_offset].toInt());
    c->setBoxWidth(l[22 + s_offset].toInt());
    c->setBoxRange(l[23 + s_offset].toInt(), l[24 + s_offset].toDouble());
    c->setWhiskersRange(l[25 + s_offset].toInt(), l[26 + s_offset].toDouble());
    if (l.count() > 33 + s_offset) {
        c->setViolinMode(l[27 + s_offset].toInt() != 0);
        c->setShowDataPoints(l[28 + s_offset].toInt() != 0);
    }
}

void Graph::guessUniqueCurveLayout(int &colorIndex, int &symbolIndex)
{
    // New curves follow the configured default palette (ToriGraph Standard unless changed in
    // the preset manager) together with Origin's symbol-shape increment.
    const QList<QColor> palette = paletteColors(defaultCurvePaletteName());
    const int kPaletteCount = qMax(1, palette.size());

    int curve_index = n_curves - 1;
    if (curve_index >= 0 && curve_index < c_type.size()
        && c_type[curve_index] == ErrorBars) { // find out the pen color of the master curve
        QwtErrorPlotCurve *er =
                dynamic_cast<QwtErrorPlotCurve *>(d_plot->curve(c_keys[curve_index]));
        DataCurve *master_curve = er ? er->masterCurve() : nullptr;
        if (master_curve) {
            colorIndex = ColorButton::colorIndex(master_curve->pen().color());
            symbolIndex = 1;
            return;
        }
    }

    int dataCurves = 0;
    for (int i = 0; i < n_curves; i++) {
        const QwtPlotCurve *c = curve(i);
        if (c && c->rtti() == QwtPlotItem::Rtti_PlotCurve && c_type[i] != ErrorBars)
            dataCurves++;
    }
    const int seriesIdx = qMax(0, dataCurves - 1);
    const QColor col = palette.isEmpty() ? QColor(Qt::black) : palette.at(seriesIdx % kPaletteCount);
    colorIndex = int(col.rgba());
    symbolIndex = defaultCurveSymbolIndex(seriesIdx);
}

void Graph::addFitCurve(QwtPlotCurve *c)
{
    if (c)
        d_fit_curves << c;
}

void Graph::deleteFitCurves()
{
    const QList<QwtPlotCurve *> fitCopy = d_fit_curves;
    for (QwtPlotCurve *c : fitCopy)
        removeCurve(curveIndex(c));

    d_plot->replot();
}

void Graph::plotSpectrogram(Matrix *m, CurveType type)
{
    if (type != GrayMap && type != ColorMap && type != ContourMap)
        return;

    Spectrogram *d_spectrogram = new Spectrogram(m);
    if (type == GrayMap)
        d_spectrogram->setGrayScale();
    else if (type == ContourMap) {
        d_spectrogram->setDisplayMode(QwtPlotSpectrogram::ImageMode, false);
        d_spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, true);
    } else if (type == ColorMap) {
        // New heatmaps default to perceptually uniform Viridis (the legacy rainbow map is still
        // selectable and is restored for projects that saved it).
        d_spectrogram->setColorMapPreset(Spectrogram::PresetViridis, false);
        d_spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, false);
    }

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(d_spectrogram);

    c_type.resize(n_curves);
    c_type[n_curves - 1] = type;
    syncCurveZOrder();

    QwtScaleWidget *rightAxis = d_plot->axisWidget(QwtPlot::yRight);
    rightAxis->setColorBarEnabled(type != ContourMap);
    rightAxis->setColorMap(d_spectrogram->effectiveValueRange(), d_spectrogram->colorMap());

    d_plot->setAxisScale(QwtPlot::xBottom, m->xStart(), m->xEnd());
    d_plot->setAxisScale(QwtPlot::yLeft, m->yStart(), m->yEnd());

    d_plot->setAxisScale(QwtPlot::yRight, d_spectrogram->effectiveValueRange().minValue(),
                         d_spectrogram->effectiveValueRange().maxValue());
    d_plot->enableAxis(QwtPlot::yRight, type != ContourMap);

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        QFont tf = d_plot->axisFont(a);
        tf.setFamily(QStringLiteral("Arial"));
        tf.setBold(false);
        tf.setHintingPreference(QFont::PreferNoHinting);
        d_plot->setAxisFont(a, tf);
        QwtText tt = d_plot->axisTitle(a);
        QFont ttf = tt.font();
        ttf.setFamily(QStringLiteral("Arial"));
        ttf.setBold(false);
        ttf.setHintingPreference(QFont::PreferNoHinting);
        tt.setFont(ttf);
        const QString plainT = unwrapNoWrapAxisTitle(tt.text()).trimmed();
        if (isAxisPlaceholderTitle(plainT))
            tt.setText(QString());
        d_plot->setAxisTitle(a, tt);
    }
    d_spectrogram->enforceColorBarArialFont();

    d_plot->replot();
}

void Graph::restoreSpectrogram(ApplicationWindow *app, const QStringList &lst)
{
    // An empty <spectrogram> block has no matrix line to restore from: flag the load as
    // truncated (same mechanism as openTable/openMatrix) instead of dereferencing lst.end().
    if (lst.isEmpty()) {
        if (app)
            app->setProperty("_tg_load_truncated", true);
        return;
    }
    QStringList::const_iterator line = lst.begin();
    QString s = (*line).trimmed();
    QString matrixName = s.remove("<matrix>").remove("</matrix>");
    Matrix *m = app->matrix(matrixName);
    if (!m)
        return;

    Spectrogram *sp = new Spectrogram(m);

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = Graph::ColorMap;
    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(sp);
    syncCurveZOrder();

    bool hasPresetTag = false;
    Spectrogram::ColorMapPreset loadedPreset = Spectrogram::PresetLegacyDefault;
    bool revPalette = false;
    bool hasCustomZ = false;
    double zMin = 0.0, zMax = 1.0;
    int pendingLevels = -1;
    bool hasColorBar = false;
    int colorBarAxis = QwtPlot::yRight;
    int colorBarW = 16;
    QString cbTitle;
    bool maskInvalid = false, logColor = false, cellBlocks = false; // E11b

    for (line++; line != lst.end(); line++) {
        QString s = *line;
        if (s.contains("<ToriMaskInvalid>")) {
            maskInvalid = s.remove("<ToriMaskInvalid>").remove("</ToriMaskInvalid>").trimmed().toInt() != 0;
        } else if (s.contains("<ToriLogColor>")) {
            logColor = s.remove("<ToriLogColor>").remove("</ToriLogColor>").trimmed().toInt() != 0;
        } else if (s.contains("<ToriCellBlocks>")) {
            cellBlocks = s.remove("<ToriCellBlocks>").remove("</ToriCellBlocks>").trimmed().toInt() != 0;
        } else if (s.contains("<ColorPolicy>")) {
            int color_policy = s.remove("<ColorPolicy>").remove("</ColorPolicy>").trimmed().toInt();
            if (color_policy == Spectrogram::GrayScale)
                sp->setGrayScale();
            else if (color_policy == Spectrogram::Default)
                sp->setDefaultColorMap();
        } else if (s.contains("<ColorMap>")) {
            s = *(++line);
            int mode = s.remove("<Mode>").remove("</Mode>").trimmed().toInt();
            s = *(++line);
            QColor color1 =
                    QColor(COLORVALUE(s.remove("<MinColor>").remove("</MinColor>").trimmed()));
            s = *(++line);
            QColor color2 =
                    QColor(COLORVALUE(s.remove("<MaxColor>").remove("</MaxColor>").trimmed()));

            QwtLinearColorMap colorMap = QwtLinearColorMap(color1, color2);
            colorMap.setMode((QwtLinearColorMap::Mode)mode);

            s = *(++line);
            int stops = s.remove("<ColorStops>").remove("</ColorStops>").trimmed().toInt();
            for (int i = 0; i < stops; i++) {
                s = (*(++line)).trimmed();
                QStringList l = s.remove("<Stop>").remove("</Stop>").split("\t");
                colorMap.addColorStop(l[0].toDouble(), QColor(COLORVALUE(l[1])));
            }
            sp->setCustomColorMap(colorMap);
            line++;
        } else if (s.contains("<Image>")) {
            int mode = s.remove("<Image>").remove("</Image>").trimmed().toInt();
            sp->setDisplayMode(QwtPlotSpectrogram::ImageMode, mode);
        } else if (s.contains("<ContourLines>")) {
            int contours = s.remove("<ContourLines>").remove("</ContourLines>").trimmed().toInt();
            sp->setDisplayMode(QwtPlotSpectrogram::ContourMode, contours);
            if (contours) {
                s = (*(++line)).trimmed();
                int levels = s.remove("<Levels>").remove("</Levels>").toInt();
                pendingLevels = levels;
                sp->setLevelsNumber(levels);

                s = (*(++line)).trimmed();
                int defaultPen = s.remove("<DefaultPen>").remove("</DefaultPen>").toInt();
                if (!defaultPen)
                    sp->setDefaultContourPen(Qt::NoPen);
                else {
                    s = (*(++line)).trimmed();
                    QColor c = QColor(COLORVALUE(s.remove("<PenColor>").remove("</PenColor>")));
                    s = (*(++line)).trimmed();
                    int width = s.remove("<PenWidth>").remove("</PenWidth>").toInt();
                    s = (*(++line)).trimmed();
                    int style = s.remove("<PenStyle>").remove("</PenStyle>").toInt();
                    sp->setDefaultContourPen(QPen(c, width, Graph::getPenStyle(style)));
                }
            }
        } else if (s.contains("<ColorBar>")) {
            s = *(++line);
            colorBarAxis = s.remove("<axis>").remove("</axis>").trimmed().toInt();
            s = *(++line);
            colorBarW = s.remove("<width>").remove("</width>").trimmed().toInt();
            hasColorBar = true;
            line++;
        } else if (s.contains("<ColorPreset>")) {
            const int p = s.remove("<ColorPreset>").remove("</ColorPreset>").trimmed().toInt();
            if (p >= static_cast<int>(Spectrogram::PresetLegacyDefault)
                && p <= static_cast<int>(Spectrogram::PresetCustom)) {
                loadedPreset = static_cast<Spectrogram::ColorMapPreset>(p);
                hasPresetTag = true;
            }
        } else if (s.contains("<ReversePalette>")) {
            revPalette =
                    s.remove("<ReversePalette>").remove("</ReversePalette>").trimmed().toInt() != 0;
        } else if (s.contains("<CustomZRange>")) {
            const QStringList parts =
                    s.remove("<CustomZRange>").remove("</CustomZRange>").trimmed().split("\t");
            if (parts.size() >= 3 && parts[0].toInt() != 0) {
                hasCustomZ = true;
                zMin = parts[1].toDouble();
                zMax = parts[2].toDouble();
            }
        } else if (s.contains("<ColorBarTitle>")) {
            QString raw = s.trimmed();
            if (raw.startsWith("<ColorBarTitle>"))
                raw = raw.mid(15);
            if (raw.endsWith("</ColorBarTitle>"))
                raw.chop(16);
            raw.replace("&#10;", "\n")
                    .replace("&gt;", ">")
                    .replace("&lt;", "<")
                    .replace("&amp;", "&");
            cbTitle = raw;
        } else if (s.contains("<Visible>")) {
            int on = s.remove("<Visible>").remove("</Visible>").trimmed().toInt();
            sp->setVisible(on);
        }
    }

    if (maskInvalid)
        sp->setMaskInvalidCells(true);
    if (cellBlocks)
        sp->setCellBlocks(true);
    if (logColor)
        sp->setLogColorScale(true);
    if ((maskInvalid || cellBlocks || logColor) && !hasCustomZ && pendingLevels >= 2)
        sp->setLevelsNumber(pendingLevels);
    if (hasCustomZ) {
        sp->setCustomValueRange(true, zMin, zMax);
        if (pendingLevels >= 2)
            sp->setLevelsNumber(pendingLevels);
    }
    if (hasPresetTag)
        sp->setColorMapPreset(loadedPreset, revPalette);
    else if (revPalette)
        sp->setReversePalette(true);

    if (hasColorBar) {
        sp->showColorScale(colorBarAxis, true);
        sp->setColorBarWidth(colorBarW);
    } else {
        if (QwtScaleWidget *ca = d_plot->axisWidget(sp->colorScaleAxis()))
            ca->setColorBarEnabled(false);
    }
    if (!cbTitle.isEmpty())
        sp->setColorBarTitle(cbTitle);
    sp->syncColorScale();
    sp->enforceColorBarArialFont();
}

bool Graph::validCurvesDataSize() const
{
    if (!n_curves) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(const_cast<Graph *>(this), tr("Warning"),
                                 tr("There are no curves available on this plot!"));
        }
        return false;
    } else {
        for (int i = 0; i < n_curves; i++) {
            QwtPlotItem *item = curve(i);
            if (item && item->rtti() != QwtPlotItem::Rtti_PlotSpectrogram) {
                QwtPlotCurve *c = (QwtPlotCurve *)item;
                if (c->dataSize() >= 2)
                    return true;
            }
        }
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(const_cast<Graph *>(this), tr("Error"),
                                 tr("There are no curves with more than two points on this plot. "
                                    "Operation aborted!"));
        }
        return false;
    }
}

void Graph::updateCurveNames(const QString &oldName, const QString &newName, bool updateTableName)
{
    if (oldName.isEmpty() || oldName == newName)
        return;

    struct ChangedCurveInfo
    {
        int curveIdx = -1;
        QString prevTitle;
        QString nextTitle;
        QString oldShort;
        QString newShort;
        QString colLong;
    };
    QHash<int, ChangedCurveInfo> changedByOneBased;
    QList<ChangedCurveInfo> changedList;

    // update plotted curves list
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it || it->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;

        DataCurve *c = dynamic_cast<DataCurve *>(it);
        if (!c)
            continue;

        const QString prevTitle = c->title().text();
        c->updateColumnNames(oldName, newName, updateTableName);
        const QString nextTitle = c->title().text();
        if (prevTitle != nextTitle) {
            ChangedCurveInfo info;
            info.curveIdx = curveIndex(c);
            info.prevTitle = prevTitle;
            info.nextTitle = nextTitle;
            if (Table *tbl = c->table()) {
                if (!updateTableName) {
                    const QString tblPrefix = QString(tbl->name()) + QLatin1Char('_');
                    if (prevTitle.startsWith(tblPrefix))
                        info.oldShort = prevTitle.mid(tblPrefix.size());
                    if (nextTitle.startsWith(tblPrefix))
                        info.newShort = nextTitle.mid(tblPrefix.size());
                }
                const int yIdx = tbl->colIndex(nextTitle);
                if (Column *yc = (yIdx >= 0 ? tbl->column(yIdx) : nullptr))
                    info.colLong = yc->longName().trimmed();
            }
            changedList.append(info);
            if (info.curveIdx >= 0)
                changedByOneBased.insert(info.curveIdx + 1, info);
        }
    }

    const bool extraTagsChanged =
            ExtraPlotTypes::updateGraphTagsForRename(this, oldName, newName, updateTableName, false);

    if (Legend *mrk = legend()) {
        auto isIdentChar = [](QChar ch) -> bool {
            return ch.isLetterOrNumber() || ch == QLatin1Char('_');
        };
        auto replaceExactToken = [&](const QString &src, const QString &fromTok,
                                     const QString &toTok) -> QString {
            if (fromTok.isEmpty() || fromTok == toTok)
                return src;
            QString out;
            out.reserve(src.size());
            int pos = 0;
            while (pos < src.size()) {
                const int hit = src.indexOf(fromTok, pos);
                if (hit < 0) {
                    out.append(src.mid(pos));
                    break;
                }
                const bool leftBoundary = (hit == 0) || !isIdentChar(src.at(hit - 1));
                const int after = hit + fromTok.size();
                const bool rightBoundary = (after >= src.size()) || !isIdentChar(src.at(after));
                if (leftBoundary && rightBoundary) {
                    out.append(src.mid(pos, hit - pos));
                    out.append(toTok);
                    pos = after;
                } else {
                    out.append(src.mid(pos, (hit - pos) + 1));
                    pos = hit + 1;
                }
            }
            return out;
        };

        const QString origText = mrk->text();
        const bool trailingNewline = origText.endsWith(QLatin1Char('\n'));
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList lst = origText.split("\n", Qt::SkipEmptyParts);
#else
        QStringList lst = origText.split("\n", QString::SkipEmptyParts);
#endif
        bool anyLegendChanged = false;
        for (int li = 0; li < lst.size(); ++li) {
            const QString line = lst[li];
            int prefixEnd = -1;
            int oneBased = -1;

            const int cPos = line.indexOf(QStringLiteral("\\c{"));
            if (cPos >= 0 && line.left(cPos).trimmed().isEmpty()) {
                const int closeBrace = line.indexOf(QLatin1Char('}'), cPos + 3);
                if (closeBrace > cPos + 3) {
                    bool ok = false;
                    const int parsed = line.mid(cPos + 3, closeBrace - (cPos + 3)).toInt(&ok);
                    if (ok && parsed >= 1) {
                        oneBased = parsed;
                        prefixEnd = closeBrace + 1;
                    }
                }
            } else {
                const int lPos = line.indexOf(QStringLiteral("\\l("));
                if (lPos >= 0 && line.left(lPos).trimmed().isEmpty()) {
                    const int closeParen = line.indexOf(QLatin1Char(')'), lPos + 3);
                    if (closeParen > lPos + 3) {
                        bool ok = false;
                        const int parsed = line.mid(lPos + 3, closeParen - (lPos + 3)).toInt(&ok);
                        if (ok && parsed >= 1) {
                            oneBased = parsed;
                            prefixEnd = closeParen + 1;
                        }
                    }
                }
            }

            if (oneBased >= 1 && prefixEnd > 0) {
                if (!changedByOneBased.contains(oneBased))
                    continue;

                const ChangedCurveInfo &info = changedByOneBased.value(oneBased);
                const QString cmdPrefix = line.left(prefixEnd);
                const QString rest = line.mid(prefixEnd);
                QString newRest = rest;

                if (rest == info.prevTitle) {
                    newRest = info.nextTitle;
                } else if (!updateTableName && !info.oldShort.isEmpty() && rest == info.oldShort) {
                    newRest = !info.colLong.isEmpty() ? info.colLong : info.newShort;
                    if (info.curveIdx >= 0)
                        d_last_auto_curve_labels[info.curveIdx] = newRest;
                } else if (!updateTableName && !info.oldShort.isEmpty()
                           && rest.trimmed() == info.oldShort) {
                    const int idx = rest.indexOf(info.oldShort);
                    const QString target = !info.colLong.isEmpty() ? info.colLong : info.newShort;
                    newRest = rest.left(idx) + target + rest.mid(idx + info.oldShort.size());
                } else {
                    newRest = replaceExactToken(rest, info.prevTitle, info.nextTitle);
                }

                if (newRest != rest) {
                    lst[li] = cmdPrefix + newRest;
                    anyLegendChanged = true;
                }
            } else if (!changedList.isEmpty()) {
                QString updated = line;
                for (const ChangedCurveInfo &info : changedList)
                    updated = replaceExactToken(updated, info.prevTitle, info.nextTitle);
                if (updated != line) {
                    lst[li] = updated;
                    anyLegendChanged = true;
                }
            }
        }

        if (anyLegendChanged) {
            QString joined = lst.join("\n");
            if (trailingNewline && !joined.isEmpty())
                joined += QLatin1Char('\n');
            mrk->setText(joined);
        }
        if (anyLegendChanged || !changedList.isEmpty() || extraTagsChanged)
            d_plot->replot();
    } else if (!changedList.isEmpty() || extraTagsChanged) {
        d_plot->replot();
    }
}

void Graph::setCurveFullRange(int curveIndex)
{
    DataCurve *c = dynamic_cast<DataCurve *>(curve(curveIndex));
    if (c) {
        c->setFullRange();
        updatePlot();
        emit modifiedGraph();
    }
}

DataCurve *Graph::masterCurve(QwtErrorPlotCurve *er)
{
    if (!er || !er->masterCurve())
        return nullptr;
    for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); i++) {
        if (c_type[i] == ErrorBars)
            continue;
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it || it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(it);
        if (!pc || pc->type() == Function)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (dc && dc->plotAssociation() == er->masterCurve()->plotAssociation())
            return dc;
    }
    return nullptr;
}

DataCurve *Graph::masterCurve(const QString &xColName, const QString &yColName)
{
    QString master_curve = xColName + "(X)," + yColName + "(Y)";
    for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); i++) {
        if (c_type[i] == ErrorBars)
            continue;
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it || it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(it);
        if (!pc || pc->type() == Function)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (dc && dc->plotAssociation() == master_curve)
            return dc;
    }
    return nullptr;
}

bool Graph::isCurveVisible(int index) const
{
    if (index < 0 || index >= n_curves || index >= c_keys.size() || !d_plot)
        return false;
    const QwtPlotItem *it = d_plot->plotItem(c_keys[index]);
    return it ? it->isVisible() : false;
}

void Graph::showCurve(int index, bool visible)
{
    if (index < 0 || index >= n_curves)
        return;

    QwtPlotItem *it = plotItem(index);
    if (!it)
        return;

    it->setVisible(visible);

    if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
        Spectrogram *sp = static_cast<Spectrogram *>(it);
        if (QwtScaleWidget *sw = d_plot->axisWidget(sp->colorScaleAxis()))
            sw->setColorBarEnabled(visible && sp->hasColorScale());
    } else if (c_type[index] != ErrorBars && c_type[index] != Function) {
        if (DataCurve *dc = dynamic_cast<DataCurve *>(it)) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->setVisible(visible);
            }
        }
    }

    if (!visible && d_plot->highlightedCurveKey() == c_keys[index])
        d_plot->setHighlightedCurveKey(-1);

    updateStatReferenceLines();
    d_plot->replot();
    emit curveVisibilityChanged(index, visible);
    emit modifiedGraph();
}

int Graph::visibleCurves()
{
    int c = 0;
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (it && it->isVisible())
            c++;
    }
    return c;
}

// ============================================================================
// OriginPro Plot Types Support (Waterfall Y-Offset Stack & Ternary Diagram)
// ============================================================================

void Graph::applyYOffsetStack(double offsetStep, bool autoOffset)
{
    QList<QwtPlotCurve *> visibleDataCurves;
    for (int i = 0; i < n_curves; ++i) {
        if (c_type[i] == ErrorBars || c_type[i] == Pie || c_type[i] == Box
            || c_type[i] == ColorMap || c_type[i] == GrayMap || c_type[i] == ContourMap)
            continue;
        QwtPlotCurve *c = curve(i);
        if (c && c->isVisible() && c->dataSize() > 0)
            visibleDataCurves.append(c);
    }

    if (visibleDataCurves.size() < 2)
        return;

    double deltaY = offsetStep;
    if (autoOffset && qAbs(offsetStep) <= 1e-15) {
        double sumSpan = 0.0;
        int validSpans = 0;
        for (QwtPlotCurve *c : visibleDataCurves) {
            const int n = c->dataSize();
            if (n <= 0)
                continue;
            double minY = c->y(0);
            double maxY = c->y(0);
            for (int j = 1; j < n; ++j) {
                const double yv = c->y(j);
                if (yv < minY)
                    minY = yv;
                if (yv > maxY)
                    maxY = yv;
            }
            sumSpan += (maxY - minY);
            validSpans++;
        }
        const double avgSpan = (validSpans > 0) ? (sumSpan / double(validSpans)) : 0.0;
        deltaY = (avgSpan > 1e-12) ? (0.8 * avgSpan) : 1.0;
    }

    for (int k = 1; k < visibleDataCurves.size(); ++k) {
        QwtPlotCurve *c = visibleDataCurves[k];
        const int n = c->dataSize();
        const double yShift = double(k) * deltaY;
        QVector<double> xVals(n);
        QVector<double> yVals(n);
        for (int j = 0; j < n; ++j) {
            xVals[j] = c->x(j);
            yVals[j] = c->y(j) + yShift;
        }
        c->setData(xVals.constData(), yVals.constData(), n);

        DataCurve *dc = dynamic_cast<DataCurve *>(c);
        if (dc) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->loadData(); // follows the shifted points, keeps errors per table row
            }
        }
    }

    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        setToriAxisUserLocked(d_plot, axis, false);
        d_plot->setAxisAutoScale(axis);
    }
    updatePlot();

    {
        double overallMinY = 1e300;
        double overallMaxY = -1e300;
        for (QwtPlotCurve *c : visibleDataCurves) {
            const int n = c->dataSize();
            for (int j = 0; j < n; ++j) {
                const double yv = c->y(j);
                if (std::isfinite(yv)) {
                    overallMinY = qMin(overallMinY, yv);
                    overallMaxY = qMax(overallMaxY, yv);
                }
            }
        }
        const QwtScaleDiv *yDiv = d_plot->axisScaleDiv(QwtPlot::yLeft);
        const QwtScaleEngine *yEng = d_plot->axisScaleEngine(QwtPlot::yLeft);
        if (yDiv && yEng && !Plot::isLog10ScaleEngine(yEng)
            && !yEng->testAttribute(QwtScaleEngine::Inverted)
            && !d_axis_breaks[QwtPlot::yLeft].enabled && overallMaxY > 0.0
            && overallMinY >= -0.04 * overallMaxY) {
            const double autoLo = yDiv->lowerBound();
            const double autoHi = yDiv->upperBound();
            if (autoHi > 0.0 && autoLo < -0.035 * autoHi) {
                const double clampedLo = (overallMinY >= 0.0) ? 0.0 : (-0.03 * autoHi);
                const QwtValueList majTicks = yDiv->ticks(QwtScaleDiv::MajorTick);
                const double step = (majTicks.size() >= 2)
                        ? std::fabs(majTicks[1] - majTicks[0])
                        : d_user_step[QwtPlot::yLeft];
                const int maj = d_plot->axisMaxMajor(QwtPlot::yLeft);
                const int minT = d_plot->axisMaxMinor(QwtPlot::yLeft);
                const int maxMinInt = (minT >= 1) ? (minT + 1) : 0;
                QwtLinearScaleEngine *newEng = new QwtLinearScaleEngine();
                QwtScaleDiv newDiv = newEng->divideScale(clampedLo, autoHi, maj, maxMinInt, step);
                if (minT == 1)
                    ensureLinearHalfStepMinorTicks(newDiv);
                d_plot->setAxisScale(QwtPlot::yLeft, clampedLo, autoHi, step);
                d_plot->setAxisScaleEngine(QwtPlot::yLeft, newEng);
                d_plot->setAxisScaleDiv(QwtPlot::yLeft, newDiv);
                updateSecondaryAxis(QwtPlot::yRight);
                d_zoomer[0]->setZoomBase();
                d_plot->replot();
                syncChildLinkedLayers();
            }
        }
    }

    emit modifiedGraph();
}

namespace GraphInternal {

TernaryDiagramItem::TernaryDiagramItem(const Graph *graph) : d_graph(graph)
{
        setZ(12.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
}

int TernaryDiagramItem::rtti() const
{
    return QwtPlotItem::Rtti_PlotUserItem + 44;
}

void TernaryDiagramItem::draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                              const QRect &canvasRect) const
{
        if (!painter || !d_graph)
            return;

        const double h = std::sqrt(3.0) * 0.5;
        auto toScreen = [&](const QPointF &w) -> QPointF {
            return QPointF(xMap.xTransform(w.x()), yMap.xTransform(w.y()));
        };

        const QPointF pA = toScreen(QPointF(0.0, 0.0));
        const QPointF pB = toScreen(QPointF(1.0, 0.0));
        const QPointF pC = toScreen(QPointF(0.5, h));
        if (!std::isfinite(pA.x()) || !std::isfinite(pA.y()) || !std::isfinite(pB.x())
            || !std::isfinite(pB.y()) || !std::isfinite(pC.x()) || !std::isfinite(pC.y()))
            return;

        const double sc = Plot::printDeviceScale();
        QFont tickFont = d_graph->plotWidget()->displayFont(
                const_cast<Graph *>(d_graph)->axisFont(QwtPlot::xBottom));
        tickFont.setFamily(QStringLiteral("Arial"));
        tickFont.setBold(false);
        tickFont.setWeight(QFont::Normal);
        tickFont.setHintingPreference(QFont::PreferNoHinting);

        QFont vertFont = d_graph->plotWidget()->displayFont(
                const_cast<Graph *>(d_graph)->axisTitleFont(QwtPlot::xBottom));
        vertFont.setFamily(QStringLiteral("Arial"));
        vertFont.setBold(false);
        vertFont.setWeight(QFont::Normal);
        vertFont.setHintingPreference(QFont::PreferNoHinting);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        // 1. Interior 20% dashed grid lines
        QPen gridPen(QColor(184, 192, 204), 1.0 * sc, Qt::DashLine, Qt::FlatCap, Qt::MiterJoin);
        gridPen.setCosmetic(false);
        painter->setPen(gridPen);
        for (int step = 1; step <= 4; ++step) {
            const double f = 0.2 * double(step);
            // Constant-A line: (f, 1-f, 0) -> (f, 0, 1-f)
            painter->drawLine(toScreen(Graph::ternaryToCartesian(f, 1.0 - f, 0.0)),
                              toScreen(Graph::ternaryToCartesian(f, 0.0, 1.0 - f)));
            // Constant-B line: (1-f, f, 0) -> (0, f, 1-f)
            painter->drawLine(toScreen(Graph::ternaryToCartesian(1.0 - f, f, 0.0)),
                              toScreen(Graph::ternaryToCartesian(0.0, f, 1.0 - f)));
            // Constant-C line: (1-f, 0, f) -> (0, 1-f, f)
            painter->drawLine(toScreen(Graph::ternaryToCartesian(1.0 - f, 0.0, f)),
                              toScreen(Graph::ternaryToCartesian(0.0, 1.0 - f, f)));
        }

        // 2. Exterior ticks & thermodynamically consistent tick labels:
        //    - Base AB (left -> right): B% = 20, 40, 60, 80
        //    - Right BC (bottom -> top): C% = 20, 40, 60, 80
        //    - Left CA (bottom -> top): A% = 80, 60, 40, 20 (increases top -> bottom toward A=100% at vA!)
        const double baseLw = qMax(
                1.3,
                double(d_graph->plotWidget() ? d_graph->plotWidget()->axesLinewidth() : 2));
        QPen tickPen(QColor(Qt::black), baseLw * sc, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        tickPen.setCosmetic(false);

        const double tickLen = 6.5 * sc;
        const double padPx = 3.5 * sc;
        painter->setFont(tickFont);
        const QFontMetricsF tfm(tickFont, painter->device());
        const double tH = tfm.height();

        for (int step = 1; step <= 4; ++step) {
            const double f = 0.2 * double(step);
            const QString bText = QString::number(step * 20);
            const QString cText = QString::number(step * 20);
            const QString aText = QString::number(100 - step * 20);

            // Base AB tick (pointing down-left along constant-B or straight down)
            const QPointF basePt = toScreen(QPointF(f, 0.0));
            const QPointF baseTip(basePt.x(), basePt.y() + tickLen);
            painter->setPen(tickPen);
            painter->drawLine(basePt, baseTip);
            const double bw = tfm.horizontalAdvance(bText) + 4.0 * sc;
            painter->setPen(QColor(30, 41, 59));
            painter->drawText(QRectF(baseTip.x() - 0.5 * bw, baseTip.y() + padPx, bw, tH),
                              Qt::AlignHCenter | Qt::AlignTop | Qt::TextDontClip, bText);

            // Right BC tick (outward normal (+sqrt(3)/2, -0.5) in screen coords -> (+0.866, -0.5))
            const QPointF rightPt = toScreen(Graph::ternaryToCartesian(0.0, 1.0 - f, f));
            const QPointF rightTip(rightPt.x() + 0.8660254 * tickLen,
                                   rightPt.y() - 0.5 * tickLen);
            painter->setPen(tickPen);
            painter->drawLine(rightPt, rightTip);
            const double cw = tfm.horizontalAdvance(cText) + 4.0 * sc;
            painter->setPen(QColor(30, 41, 59));
            painter->drawText(QRectF(rightTip.x() + padPx, rightTip.y() - 0.55 * tH, cw, tH),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, cText);

            // Left CA tick (outward normal (-0.866, -0.5) in screen coords)
            const QPointF leftPt = toScreen(Graph::ternaryToCartesian(1.0 - f, 0.0, f));
            const QPointF leftTip(leftPt.x() - 0.8660254 * tickLen,
                                  leftPt.y() - 0.5 * tickLen);
            painter->setPen(tickPen);
            painter->drawLine(leftPt, leftTip);
            const double aw = tfm.horizontalAdvance(aText) + 4.0 * sc;
            painter->setPen(QColor(30, 41, 59));
            painter->drawText(QRectF(leftTip.x() - padPx - aw, leftTip.y() - 0.55 * tH, aw, tH),
                              Qt::AlignRight | Qt::AlignVCenter | Qt::TextDontClip, aText);
        }

        // 3. Equilateral triangle outer frame
        QPolygonF tri(3);
        tri[0] = pA;
        tri[1] = pB;
        tri[2] = pC;
        QPen borderPen(QColor(Qt::black), qMax(1.6, baseLw) * sc, Qt::SolidLine, Qt::SquareCap,
                       Qt::MiterJoin);
        borderPen.setCosmetic(false);
        painter->setPen(borderPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPolygon(tri);

        // 4. Non-bold Arial vertex component labels with font-metrics clearance
        painter->setFont(vertFont);
        painter->setPen(QColor(Qt::black));
        const QFontMetricsF vfm(vertFont, painter->device());
        const double vH = vfm.height();

        const QString lblA = d_graph->ternaryLabel(0);
        const QString lblB = d_graph->ternaryLabel(1);
        const QString lblC = d_graph->ternaryLabel(2);

        const double wA = vfm.horizontalAdvance(lblA) + 8.0 * sc;
        const double wB = vfm.horizontalAdvance(lblB) + 8.0 * sc;
        const double wC = vfm.horizontalAdvance(lblC) + 8.0 * sc;
        const double botVertY = qMax(pA.y(), pB.y()) + tickLen + padPx + 0.35 * tH;

        const QRectF safeCanvas =
                QRectF(canvasRect).adjusted(2.0 * sc, 2.0 * sc, -2.0 * sc, -2.0 * sc);
        QRectF rectA(pA.x() - wA + 4.0 * sc, botVertY, wA, vH);
        QRectF rectB(pB.x() - 4.0 * sc, botVertY, wB, vH);
        QRectF rectC(pC.x() - 0.5 * wC, pC.y() - vH - 6.0 * sc, wC, vH);
        if (safeCanvas.width() > wA + 4.0 * sc && safeCanvas.height() > vH + 4.0 * sc) {
            if (rectA.left() < safeCanvas.left())
                rectA.moveLeft(safeCanvas.left());
            if (rectA.bottom() > safeCanvas.bottom())
                rectA.moveBottom(safeCanvas.bottom());
        }
        if (safeCanvas.width() > wB + 4.0 * sc && safeCanvas.height() > vH + 4.0 * sc) {
            if (rectB.right() > safeCanvas.right())
                rectB.moveRight(safeCanvas.right());
            if (rectB.bottom() > safeCanvas.bottom())
                rectB.moveBottom(safeCanvas.bottom());
        }
        if (safeCanvas.width() > wC + 4.0 * sc && safeCanvas.height() > vH + 4.0 * sc) {
            if (rectC.top() < safeCanvas.top())
                rectC.moveTop(safeCanvas.top());
            rectC.moveLeft(qBound(safeCanvas.left(), rectC.left(), safeCanvas.right() - wC));
        }

        painter->drawText(rectA, Qt::AlignRight | Qt::AlignTop | Qt::TextDontClip, lblA);
        painter->drawText(rectB, Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, lblB);
        painter->drawText(rectC, Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip, lblC);

        painter->restore();
}

} // namespace GraphInternal

namespace {

class PolarAxesItem : public QwtPlotItem
{
public:
    explicit PolarAxesItem(const Graph *graph) : d_graph(graph)
    {
        setZ(12.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 45; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_graph)
            return;

        const double rMax = qMax(1e-6, d_graph->polarRadiusMax());
        const double cx = xMap.xTransform(0.0);
        const double cy = yMap.xTransform(0.0);
        const double rxOuter = std::fabs(xMap.xTransform(rMax) - cx);
        const double ryOuter = std::fabs(yMap.xTransform(rMax) - cy);
        if (!std::isfinite(cx) || !std::isfinite(cy) || rxOuter < 6.0 || ryOuter < 6.0)
            return;

        const double sc = Plot::printDeviceScale();
        const double baseLw = qMax(
                1.3,
                double(d_graph->plotWidget() ? d_graph->plotWidget()->axesLinewidth() : 2));

        QFont tickFont = d_graph->plotWidget()->displayFont(
                const_cast<Graph *>(d_graph)->axisFont(QwtPlot::xBottom));
        tickFont.setFamily(QStringLiteral("Arial"));
        tickFont.setBold(false);
        tickFont.setWeight(QFont::Normal);
        tickFont.setHintingPreference(QFont::PreferNoHinting);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        // 1. Radial concentric rings
        QwtLinearScaleEngine rse;
        const QwtScaleDiv rDiv = rse.divideScale(0.0, rMax, 4, 0);
        const QwtValueList rMaj = rDiv.ticks(QwtScaleDiv::MajorTick);
        QVector<double> rings;
        for (double rv : rMaj) {
            if (rv > 1e-9 * rMax && rv < rMax * (1.0 - 1e-4))
                rings.append(rv);
        }
        if (rings.isEmpty()) {
            rings << (0.25 * rMax) << (0.50 * rMax) << (0.75 * rMax);
        }

        QPen innerRingPen(QColor(203, 213, 225), 1.0 * sc, Qt::DashLine);
        innerRingPen.setCosmetic(false);
        painter->setPen(innerRingPen);
        painter->setBrush(Qt::NoBrush);
        for (double rv : rings) {
            const double rx = std::fabs(xMap.xTransform(rv) - cx);
            const double ry = std::fabs(yMap.xTransform(rv) - cy);
            painter->drawEllipse(QPointF(cx, cy), rx, ry);
        }

        // 2. Angular spokes every 30 degrees (0..330)
        constexpr double kPi = 3.14159265358979323846;
        QPen spokePen(QColor(203, 213, 225), 0.95 * sc, Qt::SolidLine);
        spokePen.setCosmetic(false);
        painter->setPen(spokePen);
        for (int deg = 0; deg < 360; deg += 30) {
            const double rad = double(deg) * kPi / 180.0;
            const double ex = xMap.xTransform(rMax * std::cos(rad));
            const double ey = yMap.xTransform(rMax * std::sin(rad));
            painter->drawLine(QPointF(cx, cy), QPointF(ex, ey));
        }

        // 3. Outer polar circle
        QPen outerPen(QColor(15, 23, 42), qMax(1.5, baseLw) * sc, Qt::SolidLine);
        outerPen.setCosmetic(false);
        painter->setPen(outerPen);
        painter->drawEllipse(QPointF(cx, cy), rxOuter, ryOuter);

        // 4. Exterior degree labels (0°, 30°, ..., 330°) in non-bold Arial
        painter->setFont(tickFont);
        const QFontMetricsF fm(tickFont, painter->device());
        const double lblH = fm.height();
        const QRectF safePolarCanvas =
                QRectF(canvasRect).adjusted(2.0 * sc, 2.0 * sc, -2.0 * sc, -2.0 * sc);
        const double degPad = 5.5 * sc;
        painter->setPen(QColor(15, 23, 42));
        for (int deg = 0; deg < 360; deg += 30) {
            const double rad = double(deg) * kPi / 180.0;
            const double ux = std::cos(rad);
            const double uy = -std::sin(rad); // screen Y points downward
            const QString degStr = QString::number(deg) + QString::fromUtf8("\xC2\xB0");
            const double tw = fm.horizontalAdvance(degStr) + 4.0 * sc;
            const double anchorX = cx + (rxOuter + degPad + 0.5 * tw) * ux;
            const double anchorY = cy + (ryOuter + degPad + 0.45 * lblH) * uy;
            QRectF degRect(anchorX - 0.5 * tw, anchorY - 0.5 * lblH, tw, lblH);
            if (safePolarCanvas.width() > tw + 4.0 * sc && safePolarCanvas.height() > lblH + 4.0 * sc) {
                degRect.moveLeft(
                        qBound(safePolarCanvas.left(), degRect.left(), safePolarCanvas.right() - tw));
                degRect.moveTop(
                        qBound(safePolarCanvas.top(), degRect.top(), safePolarCanvas.bottom() - lblH));
            }
            painter->drawText(degRect, Qt::AlignCenter | Qt::TextDontClip, degStr);
        }

        // Radial tick values & pills are rendered in PolarRadialLabelsItem (z = 165.0)
        // so plotted polar curves and outer boundary ring never slice over tick digits.
        painter->restore();
    }

private:
    const Graph *d_graph;
};

class PolarRadialLabelsItem : public QwtPlotItem
{
public:
    explicit PolarRadialLabelsItem(const Graph *graph) : d_graph(graph)
    {
        setZ(165.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 46; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_graph || !d_graph->isPolarPlot())
            return;

        const double rMax = qMax(1e-6, d_graph->polarRadiusMax());
        const double cx = xMap.xTransform(0.0);
        const double cy = yMap.xTransform(0.0);
        const double rxOuter = std::fabs(xMap.xTransform(rMax) - cx);
        const double ryOuter = std::fabs(yMap.xTransform(rMax) - cy);
        if (!std::isfinite(cx) || !std::isfinite(cy) || rxOuter < 6.0 || ryOuter < 6.0)
            return;

        const double sc = Plot::printDeviceScale();
        QFont tickFont = d_graph->plotWidget()->displayFont(
                const_cast<Graph *>(d_graph)->axisFont(QwtPlot::xBottom));
        tickFont.setFamily(QStringLiteral("Arial"));
        tickFont.setBold(false);
        tickFont.setWeight(QFont::Normal);
        tickFont.setHintingPreference(QFont::PreferNoHinting);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        painter->setFont(tickFont);

        QwtLinearScaleEngine rse;
        const QwtScaleDiv rDiv = rse.divideScale(0.0, rMax, 4, 0);
        const QwtValueList rMaj = rDiv.ticks(QwtScaleDiv::MajorTick);
        QVector<double> rings;
        for (double rv : rMaj) {
            if (rv > 1e-9 * rMax && rv < rMax * (1.0 - 1e-4))
                rings.append(rv);
        }
        if (rings.isEmpty())
            rings << (0.25 * rMax) << (0.50 * rMax) << (0.75 * rMax);

        constexpr double kPi = 3.14159265358979323846;
        const QFontMetricsF fm(tickFont, painter->device());
        const double lblH = fm.height();
        const double degPad = 5.5 * sc;
        const QRectF safePolarCanvas =
                QRectF(canvasRect).adjusted(2.0 * sc, 2.0 * sc, -2.0 * sc, -2.0 * sc);

        auto degLabelBox = [&](int deg) -> QRectF {
            const double rad = double(deg) * kPi / 180.0;
            const double ux = std::cos(rad);
            const double uy = -std::sin(rad);
            const QString degStr = QString::number(deg) + QString::fromUtf8("\xC2\xB0");
            const double tw = fm.horizontalAdvance(degStr) + 4.0 * sc;
            const double ax = cx + (rxOuter + degPad + 0.5 * tw) * ux;
            const double ay = cy + (ryOuter + degPad + 0.45 * lblH) * uy;
            return QRectF(ax - 0.5 * tw, ay - 0.5 * lblH, tw, lblH);
        };
        const QRectF deg0Box = degLabelBox(0).adjusted(-1.5 * sc, -1.0 * sc, 1.5 * sc, 1.0 * sc);
        const QRectF deg30Box = degLabelBox(30).adjusted(-1.5 * sc, -1.0 * sc, 1.5 * sc, 1.0 * sc);

        const double rayRad = 15.0 * kPi / 180.0;
        const double cosRay = std::cos(rayRad);
        const double sinRay = std::sin(rayRad);
        QVector<double> candRings = rings;
        candRings.append(rMax);

        struct RadialPillSpec
        {
            QRectF pill;
            QString text;
        };
        QVector<RadialPillSpec> acceptedPills;
        acceptedPills.reserve(candRings.size());
        for (int ri = candRings.size() - 1; ri >= 0; --ri) {
            const double rv = candRings[ri];
            const QString rTxt = QLocale::c().toString(rv, 'g', 4);
            const double rw = fm.horizontalAdvance(rTxt) + 3.6 * sc;
            const double rh = lblH * 0.90;
            double px = xMap.xTransform(rv * cosRay);
            double py = yMap.xTransform(rv * sinRay);
            QRectF pill(px - 0.5 * rw, py - 0.5 * rh, rw, rh);

            if (ri == candRings.size() - 1) {
                const double outPx = cx + (rxOuter + 0.52 * rw + 3.2 * sc) * cosRay;
                const double outPy = cy - (ryOuter + 0.52 * rh + 3.2 * sc) * sinRay;
                const QRectF outPill(outPx - 0.5 * rw, outPy - 0.5 * rh, rw, rh);
                if (safePolarCanvas.contains(outPill) && !outPill.intersects(deg0Box)
                    && !outPill.intersects(deg30Box)) {
                    pill = outPill;
                }
            }

            const QRectF padded = pill.adjusted(-1.8 * sc, -1.0 * sc, 1.8 * sc, 1.0 * sc);
            bool overlapsAccepted = false;
            for (const RadialPillSpec &acc : acceptedPills) {
                if (padded.intersects(acc.pill)) {
                    overlapsAccepted = true;
                    break;
                }
            }
            if (!overlapsAccepted)
                acceptedPills.prepend({ pill, rTxt });
        }

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 248));
        for (const RadialPillSpec &rp : acceptedPills)
            painter->drawRoundedRect(rp.pill, 2.0 * sc, 2.0 * sc);

        painter->setBrush(Qt::NoBrush);
        painter->setPen(QColor(15, 23, 42));
        for (const RadialPillSpec &rp : acceptedPills)
            painter->drawText(rp.pill, Qt::AlignCenter | Qt::TextDontClip, rp.text);

        painter->restore();
    }

private:
    const Graph *d_graph;
};

} // namespace

QPointF Graph::ternaryToCartesian(double a, double b, double c)
{
    const double sum = a + b + c;
    const double h = std::sqrt(3.0) * 0.5;
    if (qAbs(sum) <= 1e-15)
        return QPointF(0.5, h / 3.0);

    const double bn = b / sum;
    const double cn = c / sum;
    const double x = bn + 0.5 * cn;
    const double y = h * cn;
    return QPointF(x, y);
}

void Graph::setupTernaryPlot(const QString &labelA, const QString &labelB, const QString &labelC)
{
    d_is_ternary = true;
    d_is_polar = false;
    d_ternary_labels[0] = labelA.isEmpty() ? QString("A") : labelA;
    d_ternary_labels[1] = labelB.isEmpty() ? QString("B") : labelB;
    d_ternary_labels[2] = labelC.isEmpty() ? QString("C") : labelC;
    m_autoscale = false;

    if (d_polar_item) {
        d_polar_item->detach();
        delete d_polar_item;
        d_polar_item = nullptr;
    }
    if (d_plot) {
        const QwtPlotItemList items = d_plot->itemList();
        for (QwtPlotItem *it : items) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotUserItem + 46) {
                it->detach();
                delete it;
            }
        }
    }

    if (grid()) {
        grid()->enableX(false);
        grid()->enableXMin(false);
        grid()->enableY(false);
        grid()->enableYMin(false);
    }
    drawCanvasFrame(false, 0);
    drawAxesBackbones(false);

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QString plainT = unwrapNoWrapAxisTitle(axisTitle(a)).trimmed();
        if (isAxisPlaceholderTitle(plainT)) {
            QwtText emptyT = d_plot->axisTitle(a);
            emptyT.setText(QString());
            d_plot->setAxisTitle(a, emptyT);
        }
    }

    // Equal 1.48 x 1.48 world span guarantees a strictly 60-degree equilateral triangle on 1:1 canvases
    // with generous outer margin for long component titles (e.g. "Al (at.%)", "Zn (at.%)").
    setScale(QwtPlot::xBottom, -0.24, 1.24, 0.2, 6, 0, 0, false);
    setScale(QwtPlot::yLeft, -0.25, 1.23, 0.2, 6, 0, 0, false);
    for (int i = 0; i < QwtPlot::axisCnt; ++i)
        enableAxis(i, false);

    if (!d_ternary_item) {
        d_ternary_item = new TernaryDiagramItem(this);
        d_ternary_item->attach(d_plot);
    }

    if (hasLegend()) {
        if (n_curves == 1) {
            if (Legend *leg = legend()) {
                const QString lt = leg->text().trimmed();
                if (lt.contains(QLatin1String("Ternary_Y"))
                    || lt.endsWith(QLatin1String("}Y"))) {
                    setLegendVisible(false);
                }
            }
        }
        if (isLegendVisible())
            placeLegendAnchor(LegendTopRight);
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setupPolarPlot(const QVector<double> &thetaDeg, const QVector<double> &radius,
                           const QString &seriesLabel, bool thetaInRadians)
{
    const bool alreadyPolar = d_is_polar;
    d_is_polar = true;
    d_polar_view_set = false;
    d_is_ternary = false;
    m_autoscale = false;

    if (d_ternary_item) {
        d_ternary_item->detach();
        delete d_ternary_item;
        d_ternary_item = nullptr;
    }

    if (grid()) {
        grid()->enableX(false);
        grid()->enableXMin(false);
        grid()->enableY(false);
        grid()->enableYMin(false);
    }
    drawCanvasFrame(false, 0);
    drawAxesBackbones(false);

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QString plainT = unwrapNoWrapAxisTitle(axisTitle(a)).trimmed();
        if (isAxisPlaceholderTitle(plainT)) {
            QwtText emptyT = d_plot->axisTitle(a);
            emptyT.setText(QString());
            d_plot->setAxisTitle(a, emptyT);
        }
    }

    const int n = qMin(thetaDeg.size(), radius.size());
    QVector<double> pxVec, pyVec;
    pxVec.reserve(n);
    pyVec.reserve(n);
    double maxR = alreadyPolar ? d_polar_rmax : 0.0;
    constexpr double kPi = 3.14159265358979323846;

    for (int i = 0; i < n; ++i) {
        const double th = thetaDeg[i];
        const double rv = radius[i];
        if (!std::isfinite(th) || !std::isfinite(rv))
            continue;
        const double rad = thetaInRadians ? th : (th * kPi / 180.0);
        pxVec.append(rv * std::cos(rad));
        pyVec.append(rv * std::sin(rad));
        maxR = qMax(maxR, std::fabs(rv));
    }

    if (maxR > 0.0) {
        double rLo = 0.0;
        double rHi = maxR;
        double rStep = 0.0;
        QwtLinearScaleEngine rse;
        rse.autoScale(4, rLo, rHi, rStep);
        d_polar_rmax = (rHi >= maxR && std::isfinite(rHi)) ? rHi : maxR;
    } else if (!(d_polar_rmax > 0.0) || !std::isfinite(d_polar_rmax)) {
        d_polar_rmax = 1.0;
    }

    if (!d_polar_item) {
        d_polar_item = new PolarAxesItem(this);
        d_polar_item->attach(d_plot);
    }
    if (d_plot) {
        bool hasFgLabels = false;
        for (QwtPlotItem *it : d_plot->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotUserItem + 46) {
                hasFgLabels = true;
                break;
            }
        }
        if (!hasFgLabels) {
            PolarRadialLabelsItem *fgLbls = new PolarRadialLabelsItem(this);
            fgLbls->attach(d_plot);
        }
    }

    if (!pxVec.isEmpty()) {
        const QList<QColor> polarPalette = paletteColors(defaultCurvePaletteName());
        const int sIdx = d_polar_series.size();
        const QColor col = polarPalette.isEmpty()
                ? QColor(Qt::black)
                : polarPalette.at(sIdx % polarPalette.size());
        const QString lbl = seriesLabel.trimmed().isEmpty()
                ? tr("Polar %1").arg(sIdx + 1)
                : seriesLabel.trimmed();

        PlotCurve *pc = new PlotCurve(lbl);
        pc->setType(Function);
        pc->setData(pxVec.constData(), pyVec.constData(), pxVec.size());
        QPen p(col, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setCosmetic(false);
        pc->setPen(p);

        c_type.resize(++n_curves);
        c_type[n_curves - 1] = Function;
        c_keys.resize(n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(pc);
        syncCurveZOrder();

        PolarSeriesSpec ps;
        ps.x = pxVec;
        ps.y = pyVec;
        ps.label = lbl;
        ps.color = col;
        ps.width = 2.0;
        d_polar_series.append(ps);

        addLegendItem(lbl);
    }

    const double rMax = qMax(1e-6, d_polar_rmax);
    const int nPolarRows = qMax(1, qMax(int(d_polar_series.size()), n_curves));
    const int extraRows = qMax(0, nPolarRows - 1);
    const double yLo = -1.44 * rMax;
    const double yHi = (1.64 + 0.36 * double(extraRows)) * rMax;
    const double halfX = 0.5 * (yHi - yLo);
    setScale(QwtPlot::xBottom, -halfX, halfX, 0.0, 5, 0, 0, false);
    setScale(QwtPlot::yLeft, yLo, yHi, 0.0, 5, 0, 0, false);
    for (int i = 0; i < QwtPlot::axisCnt; ++i)
        enableAxis(i, false);

    if (hasLegend())
        placeLegendAnchor(LegendTopRight);

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::polarDefaultViewRange(double *xMin, double *xMax, double *yMin, double *yMax) const
{
    const double rMax = qMax(1e-6, d_polar_rmax);
    const int nPolarRows = qMax(1, qMax(int(d_polar_series.size()), n_curves));
    const int extraRows = qMax(0, nPolarRows - 1);
    const double yLo = -1.44 * rMax;
    const double yHi = (1.64 + 0.36 * double(extraRows)) * rMax;
    const double halfX = 0.5 * (yHi - yLo);
    *xMin = -halfX;
    *xMax = halfX;
    *yMin = yLo;
    *yMax = yHi;
}

void Graph::setPolarViewRange(double xMin, double xMax, double yMin, double yMax)
{
    if (!d_is_polar || !std::isfinite(xMin) || !std::isfinite(xMax) || !std::isfinite(yMin)
        || !std::isfinite(yMax) || !(xMax > xMin) || !(yMax > yMin))
        return;
    d_polar_view[0] = xMin;
    d_polar_view[1] = xMax;
    d_polar_view[2] = yMin;
    d_polar_view[3] = yMax;
    d_polar_view_set = true;
    setScale(QwtPlot::xBottom, xMin, xMax, 0.0, 5, 0, 0, false);
    setScale(QwtPlot::yLeft, yMin, yMax, 0.0, 5, 0, 0, false);
    for (int a = 0; a < QwtPlot::axisCnt; ++a)
        enableAxis(a, false);
}

DataCurve *Graph::pointColorMapCurve() const
{
    if (!d_plot)
        return nullptr;
    for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); ++i) {
        if (c_type[i] == ErrorBars)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(d_plot->curve(c_keys[i]));
        if (dc && dc->isVisible() && dc->hasPointColorMap())
            return dc;
    }
    return nullptr;
}

int Graph::errorBandCount() const
{
    int n = 0;
    for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); ++i) {
        if (c_type[i] != ErrorBars)
            continue;
        if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(d_plot->curve(c_keys[i])))
            if (er->isBand())
                ++n;
    }
    return n;
}

void Graph::syncPointColorBar()
{
    if (!d_plot)
        return;
    const int axis = QwtPlot::yRight;
    // A heatmap that owns the right colour bar wins.
    for (QwtPlotItem *it : d_plot->itemList()) {
        if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = static_cast<Spectrogram *>(it);
            if (sp->hasColorScale() && sp->colorScaleAxis() == axis)
                return;
        }
    }
    QwtScaleWidget *sw = d_plot->axisWidget(axis);
    if (!sw)
        return;
    DataCurve *src = pointColorMapCurve();
    if (!src || !src->pointColorMap().showColorBar) {
        if (d_point_colorbar_active || sw->isColorBarEnabled()) {
            d_point_colorbar_active = false;
            sw->setColorBarEnabled(false);
            sw->setMargin(0);
            bool anyRightCurve = false;
            for (int k = 0; k < n_curves && k < c_keys.size(); ++k) {
                if (QwtPlotItem *it = d_plot->plotItem(c_keys[k])) {
                    if (it->yAxis() == axis) {
                        anyRightCurve = true;
                        break;
                    }
                }
            }
            if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
                sd->setColorBarBorder(false, 0, 1);
                if (!anyRightCurve)
                    sd->enableComponent(QwtAbstractScaleDraw::Labels, false);
            }
            if (!anyRightCurve) {
                if (d_plot->axisEnabled(QwtPlot::xTop) && isAxisBackboneVisible(axis))
                    updateSecondaryAxis(axis);
                else
                    enableAxis(axis, false);
            }
            d_plot->updateLayout();
            d_plot->replot();
        }
        return;
    }
    double lo = 0.0, hi = 1.0;
    src->pointColorRange(&lo, &hi);
    const DataCurve::PointColorMap &spec = src->pointColorMap();
    const QwtLinearColorMap map = Spectrogram::presetColorMap(
            static_cast<Spectrogram::ColorMapPreset>(spec.preset), spec.reverse);
    d_point_colorbar_active = true;
    if (!d_plot->axisEnabled(axis))
        d_plot->enableAxis(axis, true);
    sw->setColorBarEnabled(true);
    sw->setColorMap(QwtDoubleInterval(lo, hi), map);
    sw->setMargin(4);
    sw->setSpacing(0);
    sw->setMinBorderDist(0, 0);
    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->setColorBarBorder(true, sw->colorBarWidth(), qMax(1, sw->penWidth()));
    }
    d_plot->setAxisScale(axis, lo, hi);
    if (!spec.colorBarTitle.isEmpty())
        setAxisTitle(axis, spec.colorBarTitle);
    QFont tf = d_plot->axisFont(axis);
    tf.setFamily(QStringLiteral("Arial"));
    tf.setHintingPreference(QFont::PreferNoHinting);
    d_plot->setAxisFont(axis, tf);
    sw->show();
    d_plot->updateLayout();
    d_plot->replot();
}

bool Graph::isPoleFigurePlot() const
{
    if (!d_plot
        || ExtraPlotTypes::graphTag(const_cast<Graph *>(this), QStringLiteral("ToriPoleFigure"))
                   .isEmpty()) {
        return false;
    }
    for (QwtPlotItem *it : d_plot->itemList()) {
        if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            return true;
    }
    return false;
}

void Graph::setStackOffsets(int mode, double yGapPct, double xGapPct, bool normalize01)
{
    QList<PlotCurve *> targetCurves;
    for (int i = 0; i < n_curves; ++i) {
        if (c_type[i] == ErrorBars || c_type[i] == Pie || c_type[i] == Box
            || c_type[i] == ColorMap || c_type[i] == GrayMap || c_type[i] == ContourMap)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(i));
        if (pc && pc->isVisible() && pc->dataSize() > 0)
            targetCurves.append(pc);
    }
    if (targetCurves.isEmpty())
        return;

    // First reset to clean raw unstacked values (with optional 0..1 normalization)
    for (PlotCurve *pc : targetCurves) {
        pc->setStackOffset(0.0, 0.0);
        pc->setStackNormalize01(normalize01);
    }

    if (targetCurves.size() < 2) {
        if (m_autoscale) {
            for (int a = 0; a < QwtPlot::axisCnt; ++a)
                d_plot->setAxisAutoScale(a);
        }
        updatePlot();
        emit modifiedGraph();
        return;
    }

    const int m = targetCurves.size();
    QVector<double> minY(m, 0.0), maxY(m, 0.0), spanY(m, 0.0);
    double sumSpanY = 0.0;
    int validSpansY = 0;
    double maxSpanX = 0.0;

    for (int k = 0; k < m; ++k) {
        PlotCurve *pc = targetCurves[k];
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        const int n = pc->dataSize();
        Column *dcXCol = nullptr;
        Column *dcYCol = nullptr;
        const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
        if (dcHasMasks)
            dc->resolveColumns(dcXCol, dcYCol);
        bool first = true;
        double loX = 0.0, hiX = 0.0, loY = 0.0, hiY = 0.0;
        for (int j = 0; j < n; ++j) {
            if (dcHasMasks && dc->isPointMaskedFast(j, dcXCol, dcYCol))
                continue;
            const double xv = pc->x(j);
            const double yv = pc->y(j);
            if (!std::isfinite(xv) || !std::isfinite(yv))
                continue;
            if (first) {
                loX = hiX = xv;
                loY = hiY = yv;
                first = false;
            } else {
                loX = qMin(loX, xv);
                hiX = qMax(hiX, xv);
                loY = qMin(loY, yv);
                hiY = qMax(hiY, yv);
            }
        }
        minY[k] = loY;
        maxY[k] = hiY;
        spanY[k] = qMax(0.0, hiY - loY);
        if (!first) {
            sumSpanY += spanY[k];
            validSpansY++;
            maxSpanX = qMax(maxSpanX, hiX - loX);
        }
    }

    const double avgSpanY =
            (validSpansY > 0 && sumSpanY > 1e-12) ? (sumSpanY / double(validSpansY)) : 1.0;
    const double refSpanX = (maxSpanX > 1e-12) ? maxSpanX : 1.0;
    const double stepX =
            (qAbs(xGapPct) > 1e-12) ? (refSpanX * (xGapPct / 100.0)) : 0.0;

    double prevTop = maxY[0];
    for (int k = 1; k < m; ++k) {
        PlotCurve *pc = targetCurves[k];
        const double dx = double(k) * stepX;
        double dy = 0.0;
        if (mode == 1) {
            // AutoGap: each curve sits cleanly above the previous curve with yGapPct% margin
            dy = (prevTop - minY[k]) + (yGapPct / 100.0) * avgSpanY;
            prevTop = maxY[k] + dy;
        } else {
            // Uniform: constant vertical step across all curves
            if (qAbs(yGapPct) <= 1e-12 && qAbs(xGapPct) > 1e-12)
                dy = 0.0;
            else
                dy = double(k) * avgSpanY * (1.0 + yGapPct / 100.0);
        }
        pc->setStackOffset(dx, dy);

        if (DataCurve *dc = dynamic_cast<DataCurve *>(pc)) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->loadData();
            }
        }
    }

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        setToriAxisUserLocked(d_plot, a, false);
        d_plot->setAxisAutoScale(a);
    }
    updatePlot();

    {
        double overallMinY = 1e300;
        double overallMaxY = -1e300;
        for (PlotCurve *pc : targetCurves) {
            DataCurve *dc = dynamic_cast<DataCurve *>(pc);
            const int n = pc->dataSize();
            Column *dcXCol = nullptr;
            Column *dcYCol = nullptr;
            const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
            if (dcHasMasks)
                dc->resolveColumns(dcXCol, dcYCol);
            for (int j = 0; j < n; ++j) {
                if (dcHasMasks && dc->isPointMaskedFast(j, dcXCol, dcYCol))
                    continue;
                const double yv = pc->y(j);
                if (std::isfinite(yv)) {
                    overallMinY = qMin(overallMinY, yv);
                    overallMaxY = qMax(overallMaxY, yv);
                }
            }
        }
        const QwtScaleDiv *yDiv = d_plot->axisScaleDiv(QwtPlot::yLeft);
        const QwtScaleEngine *yEng = d_plot->axisScaleEngine(QwtPlot::yLeft);
        if (yDiv && yEng && !Plot::isLog10ScaleEngine(yEng)
            && !yEng->testAttribute(QwtScaleEngine::Inverted)
            && !d_axis_breaks[QwtPlot::yLeft].enabled && overallMaxY > 0.0
            && overallMinY >= -0.04 * overallMaxY) {
            const double autoLo = yDiv->lowerBound();
            const double autoHi = yDiv->upperBound();
            if (autoHi > 0.0 && autoLo < -0.035 * autoHi) {
                const double clampedLo = (overallMinY >= 0.0) ? 0.0 : (-0.03 * autoHi);
                const QwtValueList majTicks = yDiv->ticks(QwtScaleDiv::MajorTick);
                const double step = (majTicks.size() >= 2)
                        ? std::fabs(majTicks[1] - majTicks[0])
                        : d_user_step[QwtPlot::yLeft];
                const int maj = d_plot->axisMaxMajor(QwtPlot::yLeft);
                const int minT = d_plot->axisMaxMinor(QwtPlot::yLeft);
                const int maxMinInt = (minT >= 1) ? (minT + 1) : 0;
                QwtLinearScaleEngine *newEng = new QwtLinearScaleEngine();
                QwtScaleDiv newDiv = newEng->divideScale(clampedLo, autoHi, maj, maxMinInt, step);
                if (minT == 1)
                    ensureLinearHalfStepMinorTicks(newDiv);
                d_plot->setAxisScale(QwtPlot::yLeft, clampedLo, autoHi, step);
                d_plot->setAxisScaleEngine(QwtPlot::yLeft, newEng);
                d_plot->setAxisScaleDiv(QwtPlot::yLeft, newDiv);
                updateSecondaryAxis(QwtPlot::yRight);
                d_zoomer[0]->setZoomBase();
                d_plot->replot();
                syncChildLinkedLayers();
            }
        }
    }

    emit modifiedGraph();
}

void Graph::resetStackOffsets()
{
    bool changed = false;
    for (int i = 0; i < n_curves; ++i) {
        PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(i));
        if (!pc)
            continue;
        if (qAbs(pc->stackOffsetX()) > 1e-15 || qAbs(pc->stackOffsetY()) > 1e-15
            || pc->stackNormalize01()) {
            pc->setStackNormalize01(false);
            pc->setStackOffset(0.0, 0.0);
            changed = true;
            if (DataCurve *dc = dynamic_cast<DataCurve *>(pc)) {
                for (DataCurve *eb : dc->errorBarsList()) {
                    if (eb)
                        eb->loadData();
                }
            }
        }
    }
    if (changed) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            setToriAxisUserLocked(d_plot, a, false);
            d_plot->setAxisAutoScale(a);
        }
        updatePlot();
        emit modifiedGraph();
    }
}

void Graph::syncCurveZOrder()
{
    if (!d_plot)
        return;

    for (int i = 0; i < n_curves && i < c_keys.size(); ++i) {
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it)
            continue;
        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            it->setZ(8.0 + i * 0.01);
        } else if (i < c_type.size() && c_type[i] != ErrorBars) {
            it->setZ(20.0 + i * 0.01);
        }
    }

    QMap<DataCurve *, int> errOrdinalPerMaster;
    for (int i = 0; i < n_curves && i < c_keys.size(); ++i) {
        if (i >= c_type.size() || c_type[i] != ErrorBars)
            continue;
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it)
            continue;
        QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(it);
        if (er && er->masterCurve() && er->isBand()) {
            // E11: error bands sit under every curve so a band never hides another line.
            const int ord = errOrdinalPerMaster[er->masterCurve()]++;
            it->setZ(er->masterCurve()->z() - 5.0 + ord * 0.0005);
        } else if (er && er->masterCurve()) {
            const int ord = errOrdinalPerMaster[er->masterCurve()]++;
            it->setZ(er->masterCurve()->z() - 0.004 + ord * 0.0005);
        } else {
            it->setZ(20.0 + i * 0.01);
        }
    }
}

bool Graph::moveCurve(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= n_curves || toIndex < 0 || toIndex >= n_curves
        || fromIndex == toIndex || c_keys.size() < n_curves || c_type.size() < n_curves) {
        return false;
    }

    auto masterOldIndexFor = [&](int errIdx) -> int {
        if (errIdx < 0 || errIdx >= n_curves || c_type[errIdx] != ErrorBars)
            return -1;
        QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(plotItem(errIdx));
        if (!er || !er->masterCurve())
            return -1;
        return plotItemIndex(er->masterCurve());
    };

    QVector<int> order;
    order.reserve(n_curves);

    if (c_type[fromIndex] == ErrorBars) {
        const int mIdx = masterOldIndexFor(fromIndex);
        if (mIdx >= 0)
            toIndex = qMax(toIndex, mIdx + 1);
        if (toIndex == fromIndex || toIndex >= n_curves)
            return false;

        order.resize(n_curves);
        for (int i = 0; i < n_curves; ++i)
            order[i] = i;
        const int movedVal = order[fromIndex];
        order.remove(fromIndex);
        order.insert(toIndex, movedVal);

        // Enforce .sciprj load invariant: every QwtErrorPlotCurve must sit strictly after its master
        for (int oldIdx = 0; oldIdx < n_curves; ++oldIdx) {
            if (c_type[oldIdx] != ErrorBars)
                continue;
            const int mOld = masterOldIndexFor(oldIdx);
            if (mOld < 0)
                continue;
            const int posErr = order.indexOf(oldIdx);
            const int posMaster = order.indexOf(mOld);
            if (posErr >= 0 && posMaster >= 0 && posErr < posMaster) {
                order.remove(posErr);
                const int newMasterPos = order.indexOf(mOld);
                int insertPos = newMasterPos + 1;
                while (insertPos < order.size() && masterOldIndexFor(order[insertPos]) == mOld)
                    ++insertPos;
                order.insert(insertPos, oldIdx);
            }
        }
    } else {
        // Partition curves into logical [MasterCurve + attached ErrorBars] groups so moving a
        // master curve steps cleanly across neighbor groups without snapping back or splitting.
        QVector<QVector<int>> groups;
        QVector<int> idxToGroup(n_curves, -1);
        for (int i = 0; i < n_curves; ++i) {
            if (idxToGroup[i] >= 0)
                continue;
            if (c_type[i] == ErrorBars) {
                const int mOld = masterOldIndexFor(i);
                if (mOld >= 0 && idxToGroup[mOld] >= 0) {
                    const int g = idxToGroup[mOld];
                    groups[g].append(i);
                    idxToGroup[i] = g;
                    continue;
                }
                if (mOld >= 0 && mOld > i) {
                    const int g = groups.size();
                    QVector<int> grp;
                    grp.append(mOld);
                    idxToGroup[mOld] = g;
                    for (int k = 0; k < n_curves; ++k) {
                        if (k != mOld && masterOldIndexFor(k) == mOld) {
                            grp.append(k);
                            idxToGroup[k] = g;
                        }
                    }
                    groups.append(grp);
                    continue;
                }
                const int g = groups.size();
                groups.append(QVector<int>{ i });
                idxToGroup[i] = g;
            } else {
                const int g = groups.size();
                QVector<int> grp;
                grp.append(i);
                idxToGroup[i] = g;
                for (int k = 0; k < n_curves; ++k) {
                    if (k != i && masterOldIndexFor(k) == i) {
                        grp.append(k);
                        idxToGroup[k] = g;
                    }
                }
                groups.append(grp);
            }
        }

        const int srcGroup = idxToGroup[fromIndex];
        int dstGroup = idxToGroup[toIndex];
        if (srcGroup < 0 || dstGroup < 0)
            return false;
        if (dstGroup == srcGroup) {
            if (toIndex > fromIndex)
                dstGroup = srcGroup + 1;
            else if (toIndex < fromIndex)
                dstGroup = srcGroup - 1;
        }
        if (dstGroup < 0 || dstGroup >= groups.size() || dstGroup == srcGroup)
            return false;

        const QVector<int> movedGrp = groups.takeAt(srcGroup);
        groups.insert(dstGroup, movedGrp);

        for (const QVector<int> &grp : groups) {
            for (int oldIdx : grp)
                order.append(oldIdx);
        }
    }

    if (order.size() != n_curves)
        return false;

    bool anyOrderChange = false;
    for (int i = 0; i < n_curves; ++i) {
        if (order[i] != i) {
            anyOrderChange = true;
            break;
        }
    }
    if (!anyOrderChange)
        return false;

    QVector<int> oldToNew(n_curves, 0);
    QVector<int> newKeys(n_curves);
    QVector<int> newTypes(n_curves);
    for (int newIdx = 0; newIdx < n_curves; ++newIdx) {
        const int oldIdx = order[newIdx];
        oldToNew[oldIdx] = newIdx;
        newKeys[newIdx] = c_keys[oldIdx];
        newTypes[newIdx] = c_type[oldIdx];
    }
    if (newKeys == c_keys)
        return false;
    c_keys = newKeys;
    c_type = newTypes;

    {
        QMap<int, QString> remappedAutoLabels;
        for (auto it = d_last_auto_curve_labels.constBegin();
             it != d_last_auto_curve_labels.constEnd(); ++it) {
            if (it.key() >= 0 && it.key() < n_curves)
                remappedAutoLabels.insert(oldToNew[it.key()], it.value());
        }
        d_last_auto_curve_labels = remappedAutoLabels;
    }

    if ((d_selection.kind == GraphSelection::Curve
         || d_selection.kind == GraphSelection::ErrorBars
         || d_selection.kind == GraphSelection::Spectrogram)
        && d_selection.primaryId >= 0 && d_selection.primaryId < n_curves) {
        d_selection.primaryId = oldToNew[d_selection.primaryId];
    }

    // Single-pass regex remap of 1-based \c{k}, \l(k), %(k) tokens across all Legend/text markers
    for (int tid : d_texts) {
        Legend *tm = dynamic_cast<Legend *>(d_plot->marker(tid));
        if (!tm)
            continue;
        const QString src = tm->text();
        const QString dst = remapLegendCurveTokens(src, oldToNew);
        if (dst != src)
            tm->setText(dst);
    }

    syncCurveZOrder();
    d_plot->replot();
    emit curveOrderChanged();
    emit modifiedGraph();
    return true;
}
