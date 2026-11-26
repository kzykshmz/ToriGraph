/***************************************************************************
    File                 : TranslateCurveTool.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006,2007 by Ion Vasilief,
                           Tilman Benkert, Knut Franke
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net,
                           knut.franke*gmx.de
    Description          : Plot tool for translating curves.

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
#include "TranslateCurveTool.h"
#include "Graph.h"
#include "PlotCurve.h"
#include "FunctionCurve.h"
#include "ApplicationWindow.h"
#include "DataPickerTool.h"
#include "ScreenPickerTool.h"
#include <QMessageBox>
#include <QLocale>
#include <QUndoStack>
#include <qwt_plot_curve.h>
#include "core/Project.h"
#include "core/column/Column.h"

#include <cmath>
#include <stdexcept>
using namespace std;

TranslateCurveTool::TranslateCurveTool(Graph *graph, ApplicationWindow *app, Direction dir,
                                       const QObject *status_target, const char *status_slot)
    : PlotToolInterface(graph),
      d_dir(dir),
      d_sub_tool(nullptr),
      d_selected_curve(nullptr),
      d_app(app)
{
    if (status_target)
        connect(this, SIGNAL(statusText(const QString &)), status_target, status_slot);
    // d_graph->plotWidget()->canvas()->setCursor(QCursor(QPixmap(vizor_xpm), -1, -1));
    // emit statusText(tr("Double-click on plot to select a data point!"));

    // Phase 1: select curve point
    d_sub_tool = new DataPickerTool(d_graph, app, DataPickerTool::Display, this,
                                    SIGNAL(statusText(const QString &)));
    connect((DataPickerTool *)d_sub_tool, SIGNAL(selected(QwtPlotCurve *, int)), this,
            SLOT(selectCurvePoint(QwtPlotCurve *, int)));
}

TranslateCurveTool::~TranslateCurveTool()
{
    delete d_sub_tool;
    d_sub_tool = nullptr;
}

void TranslateCurveTool::selectCurvePoint(QwtPlotCurve *curve, int point_index)
{
    if (!curve || !d_graph || d_graph->curveIndex(curve) < 0 || point_index < 0
        || point_index >= curve->dataSize())
        return;

    d_selected_curve = curve;
    d_curve_point = QwtDoublePoint(curve->x(point_index), curve->y(point_index));
    delete d_sub_tool;
    d_sub_tool = nullptr;

    // Phase 2: select destination
    ScreenPickerTool *picker =
            new ScreenPickerTool(d_graph, this, SIGNAL(statusText(const QString &)));
    picker->setAxis(d_selected_curve->xAxis(), d_selected_curve->yAxis());
    d_sub_tool = picker;
    connect(picker, SIGNAL(selected(const QwtDoublePoint &)), this,
            SLOT(selectDestination(const QwtDoublePoint &)));
    emit statusText(tr("Curve selected! Move cursor and click to choose a point and "
                       "double-click/press 'Enter' to finish!"));
}

void TranslateCurveTool::selectDestination(const QwtDoublePoint &point)
{
    delete d_sub_tool;
    d_sub_tool = nullptr;
    if (!d_graph || !d_selected_curve || d_graph->curveIndex(d_selected_curve) < 0) {
        if (d_graph)
            d_graph->setActiveTool(NULL);
        return;
    }

    // Phase 3: execute the translation
    const double d = (d_dir == Horizontal) ? point.x() - d_curve_point.x()
                                           : point.y() - d_curve_point.y();
    translateCurve(d_selected_curve, d);
    d_graph->setActiveTool(NULL);
    // attention: I'm now deleted. Maybe there is a cleaner solution...*/
}

