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
#include "ApplicationWindow.h"
#include "MultiLayer.h"
#include "Graph.h"
#include "PlotCurve.h"
#include "core/Project.h"
#include "core/column/Column.h"
#include "lib/Interval.h"
#include "table/TableModel.h"
#include "core/datatypes/Double2StringFilter.h"
#include "core/datatypes/String2DoubleFilter.h"
#include "core/datatypes/DateTime2StringFilter.h"
#include "table/AsciiTableImportFilter.h"
#include "table/TableDoubleHeaderView.h"
#include "XlsxImport.h"
#include "ScriptEdit.h"

#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QAbstractItemView>
#include <QSet>
#include <QPair>
#include <algorithm>
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
#include <QSaveFile>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <QToolButton>
#include <QTextCursor>
#include <QUndoStack>
#include <QRegularExpression>
#include "Script.h"
#include "I18nJapanese.h"
#include <mutex>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <memory>
#include <vector>
#include <cmath>
#include <iostream>
using namespace std;

namespace {
QString stripLeadingFormulaEquals(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.startsWith(QLatin1Char('=')))
        s = s.mid(1).trimmed();
    return s;
}

bool isGroupedNumberTokenGlobal(const QString &tok)
{
    static const QRegularExpression kGroupedNumberRx(
            QStringLiteral("^[+-]?\\d{1,3}(?:,\\d{3})+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?$"));
    return kGroupedNumberRx.match(tok).hasMatch();
}

bool parseTableDoubleToken(const QString &s, double &outVal)
{
    bool ok = false;
    if (QLocale().decimalPoint() != QLatin1Char(',') && isGroupedNumberTokenGlobal(s)) {
        QString noGroup = s;
        noGroup.remove(QLatin1Char(','));
        outVal = QLocale::c().toDouble(noGroup, &ok);
        if (ok)
            return true;
    }
    outVal = QLocale::c().toDouble(s, &ok);
    if (ok)
        return true;
    outVal = QLocale().toDouble(s, &ok);
    if (ok)
        return true;
    QString alt = s;
    alt.replace(QLatin1Char(','), QLatin1Char('.'));
    outVal = QLocale::c().toDouble(alt, &ok);
    return ok;
}

void registerTableWorksheetTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        // paste (D2)
        reg("%n pasted cell(s) were not numbers and were left empty.",
            "数値でない %n 個のセルは空欄にしました。");
        // fill down / find & replace (D3)
        reg("%1: fill down", "%1: 下方向へコピー");
        reg("%1: replace", "%1: 置換");
        reg("Find and Replace in Worksheet", "ワークシートの検索と置換");
        reg("Find in Worksheet", "ワークシートの検索");
        reg("Find:", "検索する文字列:");
        reg("Replace with:", "置換後の文字列:");
        reg("Match case", "大文字と小文字を区別する");
        reg("Whole cell", "セル全体が一致するものだけ");
        reg("Selected cells only", "選択したセルだけ");
        reg("Find Next", "次を検索");
        reg("Replace", "置換");
        reg("Replace All", "すべて置換");
        reg("Not found.", "見つかりませんでした。");
        reg("%n cell(s) replaced.", "%n 個のセルを置換しました。");
        reg("Convert Numeric Column to Text?", "数値列をテキスト列へ変換しますか?");
        reg("The replacement text is not numeric for %n column(s).\n"
            "Convert those column(s) to Text so the replaced text is preserved?",
            "%n 列で置換後の文字列が数値ではありません。\n"
            "文字列を保持するため、該当列をテキスト列へ変換しますか?");
        reg("Convert to Text", "テキスト列へ変換");
        // column formulas (D5)
        reg("Formula error: %1", "数式のエラー: %1");
        reg("Column %1: %2", "列 %1: %2");
        reg("Column formula error in %1: %2", "列 %1 の数式エラー: %2");
        reg("%1: modify column properties", "%1: 列プロパティを変更");
        reg("The formula could not be evaluated.", "数式を計算できませんでした。");
        reg("Circular reference: the formula depends on its own result. "
            "The values were not updated.",
            "循環参照: 数式が自分自身の結果を参照しています。値は更新していません。");
        reg("Not updated: the formula uses a column with a circular reference.",
            "未更新: 循環参照のある列を数式が参照しています。");
        reg("Circular reference between worksheets: the values did not settle and may be "
            "out of date.",
            "ワークシート間の循環参照: 値が定まらず、古い値の可能性があります。");
        // export / reimport (D4/D6)
        reg("Could not open the file: %1\n%2", "ファイルを開けませんでした: %1\n%2");
        reg("Could not parse data from file: %1", "ファイルからデータを読み取れませんでした: %1");
        reg("Import failed", "読み込みに失敗しました");
        reg("%1: re-import from source", "%1: 元ファイルから再読込");
        reg("No original source file is recorded for this worksheet.",
            "このワークシートには元のファイルパスが記録されていません。");
        reg("Source file not found:\n%1", "元のファイルが見つかりません:\n%1");
        reg("Sheet '%1' was not found in '%2'.", "シート '%1' が '%2' 内に見つかりませんでした。");
        reg("Re-imported from %1", "元ファイルから再読込しました: %1");
        reg("No cells are selected. Select the cells to export first.",
            "選択されたセルがありません。書き出すセルを先に選択してください。");
        reg("Exported %1", "書き出しました: %1");
        reg("Export stopped at table %1: %2", "表 %1 で書き出しを中止しました: %2");
        reg("Please verify that you have the right to write to this location!",
            "この場所に書き込む権限があるか確認してください。");
        reg("Writing failed (%1). The disk may be full or the file may be in use.",
            "書き込みに失敗しました(%1)。ディスクの空きが無いか、ファイルが他のアプリで開かれている可能性があります。");
    });
}

int computeImportedColumnSpan(future::Table *ft)
{
    if (!ft)
        return 0;
    const int totalCols = ft->columnCount();
    int span = ft->importSourceColumnCount();
    if (span < 0)
        span = totalCols;
    span = qBound(0, span, totalCols);
    // Preserve trailing formula/derived columns even when import_source_columns is unset or stale.
    while (span > 0) {
        Column *c = ft->column(span - 1);
        if (!c || c->formulaIntervals().isEmpty())
            break;
        --span;
    }
    return span;
}

QVector<int> collectPreservedFormulaCols(future::Table *ft, int startCol, int prevRows, int newRows)
{
    QVector<int> derivedCols;
    if (!ft)
        return derivedCols;
    for (int c = qMax(0, startCol); c < ft->columnCount(); ++c) {
        Column *uc = ft->column(c);
        if (!uc)
            continue;
        const QList<Interval<int>> fxIntervals = uc->formulaIntervals();
        if (fxIntervals.isEmpty())
            continue;
        if (fxIntervals.size() == 1 && fxIntervals.at(0).start() == 0
            && (prevRows <= 0 || fxIntervals.at(0).end() >= prevRows - 1) && newRows > 0
            && fxIntervals.at(0).end() != newRows - 1) {
            uc->setFormula(Interval<int>(0, newRows - 1), uc->formula(0));
        }
        derivedCols.append(c);
    }
    return derivedCols;
}
} // namespace


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
        if (!d_future_table || d_future_table->columnCount() == 0) {
            delete d_future_table;
            d_future_table = nullptr;
            d_import_failed = true;
            d_last_import_error = tr("Could not parse data from file: %1")
                                          .arg(QDir::toNativeSeparators(fname));
            const QString refusal = AsciiTableImportFilter::lastImportReport().refusal;
            if (!refusal.isEmpty())
                d_last_import_error += QStringLiteral("\n") + refusal;
            qWarning("%s", qPrintable(d_last_import_error));
            if (!ApplicationWindow::isNonInteractiveContext(parent))
                QMessageBox::warning(parent, tr("Import failed"), d_last_import_error);
        } else {
            d_future_table->setName(label);
            d_import_failed = false;
        }
    } else {
        d_future_table = nullptr;
        d_import_failed = true;
        const QString msg = tr("Could not open the file: %1\n%2")
                                    .arg(QDir::toNativeSeparators(fname), file.errorString());
        d_last_import_error = msg;
        qWarning("%s", qPrintable(msg));
        if (!ApplicationWindow::isNonInteractiveContext(parent))
            QMessageBox::warning(parent, tr("Import failed"), msg);
    }
    setWindowLabel(fname);
    if (d_future_table && !fname.isEmpty()) {
        d_future_table->setImportSource(fname, QString(), sep,
                                        AsciiTableImportFilter::dialogDefaultEncoding(),
                                        ignoredLines,
                                        AsciiTableImportFilter::dialogDefaultUnitsRow(),
                                        d_future_table->columnCount(),
                                        future::Table::numericLocaleNameForImport(numericLocale));
    }
    init();
}

Table::Table(ScriptingEnv *env, int r, int c, const QString &label, QWidget *parent,
             const char *name, Qt::WindowFlags f)
    : TableView(label, parent, name, f), scripted(env)
{
    d_future_table = new future::Table(r, c, label);
    if (d_future_table) {
        for (int i = 0; i < d_future_table->columnCount(); ++i) {
            Column *colPtr = d_future_table->column(i);
            if (colPtr && colPtr->columnMode() == SciDAVis::ColumnMode::Numeric) {
                if (auto *flt = dynamic_cast<Double2StringFilter *>(colPtr->outputFilter())) {
                    if (flt->numericFormat() == 'f' && flt->numDigits() == 14) {
                        flt->setNumericFormat('g');
                        flt->setNumDigits(6);
                    }
                }
            }
        }
    }
    init();
}

namespace {
static int s_tableUndoRedoReplayDepth = 0;

static bool isUndoStackInMacro(const QUndoStack *us)
{
    return us && us->count() > 0 && !us->canUndo() && !us->canRedo();
}
} // namespace

Table::~Table()
{
    if (d_future_table && !d_future_table->parentAspect() && d_future_table->view() == this) {
        future::Table *ft = d_future_table.data();
        d_future_table.clear();
        ft->setView(nullptr);
        if (!ft->property("_tg_owned_by_undo_cmd").toBool())
            delete ft;
    }
}

void Table::beginUndoRedoReplay()
{
    ++s_tableUndoRedoReplayDepth;
}

void Table::endUndoRedoReplay()
{
    if (s_tableUndoRedoReplayDepth > 0)
        --s_tableUndoRedoReplayDepth;
}

bool Table::isUndoRedoReplaying()
{
    return s_tableUndoRedoReplayDepth > 0;
}

void Table::init()
{
    if (d_future_table) {
        TableView::setTable(d_future_table);
        birthdate = QLocale().toString(d_future_table->creationTime());
    } else // the rest is meaningless
        return;

    setMinimumSize(400, 300);
    resize(680, 500);
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
    connect(d_future_table, SIGNAL(autoRecalcFormulasChanged(bool)), this,
            SLOT(syncAutoRecalcFromFuture(bool)));
    d_auto_recalc_formulas = d_future_table->autoRecalcFormulas();

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
    if (d_auto_recalc_formulas && !d_in_auto_recalc && !isUndoRedoReplaying())
        scheduleOrRunDependentRecalc(first, count);
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
    if (d_auto_recalc_formulas && !d_in_auto_recalc && !isUndoRedoReplaying())
        scheduleOrRunDependentRecalc(-1, 0);
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
    if (!d_future_table)
        return;
    d_future_table->beginMacro(tr("%1: apply formula to column").arg(name()));
    const bool prevGuard = d_in_auto_recalc;
    d_in_auto_recalc = true;
    for (int i = 0; i < (int)com.size() && i < numCols(); i++) {
        const QString trimmed = stripLeadingFormulaEquals(com.at(i));
        if (trimmed.isEmpty())
            column(i)->clearFormulas();
        else
            column(i)->setFormula(Interval<int>(0, qMax(0, numRows() - 1)), trimmed);
    }
    d_in_auto_recalc = prevGuard;
    if (d_auto_recalc_formulas && !d_in_auto_recalc && !isUndoRedoReplaying()) {
        autoRecalculateDependentColumns(-1, 0);
        reportFormulaErrorsToUser();
    }
    d_future_table->endMacro();
    if (d_future_table->columnCount() > 0)
        emit d_future_table->headerDataChanged(Qt::Horizontal, 0,
                                               d_future_table->columnCount() - 1);
}

void Table::setCommand(int col, const QString &com)
{
    Column *c = column(col);
    if (!c || !d_future_table)
        return;
    d_future_table->beginMacro(tr("%1: apply formula to column").arg(name()));
    const QString trimmed = stripLeadingFormulaEquals(com);
    const bool prevGuard = d_in_auto_recalc;
    d_in_auto_recalc = true;
    if (trimmed.isEmpty())
        c->clearFormulas();
    else
        c->setFormula(Interval<int>(0, qMax(0, numRows() - 1)), trimmed);
    d_in_auto_recalc = prevGuard;
    if (!trimmed.isEmpty() && d_auto_recalc_formulas && !d_in_auto_recalc && !isUndoRedoReplaying()) {
        evaluateCrossTableDag({ col }, false);
        autoRecalculateDependentColumns(col, 1);
        reportFormulaErrorsToUser(col);
    } else if (trimmed.isEmpty()) {
        setColumnFormulaError(col, QString());
        if (d_auto_recalc_formulas && !d_in_auto_recalc && !isUndoRedoReplaying()) {
            autoRecalculateDependentColumns(col, 1);
            reportFormulaErrorsToUser(col);
        }
    }
    d_future_table->endMacro();
    emit d_future_table->headerDataChanged(Qt::Horizontal, col, col);
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
    QVector<int> targetCols;
    for (int col = firstCol; col <= lastCol; ++col) {
        Column *c = column(col);
        if (c && !c->formulaIntervals().isEmpty())
            targetCols.push_back(col);
    }
    if (targetCols.isEmpty())
        return true;

    const bool partialRows = (selectedRowCount(false) >= 2 && selectedRowCount(false) < numRows());
    if (partialRows) {
        for (int col : targetCols) {
            if (!recalculate(col, true))
                return false;
        }
        return true;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    evaluateCrossTableDag(targetCols, false);
    QApplication::restoreOverrideCursor();
    bool allOk = true;
    int firstErrCol = -1;
    for (int col : targetCols) {
        if (Column *c = column(col)) {
            if (!c->formulaError().isEmpty()) {
                allOk = false;
                if (firstErrCol < 0)
                    firstErrCol = col;
            }
        }
    }
    if (!allOk)
        reportFormulaErrorsToUser(firstErrCol);
    return allOk;
}

bool Table::recalculate(int col, bool only_selected_rows)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = recalculateColumnInternal(col, only_selected_rows, false);
    QApplication::restoreOverrideCursor();
    return ok;
}

bool Table::recalculateColumnInternal(int col, bool only_selected_rows, bool silentErrors,
                                      QString *errorOut)
{
    Column *col_ptr = column(col);
    if (!col_ptr || !scriptEnv)
        return false;
    const bool ok = [&]() -> bool {

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
        if (errorOut) {
            connect(colscript, &Script::error,
                    [errorOut](const QString &message, const QString &, int) {
                        if (errorOut->isEmpty())
                            *errorOut = message;
                    });
        }
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
            col_ptr->setFormula(Interval<int>(0, end_row), formula);
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
    }();
    if (!ok && errorOut && errorOut->isEmpty())
        *errorOut = tr("The formula could not be evaluated.");
    // a successful manual or automatic recalculation clears the error mark of the column
    if (ok && !only_selected_rows && !col_ptr->formulaError().isEmpty())
        setColumnFormulaError(col, QString());
    return ok;
}

namespace {
static ApplicationWindow *owningApplicationWindowForTable(const Table *tbl)
{
    const QObject *cur = tbl;
    while (cur) {
        if (const ApplicationWindow *aw = qobject_cast<const ApplicationWindow *>(cur))
            return const_cast<ApplicationWindow *>(aw);
        if (const QWidget *w = qobject_cast<const QWidget *>(cur)) {
            if (ApplicationWindow *winAw = qobject_cast<ApplicationWindow *>(w->window()))
                return winAw;
        }
        cur = cur->parent();
    }
    for (QWidget *tlw : QApplication::topLevelWidgets()) {
        if (ApplicationWindow *aw = qobject_cast<ApplicationWindow *>(tlw)) {
            if (aw->windowsList().contains(const_cast<Table *>(tbl)))
                return aw;
        }
    }
    return nullptr;
}

static QVector<int> collectFormulaColumnIndices(Table *tbl, int skipFirstCol, int skipCount,
                                                bool *outHasCrossTableRef = nullptr)
{
    if (outHasCrossTableRef)
        *outHasCrossTableRef = false;
    QVector<int> cols;
    if (!tbl || !tbl->d_future_table)
        return cols;
    const int nCols = tbl->numCols();
    for (int c = 0; c < nCols; ++c) {
        if (skipCount > 0 && c >= skipFirstCol && c < skipFirstCol + skipCount)
            continue;
        Column *cp = tbl->column(c);
        if (!cp || cp->formulaIntervals().isEmpty())
            continue;
        bool colHasFormula = false;
        const QList<Interval<int>> ivs = cp->formulaIntervals();
        for (const Interval<int> &iv : ivs) {
            const QString f = cp->formula(iv.start()).trimmed();
            if (!f.isEmpty()) {
                colHasFormula = true;
                if (outHasCrossTableRef
                    && (f.contains(QLatin1Char('/'))
                        || f.contains(QLatin1String("tablecol"), Qt::CaseInsensitive)
                        || f.contains(QLatin1String("column__"), Qt::CaseInsensitive))) {
                    *outHasCrossTableRef = true;
                }
            }
        }
        if (colHasFormula)
            cols.push_back(c);
    }
    return cols;
}

static bool s_inCrossTableAutoRecalc = false;

struct ScopedCrossTableRecalcGuard
{
    ScopedCrossTableRecalcGuard() { s_inCrossTableAutoRecalc = true; }
    ~ScopedCrossTableRecalcGuard() { s_inCrossTableAutoRecalc = false; }
};
} // namespace

