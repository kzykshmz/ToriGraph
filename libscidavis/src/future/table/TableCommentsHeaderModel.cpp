/***************************************************************************
    File                 : TableCommentsHeaderModel.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Tilman Benkert,
    Email (use @ for *)  : thzs*gmx.net
    Description          : Model wrapping a TableModel to display column
                           comments in a TableCommentsHeaderView

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

#include "TableCommentsHeaderModel.h"
#include "core/column/Column.h"
#include <QRegularExpression>
#include <QTextDocument>

namespace {
QString stripSafeHeaderHtmlTags(const QString &raw)
{
    if (!raw.contains(QLatin1Char('<')))
        return raw;
    static const QRegularExpression kSafeTag(
            QStringLiteral("</?(?:i|b|sub|sup|u)>"),
            QRegularExpression::CaseInsensitiveOption);
    QString out = raw;
    out.remove(kSafeTag);
    return out;
}
} // namespace

TableCommentsHeaderModel::TableCommentsHeaderModel(TableModel *table_model, QObject *parent)
    : QAbstractTableModel(parent), d_table_model(table_model)
{
    if (d_table_model) {
        connect(d_table_model, SIGNAL(headerDataChanged(Qt::Orientation, int, int)), this,
                SIGNAL(headerDataChanged(Qt::Orientation, int, int)));
        connect(d_table_model, SIGNAL(columnsAboutToBeInserted(const QModelIndex &, int, int)), this,
                SIGNAL(columnsAboutToBeInserted(const QModelIndex &, int, int)));
        connect(d_table_model, SIGNAL(columnsAboutToBeRemoved(const QModelIndex &, int, int)), this,
                SIGNAL(columnsAboutToBeRemoved(const QModelIndex &, int, int)));
        connect(d_table_model, SIGNAL(columnsInserted(const QModelIndex &, int, int)), this,
                SIGNAL(columnsInserted(const QModelIndex &, int, int)));
        connect(d_table_model, SIGNAL(columnsRemoved(const QModelIndex &, int, int)), this,
                SIGNAL(columnsRemoved(const QModelIndex &, int, int)));
    }
}

TableCommentsHeaderModel::~TableCommentsHeaderModel() { }

Qt::ItemFlags TableCommentsHeaderModel::flags(const QModelIndex &index) const
{
    if (!d_table_model)
        return Qt::NoItemFlags;
    if (index.isValid())
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    else
        return Qt::ItemIsEnabled;
}

QVariant TableCommentsHeaderModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index);
    Q_UNUSED(role);
    return QVariant();
}

QVariant TableCommentsHeaderModel::headerData(int section, Qt::Orientation orientation,
                                              int role) const
{
    if (!d_table_model || orientation != Qt::Horizontal || section < 0 || section >= columnCount())
        return QVariant();

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return QVariant();

    Column *col = d_table_model ? d_table_model->column(section) : nullptr;
    if (!col) {
        if (role == Qt::DisplayRole && d_table_model)
            return QVariant(
                    d_table_model->headerData(section, Qt::Horizontal, TableModel::CommentRole));
        return QVariant();
    }

    const QString ln = col->longName().trimmed();
    QString u = col->units().trimmed();
    if ((u.startsWith(QLatin1Char('[')) && u.endsWith(QLatin1Char(']')))
        || (u.startsWith(QLatin1Char('(')) && u.endsWith(QLatin1Char(')')))
        || (u.startsWith(QChar(0xFF08)) && u.endsWith(QChar(0xFF09)))) {
        u = u.mid(1, u.length() - 2).trimmed();
    }
    QString cmt = col->comment().trimmed();
    cmt.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    cmt.replace(QLatin1Char('\r'), QLatin1Char('\n'));
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

    if (role == Qt::DisplayRole) {
        QStringList lines;
        if (!ln.isEmpty())
            lines << ln;
        if (!u.isEmpty())
            lines << QString("[%1]").arg(u);
        if (!cmt.isEmpty() && cmt != ln)
            lines << cmt;
        if (!fx.isEmpty())
            lines << QString("F(x)= %1").arg(fx);
        return lines.join(QLatin1String("\n"));
    }

    const QString plainTitle = stripSafeHeaderHtmlTags(col->formattedAxisTitle());
    const QString plainLn = stripSafeHeaderHtmlTags(ln);
    const QString plainU = stripSafeHeaderHtmlTags(u);
    const QString plainCmt = stripSafeHeaderHtmlTags(cmt);

    QStringList tipLines;
    tipLines << QString("%1 [%2]").arg(col->name(), plainTitle);
    if (!plainLn.isEmpty())
        tipLines << tr("Long Name:") + QLatin1String(" ") + plainLn;
    if (!plainU.isEmpty())
        tipLines << tr("Units:") + QLatin1String(" ") + plainU;
    if (!plainCmt.isEmpty())
        tipLines << tr("Comments:") + QLatin1String(" ") + plainCmt;
    if (!fx.isEmpty())
        tipLines << QString("F(x)= %1").arg(fx);
    const QString fxErr = col->formulaError().trimmed();
    if (!fxErr.isEmpty())
        tipLines << fxErr;
    tipLines << tr("Double-click header to edit Long Name, Units & F(x)=");
    const QString tip = tipLines.join(QLatin1String("\n"));
    if (Qt::mightBeRichText(tip))
        return Qt::convertFromPlainText(tip);
    return tip;
}

int TableCommentsHeaderModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return d_table_model ? d_table_model->rowCount() : 0;
}

int TableCommentsHeaderModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return d_table_model ? d_table_model->columnCount() : 0;
}
