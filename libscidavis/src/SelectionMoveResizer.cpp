/***************************************************************************
    File                 : SelectionMoveResizer.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Knut Franke
    Email (use @ for *)  : knut.franke*gmx.de
    Description          : Selection of Widgets and QwtPlotMarkers

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

#include "SelectionMoveResizer.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QSet>
#include <QVariant>

#include <qwt_scale_map.h>
#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>

#include "Legend.h"
#include "ArrowMarker.h"
#include "ImageMarker.h"
#include "PlotEnrichement.h"
#include "Graph.h"
#include "CanvasPicker.h"
#include "GraphMiniToolbar.h"
#include "Plot.h"
#include "MultiLayer.h"

SelectionMoveResizer::SelectionMoveResizer(Legend *target) : QWidget(target->plot()->canvas())
{
    init();
    add(target);
}
SelectionMoveResizer::SelectionMoveResizer(ArrowMarker *target) : QWidget(target->plot()->canvas())
{
    init();
    add(target);
}
SelectionMoveResizer::SelectionMoveResizer(ImageMarker *target) : QWidget(target->plot()->canvas())
{
    init();
    add(target);
}

SelectionMoveResizer::SelectionMoveResizer(QWidget *target) : QWidget(target->parentWidget())
{
    init();
    add(target);
}

void SelectionMoveResizer::init()
{
    d_bounding_rect = QRect(0, 0, -1, -1);
    d_op = None;
    d_op_start = QPoint(0, 0);
    d_op_dp = QPoint(0, 0);
    d_target_parent = parentWidget();
    d_owning_graph = owningGraph();

    if (parentWidget()) {
        setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
        parentWidget()->installEventFilter(this);
    }
    setMouseTracking(true);
    show();
    setFocus();
}

SelectionMoveResizer::~SelectionMoveResizer()
{
    if (parentWidget())
        parentWidget()->removeEventFilter(this);
    if (d_target_parent && d_target_parent != parentWidget())
        d_target_parent->removeEventFilter(this);
}

void SelectionMoveResizer::add(Legend *target)
{
    if (d_scheduled_delete || !target || (QWidget *)target->plot()->canvas() != parent())
        return;
    d_legend_markers << target;

    if (d_bounding_rect.isValid())
        d_bounding_rect |= boundingRectOf(target);
    else
        d_bounding_rect = boundingRectOf(target);

    update();
}
void SelectionMoveResizer::add(ArrowMarker *target)
{
    if (d_scheduled_delete || !target || (QWidget *)target->plot()->canvas() != parent())
        return;
    d_line_markers << target;

    if (d_bounding_rect.isValid())
        d_bounding_rect |= boundingRectOf(target);
    else
        d_bounding_rect = boundingRectOf(target);

    update();
}
void SelectionMoveResizer::add(ImageMarker *target)
{
    if (d_scheduled_delete || !target || (QWidget *)target->plot()->canvas() != parent())
        return;
    d_image_markers << target;

    if (d_bounding_rect.isValid())
        d_bounding_rect |= boundingRectOf(target);
    else
        d_bounding_rect = boundingRectOf(target);

    update();
}

void SelectionMoveResizer::add(QWidget *target)
{
    if (d_scheduled_delete || !target || target->parentWidget() != parent())
        return;
    d_widgets << target;
    target->installEventFilter(this);
    connect(target, SIGNAL(destroyed(QObject *)), this, SLOT(removeWidget(QObject *)));

    if (d_bounding_rect.isValid())
        d_bounding_rect |= target->frameGeometry();
    else
        d_bounding_rect = target->frameGeometry();

    update();
}

QRect SelectionMoveResizer::boundingRectOf(QwtPlotMarker *target) const
{
    return ((PlotEnrichement *)target)->rect();
}

void SelectionMoveResizer::detachAndDelete()
{
    if (d_scheduled_delete)
        return;
    d_scheduled_delete = true;
    const bool hadFocus = hasFocus();
    QPointer<QWidget> pw(parentWidget());
    QPointer<QWidget> focusRestoreTarget = pw;
    if (MultiLayer *ml = owningMultiLayer()) {
        if (Graph *ag = ml->activeGraph()) {
            if (ag->plotWidget() && ag->plotWidget()->canvas())
                focusRestoreTarget = ag->plotWidget()->canvas();
            else
                focusRestoreTarget = ag;
        }
    }
    blockSignals(true);
    disconnect();
    if (pw)
        pw->removeEventFilter(this);
    if (d_target_parent && d_target_parent != pw)
        d_target_parent->removeEventFilter(this);
    for (QWidget *w : d_widgets) {
        if (w)
            w->removeEventFilter(this);
    }
    d_legend_markers.clear();
    d_line_markers.clear();
    d_image_markers.clear();
    d_widgets.clear();
    hide();
    setParent(nullptr);
    if (hadFocus && focusRestoreTarget)
        focusRestoreTarget->setFocus();
    deleteLater();
}

void SelectionMoveResizer::cleanupIfEmpty()
{
    if (d_scheduled_delete)
        return;
    if (d_legend_markers.isEmpty() && d_line_markers.isEmpty() && d_image_markers.isEmpty()
        && d_widgets.isEmpty()) {
        if (d_defer_self_delete) {
            hide();
            return;
        }
        const bool hadParent = (parentWidget() != nullptr);
        if (!owningGraph()) {
            if (MultiLayer *ml = owningMultiLayer())
                ml->clearLayerSelector();
        }
        if (!hadParent || parentWidget() != nullptr) {
            detachAndDelete();
        } else {
            if (d_target_parent)
                d_target_parent->removeEventFilter(this);
            d_scheduled_delete = true;
            d_legend_markers.clear();
            d_line_markers.clear();
            d_image_markers.clear();
            d_widgets.clear();
        }
    } else {
        recalcBoundingRect();
    }
}

int SelectionMoveResizer::removeAll(Legend *target)
{
    int result = d_legend_markers.removeAll(target);
    cleanupIfEmpty();
    return result;
}
int SelectionMoveResizer::removeAll(ArrowMarker *target)
{
    int result = d_line_markers.removeAll(target);
    cleanupIfEmpty();
    return result;
}
int SelectionMoveResizer::removeAll(ImageMarker *target)
{
    int result = d_image_markers.removeAll(target);
    cleanupIfEmpty();
    return result;
}

int SelectionMoveResizer::removeAll(QWidget *target)
{
    int result = d_widgets.removeAll(target);
    cleanupIfEmpty();
    return result;
}

void SelectionMoveResizer::recalcBoundingRect()
{
    if (d_scheduled_delete)
        return;
    d_bounding_rect = QRect(0, 0, -1, -1);

    foreach (Legend *i, d_legend_markers) {
        if (d_bounding_rect.isValid())
            d_bounding_rect |= boundingRectOf(i);
        else
            d_bounding_rect = boundingRectOf(i);
    }
    foreach (ArrowMarker *i, d_line_markers) {
        if (d_bounding_rect.isValid())
            d_bounding_rect |= boundingRectOf(i);
        else
            d_bounding_rect = boundingRectOf(i);
    }
    foreach (ImageMarker *i, d_image_markers) {
        if (d_bounding_rect.isValid())
            d_bounding_rect |= boundingRectOf(i);
        else
            d_bounding_rect = boundingRectOf(i);
    }
    foreach (QWidget *i, d_widgets) {
        if (d_bounding_rect.isValid())
            d_bounding_rect |= i->frameGeometry();
        else
            d_bounding_rect = i->frameGeometry();
    }

    update();
}

const QRect SelectionMoveResizer::handlerRect(QRect rect, Operation op)
{
    switch (op) {
    case Resize_N:
        return QRect(QPoint(rect.center().x() - handler_size / 2, rect.top() + 1),
                     QSize(handler_size, handler_size));
    case Resize_NE:
        return QRect(QPoint(rect.right() - handler_size + 1, rect.top() + 1),
                     QSize(handler_size, handler_size));
    case Resize_E:
        return QRect(
                QPoint(rect.right() - handler_size + 1, rect.center().y() - handler_size / 2 + 1),
                QSize(handler_size, handler_size));
    case Resize_SE:
        return QRect(QPoint(rect.right() - handler_size + 1, rect.bottom() - handler_size + 1),
                     QSize(handler_size, handler_size));
    case Resize_S:
        return QRect(QPoint(rect.center().x() - handler_size / 2, rect.bottom() - handler_size + 1),
                     QSize(handler_size, handler_size));
    case Resize_SW:
        return QRect(QPoint(rect.left(), rect.bottom() - handler_size + 1),
                     QSize(handler_size, handler_size));
    case Resize_W:
        return QRect(QPoint(rect.left(), rect.center().y() - handler_size / 2 + 1),
                     QSize(handler_size, handler_size));
    case Resize_NW:
        return QRect(rect.topLeft() + QPoint(0, 1), QSize(handler_size, handler_size));
    default:
        return QRect(rect.topLeft(), QSize(0, 0));
    }
}

QRect SelectionMoveResizer::operateOn(const QRect in)
{
    QRect boundary_out = d_bounding_rect;
    switch (d_op) {
    case Move:
        boundary_out.translate(d_op_dp);
        break;
    case Resize_N:
        boundary_out.setTop(boundary_out.top() + d_op_dp.y());
        break;
    case Resize_E:
        boundary_out.setRight(boundary_out.right() + d_op_dp.x());
        break;
    case Resize_S:
        boundary_out.setBottom(boundary_out.bottom() + d_op_dp.y());
        break;
    case Resize_W:
        boundary_out.setLeft(boundary_out.left() + d_op_dp.x());
        break;
    case Resize_NE:
        boundary_out.setTop(boundary_out.top() + d_op_dp.y());
        boundary_out.setRight(boundary_out.right() + d_op_dp.x());
        break;
    case Resize_SE:
        boundary_out.setBottom(boundary_out.bottom() + d_op_dp.y());
        boundary_out.setRight(boundary_out.right() + d_op_dp.x());
        break;
    case Resize_SW:
        boundary_out.setBottom(boundary_out.bottom() + d_op_dp.y());
        boundary_out.setLeft(boundary_out.left() + d_op_dp.x());
        break;
    case Resize_NW:
        boundary_out.setTop(boundary_out.top() + d_op_dp.y());
        boundary_out.setLeft(boundary_out.left() + d_op_dp.x());
        break;
    default:
        return in;
    }
    boundary_out = boundary_out.normalized();
    if (!d_widgets.isEmpty() && d_op != Move && d_bounding_rect.width() > 0
        && d_bounding_rect.height() > 0) {
        MultiLayer *ml = nullptr;
        for (QWidget *p = parentWidget(); p; p = p->parentWidget()) {
            if ((ml = qobject_cast<MultiLayer *>(p)) != nullptr)
                break;
        }
        const bool isSideHandle = (d_op == Resize_N || d_op == Resize_S
                                   || d_op == Resize_E || d_op == Resize_W);
        const bool shiftHeld = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
        const bool lockAspect =
                !isSideHandle && !shiftHeld && (ml ? ml->keepAspectRatio() : true);
        int marginW = 0;
        int marginH = 0;
        double canvasAspect =
                (double)d_bounding_rect.width() / (double)d_bounding_rect.height();
        if (d_widgets.size() == 1) {
            Graph *g = qobject_cast<Graph *>(d_widgets.first());
            if (g && g->plotWidget() && g->plotWidget()->plotLayout()) {
                QRect cRect = g->plotWidget()->plotLayout()->canvasRect();
                if (cRect.width() > 0 && cRect.height() > 0) {
                    marginW = qMax(0, d_bounding_rect.width() - cRect.width());
                    marginH = qMax(0, d_bounding_rect.height() - cRect.height());
                    canvasAspect = (double)cRect.width() / (double)cRect.height();
                }
            }
        }
        if (ml && ml->targetAspectRatio() > 0.05)
            canvasAspect = ml->targetAspectRatio();

        int newCW = qMax(50, boundary_out.width() - marginW);
        int newCH = qMax(50, boundary_out.height() - marginH);
        if (lockAspect) {
            if (d_op == Resize_E || d_op == Resize_W) {
                newCH = qMax(50, qRound(newCW / canvasAspect));
            } else if (d_op == Resize_N || d_op == Resize_S) {
                newCW = qMax(50, qRound(newCH * canvasAspect));
            } else {
                if (qAbs(boundary_out.width() - d_bounding_rect.width())
                    >= qAbs(boundary_out.height() - d_bounding_rect.height())) {
                    newCH = qMax(50, qRound(newCW / canvasAspect));
                } else {
                    newCW = qMax(50, qRound(newCH * canvasAspect));
                }
            }
            const int newW = newCW + marginW;
            const int newH = newCH + marginH;
            int left = d_bounding_rect.left();
            int top = d_bounding_rect.top();
            if (d_op == Resize_W || d_op == Resize_NW || d_op == Resize_SW)
                left = d_bounding_rect.right() - newW + 1;
            else if (d_op == Resize_N || d_op == Resize_S)
                left = d_bounding_rect.center().x() - newW / 2;

            if (d_op == Resize_N || d_op == Resize_NW || d_op == Resize_NE)
                top = d_bounding_rect.bottom() - newH + 1;
            else if (d_op == Resize_E || d_op == Resize_W)
                top = d_bounding_rect.center().y() - newH / 2;

            boundary_out = QRect(left, top, newW, newH);
        } else {
            const int newW = newCW + marginW;
            const int newH = newCH + marginH;
            int left = boundary_out.left();
            int top = boundary_out.top();
            if (d_op == Resize_W || d_op == Resize_NW || d_op == Resize_SW)
                left = d_bounding_rect.right() - newW + 1;
            if (d_op == Resize_N || d_op == Resize_NW || d_op == Resize_NE)
                top = d_bounding_rect.bottom() - newH + 1;
            boundary_out = QRect(left, top, newW, newH);
        }
    }
    if (in == d_bounding_rect)
        return boundary_out;
    double scale_x = ((double)boundary_out.width()) / d_bounding_rect.width();
    double scale_y = ((double)boundary_out.height()) / d_bounding_rect.height();
    int offset_x = qRound(boundary_out.left() - d_bounding_rect.left() * scale_x);
    int offset_y = qRound(boundary_out.top() - d_bounding_rect.top() * scale_y);
    return QRect(
            QPoint(qRound(in.left() * scale_x) + offset_x, qRound(in.top() * scale_y) + offset_y),
            QSize(qRound(in.width() * scale_x), qRound(in.height() * scale_y)));
}

bool SelectionMoveResizer::isLegendOnlySelection() const
{
    return !d_legend_markers.isEmpty() && d_line_markers.isEmpty() && d_image_markers.isEmpty()
            && d_widgets.isEmpty();
}

Graph *SelectionMoveResizer::owningGraph() const
{
    for (QWidget *p = parentWidget(); p; p = p->parentWidget()) {
        if (Graph *g = qobject_cast<Graph *>(p))
            return g;
    }
    return d_owning_graph.data();
}

MultiLayer *SelectionMoveResizer::owningMultiLayer() const
{
    for (QWidget *p = parentWidget(); p; p = p->parentWidget()) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(p))
            return ml;
    }
    return nullptr;
}

void SelectionMoveResizer::operateOnTargets()
{
    if (d_scheduled_delete)
        return;
    QSet<QwtPlot *> plotsToReplot;
    Graph *ownerGraph = owningGraph();
    foreach (Legend *i, d_legend_markers) {
        if (!i)
            continue;
        const bool isMainLegend = (ownerGraph && ownerGraph->legend() == i);
        const QRect old_rect = i->rect();
        QRect new_rect = operateOn(old_rect);
        i->setOrigin(new_rect.topLeft());
        if (QwtPlot *plt = i->plot()) {
            if (isMainLegend)
                plt->setProperty("_toriLegendAnchor", QVariant());
            if (isMainLegend && !d_in_key_nudge
                && plt->property("_outsideRightMargin").toInt() > 0 && plt->canvas()
                && new_rect.right() <= plt->canvas()->width()) {
                plt->setProperty("_outsideRightMargin", 0);
                plt->updateLayout();
                i->setOrigin(new_rect.topLeft());
            }
            plotsToReplot.insert(plt);
        }
    }
    foreach (ArrowMarker *i, d_line_markers) {
        if (!i)
            continue;
        QPoint p1 = i->startPoint();
        QPoint p2 = i->endPoint();
        QRect new_rect = operateOn(i->rect());
        i->setStartPoint(QPoint(p1.x() < p2.x() ? new_rect.left() : new_rect.right(),
                                p1.y() < p2.y() ? new_rect.top() : new_rect.bottom()));
        i->setEndPoint(QPoint(p2.x() < p1.x() ? new_rect.left() : new_rect.right(),
                              p2.y() < p1.y() ? new_rect.top() : new_rect.bottom()));
        if (i->plot())
            plotsToReplot.insert(i->plot());
    }
    foreach (ImageMarker *i, d_image_markers) {
        if (!i)
            continue;
        QRect new_rect = operateOn(i->rect());
        i->setOrigin(new_rect.topLeft());
        i->setSize(new_rect.size());
        if (i->plot())
            plotsToReplot.insert(i->plot());
    }
    const bool isSideHandle = (d_op == Resize_N || d_op == Resize_S
                               || d_op == Resize_E || d_op == Resize_W);
    const bool shiftHeld = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
    const bool ctrlHeld = (QApplication::keyboardModifiers() & Qt::ControlModifier) != 0;
    const bool isWidgetResize = (!d_widgets.isEmpty() && d_op != Move && d_op != None);
    Graph *resizedGraph = nullptr;
    foreach (QWidget *i, d_widgets) {
        if (!i)
            continue;
        const QRect oldGeom = i->geometry();
        const QRect newGeom = operateOn(oldGeom);
        i->setGeometry(newGeom);
        // ugly hack
        // see Graph::setIgnoreResizeEvents() and Graph::resizeEvent()
        Graph *g = qobject_cast<Graph *>(i);
        if (g) {
            if (ctrlHeld && isWidgetResize && oldGeom.width() > 10 && oldGeom.height() > 10
                && newGeom.width() > 10 && newGeom.height() > 10) {
                const double rx = double(newGeom.width()) / double(oldGeom.width());
                const double ry = double(newGeom.height()) / double(oldGeom.height());
                const double ratio = (isSideHandle && (d_op == Resize_E || d_op == Resize_W))
                        ? rx
                        : ((isSideHandle && (d_op == Resize_N || d_op == Resize_S))
                                   ? ry
                                   : std::sqrt(rx * ry));
                if (std::isfinite(ratio) && ratio > 0.0 && std::fabs(ratio - 1.0) > 1e-4)
                    g->scaleFonts(ratio);
            }
            g->plotWidget()->resize(i->size());
            g->plotWidget()->updateLayout();
            if (!resizedGraph)
                resizedGraph = g;
        }
    }
    if (isWidgetResize && resizedGraph) {
        if (MultiLayer *ml = owningMultiLayer()) {
            if (!ml->isInsetLayer(resizedGraph) && !resizedGraph->linkedParentLayer()) {
                const QRect cRect =
                        (resizedGraph->plotWidget() && resizedGraph->plotWidget()->plotLayout())
                        ? resizedGraph->plotWidget()->plotLayout()->canvasRect()
                        : QRect();
                const int cw = (cRect.width() > 20) ? cRect.width() : resizedGraph->width();
                const int ch = (cRect.height() > 20) ? cRect.height() : resizedGraph->height();
                if (cw > 0 && ch > 0) {
                    const double newRatio = double(cw) / double(ch);
                    if (isSideHandle || shiftHeld)
                        ml->syncAspectRatioAfterManualResize(newRatio, cw, ch, true);
                    else if (!ml->keepAspectRatio())
                        ml->syncAspectRatioAfterManualResize(newRatio, cw, ch, false);
                    else
                        resizedGraph->syncChildLinkedLayers();
                }
            }
        }
    }
    for (QwtPlot *plt : plotsToReplot) {
        if (plt)
            plt->replot();
    }
    recalcBoundingRect();

    d_op_start = d_op_dp = QPoint(0, 0);

    update();
    emit targetsChanged();
}

void SelectionMoveResizer::paintEvent(QPaintEvent *e)
{
    if (d_scheduled_delete)
        return;
    if (Graph *g = owningGraph()) {
        if (g->isExporting() || g->property("isExporting").toBool())
            return;
    }

    QPainter p(this);
    QRect drawn_rect = operateOn(d_bounding_rect);

    if (isLegendOnlySelection()) {
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF frameRect = QRectF(drawn_rect).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 210), 3.0, Qt::SolidLine));
        p.drawRect(frameRect);
        p.setPen(QPen(QColor(QStringLiteral("#2563EB")), 1.5, Qt::DashLine));
        p.drawRect(frameRect);
        e->accept();
        return;
    }

    QPen white_pen(Qt::white, 3, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    p.setPen(white_pen);
    p.drawRect(QRect(QPoint(drawn_rect.left(), drawn_rect.top()), drawn_rect.size()));
    white_pen.setWidth(2);
    p.setPen(white_pen);
    for (int i = 0; i < 8; i++)
        p.drawRect(handlerRect(drawn_rect, (Operation)i));
    p.setPen(QPen(Qt::black, 1, Qt::SolidLine));
    p.drawRect(QRect(QPoint(drawn_rect.left(), drawn_rect.top()), drawn_rect.size()));
    for (int i = 0; i < 8; i++)
        p.fillRect(handlerRect(drawn_rect, (Operation)i), QBrush(Qt::black));

    e->accept();
}

SelectionMoveResizer::Operation SelectionMoveResizer::operateModeFor(const QPoint &pos) const
{
    if (!d_bounding_rect.isValid())
        return None;
    if (!isLegendOnlySelection()) {
        for (int i = 0; i < 8; i++) {
            if (handlerRect(d_bounding_rect, (Operation)i).contains(pos))
                return (Operation)i;
        }
    }
    if (d_bounding_rect.contains(pos))
        return Move;
    return None;
}

void SelectionMoveResizer::mousePressEvent(QMouseEvent *me)
{
    if (d_scheduled_delete) {
        me->ignore();
        return;
    }
    if (me->button() == Qt::RightButton) {
        const Operation hitOp = operateModeFor(me->pos());
        if (hitOp != None) {
            if (owningGraph()) {
                me->accept();
                emit targetsContextMenuRequested();
            } else {
                me->ignore();
            }
            return;
        }
        if (Graph *g = owningGraph()) {
            QPointer<Graph> gGuard(g);
            QPointer<QWidget> canvas(parentWidget());
            QMouseEvent forwarded(QEvent::MouseButtonPress, me->pos(), me->windowPos(),
                                  me->screenPos(), me->button(), me->buttons(), me->modifiers(),
                                  me->source());
            me->accept();
            if (gGuard)
                gGuard->deselectMarker();
            if (canvas)
                QCoreApplication::sendEvent(canvas.data(), &forwarded);
            return;
        }
        me->ignore();
        return;
    }
    const Operation hitOp = operateModeFor(me->pos());
    if (me->button() != Qt::LeftButton || hitOp == None) {
        me->ignore();
        return;
    }
    d_op_start = me->pos();
    d_op_dp = QPoint(0, 0);
    d_drag_threshold_passed = false;
    d_undo_before_snapshot.clear();
    d_op = hitOp;

    me->accept();
}

void SelectionMoveResizer::contextMenuEvent(QContextMenuEvent *e)
{
    if (d_scheduled_delete) {
        e->ignore();
        return;
    }
    if (!isVisible() || !parentWidget()) {
        if (d_owning_graph && d_owning_graph->selectedMarkerKey() < 0 && d_target_parent) {
            QContextMenuEvent fwd(e->reason(), e->pos(), e->globalPos(), e->modifiers());
            QCoreApplication::sendEvent(d_target_parent.data(), &fwd);
        }
        e->accept();
        return;
    }
    if (operateModeFor(e->pos()) != None) {
        if (owningGraph()) {
            if (e->reason() == QContextMenuEvent::Keyboard)
                emit targetsContextMenuRequested();
            e->accept();
        } else {
            e->ignore();
        }
        return;
    }
    e->ignore();
}

void SelectionMoveResizer::mouseMoveEvent(QMouseEvent *me)
{
    if (d_scheduled_delete) {
        me->ignore();
        return;
    }
    if (d_op == None && d_bounding_rect.contains(me->pos()) && (me->buttons() & Qt::LeftButton)) {
        d_op = Move;
        d_op_start = me->pos();
        d_op_dp = QPoint(0, 0);
        d_drag_threshold_passed = false;
        d_undo_before_snapshot.clear();
    }
    if (d_op == None || d_op_start == QPoint(-1, -1)) {
        if (d_bounding_rect.contains(me->pos())) {
            setCursor(Qt::SizeAllCursor);
            if (!isLegendOnlySelection()) {
                if (handlerRect(d_bounding_rect, Resize_N).contains(me->pos())
                    || handlerRect(d_bounding_rect, Resize_S).contains(me->pos()))
                    setCursor(Qt::SizeVerCursor);
                else if (handlerRect(d_bounding_rect, Resize_E).contains(me->pos())
                         || handlerRect(d_bounding_rect, Resize_W).contains(me->pos()))
                    setCursor(Qt::SizeHorCursor);
                else if (handlerRect(d_bounding_rect, Resize_NE).contains(me->pos())
                         || handlerRect(d_bounding_rect, Resize_SW).contains(me->pos()))
                    setCursor(Qt::SizeBDiagCursor);
                else if (handlerRect(d_bounding_rect, Resize_NW).contains(me->pos())
                         || handlerRect(d_bounding_rect, Resize_SE).contains(me->pos()))
                    setCursor(Qt::SizeFDiagCursor);
            }
        } else
            unsetCursor();
        return QWidget::mouseMoveEvent(me);
    }

    if (!d_drag_threshold_passed) {
        const int dragThresh = qMax(4, QApplication::startDragDistance());
        if ((me->pos() - d_op_start).manhattanLength() < dragThresh) {
            me->accept();
            return;
        }
        d_drag_threshold_passed = true;
        if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
            bar->hide();
        if (MultiLayer *ml = owningMultiLayer())
            d_undo_before_snapshot = ml->captureUndoSnapshot();
    }

    d_op_dp = me->pos() - d_op_start;
    repaint();
    me->accept();
}

void SelectionMoveResizer::mouseDoubleClickEvent(QMouseEvent *e)
{
    d_op = None;
    d_drag_threshold_passed = false;
    d_op_dp = QPoint(0, 0);
    d_undo_before_snapshot.clear();
    e->ignore();
}

void SelectionMoveResizer::mouseReleaseEvent(QMouseEvent *me)
{
    if (d_scheduled_delete) {
        me->ignore();
        return;
    }
    if (me->button() != Qt::LeftButton || d_op == None || d_op_start == QPoint(-1, -1))
        return QWidget::mouseReleaseEvent(me);

    if (!d_drag_threshold_passed) {
        d_op = None;
        d_op_dp = QPoint(0, 0);
        d_op_start = QPoint(0, 0);
        d_undo_before_snapshot.clear();
        if (d_legend_markers.size() == 1 && d_line_markers.isEmpty() && d_image_markers.isEmpty()
            && d_widgets.isEmpty() && GraphMiniToolbar::isMiniToolbarEnabled()) {
            if (Graph *g = owningGraph()) {
                Legend *leg = d_legend_markers.first();
                if (leg) {
                    if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                        bar->popupForLegend(g, leg, me->globalPos());
                }
            }
        }
        me->accept();
        return;
    }

    const bool wasResize = (d_op != Move);
    const QString beforeSnap = d_undo_before_snapshot;
    d_drag_threshold_passed = false;
    d_undo_before_snapshot.clear();
    d_op_dp = me->pos() - d_op_start;
    operateOnTargets();
    d_op = None;
    if (MultiLayer *ml = owningMultiLayer()) {
        if (!beforeSnap.isEmpty())
            ml->pushUndoSnapshot(wasResize ? tr("Resize Object") : tr("Move Object"), beforeSnap);
    }
    me->accept();
}

void SelectionMoveResizer::keyPressEvent(QKeyEvent *ke)
{
    if (d_scheduled_delete) {
        ke->ignore();
        return;
    }
    switch (ke->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_F2:
        if (d_op == None) {
            // Same entry as double-click: a single selected legend / text annotation opens
            // the inline text editor instead of the legacy modal TextDialog.
            if (!(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier))
                && d_legend_markers.size() == 1 && d_line_markers.isEmpty()
                && d_image_markers.isEmpty()) {
                if (Graph *g = owningGraph()) {
                    long key = g->selectedMarkerKey();
                    if (key < 0 || g->textMarker(key) != d_legend_markers.first()) {
                        key = -1;
                        for (int k : g->textMarkerKeys()) {
                            if (g->textMarker(k) == d_legend_markers.first()) {
                                key = k;
                                break;
                            }
                        }
                    }
                    if (key >= 0 && CanvasPicker::openInlineTextEditorForMarker(g, key)) {
                        ke->accept();
                        return;
                    }
                }
            }
            ke->ignore();
            break;
        }
        if (ke->key() == Qt::Key_F2) {
            ke->ignore();
            break;
        }
        operateOnTargets();
        d_op = None;
        ke->accept();
        break;
    case Qt::Key_Escape: {
        if (d_op != None) {
            d_op = None;
            d_op_dp = QPoint(0, 0);
            d_drag_threshold_passed = false;
            d_undo_before_snapshot.clear();
            repaint();
            ke->accept();
            return;
        }
        if (Graph *g = owningGraph()) {
            QPointer<SelectionMoveResizer> self(this);
            if (!g->handleEscapeKey() && self && !d_scheduled_delete)
                detachAndDelete();
        } else {
            const bool hadParent = (parentWidget() != nullptr);
            if (MultiLayer *ml = owningMultiLayer())
                ml->clearLayerSelector();
            if (!hadParent || parentWidget() != nullptr) {
                detachAndDelete();
            } else {
                for (QWidget *w : d_widgets) {
                    if (w)
                        w->removeEventFilter(this);
                }
                if (d_target_parent)
                    d_target_parent->removeEventFilter(this);
                d_scheduled_delete = true;
                d_legend_markers.clear();
                d_line_markers.clear();
                d_image_markers.clear();
                d_widgets.clear();
            }
        }
        ke->accept();
        return;
    }
    case Qt::Key_Delete: {
        if (deleteAllSelectedMarkers(owningGraph(), this)) {
            ke->accept();
            return;
        }
        ke->ignore();
        break;
    }
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down: {
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
        MultiLayer *ml = owningMultiLayer();
        const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
        d_op = Move;
        d_op_start = QPoint(0, 0);
        d_op_dp = delta;
        d_in_key_nudge = true;
        operateOnTargets();
        d_in_key_nudge = false;
        d_op = None;
        d_op_dp = QPoint(0, 0);
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(tr("Move Object"), beforeSnap, 702);
        repaint();
        ke->accept();
        break;
    }
    default:
        ke->ignore();
    }
}

bool SelectionMoveResizer::eventFilter(QObject *o, QEvent *e)
{
    if (d_scheduled_delete)
        return false;
    switch (e->type()) {
    case QEvent::Resize:
        if ((QWidget *)o == parentWidget())
            setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
        recalcBoundingRect();
        return false;
    case QEvent::Move:
        if ((QWidget *)o != parentWidget())
            recalcBoundingRect();
        return false;
    default:
        return false;
    }
}

QList<long> SelectionMoveResizer::selectedMarkerKeysForGraph(Graph *g) const
{
    QList<long> keys;
    if (!g)
        return keys;
    for (Legend *leg : d_legend_markers) {
        if (!leg)
            continue;
        for (int k : g->textMarkerKeys()) {
            if (g->textMarker(k) == leg) {
                keys << k;
                break;
            }
        }
    }
    for (ArrowMarker *arr : d_line_markers) {
        if (!arr)
            continue;
        for (int k : g->lineMarkerKeys()) {
            if (g->arrow(k) == arr) {
                keys << k;
                break;
            }
        }
    }
    for (ImageMarker *img : d_image_markers) {
        if (!img)
            continue;
        for (int k : g->imageMarkerKeys()) {
            if (g->imageMarker(k) == img) {
                keys << k;
                break;
            }
        }
    }
    return keys;
}

bool SelectionMoveResizer::deleteAllSelectedMarkers(Graph *g, SelectionMoveResizer *resizer)
{
    if (!g)
        return false;
    QPointer<SelectionMoveResizer> smr(resizer);
    if (!smr && g->plotWidget() && g->plotWidget()->canvas())
        smr = g->plotWidget()->canvas()->findChild<SelectionMoveResizer *>();
    QList<long> keys;
    if (smr)
        keys = smr->selectedMarkerKeysForGraph(g);
    if (keys.isEmpty()) {
        const long k = g->selectedMarkerKey();
        if (k < 0)
            return false;
        keys << k;
    }

    QPointer<Graph> gGuard(g);
    MultiLayer *ml = g->multiLayerParent();
    QPointer<MultiLayer> mlGuard(ml);
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();

    if (smr)
        smr->d_defer_self_delete = true;
    for (long mk : keys) {
        if (!gGuard)
            return true;
        gGuard->setSelectedMarker(mk, false);
        gGuard->removeMarker();
    }
    if (!gGuard)
        return true;
    gGuard->deselectMarker();
    gGuard->setCurrentSelection({ Graph::GraphSelection::None, -1, -1 });
    if (smr && !smr->d_scheduled_delete && smr->parentWidget()
        && smr->d_legend_markers.isEmpty() && smr->d_line_markers.isEmpty()
        && smr->d_image_markers.isEmpty() && smr->d_widgets.isEmpty()) {
        smr->detachAndDelete();
    }
    if (mlGuard && !beforeSnap.isEmpty())
        mlGuard->pushUndoSnapshot(tr("Delete Annotation"), beforeSnap);
    return true;
}
