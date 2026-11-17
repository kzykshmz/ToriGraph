/***************************************************************************
    File                 : ArrowMarker.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Arrow marker (extension to QwtPlotMarker)

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
#include "ArrowMarker.h"
#include "Graph.h"
#include "LineDialog.h"
#include "MultiLayer.h"

#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QPointer>
#include <QUndoStack>
#include <QVariant>
#include <cmath>

namespace {
QPoint snapArrowEndpointAngle45(const QPoint &anchor, const QPoint &target)
{
    const double dx = double(target.x() - anchor.x());
    const double dy = double(target.y() - anchor.y());
    const double len = std::hypot(dx, dy);
    if (len < 1.0)
        return target;
    constexpr double kPi = 3.141592653589793238462643;
    const double stepRad = kPi / 4.0; // 0 / 45 / 90 / 135 / 180 deg
    const double snappedAngle = std::round(std::atan2(dy, dx) / stepRad) * stepRad;
    return QPoint(anchor.x() + int(std::round(len * std::cos(snappedAngle))),
                  anchor.y() + int(std::round(len * std::sin(snappedAngle))));
}
} // namespace

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_painter.h>

#ifndef M_PI
#define M_PI 3.141592653589793238462643;
#endif

ArrowMarker::ArrowMarker()
    : d_end_arrow(true),
      d_fill_head(true),
      d_head_angle(45),
      d_head_length(4),
      d_rect(0, 0, 1, 1),
      d_editable(false),
      d_op(None)
{
}

void ArrowMarker::draw(QPainter *p, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                       const QRect &) const
{
    const double x0 = xMap.xTransform(d_rect.left());
    const double y0 = yMap.xTransform(d_rect.top());
    const double x1 = xMap.xTransform(d_rect.right());
    const double y1 = yMap.xTransform(d_rect.bottom());
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) || !std::isfinite(y1))
        return;

    constexpr double kPi = 3.141592653589793238462643;
    const double angleDeg = std::atan2(y1 - y0, x1 - x0) * (180.0 / kPi);
    const double headHalfW = double(d_head_length) * std::tan(kPi * double(d_head_angle) / 180.0);

    p->save();
    if (testRenderHint(QwtPlotItem::RenderAntialiased))
        p->setRenderHint(QPainter::Antialiasing, true);
    QPen pen = linePen();
    p->setPen(pen);

    QBrush brush = QBrush(pen.color(), Qt::SolidPattern);
    p->drawLine(QPointF(x0, y0), QPointF(x1, y1));
    p->restore();

    if (d_end_arrow) {
        p->save();
        if (testRenderHint(QwtPlotItem::RenderAntialiased))
            p->setRenderHint(QPainter::Antialiasing, true);
        p->translate(QPointF(x1, y1));
        p->rotate(angleDeg);

        QPolygonF endArray(3);
        endArray[0] = QPointF(0.0, 0.0);
        endArray[1] = QPointF(-double(d_head_length), headHalfW);
        endArray[2] = QPointF(-double(d_head_length), -headHalfW);

        p->setPen(QPen(pen.color(), pen.widthF(), Qt::SolidLine));
        if (d_fill_head)
            p->setBrush(brush);

        p->drawPolygon(endArray);
        p->restore();
    }

    if (d_start_arrow) {
        p->save();
        if (testRenderHint(QwtPlotItem::RenderAntialiased))
            p->setRenderHint(QPainter::Antialiasing, true);
        p->translate(QPointF(x0, y0));
        p->rotate(angleDeg);

        QPolygonF startArray(3);
        startArray[0] = QPointF(0.0, 0.0);
        startArray[1] = QPointF(double(d_head_length), headHalfW);
        startArray[2] = QPointF(double(d_head_length), -headHalfW);

        p->setPen(QPen(pen.color(), pen.widthF(), Qt::SolidLine));
        if (d_fill_head)
            p->setBrush(brush);
        p->drawPolygon(startArray);
        p->restore();
    }

    const Graph *g = plot() ? qobject_cast<const Graph *>(plot()->parent()) : nullptr;
    if (d_editable && (!g || (!g->isExporting() && !g->property("isExporting").toBool()))
        && p && p->device() && p->device()->devType() == QInternal::Widget) {
        p->save();
        p->setPen(QPen(Qt::black, 1, Qt::SolidLine));
        QRect handler(QPoint(0, 0), QSize(10, 10));
        handler.moveCenter(startPoint());
        p->fillRect(handler, QBrush(Qt::black));
        handler.moveCenter(endPoint());
        p->fillRect(handler, QBrush(Qt::black));
        p->restore();
    }
}

double ArrowMarker::theta(int xs, int ys, int xe, int ye) const
{
    double t;
    if (xe == xs) {
        if (ys > ye)
            t = 90;
        else
            t = 270;
    } else {
        t = atan2((ye - ys) * 1.0, (xe - xs) * 1.0) * 45 / atan(-1.0);
        if (t < 0)
            t = 360 + t;
    }
    return t;
}

double ArrowMarker::length()
{
    if (!plot())
        return -1.0;

    const QwtScaleMap &xMap = plot()->canvasMap(xAxis());
    const QwtScaleMap &yMap = plot()->canvasMap(yAxis());

    const int x0 = xMap.transform(d_rect.left());
    const int y0 = yMap.transform(d_rect.top());
    const int x1 = xMap.transform(d_rect.right());
    const int y1 = yMap.transform(d_rect.bottom());

    double l = sqrt(double((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)));
    return fabs(l);
}

double ArrowMarker::dist(int x, int y)
{
    if (!plot())
        return -1.0;

    const QwtScaleMap &xMap = plot()->canvasMap(xAxis());
    const QwtScaleMap &yMap = plot()->canvasMap(yAxis());

    const int x0 = xMap.transform(d_rect.left());
    const int y0 = yMap.transform(d_rect.top());
    const int x1 = xMap.transform(d_rect.right());
    const int y1 = yMap.transform(d_rect.bottom());

    int xmin = qMin(x0, x1);
    int xmax = qMax(x0, x1);
    int ymin = qMin(y0, y1);
    int ymax = qMax(y0, y1);

    if ((x > xmax || x < xmin || xmin == xmax) && (ymax < y || ymin > y || ymin == ymax))
        // return the shortest distance to one of the ends
        return qMin(sqrt(double((x - x0) * (x - x0) + (y - y0) * (y - y0))),
                    sqrt(double((x - x1) * (x - x1) + (y - y1) * (y - y1))));

    double d;
    if (x0 == x1)
        d = abs(x - x0);
    else {
        double a = (double)(y1 - y0) / (double)(x1 - x0);
        double b = y0 - a * x0;
        d = (a * x - y + b) / sqrt(a * a + 1);
    }
    return fabs(d);
}

void ArrowMarker::setColor(const QColor &c)
{
    if (linePen().color() == c)
        return;

    QPen pen = linePen();
    pen.setColor(c);
    setLinePen(pen);
}

void ArrowMarker::setWidth(int w)
{
    if (linePen().width() == w)
        return;

    QPen pen = linePen();
    pen.setWidth(w);
    setLinePen(pen);
}

void ArrowMarker::setStyle(Qt::PenStyle style)
{
    if (linePen().style() == style)
        return;

    QPen pen = linePen();
    pen.setStyle(style);
    setLinePen(pen);
}

void ArrowMarker::setCapStyle(Qt::PenCapStyle style)
{
    if (linePen().capStyle() == style)
        return;

    QPen pen = linePen();
    pen.setCapStyle(style);
    setLinePen(pen);
}

void ArrowMarker::setJoinStyle(Qt::PenJoinStyle style)
{
    if (linePen().joinStyle() == style)
        return;

    QPen pen = linePen();
    pen.setJoinStyle(style);
    setLinePen(pen);
}

void ArrowMarker::setHeadLength(int l)
{
    d_head_length = l;
}

void ArrowMarker::setHeadAngle(int a)
{
    d_head_angle = a;
}

void ArrowMarker::fillArrowHead(bool fill)
{
    d_fill_head = fill;
}

void ArrowMarker::setStartPoint(const QPoint &p)
{
    d_start = p;

    if (!plot())
        return;

    d_rect.setLeft(plot()->invTransform(xAxis(), p.x()));
    d_rect.setTop(plot()->invTransform(yAxis(), p.y()));
}

void ArrowMarker::setEndPoint(const QPoint &p)
{
    d_end = p;

    if (!plot())
        return;

    d_rect.setRight(plot()->invTransform(xAxis(), p.x()));
    d_rect.setBottom(plot()->invTransform(yAxis(), p.y()));
}

QPoint ArrowMarker::startPoint() const
{
    if (!plot())
        return QPoint();

    return QPoint(plot()->transform(xAxis(), d_rect.left()),
                  plot()->transform(yAxis(), d_rect.top()));
}

QwtDoublePoint ArrowMarker::startPointCoord()
{
    return QwtDoublePoint(d_rect.left(), d_rect.top());
}

void ArrowMarker::setStartPoint(double x, double y)
{
    if (d_rect.left() == x && d_rect.top() == y)
        return;

    d_rect.setLeft(x);
    d_rect.setTop(y);

    if (!plot())
        return;

    plot()->updateLayout();
    d_start = QPoint(plot()->transform(xAxis(), x), plot()->transform(yAxis(), y));
}

QPoint ArrowMarker::endPoint() const
{
    if (!plot())
        return QPoint();

    return QPoint(plot()->transform(xAxis(), d_rect.right()),
                  plot()->transform(yAxis(), d_rect.bottom()));
}

void ArrowMarker::setEndPoint(double x, double y)
{
    if (d_rect.right() == x && d_rect.bottom() == y)
        return;

    d_rect.setRight(x);
    d_rect.setBottom(y);

    if (!plot())
        return;

    plot()->updateLayout();
    d_end = QPoint(plot()->transform(xAxis(), x), plot()->transform(yAxis(), y));
}

QwtDoublePoint ArrowMarker::endPointCoord()
{
    return QwtDoublePoint(d_rect.right(), d_rect.bottom());
}

void ArrowMarker::setBoundingRect(double xs, double ys, double xe, double ye)
{
    if (d_rect.left() == xs && d_rect.top() == ys && d_rect.right() == xe && d_rect.bottom() == ye)
        return;

    d_rect.setLeft(xs);
    d_rect.setTop(ys);
    d_rect.setRight(xe);
    d_rect.setBottom(ye);

    if (!plot())
        return;

    plot()->updateLayout();
    d_start = QPoint(plot()->transform(xAxis(), xs), plot()->transform(yAxis(), ys));
    d_end = QPoint(plot()->transform(xAxis(), xe), plot()->transform(yAxis(), ye));
}

QwtDoubleRect ArrowMarker::boundingRect() const
{
    const QwtScaleMap &xMap = plot()->canvasMap(xAxis());
    const QwtScaleMap &yMap = plot()->canvasMap(yAxis());

    const int x0 = xMap.transform(d_rect.left());
    const int y0 = yMap.transform(d_rect.top());
    const int x1 = xMap.transform(d_rect.right());
    const int y1 = yMap.transform(d_rect.bottom());

    return QwtDoubleRect(
            x0 < x1 ? d_rect.left() : d_rect.right(), y0 < y1 ? d_rect.top() : d_rect.bottom(),
            qAbs(d_rect.left() - d_rect.right()), qAbs(d_rect.top() - d_rect.bottom()));
}

void ArrowMarker::updateBoundingRect()
{
    const QwtScaleMap &xMap = plot()->canvasMap(xAxis());
    const QwtScaleMap &yMap = plot()->canvasMap(yAxis());

    d_rect.setLeft(xMap.invTransform(d_start.x()));
    d_rect.setTop(yMap.invTransform(d_start.y()));
    d_rect.setRight(xMap.invTransform(d_end.x()));
    d_rect.setBottom(yMap.invTransform(d_end.y()));
}

ArrowMarker::~ArrowMarker()
{
    restoreCursorIfNeeded();
    if (d_editable && plot()) {
        if (plot()->canvas())
            plot()->canvas()->removeEventFilter(this);
        plot()->removeEventFilter(this);
        if (plot()->parent())
            plot()->parent()->removeEventFilter(this);
    }
}

void ArrowMarker::applyOverrideCursor()
{
    if (!d_cursor_overridden) {
        QApplication::setOverrideCursor(QCursor(Qt::SizeAllCursor));
        d_cursor_overridden = true;
    }
}

void ArrowMarker::restoreCursorIfNeeded()
{
    if (d_cursor_overridden) {
        d_cursor_overridden = false;
        QApplication::restoreOverrideCursor();
    }
}

void ArrowMarker::setEditable(bool yes)
{
    if (yes == d_editable) {
        if (!yes)
            restoreCursorIfNeeded();
        return;
    }
    if (yes) {
        d_editable = true;
        if (plot()) {
            if (plot()->canvas())
                plot()->canvas()->installEventFilter(this);
            plot()->installEventFilter(this);
            if (plot()->parent())
                plot()->parent()->installEventFilter(this);
        }
    } else {
        d_editable = false;
        d_op = None;
        setProperty("_dragMoved", false);
        setProperty("_undoSnap", QString());
        restoreCursorIfNeeded();
        if (plot()) {
            if (plot()->canvas())
                plot()->canvas()->removeEventFilter(this);
            plot()->removeEventFilter(this);
            if (plot()->parent())
                plot()->parent()->removeEventFilter(this);
        }
    }
    if (plot())
        plot()->replot();
}

bool ArrowMarker::eventFilter(QObject *obj, QEvent *e)
{
    if (!d_editable || !plot())
        return false;
    Graph *g = qobject_cast<Graph *>(plot()->parent());
    MultiLayer *ml = g ? g->multiLayerParent() : nullptr;

    if (e->type() == QEvent::KeyPress) {
        const QKeyEvent *ke = static_cast<const QKeyEvent *>(e);
        if (ke->key() == Qt::Key_Escape && d_op != None) {
            const QPoint origStart = property("_origStart").toPoint();
            const QPoint origEnd = property("_origEnd").toPoint();
            d_op = None;
            setProperty("_dragMoved", false);
            setProperty("_undoSnap", QString());
            restoreCursorIfNeeded();
            setStartPoint(origStart);
            setEndPoint(origEnd);
            plot()->replot();
            return true;
        }
        if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right
            || ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) {
            int step = 2;
            if (ke->modifiers() & Qt::ShiftModifier)
                step = 10;
            else if (ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier))
                step = 1;
            QPoint delta(0, 0);
            if (ke->key() == Qt::Key_Left)
                delta = QPoint(-step, 0);
            else if (ke->key() == Qt::Key_Right)
                delta = QPoint(step, 0);
            else if (ke->key() == Qt::Key_Up)
                delta = QPoint(0, -step);
            else if (ke->key() == Qt::Key_Down)
                delta = QPoint(0, step);
            const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
            const QPoint newStart = startPoint() + delta;
            const QPoint newEnd = endPoint() + delta;
            setStartPoint(newStart);
            setEndPoint(newEnd);
            plot()->replot();
            if (g)
                emit g->modifiedGraph();
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(Graph::tr("Move Arrow"), beforeSnap, 708);
            return true;
        }
        return false;
    }

    if (obj != plot()->canvas())
        return false;

    switch (e->type()) {
    case QEvent::MouseButtonPress: {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (me->button() != Qt::LeftButton)
            return false;
        QRect handler = QRect(QPoint(0, 0), QSize(10, 10));
        handler.moveCenter(startPoint());
        if (handler.contains(me->pos())) {
            applyOverrideCursor();
            d_op = MoveStart;
            setProperty("_pressPos", me->pos());
            setProperty("_origStart", startPoint());
            setProperty("_origEnd", endPoint());
            setProperty("_dragMoved", false);
            setProperty("_undoSnap", ml ? ml->captureUndoSnapshot() : QString());
            return true;
        }
        handler.moveCenter(endPoint());
        if (handler.contains(me->pos())) {
            applyOverrideCursor();
            d_op = MoveEnd;
            setProperty("_pressPos", me->pos());
            setProperty("_origStart", startPoint());
            setProperty("_origEnd", endPoint());
            setProperty("_dragMoved", false);
            setProperty("_undoSnap", ml ? ml->captureUndoSnapshot() : QString());
            return true;
        }
        constexpr double kPi = 3.141592653589793238462643;
        const int d = qMax(6, width() + int(std::floor(headLength() * std::tan(kPi * headAngle() / 180.0) + 0.5)));
        if (dist(me->pos().x(), me->pos().y()) <= d) {
            applyOverrideCursor();
            d_op = MoveBoth;
            d_op_startat = me->pos() - startPoint();
            setProperty("_pressPos", me->pos());
            setProperty("_origStart", startPoint());
            setProperty("_origEnd", endPoint());
            setProperty("_dragMoved", false);
            setProperty("_undoSnap", ml ? ml->captureUndoSnapshot() : QString());
            return true;
        }
        return false;
    }
    case QEvent::MouseMove: {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (d_op == None)
            return false;
        if (!property("_dragMoved").toBool()) {
            const QPoint pressPos = property("_pressPos").toPoint();
            if ((me->pos() - pressPos).manhattanLength() < QApplication::startDragDistance())
                return true;
            setProperty("_dragMoved", true);
        }
        switch (d_op) {
        case MoveStart: {
            const QPoint pt = (me->modifiers() & Qt::ShiftModifier)
                    ? snapArrowEndpointAngle45(endPoint(), me->pos())
                    : me->pos();
            setStartPoint(pt);
            plot()->replot();
            return true;
        }
        case MoveEnd: {
            const QPoint pt = (me->modifiers() & Qt::ShiftModifier)
                    ? snapArrowEndpointAngle45(startPoint(), me->pos())
                    : me->pos();
            setEndPoint(pt);
            plot()->replot();
            return true;
        }
        case MoveBoth: {
            const QPoint newStart = me->pos() - d_op_startat;
            const QPoint delta = newStart - startPoint();
            setEndPoint(endPoint() + delta);
            setStartPoint(newStart);
            plot()->replot();
            return true;
        }
        default:
            return false;
        }
    }
    case QEvent::MouseButtonRelease: {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (d_op == None) {
            restoreCursorIfNeeded();
            return false;
        }
        const Operation finishedOp = d_op;
        const bool moved = property("_dragMoved").toBool();
        const QString beforeSnap = property("_undoSnap").toString();
        d_op = None;
        setProperty("_dragMoved", false);
        setProperty("_undoSnap", QString());
        restoreCursorIfNeeded();

        if (moved) {
            switch (finishedOp) {
            case MoveStart: {
                const QPoint pt = (me->modifiers() & Qt::ShiftModifier)
                        ? snapArrowEndpointAngle45(endPoint(), me->pos())
                        : me->pos();
                setStartPoint(pt);
                break;
            }
            case MoveEnd: {
                const QPoint pt = (me->modifiers() & Qt::ShiftModifier)
                        ? snapArrowEndpointAngle45(startPoint(), me->pos())
                        : me->pos();
                setEndPoint(pt);
                break;
            }
            case MoveBoth: {
                const QPoint newStart = me->pos() - d_op_startat;
                const QPoint delta = newStart - startPoint();
                setEndPoint(endPoint() + delta);
                setStartPoint(newStart);
                break;
            }
            default:
                break;
            }
            if (plot())
                plot()->replot();
            if (g)
                emit g->modifiedGraph();
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(Graph::tr("Move Arrow"), beforeSnap);
        }
        return true;
    }
    case QEvent::MouseButtonDblClick: {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (me->button() != Qt::LeftButton || !plot())
            return false;
        QPointer<MultiLayer> mlGuard(ml);
        const QString beforeSnap = mlGuard ? mlGuard->captureUndoSnapshot() : QString();
        QUndoStack *us = mlGuard ? mlGuard->undoStack() : nullptr;
        const int countBefore = us ? us->count() : 0;
        const int indexBefore = us ? us->index() : 0;

        LineDialog *ld = new LineDialog(this, plot()->window());
        ld->setAttribute(Qt::WA_DeleteOnClose);
        ld->exec();

        if (mlGuard && !beforeSnap.isEmpty() && !mlGuard->isRestoringSnapshot()) {
            QUndoStack *usAfter = mlGuard->undoStack();
            const bool innerPushed = usAfter
                    && (usAfter->count() != countBefore || usAfter->index() != indexBefore);
            if (!innerPushed && mlGuard->captureUndoSnapshot() != beforeSnap)
                mlGuard->pushUndoSnapshot(QObject::tr("Edit Annotation"), beforeSnap);
        }
        return true;
    }
    default:
        return false;
    }
}
