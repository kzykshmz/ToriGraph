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
#include "core/column/Column.h"
#include "ApplicationWindow.h"
#include "Table.h"
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QTableView>
#include <QTimer>

namespace {
TableView *findOwnerTableView(QWidget *w)
{
    for (QWidget *p = w ? w->parentWidget() : nullptr; p; p = p->parentWidget()) {
        if (TableView *tv = qobject_cast<TableView *>(p))
            return tv;
    }
    return nullptr;
}

QString extractColumnFormula(const Column *col)
{
    if (!col)
        return QString();
    QString fx;
    const QList<Interval<int>> fIntervals = col->formulaIntervals();
    for (const Interval<int> &iv : fIntervals) {
        const QString f = col->formula(iv.start()).trimmed();
        if (!f.isEmpty()) {
            fx = f;
            break;
        }
    }
    if (fx.isEmpty())
        fx = col->formula(0).trimmed();
    fx.replace(QLatin1String("\r\n"), QLatin1String(" "));
    fx.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return fx;
}

QString stripOuterUnitBracketsForHeader(const QString &raw)
{
    QString u = raw.trimmed();
    if (u.size() >= 2
        && ((u.startsWith(QLatin1Char('[')) && u.endsWith(QLatin1Char(']')))
            || (u.startsWith(QLatin1Char('(')) && u.endsWith(QLatin1Char(')')))
            || (u.startsWith(QChar(0xFF08)) && u.endsWith(QChar(0xFF09)))
            || (u.startsWith(QChar(0xFF3B)) && u.endsWith(QChar(0xFF3D))))) {
        u = u.mid(1, u.length() - 2).trimmed();
    }
    return u;
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
    d_table_model = static_cast<TableModel *>(model);
    QAbstractItemModel *old_model = QHeaderView::model();
    TableCommentsHeaderModel *new_model = new TableCommentsHeaderModel(d_table_model);
    QHeaderView::setModel(new_model);
    QObject::disconnect(
            new_model, SIGNAL(columnsInserted(QModelIndex, int, int)), this,
            SLOT(sectionsInserted(
                    QModelIndex, int,
                    int))); // We have to make sure sectionsInserted is called in the right order
    delete old_model;
}

int TableCommentsHeaderView::subRowLineHeight() const
{
    const QFontMetrics fm(font());
    return qMax(22, fm.height() + 3);
}

bool TableCommentsHeaderView::anyColumnHasFormula() const
{
    if (!d_table_model)
        return false;
    const int cols = d_table_model->columnCount();
    for (int c = 0; c < cols; ++c) {
        if (!extractColumnFormula(d_table_model->column(c)).isEmpty())
            return true;
    }
    return false;
}

int TableCommentsHeaderView::subRowCount() const
{
    return anyColumnHasFormula() ? 4 : 3;
}

QSize TableCommentsHeaderView::sizeHint() const
{
    QSize base = QHeaderView::sizeHint();
    const int neededH = subRowCount() * subRowLineHeight() + 4;
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

    const Column *col = d_table_model ? d_table_model->column(logicalIndex) : nullptr;
    const QString ln = col ? col->longName().trimmed() : QString();
    const QString u = col ? stripOuterUnitBracketsForHeader(col->units()) : QString();
    QString cmt = col ? col->comment().trimmed() : QString();
    cmt.replace(QLatin1String("\r\n"), QLatin1String(" "));
    cmt.replace(QLatin1Char('\r'), QLatin1Char(' '));
    cmt.replace(QLatin1Char('\n'), QLatin1Char(' '));
    const QString fx = extractColumnFormula(col);

    const int nRows = subRowCount();
    const int lineH = subRowLineHeight();
    QFont baseFont = font();

    for (int r = 0; r < nRows; ++r) {
        const int rowTop = rect.top() + 2 + r * lineH;
        if (rowTop >= rect.bottom())
            break;
        const QRect rowRect(rect.left(), rowTop, rect.width(), lineH);
        if (r < nRows - 1 && rowRect.bottom() < rect.bottom()) {
            painter->setPen(QColor(0xEE, 0xF2, 0xF6));
            painter->drawLine(rect.left() + 1, rowRect.bottom(), rect.right() - 1,
                              rowRect.bottom());
        }

        QString cellTxt;
        bool isPlaceholder = false;
        QFont lineFont = baseFont;
        QColor lineCol(0x0F, 0x17, 0x2A);

        if (r == 0) {
            if (!ln.isEmpty()) {
                cellTxt = ln;
                lineFont.setWeight(QFont::DemiBold);
            } else if (logicalIndex == 0) {
                cellTxt = tr("Long Name");
                isPlaceholder = true;
            }
        } else if (r == 1) {
            if (!u.isEmpty()) {
                cellTxt = QStringLiteral("[") + u + QStringLiteral("]");
                lineCol = QColor(0x47, 0x55, 0x69);
            } else if (logicalIndex == 0) {
                cellTxt = tr("Units");
                isPlaceholder = true;
            }
        } else if (r == 2) {
            if (!cmt.isEmpty() && cmt != ln) {
                cellTxt = cmt;
                lineCol = QColor(0x47, 0x55, 0x69);
            } else if (logicalIndex == 0) {
                cellTxt = tr("Comments");
                isPlaceholder = true;
            }
        } else {
            if (!fx.isEmpty()) {
                cellTxt = QStringLiteral("F(x)= ") + fx;
                lineCol = QColor(0x1D, 0x4E, 0xD8);
            } else if (logicalIndex == 0) {
                cellTxt = QStringLiteral("F(x)=");
                isPlaceholder = true;
            }
        }

        if (isPlaceholder)
            lineCol = QColor(0x94, 0xA3, 0xB8);

        if (!cellTxt.isEmpty()) {
            painter->setFont(lineFont);
            painter->setPen(lineCol);
            const QFontMetrics lfm(lineFont);
            const QRect textRect = rowRect.adjusted(6, 0, -5, 0);
            if (textRect.width() > 4) {
                painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                                  lfm.elidedText(cellTxt, Qt::ElideRight, textRect.width()));
            }
        }
    }

