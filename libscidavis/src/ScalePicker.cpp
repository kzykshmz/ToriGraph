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
#include "ScaleDraw.h"
#include "Plot.h"
#include "Graph.h"
#include "GraphMiniToolbar.h"

#include <qwt_plot.h>
#include <qwt_scale_widget.h>
#include <qwt_text_label.h>

#include <QMouseEvent>
#include <QWheelEvent>
#include <QPen>

ScalePicker::ScalePicker(QwtPlot *plot)
    : QObject(plot),
      d_title_selected(false),
      d_labels_selected(false),
      d_selected_axis(NULL),
      d_current_axis(NULL),
      d_left_click_pending(false)
{
    refresh();
}

bool ScalePicker::eventFilter(QObject *object, QEvent *e)
{
    if (!object->inherits("QwtScaleWidget"))
        return QObject::eventFilter(object, e);

    QwtScaleWidget *scale = (QwtScaleWidget *)object;
    d_current_axis = scale;

    if (e->type() == QEvent::MouseButtonDblClick) {
        d_left_click_pending = false;
        if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
            bar->hide();
        mouseDblClicked(scale, ((QMouseEvent *)e)->pos());
        return true;
    }

    if (e->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent *>(e);
        int delta = we->angleDelta().y();
        if (delta == 0)
            delta = we->angleDelta().x();
        if (delta != 0) {
            const double factor = (delta > 0) ? 0.85 : (1.0 / 0.85);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            const QPoint pos = we->position().toPoint();
#else
            const QPoint pos = we->pos();
#endif
            int axisId = -1;
            int twinAxisId = -1;
            int pixelCoord = 0;
            switch (scale->alignment()) {
            case QwtScaleDraw::LeftScale:
                axisId = QwtPlot::yLeft;
                twinAxisId = QwtPlot::yRight;
                pixelCoord = pos.y();
                break;
            case QwtScaleDraw::RightScale:
                axisId = QwtPlot::yRight;
                twinAxisId = QwtPlot::yLeft;
                pixelCoord = pos.y();
                break;
            case QwtScaleDraw::BottomScale:
                axisId = QwtPlot::xBottom;
                twinAxisId = QwtPlot::xTop;
                pixelCoord = pos.x();
                break;
            case QwtScaleDraw::TopScale:
                axisId = QwtPlot::xTop;
                twinAxisId = QwtPlot::xBottom;
                pixelCoord = pos.x();
                break;
            }
            Plot *p = dynamic_cast<Plot *>(plot());
            if (p && axisId >= 0) {
                bool syncTwin = false;
                const QwtScaleDiv *divMain = p->axisScaleDiv(axisId);
                const QwtScaleDiv *divTwin = p->axisScaleDiv(twinAxisId);
                if (divMain && divTwin
                    && qAbs(divMain->lowerBound() - divTwin->lowerBound()) <= 1e-12
                    && qAbs(divMain->upperBound() - divTwin->upperBound()) <= 1e-12) {
                    syncTwin = true;
                }
                p->zoomAxisAroundPos(axisId, pixelCoord, factor);
                if (syncTwin)
                    p->zoomAxisAroundPos(twinAxisId, pixelCoord, factor);
                p->replot();
                if (graph())
                    graph()->notifyChanges();
                we->accept();
                return true;
            }
        }
    }

    if (e->type() == QEvent::MouseButtonPress) {
        const QMouseEvent *me = (const QMouseEvent *)e;
        QPoint pos = me->pos();
        if (me->button() == Qt::LeftButton) {
            d_press_pos = pos;
            d_left_click_pending = !(me->modifiers() & Qt::ShiftModifier);
            scale->setFocus();
            emit clicked();

            deselect();

            if (titleRect(scale).contains(pos))
                selectTitle(scale);
            else if (!scaleTicksRect(scale).contains(pos))
                selectLabels(scale);

            return !(me->modifiers() & Qt::ShiftModifier) && !scaleTicksRect(scale).contains(pos);
        } else if (me->button() == Qt::RightButton) {
            d_left_click_pending = false;
            mouseRightClicked(scale, pos);
            return true;
        }
    }

    if (e->type() == QEvent::MouseMove) {
        const QMouseEvent *me = (const QMouseEvent *)e;
        if (d_left_click_pending && (me->pos() - d_press_pos).manhattanLength() > 5)
            d_left_click_pending = false;
    }

    if (e->type() == QEvent::MouseButtonRelease) {
        const QMouseEvent *me = (const QMouseEvent *)e;
        if (me->button() == Qt::LeftButton && d_left_click_pending) {
            d_left_click_pending = false;
            if ((me->pos() - d_press_pos).manhattanLength() <= 5
                && GraphMiniToolbar::isMiniToolbarEnabled() && graph()) {
                int axisId = -1;
                switch (scale->alignment()) {
                case QwtScaleDraw::LeftScale:
                    axisId = QwtPlot::yLeft;
                    break;
                case QwtScaleDraw::RightScale:
                    axisId = QwtPlot::yRight;
                    break;
                case QwtScaleDraw::BottomScale:
                    axisId = QwtPlot::xBottom;
                    break;
                case QwtScaleDraw::TopScale:
                    axisId = QwtPlot::xTop;
                    break;
                }
                if (axisId >= 0) {
                    if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
                        bar->popupForAxis(graph(), axisId, me->globalPos());
                }
            }
        }
    }
    return QObject::eventFilter(object, e);
}

