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
#include "ScaleDraw.h"
#include "MultiLayer.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QRubberBand>
#include <QScreen>
#include <QTimer>
#include <QVector>

#include <cmath>

#include <qwt_plot_canvas.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_symbol.h>
#include <qwt_text_label.h>

namespace {
const Plot *s_lastHoverPlot = nullptr;
QPoint s_lastHoverPos(-99999, -99999);
int s_lastHoverKey = -1;
int s_lastHoverDist = 99999;
int s_lastHoverPt = -1;
} // namespace

namespace {
class PassiveDataHoverTooltip : public QFrame
{
public:
    static PassiveDataHoverTooltip *instance()
    {
        static QPointer<PassiveDataHoverTooltip> s_tip;
        if (!s_tip)
            s_tip = new PassiveDataHoverTooltip();
        return s_tip;
    }

    static void hideIfVisible()
    {
        if (PassiveDataHoverTooltip *tip = instance()) {
            if (tip->isVisible())
                tip->hide();
        }
    }

    void showHtmlAtCursor(const QString &html, const QPoint &globalCursorPos)
    {
        if (html.isEmpty()) {
            hide();
            return;
        }
        d_label->setText(html);
        d_label->adjustSize();
        adjustSize();

        const QSize sz = sizeHint();
        QPoint pos = globalCursorPos + QPoint(14, 16);
        QScreen *scr = QGuiApplication::screenAt(globalCursorPos);
        if (!scr)
            scr = QGuiApplication::primaryScreen();
        if (scr) {
            const QRect avail = scr->availableGeometry();
            if (pos.x() + sz.width() > avail.right() - 4)
                pos.setX(qMax(avail.left() + 4, globalCursorPos.x() - sz.width() - 12));
            if (pos.y() + sz.height() > avail.bottom() - 4)
                pos.setY(qMax(avail.top() + 4, globalCursorPos.y() - sz.height() - 12));
            pos.setX(qBound(avail.left() + 2, pos.x(), qMax(avail.left() + 2, avail.right() - sz.width() - 2)));
            pos.setY(qBound(avail.top() + 2, pos.y(), qMax(avail.top() + 2, avail.bottom() - sz.height() - 2)));
        }
        setGeometry(QRect(pos, sz));
        if (!isVisible())
            show();
        raise();
    }

private:
    PassiveDataHoverTooltip()
        : QFrame(nullptr,
                 Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowTransparentForInput
                         | Qt::WindowDoesNotAcceptFocus)
    {
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_StyledBackground, true);
        setObjectName(QStringLiteral("ToriPassiveDataHoverTooltip"));
        setStyleSheet(QStringLiteral(
                "#ToriPassiveDataHoverTooltip {"
                "  background-color: rgba(24, 28, 36, 238);"
                "  border: 1px solid rgba(148, 163, 184, 120);"
                "  border-radius: 5px;"
                "}"
                "QLabel {"
                "  color: #f8fafc;"
                "  background: transparent;"
                "  border: none;"
                "}"));
        QHBoxLayout *lay = new QHBoxLayout(this);
        lay->setContentsMargins(8, 5, 8, 5);
        lay->setSpacing(0);
        d_label = new QLabel(this);
        d_label->setTextFormat(Qt::RichText);
        QFont f(QStringLiteral("Arial"), 9, QFont::Normal);
        d_label->setFont(f);
        lay->addWidget(d_label);

        if (qApp) {
            QObject::connect(qApp, &QGuiApplication::applicationStateChanged, this,
                             [this](Qt::ApplicationState st) {
                                 if (st != Qt::ApplicationActive && isVisible())
                                     hide();
                             });
        }
    }

    QLabel *d_label = nullptr;
};

bool isAutoGeneratedRefLabel(const QString &lbl)
{
    const QString t = lbl.trimmed();
    if (t.isEmpty())
        return false;
    return t.startsWith(QStringLiteral("Y = ")) || t.startsWith(QStringLiteral("X = "))
            || t.startsWith(QStringLiteral("Y=")) || t.startsWith(QStringLiteral("X="));
}
} // namespace

CanvasPicker::CanvasPicker(Graph *graph) : QObject(graph)
{
    pointSelected = false;
    d_editing_marker = nullptr;
    d_forwarded_resizer = nullptr;
    d_left_click_pending = false;

    plotWidget = graph->plotWidget();

    d_hover_dwell_timer = new QTimer(this);
    d_hover_dwell_timer->setSingleShot(true);
    d_hover_dwell_timer->setInterval(120);
    connect(d_hover_dwell_timer, &QTimer::timeout, this, &CanvasPicker::showPendingHoverTooltip);

    QwtPlotCanvas *canvas = plotWidget->canvas();
    canvas->setMouseTracking(true);
    canvas->installEventFilter(this);
    plotWidget->installEventFilter(this);
}

