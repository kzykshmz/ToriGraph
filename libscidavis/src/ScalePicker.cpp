/***************************************************************************
    File                 : ScalePicker.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Scale picker

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
#include "ScalePicker.h"
#include "CanvasInlineTextEditor.h"
#include "ScaleDraw.h"
#include "Plot.h"
#include "Graph.h"
#include "GraphMiniToolbar.h"
#include "MultiLayer.h"

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>
#include <qwt_text_label.h>

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPen>
#include <QTimer>
#include <QToolTip>
#include <QWheelEvent>

#include <cmath>

ScalePicker::ScalePicker(QwtPlot *plot)
    : QObject(plot),
      d_title_selected(false),
      d_labels_selected(false),
      d_selected_axis(NULL),
      d_current_axis(NULL),
      d_left_click_pending(false),
      d_axis_panning(false)
{
    d_wheel_undo_timer = new QTimer(this);
    d_wheel_undo_timer->setSingleShot(true);
    d_wheel_undo_timer->setInterval(300);
    connect(d_wheel_undo_timer, &QTimer::timeout, this, [this]() { flushPendingWheelUndo(); });
    refresh();
}

ScalePicker::~ScalePicker()
{
    flushPendingWheelUndo();
    cancelInlineScaleEditor();
}

void ScalePicker::flushPendingWheelUndo()
{
    if (d_wheel_undo_timer && d_wheel_undo_timer->isActive())
        d_wheel_undo_timer->stop();
    if (d_pending_wheel_undo_snap.isEmpty())
        return;
    const QString snap = d_pending_wheel_undo_snap;
    d_pending_wheel_undo_snap.clear();
    if (Graph *g = graph()) {
        if (MultiLayer *ml = g->multiLayerParent())
            ml->pushUndoSnapshot(tr("Zoom Axis"), snap);
    }
}

bool ScalePicker::eventFilter(QObject *object, QEvent *e)
{
    if (d_inline_scale_edit && object == d_inline_scale_edit.data()) {
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(e);
            if (ke->key() == Qt::Key_Escape) {
                cancelInlineScaleEditor();
                return true;
            }
            if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
                commitInlineScaleEditor(true);
                return true;
            }
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                commitInlineScaleEditor(false);
                return true;
            }
        } else if (e->type() == QEvent::FocusOut) {
            if (!d_inline_closing && !d_committing_inline_scale)
                finishInlineScaleEditorOnDefocus();
            return false;
        }
        return QObject::eventFilter(object, e);
    }

    if (!object->inherits("QwtScaleWidget"))
        return QObject::eventFilter(object, e);

    QwtScaleWidget *scale = static_cast<QwtScaleWidget *>(object);
    d_current_axis = scale;

    auto resolveAxisIds = [](const QwtScaleWidget *sc, int &axisId, int &twinAxisId) {
        axisId = Plot::scaleAlignmentToAxis(sc->alignment());
        twinAxisId = -1;
        switch (axisId) {
        case QwtPlot::yLeft:
            twinAxisId = QwtPlot::yRight;
            break;
        case QwtPlot::yRight:
            twinAxisId = QwtPlot::yLeft;
            break;
        case QwtPlot::xBottom:
            twinAxisId = QwtPlot::xTop;
            break;
        case QwtPlot::xTop:
            twinAxisId = QwtPlot::xBottom;
            break;
        default:
            break;
        }
    };

    if (e->type() == QEvent::Leave) {
        if (!d_axis_panning)
            scale->unsetCursor();
        return QObject::eventFilter(object, e);
    }

    if (e->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(e);
        const Qt::KeyboardModifiers mods = ke->modifiers();
        if (ke->key() == Qt::Key_Z && (mods & Qt::AltModifier) && !(mods & Qt::ControlModifier)) {
            flushPendingWheelUndo();
            if (Graph *g = graph()) {
                if (g->stepZoomHistory((mods & Qt::ShiftModifier) ? +1 : -1)) {
                    ke->accept();
                    return true;
                }
            }
        }
        if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right || ke->key() == Qt::Key_Up
            || ke->key() == Qt::Key_Down) {
            int step = 2;
            if (mods & Qt::ShiftModifier)
                step = 10;
            else if (mods & (Qt::ControlModifier | Qt::AltModifier))
                step = 1;
            int dx = 0, dy = 0;
            if (ke->key() == Qt::Key_Left)
                dx = -step;
            else if (ke->key() == Qt::Key_Right)
                dx = step;
            else if (ke->key() == Qt::Key_Up)
                dy = -step;
            else if (ke->key() == Qt::Key_Down)
                dy = step;
            if (Graph *g = graph()) {
                if (g->nudgeSelectedElement(dx, dy)) {
                    ke->accept();
                    return true;
                }
            }
        }
        if (ke->key() == Qt::Key_Escape) {
            if (d_axis_panning) {
                d_axis_panning = false;
                d_left_click_pending = false;
                d_press_on_title = false;
                d_pan_undo_snapshot.clear();
                scale->unsetCursor();
                return true;
            }
            if (Graph *g = graph()) {
                if (!g->handleEscapeKey())
                    g->deselect();
            } else {
                deselect();
            }
            return true;
        }
    }

    if (e->type() == QEvent::MouseButtonDblClick) {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (me->button() != Qt::LeftButton)
            return false;
        flushPendingWheelUndo();
        d_left_click_pending = false;
        d_axis_panning = false;
        d_press_on_title = false;
        d_pan_undo_snapshot.clear();
        scale->unsetCursor();
        if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
            bar->hide();
        mouseDblClicked(scale, me->pos());
        return true;
    }

    if (e->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent *>(e);
        if (we->modifiers() & Qt::ControlModifier) {
            we->ignore();
            return false;
        }
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const QPoint pos = we->position().toPoint();
#else
        const QPoint pos = we->pos();
#endif
        if (titleRect(scale).contains(pos)) {
            we->ignore();
            return false;
        }

        int delta = we->angleDelta().y();
        if (delta == 0)
            delta = we->angleDelta().x();
        if (delta != 0) {
            const double factor = (delta > 0) ? 0.85 : (1.0 / 0.85);
            int axisId = -1;
            int twinAxisId = -1;
            resolveAxisIds(scale, axisId, twinAxisId);
            Plot *p = dynamic_cast<Plot *>(plot());
            Graph *g = graph();
            if (p && axisId >= 0) {
                if (!d_wheel_undo_timer->isActive()) {
                    if (g) {
                        g->recordZoomHistorySnapshot();
                        if (MultiLayer *ml = g->multiLayerParent())
                            d_pending_wheel_undo_snap = ml->captureUndoSnapshot();
                    }
                }
                d_wheel_undo_timer->start(300);

                const QPoint canvasPos = p->canvas()
                        ? p->canvas()->mapFromGlobal(scale->mapToGlobal(pos))
                        : pos;
                const int pixelCoord = (axisId == QwtPlot::yLeft || axisId == QwtPlot::yRight)
                        ? canvasPos.y()
                        : canvasPos.x();
                // If user wheels on an unlabeled ISIJ mirror opposite axis (xTop / yRight),
                // drive the primary axis (xBottom / yLeft) and keep both synchronized!
                int targetAxis = axisId;
                if ((axisId == QwtPlot::xTop || axisId == QwtPlot::yRight) && twinAxisId >= 0
                    && p->isMirrorOppositeAxis(twinAxisId, axisId)) {
                    targetAxis = twinAxisId;
                }
                p->zoomAxisAroundPos(targetAxis, pixelCoord, factor);
                if (g) {
                    g->setAxisUserLocked(targetAxis, true);
                    if (twinAxisId >= 0 && p->isMirrorOppositeAxis(targetAxis, twinAxisId))
                        g->setAxisUserLocked(twinAxisId, true);
                    g->enableAutoscaling(false);
                    g->syncChildLinkedLayers();
                    p->replot();
                    g->notifyChanges();
                } else {
                    p->replot();
                }
                we->accept();
                return true;
            }
        }
    }

    if (e->type() == QEvent::MouseButtonPress) {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        const QPoint pos = me->pos();
        const bool onTitle = titleRect(scale).contains(pos);
        if (me->button() == Qt::LeftButton) {
            if (d_inline_scale_edit)
                closeInlineScaleEditor();
            d_press_pos = pos;
            d_pan_last_pos = pos;
            d_axis_panning = false;
            d_press_on_title = onTitle;
            d_pan_undo_snapshot.clear();
            d_left_click_pending = !onTitle && !(me->modifiers() & Qt::ShiftModifier);
            scale->setFocus();
            emit clicked();

            if (onTitle) {
                if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                    bar->hide();
                selectTitle(scale, true);
            } else {
                selectLabels(scale, true);
            }

            return true;
        } else if (me->button() == Qt::MiddleButton) {
            if (onTitle)
                return false;
            d_press_pos = pos;
            d_pan_last_pos = pos;
            d_axis_panning = false;
            d_press_on_title = false;
            d_left_click_pending = false;
            d_pan_undo_snapshot.clear();
            return true;
        } else if (me->button() == Qt::RightButton) {
            d_left_click_pending = false;
            d_axis_panning = false;
            d_press_on_title = false;
            mouseRightClicked(scale, pos);
            return true;
        }
    }

    if (e->type() == QEvent::MouseMove) {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (me->buttons() == Qt::NoButton) {
            const QPoint pos = me->pos();
            if (titleRect(scale).contains(pos)) {
                scale->setCursor(Qt::ArrowCursor);
            } else if (hitTestNumericEndZone(scale, pos) != ScaleEndZone::None
                       && scale->scaleDraw()
                       && scale->scaleDraw()->hasComponent(QwtAbstractScaleDraw::Labels)
                       && !scaleTicksRect(scale).contains(pos)) {
                scale->setCursor(Qt::IBeamCursor);
            } else {
                const bool isHoriz = (scale->alignment() == QwtScaleDraw::BottomScale
                                      || scale->alignment() == QwtScaleDraw::TopScale);
                scale->setCursor(isHoriz ? Qt::SizeHorCursor : Qt::SizeVerCursor);
            }
            return false;
        }

        if (!d_press_on_title && (me->buttons() & (Qt::LeftButton | Qt::MiddleButton))) {
            const int dragThresh = qMax(4, QApplication::startDragDistance());
            if (!d_axis_panning && (me->pos() - d_press_pos).manhattanLength() >= dragThresh) {
                flushPendingWheelUndo();
                d_axis_panning = true;
                d_left_click_pending = false;
                scale->setCursor(Qt::ClosedHandCursor);
                if (Graph *g = graph()) {
                    g->recordZoomHistorySnapshot();
                    if (MultiLayer *ml = g->multiLayerParent())
                        d_pan_undo_snapshot = ml->captureUndoSnapshot();
                }
                if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                    bar->hide();
            }
            if (d_axis_panning) {
                int axisId = -1;
                int twinAxisId = -1;
                resolveAxisIds(scale, axisId, twinAxisId);
                Plot *p = dynamic_cast<Plot *>(plot());
                Graph *g = graph();
                if (p && axisId >= 0) {
                    const QPoint prevCanvasPos = p->canvas()
                            ? p->canvas()->mapFromGlobal(scale->mapToGlobal(d_pan_last_pos))
                            : d_pan_last_pos;
                    const QPoint curCanvasPos = p->canvas()
                            ? p->canvas()->mapFromGlobal(scale->mapToGlobal(me->pos()))
                            : me->pos();
                    const int prevCoord = (axisId == QwtPlot::yLeft || axisId == QwtPlot::yRight)
                            ? prevCanvasPos.y()
                            : prevCanvasPos.x();
                    const int curCoord = (axisId == QwtPlot::yLeft || axisId == QwtPlot::yRight)
                            ? curCanvasPos.y()
                            : curCanvasPos.x();
                    if (prevCoord != curCoord) {
                        int targetAxis = axisId;
                        if ((axisId == QwtPlot::xTop || axisId == QwtPlot::yRight)
                            && twinAxisId >= 0 && p->isMirrorOppositeAxis(twinAxisId, axisId)) {
                            targetAxis = twinAxisId;
                        }
                        p->panAxisByPixels(targetAxis, prevCoord, curCoord);
                        if (g) {
                            g->setAxisUserLocked(targetAxis, true);
                            if (twinAxisId >= 0 && p->isMirrorOppositeAxis(targetAxis, twinAxisId))
                                g->setAxisUserLocked(twinAxisId, true);
                            g->enableAutoscaling(false);
                            g->syncChildLinkedLayers();
                            p->replot();
                            g->notifyChanges();
                        } else {
                            p->replot();
                        }
                    }
                    d_pan_last_pos = me->pos();
                    return true;
                }
            }
        }
    }

    if (e->type() == QEvent::MouseButtonRelease) {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (d_axis_panning
            && (me->button() == Qt::LeftButton || me->button() == Qt::MiddleButton)) {
            d_axis_panning = false;
            d_left_click_pending = false;
            d_press_on_title = false;
            scale->unsetCursor();
            if (Graph *g = graph()) {
                if (MultiLayer *ml = g->multiLayerParent()) {
                    if (!d_pan_undo_snapshot.isEmpty())
                        ml->pushUndoSnapshot(tr("Pan Axis"), d_pan_undo_snapshot, 701);
                }
            }
            d_pan_undo_snapshot.clear();
            return true;
        }
        d_press_on_title = false;
        if (me->button() == Qt::LeftButton && d_left_click_pending) {
            d_left_click_pending = false;
            if ((me->pos() - d_press_pos).manhattanLength() <= 5
                && GraphMiniToolbar::isMiniToolbarEnabled() && graph()) {
                int axisId = -1;
                int twinAxisId = -1;
                resolveAxisIds(scale, axisId, twinAxisId);
                if (axisId >= 0) {
                    if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                        bar->popupForAxis(graph(), axisId, me->globalPos());
                }
            }
        }
    }
    return QObject::eventFilter(object, e);
}

ScalePicker::ScaleEndZone ScalePicker::hitTestNumericEndZone(const QwtScaleWidget *scale,
                                                             const QPoint &pos, int *outAxisId,
                                                             bool *outIsMaxEnd) const
{
    if (outAxisId)
        *outAxisId = -1;
    if (outIsMaxEnd)
        *outIsMaxEnd = false;
    if (!scale || titleRect(scale).contains(pos) || !scaleRect(scale).contains(pos))
        return ScaleEndZone::None;

    const int axisId = Plot::scaleAlignmentToAxis(scale->alignment());
    if (axisId < 0 || axisId >= QwtPlot::axisCnt)
        return ScaleEndZone::None;
    if (outAxisId)
        *outAxisId = axisId;

    Graph *g = const_cast<ScalePicker *>(this)->graph();
    Plot *p = dynamic_cast<Plot *>(const_cast<ScalePicker *>(this)->plot());
    if (!g || !p)
        return ScaleEndZone::None;

    const QList<Graph::AxisType> types = g->axesType();
    if (axisId < types.size() && types[axisId] != Graph::AxisType::Numeric)
        return ScaleEndZone::None;

    const QwtScaleEngine *se = p->axisScaleEngine(axisId);
    const QwtScaleDiv *scDiv = p->axisScaleDiv(axisId);
    const bool inverted = (se && se->testAttribute(QwtScaleEngine::Inverted))
            || (scDiv && scDiv->lowerBound() > scDiv->upperBound());

    const QwtScaleDraw *sd = scale->scaleDraw();
    const bool isHoriz = (scale->alignment() == QwtScaleDraw::BottomScale
                          || scale->alignment() == QwtScaleDraw::TopScale);
    if (isHoriz) {
        const int startPx = sd ? sd->pos().x() : 0;
        const int backboneLen = (sd && sd->length() > 0) ? sd->length() : scale->width();
        if (backboneLen < 36)
            return ScaleEndZone::None;
        const int endZone = qMax(28, qRound(0.22 * double(backboneLen)));
        if (pos.x() <= startPx + endZone) {
            if (outIsMaxEnd)
                *outIsMaxEnd = inverted;
            return ScaleEndZone::LowPixelEnd;
        }
        if (pos.x() >= startPx + backboneLen - endZone) {
            if (outIsMaxEnd)
                *outIsMaxEnd = !inverted;
            return ScaleEndZone::HighPixelEnd;
        }
    } else {
        const int startPx = sd ? sd->pos().y() : 0;
        const int backboneLen = (sd && sd->length() > 0) ? sd->length() : scale->height();
        if (backboneLen < 36)
            return ScaleEndZone::None;
        const int endZone = qMax(28, qRound(0.22 * double(backboneLen)));
        if (pos.y() <= startPx + endZone) {
            if (outIsMaxEnd)
                *outIsMaxEnd = !inverted;
            return ScaleEndZone::LowPixelEnd;
        }
        if (pos.y() >= startPx + backboneLen - endZone) {
            if (outIsMaxEnd)
                *outIsMaxEnd = inverted;
            return ScaleEndZone::HighPixelEnd;
        }
    }
    return ScaleEndZone::None;
}

void ScalePicker::openInlineScaleEditor(QwtScaleWidget *scale, int axisId, bool editMaxEnd)
{
    closeInlineScaleEditor();
    Graph *g = graph();
    Plot *p = dynamic_cast<Plot *>(plot());
    if (!scale || !g || !p || axisId < 0 || axisId >= QwtPlot::axisCnt)
        return;

    const QwtScaleDiv *scDiv = p->axisScaleDiv(axisId);
    const QwtScaleEngine *se = p->axisScaleEngine(axisId);
    if (!scDiv || !se)
        return;

    const bool inverted = se->testAttribute(QwtScaleEngine::Inverted)
            || (scDiv->lowerBound() > scDiv->upperBound());
    const double rawMin = std::min(scDiv->lowerBound(), scDiv->upperBound());
    const double rawMax = std::max(scDiv->lowerBound(), scDiv->upperBound());
    const double targetRaw = editMaxEnd ? rawMax : rawMin;

    const ScaleDraw *scDraw = dynamic_cast<const ScaleDraw *>(scale->scaleDraw());
    const double divFactor = (scDraw && std::isfinite(scDraw->divideByFactor())
                              && std::fabs(scDraw->divideByFactor()) > 1e-300)
            ? scDraw->divideByFactor()
            : 1.0;
    const double displayVal = targetRaw / divFactor;

    QLineEdit *ed = new QLineEdit(scale);
    ed->setObjectName(QStringLiteral("InlineAxisScaleEdit"));
    QFont ef(QStringLiteral("Arial"), qBound(9, scale->font().pointSize(), 14));
    ed->setFont(ef);
    ed->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  font-family: 'Arial';"
            "  background: #FFFFFF;"
            "  color: #111827;"
            "  border: 1.5px solid #2563EB;"
            "  border-radius: 3px;"
            "  padding: 1px 4px;"
            "  selection-background-color: #2563EB;"
            "  selection-color: #FFFFFF;"
            "}"));
    ed->setText(QString::number(displayVal, 'g', 10));

    const bool isHoriz = (scale->alignment() == QwtScaleDraw::BottomScale
                          || scale->alignment() == QwtScaleDraw::TopScale);
    const QwtScaleDraw *sd = scale->scaleDraw();
    const int boxW = qBound(56, qMax(68, ed->fontMetrics().horizontalAdvance(ed->text()) + 20),
                            qMax(68, scale->width()));
    const int boxH = qBound(22, ed->fontMetrics().height() + 6, qMax(22, scale->height()));

    QRect sr = scaleRect(scale).intersected(scale->rect());
    if (!sr.isValid() || sr.isEmpty())
        sr = scale->rect();

    int x = sr.left();
    int y = sr.top();
    if (isHoriz) {
        const int startPx = sd ? sd->pos().x() : sr.left();
        const int len = (sd && sd->length() > 0) ? sd->length() : sr.width();
        const bool atRightEnd = editMaxEnd ? !inverted : inverted;
        x = atRightEnd ? (startPx + len - boxW) : startPx;
        x = qBound(0, x, qMax(0, scale->width() - boxW));
        y = qBound(0, sr.top() + (sr.height() - boxH) / 2, qMax(0, scale->height() - boxH));
    } else {
        const int startPx = sd ? sd->pos().y() : sr.top();
        const int len = (sd && sd->length() > 0) ? sd->length() : sr.height();
        const bool atTopEnd = editMaxEnd ? !inverted : inverted;
        y = atTopEnd ? startPx : (startPx + len - boxH);
        y = qBound(0, y, qMax(0, scale->height() - boxH));
        if (scale->alignment() == QwtScaleDraw::LeftScale)
            x = qBound(0, sr.right() - boxW, qMax(0, scale->width() - boxW));
        else
            x = qBound(0, sr.left(), qMax(0, scale->width() - boxW));
    }

    ed->setGeometry(x, y, qMin(boxW, qMax(44, scale->width())), boxH);
    ed->setProperty("_initialText", ed->text().trimmed());
    ed->installEventFilter(this);

    d_inline_scale_edit = ed;
    d_inline_edit_scale = scale;
    d_inline_edit_axis = axisId;
    d_inline_edit_is_max = editMaxEnd;
    d_inline_closing = false;

    ed->show();
    ed->raise();
    ed->setFocus(Qt::MouseFocusReason);
    ed->selectAll();
}

void ScalePicker::finishInlineScaleEditorOnDefocus()
{
    if (!d_inline_scale_edit || d_inline_closing || d_committing_inline_scale)
        return;
    const QString curTxt = d_inline_scale_edit->text().trimmed();
    const QString initTxt = d_inline_scale_edit->property("_initialText").toString();
    if (curTxt != initTxt) {
        if (!commitInlineScaleEditor(false))
            closeInlineScaleEditor();
    } else {
        closeInlineScaleEditor();
    }
}

bool ScalePicker::commitInlineScaleEditor(bool openOppositeEnd)
{
    if (!d_inline_scale_edit || d_committing_inline_scale)
        return false;

    struct CommitGuard
    {
        bool &flag;
        explicit CommitGuard(bool &f) : flag(f) { flag = true; }
        ~CommitGuard() { flag = false; }
    } commitGuard(d_committing_inline_scale);

    Graph *g = graph();
    if (!g) {
        closeInlineScaleEditor();
        return false;
    }
    MultiLayer *ml = g->multiLayerParent();
    if (!g->isVisible() || (ml && ml->isRestoringSnapshot())) {
        closeInlineScaleEditor();
        return false;
    }

    Plot *p = dynamic_cast<Plot *>(plot());
    QwtScaleWidget *sc = d_inline_edit_scale.data();
    const int axisId = d_inline_edit_axis;
    const bool wasMax = d_inline_edit_is_max;

    if (!p || !sc || axisId < 0 || axisId >= QwtPlot::axisCnt) {
        closeInlineScaleEditor();
        return false;
    }

    const QString txt = d_inline_scale_edit->text().trimmed();
    bool ok = false;
    double enteredDisp = txt.toDouble(&ok);
    if (!ok)
        enteredDisp = QLocale().toDouble(txt, &ok);

    const ScaleDraw *scDraw = dynamic_cast<const ScaleDraw *>(sc->scaleDraw());
    const double divFactor = (scDraw && std::isfinite(scDraw->divideByFactor())
                              && std::fabs(scDraw->divideByFactor()) > 1e-300)
            ? scDraw->divideByFactor()
            : 1.0;
    const double rawNew = enteredDisp * divFactor;

    auto flagError = [this](const QString &msg) {
        if (!d_inline_scale_edit)
            return;
        d_inline_scale_edit->setStyleSheet(QStringLiteral(
                "QLineEdit {"
                "  font-family: 'Arial';"
                "  background: #FEF2F2;"
                "  color: #991B1B;"
                "  border: 1.5px solid #DC2626;"
                "  border-radius: 3px;"
                "  padding: 1px 4px;"
                "}"));
        QToolTip::showText(
                d_inline_scale_edit->mapToGlobal(QPoint(0, d_inline_scale_edit->height() + 2)), msg,
                d_inline_scale_edit);
        d_inline_scale_edit->selectAll();
    };

    if (!ok || !std::isfinite(enteredDisp) || !std::isfinite(rawNew)) {
        flagError(tr("Please enter a valid number."));
        return false;
    }

    const QwtScaleEngine *se = p->axisScaleEngine(axisId);
    const QwtScaleDiv *scDiv = p->axisScaleDiv(axisId);
    if (!se || !scDiv) {
        closeInlineScaleEditor();
        return false;
    }

    const bool isLog = Plot::isLog10ScaleEngine(se);
    if (isLog && rawNew <= 0.0) {
        flagError(tr("Log10 scale requires positive values (> 0)."));
        return false;
    }

    const bool inverted = se->testAttribute(QwtScaleEngine::Inverted)
            || (scDiv->lowerBound() > scDiv->upperBound());
    const double curMin = std::min(scDiv->lowerBound(), scDiv->upperBound());
    const double curMax = std::max(scDiv->lowerBound(), scDiv->upperBound());
    const double candA = wasMax ? curMin : rawNew;
    const double candB = wasMax ? rawNew : curMax;
    if (std::fabs(candA - candB) <= 1e-300) {
        flagError(tr("Axis minimum and maximum must differ."));
        return false;
    }

    const double newMin = std::min(candA, candB);
    const double newMax = std::max(candA, candB);

    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();

    closeInlineScaleEditor();

    g->setScale(axisId, newMin, newMax, g->axisStep(axisId), p->axisMaxMajor(axisId),
                p->axisMaxMinor(axisId), isLog ? 1 : 0, inverted);
    g->setAxisUserLocked(axisId, true);
    g->enableAutoscaling(false);
    g->syncChildLinkedLayers();
    g->notifyChanges();

    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Edit Axis Scale"), beforeSnap);

    if (openOppositeEnd && sc) {
        const QPointer<QwtScaleWidget> scGuard(sc);
        QTimer::singleShot(0, this, [this, scGuard, axisId, wasMax]() {
            Graph *curG = graph();
            MultiLayer *curMl = curG ? curG->multiLayerParent() : nullptr;
            if (scGuard && curG && curG->isVisible() && (!curMl || !curMl->isRestoringSnapshot()))
                openInlineScaleEditor(scGuard.data(), axisId, !wasMax);
        });
    }
    return true;
}

void ScalePicker::cancelInlineScaleEditor()
{
    QToolTip::hideText();
    closeInlineScaleEditor();
}

void ScalePicker::closeInlineScaleEditor()
{
    if (!d_inline_scale_edit || d_inline_closing)
        return;
    d_inline_closing = true;
    QToolTip::hideText();
    QLineEdit *ed = d_inline_scale_edit.data();
    d_inline_scale_edit = nullptr;
    d_inline_edit_scale = nullptr;
    d_inline_edit_axis = -1;
    ed->removeEventFilter(this);
    ed->hide();
    ed->deleteLater();
    d_inline_closing = false;
}

void ScalePicker::mouseDblClicked(const QwtScaleWidget *scale, const QPoint &pos)
{
    Graph *g = graph();
    const int axisId = scale ? Plot::scaleAlignmentToAxis(scale->alignment()) : -1;

    if (scale && titleRect(scale).contains(pos)) {
        if (g && axisId >= 0) {
            QRect trRect = titleRect(scale);
            if (!trRect.isValid() || trRect.isEmpty())
                trRect = scale->rect();
            const QRect globalRect(scale->mapToGlobal(trRect.topLeft()), trRect.size());
            selectTitle(const_cast<QwtScaleWidget *>(scale), true);
            CanvasInlineTextEditor::openForAxisTitle(g, axisId, globalRect);
            return;
        }
        switch (scale->alignment()) {
        case QwtScaleDraw::LeftScale:
            emit yAxisTitleDblClicked();
            break;
        case QwtScaleDraw::RightScale:
            emit rightAxisTitleDblClicked();
            break;
        case QwtScaleDraw::BottomScale:
            emit xAxisTitleDblClicked();
            break;
        case QwtScaleDraw::TopScale:
            emit topAxisTitleDblClicked();
            break;
        }
        return;
    }

    int hitAxisId = -1;
    bool hitMaxEnd = false;
    if (scale && hitTestNumericEndZone(scale, pos, &hitAxisId, &hitMaxEnd) != ScaleEndZone::None
        && hitAxisId >= 0) {
        openInlineScaleEditor(const_cast<QwtScaleWidget *>(scale), hitAxisId, hitMaxEnd);
        return;
    }

    if (scale)
        emit axisDblClicked(scale->alignment());
}

void ScalePicker::mouseRightClicked(const QwtScaleWidget *scale, const QPoint &pos)
{
    emit clicked();

    if (titleRect(scale).contains(pos))
        emit axisTitleRightClicked(scale->alignment());
    else if (scaleRect(scale).contains(pos))
        emit axisRightClicked(scale->alignment());
    else
        emit axisTitleRightClicked(scale->alignment());
}

// The rect of a scale without the title
QRect ScalePicker::scaleRect(const QwtScaleWidget *scale) const
{
    int margin = 1; // pixels tolerance
    QRect rect = scale->rect();
    rect.setRect(rect.x() - margin, rect.y() - margin, rect.width() + 2 * margin,
                 rect.height() + 2 * margin);

    if (scale->title().text().isEmpty())
        return rect;

    const QPointF shift = scale->property("_toriAxisTitleShift").toPointF();
    if (std::hypot(shift.x(), shift.y()) > 1e-4)
        return rect;

    int dh = scale->title().textSize().height();
    switch (scale->alignment()) {
    case QwtScaleDraw::LeftScale: {
        rect.setLeft(rect.left() + dh);
        break;
    }
    case QwtScaleDraw::RightScale: {
        rect.setRight(rect.right() - dh);
        break;
    }
    case QwtScaleDraw::BottomScale: {
        rect.setBottom(rect.bottom() - dh);
        break;
    }
    case QwtScaleDraw::TopScale: {
        rect.setTop(rect.top() + dh);
        break;
    }
    }
    return rect;
}

void ScalePicker::refresh()
{
    for (uint i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)plot()->axisWidget(i);
        if (scale) {
            scale->setMouseTracking(true);
            scale->setFocusPolicy(Qt::ClickFocus);
            scale->installEventFilter(this);
        }
    }
}

QRect ScalePicker::scaleTicksRect(const QwtScaleWidget *scale) const
{
    int majTickLength = scale->scaleDraw()->majTickLength();
    QRect rect = scale->rect();
    switch (scale->alignment()) {
    case QwtScaleDraw::LeftScale:
        rect.setLeft(rect.right() - majTickLength);
        break;
    case QwtScaleDraw::RightScale:
        rect.setRight(rect.left() + majTickLength);
        break;
    case QwtScaleDraw::TopScale:
        rect.setTop(rect.bottom() - majTickLength);
        break;
    case QwtScaleDraw::BottomScale:
        rect.setBottom(rect.top() + majTickLength);
        break;
    }
    return rect;
}

QRect ScalePicker::titleRect(const QwtScaleWidget *scale) const
{
    if (!scale || scale->title().text().isEmpty())
        return QRect();

    QRect rect = scale->rect();
    int margin = scale->margin();
    rect = rect.adjusted(margin, margin, -margin, -margin);

    int dh = scale->title().textSize().height();
    switch (scale->alignment()) {
    case QwtScaleDraw::LeftScale: {
        rect.setRight(rect.left() + dh);
        break;
    }
    case QwtScaleDraw::RightScale: {
        rect.setLeft(rect.right() - dh);
        break;
    }
    case QwtScaleDraw::BottomScale: {
        rect.setTop(rect.bottom() - dh);
        break;
    }
    case QwtScaleDraw::TopScale: {
        rect.setBottom(rect.top() + dh);
        break;
    }
    }

    const QPointF shift = scale->property("_toriAxisTitleShift").toPointF();
    if (std::hypot(shift.x(), shift.y()) > 1e-4) {
        const Plot *plt = qobject_cast<const Plot *>(parent());
        const double zoom = plt ? qMax(0.05, plt->displayFontScale()) : 1.0;
        rect.translate(qRound(shift.x() * zoom), qRound(shift.y() * zoom));
    }
    return rect;
}

void ScalePicker::selectTitle(QwtScaleWidget *scale, bool select)
{
    if (!scale)
        return;

    if (scale->title().backgroundPen().style() != Qt::NoPen) {
        QwtText cleanTitle = scale->title();
        cleanTitle.setBackgroundPen(QPen(Qt::NoPen));
        scale->setTitle(cleanTitle);
    }

    if (d_title_selected == select && d_selected_axis == scale)
        return;

    d_title_selected = select;
    d_selected_axis = select ? scale : nullptr;
    d_labels_selected = false;
    if (select) {
        scale->setFocusPolicy(Qt::ClickFocus);
        scale->setFocus(Qt::MouseFocusReason);
    }

    if (Graph *g = graph()) {
        const int axisId = Plot::scaleAlignmentToAxis(scale->alignment());
        if (select && axisId >= 0) {
            g->setCurrentSelection({ Graph::GraphSelection::AxisTitle, axisId, -1 });
        } else if (!select && g->currentSelection().kind == Graph::GraphSelection::AxisTitle) {
            g->setCurrentSelection({ Graph::GraphSelection::None, -1, -1 });
        }
    }
}

void ScalePicker::selectLabels(QwtScaleWidget *scale, bool select)
{
    if (!scale)
        return;

    if (d_labels_selected == select && d_selected_axis == scale)
        return;

    d_labels_selected = select;
    d_selected_axis = select ? scale : nullptr;
    d_title_selected = false;
    if (select) {
        scale->setFocusPolicy(Qt::ClickFocus);
        scale->setFocus(Qt::MouseFocusReason);
    }

    scale->repaint();

    if (Graph *g = graph()) {
        const int axisId = Plot::scaleAlignmentToAxis(scale->alignment());
        if (select && axisId >= 0) {
            g->setCurrentSelection({ Graph::GraphSelection::Axis, axisId, -1 });
        } else if (!select && g->currentSelection().kind == Graph::GraphSelection::Axis) {
            g->setCurrentSelection({ Graph::GraphSelection::None, -1, -1 });
        }
    }
}

void ScalePicker::deselect()
{
    if (d_inline_scale_edit)
        finishInlineScaleEditorOnDefocus();

    if (!d_selected_axis)
        return;

    QwtScaleWidget *prev = d_selected_axis;
    d_title_selected = false;
    d_labels_selected = false;
    d_selected_axis = nullptr;

    if (prev->title().backgroundPen().style() != Qt::NoPen) {
        QwtText cleanTitle = prev->title();
        cleanTitle.setBackgroundPen(QPen(Qt::NoPen));
        prev->setTitle(cleanTitle);
    }
    prev->repaint();
}
