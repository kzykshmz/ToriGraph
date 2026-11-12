/***************************************************************************
    File                 : SmoothFilter.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Ion Vasilief
                           (C) 2010 by Knut Franke
    Email (use @ for *)  : ion_vasilief*yahoo.fr
                           knut.franke*gmx.de
    Description          : Numerical smoothing of data sets

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
#include "SmoothFilter.h"

#include <QApplication>
#include <QMessageBox>

#include <algorithm>
#include <cmath>
#include <numeric>

#include <gsl/gsl_fft_halfcomplex.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_poly.h>

SmoothFilter::SmoothFilter(ApplicationWindow *parent, Graph *g, const QString &curveTitle, int m)
    : Filter(parent, g)
{
    setDataFromCurve(curveTitle);
    init(m);
}

SmoothFilter::SmoothFilter(ApplicationWindow *parent, Graph *g, const QString &curveTitle,
                           double start, double end, int m)
    : Filter(parent, g)
{
    setDataFromCurve(curveTitle, start, end);
    init(m);
}

void SmoothFilter::init(int m)
{
    setObjectName(tr("Smoothed"));
    d_points = static_cast<int>(d_x.size());
    d_right_points = 2;
    d_left_points = 2;
    d_polynom_order = 2;
    d_lowess_span = 0.2;
    d_whittaker_lambda = 100.0;
    setMethod(m);
}

void SmoothFilter::setMethod(int m)
{
    if (m < 1 || m > 6) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Unknown smooth filter. Valid values are: 1 - Savitzky-Golay, "
                                     "2 - FFT, 3 - Moving Window Average, 4 - LOWESS, "
                                     "5 - Whittaker, 6 - Median."));
        d_init_err = true;
        return;
    }
    d_method = (SmoothMethod)m;
}

void SmoothFilter::calculateOutputData(std::vector<double>& x, std::vector<double>&  y)
{
    const int nCopy = std::min({ d_points, static_cast<int>(d_x.size()),
                                 static_cast<int>(d_y.size()), static_cast<int>(x.size()),
                                 static_cast<int>(y.size()) });
    if (nCopy <= 0)
        return;
    for (int i = 0; i < nCopy; i++) {
        x[i] = d_x[i];
        y[i] = d_y[i]; // filtering frequencies
    }

    switch ((int)d_method) {
    case SavitzkyGolay:
        d_explanation = QString::number(d_right_points) + " " + tr("points") + " "
                + tr("Savitzky-Golay smoothing");
        smoothSavGol(x, y);
        break;
    case FFT:
        d_explanation =
                QString::number(d_right_points) + " " + tr("points") + " " + tr("FFT smoothing");
        smoothFFT(x, y);
        break;
    case Average:
        d_explanation = QString::number(d_right_points) + " " + tr("points") + " "
                + tr("average smoothing");
        smoothAverage(x, y);
        break;
    case LOWESS:
        d_explanation = tr("LOWESS smoothing") + QString(" (span=%1)").arg(d_lowess_span, 0, 'g', 4);
        smoothLowess(x, y);
        break;
    case Whittaker:
        d_explanation = tr("Whittaker smoothing")
                + QString(" (lambda=%1)").arg(d_whittaker_lambda, 0, 'g', 4);
        smoothWhittaker(x, y);
        break;
    case Median:
        d_explanation = QString::number(d_right_points) + " " + tr("points") + " "
                + tr("median smoothing");
        smoothMedian(x, y);
        break;
    }
}

void SmoothFilter::smoothFFT(std::vector<double>& x, std::vector<double>&  y)
{
    if (d_x.size() < 2 || d_y.size() < 2 || x.size() < d_x.size() || y.size() < d_y.size()
        || d_right_points <= 0 || x[1] == x[0])
        return;

    gsl_fft_real_workspace *work = gsl_fft_real_workspace_alloc(d_x.size());
    gsl_fft_real_wavetable *real = gsl_fft_real_wavetable_alloc(d_x.size());
    gsl_fft_real_transform(y.data(), 1, d_y.size(), real, work); // FFT forward
    gsl_fft_real_wavetable_free(real);

    double df = 1.0 / (double)(x[1] - x[0]);
    double lf = df / (double)d_right_points; // frequency cutoff
    df = 0.5 * df / (double)d_x.size();

    for (unsigned i = 0; i < d_x.size(); i++) {
        x[i] = d_x[i];
        y[i] = i * df > lf ? 0 : y[i]; // filtering frequencies
    }

    gsl_fft_halfcomplex_wavetable *hc = gsl_fft_halfcomplex_wavetable_alloc(d_y.size());
    gsl_fft_halfcomplex_inverse(y.data(), 1, d_y.size(), hc, work); // FFT inverse
    gsl_fft_halfcomplex_wavetable_free(hc);
    gsl_fft_real_workspace_free(work);
}

void SmoothFilter::smoothAverage(std::vector<double>&, std::vector<double>&  y)
{
    const int n = static_cast<int>(y.size());
    const int p2 = std::max(0, d_right_points / 2);
    if (n <= 1 || p2 <= 0)
        return;

    std::vector<double> s(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const int halfWin = std::min({ p2, i, n - 1 - i });
        const int lo = (halfWin == p2) ? (i - p2) : std::max(0, i - std::max(halfWin, 0));
        const int hi = (halfWin == p2) ? (i + p2) : std::min(n - 1, i + std::max(halfWin, 0));
        // When window exceeds array length (2 * p2 >= n), use clamped signed bounds [max(0, i-p2)..min(n-1, i+p2)]
        const int effLo = (2 * p2 >= n) ? std::max(0, i - p2) : lo;
        const int effHi = (2 * p2 >= n) ? std::min(n - 1, i + p2) : hi;
        double aux = 0.0;
        for (int idx = effLo; idx <= effHi; ++idx)
            aux += y[static_cast<size_t>(idx)];
        s[static_cast<size_t>(i)] = aux / static_cast<double>(effHi - effLo + 1);
    }
    y = std::move(s);
}

/**
 * \brief Compute Savitzky-Golay coefficients and store them into #h.
 *
 * This function follows GSL conventions in that it writes its result into a matrix allocated by
 * the caller and returns a non-zero result on error.
 *
 * The coefficient matrix is defined as the matrix H mapping a set of input values to the values
 * of the polynomial of order #polynom_order which minimizes squared deviations from the input
 * values. It is computed using the formula \$H=V(V^TV)^(-1)V^T\$, where \$V\$ is the Vandermonde
 * matrix of the point indices.
 *
 * For a short description of the mathematical background, see
 * http://www.statistics4u.info/fundstat_eng/cc_filter_savgol_math.html
 */
