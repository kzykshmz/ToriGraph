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
#include "GraphStatsAndSurfaceFit.h"
#include "I18nJapanese.h"
#include "Legend.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "ScaleDraw.h"
#include "Spectrogram.h"
#include "Table.h"
#include "core/column/Column.h"
#include "core/datatypes/Double2StringFilter.h"
#include "table/AsciiTableImportFilter.h"
#include "table/future_Table.h"

#include <gsl/gsl_cdf.h>

#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
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
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QPolygonF>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
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

    // Campaign 2 Materials Science Workbench (Sample Gallery, Kissinger, Kocks-Mecking, XRD, VASP)
    reg("Materials Science Sample Gallery", "材料科学サンプルデータギャラリー");
    reg("Try Sample Datasets...", "サンプルデータで試す...");
    reg("Select a built-in materials science dataset to generate a ready-to-analyze worksheet and publication plot:",
        "内蔵された材料科学サンプルデータまたは複合サンプルプロジェクトを選択し、ワークシートと論文品質グラフを自動生成します:");
    reg("Category:", "カテゴリ:");
    reg("All Categories", "すべてのカテゴリ");
    reg("Filter:", "検索:");
    reg("Search by alloy, test type, or keyword...", "合金名・試験法・キーワードで絞り込み...");
    reg("Generate Worksheet & Plot", "ワークシートとグラフを生成");
    reg("Generate & Run Recommended Analysis", "生成して推奨解析まで実行");
    reg("Sample Project Pack", "サンプルプロジェクト集");
    reg("Metallurgy / Mechanical", "金属・力学特性");
    reg("Hydrogen Embrittlement / TDS", "水素脆化・昇温脱離 (TDS)");
    reg("X-Ray Diffraction (XRD)", "X線回折 (XRD)");
    reg("Diffusion / Kinetics", "拡散・速度論");
    reg("Synchrotron CT / Local Strain", "放射光CT・局所ひずみ場");
    reg("Ceramics / Fracture Statistics", "セラミックス・破壊統計");
    reg("AA7075-T6 Tensile Stress-Strain Curve", "AA7075-T6 引張応力-ひずみ曲線");
    reg("Al-Zn-Mg Alloy Tensile Comparison by Temper (T4 / T6 / T73)",
        "Al-Zn-Mg 合金の時効処理別 引張応力-ひずみ曲線比較 (T4 / T6 / T73)");
    reg("Hydrogen Embrittlement Tensile Curves (Uncharged vs. H-Charged)",
        "高強度 Al-Zn-Mg 合金の水素脆化引張試験 (未チャージ vs. 水素チャージ)");
    reg("Fatigue S-N Curves: SCM435 Steel vs. AA7075-T6 (Wöhler Diagram)",
        "疲労 S-N 曲線比較: SCM435 鋼 vs. AA7075-T6 合金 (Wöhler 線図)");
    reg("Fatigue Crack Growth Rate da/dN vs. ΔK (Paris Law: Air vs. H-Charged)",
        "疲労き裂進展速度曲線 da/dN–ΔK (Paris 則: 大気中 vs. 水素環境)");
    reg("Hall-Petch Grain Size Strengthening (Ferritic Steel & Al-Mg Alloy)",
        "結晶粒径と降伏応力の Hall-Petch 関係 (フェライト鋼・Al-Mg 合金)");
    reg("High-Temperature Creep Curves of 9Cr-1Mo Steel (600 °C, 3 Stress Levels)",
        "9Cr-1Mo 耐熱鋼の高温クリープ曲線 (600 °C, 120 / 140 / 160 MPa)");
    reg("Charpy Impact Toughness Transition Curve (DBTT of Ferritic Steel)",
        "フェライト鋼のシャルピー衝撃吸収エネルギー遷移曲線 (DBTT)");
    reg("Isothermal Age-Hardening Curves of Al-Zn-Mg Alloy (Vickers HV, 120 vs. 160 °C)",
        "Al-Zn-Mg 合金の等温時効硬化曲線 (ビッカース硬さ HV, 120 vs. 160 °C)");
    reg("Nanoindentation Load-Depth Curves (Oliver-Pharr: Pure Al vs. AA7075-T6)",
        "ナノインデンテーション荷重-押込み深さ曲線 (Oliver-Pharr: 純 Al vs. AA7075-T6)");
    reg("Low-Cycle Fatigue Hysteresis Loops of SUS316L (Cycle 1 vs. Stabilized N=50)",
        "SUS316L 低サイクル疲労 応力-ひずみヒステリシスループ (N=1 vs. N=50)");
    reg("Fracture Toughness J-R Resistance Curve (ASTM E1820 with Blunting Lines)",
        "構造用鋼の破壊靭性 J-R 抵抗曲線 (ASTM E1820 鈍化線・0.2 mm オフセット線付)");
    reg("Strain-Rate Dependent Flow Stress of Ti-6Al-4V (10^-3 to 10^3 s^-1)",
        "Ti-6Al-4V 合金のひずみ速度依存性 流動応力曲線 (準静的〜高速衝撃 SHPB)");
    reg("Project Pack: Metal Mechanical Properties Evaluation Suite (5 Datasets + Analyses)",
        "【複合プロジェクト】金属の力学特性 総合評価パッケージ (引張・加工硬化・疲労S-N・き裂進展・Hall-Petch・DBTT)");
    reg("Project Pack: Al-Zn-Mg Age-Hardening & Hydrogen Embrittlement Study (5 Datasets + Kissinger)",
        "【複合プロジェクト】Al-Zn-Mg 合金の時効硬化・水素脆化 総合研究パッケージ (時効HV・水素引張・TDS/Kissinger・XRD・CTひずみ)");
    reg("Hydrogen TDS Spectra at 3 Heating Rates (100, 200, 400 °C/h)",
        "鋼中水素の昇温脱離スペクトル (昇温速度 100, 200, 400 °C/h)");
    reg("Peak-Aged Al-Zn-Mg Alloy XRD Pattern (Cu Kα1)",
        "時効硬化 Al-Zn-Mg 合金のX線回折プロファイル (Cu Kα1)");
    reg("Hydrogen Interstitial Diffusivity in α-Fe", "α-Fe 中の格子間水素拡散係数");
    reg("3D-XRD / Synchrotron CT Crack-Tip Local Strain Map",
        "放射光CT / 3D-XRD き裂先端局所弾性ひずみマップ");
    reg("Sintered Si3N4 4-Point Flexural Strength (JIS R 1625)",
        "窒化ケイ素 (Si3N4) 4点曲げ強度ワイブル統計 (JIS R 1625)");

    // Sample dataset descriptions (JA)
    reg("Peak-aged Al-5.6Zn-2.5Mg-1.6Cu (T6) engineering stress-strain curve (E = 71.2 GPa, σ0.2 ≈ 508 MPa, UTS ≈ 578 MPa) for 0.2% offset yield & Kocks-Mecking Considère analysis.",
        "ピーク時効 Al-5.6Zn-2.5Mg-1.6Cu (T6) 合金の公称応力-公称ひずみ曲線 (E = 71.2 GPa, σ0.2 ≈ 508 MPa, UTS ≈ 578 MPa)。0.2%耐力解析および Kocks-Mecking / Considère 加工硬化解析に最適。");
    reg("Multi-curve tensile comparison of solution-treated/naturally aged (T4), peak-aged (T6), and over-aged (T73) Al-Zn-Mg alloy showing strength-ductility trade-off.",
        "自然時効 (T4)・ピーク時効 (T6)・過時効 (T73) の Al-Zn-Mg 合金3条件の公称応力-ひずみ曲線比較。時効に伴う耐力・加工硬化率・伸びのトレードオフを可視化。");
    reg("Uncharged vs. H-charged (0.8 and 2.2 wt.ppm H) high-strength Al-Zn-Mg tensile curves demonstrating identical elastic/yield behavior with severe hydrogen-induced ductility loss.",
        "未チャージ材と水素チャージ材 (0.8 / 2.2 wt.ppm H) の引張応力-ひずみ曲線比較。弾性率・0.2%耐力は一致しつつ、水素量増加に伴い早期破断する典型的な水素脆化挙動を再現。");
    reg("High-cycle fatigue S-N (Wöhler) curves comparing quenched-and-tempered SCM435 steel (distinct endurance limit ~420 MPa at 10^7 cycles) and AA7075-T6 aluminum alloy (no knee).",
        "調質 SCM435 鋼（10^7 回付近に約 420 MPa の明瞭な疲労限度あり）と AA7075-T6 アルミニウム合金（疲労限度なしで連続低下）の疲労 S-N 曲線比較（横軸対数）。");
    reg("Log-log fatigue crack propagation da/dN vs. ΔK showing threshold (Region I), Paris law power-law regime (Region II, m ≈ 3.1), and rapid fracture (Region III) in air and H-charged environments.",
        "両対数グラフによる疲労き裂進展速度 da/dN–ΔK 曲線。下限界 ΔKth (Region I)、Paris 則直線域 (Region II, m ≈ 3.1)、破壊靭性加速域 (Region III) を大気中と水素環境で対比。");
    reg("Yield stress σy vs. inverse square-root grain size d^(-1/2) with error bars and linear Hall-Petch regression for low-carbon ferritic steel (ky ≈ 0.62 MPa·m^1/2) and Al-Mg alloy.",
        "結晶粒径の逆平方根 d^(-1/2) に対する降伏応力 σy の Hall-Petch プロット（標準偏差エラーバー・線形回帰直線付き）。低炭素フェライト鋼と Al-Mg 合金の結晶粒微細化強化係数を比較。");
    reg("Three-stage creep strain vs. time curves (primary, steady-state secondary, tertiary to rupture) of 9Cr-1Mo heat-resistant steel at 600 °C under 120, 140, and 160 MPa.",
        "9Cr-1Mo 耐熱鋼の 600 °C における 120 / 140 / 160 MPa の3応力水準クリープ曲線。遷移クリープ（第1期）・定常クリープ（第2期）・加速クリープ（第3期）および破断寿命の応力依存性を再現。");
    reg("Charpy V-notch impact absorbed energy vE (J) vs. temperature (-120 to +80 °C) with error bars and ductile-to-brittle transition temperature (DBTT ≈ -25 °C) tanh-sigmoid curve.",
        "フェライト鋼の -120 °C ～ +80 °C におけるシャルピー衝撃吸収エネルギー vE (J) と標準偏差・延性破面率データ。下棚から上棚へ遷移する DBTT ≈ -25 °C のシグモイド遷移曲線を描画。");
    reg("Isothermal Vickers hardness (HV) vs. logarithmic aging time (0.1–1000 h) at 120 °C and 160 °C for Al-Zn-Mg alloy showing under-aged, peak-aged (T6), and over-aged (T7) stages.",
        "Al-Zn-Mg 合金の 120 °C および 160 °C 等温時効におけるビッカース硬さ HV（平均±SD）vs. 対数時効時間曲線。亜時効→ピーク時効 (T6)→過時効 (T7) の典型的な析出硬化挙動を再現。");
    reg("Berkovich nanoindentation loading (Kick's law P ∝ h^2), peak-load creep hold, and Oliver-Pharr unloading curves comparing soft pure Al and precipitation-hardened AA7075-T6.",
        "純 Al と時効強化 AA7075-T6 のナノインデンテーション荷重-押込み深さ曲線。負荷域 (Kick 則)・最大荷重保持クリープ・Oliver-Pharr 除荷曲線の弾性回復の違いを比較。");
    reg("Symmetric strain-controlled (±1.0%) cyclic stress-strain hysteresis loops of austenitic SUS316L stainless steel comparing Cycle 1 and stabilized Cycle 50 (cyclic hardening & Bauschinger effect).",
        "オーステナイト系ステンレス鋼 SUS316L の ±1.0% 対称ひずみ制御 低サイクル疲労ヒステリシスループ。第1サイクルと安定化サイクル (N=50) の繰返し加工硬化とバウシンガー効果を可視化。");
    reg("ASTM E1820 elastic-plastic fracture toughness J-R curve with experimental J-Δa data, power-law fit, construction blunting line (J = 2σY·Δa), and 0.2 mm offset line yielding JQ.",
        "ASTM E1820 準拠の弾塑性破壊靭性 J-R 抵抗曲線。実験点・べき乗則フィット・鈍化線 (J = 2σY·Δa)・0.2 mm オフセット鈍化線を同一グラフに描画し、交点 JQ を視覚化。");
    reg("Johnson-Cook true stress-strain flow curves of Ti-6Al-4V across 6 decades of strain rate (10^-3 s^-1 quasi-static, 1 s^-1 intermediate, 10^3 s^-1 SHPB dynamic with adiabatic softening).",
        "Ti-6Al-4V 合金の準静的 (10^-3 s^-1)・中速度 (1 s^-1)・高速衝撃 SHPB (10^3 s^-1) の真応力-真ひずみ曲線。ひずみ速度強化と高ひずみ速度域での断熱軟化を Johnson-Cook モデルで再現。");
    reg("Multi-window sample project generating 5 core metal mechanics datasets (T4/T6/T73 tensile + 0.2% yield & Considère analysis, S-N fatigue, da/dN-ΔK crack growth, Hall-Petch, and Charpy DBTT).",
        "金属の力学特性を体系的に学べる複合サンプルプロジェクト。時効別引張比較（＋0.2%耐力・加工硬化解析）・疲労S-N曲線・疲労き裂進展 da/dN-ΔK・Hall-Petch・シャルピーDBTT の一式を一括生成します。");
    reg("Complete research workflow project for Al-Zn-Mg alloys: isothermal age-hardening HV, H-charged tensile embrittlement, 3-rate TDS + Kissinger Ea analysis, XRD pattern, and CT crack-tip strain map.",
        "Al-Zn-Mg 合金の時効組織と水素脆化を統合評価する研究サンプルプロジェクト。時効硬化 HV・水素チャージ引張・3昇温速度 TDS（＋Kissinger 活性化エネルギー解析）・XRD・き裂先端局所ひずみマップを一括生成します。");
    reg("First-order Polanyi-Wigner thermal desorption spectra of H-charged martensitic steel with dislocation (26.8 kJ/mol) and TiC precipitate (58.4 kJ/mol) traps.",
        "マルテンサイト鋼中水素の1次 Polanyi-Wigner 昇温脱離スペクトル（昇温速度 100, 200, 400 °C/h）。転位トラップ (26.8 kJ/mol) と TiC 析出物トラップ (58.4 kJ/mol) の2ピークを再現。");
    reg("Cu Kα1 (λ = 1.540598 Å) diffractogram (2θ = 20–85°) of fcc-Al broadened by D = 42 nm crystallites and ε = 1.85×10^-3 microstrain for Bragg / Williamson-Hall analysis.",
        "Cu Kα1 (λ = 1.540598 Å) による fcc-Al の X 線回折パターン (2θ = 20–85°)。結晶子サイズ D = 42 nm・格子ひずみ ε = 1.85×10^-3 を含み、Bragg / Williamson-Hall 解析に最適。");
    reg("Temperature-dependent lattice H diffusivity D(T) in α-Fe (320–880 K) with standard deviations for Arrhenius activation energy regression.",
        "α-Fe 中の格子間水素拡散係数 D(T) の温度依存性データ (320–860 K, 標準偏差付き)。アレニウスプロットによる拡散活性化エネルギー解析用。");
    reg("128×128 2D matrix of local axial strain ε_yy(x, y) (%) over a 200×200 μm field around a crack tip and micro-void ahead of the plastic zone.",
        "200×200 μm 領域のき裂先端および前方マイクロボイド周囲の局所軸ひずみ ε_yy(x, y) (%) を再現した 128×128 2次元マトリクスデータ。");
    reg("30-specimen ceramic 4-point bending dataset (25 fractures + 5 right-censored runouts at 820 MPa; m ≈ 12.4, σ0 ≈ 785 MPa) for MRR / MLE Weibull probability analysis.",
        "窒化ケイ素セラミックスの4点曲げ強度データ 30 本分（右打ち切りランアウト含む; m ≈ 12.4, σ0 ≈ 785 MPa）。MRR / MLE ワイブル確率解析用。");

    reg("Kissinger TDS / DSC Activation Energy Analyzer",
        "Kissinger 昇温脱離 (TDS / DSC) 活性化エネルギー解析");
    reg("Kissinger Analysis (TDS/DSC)...", "Kissinger解析 (TDS/DSC)...");
    reg("From Worksheet (β, Tp Columns)", "ワークシート列 (昇温速度 β, ピーク温度 Tp)");
    reg("Auto-Extract Peaks from TDS Curves", "マルチ昇温速度TDSスペクトルから Tp を自動抽出");
    reg("Manual Input (β, Tp Lists)", "数値を直接入力 (β, Tp リスト)");
    reg("Heating Rate Column (β):", "昇温速度列 (β):");
    reg("Peak Temperature Column (Tp):", "ピーク温度列 (Tp):");
    reg("Heating Rate Unit:", "昇温速度の単位:");
    reg("K/min (°C/min)", "K/min (°C/min)");
    reg("K/h (°C/h)", "K/h (°C/h)");
    reg("Peak Temperatures in Celsius (°C)", "ピーク温度は摂氏 (°C)");
    reg("Top Temperature Axis in Celsius (°C)", "上軸の温度目盛を摂氏 (°C) で表示");
    reg("TDS Flux Columns (Select 2+ Rates):", "脱離フラックス列 (2条件以上選択):");
    reg("Heating Rates (comma-separated):", "昇温速度リスト (カンマ区切り):");
    reg("Peak Temperatures (comma-separated):", "ピーク温度リスト (カンマ区切り):");
    reg("Peak Search Window Min T:", "ピーク探索 最小温度:");
    reg("Peak Search Window Max T:", "ピーク探索 最大温度:");
    reg("Kissinger analysis requires at least 2 valid (heating rate, peak temperature) pairs.",
        "Kissinger解析には有効な (昇温速度, ピーク温度) の組が2点以上必要です。");

    reg("Kocks-Mecking Work-Hardening & Considere Necking Analyzer",
        "Kocks-Mecking 加工硬化率・Considère くびれ解析");
    reg("Work-Hardening (Kocks-Mecking)...", "加工硬化率・Considère解析...");
    reg("Savitzky-Golay Smoothing Window (odd points):",
        "Savitzky-Golay 平滑化微分ウィンドウ点数 (奇数):");
    reg("Create Considere (θ & σt vs εt) Plot",
        "Considère くびれ判定プロット (θ, σt vs εt) を作成");

    reg("XRD Peak, Scherrer & Williamson-Hall Analyzer",
        "XRD 回折ピーク・Scherrer結晶子径・Williamson-Hall解析");
    reg("XRD Analyzer (Bragg / W-H)...", "XRD解析 (Bragg / W-H)...");
    reg("Diffractogram Curve:", "回折プロファイル曲線:");
    reg("2θ Column (deg):", "回折角 2θ 列 (deg):");
    reg("Intensity Column:", "回折強度列:");
    reg("X-Ray Wavelength λ (Å):", "X線波長 λ (Å):");
    reg("Radiation Source:", "X線源 (ターゲット):");
    reg("Instrumental FWHM (deg):", "装置幅 FWHM_inst (deg):");
    reg("Scherrer Shape Factor K:", "Scherrer 形状因子 K:");
    reg("Crystal System for (hkl) Indexing:", "(hkl) 指数付け結晶系:");
    reg("Strip Cu Kα2 Doublet (Rachinger)", "Cu Kα2 二重線を除去 (Rachinger法)");
    reg("Add Linked Top Bragg d-Spacing Axis (Å)", "上軸に Bragg 面間隔 d (Å) 連動軸を追加");
    reg("Create Williamson-Hall Plot (β cos θ vs 4 sin θ)",
        "Williamson-Hall プロット (β cos θ vs 4 sin θ) を作成");
    reg("No prominent Bragg diffraction peaks were detected on the curve.",
        "曲線上に有意なBragg回折ピークが検出されませんでした。");

    reg("VASP Electronic Structure Plotter (DOS & Band Structure)",
        "VASP 第一原理電子状態プロッター (状態密度 DOS・バンド構造)");
    reg("VASP DOS / Bands Plotter...", "VASP 電子状態プロット (DOS / バンド)...");
    reg("Load VASP DOSCAR / EIGENVAL File...", "VASP DOSCAR / EIGENVAL ファイルを読み込む...");
    reg("DOS / PDOS", "状態密度 (DOS / PDOS)");
    reg("Band Structure", "バンド構造 (Band Structure)");
    reg("Energy Column (eV):", "エネルギー列 (eV):");
    reg("Spin-Up / Total DOS Columns:", "状態密度列 (Total / Spin ↑):");
    reg("Spin-Down DOS Columns (Optional):", "Spin ↓ 状態密度列 (任意):");
    reg("Fermi Energy E_F (eV):", "フェルミ準位 E_F (eV):");
    reg("Vertical Energy Axis (for side-by-side Band + DOS)",
        "エネルギー軸を縦軸にする (バンド図との並列配置用)");
    reg("Wavevector Distance Column (k):", "波数経路距離列 (k):");
    reg("Band Eigenvalue Columns:", "バンド固有値列:");
    reg("High-Symmetry k-Tick Positions:", "高対称 k 点の位置 (カンマ区切り):");
    reg("High-Symmetry k-Tick Labels:", "高対称 k 点ラベル (例: Γ, X, M, K, Γ):");
    reg("Energy Window Min (E - E_F, eV):", "表示エネルギー下限 (E - E_F, eV):");
    reg("Energy Window Max (E - E_F, eV):", "表示エネルギー上限 (E - E_F, eV):");
    reg("Please open a graph or numeric worksheet with XRD data first.",
        "先にXRDデータを含むグラフまたは数値ワークシートを開いてください。");
    reg("None (Peak Table & W-H Only)", "なし (ピーク表・Williamson-Hallのみ)");
    reg("Source worksheet is null.", "元のワークシートが指定されていません。");
    reg("Energy column could not be resolved.", "エネルギー列が見つかりません。");
    reg("At least one numeric DOS column is required.", "数値の状態密度 (DOS) 列が1列以上必要です。");
    reg("Insufficient numeric rows for VASP DOS plot.", "VASP DOSプロットに必要な有効データ行数が不足しています。");
    reg("Wavevector k-distance column could not be resolved.", "波数経路距離列 (k) が見つかりません。");
    reg("At least one band eigenvalue column is required.", "バンド固有値列が1列以上必要です。");
    reg("Insufficient k-points for VASP band structure plot.", "バンド構造プロットに必要な k 点数が不足しています。");
    reg("Peak-aged Al-5.6Zn-2.5Mg-1.6Cu (T6) engineering stress-strain curve (E = 71.2 GPa, σ0.2 ≈ 508 MPa, UTS ≈ 578 MPa) for 0.2% offset yield & Kocks-Mecking Considère analysis.",
        "ピーク時効 Al-5.6Zn-2.5Mg-1.6Cu (T6) 合金の公称応力–ひずみ曲線 (E = 71.2 GPa, σ0.2 ≈ 508 MPa, UTS ≈ 578 MPa)。0.2%耐力および Kocks–Mecking・Considère くびれ解析用。");
    reg("First-order Polanyi-Wigner thermal desorption spectra of H-charged martensitic steel with dislocation (26.8 kJ/mol) and TiC precipitate (58.4 kJ/mol) traps.",
        "水素チャージしたマルテンサイト鋼の1次 Polanyi–Wigner 昇温脱離 (TDS) スペクトル (転位トラップ 26.8 kJ/mol、TiC析出物トラップ 58.4 kJ/mol)。");
    reg("Cu Kα1 (λ = 1.540598 Å) diffractogram (2θ = 20–85°) of fcc-Al broadened by D = 42 nm crystallites and ε = 1.85×10^-3 microstrain for Bragg / Williamson-Hall analysis.",
        "結晶子径 D = 42 nm および不均一ひずみ ε = 1.85×10^-3 により広がった fcc-Al の Cu Kα1 (λ = 1.540598 Å) X線回折プロファイル (2θ = 20–85°)。Bragg面間隔・Williamson–Hall解析用。");
    reg("Temperature-dependent lattice H diffusivity D(T) in α-Fe (320–880 K) with standard deviations for Arrhenius activation energy regression.",
        "α-Fe 中の格子水素拡散係数 D(T) の温度依存性データ (320–880 K、標準偏差付き)。アレニウス活性化エネルギー回帰用。");
    reg("128×128 2D matrix of local axial strain ε_yy(x, y) (%) over a 200×200 μm field around a crack tip and micro-void ahead of the plastic zone.",
        "き裂先端および塑性域前方の微小ボイド周囲 200×200 μm 領域における局所軸ひずみ ε_yy(x, y) (%) の 128×128 2次元マトリクス。");
    reg("30-specimen ceramic 4-point bending dataset (25 fractures + 5 right-censored runouts at 820 MPa; m ≈ 12.4, σ0 ≈ 785 MPa) for MRR / MLE Weibull probability analysis.",
        "セラミックス試験片30本の4点曲げ強度データ (破断25本 + 820 MPa 右打ち切りランアウト5本; m ≈ 12.4, σ0 ≈ 785 MPa)。MRR / 最尤法 (MLE) ワイブル統計解析用。");
    reg("Plot Mode:", "プロット形式:");
    reg("Considère (θ & σt vs εt)", "Considère くびれ判定 (θ, σt vs εt)");
    reg("Kocks-Mecking Stage-III (θ vs σt)", "Kocks-Mecking ステージIII (θ vs σt)");
    reg("HCP / Hexagonal (100, 002, 101...)", "HCP / 六方最密 (100, 002, 101...)");
    reg("Axial Ratio c/a (for HCP):", "軸比 c/a (HCP用):");
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
    auto normArial = [](QFont f, int fallbackPt) -> QFont {
        const int pt = (f.pointSize() > 0) ? f.pointSize() : fallbackPt;
        QFont out(QStringLiteral("Arial"), pt, QFont::Normal);
        out.setBold(false);
        out.setStyleStrategy(f.styleStrategy());
        return out;
    };
    const QFont titleFnt = normArial(g->axisTitleFont(QwtPlot::xBottom), 11);
    const QFont tickFnt = normArial(g->axisFont(QwtPlot::xBottom), 10);
    g->initFonts(titleFnt, tickFnt);
    g->setTitleFont(normArial(titleFnt, 11));
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        g->setAxisTitleFont(a, normArial(g->axisTitleFont(a), 11));
        g->setAxisFont(a, normArial(g->axisFont(a), 10));
    }
    if (g->legend())
        g->legend()->setFont(normArial(g->legend()->font(), 9));
    g->setTextMarkerDefaults(g->textMarkerDefaultFrame(),
                             normArial(g->defaultTextMarkerFont(), 9),
                             g->textMarkerDefaultColor(),
                             g->textMarkerDefaultBackground());
}

double matSciClipSegmentLengthInRect(double x0, double y0, double x1, double y1, const QRectF &r,
                                     bool *outHit = nullptr)
{
    if (outHit)
        *outHit = false;
    double t0 = 0.0, t1 = 1.0;
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    auto clipTest = [&](double p, double q) -> bool {
        if (std::fabs(p) <= 1e-12)
            return q >= 0.0;
        const double t = q / p;
        if (p < 0.0) {
            if (t > t1)
                return false;
            if (t > t0)
                t0 = t;
        } else {
            if (t < t0)
                return false;
            if (t < t1)
                t1 = t;
        }
        return true;
    };
    if (!clipTest(-dx, x0 - r.left()) || !clipTest(dx, r.right() - x0)
        || !clipTest(-dy, y0 - r.top()) || !clipTest(dy, r.bottom() - y0)) {
        return 0.0;
    }
    if (t1 < t0)
        return 0.0;
    if (outHit)
        *outHit = true;
    return std::hypot(dx * (t1 - t0), dy * (t1 - t0));
}

