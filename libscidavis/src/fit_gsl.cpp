/***************************************************************************
   File                 : fit_gsl.cpp
   Project              : SciDAVis
   Description          : Built-in data fit models for QtiPlot
   --------------------------------------------------------------------
   Copyright            : (C) 2004-2007 Ion Vasilief (ion_vasilief*yahoo.fr)
                          (C) 2008-2009 Knut Franke (knut.franke*gmx.de)
                                                  (replace * with @ in the email address)
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  As a special exception, you may omit the above copyright notice when   *
 *  distributing modified copies of this file (for instance, when using it *
 *  as a template for your own fit plugin).                                *
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

#include <cmath>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include <qmessagebox.h>

#include <gsl/gsl_blas.h>
#include <gsl/gsl_math.h>
#include "fit_gsl.h"
#include "Fit.h"

static inline bool fit_gsl_invalid_sigma(const double *sigma, size_t i)
{
    if (!sigma)
        return false;
    const double s = sigma[i];
    return !std::isfinite(s) || s <= 0.0 || s >= 1e140;
}

static inline void fit_gsl_zero_jac_row(gsl_matrix *J, size_t row, size_t ncols)
{
    for (size_t c = 0; c < ncols; ++c)
        gsl_matrix_set(J, row, c, 0.0);
}

int expd3_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    double A1 = gsl_vector_get(x, 0);
    double t1 = gsl_vector_get(x, 1);
    double A2 = gsl_vector_get(x, 2);
    double t2 = gsl_vector_get(x, 3);
    double A3 = gsl_vector_get(x, 4);
    double t3 = gsl_vector_get(x, 5);
    double y0 = gsl_vector_get(x, 6);

    size_t i;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double Yi = A1 * exp(-X[i] * t1) + A2 * exp(-X[i] * t2) + A3 * exp(-X[i] * t3) + y0;
        gsl_vector_set(f, i, (Yi - Y[i]) / s);
    }

    return GSL_SUCCESS;
}

double expd3_d(const gsl_vector *x, void *params)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    double A1 = gsl_vector_get(x, 0);
    double t1 = gsl_vector_get(x, 1);
    double A2 = gsl_vector_get(x, 2);
    double t2 = gsl_vector_get(x, 3);
    double A3 = gsl_vector_get(x, 4);
    double t3 = gsl_vector_get(x, 5);
    double y0 = gsl_vector_get(x, 6);

    size_t i;
    double val = 0;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double dYi =
                ((A1 * exp(-X[i] * t1) + A2 * exp(-X[i] * t2) + A3 * exp(-X[i] * t3) + y0) - Y[i])
                / s;
        val += dYi * dYi;
    }

    return val;
}

int expd3_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *sigma = ((struct FitData *)params)->sigma;

    double A1 = gsl_vector_get(x, 0);
    double l1 = gsl_vector_get(x, 1);
    double A2 = gsl_vector_get(x, 2);
    double l2 = gsl_vector_get(x, 3);
    double A3 = gsl_vector_get(x, 4);
    double l3 = gsl_vector_get(x, 5);

    size_t i;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, J->size2);
            continue;
        }
        /* Jacobian matrix J(i,j) = dfi / dxj, */
        /* where fi = (Yi - yi)/sigma[i],      */
        /*       Yi = A1 * exp(-xi*l1) + A2 * exp(-xi*l2) +y0  */
        /* and the xj are the parameters (A1,l1,A2,l2,y0) */
        double t = X[i];
        double s = sigma ? sigma[i] : 1.0;
        double e1 = exp(-t * l1) / s;
        double e2 = exp(-t * l2) / s;
        double e3 = exp(-t * l3) / s;

        gsl_matrix_set(J, i, 0, e1);
        gsl_matrix_set(J, i, 1, -t * A1 * e1);
        gsl_matrix_set(J, i, 2, e2);
        gsl_matrix_set(J, i, 3, -t * A2 * e2);
        gsl_matrix_set(J, i, 4, e3);
        gsl_matrix_set(J, i, 5, -t * A3 * e3);
        gsl_matrix_set(J, i, 6, 1 / s);
    }
    return GSL_SUCCESS;
}

int expd3_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    expd3_f(x, params, f);
    expd3_df(x, params, J);

    return GSL_SUCCESS;
}

int expd2_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    double A1 = gsl_vector_get(x, 0);
    double t1 = gsl_vector_get(x, 1);
    double A2 = gsl_vector_get(x, 2);
    double t2 = gsl_vector_get(x, 3);
    double y0 = gsl_vector_get(x, 4);

    size_t i;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double Yi = A1 * exp(-X[i] * t1) + A2 * exp(-X[i] * t2) + y0;
        gsl_vector_set(f, i, (Yi - Y[i]) / s);
    }

    return GSL_SUCCESS;
}

