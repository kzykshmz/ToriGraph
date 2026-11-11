/***************************************************************************
   File                 : fit_gsl.h
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

#ifndef FIT_GSL_H
#define FIT_GSL_H

#include <gsl/gsl_vector.h>

class Fit;

//! Structure for fitting data
struct FitData
{
    size_t n;
    size_t p;
    double *X;
    double *Y;
    double *sigma; // standard deviation of Y (for weighting)
    Fit *fit;
};

int expd3_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);
int expd3_df(const gsl_vector *x, void *params, gsl_matrix *J);
int expd3_f(const gsl_vector *x, void *params, gsl_vector *f);
double expd3_d(const gsl_vector *x, void *params);

int expd2_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);
int expd2_df(const gsl_vector *x, void *params, gsl_matrix *J);
int expd2_f(const gsl_vector *x, void *params, gsl_vector *f);
double expd2_d(const gsl_vector *x, void *params);

int exp_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);
int exp_df(const gsl_vector *x, void *params, gsl_matrix *J);
int exp_f(const gsl_vector *x, void *params, gsl_vector *f);
double exp_d(const gsl_vector *x, void *params);

int boltzmann_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);
int boltzmann_df(const gsl_vector *x, void *params, gsl_matrix *J);
int boltzmann_f(const gsl_vector *x, void *params, gsl_vector *f);
double boltzmann_d(const gsl_vector *x, void *params);

int gauss_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);
int gauss_df(const gsl_vector *x, void *params, gsl_matrix *J);
int gauss_f(const gsl_vector *x, void *params, gsl_vector *f);
double gauss_d(const gsl_vector *x, void *params);

int gauss_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double gauss_multi_peak_d(const gsl_vector *x, void *params);
int gauss_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int gauss_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

int lorentz_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double lorentz_multi_peak_d(const gsl_vector *x, void *params);
int lorentz_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int lorentz_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

int gauss_amp_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double gauss_amp_multi_peak_d(const gsl_vector *x, void *params);
int gauss_amp_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int gauss_amp_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

int pseudovoigt_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double pseudovoigt_multi_peak_d(const gsl_vector *x, void *params);
int pseudovoigt_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int pseudovoigt_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

int pearson7_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double pearson7_multi_peak_d(const gsl_vector *x, void *params);
int pearson7_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int pearson7_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

double eval_origin_peak_profile(int profile, double A, double xc, double w, double x,
                                double mu, double pearson_m, double asym_alpha, double tail_tau);

int voigt_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double voigt_multi_peak_d(const gsl_vector *x, void *params);
int voigt_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int voigt_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

int bigaussian_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double bigaussian_multi_peak_d(const gsl_vector *x, void *params);
int bigaussian_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int bigaussian_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

int emg_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double emg_multi_peak_d(const gsl_vector *x, void *params);
int emg_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int emg_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

int lognormal_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double lognormal_multi_peak_d(const gsl_vector *x, void *params);
int lognormal_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int lognormal_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

int asym2sig_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double asym2sig_multi_peak_d(const gsl_vector *x, void *params);
int asym2sig_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int asym2sig_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

int doniach_sunjic_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
double doniach_sunjic_multi_peak_d(const gsl_vector *x, void *params);
int doniach_sunjic_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
int doniach_sunjic_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

int user_f(const gsl_vector *x, void *params, gsl_vector *f);
double user_d(const gsl_vector *x, void *params);
int user_df(const gsl_vector *x, void *params, gsl_matrix *J);
int user_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *J);

#endif
