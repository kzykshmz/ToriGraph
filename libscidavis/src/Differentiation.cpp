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

#include <QApplication>
#include <QDateTime>
#include <QLocale>
#include <QMessageBox>

#include <algorithm>
#include <numeric>
#include <vector>

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
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QString::fromUtf8("X is not in order (%1 changes of direction in %2 points); the points "
                              "were sorted by X before differentiating."),
            QString::fromUtf8("X が順に並んでいません (%2 点で向きが %1 回変わる)。X の順に並べ替えてから微分しました。"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QString::fromUtf8("X turns back %1 time(s) (first at x = %2); each branch was "
                              "differentiated in row order, and the turning points use the "
                              "one-sided slope of the incoming branch."),
            QString::fromUtf8("X が %1 回折り返しています (最初は x = %2)。枝ごとに行の順で微分し、"
                              "折り返し点は手前の枝の片側差分にしました。"));
}

} // namespace

void Differentiation::init()
{
    setObjectName(tr("Derivative"));
    d_min_points = 4;
    // Take the points in row order; output() decides how to treat X (see there). Sorting by X
    // up front broke scans that go out and back (loading/unloading, CV, hysteresis).
    d_sort_data = false;
}

QString Differentiation::logInfo()
{
    if (d_notice.isEmpty())
        return QString();
    return QStringLiteral("[%1] %2: %3\n")
            .arg(QLocale().toString(QDateTime::currentDateTime()), objectName(), d_notice);
}

void Differentiation::output()
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent());
    if (!app || d_x.size() < 3 || d_y.size() != d_x.size())
        return;

    registerDifferentiationTranslationsOnce();
    d_notice.clear();
    d_error_message.clear();

    // How X runs decides the order of differentiation:
    //  - monotonic (rising or falling): row order, nothing to say;
    //  - a few changes of direction: a scan made of monotonic branches that goes out and back
    //    (loading/unloading, CV, hysteresis), often on the same X grid. Each branch is
    //    differentiated in row order; sorting would interleave the branches (or put equal X
    //    next to each other) and give nonsense slopes;
    //  - many changes of direction: rows in no particular order, sorted by X.
    // The boundary is more than one change per 4 points: a branch then has fewer than 4 points
    // on average, too short to be a sweep, while shuffled rows change direction at about 2 of
    // every 3 points.
    const size_t nPts = d_x.size();
    int nTurns = 0;
    {
        int prevSign = 0;
        for (size_t i = 1; i < nPts; ++i) {
            const double dx = d_x[i] - d_x[i - 1];
            const int sign = (dx > 0.0) ? 1 : (dx < 0.0 ? -1 : 0);
            if (sign == 0)
                continue;
            if (prevSign != 0 && sign != prevSign)
                ++nTurns;
            prevSign = sign;
        }
    }
    if (4 * size_t(nTurns) > nPts) {
        std::vector<size_t> order(nPts);
        std::iota(order.begin(), order.end(), size_t(0));
        std::stable_sort(order.begin(), order.end(),
                         [this](size_t a, size_t b) { return d_x[a] < d_x[b]; });
        std::vector<double> xs(nPts), ys(nPts);
        for (size_t i = 0; i < nPts; ++i) {
            xs[i] = d_x[order[i]];
            ys[i] = d_y[order[i]];
        }
        d_x.swap(xs);
        d_y.swap(ys);
        d_notice = tr("X is not in order (%1 changes of direction in %2 points); the points were "
                      "sorted by X before differentiating.")
                           .arg(nTurns)
                           .arg(int(nPts));
        nTurns = 0;
    }

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
            d_notice.clear();
            const QString msg =
                    tr("Cannot differentiate: %1 pair(s) of adjacent points have the same X "
                       "(first at x = %2), which would divide by zero. Remove or average "
                       "the duplicate X values first.")
                            .arg(nDup)
                            .arg(firstDupX, 0, 'g', 8);
            d_error_message = msg;
            app->updateLog(msg + QLatin1Char('\n'));
            if (!isSilentOrNonInteractive()) {
                // Filter::run() has set the wait cursor: take it off while the message is up
                // (otherwise the box shows a busy cursor), then put it back so run()'s own
                // restoreOverrideCursor() stays balanced.
                int restored = 0;
                while (QApplication::overrideCursor()) {
                    QApplication::restoreOverrideCursor();
                    ++restored;
                }
                QMessageBox::critical(app, tr("ToriGraph") + " - " + tr("Error"), msg);
                for (int k = 0; k < restored; ++k)
                    QApplication::setOverrideCursor(Qt::WaitCursor);
            }
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
    // one-sided (forward / backward) difference, so the output keeps all n points. A turning
    // point of an out-and-back scan ends one branch and starts the next: the mean would mix
    // the slopes of two different branches, so it takes the incoming branch's slope.
    int nTurnPts = 0;
    double firstTurnX = 0.0;
    for (size_t i = 0; i < nPts; i++) {
        const bool turning = nTurns > 0 && i > 0 && i + 1 < nPts
                && (d_x[i] - d_x[i - 1]) * (d_x[i + 1] - d_x[i]) < 0.0;
        if (turning && nTurnPts++ == 0)
            firstTurnX = d_x[i];
        double dydx;
        if (i == 0)
            dydx = (d_y[1] - d_y[0]) / (d_x[1] - d_x[0]);
        else if (i == nPts - 1 || turning)
            dydx = (d_y[i] - d_y[i - 1]) / (d_x[i] - d_x[i - 1]);
        else
            dydx = 0.5
                    * ((d_y[i + 1] - d_y[i]) / (d_x[i + 1] - d_x[i])
                       + (d_y[i] - d_y[i - 1]) / (d_x[i] - d_x[i - 1]));
        xCol->setValueAt(int(i), d_x[i]);
        yCol->setValueAt(int(i), dydx);
    }
    if (nTurnPts > 0)
        d_notice = tr("X turns back %1 time(s) (first at x = %2); each branch was differentiated "
                      "in row order, and the turning points use the one-sided slope of the "
                      "incoming branch.")
                           .arg(nTurnPts)
                           .arg(firstTurnX, 0, 'g', 8);

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