double expd2_d(const gsl_vector *x, void *params)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    double A1 = gsl_vector_get(x, 0);
    double t1 = gsl_vector_get(x, 1);
    double A2 = gsl_vector_get(x, 2);
    double t2 = gsl_vector_get(x, 3);
    double y0 = gsl_vector_get(x, 4);

    size_t i;
    double val = 0;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double dYi = ((A1 * exp(-X[i] * t1) + A2 * exp(-X[i] * t2) + y0) - Y[i]) / s;
        val += dYi * dYi;
    }

    return val;
}

int expd2_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *sigma = ((struct FitData *)params)->sigma;

    double A1 = gsl_vector_get(x, 0);
    double l1 = gsl_vector_get(x, 1);
    double A2 = gsl_vector_get(x, 2);
    double l2 = gsl_vector_get(x, 3);

    size_t i;

    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, J->size2);
            continue;
        }
        /* Jacobian matrix J(i,j) = dfi / dxj, */
        /* where fi = (Yi - yi)/sigma[i],      */
        /*       Yi = A1 * exp(-xi*l1) + A2 * exp(-xi*l2) +y0  */
        /* and the xj are the parameters (A1,l1,A2,l2,y0) */
        double s = sigma ? sigma[i] : 1.0;
        double t = X[i];
        double e1 = exp(-t * l1) / s;
        double e2 = exp(-t * l2) / s;

        gsl_matrix_set(J, i, 0, e1);
        gsl_matrix_set(J, i, 1, -t * A1 * e1);
        gsl_matrix_set(J, i, 2, e2);
        gsl_matrix_set(J, i, 3, -t * A2 * e2);
        gsl_matrix_set(J, i, 4, 1 / s);
    }
    return GSL_SUCCESS;
}

int expd2_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    expd2_f(x, params, f);
    expd2_df(x, params, J);
    return GSL_SUCCESS;
}

int exp_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    double A = gsl_vector_get(x, 0);
    double lambda = gsl_vector_get(x, 1);
    double b = gsl_vector_get(x, 2);
    size_t i;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double Yi = A * exp(-lambda * X[i]) + b;
        gsl_vector_set(f, i, (Yi - Y[i]) / s);
    }
    return GSL_SUCCESS;
}

double exp_d(const gsl_vector *x, void *params)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    double A = gsl_vector_get(x, 0);
    double lambda = gsl_vector_get(x, 1);
    double b = gsl_vector_get(x, 2);
    size_t i;
    double val = 0;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double dYi = ((A * exp(-lambda * X[i]) + b) - Y[i]) / s;
        val += dYi * dYi;
    }
    return val;
}

int exp_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *sigma = ((struct FitData *)params)->sigma;

    double A = gsl_vector_get(x, 0);
    double lambda = gsl_vector_get(x, 1);
    size_t i;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, J->size2);
            continue;
        }
        /* Jacobian matrix J(i,j) = dfi / dxj, */
        /* where fi = (Yi - yi)/sigma[i],      */
        /*       Yi = A * exp(-lambda * i) + b  */
        /* and the xj are the parameters (A,lambda,b) */

        double t = X[i];
        double s = sigma ? sigma[i] : 1.0;
        double e = exp(-lambda * t);
        gsl_matrix_set(J, i, 0, e / s);
        gsl_matrix_set(J, i, 1, -t * A * e / s);
        gsl_matrix_set(J, i, 2, 1 / s);
    }
    return GSL_SUCCESS;
}

int exp_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    exp_f(x, params, f);
    exp_df(x, params, J);
    return GSL_SUCCESS;
}

int gauss_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    double Y0 = gsl_vector_get(x, 0);
    double A = gsl_vector_get(x, 1);
    double C = gsl_vector_get(x, 2);
    double w = gsl_vector_get(x, 3);

    size_t i;

    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double diff = X[i] - C;
        double Yi = A * exp(-0.5 * diff * diff / (w * w)) + Y0;
        gsl_vector_set(f, i, (Yi - Y[i]) / s);
    }
    return GSL_SUCCESS;
}

double gauss_d(const gsl_vector *x, void *params)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    double Y0 = gsl_vector_get(x, 0);
    double A = gsl_vector_get(x, 1);
    double C = gsl_vector_get(x, 2);
    double w = gsl_vector_get(x, 3);

    size_t i;
    double val = 0;

    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double diff = X[i] - C;
        double dYi = ((A * exp(-0.5 * diff * diff / (w * w)) + Y0) - Y[i]) / s;
        val += dYi * dYi;
    }
    return val;
}

int gauss_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *sigma = ((struct FitData *)params)->sigma;

    double A = gsl_vector_get(x, 1);
    double C = gsl_vector_get(x, 2);
    double w = gsl_vector_get(x, 3);

    size_t i;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, J->size2);
            continue;
        }
        /* Jacobian matrix J(i,j) = dfi / dxj,	 */
        /* where fi = Yi - yi,					*/
        /* Yi = y=A*exp[-(Xi-xc)^2/(2*w*w)]+B		*/
        /* and the xj are the parameters (B,A,C,w) */

        double s = sigma ? sigma[i] : 1.0;
        double diff = X[i] - C;
        double e = exp(-0.5 * diff * diff / (w * w)) / s;

        gsl_matrix_set(J, i, 0, 1 / s);
        gsl_matrix_set(J, i, 1, e);
        gsl_matrix_set(J, i, 2, diff * A * e / (w * w));
        gsl_matrix_set(J, i, 3, diff * diff * A * e / (w * w * w));
    }
    return GSL_SUCCESS;
}