void ScalePicker::mouseDblClicked(const QwtScaleWidget *scale, const QPoint &pos)
{
    if (!titleRect(scale).contains(pos))
        emit axisDblClicked(scale->alignment());
    else { // Click on the title
        switch (scale->alignment()) {
        case QwtScaleDraw::LeftScale: {
            emit yAxisTitleDblClicked();
            break;
        }
        case QwtScaleDraw::RightScale: {
            emit rightAxisTitleDblClicked();
            break;
        }
        case QwtScaleDraw::BottomScale: {
            emit xAxisTitleDblClicked();
            break;
        }
        case QwtScaleDraw::TopScale: {
            emit topAxisTitleDblClicked();
            break;
        }
        }
    }
}

void ScalePicker::mouseRightClicked(const QwtScaleWidget *scale, const QPoint &pos)
{
    emit clicked();

    if (scaleRect(scale).contains(pos))
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
        if (scale)
            scale->installEventFilter(this);
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
    if (scale->title().text().isEmpty())
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
    return rect;
}

void ScalePicker::selectTitle(QwtScaleWidget *scale, bool select)
{
    if (!scale)
        return;

    if (d_title_selected == select && d_selected_axis == scale)
        return;

    Graph *g = graph();
    g->deselect();

    d_title_selected = select;
    d_selected_axis = scale;
    d_labels_selected = false;

    QwtText title = scale->title();
    if (select) {
        title.setBackgroundPen(QPen(Qt::blue));
    } else
        title.setBackgroundPen(QPen(Qt::NoPen));

    scale->setTitle(title);
}

void ScalePicker::selectLabels(QwtScaleWidget *scale, bool select)
{
    if (!scale)
        return;

    if (d_labels_selected == select && d_selected_axis == scale)
        return;

    Graph *g = graph();
    g->deselect();

    d_labels_selected = select;
    d_selected_axis = scale;
    d_title_selected = false;

    /*
            ScaleDraw *sc_draw = (ScaleDraw *)scale->scaleDraw();
            sc_draw->setSelected(select);
    */
    scale->repaint();
}

void ScalePicker::deselect()
{
    if (!d_selected_axis)
        return;

    d_title_selected = false;
    d_labels_selected = false;

    QwtText title = d_selected_axis->title();
    title.setBackgroundPen(QPen(Qt::NoPen));
    d_selected_axis->setTitle(title);
    /*
            ScaleDraw *sc_draw = (ScaleDraw *)d_selected_axis->scaleDraw();
            sc_draw->setSelected(false);
    */
    d_selected_axis->repaint();
    d_selected_axis = NULL;
}
