/***************************************************************************
    File                 : Interpolation.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Numerical interpolation of data sets

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
#include "Interpolation.h"

#include <QApplication>
#include <QMessageBox>

#include <gsl/gsl_sort.h>
#include <gsl/gsl_spline.h>
#include <gsl/gsl_interp.h>

#include <algorithm>
#include <cmath>
#include <limits>

Interpolation::Interpolation(ApplicationWindow *parent, Graph *g, const QString &curveTitle, int m)
    : Filter(parent, g)
{
    init(m);
    setDataFromCurve(curveTitle);
}

Interpolation::Interpolation(ApplicationWindow *parent, Graph *g, const QString &curveTitle,
                             double start, double end, int m)
    : Filter(parent, g)
{
    init(m);
    setDataFromCurve(curveTitle, start, end);
}

void Interpolation::init(int m)
{
    if (m < 0 || m > 2) {
        if (!isSilentOrNonInteractive()
            && !(qApp && qApp->property("torigraph_noninteractive").toBool())) {
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Unknown interpolation method. Valid values are: 0 - Linear, "
                                     "1 - Cubic, 2 - Akima."));
        }
        d_init_err = true;
        return;
    }
    d_method = m;
    switch (d_method) {
    case 0:
        setObjectName(tr("Linear") + tr("Int"));
        d_explanation = tr("Linear") + " " + tr("Interpolation");
        break;
    case 1:
        setObjectName(tr("Cubic") + tr("Int"));
        d_explanation = tr("Cubic") + " " + tr("Interpolation");
        break;
    case 2:
        setObjectName(tr("Akima") + tr("Int"));
        d_explanation = tr("Akima") + " " + tr("Interpolation");
        break;
    }
    d_sort_data = true;
    d_min_points = (d_method == Linear) ? 2 : (d_method + 3);
}

void Interpolation::setDataCurve(int curve, double start, double end)
{
    if (start > end)
        qSwap(start, end);
    d_range_clipped = false;
    // The interpolant goes through every point of the curve: building it from the points inside
    // [start, end] only would change the spline (and leave nothing to interpolate in a gap
    // between two points). Filter::setDataCurve sets d_from / d_to to the data range.
    Filter::setDataCurve(curve, -std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::infinity());
    if (d_init_err)
        return;
    d_data_x_min = d_from;
    d_data_x_max = d_to;
    const double from = std::max(start, d_data_x_min);
    const double to = std::min(end, d_data_x_max);
    if (!(from < to)) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("The interpolation range from %1 to %2 does not overlap the X "
                                     "range of the data (%3 to %4).")
                                          .arg(start)
                                          .arg(end)
                                          .arg(d_data_x_min)
                                          .arg(d_data_x_max));
        d_init_err = true;
        return;
    }
    d_range_clipped = (from > start) || (to < end);
    d_from = from;
    d_to = to;
}

void Interpolation::setMethod(int m)
{
    const bool quiet = isSilentOrNonInteractive()
            || (qApp && qApp->property("torigraph_noninteractive").toBool());
    if (m < 0 || m > 2) {
        if (!quiet) {
            QMessageBox::critical((ApplicationWindow *)parent(), tr("Error"),
                                  tr("Unknown interpolation method, valid values are: 0 - Linear, "
                                     "1 - Cubic, 2 - Akima."));
        }
        d_init_err = true;
        return;
    }
    unsigned min_points = (m == Linear) ? 2u : static_cast<unsigned>(m + 3);
    if (d_x.size() < min_points) {
        if (!quiet) {
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("You need at least %1 points in order to perform this "
                                     "operation!")
                                          .arg(min_points));
        }
        d_init_err = true;
        return;
    }
    d_method = m;
    d_min_points = static_cast<int>(min_points);
}

void Interpolation::calculateOutputData(std::vector<double>& x, std::vector<double>& y)
{
    if (d_points <= 0 || d_x.size() < static_cast<size_t>(std::max(2, d_min_points))
        || d_y.size() < d_x.size()) {
        d_init_err = true;
        return;
    }
    for (size_t i = 0; i < d_x.size(); ++i) {
        if (!std::isfinite(d_x[i]) || !std::isfinite(d_y[i])
            || (i > 0 && !(d_x[i - 1] < d_x[i]))) {
            d_init_err = true;
            return;
        }
    }

    gsl_interp_accel *acc = gsl_interp_accel_alloc();
    const gsl_interp_type *method = nullptr;
    switch (d_method) {
    case 0:
        method = gsl_interp_linear;
        break;
    case 1:
        method = gsl_interp_cspline;
        break;
    case 2:
        method = gsl_interp_akima;
        break;
    default:
        method = gsl_interp_linear;
        break;
    }

    gsl_spline *interp = gsl_spline_alloc(method, d_x.size());
    if (!interp || gsl_spline_init(interp, d_x.data(), d_y.data(), d_x.size()) != 0) {
        if (interp)
            gsl_spline_free(interp);
        if (acc)
            gsl_interp_accel_free(acc);
        d_init_err = true;
        return;
    }

    const double xMin = d_x.front();
    const double xMax = d_x.back();
    const double step = (d_points > 1) ? ((d_to - d_from) / static_cast<double>(d_points - 1)) : 0.0;
    for (int j = 0; j < d_points; j++) {
        const double rawX = (j == 0) ? d_from : ((j == d_points - 1) ? d_to : (d_from + j * step));
        x[j] = rawX;
        const double evalX = std::clamp(rawX, xMin, xMax);
        y[j] = gsl_spline_eval(interp, evalX, acc);
    }

    gsl_spline_free(interp);
    gsl_interp_accel_free(acc);
}

bool Interpolation::isDataAcceptable() const
{
    if (!Filter::isDataAcceptable())
        return false;

    for (size_t i = 0; i < d_x.size(); ++i) {
        if (!std::isfinite(d_x[i]) || (i < d_y.size() && !std::isfinite(d_y[i])))
            return false;
    }

    // GSL interpolation routines fail on duplicate or non-strictly-monotonic x values
    for (size_t i = 1; i < d_x.size(); i++) {
        if (!(d_x[i - 1] < d_x[i])) {
            if (!isSilentOrNonInteractive()
                && !(qApp && qApp->property("torigraph_noninteractive").toBool())) {
                QMessageBox::critical((ApplicationWindow *)parent(),
                                      tr("ToriGraph") + " - " + tr("Error"),
                                      tr("Several data points have the same x value causing divisions "
                                         "by zero, operation aborted!"));
            }
            return false;
        }
    }

    return true;
}