namespace {
// Same-table column references of a formula: col("B"), col(B), col("B", i), cell("B", i),
// column("B"), col(2), column_(2), cell_(2, i). `selfRowRef` lists references that carry a row
// argument (cell(...) / col(name, row)): a column reading other rows of itself is a recursive
// series, not a circular reference. Returns false if some reference could not be resolved
// (e.g. col(j-1)); the caller then falls back to repeated passes.
static bool collectSameTableRefs(Table *tbl, int ownCol, const QString &formula,
                                 QSet<int> &refs, bool &selfWithoutRow)
{
    static const QRegularExpression kRef(QStringLiteral(
            "(?<![A-Za-z0-9_])(col|column|column_|cell|cell_)\\s*\\(\\s*(\"((?:[^\"\\\\]|\\\\.)*)\"|[^,()]*)\\s*(,|\\))"));
    bool allResolved = true;
    selfWithoutRow = false;
    QRegularExpressionMatchIterator it = kRef.globalMatch(formula);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString fn = m.captured(1);
        const bool hasRowArg = (m.captured(4) == QLatin1String(","));
        const bool rowForm = fn.startsWith(QLatin1String("cell")) || hasRowArg;
        int idx = -1;
        if (m.capturedStart(3) >= 0) {
            QString name = m.captured(3);
            name.replace(QLatin1String("\\/"), QLatin1String("/"));
            name.replace(QLatin1String("\\\\"), QLatin1String("\\"));
            if (name.contains(QLatin1Char('/'))) {
                const QString tablePart = name.section(QLatin1Char('/'), 0, 0);
                if (tablePart != tbl->name())
                    continue; // another table: handled by the cross-table passes
                name = name.section(QLatin1Char('/'), 1);
            }
            idx = tbl->colIndex(name);
        } else {
            const QString arg = m.captured(2).trimmed();
            bool isNum = false;
            const int n = arg.toInt(&isNum);
            if (isNum)
                idx = n - 1; // 1-based column number
            else if (tbl->colIndex(arg) >= 0)
                idx = tbl->colIndex(arg);
        }
        if (idx < 0 || idx >= tbl->numCols()) {
            allResolved = false;
            continue;
        }
        if (idx == ownCol) {
            if (!rowForm)
                selfWithoutRow = true;
            continue;
        }
        refs.insert(idx);
    }
    return allResolved;
}
static void collectCrossTableRefs(Table *ownerTbl, int ownCol, const QString &formula,
                                  const QHash<QString, Table *> &tablesByName,
                                  QSet<QPair<Table *, int>> &refs, bool &selfWithoutRow,
                                  bool &allResolved)
{
    static const QRegularExpression kColRef(QStringLiteral(
            "(?<![A-Za-z0-9_])(col|column|column_|cell|cell_)\\s*\\(\\s*(\"((?:[^\"\\\\]|\\\\.)*)\"|[^,()]*)\\s*(,|\\))"));
    static const QRegularExpression kTableColRef(QStringLiteral(
            "(?<![A-Za-z0-9_])(tablecol|column__)\\s*\\(\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*,\\s*(\"((?:[^\"\\\\]|\\\\.)*)\"|[^,()]*)\\s*(,|\\))"));

    selfWithoutRow = false;
    QRegularExpressionMatchIterator it = kColRef.globalMatch(formula);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString fn = m.captured(1);
        const bool hasRowArg = (m.captured(4) == QLatin1String(","));
        const bool rowForm = fn.startsWith(QLatin1String("cell")) || hasRowArg;
        Table *targetTbl = ownerTbl;
        int idx = -1;
        if (m.capturedStart(3) >= 0) {
            QString name = m.captured(3);
            name.replace(QLatin1String("\\/"), QLatin1String("/"));
            name.replace(QLatin1String("\\\\"), QLatin1String("\\"));
            if (name.contains(QLatin1Char('/'))) {
                const QString tablePart = name.section(QLatin1Char('/'), 0, 0);
                targetTbl = tablesByName.value(tablePart, nullptr);
                if (!targetTbl) {
                    allResolved = false;
                    continue;
                }
                name = name.section(QLatin1Char('/'), 1);
            }
            idx = targetTbl->colIndex(name);
        } else {
            const QString arg = m.captured(2).trimmed();
            bool isNum = false;
            const int n = arg.toInt(&isNum);
            if (isNum)
                idx = n - 1;
            else if (ownerTbl->colIndex(arg) >= 0)
                idx = ownerTbl->colIndex(arg);
        }
        if (!targetTbl || idx < 0 || idx >= targetTbl->numCols()) {
            allResolved = false;
            continue;
        }
        if (targetTbl == ownerTbl && idx == ownCol) {
            if (!rowForm)
                selfWithoutRow = true;
            continue;
        }
        refs.insert(qMakePair(targetTbl, idx));
    }

    QRegularExpressionMatchIterator itTbl = kTableColRef.globalMatch(formula);
    while (itTbl.hasNext()) {
        const QRegularExpressionMatch m = itTbl.next();
        QString tblName = m.captured(2);
        tblName.replace(QLatin1String("\\\\"), QLatin1String("\\"));
        Table *targetTbl = tablesByName.value(tblName, nullptr);
        if (!targetTbl) {
            allResolved = false;
            continue;
        }
        int idx = -1;
        if (m.capturedStart(4) >= 0) {
            QString colNameStr = m.captured(4);
            colNameStr.replace(QLatin1String("\\\\"), QLatin1String("\\"));
            idx = targetTbl->colIndex(colNameStr);
        } else {
            const QString arg = m.captured(3).trimmed();
            bool isNum = false;
            const int n = arg.toInt(&isNum);
            if (isNum)
                idx = n - 1;
            else if (targetTbl->colIndex(arg) >= 0)
                idx = targetTbl->colIndex(arg);
        }
        if (idx < 0 || idx >= targetTbl->numCols()) {
            allResolved = false;
            continue;
        }
        if (targetTbl == ownerTbl && idx == ownCol) {
            selfWithoutRow = true;
            continue;
        }
        refs.insert(qMakePair(targetTbl, idx));
    }
}
} // namespace

void Table::setColumnFormulaError(int col, const QString &message)
{
    Column *c = column(col);
    if (!c || c->formulaError() == message)
        return;
    c->setFormulaError(message);
    if (d_future_table)
        emit d_future_table->headerDataChanged(Qt::Horizontal, col, col);
}

QList<int> Table::formulaErrorColumns() const
{
    QList<int> out;
    if (!d_future_table)
        return out;
    for (int c = 0; c < d_future_table->columnCount(); ++c) {
        if (Column *cp = d_future_table->column(c)) {
            if (!cp->formulaError().isEmpty())
                out << c;
        }
    }
    return out;
}

void Table::reportFormulaErrorsToUser(int focusCol) const
{
    if (!d_future_table)
        return;
    registerTableWorksheetTranslations();
    ApplicationWindow *aw = owningApplicationWindowForTable(this);
    if (!aw || !aw->statusBar())
        return;
    int errCol = -1;
    if (focusCol >= 0 && focusCol < d_future_table->columnCount()) {
        if (Column *cp = d_future_table->column(focusCol)) {
            if (!cp->formulaError().isEmpty())
                errCol = focusCol;
        }
    }
    if (errCol < 0) {
        const QList<int> errs = formulaErrorColumns();
        if (!errs.isEmpty())
            errCol = errs.first();
    }
    if (errCol >= 0) {
        if (Column *cp = d_future_table->column(errCol)) {
            aw->statusBar()->showMessage(
                    tr("Column formula error in %1: %2").arg(cp->name(), cp->formulaError()),
                    8000);
        }
    }
}

bool Table::evaluateCrossTableDag(const QVector<int> &localEvalCols, bool includeProjectTables)
{
    registerTableWorksheetTranslations();
    ApplicationWindow *aw = owningApplicationWindowForTable(this);
    QList<Table *> allTables;
    allTables.append(this);
    if (aw) {
        for (MyWidget *mw : aw->windowsList()) {
            if (!mw || mw == this || !mw->inherits("Table"))
                continue;
            Table *other = static_cast<Table *>(mw);
            if (!other->d_future_table || !other->scriptEnv)
                continue;
            allTables.append(other);
        }
    }

    QHash<QString, Table *> tablesByName;
    for (Table *t : allTables)
        tablesByName.insert(t->name(), t);

    struct DagNode
    {
        Table *tbl = nullptr;
        int col = -1;
        bool evaluate = false;
        bool markErrors = false;
        bool crossRef = false;
        bool unresolved = false;
        bool selfCircular = false;
    };

    QVector<DagNode> nodes;
    QHash<Table *, QHash<int, int>> nodeIndex;
    const QSet<int> localEvalSet(localEvalCols.begin(), localEvalCols.end());

    for (Table *t : allTables) {
        const bool isRoot = (t == this);
        if (!isRoot && !t->autoRecalcFormulas() && includeProjectTables) {
            // Even when another table has auto-recalc disabled, include its formula nodes as
            // edges for cycle detection while leaving its evaluate flag false.
        }
        bool hasCross = false;
        const QVector<int> cols = collectFormulaColumnIndices(t, -1, 0, &hasCross);
        if (cols.isEmpty())
            continue;
        for (int c : cols) {
            DagNode n;
            n.tbl = t;
            n.col = c;
            if (isRoot) {
                n.evaluate = localEvalSet.contains(c);
                n.markErrors = includeProjectTables || localEvalSet.contains(c);
            } else {
                n.evaluate = includeProjectTables && t->autoRecalcFormulas() && !t->d_in_auto_recalc
                        && hasCross;
                n.markErrors = n.evaluate;
            }
            nodeIndex[t][c] = nodes.size();
            nodes.push_back(n);
        }
    }

    if (nodes.isEmpty())
        return false;

    QVector<QSet<int>> deps(nodes.size());
    QVector<QList<int>> users(nodes.size());
    for (int i = 0; i < nodes.size(); ++i) {
        DagNode &n = nodes[i];
        Column *cp = n.tbl->column(n.col);
        if (!cp)
            continue;
        QSet<QPair<Table *, int>> rawRefs;
        bool resolved = true;
        for (const Interval<int> &iv : cp->formulaIntervals()) {
            const QString f = cp->formula(iv.start());
            if (f.contains(QLatin1Char('/'))
                || f.contains(QLatin1String("tablecol"), Qt::CaseInsensitive)
                || f.contains(QLatin1String("column__"), Qt::CaseInsensitive)) {
                n.crossRef = true;
            }
            bool selfNoRow = false;
            collectCrossTableRefs(n.tbl, n.col, f, tablesByName, rawRefs, selfNoRow, resolved);
            if (selfNoRow)
                n.selfCircular = true;
        }
        n.unresolved = !resolved;
        for (const auto &ref : rawRefs) {
            const int depIdx = nodeIndex.value(ref.first).value(ref.second, -1);
            if (depIdx >= 0 && !deps[i].contains(depIdx)) {
                deps[i].insert(depIdx);
                users[depIdx].append(i);
            }
        }
    }

    QVector<int> indeg(nodes.size(), 0);
    for (int i = 0; i < nodes.size(); ++i)
        indeg[i] = deps[i].size();

    QList<int> ready;
    QVector<int> order;
    for (int i = 0; i < nodes.size(); ++i) {
        if (indeg[i] == 0 && !nodes[i].selfCircular)
            ready.append(i);
    }
    while (!ready.isEmpty()) {
        const int cur = ready.takeFirst();
        order.push_back(cur);
        for (int u : users[cur]) {
            if (--indeg[u] == 0 && !nodes[u].selfCircular)
                ready.append(u);
        }
    }

    QVector<bool> inOrder(nodes.size(), false);
    for (int idx : order)
        inOrder[idx] = true;

    auto reachesItself = [&](int start) -> bool {
        if (nodes[start].selfCircular)
            return true;
        QSet<int> seen;
        QList<int> stack = deps[start].values();
        while (!stack.isEmpty()) {
            const int x = stack.takeLast();
            if (x == start)
                return true;
            if (seen.contains(x))
                continue;
            seen.insert(x);
            stack << deps[x].values();
        }
        return false;
    };

    QHash<Table *, bool> prevGuards;
    QList<QMetaObject::Connection> conns;
    bool anyChanged = false;
    for (Table *t : allTables) {
        prevGuards.insert(t, t->d_in_auto_recalc);
        t->d_in_auto_recalc = true;
        if (t->d_future_table) {
            conns.append(QObject::connect(
                    t->d_future_table, &future::Table::dataChanged,
                    [&anyChanged](int, int, int, int) { anyChanged = true; }));
        }
    }

    bool anyUnresolvedEval = false;
    auto evalPass = [&](bool onlyUnresolved) -> bool {
        bool passChanged = false;
        for (int idx : order) {
            const DagNode &n = nodes[idx];
            if (!n.evaluate)
                continue;
            if (onlyUnresolved && !n.unresolved)
                continue;
            if (n.unresolved)
                anyUnresolvedEval = true;
            bool colChanged = false;
            QMetaObject::Connection cc;
            if (n.tbl->d_future_table) {
                cc = QObject::connect(n.tbl->d_future_table, &future::Table::dataChanged,
                                      [&colChanged](int, int, int, int) { colChanged = true; });
            }
            QString err;
            if (n.tbl->recalculateColumnInternal(n.col, false, true, &err))
                n.tbl->setColumnFormulaError(n.col, QString());
            else
                n.tbl->setColumnFormulaError(n.col, tr("Formula error: %1").arg(err));
            if (cc)
                QObject::disconnect(cc);
            if (colChanged) {
                passChanged = true;
                anyChanged = true;
            }
        }
        return passChanged;
    };

    evalPass(false);
    if (anyUnresolvedEval) {
        const int maxPasses = qBound(1, order.size(), 16);
        for (int pass = 1; pass < maxPasses; ++pass) {
            if (!evalPass(true))
                break;
        }
    }

    for (const QMetaObject::Connection &c : conns)
        QObject::disconnect(c);
    for (auto it = prevGuards.cbegin(); it != prevGuards.cend(); ++it)
        it.key()->d_in_auto_recalc = it.value();

    for (int i = 0; i < nodes.size(); ++i) {
        if (inOrder[i])
            continue;
        const DagNode &n = nodes[i];
        if (!n.markErrors && !n.evaluate)
            continue;
        if (reachesItself(i)) {
            if (n.crossRef) {
                n.tbl->setColumnFormulaError(
                        n.col,
                        tr("Circular reference between worksheets: the values did not "
                           "settle and may be out of date."));
            } else {
                n.tbl->setColumnFormulaError(
                        n.col,
                        tr("Circular reference: the formula depends on its own result. "
                           "The values were not updated."));
            }
        } else {
            n.tbl->setColumnFormulaError(
                    n.col,
                    tr("Not updated: the formula uses a column with a circular reference."));
        }
    }
    return anyChanged;
}

bool Table::recalculateFormulaColumnsOrdered(const QVector<int> &fCols)
{
    registerTableWorksheetTranslations();
    // 1) dependency graph between all formula columns of this table (cycles through a column
    //    that was just edited must be found too); only `fCols` are evaluated
    const QVector<int> allCols = collectFormulaColumnIndices(this, -1, 0);
    const QSet<int> fSet(allCols.begin(), allCols.end());
    const QSet<int> evalSet(fCols.begin(), fCols.end());
    QHash<int, QSet<int>> deps; // column -> formula columns it reads
    QSet<int> selfCircular;
    bool allResolved = true;
    for (int c : allCols) {
        Column *cp = column(c);
        QSet<int> refs;
        for (const Interval<int> &iv : cp->formulaIntervals()) {
            bool selfNoRow = false;
            if (!collectSameTableRefs(this, c, cp->formula(iv.start()), refs, selfNoRow))
                allResolved = false;
            if (selfNoRow)
                selfCircular.insert(c);
        }
        deps[c] = refs & fSet;
    }

    // 2) topological order (Kahn); what is left over sits on or behind a cycle
    QHash<int, int> indeg;
    QHash<int, QList<int>> users;
    for (int c : allCols) {
        indeg[c] = deps[c].size();
        for (int d : deps[c])
            users[d] << c;
    }
    QList<int> ready, order;
    for (int c : allCols) {
        if (indeg[c] == 0 && !selfCircular.contains(c))
            ready << c;
    }
    while (!ready.isEmpty()) {
        const int c = ready.takeFirst();
        order << c;
        for (int u : users.value(c)) {
            if (--indeg[u] == 0 && !selfCircular.contains(u))
                ready << u;
        }
    }
    QSet<int> blocked;
    for (int c : allCols) {
        if (!order.contains(c))
            blocked.insert(c);
    }
    // a blocked column is circular if it can reach itself; otherwise it only depends on one
    auto reachesItself = [&](int start) -> bool {
        if (selfCircular.contains(start))
            return true;
        QSet<int> seen;
        QList<int> stack = deps.value(start).values();
        while (!stack.isEmpty()) {
            const int x = stack.takeLast();
            if (x == start)
                return true;
            if (seen.contains(x))
                continue;
            seen.insert(x);
            stack << deps.value(x).values();
        }
        return false;
    };

    bool anyChanged = false;
    QMetaObject::Connection conn = QObject::connect(
            d_future_table, &future::Table::dataChanged,
            [&anyChanged](int, int, int, int) { anyChanged = true; });

    auto evalOnce = [&]() {
        for (int c : order) {
            if (!evalSet.contains(c))
                continue;
            QString err;
            if (recalculateColumnInternal(c, false, true, &err))
                setColumnFormulaError(c, QString());
            else
                setColumnFormulaError(c, tr("Formula error: %1").arg(err));
        }
    };
    evalOnce();
    if (!allResolved) {
        // references we could not resolve statically (col(j-1), ...): repeat until stable
        for (int pass = 1; pass < allCols.size(); ++pass) {
            bool passChanged = false;
            QMetaObject::Connection pc = QObject::connect(
                    d_future_table, &future::Table::dataChanged,
                    [&passChanged](int, int, int, int) { passChanged = true; });
            evalOnce();
            QObject::disconnect(pc);
            if (!passChanged)
                break;
        }
    }
    QObject::disconnect(conn);

    for (int c : blocked) {
        if (reachesItself(c))
            setColumnFormulaError(
                    c, tr("Circular reference: the formula depends on its own result. "
                          "The values were not updated."));
        else
            setColumnFormulaError(
                    c, tr("Not updated: the formula uses a column with a circular reference."));
    }
    return anyChanged;
}

