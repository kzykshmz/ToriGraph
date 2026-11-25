/***************************************************************************
    File                 : Table.cpp
    Project              : SciDAVis
    Description          : Aspect providing a spreadsheet table with column logic
    --------------------------------------------------------------------
    Copyright            : (C) 2006-2009 Tilman Benkert (thzs*gmx.net)
    Copyright            : (C) 2006-2009 Knut Franke (knut.franke*gmx.de)
    Copyright            : (C) 2006-2007 Ion Vasilief (ion_vasilief*yahoo.fr)
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
#include "table/future_Table.h"
#include "core/Project.h"
#include "lib/ActionManager.h"
#include <QItemSelectionModel>
#include <QTime>
#include <QtGlobal>
#include <QHBoxLayout>
#include <QShortcut>
#include <QApplication>
#include <QContextMenuEvent>
#include <climits> // for RAND_MAX
#include <cmath>
#include <limits>
#include <QMenu>
#include <QItemSelection>
#include <QModelIndex>
#include <QModelIndexList>
#include <QInputDialog>
#include <QMapIterator>
#include <QDialog>
#include <QMenuBar>
#include <QClipboard>
#include <QToolBar>
#include <QtDebug>
#include <QMimeData>
#include <QMdiSubWindow>
#include <QStyle>
#include <QColor>
#include <qwt_plot.h>
#include "ApplicationWindow.h"
#include "MultiLayer.h"
#include "Graph.h"
#include "Table.h"
#include "I18nJapanese.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include <algorithm>
#include <mutex>
#include <QSet>
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#endif

#include "table/TableModel.h"
#include "table/TableView.h"
#include "table/tablecommands.h"
#include "table/future_SortDialog.h"
#include "core/column/Column.h"
#include "core/AbstractFilter.h"
#include "core/datatypes/String2DoubleFilter.h"
#include "core/datatypes/Double2StringFilter.h"
#include "core/datatypes/DateTime2StringFilter.h"
#include "core/datatypes/String2DayOfWeekFilter.h"
#include "core/datatypes/String2MonthFilter.h"
#include "core/datatypes/Double2DateTimeFilter.h"
#include "core/datatypes/Double2MonthFilter.h"
#include "core/datatypes/Double2DayOfWeekFilter.h"
#include "core/datatypes/String2DateTimeFilter.h"
#include "core/datatypes/DateTime2DoubleFilter.h"
#include "core/datatypes/SimpleCopyThroughFilter.h"
#include "TeXTableExportDialog.h"
#include "TeXTableSettings.h"

#include "ui_DimensionsDialog.h"
#include <QEvent>
#include <QPointer>
#include <functional>

#define WAIT_CURSOR QApplication::setOverrideCursor(QCursor(Qt::WaitCursor))
#define RESET_CURSOR QApplication::restoreOverrideCursor()

namespace future {

namespace {
class TableLangChangeFilter : public QObject
{
public:
    std::function<void()> onLanguageChange;
    QPointer<QMenu> projectMenu;
    QPointer<QMenu> submenuSetAs;
    QPointer<QMenu> submenuFill;
    explicit TableLangChangeFilter(QObject *parent) : QObject(parent)
    {
        setObjectName(QStringLiteral("_tg_table_lang_flt"));
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event && event->type() == QEvent::LanguageChange && onLanguageChange
            && (watched == parent() || watched == projectMenu.data()))
            onLanguageChange();
        return QObject::eventFilter(watched, event);
    }
};

TableLangChangeFilter *ensureTableLangFilter(Table *tbl, std::function<void()> cb)
{
    auto *flt = tbl->findChild<TableLangChangeFilter *>(QStringLiteral("_tg_table_lang_flt"),
                                                        Qt::FindDirectChildrenOnly);
    if (!flt) {
        flt = new TableLangChangeFilter(tbl);
        flt->onLanguageChange = std::move(cb);
        tbl->installEventFilter(flt);
    }
    return flt;
}

void registerTableFutureTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("Copy with &Headers", "ヘッダを含めてコピー(&H)");
        reg("Fill &Down", "下へフィル(&D)");
        reg("&Find and Replace...", "検索と置換(&F)...");
        reg("Re-&import from Source", "元ファイルから再読込(&I)");
    });
}

QString quoteTsvCell(const QString &raw)
{
    const bool needsQuotes = raw.contains(QLatin1Char('\t')) || raw.contains(QLatin1Char('\n'))
            || raw.contains(QLatin1Char('\r')) || raw.contains(QLatin1Char('"'));
    if (!needsQuotes)
        return raw;
    QString esc = raw;
    esc.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QLatin1Char('"') + esc + QLatin1Char('"');
}
} // anonymous namespace

bool Table::d_default_comment_visibility = true;
int Table::default_column_width = 120;

// TODO: move all selection related stuff to the primary view ?

#ifndef LEGACY_CODE_0_2_x
Table::Table(AbstractScriptingEngine *engine, int rows, int columns, const QString &name)
    : AbstractPart(name), d_plot_menu(0), scripted(engine), d_table_private(*this)
#else
Table::Table(int rows, int columns, const QString &name)
    : AbstractPart(name), d_plot_menu(0), d_table_private(*this)
#endif
{
    // set initial number of rows and columns
    QList<Column *> cols;
    for (int i = 0; i < columns; i++) {
        Column *new_col = new Column(QString::number(i + 1), SciDAVis::ColumnMode::Numeric);
        new_col->setPlotDesignation(i == 0 ? SciDAVis::X : SciDAVis::Y);
        cols << new_col;
    }
    appendColumns(cols);
    setRowCount(rows);

    d_view = NULL;
    createActions();
    connectActions();
}

#ifndef LEGACY_CODE_0_2_x
Table::Table() : AbstractPart("temp"), scripted(0)
#else
Table::Table() : AbstractPart("temp"), d_table_private(*this)
#endif
{
    d_view = NULL;
    createActions();
}

Table::~Table()
{
    if (d_view && !d_view->parentWidget()) {
        TableView *detachedView = d_view.data();
        d_view.clear();
        detachedView->hide();
        delete detachedView;
    }
}

Column *Table::column(int index) const
{
    return d_table_private.column(index);
}

Column *Table::currentColumn() const
{
    return d_table_private.currentColumn();
}

bool Table::setCurrentColumn(int index)
{
    return d_table_private.setCurrentColumn(index);
}

Column *Table::column(const QString &name, bool legacy_kludge) const
{
    // Pass 1: exact column name match always wins (prevents "col_1" matching "1")
    for (int i = 0; i < columnCount(); i++) {
        Column *col = d_table_private.column(i);
        if (col && col->name() == name)
            return col;
    }

    // Pass 2: legacy "TableName_ColName" prefix stripping only when no exact match exists
    if (legacy_kludge) {
        if (!this->name().isEmpty() && name.startsWith(this->name() + QLatin1Char('_'))) {
            const QString prefixStripped = name.mid(this->name().length() + 1);
            for (int i = 0; i < columnCount(); i++) {
                Column *col = d_table_private.column(i);
                if (col && col->name() == prefixStripped)
                    return col;
            }
        }
        const int pos = name.indexOf(QLatin1Char('_'));
        if (pos >= 0) {
            const QString label = name.mid(pos + 1);
            for (int i = 0; i < columnCount(); i++) {
                Column *col = d_table_private.column(i);
                if (col && col->name() == label)
                    return col;
            }
        }
    }

    // OriginPro-compatible fallback for formula lookup (!legacy_kludge):
    // match Long Name or Excel/Origin column letter ("A"->0, "B"->1, ...) or 1-based index
    if (!legacy_kludge && !name.isEmpty()) {
        for (int i = 0; i < columnCount(); i++) {
            Column *col = d_table_private.column(i);
            if (!col->longName().isEmpty() && col->longName() == name)
                return col;
        }
        bool allUpperAlpha = true;
        int alphaIdx = 0;
        for (int k = 0; k < name.size(); ++k) {
            const QChar ch = name.at(k);
            if (ch < QLatin1Char('A') || ch > QLatin1Char('Z')) {
                allUpperAlpha = false;
                break;
            }
            alphaIdx = alphaIdx * 26 + (ch.unicode() - QLatin1Char('A').unicode() + 1);
        }
        if (allUpperAlpha && alphaIdx >= 1 && alphaIdx <= columnCount())
            return d_table_private.column(alphaIdx - 1);

        bool okNum = false;
        const int oneBased = name.toInt(&okNum);
        if (okNum && oneBased >= 1 && oneBased <= columnCount())
            return d_table_private.column(oneBased - 1);
    }

    return NULL;
}

void Table::setView(TableView *view)
{
    d_view = view;
    if (!d_view)
        return;
    addActionsToView();
    if (d_view)
        d_view->showComments(d_default_comment_visibility);
}

QWidget *Table::view()
{
#ifndef LEGACY_CODE_0_2_x
    if (!d_view)
        setView(new TableView(this));
#endif
    return d_view;
}

void Table::insertColumns(int before, QList<Column *> new_cols)
{
    if (new_cols.size() < 1 || before < 0 || before > columnCount())
        return;
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: insert %2 column(s)").arg(name()).arg(new_cols.size()));
    int pos = before;
    foreach (Column *col, new_cols)
        insertChild(col, pos++);
    // remark: the TableInsertColumnsCmd will be created in completeAspectInsertion()
    endMacro();
    RESET_CURSOR;
}

void Table::removeColumns(int first, int count)
{
    if (count < 1 || first < 0 || first + count > columnCount())
        return;
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: remove %2 column(s)").arg(name()).arg(count));
    QList<Column *> cols;
    for (int i = first; i < (first + count); i++)
        cols.append(d_table_private.column(i));
    // remark:  the TableRemoveColumnsCmd will be created in prepareAspectRemoval()
    foreach (Column *col, cols)
        removeChild(col);
    endMacro();
    RESET_CURSOR;
}

void Table::removeColumn(Column *col)
{
    removeColumns(columnIndex(col), 1);
}

void Table::removeRows(int first, int count)
{
    if (count < 1 || first < 0 || first + count > rowCount())
        return;
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: remove %2 row(s)").arg(name()).arg(count));
    int end = d_table_private.columnCount();
    for (int col = 0; col < end; col++)
        d_table_private.column(col)->removeRows(first, count);
    exec(new TableSetNumberOfRowsCmd(d_table_private, d_table_private.rowCount() - count));
    endMacro();
    RESET_CURSOR;
}

void Table::insertRows(int before, int count)
{
    if (count < 1 || before < 0 || before > rowCount())
        return;
    WAIT_CURSOR;
    int new_row_count = rowCount() + count;
    beginMacro(QObject::tr("%1: insert %2 row(s)").arg(name()).arg(count));
    int end = d_table_private.columnCount();
    for (int col = 0; col < end; col++)
        d_table_private.column(col)->insertRows(before, count);
    setRowCount(new_row_count);
    endMacro();
    RESET_CURSOR;
}

void Table::setRowCount(int new_size)
{
    if ((new_size < 0) || (new_size == d_table_private.rowCount()))
        return;
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: set the number of rows to %2").arg(name()).arg(new_size));
    if (new_size < d_table_private.rowCount()) {
        int end = d_table_private.columnCount();
        for (int col = 0; col < end; col++) {
            Column *col_ptr = d_table_private.column(col);
            if (col_ptr->rowCount() > new_size)
                col_ptr->removeRows(new_size, col_ptr->rowCount() - new_size);
        }
    }
    exec(new TableSetNumberOfRowsCmd(d_table_private, new_size));
    endMacro();
    RESET_CURSOR;
}

int Table::columnCount() const
{
    return d_table_private.columnCount();
}

int Table::rowCount() const
{
    return d_table_private.rowCount();
}

int Table::columnCount(SciDAVis::PlotDesignation pd) const
{
    int count = 0;
    int cols = columnCount();
    for (int i = 0; i < cols; i++)
        if (column(i)->plotDesignation() == pd)
            count++;

    return count;
}

QStringList Table::nextSequentialColumnNames(int count) const
{
    QStringList out;
    if (count <= 0)
        return out;
    QSet<QString> existing;
    int maxNum = columnCount();
    for (int i = 0; i < columnCount(); ++i) {
        const Column *c = column(i);
        if (!c)
            continue;
        const QString nm = c->name();
        existing.insert(nm);
        bool ok = false;
        const int v = nm.toInt(&ok);
        if (ok && v > maxNum)
            maxNum = v;
    }
    int candidate = maxNum + 1;
    out.reserve(count);
    for (int k = 0; k < count; ++k) {
        while (existing.contains(QString::number(candidate)))
            ++candidate;
        const QString name = QString::number(candidate);
        existing.insert(name);
        out << name;
        ++candidate;
    }
    return out;
}

void Table::setColumnCount(int new_size)
{
    int old_size = columnCount();
    if (old_size == new_size || new_size < 0)
        return;

    WAIT_CURSOR;
    if (new_size < old_size)
        removeColumns(new_size, old_size - new_size);
    else {
        QList<Column *> cols;
        const QStringList names = nextSequentialColumnNames(new_size - old_size);
        for (const QString &nm : names) {
            Column *new_col = new Column(nm, SciDAVis::ColumnMode::Numeric);
            new_col->setPlotDesignation(SciDAVis::Y);
            cols << new_col;
        }
        appendColumns(cols);
    }
    RESET_CURSOR;
}

int Table::columnIndex(const Column *col) const
{
    return d_table_private.columnIndex(col);
}

void Table::clear()
{
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: clear").arg(name()));
    int cols = columnCount();
    for (int i = 0; i < cols; i++)
        column(i)->clear();
    endMacro();
    RESET_CURSOR;
}

void Table::clearMasks()
{
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: clear all masks").arg(name()));
    int cols = columnCount();
    for (int i = 0; i < cols; i++)
        column(i)->clearMasks();
    endMacro();
    RESET_CURSOR;
}

void Table::addColumn()
{
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: add column").arg(name()));
    setColumnCount(columnCount() + 1);
    endMacro();
    RESET_CURSOR;
}

void Table::addColumns()
{
    if (!d_view)
        return;
    WAIT_CURSOR;
    int count = d_view->selectedColumnCount(false);
    beginMacro(QObject::tr("%1: add %2 column(s)").arg(name()).arg(count));
    setColumnCount(columnCount() + count);
    endMacro();
    RESET_CURSOR;
}

void Table::cutSelection()
{
    if (!d_view)
        return;
    int first = d_view->firstSelectedRow();
    if (first < 0)
        return;

    WAIT_CURSOR;
    beginMacro(tr("%1: cut selected cell(s)").arg(name()));
    copySelection();
    clearSelectedCells();
    endMacro();
    RESET_CURSOR;
}

void Table::copySelection()
{
    if (!d_view)
        return;
    int first_col = d_view->firstSelectedColumn(false);
    if (first_col == -1)
        return;
    int last_col = d_view->lastSelectedColumn(false);
    if (last_col == -2)
        return;
    int first_row = d_view->firstSelectedRow(false);
    if (first_row == -1)
        return;
    int last_row = d_view->lastSelectedRow(false);
    if (last_row == -2)
        return;
    int cols = last_col - first_col + 1;
    int rows = last_row - first_row + 1;

    WAIT_CURSOR;
    QString output_str;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            Column *col_ptr = column(first_col + c);
            if (col_ptr && d_view->isCellSelected(first_row + r, first_col + c)) {
                QString cellOut;
                if (d_view->formulaModeActive()) {
                    cellOut = col_ptr->formula(first_row + r);
                } else if (col_ptr->dataType() == SciDAVis::TypeDouble) {
                    if ((col_ptr->isInvalid(first_row + r))
                        || (col_ptr->rowCount() <= first_row + r)) {
                        // Copy invalid / missing cells as empty string (""), not "-"
                    } else {
                        Double2StringFilter *out_fltr =
                                static_cast<Double2StringFilter *>(col_ptr->outputFilter());
                        // create a copy of current locale
                        QLocale noSeparators;
                        // we do not need separators on output!
                        noSeparators.setNumberOptions(noSeparators.numberOptions()
                                                      | QLocale::OmitGroupSeparator);
                        // convert using modified locale
                        cellOut = noSeparators.toString(col_ptr->valueAt(first_row + r),
                                                        out_fltr->numericFormat(),
                                                        16); // copy with max. precision
                    }
                } else {
                    if (!col_ptr->isInvalid(first_row + r)
                        && col_ptr->rowCount() > first_row + r) {
                        cellOut = text(first_row + r, first_col + c);
                    }
                }
                output_str += quoteTsvCell(cellOut);
            }
            if (c < cols - 1)
                output_str += "\t";
        }
        if (r < rows - 1)
            output_str += "\n";
    }
    QApplication::clipboard()->setText(output_str);
    RESET_CURSOR;
}

void Table::copySelectionWithHeaders()
{
    if (!d_view)
        return;
    int first_col = d_view->firstSelectedColumn(false);
    int last_col = d_view->lastSelectedColumn(false);
    int first_row = d_view->firstSelectedRow(false);
    int last_row = d_view->lastSelectedRow(false);
    const bool copyAll = (d_view->firstSelectedColumn(false) < 0 || d_view->firstSelectedRow(false) < 0);
    if (copyAll || last_col < first_col) {
        if (columnCount() <= 0)
            return;
        first_col = 0;
        last_col = columnCount() - 1;
        first_row = 0;
        last_row = rowCount() - 1;
    }

    WAIT_CURSOR;
    QStringList lines;
    QStringList nameRow;
    QStringList unitRow;
    bool anyUnit = false;
    for (int c = first_col; c <= last_col; ++c) {
        Column *col_ptr = column(c);
        const QString ln = col_ptr ? col_ptr->longName().trimmed() : QString();
        const QString hdr = !ln.isEmpty() ? ln : (col_ptr ? col_ptr->name() : QString());
        nameRow << quoteTsvCell(hdr);
        const QString u = col_ptr ? col_ptr->units().trimmed() : QString();
        if (!u.isEmpty())
            anyUnit = true;
        unitRow << quoteTsvCell(u);
    }
    lines << nameRow.join(QLatin1Char('\t'));
    if (anyUnit)
        lines << unitRow.join(QLatin1Char('\t'));

    QLocale noSeparators;
    noSeparators.setNumberOptions(noSeparators.numberOptions() | QLocale::OmitGroupSeparator);
    const bool formulaMode = d_view->formulaModeActive();
    for (int r = first_row; r <= last_row; ++r) {
        QStringList rowCells;
        for (int c = first_col; c <= last_col; ++c) {
            Column *col_ptr = column(c);
            QString cellOut;
            if (col_ptr && (copyAll || d_view->isCellSelected(r, c))) {
                if (formulaMode) {
                    cellOut = col_ptr->formula(r);
                } else if (col_ptr->dataType() == SciDAVis::TypeDouble) {
                    if (!col_ptr->isInvalid(r) && col_ptr->rowCount() > r) {
                        Double2StringFilter *out_fltr =
                                static_cast<Double2StringFilter *>(col_ptr->outputFilter());
                        cellOut = noSeparators.toString(col_ptr->valueAt(r),
                                                        out_fltr->numericFormat(), 16);
                    }
                } else if (!col_ptr->isInvalid(r) && col_ptr->rowCount() > r) {
                    cellOut = text(r, c);
                }
            }
            rowCells << quoteTsvCell(cellOut);
        }
        lines << rowCells.join(QLatin1Char('\t'));
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    RESET_CURSOR;
}

void Table::fillSelectionDown()
{
    if (::Table *legacy = qobject_cast<::Table *>(d_view.data()))
        legacy->fillSelectionDown();
}

void Table::showFindReplaceDialog()
{
    if (::Table *legacy = qobject_cast<::Table *>(d_view.data()))
        legacy->showFindReplaceDialog(true);
}

void Table::reimportFromSource()
{
    if (::Table *legacy = qobject_cast<::Table *>(d_view.data()))
        legacy->reimportFromSource();
}

void Table::pasteIntoSelection(const bool isTransposed)
{
    if (!d_view)
        return;
    // Menu / context-menu paste and Ctrl+V share one implementation (::Table::pasteText):
    // tab first, leading empty cells kept, header/unit rows promoted, column-wise bulk writes
    // under one Undo step.
    if (::Table *legacy = qobject_cast<::Table *>(d_view.data())) {
        legacy->pasteSelection(isTransposed);
        return;
    }
    if (columnCount() < 1 || rowCount() < 1)
        return;

    WAIT_CURSOR;
    beginMacro(tr("%1: paste from clipboard").arg(name()));

    int first_col = d_view->firstSelectedColumn(false);
    int last_col = d_view->lastSelectedColumn(false);
    int first_row = d_view->firstSelectedRow(false);
    int last_row = d_view->lastSelectedRow(false);
    int input_row_count = 0;
    int input_col_count = 0;
    int rows, cols;

    const QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    if (mimeData->hasText()) {
        QString input_str = clipboard->text().trimmed();
        QList<QStringList> cell_texts;
        QStringList input_rows(input_str.split(QRegExp("\\n|\\r\\n|\\r")));
        for (int i = 0; i < input_rows.count(); i++) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            auto cells = input_rows.at(i).trimmed().split(QRegExp("( +|\\s)"), Qt::KeepEmptyParts);
#else
            auto cells = input_rows.at(i).trimmed().split(QRegExp("( +|\\s)"), QString::KeepEmptyParts);
#endif
            if (isTransposed) {
                for (; cells.count() > cell_texts.count();)
                    cell_texts.append(QStringList());
                auto jj = 0;
                for (const auto &cell : cells)
                    cell_texts[jj++].append(cell);
            } else
                cell_texts.append(cells);
        }
        input_row_count = cell_texts.count();
        input_col_count = std::max_element(cell_texts.cbegin(), cell_texts.cend(),
                                           [](const QStringList &a, const QStringList &b) {
                                               return a.count() > b.count();
                                           })
                                  ->count();
        if ((first_col == -1 || first_row == -1)
            || (last_row == first_row && last_col == first_col))
        // if the is no selection or only one cell selected, the
        // selection will be expanded to the needed size from the current cell
        {
            int current_row, current_col;
            d_view->getCurrentCell(&current_row, &current_col);
            if (current_row == -1)
                current_row = 0;
            if (current_col == -1)
                current_col = 0;
            d_view->setCellSelected(current_row, current_col);
            first_col = current_col;
            first_row = current_row;
            last_row = first_row + input_row_count - 1;
            last_col = first_col + input_col_count - 1;
            // resize the table if necessary
            if (last_col >= columnCount()) {
                QList<Column *> cols;
                const QStringList names = nextSequentialColumnNames(last_col + 1 - columnCount());
                for (const QString &nm : names) {
                    Column *new_col = new Column(nm, SciDAVis::ColumnMode::Numeric);
                    new_col->setPlotDesignation(SciDAVis::Y);
                    cols << new_col;
                }
                appendColumns(cols);
            }
            if (last_row >= rowCount())
                appendRows(last_row + 1 - rowCount());
            // select the rectangle to be pasted in
            d_view->setCellsSelected(first_row, first_col, last_row, last_col);
        }

        rows = last_row - first_row + 1;
        cols = last_col - first_col + 1;
        if ((d_view->formulaModeActive()) || (d_view->hasMultiSelection())) {
            for (int r = 0; r < rows && r < input_row_count; r++) {
                for (int c = 0; c < cols && c < input_col_count; c++) {
                    if (d_view->isCellSelected(first_row + r, first_col + c)
                        && (c < cell_texts.at(r).count())) {
                        Column *col_ptr = d_table_private.column(first_col + c);
                        col_ptr->setFormula(first_row + r, cell_texts.at(r).at(c));
                        col_ptr->setInvalid(first_row + r, false);
                    }
                }
            }
        } else {
            QList<QStringList> cols_texts;
            // transpose clipboard data to use replaceTexts
            for (int c = 0; c < input_col_count; c++) {
                QStringList cur_column;
                for (int r = 0; r < input_row_count; r++) {
                    if (c < cell_texts.at(r).size()) {
                        cur_column << cell_texts.at(r).at(c);
                    } else {
                        cur_column << QString();
                    }
                }
                cols_texts << cur_column;
            }

            auto &settings = ApplicationWindow::getSettings();
            bool convertToTextColumn =
                    settings.value("/General/SetColumnTypeToTextOnInvalidInput", true).toBool();

            for (int c = 0; c < cols && c < input_col_count; c++) {
                Column *col_ptr = d_table_private.column(first_col + c);
                if (convertToTextColumn && (col_ptr->columnMode() == SciDAVis::ColumnMode::Numeric)) {
                    auto filter = dynamic_cast<String2DoubleFilter *>(col_ptr->inputFilter());
                    if (nullptr != filter)
                        for (const auto &value : cols_texts.at(c))
                            if (("-" != value) && (filter->isInvalid(value))) {
                                col_ptr->setColumnMode(SciDAVis::ColumnMode::Text);
                                break;
                            }
                }
                col_ptr->asStringColumn()->replaceTexts(first_row, cols_texts.at(c).mid(0, rows));
            }
        }

        recalculateSelectedCells();
    }
    endMacro();
    RESET_CURSOR;
}

void Table::pasteIntoSelectionTransposed()
{
    return pasteIntoSelection(true);
}

void Table::maskSelection()
{
    if (!d_view)
        return;
    int first = d_view->firstSelectedRow();
    int last = d_view->lastSelectedRow();
    if (first < 0)
        return;

    WAIT_CURSOR;
    beginMacro(tr("%1: mask selected cell(s)").arg(name()));
    QList<Column *> list = d_view->selectedColumns();
    foreach (Column *col_ptr, list) {
        int col = columnIndex(col_ptr);
        for (int row = first; row <= last; row++)
            if (d_view->isCellSelected(row, col))
                col_ptr->setMasked(row);
    }
    endMacro();
    RESET_CURSOR;
}

void Table::unmaskSelection()
{
    if (!d_view)
        return;
    int first = d_view->firstSelectedRow();
    int last = d_view->lastSelectedRow();
    if (first < 0)
        return;

    WAIT_CURSOR;
    beginMacro(tr("%1: unmask selected cell(s)").arg(name()));
    QList<Column *> list = d_view->selectedColumns();
    foreach (Column *col_ptr, list) {
        int col = columnIndex(col_ptr);
        for (int row = first; row <= last; row++)
            if (d_view->isCellSelected(row, col))
                col_ptr->setMasked(row, false);
    }
    endMacro();
    RESET_CURSOR;
}

void Table::setFormulaForSelection()
{
    if (!d_view)
        return;
    d_view->showControlFormulaTab();
}

void Table::recalculateSelectedCells()
{
    if (!d_view)
        return;
#ifdef LEGACY_CODE_0_2_x
    WAIT_CURSOR;
    beginMacro(tr("%1: apply formula to selection").arg(name()));
    emit recalculate();
    endMacro();
    RESET_CURSOR;
#endif
}

void Table::fillSelectedCellsWithRowNumbers()
{
    if (!d_view)
        return;
    if (d_view->selectedColumnCount() < 1)
        return;
    int first = d_view->firstSelectedRow();
    int last = d_view->lastSelectedRow();
    if (first < 0)
        return;

    WAIT_CURSOR;
    beginMacro(tr("%1: fill cells with row numbers").arg(name()));
    foreach (Column *col_ptr, d_view->selectedColumns()) {
        int col = columnIndex(col_ptr);
        switch (col_ptr->columnMode()) {
        case SciDAVis::ColumnMode::Numeric: {
            QVector<qreal> results(last - first + 1);
            for (int row = first; row <= last; row++)
                if (d_view->isCellSelected(row, col))
                    results[row - first] = row + 1;
                else
                    results[row - first] = col_ptr->valueAt(row);
            col_ptr->replaceValues(first, results);
            break;
        }
        case SciDAVis::ColumnMode::Text: {
            QStringList results;
            for (int row = first; row <= last; row++)
                if (d_view->isCellSelected(row, col))
                    results << QString::number(row + 1);
                else
                    results << col_ptr->textAt(row);
            col_ptr->replaceTexts(first, results);
            break;
        }
        default:
            break;
        }
    }
    endMacro();
    RESET_CURSOR;
}

void Table::fillSelectedCellsWithRandomNumbers()
{
    if (!d_view)
        return;
    if (d_view->selectedColumnCount() < 1)
        return;
    int first = d_view->firstSelectedRow();
    int last = d_view->lastSelectedRow();
    if (first < 0)
        return;

    WAIT_CURSOR;
    beginMacro(tr("%1: fill cells with random values").arg(name()));
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    QRandomGenerator(QTime::currentTime().msec());
#else
    qsrand(QTime::currentTime().msec());
#endif
    foreach (Column *col_ptr, d_view->selectedColumns()) {
        int col = columnIndex(col_ptr);
        switch (col_ptr->columnMode()) {
        case SciDAVis::ColumnMode::Numeric: {
            QVector<qreal> results(last - first + 1);
            for (int row = first; row <= last; row++)
                if (d_view->isCellSelected(row, col))
                    results[row - first] =
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
                            QRandomGenerator::global()->generateDouble();
#else
                            double(qrand()) / double(RAND_MAX);
#endif
                else
                    results[row - first] = col_ptr->valueAt(row);
            col_ptr->replaceValues(first, results);
            break;
        }
        case SciDAVis::ColumnMode::Text: {
            QStringList results;
            for (int row = first; row <= last; row++)
                if (d_view->isCellSelected(row, col))
                    results << QString::number(
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
                            QRandomGenerator::global()->generateDouble());
#else
                            double(qrand()) / double(RAND_MAX));
#endif
                else
                    results << col_ptr->textAt(row);
            col_ptr->replaceTexts(first, results);
            break;
        }
        case SciDAVis::ColumnMode::DateTime:
        case SciDAVis::ColumnMode::Month:
        case SciDAVis::ColumnMode::Day: {
            QList<QDateTime> results;
            QDate earliestDate(1, 1, 1);
            QDate latestDate(2999, 12, 31);
            QTime midnight(0, 0, 0, 0);
            for (int row = first; row <= last; row++)
                if (d_view->isCellSelected(row, col))
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
                    results << QDateTime(
                            earliestDate.addDays(QRandomGenerator::global()->generateDouble()
                                                 * ((double)earliestDate.daysTo(latestDate))),
                            midnight.addMSecs(QRandomGenerator::global()->generate64() * 1000 * 60
                                              * 60 * 24 / RAND_MAX));
#else
                    results << QDateTime(
                            earliestDate.addDays(((double)qrand())
                                                 * ((double)earliestDate.daysTo(latestDate))
                                                 / ((double)RAND_MAX)),
                            midnight.addMSecs(((qint64)qrand()) * 1000 * 60 * 60 * 24 / RAND_MAX));
#endif
                else
                    results << col_ptr->dateTimeAt(row);
            col_ptr->replaceDateTimes(first, results);
            break;
        }
        }
    }
    endMacro();
    RESET_CURSOR;
}

void Table::sortTable()
{
    QList<Column *> cols;

    for (int i = 0; i < columnCount(); i++)
        cols.append(column(i));

    sortDialog(cols);
}

void Table::insertEmptyColumns()
{
    if (!d_view)
        return;
    int first = d_view->firstSelectedColumn();
    int last = d_view->lastSelectedColumn();
    if (first < 0)
        return;
    int count, current = first;
    QList<Column *> cols;

    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: insert empty column(s)").arg(name()));
    while (current <= last) {
        current = first + 1;
        while (current <= last && d_view->isColumnSelected(current))
            current++;
        count = current - first;
        const QStringList names = nextSequentialColumnNames(count);
        for (const QString &nm : names) {
            Column *new_col = new Column(nm, SciDAVis::ColumnMode::Numeric);
            new_col->setPlotDesignation(SciDAVis::Y);
            cols << new_col;
        }
        insertColumns(first, cols);
        cols.clear();
        current += count;
        last += count;
        while (current <= last && !d_view->isColumnSelected(current))
            current++;
        first = current;
    }
    endMacro();
    RESET_CURSOR;
}

void Table::removeSelectedColumns()
{
    if (!d_view)
        return;
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: remove selected column(s)").arg(name()));

    QList<Column *> list = d_view->selectedColumns();
    foreach (Column *ptr, list)
        removeColumn(ptr);

    endMacro();
    RESET_CURSOR;
}

void Table::clearSelectedColumns()
{
    if (!d_view)
        return;
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: clear selected column(s)").arg(name()));

    QList<Column *> list = d_view->selectedColumns();
    if (d_view->formulaModeActive()) {
        foreach (Column *ptr, list)
            ptr->clearFormulas();
    } else {
        foreach (Column *ptr, list)
            ptr->clear();
    }

    endMacro();
    RESET_CURSOR;
}

void Table::setSelectionAs(SciDAVis::PlotDesignation pd)
{
    if (!d_view)
        return;
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: set plot designation(s)").arg(name()));

    QList<Column *> list = d_view->selectedColumns();
    foreach (Column *ptr, list)
        ptr->setPlotDesignation(pd);

    endMacro();
    RESET_CURSOR;
}

void Table::setSelectedColumnsAsX()
{
    setSelectionAs(SciDAVis::X);
}

void Table::setSelectedColumnsAsY()
{
    setSelectionAs(SciDAVis::Y);
}

void Table::setSelectedColumnsAsZ()
{
    setSelectionAs(SciDAVis::Z);
}

void Table::setSelectedColumnsAsYError()
{
    setSelectionAs(SciDAVis::yErr);
}

void Table::setSelectedColumnsAsXError()
{
    setSelectionAs(SciDAVis::xErr);
}

void Table::setSelectedColumnsAsNone()
{
    setSelectionAs(SciDAVis::noDesignation);
}

namespace {
// Writes values[row] back only for the rows flagged in `write`, one replaceValues() per
// contiguous run, so the other rows keep both their value and their invalid flag (a single
// replaceValues(0, ...) over the whole column used to turn empty cells into valid zeros).
void normalizeReplaceRowRuns(Column *col, const QVector<qreal> &values, const QVector<bool> &write)
{
    const int n = values.size();
    int row = 0;
    while (row < n) {
        if (!write.at(row)) {
            ++row;
            continue;
        }
        int last = row;
        while (last + 1 < n && write.at(last + 1))
            ++last;
        col->replaceValues(row, values.mid(row, last - row + 1));
        row = last + 1;
    }
}
} // namespace

void Table::normalizeColumns(QList<Column *> cols)
{
    if (!d_view)
        return;

    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: normalize column(s)").arg(name()));
    foreach (Column *col, cols) {
        if (col->dataType() != SciDAVis::TypeDouble)
            continue;
        const int n = col->rowCount();
        // max of the valid (finite) values, seeded with the first one: starting from 0 turned
        // an all-negative column into zeros.
        bool haveMax = false;
        double max = 0.0;
        for (int row = 0; row < n; row++) {
            if (col->isInvalid(row))
                continue;
            const double v = col->valueAt(row);
            if (!std::isfinite(v))
                continue;
            if (!haveMax || v > max) {
                max = v;
                haveMax = true;
            }
        }
        if (!haveMax || max == 0.0) // nothing to scale / avoid division by zero
            continue;
        QVector<qreal> results(n);
        QVector<bool> write(n, false);
        for (int row = 0; row < n; row++) {
            if (col->isInvalid(row))
                continue; // empty cells stay empty
            results[row] = col->valueAt(row) / max;
            write[row] = true;
        }
        normalizeReplaceRowRuns(col, results, write);
    }
    endMacro();
    RESET_CURSOR;
}

void Table::normalizeSelectedColumns()
{
    normalizeColumns(d_view->selectedColumns());
}

void Table::normalizeSelection()
{
    if (!d_view)
        return;

    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: normalize selection").arg(name()));
    // max of the selected valid (finite) cells, seeded with the first one (see normalizeColumns)
    bool haveMax = false;
    double max = 0.0;
    foreach (Column *col_ptr, d_view->selectedColumns()) {
        int col = columnIndex(col_ptr);
        if (col_ptr->dataType() != SciDAVis::TypeDouble)
            continue;
        for (int row = 0; row < col_ptr->rowCount(); row++) {
            if (!d_view->isCellSelected(row, col) || col_ptr->isInvalid(row))
                continue;
            const double v = col_ptr->valueAt(row);
            if (!std::isfinite(v))
                continue;
            if (!haveMax || v > max) {
                max = v;
                haveMax = true;
            }
        }
    }

    if (haveMax && max != 0.0) // avoid division by zero
    {
        foreach (Column *col_ptr, d_view->selectedColumns()) {
            int col = columnIndex(col_ptr);
            if (col_ptr->dataType() != SciDAVis::TypeDouble)
                continue;
            // Only the column's own rows: sizing this by the table's rowCount() appended zeros
            // to shorter columns.
            const int n = col_ptr->rowCount();
            QVector<qreal> results(n);
            QVector<bool> write(n, false);
            for (int row = 0; row < n; row++) {
                if (!d_view->isCellSelected(row, col) || col_ptr->isInvalid(row))
                    continue;
                results[row] = col_ptr->valueAt(row) / max;
                write[row] = true;
            }
            normalizeReplaceRowRuns(col_ptr, results, write);
        }
    }
    endMacro();
    RESET_CURSOR;
}

void Table::sortSelectedColumns()
{
    if (!d_view)
        return;
    QList<Column *> cols = d_view->selectedColumns();
    sortDialog(cols);
}

void Table::statisticsOnSelectedColumns()
{
#ifdef LEGACY_CODE_0_2_x
    // TODO: this is only an ugly hack for 0.2.0
    emit requestColumnStatistics();
#endif
}

void Table::statisticsOnSelectedRows()
{
#ifdef LEGACY_CODE_0_2_x
    // TODO: this is only an ugly hack for 0.2.0
    emit requestRowStatistics();
#endif
}

void Table::insertEmptyRows()
{
    if (!d_view)
        return;
    int first = d_view->firstSelectedRow();
    int last = d_view->lastSelectedRow();
    int count, current = first;

    if (first < 0)
        return;

    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: insert empty rows(s)").arg(name()));
    while (current <= last) {
        current = first + 1;
        while (current <= last && d_view->isRowSelected(current))
            current++;
        count = current - first;
        insertRows(first, count);
        current += count;
        last += count;
        while (current <= last && !d_view->isRowSelected(current))
            current++;
        first = current;
    }
    endMacro();
    RESET_CURSOR;
}

void Table::removeSelectedRows()
{
    if (!d_view)
        return;

    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: remove selected rows(s)").arg(name()));
    // Remove from the bottom up so earlier removals do not shift the row indices of the
    // intervals that are still to be removed (multi-interval selections deleted wrong rows).
    QList<Interval<int>> rowIntervals = d_view->selectedRows().intervals();
    std::sort(rowIntervals.begin(), rowIntervals.end(),
              [](const Interval<int> &a, const Interval<int> &b) { return a.start() > b.start(); });
    for (int k = 0; k < rowIntervals.size(); ++k)
        removeRows(rowIntervals.at(k).start(), rowIntervals.at(k).size());
    endMacro();
    RESET_CURSOR;
}

void Table::clearSelectedCells()
{
    if (!d_view)
        return;
    int first = d_view->firstSelectedRow();
    // because this is not marked constant, we'd better call it.
    d_view->lastSelectedRow();
    if (first < 0)
        return;

    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: clear selected cell(s)").arg(name()));
    QList<Column *> list = d_view->selectedColumns();
    const bool formulaMode = d_view->formulaModeActive();
    foreach (Column *col_ptr, list) {
        const int colIdx = columnIndex(col_ptr);
        const int rows = col_ptr->rowCount();
        // Walk the *actually selected* cells of this column (not the union of the selected rows
        // of all columns, which erased unselected cells for non-rectangular selections) and
        // build one run per contiguous block, never beyond the column's own length.
        QList<Interval<int>> runs;
        int runStart = -1;
        for (int r = 0; r <= rows; ++r) {
            const bool sel = (r < rows) && d_view->isCellSelected(r, colIdx);
            if (sel && runStart < 0)
                runStart = r;
            else if (!sel && runStart >= 0) {
                runs.append(Interval<int>(runStart, r - 1));
                runStart = -1;
            }
        }
        // bottom-up so that removing a trailing run cannot shift the remaining ones
        for (int k = runs.size() - 1; k >= 0; --k) {
            const Interval<int> i = runs.at(k);
            if (formulaMode)
                col_ptr->setFormula(i, QString());
            else if (i.end() == rows - 1)
                col_ptr->removeRows(i.start(), i.size());
            else if (col_ptr->columnMode() == SciDAVis::ColumnMode::Text) {
                QStringList empties;
                for (int j = 0; j < i.size(); j++)
                    empties << QString();
                col_ptr->replaceTexts(i.start(), empties);
            } else
                col_ptr->setInvalid(i, true);
        }
    }
    endMacro();
    RESET_CURSOR;
}

void Table::editTypeAndFormatOfSelectedColumns()
{
    if (!d_view)
        return;
    d_view->showControlTypeTab();
}

void Table::editDescriptionOfCurrentColumn()
{
    if (!d_view)
        return;
    d_view->showControlDescriptionTab();
}

void Table::editColumnLabelsAndFormula()
{
    int col = -1;
    if (d_view)
        col = d_view->firstSelectedColumn(false);
    if (col < 0 && currentColumn())
        col = columnIndex(currentColumn());
    if (col < 0 && columnCount() > 0)
        col = 0;
    emit requestColumnPropertiesDialog(col);
}

void Table::addRows()
{
    if (!d_view)
        return;
    WAIT_CURSOR;
    int count = d_view->selectedRowCount(false);
    beginMacro(QObject::tr("%1: add %2 rows(s)").arg(name()).arg(count));
    exec(new TableSetNumberOfRowsCmd(d_table_private, rowCount() + count));
    endMacro();
    RESET_CURSOR;
}

bool Table::fillProjectMenu(QMenu *menu)
{
    menu->setTitle(tr("&Table"));

    QMenu *submenuSetAs = new QMenu(tr("S&et Column(s) As"), menu);
    submenuSetAs->addAction(action_set_as_x);
    submenuSetAs->addAction(action_set_as_y);
    submenuSetAs->addAction(action_set_as_z);
    submenuSetAs->addSeparator();
    submenuSetAs->addAction(action_set_as_xerr);
    submenuSetAs->addAction(action_set_as_yerr);
    submenuSetAs->addSeparator();
    submenuSetAs->addAction(action_set_as_none);
    menu->addMenu(submenuSetAs);
    menu->addSeparator();

    QMenu *submenuFill = new QMenu(tr("Fi&ll Selection with"), menu);
    submenuFill->addAction(action_fill_row_numbers);
    submenuFill->addAction(action_fill_random);
    menu->addMenu(submenuFill);
    menu->addSeparator();

    connect(menu, SIGNAL(aboutToShow()), this, SLOT(adjustActionNames()));
    menu->addAction(action_edit_column_labels_formula);
    menu->addAction(action_toggle_comments);
    menu->addAction(action_toggle_tabbar);
    menu->addAction(action_formula_mode);
    menu->addAction(action_edit_description);
    menu->addAction(action_type_format);
    menu->addSeparator();
    menu->addAction(action_clear_table);
    menu->addAction(action_clear_masks);
    menu->addAction(action_sort_table);
    menu->addAction(action_reimport_source);
    menu->addSeparator();
    menu->addAction(action_set_formula);
    menu->addAction(action_recalculate);
    menu->addSeparator();
    menu->addAction(action_add_column);
    menu->addAction(action_dimensions_dialog);
    menu->addSeparator();
    menu->addAction(action_find_replace);
    menu->addAction(action_go_to_cell);

    translateActionsStrings();
    adjustActionNames();

    QPointer<Table> self(this);
    auto *flt = ensureTableLangFilter(this, [self]() {
        if (!self)
            return;
        self->translateActionsStrings();
        self->adjustActionNames();
        if (auto *f = self->findChild<TableLangChangeFilter *>(
                    QStringLiteral("_tg_table_lang_flt"), Qt::FindDirectChildrenOnly)) {
            if (f->projectMenu)
                f->projectMenu->setTitle(Table::tr("&Table"));
            if (f->submenuSetAs)
                f->submenuSetAs->setTitle(Table::tr("S&et Column(s) As"));
            if (f->submenuFill)
                f->submenuFill->setTitle(Table::tr("Fi&ll Selection with"));
        }
    });
    flt->projectMenu = menu;
    flt->submenuSetAs = submenuSetAs;
    flt->submenuFill = submenuFill;
    menu->removeEventFilter(flt);
    menu->installEventFilter(flt);

    ModernUiTheme::upgradeMenuIcons(menu);
    return true;
}

bool Table::fillProjectToolBar(QToolBar *bar)
{
    bar->addAction(action_dimensions_dialog);
    bar->addAction(action_add_column);
    bar->addAction(action_statistics_columns);
    bar->addAction(action_statistics_rows);

    return true;
}

QMenu *Table::createContextMenu() const
{
    QMenu *menu = AbstractPart::createContextMenu();
    Q_ASSERT(menu);
    menu->addSeparator();

    // TODO
    // Export to ASCII
    // Print --> maybe should go to AbstractPart::createContextMenu()
    // ----
    // Rename --> AbstractAspect::createContextMenu(); maybe call this "Properties" and include
    // changing comment/caption spec Duplicate --> AbstractPart::createContextMenu() Hide/Show -->
    // Do we need hiding of views (in addition to minimizing)? How do we avoid confusion with hiding
    // of Aspects? Activate ? Resize --> AbstractPart::createContextMenu()

    ModernUiTheme::upgradeMenuIcons(menu);
    return menu;
}

void Table::createActions()
{
    // selection related actions
    action_cut_selection = new QAction(IconLoader::duotoneIcon("cut"), tr("Cu&t"), this);
    actionManager()->addAction(action_cut_selection, "cut_selection");

    action_copy_selection = new QAction(IconLoader::duotoneIcon("copy"), tr("&Copy"), this);
    actionManager()->addAction(action_copy_selection, "copy_selection");

    action_copy_with_headers =
            new QAction(IconLoader::duotoneIcon("copy"), tr("Copy with &Headers"), this);
    actionManager()->addAction(action_copy_with_headers, "copy_with_headers");

    action_paste_into_selection =
            new QAction(IconLoader::duotoneIcon("paste"), tr("Past&e"), this);
    actionManager()->addAction(action_paste_into_selection, "paste_into_selection");

    action_paste_into_selection_transposed =
            new QAction(IconLoader::duotoneIcon("paste"), tr("Paste transposed"), this);
    actionManager()->addAction(action_paste_into_selection_transposed, "paste_into_selection_transposed");

    action_fill_down =
            new QAction(IconLoader::duotoneIcon("row-numbers"), tr("Fill &Down"), this);
    actionManager()->addAction(action_fill_down, "fill_down");

    action_find_replace =
            new QAction(IconLoader::duotoneIcon("find"), tr("&Find and Replace..."), this);
    actionManager()->addAction(action_find_replace, "find_replace");
    action_mask_selection =
            new QAction(IconLoader::duotoneIcon("mask-points"), tr("&Mask", "mask selection"), this);
    actionManager()->addAction(action_mask_selection, "mask_selection");

    action_unmask_selection = new QAction(IconLoader::duotoneIcon("unmask-points"),
                                          tr("&Unmask", "unmask selection"), this);
    actionManager()->addAction(action_unmask_selection, "unmask_selection");

    action_set_formula =
            new QAction(IconLoader::duotoneIcon("formula"), tr("Assign &Formula"), this);
    action_set_formula->setShortcut(tr("Alt+Q"));
    actionManager()->addAction(action_set_formula, "set_formula");

    action_clear_selection = new QAction(IconLoader::duotoneIcon("clear"),
                                         tr("Clea&r", "clear selection"), this);
    actionManager()->addAction(action_clear_selection, "clear_selection");

    action_recalculate =
            new QAction(IconLoader::duotoneIcon("recalc-now"), tr("Recalculate"), this);
    action_recalculate->setShortcut(tr("Ctrl+Return"));
    actionManager()->addAction(action_recalculate, "recalculate");

    action_fill_row_numbers =
            new QAction(IconLoader::duotoneIcon("row-numbers"), tr("Row Numbers"), this);
    actionManager()->addAction(action_fill_row_numbers, "fill_row_numbers");

    action_fill_random =
            new QAction(IconLoader::duotoneIcon("random-numbers"), tr("Random Values"), this);
    actionManager()->addAction(action_fill_random, "fill_random");

    // table related actions
    action_toggle_comments = new QAction(IconLoader::duotoneIcon("new-table"),
                                         tr("Show/Hide comments"), this);
    actionManager()->addAction(action_toggle_comments, "toggle_comments");

    action_toggle_tabbar = new QAction(IconLoader::duotoneIcon("settings"),
                                       tr("Show/Hide Controls"), this);
    action_toggle_tabbar->setShortcut(tr("F12"));
    actionManager()->addAction(action_toggle_tabbar, "toggle_tabbar");

    action_formula_mode = new QAction(IconLoader::duotoneIcon("formula"),
                                      tr("Formula Edit Mode"), this);
    action_formula_mode->setCheckable(true);
    actionManager()->addAction(action_formula_mode, "formula_mode");

    action_select_all =
            new QAction(IconLoader::duotoneIcon("table-drawer"), tr("Select All"), this);
    actionManager()->addAction(action_select_all, "select_all");

    action_add_column =
            new QAction(IconLoader::duotoneIcon("add-column"), tr("&Add Column"), this);
    action_add_column->setToolTip(tr("append a new column to the table"));
    actionManager()->addAction(action_add_column, "add_column");

    action_clear_table = new QAction(IconLoader::duotoneIcon("clear"),
                                     tr("Clear Table"), this);
    actionManager()->addAction(action_clear_table, "clear_table");

    action_export_to_TeX =
            new QAction(IconLoader::duotoneIcon("export-pdf"), tr("Export to TeX..."), this);
    actionManager()->addAction(action_export_to_TeX, "export_to_TeX");

    action_reimport_source = new QAction(IconLoader::duotoneIcon("recalc-now"),
                                         tr("Re-&import from Source"), this);
    action_reimport_source->setEnabled(!d_import_source_path.isEmpty());
    actionManager()->addAction(action_reimport_source, "reimport_source");

    action_clear_masks =
            new QAction(IconLoader::duotoneIcon("unmask-points"), tr("Clear Masks"), this);
    actionManager()->addAction(action_clear_masks, "clear_masks");

    action_sort_table =
            new QAction(IconLoader::duotoneIcon("sort-table"), tr("&Sort Table"), this);
    actionManager()->addAction(action_sort_table, "sort_table");

    action_go_to_cell =
            new QAction(IconLoader::duotoneIcon("find"), tr("&Go to Cell"), this);
    action_go_to_cell->setShortcut(tr("Ctrl+Alt+G"));
    actionManager()->addAction(action_go_to_cell, "go_to_cell");

    action_dimensions_dialog = new QAction(IconLoader::duotoneIcon("table-dimensions"),
                                           tr("&Dimensions", "table size"), this);
    action_dimensions_dialog->setToolTip(tr("change the table size"));
    actionManager()->addAction(action_dimensions_dialog, "dimensions_dialog");

    // column related actions
    action_insert_columns = new QAction(IconLoader::duotoneIcon("add-column"),
                                        tr("&Insert Empty Columns"), this);
    actionManager()->addAction(action_insert_columns, "insert_columns");

    action_remove_columns = new QAction(IconLoader::duotoneIcon("delete"),
                                        tr("Remo&ve Columns"), this);
    actionManager()->addAction(action_remove_columns, "remove_columns");

    action_clear_columns = new QAction(IconLoader::duotoneIcon("clear"),
                                       tr("Clea&r Columns"), this);
    actionManager()->addAction(action_clear_columns, "clear_columns");

    action_add_columns = new QAction(IconLoader::duotoneIcon("add-column"),
                                     tr("&Add Columns"), this);
    actionManager()->addAction(action_add_columns, "add_columns");

    action_set_as_x = new QAction(IconLoader::duotoneIcon("set_as_x"), tr("X", "plot designation"), this);
    actionManager()->addAction(action_set_as_x, "set_as_x");

    action_set_as_y = new QAction(IconLoader::duotoneIcon("set_as_y"), tr("Y", "plot designation"), this);
    actionManager()->addAction(action_set_as_y, "set_as_y");

    action_set_as_z = new QAction(IconLoader::duotoneIcon("set_as_z"), tr("Z", "plot designation"), this);
    actionManager()->addAction(action_set_as_z, "set_as_z");

    action_set_as_xerr = new QAction(IconLoader::duotoneIcon("error-bars"),
                                     tr("X Error", "plot designation"), this);
    actionManager()->addAction(action_set_as_xerr, "set_as_xerr");

    action_set_as_yerr = new QAction(IconLoader::duotoneIcon("error-bars"),
                                     tr("Y Error", "plot designation"), this);
    actionManager()->addAction(action_set_as_yerr, "set_as_yerr");

    action_set_as_none =
            new QAction(IconLoader::duotoneIcon("set_as_none"), tr("None", "plot designation"), this);
    actionManager()->addAction(action_set_as_none, "set_as_none");

    action_normalize_columns = new QAction(IconLoader::duotoneIcon("curve-normalize"),
                                           tr("&Normalize Columns"), this);
    actionManager()->addAction(action_normalize_columns, "normalize_columns");

    action_normalize_selection = new QAction(IconLoader::duotoneIcon("curve-normalize"),
                                             tr("&Normalize Selection"), this);
    actionManager()->addAction(action_normalize_selection, "normalize_selection");

    action_sort_columns =
            new QAction(IconLoader::duotoneIcon("sort-table"), tr("&Sort Columns"), this);
    actionManager()->addAction(action_sort_columns, "sort_columns");

    action_statistics_columns =
            new QAction(IconLoader::duotoneIcon("col-stats"), tr("Column Statisti&cs"), this);
    action_statistics_columns->setToolTip(tr("statistics on columns"));
    actionManager()->addAction(action_statistics_columns, "statistics_columns");

    action_type_format = new QAction(IconLoader::duotoneIcon("settings"),
                                     tr("Change &Type && Format"), this);
    action_type_format->setShortcut(tr("Ctrl+Alt+O"));
    actionManager()->addAction(action_type_format, "type_format");

    action_edit_description = new QAction(IconLoader::duotoneIcon("text-label"),
                                          tr("Edit Column &Description"), this);
    actionManager()->addAction(action_edit_description, "edit_description");
    action_edit_column_labels_formula = new QAction(
            IconLoader::duotoneIcon("text-label"),
            tr("Edit Column Labels && Formula (Long Name / Units / F(x))..."), this);
    actionManager()->addAction(action_edit_column_labels_formula, "edit_column_labels_formula");

    // row related actions
    action_insert_rows = new QAction(IconLoader::duotoneIcon("new-table"),
                                     tr("&Insert Empty Rows"), this);
    actionManager()->addAction(action_insert_rows, "insert_rows");

    action_remove_rows = new QAction(IconLoader::duotoneIcon("delete"),
                                     tr("Remo&ve Rows"), this);
    actionManager()->addAction(action_remove_rows, "remove_rows");

    action_clear_rows = new QAction(IconLoader::duotoneIcon("clear"),
                                    tr("Clea&r Rows"), this);
    actionManager()->addAction(action_clear_rows, "clear_rows");

    action_add_rows =
            new QAction(IconLoader::duotoneIcon("new-table"), tr("&Add Rows"), this);
    actionManager()->addAction(action_add_rows, "add_rows");

    action_statistics_rows =
            new QAction(IconLoader::duotoneIcon("row-stats"), tr("Row Statisti&cs"), this);
    action_statistics_rows->setToolTip(tr("statistics on rows"));
    actionManager()->addAction(action_statistics_rows, "statistics_rows");

    QPointer<Table> self(this);
    ensureTableLangFilter(this, [self]() {
        if (!self)
            return;
        self->translateActionsStrings();
        self->adjustActionNames();
        if (auto *f = self->findChild<TableLangChangeFilter *>(
                    QStringLiteral("_tg_table_lang_flt"), Qt::FindDirectChildrenOnly)) {
            if (f->projectMenu)
                f->projectMenu->setTitle(Table::tr("&Table"));
            if (f->submenuSetAs)
                f->submenuSetAs->setTitle(Table::tr("S&et Column(s) As"));
            if (f->submenuFill)
                f->submenuFill->setTitle(Table::tr("Fi&ll Selection with"));
        }
    });
}

void Table::connectActions()
{
    connect(action_cut_selection, SIGNAL(triggered()), this, SLOT(cutSelection()));
    connect(action_copy_selection, SIGNAL(triggered()), this, SLOT(copySelection()));
    connect(action_copy_with_headers, SIGNAL(triggered()), this, SLOT(copySelectionWithHeaders()));
    connect(action_paste_into_selection, SIGNAL(triggered()), this, SLOT(pasteIntoSelection()));
    connect(action_paste_into_selection_transposed, SIGNAL(triggered()), this, SLOT(pasteIntoSelectionTransposed()));
    connect(action_fill_down, SIGNAL(triggered()), this, SLOT(fillSelectionDown()));
    connect(action_find_replace, SIGNAL(triggered()), this, SLOT(showFindReplaceDialog()));
    connect(action_mask_selection, SIGNAL(triggered()), this, SLOT(maskSelection()));
    connect(action_unmask_selection, SIGNAL(triggered()), this, SLOT(unmaskSelection()));
    connect(action_set_formula, SIGNAL(triggered()), this, SLOT(setFormulaForSelection()));
    connect(action_clear_selection, SIGNAL(triggered()), this, SLOT(clearSelectedCells()));
    connect(action_recalculate, SIGNAL(triggered()), this, SLOT(recalculateSelectedCells()));
    connect(action_fill_row_numbers, SIGNAL(triggered()), this,
            SLOT(fillSelectedCellsWithRowNumbers()));
    connect(action_fill_random, SIGNAL(triggered()), this,
            SLOT(fillSelectedCellsWithRandomNumbers()));
    connect(action_select_all, SIGNAL(triggered()), this, SLOT(selectAll()));
    connect(action_add_column, SIGNAL(triggered()), this, SLOT(addColumn()));
    connect(action_clear_table, SIGNAL(triggered()), this, SLOT(clear()));

    // Export to TeX
    connect(action_export_to_TeX, SIGNAL(triggered()), this, SLOT(showTeXTableExportDialog()));
    connect(action_reimport_source, SIGNAL(triggered()), this, SLOT(reimportFromSource()));

    connect(action_clear_masks, SIGNAL(triggered()), this, SLOT(clearMasks()));
    connect(action_sort_table, SIGNAL(triggered()), this, SLOT(sortTable()));
    connect(action_go_to_cell, SIGNAL(triggered()), this, SLOT(goToCell()));
    connect(action_dimensions_dialog, SIGNAL(triggered()), this, SLOT(dimensionsDialog()));
    connect(action_insert_columns, SIGNAL(triggered()), this, SLOT(insertEmptyColumns()));
    connect(action_remove_columns, SIGNAL(triggered()), this, SLOT(removeSelectedColumns()));
    connect(action_clear_columns, SIGNAL(triggered()), this, SLOT(clearSelectedColumns()));
    connect(action_add_columns, SIGNAL(triggered()), this, SLOT(addColumns()));
    connect(action_set_as_x, SIGNAL(triggered()), this, SLOT(setSelectedColumnsAsX()));
    connect(action_set_as_y, SIGNAL(triggered()), this, SLOT(setSelectedColumnsAsY()));
    connect(action_set_as_z, SIGNAL(triggered()), this, SLOT(setSelectedColumnsAsZ()));
    connect(action_set_as_xerr, SIGNAL(triggered()), this, SLOT(setSelectedColumnsAsXError()));
    connect(action_set_as_yerr, SIGNAL(triggered()), this, SLOT(setSelectedColumnsAsYError()));
    connect(action_set_as_none, SIGNAL(triggered()), this, SLOT(setSelectedColumnsAsNone()));
    connect(action_normalize_columns, SIGNAL(triggered()), this, SLOT(normalizeSelectedColumns()));
    connect(action_normalize_selection, SIGNAL(triggered()), this, SLOT(normalizeSelection()));
    connect(action_sort_columns, SIGNAL(triggered()), this, SLOT(sortSelectedColumns()));
    connect(action_statistics_columns, SIGNAL(triggered()), this,
            SLOT(statisticsOnSelectedColumns()));
    connect(action_type_format, SIGNAL(triggered()), this,
            SLOT(editTypeAndFormatOfSelectedColumns()));
    connect(action_edit_description, SIGNAL(triggered()), this,
            SLOT(editDescriptionOfCurrentColumn()));
    connect(action_edit_column_labels_formula, SIGNAL(triggered()), this,
            SLOT(editColumnLabelsAndFormula()));
    connect(action_insert_rows, SIGNAL(triggered()), this, SLOT(insertEmptyRows()));
    connect(action_remove_rows, SIGNAL(triggered()), this, SLOT(removeSelectedRows()));
    connect(action_clear_rows, SIGNAL(triggered()), this, SLOT(clearSelectedCells()));
    connect(action_add_rows, SIGNAL(triggered()), this, SLOT(addRows()));
    connect(action_statistics_rows, SIGNAL(triggered()), this, SLOT(statisticsOnSelectedRows()));
}

void Table::addActionsToView()
{
    if (!d_view)
        return;
    QPointer<Table> self(this);
    auto *flt = ensureTableLangFilter(this, [self]() {
        if (!self)
            return;
        self->translateActionsStrings();
        self->adjustActionNames();
        if (auto *f = self->findChild<TableLangChangeFilter *>(
                    QStringLiteral("_tg_table_lang_flt"), Qt::FindDirectChildrenOnly)) {
            if (f->projectMenu)
                f->projectMenu->setTitle(Table::tr("&Table"));
            if (f->submenuSetAs)
                f->submenuSetAs->setTitle(Table::tr("S&et Column(s) As"));
            if (f->submenuFill)
                f->submenuFill->setTitle(Table::tr("Fi&ll Selection with"));
        }
    });
    if (d_view) {
        d_view->removeEventFilter(flt);
        d_view->installEventFilter(flt);
    }

    connect(action_toggle_comments, SIGNAL(triggered()), d_view, SLOT(toggleComments()));
    connect(action_toggle_tabbar, SIGNAL(triggered()), d_view, SLOT(toggleControlTabBar()));
    connect(action_formula_mode, SIGNAL(toggled(bool)), d_view, SLOT(activateFormulaMode(bool)));

    d_view->addAction(action_cut_selection);
    d_view->addAction(action_copy_selection);
    d_view->addAction(action_copy_with_headers);
    d_view->addAction(action_paste_into_selection);
    d_view->addAction(action_paste_into_selection_transposed);
    d_view->addAction(action_fill_down);
    d_view->addAction(action_find_replace);
    d_view->addAction(action_mask_selection);
    d_view->addAction(action_unmask_selection);
    d_view->addAction(action_set_formula);
    d_view->addAction(action_clear_selection);
    d_view->addAction(action_recalculate);
    d_view->addAction(action_fill_row_numbers);
    d_view->addAction(action_fill_random);
    d_view->addAction(action_toggle_comments);
    d_view->addAction(action_toggle_tabbar);
    d_view->addAction(action_formula_mode);
    d_view->addAction(action_select_all);
    d_view->addAction(action_add_column);
    d_view->addAction(action_clear_table);
    d_view->addAction(action_export_to_TeX);
    d_view->addAction(action_reimport_source);
    d_view->addAction(action_clear_masks);
    d_view->addAction(action_sort_table);
    d_view->addAction(action_go_to_cell);
    d_view->addAction(action_dimensions_dialog);
    d_view->addAction(action_insert_columns);
    d_view->addAction(action_remove_columns);
    d_view->addAction(action_clear_columns);
    d_view->addAction(action_add_columns);
    d_view->addAction(action_set_as_x);
    d_view->addAction(action_set_as_y);
    d_view->addAction(action_set_as_z);
    d_view->addAction(action_set_as_xerr);
    d_view->addAction(action_set_as_yerr);
    d_view->addAction(action_set_as_none);
    d_view->addAction(action_normalize_columns);
    d_view->addAction(action_normalize_selection);
    d_view->addAction(action_sort_columns);
    d_view->addAction(action_statistics_columns);
    d_view->addAction(action_type_format);
    d_view->addAction(action_edit_description);
    d_view->addAction(action_edit_column_labels_formula);
    d_view->addAction(action_insert_rows);
    d_view->addAction(action_remove_rows);
    d_view->addAction(action_clear_rows);
    d_view->addAction(action_add_rows);
    d_view->addAction(action_statistics_rows);
}

void Table::translateActionsStrings()
{
    action_cut_selection->setText(tr("Cu&t"));
    action_copy_selection->setText(tr("&Copy"));
    action_copy_with_headers->setText(tr("Copy with &Headers"));
    action_paste_into_selection->setText(tr("Past&e"));
    action_paste_into_selection_transposed->setText(tr("Paste transposed"));
    action_fill_down->setText(tr("Fill &Down"));
    action_find_replace->setText(tr("&Find and Replace..."));
    action_mask_selection->setText(tr("&Mask", "mask selection"));
    action_unmask_selection->setText(tr("&Unmask", "unmask selection"));
    action_clear_masks->setText(tr("Clear Masks"));
    action_set_formula->setText(tr("Assign &Formula"));
    action_clear_selection->setText(tr("Clea&r", "clear selection"));
    action_recalculate->setText(tr("Recalculate"));
    action_fill_row_numbers->setText(tr("Row Numbers"));
    action_fill_random->setText(tr("Random Values"));
    action_formula_mode->setText(tr("Formula Edit Mode"));
    action_select_all->setText(tr("Select All"));
    action_add_column->setText(tr("&Add Column"));
    action_clear_table->setText(tr("Clear Table"));
    action_export_to_TeX->setText(tr("Export to TeX..."));
    action_reimport_source->setText(tr("Re-&import from Source"));
    action_sort_table->setText(tr("&Sort Table"));
    action_go_to_cell->setText(tr("&Go to Cell"));
    action_dimensions_dialog->setText(tr("&Dimensions", "table size"));
    action_insert_columns->setText(tr("&Insert Empty Columns"));
    action_remove_columns->setText(tr("Remo&ve Columns"));
    action_clear_columns->setText(tr("Clea&r Columns"));
    action_add_columns->setText(tr("&Add Columns"));
    action_set_as_x->setText(tr("X", "plot designation"));
    action_set_as_y->setText(tr("Y", "plot designation"));
    action_set_as_z->setText(tr("Z", "plot designation"));
    action_set_as_xerr->setText(tr("X Error", "plot designation"));
    action_set_as_yerr->setText(tr("Y Error", "plot designation"));
    action_set_as_none->setText(tr("None", "plot designation"));
    action_normalize_columns->setText(tr("&Normalize Columns"));
    action_normalize_selection->setText(tr("&Normalize Selection"));
    action_sort_columns->setText(tr("&Sort Columns"));
    action_statistics_columns->setText(tr("Column Statisti&cs"));
    action_type_format->setText(tr("Change &Type && Format"));
    action_edit_description->setText(tr("Edit Column &Description"));
    action_edit_column_labels_formula->setText(
            tr("Edit Column Labels && Formula (Long Name / Units / F(x))..."));
    action_insert_rows->setText(tr("&Insert Empty Rows"));
    action_remove_rows->setText(tr("Remo&ve Rows"));
    action_clear_rows->setText(tr("Clea&r Rows"));
    action_add_rows->setText(tr("&Add Rows"));
    action_statistics_rows->setText(tr("Row Statisti&cs"));
    action_add_column->setToolTip(tr("append a new column to the table"));
    action_dimensions_dialog->setToolTip(tr("change the table size"));
    action_statistics_columns->setToolTip(tr("statistics on columns"));
    action_statistics_rows->setToolTip(tr("statistics on rows"));
    if (d_view) {
        adjustActionNames();
    } else {
        action_toggle_comments->setText(tr("Show/Hide comments"));
        action_toggle_tabbar->setText(tr("Show/Hide Controls"));
    }
}
void Table::showTableViewContextMenu(const QPoint &pos)
{
    if (!d_view)
        return;
    QMenu context_menu;

    // TODO: Does undo/redo really be belong into a context menu?
    //	context_menu.addAction(undoAction(&context_menu));
    //	context_menu.addAction(redoAction(&context_menu));

    if (d_plot_menu) {
        context_menu.addMenu(d_plot_menu);
        context_menu.addSeparator();
    }

    createSelectionMenu(&context_menu);
    context_menu.addSeparator();
    createTableMenu(&context_menu);
    context_menu.addSeparator();

    context_menu.exec(pos);
}

void Table::showTableViewColumnContextMenu(const QPoint &pos)
{
    if (!d_view)
        return;
    QMenu context_menu;

    // TODO: Does undo/redo really be belong into a context menu?
    //	context_menu.addAction(undoAction(&context_menu));
    //	context_menu.addAction(redoAction(&context_menu));

    if (d_plot_menu) {
        context_menu.addMenu(d_plot_menu);
        context_menu.addSeparator();
    }

    createColumnMenu(&context_menu);
    context_menu.addSeparator();

    context_menu.exec(pos);
}

void Table::showTableViewRowContextMenu(const QPoint &pos)
{
    if (!d_view)
        return;
    QMenu context_menu;

    // TODO: Does undo/redo really be belong into a context menu?
    //	context_menu.addAction(undoAction(&context_menu));
    //	context_menu.addAction(redoAction(&context_menu));

    createRowMenu(&context_menu);

    context_menu.exec(pos);
}

void Table::showTeXTableExportDialog()
{
    TeXTableExportDialog export_Dialog;

    export_Dialog.setFileMode(QFileDialog::AnyFile);
    export_Dialog.setNameFilter("*.tex");

    // Set the default file name by the name of the table
    export_Dialog.selectFile(name());

    export_Dialog.exec();

    if (export_Dialog.result() == QDialog::Accepted) {
        // Get file name
        QString fileName = export_Dialog.selectedFiles().first();

        // Add  file extention
        fileName += export_Dialog.selectedNameFilter().remove(0, 1);

        // Get TeX table settings
        TeXTableSettings tex_settings = export_Dialog.tex_TableSettings();

        // Export to TeX table
        export_to_TeX(fileName, tex_settings);
    }
}

bool Table::export_to_TeX(QString fileName, TeXTableSettings &tex_settings)
{

    WAIT_CURSOR;

    QFile file(fileName);

    if (!file.open(QIODevice::WriteOnly)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(0, tr("TeX Export Error"),
                              tr("Could not write to file: <br><h4>%1</h4><p>Please verify that "
                                 "you have the right to write to this location!")
                                      .arg(fileName));
        return false;
    }

    QList<Column *> columns_list;
    int first_row_index = 0;
    int last_row_index = 0;

    if (d_view->selectedColumnCount() != 0) {
        // Get selected columns
        for (int i = 0; i < columnCount(); i++) {
            if (d_view->isColumnSelected(i))
                columns_list << column(i);
        }

        // Get the first and last selected row index
        first_row_index = d_view->firstSelectedRow();
        last_row_index = d_view->lastSelectedRow();
    } else {
        // Get all columns
        for (int i = 0; i < columnCount(); i++)
            columns_list << column(i);

        // Get the first and last row index
        first_row_index = 0;
        if (rowCount() > 0)
            last_row_index = rowCount() - 1;
        else
            last_row_index = 0;
    }

    // Get the TeX column alignment
    char cl_alignment = 'c';
    if (tex_settings.columnsAlignment() == ALIGN_LEFT)
        cl_alignment = 'l';
    else if (tex_settings.columnsAlignment() == ALIGN_RIGHT)
        cl_alignment = 'r';

    QTextStream out(&file);

    // Check whether the TeX table should have caption
    if (tex_settings.with_caption())
        out << "\\begin{table} \n \\caption{" << name() << "}\n";

    // begin tabular with all parameters
    out << QString("\\begin{tabular}{|*{") + QString().setNum(columns_list.count()) + ("}{")
                    + QString(cl_alignment) + QString("|}}\n");
    out << "\\hline \n";

    // Check if export with table labels
    if (tex_settings.with_labels()) {
        // Get the columns labes
        QStringList columns_labels;
        foreach (Column *col, columns_list)
            columns_labels << col->name();
        out << columns_labels.join(" & ") << "\\\\ \\hline \n";
    }

    // Export TeX table content
    QStringList str_row;
    for (int row_index = first_row_index; row_index < last_row_index + 1; row_index++) {
        str_row.clear();
        foreach (Column *col, columns_list)
            str_row << col->asStringColumn()->textAt(row_index);
        out << str_row.join(" & ") << " \\\\ \\hline \n ";
    }

    out << "\\end{tabular} \n";

    if (tex_settings.with_caption())
        out << "\\end{table} \n";

    RESET_CURSOR;

    return true;
}

QMenu *Table::createSelectionMenu(QMenu *append_to)
{
    QMenu *menu = append_to;
    if (!menu)
        menu = new QMenu();

    QMenu *submenu = new QMenu(tr("Fi&ll Selection with"));
    submenu->addAction(action_fill_down);
    submenu->addSeparator();
    submenu->addAction(action_fill_row_numbers);
    submenu->addAction(action_fill_random);
    menu->addMenu(submenu);
    menu->addSeparator();

    menu->addAction(action_cut_selection);
    menu->addAction(action_copy_selection);
    menu->addAction(action_copy_with_headers);
    menu->addAction(action_paste_into_selection);
    menu->addAction(action_paste_into_selection_transposed);
    menu->addAction(action_fill_down);
    menu->addAction(action_clear_selection);
    menu->addSeparator();
    menu->addAction(action_find_replace);
    menu->addSeparator();
    menu->addAction(action_mask_selection);
    menu->addAction(action_unmask_selection);
    menu->addSeparator();
    menu->addAction(action_normalize_selection);
    menu->addSeparator();
    menu->addAction(action_set_formula);
    menu->addAction(action_recalculate);
    menu->addSeparator();
    ModernUiTheme::upgradeMenuIcons(menu);

    return menu;
}

namespace {
ApplicationWindow *findTableAppWindow(QWidget *viewWidget)
{
    for (QWidget *w = viewWidget; w; w = w->parentWidget()) {
        if (ApplicationWindow *app = qobject_cast<ApplicationWindow *>(w))
            return app;
    }
    const QWidgetList topWidgets = QApplication::topLevelWidgets();
    for (QWidget *w : topWidgets) {
        if (ApplicationWindow *app = qobject_cast<ApplicationWindow *>(w))
            return app;
    }
    return nullptr;
}

MultiLayer *findActiveOrRecentMultiLayer(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    if (QMdiSubWindow *act = app->d_workspace.activeSubWindow()) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(act))
            return ml;
    }
    const QList<QMdiSubWindow *> history =
            app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
    for (int i = history.size() - 1; i >= 0; --i) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(history.at(i)))
            return ml;
    }
    const QList<MyWidget *> wins = app->windowsList();
    for (int i = wins.size() - 1; i >= 0; --i) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(wins.at(i)))
            return ml;
    }
    return nullptr;
}
} // namespace

QMenu *Table::createColumnMenu(QMenu *append_to)
{
    QMenu *menu = append_to;
    if (!menu)
        menu = new QMenu();

    QAction *actAddActiveGraph = menu->addAction(tr("Add to Active Graph"));
    QAction *actAddActiveGraphRightY = menu->addAction(tr("Add to Active Graph (Right Y)"));
    menu->addSeparator();

    QPointer<QAction> actLeftGuard(actAddActiveGraph);
    QPointer<QAction> actRightGuard(actAddActiveGraphRightY);
    auto updateAddToGraphState = [this, actLeftGuard, actRightGuard]() {
        ApplicationWindow *app = findTableAppWindow(d_view);
        MultiLayer *ml = findActiveOrRecentMultiLayer(app);
        const bool canAdd = (app != nullptr && ml != nullptr && columnCount() > 0);
        if (actLeftGuard)
            actLeftGuard->setEnabled(canAdd);
        if (actRightGuard)
            actRightGuard->setEnabled(canAdd);
    };
    updateAddToGraphState();
    QObject::connect(menu, &QMenu::aboutToShow, this, updateAddToGraphState);

    auto triggerAddToActiveGraph = [this](bool rightY) {
        ApplicationWindow *app = findTableAppWindow(d_view);
        MultiLayer *targetMl = findActiveOrRecentMultiLayer(app);
        if (!app || !targetMl || columnCount() <= 0)
            return;

        QList<int> cols = d_view ? d_view->selectedColumnIndices(false) : QList<int>();
        const int curIdx = currentColumn() ? columnIndex(currentColumn()) : -1;
        if (cols.isEmpty() && curIdx >= 0)
            cols.append(curIdx);

        const QByteArray payload = TableView::buildColumnMimePayload(this, cols, curIdx);
        if (payload.isEmpty())
            return;

        QMimeData mimeData;
        mimeData.setData(QString::fromUtf8(MultiLayer::kColumnMimeType), payload);
        Graph *targetGraph =
                targetMl->activeGraph() ? targetMl->activeGraph() : targetMl->layer(1);
        const int targetAxisId = rightY ? QwtPlot::yRight : QwtPlot::yLeft;
        app->handleColumnMimeDrop(&mimeData, targetMl, targetGraph, targetAxisId, false);
    };
    QObject::connect(actAddActiveGraph, &QAction::triggered, this,
                     [triggerAddToActiveGraph]() { triggerAddToActiveGraph(false); });
    QObject::connect(actAddActiveGraphRightY, &QAction::triggered, this,
                     [triggerAddToActiveGraph]() { triggerAddToActiveGraph(true); });

    menu->addAction(action_edit_column_labels_formula);
    menu->addAction(action_copy_with_headers);
    menu->addSeparator();

    QMenu *submenu = new QMenu(tr("S&et Column(s) As"), menu);
    submenu->addAction(action_set_as_x);
    submenu->addAction(action_set_as_y);
    submenu->addAction(action_set_as_z);
    submenu->addSeparator();
    submenu->addAction(action_set_as_xerr);
    submenu->addAction(action_set_as_yerr);
    submenu->addSeparator();
    submenu->addAction(action_set_as_none);
    menu->addMenu(submenu);
    menu->addSeparator();

    submenu = new QMenu(tr("Fi&ll Selection with"), menu);
    submenu->addAction(action_fill_row_numbers);
    submenu->addAction(action_fill_random);
    menu->addMenu(submenu);
    menu->addSeparator();

    menu->addAction(action_insert_columns);
    menu->addAction(action_remove_columns);
    menu->addAction(action_clear_columns);
    menu->addAction(action_add_columns);
    menu->addSeparator();

    menu->addAction(action_normalize_columns);
    menu->addAction(action_sort_columns);
    menu->addSeparator();

    menu->addAction(action_edit_description);
    menu->addAction(action_type_format);
    connect(menu, SIGNAL(aboutToShow()), this, SLOT(adjustActionNames()));
    menu->addAction(action_toggle_comments);
    menu->addSeparator();

    menu->addAction(action_statistics_columns);
    ModernUiTheme::upgradeMenuIcons(menu);

    return menu;
}

QMenu *Table::createTableMenu(QMenu *append_to)
{
    QMenu *menu = append_to;
    if (!menu)
        menu = new QMenu();

    connect(menu, SIGNAL(aboutToShow()), this, SLOT(adjustActionNames()));
    QPointer<QAction> reimportAct(action_reimport_source);
    connect(menu, &QMenu::aboutToShow, this, [this, reimportAct]() {
        if (reimportAct)
            reimportAct->setEnabled(!d_import_source_path.isEmpty());
    });
    menu->addAction(action_edit_column_labels_formula);
    menu->addAction(action_toggle_comments);
    menu->addAction(action_toggle_tabbar);
    menu->addAction(action_formula_mode);
    menu->addSeparator();
    menu->addAction(action_select_all);
    menu->addAction(action_clear_table);
    menu->addAction(action_export_to_TeX);
    menu->addAction(action_clear_masks);
#ifndef LEGACY_CODE_0_2_x
    menu->addAction(action_sort_table);
#endif
    menu->addAction(action_reimport_source);
    menu->addSeparator();
    menu->addAction(action_add_column);
    menu->addSeparator();
    menu->addAction(action_find_replace);
    menu->addAction(action_go_to_cell);
    ModernUiTheme::upgradeMenuIcons(menu);

    return menu;
}

QMenu *Table::createRowMenu(QMenu *append_to)
{
    QMenu *menu = append_to;
    if (!menu)
        menu = new QMenu();

    menu->addAction(action_insert_rows);
    menu->addAction(action_remove_rows);
    menu->addAction(action_clear_rows);
    menu->addAction(action_add_rows);
    menu->addSeparator();
    QMenu *submenu = new QMenu(tr("Fi&ll Selection with"), menu);
    submenu->addAction(action_fill_row_numbers);
    submenu->addAction(action_fill_random);
    menu->addMenu(submenu);
    menu->addSeparator();
    menu->addAction(action_statistics_rows);
    ModernUiTheme::upgradeMenuIcons(menu);

    return menu;
}

void Table::goToCell()
{
    if (!d_view)
        return;
    bool ok;

    int col = QInputDialog::getInt(0, tr("Go to Cell"), tr("Enter column"), 1, 1, columnCount(), 1,
                                   &ok);
    if (!ok)
        return;

    int row = QInputDialog::getInt(0, tr("Go to Cell"), tr("Enter row"), 1, 1, rowCount(), 1, &ok);
    if (!ok)
        return;

    d_view->goToCell(row - 1, col - 1);
}

void Table::dimensionsDialog()
{
    Ui::DimensionsDialog ui;
    QDialog dialog;
    ui.setupUi(&dialog);
    dialog.setWindowTitle(tr("Set Table Dimensions"));
    ui.columnsSpinBox->setValue(columnCount());
    ui.rowsSpinBox->setValue(rowCount());
    connect(ui.buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));
    connect(ui.buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));

    if (dialog.exec()) {
        setColumnCount(ui.columnsSpinBox->value());
        setRowCount(ui.rowsSpinBox->value());
    }
}

void Table::moveColumn(int from, int to)
{
    beginMacro(tr("%1: move column %2 from position %3 to %4.")
                       .arg(name())
                       .arg(d_table_private.column(from)->name())
                       .arg(from + 1)
                       .arg(to + 1));
    moveChild(from, to);
    exec(new TableMoveColumnCmd(d_table_private, from, to));
    endMacro();
}

void Table::copy(Table *other)
{
    WAIT_CURSOR;
    beginMacro(QObject::tr("%1: copy %2").arg(name()).arg(other->name()));

    removeColumns(0, columnCount());
    QList<Column *> columns;
    for (int i = 0; i < other->columnCount(); i++) {
        Column *src_col = other->column(i);
        Column *new_col = new Column(src_col->name(), src_col->columnMode());
        new_col->copy(src_col);
        new_col->setPlotDesignation(src_col->plotDesignation());
        new_col->setLongName(src_col->longName());
        new_col->setUnits(src_col->units());
        new_col->setComment(src_col->comment());
        if (src_col->property("_tg_label_role").isValid())
            new_col->setProperty("_tg_label_role", src_col->property("_tg_label_role"));
        QList<Interval<int>> masks = src_col->maskedIntervals();
        foreach (Interval<int> iv, masks)
            new_col->setMasked(iv);
        QList<Interval<int>> formulas = src_col->formulaIntervals();
        foreach (Interval<int> iv, formulas)
            new_col->setFormula(iv, src_col->formula(iv.start()));
        columns.append(new_col);
    }
    appendColumns(columns);
    setImportSource(other->importSourcePath(), other->importSourceSheet(),
                    other->importSourceSeparator(), other->importSourceEncoding(),
                    other->importSourceIgnoredLines(), other->importSourceUnitsRow(),
                    other->importSourceColumnCount(), other->importSourceNumericLocale());
    setCaptionSpec(other->captionSpec());
    setComment(other->comment());
    for (int i = 0; i < columnCount(); i++)
        setColumnWidth(i, other->columnWidth(i));
    if (d_view)
        d_view->rereadSectionSizes();

    endMacro();
    RESET_CURSOR;
}

int Table::colX(int col)
{
    for (int i = col - 1; i >= 0; i--) {
        if (column(i)->plotDesignation() == SciDAVis::X)
            return i;
    }
    int cols = columnCount();
    for (int i = col + 1; i < cols; i++) {
        if (column(i)->plotDesignation() == SciDAVis::X)
            return i;
    }
    return -1;
}

int Table::colY(int col)
{
    int cols = columnCount();

    if (column(col)->plotDesignation() == SciDAVis::xErr
        || column(col)->plotDesignation() == SciDAVis::yErr) {
        // look to the left first
        for (int i = col - 1; i >= 0; i--) {
            if (column(i)->plotDesignation() == SciDAVis::Y)
                return i;
        }
        for (int i = col + 1; i < cols; i++) {
            if (column(i)->plotDesignation() == SciDAVis::Y)
                return i;
        }
    } else {
        // look to the right first
        for (int i = col + 1; i < cols; i++) {
            if (column(i)->plotDesignation() == SciDAVis::Y)
                return i;
        }
        for (int i = col - 1; i >= 0; i--) {
            if (column(i)->plotDesignation() == SciDAVis::Y)
                return i;
        }
    }
    return -1;
}

void Table::setPlotMenu(QMenu *menu)
{
    d_plot_menu = menu;
}

void Table::sortDialog(QList<Column *> cols)
{
    if (cols.isEmpty())
        return;

    SortDialog *sortd = new future::SortDialog();
    sortd->setAttribute(Qt::WA_DeleteOnClose);
    connect(sortd, SIGNAL(sort(Column *, QList<Column *>, bool)), this,
            SLOT(sortColumns(Column *, QList<Column *>, bool)));
    sortd->setColumnsList(cols);
    sortd->exec();
}

namespace {

template <typename T, typename Cmp, typename Getter>
QVector<int> buildTypedSortPermutation(int rows, Cmp cmp, Getter getter)
{
    QVector<QPair<T, int>> pairs;
    pairs.reserve(rows);
    for (int r = 0; r < rows; ++r)
        pairs.append(qMakePair(getter(r), r));
    std::stable_sort(pairs.begin(), pairs.end(), cmp);
    QVector<int> perm(rows);
    for (int k = 0; k < rows; ++k)
        perm[k] = pairs.at(k).second;
    return perm;
}

template <typename Setter>
void applyContiguousTrueRuns(const QVector<bool> &flags, Setter setter)
{
    const int n = flags.size();
    int i = 0;
    while (i < n) {
        if (!flags.at(i)) {
            ++i;
            continue;
        }
        const int start = i;
        while (i < n && flags.at(i))
            ++i;
        setter(Interval<int>(start, i - 1));
    }
}

struct SortComparators
{
    static bool doubleLess(const QPair<double, int> &a, const QPair<double, int> &b)
    {
        const bool na = std::isnan(a.first);
        const bool nb = std::isnan(b.first);
        if (na != nb)
            return !na; // NaN / invalid always goes to the end
        if (na)
            return false;
        return a.first < b.first;
    }
    static bool doubleGreater(const QPair<double, int> &a, const QPair<double, int> &b)
    {
        const bool na = std::isnan(a.first);
        const bool nb = std::isnan(b.first);
        if (na != nb)
            return !na; // NaN / invalid always goes to the end
        if (na)
            return false;
        return a.first > b.first;
    }
    static bool stringLess(const QPair<QString, int> &a, const QPair<QString, int> &b)
    {
        const bool ea = a.first.isEmpty();
        const bool eb = b.first.isEmpty();
        if (ea != eb)
            return !ea; // empty / invalid always goes to the end
        if (ea)
            return false;
        return a.first < b.first;
    }
    static bool stringGreater(const QPair<QString, int> &a, const QPair<QString, int> &b)
    {
        const bool ea = a.first.isEmpty();
        const bool eb = b.first.isEmpty();
        if (ea != eb)
            return !ea; // empty / invalid always goes to the end
        if (ea)
            return false;
        return a.first > b.first;
    }
    static bool dateTimeLess(const QPair<QDateTime, int> &a, const QPair<QDateTime, int> &b)
    {
        const bool ia = !a.first.isValid();
        const bool ib = !b.first.isValid();
        if (ia != ib)
            return !ia; // invalid / empty always goes to the end
        if (ia)
            return false;
        return a.first < b.first;
    }
    static bool dateTimeGreater(const QPair<QDateTime, int> &a, const QPair<QDateTime, int> &b)
    {
        const bool ia = !a.first.isValid();
        const bool ib = !b.first.isValid();
        if (ia != ib)
            return !ia; // invalid / empty always goes to the end
        if (ia)
            return false;
        return a.first > b.first;
    }
};

QVector<int> buildSortPermutation(const Column *keyCol, int rows, bool ascending)
{
    if (!keyCol || rows <= 0)
        return QVector<int>();
    const int keyRows = keyCol->rowCount();
    switch (keyCol->dataType()) {
    case SciDAVis::TypeDouble:
        return buildTypedSortPermutation<double>(
                rows, ascending ? SortComparators::doubleLess : SortComparators::doubleGreater,
                [keyCol, keyRows](int r) {
                    return (r >= keyRows || keyCol->isInvalid(r))
                            ? std::numeric_limits<double>::quiet_NaN()
                            : keyCol->valueAt(r);
                });
    case SciDAVis::TypeQString:
        return buildTypedSortPermutation<QString>(
                rows, ascending ? SortComparators::stringLess : SortComparators::stringGreater,
                [keyCol, keyRows](int r) {
                    return (r >= keyRows || keyCol->isInvalid(r)) ? QString() : keyCol->textAt(r);
                });
    case SciDAVis::TypeQDateTime:
        return buildTypedSortPermutation<QDateTime>(
                rows, ascending ? SortComparators::dateTimeLess : SortComparators::dateTimeGreater,
                [keyCol, keyRows](int r) {
                    return (r >= keyRows || keyCol->isInvalid(r)) ? QDateTime()
                                                                  : keyCol->dateTimeAt(r);
                });
    default:
        return QVector<int>();
    }
}

void applySortPermutation(Column *col, const QVector<int> &perm)
{
    if (!col || perm.isEmpty())
        return;
    const int oldRows = col->rowCount();
    const int n = perm.size();
    int outRows = 0;
    for (int k = n - 1; k >= 0; --k) {
        if (perm.at(k) < oldRows) {
            outRows = k + 1;
            break;
        }
    }
    if (outRows <= 0)
        return;

    bool changed = (outRows != oldRows);
    if (!changed) {
        for (int k = 0; k < outRows; ++k) {
            if (perm.at(k) != k) {
                changed = true;
                break;
            }
        }
    }
    if (!changed)
        return;

    QVector<bool> newInvalid(outRows, false);
    QVector<bool> newMasked(outRows, false);
    bool anyNewMasked = false;
    for (int k = 0; k < outRows; ++k) {
        const int src = perm.at(k);
        const bool inRange = (src >= 0 && src < oldRows);
        newInvalid[k] = !inRange || col->isInvalid(src);
        const bool masked = inRange && col->isMasked(src);
        newMasked[k] = masked;
        if (masked)
            anyNewMasked = true;
    }

    switch (col->dataType()) {
    case SciDAVis::TypeDouble: {
        QVector<qreal> sorted(outRows);
        for (int k = 0; k < outRows; ++k)
            sorted[k] = newInvalid.at(k) ? 0.0 : col->valueAt(perm.at(k));
        col->replaceValues(0, sorted);
        applyContiguousTrueRuns(newInvalid, [col](Interval<int> iv) { col->setInvalid(iv, true); });
        break;
    }
    case SciDAVis::TypeQString: {
        QStringList sorted;
        sorted.reserve(outRows);
        for (int k = 0; k < outRows; ++k)
            sorted.append(newInvalid.at(k) ? QString() : col->textAt(perm.at(k)));
        col->replaceTexts(0, sorted);
        applyContiguousTrueRuns(newInvalid, [col](Interval<int> iv) { col->setInvalid(iv, true); });
        break;
    }
    case SciDAVis::TypeQDateTime: {
        QList<QDateTime> sorted;
        sorted.reserve(outRows);
        for (int k = 0; k < outRows; ++k) {
            const QDateTime dt = newInvalid.at(k) ? QDateTime() : col->dateTimeAt(perm.at(k));
            if (!dt.isValid())
                newInvalid[k] = true;
            sorted.append(dt);
        }
        col->replaceDateTimes(0, sorted);
        applyContiguousTrueRuns(newInvalid, [col](Interval<int> iv) { col->setInvalid(iv, true); });
        break;
    }
    default:
        return;
    }

    // Remap masks in the same macro instead of passing isMasked() bool as row index.
    const bool hadMasks = !col->maskedIntervals().isEmpty();
    if (hadMasks || anyNewMasked) {
        col->setMasked(Interval<int>(0, qMax(oldRows, outRows) - 1), false);
        applyContiguousTrueRuns(newMasked, [col](Interval<int> iv) { col->setMasked(iv, true); });
    }
}

} // namespace

void Table::sortColumns(Column *leading, QList<Column *> cols, bool ascending)
{
    if (cols.isEmpty())
        return;

    WAIT_CURSOR;
    beginMacro(tr("%1: sort column(s)").arg(name()));

    QSet<Column *> visited;
    if (leading == nullptr) {
        for (Column *col : cols) {
            if (!col || visited.contains(col))
                continue;
            visited.insert(col);
            const QVector<int> perm = buildSortPermutation(col, col->rowCount(), ascending);
            applySortPermutation(col, perm);
        }
    } else {
        int maxRows = leading->rowCount();
        for (Column *col : cols) {
            if (col)
                maxRows = qMax(maxRows, col->rowCount());
        }
        const QVector<int> perm = buildSortPermutation(leading, maxRows, ascending);
        for (Column *col : cols) {
            if (!col || visited.contains(col))
                continue;
            visited.insert(col);
            applySortPermutation(col, perm);
        }
    }
    endMacro();
    RESET_CURSOR;
} // end of sortColumns()

QIcon Table::icon() const
{
    return IconLoader::duotoneIcon("new-table");
}

QString Table::text(int row, int col)
{
    Column *col_ptr = column(col);
    if (!col_ptr)
        return QString();
    if (col_ptr->isInvalid(row))
        return QString();

    AbstractSimpleFilter *out_fltr = col_ptr->outputFilter();
    out_fltr->input(0, col_ptr);
    return out_fltr->output(0)->textAt(row);
}

void Table::selectAll()
{
    if (!d_view)
        return;
    d_view->selectAll();
}

void Table::handleModeChange(const AbstractColumn *col)
{
    int index = columnIndex(static_cast<const Column *>(col));
    if (index != -1)
        d_table_private.updateHorizontalHeader(index, index);
}

void Table::handleDescriptionChange(const AbstractAspect *aspect)
{
    int index = columnIndex(static_cast<const Column *>(aspect));
    if (index != -1)
        d_table_private.updateHorizontalHeader(index, index);
}

void Table::handlePlotDesignationChange(const AbstractColumn *col)
{
    int index = columnIndex(static_cast<const Column *>(col));
    if (index != -1)
        d_table_private.updateHorizontalHeader(index, columnCount() - 1);
}

void Table::handleDataChange(const AbstractColumn *col)
{
    int index = columnIndex(static_cast<const Column *>(col));
    if (index != -1) {
        if (col->rowCount() > rowCount()) {
            if (!::Table::isUndoRedoReplaying())
                setRowCount(col->rowCount());
        }
        const int lastRow = qMin(rowCount(), col->rowCount()) - 1;
        if (lastRow >= 0)
            emit dataChanged(0, index, lastRow, index);
    }
}

void Table::handleRowsAboutToBeInserted(const AbstractColumn *col, int before, int count)
{
    int new_size = col->rowCount() + count;
    if (before <= col->rowCount() && new_size > rowCount()) {
        if (!::Table::isUndoRedoReplaying())
            setRowCount(new_size);
    }
}

void Table::handleRowsInserted(const AbstractColumn *col, int before, int count)
{
    Q_UNUSED(count);
    int index = columnIndex(static_cast<const Column *>(col));
    if (index != -1 && before <= col->rowCount()) {
        const int lastRow = qMin(rowCount(), col->rowCount()) - 1;
        if (lastRow >= before)
            emit dataChanged(before, index, lastRow, index);
    }
}

void Table::handleRowsAboutToBeRemoved(const AbstractColumn *col, int first, int count)
{
    Q_UNUSED(col);
    Q_UNUSED(first);
    Q_UNUSED(count);
}

void Table::handleRowsRemoved(const AbstractColumn *col, int first, int count)
{
    Q_UNUSED(count);
    int index = columnIndex(static_cast<const Column *>(col));
    if (index != -1)
        emit dataChanged(first, index, col->rowCount() - 1, index);
}

void Table::connectColumn(const Column *col)
{
    connect(col, SIGNAL(aspectDescriptionChanged(const AbstractAspect *)), this,
            SLOT(handleDescriptionChange(const AbstractAspect *)));
    connect(col, SIGNAL(plotDesignationChanged(const AbstractColumn *)), this,
            SLOT(handlePlotDesignationChange(const AbstractColumn *)));
    connect(col, SIGNAL(modeChanged(const AbstractColumn *)), this,
            SLOT(handleDataChange(const AbstractColumn *)));
    connect(col, SIGNAL(dataChanged(const AbstractColumn *)), this,
            SLOT(handleDataChange(const AbstractColumn *)));
    connect(col, SIGNAL(modeChanged(const AbstractColumn *)), this,
            SLOT(handleModeChange(const AbstractColumn *)));
    connect(col, SIGNAL(rowsAboutToBeInserted(const AbstractColumn *, int, int)), this,
            SLOT(handleRowsAboutToBeInserted(const AbstractColumn *, int, int)));
    connect(col, SIGNAL(rowsInserted(const AbstractColumn *, int, int)), this,
            SLOT(handleRowsInserted(const AbstractColumn *, int, int)));
    connect(col, SIGNAL(rowsAboutToBeRemoved(const AbstractColumn *, int, int)), this,
            SLOT(handleRowsAboutToBeRemoved(const AbstractColumn *, int, int)));
    connect(col, SIGNAL(rowsRemoved(const AbstractColumn *, int, int)), this,
            SLOT(handleRowsRemoved(const AbstractColumn *, int, int)));
    connect(col, SIGNAL(maskingChanged(const AbstractColumn *)), this,
            SLOT(handleDataChange(const AbstractColumn *)));
}

void Table::disconnectColumn(const Column *col)
{
    disconnect(col, 0, this, 0);
}

QVariant Table::headerData(int section, Qt::Orientation orientation, int role) const
{
    return d_table_private.headerData(section, orientation, role);
}

void Table::completeAspectInsertion(AbstractAspect *aspect, int index)
{
    Column *column = qobject_cast<Column *>(aspect);
    if (!column)
        return;
    QList<Column *> cols;
    cols.append(column);
    exec(new TableInsertColumnsCmd(d_table_private, index, cols));
}

void Table::prepareAspectRemoval(AbstractAspect *aspect)
{
    Column *column = qobject_cast<Column *>(aspect);
    if (!column)
        return;
    int first = columnIndex(column);
    QList<Column *> cols;
    cols.append(column);
    exec(new TableRemoveColumnsCmd(d_table_private, first, 1, cols));
}

void Table::setAutoRecalcFormulas(bool on)
{
    if (d_auto_recalc_formulas == on)
        return;
    d_auto_recalc_formulas = on;
    emit autoRecalcFormulasChanged(on);
}

void Table::setImportSource(const QString &path, const QString &sheet, const QString &separator,
                            const QString &encoding, int ignoredLines, int unitsRow,
                            int importColumnCount, const QString &numericLocaleName)
{
    d_import_numeric_locale = numericLocaleName;
    d_import_source_path = path;
    d_import_source_sheet = sheet;
    d_import_source_separator = separator;
    d_import_source_encoding = encoding;
    d_import_ignored_lines = ignoredLines;
    d_import_units_row = unitsRow;
    d_import_column_count = importColumnCount;
    if (action_reimport_source)
        action_reimport_source->setEnabled(!d_import_source_path.isEmpty());
}

void Table::save(QXmlStreamWriter *writer) const
{
    int cols = columnCount();
    int rows = rowCount();
    writer->writeStartElement("table");
    writeBasicAttributes(writer);
    writer->writeAttribute("columns", QString::number(cols));
    writer->writeAttribute("rows", QString::number(rows));
    writer->writeAttribute("auto_recalc_formulas", QString::number(d_auto_recalc_formulas ? 1 : 0));
    if (!d_import_source_path.isEmpty()) {
        writer->writeAttribute("import_source_path", d_import_source_path);
        if (!d_import_source_sheet.isEmpty())
            writer->writeAttribute("import_source_sheet", d_import_source_sheet);
        if (!d_import_source_separator.isEmpty())
            writer->writeAttribute("import_source_separator", d_import_source_separator);
        if (!d_import_source_encoding.isEmpty())
            writer->writeAttribute("import_source_encoding", d_import_source_encoding);
        if (d_import_ignored_lines != 0)
            writer->writeAttribute("import_source_ignored",
                                   QString::number(d_import_ignored_lines));
        if (d_import_units_row >= 0)
            writer->writeAttribute("import_source_units", QString::number(d_import_units_row));
        if (d_import_column_count >= 0)
            writer->writeAttribute("import_source_columns",
                                   QString::number(d_import_column_count));
        if (!d_import_numeric_locale.isEmpty())
            writer->writeAttribute("import_source_numeric_locale", d_import_numeric_locale);
    }
    writeCommentElement(writer);

    for (int col = 0; col < cols; col++)
        column(col)->save(writer);
    for (int col = 0; col < cols; col++) {
        writer->writeStartElement("column_width");
        writer->writeAttribute("column", QString::number(col));
        writer->writeCharacters(QString::number(columnWidth(col)));
        writer->writeEndElement();
    }
    writer->writeEndElement(); // "table"
}

bool Table::load(XmlStreamReader *reader)
{
    if (reader->isStartElement() && reader->name() == "table") {
        setColumnCount(0);
        setRowCount(0);
        setComment("");

        if (!readBasicAttributes(reader))
            return false;

        const QString arAttr = reader->attributes().value("auto_recalc_formulas").toString();
        const bool loadedAutoRecalc = arAttr.isEmpty() ? true : (arAttr.toInt() != 0);
        d_auto_recalc_formulas = false;
        emit autoRecalcFormulasChanged(false);

        const QXmlStreamAttributes attrs = reader->attributes();
        const int loadedIgnored = attrs.hasAttribute("import_source_ignored")
                ? attrs.value("import_source_ignored").toInt()
                : 0;
        const int loadedUnits = attrs.hasAttribute("import_source_units")
                ? attrs.value("import_source_units").toInt()
                : -1;
        const int loadedCols = attrs.hasAttribute("import_source_columns")
                ? attrs.value("import_source_columns").toInt()
                : -1;
        setImportSource(attrs.value("import_source_path").toString(),
                        attrs.value("import_source_sheet").toString(),
                        attrs.value("import_source_separator").toString(),
                        attrs.value("import_source_encoding").toString(), loadedIgnored,
                        loadedUnits, loadedCols,
                        attrs.value("import_source_numeric_locale").toString());

        // read dimensions
        bool ok1, ok2;
        int rows, cols;
        rows = reader->readAttributeInt("rows", &ok1);
        cols = reader->readAttributeInt("columns", &ok2);
        if (!ok1 || !ok2) {
            reader->raiseError(tr("invalid row or column count"));
            return false;
        }

        setRowCount(rows);
        // read child elements
        while (!reader->atEnd()) {
            reader->readNext();

            if (reader->isEndElement())
                break;

            if (reader->isStartElement()) {
                if (reader->name() == "comment") {
                    if (!readCommentElement(reader))
                        return false;
                } else if (reader->name() == "column") {
                    Column *column = new Column(tr("Column %1").arg(1), SciDAVis::ColumnMode::Text);
                    if (!column->load(reader)) {
                        setColumnCount(0);
                        return false;
                    }
                    QList<Column *> columns;
                    columns.append(column);
                    appendColumns(columns);
                } else if (reader->name() == "column_width") {
                    if (!readColumnWidthElement(reader))
                        return false;
                } else // unknown element
                {
                    reader->raiseWarning(tr("unknown element '%1'").arg(reader->name().toString()));
                    if (!reader->skipToEndElement())
                        return false;
                }
            }
        }
        if (cols != columnCount())
            reader->raiseWarning(tr("columns attribute and number of read columns do not match"));

        d_auto_recalc_formulas = loadedAutoRecalc;
        emit autoRecalcFormulasChanged(loadedAutoRecalc);
    } else // no table element
        reader->raiseError(tr("no table element found"));

    return !reader->hasError();
}

void Table::adjustActionNames()
{
    if (!d_view)
        return;

    QString action_name;
    if (d_view->areCommentsShown())
        action_name = tr("Hide Comments");
    else
        action_name = tr("Show Comments");
    action_toggle_comments->setText(action_name);

    if (d_view->isControlTabBarVisible())
        action_name = tr("Hide Controls");
    else
        action_name = tr("Show Controls");
    action_toggle_tabbar->setText(action_name);
}

bool Table::readColumnWidthElement(XmlStreamReader *reader)
{
    Q_ASSERT(reader->isStartElement() && reader->name() == "column_width");
    bool ok;
    int col = reader->readAttributeInt("column", &ok);
    if (!ok) {
        reader->raiseError(tr("invalid or missing column index"));
        return false;
    }
    QString str = reader->readElementText();
    int value = str.toInt(&ok);
    if (!ok) {
        reader->raiseError(tr("invalid column width"));
        return false;
    }
    if (d_view)
        d_view->setColumnWidth(col, value);
    else
        setColumnWidth(col, value);
    return true;
}

void Table::setColumnWidth(int col, int width)
{
    d_table_private.setColumnWidth(col, width);
}

int Table::columnWidth(int col) const
{
    return d_table_private.columnWidth(col);
}

/* ========================= static methods ======================= */
ActionManager *Table::action_manager = 0;

