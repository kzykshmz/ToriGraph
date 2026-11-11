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
#include "Graph.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "Table.h"
#include "core/column/Column.h"

#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>
#include <qwt_symbol.h>
#include <qwt_text.h>

#include <QCheckBox>
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
        const QString title = c->title().text();
        if (title == curveTitle || title.endsWith("_" + curveTitle))
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
    if (!app || !g)
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
    if (!app || !g)
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
    if (!app || !g)
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
    if (!app || !g)
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
    if (!app || !g)
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
