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
    if (d_profile == DoniachSunjic)
        d_asym_alpha = 0.15;

    updateProfileFunctions();
    setNumPeaks(peaks);

    generate_peak_curves = true;
    d_peaks_color = QColor("#2ca02c"); // publication green
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
    if (d_profile == DoniachSunjic && d_asym_alpha >= 0.85)
        d_asym_alpha = 0.15;
    else if (d_profile != DoniachSunjic && d_asym_alpha < 0.3)
        d_asym_alpha = 1.5;
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

void MultiPeakFit::setAsymmetryFactor(double alpha)
{
    if (d_profile == DoniachSunjic)
        d_asym_alpha = std::max(0.0, std::min(0.85, alpha));
    else
        d_asym_alpha = std::max(0.05, alpha);
}

void MultiPeakFit::setTailFactor(double tau)
{
    d_tail_tau = std::max(1e-4, tau);
}

void MultiPeakFit::setBaselineMode(BaselineMode mode)
{
    d_baseline_mode = mode;
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
        case Voigt:
            return "y0+A*(0.5924*2/PI*w/(4*(x-xc)^2+w^2)+0.4076*sqrt(4*ln(2)/PI)/w*exp(-4*ln(2)*((x-xc)/w)^2))";
        case BiGaussian:
            return "y0+((x<xc)?(A*exp(-ln(2)*((x-xc)/(0.4*w))^2)):(A*exp(-ln(2)*((x-xc)/(0.6*w))^2)))";
        case EMG:
            return "y0+(A/(2*1.2))*exp((0.5*w/1.2)^2/2-(x-xc)/1.2)*erfc((0.5*w/1.2-(x-xc)/(0.5*w))/sqrt(2))";
        case LogNormal:
            return "y0+((1+3*(x-xc)/w>0)?(A*exp(-ln(2)*(ln(1+3*(x-xc)/w)/ln(1.5+sqrt(3.25)))^2)):0)";
        case Asym2Sig:
            return "y0+A*(1/(1+exp(-(x-xc+w/2)/(0.1*w))))*(1-1/(1+exp(-(x-xc-w/2)/(0.15*w))))";
        case DoniachSunjic:
            return "y0+A*cos(PI*0.15/2+(1-0.15)*atan(2*(x-xc)/w))/(1+4*((x-xc)/w)^2)^((1-0.15)/2)";
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
    case Voigt:
        formula += "A" + index + "*(0.5924*2/PI*w" + index + "/(4*(x-xc" + index + ")^2+w" + index
                + "^2)+0.4076*sqrt(4*ln(2)/PI)/w" + index + "*exp(-4*ln(2)*(x-xc" + index + ")^2/w"
                + index + "^2))";
        break;
    case BiGaussian:
        formula += "((x<xc" + index + ")?(A" + index + "*exp(-ln(2)*((x-xc" + index + ")/(0.4*w"
                + index + "))^2)):(A" + index + "*exp(-ln(2)*((x-xc" + index + ")/(0.6*w" + index
                + "))^2)))";
        break;
    case EMG:
        formula += "(A" + index + "/(2*1.2))*exp((0.5*w" + index + "/1.2)^2/2-(x-xc" + index
                + ")/1.2)*erfc((0.5*w" + index + "/1.2-(x-xc" + index + ")/(0.5*w" + index
                + "))/sqrt(2))";
        break;
    case LogNormal:
        formula += "((1+3*(x-xc" + index + ")/w" + index + ">0)?(A" + index
                + "*exp(-ln(2)*(ln(1+3*(x-xc" + index + ")/w" + index
                + ")/ln(1.5+sqrt(3.25)))^2)):0)";
        break;
    case Asym2Sig:
        formula += "A" + index + "*(1/(1+exp(-(x-xc" + index + "+w" + index + "/2)/(0.1*w" + index
                + "))))*(1-1/(1+exp(-(x-xc" + index + "-w" + index + "/2)/(0.15*w" + index + "))))";
        break;
    case DoniachSunjic:
        formula += "A" + index + "*cos(PI*0.15/2+(1-0.15)*atan(2*(x-xc" + index + ")/w" + index
                + "))/(1+4*((x-xc" + index + ")/w" + index + ")^2)^((1-0.15)/2)";
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
    double y0 = (d_baseline_subtracted || d_baseline_mode == BaselineZero)
            ? 0.0
            : std::max(y_min, std::min(y_p10, y_ends));
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
        case Voigt: {
            w_init = fwhm;
            double d = std::max(0.0, std::min(1.0, d_voigt_mu));
            double eta = std::max(0.0, std::min(1.0, 1.36603 * d - 0.47719 * d * d + 0.11116 * d * d * d));
            double cG = std::sqrt(4.0 * M_LN2 / M_PI);
            double cPeak = eta * M_2_PI + (1.0 - eta) * cG;
            A_init = (H * w_init) / std::max(1e-12, cPeak);
            break;
        }
        case BiGaussian:
        case LogNormal:
            w_init = fwhm;
            A_init = H;
            break;
        case EMG: {
            w_init = std::max(minW, fwhm / std::sqrt(2.0 * M_LN2));
            for (int iter = 0; iter < 2; ++iter) {
                double bestH = 1e-12, bestDx = 0.0;
                double spanSearch = 3.0 * w_init + 4.0 * d_tail_tau;
                const int nGrid = 121;
                std::vector<double> gx(nGrid), gy(nGrid);
                int maxG = 0;
                for (int gi = 0; gi < nGrid; ++gi) {
                    gx[gi] = -2.0 * w_init + (spanSearch + 2.0 * w_init) * double(gi) / double(nGrid - 1);
                    gy[gi] = eval_origin_peak_profile(EMG, 1.0, 0.0, w_init, gx[gi],
                                                      d_voigt_mu, d_pearson_m, d_asym_alpha, d_tail_tau);
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
                    if (fwhm1 > 1e-9)
                        w_init = std::max(minW, std::min(maxW, w_init * (fwhm / fwhm1)));
                } else {
                    xc -= bestDx;
                    A_init = H / std::max(1e-12, bestH);
                }
            }
            break;
        }
        case Asym2Sig: {
            w_init = fwhm;
            double bestH = 1e-12, bestDx = 0.0;
            for (int gi = -40; gi <= 40; ++gi) {
                double dx = (double(gi) / 40.0) * w_init;
                double v = eval_origin_peak_profile(Asym2Sig, 1.0, 0.0, w_init, dx,
                                                    d_voigt_mu, d_pearson_m, d_asym_alpha, d_tail_tau);
                if (v > bestH) {
                    bestH = v;
                    bestDx = dx;
                }
            }
            xc -= bestDx;
            A_init = H / std::max(1e-12, bestH);
            break;
        }
        case DoniachSunjic: {
            w_init = fwhm;
            double bestH = 1e-12, bestDx = 0.0;
            for (int gi = -40; gi <= 40; ++gi) {
                double dx = (double(gi) / 40.0) * w_init;
                double v = eval_origin_peak_profile(DoniachSunjic, 1.0, 0.0, w_init, dx,
                                                    d_voigt_mu, d_pearson_m, d_asym_alpha, d_tail_tau);
                if (v > bestH) {
                    bestH = v;
                    bestDx = dx;
                }
            }
            xc -= bestDx;
            A_init = H / std::max(1e-12, bestH);
            break;
        }
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
    case Voigt:
    case BiGaussian:
    case EMG:
    case LogNormal:
    case Asym2Sig:
    case DoniachSunjic:
        return eval_origin_peak_profile(int(d_profile), std::abs(A), xc, w, x,
                                        d_voigt_mu, d_pearson_m, d_asym_alpha, d_tail_tau);
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
    c->setPen(QPen(d_peaks_color, 2, Qt::DashLine));
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
        case Voigt: {
            area = rawA;
            fwhm = w;
            double d = std::max(0.0, std::min(1.0, d_voigt_mu));
            double eta = std::max(0.0, std::min(1.0, 1.36603 * d - 0.47719 * d * d + 0.11116 * d * d * d));
            double cG = std::sqrt(4.0 * M_LN2 / M_PI);
            height = (rawA / w) * (eta * M_2_PI + (1.0 - eta) * cG);
            break;
        }
        case BiGaussian:
            height = rawA;
            fwhm = w;
            area = 0.5 * std::sqrt(M_PI / M_LN2) * rawA * w;
            break;
        case EMG: {
            area = rawA;
            double spanSearch = 3.0 * w + 4.0 * d_tail_tau;
            const int nGrid = 121;
            std::vector<double> gx(nGrid), gy(nGrid);
            int maxG = 0;
            height = 0.0;
            for (int gi = 0; gi < nGrid; ++gi) {
                gx[gi] = -2.0 * w + (spanSearch + 2.0 * w) * double(gi) / double(nGrid - 1);
                gy[gi] = eval_origin_peak_profile(EMG, rawA, 0.0, w, gx[gi],
                                                  d_voigt_mu, d_pearson_m, d_asym_alpha, d_tail_tau);
                if (gy[gi] > height) {
                    height = gy[gi];
                    maxG = gi;
                }
            }
            if (height > 1e-15) {
                double halfH = 0.5 * height;
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
                fwhm = std::max(1e-12, gr - gl);
            } else {
                fwhm = w * std::sqrt(2.0 * M_LN2);
            }
            break;
        }
        case LogNormal: {
            height = rawA;
            fwhm = w;
            double asym = (std::abs(d_asym_alpha) > 1e-4) ? std::abs(d_asym_alpha) : 1.5;
            double b = std::log(asym + std::sqrt(asym * asym + 1.0));
            area = rawA * w * std::sqrt(M_PI / (4.0 * M_LN2)) * (b / asym)
                    * std::exp((b * b) / (4.0 * M_LN2));
            break;
        }
        case Asym2Sig:
        case DoniachSunjic: {
            fwhm = w;
            height = 0.0;
            area = 0.0;
            const int nGrid = 161;
            double x0_grid = -4.0 * w;
            double step_g = (8.0 * w) / double(nGrid - 1);
            double prev_v = 0.0;
            for (int gi = 0; gi < nGrid; ++gi) {
                double dx = x0_grid + gi * step_g;
                double v = std::max(0.0, eval_origin_peak_profile(int(d_profile), rawA, 0.0, w, dx,
                                                                  d_voigt_mu, d_pearson_m, d_asym_alpha, d_tail_tau));
                if (v > height)
                    height = v;
                if (gi > 0)
                    area += 0.5 * (prev_v + v) * step_g;
                prev_v = v;
            }
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

std::vector<MultiPeakFit::PeakSummaryRow> MultiPeakFit::initialPeakSummary() const
{
    if (!d_initial_guesses_set && !d_x.empty())
        const_cast<MultiPeakFit *>(this)->autoDetectPeaks(d_peaks);
    if (!d_param_init || d_p < unsigned(3 * d_peaks + 1))
        return std::vector<PeakSummaryRow>();
    std::vector<double> initVec(d_p);
    for (unsigned i = 0; i < d_p; ++i)
        initVec[i] = gsl_vector_get(d_param_init, i);
    if (d_profile == Lorentz) {
        for (int j = 0; j < d_peaks; ++j)
            initVec[3 * j] *= M_PI_2;
    }
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
        std::vector<double> y_sorted = y;
        std::sort(y_sorted.begin(), y_sorted.end());
        double y_min = y_sorted.front();
        double y_p10 = y_sorted[std::min(n - 1, n / 10)];
        double y_ends = 0.5 * (y.front() + y.back());
        double y0 = std::max(y_min, std::min(y_p10, y_ends));
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
        // Initial mild 3-point noise suppression
        base = y;
        if (n >= 3) {
            for (size_t i = 1; i + 1 < n; ++i)
                base[i] = 0.25 * y[i - 1] + 0.5 * y[i] + 0.25 * y[i + 1];
        }
        // Iterative SNIP / moving-chord minimum envelope
        int maxW = std::max(3, int(n) / 4);
        std::vector<double> tmp = base;
        for (int p = maxW; p >= 1; --p) {
            for (size_t i = size_t(p); i + size_t(p) < n; ++i) {
                double chord = 0.5 * (base[i - p] + base[i + p]);
                tmp[i] = std::min(base[i], chord);
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
    c->setFormula(tr("Baseline"));
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

    std::vector<double> y_net(n, 0.0);
    for (size_t i = 0; i < n; ++i)
        y_net[i] = std::max(0.0, d_y[i] - base[i]);

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

        rows[m] = { m + 1, xc, maxH, fwhm, area, 0.0, centroid };
        totalArea += area;
    }

    for (int m = 0; m < d_peaks; ++m)
        rows[m].areaPct =
                (totalArea > 1e-15) ? (100.0 * rows[m].area / totalArea) : (100.0 / d_peaks);

    QString name = tableName.isEmpty() ? app->generateUniqueName(tr("PeakIntegration"))
                                       : app->generateUniqueName(tableName, false);
    Table *t = app->newTable(name, d_peaks, 7);
    if (!t)
        return nullptr;

    t->setHeader(QStringList() << tr("Peak") << tr("Center") << tr("Height") << tr("FWHM")
                               << tr("IntegratedArea") << tr("AreaPercent") << tr("Centroid"));
    for (int col = 0; col < 7; ++col)
        t->column(col)->setColumnMode(SciDAVis::ColumnMode::Numeric);

    for (int m = 0; m < d_peaks; ++m) {
        t->column(0)->setValueAt(m, rows[m].index);
        t->column(1)->setValueAt(m, rows[m].center);
        t->column(2)->setValueAt(m, rows[m].height);
        t->column(3)->setValueAt(m, rows[m].fwhm);
        t->column(4)->setValueAt(m, rows[m].area);
        t->column(5)->setValueAt(m, rows[m].areaPct);
        t->column(6)->setValueAt(m, rows[m].centroid);
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
