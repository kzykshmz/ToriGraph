/***************************************************************************
    File                 : fitclasses.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Exponential fit classes

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
#include "ExponentialFit.h"
#include "fit_gsl.h"
#include <cmath>
#include <limits>
using namespace std;

namespace {

constexpr double kZeroRateEps = 1e-300;

inline double safeReciprocal(double val, double sign)
{
    if (!std::isfinite(val) || std::abs(val) < kZeroRateEps)
        return std::numeric_limits<double>::quiet_NaN();
    const double res = sign / val;
    return std::isfinite(res) ? res : std::numeric_limits<double>::quiet_NaN();
}

inline double safeReciprocalDeriv(double solverVal, double sign)
{
    // For stored = sign / solverVal, d(stored)/d(solverVal) = -sign / solverVal^2.
    if (!std::isfinite(solverVal) || std::abs(solverVal) < kZeroRateEps)
        return std::numeric_limits<double>::quiet_NaN();
    const double denom = solverVal * solverVal;
    if (!std::isfinite(denom) || !(denom > 0.0))
        return std::numeric_limits<double>::quiet_NaN();
    const double res = (-sign) / denom;
    return std::isfinite(res) ? res : std::numeric_limits<double>::quiet_NaN();
}

inline bool allFinitePrefix(const std::vector<double> &par, unsigned count)
{
    if (count == 0 || par.size() < count)
        return false;
    for (unsigned i = 0; i < count; ++i) {
        if (!std::isfinite(par[i]))
            return false;
    }
    return true;
}

inline void scaleCovarianceMatrix(gsl_matrix *covar, unsigned d_p, const std::vector<double> &scale)
{
    if (!covar || covar->size1 < d_p || covar->size2 < d_p || scale.size() < d_p)
        return;
    bool anyScaled = false;
    for (unsigned i = 0; i < d_p; ++i) {
        if (scale[i] != 1.0) {
            anyScaled = true;
            break;
        }
    }
    if (!anyScaled)
        return;
    for (unsigned a = 0; a < d_p; ++a) {
        for (unsigned b = 0; b < d_p; ++b) {
            gsl_matrix_set(covar, a, b, gsl_matrix_get(covar, a, b) * scale[a] * scale[b]);
        }
    }
}

} // namespace

/*****************************************************************************
 *
 * Class ExponentialFit
 *
 *****************************************************************************/

ExponentialFit::ExponentialFit(ApplicationWindow *parent, Graph *g, bool expGrowth)
    : Fit(parent, g), is_exp_growth(expGrowth)
{
    init();
}

ExponentialFit::ExponentialFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle,
                               bool expGrowth)
    : Fit(parent, g), is_exp_growth(expGrowth)
{
    init();
    setDataFromCurve(curveTitle);
}

ExponentialFit::ExponentialFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle,
                               double start, double end, bool expGrowth)
    : Fit(parent, g), is_exp_growth(expGrowth)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

void ExponentialFit::init()
{
    d_f = exp_f;
    d_df = exp_df;
    d_fdf = exp_fdf;
    d_fsimplex = exp_d;
    d_p = 3;
    d_min_points = d_p;
    d_param_init = gsl_vector_alloc(d_p);
    gsl_vector_set_all(d_param_init, 1.0);
    if (is_exp_growth)
        gsl_vector_set(d_param_init, 1, -1.0);

    covar = gsl_matrix_alloc(d_p, d_p);
    d_results.resize(d_p);
    d_param_names << "A"
                  << "t"
                  << "y0";

    if (is_exp_growth) {
        setObjectName("ExpGrowth");
        d_explanation = tr("Exponential growth");
        d_formula = "y0+A*exp(x/t)";
        d_param_explain << tr("(amplitude)") << tr("(lifetime)") << tr("(offset)");
    } else {
        setObjectName("ExpDecay");
        d_explanation = tr("Exponential decay");
        d_formula = "y0+A*exp(-x/t)";
        d_param_explain << tr("(amplitude)") << tr("(e-folding time)") << tr("(offset)");
    }
}

