/***************************************************************************
    File                 : CurveMathTools.cpp
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Description          : OriginPro Curve Math, Multi-Curve Averaging,
                           Curve Normalization, Intersections, X/Y Lookup,
                           Polygon Loop Area & Arc Length
 ***************************************************************************/

#include "CurveMathTools.h"

#include "ApplicationWindow.h"
#include "ArrowMarker.h"
#include "Graph.h"
#include "I18nJapanese.h"
#include "Legend.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "Table.h"
#include "core/column/Column.h"
#include "core/datatypes/Double2StringFilter.h"

#include <gsl/gsl_cdf.h>

#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>
#include <qwt_scale_map.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>
#include <qwt_text.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace {

inline QString trMath(const char *text)
{
    return QCoreApplication::translate("CurveMathTools", text);
}

QwtPlotCurve *resolveCurve(Graph *g, const QString &curveTitle)
{
    if (!g || curveTitle.isEmpty())
        return nullptr;

    if (QwtPlotCurve *exact = g->curve(curveTitle))
        return exact;

    for (int i = 0; i < g->curves(); ++i) {
        QwtPlotCurve *c = g->curve(i);
        if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;
        if (c->title().text() == curveTitle)
            return c;
    }

    for (int i = 0; i < g->curves(); ++i) {
        QwtPlotCurve *c = g->curve(i);
        if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;
        if (auto *dc = dynamic_cast<DataCurve *>(c)) {
            if (Table *tbl = dc->table()) {
                const QString expected = tbl->name() + QStringLiteral("_") + curveTitle;
                if (dc->yColumnName() == expected || dc->yColumnName() == curveTitle)
                    return c;
            }
        }
    }

    for (int i = 0; i < g->curves(); ++i) {
        QwtPlotCurve *c = g->curve(i);
        if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;
        if (c->title().text().compare(curveTitle, Qt::CaseInsensitive) == 0)
            return c;
    }

    for (int i = 0; i < g->curves(); ++i) {
        QwtPlotCurve *c = g->curve(i);
        if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;
        if (auto *dc = dynamic_cast<DataCurve *>(c)) {
            if (Table *tbl = dc->table()) {
                const QString expected = tbl->name() + QStringLiteral("_") + curveTitle;
                if (dc->yColumnName().compare(expected, Qt::CaseInsensitive) == 0
                    || dc->yColumnName().compare(curveTitle, Qt::CaseInsensitive) == 0)
                    return c;
            }
        }
    }

    const QString suffix = QStringLiteral("_") + curveTitle;
    for (int i = 0; i < g->curves(); ++i) {
        QwtPlotCurve *c = g->curve(i);
        if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;
        const auto *dc = dynamic_cast<const DataCurve *>(c);
        if (dc && dc->table())
            continue;
        const QString title = c->title().text();
        if (title.endsWith(suffix) || title.endsWith(suffix, Qt::CaseInsensitive))
            return c;
    }
    return nullptr;
}

bool extractUnmaskedCurvePoints(Graph *g, const QString &curveTitle,
                                std::vector<double> &xOut, std::vector<double> &yOut,
                                bool sortByX, bool mergeDuplicateX = false)
{
    xOut.clear();
    yOut.clear();

    QwtPlotCurve *c = resolveCurve(g, curveTitle);
    if (!c || c->dataSize() <= 0)
        return false;

    DataCurve *dc = dynamic_cast<DataCurve *>(c);
    Table *tbl = dc ? dc->table() : nullptr;
    Column *xCol = (tbl && dc) ? tbl->column(dc->xColumnName()) : nullptr;
    Column *yCol = (tbl && dc) ? tbl->column(dc->yColumnName()) : nullptr;

    const int n = c->dataSize();
    std::vector<std::pair<double, double>> pts;
    pts.reserve(static_cast<size_t>(n));

    for (int i = 0; i < n; ++i) {
        if (dc && tbl) {
            int row = dc->tableRow(i);
            if (row < 0 && yCol && n == yCol->rowCount())
                row = i;
            if (row >= 0) {
                if ((xCol && xCol->isMasked(row)) || (yCol && yCol->isMasked(row)))
                    continue;
            }
        }
        const double xv = c->x(i);
        const double yv = c->y(i);
        if (!std::isfinite(xv) || !std::isfinite(yv))
            continue;
        pts.emplace_back(xv, yv);
    }

    if (pts.empty())
        return false;

    if (sortByX) {
        std::stable_sort(pts.begin(), pts.end(),
                         [](const std::pair<double, double> &a,
                            const std::pair<double, double> &b) { return a.first < b.first; });

        if (mergeDuplicateX && pts.size() >= 2) {
            const double span = std::max(1.0, std::fabs(pts.back().first - pts.front().first));
            const double eps = 1e-13 * span;
            std::vector<std::pair<double, double>> merged;
            merged.reserve(pts.size());
            size_t i = 0;
            while (i < pts.size()) {
                double sumX = pts[i].first;
                double sumY = pts[i].second;
                int cnt = 1;
                size_t j = i + 1;
                while (j < pts.size() && std::fabs(pts[j].first - pts[i].first) <= eps) {
                    sumX += pts[j].first;
                    sumY += pts[j].second;
                    ++cnt;
                    ++j;
                }
                merged.emplace_back(sumX / cnt, sumY / cnt);
                i = j;
            }
            pts.swap(merged);
        }
    }

    xOut.resize(pts.size());
    yOut.resize(pts.size());
    for (size_t i = 0; i < pts.size(); ++i) {
        xOut[i] = pts[i].first;
        yOut[i] = pts[i].second;
    }
    return true;
}

double interpLinearSorted(const std::vector<double> &xs, const std::vector<double> &ys, double xq)
{
    const size_t n = xs.size();
    if (n == 0)
        return std::numeric_limits<double>::quiet_NaN();
    if (n == 1 || xq <= xs.front())
        return ys.front();
    if (xq >= xs.back())
        return ys.back();

    auto it = std::lower_bound(xs.begin(), xs.end(), xq);
    if (it == xs.begin())
        return ys.front();
    if (it != xs.end() && *it == xq)
        return ys[static_cast<size_t>(it - xs.begin())];

    const size_t hi = static_cast<size_t>(it - xs.begin());
    const size_t lo = hi - 1;
    const double dx = xs[hi] - xs[lo];
    if (std::fabs(dx) <= 1e-300)
        return 0.5 * (ys[lo] + ys[hi]);
    const double t = (xq - xs[lo]) / dx;
    return ys[lo] + t * (ys[hi] - ys[lo]);
}

QString makeUniqueTableName(ApplicationWindow *app, const QString &prefix)
{
    QString cleanPrefix = prefix;
    cleanPrefix.replace('_', "");
    QString candidate = app->generateUniqueName(cleanPrefix);
    while (app->alreadyUsedName(candidate))
        candidate = app->generateUniqueName(cleanPrefix);
    return candidate;
}

QString sanitizeColLabel(const QString &raw)
{
    QString out = raw;
    out.replace('_', '-');
    if (out.isEmpty())
        out = "Y";
    return out;
}

Table *createNumericTable(ApplicationWindow *app, const QString &prefix,
                          const QStringList &colLabels,
                          const QList<SciDAVis::PlotDesignation> &designations,
                          const std::vector<std::vector<double>> &colsData)
{
    if (!app || colsData.empty() || colsData[0].empty())
        return nullptr;

    const int nRows = static_cast<int>(colsData[0].size());
    const int nCols = static_cast<int>(colsData.size());
    const QString tableName = makeUniqueTableName(app, prefix);

    Table *t = app->newTable(tableName, nRows, nCols);
    if (!t)
        return nullptr;

    for (int c = 0; c < nCols; ++c) {
        const QString lbl = (c < colLabels.size()) ? sanitizeColLabel(colLabels[c])
                                                   : QString("Col%1").arg(c + 1);
        t->setColName(c, lbl);
        const SciDAVis::PlotDesignation pd =
                (c < designations.size()) ? designations[c]
                                          : (c == 0 ? SciDAVis::X : SciDAVis::Y);
        t->setColPlotDesignation(c, pd);

        if (Column *colPtr = t->column(c)) {
            QVector<qreal> vals(nRows);
            for (int r = 0; r < nRows; ++r)
                vals[r] = colsData[static_cast<size_t>(c)][static_cast<size_t>(r)];
            colPtr->replaceValues(0, vals);
            if (auto *d2s = qobject_cast<Double2StringFilter *>(colPtr->outputFilter())) {
                d2s->setNumericFormat('g');
                d2s->setNumDigits(6);
            }
        } else {
            for (int r = 0; r < nRows; ++r)
                t->setCell(r, c, colsData[static_cast<size_t>(c)][static_cast<size_t>(r)]);
        }
    }
    return t;
}

QwtPlotCurve *insertStyledCurve(Graph *g, Table *t, int xColIdx, int yColIdx, int nRows,
                                const QPen &pen)
{
    if (!g || !t || nRows <= 0)
        return nullptr;
    if (!g->insertCurve(t, t->colName(xColIdx), t->colName(yColIdx), Graph::Line, 0, nRows - 1))
        return nullptr;
    QwtPlotCurve *curve = g->curve(g->curves() - 1);
    if (curve)
        curve->setPen(pen);
    return curve;
}

} // namespace

QwtPlotCurve *CurveMathTools::simpleCurveMath(ApplicationWindow *app, Graph *g,
                                              const QString &curveA, const QString &curveB,
                                              MathOp op, bool useUnionGrid)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> xA, yA, xB, yB;
    if (!extractUnmaskedCurvePoints(g, curveA, xA, yA, true, true)
        || !extractUnmaskedCurvePoints(g, curveB, xB, yB, true, true))
        return nullptr;

    if (xA.empty() || xB.empty())
        return nullptr;

    const double xLo = std::max(xA.front(), xB.front());
    const double xHi = std::min(xA.back(), xB.back());
    const double span = std::max(1.0, std::max(xA.back() - xA.front(), xB.back() - xB.front()));
    const double tol = 1e-12 * span;

    if (xLo > xHi + tol)
        return nullptr;

    std::vector<double> targetX;
    if (!useUnionGrid) {
        for (double xv : xA) {
            if (xv >= xLo - tol && xv <= xHi + tol)
                targetX.push_back(std::clamp(xv, xLo, xHi));
        }
    }

    if (useUnionGrid || targetX.size() < 2) {
        targetX.clear();
        for (double xv : xA) {
            if (xv >= xLo - tol && xv <= xHi + tol)
                targetX.push_back(std::clamp(xv, xLo, xHi));
        }
        for (double xv : xB) {
            if (xv >= xLo - tol && xv <= xHi + tol)
                targetX.push_back(std::clamp(xv, xLo, xHi));
        }
        if (targetX.size() < 2 && xHi > xLo) {
            targetX.push_back(xLo);
            targetX.push_back(xHi);
        }
        std::sort(targetX.begin(), targetX.end());
        std::vector<double> dedup;
        dedup.reserve(targetX.size());
        for (double xv : targetX) {
            if (dedup.empty() || std::fabs(xv - dedup.back()) > tol)
                dedup.push_back(xv);
        }
        targetX.swap(dedup);
    }

    std::vector<double> outX, outY;
    outX.reserve(targetX.size());
    outY.reserve(targetX.size());

    for (double xv : targetX) {
        const double va = interpLinearSorted(xA, yA, xv);
        const double vb = interpLinearSorted(xB, yB, xv);
        double res = 0.0;
        switch (op) {
        case Add:
            res = va + vb;
            break;
        case Subtract:
            res = va - vb;
            break;
        case Multiply:
            res = va * vb;
            break;
        case Divide:
            if (std::fabs(vb) <= 1e-300)
                continue;
            res = va / vb;
            break;
        }
        if (std::isfinite(res)) {
            outX.push_back(xv);
            outY.push_back(res);
        }
    }

    if (outX.empty())
        return nullptr;

    QString opTag = "Add";
    switch (op) {
    case Add:
        opTag = "Add";
        break;
    case Subtract:
        opTag = "Sub";
        break;
    case Multiply:
        opTag = "Mul";
        break;
    case Divide:
        opTag = "Div";
        break;
    }

    Table *t = createNumericTable(app, "CurveMath",
                                  QStringList() << "X" << ("Math" + opTag),
                                  QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                                  { outX, outY });
    if (!t)
        return nullptr;

    QwtPlotCurve *resCurve = insertStyledCurve(g, t, 0, 1, static_cast<int>(outX.size()),
                                               QPen(QColor(0, 114, 178), 2.0, Qt::SolidLine));
    g->updatePlot();

    app->updateLog(trMath("[%1] Curve Math (%2): %3 vs %4 -> %5 (%6 pts)\n")
                           .arg(QLocale().toString(QDateTime::currentDateTime()), opTag, curveA,
                                curveB, t->name())
                           .arg(outX.size()));
    return resCurve;
}

QwtPlotCurve *CurveMathTools::subtractStraightLine(ApplicationWindow *app, Graph *g,
                                                   const QString &curveTitle,
                                                   QPointF p1, QPointF p2, bool inPlace)
{
    if (!app || !g)
        return nullptr;

    QwtPlotCurve *c = resolveCurve(g, curveTitle);
    if (!c)
        return nullptr;

    const double dx = p2.x() - p1.x();
    const bool hasSlope = std::fabs(dx) > 1e-15;
    const double slope = hasSlope ? ((p2.y() - p1.y()) / dx) : 0.0;
    const double yConst = 0.5 * (p1.y() + p2.y());

    auto evalLine = [&](double xv) -> double {
        return hasSlope ? (p1.y() + slope * (xv - p1.x())) : yConst;
    };

    if (inPlace) {
        DataCurve *dc = dynamic_cast<DataCurve *>(c);
        Table *tbl = dc ? dc->table() : nullptr;
        Column *xCol = (tbl && dc) ? tbl->column(dc->xColumnName()) : nullptr;
        Column *yCol = (tbl && dc) ? tbl->column(dc->yColumnName()) : nullptr;
        if (dc && tbl && yCol) {
            const int n = dc->dataSize();
            for (int i = 0; i < n; ++i) {
                int row = dc->tableRow(i);
                if (row < 0 && n == yCol->rowCount())
                    row = i;
                if (row < 0 || row >= yCol->rowCount())
                    continue;
                if ((xCol && xCol->isMasked(row)) || yCol->isMasked(row))
                    continue;
                const double xv = dc->x(i);
                const double yv = dc->y(i);
                if (std::isfinite(xv) && std::isfinite(yv))
                    yCol->setValueAt(row, yv - evalLine(xv));
            }
            dc->loadData();
            g->updatePlot();
            return dc;
        }
    }

    std::vector<double> xs, ys;
    if (!extractUnmaskedCurvePoints(g, curveTitle, xs, ys, false, false) || xs.empty())
        return nullptr;

    std::vector<double> outY(xs.size());
    for (size_t i = 0; i < xs.size(); ++i)
        outY[i] = ys[i] - evalLine(xs[i]);

    Table *t = createNumericTable(app, "SubLine",
                                  QStringList() << "X" << "SubtractedY",
                                  QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                                  { xs, outY });
    if (!t)
        return nullptr;

    QwtPlotCurve *resCurve = insertStyledCurve(g, t, 0, 1, static_cast<int>(xs.size()),
                                               QPen(QColor(0, 158, 115), 2.0, Qt::SolidLine));
    g->updatePlot();
    return resCurve;
}

QwtPlotCurve *CurveMathTools::averageMultipleCurves(ApplicationWindow *app, Graph *g,
                                                    const QStringList &curveTitles,
                                                    int numPoints, bool addSdBand,
                                                    Table **outTable)
{
    if (outTable)
        *outTable = nullptr;
    if (!app || !g)
        return nullptr;

    const QStringList names = curveTitles.isEmpty() ? g->analysableCurvesList() : curveTitles;
    if (names.isEmpty())
        return nullptr;

    struct CurveSeries {
        std::vector<double> x;
        std::vector<double> y;
    };
    std::vector<CurveSeries> series;
    series.reserve(static_cast<size_t>(names.size()));

    for (const QString &name : names) {
        CurveSeries cs;
        if (extractUnmaskedCurvePoints(g, name, cs.x, cs.y, true, true) && cs.x.size() >= 2)
            series.push_back(std::move(cs));
    }

    if (series.empty())
        return nullptr;

    double commonMinX = series[0].x.front();
    double commonMaxX = series[0].x.back();
    double unionMinX = series[0].x.front();
    double unionMaxX = series[0].x.back();

    for (size_t m = 1; m < series.size(); ++m) {
        commonMinX = std::max(commonMinX, series[m].x.front());
        commonMaxX = std::min(commonMaxX, series[m].x.back());
        unionMinX = std::min(unionMinX, series[m].x.front());
        unionMaxX = std::max(unionMaxX, series[m].x.back());
    }

    const bool useCommonRange = (commonMaxX > commonMinX);
    const double xStart = useCommonRange ? commonMinX : unionMinX;
    const double xEnd = useCommonRange ? commonMaxX : unionMaxX;
    if (!(xEnd > xStart))
        return nullptr;

    const int nGrid = std::max(2, numPoints);
    const double step = (xEnd - xStart) / static_cast<double>(nGrid - 1);
    const double rangeTol = 1e-12 * std::max(1.0, xEnd - xStart);

    std::vector<double> gridX, meanY, sdY, plusSdY, minusSdY;
    gridX.reserve(static_cast<size_t>(nGrid));
    meanY.reserve(static_cast<size_t>(nGrid));
    sdY.reserve(static_cast<size_t>(nGrid));
    plusSdY.reserve(static_cast<size_t>(nGrid));
    minusSdY.reserve(static_cast<size_t>(nGrid));

    std::vector<double> vals;
    vals.reserve(series.size());

    for (int k = 0; k < nGrid; ++k) {
        const double xq = (k == nGrid - 1) ? xEnd : (xStart + k * step);
        vals.clear();
        for (const CurveSeries &cs : series) {
            if (useCommonRange
                || (xq >= cs.x.front() - rangeTol && xq <= cs.x.back() + rangeTol)) {
                const double v = interpLinearSorted(cs.x, cs.y, xq);
                if (std::isfinite(v))
                    vals.push_back(v);
            }
        }
        if (vals.empty())
            continue;

        const double cnt = static_cast<double>(vals.size());
        const double mu = std::accumulate(vals.begin(), vals.end(), 0.0) / cnt;
        double sd = 0.0;
        if (vals.size() >= 2) {
            double sqSum = 0.0;
            for (double v : vals) {
                const double d = v - mu;
                sqSum += d * d;
            }
            sd = std::sqrt(sqSum / (cnt - 1.0));
        }

        gridX.push_back(xq);
        meanY.push_back(mu);
        sdY.push_back(sd);
        plusSdY.push_back(mu + sd);
        minusSdY.push_back(mu - sd);
    }

    if (gridX.size() < 2)
        return nullptr;

    QStringList colLabels;
    colLabels << "X" << "Mean" << "SD" << "Mean+SD" << "Mean-SD";
    QList<SciDAVis::PlotDesignation> pds;
    pds << SciDAVis::X << SciDAVis::Y << SciDAVis::yErr << SciDAVis::Y << SciDAVis::Y;

    Table *t = createNumericTable(app, "AvgCurves", colLabels, pds,
                                  { gridX, meanY, sdY, plusSdY, minusSdY });
    if (!t)
        return nullptr;

    if (outTable)
        *outTable = t;

    const int nOut = static_cast<int>(gridX.size());
    const QColor mainColor(213, 94, 0);
    QwtPlotCurve *meanCurve =
            insertStyledCurve(g, t, 0, 1, nOut, QPen(mainColor, 2.2, Qt::SolidLine));

    if (addSdBand) {
        const QPen bandPen(QColor(213, 94, 0, 190), 1.4, Qt::DashLine);
        insertStyledCurve(g, t, 0, 3, nOut, bandPen);
        insertStyledCurve(g, t, 0, 4, nOut, bandPen);
    }

    g->updatePlot();
    app->updateLog(trMath("[%1] Averaged %2 curves -> %3 (%4 grid points)\n")
                           .arg(QLocale().toString(QDateTime::currentDateTime()))
                           .arg(series.size())
                           .arg(t->name())
                           .arg(nOut));
    return meanCurve;
}

