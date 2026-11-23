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
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <gsl/gsl_cdf.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_fit.h>
#include <gsl/gsl_linalg.h>
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

    int activePointCount() const { return effectivePointCount(); }

    double weightedOrUnweightedTss() const
    {
        const size_t m = d_y.size();
        if (m == 0)
            return 0.0;
        if (d_y_error_source == Fit::UnknownErrors || d_y_errors.size() != m) {
            const double meanY = std::accumulate(d_y.begin(), d_y.end(), 0.0) / double(m);
            double tss = 0.0;
            for (double yi : d_y) {
                const double dy = yi - meanY;
                tss += dy * dy;
            }
            return tss;
        }
        double wSum = 0.0, wySum = 0.0;
        for (size_t i = 0; i < m; ++i) {
            const double s = d_y_errors[i];
            if (!std::isfinite(s) || !(s > 0.0) || s >= 1e140)
                continue;
            const double w = 1.0 / (s * s);
            wSum += w;
            wySum += w * d_y[i];
        }
        if (!(wSum > 0.0))
            return 0.0;
        const double meanY = wySum / wSum;
        double tss = 0.0;
        for (size_t i = 0; i < m; ++i) {
            const double s = d_y_errors[i];
            if (!std::isfinite(s) || !(s > 0.0) || s >= 1e140)
                continue;
            const double dy = (d_y[i] - meanY) / s;
            tss += dy * dy;
        }
        return tss;
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

static bool invertMatrixGsl(const std::vector<std::vector<double>> &mat,
                            std::vector<std::vector<double>> &invOut)
{
    const size_t d = mat.size();
    if (d == 0)
        return false;

    gsl_error_handler_t *oldHandler = gsl_set_error_handler_off();
    gsl_matrix *A = gsl_matrix_alloc(d, d);
    gsl_matrix *Inv = gsl_matrix_alloc(d, d);
    gsl_permutation *perm = gsl_permutation_alloc(d);
    if (!A || !Inv || !perm) {
        if (A)
            gsl_matrix_free(A);
        if (Inv)
            gsl_matrix_free(Inv);
        if (perm)
            gsl_permutation_free(perm);
        gsl_set_error_handler(oldHandler);
        return false;
    }

    for (size_t r = 0; r < d; ++r) {
        for (size_t c = 0; c < d; ++c)
            gsl_matrix_set(A, r, c, mat[r][c]);
    }

    int signum = 0;
    bool ok = (gsl_linalg_LU_decomp(A, perm, &signum) == GSL_SUCCESS)
            && (gsl_linalg_LU_invert(A, perm, Inv) == GSL_SUCCESS);

    invOut.assign(d, std::vector<double>(d, 0.0));
    if (ok) {
        for (size_t r = 0; r < d; ++r) {
            for (size_t c = 0; c < d; ++c) {
                const double v = gsl_matrix_get(Inv, r, c);
                if (!std::isfinite(v))
                    ok = false;
                invOut[r][c] = v;
            }
        }
    }

    gsl_permutation_free(perm);
    gsl_matrix_free(Inv);
    gsl_matrix_free(A);
    gsl_set_error_handler(oldHandler);
    return ok;
}

struct DialogPreviewCurveGuard
{
    QPointer<Graph> graph;
    QwtPlotCurve *curve = nullptr;
    ~DialogPreviewCurveGuard() { clear(); }
    void clear()
    {
        if (curve) {
            if (graph && graph->plotWidget()) {
                curve->detach();
                graph->replot();
            }
            delete curve;
            curve = nullptr;
        }
    }
    void showData(const QString &srcCurveTitle, const std::vector<double> &gx,
                  const std::vector<double> &gy, const QColor &color)
    {
        if (!graph || !graph->plotWidget() || gx.size() < 2 || gy.size() != gx.size()) {
            clear();
            return;
        }
        if (!curve) {
            curve = new QwtPlotCurve(QObject::tr("Live Fit Preview"));
            curve->setItemAttribute(QwtPlotItem::Legend, false);
            curve->attach(graph->plotWidget());
        }
        curve->setPen(QPen(color, 2, Qt::DashLine));
        if (QwtPlotCurve *src = resolveCurveOnGraph(graph, srcCurveTitle))
            curve->setAxis(src->xAxis(), src->yAxis());
        curve->setData(gx.data(), gy.data(), static_cast<int>(gx.size()));
        graph->replot();
    }
};

static bool computePiecewiseCore(const std::vector<double> &xv, const std::vector<double> &yv,
                                 int numSegments, PiecewiseAndRobustFit::PiecewiseResult &res,
                                 double &bestRssOut, std::vector<double> &gxOut,
                                 std::vector<double> &gyOut)
{
    const int S = std::clamp(numSegments, 2, 3);
    res.segments = S;
    const size_t n = xv.size();
    if (n < size_t(2 * S) || yv.size() != n)
        return false;

    const double xMin = xv.front();
    const double xMax = xv.back();
    const double xSpan = xMax - xMin;
    if (!(xSpan > 1e-15))
        return false;

    HingeContext ctx;
    ctx.numKnots = S - 1;
    ctx.u.resize(n);
    ctx.y = yv;
    for (size_t i = 0; i < n; ++i)
        ctx.u[i] = (xv[i] - xMin) / xSpan;

    std::vector<double> uDistinct;
    uDistinct.reserve(n);
    for (double ui : ctx.u) {
        if (uDistinct.empty() || (ui - uDistinct.back()) > 1e-12)
            uDistinct.push_back(ui);
    }
    if (uDistinct.size() < size_t(2 * S))
        return false;

    const double tauLo = std::max(1e-5, ctx.u[1] + 1e-7);
    const double tauHi = std::min(1.0 - 1e-5, ctx.u[n - 2] - 1e-7);
    if (!(tauHi > tauLo))
        return false;

    ctx.minGap = std::max(1e-4, 0.5 / double(uDistinct.size()));

    std::vector<double> bestTau(size_t(S - 1), 0.5);
    double bestRss = 1e100;

    if (S == 2) {
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
        return false;

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

    const int K = S - 1;
    res.breakpointsX.resize(K);
    res.breakpointsXSE.fill(0.0, K);
    res.breakpointsY.resize(K);
    for (int j = 0; j < K; ++j) {
        const double bpX = xMin + bestTau[size_t(j)] * xSpan;
        res.breakpointsX[j] = bpX;
        res.breakpointsY[j] = evalModelY(bpX);
    }

    double yAbsMax = 0.0;
    for (double yi : yv)
        yAbsMax = std::max(yAbsMax, std::fabs(yi));
    const double yScale = std::max(yAbsMax, std::numeric_limits<double>::min());
    const double yScale2 = yScale * yScale;

    const double meanY = std::accumulate(yv.begin(), yv.end(), 0.0) / double(n);
    double tss = 0.0;
    for (double yi : yv) {
        const double dy = yi - meanY;
        tss += dy * dy;
    }
    const double tssRef = std::max(tss, yScale2);
    const double tssTiny = 1e-28 * yScale2;
    res.rSquare = (tss > tssTiny) ? std::clamp(1.0 - bestRss / tss, 0.0, 1.0)
                                  : ((bestRss <= 1e-24 * tssRef) ? 1.0 : 0.0);
    const int kFree = 2 * S; // a0 + S slopes + (S-1) breakpoints = 2*S
    const double effRss = std::max(bestRss, 1e-30 * tssRef);
    res.aic = double(n) * std::log(effRss / double(n)) + 2.0 * double(kFree + 1);

    // Compute Breakpoint X Standard Errors (Delta-Method Segmented Fisher Info + Profile-RSS Fallback)
    const int dof = std::max(1, static_cast<int>(n) - kFree);
    const bool noiseless = (bestRss <= 1e-24 * tssRef);
    if (!noiseless && static_cast<int>(n) > kFree) {
        const double s2 = bestRss / double(dof);
        const size_t dim = static_cast<size_t>(kFree);
        std::vector<std::vector<double>> jtj(dim, std::vector<double>(dim, 0.0));
        std::vector<double> jRow(dim, 0.0);
        for (size_t i = 0; i < n; ++i) {
            const double ui = ctx.u[i];
            jRow[0] = 1.0;
            jRow[1] = ui;
            for (int j = 0; j < K; ++j) {
                const double tj = bestTau[size_t(j)];
                const double dGamma = coeffs[size_t(2 + j)];
                jRow[size_t(2 + j)] = (ui > tj) ? (ui - tj) : 0.0;
                jRow[size_t(2 + K + j)] = (ui > tj) ? (-dGamma) : 0.0;
            }
            for (size_t r = 0; r < dim; ++r) {
                for (size_t c = r; c < dim; ++c)
                    jtj[r][c] += jRow[r] * jRow[c];
            }
        }
        double trG = 0.0;
        for (size_t r = 0; r < dim; ++r) {
            trG += jtj[r][r];
            for (size_t c = 0; c < r; ++c)
                jtj[r][c] = jtj[c][r];
        }
        const double tinyRidge = std::max(1e-18, 1e-13 * trG / double(dim));
        std::vector<std::vector<double>> jtjReg = jtj;
        for (size_t r = 0; r < dim; ++r)
            jtjReg[r][r] += tinyRidge;

        std::vector<std::vector<double>> invG;
        const bool invOk = invertMatrixGsl(jtjReg, invG);

        // Profile-likelihood 1-SE interval helper: RSS(tau_j) <= bestRss + s^2
        auto profileTauSE = [&](int knotIdx) -> double {
            const double t0 = bestTau[size_t(knotIdx)];
            const double loLim = (knotIdx == 0) ? tauLo
                                                : std::min(tauHi, bestTau[size_t(knotIdx - 1)] + ctx.minGap);
            const double hiLim = (knotIdx + 1 == K) ? tauHi
                                                    : std::max(tauLo, bestTau[size_t(knotIdx + 1)] - ctx.minGap);
            const double critRss = bestRss + s2;
            const int nProbe = 120;

            double tLeft = loLim;
            for (int s = 1; s <= nProbe; ++s) {
                double cand = t0 - (t0 - loLim) * double(s) / double(nProbe);
                std::vector<double> trial = bestTau;
                trial[size_t(knotIdx)] = cand;
                const double r = ctx.evalObjective(trial);
                if (r >= critRss && r < 1e90) {
                    const double prevCand = t0 - (t0 - loLim) * double(s - 1) / double(nProbe);
                    std::vector<double> prevTrial = bestTau;
                    prevTrial[size_t(knotIdx)] = prevCand;
                    const double rPrev = ctx.evalObjective(prevTrial);
                    const double frac = (r > rPrev + 1e-30 * tssRef)
                            ? std::clamp((critRss - rPrev) / (r - rPrev), 0.0, 1.0)
                            : 0.5;
                    tLeft = prevCand + frac * (cand - prevCand);
                    break;
                }
            }

            double tRight = hiLim;
            for (int s = 1; s <= nProbe; ++s) {
                double cand = t0 + (hiLim - t0) * double(s) / double(nProbe);
                std::vector<double> trial = bestTau;
                trial[size_t(knotIdx)] = cand;
                const double r = ctx.evalObjective(trial);
                if (r >= critRss && r < 1e90) {
                    const double prevCand = t0 + (hiLim - t0) * double(s - 1) / double(nProbe);
                    std::vector<double> prevTrial = bestTau;
                    prevTrial[size_t(knotIdx)] = prevCand;
                    const double rPrev = ctx.evalObjective(prevTrial);
                    const double frac = (r > rPrev + 1e-30 * tssRef)
                            ? std::clamp((critRss - rPrev) / (r - rPrev), 0.0, 1.0)
                            : 0.5;
                    tRight = prevCand + frac * (cand - prevCand);
                    break;
                }
            }
            return std::max(0.0, 0.5 * (tRight - tLeft));
        };

        for (int j = 0; j < K; ++j) {
            const double dGamma = std::fabs(coeffs[size_t(2 + j)]);
            const size_t idxBp = size_t(2 + K + j);
            double seTau = 0.0;
            if (invOk && dGamma > 1e-8 * yScale && invG[idxBp][idxBp] > 0.0) {
                const double deltaSE = std::sqrt(s2 * invG[idxBp][idxBp]);
                if (std::isfinite(deltaSE) && deltaSE <= 0.5) {
                    seTau = deltaSE;
                } else {
                    seTau = profileTauSE(j);
                }
            } else {
                seTau = profileTauSE(j);
            }
            res.breakpointsXSE[j] = (std::isfinite(seTau) && seTau > 0.0) ? (seTau * xSpan) : 0.0;
        }
    }

    res.converged = true;
    bestRssOut = bestRss;

    gxOut.clear();
    gxOut.reserve(210);
    for (int k = 0; k <= 200; ++k)
        gxOut.push_back(xMin + xSpan * double(k) / 200.0);
    for (double bpX : res.breakpointsX)
        gxOut.push_back(bpX);
    std::sort(gxOut.begin(), gxOut.end());
    gxOut.erase(std::unique(gxOut.begin(), gxOut.end(),
                            [xSpan](double a, double b) { return std::fabs(a - b) <= 1e-13 * xSpan; }),
                gxOut.end());

    gyOut.resize(gxOut.size());
    for (size_t i = 0; i < gxOut.size(); ++i)
        gyOut[i] = evalModelY(gxOut[i]);

    return true;
}

static bool computeRobustOrQuantileCore(const std::vector<double> &xv,
                                        const std::vector<double> &yv,
                                        PiecewiseAndRobustFit::RobustMethod method,
                                        double quantileTau,
                                        PiecewiseAndRobustFit::RobustFitResult &res)
{
    const size_t n = xv.size();
    if (n < 2 || yv.size() != n)
        return false;

    const double xSpan = xv.back() - xv.front();
    if (!(xSpan > 1e-15))
        return false;

    if (method == PiecewiseAndRobustFit::OrdinaryLS) {
        double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0, sumsq = 0.0;
        if (gsl_fit_linear(xv.data(), 1, yv.data(), 1, n, &c0, &c1, &cov00, &cov01, &cov11, &sumsq)
            != GSL_SUCCESS)
            return false;

        std::vector<double> resid(n);
        for (size_t i = 0; i < n; ++i)
            resid[i] = yv[i] - (c0 + c1 * xv[i]);

        res.converged = true;
        res.intercept = c0;
        res.slope = c1;
        // gsl_fit_linear() already scales cov by the residual variance.  With
        // n == 2 there are no residual degrees of freedom -> SE undefined.
        const double nanSE = std::numeric_limits<double>::quiet_NaN();
        res.interceptSE = (n > 2) ? ((cov00 >= 0.0) ? std::sqrt(cov00) : nanSE) : nanSE;
        res.slopeSE = (n > 2) ? ((cov11 >= 0.0) ? std::sqrt(cov11) : nanSE) : nanSE;
        res.scaleMAD = computeMADScale(resid);
        return true;
    }

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
        return false;

    double curSlope = computeVectorMedian(std::move(pairSlopes));
    std::vector<double> candIntercepts(n);
    for (size_t i = 0; i < n; ++i)
        candIntercepts[i] = yv[i] - curSlope * xv[i];

    const double tau = std::clamp(quantileTau, 0.01, 0.99);
    double curIntercept = (method == PiecewiseAndRobustFit::QuantileReg)
            ? computeVectorQuantile(std::move(candIntercepts), tau)
            : computeVectorMedian(std::move(candIntercepts));

    // Data-scale-relative robust scale (no absolute 1.0 floor: strain ~1e-4,
    // diffusion coefficients ~1e-11 or nA currents must be handled correctly).
    double yAbsMax = 0.0, yMin = std::numeric_limits<double>::infinity(),
           yMax = -std::numeric_limits<double>::infinity();
    for (double yi : yv) {
        yAbsMax = std::max(yAbsMax, std::fabs(yi));
        yMin = std::min(yMin, yi);
        yMax = std::max(yMax, yi);
    }
    const double ySpan = (yMax > yMin) ? (yMax - yMin) : 0.0;
    double yScale = std::max(ySpan, yAbsMax);
    if (!(yScale > 0.0) || !std::isfinite(yScale))
        yScale = std::numeric_limits<double>::min();
    const double madRaw = computeMADScale(yv);
    const double madY = (madRaw > 1e-12 * yScale && std::isfinite(madRaw)) ? madRaw
                                                                            : std::max(1e-12 * yScale, std::numeric_limits<double>::min());
    const double minScaleFloor = 1e-7 * madY;

    std::vector<double> resid(n, 0.0);
    std::vector<double> weights(n, 1.0);
    double curScaleMAD = 0.0;
    double lastCov00 = 0.0, lastCov11 = 0.0, lastChisq = 0.0;
    bool haveCov = false;
    bool converged = false;
    bool degenerateWeights = false;
    double lastDelta = std::numeric_limits<double>::infinity();

    const int maxIter = (method == PiecewiseAndRobustFit::QuantileReg) ? 160 : 100;
    for (int iter = 0; iter < maxIter; ++iter) {
        for (size_t i = 0; i < n; ++i)
            resid[i] = yv[i] - (curIntercept + curSlope * xv[i]);

        curScaleMAD = computeMADScale(resid);
        const double s = std::max(curScaleMAD, minScaleFloor);

        int posWeightCount = 0;
        double minWeightedX = std::numeric_limits<double>::infinity();
        double maxWeightedX = -std::numeric_limits<double>::infinity();

        if (method == PiecewiseAndRobustFit::HuberM) {
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
        } else if (method == PiecewiseAndRobustFit::TukeyBisquare) {
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
            const double epsPinball = std::max(1e-12 * yScale, 1e-6 * madY);
            for (size_t i = 0; i < n; ++i) {
                const double asym = (resid[i] >= 0.0) ? tau : (1.0 - tau);
                weights[i] = asym / std::max(std::fabs(resid[i]), epsPinball);
                ++posWeightCount;
                minWeightedX = std::min(minWeightedX, xv[i]);
                maxWeightedX = std::max(maxWeightedX, xv[i]);
            }
        }

        if (posWeightCount < 2 || !(maxWeightedX > minWeightedX + 1e-13 * xSpan)) {
            // All (or all but one) points were rejected, or the remaining
            // points share one X: the line is not identifiable.  This is NOT
            // convergence - report it as such and leave SE undefined.
            degenerateWeights = true;
            converged = false;
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
        haveCov = true;

        const double delta = std::fabs(nextInt - curIntercept)
                + std::fabs(nextSlope - curSlope) * xSpan;
        lastDelta = delta;
        curIntercept = nextInt;
        curSlope = nextSlope;
        if (delta <= 1e-11 * std::max(madY, 1e-15 * yScale)) {
            converged = true;
            break;
        }
        // Reaching maxIter without meeting the tolerance is reported as
        // "not converged" (the last iterate is still returned to the caller).
    }
    if (!converged && !degenerateWeights && haveCov && std::isfinite(lastDelta)) {
        // The smoothed-pinball IRLS for quantile regression approaches the LP
        // optimum only asymptotically; accept a looser (still data-scale
        // relative) step criterion.  M-estimators get a slightly looser
        // criterion than the strict one as well.
        const double relaxedTol = (method == PiecewiseAndRobustFit::QuantileReg) ? 1e-6 : 1e-8;
        if (lastDelta <= relaxedTol * madY)
            converged = true;
    }

    for (size_t i = 0; i < n; ++i)
        resid[i] = yv[i] - (curIntercept + curSlope * xv[i]);
    curScaleMAD = computeMADScale(resid);

    res.converged = converged;
    res.intercept = curIntercept;
    res.slope = curSlope;
    res.scaleMAD = curScaleMAD;

    const double nanSE = std::numeric_limits<double>::quiet_NaN();
    // The core returns "usable" (a finite line estimate exists); whether the
    // iteration actually converged is reported separately in res.converged.
    const bool usable = std::isfinite(curIntercept) && std::isfinite(curSlope);

    if (method == PiecewiseAndRobustFit::QuantileReg) {
        res.interceptSE = nanSE;
        res.slopeSE = nanSE;
        // Data-scale-relative "tiny" thresholds (no absolute 1e-14).
        const double rTiny = std::max(1e-13 * yScale, std::numeric_limits<double>::min());
        if (n > 2) {
            double meanX = 0.0;
            for (double xi : xv)
                meanX += xi;
            meanX /= double(n);
            double sxx = 0.0;
            for (double xi : xv) {
                const double dx = xi - meanX;
                sxx += dx * dx;
            }

            double meanR = 0.0;
            for (double ri : resid)
                meanR += ri;
            meanR /= double(n);
            double srr = 0.0;
            for (double ri : resid) {
                const double dr = ri - meanR;
                srr += dr * dr;
            }
            const double sdR = std::sqrt(srr / double(n - 2));
            const double q25 = computeVectorQuantile(std::vector<double>(resid), 0.25);
            const double q75 = computeVectorQuantile(std::vector<double>(resid), 0.75);
            const double iqrScale = (q75 - q25) / 1.3489795003921634;

            double sigmaHat = 0.0;
            if (sdR > rTiny && iqrScale > rTiny)
                sigmaHat = std::min(sdR, iqrScale);
            else if (curScaleMAD > rTiny)
                sigmaHat = curScaleMAD;
            else
                sigmaHat = std::max(sdR, iqrScale);

            if (!(sigmaHat > rTiny)) {
                // Residuals are (numerically) zero: exact fit, SE = 0.
                res.interceptSE = 0.0;
                res.slopeSE = 0.0;
            }
            if (sigmaHat > rTiny && sxx > 0.0 && std::isfinite(sxx)) {
                // Hall-Sheather / Bofinger bandwidth + Powell Gaussian kernel sparsity estimator
                constexpr double kInvSqrt2Pi = 0.39894228040143267794;
                const double zTau = gsl_cdf_ugaussian_Pinv(tau);
                const double phiTau = kInvSqrt2Pi * std::exp(-0.5 * zTau * zTau);
                const double zAlpha = 1.959963984540054; // Phi^{-1}(0.975)
                const double hsTerm = (1.5 * phiTau * phiTau) / (2.0 * zTau * zTau + 1.0);
                const double h_hs = std::pow(double(n), -1.0 / 3.0)
                        * std::pow(zAlpha, 2.0 / 3.0)
                        * std::pow(std::max(1e-12, hsTerm), 1.0 / 3.0);
                const double bofTerm =
                        (4.5 * std::pow(phiTau, 4)) / std::pow(2.0 * zTau * zTau + 1.0, 2);
                const double h_bof = std::pow(double(n), -0.2)
                        * std::pow(std::max(1e-16, bofTerm), 0.2);
                const double maxH = std::max(0.004, std::min(0.45, 0.9 * std::min(tau, 1.0 - tau)));
                const double minH = std::min(0.01, maxH);
                const double hProb = std::clamp(0.5 * (h_hs + h_bof), minH, maxH);
                const double pLo = std::clamp(tau - hProb, 0.002, 0.996);
                const double pHi = std::clamp(tau + hProb, std::min(0.997, pLo + 0.002), 0.998);
                const double qSpan =
                        std::max(0.05, gsl_cdf_ugaussian_Pinv(pHi) - gsl_cdf_ugaussian_Pinv(pLo));
                const double bwFloor = 0.35 * sigmaHat * std::pow(double(n), -0.2);
                const double bw = std::max({ 0.5 * sigmaHat * qSpan, bwFloor, rTiny });

                double kSum = 0.0;
                for (double ri : resid) {
                    const double u = ri / bw;
                    kSum += kInvSqrt2Pi * std::exp(-0.5 * u * u);
                }
                const double fHat0 = std::max(kSum / (double(n) * bw), 1e-12 / sigmaHat);
                const double omega2 = (tau * (1.0 - tau)) / (fHat0 * fHat0);
                const double invXtX_00 = (1.0 / double(n)) + (meanX * meanX) / sxx;
                const double invXtX_11 = 1.0 / sxx;
                res.interceptSE = std::sqrt(std::max(0.0, omega2 * invXtX_00));
                res.slopeSE = std::sqrt(std::max(0.0, omega2 * invXtX_11));
            }
        }
        return usable;
    }

    // gsl_fit_wlinear() returns the UNSCALED (X^T W X)^-1, so the robust
    // residual variance chisq_w / (nEff - 2) is applied exactly once here.
    int nEff = 0;
    for (double wi : weights) {
        if (wi > 1e-10)
            ++nEff;
    }
    if (degenerateWeights || !haveCov || nEff <= 2 || !std::isfinite(lastChisq)) {
        // All weights vanished (Tukey), or no residual degrees of freedom:
        // the standard errors are undefined, never "0".
        res.interceptSE = nanSE;
        res.slopeSE = nanSE;
        return usable;
    }
    const double dof = double(nEff - 2);
    const double varScale = std::max(0.0, lastChisq) / dof;

    res.interceptSE = (lastCov00 >= 0.0 && std::isfinite(lastCov00)) ? std::sqrt(varScale * lastCov00) : nanSE;
    res.slopeSE = (lastCov11 >= 0.0 && std::isfinite(lastCov11)) ? std::sqrt(varScale * lastCov11) : nanSE;
    return usable;
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

    double bestRss = 0.0;
    std::vector<double> gx, gy;
    if (!computePiecewiseCore(xv, yv, S, res, bestRss, gx, gy))
        return res;

    res.fittedCurve = addFittedLineToGraph(app, g, QStringLiteral("PiecewiseFit"), gx, gy,
                                           QColor(215, 25, 28));

    if (annotateGraph) {
        QStringList lines;
        lines << QObject::tr("Piecewise Linear Fit (%1 segments)").arg(S);
        lines << QObject::tr("Dataset: %1").arg(resolvedName);
        for (int j = 0; j < S - 1; ++j) {
            const double bpSE = (j < res.breakpointsXSE.size()) ? res.breakpointsXSE[j] : 0.0;
            lines << QObject::tr("Breakpoint %1: X = %2 ± %3, Y = %4")
                             .arg(j + 1)
                             .arg(res.breakpointsX[j], 0, 'g', 6)
                             .arg(bpSE, 0, 'g', 4)
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
        reportRows << qMakePair(QStringLiteral("Breakpoint_X_SE_%1").arg(j + 1),
                                (j < res.breakpointsXSE.size()) ? res.breakpointsXSE[j] : 0.0);
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
        tbl->setHeader(QStringList() << QStringLiteral("Parameter") << QStringLiteral("Value"));
        tbl->column(0)->setLongName(QObject::tr("Parameter"));
        tbl->column(1)->setLongName(QObject::tr("Value"));
        tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        tbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        for (int r = 0; r < reportRows.size(); ++r) {
            tbl->column(0)->setTextAt(r, reportRows[r].first);
            if (std::isfinite(reportRows[r].second))
                tbl->column(1)->setValueAt(r, reportRows[r].second);
            else
                tbl->column(1)->setInvalid(r, true);
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
    if (!extractUnmaskedSortedXY(curve, xv, yv) || xv.size() < 2)
        return res;

    if (!computeRobustOrQuantileCore(xv, yv, method, quantileTau, res))
        return res;

    const size_t n = xv.size();
    const double xMin = xv.front();
    const double xMax = xv.back();

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
        auto seText = [](double v) {
            return std::isfinite(v) ? QString::number(v, 'g', 4) : QObject::tr("n/a");
        };
        lines << QObject::tr("Slope = %1 ± %2")
                         .arg(res.slope, 0, 'g', 6)
                         .arg(seText(res.slopeSE));
        lines << QObject::tr("Intercept = %1 ± %2")
                         .arg(res.intercept, 0, 'g', 6)
                         .arg(seText(res.interceptSE));
        lines << QObject::tr("MAD Scale = %1").arg(res.scaleMAD, 0, 'g', 5);
        if (!res.converged)
            lines << QObject::tr("Warning: iteration did not converge (SE may be unreliable)");
        insertArialSummaryBox(g, lines.join('\n'));
        g->replot();
    }

    // Create Report Worksheet
    const QString tblName = app->generateUniqueName(methodTag + QStringLiteral("_Report"), false);
    Table *tbl = app->newTable(6, 3, tblName, QObject::tr("Robust / Quantile Regression Report"));
    if (tbl) {
        tbl->setHeader(QStringList() << QStringLiteral("Parameter") << QStringLiteral("Estimate")
                                     << QStringLiteral("Std_Error"));
        tbl->column(0)->setLongName(QObject::tr("Parameter"));
        tbl->column(1)->setLongName(QObject::tr("Estimate"));
        tbl->column(2)->setLongName(QObject::tr("Std_Error"));
        tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        tbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        tbl->column(2)->setColumnMode(SciDAVis::ColumnMode::Numeric);

        auto setNumOrInvalid = [&](int row, int col, double val) {
            if (std::isfinite(val))
                tbl->column(col)->setValueAt(row, val);
            else
                tbl->column(col)->setInvalid(row, true);
        };

        tbl->column(0)->setTextAt(0, QStringLiteral("Intercept"));
        setNumOrInvalid(0, 1, res.intercept);
        setNumOrInvalid(0, 2, res.interceptSE);

        tbl->column(0)->setTextAt(1, QStringLiteral("Slope"));
        setNumOrInvalid(1, 1, res.slope);
        setNumOrInvalid(1, 2, res.slopeSE);

        tbl->column(0)->setTextAt(2, QStringLiteral("MAD_Scale"));
        setNumOrInvalid(2, 1, res.scaleMAD);
        tbl->column(2)->setValueAt(2, 0.0);

        tbl->column(0)->setTextAt(3, QStringLiteral("N_Points"));
        tbl->column(1)->setValueAt(3, double(n));
        tbl->column(2)->setValueAt(3, 0.0);

        tbl->column(0)->setTextAt(4, QStringLiteral("Quantile_Tau"));
        tbl->column(1)->setValueAt(4, (method == QuantileReg) ? std::clamp(quantileTau, 0.01, 0.99)
                                                              : 0.5);
        tbl->column(2)->setValueAt(4, 0.0);

        tbl->column(0)->setTextAt(5, QStringLiteral("Converged"));
        tbl->column(1)->setValueAt(5, res.converged ? 1.0 : 0.0);
        tbl->column(2)->setValueAt(5, 0.0);

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
    double tssEffective = 0.0;
    for (int m = 0; m < K; ++m) {
        SilentNonLinearFit sepFitter(app, g);
        if (!configureModelOnFitter(sepFitter, modelName))
            return res;
        const QVector<bool> noneShared(p, false);
        if (!sepFitter.fitGlobal(QStringList() << validCurves[m], noneShared, nullptr))
            return res;
        rssSeparate += std::max(0.0, sepFitter.chiSquare());
        tssEffective += sepFitter.weightedOrUnweightedTss();
    }

    // Also evaluate joint unshared global fit in case shared optimizer landed in a slightly worse
    // local basin on identical datasets; shared minimum is >= separate minimum mathematically
    if (rssShared < rssSeparate)
        rssShared = rssSeparate;

    const int nEff = (globalFitter.activePointCount() > 0)
            ? globalFitter.activePointCount()
            : nTotal;
    const int dfShared = nEff - p;
    const int dfSeparate = nEff - K * p;
    const int dfDiff = dfShared - dfSeparate; // (K - 1) * p > 0
    if (dfSeparate <= 0 || dfDiff <= 0)
        return res;

    const double baseTss = (tssEffective > 0.0) ? tssEffective : tssTotal;
    const double objScale = std::max({ baseTss, rssShared, std::numeric_limits<double>::min() });
    const double noiselessFloor = 1e-18 * objScale;
    double fStat = 0.0;
    double pVal = 1.0;

    if (rssShared <= noiselessFloor && rssSeparate <= noiselessFloor) {
        fStat = 0.0;
        pVal = 1.0;
    } else if (rssSeparate <= 1e-26 * std::max(rssShared, objScale)) {
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
        if (nEff <= kParams + 2)
            return std::numeric_limits<double>::quiet_NaN();
        const double effRss = std::max(rssVal, 1e-24 * objScale);
        const double k = double(kParams + 1);
        const double aic = double(nEff) * std::log(effRss / double(nEff)) + 2.0 * k;
        return aic + (2.0 * k * (k + 1.0)) / double(nEff - kParams - 2);
    };

    const double aiccShared = computeAICc(rssShared, p);
    const double aiccSeparate = computeAICc(rssSeparate, K * p);
    const double deltaAICc = (std::isfinite(aiccShared) && std::isfinite(aiccSeparate))
            ? (aiccShared - aiccSeparate)
            : std::numeric_limits<double>::quiet_NaN();

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
        tbl->setHeader(QStringList() << QStringLiteral("Hypothesis_Model") << QStringLiteral("RSS")
                                     << QStringLiteral("DF") << QStringLiteral("AICc")
                                     << QStringLiteral("F_Statistic") << QStringLiteral("p_Value")
                                     << QStringLiteral("Conclusion"));
        const QStringList cmpLongNames = {
            QObject::tr("Hypothesis_Model"), QObject::tr("RSS"),
            QObject::tr("DF"),               QObject::tr("AICc"),
            QObject::tr("F_Statistic"),      QObject::tr("p_Value"),
            QObject::tr("Conclusion")
        };
        tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 1; c <= 5; ++c)
            tbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        tbl->column(6)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 0; c < 7; ++c)
            tbl->column(c)->setLongName(cmpLongNames[c]);

        const QString conclusion = res.preferSeparate
                ? QObject::tr("Reject Shared (Different parameters across datasets)")
                : QObject::tr("Do Not Reject Shared (Global shared model adequate)");

        tbl->column(0)->setTextAt(0, QStringLiteral("Global_Shared (%1)").arg(modelName));
        tbl->column(1)->setValueAt(0, rssShared);
        tbl->column(2)->setValueAt(0, double(dfShared));
        if (std::isfinite(aiccShared))
            tbl->column(3)->setValueAt(0, aiccShared);
        else
            tbl->column(3)->setInvalid(0, true);
        tbl->column(4)->setValueAt(0, fStat);
        tbl->column(5)->setValueAt(0, pVal);
        tbl->column(6)->setTextAt(0, conclusion);

        tbl->column(0)->setTextAt(1, QStringLiteral("Separate_Per_Dataset (%1)").arg(modelName));
        tbl->column(1)->setValueAt(1, rssSeparate);
        tbl->column(2)->setValueAt(1, double(dfSeparate));
        if (std::isfinite(aiccSeparate))
            tbl->column(3)->setValueAt(1, aiccSeparate);
        else
            tbl->column(3)->setInvalid(1, true);
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

    double yAbsMax = 0.0;
    for (double yi : yObs)
        yAbsMax = std::max(yAbsMax, std::fabs(yi));
    const double yScale = std::max(yAbsMax, std::numeric_limits<double>::min());
    const double yScale2 = yScale * yScale;
    const double tssRef = std::max(tss, yScale2);
    const double tssTiny = 1e-28 * yScale2;

    const double nan = std::numeric_limits<double>::quiet_NaN();
    res.rSquare = (tss > tssTiny) ? std::clamp(1.0 - rss / tss, 0.0, 1.0)
                                  : ((rss <= 1e-24 * tssRef) ? 1.0 : 0.0);
    res.adjRSquare = (n > p)
            ? ((tss > tssTiny) ? (1.0 - (1.0 - res.rSquare) * double(n - 1) / double(n - p))
                               : res.rSquare)
            : nan;

    const int dfErr = std::max(1, n - p);
    const bool noiseless = (rss <= 1e-22 * tssRef);

    res.stdErrors.resize(p);
    res.tValues.resize(p);
    res.pValues.resize(p);
    // NOTE: gsl_multifit_linear() (unweighted) already returns
    //   cov = s^2 (X^T X)^-1,  s^2 = chisq / (n - rank)
    // (verified against the bundled GSL 2.8: cov11 == chisq/(n-p)/Sxx for a
    // straight line).  Multiplying by rss/dfErr again would double-scale the
    // SE, so the diagonal is used as-is.  Only gsl_multifit_wlinear() returns
    // the unscaled (X^T W X)^-1.
    for (int j = 0; j < p; ++j) {
        if (n <= p) {
            // Residual degrees of freedom are zero: SE / t / p are undefined.
            res.stdErrors[j] = nan;
            res.tValues[j] = nan;
            res.pValues[j] = nan;
            continue;
        }
        double xScaleJ = 1.0;
        if (j > 0) {
            double xAbsMax = 0.0;
            for (int i = 0; i < n; ++i)
                xAbsMax = std::max(xAbsMax, std::fabs(xObs[size_t(i)][size_t(j - 1)]));
            xScaleJ = std::max(xAbsMax, std::numeric_limits<double>::min());
        }
        const double betaScale = (j == 0) ? yScale : (yScale / xScaleJ);

        const double cjj = gsl_matrix_get(cov, size_t(j), size_t(j));
        const double se = noiseless ? 0.0
                                    : ((std::isfinite(cjj) && cjj >= 0.0) ? std::sqrt(cjj) : nan);
        res.stdErrors[j] = se;

        if (!std::isfinite(se)) {
            res.tValues[j] = nan;
            res.pValues[j] = nan;
        } else if (se > 1e-30 * betaScale) {
            const double tv = res.betas[j] / se;
            res.tValues[j] = tv;
            res.pValues[j] = (n > p)
                    ? std::clamp(2.0 * gsl_cdf_tdist_Q(std::fabs(tv), double(dfErr)), 0.0, 1.0)
                    : 1.0;
        } else if (std::fabs(res.betas[j]) > 1e-12 * betaScale) {
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
    if (n <= p) {
        res.fStatistic = nan;
        res.fPValue = nan;
    } else if (noiseless && mss > 1e-24 * tssRef) {
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
            double xAbsMaxM = 0.0;
            for (int i = 0; i < n; ++i) {
                const double xmVal = xObs[size_t(i)][size_t(m)];
                meanXm += xmVal;
                xAbsMaxM = std::max(xAbsMaxM, std::fabs(xmVal));
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
            const double xScaleM = std::max(xAbsMaxM, std::numeric_limits<double>::min());

            double tssM = 0.0;
            for (int i = 0; i < n; ++i) {
                const double dxm = xObs[size_t(i)][size_t(m)] - meanXm;
                tssM += dxm * dxm;
            }

            double chisqSub = 0.0;
            if (tssM <= 1e-28 * xScaleM * xScaleM
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
        tbl->setHeader(QStringList() << QStringLiteral("Term") << QStringLiteral("Beta")
                                     << QStringLiteral("Std_Error") << QStringLiteral("t_Value")
                                     << QStringLiteral("p_Value") << QStringLiteral("VIF")
                                     << QStringLiteral("Model_Metric")
                                     << QStringLiteral("Metric_Value"));
        const QStringList mlrLongNames = {
            QObject::tr("Term"),         QObject::tr("Beta"),
            QObject::tr("Std_Error"),    QObject::tr("t_Value"),
            QObject::tr("p_Value"),      QObject::tr("VIF"),
            QObject::tr("Model_Metric"), QObject::tr("Metric_Value")
        };
        tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 1; c <= 5; ++c)
            tbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        tbl->column(6)->setColumnMode(SciDAVis::ColumnMode::Text);
        tbl->column(7)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        for (int c = 0; c < 8; ++c)
            tbl->column(c)->setLongName(mlrLongNames[c]);

        auto setNumOrInvalid = [&](int row, int col, double val) {
            if (std::isfinite(val))
                tbl->column(col)->setValueAt(row, val);
            else
                tbl->column(col)->setInvalid(row, true);
        };

        for (int r = 0; r < reportRows; ++r) {
            if (r < p) {
                tbl->column(0)->setTextAt(r, (r == 0) ? QStringLiteral("(Intercept)")
                                                      : xLabels[r - 1]);
                setNumOrInvalid(r, 1, res.betas[r]);
                setNumOrInvalid(r, 2, res.stdErrors[r]);
                setNumOrInvalid(r, 3, res.tValues[r]);
                setNumOrInvalid(r, 4, res.pValues[r]);
                if (r == 0)
                    tbl->column(5)->setInvalid(r, true);
                else
                    setNumOrInvalid(r, 5, res.vif[r - 1]);
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
                setNumOrInvalid(r, 7, modelSummary[r].second);
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
            diagTbl->setHeader(QStringList() << QStringLiteral("Predicted_Y")
                                             << QStringLiteral("Actual_Y")
                                             << QStringLiteral("Residual"));
            diagTbl->column(0)->setLongName(QObject::tr("Predicted_Y"));
            diagTbl->column(1)->setLongName(QObject::tr("Actual_Y"));
            diagTbl->column(2)->setLongName(QObject::tr("Residual"));
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
// 4b. Binary Logistic Regression (IRLS / Newton-Raphson MLE + L2 Safeguard)
// ============================================================================

PiecewiseAndRobustFit::LogisticResult
PiecewiseAndRobustFit::runLogisticRegression(ApplicationWindow *app, Table *srcTable,
                                             const QString &yBinaryCol,
                                             const QStringList &xColNames,
                                             bool plotProbCurve)
{
    LogisticResult res;
    if (!app || !srcTable || yBinaryCol.trimmed().isEmpty() || xColNames.isEmpty())
        return res;

    const int yCol = resolveTableColumnIndex(srcTable, yBinaryCol);
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
    const int p = k + 1;

    auto readNumericCell = [&](int r, int c, double &valOut) -> bool {
        Column *col = srcTable->column(c);
        if (!col || col->isInvalid(r) || col->isMasked(r))
            return false;
        const QString rawTxt = srcTable->text(r, c).trimmed();
        if (rawTxt.isEmpty())
            return false;
        if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
            const double v = col->valueAt(r);
            if (!std::isfinite(v))
                return false;
            valOut = v;
            return true;
        }
        bool okNum = false;
        double v = QLocale().toDouble(rawTxt, &okNum);
        if (!okNum)
            v = rawTxt.toDouble(&okNum);
        if (!okNum || !std::isfinite(v))
            return false;
        valOut = v;
        return true;
    };

    Column *yColumnPtr = srcTable->column(yCol);
    if (!yColumnPtr)
        return res;

    struct RawLogRow
    {
        bool yIsNum = false;
        double yNum = 0.0;
        QString yStr;
        std::vector<double> xRow;
    };
    std::vector<RawLogRow> rawRows;
    const int totalRows = srcTable->numRows();
    rawRows.reserve(size_t(std::max(0, totalRows)));

    for (int r = 0; r < totalRows; ++r) {
        if (yColumnPtr->isInvalid(r) || yColumnPtr->isMasked(r))
            continue;
        const QString yTxt = srcTable->text(r, yCol).trimmed();
        if (yTxt.isEmpty())
            continue;

        std::vector<double> xr(size_t(k), 0.0);
        bool okX = true;
        for (int j = 0; j < k; ++j) {
            if (!readNumericCell(r, xCols[j], xr[size_t(j)])) {
                okX = false;
                break;
            }
        }
        if (!okX)
            continue;

        RawLogRow rr;
        rr.xRow = std::move(xr);
        rr.yStr = yTxt;
        double yNumVal = 0.0;
        if (yColumnPtr->columnMode() == SciDAVis::ColumnMode::Numeric
            && readNumericCell(r, yCol, yNumVal)) {
            rr.yIsNum = true;
            rr.yNum = yNumVal;
        } else {
            rr.yIsNum = false;
        }
        rawRows.push_back(std::move(rr));
    }

    if (static_cast<int>(rawRows.size()) < p + 1)
        return res;

    bool allNumericY = (yColumnPtr->columnMode() == SciDAVis::ColumnMode::Numeric);
    for (const auto &rr : rawRows) {
        if (!rr.yIsNum) {
            allNumericY = false;
            break;
        }
    }

    std::vector<int> yBin(rawRows.size(), 0);
    if (allNumericY) {
        bool strictZeroOne = true;
        std::vector<double> distinctVals;
        for (const auto &rr : rawRows) {
            if (std::fabs(rr.yNum) > 1e-9 && std::fabs(rr.yNum - 1.0) > 1e-9)
                strictZeroOne = false;
            bool seen = false;
            for (double dv : distinctVals) {
                if (std::fabs(dv - rr.yNum) <= 1e-9) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                distinctVals.push_back(rr.yNum);
                if (distinctVals.size() > 2 && !strictZeroOne)
                    return res;
            }
        }
        if (strictZeroOne) {
            for (size_t i = 0; i < rawRows.size(); ++i)
                yBin[i] = (std::fabs(rawRows[i].yNum - 1.0) <= 1e-9) ? 1 : 0;
        } else {
            if (distinctVals.size() != 2)
                return res;
            std::sort(distinctVals.begin(), distinctVals.end());
            const double hiVal = distinctVals[1];
            for (size_t i = 0; i < rawRows.size(); ++i)
                yBin[i] = (std::fabs(rawRows[i].yNum - hiVal) <= 1e-9) ? 1 : 0;
        }
    } else {
        auto isPos = [](const QString &low) -> bool {
            return low == QStringLiteral("1") || low == QStringLiteral("true")
                    || low == QStringLiteral("yes") || low == QStringLiteral("pos")
                    || low == QStringLiteral("positive") || low == QStringLiteral("case")
                    || low == QStringLiteral("event") || low == QStringLiteral("disease")
                    || low == QStringLiteral("treated") || low == QStringLiteral("pass")
                    || low == QStringLiteral("\u967d\u6027");
        };
        auto isNeg = [](const QString &low) -> bool {
            return low == QStringLiteral("0") || low == QStringLiteral("false")
                    || low == QStringLiteral("no") || low == QStringLiteral("none")
                    || low == QStringLiteral("neg") || low == QStringLiteral("negative")
                    || low == QStringLiteral("control") || low == QStringLiteral("healthy")
                    || low == QStringLiteral("normal") || low == QStringLiteral("\u9670\u6027");
        };
        QStringList levels;
        for (const auto &rr : rawRows) {
            if (!levels.contains(rr.yStr))
                levels.append(rr.yStr);
        }
        if (levels.size() == 2) {
            const QString &L0 = levels[0];
            const QString &L1 = levels[1];
            const QString low0 = L0.toLower();
            const QString low1 = L1.toLower();
            const bool p0 = isPos(low0), p1 = isPos(low1);
            const bool n0 = isNeg(low0), n1 = isNeg(low1);
            const QString posLevel = ((p0 && !p1) || (n1 && !n0)) ? L0 : L1;
            for (size_t i = 0; i < rawRows.size(); ++i)
                yBin[i] = (rawRows[i].yStr == posLevel) ? 1 : 0;
        } else if (levels.size() > 2) {
            for (const auto &rr : rawRows) {
                const QString low = rr.yStr.toLower();
                if (isPos(low) == isNeg(low))
                    return res;
            }
            for (size_t i = 0; i < rawRows.size(); ++i)
                yBin[i] = isPos(rawRows[i].yStr.toLower()) ? 1 : 0;
        } else {
            return res;
        }
    }

    const int n = static_cast<int>(rawRows.size());
    int n1 = 0;
    for (int yi : yBin)
        n1 += yi;
    const int n0 = n - n1;
    if (n0 < 1 || n1 < 1 || n < p + 1)
        return res;

    // Standardize predictors X_1..X_k for well-conditioned Newton-Raphson / IRLS
    std::vector<double> xMean(size_t(k), 0.0);
    std::vector<double> xScale(size_t(k), 1.0);
    for (int j = 0; j < k; ++j) {
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
            sum += rawRows[size_t(i)].xRow[size_t(j)];
        xMean[size_t(j)] = sum / double(n);

        double sq = 0.0;
        for (int i = 0; i < n; ++i) {
            const double d = rawRows[size_t(i)].xRow[size_t(j)] - xMean[size_t(j)];
            sq += d * d;
        }
        const double sd = std::sqrt(sq / double(n));
        xScale[size_t(j)] = (std::isfinite(sd) && sd > 1e-15) ? sd : 1.0;
    }

    std::vector<std::vector<double>> Z(size_t(n), std::vector<double>(size_t(p), 1.0));
    for (int i = 0; i < n; ++i) {
        Z[size_t(i)][0] = 1.0;
        for (int j = 0; j < k; ++j) {
            Z[size_t(i)][size_t(j + 1)] =
                    (rawRows[size_t(i)].xRow[size_t(j)] - xMean[size_t(j)]) / xScale[size_t(j)];
        }
    }

    auto sigmoidStable = [](double eta) -> double {
        if (eta >= 0.0) {
            const double ez = std::exp(-eta);
            return 1.0 / (1.0 + ez);
        }
        const double ez = std::exp(eta);
        return ez / (1.0 + ez);
    };

    const double ridgeLambda = 1e-9; // Tiny L2 safeguard against quasi-complete separation

    auto evalLogLik = [&](const std::vector<double> &gamma, bool includePenalty) -> double {
        double ll = 0.0;
        for (int i = 0; i < n; ++i) {
            double eta = 0.0;
            for (int m = 0; m < p; ++m)
                eta += Z[size_t(i)][size_t(m)] * gamma[size_t(m)];
            const double absEta = std::fabs(eta);
            ll += double(yBin[size_t(i)]) * eta - std::max(0.0, eta)
                    - std::log1p(std::exp(-absEta));
        }
        if (includePenalty) {
            double pen = 0.0;
            for (int j = 1; j < p; ++j)
                pen += gamma[size_t(j)] * gamma[size_t(j)];
            ll -= 0.5 * ridgeLambda * pen;
        }
        return ll;
    };

    const double p0 = double(n1) / double(n);
    const double llNull = double(n1) * std::log(p0) + double(n0) * std::log(1.0 - p0);

    std::vector<double> gamma(size_t(p), 0.0);
    gamma[0] = std::log(p0 / (1.0 - p0));

    double curPenLL = evalLogLik(gamma, true);
    bool mleConverged = false;
    const int maxIter = 100;

    for (int iter = 0; iter < maxIter; ++iter) {
        std::vector<double> grad(size_t(p), 0.0);
        std::vector<std::vector<double>> H(size_t(p), std::vector<double>(size_t(p), 0.0));

        for (int i = 0; i < n; ++i) {
            double eta = 0.0;
            for (int m = 0; m < p; ++m)
                eta += Z[size_t(i)][size_t(m)] * gamma[size_t(m)];
            const double pi = sigmoidStable(eta);
            const double diff = double(yBin[size_t(i)]) - pi;
            const double wi = std::max(pi * (1.0 - pi), 1e-14);
            for (int a = 0; a < p; ++a) {
                const double za = Z[size_t(i)][size_t(a)];
                grad[size_t(a)] += diff * za;
                for (int b = a; b < p; ++b)
                    H[size_t(a)][size_t(b)] += wi * za * Z[size_t(i)][size_t(b)];
            }
        }

        H[0][0] += 1e-12;
        for (int j = 1; j < p; ++j) {
            grad[size_t(j)] -= ridgeLambda * gamma[size_t(j)];
            H[size_t(j)][size_t(j)] += ridgeLambda;
        }
        for (int a = 0; a < p; ++a) {
            for (int b = 0; b < a; ++b)
                H[size_t(a)][size_t(b)] = H[size_t(b)][size_t(a)];
        }

        double maxGrad = 0.0;
        for (double gv : grad)
            maxGrad = std::max(maxGrad, std::fabs(gv));
        if (maxGrad < 1e-8) {
            mleConverged = true;
            break;
        }

        std::vector<std::vector<double>> invH;
        std::vector<double> stepVec(size_t(p), 0.0);
        if (invertMatrixGsl(H, invH)) {
            for (int a = 0; a < p; ++a) {
                double s = 0.0;
                for (int b = 0; b < p; ++b)
                    s += invH[size_t(a)][size_t(b)] * grad[size_t(b)];
                stepVec[size_t(a)] = s;
            }
        } else {
            stepVec = grad;
        }

        double dirDeriv = 0.0;
        double maxStep = 0.0;
        for (int m = 0; m < p; ++m) {
            dirDeriv += grad[size_t(m)] * stepVec[size_t(m)];
            maxStep = std::max(maxStep, std::fabs(stepVec[size_t(m)]));
        }
        if (!(dirDeriv > 0.0) || !std::isfinite(maxStep)) {
            stepVec = grad;
            dirDeriv = 0.0;
            maxStep = 0.0;
            for (int m = 0; m < p; ++m) {
                dirDeriv += grad[size_t(m)] * stepVec[size_t(m)];
                maxStep = std::max(maxStep, std::fabs(stepVec[size_t(m)]));
            }
        }
        if (maxStep > 5.0) {
            const double shrink = 5.0 / maxStep;
            for (double &sv : stepVec)
                sv *= shrink;
            dirDeriv *= shrink;
            maxStep = 5.0;
        }

        double alpha = 1.0;
        bool accepted = false;
        std::vector<double> trialGamma(size_t(p), 0.0);
        while (alpha >= 1e-10) {
            for (int m = 0; m < p; ++m)
                trialGamma[size_t(m)] = gamma[size_t(m)] + alpha * stepVec[size_t(m)];
            const double trialLL = evalLogLik(trialGamma, true);
            if (std::isfinite(trialLL)
                && trialLL >= curPenLL + 1e-4 * alpha * dirDeriv - 1e-13 * (1.0 + std::fabs(curPenLL))) {
                gamma = trialGamma;
                curPenLL = trialLL;
                accepted = true;
                break;
            }
            alpha *= 0.5;
        }

        if (!accepted) {
            if (maxGrad < 1e-4)
                mleConverged = true;
            break;
        }

        if (alpha * maxStep < 1e-9) {
            mleConverged = true;
            break;
        }
        // Reaching maxIter is NOT convergence (complete / quasi-complete
        // separation typically ends here); reported via res.converged = false.
    }

    if (!std::isfinite(curPenLL))
        return res;

    // Final Fisher information & covariance in standardized space
    std::vector<std::vector<double>> Hfinal(size_t(p), std::vector<double>(size_t(p), 0.0));
    for (int i = 0; i < n; ++i) {
        double eta = 0.0;
        for (int m = 0; m < p; ++m)
            eta += Z[size_t(i)][size_t(m)] * gamma[size_t(m)];
        const double pi = sigmoidStable(eta);
        const double wi = std::max(pi * (1.0 - pi), 1e-14);
        for (int a = 0; a < p; ++a) {
            for (int b = a; b < p; ++b)
                Hfinal[size_t(a)][size_t(b)] += wi * Z[size_t(i)][size_t(a)] * Z[size_t(i)][size_t(b)];
        }
    }
    Hfinal[0][0] += 1e-12;
    for (int j = 1; j < p; ++j)
        Hfinal[size_t(j)][size_t(j)] += ridgeLambda;
    for (int a = 0; a < p; ++a) {
        for (int b = 0; b < a; ++b)
            Hfinal[size_t(a)][size_t(b)] = Hfinal[size_t(b)][size_t(a)];
    }

    std::vector<std::vector<double>> covGamma(size_t(p), std::vector<double>(size_t(p), 0.0));
    const bool fisherInvertible = invertMatrixGsl(Hfinal, covGamma);

    // Unstandardize coefficients beta = M * gamma and covariance covBeta = M * covGamma * M^T
    std::vector<std::vector<double>> M(size_t(p), std::vector<double>(size_t(p), 0.0));
    M[0][0] = 1.0;
    for (int j = 0; j < k; ++j) {
        M[0][size_t(j + 1)] = -xMean[size_t(j)] / xScale[size_t(j)];
        M[size_t(j + 1)][size_t(j + 1)] = 1.0 / xScale[size_t(j)];
    }

    std::vector<std::vector<double>> covBeta(size_t(p), std::vector<double>(size_t(p), 0.0));
    for (int a = 0; a < p; ++a) {
        for (int b = 0; b < p; ++b) {
            double sum = 0.0;
            for (int u = 0; u < p; ++u) {
                if (M[size_t(a)][size_t(u)] == 0.0)
                    continue;
                for (int v = 0; v < p; ++v) {
                    if (M[size_t(b)][size_t(v)] != 0.0)
                        sum += M[size_t(a)][size_t(u)] * covGamma[size_t(u)][size_t(v)]
                                * M[size_t(b)][size_t(v)];
                }
            }
            covBeta[size_t(a)][size_t(b)] = sum;
        }
    }

    res.betas.resize(p);
    res.stdErrors.resize(p);
    res.waldZ.resize(p);
    res.pValues.resize(p);
    res.oddsRatios.resize(k);

    res.betas[0] = gamma[0];
    for (int j = 0; j < k; ++j) {
        const double bj = gamma[size_t(j + 1)] / xScale[size_t(j)];
        res.betas[j + 1] = bj;
        res.betas[0] -= bj * xMean[size_t(j)];
        res.oddsRatios[j] = std::exp(std::clamp(bj, -500.0, 500.0));
    }

    const double nanSE = std::numeric_limits<double>::quiet_NaN();
    for (int m = 0; m < p; ++m) {
        const double vmm = fisherInvertible ? covBeta[size_t(m)][size_t(m)] : nanSE;
        // Singular Fisher information or a non-positive variance: SE / z / p
        // are undefined (never "SE = 0, p = 1").
        const double se = (std::isfinite(vmm) && vmm > 0.0) ? std::sqrt(vmm) : nanSE;
        res.stdErrors[m] = se;
        if (std::isfinite(se) && se > 0.0) {
            const double zv = res.betas[m] / se;
            res.waldZ[m] = zv;
            res.pValues[m] = std::clamp(2.0 * gsl_cdf_ugaussian_Q(std::fabs(zv)), 0.0, 1.0);
        } else {
            res.waldZ[m] = nanSE;
            res.pValues[m] = nanSE;
        }
    }

    res.logLikelihood = evalLogLik(gamma, false);
    res.lrChi2 = std::max(0.0, 2.0 * (res.logLikelihood - llNull));
    res.lrPValue = std::clamp(gsl_cdf_chisq_Q(res.lrChi2, double(k)), 0.0, 1.0);
    res.mcFaddenR2 = (llNull < -1e-15) ? std::clamp(1.0 - (res.logLikelihood / llNull), 0.0, 1.0)
                                       : 0.0;
    res.aic = -2.0 * res.logLikelihood + 2.0 * double(p);
    res.converged = mleConverged && fisherInvertible;

    // Create Report Worksheet
    const int nSummaryMetrics = 8;
    const int reportRows = std::max(p, nSummaryMetrics);
    const QString tblName =
            app->generateUniqueName(srcTable->name() + QStringLiteral("_Logistic"), false);
    Table *tbl = app->newTable(reportRows, 10, tblName,
                               QObject::tr("Binary Logistic Regression Summary"));
    if (tbl) {
        tbl->setHeader(QStringList()
                       << QStringLiteral("Term") << QStringLiteral("Beta")
                       << QStringLiteral("Std_Error") << QStringLiteral("Wald_Z")
                       << QStringLiteral("p_Value") << QStringLiteral("Odds_Ratio")
                       << QStringLiteral("OR_95CI_Low") << QStringLiteral("OR_95CI_High")
                       << QStringLiteral("Model_Metric") << QStringLiteral("Metric_Value"));
        const QStringList logLongNames = {
            QObject::tr("Term"),         QObject::tr("Beta"),        QObject::tr("Std_Error"),
            QObject::tr("Wald_Z"),       QObject::tr("p_Value"),     QObject::tr("Odds_Ratio"),
            QObject::tr("OR_95CI_Low"),  QObject::tr("OR_95CI_High"), QObject::tr("Model_Metric"),
            QObject::tr("Metric_Value")
        };
        tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 1; c <= 7; ++c)
            tbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        tbl->column(8)->setColumnMode(SciDAVis::ColumnMode::Text);
        tbl->column(9)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        for (int c = 0; c < 10; ++c)
            tbl->column(c)->setLongName(logLongNames[c]);

        auto setNumOrInvalid = [&](int row, int col, double val) {
            if (std::isfinite(val))
                tbl->column(col)->setValueAt(row, val);
            else
                tbl->column(col)->setInvalid(row, true);
        };

        const double z95 = 1.959963984540054;
        for (int r = 0; r < reportRows; ++r) {
            if (r < p) {
                tbl->column(0)->setTextAt(r, (r == 0) ? QStringLiteral("(Intercept)")
                                                      : xLabels[r - 1]);
                setNumOrInvalid(r, 1, res.betas[r]);
                setNumOrInvalid(r, 2, res.stdErrors[r]);
                setNumOrInvalid(r, 3, res.waldZ[r]);
                setNumOrInvalid(r, 4, res.pValues[r]);
                if (r == 0) {
                    tbl->column(5)->setInvalid(r, true);
                    tbl->column(6)->setInvalid(r, true);
                    tbl->column(7)->setInvalid(r, true);
                } else {
                    const double b = res.betas[r];
                    const double se = res.stdErrors[r];
                    setNumOrInvalid(r, 5, res.oddsRatios[r - 1]);
                    if (fisherInvertible && std::isfinite(b) && std::isfinite(se)) {
                        tbl->column(6)->setValueAt(
                                r, std::exp(std::clamp(b - z95 * se, -500.0, 500.0)));
                        tbl->column(7)->setValueAt(
                                r, std::exp(std::clamp(b + z95 * se, -500.0, 500.0)));
                    } else {
                        tbl->column(6)->setInvalid(r, true);
                        tbl->column(7)->setInvalid(r, true);
                    }
                }
            } else {
                for (int c = 1; c <= 7; ++c)
                    tbl->column(c)->setInvalid(r, true);
            }
        }

        const QVector<QPair<QString, double>> modelMetrics = {
            { QStringLiteral("Log_Likelihood"), res.logLikelihood },
            { QStringLiteral("Null_Log_Likelihood"), llNull },
            { QStringLiteral("LR_Chi2"), res.lrChi2 },
            { QStringLiteral("LR_p_Value"), res.lrPValue },
            { QStringLiteral("McFadden_R2"), res.mcFaddenR2 },
            { QStringLiteral("AIC"), res.aic },
            { QStringLiteral("N_Observations"), double(n) },
            { QStringLiteral("Converged"), res.converged ? 1.0 : 0.0 }
        };
        for (int r = 0; r < reportRows; ++r) {
            if (r < modelMetrics.size()) {
                tbl->column(8)->setTextAt(r, modelMetrics[r].first);
                setNumOrInvalid(r, 9, modelMetrics[r].second);
            } else {
                tbl->column(9)->setInvalid(r, true);
            }
        }
        res.reportTable = tbl;
    }

    // Optional Logistic Probability + 95% CI Curve MultiLayer (explicit "Arial" fonts)
    if (plotProbCurve) {
        std::vector<double> axisCoord(size_t(n), 0.0);
        double normGammaSlopeSq = 0.0;
        for (int j = 1; j < p; ++j)
            normGammaSlopeSq += gamma[size_t(j)] * gamma[size_t(j)];

        for (int i = 0; i < n; ++i) {
            if (k == 1) {
                axisCoord[size_t(i)] = rawRows[size_t(i)].xRow[0];
            } else {
                double eta = res.betas[0];
                for (int j = 0; j < k; ++j)
                    eta += res.betas[j + 1] * rawRows[size_t(i)].xRow[size_t(j)];
                axisCoord[size_t(i)] = eta;
            }
        }

        double tMin = *std::min_element(axisCoord.begin(), axisCoord.end());
        double tMax = *std::max_element(axisCoord.begin(), axisCoord.end());
        if (!(tMax > tMin + 1e-12)) {
            tMin -= 1.0;
            tMax += 1.0;
        } else {
            const double pad = 0.04 * (tMax - tMin);
            tMin -= pad;
            tMax += pad;
        }

        const QString dataTblName =
                app->generateUniqueName(srcTable->name() + QStringLiteral("_Logistic_Plot"), false);
        Table *plotTbl = app->newTable(n, 3, dataTblName,
                                       QObject::tr("Logistic Regression Probability Data"));
        if (plotTbl) {
            plotTbl->setHeader(QStringList()
                               << ((k == 1) ? xLabels[0] : QStringLiteral("Linear_Score_z"))
                               << QStringLiteral("Observed_Y")
                               << QStringLiteral("Predicted_Prob"));
            plotTbl->column(0)->setLongName((k == 1) ? xLabels[0] : QObject::tr("Linear_Score_z"));
            plotTbl->column(1)->setLongName(QObject::tr("Observed_Y"));
            plotTbl->column(2)->setLongName(QObject::tr("Predicted_Prob"));
            for (int c = 0; c < 3; ++c)
                plotTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            plotTbl->column(0)->setPlotDesignation(SciDAVis::X);
            plotTbl->column(1)->setPlotDesignation(SciDAVis::Y);
            plotTbl->column(2)->setPlotDesignation(SciDAVis::Y);

            for (int i = 0; i < n; ++i) {
                double eta = res.betas[0];
                for (int j = 0; j < k; ++j)
                    eta += res.betas[j + 1] * rawRows[size_t(i)].xRow[size_t(j)];
                plotTbl->column(0)->setValueAt(i, axisCoord[size_t(i)]);
                plotTbl->column(1)->setValueAt(i, double(yBin[size_t(i)]));
                plotTbl->column(2)->setValueAt(i, sigmoidStable(eta));
            }

            MultiLayer *ml = app->newGraph(
                    app->generateUniqueName(QStringLiteral("Logistic_Probability"), false));
            if (ml) {
                Graph *gp = ml->activeGraph();
                const QFont titleFnt("Arial", 11, QFont::Bold);
                const QFont axisFnt("Arial", 10, QFont::Bold);
                const QFont numFnt("Arial", 9);
                const QFont legFnt("Arial", 9);
                ml->setFonts(titleFnt, axisFnt, numFnt, legFnt);

                if (gp) {
                    if (gp->insertCurve(plotTbl, plotTbl->colName(0), plotTbl->colName(1),
                                        Graph::Scatter)) {
                        const int cIdx = gp->curves() - 1;
                        QwtSymbol sym(QwtSymbol::Ellipse, QBrush(QColor(31, 119, 180, 175)),
                                      QPen(QColor(20, 80, 140), 1), QSize(7, 7));
                        gp->setCurveSymbol(cIdx, sym);
                    }

                    const int nGrid = 201;
                    std::vector<double> gx(static_cast<size_t>(nGrid)),
                            gyProb(static_cast<size_t>(nGrid)),
                            gyLo(static_cast<size_t>(nGrid)),
                            gyHi(static_cast<size_t>(nGrid));
                    const double z95 = 1.959963984540054;
                    std::vector<double> vStd(size_t(p), 0.0);
                    bool plotCI = fisherInvertible;

                    for (int gi = 0; gi < nGrid; ++gi) {
                        const double tg = tMin + (tMax - tMin) * double(gi) / double(nGrid - 1);
                        gx[size_t(gi)] = tg;

                        double etaHat = 0.0;
                        double varEta = 0.0;
                        if (k == 1) {
                            etaHat = res.betas[0] + res.betas[1] * tg;
                            if (plotCI) {
                                varEta = covBeta[0][0] + 2.0 * tg * covBeta[0][1]
                                        + tg * tg * covBeta[1][1];
                            }
                        } else {
                            etaHat = tg;
                            if (plotCI) {
                                vStd[0] = 1.0;
                                const double scaleDir = (normGammaSlopeSq > 1e-24)
                                        ? ((tg - gamma[0]) / normGammaSlopeSq)
                                        : 0.0;
                                for (int j = 1; j < p; ++j)
                                    vStd[size_t(j)] = scaleDir * gamma[size_t(j)];
                                for (int a = 0; a < p; ++a) {
                                    for (int b = 0; b < p; ++b)
                                        varEta += vStd[size_t(a)] * covGamma[size_t(a)][size_t(b)]
                                                * vStd[size_t(b)];
                                }
                            }
                        }

                        gyProb[size_t(gi)] = sigmoidStable(etaHat);
                        if (plotCI) {
                            if (std::isfinite(varEta) && varEta > 0.0) {
                                const double seEta = std::sqrt(varEta);
                                gyLo[size_t(gi)] = sigmoidStable(etaHat - z95 * seEta);
                                gyHi[size_t(gi)] = sigmoidStable(etaHat + z95 * seEta);
                            } else {
                                plotCI = false;
                            }
                        }
                    }

                    auto attachCurve = [&](const QString &name, const std::vector<double> &yvCurve,
                                           const QPen &pen) {
                        FunctionCurve *fc = new FunctionCurve(
                                app, FunctionCurve::Normal, gp->generateFunctionName(name));
                        fc->setPen(pen);
                        fc->setData(gx.data(), yvCurve.data(), nGrid);
                        fc->setRange(tMin, tMax);
                        gp->insertPlotItem(fc, Graph::Line);
                        gp->addFitCurve(fc);
                    };

                    attachCurve(QStringLiteral("Logistic_P"), gyProb,
                                QPen(QColor(215, 25, 28), 2, Qt::SolidLine));
                    if (plotCI) {
                        attachCurve(QStringLiteral("Logistic_95CI_Lower"), gyLo,
                                    QPen(QColor(0, 114, 178), 1, Qt::DashLine));
                        attachCurve(QStringLiteral("Logistic_95CI_Upper"), gyHi,
                                    QPen(QColor(0, 114, 178), 1, Qt::DashLine));
                    }

                    // P = 0.5 decision threshold reference line
                    FunctionCurve *halfLn = new FunctionCurve(
                            app, FunctionCurve::Normal, gp->generateFunctionName(QStringLiteral("P_0.5")));
                    halfLn->setPen(QPen(Qt::darkGray, 1, Qt::DotLine));
                    const double hx[2] = { tMin, tMax };
                    const double hy[2] = { 0.5, 0.5 };
                    halfLn->setData(hx, hy, 2);
                    halfLn->setRange(tMin, tMax);
                    gp->insertPlotItem(halfLn, Graph::Line);

                    gp->setTitle(QObject::tr("Binary Logistic Regression (McFadden R² = %1, LR p = %2)")
                                         .arg(res.mcFaddenR2, 0, 'f', 4)
                                         .arg(res.lrPValue, 0, 'g', 4));
                    gp->setTitleFont(titleFnt);
                    gp->setXAxisTitle((k == 1)
                                              ? xLabels[0]
                                              : QObject::tr("Linear Predictor Score z = X·β"));
                    gp->setYAxisTitle(QObject::tr("Probability P(%1 = 1)")
                                              .arg(srcTable->colLabel(yCol)));
                    gp->setScale(2, -0.05, 1.05);
                    gp->initFonts(axisFnt, numFnt);
                    if (gp->legend())
                        gp->legend()->setFont(legFnt);

                    QStringList summaryLines;
                    summaryLines << QObject::tr("Logistic MLE (95% CI Band)");
                    summaryLines << QObject::tr("LR χ²(%1) = %2 (p = %3)")
                                            .arg(k)
                                            .arg(res.lrChi2, 0, 'g', 5)
                                            .arg(res.lrPValue, 0, 'g', 4);
                    summaryLines << QObject::tr("McFadden R² = %1, AIC = %2")
                                            .arg(res.mcFaddenR2, 0, 'f', 4)
                                            .arg(res.aic, 0, 'g', 6);
                    if (k == 1) {
                        summaryLines << QObject::tr("OR(%1) = %2")
                                                .arg(xLabels[0])
                                                .arg(res.oddsRatios[0], 0, 'g', 5);
                    }
                    insertArialSummaryBox(gp, summaryLines.join('\n'));
                    gp->replot();
                }

                ml->setFonts(titleFnt, axisFnt, numFnt, legFnt);
                res.probGraph = ml;
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

    auto *livePreviewChk = new QCheckBox(QObject::tr("Live preview on graph"), &dlg);
    livePreviewChk->setChecked(true);
    form->addRow(QString(), livePreviewChk);

    mainLay->addLayout(form);

    DialogPreviewCurveGuard previewGuard;
    previewGuard.graph = g;
    auto updatePiecewisePreview = [&]() {
        if (!livePreviewChk->isChecked() || !previewGuard.graph) {
            previewGuard.clear();
            return;
        }
        QwtPlotCurve *c = resolveCurveOnGraph(previewGuard.graph, curveCombo->currentText());
        std::vector<double> xv, yv;
        if (!extractUnmaskedSortedXY(c, xv, yv)) {
            previewGuard.clear();
            return;
        }
        PiecewiseResult tmpRes;
        double rss = 0.0;
        std::vector<double> gx, gy;
        if (computePiecewiseCore(xv, yv, segSpin->value(), tmpRes, rss, gx, gy)) {
            previewGuard.showData(curveCombo->currentText(), gx, gy, QColor(215, 25, 28, 210));
        } else {
            previewGuard.clear();
        }
    };

    QObject::connect(curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { updatePiecewisePreview(); });
    QObject::connect(segSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dlg,
                     [&](int) { updatePiecewisePreview(); });
    QObject::connect(livePreviewChk, &QCheckBox::toggled, &dlg,
                     [&](bool) { updatePiecewisePreview(); });
    updatePiecewisePreview();

    auto *btns = new QDialogButtonBox(&dlg);
    QPushButton *okBtn = btns->addButton(QObject::tr("Fit"), QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn = btns->addButton(QObject::tr("Cancel"), QDialogButtonBox::RejectRole);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    mainLay->addWidget(btns);

    const int rc = dlg.exec();
    previewGuard.clear();
    if (rc == QDialog::Accepted) {
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

    auto *annotateChk = new QCheckBox(QObject::tr("Add summary box on graph"), &dlg);
    annotateChk->setChecked(true);
    form->addRow(QString(), annotateChk);

    auto *livePreviewChk = new QCheckBox(QObject::tr("Live preview on graph"), &dlg);
    livePreviewChk->setChecked(true);
    form->addRow(QString(), livePreviewChk);

    mainLay->addLayout(form);

    DialogPreviewCurveGuard previewGuard;
    previewGuard.graph = g;
    auto updateRobustPreview = [&]() {
        if (!livePreviewChk->isChecked() || !previewGuard.graph) {
            previewGuard.clear();
            return;
        }
        QwtPlotCurve *c = resolveCurveOnGraph(previewGuard.graph, curveCombo->currentText());
        std::vector<double> xv, yv;
        if (!extractUnmaskedSortedXY(c, xv, yv) || xv.size() < 2) {
            previewGuard.clear();
            return;
        }
        const auto m = static_cast<RobustMethod>(methodCombo->currentData().toInt());
        RobustFitResult tmpRes;
        if (computeRobustOrQuantileCore(xv, yv, m, tauSpin->value(), tmpRes)) {
            const std::vector<double> gx = { xv.front(), xv.back() };
            const std::vector<double> gy = { tmpRes.intercept + tmpRes.slope * xv.front(),
                                             tmpRes.intercept + tmpRes.slope * xv.back() };
            previewGuard.showData(curveCombo->currentText(), gx, gy, QColor(0, 114, 178, 210));
        } else {
            previewGuard.clear();
        }
    };

    QObject::connect(methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [methodCombo, tauSpin, &updateRobustPreview](int) {
                         tauSpin->setEnabled(methodCombo->currentData().toInt() == int(QuantileReg));
                         updateRobustPreview();
                     });
    QObject::connect(curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { updateRobustPreview(); });
    QObject::connect(tauSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                     [&](double) { updateRobustPreview(); });
    QObject::connect(livePreviewChk, &QCheckBox::toggled, &dlg,
                     [&](bool) { updateRobustPreview(); });
    updateRobustPreview();

    auto *btns = new QDialogButtonBox(&dlg);
    QPushButton *okBtn = btns->addButton(QObject::tr("Fit"), QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn = btns->addButton(QObject::tr("Cancel"), QDialogButtonBox::RejectRole);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    mainLay->addWidget(btns);

    const int rc = dlg.exec();
    previewGuard.clear();
    if (rc == QDialog::Accepted) {
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

void PiecewiseAndRobustFit::showLogisticRegressionDialog(ApplicationWindow *app)
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
        QMessageBox::information(
                app, QObject::tr("Binary Logistic Regression"),
                QObject::tr("Please open a worksheet with a binary Y column and at least one predictor column."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Binary Logistic Regression (MLE & Odds Ratios)"));
    dlg.setMinimumWidth(440);

    auto *mainLay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();

    auto *tblCombo = new QComboBox(&dlg);
    for (Table *t : tables)
        tblCombo->addItem(t->name());
    form->addRow(QObject::tr("Worksheet:"), tblCombo);

    auto *yCombo = new QComboBox(&dlg);
    form->addRow(QObject::tr("Binary Response (Y in {0, 1}):"), yCombo);
    mainLay->addLayout(form);

    auto *xGrp = new QGroupBox(QObject::tr("Predictor Variables (X1, X2, ...)"), &dlg);
    auto *xLay = new QVBoxLayout(xGrp);
    auto *xList = new QListWidget(xGrp);
    xList->setSelectionMode(QAbstractItemView::NoSelection);
    xLay->addWidget(xList);
    mainLay->addWidget(xGrp);

    auto *probChk = new QCheckBox(
            QObject::tr("Plot Logistic Probability & 95% CI Curve"), &dlg);
    probChk->setChecked(true);
    mainLay->addWidget(probChk);

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
    QPushButton *okBtn =
            btns->addButton(QObject::tr("Run Logistic Regression"), QDialogButtonBox::AcceptRole);
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
            QMessageBox::warning(
                    app, QObject::tr("Binary Logistic Regression"),
                    QObject::tr("Please select at least one predictor column distinct from Y."));
            return;
        }
        const auto out = runLogisticRegression(app, t, yName, xNames, probChk->isChecked());
        if (!out.converged) {
            QMessageBox::warning(
                    app, QObject::tr("Binary Logistic Regression"),
                    QObject::tr("Logistic regression could not converge (ensure Y has two distinct binary outcomes and sufficient valid rows)."));
        }
    }
}
