/***************************************************************************
    File                 : QwtBarCurve.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Bar curve

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
#include "QwtBarCurve.h"
#include "Plot.h"
#include "core/column/Column.h"
#include <QPainter>
#include <cmath>
#include <qwt_scale_map.h>

QwtBarCurve::QwtBarCurve(BarStyle style, Table *t, const QString &xColName, const QString &name,
                         int startRow, int endRow)
    : DataCurve(t, xColName, name, startRow, endRow)
{
    bar_offset = 0;
    bar_gap = 30;
    bar_style = style;
    d_stacked = false;

    QPen borderPen(Qt::black, 1, Qt::SolidLine);
    borderPen.setCosmetic(false);
    setPen(borderPen);
    setBrush(QBrush(QColor(0, 114, 178)));

    if (bar_style == Vertical)
        setType(Graph::VerticalBars);
    else
        setType(Graph::HorizontalBars);
}

void QwtBarCurve::copy(const QwtBarCurve *b)
{
    bar_gap = b->bar_gap;
    bar_offset = b->bar_offset;
    bar_style = b->bar_style;
    d_stacked = b->d_stacked;
}

void QwtBarCurve::draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                       int from, int to) const
{
    if (!painter || dataSize() <= 0)
        return;

    if (from < 0)
        from = 0;
    if (to < 0 || to >= dataSize())
        to = dataSize() - 1;
    if (from > to)
        return;

    painter->save();
    const double sc = renderPenScale();
    QPen barPen = QwtPlotCurve::pen();
    if (barPen.style() != Qt::NoPen) {
        double wf = barPen.widthF();
        if (wf <= 0.0)
            wf = (barPen.width() > 0) ? double(barPen.width()) : 1.0;
        barPen.setCosmetic(false);
        barPen.setWidthF(qMax(0.1, wf) * sc);
    }
    painter->setPen(barPen);
    painter->setBrush(QwtPlotCurve::brush());

    const AxisBreakBands brk = axisBreakBands(xMap, yMap);
    Column *xCol = nullptr;
    Column *yCol = nullptr;
    const bool hasMasks = hasAnyMaskedPoints();
    if (hasMasks)
        resolveColumns(xCol, yCol);

    double dx = 0.0, dy = 0.0;
    double bar_width = 0.0;

    if (bar_style == Vertical) {
        const bool isLogY = yMap.transformation()
                && yMap.transformation()->type() == QwtScaleTransformation::Log10;
        const double pyS1 = yMap.xTransform(yMap.s1());
        const double pyS2 = yMap.xTransform(yMap.s2());
        const double pyMin = qMin(pyS1, pyS2);
        const double pyMax = qMax(pyS1, pyS2);
        const double baseVal = isLogY ? qMax(1e-100, qMin(yMap.s1(), yMap.s2())) : 0.0;
        const double basePy = qBound(pyMin, yMap.xTransform(baseVal), pyMax);

        if (dataSize() >= 2) {
            for (int i = 0; i + 1 < dataSize(); i++) {
                const double x0 = x(i);
                const double x1 = x(i + 1);
                if (brk.hasX && (brk.xValueInGap(x0) || brk.xValueInGap(x1)
                                 || brk.xValuesStraddle(x0, x1)))
                    continue;
                const double step = std::fabs(xMap.xTransform(x1) - xMap.xTransform(x0));
                if (step > 1e-6 && (dx <= 1e-6 || step < dx))
                    dx = step;
            }
        }
        if (dx <= 1e-6) {
            const double spanPx = std::fabs(xMap.xTransform(xMap.s2()) - xMap.xTransform(xMap.s1()));
            dx = qMax(8.0 * sc, spanPx / 5.0);
        }
        bar_width = qMax(2.0 * sc, dx * (1.0 - bar_gap * 0.01));
        const double half_width = (0.5 - bar_offset * 0.01) * bar_width;

        for (int i = from; i <= to; i++) {
            if (hasMasks && isPointMaskedFast(i, xCol, yCol))
                continue;
            const double vx = x(i);
            const double vy = y(i);
            if (brk.xValueInGap(vx))
                continue;
            const double px = xMap.xTransform(vx);
            const double rawPy = yMap.xTransform(vy);
            if (!std::isfinite(px) || !std::isfinite(rawPy))
                continue;
            if (brk.xPixelInGap(px))
                continue;
            double xL = px - half_width;
            double xR = xL + bar_width;
            if (brk.hasX) {
                if (px <= brk.xLo && xR > brk.xLo)
                    xR = brk.xLo;
                else if (px >= brk.xHi && xL < brk.xHi)
                    xL = brk.xHi;
                if (xR - xL <= 0.5)
                    continue;
            }
            const double py = qBound(pyMin, rawPy, pyMax);
            const double top = qMin(py, basePy);
            const double bot = qMax(py, basePy);
            const double bw = xR - xL;
            if (!brk.hasY || bot <= brk.yLo || top >= brk.yHi) {
                painter->drawRect(QRectF(xL, top, bw, qMax(0.5, bot - top)));
            } else {
                if (top < brk.yLo)
                    painter->drawRect(QRectF(xL, top, bw, brk.yLo - top));
                if (bot > brk.yHi)
                    painter->drawRect(QRectF(xL, brk.yHi, bw, bot - brk.yHi));
            }
        }
    } else {
        const bool isLogX = xMap.transformation()
                && xMap.transformation()->type() == QwtScaleTransformation::Log10;
        const double pxS1 = xMap.xTransform(xMap.s1());
        const double pxS2 = xMap.xTransform(xMap.s2());
        const double pxMin = qMin(pxS1, pxS2);
        const double pxMax = qMax(pxS1, pxS2);
        const double baseVal = isLogX ? qMax(1e-100, qMin(xMap.s1(), xMap.s2())) : 0.0;
        const double basePx = qBound(pxMin, xMap.xTransform(baseVal), pxMax);

        if (dataSize() >= 2) {
            for (int i = 0; i + 1 < dataSize(); i++) {
                const double y0 = y(i);
                const double y1 = y(i + 1);
                if (brk.hasY && (brk.yValueInGap(y0) || brk.yValueInGap(y1)
                                 || brk.yValuesStraddle(y0, y1)))
                    continue;
                const double step = std::fabs(yMap.xTransform(y1) - yMap.xTransform(y0));
                if (step > 1e-6 && (dy <= 1e-6 || step < dy))
                    dy = step;
            }
        }
        if (dy <= 1e-6) {
            const double spanPx = std::fabs(yMap.xTransform(yMap.s2()) - yMap.xTransform(yMap.s1()));
            dy = qMax(8.0 * sc, spanPx / 5.0);
        }
        bar_width = qMax(2.0 * sc, dy * (1.0 - bar_gap * 0.01));
        const double half_width = (0.5 - bar_offset * 0.01) * bar_width;

        for (int i = from; i <= to; i++) {
            if (hasMasks && isPointMaskedFast(i, xCol, yCol))
                continue;
            const double vx = x(i);
            const double vy = y(i);
            if (brk.yValueInGap(vy))
                continue;
            const double rawPx = xMap.xTransform(vx);
            const double py = yMap.xTransform(vy);
            if (!std::isfinite(rawPx) || !std::isfinite(py))
                continue;
            if (brk.yPixelInGap(py))
                continue;
            double yT = py - half_width;
            double yB = yT + bar_width;
            if (brk.hasY) {
                if (py <= brk.yLo && yB > brk.yLo)
                    yB = brk.yLo;
                else if (py >= brk.yHi && yT < brk.yHi)
                    yT = brk.yHi;
                if (yB - yT <= 0.5)
                    continue;
            }
            const double px = qBound(pxMin, rawPx, pxMax);
            const double left = qMin(px, basePx);
            const double right = qMax(px, basePx);
            const double bh = yB - yT;
            if (!brk.hasX || right <= brk.xLo || left >= brk.xHi) {
                painter->drawRect(QRectF(left, yT, qMax(0.5, right - left), bh));
            } else {
                if (left < brk.xLo)
                    painter->drawRect(QRectF(left, yT, brk.xLo - left, bh));
                if (right > brk.xHi)
                    painter->drawRect(QRectF(brk.xHi, yT, right - brk.xHi, bh));
            }
        }
    }
    painter->restore();
}

QwtDoubleRect QwtBarCurve::boundingRect() const
{
    QwtDoubleRect rect = QwtPlotCurve::boundingRect();
    double n = (double)dataSize();
    if (n <= 0.0)
        return rect;

    if (bar_style == Vertical) {
        double dx = (n > 1.0) ? ((rect.right() - rect.left()) / (n - 1.0)) : 1.0;
        if (dx <= 0.0 || !std::isfinite(dx))
            dx = 1.0;
        rect.setLeft(rect.left() - 0.6 * dx);
        rect.setRight(rect.right() + 0.6 * dx);
        const bool yLog = plot() && Plot::isLog10ScaleEngine(plot()->axisScaleEngine(yAxis()));
        if (!yLog) {
            if (rect.top() > 0.0)
                rect.setTop(0.0);
            if (rect.bottom() < 0.0)
                rect.setBottom(0.0);
        }
    } else {
        double dy = (n > 1.0) ? ((rect.bottom() - rect.top()) / (n - 1.0)) : 1.0;
        if (dy <= 0.0 || !std::isfinite(dy))
            dy = 1.0;
        rect.setTop(rect.top() - 0.6 * dy);
        rect.setBottom(rect.bottom() + 0.6 * dy);
        const bool xLog = plot() && Plot::isLog10ScaleEngine(plot()->axisScaleEngine(xAxis()));
        if (!xLog) {
            if (rect.left() > 0.0)
                rect.setLeft(0.0);
            if (rect.right() < 0.0)
                rect.setRight(0.0);
        }
    }

    return rect;
}

void QwtBarCurve::setGap(int gap)
{
    if (bar_gap == gap)
        return;

    bar_gap = gap;
}

void QwtBarCurve::setOffset(int offset)
{
    if (d_stacked) {
        bar_offset = 0;
        return;
    }
    if (bar_offset == offset)
        return;

    bar_offset = offset;
}

double QwtBarCurve::dataOffset()
{
    if (!plot() || dataSize() < 1)
        return 0.0;

    const AxisBreakBands brk = axisBreakBands(plot()->canvasMap(xAxis()), plot()->canvasMap(yAxis()));

    if (bar_style == Vertical) {
        QwtScaleMap xMap = plot()->canvasMap(xAxis());
        if (xMap.pDist() < 2.0) {
            const_cast<QwtPlot *>(plot())->updateLayout();
            xMap = plot()->canvasMap(xAxis());
        }
        if (xMap.pDist() < 2.0)
            return 0.0;

        double dx = 0.0;
        if (dataSize() >= 2) {
            for (int i = 1; i < dataSize(); i++) {
                const double x0 = x(i - 1);
                const double x1 = x(i);
                if (brk.hasX && (brk.xValueInGap(x0) || brk.xValueInGap(x1)
                                 || brk.xValuesStraddle(x0, x1)))
                    continue;
                const double step = std::fabs(xMap.xTransform(x1) - xMap.xTransform(x0));
                if (step > 1e-6 && (dx <= 1e-6 || step < dx))
                    dx = step;
            }
        }
        if (dx <= 1e-6)
            dx = std::fabs(xMap.xTransform(xMap.s2()) - xMap.xTransform(xMap.s1())) / 5.0;

        const double bar_width = dx * (1.0 - bar_gap * 0.01);
        const double x1 = xMap.xTransform(minXValue()) + bar_offset * 0.01 * bar_width;
        const double inv = xMap.invTransform(x1) - minXValue();
        return std::isfinite(inv) ? inv : 0.0;
    } else {
        QwtScaleMap yMap = plot()->canvasMap(yAxis());
        if (yMap.pDist() < 2.0) {
            const_cast<QwtPlot *>(plot())->updateLayout();
            yMap = plot()->canvasMap(yAxis());
        }
        if (yMap.pDist() < 2.0)
            return 0.0;

        double dy = 0.0;
        if (dataSize() >= 2) {
            for (int i = 1; i < dataSize(); i++) {
                const double y0 = y(i - 1);
                const double y1 = y(i);
                if (brk.hasY && (brk.yValueInGap(y0) || brk.yValueInGap(y1)
                                 || brk.yValuesStraddle(y0, y1)))
                    continue;
                const double step = std::fabs(yMap.xTransform(y1) - yMap.xTransform(y0));
                if (step > 1e-6 && (dy <= 1e-6 || step < dy))
                    dy = step;
            }
        }
        if (dy <= 1e-6)
            dy = std::fabs(yMap.xTransform(yMap.s2()) - yMap.xTransform(yMap.s1())) / 5.0;

        const double bar_width = dy * (1.0 - bar_gap * 0.01);
        const double y1 = yMap.xTransform(minYValue()) + bar_offset * 0.01 * bar_width;
        const double inv = yMap.invTransform(y1) - minYValue();
        return std::isfinite(inv) ? inv : 0.0;
    }
}
