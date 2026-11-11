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
#include "fit_gsl.h"
#include "PlotCurve.h"
#include "FunctionCurve.h"
#include "ColorButton.h"
#include "Table.h"
#include "core/column/Column.h"

#include <QLocale>
#include <QMessageBox>
#include <algorithm>
#include <cmath>
#include <numeric>

using namespace std;

MultiPeakFit::MultiPeakFit(ApplicationWindow *parent, Graph *g, PeakProfile profile, int peaks)
    : Fit(parent, g), d_profile(profile)
{
    setObjectName(tr("MultiPeak"));

    d_param_init = NULL;
    covar = NULL;
    d_peaks = 0;

    updateProfileFunctions();
    setNumPeaks(peaks);

    generate_peak_curves = true;
    d_peaks_color = ColorButton::color(2); // green
}

void MultiPeakFit::updateProfileFunctions()
{
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
    }
}

void MultiPeakFit::setPeakProfile(PeakProfile profile)
{
    d_profile = profile;
    updateProfileFunctions();
    setNumPeaks(d_peaks > 0 ? d_peaks : 1);
}

void MultiPeakFit::setLorentzFraction(double mu)
{
    d_voigt_mu = std::max(0.0, std::min(1.0, mu));
}

void MultiPeakFit::setPearsonShape(double m)
{
    d_pearson_m = std::max(0.55, m);
}

void MultiPeakFit::setNumPeaks(int n)
{
    d_peaks = std::max(1, n);
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
    }
    if (d_peaks > 1)
        d_explanation += "(" + QString::number(d_peaks) + ") " + tr("multi-peak");

    d_p = 3 * d_peaks + 1;
    d_min_points = d_p;

    if (d_param_init)
        gsl_vector_free(d_param_init);
    d_param_init = gsl_vector_alloc(d_p);
    gsl_vector_set_all(d_param_init, 1.0);

    if (covar)
        gsl_matrix_free(covar);
    covar = gsl_matrix_alloc(d_p, d_p);
    d_results.resize(d_p);
    d_initial_guesses_set = false;

    d_param_names = generateParameterList(d_peaks);
    d_param_explain = generateExplanationList(d_peaks);
    d_formula = generateFormula(d_peaks, d_profile);
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
    if (peaks == 1)
        return QStringList() << tr("(amplitude)") << tr("(center)") << tr("(width)")
                             << tr("(offset)");

    QStringList lst;
    for (int i = 0; i < peaks; i++) {
        QString index = QString::number(i + 1);
        lst << tr("(amplitude %1)").arg(index);
        lst << tr("(center %1)").arg(index);
        lst << tr("(width %1)").arg(index);
    }
    lst << tr("(offset)");
    return lst;
}

QString MultiPeakFit::generateFormula(int peaks, PeakProfile profile)
{
    if (peaks == 1) {
        switch (profile) {
        case Gauss:
            return "y0+A*sqrt(2/PI)/w*exp(-2*((x-xc)/w)^2)";
        case Lorentz:
            return "y0+2*A/PI*w/(4*(x-xc)^2+w^2)";
        case GaussAmp:
            return "y0+A*exp(-2*((x-xc)/w)^2)";
        case PseudoVoigt:
            return "y0+A*(0.5*2/PI*w/(4*(x-xc)^2+w^2)+0.5*sqrt(4*ln(2)/PI)/w*exp(-4*ln(2)*((x-xc)/w)^2))";
        case PearsonVII:
            return "y0+A*(1+(sqrt(2)-1)*(2*(x-xc)/w)^2)^(-2)";
        }
    }

    QString formula = "y0+";
    for (int i = 0; i < peaks; i++) {
        formula += peakFormula(i + 1, profile);
        if (i < peaks - 1)
            formula += "+";
    }
    return formula;
}

QString MultiPeakFit::peakFormula(int peakIndex, PeakProfile profile)
{
    QString formula;
    QString index = QString::number(peakIndex);
    switch (profile) {
    case Gauss:
        formula += "sqrt(2/PI)*A" + index + "/w" + index;
        formula += "*exp(-2*(x-xc" + index + ")^2/w" + index + "^2)";
        break;
    case Lorentz:
        formula += "2*A" + index + "/PI*w" + index + "/(4*(x-xc" + index + ")^2+w" + index + "^2)";
        break;
    case GaussAmp:
        formula += "A" + index + "*exp(-2*(x-xc" + index + ")^2/w" + index + "^2)";
        break;
    case PseudoVoigt:
        formula += "A" + index + "*(0.5*2/PI*w" + index + "/(4*(x-xc" + index + ")^2+w" + index
                + "^2)+0.5*sqrt(4*ln(2)/PI)/w" + index + "*exp(-4*ln(2)*(x-xc" + index + ")^2/w"
                + index + "^2))";
        break;
    case PearsonVII:
        formula += "A" + index + "*(1+(sqrt(2)-1)*(2*(x-xc" + index + ")/w" + index + ")^2)^(-2)";
        break;
    }
    return formula;
}