int SmoothFilter::savitzkyGolayCoefficients(int points, int polynom_order, gsl_matrix *h)
{
    int error = 0; // catch GSL error codes

    // compute Vandermonde matrix
    gsl_matrix *vandermonde = gsl_matrix_alloc(points, polynom_order + 1);
    for (int i = 0; i < points; ++i) {
        gsl_matrix_set(vandermonde, i, 0, 1.0);
        for (int j = 1; j <= polynom_order; ++j)
            gsl_matrix_set(vandermonde, i, j, gsl_matrix_get(vandermonde, i, j - 1) * i);
    }

    // compute V^TV
    gsl_matrix *vtv = gsl_matrix_alloc(polynom_order + 1, polynom_order + 1);
    error = gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, vandermonde, vandermonde, 0.0, vtv);

    if (!error) {
        // compute (V^TV)^(-1) using LU decomposition
        gsl_permutation *p = gsl_permutation_alloc(polynom_order + 1);
        int signum;
        error = gsl_linalg_LU_decomp(vtv, p, &signum);

        if (!error) {
            gsl_matrix *vtv_inv = gsl_matrix_alloc(polynom_order + 1, polynom_order + 1);
            error = gsl_linalg_LU_invert(vtv, p, vtv_inv);
            if (!error) {
                // compute (V^TV)^(-1)V^T
                gsl_matrix *vtv_inv_vt = gsl_matrix_alloc(polynom_order + 1, points);
                error = gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, vtv_inv, vandermonde, 0.0,
                                       vtv_inv_vt);

                if (!error) {
                    // finally, compute H = V(V^TV)^(-1)V^T
                    error = gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, vandermonde, vtv_inv_vt,
                                           0.0, h);
                }
                gsl_matrix_free(vtv_inv_vt);
            }
            gsl_matrix_free(vtv_inv);
        }
        gsl_permutation_free(p);
    }
    gsl_matrix_free(vtv);
    gsl_matrix_free(vandermonde);

    return error;
}