QwtPlotCurve *CurveMathTools::normalizeCurve(ApplicationWindow *app, Graph *g,
                                             const QString &curveTitle, NormMode mode,
                                             double refX)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> xs, ys;
    if (!extractUnmaskedCurvePoints(g, curveTitle, xs, ys, true, false) || xs.empty())
        return nullptr;

    const size_t n = ys.size();
    std::vector<double> outY(n, 0.0);

    const auto minMaxIt = std::minmax_element(ys.begin(), ys.end());
    const double yMin = *minMaxIt.first;
    const double yMax = *minMaxIt.second;
    const double span = yMax - yMin;

    switch (mode) {
    case DivideByMax: {
        double denom = yMax;
        if (std::fabs(denom) <= 1e-300) {
            double maxAbs = 0.0;
            for (double v : ys) {
                if (std::fabs(v) > std::fabs(maxAbs))
                    maxAbs = v;
            }
            denom = maxAbs;
        }
        if (std::fabs(denom) > 1e-300) {
            for (size_t i = 0; i < n; ++i)
                outY[i] = ys[i] / denom;
        }
        break;
    }
    case Range0To1: {
        if (span > 1e-300) {
            for (size_t i = 0; i < n; ++i)
                outY[i] = (ys[i] - yMin) / span;
        }
        break;
    }
    case RangeMinus1To1: {
        if (span > 1e-300) {
            for (size_t i = 0; i < n; ++i)
                outY[i] = 2.0 * (ys[i] - yMin) / span - 1.0;
        }
        break;
    }
    case ZScore: {
        const double mean = std::accumulate(ys.begin(), ys.end(), 0.0) / static_cast<double>(n);
        double sqSum = 0.0;
        for (double v : ys) {
            const double d = v - mean;
            sqSum += d * d;
        }
        const double sd = (n >= 2) ? std::sqrt(sqSum / static_cast<double>(n - 1)) : 0.0;
        if (sd > 1e-300) {
            for (size_t i = 0; i < n; ++i)
                outY[i] = (ys[i] - mean) / sd;
        }
        break;
    }
    case UnitArea: {
        double area = 0.0;
        double absArea = 0.0;
        for (size_t i = 0; i + 1 < n; ++i) {
            const double dx = xs[i + 1] - xs[i];
            area += 0.5 * (ys[i] + ys[i + 1]) * dx;
            absArea += 0.5 * (std::fabs(ys[i]) + std::fabs(ys[i + 1])) * std::fabs(dx);
        }
        const double denom = (std::fabs(area) > 1e-300) ? std::fabs(area) : absArea;
        if (denom > 1e-300) {
            for (size_t i = 0; i < n; ++i)
                outY[i] = ys[i] / denom;
        }
        break;
    }
    case SumTo100: {
        double sum = std::accumulate(ys.begin(), ys.end(), 0.0);
        if (std::fabs(sum) <= 1e-300) {
            for (double v : ys)
                sum += std::fabs(v);
        }
        if (std::fabs(sum) > 1e-300) {
            for (size_t i = 0; i < n; ++i)
                outY[i] = 100.0 * ys[i] / sum;
        }
        break;
    }
    case DivideByRefX: {
        std::vector<double> xm, ym;
        extractUnmaskedCurvePoints(g, curveTitle, xm, ym, true, true);
        const double refVal = interpLinearSorted(xm, ym, refX);
        if (std::isfinite(refVal) && std::fabs(refVal) > 1e-300) {
            for (size_t i = 0; i < n; ++i)
                outY[i] = ys[i] / refVal;
        }
        break;
    }
    }

    Table *t = createNumericTable(app, "NormCurve",
                                  QStringList() << "X" << "NormalizedY",
                                  QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                                  { xs, outY });
    if (!t)
        return nullptr;

    QwtPlotCurve *resCurve = insertStyledCurve(g, t, 0, 1, static_cast<int>(n),
                                               QPen(QColor(204, 121, 167), 2.0, Qt::SolidLine));
    g->updatePlot();
    return resCurve;
}

QVector<QPointF> CurveMathTools::findCurveIntersections(Graph *g, const QString &curveA,
                                                        const QString &curveB, bool markOnGraph,
                                                        ApplicationWindow *app, Table **outTable)
{
    if (outTable)
        *outTable = nullptr;

    QVector<QPointF> result;
    if (!g)
        return result;

    std::vector<double> xA, yA, xB, yB;
    if (!extractUnmaskedCurvePoints(g, curveA, xA, yA, false, false)
        || !extractUnmaskedCurvePoints(g, curveB, xB, yB, false, false))
        return result;

    if (xA.size() < 2 || xB.size() < 2)
        return result;

    double minX = xA[0], maxX = xA[0], minY = yA[0], maxY = yA[0];
    auto expandBounds = [&](const std::vector<double> &xs, const std::vector<double> &ys) {
        for (size_t i = 0; i < xs.size(); ++i) {
            minX = std::min(minX, xs[i]);
            maxX = std::max(maxX, xs[i]);
            minY = std::min(minY, ys[i]);
            maxY = std::max(maxY, ys[i]);
        }
    };
    expandBounds(xA, yA);
    expandBounds(xB, yB);

    const double spanX = std::max(1.0, maxX - minX);
    const double spanY = std::max(1.0, maxY - minY);
    const double epsParam = 1e-11;
    const double dedupX = 1e-9 * spanX;
    const double dedupY = 1e-9 * spanY;

    std::vector<QPointF> rawPts;

    auto pointOnSegment = [&](double px, double py, double x1, double y1, double x2, double y2) {
        if (px < std::min(x1, x2) - dedupX || px > std::max(x1, x2) + dedupX)
            return false;
        if (py < std::min(y1, y2) - dedupY || py > std::max(y1, y2) + dedupY)
            return false;
        const double cross = (px - x1) * (y2 - y1) - (py - y1) * (x2 - x1);
        return std::fabs(cross) <= 1e-11 * spanX * spanY;
    };

    for (size_t i = 0; i + 1 < xA.size(); ++i) {
        const double ax1 = xA[i], ay1 = yA[i];
        const double ax2 = xA[i + 1], ay2 = yA[i + 1];
        const double aMinX = std::min(ax1, ax2) - dedupX;
        const double aMaxX = std::max(ax1, ax2) + dedupX;
        const double aMinY = std::min(ay1, ay2) - dedupY;
        const double aMaxY = std::max(ay1, ay2) + dedupY;
        const double rX = ax2 - ax1;
        const double rY = ay2 - ay1;

        for (size_t j = 0; j + 1 < xB.size(); ++j) {
            const double bx1 = xB[j], by1 = yB[j];
            const double bx2 = xB[j + 1], by2 = yB[j + 1];

            if (std::max(bx1, bx2) < aMinX || std::min(bx1, bx2) > aMaxX)
                continue;
            if (std::max(by1, by2) < aMinY || std::min(by1, by2) > aMaxY)
                continue;

            const double sX = bx2 - bx1;
            const double sY = by2 - by1;
            const double denom = rX * sY - rY * sX;
            const double qpX = bx1 - ax1;
            const double qpY = by1 - ay1;

            if (std::fabs(denom) > 1e-14 * spanX * spanY) {
                const double t = (qpX * sY - qpY * sX) / denom;
                const double u = (qpX * rY - qpY * rX) / denom;
                if (t >= -epsParam && t <= 1.0 + epsParam && u >= -epsParam
                    && u <= 1.0 + epsParam) {
                    const double tc = std::clamp(t, 0.0, 1.0);
                    rawPts.emplace_back(ax1 + tc * rX, ay1 + tc * rY);
                }
            } else {
                // Parallel / collinear endpoint touch check
                if (pointOnSegment(ax1, ay1, bx1, by1, bx2, by2))
                    rawPts.emplace_back(ax1, ay1);
                if (pointOnSegment(ax2, ay2, bx1, by1, bx2, by2))
                    rawPts.emplace_back(ax2, ay2);
                if (pointOnSegment(bx1, by1, ax1, ay1, ax2, ay2))
                    rawPts.emplace_back(bx1, by1);
                if (pointOnSegment(bx2, by2, ax1, ay1, ax2, ay2))
                    rawPts.emplace_back(bx2, by2);
            }
        }
    }

    std::sort(rawPts.begin(), rawPts.end(), [](const QPointF &a, const QPointF &b) {
        if (std::fabs(a.x() - b.x()) > 1e-12)
            return a.x() < b.x();
        return a.y() < b.y();
    });

    for (const QPointF &pt : rawPts) {
        bool duplicate = false;
        for (const QPointF &existing : result) {
            if (std::fabs(pt.x() - existing.x()) <= dedupX
                && std::fabs(pt.y() - existing.y()) <= dedupY) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            result.append(pt);
    }

    if (app && (!result.isEmpty() || outTable)) {
        std::vector<double> ix(static_cast<size_t>(result.size()));
        std::vector<double> iy(static_cast<size_t>(result.size()));
        for (int k = 0; k < result.size(); ++k) {
            ix[static_cast<size_t>(k)] = result[k].x();
            iy[static_cast<size_t>(k)] = result[k].y();
        }
        if (!ix.empty()) {
            Table *t = createNumericTable(
                    app, "Intersections", QStringList() << "IntersectX" << "IntersectY",
                    QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y, { ix, iy });
            if (outTable)
                *outTable = t;
        }
    }

    if (markOnGraph && g->plotWidget() && !result.isEmpty()) {
        for (const QPointF &pt : result) {
            QwtPlotMarker *mrk = new QwtPlotMarker();
            mrk->setLineStyle(QwtPlotMarker::NoLine);
            mrk->setSymbol(QwtSymbol(QwtSymbol::Diamond, QBrush(QColor(220, 30, 30)),
                                     QPen(QColor(255, 255, 255), 1.5), QSize(10, 10)));
            mrk->setValue(pt.x(), pt.y());
            mrk->attach(g->plotWidget());
        }
        g->replot();
    }

    return result;
}

QVector<double> CurveMathTools::findXFromY(Graph *g, const QString &curveTitle, double targetY)
{
    QVector<double> roots;
    if (!g || !std::isfinite(targetY))
        return roots;

    std::vector<double> xs, ys;
    if (!extractUnmaskedCurvePoints(g, curveTitle, xs, ys, false, false) || xs.empty())
        return roots;

    if (xs.size() == 1) {
        if (std::fabs(ys[0] - targetY) <= 1e-12 * std::max(1.0, std::fabs(targetY)))
            roots.append(xs[0]);
        return roots;
    }

    const auto xMM = std::minmax_element(xs.begin(), xs.end());
    const auto yMM = std::minmax_element(ys.begin(), ys.end());
    const double spanX = std::max(1.0, *xMM.second - *xMM.first);
    const double spanY = std::max(1.0, *yMM.second - *yMM.first);
    const double yTol = 1e-12 * spanY;
    const double xDedup = 1e-9 * spanX;

    std::vector<double> rawX;
    for (size_t i = 0; i + 1 < xs.size(); ++i) {
        const double d0 = ys[i] - targetY;
        const double d1 = ys[i + 1] - targetY;
        if (std::fabs(d0) <= yTol && std::fabs(d1) <= yTol) {
            rawX.push_back(0.5 * (xs[i] + xs[i + 1]));
        } else if (std::fabs(d0) <= yTol) {
            rawX.push_back(xs[i]);
        } else if (std::fabs(d1) <= yTol) {
            rawX.push_back(xs[i + 1]);
        } else if ((d0 < 0.0 && d1 > 0.0) || (d0 > 0.0 && d1 < 0.0)) {
            const double t = (targetY - ys[i]) / (ys[i + 1] - ys[i]);
            rawX.push_back(xs[i] + t * (xs[i + 1] - xs[i]));
        }
    }

    std::sort(rawX.begin(), rawX.end());
    for (double xv : rawX) {
        if (roots.isEmpty() || std::fabs(xv - roots.back()) > xDedup)
            roots.append(xv);
    }
    return roots;
}

double CurveMathTools::findYFromX(Graph *g, const QString &curveTitle, double targetX, bool *ok)
{
    if (ok)
        *ok = false;
    if (!g || !std::isfinite(targetX))
        return std::numeric_limits<double>::quiet_NaN();

    std::vector<double> xs, ys;
    if (!extractUnmaskedCurvePoints(g, curveTitle, xs, ys, true, true) || xs.empty())
        return std::numeric_limits<double>::quiet_NaN();

    const double spanX = std::max(1.0, xs.back() - xs.front());
    const double tolX = 1e-11 * spanX;
    if (targetX < xs.front() - tolX || targetX > xs.back() + tolX)
        return std::numeric_limits<double>::quiet_NaN();

    const double yVal = interpLinearSorted(xs, ys, std::clamp(targetX, xs.front(), xs.back()));
    if (ok)
        *ok = std::isfinite(yVal);
    return yVal;
}

double CurveMathTools::polygonArea(Graph *g, const QString &curveTitle, bool mathematicalSigned)
{
    if (!g)
        return 0.0;

    std::vector<double> xs, ys;
    if (!extractUnmaskedCurvePoints(g, curveTitle, xs, ys, false, false) || xs.size() < 3)
        return 0.0;

    const size_t n = xs.size();
    const double x0 = xs[0];
    const double y0 = ys[0];
    double twiceArea = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const size_t j = (i + 1) % n;
        const double xi = xs[i] - x0;
        const double yi = ys[i] - y0;
        const double xj = xs[j] - x0;
        const double yj = ys[j] - y0;
        twiceArea += (xi * yj - xj * yi);
    }

    const double signedArea = 0.5 * twiceArea;
    return mathematicalSigned ? signedArea : std::fabs(signedArea);
}

double CurveMathTools::curveArcLength(Graph *g, const QString &curveTitle)
{
    if (!g)
        return 0.0;

    std::vector<double> xs, ys;
    if (!extractUnmaskedCurvePoints(g, curveTitle, xs, ys, false, false) || xs.size() < 2)
        return 0.0;

    double length = 0.0;
    for (size_t i = 0; i + 1 < xs.size(); ++i)
        length += std::hypot(xs[i + 1] - xs[i], ys[i + 1] - ys[i]);
    return length;
}

void CurveMathTools::showCurveMathDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g || ApplicationWindow::isNonInteractiveContext(app))
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trMath("Simple Curve Math"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbA = new QComboBox(&dlg);
    cbA->addItems(curves);
    QComboBox *cbOp = new QComboBox(&dlg);
    cbOp->addItem(trMath("Add (A + B)"), static_cast<int>(Add));
    cbOp->addItem(trMath("Subtract (A - B)"), static_cast<int>(Subtract));
    cbOp->addItem(trMath("Multiply (A * B)"), static_cast<int>(Multiply));
    cbOp->addItem(trMath("Divide (A / B)"), static_cast<int>(Divide));
    QComboBox *cbB = new QComboBox(&dlg);
    cbB->addItems(curves);
    if (curves.size() >= 2)
        cbB->setCurrentIndex(1);

    QCheckBox *chkUnion = new QCheckBox(trMath("Use Union X Grid of Both Curves"), &dlg);
    chkUnion->setChecked(false);

    form->addRow(trMath("Curve A (Reference):"), cbA);
    form->addRow(trMath("Operator:"), cbOp);
    form->addRow(trMath("Curve B:"), cbB);
    form->addRow(QString(), chkUnion);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);

    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        simpleCurveMath(app, g, cbA->currentText(), cbB->currentText(),
                        static_cast<MathOp>(cbOp->currentData().toInt()), chkUnion->isChecked());
    }
}

void CurveMathTools::showSubtractLineDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g || ApplicationWindow::isNonInteractiveContext(app))
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trMath("Subtract Straight Line"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);

    auto makeSpin = [&](double val) {
        QDoubleSpinBox *sb = new QDoubleSpinBox(&dlg);
        sb->setRange(-1e15, 1e15);
        sb->setDecimals(6);
        sb->setValue(val);
        return sb;
    };

    std::vector<double> xs, ys;
    extractUnmaskedCurvePoints(g, curves.first(), xs, ys, true, false);
    const double x1Init = xs.empty() ? 0.0 : xs.front();
    const double y1Init = ys.empty() ? 0.0 : ys.front();
    const double x2Init = xs.empty() ? 1.0 : xs.back();
    const double y2Init = ys.empty() ? 0.0 : ys.back();

    QDoubleSpinBox *sbX1 = makeSpin(x1Init);
    QDoubleSpinBox *sbY1 = makeSpin(y1Init);
    QDoubleSpinBox *sbX2 = makeSpin(x2Init);
    QDoubleSpinBox *sbY2 = makeSpin(y2Init);
    QCheckBox *chkInPlace = new QCheckBox(trMath("Modify Source Curve In-Place"), &dlg);

    QObject::connect(cbCurve, &QComboBox::currentTextChanged, &dlg, [&](const QString &name) {
        std::vector<double> cx, cy;
        if (extractUnmaskedCurvePoints(g, name, cx, cy, true, false) && !cx.empty()) {
            sbX1->setValue(cx.front());
            sbY1->setValue(cy.front());
            sbX2->setValue(cx.back());
            sbY2->setValue(cy.back());
        }
    });

    form->addRow(trMath("Curve:"), cbCurve);
    form->addRow(trMath("Point 1 X:"), sbX1);
    form->addRow(trMath("Point 1 Y:"), sbY1);
    form->addRow(trMath("Point 2 X:"), sbX2);
    form->addRow(trMath("Point 2 Y:"), sbY2);
    form->addRow(QString(), chkInPlace);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);

    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        subtractStraightLine(app, g, cbCurve->currentText(),
                             QPointF(sbX1->value(), sbY1->value()),
                             QPointF(sbX2->value(), sbY2->value()), chkInPlace->isChecked());
    }
}

void CurveMathTools::showAverageCurvesDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g || ApplicationWindow::isNonInteractiveContext(app))
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trMath("Average Multiple Curves"));

    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addWidget(new QLabel(trMath("Select Curves to Average:"), &dlg));

    QListWidget *listWidget = new QListWidget(&dlg);
    listWidget->setSelectionMode(QAbstractItemView::MultiSelection);
    for (const QString &c : curves) {
        QListWidgetItem *item = new QListWidgetItem(c, listWidget);
        item->setSelected(true);
    }
    vbox->addWidget(listWidget);

    QFormLayout *form = new QFormLayout();
    QSpinBox *sbPts = new QSpinBox(&dlg);
    sbPts->setRange(10, 100000);
    sbPts->setValue(200);
    QCheckBox *chkBand = new QCheckBox(trMath("Plot Mean ± SD Dashed Curves"), &dlg);
    chkBand->setChecked(true);

    form->addRow(trMath("Interpolated Grid Points:"), sbPts);
    form->addRow(QString(), chkBand);
    vbox->addLayout(form);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        QStringList selected;
        for (QListWidgetItem *it : listWidget->selectedItems())
            selected << it->text();
        if (!selected.isEmpty())
            averageMultipleCurves(app, g, selected, sbPts->value(), chkBand->isChecked(), nullptr);
    }
}

void CurveMathTools::showNormalizeDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g || ApplicationWindow::isNonInteractiveContext(app))
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trMath("Normalize Curves"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItem(trMath("[All Curves on Graph]"), "__ALL__");
    for (const QString &c : curves)
        cbCurve->addItem(c, c);
    if (curves.size() == 1)
        cbCurve->setCurrentIndex(1);

    QComboBox *cbMode = new QComboBox(&dlg);
    cbMode->addItem(trMath("Divide by Maximum (Max = 1)"), static_cast<int>(DivideByMax));
    cbMode->addItem(trMath("Normalize to [0, 1]"), static_cast<int>(Range0To1));
    cbMode->addItem(trMath("Normalize to [-1, 1]"), static_cast<int>(RangeMinus1To1));
    cbMode->addItem(trMath("Z-Score Standardization (Mean=0, SD=1)"), static_cast<int>(ZScore));
    cbMode->addItem(trMath("Unit Area (Integral = 1)"), static_cast<int>(UnitArea));
    cbMode->addItem(trMath("Sum to 100%"), static_cast<int>(SumTo100));
    cbMode->addItem(trMath("Divide by Y at Reference X"), static_cast<int>(DivideByRefX));

    QDoubleSpinBox *sbRefX = new QDoubleSpinBox(&dlg);
    sbRefX->setRange(-1e15, 1e15);
    sbRefX->setDecimals(6);
    sbRefX->setEnabled(false);

    QObject::connect(cbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int idx) {
                         sbRefX->setEnabled(cbMode->itemData(idx).toInt()
                                            == static_cast<int>(DivideByRefX));
                     });

    form->addRow(trMath("Target Curve:"), cbCurve);
    form->addRow(trMath("Normalization Method:"), cbMode);
    form->addRow(trMath("Reference X:"), sbRefX);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const NormMode mode = static_cast<NormMode>(cbMode->currentData().toInt());
        const QString sel = cbCurve->currentData().toString();
        if (sel == "__ALL__") {
            for (const QString &c : curves)
                normalizeCurve(app, g, c, mode, sbRefX->value());
        } else {
            normalizeCurve(app, g, sel, mode, sbRefX->value());
        }
    }
}

