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
    d_sub_tool = new ScreenPickerTool(d_graph, this, SIGNAL(statusText(const QString &)));
    connect((ScreenPickerTool *)d_sub_tool, SIGNAL(selected(const QwtDoublePoint &)), this,
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

    if (((PlotCurve *)d_selected_curve)->type() == Graph::Function) {
        if (d_dir == Horizontal) {
            if (!ApplicationWindow::isNonInteractiveContext(d_graph))
                QMessageBox::warning(d_graph, tr("Warning"),
                                     tr("This operation cannot be performed on function curves."));
        } else {
            FunctionCurve *func = (FunctionCurve *)d_selected_curve;
            if (func->functionType() == FunctionCurve::Normal && !func->formulas().isEmpty()) {
                QString formula = func->formulas().first();
                double d = point.y() - d_curve_point.y();
                if (d > 0)
                    func->setFormula(formula + "+" + QString::number(d, 'g', 15));
                else
                    func->setFormula(formula + QString::number(d, 'g', 15));
                func->loadData();
            }
        }
        d_graph->setActiveTool(NULL);
        return;
    } else {
        DataCurve *c = dynamic_cast<DataCurve *>(d_selected_curve);
        if (!c || !d_app) {
            d_graph->setActiveTool(NULL);
            return;
        }
        double d;
        QString col_name;
        switch (d_dir) {
        case Vertical: {
            col_name = c->title().text();
            d = point.y() - d_curve_point.y();
            break;
        }
        case Horizontal: {
            col_name = c->xColumnName();
            d = point.x() - d_curve_point.x();
            break;
        }
        default:
            throw runtime_error("Invalid direction");
        }
        Table *tab = d_app->table(col_name);
        if (!tab) {
            d_graph->setActiveTool(NULL);
            return;
        }
        int col = tab->colIndex(col_name);
        if (col < 0 || !tab->column(col)) {
            d_graph->setActiveTool(NULL);
            return;
        }
        if (tab->columnType(col) != SciDAVis::ColumnMode::Numeric) {
            if (!ApplicationWindow::isNonInteractiveContext(d_graph))
                QMessageBox::warning(d_graph, tr("Warning"),
                                     tr("This operation cannot be performed on curves plotted from "
                                        "columns having a non-numerical format."));
            d_graph->setActiveTool(NULL);
            return;
        }

        const int row_start = qMax(0, c->tableRow(0));
        const int row_end = qMin(tab->column(col)->rowCount(), row_start + c->dataSize());
        QUndoStack *undoStack = (d_app && d_app->project() && d_app->project()->undoStack())
                ? d_app->project()->undoStack()
                : (tab->d_future_table ? tab->d_future_table->undoStack() : nullptr);
        if (undoStack) {
            undoStack->beginMacro(d_dir == Horizontal ? tr("Translate Curve Horizontally")
                                                      : tr("Translate Curve Vertically"));
        }
        for (int i = row_start; i < row_end; i++) {
            const int ptIdx = i - row_start;
            if (!tab->column(col)->isInvalid(i) && ptIdx >= 0 && ptIdx < d_selected_curve->dataSize())
                tab->column(col)->setValueAt(
                        i,
                        (d_dir == Horizontal ? d_selected_curve->x(ptIdx)
                                             : d_selected_curve->y(ptIdx))
                                + d);
        }
        if (undoStack)
            undoStack->endMacro();
        d_app->updateCurves(tab, col_name);
        d_app->modifiedProject();
        d_graph->setActiveTool(NULL);
        // attention: I'm now deleted. Maybe there is a cleaner solution...*/
    }
}
