/***************************************************************************
    File                 : Matrix.cpp
    Project              : SciDAVis
    Description          : Matrix worksheet class
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
#include "Matrix.h"
#include "ApplicationWindow.h"
#include "future/matrix/MatrixView.h"
#include "ScriptEdit.h"

#include <QtGlobal>
#include <QTextStream>
#include <QList>
#include <QEvent>
#include <QContextMenuEvent>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QHeaderView>
#include <QDateTime>
#include <QApplication>
#include <QMessageBox>
#include <QVarLengthArray>
#include <QClipboard>
#include <QShortcut>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QLocale>
#include <QUndoStack>
#include <QXmlStreamWriter>
#include <QtDebug>

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <cmath>
#include <limits>
#include <vector>

#include <gsl/gsl_linalg.h>
#include <gsl/gsl_math.h>

Matrix::Matrix(ScriptingEnv *env, int r, int c, const QString &label, QWidget *parent,
               const char *name, Qt::WindowFlags f)
    : MatrixView(label, parent, name, f), scripted(env)
{
    d_future_matrix = new future::Matrix(0, r, c, label);
    init(r, c);
}

Matrix::Matrix(future::Matrix *future_matrix, ScriptingEnv *env, int r, int c, const QString &label,
               QWidget *parent, const char *name, Qt::WindowFlags f)
    : MatrixView(label, parent, name, f), scripted(env)
{
    d_future_matrix = future_matrix;
    init(r, c);
}

void Matrix::init(int, int)
{
    MatrixView::setMatrix(d_future_matrix);
    d_future_matrix->setView(this);
    d_future_matrix->setNumericFormat('f');
    d_future_matrix->setDisplayedDigits(6);
    d_future_matrix->setCoordinates(1.0, 10.0, 1.0, 10.0);
    dMatrix = 0;

    birthdate = QLocale().toString(d_future_matrix->creationTime());

    setMinimumSize(400, 300);
    // this is not very nice but works for the moment
    ui.gridLayout2->removeWidget(ui.formula_box);
    delete ui.formula_box;
    ui.formula_box = new ScriptEdit(scriptEnv, ui.formula_tab);
    ui.formula_box->setObjectName(QString::fromUtf8("formula_box"));
    ui.formula_box->setMinimumSize(QSize(60, 10));
    ui.formula_box->setAcceptRichText(false);
    ui.formula_box->setLineWrapMode(QTextEdit::WidgetWidth);
    ui.gridLayout2->addWidget(ui.formula_box, 1, 0, 1, 3);

    ui.add_cell_combobox->addItem("cell(i, j)");
    ui.add_function_combobox->addItems(scriptEnv->mathFunctions());
    updateFunctionDoc();

    connect(ui.add_function_combobox, SIGNAL(currentIndexChanged(int)), this,
            SLOT(updateFunctionDoc()));
    connect(ui.button_set_formula, SIGNAL(pressed()), this, SLOT(applyFormula()));
    connect(ui.add_function_button, SIGNAL(pressed()), this, SLOT(addFunction()));
    connect(ui.add_cell_button, SIGNAL(pressed()), this, SLOT(addCell()));

    connect(d_future_matrix, SIGNAL(columnsInserted(int, int)), this, SLOT(handleChange()));
    connect(d_future_matrix, SIGNAL(columnsRemoved(int, int)), this, SLOT(handleChange()));
    connect(d_future_matrix, SIGNAL(rowsInserted(int, int)), this, SLOT(handleChange()));
    connect(d_future_matrix, SIGNAL(rowsRemoved(int, int)), this, SLOT(handleChange()));
    connect(d_future_matrix, SIGNAL(dataChanged(int, int, int, int)), this, SLOT(handleChange()));
    connect(d_future_matrix, SIGNAL(coordinatesChanged()), this, SLOT(handleChange()));
    connect(d_future_matrix, SIGNAL(formulaChanged()), this, SLOT(handleChange()));
    connect(d_future_matrix, SIGNAL(formatChanged()), this, SLOT(handleChange()));
    connect(d_future_matrix, SIGNAL(recalculate()), this, SLOT(recalculate()));

    connect(d_future_matrix, SIGNAL(aspectDescriptionChanged(const AbstractAspect *)), this,
            SLOT(handleAspectDescriptionChange(const AbstractAspect *)));
}

Matrix::~Matrix()
{
    if (dMatrix) {
        freeMatrixData(dMatrix, d_future_matrix ? d_future_matrix->rowCount() : 0);
        dMatrix = 0;
    }
    if (d_future_matrix && !d_future_matrix->parentAspect() && d_future_matrix->view() == this) {
        future::Matrix *fm = d_future_matrix.data();
        d_future_matrix.clear();
        fm->setView(nullptr);
        if (!fm->property("_tg_owned_by_undo_cmd").toBool())
            delete fm;
    }
}

void Matrix::handleChange()
{
    emit modifiedWindow(this);
}

double Matrix::cell(int row, int col)
{
    if (dMatrix)
        return dMatrix[row][col];
    else
        return d_future_matrix->cell(row, col);
}

void Matrix::setCell(int row, int col, double value)
{
    d_future_matrix->setCell(row, col, value);
}

void Matrix::setCells(const QVector<qreal> &data)
{
    d_future_matrix->setCells(data);
}

QString Matrix::text(int row, int col)
{
    return d_future_matrix->text(row, col);
}

void Matrix::setText(int row, int col, const QString &new_text)
{
    bool ok = true;
    QLocale locale;
    double res = locale.toDouble(new_text, &ok);
    if (ok)
        d_future_matrix->setCell(row, col, res);
    else {
        Script *script = scriptEnv->newScript(new_text, this,
                                              QString("<%1_%2_%3>").arg(name()).arg(row).arg(col));
        connect(script, SIGNAL(error(const QString &, const QString &, int)), scriptEnv,
                SIGNAL(error(const QString &, const QString &, int)));

        script->setInt(row + 1, "row");
        script->setInt(row + 1, "i");
        script->setInt(col + 1, "col");
        script->setInt(col + 1, "j");

        QVariant ret = script->eval();
        setCell(row, col, ret.toDouble());
    }
}

void Matrix::setCoordinates(double xs, double xe, double ys, double ye)
{
    d_future_matrix->setCoordinates(xs, xe, ys, ye);
}

QString Matrix::saveToString(const QString &geometry)
{
    QString s = "<matrix>\n";
    QString xml;
    QXmlStreamWriter writer(&xml);
    d_future_matrix->save(&writer);
    s += QString::number(xml.length()) + "\n"; // this is need in case there are newlines in the XML
    s += xml + "\n";
    s += geometry + "\n";
    s += "</matrix>\n";
    return s;

#if 0
	QString s = "<matrix>\n";
	s += QString(name()) + "\t";
	s += QString::number(numRows())+"\t";
	s += QString::number(numCols())+"\t";
	s += birthDate() + "\n";
	s += info;
	s += "ColWidth\t" + QString::number(columnWidth(0))+"\n";
	s += "<formula>\n" + formula() + "\n</formula>\n";
	s += "TextFormat\t" + QString(d_future_matrix->numericFormat()) + "\t" + QString::number(d_future_matrix->displayedDigits()) + "\n";
	s += "WindowLabel\t" + windowLabel() + "\t" + QString::number(captionPolicy()) + "\n";
	s += "Coordinates\t" + QString::number(xStart(),'g',15) + "\t" +QString::number(xEnd(),'g',15) + "\t";
	s += QString::number(yStart(),'g',15) + "\t" + QString::number(yEnd(),'g',15) + "\n";
	s += saveText();
	s +="</matrix>\n";
	return s;
#endif
}

QString Matrix::saveAsTemplate(const QString &info)
{
    QString s = "<matrix>\t";
    s += QString::number(numRows()) + "\t";
    s += QString::number(numCols()) + "\n";
    s += info;
    s += "ColWidth\t" + QString::number(columnWidth(0)) + "\n";
    s += "<formula>\n" + formula() + "\n</formula>\n";
    s += "TextFormat\t" + QString(d_future_matrix->numericFormat()) + "\t"
            + QString::number(d_future_matrix->displayedDigits()) + "\n";
    s += "Coordinates\t" + QString::number(xStart(), 'g', 15) + "\t"
            + QString::number(xEnd(), 'g', 15) + "\t";
    s += QString::number(yStart(), 'g', 15) + "\t" + QString::number(yEnd(), 'g', 15) + "\n";
    s += "</matrix>\n";
    return s;
}

QString Matrix::saveText()
{
    QString out_text = "<data>\n";
    int cols = d_future_matrix->columnCount();
    for (int i = 0; i < d_future_matrix->rowCount(); i++) {
        out_text += QString::number(i) + "\t";
        for (int j = 0; j < cols - 1; j++)
            out_text += QString::number(cell(i, j), 'e', 16) + "\t";

        out_text += QString::number(cell(i, cols - 1), 'e', 16) + "\n";
    }
    return out_text + "</data>\n";
}

void Matrix::setFormula(const QString &s)
{
    d_future_matrix->setFormula(s);
}

QString Matrix::formula()
{
    return d_future_matrix->formula();
}

void Matrix::setNumericFormat(const QChar &f, int prec)
{
    if (d_future_matrix->numericFormat() == f && d_future_matrix->displayedDigits() == prec)
        return;

    d_future_matrix->setNumericFormat(f.toLatin1());
    d_future_matrix->setDisplayedDigits(prec);

    emit modifiedWindow(this);
}

void Matrix::setTextFormat(const QChar &format, int precision)
{
    d_future_matrix->setNumericFormat(format.toLatin1());
    d_future_matrix->setDisplayedDigits(precision);
}

void Matrix::setColumnsWidth(int width)
{
    for (int i = 0; i < d_future_matrix->columnCount(); i++)
        setColumnWidth(i, width);
}

void Matrix::setDimensions(int rows, int cols)
{
    d_future_matrix->setDimensions(rows, cols);
}

int Matrix::numRows()
{
    return d_future_matrix->rowCount();
}

void Matrix::setNumRows(int rows)
{
    d_future_matrix->setDimensions(rows, d_future_matrix->columnCount());
}

int Matrix::numCols()
{
    return d_future_matrix->columnCount();
}

void Matrix::setNumCols(int cols)
{
    d_future_matrix->setDimensions(d_future_matrix->rowCount(), cols);
}

namespace {

struct DenseLuDecomposition
{
    int n = 0;
    double norm1 = 0.0;
    int sign = 1;
    bool singular = false;
    bool nonFinite = false;
    std::vector<double> lu;
    std::vector<int> piv;
};

// relPivotTol > 0: a pivot not above relPivotTol * ||A||_1 counts as singular (what invert()
// needs). relPivotTol == 0: only an exactly zero or non-finite pivot counts as singular.
// determinant() does not use this; it goes through Matrix::denseDeterminant().
static DenseLuDecomposition decomposeDenseMatrixLu(Matrix *m, int n, double relPivotTol)
{
    DenseLuDecomposition res;
    res.n = n;
    res.lu.resize(size_t(n) * size_t(n), 0.0);
    res.piv.resize(size_t(n), 0);
    for (int i = 0; i < n; ++i)
        res.piv[size_t(i)] = i;

    for (int j = 0; j < n; ++j) {
        double colAbsSum = 0.0;
        for (int i = 0; i < n; ++i) {
            const double v = m->cell(i, j);
            if (!std::isfinite(v)) {
                res.nonFinite = true;
                res.singular = true;
                return res;
            }
            res.lu[size_t(i) * size_t(n) + size_t(j)] = v;
            colAbsSum += std::abs(v);
        }
        if (colAbsSum > res.norm1)
            res.norm1 = colAbsSum;
    }

    if (relPivotTol > 0.0 && (!(res.norm1 > 0.0) || !std::isfinite(res.norm1))) {
        res.singular = true;
        return res;
    }

    const double pivTol = relPivotTol > 0.0 ? relPivotTol * res.norm1 : 0.0;
    for (int k = 0; k < n; ++k) {
        int pivRow = k;
        double maxAbs = std::abs(res.lu[size_t(k) * size_t(n) + size_t(k)]);
        for (int i = k + 1; i < n; ++i) {
            const double cand = std::abs(res.lu[size_t(i) * size_t(n) + size_t(k)]);
            if (cand > maxAbs) {
                maxAbs = cand;
                pivRow = i;
            }
        }
        if (!(maxAbs > pivTol) || !std::isfinite(maxAbs)) {
            res.singular = true;
            return res;
        }
        if (pivRow != k) {
            for (int j = 0; j < n; ++j) {
                std::swap(res.lu[size_t(k) * size_t(n) + size_t(j)],
                          res.lu[size_t(pivRow) * size_t(n) + size_t(j)]);
            }
            std::swap(res.piv[size_t(k)], res.piv[size_t(pivRow)]);
            res.sign = -res.sign;
        }

        const double invDiag = 1.0 / res.lu[size_t(k) * size_t(n) + size_t(k)];
        for (int i = k + 1; i < n; ++i) {
            double *rowI = &res.lu[size_t(i) * size_t(n)];
            const double *rowK = &res.lu[size_t(k) * size_t(n)];
            const double lik = rowI[k] * invDiag;
            rowI[k] = lik;
            if (lik != 0.0) {
                for (int j = k + 1; j < n; ++j)
                    rowI[j] -= lik * rowK[j];
            }
        }
    }

    return res;
}

} // namespace

double Matrix::denseDeterminant(const double *a, int n)
{
    if (!a || n <= 0)
        return std::numeric_limits<double>::quiet_NaN();
    const size_t nn = size_t(n);
    std::vector<double> lu(a, a + nn * nn);
    std::vector<double> colMax(nn, 0.0);
    for (size_t i = 0; i < nn; ++i) {
        for (size_t j = 0; j < nn; ++j) {
            const double v = lu[i * nn + j];
            if (!std::isfinite(v))
                return std::numeric_limits<double>::quiet_NaN();
            colMax[j] = std::max(colMax[j], std::abs(v));
        }
    }

    const double relTol = double(n) * std::numeric_limits<double>::epsilon();
    double det = 1.0;
    for (size_t k = 0; k < nn; ++k) {
        size_t pivRow = k;
        double maxAbs = std::abs(lu[k * nn + k]);
        for (size_t i = k + 1; i < nn; ++i) {
            const double cand = std::abs(lu[i * nn + k]);
            if (cand > maxAbs) {
                maxAbs = cand;
                pivRow = i;
            }
        }
        // Singular when the pivot is lost in the rounding of its own column (or not finite).
        if (!std::isfinite(maxAbs) || !(maxAbs > relTol * colMax[k]))
            return 0.0;
        if (pivRow != k) {
            for (size_t j = 0; j < nn; ++j)
                std::swap(lu[k * nn + j], lu[pivRow * nn + j]);
            det = -det;
        }
        const double diag = lu[k * nn + k];
        det *= diag;
        for (size_t i = k + 1; i < nn; ++i) {
            const double lik = lu[i * nn + k] / diag;
            if (lik != 0.0) {
                for (size_t j = k + 1; j < nn; ++j)
                    lu[i * nn + j] -= lik * lu[k * nn + j];
            }
        }
    }
    return det;
}

double Matrix::determinant()
{
    if (!d_future_matrix)
        return std::numeric_limits<double>::quiet_NaN();

    const int rows = numRows();
    const int cols = numCols();
    const bool nonInteractive = ApplicationWindow::isNonInteractiveContext(this);

    if (rows != cols) {
        if (!nonInteractive)
            QMessageBox::critical(0, tr("Error"), tr("Calculation failed, the matrix is not square!"));
        return GSL_POSINF;
    }
    if (rows <= 0 || rows > 4096) {
        if (!nonInteractive)
            QMessageBox::critical(0, tr("Error"),
                                  tr("Matrix size (%1x%1) exceeds dense LU limit (4096).").arg(rows));
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    std::vector<double> a(size_t(rows) * size_t(rows), 0.0);
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < rows; ++j)
            a[size_t(i) * size_t(rows) + size_t(j)] = cell(i, j);
    const double det = denseDeterminant(a.data(), rows);

    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
    return det;
}

void Matrix::invert()
{
    if (!d_future_matrix)
        return;

    const int rows = numRows();
    const int cols = numCols();
    const bool nonInteractive = ApplicationWindow::isNonInteractiveContext(this);

    if (rows != cols) {
        if (!nonInteractive)
            QMessageBox::critical(0, tr("Error"), tr("Inversion failed, the matrix is not square!"));
        return;
    }
    if (rows <= 0 || rows > 4096) {
        if (!nonInteractive)
            QMessageBox::critical(0, tr("Error"),
                                  tr("Matrix size (%1x%1) exceeds dense LU limit (4096).").arg(rows));
        return;
    }

    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    const int n = rows;
    const DenseLuDecomposition lu = decomposeDenseMatrixLu(this, n, 1e-14);
    if (lu.singular || lu.nonFinite) {
        if (!nonInteractive) {
            QApplication::restoreOverrideCursor();
            QMessageBox::critical(0, tr("Error"),
                                  tr("Inversion failed, the matrix is singular or ill-conditioned!"));
        }
        return;
    }

    std::vector<double> inv(size_t(n) * size_t(n), 0.0);
    std::vector<double> colVec(size_t(n), 0.0);
    double invNorm1 = 0.0;
    bool badEntry = false;

    for (int col = 0; col < n && !badEntry; ++col) {
        for (int i = 0; i < n; ++i)
            colVec[size_t(i)] = (lu.piv[size_t(i)] == col) ? 1.0 : 0.0;

        // Forward substitution: L * y = P * e_col
        for (int i = 0; i < n; ++i) {
            double sum = colVec[size_t(i)];
            const double *rowI = &lu.lu[size_t(i) * size_t(n)];
            for (int j = 0; j < i; ++j)
                sum -= rowI[j] * colVec[size_t(j)];
            colVec[size_t(i)] = sum;
        }

        // Back substitution: U * x = y
        double colAbsSum = 0.0;
        for (int i = n - 1; i >= 0; --i) {
            double sum = colVec[size_t(i)];
            const double *rowI = &lu.lu[size_t(i) * size_t(n)];
            for (int j = i + 1; j < n; ++j)
                sum -= rowI[j] * colVec[size_t(j)];
            const double xVal = sum / rowI[i];
            if (!std::isfinite(xVal)) {
                badEntry = true;
                break;
            }
            colVec[size_t(i)] = xVal;
            inv[size_t(i) * size_t(n) + size_t(col)] = xVal;
            colAbsSum += std::abs(xVal);
        }
        if (colAbsSum > invNorm1)
            invNorm1 = colAbsSum;
    }

    const double rcond = (!badEntry && lu.norm1 > 0.0 && invNorm1 > 0.0 && std::isfinite(invNorm1))
            ? (1.0 / (lu.norm1 * invNorm1))
            : 0.0;
    if (badEntry || !std::isfinite(rcond) || rcond < 1e-13 * double(n)) {
        if (!nonInteractive) {
            QApplication::restoreOverrideCursor();
            QMessageBox::critical(0, tr("Error"),
                                  tr("Inversion failed, the matrix is singular or ill-conditioned!"));
        }
        return;
    }

    // future::Matrix::setCells expects column-major flat layout (col * rows + row) or let's verify!
    this->blockSignals(true);
    d_future_matrix->beginMacro(tr("%1: invert").arg(name()));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j)
            setCell(i, j, inv[size_t(i) * size_t(n) + size_t(j)]);
    }
    d_future_matrix->endMacro();
    this->blockSignals(false);

    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
    emit modifiedWindow(this);
}

void Matrix::transpose()
{
    d_future_matrix->transpose();
}

void Matrix::saveCellsToMemory()
{
    int rows = numRows();
    int cols = numCols();
    dMatrix = allocateMatrixData(rows, cols);
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            dMatrix[i][j] = d_future_matrix->cell(i, j);
}

void Matrix::forgetSavedCells()
{
    freeMatrixData(dMatrix, numRows());
    dMatrix = 0;
}

bool Matrix::recalculate()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    Script *script = scriptEnv->newScript(formula(), this, QString("<%1>").arg(name()));
    connect(script, SIGNAL(error(const QString &, const QString &, int)), scriptEnv,
            SIGNAL(error(const QString &, const QString &, int)));
    connect(script, SIGNAL(print(const QString &)), scriptEnv, SIGNAL(print(const QString &)));
    if (!script->compile()) {
        QApplication::restoreOverrideCursor();
        return false;
    }

    this->blockSignals(true);

    int startRow = firstSelectedRow(false);
    int endRow = lastSelectedRow(false);
    int startCol = firstSelectedColumn(false);
    int endCol = lastSelectedColumn(false);

    QVariant ret;
    saveCellsToMemory();
    // x runs along the columns and y along the rows (as in MatrixModel::headerData), so dx is
    // divided by the column count and dy by the row count; the sign is kept so that a reversed
    // range (xEnd < xStart) matches the header, and a single column/row gives a step of 0.
    const int nCols = numCols();
    const int nRows = numRows();
    const double dx = nCols > 1 ? (xEnd() - xStart()) / double(nCols - 1) : 0.0;
    const double dy = nRows > 1 ? (yEnd() - yStart()) / double(nRows - 1) : 0.0;
    std::pair<double, bool> unsetValue(std::numeric_limits<double>::quiet_NaN(), false);
    std::vector<std::vector<std::pair<double, bool>>> calculatedValues(
            endRow - startRow + 1,
            std::vector<std::pair<double, bool>>(endCol - startCol + 1, unsetValue));
    for (int row = startRow; row <= endRow; row++)
        for (int col = startCol; col <= endCol; col++) {
            if (!isCellSelected(row, col))
                continue;
            script->setInt(row + 1, "i");
            script->setInt(row + 1, "row");
            script->setDouble(yStart() + row * dy, "y");
            script->setInt(col + 1, "j");
            script->setInt(col + 1, "col");
            script->setDouble(xStart() + col * dx, "x");
            ret = script->eval();
            if (!ret.isValid()) {
                forgetSavedCells();
                blockSignals(false);
                emit modifiedWindow(this);
                QApplication::restoreOverrideCursor();
                return false;
            }
            calculatedValues[row - startRow][col - startCol] = std::make_pair(ret.toDouble(), true);
        }
    d_future_matrix->setCells(startRow, startCol, calculatedValues);
    forgetSavedCells();

    blockSignals(false);
    emit modifiedWindow(this);
    QApplication::restoreOverrideCursor();
    return true;
}

void Matrix::clearSelection()
{
    d_future_matrix->clearSelectedCells();
}

void Matrix::copySelection()
{
    d_future_matrix->copySelection();
}

void Matrix::cutSelection()
{
    copySelection();
    clearSelection();
}

bool Matrix::rowsSelected()
{
    for (int i = 0; i < numRows(); i++) {
        if (this->isRowSelected(i, true))
            return true;
    }
    return false;
}

void Matrix::deleteSelectedRows()
{
    d_future_matrix->removeSelectedRows();
}

void Matrix::insertColumn()
{
    d_future_matrix->insertEmptyColumns();
}

bool Matrix::columnsSelected()
{
    for (int i = 0; i < numCols(); i++) {
        if (this->isColumnSelected(i, true))
            return true;
    }
    return false;
}

void Matrix::deleteSelectedColumns()
{
    d_future_matrix->removeSelectedColumns();
}

int Matrix::numSelectedRows()
{
    int r = 0;
    for (int i = 0; i < numRows(); i++)
        if (this->isRowSelected(i, true))
            r++;
    return r;
}

int Matrix::numSelectedColumns()
{
    int c = 0;
    for (int i = 0; i < numCols(); i++)
        if (this->isColumnSelected(i, true))
            c++;
    return c;
}

void Matrix::insertRow()
{
    d_future_matrix->insertEmptyRows();
}

void Matrix::pasteSelection()
{
    d_future_matrix->pasteIntoSelection();
}

void Matrix::customEvent(QEvent *e)
{
    if (e->type() == SCRIPTING_CHANGE_EVENT)
        scriptingChangeEvent((ScriptingChangeEvent *)e);
}

void Matrix::closeEvent(QCloseEvent *e)
{
    if (askOnClose && !ApplicationWindow::isNonInteractiveContext(this)) {
        switch (QMessageBox::information(this, tr("ToriGraph"),
                                         tr("Do you want to hide or delete") + "<p><b>'"
                                                 + objectName() + "'</b> ?",
                                         tr("Delete"), tr("Hide"), tr("Cancel"), 1, 2)) {
        case 0: {
            e->accept();
            QPointer<Matrix> self(this);
            const bool retainForUndo = d_future_matrix && d_future_matrix->parentAspect()
                    && d_future_matrix->undoStack() != nullptr;
            if (d_future_matrix)
                d_future_matrix->remove();
            if (!retainForUndo && self)
                self->deleteLater();
            return;
        }

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
        QPointer<Matrix> self(this);
        const bool retainForUndo = d_future_matrix && d_future_matrix->parentAspect()
                && d_future_matrix->undoStack() != nullptr;
        if (d_future_matrix)
            d_future_matrix->remove();
        if (!retainForUndo && self)
            self->deleteLater();
        return;
    }
}

void Matrix::exportPDF(const QString &fileName)
{
    print(fileName);
}

void Matrix::print()
{
    print(QString());
}

void Matrix::print(const QString &fileName)
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

    QHeaderView *vHeader = d_view_widget->verticalHeader();

    int rows = numRows();
    int cols = numCols();
    int height = margin;
    int i, vertHeaderWidth = vHeader->width();
    int right = margin + vertHeaderWidth;

    // print header
    p.setFont(QFont());
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

void Matrix::range(double *min, double *max)
{
    double d_min = cell(0, 0);
    double d_max = d_min;

    for (int i = 0; i < numRows(); i++) {
        for (int j = 0; j < numCols(); j++) {
            double aux = cell(i, j);
            if (aux <= d_min)
                d_min = aux;

            if (aux >= d_max)
                d_max = aux;
        }
    }

    *min = d_min;
    *max = d_max;
}

double **Matrix::allocateMatrixData(int rows, int columns)
{
    double **data = new double *[rows];
    for (int i = 0; i < rows; ++i)
        data[i] = new double[columns];

    return data;
}

void Matrix::freeMatrixData(double **data, int rows)
{
    for (int i = 0; i < rows; i++)
        delete[] data[i];

    delete[] data;
}

void Matrix::copy(Matrix *m)
{
    if (!m)
        return;

    d_future_matrix->copy(m->d_future_matrix);
}

Matrix *Matrix::fromImage(const QImage &image, ScriptingEnv *env)
{
    future::Matrix *fm = future::Matrix::fromImage(image);
    if (!fm)
        return NULL;
    return new Matrix(fm, env, image.height(), image.width(), tr("Matrix %1").arg(1));
}

void Matrix::applyFormula()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    d_future_matrix->beginMacro(tr("%1: apply formula to selection").arg(name()));

    setFormula(ui.formula_box->toPlainText());
    recalculate();

    d_future_matrix->endMacro();
    QApplication::restoreOverrideCursor();
}

void Matrix::addFunction()
{
    static_cast<ScriptEdit *>(ui.formula_box)
            ->insertFunction(ui.add_function_combobox->currentText());
}

void Matrix::addCell()
{
    ui.formula_box->insertPlainText(ui.add_cell_combobox->currentText());
}

void Matrix::updateFunctionDoc()
{
    ui.add_function_combobox->setToolTip(
            scriptEnv->mathFunctionDoc(ui.add_function_combobox->currentText()));
}

void Matrix::handleAspectDescriptionChange(const AbstractAspect *aspect)
{
    if (aspect != d_future_matrix)
        return;
    setObjectName(d_future_matrix->name());
    updateCaption();
}
