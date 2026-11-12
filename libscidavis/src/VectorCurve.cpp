/***************************************************************************
    File                 : VectorCurve.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Vector curve class

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
#include "VectorCurve.h"
#include "core/column/Column.h"

#include <qwt_painter.h>
#include <qwt_double_rect.h>
#include <QDateTime>
#include <QPainter>
#include <QLocale>
#include <cmath>

namespace {

int resolveVectorColIndex(Table *table, const QString &spec)
{
    if (!table || !table->d_future_table || spec.isEmpty())
        return -1;

    const int numCols = table->numCols();
    const QString tblPrefix = QString(table->name()) + QLatin1Char('_');
    if (spec.startsWith(tblPrefix)) {
        const QString shortName = spec.mid(tblPrefix.size());
        for (int i = 0; i < numCols; ++i) {
            if (const Column *col = table->column(i)) {
                if (col->name() == shortName)
                    return i;
            }
        }
    }

    for (int i = 0; i < numCols; ++i) {
        if (const Column *col = table->column(i)) {
            if (col->name() == spec)
                return i;
        }
    }

    const int fallback = table->colIndex(spec);
    if (fallback >= 0 && fallback < numCols && table->column(fallback) != nullptr)
        return fallback;
    return -1;
}

bool matchesVectorColName(Table *table, const QString &storedSpec, const QString &colName)
{
    if (storedSpec == colName)
        return true;
    if (!table || storedSpec.isEmpty() || colName.isEmpty())
        return false;
    const QString tblPrefix = QString(table->name()) + QLatin1Char('_');
    if (colName.startsWith(tblPrefix) && storedSpec == colName.mid(tblPrefix.size()))
        return true;
    if (storedSpec.startsWith(tblPrefix) && storedSpec.mid(tblPrefix.size()) == colName)
        return true;
    return false;
}

bool extractFiniteVectorValue(const Column *col, int row, double &outVal)
{
    if (!col || row < 0 || row >= col->rowCount())
        return false;
    if (col->isInvalid(row) || col->isMasked(row))
        return false;

    switch (col->columnMode()) {
    case SciDAVis::ColumnMode::Numeric: {
        const double v = col->valueAt(row);
        if (!std::isfinite(v))
            return false;
        outVal = v;
        return true;
    }
    case SciDAVis::ColumnMode::Text: {
        const QString txt = col->textAt(row).trimmed();
        if (txt.isEmpty())
            return false;
        bool ok = false;
        double v = QLocale().toDouble(txt, &ok);
        if (!ok)
            v = QLocale::c().toDouble(txt, &ok);
        if (!ok || !std::isfinite(v))
            return false;
        outVal = v;
        return true;
    }
    case SciDAVis::ColumnMode::DateTime:
    case SciDAVis::ColumnMode::Month:
    case SciDAVis::ColumnMode::Day: {
        const QDateTime dt = col->dateTimeAt(row);
        if (!dt.isValid())
            return false;
        outVal = dt.toMSecsSinceEpoch() / 86400000.0 + 2440587.5;
        return std::isfinite(outVal);
    }
    default:
        return false;
    }
}

} // namespace

VectorCurve::VectorCurve(VectorStyle style, Table *t, const QString &xColName, QString name,
                         const QString &endCol1, const QString &endCol2, int startRow, int endRow)
    : DataCurve(t, xColName, name, startRow, endRow), vectorEnd(nullptr)
{
    switch (style) {
    case XYXY:
        d_type = Graph::VectXYXY;
        break;
    case XYAM:
        d_type = Graph::VectXYAM;
        break;
    }
    pen = QPen(Qt::black, 1, Qt::SolidLine);
    filledArrow = true;
    d_headLength = 4;
    d_headAngle = 45;
    d_position = Tail;

    d_end_x_a = endCol1;
    d_end_y_m = endCol2;
}

void VectorCurve::copy(const VectorCurve *vc)
{
    if (!vc || vc == this)
        return;

    d_type = vc->type();
    filledArrow = vc->filledArrow;
    d_headLength = vc->d_headLength;
    d_headAngle = vc->d_headAngle;
    d_position = vc->d_position;
    pen = vc->pen;
    delete vectorEnd;
    vectorEnd = vc->vectorEnd ? static_cast<QwtArrayData *>(vc->vectorEnd->copy()) : nullptr;
}

void VectorCurve::draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                       int from, int to) const
{
    if (!painter || dataSize() <= 0)
        return;

    if (to < 0)
        to = dataSize() - 1;

    PlotCurve::draw(painter, xMap, yMap, from, to);

    if (!vectorEnd || vectorEnd->size() == 0)
        return;

    painter->save();
    if (testRenderHint(QwtPlotItem::RenderAntialiased))
        painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(pen);
    drawVector(painter, xMap, yMap, from, to);
    painter->restore();
}

void VectorCurve::drawVector(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                             int from, int to) const
{
    if (!painter || !vectorEnd || dataSize() <= 0 || vectorEnd->size() == 0)
        return;

    const int maxIdx = qMin(dataSize(), static_cast<int>(vectorEnd->size())) - 1;
    const int startIdx = qMax(0, from);
    const int endIdx = (to < 0) ? maxIdx : qMin(to, maxIdx);
    if (startIdx > endIdx)
        return;

    constexpr double kPi = 3.14159265358979323846;
    const double headHalfW = double(d_headLength) * std::tan(kPi * double(d_headAngle) / 180.0);

    auto drawFloatingVector = [&](double xs, double ys, double xe, double ye) {
        if (!std::isfinite(xs) || !std::isfinite(ys) || !std::isfinite(xe) || !std::isfinite(ye))
            return;
        painter->drawLine(QPointF(xs, ys), QPointF(xe, ye));
        const double dx = xe - xs;
        const double dy = ye - ys;
        if (dx * dx + dy * dy <= 1e-12)
            return;
        const double angleDeg = std::atan2(dy, dx) * (180.0 / kPi);
        painter->save();
        painter->translate(QPointF(xe, ye));
        painter->rotate(angleDeg);
        QPolygonF headPoly(3);
        headPoly[0] = QPointF(0.0, 0.0);
        headPoly[1] = QPointF(-double(d_headLength), headHalfW);
        headPoly[2] = QPointF(-double(d_headLength), -headHalfW);
        if (filledArrow)
            painter->setBrush(QBrush(pen.color(), Qt::SolidPattern));
        else
            painter->setBrush(Qt::NoBrush);
        painter->drawPolygon(headPoly);
        painter->restore();
    };

    if (d_type == Graph::VectXYAM) {
        for (int i = startIdx; i <= endIdx; i++) {
            const double x0 = x(i);
            const double y0 = y(i);
            const double angle = vectorEnd->x(i);
            const double mag = vectorEnd->y(i);

            double xs = 0.0, ys = 0.0, xe = 0.0, ye = 0.0;
            switch (d_position) {
            case Tail:
                xs = xMap.xTransform(x0);
                ys = yMap.xTransform(y0);
                xe = xMap.xTransform(x0 + mag * cos(angle));
                ye = yMap.xTransform(y0 + mag * sin(angle));
                break;

            case Middle: {
                double dxh = 0.5 * mag * cos(angle);
                double dyh = 0.5 * mag * sin(angle);
                xs = xMap.xTransform(x0 - dxh);
                ys = yMap.xTransform(y0 - dyh);
                xe = xMap.xTransform(x0 + dxh);
                ye = yMap.xTransform(y0 + dyh);
            } break;

            case Head:
                xs = xMap.xTransform(x0 - mag * cos(angle));
                ys = yMap.xTransform(y0 - mag * sin(angle));
                xe = xMap.xTransform(x0);
                ye = yMap.xTransform(y0);
                break;
            }
            drawFloatingVector(xs, ys, xe, ye);
        }
    } else {
        for (int i = startIdx; i <= endIdx; i++) {
            const double xs = xMap.xTransform(x(i));
            const double ys = yMap.xTransform(y(i));
            const double xe = xMap.xTransform(vectorEnd->x(i));
            const double ye = yMap.xTransform(vectorEnd->y(i));
            drawFloatingVector(xs, ys, xe, ye);
        }
    }
}

void VectorCurve::drawArrowHead(QPainter *p, int xs, int ys, int xe, int ye) const
{
    p->save();
    p->translate(xe, ye);
    double t = theta(xs, ys, xe, ye);
    p->rotate(-t);

    double pi = 4 * atan(-1.0);
    int d = qRound(d_headLength * tan(pi * (double)d_headAngle / 180.0));

    QPolygon endArray(3);
    endArray[0] = QPoint(0, 0);
    endArray[1] = QPoint(-d_headLength, d);
    endArray[2] = QPoint(-d_headLength, -d);

    if (filledArrow)
        p->setBrush(QBrush(pen.color(), Qt::SolidPattern));

    QwtPainter::drawPolygon(p, endArray);
    p->restore();
}

double VectorCurve::theta(int x0, int y0, int x1, int y1) const
{
    double t, pi = 4 * atan(-1.0);
    if (x1 == x0) {
        if (y0 > y1)
            t = 90;
        else
            t = 270;
    } else {
        t = atan2((y1 - y0) * 1.0, (x1 - x0) * 1.0) * 180 / pi;
        if (t < 0)
            t = 360 + t;
    }
    return t;
}

void VectorCurve::setVectorEnd(const QString &xColName, const QString &yColName)
{
    if (d_end_x_a == xColName && d_end_y_m == yColName)
        return;

    d_end_x_a = xColName;
    d_end_y_m = yColName;

    loadData();
}

void VectorCurve::setVectorEnd(const QwtArray<double> &x, const QwtArray<double> &y)
{
    delete vectorEnd;
    vectorEnd = new QwtArrayData(x, y);
}

int VectorCurve::width()
{
    return pen.width();
}

void VectorCurve::setWidth(int w)
{
    pen.setWidth(w);
}

QColor VectorCurve::color()
{
    return pen.color();
}

void VectorCurve::setColor(const QColor &c)
{
    if (pen.color() != c)
        pen.setColor(c);
}

void VectorCurve::setHeadLength(int l)
{
    if (d_headLength != l)
        d_headLength = l;
}

void VectorCurve::setHeadAngle(int a)
{
    if (d_headAngle != a)
        d_headAngle = a;
}

void VectorCurve::fillArrowHead(bool fill)
{
    if (filledArrow != fill)
        filledArrow = fill;
}

QwtDoubleRect VectorCurve::boundingRect() const
{
    QwtDoubleRect rect = QwtPlotCurve::boundingRect();
    if (!vectorEnd || dataSize() <= 0 || vectorEnd->size() == 0)
        return rect;

    if (d_type == Graph::VectXYXY)
        rect |= vectorEnd->boundingRect();
    else {
        const int count = qMin(dataSize(), static_cast<int>(vectorEnd->size()));
        for (int i = 0; i < count; i++) {
            double x_i = x(i);
            double y_i = y(i);
            double angle = vectorEnd->x(i);
            double mag = vectorEnd->y(i);
            switch (d_position) {
            case Tail:
                rect |= QwtDoubleRect(x_i, y_i, mag * cos(angle), mag * sin(angle)).normalized();
                break;
            case Middle: {
                QwtDoubleRect rect_i(0, 0, fabs(mag * cos(angle)), fabs(mag * sin(angle)));
                rect_i.moveCenter(QwtDoublePoint(x_i, y_i));
                rect |= rect_i;
                break;
            }
            case Head:
                rect |= QwtDoubleRect(x_i, y_i, -mag * cos(angle), -mag * sin(angle)).normalized();
                break;
            }
        }
    }
    return rect;
}

void VectorCurve::updateColumnNames(const QString &oldName, const QString &newName,
                                    bool updateTableName)
{
    if (oldName.isEmpty() || oldName == newName)
        return;

    if (updateTableName) {
        if (d_table && d_table->name() != oldName && d_table->name() != newName)
            return;

        const QString prefix = oldName + QLatin1Char('_');
        const QString curTitle = title().text();
        if (curTitle.startsWith(prefix))
            setTitle(newName + QLatin1Char('_') + curTitle.mid(prefix.size()));

        if (d_x_column.startsWith(prefix))
            d_x_column = newName + QLatin1Char('_') + d_x_column.mid(prefix.size());

        if (d_end_x_a.startsWith(prefix))
            d_end_x_a = newName + QLatin1Char('_') + d_end_x_a.mid(prefix.size());

        if (d_end_y_m.startsWith(prefix))
            d_end_y_m = newName + QLatin1Char('_') + d_end_y_m.mid(prefix.size());
    } else {
        if (d_table) {
            const QString tblPrefix = QString(d_table->name()) + QLatin1Char('_');
            if (!oldName.startsWith(tblPrefix) || !newName.startsWith(tblPrefix))
                return;
            const QString candOld = oldName.mid(tblPrefix.size());
            const QString candNew = newName.mid(tblPrefix.size());
            bool hasOld = false;
            bool hasNew = false;
            for (int i = 0; i < d_table->numCols(); ++i) {
                if (const Column *col = d_table->column(i)) {
                    if (col->name() == candOld)
                        hasOld = true;
                    if (col->name() == candNew)
                        hasNew = true;
                }
            }
            if (hasOld && !hasNew)
                return;
        }

        if (title().text() == oldName)
            setTitle(newName);
        if (d_x_column == oldName)
            d_x_column = newName;
        if (d_end_x_a == oldName)
            d_end_x_a = newName;
        if (d_end_y_m == oldName)
            d_end_y_m = newName;
    }
}

QString VectorCurve::plotAssociation()
{
    QString base = d_x_column + "(X)," + title().text() + "(Y)," + d_end_x_a;
    if (d_type == Graph::VectXYAM)
        return base + "(A)," + d_end_y_m + "(M)";
    else
        return base + "(X)," + d_end_y_m + "(Y)";
}

bool VectorCurve::updateData(Table *t, const QString &colName)
{
    if (!t || d_table != t)
        return false;

    if (!matchesVectorColName(t, title().text(), colName)
        && !matchesVectorColName(t, d_x_column, colName)
        && !matchesVectorColName(t, d_end_x_a, colName)
        && !matchesVectorColName(t, d_end_y_m, colName))
        return false;

    loadData();
    return true;
}

bool VectorCurve::loadData()
{
    auto clearInvalidData = [this]() {
        delete vectorEnd;
        vectorEnd = nullptr;
        const double dummy = 0.0;
        setData(&dummy, &dummy, 0);
        foreach (DataCurve *c, d_error_bars) {
            if (c)
                c->setData(&dummy, &dummy, 0);
        }
    };

    if (!d_table || !d_table->d_future_table) {
        clearInvalidData();
        return false;
    }

    const int xcol = resolveVectorColIndex(d_table, d_x_column);
    const int ycol = resolveVectorColIndex(d_table, title().text());
    const int endXCol = resolveVectorColIndex(d_table, d_end_x_a);
    const int endYCol = resolveVectorColIndex(d_table, d_end_y_m);
    if (xcol < 0 || ycol < 0 || endXCol < 0 || endYCol < 0) {
        clearInvalidData();
        return false;
    }

    const Column *cX = d_table->column(xcol);
    const Column *cY = d_table->column(ycol);
    const Column *cEndX = d_table->column(endXCol);
    const Column *cEndY = d_table->column(endYCol);
    if (!cX || !cY || !cEndX || !cEndY) {
        clearInvalidData();
        return false;
    }

    const int totalRows = d_table->numRows();
    if (totalRows <= 0) {
        clearInvalidData();
        return false;
    }

    const int startRow = qMax(0, d_start_row);
    const int endRow = (d_end_row < 0) ? (totalRows - 1) : qMin(d_end_row, totalRows - 1);
    if (startRow > endRow) {
        clearInvalidData();
        return false;
    }

    const int maxRows = endRow - startRow + 1;
    QVector<double> X(maxRows), Y(maxRows), X2(maxRows), Y2(maxRows);
    int size = 0;
    for (int i = startRow; i <= endRow; ++i) {
        double vx = 0.0, vy = 0.0, vx2 = 0.0, vy2 = 0.0;
        if (!extractFiniteVectorValue(cX, i, vx))
            continue;
        if (!extractFiniteVectorValue(cY, i, vy))
            continue;
        if (!extractFiniteVectorValue(cEndX, i, vx2))
            continue;
        if (!extractFiniteVectorValue(cEndY, i, vy2))
            continue;

        X[size] = vx;
        Y[size] = vy;
        X2[size] = vx2;
        Y2[size] = vy2;
        ++size;
    }

    if (size <= 0) {
        clearInvalidData();
        return false;
    }

    X.resize(size);
    Y.resize(size);
    X2.resize(size);
    Y2.resize(size);
    setData(X.data(), Y.data(), size);
    foreach (DataCurve *c, d_error_bars) {
        if (c)
            c->setData(X.data(), Y.data(), size);
    }
    setVectorEnd(X2, Y2);

    return true;
}

VectorCurve::~VectorCurve()
{
    delete vectorEnd;
    vectorEnd = nullptr;
}