    painter->setPen(QColor(0xCB, 0xD5, 0xE1));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());
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
    cancelInlineSubHeaderEdit();
    delete d_slave;
}

int TableDoubleHeaderView::topHeaderHeight() const
{
    return qMax(28, QHeaderView::sizeHint().height());
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
    const int topH = topHeaderHeight();
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
    if (!on)
        cancelInlineSubHeaderEdit();
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
    if (d_inline_editor)
        commitInlineSubHeaderEdit();

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

bool TableDoubleHeaderView::handleSubHeaderDoubleClick(const QPoint &pos)
{
    if (ApplicationWindow::isNonInteractiveContext(this))
        return false;

    const int col = logicalIndexAt(pos);
    if (col < 0)
        return false;

    // Check section resize grip first: double-clicking divider auto-fits column width
    const int leftEdge = sectionViewportPosition(col);
    const int rightEdge = leftEdge + sectionSize(col);
    if (cursor().shape() == Qt::SplitHCursor || qAbs(pos.x() - rightEdge) <= 4
        || qAbs(pos.x() - leftEdge) <= 4) {
        int resizeCol = col;
        if (qAbs(pos.x() - leftEdge) <= 4) {
            const int prevVisual = visualIndex(col) - 1;
            if (prevVisual >= 0)
                resizeCol = logicalIndex(prevVisual);
        }
        if (TableView *tv = findOwnerTableView(this))
            tv->autoExpandColumnWidthForHeader(resizeCol, true);
        return true;
    }

    const int topH = topHeaderHeight();
    if (!d_show_comments || pos.y() < topH)
        return false;

    const int lineH = d_slave ? d_slave->subRowLineHeight() : 17;
    const int maxRows = d_slave ? d_slave->subRowCount() : 3;
    const int subRow = qBound(0, (pos.y() - topH - 2) / qMax(1, lineH), qMax(0, maxRows - 1));
    beginInlineSubHeaderEdit(col, subRow);
    return true;
}

void TableDoubleHeaderView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event && event->button() == Qt::LeftButton && handleSubHeaderDoubleClick(event->pos())) {
        event->accept();
        return;
    }
    QHeaderView::mouseDoubleClickEvent(event);
}

void TableDoubleHeaderView::beginInlineSubHeaderEdit(int logicalCol, int subRow)
{
    if (ApplicationWindow::isNonInteractiveContext(this))
        return;
    TableModel *tm = qobject_cast<TableModel *>(QHeaderView::model());
    if (!tm || logicalCol < 0 || logicalCol >= tm->columnCount())
        return;
    Column *col = tm->column(logicalCol);
    if (!col)
        return;

    if (d_inline_editor)
        commitInlineSubHeaderEdit();

    const int topH = topHeaderHeight();
    const int lineH = d_slave ? d_slave->subRowLineHeight() : 17;
    const int secX = sectionViewportPosition(logicalCol);
    const int secW = sectionSize(logicalCol);
    const QRect editRect(secX + 1, topH + 1 + subRow * lineH, qMax(56, secW - 2), lineH + 1);

    QWidget *parentVp = viewport() ? viewport() : static_cast<QWidget *>(this);
    QLineEdit *ed = new QLineEdit(parentVp);
    ed->setObjectName(QStringLiteral("TableSubHeaderInlineEditor"));
    ed->setGeometry(editRect);
    ed->setFont(font());
    ed->setStyleSheet(QStringLiteral(
            "QLineEdit#TableSubHeaderInlineEditor {"
            "  background: #FFFFFF;"
            "  color: #0F172A;"
            "  border: 1px solid #2563EB;"
            "  border-radius: 2px;"
            "  padding: 0px 4px;"
            "}"));

    if (subRow == 0) {
        ed->setText(col->longName());
        ed->setPlaceholderText(tr("Long Name"));
    } else if (subRow == 1) {
        ed->setText(stripOuterUnitBracketsForHeader(col->units()));
        ed->setPlaceholderText(tr("Units (e.g. MPa, %)"));
    } else if (subRow == 2) {
        QString cmt = col->comment();
        cmt.replace(QLatin1String("\r\n"), QLatin1String(" "));
        cmt.replace(QLatin1Char('\n'), QLatin1Char(' '));
        ed->setText(cmt);
        ed->setPlaceholderText(tr("Comments"));
    } else {
        ed->setText(extractColumnFormula(col));
        ed->setPlaceholderText(QStringLiteral("F(x)="));
    }

    d_inline_editor = ed;
    d_inline_col = logicalCol;
    d_inline_subrow = subRow;
    ed->installEventFilter(this);
    connect(ed, &QLineEdit::editingFinished, this,
            &TableDoubleHeaderView::commitInlineSubHeaderEdit);
    ed->show();
    ed->raise();
    ed->setFocus(Qt::MouseFocusReason);
    ed->selectAll();
}