void Table::scheduleOrRunDependentRecalc(int changedFirstCol, int changedCount)
{
    if (!d_auto_recalc_formulas || d_in_auto_recalc || isUndoRedoReplaying() || !d_future_table
        || !scriptEnv) {
        return;
    }

    QUndoStack *us = d_future_table->undoStack();
    const Project *proj = d_future_table->project();
    if (!us || (proj && proj->isUndoSuppressed()) || isUndoStackInMacro(us)) {
        autoRecalculateDependentColumns(changedFirstCol, changedCount);
        return;
    }

    // Signal arrived while no macro was open on QUndoStack: we are currently inside
    // cmd->undo() (via us->undo()), cmd->redo() (via us->redo()), or cmd->redo() inside an
    // unwrapped us->push(newCmd). Defer inspection until QUndoStack::indexChanged fires at
    // the end of undo()/redo()/push() so undo()/redo() NEVER push a new QUndoCommand or wipe Redo!
    if (d_pending_unwrapped_recalc) {
        if (changedCount <= 0 || d_pending_recalc_col_count <= 0) {
            d_pending_recalc_first_col = -1;
            d_pending_recalc_col_count = 0;
        } else {
            const int lo = qMin(d_pending_recalc_first_col, changedFirstCol);
            const int hi = qMax(d_pending_recalc_first_col + d_pending_recalc_col_count,
                                changedFirstCol + changedCount);
            d_pending_recalc_first_col = lo;
            d_pending_recalc_col_count = hi - lo;
        }
        return;
    }

    // Quick check: if neither this table nor any cross-referenced table has formulas, skip connecting.
    bool anyFormula = !collectFormulaColumnIndices(this, changedFirstCol, changedCount).isEmpty();
    if (!anyFormula) {
        if (ApplicationWindow *aw = owningApplicationWindowForTable(this)) {
            for (MyWidget *mw : aw->windowsList()) {
                if (mw && mw != this && mw->inherits("Table")) {
                    bool hasCross = false;
                    if (!collectFormulaColumnIndices(static_cast<Table *>(mw), -1, 0, &hasCross).isEmpty()
                        && hasCross) {
                        anyFormula = true;
                        break;
                    }
                }
            }
        }
    }
    if (!anyFormula)
        return;

    d_pending_unwrapped_recalc = true;
    d_pending_recalc_first_col = changedFirstCol;
    d_pending_recalc_col_count = changedCount;

    const int preIdx = us->index();
    const int preCount = us->count();
    const QUndoCommand *preRedoCmd =
            (preIdx >= 0 && preIdx < preCount) ? us->command(preIdx) : nullptr;
    const QUndoCommand *preTopCmd =
            (preIdx > 0 && preIdx <= preCount) ? us->command(preIdx - 1) : nullptr;

    QPointer<Table> self(this);
    QPointer<QUndoStack> stackPtr(us);
    auto connHolder = std::make_shared<QMetaObject::Connection>();
    *connHolder = QObject::connect(
            us, &QUndoStack::indexChanged, this,
            [self, stackPtr, preIdx, preRedoCmd, preTopCmd, connHolder](int newIdx) {
                QObject::disconnect(*connHolder);
                if (!self)
                    return;
                const int firstCol = self->d_pending_recalc_first_col;
                const int colCnt = self->d_pending_recalc_col_count;
                self->d_pending_unwrapped_recalc = false;
                self->d_pending_recalc_first_col = -1;
                self->d_pending_recalc_col_count = 0;

                if (!stackPtr || Table::isUndoRedoReplaying() || !self->d_future_table)
                    return;

                // 1) Undo decreased index (note: when stack is at undoLimit, push keeps newIdx == preIdx
                //    while shifting command pointers so command(newIdx - 1) != preTopCmd).
                if (newIdx < preIdx)
                    return;
                if (newIdx == preIdx && newIdx > 0 && newIdx <= stackPtr->count()
                    && stackPtr->command(newIdx - 1) == preTopCmd) {
                    return;
                }

                // 2) Redo advanced over the exact pre-existing QUndoCommand* that was at preIdx.
                if (newIdx > preIdx && preRedoCmd != nullptr && preIdx < stackPtr->count()
                    && stackPtr->command(preIdx) == preRedoCmd) {
                    return;
                }

                // 3) Genuine unwrapped forward push(newCmd) completed: recalculate dependent formulas cleanly.
                self->d_future_table->beginMacro(
                        Table::tr("%1: apply formula to column").arg(self->name()));
                self->autoRecalculateDependentColumns(firstCol, colCnt);
                self->d_future_table->endMacro();
            });
}

void Table::autoRecalculateDependentColumns(int changedFirstCol, int changedCount)
{
    if (!d_auto_recalc_formulas || d_in_auto_recalc || isUndoRedoReplaying() || !d_future_table
        || !scriptEnv)
        return;

    const QVector<int> fCols = collectFormulaColumnIndices(this, changedFirstCol, changedCount);
    if (s_inCrossTableAutoRecalc) {
        if (!fCols.isEmpty()) {
            d_in_auto_recalc = true;
            recalculateFormulaColumnsOrdered(fCols);
            d_in_auto_recalc = false;
        }
        return;
    }

    bool anyOtherCross = false;
    if (ApplicationWindow *aw = owningApplicationWindowForTable(this)) {
        for (MyWidget *mw : aw->windowsList()) {
            if (!mw || mw == this || !mw->inherits("Table"))
                continue;
            Table *other = static_cast<Table *>(mw);
            if (!other->autoRecalcFormulas() || other->d_in_auto_recalc || !other->d_future_table
                || !other->scriptEnv)
                continue;
            bool hasCrossRef = false;
            const QVector<int> otherCols = collectFormulaColumnIndices(other, -1, 0, &hasCrossRef);
            if (!otherCols.isEmpty() && hasCrossRef) {
                anyOtherCross = true;
                break;
            }
        }
    }

    if (!anyOtherCross) {
        if (!fCols.isEmpty()) {
            d_in_auto_recalc = true;
            recalculateFormulaColumnsOrdered(fCols);
            d_in_auto_recalc = false;
            reportFormulaErrorsToUser();
        }
        return;
    }

    ScopedCrossTableRecalcGuard crossGuard;
    evaluateCrossTableDag(fCols, true);
    reportFormulaErrorsToUser();
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
        // The template format is one tab-separated line: a tab or line break inside a comment
        // would shift or cut off the following columns' comments (same policy as LongNames/Units).
        QString c = column(i)->comment();
        c.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
        s += c + "\t";
    }
    return s + "\n";
}

QString Table::saveToString(const QString &geometry)
{
    QString s = "<table>\n";
    QString xml;
    if (d_future_table) {
        d_future_table->setAutoRecalcFormulas(d_auto_recalc_formulas);
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
        d_future_table->setAutoRecalcFormulas(d_auto_recalc_formulas);
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
    if (Column *c = column(col))
        c->setComment(s);
}

void Table::setColName(int col, const QString &text)
{
    Column *c = column(col);
    if (!c)
        return;
    if (text.isEmpty() || c->name() == text) {
        c->setName(text);
        return;
    }
    // The macro lets handleAspectDescriptionChange rewrite the formula references to the
    // column as part of the same undo step.
    if (d_future_table)
        d_future_table->beginMacro(tr("%1: rename to %2").arg(c->name(), text));
    c->setName(text);
    if (d_future_table)
        d_future_table->endMacro();
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
    Column *c = column(col);
    return c ? QString(name() + "_" + c->name()) : QString();
}

void Table::insertCols(int start, int count)
{
    if (d_future_table) {
        if (start < 0)
            start = 0;

        QList<Column *> cols;
        const QStringList names = d_future_table->nextSequentialColumnNames(count);
        for (const QString &nm : names)
            cols << new Column(nm, SciDAVis::ColumnMode::Numeric);
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
        d_future_table->beginMacro(tr("%1: add column").arg(name()));
        d_future_table->addColumn();
        column(d_future_table->columnCount() - 1)
                ->setColumnMode(SciDAVis::ColumnMode::Numeric); // in case we ever change the default
        column(d_future_table->columnCount() - 1)->setPlotDesignation(pd);
        d_future_table->endMacro();
    }
}

void Table::addColumns(int c)
{
    if (d_future_table) {
        QList<Column *> cols;
        const QStringList names = d_future_table->nextSequentialColumnNames(c);
        for (const QString &nm : names)
            cols << new Column(nm, SciDAVis::ColumnMode::Numeric);
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
    Column *colPtr = column(col);
    if (!colPtr || row < 0 || row >= numRows())
        return;
    if (d_future_table)
        d_future_table->beginMacro(tr("%1: clear selected cell(s)").arg(name()));
    if (colPtr->columnMode() == SciDAVis::ColumnMode::Text) {
        if (row < colPtr->rowCount()) {
            colPtr->setTextAt(row, QString());
            colPtr->setInvalid(row, false);
        }
    } else {
        colPtr->setInvalid(row, true);
    }
    if (d_future_table)
        d_future_table->endMacro();
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
        // One bulk command per run of selected cells in each column (instead of four Undo
        // commands per cell), limited to the column's own length so that short columns are
        // not extended, and honouring non-rectangular selections.
        const bool formulaMode = formulaModeActive();
        for (int c = firstCol; c <= lastCol && c < numCols(); ++c) {
            Column *colPtr = column(c);
            if (!colPtr)
                continue;
            const int rows = qMin(lastRow + 1, colPtr->rowCount());
            QList<Interval<int>> runs;
            int runStart = -1;
            for (int r = qMax(0, firstRow); r <= rows; ++r) {
                const bool sel = (r < rows) && isCellSelected(r, c);
                if (sel && runStart < 0)
                    runStart = r;
                else if (!sel && runStart >= 0) {
                    runs.append(Interval<int>(runStart, r - 1));
                    runStart = -1;
                }
            }
            for (int k = runs.size() - 1; k >= 0; --k) {
                const Interval<int> &i = runs.at(k);
                if (formulaMode) {
                    colPtr->setFormula(i, QString());
                } else if (colPtr->columnMode() == SciDAVis::ColumnMode::Text) {
                    QStringList empties;
                    for (int j = 0; j < i.size(); ++j)
                        empties << QString();
                    colPtr->replaceTexts(i.start(), empties);
                } else {
                    colPtr->setInvalid(i, true);
                }
            }
        }
    } else {
        int curRow = -1, curCol = -1;
        getCurrentCell(&curRow, &curCol);
        if (curRow >= 0 && curRow < numRows() && curCol >= 0 && curCol < numCols()) {
            Column *colPtr = column(curCol);
            if (colPtr && curRow < colPtr->rowCount()) {
                if (formulaModeActive())
                    colPtr->setFormula(curRow, QString());
                else
                    clearCell(curRow, curCol);
            }
        }
    }
    d_future_table->endMacro();
}

void Table::copySelection()
{
    if (d_future_table)
        d_future_table->copySelection();
}

void Table::copySelectionWithHeaders()
{
    if (d_future_table)
        d_future_table->copySelectionWithHeaders();
}

namespace {
bool resolveTableImportSource(const Table *tbl, QString &outPath, QString &outSheet, QString &outSep,
                              QString &outEnc, int &outIgnored, int &outUnitsRow)
{
    if (!tbl || !tbl->d_future_table)
        return false;
    outPath = tbl->d_future_table->importSourcePath();
    outSheet = tbl->d_future_table->importSourceSheet();
    outSep = tbl->d_future_table->importSourceSeparator();
    outEnc = tbl->d_future_table->importSourceEncoding();
    outIgnored = tbl->d_future_table->importSourceIgnoredLines();
    outUnitsRow = tbl->d_future_table->importSourceUnitsRow();
    if (!outPath.isEmpty())
        return true;

    const QString lbl = tbl->d_future_table->comment().trimmed();
    if (lbl.isEmpty())
        return false;
    const QRegularExpression sheetSuffixRx(QStringLiteral("^(.+)\\s+\\[([^\\[\\]]+)\\]$"));
    const QRegularExpressionMatch m = sheetSuffixRx.match(lbl);
    if (m.hasMatch()) {
        const QString cand = m.captured(1).trimmed();
        if (QFileInfo(cand).isFile()) {
            outPath = cand;
            outSheet = m.captured(2);
            return true;
        }
    }
    if ((lbl.contains(QLatin1Char('/')) || lbl.contains(QLatin1Char('\\')))
        && QFileInfo(lbl).isFile()) {
        outPath = lbl;
        return true;
    }
    return false;
}
} // namespace

bool Table::hasImportSource() const
{
    QString path, sheet, sep, enc;
    int ignored = 0, unitsRow = -1;
    return resolveTableImportSource(this, path, sheet, sep, enc, ignored, unitsRow);
}

bool Table::reimportFromSource()
{
    if (!d_future_table)
        return false;
    registerTableWorksheetTranslations();

    QString path, sheet, sep, enc;
    int ignoredLines = 0;
    int unitsRow = -1;
    if (!resolveTableImportSource(this, path, sheet, sep, enc, ignoredLines, unitsRow)
        || path.isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::information(this, tr("Import failed"),
                                     tr("No original source file is recorded for this worksheet."));
        }
        return false;
    }
    const QString numericLocaleName = d_future_table->importSourceNumericLocale();
    if (!QFileInfo::exists(path)) {
        const QString msg = tr("Source file not found:\n%1").arg(QDir::toNativeSeparators(path));
        if (!ApplicationWindow::isNonInteractiveContext(this))
            QMessageBox::warning(this, tr("Import failed"), msg);
        return false;
    }

    const QString ext = QFileInfo(path).suffix().toLower();
    QList<Column *> newCols;
    if (ext == QLatin1String("xlsx") || ext == QLatin1String("xlsm")) {
        XlsxImport::Workbook wb;
        QString err;
        if (!XlsxImport::readWorkbook(path, wb, &err, sheet)) {
            if (!ApplicationWindow::isNonInteractiveContext(this))
                QMessageBox::warning(this, tr("Import failed"), err);
            return false;
        }
        if (wb.sheets.isEmpty()) {
            const QString msg = tr("Sheet '%1' was not found in '%2'.")
                                        .arg(sheet, QDir::toNativeSeparators(path));
            if (!ApplicationWindow::isNonInteractiveContext(this))
                QMessageBox::warning(this, tr("Import failed"), msg);
            return false;
        }
        newCols = XlsxImport::sheetToColumns(wb.sheets.first(), wb.date1904);
    } else {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            const QString msg = tr("Could not open the file: %1\n%2")
                                        .arg(QDir::toNativeSeparators(path), file.errorString());
            if (!ApplicationWindow::isNonInteractiveContext(this))
                QMessageBox::warning(this, tr("Import failed"), msg);
            return false;
        }
        AsciiTableImportFilter filter;
        filter.set_convert_to_numeric(true);
        filter.set_trim_whitespace(true);
        filter.set_separator(sep);
        filter.set_encoding(enc);
        filter.set_units_row(unitsRow);
        if (ignoredLines > 0)
            filter.set_ignored_lines(ignoredLines);
        // Same decimal mark as the original import (German "1,5"); unrecorded = C locale.
        if (!numericLocaleName.isEmpty())
            filter.set_numeric_locale(QLocale(numericLocaleName));
        std::unique_ptr<future::Table> imported(
                static_cast<future::Table *>(filter.importAspect(file)));
        if (!imported) {
            if (!ApplicationWindow::isNonInteractiveContext(this)) {
                QString msg = tr("Could not parse data from file: %1")
                                      .arg(QDir::toNativeSeparators(path));
                const QString refusal = AsciiTableImportFilter::lastImportReport().refusal;
                if (!refusal.isEmpty())
                    msg += QStringLiteral("\n") + refusal;
                QMessageBox::warning(this, tr("Import failed"), msg);
            }
            return false;
        }
        for (int i = 0; i < imported->columnCount(); ++i) {
            Column *src = imported->column(i);
            if (!src)
                continue;
            Column *cp = new Column(src->name(), src->columnMode());
            cp->copy(src);
            cp->setPlotDesignation(src->plotDesignation());
            cp->setLongName(src->longName());
            cp->setUnits(src->units());
            cp->setComment(src->comment());
            if (src->property("_tg_label_role").isValid())
                cp->setProperty("_tg_label_role", src->property("_tg_label_role"));
            newCols.append(cp);
        }
    }

    if (newCols.isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(
                    this, tr("Import failed"),
                    tr("Could not parse data from file: %1").arg(QDir::toNativeSeparators(path)));
        }
        return false;
    }

    QHash<QString, SciDAVis::PlotDesignation> oldPdByName;
    QVector<SciDAVis::PlotDesignation> oldPdByIndex;
    oldPdByIndex.reserve(numCols());
    for (int i = 0; i < numCols(); ++i) {
        if (Column *cp = column(i)) {
            oldPdByName.insert(cp->name(), cp->plotDesignation());
            oldPdByIndex.push_back(cp->plotDesignation());
        } else {
            oldPdByIndex.push_back(SciDAVis::noDesignation);
        }
    }
    for (int i = 0; i < newCols.size(); ++i) {
        Column *nc = newCols.at(i);
        if (!nc)
            continue;
        if (oldPdByName.contains(nc->name()))
            nc->setPlotDesignation(oldPdByName.value(nc->name()));
        else if (i < oldPdByIndex.size())
            nc->setPlotDesignation(oldPdByIndex.at(i));
    }

    int maxRows = 0;
    for (Column *nc : newCols) {
        if (nc)
            maxRows = qMax(maxRows, nc->rowCount());
    }
    const int importedSpan = computeImportedColumnSpan(d_future_table);
    const int common = qMin(importedSpan, newCols.size());
    const int prevRows = numRows();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    d_future_table->beginMacro(tr("%1: re-import from source").arg(name()));
    d_future_table->setRowCount(maxRows);
    if (importedSpan > newCols.size())
        d_future_table->removeColumns(newCols.size(), importedSpan - newCols.size());

    QVector<QString> targetNames(common);
    for (int i = 0; i < common; ++i) {
        Column *dst = d_future_table->column(i);
        Column *src = newCols.at(i);
        if (!dst || !src) {
            delete src;
            continue;
        }
        dst->clearFormulas();
        dst->clearMasks();
        if (dst->columnMode() != src->columnMode())
            dst->setColumnMode(src->columnMode());
        dst->copy(src);
        targetNames[i] = src->name();
        dst->setPlotDesignation(src->plotDesignation());
        dst->setLongName(src->longName());
        dst->setUnits(src->units());
        dst->setComment(src->comment());
        dst->setProperty("_tg_label_role", src->property("_tg_label_role"));
        delete src;
    }

    // Two-pass rename prevents sibling collisions when columns are swapped or removed (e.g. [A,B]->[B,A]).
    int tmpIdx = 0;
    for (int i = 0; i < common; ++i) {
        Column *dst = d_future_table->column(i);
        if (!dst || targetNames[i].isEmpty() || dst->name() == targetNames[i])
            continue;
        QString tmpName;
        do {
            tmpName = QStringLiteral("__tg_reimp_tmp_%1").arg(tmpIdx++);
        } while (d_future_table->column(tmpName, false) != nullptr
                 || targetNames.contains(tmpName));
        dst->setName(tmpName);
    }
    for (int i = 0; i < common; ++i) {
        Column *dst = d_future_table->column(i);
        if (!dst || targetNames[i].isEmpty() || dst->name() == targetNames[i])
            continue;
        dst->setName(targetNames[i]);
    }

    if (newCols.size() > common) {
        QList<Column *> extraCols;
        extraCols.reserve(newCols.size() - common);
        for (int i = common; i < newCols.size(); ++i) {
            if (Column *nc = newCols.at(i))
                extraCols.append(nc);
        }
        if (!extraCols.isEmpty())
            d_future_table->insertColumns(common, extraCols);
    }
    d_future_table->setImportSource(path, sheet, sep, enc, ignoredLines, unitsRow,
                                    newCols.size(), numericLocaleName);
    const QVector<int> derivedCols =
            collectPreservedFormulaCols(d_future_table, newCols.size(), prevRows, maxRows);
    if (!derivedCols.isEmpty() && d_auto_recalc_formulas && !isUndoRedoReplaying()) {
        evaluateCrossTableDag(derivedCols, true);
        reportFormulaErrorsToUser(-1);
    }
    d_future_table->endMacro();
    QApplication::restoreOverrideCursor();
    notifyChanges();

    if (ApplicationWindow *aw = owningApplicationWindowForTable(this)) {
        for (MyWidget *mw : aw->windowsList()) {
            MultiLayer *ml = qobject_cast<MultiLayer *>(mw);
            if (!ml)
                continue;
            for (QWidget *gw : ml->graphPtrs()) {
                Graph *g = qobject_cast<Graph *>(gw);
                if (!g)
                    continue;
                bool graphUpdated = false;
                // setFullRange() reloads the curve's error bars, which may remove a curve and
                // shift the indices; walk a snapshot of the (never reused) curve keys instead.
                QVector<long> keys;
                for (int ci = 0; ci < g->curves(); ++ci)
                    keys.append(g->curveKey(ci));
                for (long key : keys) {
                    const int ci = g->curveIndex(key);
                    if (ci < 0)
                        continue;
                    DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(ci));
                    if (!dc || dc->table() != this)
                        continue;
                    if (dc->startRow() == 0 && (prevRows <= 0 || dc->endRow() == prevRows - 1)) {
                        dc->setFullRange();
                        graphUpdated = true;
                    }
                }
                if (graphUpdated)
                    g->updatePlot();
            }
        }
    }

    bool anyMeta = false;
    for (int i = 0; i < numCols(); ++i) {
        autoExpandColumnWidthForHeader(i, false);
        if (Column *cp = column(i)) {
            if (!cp->longName().isEmpty() || !cp->units().isEmpty() || !cp->comment().isEmpty())
                anyMeta = true;
        }
    }
    if (anyMeta)
        showComments(true);
    if (ApplicationWindow *aw = owningApplicationWindowForTable(this)) {
        if (aw->statusBar()) {
            aw->statusBar()->showMessage(
                    tr("Re-imported from %1").arg(QDir::toNativeSeparators(path)), 5000);
        }
    }
    return true;
}