bool TranslateCurveTool::translateCurve(QwtPlotCurve *curve, double delta)
{
    if (!d_graph || !curve || d_graph->curveIndex(curve) < 0 || !std::isfinite(delta))
        return false;

    PlotCurve *pc = dynamic_cast<PlotCurve *>(curve);
    if (pc && pc->type() == Graph::Function) {
        if (d_dir == Horizontal) {
            if (!ApplicationWindow::isNonInteractiveContext(d_graph))
                QMessageBox::warning(d_graph, tr("Warning"),
                                     tr("This operation cannot be performed on function curves."));
            return false;
        }
        FunctionCurve *func = (FunctionCurve *)curve;
        if (func->functionType() != FunctionCurve::Normal || func->formulas().isEmpty())
            return false;
        QString formula = func->formulas().first();
        if (delta > 0)
            func->setFormula(formula + "+" + QString::number(delta, 'g', 15));
        else
            func->setFormula(formula + QString::number(delta, 'g', 15));
        func->loadData();
        return true;
    }

    DataCurve *c = dynamic_cast<DataCurve *>(curve);
    if (!c || !d_app)
        return false;
    // Horizontal bars plot the Y column along the x axis and the X column along the y axis.
    const bool swapped = (c->type() == Graph::HorizontalBars);
    const bool alongDisplayX = (d_dir == Horizontal);
    const bool writesYColumn = (alongDisplayX == swapped);
    const QString col_name = writesYColumn ? c->title().text() : c->xColumnName();
    Table *tab = c->table();
    if (!tab)
        tab = d_app->table(col_name);
    if (!tab)
        return false;
    int col = tab->colIndex(col_name);
    if (col < 0 || !tab->column(col))
        return false;
    if (tab->columnType(col) != SciDAVis::ColumnMode::Numeric) {
        if (!ApplicationWindow::isNonInteractiveContext(d_graph))
            QMessageBox::warning(d_graph, tr("Warning"),
                                 tr("This operation cannot be performed on curves plotted from "
                                    "columns having a non-numerical format."));
        return false;
    }

    // Each point goes back to the table row it came from (rows with an invalid X or Y are not
    // points of the curve), and the curve shows its data with the stack offset (and possibly the
    // 0-1 normalisation) applied while the table holds the original values. All new values are
    // computed before the first write, so a refused translation leaves the table untouched.
    Column *column = tab->column(col);
    const int n = curve->dataSize();
    QVector<QPair<int, double>> writes;
    writes.reserve(n);
    for (int pt = 0; pt < n; ++pt) {
        const int row = c->tableRow(pt);
        if (row < 0 || row >= column->rowCount() || column->isInvalid(row))
            continue;
        const double dispX = curve->x(pt) + (alongDisplayX ? delta : 0.0);
        const double dispY = curve->y(pt) + (alongDisplayX ? 0.0 : delta);
        double rawX = 0.0;
        double rawY = 0.0;
        // A move along X writes X only: undo just the applied X offset, without requiring the
        // 0-1 normalisation range (it is undefined when all Y values are equal). Only a move along
        // Y has to undo the normalisation.
        if (!c->displayToRaw(dispX, dispY, &rawX, alongDisplayX ? nullptr : &rawY)) {
            emit statusText(tr("This data point cannot be moved: the original values of the 0-1 "
                               "normalized curve cannot be recovered."));
            return false;
        }
        writes.append(qMakePair(row, alongDisplayX ? rawX : rawY));
    }
    if (writes.isEmpty()) {
        // Box plots, histograms...: the points are not table rows, nothing to write back.
        emit statusText(tr("This data point does not correspond to a single table row and "
                           "cannot be edited."));
        return false;
    }

    QUndoStack *undoStack = (d_app->project() && d_app->project()->undoStack())
            ? d_app->project()->undoStack()
            : (tab->d_future_table ? tab->d_future_table->undoStack() : nullptr);
    if (undoStack) {
        undoStack->beginMacro(d_dir == Horizontal ? tr("Translate Curve Horizontally")
                                                  : tr("Translate Curve Vertically"));
    }
    for (const QPair<int, double> &w : writes)
        column->setValueAt(w.first, w.second);
    if (undoStack)
        undoStack->endMacro();
    d_app->updateCurves(tab, col_name);
    d_app->modifiedProject();
    return true;
}
