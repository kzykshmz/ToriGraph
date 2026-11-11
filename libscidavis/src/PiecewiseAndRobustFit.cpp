/***************************************************************************
    File                 : PiecewiseAndRobustFit.cpp
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Description          : Piecewise Breakpoint Fit, Robust/Quantile Regression,
                           Dataset Comparison F-Test, and Multiple Linear Regression
 ***************************************************************************/

#include "PiecewiseAndRobustFit.h"

#include "ApplicationWindow.h"
#include "FunctionCurve.h"
#include "Graph.h"
#include "Legend.h"
#include "MultiLayer.h"
#include "MyWidget.h"
#include "NonLinearFit.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "Table.h"
#include "core/column/Column.h"

#include <qwt_plot_curve.h>
#include <qwt_symbol.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <gsl/gsl_cdf.h>
#include <gsl/gsl_fit.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_multimin.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace {

class SilentNonLinearFit : public NonLinearFit
{
public:
    SilentNonLinearFit(ApplicationWindow *app, Graph *g) : NonLinearFit(app, g)
    {
        d_silent = true;
        d_suppress_curve_output = true;
        d_tolerance = 1e-10;
        d_max_iterations = 500;
    }
};

static QwtPlotCurve *resolveCurveOnGraph(Graph *g, const QString &curveTitle, QString *outName = nullptr)
{
    if (!g)
        return nullptr;

    QwtPlotCurve *chosen = nullptr;
    if (!curveTitle.trimmed().isEmpty()) {
        chosen = g->curve(curveTitle);
        if (!chosen)
            chosen = g->curve(curveTitle.trimmed());
    }

    if (!chosen) {
        const QStringList analysable = g->analysableCurvesList();
        for (const QString &cn : analysable) {
            QwtPlotCurve *c = g->curve(cn);
            if (c && c->isVisible() && c->dataSize() > 0) {
                chosen = c;
                break;
            }
        }
    }

    if (!chosen) {
        for (int i = 0; i < g->curves(); ++i) {
            const int ctype = g->curveType(i);
            if (ctype == Graph::ErrorBars || ctype == Graph::Pie || ctype == Graph::Box)
                continue;
            QwtPlotCurve *c = g->curve(i);
            if (c && c->isVisible() && c->dataSize() > 0) {
                chosen = c;
                break;
            }
        }
    }

    if (chosen && outName)
        *outName = chosen->title().text();
    return chosen;
}

static bool extractUnmaskedSortedXY(QwtPlotCurve *c, std::vector<double> &xv, std::vector<double> &yv)
{
    xv.clear();
    yv.clear();
    if (!c || c->dataSize() <= 0)
        return false;

    const DataCurve *dc = dynamic_cast<const DataCurve *>(c);
    const int nTotal = c->dataSize();
    std::vector<std::pair<double, double>> pts;
    pts.reserve(size_t(nTotal));

    for (int i = 0; i < nTotal; ++i) {
        if (dc && dc->isPointMasked(i))
            continue;
        const double px = c->x(i);
        const double py = c->y(i);
        if (std::isfinite(px) && std::isfinite(py))
            pts.emplace_back(px, py);
    }
    if (pts.empty())
        return false;

    std::stable_sort(pts.begin(), pts.end(),
                     [](const auto &a, const auto &b) { return a.first < b.first; });

    xv.resize(pts.size());
    yv.resize(pts.size());
    for (size_t i = 0; i < pts.size(); ++i) {
        xv[i] = pts[i].first;
        yv[i] = pts[i].second;
    }
    return true;
}

static double computeVectorMedian(std::vector<double> vals)
{
    if (vals.empty())
        return 0.0;
    const size_t n = vals.size();
    const size_t mid = n / 2;
    std::nth_element(vals.begin(), vals.begin() + std::ptrdiff_t(mid), vals.end());
    const double hi = vals[mid];
    if (n % 2 == 1)
        return hi;
    const double lo = *std::max_element(vals.begin(), vals.begin() + std::ptrdiff_t(mid));
    return 0.5 * (lo + hi);
}

static double computeVectorQuantile(std::vector<double> vals, double tau)
{
    if (vals.empty())
        return 0.0;
    std::sort(vals.begin(), vals.end());
    if (vals.size() == 1)
        return vals[0];
    const double t = std::clamp(tau, 0.0, 1.0) * double(vals.size() - 1);
    const size_t idx = std::min(vals.size() - 1, size_t(std::floor(t)));
    const size_t idxNext = std::min(vals.size() - 1, idx + 1);
    const double frac = t - double(idx);
    return (1.0 - frac) * vals[idx] + frac * vals[idxNext];
}

static double computeMADScale(const std::vector<double> &r)
{
    if (r.empty())
        return 0.0;
    const double med = computeVectorMedian(r);
    std::vector<double> absDev(r.size());
    for (size_t i = 0; i < r.size(); ++i)
        absDev[i] = std::fabs(r[i] - med);
    return computeVectorMedian(std::move(absDev)) / 0.6745;
}

static void insertArialSummaryBox(Graph *g, const QString &text)
{
    if (!g || text.isEmpty())
        return;
    Legend *mrk = g->newLegend(text);
    if (mrk) {
        mrk->setFont(QFont("Arial", 9));
        if (g->hasLegend()) {
            Legend *leg = g->legend();
            if (leg && leg != mrk) {
                const QPoint p = leg->rect().bottomLeft();
                mrk->setOrigin(QPoint(p.x(), p.y() + 18));
            }
        }
    }
}

static QwtPlotCurve *addFittedLineToGraph(ApplicationWindow *app, Graph *g, const QString &baseName,
                                          const std::vector<double> &gx,
                                          const std::vector<double> &gy, const QColor &penColor,
                                          const QString &formulaDesc = QString())
{
    if (!app || !g || gx.size() < 2 || gy.size() != gx.size())
        return nullptr;

    const QString title = g->generateFunctionName(baseName);
    FunctionCurve *fc = new FunctionCurve(app, FunctionCurve::Normal, title);
    fc->setPen(QPen(penColor, 2));
    fc->setData(gx.data(), gy.data(), static_cast<int>(gx.size()));
    fc->setRange(gx.front(), gx.back());
    if (!formulaDesc.isEmpty())
        fc->setFormula(formulaDesc);
    g->insertPlotItem(fc, Graph::Line);
    g->addFitCurve(fc);
    g->replot();
    return fc;
}

// Normalized Hinge OLS Evaluator on u_i in [0, 1]:
// y(u) = alpha0 + gamma1 * u + sum_{j=1}^{K} dGamma_j * max(0, u - tau_j)
struct HingeContext
{
    std::vector<double> u;
    std::vector<double> y;
    int numKnots = 1;
    double minGap = 1e-4;

    bool solveAtKnots(const std::vector<double> &tau, std::vector<double> &coeffsOut,
                      double &rssOut) const
    {
        const size_t n = u.size();
        const size_t K = tau.size();
        const size_t p = K + 2;
        if (K == 0 || n < p + 1)
            return false;

        // Enforce strictly ordered interior knots with at least 2 points in outer segments
        const double loBound = std::max(1e-6, u[1] + 1e-9);
        const double hiBound = std::min(1.0 - 1e-6, u[n - 2] - 1e-9);
        if (!(hiBound > loBound))
            return false;

        for (size_t j = 0; j < K; ++j) {
            if (!std::isfinite(tau[j]) || tau[j] <= loBound || tau[j] >= hiBound)
                return false;
            if (j > 0 && (tau[j] - tau[j - 1]) < minGap)
                return false;
        }

        // Verify each segment contains at least one interior/support observation
        for (size_t seg = 0; seg <= K; ++seg) {
            const double segLo = (seg == 0) ? -1.0 : tau[seg - 1];
            const double segHi = (seg == K) ? 2.0 : tau[seg];
            int cnt = 0;
            for (size_t i = 0; i < n; ++i) {
                if (u[i] >= segLo - 1e-12 && u[i] <= segHi + 1e-12)
                    ++cnt;
            }
            const int req = (seg == 0 || seg == K) ? 2 : 2;
            if (cnt < req)
                return false;
        }

        gsl_matrix *X = gsl_matrix_alloc(n, p);
        gsl_vector *Y = gsl_vector_alloc(n);
        gsl_vector *C = gsl_vector_alloc(p);
        gsl_matrix *cov = gsl_matrix_alloc(p, p);
        gsl_multifit_linear_workspace *ws = gsl_multifit_linear_alloc(n, p);

        for (size_t i = 0; i < n; ++i) {
            gsl_matrix_set(X, i, 0, 1.0);
            gsl_matrix_set(X, i, 1, u[i]);
            for (size_t j = 0; j < K; ++j)
                gsl_matrix_set(X, i, 2 + j, std::max(0.0, u[i] - tau[j]));
            gsl_vector_set(Y, i, y[i]);
        }

        double chisq = 0.0;
        const int st = gsl_multifit_linear(X, Y, C, cov, &chisq, ws);
        coeffsOut.resize(p, 0.0);
        for (size_t j = 0; j < p; ++j)
            coeffsOut[j] = gsl_vector_get(C, j);

        // Compute exact RSS directly for numerical fidelity
        double rss = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double yHat = coeffsOut[0] + coeffsOut[1] * u[i];
            for (size_t j = 0; j < K; ++j) {
                if (u[i] > tau[j])
                    yHat += coeffsOut[2 + j] * (u[i] - tau[j]);
            }
            const double d = y[i] - yHat;
            rss += d * d;
        }
        rssOut = rss;

