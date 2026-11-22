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
#include <numeric>
using namespace std;

namespace {

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
        double xi = d->X[i];
        double yi = 0.0;
        double xpow = 1.0;
        for (size_t j = 0; j < d->p; ++j) {
            yi += gsl_vector_get(x, j) * xpow;
            xpow *= xi;
        }
        double s = (d->sigma && d->sigma[i] > 0.0) ? d->sigma[i] : 1.0;
        gsl_vector_set(f, i, (yi - d->Y[i]) / s);
    }
    return GSL_SUCCESS;
}

static int poly_model_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    Q_UNUSED(x)
    FitData *d = static_cast<FitData *>(params);
    for (size_t i = 0; i < d->n; ++i) {
        double xi = d->X[i];
        double s = (d->sigma && d->sigma[i] > 0.0) ? d->sigma[i] : 1.0;
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
        double xi = d->X[i];
        double yi = 0.0;
        double xpow = 1.0;
        for (size_t j = 0; j < d->p; ++j) {
            yi += gsl_vector_get(x, j) * xpow;
            xpow *= xi;
        }
        double s = (d->sigma && d->sigma[i] > 0.0) ? d->sigma[i] : 1.0;
        double r = (yi - d->Y[i]) / s;
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
    if (d_apparent_fit && (d_app_log_x || d_app_log_y)) {
        if (d_gen_function && d_app_log_x && !d_x.empty() && d_x.front() > 0.0 && d_x.back() > 0.0) {
            double lx0 = std::log10(d_x.front());
            double lstep = (d_points > 1) ? ((std::log10(d_x.back()) - lx0) / (d_points - 1)) : 0.0;
            for (int i = 0; i < d_points; ++i)
                X[i] = std::pow(10.0, lx0 + i * lstep);
        } else {
            generateX(X);
        }
        for (int i = 0; i < d_points; ++i) {
            double xt = d_app_log_x ? std::log10(std::max(1e-300, X[i])) : X[i];
            double yt = 0.0;
            double xpow = 1.0;
            for (unsigned j = 0; j < d_p && j < par.size(); ++j) {
                yt += par[j] * xpow;
                xpow *= xt;
            }
            Y[i] = d_app_log_y ? std::pow(10.0, yt) : yt;
        }
        return true;
    }

    generateX(X);
    for (int i = 0; i < d_points; i++) {
        double yi = 0.0;
        double xpow = 1.0;
        for (unsigned j = 0; j < d_p && j < par.size(); j++) {
            yi += par[j] * xpow;
            xpow *= X[i];
        }
        Y[i] = yi;
    }
    return true;
}

