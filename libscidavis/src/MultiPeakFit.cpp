/***************************************************************************
    File                 : MultiPeakFit.cpp
    Project              : SciDAVis / ToriGraph
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : MultiPeakFit module with OriginPro peak shapes

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
#include "MultiPeakFit.h"
#include "ApplicationWindow.h"
#include "Graph.h"
#include "Plot.h"
#include "fit_gsl.h"
#include "PlotCurve.h"
#include "FunctionCurve.h"
#include "ColorButton.h"
#include "Table.h"
#include "core/column/Column.h"

#include <qwt_plot_curve.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QRegExp>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <numeric>

using namespace std;

namespace {

bool isNonInteractiveFit(const QObject *parentObj, bool silent = false)
{
    if (silent)
        return true;
    if (qApp && qApp->property("torigraph_noninteractive").toBool())
        return true;
    const ApplicationWindow *app = dynamic_cast<const ApplicationWindow *>(parentObj);
    return app && app->isHeadlessMode();
}

inline double effectiveDoniachAlpha(double alpha)
{
    return (!std::isfinite(alpha) || alpha < 0.0 || alpha >= 0.85) ? 0.15 : alpha;
}

// Shape metrics of a unit-amplitude (A = 1), unit-width (w = 1) Asym2Sig / DoniachSunjic peak.
// Both profiles are pure functions of (x - xc) / w scaled by A, so for arbitrary (A, w):
//   area = A * w * unitArea, height = A * unitHeight, FWHM = w * unitFwhm.
struct AsymUnitShape
{
    double area = 0.0;
    double height = 0.0;
    double fwhm = 1.0;
};

template<class F>
double adaptiveSimpsonRec(const F &f, double a, double b, double fa, double fm, double fb,
                          double whole, double eps, int depth)
{
    const double m = 0.5 * (a + b);
    const double lm = 0.5 * (a + m), rm = 0.5 * (m + b);
    const double flm = f(lm), frm = f(rm);
    const double left = (m - a) / 6.0 * (fa + 4.0 * flm + fm);
    const double right = (b - m) / 6.0 * (fm + 4.0 * frm + fb);
    const double delta = left + right - whole;
    if (depth <= 0 || std::abs(delta) <= 15.0 * eps)
        return left + right + delta / 15.0;
    return adaptiveSimpsonRec(f, a, m, fa, flm, fm, left, 0.5 * eps, depth - 1)
            + adaptiveSimpsonRec(f, m, b, fm, frm, fb, right, 0.5 * eps, depth - 1);
}

// Integrates f over [a, b] split into nPieces fixed sub-intervals (so narrow features are never
// skipped by the initial coarse Simpson sample), each refined adaptively.
template<class F>
double piecewiseAdaptiveIntegral(const F &f, double a, double b, int nPieces, double eps)
{
    if (!(b > a) || nPieces < 1)
        return 0.0;
    const double h = (b - a) / double(nPieces);
    double total = 0.0;
    for (int i = 0; i < nPieces; ++i) {
        const double lo = a + h * double(i);
        const double hi = (i + 1 == nPieces) ? b : (lo + h);
        const double fa = f(lo), fb = f(hi), fm = f(0.5 * (lo + hi));
        const double whole = (hi - lo) / 6.0 * (fa + 4.0 * fm + fb);
        total += adaptiveSimpsonRec(f, lo, hi, fa, fm, fb, whole, eps / double(nPieces), 16);
    }
    return total;
}

AsymUnitShape computeAsymUnitShape(int prof, double alphaEff)
{
    AsymUnitShape s;
    auto g = [prof, alphaEff](double dx) -> double {
        const double v = eval_origin_peak_profile(prof, 1.0, 0.0, 1.0, dx, 0.5, 1.5, alphaEff, 1.0);
        return (std::isfinite(v) && v > 0.0) ? v : 0.0;
    };
    const bool isDS = (prof == int(MultiPeakFit::DoniachSunjic));

    // --- Area ---
    if (isDS) {
        // DS has algebraic tails: right ~ (1-a) u^(a-2), left ~ sin(pi a) |u|^(a-1)
        // + cos(pi a)(1-a) |u|^(a-2) with u = 2 dx / w. The sin(pi a) left tail is not
        // integrable for a > 0 (the DS line has formally infinite area), so the area is defined
        // on the window |dx| <= 25 w (|u| <= 50) plus the analytic convergent u^(a-2) tails
        // (which makes alpha = 0 reproduce the exact Lorentzian area pi w / 2 * A).
        const double U = 50.0;
        const double T = std::asinh(U);
        auto integrand = [&g](double t) { return g(0.5 * std::sinh(t)) * 0.5 * std::cosh(t); };
        double area = piecewiseAdaptiveIntegral(integrand, -T, T, 48, 1e-10);
        const double a = alphaEff;
        area += 0.5 * std::pow(U, a - 1.0) * (1.0 + std::cos(M_PI * a));
        s.area = area;
    } else {
        // Asym2Sig: exponential sigmoid edges at dx = -w/2 (scale s1) and +w/2 (scale s2)
        const double a = (alphaEff > 0.05) ? alphaEff : 1.5;
        const double s1 = std::max(1e-15, 0.25 / (1.0 + a));
        const double s2 = std::max(1e-15, 0.25 * a / (1.0 + a));
        const double lo = std::min(-25.0, -0.5 - 60.0 * s1);
        const double hi = std::max(25.0, 0.5 + 60.0 * s2);
        std::vector<double> br = { lo, -0.5 - 12.0 * s1, -0.5 + 12.0 * s1, 0.5 - 12.0 * s2,
                                   0.5 + 12.0 * s2, hi };
        for (double &v : br)
            v = std::min(hi, std::max(lo, v));
        std::sort(br.begin(), br.end());
        double area = 0.0;
        for (size_t i = 0; i + 1 < br.size(); ++i)
            area += piecewiseAdaptiveIntegral(g, br[i], br[i + 1], 16, 1e-10);
        s.area = area;
    }

    // --- Height (maximum) ---
    const int nScan = 6001;
    const double scanLo = -3.0, scanHi = 3.0;
    const double scanStep = (scanHi - scanLo) / double(nScan - 1);
    int iMax = 0;
    double vMax = -1.0;
    for (int i = 0; i < nScan; ++i) {
        const double v = g(scanLo + scanStep * double(i));
        if (v > vMax) {
            vMax = v;
            iMax = i;
        }
    }
    double xa = scanLo + scanStep * double(std::max(0, iMax - 1));
    double xb = scanLo + scanStep * double(std::min(nScan - 1, iMax + 1));
    const double gr = 0.5 * (std::sqrt(5.0) - 1.0);
    double xc1 = xb - gr * (xb - xa), xc2 = xa + gr * (xb - xa);
    double f1 = g(xc1), f2 = g(xc2);
    for (int it = 0; it < 80; ++it) {
        if (f1 >= f2) {
            xb = xc2;
            xc2 = xc1;
            f2 = f1;
            xc1 = xb - gr * (xb - xa);
            f1 = g(xc1);
        } else {
            xa = xc1;
            xc1 = xc2;
            f1 = f2;
            xc2 = xa + gr * (xb - xa);
            f2 = g(xc2);
        }
    }
    double xPeak = 0.5 * (xa + xb);
    double hPeak = g(xPeak);
    if (vMax > hPeak) {
        hPeak = vMax;
        xPeak = scanLo + scanStep * double(iMax);
    }
    s.height = std::max(0.0, hPeak);

    // --- Measured FWHM (left/right half-maximum crossings, bracketed then bisected) ---
    s.fwhm = 1.0;
    if (s.height > 1e-300) {
        const double half = 0.5 * s.height;
        auto findCrossing = [&](double dir, double &xOut) -> bool {
            double inner = xPeak, step = 1e-3;
            for (int k = 0; k < 400; ++k) {
                const double outer = inner + dir * step;
                if (g(outer) <= half) {
                    double aIn = inner, aOut = outer;
                    for (int it = 0; it < 100; ++it) {
                        const double mid = 0.5 * (aIn + aOut);
                        if (g(mid) > half)
                            aIn = mid;
                        else
                            aOut = mid;
                    }
                    xOut = 0.5 * (aIn + aOut);
                    return true;
                }
                inner = outer;
                step *= 1.1;
                if (std::abs(inner - xPeak) > 1e6)
                    break;
            }
            return false;
        };
        double xl = 0.0, xr = 0.0;
        if (findCrossing(-1.0, xl) && findCrossing(1.0, xr) && xr > xl)
            s.fwhm = xr - xl;
    }
    if (!std::isfinite(s.area) || s.area < 0.0)
        s.area = 0.0;
    if (!std::isfinite(s.fwhm) || s.fwhm <= 0.0)
        s.fwhm = 1.0;
    return s;
}

// Cached per-thread (the shape only depends on profile and alpha; the multi-peak fit callbacks
// query it on every function evaluation when doublet constraints are active).
AsymUnitShape asymUnitShape(int prof, double alphaEff)
{
    struct Slot
    {
        bool valid = false;
        double alpha = 0.0;
        AsymUnitShape shape;
    };
    thread_local Slot cacheSlots[2];
    Slot &sl = cacheSlots[(prof == int(MultiPeakFit::DoniachSunjic)) ? 1 : 0];
    if (!sl.valid || sl.alpha != alphaEff) {
        sl.shape = computeAsymUnitShape(prof, alphaEff);
        sl.alpha = alphaEff;
        sl.valid = true;
    }
    return sl.shape;
}

} // namespace

MultiPeakFit::MultiPeakFit(ApplicationWindow *parent, Graph *g, PeakProfile profile, int peaks)
    : Fit(parent, g), d_profile(profile)
{
    setObjectName(tr("MultiPeak"));

    d_param_init = NULL;
    covar = NULL;
    d_peaks = 0;
    if (d_profile == DoniachSunjic)
        d_asym_alpha = 0.15;

    updateProfileFunctions();
    setNumPeaks(peaks);

    generate_peak_curves = true;
    d_peaks_color = QColor("#2ca02c"); // publication green
}

void MultiPeakFit::setDataCurve(int curve, double start, double end)
{
    Fit::setDataCurve(curve, start, end);
    d_detected_2nd_deriv_centers.clear();
    d_has_seed_request = false;
    d_seed_centers.clear();
    d_baseline_subtracted = false;
    d_last_baseline.clear();
}

MultiPeakFit::PeakProfile MultiPeakFit::peakProfileAt(int peakIndex) const
{
    if (peakIndex >= 0 && peakIndex < d_per_peak_profiles.size())
        return d_per_peak_profiles[peakIndex];
    return d_profile;
}

bool MultiPeakFit::usesCustomMultiPeakCallbacks() const
{
    return !d_per_peak_profiles.isEmpty() || !d_doublet_constraints.isEmpty()
            || d_negative_peaks_allowed;
}

double MultiPeakFit::estimateConstantBaselineY0(const std::vector<double> &y) const
{
    const size_t n = y.size();
    if (n == 0)
        return 0.0;
    std::vector<double> y_sorted = y;
    std::sort(y_sorted.begin(), y_sorted.end());
    const double y_min = y_sorted.front();
    const double y_max = y_sorted.back();
    const double y_p10 = y_sorted[std::min(n - 1, n / 10)];
    const double y_ends = 0.5 * (y.front() + y.back());
    if (!d_negative_peaks_allowed)
        return std::max(y_min, std::min(y_p10, y_ends));

    const double y_med = y_sorted[n / 2];
    const double y_p90 = y_sorted[std::min(n - 1, (9 * n) / 10)];
    const double upExc = y_max - y_med;
    const double downExc = y_med - y_min;
    if (downExc > 1.25 * upExc && (y_ends - y_min) >= (y_max - y_ends))
        return std::min(y_max, std::max(y_p90, y_ends));
    if (upExc > 1.25 * downExc)
        return std::max(y_min, std::min(y_p10, y_ends));
    return 0.5 * (y_med + y_ends);
}

QString MultiPeakFit::profileDisplayName(PeakProfile profile)
{
    switch (profile) {
    case Gauss:
        return QStringLiteral("Gauss");
    case Lorentz:
        return QStringLiteral("Lorentz");
    case GaussAmp:
        return QStringLiteral("GaussAmp");
    case PseudoVoigt:
        return QStringLiteral("PseudoVoigt");
    case PearsonVII:
        return QStringLiteral("PearsonVII");
    case Voigt:
        return QStringLiteral("Voigt");
    case BiGaussian:
        return QStringLiteral("BiGaussian");
    case EMG:
        return QStringLiteral("EMG");
    case LogNormal:
        return QStringLiteral("LogNormal");
    case Asym2Sig:
        return QStringLiteral("Asym2Sig");
    case DoniachSunjic:
        return QStringLiteral("DoniachSunjic");
    }
    return QStringLiteral("Gauss");
}

QString MultiPeakFit::buildFormula() const
{
    if (d_peaks == 1 || d_per_peak_profiles.isEmpty()) {
        const PeakProfile prof = (d_peaks == 1) ? peakProfileAt(0) : d_profile;
        const double alpha =
                (prof == DoniachSunjic) ? effectiveDoniachAlpha(d_asym_alpha) : d_asym_alpha;
        return generateFormula(d_peaks, prof, d_voigt_mu, d_pearson_m, alpha, d_tail_tau);
    }
    QString formula = "y0+";
    for (int i = 0; i < d_peaks; ++i) {
        formula += instancePeakFormula(i + 1, peakProfileAt(i));
        if (i < d_peaks - 1)
            formula += "+";
    }
    return formula;
}

double MultiPeakFit::peakFWHMFromW(PeakProfile prof, double w) const
{
    double wSafe = std::max(1e-15, std::abs(w));
    switch (prof) {
    case Gauss:
    case EMG:
        return wSafe * std::sqrt(2.0 * M_LN2);
    case GaussAmp:
        return wSafe * 2.0 * std::sqrt(2.0 * M_LN2);
    case Asym2Sig:
    case DoniachSunjic: {
        const double alphaEff =
                (prof == DoniachSunjic) ? effectiveDoniachAlpha(d_asym_alpha) : d_asym_alpha;
        return wSafe * asymUnitShape(int(prof), alphaEff).fwhm;
    }
    default:
        return wSafe;
    }
}

double MultiPeakFit::wFromPeakFWHM(PeakProfile prof, double fwhm) const
{
    double fSafe = std::max(1e-15, std::abs(fwhm));
    switch (prof) {
    case Gauss:
    case EMG:
        return fSafe / std::sqrt(2.0 * M_LN2);
    case GaussAmp:
        return fSafe / (2.0 * std::sqrt(2.0 * M_LN2));
    case Asym2Sig:
    case DoniachSunjic: {
        const double alphaEff =
                (prof == DoniachSunjic) ? effectiveDoniachAlpha(d_asym_alpha) : d_asym_alpha;
        const double uf = asymUnitShape(int(prof), alphaEff).fwhm;
        return (uf > 1e-12) ? (fSafe / uf) : fSafe;
    }
    default:
        return fSafe;
    }
}

double MultiPeakFit::peakTrueArea(PeakProfile prof, double rawA, double w) const
{
    double aAbs = std::abs(rawA);
    double wSafe = std::max(1e-15, std::abs(w));
    switch (prof) {
    case Gauss:
    case Lorentz:
    case PseudoVoigt:
    case Voigt:
    case EMG:
        return aAbs;
    case GaussAmp:
        return aAbs * wSafe * std::sqrt(2.0 * M_PI);
    case PearsonVII: {
        double m = std::max(0.55, d_pearson_m);
        double cm = std::pow(2.0, 1.0 / m) - 1.0;
        return aAbs * wSafe * (std::sqrt(M_PI) * std::tgamma(m - 0.5))
                / (2.0 * std::sqrt(cm) * std::tgamma(m));
    }
    case BiGaussian:
        return 0.5 * std::sqrt(M_PI / M_LN2) * aAbs * wSafe;
    case LogNormal: {
        double asym = (std::abs(d_asym_alpha) > 1e-4) ? std::abs(d_asym_alpha) : 1.5;
        double b = std::log(asym + std::sqrt(asym * asym + 1.0));
        return aAbs * wSafe * std::sqrt(M_PI / (4.0 * M_LN2)) * (b / asym)
                * std::exp((b * b) / (4.0 * M_LN2));
    }
    case Asym2Sig:
    case DoniachSunjic: {
        // Full-tail adaptive integral (Asym2Sig) / ±25w window + analytic tails (DS) instead of
        // the former fixed ±4w trapezoid that under-estimated the area (E4-7).
        double alphaEff =
                (prof == DoniachSunjic) ? effectiveDoniachAlpha(d_asym_alpha) : d_asym_alpha;
        return aAbs * wSafe * asymUnitShape(int(prof), alphaEff).area;
    }
    }
    return aAbs;
}

double MultiPeakFit::rawParamFromTrueArea(PeakProfile prof, double targetArea, double w) const
{
    double areaPos = std::max(0.0, targetArea);
    double wSafe = std::max(1e-15, std::abs(w));
    switch (prof) {
    case Gauss:
    case Lorentz:
    case PseudoVoigt:
    case Voigt:
    case EMG:
        return areaPos;
    default: {
        double unitArea = peakTrueArea(prof, 1.0, wSafe);
        return (unitArea > 1e-15) ? (areaPos / unitArea) : areaPos;
    }
    }
}

void MultiPeakFit::applyDoubletConstraints(std::vector<double> &par) const
{
    if (d_doublet_constraints.isEmpty() || par.size() < size_t(3 * d_peaks + 1))
        return;

    int passes = std::max(1, d_doublet_constraints.size());
    for (int pass = 0; pass < passes; ++pass) {
        for (const auto &dc : d_doublet_constraints) {
            if (dc.priIdx < 0 || dc.priIdx >= d_peaks || dc.secIdx < 0 || dc.secIdx >= d_peaks
                || dc.priIdx == dc.secIdx)
                continue;
            PeakProfile profPri = peakProfileAt(dc.priIdx);
            PeakProfile profSec = peakProfileAt(dc.secIdx);
            double rawA_pri = par[3 * dc.priIdx];
            double xc_pri = par[3 * dc.priIdx + 1];
            double w_pri = std::max(1e-15, std::abs(par[3 * dc.priIdx + 2]));

            par[3 * dc.secIdx + 1] = xc_pri + dc.centerOffset;
            if (dc.shareWidth) {
                if (profPri == profSec)
                    par[3 * dc.secIdx + 2] = w_pri;
                else
                    par[3 * dc.secIdx + 2] = wFromPeakFWHM(profSec, peakFWHMFromW(profPri, w_pri));
            }
            double w_sec = std::max(1e-15, std::abs(par[3 * dc.secIdx + 2]));
            double areaPri = peakTrueArea(profPri, rawA_pri, w_pri);
            double areaSec = std::max(0.0, dc.areaRatio) * areaPri;
            const double signPri = (d_negative_peaks_allowed && rawA_pri < 0.0) ? -1.0 : 1.0;
            par[3 * dc.secIdx] = signPri * rawParamFromTrueArea(profSec, areaSec, w_sec);
        }
    }
}

int MultiPeakFit::custom_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    FitData *data = static_cast<FitData *>(params);
    MultiPeakFit *mp = static_cast<MultiPeakFit *>(data->fit);
    const size_t n = data->n;
    const size_t p = data->p;

    std::vector<double> par(p);
    for (size_t j = 0; j < p; ++j)
        par[j] = gsl_vector_get(x, j);
    mp->applyDoubletConstraints(par);

    const double y0 = par[p - 1];
    for (size_t i = 0; i < n; ++i) {
        const double sig = data->sigma ? data->sigma[i] : 1.0;
        if (!std::isfinite(sig) || sig <= 0.0 || sig >= 1e140) {
            gsl_vector_set(f, i, 0.0);
            continue;
        }
        double yi = y0;
        for (int k = 0; k < mp->d_peaks; ++k)
            yi += mp->evalPeakAt(par, k, data->X[i], false);
        gsl_vector_set(f, i, (yi - data->Y[i]) / sig);
    }
    return GSL_SUCCESS;
}

int MultiPeakFit::custom_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J)
{
    FitData *data = static_cast<FitData *>(params);
    MultiPeakFit *mp = static_cast<MultiPeakFit *>(data->fit);
    const size_t n = data->n;
    const size_t p = data->p;

    std::vector<double> parBase(p);
    for (size_t j = 0; j < p; ++j)
        parBase[j] = gsl_vector_get(x, j);

    for (size_t col = 0; col + 1 < p; ++col) {
        if (mp->isParameterFixed(int(col))) {
            for (size_t i = 0; i < n; ++i)
                gsl_matrix_set(J, i, col, 0.0);
            continue;
        }
        double h = std::max(1e-12, 1e-6 * std::abs(parBase[col]));
        std::vector<double> parPlus = parBase;
        std::vector<double> parMinus = parBase;
        parPlus[col] += h;
        parMinus[col] -= h;
        mp->applyDoubletConstraints(parPlus);
        mp->applyDoubletConstraints(parMinus);

        const double y0p = parPlus[p - 1];
        const double y0m = parMinus[p - 1];
        const double inv2h = 0.5 / h;
        for (size_t i = 0; i < n; ++i) {
            const double sig = data->sigma ? data->sigma[i] : 1.0;
            if (!std::isfinite(sig) || sig <= 0.0 || sig >= 1e140) {
                gsl_matrix_set(J, i, col, 0.0);
                continue;
            }
            double yp = y0p;
            double ym = y0m;
            for (int k = 0; k < mp->d_peaks; ++k) {
                yp += mp->evalPeakAt(parPlus, k, data->X[i], false);
                ym += mp->evalPeakAt(parMinus, k, data->X[i], false);
            }
            gsl_matrix_set(J, i, col, ((yp - ym) * inv2h) / sig);
        }
    }

    for (size_t i = 0; i < n; ++i) {
        const double sig = data->sigma ? data->sigma[i] : 1.0;
        if (mp->isParameterFixed(int(p - 1)) || !std::isfinite(sig) || sig <= 0.0
            || sig >= 1e140) {
            gsl_matrix_set(J, i, p - 1, 0.0);
        } else {
            gsl_matrix_set(J, i, p - 1, 1.0 / sig);
        }
    }
    return GSL_SUCCESS;
}

int MultiPeakFit::custom_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f,
                                        gsl_matrix *J)
{
    custom_multi_peak_f(x, params, f);
    custom_multi_peak_df(x, params, J);
    return GSL_SUCCESS;
}

double MultiPeakFit::custom_multi_peak_d(const gsl_vector *x, void *params)
{
    FitData *data = static_cast<FitData *>(params);
    gsl_vector *f = gsl_vector_alloc(data->n);
    custom_multi_peak_f(x, params, f);
    double val = 0.0;
    for (size_t i = 0; i < data->n; ++i) {
        double r = gsl_vector_get(f, i);
        val += r * r;
    }
    gsl_vector_free(f);
    return val;
}

void MultiPeakFit::updateProfileFunctions()
{
    if (usesCustomMultiPeakCallbacks()) {
        d_f = &MultiPeakFit::custom_multi_peak_f;
        d_df = &MultiPeakFit::custom_multi_peak_df;
        d_fdf = &MultiPeakFit::custom_multi_peak_fdf;
        d_fsimplex = &MultiPeakFit::custom_multi_peak_d;
        return;
    }

    switch (d_profile) {
    case Gauss:
        d_f = gauss_multi_peak_f;
        d_df = gauss_multi_peak_df;
        d_fdf = gauss_multi_peak_fdf;
        d_fsimplex = gauss_multi_peak_d;
        break;
    case Lorentz:
        d_f = lorentz_multi_peak_f;
        d_df = lorentz_multi_peak_df;
        d_fdf = lorentz_multi_peak_fdf;
        d_fsimplex = lorentz_multi_peak_d;
        break;
    case GaussAmp:
        d_f = gauss_amp_multi_peak_f;
        d_df = gauss_amp_multi_peak_df;
        d_fdf = gauss_amp_multi_peak_fdf;
        d_fsimplex = gauss_amp_multi_peak_d;
        break;
    case PseudoVoigt:
        d_f = pseudovoigt_multi_peak_f;
        d_df = pseudovoigt_multi_peak_df;
        d_fdf = pseudovoigt_multi_peak_fdf;
        d_fsimplex = pseudovoigt_multi_peak_d;
        break;
    case PearsonVII:
        d_f = pearson7_multi_peak_f;
        d_df = pearson7_multi_peak_df;
        d_fdf = pearson7_multi_peak_fdf;
        d_fsimplex = pearson7_multi_peak_d;
        break;
    case Voigt:
        d_f = voigt_multi_peak_f;
        d_df = voigt_multi_peak_df;
        d_fdf = voigt_multi_peak_fdf;
        d_fsimplex = voigt_multi_peak_d;
        break;
    case BiGaussian:
        d_f = bigaussian_multi_peak_f;
        d_df = bigaussian_multi_peak_df;
        d_fdf = bigaussian_multi_peak_fdf;
        d_fsimplex = bigaussian_multi_peak_d;
        break;
    case EMG:
        d_f = emg_multi_peak_f;
        d_df = emg_multi_peak_df;
        d_fdf = emg_multi_peak_fdf;
        d_fsimplex = emg_multi_peak_d;
        break;
    case LogNormal:
        d_f = lognormal_multi_peak_f;
        d_df = lognormal_multi_peak_df;
        d_fdf = lognormal_multi_peak_fdf;
        d_fsimplex = lognormal_multi_peak_d;
        break;
    case Asym2Sig:
        d_f = asym2sig_multi_peak_f;
        d_df = asym2sig_multi_peak_df;
        d_fdf = asym2sig_multi_peak_fdf;
        d_fsimplex = asym2sig_multi_peak_d;
        break;
    case DoniachSunjic:
        d_f = doniach_sunjic_multi_peak_f;
        d_df = doniach_sunjic_multi_peak_df;
        d_fdf = doniach_sunjic_multi_peak_fdf;
        d_fsimplex = doniach_sunjic_multi_peak_d;
        break;
    }
}

void MultiPeakFit::setPeakProfile(PeakProfile profile)
{
    d_profile = profile;
    d_per_peak_profiles.clear();
    if (d_profile == DoniachSunjic)
        d_asym_alpha = effectiveDoniachAlpha(d_asym_alpha);
    else if (d_profile != DoniachSunjic && d_asym_alpha < 0.3)
        d_asym_alpha = 1.5;
    updateProfileFunctions();
    setNumPeaks(d_peaks > 0 ? d_peaks : 1);
}

void MultiPeakFit::setLorentzFraction(double mu)
{
    d_voigt_mu = std::max(0.0, std::min(1.0, mu));
    d_formula = buildFormula();
}

void MultiPeakFit::setPearsonShape(double m)
{
    d_pearson_m = std::max(0.55, m);
    d_formula = buildFormula();
}

void MultiPeakFit::setAsymmetryFactor(double alpha)
{
    if (d_profile == DoniachSunjic)
        d_asym_alpha = effectiveDoniachAlpha(alpha);
    else
        d_asym_alpha = std::isfinite(alpha) ? std::max(0.05, alpha) : 1.5;
    d_formula = buildFormula();
}

void MultiPeakFit::setTailFactor(double tau)
{
    d_tail_tau = std::max(1e-4, tau);
    d_formula = buildFormula();
}

void MultiPeakFit::setBaselineMode(BaselineMode mode)
{
    d_baseline_mode = mode;
}

void MultiPeakFit::setBaselineAnchors(const QVector<QPointF> &anchors, bool cubicSpline)
{
    QVector<QPointF> valid;
    valid.reserve(anchors.size());
    for (const QPointF &pt : anchors) {
        if (std::isfinite(pt.x()) && std::isfinite(pt.y()))
            valid.push_back(pt);
    }
    std::sort(valid.begin(), valid.end(),
              [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });

    d_baseline_anchors.clear();
    for (int i = 0; i < valid.size();) {
        double xSum = valid[i].x();
        double ySum = valid[i].y();
        int cnt = 1;
        int j = i + 1;
        while (j < valid.size() && std::abs(valid[j].x() - valid[i].x()) <= 1e-12) {
            xSum += valid[j].x();
            ySum += valid[j].y();
            ++cnt;
            ++j;
        }
        d_baseline_anchors.push_back(QPointF(xSum / double(cnt), ySum / double(cnt)));
        i = j;
    }
    d_anchors_cubic_spline = cubicSpline;
    d_baseline_mode = BaselineAnchors;
}

void MultiPeakFit::setNumPeaks(int n)
{
    d_peaks = std::max(1, n);
    if (!d_per_peak_profiles.isEmpty() && d_per_peak_profiles.size() != d_peaks) {
        while (d_per_peak_profiles.size() < d_peaks)
            d_per_peak_profiles.push_back(d_profile);
        if (d_per_peak_profiles.size() > d_peaks)
            d_per_peak_profiles.resize(d_peaks);
    }

    if (!d_per_peak_profiles.isEmpty()) {
        d_explanation = tr("Mixed-Profile(%1) multi-peak").arg(d_peaks);
    } else {
        switch (d_profile) {
        case Gauss:
            d_explanation = tr("Gauss Fit");
            break;
        case Lorentz:
            d_explanation = tr("Lorentz Fit");
            break;
        case GaussAmp:
            d_explanation = tr("GaussAmp Fit");
            break;
        case PseudoVoigt:
            d_explanation = tr("PseudoVoigt Fit");
            break;
        case PearsonVII:
            d_explanation = tr("PearsonVII Fit");
            break;
        case Voigt:
            d_explanation = tr("Voigt Fit");
            break;
        case BiGaussian:
            d_explanation = tr("BiGaussian Fit");
            break;
        case EMG:
            d_explanation = tr("EMG Fit");
            break;
        case LogNormal:
            d_explanation = tr("LogNormal Fit");
            break;
        case Asym2Sig:
            d_explanation = tr("Asym2Sig Fit");
            break;
        case DoniachSunjic:
            d_explanation = tr("DoniachSunjic Fit");
            break;
        }
        if (d_peaks > 1)
            d_explanation += "(" + QString::number(d_peaks) + ") " + tr("multi-peak");
    }

    d_p = 3 * d_peaks + 1;
    d_min_points = d_p;

    if (d_param_init)
        gsl_vector_free(d_param_init);
    d_param_init = gsl_vector_alloc(d_p);
    gsl_vector_set_all(d_param_init, 1.0);

    if (covar)
        gsl_matrix_free(covar);
    covar = gsl_matrix_calloc(d_p, d_p);
    d_covar_scaled = false;
    d_results.resize(d_p);
    d_initial_guesses_set = false;

    d_param_names = generateParameterList(d_peaks);
    d_param_explain = generateExplanationList(d_peaks, d_profile, d_per_peak_profiles);
    d_formula = buildFormula();
    updateProfileFunctions();
}

QStringList MultiPeakFit::generateParameterList(int peaks)
{
    if (peaks == 1)
        return QStringList() << "A"
                             << "xc"
                             << "w"
                             << "y0";

    QStringList lst;
    for (int i = 0; i < peaks; i++) {
        QString index = QString::number(i + 1);
        lst << "A" + index;
        lst << "xc" + index;
        lst << "w" + index;
    }
    lst << "y0";
    return lst;
}

QStringList MultiPeakFit::generateExplanationList(int peaks)
{
    return generateExplanationList(peaks, Gauss);
}

bool MultiPeakFit::profileUsesAreaParameter(PeakProfile profile)
{
    // Area-normalised profiles: the A parameter is the integrated peak area.
    switch (profile) {
    case Gauss:
    case Lorentz:
    case PseudoVoigt:
    case Voigt:
    case EMG:
        return true;
    default:
        return false;
    }
}

QStringList MultiPeakFit::generateExplanationList(int peaks, PeakProfile profile,
                                                  const QVector<PeakProfile> &perPeak)
{
    auto aExplain = [&](int i, const QString &index) -> QString {
        const PeakProfile prof = (i < perPeak.size()) ? perPeak[i] : profile;
        if (profileUsesAreaParameter(prof))
            return index.isEmpty() ? tr("(area)") : tr("(area %1)").arg(index);
        return index.isEmpty() ? tr("(height)") : tr("(height %1)").arg(index);
    };
    if (peaks == 1)
        return QStringList() << aExplain(0, QString()) << tr("(center)") << tr("(width)")
                             << tr("(offset)");

    QStringList lst;
    for (int i = 0; i < peaks; i++) {
        QString index = QString::number(i + 1);
        lst << aExplain(i, index);
        lst << tr("(center %1)").arg(index);
        lst << tr("(width %1)").arg(index);
    }
    lst << tr("(offset)");
    return lst;
}

namespace {

QString fmtShape(double v)
{
    return QString::number(v, 'g', 10);
}

// Single peak term with parameter names A<sfx>, xc<sfx>, w<sfx>. The numeric shape constants are
// the values actually used by the solver, so the formula string reproduces the fitted curve.
QString peakTerm(const QString &sfx, int profile, double mu, double pearsonM, double alpha,
                 double tau)
{
    const QString A = "A" + sfx, xc = "xc" + sfx, w = "w" + sfx;
    const QString d = "(x-" + xc + ")";
    switch (profile) {
    case MultiPeakFit::Gauss:
        return "sqrt(2/PI)*" + A + "/" + w + "*exp(-2*" + d + "^2/" + w + "^2)";
    case MultiPeakFit::Lorentz:
        return "2*" + A + "/PI*" + w + "/(4*" + d + "^2+" + w + "^2)";
    case MultiPeakFit::GaussAmp:
        // OriginPro GaussAmp: w is the standard deviation sigma (FWHM = 2*sqrt(2 ln2)*w)
        return A + "*exp(-" + d + "^2/(2*" + w + "^2))";
    case MultiPeakFit::PseudoVoigt:
    case MultiPeakFit::Voigt: {
        double eta = std::max(0.0, std::min(1.0, mu));
        if (profile == MultiPeakFit::Voigt)
            eta = std::max(0.0, std::min(1.0, 1.36603 * eta - 0.47719 * eta * eta
                                                      + 0.11116 * eta * eta * eta));
        return A + "*(" + fmtShape(eta) + "*2/PI*" + w + "/(4*" + d + "^2+" + w + "^2)+"
                + fmtShape(1.0 - eta) + "*sqrt(4*ln(2)/PI)/" + w + "*exp(-4*ln(2)*" + d + "^2/"
                + w + "^2))";
    }
    case MultiPeakFit::PearsonVII: {
        const double m = std::max(0.55, pearsonM);
        return A + "*(1+(2^(1/" + fmtShape(m) + ")-1)*(2*" + d + "/" + w + ")^2)^(-"
                + fmtShape(m) + ")";
    }
    case MultiPeakFit::BiGaussian: {
        const double a = (alpha > 0.05) ? alpha : 1.5;
        const QString left = fmtShape(1.0 / (1.0 + a)), right = fmtShape(a / (1.0 + a));
        return "((x<" + xc + ")?(" + A + "*exp(-ln(2)*(" + d + "/(" + left + "*" + w
                + "))^2)):(" + A + "*exp(-ln(2)*(" + d + "/(" + right + "*" + w + "))^2)))";
    }
    case MultiPeakFit::EMG: {
        const QString t = fmtShape((tau > 1e-6) ? tau : 1.2);
        return "(" + A + "/(2*" + t + "))*exp((0.5*" + w + "/" + t + ")^2/2-" + d + "/" + t
                + ")*erfc((0.5*" + w + "/" + t + "-" + d + "/(0.5*" + w + "))/sqrt(2))";
    }
    case MultiPeakFit::LogNormal: {
        const double a = (std::abs(alpha) > 1e-4) ? std::abs(alpha) : 1.5;
        const QString twoA = fmtShape(2.0 * a);
        const QString b = fmtShape(std::log(a + std::sqrt(a * a + 1.0)));
        return "((1+" + twoA + "*" + d + "/" + w + ">0)?(" + A + "*exp(-ln(2)*(ln(1+" + twoA
                + "*" + d + "/" + w + ")/" + b + ")^2)):0)";
    }
    case MultiPeakFit::Asym2Sig: {
        const double a = (alpha > 0.05) ? alpha : 1.5;
        const QString s1 = fmtShape(0.25 / (1.0 + a)), s2 = fmtShape(0.25 * a / (1.0 + a));
        return A + "*(1/(1+exp(-(" + d + "+" + w + "/2)/(" + s1 + "*" + w + "))))*(1-1/(1+exp(-("
                + d + "-" + w + "/2)/(" + s2 + "*" + w + "))))";
    }
    case MultiPeakFit::DoniachSunjic: {
        const double a = (!std::isfinite(alpha) || alpha >= 0.85 || alpha < 0.0) ? 0.15 : alpha;
        const QString al = fmtShape(a);
        return A + "*cos(PI*" + al + "/2+(1-" + al + ")*atan(2*" + d + "/" + w + "))/(1+4*(" + d
                + "/" + w + ")^2)^((1-" + al + ")/2)";
    }
    }
    return QString();
}

} // namespace

QString MultiPeakFit::generateFormula(int peaks, PeakProfile profile)
{
    return generateFormula(peaks, profile, 0.5, 2.0, (profile == DoniachSunjic) ? 0.15 : 1.5,
                           1.2);
}

QString MultiPeakFit::generateFormula(int peaks, PeakProfile profile, double mu, double pearsonM,
                                      double asymAlpha, double tailTau)
{
    if (peaks == 1)
        return "y0+" + peakTerm(QString(), profile, mu, pearsonM, asymAlpha, tailTau);

    QString formula = "y0+";
    for (int i = 0; i < peaks; i++) {
        formula += peakFormula(i + 1, profile, mu, pearsonM, asymAlpha, tailTau);
        if (i < peaks - 1)
            formula += "+";
    }
    return formula;
}

QString MultiPeakFit::peakFormula(int peakIndex, PeakProfile profile)
{
    return peakFormula(peakIndex, profile, 0.5, 2.0, (profile == DoniachSunjic) ? 0.15 : 1.5,
                       1.2);
}

QString MultiPeakFit::peakFormula(int peakIndex, PeakProfile profile, double mu, double pearsonM,
                                  double asymAlpha, double tailTau)
{
    return peakTerm(QString::number(peakIndex), profile, mu, pearsonM, asymAlpha, tailTau);
}

QString MultiPeakFit::instancePeakFormula(int peakIndex, PeakProfile profile) const
{
    const double alpha = (profile == DoniachSunjic) ? effectiveDoniachAlpha(d_asym_alpha)
                                                    : d_asym_alpha;
    return peakFormula(peakIndex, profile, d_voigt_mu, d_pearson_m, alpha, d_tail_tau);
}

int MultiPeakFit::estimatePeakCount(double minProminenceRatio) const
{
    const size_t n = d_x.size();
    if (n < 3 || d_y.size() != n)
        return std::max(1, d_peaks);

    std::vector<double> y_sorted = d_y;
    std::sort(y_sorted.begin(), y_sorted.end());
    double y0 = (d_baseline_subtracted || d_baseline_mode == BaselineZero)
            ? 0.0
            : estimateConstantBaselineY0(d_y);
    double y_min = y_sorted.front();
    double y_max = y_sorted.back();
    double span = d_negative_peaks_allowed ? std::max(y_max - y0, y0 - y_min) : (y_max - y0);
    if (span <= 0.0)
        return 1;

    std::vector<double> y_smooth(n);
    y_smooth[0] = (2.0 * d_y[0] + d_y[1]) / 3.0;
    for (size_t i = 1; i + 1 < n; ++i)
        y_smooth[i] = (d_y[i - 1] + d_y[i] + d_y[i + 1]) / 3.0;
    y_smooth[n - 1] = (d_y[n - 2] + 2.0 * d_y[n - 1]) / 3.0;

    struct Cand
    {
        size_t idx;
        double prom;
    };
    std::vector<Cand> cands;
    for (size_t j = 1; j + 1 < n; ++j) {
        if (y_smooth[j] >= y_smooth[j - 1] && y_smooth[j] >= y_smooth[j + 1]
            && (y_smooth[j] > y_smooth[j - 1] || y_smooth[j] > y_smooth[j + 1])) {
            double h = d_y[j] - y0;
            if (h >= minProminenceRatio * span) {
                double min_left = y_smooth[j];
                for (int k = int(j) - 1; k >= 0; --k) {
                    if (y_smooth[k] > y_smooth[j])
                        break;
                    min_left = std::min(min_left, y_smooth[k]);
                }
                double min_right = y_smooth[j];
                for (size_t k = j + 1; k < n; ++k) {
                    if (y_smooth[k] > y_smooth[j])
                        break;
                    min_right = std::min(min_right, y_smooth[k]);
                }
                double prom = y_smooth[j] - std::max(min_left, min_right);
                if (prom >= 0.5 * minProminenceRatio * span)
                    cands.push_back({ j, prom });
            }
        }
        if (d_negative_peaks_allowed && y_smooth[j] <= y_smooth[j - 1]
            && y_smooth[j] <= y_smooth[j + 1]
            && (y_smooth[j] < y_smooth[j - 1] || y_smooth[j] < y_smooth[j + 1])) {
            double depth = y0 - d_y[j];
            if (depth >= minProminenceRatio * span) {
                double max_left = y_smooth[j];
                for (int k = int(j) - 1; k >= 0; --k) {
                    if (y_smooth[k] < y_smooth[j])
                        break;
                    max_left = std::max(max_left, y_smooth[k]);
                }
                double max_right = y_smooth[j];
                for (size_t k = j + 1; k < n; ++k) {
                    if (y_smooth[k] < y_smooth[j])
                        break;
                    max_right = std::max(max_right, y_smooth[k]);
                }
                double prom = std::min(max_left, max_right) - y_smooth[j];
                if (prom >= 0.5 * minProminenceRatio * span)
                    cands.push_back({ j, prom });
            }
        }
    }

    std::sort(cands.begin(), cands.end(),
              [](const Cand &a, const Cand &b) { return a.prom > b.prom; });

    int minSep = std::max(2, int(n) / 25);
    std::vector<size_t> chosen;
    for (const auto &c : cands) {
        bool far_enough = true;
        for (size_t sel : chosen) {
            if (std::abs(int(c.idx) - int(sel)) < minSep) {
                far_enough = false;
                break;
            }
        }
        if (far_enough)
            chosen.push_back(c.idx);
    }
    return std::max(1, int(chosen.size()));
}

int MultiPeakFit::autoDetectAndUpdateNumPeaks(int maxPeaks, double minProminenceRatio)
{
    int est = estimatePeakCount(minProminenceRatio);
    if (maxPeaks > 0)
        est = std::min(est, maxPeaks);
    est = std::max(1, est);
    setNumPeaks(est);
    autoDetectPeaks(est, minProminenceRatio);
    return est;
}

bool MultiPeakFit::autoDetectPeaks(int maxPeaks, double minProminenceRatio)
{
    if (maxPeaks > 0 && maxPeaks != d_peaks)
        setNumPeaks(maxPeaks);
    return seedPeaksFromCenters(std::vector<double>(),
                                minProminenceRatio > 0.0 ? minProminenceRatio : 0.08);
}

bool MultiPeakFit::seedPeaksFromCenters(const std::vector<double> &pickedCenters,
                                        double minProminenceRatio)
{
    d_has_seed_request = true;
    d_seed_centers = pickedCenters;
    d_seed_prominence = minProminenceRatio;

    const size_t n = d_x.size();
    if (n < 3 || d_y.size() != n)
        return false;

    int targetPeaks = std::max(1, d_peaks);

    // 1. Estimate baseline y0
    std::vector<double> y_sorted = d_y;
    std::sort(y_sorted.begin(), y_sorted.end());
    double y_min = y_sorted.front();
    double y_max = y_sorted.back();
    double y0 = (d_baseline_subtracted || d_baseline_mode == BaselineZero)
            ? 0.0
            : estimateConstantBaselineY0(d_y);
    double span = d_negative_peaks_allowed ? std::max(y_max - y0, y0 - y_min) : (y_max - y0);
    if (span <= 0.0)
        span = std::max(1e-12, std::max(std::abs(y_max), std::abs(y_min)));

    // 2. Smooth d_y with a 3-point moving average to suppress noise spikes
    std::vector<double> y_smooth(n);
    y_smooth[0] = (2.0 * d_y[0] + d_y[1]) / 3.0;
    for (size_t i = 1; i + 1 < n; ++i)
        y_smooth[i] = (d_y[i - 1] + d_y[i] + d_y[i + 1]) / 3.0;
    y_smooth[n - 1] = (d_y[n - 2] + 2.0 * d_y[n - 1]) / 3.0;

    // Full width at half height, in samples, of the extremum at idx of arr measured from base.
    // A side that reaches a valley first (the curve turns back towards a neighbouring peak by
    // more than a quarter of the height) or the end of the data gives no width; 0 when neither
    // side does.
    auto localFwhmIdx = [n](const std::vector<double> &arr, double base, size_t idx) -> double {
        const double h = arr[idx] - base;
        if (!(std::abs(h) > 0.0) || !std::isfinite(h))
            return 0.0;
        const double sgn = (h > 0.0) ? 1.0 : -1.0;
        const double hAbs = std::abs(h);
        auto rel = [&](long k) { return sgn * (arr[size_t(k)] - base); };
        auto side = [&](long dir) -> double {
            double lowest = hAbs;
            for (long k = long(idx) + dir; k >= 0 && k < long(n); k += dir) {
                const double v = rel(k);
                if (v <= 0.5 * hAbs) {
                    const double prev = rel(k - dir);
                    double frac = (prev - v > 0.0) ? (prev - 0.5 * hAbs) / (prev - v) : 0.5;
                    frac = std::max(0.0, std::min(1.0, frac));
                    return double(std::labs(k - long(idx))) - 1.0 + frac;
                }
                lowest = std::min(lowest, v);
                if (v - lowest > 0.25 * hAbs)
                    return -1.0;
            }
            return -1.0;
        };
        const double l = side(-1), r = side(+1);
        if (l > 0.0 && r > 0.0)
            return l + r;
        if (l > 0.0)
            return 2.0 * l;
        if (r > 0.0)
            return 2.0 * r;
        return 0.0;
    };

    // Width used when a peak's own width cannot be measured: the former point-count rule
    // (twice the old minimum separation n / max(12, 5k)).
    const double fallbackFwhmIdx =
            2.0 * double(std::max(2, int(n) / std::max(12, 5 * targetPeaks)));

    struct ChosenPeak
    {
        size_t idx = 0;
        double exactXc = 0.0;
        bool hasExactXc = false;
        double fwhmIdx = 0.0;
    };
    // Two extrema closer than half the narrower peak's FWHM are the same peak (noise on its
    // top). Resolved peaks of the data (separated by a valley) are farther apart than that, so
    // they are all kept; the former rule (1/15 of the point count) dropped a neighbour 1.7 FWHM
    // away.
    auto farEnough = [](size_t i, double fwhmI, size_t j, double fwhmJ) {
        const double sep = std::max(2.0, 0.5 * std::min(fwhmI, fwhmJ));
        return double(std::labs(long(i) - long(j))) >= sep;
    };
    std::vector<ChosenPeak> chosen;

    // Include any manually picked peak centers first
    for (double xc_pick : pickedCenters) {
        if (int(chosen.size()) >= targetPeaks)
            break;
        size_t bestIdx = 0;
        double bestDist = std::abs(d_x[0] - xc_pick);
        for (size_t i = 1; i < n; ++i) {
            double dist = std::abs(d_x[i] - xc_pick);
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = i;
            }
        }
        double w = localFwhmIdx(y_smooth, y0, bestIdx);
        chosen.push_back({ bestIdx, xc_pick, true, w > 0.0 ? w : fallbackFwhmIdx });
    }

    // 3. Find local extrema candidates
    struct PeakCand
    {
        size_t idx;
        double prom;
        double height;
        double fwhmIdx;
    };
    std::vector<PeakCand> cands;
    for (size_t j = 1; j + 1 < n; ++j) {
        if (y_smooth[j] >= y_smooth[j - 1] && y_smooth[j] >= y_smooth[j + 1]
            && (y_smooth[j] > y_smooth[j - 1] || y_smooth[j] > y_smooth[j + 1])) {
            size_t rawIdx = j;
            if (d_y[j - 1] > d_y[rawIdx])
                rawIdx = j - 1;
            if (d_y[j + 1] > d_y[rawIdx])
                rawIdx = j + 1;

            double h = d_y[rawIdx] - y0;
            if (h >= minProminenceRatio * span) {
                double min_left = y_smooth[j];
                for (int k = int(j) - 1; k >= 0; --k) {
                    if (y_smooth[k] > y_smooth[j])
                        break;
                    min_left = std::min(min_left, y_smooth[k]);
                }
                double min_right = y_smooth[j];
                for (size_t k = j + 1; k < n; ++k) {
                    if (y_smooth[k] > y_smooth[j])
                        break;
                    min_right = std::min(min_right, y_smooth[k]);
                }
                double prom = y_smooth[j] - std::max(min_left, min_right);
                if (prom >= 0.35 * minProminenceRatio * span)
                    cands.push_back({ rawIdx, prom, h, localFwhmIdx(y_smooth, y0, j) });
            }
        }
        if (d_negative_peaks_allowed && y_smooth[j] <= y_smooth[j - 1]
            && y_smooth[j] <= y_smooth[j + 1]
            && (y_smooth[j] < y_smooth[j - 1] || y_smooth[j] < y_smooth[j + 1])) {
            size_t rawIdx = j;
            if (d_y[j - 1] < d_y[rawIdx])
                rawIdx = j - 1;
            if (d_y[j + 1] < d_y[rawIdx])
                rawIdx = j + 1;

            double depth = y0 - d_y[rawIdx];
            if (depth >= minProminenceRatio * span) {
                double max_left = y_smooth[j];
                for (int k = int(j) - 1; k >= 0; --k) {
                    if (y_smooth[k] < y_smooth[j])
                        break;
                    max_left = std::max(max_left, y_smooth[k]);
                }
                double max_right = y_smooth[j];
                for (size_t k = j + 1; k < n; ++k) {
                    if (y_smooth[k] < y_smooth[j])
                        break;
                    max_right = std::max(max_right, y_smooth[k]);
                }
                double prom = std::min(max_left, max_right) - y_smooth[j];
                if (prom >= 0.35 * minProminenceRatio * span)
                    cands.push_back({ rawIdx, prom, depth, localFwhmIdx(y_smooth, y0, j) });
            }
        }
    }

    std::sort(cands.begin(), cands.end(), [](const PeakCand &a, const PeakCand &b) {
        if (std::abs(a.prom - b.prom) > 1e-12)
            return a.prom > b.prom;
        return a.height > b.height;
    });

    {
        // Candidates whose width could not be measured take the median of the measured ones.
        std::vector<double> ws;
        for (const auto &c : cands) {
            if (c.fwhmIdx > 0.0)
                ws.push_back(c.fwhmIdx);
        }
        double typical = fallbackFwhmIdx;
        if (!ws.empty()) {
            std::nth_element(ws.begin(), ws.begin() + long(ws.size() / 2), ws.end());
            typical = ws[ws.size() / 2];
        }
        for (auto &c : cands) {
            if (!(c.fwhmIdx > 0.0))
                c.fwhmIdx = typical;
        }
    }

    for (const auto &c : cands) {
        if (int(chosen.size()) >= targetPeaks)
            break;
        bool ok = true;
        for (const auto &sel : chosen) {
            if (!farEnough(c.idx, c.fwhmIdx, sel.idx, sel.fwhmIdx)) {
                ok = false;
                break;
            }
        }
        if (ok)
            chosen.push_back({ c.idx, d_x[c.idx], false, c.fwhmIdx });
    }

    // 4. Fallback if fewer than targetPeaks were found
    std::vector<double> residual(n);
    for (size_t i = 0; i < n; ++i) {
        residual[i] = d_negative_peaks_allowed ? std::abs(d_y[i] - y0)
                                               : std::max(0.0, d_y[i] - y0);
    }

    // Remove each chosen peak from the residual as a Gaussian of its own local width (the former
    // common width n/(4k) samples was far wider than narrow peaks and erased their neighbours).
    auto subtractFromResidual = [&](size_t idx, double fwhmIdx) {
        const double sigmaIdx = std::max(1.0, fwhmIdx / (2.0 * std::sqrt(2.0 * M_LN2)));
        const double h = residual[idx];
        for (size_t i = 0; i < n; ++i) {
            double d = (double(i) - double(idx)) / sigmaIdx;
            residual[i] = std::max(0.0, residual[i] - h * std::exp(-0.5 * d * d));
        }
    };
    for (const auto &sel : chosen)
        subtractFromResidual(sel.idx, sel.fwhmIdx);

    while (int(chosen.size()) < targetPeaks) {
        size_t bestIdx = 0;
        double bestVal = -1.0;
        for (size_t i = 1; i + 1 < n; ++i) {
            bool far_enough = true;
            for (const auto &sel : chosen) {
                if (!farEnough(i, sel.fwhmIdx, sel.idx, sel.fwhmIdx)) {
                    far_enough = false;
                    break;
                }
            }
            if (far_enough && residual[i] > bestVal) {
                bestVal = residual[i];
                bestIdx = i;
            }
        }
        if (bestVal <= 0.0) {
            bestIdx = std::min(n - 2,
                               std::max<size_t>(1, ((chosen.size() + 1) * n) / (targetPeaks + 1)));
        }
        double w = (bestVal > 0.0) ? localFwhmIdx(residual, 0.0, bestIdx) : 0.0;
        if (!(w > 0.0))
            w = fallbackFwhmIdx;
        chosen.push_back({ bestIdx, d_x[bestIdx], false, w });
        subtractFromResidual(bestIdx, w);
    }

    // Sort chosen peaks by xc ascending
    std::sort(chosen.begin(), chosen.end(), [this](const ChosenPeak &a, const ChosenPeak &b) {
        double xa = a.hasExactXc ? a.exactXc : d_x[a.idx];
        double xb = b.hasExactXc ? b.exactXc : d_x[b.idx];
        return xa < xb;
    });

    double xSpan = std::abs(d_x.back() - d_x.front());
    if (xSpan <= 0.0)
        xSpan = 1.0;
    double minW = (xSpan / double(n)) * 1.2;
    double maxW = xSpan * 0.8;

    auto initSinglePeakParams = [&](PeakProfile prof, double H, double fwhm, double &xcInOut,
                                    double &AOut, double &wOut) {
        AOut = 1.0;
        wOut = fwhm;
        double alphaEff =
                (prof == DoniachSunjic) ? effectiveDoniachAlpha(d_asym_alpha) : d_asym_alpha;
        switch (prof) {
        case Gauss:
            wOut = fwhm / std::sqrt(2.0 * M_LN2);
            AOut = H * wOut * std::sqrt(M_PI / 2.0);
            break;
        case Lorentz:
            wOut = fwhm;
            AOut = usesCustomMultiPeakCallbacks() ? (M_PI_2 * H * wOut) : (H * wOut);
            break;
        case GaussAmp:
            wOut = fwhm / (2.0 * std::sqrt(2.0 * M_LN2));
            AOut = H;
            break;
        case PseudoVoigt: {
            wOut = fwhm;
            double cG = std::sqrt(4.0 * M_LN2 / M_PI);
            double cPeak = d_voigt_mu * M_2_PI + (1.0 - d_voigt_mu) * cG;
            AOut = (H * wOut) / std::max(1e-12, cPeak);
            break;
        }
        case PearsonVII:
            wOut = fwhm;
            AOut = H;
            break;
        case Voigt: {
            wOut = fwhm;
            double d = std::max(0.0, std::min(1.0, d_voigt_mu));
            double eta = std::max(0.0,
                                  std::min(1.0, 1.36603 * d - 0.47719 * d * d + 0.11116 * d * d * d));
            double cG = std::sqrt(4.0 * M_LN2 / M_PI);
            double cPeak = eta * M_2_PI + (1.0 - eta) * cG;
            AOut = (H * wOut) / std::max(1e-12, cPeak);
            break;
        }
        case BiGaussian:
        case LogNormal:
            wOut = fwhm;
            AOut = H;
            break;
        case EMG: {
            wOut = std::max(minW, fwhm / std::sqrt(2.0 * M_LN2));
            for (int iter = 0; iter < 2; ++iter) {
                double bestH = 1e-12, bestDx = 0.0;
                double spanSearch = 3.0 * wOut + 4.0 * d_tail_tau;
                const int nGrid = 121;
                std::vector<double> gx(nGrid), gy(nGrid);
                int maxG = 0;
                for (int gi = 0; gi < nGrid; ++gi) {
                    gx[gi] = -2.0 * wOut
                            + (spanSearch + 2.0 * wOut) * double(gi) / double(nGrid - 1);
                    gy[gi] = eval_origin_peak_profile(EMG, 1.0, 0.0, wOut, gx[gi], d_voigt_mu,
                                                      d_pearson_m, alphaEff, d_tail_tau);
                    if (gy[gi] > bestH) {
                        bestH = gy[gi];
                        bestDx = gx[gi];
                        maxG = gi;
                    }
                }
                if (iter == 0 && bestH > 1e-12) {
                    double halfH = 0.5 * bestH;
                    double gl = gx.front(), gr = gx.back();
                    for (int gi = maxG; gi > 0; --gi) {
                        if (gy[gi - 1] <= halfH) {
                            double f = (halfH - gy[gi - 1]) / std::max(1e-15, gy[gi] - gy[gi - 1]);
                            gl = gx[gi - 1] + f * (gx[gi] - gx[gi - 1]);
                            break;
                        }
                    }
                    for (int gi = maxG; gi + 1 < nGrid; ++gi) {
                        if (gy[gi + 1] <= halfH) {
                            double f = (gy[gi] - halfH) / std::max(1e-15, gy[gi] - gy[gi + 1]);
                            gr = gx[gi] + f * (gx[gi + 1] - gx[gi]);
                            break;
                        }
                    }
                    double fwhm1 = gr - gl;
                    if (fwhm1 > 1e-12 * xSpan)
                        wOut = std::max(minW, std::min(maxW, wOut * (fwhm / fwhm1)));
                } else {
                    xcInOut -= bestDx;
                    AOut = H / std::max(1e-12, bestH);
                }
            }
            break;
        }
        case Asym2Sig:
        case DoniachSunjic: {
            wOut = fwhm;
            double bestH = 1e-12, bestDx = 0.0;
            for (int gi = -40; gi <= 40; ++gi) {
                double dx = (double(gi) / 40.0) * wOut;
                double v = eval_origin_peak_profile(int(prof), 1.0, 0.0, wOut, dx, d_voigt_mu,
                                                    d_pearson_m, alphaEff, d_tail_tau);
                if (v > bestH) {
                    bestH = v;
                    bestDx = dx;
                }
            }
            xcInOut -= bestDx;
            AOut = H / std::max(1e-12, bestH);
            break;
        }
        }
    };

    // 5. Estimate each peak's center xc, FWHM, and profile parameters (A_i, xc_i, w_i)
    for (int m = 0; m < targetPeaks; ++m) {
        size_t idx = chosen[m].idx;
        const double rawDelta = d_y[idx] - y0;
        const bool isTrough = d_negative_peaks_allowed && (rawDelta < 0.0);

        double xc = chosen[m].hasExactXc ? chosen[m].exactXc : d_x[idx];
        if (!chosen[m].hasExactXc && idx > 0 && idx + 1 < n) {
            double ym1 = d_y[idx - 1];
            double y0p = d_y[idx];
            double yp1 = d_y[idx + 1];
            double denom = 2.0 * (2.0 * y0p - ym1 - yp1);
            bool validExtrema = isTrough ? (y0p <= ym1 && y0p <= yp1) : (y0p >= ym1 && y0p >= yp1);
            if (std::abs(denom) > 1e-12 && validExtrema) {
                double delta = std::max(-0.5, std::min(0.5, (ym1 - yp1) / denom));
                xc = d_x[idx] + delta * 0.5 * (d_x[idx + 1] - d_x[idx - 1]);
            }
        }

        double H = std::max(1e-6 * span, isTrough ? (y0 - d_y[idx]) : (d_y[idx] - y0));
        double halfLevel = isTrough ? (y0 - 0.5 * H) : (y0 + 0.5 * H);

        size_t leftLimit = 0;
        if (m > 0) {
            size_t prevIdx = chosen[m - 1].idx;
            leftLimit = prevIdx;
            double extValley = d_y[idx];
            for (size_t k = prevIdx; k <= idx; ++k) {
                if ((!isTrough && d_y[k] < extValley) || (isTrough && d_y[k] > extValley)) {
                    extValley = d_y[k];
                    leftLimit = k;
                }
            }
        }

        size_t rightLimit = n - 1;
        if (m + 1 < targetPeaks) {
            size_t nextIdx = chosen[m + 1].idx;
            rightLimit = nextIdx;
            double extValley = d_y[idx];
            for (size_t k = idx; k <= nextIdx; ++k) {
                if ((!isTrough && d_y[k] < extValley) || (isTrough && d_y[k] > extValley)) {
                    extValley = d_y[k];
                    rightLimit = k;
                }
            }
        }

        bool foundLeft = false;
        double xLeft = d_x[leftLimit];
        for (int k = int(idx); k > int(leftLimit); --k) {
            if ((!isTrough && d_y[k - 1] <= halfLevel) || (isTrough && d_y[k - 1] >= halfLevel)) {
                double dy = d_y[k] - d_y[k - 1];
                double frac = (std::abs(dy) > 1e-15) ? ((halfLevel - d_y[k - 1]) / dy) : 0.5;
                frac = std::max(0.0, std::min(1.0, frac));
                xLeft = d_x[k - 1] + frac * (d_x[k] - d_x[k - 1]);
                foundLeft = true;
                break;
            }
        }

        bool foundRight = false;
        double xRight = d_x[rightLimit];
        for (size_t k = idx; k < rightLimit; ++k) {
            if ((!isTrough && d_y[k + 1] <= halfLevel) || (isTrough && d_y[k + 1] >= halfLevel)) {
                double dy = d_y[k] - d_y[k + 1];
                double frac = (std::abs(dy) > 1e-15) ? ((d_y[k] - halfLevel) / dy) : 0.5;
                frac = std::max(0.0, std::min(1.0, frac));
                xRight = d_x[k] + frac * (d_x[k + 1] - d_x[k]);
                foundRight = true;
                break;
            }
        }

        double fwhm = 0.0;
        if (foundLeft && foundRight)
            fwhm = xRight - xLeft;
        else if (foundLeft)
            fwhm = 2.0 * (xc - xLeft);
        else if (foundRight)
            fwhm = 2.0 * (xRight - xc);
        else
            fwhm = std::max(minW, std::min(std::abs(xc - xLeft), std::abs(xRight - xc)) * 1.2);

        if (chosen[m].hasExactXc && targetPeaks > 1) {
            double neighborDist = xSpan;
            if (m > 0) {
                double xPrev = chosen[m - 1].hasExactXc ? chosen[m - 1].exactXc : d_x[chosen[m - 1].idx];
                neighborDist = std::min(neighborDist, std::abs(xc - xPrev));
            }
            if (m + 1 < targetPeaks) {
                double xNext = chosen[m + 1].hasExactXc ? chosen[m + 1].exactXc : d_x[chosen[m + 1].idx];
                neighborDist = std::min(neighborDist, std::abs(xNext - xc));
            }
            if ((!foundLeft || !foundRight) && neighborDist > minW)
                fwhm = std::min(fwhm, 1.25 * neighborDist);
            H = std::min(H, 0.75 * span);
        }

        fwhm = std::max(minW, std::min(maxW, fwhm));

        double A_init = 1.0;
        double w_init = fwhm;
        initSinglePeakParams(peakProfileAt(m), H, fwhm, xc, A_init, w_init);
        if (isTrough)
            A_init = -A_init;

        gsl_vector_set(d_param_init, 3 * m, A_init);
        gsl_vector_set(d_param_init, 3 * m + 1, xc);
        gsl_vector_set(d_param_init, 3 * m + 2, w_init);
    }

    gsl_vector_set(d_param_init, 3 * targetPeaks, y0);

    // 6. When doublet constraints exist, ensure primary/secondary assignment minimizes initial SSR
    if (!d_doublet_constraints.isEmpty()) {
        std::vector<double> baseSeed(d_p);
        for (unsigned j = 0; j < d_p; ++j)
            baseSeed[j] = gsl_vector_get(d_param_init, j);

        auto evalSeedSsr = [&](std::vector<double> cand) -> double {
            applyDoubletConstraints(cand);
            double ssr = 0.0;
            double off = cand[d_p - 1];
            for (size_t i = 0; i < n; ++i) {
                double yi = off;
                for (int k = 0; k < targetPeaks; ++k)
                    yi += evalPeakAt(cand, k, d_x[i], false);
                double r = d_y[i] - yi;
                ssr += r * r;
            }
            return ssr;
        };

        std::vector<double> bestSeed = baseSeed;
        applyDoubletConstraints(bestSeed);
        double bestSsr = evalSeedSsr(bestSeed);

        auto maxIt = std::max_element(d_y.begin(), d_y.end());
        size_t globalMaxIdx = size_t(std::distance(d_y.begin(), maxIt));
        double xGlobalMax = d_x[globalMaxIdx];
        double hGlobalMax = std::max(1e-6 * span, *maxIt - y0);

        for (const auto &dc : d_doublet_constraints) {
            if (dc.priIdx < 0 || dc.priIdx >= targetPeaks || dc.secIdx < 0
                || dc.secIdx >= targetPeaks)
                continue;
            double fwhmEst = peakFWHMFromW(peakProfileAt(dc.priIdx), baseSeed[3 * dc.priIdx + 2]);
            fwhmEst = std::max(minW, std::min(maxW * 0.4, fwhmEst));

            std::vector<double> xcCandidates = { xGlobalMax, xGlobalMax - dc.centerOffset };
            if (dc.secIdx < targetPeaks)
                xcCandidates.push_back(baseSeed[3 * dc.secIdx + 1]);

            for (double xcTry : xcCandidates) {
                std::vector<double> trial = baseSeed;
                double xcWork = xcTry;
                double aTry = 1.0, wTry = fwhmEst;
                double hScale = (std::abs(xcTry - xGlobalMax) < 1e-9)
                        ? hGlobalMax
                        : (hGlobalMax / std::max(0.2, dc.areaRatio));
                initSinglePeakParams(peakProfileAt(dc.priIdx), hScale, fwhmEst, xcWork, aTry, wTry);
                trial[3 * dc.priIdx] = aTry;
                trial[3 * dc.priIdx + 1] = xcWork;
                trial[3 * dc.priIdx + 2] = wTry;
                applyDoubletConstraints(trial);
                double ssr = evalSeedSsr(trial);
                if (ssr < bestSsr) {
                    bestSsr = ssr;
                    bestSeed = trial;
                }
            }
        }

        for (unsigned j = 0; j < d_p; ++j)
            gsl_vector_set(d_param_init, j, bestSeed[j]);
    }

    d_initial_guesses_set = true;
    applyCustomInitialWidths();
    return true;
}

void MultiPeakFit::guessInitialValues()
{
    if (d_has_seed_request) {
        const std::vector<double> centers = d_seed_centers;
        seedPeaksFromCenters(centers, d_seed_prominence);
        return;
    }
    if (!d_detected_2nd_deriv_centers.empty()
        && int(d_detected_2nd_deriv_centers.size()) == d_peaks) {
        seedPeaksFromCenters(d_detected_2nd_deriv_centers);
        return;
    }
    autoDetectPeaks(d_peaks);
}

void MultiPeakFit::refineSolverResult(std::vector<double> &par, int &iterations, int &status)
{
    // A surplus peak may turn into a hump centred outside the X range of the data that only
    // stands in for a sloping background: so broad that it spans a good part of the range, or
    // so far out that it no longer reaches the data. Such a solution is not adopted: the fit is
    // repeated from the same initial values with those centres confined to the data range.
    // A centre just outside is kept: a peak cut at the end of the measurement, the tail of a
    // neighbour inside a fit range, or a centre the user gave (analyze_peaks centers, picked in
    // the GUI) -- pinning those to the end of the range biases their width and area.
    if (d_in_bootstrap || d_peaks <= 0 || !d_param_init || par.size() < d_p
        || d_p < unsigned(3 * d_peaks + 1) || d_x.empty())
        return;
    if (!d_doublet_constraints.isEmpty() || !d_linear_constraint_expr.trimmed().isEmpty())
        return;

    double xLo = std::numeric_limits<double>::infinity();
    double xHi = -std::numeric_limits<double>::infinity();
    for (double xv : d_x) {
        if (std::isfinite(xv)) {
            xLo = std::min(xLo, xv);
            xHi = std::max(xHi, xv);
        }
    }
    if (!(xHi > xLo))
        return;

    const double range = xHi - xLo;

    // A centre the user gave: an explicit initial value (a peak picked on the plot), or a seed
    // centre (analyze_peaks centers, the peak dialog's centres) the fit started from exactly --
    // not one found by the second-derivative search, which is a detection like the automatic one.
    auto sameX = [range](double a, double b) {
        return std::isfinite(a) && std::isfinite(b)
                && std::abs(a - b) <= 1e-9 * std::max(range, std::abs(b));
    };
    auto userGivenCentre = [&](int idx) {
        if (d_explicit_init_vals.contains(idx))
            return true;
        if (!d_has_seed_request || d_seed_centers.empty())
            return false;
        const double init = initialGuess(idx);
        for (double c : d_seed_centers) {
            if (!sameX(init, c))
                continue;
            bool detected = false;
            for (double dc : d_detected_2nd_deriv_centers)
                detected = detected || sameX(c, dc);
            if (!detected)
                return true;
        }
        return false;
    };

    std::vector<int> escaped;
    for (int m = 0; m < d_peaks; ++m) {
        const int idx = 3 * m + 1;
        if (unsigned(idx + 1) >= d_p || isParameterFixed(idx) || hasParameterBounds(idx))
            continue;
        const double xc = par[size_t(idx)];
        if (std::isfinite(xc) && xc >= xLo && xc <= xHi)
            continue;
        if (userGivenCentre(idx))
            continue;
        const double wPar = par[size_t(idx + 1)];
        const double fwhm =
                std::isfinite(wPar) ? peakFWHMFromW(peakProfileAt(m), wPar)
                                    : std::numeric_limits<double>::quiet_NaN();
        const double outside = std::isfinite(xc) ? std::max(xLo - xc, xc - xHi)
                                                 : std::numeric_limits<double>::infinity();
        const bool broad = !(fwhm < 0.5 * range);   // spans half the range or more (or NaN)
        const bool far = !(outside < 5.0 * fwhm);   // 5 FWHM beyond the last point
        if (broad || far)
            escaped.push_back(idx);
    }
    if (escaped.empty())
        return;

    for (int idx : escaped)
        setParameterBounds(idx, xLo, xHi);
    int iter2 = 0, status2 = GSL_EINVAL;
    std::vector<double> par2 = (d_solver == NelderMeadSimplex) ? fitGslMultimin(iter2, status2)
                                                               : fitGslMultifit(iter2, status2);
    for (int idx : escaped)
        clearParameterBounds(idx);

    if (status2 == GSL_SUCCESS && std::isfinite(chi_2) && chi_2 >= 0.0) {
        par = par2;
        iterations += iter2;
        status = status2;
        d_fit_notes << tr("Peak centres that left the X range of the data were confined to it.");
        return;
    }
    // The confined fit failed: restore the solver state of the original solution.
    int iter3 = 0, status3 = GSL_EINVAL;
    std::vector<double> par3 = (d_solver == NelderMeadSimplex) ? fitGslMultimin(iter3, status3)
                                                               : fitGslMultifit(iter3, status3);
    par = par3;
    iterations += iter2 + iter3;
    status = status3;
}

QVector<double> MultiPeakFit::detectPeaksSecondDerivative(int sgWindow, double relThreshold)
{
    QVector<double> detected;
    const size_t n = d_x.size();
    if (n < 5 || d_y.size() != n)
        return detected;

    int win = std::max(5, sgWindow);
    if (win % 2 == 0)
        ++win;
    int maxOdd = (int(n) % 2 == 1) ? int(n) : (int(n) - 1);
    win = std::min(win, maxOdd);
    if (win < 5)
        return detected;
    const int hw = win / 2;
    const int deg = (win >= 7) ? 4 : 2;
    const int mCols = deg + 1;

    // Fit local Savitzky-Golay polynomial in scaled physical coordinate u = (x_k - x_i) / hScale
    std::vector<double> negD2(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        int left = std::max(0, std::min(int(n) - win, int(i) - hw));
        int right = left + win - 1;
        double hScale =
                std::max(1e-15, std::max(std::abs(d_x[size_t(right)] - d_x[i]),
                                         std::abs(d_x[i] - d_x[size_t(left)])));

        double ata[5][5] = {};
        double atb[5] = {};
        for (int k = left; k <= right; ++k) {
            double u = (d_x[size_t(k)] - d_x[i]) / hScale;
            double upow[9];
            upow[0] = 1.0;
            for (int p = 1; p <= 2 * deg; ++p)
                upow[p] = upow[p - 1] * u;
            double yk = d_y[size_t(k)];
            for (int r = 0; r < mCols; ++r) {
                atb[r] += upow[r] * yk;
                for (int c = 0; c < mCols; ++c)
                    ata[r][c] += upow[r + c];
            }
        }

        // Solve small mCols x mCols normal system via Gaussian elimination with partial pivoting
        double aug[5][6] = {};
        for (int r = 0; r < mCols; ++r) {
            for (int c = 0; c < mCols; ++c)
                aug[r][c] = ata[r][c];
            aug[r][mCols] = atb[r];
        }
        bool ok = true;
        for (int col = 0; col < mCols; ++col) {
            int piv = col;
            for (int r = col + 1; r < mCols; ++r) {
                if (std::abs(aug[r][col]) > std::abs(aug[piv][col]))
                    piv = r;
            }
            if (std::abs(aug[piv][col]) < 1e-15) {
                ok = false;
                break;
            }
            if (piv != col) {
                for (int c = col; c <= mCols; ++c)
                    std::swap(aug[col][c], aug[piv][c]);
            }
            double invDiag = 1.0 / aug[col][col];
            for (int c = col; c <= mCols; ++c)
                aug[col][c] *= invDiag;
            for (int r = 0; r < mCols; ++r) {
                if (r == col)
                    continue;
                double factor = aug[r][col];
                for (int c = col; c <= mCols; ++c)
                    aug[r][c] -= factor * aug[col][c];
            }
        }
        if (ok) {
            double c2 = aug[2][mCols];
            double d2y = (2.0 * c2) / (hScale * hScale);
            negD2[i] = d_negative_peaks_allowed ? std::abs(d2y) : (-d2y);
        }
    }

    // Mild 3-point binomial smoothing on -y''(x) to suppress high-frequency discretization noise
    std::vector<double> sSmooth = negD2;
    if (n >= 7) {
        for (size_t i = 1; i + 1 < n; ++i)
            sSmooth[i] = 0.25 * negD2[i - 1] + 0.5 * negD2[i] + 0.25 * negD2[i + 1];
    }

    double maxS = *std::max_element(sSmooth.begin(), sSmooth.end());
    if (maxS <= 0.0 || !std::isfinite(maxS))
        return detected;

    double yMin = *std::min_element(d_y.begin(), d_y.end());
    double yMax = *std::max_element(d_y.begin(), d_y.end());
    double ySpan = std::max(0.0, yMax - yMin);
    double y0Est = estimateConstantBaselineY0(d_y);
    double thresh = std::max(0.0, relThreshold) * maxS;

    struct D2Cand
    {
        size_t idx;
        double score;
        double xc;
    };
    std::vector<D2Cand> rawCands;
    for (size_t i = 1; i + 1 < n; ++i) {
        if (sSmooth[i] >= thresh && sSmooth[i] >= sSmooth[i - 1] && sSmooth[i] >= sSmooth[i + 1]
            && (sSmooth[i] > sSmooth[i - 1] || sSmooth[i] > sSmooth[i + 1])) {
            if (!d_negative_peaks_allowed && ySpan > 0.0 && (d_y[i] - yMin) < 0.01 * ySpan)
                continue;
            if (d_negative_peaks_allowed && ySpan > 0.0 && std::abs(d_y[i] - y0Est) < 0.01 * ySpan)
                continue;
            double xc = d_x[i];
            double denom = 2.0 * (2.0 * sSmooth[i] - sSmooth[i - 1] - sSmooth[i + 1]);
            if (std::abs(denom) > 1e-15) {
                double delta =
                        std::max(-0.5, std::min(0.5, (sSmooth[i - 1] - sSmooth[i + 1]) / denom));
                xc = d_x[i] + delta * 0.5 * (d_x[i + 1] - d_x[i - 1]);
            }
            rawCands.push_back({ i, sSmooth[i], xc });
        }
    }

    std::sort(rawCands.begin(), rawCands.end(),
              [](const D2Cand &a, const D2Cand &b) { return a.score > b.score; });

    const int minSepPts = std::max(2, hw / 2);
    std::vector<D2Cand> filtered;
    for (const auto &c : rawCands) {
        bool distinct = true;
        for (const auto &f : filtered) {
            if (std::abs(int(c.idx) - int(f.idx)) < minSepPts) {
                distinct = false;
                break;
            }
        }
        if (distinct)
            filtered.push_back(c);
    }

    std::sort(filtered.begin(), filtered.end(),
              [](const D2Cand &a, const D2Cand &b) { return a.xc < b.xc; });

    d_detected_2nd_deriv_centers.clear();
    for (const auto &c : filtered) {
        detected.push_back(c.xc);
        d_detected_2nd_deriv_centers.push_back(c.xc);
    }

    if (!d_detected_2nd_deriv_centers.empty()) {
        if (d_per_peak_profiles.isEmpty() && d_doublet_constraints.isEmpty()
            && d_peaks != int(d_detected_2nd_deriv_centers.size())) {
            setNumPeaks(int(d_detected_2nd_deriv_centers.size()));
        }
        seedPeaksFromCenters(d_detected_2nd_deriv_centers,
                             relThreshold > 0.0 ? relThreshold : 0.08);
    }

    return detected;
}

void MultiPeakFit::setInitialPeakWidths(const QVector<double> &fwhms)
{
    d_initial_peak_widths = fwhms;
    if (!d_initial_guesses_set && !d_x.empty() && !d_all_guesses_set
        && d_explicit_init_vals.isEmpty()) {
        guessInitialValues();
        return; // guessInitialValues() calls applyCustomInitialWidths() via seedPeaksFromCenters()
    }
    applyCustomInitialWidths();
}

void MultiPeakFit::applyCustomInitialWidths()
{
    if (d_initial_peak_widths.isEmpty() || !d_param_init || d_p < unsigned(3 * d_peaks + 1))
        return;

    bool modified = false;
    const int nApply = std::min(d_peaks, d_initial_peak_widths.size());
    for (int m = 0; m < nApply; ++m) {
        const double fwhmCustom = d_initial_peak_widths[m];
        if (!std::isfinite(fwhmCustom) || fwhmCustom <= 0.0)
            continue;

        const PeakProfile prof = peakProfileAt(m);
        const double curA_int = gsl_vector_get(d_param_init, 3 * m);
        const double curW = std::max(1e-15, std::abs(gsl_vector_get(d_param_init, 3 * m + 2)));
        const double curA_user = fromInternalParameter(3 * m, curA_int);
        double fwhmCur = 0.0, hCur = 0.0, areaCur = 0.0;
        evalSinglePeakMetrics(prof, curA_user, curW, fwhmCur, hCur, areaCur);

        const double newW = wFromPeakFWHM(prof, fwhmCustom);
        double newA_user = curA_user;
        if (std::abs(hCur) > 1e-15) {
            double fwhmUnit = 0.0, hUnit = 0.0, areaUnit = 0.0;
            evalSinglePeakMetrics(prof, 1.0, newW, fwhmUnit, hUnit, areaUnit);
            if (hUnit > 1e-15)
                newA_user = hCur / hUnit;
        }
        gsl_vector_set(d_param_init, 3 * m, toInternalParameter(3 * m, newA_user));
        gsl_vector_set(d_param_init, 3 * m + 2, newW);
        modified = true;
    }

    if (modified && !d_doublet_constraints.isEmpty()) {
        std::vector<double> cur(d_p);
        for (unsigned j = 0; j < d_p; ++j)
            cur[j] = gsl_vector_get(d_param_init, j);
        applyDoubletConstraints(cur);
        for (unsigned j = 0; j < d_p; ++j)
            gsl_vector_set(d_param_init, j, cur[j]);
    }
}

void MultiPeakFit::setNegativePeaksAllowed(bool allow)
{
    if (d_negative_peaks_allowed == allow)
        return;

    const bool oldCustom = usesCustomMultiPeakCallbacks();
    d_negative_peaks_allowed = allow;
    const bool newCustom = usesCustomMultiPeakCallbacks();

    // Convert Lorentz solver amplitude scale if callback convention changed
    if (d_param_init && d_p >= unsigned(3 * d_peaks + 1) && d_profile == Lorentz
        && oldCustom != newCustom) {
        const double factor = newCustom ? M_PI_2 : M_2_PI;
        for (int j = 0; j < d_peaks; ++j)
            gsl_vector_set(d_param_init, 3 * j, factor * gsl_vector_get(d_param_init, 3 * j));
    }

    updateProfileFunctions();

    if (!d_x.empty() && !d_all_guesses_set && d_explicit_init_vals.isEmpty()) {
        d_initial_guesses_set = false;
        guessInitialValues();
    }
}

void MultiPeakFit::setPerPeakProfiles(const QVector<PeakProfile> &profiles)
{
    const bool oldCustom = usesCustomMultiPeakCallbacks();
    if (profiles.isEmpty()) {
        d_per_peak_profiles.clear();
        const bool newCustom = usesCustomMultiPeakCallbacks();
        if (d_param_init && d_p >= unsigned(3 * d_peaks + 1) && d_profile == Lorentz
            && oldCustom != newCustom) {
            const double factor = newCustom ? M_PI_2 : M_2_PI;
            for (int j = 0; j < d_peaks; ++j)
                gsl_vector_set(d_param_init, 3 * j, factor * gsl_vector_get(d_param_init, 3 * j));
        }
        updateProfileFunctions();
        d_formula = buildFormula();
        return;
    }

    const bool resized = (profiles.size() != d_peaks);
    d_per_peak_profiles.clear();
    if (resized)
        setNumPeaks(profiles.size());
    d_per_peak_profiles = profiles;
    const bool newCustom = usesCustomMultiPeakCallbacks();
    if (!resized && d_param_init && d_p >= unsigned(3 * d_peaks + 1) && d_profile == Lorentz
        && oldCustom != newCustom) {
        const double factor = newCustom ? M_PI_2 : M_2_PI;
        for (int j = 0; j < d_peaks; ++j)
            gsl_vector_set(d_param_init, 3 * j, factor * gsl_vector_get(d_param_init, 3 * j));
    }
    d_explanation = tr("Mixed-Profile(%1) multi-peak").arg(d_peaks);
    d_param_explain = generateExplanationList(d_peaks, d_profile, d_per_peak_profiles);
    d_formula = buildFormula();
    updateProfileFunctions();

    if (!d_x.empty() && !d_all_guesses_set && d_explicit_init_vals.isEmpty()) {
        d_initial_guesses_set = false;
        guessInitialValues();
    }
}

void MultiPeakFit::addPeakDoubletConstraint(int secondaryPeakIdx, int primaryPeakIdx,
                                            double areaRatio, double centerOffset, bool shareWidth)
{
    if (secondaryPeakIdx < 0 || primaryPeakIdx < 0 || secondaryPeakIdx == primaryPeakIdx)
        return;

    const bool oldCustom = usesCustomMultiPeakCallbacks();

    int reqPeaks = std::max(secondaryPeakIdx, primaryPeakIdx) + 1;
    const bool resized = (reqPeaks > d_peaks);
    if (resized)
        setNumPeaks(reqPeaks);

    DoubletConstraint dc;
    dc.secIdx = secondaryPeakIdx;
    dc.priIdx = primaryPeakIdx;
    dc.areaRatio = std::max(0.0, areaRatio);
    dc.centerOffset = centerOffset;
    dc.shareWidth = shareWidth;

    bool replaced = false;
    for (int i = 0; i < d_doublet_constraints.size(); ++i) {
        if (d_doublet_constraints[i].secIdx == secondaryPeakIdx) {
            d_doublet_constraints[i] = dc;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        d_doublet_constraints.push_back(dc);

    const bool newCustom = usesCustomMultiPeakCallbacks();

    // Convert Lorentz solver amplitude scale if callback convention changed (E4-7)
    if (!resized && d_param_init && d_p >= unsigned(3 * d_peaks + 1) && d_profile == Lorentz
        && oldCustom != newCustom) {
        const double factor = newCustom ? M_PI_2 : M_2_PI;
        for (int j = 0; j < d_peaks; ++j)
            gsl_vector_set(d_param_init, 3 * j, factor * gsl_vector_get(d_param_init, 3 * j));
    }

    setParameterFixed(3 * secondaryPeakIdx, true);
    setParameterFixed(3 * secondaryPeakIdx + 1, true);
    setParameterFixed(3 * secondaryPeakIdx + 2, shareWidth);

    updateProfileFunctions();

    if (!d_x.empty() && !d_all_guesses_set && d_explicit_init_vals.isEmpty()) {
        d_initial_guesses_set = false;
        guessInitialValues();
    } else if (d_param_init && d_p >= unsigned(3 * d_peaks + 1)) {
        std::vector<double> cur(d_p);
        for (unsigned j = 0; j < d_p; ++j)
            cur[j] = gsl_vector_get(d_param_init, j);
        applyDoubletConstraints(cur);
        for (unsigned j = 0; j < d_p; ++j)
            gsl_vector_set(d_param_init, j, cur[j]);
    }
}

double MultiPeakFit::covarSolverToStoredScale(int parIndex, double solverVal) const
{
    if (!d_covar_scaled || parIndex < 0 || parIndex >= 3 * d_peaks)
        return 1.0;
    const int rem = parIndex % 3;
    if (rem == 0) {
        double sA = (!usesCustomMultiPeakCallbacks() && d_profile == Lorentz) ? M_PI_2 : 1.0;
        if (!d_negative_peaks_allowed && solverVal < 0.0)
            sA = -sA;
        return sA;
    }
    if (rem == 2)
        return (solverVal < 0.0) ? -1.0 : 1.0;
    return 1.0;
}

void MultiPeakFit::storeCustomFitResults(const vector<double> &par)
{
    d_results = par;
    if (!d_doublet_constraints.isEmpty())
        applyDoubletConstraints(d_results);

    bool parAllFinite = (d_p > 0 && d_results.size() >= d_p);
    for (unsigned i = 0; parAllFinite && i < d_p; ++i) {
        if (!std::isfinite(d_results[i]))
            parAllFinite = false;
    }

    if (!d_covar_scaled && parAllFinite && covar && covar->size1 >= d_p && covar->size2 >= d_p) {
        d_covar_scaled = true;
        std::vector<double> scale(d_p, 1.0);
        bool anyScaled = false;
        for (unsigned i = 0; i < d_p; ++i) {
            scale[i] = covarSolverToStoredScale(int(i), d_results[i]);
            if (scale[i] != 1.0)
                anyScaled = true;
        }
        if (anyScaled) {
            for (unsigned a = 0; a < d_p; ++a) {
                for (unsigned b = 0; b < d_p; ++b) {
                    gsl_matrix_set(covar, a, b,
                                   gsl_matrix_get(covar, a, b) * scale[a] * scale[b]);
                }
            }
        }
    }

    for (int j = 0; j < d_peaks && unsigned(3 * j + 2) < d_results.size(); j++) {
        d_results[3 * j + 2] = std::abs(d_results[3 * j + 2]);
        if (!usesCustomMultiPeakCallbacks() && d_profile == Lorentz)
            d_results[3 * j] = M_PI_2 * d_results[3 * j];
        if (!d_negative_peaks_allowed)
            d_results[3 * j] = std::abs(d_results[3 * j]);
    }

    d_last_results = d_results;
}

double MultiPeakFit::evalPeakAt(const std::vector<double> &par, int peakIndex, double x,
                                bool isStoredResults) const
{
    PeakProfile prof = peakProfileAt(peakIndex);
    double A = par[3 * peakIndex];
    double xc = par[3 * peakIndex + 1];
    double w = std::abs(par[3 * peakIndex + 2]);
    if (w < 1e-15)
        w = 1e-15;
    double diff = x - xc;
    const double signA = (d_negative_peaks_allowed && A < 0.0) ? -1.0 : 1.0;
    const double absA = std::abs(A);

    switch (prof) {
    case Gauss:
        return signA * std::sqrt(M_2_PI) * absA / w * std::exp(-2.0 * diff * diff / (w * w));
    case Lorentz: {
        double a_raw = (isStoredResults || usesCustomMultiPeakCallbacks())
                ? (M_2_PI * absA)
                : absA;
        return signA * a_raw * w / (4.0 * diff * diff + w * w);
    }
    case GaussAmp:
        return (d_negative_peaks_allowed ? A : absA) * std::exp(-0.5 * diff * diff / (w * w));
    case PseudoVoigt: {
        const double kG = 4.0 * M_LN2;
        const double cG = std::sqrt(kG / M_PI);
        const double cL = 2.0 / M_PI;
        double L = cL * w / (4.0 * diff * diff + w * w);
        double G = (cG / w) * std::exp(-kG * diff * diff / (w * w));
        return signA * absA * (d_voigt_mu * L + (1.0 - d_voigt_mu) * G);
    }
    case PearsonVII: {
        double m = std::max(0.55, d_pearson_m);
        double cm = std::pow(2.0, 1.0 / m) - 1.0;
        double u = 2.0 * diff / w;
        return (d_negative_peaks_allowed ? A : absA) * std::pow(1.0 + cm * u * u, -m);
    }
    case Voigt:
    case BiGaussian:
    case EMG:
    case LogNormal:
    case Asym2Sig:
    case DoniachSunjic: {
        double alphaEff =
                (prof == DoniachSunjic) ? effectiveDoniachAlpha(d_asym_alpha) : d_asym_alpha;
        return signA * eval_origin_peak_profile(int(prof), absA, xc, w, x,
                                                d_voigt_mu, d_pearson_m, alphaEff, d_tail_tau);
    }
    }
    return 0.0;
}

bool MultiPeakFit::calculateFitCurveData(const std::vector<double> &par, std::vector<double> &X,
                                         std::vector<double> &Y)
{
    if (par.size() < size_t(3 * d_peaks + 1))
        return false;
    std::vector<double> effPar = par;
    if (!d_doublet_constraints.isEmpty())
        applyDoubletConstraints(effPar);
    generateX(X);
    double y0 = effPar[d_p - 1];
    for (int i = 0; i < d_points; ++i) {
        double yi = y0;
        for (int j = 0; j < d_peaks; ++j)
            yi += evalPeakAt(effPar, j, X[i], true);
        Y[i] = yi;
    }
    return true;
}

bool MultiPeakFit::evaluateModelCurve(const std::vector<double> &solverPar, std::vector<double> &X,
                                      std::vector<double> &Y)
{
    if (solverPar.size() < size_t(3 * d_peaks + 1))
        return false;
    std::vector<double> effPar = solverPar;
    if (!d_doublet_constraints.isEmpty())
        applyDoubletConstraints(effPar);
    generateX(X);
    double y0 = effPar[d_p - 1];
    for (int i = 0; i < d_points; ++i) {
        double yi = y0;
        for (int j = 0; j < d_peaks; ++j)
            yi += evalPeakAt(effPar, j, X[i], false);
        Y[i] = yi;
    }
    return true;
}

double MultiPeakFit::toInternalParameter(int index, double userVal) const
{
    if (!usesCustomMultiPeakCallbacks() && d_profile == Lorentz && index >= 0
        && index < 3 * d_peaks && (index % 3 == 0)) {
        return M_2_PI * userVal;
    }
    return userVal;
}

double MultiPeakFit::fromInternalParameter(int index, double internalVal) const
{
    if (!usesCustomMultiPeakCallbacks() && d_profile == Lorentz && index >= 0
        && index < 3 * d_peaks && (index % 3 == 0)) {
        return M_PI_2 * internalVal;
    }
    return internalVal;
}

std::vector<double>
MultiPeakFit::evaluateSubtractedBaselineOnGrid(const std::vector<double> &X) const
{
    std::vector<double> out(X.size(), 0.0);
    if (!d_baseline_subtracted || d_x.empty() || d_last_baseline.size() != d_x.size()
        || X.empty()) {
        return out;
    }

    const size_t n = d_x.size();
    if (n == 1) {
        std::fill(out.begin(), out.end(), d_last_baseline[0]);
        return out;
    }

    // Fast exact match when output grid equals d_x
    if (X.size() == n) {
        bool identical = true;
        for (size_t i = 0; i < n; ++i) {
            if (X[i] != d_x[i]) {
                identical = false;
                break;
            }
        }
        if (identical)
            return d_last_baseline;
    }

    std::vector<std::pair<double, double>> knots(n);
    for (size_t i = 0; i < n; ++i)
        knots[i] = { d_x[i], d_last_baseline[i] };
    std::stable_sort(knots.begin(), knots.end(),
                     [](const std::pair<double, double> &a, const std::pair<double, double> &b) {
                         return a.first < b.first;
                     });

    for (size_t k = 0; k < X.size(); ++k) {
        const double xq = X[k];
        if (xq <= knots.front().first) {
            out[k] = knots.front().second;
            continue;
        }
        if (xq >= knots.back().first) {
            out[k] = knots.back().second;
            continue;
        }
        auto it = std::lower_bound(
                knots.begin(), knots.end(), xq,
                [](const std::pair<double, double> &p, double v) { return p.first < v; });
        if (it == knots.begin()) {
            out[k] = knots.front().second;
        } else if (it != knots.end() && it->first == xq) {
            out[k] = it->second;
        } else {
            const size_t hi = static_cast<size_t>(it - knots.begin());
            const size_t lo = hi - 1;
            const double dx = knots[hi].first - knots[lo].first;
            const double t = (std::abs(dx) > 1e-300) ? ((xq - knots[lo].first) / dx) : 0.5;
            out[k] = knots[lo].second + t * (knots[hi].second - knots[lo].second);
        }
    }
    return out;
}

QColor MultiPeakFit::resolvedPeakComponentColor(int peakIndex) const
{
    if (d_custom_peaks_color || d_peaks <= 1)
        return d_peaks_color;
    static const QColor kPalette[] = {
        QColor("#009E73"), // bluish green
        QColor("#E69F00"), // amber
        QColor("#56B4E9"), // sky blue
        QColor("#CC79A7"), // reddish purple
        QColor("#0072B2"), // ocean blue
        QColor("#6A3D9A")  // deep violet
    };
    const int nPal = int(sizeof(kPalette) / sizeof(kPalette[0]));
    return kPalette[std::max(0, peakIndex) % nPal];
}

void MultiPeakFit::insertPeakFunctionCurve(std::vector<double> &x, std::vector<double> &y, int peak)
{
    QStringList curves = d_graph->curvesList();
    int index = 0;
    QString prefix = tr("Peak");
    for (int i = 0; i < (int)curves.count(); i++) {
        if (curves[i].startsWith(prefix)) {
            bool ok = false;
            curves[i].mid(prefix.length()).toInt(&ok);
            if (ok)
                index++;
        }
    }
    QString title = prefix + QString::number(++index);

    FunctionCurve *c =
            new FunctionCurve((ApplicationWindow *)parent(), FunctionCurve::Normal, title);
    c->setPen(QPen(resolvedPeakComponentColor(peak), 2, Qt::DashLine));
    c->setData(x.data(), y.data(), d_points);
    c->setRange(d_x.front(), d_x.back());

    const bool hasSubBase = (d_baseline_subtracted && d_last_baseline.size() == d_x.size()
                             && !d_last_baseline.empty());
    if (hasSubBase && d_baseline_mode != BaselineConstant && d_baseline_mode != BaselineZero) {
        // Non-constant subtracted baseline (Linear / ALS / Spline Anchors) is baked into (x, y)
        c->setFormula(QString());
    } else {
        const double effY0 = d_results[d_p - 1]
                + ((hasSubBase && d_baseline_mode == BaselineConstant) ? d_last_baseline.front()
                                                                       : 0.0);
        QString formula;
        for (int j = 0; j < 3; j++) {
            int p = 3 * peak + j;
            formula += QString("%1=%2\n").arg(d_param_names[p]).arg(d_results[p], 0, 'g', d_prec);
        }
        formula += QString("%1=%2\n\ny0+%3")
                           .arg(d_param_names[d_p - 1])
                           .arg(effY0, 1, 'g', d_prec)
                           .arg(instancePeakFormula(peak + 1, peakProfileAt(peak)));
        c->setFormula(formula);
    }
    attachToSourceAxes(c);
    d_graph->insertPlotItem(c, Graph::Line);
    d_graph->addFitCurve(c);
}

void MultiPeakFit::generateFitCurve(const vector<double> &par)
{
    if (d_suppress_curve_output || !d_graph)
        return;

    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (!d_gen_function)
        d_points = int(d_x.size());

    if (d_points <= 0 || d_x.empty())
        return;

    gsl_matrix *m = gsl_matrix_alloc(d_points, d_peaks);
    if (!m) {
        if (!isNonInteractiveFit(app, d_silent))
            QMessageBox::warning(app, tr("Fit Error"),
                                 tr("Could not allocate enough memory for the fit curves!"));
        return;
    }

    std::vector<double> effPar = par;
    if (!d_doublet_constraints.isEmpty())
        applyDoubletConstraints(effPar);

    std::vector<double> X(d_points);
    std::vector<double> Y(d_points);
    int i, j;
    int peaks_aux = generate_peak_curves ? d_peaks : 0;

    const bool nonAnalyticSubBaseline =
            (d_baseline_subtracted && d_last_baseline.size() == d_x.size()
             && !d_last_baseline.empty() && d_baseline_mode != BaselineZero
             && d_baseline_mode != BaselineConstant);

    if (d_gen_function && !nonAnalyticSubBaseline) {
        double step = (d_points > 1) ? ((d_x.back() - d_x.front()) / (d_points - 1)) : 0.0;
        for (i = 0; i < d_points; i++)
            X[i] = d_x[0] + i * step;

        const std::vector<double> baseGrid = evaluateSubtractedBaselineOnGrid(X);

        for (i = 0; i < d_points; i++) {
            const double baseVal = (size_t(i) < baseGrid.size()) ? baseGrid[size_t(i)] : 0.0;
            double yi = 0;
            for (j = 0; j < d_peaks; j++) {
                double y_aux = evalPeakAt(effPar, j, X[i], false);
                yi += y_aux;
                y_aux += effPar[d_p - 1] + baseVal;
                gsl_matrix_set(m, i, j, y_aux);
            }
            Y[i] = yi + effPar[d_p - 1] + baseVal;
        }

        if (d_peaks > 1)
            insertFitFunctionCurve(objectName() + tr("Fit"), X, Y, 2);
        else
            insertFitFunctionCurve(objectName() + tr("Fit"), X, Y);

        if (d_baseline_subtracted && d_last_baseline.size() == d_x.size()
            && !d_last_baseline.empty() && !d_graph->fitCurvesList().isEmpty()) {
            if (auto *sumFc = dynamic_cast<FunctionCurve *>(d_graph->fitCurvesList().last())) {
                if (d_baseline_mode == BaselineConstant) {
                    const double effY0 = d_results[d_p - 1] + d_last_baseline.front();
                    QString formula;
                    for (unsigned p = 0; p + 1 < d_p; ++p) {
                        formula += QString("%1=%2\n")
                                           .arg(d_param_names[p])
                                           .arg(d_results[p], 0, 'g', d_prec);
                    }
                    formula += QString("%1=%2\n\n%3")
                                       .arg(d_param_names[d_p - 1])
                                       .arg(effY0, 0, 'g', d_prec)
                                       .arg(d_formula);
                    sumFc->setFormula(formula);
                } else if (d_baseline_mode != BaselineZero) {
                    sumFc->setFormula(QString());
                }
            }
        }

        if (generate_peak_curves) {
            for (i = 0; i < peaks_aux; i++) { // add the peak curves
                for (j = 0; j < d_points; j++)
                    Y[j] = gsl_matrix_get(m, j, i);

                insertPeakFunctionCurve(X, Y, i);
            }
        }
    } else {
        QString tableName = app->generateUniqueName(QStringLiteral("Fit"));
        const QString srcTitle = (d_curve && d_graph->curveIndex(d_curve) >= 0)
                ? d_curve->title().text()
                : objectName();
        QString label = d_explanation + " " + tr("fit of") + " " + srcTitle;

        QList<Column *> columns;
        Column *colX = new Column(QStringLiteral("1"), SciDAVis::ColumnMode::Numeric);
        colX->setPlotDesignation(SciDAVis::X);
        columns << colX;
        for (i = 0; i < peaks_aux; i++) {
            Column *colPk = new Column(QStringLiteral("peak%1").arg(i + 1),
                                       SciDAVis::ColumnMode::Numeric);
            colPk->setPlotDesignation(SciDAVis::Y);
            colPk->setComment(tr("Peak %1").arg(i + 1));
            columns << colPk;
        }
        Column *colSum = new Column(QStringLiteral("2"), SciDAVis::ColumnMode::Numeric);
        colSum->setPlotDesignation(SciDAVis::Y);
        colSum->setComment((d_peaks > 1) ? tr("Multi-Peak Fit") : (objectName() + tr("Fit")));
        columns << colSum;
        Table *t = app->newHiddenTable(tableName, label, columns);
        if (t)
            tableName = t->name();

        if (d_gen_function) {
            const double step =
                    (d_points > 1) ? ((d_x.back() - d_x.front()) / (d_points - 1)) : 0.0;
            for (i = 0; i < d_points; i++)
                X[i] = d_x[0] + i * step;
        } else {
            for (i = 0; i < d_points; i++)
                X[i] = d_x[i];
        }
        const std::vector<double> baseGrid = evaluateSubtractedBaselineOnGrid(X);

        for (i = 0; i < d_points; i++) {
            columns.at(0)->setValueAt(i, X[i]);
            const double baseVal = (size_t(i) < baseGrid.size()) ? baseGrid[size_t(i)] : 0.0;

            double yi = 0;
            for (j = 0; j < d_peaks; j++) {
                double y_aux = evalPeakAt(effPar, j, X[i], false);
                yi += y_aux;
                y_aux += effPar[d_p - 1] + baseVal;
                if (j < peaks_aux)
                    columns.at(j + 1)->setValueAt(i, y_aux);
                gsl_matrix_set(m, i, j, y_aux);
            }
            Y[i] = yi + effPar[d_p - 1] + baseVal;
            columns.at(peaks_aux + 1)->setValueAt(i, Y[i]);
        }

        label = tableName + "_2";
        DataCurve *c = new DataCurve(t, tableName + "_" + columns.at(0)->name(), label);
        if (d_peaks > 1)
            c->setPen(QPen(d_curveColor, 2));
        else
            c->setPen(QPen(d_curveColor, 1));
        c->setData(X.data(), Y.data(), d_points);
        attachToSourceAxes(c);
        d_graph->insertPlotItem(c, Graph::Line);
        d_graph->addFitCurve(c);

        if (generate_peak_curves) {
            for (i = 0; i < peaks_aux; i++) { // add the peak curves
                for (j = 0; j < d_points; j++)
                    Y[j] = gsl_matrix_get(m, j, i);

                label = tableName + "_" + columns.at(i + 1)->name();
                c = new DataCurve(t, tableName + "_" + columns.at(0)->name(), label);
                c->setPen(QPen(resolvedPeakComponentColor(i), 2, Qt::DashLine));
                c->setData(X.data(), Y.data(), d_points);
                attachToSourceAxes(c);
                d_graph->insertPlotItem(c, Graph::Line);
                d_graph->addFitCurve(c);
            }
        }
    }
    d_graph->replot();

    gsl_matrix_free(m);
}

void MultiPeakFit::evalSinglePeakMetrics(PeakProfile prof, double signedRawA, double w,
                                         double &fwhmOut, double &heightOut, double &areaOut) const
{
    const double sign = (d_negative_peaks_allowed && signedRawA < 0.0) ? -1.0 : 1.0;
    const double rawA = std::abs(signedRawA);
    const double wSafe = std::max(1e-15, std::abs(w));
    const double alphaEff =
            (prof == DoniachSunjic) ? effectiveDoniachAlpha(d_asym_alpha) : d_asym_alpha;

    double fwhm = wSafe;
    double height = 0.0;
    double area = rawA;

    switch (prof) {
    case Gauss:
        area = rawA;
        fwhm = wSafe * std::sqrt(2.0 * M_LN2);
        height = std::sqrt(M_2_PI) * rawA / wSafe;
        break;
    case Lorentz:
        area = rawA;
        fwhm = wSafe;
        height = M_2_PI * rawA / wSafe;
        break;
    case GaussAmp:
        height = rawA;
        fwhm = wSafe * 2.0 * std::sqrt(2.0 * M_LN2);
        area = rawA * wSafe * std::sqrt(2.0 * M_PI);
        break;
    case PseudoVoigt: {
        area = rawA;
        fwhm = wSafe;
        double cG = std::sqrt(4.0 * M_LN2 / M_PI);
        height = (rawA / wSafe) * (d_voigt_mu * M_2_PI + (1.0 - d_voigt_mu) * cG);
        break;
    }
    case PearsonVII: {
        height = rawA;
        fwhm = wSafe;
        double m = std::max(0.55, d_pearson_m);
        double cm = std::pow(2.0, 1.0 / m) - 1.0;
        area = rawA * wSafe * (std::sqrt(M_PI) * std::tgamma(m - 0.5))
                / (2.0 * std::sqrt(cm) * std::tgamma(m));
        break;
    }
    case Voigt: {
        area = rawA;
        fwhm = wSafe;
        double d = std::max(0.0, std::min(1.0, d_voigt_mu));
        double eta = std::max(0.0, std::min(1.0, 1.36603 * d - 0.47719 * d * d + 0.11116 * d * d * d));
        double cG = std::sqrt(4.0 * M_LN2 / M_PI);
        height = (rawA / wSafe) * (eta * M_2_PI + (1.0 - eta) * cG);
        break;
    }
    case BiGaussian:
        height = rawA;
        fwhm = wSafe;
        area = 0.5 * std::sqrt(M_PI / M_LN2) * rawA * wSafe;
        break;
    case EMG: {
        area = rawA;
        double spanSearch = 3.0 * wSafe + 4.0 * d_tail_tau;
        const int nGrid = 201;
        std::vector<double> gx(static_cast<size_t>(nGrid)), gy(static_cast<size_t>(nGrid));
        int maxG = 0;
        height = 0.0;
        for (int gi = 0; gi < nGrid; ++gi) {
            gx[size_t(gi)] = -2.0 * wSafe + (spanSearch + 2.0 * wSafe) * double(gi) / double(nGrid - 1);
            gy[size_t(gi)] = eval_origin_peak_profile(EMG, rawA, 0.0, wSafe, gx[size_t(gi)],
                                                      d_voigt_mu, d_pearson_m, alphaEff, d_tail_tau);
            if (gy[size_t(gi)] > height) {
                height = gy[size_t(gi)];
                maxG = gi;
            }
        }
        if (maxG > 0 && maxG + 1 < nGrid) {
            const double ym1 = gy[size_t(maxG - 1)], y0p = gy[size_t(maxG)], yp1 = gy[size_t(maxG + 1)];
            const double denom = 2.0 * (2.0 * y0p - ym1 - yp1);
            if (denom > 1e-15) {
                const double delta = std::clamp((ym1 - yp1) / denom, -0.5, 0.5);
                height = std::max(height, y0p + 0.25 * (ym1 - yp1) * delta);
            }
        }
        if (height > 1e-15) {
            double halfH = 0.5 * height;
            double gl = gx.front(), gr = gx.back();
            for (int gi = maxG; gi > 0; --gi) {
                if (gy[size_t(gi - 1)] <= halfH) {
                    double f = (halfH - gy[size_t(gi - 1)])
                            / std::max(1e-15, gy[size_t(gi)] - gy[size_t(gi - 1)]);
                    gl = gx[size_t(gi - 1)] + f * (gx[size_t(gi)] - gx[size_t(gi - 1)]);
                    break;
                }
            }
            for (int gi = maxG; gi + 1 < nGrid; ++gi) {
                if (gy[size_t(gi + 1)] <= halfH) {
                    double f = (gy[size_t(gi)] - halfH)
                            / std::max(1e-15, gy[size_t(gi)] - gy[size_t(gi + 1)]);
                    gr = gx[size_t(gi)] + f * (gx[size_t(gi + 1)] - gx[size_t(gi)]);
                    break;
                }
            }
            fwhm = std::max(1e-12, gr - gl);
        } else {
            fwhm = wSafe * std::sqrt(2.0 * M_LN2);
        }
        break;
    }
    case LogNormal: {
        height = rawA;
        fwhm = wSafe;
        double asym = (std::abs(alphaEff) > 1e-4) ? std::abs(alphaEff) : 1.5;
        double b = std::log(asym + std::sqrt(asym * asym + 1.0));
        area = rawA * wSafe * std::sqrt(M_PI / (4.0 * M_LN2)) * (b / asym)
                * std::exp((b * b) / (4.0 * M_LN2));
        break;
    }
    case Asym2Sig:
    case DoniachSunjic: {
        // Shape metrics scale exactly with (A, w); the unit shape is integrated over the full
        // tails and the FWHM is measured from the half-maximum crossings (E4-7).
        const AsymUnitShape us = asymUnitShape(int(prof), alphaEff);
        fwhm = wSafe * us.fwhm;
        height = rawA * us.height;
        area = rawA * wSafe * us.area;
        break;
    }
    }

    fwhmOut = fwhm;
    heightOut = sign * height;
    areaOut = sign * area;
}

std::vector<MultiPeakFit::PeakSummaryRow>
MultiPeakFit::computePeakSummary(const std::vector<double> &res) const
{
    std::vector<PeakSummaryRow> rows;
    if (d_peaks <= 0 || res.size() < size_t(3 * d_peaks + 1))
        return rows;

    std::vector<double> effRes = res;
    if (!d_doublet_constraints.isEmpty())
        applyDoubletConstraints(effRes);

    rows.resize(static_cast<size_t>(d_peaks));
    const double y0 = effRes[size_t(3 * d_peaks)];
    double totalAbsArea = 0.0;

    for (int j = 0; j < d_peaks; ++j) {
        const PeakProfile prof = peakProfileAt(j);
        const double signedA = d_negative_peaks_allowed ? effRes[size_t(3 * j)]
                                                        : std::abs(effRes[size_t(3 * j)]);
        const double xc = effRes[size_t(3 * j + 1)];
        const double w = std::max(1e-15, std::abs(effRes[size_t(3 * j + 2)]));

        double fwhm = w, height = 0.0, area = 0.0;
        evalSinglePeakMetrics(prof, signedA, w, fwhm, height, area);

        rows[size_t(j)].index = j + 1;
        rows[size_t(j)].center = xc;
        rows[size_t(j)].fwhm = fwhm;
        rows[size_t(j)].height = height;
        rows[size_t(j)].area = area;
        rows[size_t(j)].areaPercent = 0.0;
        rows[size_t(j)].baseline = y0;
        totalAbsArea += std::abs(area);
    }

    for (int j = 0; j < d_peaks; ++j) {
        rows[size_t(j)].areaPercent = (totalAbsArea > 1e-15)
                ? (100.0 * std::abs(rows[size_t(j)].area) / totalAbsArea)
                : (100.0 / double(d_peaks));
    }

    // Propagate parameter covariance (including off-diagonal Cov(A, w) & doublet / linear couplings)
    const size_t nPeakPar = static_cast<size_t>(3 * d_peaks);
    const int dof = degreesOfFreedom();
    if (covar && std::isfinite(chi_2) && chi_2 >= 0.0 && !d_x.empty() && dof > 0
        && covar->size1 >= nPeakPar + 1 && covar->size2 >= nPeakPar + 1) {
        const double covFactor = effectiveCovarScale();
        const double nanVal = std::numeric_limits<double>::quiet_NaN();
        if (std::isfinite(covFactor)) {
            std::vector<std::vector<double>> cUser(nPeakPar, std::vector<double>(nPeakPar, 0.0));
            std::vector<bool> singularParam(nPeakPar, false);
            for (size_t a = 0; a < nPeakPar; ++a) {
                for (size_t b = 0; b < nPeakPar; ++b) {
                    const double rawC = gsl_matrix_get(covar, a, b);
                    cUser[a][b] = std::isfinite(rawC) ? (covFactor * rawC) : nanVal;
                }
                const double rawDiag = gsl_matrix_get(covar, a, a);
                singularParam[a] = !isParameterFixed(int(a))
                        && !(std::isfinite(rawDiag) && rawDiag > 0.0);
            }

            const size_t nPeaksSz = static_cast<size_t>(d_peaks);
            std::vector<std::vector<double>> dCenter(nPeaksSz, std::vector<double>(nPeakPar, 0.0));
            std::vector<std::vector<double>> dFwhm(nPeaksSz, std::vector<double>(nPeakPar, 0.0));
            std::vector<std::vector<double>> dHeight(nPeaksSz, std::vector<double>(nPeakPar, 0.0));
            std::vector<std::vector<double>> dArea(nPeaksSz, std::vector<double>(nPeakPar, 0.0));

            for (size_t k = 0; k < nPeakPar; ++k) {
                if (isParameterFixed(int(k)))
                    continue;
                const double h = std::max(1e-12, 1e-4 * std::abs(res[k]));
                std::vector<double> pPlus = res;
                std::vector<double> pMinus = res;
                pPlus[k] += h;
                pMinus[k] -= h;
                if (!d_doublet_constraints.isEmpty()) {
                    applyDoubletConstraints(pPlus);
                    applyDoubletConstraints(pMinus);
                }
                const double inv2h = 0.5 / h;
                for (int j = 0; j < d_peaks; ++j) {
                    const PeakProfile prof = peakProfileAt(j);
                    const double aP = d_negative_peaks_allowed ? pPlus[size_t(3 * j)]
                                                               : std::abs(pPlus[size_t(3 * j)]);
                    const double aM = d_negative_peaks_allowed ? pMinus[size_t(3 * j)]
                                                               : std::abs(pMinus[size_t(3 * j)]);
                    const double wP = std::max(1e-15, std::abs(pPlus[size_t(3 * j + 2)]));
                    const double wM = std::max(1e-15, std::abs(pMinus[size_t(3 * j + 2)]));
                    double fwP = 0.0, hP = 0.0, arP = 0.0;
                    double fwM = 0.0, hM = 0.0, arM = 0.0;
                    evalSinglePeakMetrics(prof, aP, wP, fwP, hP, arP);
                    evalSinglePeakMetrics(prof, aM, wM, fwM, hM, arM);

                    dCenter[size_t(j)][k] =
                            (pPlus[size_t(3 * j + 1)] - pMinus[size_t(3 * j + 1)]) * inv2h;
                    dFwhm[size_t(j)][k] = (fwP - fwM) * inv2h;
                    dHeight[size_t(j)][k] = (hP - hM) * inv2h;
                    dArea[size_t(j)][k] = (arP - arM) * inv2h;
                }
            }

            auto quadSE = [&](const std::vector<double> &grad) -> double {
                if (dof <= 0)
                    return nanVal;
                bool anyFreeGrad = false;
                double var = 0.0;
                for (size_t a = 0; a < nPeakPar; ++a) {
                    if (grad[a] == 0.0)
                        continue;
                    if (singularParam[a])
                        return nanVal;
                    if (!isParameterFixed(int(a)))
                        anyFreeGrad = true;
                    double rowSum = 0.0;
                    for (size_t b = 0; b < nPeakPar; ++b) {
                        if (grad[b] != 0.0)
                            rowSum += cUser[a][b] * grad[b];
                    }
                    var += grad[a] * rowSum;
                }
                if (!std::isfinite(var) || var < 0.0)
                    return nanVal;
                if (var == 0.0 && anyFreeGrad && chi_2 > 0.0)
                    return nanVal;
                return std::sqrt(var);
            };

            for (int j = 0; j < d_peaks; ++j) {
                rows[size_t(j)].centerSE = quadSE(dCenter[size_t(j)]);
                rows[size_t(j)].fwhmSE = quadSE(dFwhm[size_t(j)]);
                rows[size_t(j)].heightSE = quadSE(dHeight[size_t(j)]);
                rows[size_t(j)].areaSE = quadSE(dArea[size_t(j)]);
            }
        }
    }

    return rows;
}

namespace {

// "2θ / deg", "Intensity (cps)", "Load [N]" -> "deg", "cps", "N"
QString unitFromAxisTitle(const QString &rawTitle)
{
    QString t = rawTitle;
    t.remove(QRegExp(QStringLiteral("<[^>]*>")));
    t.replace(QStringLiteral("&sigma;"), QString(QChar(0x03C3)));
    t.replace(QStringLiteral("&theta;"), QString(QChar(0x03B8)));
    t.replace(QStringLiteral("&mu;"), QString(QChar(0x03BC)));
    t = t.trimmed();
    if (t.isEmpty())
        return QString();
    const int slash = t.lastIndexOf(QStringLiteral(" / "));
    if (slash >= 0)
        return t.mid(slash + 3).trimmed();
    QRegExp br(QStringLiteral("[\\(\\[]([^\\(\\)\\[\\]]+)[\\)\\]]\\s*$"));
    if (br.indexIn(t) >= 0)
        return br.cap(1).trimmed();
    return QString();
}

QString bracketUnit(const QString &u)
{
    return u.isEmpty() ? QString() : (QStringLiteral(" [") + u + QStringLiteral("]"));
}

} // namespace

void MultiPeakFit::resolveDataUnits(QString &xUnit, QString &yUnit) const
{
    xUnit.clear();
    yUnit.clear();
    if (const DataCurve *dc = dynamic_cast<const DataCurve *>(d_curve)) {
        if (Table *tbl = dc->table()) {
            if (Column *cx = tbl->column(dc->xColumnName()))
                xUnit = cx->units().trimmed();
            if (Column *cy = tbl->column(dc->yColumnName()))
                yUnit = cy->units().trimmed();
        }
    }
    if (d_graph && d_curve) {
        if (xUnit.isEmpty())
            xUnit = unitFromAxisTitle(d_graph->axisTitle(d_curve->xAxis()));
        if (yUnit.isEmpty())
            yUnit = unitFromAxisTitle(d_graph->axisTitle(d_curve->yAxis()));
    }
}

QString MultiPeakFit::areaUnit(const QString &xUnit, const QString &yUnit)
{
    if (xUnit.isEmpty() && yUnit.isEmpty())
        return QString();
    if (xUnit.isEmpty())
        return yUnit;
    if (yUnit.isEmpty())
        return xUnit;
    return yUnit + QString(QChar(0x00B7)) + xUnit;
}

QString MultiPeakFit::logFitInfo(const vector<double> &par, int iterations, int status,
                                 const QString &plotName)
{
    QString info = Fit::logFitInfo(par, iterations, status, plotName);

    auto rows = computePeakSummary(par);
    if (rows.empty())
        return info;

    auto fmtValSE = [this](double val, double se) -> QString {
        QString s = QLocale().toString(val, 'g', d_prec);
        if (std::isfinite(se) && se > 0.0)
            s += QStringLiteral(" ± ") + QLocale().toString(se, 'g', std::max(3, d_prec - 1));
        return s;
    };

    QString xU, yU;
    resolveDataUnits(xU, yU);
    const QString aU = areaUnit(xU, yU);
    info += tr("Peak #") + "\t" + tr("Center (xc)") + bracketUnit(xU) + "\t" + tr("FWHM")
            + bracketUnit(xU) + "\t" + tr("Height") + bracketUnit(yU) + "\t" + tr("Area")
            + bracketUnit(aU) + "\t" + tr("Area (%)") + "\t" + tr("Baseline (y0)")
            + bracketUnit(yU) + "\n";
    info += "--------------------------------------------------------------------------------------"
            "-\n";
    for (const auto &r : rows) {
        info += QString::number(r.index) + "\t";
        info += fmtValSE(r.center, r.centerSE) + "\t";
        info += fmtValSE(r.fwhm, r.fwhmSE) + "\t";
        info += fmtValSE(r.height, r.heightSE) + "\t";
        info += fmtValSE(r.area, r.areaSE) + "\t";
        info += QLocale().toString(r.areaPercent, 'f', 2) + "\t";
        info += QLocale().toString(r.baseline, 'g', d_prec) + "\n";
    }
    info += "--------------------------------------------------------------------------------------"
            "-\n";
    return info;
}

Table *MultiPeakFit::createPeakSummaryTable(const QString &tableName)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    const std::vector<double> &res = !d_last_results.empty() ? d_last_results : d_results;
    if (!app || d_init_err || res.size() < size_t(3 * d_peaks + 1))
        return nullptr;

    auto rows = computePeakSummary(res);
    if (rows.empty())
        return nullptr;

    QString name = tableName.isEmpty() ? app->generateUniqueName(tr("PeakSummary"))
                                       : app->generateUniqueName(tableName, false);
    Table *t = app->newTable(name, d_peaks, 17);
    if (!t)
        return nullptr;

    t->setHeader(QStringList() << QStringLiteral("Peak") << QStringLiteral("Center")
                               << QStringLiteral("FWHM") << QStringLiteral("Height")
                               << QStringLiteral("Area") << QStringLiteral("AreaPercent")
                               << QStringLiteral("y0") << QStringLiteral("COD_R2")
                               << QStringLiteral("Center_SE") << QStringLiteral("FWHM_SE")
                               << QStringLiteral("Height_SE") << QStringLiteral("Area_SE")
                               << QStringLiteral("Profile") << QStringLiteral("mu")
                               << QStringLiteral("m") << QStringLiteral("alpha")
                               << QStringLiteral("tau"));
    const QStringList longNames = {
        tr("Peak"),      tr("Center"),    tr("FWHM"),      tr("Height"),
        tr("Area"),      tr("AreaPercent"), tr("y0"),      tr("COD_R2"),
        tr("Center_SE"), tr("FWHM_SE"),   tr("Height_SE"), tr("Area_SE"),
        tr("Profile"),   QStringLiteral("mu"), QStringLiteral("m"), QStringLiteral("alpha"),
        QStringLiteral("tau")
    };
    for (int col = 0; col < 17; ++col) {
        t->column(col)->setColumnMode(col == 12 ? SciDAVis::ColumnMode::Text
                                                : SciDAVis::ColumnMode::Numeric);
        t->column(col)->setLongName(longNames[col]);
    }

    auto setNumOrInvalid = [&](int row, int col, double val) {
        if (std::isfinite(val))
            t->column(col)->setValueAt(row, val);
        else
            t->column(col)->setInvalid(row, true);
    };
    double r2 = rSquare();
    for (int i = 0; i < d_peaks && i < int(rows.size()); ++i) {
        const PeakProfile prof = peakProfileAt(i);
        t->column(0)->setValueAt(i, rows[i].index);
        t->column(1)->setValueAt(i, rows[i].center);
        t->column(2)->setValueAt(i, rows[i].fwhm);
        t->column(3)->setValueAt(i, rows[i].height);
        t->column(4)->setValueAt(i, rows[i].area);
        t->column(5)->setValueAt(i, rows[i].areaPercent);
        t->column(6)->setValueAt(i, rows[i].baseline);
        setNumOrInvalid(i, 7, r2);
        setNumOrInvalid(i, 8, rows[i].centerSE);
        setNumOrInvalid(i, 9, rows[i].fwhmSE);
        setNumOrInvalid(i, 10, rows[i].heightSE);
        setNumOrInvalid(i, 11, rows[i].areaSE);
        t->column(12)->setTextAt(i, profileDisplayName(prof));
        if (prof == PseudoVoigt || prof == Voigt)
            t->column(13)->setValueAt(i, d_voigt_mu);
        if (prof == PearsonVII)
            t->column(14)->setValueAt(i, d_pearson_m);
        if (prof == BiGaussian || prof == LogNormal || prof == Asym2Sig)
            t->column(15)->setValueAt(i, d_asym_alpha);
        else if (prof == DoniachSunjic)
            t->column(15)->setValueAt(i, effectiveDoniachAlpha(d_asym_alpha));
        if (prof == EMG)
            t->column(16)->setValueAt(i, d_tail_tau);
    }

    QString xU, yU;
    resolveDataUnits(xU, yU);
    const QString aU = areaUnit(xU, yU);
    for (int col : { 1, 2, 8, 9 })
        t->column(col)->setUnits(xU);
    for (int col : { 3, 6, 10 })
        t->column(col)->setUnits(yU);
    for (int col : { 4, 11 })
        t->column(col)->setUnits(aU);
    t->column(5)->setUnits(QStringLiteral("%"));

    if (!d_silent)
        t->showNormal();
    return t;
}

std::vector<MultiPeakFit::PeakSummaryRow> MultiPeakFit::initialPeakSummary() const
{
    if (!d_initial_guesses_set && !d_x.empty())
        const_cast<MultiPeakFit *>(this)->guessInitialValues();
    if (!d_param_init || d_p < unsigned(3 * d_peaks + 1))
        return std::vector<PeakSummaryRow>();
    std::vector<double> initVec(d_p);
    for (unsigned i = 0; i < d_p; ++i)
        initVec[i] = gsl_vector_get(d_param_init, i);
    if (!usesCustomMultiPeakCallbacks() && d_profile == Lorentz) {
        for (int j = 0; j < d_peaks; ++j)
            initVec[3 * j] *= M_PI_2;
    }
    if (!d_doublet_constraints.isEmpty())
        applyDoubletConstraints(initVec);
    return computePeakSummary(initVec);
}

std::vector<double> MultiPeakFit::computeBaseline(const std::vector<double> &x,
                                                  const std::vector<double> &y) const
{
    const size_t n = x.size();
    std::vector<double> base(n, 0.0);
    if (n == 0 || y.size() != n)
        return base;

    switch (d_baseline_mode) {
    case BaselineZero:
        return base;
    case BaselineConstant: {
        double y0 = estimateConstantBaselineY0(y);
        std::fill(base.begin(), base.end(), y0);
        return base;
    }
    case BaselineLinear: {
        size_t k = std::min<size_t>(3, n);
        double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;
        for (size_t i = 0; i < k; ++i) {
            x1 += x[i];
            y1 += y[i];
            x2 += x[n - 1 - i];
            y2 += y[n - 1 - i];
        }
        x1 /= double(k);
        y1 /= double(k);
        x2 /= double(k);
        y2 /= double(k);
        double dx = x2 - x1;
        for (size_t i = 0; i < n; ++i) {
            if (std::abs(dx) > 1e-15)
                base[i] = y1 + (y2 - y1) * ((x[i] - x1) / dx);
            else
                base[i] = 0.5 * (y1 + y2);
        }
        return base;
    }
    case BaselineALS: {
        bool upperEnvelope = false;
        if (d_negative_peaks_allowed && n >= 3) {
            std::vector<double> ys = y;
            std::sort(ys.begin(), ys.end());
            const double yMed = ys[n / 2];
            upperEnvelope = ((yMed - ys.front()) > 1.25 * (ys.back() - yMed));
        }
        // Initial mild 3-point noise suppression
        base = y;
        if (n >= 3) {
            for (size_t i = 1; i + 1 < n; ++i)
                base[i] = 0.25 * y[i - 1] + 0.5 * y[i] + 0.25 * y[i + 1];
        }
        // Iterative SNIP / moving-chord envelope
        int maxW = std::max(3, int(n) / 4);
        std::vector<double> tmp = base;
        for (int p = maxW; p >= 1; --p) {
            for (size_t i = size_t(p); i + size_t(p) < n; ++i) {
                double chord = 0.5 * (base[i - p] + base[i + p]);
                tmp[i] = upperEnvelope ? std::max(base[i], chord) : std::min(base[i], chord);
            }
            base = tmp;
        }
        // Smooth interior with 5-point Gaussian filter
        for (int pass = 0; pass < 3 && n >= 5; ++pass) {
            tmp = base;
            for (size_t i = 2; i + 2 < n; ++i) {
                tmp[i] = (base[i - 2] + 4.0 * base[i - 1] + 6.0 * base[i] + 4.0 * base[i + 1]
                          + base[i + 2])
                        / 16.0;
            }
            base = tmp;
        }
        return base;
    }
    case BaselineAnchors: {
        QVector<QPointF> anchors = d_baseline_anchors;
        if (anchors.isEmpty() && n >= 4) {
            // Automatic fallback 4-anchor envelope when no explicit anchors were supplied
            const int nBins = 4;
            for (int b = 0; b < nBins; ++b) {
                size_t i0 = (size_t(b) * n) / nBins;
                size_t i1 = std::max(i0 + 1, (size_t(b + 1) * n) / nBins);
                size_t minIdx = i0;
                for (size_t k = i0 + 1; k < i1 && k < n; ++k) {
                    if (y[k] < y[minIdx])
                        minIdx = k;
                }
                if (anchors.isEmpty() || std::abs(x[minIdx] - anchors.back().x()) > 1e-12)
                    anchors.push_back(QPointF(x[minIdx], y[minIdx]));
            }
        }
        const int m = anchors.size();
        if (m == 0)
            return base;
        if (m == 1) {
            std::fill(base.begin(), base.end(), anchors[0].y());
            return base;
        }

        std::vector<double> ax(static_cast<size_t>(m)), ay(static_cast<size_t>(m));
        for (int k = 0; k < m; ++k) {
            ax[size_t(k)] = anchors[k].x();
            ay[size_t(k)] = anchors[k].y();
        }

        if (m >= 3 && d_anchors_cubic_spline) {
            gsl_error_handler_t *oldHandler = gsl_set_error_handler_off();
            gsl_interp_accel *acc = gsl_interp_accel_alloc();
            gsl_spline *sp = gsl_spline_alloc(gsl_interp_cspline, size_t(m));
            if (acc && sp && gsl_spline_init(sp, ax.data(), ay.data(), size_t(m)) == GSL_SUCCESS) {
                double dLeft = gsl_spline_eval_deriv(sp, ax.front(), acc);
                double dRight = gsl_spline_eval_deriv(sp, ax.back(), acc);
                for (size_t i = 0; i < n; ++i) {
                    double xi = x[i];
                    if (xi <= ax.front())
                        base[i] = ay.front() + dLeft * (xi - ax.front());
                    else if (xi >= ax.back())
                        base[i] = ay.back() + dRight * (xi - ax.back());
                    else
                        base[i] = gsl_spline_eval(sp, xi, acc);
                }
                gsl_spline_free(sp);
                gsl_interp_accel_free(acc);
                gsl_set_error_handler(oldHandler);
                return base;
            }
            if (sp)
                gsl_spline_free(sp);
            if (acc)
                gsl_interp_accel_free(acc);
            gsl_set_error_handler(oldHandler);
        }

        // Piecewise linear interpolation + linear boundary extrapolation
        for (size_t i = 0; i < n; ++i) {
            double xi = x[i];
            if (xi <= ax.front()) {
                double dx0 = ax[1] - ax[0];
                double slope = (std::abs(dx0) > 1e-15) ? ((ay[1] - ay[0]) / dx0) : 0.0;
                base[i] = ay[0] + slope * (xi - ax[0]);
            } else if (xi >= ax.back()) {
                double dxEnd = ax[size_t(m - 1)] - ax[size_t(m - 2)];
                double slope = (std::abs(dxEnd) > 1e-15)
                        ? ((ay[size_t(m - 1)] - ay[size_t(m - 2)]) / dxEnd)
                        : 0.0;
                base[i] = ay.back() + slope * (xi - ax.back());
            } else {
                auto it = std::upper_bound(ax.begin(), ax.end(), xi);
                size_t k1 = size_t(std::distance(ax.begin(), it));
                if (k1 == 0)
                    k1 = 1;
                if (k1 >= size_t(m))
                    k1 = size_t(m - 1);
                size_t k0 = k1 - 1;
                double dxSeg = ax[k1] - ax[k0];
                double frac = (std::abs(dxSeg) > 1e-15) ? ((xi - ax[k0]) / dxSeg) : 0.0;
                base[i] = ay[k0] + frac * (ay[k1] - ay[k0]);
            }
        }
        return base;
    }
    }
    return base;
}

std::vector<double> MultiPeakFit::computeBaseline() const
{
    return computeBaseline(d_x, d_y);
}

bool MultiPeakFit::subtractBaselineFromData()
{
    if (d_x.empty() || d_y.size() != d_x.size())
        return false;

    d_last_baseline = computeBaseline(d_x, d_y);
    if (d_last_baseline.size() != d_y.size())
        return false;

    for (size_t i = 0; i < d_y.size(); ++i)
        d_y[i] -= d_last_baseline[i];

    d_baseline_subtracted = true;
    autoDetectPeaks(d_peaks);
    if (d_param_init && d_p > 0)
        gsl_vector_set(d_param_init, d_p - 1, 0.0);
    if (d_results.size() >= d_p && d_p > 0)
        d_results[d_p - 1] = 0.0;
    return true;
}

PlotCurve *MultiPeakFit::insertBaselineCurve()
{
    if (!d_graph || d_x.empty() || d_y.size() != d_x.size())
        return nullptr;

    std::vector<double> base = (d_baseline_subtracted && d_last_baseline.size() == d_x.size())
            ? d_last_baseline
            : computeBaseline(d_x, d_y);
    if (base.size() != d_x.size())
        return nullptr;

    QString title = d_graph->generateFunctionName(tr("Baseline"));
    FunctionCurve *c =
            new FunctionCurve((ApplicationWindow *)parent(), FunctionCurve::Normal, title);
    c->setPen(QPen(QColor("#1f77b4"), 2, Qt::DashLine));
    c->setData(d_x.data(), base.data(), int(d_x.size()));
    c->setRange(d_x.front(), d_x.back());
    c->setFormula(QString());
    attachToSourceAxes(c);
    d_graph->insertPlotItem(c, Graph::Line);
    d_graph->addFitCurve(c);
    d_graph->replot();
    return c;
}

Table *MultiPeakFit::integratePeaksToTable(const QString &tableName)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    const size_t n = d_x.size();
    if (!app || n < 3 || d_y.size() != n)
        return nullptr;

    if (!d_initial_guesses_set && d_last_results.empty())
        autoDetectPeaks(d_peaks);

    std::vector<double> base(n, 0.0);
    if (!d_baseline_subtracted) {
        if (d_baseline_mode == BaselineConstant && !d_last_results.empty()
            && d_last_results.size() >= size_t(3 * d_peaks + 1)) {
            std::fill(base.begin(), base.end(), d_last_results[3 * d_peaks]);
        } else {
            base = computeBaseline(d_x, d_y);
        }
    }

    std::vector<double> y_signed(n, 0.0);
    std::vector<double> y_net(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        y_signed[i] = d_y[i] - base[i];
        y_net[i] = d_negative_peaks_allowed ? std::abs(y_signed[i]) : std::max(0.0, y_signed[i]);
    }

    // Determine peak centers from fitted results or initial guesses
    const std::vector<double> &parSrc = !d_last_results.empty() ? d_last_results : d_results;
    std::vector<double> centers(d_peaks, 0.0);
    for (int m = 0; m < d_peaks; ++m) {
        if (parSrc.size() >= size_t(3 * d_peaks + 1) && parSrc[3 * m + 1] != 0.0)
            centers[m] = parSrc[3 * m + 1];
        else if (d_param_init && d_p >= unsigned(3 * d_peaks + 1))
            centers[m] = gsl_vector_get(d_param_init, 3 * m + 1);
        else
            centers[m] = d_x[(size_t(m + 1) * n) / size_t(d_peaks + 1)];
    }
    std::sort(centers.begin(), centers.end());

    std::vector<size_t> peakIdx(d_peaks, 0);
    for (int m = 0; m < d_peaks; ++m) {
        size_t bestI = 0;
        double bestD = std::abs(d_x[0] - centers[m]);
        for (size_t i = 1; i < n; ++i) {
            double d = std::abs(d_x[i] - centers[m]);
            if (d < bestD) {
                bestD = d;
                bestI = i;
            }
        }
        peakIdx[m] = bestI;
    }

    // Partition data domain at valleys between adjacent peaks
    std::vector<size_t> bounds(d_peaks + 1, 0);
    bounds[0] = 0;
    bounds[d_peaks] = n - 1;
    for (int m = 1; m < d_peaks; ++m) {
        size_t iL = peakIdx[m - 1];
        size_t iR = std::max(iL, peakIdx[m]);
        size_t valleyIdx = (iL + iR) / 2;
        double minVal = y_net[valleyIdx];
        for (size_t k = iL; k <= iR; ++k) {
            if (y_net[k] < minVal) {
                minVal = y_net[k];
                valleyIdx = k;
            }
        }
        bounds[m] = valleyIdx;
    }

    struct IntRow
    {
        int index;
        double center;
        double height;
        double fwhm;
        double area;
        double areaPct;
        double centroid;
    };
    std::vector<IntRow> rows(d_peaks);
    double totalArea = 0.0;

    for (int m = 0; m < d_peaks; ++m) {
        size_t iStart = bounds[m];
        size_t iEnd = bounds[m + 1];
        if (iEnd <= iStart) {
            iStart = 0;
            iEnd = n - 1;
        }
        size_t maxIdx = peakIdx[m];
        double maxH = y_net[maxIdx];
        for (size_t i = iStart; i <= iEnd; ++i) {
            if (y_net[i] > maxH) {
                maxH = y_net[i];
                maxIdx = i;
            }
        }
        double xc = d_x[maxIdx];
        double halfH = 0.5 * maxH;
        double xL = d_x[iStart], xR = d_x[iEnd];
        for (int k = int(maxIdx); k > int(iStart); --k) {
            if (y_net[k - 1] <= halfH) {
                double dy = y_net[k] - y_net[k - 1];
                double f = (std::abs(dy) > 1e-15) ? ((halfH - y_net[k - 1]) / dy) : 0.5;
                xL = d_x[k - 1] + std::max(0.0, std::min(1.0, f)) * (d_x[k] - d_x[k - 1]);
                break;
            }
        }
        for (size_t k = maxIdx; k < iEnd; ++k) {
            if (y_net[k + 1] <= halfH) {
                double dy = y_net[k] - y_net[k + 1];
                double f = (std::abs(dy) > 1e-15) ? ((y_net[k] - halfH) / dy) : 0.5;
                xR = d_x[k] + std::max(0.0, std::min(1.0, f)) * (d_x[k + 1] - d_x[k]);
                break;
            }
        }
        double fwhm = std::max(1e-9, std::abs(xR - xL));

        double area = 0.0;
        double moment1 = 0.0;
        for (size_t i = iStart; i < iEnd; ++i) {
            double dx = std::abs(d_x[i + 1] - d_x[i]);
            double segArea = 0.5 * (y_net[i] + y_net[i + 1]) * dx;
            double segMoment = 0.5 * (d_x[i] * y_net[i] + d_x[i + 1] * y_net[i + 1]) * dx;
            area += segArea;
            moment1 += segMoment;
        }
        double centroid = (area > 1e-15) ? (moment1 / area) : xc;
        const double signPk =
                (d_negative_peaks_allowed && y_signed[maxIdx] < 0.0) ? -1.0 : 1.0;

        rows[m] = { m + 1, xc, signPk * maxH, fwhm, signPk * area, 0.0, centroid };
        totalArea += area;
    }

    for (int m = 0; m < d_peaks; ++m)
        rows[m].areaPct = (totalArea > 1e-15)
                ? (100.0 * std::abs(rows[m].area) / totalArea)
                : (100.0 / d_peaks);

    QString name = tableName.isEmpty() ? app->generateUniqueName(tr("PeakIntegration"))
                                       : app->generateUniqueName(tableName, false);
    Table *t = app->newTable(name, d_peaks, 7);
    if (!t)
        return nullptr;

    t->setHeader(QStringList() << QStringLiteral("Peak") << QStringLiteral("Center")
                               << QStringLiteral("Height") << QStringLiteral("FWHM")
                               << QStringLiteral("IntegratedArea") << QStringLiteral("AreaPercent")
                               << QStringLiteral("Centroid"));
    const QStringList longNames = {
        tr("Peak"), tr("Center"), tr("Height"), tr("FWHM"),
        tr("IntegratedArea"), tr("AreaPercent"), tr("Centroid")
    };
    for (int col = 0; col < 7; ++col) {
        t->column(col)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        t->column(col)->setLongName(longNames[col]);
    }

    for (int m = 0; m < d_peaks; ++m) {
        t->column(0)->setValueAt(m, rows[m].index);
        t->column(1)->setValueAt(m, rows[m].center);
        t->column(2)->setValueAt(m, rows[m].height);
        t->column(3)->setValueAt(m, rows[m].fwhm);
        t->column(4)->setValueAt(m, rows[m].area);
        t->column(5)->setValueAt(m, rows[m].areaPct);
        t->column(6)->setValueAt(m, rows[m].centroid);
    }

    QString xU, yU;
    resolveDataUnits(xU, yU);
    const QString aU = areaUnit(xU, yU);
    for (int col : { 1, 3, 6 })
        t->column(col)->setUnits(xU);
    t->column(2)->setUnits(yU);
    t->column(4)->setUnits(aU);
    t->column(5)->setUnits(QStringLiteral("%"));

    t->showNormal();
    return t;
}

Table *MultiPeakFit::batchAnalyzeCurves(ApplicationWindow *app, Graph *g,
                                        const QStringList &curveTitles, PeakProfile profile,
                                        BaselineMode baseMode, bool allowNegativePeaks)
{
    if (!app || !g || curveTitles.isEmpty())
        return nullptr;

    struct BatchPeakRow
    {
        QString curveName;
        int peakIdx = 1;
        QString profileStr;
        double center = 0.0;
        double fwhm = 0.0;
        double height = 0.0;
        double area = 0.0;
        double areaPct = 0.0;
        double r2 = 0.0;
        double centerSE = std::numeric_limits<double>::quiet_NaN();
        double fwhmSE = std::numeric_limits<double>::quiet_NaN();
        double heightSE = std::numeric_limits<double>::quiet_NaN();
        double areaSE = std::numeric_limits<double>::quiet_NaN();
    };

    std::vector<BatchPeakRow> allRows;
    const QString profStr = profileDisplayName(profile);
    QString batchXU, batchYU;
    bool haveUnits = false;

    for (const QString &curveTitle : curveTitles) {
        if (curveTitle.isEmpty() || g->curveIndex(curveTitle) < 0)
            continue;

        MultiPeakFit mpf(app, g, profile, 1);
        mpf.d_silent = true;
        mpf.enablePeakCurves(false);
        mpf.setNegativePeaksAllowed(allowNegativePeaks);
        if (!mpf.setDataFromCurve(curveTitle, g) || mpf.error())
            continue;
        if (!haveUnits) {
            mpf.resolveDataUnits(batchXU, batchYU);
            haveUnits = (!batchXU.isEmpty() || !batchYU.isEmpty());
        }

        mpf.setBaselineMode(baseMode);
        if (baseMode != BaselineZero)
            mpf.subtractBaselineFromData();

        int detectedCount = mpf.autoDetectAndUpdateNumPeaks(10, 0.08);
        if (detectedCount < 1) {
            mpf.setNumPeaks(1);
            mpf.guessInitialValues();
        }

        mpf.fit();

        const std::vector<double> &res = mpf.results();
        if (res.empty())
            continue;

        auto summary = mpf.computePeakSummary(res);
        double r2Val = mpf.rSquare();
        for (const auto &s : summary) {
            BatchPeakRow row;
            row.curveName = curveTitle;
            row.peakIdx = s.index;
            row.profileStr = profStr;
            row.center = s.center;
            row.fwhm = s.fwhm;
            row.height = s.height;
            row.area = s.area;
            row.areaPct = s.areaPercent;
            row.r2 = r2Val;
            row.centerSE = s.centerSE;
            row.fwhmSE = s.fwhmSE;
            row.heightSE = s.heightSE;
            row.areaSE = s.areaSE;
            allRows.push_back(row);
        }
    }

    if (allRows.empty())
        return nullptr;

    QString tblName = app->generateUniqueName(tr("BatchPeakSummary"));
    int nRows = int(allRows.size());
    Table *tbl = app->newTable(tblName, nRows, 13);
    if (!tbl)
        return nullptr;

    tbl->setHeader(QStringList() << QStringLiteral("Curve") << QStringLiteral("Peak")
                                 << QStringLiteral("Profile") << QStringLiteral("Center")
                                 << QStringLiteral("FWHM") << QStringLiteral("Height")
                                 << QStringLiteral("Area") << QStringLiteral("AreaPercent")
                                 << QStringLiteral("COD_R2") << QStringLiteral("Center_SE")
                                 << QStringLiteral("FWHM_SE") << QStringLiteral("Height_SE")
                                 << QStringLiteral("Area_SE"));
    const QStringList batchLongNames = {
        tr("Curve"),     tr("Peak"),      tr("Profile"),   tr("Center"),
        tr("FWHM"),      tr("Height"),    tr("Area"),      tr("AreaPercent"),
        tr("COD_R2"),    tr("Center_SE"), tr("FWHM_SE"),   tr("Height_SE"),
        tr("Area_SE")
    };
    tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
    tbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    tbl->column(2)->setColumnMode(SciDAVis::ColumnMode::Text);
    for (int col = 3; col < 13; ++col)
        tbl->column(col)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    for (int col = 0; col < 13; ++col)
        tbl->column(col)->setLongName(batchLongNames[col]);

    auto setBatchNumOrInvalid = [&](int row, int col, double val) {
        if (std::isfinite(val))
            tbl->column(col)->setValueAt(row, val);
        else
            tbl->column(col)->setInvalid(row, true);
    };
    for (int i = 0; i < nRows; ++i) {
        const auto &r = allRows[size_t(i)];
        tbl->column(0)->setTextAt(i, r.curveName);
        tbl->column(1)->setValueAt(i, r.peakIdx);
        tbl->column(2)->setTextAt(i, r.profileStr);
        tbl->column(3)->setValueAt(i, r.center);
        tbl->column(4)->setValueAt(i, r.fwhm);
        tbl->column(5)->setValueAt(i, r.height);
        tbl->column(6)->setValueAt(i, r.area);
        tbl->column(7)->setValueAt(i, r.areaPct);
        setBatchNumOrInvalid(i, 8, r.r2);
        setBatchNumOrInvalid(i, 9, r.centerSE);
        setBatchNumOrInvalid(i, 10, r.fwhmSE);
        setBatchNumOrInvalid(i, 11, r.heightSE);
        setBatchNumOrInvalid(i, 12, r.areaSE);
    }

    const QString batchAU = areaUnit(batchXU, batchYU);
    if (!batchXU.isEmpty()) {
        for (int col : { 3, 4, 9, 10 })
            tbl->column(col)->setUnits(batchXU);
    }
    if (!batchYU.isEmpty()) {
        for (int col : { 5, 11 })
            tbl->column(col)->setUnits(batchYU);
    }
    if (!batchAU.isEmpty()) {
        for (int col : { 6, 12 })
            tbl->column(col)->setUnits(batchAU);
    }
    tbl->column(7)->setUnits(QStringLiteral("%"));

    tbl->showNormal();
    return tbl;
}

void MultiPeakFit::showBatchPeakDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g || isNonInteractiveFit(app))
        return;

    QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        curves = g->curvesList();
    if (curves.isEmpty()) {
        QMessageBox::information(app, tr("Batch Peak Analyzer"),
                                 tr("There are no data curves available on the active graph."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(tr("Batch Peak Analyzer"));
    dlg.resize(440, 410);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->addWidget(new QLabel(tr("Select curves to analyze:")));

    QListWidget *curveList = new QListWidget(&dlg);
    for (const QString &cName : curves) {
        QListWidgetItem *item = new QListWidgetItem(cName, curveList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    if (curveList->count() > 0)
        curveList->setCurrentRow(0);
    mainLayout->addWidget(curveList);

    QFormLayout *form = new QFormLayout();
    QComboBox *profileBox = new QComboBox(&dlg);
    profileBox->addItem(tr("Gaussian"), int(Gauss));
    profileBox->addItem(tr("Lorentzian"), int(Lorentz));
    profileBox->addItem(tr("Gaussian (Amplitude)"), int(GaussAmp));
    profileBox->addItem(tr("Pseudo-Voigt"), int(PseudoVoigt));
    profileBox->addItem(tr("Pearson VII"), int(PearsonVII));
    profileBox->addItem(tr("Voigt"), int(Voigt));
    profileBox->addItem(tr("BiGaussian"), int(BiGaussian));
    profileBox->addItem(tr("EMG (Exponentially Modified Gaussian)"), int(EMG));
    profileBox->addItem(tr("LogNormal"), int(LogNormal));
    profileBox->addItem(tr("Asymmetric Double Sigmoidal"), int(Asym2Sig));
    profileBox->addItem(tr("Doniach-Sunjic"), int(DoniachSunjic));
    form->addRow(tr("Peak Profile:"), profileBox);

    QComboBox *baseBox = new QComboBox(&dlg);
    baseBox->addItem(tr("Constant"), int(BaselineConstant));
    baseBox->addItem(tr("Zero (y = 0)"), int(BaselineZero));
    baseBox->addItem(tr("Linear Endpoints"), int(BaselineLinear));
    baseBox->addItem(tr("ALS / SNIP Envelope"), int(BaselineALS));
    baseBox->addItem(tr("Cubic Spline Anchors"), int(BaselineAnchors));
    baseBox->setCurrentIndex(2); // BaselineLinear default
    form->addRow(tr("Baseline Mode:"), baseBox);

    QCheckBox *negPeaksChk = new QCheckBox(tr("Allow negative peaks (troughs)"), &dlg);
    form->addRow(QString(), negPeaksChk);

    QCheckBox *livePreviewChk =
            new QCheckBox(tr("Live composite initial-guess preview on graph"), &dlg);
    livePreviewChk->setChecked(true);
    form->addRow(QString(), livePreviewChk);

    mainLayout->addLayout(form);

    // Lifetime-safe composite preview curve attached to g->plotWidget()
    struct PreviewGuard
    {
        QPointer<Graph> graph;
        QwtPlotCurve *curve = nullptr;
        ~PreviewGuard()
        {
            clear();
        }
        void clear()
        {
            if (curve) {
                if (graph && graph->plotWidget()) {
                    curve->detach();
                    graph->replot();
                }
                delete curve;
                curve = nullptr;
            }
        }
    } previewGuard;
    previewGuard.graph = g;

    auto updateCompositePreview = [&]() {
        if (!previewGuard.graph || !previewGuard.graph->plotWidget()
            || !livePreviewChk->isChecked()) {
            previewGuard.clear();
            return;
        }
        QString targetCurve;
        if (QListWidgetItem *cur = curveList->currentItem())
            targetCurve = cur->text();
        if (targetCurve.isEmpty()) {
            for (int i = 0; i < curveList->count(); ++i) {
                if (curveList->item(i)->checkState() == Qt::Checked) {
                    targetCurve = curveList->item(i)->text();
                    break;
                }
            }
        }
        if (targetCurve.isEmpty() || previewGuard.graph->curveIndex(targetCurve) < 0) {
            previewGuard.clear();
            return;
        }

        const PeakProfile prof = static_cast<PeakProfile>(profileBox->currentData().toInt());
        const BaselineMode bMode = static_cast<BaselineMode>(baseBox->currentData().toInt());

        MultiPeakFit mpf(app, previewGuard.graph, prof, 1);
        mpf.d_silent = true;
        mpf.d_suppress_curve_output = true;
        mpf.enablePeakCurves(false);
        mpf.setNegativePeaksAllowed(negPeaksChk->isChecked());
        if (!mpf.setDataFromCurve(targetCurve, previewGuard.graph) || mpf.error()) {
            previewGuard.clear();
            return;
        }
        mpf.setBaselineMode(bMode);
        std::vector<double> baseVec;
        if (bMode != BaselineZero) {
            baseVec = mpf.computeBaseline();
            mpf.subtractBaselineFromData();
        }
        mpf.autoDetectAndUpdateNumPeaks(10, 0.08);
        std::vector<double> px, py;
        if (!mpf.evaluatePreviewCurve(px, py, int(mpf.d_x.size())) || px.size() < 2) {
            previewGuard.clear();
            return;
        }
        if (baseVec.size() == mpf.d_x.size() && py.size() == baseVec.size()) {
            for (size_t i = 0; i < py.size(); ++i)
                py[i] += baseVec[i];
        }

        if (!previewGuard.curve) {
            previewGuard.curve = new QwtPlotCurve(tr("Composite Preview"));
            previewGuard.curve->setItemAttribute(QwtPlotItem::Legend, false);
            QPen pen(QColor(217, 95, 2, 215), 2, Qt::DashLine);
            previewGuard.curve->setPen(pen);
            previewGuard.curve->attach(previewGuard.graph->plotWidget());
        }
        if (QwtPlotCurve *srcC = previewGuard.graph->curve(targetCurve))
            previewGuard.curve->setAxis(srcC->xAxis(), srcC->yAxis());
        previewGuard.curve->setData(px.data(), py.data(), static_cast<int>(px.size()));
        previewGuard.graph->replot();
    };

    QObject::connect(profileBox, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { updateCompositePreview(); });
    QObject::connect(baseBox, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { updateCompositePreview(); });
    QObject::connect(negPeaksChk, &QCheckBox::toggled, &dlg,
                     [&](bool) { updateCompositePreview(); });
    QObject::connect(livePreviewChk, &QCheckBox::toggled, &dlg,
                     [&](bool) { updateCompositePreview(); });
    QObject::connect(curveList, &QListWidget::currentRowChanged, &dlg,
                     [&](int) { updateCompositePreview(); });
    QObject::connect(curveList, &QListWidget::itemChanged, &dlg,
                     [&](QListWidgetItem *) { updateCompositePreview(); });
    updateCompositePreview();

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    mainLayout->addWidget(btns);

    const int rc = dlg.exec();
    previewGuard.clear();
    if (rc != QDialog::Accepted)
        return;

    QStringList selectedCurves;
    for (int i = 0; i < curveList->count(); ++i) {
        if (curveList->item(i)->checkState() == Qt::Checked)
            selectedCurves << curveList->item(i)->text();
    }
    if (selectedCurves.isEmpty())
        return;

    PeakProfile selProf = static_cast<PeakProfile>(profileBox->currentData().toInt());
    BaselineMode selBase = static_cast<BaselineMode>(baseBox->currentData().toInt());
    batchAnalyzeCurves(app, g, selectedCurves, selProf, selBase, negPeaksChk->isChecked());
}

/*****************************************************************************
 *
 * Class LorentzFit
 *
 *****************************************************************************/