void CurveMathTools::showIntersectionLookupDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g || ApplicationWindow::isNonInteractiveContext(app))
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trMath("Curve Intersections & X/Y Lookup"));
    dlg.resize(480, 380);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    QTabWidget *tabs = new QTabWidget(&dlg);
    QTextEdit *outputBox = new QTextEdit(&dlg);
    outputBox->setReadOnly(true);

    // Tab 1: Curve Intersections
    QWidget *tabInter = new QWidget(&dlg);
    QFormLayout *formInter = new QFormLayout(tabInter);
    QComboBox *cbInterA = new QComboBox(tabInter);
    cbInterA->addItems(curves);
    QComboBox *cbInterB = new QComboBox(tabInter);
    cbInterB->addItems(curves);
    if (curves.size() >= 2)
        cbInterB->setCurrentIndex(1);
    QCheckBox *chkMark = new QCheckBox(trMath("Mark Intersection Points on Graph"), tabInter);
    chkMark->setChecked(true);
    QPushButton *btnComputeInter = new QPushButton(trMath("Find Intersections"), tabInter);
    formInter->addRow(trMath("Curve A:"), cbInterA);
    formInter->addRow(trMath("Curve B:"), cbInterB);
    formInter->addRow(QString(), chkMark);
    formInter->addRow(QString(), btnComputeInter);
    tabs->addTab(tabInter, trMath("Intersections"));

    // Tab 2: X from Y / Y from X Lookup & Loop Area
    QWidget *tabLookup = new QWidget(&dlg);
    QFormLayout *formLookup = new QFormLayout(tabLookup);
    QComboBox *cbLookCurve = new QComboBox(tabLookup);
    cbLookCurve->addItems(curves);
    QDoubleSpinBox *sbQueryVal = new QDoubleSpinBox(tabLookup);
    sbQueryVal->setRange(-1e15, 1e15);
    sbQueryVal->setDecimals(6);
    QHBoxLayout *lookupBtnRow = new QHBoxLayout();
    QPushButton *btnFindY = new QPushButton(trMath("Find Y at X"), tabLookup);
    QPushButton *btnFindX = new QPushButton(trMath("Find X at Y"), tabLookup);
    QPushButton *btnArea = new QPushButton(trMath("Polygon Area & Arc Length"), tabLookup);
    lookupBtnRow->addWidget(btnFindY);
    lookupBtnRow->addWidget(btnFindX);
    lookupBtnRow->addWidget(btnArea);
    formLookup->addRow(trMath("Curve:"), cbLookCurve);
    formLookup->addRow(trMath("Query X or Y Value:"), sbQueryVal);
    formLookup->addRow(lookupBtnRow);
    tabs->addTab(tabLookup, trMath("X/Y Lookup & Geometry"));

    mainLayout->addWidget(tabs);
    mainLayout->addWidget(outputBox);

    QDialogButtonBox *closeBtn = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    mainLayout->addWidget(closeBtn);
    QObject::connect(closeBtn, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    QObject::connect(btnComputeInter, &QPushButton::clicked, &dlg, [&]() {
        Table *outTbl = nullptr;
        const QVector<QPointF> pts = findCurveIntersections(
                g, cbInterA->currentText(), cbInterB->currentText(), chkMark->isChecked(), app,
                &outTbl);
        QString txt = trMath("Found %1 intersection(s):\n").arg(pts.size());
        for (int i = 0; i < pts.size(); ++i)
            txt += QString("  #%1: X = %2, Y = %3\n")
                           .arg(i + 1)
                           .arg(pts[i].x(), 0, 'g', 10)
                           .arg(pts[i].y(), 0, 'g', 10);
        if (outTbl)
            txt += trMath("Output Table: %1\n").arg(outTbl->name());
        outputBox->setPlainText(txt);
        app->updateLog(txt);
    });

    QObject::connect(btnFindY, &QPushButton::clicked, &dlg, [&]() {
        bool ok = false;
        const double yVal = findYFromX(g, cbLookCurve->currentText(), sbQueryVal->value(), &ok);
        const QString txt = ok
                ? QString("Y(X = %1) = %2\n")
                          .arg(sbQueryVal->value(), 0, 'g', 10)
                          .arg(yVal, 0, 'g', 10)
                : trMath("Target X is outside the curve's X range.\n");
        outputBox->setPlainText(txt);
        app->updateLog(txt);
    });

    QObject::connect(btnFindX, &QPushButton::clicked, &dlg, [&]() {
        const QVector<double> xs = findXFromY(g, cbLookCurve->currentText(), sbQueryVal->value());
        QString txt = trMath("X values for Y = %1 (%2 match(es)):\n")
                              .arg(sbQueryVal->value(), 0, 'g', 10)
                              .arg(xs.size());
        for (int i = 0; i < xs.size(); ++i)
            txt += QString("  #%1: X = %2\n").arg(i + 1).arg(xs[i], 0, 'g', 10);
        outputBox->setPlainText(txt);
        app->updateLog(txt);
    });

    QObject::connect(btnArea, &QPushButton::clicked, &dlg, [&]() {
        const double areaAbs = polygonArea(g, cbLookCurve->currentText(), false);
        const double areaSigned = polygonArea(g, cbLookCurve->currentText(), true);
        const double arcLen = curveArcLength(g, cbLookCurve->currentText());
        const QString txt = trMath("Curve: %1\n  Polygon Area = %2 (Signed = %3)\n  Arc Length = %4\n")
                                    .arg(cbLookCurve->currentText())
                                    .arg(areaAbs, 0, 'g', 10)
                                    .arg(areaSigned, 0, 'g', 10)
                                    .arg(arcLen, 0, 'g', 10);
        outputBox->setPlainText(txt);
        app->updateLog(txt);
    });

    dlg.exec();
}

// ============================================================================
// Phase 5: Materials-Science Analyzers (Arrhenius, Tauc) & Fill Between Curves
// ============================================================================

namespace {

void registerMatSciTranslationsOnce()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(
                QString::fromUtf8(en), QString::fromUtf8(ja));
    };
    reg("Arrhenius Plot (Activation Energy)", "アレニウスプロット (活性化エネルギー)");
    reg("Arrhenius Plot (Activation Energy)...", "アレニウスプロット (活性化エネルギー)...");
    reg("Tauc Plot (Optical Bandgap)", "Taucプロット (光学バンドギャップ)");
    reg("Tauc Plot (Optical Bandgap)...", "Taucプロット (光学バンドギャップ)...");
    reg("Fill Area Between Curves", "2曲線間の領域塗りつぶし");
    reg("Fill Area Between Curves...", "2曲線間の領域塗りつぶし...");
    reg("Source Worksheet:", "入力ワークシート:");
    reg("Temperature Column:", "温度データ列:");
    reg("Rate / Conductivity Column:", "反応速度 / 導電率列:");
    reg("Temperature in Celsius (°C -> +273.15 K)", "摂氏温度入力 (°C -> +273.15 K)");
    reg("Plot vs. 1000 / T (K^-1)", "横軸を 1000 / T (K^-1) にする");
    reg("Modified Arrhenius: ln(k * T) (Ionic Conductivity)", "修正アレニウス式: ln(k * T) (イオン伝導度等)");
    reg("1000 / T (K^-1)", "1000 / T (K^-1)");
    reg("1 / T (K^-1)", "1 / T (K^-1)");
    reg("ln(k * T)", "ln(k * T)");
    reg("ln(k)", "ln(k)");
    reg("Wavelength / Energy Column:", "波長 / 光子エネルギー列:");
    reg("Absorption Coefficient Column (α):", "吸収係数データ列 (α):");
    reg("X Column Unit:", "X列の単位:");
    reg("Wavelength λ (nm -> hν = 1239.84 / λ eV)", "波長 λ (nm -> hν = 1239.84 / λ eV)");
    reg("Photon Energy hν (eV)", "光子エネルギー hν (eV)");
    reg("Transition Type:", "光学遷移タイプ:");
    reg("Direct Allowed: (αhν)^2", "直接許容遷移: (αhν)^2");
    reg("Indirect Allowed: (αhν)^(1/2)", "間接許容遷移: (αhν)^(1/2)");
    reg("Photon Energy hν (eV)", "光子エネルギー hν (eV)");
    reg("(αhν)^2 ((eV/cm)^2)", "(αhν)^2 ((eV/cm)^2)");
    reg("(αhν)^(1/2) ((eV/cm)^(1/2))", "(αhν)^(1/2) ((eV/cm)^(1/2))");
    reg("First Curve (Boundary A):", "曲線A (境界1):");
    reg("Second Curve (Boundary B):", "曲線B (境界2):");
    reg("Fill Color & Opacity:", "塗りつぶし色・不透明度:");
    reg("Choose Color...", "色を選択...");
    reg("Fill Opacity (%):", "不透明度 (%):");
    reg("No numeric worksheets are open.", "数値データを含むワークシートが開かれていません。");
    reg("Please open a graph with at least two curves first.",
        "2本以上の曲線を含むグラフを開いてください。");
    reg("EIS Nyquist & Bode Analyzer (Randles-CPE)",
        "EIS ナイキスト・ボード解析 (Randles-CPE等価回路)");
    reg("EIS Nyquist & Bode Analyzer...", "EIS ナイキスト・ボード解析...");
    reg("EIS Nyquist Plot (Randles-CPE)", "EIS ナイキスト線図 (Randles-CPE)");
    reg("EIS Bode Plot (Impedance & Phase)", "EIS ボード線図 (インピーダンス・位相)");
    reg("Frequency Column (Hz):", "周波数データ列 (Hz):");
    reg("Real Impedance Column (Z'):", "実部インピーダンス列 (Z'):");
    reg("Imaginary Impedance Column (Z'' or -Z''):", "虚部インピーダンス列 (Z'' または -Z''):");
    reg("Imaginary column is already negated (-Z'' > 0)", "虚部列は既に符号反転済み (-Z'' > 0)");
    reg("Fit Randles + CPE Equivalent Circuit (Rs + Rct || CPE)",
        "Randles + CPE 等価回路フィッティングを実行 (Rs + Rct || CPE)");
    reg("Z' (Ω)", "Z' (Ω)");
    reg("-Z'' (Ω)", "-Z'' (Ω)");
    reg("log10(f / Hz)", "log10(f / Hz)");
    reg("log10(|Z| / Ω)", "log10(|Z| / Ω)");
    reg("Phase (°)", "位相角 (°)");
    reg("Please open a worksheet with at least 3 numeric columns (Frequency, Z', Z'').",
        "3列以上の数値データ列 (周波数, Z', Z'') を含むワークシートを開いてください。");
    reg("EIS analysis requires at least 4 valid positive-frequency data points.",
        "EIS解析には有効な正の周波数データ点が4点以上必要です。");
    reg("Measured", "実測値");
    reg("Linear fit", "線形フィット");
    reg("Tensile / 0.2% Proof Stress Analyzer", "引張試験・0.2%耐力解析");
    reg("Tensile / 0.2% Yield", "引張・0.2%耐力");
    reg("Tensile / 0.2% Yield...", "引張・0.2%耐力...");
    reg("Data Source:", "データ入力元:");
    reg("Active Graph Curve", "グラフ上の曲線");
    reg("Worksheet Columns (Plot & Analyze)", "ワークシート列 (プロットして解析)");
    reg("Target Graph:", "対象グラフ:");
    reg("Stress-Strain Curve:", "応力-ひずみ曲線:");
    reg("Strain Column (X):", "ひずみ列 (X):");
    reg("Stress Column (Y):", "応力列 (Y):");
    reg("Offset Proof Strain (%):", "オフセット耐力ひずみ (%):");
    reg("Strain Unit:", "ひずみ単位:");
    reg("Auto-detect (% or mm/mm)", "自動判定 (% または mm/mm)");
    reg("Percent (%)", "パーセント (%)");
    reg("Engineering Fraction (mm/mm)", "無次元比 (mm/mm)");
    reg("Auto-detect linear-elastic region (max R²)", "線形弾性域を自動検出 (最大 R² 区間)");
    reg("Elastic Strain Min:", "弾性域の最小ひずみ:");
    reg("Elastic Strain Max:", "弾性域の最大ひずみ:");
    reg("Plot 0.2% offset construction line & yield marker",
        "オフセット補助線と耐力基準線をプロット");
    reg("Add summary callout (σ0.2, E, UTS) on graph",
        "グラフ内に解析結果ボックス (σ0.2, E, UTS) を表示");
    reg("Generate True Stress - True Strain worksheet",
        "真応力-真ひずみワークシートを生成");
    reg("Please open a graph or numeric worksheet with stress-strain data first.",
        "応力-ひずみデータを含むグラフまたはワークシートを先に開いてください。");
    reg("No active graph found for tensile analysis.",
        "引張解析の対象となるグラフが見つかりません。");
    reg("Curve '%1' not found on graph.",
        "曲線 '%1' がグラフ上に存在しません。");
    reg("No data curves available on graph.",
        "グラフ上に解析可能なデータ曲線がありません。");
    reg("At least 4 valid data points are required for tensile analysis.",
        "引張解析には有効なデータ点が4点以上必要です。");
    reg("Tensile stress values must contain positive stress.",
        "応力データには正の値が含まれている必要があります。");
    reg("Could not determine a positive linear-elastic slope in the specified range.",
        "指定範囲内で正のヤング率 (線形弾性勾配) を算出できませんでした。");
    reg("Curve fractured before reaching the %1% offset yield line.",
        "%1% オフセット耐力線に交差する前に破断しました。");
}

bool readMatSciCell(const Column *col, int row, double &val)
{
    if (!col || row < 0 || row >= col->rowCount())
        return false;
    if (col->isInvalid(row) || col->isMasked(row))
        return false;
    if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
        val = col->valueAt(row);
        return std::isfinite(val);
    }
    QString txt;
    if (col->columnMode() == SciDAVis::ColumnMode::Text)
        txt = col->textAt(row).trimmed();
    else if (col->asStringColumn())
        txt = col->asStringColumn()->textAt(row).trimmed();
    if (txt.isEmpty())
        return false;
    bool ok = false;
    val = txt.toDouble(&ok);
    if (!ok)
        val = QLocale().toDouble(txt, &ok);
    return ok && std::isfinite(val);
}

bool matSciColumnHasNumeric(const Column *col)
{
    if (!col)
        return false;
    if (col->columnMode() == SciDAVis::ColumnMode::Numeric)
        return true;
    double tmp = 0.0;
    const int n = col->rowCount();
    for (int r = 0; r < n; ++r) {
        if (readMatSciCell(col, r, tmp))
            return true;
    }
    return false;
}

Column *resolveMatSciColumn(Table *t, const QString &colSpec)
{
    if (!t || colSpec.trimmed().isEmpty())
        return nullptr;
    if (Column *c = t->column(colSpec))
        return c;
    const QString trimmed = colSpec.trimmed();
    if (trimmed.startsWith(t->name() + "_")) {
        if (Column *c = t->column(trimmed.mid(t->name().length() + 1)))
            return c;
    }
    for (int i = 0; i < t->numCols(); ++i) {
        Column *cand = t->column(i);
        if (!cand)
            continue;
        if (cand->name().compare(trimmed, Qt::CaseInsensitive) == 0
            || t->colName(i).compare(trimmed, Qt::CaseInsensitive) == 0)
            return cand;
    }
    return nullptr;
}

QList<Table *> collectProjectTables(ApplicationWindow *app)
{
    QList<Table *> list;
    if (!app)
        return list;
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("Table"))
            list.append(static_cast<Table *>(w));
    }
    return list;
}

Table *findActiveOrFirstTable(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    if (QMdiSubWindow *sub = app->d_workspace.activeSubWindow()) {
        if (sub->inherits("Table"))
            return static_cast<Table *>(sub);
        if (QWidget *w = sub->widget()) {
            if (w->inherits("Table"))
                return static_cast<Table *>(w);
        }
    }
    const QList<Table *> tbls = collectProjectTables(app);
    return tbls.isEmpty() ? nullptr : tbls.first();
}

Graph *findActiveOrFirstGraph(ApplicationWindow *app, Graph *explicitGraph)
{
    if (explicitGraph)
        return explicitGraph;
    if (!app)
        return nullptr;
    if (QMdiSubWindow *sub = app->d_workspace.activeSubWindow()) {
        if (sub->inherits("MultiLayer"))
            return static_cast<MultiLayer *>(sub)->activeGraph();
        if (QWidget *w = sub->widget()) {
            if (w->inherits("MultiLayer"))
                return static_cast<MultiLayer *>(w)->activeGraph();
        }
    }
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer")) {
            if (Graph *g = static_cast<MultiLayer *>(w)->activeGraph())
                return g;
        }
    }
    return nullptr;
}

void applyArialToMatSciGraph(Graph *g)
{
    if (!g)
        return;
    const QFont titleFnt(QStringLiteral("Arial"), 11, QFont::Normal);
    const QFont tickFnt(QStringLiteral("Arial"), 10, QFont::Normal);
    g->initFonts(titleFnt, tickFnt);
    g->setTitleFont(QFont(QStringLiteral("Arial"), 11, QFont::Normal));
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        g->setAxisTitleFont(a, titleFnt);
        g->setAxisFont(a, tickFnt);
    }
    if (g->legend())
        g->legend()->setFont(QFont(QStringLiteral("Arial"), 9, QFont::Normal));
    g->setTextMarkerDefaults(g->textMarkerDefaultFrame(),
                             QFont(QStringLiteral("Arial"), 9, QFont::Normal),
                             g->textMarkerDefaultColor(),
                             g->textMarkerDefaultBackground());
}

Legend *addMatSciCalloutBox(Graph *g, double x, double y, const QString &text)
{
    if (!g)
        return nullptr;
    Legend *box = g->newLegend(text);
    if (!box)
        return nullptr;
    const int basePt = (g->legend() ? g->legend()->font().pointSize() : 9);
    QFont calloutFnt(QStringLiteral("Arial"), std::clamp(basePt, 8, 22), QFont::Normal);
    calloutFnt.setBold(false);
    box->setFrameStyle(Legend::Line);
    box->setBackgroundColor(QColor(255, 255, 255, 232));
    box->setTextColor(QColor(20, 30, 50));
    box->setFont(calloutFnt);
    box->setOriginCoord(x, y);

    if (Plot *pw = g->plotWidget()) {
        g->replot();
        if (QWidget *cv = pw->canvas()) {
            const QRect cRect = cv->rect();
            QRect bRect = box->rect();
            const int pad = 6;
            while (cRect.width() > 40 && bRect.width() > cRect.width() - 2 * pad
                   && calloutFnt.pointSize() > 7) {
                calloutFnt.setPointSize(calloutFnt.pointSize() - 1);
                box->setFont(calloutFnt);
                g->replot();
                bRect = box->rect();
            }
            if (cRect.width() > 30 && cRect.height() > 30 && bRect.width() > 0
                && bRect.height() > 0) {
                const int maxLeft = std::max(pad, cRect.width() - bRect.width() - pad);
                const int maxTop = std::max(pad, cRect.height() - bRect.height() - pad);
                const int clampedPx = std::clamp(bRect.left(), pad, maxLeft);
                const int clampedPy = std::clamp(bRect.top(), pad, maxTop);
                if (clampedPx != bRect.left() || clampedPy != bRect.top()) {
                    const double newX = pw->invTransform(QwtPlot::xBottom, clampedPx);
                    const double newY = pw->invTransform(QwtPlot::yLeft, clampedPy);
                    if (std::isfinite(newX) && std::isfinite(newY))
                        box->setOriginCoord(newX, newY);
                }
            }
        }
    }
    return box;
}

} // namespace