int gauss_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    gauss_f(x, params, f);
    gauss_df(x, params, J);

    return GSL_SUCCESS;
}

int gauss_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    size_t n = ((struct FitData *)params)->n;
    size_t p = ((struct FitData *)params)->p;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w2 = new double[peaks];
    double offset = gsl_vector_get(x, p - 1);

    size_t i, j;
    for (i = 0; i < peaks; i++) {
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        double wi = gsl_vector_get(x, 3 * i + 2);
        if (fabs(wi) < 1e-15)
            wi = (wi < 0 ? -1e-15 : 1e-15);
        a[i] = sqrt(M_2_PI) * gsl_vector_get(x, 3 * i) / wi;
        w2[i] = wi * wi;
    }
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double res = 0;
        for (j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            res += a[j] * exp(-2 * diff * diff / w2[j]);
        }
        gsl_vector_set(f, i, (res + offset - Y[i]) / s);
    }
    delete[] a;
    delete[] xc;
    delete[] w2;
    return GSL_SUCCESS;
}

double gauss_multi_peak_d(const gsl_vector *x, void *params)
{
    size_t n = ((struct FitData *)params)->n;
    size_t p = ((struct FitData *)params)->p;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w2 = new double[peaks];
    double offset = gsl_vector_get(x, p - 1);

    size_t i, j;
    double val = 0;
    for (i = 0; i < peaks; i++) {
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        double wi = gsl_vector_get(x, 3 * i + 2);
        if (fabs(wi) < 1e-15)
            wi = (wi < 0 ? -1e-15 : 1e-15);
        a[i] = sqrt(M_2_PI) * gsl_vector_get(x, 3 * i) / wi;
        w2[i] = wi * wi;
    }
    double t;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double res = 0;
        for (j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            res += a[j] * exp(-2 * diff * diff / w2[j]);
        }
        t = (res + offset - Y[i]) / s;
        val += t * t;
    }
    delete[] a;
    delete[] xc;
    delete[] w2;
    return val;
}

int gauss_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    size_t n = ((struct FitData *)params)->n;
    size_t p = ((struct FitData *)params)->p;
    double *X = ((struct FitData *)params)->X;
    double *sigma = ((struct FitData *)params)->sigma;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];

    size_t i, j;
    for (i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, p);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        for (j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            double w2 = w[j] * w[j];
            double e = sqrt(M_2_PI) / s * exp(-2 * diff * diff / w2);

            gsl_matrix_set(J, i, 3 * j, e / w[j]);
            gsl_matrix_set(J, i, 3 * j + 1, 4 * diff * a[j] * e / (w2 * w[j]));
            gsl_matrix_set(J, i, 3 * j + 2, a[j] / w2 * e * (4 * diff * diff / w2 - 1));
        }
        gsl_matrix_set(J, i, p - 1, 1.0 / s);
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return GSL_SUCCESS;
}

int gauss_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    gauss_multi_peak_f(x, params, f);
    gauss_multi_peak_df(x, params, J);
    return GSL_SUCCESS;
}

int lorentz_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    size_t n = ((struct FitData *)params)->n;
    size_t p = ((struct FitData *)params)->p;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];
    double offset = gsl_vector_get(x, p - 1);

    size_t i, j;
    for (i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double res = 0;
        for (j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            res += a[j] * w[j] / (4 * diff * diff + w[j] * w[j]);
        }
        gsl_vector_set(f, i, (res + offset - Y[i]) / s);
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return GSL_SUCCESS;
}

double lorentz_multi_peak_d(const gsl_vector *x, void *params)
{
    size_t n = ((struct FitData *)params)->n;
    size_t p = ((struct FitData *)params)->p;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];
    double offset = gsl_vector_get(x, p - 1);

    size_t i, j;
    double val = 0, t;
    for (i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double res = 0;
        for (j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            res += a[j] * w[j] / (4 * diff * diff + w[j] * w[j]);
        }
        t = (res + offset - Y[i]) / s;
        val += t * t;
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return val;
}

int lorentz_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    size_t n = ((struct FitData *)params)->n;
    size_t p = ((struct FitData *)params)->p;
    double *X = ((struct FitData *)params)->X;
    double *sigma = ((struct FitData *)params)->sigma;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];

    size_t i, j;
    for (i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, p);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        for (j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            double w2 = w[j] * w[j];
            double num = 1.0 / (4 * diff * diff + w2);
            double den = 4 * diff * diff - w2;

            gsl_matrix_set(J, i, 3 * j, w[j] * num / s);
            gsl_matrix_set(J, i, 3 * j + 1, 8 * diff * a[j] * w[j] * num * num / s);
            gsl_matrix_set(J, i, 3 * j + 2, den * a[j] * num * num / s);
        }
        gsl_matrix_set(J, i, p - 1, 1.0 / s);
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return GSL_SUCCESS;
}