LorentzFit::LorentzFit(ApplicationWindow *parent, Graph *g)
    : MultiPeakFit(parent, g, MultiPeakFit::Lorentz, 1)
{
    init();
}

LorentzFit::LorentzFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle)
    : MultiPeakFit(parent, g, MultiPeakFit::Lorentz, 1)
{
    init();
    setDataFromCurve(curveTitle);
}

LorentzFit::LorentzFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
                       double end)
    : MultiPeakFit(parent, g, MultiPeakFit::Lorentz, 1)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

void LorentzFit::init()
{
    setObjectName("Lorentz");
    d_explanation = tr("Lorentz");
    d_param_explain.clear();
    d_param_explain << tr("(area)") << tr("(center)") << tr("(width)") << tr("(offset)");
    enablePeakCurves(false);
}

/*****************************************************************************
 *
 * Class GaussFit
 *
 *****************************************************************************/

GaussFit::GaussFit(ApplicationWindow *parent, Graph *g)
    : MultiPeakFit(parent, g, MultiPeakFit::Gauss, 1)
{
    init();
}

GaussFit::GaussFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle)
    : MultiPeakFit(parent, g, MultiPeakFit::Gauss, 1)
{
    init();
    setDataFromCurve(curveTitle);
}

GaussFit::GaussFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
                   double end)
    : MultiPeakFit(parent, g, MultiPeakFit::Gauss, 1)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