/**
 * \brief Savitzky-Golay smoothing of (uniformly distributed) data.
 *
 * When the data is not uniformly distributed, Savitzky-Golay looses its interesting conservation
 * properties. On the other hand, a central point of the algorithm is that for uniform data, the
 * operation can be implemented as a convolution. This is considerably more efficient than a more
 * generic method (see smoothModifiedSavGol()) able to handle non-uniform input data.
 *
 * There are at least three possible approaches to handling edges of the data vector (cutting them
 * off, zero padding and using the left-/rightmost smoothing polynomial for computing smoothed
 * values near the edges). Zero-padding is a particularly bad choice for signals with a distinctly
 * non-zero baseline and cutting off edges makes further computations on the original and smoothed
 * signals more difficult; therefore, deviating from the user-specified number of left/right
 * adjacent points (by smoothing over a fixed window at the edges) would be the least annoying
 * alternative; if it were not for the fact that previous versions of SciDAVis had a different
 * behaviour and such a subtle change to the behaviour would be even more annoying, especially
 * between bugfix releases. TODO: reconsider issue for next minor release (also: would it help
 * to add an "edge behaviour" option to the UI?)
 */
void SmoothFilter::smoothSavGol(std::vector<double>&, std::vector<double>&  y_inout)
{
    // total number of points in smoothing window
    int points = d_left_points + d_right_points + 1;

    if (points < d_polynom_order + 1) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("The polynomial order must be lower than the number of left "
                                     "points plus the number of right points!"));
        d_init_err = true;
        return;
    }

    if (y_inout.size() < unsigned(points)) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("ToriGraph") + " - " + tr("Error"),
                    tr("Tried to smooth over more points (left+right+1=%1) than given as input (%2).")
                            .arg(points)
                            .arg(y_inout.size()));
        d_init_err = true;
        return;
    }

    // Savitzky-Golay coefficient matrix, y' = H y
    gsl_matrix *h = gsl_matrix_alloc(points, points);
    if (int error = savitzkyGolayCoefficients(points, d_polynom_order, h)) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Internal error in Savitzky-Golay algorithm.\n")
                                          + gsl_strerror(error));
        gsl_matrix_free(h);
        d_init_err = true;
        return;
    }

    // allocate memory for the result (temporary; don't overwrite y_inout while we still read from
    // it)
    std::vector<double> result(y_inout.size());

    // near left edge: use interpolation of (points) left-most input values
    // i.e. we deviate from the specified left/right points to use
    /*
    for (int i=0; i<d_left_points; i++) {
            double convolution = 0.0;
            for (int k=0; k<points; k++)
                    convolution += gsl_matrix_get(h, i, k) * y_inout[k];
            result[i] =  convolution;
    }
    */
    // legacy behaviour: handle left edge by zero padding
    for (int i = 0; i < d_left_points; i++) {
        double convolution = 0.0;
        for (int k = d_left_points - i; k < points; k++)
            convolution += gsl_matrix_get(h, d_left_points, k) * y_inout[i - d_left_points + k];
        result[i] = convolution;
    }
    // central part: convolve with fixed row of h (as given by number of left points to use)
    for (unsigned i = d_left_points; i < y_inout.size() - d_right_points; i++) {
        double convolution = 0.0;
        for (int k = 0; k < points; k++)
            convolution += gsl_matrix_get(h, d_left_points, k) * y_inout[i - d_left_points + k];
        result[i] = convolution;
    }
    // legacy behaviour: handle right edge by zero padding
    for (unsigned i = y_inout.size() - d_right_points; i < y_inout.size(); i++) {
        double convolution = 0.0;
        for (unsigned k = 0; i - d_left_points + k < y_inout.size(); k++)
            convolution += gsl_matrix_get(h, d_left_points, k) * y_inout[i - d_left_points + k];
        result[i] = convolution;
    }

    // deallocate memory
    gsl_matrix_free(h);

    // write result into y_inout
    y_inout = std::move(result);
}

