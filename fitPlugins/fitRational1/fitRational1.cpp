/***************************************************************************
   File                 : fitRational1.cpp
   Project              : SciDAVis
   Description          : Fit plugin for "A*t^2*x/(1+4*PI^2*t^2*x^2)"
   --------------------------------------------------------------------
   Copyright            : (C) 2005 Ion Vasilief (ion_vasilief*yahoo.fr)
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

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include <gsl/gsl_vector.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(_MSC_VER) // MSVC Compiler
#define MY_EXPORT __declspec(dllexport)
#else
#define MY_EXPORT
#endif

struct fitData
{
    size_t n;
    size_t p;
    double *X;
    double *Y;
    double *sigma; // weighting data
};

extern "C" MY_EXPORT const char *name()
{
    return "Rational1";
}

extern "C" MY_EXPORT const char *function()
{
    return "A*t^2*x/(1+4*PI^2*t^2*x^2)";
}

extern "C" MY_EXPORT const char *parameters()
{
    return "A,t";
}

extern "C" MY_EXPORT double function_eval(double x, double *params)
{
    const double pi2 = M_PI * M_PI;
    double t2 = params[1] * params[1];
    return (params[0] * t2 * x / (1.0 + 4.0 * pi2 * x * x * t2));
}

extern "C" MY_EXPORT int function_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    size_t n = ((struct fitData *)params)->n;
    double *X = ((struct fitData *)params)->X;
    double *Y = ((struct fitData *)params)->Y;
    double *sigma = ((struct fitData *)params)->sigma;

    double a = gsl_vector_get(x, 0);
    double tau = gsl_vector_get(x, 1);
    double tau2 = tau * tau;
    const double pi2 = M_PI * M_PI;

    size_t i;
    for (i = 0; i < n; i++) {
        double t = X[i];
        double Yi = a * tau2 * t / (1.0 + 4.0 * pi2 * t * t * tau2);
        gsl_vector_set(f, i, (Yi - Y[i]) / sigma[i]);
    }

    return GSL_SUCCESS;
}

extern "C" MY_EXPORT double function_d(const gsl_vector *x, void *params)
{
    size_t n = ((struct fitData *)params)->n;
    double *X = ((struct fitData *)params)->X;
    double *Y = ((struct fitData *)params)->Y;
    double *sigma = ((struct fitData *)params)->sigma;

    double a = gsl_vector_get(x, 0);
    double tau = gsl_vector_get(x, 1);
    double tau2 = tau * tau;
    const double pi2 = M_PI * M_PI;

    size_t i;
    double val = 0;
    for (i = 0; i < n; i++) {
        double t = X[i];
        double dYi = ((a * tau2 * t / (1.0 + 4.0 * pi2 * t * t * tau2)) - Y[i]) / sigma[i];
        val += dYi * dYi;
    }

    return val;
}

extern "C" MY_EXPORT int function_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    size_t n = ((struct fitData *)params)->n;
    double *X = ((struct fitData *)params)->X;
    double *sigma = ((struct fitData *)params)->sigma;

    double a = gsl_vector_get(x, 0);
    double tau = gsl_vector_get(x, 1);
    double tau2 = tau * tau;
    const double pi2 = M_PI * M_PI;

    size_t i;
    for (i = 0; i < n; i++) {
        /* Jacobian matrix J(i,j) = dfi / dxj,
           where fi = (Yi - Y[i])/sigma[i],
           Yi = a*tau^2*t/(1 + 4*PI^2*t*t*tau^2)
           and the xj are the parameters (a, tau) */

        double s = sigma[i];
        double t = X[i];
        double r = 1.0 / (1.0 + 4.0 * pi2 * tau2 * t * t);
        gsl_matrix_set(J, i, 0, tau2 * t * r / s);
        gsl_matrix_set(J, i, 1, 2.0 * a * tau * t * r * r / s);
    }
    return GSL_SUCCESS;
}

extern "C" MY_EXPORT int function_fdf(const gsl_vector *x, void *params, gsl_vector *f,
                                      gsl_matrix *J)
{
    function_f(x, params, f);
    function_df(x, params, J);

    return GSL_SUCCESS;
}