void GaussFit::init()
{
    setObjectName("Gauss");
    d_explanation = tr("Gauss");
    d_param_explain.clear();
    d_param_explain << tr("(area)") << tr("(center)") << tr("(width)") << tr("(offset)");
    enablePeakCurves(false);
}

/*****************************************************************************
 *
 * Class GaussAmpFit
 *
 *****************************************************************************/

GaussAmpFit::GaussAmpFit(ApplicationWindow *parent, Graph *g) : Fit(parent, g)
{
    init();
}

GaussAmpFit::GaussAmpFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle);
}

GaussAmpFit::GaussAmpFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle,
                         double start, double end)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

void GaussAmpFit::init()
{
    setObjectName("GaussAmp");
    d_f = gauss_f;
    d_df = gauss_df;
    d_fdf = gauss_fdf;
    d_fsimplex = gauss_d;
    d_p = 4;
    d_min_points = d_p;
    d_param_init = gsl_vector_alloc(d_p);
    gsl_vector_set_all(d_param_init, 1.0);
    covar = gsl_matrix_alloc(d_p, d_p);
    d_results.resize(d_p);
    d_param_explain << tr("(offset)") << tr("(height)") << tr("(center)") << tr("(width)");
    d_param_names << "y0"
                  << "A"
                  << "xc"
                  << "w";
    d_explanation = tr("GaussAmp Fit");
    d_formula = "y0+A*exp(-(x-xc)^2/(2*w^2))";
}