void ExponentialFit::guessInitialValues()
{
    if (!d_param_init || d_x.size() < 2 || d_y.size() != d_x.size())
        return;

    size_t minIdx = 0, maxIdx = 0;
    double xMin = std::numeric_limits<double>::infinity();
    double xMax = -std::numeric_limits<double>::infinity();
    double yMin = std::numeric_limits<double>::infinity();
    double yMax = -std::numeric_limits<double>::infinity();
    bool haveAny = false;
    for (size_t i = 0; i < d_x.size(); ++i) {
        if (!std::isfinite(d_x[i]) || !std::isfinite(d_y[i]))
            continue;
        if (!haveAny || d_x[i] < xMin) {
            xMin = d_x[i];
            minIdx = i;
        }
        if (!haveAny || d_x[i] > xMax) {
            xMax = d_x[i];
            maxIdx = i;
        }
        yMin = std::min(yMin, d_y[i]);
        yMax = std::max(yMax, d_y[i]);
        haveAny = true;
    }
    if (!haveAny)
        return;

    const double xSpan = xMax - xMin;
    const double maxAbsX = std::max(std::abs(xMin), std::abs(xMax));
    double rate = (xSpan > 1e-12) ? (1.0 / xSpan) : 1.0;
    if (maxAbsX > 0.0 && rate * maxAbsX > 20.0)
        rate = 20.0 / maxAbsX;
    if (!std::isfinite(rate) || !(rate > 0.0))
        rate = 1e-3;
    const double lambdaInit = is_exp_growth ? -rate : rate;

    if (!d_explicit_init_vals.contains(1))
        gsl_vector_set(d_param_init, 1, lambdaInit);

    const double yAtMin = d_y[minIdx];
    const double yAtMax = d_y[maxIdx];
    double y0Init = is_exp_growth ? yAtMin : yAtMax;
    double refX = is_exp_growth ? xMax : xMin;
    double expRef = std::exp(-lambdaInit * refX);
    double aInit = (std::isfinite(expRef) && std::abs(expRef) > 1e-30)
            ? ((is_exp_growth ? (yAtMax - y0Init) : (yAtMin - y0Init)) / expRef)
            : (yMax - yMin);
    if (!std::isfinite(aInit) || std::abs(aInit) <= 1e-30)
        aInit = (yMax > yMin) ? (yMax - yMin) : 1.0;
    if (!std::isfinite(y0Init))
        y0Init = 0.0;

    if (!d_explicit_init_vals.contains(0))
        gsl_vector_set(d_param_init, 0, aInit);
    if (!d_explicit_init_vals.contains(2))
        gsl_vector_set(d_param_init, 2, y0Init);
    d_initial_guesses_set = true;
}

double ExponentialFit::toInternalParameter(int index, double userVal) const
{
    if (index == 1)
        return safeReciprocal(userVal, is_exp_growth ? -1.0 : 1.0);
    return userVal;
}

double ExponentialFit::fromInternalParameter(int index, double internalVal) const
{
    if (index == 1)
        return safeReciprocal(internalVal, is_exp_growth ? -1.0 : 1.0);
    return internalVal;
}

double ExponentialFit::covarSolverToStoredScale(int parIndex, double solverVal) const
{
    if (!d_covar_scaled || parIndex != 1)
        return 1.0;
    return safeReciprocalDeriv(solverVal, is_exp_growth ? -1.0 : 1.0);
}

void ExponentialFit::storeCustomFitResults(const vector<double> &par)
{
    d_results = par;
    if (!d_covar_scaled && allFinitePrefix(par, d_p) && covar && covar->size1 >= d_p
        && covar->size2 >= d_p) {
        d_covar_scaled = true;
        std::vector<double> scale(d_p, 1.0);
        for (unsigned i = 0; i < d_p; ++i)
            scale[i] = covarSolverToStoredScale(int(i), par[i]);
        scaleCovarianceMatrix(covar, d_p, scale);
    }

    if (d_results.size() >= 2)
        d_results[1] = fromInternalParameter(1, par[1]);
}

bool ExponentialFit::evaluateModelCurve(const vector<double> &solverPar, std::vector<double> &X,
                                        std::vector<double> &Y)
{
    if (solverPar.size() < 3)
        return false;
    generateX(X);
    for (int i = 0; i < d_points; i++)
        Y[i] = solverPar[0] * exp(-solverPar[1] * X[i]) + solverPar[2];
    return true;
}