void Table::pasteSelection()
{
    pasteSelection(false);
}

void Table::pasteSelection(bool transposed)
{
    if (!d_future_table)
        return;
    const QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard)
        return;
    const QString rawText = clipboard->text();
    if (rawText.isEmpty())
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    pasteText(rawText, transposed);
    QApplication::restoreOverrideCursor();
}

bool Table::pasteText(const QString &rawText, bool transposed)
{
    if (!d_future_table || rawText.isEmpty())
        return false;

    QString normalized = rawText;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');

    auto splitLogicalRecords = [](const QString &src) -> QStringList {
        if (!src.contains(QLatin1Char('"')))
            return src.split(QLatin1Char('\n'));
        QStringList out;
        int recStart = 0;
        bool inQuotes = false;
        bool atFieldStart = true;
        const int n = src.size();
        for (int i = 0; i < n; ++i) {
            const QChar ch = src.at(i);
            if (inQuotes) {
                if (ch == QLatin1Char('"')) {
                    if (i + 1 < n && src.at(i + 1) == QLatin1Char('"'))
                        ++i;
                    else
                        inQuotes = false;
                }
            } else if (ch == QLatin1Char('\n')) {
                out.append(src.mid(recStart, i - recStart));
                recStart = i + 1;
                atFieldStart = true;
            } else if (ch == QLatin1Char('\t') || ch == QLatin1Char(',')
                       || ch == QLatin1Char(';')) {
                atFieldStart = true;
            } else if (atFieldStart && ch == QLatin1Char('"')) {
                inQuotes = true;
                atFieldStart = false;
            } else if (ch != QLatin1Char(' ')) {
                atFieldStart = false;
            }
        }
        if (recStart <= n)
            out.append(src.mid(recStart));
        return out;
    };

    auto hasUnquotedChar = [](const QString &src, QChar target) -> bool {
        if (!src.contains(target))
            return false;
        if (!src.contains(QLatin1Char('"')))
            return true;
        bool inQuotes = false;
        bool atFieldStart = true;
        const int n = src.size();
        for (int i = 0; i < n; ++i) {
            const QChar ch = src.at(i);
            if (inQuotes) {
                if (ch == QLatin1Char('"')) {
                    if (i + 1 < n && src.at(i + 1) == QLatin1Char('"'))
                        ++i;
                    else
                        inQuotes = false;
                }
            } else if (ch == target) {
                return true;
            } else if (ch == QLatin1Char('\n') || ch == QLatin1Char('\t')
                       || ch == QLatin1Char(',') || ch == QLatin1Char(';')) {
                atFieldStart = true;
            } else if (atFieldStart && ch == QLatin1Char('"')) {
                inQuotes = true;
                atFieldStart = false;
            } else if (ch != QLatin1Char(' ')) {
                atFieldStart = false;
            }
        }
        return false;
    };

    auto splitDelimitedLine = [](const QString &line, QChar sep) -> QStringList {
        if (!line.contains(QLatin1Char('"'))) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            return line.split(sep, Qt::KeepEmptyParts);
#else
            return line.split(sep, QString::KeepEmptyParts);
#endif
        }
        QStringList cells;
        const int n = line.size();
        int i = 0;
        while (true) {
            int scan = i;
            if (sep != QLatin1Char('\t')) {
                while (scan < n && line.at(scan) == QLatin1Char(' '))
                    ++scan;
            }
            if (scan < n && line.at(scan) == QLatin1Char('"')) {
                QString val;
                int p = scan + 1;
                while (p < n) {
                    if (line.at(p) == QLatin1Char('"')) {
                        if (p + 1 < n && line.at(p + 1) == QLatin1Char('"')) {
                            val.append(QLatin1Char('"'));
                            p += 2;
                        } else {
                            ++p;
                            break;
                        }
                    } else {
                        val.append(line.at(p++));
                    }
                }
                while (p < n && line.at(p) != sep) {
                    if (line.at(p) != QLatin1Char(' '))
                        val.append(line.at(p));
                    ++p;
                }
                cells.append(val);
                if (p < n && line.at(p) == sep) {
                    i = p + 1;
                    continue;
                }
                break;
            }
            int end = i;
            while (end < n && line.at(end) != sep)
                ++end;
            cells.append(line.mid(i, end - i));
            if (end < n && line.at(end) == sep) {
                i = end + 1;
                continue;
            }
            break;
        }
        return cells;
    };

    QStringList lines = splitLogicalRecords(normalized);
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    if (lines.isEmpty())
        return false;

    bool hasTab = hasUnquotedChar(normalized, QLatin1Char('\t'));
    bool hasComma = !hasTab && hasUnquotedChar(normalized, QLatin1Char(','));
    bool hasSemicolon = !hasTab && !hasComma && hasUnquotedChar(normalized, QLatin1Char(';'));

    auto isMissingToken = [](const QString &s) -> bool {
        return AsciiTableImportFilter::isMissingDataToken(s);
    };

    auto isGroupedNumberToken = [](const QString &tok) -> bool {
        return isGroupedNumberTokenGlobal(tok);
    };
    auto unquoteCellTrim = [](const QString &raw) -> QString {
        QString cell = raw.trimmed();
        if (cell.size() >= 2 && cell.startsWith(QLatin1Char('"'))
            && cell.endsWith(QLatin1Char('"'))) {
            cell = cell.mid(1, cell.size() - 2)
                           .replace(QLatin1String("\"\""), QLatin1String("\""))
                           .trimmed();
        }
        return cell;
    };
    auto hasUnquotedComma = [](const QString &raw) -> bool {
        bool inQuotes = false;
        for (int i = 0; i < raw.size(); ++i) {
            const QChar ch = raw.at(i);
            if (ch == QLatin1Char('"'))
                inQuotes = !inQuotes;
            else if (ch == QLatin1Char(',') && !inQuotes)
                return true;
        }
        return false;
    };

    auto tryParseDouble = [](const QString &s, double &outVal) -> bool {
        return parseTableDoubleToken(s, outVal);
    };

    // Detect single-column paste containing thousand-grouped numbers (e.g. "12,345\n23,456" or
    // "Stress\n1,012\n1,030") when at most one column is selected, while splitting into CSV
    // columns whenever >= 2 columns are selected, any non-3-digit/decimal-first comma pair
    // appears (e.g. "10,20\n20,40" or "1.0,200\n2.0,400"), or the locale uses ',' as decimal.
    bool keepSingleColumn = false;
    bool stripSingleColGroupCommas = false;
    if (!hasTab && hasComma && QLocale().decimalPoint() != QLatin1Char(',')) {
        int checkIdx = 0;
        while (checkIdx < lines.size()
               && (lines.at(checkIdx).trimmed().isEmpty()
                   || lines.at(checkIdx).trimmed().startsWith(QLatin1Char('#')))) {
            ++checkIdx;
        }
        const int postCommentIdx = checkIdx;
        // Allow up to 2 non-numeric header/unit lines only if they contain no unquoted comma
        for (int hStep = 0; hStep < 2 && checkIdx < lines.size(); ++hStep) {
            const QString rawHdr = lines.at(checkIdx).trimmed();
            if (rawHdr.isEmpty() || hasUnquotedComma(rawHdr))
                break;
            const QString uq = unquoteCellTrim(rawHdr);
            double dv = 0.0;
            if (isGroupedNumberToken(uq) || tryParseDouble(uq, dv) || isMissingToken(uq))
                break;
            ++checkIdx;
        }
        const int headerLineCount = checkIdx - postCommentIdx;
        bool allValidSingleCol = (checkIdx < lines.size());
        int groupedNumCount = 0;
        int plainNumCount = 0;
        int nonEmptyDataCount = 0;
        bool anyUnquotedDataComma = false;
        for (int li = checkIdx; li < lines.size(); ++li) {
            const QString rawLine = lines.at(li).trimmed();
            if (rawLine.isEmpty())
                continue;
            ++nonEmptyDataCount;
            if (hasUnquotedComma(rawLine)) {
                anyUnquotedDataComma = true;
                if (rawLine.contains(QLatin1Char('"'))) {
                    allValidSingleCol = false;
                    break;
                }
            }
            const QString uq = unquoteCellTrim(rawLine);
            if (isGroupedNumberToken(uq)) {
                ++groupedNumCount;
                continue;
            }
            if (uq.contains(QLatin1Char(','))) {
                allValidSingleCol = false;
                break;
            }
            if (isMissingToken(uq))
                continue;
            double dv = 0.0;
            bool okPlain = false;
            dv = QLocale::c().toDouble(uq, &okPlain);
            if (!okPlain) {
                allValidSingleCol = false;
                break;
            }
            ++plainNumCount;
            Q_UNUSED(dv);
        }
        const int selCols = selectedColumnCount(false);
        const bool explicitMultiColSelection =
                (selCols >= 2 && nonEmptyDataCount >= 1 && anyUnquotedDataComma);
        Q_UNUSED(headerLineCount);
        Q_UNUSED(plainNumCount);
        if (allValidSingleCol && groupedNumCount > 0 && !explicitMultiColSelection) {
            hasComma = false;
            hasSemicolon = false;
            keepSingleColumn = true;
            stripSingleColGroupCommas = true;
        }
    }

    // Strip leading '#' preamble lines above header/data (keep last '#' line if followed immediately by numeric row)
    if (!formulaModeActive() && lines.size() >= 2 && lines.first().trimmed().startsWith(QLatin1Char('#'))) {
        int hashCount = 0;
        while (hashCount < lines.size() && lines.at(hashCount).trimmed().startsWith(QLatin1Char('#')))
            ++hashCount;
        if (hashCount < lines.size()) {
            const QString firstNonHash = lines.at(hashCount).trimmed();
            QString firstCell = firstNonHash;
            if (hasTab)
                firstCell = firstNonHash.section(QLatin1Char('\t'), 0, 0).trimmed();
            else if (hasComma)
                firstCell = firstNonHash.section(QLatin1Char(','), 0, 0).trimmed();
            else if (hasSemicolon)
                firstCell = firstNonHash.section(QLatin1Char(';'), 0, 0).trimmed();
            double dv = 0.0;
            if (tryParseDouble(firstCell, dv) && !std::isnan(dv)) {
                lines = lines.mid(hashCount - 1);
                lines[0] = lines[0].trimmed().mid(1).trimmed();
            } else {
                lines = lines.mid(hashCount);
            }
        }
        if (lines.isEmpty())
            return false;
    }

    if (!keepSingleColumn && !hasTab && !hasComma && !hasSemicolon && lines.size() >= 2) {
        const QString h0 = lines.at(0).trimmed();
        double dummy0 = 0.0;
        if (!h0.isEmpty() && !tryParseDouble(h0, dummy0)) {
            int dataCheckStart = 1;
            if (lines.size() >= 3) {
                const QString h1 = lines.at(1).trimmed();
                double dummy1 = 0.0;
                if (!h1.isEmpty() && !tryParseDouble(h1, dummy1) && !isMissingToken(h1))
                    dataCheckStart = 2;
            }
            bool allDataSingleToken = true;
            bool anyDataNumeric = false;
            for (int li = dataCheckStart; li < lines.size(); ++li) {
                const QString tl = lines.at(li).trimmed();
                if (tl.isEmpty())
                    continue;
                if (tl.contains(QRegularExpression(QStringLiteral("\\s")))) {
                    allDataSingleToken = false;
                    break;
                }
                double dv = 0.0;
                if (tryParseDouble(tl, dv))
                    anyDataNumeric = true;
            }
            if (allDataSingleToken && anyDataNumeric)
                keepSingleColumn = true;
        }
    }

    QList<QStringList> grid;
    int maxCols = 0;
    for (const QString &line : lines) {
        QStringList rowCells;
        if (hasTab) {
            rowCells = splitDelimitedLine(line, QLatin1Char('\t'));
        } else if (hasComma) {
            rowCells = splitDelimitedLine(line, QLatin1Char(','));
        } else if (hasSemicolon) {
            rowCells = splitDelimitedLine(line, QLatin1Char(';'));
        } else if (keepSingleColumn) {
            rowCells << line.trimmed();
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
        for (int i = 0; i < rowCells.size(); ++i) {
            QString cell = unquoteCellTrim(rowCells[i]);
            if (stripSingleColGroupCommas && isGroupedNumberToken(cell))
                cell.remove(QLatin1Char(','));
            rowCells[i] = cell;
        }
        maxCols = qMax(maxCols, rowCells.size());
        grid.append(rowCells);
    }

    if (transposed) {
        QList<QStringList> t;
        for (int c = 0; c < maxCols; ++c) {
            QStringList row;
            for (const QStringList &src : grid)
                row << ((c < src.size()) ? src.at(c) : QString());
            t << row;
        }
        maxCols = grid.size();
        grid = t;
    }

    int pastedRows = grid.size();
    int pastedCols = maxCols;
    if (pastedRows <= 0 || pastedCols <= 0)
        return false;

    const QModelIndexList selIdxs = (d_view_widget && d_view_widget->selectionModel())
            ? d_view_widget->selectionModel()->selectedIndexes()
            : QModelIndexList();
    if (pastedRows == 1 && pastedCols == 1 && selIdxs.size() >= 2) {
        QMap<int, QList<int>> colRows;
        for (const QModelIndex &idx : selIdxs) {
            if (idx.isValid() && idx.column() >= 0 && idx.column() < numCols() && idx.row() >= 0
                && idx.row() < numRows()) {
                colRows[idx.column()].append(idx.row());
            }
        }
        if (!colRows.isEmpty()) {
            const QString scalarVal = grid[0].isEmpty() ? QString() : grid[0][0];
            const bool isMissing = isMissingToken(scalarVal);
            double parsedNum = 0.0;
            const bool isNumericVal = !isMissing && tryParseDouble(scalarVal, parsedNum)
                    && !std::isnan(parsedNum);
            const bool isNonNumText = !isMissing && !isNumericVal;
            int droppedTextCells = 0;

            d_future_table->beginMacro(tr("%1: paste from clipboard").arg(name()));
            for (auto it = colRows.begin(); it != colRows.end(); ++it) {
                Column *colPtr = column(it.key());
                if (!colPtr)
                    continue;
                QList<int> rowsList = it.value();
                std::sort(rowsList.begin(), rowsList.end());
                rowsList.erase(std::unique(rowsList.begin(), rowsList.end()), rowsList.end());
                if (rowsList.isEmpty())
                    continue;

                QVector<Interval<int>> runs;
                int runStart = rowsList.first();
                int runPrev = runStart;
                for (int i = 1; i < rowsList.size(); ++i) {
                    const int r = rowsList.at(i);
                    if (r == runPrev + 1) {
                        runPrev = r;
                    } else {
                        runs.append(Interval<int>(runStart, runPrev));
                        runStart = runPrev = r;
                    }
                }
                runs.append(Interval<int>(runStart, runPrev));

                if (formulaModeActive()) {
                    for (const Interval<int> &iv : runs) {
                        colPtr->setFormula(iv, scalarVal);
                        colPtr->setInvalid(iv, false);
                    }
                    continue;
                }

                bool dropAsInvalid = false;
                if (colPtr->columnMode() == SciDAVis::ColumnMode::Numeric && isNonNumText) {
                    QSet<int> targetSet;
                    for (int r : rowsList)
                        targetSet.insert(r);
                    bool otherNumbers = false;
                    for (int r = 0; r < colPtr->rowCount() && !otherNumbers; ++r) {
                        if (!targetSet.contains(r) && !colPtr->isInvalid(r))
                            otherNumbers = true;
                    }
                    if (!otherNumbers && colPtr->formulaIntervals().isEmpty()) {
                        colPtr->setColumnMode(SciDAVis::ColumnMode::Text);
                    } else {
                        droppedTextCells += rowsList.size();
                        dropAsInvalid = true;
                    }
                }

                for (const Interval<int> &iv : runs) {
                    const int span = iv.size();
                    if (colPtr->columnMode() == SciDAVis::ColumnMode::Numeric) {
                        if (isMissing || dropAsInvalid) {
                            colPtr->setInvalid(iv, true);
                        } else {
                            QVector<qreal> vals(span, parsedNum);
                            colPtr->replaceValues(iv.start(), vals);
                        }
                    } else if (colPtr->columnMode() == SciDAVis::ColumnMode::Text) {
                        QStringList strs;
                        strs.reserve(span);
                        for (int k = 0; k < span; ++k)
                            strs << scalarVal;
                        colPtr->replaceTexts(iv.start(), strs);
                    } else {
                        QStringList strs;
                        strs.reserve(span);
                        const QString fillStr = isMissing ? QString() : scalarVal;
                        for (int k = 0; k < span; ++k)
                            strs << fillStr;
                        colPtr->asStringColumn()->replaceTexts(iv.start(), strs);
                        if (isMissing)
                            colPtr->setInvalid(iv, true);
                    }
                }
            }
            d_future_table->endMacro();

            if (droppedTextCells > 0) {
                registerTableWorksheetTranslations();
                if (ApplicationWindow *aw = owningApplicationWindowForTable(this)) {
                    aw->statusBar()->showMessage(
                            tr("%n pasted cell(s) were not numbers and were left empty.", "",
                               droppedTextCells),
                            10000);
                }
            }
            return true;
        }
    }

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
    if (neededCols > numCols())
        setNumCols(neededCols);

    auto stripOuterUnitBrackets = [](const QString &raw) -> QString {
        const QString s = raw.trimmed();
        if (s.size() >= 2) {
            if ((s.startsWith('(') && s.endsWith(')'))
                || (s.startsWith('[') && s.endsWith(']'))
                || (s.startsWith(QChar(0xFF08)) && s.endsWith(QChar(0xFF09)))
                || (s.startsWith(QChar(0xFF3B)) && s.endsWith(QChar(0xFF3D)))) {
                return s.mid(1, s.size() - 2).trimmed();
            }
        }
        return s;
    };

    int dataStartRow = 0;
    if (startRow == 0 && !formulaModeActive() && !transposed && pastedRows >= 2) {
        bool row0HasNonNumericHeaderOnNumCol = false;
        bool row0HasNumericOnNumCol = false;
        auto rowNumericStatsOnNumCols = [&](int rIdx, bool &anyNum, bool &anyNonNumText) {
            anyNum = false;
            anyNonNumText = false;
            if (rIdx < 0 || rIdx >= pastedRows)
                return;
            for (int c = 0; c < pastedCols; ++c) {
                Column *colPtr = column(startCol + c);
                if (!colPtr || colPtr->columnMode() != SciDAVis::ColumnMode::Numeric)
                    continue;
                const QString token = (c < grid[rIdx].size()) ? grid[rIdx][c].trimmed() : QString();
                if (token.isEmpty() || isMissingToken(token))
                    continue;
                double dv = 0.0;
                if (tryParseDouble(token, dv) && !std::isnan(dv))
                    anyNum = true;
                else
                    anyNonNumText = true;
            }
        };

        auto normalizeHeaderCell = [](QString s) -> QString {
            s.replace(QLatin1Char('\r'), QLatin1Char(' '));
            s.replace(QLatin1Char('\n'), QLatin1Char(' '));
            return s.simplified();
        };

        rowNumericStatsOnNumCols(0, row0HasNumericOnNumCol, row0HasNonNumericHeaderOnNumCol);
        auto allRowsPureNumericFrom = [&](int fromRow) -> bool {
            if (fromRow >= pastedRows)
                return false;
            bool sawAnyNum = false;
            for (int r = fromRow; r < pastedRows; ++r) {
                bool rNum = false, rTxt = false;
                rowNumericStatsOnNumCols(r, rNum, rTxt);
                if (rTxt)
                    return false;
                if (rNum)
                    sawAnyNum = true;
            }
            return sawAnyNum;
        };
        bool row0AllColsNonBlank = (pastedCols >= 2 && grid[0].size() >= pastedCols);
        for (int c = 0; row0AllColsNonBlank && c < pastedCols; ++c) {
            if (grid[0][c].trimmed().isEmpty())
                row0AllColsNonBlank = false;
        }
        const bool allowMixedHeaderRow0 = row0HasNonNumericHeaderOnNumCol && row0HasNumericOnNumCol
                && row0AllColsNonBlank
                && (allRowsPureNumericFrom(1)
                    || (pastedRows >= 3 && allRowsPureNumericFrom(2)));
        if (row0HasNonNumericHeaderOnNumCol && (!row0HasNumericOnNumCol || allowMixedHeaderRow0)) {
            bool row1AnyNum = false;
            bool row1AnyNonNum = false;
            rowNumericStatsOnNumCols(1, row1AnyNum, row1AnyNonNum);
            int headerRows = 0;
            if (row1AnyNum) {
                headerRows = 1;
            } else if (pastedRows >= 3 && !row1AnyNum) {
                bool row1HasUnitToken = row1AnyNonNum;
                if (!row1HasUnitToken) {
                    // Check if row 1 has '-' dimensionless unit markers while row 2 is numeric
                    for (int c = 0; c < grid[1].size(); ++c) {
                        const QString t1 = grid[1][c].trimmed();
                        if (t1 == QLatin1String("-") || t1 == QStringLiteral("\u2014"))
                            row1HasUnitToken = true;
                    }
                }
                bool row2AnyNum = false;
                bool row2AnyNonNum = false;
                rowNumericStatsOnNumCols(2, row2AnyNum, row2AnyNonNum);
                if (row1HasUnitToken && row2AnyNum)
                    headerRows = 2;
            }

            if (headerRows == 1 || headerRows == 2) {
                for (int c = 0; c < pastedCols; ++c) {
                    Column *colPtr = column(startCol + c);
                    if (!colPtr)
                        continue;
                    const QString h0 =
                            (c < grid[0].size()) ? normalizeHeaderCell(grid[0][c]) : QString();
                    if (headerRows == 1) {
                        if (!h0.isEmpty()) {
                            QString shortName, parsedLongName, parsedUnit;
                            AsciiTableImportFilter::splitHeaderNameAndUnits(
                                    h0, shortName, parsedLongName, parsedUnit);
                            colPtr->setLongName(parsedLongName.isEmpty() ? h0 : parsedLongName);
                            if (!parsedUnit.isEmpty())
                                colPtr->setUnits(parsedUnit);
                        }
                    } else {
                        const QString h1 =
                                (c < grid[1].size()) ? normalizeHeaderCell(grid[1][c]) : QString();
                        QString short0, long0, unit0;
                        AsciiTableImportFilter::splitHeaderNameAndUnits(h0, short0, long0, unit0);
                        const QString unit1 = stripOuterUnitBrackets(h1);
                        if (!h0.isEmpty()) {
                            colPtr->setLongName((!unit1.isEmpty() && unit0.isEmpty())
                                                        ? h0
                                                        : (long0.isEmpty() ? h0 : long0));
                        }
                        if (!unit1.isEmpty())
                            colPtr->setUnits(unit1);
                        else if (!unit0.isEmpty())
                            colPtr->setUnits(unit0);
                    }
                    autoExpandColumnWidthForHeader(startCol + c, false);
                }
                showComments(true);
                dataStartRow = headerRows;
            }
        }
    }

    const int dataRowCount = pastedRows - dataStartRow;
    int neededRows = startRow + qMax(1, dataRowCount);
    if (neededRows > numRows())
        setNumRows(neededRows);

    // Column-wise bulk writes: one replaceValues()/replaceTexts() per column plus one
    // setInvalid() per run of empty cells, instead of one Undo command per cell (a 10,000-row
    // paste used to push tens of thousands of commands). Everything stays in this one macro.
    auto markInvalidRuns = [](Column *colPtr, int firstRow, const QVector<bool> &invalid) {
        int k = 0;
        const int n = invalid.size();
        while (k < n) {
            if (!invalid[k]) {
                ++k;
                continue;
            }
            int kEnd = k;
            while (kEnd + 1 < n && invalid[kEnd + 1])
                ++kEnd;
            colPtr->setInvalid(Interval<int>(firstRow + k, firstRow + kEnd), true);
            k = kEnd + 1;
        }
    };
    int droppedTextCells = 0;
    for (int c = 0; c < pastedCols && dataRowCount > 0; ++c) {
        int targetCol = startCol + c;
        Column *colPtr = column(targetCol);
        if (!colPtr)
            continue;
        QStringList cells;
        cells.reserve(dataRowCount);
        for (int r = 0; r < dataRowCount; ++r) {
            const int srcRow = dataStartRow + r;
            cells << ((c < grid[srcRow].size()) ? grid[srcRow][c] : QString());
        }

        if (formulaModeActive()) {
            for (int r = 0; r < dataRowCount; ++r) {
                colPtr->setFormula(startRow + r, cells.at(r));
                colPtr->setInvalid(startRow + r, false);
            }
            continue;
        }

        QVector<bool> invalid(dataRowCount, false);
        if (colPtr->columnMode() == SciDAVis::ColumnMode::Numeric) {
            QVector<qreal> values(dataRowCount, 0.0);
            int textCells = 0;
            for (int r = 0; r < dataRowCount; ++r) {
                const QString &cellStr = cells.at(r);
                double val = 0.0;
                if (isMissingToken(cellStr)) {
                    invalid[r] = true;
                } else if (tryParseDouble(cellStr, val) && !std::isnan(val)) {
                    values[r] = val;
                } else {
                    invalid[r] = true;
                    ++textCells;
                }
            }
            if (textCells > 0) {
                // A column that holds no numbers outside the pasted range becomes a text
                // column (sample names, dates as text, ...). A column with other numbers keeps
                // its type; the text cells are left empty and counted below.
                bool otherNumbers = false;
                for (int r = 0; r < colPtr->rowCount() && !otherNumbers; ++r) {
                    if ((r < startRow || r >= startRow + dataRowCount) && !colPtr->isInvalid(r))
                        otherNumbers = true;
                }
                if (!otherNumbers && colPtr->formulaIntervals().isEmpty()) {
                    colPtr->setColumnMode(SciDAVis::ColumnMode::Text);
                } else {
                    droppedTextCells += textCells;
                }
            }
            if (colPtr->columnMode() == SciDAVis::ColumnMode::Numeric) {
                colPtr->replaceValues(startRow, values);
                markInvalidRuns(colPtr, startRow, invalid);
                continue;
            }
            invalid.fill(false);
        }

        if (colPtr->columnMode() == SciDAVis::ColumnMode::Text) {
            colPtr->replaceTexts(startRow, cells);
        } else {
            for (int r = 0; r < dataRowCount; ++r) {
                if (isMissingToken(cells.at(r))) {
                    invalid[r] = true;
                    cells[r].clear();
                }
            }
            colPtr->asStringColumn()->replaceTexts(startRow, cells);
            markInvalidRuns(colPtr, startRow, invalid);
        }
    }

    deselectAll();
    if (dataRowCount > 0) {
        setCellsSelected(startRow, startCol, startRow + dataRowCount - 1,
                         startCol + pastedCols - 1, true);
    }
    goToCell(startRow, startCol);

    d_future_table->endMacro();

    if (droppedTextCells > 0) {
        registerTableWorksheetTranslations();
        if (ApplicationWindow *aw = owningApplicationWindowForTable(this))
            aw->statusBar()->showMessage(
                    tr("%n pasted cell(s) were not numbers and were left empty.", "",
                       droppedTextCells),
                    10000);
    }
    return true;
}