void GaussAmpFit::guessInitialValues()
{
    if (d_x.empty() || d_y.size() != d_x.size())
        return;

    // A dip (largest deviation from the baseline downward) is the same shape with A < 0: work
    // on the mirrored data s*y so that the extremum, half level and width are found the same way.
    const double s = dataLooksLikeDip(d_y) ? -1.0 : 1.0;
    auto sy = [&](size_t k) { return s * d_y[k]; };
    size_t lowIdx = 0, topIdx = 0;
    for (size_t k = 1; k < d_y.size(); ++k) {
        if (sy(k) < sy(lowIdx))
            lowIdx = k;
        if (sy(k) > sy(topIdx))
            topIdx = k;
    }
    const double xSpan = std::abs(d_x.back() - d_x.front());
    const double xFloorScale = (xSpan > 0.0) ? std::min(1.0, xSpan) : 1.0;
    const double rawSpan = sy(topIdx) - sy(lowIdx);
    const double ySpan = (rawSpan > 0.0) ? rawSpan : 1.0;
    double A = std::max(1e-9 * std::min(1.0, ySpan), rawSpan);
    double y0 = d_y[lowIdx];
    size_t maxIdx = topIdx;
    double xc = d_x[maxIdx];
    double halfY = sy(lowIdx) + 0.5 * A;
    double xLeft = d_x.front(), xRight = d_x.back();
    for (int k = int(maxIdx); k > 0; --k) {
        if (sy(size_t(k - 1)) <= halfY) {
            xLeft = d_x[k - 1];
            break;
        }
    }
    for (size_t k = maxIdx; k + 1 < d_y.size(); ++k) {
        if (sy(k + 1) <= halfY) {
            xRight = d_x[k + 1];
            break;
        }
    }
    double fwhm = std::max(1e-6 * xFloorScale, xRight - xLeft);
    double sigma = fwhm / (2.0 * std::sqrt(2.0 * M_LN2));
    A *= s;

    gsl_vector_set(d_param_init, 0, y0);
    gsl_vector_set(d_param_init, 1, A);
    gsl_vector_set(d_param_init, 2, xc);
    gsl_vector_set(d_param_init, 3, sigma);
    d_initial_guesses_set = true;
}

bool GaussAmpFit::calculateFitCurveData(const vector<double> &par, std::vector<double> &X,
                                        std::vector<double> &Y)
{
    generateX(X);
    double w2 = par[3] * par[3];
    for (int i = 0; i < d_points; i++) {
        double diff = X[i] - par[2];
        Y[i] = par[1] * exp(-0.5 * diff * diff / w2) + par[0];
    }
    return true;
}
