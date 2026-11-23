/***************************************************************************
    File                 : QwtErrorPlotCurve.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Error bars curve

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
#include "QwtErrorPlotCurve.h"
#include "QwtBarCurve.h"
#include "core/column/Column.h"

#include <qwt_painter.h>
#include <qwt_symbol.h>

#include <QPainter>
#include <QLocale>
#include <cmath>

QwtErrorPlotCurve::QwtErrorPlotCurve(int orientation, Table *t, const QString &name)
    : DataCurve(t, QString(), name), d_master_curve(NULL)
{
    cap = 10;
    type = orientation;
    plus = true;
    minus = true;
    through = false;
    setType(Graph::ErrorBars);
}

QwtErrorPlotCurve::QwtErrorPlotCurve(Table *t, const QString &name)
    : DataCurve(t, QString(), name), d_master_curve(NULL)
{
    cap = 10;
    type = Vertical;
    plus = true;
    minus = true;
    through = false;
    setType(Graph::ErrorBars);
}

void QwtErrorPlotCurve::copy(const QwtErrorPlotCurve *e)
{
    cap = e->cap;
    type = e->type;
    plus = e->plus;
    minus = e->minus;
    through = e->through;
    d_band = e->d_band;
    d_band_alpha = e->d_band_alpha;
    setPen(e->pen());
    err = e->err;
}

QColor QwtErrorPlotCurve::bandFillColor() const
{
    QColor c = pen().color();
    c.setAlpha(d_band_alpha);
    return c;
}

void QwtErrorPlotCurve::drawErrorBand(QPainter *painter, const QwtScaleMap &xMap,
                                      const QwtScaleMap &yMap, int from, int to) const
{
    if (!d_master_curve || !painter)
        return;
    const bool vertical = (type != Horizontal);
    const QwtScaleMap &valMap = vertical ? yMap : xMap;
    const bool logVal = valMap.transformation()
            && valMap.transformation()->type() == QwtScaleTransformation::Log10;
    const double logFloor = qMin(valMap.s1(), valMap.s2());
    const AxisBreakBands brk = axisBreakBands(xMap, yMap);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(bandFillColor()));

    // One closed polygon per run of consecutive valid points: the upper edge forwards,
    // then the lower edge backwards.
    QVector<QPointF> upper, lower;
    auto flush = [&]() {
        if (upper.size() >= 2) {
            QPolygonF poly;
            poly.reserve(upper.size() * 2);
            for (const QPointF &pt : upper)
                poly << pt;
            for (int k = lower.size() - 1; k >= 0; --k)
                poly << lower[k];
            if (brk.any()) {
                const QVector<QPolygonF> pieces = clipPolygonToBreaks(poly, brk);
                for (const QPolygonF &piece : pieces)
                    painter->drawPolygon(piece);
            } else {
                painter->drawPolygon(poly);
            }
        }
        upper.clear();
        lower.clear();
    };

    bool hasPrev = false;
    double prevVx = 0.0, prevVy = 0.0, prevIndPx = 0.0;
    for (int i = from; i <= to && i < err.size(); i++) {
        const double vx = x(i);
        const double vy = y(i);
        const double v = vertical ? vy : vx;
        const double e = err[i];
        if (!std::isfinite(v) || !std::isfinite(e) || !std::isfinite(vx) || !std::isfinite(vy)
            || brk.valueInGap(vx, vy)) {
            flush();
            hasPrev = false;
            continue;
        }
        double hi = plus ? v + e : v;
        double lo = minus ? v - e : v;
        if (logVal) {
            if (hi <= 0.0) {
                flush();
                hasPrev = false;
                continue;
            }
            if (lo <= 0.0)
                lo = logFloor;
        }
        if (vertical) {
            const double px = xMap.xTransform(vx);
            const double pHi = yMap.xTransform(hi);
            const double pLo = yMap.xTransform(lo);
            if (!std::isfinite(px) || !std::isfinite(pHi) || !std::isfinite(pLo)
                || brk.xPixelInGap(px)) {
                flush();
                hasPrev = false;
                continue;
            }
            if (hasPrev && (brk.xValuesStraddle(prevVx, vx)
                            || (brk.hasX && ((prevIndPx <= brk.xLo && px >= brk.xHi)
                                             || (prevIndPx >= brk.xHi && px <= brk.xLo))))) {
                flush();
            }
            upper << QPointF(px, pHi);
            lower << QPointF(px, pLo);
            prevIndPx = px;
        } else {
            const double py = yMap.xTransform(vy);
            const double pHi = xMap.xTransform(hi);
            const double pLo = xMap.xTransform(lo);
            if (!std::isfinite(py) || !std::isfinite(pHi) || !std::isfinite(pLo)
                || brk.yPixelInGap(py)) {
                flush();
                hasPrev = false;
                continue;
            }
            if (hasPrev && (brk.yValuesStraddle(prevVy, vy)
                            || (brk.hasY && ((prevIndPx <= brk.yLo && py >= brk.yHi)
                                             || (prevIndPx >= brk.yHi && py <= brk.yLo))))) {
                flush();
            }
            upper << QPointF(pHi, py);
            lower << QPointF(pLo, py);
            prevIndPx = py;
        }
        prevVx = vx;
        prevVy = vy;
        hasPrev = true;
    }
    flush();
}

void QwtErrorPlotCurve::draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                             int from, int to) const
{
    if (!painter || dataSize() <= 0)
        return;

    if (to < 0)
        to = dataSize() - 1;

    painter->save();
    if (d_band) {
        drawErrorBand(painter, xMap, yMap, from, to);
    } else {
        painter->setPen(pen());
        drawErrorBars(painter, xMap, yMap, from, to);
    }
    painter->restore();
}

void QwtErrorPlotCurve::drawErrorBars(QPainter *painter, const QwtScaleMap &xMap,
                                      const QwtScaleMap &yMap, int from, int to) const
{
    if (!d_master_curve)
        return;

    if (testRenderHint(QwtPlotItem::RenderAntialiased))
        painter->setRenderHint(QPainter::Antialiasing, true);

    const double sc = renderPenScale();
    QPen p = pen();
    if (p.style() != Qt::NoPen) {
        double wf = p.widthF();
        if (wf <= 0.0)
            wf = (p.width() > 0) ? double(p.width()) : 1.0;
        p.setCosmetic(false);
        p.setWidthF(qMax(0.1, wf) * sc);
    }
    painter->setPen(p);

    double sh = 0.0;
    const QwtSymbol symbol = d_master_curve->symbol();
    if (symbol.style() != QwtSymbol::NoSymbol) {
        const double baseSym = (type == Horizontal) ? double(symbol.size().width())
                                                    : double(symbol.size().height());
        sh = baseSym * sc;
    }

    double d_xOffset = 0.0;
    double d_yOffset = 0.0;
    if (d_master_curve->type() == Graph::VerticalBars)
        d_xOffset = ((QwtBarCurve *)d_master_curve)->dataOffset();
    else if (d_master_curve->type() == Graph::HorizontalBars)
        d_yOffset = ((QwtBarCurve *)d_master_curve)->dataOffset();

    const double halfCap = 0.5 * double(cap) * sc;
    const double halfSym = 0.5 * sh;
    const AxisBreakBands brk = axisBreakBands(xMap, yMap);

    QVector<QLineF> segs;
    segs.reserve(4);
    auto drawBreakAwareLine = [&](const QPointF &a, const QPointF &b) {
        if (!brk.any()) {
            painter->drawLine(QLineF(a, b));
            return;
        }
        segs.clear();
        appendSegmentOutsideBreaks(QLineF(a, b), brk, segs);
        if (!segs.isEmpty())
            painter->drawLines(segs);
    };

    for (int i = from; i <= to && i < err.size(); i++) {
        const double vx = x(i) + d_xOffset;
        const double vy = y(i) + d_yOffset;
        if (brk.valueInGap(vx, vy))
            continue;
        const double xi = xMap.xTransform(vx);
        const double yi = yMap.xTransform(vy);
        if (!std::isfinite(xi) || !std::isfinite(yi))
            continue;
        if (brk.pixelInGap(QPointF(xi, yi)))
            continue;

        if (type == Vertical) {
            const double yPlusVal = y(i) + err[i];
            const double yMinusVal = y(i) - err[i];
            double y_plus = yMap.xTransform(yPlusVal);
            double y_minus = yMap.xTransform(yMinusVal);
            bool y_minus_is_finite = std::isfinite(y_minus);

            if (yMap.transformation()
                && yMap.transformation()->type() == QwtScaleTransformation::Log10
                && err[i] >= y(i)) {
                y_minus = yMap.xTransform(qMin(yMap.s1(), yMap.s2()));
                y_minus_is_finite = false;
            }
            if (!std::isfinite(y_plus) || !std::isfinite(y_minus))
                continue;

            if (plus && !brk.yValueInGap(yPlusVal))
                drawBreakAwareLine(QPointF(xi - halfCap, y_plus), QPointF(xi + halfCap, y_plus));
            if (minus && y_minus_is_finite && !brk.yValueInGap(yMinusVal))
                drawBreakAwareLine(QPointF(xi - halfCap, y_minus), QPointF(xi + halfCap, y_minus));

            if (through) {
                if (plus && minus)
                    drawBreakAwareLine(QPointF(xi, y_minus), QPointF(xi, y_plus));
                else if (plus)
                    drawBreakAwareLine(QPointF(xi, yi), QPointF(xi, y_plus));
                else if (minus)
                    drawBreakAwareLine(QPointF(xi, y_minus), QPointF(xi, yi));
            } else if (y_plus <= y_minus) {
                if (plus && y_plus < yi - halfSym)
                    drawBreakAwareLine(QPointF(xi, yi - halfSym), QPointF(xi, y_plus));
                if (minus && y_minus > yi + halfSym)
                    drawBreakAwareLine(QPointF(xi, yi + halfSym), QPointF(xi, y_minus));
            } else {
                if (plus && y_plus > yi + halfSym)
                    drawBreakAwareLine(QPointF(xi, yi + halfSym), QPointF(xi, y_plus));
                if (minus && y_minus < yi - halfSym)
                    drawBreakAwareLine(QPointF(xi, yi - halfSym), QPointF(xi, y_minus));
            }
        } else if (type == Horizontal) {
            const double xPlusVal = x(i) + err[i];
            const double xMinusVal = x(i) - err[i];
            double x_plus = xMap.xTransform(xPlusVal);
            double x_minus = xMap.xTransform(xMinusVal);
            bool x_minus_is_finite = std::isfinite(x_minus);

            if (xMap.transformation()
                && xMap.transformation()->type() == QwtScaleTransformation::Log10
                && err[i] >= x(i)) {
                x_minus = xMap.xTransform(qMin(xMap.s1(), xMap.s2()));
                x_minus_is_finite = false;
            }
            if (!std::isfinite(x_plus) || !std::isfinite(x_minus))
                continue;

            if (plus && !brk.xValueInGap(xPlusVal))
                drawBreakAwareLine(QPointF(x_plus, yi - halfCap), QPointF(x_plus, yi + halfCap));
            if (minus && x_minus_is_finite && !brk.xValueInGap(xMinusVal))
                drawBreakAwareLine(QPointF(x_minus, yi - halfCap), QPointF(x_minus, yi + halfCap));

            if (through) {
                if (plus && minus)
                    drawBreakAwareLine(QPointF(x_minus, yi), QPointF(x_plus, yi));
                else if (plus)
                    drawBreakAwareLine(QPointF(xi, yi), QPointF(x_plus, yi));
                else if (minus)
                    drawBreakAwareLine(QPointF(x_minus, yi), QPointF(xi, yi));
            } else if (x_plus >= x_minus) {
                if (plus && x_plus > xi + halfSym)
                    drawBreakAwareLine(QPointF(xi + halfSym, yi), QPointF(x_plus, yi));
                if (minus && x_minus < xi - halfSym)
                    drawBreakAwareLine(QPointF(xi - halfSym, yi), QPointF(x_minus, yi));
            } else {
                if (plus && x_plus < xi - halfSym)
                    drawBreakAwareLine(QPointF(xi - halfSym, yi), QPointF(x_plus, yi));
                if (minus && x_minus > xi + halfSym)
                    drawBreakAwareLine(QPointF(xi + halfSym, yi), QPointF(x_minus, yi));
            }
        }
    }
}

double QwtErrorPlotCurve::errorValue(int i)
{
    if (i >= 0 && i < dataSize())
        return err[i];
    else
        return 0.0;
}

bool QwtErrorPlotCurve::xErrors()
{
    bool x = false;
    if (type == Horizontal)
        x = true;

    return x;
}

void QwtErrorPlotCurve::setXErrors(bool yes)
{
    if (yes)
        type = Horizontal;
    else
        type = Vertical;
}

void QwtErrorPlotCurve::setWidth(int w)
{
    QPen p = pen();
    p.setCosmetic(false);
    p.setWidthF(double(w));
    setPen(p);
}

void QwtErrorPlotCurve::setWidthF(double w)
{
    QPen p = pen();
    p.setCosmetic(false);
    p.setWidthF(qMax(0.1, w));
    setPen(p);
}

void QwtErrorPlotCurve::setColor(const QColor &c)
{
    QPen p = pen();
    p.setColor(c);
    setPen(p);
}

QwtDoubleRect QwtErrorPlotCurve::boundingRect() const
{
    QwtDoubleRect rect = QwtPlotCurve::boundingRect();

    int size = dataSize();

    QwtArray<double> X(size), Y(size), min(size), max(size);
    for (int i = 0; i < size; i++) {
        X[i] = x(i);
        Y[i] = y(i);
        if (type == Vertical) {
            min[i] = y(i) - err[i];
            max[i] = y(i) + err[i];
        } else {
            min[i] = x(i) - err[i];
            max[i] = x(i) + err[i];
        }
    }

    QwtArrayData *erMin, *erMax;
    if (type == Vertical) {
        erMin = new QwtArrayData(X, min);
        erMax = new QwtArrayData(X, max);
    } else {
        erMin = new QwtArrayData(min, Y);
        erMax = new QwtArrayData(max, Y);
    }

    QwtDoubleRect minrect = erMin->boundingRect();
    QwtDoubleRect maxrect = erMax->boundingRect();

    rect.setTop(qMin(minrect.top(), maxrect.top()));
    rect.setBottom(qMax(minrect.bottom(), maxrect.bottom()));
    rect.setLeft(qMin(minrect.left(), maxrect.left()));
    rect.setRight(qMax(minrect.right(), maxrect.right()));

    delete erMin;
    delete erMax;

    return rect;
}

void QwtErrorPlotCurve::setMasterCurve(DataCurve *c)
{
    if (!c || d_master_curve == c)
        return;

    d_master_curve = c;
    setAxis(c->xAxis(), c->yAxis());
    d_start_row = c->startRow();
    d_end_row = c->endRow();
    c->addErrorBars(this);

    loadData();
}

bool QwtErrorPlotCurve::loadData()
{
    if (!d_master_curve)
        return false;
    Table *mt = d_master_curve->table();
    if (!mt)
        return false;
    Column *x = mt->column(d_master_curve->xColumnName());
    Column *y = mt->column(d_master_curve->title().text());

    Column *err = d_table->column(title().text());

    if (!x || !y || !err) {
        remove();
        return false;
    }

    QList<QVector<double>> data = convertData(
            d_master_curve->type() == Graph::HorizontalBars ? (QList<Column *>() << y << x << err)
                                                            : (QList<Column *>() << x << y << err),
            QList<int>() << xAxis() << yAxis() << (type == Horizontal ? xAxis() : yAxis()));

    if (data.isEmpty() || data[0].size() == 0) {
        remove();
        return false;
    }

    setData(data[0].data(), data[1].data(), data[0].size());
    setErrors(data[2]);

    return true;
}

QString QwtErrorPlotCurve::plotAssociation()
{
    if (!d_master_curve)
        return QString();

    QString base = d_master_curve->xColumnName() + "(X)," + d_master_curve->title().text() + "(Y),"
            + title().text();
    if (type == Horizontal)
        return base + "(xErr)";
    else
        return base + "(yErr)";
}

bool QwtErrorPlotCurve::updateData(Table *t, const QString &colName)
{
    if (d_table != t || colName != title().text())
        return false;

    loadData();
    return true;
}