void Table::fillSelectionDown()
{
    if (!d_future_table)
        return;
    int firstRow = firstSelectedRow(false);
    int lastRow = lastSelectedRow(false);
    int firstCol = firstSelectedColumn(false);
    int lastCol = lastSelectedColumn(false);
    if (firstRow < 0 || firstCol < 0) {
        int r = -1, c = -1;
        getCurrentCell(&r, &c);
        firstRow = lastRow = r;
        firstCol = lastCol = c;
    }
    if (firstRow < 0 || firstCol < 0)
        return;
    // Like Excel: with a single row selected, copy the row above into it.
    int srcRow = firstRow;
    int fillFrom = firstRow + 1;
    if (lastRow == firstRow) {
        if (firstRow == 0)
            return;
        srcRow = firstRow - 1;
        fillFrom = firstRow;
    }
    const int count = lastRow - fillFrom + 1;
    if (count <= 0)
        return;

    d_future_table->beginMacro(tr("%1: fill down").arg(name()));
    for (int c = firstCol; c <= lastCol && c < numCols(); ++c) {
        Column *col = column(c);
        if (!col)
            continue;
        if (formulaModeActive()) {
            col->setFormula(Interval<int>(fillFrom, lastRow), col->formula(srcRow));
            continue;
        }
        const bool srcInvalid = col->isInvalid(srcRow);
        if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
            col->replaceValues(fillFrom, QVector<qreal>(count, srcInvalid ? 0.0 : col->valueAt(srcRow)));
        } else {
            QStringList texts;
            const QString t = srcInvalid ? QString() : col->asStringColumn()->textAt(srcRow);
            for (int k = 0; k < count; ++k)
                texts << t;
            if (col->columnMode() == SciDAVis::ColumnMode::Text)
                col->replaceTexts(fillFrom, texts);
            else
                col->asStringColumn()->replaceTexts(fillFrom, texts);
        }
        if (srcInvalid)
            col->setInvalid(Interval<int>(fillFrom, lastRow), true);
    }
    d_future_table->endMacro();
}

namespace {
bool cellTextMatches(const QString &cell, const QString &what, bool matchCase, bool wholeCell)
{
    const Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
    if (wholeCell)
        return cell.compare(what, cs) == 0;
    return cell.contains(what, cs);
}

QString cellRawEditString(Table *tbl, Column *col, int r, int c, bool formulaMode)
{
    if (!col || !tbl)
        return QString();
    if (formulaMode)
        return col->formula(r);
    if (col->isInvalid(r) || r >= col->rowCount())
        return QString();
    if (col->dataType() == SciDAVis::TypeDouble)
        return QLocale::c().toString(col->valueAt(r), 'g', QLocale::FloatingPointShortest);
    return tbl->text(r, c);
}
} // namespace

bool Table::findNextCell(const QString &what, bool matchCase, bool wholeCell)
{
    if (what.isEmpty() || numRows() <= 0 || numCols() <= 0)
        return false;
    int curRow = 0, curCol = -1;
    getCurrentCell(&curRow, &curCol);
    if (curRow < 0)
        curRow = 0;
    const bool formulaMode = formulaModeActive();
    const int rows = numRows(), cols = numCols();
    const long total = static_cast<long>(rows) * cols;
    long start = static_cast<long>(curRow) * cols + qMax(curCol, -1);
    for (long k = 1; k <= total; ++k) {
        const long pos = (start + k) % total;
        const int r = static_cast<int>(pos / cols);
        const int c = static_cast<int>(pos % cols);
        Column *col = column(c);
        if (!col || (!formulaMode && col->isInvalid(r)))
            continue;
        const QString raw = cellRawEditString(this, col, r, c, formulaMode);
        const QString disp = formulaMode ? raw : text(r, c);
        if (cellTextMatches(raw, what, matchCase, wholeCell)
            || (!disp.isEmpty() && disp != raw
                && cellTextMatches(disp, what, matchCase, wholeCell))) {
            deselectAll();
            goToCell(r, c);
            setCellSelected(r, c, true);
            return true;
        }
    }
    return false;
}

int Table::replaceAllCells(const QString &what, const QString &with, bool matchCase,
                           bool wholeCell, bool selectionOnly)
{
    if (what.isEmpty() || !d_future_table)
        return 0;
    registerTableWorksheetTranslations();
    const Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const bool formulaMode = formulaModeActive();

    struct PlannedEdit
    {
        int col = 0;
        int row = 0;
        QString nextVal;
        double numericVal = 0.0;
        bool isEmptyClear = false;
        bool isNumericValid = false;
    };
    QVector<PlannedEdit> edits;
    QSet<int> nonNumericConflictCols;

    for (int c = 0; c < numCols(); ++c) {
        Column *col = column(c);
        if (!col)
            continue;
        for (int r = 0; r < numRows(); ++r) {
            if ((!formulaMode && col->isInvalid(r)) || (selectionOnly && !isCellSelected(r, c)))
                continue;
            const QString old = cellRawEditString(this, col, r, c, formulaMode);
            if (!cellTextMatches(old, what, matchCase, wholeCell))
                continue;
            QString now = old;
            if (wholeCell)
                now = with;
            else
                now.replace(what, with, cs);
            if (now == old)
                continue;
            PlannedEdit pe;
            pe.col = c;
            pe.row = r;
            pe.nextVal = now;
            pe.isEmptyClear = now.trimmed().isEmpty()
                    || (!formulaMode && col->columnMode() != SciDAVis::ColumnMode::Text
                        && AsciiTableImportFilter::isMissingDataToken(now));
            if (!formulaMode && col->columnMode() == SciDAVis::ColumnMode::Numeric
                && !pe.isEmptyClear) {
                double dv = 0.0;
                if (parseTableDoubleToken(now, dv) && !std::isnan(dv)) {
                    pe.isNumericValid = true;
                    pe.numericVal = dv;
                } else {
                    nonNumericConflictCols.insert(c);
                }
            }
            edits.append(pe);
        }
    }

    if (edits.isEmpty())
        return 0;

    QSet<int> colsToConvertToText;
    if (!nonNumericConflictCols.isEmpty() && !formulaMode) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox box(QMessageBox::Warning, tr("Convert Numeric Column to Text?"),
                            tr("The replacement text is not numeric for %n column(s).\n"
                               "Convert those column(s) to Text so the replaced text is preserved?",
                               "", nonNumericConflictCols.size()),
                            QMessageBox::NoButton, this);
            QPushButton *convertBtn =
                    box.addButton(tr("Convert to Text"), QMessageBox::AcceptRole);
            box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(convertBtn);
            QPointer<Table> guard(this);
            box.exec();
            if (!guard || !d_future_table)
                return 0;
            if (box.clickedButton() != convertBtn)
                return 0;
            colsToConvertToText = nonNumericConflictCols;
        } else {
            for (int c : nonNumericConflictCols) {
                Column *col = column(c);
                if (!col)
                    continue;
                QSet<int> targetedRows;
                for (const PlannedEdit &pe : edits) {
                    if (pe.col == c)
                        targetedRows.insert(pe.row);
                }
                bool otherNumbers = false;
                for (int r = 0; r < col->rowCount() && !otherNumbers; ++r) {
                    if (!targetedRows.contains(r) && !col->isInvalid(r))
                        otherNumbers = true;
                }
                if (!otherNumbers && col->formulaIntervals().isEmpty())
                    colsToConvertToText.insert(c);
            }
            QVector<PlannedEdit> filtered;
            filtered.reserve(edits.size());
            for (const PlannedEdit &pe : edits) {
                if (nonNumericConflictCols.contains(pe.col) && !colsToConvertToText.contains(pe.col)
                    && !pe.isEmptyClear && !pe.isNumericValid) {
                    continue;
                }
                filtered.append(pe);
            }
            edits = filtered;
            if (edits.isEmpty())
                return 0;
        }
    }

    int replaced = 0;
    d_future_table->beginMacro(tr("%1: replace").arg(name()));
    for (int c : colsToConvertToText) {
        if (Column *col = column(c))
            col->setColumnMode(SciDAVis::ColumnMode::Text);
    }
    for (const PlannedEdit &pe : edits) {
        Column *col = column(pe.col);
        if (!col)
            continue;
        if (formulaMode) {
            col->setFormula(pe.row, pe.nextVal);
            col->setInvalid(pe.row, false);
        } else if (pe.isEmptyClear) {
            clearCell(pe.row, pe.col);
        } else if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
            col->setValueAt(pe.row, pe.numericVal);
            col->setInvalid(pe.row, false);
        } else if (col->columnMode() == SciDAVis::ColumnMode::Text) {
            col->setTextAt(pe.row, pe.nextVal);
            col->setInvalid(pe.row, false);
        } else {
            col->asStringColumn()->setTextAt(pe.row, pe.nextVal);
        }
        ++replaced;
    }
    d_future_table->endMacro();
    return replaced;
}

