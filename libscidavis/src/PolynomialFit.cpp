/***************************************************************************
    File                 : PolynomialFit.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Polynomial Fit and Linear Fit classes

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
#include "PolynomialFit.h"
#include "fit_gsl.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"
#include "Table.h"
#include "future/core/column/Column.h"

#include <qwt_plot.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>

#include <QApplication>
#include <QMessageBox>
#include <QLocale>

#include <gsl/gsl_multifit.h>
#include <gsl/gsl_fit.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <numeric>
#include <unordered_map>
using namespace std;

namespace {

static bool isNonInteractivePolyFit(const QObject *parentObj, bool silent)
{
    if (silent)
        return true;
    if (qApp && qApp->property("torigraph_noninteractive").toBool())
        return true;
    const ApplicationWindow *app = dynamic_cast<const ApplicationWindow *>(parentObj);
    return app && app->isHeadlessMode();
}

struct CenteredPolyCache
{
    long double xMean = 0.0L;
    long double xScale = 1.0L;
    std::vector<double> cU;
    std::vector<double> coeffsX;
    bool apparent = false;
};

static std::mutex &centeredPolyCacheMutex()
{
    static std::mutex m;
    return m;
}

static std::unordered_map<const Fit *, CenteredPolyCache> &centeredPolyCacheMap()
{
    static std::unordered_map<const Fit *, CenteredPolyCache> mp;
    return mp;
}

static void eraseCenteredPolyCache(const Fit *key)
{
    std::lock_guard<std::mutex> lock(centeredPolyCacheMutex());
    centeredPolyCacheMap().erase(key);
}

static void storeCenteredPolyCache(const Fit *key, const CenteredPolyCache &cache)
{
    std::lock_guard<std::mutex> lock(centeredPolyCacheMutex());
    centeredPolyCacheMap()[key] = cache;
}

static bool loadCenteredPolyCache(const Fit *key, CenteredPolyCache &out)
{
    std::lock_guard<std::mutex> lock(centeredPolyCacheMutex());
    auto it = centeredPolyCacheMap().find(key);
    if (it == centeredPolyCacheMap().end())
        return false;
    out = it->second;
    return true;
}

static double evalPolyHornerLD(const std::vector<double> &par, unsigned nParams, double x)
{
    const size_t m = std::min<size_t>(size_t(nParams), par.size());
    if (m == 0)
        return 0.0;
    const long double xld = static_cast<long double>(x);
    long double yi = static_cast<long double>(par[m - 1]);
    for (size_t j = m - 1; j > 0; --j)
        yi = yi * xld + static_cast<long double>(par[j - 1]);
    return static_cast<double>(yi);
}

static bool evalWithCenteredCacheIfApplicable(const CenteredPolyCache &cache,
                                              const std::vector<double> &par, unsigned nParams,
                                              double xt, double &outVal)
{
    if (cache.cU.empty() || cache.coeffsX.size() != par.size()
        || size_t(nParams) != cache.coeffsX.size())
        return false;

    int diffCount = 0;
    size_t diffIdx = 0;
    for (size_t j = 0; j < par.size(); ++j) {
        if (par[j] != cache.coeffsX[j]) {
            ++diffCount;
            diffIdx = j;
            if (diffCount > 1)
                return false;
        }
    }
    if (diffCount == 1) {
        const double d = std::abs(par[diffIdx] - cache.coeffsX[diffIdx]);
        if (!std::isfinite(d) || d > 1e-2 * std::max(1.0, std::abs(cache.coeffsX[diffIdx])))
            return false;
    }

    const long double u = (static_cast<long double>(xt) - cache.xMean) / cache.xScale;
    long double val = static_cast<long double>(cache.cU.back());
    for (size_t j = cache.cU.size() - 1; j > 0; --j)
        val = val * u + static_cast<long double>(cache.cU[j - 1]);
    if (diffCount == 1) {
        const long double diff = static_cast<long double>(par[diffIdx])
                - static_cast<long double>(cache.coeffsX[diffIdx]);
        long double xpow = 1.0L;
        const long double xld = static_cast<long double>(xt);
        for (size_t k = 0; k < diffIdx; ++k)
            xpow *= xld;
        val += diff * xpow;
    }
    outVal = static_cast<double>(val);
    return true;
}

static void collectActiveXY(const std::vector<double> &x, const std::vector<double> &y,
                            const std::vector<double> &weights, std::vector<double> &xOut,
                            std::vector<double> &yOut)
{
    const size_t n = std::min(x.size(), y.size());
    xOut.clear();
    yOut.clear();
    xOut.reserve(n);
    yOut.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (i < weights.size() && (!std::isfinite(weights[i]) || !(weights[i] > 0.0)))
            continue;
        if (!std::isfinite(x[i]) || !std::isfinite(y[i]))
            continue;
        xOut.push_back(x[i]);
        yOut.push_back(y[i]);
    }
}

static bool computeWeightedMeanXY(const std::vector<double> &x, const std::vector<double> &y,
                                  const std::vector<double> &weights, double &wmXOut,
                                  double &wmYOut)
{
    const size_t n = std::min(x.size(), y.size());
    double W = 0.0;
    double wmX = 0.0;
    double wmY = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double wi = (i < weights.size()) ? weights[i] : 1.0;
        if (std::isfinite(wi) && wi > 0.0 && std::isfinite(x[i]) && std::isfinite(y[i])) {
            W += wi;
            wmX += (x[i] - wmX) * (wi / W);
            wmY += (y[i] - wmY) * (wi / W);
        }
    }
    if (!(W > 0.0) || !std::isfinite(wmX) || !std::isfinite(wmY))
        return false;
    wmXOut = wmX;
    wmYOut = wmY;
    return true;
}

static bool solveCenteredWeightedPolynomial(const std::vector<double> &x,
                                            const std::vector<double> &y,
                                            const std::vector<double> &weights, unsigned p,
                                            std::vector<double> &coeffsOut, gsl_matrix *covarOut,
                                            double &chi2Out, std::vector<double> *yFitOut,
                                            CenteredPolyCache *cacheOut)
{
    const size_t n = std::min(x.size(), y.size());
    if (n == 0 || p == 0)
        return false;

    long double sumX = 0.0L;
    size_t activeCount = 0;
    for (size_t i = 0; i < n; ++i) {
        const bool active = (i < weights.size())
                ? (std::isfinite(weights[i]) && weights[i] > 0.0)
                : true;
        if (active && std::isfinite(x[i]) && std::isfinite(y[i])) {
            sumX += static_cast<long double>(x[i]);
            ++activeCount;
        }
    }
    if (activeCount == 0) {
        for (size_t i = 0; i < n; ++i) {
            if (std::isfinite(x[i])) {
                sumX += static_cast<long double>(x[i]);
                ++activeCount;
            }
        }
    }
    const long double xMean =
            (activeCount > 0) ? (sumX / static_cast<long double>(activeCount)) : 0.0L;

    long double maxDev = 0.0L;
    for (size_t i = 0; i < n; ++i) {
        const bool active = (i < weights.size())
                ? (std::isfinite(weights[i]) && weights[i] > 0.0)
                : true;
        if (active && std::isfinite(x[i]) && std::isfinite(y[i])) {
            const long double dev = std::abs(static_cast<long double>(x[i]) - xMean);
            if (dev > maxDev)
                maxDev = dev;
        }
    }
    const long double xScale =
            (maxDev > 0.0L && std::isfinite(static_cast<double>(maxDev))) ? maxDev : 1.0L;

    gsl_matrix *U = gsl_matrix_alloc(n, p);
    gsl_vector *cU = gsl_vector_alloc(p);
    gsl_matrix *covU = gsl_matrix_alloc(p, p);
    gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(n, p);

    const bool hasWeights = (weights.size() >= n);
    std::vector<double> yClean(n, 0.0);
    std::vector<double> wClean(n, 0.0);
    bool anyInactive = false;

    for (size_t i = 0; i < n; ++i) {
        const double wi = hasWeights ? weights[i] : 1.0;
        if (!std::isfinite(wi) || !(wi > 0.0) || !std::isfinite(x[i]) || !std::isfinite(y[i])) {
            anyInactive = true;
            for (unsigned j = 0; j < p; ++j)
                gsl_matrix_set(U, i, j, 0.0);
            yClean[i] = 0.0;
            wClean[i] = 0.0;
            continue;
        }
        const double ui = static_cast<double>((static_cast<long double>(x[i]) - xMean) / xScale);
        double upow = 1.0;
        bool rowFinite = std::isfinite(ui);
        for (unsigned j = 0; j < p; ++j) {
            if (!std::isfinite(upow)) {
                rowFinite = false;
                break;
            }
            gsl_matrix_set(U, i, j, upow);
            upow *= ui;
        }
        if (!rowFinite) {
            anyInactive = true;
            for (unsigned j = 0; j < p; ++j)
                gsl_matrix_set(U, i, j, 0.0);
            yClean[i] = 0.0;
            wClean[i] = 0.0;
        } else {
            yClean[i] = y[i];
            wClean[i] = wi;
        }
    }

    gsl_vector_const_view yv = gsl_vector_const_view_array(yClean.data(), n);
    int status = GSL_SUCCESS;
    if (hasWeights || anyInactive) {
        gsl_vector_const_view wv = gsl_vector_const_view_array(wClean.data(), n);
        status = gsl_multifit_wlinear(U, &wv.vector, &yv.vector, cU, covU, &chi2Out, work);
    } else {
        status = gsl_multifit_linear(U, &yv.vector, cU, covU, &chi2Out, work);
    }

    std::vector<double> cUVec(p, 0.0);
    for (unsigned j = 0; j < p; ++j)
        cUVec[j] = gsl_vector_get(cU, j);

    // Evaluate fitted Y values directly in centered u-basis so residuals and RSS avoid
    // catastrophic cancellation at large X offsets.
    if (yFitOut) {
        yFitOut->resize(n);
        for (size_t i = 0; i < n; ++i) {
            const long double ui = (static_cast<long double>(x[i]) - xMean) / xScale;
            long double yi = static_cast<long double>(cUVec[p - 1]);
            for (unsigned j = p - 1; j > 0; --j)
                yi = yi * ui + static_cast<long double>(cUVec[j - 1]);
            (*yFitOut)[i] = static_cast<double>(yi);
        }
    }

    // Exact upper-triangular binomial shift+scale matrix T mapping centered coefficients c_u
    // back to the original x basis (a = T * c_u, cov_x = T * cov_u * T^T).
    std::vector<std::vector<long double>> T(p, std::vector<long double>(p, 0.0L));
    T[0][0] = 1.0L;
    const long double invS = 1.0L / xScale;
    const long double negMuOverS = -xMean / xScale;
    for (unsigned k = 1; k < p; ++k) {
        T[0][k] = negMuOverS * T[0][k - 1];
        for (unsigned j = 1; j < k; ++j)
            T[j][k] = invS * T[j - 1][k - 1] + negMuOverS * T[j][k - 1];
        T[k][k] = invS * T[k - 1][k - 1];
    }

    coeffsOut.resize(p, 0.0);
    for (unsigned j = 0; j < p; ++j) {
        long double acc = 0.0L;
        for (unsigned k = j; k < p; ++k)
            acc += T[j][k] * static_cast<long double>(cUVec[k]);
        coeffsOut[j] = static_cast<double>(acc);
    }

    if (covarOut && covarOut->size1 >= p && covarOut->size2 >= p) {
        std::vector<std::vector<long double>> tmp(p, std::vector<long double>(p, 0.0L));
        for (unsigned a = 0; a < p; ++a) {
            for (unsigned m = 0; m < p; ++m) {
                long double s = 0.0L;
                for (unsigned k = a; k < p; ++k)
                    s += T[a][k] * static_cast<long double>(gsl_matrix_get(covU, k, m));
                tmp[a][m] = s;
            }
        }
        for (unsigned a = 0; a < p; ++a) {
            for (unsigned b = a; b < p; ++b) {
                long double s = 0.0L;
                for (unsigned m = b; m < p; ++m)
                    s += tmp[a][m] * T[b][m];
                const double val = static_cast<double>(s);
                gsl_matrix_set(covarOut, a, b, val);
                gsl_matrix_set(covarOut, b, a, val);
            }
        }
    }

    if (cacheOut) {
        cacheOut->xMean = xMean;
        cacheOut->xScale = xScale;
        cacheOut->cU = std::move(cUVec);
        cacheOut->coeffsX = coeffsOut;
    }

    gsl_multifit_linear_free(work);
    gsl_matrix_free(covU);
    gsl_vector_free(cU);
    gsl_matrix_free(U);
    return status == GSL_SUCCESS;
}

// Reports why an apparent (log-space) linear fit cannot run, instead of returning silently.
static void reportApparentFitShortage(Fit *fit, ApplicationWindow *app, bool silent, int needed,
                                      int remaining)
{
    const QString msg =
            QObject::tr("Apparent linear fit needs at least %1 points with positive values on the "
                        "logarithmic axis, but only %2 remain after excluding non-positive values.")
                    .arg(needed)
                    .arg(remaining);
    if (app && app->writeFitResultsToLog && !silent)
        app->updateLog(QStringLiteral("[") + fit->objectName() + QStringLiteral("] ") + msg
                       + QStringLiteral("\n"));
    if (!silent && app && !app->isHeadlessMode()
        && !(qApp && qApp->property("torigraph_noninteractive").toBool()))
        QMessageBox::warning(app, QObject::tr("Fit Error"), msg);
}

static bool isGraphAxisLog10(Graph *g, int axis)
{
    if (!g || !g->plotWidget())
        return false;
    const QwtScaleEngine *se = g->plotWidget()->axisScaleEngine(axis);
    if (dynamic_cast<const QwtLog10ScaleEngine *>(se) != nullptr)
        return true;
    const QwtScaleMap map = g->plotWidget()->canvasMap(axis);
    if (map.transformation() && map.transformation()->type() == QwtScaleTransformation::Log10)
        return true;
    return false;
}

static int poly_model_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    FitData *d = static_cast<FitData *>(params);
    for (size_t i = 0; i < d->n; ++i) {
        const double s = d->sigma ? d->sigma[i] : 1.0;
        if (!std::isfinite(s) || !(s > 0.0) || s >= 1e140) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        const long double xi = static_cast<long double>(d->X[i]);
        long double yi = (d->p > 0) ? static_cast<long double>(gsl_vector_get(x, d->p - 1)) : 0.0L;
        for (size_t j = (d->p > 0 ? d->p - 1 : 0); j > 0; --j)
            yi = yi * xi + static_cast<long double>(gsl_vector_get(x, j - 1));
        gsl_vector_set(f, i, (static_cast<double>(yi) - d->Y[i]) / s);
    }
    return GSL_SUCCESS;
}

static int poly_model_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    Q_UNUSED(x)
    FitData *d = static_cast<FitData *>(params);
    for (size_t i = 0; i < d->n; ++i) {
        const double s = d->sigma ? d->sigma[i] : 1.0;
        if (!std::isfinite(s) || !(s > 0.0) || s >= 1e140) {
            for (size_t j = 0; j < d->p; ++j)
                gsl_matrix_set(J, i, j, 0.0);
            continue;
        }
        double xi = d->X[i];
        double xpow = 1.0;
        for (size_t j = 0; j < d->p; ++j) {
            gsl_matrix_set(J, i, j, xpow / s);
            xpow *= xi;
        }
    }
    return GSL_SUCCESS;
}

static int poly_model_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    poly_model_f(x, params, f);
    poly_model_df(x, params, J);
    return GSL_SUCCESS;
}

static double poly_model_d(const gsl_vector *x, void *params)
{
    FitData *d = static_cast<FitData *>(params);
    double sum = 0.0;
    for (size_t i = 0; i < d->n; ++i) {
        const double s = d->sigma ? d->sigma[i] : 1.0;
        if (!std::isfinite(s) || !(s > 0.0) || s >= 1e140)
            continue;
        const long double xi = static_cast<long double>(d->X[i]);
        long double yi = (d->p > 0) ? static_cast<long double>(gsl_vector_get(x, d->p - 1)) : 0.0L;
        for (size_t j = (d->p > 0 ? d->p - 1 : 0); j > 0; --j)
            yi = yi * xi + static_cast<long double>(gsl_vector_get(x, j - 1));
        double r = (static_cast<double>(yi) - d->Y[i]) / s;
        sum += r * r;
    }
    return sum;
}

static double computeTransformedR2(const std::vector<double> &yt,
                                   const std::vector<double> &yfit_t)
{
    if (yt.size() < 2 || yfit_t.size() != yt.size())
        return 1.0;
    double mean = std::accumulate(yt.begin(), yt.end(), 0.0) / double(yt.size());
    double tss = 0.0, rss = 0.0;
    for (size_t i = 0; i < yt.size(); ++i) {
        double dt = yt[i] - mean;
        double dr = yt[i] - yfit_t[i];
        tss += dt * dt;
        rss += dr * dr;
    }
    return (tss > 1e-30) ? (1.0 - rss / tss) : 1.0;
}

struct ApparentLogData
{
    std::vector<double> xt;
    std::vector<double> yt;
    std::vector<double> wt;
    std::vector<size_t> activeIdx;
    int excludedLogCount = 0;
    int excludedWeightCount = 0;
};

static ApparentLogData prepareApparentLogData(const std::vector<double> &dx,
                                              const std::vector<double> &dy, bool logX, bool logY,
                                              bool isWeighted, const std::vector<double> &baseW)
{
    ApparentLogData out;
    const size_t n = std::min(dx.size(), dy.size());
    out.xt.reserve(n);
    out.yt.reserve(n);
    out.wt.reserve(n);
    out.activeIdx.reserve(n);
    const double ln10 = std::log(10.0);
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(dx[i]) || !std::isfinite(dy[i])) {
            if (logX || logY)
                ++out.excludedLogCount;
            continue;
        }
        if ((logX && !(dx[i] > 0.0)) || (logY && !(dy[i] > 0.0))) {
            ++out.excludedLogCount;
            continue;
        }
        double wi = 1.0;
        if (isWeighted) {
            if (i >= baseW.size() || !std::isfinite(baseW[i]) || baseW[i] <= 0.0) {
                ++out.excludedWeightCount;
                continue;
            }
            wi = baseW[i];
        }
        out.xt.push_back(logX ? std::log10(dx[i]) : dx[i]);
        out.yt.push_back(logY ? std::log10(dy[i]) : dy[i]);
        if (!isWeighted) {
            out.wt.push_back(1.0);
        } else {
            const double w_trans = logY ? (wi * (dy[i] * ln10) * (dy[i] * ln10)) : wi;
            out.wt.push_back(std::max(1e-30, w_trans));
        }
        out.activeIdx.push_back(i);
    }
    return out;
}

static void populateApparentLogFitOutputs(const ApparentLogData &data,
                                          const std::vector<double> &dy, bool logY,
                                          const std::vector<double> &yfit_t,
                                          std::vector<double> &yFitOut, bool &hasCustomRss,
                                          double &customRss)
{
    yFitOut.assign(dy.size(), std::numeric_limits<double>::quiet_NaN());
    double rssActive = 0.0;
    for (size_t k = 0; k < data.activeIdx.size(); ++k) {
        const size_t origIdx = data.activeIdx[k];
        const double yHat = logY ? std::pow(10.0, yfit_t[k]) : yfit_t[k];
        yFitOut[origIdx] = yHat;
        const double r = dy[origIdx] - yHat;
        rssActive += r * r;
    }
    hasCustomRss = true;
    customRss = rssActive;
}

static bool tryGenerateApparentLogX(const std::vector<double> &dx, bool genFunction, bool logX,
                                    int nPoints, std::vector<double> &X)
{
    if (!genFunction || !logX || dx.empty())
        return false;
    double minPos = std::numeric_limits<double>::infinity();
    double maxPos = -std::numeric_limits<double>::infinity();
    bool foundPos = false;
    for (double xv : dx) {
        if (std::isfinite(xv) && xv > 0.0) {
            minPos = std::min(minPos, xv);
            maxPos = std::max(maxPos, xv);
            foundPos = true;
        }
    }
    if (!foundPos)
        return false;
    const double lx0 = std::log10(minPos);
    const double lx1 = std::log10(maxPos);
    const double lstep = (nPoints > 1) ? ((lx1 - lx0) / (nPoints - 1)) : 0.0;
    for (int i = 0; i < nPoints; ++i)
        X[size_t(i)] = std::pow(10.0, lx0 + i * lstep);
    return true;
}

static void evaluateApparentPolyY(bool logX, bool logY, int nPoints, unsigned nParams,
                                  const std::vector<double> &par, const std::vector<double> &X,
                                  std::vector<double> &Y,
                                  const CenteredPolyCache *cache = nullptr)
{
    for (int i = 0; i < nPoints; ++i) {
        if (logX && !(X[size_t(i)] > 0.0)) {
            Y[size_t(i)] = std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        const double xt = logX ? std::log10(X[size_t(i)]) : X[size_t(i)];
        double yt = 0.0;
        if (!cache || !evalWithCenteredCacheIfApplicable(*cache, par, nParams, xt, yt))
            yt = evalPolyHornerLD(par, nParams, xt);
        Y[size_t(i)] = logY ? std::pow(10.0, yt) : yt;
    }
}

} // namespace

PolynomialFit::PolynomialFit(ApplicationWindow *parent, Graph *g, int order, bool legend)
    : Fit(parent, g), d_order(order), show_legend(legend)
{
    init();
}

PolynomialFit::PolynomialFit(ApplicationWindow *parent, Graph *g, QString &curveTitle, int order,
                             bool legend)
    : Fit(parent, g), d_order(order), show_legend(legend)
{
    init();
    setDataFromCurve(curveTitle);
}

PolynomialFit::PolynomialFit(ApplicationWindow *parent, Graph *g, QString &curveTitle, double start,
                             double end, int order, bool legend)
    : Fit(parent, g), d_order(order), show_legend(legend)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

PolynomialFit::~PolynomialFit()
{
    eraseCenteredPolyCache(this);
    if (!is_non_linear && d_param_init) {
        gsl_vector_free(d_param_init);
        d_param_init = nullptr;
    }
}

void PolynomialFit::init()
{
    setObjectName(tr("Poly"));
    is_non_linear = false;
    d_f = poly_model_f;
    d_df = poly_model_df;
    d_fdf = poly_model_fdf;
    d_fsimplex = poly_model_d;
    d_explanation = tr("Polynomial");
    d_p = d_order + 1;
    d_min_points = d_p;

    d_param_init = gsl_vector_alloc(d_p);
    gsl_vector_set_all(d_param_init, 1.0);
    covar = gsl_matrix_alloc(d_p, d_p);
    d_results.resize(d_p);

    d_formula = generateFormula(d_order);
    d_param_names = generateParameterList(d_order);

    for (unsigned i = 0; i < d_p; i++)
        d_param_explain << "";
}

QString PolynomialFit::generateFormula(int order)
{
    QString formula;
    for (int i = 0; i < order + 1; i++) {
        QString par = "a" + QString::number(i);
        formula += par;
        if (i > 0)
            formula += "*x";
        if (i > 1)
            formula += "^" + QString::number(i);
        if (i != order)
            formula += "+";
    }
    return formula;
}

QStringList PolynomialFit::generateParameterList(int order)
{
    QStringList lst;
    for (int i = 0; i < order + 1; i++)
        lst << "a" + QString::number(i);
    return lst;
}

bool PolynomialFit::calculateFitCurveData(const vector<double> &par, std::vector<double> &X,
                                          std::vector<double> &Y)
{
    CenteredPolyCache cache;
    const bool hasCache = loadCenteredPolyCache(this, cache);
    const bool isApparent = d_apparent_fit && (d_app_log_x || d_app_log_y);
    const CenteredPolyCache *useCache =
            (hasCache && cache.apparent == isApparent && cache.coeffsX == d_results) ? &cache
                                                                                     : nullptr;

    if (isApparent) {
        if (!tryGenerateApparentLogX(d_x, d_gen_function, d_app_log_x, d_points, X))
            generateX(X);
        evaluateApparentPolyY(d_app_log_x, d_app_log_y, d_points, d_p, par, X, Y, useCache);
        return true;
    }

    generateX(X);
    for (int i = 0; i < d_points; i++) {
        double yi = 0.0;
        if (!useCache || !evalWithCenteredCacheIfApplicable(*useCache, par, d_p, X[i], yi))
            yi = evalPolyHornerLD(par, d_p, X[i]);
        Y[i] = yi;
    }
    return true;
}

void PolynomialFit::fit()
{
    if (d_init_err)
        return;

    eraseCenteredPolyCache(this);
    d_result_errors.clear();
    d_has_custom_r2 = false;
    d_has_custom_rss = false;
    d_custom_rss = 0.0;
    d_has_custom_dof = false;
    d_custom_n_points = 0;
    d_custom_p_free = 0;
    d_straight_line_fit = false;
    d_convergence = Converged;
    d_fit_notes.clear();

    const int minNeeded = std::max(1, effectiveFreeParameters());
    const int nActive = effectivePointCount();
    if (minNeeded > nActive) {
        chi_2 = -1.0;
        d_y_fit.clear();
        d_results.assign(d_p, std::numeric_limits<double>::quiet_NaN());
        if (!isNonInteractivePolyFit(parent(), d_silent))
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Fit Error"),
                    tr("You need at least %1 valid data points (with positive weight) for this fit operation. "
                       "Operation aborted!")
                            .arg(minNeeded));
        return;
    }

    int xAx = d_curve ? d_curve->xAxis() : QwtPlot::xBottom;
    int yAx = d_curve ? d_curve->yAxis() : QwtPlot::yLeft;
    d_app_log_x = d_apparent_fit && isGraphAxisLog10(d_graph, xAx);
    d_app_log_y = d_apparent_fit && isGraphAxisLog10(d_graph, yAx);
    if (d_apparent_fit && !d_app_log_x && !d_app_log_y)
        d_fit_notes << tr("Apparent linear fit: no axis is logarithmic, so an ordinary linear fit "
                          "was performed.");

    if (d_apparent_fit && (d_app_log_x || d_app_log_y)) {
        const bool isWeighted =
                !(d_y_error_source == UnknownErrors && d_weighting_method == WeightNone);
        const std::vector<double> baseW = isWeighted ? computeWeightsVector() : std::vector<double>();
        const ApparentLogData logData =
                prepareApparentLogData(d_x, d_y, d_app_log_x, d_app_log_y, isWeighted, baseW);
        if (logData.excludedLogCount > 0)
            d_fit_notes << tr("%1 of %2 points with non-positive values were excluded from the "
                              "log-space fit.")
                                   .arg(logData.excludedLogCount)
                                   .arg(int(d_x.size()));
        if (logData.excludedWeightCount > 0)
            d_fit_notes << tr("%1 of %2 points with non-positive or invalid weights were excluded "
                              "from the log-space fit.")
                                   .arg(logData.excludedWeightCount)
                                   .arg(int(d_x.size()));
        const int effFree = effectiveFreeParameters();
        const int minLogNeeded = std::max(1, effFree);
        if (logData.xt.size() < size_t(minLogNeeded)) {
            chi_2 = -1.0;
            d_y_fit.clear();
            d_results.assign(d_p, std::numeric_limits<double>::quiet_NaN());
            reportApparentFitShortage(this, (ApplicationWindow *)parent(), d_silent, minLogNeeded,
                                      int(logData.xt.size()));
            return;
        }
        if (d_p == 2)
            setStraightLinePearson(logData.xt, logData.yt);

        if (hasActiveConstraints()) {
            // Honor fixed parameters, bounds, and linear constraints inside apparent log-space fit
            if (logData.xt.size() >= size_t(d_p)) {
                std::vector<double> seedC;
                double tmpChi2 = 0.0;
                if (solveCenteredWeightedPolynomial(logData.xt, logData.yt, logData.wt, d_p, seedC,
                                                    nullptr, tmpChi2, nullptr, nullptr)) {
                    for (unsigned j = 0; j < d_p && j < seedC.size(); ++j) {
                        if (!d_all_guesses_set && !d_explicit_init_vals.contains(int(j)))
                            gsl_vector_set(d_param_init, j, seedC[j]);
                    }
                }
            }
            applyExplicitInitialGuesses();

            const std::vector<double> savedX = d_x;
            const std::vector<double> savedY = d_y;
            const std::vector<double> savedYErr = d_y_errors;
            const std::vector<double> savedRawW = d_raw_weights;
            const std::vector<double> savedRawYErr = d_raw_y_errors;
            const WeightingMethod savedWMethod = d_weighting_method;
            const ErrorSource savedErrSrc = d_y_error_source;
            const bool savedAppLogX = d_app_log_x;
            const bool savedAppLogY = d_app_log_y;

            d_x = logData.xt;
            d_y = logData.yt;
            d_app_log_x = false;
            d_app_log_y = false;
            if (isWeighted) {
                d_weighting_method = WeightDirect;
                d_y_error_source = CustomErrors;
                d_raw_weights = logData.wt;
                d_raw_y_errors.clear();
                applyWeightsToErrorVector(logData.wt);
            } else {
                d_weighting_method = WeightNone;
                d_y_error_source = UnknownErrors;
                d_raw_weights.clear();
                d_raw_y_errors.clear();
                d_y_errors.assign(logData.xt.size(), 1.0);
            }

            int iterations = 0, status = GSL_SUCCESS;
            d_results = fitGslMultifit(iterations, status);
            const std::vector<double> yfit_t = d_y_fit;

            d_x = savedX;
            d_y = savedY;
            d_y_errors = savedYErr;
            d_raw_weights = savedRawW;
            d_raw_y_errors = savedRawYErr;
            d_weighting_method = savedWMethod;
            d_y_error_source = savedErrSrc;
            d_app_log_x = savedAppLogX;
            d_app_log_y = savedAppLogY;

            if (status == GSL_SUCCESS && std::isfinite(chi_2) && chi_2 >= 0.0
                && yfit_t.size() == logData.xt.size()) {
                d_has_custom_dof = true;
                d_custom_n_points = int(logData.xt.size());
                d_custom_p_free = effFree;
                d_has_custom_r2 = true;
                d_custom_r2 = computeTransformedR2(logData.yt, yfit_t);
                populateApparentLogFitOutputs(logData, d_y, d_app_log_y, yfit_t, d_y_fit,
                                              d_has_custom_rss, d_custom_rss);
            } else {
                chi_2 = -1.0;
                d_y_fit.clear();
                d_results.assign(d_p, std::numeric_limits<double>::quiet_NaN());
                return;
            }
        } else {
            d_has_custom_dof = true;
            d_custom_n_points = int(logData.xt.size());
            d_custom_p_free = int(d_p);

            std::vector<double> yfit_t;
            CenteredPolyCache cache;
            solveCenteredWeightedPolynomial(logData.xt, logData.yt, logData.wt, d_p, d_results,
                                            covar, chi_2, &yfit_t, &cache);
            cache.apparent = true;
            storeCenteredPolyCache(this, cache);

            d_has_custom_r2 = true;
            d_custom_r2 = computeTransformedR2(logData.yt, yfit_t);
            populateApparentLogFitOutputs(logData, d_y, d_app_log_y, yfit_t, d_y_fit,
                                          d_has_custom_rss, d_custom_rss);
        }
    } else if (hasActiveConstraints()) {
        const std::vector<double> weightsVec = computeWeightsVector();
        // Seed unconstrained centered WLS estimates into non-explicit parameters first
        if (nActive >= int(d_p)) {
            std::vector<double> seedC;
            double tmpChi2 = 0.0;
            if (solveCenteredWeightedPolynomial(d_x, d_y, weightsVec, d_p, seedC, nullptr, tmpChi2,
                                                nullptr, nullptr)) {
                for (unsigned j = 0; j < d_p && j < seedC.size(); ++j) {
                    if (!d_all_guesses_set && !d_explicit_init_vals.contains(int(j)))
                        gsl_vector_set(d_param_init, j, seedC[j]);
                }
            }
        }
        applyExplicitInitialGuesses();
        int iterations = 0, status = GSL_SUCCESS;
        d_results = fitGslMultifit(iterations, status);
        if (d_p == 2) {
            std::vector<double> xAct, yAct;
            collectActiveXY(d_x, d_y, weightsVec, xAct, yAct);
            setStraightLinePearson(xAct, yAct);
        }
    } else {
        const std::vector<double> weightsVec = computeWeightsVector();
        applyWeightsToErrorVector(weightsVec);
        if (d_p == 2) {
            std::vector<double> xAct, yAct;
            collectActiveXY(d_x, d_y, weightsVec, xAct, yAct);
            setStraightLinePearson(xAct, yAct);
        }

        CenteredPolyCache cache;
        solveCenteredWeightedPolynomial(d_x, d_y, weightsVec, d_p, d_results, covar, chi_2,
                                        &d_y_fit, &cache);
        cache.apparent = false;
        storeCenteredPolyCache(this, cache);
    }

    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (app && app->writeFitResultsToLog && !d_silent && d_graph)
        app->updateLog(logFitInfo(d_results, 0, 0, d_graph->parentPlotName()));

    if ((show_legend || (app && app->pasteFitResultsToPlot)) && !d_suppress_curve_output && d_graph)
        showLegend();

    if (!d_suppress_curve_output && d_graph) {
        generateFitCurve(d_results);
        if (d_conf_band || d_pred_band)
            addConfidenceAndPredictionBands(d_results);
    }
}

QString PolynomialFit::legendInfo()
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    QString legend = "";
    if (show_legend) {
        legend = "Y=" + QLocale().toString(d_results[0], 'g', d_prec);
        for (unsigned j = 1; j < d_p; j++) {
            double cj = d_results[j];
            if (cj > 0 && !legend.isEmpty())
                legend += "+";

            QString s;
            s.asprintf("%.5f", cj);
            if (s != "1.00000")
                legend += QLocale().toString(cj, 'g', d_prec);

            legend += "X";
            if (j > 1)
                legend += "<sup>" + QString::number(j) + "</sup>";
        }
    }
    if (app->pasteFitResultsToPlot) {
        if (show_legend) {
            legend += "\n";
        }
        legend += Fit::legendInfo();
    }
    return legend;
}

/*****************************************************************************
 *
 * Class LinearFit
 *
 *****************************************************************************/