ActionManager *Table::actionManager()
{
    if (!action_manager)
        initActionManager();

    return action_manager;
}

void Table::initActionManager()
{
    if (!action_manager)
        action_manager = new ActionManager();

    action_manager->setTitle(tr("Table"));
    volatile Table *action_creator = new Table(); // initialize the action texts
    delete action_creator;
}

/* ========================== Table::Private ====================== */

Column *Table::Private::column(int index) const
{
    return d_columns.value(index);
}

Column *Table::Private::currentColumn() const
{
    if ((-1 < d_current_column) && (d_current_column < d_columns.size()))
        return d_columns.value(d_current_column);
    return nullptr;
}

bool future::Table::Private::setCurrentColumn(int index)
{
    if ((-1 < index) && (index < d_columns.size())) {
        d_current_column = index;
        return true;
    }
    d_current_column = -1;
    return false;
}

void Table::Private::replaceColumns(int first, QList<Column *> new_cols)
{
    if ((first < 0) || (first + new_cols.size() > d_column_count))
        return;

    int count = new_cols.size();
    emit d_owner.columnsAboutToBeReplaced(first, new_cols.count());
    for (int i = 0; i < count; i++) {
        int rows = new_cols.at(i)->rowCount();
        if (rows > d_row_count)
            setRowCount(rows);

        if (d_columns.at(first + i))
            d_columns.at(first + i)->notifyReplacement(new_cols.at(i));

        d_columns[first + i] = new_cols.at(i);
        d_owner.connectColumn(new_cols.at(i));
    }
    updateHorizontalHeader(first, first + count - 1);
    emit d_owner.columnsReplaced(first, new_cols.count());
    emit d_owner.dataChanged(0, first, d_row_count - 1, first + count - 1);
}

