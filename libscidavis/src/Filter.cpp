/***************************************************************************
    File                 : Fit.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Abstract base class for data analysis operations

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
#include "Filter.h"
#include "Legend.h"
#include "ColorButton.h"
#include "Table.h"
#include "FunctionCurve.h"
#include "PlotCurve.h"
#include "QwtHistogram.h"
#include "CurveMathTools.h"
#include "future/core/column/Column.h"

#include <QApplication>
#include <QMessageBox>
#include <QLocale>

#include <gsl/gsl_sort.h>

#include <algorithm>
using namespace std;

Filter::Filter(ApplicationWindow *parent, Graph *g, QString name) : QObject(parent)
{
    QObject::setObjectName(name);
    init();
    d_graph = g;
}

Filter::Filter(ApplicationWindow *parent, Table *t, QString name) : QObject(parent)
{
    QObject::setObjectName(name);
    init();
    d_table = t;
}

Filter::~Filter() = default;

void Filter::init()
{
    d_curveColor = ColorButton::color(1);
    d_tolerance = 1e-4;
    d_points = 100;
    d_max_iterations = 1000;
    d_curve = nullptr;
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent());
    d_prec = app ? app->fit_output_precision : 13;
    d_init_err = false;
    d_sort_data = false;
    d_min_points = 2;
    d_explanation = objectName();
    d_graph = nullptr;
    d_table = nullptr;
    d_silent = false;
}

bool Filter::isSilentOrNonInteractive() const
{
    if (d_silent || (qApp && qApp->property("torigraph_noninteractive").toBool()))
        return true;
    if (ApplicationWindow::isNonInteractiveContext(parent()))
        return true;
    if (const ApplicationWindow *app = qobject_cast<const ApplicationWindow *>(parent()))
        return app->isNonInteractive();
    return false;
}

void Filter::setInterval(double from, double to)
{
    if (!d_curve || (d_graph && d_graph->curveIndex(d_curve) < 0)) {
        d_curve = nullptr;
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Please assign a curve first!"));
        return;
    }
    setDataFromCurve(d_curve->title().text(), from, to);
}

void Filter::setDataCurve(int curve, double start, double end)
{
    if (start > end)
        qSwap(start, end);

    d_init_err = false;
    if (!d_graph) {
        d_init_err = true;
        return;
    }
    d_curve = d_graph->curve(curve);
    if ((nullptr == d_curve)|| (d_curve->rtti() != QwtPlotItem::Rtti_PlotCurve)) {
        d_init_err = true;
        return;
    }
    QList<Graph::AxisType> axType = d_graph->axesType();
    for (const auto &type : axType)
        if (Graph::AxisType::Numeric != type) {
            if (isSilentOrNonInteractive())
                break;
            auto ret = QMessageBox::warning((ApplicationWindow *)parent(),
                                            tr("ToriGraph") + " - " + tr("Warning"),
                                            tr("At least one of the axis is not numerical!"),
                                            QMessageBox::Abort | QMessageBox::Ignore);
            if (QMessageBox::Abort == ret) {
                d_init_err = true;
                return;
            } else
                break;
        }

    std::vector<double> temp(d_curve->dataSize());
    for (auto ii=0; d_curve->dataSize() > ii; ++ii )
        temp[ii]=d_curve->x(ii);

    d_indices = getIndices(temp, start, end, d_sort_data);

    // Mask-aware filtering: skip points whose X or Y cell is masked in the source table.
    // Histogram points are bins, not table rows (masked cells are already left out of the
    // counts), so they are never matched to rows here.
    DataCurve *maskDc = dynamic_cast<QwtHistogram *>(d_curve)
            ? nullptr
            : dynamic_cast<DataCurve *>(d_curve);
    if (DataCurve *dc = maskDc) {
        if (Table *tbl = dc->table()) {
            int xi = tbl->colIndex(dc->xColumnName());
            Column *xCol = (xi >= 0) ? tbl->column(xi) : tbl->column(dc->xColumnName());
            int yi = tbl->colIndex(dc->yColumnName());
            Column *yCol = (yi >= 0) ? tbl->column(yi) : tbl->column(dc->yColumnName());
            if (xCol || yCol) {
                std::vector<size_t> unmasked;
                unmasked.reserve(d_indices.size());
                for (size_t idx : d_indices) {
                    int row = dc->tableRow(int(idx));
                    if (row < 0)
                        row = dc->startRow() + int(idx);
                    if (row >= 0 && ((xCol && xCol->isMasked(row)) || (yCol && yCol->isMasked(row))))
                        continue;
                    unmasked.push_back(idx);
                }
                d_indices.swap(unmasked);
            }
        }
    }

    // Filter out non-finite X/Y values so GSL routines never see NaN/Inf
    {
        std::vector<size_t> finiteIndices;
        finiteIndices.reserve(d_indices.size());
        for (size_t idx : d_indices) {
            const double xv = d_curve->x(int(idx));
            const double yv = d_curve->y(int(idx));
            if (std::isfinite(xv) && std::isfinite(yv))
                finiteIndices.push_back(idx);
        }
        d_indices.swap(finiteIndices);
    }

    try
    {
        d_x.resize(d_indices.size());
        d_y.resize(d_indices.size());
    }
    catch (const std::bad_alloc &e) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Could not allocate memory, operation aborted!\n")
                                          + tr("Allocator returned: ") + e.what());
        d_init_err = true;
        return;
    }

    for (size_t ii = 0; d_indices.size() > ii; ++ii)
    {
        d_x[ii] = d_curve->x(static_cast<int>(d_indices[ii]));
        d_y[ii] = d_curve->y(static_cast<int>(d_indices[ii]));
    }

    if (!isDataAcceptable()) {
        d_init_err = true;
        return;
    }

    // ensure range is within data range
    if (!d_x.empty()) {
        auto minMax = std::minmax_element(d_x.cbegin(), d_x.cend());
        d_from = *(minMax.first);
        d_to = *(minMax.second);
    }
    else
    {
        d_init_err = true;
        return;
    }
}

bool Filter::isDataAcceptable() const
{

    if (d_x.size() < unsigned(d_min_points)) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("You need at least %1 points in order to perform this operation!")
                                          .arg(d_min_points));
        return false;
    }
    return true;
}

int Filter::curveIndex(const QString &curveTitle, Graph *const g)
{
    if (curveTitle.isEmpty()) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(), tr("Filter Error"),
                                  tr("Please enter a valid curve name!"));
        d_init_err = true;
        return -1;
    }

    if (g)
        d_graph = g;

    if (!d_graph) {
        d_init_err = true;
        return -1;
    }

    return d_graph->curveIndex(curveTitle);
}

bool Filter::setDataFromCurve(const QString &curveTitle, Graph *const g)
{
    int index = curveIndex(curveTitle, g);
    if (index < 0) {
        d_init_err = true;
        return false;
    }

    d_graph->range(index, d_from, d_to);
    setDataCurve(index, d_from, d_to);
    return !d_init_err;
}

bool Filter::setDataFromCurve(const QString &curveTitle, const double from, const double to,
                              Graph *const g)
{
    int index = curveIndex(curveTitle, g);
    if (index < 0) {
        d_init_err = true;
        return false;
    }

    setDataCurve(index, from, to);
    return !d_init_err;
}

void Filter::setColor(const QString &colorName)
{
    QColor c = QColor(COLORVALUE(colorName));
    if (colorName == "green")
        c = QColor(Qt::green);
    else if (colorName == "darkYellow")
        c = QColor(Qt::darkYellow);
    if (!ColorButton::isValidColor(c)) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Color Name Error"),
                    tr("The color name '%1' is not valid, a default color (red) will be used instead!")
                            .arg(colorName));
        d_curveColor = ColorButton::color(1);
        return;
    }

    d_curveColor = c;
}

void Filter::showLegend()
{
    if (!d_graph)
        return;
    Legend *mrk = d_graph->newLegend(legendInfo());
    if (mrk)
        CurveMathTools::placeCalloutAutoAvoid(d_graph, mrk, 14, false, true);
    d_graph->replot();
}

bool Filter::run()
{
    if (d_init_err || d_points <= 0 || d_x.size() < unsigned(qMax(1, d_min_points)))
        return false;

    const bool nonInteractive = isSilentOrNonInteractive();
    if (!nonInteractive)
        QApplication::setOverrideCursor(Qt::WaitCursor);

    output(); // data analysis and output
    if (ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent()))
        app->updateLog(logInfo());

    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
    return !d_init_err;
}

void Filter::output()
{
    std::vector<double> X(d_points);
    std::vector<double> Y(d_points);

    // do the data analysis
    calculateOutputData(X, Y);

    addResultCurve(X, Y);
}



QwtPlotCurve *Filter::addResultCurve(const std::vector<double> &x, const std::vector<double> &y)
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent());
    if (!app || !d_graph)
        return nullptr;
    const int nPts = qMin(d_points, qMin(int(x.size()), int(y.size())));
    if (nPts <= 0)
        return nullptr;

    const QString tableName = app->generateUniqueName(this->objectName());
    Column *xCol = new Column(tr("1", "filter table x column name"), SciDAVis::ColumnMode::Numeric);
    Column *yCol = new Column(tr("2", "filter table y column name"), SciDAVis::ColumnMode::Numeric);
    xCol->setPlotDesignation(SciDAVis::X);
    yCol->setPlotDesignation(SciDAVis::Y);
    for (int i = 0; i < nPts; i++) {
        xCol->setValueAt(i, x[size_t(i)]);
        yCol->setValueAt(i, y[size_t(i)]);
    }
    const QString srcTitle =
            (d_curve && d_graph->curveIndex(d_curve) >= 0) ? d_curve->title().text() : objectName();
    // first set the values, then add the columns to the table, otherwise, we generate too many undo
    // commands
    Table *t = app->newHiddenTable(tableName,
                                   d_explanation + " " + tr("of") + " " + srcTitle,
                                   QList<Column *>() << xCol << yCol);

    DataCurve *c = new DataCurve(t, tableName + "_" + xCol->name(), tableName + "_" + yCol->name());
    c->setData(x.data(), y.data(), nPts);
    c->setPen(QPen(d_curveColor, 1));
    d_graph->insertPlotItem(c, Graph::Line);
    d_graph->updatePlot();

    return (QwtPlotCurve *)c;
}