CurveMathTools::ArrheniusResult CurveMathTools::createArrheniusPlot(
        ApplicationWindow *app, Table *srcTable, const QString &tempCol, const QString &rateCol,
        bool tempInCelsius, bool use1000OverT, bool sigmaTimesT)
{
    registerMatSciTranslationsOnce();
    ArrheniusResult res;
    if (!srcTable)
        return res;

    Column *cTemp = resolveMatSciColumn(srcTable, tempCol);
    Column *cRate = resolveMatSciColumn(srcTable, rateCol);
    if (!cTemp || !cRate)
        return res;

    constexpr double kGasR = 8.314462618;       // J / (mol * K)
    constexpr double kBoltzEV = 8.617333262e-5; // eV / K
    const double scaleX = use1000OverT ? 1000.0 : 1.0;

    struct ArrPt {
        double x = 0.0;
        double y = 0.0;
        double tempK = 0.0;
    };
    std::vector<ArrPt> pts;
    const int nRows = std::min(cTemp->rowCount(), cRate->rowCount());
    pts.reserve(static_cast<size_t>(std::max(0, nRows)));

    for (int r = 0; r < nRows; ++r) {
        double rawT = 0.0, rawK = 0.0;
        if (!readMatSciCell(cTemp, r, rawT) || !readMatSciCell(cRate, r, rawK))
            continue;
        const double tK = tempInCelsius ? (rawT + 273.15) : rawT;
        const double prod = sigmaTimesT ? (rawK * tK) : rawK;
        if (tK <= 0.0 || prod <= 0.0)
            continue;
        pts.push_back({ scaleX / tK, std::log(prod), tK });
    }

    if (pts.size() < 2)
        return res;

    std::sort(pts.begin(), pts.end(),
              [](const ArrPt &a, const ArrPt &b) { return a.x < b.x; });

    const int n = static_cast<int>(pts.size());
    double sumX = 0.0, sumY = 0.0;
    for (const ArrPt &p : pts) {
        sumX += p.x;
        sumY += p.y;
    }
    const double meanX = sumX / static_cast<double>(n);
    const double meanY = sumY / static_cast<double>(n);

    double sxx = 0.0, sxy = 0.0, syy = 0.0;
    for (const ArrPt &p : pts) {
        const double dx = p.x - meanX;
        const double dy = p.y - meanY;
        sxx += dx * dx;
        sxy += dx * dy;
        syy += dy * dy;
    }
    if (sxx <= 1e-30)
        return res;

    const double slope = sxy / sxx;
    const double intercept = meanY - slope * meanX;
    double rss = 0.0;
    for (const ArrPt &p : pts) {
        const double e = p.y - (intercept + slope * p.x);
        rss += e * e;
    }
    const double r2 = (syy > 1e-30) ? std::clamp(1.0 - rss / syy, 0.0, 1.0) : 1.0;
    const double sResid = (n > 2) ? std::sqrt(std::max(0.0, rss / static_cast<double>(n - 2))) : 0.0;
    const double slopeSE = (n > 2) ? (sResid / std::sqrt(sxx)) : 0.0;

    res.valid = true;
    res.activationEnergyKJmol = -slope * kGasR * scaleX / 1000.0;
    res.activationEnergyKJmolSE = slopeSE * kGasR * scaleX / 1000.0;
    res.activationEnergyEV = -slope * kBoltzEV * scaleX;
    res.activationEnergyEVSE = slopeSE * kBoltzEV * scaleX;
    res.lnPreExponential = intercept;
    res.rSquare = r2;

    if (!app)
        return res;

    const double tCrit = (n > 2) ? gsl_cdf_tdist_Pinv(0.975, static_cast<double>(n - 2)) : 0.0;
    std::vector<double> colInvT(static_cast<size_t>(n));
    std::vector<double> colTempK(static_cast<size_t>(n));
    std::vector<double> colLnK(static_cast<size_t>(n));
    std::vector<double> colFitY(static_cast<size_t>(n));
    std::vector<double> colCiLo(static_cast<size_t>(n));
    std::vector<double> colCiHi(static_cast<size_t>(n));

    for (int i = 0; i < n; ++i) {
        const double xv = pts[static_cast<size_t>(i)].x;
        const double yh = intercept + slope * xv;
        const double halfBand = (n > 2)
                ? (tCrit * sResid
                   * std::sqrt(1.0 / static_cast<double>(n)
                               + ((xv - meanX) * (xv - meanX)) / sxx))
                : 0.0;
        colInvT[static_cast<size_t>(i)] = xv;
        colTempK[static_cast<size_t>(i)] = pts[static_cast<size_t>(i)].tempK;
        colLnK[static_cast<size_t>(i)] = pts[static_cast<size_t>(i)].y;
        colFitY[static_cast<size_t>(i)] = yh;
        colCiLo[static_cast<size_t>(i)] = yh - halfBand;
        colCiHi[static_cast<size_t>(i)] = yh + halfBand;
    }

    const QString invHeader = use1000OverT ? QStringLiteral("Inv1000T") : QStringLiteral("InvT");
    const QString lnHeader = sigmaTimesT ? QStringLiteral("LnKT") : QStringLiteral("LnK");
    Table *tbl = createNumericTable(
            app, QStringLiteral("ArrheniusData"),
            QStringList() << invHeader << QStringLiteral("TempK") << lnHeader
                          << QStringLiteral("LinearFit") << QStringLiteral("CI95Lower")
                          << QStringLiteral("CI95Upper"),
            QList<SciDAVis::PlotDesignation>()
                    << SciDAVis::X << SciDAVis::noDesignation << SciDAVis::Y << SciDAVis::Y
                    << SciDAVis::Y << SciDAVis::Y,
            { colInvT, colTempK, colLnK, colFitY, colCiLo, colCiHi });
    res.dataTable = tbl;
    if (!tbl)
        return res;

    if (Column *c0 = tbl->column(0)) {
        c0->setLongName(use1000OverT ? QStringLiteral("1000 / <i>T</i>")
                                     : QStringLiteral("1 / <i>T</i>"));
        c0->setUnits(QStringLiteral("K<sup>-1</sup>"));
    }
    if (Column *c2 = tbl->column(2)) {
        c2->setLongName(sigmaTimesT ? QStringLiteral("ln(<i>kT</i>)")
                                    : QStringLiteral("ln <i>k</i>"));
        c2->setComment(QObject::tr("Measured"));
    }
    if (Column *c3 = tbl->column(3))
        c3->setComment(QObject::tr("Linear fit"));

    MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("ArrheniusPlot")));
    res.plot = ml;
    if (ml) {
        if (Graph *g = ml->activeGraph()) {
            if (n > 2 && sResid > 0.0) {
                QVector<double> vx(n), vyLo(n), vyHi(n);
                for (int i = 0; i < n; ++i) {
                    vx[i] = colInvT[static_cast<size_t>(i)];
                    vyLo[i] = colCiLo[static_cast<size_t>(i)];
                    vyHi[i] = colCiHi[static_cast<size_t>(i)];
                }
                if (QwtPlotItem *band = g->addShadedBand(vx, vyLo, vyHi, QColor(205, 35, 45, 38),
                                                         QStringLiteral("95% CI"))) {
                    band->setZ(1.0);
                }
            }
            if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(2), Graph::Scatter, 0, n - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(25, 65, 135), 1.5));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(35, 95, 185)),
                                            QPen(QColor(15, 45, 95), 1.2), QSize(8, 8)));
            }
            insertStyledCurve(g, tbl, 0, 3, n, QPen(QColor(205, 35, 45), 2.2, Qt::SolidLine));

            g->setXAxisTitle(use1000OverT ? QStringLiteral("1000 / <i>T</i> / K<sup>-1</sup>")
                                          : QStringLiteral("1 / <i>T</i> / K<sup>-1</sup>"));
            g->setYAxisTitle(sigmaTimesT ? QStringLiteral("ln(<i>kT</i>)")
                                         : QStringLiteral("ln <i>k</i>"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->setAutoScale();
            applyArialToMatSciGraph(g);

            // Attach linked Top-X temperature layer (T / K or T / °C) first and settle layout
            // before placing legend & callout so parent and child canvasRects align 1:1
            const QString topFormula = tempInCelsius
                    ? (use1000OverT ? QStringLiteral("1000.0/x - 273.15")
                                    : QStringLiteral("1.0/x - 273.15"))
                    : (use1000OverT ? QStringLiteral("1000.0/x")
                                    : QStringLiteral("1.0/x"));
            Graph *topLayer =
                    ml->addLinkedLayer(g, true, false, topFormula, QStringLiteral("y"));
            if (topLayer) {
                topLayer->setAxisTitle(QwtPlot::xTop,
                                       tempInCelsius ? QStringLiteral("<i>T</i> / \u00B0C")
                                                     : QStringLiteral("<i>T</i> / K"));
                applyArialToMatSciGraph(topLayer);
            }
            ml->arrangeLayers(false, false);
            g->syncChildLinkedLayers();
            g->replot();
            if (topLayer)
                topLayer->replot();
            ml->setActiveGraph(g);

            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);

            const double xSpan = std::max(1e-9, colInvT.back() - colInvT.front());
            const auto yMM = std::minmax_element(colLnK.begin(), colLnK.end());
            const double ySpan = std::max(1e-9, *yMM.second - *yMM.first);
            const QString summary = QStringLiteral(
                    "<i>E</i><sub>a</sub> = %1 \u00B1 %2 kJ mol<sup>-1</sup>\n"
                    "(%3 eV), ln <i>A</i> = %4\n"
                    "<i>R</i><sup>2</sup> = %5")
                    .arg(res.activationEnergyKJmol, 0, 'f', 2)
                    .arg(res.activationEnergyKJmolSE, 0, 'f', 2)
                    .arg(res.activationEnergyEV, 0, 'f', 3)
                    .arg(res.lnPreExponential, 0, 'f', 2)
                    .arg(res.rSquare, 0, 'f', 4);
            addMatSciCalloutBox(g, colInvT.front() + 0.05 * xSpan,
                                *yMM.first + 0.28 * ySpan, summary);
            g->replot();
            if (topLayer)
                topLayer->replot();
        }
    }
    return res;
}