void Table::showFindReplaceDialog(bool replace)
{
    registerTableWorksheetTranslations();
    if (!d_find_replace_dialog) {
        QDialog *dlg = new QDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        QFormLayout *form = new QFormLayout();
        QLineEdit *findEdit = new QLineEdit(dlg);
        QLabel *replaceLabel = new QLabel(tr("Replace with:"), dlg);
        QLineEdit *replaceEdit = new QLineEdit(dlg);
        form->addRow(tr("Find:"), findEdit);
        form->addRow(replaceLabel, replaceEdit);
        QCheckBox *caseBox = new QCheckBox(tr("Match case"), dlg);
        QCheckBox *wholeBox = new QCheckBox(tr("Whole cell"), dlg);
        QCheckBox *selBox = new QCheckBox(tr("Selected cells only"), dlg);
        QLabel *status = new QLabel(dlg);
        QDialogButtonBox *buttons = new QDialogButtonBox(dlg);
        QPushButton *findBtn = buttons->addButton(tr("Find Next"), QDialogButtonBox::ActionRole);
        QPushButton *replaceOneBtn = buttons->addButton(tr("Replace"), QDialogButtonBox::ActionRole);
        QPushButton *replaceAllBtn =
                buttons->addButton(tr("Replace All"), QDialogButtonBox::ActionRole);
        buttons->addButton(QDialogButtonBox::Close);
        findBtn->setDefault(true);

        QVBoxLayout *vl = new QVBoxLayout(dlg);
        vl->addLayout(form);
        vl->addWidget(caseBox);
        vl->addWidget(wholeBox);
        vl->addWidget(selBox);
        vl->addWidget(status);
        vl->addWidget(buttons);

        d_find_replace_dialog = dlg;
        d_fr_find_edit = findEdit;
        d_fr_replace_label = replaceLabel;
        d_fr_replace_edit = replaceEdit;
        d_fr_case_box = caseBox;
        d_fr_whole_box = wholeBox;
        d_fr_sel_box = selBox;
        d_fr_status_label = status;
        d_fr_replace_one_btn = replaceOneBtn;
        d_fr_replace_all_btn = replaceAllBtn;

        connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
        connect(findBtn, &QPushButton::clicked, dlg, [this]() {
            if (!d_fr_find_edit || !d_fr_status_label)
                return;
            d_fr_status_label->setText(findNextCell(d_fr_find_edit->text(),
                                                    d_fr_case_box->isChecked(),
                                                    d_fr_whole_box->isChecked())
                                               ? QString()
                                               : tr("Not found."));
        });
        connect(replaceOneBtn, &QPushButton::clicked, dlg, [this]() {
            if (!d_fr_find_edit || !d_fr_replace_edit || !d_fr_status_label)
                return;
            const QString what = d_fr_find_edit->text();
            if (what.isEmpty())
                return;
            const bool matchCase = d_fr_case_box->isChecked();
            const bool wholeCell = d_fr_whole_box->isChecked();
            const bool formulaMode = formulaModeActive();
            int curRow = -1, curCol = -1;
            getCurrentCell(&curRow, &curCol);
            bool replacedCurrent = false;
            if (curRow >= 0 && curRow < numRows() && curCol >= 0 && curCol < numCols()) {
                Column *col = column(curCol);
                if (col && (formulaMode || !col->isInvalid(curRow))) {
                    const QString curRaw =
                            cellRawEditString(this, col, curRow, curCol, formulaMode);
                    if (cellTextMatches(curRaw, what, matchCase, wholeCell)) {
                        const Qt::CaseSensitivity cs =
                                matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
                        const QString nextStr = wholeCell
                                ? d_fr_replace_edit->text()
                                : QString(curRaw).replace(what, d_fr_replace_edit->text(), cs);
                        const bool isEmptyClear = nextStr.trimmed().isEmpty()
                                || (!formulaMode && col->columnMode() != SciDAVis::ColumnMode::Text
                                    && AsciiTableImportFilter::isMissingDataToken(nextStr));
                        double dv = 0.0;
                        bool convertColToText = false;
                        bool canApply = true;
                        if (!formulaMode && col->columnMode() == SciDAVis::ColumnMode::Numeric
                            && !isEmptyClear) {
                            if (!parseTableDoubleToken(nextStr, dv) || std::isnan(dv)) {
                                if (!ApplicationWindow::isNonInteractiveContext(this)) {
                                    QMessageBox box(
                                            QMessageBox::Warning,
                                            tr("Convert Numeric Column to Text?"),
                                            tr("The replacement text is not numeric for %n "
                                               "column(s).\nConvert those column(s) to Text so the "
                                               "replaced text is preserved?",
                                               "", 1),
                                            QMessageBox::NoButton, this);
                                    QPushButton *convertBtn = box.addButton(
                                            tr("Convert to Text"), QMessageBox::AcceptRole);
                                    box.addButton(QMessageBox::Cancel);
                                    box.setDefaultButton(convertBtn);
                                    QPointer<Table> guard(this);
                                    box.exec();
                                    if (!guard || !d_future_table)
                                        return;
                                    col = column(curCol);
                                    if (!col)
                                        return;
                                    if (box.clickedButton() != convertBtn)
                                        canApply = false;
                                    else
                                        convertColToText = true;
                                } else {
                                    canApply = false;
                                }
                            }
                        }
                        if (canApply && nextStr != curRaw) {
                            d_future_table->beginMacro(tr("%1: replace").arg(name()));
                            if (convertColToText)
                                col->setColumnMode(SciDAVis::ColumnMode::Text);
                            if (formulaMode) {
                                col->setFormula(curRow, nextStr);
                                col->setInvalid(curRow, false);
                            } else if (isEmptyClear) {
                                clearCell(curRow, curCol);
                            } else if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                                col->setValueAt(curRow, dv);
                                col->setInvalid(curRow, false);
                            } else if (col->columnMode() == SciDAVis::ColumnMode::Text) {
                                col->setTextAt(curRow, nextStr);
                                col->setInvalid(curRow, false);
                            } else {
                                col->asStringColumn()->setTextAt(curRow, nextStr);
                            }
                            d_future_table->endMacro();
                            replacedCurrent = true;
                        }
                    }
                }
            }
            const bool foundNext = findNextCell(what, matchCase, wholeCell);
            if (!replacedCurrent && !foundNext)
                d_fr_status_label->setText(tr("Not found."));
            else if (replacedCurrent)
                d_fr_status_label->setText(tr("%n cell(s) replaced.", "", 1));
            else
                d_fr_status_label->setText(QString());
        });
        connect(replaceAllBtn, &QPushButton::clicked, dlg, [this]() {
            if (!d_fr_find_edit || !d_fr_replace_edit || !d_fr_status_label)
                return;
            QPointer<Table> guard(this);
            const int n = replaceAllCells(d_fr_find_edit->text(), d_fr_replace_edit->text(),
                                          d_fr_case_box->isChecked(), d_fr_whole_box->isChecked(),
                                          d_fr_sel_box->isChecked());
            if (!guard || !d_fr_status_label)
                return;
            d_fr_status_label->setText(tr("%n cell(s) replaced.", "", n));
        });
    }

    d_find_replace_dialog->setWindowTitle(
            replace ? tr("Find and Replace in Worksheet") : tr("Find in Worksheet"));
    if (d_fr_replace_label)
        d_fr_replace_label->setVisible(replace);
    if (d_fr_replace_edit)
        d_fr_replace_edit->setVisible(replace);
    if (d_fr_sel_box)
        d_fr_sel_box->setVisible(replace);
    if (d_fr_replace_one_btn)
        d_fr_replace_one_btn->setVisible(replace);
    if (d_fr_replace_all_btn)
        d_fr_replace_all_btn->setVisible(replace);
    if (d_fr_status_label)
        d_fr_status_label->clear();

    d_find_replace_dialog->show();
    d_find_replace_dialog->raise();
    d_find_replace_dialog->activateWindow();
    if (d_fr_find_edit) {
        d_fr_find_edit->setFocus();
        d_fr_find_edit->selectAll();
    }
}

void Table::removeCol()
{
    if (d_future_table)
        d_future_table->removeSelectedColumns();
}