void Table::Private::insertColumns(int before, QList<Column *> cols)
{
    int count = cols.count();

    if ((count < 1) || (before > d_column_count))
        return;

    Q_ASSERT(before >= 0);

    int i, rows;
    for (i = 0; i < count; i++) {
        rows = cols.at(i)->rowCount();
        if (rows > d_row_count)
            setRowCount(rows);
    }

    emit d_owner.columnsAboutToBeInserted(before, cols);
    for (int i = count - 1; i >= 0; i--) {
        d_columns.insert(before, cols.at(i));
        d_owner.connectColumn(cols.at(i));
        d_column_widths.insert(before, Table::defaultColumnWidth());
    }
    d_column_count += count;
    emit d_owner.columnsInserted(before, cols.count());
    updateHorizontalHeader(before, before + count - 1);
}

void Table::Private::removeColumns(int first, int count)
{
    if ((count < 1) || (first >= d_column_count))
        return;

    Q_ASSERT(first >= 0);

    Q_ASSERT(first + count <= d_column_count);

    emit d_owner.columnsAboutToBeRemoved(first, count);
    for (int i = count - 1; i >= 0; i--) {
        d_owner.disconnectColumn(d_columns.at(first));
        d_columns.removeAt(first);
        d_column_widths.removeAt(first);
    }
    d_column_count -= count;
    emit d_owner.columnsRemoved(first, count);
    updateHorizontalHeader(first, d_column_count - 1);
}