/**
 * \brief A variant of the Savitzky-Golay algorithm able to handle non-uniformly distributed data.
 *
 * In comparison to smoothSavGol(), this method trades proper handling of the X coordinates for
 * runtime efficiency by abandoning a central idea of Savitzky-Golay algorithm, namely that
 * polynomial smoothing can be expressed as a convolution.
 *
 * TODO: integrate this option into the GUI.
 */
void SmoothFilter::smoothModifiedSavGol(std::vector<double>& x_in, std::vector<double>&  y_inout)
{
    // total number of points in smoothing window
    int points = d_left_points + d_right_points + 1;

    if (points < d_polynom_order + 1) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("The polynomial order must be lower than the number of left "
                                     "points plus the number of right points!"));
        d_init_err = true;
        return;
    }

    if (x_in.size() < unsigned(points) || y_inout.size() < unsigned(points)) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("ToriGraph") + " - " + tr("Error"),
                    tr("Tried to smooth over more points (left+right+1=%1) than given as input (%2).")
                            .arg(points)
                            .arg(y_inout.size()));
        d_init_err = true;
        return;
    }

    // allocate memory for the result
    std::vector<double> result(x_in.size());

    // allocate memory for the linear algegra computations
    // Vandermonde matrix for x values of points in the current smoothing window
    gsl_matrix *vandermonde = gsl_matrix_alloc(points, d_polynom_order + 1);
    // stores part of the QR decomposition of vandermonde
    gsl_vector *tau = gsl_vector_alloc(qMin(points, d_polynom_order + 1));
    // coefficients of polynomial approximation computed for each smoothing window
    gsl_vector *poly = gsl_vector_alloc(d_polynom_order + 1);
    // residual of the (least-squares) approximation (by-product of GSL's algorithm)
    gsl_vector *residual = gsl_vector_alloc(points);

    for (unsigned target_index = 0; target_index < x_in.size(); target_index++) {
        int offset = target_index - d_left_points;
        // use a fixed number of points; near left/right borders, use offset to change
        // effective number of left/right points considered
        if (target_index < unsigned(d_left_points))
            offset += d_left_points - target_index;
        else if (unsigned(target_index + d_right_points) >= y_inout.size())
            offset += y_inout.size() - 1 - (target_index + d_right_points);

        // fill Vandermonde matrix
        for (int i = 0; i < points; ++i) {
            gsl_matrix_set(vandermonde, i, 0, 1.0);
            for (int j = 1; j <= d_polynom_order; ++j)
                gsl_matrix_set(vandermonde, i, j,
                               gsl_matrix_get(vandermonde, i, j - 1) * x_in[offset + i]);
        }

        // Y values within current smoothing window
        gsl_vector_view y_slice = gsl_vector_view_array(y_inout.data() + offset, points);

        // compute QR decomposition of Vandermonde matrix
        if (int error = gsl_linalg_QR_decomp(vandermonde, tau)) {
            if (!isSilentOrNonInteractive())
                QMessageBox::critical(
                        (ApplicationWindow *)parent(), tr("ToriGraph") + " - " + tr("Error"),
                        tr("Internal error in Savitzky-Golay algorithm: QR decomposition failed.\n")
                                + gsl_strerror(error));
        }
        // least-squares-solve vandermonde*poly=y_slice using the QR decomposition now stored in
        // vandermonde and tau
        else if (int error =
                         gsl_linalg_QR_lssolve(vandermonde, tau, &y_slice.vector, poly, residual)) {
            if (!isSilentOrNonInteractive())
                QMessageBox::critical((ApplicationWindow *)parent(),
                                      tr("ToriGraph") + " - " + tr("Error"),
                                      tr("Internal error in Savitzky-Golay algorithm: least-squares "
                                         "solution failed.\n")
                                              + gsl_strerror(error));
        }
        else
            result[target_index] =
                    gsl_poly_eval(poly->data, d_polynom_order + 1, x_in[target_index]);
    }

    // deallocate memory
    gsl_vector_free(residual);
    gsl_vector_free(poly);
    gsl_vector_free(tau);
    gsl_matrix_free(vandermonde);

    // write result into y_inout
    y_inout = std::move(result);
}

