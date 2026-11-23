/***************************************************************************
    File                 : TitlePicker.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Title picker

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
#include "TitlePicker.h"
#include "CanvasInlineTextEditor.h"
#include "Graph.h"
#include "GraphMiniToolbar.h"
#include "MultiLayer.h"

#include <qwt_plot.h>
#include <qwt_text_label.h>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPen>

TitlePicker::TitlePicker(QwtPlot *plot) : QObject(plot)
{
    d_selected = false;
    title = (QwtTextLabel *)plot->titleLabel();
    if (title) {
        title->setFocusPolicy(Qt::StrongFocus);
        title->setMouseTracking(true);
        title->installEventFilter(this);
    }
}

bool TitlePicker::eventFilter(QObject *object, QEvent *e)
{
    if (object != (QObject *)title)
        return false;

    Graph *g = parent() ? qobject_cast<Graph *>(parent()->parent()) : nullptr;

    if (object->inherits("QwtTextLabel") && e->type() == QEvent::MouseButtonDblClick) {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (me->button() != Qt::LeftButton)
            return false;
        setSelected(true);
        if (g && title) {
            QRect trRect = title->textRect();
            if (!trRect.isValid() || trRect.isEmpty())
                trRect = title->rect();
            const QRect globalRect(title->mapToGlobal(trRect.topLeft()), trRect.size());
            CanvasInlineTextEditor::openForPlotTitle(g, globalRect);
        } else {
            emit doubleClicked();
        }
        return true;
    }

    if (object->inherits("QwtTextLabel") && e->type() == QEvent::MouseButtonPress) {
        const QMouseEvent *me = static_cast<const QMouseEvent *>(e);
        if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
            bar->hide();
        if (title)
            title->setFocus();
        setSelected(true);
        emit clicked();

        if (me->button() == Qt::RightButton)
            emit showTitleMenu();
        if (me->button() == Qt::LeftButton || me->button() == Qt::RightButton)
            return true;
        return false;
    }

    if (object->inherits("QwtTextLabel") && e->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(e);
        if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right || ke->key() == Qt::Key_Up
            || ke->key() == Qt::Key_Down) {
            const Qt::KeyboardModifiers mods = ke->modifiers();
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
            if (g && g->nudgeSelectedElement(dx, dy)) {
                ke->accept();
                return true;
            }
        }
        if (ke->key() == Qt::Key_Escape) {
            if (g)
                g->deselect();
            else
                setSelected(false);
            return true;
        }
        if (ke->key() == Qt::Key_Delete) {
            MultiLayer *ml = g ? g->multiLayerParent() : nullptr;
            const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
            emit removeTitle();
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Remove Plot Title"), beforeSnap);
            return true;
        }
    }

    return QObject::eventFilter(object, e);
}

void TitlePicker::setSelected(bool select)
{
    if (!title)
        return;

    if (title->text().backgroundPen().style() != Qt::NoPen) {
        QwtText cleanText = title->text();
        cleanText.setBackgroundPen(QPen(Qt::NoPen));
        if (QwtPlot *plt = qobject_cast<QwtPlot *>(parent()))
            plt->setTitle(cleanText);
    }

    if (d_selected == select)
        return;

    d_selected = select;

    if (Graph *g = parent() ? qobject_cast<Graph *>(parent()->parent()) : nullptr) {
        if (select) {
            if (g->currentSelection().kind != Graph::GraphSelection::PlotTitle)
                g->setCurrentSelection({ Graph::GraphSelection::PlotTitle, -1, -1 });
        } else {
            if (g->currentSelection().kind == Graph::GraphSelection::PlotTitle)
                g->setCurrentSelection({ Graph::GraphSelection::None, -1, -1 });
        }
    }
}
