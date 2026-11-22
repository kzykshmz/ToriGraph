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

#include "I18nJapanese.h"

#include <QLocale>
#include <QMessageBox>

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

namespace {

void registerDifferentiationTranslationsOnce()
{
    static bool s_done = false;
    if (s_done)
        return;
    s_done = true;
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QString::fromUtf8("Cannot differentiate: %1 pair(s) of adjacent points have the same X "
                              "(first at x = %2), which would divide by zero. Remove or average "
                              "the duplicate X values first."),
            QString::fromUtf8("微分できません: 隣り合う点で X が同じ組が %1 組あり (最初は x = %2)、"
                              "0 で割ることになります。重複した X を削除するか平均してから実行してください。"));
}

} // namespace

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

    // Adjacent duplicate X would give inf/NaN slopes: refuse up front with a clear message.
    {
        int nDup = 0;
        double firstDupX = 0.0;
        for (size_t i = 1; i < d_x.size(); ++i) {
            if (d_x[i] == d_x[i - 1]) {
                if (nDup == 0)
                    firstDupX = d_x[i];
                ++nDup;
            }
        }
        if (nDup > 0) {
            registerDifferentiationTranslationsOnce();
            const QString msg =
                    tr("Cannot differentiate: %1 pair(s) of adjacent points have the same X "
                       "(first at x = %2), which would divide by zero. Remove or average "
                       "the duplicate X values first.")
                            .arg(nDup)
                            .arg(firstDupX, 0, 'g', 8);
            app->updateLog(msg + QLatin1Char('\n'));
            if (!isSilentOrNonInteractive())
                QMessageBox::critical(app, tr("ToriGraph") + " - " + tr("Error"), msg);
            d_init_err = true;
            return;
        }
    }

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
    // Interior points: mean of the left and right slopes (Origin's definition). End points:
    // one-sided (forward / backward) difference, so the output keeps all n points.
    const size_t nPts = d_x.size();
    for (size_t i = 0; i < nPts; i++) {
        double dydx;
        if (i == 0)
            dydx = (d_y[1] - d_y[0]) / (d_x[1] - d_x[0]);
        else if (i == nPts - 1)
            dydx = (d_y[i] - d_y[i - 1]) / (d_x[i] - d_x[i - 1]);
        else
            dydx = 0.5
                    * ((d_y[i + 1] - d_y[i]) / (d_x[i + 1] - d_x[i])
                       + (d_y[i] - d_y[i - 1]) / (d_x[i] - d_x[i - 1]));
        xCol->setValueAt(int(i), d_x[i]);
        yCol->setValueAt(int(i), dydx);
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