void SmoothFilter::smoothLowess(std::vector<double> &x, std::vector<double> &y)
{
    const int n = static_cast<int>(y.size());
    if (n < 2)
        return;

    const double span = std::clamp(d_lowess_span, 2.0 / static_cast<double>(n), 1.0);
    const int r = std::clamp(static_cast<int>(std::ceil(span * static_cast<double>(n))), 2, n);

    // Sort indices by x ascending so r-nearest neighbors can be found in O(1) amortized per point
    std::vector<int> ord(n);
    std::iota(ord.begin(), ord.end(), 0);
    std::stable_sort(ord.begin(), ord.end(),
                     [&x](int a, int b) { return x[a] < x[b]; });

    std::vector<double> xs(n), ys(n);
    for (int i = 0; i < n; ++i) {
        xs[i] = x[ord[i]];
        ys[i] = y[ord[i]];
    }

    std::vector<double> robWeights(n, 1.0);
    std::vector<double> yhat(n, 0.0);
    std::vector<double> absRes(n, 0.0);

    double yMaxAbs = 0.0;
    for (double v : ys)
        yMaxAbs = std::max(yMaxAbs, std::fabs(v));

    // Cleveland (1979): initial fit + 2 bisquare robustness iterations (3 passes total)
    for (int iter = 0; iter < 3; ++iter) {
        int left = 0;
        int right = r - 1;

        for (int i = 0; i < n; ++i) {
            const double xi = xs[i];
            while (right + 1 < n
                   && std::fabs(xs[right + 1] - xi) < std::fabs(xs[left] - xi)) {
                ++left;
                ++right;
            }

            const double dLeft = std::fabs(xi - xs[left]);
            const double dRight = std::fabs(xs[right] - xi);
            const double dr = std::max(dLeft, dRight);

            if (dr <= 0.0) {
                double wSum = 0.0, wySum = 0.0;
                for (int k = left; k <= right; ++k) {
                    wSum += robWeights[k];
                    wySum += robWeights[k] * ys[k];
                }
                yhat[i] = (wSum > 0.0) ? (wySum / wSum) : ys[i];
                continue;
            }

            double nextDist = dr * 1.001;
            if (left > 0 && right + 1 < n) {
                nextDist = std::min(std::fabs(xi - xs[left - 1]), std::fabs(xs[right + 1] - xi));
            } else if (left > 0) {
                nextDist = std::fabs(xi - xs[left - 1]);
            } else if (right + 1 < n) {
                nextDist = std::fabs(xs[right + 1] - xi);
            }
            const double h = std::max(dr * 1.0001, 0.5 * (dr + std::max(dr, nextDist)));

            auto evalLocalLinear = [&](bool useRobustness, double &outVal) -> bool {
                double s0 = 0.0, s1 = 0.0, s2 = 0.0, t0 = 0.0, t1 = 0.0;
                for (int k = left; k <= right; ++k) {
                    const double dx = xs[k] - xi;
                    const double u = std::fabs(dx) / h;
                    if (u >= 1.0)
                        continue;
                    const double oneMinusU3 = 1.0 - u * u * u;
                    const double wTri = oneMinusU3 * oneMinusU3 * oneMinusU3;
                    const double w = useRobustness ? (robWeights[k] * wTri) : wTri;
                    if (w <= 0.0)
                        continue;
                    s0 += w;
                    s1 += w * dx;
                    s2 += w * dx * dx;
                    t0 += w * ys[k];
                    t1 += w * dx * ys[k];
                }
                if (s0 <= 0.0)
                    return false;
                const double det = s0 * s2 - s1 * s1;
                if (det > 1e-18 * s0 * s0 * h * h)
                    outVal = (s2 * t0 - s1 * t1) / det;
                else
                    outVal = t0 / s0;
                return std::isfinite(outVal);
            };

            double val = ys[i];
            if (!evalLocalLinear(true, val))
                evalLocalLinear(false, val);
            yhat[i] = val;
        }

        if (iter < 2) {
            for (int i = 0; i < n; ++i)
                absRes[i] = std::fabs(ys[i] - yhat[i]);

            std::vector<double> sortedRes = absRes;
            const int mid = n / 2;
            std::nth_element(sortedRes.begin(), sortedRes.begin() + mid, sortedRes.end());
            double mad = sortedRes[mid];
            if (n % 2 == 0) {
                const double lowerMax = *std::max_element(sortedRes.begin(), sortedRes.begin() + mid);
                mad = 0.5 * (lowerMax + mad);
            }

            const double scale = 6.0 * mad;
            if (scale <= 1e-14 * (yMaxAbs + 1.0))
                break;

            for (int i = 0; i < n; ++i) {
                const double u = absRes[i] / scale;
                if (u >= 1.0) {
                    robWeights[i] = 0.0;
                } else {
                    const double oneMinusU2 = 1.0 - u * u;
                    robWeights[i] = oneMinusU2 * oneMinusU2;
                }
            }
        }
    }

    for (int i = 0; i < n; ++i)
        y[ord[i]] = yhat[i];
}