CurveMathTools::TaucResult CurveMathTools::createTaucPlot(
        ApplicationWindow *app, Table *srcTable, const QString &waveOrEnergyCol,
        const QString &alphaCol, bool xIsWavelengthNm, bool directAllowed)
{
    registerMatSciTranslationsOnce();
    TaucResult res;
    if (!srcTable)
        return res;

    Column *cX = resolveMatSciColumn(srcTable, waveOrEnergyCol);
    Column *cAlpha = resolveMatSciColumn(srcTable, alphaCol);
    if (!cX || !cAlpha)
        return res;

    constexpr double kPlanckHcEvNm = 1239.84193;
    std::vector<std::pair<double, double>> rawPts;
    const int nRows = std::min(cX->rowCount(), cAlpha->rowCount());
    rawPts.reserve(static_cast<size_t>(std::max(0, nRows)));

    for (int r = 0; r < nRows; ++r) {
        double xv = 0.0, av = 0.0;
        if (!readMatSciCell(cX, r, xv) || !readMatSciCell(cAlpha, r, av))
            continue;
        if (xv <= 0.0 || av < 0.0)
            continue;
        const double hv = xIsWavelengthNm ? (kPlanckHcEvNm / xv) : xv;
        if (!std::isfinite(hv) || hv <= 0.0)
            continue;
        const double ahv = std::max(0.0, av * hv);
        const double taucY = directAllowed ? (ahv * ahv) : std::sqrt(ahv);
        if (std::isfinite(taucY))
            rawPts.emplace_back(hv, taucY);
    }

    if (rawPts.size() < 4)
        return res;

    std::sort(rawPts.begin(), rawPts.end(),
              [](const std::pair<double, double> &a, const std::pair<double, double> &b) {
                  return a.first < b.first;
              });

    // Merge near-duplicate photon energies
    std::vector<double> E;
    std::vector<double> Y;
    E.reserve(rawPts.size());
    Y.reserve(rawPts.size());
    const double eSpanRaw = std::max(1e-6, rawPts.back().first - rawPts.front().first);
    const double eEps = 1e-12 * eSpanRaw;
    for (size_t i = 0; i < rawPts.size();) {
        double sE = rawPts[i].first;
        double sY = rawPts[i].second;
        int cnt = 1;
        size_t j = i + 1;
        while (j < rawPts.size() && std::fabs(rawPts[j].first - rawPts[i].first) <= eEps) {
            sE += rawPts[j].first;
            sY += rawPts[j].second;
            ++cnt;
            ++j;
        }
        E.push_back(sE / cnt);
        Y.push_back(sY / cnt);
        i = j;
    }

    const int N = static_cast<int>(E.size());
    if (N < 4)
        return res;

    const double yMax = *std::max_element(Y.begin(), Y.end());
    if (yMax <= 0.0)
        return res;

    // Prefix sums for O(1) window OLS
    std::vector<double> pE(static_cast<size_t>(N + 1), 0.0);
    std::vector<double> pY(static_cast<size_t>(N + 1), 0.0);
    std::vector<double> pEE(static_cast<size_t>(N + 1), 0.0);
    std::vector<double> pYY(static_cast<size_t>(N + 1), 0.0);
    std::vector<double> pEY(static_cast<size_t>(N + 1), 0.0);
    for (int k = 0; k < N; ++k) {
        const double ek = E[static_cast<size_t>(k)];
        const double yk = Y[static_cast<size_t>(k)];
        pE[static_cast<size_t>(k + 1)] = pE[static_cast<size_t>(k)] + ek;
        pY[static_cast<size_t>(k + 1)] = pY[static_cast<size_t>(k)] + yk;
        pEE[static_cast<size_t>(k + 1)] = pEE[static_cast<size_t>(k)] + ek * ek;
        pYY[static_cast<size_t>(k + 1)] = pYY[static_cast<size_t>(k)] + yk * yk;
        pEY[static_cast<size_t>(k + 1)] = pEY[static_cast<size_t>(k)] + ek * yk;
    }

    struct EdgeCandidate {
        int i = 0;
        int j = 0;
        double a = 0.0;
        double b = 0.0;
        double r2 = 0.0;
        double eg = 0.0;
        double egSE = 0.0;
        double yiNorm = 0.0;
        double yjNorm = 0.0;
    };

    const int mMin = std::max(4, std::min(6, N / 6));
    const int mMax = std::max(mMin, std::min(N, std::max(8, N / 2)));

    auto collectCandidates = [&](double minYiFrac, double minRiseFrac) {
        std::vector<EdgeCandidate> cands;
        for (int i = 0; i < N; ++i) {
            if (Y[static_cast<size_t>(i)] < minYiFrac * yMax)
                continue;
            for (int m = mMin; m <= mMax && i + m - 1 < N; ++m) {
                const int j = i + m - 1;
                if (Y[static_cast<size_t>(j)] - Y[static_cast<size_t>(i)] < minRiseFrac * yMax)
                    continue;
                const double s1 = static_cast<double>(m);
                const double sumE = pE[static_cast<size_t>(j + 1)] - pE[static_cast<size_t>(i)];
                const double sumY = pY[static_cast<size_t>(j + 1)] - pY[static_cast<size_t>(i)];
                const double meanE = sumE / s1;
                const double meanY = sumY / s1;
                const double sEE = (pEE[static_cast<size_t>(j + 1)] - pEE[static_cast<size_t>(i)])
                        - s1 * meanE * meanE;
                const double sYY = (pYY[static_cast<size_t>(j + 1)] - pYY[static_cast<size_t>(i)])
                        - s1 * meanY * meanY;
                const double sEY = (pEY[static_cast<size_t>(j + 1)] - pEY[static_cast<size_t>(i)])
                        - s1 * meanE * meanY;
                if (sEE <= 1e-24 || sYY <= 1e-30 || sEY <= 0.0)
                    continue;

                const double b = sEY / sEE;
                if (b <= 0.0)
                    continue;
                const double a = meanY - b * meanE;
                const double eg = -a / b;
                if (!std::isfinite(eg) || eg <= 0.0 || eg >= E[static_cast<size_t>(j)])
                    continue;

                const double rss = std::max(0.0, sYY - b * sEY);
                const double r2 = std::clamp(1.0 - rss / sYY, 0.0, 1.0);
                const double sResid = (m > 2)
                        ? std::sqrt(rss / static_cast<double>(m - 2))
                        : 0.0;
                const double egSE = (m > 2 && std::fabs(b) > 1e-30)
                        ? ((sResid / std::fabs(b))
                           * std::sqrt(1.0 / s1 + ((eg - meanE) * (eg - meanE)) / sEE))
                        : 0.0;

                cands.push_back({ i, j, a, b, r2, eg, egSE,
                                  Y[static_cast<size_t>(i)] / yMax,
                                  Y[static_cast<size_t>(j)] / yMax });
            }
        }
        return cands;
    };

    std::vector<EdgeCandidate> cands = collectCandidates(0.015, 0.06);
    if (cands.empty())
        cands = collectCandidates(0.002, 0.02);
    if (cands.empty())
        cands = collectCandidates(0.0, 0.0);
    if (cands.empty())
        return res;

    double bRefMax = 0.0;
    for (const EdgeCandidate &c : cands) {
        if (c.r2 >= 0.90)
            bRefMax = std::max(bRefMax, c.b);
    }
    if (bRefMax <= 0.0) {
        for (const EdgeCandidate &c : cands)
            bRefMax = std::max(bRefMax, c.b);
    }

    // Prefer absorption-onset / mid-edge windows before high-energy non-parabolic drift or saturation
    std::vector<EdgeCandidate> onsetPool;
    for (const EdgeCandidate &c : cands) {
        if (c.b >= 0.25 * bRefMax && c.yiNorm <= 0.35 && c.yjNorm <= 0.80 && c.r2 >= 0.985)
            onsetPool.push_back(c);
    }
    const std::vector<EdgeCandidate> &pool = onsetPool.empty() ? cands : onsetPool;

    double bestPoolR2 = 0.0;
    double maxPoolB = 0.0;
    for (const EdgeCandidate &c : pool) {
        bestPoolR2 = std::max(bestPoolR2, c.r2);
        maxPoolB = std::max(maxPoolB, c.b);
    }
    const double r2Floor = std::max(0.90, bestPoolR2 - 0.005);

    const EdgeCandidate *winner = &pool.front();
    double bestScore = -1.0;
    for (const EdgeCandidate &c : pool) {
        if (c.r2 < r2Floor && bestPoolR2 >= 0.95)
            continue;
        const double slopeTerm = (maxPoolB > 0.0) ? std::sqrt(c.b / maxPoolB) : 1.0;
        const double onsetWeight = 1.0 / (1.0 + 2.0 * c.yiNorm);
        const double score = std::pow(c.r2, 8) * slopeTerm * onsetWeight;
        if (score > bestScore) {
            bestScore = score;
            winner = &c;
        }
    }

    res.valid = true;
    res.bandgapEV = winner->eg;
    res.bandgapEVSE = winner->egSE;
    res.edgeSlope = winner->b;
    res.rSquare = winner->r2;

    if (!app)
        return res;

    // Build Tauc data + extrapolated tangent line table (extending down to (Eg, 0))
    const double tanE1 = E[static_cast<size_t>(winner->j)];
    const double tanY1 = winner->a + winner->b * tanE1;
    std::vector<double> colHv = E;
    std::vector<double> colTauc = Y;

    Table *tbl = app->newTable(makeUniqueTableName(app, QStringLiteral("TaucData")),
                               std::max(N, 2), 4);
    res.dataTable = tbl;
    if (!tbl)
        return res;

    tbl->setColName(0, QStringLiteral("PhotonEnergyEV"));
    tbl->setColName(1, directAllowed ? QStringLiteral("TaucDirect") : QStringLiteral("TaucIndirect"));
    tbl->setColName(2, QStringLiteral("TangentEV"));
    tbl->setColName(3, QStringLiteral("TangentY"));
    tbl->setColPlotDesignation(0, SciDAVis::X);
    tbl->setColPlotDesignation(1, SciDAVis::Y);
    tbl->setColPlotDesignation(2, SciDAVis::X);
    tbl->setColPlotDesignation(3, SciDAVis::Y);

    for (int r = 0; r < N; ++r) {
        tbl->column(0)->setValueAt(r, colHv[static_cast<size_t>(r)]);
        tbl->column(1)->setValueAt(r, colTauc[static_cast<size_t>(r)]);
    }
    tbl->column(2)->setValueAt(0, res.bandgapEV);
    tbl->column(3)->setValueAt(0, 0.0);
    tbl->column(2)->setValueAt(1, tanE1);
    tbl->column(3)->setValueAt(1, std::max(0.0, tanY1));
    for (int r = 2; r < tbl->numRows(); ++r) {
        tbl->column(2)->setInvalid(r, true);
        tbl->column(3)->setInvalid(r, true);
    }

    MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("TaucPlot")));
    res.plot = ml;
    if (ml) {
        if (Graph *g = ml->activeGraph()) {
            insertStyledCurve(g, tbl, 0, 1, N, QPen(QColor(25, 85, 165), 2.2, Qt::SolidLine));
            insertStyledCurve(g, tbl, 2, 3, 2, QPen(QColor(215, 35, 40), 2.0, Qt::DashLine));

            // Baseline Y = 0 reference marker
            ArrowMarker baseLine;
            const double xLo = std::min(colHv.front(), res.bandgapEV);
            const double xHi = colHv.back();
            baseLine.setStartPoint(xLo, 0.0);
            baseLine.setEndPoint(xHi, 0.0);
            baseLine.setBoundingRect(xLo, 0.0, xHi, 0.0);
            baseLine.setColor(QColor(120, 130, 145));
            baseLine.setWidth(1);
            baseLine.setStyle(Qt::DotLine);
            baseLine.drawStartArrow(false);
            baseLine.drawEndArrow(false);
            g->addArrow(&baseLine);

            g->setXAxisTitle(QStringLiteral("<i>h\u03BD</i> / eV"));
            g->setYAxisTitle(directAllowed
                                     ? QStringLiteral("(<i>\u03B1h\u03BD</i>)<sup>2</sup> / (eV cm<sup>-1</sup>)<sup>2</sup>")
                                     : QStringLiteral("(<i>\u03B1h\u03BD</i>)<sup>1/2</sup> / (eV cm<sup>-1</sup>)<sup>1/2</sup>"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->setAutoScale();
            applyArialToMatSciGraph(g);

            const double eSpan = std::max(1e-6, xHi - xLo);
            const QString summary = QStringLiteral(
                    "<i>E</i><sub>g</sub> = %1 \u00B1 %2 eV (%3)\n"
                    "Edge [%4 \u2013 %5 eV]\n"
                    "<i>R</i><sup>2</sup> = %6")
                    .arg(res.bandgapEV, 0, 'f', 3)
                    .arg(res.bandgapEVSE, 0, 'f', 3)
                    .arg(directAllowed ? QStringLiteral("Direct") : QStringLiteral("Indirect"))
                    .arg(E[static_cast<size_t>(winner->i)], 0, 'f', 2)
                    .arg(E[static_cast<size_t>(winner->j)], 0, 'f', 2)
                    .arg(res.rSquare, 0, 'f', 4);
            addMatSciCalloutBox(g, xLo + 0.05 * eSpan, 0.72 * yMax, summary);
            g->replot();
        }
    }
    return res;
}

QwtPlotCurve *CurveMathTools::fillBetweenCurves(ApplicationWindow *app, Graph *g,
                                                const QString &curveA, const QString &curveB,
                                                const QColor &fillColor)
{
    registerMatSciTranslationsOnce();
    if (!app || !g)
        return nullptr;

    QwtPlotCurve *cA = resolveCurve(g, curveA);
    QwtPlotCurve *cB = resolveCurve(g, curveB);
    if (!cA || !cB)
        return nullptr;

    std::vector<double> xA, yA, xB, yB;
    if (!extractUnmaskedCurvePoints(g, curveA, xA, yA, true, true)
        || !extractUnmaskedCurvePoints(g, curveB, xB, yB, true, true))
        return nullptr;
    if (xA.size() < 2 || xB.size() < 2)
        return nullptr;

    const double ovLo = std::max(xA.front(), xB.front());
    const double ovHi = std::min(xA.back(), xB.back());
    if (!(ovHi > ovLo))
        return nullptr;
    const double xStart = ovLo;
    const double xEnd = ovHi;

    // Collect union X knots + dense sampling + exact intersection X points
    std::vector<double> knots;
    knots.reserve(xA.size() + xB.size() + 128);
    knots.push_back(xStart);
    knots.push_back(xEnd);
    for (double xv : xA) {
        if (xv >= xStart && xv <= xEnd)
            knots.push_back(xv);
    }
    for (double xv : xB) {
        if (xv >= xStart && xv <= xEnd)
            knots.push_back(xv);
    }
    constexpr int kMinDense = 100;
    for (int k = 1; k < kMinDense; ++k)
        knots.push_back(xStart + (xEnd - xStart) * static_cast<double>(k) / kMinDense);

    std::sort(knots.begin(), knots.end());
    const double dedupEps = 1e-12 * std::max(1.0, xEnd - xStart);
    std::vector<double> baseGrid;
    baseGrid.reserve(knots.size());
    for (double xv : knots) {
        if (baseGrid.empty() || xv - baseGrid.back() > dedupEps)
            baseGrid.push_back(xv);
    }
    if (baseGrid.size() < 2)
        return nullptr;

    // Insert exact crossover roots where (yA - yB) changes sign
    std::vector<double> xg;
    std::vector<double> yAg;
    std::vector<double> yBg;
    xg.reserve(baseGrid.size() + 16);
    yAg.reserve(baseGrid.size() + 16);
    yBg.reserve(baseGrid.size() + 16);

    for (size_t k = 0; k < baseGrid.size(); ++k) {
        const double x0 = baseGrid[k];
        const double a0 = interpLinearSorted(xA, yA, x0);
        const double b0 = interpLinearSorted(xB, yB, x0);
        xg.push_back(x0);
        yAg.push_back(a0);
        yBg.push_back(b0);

        if (k + 1 < baseGrid.size()) {
            const double x1 = baseGrid[k + 1];
            const double a1 = interpLinearSorted(xA, yA, x1);
            const double b1 = interpLinearSorted(xB, yB, x1);
            const double d0 = a0 - b0;
            const double d1 = a1 - b1;
            if ((d0 < 0.0 && d1 > 0.0) || (d0 > 0.0 && d1 < 0.0)) {
                const double t = d0 / (d0 - d1);
                if (t > 1e-6 && t < 1.0 - 1e-6) {
                    const double xc = x0 + t * (x1 - x0);
                    const double yc = a0 + t * (a1 - a0);
                    xg.push_back(xc);
                    yAg.push_back(yc);
                    yBg.push_back(yc);
                }
            }
        }
    }

    const int m = static_cast<int>(xg.size());
    if (m < 2)
        return nullptr;

    QVector<double> vx(m), vyLow(m), vyHigh(m), vDiff(m);
    for (int k = 0; k < m; ++k) {
        const size_t idx = static_cast<size_t>(k);
        vx[k] = xg[idx];
        vyLow[k] = std::min(yAg[idx], yBg[idx]);
        vyHigh[k] = std::max(yAg[idx], yBg[idx]);
        vDiff[k] = yAg[idx] - yBg[idx];
    }

    const double minCurveZ = std::min(cA->z(), cB->z());
    QwtPlotItem *bandItem = g->addShadedBand(vx, vyLow, vyHigh, fillColor,
                                             QStringLiteral("FillBetween"));
    if (bandItem) {
        bandItem->setAxis(cA->xAxis(), cA->yAxis());
        bandItem->setZ(minCurveZ - 0.25);
    }

    Column *colX = new Column(QStringLiteral("FillX"), SciDAVis::ColumnMode::Numeric);
    Column *colDiff = new Column(QStringLiteral("DiffAB"), SciDAVis::ColumnMode::Numeric);
    colX->setPlotDesignation(SciDAVis::X);
    colDiff->setPlotDesignation(SciDAVis::Y);
    colX->replaceValues(0, vx);
    colDiff->replaceValues(0, vDiff);

    const QString hidName = app->generateUniqueName(QStringLiteral("FillBetween"));
    Table *hidTbl = app->newHiddenTable(hidName, QStringLiteral("Fill Between Curves"),
                                        QList<Column *>() << colX << colDiff);
    if (!hidTbl)
        return nullptr;

    auto *diffCurve = new DataCurve(hidTbl, hidName + QStringLiteral("_") + colX->name(),
                                    hidName + QStringLiteral("_") + colDiff->name(), 0, m - 1);
    diffCurve->setType(Graph::Line);
    diffCurve->setStyle(QwtPlotCurve::Lines);
    QColor diffStroke = fillColor;
    diffStroke.setAlpha(std::clamp(fillColor.alpha() + 95, 95, 215));
    diffCurve->setPen(QPen(diffStroke, 1.2, Qt::DashLine));
    diffCurve->setBrush(QBrush(Qt::NoBrush));
    diffCurve->loadData();
    g->insertPlotItem(diffCurve, Graph::Line);
    diffCurve->setAxis(cA->xAxis(), cA->yAxis());
    diffCurve->setZ(minCurveZ - 0.15);
    g->updatePlot();
    return diffCurve;
}

CurveMathTools::EISResult CurveMathTools::analyzeEIS(
        ApplicationWindow *app, Table *srcTable, const QString &freqCol,
        const QString &zRealCol, const QString &zImagCol,
        bool zImagAlreadyNegated, bool fitRandlesCPE)
{
    registerMatSciTranslationsOnce();
    EISResult res;
    if (!srcTable)
        return res;

    Column *cFreq = resolveMatSciColumn(srcTable, freqCol);
    Column *cRe = resolveMatSciColumn(srcTable, zRealCol);
    Column *cIm = resolveMatSciColumn(srcTable, zImagCol);
    if (!cFreq || !cRe || !cIm)
        return res;

    struct EisSample {
        double f = 0.0;
        double omega = 0.0;
        double zRe = 0.0;
        double negZIm = 0.0; // capacitive positive ordinate: -Z''
    };

    const int nRows = std::min({ cFreq->rowCount(), cRe->rowCount(), cIm->rowCount() });
    std::vector<EisSample> pts;
    pts.reserve(static_cast<size_t>(std::max(0, nRows)));
    for (int r = 0; r < nRows; ++r) {
        double fv = 0.0, zr = 0.0, ziRaw = 0.0;
        if (!readMatSciCell(cFreq, r, fv) || !readMatSciCell(cRe, r, zr)
            || !readMatSciCell(cIm, r, ziRaw))
            continue;
        if (!(fv > 0.0) || !std::isfinite(zr) || !std::isfinite(ziRaw))
            continue;
        const double negZi = zImagAlreadyNegated ? ziRaw : (-ziRaw);
        pts.push_back({ fv, 2.0 * M_PI * fv, zr, negZi });
    }

    if (pts.size() < 4)
        return res;

    // Sort by ascending frequency so log10(f) is monotonic for Bode and Z' traces smoothly for Nyquist
    std::stable_sort(pts.begin(), pts.end(),
                     [](const EisSample &a, const EisSample &b) { return a.f < b.f; });

    const int N = static_cast<int>(pts.size());

    // Helper: evaluate Randles + CPE model Z(w) = Rs + Rct / (1 + Rct * Q * (j*w)^n)
    auto evalRandlesCPE = [](double omega, double rs, double rct, double q, double n,
                             double &outRe, double &outNegIm) {
        const double nClamped = std::clamp(n, 0.05, 1.0);
        const double lnAlpha = std::log(std::max(1e-300, rct)) + std::log(std::max(1e-300, q))
                + nClamped * std::log(std::max(1e-300, omega));
        if (lnAlpha > 100.0) {
            outRe = rs;
            outNegIm = 0.0;
            return;
        }
        const double alpha = (lnAlpha < -100.0) ? 0.0 : std::exp(lnAlpha);
        const double ang = 0.5 * M_PI * nClamped;
        const double c = std::cos(ang);
        const double s = std::sin(ang);
        const double denom = std::max(1e-300, 1.0 + 2.0 * alpha * c + alpha * alpha);
        outRe = rs + rct * (1.0 + alpha * c) / denom;
        outNegIm = rct * (alpha * s) / denom;
    };

    // Step 1: Taubin (1991) algebraic circle fit on (x = Z', y = -Z'')
    double meanX = 0.0, meanY = 0.0;
    double minRe = pts.front().zRe, maxRe = pts.front().zRe;
    int idxPeak = 0;
    for (int i = 0; i < N; ++i) {
        const EisSample &s = pts[static_cast<size_t>(i)];
        meanX += s.zRe;
        meanY += s.negZIm;
        minRe = std::min(minRe, s.zRe);
        maxRe = std::max(maxRe, s.zRe);
        if (s.negZIm > pts[static_cast<size_t>(idxPeak)].negZIm)
            idxPeak = i;
    }
    meanX /= static_cast<double>(N);
    meanY /= static_cast<double>(N);

    double sumR2 = 0.0;
    for (const EisSample &s : pts) {
        const double dx = s.zRe - meanX;
        const double dy = s.negZIm - meanY;
        sumR2 += dx * dx + dy * dy;
    }
    const double scale = std::sqrt(std::max(1e-24, sumR2 / static_cast<double>(N)));

    double mXX = 0.0, mXY = 0.0, mYY = 0.0, mXZ = 0.0, mYZ = 0.0, mZZ = 0.0;
    for (const EisSample &s : pts) {
        const double u = (s.zRe - meanX) / scale;
        const double v = (s.negZIm - meanY) / scale;
        const double z = u * u + v * v;
        mXX += u * u;
        mXY += u * v;
        mYY += v * v;
        mXZ += u * z;
        mYZ += v * z;
        mZZ += z * z;
    }
    const double invN = 1.0 / static_cast<double>(N);
    mXX *= invN;
    mXY *= invN;
    mYY *= invN;
    mXZ *= invN;
    mYZ *= invN;
    mZZ *= invN;

    const double mZ = mXX + mYY; // == 1.0 when normalized
    const double covXY = mXX * mYY - mXY * mXY;
    const double varZ = mZZ - mZ * mZ;
    const double a3 = 4.0 * mZ;
    const double a2 = -3.0 * mZ * mZ - mZZ;
    const double a1 = varZ * mZ + 4.0 * covXY * mZ - mXZ * mXZ - mYZ * mYZ;
    const double a0 = mXZ * (mXZ * mYY - mYZ * mXY) + mYZ * (mYZ * mXX - mXZ * mXY) - varZ * covXY;

    // Newton-Raphson from eta = 0 finds the smallest non-negative root of Taubin's characteristic polynomial
    double eta = 0.0;
    for (int it = 0; it < 30; ++it) {
        const double fVal = ((a3 * eta + a2) * eta + a1) * eta + a0;
        const double dfVal = (3.0 * a3 * eta + 2.0 * a2) * eta + a1;
        if (std::fabs(dfVal) <= 1e-30)
            break;
        const double nextEta = std::max(0.0, eta - fVal / dfVal);
        if (std::fabs(nextEta - eta) <= 1e-13) {
            eta = nextEta;
            break;
        }
        eta = nextEta;
    }

    const double cXX = mXX - eta;
    const double cYY = mYY - eta;
    const double det2 = cXX * cYY - mXY * mXY;

    double rsSeed = std::max(1e-6, std::max(0.0, pts.back().zRe));
    double rctSeed = std::max(1e-3, maxRe - minRe);
    double nSeed = 0.90;

    if (std::fabs(det2) > 1e-18) {
        const double uc = (mXZ * cYY - mYZ * mXY) / (2.0 * det2);
        const double vc = (mYZ * cXX - mXZ * mXY) / (2.0 * det2);
        const double radNorm = std::sqrt(std::max(1e-18, uc * uc + vc * vc + mZ));
        const double xc = meanX + uc * scale;
        const double yc = meanY + vc * scale;
        const double rad = radNorm * scale;
        if (std::isfinite(xc) && std::isfinite(yc) && std::isfinite(rad) && rad > 0.0) {
            const double halfChordSq = rad * rad - yc * yc;
            if (halfChordSq > 1e-18) {
                const double halfChord = std::sqrt(halfChordSq);
                rsSeed = std::max(1e-6, xc - halfChord);
                rctSeed = std::max(1e-4, 2.0 * halfChord);
                const double depAngle = std::clamp(std::atan2(-yc, halfChord), 0.0, 0.46 * M_PI);
                nSeed = std::clamp(1.0 - (2.0 * depAngle / M_PI), 0.35, 1.0);
            } else {
                rsSeed = std::max(1e-6, std::min(minRe, pts.back().zRe));
                rctSeed = std::max(1e-4, std::max(2.0 * rad, maxRe - rsSeed));
            }
        }
    } else {
        rsSeed = std::max(1e-6, std::min(minRe, pts.back().zRe));
        rctSeed = std::max(1e-4, std::max(maxRe - rsSeed, 2.0 * pts[static_cast<size_t>(idxPeak)].negZIm));
    }

    const double omegaPeakSeed = std::max(1e-9, pts[static_cast<size_t>(idxPeak)].omega);
    double qSeed = 1.0 / std::max(1e-30, rctSeed * std::pow(omegaPeakSeed, nSeed));
    qSeed = std::clamp(qSeed, 1e-18, 1e6);

    auto unpackParams = [](const double p[4], double &rs, double &rct, double &q, double &n) {
        rs = std::exp(std::clamp(p[0], -30.0, 30.0));
        rct = std::exp(std::clamp(p[1], -30.0, 30.0));
        q = std::exp(std::clamp(p[2], -45.0, 20.0));
        n = 0.05 + 0.475 * (1.0 + std::cos(p[3]));
    };

    auto packParams = [](double rs, double rct, double q, double n, double p[4]) {
        p[0] = std::log(std::clamp(rs, 1e-12, 1e12));
        p[1] = std::log(std::clamp(rct, 1e-12, 1e12));
        p[2] = std::log(std::clamp(q, 1e-19, 1e8));
        const double cosVal = std::clamp((std::clamp(n, 0.05, 1.0) - 0.05) / 0.475 - 1.0, -1.0, 1.0);
        p[3] = std::acos(cosVal);
    };

    auto evalRssParams = [&](const double p[4]) -> double {
        double rs = 0.0, rct = 0.0, q = 0.0, n = 1.0;
        unpackParams(p, rs, rct, q, n);
        double rss = 0.0;
        for (const EisSample &s : pts) {
            double mr = 0.0, mi = 0.0;
            evalRandlesCPE(s.omega, rs, rct, q, n, mr, mi);
            const double er = s.zRe - mr;
            const double ei = s.negZIm - mi;
            rss += er * er + ei * ei;
        }
        return rss;
    };

    double bestP[4] = { 0.0, 0.0, 0.0, 0.0 };
    packParams(rsSeed, rctSeed, qSeed, nSeed, bestP);
    double bestRss = evalRssParams(bestP);

    if (fitRandlesCPE) {
        // Also test ideal n = 1.0 Debye capacitor seed in case circle depression was slightly noisy
        double idealP[4] = { 0.0, 0.0, 0.0, 0.0 };
        packParams(rsSeed, rctSeed, 1.0 / std::max(1e-30, rctSeed * omegaPeakSeed), 1.0, idealP);
        const double idealRss = evalRssParams(idealP);
        if (idealRss < bestRss) {
            for (int k = 0; k < 4; ++k)
                bestP[k] = idealP[k];
            bestRss = idealRss;
        }

        // Step 2: Levenberg-Marquardt nonlinear least squares (2N residuals x 4 parameters)
        double lambda = 1e-3;
        std::vector<double> rVec(static_cast<size_t>(2 * N), 0.0);
        std::vector<double> jac(static_cast<size_t>(2 * N * 4), 0.0);

        auto solve4x4 = [](double A[4][4], double b[4], double x[4]) -> bool {
            double aug[4][5];
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c)
                    aug[r][c] = A[r][c];
                aug[r][4] = b[r];
            }
            for (int col = 0; col < 4; ++col) {
                int piv = col;
                for (int r = col + 1; r < 4; ++r) {
                    if (std::fabs(aug[r][col]) > std::fabs(aug[piv][col]))
                        piv = r;
                }
                if (std::fabs(aug[piv][col]) <= 1e-30)
                    return false;
                if (piv != col) {
                    for (int c = col; c < 5; ++c)
                        std::swap(aug[col][c], aug[piv][c]);
                }
                const double invDiag = 1.0 / aug[col][col];
                for (int c = col; c < 5; ++c)
                    aug[col][c] *= invDiag;
                for (int r = 0; r < 4; ++r) {
                    if (r == col)
                        continue;
                    const double f = aug[r][col];
                    for (int c = col; c < 5; ++c)
                        aug[r][c] -= f * aug[col][c];
                }
            }
            for (int r = 0; r < 4; ++r)
                x[r] = aug[r][4];
            return true;
        };

        for (int iter = 0; iter < 120; ++iter) {
            double rs = 0.0, rct = 0.0, q = 0.0, n = 1.0;
            unpackParams(bestP, rs, rct, q, n);
            for (int i = 0; i < N; ++i) {
                double mr = 0.0, mi = 0.0;
                evalRandlesCPE(pts[static_cast<size_t>(i)].omega, rs, rct, q, n, mr, mi);
                rVec[static_cast<size_t>(2 * i)] = mr - pts[static_cast<size_t>(i)].zRe;
                rVec[static_cast<size_t>(2 * i + 1)] = mi - pts[static_cast<size_t>(i)].negZIm;
            }

            for (int j = 0; j < 4; ++j) {
                const double h = (j == 3) ? 1e-6 : (1e-6 * std::max(1.0, std::fabs(bestP[j])));
                double pPlus[4] = { bestP[0], bestP[1], bestP[2], bestP[3] };
                double pMinus[4] = { bestP[0], bestP[1], bestP[2], bestP[3] };
                pPlus[j] += h;
                pMinus[j] -= h;
                double rsP = 0.0, rctP = 0.0, qP = 0.0, nP = 1.0;
                double rsM = 0.0, rctM = 0.0, qM = 0.0, nM = 1.0;
                unpackParams(pPlus, rsP, rctP, qP, nP);
                unpackParams(pMinus, rsM, rctM, qM, nM);
                const double inv2h = 0.5 / h;
                for (int i = 0; i < N; ++i) {
                    const double w = pts[static_cast<size_t>(i)].omega;
                    double mrP = 0.0, miP = 0.0, mrM = 0.0, miM = 0.0;
                    evalRandlesCPE(w, rsP, rctP, qP, nP, mrP, miP);
                    evalRandlesCPE(w, rsM, rctM, qM, nM, mrM, miM);
                    jac[static_cast<size_t>((2 * i) * 4 + j)] = (mrP - mrM) * inv2h;
                    jac[static_cast<size_t>((2 * i + 1) * 4 + j)] = (miP - miM) * inv2h;
                }
            }

            double jtj[4][4] = {};
            double negJtr[4] = {};
            for (int row = 0; row < 2 * N; ++row) {
                const double rv = rVec[static_cast<size_t>(row)];
                for (int j = 0; j < 4; ++j) {
                    const double jv = jac[static_cast<size_t>(row * 4 + j)];
                    negJtr[j] -= jv * rv;
                    for (int k = j; k < 4; ++k)
                        jtj[j][k] += jv * jac[static_cast<size_t>(row * 4 + k)];
                }
            }
            for (int j = 0; j < 4; ++j) {
                for (int k = 0; k < j; ++k)
                    jtj[j][k] = jtj[k][j];
            }

            bool accepted = false;
            for (int sub = 0; sub < 10; ++sub) {
                double lhs[4][4];
                for (int j = 0; j < 4; ++j) {
                    for (int k = 0; k < 4; ++k)
                        lhs[j][k] = jtj[j][k];
                    lhs[j][j] += lambda * std::max(1e-8, jtj[j][j]);
                }
                double step[4] = {};
                if (!solve4x4(lhs, negJtr, step)) {
                    lambda *= 10.0;
                    continue;
                }
                double cand[4];
                double stepNorm = 0.0;
                for (int j = 0; j < 4; ++j) {
                    cand[j] = bestP[j] + step[j];
                    stepNorm += step[j] * step[j];
                }
                const double candRss = evalRssParams(cand);
                if (std::isfinite(candRss) && candRss < bestRss) {
                    const double relImprov = (bestRss - candRss) / std::max(1e-30, bestRss);
                    for (int j = 0; j < 4; ++j)
                        bestP[j] = cand[j];
                    bestRss = candRss;
                    lambda = std::max(1e-12, lambda * 0.3);
                    accepted = true;
                    if (stepNorm < 1e-20 || (relImprov < 1e-14 && bestRss < 1e-18))
                        iter = 120;
                    break;
                }
                lambda = std::min(1e12, lambda * 5.0);
            }
            if (!accepted && lambda >= 1e11)
                break;
        }

        // Step 3: Nelder-Mead simplex polish (escapes cosine boundary saddles smoothly)
        double simp[5][4];
        double fSimp[5];
        for (int j = 0; j < 4; ++j)
            simp[0][j] = bestP[j];
        fSimp[0] = bestRss;
        const double initDelta[4] = { 0.04, 0.04, 0.08, 0.05 };
        for (int v = 1; v <= 4; ++v) {
            for (int j = 0; j < 4; ++j)
                simp[v][j] = bestP[j];
            simp[v][v - 1] += initDelta[v - 1];
            fSimp[v] = evalRssParams(simp[v]);
        }

        for (int it = 0; it < 240; ++it) {
            int order[5] = { 0, 1, 2, 3, 4 };
            std::sort(order, order + 5, [&](int a, int b) { return fSimp[a] < fSimp[b]; });
            double sortedS[5][4];
            double sortedF[5];
            for (int v = 0; v < 5; ++v) {
                sortedF[v] = fSimp[order[v]];
                for (int j = 0; j < 4; ++j)
                    sortedS[v][j] = simp[order[v]][j];
            }
            for (int v = 0; v < 5; ++v) {
                fSimp[v] = sortedF[v];
                for (int j = 0; j < 4; ++j)
                    simp[v][j] = sortedS[v][j];
            }
            if (std::fabs(fSimp[4] - fSimp[0]) <= 1e-22 * std::max(1.0, fSimp[0]))
                break;

            double cent[4] = {};
            for (int v = 0; v < 4; ++v) {
                for (int j = 0; j < 4; ++j)
                    cent[j] += 0.25 * simp[v][j];
            }

            double xr[4];
            for (int j = 0; j < 4; ++j)
                xr[j] = cent[j] + (cent[j] - simp[4][j]);
            const double fr = evalRssParams(xr);

            if (fr < fSimp[0]) {
                double xe[4];
                for (int j = 0; j < 4; ++j)
                    xe[j] = cent[j] + 2.0 * (xr[j] - cent[j]);
                const double fe = evalRssParams(xe);
                for (int j = 0; j < 4; ++j)
                    simp[4][j] = (fe < fr) ? xe[j] : xr[j];
                fSimp[4] = std::min(fe, fr);
            } else if (fr < fSimp[3]) {
                for (int j = 0; j < 4; ++j)
                    simp[4][j] = xr[j];
                fSimp[4] = fr;
            } else {
                double xc[4];
                for (int j = 0; j < 4; ++j)
                    xc[j] = cent[j] + 0.5 * (((fr < fSimp[4]) ? xr[j] : simp[4][j]) - cent[j]);
                const double fc = evalRssParams(xc);
                if (fc < std::min(fr, fSimp[4])) {
                    for (int j = 0; j < 4; ++j)
                        simp[4][j] = xc[j];
                    fSimp[4] = fc;
                } else {
                    for (int v = 1; v < 5; ++v) {
                        for (int j = 0; j < 4; ++j)
                            simp[v][j] = simp[0][j] + 0.5 * (simp[v][j] - simp[0][j]);
                        fSimp[v] = evalRssParams(simp[v]);
                    }
                }
            }
        }
        int bestIdx = 0;
        for (int v = 1; v < 5; ++v) {
            if (fSimp[v] < fSimp[bestIdx])
                bestIdx = v;
        }
        if (fSimp[bestIdx] < bestRss) {
            bestRss = fSimp[bestIdx];
            for (int j = 0; j < 4; ++j)
                bestP[j] = simp[bestIdx][j];
        }
    }

    unpackParams(bestP, res.rs, res.rct, res.qCpe, res.nCpe);
    res.cEff = std::pow(std::max(1e-300, res.qCpe * std::pow(std::max(1e-300, res.rct), 1.0 - res.nCpe)),
                        1.0 / std::max(0.05, res.nCpe));
    res.fPeakHz = 1.0 / (2.0 * M_PI * std::pow(std::max(1e-300, res.rct * res.qCpe),
                                               1.0 / std::max(0.05, res.nCpe)));
    res.rmse = std::sqrt(std::max(0.0, bestRss / static_cast<double>(2 * N)));
    res.valid = true;

    if (!app)
        return res;

    // Build experimental + fitted worksheet
    std::vector<double> colFreq(static_cast<size_t>(N));
    std::vector<double> colLogF(static_cast<size_t>(N));
    std::vector<double> colZRe(static_cast<size_t>(N));
    std::vector<double> colNegZIm(static_cast<size_t>(N));
    std::vector<double> colZReFit(static_cast<size_t>(N));
    std::vector<double> colNegZImFit(static_cast<size_t>(N));
    std::vector<double> colLogMag(static_cast<size_t>(N));
    std::vector<double> colLogMagFit(static_cast<size_t>(N));
    std::vector<double> colPhase(static_cast<size_t>(N));
    std::vector<double> colPhaseFit(static_cast<size_t>(N));

    for (int i = 0; i < N; ++i) {
        const size_t idx = static_cast<size_t>(i);
        const EisSample &s = pts[idx];
        double mr = 0.0, mNegIm = 0.0;
        evalRandlesCPE(s.omega, res.rs, res.rct, res.qCpe, res.nCpe, mr, mNegIm);
        const double zIm = -s.negZIm;
        const double zImFit = -mNegIm;
        colFreq[idx] = s.f;
        colLogF[idx] = std::log10(s.f);
        colZRe[idx] = s.zRe;
        colNegZIm[idx] = s.negZIm;
        colZReFit[idx] = mr;
        colNegZImFit[idx] = mNegIm;
        colLogMag[idx] = std::log10(std::max(1e-30, std::hypot(s.zRe, zIm)));
        colLogMagFit[idx] = std::log10(std::max(1e-30, std::hypot(mr, zImFit)));
        colPhase[idx] = std::atan2(zIm, s.zRe) * (180.0 / M_PI);
        colPhaseFit[idx] = std::atan2(zImFit, mr) * (180.0 / M_PI);
    }

    Table *resTbl = createNumericTable(
            app, QStringLiteral("EISData"),
            QStringList() << QStringLiteral("FreqHz") << QStringLiteral("LogFreqHz")
                          << QStringLiteral("ZReal") << QStringLiteral("NegZImag")
                          << QStringLiteral("ZRealFit") << QStringLiteral("NegZImagFit")
                          << QStringLiteral("LogMagZ") << QStringLiteral("LogMagZFit")
                          << QStringLiteral("PhaseDeg") << QStringLiteral("PhaseDegFit"),
            QList<SciDAVis::PlotDesignation>()
                    << SciDAVis::X << SciDAVis::X << SciDAVis::X << SciDAVis::Y << SciDAVis::X
                    << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y,
            { colFreq, colLogF, colZRe, colNegZIm, colZReFit, colNegZImFit,
              colLogMag, colLogMagFit, colPhase, colPhaseFit });
    res.resultTable = resTbl;
    if (!resTbl)
        return res;

    // Dense 200-point Randles-CPE arc backing table for smooth Nyquist semicircle & Bode fit curves
    constexpr int kArcPts = 200;
    const double logFMin = colLogF.front() - 0.15;
    const double logFMax = colLogF.back() + 0.15;
    QVector<qreal> arcLogF(kArcPts), arcZRe(kArcPts), arcNegZIm(kArcPts);
    QVector<qreal> arcLogMag(kArcPts), arcPhase(kArcPts);
    for (int k = 0; k < kArcPts; ++k) {
        const double lf = logFMin + (logFMax - logFMin) * static_cast<double>(k) / (kArcPts - 1);
        const double w = 2.0 * M_PI * std::pow(10.0, lf);
        double mr = 0.0, mNegIm = 0.0;
        evalRandlesCPE(w, res.rs, res.rct, res.qCpe, res.nCpe, mr, mNegIm);
        arcLogF[k] = lf;
        arcZRe[k] = mr;
        arcNegZIm[k] = mNegIm;
        arcLogMag[k] = std::log10(std::max(1e-30, std::hypot(mr, -mNegIm)));
        arcPhase[k] = std::atan2(-mNegIm, mr) * (180.0 / M_PI);
    }

    Column *cArcLogF = new Column(QStringLiteral("ArcLogF"), SciDAVis::ColumnMode::Numeric);
    Column *cArcZRe = new Column(QStringLiteral("ArcZRe"), SciDAVis::ColumnMode::Numeric);
    Column *cArcNegZIm = new Column(QStringLiteral("ArcNegZIm"), SciDAVis::ColumnMode::Numeric);
    Column *cArcLogMag = new Column(QStringLiteral("ArcLogMag"), SciDAVis::ColumnMode::Numeric);
    Column *cArcPhase = new Column(QStringLiteral("ArcPhase"), SciDAVis::ColumnMode::Numeric);
    cArcLogF->setPlotDesignation(SciDAVis::X);
    cArcZRe->setPlotDesignation(SciDAVis::X);
    cArcNegZIm->setPlotDesignation(SciDAVis::Y);
    cArcLogMag->setPlotDesignation(SciDAVis::Y);
    cArcPhase->setPlotDesignation(SciDAVis::Y);
    cArcLogF->replaceValues(0, arcLogF);
    cArcZRe->replaceValues(0, arcZRe);
    cArcNegZIm->replaceValues(0, arcNegZIm);
    cArcLogMag->replaceValues(0, arcLogMag);
    cArcPhase->replaceValues(0, arcPhase);

    const QString hidArcName = app->generateUniqueName(QStringLiteral("EISModelArc"));
    Table *arcTbl = app->newHiddenTable(
            hidArcName, QStringLiteral("EIS Randles-CPE Smooth Fit"),
            QList<Column *>() << cArcLogF << cArcZRe << cArcNegZIm << cArcLogMag << cArcPhase);

    // 1. Nyquist Plot (-Z'' vs Z' with 1:1 isometric equal-span axes)
    MultiLayer *mlNyq = app->newGraph(makeUniqueTableName(app, QStringLiteral("EISNyquistPlot")));
    res.nyquistPlot = mlNyq;
    if (mlNyq) {
        mlNyq->resize(560, 540);
        if (Graph *gNyq = mlNyq->activeGraph()) {
            if (gNyq->insertCurve(resTbl, resTbl->colName(2), resTbl->colName(3),
                                  Graph::Scatter, 0, N - 1)) {
                const int idx = gNyq->curves() - 1;
                gNyq->setCurvePen(idx, QPen(QColor(25, 75, 155), 1.5));
                gNyq->setCurveSymbol(idx,
                                     QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(40, 105, 195)),
                                               QPen(QColor(15, 45, 95), 1.2), QSize(8, 8)));
            }
            if (arcTbl) {
                insertStyledCurve(gNyq, arcTbl, 1, 2, kArcPts,
                                  QPen(QColor(205, 30, 40), 2.2, Qt::SolidLine));
            } else {
                insertStyledCurve(gNyq, resTbl, 4, 5, N,
                                  QPen(QColor(205, 30, 40), 2.2, Qt::SolidLine));
            }

            double xLo = *std::min_element(colZRe.begin(), colZRe.end());
            double xHi = *std::max_element(colZRe.begin(), colZRe.end());
            double yLo = std::min(0.0, *std::min_element(colNegZIm.begin(), colNegZIm.end()));
            double yHi = *std::max_element(colNegZIm.begin(), colNegZIm.end());
            xLo = std::min(xLo, res.rs);
            xHi = std::max(xHi, res.rs + res.rct);
            for (int k = 0; k < kArcPts; ++k) {
                xLo = std::min(xLo, static_cast<double>(arcZRe[k]));
                xHi = std::max(xHi, static_cast<double>(arcZRe[k]));
                yLo = std::min(yLo, static_cast<double>(arcNegZIm[k]));
                yHi = std::max(yHi, static_cast<double>(arcNegZIm[k]));
            }
            const double rawSpan = std::max({ 1e-6, xHi - xLo, yHi - yLo });
            const double isoSpan = rawSpan * 1.16;
            const double xMid = 0.5 * (xLo + xHi);
            const double isoXStart = xMid - 0.5 * isoSpan;
            const double isoXEnd = xMid + 0.5 * isoSpan;
            const double isoYStart = yLo - 0.05 * isoSpan;
            const double isoYEnd = isoYStart + isoSpan;

            gNyq->setScale(QwtPlot::xBottom, isoXStart, isoXEnd);
            gNyq->setScale(QwtPlot::yLeft, isoYStart, isoYEnd);
            gNyq->setXAxisTitle(QStringLiteral("<i>Z</i>' / \u03A9"));
            gNyq->setYAxisTitle(QStringLiteral("-<i>Z</i>'' / \u03A9"));
            gNyq->setTitle(QString());
            if (gNyq->legend())
                gNyq->legend()->setText(gNyq->legendText());
            applyArialToMatSciGraph(gNyq);

            const QString summary = QStringLiteral(
                    "<i>R</i><sub>s</sub> = %1 \u03A9,  <i>R</i><sub>ct</sub> = %2 \u03A9\n"
                    "<i>Q</i> = %3 S s<sup><i>n</i></sup>,  <i>n</i> = %4\n"
                    "<i>C</i><sub>eff</sub> = %5 F,  <i>f</i><sub>peak</sub> = %6 Hz")
                    .arg(res.rs, 0, 'g', 4)
                    .arg(res.rct, 0, 'g', 4)
                    .arg(res.qCpe, 0, 'e', 2)
                    .arg(res.nCpe, 0, 'f', 3)
                    .arg(res.cEff, 0, 'e', 2)
                    .arg(res.fPeakHz, 0, 'g', 4);
            addMatSciCalloutBox(gNyq, isoXStart + 0.05 * isoSpan,
                                isoYStart + 0.93 * isoSpan, summary);
            gNyq->replot();
        }
    }

    // 2. Dual-Y Bode Plot (log10|Z| on yLeft & Phase deg on yRight vs log10(f))
    MultiLayer *mlBode = app->newGraph(makeUniqueTableName(app, QStringLiteral("EISBodePlot")));
    res.bodePlot = mlBode;
    if (mlBode) {
        if (Graph *gBode = mlBode->activeGraph()) {
            gBode->enableAxis(QwtPlot::yRight, true);

            if (gBode->insertCurve(resTbl, resTbl->colName(1), resTbl->colName(6),
                                   Graph::Scatter, 0, N - 1)) {
                const int idx = gBode->curves() - 1;
                gBode->setCurvePen(idx, QPen(QColor(25, 75, 155), 1.4));
                gBode->setCurveSymbol(idx,
                                      QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(35, 95, 185)),
                                                QPen(QColor(15, 45, 95), 1.1), QSize(7, 7)));
            }
            if (arcTbl) {
                insertStyledCurve(gBode, arcTbl, 0, 3, kArcPts,
                                  QPen(QColor(25, 75, 155), 2.1, Qt::SolidLine));
            } else {
                insertStyledCurve(gBode, resTbl, 1, 7, N,
                                  QPen(QColor(25, 75, 155), 2.1, Qt::SolidLine));
            }

            if (gBode->insertCurve(resTbl, resTbl->colName(1), resTbl->colName(8),
                                   Graph::Scatter, 0, N - 1)) {
                const int idx = gBode->curves() - 1;
                gBode->setCurvePen(idx, QPen(QColor(195, 55, 30), 1.4));
                gBode->setCurveSymbol(idx,
                                      QwtSymbol(QwtSymbol::Triangle, QBrush(QColor(215, 75, 40)),
                                                QPen(QColor(145, 35, 15), 1.1), QSize(7, 7)));
                if (QwtPlotCurve *cPhPts = gBode->curve(idx))
                    cPhPts->setAxis(QwtPlot::xBottom, QwtPlot::yRight);
            }
            QwtPlotCurve *cPhFit = arcTbl
                    ? insertStyledCurve(gBode, arcTbl, 0, 4, kArcPts,
                                        QPen(QColor(195, 55, 30), 2.0, Qt::DashLine))
                    : insertStyledCurve(gBode, resTbl, 1, 9, N,
                                        QPen(QColor(195, 55, 30), 2.0, Qt::DashLine));
            if (cPhFit)
                cPhFit->setAxis(QwtPlot::xBottom, QwtPlot::yRight);

            const auto magMM = std::minmax_element(colLogMag.begin(), colLogMag.end());
            const double magSpan = std::max(0.2, *magMM.second - *magMM.first);
            const auto phMM = std::minmax_element(colPhase.begin(), colPhase.end());
            const double phLo = std::min(-90.0, *phMM.first - 5.0);
            const double phHi = std::max(5.0, *phMM.second + 5.0);

            gBode->setScale(QwtPlot::xBottom, logFMin, logFMax);
            gBode->setScale(QwtPlot::yLeft, *magMM.first - 0.12 * magSpan,
                            *magMM.second + 0.15 * magSpan);
            gBode->setScale(QwtPlot::yRight, phLo, phHi);

            gBode->setXAxisTitle(QStringLiteral("log<sub>10</sub>(<i>f</i> / Hz)"));
            gBode->setYAxisTitle(QStringLiteral("log<sub>10</sub>(|<i>Z</i>| / \u03A9)"));
            gBode->setRightAxisTitle(QObject::tr("Phase (°)"));
            gBode->setTitle(QString());
            gBode->setYAxisTitleColor(QColor(25, 75, 155));
            gBode->setRightAxisTitleColor(QColor(195, 55, 30));
            if (gBode->legend())
                gBode->legend()->setText(gBode->legendText());
            applyArialToMatSciGraph(gBode);
            gBode->replot();
        }
    }

    return res;
}

