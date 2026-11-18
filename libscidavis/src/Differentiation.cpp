/***************************************************************************
    File                 : Differentiation.cpp
    Project              : SciDAVis / ToriGraph
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Numerical differentiation of data sets

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
#include "Differentiation.h"
#include "MultiLayer.h"
#include "Legend.h"
#include "PlotCurve.h"
#include "core/column/Column.h"
#include "core/datatypes/Double2StringFilter.h"

#include <QLocale>

Differentiation::Differentiation(ApplicationWindow *parent, Graph *g) : Filter(parent, g)
{
    init();
}

Differentiation::Differentiation(ApplicationWindow *parent, Graph *g, const QString &curveTitle)
    : Filter(parent, g)
{
    init();
    setDataFromCurve(curveTitle);
}

Differentiation::Differentiation(ApplicationWindow *parent, Graph *g, const QString &curveTitle,
                                 double start, double end)
    : Filter(parent, g)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

void Differentiation::init()
{
    setObjectName(tr("Derivative"));
    d_min_points = 4;
}

void Differentiation::output()
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent());
    if (!app || d_x.size() < 3 || d_y.size() != d_x.size())
        return;

    const bool curveAlive = (d_curve && (!d_graph || d_graph->curveIndex(d_curve) >= 0));
    const QString curveTitle = curveAlive ? d_curve->title().text() : objectName();

    QString xColLabel = QStringLiteral("x");
    QString yColLabel = curveTitle.isEmpty() ? QStringLiteral("y") : curveTitle;
    QString xLongName;
    QString xUnits;
    QString yUnits;

    if (DataCurve *dc = curveAlive ? dynamic_cast<DataCurve *>(d_curve) : nullptr) {
        if (Table *srcTbl = dc->table()) {
            Column *srcX = srcTbl->column(dc->xColumnName());
            Column *srcY = srcTbl->column(dc->yColumnName());
            if (srcX) {
                const QString ln = srcX->longName().trimmed();
                xLongName = ln.isEmpty() ? srcX->name() : ln;
                xUnits = srcX->units().trimmed();
                xColLabel = (!ln.isEmpty() && ln.size() <= 18 && !ln.contains(QLatin1Char(',')))
                        ? ln
                        : srcX->name();
            }
            if (srcY) {
                const QString ln = srcY->longName().trimmed();
                yUnits = srcY->units().trimmed();
                yColLabel = (!ln.isEmpty() && ln.size() <= 18 && !ln.contains(QLatin1Char(',')))
                        ? ln
                        : srcY->name();
            }
        }
    }
    if (xColLabel.trimmed().isEmpty())
        xColLabel = QStringLiteral("x");
    if (yColLabel.trimmed().isEmpty())
        yColLabel = QStringLiteral("y");

    const QString derivLabel = QStringLiteral("d(%1)/d(%2)").arg(yColLabel, xColLabel);

    Column *xCol =
            new Column(tr("1", "differention table x column name"), SciDAVis::ColumnMode::Numeric);
    Column *yCol =
            new Column(tr("2", "differention table y column name"), SciDAVis::ColumnMode::Numeric);
    xCol->setPlotDesignation(SciDAVis::X);
    yCol->setPlotDesignation(SciDAVis::Y);
    for (unsigned i = 1; i < d_x.size() - 1; i++) {
        xCol->setValueAt(int(i) - 1, d_x[i]);
        yCol->setValueAt(int(i) - 1,
                         0.5
                                 * ((d_y[i + 1] - d_y[i]) / (d_x[i + 1] - d_x[i])
                                    + (d_y[i] - d_y[i - 1]) / (d_x[i] - d_x[i - 1])));
    }

    if (!xLongName.isEmpty())
        xCol->setLongName(xLongName);
    if (!xUnits.isEmpty())
        xCol->setUnits(xUnits);
    yCol->setLongName(derivLabel);
    yCol->setComment(derivLabel);
    if (!yUnits.isEmpty() && (xUnits.isEmpty() || xUnits == QLatin1String("-")))
        yCol->setUnits(yUnits);
    else if (!yUnits.isEmpty() && !xUnits.isEmpty())
        yCol->setUnits(QStringLiteral("%1 / %2").arg(yUnits, xUnits));

    if (auto *d2sX = qobject_cast<Double2StringFilter *>(xCol->outputFilter())) {
        d2sX->setNumericFormat('g');
        d2sX->setNumDigits(6);
    }
    if (auto *d2sY = qobject_cast<Double2StringFilter *>(yCol->outputFilter())) {
        d2sY->setNumericFormat('g');
        d2sY->setNumDigits(6);
    }

    QString tableName = app->generateUniqueName(objectName());
    Table *t = app->newHiddenTable(
            tableName, tr("Derivative") + " " + tr("of", "Derivative of") + " " + curveTitle,
            QList<Column *>() << xCol << yCol);
    MultiLayer *ml = app->newGraph(tr("Plot") + tr("Derivative"));
    if (ml && ml->activeGraph()) {
        Graph *dg = ml->activeGraph();
        dg->insertCurve(t, tableName + "_" + yCol->name(), 0);
        dg->refreshAutoAxisTitlesFromColumns();
        dg->setTitle(QString());
        if (Legend *l = dg->legend())
            l->setText(QStringLiteral("\\c{1}") + derivLabel);
        dg->replot();
    }
}