void Table::Private::appendColumns(QList<Column *> cols)
{
    insertColumns(d_column_count, cols);
}

void Table::Private::moveColumn(int from, int to)
{
    if (from < 0 || from >= d_column_count)
        return;
    if (to < 0 || to >= d_column_count)
        return;

    d_columns.move(from, to);
    d_owner.connectColumn(d_columns.at(to));
    d_column_widths.move(from, to);
    updateHorizontalHeader(qMin(from, to), qMax(from, to));
    emit d_owner.dataChanged(0, from, d_row_count - 1, from);
    emit d_owner.dataChanged(0, to, d_row_count - 1, to);
    if (d_owner.d_view)
        d_owner.d_view->rereadSectionSizes();
}

void Table::Private::setRowCount(int rows)
{
    int diff = rows - d_row_count;
    int old_row_count = d_row_count;
    if (diff == 0)
        return;

    if (diff > 0) {
        emit d_owner.rowsAboutToBeInserted(d_row_count, diff);
        d_row_count = rows;
        updateVerticalHeader(d_row_count - diff);
        emit d_owner.rowsInserted(old_row_count, diff);
    } else {
        emit d_owner.rowsAboutToBeRemoved(rows, -diff);
        d_row_count = rows;
        emit d_owner.rowsRemoved(rows, -diff);
    }
}