CanvasPicker::~CanvasPicker()
{
    if (d_hover_dwell_timer)
        d_hover_dwell_timer->stop();
    hidePassiveHoverTooltip();
    if (d_z_marquee_band)
        delete d_z_marquee_band;
}

void CanvasPicker::hidePassiveHoverTooltip()
{
    PassiveDataHoverTooltip::hideIfVisible();
}

bool CanvasPicker::cancelActiveInteraction()
{
    hidePassiveHoverTooltip();
    if (d_hover_dwell_timer)
        d_hover_dwell_timer->stop();
    if (plotWidget)
        plotWidget->clearHoverPoint();

    if (d_z_marquee_active) {
        d_z_marquee_active = false;
        if (d_z_marquee_band)
            d_z_marquee_band->hide();
        if (plotWidget && plotWidget->canvas())
            plotWidget->canvas()->setCursor(Qt::ArrowCursor);
        return true;
    }

    if (d_ref_line_dragging) {
        Graph *g = plot();
        if (g && d_ref_line_moved && d_ref_line_index >= 0
            && d_ref_line_index < g->referenceLines().size()) {
            Graph::ReferenceLineSpec orig = g->referenceLines()[d_ref_line_index];
            orig.value = d_ref_line_orig_value;
            orig.statMode = d_ref_line_orig_stat_mode;
            g->updateReferenceLine(d_ref_line_index, orig);
        }
        d_ref_line_dragging = false;
        d_ref_line_moved = false;
        d_ref_line_index = -1;
        d_ref_line_undo_before.clear();
        if (plotWidget && plotWidget->canvas())
            plotWidget->canvas()->setCursor(Qt::ArrowCursor);
        return true;
    }

    if (d_outer_legend_dragging) {
        Graph *g = plot();
        if (g && d_outer_legend_moved && d_outer_legend_key >= 0) {
            if (Legend *leg = dynamic_cast<Legend *>(plotWidget->marker(d_outer_legend_key))) {
                leg->setOrigin(d_outer_legend_orig_origin);
                plotWidget->replot();
            }
        }
        d_outer_legend_dragging = false;
        d_outer_legend_moved = false;
        d_outer_legend_key = -1;
        d_outer_legend_undo_before.clear();
        return true;
    }

    if (d_editing_marker) {
        disableEditing();
        return true;
    }
    return false;
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

    if (object != static_cast<QObject *>(plotWidget->canvas())) {
        if (e->type() == QEvent::Hide || e->type() == QEvent::WindowDeactivate
            || e->type() == QEvent::FocusOut) {
            if (d_hover_dwell_timer)
                d_hover_dwell_timer->stop();
            hidePassiveHoverTooltip();
            plotWidget->clearHoverPoint();
        }
        return false;
    }

    if (e->type() == QEvent::Leave || e->type() == QEvent::Wheel || e->type() == QEvent::Hide
        || e->type() == QEvent::FocusOut || e->type() == QEvent::WindowDeactivate) {
        if (d_hover_dwell_timer)
            d_hover_dwell_timer->stop();
        d_hover_pending_curve_key = -1;
        d_hover_pending_point = -1;
        hidePassiveHoverTooltip();
        plotWidget->clearHoverPoint();
        if (e->type() == QEvent::Leave && !d_z_marquee_active && !d_ref_line_dragging
            && !g->zoomOn() && !g->activeTool() && !g->drawLineActive() && !g->drawTextActive()) {
            plotWidget->canvas()->setCursor(Qt::ArrowCursor);
        }
        if (e->type() != QEvent::KeyPress)
            return QObject::eventFilter(object, e);
    }

    switch (e->type()) {
    case QEvent::MouseButtonPress: {
        if (d_hover_dwell_timer)
            d_hover_dwell_timer->stop();
        d_hover_pending_curve_key = -1;
        d_hover_pending_point = -1;
        hidePassiveHoverTooltip();
        plotWidget->clearHoverPoint();
        plotWidget->flushPendingWheelUndo();

        emit selectPlot();

        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        d_forwarded_resizer = nullptr;

        const Qt::CursorShape curShape = plotWidget->canvas()->cursor().shape();
        if (me->button() == Qt::MiddleButton
            || (me->button() == Qt::LeftButton && !plotWidget->isZKeyPressed()
                && ((me->modifiers() & Qt::AltModifier) || plotWidget->isSpaceKeyPressed()
                    || curShape == Qt::OpenHandCursor || curShape == Qt::ClosedHandCursor))) {
            return false;
        }

        // OriginPro Key-Chord: Z + Left-Drag rubber-band marquee zoom in Pointer mode
        // without locking into sticky modal btnZoomIn!
        if (me->button() == Qt::LeftButton && plotWidget->isZKeyPressed()
            && !(me->modifiers() & (Qt::ControlModifier | Qt::AltModifier))
            && !g->zoomOn() && !g->activeTool() && !g->rangeSelectorsEnabled()
            && !g->drawLineActive() && !g->drawTextActive() && !g->isPiePlot()
            && !g->isTernaryPlot()) {
            if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                bar->cancelScheduledAndHide();
            d_left_click_pending = false;
            d_z_marquee_active = true;
            d_z_marquee_origin = me->pos();
            if (!d_z_marquee_band)
                d_z_marquee_band = new QRubberBand(QRubberBand::Rectangle, plotWidget->canvas());
            d_z_marquee_band->setGeometry(QRect(d_z_marquee_origin, QSize()));
            d_z_marquee_band->show();
            plotWidget->canvas()->setCursor(Qt::CrossCursor);
            return true;
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

        // Check interactive Reference Lines unless cursor is right on top of a data vertex (< 5 px)
        int vDistCheck = 999;
        int vPtCheck = -1;
        const int vKeyCheck =
                g->isPiePlot() ? -1 : plotWidget->closestDataPoint(me->pos(), vDistCheck, vPtCheck);
        const int refHitIdx =
                (vKeyCheck >= 0 && vDistCheck < 5) ? -1 : g->referenceLineAtCanvasPos(me->pos(), 6);
        if (refHitIdx >= 0) {
            d_left_click_pending = false;
            if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                bar->cancelScheduledAndHide();
            g->setCurrentSelection({ Graph::GraphSelection::ReferenceLine, refHitIdx, -1 });
            const QVector<Graph::ReferenceLineSpec> refs = g->referenceLines();
            const Graph::ReferenceLineSpec &rspec = refs[refHitIdx];
            if (me->button() == Qt::LeftButton) {
                d_ref_line_dragging = true;
                d_ref_line_moved = false;
                d_ref_line_index = refHitIdx;
                d_ref_line_orig_value = rspec.value;
                d_ref_line_orig_stat_mode = rspec.statMode;
                d_ref_line_undo_before.clear();
                plotWidget->canvas()->setCursor(rspec.orientation == 0 ? Qt::SplitVCursor
                                                                       : Qt::SplitHCursor);
                return true;
            }
            if (me->button() == Qt::RightButton) {
                QMenu refMenu(plotWidget->canvas());
                QAction *editAct = refMenu.addAction(tr("&Edit Reference Line..."));
                QAction *delAct = refMenu.addAction(tr("&Delete Reference Line"));
                QAction *picked = refMenu.exec(me->globalPos());
                if (picked == editAct)
                    g->editReferenceLineDialog(refHitIdx);
                else if (picked == delAct)
                    g->removeReferenceLine(refHitIdx);
                return true;
            }
        }

        int dist = 999;
        int point = -1;
        int curveKey = g->isPiePlot()
                ? -1
                : plotWidget->closestCurveInteractive(me->pos(), dist, point);
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
                const DataCurve *dc = dynamic_cast<const DataCurve *>(hitCurve);
                const int rowOneBased = (dc ? dc->tableRow(point) : point) + 1;
                QString dispLabel = (cIdx >= 0) ? g->resolvedAutoCurveLabel(cIdx) : QString();
                if (dispLabel.isEmpty())
                    dispLabel = hitCurve->title().text();
                emit g->cursorInfo(QString("%1 [%2]: X=%3, Y=%4")
                                           .arg(dispLabel)
                                           .arg(rowOneBased)
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
        if (d_hover_dwell_timer)
            d_hover_dwell_timer->stop();
        hidePassiveHoverTooltip();
        plotWidget->clearHoverPoint();
        d_left_click_pending = false;
        d_ref_line_dragging = false;
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

        if (dblMe->button() == Qt::LeftButton && !g->isPiePlot()) {
            const int refIdx = g->referenceLineAtCanvasPos(dblMe->pos(), 6);
            if (refIdx >= 0) {
                g->setCurrentSelection({ Graph::GraphSelection::ReferenceLine, refIdx, -1 });
                g->editReferenceLineDialog(refIdx);
                return true;
            }
        }

        if (g->isPiePlot()) {
            emit showPlotDialog(g->curveKey(0));
            return true;
        } else {
            int dist = 999, point = -1;
            int curveKey = plotWidget->closestCurveInteractive(dblMe->pos(), dist, point);
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
        if (d_z_marquee_active) {
            if (!(me->buttons() & Qt::LeftButton)) {
                d_z_marquee_active = false;
                if (d_z_marquee_band)
                    d_z_marquee_band->hide();
                return false;
            }
            if (d_z_marquee_band) {
                const QRect box = QRect(d_z_marquee_origin, me->pos())
                                          .normalized()
                                          .intersected(plotWidget->canvas()->rect());
                d_z_marquee_band->setGeometry(box);
            }
            return true;
        }

        if (d_ref_line_dragging) {
            if (!(me->buttons() & Qt::LeftButton) || d_ref_line_index < 0
                || d_ref_line_index >= g->referenceLines().size()) {
                d_ref_line_dragging = false;
                d_ref_line_moved = false;
                d_ref_line_index = -1;
                d_ref_line_undo_before.clear();
                hidePassiveHoverTooltip();
                return false;
            }
            const QRect cr = plotWidget->canvas()->rect();
            const QPoint clampedPos(qBound(cr.left(), me->pos().x(), cr.right()),
                                    qBound(cr.top(), me->pos().y(), cr.bottom()));
            if (!d_ref_line_moved) {
                const int dragThresh = qMax(3, QApplication::startDragDistance());
                if ((me->pos() - d_press_pos).manhattanLength() < dragThresh
                    && d_press_pos != QPoint())
                    return true;
                d_ref_line_moved = true;
                if (MultiLayer *ml = g->multiLayerParent())
                    d_ref_line_undo_before = ml->captureUndoSnapshot();
            }
            Graph::ReferenceLineSpec spec = g->referenceLines()[d_ref_line_index];
            const int mapAxis = (spec.orientation == 0) ? QwtPlot::yLeft : QwtPlot::xBottom;
            const double newVal = plotWidget->canvasMap(mapAxis).invTransform(
                    spec.orientation == 0 ? clampedPos.y() : clampedPos.x());
            if (std::isfinite(newVal)) {
                spec.value = newVal;
                spec.statMode.clear();
                if (isAutoGeneratedRefLabel(spec.label)) {
                    spec.label = QStringLiteral("%1 = %2")
                                         .arg(spec.orientation == 0 ? QStringLiteral("Y")
                                                                    : QStringLiteral("X"))
                                         .arg(QLocale::c().toString(newVal, 'g', 4));
                }
                g->updateReferenceLine(d_ref_line_index, spec);
                const QString axisTag =
                        (spec.orientation == 0) ? QStringLiteral("Y") : QStringLiteral("X");
                const QString valStr = QLocale::c().toString(newVal, 'g', 6);
                emit g->cursorInfo(QStringLiteral("%1 = %2").arg(axisTag, valStr));
                const QString tipHtml =
                        QStringLiteral("<span style='color:%1;'>&#9679;</span> <b>%2 = %3</b>")
                                .arg(spec.color.name(), axisTag, valStr);
                PassiveDataHoverTooltip::instance()->showHtmlAtCursor(tipHtml, me->globalPos());
            }
            return true;
        }

        if (d_forwarded_resizer && (me->buttons() & Qt::LeftButton)) {
            const QPoint localPos = d_forwarded_resizer->mapFrom(plotWidget->canvas(), me->pos());
            QMouseEvent mappedMove(QEvent::MouseMove, localPos, me->windowPos(), me->screenPos(),
                                   me->button(), me->buttons(), me->modifiers(), me->source());
            QCoreApplication::sendEvent(d_forwarded_resizer, &mappedMove);
            return true;
        }

        if (me->buttons() == Qt::NoButton) {
            updatePassiveHoverReadout(me->pos());
            updateHoverCursor(me->pos());
            s_lastHoverPlot = nullptr;
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
        if (d_z_marquee_active && me->button() == Qt::LeftButton) {
            d_z_marquee_active = false;
            if (d_z_marquee_band)
                d_z_marquee_band->hide();
            const QRect cr = plotWidget->canvas()->rect();
            const QRect zoomBox = QRect(d_z_marquee_origin, me->pos()).normalized().intersected(cr);
            if (zoomBox.width() >= 6 && zoomBox.height() >= 6) {
                MultiLayer *ml = g->multiLayerParent();
                const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
                g->recordZoomHistorySnapshot();

                auto applyAxisPixelSpan = [&](int axis, int pStart, int pEnd) {
                    if (axis < 0 || axis >= QwtPlot::axisCnt)
                        return;
                    QwtScaleEngine *se = plotWidget->axisScaleEngine(axis);
                    const QwtScaleDiv *scDiv = plotWidget->axisScaleDiv(axis);
                    if (!se || !scDiv)
                        return;
                    const QwtScaleMap map = plotWidget->canvasMap(axis);
                    const double v1 = map.invTransform(pStart);
                    const double v2 = map.invTransform(pEnd);
                    if (!std::isfinite(v1) || !std::isfinite(v2))
                        return;
                    const double newMin = qMin(v1, v2);
                    const double newMax = qMax(v1, v2);
                    if (newMax - newMin <= 1e-15)
                        return;
                    const bool hasBreak = g->axisBreak(axis).enabled;
                    const Graph::AxisBreakSpec brk = g->axisBreak(axis);
                    const bool isLog = hasBreak ? (brk.log10Before && brk.log10After)
                                                : Plot::isLog10ScaleEngine(se);
                    if (isLog && (newMin <= 0.0 || newMax <= 0.0))
                        return;
                    const bool inverted = se->testAttribute(QwtScaleEngine::Inverted)
                            || (scDiv->lowerBound() > scDiv->upperBound());
                    double effStep = 0.0;
                    if (!isLog) {
                        const double userStep = g->axisStep(axis);
                        const double nMaj = (userStep > 0.0) ? ((newMax - newMin) / userStep) : 0.0;
                        if (userStep > 0.0 && nMaj >= 1.5 && nMaj <= 25.0)
                            effStep = userStep;
                    }
                    int maxMajor = qMax(plotWidget->axisMaxMajor(axis), 2);
                    if (hasBreak && g->axisFont(axis).pointSize() >= 20)
                        maxMajor = qMin(maxMajor, 5);
                    const int minorTicks = plotWidget->axisMaxMinor(axis);
                    const int maxMinIntervals =
                            (minorTicks == 1) ? (hasBreak ? 2 : 3)
                                              : ((minorTicks > 1) ? (minorTicks + 1) : 0);
                    QwtScaleDiv div =
                            se->divideScale(newMin, newMax, maxMajor, maxMinIntervals, effStep);
                    if (inverted) {
                        se->setAttribute(QwtScaleEngine::Inverted);
                        if (div.lowerBound() < div.upperBound())
                            div.invert();
                    }
                    plotWidget->setAxisScaleDiv(axis, div);
                    if (hasBreak) {
                        const double bf = qMin(brk.breakFrom, brk.breakTo);
                        const double bt = qMax(brk.breakFrom, brk.breakTo);
                        if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(plotWidget->axisScaleDraw(axis)))
                            sd->setAxisBreakIndicator(bf > newMin && bt < newMax && bt > bf + 1e-15, bf);
                    }
                    g->setAxisUserLocked(axis, true);
                    plotWidget->syncMirrorAxisIfNeeded(axis);
                };

                applyAxisPixelSpan(QwtPlot::xBottom, zoomBox.left(), zoomBox.right());
                if (plotWidget->hasVisibleItemOnAxis(QwtPlot::xTop))
                    applyAxisPixelSpan(QwtPlot::xTop, zoomBox.left(), zoomBox.right());
                applyAxisPixelSpan(QwtPlot::yLeft, zoomBox.bottom(), zoomBox.top());
                if (plotWidget->hasVisibleItemOnAxis(QwtPlot::yRight))
                    applyAxisPixelSpan(QwtPlot::yRight, zoomBox.bottom(), zoomBox.top());

                g->enableAutoscaling(false);
                g->syncChildLinkedLayers();
                plotWidget->replot();
                g->notifyChanges();
                if (ml && !beforeSnap.isEmpty())
                    ml->pushUndoSnapshot(tr("Zoom Marquee"), beforeSnap);
            }
            plotWidget->canvas()->setCursor(plotWidget->isZKeyPressed() ? Qt::CrossCursor
                                                                        : Qt::ArrowCursor);
            return true;
        }

        if (d_ref_line_dragging && me->button() == Qt::LeftButton) {
            const bool moved = d_ref_line_moved;
            const QString beforeSnap = d_ref_line_undo_before;
            d_ref_line_dragging = false;
            d_ref_line_moved = false;
            d_ref_line_index = -1;
            d_ref_line_undo_before.clear();
            hidePassiveHoverTooltip();
            if (moved) {
                if (MultiLayer *ml = g->multiLayerParent()) {
                    if (!beforeSnap.isEmpty())
                        ml->pushUndoSnapshot(tr("Move Reference Line"), beforeSnap);
                }
            }
            updateHoverCursor(me->pos());
            return true;
        }

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
        if (d_hover_dwell_timer)
            d_hover_dwell_timer->stop();
        hidePassiveHoverTooltip();
        plotWidget->clearHoverPoint();

        const QKeyEvent *ke = static_cast<const QKeyEvent *>(e);
        const int key = ke->key();
        const Qt::KeyboardModifiers mods = ke->modifiers();

        if (key == Qt::Key_Escape) {
            if (cancelActiveInteraction())
                return true;
            if (g->handleEscapeKey())
                return true;
            disableEditing();
            g->deselect();
            return true;
        }

        const Graph::GraphSelection curSel = g->currentSelection();
        if (key == Qt::Key_Delete) {
            if (curSel.kind == Graph::GraphSelection::ReferenceLine
                && curSel.primaryId >= 0 && curSel.primaryId < g->referenceLines().size()) {
                g->removeReferenceLine(curSel.primaryId);
                return true;
            }
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

        if (curSel.kind == Graph::GraphSelection::ReferenceLine && curSel.primaryId >= 0
            && curSel.primaryId < g->referenceLines().size()) {
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                g->editReferenceLineDialog(curSel.primaryId);
                return true;
            }
            const int stepPx = (mods & Qt::ShiftModifier)
                    ? 10
                    : ((mods & (Qt::ControlModifier | Qt::AltModifier)) ? 1 : 2);
            Graph::ReferenceLineSpec rspec = g->referenceLines()[curSel.primaryId];
            int dPx = 0;
            if (rspec.orientation == 0 && (key == Qt::Key_Up || key == Qt::Key_Down))
                dPx = (key == Qt::Key_Up) ? -stepPx : stepPx;
            else if (rspec.orientation == 1 && (key == Qt::Key_Left || key == Qt::Key_Right))
                dPx = (key == Qt::Key_Left) ? -stepPx : stepPx;
            if (dPx != 0) {
                const int mapAxis = (rspec.orientation == 0) ? QwtPlot::yLeft : QwtPlot::xBottom;
                const QwtScaleMap map = plotWidget->canvasMap(mapAxis);
                const double curPx = map.transform(rspec.value);
                const double nextVal = map.invTransform(curPx + double(dPx));
                if (std::isfinite(nextVal)) {
                    MultiLayer *ml = g->multiLayerParent();
                    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
                    rspec.value = nextVal;
                    rspec.statMode.clear();
                    if (isAutoGeneratedRefLabel(rspec.label)) {
                        rspec.label = QStringLiteral("%1 = %2")
                                              .arg(rspec.orientation == 0 ? QStringLiteral("Y")
                                                                          : QStringLiteral("X"))
                                              .arg(QLocale::c().toString(nextVal, 'g', 4));
                    }
                    g->updateReferenceLine(curSel.primaryId, rspec);
                    if (ml && !beforeSnap.isEmpty())
                        ml->pushUndoSnapshot(tr("Move Reference Line"), beforeSnap, 709);
                }
                return true;
            }
        }

        if (d_editing_marker
            && (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up
                || key == Qt::Key_Down)) {
            if (d_editing_marker->eventFilter(plotWidget->canvas(), e))
                return true;
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

void CanvasPicker::updateHoverCursor(const QPoint &canvasPos)
{
    Graph *g = plot();
    if (!g || !plotWidget || !plotWidget->canvas())
        return;
    if (g->zoomOn() || g->activeTool() || g->rangeSelectorsEnabled() || g->drawLineActive()
        || g->drawTextActive()) {
        return;
    }
    QwtPlotCanvas *cv = plotWidget->canvas();
    if (plotWidget->isSpaceKeyPressed()) {
        cv->setCursor(Qt::OpenHandCursor);
        return;
    }
    if (plotWidget->isZKeyPressed()) {
        cv->setCursor(Qt::CrossCursor);
        return;
    }

    // Draggable Legend / text / image / arrow annotations
    for (int id : g->textMarkerKeys()) {
        Legend *m = g->textMarker(id);
        if (m && m->isVisible() && m->rect().contains(canvasPos)) {
            cv->setCursor(Qt::SizeAllCursor);
            return;
        }
    }
    for (int id : g->imageMarkerKeys()) {
        ImageMarker *im = g->imageMarker(id);
        if (im && im->isVisible() && im->rect().contains(canvasPos)) {
            cv->setCursor(Qt::SizeAllCursor);
            return;
        }
    }
    constexpr double kPi = 3.141592653589793238462643;
    for (int id : g->lineMarkerKeys()) {
        ArrowMarker *am = g->arrow(id);
        if (!am || !am->isVisible())
            continue;
        const int tol = qMax(
                6,
                am->width()
                        + int(std::floor(am->headLength() * std::tan(kPi * am->headAngle() / 180.0)
                                         + 0.5)));
        if (am->dist(canvasPos.x(), canvasPos.y()) <= tol) {
            cv->setCursor(Qt::SizeAllCursor);
            return;
        }
    }

    // Reference lines vs curves
    int vDist = 999;
    int vPt = -1;
    int vKey = -1;
    if (!g->isPiePlot()) {
        if (s_lastHoverPlot == plotWidget && s_lastHoverPos == canvasPos) {
            vKey = s_lastHoverKey;
            vDist = s_lastHoverDist;
            vPt = s_lastHoverPt;
        } else {
            vKey = plotWidget->closestDataPoint(canvasPos, vDist, vPt);
        }
    }
    if (!(vKey >= 0 && vDist < 5)) {
        const int refIdx = g->referenceLineAtCanvasPos(canvasPos, 6);
        if (refIdx >= 0 && refIdx < g->referenceLines().size()) {
            const int orient = g->referenceLines()[refIdx].orientation;
            cv->setCursor(orient == 0 ? Qt::SplitVCursor : Qt::SplitHCursor);
            return;
        }
    }

    if (!g->isPiePlot()) {
        if (vKey >= 0 && vDist < 10) {
            cv->setCursor(Qt::PointingHandCursor);
            return;
        }
        int cDist = 999;
        int cPt = -1;
        const int cKey = plotWidget->closestCurveInteractive(canvasPos, cDist, cPt);
        if (cKey >= 0 && cDist < 10) {
            cv->setCursor(Qt::PointingHandCursor);
            return;
        }
    }

    cv->setCursor(Qt::ArrowCursor);
}

void CanvasPicker::showPendingHoverTooltip()
{
    Graph *g = plot();
    if (!g || !plotWidget || !plotWidget->canvas() || !plotWidget->canvas()->isVisible()
        || g->zoomOn() || g->activeTool() || g->rangeSelectorsEnabled() || g->drawLineActive()
        || g->drawTextActive() || g->isPiePlot() || QApplication::mouseButtons() != Qt::NoButton
        || plotWidget->isZKeyPressed() || plotWidget->isSpaceKeyPressed()) {
        hidePassiveHoverTooltip();
        return;
    }

    if (d_hover_pending_curve_key < 0 || d_hover_pending_point < 0) {
        hidePassiveHoverTooltip();
        return;
    }

    QwtPlotCurve *c = plotWidget->curve(d_hover_pending_curve_key);
    if (!c || !c->isVisible() || d_hover_pending_point >= c->dataSize()) {
        hidePassiveHoverTooltip();
        return;
    }

    const DataCurve *dc = dynamic_cast<const DataCurve *>(c);
    if (dc && dc->isPointMasked(d_hover_pending_point)) {
        hidePassiveHoverTooltip();
        return;
    }

    const int cIdx = g->curveIndex(c);
    QString curveLabel = (cIdx >= 0) ? g->resolvedAutoCurveLabel(cIdx) : QString();
    if (curveLabel.isEmpty())
        curveLabel = c->title().text().trimmed();

    QColor swatch = c->pen().color();
    if (!swatch.isValid() || swatch.alpha() == 0)
        swatch = c->symbol().pen().color();
    if (!swatch.isValid() || swatch.alpha() == 0)
        swatch = QColor(30, 102, 184);

    const int rowOneBased =
            (dc ? dc->tableRow(d_hover_pending_point) : d_hover_pending_point) + 1;
    const double xv = c->x(d_hover_pending_point);
    const double yv = c->y(d_hover_pending_point);

    QString xStr = QLocale::c().toString(xv, 'g', 6);
    QString yStr = QLocale::c().toString(yv, 'g', 6);

    if (dc) {
        const QList<DataCurve *> errList = const_cast<DataCurve *>(dc)->errorBarsList();
        for (DataCurve *eb : errList) {
            QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(eb);
            if (!er || !er->isVisible() || d_hover_pending_point >= er->dataSize())
                continue;
            const double ev = std::fabs(er->errorValue(d_hover_pending_point));
            if (!std::isfinite(ev) || ev <= 0.0)
                continue;
            const QString errTxt =
                    QStringLiteral(" &plusmn; %1").arg(QLocale::c().toString(ev, 'g', 4));
            if (er->direction() == QwtErrorPlotCurve::Vertical)
                yStr += errTxt;
            else
                xStr += errTxt;
        }
    }

    const QString html = QStringLiteral(
                                 "<span style='color:%1;'>&#9679;</span> <b>%2</b><br>"
                                 "<span style='color:#cbd5e1;'>[%3 %4]</span>"
                                 "&nbsp;&nbsp;X = %5,&nbsp;&nbsp;Y = %6")
                                 .arg(swatch.name(), curveLabel.toHtmlEscaped(), tr("Row"),
                                      QString::number(rowOneBased), xStr, yStr);

    const QPoint globalCursor = QCursor::pos();
    const QPoint localCursor = plotWidget->canvas()->mapFromGlobal(globalCursor);
    if (!plotWidget->canvas()->rect().adjusted(-4, -4, 4, 4).contains(localCursor)) {
        hidePassiveHoverTooltip();
        return;
    }
    PassiveDataHoverTooltip::instance()->showHtmlAtCursor(html, globalCursor);
}

void CanvasPicker::updatePassiveHoverReadout(const QPoint &canvasPos)
{
    Graph *g = plot();
    if (!g || !plotWidget || g->activeTool() || g->zoomOn() || g->rangeSelectorsEnabled()
        || g->drawLineActive() || g->drawTextActive() || g->isPiePlot()) {
        if (d_hover_dwell_timer)
            d_hover_dwell_timer->stop();
        hidePassiveHoverTooltip();
        if (plotWidget)
            plotWidget->clearHoverPoint();
        return;
    }

    d_hover_last_canvas_pos = canvasPos;
    int vDist = 99999;
    int bestPoint = -1;
    const int bestKey = plotWidget->closestDataPoint(canvasPos, vDist, bestPoint);
    s_lastHoverPlot = plotWidget;
    s_lastHoverPos = canvasPos;
    s_lastHoverKey = bestKey;
    s_lastHoverDist = vDist;
    s_lastHoverPt = bestPoint;
    const QwtPlotCurve *bestCurve = (bestKey >= 0) ? plotWidget->curve(bestKey) : nullptr;

    if (bestCurve && bestPoint >= 0 && vDist <= 14) {
        const DataCurve *dc = dynamic_cast<const DataCurve *>(bestCurve);
        const int rowOneBased = (dc ? dc->tableRow(bestPoint) : bestPoint) + 1;
        const int cIdx = g->curveIndex(const_cast<QwtPlotCurve *>(bestCurve));
        QString dispLabel = (cIdx >= 0) ? g->resolvedAutoCurveLabel(cIdx) : QString();
        if (dispLabel.isEmpty())
            dispLabel = bestCurve->title().text();
        emit g->cursorInfo(QString("%1 [%2]: X=%3, Y=%4")
                                   .arg(dispLabel)
                                   .arg(rowOneBased)
                                   .arg(bestCurve->x(bestPoint), 0, 'g', 6)
                                   .arg(bestCurve->y(bestPoint), 0, 'g', 6));

        // Passive tooltip + subtle hover ring activate within 10 px after 120 ms dwell
        if (vDist < 10 && QApplication::mouseButtons() == Qt::NoButton
            && !plotWidget->isZKeyPressed() && !plotWidget->isSpaceKeyPressed()) {
            plotWidget->setHoverPoint(bestKey, bestPoint);
            if (d_hover_pending_curve_key != bestKey || d_hover_pending_point != bestPoint
                || !PassiveDataHoverTooltip::instance()->isVisible()) {
                d_hover_pending_curve_key = bestKey;
                d_hover_pending_point = bestPoint;
                if (PassiveDataHoverTooltip::instance()->isVisible())
                    showPendingHoverTooltip();
                else if (d_hover_dwell_timer)
                    d_hover_dwell_timer->start(120);
            }
        } else {
            if (d_hover_dwell_timer)
                d_hover_dwell_timer->stop();
            d_hover_pending_curve_key = -1;
            d_hover_pending_point = -1;
            hidePassiveHoverTooltip();
            plotWidget->clearHoverPoint();
        }
    } else {
        if (d_hover_dwell_timer)
            d_hover_dwell_timer->stop();
        d_hover_pending_curve_key = -1;
        d_hover_pending_point = -1;
        hidePassiveHoverTooltip();
        plotWidget->clearHoverPoint();

        int xAxis = (!plotWidget->axisEnabled(QwtPlot::xBottom)
                     && plotWidget->axisEnabled(QwtPlot::xTop))
                ? QwtPlot::xTop
                : QwtPlot::xBottom;
        int yAxis = (!plotWidget->axisEnabled(QwtPlot::yLeft)
                     && plotWidget->axisEnabled(QwtPlot::yRight))
                ? QwtPlot::yRight
                : QwtPlot::yLeft;

        // Use selected curve's (xAxis, yAxis) if a curve is currently selected
        const Graph::GraphSelection sel = g->currentSelection();
        if ((sel.kind == Graph::GraphSelection::Curve
             || sel.kind == Graph::GraphSelection::ErrorBars)
            && sel.primaryId >= 0 && sel.primaryId < g->curves()) {
            if (const QwtPlotItem *selIt = g->plotItem(sel.primaryId)) {
                xAxis = selIt->xAxis();
                yAxis = selIt->yAxis();
            }
        }

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