void TableDoubleHeaderView::commitInlineSubHeaderEdit()
{
    if (d_committing_inline || !d_inline_editor)
        return;
    d_committing_inline = true;

    QPointer<QLineEdit> ed = d_inline_editor;
    const int logicalCol = d_inline_col;
    const int subRow = d_inline_subrow;
    const QString txt = ed->text().trimmed();

    d_inline_editor = nullptr;
    d_inline_col = -1;
    d_inline_subrow = -1;
    ed->hide();
    ed->deleteLater();

    TableModel *tm = qobject_cast<TableModel *>(QHeaderView::model());
    Column *col = (tm && logicalCol >= 0 && logicalCol < tm->columnCount())
            ? tm->column(logicalCol)
            : nullptr;
    if (col) {
        if (subRow == 0) {
            if (col->longName() != txt)
                col->setLongName(txt);
        } else if (subRow == 1) {
            const QString uClean = stripOuterUnitBracketsForHeader(txt);
            if (col->units() != uClean)
                col->setUnits(uClean);
        } else if (subRow == 2) {
            if (col->comment() != txt)
                col->setComment(txt);
        } else if (subRow == 3) {
            TableView *tv = findOwnerTableView(this);
            if (::Table *legacyTbl = qobject_cast<::Table *>(tv)) {
                legacyTbl->setCommand(logicalCol, txt);
                if (!txt.isEmpty())
                    legacyTbl->recalculate(logicalCol, false);
            } else {
                if (txt.isEmpty())
                    col->clearFormulas();
                else
                    col->setFormula(Interval<int>(0, qMax(0, tm->rowCount() - 1)), txt);
            }
        }
        if (TableView *tv = findOwnerTableView(this))
            tv->autoExpandColumnWidthForHeader(logicalCol, false);
        refresh();
    }

    d_committing_inline = false;
}

void TableDoubleHeaderView::cancelInlineSubHeaderEdit()
{
    if (!d_inline_editor)
        return;
    const bool prev = d_committing_inline;
    d_committing_inline = true;
    QPointer<QLineEdit> ed = d_inline_editor;
    d_inline_editor = nullptr;
    d_inline_col = -1;
    d_inline_subrow = -1;
    if (ed) {
        ed->hide();
        ed->deleteLater();
    }
    d_committing_inline = prev;
    update();
}

bool TableDoubleHeaderView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == d_inline_editor && event && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            cancelInlineSubHeaderEdit();
            return true;
        }
        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab
            || ke->key() == Qt::Key_Down || ke->key() == Qt::Key_Up) {
            const int curCol = d_inline_col;
            const int curSub = d_inline_subrow;
            const int totalCols = count();
            const int maxRows = d_slave ? d_slave->subRowCount() : 3;
            commitInlineSubHeaderEdit();
            int nextCol = curCol;
            int nextSub = curSub;
            if (ke->key() == Qt::Key_Tab && totalCols > 0)
                nextCol = (curCol + 1) % totalCols;
            else if (ke->key() == Qt::Key_Backtab && totalCols > 0)
                nextCol = (curCol - 1 + totalCols) % totalCols;
            else if (ke->key() == Qt::Key_Down)
                nextSub = qMin(maxRows - 1, curSub + 1);
            else if (ke->key() == Qt::Key_Up)
                nextSub = qMax(0, curSub - 1);
            QPointer<TableDoubleHeaderView> self(this);
            QTimer::singleShot(0, this, [self, nextCol, nextSub]() {
                if (self)
                    self->beginInlineSubHeaderEdit(nextCol, nextSub);
            });
            return true;
        }
    }
    return QHeaderView::eventFilter(watched, event);
}
