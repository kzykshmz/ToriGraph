/***************************************************************************
    File                 : Fit.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Fit base class

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
#include "Fit.h"
#include "fit_gsl.h"
#include "Table.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"
#include "Legend.h"
#include "FunctionCurve.h"
#include "ColorButton.h"
#include "Script.h"
#include "NonLinearFit.h"
#include "PolynomialFit.h"
#include "MultiPeakFit.h"
#include "ExponentialFit.h"
#include "SigmoidalFit.h"
#include "future/core/column/Column.h"

#include <gsl/gsl_statistics.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_deriv.h>
#include <gsl/gsl_version.h>

#include <QApplication>
#include <QMessageBox>
#include <QDateTime>
#include <QLocale>
#include <QRegExp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>

using namespace std;

namespace {

enum ParamBoundMode {
    BoundNone = 0,
    BoundBoth = 1,
    BoundLower = 2,
    BoundUpper = 3
};

struct ConstrainedFitContext
{
    FitData *orig_data = nullptr;
    Fit::fit_function orig_f = nullptr;
    Fit::fit_function_df orig_df = nullptr;
    Fit::fit_function_fdf orig_fdf = nullptr;
    Fit::fit_function_simplex orig_fsimplex = nullptr;
    size_t n_pts = 0;
    unsigned p_full = 0;
    std::vector<int> free_to_full;
    std::vector<double> base_p;
    std::vector<ParamBoundMode> modes;
    std::vector<double> lo;
    std::vector<double> hi;
    gsl_vector *x_full_buf = nullptr;
    gsl_matrix *J_full_buf = nullptr;

    ~ConstrainedFitContext()
    {
        if (x_full_buf)
            gsl_vector_free(x_full_buf);
        if (J_full_buf)
            gsl_matrix_free(J_full_buf);
    }

    double mapUtoP(size_t k, double u) const
    {
        switch (modes[k]) {
        case BoundNone:
            return u;
        case BoundBoth: {
            double uc = std::max(-16.0, std::min(16.0, u));
            return lo[k] + 0.5 * (hi[k] - lo[k]) * (std::tanh(uc) + 1.0);
        }
        case BoundLower: {
            double uc = std::max(-1e8, std::min(1e8, u));
            return lo[k] + uc * uc;
        }
        case BoundUpper: {
            double uc = std::max(-1e8, std::min(1e8, u));
            return hi[k] - uc * uc;
        }
        }
        return u;
    }

    double dPdU(size_t k, double u) const
    {
        switch (modes[k]) {
        case BoundNone:
            return 1.0;
        case BoundBoth: {
            double uc = std::max(-16.0, std::min(16.0, u));
            double ch = std::cosh(uc);
            double sech2 = 1.0 / (ch * ch);
            return 0.5 * (hi[k] - lo[k]) * std::max(1e-13, sech2);
        }
        case BoundLower: {
            double deriv = 2.0 * u;
            return (std::abs(deriv) < 1e-10) ? (u >= 0.0 ? 1e-10 : -1e-10) : deriv;
        }
        case BoundUpper: {
            double deriv = -2.0 * u;
            return (std::abs(deriv) < 1e-10) ? (u >= 0.0 ? -1e-10 : 1e-10) : deriv;
        }
        }
        return 1.0;
    }

    double snapP(size_t k, double p) const
    {
        if (modes[k] == BoundBoth) {
            double span = std::max(1.0, hi[k] - lo[k]);
            if (p <= lo[k] + 1e-6 * span)
                return lo[k];
            if (p >= hi[k] - 1e-6 * span)
                return hi[k];
            return std::max(lo[k], std::min(hi[k], p));
        }
        if (modes[k] == BoundLower) {
            if (p <= lo[k] + 1e-7 * std::max(1.0, std::abs(lo[k])))
                return lo[k];
            return std::max(lo[k], p);
        }
        if (modes[k] == BoundUpper) {
            if (p >= hi[k] - 1e-7 * std::max(1.0, std::abs(hi[k])))
                return hi[k];
            return std::min(hi[k], p);
        }
        return p;
    }

    void unpackToFull(const gsl_vector *u_vec, gsl_vector *x_full) const
    {
        for (unsigned j = 0; j < p_full; ++j)
            gsl_vector_set(x_full, j, base_p[j]);
        for (size_t k = 0; k < free_to_full.size(); ++k) {
            double u = gsl_vector_get(u_vec, k);
            gsl_vector_set(x_full, free_to_full[k], mapUtoP(k, u));
        }
    }

    static int eval_f(const gsl_vector *u, void *params, gsl_vector *f)
    {
        ConstrainedFitContext *ctx = static_cast<ConstrainedFitContext *>(params);
        ctx->unpackToFull(u, ctx->x_full_buf);
        return ctx->orig_f(ctx->x_full_buf, ctx->orig_data, f);
    }

    static int eval_df(const gsl_vector *u, void *params, gsl_matrix *J_free)
    {
        ConstrainedFitContext *ctx = static_cast<ConstrainedFitContext *>(params);
        ctx->unpackToFull(u, ctx->x_full_buf);
        int st = ctx->orig_df(ctx->x_full_buf, ctx->orig_data, ctx->J_full_buf);
        if (st != GSL_SUCCESS)
            return st;
        for (size_t k = 0; k < ctx->free_to_full.size(); ++k) {
            int j = ctx->free_to_full[k];
            double scale = ctx->dPdU(k, gsl_vector_get(u, k));
            for (size_t i = 0; i < ctx->n_pts; ++i)
                gsl_matrix_set(J_free, i, k, gsl_matrix_get(ctx->J_full_buf, i, j) * scale);
        }
        return GSL_SUCCESS;
    }

    static int eval_fdf(const gsl_vector *u, void *params, gsl_vector *f, gsl_matrix *J_free)
    {
        ConstrainedFitContext *ctx = static_cast<ConstrainedFitContext *>(params);
        ctx->unpackToFull(u, ctx->x_full_buf);
        int st = GSL_SUCCESS;
        if (ctx->orig_fdf) {
            st = ctx->orig_fdf(ctx->x_full_buf, ctx->orig_data, f, ctx->J_full_buf);
        } else {
            st = ctx->orig_f(ctx->x_full_buf, ctx->orig_data, f);
            if (st == GSL_SUCCESS)
                st = ctx->orig_df(ctx->x_full_buf, ctx->orig_data, ctx->J_full_buf);
        }
        if (st != GSL_SUCCESS)
            return st;
        for (size_t k = 0; k < ctx->free_to_full.size(); ++k) {
            int j = ctx->free_to_full[k];
            double scale = ctx->dPdU(k, gsl_vector_get(u, k));
            for (size_t i = 0; i < ctx->n_pts; ++i)
                gsl_matrix_set(J_free, i, k, gsl_matrix_get(ctx->J_full_buf, i, j) * scale);
        }
        return GSL_SUCCESS;
    }

    static double eval_simplex(const gsl_vector *u, void *params)
    {
        ConstrainedFitContext *ctx = static_cast<ConstrainedFitContext *>(params);
        ctx->unpackToFull(u, ctx->x_full_buf);
        return ctx->orig_fsimplex(ctx->x_full_buf, ctx->orig_data);
    }
};

} // namespace

Fit::Fit(ApplicationWindow *parent, Graph *g, QString name)
    : Filter(parent, g, name), scripted(ScriptingLangManager::newEnv("muParser", parent))
{
    d_p = 0;
    d_curveColor = ColorButton::color(1);
    d_solver = ScaledLevenbergMarquardt;
    d_tolerance = 1e-4;
    d_gen_function = true;
    d_points = 100;
    d_max_iterations = 1000;
    d_curve = 0;
    d_formula = QString();
    d_explanation = QString();
    d_y_error_source = UnknownErrors;
    d_y_error_dataset = QString();
    d_prec = parent->fit_output_precision;
    d_init_err = false;
    chi_2 = -1;
    d_scale_errors = false;
    d_sort_data = true;
    d_initial_guesses_set = false;
}

vector<double> Fit::fitGslMultifit(int &iterations, int &status)
{
    vector<double> result(d_p);

    // declare input data
    struct FitData data = { d_x.size(), size_t(d_p), d_x.data(), d_y.data(), &d_y_errors[0], this };

    const gsl_multifit_fdfsolver_type *T = nullptr;
    switch (d_solver) {
    case ScaledLevenbergMarquardt:
        T = gsl_multifit_fdfsolver_lmsder;
        break;
    case UnscaledLevenbergMarquardt:
        T = gsl_multifit_fdfsolver_lmder;
        break;
    default:
        T = gsl_multifit_fdfsolver_lmsder;
        break;
    }

    if (hasActiveConstraints()) {
        ConstrainedFitContext ctx;
        ctx.orig_data = &data;
        ctx.orig_f = d_f;
        ctx.orig_df = d_df;
        ctx.orig_fdf = d_fdf;
        ctx.orig_fsimplex = d_fsimplex;
        ctx.n_pts = d_x.size();
        ctx.p_full = d_p;
        ctx.base_p.resize(d_p, 0.0);
        ctx.x_full_buf = gsl_vector_alloc(d_p);
        ctx.J_full_buf = gsl_matrix_alloc(d_x.size(), d_p);

        std::vector<double> u0_vals;
        for (unsigned j = 0; j < d_p; ++j) {
            double p0 = gsl_vector_get(d_param_init, j);
            double lo_u = -std::numeric_limits<double>::infinity();
            double hi_u = std::numeric_limits<double>::infinity();
            bool bounded = hasParameterBounds(int(j), &lo_u, &hi_u);
            double lo = lo_u, hi = hi_u;
            if (bounded) {
                if (std::isfinite(lo_u))
                    lo = toInternalParameter(int(j), lo_u);
                if (std::isfinite(hi_u))
                    hi = toInternalParameter(int(j), hi_u);
                if (lo > hi)
                    std::swap(lo, hi);
            }

            if (isParameterFixed(int(j)) || (bounded && std::isfinite(lo) && std::isfinite(hi) && std::abs(hi - lo) <= 1e-15)) {
                if (!isParameterFixed(int(j)) && bounded)
                    p0 = 0.5 * (lo + hi);
                ctx.base_p[j] = p0;
                continue;
            }

            ctx.base_p[j] = p0;
            ctx.free_to_full.push_back(int(j));
            if (!bounded || (!std::isfinite(lo) && !std::isfinite(hi))) {
                ctx.modes.push_back(BoundNone);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                u0_vals.push_back(p0);
            } else if (std::isfinite(lo) && std::isfinite(hi)) {
                ctx.modes.push_back(BoundBoth);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                double t = (p0 - lo) / (hi - lo);
                t = std::max(1e-4, std::min(1.0 - 1e-4, t));
                u0_vals.push_back(std::atanh(2.0 * t - 1.0));
            } else if (std::isfinite(lo)) {
                ctx.modes.push_back(BoundLower);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                double d0 = std::max(1e-3, p0 - lo);
                u0_vals.push_back(std::sqrt(d0));
            } else {
                ctx.modes.push_back(BoundUpper);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                double d0 = std::max(1e-3, hi - p0);
                u0_vals.push_back(std::sqrt(d0));
            }
        }

        const size_t p_free = ctx.free_to_full.size();
        if (p_free == 0) {
            for (unsigned j = 0; j < d_p; ++j) {
                result[j] = ctx.base_p[j];
                gsl_vector_set(ctx.x_full_buf, j, result[j]);
            }
            gsl_vector *f_vec = gsl_vector_alloc(d_x.size());
            d_f(ctx.x_full_buf, &data, f_vec);
            gsl_blas_ddot(f_vec, f_vec, &chi_2);
            d_y_fit.resize(d_x.size());
            for (size_t i = 0; i < d_x.size(); ++i)
                d_y_fit[i] = d_y[i] + gsl_vector_get(f_vec, i) * d_y_errors[i];
            gsl_vector_free(f_vec);
            gsl_matrix_set_zero(covar);
            iterations = 0;
            status = GSL_SUCCESS;
            return result;
        }

        gsl_vector *u_init = gsl_vector_alloc(p_free);
        for (size_t k = 0; k < p_free; ++k)
            gsl_vector_set(u_init, k, u0_vals[k]);

        gsl_multifit_function_fdf f;
        f.f = &ConstrainedFitContext::eval_f;
        f.df = &ConstrainedFitContext::eval_df;
        f.fdf = &ConstrainedFitContext::eval_fdf;
        f.n = d_x.size();
        f.p = p_free;
        f.params = &ctx;

        gsl_multifit_fdfsolver *s = gsl_multifit_fdfsolver_alloc(T, d_x.size(), p_free);
        status = gsl_multifit_fdfsolver_set(s, &f, u_init);
        if (status) {
            gsl_multifit_fdfsolver_free(s);
            gsl_vector_free(u_init);
            return result;
        }

        std::vector<double> prev_phys(p_free);
        for (size_t k = 0; k < p_free; ++k)
            prev_phys[k] = ctx.mapUtoP(k, u0_vals[k]);

        for (iterations = 0; iterations < d_max_iterations; iterations++) {
            status = gsl_multifit_fdfsolver_iterate(s);
            int delta_status = gsl_multifit_test_delta(s->dx, s->x, d_tolerance, d_tolerance);

            bool phys_converged = (iterations >= 2);
            double dx_norm = gsl_blas_dnrm2(s->dx);
            if (dx_norm == 0.0 && status == GSL_CONTINUE)
                phys_converged = false;
            for (size_t k = 0; k < p_free; ++k) {
                double pj_new = ctx.mapUtoP(k, gsl_vector_get(s->x, k));
                if (std::abs(pj_new - prev_phys[k]) > d_tolerance * (d_tolerance + std::abs(pj_new)))
                    phys_converged = false;
                prev_phys[k] = pj_new;
            }

            if (delta_status == GSL_SUCCESS || phys_converged) {
                status = GSL_SUCCESS;
                break;
            }
            if (status) {
                if (status == GSL_ENOPROG || status == GSL_ETOLF || status == GSL_ETOLX
                    || status == GSL_ETOLG)
                    status = GSL_SUCCESS;
                break;
            }
            status = delta_status;
            if (status != GSL_CONTINUE)
                break;
        }
        if (status == GSL_CONTINUE)
            status = GSL_SUCCESS;

        result = ctx.base_p;
        for (size_t k = 0; k < p_free; ++k) {
            double p_val = ctx.snapP(k, ctx.mapUtoP(k, gsl_vector_get(s->x, k)));
            result[ctx.free_to_full[k]] = p_val;
        }
        for (unsigned j = 0; j < d_p; ++j)
            gsl_vector_set(ctx.x_full_buf, j, result[j]);

        gsl_vector *f_final = gsl_vector_alloc(d_x.size());
        if (d_f(ctx.x_full_buf, &data, f_final) == GSL_SUCCESS) {
            gsl_blas_ddot(f_final, f_final, &chi_2);
            d_y_fit.resize(d_x.size());
            for (size_t i = 0; i < d_x.size(); ++i)
                d_y_fit[i] = d_y[i] + gsl_vector_get(f_final, i) * d_y_errors[i];
        } else {
            gsl_blas_ddot(s->f, s->f, &chi_2);
            d_y_fit.resize(d_x.size());
            for (size_t i = 0; i < d_x.size(); ++i)
                d_y_fit[i] = d_y[i] + gsl_vector_get(s->f, i) * d_y_errors[i];
        }
        gsl_vector_free(f_final);

        gsl_matrix_set_zero(covar);
        if (d_df(ctx.x_full_buf, &data, ctx.J_full_buf) == GSL_SUCCESS) {
            gsl_matrix *J_phys_free = gsl_matrix_alloc(d_x.size(), p_free);
            gsl_matrix *cov_free = gsl_matrix_alloc(p_free, p_free);
            for (size_t k = 0; k < p_free; ++k) {
                int j = ctx.free_to_full[k];
                for (size_t i = 0; i < d_x.size(); ++i)
                    gsl_matrix_set(J_phys_free, i, k, gsl_matrix_get(ctx.J_full_buf, i, j));
            }
            gsl_multifit_covar(J_phys_free, 0.0, cov_free);
            for (size_t a = 0; a < p_free; ++a) {
                for (size_t b = 0; b < p_free; ++b) {
                    gsl_matrix_set(covar, ctx.free_to_full[a], ctx.free_to_full[b],
                                   gsl_matrix_get(cov_free, a, b));
                }
            }
            gsl_matrix_free(J_phys_free);
            gsl_matrix_free(cov_free);
        }
        if (d_y_error_source == UnknownErrors) {
            double dof = (d_x.size() > p_free) ? double(d_x.size() - p_free) : 1.0;
            gsl_matrix_scale(covar, chi_2 / dof);
        }

        gsl_multifit_fdfsolver_free(s);
        gsl_vector_free(u_init);
        return result;
    }

    gsl_multifit_function_fdf f;
    f.f = d_f;
    f.df = d_df;
    f.fdf = d_fdf;
    f.n = d_x.size();
    f.p = d_p;
    f.params = &data;

    gsl_multifit_fdfsolver *s = gsl_multifit_fdfsolver_alloc(T, d_x.size(), d_p);
    status = gsl_multifit_fdfsolver_set(s, &f, d_param_init);
    if (status) {
        gsl_multifit_fdfsolver_free(s);
        return result;
    }

    // iterate solver algorithm
    for (iterations = 0; iterations < d_max_iterations; iterations++) {
        status = gsl_multifit_fdfsolver_iterate(s);
        int delta_status = gsl_multifit_test_delta(s->dx, s->x, d_tolerance, d_tolerance);
        if (delta_status == GSL_SUCCESS) {
            status = GSL_SUCCESS;
            break;
        }
        if (status) {
            if (status == GSL_ENOPROG || status == GSL_ETOLF || status == GSL_ETOLX
                || status == GSL_ETOLG)
                status = GSL_SUCCESS;
            break;
        }
        status = delta_status;
        if (status != GSL_CONTINUE)
            break;
    }
    if (status == GSL_CONTINUE)
        status = GSL_SUCCESS;

    // grab results
    for (unsigned i = 0; i < d_p; i++)
        result[i] = gsl_vector_get(s->x, i);
    gsl_blas_ddot(s->f, s->f, &chi_2);
    d_y_fit.resize(d_x.size());
    for (size_t i = 0; i < d_x.size(); ++i)
        d_y_fit[i] = d_y[i] + gsl_vector_get(s->f, i) * d_y_errors[i];
#if GSL_MAJOR_VERSION < 2
    gsl_multifit_covar(s->J, 0.0, covar);
#else
    {
        gsl_matrix *J = gsl_matrix_alloc(d_x.size(), d_p);
        gsl_multifit_fdfsolver_jac(s, J);
        gsl_multifit_covar(J, 0.0, covar);
        gsl_matrix_free(J);
    }
#endif
    if (d_y_error_source == UnknownErrors) {
        // multiply covar by variance of residuals, which is used as an estimate for the
        // statistical errors (this relies on the Y errors being set to 1.0, so that
        // s->f is properly normalized)
        double dof = (d_x.size() > d_p) ? double(d_x.size() - d_p) : 1.0;
        gsl_matrix_scale(covar, chi_2 / dof);
    }

    // free memory allocated for fitting
    gsl_multifit_fdfsolver_free(s);

    return result;
}

vector<double> Fit::fitGslMultimin(int &iterations, int &status)
{
    vector<double> result(d_p);

    // declare input data
    struct FitData data = { d_x.size(), size_t(d_p), d_x.data(), d_y.data(), &d_y_errors[0], this };

    if (hasActiveConstraints()) {
        ConstrainedFitContext ctx;
        ctx.orig_data = &data;
        ctx.orig_f = d_f;
        ctx.orig_df = d_df;
        ctx.orig_fdf = d_fdf;
        ctx.orig_fsimplex = d_fsimplex;
        ctx.n_pts = d_x.size();
        ctx.p_full = d_p;
        ctx.base_p.resize(d_p, 0.0);
        ctx.x_full_buf = gsl_vector_alloc(d_p);
        ctx.J_full_buf = gsl_matrix_alloc(d_x.size(), d_p);

        std::vector<double> u0_vals;
        for (unsigned j = 0; j < d_p; ++j) {
            double p0 = gsl_vector_get(d_param_init, j);
            double lo_u = -std::numeric_limits<double>::infinity();
            double hi_u = std::numeric_limits<double>::infinity();
            bool bounded = hasParameterBounds(int(j), &lo_u, &hi_u);
            double lo = lo_u, hi = hi_u;
            if (bounded) {
                if (std::isfinite(lo_u))
                    lo = toInternalParameter(int(j), lo_u);
                if (std::isfinite(hi_u))
                    hi = toInternalParameter(int(j), hi_u);
                if (lo > hi)
                    std::swap(lo, hi);
            }

            if (isParameterFixed(int(j)) || (bounded && std::isfinite(lo) && std::isfinite(hi) && std::abs(hi - lo) <= 1e-15)) {
                if (!isParameterFixed(int(j)) && bounded)
                    p0 = 0.5 * (lo + hi);
                ctx.base_p[j] = p0;
                continue;
            }

            ctx.base_p[j] = p0;
            ctx.free_to_full.push_back(int(j));
            if (!bounded || (!std::isfinite(lo) && !std::isfinite(hi))) {
                ctx.modes.push_back(BoundNone);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                u0_vals.push_back(p0);
            } else if (std::isfinite(lo) && std::isfinite(hi)) {
                ctx.modes.push_back(BoundBoth);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                double t = (p0 - lo) / (hi - lo);
                t = std::max(1e-4, std::min(1.0 - 1e-4, t));
                u0_vals.push_back(std::atanh(2.0 * t - 1.0));
            } else if (std::isfinite(lo)) {
                ctx.modes.push_back(BoundLower);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                u0_vals.push_back(std::sqrt(std::max(1e-3, p0 - lo)));
            } else {
                ctx.modes.push_back(BoundUpper);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                u0_vals.push_back(std::sqrt(std::max(1e-3, hi - p0)));
            }
        }

        const size_t p_free = ctx.free_to_full.size();
        if (p_free == 0) {
            result = ctx.base_p;
            for (unsigned j = 0; j < d_p; ++j)
                gsl_vector_set(ctx.x_full_buf, j, result[j]);
            chi_2 = d_fsimplex(ctx.x_full_buf, &data);
            gsl_matrix_set_zero(covar);
            iterations = 0;
            status = GSL_SUCCESS;
            return result;
        }

        gsl_vector *u_init = gsl_vector_alloc(p_free);
        gsl_vector *ss = gsl_vector_alloc(p_free);
        for (size_t k = 0; k < p_free; ++k) {
            gsl_vector_set(u_init, k, u0_vals[k]);
            gsl_vector_set(ss, k, (ctx.modes[k] == BoundNone) ? 1.0 : 0.5);
        }

        gsl_multimin_function f;
        f.f = &ConstrainedFitContext::eval_simplex;
        f.n = p_free;
        f.params = &ctx;

        const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex;
        gsl_multimin_fminimizer *s_min = gsl_multimin_fminimizer_alloc(T, f.n);
        gsl_multimin_fminimizer_set(s_min, &f, u_init, ss);

        for (iterations = 0; iterations < d_max_iterations; iterations++) {
            status = gsl_multimin_fminimizer_iterate(s_min);
            if (status)
                break;
            double size = gsl_multimin_fminimizer_size(s_min);
            status = gsl_multimin_test_size(size, d_tolerance);
            if (status != GSL_CONTINUE)
                break;
        }
        if (status == GSL_CONTINUE)
            status = GSL_SUCCESS;

        result = ctx.base_p;
        for (size_t k = 0; k < p_free; ++k)
            result[ctx.free_to_full[k]] = ctx.snapP(k, ctx.mapUtoP(k, gsl_vector_get(s_min->x, k)));
        for (unsigned j = 0; j < d_p; ++j)
            gsl_vector_set(ctx.x_full_buf, j, result[j]);

        chi_2 = s_min->fval;
        if (d_f) {
            gsl_vector *f_vec = gsl_vector_alloc(d_x.size());
            if (d_f(ctx.x_full_buf, &data, f_vec) == GSL_SUCCESS) {
                d_y_fit.resize(d_x.size());
                for (size_t i = 0; i < d_x.size(); ++i)
                    d_y_fit[i] = d_y[i] + gsl_vector_get(f_vec, i) * d_y_errors[i];
            }
            gsl_vector_free(f_vec);
        }
        gsl_matrix_set_zero(covar);
        if (d_df && d_df(ctx.x_full_buf, &data, ctx.J_full_buf) == GSL_SUCCESS) {
            gsl_matrix *J_phys_free = gsl_matrix_alloc(d_x.size(), p_free);
            gsl_matrix *cov_free = gsl_matrix_alloc(p_free, p_free);
            for (size_t k = 0; k < p_free; ++k) {
                int j = ctx.free_to_full[k];
                for (size_t i = 0; i < d_x.size(); ++i)
                    gsl_matrix_set(J_phys_free, i, k, gsl_matrix_get(ctx.J_full_buf, i, j));
            }
            gsl_multifit_covar(J_phys_free, 0.0, cov_free);
            for (size_t a = 0; a < p_free; ++a)
                for (size_t b = 0; b < p_free; ++b)
                    gsl_matrix_set(covar, ctx.free_to_full[a], ctx.free_to_full[b],
                                   gsl_matrix_get(cov_free, a, b));
            gsl_matrix_free(J_phys_free);
            gsl_matrix_free(cov_free);
        }
        if (d_y_error_source == UnknownErrors) {
            double dof = (d_x.size() > p_free) ? double(d_x.size() - p_free) : 1.0;
            gsl_matrix_scale(covar, chi_2 / dof);
        }

        gsl_multimin_fminimizer_free(s_min);
        gsl_vector_free(ss);
        gsl_vector_free(u_init);
        return result;
    }

    gsl_multimin_function f;
    f.f = d_fsimplex;
    f.n = d_p;
    f.params = &data;

    // step size (size of the simplex)
    // can be increased for faster convergence
    gsl_vector *ss = gsl_vector_alloc(f.n);
    gsl_vector_set_all(ss, 10.0);

    // initialize minimizer
    const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex;
    gsl_multimin_fminimizer *s_min = gsl_multimin_fminimizer_alloc(T, f.n);
    gsl_multimin_fminimizer_set(s_min, &f, d_param_init, ss);

    // iterate minimization algorithm
    for (iterations = 0; iterations < d_max_iterations; iterations++) {
        status = gsl_multimin_fminimizer_iterate(s_min);
        if (status)
            break;

        double size = gsl_multimin_fminimizer_size(s_min);
        status = gsl_multimin_test_size(size, d_tolerance);
        if (status != GSL_CONTINUE)
            break;
    }
    if (status == GSL_CONTINUE)
        status = GSL_SUCCESS;

    // grab results
    for (unsigned i = 0; i < d_p; i++)
        result[i] = gsl_vector_get(s_min->x, i);
    chi_2 = s_min->fval;
    if (d_f) {
        gsl_vector *f_vec = gsl_vector_alloc(d_x.size());
        if (d_f(s_min->x, (void *)f.params, f_vec) == GSL_SUCCESS) {
            d_y_fit.resize(d_x.size());
            for (size_t i = 0; i < d_x.size(); ++i)
                d_y_fit[i] = d_y[i] + gsl_vector_get(f_vec, i) * d_y_errors[i];
        }
        gsl_vector_free(f_vec);
    }
    gsl_matrix *J = gsl_matrix_alloc(d_x.size(), d_p);
    d_df(s_min->x, (void *)f.params, J);
    gsl_multifit_covar(J, 0.0, covar);
    if (d_y_error_source == UnknownErrors) {
        // multiply covar by variance of residuals, which is used as an estimate for the
        // statistical errors (this relies on the Y errors being set to 1.0)
        double dof = (d_x.size() > d_p) ? double(d_x.size() - d_p) : 1.0;
        gsl_matrix_scale(covar, chi_2 / dof);
    }

    // free previously allocated memory
    gsl_matrix_free(J);
    gsl_multimin_fminimizer_free(s_min);
    gsl_vector_free(ss);

    return result;
}

void Fit::setDataCurve(int curve, double start, double end)
{
    Filter::setDataCurve(curve, start, end);

    if (!setYErrorSource(AssociatedErrors, QString(), true))
        setYErrorSource(UnknownErrors);
}

void Fit::setInitialGuesses(double *x_init)
{
    d_explicit_init_vals.clear();
    for (unsigned i = 0; i < d_p; i++)
        gsl_vector_set(d_param_init, i, x_init[i]);
    d_initial_guesses_set = true;
    d_all_guesses_set = true;
}

void Fit::generateFunction(bool yes, int points)
{
    d_gen_function = yes;
    if (d_gen_function)
        d_points = points;
}

QString Fit::logFitInfo(const vector<double> &par, int iterations, int status,
                        const QString &plotName)
{
    QDateTime dt = QDateTime::currentDateTime();
    QString info = "[" + QLocale().toString(dt) + "\t" + tr("Plot") + ": ''" + plotName + "'']\n";
    info += d_explanation + " " + tr("fit of dataset") + ": " + d_curve->title().text();
    if (!d_formula.isEmpty())
        info += ", " + tr("using function") + ": " + d_formula + "\n";
    else
        info += "\n";

    info += tr("Y standard errors") + ": ";
    switch (d_y_error_source) {
    case UnknownErrors:
        info += tr("Unknown");
        break;
    case AssociatedErrors:
        info += tr("Associated dataset (%1)").arg(d_y_error_dataset);
        break;
    case PoissonErrors:
        info += tr("Statistical (assuming Poisson distribution)");
        break;
    case CustomErrors:
        info += tr("Arbitrary Dataset") + ": " + d_y_error_dataset;
        break;
    }
    info += "\n";

    if (is_non_linear) {
        if (d_solver == NelderMeadSimplex)
            info += tr("Nelder-Mead Simplex");
        else if (d_solver == UnscaledLevenbergMarquardt)
            info += tr("Unscaled Levenberg-Marquardt");
        else
            info += tr("Scaled Levenberg-Marquardt");

        info += tr(" algorithm with tolerance = ") + QLocale().toString(d_tolerance) + "\n";
    }

    info += tr("From x") + " = " + QLocale().toString(d_x[0], 'g', 15) + " " + tr("to x") + " = "
            + QLocale().toString(d_x.back(), 'g', 15) + "\n";
    double chi_2_dof = reducedChiSquare();
    for (unsigned i = 0; i < d_p; i++) {
        info += d_param_names[i] + " " + d_param_explain[i] + " = "
                + QLocale().toString(par[i], 'g', d_prec) + " +/- ";
        if (d_scale_errors)
            info += QLocale().toString(sqrt(chi_2_dof * gsl_matrix_get(covar, i, i)), 'g', d_prec)
                    + "\n";
        else
            info += QLocale().toString(sqrt(gsl_matrix_get(covar, i, i)), 'g', d_prec) + "\n";
    }
    info += "--------------------------------------------------------------------------------------"
            "\n";
    info += "Chi^2 = " + QLocale().toString(chi_2, 'g', d_prec) + "\n";
    info += tr("Reduced Chi^2") + " (Chi^2/doF) = "
            + QLocale().toString(chi_2_dof, 'g', d_prec) + "\n";
    info += tr("RSS") + " = " + QLocale().toString(rss(), 'g', d_prec) + "\n";
    info += tr("R^2") + " (COD) = " + QLocale().toString(rSquare(), 'g', d_prec) + "\n";
    info += tr("Adjusted R^2") + " = " + QLocale().toString(adjustedRSquare(), 'g', d_prec) + "\n";
    info += tr("Pearson's r") + " = " + QLocale().toString(pearsonR(), 'g', d_prec) + "\n";
    info += "--------------------------------------------------------------------------------------"
            "-\n";
    if (is_non_linear) {
        info += tr("Iterations") + " = " + QString::number(iterations) + "\n";
        info += tr("Status") + " = " + gsl_strerror(status) + "\n";
        info += "----------------------------------------------------------------------------------"
                "-----\n";
    }
    return info;
}

double Fit::reducedChiSquare() const
{
    int p_eff = effectiveFreeParameters();
    if (int(d_x.size()) <= p_eff)
        return chi_2;
    return chi_2 / double(int(d_x.size()) - p_eff);
}

double Fit::rss() const
{
    if (d_y_fit.size() == d_y.size() && !d_y.empty()) {
        double sum = 0.0;
        for (size_t i = 0; i < d_y.size(); ++i) {
            double r = d_y[i] - d_y_fit[i];
            sum += r * r;
        }
        return sum;
    }
    return chi_2 >= 0.0 ? chi_2 : 0.0;
}

double Fit::rSquare() const
{
    if (d_has_custom_r2)
        return d_custom_r2;

    if (d_y.empty())
        return 0.0;

    double mean = 0.0, tss = 0.0, weights_sum = 0.0;

    if (d_y_error_source == UnknownErrors) {
        mean = std::accumulate(d_y.cbegin(), d_y.cend(), 0.0) / double(d_y.size());
        for (auto it = d_y.cbegin(); d_y.cend() != it; ++it)
            tss += (*it - mean) * (*it - mean);
    } else {
        for (unsigned i = 0; i < d_y.size(); i++) {
            mean += d_y[i] / (d_y_errors[i] * d_y_errors[i]);
            weights_sum += 1.0 / (d_y_errors[i] * d_y_errors[i]);
        }
        if (weights_sum > 0.0)
            mean /= weights_sum;
        for (unsigned i = 0; i < d_y.size(); i++)
            tss += (d_y[i] - mean) * (d_y[i] - mean) / (d_y_errors[i] * d_y_errors[i]);
    }
    if (tss <= 0.0)
        return 1.0;
    return 1.0 - chi_2 / tss;
}

double Fit::adjustedRSquare() const
{
    double r2 = rSquare();
    int p_eff = effectiveFreeParameters();
    if (int(d_x.size()) <= p_eff || d_x.size() <= 1)
        return r2;
    return 1.0 - (1.0 - r2) * double(d_x.size() - 1) / double(int(d_x.size()) - p_eff);
}

double Fit::pearsonR() const
{
    if (d_y_fit.size() == d_y.size() && d_y.size() > 1) {
        double mean_y = std::accumulate(d_y.cbegin(), d_y.cend(), 0.0) / double(d_y.size());
        double mean_f = std::accumulate(d_y_fit.cbegin(), d_y_fit.cend(), 0.0) / double(d_y_fit.size());
        double s_yf = 0.0, s_yy = 0.0, s_ff = 0.0;
        for (size_t i = 0; i < d_y.size(); ++i) {
            double dy = d_y[i] - mean_y;
            double df = d_y_fit[i] - mean_f;
            s_yf += dy * df;
            s_yy += dy * dy;
            s_ff += df * df;
        }
        if (s_yy > 0.0 && s_ff > 0.0)
            return s_yf / sqrt(s_yy * s_ff);
    }
    double r2 = rSquare();
    return r2 > 0.0 ? sqrt(r2) : 0.0;
}

QString Fit::legendInfo()
{
    QString info = tr("Dataset") + ": " + d_curve->title().text() + "\n";
    info += tr("Function") + ": " + d_formula + "\n\n";

    double chi_2_dof = reducedChiSquare();
    info += "Chi^2/doF = " + QLocale().toString(chi_2_dof, 'g', d_prec) + "\n";
    info += tr("R^2") + " (COD) = " + QLocale().toString(rSquare(), 'g', d_prec) + "\n";
    info += tr("Adj. R^2") + " = " + QLocale().toString(adjustedRSquare(), 'g', d_prec) + "\n";

    for (unsigned i = 0; i < d_p; i++) {
        info += d_param_names[i] + " = " + QLocale().toString(d_results[i], 'g', d_prec) + " +/- ";
        if (d_scale_errors)
            info += QLocale().toString(sqrt(chi_2_dof * gsl_matrix_get(covar, i, i)), 'g', d_prec)
                    + "\n";
        else
            info += QLocale().toString(sqrt(gsl_matrix_get(covar, i, i)), 'g', d_prec) + "\n";
    }
    return info;
}

bool Fit::setYErrorSource(ErrorSource err, const QString &colName, bool fail_silently)
{
    try
    {
        d_y_errors.resize(d_x.size());
    }
    catch (const std::bad_alloc& e)
    {
        if (!fail_silently)
         QMessageBox::critical((ApplicationWindow *)parent(), tr("ToriGraph") + " - " + tr("Error"),
                              tr("Could not allocate memory, operation aborted!\n")
                                      + tr("Allocator returned: ") + e.what());
        d_init_err = true;
        return false;
    }

    d_y_error_source = err;
    switch (d_y_error_source) {
    case UnknownErrors: {
        d_y_error_dataset = QString();
        // using 1.0 here is important for correct error estimates,
        // cmp. Fit::fitGslMultifit and Fit::fitGslMultimin
        std::fill(d_y_errors.begin(),d_y_errors.end(),1.0);
        break;
    }
    case AssociatedErrors: {
        bool error = true;
        QwtErrorPlotCurve *er = 0;
        if (d_curve && ((PlotCurve *)d_curve)->type() != Graph::Function) {
            QList<DataCurve *> lst = ((DataCurve *)d_curve)->errorBarsList();
            foreach (DataCurve *c, lst) {
                er = (QwtErrorPlotCurve *)c;
                if (!er->xErrors()) {
                    d_y_error_dataset = er->title().text();
                    error = false;
                    break;
                }
            }
        }
        if (error) {
            if (!fail_silently)
                QMessageBox::critical((ApplicationWindow *)parent(), tr("Error"),
                                      tr("The curve %1 has no associated Y error bars.")
                                              .arg(d_curve->title().text()));
            return false;
        }
        if (er)
            for (unsigned j = 0; j < d_y_errors.size(); j++)
                d_y_errors[j]=er->errorValue(d_indices[j]);
        break;
    }
    case PoissonErrors: {
        d_y_error_dataset = d_curve->title().text();

        for (unsigned i = 0; i < d_y.size(); i++)
            d_y_errors[i] = sqrt(d_y[i]);
    } break;
    case CustomErrors: { // d_y_errors are equal to the values of the arbitrary dataset
        if (colName.isEmpty())
            return false;

        Table *t = ((ApplicationWindow *)parent())->table(colName);
        if (!t)
            return false;

        if (unsigned(t->numRows()) < d_y_errors.size()) {
            if (!fail_silently)
                QMessageBox::critical((ApplicationWindow *)parent(), tr("Error"),
                                      tr("The column %1 has less points than the fitted data set. "
                                         "Please choose another column!")
                                              .arg(colName));
            return false;
        }

        d_y_error_dataset = colName;

        int col = t->colIndex(colName);
        DataCurve *dc = dynamic_cast<DataCurve *>(d_curve);
        for (unsigned i = 0; i < d_y_errors.size(); i++) {
            int row = int(d_indices[i]);
            if (dc && dc->table() == t) {
                int trRow = dc->tableRow(int(d_indices[i]));
                if (trRow >= 0)
                    row = trRow;
            }
            if (row < 0 || row >= t->numRows()) {
                if (!fail_silently && !d_silent)
                    QMessageBox::critical((ApplicationWindow *)parent(), tr("Error"),
                                          tr("The column %1 has less points than the fitted data set. "
                                             "Please choose another column!")
                                                  .arg(colName));
                return false;
            }
            d_y_errors[i] = t->cell(row, col);
        }
    } break;
    }
    return true;
}

Table *Fit::parametersTable(const QString &tableName)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    Table *t = app->newTable(tableName, d_p, 3);
    t->setHeader(QStringList() << tr("Parameter") << tr("Value") << tr("Error"));
    t->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
    t->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    t->column(2)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    for (unsigned i = 0; i < d_p; i++) {
        t->column(0)->setTextAt(i, d_param_names[i]);
        t->column(1)->setValueAt(i, d_results[i]);
        t->column(2)->setValueAt(i, sqrt(gsl_matrix_get(covar, i, i)));
    }

    t->column(2)->setPlotDesignation(SciDAVis::yErr);
// TODO: replace or remove this
#if 0
	for (int j=0; j<3; j++)
		t->table()->adjustColumn(j);
#endif
    t->showNormal();
    return t;
}

Matrix *Fit::covarianceMatrix(const QString &matrixName)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    Matrix *m = app->newMatrix(matrixName, d_p, d_p);
    for (unsigned i = 0; i < d_p; i++) {
        for (unsigned j = 0; j < d_p; j++)
            m->setText(i, j, QLocale().toString(gsl_matrix_get(covar, i, j), 'g', d_prec));
    }
    m->showNormal();
    return m;
}

Table *Fit::createResidualsTable(const QString &tableName)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (!app || d_init_err || d_x.empty() || d_y.size() != d_x.size())
        return nullptr;

    if (d_y_fit.size() != d_x.size() && !d_results.empty()) {
        bool old_gen = d_gen_function;
        int old_pts = d_points;
        d_gen_function = false;
        d_points = int(d_x.size());
        std::vector<double> X_tmp(d_x.size()), Y_tmp(d_x.size());
        if (calculateFitCurveData(d_results, X_tmp, Y_tmp)) {
            d_y_fit = Y_tmp;
        }
        d_gen_function = old_gen;
        d_points = old_pts;
    }

    if (d_y_fit.size() != d_x.size())
        return nullptr;

    QString name = tableName.isEmpty() ? app->generateUniqueName(tr("Residuals"))
                                       : app->generateUniqueName(tableName, false);
    int rows = int(d_x.size());
    Table *t = app->newTable(name, rows, 4);
    if (!t)
        return nullptr;

    t->setHeader(QStringList() << tr("X") << tr("Y_Obs") << tr("Y_Fit") << tr("Residual"));
    for (int col = 0; col < 4; ++col)
        t->column(col)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    t->column(0)->setPlotDesignation(SciDAVis::X);
    t->column(1)->setPlotDesignation(SciDAVis::Y);
    t->column(2)->setPlotDesignation(SciDAVis::Y);
    t->column(3)->setPlotDesignation(SciDAVis::Y);

    for (int i = 0; i < rows; ++i) {
        t->column(0)->setValueAt(i, d_x[i]);
        t->column(1)->setValueAt(i, d_y[i]);
        t->column(2)->setValueAt(i, d_y_fit[i]);
        t->column(3)->setValueAt(i, d_y[i] - d_y_fit[i]);
    }

    t->showNormal();
    return t;
}

const vector<double> &Fit::errors()
{
    if (d_result_errors.empty()) {
        d_result_errors.resize(d_p);
        double chi_2_dof = reducedChiSquare();
        for (unsigned i = 0; i < d_p; i++) {
            if (d_scale_errors)
                d_result_errors[i] = sqrt(chi_2_dof * gsl_matrix_get(covar, i, i));
            else
                d_result_errors[i] = sqrt(gsl_matrix_get(covar, i, i));
        }
    }
    return d_result_errors;
}

void Fit::fit()
{
    if (!d_graph || d_init_err)
        return;

    if (d_x.empty()) {
        if (!d_silent)
            QMessageBox::critical((ApplicationWindow *)parent(), tr("Fit Error"),
                                  tr("You didn't specify a valid data set for this fit operation. "
                                     "Operation aborted!"));
        return;
    }
    if (!d_p) {
        if (!d_silent)
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Fit Error"),
                    tr("There are no parameters specified for this fit operation. Operation aborted!"));
        return;
    }
    int minNeeded = std::max(1, effectiveFreeParameters());
    if (unsigned(minNeeded) > d_x.size()) {
        if (!d_silent)
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Fit Error"),
                    tr("You need at least %1 data points for this fit operation. Operation aborted!")
                            .arg(minNeeded));
        return;
    }
    if (d_formula.isEmpty()) {
        if (!d_silent)
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Fit Error"),
                    tr("You must specify a valid fit function first. Operation aborted!"));
        return;
    }

    d_has_custom_r2 = false;
    if (!d_all_guesses_set && unsigned(d_explicit_init_vals.size()) < d_p) {
        guessInitialValues();
        applyExplicitInitialGuesses();
    } else if (!d_initial_guesses_set) {
        guessInitialValues();
    } else {
        applyExplicitInitialGuesses();
    }

    d_result_errors.clear();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    int status, iterations;
    vector<double> par;
    d_script.reset(scriptEnv->newScript(d_formula, this, metaObject()->className()));
    connect(d_script.get(), SIGNAL(error(const QString &, const QString &, int)), this,
            SLOT(scriptError(const QString &, const QString &, int)));

    if (d_solver == NelderMeadSimplex)
        par = fitGslMultimin(iterations, status);
    else
        par = fitGslMultifit(iterations, status);

    storeCustomFitResults(par);
    if (status == GSL_SUCCESS && !d_suppress_curve_output) {
        generateFitCurve(par);
        if (d_conf_band || d_pred_band)
            addConfidenceAndPredictionBands(par);
    }

    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (app && app->writeFitResultsToLog && !d_silent)
        app->updateLog(logFitInfo(d_results, iterations, status, d_graph->parentPlotName()));
    disconnect(d_script.get(), SIGNAL(error(const QString &, const QString &, int)), this,
               SLOT(scriptError(const QString &, const QString &, int)));
    QApplication::restoreOverrideCursor();
}

void Fit::scriptError(const QString &message, const QString &script_name, int line_number)
{
    if (!d_silent)
        QMessageBox::critical(qobject_cast<QWidget *>(parent()), tr("Input function error"),
                              QString("%1:%2\n").arg(script_name).arg(line_number) + message);
}

int Fit::evaluate_f(const gsl_vector *x, gsl_vector *f)
{
    for (unsigned i = 0; i < d_p; i++) {
        d_script->setDouble(gsl_vector_get(x, i), d_param_names[i].toUtf8());
    }
    for (unsigned j = 0; j < d_x.size(); j++) {
        if (!d_script->setDouble(d_x[j], "x"))
            return GSL_EINVAL;
        bool success;
        auto y = d_script->eval();
        if (y.isNull())
            return GSL_EINVAL;
        gsl_vector_set(f, j, (y.toDouble(&success) - d_y[j]) / d_y_errors[j]);
        if (!success)
            return GSL_EINVAL;
    }
    return GSL_SUCCESS;
}

double Fit::evaluate_d(const gsl_vector *x)
{
    double result = 0.0;
    for (unsigned i = 0; i < d_p; i++)
        d_script->setDouble(gsl_vector_get(x, i), d_param_names[i].toUtf8());
    for (unsigned j = 0; j < d_x.size(); j++) {
        d_script->setDouble(d_x[j], "x");
        bool success;
        result += pow((d_script->eval().toDouble(&success) - d_y[j]) / d_y_errors[j], 2);
        if (!success)
            return GSL_EINVAL;
    }
    return result;
}

typedef struct
{
    Script *script;
    QString param;
    bool success;
} DiffData;

double Fit::evaluate_df_helper(double x, void *params)
{
    DiffData *data = static_cast<DiffData *>(params);
    data->script->setDouble(x, (data->param).toUtf8());
    bool success;
    double result = data->script->eval().toDouble(&success);
    if (!success) {
        data->script->disconnect(SIGNAL(error(const QString &, const QString &, int)));
        data->success = false;
    }
    return result;
}

int Fit::evaluate_df(const gsl_vector *x, gsl_matrix *J)
{
    double result, abserr;
    gsl_function F;
    F.function = &evaluate_df_helper;
    DiffData data;
    F.params = &data;
    data.script = d_script.get();
    data.success = true;
    for (unsigned i = 0; i < d_p; i++)
        d_script->setDouble(gsl_vector_get(x, i), d_param_names[i].toUtf8());
    for (unsigned i = 0; i < d_x.size(); i++) {
        d_script->setDouble(d_x[i], "x");
        double s = (i < d_y_errors.size() && d_y_errors[i] > 0.0) ? d_y_errors[i] : 1.0;
        for (unsigned j = 0; j < d_p; j++) {
            if (isParameterFixed(int(j))) {
                gsl_matrix_set(J, i, j, 0.0);
                continue;
            }
            double xj = gsl_vector_get(x, j);
            double h = std::max(1e-8, 1e-6 * std::abs(xj));
            data.param = d_param_names[j];
            gsl_deriv_central(&F, xj, h, &result, &abserr);
            d_script->setDouble(xj, d_param_names[j].toUtf8());
            if (!data.success)
                return GSL_EINVAL;
            gsl_matrix_set(J, i, j, result / s);
        }
    }
    return GSL_SUCCESS;
}

void Fit::generateFitCurve(const vector<double> &par)
{
    if (d_suppress_curve_output)
        return;

    if (!d_gen_function)
        d_points = d_x.size();

    std::vector<double> X(d_points);
    std::vector<double> Y(d_points);

    if (!calculateFitCurveData(par, X, Y)) {
        if (!d_silent)
            QMessageBox::critical(0, tr("Fit failed"), tr("An error occurred during fit!"));
        return;
    }

    if (d_gen_function) {
        insertFitFunctionCurve(objectName() + tr("Fit"), X, Y);
        d_graph->replot();
    } else
        d_graph->addFitCurve(addResultCurve(X, Y));
}

void Fit::insertFitFunctionCurve(const QString &name, std::vector<double> &x,
                                 std::vector<double> &y, int penWidth)
{
    QString title = d_graph->generateFunctionName(name);
    FunctionCurve *c =
            new FunctionCurve((ApplicationWindow *)parent(), FunctionCurve::Normal, title);
    c->setPen(QPen(d_curveColor, penWidth));
    // "Set data by copying x- and y-values from specified memory blocks."
    c->setData(x.data(), y.data(), d_points);
    c->setRange(d_x.front(), d_x.back());

    QString formula;
    for (unsigned j = 0; j < d_p; j++)
        formula += QString("%1=%2\n").arg(d_param_names[j]).arg(d_results[j], 0, 'g', d_prec);
    formula += "\n";
    formula += d_formula;
    c->setFormula(formula);
    d_graph->insertPlotItem(c, Graph::Line);
    d_graph->addFitCurve(c);
}

Fit::~Fit()
{
    if (!d_p)
        return;

    if (is_non_linear)
        gsl_vector_free(d_param_init);

    gsl_matrix_free(covar);
}

void Fit::generateX(std::vector<double> &X) const
{
    if (d_gen_function) {
        double X0 = d_x.front();
        double step = (d_points > 1) ? ((d_x.back() - X0) / (d_points - 1)) : 0.0;
        for (int i = 0; i < d_points; i++)
            X[i] = X0 + i * step;
    } else
        for (int i = 0; i < d_points; i++)
            X[i] = d_x[i];
}

void Fit::ensureConstraintCapacity(int index)
{
    if (index < 0)
        return;
    size_t req = std::max<size_t>(size_t(d_p), size_t(index + 1));
    if (d_param_fixed.size() < req)
        d_param_fixed.resize(req, false);
    if (d_param_bounded.size() < req)
        d_param_bounded.resize(req, false);
    if (d_param_min.size() < req)
        d_param_min.resize(req, -std::numeric_limits<double>::infinity());
    if (d_param_max.size() < req)
        d_param_max.resize(req, std::numeric_limits<double>::infinity());
}

void Fit::applyExplicitInitialGuesses()
{
    if (!d_param_init)
        return;
    for (auto it = d_explicit_init_vals.constBegin(); it != d_explicit_init_vals.constEnd(); ++it) {
        int idx = it.key();
        if (idx >= 0 && unsigned(idx) < d_p)
            gsl_vector_set(d_param_init, idx, toInternalParameter(idx, it.value()));
    }
}

void Fit::setParameterFixed(int index, bool fixed)
{
    if (index < 0)
        return;
    ensureConstraintCapacity(index);
    d_param_fixed[size_t(index)] = fixed;
}

bool Fit::isParameterFixed(int index) const
{
    if (index < 0 || size_t(index) >= d_param_fixed.size())
        return false;
    return d_param_fixed[size_t(index)];
}

void Fit::setParameterBounds(int index, double minVal, double maxVal)
{
    if (index < 0)
        return;
    ensureConstraintCapacity(index);
    if (minVal > maxVal)
        std::swap(minVal, maxVal);
    d_param_bounded[size_t(index)] = true;
    d_param_min[size_t(index)] = minVal;
    d_param_max[size_t(index)] = maxVal;
}

void Fit::clearParameterBounds(int index)
{
    if (index < 0 || size_t(index) >= d_param_bounded.size())
        return;
    d_param_bounded[size_t(index)] = false;
    d_param_min[size_t(index)] = -std::numeric_limits<double>::infinity();
    d_param_max[size_t(index)] = std::numeric_limits<double>::infinity();
}

bool Fit::hasParameterBounds(int index, double *minOut, double *maxOut) const
{
    if (index < 0 || size_t(index) >= d_param_bounded.size() || !d_param_bounded[size_t(index)])
        return false;
    if (minOut)
        *minOut = d_param_min[size_t(index)];
    if (maxOut)
        *maxOut = d_param_max[size_t(index)];
    return true;
}

int Fit::effectiveFreeParameters() const
{
    int count = 0;
    for (unsigned j = 0; j < d_p; ++j) {
        if (!isParameterFixed(int(j)))
            ++count;
    }
    return count;
}

bool Fit::hasActiveConstraints() const
{
    for (unsigned j = 0; j < d_p; ++j) {
        if (isParameterFixed(int(j)) || hasParameterBounds(int(j)))
            return true;
    }
    return false;
}

void Fit::enableConfidenceBand(bool on, double confLevel)
{
    d_conf_band = on;
    if (confLevel > 0.0 && confLevel < 1.0)
        d_conf_level = confLevel;
}

void Fit::enablePredictionBand(bool on, double confLevel)
{
    d_pred_band = on;
    if (confLevel > 0.0 && confLevel < 1.0)
        d_pred_level = confLevel;
}

void Fit::addConfidenceAndPredictionBands(const std::vector<double> &solverPar)
{
    if ((!d_conf_band && !d_pred_band) || !d_graph || d_suppress_curve_output || d_x.empty()
        || !covar || solverPar.size() < d_p)
        return;

    int savedPoints = d_points;
    int nPts = d_gen_function ? std::max(2, d_points) : int(d_x.size());
    d_points = nPts;

    std::vector<double> X_band(nPts), Y_hat(nPts);
    if (!evaluateModelCurve(solverPar, X_band, Y_hat)) {
        d_points = savedPoints;
        return;
    }

    std::vector<std::vector<double>> G(nPts, std::vector<double>(d_p, 0.0));
    std::vector<double> X_tmp(nPts), Y_plus(nPts), Y_minus(nPts);
    for (unsigned j = 0; j < d_p; ++j) {
        if (isParameterFixed(int(j)))
            continue;
        double pj = solverPar[j];
        double h = std::max(1e-7, 1e-5 * std::abs(pj));
        std::vector<double> p_plus = solverPar;
        std::vector<double> p_minus = solverPar;
        p_plus[j] = pj + h;
        p_minus[j] = pj - h;
        if (evaluateModelCurve(p_plus, X_tmp, Y_plus)
            && evaluateModelCurve(p_minus, X_tmp, Y_minus)) {
            for (int k = 0; k < nPts; ++k)
                G[k][j] = (Y_plus[k] - Y_minus[k]) / (2.0 * h);
        }
    }
    d_points = savedPoints;

    int p_free = std::max(1, effectiveFreeParameters());
    double dof = (int(d_x.size()) > p_free) ? double(int(d_x.size()) - p_free) : 1.0;
    double s2 = std::max(0.0, rss() / dof);

    double covScale = 1.0;
    if (d_y_error_source != UnknownErrors && d_scale_errors)
        covScale = std::max(0.0, reducedChiSquare());

    double confAlpha = std::max(0.50, std::min(0.9999, d_conf_level));
    double predAlpha = std::max(0.50, std::min(0.9999, d_pred_level));
    double t_conf = gsl_cdf_tdist_Pinv(0.5 * (1.0 + confAlpha), dof);
    double t_pred = gsl_cdf_tdist_Pinv(0.5 * (1.0 + predAlpha), dof);

    std::vector<double> confUpper(nPts), confLower(nPts), predUpper(nPts), predLower(nPts);
    for (int k = 0; k < nPts; ++k) {
        double var_mean = 0.0;
        for (unsigned a = 0; a < d_p; ++a) {
            if (G[k][a] == 0.0)
                continue;
            for (unsigned b = 0; b < d_p; ++b)
                var_mean += G[k][a] * (covScale * gsl_matrix_get(covar, a, b)) * G[k][b];
        }
        var_mean = std::max(0.0, var_mean);
        double var_pred = std::max(0.0, s2 + var_mean);

        confUpper[k] = Y_hat[k] + t_conf * std::sqrt(var_mean);
        confLower[k] = Y_hat[k] - t_conf * std::sqrt(var_mean);
        predUpper[k] = Y_hat[k] + t_pred * std::sqrt(var_pred);
        predLower[k] = Y_hat[k] - t_pred * std::sqrt(var_pred);
    }

    auto insertBandCurve = [this](const QString &exactTitle, const std::vector<double> &Xb,
                                  const std::vector<double> &Yb, const QColor &col) {
        if (d_graph->curveIndex(exactTitle) >= 0)
            d_graph->removeCurve(exactTitle);
        FunctionCurve *c = new FunctionCurve((ApplicationWindow *)parent(), FunctionCurve::Normal,
                                             exactTitle);
        c->setPen(QPen(col, 1, Qt::DashLine));
        c->setData(Xb.data(), Yb.data(), int(Xb.size()));
        if (!Xb.empty())
            c->setRange(Xb.front(), Xb.back());
        d_graph->insertPlotItem(c, Graph::Line);
        d_graph->addFitCurve(c);
    };

    QString baseName = d_curve ? d_curve->title().text() : objectName();
    if (d_conf_band) {
        insertBandCurve(baseName + "_ConfUpper", X_band, confUpper, QColor(Qt::red));
        insertBandCurve(baseName + "_ConfLower", X_band, confLower, QColor(Qt::red));
    }
    if (d_pred_band) {
        insertBandCurve(baseName + "_PredUpper", X_band, predUpper, QColor(255, 140, 0));
        insertBandCurve(baseName + "_PredLower", X_band, predLower, QColor(255, 140, 0));
    }
    d_graph->replot();
}

double Fit::aic() const
{
    int n = int(d_x.size());
    if (n <= 0)
        return 0.0;
    int k = std::max(1, effectiveFreeParameters()) + 1;
    double rssVal = std::max(1e-300, rss());
    return double(n) * std::log(rssVal / double(n)) + 2.0 * double(k);
}

double Fit::aicc() const
{
    int n = int(d_x.size());
    if (n <= 0)
        return 0.0;
    int k = std::max(1, effectiveFreeParameters()) + 1;
    double baseAic = aic();
    if (n > k + 1)
        return baseAic + (2.0 * double(k) * double(k + 1)) / double(n - k - 1);
    return baseAic + 2.0 * double(k) * double(k + 1);
}

double Fit::bic() const
{
    int n = int(d_x.size());
    if (n <= 0)
        return 0.0;
    int k = std::max(1, effectiveFreeParameters()) + 1;
    double rssVal = std::max(1e-300, rss());
    return double(n) * std::log(rssVal / double(n)) + double(k) * std::log(std::max(1.0, double(n)));
}

double Fit::anovaFValue() const
{
    int n = int(d_x.size());
    if (n <= 1 || d_y.empty())
        return 0.0;

    double mean = std::accumulate(d_y.cbegin(), d_y.cend(), 0.0) / double(n);
    double tss = 0.0;
    for (double yv : d_y) {
        double d = yv - mean;
        tss += d * d;
    }
    double rssVal = std::max(0.0, rss());
    double mss = std::max(0.0, tss - rssVal);

    int p_free = std::max(1, effectiveFreeParameters());
    int df_model = (p_free > 1) ? (p_free - 1) : 1;
    int df_err = std::max(1, n - p_free);

    double ms_model = mss / double(df_model);
    double ms_err = rssVal / double(df_err);
    if (ms_err <= 1e-300)
        return (ms_model > 0.0) ? 1e15 : 0.0;
    return ms_model / ms_err;
}

double Fit::anovaPValue() const
{
    int n = int(d_x.size());
    if (n <= 1 || d_y.empty())
        return 1.0;

    int p_free = std::max(1, effectiveFreeParameters());
    int df_model = (p_free > 1) ? (p_free - 1) : 1;
    int df_err = std::max(1, n - p_free);

    double F = anovaFValue();
    if (!std::isfinite(F) || F <= 0.0)
        return 1.0;
    if (F >= 1e14)
        return 0.0;
    double p = gsl_cdf_fdist_Q(F, double(df_model), double(df_err));
    return std::max(0.0, std::min(1.0, p));
}

void Fit::insertFitSummaryBoxOnGraph()
{
    if (!d_graph)
        return;

    QString curveName = d_curve ? d_curve->title().text() : objectName();
    QString text = tr("Model") + ": " + objectName();
    if (!d_formula.isEmpty())
        text += " (" + d_formula + ")";
    text += "\n" + tr("Dataset") + ": " + curveName + "\n";
    text += "----------------------------------\n";

    double chi2Dof = reducedChiSquare();
    for (unsigned i = 0; i < d_p && i < d_results.size(); ++i) {
        QString pName = (int(i) < d_param_names.size()) ? d_param_names[int(i)]
                                                        : QString("p%1").arg(i);
        text += pName + " = " + QLocale().toString(d_results[i], 'g', d_prec);
        if (isParameterFixed(int(i))) {
            text += " (" + tr("fixed") + ")\n";
        } else if (covar) {
            double err = d_scale_errors ? std::sqrt(std::max(0.0, chi2Dof * gsl_matrix_get(covar, i, i)))
                                        : std::sqrt(std::max(0.0, gsl_matrix_get(covar, i, i)));
            text += " +/- " + QLocale().toString(err, 'g', d_prec) + "\n";
        } else {
            text += "\n";
        }
    }
    text += "----------------------------------\n";
    text += tr("Reduced Chi^2") + " = " + QLocale().toString(chi2Dof, 'g', d_prec) + "\n";
    text += tr("R^2 (COD)") + " = " + QLocale().toString(rSquare(), 'g', d_prec) + "\n";
    text += tr("Adj. R^2") + " = " + QLocale().toString(adjustedRSquare(), 'g', d_prec) + "\n";
    text += "AIC = " + QLocale().toString(aic(), 'g', d_prec)
            + ", BIC = " + QLocale().toString(bic(), 'g', d_prec) + "\n";
    text += "ANOVA F = " + QLocale().toString(anovaFValue(), 'g', d_prec)
            + " (p = " + QLocale().toString(anovaPValue(), 'g', 4) + ")";

    Legend *mrk = d_graph->newLegend(text);
    if (mrk) {
        mrk->setFont(QFont("Arial", 9));
        if (d_graph->hasLegend()) {
            Legend *leg = d_graph->legend();
            if (leg && leg != mrk) {
                QPoint p = leg->rect().bottomLeft();
                mrk->setOrigin(QPoint(p.x(), p.y() + 20));
            }
        }
    }
    d_graph->replot();
}

MultiLayer *Fit::createResidualPlotWindow()
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (!app)
        return nullptr;

    Table *resTable = createResidualsTable();
    if (!resTable || resTable->numRows() <= 0)
        return nullptr;

    QStringList plotCols;
    plotCols << resTable->colName(3);
    MultiLayer *ml = app->multilayerPlot(resTable, plotCols, Graph::Scatter, 0,
                                         resTable->numRows() - 1);
    if (!ml)
        return nullptr;

    ml->setFonts(QFont("Arial", 11, QFont::Bold), QFont("Arial", 10, QFont::Bold),
                 QFont("Arial", 9), QFont("Arial", 9));
    Graph *rg = ml->activeGraph();
    if (rg) {
        QString srcTitle = d_curve ? d_curve->title().text() : objectName();
        rg->setTitle(tr("Residuals of %1 (%2)").arg(srcTitle, objectName()));
        rg->setTitleFont(QFont("Arial", 11, QFont::Bold));
        rg->setXAxisTitle(tr("X"));
        rg->setYAxisTitle(tr("Residual (Y - Y_Fit)"));
        rg->initFonts(QFont("Arial", 10, QFont::Bold), QFont("Arial", 9));
        if (rg->legend())
            rg->legend()->setFont(QFont("Arial", 9));
        if (!d_x.empty()) {
            double xMin = *std::min_element(d_x.begin(), d_x.end());
            double xMax = *std::max_element(d_x.begin(), d_x.end());
            if (xMax > xMin) {
                FunctionCurve *zeroLine =
                        new FunctionCurve(app, FunctionCurve::Normal, tr("ZeroResidualLine"));
                zeroLine->setPen(QPen(Qt::darkGray, 1, Qt::DashLine));
                double zx[2] = { xMin, xMax };
                double zy[2] = { 0.0, 0.0 };
                zeroLine->setData(zx, zy, 2);
                zeroLine->setRange(xMin, xMax);
                zeroLine->setFormula("0");
                rg->insertPlotItem(zeroLine, Graph::Line);
            }
        }
        rg->replot();
    }
    return ml;
}

Table *Fit::compareModels(ApplicationWindow *app, Graph *g, const QString &curveTitle,
                          const QStringList &modelNames)
{
    if (!app || !g || curveTitle.isEmpty() || modelNames.isEmpty())
        return nullptr;
    if (g->curveIndex(curveTitle) < 0)
        return nullptr;

    struct ModelEvalRow
    {
        QString name;
        int numParams = 0;
        double rssVal = 0.0;
        double r2Val = 0.0;
        double adjR2Val = 0.0;
        double aicVal = 0.0;
        double aiccVal = 0.0;
        double bicVal = 0.0;
        double deltaAic = 0.0;
        double akaikeWeight = 0.0;
    };

    std::vector<ModelEvalRow> evalRows;
    evalRows.reserve(size_t(modelNames.size()));

    for (const QString &rawName : modelNames) {
        QString mName = rawName.trimmed();
        if (mName.isEmpty())
            continue;

        std::unique_ptr<Fit> fitter;
        QRegExp polyRx("^(?:poly(?:nomial)?)[_\\s]*(\\d+)$", Qt::CaseInsensitive);
        if (mName.compare("Linear", Qt::CaseInsensitive) == 0
            || mName.compare("Line", Qt::CaseInsensitive) == 0
            || mName.compare("Poly1", Qt::CaseInsensitive) == 0) {
            fitter.reset(new LinearFit(app, g));
        } else if (mName.compare("Quadratic", Qt::CaseInsensitive) == 0
                   || mName.compare("Parabola", Qt::CaseInsensitive) == 0) {
            fitter.reset(new PolynomialFit(app, g, 2, false));
        } else if (mName.compare("Cubic", Qt::CaseInsensitive) == 0) {
            fitter.reset(new PolynomialFit(app, g, 3, false));
        } else if (mName.compare("Quartic", Qt::CaseInsensitive) == 0) {
            fitter.reset(new PolynomialFit(app, g, 4, false));
        } else if (polyRx.exactMatch(mName)) {
            int ord = std::max(1, polyRx.cap(1).toInt());
            if (ord == 1)
                fitter.reset(new LinearFit(app, g));
            else
                fitter.reset(new PolynomialFit(app, g, ord, false));
        } else {
            std::unique_ptr<NonLinearFit> nlf(new NonLinearFit(app, g));
            if (nlf->setBuiltInModel(mName)) {
                fitter = std::move(nlf);
            } else if (mName.compare("Gauss", Qt::CaseInsensitive) == 0) {
                fitter.reset(new GaussFit(app, g));
            } else if (mName.compare("Lorentz", Qt::CaseInsensitive) == 0) {
                fitter.reset(new LorentzFit(app, g));
            } else if (mName.compare("GaussAmp", Qt::CaseInsensitive) == 0) {
                fitter.reset(new GaussAmpFit(app, g));
            } else if (mName.compare("PseudoVoigt", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::PseudoVoigt, 1));
            } else if (mName.compare("PearsonVII", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::PearsonVII, 1));
            } else if (mName.compare("Voigt", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::Voigt, 1));
            } else if (mName.compare("BiGaussian", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::BiGaussian, 1));
            } else if (mName.compare("EMG", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::EMG, 1));
            } else if (mName.compare("LogNormal", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::LogNormal, 1));
            } else if (mName.compare("Asym2Sig", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::Asym2Sig, 1));
            } else if (mName.compare("DoniachSunjic", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::DoniachSunjic, 1));
            } else if (mName.compare("Boltzmann", Qt::CaseInsensitive) == 0
                       || mName.compare("Sigmoidal", Qt::CaseInsensitive) == 0) {
                fitter.reset(new SigmoidalFit(app, g));
            } else if (mName.compare("ExpDecay", Qt::CaseInsensitive) == 0
                       || mName.compare("ExpDecay1", Qt::CaseInsensitive) == 0) {
                fitter.reset(new ExponentialFit(app, g, false));
            } else if (mName.compare("ExpDecay2", Qt::CaseInsensitive) == 0) {
                fitter.reset(new TwoExpFit(app, g));
            } else if (mName.compare("ExpDecay3", Qt::CaseInsensitive) == 0) {
                fitter.reset(new ThreeExpFit(app, g));
            } else if (mName.compare("ExpGrowth", Qt::CaseInsensitive) == 0) {
                fitter.reset(new ExponentialFit(app, g, true));
            } else {
                continue;
            }
        }

        fitter->d_silent = true;
        fitter->d_suppress_curve_output = true;
        if (!fitter->setDataFromCurve(curveTitle, g) || fitter->error())
            continue;

        fitter->fit();
        if (fitter->results().empty() || !std::isfinite(fitter->rss()))
            continue;

        ModelEvalRow row;
        row.name = rawName;
        row.numParams = fitter->effectiveFreeParameters();
        row.rssVal = fitter->rss();
        row.r2Val = fitter->rSquare();
        row.adjR2Val = fitter->adjustedRSquare();
        row.aicVal = fitter->aic();
        row.aiccVal = fitter->aicc();
        row.bicVal = fitter->bic();
        evalRows.push_back(row);
    }

    if (evalRows.empty())
        return nullptr;

    std::sort(evalRows.begin(), evalRows.end(),
              [](const ModelEvalRow &a, const ModelEvalRow &b) { return a.aicVal < b.aicVal; });

    double minAic = evalRows.front().aicVal;
    double weightSum = 0.0;
    for (auto &r : evalRows) {
        r.deltaAic = r.aicVal - minAic;
        r.akaikeWeight = std::exp(-0.5 * std::min(700.0, r.deltaAic));
        weightSum += r.akaikeWeight;
    }
    if (weightSum > 0.0) {
        for (auto &r : evalRows)
            r.akaikeWeight /= weightSum;
    }

    QString tblName = app->generateUniqueName(tr("ModelComparison"));
    int nRows = int(evalRows.size());
    Table *tbl = app->newTable(tblName, nRows, 11);
    if (!tbl)
        return nullptr;

    tbl->setHeader(QStringList() << tr("Model") << tr("NumParams") << tr("RSS") << tr("R2")
                                 << tr("AdjR2") << tr("AIC") << tr("AICc") << tr("BIC")
                                 << tr("DeltaAIC") << tr("AkaikeWeight") << tr("Rank"));
    tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
    for (int c = 1; c < 11; ++c)
        tbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);

    for (int i = 0; i < nRows; ++i) {
        const auto &r = evalRows[size_t(i)];
        tbl->column(0)->setTextAt(i, r.name);
        tbl->column(1)->setValueAt(i, r.numParams);
        tbl->column(2)->setValueAt(i, r.rssVal);
        tbl->column(3)->setValueAt(i, r.r2Val);
        tbl->column(4)->setValueAt(i, r.adjR2Val);
        tbl->column(5)->setValueAt(i, r.aicVal);
        tbl->column(6)->setValueAt(i, r.aiccVal);
        tbl->column(7)->setValueAt(i, r.bicVal);
        tbl->column(8)->setValueAt(i, r.deltaAic);
        tbl->column(9)->setValueAt(i, r.akaikeWeight);
        tbl->column(10)->setValueAt(i, i + 1);
    }

    tbl->showNormal();
    return tbl;
}