int lorentz_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    lorentz_multi_peak_f(x, params, f);
    lorentz_multi_peak_df(x, params, J);
    return GSL_SUCCESS;
}

int gauss_amp_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    size_t n = ((struct FitData *)params)->n;
    size_t p = ((struct FitData *)params)->p;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w2 = new double[peaks];
    double offset = gsl_vector_get(x, p - 1);

    for (size_t i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        double wi = gsl_vector_get(x, 3 * i + 2);
        if (fabs(wi) < 1e-15)
            wi = (wi < 0 ? -1e-15 : 1e-15);
        w2[i] = wi * wi;
    }
    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double res = 0.0;
        for (size_t j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            res += a[j] * exp(-0.5 * diff * diff / w2[j]);
        }
        gsl_vector_set(f, i, (res + offset - Y[i]) / s);
    }
    delete[] a;
    delete[] xc;
    delete[] w2;
    return GSL_SUCCESS;
}

double gauss_amp_multi_peak_d(const gsl_vector *x, void *params)
{
    size_t n = ((struct FitData *)params)->n;
    size_t p = ((struct FitData *)params)->p;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w2 = new double[peaks];
    double offset = gsl_vector_get(x, p - 1);

    for (size_t i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        double wi = gsl_vector_get(x, 3 * i + 2);
        if (fabs(wi) < 1e-15)
            wi = (wi < 0 ? -1e-15 : 1e-15);
        w2[i] = wi * wi;
    }
    double val = 0.0;
    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double res = 0.0;
        for (size_t j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            res += a[j] * exp(-0.5 * diff * diff / w2[j]);
        }
        double t = (res + offset - Y[i]) / s;
        val += t * t;
    }
    delete[] a;
    delete[] xc;
    delete[] w2;
    return val;
}

int gauss_amp_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    size_t n = ((struct FitData *)params)->n;
    size_t p = ((struct FitData *)params)->p;
    double *X = ((struct FitData *)params)->X;
    double *sigma = ((struct FitData *)params)->sigma;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];

    for (size_t i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }
    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, p);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        for (size_t j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            double w2 = w[j] * w[j];
            double e = exp(-0.5 * diff * diff / w2) / s;

            gsl_matrix_set(J, i, 3 * j, e);
            gsl_matrix_set(J, i, 3 * j + 1, diff * a[j] * e / w2);
            gsl_matrix_set(J, i, 3 * j + 2, diff * diff * a[j] * e / (w2 * w[j]));
        }
        gsl_matrix_set(J, i, p - 1, 1.0 / s);
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return GSL_SUCCESS;
}

int gauss_amp_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    gauss_amp_multi_peak_f(x, params, f);
    gauss_amp_multi_peak_df(x, params, J);
    return GSL_SUCCESS;
}

int pseudovoigt_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    struct FitData *data = (struct FitData *)params;
    size_t n = data->n;
    size_t p = data->p;
    double *X = data->X;
    double *Y = data->Y;
    double *sigma = data->sigma;
    double mu = data->fit ? data->fit->voigtLorentzFraction() : 0.5;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];
    double offset = gsl_vector_get(x, p - 1);

    for (size_t i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }

    const double kG = 4.0 * M_LN2;
    const double cG = sqrt(kG / M_PI);
    const double cL = 2.0 / M_PI;

    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double res = 0.0;
        for (size_t j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            double wj = w[j];
            double w2 = wj * wj;
            double L = cL * wj / (4.0 * diff * diff + w2);
            double G = (cG / wj) * exp(-kG * diff * diff / w2);
            res += a[j] * (mu * L + (1.0 - mu) * G);
        }
        gsl_vector_set(f, i, (res + offset - Y[i]) / s);
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return GSL_SUCCESS;
}

double pseudovoigt_multi_peak_d(const gsl_vector *x, void *params)
{
    struct FitData *data = (struct FitData *)params;
    size_t n = data->n;
    size_t p = data->p;
    double *X = data->X;
    double *Y = data->Y;
    double *sigma = data->sigma;
    double mu = data->fit ? data->fit->voigtLorentzFraction() : 0.5;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];
    double offset = gsl_vector_get(x, p - 1);

    for (size_t i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }

    const double kG = 4.0 * M_LN2;
    const double cG = sqrt(kG / M_PI);
    const double cL = 2.0 / M_PI;

    double val = 0.0;
    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double res = 0.0;
        for (size_t j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            double wj = w[j];
            double w2 = wj * wj;
            double L = cL * wj / (4.0 * diff * diff + w2);
            double G = (cG / wj) * exp(-kG * diff * diff / w2);
            res += a[j] * (mu * L + (1.0 - mu) * G);
        }
        double t = (res + offset - Y[i]) / s;
        val += t * t;
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return val;
}