void placeCalloutAutoAvoid(Graph *g, Legend *box, int marginPx = 14, bool allowScaleExpand = true,
                           bool preferUpperRight = false)
{
    if (!g || !box)
        return;
    Plot *pw = g->plotWidget();
    if (!pw || !pw->canvas())
        return;

    g->replot();
    QRect cRect = pw->canvas()->rect();
    QRect bRect = box->rect();
    const int majTickLen = std::max(0, g->majorTickLength());
    const int pad = std::max({ 14, marginPx, majTickLen + 6 });
    QFont calloutFnt = box->font();
    if (calloutFnt.bold()) {
        calloutFnt.setBold(false);
        box->setFont(calloutFnt);
    }
    while (cRect.width() > 40 && bRect.width() > cRect.width() - 2 * pad
           && calloutFnt.pointSize() > 7) {
        calloutFnt.setPointSize(calloutFnt.pointSize() - 1);
        box->setFont(calloutFnt);
        g->replot();
        bRect = box->rect();
    }

    auto solvePlacement = [&](int &outBestL, int &outBestT, double &outBestScore) -> bool {
        cRect = pw->canvas()->rect();
        bRect = box->rect();
        const int bw = bRect.width();
        const int bh = bRect.height();
        const int cw = cRect.width();
        const int ch = cRect.height();
        if (bw <= 0 || bh <= 0 || cw <= 2 * pad + 4 || ch <= 2 * pad + 4)
            return false;

        const int minL = pad;
        const int maxL = std::max(minL, cw - bw - pad);
        const int minT = pad;
        const int maxT = std::max(minT, ch - bh - pad);

        struct AnchorCand {
            int left = 0;
            int top = 0;
            double pref = 0.0;
        };
        QVector<AnchorCand> cands;
        cands.reserve(80);
        if (preferUpperRight) {
            cands.append({ maxL, minT, 0.0 });
            cands.append({ maxL, minT + (maxT - minT) / 6, 0.15 });
            cands.append({ maxL, (minT + maxT) / 2, 0.35 });
            cands.append({ maxL, maxT, 0.55 });
            cands.append({ (minL + maxL) / 2, minT, 2.0 });
            cands.append({ minL, minT, 8.0 });
            cands.append({ minL, maxT, 8.5 });
        } else {
            cands.append({ minL, minT, 0.0 });
            cands.append({ maxL, maxT, 0.1 });
            cands.append({ minL, maxT, 0.2 });
            cands.append({ maxL, minT, 0.3 });
            cands.append({ maxL, (minT + maxT) / 2, 0.45 });
            cands.append({ minL, (minT + maxT) / 2, 0.55 });
            cands.append({ (minL + maxL) / 2, minT, 0.65 });
            cands.append({ (minL + maxL) / 2, maxT, 0.75 });
        }
        for (int gy = 0; gy < 8; ++gy) {
            for (int gx = 0; gx < 8; ++gx) {
                const int lx = minL + qRound((maxL - minL) * (double(gx) / 7.0));
                const int ty = minT + qRound((maxT - minT) * (double(gy) / 7.0));
                const double pBias = preferUpperRight
                        ? (0.4 + 1.8 * (double(7 - gx) / 7.0) + 0.9 * (double(gy) / 7.0))
                        : (0.9 + 0.06 * std::min(gx, 7 - gx) + 0.06 * std::min(gy, 7 - gy));
                cands.append({ lx, ty, pBias });
            }
        }

        const Legend *mainLeg = (g->isLegendVisible() ? g->legend() : nullptr);
        const QRectF legRect = (mainLeg && mainLeg != box)
                ? QRectF(mainLeg->rect()).adjusted(-5, -5, 5, 5)
                : QRectF();

        auto evalCost = [&](int lx, int ty) -> double {
            double cost = 0.0;
            const QRectF testBox(lx - 4.0, ty - 4.0, bw + 8.0, bh + 8.0);
            if (legRect.isValid()) {
                const QRectF inter = testBox.intersected(legRect);
                if (!inter.isEmpty())
                    cost += 200000.0 + inter.width() * inter.height() * 60.0;
            }
            for (int tid : g->textMarkerKeys()) {
                Legend *tm = g->textMarker(tid);
                if (!tm || tm == box || tm == mainLeg || !tm->isVisible())
                    continue;
                const QRectF inter = testBox.intersected(QRectF(tm->rect()).adjusted(-3, -3, 3, 3));
                if (!inter.isEmpty())
                    cost += 80000.0 + inter.width() * inter.height() * 30.0;
            }
            for (int aid : g->lineMarkerKeys()) {
                ArrowMarker *am = g->arrow(aid);
                if (!am)
                    continue;
                const QwtScaleMap xMap = pw->canvasMap(am->xAxis());
                const QwtScaleMap yMap = pw->canvasMap(am->yAxis());
                const QPointF sp = am->startPointCoord();
                const QPointF ep = am->endPointCoord();
                bool hit = false;
                const double len = matSciClipSegmentLengthInRect(
                        xMap.xTransform(sp.x()), yMap.xTransform(sp.y()),
                        xMap.xTransform(ep.x()), yMap.xTransform(ep.y()), testBox, &hit);
                if (hit)
                    cost += 18.0 + len * 2.0;
            }
            for (const Graph::ReferenceLineSpec &rl : g->referenceLines()) {
                if (rl.orientation == 0) {
                    const double py = pw->canvasMap(QwtPlot::yLeft).xTransform(rl.value);
                    if (py >= testBox.top() && py <= testBox.bottom())
                        cost += 4000.0;
                } else {
                    const double px = pw->canvasMap(QwtPlot::xBottom).xTransform(rl.value);
                    if (px >= testBox.left() && px <= testBox.right())
                        cost += 4000.0;
                }
            }
            for (int i = 0; i < g->curves(); ++i) {
                QwtPlotCurve *c = g->curve(i);
                if (!c || !c->isVisible() || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
                    continue;
                const int nPts = c->dataSize();
                if (nPts <= 0)
                    continue;
                const auto *dc = dynamic_cast<const DataCurve *>(c);
                const QwtScaleMap xMap = pw->canvasMap(c->xAxis());
                const QwtScaleMap yMap = pw->canvasMap(c->yAxis());
                const bool hasSym = (c->symbol().style() != QwtSymbol::NoSymbol);
                const bool hasLine = (c->style() != QwtPlotCurve::NoCurve);
                const int stride = (nPts > 4000) ? std::max(1, nPts / 2000) : 1;
                bool hasPrev = false;
                double ppx = 0.0, ppy = 0.0;
                for (int p = 0; p < nPts; p += stride) {
                    if (dc && dc->isPointMasked(p)) {
                        hasPrev = false;
                        continue;
                    }
                    const double vx = c->x(p), vy = c->y(p);
                    if (!std::isfinite(vx) || !std::isfinite(vy)) {
                        hasPrev = false;
                        continue;
                    }
                    const double px = xMap.xTransform(vx);
                    const double py = yMap.xTransform(vy);
                    if (!std::isfinite(px) || !std::isfinite(py)) {
                        hasPrev = false;
                        continue;
                    }
                    if (hasSym && testBox.contains(QPointF(px, py)))
                        cost += 90.0 * stride;
                    if (hasLine && hasPrev) {
                        bool hit = false;
                        const double len =
                                matSciClipSegmentLengthInRect(ppx, ppy, px, py, testBox, &hit);
                        if (hit)
                            cost += (25.0 + len * 4.5) * stride;
                    }
                    ppx = px;
                    ppy = py;
                    hasPrev = true;
                }
            }
            return cost;
        };

        outBestScore = std::numeric_limits<double>::infinity();
        outBestL = std::clamp(bRect.left(), minL, maxL);
        outBestT = std::clamp(bRect.top(), minT, maxT);
        for (const AnchorCand &ac : cands) {
            const double sc = evalCost(ac.left, ac.top) + ac.pref;
            if (sc < outBestScore) {
                outBestScore = sc;
                outBestL = ac.left;
                outBestT = ac.top;
            }
        }
        return true;
    };

    int bestL = 0, bestT = 0;
    double bestScore = 0.0;
    if (!solvePlacement(bestL, bestT, bestScore))
        return;

    // If every candidate hits a curve/reference-line/legend on a compact canvas, step down font to >= 7pt
    while (bestScore >= 25.0 && calloutFnt.pointSize() > 7) {
        calloutFnt.setPointSize(calloutFnt.pointSize() - 1);
        box->setFont(calloutFnt);
        g->replot();
        if (!solvePlacement(bestL, bestT, bestScore))
            break;
    }

    // If still obstructed and scale expansion is allowed on a linear Y axis, add top headroom
    if (allowScaleExpand && bestScore >= 25.0 && !g->isAxisUserLocked(QwtPlot::yLeft)
        && pw->axisScaleEngine(QwtPlot::yLeft)
        && dynamic_cast<QwtLinearScaleEngine *>(pw->axisScaleEngine(QwtPlot::yLeft))) {
        if (const QwtScaleDiv *yd = pw->axisScaleDiv(QwtPlot::yLeft)) {
            const double yLo = yd->lowerBound();
            const double yHi = yd->upperBound();
            if (yHi > yLo && std::isfinite(yHi - yLo)) {
                g->setScale(QwtPlot::yLeft, yLo, yHi + 0.22 * (yHi - yLo));
                g->replot();
                solvePlacement(bestL, bestT, bestScore);
            }
        }
    }

    const double newX = pw->invTransform(QwtPlot::xBottom, bestL);
    const double newY = pw->invTransform(QwtPlot::yLeft, bestT);
    if (std::isfinite(newX) && std::isfinite(newY))
        box->setOriginCoord(newX, newY);
}

Legend *addMatSciCalloutBox(Graph *g, double x, double y, const QString &text,
                            bool preferUpperRight = false)
{
    if (!g)
        return nullptr;
    if (g->isLegendVisible() && g->legend())
        g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);

    Legend *box = g->newLegend(text);
    if (!box)
        return nullptr;
    const int basePt = (g->legend() && g->legend()->font().pointSize() > 0)
            ? g->legend()->font().pointSize()
            : 9;
    QFont calloutFnt(QStringLiteral("Arial"), std::clamp(basePt, 8, 22), QFont::Normal);
    calloutFnt.setBold(false);
    box->setFrameStyle(Legend::Line);
    box->setBackgroundColor(QColor(255, 255, 255, 232));
    box->setTextColor(QColor(20, 30, 50));
    box->setFont(calloutFnt);
    box->setOriginCoord(x, y);

    placeCalloutAutoAvoid(g, box, 14, true, preferUpperRight);

    int markerId = -1;
    for (int tid : g->textMarkerKeys()) {
        if (g->textMarker(tid) == box) {
            markerId = tid;
            break;
        }
    }
    if (markerId >= 0) {
        auto guard = std::make_shared<bool>(false);
        QPointer<Graph> gPtr(g);
        QObject::connect(g, &Graph::modifiedGraph, g,
                         [gPtr, markerId, preferUpperRight, guard]() {
                             if (*guard || !gPtr)
                                 return;
                             Legend *liveBox = gPtr->textMarker(markerId);
                             if (!liveBox || !liveBox->isVisible())
                                 return;
                             *guard = true;
                             placeCalloutAutoAvoid(gPtr.data(), liveBox, 14, false,
                                                   preferUpperRight);
                             *guard = false;
                         });
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

            g->setXAxisTitle(use1000OverT
                                     ? QStringLiteral("Inverse temperature, 1000/<i>T</i> / K<sup>-1</sup>")
                                     : QStringLiteral("Inverse temperature, 1/<i>T</i> / K<sup>-1</sup>"));
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
                topLayer->setAxisTitle(
                        QwtPlot::xTop,
                        tempInCelsius ? QStringLiteral("Temperature, <i>T</i> / \u00B0C")
                                      : QStringLiteral("Temperature, <i>T</i> / K"));
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
            gBode->setRightAxisTitle(QStringLiteral("Phase angle, <i>\u03C6</i> / deg"));
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
        if (pts[k].y > 0.25 * res.utsY && pts[k + 1].y < 0.5 * pts[k].y)
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
                : QStringLiteral("<i>\u03C3</i><sub>%1</sub> = %2 %3")
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
                "<i>\u03C3</i><sub>%1</sub> = %2 %3\n"
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

// ============================================================================
// Campaign 2 Materials Science Workbench:
//   1. Built-in Materials Science Sample Gallery (6 Deterministic Generators)
//   2. Kissinger TDS / DSC Activation Energy Analyzer
//   3. Kocks-Mecking Work-Hardening Rate & Considere Necking Analyzer
// ============================================================================

QVector<CurveMathTools::SampleDatasetInfo> CurveMathTools::listSampleDatasets()
{
    registerMatSciTranslationsOnce();
    QVector<SampleDatasetInfo> list;
    list.reserve(20);
    // --- Metallurgy / Mechanical (13 datasets) ---
    list.append({ QStringLiteral("tensile_alznmg"),
                  QObject::tr("AA7075-T6 Tensile Stress-Strain Curve"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Peak-aged Al-5.6Zn-2.5Mg-1.6Cu (T6) engineering stress-strain curve (E = 71.2 GPa, σ0.2 ≈ 508 MPa, UTS ≈ 578 MPa) for 0.2% offset yield & Kocks-Mecking Considère analysis.") });
    list.append({ QStringLiteral("tensile_temper_compare"),
                  QObject::tr("Al-Zn-Mg Alloy Tensile Comparison by Temper (T4 / T6 / T73)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Multi-curve tensile comparison of solution-treated/naturally aged (T4), peak-aged (T6), and over-aged (T73) Al-Zn-Mg alloy showing strength-ductility trade-off.") });
    list.append({ QStringLiteral("tensile_hydrogen_embrittlement"),
                  QObject::tr("Hydrogen Embrittlement Tensile Curves (Uncharged vs. H-Charged)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Uncharged vs. H-charged (0.8 and 2.2 wt.ppm H) high-strength Al-Zn-Mg tensile curves demonstrating identical elastic/yield behavior with severe hydrogen-induced ductility loss.") });
    list.append({ QStringLiteral("fatigue_sn_curve"),
                  QObject::tr("Fatigue S-N Curves: SCM435 Steel vs. AA7075-T6 (Wöhler Diagram)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("High-cycle fatigue S-N (Wöhler) curves comparing quenched-and-tempered SCM435 steel (distinct endurance limit ~420 MPa at 10^7 cycles) and AA7075-T6 aluminum alloy (no knee).") });
    list.append({ QStringLiteral("fatigue_crack_growth_paris"),
                  QObject::tr("Fatigue Crack Growth Rate da/dN vs. ΔK (Paris Law: Air vs. H-Charged)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Log-log fatigue crack propagation da/dN vs. ΔK showing threshold (Region I), Paris law power-law regime (Region II, m ≈ 3.1), and rapid fracture (Region III) in air and H-charged environments.") });
    list.append({ QStringLiteral("hall_petch_grain_size"),
                  QObject::tr("Hall-Petch Grain Size Strengthening (Ferritic Steel & Al-Mg Alloy)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Yield stress σy vs. inverse square-root grain size d^(-1/2) with error bars and linear Hall-Petch regression for low-carbon ferritic steel (ky ≈ 0.62 MPa·m^1/2) and Al-Mg alloy.") });
    list.append({ QStringLiteral("creep_curve_9cr1mo"),
                  QObject::tr("High-Temperature Creep Curves of 9Cr-1Mo Steel (600 °C, 3 Stress Levels)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Three-stage creep strain vs. time curves (primary, steady-state secondary, tertiary to rupture) of 9Cr-1Mo heat-resistant steel at 600 °C under 120, 140, and 160 MPa.") });
    list.append({ QStringLiteral("charpy_dbtt_curve"),
                  QObject::tr("Charpy Impact Toughness Transition Curve (DBTT of Ferritic Steel)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Charpy V-notch impact absorbed energy vE (J) vs. temperature (-120 to +80 °C) with error bars and ductile-to-brittle transition temperature (DBTT ≈ -25 °C) tanh-sigmoid curve.") });
    list.append({ QStringLiteral("vickers_age_hardening"),
                  QObject::tr("Isothermal Age-Hardening Curves of Al-Zn-Mg Alloy (Vickers HV, 120 vs. 160 °C)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Isothermal Vickers hardness (HV) vs. logarithmic aging time (0.1–1000 h) at 120 °C and 160 °C for Al-Zn-Mg alloy showing under-aged, peak-aged (T6), and over-aged (T7) stages.") });
    list.append({ QStringLiteral("nanoindentation_load_depth"),
                  QObject::tr("Nanoindentation Load-Depth Curves (Oliver-Pharr: Pure Al vs. AA7075-T6)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Berkovich nanoindentation loading (Kick's law P ∝ h^2), peak-load creep hold, and Oliver-Pharr unloading curves comparing soft pure Al and precipitation-hardened AA7075-T6.") });
    list.append({ QStringLiteral("cyclic_hysteresis_bauschinger"),
                  QObject::tr("Low-Cycle Fatigue Hysteresis Loops of SUS316L (Cycle 1 vs. Stabilized N=50)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Symmetric strain-controlled (±1.0%) cyclic stress-strain hysteresis loops of austenitic SUS316L stainless steel comparing Cycle 1 and stabilized Cycle 50 (cyclic hardening & Bauschinger effect).") });
    list.append({ QStringLiteral("fracture_toughness_jr_curve"),
                  QObject::tr("Fracture Toughness J-R Resistance Curve (ASTM E1820 with Blunting Lines)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("ASTM E1820 elastic-plastic fracture toughness J-R curve with experimental J-Δa data, power-law fit, construction blunting line (J = 2σY·Δa), and 0.2 mm offset line yielding JQ.") });
    list.append({ QStringLiteral("strain_rate_flow_stress"),
                  QObject::tr("Strain-Rate Dependent Flow Stress of Ti-6Al-4V (10^-3 to 10^3 s^-1)"),
                  QObject::tr("Metallurgy / Mechanical"),
                  QObject::tr("Johnson-Cook true stress-strain flow curves of Ti-6Al-4V across 6 decades of strain rate (10^-3 s^-1 quasi-static, 1 s^-1 intermediate, 10^3 s^-1 SHPB dynamic with adiabatic softening).") });

    // --- Other Materials Science Datasets (5 datasets) ---
    list.append({ QStringLiteral("tds_hydrogen_3rates"),
                  QObject::tr("Hydrogen TDS Spectra at 3 Heating Rates (100, 200, 400 °C/h)"),
                  QObject::tr("Hydrogen Embrittlement / TDS"),
                  QObject::tr("First-order Polanyi-Wigner thermal desorption spectra of H-charged martensitic steel with dislocation (26.8 kJ/mol) and TiC precipitate (58.4 kJ/mol) traps.") });
    list.append({ QStringLiteral("xrd_aged_alloy"),
                  QObject::tr("Peak-Aged Al-Zn-Mg Alloy XRD Pattern (Cu Kα1)"),
                  QObject::tr("X-Ray Diffraction (XRD)"),
                  QObject::tr("Cu Kα1 (λ = 1.540598 Å) diffractogram (2θ = 20–85°) of fcc-Al broadened by D = 42 nm crystallites and ε = 1.85×10^-3 microstrain for Bragg / Williamson-Hall analysis.") });
    list.append({ QStringLiteral("arrhenius_diffusion"),
                  QObject::tr("Hydrogen Interstitial Diffusivity in α-Fe"),
                  QObject::tr("Diffusion / Kinetics"),
                  QObject::tr("Temperature-dependent lattice H diffusivity D(T) in α-Fe (320–880 K) with standard deviations for Arrhenius activation energy regression.") });
    list.append({ QStringLiteral("ct_local_strain_map"),
                  QObject::tr("3D-XRD / Synchrotron CT Crack-Tip Local Strain Map"),
                  QObject::tr("Synchrotron CT / Local Strain"),
                  QObject::tr("128×128 2D matrix of local axial strain ε_yy(x, y) (%) over a 200×200 μm field around a crack tip and micro-void ahead of the plastic zone.") });
    list.append({ QStringLiteral("weibull_ceramics"),
                  QObject::tr("Sintered Si3N4 4-Point Flexural Strength (JIS R 1625)"),
                  QObject::tr("Ceramics / Fracture Statistics"),
                  QObject::tr("30-specimen ceramic 4-point bending dataset (25 fractures + 5 right-censored runouts at 820 MPa; m ≈ 12.4, σ0 ≈ 785 MPa) for MRR / MLE Weibull probability analysis.") });

    // --- Multi-Window Sample Project Packs (2 packs) ---
    list.append({ QStringLiteral("project_metal_mechanics_suite"),
                  QObject::tr("Project Pack: Metal Mechanical Properties Evaluation Suite (5 Datasets + Analyses)"),
                  QObject::tr("Sample Project Pack"),
                  QObject::tr("Multi-window sample project generating 5 core metal mechanics datasets (T4/T6/T73 tensile + 0.2% yield & Considère analysis, S-N fatigue, da/dN-ΔK crack growth, Hall-Petch, and Charpy DBTT).") });
    list.append({ QStringLiteral("project_alznmg_he_research"),
                  QObject::tr("Project Pack: Al-Zn-Mg Age-Hardening & Hydrogen Embrittlement Study (5 Datasets + Kissinger)"),
                  QObject::tr("Sample Project Pack"),
                  QObject::tr("Complete research workflow project for Al-Zn-Mg alloys: isothermal age-hardening HV, H-charged tensile embrittlement, 3-rate TDS + Kissinger Ea analysis, XRD pattern, and CT crack-tip strain map.") });

    return list;
}

MultiLayer *CurveMathTools::loadSampleDataset(ApplicationWindow *app, const QString &datasetId,
                                              bool autoPlot, Table **outTable, Matrix **outMatrix)
{
    registerMatSciTranslationsOnce();
    if (outTable)
        *outTable = nullptr;
    if (outMatrix)
        *outMatrix = nullptr;
    if (!app)
        return nullptr;

    const QString key = datasetId.trimmed().toLower();

    // 1. AA7075-T6 Tensile Stress-Strain
    if (key == QLatin1String("tensile_alznmg") || key == QLatin1String("tensile")) {
        constexpr int N = 241;
        std::vector<double> strainPct(N), stressMPa(N);
        constexpr double E_MPa = 71200.0; // 71.2 GPa
        constexpr double sigY0 = 492.0;
        constexpr double voceQ = 132.0;
        constexpr double voceB = 26.5;
        constexpr double theta4 = 235.0;

        double utsTrueEps = 0.108;
        double prevSigEng = 0.0;
        for (int i = 0; i < N; ++i) {
            const double ePct = 14.6 * double(i) / double(N - 1);
            const double eFrac = 0.01 * ePct;
            const double epsT = std::log(1.0 + eFrac);
            // Smooth elastic-plastic transition via harmonic-like blend
            const double sigEl = E_MPa * epsT;
            const double epsP = std::max(0.0, epsT - sigY0 / E_MPa);
            const double sigPl = sigY0 + voceQ * (1.0 - std::exp(-voceB * epsP)) + theta4 * epsP;
            double sigT = 0.0;
            if (sigEl <= sigY0 * 0.92) {
                sigT = sigEl;
            } else {
                // Smooth C1 knee between linear elastic and Voce+Stage-IV hardening
                const double w = 1.0 - std::exp(-E_MPa * std::max(0.0, epsT - 0.92 * sigY0 / E_MPa)
                                                / (0.08 * sigY0 + 18.0));
                const double sigBlend = 0.92 * sigY0 + (sigPl - 0.92 * sigY0) * w;
                sigT = std::min(sigEl, sigBlend);
            }
            double sigEng = sigT / (1.0 + eFrac);
            if (i > 0 && sigEng >= prevSigEng)
                utsTrueEps = epsT;
            if (epsT > utsTrueEps) {
                const double dEps = epsT - utsTrueEps;
                sigEng -= 2650.0 * dEps * dEps;
            }
            prevSigEng = std::max(prevSigEng, sigEng);
            strainPct[size_t(i)] = ePct;
            stressMPa[size_t(i)] = std::max(0.0, sigEng);
        }

        Table *tbl = createNumericTable(
                app, QStringLiteral("AA7075Tensile"),
                QStringList() << QStringLiteral("Strain") << QStringLiteral("Stress"),
                QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                { strainPct, stressMPa });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("Nominal strain, <i>e</i>"));
            c0->setUnits(QStringLiteral("%"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(QStringLiteral("Nominal stress, <i>\u03C3</i>"));
            c1->setUnits(QStringLiteral("MPa"));
            c1->setComment(QStringLiteral("AA7075-T6"));
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(QStringLiteral("AA7075TensilePlot"));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            insertStyledCurve(g, tbl, 0, 1, N, QPen(QColor(0, 114, 178), 2.1, Qt::SolidLine));
            g->setXAxisTitle(QStringLiteral("Nominal strain, <i>e</i> / %"));
            g->setYAxisTitle(QStringLiteral("Nominal stress, <i>\u03C3</i> / MPa"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 0.0, 16.0);
            g->setScale(QwtPlot::yLeft, 0.0, 650.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 2. Hydrogen TDS at 3 Heating Rates (100, 200, 400 °C/h)
    if (key == QLatin1String("tds_hydrogen_3rates") || key == QLatin1String("tds")) {
        constexpr int N = 201;
        constexpr double kR = 8.314462618;
        const double ratesCh[3] = { 100.0, 200.0, 400.0 };
        std::vector<double> tempC(N), flux1(N, 0.0), flux2(N, 0.0), flux3(N, 0.0);
        for (int i = 0; i < N; ++i)
            tempC[size_t(i)] = 30.0 + 3.0 * double(i); // 30 .. 630 °C

        auto simulateTds = [&](double rateCh, std::vector<double> &outFlux) {
            const double betaKs = rateCh / 3600.0; // K / s
            // Trap 1: Dislocations (Ed1 = 26.8 kJ/mol, A1 = 18.0 s^-1, C0 = 0.85 wt.ppm)
            // Trap 2: TiC precipitates (Ed2 = 58.4 kJ/mol, A2 = 1650.0 s^-1, C0 = 0.55 wt.ppm)
            double th1 = 1.0, th2 = 1.0;
            double prevTK = tempC[0] + 273.15;
            for (int i = 0; i < N; ++i) {
                const double tK = tempC[size_t(i)] + 273.15;
                if (i > 0) {
                    const double dT = tK - prevTK;
                    constexpr int kSub = 8;
                    const double h = dT / double(kSub);
                    for (int s = 0; s < kSub; ++s) {
                        const double tMid = prevTK + (s + 0.5) * h;
                        const double k1 = (18.0 / betaKs) * std::exp(-26800.0 / (kR * tMid));
                        const double k2 = (1650.0 / betaKs) * std::exp(-58400.0 / (kR * tMid));
                        th1 *= std::exp(-k1 * h);
                        th2 *= std::exp(-k2 * h);
                    }
                }
                const double r1 = 0.85 * 18.0 * th1 * std::exp(-26800.0 / (kR * tK));
                const double r2 = 0.55 * 1650.0 * th2 * std::exp(-58400.0 / (kR * tK));
                outFlux[size_t(i)] = (r1 + r2) * 1e3; // 10^-3 wt.ppm / s
                prevTK = tK;
            }
        };
        simulateTds(ratesCh[0], flux1);
        simulateTds(ratesCh[1], flux2);
        simulateTds(ratesCh[2], flux3);

        Table *tbl = createNumericTable(
                app, QStringLiteral("SteelTDS3Rates"),
                QStringList() << QStringLiteral("TempC") << QStringLiteral("Flux100Ch")
                              << QStringLiteral("Flux200Ch") << QStringLiteral("Flux400Ch"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y,
                { tempC, flux1, flux2, flux3 });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("Temperature, <i>T</i>"));
            c0->setUnits(QStringLiteral("\u00B0C"));
        }
        const char *comments[3] = { "100 \u00B0C/h", "200 \u00B0C/h", "400 \u00B0C/h" };
        for (int k = 0; k < 3; ++k) {
            if (Column *ck = tbl->column(k + 1)) {
                ck->setLongName(QStringLiteral("Desorption rate"));
                ck->setUnits(QStringLiteral("10<sup>-3</sup> wt.ppm s<sup>-1</sup>"));
                ck->setComment(QString::fromUtf8(comments[k]));
            }
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("SteelTDSPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            const QColor cols[3] = { QColor(0, 114, 178), QColor(0, 158, 115), QColor(213, 94, 0) };
            for (int k = 0; k < 3; ++k)
                insertStyledCurve(g, tbl, 0, k + 1, N, QPen(cols[k], 2.0, Qt::SolidLine));
            g->setXAxisTitle(QStringLiteral("Temperature, <i>T</i> / \u00B0C"));
            g->setYAxisTitle(
                    QStringLiteral("Desorption rate / 10<sup>-3</sup> wt.ppm s<sup>-1</sup>"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setAutoScale();
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 3. Peak-Aged Al-Zn-Mg XRD Pattern (Cu Kα1)
    if (key == QLatin1String("xrd_aged_alloy") || key == QLatin1String("xrd")) {
        constexpr int N = 1301;
        constexpr double lambda = 1.540598; // Cu Kα1 (Å)
        constexpr double a0 = 4.0495;       // fcc-Al (Å)
        constexpr double D_nm = 42.0;
        constexpr double epsMicro = 1.85e-3;
        constexpr double fwhmInstDeg = 0.05;

        struct Refl {
            int h2k2l2;
            double relI;
        };
        const Refl alPeaks[5] = { { 3, 1000.0 }, { 4, 470.0 }, { 8, 260.0 },
                                  { 11, 290.0 }, { 12, 85.0 } };

        std::vector<double> twoTheta(N), intensity(N);
        for (int i = 0; i < N; ++i) {
            const double tt = 20.0 + 0.05 * double(i);
            twoTheta[size_t(i)] = tt;
            // Smooth low-angle scattering background
            double bg = 32.0 + 48.0 * std::exp(-(tt - 20.0) / 18.0);
            double sumI = bg;
            for (const Refl &pk : alPeaks) {
                const double d = a0 / std::sqrt(double(pk.h2k2l2));
                const double sinTh = lambda / (2.0 * d);
                if (sinTh >= 0.995)
                    continue;
                const double thRad = std::asin(sinTh);
                const double centerDeg = 2.0 * thRad * (180.0 / M_PI);
                const double betaPhysRad =
                        (0.90 * lambda) / (10.0 * D_nm * std::cos(thRad)) + 4.0 * epsMicro * std::tan(thRad);
                const double fwhmPhysDeg = betaPhysRad * (180.0 / M_PI);
                const double fwhmTot = std::hypot(fwhmPhysDeg, fwhmInstDeg);
                const double u = (tt - centerDeg) / (0.5 * fwhmTot);
                const double pVoigt = 0.45 / (1.0 + u * u) + 0.55 * std::exp(-M_LN2 * u * u);
                sumI += pk.relI * pVoigt;
            }
            intensity[size_t(i)] = sumI;
        }

        Table *tbl = createNumericTable(
                app, QStringLiteral("AlZnMgXRD"),
                QStringList() << QStringLiteral("TwoTheta") << QStringLiteral("Intensity"),
                QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                { twoTheta, intensity });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("Diffraction angle, 2<i>\u03B8</i>"));
            c0->setUnits(QStringLiteral("deg"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(QStringLiteral("Intensity"));
            c1->setUnits(QStringLiteral("counts"));
            c1->setComment(QStringLiteral("Al-5.5Zn-2.5Mg (T6)"));
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(QStringLiteral("AlZnMgXRDPlot"));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            insertStyledCurve(g, tbl, 0, 1, N, QPen(QColor(25, 45, 85), 1.6, Qt::SolidLine));
            g->setXAxisTitle(QStringLiteral("Diffraction angle, 2<i>\u03B8</i> / deg"));
            g->setYAxisTitle(QStringLiteral("Intensity / counts"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 20.0, 85.0);
            ml->applyAspectRatioPreset(4.0 / 3.0, true);
            g->setAutoScaleAxis(QwtPlot::yLeft);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 4. Hydrogen Diffusivity in α-Fe (Arrhenius)
    if (key == QLatin1String("arrhenius_diffusion") || key == QLatin1String("arrhenius")) {
        const std::vector<double> tempK = { 320.0, 360.0, 410.0, 470.0, 540.0, 620.0, 730.0, 860.0 };
        const double factors[8] = { 0.97, 1.03, 0.98, 1.02, 1.01, 0.98, 1.02, 0.99 };
        std::vector<double> diff(8), diffSD(8);
        constexpr double D0 = 7.8e-8;     // m^2 / s
        constexpr double Ea = 13400.0;    // J / mol (13.4 kJ/mol)
        constexpr double R = 8.314462618; // J / (mol K)
        for (size_t i = 0; i < 8; ++i) {
            diff[i] = D0 * std::exp(-Ea / (R * tempK[i])) * factors[i];
            diffSD[i] = 0.065 * diff[i];
        }

        Table *tbl = createNumericTable(
                app, QStringLiteral("FeHDiffusivity"),
                QStringList() << QStringLiteral("TempK") << QStringLiteral("Diffusivity")
                              << QStringLiteral("DiffusivitySD"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::yErr,
                { tempK, diff, diffSD });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("Temperature, <i>T</i>"));
            c0->setUnits(QStringLiteral("K"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(QStringLiteral("Diffusivity, <i>D</i>"));
            c1->setUnits(QStringLiteral("m<sup>2</sup> s<sup>-1</sup>"));
            c1->setComment(QStringLiteral("H in \u03B1-Fe"));
        }
        if (Column *c2 = tbl->column(2)) {
            c2->setLongName(QStringLiteral("SD"));
            c2->setUnits(QStringLiteral("m<sup>2</sup> s<sup>-1</sup>"));
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("FeHDiffPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Scatter, 0, 7)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(25, 65, 135), 1.5));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(35, 95, 185)),
                                            QPen(QColor(15, 45, 95), 1.2), QSize(8, 8)));
                g->addErrorBars(tbl->colName(1), tbl, tbl->colName(2), 1, 1, 6,
                                QColor(25, 65, 135), true, true, true);
            }
            g->setXAxisTitle(QStringLiteral("Temperature, <i>T</i> / K"));
            g->setYAxisTitle(QStringLiteral("Diffusivity, <i>D</i> / m<sup>2</sup> s<sup>-1</sup>"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setAutoScale();
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 5. Synchrotron CT / 3D-XRD Crack-Tip Local Strain Map (128x128 Matrix)
    if (key == QLatin1String("ct_local_strain_map") || key == QLatin1String("ct")) {
        constexpr int G = 128;
        const QString matName = makeUniqueTableName(app, QStringLiteral("CTCrackTipStrain"));
        Matrix *mat = app->newMatrix(matName, G, G);
        if (outMatrix)
            *outMatrix = mat;
        if (!mat)
            return nullptr;

        mat->setCoordinates(0.0, 200.0, 0.0, 200.0);
        mat->setWindowLabel(
                QStringLiteral("Synchrotron 3D-XRD Local Strain <i>\u03B5</i><sub>yy</sub> (%)"));
        QVector<qreal> cells(G * G, 0.0);
        // Crack tip at (x0 = 68 μm, y0 = 100 μm) + ahead micro-void at (136 μm, 104 μm)
        for (int r = 0; r < G; ++r) {
            const double y = 200.0 * double(r) / double(G - 1);
            for (int c = 0; c < G; ++c) {
                const double x = 200.0 * double(c) / double(G - 1);
                const double dx = x - 68.0;
                const double dy = y - 100.0;
                const double rad = std::sqrt(dx * dx + dy * dy + 18.0);
                const double theta = std::atan2(dy, dx);
                // Mode-I Westergaard angular lobe: cos(theta/2) * (1 + sin(theta/2)*sin(3*theta/2))
                const double th2 = 0.5 * theta;
                const double modeI =
                        std::cos(th2) * (1.0 + std::sin(th2) * std::sin(3.0 * th2));
                double epsPct = 6.2 * modeI / std::sqrt(rad);
                // Compressive wake behind crack faces (dx < 0, |dy| < 12)
                if (dx < 0.0)
                    epsPct -= 0.28 * std::exp(-(dy * dy) / 120.0) * std::exp(dx / 45.0);
                // Ahead micro-void strain concentration at (136, 104)
                const double vx = x - 136.0;
                const double vy = y - 104.0;
                const double vRad2 = vx * vx + vy * vy + 28.0;
                epsPct += 18.0 * (vy * vy - 0.35 * vx * vx + 14.0) / (vRad2 * std::sqrt(vRad2));
                cells[r * G + c] = std::clamp(epsPct, -0.25, 1.45);
            }
        }
        mat->setCells(cells);
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("CTStrainMapPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            g->plotSpectrogram(mat, Graph::ColorMap);
            if (Plot *pw = g->plotWidget()) {
                for (QwtPlotItem *it : pw->itemList()) {
                    if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                        auto *sp = static_cast<Spectrogram *>(it);
                        sp->setColorMapPreset(Spectrogram::PresetViridis, false);
                        sp->showColorScale(QwtPlot::yRight, true);
                        sp->setColorBarTitle(
                                QStringLiteral("Local strain, <i>\u03B5</i><sub>yy</sub> / %"));
                        sp->enforceColorBarArialFont();
                    }
                }
            }
            g->setXAxisTitle(QStringLiteral("Position, <i>x</i> / \u03BCm"));
            g->setYAxisTitle(QStringLiteral("Position, <i>y</i> / \u03BCm"));
            g->setTitle(QString());
            g->removeLegend();
            applyArialToMatSciGraph(g);
            g->replot();
        }
        return ml;
    }

    // 6. Sintered Si3N4 Ceramic 4-Point Bending Weibull Dataset (JIS R 1625)
    if (key == QLatin1String("weibull_ceramics") || key == QLatin1String("weibull")) {
        constexpr int N = 30;
        constexpr double mTrue = 12.4;
        constexpr double sig0True = 785.0;
        constexpr double runoutMPa = 820.0;

        std::vector<double> strength(N), status(N, 1.0);
        for (int i = 0; i < N; ++i) {
            // Deterministic quantiles with realistic ceramic flaw scatter
            const double p = (double(i + 1) - 0.3) / (double(N) + 0.4);
            const double ripple = 1.0 + 0.008 * std::sin(2.1 * double(i + 1));
            const double sVal = sig0True * std::pow(-std::log(1.0 - p), 1.0 / mTrue) * ripple;
            if (i >= N - 2 || sVal >= runoutMPa) {
                strength[size_t(i)] = runoutMPa;
                status[size_t(i)] = 0.0; // Right-censored runout
            } else {
                strength[size_t(i)] = std::round(sVal * 10.0) / 10.0;
                status[size_t(i)] = 1.0; // Failure
            }
        }

        Table *tbl = createNumericTable(
                app, QStringLiteral("Si3N4Weibull"),
                QStringList() << QStringLiteral("FlexuralStrength") << QStringLiteral("Status"),
                QList<SciDAVis::PlotDesignation>() << SciDAVis::Y << SciDAVis::noDesignation,
                { strength, status });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("Flexural strength, <i>\u03C3</i><sub>f</sub>"));
            c0->setUnits(QStringLiteral("MPa"));
            c0->setComment(QStringLiteral("Si<sub>3</sub>N<sub>4</sub> (JIS R 1625)"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(QStringLiteral("Status (1=Fail, 0=Censored)"));
        }
        if (!autoPlot)
            return nullptr;

        const auto wb = GraphStatsAndSurfaceFit::plotWeibullProbability(
                app, tbl, QStringLiteral("FlexuralStrength"), QStringLiteral("Status"), 0, false,
                false, true);
        return wb.weibullPlot;
    }

    // 7. Al-Zn-Mg Alloy Tensile Comparison by Temper (T4 / T6 / T73)
    if (key == QLatin1String("tensile_temper_compare") || key == QLatin1String("temper")) {
        constexpr int N = 201;
        auto synthTensile = [&](double eMaxPct, double E_MPa, double sigY0, double Q, double b,
                                double th4, double neckDrop, std::vector<double> &outE,
                                std::vector<double> &outS) {
            outE.resize(N);
            outS.resize(N);
            double utsTrueEps = 0.01 * eMaxPct * 0.72;
            double prevSigEng = 0.0;
            for (int i = 0; i < N; ++i) {
                const double ePct = eMaxPct * double(i) / double(N - 1);
                const double eFrac = 0.01 * ePct;
                const double epsT = std::log(1.0 + eFrac);
                const double sigEl = E_MPa * epsT;
                const double epsP = std::max(0.0, epsT - sigY0 / E_MPa);
                const double sigPl = sigY0 + Q * (1.0 - std::exp(-b * epsP)) + th4 * epsP;
                double sigT = 0.0;
                if (sigEl <= sigY0 * 0.92) {
                    sigT = sigEl;
                } else {
                    const double w = 1.0 - std::exp(-E_MPa * std::max(0.0, epsT - 0.92 * sigY0 / E_MPa)
                                                    / (0.08 * sigY0 + 18.0));
                    sigT = std::min(sigEl, 0.92 * sigY0 + (sigPl - 0.92 * sigY0) * w);
                }
                double sigEng = sigT / (1.0 + eFrac);
                if (i > 0 && sigEng >= prevSigEng)
                    utsTrueEps = epsT;
                if (epsT > utsTrueEps) {
                    const double dEps = epsT - utsTrueEps;
                    sigEng -= neckDrop * dEps * dEps;
                }
                prevSigEng = std::max(prevSigEng, sigEng);
                outE[size_t(i)] = ePct;
                outS[size_t(i)] = std::max(0.0, sigEng);
            }
        };

        std::vector<double> eT4, sT4, eT6, sT6, eT73, sT73;
        synthTensile(19.2, 70500.0, 295.0, 245.0, 14.5, 290.0, 1450.0, eT4, sT4);
        synthTensile(14.2, 71200.0, 492.0, 132.0, 26.5, 235.0, 2650.0, eT6, sT6);
        synthTensile(15.8, 71000.0, 422.0, 128.0, 22.0, 210.0, 2150.0, eT73, sT73);

        Table *tbl = createNumericTable(
                app, QStringLiteral("AlZnMgTemperTensile"),
                QStringList() << QStringLiteral("Strain_T4") << QStringLiteral("Stress_T4")
                              << QStringLiteral("Strain_T6") << QStringLiteral("Stress_T6")
                              << QStringLiteral("Strain_T73") << QStringLiteral("Stress_T73"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::X << SciDAVis::Y
                        << SciDAVis::X << SciDAVis::Y,
                { eT4, sT4, eT6, sT6, eT73, sT73 });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        const char *conds[3] = { "T4 (Naturally aged)", "T6 (Peak aged)", "T73 (Over aged)" };
        for (int k = 0; k < 3; ++k) {
            if (Column *cx = tbl->column(2 * k)) {
                cx->setLongName(QStringLiteral("Nominal strain, <i>e</i>"));
                cx->setUnits(QStringLiteral("%"));
            }
            if (Column *cy = tbl->column(2 * k + 1)) {
                cy->setLongName(QStringLiteral("Nominal stress, <i>\u03C3</i>"));
                cy->setUnits(QStringLiteral("MPa"));
                cy->setComment(QString::fromUtf8(conds[k]));
            }
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml =
                app->newGraph(makeUniqueTableName(app, QStringLiteral("AlZnMgTemperPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            const QColor cols[3] = { QColor(0, 158, 115), QColor(0, 114, 178), QColor(213, 94, 0) };
            for (int k = 0; k < 3; ++k)
                insertStyledCurve(g, tbl, 2 * k, 2 * k + 1, N, QPen(cols[k], 2.1, Qt::SolidLine));
            g->setXAxisTitle(QStringLiteral("Nominal strain, <i>e</i> / %"));
            g->setYAxisTitle(QStringLiteral("Nominal stress, <i>\u03C3</i> / MPa"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 0.0, 20.0);
            g->setScale(QwtPlot::yLeft, 0.0, 650.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 8. Hydrogen Embrittlement Tensile Curves (Uncharged vs. H-Charged)
    if (key == QLatin1String("tensile_hydrogen_embrittlement") || key == QLatin1String("he_tensile")) {
        constexpr int N = 201;
        auto synthHE = [&](double eFracMaxPct, bool suddenFracture, std::vector<double> &outE,
                           std::vector<double> &outS) {
            outE.resize(N);
            outS.resize(N);
            constexpr double E_MPa = 71200.0;
            constexpr double sigY0 = 492.0;
            constexpr double Q = 132.0;
            constexpr double b = 26.5;
            constexpr double th4 = 235.0;
            double utsTrueEps = 0.105;
            double prevSigEng = 0.0;
            for (int i = 0; i < N; ++i) {
                const double ePct = eFracMaxPct * double(i) / double(N - 1);
                const double eFrac = 0.01 * ePct;
                const double epsT = std::log(1.0 + eFrac);
                const double sigEl = E_MPa * epsT;
                const double epsP = std::max(0.0, epsT - sigY0 / E_MPa);
                const double sigPl = sigY0 + Q * (1.0 - std::exp(-b * epsP)) + th4 * epsP;
                double sigT = 0.0;
                if (sigEl <= sigY0 * 0.92) {
                    sigT = sigEl;
                } else {
                    const double w = 1.0 - std::exp(-E_MPa * std::max(0.0, epsT - 0.92 * sigY0 / E_MPa)
                                                    / (0.08 * sigY0 + 18.0));
                    sigT = std::min(sigEl, 0.92 * sigY0 + (sigPl - 0.92 * sigY0) * w);
                }
                double sigEng = sigT / (1.0 + eFrac);
                if (i > 0 && sigEng >= prevSigEng)
                    utsTrueEps = epsT;
                if (!suddenFracture && epsT > utsTrueEps) {
                    const double dEps = epsT - utsTrueEps;
                    sigEng -= 2650.0 * dEps * dEps;
                }
                prevSigEng = std::max(prevSigEng, sigEng);
                outE[size_t(i)] = ePct;
                outS[size_t(i)] = std::max(0.0, sigEng);
            }
        };

        std::vector<double> e0, s0, e1, s1, e2, s2;
        synthHE(14.5, false, e0, s0); // Uncharged (ductile necking)
        synthHE(7.8, true, e1, s1);   // 0.8 wt.ppm H (quasi-brittle fracture)
        synthHE(3.4, true, e2, s2);   // 2.2 wt.ppm H (severe HE fracture)

        Table *tbl = createNumericTable(
                app, QStringLiteral("AlZnMgHETensile"),
                QStringList() << QStringLiteral("Strain_Uncharged")
                              << QStringLiteral("Stress_Uncharged")
                              << QStringLiteral("Strain_0p8ppmH")
                              << QStringLiteral("Stress_0p8ppmH")
                              << QStringLiteral("Strain_2p2ppmH")
                              << QStringLiteral("Stress_2p2ppmH"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::X << SciDAVis::Y
                        << SciDAVis::X << SciDAVis::Y,
                { e0, s0, e1, s1, e2, s2 });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        const char *conds[3] = { "Uncharged (0.05 wt.ppm H)", "H-charged (0.8 wt.ppm H)",
                                 "H-charged (2.2 wt.ppm H)" };
        for (int k = 0; k < 3; ++k) {
            if (Column *cx = tbl->column(2 * k)) {
                cx->setLongName(QStringLiteral("Nominal strain, <i>e</i>"));
                cx->setUnits(QStringLiteral("%"));
            }
            if (Column *cy = tbl->column(2 * k + 1)) {
                cy->setLongName(QStringLiteral("Nominal stress, <i>\u03C3</i>"));
                cy->setUnits(QStringLiteral("MPa"));
                cy->setComment(QString::fromUtf8(conds[k]));
            }
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("AlZnMgHETensilePlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            const QColor cols[3] = { QColor(0, 114, 178), QColor(230, 159, 0), QColor(213, 94, 0) };
            for (int k = 0; k < 3; ++k)
                insertStyledCurve(g, tbl, 2 * k, 2 * k + 1, N, QPen(cols[k], 2.1, Qt::SolidLine));
            g->setXAxisTitle(QStringLiteral("Nominal strain, <i>e</i> / %"));
            g->setYAxisTitle(QStringLiteral("Nominal stress, <i>\u03C3</i> / MPa"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 0.0, 16.0);
            g->setScale(QwtPlot::yLeft, 0.0, 650.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 9. Fatigue S-N Curves: SCM435 Steel vs. AA7075-T6 (Wöhler Diagram)
    if (key == QLatin1String("fatigue_sn_curve") || key == QLatin1String("sn_curve")) {
        const std::vector<double> nSteel = { 1.2e4, 2.8e4, 6.5e4, 1.5e5, 3.8e5, 9.0e5,
                                             2.2e6, 5.0e6, 1.0e7, 1.0e7 };
        const std::vector<double> sSteel = { 660.0, 610.0, 560.0, 515.0, 470.0, 435.0,
                                             422.0, 420.0, 420.0, 405.0 };
        const std::vector<double> nAl = { 1.0e4, 2.5e4, 6.0e4, 1.6e5, 4.2e5, 1.1e6,
                                          2.8e6, 6.5e6, 1.5e7, 3.0e7 };
        const std::vector<double> sAl = { 385.0, 345.0, 310.0, 275.0, 245.0, 218.0,
                                          196.0, 178.0, 162.0, 150.0 };

        Table *tbl = createNumericTable(
                app, QStringLiteral("FatigueSNData"),
                QStringList() << QStringLiteral("Cycles_SCM435") << QStringLiteral("StressAmp_SCM435")
                              << QStringLiteral("Cycles_AA7075") << QStringLiteral("StressAmp_AA7075"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::X << SciDAVis::Y,
                { nSteel, sSteel, nAl, sAl });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("Number of cycles to failure, <i>N</i><sub>f</sub>"));
            c0->setUnits(QStringLiteral("cycles"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(QStringLiteral("Stress amplitude, <i>\u03C3</i><sub>a</sub>"));
            c1->setUnits(QStringLiteral("MPa"));
            c1->setComment(QStringLiteral("SCM435 Steel (R = -1)"));
        }
        if (Column *c2 = tbl->column(2)) {
            c2->setLongName(QStringLiteral("Number of cycles to failure, <i>N</i><sub>f</sub>"));
            c2->setUnits(QStringLiteral("cycles"));
        }
        if (Column *c3 = tbl->column(3)) {
            c3->setLongName(QStringLiteral("Stress amplitude, <i>\u03C3</i><sub>a</sub>"));
            c3->setUnits(QStringLiteral("MPa"));
            c3->setComment(QStringLiteral("AA7075-T6 (R = -1)"));
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("FatigueSNPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            const int nPts = static_cast<int>(nSteel.size());
            if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::LineSymbols, 0, nPts - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(0, 114, 178), 1.9));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(0, 114, 178)),
                                            QPen(QColor(0, 70, 120), 1.2), QSize(8, 8)));
            }
            if (g->insertCurve(tbl, tbl->colName(2), tbl->colName(3), Graph::LineSymbols, 0, nPts - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(213, 94, 0), 1.9, Qt::DashLine));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Rect, QBrush(QColor(213, 94, 0)),
                                            QPen(QColor(140, 55, 0), 1.2), QSize(8, 8)));
            }
            g->setXAxisTitle(QStringLiteral("Number of cycles to failure, <i>N</i><sub>f</sub>"));
            g->setYAxisTitle(QStringLiteral("Stress amplitude, <i>\u03C3</i><sub>a</sub> / MPa"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 1.0e4, 5.0e7, 0.0, 5, 8, 1 /*Log10*/);
            g->setScale(QwtPlot::yLeft, 100.0, 750.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 10. Fatigue Crack Growth Rate da/dN vs. ΔK (Paris Law: Air vs. H-Charged)
    if (key == QLatin1String("fatigue_crack_growth_paris") || key == QLatin1String("paris")) {
        constexpr int N = 28;
        std::vector<double> dkAir(N), dadnAir(N), dkH(N), dadnH(N);
        // Forman-like 3-regime model: da/dN = C * (ΔK^m - ΔKth^m) / (1 - ΔK/Kc)
        constexpr double C_air = 1.8e-11, m_air = 3.15, dkTh_air = 5.2, kc_air = 58.0;
        constexpr double C_h = 8.5e-11, m_h = 3.25, dkTh_h = 3.6, kc_h = 42.0;
        for (int i = 0; i < N; ++i) {
            const double t = double(i) / double(N - 1);
            const double kAir = 5.45 * std::pow(50.0 / 5.45, t);
            const double rAir =
                    C_air * std::max(1e-4, std::pow(kAir, m_air) - std::pow(dkTh_air, m_air))
                    / std::max(0.08, 1.0 - kAir / kc_air);
            const double rippleA = 1.0 + 0.06 * std::sin(1.9 * double(i + 1));
            dkAir[size_t(i)] = kAir;
            dadnAir[size_t(i)] = rAir * rippleA;

            const double kH = 3.80 * std::pow(36.0 / 3.80, t);
            const double rH = C_h * std::max(1e-4, std::pow(kH, m_h) - std::pow(dkTh_h, m_h))
                    / std::max(0.08, 1.0 - kH / kc_h);
            const double rippleH = 1.0 + 0.06 * std::cos(2.3 * double(i + 1));
            dkH[size_t(i)] = kH;
            dadnH[size_t(i)] = rH * rippleH;
        }

        Table *tbl = createNumericTable(
                app, QStringLiteral("FatigueCrackGrowth"),
                QStringList() << QStringLiteral("DeltaK_Air") << QStringLiteral("dadN_Air")
                              << QStringLiteral("DeltaK_H") << QStringLiteral("dadN_H"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::X << SciDAVis::Y,
                { dkAir, dadnAir, dkH, dadnH });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        for (int k = 0; k < 2; ++k) {
            if (Column *cx = tbl->column(2 * k)) {
                cx->setLongName(QStringLiteral("Stress intensity factor range, \u0394<i>K</i>"));
                cx->setUnits(QStringLiteral("MPa m<sup>1/2</sup>"));
            }
            if (Column *cy = tbl->column(2 * k + 1)) {
                cy->setLongName(QStringLiteral("Crack growth rate, d<i>a</i>/d<i>N</i>"));
                cy->setUnits(QStringLiteral("m cycle<sup>-1</sup>"));
                cy->setComment(k == 0 ? QStringLiteral("Air (R = 0.1)")
                                      : QStringLiteral("H-charged (R = 0.1)"));
            }
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("ParisLawPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::LineSymbols, 0, N - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(0, 114, 178), 1.8));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(0, 114, 178)),
                                            QPen(QColor(0, 70, 120), 1.1), QSize(7, 7)));
            }
            if (g->insertCurve(tbl, tbl->colName(2), tbl->colName(3), Graph::LineSymbols, 0, N - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(213, 94, 0), 1.8));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Triangle, QBrush(QColor(213, 94, 0)),
                                            QPen(QColor(140, 55, 0), 1.1), QSize(8, 8)));
            }
            g->setXAxisTitle(QStringLiteral(
                    "Stress intensity factor range, \u0394<i>K</i> / MPa m<sup>1/2</sup>"));
            g->setYAxisTitle(QStringLiteral(
                    "Crack growth rate, d<i>a</i>/d<i>N</i> / m cycle<sup>-1</sup>"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 2.0, 100.0, 0.0, 5, 8, 1 /*Log10*/);
            g->setScale(QwtPlot::yLeft, 1.0e-10, 1.0e-4, 0.0, 6, 8, 1 /*Log10*/);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 11. Hall-Petch Grain Size Strengthening (Ferritic Steel & Al-Mg Alloy)
    if (key == QLatin1String("hall_petch_grain_size") || key == QLatin1String("hall_petch")) {
        const std::vector<double> dUm = { 120.0, 64.0, 36.0, 20.0, 11.0, 6.2, 3.2, 1.6 };
        const int N = static_cast<int>(dUm.size());
        std::vector<double> invSqrtD(N), sySteel(N), sdSteel(N), fitSteel(N);
        std::vector<double> syAl(N), sdAl(N), fitAl(N);
        // σy = σ0 + ky * d^(-1/2) (with d in μm -> d_m^(-1/2) = 1000 * d_um^(-1/2))
        constexpr double s0Steel = 76.0, kySteel = 620.0; // MPa * μm^(1/2) == 0.62 MPa * m^(1/2)
        constexpr double s0Al = 38.0, kyAl = 165.0;       // MPa * μm^(1/2) == 0.165 MPa * m^(1/2)
        const double jitter[8] = { -6.0, 5.0, -4.0, 8.0, -7.0, 6.0, -9.0, 7.0 };
        for (int i = 0; i < N; ++i) {
            const double x = 1.0 / std::sqrt(dUm[size_t(i)]); // μm^(-1/2)
            invSqrtD[size_t(i)] = x;
            fitSteel[size_t(i)] = s0Steel + kySteel * x;
            sySteel[size_t(i)] = fitSteel[size_t(i)] + jitter[i];
            sdSteel[size_t(i)] = 10.0 + 14.0 * x;

            fitAl[size_t(i)] = s0Al + kyAl * x;
            syAl[size_t(i)] = fitAl[size_t(i)] - 0.45 * jitter[i];
            sdAl[size_t(i)] = 5.0 + 6.0 * x;
        }

        Table *tbl = createNumericTable(
                app, QStringLiteral("HallPetchData"),
                QStringList() << QStringLiteral("GrainSize_um") << QStringLiteral("InvSqrtD")
                              << QStringLiteral("YieldSteel") << QStringLiteral("YieldSteelSD")
                              << QStringLiteral("FitSteel") << QStringLiteral("YieldAlMg")
                              << QStringLiteral("YieldAlMgSD") << QStringLiteral("FitAlMg"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::noDesignation << SciDAVis::X << SciDAVis::Y << SciDAVis::yErr
                        << SciDAVis::Y << SciDAVis::Y << SciDAVis::yErr << SciDAVis::Y,
                { dUm, invSqrtD, sySteel, sdSteel, fitSteel, syAl, sdAl, fitAl });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("Mean grain diameter, <i>d</i>"));
            c0->setUnits(QStringLiteral("\u03BCm"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(QStringLiteral("<i>d</i><sup>-1/2</sup>"));
            c1->setUnits(QStringLiteral("\u03BCm<sup>-1/2</sup>"));
        }
        if (Column *c2 = tbl->column(2)) {
            c2->setLongName(QStringLiteral("Yield stress, <i>\u03C3</i><sub>y</sub>"));
            c2->setUnits(QStringLiteral("MPa"));
            c2->setComment(QStringLiteral("Ferritic steel (measured)"));
        }
        if (Column *c4 = tbl->column(4)) {
            c4->setLongName(QStringLiteral("Hall-Petch fit"));
            c4->setUnits(QStringLiteral("MPa"));
            c4->setComment(QStringLiteral("Steel fit (k_y = 0.62 MPa m^1/2)"));
        }
        if (Column *c5 = tbl->column(5)) {
            c5->setLongName(QStringLiteral("Yield stress, <i>\u03C3</i><sub>y</sub>"));
            c5->setUnits(QStringLiteral("MPa"));
            c5->setComment(QStringLiteral("Al-Mg alloy (measured)"));
        }
        if (Column *c7 = tbl->column(7)) {
            c7->setLongName(QStringLiteral("Hall-Petch fit"));
            c7->setUnits(QStringLiteral("MPa"));
            c7->setComment(QStringLiteral("Al-Mg fit (k_y = 0.165 MPa m^1/2)"));
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("HallPetchPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            if (g->insertCurve(tbl, tbl->colName(1), tbl->colName(2), Graph::Scatter, 0, N - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(0, 114, 178), 1.5));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(0, 114, 178)),
                                            QPen(QColor(0, 70, 120), 1.2), QSize(8, 8)));
                g->addErrorBars(tbl->colName(2), tbl, tbl->colName(3), 1, 1, 6,
                                QColor(0, 114, 178), true, true, true);
            }
            insertStyledCurve(g, tbl, 1, 4, N, QPen(QColor(0, 114, 178), 1.9, Qt::DashLine));
            if (g->insertCurve(tbl, tbl->colName(1), tbl->colName(5), Graph::Scatter, 0, N - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(0, 158, 115), 1.5));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Rect, QBrush(QColor(0, 158, 115)),
                                            QPen(QColor(0, 100, 70), 1.2), QSize(8, 8)));
                g->addErrorBars(tbl->colName(5), tbl, tbl->colName(6), 1, 1, 6,
                                QColor(0, 158, 115), true, true, true);
            }
            insertStyledCurve(g, tbl, 1, 7, N, QPen(QColor(0, 158, 115), 1.9, Qt::DashLine));
            g->setXAxisTitle(QStringLiteral(
                    "Inverse square-root grain size, <i>d</i><sup>-1/2</sup> / \u03BCm<sup>-1/2</sup>"));
            g->setYAxisTitle(QStringLiteral("Yield stress, <i>\u03C3</i><sub>y</sub> / MPa"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 0.0, 0.90);
            g->setScale(QwtPlot::yLeft, 0.0, 650.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 12. High-Temperature Creep Curves of 9Cr-1Mo Steel (600 °C, 120/140/160 MPa)
    if (key == QLatin1String("creep_curve_9cr1mo") || key == QLatin1String("creep")) {
        constexpr int N = 161;
        auto synthCreep = [&](double tRuptureH, double eps0, double aPrim, double tauPrim,
                              double rateMin, double cTert, double pTert,
                              std::vector<double> &outT, std::vector<double> &outEps) {
            outT.resize(N);
            outEps.resize(N);
            for (int i = 0; i < N; ++i) {
                const double t = tRuptureH * double(i) / double(N - 1);
                const double u = t / tRuptureH;
                const double e = eps0 + aPrim * (1.0 - std::exp(-t / tauPrim)) + rateMin * t
                        + cTert * std::pow(u, pTert);
                outT[size_t(i)] = t;
                outEps[size_t(i)] = e;
            }
        };

        std::vector<double> t120, e120, t140, e140, t160, e160;
        synthCreep(980.0, 0.08, 0.65, 45.0, 0.0042, 7.8, 7.5, t120, e120);
        synthCreep(460.0, 0.11, 0.80, 26.0, 0.0115, 8.6, 6.8, t140, e140);
        synthCreep(195.0, 0.15, 0.95, 14.0, 0.0320, 9.5, 6.2, t160, e160);

        Table *tbl = createNumericTable(
                app, QStringLiteral("Creep9Cr1Mo"),
                QStringList() << QStringLiteral("Time_120MPa") << QStringLiteral("Strain_120MPa")
                              << QStringLiteral("Time_140MPa") << QStringLiteral("Strain_140MPa")
                              << QStringLiteral("Time_160MPa") << QStringLiteral("Strain_160MPa"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::X << SciDAVis::Y
                        << SciDAVis::X << SciDAVis::Y,
                { t120, e120, t140, e140, t160, e160 });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        const char *conds[3] = { "120 MPa (600 \u00B0C)", "140 MPa (600 \u00B0C)",
                                 "160 MPa (600 \u00B0C)" };
        for (int k = 0; k < 3; ++k) {
            if (Column *cx = tbl->column(2 * k)) {
                cx->setLongName(QStringLiteral("Time, <i>t</i>"));
                cx->setUnits(QStringLiteral("h"));
            }
            if (Column *cy = tbl->column(2 * k + 1)) {
                cy->setLongName(QStringLiteral("Creep strain, <i>\u03B5</i>"));
                cy->setUnits(QStringLiteral("%"));
                cy->setComment(QString::fromUtf8(conds[k]));
            }
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("Creep9Cr1MoPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            const QColor cols[3] = { QColor(0, 114, 178), QColor(0, 158, 115), QColor(213, 94, 0) };
            for (int k = 0; k < 3; ++k)
                insertStyledCurve(g, tbl, 2 * k, 2 * k + 1, N, QPen(cols[k], 2.1, Qt::SolidLine));
            g->setXAxisTitle(QStringLiteral("Time, <i>t</i> / h"));
            g->setYAxisTitle(QStringLiteral("Creep strain, <i>\u03B5</i> / %"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 0.0, 1050.0);
            g->setScale(QwtPlot::yLeft, 0.0, 18.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 13. Charpy Impact Toughness Transition Curve (DBTT of Ferritic Steel)
    if (key == QLatin1String("charpy_dbtt_curve") || key == QLatin1String("charpy")
        || key == QLatin1String("dbtt")) {
        const std::vector<double> tempC = { -120.0, -100.0, -80.0, -60.0, -40.0, -30.0, -20.0,
                                            -10.0,  0.0,    20.0,  40.0,  60.0,  80.0 };
        const int N = static_cast<int>(tempC.size());
        std::vector<double> vE(N), vESD(N), shearPct(N);
        constexpr double eLo = 9.5, eHi = 172.0, t0 = -24.0, width = 26.0;
        for (int i = 0; i < N; ++i) {
            const double th = std::tanh((tempC[size_t(i)] - t0) / width);
            const double baseE = 0.5 * (eHi + eLo) + 0.5 * (eHi - eLo) * th;
            const double ripple = 3.2 * std::sin(1.7 * double(i + 1));
            vE[size_t(i)] = std::max(4.0, baseE + ripple);
            // Larger scatter in transition zone
            const double sech2 = 1.0 - th * th;
            vESD[size_t(i)] = 3.5 + 11.0 * sech2;
            shearPct[size_t(i)] = std::clamp(50.0 * (1.0 + th), 0.0, 100.0);
        }

        Table *tbl = createNumericTable(
                app, QStringLiteral("CharpyDBTT"),
                QStringList() << QStringLiteral("TempC") << QStringLiteral("AbsorbedEnergy")
                              << QStringLiteral("EnergySD") << QStringLiteral("DuctileFracturePct"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::yErr << SciDAVis::noDesignation,
                { tempC, vE, vESD, shearPct });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("Test temperature, <i>T</i>"));
            c0->setUnits(QStringLiteral("\u00B0C"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(QStringLiteral("Charpy absorbed energy, <i>vE</i>"));
            c1->setUnits(QStringLiteral("J"));
            c1->setComment(QStringLiteral("Low-C Ferritic Steel (DBTT \u2248 -24 \u00B0C)"));
        }
        if (Column *c2 = tbl->column(2)) {
            c2->setLongName(QStringLiteral("SD"));
            c2->setUnits(QStringLiteral("J"));
        }
        if (Column *c3 = tbl->column(3)) {
            c3->setLongName(QStringLiteral("Ductile fracture surface ratio"));
            c3->setUnits(QStringLiteral("%"));
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("CharpyDBTTPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::LineSymbols, 0, N - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(0, 114, 178), 2.0));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(0, 114, 178)),
                                            QPen(QColor(0, 70, 120), 1.2), QSize(8, 8)));
                g->addErrorBars(tbl->colName(1), tbl, tbl->colName(2), 1, 1, 6,
                                QColor(0, 114, 178), true, true, true);
            }
            g->setXAxisTitle(QStringLiteral("Test temperature, <i>T</i> / \u00B0C"));
            g->setYAxisTitle(QStringLiteral("Charpy absorbed energy, <i>vE</i> / J"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, -130.0, 90.0);
            g->setScale(QwtPlot::yLeft, 0.0, 200.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 14. Isothermal Age-Hardening Curves of Al-Zn-Mg Alloy (Vickers HV, 120 vs. 160 °C)
    if (key == QLatin1String("vickers_age_hardening") || key == QLatin1String("age_hardening")) {
        const std::vector<double> timeH = { 0.1, 0.3, 1.0,  2.5,  6.0,  12.0,
                                            24.0, 48.0, 100.0, 240.0, 500.0, 1000.0 };
        const int N = static_cast<int>(timeH.size());
        std::vector<double> hv120(N), sd120(N), hv160(N), sd160(N);
        for (int i = 0; i < N; ++i) {
            const double lt = std::log10(timeH[size_t(i)]);
            // 120 °C: peak around 24 h (~186 HV)
            const double d120 = (lt - std::log10(26.0)) / 1.35;
            hv120[size_t(i)] = 92.0 + 95.0 * std::exp(-0.5 * d120 * d120)
                    + 1.8 * std::sin(2.1 * double(i + 1));
            sd120[size_t(i)] = 3.2;
            // 160 °C: faster kinetics, peak around 3 h (~168 HV), stronger over-aging drop
            const double d160 = (lt - std::log10(3.2)) / 1.12;
            hv160[size_t(i)] = 88.0 + 80.0 * std::exp(-0.5 * d160 * d160)
                    - (lt > 0.5 ? 6.5 * (lt - 0.5) : 0.0)
                    + 1.5 * std::cos(1.8 * double(i + 1));
            sd160[size_t(i)] = 3.4;
        }

        Table *tbl = createNumericTable(
                app, QStringLiteral("AgeHardeningHV"),
                QStringList() << QStringLiteral("AgingTime_h") << QStringLiteral("HV_120C")
                              << QStringLiteral("SD_120C") << QStringLiteral("HV_160C")
                              << QStringLiteral("SD_160C"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::yErr << SciDAVis::Y
                        << SciDAVis::yErr,
                { timeH, hv120, sd120, hv160, sd160 });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("Aging time, <i>t</i>"));
            c0->setUnits(QStringLiteral("h"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(QStringLiteral("Vickers hardness, HV"));
            c1->setUnits(QStringLiteral("HV"));
            c1->setComment(QStringLiteral("120 \u00B0C aging"));
        }
        if (Column *c3 = tbl->column(3)) {
            c3->setLongName(QStringLiteral("Vickers hardness, HV"));
            c3->setUnits(QStringLiteral("HV"));
            c3->setComment(QStringLiteral("160 \u00B0C aging"));
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("AgeHardeningPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::LineSymbols, 0, N - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(0, 114, 178), 2.0));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(0, 114, 178)),
                                            QPen(QColor(0, 70, 120), 1.2), QSize(8, 8)));
                g->addErrorBars(tbl->colName(1), tbl, tbl->colName(2), 1, 1, 6,
                                QColor(0, 114, 178), true, true, true);
            }
            if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(3), Graph::LineSymbols, 0, N - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(213, 94, 0), 2.0, Qt::DashLine));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Rect, QBrush(QColor(213, 94, 0)),
                                            QPen(QColor(140, 55, 0), 1.2), QSize(8, 8)));
                g->addErrorBars(tbl->colName(3), tbl, tbl->colName(4), 1, 1, 6,
                                QColor(213, 94, 0), true, true, true);
            }
            g->setXAxisTitle(QStringLiteral("Aging time, <i>t</i> / h"));
            g->setYAxisTitle(QStringLiteral("Vickers hardness / HV"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 0.08, 1500.0, 0.0, 5, 8, 1 /*Log10*/);
            g->setScale(QwtPlot::yLeft, 60.0, 210.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 15. Nanoindentation Load-Depth Curves (Oliver-Pharr: Pure Al vs. AA7075-T6)
    if (key == QLatin1String("nanoindentation_load_depth") || key == QLatin1String("nanoindentation")) {
        constexpr int nLoad = 80, nHold = 15, nUnload = 65;
        constexpr int N = nLoad + nHold + nUnload;
        auto synthNano = [&](double hMaxLoad, double hCreep, double hFinal, double pMax,
                             double mUnl, std::vector<double> &outH, std::vector<double> &outP) {
            outH.resize(N);
            outP.resize(N);
            const double hPeak = hMaxLoad + hCreep;
            for (int i = 0; i < nLoad; ++i) {
                const double u = double(i) / double(nLoad - 1);
                const double h = hMaxLoad * u;
                outH[size_t(i)] = h;
                outP[size_t(i)] = pMax * u * u; // Kick's law P = C h^2
            }
            for (int i = 0; i < nHold; ++i) {
                const double u = double(i + 1) / double(nHold);
                outH[size_t(nLoad + i)] = hMaxLoad + hCreep * (1.0 - std::exp(-3.2 * u));
                outP[size_t(nLoad + i)] = pMax;
            }
            for (int i = 0; i < nUnload; ++i) {
                const double u = double(i + 1) / double(nUnload);
                const double h = hPeak - (hPeak - hFinal) * u;
                const double frac = std::max(0.0, (h - hFinal) / std::max(1e-9, hPeak - hFinal));
                outH[size_t(nLoad + nHold + i)] = h;
                outP[size_t(nLoad + nHold + i)] = pMax * std::pow(frac, mUnl);
            }
        };

        std::vector<double> hAl, pAl, h7075, p7075;
        synthNano(420.0, 14.0, 398.0, 10.0, 1.38, hAl, pAl);       // Pure Al (soft, low elastic recovery)
        synthNano(225.0, 5.0, 182.0, 10.0, 1.42, h7075, p7075);    // AA7075-T6 (hard, higher elastic recovery)

        Table *tbl = createNumericTable(
                app, QStringLiteral("NanoindentationPH"),
                QStringList() << QStringLiteral("Depth_PureAl") << QStringLiteral("Load_PureAl")
                              << QStringLiteral("Depth_AA7075") << QStringLiteral("Load_AA7075"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::X << SciDAVis::Y,
                { hAl, pAl, h7075, p7075 });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        for (int k = 0; k < 2; ++k) {
            if (Column *cx = tbl->column(2 * k)) {
                cx->setLongName(QStringLiteral("Indentation depth, <i>h</i>"));
                cx->setUnits(QStringLiteral("nm"));
            }
            if (Column *cy = tbl->column(2 * k + 1)) {
                cy->setLongName(QStringLiteral("Load, <i>P</i>"));
                cy->setUnits(QStringLiteral("mN"));
                cy->setComment(k == 0 ? QStringLiteral("Pure Al (annealed)")
                                      : QStringLiteral("AA7075-T6 (peak aged)"));
            }
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml =
                app->newGraph(makeUniqueTableName(app, QStringLiteral("NanoindentationPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            insertStyledCurve(g, tbl, 0, 1, N, QPen(QColor(0, 158, 115), 2.1, Qt::SolidLine));
            insertStyledCurve(g, tbl, 2, 3, N, QPen(QColor(0, 114, 178), 2.1, Qt::SolidLine));
            g->setXAxisTitle(QStringLiteral("Indentation depth, <i>h</i> / nm"));
            g->setYAxisTitle(QStringLiteral("Load, <i>P</i> / mN"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 0.0, 460.0);
            g->setScale(QwtPlot::yLeft, 0.0, 11.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 16. Low-Cycle Fatigue Hysteresis Loops of SUS316L (Cycle 1 vs. Stabilized N=50)
    if (key == QLatin1String("cyclic_hysteresis_bauschinger") || key == QLatin1String("hysteresis")) {
        constexpr int nHalf = 100;
        constexpr int N = 2 * nHalf + 1;
        auto synthLoop = [&](double E_MPa, double kPrime, double nPrime, double epsAmpPct,
                             std::vector<double> &outEpsPct, std::vector<double> &outSig) {
            // Masing behavior: Δε/2 = Δσ/(2E) + (Δσ/(2K'))^(1/n')
            // Solve peak stress σ_a for given ε_a = epsAmpPct / 100
            const double epsA = 0.01 * epsAmpPct;
            auto halfStrain = [&](double sigA) {
                return sigA / E_MPa + std::pow(std::max(0.0, sigA) / kPrime, 1.0 / nPrime);
            };
            double lo = 1.0, hi = 1200.0;
            for (int it = 0; it < 60; ++it) {
                const double mid = 0.5 * (lo + hi);
                if (halfStrain(mid) < epsA)
                    lo = mid;
                else
                    hi = mid;
            }
            const double sigA = 0.5 * (lo + hi);
            outEpsPct.resize(N);
            outSig.resize(N);
            // Ascending branch: from (-epsA, -sigA) to (+epsA, +sigA)
            for (int i = 0; i <= nHalf; ++i) {
                const double dSig = 2.0 * sigA * double(i) / double(nHalf);
                const double dEps =
                        dSig / E_MPa + 2.0 * std::pow(std::max(0.0, dSig) / (2.0 * kPrime), 1.0 / nPrime);
                outEpsPct[size_t(i)] = 100.0 * (-epsA + dEps);
                outSig[size_t(i)] = -sigA + dSig;
            }
            // Descending branch: from (+epsA, +sigA) back to (-epsA, -sigA)
            for (int i = 1; i <= nHalf; ++i) {
                const double dSig = 2.0 * sigA * double(i) / double(nHalf);
                const double dEps =
                        dSig / E_MPa + 2.0 * std::pow(std::max(0.0, dSig) / (2.0 * kPrime), 1.0 / nPrime);
                outEpsPct[size_t(nHalf + i)] = 100.0 * (epsA - dEps);
                outSig[size_t(nHalf + i)] = sigA - dSig;
            }
        };

        std::vector<double> e1, s1, e50, s50;
        synthLoop(193000.0, 620.0, 0.16, 1.0, e1, s1);   // Cycle 1
        synthLoop(193000.0, 890.0, 0.145, 1.0, e50, s50); // Stabilized N=50 (cyclic hardening)

        Table *tbl = createNumericTable(
                app, QStringLiteral("SUS316LHysteresis"),
                QStringList() << QStringLiteral("Strain_N1") << QStringLiteral("Stress_N1")
                              << QStringLiteral("Strain_N50") << QStringLiteral("Stress_N50"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::X << SciDAVis::Y,
                { e1, s1, e50, s50 });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        for (int k = 0; k < 2; ++k) {
            if (Column *cx = tbl->column(2 * k)) {
                cx->setLongName(QStringLiteral("Total strain, <i>\u03B5</i>"));
                cx->setUnits(QStringLiteral("%"));
            }
            if (Column *cy = tbl->column(2 * k + 1)) {
                cy->setLongName(QStringLiteral("Stress, <i>\u03C3</i>"));
                cy->setUnits(QStringLiteral("MPa"));
                cy->setComment(k == 0 ? QStringLiteral("Cycle 1 (SUS316L)")
                                      : QStringLiteral("Stabilized N = 50 (SUS316L)"));
            }
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml =
                app->newGraph(makeUniqueTableName(app, QStringLiteral("SUS316LHysteresisPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            insertStyledCurve(g, tbl, 0, 1, N, QPen(QColor(0, 114, 178), 2.0, Qt::DashLine));
            insertStyledCurve(g, tbl, 2, 3, N, QPen(QColor(213, 94, 0), 2.2, Qt::SolidLine));
            g->setXAxisTitle(QStringLiteral("Total strain, <i>\u03B5</i> / %"));
            g->setYAxisTitle(QStringLiteral("Stress, <i>\u03C3</i> / MPa"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, -1.2, 1.2);
            g->setScale(QwtPlot::yLeft, -520.0, 520.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 17. Fracture Toughness J-R Resistance Curve (ASTM E1820)
    if (key == QLatin1String("fracture_toughness_jr_curve") || key == QLatin1String("jr_curve")) {
        const std::vector<double> daExp = { 0.08, 0.16, 0.24, 0.35, 0.48, 0.62, 0.80,
                                            1.02, 1.28, 1.55, 1.85, 2.20, 2.55 };
        const int N = static_cast<int>(daExp.size());
        constexpr double C1 = 265.0, C2 = 0.44; // J = C1 * (Δa)^C2 (kJ/m^2, Δa in mm)
        constexpr double sigFlow = 560.0;       // MPa -> 2*σY = 1120 MPa = 1120 kJ/m^2 per mm
        std::vector<double> jExp(N), jFit(N), daBlunt(N), jBlunt(N), daOff(N), jOff(N);
        for (int i = 0; i < N; ++i) {
            const double a = daExp[size_t(i)];
            jFit[size_t(i)] = C1 * std::pow(a, C2);
            jExp[size_t(i)] = jFit[size_t(i)] * (1.0 + 0.032 * std::sin(2.4 * double(i + 1)));
            // Construction lines sampled smoothly across [0, 0.42] and [0.2, 0.62] mm
            const double u = double(i) / double(N - 1);
            daBlunt[size_t(i)] = 0.42 * u;
            jBlunt[size_t(i)] = 2.0 * sigFlow * daBlunt[size_t(i)];
            daOff[size_t(i)] = 0.20 + 0.42 * u;
            jOff[size_t(i)] = 2.0 * sigFlow * (daOff[size_t(i)] - 0.20);
        }

        Table *tbl = createNumericTable(
                app, QStringLiteral("FractureJRCurve"),
                QStringList() << QStringLiteral("DeltaA_mm") << QStringLiteral("J_Measured")
                              << QStringLiteral("J_PowerFit") << QStringLiteral("DeltaA_Blunt")
                              << QStringLiteral("J_Blunting") << QStringLiteral("DeltaA_Offset02")
                              << QStringLiteral("J_Offset02"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::Y << SciDAVis::X
                        << SciDAVis::Y << SciDAVis::X << SciDAVis::Y,
                { daExp, jExp, jFit, daBlunt, jBlunt, daOff, jOff });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("Crack extension, \u0394<i>a</i>"));
            c0->setUnits(QStringLiteral("mm"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(QStringLiteral("<i>J</i>-integral"));
            c1->setUnits(QStringLiteral("kJ m<sup>-2</sup>"));
            c1->setComment(QStringLiteral("Measured (ASTM E1820)"));
        }
        if (Column *c2 = tbl->column(2)) {
            c2->setLongName(QStringLiteral("Power-law fit"));
            c2->setUnits(QStringLiteral("kJ m<sup>-2</sup>"));
            c2->setComment(QStringLiteral("Power-law fit"));
        }
        if (Column *c4 = tbl->column(4)) {
            c4->setLongName(QStringLiteral("Blunting line"));
            c4->setUnits(QStringLiteral("kJ m<sup>-2</sup>"));
            c4->setComment(QStringLiteral("Blunting line (2\u03C3_Y \u0394a)"));
        }
        if (Column *c6 = tbl->column(6)) {
            c6->setLongName(QStringLiteral("0.2 mm offset line"));
            c6->setUnits(QStringLiteral("kJ m<sup>-2</sup>"));
            c6->setComment(QStringLiteral("0.2 mm offset line"));
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("FractureJRPlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Scatter, 0, N - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(0, 114, 178), 1.5));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(0, 114, 178)),
                                            QPen(QColor(0, 70, 120), 1.2), QSize(8, 8)));
            }
            insertStyledCurve(g, tbl, 0, 2, N, QPen(QColor(0, 114, 178), 2.1, Qt::SolidLine));
            insertStyledCurve(g, tbl, 3, 4, N, QPen(QColor(90, 90, 90), 1.5, Qt::DotLine));
            insertStyledCurve(g, tbl, 5, 6, N, QPen(QColor(213, 94, 0), 1.8, Qt::DashLine));
            g->setXAxisTitle(QStringLiteral("Crack extension, \u0394<i>a</i> / mm"));
            g->setYAxisTitle(QStringLiteral("<i>J</i>-integral / kJ m<sup>-2</sup>"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 0.0, 2.8);
            g->setScale(QwtPlot::yLeft, 0.0, 480.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 18. Strain-Rate Dependent Flow Stress of Ti-6Al-4V (10^-3, 1, 10^3 s^-1)
    if (key == QLatin1String("strain_rate_flow_stress") || key == QLatin1String("strain_rate")) {
        constexpr int N = 161;
        std::vector<double> epsT(N), sigLow(N), sigMid(N), sigHigh(N);
        // Johnson-Cook parameters for Ti-6Al-4V: A=860 MPa, B=680 MPa, n=0.47, C=0.015
        constexpr double A = 860.0, B = 680.0, nJC = 0.47, C_jc = 0.015, epsDot0 = 1.0e-3;
        constexpr double rhoCp = 2.34e6; // J / (m^3 K)
        constexpr double betaTQ = 0.90;  // Taylor-Quinney factor
        constexpr double T0 = 298.0, Tm = 1933.0, mTherm = 1.0;

        auto evalJC = [&](double epsP, double epsDot, bool adiabatic) -> double {
            const double rateFac = 1.0 + C_jc * std::log(std::max(1.0, epsDot / epsDot0));
            const double baseSig = (A + B * std::pow(std::max(0.0, epsP), nJC)) * rateFac;
            if (!adiabatic)
                return baseSig;
            // Approximate adiabatic temperature rise ΔT ≈ β/(ρCp) * ∫ σ dε_p
            const double workJ = (A * epsP + B * std::pow(std::max(0.0, epsP), nJC + 1.0) / (nJC + 1.0))
                    * rateFac * 1.0e6;
            const double dT = (betaTQ * workJ) / rhoCp;
            const double tStar = std::clamp(dT / (Tm - T0), 0.0, 0.95);
            return baseSig * (1.0 - std::pow(tStar, mTherm));
        };

        for (int i = 0; i < N; ++i) {
            const double ep = 0.25 * double(i) / double(N - 1);
            epsT[size_t(i)] = ep;
            sigLow[size_t(i)] = evalJC(ep, 1.0e-3, false);
            sigMid[size_t(i)] = evalJC(ep, 1.0, false);
            sigHigh[size_t(i)] = evalJC(ep, 1.0e3, true);
        }

        Table *tbl = createNumericTable(
                app, QStringLiteral("Ti64StrainRateFlow"),
                QStringList() << QStringLiteral("TruePlasticStrain")
                              << QStringLiteral("Stress_1e_3") << QStringLiteral("Stress_1e0")
                              << QStringLiteral("Stress_1e3"),
                QList<SciDAVis::PlotDesignation>()
                        << SciDAVis::X << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y,
                { epsT, sigLow, sigMid, sigHigh });
        if (outTable)
            *outTable = tbl;
        if (!tbl)
            return nullptr;

        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("True plastic strain, <i>\u03B5</i><sub>p</sub>"));
            c0->setUnits(QStringLiteral("-"));
        }
        const char *rateLabels[3] = { "10^-3 s^-1 (Quasi-static)", "10^0 s^-1 (Intermediate)",
                                      "10^3 s^-1 (SHPB, adiabatic)" };
        for (int k = 0; k < 3; ++k) {
            if (Column *ck = tbl->column(k + 1)) {
                ck->setLongName(QStringLiteral("True flow stress, <i>\u03C3</i>"));
                ck->setUnits(QStringLiteral("MPa"));
                ck->setComment(QString::fromUtf8(rateLabels[k]));
            }
        }
        if (!autoPlot)
            return nullptr;

        MultiLayer *ml =
                app->newGraph(makeUniqueTableName(app, QStringLiteral("Ti64StrainRatePlot")));
        if (ml && ml->activeGraph()) {
            Graph *g = ml->activeGraph();
            const QColor cols[3] = { QColor(0, 114, 178), QColor(0, 158, 115), QColor(213, 94, 0) };
            for (int k = 0; k < 3; ++k)
                insertStyledCurve(g, tbl, 0, k + 1, N, QPen(cols[k], 2.1, Qt::SolidLine));
            g->setXAxisTitle(QStringLiteral("True plastic strain, <i>\u03B5</i><sub>p</sub>"));
            g->setYAxisTitle(QStringLiteral("True flow stress, <i>\u03C3</i> / MPa"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            g->setScale(QwtPlot::xBottom, 0.0, 0.26);
            g->setScale(QwtPlot::yLeft, 800.0, 1450.0);
            applyArialToMatSciGraph(g);
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            g->replot();
        }
        return ml;
    }

    // 19. Project Pack: Metal Mechanical Properties Evaluation Suite
    if (key == QLatin1String("project_metal_mechanics_suite")
        || key == QLatin1String("metal_mechanics_project")) {
        Table *tMain = nullptr;
        MultiLayer *mlMain =
                loadSampleDataset(app, QStringLiteral("tensile_temper_compare"), autoPlot, &tMain, nullptr);
        if (autoPlot && mlMain && mlMain->activeGraph() && tMain && tMain->numCols() > 3) {
            // Run 0.2% offset yield & Considère work-hardening analysis on T6 curve (column 3)
            const QString t6Curve = tMain->colName(3);
            analyzeTensile(app, mlMain->activeGraph(), t6Curve, 0.002, 1,
                           std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::quiet_NaN(), true, true, false);
            analyzeWorkHardening(app, mlMain->activeGraph(), t6Curve, 11, true);
        }
        loadSampleDataset(app, QStringLiteral("fatigue_sn_curve"), autoPlot, nullptr, nullptr);
        loadSampleDataset(app, QStringLiteral("fatigue_crack_growth_paris"), autoPlot, nullptr, nullptr);
        loadSampleDataset(app, QStringLiteral("hall_petch_grain_size"), autoPlot, nullptr, nullptr);
        loadSampleDataset(app, QStringLiteral("charpy_dbtt_curve"), autoPlot, nullptr, nullptr);
        if (outTable)
            *outTable = tMain;
        return mlMain;
    }

    // 20. Project Pack: Al-Zn-Mg Age-Hardening & Hydrogen Embrittlement Study
    if (key == QLatin1String("project_alznmg_he_research")
        || key == QLatin1String("alznmg_he_project")) {
        Table *tMain = nullptr;
        MultiLayer *mlMain = loadSampleDataset(
                app, QStringLiteral("tensile_hydrogen_embrittlement"), autoPlot, &tMain, nullptr);
        loadSampleDataset(app, QStringLiteral("vickers_age_hardening"), autoPlot, nullptr, nullptr);
        Table *tTds = nullptr;
        loadSampleDataset(app, QStringLiteral("tds_hydrogen_3rates"), autoPlot, &tTds, nullptr);
        if (autoPlot && tTds) {
            QVector<double> rates = { 100.0, 200.0, 400.0 };
            QVector<double> tPeaks;
            for (int c = 1; c <= 3 && c < tTds->numCols(); ++c) {
                double bestY = -1e300, bestT = 100.0;
                for (int r = 0; r < tTds->numRows(); ++r) {
                    double tv = 0.0, fv = 0.0;
                    if (readMatSciCell(tTds->column(0), r, tv)
                        && readMatSciCell(tTds->column(c), r, fv) && tv <= 260.0 && fv > bestY) {
                        bestY = fv;
                        bestT = tv;
                    }
                }
                tPeaks.append(bestT);
            }
            analyzeKissinger(app, rates, tPeaks, true /*K/h*/, true /*°C*/, true);
        }
        loadSampleDataset(app, QStringLiteral("xrd_aged_alloy"), autoPlot, nullptr, nullptr);
        Matrix *matCt = nullptr;
        loadSampleDataset(app, QStringLiteral("ct_local_strain_map"), autoPlot, nullptr, &matCt);
        if (outTable)
            *outTable = tMain;
        if (outMatrix)
            *outMatrix = matCt;
        return mlMain;
    }

    return nullptr;
}

void CurveMathTools::showSampleGalleryDialog(ApplicationWindow *app)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    registerMatSciTranslationsOnce();

    const QVector<SampleDatasetInfo> datasets = listSampleDatasets();
    if (datasets.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Materials Science Sample Gallery"));
    dlg.setMinimumSize(740, 520);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);

    QLabel *intro = new QLabel(
            QObject::tr("Select a built-in materials science dataset to generate a ready-to-analyze worksheet and publication plot:"),
            &dlg);
    intro->setWordWrap(true);
    vbox->addWidget(intro);

    // Category filter & keyword search row
    QHBoxLayout *filterRow = new QHBoxLayout();
    filterRow->addWidget(new QLabel(QObject::tr("Category:"), &dlg));
    QComboBox *cbCat = new QComboBox(&dlg);
    cbCat->addItem(QObject::tr("All Categories"), QString());
    QStringList seenCats;
    for (const SampleDatasetInfo &ds : datasets) {
        if (!seenCats.contains(ds.category)) {
            seenCats.append(ds.category);
            cbCat->addItem(ds.category, ds.category);
        }
    }
    filterRow->addWidget(cbCat, 1);

    filterRow->addWidget(new QLabel(QObject::tr("Filter:"), &dlg));
    QLineEdit *leSearch = new QLineEdit(&dlg);
    leSearch->setClearButtonEnabled(true);
    leSearch->setPlaceholderText(QObject::tr("Search by alloy, test type, or keyword..."));
    filterRow->addWidget(leSearch, 1);
    vbox->addLayout(filterRow);

    QListWidget *lw = new QListWidget(&dlg);
    vbox->addWidget(lw, 1);

    QLabel *descLbl = new QLabel(&dlg);
    descLbl->setWordWrap(true);
    descLbl->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    descLbl->setMargin(8);
    descLbl->setMinimumHeight(54);
    vbox->addWidget(descLbl);

    auto findDatasetById = [&](const QString &dsId) -> const SampleDatasetInfo * {
        for (const SampleDatasetInfo &ds : datasets) {
            if (ds.id == dsId)
                return &ds;
        }
        return nullptr;
    };

    auto updateDesc = [&]() {
        QListWidgetItem *cur = lw->currentItem();
        if (!cur) {
            descLbl->clear();
            return;
        }
        if (const SampleDatasetInfo *ds = findDatasetById(cur->data(Qt::UserRole).toString()))
            descLbl->setText(ds->description);
    };

    auto repopulate = [&]() {
        const QString wantCat = cbCat->currentData().toString();
        const QString query = leSearch->text().trimmed();
        const QString prevId =
                lw->currentItem() ? lw->currentItem()->data(Qt::UserRole).toString() : QString();
        lw->clear();
        int restoreRow = -1;
        for (const SampleDatasetInfo &ds : datasets) {
            if (!wantCat.isEmpty() && ds.category != wantCat)
                continue;
            if (!query.isEmpty()) {
                const bool hit = ds.title.contains(query, Qt::CaseInsensitive)
                        || ds.category.contains(query, Qt::CaseInsensitive)
                        || ds.description.contains(query, Qt::CaseInsensitive)
                        || ds.id.contains(query, Qt::CaseInsensitive);
                if (!hit)
                    continue;
            }
            QListWidgetItem *it = new QListWidgetItem(
                    QStringLiteral("[%1]  %2").arg(ds.category, ds.title), lw);
            it->setData(Qt::UserRole, ds.id);
            if (!prevId.isEmpty() && ds.id == prevId)
                restoreRow = lw->count() - 1;
        }
        if (lw->count() > 0)
            lw->setCurrentRow(restoreRow >= 0 ? restoreRow : 0);
        updateDesc();
    };
    repopulate();

    QObject::connect(lw, &QListWidget::currentRowChanged, &dlg, [&](int) { updateDesc(); });
    QObject::connect(cbCat, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { repopulate(); });
    QObject::connect(leSearch, &QLineEdit::textChanged, &dlg, [&](const QString &) { repopulate(); });

    int chosenAction = 0; // 1 = Load & Plot, 2 = Load, Plot & Analyze
    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *btnLoadPlot = new QPushButton(QObject::tr("Generate Worksheet && Plot"), &dlg);
    QPushButton *btnAnalyze =
            new QPushButton(QObject::tr("Generate && Run Recommended Analysis"), &dlg);
    QPushButton *btnCancel = new QPushButton(QObject::tr("Cancel"), &dlg);
    btnLoadPlot->setDefault(true);
    btnRow->addStretch(1);
    btnRow->addWidget(btnLoadPlot);
    btnRow->addWidget(btnAnalyze);
    btnRow->addWidget(btnCancel);
    vbox->addLayout(btnRow);

    QObject::connect(btnLoadPlot, &QPushButton::clicked, &dlg, [&]() {
        if (lw->currentItem()) {
            chosenAction = 1;
            dlg.accept();
        }
    });
    QObject::connect(btnAnalyze, &QPushButton::clicked, &dlg, [&]() {
        if (lw->currentItem()) {
            chosenAction = 2;
            dlg.accept();
        }
    });
    QObject::connect(lw, &QListWidget::itemDoubleClicked, &dlg, [&](QListWidgetItem *) {
        chosenAction = 1;
        dlg.accept();
    });
    QObject::connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted || chosenAction == 0 || !lw->currentItem())
        return;

    const QString id = lw->currentItem()->data(Qt::UserRole).toString();
    Table *tbl = nullptr;
    Matrix *mat = nullptr;
    const bool skipAutoForArrhenius =
            (chosenAction == 2 && id == QLatin1String("arrhenius_diffusion"));
    MultiLayer *ml = loadSampleDataset(app, id, !skipAutoForArrhenius, &tbl, &mat);

    if (chosenAction == 2) {
        if (id == QLatin1String("tensile_alznmg") && ml && ml->activeGraph()) {
            analyzeTensile(app, ml->activeGraph(), QString(), 0.002, 1,
                           std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::quiet_NaN(), true, true, false);
            analyzeWorkHardening(app, ml->activeGraph(), QString(), 11, true);
        } else if (id == QLatin1String("tensile_temper_compare") && ml && ml->activeGraph() && tbl
                   && tbl->numCols() > 3) {
            const QString t6Curve = tbl->colName(3);
            analyzeTensile(app, ml->activeGraph(), t6Curve, 0.002, 1,
                           std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::quiet_NaN(), true, true, false);
            analyzeWorkHardening(app, ml->activeGraph(), t6Curve, 11, true);
        } else if (id == QLatin1String("tensile_hydrogen_embrittlement") && ml && ml->activeGraph()
                   && tbl && tbl->numCols() > 1) {
            const QString unchCurve = tbl->colName(1);
            analyzeTensile(app, ml->activeGraph(), unchCurve, 0.002, 1,
                           std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::quiet_NaN(), true, true, false);
            analyzeWorkHardening(app, ml->activeGraph(), unchCurve, 11, true);
        } else if (id == QLatin1String("tds_hydrogen_3rates") && tbl) {
            QVector<double> rates = { 100.0, 200.0, 400.0 };
            QVector<double> tPeaks;
            for (int c = 1; c <= 3 && c < tbl->numCols(); ++c) {
                double bestY = -1e300, bestT = 100.0;
                for (int r = 0; r < tbl->numRows(); ++r) {
                    double tv = 0.0, fv = 0.0;
                    if (readMatSciCell(tbl->column(0), r, tv) && readMatSciCell(tbl->column(c), r, fv)
                        && tv <= 260.0 && fv > bestY) {
                        bestY = fv;
                        bestT = tv;
                    }
                }
                tPeaks.append(bestT);
            }
            analyzeKissinger(app, rates, tPeaks, true /*K/h*/, true /*°C*/, true);
        } else if (id == QLatin1String("xrd_aged_alloy") && ml && ml->activeGraph()) {
            analyzeXRD(app, ml->activeGraph(), QString(), 1.540598, 0.05, 0.90,
                       QStringLiteral("fcc"), false, true, true);
        } else if (id == QLatin1String("arrhenius_diffusion") && tbl) {
            createArrheniusPlot(app, tbl, QStringLiteral("TempK"), QStringLiteral("Diffusivity"),
                                false, true, false);
        }
    }
}

// ----------------------------------------------------------------------------
// 2. Kissinger TDS / DSC Activation Energy Analyzer
// ----------------------------------------------------------------------------

CurveMathTools::KissingerResult CurveMathTools::analyzeKissinger(
        ApplicationWindow *app, const QVector<double> &heatingRates,
        const QVector<double> &peakTemps, bool rateInKPerHour, bool tempInCelsius,
        bool topAxisInCelsius)
{
    registerMatSciTranslationsOnce();
    KissingerResult res;

    const int nIn = std::min(heatingRates.size(), peakTemps.size());
    struct KissPt {
        double inv1000T = 0.0;
        double kissY = 0.0;
        double rateKmin = 0.0;
        double tempK = 0.0;
    };
    std::vector<KissPt> pts;
    pts.reserve(static_cast<size_t>(std::max(0, nIn)));

    for (int i = 0; i < nIn; ++i) {
        const double rawRate = heatingRates[i];
        const double rawTemp = peakTemps[i];
        if (!std::isfinite(rawRate) || !std::isfinite(rawTemp) || rawRate <= 0.0)
            continue;
        const double rateKmin = rateInKPerHour ? (rawRate / 60.0) : rawRate;
        const double tK = tempInCelsius ? (rawTemp + 273.15) : rawTemp;
        if (!(rateKmin > 0.0) || !(tK > 0.0))
            continue;
        const double ratio = rateKmin / (tK * tK);
        if (!(ratio > 0.0) || !std::isfinite(ratio))
            continue;
        pts.push_back({ 1000.0 / tK, std::log(ratio), rateKmin, tK });
    }

    if (pts.size() < 2) {
        res.error = QObject::tr(
                "Kissinger analysis requires at least 2 valid (heating rate, peak temperature) pairs.");
        return res;
    }

    std::sort(pts.begin(), pts.end(),
              [](const KissPt &a, const KissPt &b) { return a.inv1000T < b.inv1000T; });

    const int n = static_cast<int>(pts.size());
    double sumX = 0.0, sumY = 0.0;
    for (const KissPt &p : pts) {
        sumX += p.inv1000T;
        sumY += p.kissY;
    }
    const double meanX = sumX / double(n);
    const double meanY = sumY / double(n);

    double sxx = 0.0, sxy = 0.0, syy = 0.0;
    for (const KissPt &p : pts) {
        const double dx = p.inv1000T - meanX;
        const double dy = p.kissY - meanY;
        sxx += dx * dx;
        sxy += dx * dy;
        syy += dy * dy;
    }
    if (sxx <= 1e-30) {
        res.error = QObject::tr(
                "Kissinger analysis requires at least 2 distinct peak temperatures.");
        return res;
    }

    constexpr double kGasR = 8.314462618;       // J / (mol K)
    constexpr double kBoltzEV = 8.617333262e-5; // eV / K

    const double slope = sxy / sxx;
    const double intercept = meanY - slope * meanX;
    double rss = 0.0;
    for (const KissPt &p : pts) {
        const double e = p.kissY - (intercept + slope * p.inv1000T);
        rss += e * e;
    }
    const double r2 = (syy > 1e-30) ? std::clamp(1.0 - rss / syy, 0.0, 1.0) : 1.0;
    const double sResid = (n > 2) ? std::sqrt(std::max(0.0, rss / double(n - 2))) : 0.0;
    const double slopeSE = (n > 2) ? (sResid / std::sqrt(sxx)) : 0.0;

    res.valid = true;
    res.slope = slope;
    res.intercept = intercept;
    res.rSquare = r2;
    res.activationEnergyKJmol = -slope * kGasR;
    res.activationEnergyKJmolSE = slopeSE * kGasR;
    res.activationEnergyEV = -slope * 1000.0 * kBoltzEV;
    res.activationEnergyEVSE = slopeSE * 1000.0 * kBoltzEV;
    // ln(beta_Ks / Tp^2) = (intercept - ln(60)) + slope * (1000 / Tp) = ln(A * R / Ed) - Ed / (R * Tp)
    // => A = (Ed_Jmol / R) * exp(intercept - ln(60)) = (-1000 * slope) * exp(intercept) / 60
    const double edOverR = std::max(0.0, -1000.0 * slope);
    res.preExponentialSInv =
            edOverR * std::exp(std::clamp(intercept - std::log(60.0), -500.0, 500.0));

    res.heatingRatesKmin.reserve(n);
    res.peakTempsK.reserve(n);
    for (const KissPt &p : pts) {
        res.heatingRatesKmin.append(p.rateKmin);
        res.peakTempsK.append(p.tempK);
    }

    if (!app)
        return res;

    const double tCrit = (n > 2) ? gsl_cdf_tdist_Pinv(0.975, double(n - 2)) : 0.0;
    const size_t nSz = static_cast<size_t>(n);
    std::vector<double> colInvT(nSz), colRate(nSz), colTpK(nSz), colTpC(nSz);
    std::vector<double> colKissY(nSz), colFitY(nSz), colCiLo(nSz), colCiHi(nSz);

    for (int i = 0; i < n; ++i) {
        const KissPt &p = pts[size_t(i)];
        const double yh = intercept + slope * p.inv1000T;
        const double hb = (n > 2)
                ? (tCrit * sResid
                   * std::sqrt(1.0 / double(n) + ((p.inv1000T - meanX) * (p.inv1000T - meanX)) / sxx))
                : 0.0;
        colInvT[size_t(i)] = p.inv1000T;
        colRate[size_t(i)] = p.rateKmin;
        colTpK[size_t(i)] = p.tempK;
        colTpC[size_t(i)] = p.tempK - 273.15;
        colKissY[size_t(i)] = p.kissY;
        colFitY[size_t(i)] = yh;
        colCiLo[size_t(i)] = yh - hb;
        colCiHi[size_t(i)] = yh + hb;
    }

    Table *tbl = createNumericTable(
            app, QStringLiteral("KissingerData"),
            QStringList() << QStringLiteral("Inv1000Tp") << QStringLiteral("HeatingRateKmin")
                          << QStringLiteral("PeakTempK") << QStringLiteral("PeakTempC")
                          << QStringLiteral("KissingerY") << QStringLiteral("LinearFit")
                          << QStringLiteral("CI95Lower") << QStringLiteral("CI95Upper"),
            QList<SciDAVis::PlotDesignation>()
                    << SciDAVis::X << SciDAVis::noDesignation << SciDAVis::noDesignation
                    << SciDAVis::noDesignation << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y
                    << SciDAVis::Y,
            { colInvT, colRate, colTpK, colTpC, colKissY, colFitY, colCiLo, colCiHi });
    res.resultTable = tbl;
    if (!tbl)
        return res;

    if (Column *c0 = tbl->column(0)) {
        c0->setLongName(QStringLiteral("1000 / <i>T</i><sub>p</sub>"));
        c0->setUnits(QStringLiteral("K<sup>-1</sup>"));
    }
    if (Column *c4 = tbl->column(4)) {
        c4->setLongName(QStringLiteral("ln(<i>\u03B2</i> / <i>T</i><sub>p</sub><sup>2</sup>)"));
        c4->setUnits(QStringLiteral("K<sup>-1</sup> min<sup>-1</sup>"));
        c4->setComment(QObject::tr("Measured"));
    }
    if (Column *c5 = tbl->column(5)) {
        c5->setLongName(QStringLiteral("ln(<i>\u03B2</i> / <i>T</i><sub>p</sub><sup>2</sup>)"));
        c5->setUnits(QStringLiteral("K<sup>-1</sup> min<sup>-1</sup>"));
        c5->setComment(QObject::tr("Linear fit"));
    }

    MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("KissingerPlot")));
    res.plot = ml;
    if (ml && ml->activeGraph()) {
        Graph *g = ml->activeGraph();
        if (n > 2 && sResid > 0.0) {
            QVector<double> vx(n), vyLo(n), vyHi(n);
            for (int i = 0; i < n; ++i) {
                vx[i] = colInvT[size_t(i)];
                vyLo[i] = colCiLo[size_t(i)];
                vyHi[i] = colCiHi[size_t(i)];
            }
            if (QwtPlotItem *band =
                        g->addShadedBand(vx, vyLo, vyHi, QColor(205, 35, 45, 38), QString())) {
                band->setZ(1.0);
                band->setItemAttribute(QwtPlotItem::Legend, false);
            }
        }
        if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(4), Graph::Scatter, 0, n - 1)) {
            const int idx = g->curves() - 1;
            g->setCurvePen(idx, QPen(QColor(25, 65, 135), 1.5));
            g->setCurveSymbol(idx,
                              QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(35, 95, 185)),
                                        QPen(QColor(15, 45, 95), 1.2), QSize(8, 8)));
        }
        insertStyledCurve(g, tbl, 0, 5, n, QPen(QColor(205, 35, 45), 2.1, Qt::SolidLine));

        g->applyFramePreset(Graph::FrameBoxInwardTicks);
        if (Plot *pw = g->plotWidget())
            pw->setAxisMaxMajor(QwtPlot::xBottom, 5);
        g->setXAxisTitle(
                QStringLiteral("Inverse peak temperature, 1000/<i>T</i><sub>p</sub> / K<sup>-1</sup>"));
        g->setYAxisTitle(QStringLiteral(
                "ln(<i>\u03B2</i> / <i>T</i><sub>p</sub><sup>2</sup>) / K<sup>-1</sup> min<sup>-1</sup>"));
        g->setTitle(QString());
        if (g->legend())
            g->legend()->setText(g->legendText());
        g->setAutoScale();
        applyArialToMatSciGraph(g);

        const QString topFormula = topAxisInCelsius ? QStringLiteral("1000.0/x - 273.15")
                                                    : QStringLiteral("1000.0/x");
        Graph *topLayer = ml->addLinkedLayer(g, true, false, topFormula, QStringLiteral("y"));
        if (topLayer) {
            topLayer->setAxisTitle(
                    QwtPlot::xTop,
                    topAxisInCelsius
                            ? QStringLiteral("Peak temperature, <i>T</i><sub>p</sub> / \u00B0C")
                            : QStringLiteral("Peak temperature, <i>T</i><sub>p</sub> / K"));
            applyArialToMatSciGraph(topLayer);
        }
        ml->arrangeLayers(false, false);
        g->syncChildLinkedLayers();
        g->replot();
        if (topLayer)
            topLayer->replot();
        ml->setActiveGraph(g);

        const double xSpan = std::max(1e-9, colInvT.back() - colInvT.front());
        const auto yMM = std::minmax_element(colKissY.begin(), colKissY.end());
        const double ySpan = std::max(1e-9, *yMM.second - *yMM.first);
        const QString summary = (n > 2)
                ? QStringLiteral("<i>E</i><sub>d</sub> = %1 \u00B1 %2 kJ mol<sup>-1</sup>\n"
                                 "(%3 eV), <i>A</i> = %4 s<sup>-1</sup>\n"
                                 "<i>R</i><sup>2</sup> = %5")
                          .arg(res.activationEnergyKJmol, 0, 'f', 2)
                          .arg(res.activationEnergyKJmolSE, 0, 'f', 2)
                          .arg(res.activationEnergyEV, 0, 'f', 3)
                          .arg(res.preExponentialSInv, 0, 'e', 2)
                          .arg(res.rSquare, 0, 'f', 4)
                : QStringLiteral("<i>E</i><sub>d</sub> = %1 kJ mol<sup>-1</sup> (%2 eV)\n"
                                 "<i>A</i> = %3 s<sup>-1</sup>")
                          .arg(res.activationEnergyKJmol, 0, 'f', 2)
                          .arg(res.activationEnergyEV, 0, 'f', 3)
                          .arg(res.preExponentialSInv, 0, 'e', 2);
        addMatSciCalloutBox(g, colInvT.front() + 0.05 * xSpan, *yMM.first + 0.25 * ySpan, summary);
        g->replot();
        if (topLayer)
            topLayer->replot();
    }
    return res;
}

void CurveMathTools::showKissingerDialog(ApplicationWindow *app)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    registerMatSciTranslationsOnce();

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

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Kissinger TDS / DSC Activation Energy Analyzer"));
    dlg.setMinimumWidth(540);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    QFormLayout *topForm = new QFormLayout();

    QComboBox *cbMode = new QComboBox(&dlg);
    if (!tables.isEmpty()) {
        cbMode->addItem(QObject::tr("From Worksheet (β, Tp Columns)"), 0);
        cbMode->addItem(QObject::tr("Auto-Extract Peaks from TDS Curves"), 1);
    }
    cbMode->addItem(QObject::tr("Manual Input (β, Tp Lists)"), 2);
    topForm->addRow(QObject::tr("Data Source:"), cbMode);

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *actTbl = findActiveOrFirstTable(app);
    for (int i = 0; i < tables.size(); ++i) {
        cbTable->addItem(tables[i]->name());
        if (tables[i] == actTbl)
            cbTable->setCurrentIndex(i);
    }
    QLabel *lblTable = new QLabel(QObject::tr("Source Worksheet:"), &dlg);
    topForm->addRow(lblTable, cbTable);

    // Mode 0 widgets
    QComboBox *cbRateCol = new QComboBox(&dlg);
    QComboBox *cbTpCol = new QComboBox(&dlg);
    QLabel *lblRateCol = new QLabel(QObject::tr("Heating Rate Column (β):"), &dlg);
    QLabel *lblTpCol = new QLabel(QObject::tr("Peak Temperature Column (Tp):"), &dlg);
    topForm->addRow(lblRateCol, cbRateCol);
    topForm->addRow(lblTpCol, cbTpCol);

    // Mode 1 widgets (Spectra Peak Extraction)
    QComboBox *cbSpecTempCol = new QComboBox(&dlg);
    QListWidget *lwFluxCols = new QListWidget(&dlg);
    lwFluxCols->setSelectionMode(QAbstractItemView::MultiSelection);
    lwFluxCols->setMaximumHeight(110);
    QLineEdit *leSpecRates = new QLineEdit(QStringLiteral("100, 200, 400"), &dlg);
    QDoubleSpinBox *sbWinMin = new QDoubleSpinBox(&dlg);
    sbWinMin->setRange(-273.15, 3000.0);
    sbWinMin->setValue(30.0);
    QDoubleSpinBox *sbWinMax = new QDoubleSpinBox(&dlg);
    sbWinMax->setRange(-273.15, 3000.0);
    sbWinMax->setValue(280.0);
    QLabel *lblSpecTemp = new QLabel(QObject::tr("Temperature Column:"), &dlg);
    QLabel *lblFluxCols = new QLabel(QObject::tr("TDS Flux Columns (Select 2+ Rates):"), &dlg);
    QLabel *lblSpecRates = new QLabel(QObject::tr("Heating Rates (comma-separated):"), &dlg);
    QLabel *lblWinMin = new QLabel(QObject::tr("Peak Search Window Min T:"), &dlg);
    QLabel *lblWinMax = new QLabel(QObject::tr("Peak Search Window Max T:"), &dlg);
    topForm->addRow(lblSpecTemp, cbSpecTempCol);
    topForm->addRow(lblFluxCols, lwFluxCols);
    topForm->addRow(lblSpecRates, leSpecRates);
    topForm->addRow(lblWinMin, sbWinMin);
    topForm->addRow(lblWinMax, sbWinMax);

    // Mode 2 widgets (Manual)
    QLineEdit *leManualRates = new QLineEdit(QStringLiteral("100, 200, 400"), &dlg);
    QLineEdit *leManualTemps = new QLineEdit(QStringLiteral("119.5, 134.8, 151.6"), &dlg);
    QLabel *lblManRates = new QLabel(QObject::tr("Heating Rates (comma-separated):"), &dlg);
    QLabel *lblManTemps = new QLabel(QObject::tr("Peak Temperatures (comma-separated):"), &dlg);
    topForm->addRow(lblManRates, leManualRates);
    topForm->addRow(lblManTemps, leManualTemps);

    QComboBox *cbRateUnit = new QComboBox(&dlg);
    cbRateUnit->addItem(QObject::tr("K/min (°C/min)"), false);
    cbRateUnit->addItem(QObject::tr("K/h (°C/h)"), true);

    QCheckBox *chkTempC = new QCheckBox(QObject::tr("Peak Temperatures in Celsius (°C)"), &dlg);
    chkTempC->setChecked(true);
    QCheckBox *chkTopC = new QCheckBox(QObject::tr("Top Temperature Axis in Celsius (°C)"), &dlg);
    chkTopC->setChecked(true);

    topForm->addRow(QObject::tr("Heating Rate Unit:"), cbRateUnit);
    topForm->addRow(QString(), chkTempC);
    topForm->addRow(QString(), chkTopC);
    vbox->addLayout(topForm);

    auto refreshCols = [&]() {
        cbRateCol->clear();
        cbTpCol->clear();
        cbSpecTempCol->clear();
        lwFluxCols->clear();
        const int tIdx = cbTable->currentIndex();
        if (tIdx < 0 || tIdx >= tables.size())
            return;
        Table *t = tables[tIdx];
        QStringList parsedRates;
        static const QRegularExpression kNumRe(QStringLiteral("([0-9]+(?:\\.[0-9]+)?)"));
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!matSciColumnHasNumeric(col))
                continue;
            const QString cn = col->name();
            cbRateCol->addItem(cn);
            cbTpCol->addItem(cn);
            cbSpecTempCol->addItem(cn);
            if (c >= 1) {
                auto *it = new QListWidgetItem(cn, lwFluxCols);
                it->setSelected(true);
                const QString meta = col->comment() + QLatin1Char(' ') + cn;
                const auto m = kNumRe.match(meta);
                if (m.hasMatch())
                    parsedRates << m.captured(1);
                if (meta.contains(QLatin1String("/h"), Qt::CaseInsensitive)
                    || meta.contains(QLatin1String("Ch"), Qt::CaseInsensitive)) {
                    cbRateUnit->setCurrentIndex(1);
                }
            }
        }
        if (cbTpCol->count() >= 2)
            cbTpCol->setCurrentIndex(1);
        if (parsedRates.size() >= 2)
            leSpecRates->setText(parsedRates.join(QStringLiteral(", ")));
        if (t->numCols() >= 4)
            cbMode->setCurrentIndex(1);
    };
    if (!tables.isEmpty())
        refreshCols();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refreshCols(); });

    auto syncVis = [&]() {
        const int mode = cbMode->currentData().toInt();
        lblTable->setVisible(mode != 2);
        cbTable->setVisible(mode != 2);
        lblRateCol->setVisible(mode == 0);
        cbRateCol->setVisible(mode == 0);
        lblTpCol->setVisible(mode == 0);
        cbTpCol->setVisible(mode == 0);
        lblSpecTemp->setVisible(mode == 1);
        cbSpecTempCol->setVisible(mode == 1);
        lblFluxCols->setVisible(mode == 1);
        lwFluxCols->setVisible(mode == 1);
        lblSpecRates->setVisible(mode == 1);
        leSpecRates->setVisible(mode == 1);
        lblWinMin->setVisible(mode == 1);
        sbWinMin->setVisible(mode == 1);
        lblWinMax->setVisible(mode == 1);
        sbWinMax->setVisible(mode == 1);
        lblManRates->setVisible(mode == 2);
        leManualRates->setVisible(mode == 2);
        lblManTemps->setVisible(mode == 2);
        leManualTemps->setVisible(mode == 2);
    };
    syncVis();
    QObject::connect(cbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { syncVis(); });

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    auto parseCsvDoubles = [](const QString &s) -> QVector<double> {
        QVector<double> out;
        for (const QString &tok : s.split(QRegularExpression(QStringLiteral("[,;\\s]+")),
                                          Qt::SkipEmptyParts)) {
            bool ok = false;
            const double v = tok.toDouble(&ok);
            if (ok && std::isfinite(v))
                out.append(v);
        }
        return out;
    };

    QVector<double> rates, peakTemps;
    const int mode = cbMode->currentData().toInt();
    if (mode == 0 && cbTable->currentIndex() >= 0) {
        Table *t = tables[cbTable->currentIndex()];
        Column *cR = resolveMatSciColumn(t, cbRateCol->currentText());
        Column *cT = resolveMatSciColumn(t, cbTpCol->currentText());
        if (cR && cT) {
            const int nR = std::min(cR->rowCount(), cT->rowCount());
            for (int r = 0; r < nR; ++r) {
                double rv = 0.0, tv = 0.0;
                if (readMatSciCell(cR, r, rv) && readMatSciCell(cT, r, tv)) {
                    rates.append(rv);
                    peakTemps.append(tv);
                }
            }
        }
    } else if (mode == 1 && cbTable->currentIndex() >= 0) {
        Table *t = tables[cbTable->currentIndex()];
        Column *cTemp = resolveMatSciColumn(t, cbSpecTempCol->currentText());
        const QVector<double> inRates = parseCsvDoubles(leSpecRates->text());
        const QList<QListWidgetItem *> selItems = lwFluxCols->selectedItems();
        const double wLo = std::min(sbWinMin->value(), sbWinMax->value());
        const double wHi = std::max(sbWinMin->value(), sbWinMax->value());
        const int kCnt = std::min(inRates.size(), selItems.size());
        for (int k = 0; k < kCnt; ++k) {
            Column *cF = resolveMatSciColumn(t, selItems[k]->text());
            if (!cTemp || !cF)
                continue;
            int bestIdx = -1;
            double bestVal = -std::numeric_limits<double>::infinity();
            const int nR = std::min(cTemp->rowCount(), cF->rowCount());
            for (int r = 0; r < nR; ++r) {
                double tv = 0.0, fv = 0.0;
                if (readMatSciCell(cTemp, r, tv) && readMatSciCell(cF, r, fv) && tv >= wLo
                    && tv <= wHi && fv > bestVal) {
                    bestVal = fv;
                    bestIdx = r;
                }
            }
            if (bestIdx >= 0) {
                double tp = 0.0;
                readMatSciCell(cTemp, bestIdx, tp);
                // Parabolic 3-point sub-grid peak refinement
                if (bestIdx > 0 && bestIdx + 1 < nR) {
                    double t0 = 0.0, t1 = tp, t2 = 0.0, y0 = 0.0, y1 = bestVal, y2 = 0.0;
                    if (readMatSciCell(cTemp, bestIdx - 1, t0)
                        && readMatSciCell(cTemp, bestIdx + 1, t2)
                        && readMatSciCell(cF, bestIdx - 1, y0)
                        && readMatSciCell(cF, bestIdx + 1, y2)) {
                        const double denom = 2.0 * (y0 - 2.0 * y1 + y2);
                        if (std::fabs(denom) > 1e-30 && y0 <= y1 && y2 <= y1) {
                            const double delta = std::clamp((y0 - y2) / denom, -0.5, 0.5);
                            tp = t1 + delta * 0.5 * (t2 - t0);
                        }
                    }
                }
                rates.append(inRates[k]);
                peakTemps.append(tp);
            }
        }
    } else {
        rates = parseCsvDoubles(leManualRates->text());
        peakTemps = parseCsvDoubles(leManualTemps->text());
    }

    const KissingerResult kr = analyzeKissinger(app, rates, peakTemps,
                                                cbRateUnit->currentData().toBool(),
                                                chkTempC->isChecked(), chkTopC->isChecked());
    if (!kr.valid && !ApplicationWindow::isNonInteractiveContext(app)) {
        QMessageBox::warning(app,
                             QObject::tr("Kissinger TDS / DSC Activation Energy Analyzer"),
                             kr.error);
    }
}

// ----------------------------------------------------------------------------
// 3. Kocks-Mecking Work-Hardening Rate & Considere Necking Analyzer
// ----------------------------------------------------------------------------

CurveMathTools::WorkHardeningResult CurveMathTools::analyzeWorkHardening(
        ApplicationWindow *app, Graph *g, const QString &curveTitle, int smoothWindow,
        bool createPlot, bool kocksMeckingPlot)
{
    registerMatSciTranslationsOnce();
    WorkHardeningResult res;

    Graph *targetGraph = findActiveOrFirstGraph(app, g);
    if (!targetGraph) {
        res.error = QObject::tr("No active graph found for tensile analysis.");
        return res;
    }

    const TensileResult tr = analyzeTensile(
            app, targetGraph, curveTitle, 0.002, -1,
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
            false /*plotConstruction*/, false /*addCallout*/, false /*createTrueStressTable*/);
    if (!tr.valid || !(tr.youngsModulus > 0.0)) {
        res.error = tr.error.isEmpty()
                ? QObject::tr("Could not determine a positive linear-elastic slope in the specified range.")
                : tr.error;
        return res;
    }

    QwtPlotCurve *srcCurve = curveTitle.trimmed().isEmpty()
            ? nullptr
            : resolveCurve(targetGraph, curveTitle.trimmed());
    if (!srcCurve) {
        const QStringList cList = targetGraph->analysableCurvesList();
        if (!cList.isEmpty())
            srcCurve = resolveCurve(targetGraph, cList.first());
    }
    if (!srcCurve || srcCurve->dataSize() < 5) {
        res.error = QObject::tr("At least 4 valid data points are required for tensile analysis.");
        return res;
    }

    // Collect engineering (eFrac, stress) along curve order up to post-UTS diffuse necking
    const auto *dc = dynamic_cast<const DataCurve *>(srcCurve);
    Table *srcTbl = dc ? dc->table() : nullptr;
    Column *xCol = (srcTbl && dc) ? srcTbl->column(dc->xColumnName()) : nullptr;
    Column *yCol = (srcTbl && dc) ? srcTbl->column(dc->yColumnName()) : nullptr;

    struct TrueSample {
        double epsT = 0.0;
        double sigT = 0.0;
        double epsP = 0.0;
    };
    std::vector<TrueSample> samples;
    const int nRaw = srcCurve->dataSize();
    samples.reserve(static_cast<size_t>(nRaw));

    double maxEngY = -1e300;
    size_t utsRawIdx = 0;
    std::vector<std::pair<double, double>> engPts;
    engPts.reserve(static_cast<size_t>(nRaw));
    for (int i = 0; i < nRaw; ++i) {
        if (dc && srcTbl) {
            int row = dc->tableRow(i);
            if (row < 0 && yCol && nRaw == yCol->rowCount())
                row = i;
            if (row >= 0 && ((xCol && xCol->isMasked(row)) || (yCol && yCol->isMasked(row))))
                continue;
        }
        const double xv = srcCurve->x(i);
        const double yv = srcCurve->y(i);
        if (!std::isfinite(xv) || !std::isfinite(yv))
            continue;
        if (yv > maxEngY) {
            maxEngY = yv;
            utsRawIdx = engPts.size();
        }
        engPts.emplace_back(xv, yv);
    }

    const size_t postAllowance = std::max<size_t>(8, engPts.size() / 6);
    const size_t stopIdx = std::min(engPts.size(), utsRawIdx + postAllowance);
    double lastEpsT = -1e300;
    for (size_t i = 0; i < stopIdx; ++i) {
        const double eFrac = tr.strainInPercent ? (0.01 * engPts[i].first) : engPts[i].first;
        const double sigEng = engPts[i].second;
        if (1.0 + eFrac <= 1e-6 || sigEng < -0.05 * maxEngY)
            continue;
        if (i > utsRawIdx && sigEng < 0.55 * maxEngY)
            break;
        const double epsT = std::log(1.0 + eFrac);
        const double sigT = sigEng * (1.0 + eFrac);
        if (epsT <= lastEpsT + 1e-7)
            continue;
        const double epsP = std::max(0.0, epsT - sigT / tr.youngsModulus);
        samples.push_back({ epsT, sigT, epsP });
        lastEpsT = epsT;
    }

    const int N = static_cast<int>(samples.size());
    if (N < 5) {
        res.error = QObject::tr("At least 4 valid data points are required for tensile analysis.");
        return res;
    }

    // Local quadratic Savitzky-Golay derivative on actual non-uniform Δε_t grid
    int win = (smoothWindow <= 0) ? 11 : smoothWindow;
    if (win % 2 == 0)
        ++win;
    win = std::clamp(win, 5, std::min(51, (N % 2 == 1) ? N : (N - 1)));
    const int halfW = std::max(2, (win - 1) / 2);

    std::vector<double> theta(static_cast<size_t>(N), 0.0);
    for (int i = 0; i < N; ++i) {
        const int iLo = std::max(0, i - halfW);
        const int iHi = std::min(N - 1, i + halfW);
        const double x0 = samples[size_t(i)].epsT;
        // Normal equations for y(u) = c0 + c1*u + c2*u^2 where u = epsT - x0
        double s0 = 0.0, s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0;
        double sy0 = 0.0, sy1 = 0.0, sy2 = 0.0;
        for (int k = iLo; k <= iHi; ++k) {
            const double u = samples[size_t(k)].epsT - x0;
            const double y = samples[size_t(k)].sigT;
            const double u2 = u * u;
            s0 += 1.0;
            s1 += u;
            s2 += u2;
            s3 += u2 * u;
            s4 += u2 * u2;
            sy0 += y;
            sy1 += u * y;
            sy2 += u2 * y;
        }
        // Solve 3x3 linear system via Gaussian elimination with partial pivoting
        double aug[3][4] = {
            { s0, s1, s2, sy0 },
            { s1, s2, s3, sy1 },
            { s2, s3, s4, sy2 }
        };
        bool ok3 = true;
        for (int p = 0; p < 3; ++p) {
            int bestR = p;
            for (int r = p + 1; r < 3; ++r) {
                if (std::fabs(aug[r][p]) > std::fabs(aug[bestR][p]))
                    bestR = r;
            }
            if (std::fabs(aug[bestR][p]) <= 1e-30) {
                ok3 = false;
                break;
            }
            if (bestR != p) {
                for (int c = p; c < 4; ++c)
                    std::swap(aug[p][c], aug[bestR][c]);
            }
            const double piv = aug[p][p];
            for (int c = p; c < 4; ++c)
                aug[p][c] /= piv;
            for (int r = 0; r < 3; ++r) {
                if (r == p)
                    continue;
                const double f = aug[r][p];
                for (int c = p; c < 4; ++c)
                    aug[r][c] -= f * aug[p][c];
            }
        }
        if (ok3 && std::isfinite(aug[1][3])) {
            theta[size_t(i)] = aug[1][3];
        } else {
            const int l = std::max(0, i - 1);
            const int r = std::min(N - 1, i + 1);
            const double dx = samples[size_t(r)].epsT - samples[size_t(l)].epsT;
            theta[size_t(i)] = (dx > 1e-12)
                    ? ((samples[size_t(r)].sigT - samples[size_t(l)].sigT) / dx)
                    : 0.0;
        }
    }

    const double eYieldFrac = tr.strainInPercent ? (0.01 * tr.offsetYieldX) : tr.offsetYieldX;
    const double epsYield = std::log(std::max(1e-6, 1.0 + eYieldFrac));
    int yieldIdx = 0;
    while (yieldIdx + 1 < N && samples[size_t(yieldIdx)].epsT < epsYield)
        ++yieldIdx;

    // Solve Considere criterion: g(epsT) = theta(epsT) - sigT(epsT) = 0 for i >= yieldIdx
    bool foundConsidere = false;
    int neckIdx = N - 1;
    for (int i = yieldIdx; i + 1 < N; ++i) {
        const double g0 = theta[size_t(i)] - samples[size_t(i)].sigT;
        const double g1 = theta[size_t(i + 1)] - samples[size_t(i + 1)].sigT;
        if (g0 >= 0.0 && g1 <= 0.0) {
            const double denom = g0 - g1;
            const double t = (std::fabs(denom) > 1e-30) ? std::clamp(g0 / denom, 0.0, 1.0) : 0.0;
            res.considereTrueStrain = samples[size_t(i)].epsT
                    + t * (samples[size_t(i + 1)].epsT - samples[size_t(i)].epsT);
            res.considereTrueStress = samples[size_t(i)].sigT
                    + t * (samples[size_t(i + 1)].sigT - samples[size_t(i)].sigT);
            neckIdx = i + 1;
            foundConsidere = true;
            break;
        }
    }
    if (!foundConsidere) {
        const double eUtsFrac = tr.strainInPercent ? (0.01 * tr.utsX) : tr.utsX;
        res.considereTrueStrain = std::log(std::max(1e-6, 1.0 + eUtsFrac));
        res.considereTrueStress = tr.utsY * (1.0 + eUtsFrac);
    }
    res.considereEngStrainPct = (std::exp(res.considereTrueStrain) - 1.0) * 100.0;

    // Hollomon power-law fit: ln(sigT) = ln(K) + n * ln(epsP) over uniform plastic interval
    {
        std::vector<double> u, v;
        for (int i = yieldIdx; i <= neckIdx && i < N; ++i) {
            const double ep = samples[size_t(i)].epsP;
            const double st = samples[size_t(i)].sigT;
            if (ep > 2.5e-3 && samples[size_t(i)].epsT <= res.considereTrueStrain + 1e-6 && st > 0.0) {
                u.push_back(std::log(ep));
                v.push_back(std::log(st));
            }
        }
        if (u.size() >= 2) {
            const double mu = std::accumulate(u.begin(), u.end(), 0.0) / double(u.size());
            const double mv = std::accumulate(v.begin(), v.end(), 0.0) / double(v.size());
            double suu = 0.0, suv = 0.0;
            for (size_t k = 0; k < u.size(); ++k) {
                suu += (u[k] - mu) * (u[k] - mu);
                suv += (u[k] - mu) * (v[k] - mv);
            }
            if (suu > 1e-24) {
                res.hollomonN = std::max(0.0, suv / suu);
                res.hollomonK = std::exp(std::clamp(mv - res.hollomonN * mu, -50.0, 50.0));
            }
        }
    }

    // Stage-III Kocks-Mecking fit: theta = theta0 * (1 - sigT / voceSigmaS)
    {
        const double sigStart = tr.offsetYieldY + 0.20 * (res.considereTrueStress - tr.offsetYieldY);
        const double sigEnd = 0.98 * res.considereTrueStress;
        std::vector<double> sVec, thVec;
        for (int i = yieldIdx; i <= neckIdx && i < N; ++i) {
            const double st = samples[size_t(i)].sigT;
            const double th = theta[size_t(i)];
            if (st >= sigStart && st <= sigEnd && th > 0.0 && th < 0.5 * tr.youngsModulus) {
                sVec.push_back(st);
                thVec.push_back(th);
            }
        }
        if (sVec.size() >= 2) {
            const double ms = std::accumulate(sVec.begin(), sVec.end(), 0.0) / double(sVec.size());
            const double mt = std::accumulate(thVec.begin(), thVec.end(), 0.0) / double(thVec.size());
            double sss = 0.0, sst = 0.0;
            for (size_t k = 0; k < sVec.size(); ++k) {
                sss += (sVec[k] - ms) * (sVec[k] - ms);
                sst += (sVec[k] - ms) * (thVec[k] - mt);
            }
            if (sss > 1e-18) {
                const double slopeKM = sst / sss;
                const double theta0 = mt - slopeKM * ms;
                if (slopeKM < -1e-6 && theta0 > 0.0)
                    res.voceSigmaS = -theta0 / slopeKM;
            }
        }
        if (!(res.voceSigmaS > 0.0) || !std::isfinite(res.voceSigmaS))
            res.voceSigmaS = 1.08 * res.considereTrueStress;
    }

    res.valid = true;
    if (!app)
        return res;

    const size_t nSz = static_cast<size_t>(N);
    std::vector<double> colEpsT(nSz), colSigT(nSz), colThetaAll(nSz), colEpsP(nSz);
    for (int i = 0; i < N; ++i) {
        colEpsT[size_t(i)] = samples[size_t(i)].epsT;
        colSigT[size_t(i)] = samples[size_t(i)].sigT;
        colThetaAll[size_t(i)] = theta[size_t(i)];
        colEpsP[size_t(i)] = samples[size_t(i)].epsP;
    }

    Table *tbl = createNumericTable(
            app, QStringLiteral("WorkHardening"),
            QStringList() << QStringLiteral("TrueStrain") << QStringLiteral("TrueStress")
                          << QStringLiteral("WorkHardeningRate")
                          << QStringLiteral("TruePlasticStrain")
                          << QStringLiteral("ConsidereLine"),
            QList<SciDAVis::PlotDesignation>()
                    << SciDAVis::X << (kocksMeckingPlot ? SciDAVis::X : SciDAVis::Y) << SciDAVis::Y
                    << SciDAVis::noDesignation << SciDAVis::Y,
            { colEpsT, colSigT, colThetaAll, colEpsP, colSigT });
    res.resultTable = tbl;
    if (tbl) {
        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("True strain, <i>\u03B5</i><sub>t</sub>"));
            c0->setUnits(QStringLiteral("-"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(kocksMeckingPlot
                                    ? QStringLiteral("True stress, <i>\u03C3</i><sub>t</sub>")
                                    : QStringLiteral("<i>\u03C3</i><sub>t</sub>"));
            c1->setUnits(tr.stressUnit);
            c1->setComment(QStringLiteral("<i>\u03C3</i><sub>t</sub>"));
        }
        if (Column *c2 = tbl->column(2)) {
            c2->setLongName(
                    QStringLiteral("<i>\u03B8</i> = d<i>\u03C3</i><sub>t</sub>/d<i>\u03B5</i><sub>t</sub>"));
            c2->setUnits(tr.stressUnit);
            c2->setComment(
                    QStringLiteral("<i>\u03B8</i> = d<i>\u03C3</i><sub>t</sub>/d<i>\u03B5</i><sub>t</sub>"));
        }
        if (Column *c4 = tbl->column(4)) {
            c4->setLongName(
                    QStringLiteral("<i>\u03B8</i> = d<i>\u03C3</i><sub>t</sub>/d<i>\u03B5</i><sub>t</sub>"));
            c4->setUnits(tr.stressUnit);
            c4->setComment(QStringLiteral("<i>\u03B8</i> = <i>\u03C3</i><sub>t</sub>"));
        }
    }

    if (!createPlot || !tbl)
        return res;

    // Find start row of plastic hardening branch where theta <= 2.6 * considereTrueStress so elastic E doesn't blow up Y scale
    int thetaPlotStart = yieldIdx;
    const double yCeil = 2.5 * res.considereTrueStress;
    while (thetaPlotStart + 2 < N && theta[size_t(thetaPlotStart)] > yCeil)
        ++thetaPlotStart;

    const QString plotBaseName = kocksMeckingPlot ? QStringLiteral("KocksMeckingPlot")
                                                  : QStringLiteral("ConsiderePlot");
    MultiLayer *ml = app->newGraph(makeUniqueTableName(app, plotBaseName));
    res.plot = ml;
    if (ml && ml->activeGraph()) {
        Graph *gp = ml->activeGraph();
        const QString suTrim = tr.stressUnit.trimmed();
        const QString suSuffix = (suTrim.isEmpty() || suTrim == QLatin1String("-"))
                ? QString()
                : QStringLiteral(" / %1").arg(suTrim);

        if (!kocksMeckingPlot) {
            insertStyledCurve(gp, tbl, 0, 1, N, QPen(QColor(0, 114, 178), 2.2, Qt::SolidLine));
            if (gp->insertCurve(tbl, tbl->colName(0), tbl->colName(2), Graph::Line, thetaPlotStart,
                                N - 1)) {
                gp->setCurvePen(gp->curves() - 1, QPen(QColor(213, 94, 0), 2.0, Qt::SolidLine));
            }

            // Considere necking criterion vertical marker at eps_u
            Graph::ReferenceLineSpec neckLine;
            neckLine.orientation = 1; // Vertical X
            neckLine.value = res.considereTrueStrain;
            neckLine.color = QColor(185, 28, 28, 200);
            neckLine.style = Qt::DashLine;
            neckLine.width = 1.4;
            gp->addReferenceLine(neckLine);

            gp->applyFramePreset(Graph::FrameBoxInwardTicks);
            gp->setXAxisTitle(QStringLiteral("True strain, <i>\u03B5</i><sub>t</sub>"));
            gp->setYAxisTitle(QStringLiteral(
                    "<i>\u03C3</i><sub>t</sub>, <i>\u03B8</i> = d<i>\u03C3</i><sub>t</sub>/d<i>\u03B5</i><sub>t</sub>%1")
                                      .arg(suSuffix));
            gp->setTitle(QString());
            if (gp->legend()) {
                gp->legend()->setText(
                        QStringLiteral("\\c{1}<i>\u03C3</i><sub>t</sub>\n"
                                       "\\c{2}<i>\u03B8</i> = d<i>\u03C3</i><sub>t</sub>/d<i>\u03B5</i><sub>t</sub>"));
            }
            const double xUpper = std::max({ 0.02, colEpsT.back() * 1.24,
                                             res.considereTrueStrain * 2.35 });
            gp->setScale(QwtPlot::xBottom, 0.0, xUpper);
            gp->setScale(QwtPlot::yLeft, 0.0,
                         std::max(res.considereTrueStress * 2.60, tr.utsY * 1.80));
        } else {
            if (gp->insertCurve(tbl, tbl->colName(1), tbl->colName(2), Graph::Line, thetaPlotStart,
                                N - 1)) {
                gp->setCurvePen(gp->curves() - 1, QPen(QColor(213, 94, 0), 2.1, Qt::SolidLine));
            }
            if (gp->insertCurve(tbl, tbl->colName(1), tbl->colName(4), Graph::Line, 0, N - 1)) {
                gp->setCurvePen(gp->curves() - 1, QPen(QColor(185, 28, 28), 1.6, Qt::DashLine));
            }

            Graph::ReferenceLineSpec neckSig;
            neckSig.orientation = 1; // Vertical X at sigma_u
            neckSig.value = res.considereTrueStress;
            neckSig.color = QColor(100, 116, 139, 185);
            neckSig.style = Qt::DotLine;
            neckSig.width = 1.2;
            gp->addReferenceLine(neckSig);

            const double maxSig = *std::max_element(colSigT.begin(), colSigT.end());
            gp->applyFramePreset(Graph::FrameBoxInwardTicks);
            gp->setXAxisTitle(QStringLiteral("True stress, <i>\u03C3</i><sub>t</sub>%1")
                                      .arg(suSuffix));
            gp->setYAxisTitle(QStringLiteral(
                    "Work hardening rate, d<i>\u03C3</i><sub>t</sub>/d<i>\u03B5</i><sub>t</sub>%1")
                                      .arg(suSuffix));
            gp->setTitle(QString());
            if (gp->legend()) {
                gp->legend()->setText(
                        QStringLiteral("\\c{1}<i>\u03B8</i> = d<i>\u03C3</i><sub>t</sub>/d<i>\u03B5</i><sub>t</sub>\n"
                                       "\\c{2}<i>\u03B8</i> = <i>\u03C3</i><sub>t</sub>"));
            }
            gp->setScale(QwtPlot::xBottom, 0.0,
                         std::max(maxSig * 1.12, res.voceSigmaS * 1.06));
            gp->setScale(QwtPlot::yLeft, 0.0,
                         std::max(res.considereTrueStress * 2.4, tr.utsY * 1.7));
        }

        applyArialToMatSciGraph(gp);
        gp->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);

        const QString summary = QStringLiteral(
                "<i>\u03B5</i><sub>u</sub> = %1 (%2%)\n"
                "<i>\u03C3</i><sub>u</sub> = %3 %4\n"
                "<i>n</i> = %5, <i>K</i> = %6\n"
                "<i>\u03C3</i><sub>s</sub> = %7 %4")
                .arg(res.considereTrueStrain, 0, 'f', 3)
                .arg(res.considereEngStrainPct, 0, 'f', 1)
                .arg(res.considereTrueStress, 0, 'f', 1)
                .arg(tr.stressUnit)
                .arg(res.hollomonN, 0, 'f', 3)
                .arg(res.hollomonK, 0, 'f', 0)
                .arg(res.voceSigmaS, 0, 'f', 1);
        const double hintX = kocksMeckingPlot
                ? (0.06 * res.considereTrueStress)
                : std::max(res.considereTrueStrain * 1.15, 0.72 * colEpsT.back());
        addMatSciCalloutBox(gp, hintX, 1.75 * res.considereTrueStress, summary,
                            !kocksMeckingPlot);
        gp->replot();
    }

    return res;
}

void CurveMathTools::showWorkHardeningDialog(ApplicationWindow *app)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    registerMatSciTranslationsOnce();

    QList<Graph *> graphs;
    Graph *activeG = findActiveOrFirstGraph(app, nullptr);
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer")) {
            if (Graph *g = static_cast<MultiLayer *>(w)->activeGraph()) {
                if (!g->analysableCurvesList().isEmpty())
                    graphs.append(g);
            }
        }
    }
    if (graphs.isEmpty()) {
        QMessageBox::information(
                app, QObject::tr("Kocks-Mecking Work-Hardening & Considere Necking Analyzer"),
                QObject::tr("Please open a graph or numeric worksheet with stress-strain data first."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Kocks-Mecking Work-Hardening & Considere Necking Analyzer"));
    dlg.setMinimumWidth(520);
    QFormLayout *form = new QFormLayout(&dlg);

    QComboBox *cbGraph = new QComboBox(&dlg);
    QComboBox *cbCurve = new QComboBox(&dlg);
    int initIdx = 0;
    for (int i = 0; i < graphs.size(); ++i) {
        MultiLayer *ml = graphs[i]->multiLayerParent();
        cbGraph->addItem(ml ? ml->name() : graphs[i]->parentPlotName());
        if (graphs[i] == activeG)
            initIdx = i;
    }
    cbGraph->setCurrentIndex(initIdx);
    auto fillCurves = [&](int idx) {
        cbCurve->clear();
        if (idx >= 0 && idx < graphs.size())
            cbCurve->addItems(graphs[idx]->analysableCurvesList());
    };
    fillCurves(initIdx);
    QObject::connect(cbGraph, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     fillCurves);

    QSpinBox *sbWin = new QSpinBox(&dlg);
    sbWin->setRange(5, 51);
    sbWin->setSingleStep(2);
    sbWin->setValue(11);

    QCheckBox *chkPlot =
            new QCheckBox(QObject::tr("Create Considere (θ & σt vs εt) Plot"), &dlg);
    chkPlot->setChecked(true);

    QComboBox *cbPlotMode = new QComboBox(&dlg);
    cbPlotMode->addItem(QObject::tr("Considère (θ & σt vs εt)"), false);
    cbPlotMode->addItem(QObject::tr("Kocks-Mecking Stage-III (θ vs σt)"), true);
    QObject::connect(chkPlot, &QCheckBox::toggled, cbPlotMode, &QWidget::setEnabled);

    form->addRow(QObject::tr("Target Graph:"), cbGraph);
    form->addRow(QObject::tr("Stress-Strain Curve:"), cbCurve);
    form->addRow(QObject::tr("Savitzky-Golay Smoothing Window (odd points):"), sbWin);
    form->addRow(QString(), chkPlot);
    form->addRow(QObject::tr("Plot Mode:"), cbPlotMode);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const WorkHardeningResult wr = analyzeWorkHardening(
            app, graphs[cbGraph->currentIndex()], cbCurve->currentText(), sbWin->value(),
            chkPlot->isChecked(), cbPlotMode->currentData().toBool());
    if (!wr.valid && !ApplicationWindow::isNonInteractiveContext(app)) {
        QMessageBox::warning(
                app, QObject::tr("Kocks-Mecking Work-Hardening & Considere Necking Analyzer"),
                wr.error);
    }
}

// ============================================================================
// 4. XRD Peak, Bragg d-Spacing, Scherrer & Williamson-Hall Analyzer
// ============================================================================

CurveMathTools::XRDAnalysisResult CurveMathTools::analyzeXRD(
        ApplicationWindow *app, Graph *g, const QString &curveTitle, double wavelengthAngstrom,
        double instrFwhmDeg, double scherrerK, const QString &crystalSystem, bool stripKAlpha2,
        bool addTopDSpacingAxis, bool createWilliamsonHallPlot, double cOverA)
{
    registerMatSciTranslationsOnce();
    XRDAnalysisResult res;
    res.wavelengthAngstrom = (std::isfinite(wavelengthAngstrom) && wavelengthAngstrom > 0.05)
            ? wavelengthAngstrom
            : 1.540598;
    const double effLambda = res.wavelengthAngstrom;
    const double effInstFwhm = (std::isfinite(instrFwhmDeg) && instrFwhmDeg > 0.0) ? instrFwhmDeg : 0.0;
    const double effK = (std::isfinite(scherrerK) && scherrerK > 0.05) ? scherrerK : 0.90;

    Graph *targetGraph = findActiveOrFirstGraph(app, g);
    if (!targetGraph) {
        res.error = QObject::tr("Please open a graph or numeric worksheet with XRD data first.");
        return res;
    }

    QwtPlotCurve *srcCurve = nullptr;
    if (!curveTitle.trimmed().isEmpty()) {
        srcCurve = resolveCurve(targetGraph, curveTitle.trimmed());
    } else {
        const QString sel = targetGraph->selectedCurveTitle();
        if (!sel.isEmpty())
            srcCurve = resolveCurve(targetGraph, sel);
        if (!srcCurve) {
            const QStringList lst = targetGraph->analysableCurvesList();
            if (!lst.isEmpty())
                srcCurve = resolveCurve(targetGraph, lst.first());
        }
        if (!srcCurve) {
            for (int i = 0; i < targetGraph->curves(); ++i) {
                QwtPlotCurve *c = targetGraph->curve(i);
                if (c && c->rtti() == QwtPlotItem::Rtti_PlotCurve && c->dataSize() >= 15) {
                    srcCurve = c;
                    break;
                }
            }
        }
    }
    if (!srcCurve || srcCurve->dataSize() < 15) {
        res.error = QObject::tr("No prominent Bragg diffraction peaks were detected on the curve.");
        return res;
    }

    const auto *dc = dynamic_cast<const DataCurve *>(srcCurve);
    std::vector<std::pair<double, double>> rawPts;
    rawPts.reserve(static_cast<size_t>(srcCurve->dataSize()));
    for (int i = 0; i < srcCurve->dataSize(); ++i) {
        if (dc && dc->isPointMasked(i))
            continue;
        const double tt = srcCurve->x(i);
        const double yy = srcCurve->y(i);
        if (!std::isfinite(tt) || !std::isfinite(yy) || tt <= 0.5 || tt >= 179.5)
            continue;
        rawPts.emplace_back(tt, yy);
    }
    if (rawPts.size() < 15) {
        res.error = QObject::tr("No prominent Bragg diffraction peaks were detected on the curve.");
        return res;
    }

    std::sort(rawPts.begin(), rawPts.end(),
              [](const std::pair<double, double> &a, const std::pair<double, double> &b) {
                  return a.first < b.first;
              });

    std::vector<double> ttGrid;
    std::vector<double> yRaw;
    ttGrid.reserve(rawPts.size());
    yRaw.reserve(rawPts.size());
    for (size_t i = 0; i < rawPts.size();) {
        double sx = rawPts[i].first;
        double sy = rawPts[i].second;
        int cnt = 1;
        size_t j = i + 1;
        while (j < rawPts.size() && std::fabs(rawPts[j].first - rawPts[i].first) <= 1e-8) {
            sx += rawPts[j].first;
            sy += rawPts[j].second;
            ++cnt;
            ++j;
        }
        ttGrid.push_back(sx / double(cnt));
        yRaw.push_back(sy / double(cnt));
        i = j;
    }

    const int N = static_cast<int>(ttGrid.size());
    if (N < 15) {
        res.error = QObject::tr("No prominent Bragg diffraction peaks were detected on the curve.");
        return res;
    }

    const double minYRaw = *std::min_element(yRaw.begin(), yRaw.end());
    const double shiftZero = (minYRaw < 0.0) ? -minYRaw : 0.0;

    // 1. Iterative SNIP background estimation in LLS space
    const double avgStep = std::max(1e-4, (ttGrid.back() - ttGrid.front()) / double(N - 1));
    const int snipWin = std::clamp(static_cast<int>(std::llround(2.2 / avgStep)), 6,
                                   std::max(6, std::min(64, N / 4)));
    std::vector<double> lls(size_t(N), 0.0);
    for (int i = 0; i < N; ++i) {
        const double posI = std::max(0.0, yRaw[size_t(i)] + shiftZero);
        lls[size_t(i)] = std::log(std::log(std::sqrt(posI + 1.0) + 1.0) + 1.0);
    }
    std::vector<double> llsTmp = lls;
    for (int p = snipWin; p >= 1; --p) {
        for (int i = p; i < N - p; ++i) {
            const double avgNeighbor = 0.5 * (lls[size_t(i - p)] + lls[size_t(i + p)]);
            llsTmp[size_t(i)] = std::min(lls[size_t(i)], avgNeighbor);
        }
        lls.swap(llsTmp);
    }
    std::vector<double> bg(size_t(N), 0.0);
    std::vector<double> yNet(size_t(N), 0.0);
    for (int i = 0; i < N; ++i) {
        const double bVal = std::pow(std::exp(std::exp(lls[size_t(i)]) - 1.0) - 1.0, 2.0) - 1.0;
        const double posI = std::max(0.0, yRaw[size_t(i)] + shiftZero);
        bg[size_t(i)] = std::clamp(bVal, 0.0, posI) - shiftZero;
        yNet[size_t(i)] = std::max(0.0, posI - std::clamp(bVal, 0.0, posI));
    }

    // 2. Optional Rachinger Kα2 doublet stripping on net intensity
    if (stripKAlpha2) {
        constexpr double kAlphaRatio = 1.544426 / 1.540598; // λ(Kα2) / λ(Kα1)
        std::vector<double> alpha1(size_t(N), 0.0);
        for (int i = 0; i < N; ++i) {
            const double thRad = 0.5 * ttGrid[size_t(i)] * (M_PI / 180.0);
            const double sinTh1 = std::sin(thRad) / kAlphaRatio;
            double sub = 0.0;
            if (sinTh1 > 0.0 && sinTh1 < 1.0) {
                const double tt1 = 2.0 * std::asin(sinTh1) * (180.0 / M_PI);
                if (tt1 >= ttGrid.front() && tt1 < ttGrid[size_t(i)]) {
                    auto it = std::lower_bound(ttGrid.begin(), ttGrid.begin() + i + 1, tt1);
                    const int idx = static_cast<int>(it - ttGrid.begin());
                    if (idx > 0 && idx <= i) {
                        const double x0 = ttGrid[size_t(idx - 1)];
                        const double x1 = ttGrid[size_t(idx)];
                        const double t = (x1 > x0) ? ((tt1 - x0) / (x1 - x0)) : 0.0;
                        const double i1 = alpha1[size_t(idx - 1)]
                                + t * (alpha1[size_t(idx)] - alpha1[size_t(idx - 1)]);
                        sub = 0.5 * std::max(0.0, i1);
                    }
                }
            }
            alpha1[size_t(i)] = std::max(0.0, yNet[size_t(i)] - sub);
        }
        yNet.swap(alpha1);
    }

    const double maxNet = *std::max_element(yNet.begin(), yNet.end());
    if (!(maxNet > 0.0)) {
        res.error = QObject::tr("No prominent Bragg diffraction peaks were detected on the curve.");
        return res;
    }

    // 3. Prominent peak detection with 3-point smoothing + local prominence
    std::vector<double> ySmooth(size_t(N), 0.0);
    ySmooth[0] = yNet[0];
    ySmooth[size_t(N - 1)] = yNet[size_t(N - 1)];
    for (int i = 1; i < N - 1; ++i)
        ySmooth[size_t(i)] = 0.25 * yNet[size_t(i - 1)] + 0.50 * yNet[size_t(i)] + 0.25 * yNet[size_t(i + 1)];

    const int promSpan = std::max(3, static_cast<int>(std::llround(1.4 / avgStep)));
    std::vector<int> candIdx;
    for (int i = 2; i < N - 2; ++i) {
        if (ySmooth[size_t(i)] < 0.035 * maxNet || yNet[size_t(i)] < 0.035 * maxNet)
            continue;
        if (!(ySmooth[size_t(i)] >= ySmooth[size_t(i - 1)]
              && ySmooth[size_t(i)] > ySmooth[size_t(i + 1)])) {
            continue;
        }
        double minL = ySmooth[size_t(i)];
        for (int k = std::max(0, i - promSpan); k < i; ++k)
            minL = std::min(minL, ySmooth[size_t(k)]);
        double minR = ySmooth[size_t(i)];
        for (int k = i + 1; k <= std::min(N - 1, i + promSpan); ++k)
            minR = std::min(minR, ySmooth[size_t(k)]);
        const double prom = ySmooth[size_t(i)] - std::max(minL, minR);
        if (prom >= 0.03 * maxNet)
            candIdx.push_back(i);
    }

    // Merge close candidates (< 0.32 deg apart) keeping taller peak, cap at top 20
    std::sort(candIdx.begin(), candIdx.end(), [&](int a, int b) {
        return yNet[size_t(a)] > yNet[size_t(b)];
    });
    std::vector<int> selIdx;
    for (int idx : candIdx) {
        bool tooClose = false;
        for (int kept : selIdx) {
            if (std::fabs(ttGrid[size_t(idx)] - ttGrid[size_t(kept)]) < 0.32) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose) {
            selIdx.push_back(idx);
            if (selIdx.size() >= 20)
                break;
        }
    }
    if (selIdx.empty()) {
        res.error = QObject::tr("No prominent Bragg diffraction peaks were detected on the curve.");
        return res;
    }
    std::sort(selIdx.begin(), selIdx.end());

    // 4. Refine apex, FWHM (half-max crossing + local Pseudo-Voigt core polish), d, Scherrer D
    struct InternalPeak {
        XRDPeakEntry pub;
        double rawApexY = 0.0;
        double fwhmObsDeg = 0.0;
        double betaRad = 0.0;
        double thetaRad = 0.0;
        bool indexed = false;
        bool resolutionLimited = false;
    };
    std::vector<InternalPeak> pks;
    pks.reserve(selIdx.size());

    for (int ip : selIdx) {
        // Snap ip to local maximum of un-smoothed yNet within +-1 sample
        int pkIdx = ip;
        if (pkIdx > 1 && yNet[size_t(pkIdx - 1)] > yNet[size_t(pkIdx)])
            --pkIdx;
        if (pkIdx + 1 < N - 1 && yNet[size_t(pkIdx + 1)] > yNet[size_t(pkIdx)])
            ++pkIdx;

        double xApex = ttGrid[size_t(pkIdx)];
        double hApex = yNet[size_t(pkIdx)];
        if (pkIdx > 0 && pkIdx + 1 < N) {
            const double x0 = ttGrid[size_t(pkIdx - 1)];
            const double x1 = ttGrid[size_t(pkIdx)];
            const double x2 = ttGrid[size_t(pkIdx + 1)];
            const double l0 = std::log(std::max(1e-9, yNet[size_t(pkIdx - 1)]));
            const double l1 = std::log(std::max(1e-9, yNet[size_t(pkIdx)]));
            const double l2 = std::log(std::max(1e-9, yNet[size_t(pkIdx + 1)]));
            const double denom = (x0 - x1) * (x0 - x2) * (x1 - x2);
            if (std::fabs(denom) > 1e-20) {
                const double A = (x2 * (l1 - l0) + x1 * (l0 - l2) + x0 * (l2 - l1)) / denom;
                const double B = (x2 * x2 * (l0 - l1) + x1 * x1 * (l2 - l0) + x0 * x0 * (l1 - l2)) / denom;
                if (A < -1e-12) {
                    const double xv = -B / (2.0 * A);
                    if (xv >= x0 && xv <= x2) {
                        const double C = l1 - A * x1 * x1 - B * x1;
                        xApex = xv;
                        hApex = std::max(hApex, std::exp(A * xv * xv + B * xv + C));
                    }
                }
            }
        }

        const double halfH = 0.5 * hApex;
        bool hasLeft = false, hasRight = false;
        double xLeft = xApex - avgStep;
        double xRight = xApex + avgStep;

        for (int k = pkIdx; k > 0; --k) {
            if (yNet[size_t(k - 1)] <= halfH && yNet[size_t(k)] >= halfH) {
                const double dy = yNet[size_t(k)] - yNet[size_t(k - 1)];
                const double frac = (dy > 1e-12) ? ((halfH - yNet[size_t(k - 1)]) / dy) : 0.5;
                xLeft = ttGrid[size_t(k - 1)] + frac * (ttGrid[size_t(k)] - ttGrid[size_t(k - 1)]);
                hasLeft = true;
                break;
            }
            if (k < pkIdx - 1 && yNet[size_t(k)] < 0.75 * hApex
                && yNet[size_t(k - 1)] > yNet[size_t(k)] * 1.08) {
                break;
            }
        }
        for (int k = pkIdx; k + 1 < N; ++k) {
            if (yNet[size_t(k + 1)] <= halfH && yNet[size_t(k)] >= halfH) {
                const double dy = yNet[size_t(k)] - yNet[size_t(k + 1)];
                const double frac = (dy > 1e-12) ? ((yNet[size_t(k)] - halfH) / dy) : 0.5;
                xRight = ttGrid[size_t(k)] + frac * (ttGrid[size_t(k + 1)] - ttGrid[size_t(k)]);
                hasRight = true;
                break;
            }
            if (k > pkIdx + 1 && yNet[size_t(k)] < 0.75 * hApex
                && yNet[size_t(k + 1)] > yNet[size_t(k)] * 1.08) {
                break;
            }
        }

        double fwhm0 = 2.0 * avgStep;
        if (hasLeft && hasRight)
            fwhm0 = std::max(0.5 * avgStep, xRight - xLeft);
        else if (hasLeft)
            fwhm0 = std::max(0.5 * avgStep, 2.0 * (xApex - xLeft));
        else if (hasRight)
            fwhm0 = std::max(0.5 * avgStep, 2.0 * (xRight - xApex));

        // Local Pseudo-Voigt core refinement (I >= 0.30 * hApex within +-1.1 * fwhm0)
        std::vector<int> corePts;
        for (int k = std::max(0, pkIdx - promSpan); k <= std::min(N - 1, pkIdx + promSpan); ++k) {
            if (std::fabs(ttGrid[size_t(k)] - xApex) <= 1.15 * fwhm0
                && yNet[size_t(k)] >= 0.28 * hApex) {
                corePts.push_back(k);
            }
        }
        double fwhmObs = fwhm0;
        if (corePts.size() >= 5) {
            auto evalPvRss = [&](double wTry, double etaTry, double hTry) -> double {
                const double halfW = std::max(1e-6, 0.5 * wTry);
                double rss = 0.0;
                for (int k : corePts) {
                    const double u = (ttGrid[size_t(k)] - xApex) / halfW;
                    const double u2 = u * u;
                    const double pv = etaTry / (1.0 + u2) + (1.0 - etaTry) * std::exp(-M_LN2 * u2);
                    const double diff = yNet[size_t(k)] - hTry * pv;
                    rss += diff * diff;
                }
                return rss;
            };
            double bestW = fwhm0;
            double bestEta = 0.5;
            double bestH = hApex;
            double bestRss = evalPvRss(bestW, bestEta, bestH);
            double stepW = 0.12 * fwhm0;
            double stepEta = 0.20;
            double stepH = 0.03 * hApex;
            const double wLo = 0.72 * fwhm0;
            const double wHi = 1.28 * fwhm0;
            for (int it = 0; it < 18; ++it) {
                for (int dir : { -1, 1 }) {
                    const double candW = std::clamp(bestW + dir * stepW, wLo, wHi);
                    const double r = evalPvRss(candW, bestEta, bestH);
                    if (r < bestRss) {
                        bestRss = r;
                        bestW = candW;
                    }
                }
                for (int dir : { -1, 1 }) {
                    const double candEta = std::clamp(bestEta + dir * stepEta, 0.0, 1.0);
                    const double r = evalPvRss(bestW, candEta, bestH);
                    if (r < bestRss) {
                        bestRss = r;
                        bestEta = candEta;
                    }
                }
                for (int dir : { -1, 1 }) {
                    const double candH = std::clamp(bestH + dir * stepH, 0.85 * hApex, 1.15 * hApex);
                    const double r = evalPvRss(bestW, bestEta, candH);
                    if (r < bestRss) {
                        bestRss = r;
                        bestH = candH;
                    }
                }
                stepW *= 0.65;
                stepEta *= 0.65;
                stepH *= 0.65;
            }
            fwhmObs = bestW;
            hApex = bestH;
        }

        const double thRad = 0.5 * xApex * (M_PI / 180.0);
        const double sinTh = std::sin(thRad);
        const double cosTh = std::cos(thRad);
        if (sinTh <= 1e-6 || cosTh <= 1e-6)
            continue;

        const double dAng = effLambda / (2.0 * sinTh);
        const bool resLimited = (fwhmObs <= effInstFwhm + 1e-6);
        const double fwhmPhysDeg = resLimited
                ? 0.0
                : std::sqrt(std::max(0.0, fwhmObs * fwhmObs - effInstFwhm * effInstFwhm));
        const double betaRad = fwhmPhysDeg * (M_PI / 180.0);
        const double dSchNm = resLimited
                ? std::numeric_limits<double>::quiet_NaN()
                : ((effK * effLambda) / (10.0 * std::max(1e-9, betaRad * cosTh)));

        InternalPeak ipk;
        ipk.pub.twoThetaDeg = xApex;
        ipk.pub.dSpacingAngstrom = dAng;
        ipk.pub.fwhmDeg = fwhmPhysDeg;
        ipk.pub.intensity = bg[size_t(pkIdx)] + hApex;
        ipk.pub.scherrerSizeNm = dSchNm;
        ipk.rawApexY = std::max(yRaw[size_t(pkIdx)], bg[size_t(pkIdx)] + hApex);
        ipk.fwhmObsDeg = fwhmObs;
        ipk.betaRad = betaRad;
        ipk.thetaRad = thRad;
        ipk.resolutionLimited = resLimited;
        pks.push_back(ipk);
    }

    if (pks.empty()) {
        res.error = QObject::tr("No prominent Bragg diffraction peaks were detected on the curve.");
        return res;
    }

    // 5. Optional (hkl) auto-indexing ("fcc", "bcc", "sc", "hcp")
    const QString sysKey = crystalSystem.trimmed().toLower();
    if (!sysKey.isEmpty() && sysKey != QLatin1String("none")) {
        struct AllowedRefl {
            double M; // d = a / sqrt(M)
            const char *label;
        };
        std::vector<AllowedRefl> allowed;
        if (sysKey == QLatin1String("fcc")) {
            allowed = { { 3.0, "(111)" },  { 4.0, "(200)" },  { 8.0, "(220)" },  { 11.0, "(311)" },
                        { 12.0, "(222)" }, { 16.0, "(400)" }, { 19.0, "(331)" }, { 20.0, "(420)" },
                        { 24.0, "(422)" }, { 27.0, "(511)" } };
        } else if (sysKey == QLatin1String("bcc")) {
            allowed = { { 2.0, "(110)" },  { 4.0, "(200)" },  { 6.0, "(211)" },  { 8.0, "(220)" },
                        { 10.0, "(310)" }, { 12.0, "(222)" }, { 14.0, "(321)" }, { 16.0, "(400)" },
                        { 18.0, "(411)" }, { 20.0, "(420)" } };
        } else if (sysKey == QLatin1String("sc") || sysKey == QLatin1String("cubic")) {
            allowed = { { 1.0, "(100)" },  { 2.0, "(110)" },  { 3.0, "(111)" },  { 4.0, "(200)" },
                        { 5.0, "(210)" },  { 6.0, "(211)" },  { 8.0, "(220)" },  { 9.0, "(300)" },
                        { 10.0, "(310)" }, { 11.0, "(311)" }, { 12.0, "(222)" } };
        } else if (sysKey == QLatin1String("hcp") || sysKey == QLatin1String("hex")
                   || sysKey == QLatin1String("hexagonal")) {
            const double effCA = (std::isfinite(cOverA) && cOverA > 0.3 && cOverA < 6.0)
                    ? cOverA
                    : 1.632993161855452;
            const double invCA2 = 1.0 / (effCA * effCA);
            struct HcpCand {
                int h, k, l;
                const char *lbl;
            };
            static const HcpCand kHcpList[] = {
                { 1, 0, 0, "(100)" }, { 0, 0, 2, "(002)" }, { 1, 0, 1, "(101)" },
                { 1, 0, 2, "(102)" }, { 1, 1, 0, "(110)" }, { 1, 0, 3, "(103)" },
                { 2, 0, 0, "(200)" }, { 1, 1, 2, "(112)" }, { 2, 0, 1, "(201)" },
                { 0, 0, 4, "(004)" }, { 2, 0, 2, "(202)" }, { 1, 0, 4, "(104)" },
                { 2, 0, 3, "(203)" }, { 2, 1, 0, "(210)" }, { 2, 1, 1, "(211)" },
                { 1, 1, 4, "(114)" }, { 2, 1, 2, "(212)" }, { 1, 0, 5, "(105)" }
            };
            for (const HcpCand &hc : kHcpList) {
                const double mVal = (4.0 / 3.0) * double(hc.h * hc.h + hc.h * hc.k + hc.k * hc.k)
                        + double(hc.l * hc.l) * invCA2;
                allowed.push_back({ mVal, hc.lbl });
            }
            std::sort(allowed.begin(), allowed.end(),
                      [](const AllowedRefl &a, const AllowedRefl &b) { return a.M < b.M; });
        }

        if (!allowed.empty()) {
            double bestScore = -1.0;
            double bestA0 = 0.0;
            std::vector<int> bestAssign(pks.size(), -1);

            const size_t maxRefPeak = std::min<size_t>(2, pks.size());
            const size_t maxFirstHkl = std::min<size_t>(4, allowed.size());
            for (size_t rp = 0; rp < maxRefPeak; ++rp) {
                for (size_t fi = 0; fi < maxFirstHkl; ++fi) {
                    const double aTry = pks[rp].pub.dSpacingAngstrom * std::sqrt(allowed[fi].M);
                    if (aTry < 1.8 || aTry > 18.0)
                        continue;
                    std::vector<int> assign(pks.size(), -1);
                    int matched = 0;
                    double sumErr2 = 0.0;
                    double sumA = 0.0;
                    double sumW = 0.0;
                    int prevK = -1;
                    for (size_t p = 0; p < pks.size(); ++p) {
                        const double mEst = (aTry / pks[p].pub.dSpacingAngstrom)
                                * (aTry / pks[p].pub.dSpacingAngstrom);
                        int bestK = -1;
                        double bestRel = 0.038;
                        for (size_t k = 0; k < allowed.size(); ++k) {
                            if (static_cast<int>(k) <= prevK)
                                continue;
                            const double rel = std::fabs(mEst - allowed[k].M) / allowed[k].M;
                            if (rel < bestRel) {
                                bestRel = rel;
                                bestK = static_cast<int>(k);
                            }
                        }
                        if (bestK >= 0) {
                            assign[p] = bestK;
                            prevK = bestK;
                            ++matched;
                            sumErr2 += bestRel * bestRel;
                            const double w = std::sin(pks[p].thetaRad);
                            sumA += w * pks[p].pub.dSpacingAngstrom
                                    * std::sqrt(allowed[size_t(bestK)].M);
                            sumW += w;
                        }
                    }
                    if (matched > 0) {
                        const double rmse = std::sqrt(sumErr2 / double(matched));
                        const double score = double(matched) - 12.0 * rmse;
                        if (score > bestScore) {
                            bestScore = score;
                            bestA0 = (sumW > 0.0) ? (sumA / sumW) : aTry;
                            bestAssign = assign;
                        }
                    }
                }
            }

            if (bestA0 > 0.0) {
                res.latticeParamAngstrom = bestA0;
                for (size_t p = 0; p < pks.size(); ++p) {
                    if (bestAssign[p] >= 0) {
                        pks[p].pub.hklLabel =
                                QString::fromLatin1(allowed[size_t(bestAssign[p])].label);
                        pks[p].indexed = true;
                    }
                }
            }
        }
    }

    // 6. Mean Scherrer size & Williamson-Hall linear regression (β cos θ = Kλ/D + 4ε sin θ)
    double sumSch = 0.0;
    int cntSch = 0;
    int resLimitedCount = 0;
    for (const InternalPeak &ipk : pks) {
        if (ipk.resolutionLimited) {
            ++resLimitedCount;
        } else if (std::isfinite(ipk.pub.scherrerSizeNm) && ipk.pub.scherrerSizeNm > 0.0) {
            sumSch += ipk.pub.scherrerSizeNm;
            ++cntSch;
        }
    }
    res.meanScherrerSizeNm = (cntSch > 0) ? (sumSch / double(cntSch))
                                          : std::numeric_limits<double>::quiet_NaN();

    std::vector<size_t> whPool;
    for (size_t p = 0; p < pks.size(); ++p) {
        if (pks[p].indexed && !pks[p].resolutionLimited)
            whPool.push_back(p);
    }
    if (whPool.size() < 2) {
        whPool.clear();
        for (size_t p = 0; p < pks.size(); ++p) {
            if (!pks[p].resolutionLimited)
                whPool.push_back(p);
        }
    }
    if (whPool.empty()) {
        for (size_t p = 0; p < pks.size(); ++p)
            whPool.push_back(p);
    }

    double whSlope = 0.0;
    double whIntercept = (cntSch > 0 && std::isfinite(res.meanScherrerSizeNm))
            ? ((effK * effLambda) / (10.0 * std::max(0.5, res.meanScherrerSizeNm)))
            : 0.0;
    double whR2 = 0.0;
    double whSResid = 0.0;
    double whMeanX = 0.0;
    double whSxx = 0.0;
    if (whPool.size() >= 2) {
        const double m = double(whPool.size());
        double sx = 0.0, sy = 0.0;
        for (size_t idx : whPool) {
            sx += 4.0 * std::sin(pks[idx].thetaRad);
            sy += pks[idx].betaRad * std::cos(pks[idx].thetaRad);
        }
        whMeanX = sx / m;
        const double meanY = sy / m;
        double sxy = 0.0, syy = 0.0;
        for (size_t idx : whPool) {
            const double dx = 4.0 * std::sin(pks[idx].thetaRad) - whMeanX;
            const double dy = pks[idx].betaRad * std::cos(pks[idx].thetaRad) - meanY;
            whSxx += dx * dx;
            sxy += dx * dy;
            syy += dy * dy;
        }
        if (whSxx > 1e-24) {
            whSlope = sxy / whSxx;
            whIntercept = meanY - whSlope * whMeanX;
            double rss = 0.0;
            for (size_t idx : whPool) {
                const double x = 4.0 * std::sin(pks[idx].thetaRad);
                const double y = pks[idx].betaRad * std::cos(pks[idx].thetaRad);
                const double e = y - (whIntercept + whSlope * x);
                rss += e * e;
            }
            whR2 = (syy > 1e-30) ? std::clamp(1.0 - rss / syy, 0.0, 1.0) : 1.0;
            if (whPool.size() > 2)
                whSResid = std::sqrt(std::max(0.0, rss / double(whPool.size() - 2)));
        }
    }
    res.whMicrostrain = whSlope;
    res.whSizeNm = (cntSch > 0 && whIntercept > 1e-7)
            ? ((effK * effLambda) / (10.0 * whIntercept))
            : res.meanScherrerSizeNm;
    res.whRSquare = whR2;
    res.valid = true;
    if (resLimitedCount > 0) {
        res.error = QObject::tr(
                "Warning: %1 of %2 peak(s) have observed FWHM <= instrumental FWHM (%3 deg); "
                "Scherrer crystallite size is resolution-limited (N/A).")
                            .arg(resLimitedCount)
                            .arg(static_cast<int>(pks.size()))
                            .arg(effInstFwhm, 0, 'g', 4);
        if (app)
            app->updateLog(res.error + QLatin1Char('\n'));
    }

    res.peaks.reserve(static_cast<int>(pks.size()));
    for (const InternalPeak &ipk : pks)
        res.peaks.append(ipk.pub);

    // 7. Annotate diffractogram on targetGraph: (hkl) apex labels + optional Bragg Top-X axis
    MultiLayer *srcML = targetGraph->multiLayerParent();
    const QString beforeSnap = srcML ? srcML->captureUndoSnapshot() : QString();

    applyArialToMatSciGraph(targetGraph);
    if (Plot *pw = targetGraph->plotWidget()) {
        double maxApexRaw = *std::max_element(yRaw.begin(), yRaw.end());
        for (const InternalPeak &ipk : pks)
            maxApexRaw = std::max(maxApexRaw, ipk.rawApexY);

        const QwtScaleDiv *yDiv = pw->axisScaleDiv(QwtPlot::yLeft);
        const double curYLo = yDiv ? std::min(yDiv->lowerBound(), yDiv->upperBound()) : 0.0;
        const double curYHi = yDiv ? std::max(yDiv->lowerBound(), yDiv->upperBound()) : maxApexRaw;
        const bool hasAnyHkl = std::any_of(pks.begin(), pks.end(), [](const InternalPeak &ipk) {
            return !ipk.pub.hklLabel.isEmpty();
        });
        if (hasAnyHkl && curYHi < curYLo + 1.24 * (maxApexRaw - curYLo)) {
            targetGraph->setScale(QwtPlot::yLeft, curYLo,
                                  curYLo + 1.25 * std::max(1.0, maxApexRaw - curYLo));
        }

        if (addTopDSpacingAxis && srcML) {
            bool hasLinkedTop = false;
            for (Graph *lg : srcML->layersList()) {
                if (lg && lg->linkedParentLayer() == targetGraph && lg->plotWidget()
                    && lg->plotWidget()->axisEnabled(QwtPlot::xTop)) {
                    hasLinkedTop = true;
                    break;
                }
            }
            if (!hasLinkedTop) {
                const QwtScaleDiv *xDiv = pw->axisScaleDiv(QwtPlot::xBottom);
                double xLo = xDiv ? std::min(xDiv->lowerBound(), xDiv->upperBound()) : ttGrid.front();
                double xHi = xDiv ? std::max(xDiv->lowerBound(), xDiv->upperBound()) : ttGrid.back();
                if (xLo < 1.0) {
                    xLo = std::max(1.0, ttGrid.front());
                    if (xHi <= xLo)
                        xHi = xLo + 10.0;
                    targetGraph->setScale(QwtPlot::xBottom, xLo, xHi);
                }
                const QString dFormula = QStringLiteral("%1/(2.0*sin(x*3.14159265358979323846/360.0))")
                                                 .arg(effLambda, 0, 'g', 10);
                Graph *topLayer = srcML->addLinkedLayer(targetGraph, true, false, dFormula,
                                                        QStringLiteral("y"));
                if (topLayer) {
                    topLayer->setAxisTitle(QwtPlot::xTop, QStringLiteral("<i>d</i> / \u00C5"));
                    applyArialToMatSciGraph(topLayer);
                }
                srcML->arrangeLayers(false, false);
                targetGraph->syncChildLinkedLayers();
                targetGraph->replot();
                if (topLayer)
                    topLayer->replot();
                srcML->setActiveGraph(targetGraph);
            }
        }

        targetGraph->replot();
        if (hasAnyHkl) {
            struct HklPin {
                int markerId = -1;
                double twoTheta = 0.0;
                double apexY = 0.0;
                QString text;
            };
            QVector<HklPin> pins;
            QFont hklFont(QStringLiteral("Arial"), 9, QFont::Normal);
            hklFont.setBold(false);
            for (const InternalPeak &ipk : pks) {
                if (ipk.pub.hklLabel.isEmpty())
                    continue;
                Legend *lbl = targetGraph->newLegend(ipk.pub.hklLabel);
                if (!lbl)
                    continue;
                lbl->setFrameStyle(Legend::None);
                lbl->setBackgroundColor(QColor(255, 255, 255, 0));
                lbl->setTextColor(QColor(20, 35, 65));
                lbl->setFont(hklFont);
                int foundId = -1;
                for (int tid : targetGraph->textMarkerKeys()) {
                    if (targetGraph->textMarker(tid) == lbl) {
                        foundId = tid;
                        break;
                    }
                }
                if (foundId >= 0)
                    pins.append({ foundId, ipk.pub.twoThetaDeg, ipk.rawApexY, ipk.pub.hklLabel });
            }

            auto syncHklPins = [gPtr = QPointer<Graph>(targetGraph), pins]() {
                if (!gPtr || !gPtr->plotWidget() || !gPtr->plotWidget()->canvas())
                    return;
                Plot *plotW = gPtr->plotWidget();
                const int cw = plotW->canvas()->width();
                if (cw <= 24)
                    return;
                const QwtScaleMap xMap = plotW->canvasMap(QwtPlot::xBottom);
                const QwtScaleMap yMap = plotW->canvasMap(QwtPlot::yLeft);
                QVector<QRectF> placedRects;
                placedRects.reserve(pins.size());

                for (const HklPin &pin : pins) {
                    Legend *lbl = gPtr->textMarker(pin.markerId);
                    if (!lbl)
                        continue;
                    QFont lf = lbl->font();
                    if (lf.bold() || lf.pointSize() > 11) {
                        lf.setBold(false);
                        if (lf.pointSize() > 11 && cw < 340)
                            lf.setPointSize(9);
                        lbl->setFont(lf);
                    }
                    const QFontMetricsF fm(lf);
                    const double textW = fm.horizontalAdvance(pin.text);
                    const double textH = fm.height();
                    const double pxApex = xMap.xTransform(pin.twoTheta);
                    const double pyApex = yMap.xTransform(pin.apexY);
                    const double maxOriginL = std::max(1.0, double(cw) - textW - 8.0);
                    const double boxLeftPx = std::clamp(pxApex - 0.5 * textW - 6.0, 1.0, maxOriginL);
                    double boxTopPx = std::max(3.0, pyApex - textH - 6.0);

                    QRectF candRect(boxLeftPx, boxTopPx, textW + 8.0, textH + 4.0);
                    for (int attempt = 0; attempt < 4; ++attempt) {
                        bool hit = false;
                        for (const QRectF &pr : placedRects) {
                            if (candRect.intersects(pr)) {
                                boxTopPx = std::max(2.0, pr.top() - textH - 3.0);
                                candRect.moveTop(boxTopPx);
                                hit = true;
                            }
                        }
                        if (!hit)
                            break;
                    }
                    placedRects.append(candRect);
                    lbl->setOriginCoord(plotW->invTransform(QwtPlot::xBottom, qRound(boxLeftPx)),
                                        plotW->invTransform(QwtPlot::yLeft, qRound(boxTopPx)));
                }
            };

            syncHklPins();
            targetGraph->replot();
            if (targetGraph->isLegendVisible() && targetGraph->legend())
                targetGraph->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);

            if (!pins.isEmpty()) {
                auto guard = std::make_shared<bool>(false);
                QObject::connect(targetGraph, &Graph::modifiedGraph, targetGraph,
                                 [gPtr = QPointer<Graph>(targetGraph), syncHklPins, guard]() {
                                     if (*guard || !gPtr)
                                         return;
                                     *guard = true;
                                     syncHklPins();
                                     if (gPtr->isLegendVisible() && gPtr->legend())
                                         gPtr->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 10);
                                     *guard = false;
                                 });
            }
        }
        targetGraph->replot();
    }

    if (srcML && !beforeSnap.isEmpty())
        srcML->pushUndoSnapshot(QObject::tr("XRD Peak, Scherrer & Williamson-Hall Analyzer"), beforeSnap);

    if (!app)
        return res;

    // 8. Build XRD Peaks + Williamson-Hall worksheet
    const int M = static_cast<int>(pks.size());
    const size_t mSz = static_cast<size_t>(M);
    std::vector<double> col2Th(mSz), colD(mSz), colQ(mSz), colInt(mSz);
    std::vector<double> colFwhmObs(mSz), colFwhmCorr(mSz), colSch(mSz);
    std::vector<double> colWhX(mSz), colWhY(mSz), colWhFit(mSz);
    std::vector<double> colWhLo(mSz), colWhHi(mSz);
    const double tCrit = (whPool.size() > 2)
            ? gsl_cdf_tdist_Pinv(0.975, double(whPool.size() - 2))
            : 0.0;

    for (int i = 0; i < M; ++i) {
        const InternalPeak &ipk = pks[size_t(i)];
        const double whX = 4.0 * std::sin(ipk.thetaRad);
        const double whY = ipk.betaRad * std::cos(ipk.thetaRad);
        const double yh = whIntercept + whSlope * whX;
        const double hb = (whPool.size() > 2 && whSxx > 1e-24)
                ? (tCrit * whSResid
                   * std::sqrt(1.0 / double(whPool.size())
                               + ((whX - whMeanX) * (whX - whMeanX)) / whSxx))
                : 0.0;
        col2Th[size_t(i)] = ipk.pub.twoThetaDeg;
        colD[size_t(i)] = ipk.pub.dSpacingAngstrom;
        colQ[size_t(i)] = (2.0 * M_PI) / std::max(1e-9, ipk.pub.dSpacingAngstrom);
        colInt[size_t(i)] = ipk.pub.intensity;
        colFwhmObs[size_t(i)] = ipk.fwhmObsDeg;
        colFwhmCorr[size_t(i)] = ipk.pub.fwhmDeg;
        colSch[size_t(i)] = ipk.pub.scherrerSizeNm;
        colWhX[size_t(i)] = whX;
        colWhY[size_t(i)] = whY;
        colWhFit[size_t(i)] = yh;
        colWhLo[size_t(i)] = yh - hb;
        colWhHi[size_t(i)] = yh + hb;
    }

    Table *tbl = createNumericTable(
            app, QStringLiteral("XRDPeaks"),
            QStringList() << QStringLiteral("TwoTheta") << QStringLiteral("dSpacing")
                          << QStringLiteral("QInvA") << QStringLiteral("Intensity")
                          << QStringLiteral("FWHM_obs") << QStringLiteral("FWHM_corr")
                          << QStringLiteral("ScherrerD_nm") << QStringLiteral("FourSinTheta")
                          << QStringLiteral("BetaCosTheta") << QStringLiteral("WHLinearFit")
                          << QStringLiteral("WHCI95Lo") << QStringLiteral("WHCI95Hi"),
            QList<SciDAVis::PlotDesignation>()
                    << SciDAVis::X << SciDAVis::noDesignation << SciDAVis::noDesignation
                    << SciDAVis::Y << SciDAVis::noDesignation << SciDAVis::noDesignation
                    << SciDAVis::Y << SciDAVis::X << SciDAVis::Y << SciDAVis::Y
                    << SciDAVis::noDesignation << SciDAVis::noDesignation,
            { col2Th, colD, colQ, colInt, colFwhmObs, colFwhmCorr, colSch,
              colWhX, colWhY, colWhFit, colWhLo, colWhHi });
    res.peakTable = tbl;
    if (tbl) {
        if (Column *c0 = tbl->column(0)) {
            c0->setLongName(QStringLiteral("2<i>\u03B8</i>"));
            c0->setUnits(QStringLiteral("deg"));
        }
        if (Column *c1 = tbl->column(1)) {
            c1->setLongName(QStringLiteral("<i>d</i>-spacing"));
            c1->setUnits(QStringLiteral("\u00C5"));
        }
        if (Column *c6 = tbl->column(6)) {
            c6->setLongName(QStringLiteral("Scherrer <i>D</i>"));
            c6->setUnits(QStringLiteral("nm"));
        }
        if (Column *c7 = tbl->column(7))
            c7->setLongName(QStringLiteral("4 sin <i>\u03B8</i>"));
        if (Column *c8 = tbl->column(8)) {
            c8->setLongName(QStringLiteral("<i>\u03B2</i> cos <i>\u03B8</i>"));
            c8->setUnits(QStringLiteral("rad"));
            c8->setComment(QObject::tr("Measured"));
        }
        if (Column *c9 = tbl->column(9)) {
            c9->setLongName(QStringLiteral("<i>\u03B2</i> cos <i>\u03B8</i>"));
            c9->setUnits(QStringLiteral("rad"));
            c9->setComment(QObject::tr("Linear fit"));
        }

        // Append text (hkl) column when indexed
        Column *cHkl = new Column(QStringLiteral("hkl"), SciDAVis::ColumnMode::Text);
        cHkl->setPlotDesignation(SciDAVis::noDesignation);
        for (int i = 0; i < M; ++i)
            cHkl->setTextAt(i, pks[size_t(i)].pub.hklLabel);
        if (tbl->d_future_table)
            tbl->d_future_table->appendColumns(QList<Column *>() << cHkl);
        else
            delete cHkl;
    }

    // 9. Optional Williamson-Hall plot (β cos θ vs 4 sin θ)
    if (createWilliamsonHallPlot && tbl && M >= 2) {
        MultiLayer *mlWh = app->newGraph(makeUniqueTableName(app, QStringLiteral("WilliamsonHallPlot")));
        res.whPlot = mlWh;
        if (mlWh && mlWh->activeGraph()) {
            Graph *gw = mlWh->activeGraph();
            if (whPool.size() > 2 && whSResid > 0.0) {
                QVector<double> vx(M), vyLo(M), vyHi(M);
                for (int i = 0; i < M; ++i) {
                    vx[i] = colWhX[size_t(i)];
                    vyLo[i] = colWhLo[size_t(i)];
                    vyHi[i] = colWhHi[size_t(i)];
                }
                if (QwtPlotItem *band =
                            gw->addShadedBand(vx, vyLo, vyHi, QColor(205, 35, 45, 36), QString())) {
                    band->setZ(1.0);
                    band->setItemAttribute(QwtPlotItem::Legend, false);
                }
            }
            if (gw->insertCurve(tbl, tbl->colName(7), tbl->colName(8), Graph::Scatter, 0, M - 1)) {
                const int idx = gw->curves() - 1;
                gw->setCurvePen(idx, QPen(QColor(25, 65, 135), 1.5));
                gw->setCurveSymbol(idx,
                                   QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(35, 95, 185)),
                                             QPen(QColor(15, 45, 95), 1.2), QSize(8, 8)));
            }
            insertStyledCurve(gw, tbl, 7, 9, M, QPen(QColor(205, 35, 45), 2.1, Qt::SolidLine));

            gw->applyFramePreset(Graph::FrameBoxInwardTicks);
            gw->setXAxisTitle(QStringLiteral("4 sin <i>\u03B8</i>"));
            gw->setYAxisTitle(QStringLiteral("<i>\u03B2</i> cos <i>\u03B8</i> / rad"));
            gw->setTitle(QString());
            if (gw->legend())
                gw->legend()->setText(gw->legendText());
            gw->setAutoScale();
            applyArialToMatSciGraph(gw);
            gw->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);

            const QString whStr = std::isfinite(res.whSizeNm)
                    ? QString::number(res.whSizeNm, 'f', 1) + QStringLiteral(" nm")
                    : QStringLiteral("N/A (res-limited)");
            const QString schStr = std::isfinite(res.meanScherrerSizeNm)
                    ? QString::number(res.meanScherrerSizeNm, 'f', 1) + QStringLiteral(" nm")
                    : QStringLiteral("N/A");
            QString summary = QStringLiteral(
                    "<i>D</i><sub>W-H</sub> = %1 (<i>D</i><sub>Sch</sub> = %2)\n"
                    "<i>\u03B5</i> = %3 \u00D7 10<sup>-3</sup>, <i>R</i><sup>2</sup> = %4")
                    .arg(whStr, schStr)
                    .arg(res.whMicrostrain * 1e3, 0, 'f', 2)
                    .arg(res.whRSquare, 0, 'f', 4);
            if (res.latticeParamAngstrom > 0.0) {
                summary += QStringLiteral("\n<i>a</i><sub>0</sub> = %1 \u00C5")
                                   .arg(res.latticeParamAngstrom, 0, 'f', 4);
            }
            const double xSpan = std::max(1e-6, colWhX.back() - colWhX.front());
            const auto yMM = std::minmax_element(colWhY.begin(), colWhY.end());
            const double ySpan = std::max(1e-6, *yMM.second - *yMM.first);
            addMatSciCalloutBox(gw, colWhX.front() + 0.06 * xSpan,
                                *yMM.first + 0.72 * ySpan, summary);
            gw->replot();
        }
    }

    app->modifiedProject();
    return res;
}

void CurveMathTools::showXRDAnalyzerDialog(ApplicationWindow *app)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    registerMatSciTranslationsOnce();

    QList<Graph *> graphs;
    Graph *activeG = findActiveOrFirstGraph(app, nullptr);
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer")) {
            if (Graph *g = static_cast<MultiLayer *>(w)->activeGraph()) {
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
                app, QObject::tr("XRD Peak, Scherrer & Williamson-Hall Analyzer"),
                QObject::tr("Please open a graph or numeric worksheet with XRD data first."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("XRD Peak, Scherrer & Williamson-Hall Analyzer"));
    dlg.setMinimumWidth(540);
    QFormLayout *form = new QFormLayout();

    QComboBox *cbSource = new QComboBox(&dlg);
    if (!graphs.isEmpty())
        cbSource->addItem(QObject::tr("Active Graph Curve"), 0);
    if (!tables.isEmpty())
        cbSource->addItem(QObject::tr("Worksheet Columns (Plot & Analyze)"), 1);

    QComboBox *cbGraph = new QComboBox(&dlg);
    QComboBox *cbCurve = new QComboBox(&dlg);
    int initG = 0;
    for (int i = 0; i < graphs.size(); ++i) {
        MultiLayer *ml = graphs[i]->multiLayerParent();
        cbGraph->addItem(ml ? ml->name() : graphs[i]->parentPlotName());
        if (graphs[i] == activeG)
            initG = i;
    }
    cbGraph->setCurrentIndex(initG);
    auto fillCurves = [&](int idx) {
        cbCurve->clear();
        if (idx >= 0 && idx < graphs.size())
            cbCurve->addItems(graphs[idx]->analysableCurvesList());
    };
    fillCurves(initG);
    QObject::connect(cbGraph, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     fillCurves);

    QComboBox *cbTable = new QComboBox(&dlg);
    QComboBox *cbTwoTheta = new QComboBox(&dlg);
    QComboBox *cbInten = new QComboBox(&dlg);
    Table *actTbl = findActiveOrFirstTable(app);
    int initT = 0;
    for (int i = 0; i < tables.size(); ++i) {
        cbTable->addItem(tables[i]->name());
        if (tables[i] == actTbl)
            initT = i;
    }
    cbTable->setCurrentIndex(initT);
    auto fillCols = [&](int idx) {
        cbTwoTheta->clear();
        cbInten->clear();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            if (matSciColumnHasNumeric(t->column(c))) {
                const QString cn = t->column(c)->name();
                cbTwoTheta->addItem(cn);
                cbInten->addItem(cn);
            }
        }
        if (cbInten->count() >= 2)
            cbInten->setCurrentIndex(1);
    };
    fillCols(initT);
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, fillCols);

    QLabel *lblG = new QLabel(QObject::tr("Target Graph:"), &dlg);
    QLabel *lblC = new QLabel(QObject::tr("Diffractogram Curve:"), &dlg);
    QLabel *lblT = new QLabel(QObject::tr("Source Worksheet:"), &dlg);
    QLabel *lblX = new QLabel(QObject::tr("2θ Column (deg):"), &dlg);
    QLabel *lblY = new QLabel(QObject::tr("Intensity Column:"), &dlg);

    auto syncSrc = [&]() {
        const bool fromTbl = (cbSource->currentData().toInt() == 1);
        lblG->setVisible(!fromTbl);
        cbGraph->setVisible(!fromTbl);
        lblC->setVisible(!fromTbl);
        cbCurve->setVisible(!fromTbl);
        lblT->setVisible(fromTbl);
        cbTable->setVisible(fromTbl);
        lblX->setVisible(fromTbl);
        cbTwoTheta->setVisible(fromTbl);
        lblY->setVisible(fromTbl);
        cbInten->setVisible(fromTbl);
    };
    QObject::connect(cbSource, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { syncSrc(); });

    QComboBox *cbAnode = new QComboBox(&dlg);
    cbAnode->addItem(QStringLiteral("Cu K\u03B11 (1.540598 \u00C5)"), 1.540598);
    cbAnode->addItem(QStringLiteral("Cu K\u03B1 avg (1.541838 \u00C5)"), 1.541838);
    cbAnode->addItem(QStringLiteral("Co K\u03B11 (1.788965 \u00C5)"), 1.788965);
    cbAnode->addItem(QStringLiteral("Fe K\u03B11 (1.936040 \u00C5)"), 1.936040);
    cbAnode->addItem(QStringLiteral("Cr K\u03B11 (2.289700 \u00C5)"), 2.289700);
    cbAnode->addItem(QStringLiteral("Mo K\u03B11 (0.709319 \u00C5)"), 0.709319);

    QDoubleSpinBox *sbLambda = new QDoubleSpinBox(&dlg);
    sbLambda->setRange(0.05, 10.0);
    sbLambda->setDecimals(6);
    sbLambda->setValue(1.540598);
    QObject::connect(cbAnode, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { sbLambda->setValue(cbAnode->currentData().toDouble()); });

    QDoubleSpinBox *sbInst = new QDoubleSpinBox(&dlg);
    sbInst->setRange(0.0, 2.0);
    sbInst->setDecimals(4);
    sbInst->setSingleStep(0.01);
    sbInst->setValue(0.0500);

    QDoubleSpinBox *sbK = new QDoubleSpinBox(&dlg);
    sbK->setRange(0.50, 1.50);
    sbK->setDecimals(2);
    sbK->setSingleStep(0.02);
    sbK->setValue(0.90);

    QComboBox *cbCrystal = new QComboBox(&dlg);
    cbCrystal->addItem(QStringLiteral("FCC (111, 200, 220, 311...)"), QStringLiteral("fcc"));
    cbCrystal->addItem(QStringLiteral("BCC (110, 200, 211, 220...)"), QStringLiteral("bcc"));
    cbCrystal->addItem(QObject::tr("HCP / Hexagonal (100, 002, 101...)"), QStringLiteral("hcp"));
    cbCrystal->addItem(QStringLiteral("Simple Cubic (100, 110, 111...)"), QStringLiteral("sc"));
    cbCrystal->addItem(QObject::tr("None (Peak Table & W-H Only)"), QStringLiteral("none"));

    QDoubleSpinBox *sbCOverA = new QDoubleSpinBox(&dlg);
    sbCOverA->setRange(0.50, 4.00);
    sbCOverA->setDecimals(4);
    sbCOverA->setSingleStep(0.01);
    sbCOverA->setValue(1.6330);
    sbCOverA->setEnabled(false);
    QObject::connect(cbCrystal, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) {
                         sbCOverA->setEnabled(cbCrystal->currentData().toString()
                                              == QLatin1String("hcp"));
                     });

    QCheckBox *chkStripKa2 =
            new QCheckBox(QObject::tr("Strip Cu Kα2 Doublet (Rachinger)"), &dlg);
    chkStripKa2->setChecked(false);

    QCheckBox *chkTopD =
            new QCheckBox(QObject::tr("Add Linked Top Bragg d-Spacing Axis (Å)"), &dlg);
    chkTopD->setChecked(true);

    QCheckBox *chkWH = new QCheckBox(
            QObject::tr("Create Williamson-Hall Plot (β cos θ vs 4 sin θ)"), &dlg);
    chkWH->setChecked(true);

    if (cbSource->count() > 1)
        form->addRow(QObject::tr("Data Source:"), cbSource);
    form->addRow(lblG, cbGraph);
    form->addRow(lblC, cbCurve);
    form->addRow(lblT, cbTable);
    form->addRow(lblX, cbTwoTheta);
    form->addRow(lblY, cbInten);
    form->addRow(QObject::tr("Radiation Source:"), cbAnode);
    form->addRow(QObject::tr("X-Ray Wavelength λ (Å):"), sbLambda);
    form->addRow(QObject::tr("Instrumental FWHM (deg):"), sbInst);
    form->addRow(QObject::tr("Scherrer Shape Factor K:"), sbK);
    form->addRow(QObject::tr("Crystal System for (hkl) Indexing:"), cbCrystal);
    form->addRow(QObject::tr("Axial Ratio c/a (for HCP):"), sbCOverA);
    form->addRow(QString(), chkStripKa2);
    form->addRow(QString(), chkTopD);
    form->addRow(QString(), chkWH);
    syncSrc();

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    Graph *chosenG = nullptr;
    QString chosenC;
    if (cbSource->currentData().toInt() == 1) {
        Table *t = tables[cbTable->currentIndex()];
        MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("XRDPlot")));
        if (!ml || !ml->activeGraph())
            return;
        chosenG = ml->activeGraph();
        const QString fx = t->name() + QStringLiteral("_") + cbTwoTheta->currentText();
        const QString fy = t->name() + QStringLiteral("_") + cbInten->currentText();
        if (chosenG->insertCurve(t, fx, fy, Graph::Line)) {
            chosenG->setCurvePen(chosenG->curves() - 1,
                                 QPen(QColor(25, 45, 85), 1.6, Qt::SolidLine));
            chosenC = fy;
        }
        chosenG->applyFramePreset(Graph::FrameBoxInwardTicks);
        chosenG->setXAxisTitle(QStringLiteral("Diffraction angle, 2<i>\u03B8</i> / deg"));
        chosenG->setYAxisTitle(QStringLiteral("Intensity, <i>I</i> / counts"));
        chosenG->setTitle(QString());
        chosenG->setAutoScale();
        applyArialToMatSciGraph(chosenG);
        chosenG->replot();
    } else {
        chosenG = graphs[cbGraph->currentIndex()];
        chosenC = cbCurve->currentText();
    }

    const XRDAnalysisResult xr = analyzeXRD(
            app, chosenG, chosenC, sbLambda->value(), sbInst->value(), sbK->value(),
            cbCrystal->currentData().toString(), chkStripKa2->isChecked(),
            chkTopD->isChecked(), chkWH->isChecked(), sbCOverA->value());
    if (!xr.valid && !ApplicationWindow::isNonInteractiveContext(app)) {
        QMessageBox::warning(app, QObject::tr("XRD Peak, Scherrer & Williamson-Hall Analyzer"),
                             xr.error);
    }
}

// ============================================================================
// 5. VASP Electronic Structure Helpers (DOS / PDOS & Band Structure)
// ============================================================================

namespace {

class HorizontalShadedDosItem : public QwtPlotItem
{
public:
    HorizontalShadedDosItem(const QVector<double> &dosX, const QVector<double> &energyY,
                            const QColor &fillColor)
        : d_dosX(dosX), d_energyY(energyY), d_color(fillColor)
    {
        setZ(1.0);
        setItemAttribute(QwtPlotItem::Legend, false);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 77; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &) const override
    {
        const int n = std::min(d_dosX.size(), d_energyY.size());
        if (!painter || n < 2)
            return;
        const double px0 = xMap.xTransform(0.0);
        QPolygonF poly;
        poly.reserve(n + 2);
        poly.append(QPointF(px0, yMap.xTransform(d_energyY.front())));
        for (int i = 0; i < n; ++i)
            poly.append(QPointF(xMap.xTransform(d_dosX[i]), yMap.xTransform(d_energyY[i])));
        poly.append(QPointF(px0, yMap.xTransform(d_energyY.back())));
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(d_color));
        painter->drawPolygon(poly);
        painter->restore();
    }

private:
    QVector<double> d_dosX;
    QVector<double> d_energyY;
    QColor d_color;
};

class HighSymmetryKScaleDraw : public ScaleDraw
{
public:
    HighSymmetryKScaleDraw(const ScaleDraw &base, const QVector<double> &positions,
                           const QStringList &labels, double span)
        : ScaleDraw(base),
          d_positions(positions),
          d_labels(labels),
          d_tol(std::max(1e-5, 0.008 * std::max(1e-4, span)))
    {
    }

    QwtText label(double value) const override
    {
        int bestIdx = -1;
        double bestDist = d_tol;
        const int n = std::min(d_positions.size(), d_labels.size());
        for (int i = 0; i < n; ++i) {
            const double d = std::fabs(value - d_positions[i]);
            if (d <= bestDist) {
                bestDist = d;
                bestIdx = i;
            }
        }
        if (bestIdx >= 0) {
            QwtText txt(d_labels[bestIdx]);
            return txt;
        }
        return QwtText(QString());
    }

private:
    QVector<double> d_positions;
    QStringList d_labels;
    double d_tol = 1e-3;
};

QString normalizeKPointLabel(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.compare(QLatin1String("\\Gamma"), Qt::CaseInsensitive) == 0
        || s.compare(QLatin1String("GAMMA"), Qt::CaseInsensitive) == 0
        || s == QLatin1String("G")) {
        return QStringLiteral("\u0393");
    }
    s.replace(QRegularExpression(QStringLiteral("\\\\Gamma\\b"),
                                 QRegularExpression::CaseInsensitiveOption),
              QStringLiteral("\u0393"));
    return s;
}

bool extractAutoVaspFermiFromColumn(const Column *col, double &outFermi)
{
    if (!col)
        return false;
    const QString cmt = col->comment().trimmed();
    if (cmt.startsWith(QLatin1String("VASP_EFERMI="))) {
        bool ok = false;
        const double v = cmt.mid(12).trimmed().toDouble(&ok);
        if (ok && std::isfinite(v)) {
            outFermi = v;
            return true;
        }
    }
    return false;
}

bool isDOrbitalColumnName(const Column *col)
{
    if (!col)
        return false;
    static const QRegularExpression kDRegex(
            QStringLiteral("(^|[^a-zA-Z0-9])([345]?[dD])([^a-zA-Z0-9]|$)"));
    return kDRegex.match(col->name()).hasMatch() || kDRegex.match(col->longName()).hasMatch();
}

Table *importVaspFileAsProjectTable(ApplicationWindow *app, const QString &filePath,
                                    double *outFermiEV)
{
    if (outFermiEV)
        *outFermiEV = 0.0;
    if (!app || filePath.trimmed().isEmpty())
        return nullptr;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return nullptr;
    const QByteArray raw = f.readAll();
    f.close();

    const QString baseStem = QFileInfo(filePath).baseName().isEmpty()
            ? QStringLiteral("VASPData")
            : QFileInfo(filePath).baseName();
    const QString uniqueName = app->generateUniqueName(baseStem);
    double ef = 0.0;
    future::Table *ft = AsciiTableImportFilter::importVaspTextData(raw, uniqueName, filePath, &ef);
    if (!ft)
        ft = AsciiTableImportFilter::importLabTextData(raw, uniqueName, filePath);
    if (!ft)
        return nullptr;

    QList<Column *> clonedCols;
    for (int i = 0; i < ft->columnCount(); ++i) {
        if (Column *src = ft->column(i)) {
            Column *dst = new Column(src->name(), src->columnMode());
            dst->copy(src);
            dst->setPlotDesignation(src->plotDesignation());
            dst->setLongName(src->longName());
            dst->setUnits(src->units());
            dst->setComment(src->comment());
            clonedCols.append(dst);
        }
    }
    const QString commentLabel = ft->comment().isEmpty() ? QFileInfo(filePath).fileName() : ft->comment();
    delete ft;
    if (clonedCols.isEmpty())
        return nullptr;

    if (outFermiEV)
        *outFermiEV = ef;
    return app->newTable(uniqueName, commentLabel, clonedCols);
}

} // namespace

CurveMathTools::VaspPlotResult CurveMathTools::plotVaspDos(
        ApplicationWindow *app, Table *srcTable, const QString &energyCol,
        const QStringList &dosCols, const QStringList &downDosCols, double fermiEnergyEV,
        bool verticalEnergyAxis)
{
    registerMatSciTranslationsOnce();
    VaspPlotResult res;
    if (!srcTable) {
        res.error = QObject::tr("Source worksheet is null.");
        return res;
    }

    Column *cE = energyCol.trimmed().isEmpty() ? srcTable->column(0)
                                               : resolveMatSciColumn(srcTable, energyCol);
    if (!cE) {
        res.error = QObject::tr("Energy column could not be resolved.");
        return res;
    }

    QList<Column *> upCols;
    QList<Column *> dnCols;
    for (const QString &nm : dosCols) {
        if (Column *c = resolveMatSciColumn(srcTable, nm))
            upCols.append(c);
    }
    for (const QString &nm : downDosCols) {
        if (Column *c = resolveMatSciColumn(srcTable, nm))
            dnCols.append(c);
    }
    if (upCols.isEmpty()) {
        for (int i = 0; i < srcTable->numCols(); ++i) {
            Column *c = srcTable->column(i);
            if (!c || c == cE || !matSciColumnHasNumeric(c))
                continue;
            const QString lo = c->name().toLower();
            if (downDosCols.isEmpty()
                && (lo.endsWith(QLatin1String("_down")) || lo.endsWith(QLatin1String("_dw"))
                    || lo.endsWith(QLatin1String("_dn")))) {
                dnCols.append(c);
            } else {
                upCols.append(c);
            }
        }
    }
    if (upCols.isEmpty()) {
        res.error = QObject::tr("At least one numeric DOS column is required.");
        return res;
    }

    double effFermi = std::isfinite(fermiEnergyEV) ? fermiEnergyEV : 0.0;
    if (effFermi == 0.0)
        extractAutoVaspFermiFromColumn(cE, effFermi);
    res.fermiEnergyEV = effFermi;

    int nRows = cE->rowCount();
    for (Column *c : upCols)
        nRows = std::min(nRows, c->rowCount());
    for (Column *c : dnCols)
        nRows = std::min(nRows, c->rowCount());

    struct DosRow {
        double eShift = 0.0;
        std::vector<double> up;
        std::vector<double> dn;
    };
    std::vector<DosRow> rows;
    rows.reserve(size_t(std::max(0, nRows)));
    for (int r = 0; r < nRows; ++r) {
        double ev = 0.0;
        if (!readMatSciCell(cE, r, ev))
            continue;
        DosRow dr;
        dr.eShift = ev - effFermi;
        dr.up.resize(size_t(upCols.size()), 0.0);
        dr.dn.resize(size_t(dnCols.size()), 0.0);
        bool ok = true;
        for (int j = 0; j < upCols.size(); ++j) {
            if (!readMatSciCell(upCols[j], r, dr.up[size_t(j)])) {
                ok = false;
                break;
            }
        }
        if (!ok)
            continue;
        for (int j = 0; j < dnCols.size(); ++j) {
            double dv = 0.0;
            if (!readMatSciCell(dnCols[j], r, dv)) {
                ok = false;
                break;
            }
            dr.dn[size_t(j)] = -std::fabs(dv);
        }
        if (ok)
            rows.push_back(std::move(dr));
    }
    if (rows.size() < 3) {
        res.error = QObject::tr("Insufficient numeric rows for VASP DOS plot.");
        return res;
    }

    std::sort(rows.begin(), rows.end(),
              [](const DosRow &a, const DosRow &b) { return a.eShift < b.eShift; });
    const int N = static_cast<int>(rows.size());

    // Compute bandgap around E - E_F = 0 and Hammer-Norskov d-band center if a d-PDOS column exists
    std::vector<double> totDos(size_t(N), 0.0);
    double maxNearFermi = 0.0;
    for (int i = 0; i < N; ++i) {
        double s = std::fabs(rows[size_t(i)].up[0]);
        if (!rows[size_t(i)].dn.empty())
            s += std::fabs(rows[size_t(i)].dn[0]);
        totDos[size_t(i)] = s;
        if (std::fabs(rows[size_t(i)].eShift) <= 8.0)
            maxNearFermi = std::max(maxNearFermi, s);
    }
    const double dosThr = 0.01 * std::max(1e-6, maxNearFermi);
    // Interpolate DOS at E' = 0
    double dosAtZero = 0.0;
    if (rows.front().eShift <= 0.0 && rows.back().eShift >= 0.0) {
        auto it = std::lower_bound(rows.begin(), rows.end(), 0.0,
                                   [](const DosRow &r, double val) { return r.eShift < val; });
        const int idx = static_cast<int>(it - rows.begin());
        if (idx == 0) {
            dosAtZero = totDos[0];
        } else if (idx < N) {
            const double e0 = rows[size_t(idx - 1)].eShift;
            const double e1 = rows[size_t(idx)].eShift;
            const double t = (e1 > e0) ? (-e0 / (e1 - e0)) : 0.0;
            dosAtZero = totDos[size_t(idx - 1)] + t * (totDos[size_t(idx)] - totDos[size_t(idx - 1)]);
        }
    }
    if (dosAtZero <= dosThr) {
        double vbm = -std::numeric_limits<double>::infinity();
        double cbm = std::numeric_limits<double>::infinity();
        for (int i = 0; i < N; ++i) {
            if (rows[size_t(i)].eShift <= 0.0 && totDos[size_t(i)] > dosThr)
                vbm = std::max(vbm, rows[size_t(i)].eShift);
            if (rows[size_t(i)].eShift >= 0.0 && totDos[size_t(i)] > dosThr)
                cbm = std::min(cbm, rows[size_t(i)].eShift);
        }
        if (std::isfinite(vbm) && std::isfinite(cbm) && cbm > vbm)
            res.bandgapEV = cbm - vbm;
    }

    int dUpIdx = -1;
    for (int j = 0; j < upCols.size(); ++j) {
        if (isDOrbitalColumnName(upCols[j])) {
            dUpIdx = j;
            break;
        }
    }
    int dDnIdx = -1;
    for (int j = 0; j < dnCols.size(); ++j) {
        if (isDOrbitalColumnName(dnCols[j])) {
            dDnIdx = j;
            break;
        }
    }
    if (dUpIdx >= 0) {
        double num = 0.0, den = 0.0;
        for (int i = 1; i < N; ++i) {
            const double de = rows[size_t(i)].eShift - rows[size_t(i - 1)].eShift;
            if (de <= 0.0)
                continue;
            const double eMid = 0.5 * (rows[size_t(i)].eShift + rows[size_t(i - 1)].eShift);
            double rho0 = std::max(0.0, rows[size_t(i - 1)].up[size_t(dUpIdx)]);
            double rho1 = std::max(0.0, rows[size_t(i)].up[size_t(dUpIdx)]);
            if (dDnIdx >= 0) {
                rho0 += std::fabs(rows[size_t(i - 1)].dn[size_t(dDnIdx)]);
                rho1 += std::fabs(rows[size_t(i)].dn[size_t(dDnIdx)]);
            }
            const double rhoMid = 0.5 * (rho0 + rho1);
            num += eMid * rhoMid * de;
            den += rhoMid * de;
        }
        if (den > 1e-12)
            res.dBandCenterEV = num / den;
    }

    res.valid = true;
    if (!app)
        return res;

    // Build VaspDOSData worksheet
    QStringList colNames;
    QList<SciDAVis::PlotDesignation> desigs;
    std::vector<std::vector<double>> colsData;

    colNames << QStringLiteral("E_minus_EF");
    desigs << (verticalEnergyAxis ? SciDAVis::Y : SciDAVis::X);
    std::vector<double> colEMinusEf(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        colEMinusEf[size_t(i)] = rows[size_t(i)].eShift;
    colsData.push_back(std::move(colEMinusEf));

    for (int j = 0; j < upCols.size(); ++j) {
        colNames << upCols[j]->name();
        desigs << (verticalEnergyAxis ? SciDAVis::X : SciDAVis::Y);
        std::vector<double> v(static_cast<size_t>(N));
        for (int i = 0; i < N; ++i)
            v[size_t(i)] = rows[size_t(i)].up[size_t(j)];
        colsData.push_back(std::move(v));
    }
    for (int j = 0; j < dnCols.size(); ++j) {
        QString nm = dnCols[j]->name();
        if (colNames.contains(nm))
            nm += QStringLiteral("_dn");
        colNames << nm;
        desigs << (verticalEnergyAxis ? SciDAVis::X : SciDAVis::Y);
        std::vector<double> v(static_cast<size_t>(N));
        for (int i = 0; i < N; ++i)
            v[size_t(i)] = rows[size_t(i)].dn[size_t(j)];
        colsData.push_back(std::move(v));
    }

    Table *tbl = createNumericTable(app, QStringLiteral("VaspDOSData"), colNames, desigs, colsData);
    res.dataTable = tbl;
    if (!tbl)
        return res;

    if (Column *c0 = tbl->column(0)) {
        c0->setLongName(QStringLiteral("<i>E</i> - <i>E</i><sub>F</sub>"));
        c0->setUnits(QStringLiteral("eV"));
    }
    for (int j = 0; j < upCols.size(); ++j) {
        if (Column *cj = tbl->column(1 + j)) {
            cj->setComment(upCols[j]->longName().isEmpty() ? upCols[j]->name() : upCols[j]->longName());
            cj->setUnits(QStringLiteral("states eV<sup>-1</sup>"));
        }
    }
    for (int j = 0; j < dnCols.size(); ++j) {
        if (Column *cj = tbl->column(1 + upCols.size() + j)) {
            cj->setComment(dnCols[j]->longName().isEmpty() ? dnCols[j]->name() : dnCols[j]->longName());
            cj->setUnits(QStringLiteral("states eV<sup>-1</sup>"));
        }
    }

    MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("VaspDOSPlot")));
    res.plot = ml;
    if (ml && ml->activeGraph()) {
        Graph *g = ml->activeGraph();
        const QColor palette[6] = {
            QColor(25, 45, 85),    // Total / 1st channel (deep slate)
            QColor(0, 114, 178),   // s / 2nd (Okabe-Ito blue)
            QColor(0, 158, 115),   // p / 3rd (Okabe-Ito bluish green)
            QColor(213, 94, 0),    // d / 4th (Okabe-Ito vermilion)
            QColor(204, 121, 167), // f / 5th
            QColor(230, 159, 0)
        };

        QVector<double> vE(N), vZero(N, 0.0);
        for (int i = 0; i < N; ++i)
            vE[i] = rows[size_t(i)].eShift;

        QStringList customLegLines;
        auto plotChannel = [&](int tblColIdx, const QColor &col, Qt::PenStyle sty) {
            QVector<double> vDos(N);
            for (int i = 0; i < N; ++i)
                vDos[i] = tbl->column(tblColIdx)->valueAt(i);
            QColor fillCol = col;
            fillCol.setAlpha(42);

            if (!verticalEnergyAxis) {
                if (QwtPlotItem *band = g->addShadedBand(vE, vZero, vDos, fillCol, QString())) {
                    band->setZ(1.0);
                    band->setItemAttribute(QwtPlotItem::Legend, false);
                }
                if (insertStyledCurve(g, tbl, 0, tblColIdx, N, QPen(col, 1.8, sty))) {
                    Column *cDos = tbl->column(tblColIdx);
                    const QString lbl = (cDos && !cDos->comment().trimmed().isEmpty())
                            ? cDos->comment().trimmed()
                            : (cDos ? cDos->name() : QString::number(tblColIdx));
                    customLegLines << QStringLiteral("\\c{%1}%2").arg(g->curves()).arg(lbl);
                }
            } else {
                if (Plot *pw = g->plotWidget()) {
                    auto *fillItem = new HorizontalShadedDosItem(vDos, vE, fillCol);
                    fillItem->attach(pw);
                }
                if (g->insertCurve(tbl, tbl->colName(tblColIdx), tbl->colName(0), Graph::Line, 0,
                                   N - 1)) {
                    const int cIdx = g->curves() - 1;
                    g->setCurvePen(cIdx, QPen(col, 1.8, sty));
                    Column *cDos = tbl->column(tblColIdx);
                    const QString lbl = (cDos && !cDos->comment().trimmed().isEmpty())
                            ? cDos->comment().trimmed()
                            : (cDos ? cDos->name() : QString::number(tblColIdx));
                    customLegLines << QStringLiteral("\\c{%1}%2").arg(cIdx + 1).arg(lbl);
                }
            }
        };

        for (int j = 0; j < upCols.size(); ++j)
            plotChannel(1 + j, palette[j % 6], Qt::SolidLine);
        for (int j = 0; j < dnCols.size(); ++j)
            plotChannel(1 + upCols.size() + j, palette[j % 6], Qt::DashLine);

        Graph::ReferenceLineSpec efSpec;
        efSpec.orientation = verticalEnergyAxis ? 0 : 1;
        efSpec.value = 0.0;
        efSpec.color = QColor(100, 116, 139);
        efSpec.style = Qt::DashLine;
        efSpec.width = 1.3;
        g->addReferenceLine(efSpec);

        if (!dnCols.isEmpty()) {
            Graph::ReferenceLineSpec zeroDos;
            zeroDos.orientation = verticalEnergyAxis ? 1 : 0;
            zeroDos.value = 0.0;
            zeroDos.color = QColor(148, 163, 184);
            zeroDos.style = Qt::SolidLine;
            zeroDos.width = 1.0;
            g->addReferenceLine(zeroDos);
        }

        g->applyFramePreset(Graph::FrameBoxInwardTicks);
        if (!verticalEnergyAxis) {
            g->setXAxisTitle(QStringLiteral("Energy, <i>E</i> - <i>E</i><sub>F</sub> / eV"));
            g->setYAxisTitle(QStringLiteral("DOS / states eV<sup>-1</sup>"));
        } else {
            g->setXAxisTitle(QStringLiteral("DOS / states eV<sup>-1</sup>"));
            g->setYAxisTitle(QStringLiteral("Energy, <i>E</i> - <i>E</i><sub>F</sub> / eV"));
        }
        g->setTitle(QString());
        if (g->legend()) {
            g->legend()->setText(customLegLines.isEmpty() ? g->legendText()
                                                          : customLegLines.join(QLatin1Char('\n')));
        }
        g->setAutoScale();
        applyArialToMatSciGraph(g);
        g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
        g->replot();
    }

    app->modifiedProject();
    return res;
}

CurveMathTools::VaspPlotResult CurveMathTools::plotVaspBands(
        ApplicationWindow *app, Table *srcTable, const QString &kDistCol,
        const QStringList &bandCols, const QVector<double> &kTickPositions,
        const QStringList &kTickLabels, double fermiEnergyEV, double energyMinEV,
        double energyMaxEV)
{
    registerMatSciTranslationsOnce();
    VaspPlotResult res;
    if (!srcTable) {
        res.error = QObject::tr("Source worksheet is null.");
        return res;
    }

    Column *cK = kDistCol.trimmed().isEmpty() ? srcTable->column(0)
                                              : resolveMatSciColumn(srcTable, kDistCol);
    if (!cK) {
        res.error = QObject::tr("Wavevector k-distance column could not be resolved.");
        return res;
    }

    QList<Column *> bCols;
    for (const QString &nm : bandCols) {
        if (Column *c = resolveMatSciColumn(srcTable, nm))
            bCols.append(c);
    }
    if (bCols.isEmpty()) {
        for (int i = 0; i < srcTable->numCols(); ++i) {
            Column *c = srcTable->column(i);
            if (c && c != cK && matSciColumnHasNumeric(c))
                bCols.append(c);
        }
    }
    if (bCols.isEmpty()) {
        res.error = QObject::tr("At least one band eigenvalue column is required.");
        return res;
    }

    double effFermi = std::isfinite(fermiEnergyEV) ? fermiEnergyEV : 0.0;
    if (effFermi == 0.0)
        extractAutoVaspFermiFromColumn(cK, effFermi);
    res.fermiEnergyEV = effFermi;

    int nRows = cK->rowCount();
    for (Column *c : bCols)
        nRows = std::min(nRows, c->rowCount());

    const int B = bCols.size();
    std::vector<double> kVec;
    std::vector<std::vector<double>> eBands(static_cast<size_t>(B));
    kVec.reserve(size_t(std::max(0, nRows)));
    for (int b = 0; b < B; ++b)
        eBands[size_t(b)].reserve(size_t(std::max(0, nRows)));

    for (int r = 0; r < nRows; ++r) {
        double kv = 0.0;
        if (!readMatSciCell(cK, r, kv))
            continue;
        std::vector<double> eb(size_t(B), 0.0);
        bool ok = true;
        for (int b = 0; b < B; ++b) {
            double ev = 0.0;
            if (!readMatSciCell(bCols[b], r, ev)) {
                ok = false;
                break;
            }
            eb[size_t(b)] = ev - effFermi;
        }
        if (!ok)
            continue;
        kVec.push_back(kv);
        for (int b = 0; b < B; ++b)
            eBands[size_t(b)].push_back(eb[size_t(b)]);
    }

    const int K = static_cast<int>(kVec.size());
    if (K < 2) {
        res.error = QObject::tr("Insufficient k-points for VASP band structure plot.");
        return res;
    }

    // Compute VBM, CBM, and fundamental bandgap
    bool metallic = false;
    double vbm = -std::numeric_limits<double>::infinity();
    double cbm = std::numeric_limits<double>::infinity();
    for (int b = 0; b < B; ++b) {
        const auto mm = std::minmax_element(eBands[size_t(b)].begin(), eBands[size_t(b)].end());
        const double bMin = *mm.first;
        const double bMax = *mm.second;
        if (bMin < -1e-4 && bMax > 1e-4) {
            metallic = true;
            break;
        }
        if (bMax <= 1e-4)
            vbm = std::max(vbm, bMax);
        if (bMin >= -1e-4)
            cbm = std::min(cbm, bMin);
    }
    res.bandgapEV = (!metallic && std::isfinite(vbm) && std::isfinite(cbm) && cbm > vbm)
            ? (cbm - vbm)
            : 0.0;
    res.valid = true;

    if (!app)
        return res;

    QStringList colNames;
    QList<SciDAVis::PlotDesignation> desigs;
    std::vector<std::vector<double>> colsData;
    colNames << QStringLiteral("k_dist");
    desigs << SciDAVis::X;
    colsData.push_back(kVec);

    for (int b = 0; b < B; ++b) {
        QString nm = bCols[b]->name();
        if (colNames.contains(nm))
            nm += QStringLiteral("_%1").arg(b + 1);
        colNames << nm;
        desigs << SciDAVis::Y;
        colsData.push_back(eBands[size_t(b)]);
    }

    Table *tbl = createNumericTable(app, QStringLiteral("VaspBandsData"), colNames, desigs, colsData);
    res.dataTable = tbl;
    if (!tbl)
        return res;

    if (Column *c0 = tbl->column(0))
        c0->setLongName(QStringLiteral("Wavevector, <i>k</i>"));
    for (int b = 0; b < B; ++b) {
        if (Column *cb = tbl->column(1 + b)) {
            cb->setLongName(QStringLiteral("<i>E</i> - <i>E</i><sub>F</sub>"));
            cb->setUnits(QStringLiteral("eV"));
        }
    }

    MultiLayer *ml = app->newGraph(makeUniqueTableName(app, QStringLiteral("VaspBandsPlot")));
    res.plot = ml;
    if (ml && ml->activeGraph()) {
        Graph *g = ml->activeGraph();
        bool hasSpinDown = false;
        int firstUpCurve = -1;
        int firstDnCurve = -1;

        for (int b = 0; b < B; ++b) {
            const QString lo = bCols[b]->name().toLower();
            const bool isDn = lo.endsWith(QLatin1String("_dw"))
                    || lo.endsWith(QLatin1String("_down"))
                    || lo.endsWith(QLatin1String("_dn"));
            const QPen pen = isDn
                    ? QPen(QColor(213, 94, 0), 1.4, Qt::DashLine)
                    : QPen(QColor(29, 78, 216), 1.5, Qt::SolidLine);
            QwtPlotCurve *c = insertStyledCurve(g, tbl, 0, 1 + b, K, pen);
            if (!c)
                continue;
            const int cIdx = g->curves() - 1;
            if (!isDn) {
                if (firstUpCurve < 0)
                    firstUpCurve = cIdx;
                else
                    c->setItemAttribute(QwtPlotItem::Legend, false);
            } else {
                hasSpinDown = true;
                if (firstDnCurve < 0)
                    firstDnCurve = cIdx;
                else
                    c->setItemAttribute(QwtPlotItem::Legend, false);
            }
        }

        const double kMin = *std::min_element(kVec.begin(), kVec.end());
        const double kMax = *std::max_element(kVec.begin(), kVec.end());
        const double yLo = (energyMinEV < energyMaxEV) ? energyMinEV : -6.0;
        const double yHi = (energyMinEV < energyMaxEV) ? energyMaxEV : 6.0;

        // Fermi level E - E_F = 0 dashed horizontal line
        Graph::ReferenceLineSpec efLine;
        efLine.orientation = 0;
        efLine.value = 0.0;
        efLine.color = QColor(100, 116, 139);
        efLine.style = Qt::DashLine;
        efLine.width = 1.2;
        g->addReferenceLine(efLine);

        g->applyFramePreset(Graph::FrameBoxInwardTicks);
        g->setXAxisTitle(QStringLiteral("Wavevector, <i>k</i>"));
        g->setYAxisTitle(QStringLiteral("Energy, <i>E</i> - <i>E</i><sub>F</sub> / eV"));
        g->setTitle(QString());
        g->setScale(QwtPlot::xBottom, kMin, kMax);
        g->setScale(QwtPlot::yLeft, yLo, yHi);

        // Single clean legend entry (or Spin ↑ / Spin ↓ for ISPIN=2)
        if (g->legend()) {
            if (hasSpinDown && firstUpCurve >= 0 && firstDnCurve >= 0) {
                g->legend()->setText(QStringLiteral("\\c{%1}Spin \u2191\n\\c{%2}Spin \u2193")
                                             .arg(firstUpCurve + 1)
                                             .arg(firstDnCurve + 1));
            } else if (firstUpCurve >= 0) {
                g->legend()->setText(QStringLiteral("\\c{%1}Electronic bands").arg(firstUpCurve + 1));
            }
        }

        // High-symmetry k-point ticks + interior vertical zone-boundary lines
        const int nTicks = std::min(kTickPositions.size(), kTickLabels.size());
        if (nTicks >= 2 && g->plotWidget()) {
            QVector<double> cleanPos;
            QStringList cleanLbls;
            for (int i = 0; i < nTicks; ++i) {
                if (!std::isfinite(kTickPositions[i]))
                    continue;
                cleanPos.append(kTickPositions[i]);
                cleanLbls.append(normalizeKPointLabel(kTickLabels[i]));

                if (kTickPositions[i] > kMin + 1e-4 * (kMax - kMin)
                    && kTickPositions[i] < kMax - 1e-4 * (kMax - kMin)) {
                    Graph::ReferenceLineSpec vz;
                    vz.orientation = 1;
                    vz.value = kTickPositions[i];
                    vz.color = QColor(148, 163, 184);
                    vz.style = Qt::SolidLine;
                    vz.width = 0.9;
                    g->addReferenceLine(vz);
                }
            }

            // Persist floating-point k-ticks on Graph (<ToriCategoryAxis>) so reload & presets preserve them
            g->setCustomTickLabels(QwtPlot::xBottom, cleanPos, cleanLbls, 0.0);

            auto installKTicks = [g, cleanPos, cleanLbls, kMin, kMax]() {
                Plot *pw = g->plotWidget();
                if (!pw)
                    return;
                g->setCustomTickLabels(QwtPlot::xBottom, cleanPos, cleanLbls, 0.0);
                QwtValueList ticks[QwtScaleDiv::NTickTypes];
                for (double p : cleanPos)
                    ticks[QwtScaleDiv::MajorTick] << p;
                pw->setAxisScaleDiv(QwtPlot::xBottom, QwtScaleDiv(kMin, kMax, ticks));
            };
            installKTicks();

            auto guard = std::make_shared<bool>(false);
            QObject::connect(g, &Graph::modifiedGraph, g,
                             [gPtr = QPointer<Graph>(g), cleanPos, installKTicks, guard]() {
                                 if (*guard || !gPtr || !gPtr->plotWidget())
                                     return;
                                 const QwtScaleDiv *xd =
                                         gPtr->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
                                 bool sameTicks = false;
                                 if (xd) {
                                     const QwtValueList maj = xd->ticks(QwtScaleDiv::MajorTick);
                                     if (maj.size() == cleanPos.size()) {
                                         sameTicks = true;
                                         for (int i = 0; i < maj.size(); ++i) {
                                             if (std::fabs(maj[i] - cleanPos[i]) > 1e-5) {
                                                 sameTicks = false;
                                                 break;
                                             }
                                         }
                                     }
                                 }
                                 if (sameTicks)
                                     return;
                                 *guard = true;
                                 installKTicks();
                                 *guard = false;
                             });
        }

        applyArialToMatSciGraph(g);
        g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
        g->replot();
    }

    app->modifiedProject();
    return res;
}

void CurveMathTools::showVaspPlotterDialog(ApplicationWindow *app)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    registerMatSciTranslationsOnce();

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("VASP Electronic Structure Plotter (DOS & Band Structure)"));
    dlg.setMinimumWidth(540);
    QVBoxLayout *root = new QVBoxLayout(&dlg);

    QHBoxLayout *topBar = new QHBoxLayout();
    QComboBox *cbTable = new QComboBox(&dlg);
    QPushButton *btnLoadFile =
            new QPushButton(QObject::tr("Load VASP DOSCAR / EIGENVAL File..."), &dlg);
    topBar->addWidget(new QLabel(QObject::tr("Source Worksheet:"), &dlg));
    topBar->addWidget(cbTable, 1);
    topBar->addWidget(btnLoadFile);
    root->addLayout(topBar);

    QTabWidget *tabs = new QTabWidget(&dlg);
    QWidget *tabDos = new QWidget(&dlg);
    QWidget *tabBands = new QWidget(&dlg);
    tabs->addTab(tabDos, QObject::tr("DOS / PDOS"));
    tabs->addTab(tabBands, QObject::tr("Band Structure"));
    root->addWidget(tabs);

    // DOS controls
    QFormLayout *dosForm = new QFormLayout(tabDos);
    QComboBox *cbEnergy = new QComboBox(tabDos);
    QListWidget *lwDosUp = new QListWidget(tabDos);
    lwDosUp->setSelectionMode(QAbstractItemView::MultiSelection);
    lwDosUp->setMaximumHeight(110);
    QListWidget *lwDosDn = new QListWidget(tabDos);
    lwDosDn->setSelectionMode(QAbstractItemView::MultiSelection);
    lwDosDn->setMaximumHeight(90);
    QDoubleSpinBox *sbDosEf = new QDoubleSpinBox(tabDos);
    sbDosEf->setRange(-500.0, 500.0);
    sbDosEf->setDecimals(4);
    QCheckBox *chkVertDos = new QCheckBox(
            QObject::tr("Vertical Energy Axis (for side-by-side Band + DOS)"), tabDos);

    dosForm->addRow(QObject::tr("Energy Column (eV):"), cbEnergy);
    dosForm->addRow(QObject::tr("Spin-Up / Total DOS Columns:"), lwDosUp);
    dosForm->addRow(QObject::tr("Spin-Down DOS Columns (Optional):"), lwDosDn);
    dosForm->addRow(QObject::tr("Fermi Energy E_F (eV):"), sbDosEf);
    dosForm->addRow(QString(), chkVertDos);

    // Band Structure controls
    QFormLayout *bandForm = new QFormLayout(tabBands);
    QComboBox *cbKDist = new QComboBox(tabBands);
    QListWidget *lwBands = new QListWidget(tabBands);
    lwBands->setSelectionMode(QAbstractItemView::MultiSelection);
    lwBands->setMaximumHeight(120);
    QLineEdit *leKPos = new QLineEdit(QStringLiteral("0.0, 1.0, 1.707, 2.707"), tabBands);
    QLineEdit *leKLbl = new QLineEdit(QStringLiteral("\u0393, X, M, \u0393"), tabBands);
    QDoubleSpinBox *sbBandEf = new QDoubleSpinBox(tabBands);
    sbBandEf->setRange(-500.0, 500.0);
    sbBandEf->setDecimals(4);
    QDoubleSpinBox *sbEMin = new QDoubleSpinBox(tabBands);
    sbEMin->setRange(-100.0, 100.0);
    sbEMin->setDecimals(2);
    sbEMin->setValue(-6.0);
    QDoubleSpinBox *sbEMax = new QDoubleSpinBox(tabBands);
    sbEMax->setRange(-100.0, 100.0);
    sbEMax->setDecimals(2);
    sbEMax->setValue(6.0);

    bandForm->addRow(QObject::tr("Wavevector Distance Column (k):"), cbKDist);
    bandForm->addRow(QObject::tr("Band Eigenvalue Columns:"), lwBands);
    bandForm->addRow(QObject::tr("High-Symmetry k-Tick Positions:"), leKPos);
    bandForm->addRow(QObject::tr("High-Symmetry k-Tick Labels:"), leKLbl);
    bandForm->addRow(QObject::tr("Fermi Energy E_F (eV):"), sbBandEf);
    bandForm->addRow(QObject::tr("Energy Window Min (E - E_F, eV):"), sbEMin);
    bandForm->addRow(QObject::tr("Energy Window Max (E - E_F, eV):"), sbEMax);

    QList<Table *> tables = collectProjectTables(app);
    auto refreshTableList = [&](Table *prefer) {
        tables = collectProjectTables(app);
        cbTable->blockSignals(true);
        cbTable->clear();
        int sel = 0;
        for (int i = 0; i < tables.size(); ++i) {
            cbTable->addItem(tables[i]->name());
            if (tables[i] == prefer)
                sel = i;
        }
        if (!tables.isEmpty())
            cbTable->setCurrentIndex(sel);
        cbTable->blockSignals(false);
    };

    auto populateFromTable = [&](int tIdx) {
        cbEnergy->clear();
        lwDosUp->clear();
        lwDosDn->clear();
        cbKDist->clear();
        lwBands->clear();
        if (tIdx < 0 || tIdx >= tables.size())
            return;
        Table *t = tables[tIdx];
        double autoEf = 0.0;
        if (t->numCols() > 0 && extractAutoVaspFermiFromColumn(t->column(0), autoEf)) {
            sbDosEf->setValue(autoEf);
            sbBandEf->setValue(autoEf);
        }
        bool looksLikeBands = false;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!matSciColumnHasNumeric(col))
                continue;
            const QString cn = col->name();
            cbEnergy->addItem(cn);
            cbKDist->addItem(cn);
            if (c > 0) {
                const QString lo = cn.toLower();
                if (lo.startsWith(QLatin1String("band_")))
                    looksLikeBands = true;
                QListWidgetItem *itBand = new QListWidgetItem(cn, lwBands);
                itBand->setSelected(true);

                if (lo.endsWith(QLatin1String("_down")) || lo.endsWith(QLatin1String("_dw"))
                    || lo.endsWith(QLatin1String("_dn"))) {
                    QListWidgetItem *itDn = new QListWidgetItem(cn, lwDosDn);
                    itDn->setSelected(true);
                } else {
                    QListWidgetItem *itUp = new QListWidgetItem(cn, lwDosUp);
                    itUp->setSelected(true);
                    lwDosDn->addItem(cn);
                }
            }
        }
        if (looksLikeBands)
            tabs->setCurrentIndex(1);
    };

    refreshTableList(findActiveOrFirstTable(app));
    populateFromTable(cbTable->currentIndex());
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     populateFromTable);

    QObject::connect(btnLoadFile, &QPushButton::clicked, &dlg, [&]() {
        const QString path = QFileDialog::getOpenFileName(
                &dlg, QObject::tr("Load VASP DOSCAR / EIGENVAL File..."), QString(),
                QStringLiteral("VASP & ASCII Files (DOSCAR* EIGENVAL* *.dat *.txt *.csv *);;All Files (*)"));
        if (path.isEmpty())
            return;
        double ef = 0.0;
        if (Table *loaded = importVaspFileAsProjectTable(app, path, &ef)) {
            refreshTableList(loaded);
            populateFromTable(cbTable->currentIndex());
            if (ef != 0.0) {
                sbDosEf->setValue(ef);
                sbBandEf->setValue(ef);
            }
        }
    });

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    root->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const int tIdx = cbTable->currentIndex();
    if (tIdx < 0 || tIdx >= tables.size())
        return;
    Table *srcTbl = tables[tIdx];

    VaspPlotResult vr;
    if (tabs->currentIndex() == 0) {
        QStringList upSel, dnSel;
        for (QListWidgetItem *it : lwDosUp->selectedItems())
            upSel << it->text();
        for (QListWidgetItem *it : lwDosDn->selectedItems())
            dnSel << it->text();
        vr = plotVaspDos(app, srcTbl, cbEnergy->currentText(), upSel, dnSel,
                         sbDosEf->value(), chkVertDos->isChecked());
    } else {
        QStringList bandSel;
        for (QListWidgetItem *it : lwBands->selectedItems())
            bandSel << it->text();
        QVector<double> kPos;
        for (const QString &tok : leKPos->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            bool ok = false;
            const double v = tok.trimmed().toDouble(&ok);
            if (ok)
                kPos.append(v);
        }
        QStringList kLbls;
        for (const QString &tok : leKLbl->text().split(QLatin1Char(','), Qt::SkipEmptyParts))
            kLbls << tok.trimmed();
        vr = plotVaspBands(app, srcTbl, cbKDist->currentText(), bandSel, kPos, kLbls,
                           sbBandEf->value(), sbEMin->value(), sbEMax->value());
    }
    if (!vr.valid && !ApplicationWindow::isNonInteractiveContext(app)) {
        QMessageBox::warning(
                app, QObject::tr("VASP Electronic Structure Plotter (DOS & Band Structure)"),
                vr.error);
    }
}