bool ExponentialFit::calculateFitCurveData(const vector<double> &par, std::vector<double> &X,
                                           std::vector<double> &Y)
{
    if (par.size() < 3)
        return false;
    const double lambda = toInternalParameter(1, par[1]);
    if (!std::isfinite(lambda))
        return false;
    generateX(X);
    for (int i = 0; i < d_points; i++)
        Y[i] = par[0] * exp(-lambda * X[i]) + par[2];
    return true;
}

/*****************************************************************************
 *
 * Class TwoExpFit
 *
 *****************************************************************************/

TwoExpFit::TwoExpFit(ApplicationWindow *parent, Graph *g) : Fit(parent, g)
{
    init();
}

TwoExpFit::TwoExpFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle);
}

TwoExpFit::TwoExpFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
                     double end)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

void TwoExpFit::init()
{
    setObjectName("ExpDecay");
    d_f = expd2_f;
    d_df = expd2_df;
    d_fdf = expd2_fdf;
    d_fsimplex = expd2_d;
    d_p = 5;
    d_min_points = d_p;
    d_param_init = gsl_vector_alloc(d_p);
    gsl_vector_set_all(d_param_init, 1.0);
    covar = gsl_matrix_alloc(d_p, d_p);
    d_results.resize(d_p);
    d_param_names << "A1"
                  << "t1"
                  << "A2"
                  << "t2"
                  << "y0";
    d_explanation = tr("Exponential decay");
    d_formula = "A1*exp(-x/t1)+A2*exp(-x/t2)+y0";
    d_param_explain << tr("(first amplitude)") << tr("(first lifetime)") << tr("(second amplitude)")
                    << tr("(second lifetime)") << tr("(offset)");
}

double TwoExpFit::toInternalParameter(int index, double userVal) const
{
    if (index == 1 || index == 3)
        return safeReciprocal(userVal, 1.0);
    return userVal;
}

double TwoExpFit::fromInternalParameter(int index, double internalVal) const
{
    if (index == 1 || index == 3)
        return safeReciprocal(internalVal, 1.0);
    return internalVal;
}

double TwoExpFit::covarSolverToStoredScale(int parIndex, double solverVal) const
{
    if (!d_covar_scaled || (parIndex != 1 && parIndex != 3))
        return 1.0;
    return safeReciprocalDeriv(solverVal, 1.0);
}

void TwoExpFit::storeCustomFitResults(const vector<double> &par)
{
    d_results = par;
    if (!d_covar_scaled && allFinitePrefix(par, d_p) && covar && covar->size1 >= d_p
        && covar->size2 >= d_p) {
        d_covar_scaled = true;
        std::vector<double> scale(d_p, 1.0);
        for (unsigned i = 0; i < d_p; ++i)
            scale[i] = covarSolverToStoredScale(int(i), par[i]);
        scaleCovarianceMatrix(covar, d_p, scale);
    }

    if (d_results.size() > 3) {
        d_results[1] = fromInternalParameter(1, par[1]);
        d_results[3] = fromInternalParameter(3, par[3]);
    }
}

bool TwoExpFit::evaluateModelCurve(const vector<double> &solverPar, std::vector<double> &X,
                                   std::vector<double> &Y)
{
    if (solverPar.size() < 5)
        return false;
    generateX(X);
    for (int i = 0; i < d_points; i++)
        Y[i] = solverPar[0] * exp(-solverPar[1] * X[i]) + solverPar[2] * exp(-solverPar[3] * X[i])
                + solverPar[4];
    return true;
}

bool TwoExpFit::calculateFitCurveData(const vector<double> &par, std::vector<double> &X,
                                      std::vector<double> &Y)
{
    if (par.size() < 5)
        return false;
    const double l1 = toInternalParameter(1, par[1]);
    const double l2 = toInternalParameter(3, par[3]);
    if (!std::isfinite(l1) || !std::isfinite(l2))
        return false;
    generateX(X);
    for (int i = 0; i < d_points; i++)
        Y[i] = par[0] * exp(-l1 * X[i]) + par[2] * exp(-l2 * X[i]) + par[4];
    return true;
}