void SmoothFilter::smoothWhittaker(std::vector<double> &, std::vector<double> &y)
{
    const int n = static_cast<int>(y.size());
    const double lam = std::max(0.0, d_whittaker_lambda);
    if (n < 3 || lam <= 0.0)
        return;

    // Assemble SPD pentadiagonal matrix A = I + lam * D2^T * D2 in O(N)
    std::vector<double> a(n, 1.0);
    std::vector<double> b(n - 1, 0.0);
    std::vector<double> c(n - 2, 0.0);

    for (int r = 0; r < n - 2; ++r) {
        a[r] += lam;
        a[r + 1] += 4.0 * lam;
        a[r + 2] += lam;
        b[r] -= 2.0 * lam;
        b[r + 1] -= 2.0 * lam;
        c[r] += lam;
    }

    // Banded LDL^T factorization in O(N)
    std::vector<double> d(n, 0.0);
    std::vector<double> e(n - 1, 0.0);
    std::vector<double> f(n - 2, 0.0);

    d[0] = a[0];
    e[0] = b[0] / d[0];
    f[0] = c[0] / d[0];

    d[1] = a[1] - d[0] * e[0] * e[0];
    if (n > 2)
        e[1] = (b[1] - d[0] * f[0] * e[0]) / d[1];
    if (n > 3)
        f[1] = c[1] / d[1];

    for (int i = 2; i < n; ++i) {
        d[i] = a[i] - d[i - 2] * f[i - 2] * f[i - 2] - d[i - 1] * e[i - 1] * e[i - 1];
        if (i <= n - 2)
            e[i] = (b[i] - d[i - 1] * f[i - 1] * e[i - 1]) / d[i];
        if (i <= n - 3)
            f[i] = c[i] / d[i];
    }

    // Forward substitution L * z = y
    std::vector<double> z(n, 0.0);
    z[0] = y[0];
    z[1] = y[1] - e[0] * z[0];
    for (int i = 2; i < n; ++i)
        z[i] = y[i] - e[i - 1] * z[i - 1] - f[i - 2] * z[i - 2];

    // Diagonal scaling w = D^{-1} * z
    for (int i = 0; i < n; ++i)
        z[i] /= d[i];

    // Backward substitution L^T * yhat = w
    std::vector<double> yhat(n, 0.0);
    yhat[n - 1] = z[n - 1];
    yhat[n - 2] = z[n - 2] - e[n - 2] * yhat[n - 1];
    for (int i = n - 3; i >= 0; --i)
        yhat[i] = z[i] - e[i] * yhat[i + 1] - f[i] * yhat[i + 2];

    y = std::move(yhat);
}