void CurveMathTools::showArrheniusDialog(ApplicationWindow *app)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    registerMatSciTranslationsOnce();
    const QList<Table *> tables = collectProjectTables(app);
    if (tables.isEmpty()) {
        QMessageBox::information(app, QObject::tr("Arrhenius Plot (Activation Energy)"),
                                 QObject::tr("No numeric worksheets are open."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Arrhenius Plot (Activation Energy)"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *activeTbl = findActiveOrFirstTable(app);
    int initIdx = 0;
    for (int i = 0; i < tables.size(); ++i) {
        cbTable->addItem(tables[i]->name());
        if (tables[i] == activeTbl)
            initIdx = i;
    }
    cbTable->setCurrentIndex(initIdx);

    QComboBox *cbTemp = new QComboBox(&dlg);
    QComboBox *cbRate = new QComboBox(&dlg);
    auto repopulateCols = [&](int tblIdx) {
        cbTemp->clear();
        cbRate->clear();
        if (tblIdx < 0 || tblIdx >= tables.size())
            return;
        Table *t = tables[tblIdx];
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!matSciColumnHasNumeric(col))
                continue;
            const QString cn = col ? col->name() : t->colLabel(c);
            cbTemp->addItem(cn);
            cbRate->addItem(cn);
        }
        if (cbRate->count() >= 2)
            cbRate->setCurrentIndex(1);
    };
    repopulateCols(initIdx);
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     repopulateCols);

    QCheckBox *chkCelsius =
            new QCheckBox(QObject::tr("Temperature in Celsius (°C -> +273.15 K)"), &dlg);
    QCheckBox *chk1000 = new QCheckBox(QObject::tr("Plot vs. 1000 / T (K^-1)"), &dlg);
    chk1000->setChecked(true);
    QCheckBox *chkSigmaT =
            new QCheckBox(QObject::tr("Modified Arrhenius: ln(k * T) (Ionic Conductivity)"), &dlg);

    form->addRow(QObject::tr("Source Worksheet:"), cbTable);
    form->addRow(QObject::tr("Temperature Column:"), cbTemp);
    form->addRow(QObject::tr("Rate / Conductivity Column:"), cbRate);
    form->addRow(QString(), chkCelsius);
    form->addRow(QString(), chk1000);
    form->addRow(QString(), chkSigmaT);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const int idx = cbTable->currentIndex();
        if (idx >= 0 && idx < tables.size()) {
            createArrheniusPlot(app, tables[idx], cbTemp->currentText(), cbRate->currentText(),
                                chkCelsius->isChecked(), chk1000->isChecked(),
                                chkSigmaT->isChecked());
        }
    }
}

void CurveMathTools::showTaucDialog(ApplicationWindow *app)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    registerMatSciTranslationsOnce();
    const QList<Table *> tables = collectProjectTables(app);
    if (tables.isEmpty()) {
        QMessageBox::information(app, QObject::tr("Tauc Plot (Optical Bandgap)"),
                                 QObject::tr("No numeric worksheets are open."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Tauc Plot (Optical Bandgap)"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *activeTbl = findActiveOrFirstTable(app);
    int initIdx = 0;
    for (int i = 0; i < tables.size(); ++i) {
        cbTable->addItem(tables[i]->name());
        if (tables[i] == activeTbl)
            initIdx = i;
    }
    cbTable->setCurrentIndex(initIdx);

    QComboBox *cbWave = new QComboBox(&dlg);
    QComboBox *cbAlpha = new QComboBox(&dlg);
    auto repopulateCols = [&](int tblIdx) {
        cbWave->clear();
        cbAlpha->clear();
        if (tblIdx < 0 || tblIdx >= tables.size())
            return;
        Table *t = tables[tblIdx];
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!matSciColumnHasNumeric(col))
                continue;
            const QString cn = col ? col->name() : t->colLabel(c);
            cbWave->addItem(cn);
            cbAlpha->addItem(cn);
        }
        if (cbAlpha->count() >= 2)
            cbAlpha->setCurrentIndex(1);
    };
    repopulateCols(initIdx);
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     repopulateCols);

    QComboBox *cbUnit = new QComboBox(&dlg);
    cbUnit->addItem(QObject::tr("Wavelength λ (nm -> hν = 1239.84 / λ eV)"), true);
    cbUnit->addItem(QObject::tr("Photon Energy hν (eV)"), false);

    QComboBox *cbTransition = new QComboBox(&dlg);
    cbTransition->addItem(QObject::tr("Direct Allowed: (αhν)^2"), true);
    cbTransition->addItem(QObject::tr("Indirect Allowed: (αhν)^(1/2)"), false);

    form->addRow(QObject::tr("Source Worksheet:"), cbTable);
    form->addRow(QObject::tr("Wavelength / Energy Column:"), cbWave);
    form->addRow(QObject::tr("Absorption Coefficient Column (α):"), cbAlpha);
    form->addRow(QObject::tr("X Column Unit:"), cbUnit);
    form->addRow(QObject::tr("Transition Type:"), cbTransition);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const int idx = cbTable->currentIndex();
        if (idx >= 0 && idx < tables.size()) {
            createTaucPlot(app, tables[idx], cbWave->currentText(), cbAlpha->currentText(),
                           cbUnit->currentData().toBool(), cbTransition->currentData().toBool());
        }
    }
}