/*****************************************************************************
 *
 * Class ThreeExpFit
 *
 *****************************************************************************/

ThreeExpFit::ThreeExpFit(ApplicationWindow *parent, Graph *g) : Fit(parent, g)
{
    init();
}

ThreeExpFit::ThreeExpFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle);
}

ThreeExpFit::ThreeExpFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle,
                         double start, double end)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

void ThreeExpFit::init()
{
    setObjectName("ExpDecay");
    d_f = expd3_f;
    d_df = expd3_df;
    d_fdf = expd3_fdf;
    d_fsimplex = expd3_d;
    d_p = 7;
    d_min_points = d_p;
    d_param_init = gsl_vector_alloc(d_p);
    gsl_vector_set_all(d_param_init, 1.0);
    covar = gsl_matrix_alloc(d_p, d_p);
    d_results.resize(d_p);
    d_param_names << "A1"
                  << "t1"
                  << "A2"
                  << "t2"
                  << "A3"
                  << "t3"
                  << "y0";
    d_explanation = tr("Exponential decay");
    d_formula = "A1*exp(-x/t1)+A2*exp(-x/t2)+A3*exp(-x/t3)+y0";
    d_param_explain << tr("(first amplitude)") << tr("(first lifetime)") << tr("(second amplitude)")
                    << tr("(second lifetime)") << tr("(third amplitude)") << tr("(third lifetime)")
                    << tr("(offset)");
}

double ThreeExpFit::toInternalParameter(int index, double userVal) const
{
    if (index == 1 || index == 3 || index == 5)
        return safeReciprocal(userVal, 1.0);
    return userVal;
}

double ThreeExpFit::fromInternalParameter(int index, double internalVal) const
{
    if (index == 1 || index == 3 || index == 5)
        return safeReciprocal(internalVal, 1.0);
    return internalVal;
}

double ThreeExpFit::covarSolverToStoredScale(int parIndex, double solverVal) const
{
    if (!d_covar_scaled || (parIndex != 1 && parIndex != 3 && parIndex != 5))
        return 1.0;
    return safeReciprocalDeriv(solverVal, 1.0);
}

void ThreeExpFit::storeCustomFitResults(const vector<double> &par)
{
    d_results = par;
    if (!d_covar_scaled && allFinitePrefix(par, d_p) && covar && covar->size1 >= d_p
        && covar->size2 >= d_p) {
        d_covar_scaled = true;
        std::vector<double> scale(d_p, 1.0);
        for (unsigned i = 0; i < d_p; ++i)
            scale[i] = covarSolverToStoredScale(int(i), par[i]);
        scaleCovarianceMatrix(covar, d_p, scale);
    }

    if (d_results.size() > 5) {
        d_results[1] = fromInternalParameter(1, par[1]);
        d_results[3] = fromInternalParameter(3, par[3]);
        d_results[5] = fromInternalParameter(5, par[5]);
    }
}

bool ThreeExpFit::evaluateModelCurve(const vector<double> &solverPar, std::vector<double> &X,
                                     std::vector<double> &Y)
{
    if (solverPar.size() < 7)
        return false;
    generateX(X);
    for (int i = 0; i < d_points; i++)
        Y[i] = solverPar[0] * exp(-X[i] * solverPar[1])
                + solverPar[2] * exp(-X[i] * solverPar[3])
                + solverPar[4] * exp(-X[i] * solverPar[5]) + solverPar[6];
    return true;
}

bool ThreeExpFit::calculateFitCurveData(const vector<double> &par, std::vector<double> &X,
                                        std::vector<double> &Y)
{
    if (par.size() < 7)
        return false;
    const double l1 = toInternalParameter(1, par[1]);
    const double l2 = toInternalParameter(3, par[3]);
    const double l3 = toInternalParameter(5, par[5]);
    if (!std::isfinite(l1) || !std::isfinite(l2) || !std::isfinite(l3))
        return false;
    generateX(X);
    for (int i = 0; i < d_points; i++)
        Y[i] = par[0] * exp(-X[i] * l1) + par[2] * exp(-X[i] * l2) + par[4] * exp(-X[i] * l3)
                + par[6];
    return true;
}
