/***************************************************************************
    File                 : TableDoubleHeaderView.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Tilman Benkert,
    Email (use @ for *)  : thzs*gmx.net
    Description          : Horizontal header for TableView displaying comments in a second header

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

#include "TableDoubleHeaderView.h"
#include "TableCommentsHeaderModel.h"
#include "table/TableView.h"
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QTableView>

namespace {
TableView *findOwnerTableView(QWidget *w)
{
    for (QWidget *p = w ? w->parentWidget() : nullptr; p; p = p->parentWidget()) {
        if (TableView *tv = qobject_cast<TableView *>(p))
            return tv;
    }
    return nullptr;
}
} // namespace

TableCommentsHeaderView::TableCommentsHeaderView(QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent)
{
}

TableCommentsHeaderView::~TableCommentsHeaderView()
{
    delete model();
}

void TableCommentsHeaderView::setModel(QAbstractItemModel *model)
{
    Q_ASSERT(model->inherits("TableModel"));
    QAbstractItemModel *old_model = QHeaderView::model();
    TableCommentsHeaderModel *new_model =
            new TableCommentsHeaderModel(static_cast<TableModel *>(model));
    QHeaderView::setModel(new_model);
    QObject::disconnect(
            new_model, SIGNAL(columnsInserted(QModelIndex, int, int)), this,
            SLOT(sectionsInserted(
                    QModelIndex, int,
                    int))); // We have to make sure sectionsInserted is called in the right order
    delete old_model;
}

QSize TableCommentsHeaderView::sizeHint() const
{
    QSize base = QHeaderView::sizeHint();
    const QAbstractItemModel *m = model();
    const QFontMetrics fm(font());
    int maxLines = 1;
    if (m) {
        const int cols = m->columnCount();
        for (int c = 0; c < cols; ++c) {
            const QString txt = m->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
            if (!txt.isEmpty()) {
                const int n = txt.count(QLatin1Char('\n')) + 1;
                if (n > maxLines)
                    maxLines = n;
            }
        }
    }
    const int lineH = qMax(16, fm.height() + 3);
    const int neededH = maxLines * lineH + 8;
    base.setHeight(qMax(24, neededH));
    return base;
}

void TableCommentsHeaderView::paintSection(QPainter *painter, const QRect &rect,
                                           int logicalIndex) const
{
    if (!painter || !rect.isValid())
        return;

    painter->save();
    painter->setClipRect(rect);
    painter->fillRect(rect, QColor(0xF8, 0xFA, 0xFC));
    painter->setPen(QColor(0xE2, 0xE8, 0xF0));
    painter->drawLine(rect.topRight(), rect.bottomRight());
    painter->setPen(QColor(0xCB, 0xD5, 0xE1));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    const QAbstractItemModel *m = model();
    const QString txt =
            m ? m->headerData(logicalIndex, Qt::Horizontal, Qt::DisplayRole).toString() : QString();
    if (!txt.isEmpty()) {
        const QStringList lines = txt.split(QLatin1Char('\n'));
        QFont baseFont = font();
        const QFontMetrics baseFm(baseFont);
        const int lineH = qMax(16, baseFm.height() + 3);
        const QRect contentRect = rect.adjusted(6, 4, -5, -3);
        int curY = contentRect.top();
        for (int i = 0; i < lines.size(); ++i) {
            if (curY >= contentRect.bottom())
                break;
            const QString &ln = lines.at(i);
            QFont lineFont = baseFont;
            QColor lineCol(0x0F, 0x17, 0x2A);
            if (ln.startsWith(QLatin1String("F(x)="))) {
                lineCol = QColor(0x1D, 0x4E, 0xD8);
            } else if (ln.startsWith(QLatin1Char('[')) && ln.endsWith(QLatin1Char(']'))) {
                lineCol = QColor(0x47, 0x55, 0x69);
            } else if (i == 0) {
                lineFont.setWeight(QFont::DemiBold);
            }
            painter->setFont(lineFont);
            painter->setPen(lineCol);
            const QFontMetrics lfm(lineFont);
            const QRect lineRect(contentRect.left(), curY, qMax(8, contentRect.width()), lineH);
            painter->drawText(lineRect, Qt::AlignLeft | Qt::AlignVCenter,
                              lfm.elidedText(ln, Qt::ElideRight, lineRect.width()));
            curY += lineH;
        }
    }
    painter->restore();
}

TableDoubleHeaderView::TableDoubleHeaderView(QWidget *parent) : QHeaderView(Qt::Horizontal, parent)
{
    setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    d_slave = new TableCommentsHeaderView();
    d_slave->setDefaultAlignment(Qt::AlignLeft | Qt::AlignTop);
    d_show_comments = true;
}

TableDoubleHeaderView::~TableDoubleHeaderView()
{
    delete d_slave;
}

QSize TableDoubleHeaderView::sizeHint() const
{
    QSize master_size, slave_size;
    if (d_slave->font() != font())
        d_slave->setFont(font());
    master_size = QHeaderView::sizeHint();
    if (master_size.height() < 24)
        master_size.setHeight(24);
    slave_size = d_slave->sizeHint();
    if (d_show_comments)
        master_size.setHeight(master_size.height() + slave_size.height());
    return master_size;
}

void TableDoubleHeaderView::setModel(QAbstractItemModel *model)
{
    Q_ASSERT(model->inherits("TableModel"));
    d_slave->setModel(model);
    QHeaderView::setModel(model);
    connect(model, SIGNAL(headerDataChanged(Qt::Orientation, int, int)), this,
            SLOT(headerDataChanged(Qt::Orientation, int, int)));
}

void TableDoubleHeaderView::paintSection(QPainter *painter, const QRect &rect,
                                         int logicalIndex) const
{
    if (d_slave->font() != font())
        d_slave->setFont(font());
    const int topH = qMax(24, QHeaderView::sizeHint().height());
    QRect master_rect = rect;
    if (d_show_comments && rect.height() > topH)
        master_rect = QRect(rect.left(), rect.top(), rect.width(), topH);
    QHeaderView::paintSection(painter, master_rect, logicalIndex);
    if (d_show_comments && rect.height() > topH) {
        QRect slave_rect(rect.left(), rect.top() + topH, rect.width(), rect.height() - topH);
        d_slave->paintSection(painter, slave_rect, logicalIndex);
    }
}

bool TableDoubleHeaderView::areCommentsShown() const
{
    return d_show_comments;
}

void TableDoubleHeaderView::showComments(bool on)
{
    d_show_comments = on;
    refresh();
}

void TableDoubleHeaderView::refresh()
{
    // adjust geometry and repaint header (still looking for a more elegant solution)
    d_slave->setStretchLastSection(true); // ugly hack (flaw in Qt? Does anyone know a better way?)
    d_slave->updateGeometry();
    d_slave->setStretchLastSection(false); // ugly hack part 2
    setStretchLastSection(true); // ugly hack (flaw in Qt? Does anyone know a better way?)
    updateGeometry();
    setStretchLastSection(false); // ugly hack part 2
    update();
}

void TableDoubleHeaderView::headerDataChanged(Qt::Orientation orientation, int logicalFirst,
                                              int logicalLast)
{
    Q_UNUSED(logicalFirst);
    Q_UNUSED(logicalLast);
    if (orientation == Qt::Horizontal)
        refresh();
}

void TableDoubleHeaderView::sectionsInserted(const QModelIndex &parent, int logicalFirst,
                                             int logicalLast)
{
    d_slave->sectionsInserted(parent, logicalFirst, logicalLast);
    QHeaderView::sectionsInserted(parent, logicalFirst, logicalLast);
    Q_ASSERT(d_slave->count() == QHeaderView::count());
}

void TableDoubleHeaderView::mousePressEvent(QMouseEvent *event)
{
    d_drag_candidate = false;
    d_drag_started = false;
    d_pressed_logical_col = -1;
    d_saved_selected_cols.clear();

    if (event && event->button() == Qt::LeftButton) {
        const int col = logicalIndexAt(event->pos());
        bool onResizeGrip = (cursor().shape() == Qt::SplitHCursor);
        if (!onResizeGrip && col >= 0) {
            const int leftEdge = sectionViewportPosition(col);
            const int rightEdge = leftEdge + sectionSize(col);
            if (qAbs(event->pos().x() - leftEdge) <= 4 || qAbs(event->pos().x() - rightEdge) <= 4)
                onResizeGrip = true;
        }
        if (!onResizeGrip && col >= 0) {
            d_drag_candidate = true;
            d_drag_start_pos = event->pos();
            d_pressed_logical_col = col;
            if (TableView *tv = findOwnerTableView(this))
                d_saved_selected_cols = tv->selectedColumnIndices(false);
        }
    }

    QHeaderView::mousePressEvent(event);
}

void TableDoubleHeaderView::mouseMoveEvent(QMouseEvent *event)
{
    if (event && d_drag_candidate && !d_drag_started && (event->buttons() & Qt::LeftButton)) {
        const QPoint pos = event->pos();
        const int dist = (pos - d_drag_start_pos).manhattanLength();
        const bool verticalEscape = (pos.y() > height() + 4 || pos.y() < -4);
        const bool modifierDrag =
                (event->modifiers() & (Qt::AltModifier | Qt::ControlModifier))
                && (dist >= QApplication::startDragDistance());

        if (verticalEscape || modifierDrag) {
            d_drag_started = true;
            d_drag_candidate = false;
            const int clickedCol = d_pressed_logical_col;
            const QList<int> savedCols = d_saved_selected_cols;
            const Qt::KeyboardModifiers mods = event->modifiers();

            // Cleanly cancel QHeaderView's in-progress horizontal section move or drag-select
            // at the original press position so horizontal reordering inside header stays intact.
            QMouseEvent moveBackEv(QEvent::MouseMove, d_drag_start_pos,
                                   mapToGlobal(d_drag_start_pos), Qt::NoButton, Qt::LeftButton,
                                   Qt::NoModifier);
            QHeaderView::mouseMoveEvent(&moveBackEv);
            QMouseEvent releaseCancelEv(QEvent::MouseButtonRelease, d_drag_start_pos,
                                        mapToGlobal(d_drag_start_pos), Qt::LeftButton, Qt::NoButton,
                                        Qt::NoModifier);
            QHeaderView::mouseReleaseEvent(&releaseCancelEv);
            if (viewport())
                viewport()->update();
            update();

            if (TableView *tv = findOwnerTableView(this)) {
                QList<int> dragCols;
                if (savedCols.size() > 1 && savedCols.contains(clickedCol)) {
                    dragCols = savedCols;
                } else if ((mods & Qt::ControlModifier) && !savedCols.isEmpty()) {
                    dragCols = savedCols;
                    if (clickedCol >= 0 && !dragCols.contains(clickedCol))
                        dragCols.append(clickedCol);
                } else {
                    dragCols = tv->selectedColumnIndices(false);
                    if (dragCols.isEmpty() && clickedCol >= 0)
                        dragCols.append(clickedCol);
                }

                // Restore multi-column selection in the QTableView if press collapsed it
                if (dragCols.size() > 1) {
                    if (QTableView *qtv = qobject_cast<QTableView *>(parentWidget())) {
                        if (qtv->selectionModel() && qtv->model() && qtv->model()->rowCount() > 0) {
                            const int lastRow = qtv->model()->rowCount() - 1;
                            for (int c : dragCols) {
                                if (c >= 0 && c < qtv->model()->columnCount()) {
                                    QItemSelection colSel(qtv->model()->index(0, c),
                                                          qtv->model()->index(lastRow, c));
                                    qtv->selectionModel()->select(colSel,
                                                                  QItemSelectionModel::Select);
                                }
                            }
                        }
                    }
                }

                QPointer<TableDoubleHeaderView> self(this);
                tv->startColumnDrag(dragCols, clickedCol, this);
                if (!self)
                    return;
                d_drag_started = false;
                d_pressed_logical_col = -1;
                d_saved_selected_cols.clear();
            }
            return;
        }
    }

    if (d_drag_started && (event->buttons() & Qt::LeftButton))
        return;

    QHeaderView::mouseMoveEvent(event);
}

void TableDoubleHeaderView::mouseReleaseEvent(QMouseEvent *event)
{
    const bool wasStarted = d_drag_started;
    d_drag_candidate = false;
    d_drag_started = false;
    d_pressed_logical_col = -1;
    d_saved_selected_cols.clear();

    if (wasStarted)
        return;

    QHeaderView::mouseReleaseEvent(event);
}