void CurveMathTools::showFillBetweenDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    registerMatSciTranslationsOnce();
    Graph *targetGraph = findActiveOrFirstGraph(app, g);
    if (!targetGraph || targetGraph->analysableCurvesList().size() < 2) {
        QMessageBox::information(
                app, QObject::tr("Fill Area Between Curves"),
                QObject::tr("Please open a graph with at least two curves first."));
        return;
    }

    const QStringList curves = targetGraph->analysableCurvesList();
    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Fill Area Between Curves"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbA = new QComboBox(&dlg);
    QComboBox *cbB = new QComboBox(&dlg);
    cbA->addItems(curves);
    cbB->addItems(curves);
    cbB->setCurrentIndex(1);

    QColor chosenColor(30, 102, 184, 55);
    QLabel *lblSwatch = new QLabel(&dlg);
    lblSwatch->setFixedSize(36, 22);
    auto updateSwatch = [&]() {
        lblSwatch->setStyleSheet(
                QStringLiteral("background-color: rgba(%1,%2,%3,%4); border: 1px solid #555;")
                        .arg(chosenColor.red())
                        .arg(chosenColor.green())
                        .arg(chosenColor.blue())
                        .arg(chosenColor.alpha()));
    };
    updateSwatch();

    QPushButton *btnColor = new QPushButton(QObject::tr("Choose Color..."), &dlg);
    QSpinBox *sbOpacity = new QSpinBox(&dlg);
    sbOpacity->setRange(5, 100);
    sbOpacity->setValue(std::clamp(static_cast<int>(std::round(chosenColor.alpha() * 100.0 / 255.0)),
                                   5, 100));

    QHBoxLayout *colorRow = new QHBoxLayout();
    colorRow->addWidget(lblSwatch);
    colorRow->addWidget(btnColor);

    QObject::connect(btnColor, &QPushButton::clicked, &dlg, [&]() {
        const QColor c = QColorDialog::getColor(chosenColor, &dlg,
                                                QObject::tr("Fill Area Between Curves"));
        if (c.isValid()) {
            chosenColor.setRgb(c.red(), c.green(), c.blue(),
                               std::clamp(sbOpacity->value() * 255 / 100, 10, 255));
            updateSwatch();
        }
    });
    QObject::connect(sbOpacity, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, [&](int pct) {
        chosenColor.setAlpha(std::clamp(pct * 255 / 100, 10, 255));
        updateSwatch();
    });

    form->addRow(QObject::tr("First Curve (Boundary A):"), cbA);
    form->addRow(QObject::tr("Second Curve (Boundary B):"), cbB);
    form->addRow(QObject::tr("Fill Color & Opacity:"), colorRow);
    form->addRow(QObject::tr("Fill Opacity (%):"), sbOpacity);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        fillBetweenCurves(app, targetGraph, cbA->currentText(), cbB->currentText(), chosenColor);
    }
}

void CurveMathTools::showEISAnalyzerDialog(ApplicationWindow *app)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    registerMatSciTranslationsOnce();
    const QList<Table *> allTables = collectProjectTables(app);
    QList<Table *> tables;
    for (Table *t : allTables) {
        int numValidCols = 0;
        for (int c = 0; c < t->numCols(); ++c) {
            if (matSciColumnHasNumeric(t->column(c)))
                ++numValidCols;
        }
        if (numValidCols >= 3)
            tables.append(t);
    }
    if (tables.isEmpty()) {
        QMessageBox::information(
                app, QObject::tr("EIS Nyquist & Bode Analyzer (Randles-CPE)"),
                QObject::tr("Please open a worksheet with at least 3 numeric columns (Frequency, Z', Z'')."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("EIS Nyquist & Bode Analyzer (Randles-CPE)"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *activeTbl = findActiveOrFirstTable(app);
    int initIdx = 0;
    for (int i = 0; i < tables.size(); ++i) {
        cbTable->addItem(tables[i]->name());
        if (tables[i] == activeTbl)
            initIdx = i;
    }
    cbTable->setCurrentIndex(initIdx);

    QComboBox *cbFreq = new QComboBox(&dlg);
    QComboBox *cbRe = new QComboBox(&dlg);
    QComboBox *cbIm = new QComboBox(&dlg);
    auto repopulateCols = [&](int tblIdx) {
        cbFreq->clear();
        cbRe->clear();
        cbIm->clear();
        if (tblIdx < 0 || tblIdx >= tables.size())
            return;
        Table *t = tables[tblIdx];
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!matSciColumnHasNumeric(col))
                continue;
            const QString cn = col ? col->name() : t->colLabel(c);
            cbFreq->addItem(cn);
            cbRe->addItem(cn);
            cbIm->addItem(cn);
        }
        if (cbRe->count() >= 2)
            cbRe->setCurrentIndex(1);
        if (cbIm->count() >= 3)
            cbIm->setCurrentIndex(2);
    };
    repopulateCols(initIdx);
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     repopulateCols);

    QCheckBox *chkNegated =
            new QCheckBox(QObject::tr("Imaginary column is already negated (-Z'' > 0)"), &dlg);
    chkNegated->setChecked(false);
    QCheckBox *chkFit = new QCheckBox(
            QObject::tr("Fit Randles + CPE Equivalent Circuit (Rs + Rct || CPE)"), &dlg);
    chkFit->setChecked(true);

    form->addRow(QObject::tr("Source Worksheet:"), cbTable);
    form->addRow(QObject::tr("Frequency Column (Hz):"), cbFreq);
    form->addRow(QObject::tr("Real Impedance Column (Z'):"), cbRe);
    form->addRow(QObject::tr("Imaginary Impedance Column (Z'' or -Z''):"), cbIm);
    form->addRow(QString(), chkNegated);
    form->addRow(QString(), chkFit);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const int idx = cbTable->currentIndex();
        if (idx >= 0 && idx < tables.size()) {
            const EISResult eis = analyzeEIS(
                    app, tables[idx], cbFreq->currentText(), cbRe->currentText(),
                    cbIm->currentText(), chkNegated->isChecked(), chkFit->isChecked());
            if (!eis.valid) {
                QMessageBox::warning(
                        app, QObject::tr("EIS Nyquist & Bode Analyzer (Randles-CPE)"),
                        QObject::tr("EIS analysis requires at least 4 valid positive-frequency data points."));
            }
        }
    }
}

CurveMathTools::TensileResult CurveMathTools::analyzeTensile(
        ApplicationWindow *app, Graph *g, const QString &curveTitle,
        double offsetFraction, int strainInPercentMode,
        double elasticXMin, double elasticXMax,
        bool plotConstruction, bool addCallout, bool createTrueStressTable)
{
    registerMatSciTranslationsOnce();
    TensileResult res;
    res.offsetFraction =
            (std::isfinite(offsetFraction) && offsetFraction > 0.0) ? offsetFraction : 0.002;

    Graph *targetGraph = findActiveOrFirstGraph(app, g);
    if (!targetGraph) {
        res.error = QObject::tr("No active graph found for tensile analysis.");
        return res;
    }
    res.plot = targetGraph->multiLayerParent();

    QwtPlotCurve *srcCurve = nullptr;
    if (!curveTitle.trimmed().isEmpty()) {
        srcCurve = resolveCurve(targetGraph, curveTitle);
        if (!srcCurve) {
            res.error = QObject::tr("Curve '%1' not found on graph.").arg(curveTitle);
            return res;
        }
    } else {
        const QList<QwtPlotCurve *> fitList = targetGraph->fitCurvesList();
        const QStringList cands = targetGraph->analysableCurvesList();
        for (const QString &cn : cands) {
            QwtPlotCurve *cand = targetGraph->curve(cn);
            if (!cand || fitList.contains(cand))
                continue;
            if (auto *dc = dynamic_cast<DataCurve *>(cand)) {
                if (dc->table()
                    && (dc->table()->objectName().startsWith(QStringLiteral("GadgetFit_"))
                        || dc->table()->objectName().startsWith(QStringLiteral("RpcFitCurve")))) {
                    continue;
                }
            }
            srcCurve = cand;
            break;
        }
        if (!srcCurve && targetGraph->curves() > 0)
            srcCurve = targetGraph->curve(0);
        if (!srcCurve) {
            res.error = QObject::tr("No data curves available on graph.");
            return res;
        }
    }

    DataCurve *srcDc = dynamic_cast<DataCurve *>(srcCurve);
    Table *srcTbl = srcDc ? srcDc->table() : nullptr;
    Column *xCol = (srcTbl && srcDc) ? srcTbl->column(srcDc->xColumnName()) : nullptr;
    Column *yCol = (srcTbl && srcDc) ? srcTbl->column(srcDc->yColumnName()) : nullptr;

    struct TensilePt
    {
        double x = 0.0;
        double y = 0.0;
    };
    std::vector<TensilePt> pts;
    const int rawN = srcCurve->dataSize();
    pts.reserve(static_cast<size_t>(std::max(0, rawN)));
    for (int i = 0; i < rawN; ++i) {
        if (srcDc && srcTbl) {
            int row = srcDc->tableRow(i);
            if (row < 0 && yCol && rawN == yCol->rowCount())
                row = i;
            if (row >= 0 && ((xCol && xCol->isMasked(row)) || (yCol && yCol->isMasked(row))))
                continue;
        }
        const double xv = srcCurve->x(i);
        const double yv = srcCurve->y(i);
        if (std::isfinite(xv) && std::isfinite(yv))
            pts.push_back({ xv, yv });
    }

    if (pts.size() < 4) {
        res.error = QObject::tr("At least 4 valid data points are required for tensile analysis.");
        return res;
    }

    // Find global peak stress (UTS) and fracture point
    size_t utsIdx = 0;
    double minX = pts.front().x;
    double maxX = pts.front().x;
    for (size_t i = 0; i < pts.size(); ++i) {
        minX = std::min(minX, pts[i].x);
        maxX = std::max(maxX, pts[i].x);
        if (pts[i].y > pts[utsIdx].y)
            utsIdx = i;
    }
    res.utsX = pts[utsIdx].x;
    res.utsY = pts[utsIdx].y;
    if (!(res.utsY > 0.0)) {
        res.error = QObject::tr("Tensile stress values must contain positive stress.");
        return res;
    }

    size_t fracIdx = pts.size() - 1;
    for (size_t i = utsIdx + 1; i < pts.size(); ++i) {
        if (pts[i].y <= 0.15 * res.utsY || pts[i].y < 0.5 * pts[i - 1].y) {
            fracIdx = i - 1;
            break;
        }
    }
    res.fractureX = pts[fracIdx].x;
    res.fractureY = pts[fracIdx].y;

    // Auto-detect strain in % vs dimensionless fraction
    bool strainInPercent = (maxX > 1.2);
    if (strainInPercentMode == 1) {
        strainInPercent = true;
    } else if (strainInPercentMode == 0) {
        strainInPercent = false;
    } else {
        const QString xUnit = xCol ? xCol->units().trimmed() : QString();
        const QString xTitle = targetGraph->plotWidget()
                ? targetGraph->plotWidget()->axisTitle(srcCurve->xAxis()).text()
                : QString();
        if (xUnit == QLatin1String("%")
            || (xTitle.contains(QLatin1Char('%')) && !xTitle.contains(QLatin1String("wt%"), Qt::CaseInsensitive)
                && !xTitle.contains(QLatin1String("at%"), Qt::CaseInsensitive))) {
            strainInPercent = true;
        } else if ((xUnit == QLatin1String("-") || xUnit == QStringLiteral("\u2014")
                    || xUnit.compare(QLatin1String("mm/mm"), Qt::CaseInsensitive) == 0
                    || xUnit.compare(QLatin1String("m/m"), Qt::CaseInsensitive) == 0
                    || xTitle.contains(QLatin1String("(mm/mm)"), Qt::CaseInsensitive)
                    || xTitle.contains(QLatin1String("(-)")))
                   && maxX <= 1.2) {
            strainInPercent = false;
        }
    }
    res.strainInPercent = strainInPercent;

    // Detect stress unit (default MPa)
    QString stressUnit = yCol ? yCol->units().trimmed() : QString();
    if (stressUnit.isEmpty() && targetGraph->plotWidget()) {
        const QString yTitle = targetGraph->plotWidget()->axisTitle(srcCurve->yAxis()).text();
        if (yTitle.contains(QLatin1String("GPa")))
            stressUnit = QStringLiteral("GPa");
        else if (yTitle.contains(QLatin1String("kPa")))
            stressUnit = QStringLiteral("kPa");
        else if (yTitle.contains(QLatin1String("MPa")))
            stressUnit = QStringLiteral("MPa");
        else if (yTitle.contains(QLatin1String("(Pa)")) || yTitle.contains(QLatin1String("[Pa]"))
                 || yTitle.endsWith(QLatin1String("/ Pa")))
            stressUnit = QStringLiteral("Pa");
    }
    if (stressUnit.isEmpty())
        stressUnit = QStringLiteral("MPa");
    res.stressUnit = stressUnit;

    // Fit linear-elastic slope on rising branch [0 .. utsIdx]
    const size_t nRise = std::max<size_t>(2, std::min(pts.size(), utsIdx + 1));

    auto olsRange = [&](const std::vector<TensilePt> &sub, double &outSlope, double &outIntercept,
                        double &outR2) -> bool {
        const size_t m = sub.size();
        if (m < 2)
            return false;
        double sx = 0.0, sy = 0.0;
        for (const auto &p : sub) {
            sx += p.x;
            sy += p.y;
        }
        const double mx = sx / double(m);
        const double my = sy / double(m);
        double sxx = 0.0, sxy = 0.0, syy = 0.0;
        for (const auto &p : sub) {
            const double dx = p.x - mx;
            const double dy = p.y - my;
            sxx += dx * dx;
            sxy += dx * dy;
            syy += dy * dy;
        }
        if (sxx <= 1e-30 || sxy <= 0.0)
            return false;
        outSlope = sxy / sxx;
        outIntercept = my - outSlope * mx;
        const double rss = std::max(0.0, syy - outSlope * sxy);
        outR2 = (syy > 1e-30) ? std::clamp(1.0 - rss / syy, 0.0, 1.0) : 1.0;
        return std::isfinite(outSlope) && outSlope > 0.0;
    };

    bool hasElasticFit = false;
    const bool manualRange = std::isfinite(elasticXMin) || std::isfinite(elasticXMax);
    if (manualRange) {
        double xLo = std::isfinite(elasticXMin) ? elasticXMin : minX;
        double xHi = std::isfinite(elasticXMax) ? elasticXMax : pts[utsIdx].x;
        if (xLo > xHi)
            std::swap(xLo, xHi);
        std::vector<TensilePt> sel;
        for (size_t i = 0; i < nRise; ++i) {
            if (pts[i].x >= xLo && pts[i].x <= xHi)
                sel.push_back(pts[i]);
        }
        double b = 0.0, a = 0.0, r2 = 0.0;
        if (olsRange(sel, b, a, r2)) {
            res.elasticSlopeRaw = b;
            res.elasticIntercept = a;
            res.elasticR2 = r2;
            res.elasticXMin = sel.front().x;
            res.elasticXMax = sel.back().x;
            hasElasticFit = true;
        }
    } else {
        // Prefix sums on [0 .. nRise - 1] for fast sliding-window elastic discovery
        std::vector<double> pX(nRise + 1, 0.0), pY(nRise + 1, 0.0);
        std::vector<double> pXX(nRise + 1, 0.0), pYY(nRise + 1, 0.0), pXY(nRise + 1, 0.0);
        for (size_t k = 0; k < nRise; ++k) {
            const double xk = pts[k].x;
            const double yk = pts[k].y;
            pX[k + 1] = pX[k] + xk;
            pY[k + 1] = pY[k] + yk;
            pXX[k + 1] = pXX[k] + xk * xk;
            pYY[k + 1] = pYY[k] + yk * yk;
            pXY[k + 1] = pXY[k] + xk * yk;
        }

        struct ElasticCand
        {
            size_t i = 0;
            size_t j = 1;
            double b = 0.0;
            double a = 0.0;
            double r2 = 0.0;
            double riseFrac = 0.0;
            double yiFrac = 0.0;
        };

        const size_t minWin = (nRise >= 30) ? 5 : ((nRise >= 6) ? 3 : 2);
        const size_t maxWin = std::max(minWin, std::min<size_t>(nRise, 250));
        std::vector<ElasticCand> cands;

        auto collectElasticWindows = [&](double maxStartFrac, double maxEndFrac,
                                         double minRiseFrac) {
            for (size_t i = 0; i + minWin - 1 < nRise; ++i) {
                const double yiFrac = pts[i].y / res.utsY;
                if (yiFrac < -0.05 || yiFrac > maxStartFrac)
                    continue;
                const size_t jEnd = std::min(nRise - 1, i + maxWin - 1);
                for (size_t j = i + minWin - 1; j <= jEnd; ++j) {
                    const double yjFrac = pts[j].y / res.utsY;
                    if (yjFrac > maxEndFrac)
                        break;
                    const double riseFrac = (pts[j].y - pts[i].y) / res.utsY;
                    if (riseFrac < minRiseFrac)
                        continue;
                    const double m = double(j - i + 1);
                    const double mx = (pX[j + 1] - pX[i]) / m;
                    const double my = (pY[j + 1] - pY[i]) / m;
                    const double sxx = (pXX[j + 1] - pXX[i]) - m * mx * mx;
                    const double syy = (pYY[j + 1] - pYY[i]) - m * my * my;
                    const double sxy = (pXY[j + 1] - pXY[i]) - m * mx * my;
                    if (sxx <= 1e-30 || syy <= 1e-30 || sxy <= 0.0)
                        continue;
                    const double b = sxy / sxx;
                    const double a = my - b * mx;
                    const double rss = std::max(0.0, syy - b * sxy);
                    const double r2 = std::clamp(1.0 - rss / syy, 0.0, 1.0);
                    cands.push_back({ i, j, b, a, r2, riseFrac, std::max(0.0, yiFrac) });
                }
            }
        };

        collectElasticWindows(0.28, 0.72, 0.18);
        if (cands.empty())
            collectElasticWindows(0.40, 0.90, 0.10);
        if (cands.empty()) {
            // Ultra-coarse fallback: first 2..3 points on rising branch
            const size_t jFb = std::min<size_t>(nRise - 1, 2);
            std::vector<TensilePt> sub(pts.begin(), pts.begin() + jFb + 1);
            double b = 0.0, a = 0.0, r2 = 0.0;
            if (olsRange(sub, b, a, r2)) {
                res.elasticSlopeRaw = b;
                res.elasticIntercept = a;
                res.elasticR2 = r2;
                res.elasticXMin = sub.front().x;
                res.elasticXMax = sub.back().x;
                hasElasticFit = true;
            }
        } else {
            double bestR2 = 0.0;
            for (const auto &c : cands)
                bestR2 = std::max(bestR2, c.r2);
            const double r2Gate = (bestR2 >= 0.998) ? (bestR2 - 0.0025)
                    : (bestR2 >= 0.98)              ? (bestR2 - 0.008)
                                                    : (bestR2 * 0.92);
            double maxSlopeHighR2 = 0.0;
            for (const auto &c : cands) {
                if (c.r2 >= r2Gate)
                    maxSlopeHighR2 = std::max(maxSlopeHighR2, c.b);
            }

            const ElasticCand *winner = &cands.front();
            double bestScore = -1.0;
            for (const auto &c : cands) {
                if (c.r2 < r2Gate)
                    continue;
                if (maxSlopeHighR2 > 0.0 && c.b < 0.75 * maxSlopeHighR2)
                    continue;
                const double slopeTerm =
                        (maxSlopeHighR2 > 0.0) ? std::pow(c.b / maxSlopeHighR2, 1.5) : 1.0;
                const double spanTerm = std::pow(std::min(0.65, c.riseFrac), 0.35);
                const double originTerm = 1.0 / (1.0 + 1.5 * c.yiFrac);
                const double score = std::pow(c.r2, 12.0) * slopeTerm * spanTerm * originTerm;
                if (score > bestScore) {
                    bestScore = score;
                    winner = &c;
                }
            }
            res.elasticSlopeRaw = winner->b;
            res.elasticIntercept = winner->a;
            res.elasticR2 = winner->r2;
            res.elasticXMin = pts[winner->i].x;
            res.elasticXMax = pts[winner->j].x;
            hasElasticFit = true;
        }
    }

    if (!hasElasticFit || !(res.elasticSlopeRaw > 0.0)) {
        res.error = QObject::tr(
                "Could not determine a positive linear-elastic slope in the specified range.");
        return res;
    }

    res.youngsModulus = strainInPercent ? (res.elasticSlopeRaw * 100.0) : res.elasticSlopeRaw;

    // Intersect shifted offset line L(x) = elasticSlopeRaw * (x - (x0 + deltaX)) with curve segments
    const double xSpanRise = std::max(1e-12, pts[utsIdx].x - minX);
    double x0 = -res.elasticIntercept / res.elasticSlopeRaw;
    x0 = std::clamp(x0, minX - 0.05 * xSpanRise, minX + 0.25 * xSpanRise);
    if (std::fabs(x0) < 1e-6 * xSpanRise)
        x0 = 0.0;

    const double deltaX = strainInPercent ? (res.offsetFraction * 100.0) : res.offsetFraction;
    const double xFoot = x0 + deltaX;

    bool seenAboveOffsetLine = false;
    bool foundYield = false;
    for (size_t k = 0; k < fracIdx && k + 1 < pts.size(); ++k) {
        // Never intersect the offset line with a discontinuous or post-UTS brittle fracture drop
        if (pts[k].y > 0.0 && pts[k + 1].y < 0.5 * pts[k].y)
            break;
        if (k >= utsIdx && pts[k + 1].y < 0.75 * res.utsY
            && (pts[k + 1].x - pts[k].x) < 0.25 * deltaX)
            break;
        const double l0 = res.elasticSlopeRaw * (pts[k].x - xFoot);
        const double l1 = res.elasticSlopeRaw * (pts[k + 1].x - xFoot);
        const double d0 = pts[k].y - l0;
        const double d1 = pts[k + 1].y - l1;
        if (pts[k].y > 0.05 * res.utsY && d0 > 0.0)
            seenAboveOffsetLine = true;
        if ((seenAboveOffsetLine || d0 >= 0.0) && d0 >= 0.0 && d1 <= 0.0
            && pts[k + 1].x >= xFoot) {
            const double denom = d0 - d1;
            const double t = (std::fabs(denom) > 1e-30) ? std::clamp(d0 / denom, 0.0, 1.0) : 0.0;
            res.offsetYieldX = pts[k].x + t * (pts[k + 1].x - pts[k].x);
            res.offsetYieldY = pts[k].y + t * (pts[k + 1].y - pts[k].y);
            if (res.offsetYieldY > 0.02 * res.utsY)
                foundYield = true;
            break;
        }
    }

    if (!foundYield) {
        res.valid = false;
        res.offsetYieldX = std::numeric_limits<double>::quiet_NaN();
        res.offsetYieldY = std::numeric_limits<double>::quiet_NaN();
        res.error = QObject::tr("Curve fractured before reaching the %1% offset yield line.")
                            .arg(res.offsetFraction * 100.0, 0, 'g', 4);
        return res;
    }

    res.valid = true;

    // Optional True Stress - True Strain worksheet (strictly up to uniform elongation utsIdx, prior to necking)
    if (createTrueStressTable && app) {
        std::vector<double> trueEps;
        std::vector<double> trueSig;
        std::vector<double> plasticEps;
        const size_t nUniform = std::min(pts.size(), utsIdx + 1);
        trueEps.reserve(nUniform);
        trueSig.reserve(nUniform);
        plasticEps.reserve(nUniform);
        for (size_t i = 0; i <= utsIdx && i < pts.size(); ++i) {
            const auto &p = pts[i];
            const double eFrac = strainInPercent ? (p.x / 100.0) : p.x;
            if (1.0 + eFrac <= 1e-9)
                continue;
            const double epsT = std::log(1.0 + eFrac);
            const double sigT = p.y * (1.0 + eFrac);
            const double epsP = std::max(0.0, epsT - (sigT / std::max(1e-12, res.youngsModulus)));
            trueEps.push_back(epsT);
            trueSig.push_back(sigT);
            plasticEps.push_back(epsP);
        }
        if (!trueEps.empty()) {
            Table *tTrue = createNumericTable(
                    app, QStringLiteral("TrueStressStrain"),
                    QStringList() << QStringLiteral("TrueStrain") << QStringLiteral("TrueStress")
                                  << QStringLiteral("TruePlasticStrain"),
                    QList<SciDAVis::PlotDesignation>()
                            << SciDAVis::X << SciDAVis::Y << SciDAVis::noDesignation,
                    { trueEps, trueSig, plasticEps });
            if (tTrue) {
                if (Column *c0 = tTrue->column(0)) {
                    c0->setLongName(QStringLiteral("True strain, \u03B5"));
                    c0->setUnits(QStringLiteral("-"));
                }
                if (Column *c1 = tTrue->column(1)) {
                    c1->setLongName(QStringLiteral("True stress, \u03C3"));
                    c1->setUnits(stressUnit);
                }
                if (Column *c2 = tTrue->column(2)) {
                    c2->setLongName(QStringLiteral("True plastic strain, \u03B5<sub>p</sub>"));
                    c2->setUnits(QStringLiteral("-"));
                }
                res.trueStressTable = tTrue;
            }
        }
    }

    const QString beforeSnap =
            (res.plot && (plotConstruction || addCallout)) ? res.plot->captureUndoSnapshot()
                                                           : QString();

    const QString offsetPctStr = QString::number(res.offsetFraction * 100.0, 'g', 3);

    // Check if existing graph labels are pure ASCII (avoid mixing Japanese legend onto all-English ISIJ plots)
    bool graphHasCjk = false;
    if (targetGraph->plotWidget()) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            const QString at = targetGraph->plotWidget()->axisTitle(a).text();
            for (QChar ch : at) {
                if (ch.unicode() >= 0x3000) {
                    graphHasCjk = true;
                    break;
                }
            }
        }
    }

    if (plotConstruction && app) {
        auto *colOX = new Column(QStringLiteral("OffsetStrain"), SciDAVis::ColumnMode::Numeric);
        auto *colOY = new Column(QStringLiteral("OffsetStress"), SciDAVis::ColumnMode::Numeric);
        colOX->setPlotDesignation(SciDAVis::X);
        colOY->setPlotDesignation(SciDAVis::Y);
        QVector<qreal> vx = { xFoot, res.offsetYieldX };
        QVector<qreal> vy = { 0.0, res.offsetYieldY };
        colOX->replaceValues(0, vx);
        colOY->replaceValues(0, vy);
        const QString legComment = graphHasCjk
                ? QStringLiteral("%1% \u30AA\u30D5\u30BB\u30C3\u30C8\u7DDA").arg(offsetPctStr)
                : QStringLiteral("%1% offset").arg(offsetPctStr);
        colOY->setComment(legComment);

        const QString hidName = app->generateUniqueName(QStringLiteral("GadgetFit_OffsetYield"));
        Table *hidTbl = app->newHiddenTable(hidName, legComment, QList<Column *>() << colOX << colOY);
        if (hidTbl) {
            auto *cOff = new DataCurve(hidTbl, hidName + QStringLiteral("_") + colOX->name(),
                                       hidName + QStringLiteral("_") + colOY->name(), 0, 1);
            cOff->setType(Graph::Line);
            cOff->setStyle(QwtPlotCurve::Lines);
            cOff->setPen(QPen(QColor(220, 38, 38), 1.8, Qt::DashLine));
            cOff->loadData();
            targetGraph->insertPlotItem(cOff, Graph::Line);
            cOff->setAxis(srcCurve->xAxis(), srcCurve->yAxis());
            targetGraph->addFitCurve(cOff);
            res.constructionCurveTitle = cOff->title().text();
        }

        Graph::ReferenceLineSpec refSpec;
        refSpec.orientation = 0; // Horizontal Y
        refSpec.value = res.offsetYieldY;
        refSpec.color = QColor(220, 38, 38, 195);
        refSpec.style = Qt::DotLine;
        refSpec.width = 1.3;
        refSpec.label = addCallout
                ? QString()
                : QStringLiteral("\u03C3<sub>%1</sub> = %2 %3")
                          .arg(offsetPctStr)
                          .arg(res.offsetYieldY, 0, 'f', 1)
                          .arg(stressUnit);
        targetGraph->addReferenceLine(refSpec);
    }

    if (addCallout) {
        QString eDisplay;
        if (stressUnit == QLatin1String("MPa") && res.youngsModulus >= 1000.0) {
            eDisplay = QStringLiteral("%1 GPa").arg(res.youngsModulus / 1000.0, 0, 'f', 1);
        } else {
            eDisplay = QStringLiteral("%1 %2").arg(res.youngsModulus, 0, 'g', 4).arg(stressUnit);
        }
        const QString callout = QStringLiteral(
                "\u03C3<sub>%1</sub> = %2 %3\n"
                "<i>E</i> = %4 (<i>R</i><sup>2</sup> = %5)\n"
                "UTS = %6 %3")
                .arg(offsetPctStr)
                .arg(res.offsetYieldY, 0, 'f', 1)
                .arg(stressUnit, eDisplay)
                .arg(res.elasticR2, 0, 'f', 4)
                .arg(res.utsY, 0, 'f', 1);

        const double xSpanAll = std::max(1e-9, maxX - minX);
        const double calloutX = std::min(minX + 0.52 * xSpanAll,
                                         std::max(res.offsetYieldX + 0.06 * xSpanAll,
                                                  minX + 0.28 * xSpanAll));
        const double calloutY = 0.42 * res.utsY;
        addMatSciCalloutBox(targetGraph, calloutX, calloutY, callout);
    }

    targetGraph->replot();
    if (res.plot && !beforeSnap.isEmpty()) {
        res.plot->pushUndoSnapshot(QObject::tr("Tensile / 0.2% Proof Stress Analyzer"),
                                   beforeSnap);
    }
    if (app) {
        app->updateLog(QStringLiteral(
                "[%1] Tensile Analysis (%2): E = %3 %4 (R² = %5), σ_%6 = %7 %4 at %8%9, UTS = %10 %4\n")
                .arg(QLocale().toString(QDateTime::currentDateTime()),
                     srcCurve->title().text())
                .arg(res.youngsModulus, 0, 'g', 5)
                .arg(stressUnit)
                .arg(res.elasticR2, 0, 'f', 5)
                .arg(offsetPctStr)
                .arg(res.offsetYieldY, 0, 'f', 2)
                .arg(res.offsetYieldX, 0, 'g', 4)
                .arg(strainInPercent ? QStringLiteral("%") : QString())
                .arg(res.utsY, 0, 'f', 2));
        app->modifiedProject();
    }

    return res;
}