int pseudovoigt_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    struct FitData *data = (struct FitData *)params;
    size_t n = data->n;
    size_t p = data->p;
    double *X = data->X;
    double *sigma = data->sigma;
    double mu = data->fit ? data->fit->voigtLorentzFraction() : 0.5;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];

    for (size_t i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }

    const double kG = 4.0 * M_LN2;
    const double cG = sqrt(kG / M_PI);
    const double cL = 2.0 / M_PI;

    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, p);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        for (size_t j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            double wj = w[j];
            double w2 = wj * wj;
            double D = 4.0 * diff * diff + w2;
            double invD = 1.0 / D;
            double L = cL * wj * invD;
            double G = (cG / wj) * exp(-kG * diff * diff / w2);

            double dL_dxc = cL * 8.0 * diff * wj * invD * invD;
            double dL_dw = cL * (4.0 * diff * diff - w2) * invD * invD;

            double dG_dxc = G * (2.0 * kG * diff / w2);
            double dG_dw = (G / wj) * (2.0 * kG * diff * diff / w2 - 1.0);

            gsl_matrix_set(J, i, 3 * j, (mu * L + (1.0 - mu) * G) / s);
            gsl_matrix_set(J, i, 3 * j + 1, a[j] * (mu * dL_dxc + (1.0 - mu) * dG_dxc) / s);
            gsl_matrix_set(J, i, 3 * j + 2, a[j] * (mu * dL_dw + (1.0 - mu) * dG_dw) / s);
        }
        gsl_matrix_set(J, i, p - 1, 1.0 / s);
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return GSL_SUCCESS;
}

int pseudovoigt_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    pseudovoigt_multi_peak_f(x, params, f);
    pseudovoigt_multi_peak_df(x, params, J);
    return GSL_SUCCESS;
}

int pearson7_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    struct FitData *data = (struct FitData *)params;
    size_t n = data->n;
    size_t p = data->p;
    double *X = data->X;
    double *Y = data->Y;
    double *sigma = data->sigma;
    double m = data->fit ? data->fit->pearsonShapeParameter() : 2.0;
    if (m < 0.55)
        m = 0.55;
    const double cm = pow(2.0, 1.0 / m) - 1.0;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];
    double offset = gsl_vector_get(x, p - 1);

    for (size_t i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }

    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double res = 0.0;
        for (size_t j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            double u = 2.0 * diff / w[j];
            double B = 1.0 + cm * u * u;
            res += a[j] * pow(B, -m);
        }
        gsl_vector_set(f, i, (res + offset - Y[i]) / s);
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return GSL_SUCCESS;
}

double pearson7_multi_peak_d(const gsl_vector *x, void *params)
{
    struct FitData *data = (struct FitData *)params;
    size_t n = data->n;
    size_t p = data->p;
    double *X = data->X;
    double *Y = data->Y;
    double *sigma = data->sigma;
    double m = data->fit ? data->fit->pearsonShapeParameter() : 2.0;
    if (m < 0.55)
        m = 0.55;
    const double cm = pow(2.0, 1.0 / m) - 1.0;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];
    double offset = gsl_vector_get(x, p - 1);

    for (size_t i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }

    double val = 0.0;
    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double res = 0.0;
        for (size_t j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            double u = 2.0 * diff / w[j];
            double B = 1.0 + cm * u * u;
            res += a[j] * pow(B, -m);
        }
        double t = (res + offset - Y[i]) / s;
        val += t * t;
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return val;
}

int pearson7_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    struct FitData *data = (struct FitData *)params;
    size_t n = data->n;
    size_t p = data->p;
    double *X = data->X;
    double *sigma = data->sigma;
    double m = data->fit ? data->fit->pearsonShapeParameter() : 2.0;
    if (m < 0.55)
        m = 0.55;
    const double cm = pow(2.0, 1.0 / m) - 1.0;

    size_t peaks = (p - 1) / 3;
    double *a = new double[peaks];
    double *xc = new double[peaks];
    double *w = new double[peaks];

    for (size_t i = 0; i < peaks; i++) {
        a[i] = gsl_vector_get(x, 3 * i);
        xc[i] = gsl_vector_get(x, 3 * i + 1);
        w[i] = gsl_vector_get(x, 3 * i + 2);
        if (fabs(w[i]) < 1e-15)
            w[i] = (w[i] < 0 ? -1e-15 : 1e-15);
    }

    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, p);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        for (size_t j = 0; j < peaks; j++) {
            double diff = X[i] - xc[j];
            double wj = w[j];
            double w2 = wj * wj;
            double u = 2.0 * diff / wj;
            double B = 1.0 + cm * u * u;
            double Bm = pow(B, -m);
            double Bm1 = Bm / B;

            gsl_matrix_set(J, i, 3 * j, Bm / s);
            gsl_matrix_set(J, i, 3 * j + 1, (8.0 * m * cm * a[j] * diff / w2) * Bm1 / s);
            gsl_matrix_set(J, i, 3 * j + 2, (8.0 * m * cm * a[j] * diff * diff / (w2 * wj)) * Bm1 / s);
        }
        gsl_matrix_set(J, i, p - 1, 1.0 / s);
    }
    delete[] a;
    delete[] xc;
    delete[] w;
    return GSL_SUCCESS;
}

