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
#include "Spectrogram.h"
#include <limits>
#include "core/column/Column.h"
#include "core/datatypes/DateTime2StringFilter.h"
#include <QDateTime>
#include <QLineF>
#include <QMessageBox>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <vector>
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

        if (d_point_cmap.zColumn.startsWith(prefix))
            d_point_cmap.zColumn = newName + QLatin1Char('_') + d_point_cmap.zColumn.mid(prefix.size());
        if (d_point_cmap.sizeColumn.startsWith(prefix)) {
            d_point_cmap.sizeColumn =
                    newName + QLatin1Char('_') + d_point_cmap.sizeColumn.mid(prefix.size());
        }
    } else {
        QString candOld;
        QString candNew;
        if (d_table) {
            const QString tblPrefix = QString(d_table->name()) + QLatin1Char('_');
            if (!oldName.startsWith(tblPrefix) || !newName.startsWith(tblPrefix))
                return;
            candOld = oldName.mid(tblPrefix.size());
            candNew = newName.mid(tblPrefix.size());
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

        auto remapPointCol = [&](QString &colRef) {
            if (colRef.isEmpty())
                return;
            if (colRef == oldName)
                colRef = newName;
            else if (!candOld.isEmpty() && colRef == candOld)
                colRef = candNew;
        };
        remapPointCol(d_point_cmap.zColumn);
        remapPointCol(d_point_cmap.sizeColumn);
    }
}