        gsl_multifit_linear_free(ws);
        gsl_matrix_free(cov);
        gsl_vector_free(C);
        gsl_vector_free(Y);
        gsl_matrix_free(X);
        return (st == GSL_SUCCESS) && std::isfinite(rssOut);
    }

    double evalObjective(const std::vector<double> &tau) const
    {
        std::vector<double> c;
        double rss = 1e100;
        if (!solveAtKnots(tau, c, rss))
            return 1e100;
        return rss;
    }

    static double gslSimplexFn(const gsl_vector *v, void *params)
    {
        const auto *ctx = static_cast<const HingeContext *>(params);
        std::vector<double> tau(size_t(ctx->numKnots));
        for (int j = 0; j < ctx->numKnots; ++j)
            tau[size_t(j)] = gsl_vector_get(v, size_t(j));
        return ctx->evalObjective(tau);
    }
};

static double goldenSectionMinimize1D(const HingeContext &ctx, double a, double b, double &bestXOut)
{
    const double invPhi = 0.6180339887498948482;     // (sqrt(5) - 1) / 2
    const double invPhi2 = 0.3819660112501051518;    // (3 - sqrt(5)) / 2
    double lo = a;
    double hi = b;
    if (hi <= lo) {
        bestXOut = 0.5 * (lo + hi);
        return ctx.evalObjective({ bestXOut });
    }

    double x1 = lo + invPhi2 * (hi - lo);
    double x2 = lo + invPhi * (hi - lo);
    double f1 = ctx.evalObjective({ x1 });
    double f2 = ctx.evalObjective({ x2 });

    for (int iter = 0; iter < 68; ++iter) {
        if (std::fabs(hi - lo) <= 1e-13)
            break;
        if (f1 <= f2) {
            hi = x2;
            x2 = x1;
            f2 = f1;
            x1 = lo + invPhi2 * (hi - lo);
            f1 = ctx.evalObjective({ x1 });
        } else {
            lo = x1;
            x1 = x2;
            f1 = f2;
            x2 = lo + invPhi * (hi - lo);
            f2 = ctx.evalObjective({ x2 });
        }
    }

    if (f1 <= f2) {
        bestXOut = x1;
        return f1;
    }
    bestXOut = x2;
    return f2;
}

static int resolveTableColumnIndex(Table *t, const QString &colSpec)
{
    if (!t || colSpec.trimmed().isEmpty())
        return -1;

    const QString spec = colSpec.trimmed();
    int idx = t->colIndex(spec);
    if (idx >= 0 && idx < t->numCols())
        return idx;

    for (int c = 0; c < t->numCols(); ++c) {
        if (t->colLabel(c) == spec || t->colName(c) == spec)
            return c;
    }
    for (int c = 0; c < t->numCols(); ++c) {
        if (t->colLabel(c).compare(spec, Qt::CaseInsensitive) == 0
            || t->colName(c).compare(spec, Qt::CaseInsensitive) == 0)
            return c;
    }
    return -1;
}

static bool configureModelOnFitter(SilentNonLinearFit &nlf, const QString &rawModelName)
{
    QString mName = rawModelName.trimmed();
    if (mName.isEmpty())
        mName = QStringLiteral("ExpDec1");

    if (nlf.setBuiltInModel(mName))
        return true;

    if (mName.compare(QStringLiteral("ExpDecay1"), Qt::CaseInsensitive) == 0
        || mName.compare(QStringLiteral("ExpDecay"), Qt::CaseInsensitive) == 0) {
        return nlf.setBuiltInModel(QStringLiteral("ExpDec1"));
    }
    if (mName.compare(QStringLiteral("ExpDecay2"), Qt::CaseInsensitive) == 0)
        return nlf.setBuiltInModel(QStringLiteral("ExpDec2"));
    if (mName.compare(QStringLiteral("ExpDecay3"), Qt::CaseInsensitive) == 0)
        return nlf.setBuiltInModel(QStringLiteral("ExpDec3"));
    if (mName.compare(QStringLiteral("Sigmoidal"), Qt::CaseInsensitive) == 0)
        return nlf.setBuiltInModel(QStringLiteral("Boltzmann"));
    if (mName.compare(QStringLiteral("Linear"), Qt::CaseInsensitive) == 0
        || mName.compare(QStringLiteral("Line"), Qt::CaseInsensitive) == 0) {
        nlf.setParametersList(QStringList() << QStringLiteral("B") << QStringLiteral("A"));
        nlf.setFormula(QStringLiteral("A*x+B"));
        return true;
    }
    if (mName.compare(QStringLiteral("Quadratic"), Qt::CaseInsensitive) == 0
        || mName.compare(QStringLiteral("Poly2"), Qt::CaseInsensitive) == 0) {
        nlf.setParametersList(QStringList() << QStringLiteral("a0") << QStringLiteral("a1")
                                            << QStringLiteral("a2"));
        nlf.setFormula(QStringLiteral("a0+a1*x+a2*x^2"));
        return true;
    }
    return false;
}

} // anonymous namespace

// ============================================================================
// 1. Piecewise Linear Breakpoint Regression
// ============================================================================