int pearson7_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    pearson7_multi_peak_f(x, params, f);
    pearson7_multi_peak_df(x, params, J);
    return GSL_SUCCESS;
}

double eval_origin_peak_profile(int profile, double A, double xc, double w, double x,
                                double mu, double pearson_m, double asym_alpha, double tail_tau)
{
    double w_abs = fabs(w);
    if (w_abs < 1e-15)
        w_abs = 1e-15;
    double diff = x - xc;

    switch (profile) {
    case 5: { // Voigt (Thompson-Cox-Hastings numerical approximation)
        double d = (mu < 0.0) ? 0.0 : ((mu > 1.0) ? 1.0 : mu);
        double eta = 1.36603 * d - 0.47719 * d * d + 0.11116 * d * d * d;
        if (eta < 0.0)
            eta = 0.0;
        else if (eta > 1.0)
            eta = 1.0;
        const double kG = 4.0 * M_LN2;
        const double cG = sqrt(kG / M_PI);
        const double cL = 2.0 / M_PI;
        double L = cL * w_abs / (4.0 * diff * diff + w_abs * w_abs);
        double G = (cG / w_abs) * exp(-kG * diff * diff / (w_abs * w_abs));
        return A * (eta * L + (1.0 - eta) * G);
    }
    case 6: { // BiGaussian (Asymmetric Bifurcated Gaussian)
        double alpha = (asym_alpha > 0.05) ? asym_alpha : 1.5;
        double halfW = (diff < 0.0) ? (w_abs / (1.0 + alpha)) : ((alpha * w_abs) / (1.0 + alpha));
        if (halfW < 1e-15)
            halfW = 1e-15;
        return A * exp(-M_LN2 * diff * diff / (halfW * halfW));
    }
    case 7: { // EMG (Exponentially Modified Gaussian)
        double tau = (tail_tau > 1e-6) ? tail_tau : 1.2;
        double sigma = 0.5 * w_abs;
        if (sigma < 1e-15)
            sigma = 1e-15;
        double z = (sigma / tau - diff / sigma) / M_SQRT2;
        double gaussPart = exp(-0.5 * diff * diff / (sigma * sigma));
        double erfcxPart = 0.0;
        if (z > 25.0) {
            double invZ2 = 1.0 / (z * z);
            erfcxPart = (1.0 / (sqrt(M_PI) * z)) * (1.0 - 0.5 * invZ2 + 0.75 * invZ2 * invZ2);
        } else if (z < -25.0) {
            double arg = 0.5 * (sigma * sigma) / (tau * tau) - diff / tau;
            if (arg > 700.0)
                arg = 700.0;
            return (A / (2.0 * tau)) * exp(arg) * erfc(z);
        } else {
            erfcxPart = exp(z * z) * erfc(z);
        }
        return (A / (2.0 * tau)) * gaussPart * erfcxPart;
    }
    case 8: { // LogNormal (Asymmetric Log-Normal around xc)
        double asym = (fabs(asym_alpha) > 1e-4) ? fabs(asym_alpha) : 1.5;
        double s = 1.0 + 2.0 * asym * diff / w_abs;
        if (s <= 1e-12)
            return 0.0;
        double b = log(asym + sqrt(asym * asym + 1.0));
        if (fabs(b) < 1e-12)
            b = 1e-12;
        double r = log(s) / b;
        return A * exp(-M_LN2 * r * r);
    }
    case 9: { // Asym2Sig (Asymmetric Double Sigmoidal)
        double alpha = (asym_alpha > 0.05) ? asym_alpha : 1.5;
        double s1 = 0.25 * w_abs / (1.0 + alpha);
        double s2 = 0.25 * alpha * w_abs / (1.0 + alpha);
        if (s1 < 1e-15)
            s1 = 1e-15;
        if (s2 < 1e-15)
            s2 = 1e-15;
        double u1 = -(diff + 0.5 * w_abs) / s1;
        double u2 = -(diff - 0.5 * w_abs) / s2;
        if (u1 > 700.0)
            u1 = 700.0;
        else if (u1 < -700.0)
            u1 = -700.0;
        if (u2 > 700.0)
            u2 = 700.0;
        else if (u2 < -700.0)
            u2 = -700.0;
        double sig1 = 1.0 / (1.0 + exp(u1));
        double sig2 = 1.0 - 1.0 / (1.0 + exp(u2));
        return A * sig1 * sig2;
    }
    case 10: { // DoniachSunjic (XPS Asymmetric Peak)
        double alpha = (!gsl_finite(asym_alpha) || asym_alpha >= 0.85 || asym_alpha < 0.0)
                ? 0.15
                : asym_alpha;
        double u = 2.0 * diff / w_abs;
        double num = cos(0.5 * M_PI * alpha + (1.0 - alpha) * atan(u));
        double den = pow(1.0 + u * u, 0.5 * (1.0 - alpha));
        return A * num / den;
    }
    default:
        break;
    }
    (void)pearson_m;
    return 0.0;
}

