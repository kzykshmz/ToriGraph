/***************************************************************************
    File                 : CanvasPicker.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006,2007 by Ion Vasilief,
                           Tilman Benkert, Knut Franke
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
                           knut.franke*gmx.de
    Description          : Canvas picker

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
#include "CanvasPicker.h"
#include "ImageMarker.h"
#include "Legend.h"
#include "ArrowMarker.h"
#include "GraphMiniToolbar.h"
#include "CanvasInlineTextEditor.h"
#include "SelectionMoveResizer.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"
#include "MultiLayer.h"

#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QVector>

#include <cmath>

#include <qwt_plot_canvas.h>
#include <qwt_scale_map.h>
#include <qwt_text_label.h>

CanvasPicker::CanvasPicker(Graph *graph) : QObject(graph)
{
    pointSelected = false;
    d_editing_marker = nullptr;
    d_forwarded_resizer = nullptr;
    d_left_click_pending = false;

    plotWidget = graph->plotWidget();

    QwtPlotCanvas *canvas = plotWidget->canvas();
    canvas->setMouseTracking(true);
    canvas->installEventFilter(this);
    plotWidget->installEventFilter(this);
}

bool CanvasPicker::eventFilter(QObject *object, QEvent *e)
{
    Graph *g = plot();
    if (!g || !plotWidget || !plotWidget->canvas())
        return false;

    QVector<int> images = g->imageMarkerKeys();
    QVector<int> texts = g->textMarkerKeys();
    QVector<int> lines = g->lineMarkerKeys();

    if (object == plotWidget) {
        QwtPlotCanvas *cv = plotWidget->canvas();
        if (e->type() == QEvent::MouseMove && d_outer_legend_dragging) {
            const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
            if (!(me->buttons() & Qt::LeftButton)) {
                d_outer_legend_dragging = false;
                d_outer_legend_moved = false;
                d_outer_legend_key = -1;
                d_outer_legend_undo_before.clear();
                return false;
            }
            const QPoint canvasPos = me->pos() - cv->pos();
            if (!d_outer_legend_moved) {
                const int dragThresh = qMax(4, QApplication::startDragDistance());
                if ((canvasPos - d_outer_legend_press_pos).manhattanLength() < dragThresh)
                    return true;
                d_outer_legend_moved = true;
                if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                    bar->hide();
                if (MultiLayer *ml = g->multiLayerParent())
                    d_outer_legend_undo_before = ml->captureUndoSnapshot();
            }
            Legend *leg = (d_outer_legend_key >= 0)
                    ? dynamic_cast<Legend *>(plotWidget->marker(d_outer_legend_key))
                    : nullptr;
            if (leg) {
                leg->setOrigin(d_outer_legend_orig_origin + (canvasPos - d_outer_legend_press_pos));
                plotWidget->replot();
                if (SelectionMoveResizer *sel = cv->findChild<SelectionMoveResizer *>())
                    sel->recalcBoundingRect();
            }
            return true;
        }

        if (e->type() == QEvent::MouseButtonRelease && d_outer_legend_dragging) {
            const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton) {
                const bool moved = d_outer_legend_moved;
                const long legKey = d_outer_legend_key;
                const QString beforeSnap = d_outer_legend_undo_before;
                const QPoint canvasPos = me->pos() - cv->pos();
                const QPoint targetOrigin =
                        d_outer_legend_orig_origin + (canvasPos - d_outer_legend_press_pos);
                d_outer_legend_dragging = false;
                d_outer_legend_moved = false;
                d_outer_legend_key = -1;
                d_outer_legend_undo_before.clear();

                Legend *leg = (legKey >= 0) ? dynamic_cast<Legend *>(plotWidget->marker(legKey))
                                            : nullptr;
                if (moved) {
                    if (leg) {
                        leg->setOrigin(targetOrigin);
                        if (g->legend() == leg)
                            plotWidget->setProperty("_toriLegendAnchor", QVariant());
                        if (g->legend() == leg
                            && plotWidget->property("_outsideRightMargin").toInt() > 0
                            && leg->rect().right() <= cv->width()) {
                            plotWidget->setProperty("_outsideRightMargin", 0);
                            plotWidget->updateLayout();
                            leg->setOrigin(targetOrigin);
                        }
                        plotWidget->replot();
                        if (SelectionMoveResizer *sel = cv->findChild<SelectionMoveResizer *>())
                            sel->recalcBoundingRect();
                        emit g->modifiedGraph();
                        if (MultiLayer *ml = g->multiLayerParent()) {
                            if (!beforeSnap.isEmpty())
                                ml->pushUndoSnapshot(tr("Move Object"), beforeSnap);
                        }
                    }
                } else if (leg && g->legend() == leg && GraphMiniToolbar::isMiniToolbarEnabled()) {
                    if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                        bar->popupForLegend(g, leg, me->globalPos());
                }
                return true;
            }
        }

        if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonDblClick) {
            const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
            const QPoint canvasPos = me->pos() - cv->pos();
            if (!cv->rect().contains(canvasPos)) {
                Legend *hitLeg = nullptr;
                long hitKey = -1;
                for (int k : texts) {
                    Legend *m = g->textMarker(k);
                    if (m && m->isVisible() && m->rect().contains(canvasPos)) {
                        hitLeg = m;
                        hitKey = k;
                        if (g->legend() == m)
                            break;
                    }
                }
                if (hitLeg && hitKey >= 0) {
                    if (e->type() == QEvent::MouseButtonPress) {
                        emit selectPlot();
                        disableEditing();
                        g->setSelectedMarker(hitKey, false);
                        const Graph::GraphSelection::Kind kind = (g->legend() == hitLeg)
                                ? Graph::GraphSelection::LegendBox
                                : Graph::GraphSelection::TextAnnotation;
                        g->setCurrentSelection({ kind, int(hitKey), -1 });
                        if (me->button() == Qt::RightButton) {
                            d_outer_legend_dragging = false;
                            emit showMarkerPopupMenu();
                        } else if (me->button() == Qt::LeftButton) {
                            d_outer_legend_dragging = true;
                            d_outer_legend_moved = false;
                            d_outer_legend_key = hitKey;
                            d_outer_legend_press_pos = canvasPos;
                            d_outer_legend_orig_origin = hitLeg->rect().topLeft();
                            d_outer_legend_undo_before.clear();
                        }
                        return true;
                    }
                    if (e->type() == QEvent::MouseButtonDblClick
                        && me->button() == Qt::LeftButton) {
                        d_outer_legend_dragging = false;
                        d_outer_legend_moved = false;
                        d_outer_legend_key = -1;
                        d_outer_legend_undo_before.clear();
                        if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                            bar->cancelScheduledAndHide();
                        const int rowIdx =
                                (g->legend() == hitLeg) ? hitLeg->itemIndexAt(canvasPos) : -1;
                        const QRect localRect = hitLeg->rect();
                        const QRect anchorGlobalRect(cv->mapToGlobal(localRect.topLeft()),
                                                     localRect.size());
                        CanvasInlineTextEditor::openForLegendOrText(g, hitKey, rowIdx,
                                                                    anchorGlobalRect);
                        return true;
                    }
                }
            }
        }
        return false;
    }

    if (object != static_cast<QObject *>(plotWidget->canvas()))
        return false;

    switch (e->type()) {
    case QEvent::MouseButtonPress: {
        emit selectPlot();

        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        d_forwarded_resizer = nullptr;

        const Qt::CursorShape curShape = plotWidget->canvas()->cursor().shape();
        if (me->button() == Qt::MiddleButton
            || (me->button() == Qt::LeftButton
                && ((me->modifiers() & Qt::AltModifier) || curShape == Qt::OpenHandCursor
                    || curShape == Qt::ClosedHandCursor))) {
            return false;
        }

        if (me->button() == Qt::LeftButton && g->drawLineActive()) {
            startLinePoint = me->pos();
            return true;
        }

        if (me->button() == Qt::LeftButton && g->drawTextActive()) {
            MultiLayer *ml = g->multiLayerParent();
            const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
            drawTextMarker(me->pos());
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Add Text Annotation"), beforeSnap);
            return true;
        }

        if (!g->zoomOn() && !g->activeTool() && !g->rangeSelectorsEnabled()
            && selectMarker(me)) {
            d_left_click_pending = false;
            if (me->button() == Qt::RightButton)
                emit showMarkerPopupMenu();
            return true;
        }

        if (d_editing_marker) {
            d_editing_marker->setEditable(false);
            d_editing_marker = nullptr;
        }

        if (g->markerSelected())
            g->deselectMarker();

        if (g->zoomOn() || g->activeTool() || g->rangeSelectorsEnabled()) {
            d_left_click_pending = false;
            return !(me->modifiers() & Qt::ShiftModifier);
        }

        int dist = 999;
        int point = -1;
        int curveKey = g->isPiePlot()
                ? -1
                : plotWidget->closestCurve(me->pos().x(), me->pos().y(), dist, point);
        if (curveKey >= 0 && dist < 10) {
            QwtPlotCurve *hitCurve = plotWidget->curve(curveKey);
            if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(hitCurve)) {
                if (DataCurve *mc = er->masterCurve()) {
                    hitCurve = mc;
                    const int mcIdx = g->curveIndex(mc);
                    if (mcIdx >= 0)
                        curveKey = g->curveKey(mcIdx);
                }
            }
            const int cIdx = hitCurve ? g->curveIndex(hitCurve) : g->curveIndex(curveKey);
            const Graph::GraphSelection::Kind kind =
                    (cIdx >= 0 && g->curveType(cIdx) == Graph::ErrorBars)
                    ? Graph::GraphSelection::ErrorBars
                    : Graph::GraphSelection::Curve;
            g->setCurrentSelection({ kind, cIdx, point });
            if (hitCurve && point >= 0 && point < hitCurve->dataSize()) {
                emit g->cursorInfo(QString("%1 [%2]: X=%3, Y=%4")
                                           .arg(hitCurve->title().text())
                                           .arg(point + 1)
                                           .arg(hitCurve->x(point), 0, 'g', 6)
                                           .arg(hitCurve->y(point), 0, 'g', 6));
            }
            if (me->button() == Qt::LeftButton && !(me->modifiers() & Qt::ShiftModifier)) {
                d_press_pos = me->pos();
                d_left_click_pending = true;
            } else {
                d_left_click_pending = false;
            }
        } else {
            d_left_click_pending = false;
            if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                bar->cancelScheduledAndHide();
            if (me->button() == Qt::LeftButton) {
                g->deselect();
                g->setCurrentSelection({ Graph::GraphSelection::Layer, -1, -1 });
            }
        }

        return !(me->modifiers() & Qt::ShiftModifier);
    } break;

    case QEvent::MouseButtonDblClick: {
        d_left_click_pending = false;
        d_forwarded_resizer = nullptr;
        if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
            bar->cancelScheduledAndHide();

        const QMouseEvent *dblMe = static_cast<const QMouseEvent *>(e);
        if (dblMe->button() == Qt::MiddleButton)
            return false;
        if (d_editing_marker) {
            return d_editing_marker->eventFilter(plotWidget->canvas(), e);
        }

        long mrkKey = g->selectedMarkerKey();
        if (mrkKey < 0 || !texts.contains(mrkKey)) {
            for (int k : texts) {
                Legend *m = g->textMarker(k);
                if (m && m->isVisible() && m->rect().contains(dblMe->pos())) {
                    mrkKey = k;
                    break;
                }
            }
        }
        if (mrkKey >= 0) {
            if (texts.contains(mrkKey)) {
                Legend *leg = g->textMarker(mrkKey);
                if (leg && leg->isVisible() && dblMe->button() == Qt::LeftButton) {
                    const int rowIdx =
                            (g->legend() == leg) ? leg->itemIndexAt(dblMe->pos()) : -1;
                    const QRect localRect = leg->rect();
                    const QRect anchorGlobalRect(
                            plotWidget->canvas()->mapToGlobal(localRect.topLeft()),
                            localRect.size());
                    CanvasInlineTextEditor::openForLegendOrText(g, mrkKey, rowIdx,
                                                                anchorGlobalRect);
                    return true;
                }
                emit viewTextDialog();
                return true;
            } else if (lines.contains(mrkKey)) {
                emit viewLineDialog();
                return true;
            } else if (images.contains(mrkKey)) {
                emit viewImageDialog();
                return true;
            }
        }

        if (g->isPiePlot()) {
            emit showPlotDialog(g->curveKey(0));
            return true;
        } else {
            int dist = 999, point = -1;
            int curveKey = plotWidget->closestCurve(dblMe->pos().x(), dblMe->pos().y(), dist, point);
            if (curveKey >= 0 && dist < 10) {
                if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(plotWidget->curve(curveKey))) {
                    if (DataCurve *mc = er->masterCurve()) {
                        const int mcIdx = g->curveIndex(mc);
                        if (mcIdx >= 0)
                            curveKey = g->curveKey(mcIdx);
                    }
                }
                emit showPlotDialog(curveKey);
            } else {
                emit showPlotDialog(-1);
            }
            return true;
        }
    } break;

    case QEvent::MouseMove: {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (d_forwarded_resizer && (me->buttons() & Qt::LeftButton)) {
            const QPoint localPos = d_forwarded_resizer->mapFrom(plotWidget->canvas(), me->pos());
            QMouseEvent mappedMove(QEvent::MouseMove, localPos, me->windowPos(), me->screenPos(),
                                   me->button(), me->buttons(), me->modifiers(), me->source());
            QCoreApplication::sendEvent(d_forwarded_resizer, &mappedMove);
            return true;
        }

        if (me->buttons() == Qt::NoButton) {
            updatePassiveHoverReadout(me->pos());
            return false;
        }

        if ((me->buttons() & Qt::MiddleButton)
            || ((me->buttons() & Qt::LeftButton) && (me->modifiers() & Qt::AltModifier)))
            return false;
        if (me->buttons() != Qt::LeftButton)
            return false;

        const QPoint pos = me->pos();
        if (d_left_click_pending && (pos - d_press_pos).manhattanLength() > 5)
            d_left_click_pending = false;

        if (g->drawLineActive()) {
            drawLineMarker(pos, g->drawArrow());
            return true;
        }

        return false;
    } break;

    case QEvent::MouseButtonRelease: {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (d_forwarded_resizer) {
            QPointer<SelectionMoveResizer> resizer = d_forwarded_resizer;
            d_forwarded_resizer = nullptr;
            d_left_click_pending = false;
            if (resizer) {
                const QPoint prevLegPos = g->legend() ? g->legend()->rect().topLeft() : QPoint();
                const QPoint localPos = resizer->mapFrom(plotWidget->canvas(), me->pos());
                QMouseEvent mappedRel(QEvent::MouseButtonRelease, localPos, me->windowPos(),
                                      me->screenPos(), me->button(), me->buttons(), me->modifiers(),
                                      me->source());
                QCoreApplication::sendEvent(resizer, &mappedRel);
                if (g->legend() && g->legend()->rect().topLeft() != prevLegPos)
                    plotWidget->setProperty("_toriLegendAnchor", QVariant());
            }
            return true;
        }

        if (g->drawLineActive()) {
            MultiLayer *ml = g->multiLayerParent();
            const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();

            ArrowMarker mrk;
            mrk.attach(g->plotWidget());
            mrk.setStartPoint(startLinePoint);
            mrk.setEndPoint(QPoint(me->x(), me->y()));
            mrk.setColor(g->arrowDefaultColor());
            mrk.setWidth(g->arrowDefaultWidth());
            mrk.setStyle(g->arrowLineDefaultStyle());
            mrk.setHeadLength(g->arrowHeadDefaultLength());
            mrk.setHeadAngle(g->arrowHeadDefaultAngle());
            mrk.fillArrowHead(g->arrowHeadDefaultFill());
            mrk.drawEndArrow(g->drawArrow());
            mrk.drawStartArrow(false);

            g->addArrow(&mrk);
            g->drawLine(false);
            mrk.detach();
            plotWidget->replot();
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Add Arrow"), beforeSnap);

            return true;
        }

        if (me->button() == Qt::LeftButton && d_left_click_pending) {
            d_left_click_pending = false;
            if ((me->pos() - d_press_pos).manhattanLength() <= 5
                && !g->zoomOn() && !g->activeTool() && !g->rangeSelectorsEnabled()
                && !(me->modifiers() & Qt::ShiftModifier)
                && !g->markerSelected() && !d_editing_marker && !g->isPiePlot()) {
                GraphMiniToolbar *bar = GraphMiniToolbar::isMiniToolbarEnabled()
                        ? GraphMiniToolbar::instance()
                        : nullptr;
                const Graph::GraphSelection sel = g->currentSelection();
                if (bar
                    && (sel.kind == Graph::GraphSelection::Curve
                        || sel.kind == Graph::GraphSelection::ErrorBars)
                    && sel.primaryId >= 0) {
                    bar->popupForCurve(g, sel.primaryId, me->globalPos());
                }
            }
        }
        return false;
    } break;

    case QEvent::KeyPress: {
        const int key = static_cast<const QKeyEvent *>(e)->key();

        if (key == Qt::Key_Escape) {
            disableEditing();
            g->deselect();
            return true;
        }
        if (key == Qt::Key_Delete) {
            if (g->markerSelected() || d_editing_marker) {
                MultiLayer *ml = g->multiLayerParent();
                const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
                disableEditing();
                g->removeMarker();
                g->setCurrentSelection({ Graph::GraphSelection::None, -1, -1 });
                if (ml && !beforeSnap.isEmpty())
                    ml->pushUndoSnapshot(tr("Delete Annotation"), beforeSnap);
                return true;
            }
        }

        const long selectedMarker = g->selectedMarkerKey();
        if (texts.contains(selectedMarker) && (key == Qt::Key_Enter || key == Qt::Key_Return)) {
            emit viewTextDialog();
            return true;
        }
        if (lines.contains(selectedMarker) && (key == Qt::Key_Enter || key == Qt::Key_Return)) {
            emit viewLineDialog();
            return true;
        }
        if (images.contains(selectedMarker) && (key == Qt::Key_Enter || key == Qt::Key_Return)) {
            emit viewImageDialog();
            return true;
        }
    } break;

    default:
        break;
    }
    return QObject::eventFilter(object, e);
}

void CanvasPicker::disableEditing()
{
    if (d_editing_marker) {
        d_editing_marker->setEditable(false);
        d_editing_marker = nullptr;
    }
}

void CanvasPicker::updatePassiveHoverReadout(const QPoint &canvasPos)
{
    Graph *g = plot();
    if (!g || !plotWidget || g->activeTool() || g->zoomOn() || g->drawLineActive()
        || g->drawTextActive() || g->isPiePlot())
        return;

    const QwtPlotCurve *bestCurve = nullptr;
    int bestPoint = -1;
    double bestDistSq = 14.0 * 14.0;

    const int nCurves = g->curves();
    for (int i = 0; i < nCurves; ++i) {
        if (!g->isCurveVisible(i) || g->curveType(i) == Graph::ErrorBars)
            continue;
        const QwtPlotCurve *c = g->curve(i);
        if (!c || !c->isVisible() || c->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        const int nPts = c->dataSize();
        if (nPts <= 0)
            continue;

        const DataCurve *dc = dynamic_cast<const DataCurve *>(c);
        const QwtScaleMap xMap = plotWidget->canvasMap(c->xAxis());
        const QwtScaleMap yMap = plotWidget->canvasMap(c->yAxis());
        const int stride = (nPts > 20000) ? qMax(1, nPts / 10000) : 1;

        int localBestIdx = -1;
        double localBestDistSq = bestDistSq;
        for (int p = 0; p < nPts; p += stride) {
            if (dc && dc->isPointMasked(p))
                continue;
            const double vx = c->x(p);
            const double vy = c->y(p);
            if (!std::isfinite(vx) || !std::isfinite(vy))
                continue;
            const double dx = xMap.xTransform(vx) - canvasPos.x();
            const double dy = yMap.xTransform(vy) - canvasPos.y();
            const double d2 = dx * dx + dy * dy;
            if (d2 <= localBestDistSq) {
                localBestDistSq = d2;
                localBestIdx = p;
            }
        }
        if (stride > 1 && localBestIdx >= 0) {
            const int p0 = qMax(0, localBestIdx - stride);
            const int p1 = qMin(nPts - 1, localBestIdx + stride);
            for (int p = p0; p <= p1; ++p) {
                if (dc && dc->isPointMasked(p))
                    continue;
                const double vx = c->x(p);
                const double vy = c->y(p);
                if (!std::isfinite(vx) || !std::isfinite(vy))
                    continue;
                const double dx = xMap.xTransform(vx) - canvasPos.x();
                const double dy = yMap.xTransform(vy) - canvasPos.y();
                const double d2 = dx * dx + dy * dy;
                if (d2 <= localBestDistSq) {
                    localBestDistSq = d2;
                    localBestIdx = p;
                }
            }
        }
        if (localBestIdx >= 0 && localBestDistSq <= bestDistSq) {
            bestDistSq = localBestDistSq;
            bestCurve = c;
            bestPoint = localBestIdx;
        }
    }

    if (bestCurve && bestPoint >= 0) {
        emit g->cursorInfo(QString("%1 [%2]: X=%3, Y=%4")
                                   .arg(bestCurve->title().text())
                                   .arg(bestPoint + 1)
                                   .arg(bestCurve->x(bestPoint), 0, 'g', 6)
                                   .arg(bestCurve->y(bestPoint), 0, 'g', 6));
    } else {
        const int xAxis = (!plotWidget->axisEnabled(QwtPlot::xBottom)
                           && plotWidget->axisEnabled(QwtPlot::xTop))
                ? QwtPlot::xTop
                : QwtPlot::xBottom;
        const int yAxis = (!plotWidget->axisEnabled(QwtPlot::yLeft)
                           && plotWidget->axisEnabled(QwtPlot::yRight))
                ? QwtPlot::yRight
                : QwtPlot::yLeft;
        const double vx = plotWidget->invTransform(xAxis, canvasPos.x());
        const double vy = plotWidget->invTransform(yAxis, canvasPos.y());
        emit g->cursorInfo(QString("X=%1, Y=%2").arg(vx, 0, 'g', 6).arg(vy, 0, 'g', 6));
    }
}

void CanvasPicker::drawTextMarker(const QPoint &point)
{
    Legend mrkT(plotWidget);
    mrkT.setOrigin(point);
    mrkT.setFrameStyle(plot()->textMarkerDefaultFrame());
    mrkT.setFont(plot()->defaultTextMarkerFont());
    mrkT.setTextColor(plot()->textMarkerDefaultColor());
    mrkT.setBackgroundColor(plot()->textMarkerDefaultBackground());
    mrkT.setText(tr("enter your text here"));
    plot()->insertTextMarker(&mrkT);
    plot()->drawText(false);
    emit drawTextOff();
}

void CanvasPicker::drawLineMarker(const QPoint &point, bool endArrow)
{
    plot()->plotWidget()->canvas()->repaint();
    ArrowMarker mrk;
    mrk.attach(plotWidget);

    int clw = plotWidget->canvas()->lineWidth();
    mrk.setStartPoint(QPoint(startLinePoint.x() + clw, startLinePoint.y() + clw));
    mrk.setEndPoint(QPoint(point.x() + clw, point.y() + clw));
    mrk.setWidth(1);
    mrk.setStyle(Qt::SolidLine);
    mrk.drawEndArrow(endArrow);
    mrk.drawStartArrow(false);

    if (plot()->drawLineActive())
        mrk.setColor(Qt::black);
    else
        mrk.setColor(Qt::red);

    plotWidget->replot();
    mrk.detach();
}

bool CanvasPicker::selectMarker(const QMouseEvent *e)
{
    Graph *g = plot();
    if (!g || !plotWidget || !plotWidget->canvas())
        return false;

    const QPoint point = e->pos();
    auto forwardPressToResizer = [&](const QMouseEvent *me) {
        if (me->button() != Qt::LeftButton || (me->modifiers() & Qt::ShiftModifier))
            return;
        SelectionMoveResizer *resizer =
                plotWidget->canvas()->findChild<SelectionMoveResizer *>();
        if (!resizer)
            return;
        d_forwarded_resizer = resizer;
        const QPoint localPos = resizer->mapFrom(plotWidget->canvas(), me->pos());
        QMouseEvent mappedPress(QEvent::MouseButtonPress, localPos, me->windowPos(),
                                me->screenPos(), me->button(), me->buttons(), me->modifiers(),
                                me->source());
        QCoreApplication::sendEvent(resizer, &mappedPress);
    };

    for (int i : g->textMarkerKeys()) {
        Legend *m = static_cast<Legend *>(plotWidget->marker(i));
        if (!m || !m->isVisible())
            continue;
        if (m->rect().contains(point)) {
            if (d_editing_marker) {
                d_editing_marker->setEditable(false);
                d_editing_marker = nullptr;
            }
            g->setSelectedMarker(i, e->modifiers() & Qt::ShiftModifier);
            const Graph::GraphSelection::Kind kind = (g->legend() == m)
                    ? Graph::GraphSelection::LegendBox
                    : Graph::GraphSelection::TextAnnotation;
            g->setCurrentSelection({ kind, i, -1 });
            forwardPressToResizer(e);
            return true;
        }
    }
    for (int i : g->imageMarkerKeys()) {
        ImageMarker *m = static_cast<ImageMarker *>(plotWidget->marker(i));
        if (!m || !m->isVisible())
            continue;
        if (m->rect().contains(point)) {
            if (d_editing_marker) {
                d_editing_marker->setEditable(false);
                d_editing_marker = nullptr;
            }
            g->setSelectedMarker(i, e->modifiers() & Qt::ShiftModifier);
            g->setCurrentSelection({ Graph::GraphSelection::ImageAnnotation, i, -1 });
            forwardPressToResizer(e);
            return true;
        }
    }
    constexpr double kPi = 3.141592653589793238462643;
    for (int i : g->lineMarkerKeys()) {
        ArrowMarker *mrkL = static_cast<ArrowMarker *>(plotWidget->marker(i));
        if (!mrkL || !mrkL->isVisible())
            continue;
        const int d = qMax(
                6,
                mrkL->width()
                        + int(std::floor(mrkL->headLength() * std::tan(kPi * mrkL->headAngle() / 180.0)
                                         + 0.5)));
        const double dist = mrkL->dist(point.x(), point.y());
        if (dist <= d) {
            if (d_editing_marker && d_editing_marker != mrkL) {
                d_editing_marker->setEditable(false);
                d_editing_marker = nullptr;
            }
            if (e->modifiers() & Qt::ShiftModifier) {
                g->setSelectedMarker(i, true);
                g->setCurrentSelection({ Graph::GraphSelection::ArrowAnnotation, i, -1 });
                forwardPressToResizer(e);
                return true;
            } else if (e->button() == Qt::RightButton) {
                mrkL->setEditable(false);
                d_editing_marker = nullptr;
                g->setSelectedMarker(i, false);
                g->setCurrentSelection({ Graph::GraphSelection::ArrowAnnotation, i, -1 });
                return true;
            }
            g->deselectMarker();
            mrkL->setEditable(true);
            d_editing_marker = mrkL;
            g->setCurrentSelection({ Graph::GraphSelection::ArrowAnnotation, i, -1 });
            mrkL->eventFilter(plotWidget->canvas(), const_cast<QMouseEvent *>(e));
            return true;
        }
    }
    return false;
}