int MultiPeakFit::estimatePeakCount(double minProminenceRatio) const
{
    const size_t n = d_x.size();
    if (n < 3 || d_y.size() != n)
        return std::max(1, d_peaks);

    std::vector<double> y_sorted = d_y;
    std::sort(y_sorted.begin(), y_sorted.end());
    double y0 = std::min(y_sorted[n / 10], 0.5 * (d_y.front() + d_y.back()));
    y0 = std::max(y_sorted.front(), y0);
    double y_max = y_sorted.back();
    double span = y_max - y0;
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
            if (h < minProminenceRatio * span)
                continue;

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
    const size_t n = d_x.size();
    if (n < 3 || d_y.size() != n)
        return false;

    int targetPeaks = std::max(1, d_peaks);

    // 1. Estimate baseline y0 from 10th percentile and endpoint average
    std::vector<double> y_sorted = d_y;
    std::sort(y_sorted.begin(), y_sorted.end());
    double y_min = y_sorted.front();
    double y_max = y_sorted.back();
    double y_p10 = y_sorted[std::min(n - 1, n / 10)];
    double y_ends = 0.5 * (d_y.front() + d_y.back());
    double y0 = std::max(y_min, std::min(y_p10, y_ends));
    double span = y_max - y0;
    if (span <= 0.0)
        span = std::max(1e-12, std::abs(y_max));

    // 2. Smooth d_y with a 3-point moving average to suppress noise spikes
    std::vector<double> y_smooth(n);
    y_smooth[0] = (2.0 * d_y[0] + d_y[1]) / 3.0;
    for (size_t i = 1; i + 1 < n; ++i)
        y_smooth[i] = (d_y[i - 1] + d_y[i] + d_y[i + 1]) / 3.0;
    y_smooth[n - 1] = (d_y[n - 2] + 2.0 * d_y[n - 1]) / 3.0;

    int minSep = std::max(2, int(n) / std::max(12, 5 * targetPeaks));
    std::vector<size_t> chosen;

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
        chosen.push_back(bestIdx);
    }

    // 3. Find local maxima candidates
    struct PeakCand
    {
        size_t idx;
        double prom;
        double height;
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
            if (h < minProminenceRatio * span)
                continue;

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
                cands.push_back({ rawIdx, prom, h });
        }
    }

    std::sort(cands.begin(), cands.end(), [](const PeakCand &a, const PeakCand &b) {
        if (std::abs(a.prom - b.prom) > 1e-12)
            return a.prom > b.prom;
        return a.height > b.height;
    });

    for (const auto &c : cands) {
        if (int(chosen.size()) >= targetPeaks)
            break;
        bool ok = true;
        for (size_t sel : chosen) {
            if (std::abs(int(c.idx) - int(sel)) < minSep) {
                ok = false;
                break;
            }
        }
        if (ok)
            chosen.push_back(c.idx);
    }

    // 4. Fallback if fewer than targetPeaks were found
    std::vector<double> residual(n);
    for (size_t i = 0; i < n; ++i)
        residual[i] = std::max(0.0, d_y[i] - y0);

    double defaultSigmaIdx = std::max(2.0, double(n) / (4.0 * targetPeaks));
    for (size_t sel : chosen) {
        double h = residual[sel];
        for (size_t i = 0; i < n; ++i) {
            double d = (double(i) - double(sel)) / defaultSigmaIdx;
            residual[i] = std::max(0.0, residual[i] - h * std::exp(-0.5 * d * d));
        }
    }

    while (int(chosen.size()) < targetPeaks) {
        size_t bestIdx = 0;
        double bestVal = -1.0;
        for (size_t i = 1; i + 1 < n; ++i) {
            bool far_enough = true;
            for (size_t sel : chosen) {
                if (std::abs(int(i) - int(sel)) < minSep) {
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
        chosen.push_back(bestIdx);
        double h = residual[bestIdx];
        for (size_t i = 0; i < n; ++i) {
            double d = (double(i) - double(bestIdx)) / defaultSigmaIdx;
            residual[i] = std::max(0.0, residual[i] - h * std::exp(-0.5 * d * d));
        }
    }

    // Sort chosen peaks by xc ascending
    std::sort(chosen.begin(), chosen.end(),
              [this](size_t a, size_t b) { return d_x[a] < d_x[b]; });

    double xSpan = std::abs(d_x.back() - d_x.front());
    if (xSpan <= 0.0)
        xSpan = 1.0;
    double minW = std::max(1e-9, (xSpan / double(n)) * 1.2);
    double maxW = xSpan * 0.8;

    // 5. Estimate each peak's center xc, FWHM, and profile parameters (A_i, xc_i, w_i)
    for (int m = 0; m < targetPeaks; ++m) {
        size_t idx = chosen[m];
        double xc = d_x[idx];
        if (idx > 0 && idx + 1 < n) {
            double ym1 = d_y[idx - 1];
            double y0p = d_y[idx];
            double yp1 = d_y[idx + 1];
            double denom = 2.0 * (2.0 * y0p - ym1 - yp1);
            if (std::abs(denom) > 1e-12 && y0p >= ym1 && y0p >= yp1) {
                double delta = std::max(-0.5, std::min(0.5, (ym1 - yp1) / denom));
                xc = d_x[idx] + delta * 0.5 * (d_x[idx + 1] - d_x[idx - 1]);
            }
        }

        double H = std::max(1e-6 * span, d_y[idx] - y0);
        double halfLevel = y0 + 0.5 * H;

        size_t leftLimit = 0;
        if (m > 0) {
            size_t prevIdx = chosen[m - 1];
            leftLimit = prevIdx;
            double minValley = d_y[idx];
            for (size_t k = prevIdx; k <= idx; ++k) {
                if (d_y[k] < minValley) {
                    minValley = d_y[k];
                    leftLimit = k;
                }
            }
        }

        size_t rightLimit = n - 1;
        if (m + 1 < targetPeaks) {
            size_t nextIdx = chosen[m + 1];
            rightLimit = nextIdx;
            double minValley = d_y[idx];
            for (size_t k = idx; k <= nextIdx; ++k) {
                if (d_y[k] < minValley) {
                    minValley = d_y[k];
                    rightLimit = k;
                }
            }
        }

        bool foundLeft = false;
        double xLeft = d_x[leftLimit];
        for (int k = int(idx); k > int(leftLimit); --k) {
            if (d_y[k - 1] <= halfLevel) {
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
            if (d_y[k + 1] <= halfLevel) {
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
            fwhm = std::max(minW, std::min(xc - xLeft, xRight - xc) * 1.2);

        fwhm = std::max(minW, std::min(maxW, fwhm));

        double A_init = 1.0;
        double w_init = fwhm;

        switch (d_profile) {
        case Gauss:
            w_init = fwhm / std::sqrt(2.0 * M_LN2);
            A_init = H * w_init * std::sqrt(M_PI / 2.0);
            break;
        case Lorentz:
            w_init = fwhm;
            // lorentz_multi_peak_f uses internal parameter a_raw = (2/pi)*Area = H * w
            A_init = H * w_init;
            break;
        case GaussAmp:
            w_init = fwhm / std::sqrt(2.0 * M_LN2);
            A_init = H;
            break;
        case PseudoVoigt: {
            w_init = fwhm;
            double cG = std::sqrt(4.0 * M_LN2 / M_PI);
            double cPeak = d_voigt_mu * M_2_PI + (1.0 - d_voigt_mu) * cG;
            A_init = (H * w_init) / std::max(1e-12, cPeak);
            break;
        }
        case PearsonVII:
            w_init = fwhm;
            A_init = H;
            break;
        }

        gsl_vector_set(d_param_init, 3 * m, A_init);
        gsl_vector_set(d_param_init, 3 * m + 1, xc);
        gsl_vector_set(d_param_init, 3 * m + 2, w_init);
    }

    gsl_vector_set(d_param_init, 3 * targetPeaks, y0);
    d_initial_guesses_set = true;
    return true;
}

void MultiPeakFit::guessInitialValues()
{
    autoDetectPeaks(d_peaks);
}

void MultiPeakFit::storeCustomFitResults(const vector<double> &par)
{
    d_results = par;

    for (int j = 0; j < d_peaks && unsigned(3 * j + 2) < d_results.size(); j++) {
        d_results[3 * j + 2] = std::abs(d_results[3 * j + 2]);
        if (d_profile == Lorentz)
            d_results[3 * j] = M_PI_2 * d_results[3 * j];
        d_results[3 * j] = std::abs(d_results[3 * j]);
    }

    d_last_results = d_results;
}

double MultiPeakFit::evalPeakAt(const std::vector<double> &par, int peakIndex, double x,
                                bool isStoredResults) const
{
    double A = par[3 * peakIndex];
    double xc = par[3 * peakIndex + 1];
    double w = std::abs(par[3 * peakIndex + 2]);
    if (w < 1e-15)
        w = 1e-15;
    double diff = x - xc;

    switch (d_profile) {
    case Gauss:
        return std::sqrt(M_2_PI) * std::abs(A) / w * std::exp(-2.0 * diff * diff / (w * w));
    case Lorentz: {
        double a_raw = isStoredResults ? (M_2_PI * std::abs(A)) : std::abs(A);
        return a_raw * w / (4.0 * diff * diff + w * w);
    }
    case GaussAmp:
        return A * std::exp(-2.0 * diff * diff / (w * w));
    case PseudoVoigt: {
        const double kG = 4.0 * M_LN2;
        const double cG = std::sqrt(kG / M_PI);
        const double cL = 2.0 / M_PI;
        double L = cL * w / (4.0 * diff * diff + w * w);
        double G = (cG / w) * std::exp(-kG * diff * diff / (w * w));
        return std::abs(A) * (d_voigt_mu * L + (1.0 - d_voigt_mu) * G);
    }
    case PearsonVII: {
        double m = std::max(0.55, d_pearson_m);
        double cm = std::pow(2.0, 1.0 / m) - 1.0;
        double u = 2.0 * diff / w;
        return A * std::pow(1.0 + cm * u * u, -m);
    }
    }
    return 0.0;
}

bool MultiPeakFit::calculateFitCurveData(const std::vector<double> &par, std::vector<double> &X,
                                         std::vector<double> &Y)
{
    if (par.size() < size_t(3 * d_peaks + 1))
        return false;
    generateX(X);
    double y0 = par[d_p - 1];
    for (int i = 0; i < d_points; ++i) {
        double yi = y0;
        for (int j = 0; j < d_peaks; ++j)
            yi += evalPeakAt(par, j, X[i], true);
        Y[i] = yi;
    }
    return true;
}

void MultiPeakFit::insertPeakFunctionCurve(std::vector<double> &x, std::vector<double> &y, int peak)
{
    QStringList curves = d_graph->curvesList();
    int index = 0;
    for (int i = 0; i < (int)curves.count(); i++) {
        if (curves[i].startsWith(tr("Peak")))
            index++;
    }
    QString title = tr("Peak") + QString::number(++index);

    FunctionCurve *c =
            new FunctionCurve((ApplicationWindow *)parent(), FunctionCurve::Normal, title);
    c->setPen(QPen(d_peaks_color, 1));
    c->setData(x.data(), y.data(), d_points);
    c->setRange(d_x.front(), d_x.back());

    QString formula;
    for (int j = 0; j < 3; j++) {
        int p = 3 * peak + j;
        formula += QString("%1=%2\n").arg(d_param_names[p]).arg(d_results[p], 0, 'g', d_prec);
    }
    formula += QString("%1=%2\n\ny0+%3")
                       .arg(d_param_names[d_p - 1])
                       .arg(d_results[d_p - 1], 1, 'g', d_prec)
                       .arg(peakFormula(peak + 1, d_profile));
    c->setFormula(formula);
    d_graph->insertPlotItem(c, Graph::Line);
    d_graph->addFitCurve(c);
}

void MultiPeakFit::generateFitCurve(const vector<double> &par)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (!d_gen_function)
        d_points = d_x.size();

    gsl_matrix *m = gsl_matrix_alloc(d_points, d_peaks);
    if (!m) {
        QMessageBox::warning(app, tr("Fit Error"),
                             tr("Could not allocate enough memory for the fit curves!"));
        return;
    }

    std::vector<double> X(d_points);
    std::vector<double> Y(d_points);
    int i, j;
    int peaks_aux = generate_peak_curves ? d_peaks : 0;

    if (d_gen_function) {
        double step = (d_x.back() - d_x.front()) / (d_points - 1);
        for (i = 0; i < d_points; i++) {
            X[i] = d_x[0] + i * step;
            double yi = 0;
            for (j = 0; j < d_peaks; j++) {
                double y_aux = evalPeakAt(par, j, X[i], false);
                yi += y_aux;
                y_aux += par[d_p - 1];
                gsl_matrix_set(m, i, j, y_aux);
            }
            Y[i] = yi + par[d_p - 1]; // add offset
        }

        if (d_peaks > 1)
            insertFitFunctionCurve(objectName() + tr("Fit"), X, Y, 2);
        else
            insertFitFunctionCurve(objectName() + tr("Fit"), X, Y);

        if (generate_peak_curves) {
            for (i = 0; i < peaks_aux; i++) { // add the peak curves
                for (j = 0; j < d_points; j++)
                    Y[j] = gsl_matrix_get(m, j, i);

                insertPeakFunctionCurve(X, Y, i);
            }
        }
    } else {
        QString tableName = app->generateUniqueName(tr("Fit"));
        QString label = d_explanation + " " + tr("fit of") + " " + d_curve->title().text();

        QList<Column *> columns;
        columns << new Column(tr("1", "multipeak fit table first column name"),
                              SciDAVis::ColumnMode::Numeric);
        for (i = 0; i < peaks_aux; i++)
            columns << new Column(tr("peak%1").arg(QString::number(i + 1)),
                                  SciDAVis::ColumnMode::Numeric);
        columns << new Column(tr("2", "multipeak fit table last column name"),
                              SciDAVis::ColumnMode::Numeric);
        Table *t = app->newHiddenTable(tableName, label, columns);

        for (i = 0; i < d_points; i++) {
            X[i] = d_x[i];
            columns.at(0)->setValueAt(i, X[i]);

            double yi = 0;
            for (j = 0; j < d_peaks; j++) {
                double y_aux = evalPeakAt(par, j, X[i], false);
                yi += y_aux;
                y_aux += par[d_p - 1];
                if (j < peaks_aux)
                    columns.at(j + 1)->setValueAt(i, y_aux);
                gsl_matrix_set(m, i, j, y_aux);
            }
            Y[i] = yi + par[d_p - 1]; // add offset
            columns.at(peaks_aux + 1)->setValueAt(i, Y[i]);
        }

        label = tableName + "_2";
        DataCurve *c = new DataCurve(t, tableName + "_" + columns.at(0)->name(), label);
        if (d_peaks > 1)
            c->setPen(QPen(d_curveColor, 2));
        else
            c->setPen(QPen(d_curveColor, 1));
        c->setData(X.data(), Y.data(), d_points);
        d_graph->insertPlotItem(c, Graph::Line);
        d_graph->addFitCurve(c);

        if (generate_peak_curves) {
            for (i = 0; i < peaks_aux; i++) { // add the peak curves
                for (j = 0; j < d_points; j++)
                    Y[j] = gsl_matrix_get(m, j, i);

                label = tableName + "_" + tr("peak") + QString::number(i + 1);
                c = new DataCurve(t, tableName + "_" + columns.at(0)->name(), label);
                c->setPen(QPen(d_peaks_color, 1));
                c->setData(X.data(), Y.data(), d_points);
                d_graph->insertPlotItem(c, Graph::Line);
                d_graph->addFitCurve(c);
            }
        }
    }
    d_graph->replot();

    gsl_matrix_free(m);
}

std::vector<MultiPeakFit::PeakSummaryRow>
MultiPeakFit::computePeakSummary(const std::vector<double> &res) const
{
    std::vector<PeakSummaryRow> rows;
    if (res.size() < size_t(3 * d_peaks + 1))
        return rows;

    rows.resize(d_peaks);
    double y0 = res[3 * d_peaks];
    double totalArea = 0.0;

    for (int j = 0; j < d_peaks; ++j) {
        double rawA = std::abs(res[3 * j]);
        double xc = res[3 * j + 1];
        double w = std::abs(res[3 * j + 2]);
        if (w < 1e-15)
            w = 1e-15;

        double fwhm = w;
        double height = 0.0;
        double area = rawA;

        switch (d_profile) {
        case Gauss:
            area = rawA;
            fwhm = w * std::sqrt(2.0 * M_LN2);
            height = std::sqrt(M_2_PI) * rawA / w;
            break;
        case Lorentz:
            area = rawA;
            fwhm = w;
            height = M_2_PI * rawA / w;
            break;
        case GaussAmp:
            height = rawA;
            fwhm = w * std::sqrt(2.0 * M_LN2);
            area = rawA * w * std::sqrt(M_PI / 2.0);
            break;
        case PseudoVoigt: {
            area = rawA;
            fwhm = w;
            double cG = std::sqrt(4.0 * M_LN2 / M_PI);
            height = (rawA / w) * (d_voigt_mu * M_2_PI + (1.0 - d_voigt_mu) * cG);
            break;
        }
        case PearsonVII: {
            height = rawA;
            fwhm = w;
            double m = std::max(0.55, d_pearson_m);
            double cm = std::pow(2.0, 1.0 / m) - 1.0;
            area = rawA * w * (std::sqrt(M_PI) * std::tgamma(m - 0.5))
                    / (2.0 * std::sqrt(cm) * std::tgamma(m));
            break;
        }
        }

        rows[j].index = j + 1;
        rows[j].center = xc;
        rows[j].fwhm = fwhm;
        rows[j].height = height;
        rows[j].area = area;
        rows[j].areaPercent = 0.0;
        rows[j].baseline = y0;
        totalArea += area;
    }

    for (int j = 0; j < d_peaks; ++j) {
        rows[j].areaPercent =
                (totalArea > 1e-15) ? (100.0 * rows[j].area / totalArea) : (100.0 / d_peaks);
    }

    return rows;
}

QString MultiPeakFit::logFitInfo(const vector<double> &par, int iterations, int status,
                                 const QString &plotName)
{
    QString info = Fit::logFitInfo(par, iterations, status, plotName);

    auto rows = computePeakSummary(par);
    if (rows.empty())
        return info;

    info += tr("Peak #") + "\t" + tr("Center (xc)") + "\t" + tr("FWHM") + "\t" + tr("Height") + "\t"
            + tr("Area") + "\t" + tr("Area (%)") + "\t" + tr("Baseline (y0)") + "\n";
    info += "--------------------------------------------------------------------------------------"
            "-\n";
    for (const auto &r : rows) {
        info += QString::number(r.index) + "\t";
        info += QLocale().toString(r.center, 'g', d_prec) + "\t";
        info += QLocale().toString(r.fwhm, 'g', d_prec) + "\t";
        info += QLocale().toString(r.height, 'g', d_prec) + "\t";
        info += QLocale().toString(r.area, 'g', d_prec) + "\t";
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
    Table *t = app->newTable(name, d_peaks, 8);
    if (!t)
        return nullptr;

    t->setHeader(QStringList() << tr("Peak") << tr("Center") << tr("FWHM") << tr("Height")
                               << tr("Area") << tr("AreaPercent") << tr("y0") << tr("COD_R2"));
    for (int col = 0; col < 8; ++col)
        t->column(col)->setColumnMode(SciDAVis::ColumnMode::Numeric);

    double r2 = rSquare();
    for (int i = 0; i < d_peaks && i < int(rows.size()); ++i) {
        t->column(0)->setValueAt(i, rows[i].index);
        t->column(1)->setValueAt(i, rows[i].center);
        t->column(2)->setValueAt(i, rows[i].fwhm);
        t->column(3)->setValueAt(i, rows[i].height);
        t->column(4)->setValueAt(i, rows[i].area);
        t->column(5)->setValueAt(i, rows[i].areaPercent);
        t->column(6)->setValueAt(i, rows[i].baseline);
        t->column(7)->setValueAt(i, r2);
    }

    t->showNormal();
    return t;
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

    double y0 = *std::min_element(d_y.begin(), d_y.end());
    auto maxIt = std::max_element(d_y.begin(), d_y.end());
    size_t maxIdx = size_t(std::distance(d_y.begin(), maxIt));
    double A = std::max(1e-9, *maxIt - y0);
    double xc = d_x[maxIdx];
    double halfY = y0 + 0.5 * A;
    double xLeft = d_x.front(), xRight = d_x.back();
    for (int k = int(maxIdx); k > 0; --k) {
        if (d_y[k - 1] <= halfY) {
            xLeft = d_x[k - 1];
            break;
        }
    }
    for (size_t k = maxIdx; k + 1 < d_y.size(); ++k) {
        if (d_y[k + 1] <= halfY) {
            xRight = d_x[k + 1];
            break;
        }
    }
    double fwhm = std::max(1e-6, xRight - xLeft);
    double sigma = fwhm / (2.0 * std::sqrt(2.0 * M_LN2));

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