PiecewiseAndRobustFit::PiecewiseResult
PiecewiseAndRobustFit::fitPiecewiseLinear(ApplicationWindow *app, Graph *g,
                                          const QString &curveTitle, int numSegments,
                                          bool annotateGraph)
{
    PiecewiseResult res;
    const int S = std::clamp(numSegments, 2, 3);
    res.segments = S;
    if (!app || !g)
        return res;

    QString resolvedName;
    QwtPlotCurve *curve = resolveCurveOnGraph(g, curveTitle, &resolvedName);
    std::vector<double> xv, yv;
    if (!extractUnmaskedSortedXY(curve, xv, yv))
        return res;

    const size_t n = xv.size();
    if (n < size_t(2 * S))
        return res;

    const double xMin = xv.front();
    const double xMax = xv.back();
    const double xSpan = xMax - xMin;
    if (!(xSpan > 1e-15))
        return res;

    HingeContext ctx;
    ctx.numKnots = S - 1;
    ctx.u.resize(n);
    ctx.y = yv;
    for (size_t i = 0; i < n; ++i)
        ctx.u[i] = (xv[i] - xMin) / xSpan;

    // Distinct sorted normalized abscissas for exact sub-interval search
    std::vector<double> uDistinct;
    uDistinct.reserve(n);
    for (double ui : ctx.u) {
        if (uDistinct.empty() || (ui - uDistinct.back()) > 1e-12)
            uDistinct.push_back(ui);
    }
    if (uDistinct.size() < size_t(2 * S))
        return res;

    const double tauLo = std::max(1e-5, ctx.u[1] + 1e-7);
    const double tauHi = std::min(1.0 - 1e-5, ctx.u[n - 2] - 1e-7);
    if (!(tauHi > tauLo))
        return res;

    ctx.minGap = std::max(1e-4, 0.5 / double(uDistinct.size()));

    std::vector<double> bestTau(size_t(S - 1), 0.5);
    double bestRss = 1e100;

    if (S == 2) {
        // Dense grid + per-data-interval candidates + golden-section refinement
        struct IntervalCand {
            double lo;
            double hi;
            double midRss;
        };
        std::vector<IntervalCand> intervals;
        for (size_t i = 1; i + 2 < uDistinct.size(); ++i) {
            const double ilo = std::max(tauLo, uDistinct[i]);
            const double ihi = std::min(tauHi, uDistinct[i + 1]);
            if (ihi > ilo + 1e-12) {
                const double mid = 0.5 * (ilo + ihi);
                const double mrss = ctx.evalObjective({ mid });
                intervals.push_back({ ilo, ihi, mrss });
                if (mrss < bestRss) {
                    bestRss = mrss;
                    bestTau[0] = mid;
                }
            }
        }

        const int nGrid = 240;
        for (int gIdx = 0; gIdx <= nGrid; ++gIdx) {
            const double cand = tauLo + (tauHi - tauLo) * double(gIdx) / double(nGrid);
            const double rVal = ctx.evalObjective({ cand });
            if (rVal < bestRss) {
                bestRss = rVal;
                bestTau[0] = cand;
            }
        }

        // Refine top 12 data sub-intervals with golden-section search (exact global optimum!)
        std::sort(intervals.begin(), intervals.end(),
                  [](const IntervalCand &a, const IntervalCand &b) { return a.midRss < b.midRss; });
        const size_t nRefine = std::min<size_t>(intervals.size(), 12);
        for (size_t k = 0; k < nRefine; ++k) {
            double xOpt = 0.5 * (intervals[k].lo + intervals[k].hi);
            const double fOpt = goldenSectionMinimize1D(ctx, intervals[k].lo, intervals[k].hi, xOpt);
            if (fOpt < bestRss) {
                bestRss = fOpt;
                bestTau[0] = xOpt;
            }
        }

        // Also refine wider bracket around best grid candidate
        const double wideHalf = 0.04 * (tauHi - tauLo);
        double xWideOpt = bestTau[0];
        const double fWideOpt = goldenSectionMinimize1D(
                ctx, std::max(tauLo, bestTau[0] - wideHalf),
                std::min(tauHi, bestTau[0] + wideHalf), xWideOpt);
        if (fWideOpt < bestRss) {
            bestRss = fWideOpt;
            bestTau[0] = xWideOpt;
        }
    } else {
        // S == 3 (2 breakpoints): 2D triangular grid search + multi-start Nelder-Mead simplex
        struct PairSeed {
            double t1;
            double t2;
            double rss;
        };
        std::vector<PairSeed> seeds;
        const int nGrid2D = 48;
        seeds.reserve(size_t(nGrid2D * nGrid2D / 2));
        for (int i = 0; i < nGrid2D; ++i) {
            const double t1 = tauLo + (tauHi - tauLo) * double(i) / double(nGrid2D - 1);
            for (int j = i + 1; j < nGrid2D; ++j) {
                const double t2 = tauLo + (tauHi - tauLo) * double(j) / double(nGrid2D - 1);
                if (t2 - t1 < ctx.minGap)
                    continue;
                const double rVal = ctx.evalObjective({ t1, t2 });
                if (rVal < 1e90)
                    seeds.push_back({ t1, t2, rVal });
            }
        }

        std::sort(seeds.begin(), seeds.end(),
                  [](const PairSeed &a, const PairSeed &b) { return a.rss < b.rss; });

        if (!seeds.empty()) {
            bestTau = { seeds[0].t1, seeds[0].t2 };
            bestRss = seeds[0].rss;
        }

        const size_t nStarts = std::min<size_t>(seeds.size(), 6);
        for (size_t sIdx = 0; sIdx < nStarts; ++sIdx) {
            gsl_multimin_function fn;
            fn.n = 2;
            fn.f = &HingeContext::gslSimplexFn;
            fn.params = &ctx;

            gsl_vector *x0 = gsl_vector_alloc(2);
            gsl_vector *ss = gsl_vector_alloc(2);
            gsl_vector_set(x0, 0, seeds[sIdx].t1);
            gsl_vector_set(x0, 1, seeds[sIdx].t2);
            gsl_vector_set_all(ss, 0.03 * (tauHi - tauLo));

            gsl_multimin_fminimizer *sim =
                    gsl_multimin_fminimizer_alloc(gsl_multimin_fminimizer_nmsimplex2, 2);
            if (sim) {
                gsl_multimin_fminimizer_set(sim, &fn, x0, ss);
                for (int iter = 0; iter < 400; ++iter) {
                    const int st = gsl_multimin_fminimizer_iterate(sim);
                    if (st)
                        break;
                    const double sz = gsl_multimin_fminimizer_size(sim);
                    if (gsl_multimin_test_size(sz, 1e-11) == GSL_SUCCESS)
                        break;
                }
                if (sim->fval < bestRss) {
                    bestRss = sim->fval;
                    bestTau[0] = gsl_vector_get(sim->x, 0);
                    bestTau[1] = gsl_vector_get(sim->x, 1);
                }
                gsl_multimin_fminimizer_free(sim);
            }
            gsl_vector_free(ss);
            gsl_vector_free(x0);
        }
    }

    std::vector<double> coeffs;
    if (!ctx.solveAtKnots(bestTau, coeffs, bestRss))
        return res;

    // Convert normalized u-space parameters back to physical x-space
    const double alpha0 = coeffs[0];
    const double gamma1 = coeffs[1];
    res.slopes.resize(S);
    double cumGamma = gamma1;
    for (int m = 0; m < S; ++m) {
        if (m > 0)
            cumGamma += coeffs[size_t(1 + m)];
        res.slopes[m] = cumGamma / xSpan;
    }
    res.intercept0 = alpha0 - res.slopes[0] * xMin;

    auto evalModelY = [&](double xVal) -> double {
        double yVal = res.intercept0 + res.slopes[0] * xVal;
        for (int j = 0; j < S - 1; ++j) {
            const double bpX = xMin + bestTau[size_t(j)] * xSpan;
            if (xVal > bpX)
                yVal += (res.slopes[j + 1] - res.slopes[j]) * (xVal - bpX);
        }
        return yVal;
    };

    res.breakpointsX.resize(S - 1);
    res.breakpointsY.resize(S - 1);
    for (int j = 0; j < S - 1; ++j) {
        const double bpX = xMin + bestTau[size_t(j)] * xSpan;
        res.breakpointsX[j] = bpX;
        res.breakpointsY[j] = evalModelY(bpX);
    }

    const double meanY = std::accumulate(yv.begin(), yv.end(), 0.0) / double(n);
    double tss = 0.0;
    for (double yi : yv) {
        const double dy = yi - meanY;
        tss += dy * dy;
    }
    res.rSquare = (tss > 1e-30) ? std::clamp(1.0 - bestRss / tss, 0.0, 1.0)
                                : ((bestRss <= 1e-24) ? 1.0 : 0.0);
    const int kFree = 2 * S; // a0 + S slopes + (S-1) breakpoints = 2*S
    const double effRss = std::max(bestRss, 1e-30 * std::max(1.0, tss));
    res.aic = double(n) * std::log(effRss / double(n)) + 2.0 * double(kFree + 1);
    res.converged = true;

    // Plot piecewise linear fitted curve with exact hinge breakpoints included
    std::vector<double> gx;
    gx.reserve(210);
    for (int k = 0; k <= 200; ++k)
        gx.push_back(xMin + xSpan * double(k) / 200.0);
    for (double bpX : res.breakpointsX)
        gx.push_back(bpX);
    std::sort(gx.begin(), gx.end());
    gx.erase(std::unique(gx.begin(), gx.end(),
                         [xSpan](double a, double b) { return std::fabs(a - b) <= 1e-13 * xSpan; }),
             gx.end());

    std::vector<double> gy(gx.size());
    for (size_t i = 0; i < gx.size(); ++i)
        gy[i] = evalModelY(gx[i]);

    res.fittedCurve = addFittedLineToGraph(app, g, QStringLiteral("PiecewiseFit"), gx, gy,
                                           QColor(215, 25, 28));

    if (annotateGraph) {
        QStringList lines;
        lines << QObject::tr("Piecewise Linear Fit (%1 segments)").arg(S);
        lines << QObject::tr("Dataset: %1").arg(resolvedName);
        for (int j = 0; j < S - 1; ++j) {
            lines << QObject::tr("Breakpoint %1: X = %2, Y = %3")
                             .arg(j + 1)
                             .arg(res.breakpointsX[j], 0, 'g', 6)
                             .arg(res.breakpointsY[j], 0, 'g', 6);
        }
        for (int m = 0; m < S; ++m) {
            lines << QObject::tr("Slope k%1 = %2").arg(m + 1).arg(res.slopes[m], 0, 'g', 6);
        }
        lines << QObject::tr("Intercept a0 = %1").arg(res.intercept0, 0, 'g', 6);
        lines << QObject::tr("R² = %1, AIC = %2")
                         .arg(res.rSquare, 0, 'f', 5)
                         .arg(res.aic, 0, 'g', 6);
        insertArialSummaryBox(g, lines.join('\n'));
        g->replot();
    }

    // Create Report Worksheet
    QVector<QPair<QString, double>> reportRows;
    reportRows << qMakePair(QStringLiteral("Intercept_a0"), res.intercept0);
    for (int m = 0; m < S; ++m)
        reportRows << qMakePair(QStringLiteral("Slope_%1").arg(m + 1), res.slopes[m]);
    for (int j = 0; j < S - 1; ++j) {
        reportRows << qMakePair(QStringLiteral("Breakpoint_X_%1").arg(j + 1), res.breakpointsX[j]);
        reportRows << qMakePair(QStringLiteral("Breakpoint_Y_%1").arg(j + 1), res.breakpointsY[j]);
    }
    reportRows << qMakePair(QStringLiteral("R_Square"), res.rSquare)
               << qMakePair(QStringLiteral("AIC"), res.aic)
               << qMakePair(QStringLiteral("RSS"), bestRss)
               << qMakePair(QStringLiteral("Segments"), double(S));

    const QString tblName = app->generateUniqueName(QStringLiteral("PiecewiseFit"), false);
    Table *tbl = app->newTable(reportRows.size(), 2, tblName,
                               QObject::tr("Piecewise Linear Fit Report"));
    if (tbl) {
        tbl->setHeader(QStringList() << QObject::tr("Parameter") << QObject::tr("Value"));
        tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        tbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        for (int r = 0; r < reportRows.size(); ++r) {
            tbl->column(0)->setTextAt(r, reportRows[r].first);
            tbl->column(1)->setValueAt(r, reportRows[r].second);
        }
        res.reportTable = tbl;
    }

    return res;
}

// ============================================================================
// 2. Robust M-Estimation (Huber, Tukey Bisquare) & Quantile Linear Regression
// ============================================================================

