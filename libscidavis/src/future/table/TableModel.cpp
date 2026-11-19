/***************************************************************************
    File                 : TableModel.cpp
    Project              : SciDAVis
    Description          : Model for the access to a Table
    --------------------------------------------------------------------
    Copyright            : (C) 2007 Tilman Benkert (thzs*gmx.net)
                           (replace * with @ in the email addresses)

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

#include "core/column/Column.h"
#include "table/future_Table.h"
#include "table/TableModel.h"
#include <QString>
#include <QBrush>
#include <QIcon>
#include <QLocale>
#include <QPixmap>
#include <cmath>

TableModel::TableModel(future::Table *table)
    : QAbstractItemModel(0),
      d_table(table),
      d_formula_mode(false)
#ifdef LEGACY_CODE_0_2_x
      ,
      d_read_only(false)
#endif
{
    if (d_table) {
        connect(d_table, SIGNAL(columnsAboutToBeInserted(int, QList<Column *>)), this,
                SLOT(handleColumnsAboutToBeInserted(int, QList<Column *>)));
        connect(d_table, SIGNAL(columnsInserted(int, int)), this,
                SLOT(handleColumnsInserted(int, int)));
        connect(d_table, SIGNAL(columnsAboutToBeRemoved(int, int)), this,
                SLOT(handleColumnsAboutToBeRemoved(int, int)));
        connect(d_table, SIGNAL(columnsRemoved(int, int)), this,
                SLOT(handleColumnsRemoved(int, int)));
        connect(d_table, SIGNAL(rowsAboutToBeInserted(int, int)), this,
                SLOT(handleRowsAboutToBeInserted(int, int)));
        connect(d_table, SIGNAL(rowsInserted(int, int)), this, SLOT(handleRowsInserted(int, int)));
        connect(d_table, SIGNAL(rowsAboutToBeRemoved(int, int)), this,
                SLOT(handleRowsAboutToBeRemoved(int, int)));
        connect(d_table, SIGNAL(rowsRemoved(int, int)), this, SLOT(handleRowsRemoved(int, int)));
        connect(d_table, SIGNAL(dataChanged(int, int, int, int)), this,
                SLOT(handleDataChanged(int, int, int, int)));
        connect(d_table, SIGNAL(headerDataChanged(Qt::Orientation, int, int)), this,
                SIGNAL(headerDataChanged(Qt::Orientation, int, int)));
    }
}

TableModel::~TableModel() { }

Qt::ItemFlags TableModel::flags(const QModelIndex &index) const
{
    if (!d_table)
        return Qt::NoItemFlags;
#ifdef LEGACY_CODE_0_2_x
    Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (!isReadOnly())
        result |= Qt::ItemIsEditable;
    if (index.isValid())
        return result;
#else
    if (index.isValid())
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
#endif
    else
        return Qt::ItemIsEnabled;
}

QVariant TableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !d_table)
        return QVariant();

    int row = index.row();
    int col = index.column();
    Column *col_ptr = d_table->column(col);
    if (!col_ptr)
        return QVariant();

    QString postfix;
    switch (role) {
    case Qt::ToolTipRole:
        if (col_ptr->isMasked(row))
            postfix = " " + tr("(masked)");
        if (col_ptr->isInvalid(row))
            return QVariant(tr("invalid cell (ignored in all operations)",
                               "tooltip string for invalid rows")
                            + postfix);
        [[fallthrough]];
    case Qt::EditRole:
        if (!d_formula_mode && col_ptr->isInvalid(row))
            return QVariant();
        [[fallthrough]];
    case Qt::DisplayRole: {
        if (d_formula_mode)
            return QVariant(col_ptr->formula(row));
        if (col_ptr->isInvalid(row))
            return QVariant(tr("-", "string for invalid rows"));

        return QVariant(col_ptr->asStringColumn()->textAt(row) + postfix);
    }
    case Qt::ForegroundRole: {
        if (col_ptr->isInvalid(index.row()))
            return QVariant(QBrush(QColor(0xff, 0, 0))); // invalid -> red letters
        else
            return QVariant(QBrush(QColor(0, 0, 0)));
    }
    case MaskingRole:
        return QVariant(col_ptr->isMasked(row));
    case FormulaRole:
        return QVariant(col_ptr->formula(row));
    case Qt::DecorationRole:
        if (d_formula_mode)
            return QIcon(QPixmap(":/equals.png"));
        break;
    }

    return QVariant();
}

QVariant TableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return d_table ? d_table->headerData(section, orientation, role) : QVariant();
}

int TableModel::rowCount(const QModelIndex &) const
{
    return d_table ? d_table->rowCount() : 0;
}

int TableModel::columnCount(const QModelIndex &) const
{
    return d_table ? d_table->columnCount() : 0;
}

bool TableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || !d_table)
        return false;

    int row = index.row();
    Column *col_ptr = d_table->column(index.column());
    if (!col_ptr)
        return false;

    switch (role) {
    case Qt::EditRole: {
        const QString rawStr = value.toString();
        d_table->beginMacro(tr("%1: change cell value(s)").arg(d_table->name()));
        if (d_formula_mode) {
            col_ptr->setFormula(row, rawStr);
        } else {
            bool promotedToText = false;
            if (col_ptr->columnMode() == SciDAVis::ColumnMode::Numeric
                && col_ptr->formulaIntervals().isEmpty()) {
                const QString trimmed = rawStr.trimmed();
                const bool isMissingSentinel = trimmed.isEmpty() || trimmed == QLatin1String("-")
                        || trimmed == QLatin1String("--") || trimmed == QLatin1String("---")
                        || trimmed.compare(QLatin1String("nan"), Qt::CaseInsensitive) == 0
                        || trimmed.compare(QLatin1String("na"), Qt::CaseInsensitive) == 0
                        || trimmed.compare(QLatin1String("n/a"), Qt::CaseInsensitive) == 0
                        || trimmed.compare(QLatin1String("null"), Qt::CaseInsensitive) == 0;
                if (!isMissingSentinel) {
                    bool numOk = false;
                    double parsed = QLocale().toDouble(trimmed, &numOk);
                    if (!numOk)
                        parsed = QLocale::c().toDouble(trimmed, &numOk);
                    if (!numOk || !std::isfinite(parsed)) {
                        bool hasAnyFinite = false;
                        const int nRows = col_ptr->rowCount();
                        for (int r = 0; r < nRows; ++r) {
                            if (r != row && !col_ptr->isInvalid(r)
                                && std::isfinite(col_ptr->valueAt(r))) {
                                hasAnyFinite = true;
                                break;
                            }
                        }
                        if (!hasAnyFinite) {
                            col_ptr->setColumnMode(SciDAVis::ColumnMode::Text);
                            col_ptr->setTextAt(row, rawStr);
                            promotedToText = true;
                        }
                    }
                }
            }
            if (!promotedToText)
                col_ptr->asStringColumn()->setTextAt(row, rawStr);
        }
        d_table->endMacro();
        return true;
    }
    case MaskingRole: {
        d_table->beginMacro(tr("%1: mask cells").arg(d_table->name()));
        col_ptr->setMasked(row, value.toBool());
        d_table->endMacro();
        return true;
    }
    case FormulaRole: {
        d_table->beginMacro(tr("%1: apply formula to selection").arg(d_table->name()));
        col_ptr->setFormula(row, value.toString());
        d_table->endMacro();
        return true;
    }
    }

    return false;
}

QModelIndex TableModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || !d_table || row < 0 || column < 0)
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex TableModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child)
    return QModelIndex();
}

void TableModel::handleColumnsAboutToBeInserted(int before, QList<Column *> cols)
{
    int count = cols.count();

    beginInsertColumns(QModelIndex(), before, before + count - 1);
}

void TableModel::handleColumnsInserted(int first, int count)
{
    Q_UNUSED(first)
    Q_UNUSED(count)
    endInsertColumns();
}

void TableModel::handleColumnsAboutToBeRemoved(int first, int count)
{
    beginRemoveColumns(QModelIndex(), first, first + count - 1);
}

void TableModel::handleColumnsRemoved(int first, int count)
{
    Q_UNUSED(first)
    Q_UNUSED(count)
    endRemoveColumns();
}

void TableModel::handleRowsAboutToBeInserted(int before, int count)
{
    beginInsertRows(QModelIndex(), before, before + count - 1);
}

void TableModel::handleRowsInserted(int first, int count)
{
    Q_UNUSED(first)
    Q_UNUSED(count)
    endInsertRows();
}

void TableModel::handleRowsAboutToBeRemoved(int first, int count)
{
    beginRemoveRows(QModelIndex(), first, first + count - 1);
}

void TableModel::handleRowsRemoved(int first, int count)
{
    Q_UNUSED(first)
    Q_UNUSED(count)
    endRemoveRows();
}

void TableModel::handleDataChanged(int top, int left, int bottom, int right)
{
    emit dataChanged(index(top, left), index(bottom, right));
}

Column *TableModel::column(int index)
{
    return d_table ? d_table->column(index) : nullptr;
}