void PolynomialFit::fit()
{
    if (d_init_err)
        return;

    int minNeeded = std::max(1, effectiveFreeParameters());
    if (unsigned(minNeeded) > d_x.size()) {
        if (!d_silent)
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Fit Error"),
                    tr("You need at least %1 data points for this fit operation. Operation aborted!")
                            .arg(minNeeded));
        return;
    }

    d_result_errors.clear();
    d_has_custom_r2 = false;
    d_straight_line_fit = false;
    d_convergence = Converged;
    d_fit_notes.clear();
    int xAx = d_curve ? d_curve->xAxis() : QwtPlot::xBottom;
    int yAx = d_curve ? d_curve->yAxis() : QwtPlot::yLeft;
    d_app_log_x = d_apparent_fit && isGraphAxisLog10(d_graph, xAx);
    d_app_log_y = d_apparent_fit && isGraphAxisLog10(d_graph, yAx);
    if (d_apparent_fit && !d_app_log_x && !d_app_log_y)
        d_fit_notes << tr("Apparent linear fit: no axis is logarithmic, so an ordinary linear fit "
                          "was performed.");

    if (d_apparent_fit && (d_app_log_x || d_app_log_y)) {
        const std::vector<double> baseW = computeWeightsVector();
        std::vector<double> xt, yt, wt;
        xt.reserve(d_x.size());
        yt.reserve(d_y.size());
        wt.reserve(d_y.size());
        const double ln10 = std::log(10.0);
        for (size_t i = 0; i < d_x.size(); ++i) {
            if ((d_app_log_x && d_x[i] <= 0.0) || (d_app_log_y && d_y[i] <= 0.0))
                continue;
            xt.push_back(d_app_log_x ? std::log10(d_x[i]) : d_x[i]);
            yt.push_back(d_app_log_y ? std::log10(d_y[i]) : d_y[i]);
            if (d_y_error_source == UnknownErrors && d_weighting_method == WeightNone) {
                wt.push_back(1.0);
            } else {
                const double wi = (i < baseW.size() && std::isfinite(baseW[i]) && baseW[i] > 0.0)
                                          ? baseW[i]
                                          : 1.0;
                const double w_trans =
                        d_app_log_y ? (wi * (d_y[i] * ln10) * (d_y[i] * ln10)) : wi;
                wt.push_back(std::max(1e-30, w_trans));
            }
        }
        if (xt.size() < d_x.size())
            d_fit_notes << tr("%1 of %2 points with non-positive values were excluded from the "
                              "log-space fit.")
                                   .arg(int(d_x.size() - xt.size()))
                                   .arg(int(d_x.size()));
        if (xt.size() < size_t(d_p)) {
            chi_2 = -1.0;
            d_y_fit.clear();
            d_results.assign(d_p, std::numeric_limits<double>::quiet_NaN());
            reportApparentFitShortage(this, (ApplicationWindow *)parent(), d_silent, int(d_p),
                                      int(xt.size()));
            return;
        }
        if (d_p == 2)
            setStraightLinePearson(xt, yt);

        gsl_matrix *X = gsl_matrix_alloc(xt.size(), d_p);
        gsl_vector *c = gsl_vector_alloc(d_p);
        for (size_t i = 0; i < xt.size(); ++i) {
            double xpow = 1.0;
            for (unsigned j = 0; j < d_p; ++j) {
                gsl_matrix_set(X, i, j, xpow);
                xpow *= xt[i];
            }
        }
        gsl_vector_view yv = gsl_vector_view_array(yt.data(), yt.size());
        gsl_vector_view wv = gsl_vector_view_array(wt.data(), wt.size());
        gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(xt.size(), d_p);
        gsl_multifit_wlinear(X, &wv.vector, &yv.vector, c, covar, &chi_2, work);
        if (xt.size() < d_x.size() && xt.size() > size_t(d_p)
            && (d_y_error_source == UnknownErrors || d_scale_errors)) {
            const int fullDof = int(d_x.size()) - effectiveFreeParameters();
            const int subDof = int(xt.size()) - int(d_p);
            if (fullDof > 0 && subDof > 0)
                gsl_matrix_scale(covar, double(fullDof) / double(subDof));
        }

        for (unsigned i = 0; i < d_p; ++i)
            d_results[i] = gsl_vector_get(c, i);

        gsl_multifit_linear_free(work);
        gsl_matrix_free(X);
        gsl_vector_free(c);

        std::vector<double> yfit_t(xt.size(), 0.0);
        for (size_t i = 0; i < xt.size(); ++i) {
            double xpow = 1.0;
            for (unsigned j = 0; j < d_p; ++j) {
                yfit_t[i] += d_results[j] * xpow;
                xpow *= xt[i];
            }
        }
        d_has_custom_r2 = true;
        d_custom_r2 = computeTransformedR2(yt, yfit_t);

        d_y_fit.resize(d_x.size());
        for (size_t i = 0; i < d_x.size(); ++i) {
            double xti = d_app_log_x ? std::log10(std::max(1e-300, d_x[i])) : d_x[i];
            double yti = 0.0;
            double xpow = 1.0;
            for (unsigned j = 0; j < d_p; ++j) {
                yti += d_results[j] * xpow;
                xpow *= xti;
            }
            d_y_fit[i] = d_app_log_y ? std::pow(10.0, yti) : yti;
        }
    } else if (hasActiveConstraints()) {
        // Seed unconstrained OLS estimates into non-explicit parameters first
        if (d_x.size() >= size_t(d_p)) {
            gsl_matrix *X = gsl_matrix_alloc(d_x.size(), d_p);
            gsl_vector *c = gsl_vector_alloc(d_p);
            gsl_matrix *tmpCov = gsl_matrix_alloc(d_p, d_p);
            double tmpChi2 = 0.0;
            for (size_t i = 0; i < d_x.size(); ++i) {
                double xpow = 1.0;
                for (unsigned j = 0; j < d_p; ++j) {
                    gsl_matrix_set(X, i, j, xpow);
                    xpow *= d_x[i];
                }
            }
            gsl_vector_view yv = gsl_vector_view_array(d_y.data(), d_y.size());
            gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(d_x.size(), d_p);
            gsl_multifit_linear(X, &yv.vector, c, tmpCov, &tmpChi2, work);
            for (unsigned j = 0; j < d_p; ++j) {
                if (!d_all_guesses_set && !d_explicit_init_vals.contains(int(j)))
                    gsl_vector_set(d_param_init, j, gsl_vector_get(c, j));
            }
            gsl_multifit_linear_free(work);
            gsl_matrix_free(tmpCov);
            gsl_matrix_free(X);
            gsl_vector_free(c);
        }
        applyExplicitInitialGuesses();
        int iterations = 0, status = GSL_SUCCESS;
        d_results = fitGslMultifit(iterations, status);
        if (d_p == 2)
            setStraightLinePearson(d_x, d_y);
    } else {
        if (d_p == 2)
            setStraightLinePearson(d_x, d_y);
        const size_t n = d_x.size();
        const std::vector<double> weightsVec = computeWeightsVector();
        d_y_errors.resize(n, 1.0);
        for (size_t i = 0; i < n; ++i)
            d_y_errors[i] = 1.0 / std::sqrt(std::max(weightsVec[i], 1e-300));

        gsl_matrix *X = gsl_matrix_alloc(n, d_p);
        gsl_vector *c = gsl_vector_alloc(d_p);

        for (size_t i = 0; i < n; i++) {
            double xpow = 1.0;
            for (unsigned j = 0; j < d_p; j++) {
                gsl_matrix_set(X, i, j, xpow);
                xpow *= d_x[i];
            }
        }

        gsl_vector_view y = gsl_vector_view_array(d_y.data(), n);
        gsl_vector_const_view weights = gsl_vector_const_view_array(weightsVec.data(), n);
        gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(n, d_p);

        gsl_multifit_wlinear(X, &weights.vector, &y.vector, c, covar, &chi_2, work);

        for (unsigned i = 0; i < d_p; i++)
            d_results[i] = gsl_vector_get(c, i);

        gsl_multifit_linear_free(work);
        gsl_matrix_free(X);
        gsl_vector_free(c);

        d_y_fit.resize(d_x.size());
        for (size_t i = 0; i < d_x.size(); ++i) {
            double yi = 0.0, xpow = 1.0;
            for (unsigned j = 0; j < d_p; ++j) {
                yi += d_results[j] * xpow;
                xpow *= d_x[i];
            }
            d_y_fit[i] = yi;
        }
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

    int minNeeded = std::max(1, effectiveFreeParameters());
    if (unsigned(minNeeded) > d_x.size()) {
        if (!d_silent)
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Fit Error"),
                    tr("You need at least %1 data points for this fit operation. Operation aborted!")
                            .arg(minNeeded));
        return;
    }

    d_result_errors.clear();
    d_has_custom_r2 = false;
    d_straight_line_fit = false;
    d_convergence = Converged;
    d_fit_notes.clear();
    int xAx = d_curve ? d_curve->xAxis() : QwtPlot::xBottom;
    int yAx = d_curve ? d_curve->yAxis() : QwtPlot::yLeft;
    d_app_log_x = d_apparent_fit && isGraphAxisLog10(d_graph, xAx);
    d_app_log_y = d_apparent_fit && isGraphAxisLog10(d_graph, yAx);
    if (d_apparent_fit && !d_app_log_x && !d_app_log_y)
        d_fit_notes << tr("Apparent linear fit: no axis is logarithmic, so an ordinary linear fit "
                          "was performed.");

    if (d_apparent_fit && (d_app_log_x || d_app_log_y)) {
        const std::vector<double> baseW = computeWeightsVector();
        std::vector<double> xt, yt, wt;
        xt.reserve(d_x.size());
        yt.reserve(d_y.size());
        wt.reserve(d_y.size());
        const double ln10 = std::log(10.0);
        for (size_t i = 0; i < d_x.size(); ++i) {
            if ((d_app_log_x && d_x[i] <= 0.0) || (d_app_log_y && d_y[i] <= 0.0))
                continue;
            xt.push_back(d_app_log_x ? std::log10(d_x[i]) : d_x[i]);
            yt.push_back(d_app_log_y ? std::log10(d_y[i]) : d_y[i]);
            if (d_y_error_source == UnknownErrors && d_weighting_method == WeightNone) {
                wt.push_back(1.0);
            } else {
                const double wi = (i < baseW.size() && std::isfinite(baseW[i]) && baseW[i] > 0.0)
                                          ? baseW[i]
                                          : 1.0;
                const double w_trans =
                        d_app_log_y ? (wi * (d_y[i] * ln10) * (d_y[i] * ln10)) : wi;
                wt.push_back(std::max(1e-30, w_trans));
            }
        }
        if (xt.size() < d_x.size())
            d_fit_notes << tr("%1 of %2 points with non-positive values were excluded from the "
                              "log-space fit.")
                                   .arg(int(d_x.size() - xt.size()))
                                   .arg(int(d_x.size()));
        if (xt.size() < 2) {
            chi_2 = -1.0;
            d_y_fit.clear();
            d_results.assign(d_p, std::numeric_limits<double>::quiet_NaN());
            reportApparentFitShortage(this, (ApplicationWindow *)parent(), d_silent, 2,
                                      int(xt.size()));
            return;
        }
        setStraightLinePearson(xt, yt);

        double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0;
        gsl_fit_wlinear(xt.data(), 1, wt.data(), 1, yt.data(), 1, xt.size(), &c0, &c1, &cov00,
                        &cov01, &cov11, &chi_2);
        if (xt.size() < d_x.size() && xt.size() > 2
            && (d_y_error_source == UnknownErrors || d_scale_errors)) {
            const int fullDof = int(d_x.size()) - effectiveFreeParameters();
            const int subDof = int(xt.size()) - 2;
            if (fullDof > 0 && subDof > 0) {
                const double adj = double(fullDof) / double(subDof);
                cov00 *= adj;
                cov01 *= adj;
                cov11 *= adj;
            }
        }

        d_results[0] = c0;
        d_results[1] = c1;
        gsl_matrix_set(covar, 0, 0, cov00);
        gsl_matrix_set(covar, 0, 1, cov01);
        gsl_matrix_set(covar, 1, 1, cov11);
        gsl_matrix_set(covar, 1, 0, cov01);

        std::vector<double> yfit_t(xt.size());
        for (size_t i = 0; i < xt.size(); ++i)
            yfit_t[i] = c0 + c1 * xt[i];
        d_has_custom_r2 = true;
        d_custom_r2 = computeTransformedR2(yt, yfit_t);

        d_y_fit.resize(d_x.size());
        for (size_t i = 0; i < d_x.size(); ++i) {
            double xti = d_app_log_x ? std::log10(std::max(1e-300, d_x[i])) : d_x[i];
            double yti = c0 + c1 * xti;
            d_y_fit[i] = d_app_log_y ? std::pow(10.0, yti) : yti;
        }
    } else if (hasActiveConstraints()) {
        if (d_x.size() >= 2) {
            double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0, tmpChi2 = 0.0;
            gsl_fit_linear(d_x.data(), 1, d_y.data(), 1, d_x.size(), &c0, &c1, &cov00, &cov01,
                           &cov11, &tmpChi2);
            if (!d_all_guesses_set && !d_explicit_init_vals.contains(0))
                gsl_vector_set(d_param_init, 0, c0);
            if (!d_all_guesses_set && !d_explicit_init_vals.contains(1))
                gsl_vector_set(d_param_init, 1, c1);
        }
        applyExplicitInitialGuesses();
        int iterations = 0, status = GSL_SUCCESS;
        d_results = fitGslMultifit(iterations, status);
        setStraightLinePearson(d_x, d_y);
    } else {
        setStraightLinePearson(d_x, d_y);
        double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0;
        const size_t n = d_x.size();
        const std::vector<double> weights = computeWeightsVector();
        d_y_errors.resize(n, 1.0);
        for (size_t i = 0; i < n; ++i)
            d_y_errors[i] = 1.0 / std::sqrt(std::max(weights[i], 1e-300));

        gsl_fit_wlinear(d_x.data(), 1, weights.data(), 1, d_y.data(), 1, n, &c0, &c1, &cov00,
                        &cov01, &cov11, &chi_2);

        d_results[0] = c0;
        d_results[1] = c1;

        gsl_matrix_set(covar, 0, 0, cov00);
        gsl_matrix_set(covar, 0, 1, cov01);
        gsl_matrix_set(covar, 1, 1, cov11);
        gsl_matrix_set(covar, 1, 0, cov01);

        d_y_fit.resize(d_x.size());
        for (size_t i = 0; i < d_x.size(); ++i)
            d_y_fit[i] = c0 + c1 * d_x[i];
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

    const size_t n = d_x.size();
    ApplicationWindow *app = (ApplicationWindow *)parent();
    DataCurve *dc = dynamic_cast<DataCurve *>(d_curve);

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
                for (size_t i = 0; i < n; ++i) {
                    int row = int(d_indices[i]);
                    if (dc && dc->table() == tbl) {
                        int trRow = dc->tableRow(int(d_indices[i]));
                        if (trRow >= 0)
                            row = trRow;
                    }
                    double v = (row >= 0 && row < tbl->numRows()) ? std::abs(tbl->cell(row, colIdx))
                                                                  : 0.0;
                    if (std::isfinite(v) && v > 0.0)
                        anyPos = true;
                    sigmaOut[i] = (std::isfinite(v) && v > 1e-15) ? v : 1e-12;
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
                        double v = std::abs(er->errorValue(int(d_indices[i])));
                        if (std::isfinite(v) && v > 0.0)
                            anyPos = true;
                        sigmaOut[i] = (std::isfinite(v) && v > 1e-15) ? v : 1e-12;
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

    std::vector<double> wx(n), wy(n);
    for (size_t i = 0; i < n; ++i) {
        wx[i] = 1.0 / (sx[i] * sx[i]);
        wy[i] = 1.0 / (sy[i] * sy[i]);
    }

    // Orthogonal / Deming initial slope estimate
    double mx0 = std::accumulate(d_x.begin(), d_x.end(), 0.0) / double(n);
    double my0 = std::accumulate(d_y.begin(), d_y.end(), 0.0) / double(n);
    double sxx = 0.0, syy = 0.0, sxy = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double dx = d_x[i] - mx0;
        double dy = d_y[i] - my0;
        sxx += dx * dx;
        syy += dy * dy;
        sxy += dx * dy;
    }
    double b = 0.0;
    if (std::abs(sxy) > 1e-30)
        b = ((syy - sxx) + std::sqrt((syy - sxx) * (syy - sxx) + 4.0 * sxy * sxy)) / (2.0 * sxy);
    else if (sxx > 1e-30)
        b = sxy / sxx;

    std::vector<double> W(n), U(n), V(n), beta(n);
    double Xbar = mx0, Ybar = my0;

    // York (1966 / 2004) exact iterative total least-squares solver
    const int maxIter = 500;
    for (int iter = 0; iter < maxIter; ++iter) {
        double sumW = 0.0, sumWX = 0.0, sumWY = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double denom = wx[i] + b * b * wy[i];
            W[i] = (denom > 1e-300) ? ((wx[i] * wy[i]) / denom) : 0.0;
            sumW += W[i];
            sumWX += W[i] * d_x[i];
            sumWY += W[i] * d_y[i];
        }
        if (sumW <= 0.0)
            return false;

        Xbar = sumWX / sumW;
        Ybar = sumWY / sumW;

        double num = 0.0, den = 0.0;
        for (size_t i = 0; i < n; ++i) {
            U[i] = d_x[i] - Xbar;
            V[i] = d_y[i] - Ybar;
            beta[i] = W[i] * (U[i] / wy[i] + b * V[i] / wx[i]);
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
    for (size_t i = 0; i < n; ++i) {
        double denom = wx[i] + b * b * wy[i];
        W[i] = (denom > 1e-300) ? ((wx[i] * wy[i]) / denom) : 0.0;
        sumW += W[i];
        sumWX += W[i] * d_x[i];
        sumWY += W[i] * d_y[i];
    }
    if (sumW <= 0.0)
        return false;

    Xbar = sumWX / sumW;
    Ybar = sumWY / sumW;
    double a = Ybar - b * Xbar;

    std::vector<double> x_adj(n);
    double sumWxAdj = 0.0;
    for (size_t i = 0; i < n; ++i) {
        U[i] = d_x[i] - Xbar;
        V[i] = d_y[i] - Ybar;
        beta[i] = W[i] * (U[i] / wy[i] + b * V[i] / wx[i]);
        x_adj[i] = Xbar + beta[i];
        sumWxAdj += W[i] * x_adj[i];
    }
    double x_adj_bar = sumWxAdj / sumW;

    double sumWu2 = 0.0;
    double S = 0.0;
    double S_tot = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double ui = x_adj[i] - x_adj_bar;
        sumWu2 += W[i] * ui * ui;
        double resid = d_y[i] - b * d_x[i] - a;
        S += W[i] * resid * resid;
        double dyBar = d_y[i] - Ybar;
        S_tot += W[i] * dyBar * dyBar;
    }

    double var_b = (sumWu2 > 1e-300) ? (1.0 / sumWu2) : 0.0;
    double var_a = (1.0 / sumW) + x_adj_bar * x_adj_bar * var_b;
    double cov_ab = -x_adj_bar * var_b;

    bool usedFallbackUnitErrors = (!hasExplicitX && !hasExplicitY);
    d_y_error_source = usedFallbackUnitErrors ? UnknownErrors : CustomErrors;
    d_weighting_method = usedFallbackUnitErrors ? WeightNone : WeightInstrumental;

    gsl_matrix_set(covar, 0, 0, var_a);
    gsl_matrix_set(covar, 0, 1, cov_ab);
    gsl_matrix_set(covar, 1, 0, cov_ab);
    gsl_matrix_set(covar, 1, 1, var_b);

    chi_2 = S;
    const double covScale = effectiveCovarScale();

    d_york_slope = b;
    d_york_intercept = a;
    d_york_slope_err = std::sqrt(std::max(0.0, covScale * var_b));
    d_york_intercept_err = std::sqrt(std::max(0.0, covScale * var_a));

    d_app_log_x = false;
    d_app_log_y = false;
    d_convergence = Converged;
    d_fit_notes.clear();
    setStraightLinePearson(d_x, d_y);
    d_results.resize(2);
    d_results[0] = a;
    d_results[1] = b;
    d_result_errors = { d_york_intercept_err, d_york_slope_err };

    d_has_custom_r2 = true;
    d_custom_r2 = (S_tot > 1e-300) ? (1.0 - S / S_tot) : 1.0;

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

    if (d_apparent_fit && (d_app_log_x || d_app_log_y)) {
        if (d_gen_function && d_app_log_x && !d_x.empty() && d_x.front() > 0.0 && d_x.back() > 0.0) {
            double lx0 = std::log10(d_x.front());
            double lstep = (d_points > 1) ? ((std::log10(d_x.back()) - lx0) / (d_points - 1)) : 0.0;
            for (int i = 0; i < d_points; ++i)
                X[i] = std::pow(10.0, lx0 + i * lstep);
        } else {
            generateX(X);
        }
        for (int i = 0; i < d_points; ++i) {
            double xt = d_app_log_x ? std::log10(std::max(1e-300, X[i])) : X[i];
            double yt = par[0] + par[1] * xt;
            Y[i] = d_app_log_y ? std::pow(10.0, yt) : yt;
        }
        return true;
    }

    generateX(X);
    for (int i = 0; i < d_points; i++)
        Y[i] = par[0] + par[1] * X[i];
    return true;
}
