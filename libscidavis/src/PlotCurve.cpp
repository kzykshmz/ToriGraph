/***************************************************************************
    File                 : DataCurve.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : AbstractPlotCurve and DataCurve classes

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
#include "PlotCurve.h"
#include "Graph.h"
#include "Plot.h"
#include "ScaleDraw.h"
#include "core/column/Column.h"
#include "core/datatypes/DateTime2StringFilter.h"
#include <QDateTime>
#include <QLineF>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <cmath>
#include <qwt_curve_fitter.h>
#include <qwt_painter.h>
#include <qwt_scale_map.h>
#include <qwt_symbol.h>

namespace {

Column *resolveDataCurveColumn(Table *table, const QString &spec)
{
    if (!table || !table->d_future_table || spec.isEmpty())
        return nullptr;

    const int numCols = table->numCols();
    const QString tblPrefix = QString(table->name()) + QLatin1Char('_');
    if (spec.startsWith(tblPrefix)) {
        const QString shortName = spec.mid(tblPrefix.size());
        for (int i = 0; i < numCols; ++i) {
            if (Column *col = table->column(i)) {
                if (col->name() == shortName)
                    return col;
            }
        }
    }

    for (int i = 0; i < numCols; ++i) {
        if (Column *col = table->column(i)) {
            if (col->name() == spec)
                return col;
        }
    }

    return table->column(spec);
}

} // namespace

DataCurve::DataCurve(Table *t, const QString &xColName, const QString &name, int startRow,
                     int endRow)
    : PlotCurve(name), d_table(t), d_x_column(xColName), d_start_row(startRow), d_end_row(endRow)
{
    if (t && d_end_row < 0)
        d_end_row = t->numRows() - 1;
}

void DataCurve::setRowRange(int startRow, int endRow)
{
    if (d_start_row == startRow && d_end_row == endRow)
        return;

    d_start_row = startRow;
    d_end_row = endRow;

    loadData();

    foreach (DataCurve *c, d_error_bars)
        c->loadData();
}

void DataCurve::setFullRange()
{
    if (!d_table)
        return;
    d_start_row = 0;
    d_end_row = d_table->numRows() - 1;

    loadData();

    foreach (DataCurve *c, d_error_bars)
        c->loadData();
}

bool DataCurve::isFullRange()
{
    if (!d_table || d_start_row != 0 || d_end_row != d_table->numRows() - 1)
        return false;
    else
        return true;
}

QString DataCurve::plotAssociation()
{
    if (!d_x_column.isEmpty())
        return d_x_column + "(X)," + title().text() + "(Y)";
    else
        return title().text();
}

void DataCurve::updateColumnNames(const QString &oldName, const QString &newName,
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
    }
}

bool DataCurve::updateData(Table *t, const QString &colName)
{
    if (d_table != t || (colName != title().text() && d_x_column != colName))
        return false;

    loadData();
    return true;
}

QList<QVector<double>> DataCurve::convertData(const QList<Column *> &cols,
                                              const QList<int> &axes) const
{
    Graph *g = 0;
    if (plot())
        g = static_cast<Graph *>(plot()->parent());

    int end_row = d_end_row;
    // make sure end_row is a valid index for all columns
    foreach (Column *col, cols)
        if (end_row >= col->rowCount())
            end_row = col->rowCount() - 1;

    // determine rows for which all columns have valid content
    QList<int> valid_rows;
    for (int row = d_start_row; row <= end_row; row++) {
        bool all_valid = true;
        foreach (Column *col, cols)
            if (col->isInvalid(row)) {
                all_valid = false;
                break;
            }
        if (all_valid)
            valid_rows.push_back(row);
    }

    // initialize result list
    QList<QVector<double>> result;
    for (int i = 0; i < cols.size(); i++)
        result.push_back(QVector<double>(valid_rows.size()));

    // For date and time (but not DateTime) values, numbers are relative to the first valid row's
    // content. We can't change this easily without breaking backwards compatibility (think of
    // scale ranges, placement of labels and arrows etc).
    QList<QDate> reference_dates;
    QList<QTime> reference_times;
    for (int i = 0; i < cols.size(); i++) {
        Column *col = cols[i];

        switch (col->columnMode()) {
        case SciDAVis::ColumnMode::DateTime: {
            QDateTime datetime;
            QString format;

            if (g && g->axesType()[axes[i]] == Graph::AxisType::DateTime) {
                QStringList lst = g->axisFormatInfo(axes[i]).split(";");
                datetime = QDateTime::fromString(lst[0], "yyyy-MM-ddThh:mm:ss");
                if (lst.size() >= 2)
                    format = lst[1];
            }

            if (!datetime.isValid()) {
                foreach (int row, valid_rows) {
                    datetime = col->dateTimeAt(row);
                    if (datetime.isValid())
                        break;
                }
            }

            if (format.isEmpty())
                format = static_cast<DateTime2StringFilter *>(col->outputFilter())->format();

            reference_dates.push_back(QDate());
            reference_times.push_back(QTime());
            if (g)
                g->setLabelsDateTimeFormat(axes[i], Graph::AxisType::DateTime,
                                           datetime.toString("yyyy-MM-ddThh:mm:ss") + ";" + format);
            break;
        }
        case SciDAVis::ColumnMode::Text:
            if (g)
                g->setLabelsTextFormat(axes[i], col, d_start_row, end_row);
            reference_dates.push_back(QDate());
            reference_times.push_back(QTime());
            break;
        default:
            reference_dates.push_back(QDate());
            reference_times.push_back(QTime());
            break;
        };
    }

    // convert data to numeric representation used for plotting
    for (int i = 0; i < valid_rows.size(); i++)
        for (int j = 0; j < cols.size(); j++)
            switch (cols[j]->columnMode()) {
            case SciDAVis::ColumnMode::Text:
                result[j][i] = static_cast<double>(valid_rows[i] + 1);
                break;
            case SciDAVis::ColumnMode::DateTime: {
                QDateTime dt = cols[j]->dateTimeAt(valid_rows[i]);
                result[j][i] = dt.toMSecsSinceEpoch() / 86400000. + 2440587.5;
                break;
            }
            default:
                result[j][i] = cols[j]->valueAt(valid_rows[i]);
                break;
            };

    d_index_to_row = QVector<int>::fromList(valid_rows);
    return result;
}

bool DataCurve::loadData()
{
    Column *x_col_ptr = resolveDataCurveColumn(d_table, d_x_column);
    Column *y_col_ptr = resolveDataCurveColumn(d_table, title().text());
    if (!x_col_ptr || !y_col_ptr) {
        remove();
        return false;
    }

    QList<QVector<double>> points = convertData(
            d_type == Graph::HorizontalBars ? (QList<Column *>() << y_col_ptr << x_col_ptr)
                                            : (QList<Column *>() << x_col_ptr << y_col_ptr),
            QList<int>() << xAxis() << yAxis());

    if (points.isEmpty() || points[0].size() == 0) {
        remove();
        return false;
    }

    d_applied_dx = 0.0;
    d_applied_dy = 0.0;
    if (d_norm01)
        d_raw_y_backup = points[1];
    setData(points[0].data(), points[1].data(), points[0].size());
    if (d_norm01 || std::fabs(d_stack_dx) > 0.0 || std::fabs(d_stack_dy) > 0.0) {
        applyStackTransformToCurrentData();
    } else {
        foreach (DataCurve *c, d_error_bars)
            c->setData(points[0].data(), points[1].data(), points[0].size());
    }

    return true;
}

void DataCurve::removeErrorBars(DataCurve *c)
{
    if (!c || d_error_bars.isEmpty())
        return;

    int index = d_error_bars.indexOf(c);
    if (index >= 0 && index < d_error_bars.size())
        d_error_bars.removeAt(index);
}

void DataCurve::clearErrorBars()
{
    if (d_error_bars.isEmpty())
        return;

    foreach (DataCurve *c, d_error_bars)
        c->remove();
}

void DataCurve::remove()
{
    if (!plot())
        return;
    Graph *g = (Graph *)plot()->parent();
    if (!g)
        return;

    g->removeCurve(title().text());
}

void DataCurve::setVisible(bool on)
{
    QwtPlotCurve::setVisible(on);
    foreach (DataCurve *c, d_error_bars)
        c->setVisible(on);
}

int DataCurve::tableRow(int point) const
{
    if (!d_index_to_row.isEmpty())
        return d_index_to_row.value(point, -1);
    if (d_table && point >= 0 && point < dataSize()) {
        int r = d_start_row + point;
        if (r >= 0 && r < d_table->numRows())
            return r;
    }
    return -1;
}

bool DataCurve::isPointMasked(int point) const
{
    if (!d_table || point < 0 || point >= dataSize())
        return false;
    const int row = tableRow(point);
    if (row < 0)
        return false;
    Column *yCol = resolveDataCurveColumn(d_table, title().text());
    if (yCol && yCol->isMasked(row))
        return true;
    Column *xCol = resolveDataCurveColumn(d_table, d_x_column);
    if (xCol && xCol->isMasked(row))
        return true;
    return false;
}

void DataCurve::maskPoint(int point, bool mask)
{
    if (!d_table || point < 0 || point >= dataSize())
        return;
    const int row = tableRow(point);
    if (row < 0)
        return;
    Column *yCol = resolveDataCurveColumn(d_table, title().text());
    if (!yCol)
        return;

    yCol->setMasked(row, mask);
    if (!mask) {
        Column *xCol = resolveDataCurveColumn(d_table, d_x_column);
        if (xCol && xCol->isMasked(row))
            xCol->setMasked(row, false);
    }

    emit d_table->modifiedData(d_table, title().text());
    if (plot()) {
        Graph *g = qobject_cast<Graph *>(plot()->parent());
        if (g)
            g->replot();
        else
            plot()->replot();
    }
}

int DataCurve::maskRegion(const QRectF &dataRect, bool mask)
{
    if (!d_table || dataSize() <= 0)
        return 0;
    Column *yCol = resolveDataCurveColumn(d_table, title().text());
    if (!yCol)
        return 0;
    Column *xCol = resolveDataCurveColumn(d_table, d_x_column);

    const QRectF normRect = dataRect.normalized();
    int modifiedCount = 0;
    const int n = dataSize();
    for (int i = 0; i < n; ++i) {
        const double px = x(i);
        const double py = y(i);
        if (px >= normRect.left() && px <= normRect.right() && py >= normRect.top()
            && py <= normRect.bottom()) {
            const int row = tableRow(i);
            if (row < 0)
                continue;
            const bool wasMasked = isPointMasked(i);
            yCol->setMasked(row, mask);
            if (!mask && xCol && xCol->isMasked(row))
                xCol->setMasked(row, false);
            if (wasMasked != mask)
                ++modifiedCount;
        }
    }

    if (modifiedCount > 0) {
        emit d_table->modifiedData(d_table, title().text());
        if (plot()) {
            Graph *g = qobject_cast<Graph *>(plot()->parent());
            if (g)
                g->replot();
            else
                plot()->replot();
        }
    }
    return modifiedCount;
}

void DataCurve::clearMasks()
{
    if (!d_table)
        return;
    Column *yCol = resolveDataCurveColumn(d_table, title().text());
    if (!yCol)
        return;

    yCol->clearMasks();
    emit d_table->modifiedData(d_table, title().text());
    if (plot()) {
        Graph *g = qobject_cast<Graph *>(plot()->parent());
        if (g)
            g->replot();
        else
            plot()->replot();
    }
}

int DataCurve::maskedPointCount() const
{
    if (!d_table || dataSize() <= 0)
        return 0;
    int count = 0;
    const int n = dataSize();
    for (int i = 0; i < n; ++i) {
        if (isPointMasked(i))
            ++count;
    }
    return count;
}

void DataCurve::draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap, int from,
                     int to) const
{
    if (!painter || dataSize() <= 0)
        return;

    const int n = dataSize();
    const int startIdx = qMax(0, from);
    const int endIdx = (to < 0) ? (n - 1) : qMin(to, n - 1);
    if (startIdx > endIdx)
        return;

    bool anyMasked = false;
    if (d_table) {
        for (int i = startIdx; i <= endIdx; ++i) {
            if (isPointMasked(i)) {
                anyMasked = true;
                break;
            }
        }
    }

    if (!anyMasked) {
        PlotCurve::draw(painter, xMap, yMap, from, to);
        return;
    }

    int segStart = -1;
    for (int i = startIdx; i <= endIdx + 1; ++i) {
        const bool masked = (i > endIdx) || isPointMasked(i);
        if (!masked && segStart < 0) {
            segStart = i;
        } else if (masked && segStart >= 0) {
            PlotCurve::draw(painter, xMap, yMap, segStart, i - 1);
            segStart = -1;
        }
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QPen maskPen(QColor(220, 30, 30), 2, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
    painter->setPen(maskPen);
    painter->setBrush(Qt::NoBrush);
    const double r = qMax(5.0, 0.5 * symbol().size().width() + 2.0);

    for (int i = startIdx; i <= endIdx; ++i) {
        if (!isPointMasked(i))
            continue;
        const double px = xMap.xTransform(x(i));
        const double py = yMap.xTransform(y(i));
        if (!std::isfinite(px) || !std::isfinite(py))
            continue;
        painter->drawRect(QRectF(px - r, py - r, 2.0 * r, 2.0 * r));
        painter->drawLine(QPointF(px - r + 1.0, py - r + 1.0),
                          QPointF(px + r - 1.0, py + r - 1.0));
        painter->drawLine(QPointF(px - r + 1.0, py + r - 1.0),
                          QPointF(px + r - 1.0, py - r + 1.0));
    }
    painter->restore();
}

void PlotCurve::setStackOffset(double dx, double dy)
{
    if (!std::isfinite(dx))
        dx = 0.0;
    if (!std::isfinite(dy))
        dy = 0.0;
    d_stack_dx = dx;
    d_stack_dy = dy;

    const int n = dataSize();
    if (n <= 0) {
        d_applied_dx = dx;
        d_applied_dy = dy;
        return;
    }

    const double deltaX = dx - d_applied_dx;
    const double deltaY = dy - d_applied_dy;
    if (std::fabs(deltaX) <= 1e-15 && std::fabs(deltaY) <= 1e-15)
        return;

    QVector<double> xs(n);
    QVector<double> ys(n);
    for (int i = 0; i < n; ++i) {
        xs[i] = x(i) + deltaX;
        ys[i] = y(i) + deltaY;
    }
    setData(xs.data(), ys.data(), n);
    d_applied_dx = dx;
    d_applied_dy = dy;

    if (DataCurve *dc = dynamic_cast<DataCurve *>(this)) {
        foreach (DataCurve *err, dc->errorBarsList()) {
            if (err)
                err->setData(xs.data(), ys.data(), n);
        }
    }
}

void PlotCurve::setStackNormalize01(bool normalize)
{
    if (d_norm01 == normalize)
        return;

    const int n = dataSize();
    if (n <= 0) {
        d_norm01 = normalize;
        if (!normalize)
            d_raw_y_backup.clear();
        return;
    }

    QVector<double> xs(n);
    QVector<double> ys(n);
    for (int i = 0; i < n; ++i)
        xs[i] = x(i) - d_applied_dx;

    if (normalize) {
        d_raw_y_backup.resize(n);
        for (int i = 0; i < n; ++i)
            d_raw_y_backup[i] = y(i) - d_applied_dy;
        d_norm01 = true;
        d_applied_dx = 0.0;
        d_applied_dy = 0.0;
        setData(xs.data(), d_raw_y_backup.data(), n);
        applyStackTransformToCurrentData();
    } else {
        if (d_raw_y_backup.size() == n) {
            for (int i = 0; i < n; ++i)
                ys[i] = d_raw_y_backup[i] + d_stack_dy;
        } else {
            for (int i = 0; i < n; ++i)
                ys[i] = (y(i) - d_applied_dy) + d_stack_dy;
        }
        for (int i = 0; i < n; ++i)
            xs[i] += d_stack_dx;
        d_norm01 = false;
        d_raw_y_backup.clear();
        setData(xs.data(), ys.data(), n);
        d_applied_dx = d_stack_dx;
        d_applied_dy = d_stack_dy;
        if (DataCurve *dc = dynamic_cast<DataCurve *>(this)) {
            foreach (DataCurve *err, dc->errorBarsList()) {
                if (err)
                    err->setData(xs.data(), ys.data(), n);
            }
        }
    }
}

void PlotCurve::applyStackTransformToCurrentData()
{
    const int n = dataSize();
    if (n <= 0)
        return;

    QVector<double> xs(n);
    QVector<double> ys(n);
    for (int i = 0; i < n; ++i) {
        xs[i] = x(i) - d_applied_dx;
        ys[i] = (d_norm01 && d_raw_y_backup.size() == n) ? d_raw_y_backup[i] : (y(i) - d_applied_dy);
    }

    if (d_norm01) {
        if (d_raw_y_backup.size() != n)
            d_raw_y_backup = ys;
        double minY = 1e300;
        double maxY = -1e300;
        for (int i = 0; i < n; ++i) {
            if (std::isfinite(ys[i])) {
                if (ys[i] < minY)
                    minY = ys[i];
                if (ys[i] > maxY)
                    maxY = ys[i];
            }
        }
        const double span = (maxY > minY) ? (maxY - minY) : 0.0;
        for (int i = 0; i < n; ++i) {
            if (std::isfinite(ys[i]))
                ys[i] = (span > 1e-300) ? ((ys[i] - minY) / span) : 0.0;
        }
    }

    for (int i = 0; i < n; ++i) {
        xs[i] += d_stack_dx;
        ys[i] += d_stack_dy;
    }

    setData(xs.data(), ys.data(), n);
    d_applied_dx = d_stack_dx;
    d_applied_dy = d_stack_dy;

    if (DataCurve *dc = dynamic_cast<DataCurve *>(this)) {
        foreach (DataCurve *err, dc->errorBarsList()) {
            if (err)
                err->setData(xs.data(), ys.data(), n);
        }
    }
}

QwtDoubleRect PlotCurve::boundingRect() const
{
    QwtDoubleRect r = QwtPlotCurve::boundingRect();
    if (!r.isValid() || symbol().style() == QwtSymbol::NoSymbol || !plot())
        return r;

    int margin = 1 + symbol().size().width();

    const QwtScaleMap &xMap = plot()->canvasMap(xAxis());
    const QwtScaleMap &yMap = plot()->canvasMap(yAxis());
    if (xMap.pDist() < 4.0 || yMap.pDist() < 4.0 || xMap.sDist() <= 0.0 || yMap.sDist() <= 0.0)
        return r;

    const double px_right = xMap.xTransform(r.right());
    const double px_left = xMap.xTransform(r.left());
    const double py_top = yMap.xTransform(r.top());
    const double py_bottom = yMap.xTransform(r.bottom());
    if (!std::isfinite(px_right) || !std::isfinite(px_left) || !std::isfinite(py_top)
        || !std::isfinite(py_bottom))
        return r;

    const double d_x_right = xMap.invTransform(px_right + margin);
    const double d_x_left = xMap.invTransform(px_left - margin);
    const double d_y_top = yMap.invTransform(py_top + margin);
    const double d_y_bottom = yMap.invTransform(py_bottom - margin);
    if (!std::isfinite(d_x_right) || !std::isfinite(d_x_left) || !std::isfinite(d_y_top)
        || !std::isfinite(d_y_bottom))
        return r;

    const double minX = qMin(d_x_left, d_x_right);
    const double maxX = qMax(d_x_left, d_x_right);
    const double minY = qMin(d_y_top, d_y_bottom);
    const double maxY = qMax(d_y_top, d_y_bottom);
    return QwtDoubleRect(minX, minY, maxX - minX, maxY - minY);
}

namespace {

constexpr double kMaxSafeCoord = 30000.0;

QPointF clampExtremePoint(const QPointF &pt)
{
    return QPointF(qBound(-kMaxSafeCoord, pt.x(), kMaxSafeCoord),
                   qBound(-kMaxSafeCoord, pt.y(), kMaxSafeCoord));
}

void fillCurvePolygonF(QPainter *painter, const PlotCurve *curve, const QwtScaleMap &xMap,
                       const QwtScaleMap &yMap, const QPolygonF &openPoly)
{
    if (!painter || !curve || curve->brush().style() == Qt::NoBrush || openPoly.size() < 2)
        return;

    QPolygonF closed = openPoly;
    if (curve->curveType() == QwtPlotCurve::Xfy) {
        const double refX = qBound(-kMaxSafeCoord, xMap.xTransform(curve->baseline()), kMaxSafeCoord);
        closed.append(QPointF(refX, openPoly.last().y()));
        closed.append(QPointF(refX, openPoly.first().y()));
    } else {
        const double refY = qBound(-kMaxSafeCoord, yMap.xTransform(curve->baseline()), kMaxSafeCoord);
        closed.append(QPointF(openPoly.last().x(), refY));
        closed.append(QPointF(openPoly.first().x(), refY));
    }

    QBrush b = curve->brush();
    if (!b.color().isValid())
        b.setColor(curve->pen().color());

    painter->save();
    painter->setPen(QPen(Qt::NoPen));
    painter->setBrush(b);
    painter->drawPolygon(closed);
    painter->restore();
}

void drawSymbolShapeF(QPainter *painter, QwtSymbol::Style style, double cx, double cy, double w,
                      double h)
{
    const double hw = 0.5 * w;
    const double hh = 0.5 * h;
    const QRectF r(cx - hw, cy - hh, w, h);

    switch (style) {
    case QwtSymbol::Ellipse:
        painter->drawEllipse(r);
        break;
    case QwtSymbol::Rect:
        painter->drawRect(r);
        break;
    case QwtSymbol::Diamond: {
        QPolygonF pa(4);
        pa[0] = QPointF(cx, r.top());
        pa[1] = QPointF(r.right(), cy);
        pa[2] = QPointF(cx, r.bottom());
        pa[3] = QPointF(r.left(), cy);
        painter->drawPolygon(pa);
        break;
    }
    case QwtSymbol::Cross:
        painter->drawLine(QPointF(cx, r.top()), QPointF(cx, r.bottom()));
        painter->drawLine(QPointF(r.left(), cy), QPointF(r.right(), cy));
        break;
    case QwtSymbol::XCross:
        painter->drawLine(QPointF(r.left(), r.top()), QPointF(r.right(), r.bottom()));
        painter->drawLine(QPointF(r.left(), r.bottom()), QPointF(r.right(), r.top()));
        break;
    case QwtSymbol::Triangle:
    case QwtSymbol::UTriangle: {
        QPolygonF pa(3);
        pa[0] = QPointF(cx, r.top());
        pa[1] = QPointF(r.right(), r.bottom());
        pa[2] = QPointF(r.left(), r.bottom());
        painter->drawPolygon(pa);
        break;
    }
    case QwtSymbol::DTriangle: {
        QPolygonF pa(3);
        pa[0] = QPointF(r.left(), r.top());
        pa[1] = QPointF(r.right(), r.top());
        pa[2] = QPointF(cx, r.bottom());
        painter->drawPolygon(pa);
        break;
    }
    case QwtSymbol::RTriangle: {
        QPolygonF pa(3);
        pa[0] = QPointF(r.left(), r.top());
        pa[1] = QPointF(r.right(), cy);
        pa[2] = QPointF(r.left(), r.bottom());
        painter->drawPolygon(pa);
        break;
    }
    case QwtSymbol::LTriangle: {
        QPolygonF pa(3);
        pa[0] = QPointF(r.right(), r.top());
        pa[1] = QPointF(r.left(), cy);
        pa[2] = QPointF(r.right(), r.bottom());
        painter->drawPolygon(pa);
        break;
    }
    case QwtSymbol::HLine:
        painter->drawLine(QPointF(r.left(), cy), QPointF(r.right(), cy));
        break;
    case QwtSymbol::VLine:
        painter->drawLine(QPointF(cx, r.top()), QPointF(cx, r.bottom()));
        break;
    case QwtSymbol::Star1: {
        constexpr double kSqrt1_2 = 0.70710678118654752440;
        const double dx = hw * kSqrt1_2;
        const double dy = hh * kSqrt1_2;
        painter->drawLine(QPointF(cx - dx, cy - dy), QPointF(cx + dx, cy + dy));
        painter->drawLine(QPointF(cx - dx, cy + dy), QPointF(cx + dx, cy - dy));
        painter->drawLine(QPointF(cx, r.top()), QPointF(cx, r.bottom()));
        painter->drawLine(QPointF(r.left(), cy), QPointF(r.right(), cy));
        break;
    }
    case QwtSymbol::Star2: {
        const double side = 0.5 * w * (1.0 - 0.8660254037844386);
        const double h4 = 0.25 * h;
        const double h34 = 0.75 * h;
        const double innerSpan = side + (w - 2.0 * side) / 3.0;
        const double midSpan = side + (hw - side) / 3.0;

        QPolygonF pa(12);
        pa[0] = QPointF(cx, r.top());
        pa[1] = QPointF(r.right() - innerSpan, r.top() + h4);
        pa[2] = QPointF(r.right() - side, r.top() + h4);
        pa[3] = QPointF(r.right() - midSpan, cy);
        pa[4] = QPointF(r.right() - side, r.top() + h34);
        pa[5] = QPointF(r.right() - innerSpan, r.top() + h34);
        pa[6] = QPointF(cx, r.bottom());
        pa[7] = QPointF(r.left() + innerSpan, r.top() + h34);
        pa[8] = QPointF(r.left() + side, r.top() + h34);
        pa[9] = QPointF(r.left() + midSpan, cy);
        pa[10] = QPointF(r.left() + side, r.top() + h4);
        pa[11] = QPointF(r.left() + innerSpan, r.top() + h4);
        painter->drawPolygon(pa);
        break;
    }
    case QwtSymbol::Hexagon: {
        const double side = 0.5 * w * (1.0 - 0.8660254037844386);
        const double h4 = 0.25 * h;
        const double h34 = 0.75 * h;

        QPolygonF pa(6);
        pa[0] = QPointF(cx, r.top());
        pa[1] = QPointF(r.right() - side, r.top() + h4);
        pa[2] = QPointF(r.right() - side, r.top() + h34);
        pa[3] = QPointF(cx, r.bottom());
        pa[4] = QPointF(r.left() + side, r.top() + h34);
        pa[5] = QPointF(r.left() + side, r.top() + h4);
        painter->drawPolygon(pa);
        break;
    }
    default:
        break;
    }
}

} // namespace

void PlotCurve::draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                     const QRect &canvasRect) const
{
    const QRectF prev = d_canvas_rect;
    d_canvas_rect = QRectF(canvasRect);
    QwtPlotCurve::draw(painter, xMap, yMap, canvasRect);
    d_canvas_rect = prev;
}

void PlotCurve::drawCurve(QPainter *painter, int style, const QwtScaleMap &xMap,
                          const QwtScaleMap &yMap, int from, int to) const
{
    if (!painter || dataSize() <= 0)
        return;

    if (testRenderHint(QwtPlotItem::RenderAntialiased))
        painter->setRenderHint(QPainter::Antialiasing, true);

    const int n = dataSize();
    from = qMax(0, from);
    to = (to < 0) ? (n - 1) : qMin(to, n - 1);
    if (from > to)
        return;

    switch (style) {
    case Lines: {
        const bool isFitted = testCurveAttribute(Fitted) && (curveFitter() != nullptr);
        if (isFitted) {
            const DataCurve *dc = dynamic_cast<const DataCurve *>(this);
            if (!dc || dc->maskedPointCount() == 0) {
                from = 0;
                to = n - 1;
            }
        }

        const bool decimateSubPixel =
                !isFitted && testPaintAttribute(PaintFiltered) && ((to - from) > 4000);

        auto flushRun = [&](QPolygonF &run) {
            if (run.isEmpty())
                return;
            if (isFitted && run.size() >= 2) {
                QPolygonF fitted = curveFitter()->fitCurve(run);
                if (!fitted.isEmpty()) {
                    for (int k = 0; k < fitted.size(); ++k)
                        fitted[k] = clampExtremePoint(fitted[k]);
                    run.swap(fitted);
                }
            }
            if (run.size() >= 2) {
                painter->drawPolyline(run);
                if (brush().style() != Qt::NoBrush)
                    fillCurvePolygonF(painter, this, xMap, yMap, run);
            } else if (run.size() == 1) {
                painter->drawPoint(run[0]);
            }
            run.clear();
        };

        const Graph *g = plot() ? qobject_cast<const Graph *>(plot()->parent()) : nullptr;
        bool hasXBreak = false;
        double xBf = 0.0, xBt = 0.0, xBrkPx = 0.0, xEps = 0.0;
        bool hasYBreak = false;
        double yBf = 0.0, yBt = 0.0, yBrkPx = 0.0, yEps = 0.0;
        if (g) {
            Graph::AxisBreakSpec xb = g->axisBreak(xAxis());
            if (!xb.enabled && xAxis() == QwtPlot::xTop && !g->hasColorScaleOnAxis(QwtPlot::xTop))
                xb = g->axisBreak(QwtPlot::xBottom);
            if (xb.enabled && std::isfinite(xb.breakFrom) && std::isfinite(xb.breakTo)
                && xb.breakFrom != xb.breakTo) {
                hasXBreak = true;
                xBf = qMin(xb.breakFrom, xb.breakTo);
                xBt = qMax(xb.breakFrom, xb.breakTo);
                xBrkPx = 0.5 * (xMap.xTransform(xBf) + xMap.xTransform(xBt));
                xEps = 1e-12 * qMax(1.0, xBt - xBf);
            }
            Graph::AxisBreakSpec yb = g->axisBreak(yAxis());
            if (!yb.enabled && yAxis() == QwtPlot::yRight
                && !g->hasColorScaleOnAxis(QwtPlot::yRight))
                yb = g->axisBreak(QwtPlot::yLeft);
            if (yb.enabled && std::isfinite(yb.breakFrom) && std::isfinite(yb.breakTo)
                && yb.breakFrom != yb.breakTo) {
                hasYBreak = true;
                yBf = qMin(yb.breakFrom, yb.breakTo);
                yBt = qMax(yb.breakFrom, yb.breakTo);
                yBrkPx = 0.5 * (yMap.xTransform(yBf) + yMap.xTransform(yBt));
                yEps = 1e-12 * qMax(1.0, yBt - yBf);
            }
        }
        const double halfGap = 4.5;

        QPolygonF run;
        run.reserve(qMin(4096, to - from + 1));
        bool hasPrev = false;
        double prevVx = 0.0, prevVy = 0.0;
        QPointF prevPt;
        for (int i = from; i <= to; ++i) {
            const double vx = x(i);
            const double vy = y(i);
            const double px = xMap.xTransform(vx);
            const double py = yMap.xTransform(vy);
            if (!std::isfinite(px) || !std::isfinite(py)) {
                flushRun(run);
                hasPrev = false;
                continue;
            }
            if ((hasXBreak && vx > xBf + xEps && vx < xBt - xEps)
                || (hasYBreak && vy > yBf + yEps && vy < yBt - yEps)) {
                flushRun(run);
                hasPrev = false;
                continue;
            }

            const QPointF pt = clampExtremePoint(QPointF(px, py));
            bool splitHandled = false;
            if (hasPrev && hasXBreak) {
                const bool straddlesX = (prevVx <= xBf + xEps && vx >= xBt - xEps)
                        || (prevVx >= xBt - xEps && vx <= xBf + xEps);
                const double loX = xBrkPx - halfGap;
                const double hiX = xBrkPx + halfGap;
                const double dx = pt.x() - prevPt.x();
                if (straddlesX || (prevPt.x() <= loX && pt.x() >= hiX)
                    || (prevPt.x() >= hiX && pt.x() <= loX)) {
                    if (std::fabs(dx) > 1e-6 && ((prevPt.x() < loX && pt.x() > hiX)
                                                 || (prevPt.x() > hiX && pt.x() < loX))) {
                        const double edge1X = (dx > 0.0) ? loX : hiX;
                        const double edge2X = (dx > 0.0) ? hiX : loX;
                        const double t1 = qBound(0.0, (edge1X - prevPt.x()) / dx, 1.0);
                        const double t2 = qBound(0.0, (edge2X - prevPt.x()) / dx, 1.0);
                        const double dy = pt.y() - prevPt.y();
                        run.append(clampExtremePoint(QPointF(edge1X, prevPt.y() + t1 * dy)));
                        flushRun(run);
                        run.append(clampExtremePoint(QPointF(edge2X, prevPt.y() + t2 * dy)));
                    } else {
                        flushRun(run);
                    }
                    splitHandled = true;
                } else if (pt.x() > loX && pt.x() < hiX) {
                    if (!run.isEmpty() && std::fabs(dx) > 1e-6
                        && (prevPt.x() <= loX || prevPt.x() >= hiX)) {
                        const double edge1X = (prevPt.x() <= loX) ? loX : hiX;
                        const double t1 = qBound(0.0, (edge1X - prevPt.x()) / dx, 1.0);
                        run.append(clampExtremePoint(
                                QPointF(edge1X, prevPt.y() + t1 * (pt.y() - prevPt.y()))));
                    }
                    flushRun(run);
                    prevVx = vx;
                    prevVy = vy;
                    prevPt = pt;
                    hasPrev = true;
                    continue;
                } else if (prevPt.x() > loX && prevPt.x() < hiX && std::fabs(dx) > 1e-6
                           && (pt.x() <= loX || pt.x() >= hiX)) {
                    flushRun(run);
                    const double edge2X = (pt.x() >= hiX) ? hiX : loX;
                    const double t2 = qBound(0.0, (edge2X - prevPt.x()) / dx, 1.0);
                    run.append(clampExtremePoint(
                            QPointF(edge2X, prevPt.y() + t2 * (pt.y() - prevPt.y()))));
                    splitHandled = true;
                }
            }
            if (hasPrev && !splitHandled && hasYBreak) {
                const bool straddlesY = (prevVy <= yBf + yEps && vy >= yBt - yEps)
                        || (prevVy >= yBt - yEps && vy <= yBf + yEps);
                const double loY = yBrkPx - halfGap;
                const double hiY = yBrkPx + halfGap;
                const double dy = pt.y() - prevPt.y();
                if (straddlesY || (prevPt.y() <= loY && pt.y() >= hiY)
                    || (prevPt.y() >= hiY && pt.y() <= loY)) {
                    if (std::fabs(dy) > 1e-6 && ((prevPt.y() < loY && pt.y() > hiY)
                                                 || (prevPt.y() > hiY && pt.y() < loY))) {
                        const double edge1Y = (dy > 0.0) ? loY : hiY;
                        const double edge2Y = (dy > 0.0) ? hiY : loY;
                        const double t1 = qBound(0.0, (edge1Y - prevPt.y()) / dy, 1.0);
                        const double t2 = qBound(0.0, (edge2Y - prevPt.y()) / dy, 1.0);
                        const double dx = pt.x() - prevPt.x();
                        run.append(clampExtremePoint(QPointF(prevPt.x() + t1 * dx, edge1Y)));
                        flushRun(run);
                        run.append(clampExtremePoint(QPointF(prevPt.x() + t2 * dx, edge2Y)));
                    } else {
                        flushRun(run);
                    }
                } else if (pt.y() > loY && pt.y() < hiY) {
                    if (!run.isEmpty() && std::fabs(dy) > 1e-6
                        && (prevPt.y() <= loY || prevPt.y() >= hiY)) {
                        const double edge1Y = (prevPt.y() <= loY) ? loY : hiY;
                        const double t1 = qBound(0.0, (edge1Y - prevPt.y()) / dy, 1.0);
                        run.append(clampExtremePoint(
                                QPointF(prevPt.x() + t1 * (pt.x() - prevPt.x()), edge1Y)));
                    }
                    flushRun(run);
                    prevVx = vx;
                    prevVy = vy;
                    prevPt = pt;
                    hasPrev = true;
                    continue;
                } else if (prevPt.y() > loY && prevPt.y() < hiY && std::fabs(dy) > 1e-6
                           && (pt.y() <= loY || pt.y() >= hiY)) {
                    flushRun(run);
                    const double edge2Y = (pt.y() >= hiY) ? hiY : loY;
                    const double t2 = qBound(0.0, (edge2Y - prevPt.y()) / dy, 1.0);
                    run.append(clampExtremePoint(
                            QPointF(prevPt.x() + t2 * (pt.x() - prevPt.x()), edge2Y)));
                }
            }

            if (decimateSubPixel && !run.isEmpty() && i < to) {
                const double dx = pt.x() - run.last().x();
                const double dy = pt.y() - run.last().y();
                if (dx * dx + dy * dy < 4e-4)
                    continue;
            }
            run.append(pt);
            prevVx = vx;
            prevVy = vy;
            prevPt = pt;
            hasPrev = true;
        }
        flushRun(run);
        break;
    }
    case Sticks: {
        const double x0 = qBound(-kMaxSafeCoord, xMap.xTransform(baseline()), kMaxSafeCoord);
        const double y0 = qBound(-kMaxSafeCoord, yMap.xTransform(baseline()), kMaxSafeCoord);
        QVector<QLineF> sticks;
        sticks.reserve(to - from + 1);
        for (int i = from; i <= to; ++i) {
            const double px = xMap.xTransform(x(i));
            const double py = yMap.xTransform(y(i));
            if (!std::isfinite(px) || !std::isfinite(py))
                continue;
            const QPointF pt = clampExtremePoint(QPointF(px, py));
            if (curveType() == Xfy)
                sticks.append(QLineF(QPointF(x0, pt.y()), pt));
            else
                sticks.append(QLineF(QPointF(pt.x(), y0), pt));
        }
        if (!sticks.isEmpty())
            painter->drawLines(sticks);
        break;
    }
    case Steps: {
        bool inverted = (curveType() == Yfx);
        if (testCurveAttribute(Inverted))
            inverted = !inverted;

        auto flushStepRun = [&](QPolygonF &poly) {
            if (poly.size() >= 2) {
                painter->drawPolyline(poly);
                if (brush().style() != Qt::NoBrush)
                    fillCurvePolygonF(painter, this, xMap, yMap, poly);
            } else if (poly.size() == 1) {
                painter->drawPoint(poly[0]);
            }
            poly.clear();
        };

        QPolygonF poly;
        poly.reserve(2 * (to - from) + 1);
        for (int i = from; i <= to; ++i) {
            const double px = xMap.xTransform(x(i));
            const double py = yMap.xTransform(y(i));
            if (!std::isfinite(px) || !std::isfinite(py)) {
                flushStepRun(poly);
                continue;
            }
            const QPointF pt = clampExtremePoint(QPointF(px, py));
            if (!poly.isEmpty()) {
                const QPointF prev = poly.last();
                poly.append(inverted ? QPointF(prev.x(), pt.y()) : QPointF(pt.x(), prev.y()));
            }
            poly.append(pt);
        }
        flushStepRun(poly);
        break;
    }
    case Dots: {
        const bool doFill = (brush().style() != Qt::NoBrush);
        QPolygonF pts;
        pts.reserve(to - from + 1);
        for (int i = from; i <= to; ++i) {
            const double px = xMap.xTransform(x(i));
            const double py = yMap.xTransform(y(i));
            if (!std::isfinite(px) || !std::isfinite(py))
                continue;
            pts.append(clampExtremePoint(QPointF(px, py)));
        }
        if (!pts.isEmpty()) {
            painter->drawPoints(pts);
            if (doFill && pts.size() >= 2)
                fillCurvePolygonF(painter, this, xMap, yMap, pts);
        }
        break;
    }
    case NoCurve:
    default:
        break;
    }
}

void PlotCurve::drawSymbols(QPainter *painter, const QwtSymbol &sym, const QwtScaleMap &xMap,
                            const QwtScaleMap &yMap, int from, int to) const
{
    if (!painter || dataSize() <= 0 || sym.style() == QwtSymbol::NoSymbol)
        return;

    const int n = dataSize();
    from = qMax(0, from);
    to = (to < 0) ? (n - 1) : qMin(to, n - 1);
    if (from > to)
        return;

    if (testRenderHint(QwtPlotItem::RenderAntialiased))
        painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setBrush(sym.brush());
    painter->setPen(QwtPainter::scaledPen(sym.pen()));

    const QSize layoutSz = QwtPainter::metricsMap().screenToLayout(sym.size());
    const double w = double(layoutSz.width());
    const double h = double(layoutSz.height());
    if (w <= 0.0 || h <= 0.0)
        return;

    const Graph *g = plot() ? qobject_cast<const Graph *>(plot()->parent()) : nullptr;
    bool hasXBreak = false;
    double xBf = 0.0, xBt = 0.0, xEps = 0.0;
    bool hasYBreak = false;
    double yBf = 0.0, yBt = 0.0, yEps = 0.0;
    if (g) {
        Graph::AxisBreakSpec xb = g->axisBreak(xAxis());
        if (!xb.enabled && xAxis() == QwtPlot::xTop && !g->hasColorScaleOnAxis(QwtPlot::xTop))
            xb = g->axisBreak(QwtPlot::xBottom);
        if (xb.enabled && std::isfinite(xb.breakFrom) && std::isfinite(xb.breakTo)
            && xb.breakFrom != xb.breakTo) {
            hasXBreak = true;
            xBf = qMin(xb.breakFrom, xb.breakTo);
            xBt = qMax(xb.breakFrom, xb.breakTo);
            xEps = 1e-12 * qMax(1.0, xBt - xBf);
        }
        Graph::AxisBreakSpec yb = g->axisBreak(yAxis());
        if (!yb.enabled && yAxis() == QwtPlot::yRight && !g->hasColorScaleOnAxis(QwtPlot::yRight))
            yb = g->axisBreak(QwtPlot::yLeft);
        if (yb.enabled && std::isfinite(yb.breakFrom) && std::isfinite(yb.breakTo)
            && yb.breakFrom != yb.breakTo) {
            hasYBreak = true;
            yBf = qMin(yb.breakFrom, yb.breakTo);
            yBt = qMax(yb.breakFrom, yb.breakTo);
            yEps = 1e-12 * qMax(1.0, yBt - yBf);
        }
    }

    const bool hasCanvasBounds = d_canvas_rect.isValid() && d_canvas_rect.width() > 1.0
            && d_canvas_rect.height() > 1.0;
    const QRectF validCenterBox = hasCanvasBounds
            ? QRectF(QPointF(d_canvas_rect.left() - 1.5, d_canvas_rect.top() - 1.5),
                     QPointF(d_canvas_rect.right() + 0.5, d_canvas_rect.bottom() + 0.5))
            : QRectF();
    const bool expandClip = hasCanvasBounds && painter->hasClipping();
    if (expandClip) {
        painter->save();
        const double symPenW = qMax(1.0, painter->pen().widthF());
        const double padX = 0.5 * w + symPenW + 2.0;
        const double padY = 0.5 * h + symPenW + 2.0;
        painter->setClipRect(d_canvas_rect.adjusted(-padX, -padY, padX, padY));
    }

    const bool decimateSubPixel = testPaintAttribute(PaintFiltered) && ((to - from) > 4000);
    QPointF lastDrawn(1e30, 1e30);

    for (int i = from; i <= to; ++i) {
        const double vx = x(i);
        const double vy = y(i);
        if ((hasXBreak && vx > xBf + xEps && vx < xBt - xEps)
            || (hasYBreak && vy > yBf + yEps && vy < yBt - yEps))
            continue;
        const double cx = xMap.xTransform(vx);
        const double cy = yMap.xTransform(vy);
        if (!std::isfinite(cx) || !std::isfinite(cy))
            continue;
        if (std::fabs(cx) > kMaxSafeCoord || std::fabs(cy) > kMaxSafeCoord)
            continue;
        if (hasCanvasBounds && !validCenterBox.contains(QPointF(cx, cy)))
            continue;
        if (decimateSubPixel && i > from && i < to) {
            const double dx = cx - lastDrawn.x();
            const double dy = cy - lastDrawn.y();
            if (dx * dx + dy * dy < 0.04)
                continue;
        }
        drawSymbolShapeF(painter, sym.style(), cx, cy, w, h);
        lastDrawn = QPointF(cx, cy);
    }

    if (expandClip)
        painter->restore();
}
