/***************************************************************************
    File                 : RangeSelectorTool.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006,2007 by Ion Vasilief,
                           Tilman Benkert, Knut Franke
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net,
                           knut.franke*gmx.de
    Description          : Plot tool for selecting ranges on curves.

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
#include "RangeSelectorTool.h"
#include "ApplicationWindow.h"
#include "Graph.h"
#include "Plot.h"
#include "PlotCurve.h"

#include <qwt_symbol.h>
#include <qwt_scale_map.h>
#include <QPoint>
#include <QMessageBox>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLocale>
#include <cmath>

RangeSelectorTool::RangeSelectorTool(Graph *graph, const QObject *status_target,
                                     const char *status_slot)
    : QwtPlotPicker(graph->plotWidget()->canvas()),
      PlotToolInterface(graph),
      d_active_point(0),
      d_inactive_point(0),
      d_selected_curve(NULL)
{
    const Graph::GraphSelection curSel = d_graph->currentSelection();
    if (curSel.kind == Graph::GraphSelection::Curve && curSel.primaryId >= 0) {
        QwtPlotCurve *selC = d_graph->curve(curSel.primaryId);
        if (selC && selC->rtti() == QwtPlotItem::Rtti_PlotCurve && selC->dataSize() > 0)
            d_selected_curve = selC;
    }
    if (!d_selected_curve) {
        for (int i = d_graph->curves() - 1; i >= 0; --i) {
            d_selected_curve = d_graph->curve(i);
            if (d_selected_curve && d_selected_curve->rtti() == QwtPlotItem::Rtti_PlotCurve
                && d_selected_curve->dataSize() > 0)
                break;
            d_selected_curve = NULL;
        }
    }
    if (!d_selected_curve) {
        if (!ApplicationWindow::isNonInteractiveContext(d_graph)) {
            QMessageBox::critical(d_graph, tr("Warning"),
                                  tr("All the curves on this plot are empty!"));
        }
        return;
    }

    d_active_point = 0;
    d_inactive_point = d_selected_curve->dataSize() - 1;
    int marker_size = 20;
    d_active_marker.setTitle(QStringLiteral("__tg_transient_ui_marker__"));
    d_inactive_marker.setTitle(QStringLiteral("__tg_transient_ui_marker__"));
    d_active_marker.setAxis(d_selected_curve->xAxis(), d_selected_curve->yAxis());
    d_inactive_marker.setAxis(d_selected_curve->xAxis(), d_selected_curve->yAxis());
    d_active_marker.setSymbol(QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(255, 255, 0, 128)),
                                        QPen(Qt::red, 2), QSize(marker_size, marker_size)));
    d_active_marker.setLineStyle(QwtPlotMarker::VLine);
    d_active_marker.setLinePen(QPen(Qt::red, 1, Qt::DashLine));
    d_inactive_marker.setSymbol(QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(255, 255, 0, 128)),
                                          QPen(Qt::black, 2), QSize(marker_size, marker_size)));
    d_inactive_marker.setLineStyle(QwtPlotMarker::VLine);
    d_inactive_marker.setLinePen(QPen(Qt::black, 1, Qt::DashLine));
    d_active_marker.setValue(d_selected_curve->x(d_active_point),
                             d_selected_curve->y(d_active_point));
    d_inactive_marker.setValue(d_selected_curve->x(d_inactive_point),
                               d_selected_curve->y(d_inactive_point));
    d_active_marker.attach(d_graph->plotWidget());
    d_inactive_marker.attach(d_graph->plotWidget());

    setTrackerMode(QwtPicker::AlwaysOn);
    setSelectionFlags(QwtPicker::PointSelection | QwtPicker::ClickSelection);
    d_graph->plotWidget()->canvas()->setCursor(QCursor(QPixmap(":/vizor.xpm"), -1, -1));
    d_graph->plotWidget()->canvas()->setFocus();
    d_graph->plotWidget()->replot();

    if (status_target)
        connect(this, SIGNAL(statusText(const QString &)), status_target, status_slot);
    emit statusText(
            tr("Click or use Ctrl+arrow key to select range (arrows select active cursor)!"));
}

RangeSelectorTool::~RangeSelectorTool()
{
    d_active_marker.detach();
    d_inactive_marker.detach();
    d_graph->plotWidget()->canvas()->unsetCursor();
    d_graph->plotWidget()->replot();
}

namespace {
// Equality of two abscissae with a relative and an absolute tolerance. qFuzzyCompare alone is
// purely relative and fails whenever one side is (close to) zero.
bool sameAbscissa(double a, double b, double scale)
{
    if (a == b)
        return true;
    if (!std::isfinite(a) || !std::isfinite(b))
        return false;
    const double diff = std::fabs(a - b);
    const double rel = 1e-12 * qMax(std::fabs(a), std::fabs(b));
    const double absTol = 1e-12 * scale;
    return diff <= qMax(rel, absTol);
}
} // namespace

void RangeSelectorTool::pointSelected(const QPoint &pos)
{
    int dist, point;
    const int curve_key = d_graph->plotWidget()->closestCurve(pos.x(), pos.y(), dist, point);
    if (curve_key < 0 || dist >= 10) // 10 pixels tolerance
        return;
    QwtPlotCurve *curve = (QwtPlotCurve *)d_graph->plotWidget()->curve(curve_key);
    selectCurvePoint(curve, point);
}

void RangeSelectorTool::selectCurvePoint(QwtPlotCurve *curve, int point)
{
    if (!curve || curve->dataSize() <= 0 || point < 0 || point >= curve->dataSize())
        return;

    if (curve == d_selected_curve)
        setActivePoint(point);
    else {
        // try to be intelligent about selecting the inactive point
        double min_x = curve->data().boundingRect().left();
        double max_x = curve->data().boundingRect().right();
        const double scale = qMax(std::fabs(min_x), std::fabs(max_x)) > 0.0
                ? qMax(std::fabs(min_x), std::fabs(max_x))
                : 1.0;
        int n = curve->dataSize();
        d_inactive_point = (point == 0 && n > 1) ? (n - 1) : 0;
        double second_x;
        if (curve->x(point) == min_x)
            second_x = max_x;
        else if (curve->x(point) == max_x)
            second_x = min_x;
        else if (d_active_marker.xValue() < d_inactive_marker.xValue())
            second_x = max_x;
        else
            second_x = min_x;
        if (second_x == max_x) { // start at selected point and try larger indices first
            for (int i = 0; i < n; ++i)
                if (sameAbscissa(curve->x((i + point) % n), max_x, scale))
                    d_inactive_point = (i + point) % n;
        } else { // start at selected point and try smaller indices first
            for (int i = n - 1; i >= 0; --i)
                if (sameAbscissa(curve->x((i + point) % n), min_x, scale))
                    d_inactive_point = (i + point) % n;
        }
        d_selected_curve = curve;
        d_active_marker.setAxis(d_selected_curve->xAxis(), d_selected_curve->yAxis());
        d_inactive_marker.setAxis(d_selected_curve->xAxis(), d_selected_curve->yAxis());
        d_inactive_marker.setValue(curve->x(d_inactive_point), curve->y(d_inactive_point));
        d_active_point = point;
        d_active_marker.setValue(d_selected_curve->x(d_active_point),
                                 d_selected_curve->y(d_active_point));
        emitStatusText();
        emit changed();
    }
    d_graph->plotWidget()->replot();
}

void RangeSelectorTool::setSelectedCurve(QwtPlotCurve *curve)
{
    if (!curve || curve->dataSize() <= 0) {
        d_selected_curve = NULL;
        d_active_marker.detach();
        d_inactive_marker.detach();
        return;
    }
    if (d_selected_curve == curve)
        return;
    d_selected_curve = curve;
    d_active_point = 0;
    d_inactive_point = d_selected_curve->dataSize() - 1;
    d_active_marker.setTitle(QStringLiteral("__tg_transient_ui_marker__"));
    d_inactive_marker.setTitle(QStringLiteral("__tg_transient_ui_marker__"));
    d_active_marker.setAxis(d_selected_curve->xAxis(), d_selected_curve->yAxis());
    d_inactive_marker.setAxis(d_selected_curve->xAxis(), d_selected_curve->yAxis());
    d_active_marker.setValue(d_selected_curve->x(d_active_point),
                             d_selected_curve->y(d_active_point));
    d_inactive_marker.setValue(d_selected_curve->x(d_inactive_point),
                               d_selected_curve->y(d_inactive_point));
    if (!d_active_marker.plot() && d_graph && d_graph->plotWidget())
        d_active_marker.attach(d_graph->plotWidget());
    if (!d_inactive_marker.plot() && d_graph && d_graph->plotWidget())
        d_inactive_marker.attach(d_graph->plotWidget());
    emitStatusText();
    emit changed();
}

void RangeSelectorTool::setActivePoint(int point)
{
    if (!d_selected_curve || point < 0 || point >= d_selected_curve->dataSize()
        || point == d_active_point)
        return;
    d_active_point = point;
    d_active_marker.setValue(d_selected_curve->x(d_active_point),
                             d_selected_curve->y(d_active_point));
    emitStatusText();
    emit changed();
}

void RangeSelectorTool::emitStatusText()
{
    if (!d_selected_curve || d_active_point < 0 || d_active_point >= d_selected_curve->dataSize())
        return;
    const PlotCurve *pc = dynamic_cast<const PlotCurve *>(d_selected_curve);
    DataCurve *dc = dynamic_cast<DataCurve *>(d_selected_curve);
    if ((pc && pc->type() == Graph::Function) || !dc || !dc->table()) {
        emit statusText(
                QString("%1 <=> %2[%3]: x=%4; y=%5")
                        .arg(d_active_marker.xValue() > d_inactive_marker.xValue() ? tr("Right")
                                                                                   : tr("Left"))
                        .arg(d_selected_curve->title().text())
                        .arg(d_active_point + 1)
                        .arg(QLocale().toString(d_selected_curve->x(d_active_point), 'G', 15))
                        .arg(QLocale().toString(d_selected_curve->y(d_active_point), 'G', 15)));
    } else {
        Table *t = dc->table();
        int row = dc->tableRow(d_active_point);
        int xCol = t->colIndex(dc->xColumnName());
        int yCol = t->colIndex(d_selected_curve->title().text());
        const QString xStr = (row >= 0 && xCol >= 0)
                ? t->text(row, xCol)
                : QLocale().toString(d_selected_curve->x(d_active_point), 'G', 15);
        const QString yStr = (row >= 0 && yCol >= 0)
                ? t->text(row, yCol)
                : QLocale().toString(d_selected_curve->y(d_active_point), 'G', 15);

        emit statusText(
                QString("%1 <=> %2[%3]: x=%4; y=%5")
                        .arg(d_active_marker.xValue() > d_inactive_marker.xValue() ? tr("Right")
                                                                                   : tr("Left"))
                        .arg(d_selected_curve->title().text())
                        .arg(row + 1)
                        .arg(xStr)
                        .arg(yStr));
    }
}

void RangeSelectorTool::switchActiveMarker()
{
    if (!d_selected_curve)
        return;
    QwtDoublePoint tmp = d_active_marker.value();
    d_active_marker.setValue(d_inactive_marker.value());
    d_inactive_marker.setValue(tmp);
    int tmp2 = d_active_point;
    d_active_point = d_inactive_point;
    d_inactive_point = tmp2;
    d_graph->plotWidget()->replot();

    emitStatusText();
}

int RangeSelectorTool::closestPointForCanvasX(int canvasX) const
{
    if (!d_selected_curve || d_selected_curve->dataSize() <= 0 || !d_graph
        || !d_graph->plotWidget()) {
        return -1;
    }
    const QwtScaleMap xMap = d_graph->plotWidget()->canvasMap(d_selected_curve->xAxis());
    const int n = d_selected_curve->dataSize();
    int bestIdx = -1;
    double bestDx = 1e18;
    for (int i = 0; i < n; ++i) {
        const double xv = d_selected_curve->x(i);
        const double yv = d_selected_curve->y(i);
        if (!std::isfinite(xv) || !std::isfinite(yv))
            continue;
        const double px = xMap.xTransform(xv);
        if (!std::isfinite(px))
            continue;
        const double dx = std::abs(px - double(canvasX));
        if (dx < bestDx) {
            bestDx = dx;
            bestIdx = i;
        }
    }
    return bestIdx;
}

bool RangeSelectorTool::eventFilter(QObject *obj, QEvent *event)
{
    if (d_graph && d_graph->plotWidget() && obj == d_graph->plotWidget()->canvas()) {
        QWidget *cv = d_graph->plotWidget()->canvas();
        if (event->type() == QEvent::MouseButtonPress) {
            const QMouseEvent *me = static_cast<const QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && d_selected_curve
                && d_selected_curve->dataSize() > 0) {
                const int actPx =
                        d_graph->plotWidget()->transform(d_active_marker.xAxis(),
                                                         d_active_marker.xValue());
                const int inactPx =
                        d_graph->plotWidget()->transform(d_inactive_marker.xAxis(),
                                                         d_inactive_marker.xValue());
                const int dAct = std::abs(me->pos().x() - actPx);
                const int dInact = std::abs(me->pos().x() - inactPx);
                if (qMin(dAct, dInact) <= 8) {
                    if (dInact < dAct)
                        switchActiveMarker();
                    d_dragging_marker = true;
                    cv->setCursor(Qt::SizeHorCursor);
                    const int idx = closestPointForCanvasX(me->pos().x());
                    if (idx >= 0) {
                        setActivePoint(idx);
                        d_graph->plotWidget()->replot();
                    }
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            const QMouseEvent *me = static_cast<const QMouseEvent *>(event);
            if (d_dragging_marker) {
                const int idx = closestPointForCanvasX(me->pos().x());
                if (idx >= 0 && idx != d_active_point) {
                    setActivePoint(idx);
                    d_graph->plotWidget()->replot();
                }
                return true;
            }
            if (d_selected_curve && d_selected_curve->dataSize() > 0) {
                const int actPx =
                        d_graph->plotWidget()->transform(d_active_marker.xAxis(),
                                                         d_active_marker.xValue());
                const int inactPx =
                        d_graph->plotWidget()->transform(d_inactive_marker.xAxis(),
                                                         d_inactive_marker.xValue());
                if (qMin(std::abs(me->pos().x() - actPx), std::abs(me->pos().x() - inactPx)) <= 8)
                    cv->setCursor(Qt::SizeHorCursor);
                else
                    cv->setCursor(QCursor(QPixmap(":/vizor.xpm"), -1, -1));
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const QMouseEvent *me = static_cast<const QMouseEvent *>(event);
            if (d_dragging_marker && me->button() == Qt::LeftButton) {
                const int idx = closestPointForCanvasX(me->pos().x());
                if (idx >= 0 && idx != d_active_point) {
                    setActivePoint(idx);
                    d_graph->plotWidget()->replot();
                }
                d_dragging_marker = false;
                cv->setCursor(QCursor(QPixmap(":/vizor.xpm"), -1, -1));
                return true;
            }
        }
    }

    switch (event->type()) {
    case QEvent::KeyPress:
        if (keyEventFilter((QKeyEvent *)event))
            return true;
        break;
    default:
        break;
    }
    return QwtPlotPicker::eventFilter(obj, event);
}

bool RangeSelectorTool::keyEventFilter(QKeyEvent *ke)
{
    switch (ke->key()) {
    case Qt::Key_Escape: {
        if (d_dragging_marker) {
            d_dragging_marker = false;
            if (d_graph && d_graph->plotWidget() && d_graph->plotWidget()->canvas()) {
                d_graph->plotWidget()->canvas()->setCursor(
                        QCursor(QPixmap(":/vizor.xpm"), -1, -1));
            }
            return true;
        }
        if (d_graph && d_graph->handleEscapeKey())
            return true;
        return false;
    }
    case Qt::Key_Up: {
        if (!d_graph || !d_selected_curve)
            return false;
        int n_curves = d_graph->curves();
        if (n_curves <= 0)
            return true;
        int curIdx = d_graph->curveIndex(d_selected_curve);
        int start = (curIdx >= 0 ? curIdx : 0) + 1;
        for (int i = start; i < start + n_curves; ++i) {
            QwtPlotCurve *cand = d_graph->curve(i % n_curves);
            if (cand && cand->dataSize() > 0) {
                setSelectedCurve(cand);
                break;
            }
        }
        d_graph->plotWidget()->replot();
        return true;
    }
    case Qt::Key_Down: {
        if (!d_graph || !d_selected_curve)
            return false;
        int n_curves = d_graph->curves();
        if (n_curves <= 0)
            return true;
        int curIdx = d_graph->curveIndex(d_selected_curve);
        int start = (curIdx >= 0 ? curIdx : 0) + n_curves - 1;
        for (int i = start; i > start - n_curves; --i) {
            QwtPlotCurve *cand = d_graph->curve(i % n_curves);
            if (cand && cand->dataSize() > 0) {
                setSelectedCurve(cand);
                break;
            }
        }
        d_graph->plotWidget()->replot();
        return true;
    }
    case Qt::Key_Right:
    case Qt::Key_Plus: {
        if (!d_selected_curve || d_selected_curve->dataSize() <= 0)
            return true;
        if (ke->modifiers() & Qt::ControlModifier) {
            int n_points = d_selected_curve->dataSize();
            setActivePoint((qMax(0, d_active_point) + 1) % n_points);
            d_graph->plotWidget()->replot();
        } else
            switchActiveMarker();
        return true;
    }
    case Qt::Key_Left:
    case Qt::Key_Minus: {
        if (!d_selected_curve || d_selected_curve->dataSize() <= 0)
            return true;
        if (ke->modifiers() & Qt::ControlModifier) {
            int n_points = d_selected_curve->dataSize();
            setActivePoint((qMax(0, d_active_point) - 1 + n_points) % n_points);
            d_graph->plotWidget()->replot();
        } else
            switchActiveMarker();
        return true;
    }
    default:
        break;
    }
    return false;
}