LinearFit::LinearFit(ApplicationWindow *parent, Graph *g) : Fit(parent, g)
{
    init();
}

LinearFit::LinearFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle);
}

LinearFit::LinearFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
                     double end)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

LinearFit::~LinearFit()
{
    eraseCenteredPolyCache(this);
    if (!is_non_linear && d_param_init) {
        gsl_vector_free(d_param_init);
        d_param_init = nullptr;
    }
}

void LinearFit::init()
{
    d_p = 2;
    d_min_points = d_p;

    d_f = poly_model_f;
    d_df = poly_model_df;
    d_fdf = poly_model_fdf;
    d_fsimplex = poly_model_d;

    d_param_init = gsl_vector_alloc(d_p);
    gsl_vector_set_all(d_param_init, 1.0);
    covar = gsl_matrix_alloc(d_p, d_p);
    d_results.resize(d_p);

    is_non_linear = false;
    d_formula = "A*x+B";
    d_param_names << "B"
                  << "A";
    d_param_explain << tr("(y-intercept)") << tr("(slope)");
    d_explanation = tr("Linear Regression");
    setObjectName(tr("Linear"));
}

void LinearFit::fit()
{
    if (d_init_err)
        return;

    eraseCenteredPolyCache(this);
    d_result_errors.clear();
    d_has_custom_r2 = false;
    d_has_custom_rss = false;
    d_custom_rss = 0.0;
    d_has_custom_dof = false;
    d_custom_n_points = 0;
    d_custom_p_free = 0;
    d_straight_line_fit = false;
    d_convergence = Converged;
    d_fit_notes.clear();

    const int minNeeded = std::max(1, effectiveFreeParameters());
    const int nActive = effectivePointCount();
    if (minNeeded > nActive) {
        chi_2 = -1.0;
        d_y_fit.clear();
        d_results.assign(d_p, std::numeric_limits<double>::quiet_NaN());
        if (!isNonInteractivePolyFit(parent(), d_silent))
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Fit Error"),
                    tr("You need at least %1 valid data points (with positive weight) for this fit operation. "
                       "Operation aborted!")
                            .arg(minNeeded));
        return;
    }

    int xAx = d_curve ? d_curve->xAxis() : QwtPlot::xBottom;
    int yAx = d_curve ? d_curve->yAxis() : QwtPlot::yLeft;
    d_app_log_x = d_apparent_fit && isGraphAxisLog10(d_graph, xAx);
    d_app_log_y = d_apparent_fit && isGraphAxisLog10(d_graph, yAx);
    if (d_apparent_fit && !d_app_log_x && !d_app_log_y)
        d_fit_notes << tr("Apparent linear fit: no axis is logarithmic, so an ordinary linear fit "
                          "was performed.");

    if (d_apparent_fit && (d_app_log_x || d_app_log_y)) {
        const bool isWeighted =
                !(d_y_error_source == UnknownErrors && d_weighting_method == WeightNone);
        const std::vector<double> baseW = isWeighted ? computeWeightsVector() : std::vector<double>();
        const ApparentLogData logData =
                prepareApparentLogData(d_x, d_y, d_app_log_x, d_app_log_y, isWeighted, baseW);
        if (logData.excludedLogCount > 0)
            d_fit_notes << tr("%1 of %2 points with non-positive values were excluded from the "
                              "log-space fit.")
                                   .arg(logData.excludedLogCount)
                                   .arg(int(d_x.size()));
        if (logData.excludedWeightCount > 0)
            d_fit_notes << tr("%1 of %2 points with non-positive or invalid weights were excluded "
                              "from the log-space fit.")
                                   .arg(logData.excludedWeightCount)
                                   .arg(int(d_x.size()));
        const int effFree = effectiveFreeParameters();
        const int minLogNeeded = std::max(1, effFree);
        if (logData.xt.size() < size_t(minLogNeeded)) {
            chi_2 = -1.0;
            d_y_fit.clear();
            d_results.assign(d_p, std::numeric_limits<double>::quiet_NaN());
            reportApparentFitShortage(this, (ApplicationWindow *)parent(), d_silent, minLogNeeded,
                                      int(logData.xt.size()));
            return;
        }
        setStraightLinePearson(logData.xt, logData.yt);

        if (hasActiveConstraints()) {
            // Honor fixed parameters, bounds, and linear constraints inside apparent log-space fit
            if (logData.xt.size() >= 2) {
                double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0, tmpChi2 = 0.0;
                gsl_fit_wlinear(logData.xt.data(), 1, logData.wt.data(), 1, logData.yt.data(), 1,
                                logData.xt.size(), &c0, &c1, &cov00, &cov01, &cov11, &tmpChi2);
                if (!d_all_guesses_set && !d_explicit_init_vals.contains(0))
                    gsl_vector_set(d_param_init, 0, c0);
                if (!d_all_guesses_set && !d_explicit_init_vals.contains(1))
                    gsl_vector_set(d_param_init, 1, c1);
            }
            applyExplicitInitialGuesses();

            const std::vector<double> savedX = d_x;
            const std::vector<double> savedY = d_y;
            const std::vector<double> savedYErr = d_y_errors;
            const std::vector<double> savedRawW = d_raw_weights;
            const std::vector<double> savedRawYErr = d_raw_y_errors;
            const WeightingMethod savedWMethod = d_weighting_method;
            const ErrorSource savedErrSrc = d_y_error_source;
            const bool savedAppLogX = d_app_log_x;
            const bool savedAppLogY = d_app_log_y;

            d_x = logData.xt;
            d_y = logData.yt;
            d_app_log_x = false;
            d_app_log_y = false;
            if (isWeighted) {
                d_weighting_method = WeightDirect;
                d_y_error_source = CustomErrors;
                d_raw_weights = logData.wt;
                d_raw_y_errors.clear();
                applyWeightsToErrorVector(logData.wt);
            } else {
                d_weighting_method = WeightNone;
                d_y_error_source = UnknownErrors;
                d_raw_weights.clear();
                d_raw_y_errors.clear();
                d_y_errors.assign(logData.xt.size(), 1.0);
            }

            int iterations = 0, status = GSL_SUCCESS;
            d_results = fitGslMultifit(iterations, status);
            const std::vector<double> yfit_t = d_y_fit;

            d_x = savedX;
            d_y = savedY;
            d_y_errors = savedYErr;
            d_raw_weights = savedRawW;
            d_raw_y_errors = savedRawYErr;
            d_weighting_method = savedWMethod;
            d_y_error_source = savedErrSrc;
            d_app_log_x = savedAppLogX;
            d_app_log_y = savedAppLogY;

            if (status == GSL_SUCCESS && std::isfinite(chi_2) && chi_2 >= 0.0
                && yfit_t.size() == logData.xt.size()) {
                d_has_custom_dof = true;
                d_custom_n_points = int(logData.xt.size());
                d_custom_p_free = effFree;
                d_has_custom_r2 = true;
                d_custom_r2 = computeTransformedR2(logData.yt, yfit_t);
                populateApparentLogFitOutputs(logData, d_y, d_app_log_y, yfit_t, d_y_fit,
                                              d_has_custom_rss, d_custom_rss);
            } else {
                chi_2 = -1.0;
                d_y_fit.clear();
                d_results.assign(d_p, std::numeric_limits<double>::quiet_NaN());
                return;
            }
        } else {
            d_has_custom_dof = true;
            d_custom_n_points = int(logData.xt.size());
            d_custom_p_free = 2;

            double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0;
            gsl_fit_wlinear(logData.xt.data(), 1, logData.wt.data(), 1, logData.yt.data(), 1,
                            logData.xt.size(), &c0, &c1, &cov00, &cov01, &cov11, &chi_2);

            d_results[0] = c0;
            d_results[1] = c1;
            gsl_matrix_set(covar, 0, 0, cov00);
            gsl_matrix_set(covar, 0, 1, cov01);
            gsl_matrix_set(covar, 1, 1, cov11);
            gsl_matrix_set(covar, 1, 0, cov01);

            double wmX = 0.0, wmY = 0.0;
            const bool hasMean = computeWeightedMeanXY(logData.xt, logData.yt, logData.wt, wmX, wmY)
                    && std::isfinite(c0) && std::isfinite(c1);
            if (hasMean) {
                CenteredPolyCache cache;
                cache.xMean = static_cast<long double>(wmX);
                cache.xScale = 1.0L;
                cache.cU = { wmY, c1 };
                cache.coeffsX = d_results;
                cache.apparent = true;
                storeCenteredPolyCache(this, cache);
            }

            std::vector<double> yfit_t(logData.xt.size());
            if (hasMean) {
                for (size_t i = 0; i < logData.xt.size(); ++i)
                    yfit_t[i] = wmY + c1 * (logData.xt[i] - wmX);
            } else {
                for (size_t i = 0; i < logData.xt.size(); ++i)
                    yfit_t[i] = evalPolyHornerLD(d_results, 2, logData.xt[i]);
            }
            d_has_custom_r2 = true;
            d_custom_r2 = computeTransformedR2(logData.yt, yfit_t);
            populateApparentLogFitOutputs(logData, d_y, d_app_log_y, yfit_t, d_y_fit,
                                          d_has_custom_rss, d_custom_rss);
        }
    } else if (hasActiveConstraints()) {
        const std::vector<double> weights = computeWeightsVector();
        if (nActive >= 2) {
            double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0, tmpChi2 = 0.0;
            gsl_fit_wlinear(d_x.data(), 1, weights.data(), 1, d_y.data(), 1, d_x.size(), &c0, &c1,
                            &cov00, &cov01, &cov11, &tmpChi2);
            if (!d_all_guesses_set && !d_explicit_init_vals.contains(0))
                gsl_vector_set(d_param_init, 0, c0);
            if (!d_all_guesses_set && !d_explicit_init_vals.contains(1))
                gsl_vector_set(d_param_init, 1, c1);
        }
        applyExplicitInitialGuesses();
        int iterations = 0, status = GSL_SUCCESS;
        d_results = fitGslMultifit(iterations, status);
        std::vector<double> xAct, yAct;
        collectActiveXY(d_x, d_y, weights, xAct, yAct);
        setStraightLinePearson(xAct, yAct);
    } else {
        double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0;
        const size_t n = d_x.size();
        const std::vector<double> weights = computeWeightsVector();
        applyWeightsToErrorVector(weights);
        std::vector<double> xAct, yAct;
        collectActiveXY(d_x, d_y, weights, xAct, yAct);
        setStraightLinePearson(xAct, yAct);

        gsl_fit_wlinear(d_x.data(), 1, weights.data(), 1, d_y.data(), 1, n, &c0, &c1, &cov00,
                        &cov01, &cov11, &chi_2);

        d_results[0] = c0;
        d_results[1] = c1;

        gsl_matrix_set(covar, 0, 0, cov00);
        gsl_matrix_set(covar, 0, 1, cov01);
        gsl_matrix_set(covar, 1, 1, cov11);
        gsl_matrix_set(covar, 1, 0, cov01);

        double wmX = 0.0, wmY = 0.0;
        const bool hasMean = computeWeightedMeanXY(d_x, d_y, weights, wmX, wmY)
                && std::isfinite(c0) && std::isfinite(c1);
        if (hasMean) {
            CenteredPolyCache cache;
            cache.xMean = static_cast<long double>(wmX);
            cache.xScale = 1.0L;
            cache.cU = { wmY, c1 };
            cache.coeffsX = d_results;
            cache.apparent = false;
            storeCenteredPolyCache(this, cache);
        }

        d_y_fit.resize(d_x.size());
        if (hasMean) {
            for (size_t i = 0; i < d_x.size(); ++i)
                d_y_fit[i] = wmY + c1 * (d_x[i] - wmX);
        } else {
            for (size_t i = 0; i < d_x.size(); ++i)
                d_y_fit[i] = evalPolyHornerLD(d_results, 2, d_x[i]);
        }
    }

    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (app && app->writeFitResultsToLog && !d_silent && d_graph)
        app->updateLog(logFitInfo(d_results, 0, 0, d_graph->parentPlotName()));

    if (!d_suppress_curve_output && d_graph) {
        generateFitCurve(d_results);
        if (d_conf_band || d_pred_band)
            addConfidenceAndPredictionBands(d_results);
    }
}