void SmoothFilter::smoothMedian(std::vector<double> &, std::vector<double> &y)
{
    const int n = static_cast<int>(y.size());
    if (n < 2)
        return;

    int halfLeft = d_left_points;
    int halfRight = d_right_points;
    if (halfLeft <= 0) {
        const int half = (d_right_points <= 1) ? 0 : (d_right_points / 2);
        halfLeft = half;
        halfRight = half;
    }
    if (halfLeft == 0 && halfRight == 0)
        return;

    std::vector<double> result(n);
    std::vector<double> win;
    win.reserve(static_cast<size_t>(halfLeft + halfRight + 1));

    for (int i = 0; i < n; ++i) {
        const int lo = std::max(0, i - halfLeft);
        const int hi = std::min(n - 1, i + halfRight);
        win.assign(y.begin() + lo, y.begin() + hi + 1);
        const int m = static_cast<int>(win.size());
        const int mid = m / 2;
        std::nth_element(win.begin(), win.begin() + mid, win.end());
        double med = win[mid];
        if (m % 2 == 0) {
            const double lowerMax = *std::max_element(win.begin(), win.begin() + mid);
            med = 0.5 * (lowerMax + med);
        }
        result[i] = med;
    }

    y = std::move(result);
}

void SmoothFilter::setLowessSpan(double fraction)
{
    if (!std::isfinite(fraction) || fraction <= 0.0 || fraction > 1.0) {
        d_lowess_span = std::clamp(std::isfinite(fraction) ? fraction : 0.2, 0.01, 1.0);
        return;
    }
    d_lowess_span = fraction;
}

void SmoothFilter::setWhittakerLambda(double lambda)
{
    if (!std::isfinite(lambda) || lambda < 0.0) {
        d_whittaker_lambda = 100.0;
        return;
    }
    d_whittaker_lambda = lambda;
}

void SmoothFilter::setSmoothPoints(int points, int left_points)
{
    if (points < 0 || left_points < 0) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("The number of points must be positive!"));
        d_init_err = true;
        return;
    } else if (d_method == SavitzkyGolay && d_polynom_order > points + left_points) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("The polynomial order must be lower than the number of left "
                                     "points plus the number of right points!"));
        d_init_err = true;
        return;
    }

    d_right_points = points;
    d_left_points = left_points;
}

void SmoothFilter::setPolynomOrder(int order)
{
    if (d_method != SavitzkyGolay) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Setting polynomial order is only available for "
                                     "Savitzky-Golay smooth filters! Ignored option!"));
        return;
    }

    if (order > d_right_points + d_left_points) {
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(),
                                  tr("ToriGraph") + " - " + tr("Error"),
                                  tr("The polynomial order must be lower than the number of left "
                                     "points plus the number of right points!"));
        d_init_err = true;
        return;
    }
    d_polynom_order = order;
}
