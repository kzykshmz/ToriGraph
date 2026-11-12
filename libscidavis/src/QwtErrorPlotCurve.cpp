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
    setPen(e->pen());
    err = e->err;
}

void QwtErrorPlotCurve::draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                             int from, int to) const
{
    if (!painter || dataSize() <= 0)
        return;

    if (to < 0)
        to = dataSize() - 1;

    painter->save();
    painter->setPen(pen());
    drawErrorBars(painter, xMap, yMap, from, to);
    painter->restore();
}

void QwtErrorPlotCurve::drawErrorBars(QPainter *painter, const QwtScaleMap &xMap,
                                      const QwtScaleMap &yMap, int from, int to) const
{
    if (!d_master_curve)
        return;

    if (testRenderHint(QwtPlotItem::RenderAntialiased))
        painter->setRenderHint(QPainter::Antialiasing, true);

    double sh = 0.0;
    const QwtSymbol symbol = d_master_curve->symbol();
    if (symbol.style() != QwtSymbol::NoSymbol)
        sh = double(symbol.size().height());

    double d_xOffset = 0.0;
    double d_yOffset = 0.0;
    if (d_master_curve->type() == Graph::VerticalBars)
        d_xOffset = ((QwtBarCurve *)d_master_curve)->dataOffset();
    else if (d_master_curve->type() == Graph::HorizontalBars)
        d_yOffset = ((QwtBarCurve *)d_master_curve)->dataOffset();

    const double halfCap = 0.5 * double(cap);
    const double halfSym = 0.5 * sh;

    for (int i = from; i <= to && i < err.size(); i++) {
        const double xi = xMap.xTransform(x(i) + d_xOffset);
        const double yi = yMap.xTransform(y(i) + d_yOffset);
        if (!std::isfinite(xi) || !std::isfinite(yi))
            continue;

        if (type == Vertical) {
            double y_plus = yMap.xTransform(y(i) + err[i]);
            double y_minus = yMap.xTransform(y(i) - err[i]);
            bool y_minus_is_finite = std::isfinite(y_minus);

            if (yMap.transformation()->type() == QwtScaleTransformation::Log10 && err[i] >= y(i)) {
                y_minus = yMap.xTransform(qMin(yMap.s1(), yMap.s2()));
                y_minus_is_finite = false;
            }
            if (!std::isfinite(y_plus) || !std::isfinite(y_minus))
                continue;

            if (plus)
                painter->drawLine(QPointF(xi - halfCap, y_plus), QPointF(xi + halfCap, y_plus));
            if (minus && y_minus_is_finite)
                painter->drawLine(QPointF(xi - halfCap, y_minus), QPointF(xi + halfCap, y_minus));

            if (through) {
                if (plus && minus)
                    painter->drawLine(QPointF(xi, y_minus), QPointF(xi, y_plus));
                else if (plus)
                    painter->drawLine(QPointF(xi, yi), QPointF(xi, y_plus));
                else if (minus)
                    painter->drawLine(QPointF(xi, y_minus), QPointF(xi, yi));
            } else if (y_plus <= y_minus) {
                if (plus && y_plus < yi - halfSym)
                    painter->drawLine(QPointF(xi, yi - halfSym), QPointF(xi, y_plus));
                if (minus && y_minus > yi + halfSym)
                    painter->drawLine(QPointF(xi, yi + halfSym), QPointF(xi, y_minus));
            } else {
                if (plus && y_plus > yi + halfSym)
                    painter->drawLine(QPointF(xi, yi + halfSym), QPointF(xi, y_plus));
                if (minus && y_minus < yi - halfSym)
                    painter->drawLine(QPointF(xi, yi - halfSym), QPointF(xi, y_minus));
            }
        } else if (type == Horizontal) {
            double x_plus = xMap.xTransform(x(i) + err[i]);
            double x_minus = xMap.xTransform(x(i) - err[i]);
            bool x_minus_is_finite = std::isfinite(x_minus);

            if (xMap.transformation()->type() == QwtScaleTransformation::Log10 && err[i] >= x(i)) {
                x_minus = xMap.xTransform(qMin(xMap.s1(), xMap.s2()));
                x_minus_is_finite = false;
            }
            if (!std::isfinite(x_plus) || !std::isfinite(x_minus))
                continue;

            if (plus)
                painter->drawLine(QPointF(x_plus, yi - halfCap), QPointF(x_plus, yi + halfCap));
            if (minus && x_minus_is_finite)
                painter->drawLine(QPointF(x_minus, yi - halfCap), QPointF(x_minus, yi + halfCap));

            if (through) {
                if (plus && minus)
                    painter->drawLine(QPointF(x_minus, yi), QPointF(x_plus, yi));
                else if (plus)
                    painter->drawLine(QPointF(xi, yi), QPointF(x_plus, yi));
                else if (minus)
                    painter->drawLine(QPointF(x_minus, yi), QPointF(xi, yi));
            } else if (x_plus >= x_minus) {
                if (plus && x_plus > xi + halfSym)
                    painter->drawLine(QPointF(xi + halfSym, yi), QPointF(x_plus, yi));
                if (minus && x_minus < xi - halfSym)
                    painter->drawLine(QPointF(xi - halfSym, yi), QPointF(x_minus, yi));
            } else {
                if (plus && x_plus < xi - halfSym)
                    painter->drawLine(QPointF(xi - halfSym, yi), QPointF(x_plus, yi));
                if (minus && x_minus > xi + halfSym)
                    painter->drawLine(QPointF(xi + halfSym, yi), QPointF(x_minus, yi));
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
    p.setWidth(w);
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