PiecewiseAndRobustFit::RobustFitResult
PiecewiseAndRobustFit::fitRobustOrQuantileLinear(ApplicationWindow *app, Graph *g,
                                                 const QString &curveTitle, RobustMethod method,
                                                 double quantileTau, bool annotateGraph)
{
    RobustFitResult res;
    if (!app || !g)
        return res;

    QString resolvedName;
    QwtPlotCurve *curve = resolveCurveOnGraph(g, curveTitle, &resolvedName);
    std::vector<double> xv, yv;
    if (!extractUnmaskedSortedXY(curve, xv, yv))
        return res;

    const size_t n = xv.size();
    if (n < 2)
        return res;

    const double xMin = xv.front();
    const double xMax = xv.back();
    const double xSpan = xMax - xMin;
    if (!(xSpan > 1e-15))
        return res;

    if (method == OrdinaryLS) {
        double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0, sumsq = 0.0;
        if (gsl_fit_linear(xv.data(), 1, yv.data(), 1, n, &c0, &c1, &cov00, &cov01, &cov11, &sumsq)
            != GSL_SUCCESS)
            return res;

        std::vector<double> resid(n);
        for (size_t i = 0; i < n; ++i)
            resid[i] = yv[i] - (c0 + c1 * xv[i]);

        res.converged = true;
        res.intercept = c0;
        res.slope = c1;
        res.interceptSE = (n > 2 && cov00 > 0.0) ? std::sqrt(cov00) : 0.0;
        res.slopeSE = (n > 2 && cov11 > 0.0) ? std::sqrt(cov11) : 0.0;
        res.scaleMAD = computeMADScale(resid);
    } else {
        // Outlier-immune Theil-Sen median slope initialization
        std::vector<size_t> subIdx;
        const size_t maxPairsPts = 500;
        if (n <= maxPairsPts) {
            subIdx.resize(n);
            std::iota(subIdx.begin(), subIdx.end(), 0);
        } else {
            subIdx.reserve(maxPairsPts);
            for (size_t k = 0; k < maxPairsPts; ++k)
                subIdx.push_back((k * (n - 1)) / (maxPairsPts - 1));
        }

        std::vector<double> pairSlopes;
        pairSlopes.reserve((subIdx.size() * (subIdx.size() - 1)) / 2);
        for (size_t a = 0; a < subIdx.size(); ++a) {
            const size_t ia = subIdx[a];
            for (size_t b = a + 1; b < subIdx.size(); ++b) {
                const size_t ib = subIdx[b];
                const double dx = xv[ib] - xv[ia];
                if (std::fabs(dx) > 1e-12 * xSpan)
                    pairSlopes.push_back((yv[ib] - yv[ia]) / dx);
            }
        }
        if (pairSlopes.empty())
            return res;

        double curSlope = computeVectorMedian(std::move(pairSlopes));
        std::vector<double> candIntercepts(n);
        for (size_t i = 0; i < n; ++i)
            candIntercepts[i] = yv[i] - curSlope * xv[i];

        const double tau = std::clamp(quantileTau, 0.01, 0.99);
        double curIntercept = (method == QuantileReg)
                ? computeVectorQuantile(std::move(candIntercepts), tau)
                : computeVectorMedian(std::move(candIntercepts));

        const double madY = std::max(1.0, computeMADScale(yv));
        const double minScaleFloor = 1e-7 * madY;

        std::vector<double> resid(n, 0.0);
        std::vector<double> weights(n, 1.0);
        double curScaleMAD = 0.0;
        double lastCov00 = 0.0, lastCov11 = 0.0, lastChisq = 0.0;
        bool converged = false;

        const int maxIter = (method == QuantileReg) ? 160 : 100;
        for (int iter = 0; iter < maxIter; ++iter) {
            for (size_t i = 0; i < n; ++i)
                resid[i] = yv[i] - (curIntercept + curSlope * xv[i]);

            curScaleMAD = computeMADScale(resid);
            const double s = std::max(curScaleMAD, minScaleFloor);

            int posWeightCount = 0;
            double minWeightedX = std::numeric_limits<double>::infinity();
            double maxWeightedX = -std::numeric_limits<double>::infinity();

            if (method == HuberM) {
                const double cHuber = 1.345;
                for (size_t i = 0; i < n; ++i) {
                    const double u = std::fabs(resid[i]) / s;
                    weights[i] = (u <= cHuber) ? 1.0 : (cHuber / u);
                    if (weights[i] > 1e-15) {
                        ++posWeightCount;
                        minWeightedX = std::min(minWeightedX, xv[i]);
                        maxWeightedX = std::max(maxWeightedX, xv[i]);
                    }
                }
            } else if (method == TukeyBisquare) {
                const double cTukey = 4.685;
                for (size_t i = 0; i < n; ++i) {
                    const double u = std::fabs(resid[i]) / s;
                    if (u < cTukey) {
                        const double t = u / cTukey;
                        const double oneMinusT2 = 1.0 - t * t;
                        weights[i] = oneMinusT2 * oneMinusT2;
                    } else {
                        weights[i] = 0.0;
                    }
                    if (weights[i] > 1e-12) {
                        ++posWeightCount;
                        minWeightedX = std::min(minWeightedX, xv[i]);
                        maxWeightedX = std::max(maxWeightedX, xv[i]);
                    }
                }
            } else {
                // Quantile Regression pinball IRLS
                const double epsPinball = std::max(1e-6, 1e-6 * madY);
                for (size_t i = 0; i < n; ++i) {
                    const double asym = (resid[i] >= 0.0) ? tau : (1.0 - tau);
                    weights[i] = asym / std::max(std::fabs(resid[i]), epsPinball);
                    ++posWeightCount;
                    minWeightedX = std::min(minWeightedX, xv[i]);
                    maxWeightedX = std::max(maxWeightedX, xv[i]);
                }
            }

            if (posWeightCount < 2 || !(maxWeightedX > minWeightedX + 1e-13 * xSpan)) {
                converged = true;
                break;
            }

            double nextInt = curIntercept, nextSlope = curSlope;
            double cov00 = 0.0, cov01 = 0.0, cov11 = 0.0, chisq = 0.0;
            if (gsl_fit_wlinear(xv.data(), 1, weights.data(), 1, yv.data(), 1, n, &nextInt,
                                &nextSlope, &cov00, &cov01, &cov11, &chisq)
                != GSL_SUCCESS) {
                break;
            }
            if (!std::isfinite(nextInt) || !std::isfinite(nextSlope))
                break;

            lastCov00 = cov00;
            lastCov11 = cov11;
            lastChisq = chisq;

            const double delta = std::fabs(nextInt - curIntercept)
                    + std::fabs(nextSlope - curSlope) * xSpan;
            curIntercept = nextInt;
            curSlope = nextSlope;
            if (delta <= 1e-11 * std::max(1.0, madY)) {
                converged = true;
                break;
            }
            if (iter + 1 == maxIter)
                converged = true;
        }

        for (size_t i = 0; i < n; ++i)
            resid[i] = yv[i] - (curIntercept + curSlope * xv[i]);
        curScaleMAD = computeMADScale(resid);

        int nEff = 0;
        for (double wi : weights) {
            if (wi > 1e-10)
                ++nEff;
        }
        const double dof = double(std::max(1, nEff - 2));
        const double varScale = (nEff > 2 && lastChisq > 1e-28) ? (lastChisq / dof) : 0.0;

        res.converged = converged;
        res.intercept = curIntercept;
        res.slope = curSlope;
        res.interceptSE = (varScale > 0.0 && lastCov00 > 0.0) ? std::sqrt(varScale * lastCov00) : 0.0;
        res.slopeSE = (varScale > 0.0 && lastCov11 > 0.0) ? std::sqrt(varScale * lastCov11) : 0.0;
        res.scaleMAD = curScaleMAD;
    }

    // Plot fitted line
    const std::vector<double> gx = { xMin, xMax };
    const std::vector<double> gy = { res.intercept + res.slope * xMin,
                                     res.intercept + res.slope * xMax };

    QString methodTag;
    QString methodDisplay;
    switch (method) {
    case OrdinaryLS:
        methodTag = QStringLiteral("OLS");
        methodDisplay = QObject::tr("Ordinary Least Squares");
        break;
    case HuberM:
        methodTag = QStringLiteral("HuberM");
        methodDisplay = QObject::tr("Huber M-Estimator (c=1.345)");
        break;
    case TukeyBisquare:
        methodTag = QStringLiteral("TukeyBisquare");
        methodDisplay = QObject::tr("Tukey Bisquare M-Estimator (c=4.685)");
        break;
    case QuantileReg:
        methodTag = QStringLiteral("QuantileReg");
        methodDisplay = QObject::tr("Quantile Regression (tau=%1)")
                                .arg(std::clamp(quantileTau, 0.01, 0.99), 0, 'f', 2);
        break;
    }

    res.fittedCurve = addFittedLineToGraph(app, g, methodTag + QStringLiteral("_Fit"), gx, gy,
                                           QColor(0, 114, 178));

    if (annotateGraph) {
        QStringList lines;
        lines << methodDisplay;
        lines << QObject::tr("Dataset: %1").arg(resolvedName);
        lines << QObject::tr("Slope = %1 ± %2")
                         .arg(res.slope, 0, 'g', 6)
                         .arg(res.slopeSE, 0, 'g', 4);
        lines << QObject::tr("Intercept = %1 ± %2")
                         .arg(res.intercept, 0, 'g', 6)
                         .arg(res.interceptSE, 0, 'g', 4);
        lines << QObject::tr("MAD Scale = %1").arg(res.scaleMAD, 0, 'g', 5);
        insertArialSummaryBox(g, lines.join('\n'));
        g->replot();
    }

    // Create Report Worksheet
    const QString tblName = app->generateUniqueName(methodTag + QStringLiteral("_Report"), false);
    Table *tbl = app->newTable(5, 3, tblName, QObject::tr("Robust / Quantile Regression Report"));
    if (tbl) {
        tbl->setHeader(QStringList() << QObject::tr("Parameter") << QObject::tr("Estimate")
                                     << QObject::tr("Std_Error"));
        tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        tbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        tbl->column(2)->setColumnMode(SciDAVis::ColumnMode::Numeric);

        tbl->column(0)->setTextAt(0, QStringLiteral("Intercept"));
        tbl->column(1)->setValueAt(0, res.intercept);
        tbl->column(2)->setValueAt(0, res.interceptSE);

        tbl->column(0)->setTextAt(1, QStringLiteral("Slope"));
        tbl->column(1)->setValueAt(1, res.slope);
        tbl->column(2)->setValueAt(1, res.slopeSE);

        tbl->column(0)->setTextAt(2, QStringLiteral("MAD_Scale"));
        tbl->column(1)->setValueAt(2, res.scaleMAD);
        tbl->column(2)->setValueAt(2, 0.0);

        tbl->column(0)->setTextAt(3, QStringLiteral("N_Points"));
        tbl->column(1)->setValueAt(3, double(n));
        tbl->column(2)->setValueAt(3, 0.0);

        tbl->column(0)->setTextAt(4, QStringLiteral("Quantile_Tau"));
        tbl->column(1)->setValueAt(4, (method == QuantileReg) ? std::clamp(quantileTau, 0.01, 0.99)
                                                              : 0.5);
        tbl->column(2)->setValueAt(4, 0.0);

        res.reportTable = tbl;
    }

    return res;
}