QString Table::Private::columnHeader(int col)
{
    return headerData(col, Qt::Horizontal, Qt::DisplayRole).toString();
}

int Table::Private::numColsWithPD(SciDAVis::PlotDesignation pd)
{
    int count = 0;

    for (int i = 0; i < d_column_count; i++)
        if (d_columns.at(i)->plotDesignation() == pd)
            count++;

    return count;
}

void Table::Private::updateVerticalHeader(int start_row)
{
    int current_size = d_vertical_header_data.size(), i;
    for (i = start_row; i < current_size; i++)
        d_vertical_header_data.replace(i, i + 1);
    for (; i < d_row_count; i++)
        d_vertical_header_data << i + 1;
    emit d_owner.headerDataChanged(Qt::Vertical, start_row, d_row_count - 1);
}

void Table::Private::updateHorizontalHeader(int start_col, int end_col)
{
    if (start_col > end_col)
        return;

    while (d_horizontal_header_data.size() < d_column_count)
        d_horizontal_header_data << QString();

    if (numColsWithPD(SciDAVis::X) > 1) {
        int x_cols = 0;
        for (int i = 0; i < d_column_count; i++) {
            if (d_columns.at(i)->plotDesignation() == SciDAVis::X)
                composeColumnHeader(
                        i, d_columns.at(i)->name() + "[X" + QString::number(++x_cols) + "]");
            else if (d_columns.at(i)->plotDesignation() == SciDAVis::Y) {
                if (x_cols > 0)
                    composeColumnHeader(
                            i, d_columns.at(i)->name() + "[Y" + QString::number(x_cols) + "]");
                else
                    composeColumnHeader(i, d_columns.at(i)->name() + "[Y]");
            } else if (d_columns.at(i)->plotDesignation() == SciDAVis::Z) {
                if (x_cols > 0)
                    composeColumnHeader(
                            i, d_columns.at(i)->name() + "[Z" + QString::number(x_cols) + "]");
                else
                    composeColumnHeader(i, d_columns.at(i)->name() + "[Z]");
            } else if (d_columns.at(i)->plotDesignation() == SciDAVis::xErr) {
                if (x_cols > 0)
                    composeColumnHeader(
                            i, d_columns.at(i)->name() + "[xEr" + QString::number(x_cols) + "]");
                else
                    composeColumnHeader(i, d_columns.at(i)->name() + "[xEr]");
            } else if (d_columns.at(i)->plotDesignation() == SciDAVis::yErr) {
                if (x_cols > 0)
                    composeColumnHeader(
                            i, d_columns.at(i)->name() + "[yEr" + QString::number(x_cols) + "]");
                else
                    composeColumnHeader(i, d_columns.at(i)->name() + "[yEr]");
            } else
                composeColumnHeader(i, d_columns.at(i)->name());
        }
    } else {
        for (int i = 0; i < d_column_count; i++) {
            if (d_columns.at(i)->plotDesignation() == SciDAVis::X)
                composeColumnHeader(i, d_columns.at(i)->name() + "[X]");
            else if (d_columns.at(i)->plotDesignation() == SciDAVis::Y)
                composeColumnHeader(i, d_columns.at(i)->name() + "[Y]");
            else if (d_columns.at(i)->plotDesignation() == SciDAVis::Z)
                composeColumnHeader(i, d_columns.at(i)->name() + "[Z]");
            else if (d_columns.at(i)->plotDesignation() == SciDAVis::xErr)
                composeColumnHeader(i, d_columns.at(i)->name() + "[xEr]");
            else if (d_columns.at(i)->plotDesignation() == SciDAVis::yErr)
                composeColumnHeader(i, d_columns.at(i)->name() + "[yEr]");
            else
                composeColumnHeader(i, d_columns.at(i)->name());
        }
    }
    emit d_owner.headerDataChanged(Qt::Horizontal, start_col, end_col);
}

