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

#include <gsl/gsl_cdf.h>

#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>
#include <qwt_scale_map.h>
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
    g->initFonts(QFont(QStringLiteral("Arial"), 11, QFont::Bold),
                 QFont(QStringLiteral("Arial"), 10));
    g->setTitleFont(QFont(QStringLiteral("Arial"), 12, QFont::Bold));
    if (g->legend())
        g->legend()->setFont(QFont(QStringLiteral("Arial"), 9));
    g->setTextMarkerDefaults(g->textMarkerDefaultFrame(),
                             QFont(QStringLiteral("Arial"), 10),
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
    box->setFrameStyle(Legend::Line);
    box->setBackgroundColor(QColor(255, 255, 255, 232));
    box->setTextColor(QColor(20, 30, 50));
    box->setFont(QFont(QStringLiteral("Arial"), 9));
    box->setOriginCoord(x, y);
    return box;
}

class FilledPolygonDataCurve : public DataCurve
{
public:
    FilledPolygonDataCurve(Table *t, const QString &xColName, const QString &yColName,
                           int startRow, int endRow, const QColor &fillColor)
        : DataCurve(t, xColName, yColName, startRow, endRow)
    {
        setType(Graph::Line);
        setStyle(QwtPlotCurve::Lines);
        QColor borderCol = fillColor;
        borderCol.setAlpha(std::clamp(fillColor.alpha() + 90, 90, 220));
        setPen(QPen(borderCol, 1.0, Qt::SolidLine));
        setBrush(QBrush(fillColor));
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    using DataCurve::draw;
    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              int from, int to) const override
    {
        if (!painter || dataSize() < 3)
            return;
        if (to < 0)
            to = dataSize() - 1;
        from = std::max(0, from);
        to = std::min(dataSize() - 1, to);
        if (to - from + 1 < 3)
            return;

        QPolygonF poly;
        poly.reserve(to - from + 1);
        for (int i = from; i <= to; ++i) {
            const double xv = x(i);
            const double yv = y(i);
            if (std::isfinite(xv) && std::isfinite(yv))
                poly.append(QPointF(xMap.transform(xv), yMap.transform(yv)));
        }
        if (poly.size() < 3)
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setBrush(brush());
        painter->setPen(pen());
        painter->drawPolygon(poly, Qt::OddEvenFill);
        painter->restore();
    }
};

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
        if (cTemp->isInvalid(r) || cRate->isInvalid(r)
            || cTemp->isMasked(r) || cRate->isMasked(r))
            continue;
        const double rawT = cTemp->valueAt(r);
        const double rawK = cRate->valueAt(r);
        if (!std::isfinite(rawT) || !std::isfinite(rawK))
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

    MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("ArrheniusPlot")));
    res.plot = ml;
    if (ml) {
        if (Graph *g = ml->activeGraph()) {
            if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(2), Graph::Scatter, 0, n - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(25, 65, 135), 1.5));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(35, 95, 185)),
                                            QPen(QColor(15, 45, 95), 1.2), QSize(8, 8)));
            }
            insertStyledCurve(g, tbl, 0, 3, n, QPen(QColor(205, 35, 45), 2.2, Qt::SolidLine));
            if (n > 2 && sResid > 0.0) {
                const QPen ciPen(QColor(205, 35, 45, 175), 1.3, Qt::DashLine);
                insertStyledCurve(g, tbl, 0, 4, n, ciPen);
                insertStyledCurve(g, tbl, 0, 5, n, ciPen);
            }

            g->setXAxisTitle(use1000OverT ? QObject::tr("1000 / T (K^-1)")
                                          : QObject::tr("1 / T (K^-1)"));
            g->setYAxisTitle(sigmaTimesT ? QObject::tr("ln(k * T)") : QObject::tr("ln(k)"));
            g->setTitle(QObject::tr("Arrhenius Plot (Activation Energy)"));
            if (g->legend())
                g->legend()->setText(g->legendText());
            applyArialToMatSciGraph(g);

            const double xSpan = std::max(1e-9, colInvT.back() - colInvT.front());
            const auto yMM = std::minmax_element(colLnK.begin(), colLnK.end());
            const double ySpan = std::max(1e-9, *yMM.second - *yMM.first);
            const QString summary = QStringLiteral(
                    "Ea = %1 ± %2 kJ/mol\n"
                    "Ea = %3 ± %4 eV\n"
                    "ln(A) = %5\n"
                    "R² = %6")
                    .arg(res.activationEnergyKJmol, 0, 'f', 3)
                    .arg(res.activationEnergyKJmolSE, 0, 'f', 3)
                    .arg(res.activationEnergyEV, 0, 'f', 4)
                    .arg(res.activationEnergyEVSE, 0, 'f', 4)
                    .arg(res.lnPreExponential, 0, 'f', 3)
                    .arg(res.rSquare, 0, 'f', 5);
            addMatSciCalloutBox(g, colInvT.front() + 0.06 * xSpan,
                                *yMM.first + 0.32 * ySpan, summary);
            g->setAutoScale();
            g->replot();
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
        if (cX->isInvalid(r) || cAlpha->isInvalid(r)
            || cX->isMasked(r) || cAlpha->isMasked(r))
            continue;
        const double xv = cX->valueAt(r);
        const double av = cAlpha->valueAt(r);
        if (!std::isfinite(xv) || !std::isfinite(av) || xv <= 0.0 || av < 0.0)
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

            g->setXAxisTitle(QObject::tr("Photon Energy hν (eV)"));
            g->setYAxisTitle(directAllowed ? QObject::tr("(αhν)^2 ((eV/cm)^2)")
                                           : QObject::tr("(αhν)^(1/2) ((eV/cm)^(1/2))"));
            g->setTitle(QObject::tr("Tauc Plot (Optical Bandgap)"));
            if (g->legend())
                g->legend()->setText(g->legendText());
            applyArialToMatSciGraph(g);

            const double eSpan = std::max(1e-6, xHi - xLo);
            const QString summary = QStringLiteral(
                    "Eg = %1 ± %2 eV (%3)\n"
                    "Edge [%4 – %5 eV]\n"
                    "R² = %6")
                    .arg(res.bandgapEV, 0, 'f', 3)
                    .arg(res.bandgapEVSE, 0, 'f', 3)
                    .arg(directAllowed ? QStringLiteral("Direct") : QStringLiteral("Indirect"))
                    .arg(E[static_cast<size_t>(winner->i)], 0, 'f', 2)
                    .arg(E[static_cast<size_t>(winner->j)], 0, 'f', 2)
                    .arg(res.rSquare, 0, 'f', 5);
            addMatSciCalloutBox(g, xLo + 0.06 * eSpan, 0.72 * yMax, summary);
            g->setAutoScale();
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

    std::vector<double> xA, yA, xB, yB;
    if (!extractUnmaskedCurvePoints(g, curveA, xA, yA, true, true)
        || !extractUnmaskedCurvePoints(g, curveB, xB, yB, true, true))
        return nullptr;
    if (xA.size() < 2 || xB.size() < 2)
        return nullptr;

    const double ovLo = std::max(xA.front(), xB.front());
    const double ovHi = std::min(xA.back(), xB.back());
    const double xStart = (ovHi > ovLo) ? ovLo : std::min(xA.front(), xB.front());
    const double xEnd = (ovHi > ovLo) ? ovHi : std::max(xA.back(), xB.back());
    if (!(xEnd > xStart))
        return nullptr;

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

    const size_t m = xg.size();
    std::vector<double> polyX;
    std::vector<double> polyY;
    polyX.reserve(2 * m + 1);
    polyY.reserve(2 * m + 1);
    for (size_t k = 0; k < m; ++k) {
        polyX.push_back(xg[k]);
        polyY.push_back(yAg[k]);
    }
    for (size_t k = m; k-- > 0;) {
        polyX.push_back(xg[k]);
        polyY.push_back(yBg[k]);
    }
    polyX.push_back(xg[0]);
    polyY.push_back(yAg[0]);

    Table *polyTbl = createNumericTable(
            app, QStringLiteral("FillBetween"),
            QStringList() << QStringLiteral("FillX") << QStringLiteral("FillY"),
            QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
            { polyX, polyY });
    if (!polyTbl)
        return nullptr;

    const int nPoly = static_cast<int>(polyX.size());
    auto *fillCurve = new FilledPolygonDataCurve(
            polyTbl, polyTbl->colName(0), polyTbl->colName(1), 0, nPoly - 1, fillColor);
    fillCurve->loadData();
    g->insertPlotItem(fillCurve, Graph::Line);
    g->updatePlot();
    return fillCurve;
}

void CurveMathTools::showArrheniusDialog(ApplicationWindow *app)
{
    if (!app)
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
            const QString cn = t->column(c) ? t->column(c)->name() : t->colLabel(c);
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
    if (!app)
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
            const QString cn = t->column(c) ? t->column(c)->name() : t->colLabel(c);
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
    if (!app)
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
