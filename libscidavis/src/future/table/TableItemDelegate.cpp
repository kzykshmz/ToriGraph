/***************************************************************************
    File                 : TableItemDelegate.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Tilman Benkert,
    Email (use @ for *)  : thzs*gmx.net
    Description          : Item delegate for TableView

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

#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMetaObject>
#include <QPainter>
#include <QModelIndex>
#include <QTableView>
#include <QWidget>
#include "TableItemDelegate.h"
#include "TableModel.h"

namespace {
QTableView *findParentTableView(QObject *delegateParent, QWidget *editor)
{
    if (QTableView *tv = qobject_cast<QTableView *>(delegateParent))
        return tv;
    for (QWidget *w = editor ? editor->parentWidget() : nullptr; w; w = w->parentWidget()) {
        if (QTableView *tv = qobject_cast<QTableView *>(w))
            return tv;
    }
    return nullptr;
}

QModelIndex stepCellHorizontally(QTableView *view, int curRow, int curCol, bool backward)
{
    QAbstractItemModel *m = view ? view->model() : nullptr;
    if (!m || curRow < 0 || curCol < 0)
        return QModelIndex();
    const int rows = m->rowCount();
    const int cols = m->columnCount();
    if (rows <= 0 || cols <= 0)
        return QModelIndex();
    QHeaderView *hh = view->horizontalHeader();
    int r = qBound(0, curRow, rows - 1);
    int vis = hh ? hh->visualIndex(curCol) : curCol;
    if (vis < 0 || vis >= cols)
        vis = qBound(0, curCol, cols - 1);
    const int step = backward ? -1 : 1;
    for (int tries = 0; tries < rows * cols; ++tries) {
        vis += step;
        if (vis >= cols) {
            if (r + 1 >= rows)
                return m->index(r, hh ? hh->logicalIndex(cols - 1) : (cols - 1));
            ++r;
            vis = 0;
        } else if (vis < 0) {
            if (r <= 0)
                return m->index(0, hh ? hh->logicalIndex(0) : 0);
            --r;
            vis = cols - 1;
        }
        const int logCol = hh ? hh->logicalIndex(vis) : vis;
        if (logCol >= 0 && logCol < cols && (!hh || !hh->isSectionHidden(logCol)))
            return m->index(r, logCol);
    }
    return m->index(qBound(0, curRow, rows - 1), qBound(0, curCol, cols - 1));
}
} // namespace

TableItemDelegate::TableItemDelegate(QObject *parent) : QItemDelegate(parent)
{
    d_masking_color = QColor(0xff, 0, 0);
}

void TableItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    QItemDelegate::paint(painter, option, index);
    if (!index.data(TableModel::MaskingRole).toBool())
        return;
    painter->save();
    // masked cells are displayed as hatched
    painter->fillRect(option.rect, QBrush(d_masking_color, Qt::BDiagPattern));
    painter->restore();
}

void TableItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                     const QModelIndex &index) const
{
    model->setData(index, editor->metaObject()->userProperty().read(editor), Qt::EditRole);
}

void TableItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    editor->metaObject()->userProperty().write(editor, index.data(Qt::EditRole));
}

bool TableItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    QWidget *editor = qobject_cast<QWidget *>(object);
    if (editor && event && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        const int key = ke->key();
        const Qt::KeyboardModifiers mods = ke->modifiers();
        const bool hasNonShiftMod =
                (mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) != 0;

        if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
            // Let Ctrl+Tab / Ctrl+Shift+Tab pass through untouched (e.g. MDI window switching).
            if (hasNonShiftMod)
                return false;
            const bool backward = (key == Qt::Key_Backtab)
                    || (key == Qt::Key_Tab && (mods & Qt::ShiftModifier));
            QTableView *view = findParentTableView(parent(), editor);
            const QModelIndex cur = view ? view->currentIndex() : QModelIndex();
            const int curRow = cur.row();
            const int curCol = cur.column();
            emit commitData(editor);
            if (view && view->model() && cur.isValid()) {
                emit closeEditor(editor, QAbstractItemDelegate::NoHint);
                const QModelIndex nextIdx = stepCellHorizontally(view, curRow, curCol, backward);
                if (nextIdx.isValid()) {
                    view->setCurrentIndex(nextIdx);
                    view->scrollTo(nextIdx);
                }
            } else {
                emit closeEditor(editor,
                                 backward ? QAbstractItemDelegate::EditPreviousItem
                                          : QAbstractItemDelegate::EditNextItem);
            }
            return true;
        }

        if ((key == Qt::Key_Return || key == Qt::Key_Enter) && !hasNonShiftMod) {
            const bool moveUp = (mods & Qt::ShiftModifier) != 0;
            QTableView *view = findParentTableView(parent(), editor);
            const QModelIndex cur = view ? view->currentIndex() : QModelIndex();
            const int curRow = cur.row();
            const int curCol = cur.column();
            emit commitData(editor);
            emit closeEditor(editor, QAbstractItemDelegate::NoHint);
            if (view && view->model() && curRow >= 0 && curCol >= 0) {
                if (moveUp) {
                    if (curRow > 0) {
                        const QModelIndex prevIdx = view->model()->index(curRow - 1, curCol);
                        if (prevIdx.isValid()) {
                            view->setCurrentIndex(prevIdx);
                            view->scrollTo(prevIdx);
                        }
                    }
                } else {
                    const int rows = view->model()->rowCount();
                    if (curRow + 1 < rows) {
                        const QModelIndex nextIdx = view->model()->index(curRow + 1, curCol);
                        if (nextIdx.isValid()) {
                            view->setCurrentIndex(nextIdx);
                            view->scrollTo(nextIdx);
                        }
                    } else if (view->metaObject()->indexOfSignal("advanceCell()") >= 0) {
                        QMetaObject::invokeMethod(view, "advanceCell");
                    }
                }
            }
            return true;
        }
    }
    return QItemDelegate::eventFilter(object, event);
}