void Table::removeCol(const QStringList &list)
{
    if (d_future_table && !list.isEmpty()) {
        d_future_table->beginMacro(tr("%1: remove %2 column(s)").arg(name()).arg(list.size()));
        foreach (QString colLabelName, list) {
            const int idx = colIndex(colLabelName);
            if (idx >= 0)
                d_future_table->removeColumns(idx, 1);
        }
        d_future_table->endMacro();
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
    Column *colPtr = column(col);
    if (!colPtr)
        return;
    if (d_future_table)
        d_future_table->beginMacro(tr("%1: change cell value(s)").arg(name()));
    colPtr->setValueAt(row, val);
    if (d_future_table)
        d_future_table->endMacro();
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
    Column *colPtr = column(col);
    if (!colPtr)
        return;
    if (d_future_table)
        d_future_table->beginMacro(tr("%1: change cell value(s)").arg(name()));
    colPtr->asStringColumn()->setTextAt(row, text);
    if (d_future_table)
        d_future_table->endMacro();
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
    return d_future_table ? d_future_table->columnIndex(column(name)) : -1;
}

bool Table::noXColumn()
{
    return d_future_table ? d_future_table->columnCount(SciDAVis::X) == 0 : true;
}

bool Table::noYColumn()
{
    return d_future_table ? d_future_table->columnCount(SciDAVis::Y) == 0 : true;
}

QString Table::quoteCsvField(const QString &raw, const QString &separator)
{
    // A field must be quoted when it contains any non-blank character of the separator, not
    // only the separator as a whole: with ", " or ";\t" a cell "Al,Zn" would otherwise be
    // written bare and readers that split on ',' (Excel) would break the row.
    bool hasSepChar = false;
    for (const QChar ch : separator) {
        if (!ch.isSpace() && raw.contains(ch)) {
            hasSepChar = true;
            break;
        }
    }
    const bool needsQuotes = (!separator.isEmpty() && raw.contains(separator)) || hasSepChar
            || raw.contains(QLatin1Char('"')) || raw.contains(QLatin1Char('\n'))
            || raw.contains(QLatin1Char('\r'));
    if (!needsQuotes)
        return raw;
    QString escaped = raw;
    escaped.replace(QLatin1String("\""), QLatin1String("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

bool Table::exportASCII(const QString &fname, const QString &separator, bool withLabels,
                        bool exportSelection)
{
    return exportASCII(fname, separator, withLabels, exportSelection, AsciiExportOptions());
}

bool Table::exportASCII(const QString &fname, const QString &separator, bool withLabels,
                        bool exportSelection, const AsciiExportOptions &options)
{
    registerTableWorksheetTranslations();
    d_last_export_error.clear();
    auto fail = [this, &fname](const QString &reason) -> bool {
        d_last_export_error = reason;
        QApplication::restoreOverrideCursor();
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::critical(0, tr("ASCII Export Error"),
                                  tr("Could not write to file: <br><h4>") + fname.toHtmlEscaped()
                                          + QStringLiteral("</h4><p>") + reason.toHtmlEscaped());
        }
        return false;
    };

    // QSaveFile: the old file is only replaced once everything was written successfully.
    QSaveFile file(fname);
    if (!file.open(QIODevice::WriteOnly))
        return fail(tr("Please verify that you have the right to write to this location!")
                    + QStringLiteral(" (") + file.errorString() + QStringLiteral(")"));

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(options.utf8Bom);

    auto quoteField = [&separator](const QString &raw) -> QString {
        return quoteCsvField(raw, separator);
    };

    QLocale cLoc = QLocale::c();
    cLoc.setNumberOptions(QLocale::OmitGroupSeparator);

    const int rows = numRows();
    const int cols = numCols();
    QList<int> colIdx;
    int topRow = 0, bottomRow = rows - 1;
    if (exportSelection) {
        for (int i = 0; i < cols; i++) {
            if (isColumnSelected(i))
                colIdx << i;
        }
        topRow = firstSelectedRow();
        bottomRow = lastSelectedRow();
    } else {
        for (int i = 0; i < cols; i++)
            colIdx << i;
    }
    if (exportSelection && (colIdx.isEmpty() || topRow < 0 || bottomRow < topRow)) {
        // Nothing selected: do not leave an empty file behind (QSaveFile discards on return).
        return fail(tr("No cells are selected. Select the cells to export first."));
    }

    if (withLabels && !colIdx.isEmpty()) {
        QStringList header = colNames();
        const bool anyNonDigitName = !header.filter(QRegExp("\\D")).isEmpty();
        QStringList nameRow, unitRow;
        bool anyUnit = false;
        for (int c : colIdx) {
            Column *col = column(c);
            QString label = anyNonDigitName ? header[c] : QStringLiteral("C") + header[c];
            if (options.longNameAndUnits && col && !col->longName().trimmed().isEmpty())
                label = col->longName().trimmed();
            nameRow << quoteField(label);
            const QString unit = col ? col->units().trimmed() : QString();
            if (!unit.isEmpty())
                anyUnit = true;
            unitRow << quoteField(unit);
        }
        out << nameRow.join(separator) << "\n";
        if (options.longNameAndUnits && anyUnit)
            out << unitRow.join(separator) << "\n";
    }

    QList<Column *> col_ptrs;
    for (int c : colIdx)
        col_ptrs << column(c);

    for (int i = topRow; i <= bottomRow && i >= 0; i++) {
        bool first = true;
        foreach (Column *col, col_ptrs) {
            if (first)
                first = false;
            else
                out << separator;
            if (!col || col->isInvalid(i)) {
                continue;
            }
            if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                const double val = col->valueAt(i);
                // shortest text that reads back as exactly the same double (up to 17 digits)
                if (std::isfinite(val))
                    out << quoteField(cLoc.toString(val, 'g', QLocale::FloatingPointShortest));
            } else {
                out << quoteField(col->asStringColumn()->textAt(i));
            }
        }
        out << "\n";
    }

    out.flush();
    if (out.status() != QTextStream::Ok || file.error() != QFileDevice::NoError)
        return fail(tr("Writing failed (%1). The disk may be full or the file may be in use.")
                            .arg(file.errorString()));
    if (!file.commit())
        return fail(tr("Writing failed (%1). The disk may be full or the file may be in use.")
                            .arg(file.errorString()));
    return true;
}

void Table::customEvent(QEvent *e)
{
    if (e->type() == SCRIPTING_CHANGE_EVENT)
        scriptingChangeEvent((ScriptingChangeEvent *)e);
}

void Table::closeEvent(QCloseEvent *e)
{
    auto acceptAndRemove = [this, e]() {
        e->accept();
        QPointer<Table> self(this);
        if (d_future_table)
            d_future_table->remove();
        if (!self)
            return;
        const bool ownedByUndo =
                d_future_table && d_future_table->property("_tg_owned_by_undo_cmd").toBool();
        if (!ownedByUndo)
            self->deleteLater();
    };
    if (askOnClose && !ApplicationWindow::isNonInteractiveContext(this)) {
        switch (QMessageBox::information(this, tr("ToriGraph"),
                                         tr("Do you want to hide or delete") + "<p><b>'"
                                                 + objectName() + "'</b> ?",
                                         tr("Delete"), tr("Hide"), tr("Cancel"), 1, 2)) {
        case 0:
            acceptAndRemove();
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
        acceptAndRemove();
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
    d_auto_recalc_formulas = m->d_auto_recalc_formulas;
    d_future_table->setAutoRecalcFormulas(d_auto_recalc_formulas);
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
    QString lnLine = QStringLiteral("LongNames");
    QString unLine = QStringLiteral("Units");
    for (int i = 0; i < numCols(); ++i) {
        QString ln = colLongName(i);
        ln.replace('\t', ' ').replace('\n', ' ');
        QString un = colUnits(i);
        un.replace('\t', ' ').replace('\n', ' ');
        lnLine += "\t" + ln;
        unLine += "\t" + un;
    }
    s += lnLine + "\n";
    s += unLine + "\n";
    s += QStringLiteral("AutoRecalc\t%1\n").arg(d_auto_recalc_formulas ? 1 : 0);
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

    bool restoredAutoRecalc = prevAutoRecalc;
    bool anyMetadata = false;
    for (; iterator != list_in.end(); ++iterator) {
        const QString line = *iterator;
        if (line.trimmed() == "</table>")
            break;
        if (line.startsWith("LongNames\t") || line == "LongNames") {
            const QStringList parts = line.split("\t");
            for (int i = 1; i < parts.size() && (i - 1) < numCols(); ++i) {
                setColLongName(i - 1, parts[i]);
                if (!parts[i].trimmed().isEmpty())
                    anyMetadata = true;
            }
        } else if (line.startsWith("Units\t") || line == "Units") {
            const QStringList parts = line.split("\t");
            for (int i = 1; i < parts.size() && (i - 1) < numCols(); ++i) {
                setColUnits(i - 1, parts[i]);
                if (!parts[i].trimmed().isEmpty())
                    anyMetadata = true;
            }
        } else if (line.startsWith("AutoRecalc\t")) {
            const QStringList parts = line.split("\t");
            if (parts.size() >= 2)
                restoredAutoRecalc = (parts[1].trimmed().toInt() != 0);
        }
    }

    if (anyMetadata)
        showComments(true);

    d_auto_recalc_formulas = restoredAutoRecalc;
    if (d_future_table)
        d_future_table->setAutoRecalcFormulas(restoredAutoRecalc);
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
    Column *c = column(col);
    return c ? c->name() : QString();
}

SciDAVis::PlotDesignation Table::colPlotDesignation(int col)
{
    Column *c = column(col);
    return c ? c->plotDesignation() : SciDAVis::noDesignation;
}

void Table::setColPlotDesignation(int col, SciDAVis::PlotDesignation d)
{
    if (Column *c = column(col))
        c->setPlotDesignation(d);
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
    Column *c = column(col);
    return c ? c->columnMode() : SciDAVis::ColumnMode::Numeric;
}

void Table::setColumnTypes(QList<SciDAVis::ColumnMode> ctl)
{
    if (!d_future_table)
        return;
    const int n = qMin(ctl.size(), d_future_table->columnCount());
    for (int i = 0; i < n; i++) {
        if (Column *c = column(i))
            c->setColumnMode(ctl.at(i));
    }
}

void Table::setColumnType(int col, SciDAVis::ColumnMode mode)
{
    if (Column *c = column(col))
        c->setColumnMode(mode);
}

QString Table::columnFormat(int col)
{
    // TODO: obsolete, remove in 0.3.0
    Column *col_ptr = column(col);
    if (!col_ptr)
        return QString();
    if (col_ptr->columnMode() != SciDAVis::ColumnMode::DateTime && col_ptr->columnMode() != SciDAVis::ColumnMode::Month
        && col_ptr->columnMode() != SciDAVis::ColumnMode::Day)
        return QString();

    DateTime2StringFilter *filter = static_cast<DateTime2StringFilter *>(col_ptr->outputFilter());
    return filter ? filter->format() : QString();
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
    // Worksheet keys: Ctrl+F = find, Ctrl+H = replace, Ctrl+D = fill down. Ctrl+H and Ctrl+D
    // are also application shortcuts (Ctrl+H = help, Ctrl+D = data reader in graphs), and a
    // Ctrl+F menu shortcut may be assigned again later (New Function Plot is Ctrl+Shift+F now).
    // While the worksheet has the focus these keys go to the worksheet; the application
    // shortcuts themselves are unchanged.
    if (event->type() == QEvent::ShortcutOverride
        && (watched == this || watched == d_view_widget
            || (d_view_widget && watched == d_view_widget->viewport()))) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        const Qt::KeyboardModifiers mods =
                ke->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
        if (mods == Qt::ControlModifier
            && (ke->key() == Qt::Key_F || ke->key() == Qt::Key_H || ke->key() == Qt::Key_D)) {
            event->accept();
            return true;
        }
    }
    if (event->type() == QEvent::KeyPress
        && (watched == this || watched == d_view_widget
            || (d_view_widget && watched == d_view_widget->viewport()))) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        const Qt::KeyboardModifiers mods =
                ke->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
        if (mods == Qt::ControlModifier && ke->key() == Qt::Key_D) {
            fillSelectionDown();
            return true;
        }
        if (mods == Qt::ControlModifier && (ke->key() == Qt::Key_F || ke->key() == Qt::Key_H)) {
            showFindReplaceDialog(ke->key() == Qt::Key_H);
            return true;
        }
        if (ke->key() == Qt::Key_Backspace && mods == Qt::NoModifier && !formulaModeActive()) {
            // Excel/Origin: Backspace starts editing the active cell with its content erased
            // (Esc restores it); Delete clears the whole selection.
            if (d_view_widget) {
                const QModelIndex idx = d_view_widget->currentIndex();
                if (idx.isValid()) {
                    d_view_widget->edit(idx);
                    if (QLineEdit *le = qobject_cast<QLineEdit *>(QApplication::focusWidget()))
                        le->clear();
                    return true;
                }
            }
        }
    }
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
        const bool noCtrlAlt =
                ((ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) == 0);
        if (noCtrlAlt && (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)) {
            const bool shiftDown = (ke->modifiers() & Qt::ShiftModifier) != 0;
            const QModelIndexList selIdxs = (d_view_widget && d_view_widget->selectionModel())
                    ? d_view_widget->selectionModel()->selectedIndexes()
                    : QModelIndexList();
            if (selIdxs.size() >= 2 && d_view_widget) {
                QModelIndexList sorted = selIdxs;
                std::sort(sorted.begin(), sorted.end(),
                          [](const QModelIndex &a, const QModelIndex &b) {
                              return (a.column() != b.column()) ? (a.column() < b.column())
                                                                : (a.row() < b.row());
                          });
                const int curPos = sorted.indexOf(d_view_widget->currentIndex());
                const int delta = shiftDown ? -1 : 1;
                const int nextPos = (curPos < 0)
                        ? (shiftDown ? sorted.size() - 1 : 0)
                        : ((curPos + delta + sorted.size()) % sorted.size());
                d_view_widget->selectionModel()->setCurrentIndex(sorted.at(nextPos),
                                                                 QItemSelectionModel::Current);
                d_view_widget->scrollTo(sorted.at(nextPos), QAbstractItemView::EnsureVisible);
                return true;
            }
            int curRow = -1, curCol = -1;
            getCurrentCell(&curRow, &curCol);
            if (curRow < 0 || curCol < 0 || !isCellSelected(curRow, curCol)) {
                int selRow = shiftDown ? firstSelectedRow(false) : lastSelectedRow(false);
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
            int nextRow = shiftDown ? qMax(0, curRow - 1) : (curRow + 1);
            if (!shiftDown && nextRow >= numRows())
                setNumRows(nextRow + 1);
            deselectAll();
            goToCell(nextRow, curCol);
            setCellSelected(nextRow, curCol, true);
            return true;
        }
        const bool isBacktab = (ke->key() == Qt::Key_Backtab)
                || (ke->key() == Qt::Key_Tab && (ke->modifiers() & Qt::ShiftModifier));
        const bool isForwardTab =
                (ke->key() == Qt::Key_Tab && !(ke->modifiers() & Qt::ShiftModifier));
        if (noCtrlAlt && (isForwardTab || isBacktab)) {
            const QModelIndexList selIdxs = (d_view_widget && d_view_widget->selectionModel())
                    ? d_view_widget->selectionModel()->selectedIndexes()
                    : QModelIndexList();
            if (selIdxs.size() >= 2 && d_view_widget) {
                QModelIndexList sorted = selIdxs;
                std::sort(sorted.begin(), sorted.end(),
                          [](const QModelIndex &a, const QModelIndex &b) {
                              return (a.row() != b.row()) ? (a.row() < b.row())
                                                          : (a.column() < b.column());
                          });
                const int curPos = sorted.indexOf(d_view_widget->currentIndex());
                const int delta = isBacktab ? -1 : 1;
                const int nextPos = (curPos < 0)
                        ? (isBacktab ? sorted.size() - 1 : 0)
                        : ((curPos + delta + sorted.size()) % sorted.size());
                d_view_widget->selectionModel()->setCurrentIndex(sorted.at(nextPos),
                                                                 QItemSelectionModel::Current);
                d_view_widget->scrollTo(sorted.at(nextPos), QAbstractItemView::EnsureVisible);
                return true;
            }
            int curRow = -1, curCol = -1;
            getCurrentCell(&curRow, &curCol);
            if (curRow < 0 || curCol < 0 || !isCellSelected(curRow, curCol)) {
                int selRow = firstSelectedRow(false);
                int selCol = isBacktab ? firstSelectedColumn(false) : lastSelectedColumn(false);
                if (selRow >= 0 && selCol >= 0) {
                    curRow = selRow;
                    curCol = selCol;
                }
            }
            if (curRow < 0)
                curRow = 0;
            if (curCol < 0)
                curCol = 0;
            if (numCols() > 0 && numRows() > 0) {
                int nextRow = curRow;
                int nextCol = curCol;
                if (isBacktab) {
                    if (curCol > 0) {
                        nextCol = curCol - 1;
                    } else {
                        nextCol = numCols() - 1;
                        nextRow = (curRow > 0) ? (curRow - 1) : (numRows() - 1);
                    }
                } else {
                    if (curCol + 1 < numCols()) {
                        nextCol = curCol + 1;
                    } else {
                        nextCol = 0;
                        nextRow = (curRow + 1 < numRows()) ? (curRow + 1) : 0;
                    }
                }
                deselectAll();
                goToCell(nextRow, nextCol);
                setCellSelected(nextRow, nextCol, true);
            }
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonDblClick
               && (watched == d_horizontal_header
                   || (d_horizontal_header && watched == d_horizontal_header->viewport()))) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (d_horizontal_header && d_horizontal_header->handleSubHeaderDoubleClick(me->pos()))
            return true;
        int col = d_horizontal_header ? d_horizontal_header->logicalIndexAt(me->pos()) : -1;
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
    Column *c = column(col);
    return c ? c->comment() : QString();
}

QStringList Table::colComments()
{
    QStringList list;
    if (d_future_table) {
        for (int i = 0; i < d_future_table->columnCount(); i++) {
            Column *c = column(i);
            list << (c ? c->comment() : QString());
        }
    }
    return list;
}

void Table::setColComments(const QStringList &list)
{
    if (d_future_table) {
        const int n = qMin(list.size(), d_future_table->columnCount());
        for (int i = 0; i < n; i++) {
            if (Column *c = column(i))
                c->setComment(list.at(i));
        }
    }
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

void Table::syncAutoRecalcFromFuture(bool on)
{
    d_auto_recalc_formulas = on;
}

void Table::setAutoRecalcFormulas(bool on)
{
    d_auto_recalc_formulas = on;
    if (d_future_table)
        d_future_table->setAutoRecalcFormulas(on);
    if (d_auto_recalc_formulas && !d_in_auto_recalc && d_future_table)
        autoRecalculateDependentColumns(-1, 0);
}

bool Table::autoRecalcFormulas() const
{
    return d_auto_recalc_formulas;
}

void Table::showColumnPropertiesDialog(int col)
{
    if (!d_future_table || numCols() <= 0 || ApplicationWindow::isNonInteractiveContext(this))
        return;
    if (col < 0 || col >= numCols())
        col = firstSelectedColumn(false);
    if (col < 0 || col >= numCols()) {
        if (d_future_table->currentColumn())
            col = d_future_table->columnIndex(d_future_table->currentColumn());
    }
    if (col < 0 || col >= numCols())
        col = 0;

    QPointer<QWidget> lastMetaEdit;
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
    QGroupBox *grpMeta = new QGroupBox(tr("Column Labels && Designation"), &dlg);
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
    QPushButton *btnFormatPanel = new QPushButton(tr("Type && Format..."), grpMeta);
    typeRowLay->addWidget(boxDataType, 1);
    typeRowLay->addWidget(btnFormatPanel);

    QPlainTextEdit *editComments = new QPlainTextEdit(grpMeta);
    editComments->setFixedHeight(56);

    QWidget *symBar = new QWidget(grpMeta);
    lastMetaEdit = editLongName;
    {
        QHBoxLayout *symLay = new QHBoxLayout(symBar);
        symLay->setContentsMargins(0, 0, 0, 0);
        symLay->setSpacing(2);
        auto trackFocus = [&](QWidget *w) {
            QObject::connect(qApp, &QApplication::focusChanged, symBar,
                             [w, &lastMetaEdit](QWidget *, QWidget *now) {
                                 if (now == w)
                                     lastMetaEdit = w;
                             });
        };
        trackFocus(editLongName);
        trackFocus(editUnits);
        trackFocus(editComments);

        auto toUniChar = [](QChar ch, bool isSup) -> QChar {
            if (isSup) {
                switch (ch.unicode()) {
                case '0': return QChar(0x2070);
                case '1': return QChar(0x00B9);
                case '2': return QChar(0x00B2);
                case '3': return QChar(0x00B3);
                case '4': return QChar(0x2074);
                case '5': return QChar(0x2075);
                case '6': return QChar(0x2076);
                case '7': return QChar(0x2077);
                case '8': return QChar(0x2078);
                case '9': return QChar(0x2079);
                case '+': return QChar(0x207A);
                case '-': return QChar(0x207B);
                default: return ch;
                }
            }
            switch (ch.unicode()) {
            case '0': return QChar(0x2080);
            case '1': return QChar(0x2081);
            case '2': return QChar(0x2082);
            case '3': return QChar(0x2083);
            case '4': return QChar(0x2084);
            case '5': return QChar(0x2085);
            case '6': return QChar(0x2086);
            case '7': return QChar(0x2087);
            case '8': return QChar(0x2088);
            case '9': return QChar(0x2089);
            case '+': return QChar(0x208A);
            case '-': return QChar(0x208B);
            default: return ch;
            }
        };

        struct DlgSymSpec {
            const char *label;
            const char *insertText;
            int subSupMode;
            bool preferUnits;
        };
        static const DlgSymSpec kDlgSyms[] = {
            { "\xCF\x83", "\xCF\x83", 0, false },
            { "\xCE\xB5", "\xCE\xB5", 0, false },
            { "2\xCE\xB8", "2\xCE\xB8", 0, false },
            { "\xCE\xBCm", "\xCE\xBCm", 0, true },
            { "\xC2\xB0""C", "\xC2\xB0""C", 0, true },
            { "\xC3\x85", "\xC3\x85", 0, true },
            { "\xC2\xB1", "\xC2\xB1", 0, false },
            { "\xC2\xB0", "\xC2\xB0", 0, true },
            { "cm\xE2\x81\xBB\xC2\xB9", "cm\xE2\x81\xBB\xC2\xB9", 0, true },
            { "x\xE2\x82\x82", "", 1, false },
            { "x\xC2\xB2", "", 2, false },
        };
        for (const auto &sp : kDlgSyms) {
            QToolButton *tb = new QToolButton(symBar);
            tb->setText(QString::fromUtf8(sp.label));
            tb->setAutoRaise(true);
            tb->setFocusPolicy(Qt::NoFocus);
            tb->setFixedHeight(21);
            tb->setStyleSheet(QStringLiteral(
                    "QToolButton { border: 1px solid #CBD5E1; border-radius: 3px;"
                    " background: #F8FAFC; color: #1E293B; padding: 0px 4px; font-size: 8.5pt; }"
                    "QToolButton:hover { background: #DBEAFE; border-color: #60A5FA; }"));
            const QString ins = QString::fromUtf8(sp.insertText);
            const int mode = sp.subSupMode;
            const bool prefU = sp.preferUnits;
            connect(tb, &QToolButton::clicked, &dlg,
                    [editUnits, editLongName, &lastMetaEdit, ins, mode, prefU, toUniChar]() {
                        QWidget *target = lastMetaEdit ? lastMetaEdit.data()
                                                       : (prefU ? static_cast<QWidget *>(editUnits)
                                                                : static_cast<QWidget *>(editLongName));
                        if (QLineEdit *le = qobject_cast<QLineEdit *>(target)) {
                            if (mode == 1 || mode == 2) {
                                const bool isSup = (mode == 2);
                                if (le->hasSelectedText()) {
                                    QString conv;
                                    for (QChar ch : le->selectedText())
                                        conv.append(toUniChar(ch, isSup));
                                    le->insert(conv);
                                } else {
                                    le->insert(isSup ? QString::fromUtf8("\xC2\xB2")
                                                     : QString::fromUtf8("\xE2\x82\x82"));
                                }
                            } else {
                                le->insert(ins);
                            }
                            le->setFocus();
                        } else if (QPlainTextEdit *pe = qobject_cast<QPlainTextEdit *>(target)) {
                            QTextCursor tc = pe->textCursor();
                            if (mode == 1 || mode == 2) {
                                const bool isSup = (mode == 2);
                                if (tc.hasSelection()) {
                                    QString conv;
                                    for (QChar ch : tc.selectedText())
                                        conv.append(toUniChar(ch, isSup));
                                    tc.insertText(conv);
                                } else {
                                    tc.insertText(isSup ? QString::fromUtf8("\xC2\xB2")
                                                        : QString::fromUtf8("\xE2\x82\x82"));
                                }
                            } else {
                                tc.insertText(ins);
                            }
                            pe->setFocus();
                        }
                    });
            symLay->addWidget(tb);
        }
        symLay->addStretch(1);
    }

    metaForm->addRow(tr("Short Name:"), editShortName);
    metaForm->addRow(tr("Long Name:"), editLongName);
    metaForm->addRow(tr("Units:"), editUnits);
    metaForm->addRow(tr("Symbols:"), symBar);
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
    editFormula->setPlaceholderText(QStringLiteral("col(\"1\") * 2.0 + 1.0"));
    fLay->addWidget(editFormula);

    QLabel *lblFormulaError = new QLabel(grpFormula);
    lblFormulaError->setWordWrap(true);
    lblFormulaError->setStyleSheet(QStringLiteral("color: #B91C1C; font-size: 8.5pt;"));
    lblFormulaError->hide();
    fLay->addWidget(lblFormulaError);

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
        const QString errMsg = cp->formulaError();
        lblFormulaError->setText(errMsg);
        lblFormulaError->setVisible(!errMsg.isEmpty());
        loadingUi = false;
    };

    auto applyUiToColumn = [&]() -> bool {
        if (currentCol < 0 || currentCol >= numCols())
            return false;
        Column *cp = column(currentCol);
        if (!cp || !d_future_table)
            return false;

        registerTableWorksheetTranslations();
        d_auto_recalc_formulas = chkAutoRecalc->isChecked();
        d_future_table->setAutoRecalcFormulas(d_auto_recalc_formulas);

        const QString newShort = editShortName->text().trimmed();
        const QString newLong = editLongName->text().trimmed();
        const QString newUnits = editUnits->text().trimmed();
        const QString newComment = editComments->toPlainText();
        const SciDAVis::PlotDesignation newPd =
                static_cast<SciDAVis::PlotDesignation>(boxDesignation->currentData().toInt());
        const SciDAVis::ColumnMode newMode =
                static_cast<SciDAVis::ColumnMode>(boxDataType->currentData().toInt());
        const QString newFx = stripLeadingFormulaEquals(editFormula->toPlainText());

        QString curFx;
        const QList<Interval<int>> curIvs = cp->formulaIntervals();
        for (const Interval<int> &iv : curIvs) {
            const QString f = cp->formula(iv.start()).trimmed();
            if (!f.isEmpty()) {
                curFx = f;
                break;
            }
        }
        if (curFx.isEmpty())
            curFx = cp->formula(0).trimmed();

        const bool shortChanged = (!newShort.isEmpty() && newShort != cp->name());
        const bool longChanged = (newLong != cp->longName());
        const bool unitsChanged = (newUnits != cp->units());
        const bool commentChanged = (newComment != cp->comment());
        const bool pdChanged = (newPd != cp->plotDesignation());
        const bool modeChanged = (newMode != cp->columnMode());
        const bool fxChanged = (newFx != curFx);

        if (!shortChanged && !longChanged && !unitsChanged && !commentChanged && !pdChanged
            && !modeChanged && !fxChanged) {
            return true;
        }

        d_future_table->beginMacro(tr("%1: modify column properties").arg(name()));
        if (shortChanged)
            cp->setName(newShort);
        if (longChanged)
            cp->setLongName(newLong);
        if (unitsChanged)
            cp->setUnits(newUnits);
        if (commentChanged)
            cp->setComment(newComment);
        if (pdChanged)
            cp->setPlotDesignation(newPd);
        if (modeChanged)
            cp->setColumnMode(newMode);

        if (fxChanged) {
            if (!newFx.isEmpty()) {
                if (cp->rowCount() < numRows())
                    cp->insertRows(cp->rowCount(), numRows() - cp->rowCount());
                const bool prevGuard = d_in_auto_recalc;
                d_in_auto_recalc = true;
                cp->setFormula(Interval<int>(0, qMax(0, numRows() - 1)), newFx);
                d_in_auto_recalc = prevGuard;
                evaluateCrossTableDag({ currentCol }, false);
                if (d_auto_recalc_formulas && !d_in_auto_recalc)
                    autoRecalculateDependentColumns(currentCol, 1);
            } else {
                cp->clearFormulas();
                setColumnFormulaError(currentCol, QString());
                if (d_auto_recalc_formulas && !d_in_auto_recalc)
                    autoRecalculateDependentColumns(currentCol, 1);
            }
        }
        d_future_table->endMacro();

        const QString errMsg = cp->formulaError();
        lblFormulaError->setText(errMsg);
        lblFormulaError->setVisible(!errMsg.isEmpty());

        if (!cp->longName().isEmpty() || !cp->units().isEmpty()
            || !cp->comment().isEmpty() || !newFx.isEmpty()) {
            showComments(true);
        }
        autoExpandColumnWidthForHeader(currentCol, false);
        setColumnForControlTabs(currentCol);
        d_future_table->setCurrentColumn(currentCol);
        rebuildCombos();

        if (!errMsg.isEmpty()) {
            reportFormulaErrorsToUser(currentCol);
            return false;
        }
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

    const QString formula = stripLeadingFormulaEquals(ui.formula_box->toPlainText());
    const int firstCol = firstSelectedColumn();
    const int lastCol = lastSelectedColumn();
    QVector<int> targetCols;
    if (firstCol >= 0 && lastCol >= firstCol) {
        const bool prevGuard = d_in_auto_recalc;
        d_in_auto_recalc = true;
        for (int col = firstCol; col <= lastCol; col++) {
            Column *col_ptr = column(col);
            if (!col_ptr)
                continue;
            if (formula.isEmpty()) {
                col_ptr->clearFormulas();
                setColumnFormulaError(col, QString());
            } else {
                if (col_ptr->rowCount() < rowCount())
                    col_ptr->insertRows(col_ptr->rowCount(), rowCount() - col_ptr->rowCount());
                col_ptr->setFormula(Interval<int>(0, qMax(0, rowCount() - 1)), formula);
                targetCols.push_back(col);
            }
        }
        d_in_auto_recalc = prevGuard;
        if (!targetCols.isEmpty()) {
            evaluateCrossTableDag(targetCols, false);
            reportFormulaErrorsToUser(targetCols.first());
        }
        if (d_auto_recalc_formulas && !d_in_auto_recalc)
            autoRecalculateDependentColumns(firstCol, lastCol - firstCol + 1);
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

namespace {

// The same reference forms as collectCrossTableRefs() (the formula dependency analysis).
const QRegularExpression &renameColRefRx()
{
    static const QRegularExpression rx(QStringLiteral(
            "(?<![A-Za-z0-9_])(col|column|column_|cell|cell_)\\s*\\(\\s*(\"((?:[^\"\\\\]|\\\\.)*)\"|[^,()]*)\\s*(,|\\))"));
    return rx;
}

const QRegularExpression &renameTableColRefRx()
{
    static const QRegularExpression rx(QStringLiteral(
            "(?<![A-Za-z0-9_])(tablecol|column__)\\s*\\(\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*,\\s*(\"((?:[^\"\\\\]|\\\\.)*)\"|[^,()]*)\\s*(,|\\))"));
    return rx;
}

QString unescapeFormulaPath(QString s)
{
    s.replace(QLatin1String("\\/"), QLatin1String("/"));
    s.replace(QLatin1String("\\\\"), QLatin1String("\\"));
    return s;
}

QString escapeFormulaName(QString s)
{
    s.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    return s;
}

// Text for an unquoted column argument col(A): kept unquoted when the new name is a plain
// identifier, quoted otherwise (col("new name") is read the same way).
QString bareColumnArgument(const QString &newCol)
{
    static const QRegularExpression ident(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (ident.match(newCol).hasMatch())
        return newCol;
    return QLatin1Char('"') + escapeFormulaName(newCol) + QLatin1Char('"');
}

struct FormulaEdit
{
    int start = 0;
    int length = 0;
    QString text;
};

QString applyFormulaEdits(const QString &formula, QVector<FormulaEdit> edits)
{
    if (edits.isEmpty())
        return formula;
    std::sort(edits.begin(), edits.end(),
              [](const FormulaEdit &a, const FormulaEdit &b) { return a.start > b.start; });
    QString out = formula;
    for (const FormulaEdit &e : edits)
        out.replace(e.start, e.length, e.text);
    return out;
}

// Edit for an unquoted argument captured as `capIdx` (it may carry surrounding blanks).
bool bareArgumentEdit(const QRegularExpressionMatch &m, int capIdx, const QString &oldCol,
                      const QString &newCol, FormulaEdit &edit)
{
    const QString raw = m.captured(capIdx);
    const QString arg = raw.trimmed();
    if (arg.isEmpty() || arg != oldCol)
        return false;
    bool isNum = false;
    arg.toInt(&isNum);
    if (isNum)
        return false; // a column number, not a name
    edit.start = m.capturedStart(capIdx) + raw.indexOf(arg);
    edit.length = arg.size();
    edit.text = bareColumnArgument(newCol);
    return true;
}

// Tables of the application, `self` first (also when it is not registered yet).
QList<Table *> renameTargetTables(ApplicationWindow *app, Table *self)
{
    QList<Table *> out;
    if (self)
        out.append(self);
    if (!app)
        return out;
    for (MyWidget *mw : app->windowsList()) {
        Table *t = qobject_cast<Table *>(mw);
        if (t && t != self && t->d_future_table)
            out.append(t);
    }
    return out;
}

} // namespace

QString Table::rewriteTableRefsInFormula(const QString &formula, const QString &oldTableName,
                                         const QString &newTableName)
{
    if (formula.isEmpty() || oldTableName.isEmpty() || newTableName.isEmpty()
        || oldTableName == newTableName)
        return formula;
    const QString escapedOld = QRegularExpression::escape(oldTableName);
    const QRegularExpression tableColRx(
            QStringLiteral("((?<![A-Za-z0-9_])(?:tablecol|column__)\\s*\\(\\s*\")") + escapedOld
            + QStringLiteral("(\"\\s*,)"));
    const QRegularExpression pathColRx(
            QStringLiteral("((?<![A-Za-z0-9_])(?:col|column|column_|cell|cell_)\\s*\\(\\s*\")")
            + escapedOld + QStringLiteral("(\\\\/|/)"));

    auto replaceWithCapture = [](const QString &src, const QRegularExpression &rx,
                                 const QString &midText) -> QString {
        QRegularExpressionMatchIterator it = rx.globalMatch(src);
        if (!it.hasNext())
            return src;
        QString out;
        int lastPos = 0;
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            out.append(src.mid(lastPos, m.capturedStart(0) - lastPos));
            out.append(m.captured(1));
            out.append(midText);
            out.append(m.captured(2));
            lastPos = m.capturedEnd(0);
        }
        out.append(src.mid(lastPos));
        return out;
    };

    QString out = replaceWithCapture(formula, tableColRx, newTableName);
    out = replaceWithCapture(out, pathColRx, newTableName);
    return out;
}

QString Table::rewriteColumnRefsInFormula(const QString &formula, const QString &formulaTable,
                                          const QString &targetTable, const QString &oldCol,
                                          const QString &newCol)
{
    if (formula.isEmpty() || targetTable.isEmpty() || oldCol.isEmpty() || newCol.isEmpty()
        || oldCol == newCol)
        return formula;

    QVector<FormulaEdit> edits;

    // col("A"), col("T/A"), col(A), cell("A", i), column("T/A"), ...
    QRegularExpressionMatchIterator it = renameColRefRx().globalMatch(formula);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (m.capturedStart(3) >= 0) {
            const QString raw = m.captured(3);
            const QString name = unescapeFormulaPath(raw);
            if (name.contains(QLatin1Char('/'))) {
                const QString tablePart = name.section(QLatin1Char('/'), 0, 0);
                const QString colPart = name.section(QLatin1Char('/'), 1);
                if (tablePart != targetTable || colPart != oldCol)
                    continue;
                // keep the table part (and its separator) as written, replace the column part
                const QString escapedOldTail = escapeFormulaName(oldCol);
                if (!raw.endsWith(escapedOldTail))
                    continue;
                FormulaEdit e;
                e.start = m.capturedStart(3) + raw.size() - escapedOldTail.size();
                e.length = escapedOldTail.size();
                e.text = escapeFormulaName(newCol);
                edits.push_back(e);
            } else {
                if (formulaTable != targetTable || name != oldCol)
                    continue;
                FormulaEdit e;
                e.start = m.capturedStart(3);
                e.length = raw.size();
                e.text = escapeFormulaName(newCol);
                edits.push_back(e);
            }
        } else if (formulaTable == targetTable) {
            FormulaEdit e;
            if (bareArgumentEdit(m, 2, oldCol, newCol, e))
                edits.push_back(e);
        }
    }

    // tablecol("T", "A"), tablecol("T", A), column__("T", ...)
    QRegularExpressionMatchIterator itTbl = renameTableColRefRx().globalMatch(formula);
    while (itTbl.hasNext()) {
        const QRegularExpressionMatch m = itTbl.next();
        QString tblName = m.captured(2);
        tblName.replace(QLatin1String("\\\\"), QLatin1String("\\"));
        if (tblName != targetTable)
            continue;
        if (m.capturedStart(4) >= 0) {
            QString colName = m.captured(4);
            colName.replace(QLatin1String("\\\\"), QLatin1String("\\"));
            if (colName != oldCol)
                continue;
            FormulaEdit e;
            e.start = m.capturedStart(4);
            e.length = m.captured(4).size();
            e.text = escapeFormulaName(newCol);
            edits.push_back(e);
        } else {
            FormulaEdit e;
            if (bareArgumentEdit(m, 3, oldCol, newCol, e))
                edits.push_back(e);
        }
    }

    return applyFormulaEdits(formula, edits);
}

void Table::updateCrossTableFormulaReferences(ApplicationWindow *app, const QString &oldTableName,
                                              const QString &newTableName)
{
    if (!app || oldTableName.isEmpty() || newTableName.isEmpty() || oldTableName == newTableName)
        return;
    const QList<Table *> tables = renameTargetTables(app, nullptr);
    for (Table *other : tables) {
        struct PendingUpdate
        {
            int col = 0;
            Interval<int> iv;
            QString formula;
        };
        QVector<PendingUpdate> updates;
        for (int c = 0; c < other->numCols(); ++c) {
            Column *cp = other->column(c);
            if (!cp)
                continue;
            const QList<Interval<int>> ivs = cp->formulaIntervals();
            for (const Interval<int> &iv : ivs) {
                const QString f = cp->formula(iv.start());
                const QString updated = rewriteTableRefsInFormula(f, oldTableName, newTableName);
                if (updated != f)
                    updates.push_back({ c, iv, updated });
            }
        }
        if (updates.isEmpty())
            continue;
        const bool prevGuard = other->d_in_auto_recalc;
        other->d_in_auto_recalc = true;
        other->d_future_table->beginMacro(
                QCoreApplication::translate("WorkbookManager", "Update formula references to %1")
                        .arg(newTableName));
        for (const PendingUpdate &u : updates) {
            if (Column *cp = other->column(u.col))
                cp->setFormula(u.iv, u.formula);
        }
        other->d_future_table->endMacro();
        other->d_in_auto_recalc = prevGuard;
    }
}

void Table::handleAspectDescriptionAboutToChange(const AbstractAspect *aspect)
{
    if (d_future_table && aspect == d_future_table) {
        d_stored_table_name = aspect->name();
        d_has_stored_table_name = true;
        return;
    }
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
        if (!d_has_stored_table_name)
            return;
        const QString oldTableName = d_stored_table_name;
        const QString newTableName = d_future_table->name();
        d_stored_table_name.clear();
        d_has_stored_table_name = false;
        if (oldTableName.isEmpty() || oldTableName == newTableName)
            return;
        // A rename, or the undo / redo of one: graphs, graph undo snapshots and the project
        // explorer follow the name each time. Formula references are rewritten only on the
        // forward edit (inside the rename macro); undo / redo replays those commands.
        ApplicationWindow *aw = owningApplicationWindowForTable(this);
        if (!aw || !aw->windowsList().contains(this))
            return;
        aw->updateTableNames(oldTableName, newTableName);
        aw->renameListViewItem(oldTableName, newTableName);
        const bool forwardEdit = !isUndoRedoReplaying()
                && (!d_future_table->undoStack()
                    || isUndoStackInMacro(d_future_table->undoStack()));
        if (forwardEdit)
            updateCrossTableFormulaReferences(aw, oldTableName, newTableName);
        return;
    }
    const Column *col = qobject_cast<const Column *>(aspect);
    if (!col || !d_future_table)
        return;
    const int colIdx = d_future_table->columnIndex(col);
    if (colIdx == -1)
        return;

    const bool inUndoOrOutsideMacro = isUndoRedoReplaying()
            || (d_future_table->undoStack() && !isUndoStackInMacro(d_future_table->undoStack()));

    if (d_stored_column_labels.contains(col)) {
        const QString old_name = d_stored_column_labels.take(col);
        const QString new_name = col->name();
        if (old_name != new_name) {
            emit changedColHeader(name() + "_" + old_name, name() + "_" + new_name);

            if (!inUndoOrOutsideMacro) {
                // References to this column, resolved like the formula dependency analysis:
                // own-table col("A") / col("Self/A") / tablecol("Self","A") and other tables'
                // col("Self/A") / tablecol("Self","A"). tablecol("Other","A") in this table is
                // left alone. The setFormula commands join the rename macro (undoable).
                const QString selfName = name();
                const QList<Table *> tables =
                        renameTargetTables(owningApplicationWindowForTable(this), this);
                for (Table *t : tables) {
                    const QString formulaTable = t->name();
                    const bool prevGuard = t->d_in_auto_recalc;
                    t->d_in_auto_recalc = true;
                    for (int i = 0; i < t->numCols(); i++) {
                        Column *cp = t->column(i);
                        if (!cp)
                            continue;
                        const QList<Interval<int>> formula_intervals = cp->formulaIntervals();
                        for (const Interval<int> &interval : formula_intervals) {
                            const QString formula = cp->formula(interval.start());
                            const QString updated = rewriteColumnRefsInFormula(
                                    formula, formulaTable, selfName, old_name, new_name);
                            if (updated != formula)
                                cp->setFormula(interval, updated);
                        }
                    }
                    t->d_in_auto_recalc = prevGuard;
                }
            }
        }
    }

    if (!inUndoOrOutsideMacro && d_auto_recalc_formulas && !d_in_auto_recalc
        && !col->formulaIntervals().isEmpty()) {
        d_in_auto_recalc = true;
        recalculateColumnInternal(colIdx, false, true);
        d_in_auto_recalc = false;
    }
    handleChange();
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
        if (!temp || temp->columnCount() == 0) {
            delete temp;
            d_import_failed = true;
            d_last_import_error = tr("Could not parse data from file: %1")
                                          .arg(QDir::toNativeSeparators(fname));
            const QString refusal = AsciiTableImportFilter::lastImportReport().refusal;
            if (!refusal.isEmpty())
                d_last_import_error += QStringLiteral("\n") + refusal;
            return;
        }
        d_import_failed = false;
        const int prevRows = numRows();
        const int newRows = temp->rowCount();
        const int importedSpan = computeImportedColumnSpan(d_future_table);
        const int overwritten_cols = qMin(temp->columnCount(), importedSpan);
        if (newRows != prevRows)
            d_future_table->setRowCount(newRows);
        QVector<QString> targetNames(overwritten_cols);
        for (int i = 0; i < overwritten_cols; i++) {
            Column *dst = column(i);
            Column *src = temp->column(i);
            if (!dst || !src)
                continue;
            dst->clearFormulas();
            dst->clearMasks();
            if (src->columnMode() == SciDAVis::ColumnMode::Numeric) {
                if (dst->columnMode() != SciDAVis::ColumnMode::Numeric)
                    dst->setColumnMode(SciDAVis::ColumnMode::Numeric);
                dst->copy(src);
            } else if (src->columnMode() == dst->columnMode()) {
                dst->copy(src);
            } else {
                dst->asStringColumn()->copy(src);
            }
            if (renameCols)
                targetNames[i] = src->name();
        }
        // Remove the surplus imported columns first, so that a header moving left ([A,B,C] ->
        // [C,A]) does not collide with the old column "C" during the rename below.
        if (importedSpan > overwritten_cols)
            d_future_table->removeColumns(overwritten_cols, importedSpan - overwritten_cols);
        if (renameCols) {
            int tmpIdx = 0;
            for (int i = 0; i < overwritten_cols; i++) {
                Column *dst = column(i);
                if (!dst || targetNames[i].isEmpty() || dst->name() == targetNames[i])
                    continue;
                QString tmpName;
                do {
                    tmpName = QStringLiteral("__tg_ascii_tmp_%1").arg(tmpIdx++);
                } while (d_future_table->column(tmpName, false) != nullptr
                         || targetNames.contains(tmpName));
                dst->setName(tmpName);
            }
            for (int i = 0; i < overwritten_cols; i++) {
                Column *dst = column(i);
                if (!dst || targetNames[i].isEmpty() || dst->name() == targetNames[i])
                    continue;
                dst->setName(targetNames[i]);
            }
        }
        String2DoubleFilter *numFilter = nullptr;
        QList<Column *> extraCols;
        for (int i = overwritten_cols; i < temp->columnCount(); i++) {
            Column *src = temp->column(i);
            if (!src)
                continue;
            Column *new_col = new Column(src->name(), SciDAVis::ColumnMode::Numeric);
            new_col->setPlotDesignation(SciDAVis::Y);
            if (src->columnMode() == SciDAVis::ColumnMode::Numeric) {
                new_col->copy(src);
            } else if (src->columnMode() == SciDAVis::ColumnMode::Text) {
                if (!numFilter)
                    numFilter = new String2DoubleFilter;
                numFilter->input(0, src);
                new_col->copy(numFilter->output(0));
            } else {
                new_col->setColumnMode(src->columnMode());
                new_col->copy(src);
            }
            extraCols.append(new_col);
        }
        if (!extraCols.isEmpty())
            d_future_table->insertColumns(overwritten_cols, extraCols);
        delete numFilter;
        d_future_table->setImportSource(fname, QString(), sep,
                                        AsciiTableImportFilter::dialogDefaultEncoding(),
                                        ignoredLines,
                                        AsciiTableImportFilter::dialogDefaultUnitsRow(),
                                        temp->columnCount());
        const QVector<int> derivedCols =
                collectPreservedFormulaCols(d_future_table, temp->columnCount(), prevRows, newRows);
        if (!derivedCols.isEmpty() && d_auto_recalc_formulas && !isUndoRedoReplaying()) {
            evaluateCrossTableDag(derivedCols, true);
            reportFormulaErrorsToUser(-1);
        }
        delete temp;
        setWindowLabel(fname);
    } else {
        d_import_failed = true;
        d_last_import_error = tr("Could not open the file: %1\n%2")
                                      .arg(QDir::toNativeSeparators(fname), file.errorString());
    }
}
