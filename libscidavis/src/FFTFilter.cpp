/***************************************************************************
    File                 : FFTFilter.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Numerical FFT filtering of data sets

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
#include "FFTFilter.h"

#include <QApplication>
#include <QMessageBox>
#include <QLocale>

#include <gsl/gsl_fft_halfcomplex.h>

#include <algorithm>
#include <cmath>

FFTFilter::FFTFilter(ApplicationWindow *parent, Graph *g, const QString &curveTitle, int m)
    : Filter(parent, g)
{
    d_sort_data = true;
    setDataFromCurve(curveTitle);
    init(m);
}

FFTFilter::FFTFilter(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
                     double end, int m)
    : Filter(parent, g)
{
    d_sort_data = true;
    setDataFromCurve(curveTitle, start, end);
    init(m);
}

void FFTFilter::init(int m)
{
    setObjectName(tr("FFT") + tr("Filtered"));
    setFilterType(m);
    d_points = static_cast<int>(d_x.size());
    d_offset = true;
    d_low_freq = 0;
    d_high_freq = 0;

    bool valid = (d_x.size() >= 2 && d_x.size() == d_y.size());
    if (valid) {
        for (size_t i = 0; i < d_x.size(); ++i) {
            if (!std::isfinite(d_x[i]) || !std::isfinite(d_y[i])) {
                valid = false;
                break;
            }
            if (i > 0 && d_x[i] < d_x[i - 1]) {
                valid = false;
                break;
            }
        }
        if (valid && !(d_x.back() > d_x.front()))
            valid = false;
    }
    if (!valid)
        d_init_err = true;
}

void FFTFilter::setFilterType(int type)
{
    if (type < 1 || type > 4) {
        if (!isSilentOrNonInteractive()
            && !(qApp && qApp->property("torigraph_noninteractive").toBool())) {
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Unknown filter type. Valid values are: 1 - Low pass, 2 - High "
                                     "Pass, 3 - Band Pass, 4 - Band block."));
        }
        d_init_err = true;
        return;
    }
    d_filter_type = (FilterType)type;
}

void FFTFilter::setCutoff(double f)
{
    if (d_filter_type > 2 && d_low_freq == f)
        return;

    d_low_freq = f;
}

void FFTFilter::setBand(double lowFreq, double highFreq)
{
    if (d_filter_type < 3)
        return;
    else if (lowFreq == highFreq) {
        if (!isSilentOrNonInteractive()
            && !(qApp && qApp->property("torigraph_noninteractive").toBool())) {
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Please enter different values for the band limits."));
        }
        d_init_err = true;
        return;
    }

    if (lowFreq > highFreq) {
        d_low_freq = highFreq;
        d_high_freq = lowFreq;
    } else {
        d_low_freq = lowFreq;
        d_high_freq = highFreq;
    }
}

void FFTFilter::calculateOutputData(std::vector<double>& x, std::vector<double>& y)
{
    const size_t n = std::min({ d_x.size(), d_y.size(), x.size(), y.size() });
    if (n < 2 || !std::isfinite(d_x.front()) || !std::isfinite(d_x[n - 1])
        || !(d_x[n - 1] > d_x.front())) {
        d_init_err = true;
        const double xFill = (!d_x.empty() && std::isfinite(d_x.front())) ? d_x.front() : 0.0;
        std::fill(x.begin(), x.end(), xFill);
        std::fill(y.begin(), y.end(), 0.0);
        return;
    }
    for (size_t k = 0; k < n; ++k) {
        if (!std::isfinite(d_x[k]) || !std::isfinite(d_y[k])
            || (k > 0 && d_x[k] < d_x[k - 1])) {
            d_init_err = true;
            std::fill(x.begin(), x.end(), d_x.front());
            std::fill(y.begin(), y.end(), 0.0);
            return;
        }
    }

    // interpolate y to even spacing across [d_x.front(), d_x[n - 1]]
    const double xFront = d_x.front();
    const double xBack = d_x[n - 1];
    const double delta = (xBack - xFront) / static_cast<double>(n - 1);

    size_t i = 1;
    for (size_t j = 0; j < n; ++j) {
        const double xi = (j == 0)
                ? xFront
                : ((j + 1 == n) ? xBack
                                : std::clamp(xFront + static_cast<double>(j) * delta, xFront, xBack));
        x[j] = xi;
        if (xi <= xFront) {
            y[j] = d_y.front();
        } else if (xi >= xBack) {
            y[j] = d_y[n - 1];
        } else {
            while (i + 1 < n && d_x[i] < xi)
                ++i;
            const double dx = d_x[i] - d_x[i - 1];
            if (dx > 0.0)
                y[j] = ((d_x[i] - xi) * d_y[i - 1] + (xi - d_x[i - 1]) * d_y[i]) / dx;
            else
                y[j] = d_y[i];
        }
    }

    const double df = 1.0 / (static_cast<double>(n) * delta);

    gsl_fft_real_workspace *work = gsl_fft_real_workspace_alloc(n);
    gsl_fft_real_wavetable *real = gsl_fft_real_wavetable_alloc(n);
    if (!work || !real) {
        if (real)
            gsl_fft_real_wavetable_free(real);
        if (work)
            gsl_fft_real_workspace_free(work);
        d_init_err = true;
        return;
    }

    gsl_fft_real_transform(y.data(), 1, n, real, work);
    gsl_fft_real_wavetable_free(real);

    d_explanation = QLocale().toString(d_low_freq) + " ";
    if (d_filter_type > 2)
        d_explanation += tr("to") + " " + QLocale().toString(d_high_freq) + " ";
    d_explanation += tr("Hz") + " ";

    switch ((int)d_filter_type) {
    case 1: // low pass
        d_explanation += tr("Low Pass FFT Filter");
        for (int idx = static_cast<int>(n) - 1; idx >= 0 && ((idx + 1) / 2) * df > d_low_freq; idx--)
            y[idx] = 0;
        break;

    case 2: // high pass
        d_explanation += tr("High Pass FFT Filter");
        for (size_t idx = 0; idx < n && ((idx + 1) / 2) * df < d_low_freq; idx++)
            y[idx] = 0;
        break;

    case 3: // band pass
        d_explanation += tr("Band Pass FFT Filter");
        for (size_t idx = d_offset ? 1 : 0; idx < n; idx++)
            if ((((idx + 1) / 2) * df <= d_low_freq) || (((idx + 1) / 2) * df >= d_high_freq))
                y[idx] = 0;
        break;

    case 4: // band block
        d_explanation += tr("Band Block FFT Filter");

        if (!d_offset)
            y[0] = 0; // substract DC offset

        for (size_t idx = 1; idx < n; idx++)
            if ((((idx + 1) / 2) * df > d_low_freq) && (((idx + 1) / 2) * df < d_high_freq))
                y[idx] = 0;
        break;
    }

    gsl_fft_halfcomplex_wavetable *hc = gsl_fft_halfcomplex_wavetable_alloc(n);
    if (hc) {
        gsl_fft_halfcomplex_inverse(y.data(), 1, n, hc, work);
        gsl_fft_halfcomplex_wavetable_free(hc);
    } else {
        d_init_err = true;
    }
    gsl_fft_real_workspace_free(work);
}