void Table::Private::composeColumnHeader(int col, const QString &label)
{
    if (col >= d_horizontal_header_data.size())
        d_horizontal_header_data << label;
    else
        d_horizontal_header_data.replace(col, label);
}

QVariant Table::Private::headerData(int section, Qt::Orientation orientation, int role) const
{
    switch (orientation) {
    case Qt::Horizontal:
        if (section >= d_horizontal_header_data.size())
            return QVariant();
        // A column whose automatic formula recalculation failed (error or circular reference)
        // gets a warning icon, red text and the reason in its tooltip.
        if (section < d_columns.size() && !d_columns.at(section)->formulaError().isEmpty()) {
            switch (role) {
            case Qt::ToolTipRole:
                return d_horizontal_header_data.at(section) + QStringLiteral("\n")
                        + d_columns.at(section)->formulaError();
            case Qt::DecorationRole:
                return QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
            case Qt::ForegroundRole:
                return QColor(200, 0, 0);
            default:
                break;
            }
        }
        switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::EditRole:
            return d_horizontal_header_data.at(section);
        case Qt::DecorationRole:
            return d_columns.at(section)->icon();
        case TableModel::CommentRole:
            return d_columns.at(section)->comment();
        }
        break;
    case Qt::Vertical:
        if (section >= d_vertical_header_data.size())
            return QVariant();
        switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            return d_vertical_header_data.at(section);
        }
        break;
    }
    return QVariant();
}

} // namespace