void CurveMathTools::showTensileYieldDialog(ApplicationWindow *app)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    registerMatSciTranslationsOnce();

    QList<Graph *> graphs;
    Graph *activeG = findActiveOrFirstGraph(app, nullptr);
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer")) {
            MultiLayer *ml = static_cast<MultiLayer *>(w);
            if (Graph *g = ml->activeGraph()) {
                if (!g->analysableCurvesList().isEmpty())
                    graphs.append(g);
            }
        }
    }

    const QList<Table *> allTables = collectProjectTables(app);
    QList<Table *> tables;
    for (Table *t : allTables) {
        int numCols = 0;
        for (int c = 0; c < t->numCols(); ++c) {
            if (matSciColumnHasNumeric(t->column(c)))
                ++numCols;
        }
        if (numCols >= 2)
            tables.append(t);
    }

    if (graphs.isEmpty() && tables.isEmpty()) {
        QMessageBox::information(
                app, QObject::tr("Tensile / 0.2% Proof Stress Analyzer"),
                QObject::tr("Please open a graph or numeric worksheet with stress-strain data first."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Tensile / 0.2% Proof Stress Analyzer"));
    dlg.setMinimumWidth(420);
    QFormLayout *form = new QFormLayout();

    QComboBox *cbSourceMode = new QComboBox(&dlg);
    if (!graphs.isEmpty())
        cbSourceMode->addItem(QObject::tr("Active Graph Curve"), 0);
    if (!tables.isEmpty())
        cbSourceMode->addItem(QObject::tr("Worksheet Columns (Plot & Analyze)"), 1);

    // Prefer Graph mode if active window is a MultiLayer, else Table mode if active window is a Table
    if (cbSourceMode->count() == 2) {
        if (QMdiSubWindow *sub = app->d_workspace.activeSubWindow()) {
            if (sub->inherits("Table") || (sub->widget() && sub->widget()->inherits("Table")))
                cbSourceMode->setCurrentIndex(1);
        }
    }

    QComboBox *cbGraph = new QComboBox(&dlg);
    QComboBox *cbCurve = new QComboBox(&dlg);
    int initGraphIdx = 0;
    for (int i = 0; i < graphs.size(); ++i) {
        MultiLayer *ml = graphs[i]->multiLayerParent();
        cbGraph->addItem(ml ? ml->name() : graphs[i]->parentPlotName());
        if (graphs[i] == activeG)
            initGraphIdx = i;
    }
    if (!graphs.isEmpty())
        cbGraph->setCurrentIndex(initGraphIdx);

    auto repopulateCurves = [&](int gIdx) {
        cbCurve->clear();
        if (gIdx < 0 || gIdx >= graphs.size())
            return;
        cbCurve->addItems(graphs[gIdx]->analysableCurvesList());
    };
    repopulateCurves(initGraphIdx);
    QObject::connect(cbGraph, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     repopulateCurves);

    QComboBox *cbTable = new QComboBox(&dlg);
    QComboBox *cbStrainCol = new QComboBox(&dlg);
    QComboBox *cbStressCol = new QComboBox(&dlg);
    Table *activeTbl = findActiveOrFirstTable(app);
    int initTblIdx = 0;
    for (int i = 0; i < tables.size(); ++i) {
        cbTable->addItem(tables[i]->name());
        if (tables[i] == activeTbl)
            initTblIdx = i;
    }
    if (!tables.isEmpty())
        cbTable->setCurrentIndex(initTblIdx);

    auto repopulateTableCols = [&](int tIdx) {
        cbStrainCol->clear();
        cbStressCol->clear();
        if (tIdx < 0 || tIdx >= tables.size())
            return;
        Table *t = tables[tIdx];
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!matSciColumnHasNumeric(col))
                continue;
            const QString cn = col ? col->name() : t->colLabel(c);
            cbStrainCol->addItem(cn);
            cbStressCol->addItem(cn);
        }
        if (cbStressCol->count() >= 2)
            cbStressCol->setCurrentIndex(1);
    };
    repopulateTableCols(initTblIdx);
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     repopulateTableCols);

    QLabel *lblGraph = new QLabel(QObject::tr("Target Graph:"), &dlg);
    QLabel *lblCurve = new QLabel(QObject::tr("Stress-Strain Curve:"), &dlg);
    QLabel *lblTable = new QLabel(QObject::tr("Source Worksheet:"), &dlg);
    QLabel *lblStrainCol = new QLabel(QObject::tr("Strain Column (X):"), &dlg);
    QLabel *lblStressCol = new QLabel(QObject::tr("Stress Column (Y):"), &dlg);

    auto syncSourceVisibility = [&]() {
        const bool fromTable = (cbSourceMode->currentData().toInt() == 1);
        lblGraph->setVisible(!fromTable);
        cbGraph->setVisible(!fromTable);
        lblCurve->setVisible(!fromTable);
        cbCurve->setVisible(!fromTable);
        lblTable->setVisible(fromTable);
        cbTable->setVisible(fromTable);
        lblStrainCol->setVisible(fromTable);
        cbStrainCol->setVisible(fromTable);
        lblStressCol->setVisible(fromTable);
        cbStressCol->setVisible(fromTable);
    };
    QObject::connect(cbSourceMode, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { syncSourceVisibility(); });

    QDoubleSpinBox *sbOffsetPct = new QDoubleSpinBox(&dlg);
    sbOffsetPct->setRange(0.01, 20.0);
    sbOffsetPct->setDecimals(3);
    sbOffsetPct->setSingleStep(0.05);
    sbOffsetPct->setValue(0.200);
    sbOffsetPct->setSuffix(QStringLiteral(" %"));

    QComboBox *cbStrainUnit = new QComboBox(&dlg);
    cbStrainUnit->addItem(QObject::tr("Auto-detect (% or mm/mm)"), -1);
    cbStrainUnit->addItem(QObject::tr("Percent (%)"), 1);
    cbStrainUnit->addItem(QObject::tr("Engineering Fraction (mm/mm)"), 0);

    QCheckBox *chkAutoElastic =
            new QCheckBox(QObject::tr("Auto-detect linear-elastic region (max R²)"), &dlg);
    chkAutoElastic->setChecked(true);

    QDoubleSpinBox *sbElasticMin = new QDoubleSpinBox(&dlg);
    sbElasticMin->setRange(-1000.0, 10000.0);
    sbElasticMin->setDecimals(5);
    sbElasticMin->setValue(0.02);
    sbElasticMin->setEnabled(false);

    QDoubleSpinBox *sbElasticMax = new QDoubleSpinBox(&dlg);
    sbElasticMax->setRange(-1000.0, 10000.0);
    sbElasticMax->setDecimals(5);
    sbElasticMax->setValue(0.30);
    sbElasticMax->setEnabled(false);

    QObject::connect(chkAutoElastic, &QCheckBox::toggled, &dlg, [&](bool autoOn) {
        sbElasticMin->setEnabled(!autoOn);
        sbElasticMax->setEnabled(!autoOn);
    });

    QCheckBox *chkPlotConstruct = new QCheckBox(
            QObject::tr("Plot 0.2% offset construction line & yield marker"), &dlg);
    chkPlotConstruct->setChecked(true);

    QCheckBox *chkCallout =
            new QCheckBox(QObject::tr("Add summary callout (σ0.2, E, UTS) on graph"), &dlg);
    chkCallout->setChecked(true);

    QCheckBox *chkTrueTable =
            new QCheckBox(QObject::tr("Generate True Stress - True Strain worksheet"), &dlg);
    chkTrueTable->setChecked(false);

    if (cbSourceMode->count() > 1)
        form->addRow(QObject::tr("Data Source:"), cbSourceMode);
    form->addRow(lblGraph, cbGraph);
    form->addRow(lblCurve, cbCurve);
    form->addRow(lblTable, cbTable);
    form->addRow(lblStrainCol, cbStrainCol);
    form->addRow(lblStressCol, cbStressCol);
    form->addRow(QObject::tr("Offset Proof Strain (%):"), sbOffsetPct);
    form->addRow(QObject::tr("Strain Unit:"), cbStrainUnit);
    form->addRow(QString(), chkAutoElastic);
    form->addRow(QObject::tr("Elastic Strain Min:"), sbElasticMin);
    form->addRow(QObject::tr("Elastic Strain Max:"), sbElasticMax);
    form->addRow(QString(), chkPlotConstruct);
    form->addRow(QString(), chkCallout);
    form->addRow(QString(), chkTrueTable);
    syncSourceVisibility();

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    Graph *chosenGraph = nullptr;
    QString chosenCurve;
    if (cbSourceMode->currentData().toInt() == 1) {
        const int tIdx = cbTable->currentIndex();
        if (tIdx < 0 || tIdx >= tables.size())
            return;
        Table *t = tables[tIdx];
        const QString xName = cbStrainCol->currentText();
        const QString yName = cbStressCol->currentText();
        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("TensilePlot")));
        if (!ml || !ml->activeGraph())
            return;
        chosenGraph = ml->activeGraph();
        const QString fullX = t->name() + QStringLiteral("_") + xName;
        const QString fullY = t->name() + QStringLiteral("_") + yName;
        if (chosenGraph->insertCurve(t, fullX, fullY, Graph::Line)) {
            chosenGraph->setCurvePen(chosenGraph->curves() - 1,
                                     QPen(QColor(29, 78, 216), 2.0, Qt::SolidLine));
            chosenCurve = fullY;
        }
        chosenGraph->refreshAutoAxisTitlesFromColumns();
        chosenGraph->setTitle(QString());
        chosenGraph->setAutoScale();
        applyArialToMatSciGraph(chosenGraph);
        chosenGraph->replot();
    } else {
        const int gIdx = cbGraph->currentIndex();
        if (gIdx < 0 || gIdx >= graphs.size())
            return;
        chosenGraph = graphs[gIdx];
        chosenCurve = cbCurve->currentText();
    }

    const double eMin = chkAutoElastic->isChecked()
            ? std::numeric_limits<double>::quiet_NaN()
            : sbElasticMin->value();
    const double eMax = chkAutoElastic->isChecked()
            ? std::numeric_limits<double>::quiet_NaN()
            : sbElasticMax->value();

    const TensileResult trRes = analyzeTensile(
            app, chosenGraph, chosenCurve, sbOffsetPct->value() / 100.0,
            cbStrainUnit->currentData().toInt(), eMin, eMax,
            chkPlotConstruct->isChecked(), chkCallout->isChecked(),
            chkTrueTable->isChecked());
    if (!trRes.valid && !ApplicationWindow::isNonInteractiveContext(app)) {
        QMessageBox::warning(app, QObject::tr("Tensile / 0.2% Proof Stress Analyzer"),
                             trRes.error.isEmpty()
                                     ? QObject::tr("Could not determine a positive linear-elastic slope in the specified range.")
                                     : trRes.error);
    }
}
