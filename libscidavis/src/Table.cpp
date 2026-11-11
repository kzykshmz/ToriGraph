/***************************************************************************
    File                 : Table.cpp
    Project              : SciDAVis
    Description          : Table worksheet class
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
#include "Table.h"
#include "core/column/Column.h"
#include "lib/Interval.h"
#include "table/TableModel.h"
#include "core/datatypes/Double2StringFilter.h"
#include "core/datatypes/String2DoubleFilter.h"
#include "core/datatypes/DateTime2StringFilter.h"
#include "table/AsciiTableImportFilter.h"
#include "table/TableDoubleHeaderView.h"
#include "ScriptEdit.h"

#include <QMessageBox>
#include <QDateTime>
#include <QTextStream>
#include <QTextCodec>
#include <QClipboard>
#include <QMimeData>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QPainter>
#include <QEvent>
#include <QCloseEvent>
#include <QLayout>
#include <QPrintDialog>
#include <QLocale>
#include <QShortcut>
#include <QProgressDialog>
#include <QFile>
#include <QTemporaryFile>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <vector>
#include <cmath>
#include <iostream>
using namespace std;

Table::Table(ScriptingEnv *env, const QString &fname, const QString &sep, int ignoredLines,
             bool renameCols, bool stripSpaces, bool simplifySpaces, bool convertToNumeric,
             QLocale numericLocale, const QString &label, QWidget *parent, const char *name,
             Qt::WindowFlags f)
    : TableView(label, parent, name, f), scripted(env)
{

    AsciiTableImportFilter filter;
    filter.set_ignored_lines(ignoredLines);
    filter.set_separator(sep);
    filter.set_first_row_names_columns(renameCols);
    filter.set_trim_whitespace(stripSpaces);
    filter.set_simplify_whitespace(simplifySpaces);
    filter.set_convert_to_numeric(convertToNumeric);
    filter.set_numeric_locale(numericLocale);

    QFile file(fname);
    if (file.open(QIODevice::ReadOnly)) {
        d_future_table = static_cast<future::Table *>(filter.importAspect(file));
        if (!d_future_table)
            d_future_table = new future::Table(0, 0, label);
        else
            d_future_table->setName(label);
    }
    setWindowLabel(fname);
    init();
}

Table::Table(ScriptingEnv *env, int r, int c, const QString &label, QWidget *parent,
             const char *name, Qt::WindowFlags f)
    : TableView(label, parent, name, f), scripted(env)
{
    d_future_table = new future::Table(r, c, label);
    init();
}

void Table::init()
{
    if (d_future_table) {
        TableView::setTable(d_future_table);
        birthdate = QLocale().toString(d_future_table->creationTime());
    } else // the rest is meaningless
        return;

    setMinimumSize(400, 300);
    ui.gridLayout1->removeWidget(ui.formula_box);
    delete ui.formula_box;
    ui.formula_box = new ScriptEdit(scriptEnv, ui.formula_tab);
    ui.formula_box->setObjectName(QString::fromUtf8("formula_box"));
    ui.formula_box->setMinimumSize(QSize(60, 10));
    ui.formula_box->setAcceptRichText(false);
    ui.formula_box->setLineWrapMode(QTextEdit::WidgetWidth);
    ui.gridLayout1->addWidget(ui.formula_box, 1, 0, 1, 3);

    for (int i = 0; i < columnCount(); i++)
        ui.add_reference_combobox->addItem("col(\"" + column(i)->name() + "\")");

    ui.add_function_combobox->addItems(scriptEnv->mathFunctions());
    updateFunctionDoc();

    connect(ui.add_function_combobox, SIGNAL(currentIndexChanged(int)), this,
            SLOT(updateFunctionDoc()));
    connect(ui.set_formula_button, SIGNAL(pressed()), this, SLOT(applyFormula()));
    connect(ui.add_function_button, SIGNAL(pressed()), this, SLOT(addFunction()));
    connect(ui.add_reference_button, SIGNAL(pressed()), this, SLOT(addReference()));

    connect(d_future_table, SIGNAL(columnsAboutToBeRemoved(int, int)), this,
            SLOT(handleColumnsAboutToBeRemoved(int, int)));
    connect(d_future_table, SIGNAL(columnsRemoved(int, int)), this,
            SLOT(handleColumnsRemoved(int, int)));
    connect(d_future_table, SIGNAL(rowsInserted(int, int)), this, SLOT(handleRowChange()));
    connect(d_future_table, SIGNAL(rowsRemoved(int, int)), this, SLOT(handleRowChange()));
    connect(d_future_table, SIGNAL(dataChanged(int, int, int, int)), this,
            SLOT(handleColumnChange(int, int, int, int)));
    connect(d_future_table, SIGNAL(columnsReplaced(int, int)), this,
            SLOT(handleColumnChange(int, int)));

    connect(d_future_table, SIGNAL(columnsInserted(int, int)), this, SLOT(handleChange()));
    connect(d_future_table, SIGNAL(columnsReplaced(int, int)), this, SLOT(handleChange()));
    connect(d_future_table, SIGNAL(columnsRemoved(int, int)), this, SLOT(handleChange()));
    connect(d_future_table, SIGNAL(rowsInserted(int, int)), this, SLOT(handleChange()));
    connect(d_future_table, SIGNAL(rowsRemoved(int, int)), this, SLOT(handleChange()));
    connect(d_future_table, SIGNAL(dataChanged(int, int, int, int)), this, SLOT(handleChange()));
    connect(d_future_table, SIGNAL(headerDataChanged(Qt::Orientation, int, int)), this,
            SLOT(handleChange()));
    connect(d_future_table, SIGNAL(recalculate()), this, SLOT(recalculate()));
    connect(d_future_table, SIGNAL(requestColumnPropertiesDialog(int)), this,
            SLOT(showColumnPropertiesDialog(int)));

    connect(d_future_table, SIGNAL(aspectDescriptionChanged(const AbstractAspect *)), this,
            SLOT(handleAspectDescriptionChange(const AbstractAspect *)));
    connect(d_future_table, SIGNAL(aspectDescriptionAboutToChange(const AbstractAspect *)), this,
            SLOT(handleAspectDescriptionAboutToChange(const AbstractAspect *)));

    this->installEventFilter(this);
    if (d_view_widget && d_view_widget->viewport())
        d_view_widget->viewport()->installEventFilter(this);
    if (d_horizontal_header) {
        if (d_horizontal_header->viewport())
            d_horizontal_header->viewport()->installEventFilter(this);
    }
}

void Table::handleChange()
{
    emit modifiedWindow(this);
}

void Table::handleColumnChange(int first, int count)
{
    for (int i = first; i < first + count; i++)
        emit modifiedData(this, colName(i));
    if (d_auto_recalc_formulas && !d_in_auto_recalc)
        autoRecalculateDependentColumns(first, count);
}

void Table::handleColumnChange(int top, int left, int bottom, int right)
{
    Q_UNUSED(top);
    Q_UNUSED(bottom);
    handleColumnChange(left, right - left + 1);
}

void Table::handleColumnsAboutToBeRemoved(int first, int count)
{
    for (int i = first; i < first + count; i++)
        emit aboutToRemoveCol(colName(i));
}

void Table::handleColumnsRemoved(int first, int count)
{
    for (int i = first; i < first + count; i++)
        emit removedCol(colName(i));
}

void Table::handleRowChange()
{
    for (int i = 0; i < numCols(); i++)
        emit modifiedData(this, colName(i));
}

void Table::setBackgroundColor(const QColor &col)
{
    QPalette palette;
    palette.setColor(QPalette::Window, col);
    d_view_widget->setPalette(palette);
}

void Table::setTextColor(const QColor &col)
{
    QPalette palette;
    palette.setColor(QPalette::WindowText, col);
    d_view_widget->setPalette(palette);
}

void Table::setTextFont(const QFont &fnt)
{
    d_view_widget->setFont(fnt);
}

void Table::setHeaderColor(const QColor &col)
{
    QPalette palette;
    palette.setColor(QPalette::WindowText, col);
    d_view_widget->horizontalHeader()->setPalette(palette);
}

void Table::setHeaderFont(const QFont &fnt)
{
    d_view_widget->horizontalHeader()->setFont(fnt);
}

void Table::exportPDF(const QString &fileName)
{
    print(fileName);
}

void Table::print()
{
    print(QString());
}

void Table::print(const QString &fileName)
{
    QPrinter printer;
    printer.setColorMode(QPrinter::GrayScale);

    if (!fileName.isEmpty()) {
        printer.setCreator("SciDAVis");
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
    } else {
        QPrintDialog printDialog(&printer);
        if (printDialog.exec() != QDialog::Accepted)
            return;
    }

    printer.setFullPage(true);
    QPainter p;
    if (!p.begin(&printer))
        return; // paint on printer
    int dpiy = printer.logicalDpiY();
    const int margin = (int)((1 / 2.54) * dpiy); // 1 cm margins

    QHeaderView *hHeader = d_view_widget->horizontalHeader();
    QHeaderView *vHeader = d_view_widget->verticalHeader();

    int rows = numRows();
    int cols = numCols();
    int height = margin;
    int i, vertHeaderWidth = vHeader->width();
    int right = margin + vertHeaderWidth;

    // print header
    p.setFont(hHeader->font());
    QString header_label = d_view_widget->model()->headerData(0, Qt::Horizontal).toString();
    QRect br = p.boundingRect(br, Qt::AlignCenter, header_label);
    p.drawLine(right, height, right, height + br.height());
    QRect tr(br);

    for (i = 0; i < cols; i++) {
        int w = columnWidth(i);
        tr.setTopLeft(QPoint(right, height));
        tr.setWidth(w);
        tr.setHeight(br.height());
        header_label = d_view_widget->model()->headerData(i, Qt::Horizontal).toString();
        p.drawText(tr, Qt::AlignCenter, header_label);
        right += w;
        p.drawLine(right, height, right, height + tr.height());

        if (right >= printer.width() - 2 * margin)
            break;
    }

    p.drawLine(margin + vertHeaderWidth, height, right - 1, height); // first horizontal line
    height += tr.height();
    p.drawLine(margin, height, right - 1, height);

    // print table values
    for (i = 0; i < rows; i++) {
        right = margin;
        QString cell_text = d_view_widget->model()->headerData(i, Qt::Vertical).toString() + "\t";
        tr = p.boundingRect(tr, Qt::AlignCenter, cell_text);
        p.drawLine(right, height, right, height + tr.height());

        br.setTopLeft(QPoint(right, height));
        br.setWidth(vertHeaderWidth);
        br.setHeight(tr.height());
        p.drawText(br, Qt::AlignCenter, cell_text);
        right += vertHeaderWidth;
        p.drawLine(right, height, right, height + tr.height());

        for (int j = 0; j < cols; j++) {
            int w = columnWidth(j);
            cell_text = text(i, j) + "\t";
            tr = p.boundingRect(tr, Qt::AlignCenter, cell_text);
            br.setTopLeft(QPoint(right, height));
            br.setWidth(w);
            br.setHeight(tr.height());
            p.drawText(br, Qt::AlignCenter, cell_text);
            right += w;
            p.drawLine(right, height, right, height + tr.height());

            if (right >= printer.width() - 2 * margin)
                break;
        }
        height += br.height();
        p.drawLine(margin, height, right - 1, height);

        if (height >= printer.height() - margin) {
            printer.newPage();
            height = margin;
            p.drawLine(margin, height, right, height);
        }
    }
}

int Table::colX(int col)
{
    return d_future_table ? d_future_table->colX(col) : 0;
}

int Table::colY(int col)
{
    return d_future_table ? d_future_table->colY(col) : 0;
}

void Table::setPlotDesignation(SciDAVis::PlotDesignation pd)
{
    if (d_future_table)
        d_future_table->setSelectionAs(pd);
}

int Table::columnWidth(int col)
{
    return d_future_table ? d_view_widget->columnWidth(col) : 0;
}

void Table::setColWidths(const QStringList &widths)
{
    for (int i = 0; i < widths.count(); i++)
        d_view_widget->setColumnWidth(i, widths[i].toInt());
}

void Table::setColumnTypes(const QStringList &ctl)
{
    // TODO: obsolete, remove in 0.3.0
    int n = qMin((int)ctl.count(), numCols());
    for (int i = 0; i < n; i++) {
        QStringList l = ctl[i].split(";");
        switch (l[0].toInt()) {
        //	old enum: enum ColType{Numeric = 0, Text = 1, Date = 2, Time = 3, Month = 4, Day =
        // 5};
        case 0:
            column(i)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            break;
        case 1:
            column(i)->setColumnMode(SciDAVis::ColumnMode::Text);
            break;
        case 2:
        case 3:
        case 6:
            column(i)->setColumnMode(SciDAVis::ColumnMode::DateTime);
            break;
        case 4:
            column(i)->setColumnMode(SciDAVis::ColumnMode::Month);
            break;
        case 5:
            column(i)->setColumnMode(SciDAVis::ColumnMode::Day);
            break;
        }
    }
}

QString Table::saveColumnWidths()
{
    // TODO: obsolete, remove in 0.3.0
    QString s = "ColWidth\t";
    for (int i = 0; i < numCols(); i++)
        s += QString::number(columnWidth(i)) + "\t";

    return s + "\n";
}

QString Table::saveColumnTypes()
{
    // TODO: obsolete, remove in 0.3.0
    QString s = "ColType";
    for (int i = 0; i < numCols(); i++)
        s += "\t" + QString::number(static_cast<int>(column(i)->columnMode())) + ";0/6";
    return s + "\n";
}

void Table::setCommands(const QStringList &com)
{
    const bool prevGuard = d_in_auto_recalc;
    d_in_auto_recalc = true;
    for (int i = 0; i < (int)com.size() && i < numCols(); i++) {
        const QString trimmed = com.at(i).trimmed();
        if (trimmed.isEmpty())
            column(i)->clearFormulas();
        else
            column(i)->setFormula(Interval<int>(0, qMax(0, numRows() - 1)), trimmed);
    }
    d_in_auto_recalc = prevGuard;
    if (d_auto_recalc_formulas && !d_in_auto_recalc)
        autoRecalculateDependentColumns(-1, 0);
}

void Table::setCommand(int col, const QString &com)
{
    Column *c = column(col);
    if (!c)
        return;
    const QString trimmed = com.trimmed();
    const bool prevGuard = d_in_auto_recalc;
    d_in_auto_recalc = true;
    if (trimmed.isEmpty())
        c->clearFormulas();
    else
        c->setFormula(Interval<int>(0, qMax(0, numRows() - 1)), trimmed);
    d_in_auto_recalc = prevGuard;
    if (!trimmed.isEmpty() && d_auto_recalc_formulas && !d_in_auto_recalc) {
        d_in_auto_recalc = true;
        recalculateColumnInternal(col, false, true);
        d_in_auto_recalc = false;
        autoRecalculateDependentColumns(col, 1);
    }
}

void Table::setCommands(const QString &com)
{
    QStringList lst = com.split("\t");
    lst.pop_front();
    setCommands(lst);
}

bool Table::recalculate()
{
    int firstCol = firstSelectedColumn();
    int lastCol = lastSelectedColumn();
    bool anySelectedHasFormula = false;
    if (firstCol >= 0 && lastCol >= firstCol) {
        for (int col = firstCol; col <= lastCol; ++col) {
            Column *c = column(col);
            if (c && !c->formulaIntervals().isEmpty()) {
                anySelectedHasFormula = true;
                break;
            }
        }
    }
    if (!anySelectedHasFormula) {
        firstCol = 0;
        lastCol = numCols() - 1;
    }
    for (int col = firstCol; col <= lastCol; col++) {
        Column *c = column(col);
        if (!c || c->formulaIntervals().isEmpty())
            continue;
        if (!recalculate(col, true))
            return false;
    }
    return true;
}

bool Table::recalculate(int col, bool only_selected_rows)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = recalculateColumnInternal(col, only_selected_rows, false);
    QApplication::restoreOverrideCursor();
    return ok;
}

bool Table::recalculateColumnInternal(int col, bool only_selected_rows, bool silentErrors)
{
    Column *col_ptr = column(col);
    if (!col_ptr || !scriptEnv)
        return false;

    if (only_selected_rows && selectedRowCount(false) < 2)
        only_selected_rows = false;

    QList<Interval<int>> formula_intervals = col_ptr->formulaIntervals();
    if (formula_intervals.isEmpty())
        return true;

    const int tableRows = numRows();
    if (only_selected_rows) {
        const int maxRow = qMax(tableRows, col_ptr->rowCount()) - 1;
        if (maxRow >= 0) {
            QList<Interval<int>> deselected =
                    Interval<int>(0, maxRow) - selectedRows().intervals();
            foreach (Interval<int> i, deselected)
                Interval<int>::subtractIntervalFromList(&formula_intervals, i);
        }
    }

    foreach (Interval<int> interval, formula_intervals) {
        QString formula = col_ptr->formula(interval.start());
        if (formula.isEmpty())
            continue;

        Script *colscript = scriptEnv->newScript(formula, this, QString("<%1>").arg(colName(col)));
        if (!silentErrors) {
            connect(colscript, SIGNAL(error(const QString &, const QString &, int)), scriptEnv,
                    SIGNAL(error(const QString &, const QString &, int)));
            connect(colscript, SIGNAL(print(const QString &)), scriptEnv,
                    SIGNAL(print(const QString &)));
        }

        if (!colscript->compile()) {
            delete colscript;
            return false;
        }

        colscript->setInt(col + 1, "j");
        QVariant ret;
        int start_row = qMax(0, interval.start());
        int end_row = interval.end();
        if (!only_selected_rows && formula_intervals.size() == 1 && start_row == 0
            && tableRows > 0 && end_row < tableRows - 1
            && end_row >= qMax(0, col_ptr->rowCount() - 1)) {
            end_row = tableRows - 1;
        }
        if (tableRows > 0 && end_row >= tableRows)
            end_row = tableRows - 1;
        if (end_row < start_row) {
            delete colscript;
            continue;
        }

        switch (col_ptr->columnMode()) {
        case SciDAVis::ColumnMode::Numeric: {
            const int span = end_row - start_row + 1;
            QVector<qreal> results(span, 0.0);
            QVector<bool> emptyRows(span, false);
            for (int i = start_row; i <= end_row; i++) {
                colscript->setInt(i + 1, "i");
                ret = colscript->eval();
                if (!ret.isValid()) {
                    delete colscript;
                    return false;
                }
                if (ret.type() == QVariant::String && ret.toString().isEmpty()) {
                    emptyRows[i - start_row] = true;
                    results[i - start_row] = 0.0;
                } else if (ret.canConvert(QVariant::Double)) {
                    results[i - start_row] = ret.toDouble();
                } else {
                    results[i - start_row] = NAN;
                }
            }
            bool anyChanged = (col_ptr->rowCount() <= end_row);
            if (!anyChanged) {
                for (int k = 0; k < span; ++k) {
                    const int r = start_row + k;
                    if (col_ptr->isInvalid(r) != emptyRows[k]) {
                        anyChanged = true;
                        break;
                    }
                    if (!emptyRows[k]) {
                        const double oldV = col_ptr->valueAt(r);
                        const double newV = results[k];
                        if ((std::isnan(oldV) != std::isnan(newV))
                            || (!std::isnan(newV) && oldV != newV)) {
                            anyChanged = true;
                            break;
                        }
                    }
                }
            }
            if (anyChanged) {
                col_ptr->replaceValues(start_row, results);
                int k = 0;
                while (k < span) {
                    if (emptyRows[k]) {
                        int kEnd = k;
                        while (kEnd + 1 < span && emptyRows[kEnd + 1])
                            ++kEnd;
                        col_ptr->setInvalid(Interval<int>(start_row + k, start_row + kEnd), true);
                        k = kEnd + 1;
                    } else {
                        ++k;
                    }
                }
            }
            break;
        }
        default: {
            QStringList results;
            for (int i = start_row; i <= end_row; i++) {
                colscript->setInt(i + 1, "i");
                ret = colscript->eval();
                if (!ret.isValid()) {
                    delete colscript;
                    return false;
                }
                if (ret.type() == QVariant::Double)
                    results << QLocale().toString(ret.toDouble(), 'g', 14);
                else if (ret.canConvert(QVariant::String))
                    results << ret.toString();
                else
                    results << QString();
            }
            bool anyChanged = (col_ptr->rowCount() <= end_row);
            if (!anyChanged) {
                for (int k = 0; k < results.size(); ++k) {
                    if (col_ptr->asStringColumn()->textAt(start_row + k) != results.at(k)) {
                        anyChanged = true;
                        break;
                    }
                }
            }
            if (anyChanged)
                col_ptr->asStringColumn()->replaceTexts(start_row, results);
            break;
        }
        }
        delete colscript;
    }
    return true;
}

void Table::autoRecalculateDependentColumns(int changedFirstCol, int changedCount)
{
    if (!d_auto_recalc_formulas || d_in_auto_recalc || !d_future_table || !scriptEnv)
        return;

    QVector<int> fCols;
    const int nCols = numCols();
    for (int c = 0; c < nCols; ++c) {
        if (changedCount > 0 && c >= changedFirstCol && c < changedFirstCol + changedCount)
            continue;
        Column *cp = column(c);
        if (cp && !cp->formulaIntervals().isEmpty()) {
            const QString f = cp->formula(cp->formulaIntervals().first().start()).trimmed();
            if (!f.isEmpty())
                fCols.push_back(c);
        }
    }
    if (fCols.isEmpty())
        return;

    d_in_auto_recalc = true;
    const int maxPasses = qMin(fCols.size(), 4);
    for (int pass = 0; pass < maxPasses; ++pass) {
        for (int idx = 0; idx < fCols.size(); ++idx)
            recalculateColumnInternal(fCols[idx], false, true);
    }
    d_in_auto_recalc = false;
}

QString Table::saveCommands()
{
    // TODO: obsolete, remove for 0.3.0, only needed for template saving
    QString s = "<com>\n";
    for (int col = 0; col < numCols(); col++)
        if (!column(col)->formula(0).isEmpty()) {
            s += "<col nr=\"" + QString::number(col) + "\">\n";
            s += column(col)->formula(0);
            s += "\n</col>\n";
        }
    s += "</com>\n";
    return s;
}

QString Table::saveComments()
{
    // TODO: obsolete, remove for 0.3.0, only needed for template saving
    QString s = "Comments\t";
    for (int i = 0; i < numCols(); i++) {
        s += column(i)->comment() + "\t";
    }
    return s + "\n";
}

QString Table::saveToString(const QString &geometry)
{
    QString s = "<table>\n";
    QString xml;
    if (d_future_table) {
        QXmlStreamWriter writer(&xml);
        d_future_table->save(&writer);
    }
    s += QString::number(xml.length()) + "\n"; // this is need in case there are newlines in the XML
    s += xml + "\n";
    s += geometry + "\n";
    s += "</table>\n";
    return s;
}

void Table::saveToDevice(QIODevice *device, const QString &geometry)
{
    QTextStream stream(device);
    stream.setCodec(QTextCodec::codecForName("UTF-8"));

    // write start tag
    stream << "<table>\n";
    stream.flush();

    // On Windows, writing to a QString has been observed to crash for large tables
    // (apparently due to excessive memory usage).
    // => use temporary file if possible
    QTemporaryFile tmp_file;
    QString tmp_string;
    if (d_future_table) {
        QXmlStreamWriter xml(&tmp_string);
        if (tmp_file.open())
            xml.setDevice(&tmp_file);
        d_future_table->save(&xml);
    }

    // write number of characters of QXmlStreamWriter's output
    // this is needed in case there are newlines in the XML
    int xml_chars = 0;
    if (tmp_file.isOpen()) {
        tmp_file.seek(0);
        QTextStream count(&tmp_file);
        count.setCodec(QTextCodec::codecForName("UTF-8"));
        while (!count.atEnd())
            xml_chars += count.read(1024).length();
    } else
        xml_chars = tmp_string.length();
    stream << xml_chars << "\n";
    stream.flush();

    // Copy QXmlStreamWriter's output to device
    if (tmp_file.isOpen()) {
        tmp_file.seek(0);
        qint64 bytes_read;
        char buffer[1024];
        while ((bytes_read = tmp_file.read(buffer, 1024)) > 0)
            device->write(buffer, bytes_read);
    } else
        stream << tmp_string;
    stream << "\n";

    // write geometry and end tag
    stream << geometry << "\n";
    stream << "</table>\n";
}

QString Table::saveHeader()
{
    // TODO: obsolete, remove for 0.3.0, only needed for template saving
    QString s = "header";
    for (int j = 0; j < numCols(); j++) {
        switch (column(j)->plotDesignation()) {
        case SciDAVis::X:
            s += "\t" + colLabel(j) + "[X]";
            break;
        case SciDAVis::Y:
            s += "\t" + colLabel(j) + "[Y]";
            break;
        case SciDAVis::Z:
            s += "\t" + colLabel(j) + "[Z]";
            break;
        case SciDAVis::xErr:
            s += "\t" + colLabel(j) + "[xEr]";
            break;
        case SciDAVis::yErr:
            s += "\t" + colLabel(j) + "[yEr]";
            break;
        default:
            s += "\t" + colLabel(j);
        }
    }
    return s += "\n";
}

int Table::firstXCol()
{
    for (int j = 0; j < numCols(); j++) {
        if (column(j)->plotDesignation() == SciDAVis::X)
            return j;
    }
    return -1;
}

void Table::setColComment(int col, const QString &s)
{
    column(col)->setComment(s);
}

void Table::setColName(int col, const QString &text)
{
    if (col < 0 || col >= numCols())
        return;

    column(col)->setName(text);
}

QStringList Table::selectedColumns()
{
    // TODO for 0.3.0: extended selection support, Column * lists
    QStringList names;
    for (int i = 0; i < numCols(); i++) {
        if (isColumnSelected(i))
            names << name() + "_" + column(i)->name();
    }
    return names;
}

QStringList Table::YColumns()
{
    // TODO for 0.3.0: Column * list
    QStringList names;
    for (int i = 0; i < numCols(); i++) {
        if (column(i)->plotDesignation() == SciDAVis::Y)
            names << name() + "_" + column(i)->name();
    }
    return names;
}

QStringList Table::selectedYColumns()
{
    // TODO for 0.3.0: Column * list
    QStringList names;
    for (int i = 0; i < numCols(); i++) {
        if (isColumnSelected(i) && column(i)->plotDesignation() == SciDAVis::Y)
            names << name() + "_" + column(i)->name();
    }
    return names;
}

QStringList Table::selectedErrColumns()
{
    // TODO for 0.3.0: Column * list
    QStringList names;
    for (int i = 0; i < numCols(); i++) {
        if (isColumnSelected(i)
            && (column(i)->plotDesignation() == SciDAVis::xErr
                || column(i)->plotDesignation() == SciDAVis::yErr))
            names << name() + "_" + column(i)->name();
    }
    return names;
}

QStringList Table::drawableColumnSelection()
{
    // TODO for 0.3.0: Column * list
    QStringList names;
    for (int i = 0; i < numCols(); i++) {
        if (isColumnSelected(i) && column(i)->plotDesignation() == SciDAVis::Y)
            names << name() + "_" + column(i)->name();
    }

    for (int i = 0; i < numCols(); i++) {
        if (isColumnSelected(i)
            && (column(i)->plotDesignation() == SciDAVis::xErr
                || column(i)->plotDesignation() == SciDAVis::yErr))
            names << name() + "_" + column(i)->name();
    }
    return names;
}

QMap<int, QString> Table::selectedYLabels()
{
    // TODO for 0.3.0: Column * list
    QMap<int, QString> names;
    for (int i = 0; i < numCols(); i++) {
        if (isColumnSelected(i) && column(i)->plotDesignation() == SciDAVis::Y)
            names.insert(i, column(i)->name());
    }
    return names;
}

QStringList Table::columnsList()
{
    // TODO for 0.3.0: Column * list
    QStringList names;
    for (int i = 0; i < numCols(); i++)
        names << name() + "_" + column(i)->name();

    return names;
}

int Table::numSelectedRows()
{
    return selectedRowCount();
}

int Table::selectedColsNumber()
{
    return selectedColumnCount();
}

QString Table::colName(int col)
{ // returns the table name + horizontal header text
    if (col < 0 || col >= numCols())
        return QString();

    return QString(name() + "_" + column(col)->name());
}

void Table::insertCols(int start, int count)
{
    if (d_future_table) {
        if (start < 0)
            start = 0;

        QList<Column *> cols;
        for (int i = 0; i < count; i++)
            cols << new Column(QString::number(i + 1), SciDAVis::ColumnMode::Numeric);
        d_future_table->insertColumns(start, cols);
    }
}

void Table::insertCol()
{
    if (d_future_table)
        d_future_table->insertEmptyColumns();
}

void Table::insertRow()
{
    if (d_future_table)
        d_future_table->insertEmptyRows();
}

void Table::addCol(SciDAVis::PlotDesignation pd)
{
    if (d_future_table) {
        d_future_table->addColumn();
        column(d_future_table->columnCount() - 1)
                ->setColumnMode(SciDAVis::ColumnMode::Numeric); // in case we ever change the default
        column(d_future_table->columnCount() - 1)->setPlotDesignation(pd);
    }
}

void Table::addColumns(int c)
{
    if (d_future_table) {
        QList<Column *> cols;
        for (int i = 0; i < c; i++)
            cols << new Column(QString::number(i + 1), SciDAVis::ColumnMode::Numeric);
        d_future_table->appendColumns(cols);
    }
}

void Table::clearCol()
{
    if (d_future_table)
        d_future_table->clearSelectedColumns();
}

void Table::clearCell(int row, int col)
{
    column(col)->setTextAt(row, QString());
    column(col)->setValueAt(row, 0.0);
    column(col)->setDateTimeAt(row, QDateTime());
    column(col)->setInvalid(row, true);
}

void Table::deleteSelectedRows()
{
    if (d_future_table)
        d_future_table->removeSelectedRows();
}

void Table::cutSelection()
{
    if (d_future_table)
        d_future_table->cutSelection();
}

void Table::selectAllTable()
{
    selectAll();
}

void Table::deselect()
{
    d_view_widget->clearSelection();
}

void Table::clearSelection()
{
    if (!d_future_table)
        return;

    int firstRow = firstSelectedRow(false);
    int lastRow = lastSelectedRow(false);
    int firstCol = firstSelectedColumn(false);
    int lastCol = lastSelectedColumn(false);

    d_future_table->beginMacro(tr("%1: clear selected cell(s)").arg(name()));
    if (firstRow >= 0 && firstCol >= 0) {
        for (int c = firstCol; c <= lastCol && c < numCols(); ++c) {
            Column *colPtr = column(c);
            if (!colPtr)
                continue;
            for (int r = firstRow; r <= lastRow && r < numRows(); ++r) {
                if (isCellSelected(r, c)) {
                    if (formulaModeActive())
                        colPtr->setFormula(r, QString());
                    else
                        clearCell(r, c);
                }
            }
        }
    } else {
        int curRow = -1, curCol = -1;
        getCurrentCell(&curRow, &curCol);
        if (curRow >= 0 && curRow < numRows() && curCol >= 0 && curCol < numCols()) {
            if (formulaModeActive())
                column(curCol)->setFormula(curRow, QString());
            else
                clearCell(curRow, curCol);
        }
    }
    d_future_table->endMacro();
}

void Table::copySelection()
{
    if (d_future_table)
        d_future_table->copySelection();
}

void Table::pasteSelection()
{
    if (!d_future_table)
        return;

    const QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard)
        return;
    QString rawText = clipboard->text();
    if (rawText.isEmpty())
        return;

    QString normalized = rawText;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    QStringList lines = normalized.split('\n');
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    if (lines.isEmpty())
        return;

    bool hasTab = normalized.contains('\t');
    bool hasComma = !hasTab && normalized.contains(',');

    QList<QStringList> grid;
    int maxCols = 0;
    for (const QString &line : lines) {
        QStringList rowCells;
        if (hasTab) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            rowCells = line.split('\t', Qt::KeepEmptyParts);
#else
            rowCells = line.split('\t', QString::KeepEmptyParts);
#endif
        } else if (hasComma) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            rowCells = line.split(',', Qt::KeepEmptyParts);
#else
            rowCells = line.split(',', QString::KeepEmptyParts);
#endif
        } else {
            QString trimmedLine = line.trimmed();
            if (trimmedLine.isEmpty()) {
                rowCells << QString();
            } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                rowCells = trimmedLine.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
#else
                rowCells = trimmedLine.split(QRegExp("\\s+"), QString::SkipEmptyParts);
#endif
            }
        }
        for (int i = 0; i < rowCells.size(); ++i)
            rowCells[i] = rowCells[i].trimmed();
        maxCols = qMax(maxCols, rowCells.size());
        grid.append(rowCells);
    }

    int pastedRows = grid.size();
    int pastedCols = maxCols;
    if (pastedRows <= 0 || pastedCols <= 0)
        return;

    int startRow = firstSelectedRow(false);
    int startCol = firstSelectedColumn(false);
    if (startRow < 0 || startCol < 0)
        getCurrentCell(&startRow, &startCol);
    if (startRow < 0)
        startRow = 0;
    if (startCol < 0)
        startCol = 0;

    d_future_table->beginMacro(tr("%1: paste from clipboard").arg(name()));

    int neededCols = startCol + pastedCols;
    int neededRows = startRow + pastedRows;
    if (neededCols > numCols())
        setNumCols(neededCols);
    if (neededRows > numRows())
        setNumRows(neededRows);

    auto isMissingToken = [](const QString &s) -> bool {
        if (s.isEmpty() || s == "-" || s == "--" || s == "---")
            return true;
        if (s.compare("nan", Qt::CaseInsensitive) == 0
            || s.compare("na", Qt::CaseInsensitive) == 0
            || s.compare("n/a", Qt::CaseInsensitive) == 0
            || s.compare("null", Qt::CaseInsensitive) == 0)
            return true;
        return false;
    };

    auto tryParseDouble = [](const QString &s, double &outVal) -> bool {
        bool ok = false;
        outVal = QLocale::c().toDouble(s, &ok);
        if (ok)
            return true;
        outVal = QLocale().toDouble(s, &ok);
        if (ok)
            return true;
        QString alt = s;
        alt.replace(',', '.');
        outVal = QLocale::c().toDouble(alt, &ok);
        return ok;
    };

    for (int c = 0; c < pastedCols; ++c) {
        int targetCol = startCol + c;
        Column *colPtr = column(targetCol);
        if (!colPtr)
            continue;

        for (int r = 0; r < pastedRows; ++r) {
            int targetRow = startRow + r;
            QString cellStr = (c < grid[r].size()) ? grid[r][c] : QString();

            if (formulaModeActive()) {
                colPtr->setFormula(targetRow, cellStr);
                colPtr->setInvalid(targetRow, false);
                continue;
            }

            if (isMissingToken(cellStr)) {
                clearCell(targetRow, targetCol);
                continue;
            }

            if (colPtr->columnMode() == SciDAVis::ColumnMode::Numeric) {
                double val = 0.0;
                if (tryParseDouble(cellStr, val) && !std::isnan(val)) {
                    colPtr->setValueAt(targetRow, val);
                    if (colPtr->isInvalid(targetRow))
                        colPtr->setInvalid(targetRow, false);
                } else {
                    clearCell(targetRow, targetCol);
                }
            } else {
                setText(targetRow, targetCol, cellStr);
            }
        }
    }

    deselectAll();
    setCellsSelected(startRow, startCol, startRow + pastedRows - 1, startCol + pastedCols - 1, true);
    goToCell(startRow, startCol);

    d_future_table->endMacro();
}

void Table::removeCol()
{
    if (d_future_table)
        d_future_table->removeSelectedColumns();
}

void Table::removeCol(const QStringList &list)
{
    if (d_future_table) {
        foreach (QString name, list)
            d_future_table->removeColumns(colIndex(name), 1);
    }
}

int Table::numRows()
{
    return d_future_table ? d_future_table->rowCount() : 0;
}

int Table::numCols()
{
    return d_future_table ? d_future_table->columnCount() : 0;
}

int Table::rowCount()
{
    return d_future_table ? d_future_table->rowCount() : 0;
}

int Table::columnCount()
{
    return d_future_table ? d_future_table->columnCount() : 0;
}

double Table::cell(int row, int col)
{
    Column *colPtr = column(col);
    if (!colPtr)
        return 0.0;
    if (!colPtr->isInvalid(row)) {
        if (colPtr->columnMode() == SciDAVis::ColumnMode::Text) {
            QString yval = colPtr->textAt(row);
            bool valid_data = true;
            double dbval = QLocale().toDouble(yval, &valid_data);
            if (!valid_data)
                return 0.0;
            return dbval;
        }
        return colPtr->valueAt(row);
    } else
        return 0.0;
}

void Table::setCell(int row, int col, double val)
{
    column(col)->setValueAt(row, val);
}

QString Table::text(int row, int col)
{
    Column *colPtr = column(col);
    if (!colPtr)
        return QString();
    return colPtr->asStringColumn()->textAt(row);
}

void Table::setText(int row, int col, const QString &text)
{
    column(col)->asStringColumn()->setTextAt(row, text);
}

void Table::importV0x0001XXHeader(QStringList header)
{
    if (!d_future_table)
        return;
    QStringList col_label = QStringList();
    QList<SciDAVis::PlotDesignation> col_plot_type = QList<SciDAVis::PlotDesignation>();
    for (int i = 0; i < header.count(); i++) {
        if (header[i].isEmpty())
            continue;

        QString s = header[i].replace("_", "-");
        if (s.contains("[X]")) {
            col_label << s.remove("[X]");
            col_plot_type << SciDAVis::X;
        } else if (s.contains("[Y]")) {
            col_label << s.remove("[Y]");
            col_plot_type << SciDAVis::Y;
        } else if (s.contains("[Z]")) {
            col_label << s.remove("[Z]");
            col_plot_type << SciDAVis::Z;
        } else if (s.contains("[xEr]")) {
            col_label << s.remove("[xEr]");
            col_plot_type << SciDAVis::xErr;
        } else if (s.contains("[yEr]")) {
            col_label << s.remove("[yEr]");
            col_plot_type << SciDAVis::yErr;
        } else {
            col_label << s;
            col_plot_type << SciDAVis::noDesignation;
        }
    }
    QList<Column *> quarantine;
    for (int i = 0; i < col_label.count() && i < d_future_table->columnCount(); i++)
        quarantine << column(i);
    int i = 0;
    foreach (Column *col, quarantine) {
        d_future_table->removeChild(col, true);
        // setting column name while col is still part of table triggers renaming
        // to prevent name clashes
        col->setName(col_label.at(i));
        col->setPlotDesignation(col_plot_type.at(i));
        i++;
    }
    d_future_table->appendColumns(quarantine);
}

void Table::setHeader(QStringList header)
{
    if (!d_future_table)
        return;
    QList<Column *> quarantine;
    for (int i = 0; i < header.count() && i < d_future_table->columnCount(); i++)
        quarantine << column(i);
    int i = 0;
    foreach (Column *col, quarantine) {
        d_future_table->removeChild(col, true);
        // setting column name while col is still part of table triggers renaming
        // to prevent name clashes
        col->setName(header.at(i));
        i++;
    }
    d_future_table->appendColumns(quarantine);
}

int Table::colIndex(const QString &name)
{
    return d_future_table ? d_future_table->columnIndex(column(name)) : 0;
}

bool Table::noXColumn()
{
    return d_future_table ? d_future_table->columnCount(SciDAVis::X) == 0 : true;
}

bool Table::noYColumn()
{
    return d_future_table ? d_future_table->columnCount(SciDAVis::Y) == 0 : true;
}

bool Table::exportASCII(const QString &fname, const QString &separator, bool withLabels,
                        bool exportSelection)
{
    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(0, tr("ASCII Export Error"),
                              tr("Could not write to file: <br><h4>") + fname
                                      + tr("</h4><p>Please verify that you have the right to write "
                                           "to this location!"));
        return false;
    }

    QTextStream out(&file);
    int i, j;
    int rows = numRows();
    int cols = numCols();
    int selectedCols = 0;
    int topRow = 0, bottomRow = 0;
    vector<int> sCols;
    if (exportSelection) {
        for (i = 0; i < cols; i++) {
            if (isColumnSelected(i))
                selectedCols++;
        }

        sCols.resize(selectedCols + 1);
        int temp = 1;
        for (i = 0; i < cols; i++) {
            if (isColumnSelected(i)) {
                sCols[temp] = i;
                temp++;
            }
        }

        topRow = firstSelectedRow();
        bottomRow = lastSelectedRow();
    }

    if (withLabels) {
        QStringList header = colNames();
        QStringList ls = header.filter(QRegExp("\\D"));
        if (exportSelection) {
            for (i = 1; i < selectedCols; i++) {
                if (ls.count() > 0)
                    out << header[sCols[i]] + separator;
                else
                    out << "C" + header[sCols[i]] + separator;
            }

            if (ls.count() > 0)
                out << header[sCols[selectedCols]] + "\n";
            else
                out << "C" + header[sCols[selectedCols]] + "\n";
        } else {
            if (ls.count() > 0) {
                for (j = 0; j < cols - 1; j++)
                    out << header[j] + separator;
                out << header[cols - 1] + "\n";
            } else {
                for (j = 0; j < cols - 1; j++)
                    out << "C" + header[j] + separator;
                out << "C" + header[cols - 1] + "\n";
            }
        }
    } // finished writting labels

    QList<Column *> col_ptrs;
    if (exportSelection) {
        for (j = 1; j <= selectedCols; j++)
            col_ptrs << column(sCols[j]);
    } else {
        for (j = 0; j < cols; j++)
            col_ptrs << column(j);
        topRow = 0;
        bottomRow = rows - 1;
    }

    for (i = topRow; i <= bottomRow; i++) {
        bool first = true;
        foreach (Column *col, col_ptrs) {
            if (first)
                first = false;
            else
                out << separator;
            out << col->asStringColumn()->textAt(i);
        }
        out << "\n";
    }

    file.close();
    return true;
}

void Table::customEvent(QEvent *e)
{
    if (e->type() == SCRIPTING_CHANGE_EVENT)
        scriptingChangeEvent((ScriptingChangeEvent *)e);
}

void Table::closeEvent(QCloseEvent *e)
{
    if (askOnClose) {
        switch (QMessageBox::information(this, tr("ToriGraph"),
                                         tr("Do you want to hide or delete") + "<p><b>'"
                                                 + objectName() + "'</b> ?",
                                         tr("Delete"), tr("Hide"), tr("Cancel"), 0, 2)) {
        case 0:
            e->accept();
            if (d_future_table)
                d_future_table->remove();
            return;

        case 1:
            e->ignore();
            emit hiddenWindow(this);
            break;

        case 2:
            e->ignore();
            break;
        }
    } else {
        e->accept();
        if (d_future_table)
            d_future_table->remove();
        return;
    }
}

void Table::setNumRows(int rows)
{
    if (d_future_table)
        d_future_table->setRowCount(rows);
}

void Table::setNumCols(int cols)
{
    if (d_future_table)
        d_future_table->setColumnCount(cols);
}

void Table::copy(Table *m)
{
    if (!m || !d_future_table)
        return;

    d_future_table->copy(m->d_future_table);
}

QString Table::saveAsTemplate(const QString &geometryInfo)
{
    QString s = "<table>\t" + QString::number(numRows()) + "\t";
    s += QString::number(numCols()) + "\n";
    s += geometryInfo;
    s += saveHeader();
    s += saveColumnWidths();
    s += saveCommands();
    s += saveColumnTypes();
    s += saveComments();
    s += "</table>\n";
    return s;
}

void Table::restore(const QStringList &list_in)
{
    const bool prevAutoRecalc = d_auto_recalc_formulas;
    d_auto_recalc_formulas = false;

    // TODO: obsolete, remove in 0.3.0, only needed for template loading
    QStringList temp_list;
    QStringList::const_iterator iterator = list_in.begin();

    temp_list = (*iterator++).split("\t");
    temp_list.removeFirst();
    importV0x0001XXHeader(temp_list);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    setColWidths((*iterator).right((*iterator).length() - 9).split("\t", Qt::SkipEmptyParts));
#else
    setColWidths((*iterator).right((*iterator).length() - 9).split("\t", QString::SkipEmptyParts));
#endif
    iterator++;

    temp_list = (*iterator++).split("\t");
    if (temp_list[0] == "com") {
        temp_list.removeFirst();
        setCommands(temp_list);
    } else if (temp_list[0] == "<com>") {
        QStringList commands;
        for (int col = 0; col < numCols(); col++)
            commands << "";
        for (; iterator != list_in.end() && *iterator != "</com>"; iterator++) {
            int col = (*iterator).mid(9, (*iterator).length() - 11).toInt();
            QString formula;
            for (iterator++; iterator != list_in.end() && *iterator != "</col>"; iterator++)
                formula += *iterator + "\n";
            formula.truncate(formula.length() - 1);
            commands[col] = formula;
        }
        iterator++;
        setCommands(commands);
    }

    temp_list = (*iterator++).split("\t");
    temp_list.removeFirst();
    setColumnTypes(temp_list);

    temp_list = (*iterator++).split("\t");
    temp_list.removeFirst();
    setColComments(temp_list);

    d_auto_recalc_formulas = prevAutoRecalc;
}

void Table::clear()
{
    if (d_future_table)
        d_future_table->clear();
}

QStringList Table::colNames()
{
    QStringList list;
    if (d_future_table)
        for (int i = 0; i < d_future_table->columnCount(); i++)
            list << column(i)->name();
    return list;
}

QString Table::colLabel(int col)
{
    return column(col)->name();
}

SciDAVis::PlotDesignation Table::colPlotDesignation(int col)
{
    return column(col)->plotDesignation();
}

void Table::setColPlotDesignation(int col, SciDAVis::PlotDesignation d)
{
    column(col)->setPlotDesignation(d);
}

QList<int> Table::plotDesignations()
{
    QList<int> list;
    if (d_future_table)
        for (int i = 0; i < d_future_table->columnCount(); i++)
            list << column(i)->plotDesignation();
    return list;
}

QList<SciDAVis::ColumnMode> Table::columnTypes()
{
    QList<SciDAVis::ColumnMode> list;
    if (d_future_table)
        for (int i = 0; i < d_future_table->columnCount(); i++)
            list << column(i)->columnMode();
    return list;
}

SciDAVis::ColumnMode Table::columnType(int col)
{
    return column(col)->columnMode();
}

void Table::setColumnTypes(QList<SciDAVis::ColumnMode> ctl)
{
    if (!d_future_table)
        return;
    Q_ASSERT(ctl.size() == d_future_table->columnCount());
    for (int i = 0; i < d_future_table->columnCount(); i++)
        column(i)->setColumnMode(ctl.at(i));
}

void Table::setColumnType(int col, SciDAVis::ColumnMode mode)
{
    column(col)->setColumnMode(mode);
}

QString Table::columnFormat(int col)
{
    // TODO: obsolete, remove in 0.3.0
    Column *col_ptr = column(col);
    if (col_ptr->columnMode() != SciDAVis::ColumnMode::DateTime && col_ptr->columnMode() != SciDAVis::ColumnMode::Month
        && col_ptr->columnMode() != SciDAVis::ColumnMode::Day)
        return QString();

    DateTime2StringFilter *filter = static_cast<DateTime2StringFilter *>(col_ptr->outputFilter());
    return filter->format();
}

void Table::columnNumericFormat(int col)
{
    if (!d_future_table || numCols() <= 0)
        return;
    if (col < 0 || col >= numCols())
        col = firstSelectedColumn(false);
    if (col < 0 || col >= numCols())
        col = 0;

    if (numRows() > 0) {
        deselectAll();
        setCellsSelected(0, col, numRows() - 1, col, true);
        goToCell(0, col);
    }
    setColumnForControlTabs(col);
    d_future_table->setCurrentColumn(col);
    showControlTypeTab();
}

void Table::showColMenu(int col)
{
    columnNumericFormat(col);
}

bool Table::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress
        && (watched == this || watched == d_view_widget
            || (d_view_widget && watched == d_view_widget->viewport()))) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->matches(QKeySequence::Paste)) {
            pasteSelection();
            return true;
        }
        if (ke->matches(QKeySequence::Copy)) {
            copySelection();
            return true;
        }
        if (ke->matches(QKeySequence::Cut)) {
            cutSelection();
            return true;
        }
        if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
            clearSelection();
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            int curRow = -1, curCol = -1;
            getCurrentCell(&curRow, &curCol);
            if (curRow < 0 || curCol < 0 || !isCellSelected(curRow, curCol)) {
                int selRow = lastSelectedRow(false);
                int selCol = firstSelectedColumn(false);
                if (selRow >= 0 && selCol >= 0) {
                    curRow = selRow;
                    curCol = selCol;
                }
            }
            if (curRow < 0)
                curRow = 0;
            if (curCol < 0)
                curCol = 0;
            int nextRow = curRow + 1;
            if (nextRow >= numRows())
                setNumRows(nextRow + 1);
            deselectAll();
            goToCell(nextRow, curCol);
            setCellSelected(nextRow, curCol, true);
            return true;
        }
        if (ke->key() == Qt::Key_Tab) {
            int curRow = -1, curCol = -1;
            getCurrentCell(&curRow, &curCol);
            if (curRow < 0 || curCol < 0 || !isCellSelected(curRow, curCol)) {
                int selRow = firstSelectedRow(false);
                int selCol = lastSelectedColumn(false);
                if (selRow >= 0 && selCol >= 0) {
                    curRow = selRow;
                    curCol = selCol;
                }
            }
            if (curRow < 0)
                curRow = 0;
            if (curCol < 0)
                curCol = 0;
            if (numCols() > 0) {
                int nextCol = (curCol + 1) % numCols();
                deselectAll();
                goToCell(curRow, nextCol);
                setCellSelected(curRow, nextCol, true);
            }
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonDblClick
               && (watched == d_horizontal_header
                   || (d_horizontal_header && watched == d_horizontal_header->viewport()))) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        int col = d_horizontal_header->logicalIndexAt(me->pos());
        if (col >= 0 && col < numCols()) {
            showColumnPropertiesDialog(col);
            return true;
        }
    }

    return TableView::eventFilter(watched, event);
}

int Table::verticalHeaderWidth()
{
    return d_view_widget->verticalHeader()->width();
}

QString Table::colComment(int col)
{
    return column(col)->comment();
}

QStringList Table::colComments()
{
    QStringList list;
    if (d_future_table)
        for (int i = 0; i < d_future_table->columnCount(); i++)
            list << column(i)->comment();
    return list;
}

void Table::setColComments(const QStringList &list)
{
    if (d_future_table)
        for (int i = 0; i < d_future_table->columnCount(); i++)
            column(i)->setComment(list.at(i));
}

bool Table::commentsEnabled()
{
    return areCommentsShown();
}

QString Table::colLongName(int col) const
{
    const Column *c = column(col);
    return c ? c->longName() : QString();
}

void Table::setColLongName(int col, const QString &s)
{
    if (Column *c = column(col))
        c->setLongName(s);
}

QString Table::colUnits(int col) const
{
    const Column *c = column(col);
    return c ? c->units() : QString();
}

void Table::setColUnits(int col, const QString &s)
{
    if (Column *c = column(col))
        c->setUnits(s);
}

void Table::setAutoRecalcFormulas(bool on)
{
    d_auto_recalc_formulas = on;
    if (d_auto_recalc_formulas && !d_in_auto_recalc)
        autoRecalculateDependentColumns(-1, 0);
}

bool Table::autoRecalcFormulas() const
{
    return d_auto_recalc_formulas;
}

void Table::showColumnPropertiesDialog(int col)
{
    if (!d_future_table || numCols() <= 0)
        return;
    if (col < 0 || col >= numCols())
        col = firstSelectedColumn(false);
    if (col < 0 || col >= numCols()) {
        if (d_future_table->currentColumn())
            col = d_future_table->columnIndex(d_future_table->currentColumn());
    }
    if (col < 0 || col >= numCols())
        col = 0;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Column Properties"));
    dlg.setMinimumWidth(480);

    QVBoxLayout *mainLay = new QVBoxLayout(&dlg);

    // Top bar: Column selector + Prev / Next navigation
    QHBoxLayout *navLay = new QHBoxLayout();
    QLabel *lblColTarget = new QLabel(tr("Column:"), &dlg);
    QComboBox *boxColSelect = new QComboBox(&dlg);
    boxColSelect->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QPushButton *btnPrevCol = new QPushButton(tr("<< Previous"), &dlg);
    QPushButton *btnNextCol = new QPushButton(tr("Next >>"), &dlg);
    navLay->addWidget(lblColTarget);
    navLay->addWidget(boxColSelect, 1);
    navLay->addWidget(btnPrevCol);
    navLay->addWidget(btnNextCol);
    mainLay->addLayout(navLay);

    // Labels & Designation group
    QGroupBox *grpMeta = new QGroupBox(tr("Column Labels & Designation"), &dlg);
    QFormLayout *metaForm = new QFormLayout(grpMeta);
    QLineEdit *editShortName = new QLineEdit(grpMeta);
    QLineEdit *editLongName = new QLineEdit(grpMeta);
    editLongName->setPlaceholderText(tr("e.g. Nominal Stress, Temperature"));
    QLineEdit *editUnits = new QLineEdit(grpMeta);
    editUnits->setPlaceholderText(tr("e.g. MPa, %, K, eV"));

    QComboBox *boxDesignation = new QComboBox(grpMeta);
    boxDesignation->addItem(tr("X (Abscissa)"), int(SciDAVis::X));
    boxDesignation->addItem(tr("Y (Ordinate)"), int(SciDAVis::Y));
    boxDesignation->addItem(tr("Z"), int(SciDAVis::Z));
    boxDesignation->addItem(tr("X Error"), int(SciDAVis::xErr));
    boxDesignation->addItem(tr("Y Error"), int(SciDAVis::yErr));
    boxDesignation->addItem(tr("None (Disregard)"), int(SciDAVis::noDesignation));

    QHBoxLayout *typeRowLay = new QHBoxLayout();
    QComboBox *boxDataType = new QComboBox(grpMeta);
    boxDataType->addItem(tr("Numeric"), int(SciDAVis::ColumnMode::Numeric));
    boxDataType->addItem(tr("Text"), int(SciDAVis::ColumnMode::Text));
    boxDataType->addItem(tr("Date and time"), int(SciDAVis::ColumnMode::DateTime));
    boxDataType->addItem(tr("Month names"), int(SciDAVis::ColumnMode::Month));
    boxDataType->addItem(tr("Day names"), int(SciDAVis::ColumnMode::Day));
    QPushButton *btnFormatPanel = new QPushButton(tr("Type & Format..."), grpMeta);
    typeRowLay->addWidget(boxDataType, 1);
    typeRowLay->addWidget(btnFormatPanel);

    QPlainTextEdit *editComments = new QPlainTextEdit(grpMeta);
    editComments->setFixedHeight(56);

    metaForm->addRow(tr("Short Name:"), editShortName);
    metaForm->addRow(tr("Long Name:"), editLongName);
    metaForm->addRow(tr("Units:"), editUnits);
    metaForm->addRow(tr("Plot Designation:"), boxDesignation);
    metaForm->addRow(tr("Data Type:"), typeRowLay);
    metaForm->addRow(tr("Comments:"), editComments);
    mainLay->addWidget(grpMeta);

    // Formula F(x)= group
    QGroupBox *grpFormula = new QGroupBox(tr("F(x)= Column Formula"), &dlg);
    QVBoxLayout *fLay = new QVBoxLayout(grpFormula);
    QHBoxLayout *refLay = new QHBoxLayout();
    QComboBox *boxRefCol = new QComboBox(grpFormula);
    QPushButton *btnInsertRef = new QPushButton(tr("Insert Column"), grpFormula);
    refLay->addWidget(new QLabel(tr("Reference:"), grpFormula));
    refLay->addWidget(boxRefCol, 1);
    refLay->addWidget(btnInsertRef);
    fLay->addLayout(refLay);

    QPlainTextEdit *editFormula = new QPlainTextEdit(grpFormula);
    editFormula->setFixedHeight(64);
    editFormula->setPlaceholderText(QStringLiteral("col(\"A\") * 2.0 + 1.0"));
    fLay->addWidget(editFormula);

    QCheckBox *chkAutoRecalc =
            new QCheckBox(tr("Auto-recalculate formulas when source cells change"), grpFormula);
    chkAutoRecalc->setChecked(d_auto_recalc_formulas);
    fLay->addWidget(chkAutoRecalc);
    mainLay->addWidget(grpFormula);

    QDialogButtonBox *btnBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, &dlg);
    mainLay->addWidget(btnBox);

    int currentCol = col;
    bool loadingUi = false;

    auto rebuildCombos = [&]() {
        loadingUi = true;
        boxColSelect->clear();
        boxRefCol->clear();
        for (int i = 0; i < numCols(); ++i) {
            Column *cp = column(i);
            if (!cp)
                continue;
            const QString disp = cp->longName().trimmed().isEmpty()
                    ? QString("%1. %2").arg(i + 1).arg(cp->name())
                    : QString("%1. %2 (%3)").arg(i + 1).arg(cp->name(), cp->longName().trimmed());
            boxColSelect->addItem(disp, i);
            boxRefCol->addItem(QString("col(\"%1\")").arg(cp->name()));
        }
        boxColSelect->setCurrentIndex(qBound(0, currentCol, qMax(0, boxColSelect->count() - 1)));
        loadingUi = false;
    };

    auto loadColIntoUi = [&](int cIdx) {
        if (cIdx < 0 || cIdx >= numCols())
            return;
        currentCol = cIdx;
        Column *cp = column(cIdx);
        if (!cp)
            return;
        loadingUi = true;
        boxColSelect->setCurrentIndex(cIdx);
        editShortName->setText(cp->name());
        editLongName->setText(cp->longName());
        editUnits->setText(cp->units());
        const int pdIdx = boxDesignation->findData(int(cp->plotDesignation()));
        boxDesignation->setCurrentIndex(pdIdx >= 0 ? pdIdx : 0);
        const int dtIdx = boxDataType->findData(int(cp->columnMode()));
        boxDataType->setCurrentIndex(dtIdx >= 0 ? dtIdx : 0);
        editComments->setPlainText(cp->comment());

        QString fx;
        const QList<Interval<int>> ivs = cp->formulaIntervals();
        for (const Interval<int> &iv : ivs) {
            const QString f = cp->formula(iv.start()).trimmed();
            if (!f.isEmpty()) {
                fx = f;
                break;
            }
        }
        if (fx.isEmpty())
            fx = cp->formula(0).trimmed();
        editFormula->setPlainText(fx);
        loadingUi = false;
    };

    auto applyUiToColumn = [&]() -> bool {
        if (currentCol < 0 || currentCol >= numCols())
            return false;
        Column *cp = column(currentCol);
        if (!cp)
            return false;

        d_auto_recalc_formulas = chkAutoRecalc->isChecked();
        const QString newShort = editShortName->text().trimmed();
        if (!newShort.isEmpty() && newShort != cp->name())
            cp->setName(newShort);
        cp->setLongName(editLongName->text().trimmed());
        cp->setUnits(editUnits->text().trimmed());
        cp->setComment(editComments->toPlainText());
        cp->setPlotDesignation(
                static_cast<SciDAVis::PlotDesignation>(boxDesignation->currentData().toInt()));
        const SciDAVis::ColumnMode newMode =
                static_cast<SciDAVis::ColumnMode>(boxDataType->currentData().toInt());
        if (newMode != cp->columnMode())
            cp->setColumnMode(newMode);

        const QString fx = editFormula->toPlainText().trimmed();
        if (!fx.isEmpty()) {
            if (cp->rowCount() < numRows())
                cp->insertRows(cp->rowCount(), numRows() - cp->rowCount());
            const bool prevGuard = d_in_auto_recalc;
            d_in_auto_recalc = true;
            cp->setFormula(Interval<int>(0, qMax(0, numRows() - 1)), fx);
            d_in_auto_recalc = prevGuard;
            if (!recalculate(currentCol, false))
                return false;
            if (d_auto_recalc_formulas && !d_in_auto_recalc)
                autoRecalculateDependentColumns(currentCol, 1);
        } else if (!cp->formulaIntervals().isEmpty()) {
            cp->clearFormulas();
        }

        if (!cp->longName().isEmpty() || !cp->units().isEmpty()
            || !cp->comment().isEmpty() || !fx.isEmpty()) {
            showComments(true);
        }
        setColumnForControlTabs(currentCol);
        d_future_table->setCurrentColumn(currentCol);
        rebuildCombos();
        return true;
    };

    rebuildCombos();
    loadColIntoUi(currentCol);
    editLongName->setFocus();

    connect(btnInsertRef, &QPushButton::clicked, &dlg, [&]() {
        editFormula->insertPlainText(boxRefCol->currentText());
        editFormula->setFocus();
    });
    connect(boxColSelect, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            &dlg, [&](int newIdx) {
                if (loadingUi || newIdx < 0 || newIdx >= numCols() || newIdx == currentCol)
                    return;
                applyUiToColumn();
                loadColIntoUi(newIdx);
            });
    connect(btnPrevCol, &QPushButton::clicked, &dlg, [&]() {
        if (numCols() <= 1)
            return;
        applyUiToColumn();
        loadColIntoUi((currentCol - 1 + numCols()) % numCols());
    });
    connect(btnNextCol, &QPushButton::clicked, &dlg, [&]() {
        if (numCols() <= 1)
            return;
        applyUiToColumn();
        loadColIntoUi((currentCol + 1) % numCols());
    });
    connect(btnFormatPanel, &QPushButton::clicked, &dlg, [&]() {
        applyUiToColumn();
        dlg.accept();
        columnNumericFormat(currentCol);
    });
    if (QPushButton *applyBtn = btnBox->button(QDialogButtonBox::Apply)) {
        connect(applyBtn, &QPushButton::clicked, &dlg, [&]() { applyUiToColumn(); });
    }
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, [&]() {
        if (applyUiToColumn())
            dlg.accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    dlg.exec();
}

void Table::applyFormula()
{
    if (!d_future_table)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    d_future_table->beginMacro(tr("%1: apply formula to column").arg(name()));

    QString formula = ui.formula_box->toPlainText();
    for (int col = firstSelectedColumn(); col <= lastSelectedColumn(); col++) {
        Column *col_ptr = column(col);
        col_ptr->insertRows(col_ptr->rowCount(), rowCount() - col_ptr->rowCount());
        col_ptr->setFormula(Interval<int>(0, rowCount() - 1), formula);
        if (!recalculate(col, false))
            break;
    }

    d_future_table->endMacro();
    QApplication::restoreOverrideCursor();
}

void Table::addFunction()
{
    static_cast<ScriptEdit *>(ui.formula_box)
            ->insertFunction(ui.add_function_combobox->currentText());
}

void Table::addReference()
{
    ui.formula_box->insertPlainText(ui.add_reference_combobox->currentText());
}

void Table::updateFunctionDoc()
{
    ui.add_function_combobox->setToolTip(
            scriptEnv->mathFunctionDoc(ui.add_function_combobox->currentText()));
}

void Table::handleAspectDescriptionAboutToChange(const AbstractAspect *aspect)
{
    const Column *col = qobject_cast<const Column *>(aspect);
    if (col && d_future_table && d_future_table->columnIndex(col) != -1) {
        d_stored_column_labels[col] = aspect->name();
    }
}

void Table::handleAspectDescriptionChange(const AbstractAspect *aspect)
{
    if (aspect == d_future_table) {
        setObjectName(d_future_table->name());
        updateCaption();
        return;
    }
    const Column *col = qobject_cast<const Column *>(aspect);
    if (!col || !d_future_table)
        return;
    const int colIdx = d_future_table->columnIndex(col);
    if (colIdx == -1)
        return;

    if (d_stored_column_labels.contains(col)) {
        const QString old_name = d_stored_column_labels.take(col);
        const QString new_name = col->name();
        if (old_name != new_name) {
            emit changedColHeader(name() + "_" + old_name, name() + "_" + new_name);

            const bool prevGuard = d_in_auto_recalc;
            d_in_auto_recalc = true;
            for (int i = 0; i < d_future_table->columnCount(); i++) {
                QList<Interval<int>> formula_intervals = column(i)->formulaIntervals();
                foreach (Interval<int> interval, formula_intervals) {
                    QString formula = column(i)->formula(interval.start());
                    if (formula.contains("\"" + old_name + "\"")) {
                        formula.replace("\"" + old_name + "\"", "\"" + new_name + "\"");
                        column(i)->setFormula(interval, formula);
                    }
                }
            }
            d_in_auto_recalc = prevGuard;
        }
    }

    if (d_auto_recalc_formulas && !d_in_auto_recalc && !col->formulaIntervals().isEmpty()) {
        d_in_auto_recalc = true;
        recalculateColumnInternal(colIdx, false, true);
        d_in_auto_recalc = false;
    }
}

// this function is for backwards compatibility (used by Python),
void Table::importASCII(const QString &fname, const QString &sep, int ignoredLines, bool renameCols,
                        bool stripSpaces, bool simplifySpaces, bool newTable)
{
    Q_UNUSED(newTable)

    AsciiTableImportFilter filter;
    filter.set_ignored_lines(ignoredLines);
    filter.set_separator(sep);
    filter.set_first_row_names_columns(renameCols);
    filter.set_trim_whitespace(stripSpaces);
    filter.set_simplify_whitespace(simplifySpaces);

    QFile file(fname);
    if (file.open(QIODevice::ReadOnly)) {
        future::Table *temp = static_cast<future::Table *>(filter.importAspect(file));
        if (!temp)
            return;
        int preexisting_cols = columnCount();
        int overwritten_cols = qMin(temp->columnCount(), preexisting_cols);
        for (int i = 0; i < overwritten_cols; i++) {
            column(i)->asStringColumn()->copy(temp->column(i));
            if (renameCols)
                column(i)->setName(temp->column(i)->name());
        }
        for (int i = overwritten_cols; i < preexisting_cols; i++)
            column(overwritten_cols)->remove();
        String2DoubleFilter *filter = new String2DoubleFilter;
        for (int i = overwritten_cols; i < temp->columnCount(); i++) {
            filter->input(0, temp->column(i));
            Column *new_col = new Column(temp->column(i)->name(), SciDAVis::ColumnMode::Numeric);
            new_col->setPlotDesignation(SciDAVis::Y);
            new_col->copy(filter->output(0));
            d_future_table->addChild(new_col);
        }
        delete filter;
        delete temp;
        setWindowLabel(fname);
    }
}
