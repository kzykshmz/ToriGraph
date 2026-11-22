/***************************************************************************
    File                 : QwtPieCurve.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Pie plot class

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
#include "QwtPieCurve.h"
#include "ColorButton.h"
#include "Legend.h"
#include "Graph.h"
#include "Plot.h"
#include "Table.h"
#include "core/column/Column.h"

#include <QPaintDevice>
#include <QPainter>
#include <QVarLengthArray>
#include <QVariant>
#include <QLocale>
#include <cmath>

#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_map.h>
#include <qwt_text.h>

namespace {
// Pie slices use the canonical Okabe-Ito definition (Graph::okabeItoColors()).
QColor okabeItoPieColor(int i)
{
    const QList<QColor> &cols = Graph::okabeItoColors();
    return cols.at(qMax(0, i) % cols.size());
}
} // namespace

QwtPieCurve::QwtPieCurve(Table *t, const QString &name, int startRow, int endRow)
    : DataCurve(t, QString(), name, startRow, endRow)
{
    d_pie_ray = 100;
    d_first_color = 0;
    QPen slicePen(QColor(255, 255, 255), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    slicePen.setCosmetic(false);
    setPen(slicePen);
    setStyle(QwtPlotCurve::NoCurve);
    setBrush(QBrush(okabeItoPieColor(0), Qt::SolidPattern));

    setType(Graph::Pie);
}

void QwtPieCurve::draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
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

    drawPie(painter, xMap, yMap, from, to);
}

void QwtPieCurve::drawPie(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                          int from, int to) const
{
    QRectF canvasRectF;
    if (std::fabs(xMap.pDist()) >= 4.0 && std::fabs(yMap.pDist()) >= 4.0) {
        canvasRectF = QRectF(qMin(xMap.p1(), xMap.p2()), qMin(yMap.p1(), yMap.p2()),
                             std::fabs(xMap.pDist()), std::fabs(yMap.pDist()));
    } else if (plot() && plot()->plotLayout()) {
        canvasRectF = QRectF(plot()->plotLayout()->canvasRect());
    }
    if (canvasRectF.width() <= 2.0 || canvasRectF.height() <= 2.0)
        return;

    const Graph *g = plot() ? qobject_cast<const Graph *>(plot()->parent()) : nullptr;
    Graph *mg = const_cast<Graph *>(g);
    const bool hasVisLeg = mg && mg->hasLegend() && g->isLegendVisible();
    ::Legend *leg = hasVisLeg ? mg->legend() : nullptr;

    bool hasAnyPieCallout = false;
    if (mg) {
        const QVector<int> tKeys = mg->textMarkerKeys();
        for (int tid : tKeys) {
            ::Legend *tm = mg->textMarker(tid);
            if (tm && tm != leg && tm->isVisible() && !tm->text().trimmed().isEmpty()) {
                hasAnyPieCallout = true;
                break;
            }
        }
    }

    double radius = (hasAnyPieCallout ? 0.285 : 0.34)
            * qMin(canvasRectF.width(), canvasRectF.height());
    double pieCx = canvasRectF.center().x();
    double pieCy = canvasRectF.center().y();

    if (hasVisLeg && leg && g->plotWidget()) {
        const QRect screenCr = g->plotWidget()->plotLayout()
                ? g->plotWidget()->plotLayout()->canvasRect()
                : QRect();
        const int fallbackCw =
                g->plotWidget()->canvas() ? g->plotWidget()->canvas()->width() : 320;
        const int fallbackCh =
                g->plotWidget()->canvas() ? g->plotWidget()->canvas()->height() : 320;
        const double sCw = double(qMax(40, screenCr.width() > 20 ? screenCr.width() : fallbackCw));
        const double sCh =
                double(qMax(40, screenCr.height() > 20 ? screenCr.height() : fallbackCh));

        QRect lr = leg->rect();
        if (lr.width() <= 4 || lr.height() <= 4) {
            QwtText qt(leg->text());
            qt.setFont(leg->font());
            const QSize tsz = qt.textSize();
            lr = QRect(4, 4, qMax(24, tsz.width() + 28), qMax(16, tsz.height() + 8));
        }
        const double legWFrac = double(lr.width()) / sCw;
        const QVariant ancProp = g->plotWidget()->property("_toriLegendAnchor");
        const bool topBandLegend =
                (ancProp.isValid() && ancProp.toInt() == int(Graph::LegendTopCenter))
                || (legWFrac > 0.34);
        if (topBandLegend) {
            const double legBottomFrac =
                    qBound(0.08, double(qMax(4, lr.top()) + lr.height()) / sCh, 0.52);
            const double legBottom = legBottomFrac * canvasRectF.height();
            const double pad = 0.028 * canvasRectF.height();
            const double availH = qMax(0.20 * canvasRectF.height(),
                                       canvasRectF.height() - legBottom - 2.0 * pad);
            const double rScaleW = hasAnyPieCallout ? 0.255 : 0.31;
            const double rScaleH = hasAnyPieCallout ? 0.34 : 0.44;
            radius = qMax(10.0, qMin(rScaleW * canvasRectF.width(), rScaleH * availH));
            pieCx = canvasRectF.center().x();
            pieCy = canvasRectF.top() + legBottom + pad + 0.5 * availH;
        } else {
            radius = (hasAnyPieCallout ? 0.24 : 0.285)
                    * qMin(canvasRectF.width(), canvasRectF.height());
            pieCx = canvasRectF.left() + 0.36 * canvasRectF.width();
            pieCy = canvasRectF.top() + 0.52 * canvasRectF.height();
        }
    }
    const QRectF pieRect(pieCx - radius, pieCy - radius, 2.0 * radius, 2.0 * radius);

    double sum = 0.0;
    for (int i = from; i <= to; i++) {
        const double yi = y(i);
        if (std::isfinite(yi) && yi > 0.0)
            sum += yi;
    }
    if (sum <= 0.0)
        return;

    const double sc = Plot::printDeviceScale();
    QPen slicePen = QwtPlotCurve::pen();
    slicePen.setCosmetic(false);
    slicePen.setWidthF(qMax(1.0, slicePen.widthF()) * sc);

    int angle = int(5760 * 0.75);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(slicePen);
    for (int i = from; i <= to; i++) {
        const double yi = y(i);
        if (!std::isfinite(yi) || yi <= 0.0)
            continue;
        const int value = (i == to) ? qMax(0, int(5760 * 0.75) + 5760 - angle)
                                    : qRound(yi / sum * 5760.0);
        painter->setBrush(QBrush(color(i), QwtPlotCurve::brush().style()));
        painter->drawPie(pieRect, -angle, -value);
        angle += value;
    }
    painter->restore();
}

QColor QwtPieCurve::color(int i) const
{
    if (d_first_color == 0)
        return okabeItoPieColor(i);
    int index = (d_first_color + qMax(0, i)) % ColorButton::colors_count;
    return ColorButton::color(index);
}

void QwtPieCurve::setBrushStyle(const Qt::BrushStyle &style)
{
    QBrush br = QwtPlotCurve::brush();
    if (br.style() == style)
        return;

    br.setStyle(style);
    setBrush(br);
}

bool QwtPieCurve::loadData()
{
    if (!d_table)
        return false;
    Column *x_dummy = nullptr;
    Column *y_col_ptr = nullptr;
    resolveColumns(x_dummy, y_col_ptr);
    int ycol = d_table->colIndex(title().text());
    if (!y_col_ptr && ycol >= 0 && ycol < d_table->numCols())
        y_col_ptr = d_table->column(ycol);
    if (!y_col_ptr)
        return false;

    const int startR = qMax(0, d_start_row);
    const int endR = (d_end_row < 0) ? (y_col_ptr->rowCount() - 1)
                                     : qMin(d_end_row, y_col_ptr->rowCount() - 1);
    if (startR > endR)
        return false;

    QVarLengthArray<double> Y(endR - startR + 1);
    int size = 0;
    const auto yColType = (ycol >= 0 && ycol < d_table->numCols())
            ? d_table->columnType(ycol)
            : y_col_ptr->columnMode();

    for (int row = startR; row <= endR; row++) {
        if (!y_col_ptr->isInvalid(row) && !y_col_ptr->isMasked(row)) {
            double val = 0.0;
            if (yColType == SciDAVis::ColumnMode::Text) {
                const QString yval = y_col_ptr->textAt(row);
                bool valid_data = false;
                val = QLocale().toDouble(yval, &valid_data);
                if (!valid_data)
                    val = QLocale::c().toDouble(yval, &valid_data);
                if (!valid_data)
                    continue;
            } else {
                val = y_col_ptr->valueAt(row);
            }
            if (!std::isfinite(val) || val < 0.0)
                continue;
            Y[size++] = val;
        }
    }
    Y.resize(size);
    setData(Y.data(), Y.data(), size);

    return true;
}

void QwtPieCurve::updateBoundingRect()
{
    if (!plot())
        return;

    QwtScaleMap xMap = plot()->canvasMap(xAxis());
    int x_center = (xMap.p1() + xMap.p2()) / 2;
    int x_left = x_center - d_pie_ray;
    d_left_coord = xMap.invTransform(x_left);
}