bool LinearFit::fitWithXYErrors(const QString &xErrColName, const QString &yErrColName)
{
    if (d_init_err || d_x.size() < 2 || d_y.size() != d_x.size())
        return false;

    eraseCenteredPolyCache(this);

    const size_t n = d_x.size();
    ApplicationWindow *app = (ApplicationWindow *)parent();
    DataCurve *dc = dynamic_cast<DataCurve *>(d_curve);

    // As in Fit::computeWeightsVector(): a missing or non-finite error (e.g. an empty error cell)
    // does not get a made-up sigma; the point is left out of the fit instead (NaN marks it).
    // A zero error is a valid value here: it says that coordinate is exact, and York's weight
    // W = 1 / (sy^2 + b^2 sx^2) stays finite as long as the other axis has an error. Only points
    // with both errors zero are left out (see below).
    auto validSigma = [](double v) -> double {
        return (std::isfinite(v) && v >= 0.0 && v < 1e140) ? v
                                                            : std::numeric_limits<double>::quiet_NaN();
    };

    // Reads an error cell without Table::cell(), which turns an empty or non-numeric cell into
    // 0.0 (= "exact" here). Only a real number counts; anything else gives NaN, so the point is
    // left out: an invalid cell, a Text cell that does not parse ("n/a", ""), or a column type
    // that holds no numbers (date/time).
    auto readErrorCell = [](Column *col, int row) -> double {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        if (!col || row < 0 || row >= col->rowCount() || col->isInvalid(row))
            return nan;
        if (col->columnMode() == SciDAVis::ColumnMode::Text) {
            const QString text = col->textAt(row).trimmed();
            if (text.isEmpty())
                return nan;
            bool ok = false;
            double v = QLocale().toDouble(text, &ok);
            if (!ok)
                v = QLocale::c().toDouble(text, &ok);
            return ok ? v : nan;
        }
        if (col->dataType() == SciDAVis::TypeDouble)
            return col->valueAt(row);
        return nan;
    };

    auto resolveErrors = [&](const QString &colSpec, bool wantXErr, std::vector<double> &sigmaOut,
                             bool &explicitFound) {
        sigmaOut.assign(n, 1.0);
        explicitFound = false;

        if (!colSpec.trimmed().isEmpty()) {
            QString spec = colSpec.trimmed();
            Table *tbl = nullptr;
            int colIdx = -1;

            if (dc && dc->table()) {
                int ci = dc->table()->colIndex(spec);
                if (ci >= 0) {
                    tbl = dc->table();
                    colIdx = ci;
                } else if (Column *colPtr = dc->table()->column(spec)) {
                    tbl = dc->table();
                    for (int k = 0; k < tbl->numCols(); ++k) {
                        if (tbl->column(k) == colPtr) {
                            colIdx = k;
                            break;
                        }
                    }
                }
            }
            if ((!tbl || colIdx < 0) && app) {
                if (Table *tByName = app->table(spec)) {
                    int ci = tByName->colIndex(spec);
                    if (ci >= 0) {
                        tbl = tByName;
                        colIdx = ci;
                    }
                }
            }
            if ((!tbl || colIdx < 0) && app && app->tableList()) {
                for (MyWidget *w : *app->tableList()) {
                    Table *cand = qobject_cast<Table *>(w);
                    if (!cand)
                        continue;
                    int ci = cand->colIndex(spec);
                    if (ci < 0) {
                        if (Column *colPtr = cand->column(spec)) {
                            for (int k = 0; k < cand->numCols(); ++k) {
                                if (cand->column(k) == colPtr) {
                                    ci = k;
                                    break;
                                }
                            }
                        }
                    }
                    if (ci >= 0) {
                        tbl = cand;
                        colIdx = ci;
                        break;
                    }
                }
            }

            if (tbl && colIdx >= 0) {
                bool anyPos = false;
                Column *errCol = tbl->column(colIdx);
                for (size_t i = 0; i < n; ++i) {
                    int row = int(d_indices[i]);
                    if (dc && dc->table() == tbl) {
                        int trRow = dc->tableRow(int(d_indices[i]));
                        if (trRow >= 0)
                            row = trRow;
                    }
                    // Table::cell() reads an empty or non-numeric cell as 0, which here means
                    // "exact": such a cell is an unusable error instead (NaN, point left out).
                    double v = row < tbl->numRows() ? std::abs(readErrorCell(errCol, row))
                                                    : std::numeric_limits<double>::quiet_NaN();
                    if (std::isfinite(v) && v > 0.0)
                        anyPos = true;
                    sigmaOut[i] = validSigma(v);
                }
                if (anyPos) {
                    explicitFound = true;
                    return;
                }
                sigmaOut.assign(n, 1.0);
            }
        }

        if (dc) {
            const QList<DataCurve *> errBars = dc->errorBarsList();
            for (DataCurve *ec : errBars) {
                QwtErrorPlotCurve *er = static_cast<QwtErrorPlotCurve *>(ec);
                if (er && er->xErrors() == wantXErr) {
                    bool anyPos = false;
                    for (size_t i = 0; i < n; ++i) {
                        const int pt = int(d_indices[i]);
                        double v = (pt >= 0 && pt < er->dataSize())
                                ? std::abs(er->errorValue(pt))
                                : std::numeric_limits<double>::quiet_NaN();
                        if (std::isfinite(v) && v > 0.0)
                            anyPos = true;
                        sigmaOut[i] = validSigma(v);
                    }
                    if (anyPos) {
                        explicitFound = true;
                        return;
                    }
                    sigmaOut.assign(n, 1.0);
                }
            }
        }
    };

    std::vector<double> sx, sy;
    bool hasExplicitX = false, hasExplicitY = false;
    resolveErrors(xErrColName, true, sx, hasExplicitX);
    resolveErrors(yErrColName, false, sy, hasExplicitY);

    // Points without a usable error are not part of the fit (see validSigma above), nor are
    // points with both errors zero (no finite York weight). The solver works with the error
    // variances vx = sx^2, vy = sy^2 instead of the weights wx = 1/vx, wy = 1/vy, so that an
    // exact coordinate (variance 0) enters as the limit of York's formulas:
    //   W = wx wy / (wx + b^2 wy) = 1 / (vy + b^2 vx),
    //   beta = W (U / wy + b V / wx) = W (U vy + b V vx).
    std::vector<double> xs, ys, vx, vy;
    xs.reserve(n);
    ys.reserve(n);
    vx.reserve(n);
    vy.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(sx[i]) || !std::isfinite(sy[i]))
            continue;
        if (!(sx[i] > 0.0) && !(sy[i] > 0.0))
            continue;
        xs.push_back(d_x[i]);
        ys.push_back(d_y[i]);
        vx.push_back(sx[i] * sx[i]);
        vy.push_back(sy[i] * sy[i]);
    }
    const size_t m = xs.size();
    if (m < 2)
        return false;

    // York weights at slope b. 1 / (vy + b^2 vx) is infinite only for an exact Y (vy = 0) at
    // b = 0 (or when the variances underflow): such a point gets a weight far above every other
    // one, i.e. the line is pulled through it, but the sums stay finite.
    auto yorkWeights = [&](double slope, std::vector<double> &Wout) {
        double maxW = 0.0;
        bool anyInf = false;
        for (size_t i = 0; i < m; ++i) {
            const double denom = vy[i] + slope * slope * vx[i];
            const double wi = (denom > 0.0) ? (1.0 / denom) : 0.0;
            if (denom > 0.0 && std::isfinite(wi)) {
                Wout[i] = wi;
                maxW = std::max(maxW, wi);
            } else {
                Wout[i] = -1.0;
                anyInf = true;
            }
        }
        if (anyInf) {
            const double big = (maxW > 0.0) ? std::min(maxW * 1e12, 1e300) : 1.0;
            for (size_t i = 0; i < m; ++i) {
                if (Wout[i] < 0.0)
                    Wout[i] = big;
            }
        }
    };

    // Orthogonal / Deming initial slope estimate
    double mx0 = std::accumulate(xs.begin(), xs.end(), 0.0) / double(m);
    double my0 = std::accumulate(ys.begin(), ys.end(), 0.0) / double(m);
    double sxx = 0.0, syy = 0.0, sxy = 0.0;
    for (size_t i = 0; i < m; ++i) {
        double dx = xs[i] - mx0;
        double dy = ys[i] - my0;
        sxx += dx * dx;
        syy += dy * dy;
        sxy += dx * dy;
    }
    double b = 0.0;
    if (std::abs(sxy) > 1e-30)
        b = ((syy - sxx) + std::sqrt((syy - sxx) * (syy - sxx) + 4.0 * sxy * sxy)) / (2.0 * sxy);
    else if (sxx > 1e-30)
        b = sxy / sxx;

    std::vector<double> W(m), U(m), V(m), beta(m);
    double Xbar = mx0, Ybar = my0;

    // York (1966 / 2004) exact iterative total least-squares solver
    const int maxIter = 500;
    for (int iter = 0; iter < maxIter; ++iter) {
        double sumW = 0.0, sumWX = 0.0, sumWY = 0.0;
        yorkWeights(b, W);
        for (size_t i = 0; i < m; ++i) {
            sumW += W[i];
            sumWX += W[i] * xs[i];
            sumWY += W[i] * ys[i];
        }
        if (sumW <= 0.0)
            return false;

        Xbar = sumWX / sumW;
        Ybar = sumWY / sumW;

        double num = 0.0, den = 0.0;
        for (size_t i = 0; i < m; ++i) {
            U[i] = xs[i] - Xbar;
            V[i] = ys[i] - Ybar;
            beta[i] = W[i] * (U[i] * vy[i] + b * V[i] * vx[i]);
            num += W[i] * beta[i] * V[i];
            den += W[i] * beta[i] * U[i];
        }
        if (std::abs(den) <= 1e-300)
            break;

        double b_new = num / den;
        if (!std::isfinite(b_new))
            break;
        if (std::abs(b_new - b) <= 1e-13 * (1.0 + std::abs(b_new))) {
            b = b_new;
            break;
        }
        b = b_new;
    }

    // Final pass at converged slope b for exact York (2004) intercept & standard errors
    double sumW = 0.0, sumWX = 0.0, sumWY = 0.0;
    yorkWeights(b, W);
    for (size_t i = 0; i < m; ++i) {
        sumW += W[i];
        sumWX += W[i] * xs[i];
        sumWY += W[i] * ys[i];
    }
    if (sumW <= 0.0)
        return false;

    Xbar = sumWX / sumW;
    Ybar = sumWY / sumW;
    double a = Ybar - b * Xbar;

    std::vector<double> x_adj(m);
    double sumWxAdj = 0.0;
    for (size_t i = 0; i < m; ++i) {
        U[i] = xs[i] - Xbar;
        V[i] = ys[i] - Ybar;
        beta[i] = W[i] * (U[i] * vy[i] + b * V[i] * vx[i]);
        x_adj[i] = Xbar + beta[i];
        sumWxAdj += W[i] * x_adj[i];
    }
    double x_adj_bar = sumWxAdj / sumW;

    double sumWu2 = 0.0;
    double S = 0.0;
    double S_tot = 0.0;
    for (size_t i = 0; i < m; ++i) {
        double ui = x_adj[i] - x_adj_bar;
        sumWu2 += W[i] * ui * ui;
        double resid = ys[i] - b * xs[i] - a;
        S += W[i] * resid * resid;
        double dyBar = ys[i] - Ybar;
        S_tot += W[i] * dyBar * dyBar;
    }

    const double nanVal = std::numeric_limits<double>::quiet_NaN();
    double var_b = (sumWu2 > 1e-300) ? (1.0 / sumWu2) : nanVal;
    double var_a = std::isfinite(var_b) ? ((1.0 / sumW) + x_adj_bar * x_adj_bar * var_b) : nanVal;
    double cov_ab = std::isfinite(var_b) ? (-x_adj_bar * var_b) : nanVal;

    bool usedFallbackUnitErrors = (!hasExplicitX && !hasExplicitY);
    d_y_error_source = usedFallbackUnitErrors ? UnknownErrors : CustomErrors;
    d_weighting_method = usedFallbackUnitErrors ? WeightNone : WeightInstrumental;

    gsl_matrix_set(covar, 0, 0, var_a);
    gsl_matrix_set(covar, 0, 1, cov_ab);
    gsl_matrix_set(covar, 1, 0, cov_ab);
    gsl_matrix_set(covar, 1, 1, var_b);

    chi_2 = S;
    // Left-out points do not count towards the degrees of freedom either.
    d_has_custom_dof = (m < n);
    if (d_has_custom_dof) {
        d_custom_n_points = int(m);
        d_custom_p_free = 2;
    }
    const double covScale = effectiveCovarScale();

    d_york_slope = b;
    d_york_intercept = a;
    const double scaledVarB = covScale * var_b;
    const double scaledVarA = covScale * var_a;
    d_york_slope_err =
            (std::isfinite(scaledVarB) && scaledVarB >= 0.0) ? std::sqrt(scaledVarB) : nanVal;
    d_york_intercept_err =
            (std::isfinite(scaledVarA) && scaledVarA >= 0.0) ? std::sqrt(scaledVarA) : nanVal;

    d_app_log_x = false;
    d_app_log_y = false;
    d_has_custom_rss = false;
    d_custom_rss = 0.0;
    d_convergence = Converged;
    d_fit_notes.clear();
    setStraightLinePearson(xs, ys);
    d_results.resize(2);
    d_results[0] = a;
    d_results[1] = b;
    d_result_errors = { d_york_intercept_err, d_york_slope_err };

    d_has_custom_r2 = true;
    d_custom_r2 = (S_tot > 1e-300) ? (1.0 - S / S_tot) : ((S <= 1e-300) ? 1.0 : nanVal);

    d_y_fit.resize(n);
    for (size_t i = 0; i < n; ++i)
        d_y_fit[i] = a + b * d_x[i];

    if (app && app->writeFitResultsToLog && !d_silent && d_graph)
        app->updateLog(logFitInfo(d_results, 0, 0, d_graph->parentPlotName()));

    if (!d_suppress_curve_output && d_graph) {
        generateFitCurve(d_results);
        if (d_conf_band || d_pred_band)
            addConfidenceAndPredictionBands(d_results);
    }

    return true;
}

bool LinearFit::calculateFitCurveData(const vector<double> &par, std::vector<double> &X,
                                      std::vector<double> &Y)
{
    if (par.size() < 2)
        return false;

    CenteredPolyCache cache;
    const bool hasCache = loadCenteredPolyCache(this, cache);
    const bool isApparent = d_apparent_fit && (d_app_log_x || d_app_log_y);
    const CenteredPolyCache *useCache =
            (hasCache && cache.apparent == isApparent && cache.coeffsX == d_results) ? &cache
                                                                                     : nullptr;

    if (isApparent) {
        if (!tryGenerateApparentLogX(d_x, d_gen_function, d_app_log_x, d_points, X))
            generateX(X);
        evaluateApparentPolyY(d_app_log_x, d_app_log_y, d_points, 2, par, X, Y, useCache);
        return true;
    }

    generateX(X);
    for (int i = 0; i < d_points; i++) {
        double yi = 0.0;
        if (!useCache || !evalWithCenteredCacheIfApplicable(*useCache, par, 2, X[i], yi))
            yi = evalPolyHornerLD(par, 2, X[i]);
        Y[i] = yi;
    }
    return true;
}