bool DataCurve::updateData(Table *t, const QString &colName)
{
    if (d_table != t)
        return false;

    const bool matchesXY = (colName == title().text() || d_x_column == colName);
    auto matchesPointCol = [&](const QString &ref) -> bool {
        if (ref.isEmpty())
            return false;
        if (ref == colName)
            return true;
        const QString pfx = t->name() + QLatin1Char('_');
        return (colName.startsWith(pfx) && colName.mid(pfx.size()) == ref)
                || (ref.startsWith(pfx) && ref.mid(pfx.size()) == colName);
    };
    const bool matchesPointMap = hasPointColorMap()
            && (matchesPointCol(d_point_cmap.zColumn) || matchesPointCol(d_point_cmap.sizeColumn));
    if (!matchesXY && !matchesPointMap)
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

    // determine rows for which all columns have valid, finite content and track gap boundaries
    QList<int> valid_rows;
    QVector<int> gap_before;
    bool saw_invalid_after_valid = false;
    for (int row = d_start_row; row <= end_row; row++) {
        bool all_valid = true;
        foreach (Column *col, cols) {
            if (col->isInvalid(row)) {
                all_valid = false;
                break;
            }
            if (col->columnMode() == SciDAVis::ColumnMode::Numeric
                && !std::isfinite(col->valueAt(row))) {
                all_valid = false;
                break;
            }
        }
        if (all_valid) {
            if (saw_invalid_after_valid && !valid_rows.isEmpty()) {
                gap_before.push_back(valid_rows.size());
                saw_invalid_after_valid = false;
            }
            valid_rows.push_back(row);
        } else if (!valid_rows.isEmpty()) {
            saw_invalid_after_valid = true;
        }
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
            if (g && d_type == Graph::HorizontalBars && i == 0
                && g->axesType()[axes[i]] == Graph::AxisType::Txt) {
                g->setLabelsNumericFormat(axes[i], 0, 6);
            }
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
    d_gap_before_indices = gap_before;
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

    const int idx = g->plotItemIndex(this);
    if (idx >= 0) {
        g->removeCurve(idx);
        return;
    }
    const QString curveTitle = title().text();
    g->removeCurve(curveTitle);
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

void DataCurve::resolveColumns(Column *&xColOut, Column *&yColOut) const
{
    xColOut = nullptr;
    yColOut = nullptr;
    if (!d_table)
        return;
    yColOut = resolveDataCurveColumn(d_table, title().text());
    xColOut = resolveDataCurveColumn(d_table, d_x_column);
}

bool DataCurve::hasAnyMaskedPoints() const
{
    if (!d_table || dataSize() <= 0)
        return false;
    Column *xCol = nullptr;
    Column *yCol = nullptr;
    resolveColumns(xCol, yCol);
    if (yCol && !yCol->maskedIntervals().isEmpty())
        return true;
    if (xCol && !xCol->maskedIntervals().isEmpty())
        return true;
    return false;
}

bool DataCurve::isPointMaskedFast(int point, const Column *xCol, const Column *yCol) const
{
    if (!d_table || point < 0 || point >= dataSize())
        return false;
    const int row = tableRow(point);
    if (row < 0)
        return false;
    if (yCol && yCol->isMasked(row))
        return true;
    if (xCol && xCol->isMasked(row))
        return true;
    return false;
}

bool DataCurve::hasGapBefore(int point) const
{
    if (d_gap_before_indices.isEmpty() || point <= 0)
        return false;
    return std::binary_search(d_gap_before_indices.constBegin(), d_gap_before_indices.constEnd(),
                              point);
}

bool DataCurve::isPointMasked(int point) const
{
    if (!d_table || point < 0 || point >= dataSize())
        return false;
    Column *xCol = nullptr;
    Column *yCol = nullptr;
    resolveColumns(xCol, yCol);
    return isPointMaskedFast(point, xCol, yCol);
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
    QVector<int> rowsToModify;
    for (int i = 0; i < n; ++i) {
        const double px = x(i);
        const double py = y(i);
        if (px >= normRect.left() && px <= normRect.right() && py >= normRect.top()
            && py <= normRect.bottom()) {
            const int row = tableRow(i);
            if (row < 0)
                continue;
            const bool wasMasked = isPointMaskedFast(i, xCol, yCol);
            if (wasMasked != mask || (!mask && xCol && xCol->isMasked(row)))
                rowsToModify.append(row);
        }
    }

    if (!rowsToModify.isEmpty()) {
        future::Table *ft = d_table->d_future_table;
        if (ft)
            ft->beginMacro(mask ? QObject::tr("Mask Region") : QObject::tr("Unmask Region"));
        for (int row : rowsToModify) {
            yCol->setMasked(row, mask);
            if (!mask && xCol && xCol->isMasked(row))
                xCol->setMasked(row, false);
        }
        if (ft)
            ft->endMacro();
        modifiedCount = rowsToModify.size();
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
    Column *xCol = nullptr;
    Column *yCol = nullptr;
    resolveColumns(xCol, yCol);
    if ((!yCol || yCol->maskedIntervals().isEmpty())
        && (!xCol || xCol->maskedIntervals().isEmpty())) {
        return 0;
    }
    int count = 0;
    const int n = dataSize();
    for (int i = 0; i < n; ++i) {
        if (isPointMaskedFast(i, xCol, yCol))
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

    Column *xCol = nullptr;
    Column *yCol = nullptr;
    resolveColumns(xCol, yCol);
    const bool possiblyMasked = (yCol && !yCol->maskedIntervals().isEmpty())
            || (xCol && !xCol->maskedIntervals().isEmpty());

    bool anyMasked = false;
    if (possiblyMasked) {
        for (int i = startIdx; i <= endIdx; ++i) {
            if (isPointMaskedFast(i, xCol, yCol)) {
                anyMasked = true;
                break;
            }
        }
    }

    const bool anyGaps = !d_gap_before_indices.isEmpty();
    if (!anyMasked && !anyGaps) {
        PlotCurve::draw(painter, xMap, yMap, from, to);
        return;
    }

    if (!anyMasked && anyGaps) {
        int curStart = startIdx;
        auto it = std::upper_bound(d_gap_before_indices.constBegin(),
                                   d_gap_before_indices.constEnd(), startIdx);
        while (it != d_gap_before_indices.constEnd() && *it <= endIdx) {
            const int gapIdx = *it;
            if (gapIdx - 1 >= curStart)
                PlotCurve::draw(painter, xMap, yMap, curStart, gapIdx - 1);
            curStart = gapIdx;
            ++it;
        }
        if (curStart <= endIdx)
            PlotCurve::draw(painter, xMap, yMap, curStart, endIdx);
        return;
    }

    int segStart = -1;
    for (int i = startIdx; i <= endIdx + 1; ++i) {
        const bool gapHere = anyGaps && i <= endIdx && i > startIdx && hasGapBefore(i);
        if (gapHere && segStart >= 0) {
            PlotCurve::draw(painter, xMap, yMap, segStart, i - 1);
            segStart = -1;
        }
        const bool masked = (i > endIdx) || isPointMaskedFast(i, xCol, yCol);
        if (!masked && segStart < 0) {
            segStart = i;
        } else if (masked && segStart >= 0) {
            PlotCurve::draw(painter, xMap, yMap, segStart, i - 1);
            segStart = -1;
        }
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const double sc = renderPenScale();
    QPen maskPen(QColor(220, 30, 30), 2.0 * sc, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
    maskPen.setCosmetic(false);
    painter->setPen(maskPen);
    painter->setBrush(Qt::NoBrush);
    const double r = qMax(5.0, 0.5 * symbol().size().width() + 2.0) * sc;
    const AxisBreakBands brk = axisBreakBands(xMap, yMap);

    for (int i = startIdx; i <= endIdx; ++i) {
        if (!isPointMaskedFast(i, xCol, yCol))
            continue;
        const double vx = x(i);
        const double vy = y(i);
        if (brk.valueInGap(vx, vy))
            continue;
        const double px = xMap.xTransform(vx);
        const double py = yMap.xTransform(vy);
        if (!std::isfinite(px) || !std::isfinite(py))
            continue;
        painter->drawRect(QRectF(px - r, py - r, 2.0 * r, 2.0 * r));
        painter->drawLine(QPointF(px - r + sc, py - r + sc),
                          QPointF(px + r - sc, py + r - sc));
        painter->drawLine(QPointF(px - r + sc, py + r - sc),
                          QPointF(px + r - sc, py - r + sc));
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

void PlotCurve::setSymbolSkip(int everyN)
{
    const int clamped = qMax(0, everyN);
    if (d_symbol_skip == clamped)
        return;
    d_symbol_skip = clamped;
    itemChanged();
}

int PlotCurve::symbolSkip() const
{
    return d_symbol_skip;
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

    const double rawMinX = qMin(r.left(), r.right());
    const double rawMaxX = qMax(r.left(), r.right());
    const double rawMinY = qMin(r.top(), r.bottom());
    const double rawMaxY = qMax(r.top(), r.bottom());

    double minX = qMin(d_x_left, d_x_right);
    double maxX = qMax(d_x_left, d_x_right);
    double minY = qMin(d_y_top, d_y_bottom);
    double maxY = qMax(d_y_top, d_y_bottom);
    if (rawMinX >= 0.0 && rawMaxX > 0.0 && minX < 0.0)
        minX = 0.0;
    if (rawMinY >= 0.0 && rawMaxY > 0.0 && minY < 0.0)
        minY = 0.0;
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
                       const QwtScaleMap &yMap, const QPolygonF &openPoly,
                       const PlotCurve::AxisBreakBands *bands = nullptr)
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
    if (bands && bands->any()) {
        // The area fill must not bridge an axis-break gap (neither the X gap between the two
        // sides nor the Y gap between the curve and a baseline on the other side of the break).
        const QVector<QPolygonF> pieces = PlotCurve::clipPolygonToBreaks(closed, *bands);
        for (const QPolygonF &piece : pieces)
            painter->drawPolygon(piece);
    } else {
        painter->drawPolygon(closed);
    }
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

namespace {

double screenDisplayScaleForPlot(const QwtPlot *qwtPlot)
{
    if (!QwtPainter::metricsMap().isIdentity())
        return 1.0;
    const Plot *pl = dynamic_cast<const Plot *>(qwtPlot);
    if (!pl)
        return 1.0;
    const double s = pl->displayFontScale();
    if (!std::isfinite(s) || std::fabs(s - 1.0) <= 1e-4)
        return 1.0;
    return qBound(0.25, s, 4.0);
}

QPen scaleFractionalPenForLayout(const QPen &srcPen, double screenScale = 1.0)
{
    if (srcPen.style() == Qt::NoPen)
        return srcPen;
    QPen out = srcPen;
    double wf = srcPen.widthF();
    if (wf <= 0.0)
        wf = (srcPen.width() > 0) ? double(srcPen.width()) : 1.0;
    const QwtMetricsMap &mm = QwtPainter::metricsMap();
    if (!mm.isIdentity()) {
        const double refPx = 1000.0;
        const double scaleX = double(mm.screenToLayoutX(int(refPx))) / refPx;
        if (scaleX > 0.0 && std::isfinite(scaleX))
            wf *= scaleX;
    } else if (screenScale != 1.0) {
        wf *= screenScale;
    }
    out.setCosmetic(false);
    out.setWidthF(wf);
    return out;
}

QPolygonF clipPolygonHalfPlane(const QPolygonF &poly, bool alongX, double edge, bool keepLess)
{
    QPolygonF out;
    if (poly.size() < 3)
        return out;
    auto coord = [alongX](const QPointF &p) { return alongX ? p.x() : p.y(); };
    auto inside = [&](const QPointF &p) { return keepLess ? coord(p) <= edge : coord(p) >= edge; };
    auto intersect = [&](const QPointF &a, const QPointF &b) {
        const double ca = coord(a);
        const double cb = coord(b);
        const double d = cb - ca;
        const double t = (std::fabs(d) > 1e-12) ? (edge - ca) / d : 0.0;
        QPointF r = a + t * (b - a);
        if (alongX)
            r.setX(edge);
        else
            r.setY(edge);
        return r;
    };
    out.reserve(poly.size() + 4);
    QPointF prev = poly.last();
    bool prevIn = inside(prev);
    for (const QPointF &cur : poly) {
        const bool curIn = inside(cur);
        if (curIn) {
            if (!prevIn)
                out.append(intersect(prev, cur));
            out.append(cur);
        } else if (prevIn) {
            out.append(intersect(prev, cur));
        }
        prev = cur;
        prevIn = curIn;
    }
    return out;
}

} // namespace

PlotCurve::AxisBreakBands PlotCurve::axisBreakBands(const QwtScaleMap &xMap,
                                                    const QwtScaleMap &yMap) const
{
    AxisBreakBands b;
    const Graph *g = plot() ? qobject_cast<const Graph *>(plot()->parent()) : nullptr;
    if (!g)
        return b;
    const double halfGap = kAxisBreakHalfGapPx * renderPenScale();
    Graph::AxisBreakSpec xb = g->axisBreak(xAxis());
    if (!xb.enabled && xAxis() == QwtPlot::xTop && !g->hasColorScaleOnAxis(QwtPlot::xTop))
        xb = g->axisBreak(QwtPlot::xBottom);
    if (xb.enabled && std::isfinite(xb.breakFrom) && std::isfinite(xb.breakTo)
        && xb.breakFrom != xb.breakTo) {
        const double bf = qMin(xb.breakFrom, xb.breakTo);
        const double bt = qMax(xb.breakFrom, xb.breakTo);
        const double sLo = qMin(xMap.s1(), xMap.s2());
        const double sHi = qMax(xMap.s1(), xMap.s2());
        if (bf > sLo && bt < sHi && bt > bf + 1e-15) {
            b.xBf = bf;
            b.xBt = bt;
            b.xBrkPx = 0.5 * (xMap.xTransform(b.xBf) + xMap.xTransform(b.xBt));
            b.xEps = 1e-12 * qMax(1.0, b.xBt - b.xBf);
            b.xLo = b.xBrkPx - halfGap;
            b.xHi = b.xBrkPx + halfGap;
            b.hasX = std::isfinite(b.xBrkPx);
        }
    }
    Graph::AxisBreakSpec yb = g->axisBreak(yAxis());
    if (!yb.enabled && yAxis() == QwtPlot::yRight && !g->hasColorScaleOnAxis(QwtPlot::yRight))
        yb = g->axisBreak(QwtPlot::yLeft);
    if (yb.enabled && std::isfinite(yb.breakFrom) && std::isfinite(yb.breakTo)
        && yb.breakFrom != yb.breakTo) {
        const double bf = qMin(yb.breakFrom, yb.breakTo);
        const double bt = qMax(yb.breakFrom, yb.breakTo);
        const double sLo = qMin(yMap.s1(), yMap.s2());
        const double sHi = qMax(yMap.s1(), yMap.s2());
        if (bf > sLo && bt < sHi && bt > bf + 1e-15) {
            b.yBf = bf;
            b.yBt = bt;
            b.yBrkPx = 0.5 * (yMap.xTransform(b.yBf) + yMap.xTransform(b.yBt));
            b.yEps = 1e-12 * qMax(1.0, b.yBt - b.yBf);
            b.yLo = b.yBrkPx - halfGap;
            b.yHi = b.yBrkPx + halfGap;
            b.hasY = std::isfinite(b.yBrkPx);
        }
    }
    return b;
}

void PlotCurve::appendSegmentOutsideBreaks(const QLineF &seg, const AxisBreakBands &b,
                                           QVector<QLineF> &out)
{
    if (!b.any()) {
        out.append(seg);
        return;
    }
    double cut[2][2] = { { 0.0, 0.0 }, { 0.0, 0.0 } };
    int nCut = 0;
    // Returns false when the whole segment lies inside the band (axis-parallel segment).
    auto addCut = [&](double p0, double p1, double lo, double hi) -> bool {
        const double d = p1 - p0;
        if (std::fabs(d) < 1e-9)
            return !(p0 > lo && p0 < hi);
        double t0 = (lo - p0) / d;
        double t1 = (hi - p0) / d;
        if (t0 > t1)
            std::swap(t0, t1);
        t0 = qMax(0.0, t0);
        t1 = qMin(1.0, t1);
        if (t1 > t0) {
            cut[nCut][0] = t0;
            cut[nCut][1] = t1;
            ++nCut;
        }
        return true;
    };
    if (b.hasX && !addCut(seg.x1(), seg.x2(), b.xLo, b.xHi))
        return;
    if (b.hasY && !addCut(seg.y1(), seg.y2(), b.yLo, b.yHi))
        return;
    if (nCut == 0) {
        out.append(seg);
        return;
    }
    if (nCut == 2 && cut[1][0] < cut[0][0]) {
        std::swap(cut[0][0], cut[1][0]);
        std::swap(cut[0][1], cut[1][1]);
    }
    double cur = 0.0;
    for (int k = 0; k < nCut; ++k) {
        if (cut[k][0] > cur + 1e-9)
            out.append(QLineF(seg.pointAt(cur), seg.pointAt(cut[k][0])));
        cur = qMax(cur, cut[k][1]);
    }
    if (cur < 1.0 - 1e-9)
        out.append(QLineF(seg.pointAt(cur), seg.p2()));
}

QVector<QPolygonF> PlotCurve::splitPolylineAtBreaks(const QPolygonF &poly, const AxisBreakBands &b)
{
    QVector<QPolygonF> out;
    if (poly.isEmpty())
        return out;
    if (!b.any()) {
        out.append(poly);
        return out;
    }
    if (poly.size() == 1) {
        const QPointF &p = poly.first();
        const bool inX = b.hasX && p.x() > b.xLo && p.x() < b.xHi;
        const bool inY = b.hasY && p.y() > b.yLo && p.y() < b.yHi;
        if (!inX && !inY)
            out.append(poly);
        return out;
    }
    QPolygonF cur;
    QVector<QLineF> segs;
    for (int i = 1; i < poly.size(); ++i) {
        segs.clear();
        appendSegmentOutsideBreaks(QLineF(poly[i - 1], poly[i]), b, segs);
        for (const QLineF &s : segs) {
            if (!cur.isEmpty() && std::fabs(cur.last().x() - s.x1()) < 1e-9
                && std::fabs(cur.last().y() - s.y1()) < 1e-9) {
                cur.append(s.p2());
            } else {
                if (cur.size() >= 2)
                    out.append(cur);
                cur.clear();
                cur.append(s.p1());
                cur.append(s.p2());
            }
        }
    }
    if (cur.size() >= 2)
        out.append(cur);
    return out;
}

QVector<QPolygonF> PlotCurve::clipPolygonToBreaks(const QPolygonF &closedPoly,
                                                  const AxisBreakBands &b)
{
    QVector<QPolygonF> pieces;
    if (closedPoly.size() < 3)
        return pieces;
    pieces.append(closedPoly);
    if (!b.any())
        return pieces;
    auto splitAlong = [&pieces](bool alongX, double lo, double hi) {
        QVector<QPolygonF> next;
        next.reserve(2 * pieces.size());
        for (const QPolygonF &p : pieces) {
            const QPolygonF a = clipPolygonHalfPlane(p, alongX, lo, true);
            if (a.size() >= 3)
                next.append(a);
            const QPolygonF c = clipPolygonHalfPlane(p, alongX, hi, false);
            if (c.size() >= 3)
                next.append(c);
        }
        pieces.swap(next);
    };
    if (b.hasX)
        splitAlong(true, b.xLo, b.xHi);
    if (b.hasY)
        splitAlong(false, b.yLo, b.yHi);
    return pieces;
}

double PlotCurve::renderPenScale() const
{
    const QwtMetricsMap &mm = QwtPainter::metricsMap();
    if (!mm.isIdentity()) {
        const double refPx = 1000.0;
        const double mmScale = double(mm.screenToLayoutX(int(refPx))) / refPx;
        if (std::isfinite(mmScale) && mmScale > 0.0)
            return qMax(Plot::printDeviceScale(), mmScale);
        return Plot::printDeviceScale();
    }
    const double s = Plot::printDeviceScale() * screenDisplayScaleForPlot(plot());
    return (std::isfinite(s) && s > 0.0) ? s : 1.0;
}

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

    if (pen().style() != Qt::NoPen)
        painter->setPen(scaleFractionalPenForLayout(pen(), screenDisplayScaleForPlot(plot())));

    const int n = dataSize();
    from = qMax(0, from);
    to = (to < 0) ? (n - 1) : qMin(to, n - 1);
    if (from > to)
        return;

    const AxisBreakBands brk = axisBreakBands(xMap, yMap);

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
                if (brk.any() && run.size() >= 2) {
                    const QVector<QPolygonF> pieces = splitPolylineAtBreaks(run, brk);
                    for (const QPolygonF &piece : pieces) {
                        if (piece.size() >= 2) {
                            painter->drawPolyline(piece);
                            if (brush().style() != Qt::NoBrush)
                                fillCurvePolygonF(painter, this, xMap, yMap, piece, &brk);
                        } else if (piece.size() == 1) {
                            painter->drawPoint(piece[0]);
                        }
                    }
                    run.clear();
                    return;
                }
            }
            if (run.size() >= 2) {
                painter->drawPolyline(run);
                if (brush().style() != Qt::NoBrush)
                    fillCurvePolygonF(painter, this, xMap, yMap, run, &brk);
            } else if (run.size() == 1) {
                painter->drawPoint(run[0]);
            }
            run.clear();
        };

        // 4-point (first, min, max, last) per-device-pixel-column envelope decimation for dense
        // monotonic X waveforms (e.g. 200k points -> <= 4 * devPixelWidth vertices, preserving
        // 100% of visual peak-to-peak amplitude and exact entry/exit transitions).
        const QTransform &xf = painter->transform();
        double txScale = std::max(1.0, std::hypot(xf.m11(), xf.m12()));
        const double dpr = painter->device()
                ? std::max(1.0, double(painter->device()->devicePixelRatioF()))
                : 1.0;
        const bool isRasterEngine =
                painter->paintEngine() && (painter->paintEngine()->type() == QPaintEngine::Raster);
        if (!isRasterEngine)
            txScale = std::max(txScale, 600.0 / 96.0);
        const double colScale = txScale * dpr;
        const double devPixelWidth = std::max(250.0, std::fabs(xMap.pDist()) * colScale);
        const int spanCount = to - from + 1;

        if (!isFitted && !brk.any()
            && spanCount > qMax(2000, int(4.0 * devPixelWidth))) {
            bool monoAsc = true;
            bool monoDesc = true;
            bool haveFiniteX = false;
            double prevFiniteX = 0.0;
            for (int i = from; i <= to; ++i) {
                const double vx = x(i);
                if (!std::isfinite(vx))
                    continue;
                if (haveFiniteX) {
                    if (vx < prevFiniteX)
                        monoAsc = false;
                    if (vx > prevFiniteX)
                        monoDesc = false;
                    if (!monoAsc && !monoDesc)
                        break;
                }
                prevFiniteX = vx;
                haveFiniteX = true;
            }

            if (haveFiniteX && (monoAsc || monoDesc)) {
                const bool canChunkUnfilledPolyline = (brush().style() == Qt::NoBrush && !isFitted);
                const bool useDisjointSolidEnvLines =
                        (canChunkUnfilledPolyline && isRasterEngine
                         && painter->pen().style() == Qt::SolidLine);

                if (useDisjointSolidEnvLines) {
                    QPen segPen = painter->pen();
                    if (segPen.capStyle() == Qt::FlatCap) {
                        segPen.setCapStyle(Qt::RoundCap);
                        painter->setPen(segPen);
                    }
                    QVector<QLineF> envSegs;
                    envSegs.reserve(64);
                    const double devPenW = std::max(1.0, segPen.widthF() * colScale);
                    const double bucketScale = colScale / std::max(1.0, 0.45 * devPenW);
                    const double minVertSpan = 0.35 / colScale;

                    bool hasBucket = false;
                    bool hasPrevLast = false;
                    bool prevHadVertBar = false;
                    double prevMinY = 0.0, prevMaxY = 0.0;
                    qint64 curCol = 0;
                    int firstIdx = -1, lastIdx = -1;
                    QPointF firstPt, minPt, maxPt, lastPt, prevLastPt;

                    auto flushSolidBucket = [&]() {
                        if (!hasBucket)
                            return;
                        const bool curHasVertBar = (maxPt.y() - minPt.y() > minVertSpan);
                        const bool overlapAdjacentBars = hasPrevLast && prevHadVertBar
                                && curHasVertBar
                                && (std::max(prevMinY, minPt.y()) <= std::min(prevMaxY, maxPt.y()));
                        if (hasPrevLast && !overlapAdjacentBars
                            && (std::fabs(firstPt.x() - prevLastPt.x()) > 1e-6
                                || std::fabs(firstPt.y() - prevLastPt.y()) > 1e-6)) {
                            envSegs.append(QLineF(prevLastPt, firstPt));
                        }
                        if (curHasVertBar) {
                            const double midX = 0.5 * (firstPt.x() + lastPt.x());
                            envSegs.append(
                                    QLineF(QPointF(midX, minPt.y()), QPointF(midX, maxPt.y())));
                        } else if (lastIdx != firstIdx
                                   && (std::fabs(lastPt.x() - firstPt.x()) > 1e-6
                                       || std::fabs(lastPt.y() - firstPt.y()) > 1e-6)) {
                            envSegs.append(QLineF(firstPt, lastPt));
                        }
                        prevLastPt = lastPt;
                        prevHadVertBar = curHasVertBar;
                        prevMinY = minPt.y();
                        prevMaxY = maxPt.y();
                        hasPrevLast = true;
                        hasBucket = false;
                        if (envSegs.size() >= 64) {
                            painter->drawLines(envSegs);
                            envSegs.clear();
                        }
                    };

                    for (int i = from; i <= to; ++i) {
                        const double px = xMap.xTransform(x(i));
                        const double py = yMap.xTransform(y(i));
                        if (!std::isfinite(px) || !std::isfinite(py)) {
                            flushSolidBucket();
                            if (!envSegs.isEmpty()) {
                                painter->drawLines(envSegs);
                                envSegs.clear();
                            }
                            hasPrevLast = false;
                            prevHadVertBar = false;
                            continue;
                        }
                        const QPointF pt = clampExtremePoint(QPointF(px, py));
                        const qint64 col = static_cast<qint64>(std::floor(pt.x() * bucketScale));
                        if (!hasBucket || col != curCol) {
                            flushSolidBucket();
                            hasBucket = true;
                            curCol = col;
                            firstIdx = lastIdx = i;
                            firstPt = minPt = maxPt = lastPt = pt;
                        } else {
                            if (pt.y() < minPt.y())
                                minPt = pt;
                            if (pt.y() > maxPt.y())
                                maxPt = pt;
                            lastIdx = i;
                            lastPt = pt;
                        }
                    }
                    flushSolidBucket();
                    if (!envSegs.isEmpty())
                        painter->drawLines(envSegs);
                    break;
                }

                QPolygonF envRun;
                const int maxEnvChunkPts = (painter->pen().style() == Qt::SolidLine) ? 16 : 64;
                envRun.reserve(canChunkUnfilledPolyline
                                       ? (maxEnvChunkPts + 4)
                                       : qMin(spanCount, qMax(2048, int(4.0 * devPixelWidth) + 16)));
                auto appendEnvPt = [&](const QPointF &p) {
                    if (!envRun.isEmpty()) {
                        const QPointF &last = envRun.last();
                        if (std::fabs(p.x() - last.x()) < 1e-6
                            && std::fabs(p.y() - last.y()) < 1e-6)
                            return;
                    }
                    envRun.append(p);
                    if (canChunkUnfilledPolyline && envRun.size() >= maxEnvChunkPts) {
                        const QPointF carry = envRun.last();
                        painter->drawPolyline(envRun);
                        envRun.clear();
                        envRun.append(carry);
                    }
                };

                bool hasBucket = false;
                qint64 curCol = 0;
                int firstIdx = -1, minIdx = -1, maxIdx = -1, lastIdx = -1;
                QPointF firstPt, minPt, maxPt, lastPt;

                auto flushBucket = [&]() {
                    if (!hasBucket)
                        return;
                    appendEnvPt(firstPt);
                    if (minIdx < maxIdx) {
                        if (minIdx != firstIdx && minIdx != lastIdx)
                            appendEnvPt(minPt);
                        if (maxIdx != firstIdx && maxIdx != lastIdx)
                            appendEnvPt(maxPt);
                    } else {
                        if (maxIdx != firstIdx && maxIdx != lastIdx)
                            appendEnvPt(maxPt);
                        if (minIdx != firstIdx && minIdx != lastIdx)
                            appendEnvPt(minPt);
                    }
                    if (lastIdx != firstIdx)
                        appendEnvPt(lastPt);
                    hasBucket = false;
                };

                for (int i = from; i <= to; ++i) {
                    const double px = xMap.xTransform(x(i));
                    const double py = yMap.xTransform(y(i));
                    if (!std::isfinite(px) || !std::isfinite(py)) {
                        flushBucket();
                        flushRun(envRun);
                        continue;
                    }
                    const QPointF pt = clampExtremePoint(QPointF(px, py));
                    const qint64 col = static_cast<qint64>(std::floor(pt.x() * colScale));
                    if (!hasBucket || col != curCol) {
                        flushBucket();
                        hasBucket = true;
                        curCol = col;
                        firstIdx = minIdx = maxIdx = lastIdx = i;
                        firstPt = minPt = maxPt = lastPt = pt;
                    } else {
                        if (pt.y() < minPt.y()) {
                            minIdx = i;
                            minPt = pt;
                        }
                        if (pt.y() > maxPt.y()) {
                            maxIdx = i;
                            maxPt = pt;
                        }
                        lastIdx = i;
                        lastPt = pt;
                    }
                }
                flushBucket();
                flushRun(envRun);
                break;
            }
        }

        QPolygonF run;
        run.reserve(qMin(4096, to - from + 1));
        if (!brk.any()) {
            for (int i = from; i <= to; ++i) {
                const double px = xMap.xTransform(x(i));
                const double py = yMap.xTransform(y(i));
                if (!std::isfinite(px) || !std::isfinite(py)) {
                    flushRun(run);
                    continue;
                }
                const QPointF pt = clampExtremePoint(QPointF(px, py));
                if (decimateSubPixel && !run.isEmpty() && i < to) {
                    const double dx = pt.x() - run.last().x();
                    const double dy = pt.y() - run.last().y();
                    if (dx * dx + dy * dy < 4e-4)
                        continue;
                }
                run.append(pt);
            }
        } else {
            QVector<QLineF> segs;
            segs.reserve(4);
            bool hasPrev = false;
            QPointF prevPt;
            for (int i = from; i <= to; ++i) {
                const double vx = x(i);
                const double vy = y(i);
                if (brk.valueInGap(vx, vy)) {
                    flushRun(run);
                    hasPrev = false;
                    continue;
                }
                const double px = xMap.xTransform(vx);
                const double py = yMap.xTransform(vy);
                if (!std::isfinite(px) || !std::isfinite(py)) {
                    flushRun(run);
                    hasPrev = false;
                    continue;
                }
                const QPointF pt = clampExtremePoint(QPointF(px, py));
                if (!hasPrev) {
                    if (!brk.pixelInGap(pt))
                        run.append(pt);
                    prevPt = pt;
                    hasPrev = true;
                    continue;
                }
                if (decimateSubPixel && !run.isEmpty() && !brk.pixelInGap(prevPt)
                    && !brk.pixelInGap(pt) && i < to) {
                    const double dx = pt.x() - run.last().x();
                    const double dy = pt.y() - run.last().y();
                    if (dx * dx + dy * dy < 4e-4)
                        continue;
                }
                segs.clear();
                appendSegmentOutsideBreaks(QLineF(prevPt, pt), brk, segs);
                for (const QLineF &s : segs) {
                    const QPointF p1 = clampExtremePoint(s.p1());
                    const QPointF p2 = clampExtremePoint(s.p2());
                    if (!run.isEmpty() && std::fabs(run.last().x() - p1.x()) < 1e-9
                        && std::fabs(run.last().y() - p1.y()) < 1e-9) {
                        if (std::fabs(p2.x() - run.last().x()) >= 1e-9
                            || std::fabs(p2.y() - run.last().y()) >= 1e-9)
                            run.append(p2);
                    } else {
                        if (!run.isEmpty())
                            flushRun(run);
                        run.append(p1);
                        if (std::fabs(p2.x() - p1.x()) >= 1e-9
                            || std::fabs(p2.y() - p1.y()) >= 1e-9)
                            run.append(p2);
                    }
                }
                prevPt = pt;
                hasPrev = true;
            }
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
            const double vx = x(i);
            const double vy = y(i);
            if (brk.valueInGap(vx, vy))
                continue;
            const double px = xMap.xTransform(vx);
            const double py = yMap.xTransform(vy);
            if (!std::isfinite(px) || !std::isfinite(py))
                continue;
            const QPointF pt = clampExtremePoint(QPointF(px, py));
            const QLineF seg = (curveType() == Xfy) ? QLineF(QPointF(x0, pt.y()), pt)
                                                    : QLineF(QPointF(pt.x(), y0), pt);
            if (brk.any())
                appendSegmentOutsideBreaks(seg, brk, sticks);
            else
                sticks.append(seg);
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
                    fillCurvePolygonF(painter, this, xMap, yMap, poly, &brk);
            } else if (poly.size() == 1) {
                painter->drawPoint(poly[0]);
            }
            poly.clear();
        };

        QPolygonF poly;
        poly.reserve(2 * (to - from) + 1);
        if (!brk.any()) {
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
        } else {
            QVector<QLineF> segs;
            segs.reserve(4);
            auto pushSeg = [&](const QPointF &a, const QPointF &b) {
                segs.clear();
                appendSegmentOutsideBreaks(QLineF(a, b), brk, segs);
                for (const QLineF &s : segs) {
                    const QPointF p1 = clampExtremePoint(s.p1());
                    const QPointF p2 = clampExtremePoint(s.p2());
                    if (!poly.isEmpty() && std::fabs(poly.last().x() - p1.x()) < 1e-9
                        && std::fabs(poly.last().y() - p1.y()) < 1e-9) {
                        if (std::fabs(p2.x() - poly.last().x()) >= 1e-9
                            || std::fabs(p2.y() - poly.last().y()) >= 1e-9)
                            poly.append(p2);
                    } else {
                        if (!poly.isEmpty())
                            flushStepRun(poly);
                        poly.append(p1);
                        if (std::fabs(p2.x() - p1.x()) >= 1e-9
                            || std::fabs(p2.y() - p1.y()) >= 1e-9)
                            poly.append(p2);
                    }
                }
            };

            bool hasPrev = false;
            QPointF prevPt;
            for (int i = from; i <= to; ++i) {
                const double vx = x(i);
                const double vy = y(i);
                if (brk.valueInGap(vx, vy)) {
                    flushStepRun(poly);
                    hasPrev = false;
                    continue;
                }
                const double px = xMap.xTransform(vx);
                const double py = yMap.xTransform(vy);
                if (!std::isfinite(px) || !std::isfinite(py)) {
                    flushStepRun(poly);
                    hasPrev = false;
                    continue;
                }
                const QPointF pt = clampExtremePoint(QPointF(px, py));
                if (!hasPrev) {
                    if (!brk.pixelInGap(pt))
                        poly.append(pt);
                } else {
                    const QPointF corner =
                            inverted ? QPointF(prevPt.x(), pt.y()) : QPointF(pt.x(), prevPt.y());
                    pushSeg(prevPt, corner);
                    pushSeg(corner, pt);
                }
                prevPt = pt;
                hasPrev = true;
            }
        }
        flushStepRun(poly);
        break;
    }
    case Dots: {
        const bool doFill = (brush().style() != Qt::NoBrush);
        auto flushDotsRun = [&](QPolygonF &pts) {
            if (!pts.isEmpty()) {
                painter->drawPoints(pts);
                if (doFill && pts.size() >= 2)
                    fillCurvePolygonF(painter, this, xMap, yMap, pts, &brk);
                pts.clear();
            }
        };

        QPolygonF pts;
        pts.reserve(to - from + 1);
        bool hasPrev = false;
        double prevVx = 0.0, prevVy = 0.0;
        QPointF prevPt;
        for (int i = from; i <= to; ++i) {
            const double vx = x(i);
            const double vy = y(i);
            if (brk.valueInGap(vx, vy)) {
                flushDotsRun(pts);
                hasPrev = false;
                continue;
            }
            const double px = xMap.xTransform(vx);
            const double py = yMap.xTransform(vy);
            if (!std::isfinite(px) || !std::isfinite(py)) {
                flushDotsRun(pts);
                hasPrev = false;
                continue;
            }
            const QPointF pt = clampExtremePoint(QPointF(px, py));
            if (brk.pixelInGap(pt)) {
                flushDotsRun(pts);
                hasPrev = false;
                continue;
            }
            if (hasPrev && brk.any()) {
                const bool straddlesX = brk.xValuesStraddle(prevVx, vx)
                        || (brk.hasX && ((prevPt.x() <= brk.xLo && pt.x() >= brk.xHi)
                                         || (prevPt.x() >= brk.xHi && pt.x() <= brk.xLo)));
                const bool straddlesY = brk.yValuesStraddle(prevVy, vy)
                        || (brk.hasY && ((prevPt.y() <= brk.yLo && pt.y() >= brk.yHi)
                                         || (prevPt.y() >= brk.yHi && pt.y() <= brk.yLo)));
                if (straddlesX || straddlesY)
                    flushDotsRun(pts);
            }
            pts.append(pt);
            prevVx = vx;
            prevVy = vy;
            prevPt = pt;
            hasPrev = true;
        }
        flushDotsRun(pts);
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

    const double dispScale = screenDisplayScaleForPlot(plot());
    painter->setBrush(sym.brush());
    QPen symPen = scaleFractionalPenForLayout(sym.pen(), dispScale);
    painter->setPen(symPen);

    const QSize layoutSz = QwtPainter::metricsMap().screenToLayout(sym.size());
    const double w = (dispScale != 1.0) ? (double(sym.size().width()) * dispScale)
                                        : double(layoutSz.width());
    const double h = (dispScale != 1.0) ? (double(sym.size().height()) * dispScale)
                                        : double(layoutSz.height());
    if (w <= 0.0 || h <= 0.0)
        return;

    const AxisBreakBands brk = axisBreakBands(xMap, yMap);

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

    const bool isLineSymbolsAuto = (d_symbol_skip == 0)
            && (d_type == Graph::LineSymbols || style() != QwtPlotCurve::NoCurve)
            && (d_type != Graph::Scatter);
    const bool maySkip = (d_symbol_skip >= 2) || (isLineSymbolsAuto && n > 80);

    if (maySkip) {
        struct ValidSymPt {
            double cx;
            double cy;
            bool inBox;
        };
        QVector<ValidSymPt> validPts;
        validPts.reserve(to - from + 1);
        int visCount = 0;
        for (int i = from; i <= to; ++i) {
            const double vx = x(i);
            const double vy = y(i);
            if (brk.valueInGap(vx, vy))
                continue;
            const double cx = xMap.xTransform(vx);
            const double cy = yMap.xTransform(vy);
            if (!std::isfinite(cx) || !std::isfinite(cy))
                continue;
            if (std::fabs(cx) > kMaxSafeCoord || std::fabs(cy) > kMaxSafeCoord)
                continue;
            const bool inBox = !hasCanvasBounds || validCenterBox.contains(QPointF(cx, cy));
            if (inBox)
                ++visCount;
            validPts.append({ cx, cy, inBox });
        }

        const int totalValid = validPts.size();
        if (d_symbol_skip >= 2) {
            const int step = d_symbol_skip;
            for (int k = 0; k < totalValid; ++k) {
                if (k == 0 || k == totalValid - 1 || (k % step) == 0) {
                    if (validPts[k].inBox)
                        drawSymbolShapeF(painter, sym.style(), validPts[k].cx, validPts[k].cy, w, h);
                }
            }
        } else {
            int effSkip = 1;
            if (hasCanvasBounds) {
                const double baseCanvasW = (dispScale != 1.0)
                        ? (d_canvas_rect.width() / dispScale)
                        : d_canvas_rect.width();
                const int targetMarkers =
                        (baseCanvasW < 150.0) ? qBound(6, int(baseCanvasW / 14.0), 12) : 25;
                if (visCount > 80 || (baseCanvasW < 150.0 && visCount > targetMarkers * 2))
                    effSkip = qMax(1, visCount / targetMarkers);
            } else if (totalValid > 80) {
                effSkip = qMax(1, totalValid / 25);
            }
            int visOrd = 0;
            for (int k = 0; k < totalValid; ++k) {
                if (!validPts[k].inBox)
                    continue;
                if (effSkip <= 1 || visOrd == 0 || visOrd == visCount - 1
                    || (visOrd % effSkip) == 0) {
                    drawSymbolShapeF(painter, sym.style(), validPts[k].cx, validPts[k].cy, w, h);
                }
                ++visOrd;
            }
        }

        if (expandClip)
            painter->restore();
        return;
    }

    const bool decimateSubPixel = testPaintAttribute(PaintFiltered) && ((to - from) > 4000);
    QPointF lastDrawn(1e30, 1e30);
    std::vector<uint8_t> occGrid;
    int occGridW = 0, occGridH = 0;
    double occOriginX = 0.0, occOriginY = 0.0, occScale = 2.0;
    if (decimateSubPixel && hasCanvasBounds && validCenterBox.width() > 1.0
        && validCenterBox.height() > 1.0) {
        const QTransform &xf = painter->transform();
        occScale = qBound(1.0, 2.0 * std::max(1.0, std::hypot(xf.m11(), xf.m12())), 8.0);
        const qint64 gw = static_cast<qint64>(std::ceil(validCenterBox.width() * occScale)) + 2;
        const qint64 gh = static_cast<qint64>(std::ceil(validCenterBox.height() * occScale)) + 2;
        if (gw > 0 && gh > 0 && gw <= 4096 && gh <= 4096) {
            occGridW = static_cast<int>(gw);
            occGridH = static_cast<int>(gh);
            occOriginX = validCenterBox.left();
            occOriginY = validCenterBox.top();
            occGrid.assign(static_cast<size_t>(occGridW) * static_cast<size_t>(occGridH), 0);
        }
    }

    for (int i = from; i <= to; ++i) {
        const double vx = x(i);
        const double vy = y(i);
        if (brk.valueInGap(vx, vy))
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
            if (!occGrid.empty()) {
                const int gx = qBound(0, int((cx - occOriginX) * occScale), occGridW - 1);
                const int gy = qBound(0, int((cy - occOriginY) * occScale), occGridH - 1);
                const size_t cellIdx =
                        static_cast<size_t>(gy) * static_cast<size_t>(occGridW) + size_t(gx);
                if (occGrid[cellIdx])
                    continue;
                occGrid[cellIdx] = 1;
            } else {
                const double dx = cx - lastDrawn.x();
                const double dy = cy - lastDrawn.y();
                if (dx * dx + dy * dy < 0.04)
                    continue;
            }
        }
        drawSymbolShapeF(painter, sym.style(), cx, cy, w, h);
        lastDrawn = QPointF(cx, cy);
    }

    if (expandClip)
        painter->restore();
}

// ============================================================================
// E11: colour-mapped (and optionally size-mapped) scatter symbols
// ============================================================================

namespace {

Column *resolvePointMapColumn(Table *t, const QString &name)
{
    if (!t || name.isEmpty())
        return nullptr;
    if (Column *c = t->column(name))
        return c;
    const QString prefix = t->name() + QStringLiteral("_");
    if (name.startsWith(prefix))
        return t->column(name.mid(prefix.size()));
    return nullptr;
}

double pointMapCellValue(Column *c, int row)
{
    if (!c || row < 0 || row >= c->rowCount() || c->isInvalid(row))
        return std::numeric_limits<double>::quiet_NaN();
    if (c->columnMode() != SciDAVis::ColumnMode::Numeric)
        return std::numeric_limits<double>::quiet_NaN();
    return c->valueAt(row);
}

} // namespace

double DataCurve::pointColorValue(int i) const
{
    Column *zc = resolvePointMapColumn(d_table, d_point_cmap.zColumn);
    return pointMapCellValue(zc, tableRow(i));
}

bool DataCurve::pointColorRange(double *zMin, double *zMax) const
{
    double lo = d_point_cmap.zMin;
    double hi = d_point_cmap.zMax;
    if (d_point_cmap.autoRange) {
        Column *zc = resolvePointMapColumn(d_table, d_point_cmap.zColumn);
        bool first = true;
        for (int i = 0; i < dataSize(); ++i) {
            const double v = pointMapCellValue(zc, tableRow(i));
            if (!std::isfinite(v))
                continue;
            if (first) {
                lo = hi = v;
                first = false;
            } else {
                lo = qMin(lo, v);
                hi = qMax(hi, v);
            }
        }
        if (first) {
            lo = 0.0;
            hi = 1.0;
        }
    }
    if (lo > hi)
        std::swap(lo, hi);
    if (!(hi > lo) || !std::isfinite(hi - lo))
        hi = lo + 1.0;
    if (!(hi > lo))
        hi = lo + std::max(1.0, std::fabs(lo) * 1e-6);
    if (zMin)
        *zMin = lo;
    if (zMax)
        *zMax = hi;
    return true;
}

QColor DataCurve::pointColor(int i) const
{
    const double v = pointColorValue(i);
    if (!std::isfinite(v))
        return QColor();
    double lo = 0.0, hi = 1.0;
    pointColorRange(&lo, &hi);
    const QwtLinearColorMap map = Spectrogram::presetColorMap(
            static_cast<Spectrogram::ColorMapPreset>(d_point_cmap.preset), d_point_cmap.reverse);
    return QColor(map.rgb(QwtDoubleInterval(lo, hi), qBound(lo, v, hi)));
}

void DataCurve::drawSymbols(QPainter *painter, const QwtSymbol &sym, const QwtScaleMap &xMap,
                            const QwtScaleMap &yMap, int from, int to) const
{
    if (!hasPointColorMap()) {
        PlotCurve::drawSymbols(painter, sym, xMap, yMap, from, to);
        return;
    }
    if (!painter || dataSize() <= 0 || sym.style() == QwtSymbol::NoSymbol)
        return;
    const int n = dataSize();
    from = qMax(0, from);
    to = (to < 0) ? (n - 1) : qMin(to, n - 1);
    if (from > to)
        return;

    double zLo = 0.0, zHi = 1.0;
    pointColorRange(&zLo, &zHi);
    const QwtDoubleInterval zInt(zLo, zHi);
    const QwtLinearColorMap map = Spectrogram::presetColorMap(
            static_cast<Spectrogram::ColorMapPreset>(d_point_cmap.preset), d_point_cmap.reverse);
    Column *zc = resolvePointMapColumn(d_table, d_point_cmap.zColumn);
    Column *sc = resolvePointMapColumn(d_table, d_point_cmap.sizeColumn);

    double sLo = 0.0, sHi = 0.0;
    bool haveSize = false;
    if (sc) {
        for (int i = 0; i < n; ++i) {
            const double v = pointMapCellValue(sc, tableRow(i));
            if (!std::isfinite(v))
                continue;
            if (!haveSize) {
                sLo = sHi = v;
                haveSize = true;
            } else {
                sLo = qMin(sLo, v);
                sHi = qMax(sHi, v);
            }
        }
    }

    const double dispScale = screenDisplayScaleForPlot(plot());
    painter->save();
    if (testRenderHint(QwtPlotItem::RenderAntialiased))
        painter->setRenderHint(QPainter::Antialiasing, true);
    QPen basePen = scaleFractionalPenForLayout(sym.pen(), dispScale);
    const bool whiteHollow = Graph::isSymbolWhiteHollow(sym);
    const bool noFill = (sym.brush().style() == Qt::NoBrush);
    const QSize layoutSz = QwtPainter::metricsMap().screenToLayout(sym.size());
    const double baseW = (dispScale != 1.0)
            ? qMax(1.0, double(sym.size().width()) * dispScale)
            : qMax(1.0, double(layoutSz.width()));
    const double baseH = (dispScale != 1.0)
            ? qMax(1.0, double(sym.size().height()) * dispScale)
            : qMax(1.0, double(layoutSz.height()));
    const double pxToLayout =
            double(sym.size().width()) > 0.0 ? baseW / double(sym.size().width()) : 1.0;

    const AxisBreakBands brk = axisBreakBands(xMap, yMap);

    for (int i = from; i <= to; ++i) {
        const double vx = x(i);
        const double vy = y(i);
        if (brk.valueInGap(vx, vy))
            continue;
        const double cx = xMap.xTransform(vx);
        const double cy = yMap.xTransform(vy);
        if (!std::isfinite(cx) || !std::isfinite(cy) || std::fabs(cx) > kMaxSafeCoord
            || std::fabs(cy) > kMaxSafeCoord)
            continue;
        const int row = tableRow(i);
        const double z = pointMapCellValue(zc, row);
        QColor fill;
        if (std::isfinite(z))
            fill = QColor(map.rgb(zInt, qBound(zLo, z, zHi)));
        else
            fill = QColor(200, 200, 200); // missing colour value: light grey
        double w = baseW, h = baseH;
        if (haveSize) {
            const double sv = pointMapCellValue(sc, row);
            if (std::isfinite(sv)) {
                const double t = (sHi > sLo) ? (sv - sLo) / (sHi - sLo) : 0.5;
                const double px = d_point_cmap.sizeMinPx
                        + t * (d_point_cmap.sizeMaxPx - d_point_cmap.sizeMinPx);
                w = h = qMax(1.0, px * pxToLayout);
            }
        }
        QPen pen = basePen;
        if (whiteHollow) {
            pen.setColor(fill);
            painter->setPen(pen);
            painter->setBrush(QBrush(Qt::white));
        } else if (noFill) {
            pen.setColor(fill);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
        } else {
            pen.setColor(fill.darker(150));
            painter->setPen(pen);
            painter->setBrush(QBrush(fill));
        }
        drawSymbolShapeF(painter, sym.style(), cx, cy, w, h);
    }
    painter->restore();
}