static int generic_origin_multi_peak_f(int profile, const gsl_vector *x, void *params, gsl_vector *f)
{
    struct FitData *data = (struct FitData *)params;
    size_t n = data->n;
    size_t p = data->p;
    double *X = data->X;
    double *Y = data->Y;
    double *sigma = data->sigma;
    double mu = data->fit ? data->fit->voigtLorentzFraction() : 0.5;
    double m = data->fit ? data->fit->pearsonShapeParameter() : 2.0;
    double alpha = data->fit ? data->fit->peakAsymmetryParameter() : 1.5;
    double tau = data->fit ? data->fit->peakTailParameter() : 1.2;

    size_t peaks = (p - 1) / 3;
    double offset = gsl_vector_get(x, p - 1);

    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double res = 0.0;
        for (size_t j = 0; j < peaks; j++) {
            double aj = gsl_vector_get(x, 3 * j);
            double xcj = gsl_vector_get(x, 3 * j + 1);
            double wj = gsl_vector_get(x, 3 * j + 2);
            res += eval_origin_peak_profile(profile, aj, xcj, wj, X[i], mu, m, alpha, tau);
        }
        gsl_vector_set(f, i, (res + offset - Y[i]) / s);
    }
    return GSL_SUCCESS;
}

static double generic_origin_multi_peak_d(int profile, const gsl_vector *x, void *params)
{
    struct FitData *data = (struct FitData *)params;
    size_t n = data->n;
    size_t p = data->p;
    double *X = data->X;
    double *Y = data->Y;
    double *sigma = data->sigma;
    double mu = data->fit ? data->fit->voigtLorentzFraction() : 0.5;
    double m = data->fit ? data->fit->pearsonShapeParameter() : 2.0;
    double alpha = data->fit ? data->fit->peakAsymmetryParameter() : 1.5;
    double tau = data->fit ? data->fit->peakTailParameter() : 1.2;

    size_t peaks = (p - 1) / 3;
    double offset = gsl_vector_get(x, p - 1);
    double val = 0.0;

    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double res = 0.0;
        for (size_t j = 0; j < peaks; j++) {
            double aj = gsl_vector_get(x, 3 * j);
            double xcj = gsl_vector_get(x, 3 * j + 1);
            double wj = gsl_vector_get(x, 3 * j + 2);
            res += eval_origin_peak_profile(profile, aj, xcj, wj, X[i], mu, m, alpha, tau);
        }
        double t = (res + offset - Y[i]) / s;
        val += t * t;
    }
    return val;
}

static int generic_origin_multi_peak_df(int profile, const gsl_vector *x, void *params, gsl_matrix *J)
{
    struct FitData *data = (struct FitData *)params;
    size_t n = data->n;
    size_t p = data->p;
    double *X = data->X;
    double *sigma = data->sigma;
    double mu = data->fit ? data->fit->voigtLorentzFraction() : 0.5;
    double m = data->fit ? data->fit->pearsonShapeParameter() : 2.0;
    double alpha = data->fit ? data->fit->peakAsymmetryParameter() : 1.5;
    double tau = data->fit ? data->fit->peakTailParameter() : 1.2;

    size_t peaks = (p - 1) / 3;

    for (size_t i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, p);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double xi = X[i];
        for (size_t j = 0; j < peaks; j++) {
            double aj = gsl_vector_get(x, 3 * j);
            double xcj = gsl_vector_get(x, 3 * j + 1);
            double wj = gsl_vector_get(x, 3 * j + 2);
            double w_abs = fabs(wj);
            if (w_abs < 1e-12)
                w_abs = 1e-12;

            double phi = eval_origin_peak_profile(profile, 1.0, xcj, w_abs, xi, mu, m, alpha, tau);
            // Steps relative to the peak width (an absolute floor such as 1e-6 exceeds the
            // whole peak when x is in SI units, e.g. w ~ 1e-10 m).
            double hc = 1e-6 * w_abs;
            double hw = 1e-6 * w_abs;

            double phi_cp = eval_origin_peak_profile(profile, 1.0, xcj + hc, w_abs, xi, mu, m, alpha, tau);
            double phi_cm = eval_origin_peak_profile(profile, 1.0, xcj - hc, w_abs, xi, mu, m, alpha, tau);
            double dY_dxc = aj * (phi_cp - phi_cm) / (2.0 * hc);

            double phi_wp = eval_origin_peak_profile(profile, 1.0, xcj, w_abs + hw, xi, mu, m, alpha, tau);
            double phi_wm = eval_origin_peak_profile(profile, 1.0, xcj, (w_abs > 2.0 * hw) ? (w_abs - hw) : (0.5 * w_abs), xi, mu, m, alpha, tau);
            double dw_step = (w_abs + hw) - ((w_abs > 2.0 * hw) ? (w_abs - hw) : (0.5 * w_abs));
            double dY_dw = aj * ((wj < 0.0) ? -1.0 : 1.0) * (phi_wp - phi_wm) / dw_step;

            gsl_matrix_set(J, i, 3 * j, phi / s);
            gsl_matrix_set(J, i, 3 * j + 1, dY_dxc / s);
            gsl_matrix_set(J, i, 3 * j + 2, dY_dw / s);
        }
        gsl_matrix_set(J, i, p - 1, 1.0 / s);
    }
    return GSL_SUCCESS;
}