// ============================================================================
// 3. Dataset Comparison Extra-Sum-of-Squares F-Test & AICc
// ============================================================================

PiecewiseAndRobustFit::DatasetCompareResult
PiecewiseAndRobustFit::compareDatasets(ApplicationWindow *app, Graph *g,
                                       const QStringList &curveTitles, const QString &modelName)
{
    DatasetCompareResult res;
    if (!app || !g || curveTitles.size() < 2)
        return res;

    QStringList validCurves;
    int nTotal = 0;
    double tssTotal = 0.0;
    std::vector<int> curveCounts;

    for (const QString &ct : curveTitles) {
        QwtPlotCurve *c = g->curve(ct);
        std::vector<double> xv, yv;
        if (!c || !extractUnmaskedSortedXY(c, xv, yv) || xv.empty())
            continue;
        validCurves << ct;
        const int nk = static_cast<int>(xv.size());
        curveCounts.push_back(nk);
        nTotal += nk;
        const double meanY = std::accumulate(yv.begin(), yv.end(), 0.0) / double(nk);
        for (double yi : yv) {
            const double dy = yi - meanY;
            tssTotal += dy * dy;
        }
    }

    const int K = validCurves.size();
    if (K < 2)
        return res;

    SilentNonLinearFit globalFitter(app, g);
    if (!configureModelOnFitter(globalFitter, modelName))
        return res;

    const int p = std::max(1, globalFitter.numParameters());
    for (int nk : curveCounts) {
        if (nk < p)
            return res;
    }
    if (nTotal <= K * p)
        return res;

    // 1. Fit Global (Shared parameters across all K curves)
    const QVector<bool> allShared(p, true);
    if (!globalFitter.fitGlobal(validCurves, allShared, nullptr))
        return res;
    double rssShared = std::max(0.0, globalFitter.chiSquare());

    // 2. Fit Separate parameters on each curve independently
    double rssSeparate = 0.0;
    for (int m = 0; m < K; ++m) {
        SilentNonLinearFit sepFitter(app, g);
        if (!configureModelOnFitter(sepFitter, modelName))
            return res;
        const QVector<bool> noneShared(p, false);
        if (!sepFitter.fitGlobal(QStringList() << validCurves[m], noneShared, nullptr))
            return res;
        rssSeparate += std::max(0.0, sepFitter.chiSquare());
    }

    // Also evaluate joint unshared global fit in case shared optimizer landed in a slightly worse
    // local basin on identical datasets; shared minimum is >= separate minimum mathematically
    if (rssShared < rssSeparate)
        rssShared = rssSeparate;

    const int dfShared = nTotal - p;
    const int dfSeparate = nTotal - K * p;
    const int dfDiff = dfShared - dfSeparate; // (K - 1) * p > 0
    if (dfSeparate <= 0 || dfDiff <= 0)
        return res;

    const double noiselessFloor = 1e-18 * std::max(1.0, tssTotal);
    double fStat = 0.0;
    double pVal = 1.0;

    if (rssShared <= noiselessFloor && rssSeparate <= noiselessFloor) {
        fStat = 0.0;
        pVal = 1.0;
    } else if (rssSeparate <= 1e-26 * std::max(1.0, rssShared)) {
        fStat = 1e12;
        pVal = 0.0;
    } else {
        const double msDiff = std::max(0.0, rssShared - rssSeparate) / double(dfDiff);
        const double msSep = rssSeparate / double(dfSeparate);
        fStat = (msSep > 1e-300) ? (msDiff / msSep) : ((msDiff > 0.0) ? 1e12 : 0.0);
        if (!std::isfinite(fStat) || fStat <= 0.0) {
            fStat = 0.0;
            pVal = 1.0;
        } else if (fStat >= 1e11) {
            pVal = 0.0;
        } else {
            pVal = std::clamp(gsl_cdf_fdist_Q(fStat, double(dfDiff), double(dfSeparate)), 0.0, 1.0);
        }
    }

    auto computeAICc = [&](double rssVal, int kParams) -> double {
        const double effRss = std::max(rssVal, 1e-24 * std::max(1.0, tssTotal));
        const double k = double(kParams + 1);
        const double aic = double(nTotal) * std::log(effRss / double(nTotal)) + 2.0 * k;
        if (nTotal > kParams + 2)
            return aic + (2.0 * k * (k + 1.0)) / double(nTotal - kParams - 2);
        return aic + 2.0 * k * (k + 1.0);
    };

    const double aiccShared = computeAICc(rssShared, p);
    const double aiccSeparate = computeAICc(rssSeparate, K * p);
    const double deltaAICc = aiccShared - aiccSeparate;

    res.valid = true;
    res.rssShared = rssShared;
    res.dfShared = dfShared;
    res.rssSeparate = rssSeparate;
    res.dfSeparate = dfSeparate;
    res.fStatistic = fStat;
    res.pValue = pVal;
    res.deltaAICc = deltaAICc;
    res.preferSeparate = (pVal < 0.05);

    const QString tblName = app->generateUniqueName(QStringLiteral("DatasetComparison"), false);
    Table *tbl = app->newTable(2, 7, tblName, QObject::tr("Dataset Comparison F-Test"));
    if (tbl) {
        tbl->setHeader(QStringList() << QObject::tr("Hypothesis_Model") << QObject::tr("RSS")
                                     << QObject::tr("DF") << QObject::tr("AICc")
                                     << QObject::tr("F_Statistic") << QObject::tr("p_Value")
                                     << QObject::tr("Conclusion"));
        tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 1; c <= 5; ++c)
            tbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        tbl->column(6)->setColumnMode(SciDAVis::ColumnMode::Text);

        const QString conclusion = res.preferSeparate
                ? QObject::tr("Reject Shared (Different parameters across datasets)")
                : QObject::tr("Do Not Reject Shared (Global shared model adequate)");

        tbl->column(0)->setTextAt(0, QStringLiteral("Global_Shared (%1)").arg(modelName));
        tbl->column(1)->setValueAt(0, rssShared);
        tbl->column(2)->setValueAt(0, double(dfShared));
        tbl->column(3)->setValueAt(0, aiccShared);
        tbl->column(4)->setValueAt(0, fStat);
        tbl->column(5)->setValueAt(0, pVal);
        tbl->column(6)->setTextAt(0, conclusion);

        tbl->column(0)->setTextAt(1, QStringLiteral("Separate_Per_Dataset (%1)").arg(modelName));
        tbl->column(1)->setValueAt(1, rssSeparate);
        tbl->column(2)->setValueAt(1, double(dfSeparate));
        tbl->column(3)->setValueAt(1, aiccSeparate);
        tbl->column(4)->setValueAt(1, fStat);
        tbl->column(5)->setValueAt(1, pVal);
        tbl->column(6)->setTextAt(1, conclusion);

        res.reportTable = tbl;
    }

    return res;
}

// ============================================================================
// 4. Multiple Linear Regression (OLS + ANOVA + Exact VIF + Diagnostics)
// ============================================================================

