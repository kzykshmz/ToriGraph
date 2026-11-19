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
    const double sc = Plot::printDeviceScale();
    QPen barPen = QwtPlotCurve::pen();
    if (barPen.style() != Qt::NoPen) {
        barPen.setCosmetic(false);
        barPen.setWidthF(qMax(1.0, barPen.widthF()) * sc);
    }
    painter->setPen(barPen);
    painter->setBrush(QwtPlotCurve::brush());

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
            dx = std::fabs(xMap.xTransform(x(1)) - xMap.xTransform(x(0)));
            for (int i = 1; i + 1 < dataSize(); i++) {
                const double step = std::fabs(xMap.xTransform(x(i + 1)) - xMap.xTransform(x(i)));
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
            const double px = xMap.xTransform(x(i));
            const double py = qBound(pyMin, yMap.xTransform(y(i)), pyMax);
            if (!std::isfinite(px) || !std::isfinite(py))
                continue;
            const double top = qMin(py, basePy);
            const double h = std::fabs(basePy - py);
            painter->drawRect(QRectF(px - half_width, top, bar_width, qMax(0.5, h)));
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
            dy = std::fabs(yMap.xTransform(y(1)) - yMap.xTransform(y(0)));
            for (int i = 1; i + 1 < dataSize(); i++) {
                const double step = std::fabs(yMap.xTransform(y(i + 1)) - yMap.xTransform(y(i)));
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
            const double px = qBound(pxMin, xMap.xTransform(x(i)), pxMax);
            const double py = yMap.xTransform(y(i));
            if (!std::isfinite(px) || !std::isfinite(py))
                continue;
            const double left = qMin(px, basePx);
            const double w = std::fabs(px - basePx);
            painter->drawRect(QRectF(left, py - half_width, qMax(0.5, w), bar_width));
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
            dx = std::fabs(xMap.xTransform(x(1)) - xMap.xTransform(x(0)));
            for (int i = 2; i < dataSize(); i++) {
                const double step = std::fabs(xMap.xTransform(x(i)) - xMap.xTransform(x(i - 1)));
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
            dy = std::fabs(yMap.xTransform(y(1)) - yMap.xTransform(y(0)));
            for (int i = 2; i < dataSize(); i++) {
                const double step = std::fabs(yMap.xTransform(y(i)) - yMap.xTransform(y(i - 1)));
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