#define DEFINE_ORIGIN_MULTI_PEAK_FUNCS(prefix, prof_id)                                            \
    int prefix##_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f)                    \
    {                                                                                              \
        return generic_origin_multi_peak_f(prof_id, x, params, f);                                 \
    }                                                                                              \
    double prefix##_multi_peak_d(const gsl_vector *x, void *params)                                \
    {                                                                                              \
        return generic_origin_multi_peak_d(prof_id, x, params);                                    \
    }                                                                                              \
    int prefix##_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J)                   \
    {                                                                                              \
        return generic_origin_multi_peak_df(prof_id, x, params, J);                                \
    }                                                                                              \
    int prefix##_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)   \
    {                                                                                              \
        generic_origin_multi_peak_f(prof_id, x, params, f);                                        \
        generic_origin_multi_peak_df(prof_id, x, params, J);                                       \
        return GSL_SUCCESS;                                                                        \
    }

DEFINE_ORIGIN_MULTI_PEAK_FUNCS(voigt, 5)
DEFINE_ORIGIN_MULTI_PEAK_FUNCS(bigaussian, 6)
DEFINE_ORIGIN_MULTI_PEAK_FUNCS(emg, 7)
DEFINE_ORIGIN_MULTI_PEAK_FUNCS(lognormal, 8)
DEFINE_ORIGIN_MULTI_PEAK_FUNCS(asym2sig, 9)
DEFINE_ORIGIN_MULTI_PEAK_FUNCS(doniach_sunjic, 10)

int user_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    return static_cast<struct FitData *>(params)->fit->evaluate_f(x, f);
}

double user_d(const gsl_vector *x, void *params)
{
    return static_cast<struct FitData *>(params)->fit->evaluate_d(x);
}

int user_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    return static_cast<struct FitData *>(params)->fit->evaluate_df(x, J);
}

int user_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    int st = user_f(x, params, f);
    if (st != GSL_SUCCESS)
        return st;
    return user_df(x, params, J);
}

int boltzmann_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    double A1 = gsl_vector_get(x, 0);
    double A2 = gsl_vector_get(x, 1);
    double x0 = gsl_vector_get(x, 2);
    double dx = gsl_vector_get(x, 3);
    size_t i;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double s = sigma ? sigma[i] : 1.0;
        double Yi = (A1 - A2) / (1 + exp((X[i] - x0) / dx)) + A2;
        gsl_vector_set(f, i, (Yi - Y[i]) / s);
    }

    return GSL_SUCCESS;
}

double boltzmann_d(const gsl_vector *x, void *params)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *Y = ((struct FitData *)params)->Y;
    double *sigma = ((struct FitData *)params)->sigma;

    double A1 = gsl_vector_get(x, 0);
    double A2 = gsl_vector_get(x, 1);
    double x0 = gsl_vector_get(x, 2);
    double dx = gsl_vector_get(x, 3);
    size_t i;
    double val = 0;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i))
            continue;
        double s = sigma ? sigma[i] : 1.0;
        double dYi = ((A1 - A2) / (1 + exp((X[i] - x0) / dx)) + A2 - Y[i]) / s;
        val += dYi * dYi;
    }
    return val;
}

int boltzmann_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    size_t n = ((struct FitData *)params)->n;
    double *X = ((struct FitData *)params)->X;
    double *sigma = ((struct FitData *)params)->sigma;

    double A1 = gsl_vector_get(x, 0);
    double A2 = gsl_vector_get(x, 1);
    double x0 = gsl_vector_get(x, 2);
    double dx = gsl_vector_get(x, 3);
    size_t i;
    for (i = 0; i < n; i++) {
        if (fit_gsl_invalid_sigma(sigma, i)) {
            fit_gsl_zero_jac_row(J, i, J->size2);
            continue;
        }
        /* Jacobian matrix J(i,j) = dfi / dxj,		*/
        /* where fi = Yi - yi,						*/
        /* Yi = (A1-A2)/(1+exp((X[i]-x0)/dx)) + A2	*/
        /* and the xj are the parameters (A1,A2,x0,dx)*/
        double s = sigma ? sigma[i] : 1.0;
        double diff = X[i] - x0;
        double e = exp(diff / dx);
        double r = 1 / (1 + e);
        double aux = (A1 - A2) * e * r * r / (dx * s);
        gsl_matrix_set(J, i, 0, r / s);
        gsl_matrix_set(J, i, 1, (1 - r) / s);
        gsl_matrix_set(J, i, 2, aux);
        gsl_matrix_set(J, i, 3, aux * diff / dx);
    }
    return GSL_SUCCESS;
}

int boltzmann_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J)
{
    boltzmann_f(x, params, f);
    boltzmann_df(x, params, J);
    return GSL_SUCCESS;
}