PiecewiseAndRobustFit::MLRResult
PiecewiseAndRobustFit::fitMultipleLinearRegression(ApplicationWindow *app, Table *srcTable,
                                                   const QString &yColName,
                                                   const QStringList &xColNames,
                                                   bool plotDiagnostics)
{
    MLRResult res;
    if (!app || !srcTable || yColName.trimmed().isEmpty() || xColNames.isEmpty())
        return res;

    const int yCol = resolveTableColumnIndex(srcTable, yColName);
    if (yCol < 0)
        return res;

    QVector<int> xCols;
    QStringList xLabels;
    for (const QString &xn : xColNames) {
        const int xc = resolveTableColumnIndex(srcTable, xn);
        if (xc < 0 || xc == yCol || xCols.contains(xc))
            return res;
        xCols.append(xc);
        xLabels << srcTable->colLabel(xc);
    }

    const int k = xCols.size();
    const int p = k + 1; // Intercept + k predictors

    auto readNumericCell = [&](int r, int c, double &valOut) -> bool {
        Column *col = srcTable->column(c);
        if (!col || col->isInvalid(r) || col->isMasked(r))
            return false;
        if (srcTable->text(r, c).trimmed().isEmpty())
            return false;
        const double v = (col->columnMode() == SciDAVis::ColumnMode::Numeric)
                ? col->valueAt(r)
                : srcTable->cell(r, c);
        if (!std::isfinite(v))
            return false;
        valOut = v;
        return true;
    };

    std::vector<double> yObs;
    std::vector<std::vector<double>> xObs;
    const int totalRows = srcTable->numRows();
    yObs.reserve(size_t(std::max(0, totalRows)));
    xObs.reserve(size_t(std::max(0, totalRows)));

    for (int r = 0; r < totalRows; ++r) {
        double yv = 0.0;
        if (!readNumericCell(r, yCol, yv))
            continue;
        std::vector<double> xRow(size_t(k), 0.0);
        bool okRow = true;
        for (int j = 0; j < k; ++j) {
            if (!readNumericCell(r, xCols[j], xRow[size_t(j)])) {
                okRow = false;
                break;
            }
        }
        if (okRow) {
            yObs.push_back(yv);
            xObs.push_back(std::move(xRow));
        }
    }

    const int n = static_cast<int>(yObs.size());
    if (n < p)
        return res;

    gsl_matrix *X = gsl_matrix_alloc(size_t(n), size_t(p));
    gsl_vector *Y = gsl_vector_alloc(size_t(n));
    gsl_vector *C = gsl_vector_alloc(size_t(p));
    gsl_matrix *cov = gsl_matrix_alloc(size_t(p), size_t(p));
    gsl_multifit_linear_workspace *ws = gsl_multifit_linear_alloc(size_t(n), size_t(p));

    for (int i = 0; i < n; ++i) {
        gsl_matrix_set(X, size_t(i), 0, 1.0);
        for (int j = 0; j < k; ++j)
            gsl_matrix_set(X, size_t(i), size_t(j + 1), xObs[size_t(i)][size_t(j)]);
        gsl_vector_set(Y, size_t(i), yObs[size_t(i)]);
    }

    double chisq = 0.0;
    const int st = gsl_multifit_linear(X, Y, C, cov, &chisq, ws);
    if (st != GSL_SUCCESS) {
        gsl_multifit_linear_free(ws);
        gsl_matrix_free(cov);
        gsl_vector_free(C);
        gsl_vector_free(Y);
        gsl_matrix_free(X);
        return res;
    }

    res.betas.resize(p);
    for (int j = 0; j < p; ++j)
        res.betas[j] = gsl_vector_get(C, size_t(j));

    std::vector<double> yHat(size_t(n), 0.0);
    std::vector<double> resid(size_t(n), 0.0);
    double rss = 0.0;
    const double meanY = std::accumulate(yObs.begin(), yObs.end(), 0.0) / double(n);
    double tss = 0.0;
    for (int i = 0; i < n; ++i) {
        double pred = res.betas[0];
        for (int j = 0; j < k; ++j)
            pred += res.betas[j + 1] * xObs[size_t(i)][size_t(j)];
        yHat[size_t(i)] = pred;
        resid[size_t(i)] = yObs[size_t(i)] - pred;
        rss += resid[size_t(i)] * resid[size_t(i)];
        const double dy = yObs[size_t(i)] - meanY;
        tss += dy * dy;
    }

    res.rSquare = (tss > 1e-30) ? std::clamp(1.0 - rss / tss, 0.0, 1.0)
                                : ((rss <= 1e-24) ? 1.0 : 0.0);
    res.adjRSquare = (n > p && tss > 1e-30)
            ? (1.0 - (1.0 - res.rSquare) * double(n - 1) / double(n - p))
            : res.rSquare;

    const int dfErr = std::max(1, n - p);
    const bool noiseless = (rss <= 1e-22 * std::max(1.0, tss));

    res.stdErrors.resize(p);
    res.tValues.resize(p);
    res.pValues.resize(p);
    for (int j = 0; j < p; ++j) {
        const double cjj = (n > p) ? gsl_matrix_get(cov, size_t(j), size_t(j)) : 0.0;
        const double se = (!noiseless && std::isfinite(cjj) && cjj > 0.0) ? std::sqrt(cjj) : 0.0;
        res.stdErrors[j] = se;

        if (se > 1e-30) {
            const double tv = res.betas[j] / se;
            res.tValues[j] = tv;
            res.pValues[j] = (n > p)
                    ? std::clamp(2.0 * gsl_cdf_tdist_Q(std::fabs(tv), double(dfErr)), 0.0, 1.0)
                    : 1.0;
        } else if (std::fabs(res.betas[j]) > 1e-12) {
            res.tValues[j] = (res.betas[j] > 0.0) ? 1e12 : -1e12;
            res.pValues[j] = 0.0;
        } else {
            res.tValues[j] = 0.0;
            res.pValues[j] = 1.0;
        }
    }

    // Overall ANOVA F-test
    const double mss = std::max(0.0, tss - rss);
    const double msReg = mss / double(std::max(1, k));
    const double msErr = rss / double(dfErr);
    if (noiseless && mss > 1e-24) {
        res.fStatistic = 1e12;
        res.fPValue = 0.0;
    } else if (msErr > 1e-300 && n > p) {
        res.fStatistic = msReg / msErr;
        res.fPValue = (res.fStatistic >= 1e11)
                ? 0.0
                : std::clamp(gsl_cdf_fdist_Q(res.fStatistic, double(k), double(dfErr)), 0.0, 1.0);
    } else {
        res.fStatistic = 0.0;
        res.fPValue = 1.0;
    }

    // Exact Variance Inflation Factor (VIF_j = 1 / (1 - R_j^2)) for each predictor X_j
    res.vif.resize(k);
    if (k == 1) {
        res.vif[0] = 1.0;
    } else {
        gsl_matrix *Xsub = gsl_matrix_alloc(size_t(n), size_t(k));
        gsl_vector *Ysub = gsl_vector_alloc(size_t(n));
        gsl_vector *Csub = gsl_vector_alloc(size_t(k));
        gsl_matrix *covSub = gsl_matrix_alloc(size_t(k), size_t(k));
        gsl_multifit_linear_workspace *wsSub = gsl_multifit_linear_alloc(size_t(n), size_t(k));

        for (int m = 0; m < k; ++m) {
            double meanXm = 0.0;
            for (int i = 0; i < n; ++i) {
                const double xmVal = xObs[size_t(i)][size_t(m)];
                meanXm += xmVal;
                gsl_vector_set(Ysub, size_t(i), xmVal);
                gsl_matrix_set(Xsub, size_t(i), 0, 1.0);
                int colPos = 1;
                for (int j = 0; j < k; ++j) {
                    if (j == m)
                        continue;
                    gsl_matrix_set(Xsub, size_t(i), size_t(colPos), xObs[size_t(i)][size_t(j)]);
                    ++colPos;
                }
            }
            meanXm /= double(n);

            double tssM = 0.0;
            for (int i = 0; i < n; ++i) {
                const double dxm = xObs[size_t(i)][size_t(m)] - meanXm;
                tssM += dxm * dxm;
            }

            double chisqSub = 0.0;
            if (tssM <= 1e-30
                || gsl_multifit_linear(Xsub, Ysub, Csub, covSub, &chisqSub, wsSub) != GSL_SUCCESS) {
                res.vif[m] = 1.0;
            } else {
                double rssM = 0.0;
                for (int i = 0; i < n; ++i) {
                    double xHat = gsl_vector_get(Csub, 0);
                    int colPos = 1;
                    for (int j = 0; j < k; ++j) {
                        if (j == m)
                            continue;
                        xHat += gsl_vector_get(Csub, size_t(colPos)) * xObs[size_t(i)][size_t(j)];
                        ++colPos;
                    }
                    const double em = xObs[size_t(i)][size_t(m)] - xHat;
                    rssM += em * em;
                }
                const double r2m = std::clamp(1.0 - rssM / tssM, 0.0, 1.0 - 1e-12);
                res.vif[m] = 1.0 / (1.0 - r2m);
            }
        }

        gsl_multifit_linear_free(wsSub);
        gsl_matrix_free(covSub);
        gsl_vector_free(Csub);
        gsl_vector_free(Ysub);
        gsl_matrix_free(Xsub);
    }

    gsl_multifit_linear_free(ws);
    gsl_matrix_free(cov);
    gsl_vector_free(C);
    gsl_vector_free(Y);
    gsl_matrix_free(X);

    res.valid = true;

    // Create Report Worksheet
    const int reportRows = std::max(p, 6);
    const QString tblName =
            app->generateUniqueName(srcTable->name() + QStringLiteral("_MLR"), false);
    Table *tbl = app->newTable(reportRows, 8, tblName,
                               QObject::tr("Multiple Linear Regression Summary"));
    if (tbl) {
        tbl->setHeader(QStringList() << QObject::tr("Term") << QObject::tr("Beta")
                                     << QObject::tr("Std_Error") << QObject::tr("t_Value")
                                     << QObject::tr("p_Value") << QObject::tr("VIF")
                                     << QObject::tr("Model_Metric") << QObject::tr("Metric_Value"));
        tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 1; c <= 5; ++c)
            tbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        tbl->column(6)->setColumnMode(SciDAVis::ColumnMode::Text);
        tbl->column(7)->setColumnMode(SciDAVis::ColumnMode::Numeric);

        for (int r = 0; r < reportRows; ++r) {
            if (r < p) {
                tbl->column(0)->setTextAt(r, (r == 0) ? QStringLiteral("(Intercept)")
                                                      : xLabels[r - 1]);
                tbl->column(1)->setValueAt(r, res.betas[r]);
                tbl->column(2)->setValueAt(r, res.stdErrors[r]);
                tbl->column(3)->setValueAt(r, res.tValues[r]);
                tbl->column(4)->setValueAt(r, res.pValues[r]);
                if (r == 0)
                    tbl->column(5)->setInvalid(r, true);
                else
                    tbl->column(5)->setValueAt(r, res.vif[r - 1]);
            } else {
                for (int c = 1; c <= 5; ++c)
                    tbl->column(c)->setInvalid(r, true);
            }
        }

        const QVector<QPair<QString, double>> modelSummary = {
            { QStringLiteral("R_Square"), res.rSquare },
            { QStringLiteral("Adj_R_Square"), res.adjRSquare },
            { QStringLiteral("ANOVA_F"), res.fStatistic },
            { QStringLiteral("ANOVA_p"), res.fPValue },
            { QStringLiteral("N_Observations"), double(n) },
            { QStringLiteral("RSS"), rss }
        };
        for (int r = 0; r < reportRows; ++r) {
            if (r < modelSummary.size()) {
                tbl->column(6)->setTextAt(r, modelSummary[r].first);
                tbl->column(7)->setValueAt(r, modelSummary[r].second);
            } else {
                tbl->column(7)->setInvalid(r, true);
            }
        }
        res.reportTable = tbl;
    }

    // Optional Predicted vs. Actual + Residual Diagnostic MultiLayer (explicit Arial fonts)
    if (plotDiagnostics) {
        const QString diagTblName =
                app->generateUniqueName(srcTable->name() + QStringLiteral("_MLR_Diag"), false);
        Table *diagTbl = app->newTable(n, 3, diagTblName, QObject::tr("MLR Diagnostics Data"));
        if (diagTbl) {
            diagTbl->setHeader(QStringList() << QObject::tr("Predicted_Y") << QObject::tr("Actual_Y")
                                             << QObject::tr("Residual"));
            for (int c = 0; c < 3; ++c)
                diagTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            diagTbl->column(0)->setPlotDesignation(SciDAVis::X);
            diagTbl->column(1)->setPlotDesignation(SciDAVis::Y);
            diagTbl->column(2)->setPlotDesignation(SciDAVis::Y);

            double minPred = yHat[0], maxPred = yHat[0];
            for (int i = 0; i < n; ++i) {
                diagTbl->column(0)->setValueAt(i, yHat[size_t(i)]);
                diagTbl->column(1)->setValueAt(i, yObs[size_t(i)]);
                diagTbl->column(2)->setValueAt(i, resid[size_t(i)]);
                minPred = std::min(minPred, yHat[size_t(i)]);
                maxPred = std::max(maxPred, yHat[size_t(i)]);
            }

            MultiLayer *ml = app->newGraph(
                    app->generateUniqueName(QStringLiteral("MLR_Diagnostics"), false));
            if (ml) {
                Graph *g1 = ml->activeGraph();
                Graph *g2 = ml->addLayer();
                ml->setRows(1);
                ml->setCols(2);
                ml->arrangeLayers(true, true);

                const QFont titleFnt("Arial", 11, QFont::Bold);
                const QFont axisFnt("Arial", 10, QFont::Bold);
                const QFont numFnt("Arial", 9);
                const QFont legFnt("Arial", 9);
                ml->setFonts(titleFnt, axisFnt, numFnt, legFnt);

                if (g1) {
                    if (g1->insertCurve(diagTbl, diagTbl->colName(0), diagTbl->colName(1),
                                        Graph::Scatter)) {
                        const int cIdx = g1->curves() - 1;
                        QwtSymbol sym(QwtSymbol::Ellipse, QBrush(QColor(31, 119, 180, 190)),
                                      QPen(QColor(20, 80, 140), 1), QSize(7, 7));
                        g1->setCurveSymbol(cIdx, sym);
                    }
                    if (maxPred > minPred) {
                        FunctionCurve *ident =
                                new FunctionCurve(app, FunctionCurve::Normal, QStringLiteral("1:1"));
                        ident->setPen(QPen(QColor(215, 25, 28), 1, Qt::DashLine));
                        const double ix[2] = { minPred, maxPred };
                        const double iy[2] = { minPred, maxPred };
                        ident->setData(ix, iy, 2);
                        ident->setRange(minPred, maxPred);
                        ident->setFormula(QStringLiteral("x"));
                        g1->insertPlotItem(ident, Graph::Line);
                    }
                    g1->setTitle(QObject::tr("Actual vs. Predicted (R² = %1)")
                                         .arg(res.rSquare, 0, 'f', 4));
                    g1->setTitleFont(titleFnt);
                    g1->setXAxisTitle(QObject::tr("Predicted %1").arg(srcTable->colLabel(yCol)));
                    g1->setYAxisTitle(QObject::tr("Actual %1").arg(srcTable->colLabel(yCol)));
                    g1->initFonts(axisFnt, numFnt);
                    if (g1->legend())
                        g1->legend()->setFont(legFnt);
                    g1->replot();
                }

                if (g2) {
                    if (g2->insertCurve(diagTbl, diagTbl->colName(0), diagTbl->colName(2),
                                        Graph::Scatter)) {
                        const int cIdx = g2->curves() - 1;
                        QwtSymbol sym(QwtSymbol::Ellipse, QBrush(QColor(44, 160, 44, 190)),
                                      QPen(QColor(25, 110, 25), 1), QSize(7, 7));
                        g2->setCurveSymbol(cIdx, sym);
                    }
                    if (maxPred > minPred) {
                        FunctionCurve *zeroLn = new FunctionCurve(app, FunctionCurve::Normal,
                                                                  QStringLiteral("ZeroResidual"));
                        zeroLn->setPen(QPen(Qt::darkGray, 1, Qt::DashLine));
                        const double zx[2] = { minPred, maxPred };
                        const double zy[2] = { 0.0, 0.0 };
                        zeroLn->setData(zx, zy, 2);
                        zeroLn->setRange(minPred, maxPred);
                        zeroLn->setFormula(QStringLiteral("0"));
                        g2->insertPlotItem(zeroLn, Graph::Line);
                    }
                    g2->setTitle(QObject::tr("Residuals vs. Predicted"));
                    g2->setTitleFont(titleFnt);
                    g2->setXAxisTitle(QObject::tr("Predicted %1").arg(srcTable->colLabel(yCol)));
                    g2->setYAxisTitle(QObject::tr("Residual"));
                    g2->initFonts(axisFnt, numFnt);
                    if (g2->legend())
                        g2->legend()->setFont(legFnt);
                    g2->replot();
                }

                ml->setFonts(titleFnt, axisFnt, numFnt, legFnt);
                res.diagnosticGraph = ml;
            }
        }
    }

    return res;
}

