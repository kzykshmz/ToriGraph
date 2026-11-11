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
#include "ScaleDraw.h"
#include "core/column/Column.h"
#include "core/datatypes/DateTime2StringFilter.h"
#include <QDateTime>
#include <QMessageBox>
#include <QPainter>
#include <cmath>
#include <qwt_scale_map.h>
#include <qwt_symbol.h>

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
    d_start_row = 0;
    d_end_row = d_table->numRows() - 1;

    loadData();

    foreach (DataCurve *c, d_error_bars)
        c->loadData();
}

bool DataCurve::isFullRange()
{
    if (d_start_row != 0 || d_end_row != d_table->numRows() - 1)
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
    if (updateTableName) {
        QString s = title().text();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList lst = s.split("_", Qt::KeepEmptyParts);
#else
        QStringList lst = s.split("_", QString::KeepEmptyParts);
#endif
        if (lst.size() > 0 && lst[0] == oldName)
            setTitle(newName + "_" + lst.mid(1).join("_"));

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        lst = d_x_column.split("_", Qt::KeepEmptyParts);
#else
        lst = d_x_column.split("_", QString::KeepEmptyParts);
#endif
        if (lst.size() > 0 && lst[0] == oldName)
            d_x_column = newName + "_" + lst.mid(1).join("_");
    } else {
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
    Column *x_col_ptr = d_table->column(d_x_column);
    Column *y_col_ptr = d_table->column(title().text());
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
    Column *yCol = d_table->column(d_table->colIndex(title().text()));
    if (!yCol)
        yCol = d_table->column(title().text());
    if (yCol && yCol->isMasked(row))
        return true;
    Column *xCol = d_table->column(d_table->colIndex(d_x_column));
    if (!xCol)
        xCol = d_table->column(d_x_column);
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
    Column *yCol = d_table->column(d_table->colIndex(title().text()));
    if (!yCol)
        yCol = d_table->column(title().text());
    if (!yCol)
        return;

    yCol->setMasked(row, mask);
    if (!mask) {
        Column *xCol = d_table->column(d_table->colIndex(d_x_column));
        if (!xCol)
            xCol = d_table->column(d_x_column);
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
    Column *yCol = d_table->column(d_table->colIndex(title().text()));
    if (!yCol)
        yCol = d_table->column(title().text());
    if (!yCol)
        return 0;
    Column *xCol = d_table->column(d_table->colIndex(d_x_column));
    if (!xCol)
        xCol = d_table->column(d_x_column);

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
    Column *yCol = d_table->column(d_table->colIndex(title().text()));
    if (!yCol)
        yCol = d_table->column(title().text());
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
    const int r = qMax(5, symbol().size().width() / 2 + 2);

    for (int i = startIdx; i <= endIdx; ++i) {
        if (!isPointMasked(i))
            continue;
        const int px = xMap.transform(x(i));
        const int py = yMap.transform(y(i));
        painter->drawRect(QRect(px - r, py - r, 2 * r, 2 * r));
        painter->drawLine(px - r + 1, py - r + 1, px + r - 1, py + r - 1);
        painter->drawLine(px - r + 1, py + r - 1, px + r - 1, py - r + 1);
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
    if (symbol().style() == QwtSymbol::NoSymbol)
        return r;

    int margin = 1;
    if (symbol().style() != QwtSymbol::NoSymbol)
        margin += symbol().size().width();

    const QwtScaleMap &xMap = plot()->canvasMap(xAxis());
    const QwtScaleMap &yMap = plot()->canvasMap(yAxis());

    int x_right = xMap.transform(r.right());
    double d_x_right = xMap.invTransform(x_right + margin);
    int x_left = xMap.transform(r.left());
    double d_x_left = xMap.invTransform(x_left - margin);

    int y_top = yMap.transform(r.top());
    double d_y_top = yMap.invTransform(y_top + margin);
    int y_bottom = yMap.transform(r.bottom());
    double d_y_bottom = yMap.invTransform(y_bottom - margin);

    return QwtDoubleRect(d_x_left, d_y_top, qAbs(d_x_right - d_x_left), qAbs(d_y_bottom - d_y_top));
}