// ============================================================================
// 5. Interactive Dialogs (100% tr(...) localized)
// ============================================================================

static Graph *resolveDialogGraph(ApplicationWindow *app, Graph *g)
{
    if (g)
        return g;
    if (!app)
        return nullptr;
    const QList<MyWidget *> wins = app->windowsList();
    for (MyWidget *w : wins) {
        if (auto *ml = qobject_cast<MultiLayer *>(w)) {
            if (ml->activeGraph())
                return ml->activeGraph();
        }
    }
    return nullptr;
}

void PiecewiseAndRobustFit::showPiecewiseFitDialog(ApplicationWindow *app, Graph *g)
{
    if (!app)
        return;
    g = resolveDialogGraph(app, g);
    if (!g || g->curves() <= 0) {
        QMessageBox::information(app, QObject::tr("Piecewise Linear Fit"),
                                 QObject::tr("Please activate a graph with at least one data curve."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Piecewise Linear Breakpoint Fit"));
    dlg.setMinimumWidth(380);

    auto *mainLay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();

    auto *curveCombo = new QComboBox(&dlg);
    QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty()) {
        for (int i = 0; i < g->curves(); ++i) {
            if (QwtPlotCurve *c = g->curve(i))
                curves << c->title().text();
        }
    }
    curveCombo->addItems(curves);
    form->addRow(QObject::tr("Target Curve:"), curveCombo);

    auto *segSpin = new QSpinBox(&dlg);
    segSpin->setRange(2, 3);
    segSpin->setValue(2);
    form->addRow(QObject::tr("Number of Linear Segments:"), segSpin);

    auto *annotateChk = new QCheckBox(QObject::tr("Add summary box on graph"), &dlg);
    annotateChk->setChecked(true);
    form->addRow(QString(), annotateChk);

    mainLay->addLayout(form);

    auto *btns = new QDialogButtonBox(&dlg);
    QPushButton *okBtn = btns->addButton(QObject::tr("Fit"), QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn = btns->addButton(QObject::tr("Cancel"), QDialogButtonBox::RejectRole);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    mainLay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        const auto out = fitPiecewiseLinear(app, g, curveCombo->currentText(), segSpin->value(),
                                            annotateChk->isChecked());
        if (!out.converged) {
            QMessageBox::warning(app, QObject::tr("Piecewise Linear Fit"),
                                 QObject::tr("Piecewise fit could not converge on the selected curve."));
        }
    }
}

void PiecewiseAndRobustFit::showRobustFitDialog(ApplicationWindow *app, Graph *g)
{
    if (!app)
        return;
    g = resolveDialogGraph(app, g);
    if (!g || g->curves() <= 0) {
        QMessageBox::information(app, QObject::tr("Robust / Quantile Regression"),
                                 QObject::tr("Please activate a graph with at least one data curve."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Robust & Quantile Linear Regression"));
    dlg.setMinimumWidth(400);

    auto *mainLay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();

    auto *curveCombo = new QComboBox(&dlg);
    QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty()) {
        for (int i = 0; i < g->curves(); ++i) {
            if (QwtPlotCurve *c = g->curve(i))
                curves << c->title().text();
        }
    }
    curveCombo->addItems(curves);
    form->addRow(QObject::tr("Target Curve:"), curveCombo);

    auto *methodCombo = new QComboBox(&dlg);
    methodCombo->addItem(QObject::tr("Tukey Bisquare M-Estimator (c = 4.685)"), int(TukeyBisquare));
    methodCombo->addItem(QObject::tr("Huber M-Estimator (c = 1.345)"), int(HuberM));
    methodCombo->addItem(QObject::tr("Quantile Regression (Pinball Loss)"), int(QuantileReg));
    methodCombo->addItem(QObject::tr("Ordinary Least Squares (OLS)"), int(OrdinaryLS));
    form->addRow(QObject::tr("Estimator:"), methodCombo);

    auto *tauSpin = new QDoubleSpinBox(&dlg);
    tauSpin->setRange(0.01, 0.99);
    tauSpin->setSingleStep(0.05);
    tauSpin->setDecimals(2);
    tauSpin->setValue(0.50);
    tauSpin->setEnabled(false);
    form->addRow(QObject::tr("Quantile (tau):"), tauSpin);

    QObject::connect(methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [methodCombo, tauSpin](int) {
                         tauSpin->setEnabled(methodCombo->currentData().toInt() == int(QuantileReg));
                     });

    auto *annotateChk = new QCheckBox(QObject::tr("Add summary box on graph"), &dlg);
    annotateChk->setChecked(true);
    form->addRow(QString(), annotateChk);

    mainLay->addLayout(form);

    auto *btns = new QDialogButtonBox(&dlg);
    QPushButton *okBtn = btns->addButton(QObject::tr("Fit"), QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn = btns->addButton(QObject::tr("Cancel"), QDialogButtonBox::RejectRole);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    mainLay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        const auto m = static_cast<RobustMethod>(methodCombo->currentData().toInt());
        const auto out = fitRobustOrQuantileLinear(app, g, curveCombo->currentText(), m,
                                                   tauSpin->value(), annotateChk->isChecked());
        if (!out.converged) {
            QMessageBox::warning(app, QObject::tr("Robust / Quantile Regression"),
                                 QObject::tr("Regression could not converge on the selected curve."));
        }
    }
}

void PiecewiseAndRobustFit::showCompareDatasetsDialog(ApplicationWindow *app, Graph *g)
{
    if (!app)
        return;
    g = resolveDialogGraph(app, g);
    if (!g || g->curves() < 2) {
        QMessageBox::information(
                app, QObject::tr("Compare Datasets (Extra-Sum-of-Squares F-Test)"),
                QObject::tr("Please activate a graph containing at least two data curves."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Compare Datasets (Extra-Sum-of-Squares F-Test)"));
    dlg.setMinimumWidth(420);

    auto *mainLay = new QVBoxLayout(&dlg);
    auto *grp = new QGroupBox(QObject::tr("Select Datasets to Compare (>= 2)"), &dlg);
    auto *grpLay = new QVBoxLayout(grp);
    auto *listWidget = new QListWidget(grp);
    listWidget->setSelectionMode(QAbstractItemView::NoSelection);

    QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty()) {
        for (int i = 0; i < g->curves(); ++i) {
            if (QwtPlotCurve *c = g->curve(i))
                curves << c->title().text();
        }
    }
    for (const QString &cn : curves) {
        auto *item = new QListWidgetItem(cn, listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    grpLay->addWidget(listWidget);
    mainLay->addWidget(grp);

    auto *form = new QFormLayout();
    auto *modelCombo = new QComboBox(&dlg);
    QStringList models = NonLinearFit::builtInModelNames();
    if (!models.contains(QStringLiteral("Linear")))
        models.prepend(QStringLiteral("Linear"));
    modelCombo->addItems(models);
    const int expIdx = modelCombo->findText(QStringLiteral("ExpDec1"));
    if (expIdx >= 0)
        modelCombo->setCurrentIndex(expIdx);
    form->addRow(QObject::tr("Model Function:"), modelCombo);
    mainLay->addLayout(form);

    auto *btns = new QDialogButtonBox(&dlg);
    QPushButton *okBtn = btns->addButton(QObject::tr("Compare"), QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn = btns->addButton(QObject::tr("Cancel"), QDialogButtonBox::RejectRole);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    mainLay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        QStringList chosen;
        for (int i = 0; i < listWidget->count(); ++i) {
            if (listWidget->item(i)->checkState() == Qt::Checked)
                chosen << listWidget->item(i)->text();
        }
        if (chosen.size() < 2) {
            QMessageBox::warning(app, QObject::tr("Compare Datasets"),
                                 QObject::tr("Please check at least two datasets."));
            return;
        }
        const auto out = compareDatasets(app, g, chosen, modelCombo->currentText());
        if (!out.valid) {
            QMessageBox::warning(app, QObject::tr("Compare Datasets"),
                                 QObject::tr("Dataset comparison failed (insufficient degrees of freedom or model mismatch)."));
        }
    }
}

void PiecewiseAndRobustFit::showMLRDialog(ApplicationWindow *app)
{
    if (!app)
        return;

    QList<Table *> tables;
    if (app->tableList()) {
        for (MyWidget *w : *app->tableList()) {
            if (auto *t = qobject_cast<Table *>(w)) {
                if (t->numCols() >= 2)
                    tables.append(t);
            }
        }
    }
    if (tables.isEmpty()) {
        QMessageBox::information(app, QObject::tr("Multiple Linear Regression"),
                                 QObject::tr("Please open a worksheet with at least two numeric columns."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Multiple Linear Regression (OLS & VIF)"));
    dlg.setMinimumWidth(430);

    auto *mainLay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();

    auto *tblCombo = new QComboBox(&dlg);
    for (Table *t : tables)
        tblCombo->addItem(t->name());
    form->addRow(QObject::tr("Worksheet:"), tblCombo);

    auto *yCombo = new QComboBox(&dlg);
    form->addRow(QObject::tr("Dependent Variable (Y):"), yCombo);
    mainLay->addLayout(form);

    auto *xGrp = new QGroupBox(QObject::tr("Predictor Variables (X1, X2, ...)"), &dlg);
    auto *xLay = new QVBoxLayout(xGrp);
    auto *xList = new QListWidget(xGrp);
    xList->setSelectionMode(QAbstractItemView::NoSelection);
    xLay->addWidget(xList);
    mainLay->addWidget(xGrp);

    auto *diagChk = new QCheckBox(QObject::tr("Create Predicted vs. Actual & Residual Diagnostic Graph"), &dlg);
    diagChk->setChecked(true);
    mainLay->addWidget(diagChk);

    auto repopulateCols = [&tables, tblCombo, yCombo, xList]() {
        const int idx = tblCombo->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        yCombo->clear();
        xList->clear();
        for (int c = 0; c < t->numCols(); ++c) {
            const QString lbl = t->colLabel(c);
            yCombo->addItem(lbl);
            auto *it = new QListWidgetItem(lbl, xList);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            it->setCheckState((c > 0) ? Qt::Checked : Qt::Unchecked);
        }
        if (t->numCols() > 0) {
            yCombo->setCurrentIndex(t->numCols() - 1);
            xList->item(t->numCols() - 1)->setCheckState(Qt::Unchecked);
            if (t->numCols() >= 2)
                xList->item(0)->setCheckState(Qt::Checked);
        }
    };
    QObject::connect(tblCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [repopulateCols](int) { repopulateCols(); });
    repopulateCols();

    auto *btns = new QDialogButtonBox(&dlg);
    QPushButton *okBtn = btns->addButton(QObject::tr("Run Regression"), QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn = btns->addButton(QObject::tr("Cancel"), QDialogButtonBox::RejectRole);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    mainLay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        const int idx = tblCombo->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        const QString yName = yCombo->currentText();
        QStringList xNames;
        for (int i = 0; i < xList->count(); ++i) {
            if (xList->item(i)->checkState() == Qt::Checked && xList->item(i)->text() != yName)
                xNames << xList->item(i)->text();
        }
        if (xNames.isEmpty()) {
            QMessageBox::warning(app, QObject::tr("Multiple Linear Regression"),
                                 QObject::tr("Please select at least one predictor column distinct from Y."));
            return;
        }
        const auto out = fitMultipleLinearRegression(app, t, yName, xNames, diagChk->isChecked());
        if (!out.valid) {
            QMessageBox::warning(app, QObject::tr("Multiple Linear Regression"),
                                 QObject::tr("Multiple linear regression failed (insufficient valid rows)."));
        }
    }
}
