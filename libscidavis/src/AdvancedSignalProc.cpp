/***************************************************************************
    File                 : AdvancedSignalProc.cpp
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Description          : OriginPro Advanced Signal Processing:
                           IIR Digital Filters (Butterworth/Chebyshev I/Bessel,
                           Zero-Phase filtfilt), STFT Spectrogram, Hilbert
                           Envelope, Wavelet CWT & DWT VisuShrink Denoising,
                           2D Matrix FFT/IFFT, Welch PSD & Cross-Coherence
 ***************************************************************************/

#include "AdvancedSignalProc.h"

#include "ApplicationWindow.h"
#include "Graph.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "Table.h"
#include "core/column/Column.h"

#include <gsl/gsl_fft_complex.h>
#include <gsl/gsl_fft_halfcomplex.h>
#include <gsl/gsl_fft_real.h>
#include <gsl/gsl_poly.h>

#include <qwt_plot_curve.h>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numeric>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

using cdbl = std::complex<double>;

inline QString trSig(const char *text)
{
    return QCoreApplication::translate("AdvancedSignalProc", text);
}

QwtPlotCurve *resolveSignalCurve(Graph *g, const QString &curveTitle)
{
    if (!g || curveTitle.isEmpty())
        return nullptr;

    if (QwtPlotCurve *exact = g->curve(curveTitle))
        return exact;

    for (int i = 0; i < g->curves(); ++i) {
        QwtPlotCurve *c = g->curve(i);
        if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;
        const QString title = c->title().text();
        if (title == curveTitle || title.endsWith("_" + curveTitle))
            return c;
    }
    return nullptr;
}

bool extractUniformSignal(Graph *g, const QString &curveTitle,
                          std::vector<double> &tOut, std::vector<double> &yOut, double &fsOut)
{
    tOut.clear();
    yOut.clear();
    fsOut = 1.0;

    QwtPlotCurve *c = resolveSignalCurve(g, curveTitle);
    if (!c || c->dataSize() < 2)
        return false;

    DataCurve *dc = dynamic_cast<DataCurve *>(c);
    Table *tbl = dc ? dc->table() : nullptr;
    Column *xCol = (tbl && dc) ? tbl->column(dc->xColumnName()) : nullptr;
    Column *yCol = (tbl && dc) ? tbl->column(dc->yColumnName()) : nullptr;

    const int nRaw = c->dataSize();
    std::vector<std::pair<double, double>> pts;
    pts.reserve(static_cast<size_t>(nRaw));

    for (int i = 0; i < nRaw; ++i) {
        if (dc && tbl) {
            int row = dc->tableRow(i);
            if (row < 0 && yCol && nRaw == yCol->rowCount())
                row = i;
            if (row >= 0) {
                if ((xCol && xCol->isMasked(row)) || (yCol && yCol->isMasked(row)))
                    continue;
            }
        }
        const double tv = c->x(i);
        const double yv = c->y(i);
        if (std::isfinite(tv) && std::isfinite(yv))
            pts.emplace_back(tv, yv);
    }

    if (pts.size() < 2)
        return false;

    std::stable_sort(pts.begin(), pts.end(),
                     [](const std::pair<double, double> &a, const std::pair<double, double> &b) {
                         return a.first < b.first;
                     });

    // Merge duplicate timestamps if any
    const double rawSpan = pts.back().first - pts.front().first;
    if (rawSpan <= 0.0)
        return false;

    const double mergeEps = 1e-13 * std::max(1.0, rawSpan);
    std::vector<double> xs, ys;
    xs.reserve(pts.size());
    ys.reserve(pts.size());
    size_t idx = 0;
    while (idx < pts.size()) {
        double sx = pts[idx].first;
        double sy = pts[idx].second;
        int cnt = 1;
        size_t j = idx + 1;
        while (j < pts.size() && std::fabs(pts[j].first - pts[idx].first) <= mergeEps) {
            sx += pts[j].first;
            sy += pts[j].second;
            ++cnt;
            ++j;
        }
        xs.push_back(sx / cnt);
        ys.push_back(sy / cnt);
        idx = j;
    }

    const size_t n = xs.size();
    if (n < 2)
        return false;

    const double dt = (xs.back() - xs.front()) / static_cast<double>(n - 1);
    if (dt <= 0.0 || !std::isfinite(dt))
        return false;
    fsOut = 1.0 / dt;

    bool isUniform = true;
    for (size_t i = 0; i < n; ++i) {
        const double expected = xs.front() + static_cast<double>(i) * dt;
        if (std::fabs(xs[i] - expected) > 1e-3 * dt) {
            isUniform = false;
            break;
        }
    }

    if (isUniform) {
        tOut = std::move(xs);
        yOut = std::move(ys);
        return true;
    }

    // Linearly resample onto uniform grid
    tOut.resize(n);
    yOut.resize(n);
    size_t seg = 0;
    for (size_t i = 0; i < n; ++i) {
        const double tq = (i == n - 1) ? xs.back() : (xs.front() + static_cast<double>(i) * dt);
        tOut[i] = tq;
        while (seg + 1 < n && xs[seg + 1] < tq)
            ++seg;
        if (seg + 1 >= n) {
            yOut[i] = ys.back();
        } else {
            const double dx = xs[seg + 1] - xs[seg];
            const double alpha = (dx > 1e-300) ? ((tq - xs[seg]) / dx) : 0.0;
            yOut[i] = ys[seg] + alpha * (ys[seg + 1] - ys[seg]);
        }
    }
    return true;
}

QString uniqueWinName(ApplicationWindow *app, const QString &prefix)
{
    QString clean = prefix;
    clean.replace('_', "");
    QString name = app->generateUniqueName(clean);
    while (app->alreadyUsedName(name))
        name = app->generateUniqueName(clean);
    return name;
}

Table *buildSignalTable(ApplicationWindow *app, const QString &prefix,
                        const QStringList &colNames,
                        const QList<SciDAVis::PlotDesignation> &pds,
                        const std::vector<std::vector<double>> &cols)
{
    if (!app || cols.empty() || cols[0].empty())
        return nullptr;

    const int nRows = static_cast<int>(cols[0].size());
    const int nCols = static_cast<int>(cols.size());
    Table *t = app->newTable(uniqueWinName(app, prefix), nRows, nCols);
    if (!t)
        return nullptr;

    for (int c = 0; c < nCols; ++c) {
        QString label = (c < colNames.size()) ? colNames[c] : QString("Col%1").arg(c + 1);
        label.replace('_', '-');
        t->setColName(c, label);
        t->setColPlotDesignation(c, (c < pds.size()) ? pds[c]
                                                     : (c == 0 ? SciDAVis::X : SciDAVis::Y));
        if (Column *colPtr = t->column(c)) {
            QVector<qreal> buf(nRows);
            for (int r = 0; r < nRows; ++r)
                buf[r] = cols[static_cast<size_t>(c)][static_cast<size_t>(r)];
            colPtr->replaceValues(0, buf);
        } else {
            for (int r = 0; r < nRows; ++r)
                t->setCell(r, c, cols[static_cast<size_t>(c)][static_cast<size_t>(r)]);
        }
    }
    return t;
}

void runComplexFft1D(std::vector<double> &interleaved, size_t n, bool inverse)
{
    if (n <= 1)
        return;
    gsl_fft_complex_wavetable *wt = gsl_fft_complex_wavetable_alloc(n);
    gsl_fft_complex_workspace *ws = gsl_fft_complex_workspace_alloc(n);
    if (wt && ws) {
        if (inverse)
            gsl_fft_complex_inverse(interleaved.data(), 1, n, wt, ws);
        else
            gsl_fft_complex_forward(interleaved.data(), 1, n, wt, ws);
    }
    if (wt)
        gsl_fft_complex_wavetable_free(wt);
    if (ws)
        gsl_fft_complex_workspace_free(ws);
}

// ============================================================================
// IIR Biquad (SOS) Design Helpers
// ============================================================================
struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0; // a0 = 1.0
};

std::vector<cdbl> computePrototypePoles(AdvancedSignalProc::IirDesign design, int order,
                                        double rippleDb)
{
    const int M = std::clamp(order, 1, 12);
    std::vector<cdbl> poles;
    poles.reserve(static_cast<size_t>(M));

    if (design == AdvancedSignalProc::Butterworth) {
        for (int k = 1; k <= M; ++k) {
            const double angle = M_PI * static_cast<double>(2 * k + M - 1)
                    / static_cast<double>(2 * M);
            poles.emplace_back(std::cos(angle), std::sin(angle));
        }
    } else if (design == AdvancedSignalProc::ChebyshevI) {
        const double rip = std::max(0.01, rippleDb);
        const double eps = std::sqrt(std::max(1e-8, std::pow(10.0, rip / 10.0) - 1.0));
        const double beta = std::asinh(1.0 / eps) / static_cast<double>(M);
        const double sh = std::sinh(beta);
        const double ch = std::cosh(beta);
        for (int k = 1; k <= M; ++k) {
            const double theta = M_PI * static_cast<double>(2 * k - 1) / static_cast<double>(2 * M);
            poles.emplace_back(-sh * std::sin(theta), ch * std::cos(theta));
        }
    } else {
        // Bessel: roots of reverse Bessel polynomial theta_M(s), normalized to -3 dB at Omega = 1
        std::vector<double> coeffs(static_cast<size_t>(M + 1), 1.0);
        coeffs[static_cast<size_t>(M)] = 1.0;
        for (int k = M; k >= 1; --k) {
            coeffs[static_cast<size_t>(k - 1)] = coeffs[static_cast<size_t>(k)]
                    * static_cast<double>((2 * M - k + 1) * k)
                    / static_cast<double>(2 * (M - k + 1));
        }

        std::vector<double> zRoots(static_cast<size_t>(2 * M), 0.0);
        gsl_poly_complex_workspace *ws =
                gsl_poly_complex_workspace_alloc(static_cast<size_t>(M + 1));
        int status = -1;
        if (ws) {
            status = gsl_poly_complex_solve(coeffs.data(), static_cast<size_t>(M + 1), ws,
                                            zRoots.data());
            gsl_poly_complex_workspace_free(ws);
        }

        if (status == 0) {
            for (int k = 0; k < M; ++k) {
                cdbl root(zRoots[static_cast<size_t>(2 * k)],
                          zRoots[static_cast<size_t>(2 * k + 1)]);
                if (root.real() >= -1e-9)
                    root = cdbl(-std::fabs(root.real()) - 0.1, root.imag());
                poles.push_back(root);
            }
            // Normalize -3 dB cutoff frequency to Omega = 1 rad/s
            auto magRatioSq = [&](double w) -> double {
                cdbl prodNum(1.0, 0.0), prodDen(1.0, 0.0);
                for (const cdbl &p : poles) {
                    prodNum *= (-p);
                    prodDen *= (cdbl(0.0, w) - p);
                }
                return std::norm(prodNum) / std::max(1e-300, std::norm(prodDen));
            };
            double lo = 0.0, hi = 20.0;
            while (magRatioSq(hi) > 0.5 && hi < 1e4)
                hi *= 2.0;
            for (int it = 0; it < 60; ++it) {
                const double mid = 0.5 * (lo + hi);
                if (magRatioSq(mid) > 0.5)
                    lo = mid;
                else
                    hi = mid;
            }
            const double w3dB = std::max(1e-6, 0.5 * (lo + hi));
            for (cdbl &p : poles)
                p /= w3dB;
        } else {
            // Fallback to Butterworth if GSL companion solver failed
            for (int k = 1; k <= M; ++k) {
                const double angle = M_PI * static_cast<double>(2 * k + M - 1)
                        / static_cast<double>(2 * M);
                poles.emplace_back(std::cos(angle), std::sin(angle));
            }
        }
    }
    return poles;
}

void splitConjAndReal(const std::vector<cdbl> &roots, std::vector<cdbl> &upperPairs,
                      std::vector<double> &reals)
{
    upperPairs.clear();
    reals.clear();

    std::vector<bool> used(roots.size(), false);
    const double imagEps = 1e-9;

    for (size_t i = 0; i < roots.size(); ++i) {
        if (used[i])
            continue;
        if (std::fabs(roots[i].imag()) <= imagEps) {
            used[i] = true;
            reals.push_back(roots[i].real());
            continue;
        }
        // Find closest conjugate partner
        size_t bestJ = roots.size();
        double bestDist = std::numeric_limits<double>::max();
        const cdbl target = std::conj(roots[i]);
        for (size_t j = i + 1; j < roots.size(); ++j) {
            if (used[j])
                continue;
            const double d = std::abs(roots[j] - target);
            if (d < bestDist) {
                bestDist = d;
                bestJ = j;
            }
        }
        used[i] = true;
        if (bestJ < roots.size() && bestDist < 1e-4) {
            used[bestJ] = true;
            const cdbl avg(0.5 * (roots[i].real() + roots[bestJ].real()),
                           0.5 * (std::fabs(roots[i].imag()) + std::fabs(roots[bestJ].imag())));
            upperPairs.push_back(avg);
        } else {
            reals.push_back(roots[i].real());
        }
    }

    // Sort conjugate poles by proximity to unit circle ascending (lowest Q first for stability)
    std::sort(upperPairs.begin(), upperPairs.end(),
              [](const cdbl &a, const cdbl &b) { return std::abs(a) < std::abs(b); });
    std::sort(reals.begin(), reals.end(),
              [](double a, double b) { return std::fabs(a) < std::fabs(b); });
}

std::vector<Biquad> designDigitalIirSOS(AdvancedSignalProc::IirDesign design,
                                        AdvancedSignalProc::PassType pass, int order,
                                        double fs, double fLow, double fHigh, double rippleDb)
{
    const int M = std::clamp(order, 1, 12);
    const std::vector<cdbl> protoPoles = computePrototypePoles(design, M, rippleDb);

    const double fMin = 0.0005 * fs;
    const double fMax = 0.4995 * fs;
    double fc1 = std::clamp(fLow, fMin, fMax);
    double fc2 = std::clamp(fHigh, fMin, fMax);
    if (pass == AdvancedSignalProc::BandPass || pass == AdvancedSignalProc::BandStop) {
        if (fc2 <= fc1) {
            if (fc2 > fMin)
                std::swap(fc1, fc2);
            if (fc2 <= fc1)
                fc2 = std::min(fMax, fc1 * 1.5 + 0.01 * fs);
        }
    }

    // Pre-warped analog frequencies with T = 2 (s = 2*(z-1)/(z+1))
    const double W1 = 2.0 * std::tan(M_PI * fc1 / fs);
    const double W2 = 2.0 * std::tan(M_PI * fc2 / fs);
    const double W0 = std::sqrt(W1 * W2);
    const double BW = std::max(1e-9, W2 - W1);

    std::vector<cdbl> sPoles;
    std::vector<cdbl> sZeros;

    switch (pass) {
    case AdvancedSignalProc::LowPass:
        for (const cdbl &p : protoPoles)
            sPoles.push_back(W1 * p);
        break;
    case AdvancedSignalProc::HighPass:
        for (const cdbl &p : protoPoles) {
            sPoles.push_back(W1 / p);
            sZeros.emplace_back(0.0, 0.0);
        }
        break;
    case AdvancedSignalProc::BandPass:
        for (const cdbl &p : protoPoles) {
            const cdbl half = 0.5 * BW * p;
            const cdbl disc = std::sqrt(half * half - cdbl(W0 * W0, 0.0));
            sPoles.push_back(half + disc);
            sPoles.push_back(half - disc);
            sZeros.emplace_back(0.0, 0.0);
        }
        break;
    case AdvancedSignalProc::BandStop:
        for (const cdbl &p : protoPoles) {
            const cdbl half = 0.5 * BW / p;
            const cdbl disc = std::sqrt(half * half - cdbl(W0 * W0, 0.0));
            sPoles.push_back(half + disc);
            sPoles.push_back(half - disc);
            sZeros.emplace_back(0.0, W0);
            sZeros.emplace_back(0.0, -W0);
        }
        break;
    }

    // Bilinear transform z = (2 + s) / (2 - s), padding zeros at s = inf -> z = -1
    std::vector<cdbl> zPoles;
    std::vector<cdbl> zZeros;
    zPoles.reserve(sPoles.size());
    zZeros.reserve(sPoles.size());

    for (const cdbl &sp : sPoles) {
        cdbl zp = (cdbl(2.0, 0.0) + sp) / (cdbl(2.0, 0.0) - sp);
        const double mag = std::abs(zp);
        if (mag >= 0.999999)
            zp *= (0.999999 / mag);
        zPoles.push_back(zp);
    }
    for (const cdbl &sz : sZeros)
        zZeros.push_back((cdbl(2.0, 0.0) + sz) / (cdbl(2.0, 0.0) - sz));
    while (zZeros.size() < zPoles.size())
        zZeros.emplace_back(-1.0, 0.0);

    std::vector<cdbl> polePairs, zeroPairs;
    std::vector<double> poleReals, zeroReals;
    splitConjAndReal(zPoles, polePairs, poleReals);
    splitConjAndReal(zZeros, zeroPairs, zeroReals);

    std::vector<Biquad> sos;
    size_t zpIdx = 0;
    size_t zrIdx = 0;

    for (const cdbl &pp : polePairs) {
        Biquad sec;
        sec.a1 = -2.0 * pp.real();
        sec.a2 = std::norm(pp);
        if (zpIdx < zeroPairs.size()) {
            const cdbl &zq = zeroPairs[zpIdx++];
            sec.b0 = 1.0;
            sec.b1 = -2.0 * zq.real();
            sec.b2 = std::norm(zq);
        } else if (zrIdx + 1 < zeroReals.size()) {
            const double q1 = zeroReals[zrIdx++];
            const double q2 = zeroReals[zrIdx++];
            sec.b0 = 1.0;
            sec.b1 = -(q1 + q2);
            sec.b2 = q1 * q2;
        } else if (zrIdx < zeroReals.size()) {
            const double q1 = zeroReals[zrIdx++];
            sec.b0 = 1.0;
            sec.b1 = -q1;
            sec.b2 = 0.0;
        }
        sos.push_back(sec);
    }

    for (size_t prIdx = 0; prIdx < poleReals.size(); prIdx += 2) {
        Biquad sec;
        if (prIdx + 1 < poleReals.size()) {
            const double p1 = poleReals[prIdx];
            const double p2 = poleReals[prIdx + 1];
            sec.a1 = -(p1 + p2);
            sec.a2 = p1 * p2;
            if (zpIdx < zeroPairs.size()) {
                const cdbl &zq = zeroPairs[zpIdx++];
                sec.b0 = 1.0;
                sec.b1 = -2.0 * zq.real();
                sec.b2 = std::norm(zq);
            } else if (zrIdx + 1 < zeroReals.size()) {
                const double q1 = zeroReals[zrIdx++];
                const double q2 = zeroReals[zrIdx++];
                sec.b0 = 1.0;
                sec.b1 = -(q1 + q2);
                sec.b2 = q1 * q2;
            }
        } else {
            // Single real pole -> 1st order section embedded as biquad
            sec.a1 = -poleReals[prIdx];
            sec.a2 = 0.0;
            if (zrIdx < zeroReals.size()) {
                sec.b0 = 1.0;
                sec.b1 = -zeroReals[zrIdx++];
                sec.b2 = 0.0;
            }
        }
        sos.push_back(sec);
    }

    if (sos.empty())
        return sos;

    // Normalize cascade passband gain at reference frequency
    double wRef = 0.0;
    if (pass == AdvancedSignalProc::HighPass) {
        wRef = M_PI;
    } else if (pass == AdvancedSignalProc::BandPass) {
        wRef = 2.0 * std::atan(0.5 * W0);
    }

    const cdbl zInv1 = std::exp(cdbl(0.0, -wRef));
    const cdbl zInv2 = std::exp(cdbl(0.0, -2.0 * wRef));
    cdbl H(1.0, 0.0);
    for (const Biquad &s : sos) {
        const cdbl num = s.b0 + s.b1 * zInv1 + s.b2 * zInv2;
        const cdbl den = 1.0 + s.a1 * zInv1 + s.a2 * zInv2;
        H *= (num / den);
    }

    double targetGain = 1.0;
    if (design == AdvancedSignalProc::ChebyshevI && (M % 2 == 0)) {
        const double rip = std::max(0.01, rippleDb);
        const double eps = std::sqrt(std::max(1e-8, std::pow(10.0, rip / 10.0) - 1.0));
        targetGain = 1.0 / std::sqrt(1.0 + eps * eps);
    }

    const double magH = std::abs(H);
    if (magH > 1e-30) {
        const double scale = targetGain / magH;
        // Distribute gain across sections to prevent internal overflow
        const double perSec = std::pow(scale, 1.0 / static_cast<double>(sos.size()));
        for (Biquad &s : sos) {
            s.b0 *= perSec;
            s.b1 *= perSec;
            s.b2 *= perSec;
        }
    }

    return sos;
}

void applyCascadeForward(const std::vector<Biquad> &sos, std::vector<double> &sig)
{
    if (sig.empty() || sos.empty())
        return;

    for (const Biquad &s : sos) {
        const double x0 = sig.front();
        const double denDc = 1.0 + s.a1 + s.a2;
        const double gDc = (std::fabs(denDc) > 1e-12) ? ((s.b0 + s.b1 + s.b2) / denDc) : 0.0;
        double z2 = (s.b2 - s.a2 * gDc) * x0;
        double z1 = (s.b1 - s.a1 * gDc) * x0 + z2;

        for (double &sample : sig) {
            const double x = sample;
            const double y = s.b0 * x + z1;
            z1 = s.b1 * x - s.a1 * y + z2;
            z2 = s.b2 * x - s.a2 * y;
            sample = y;
        }
    }
}

// ============================================================================
// Orthogonal Pyramid DWT / IDWT Helpers (Haar & Daubechies-4 db4)
// ============================================================================
const std::vector<double> &waveletLowpassFilter(AdvancedSignalProc::WaveletType wtype)
{
    static const std::vector<double> haarH = {
        0.70710678118654752440,
        0.70710678118654752440
    };
    static const std::vector<double> db4H = {
        -0.010597401785069032,
         0.032883011666885200,
         0.030841381835560764,
        -0.187034811719093090,
        -0.027983769416859854,
         0.630880767929858900,
         0.714846570552915400,
         0.230377813308896500
    };
    return (wtype == AdvancedSignalProc::Haar) ? haarH : db4H;
}

double computeMedian(std::vector<double> vals)
{
    if (vals.empty())
        return 0.0;
    const size_t n = vals.size();
    const size_t mid = n / 2;
    std::nth_element(vals.begin(), vals.begin() + static_cast<ptrdiff_t>(mid), vals.end());
    double med = vals[mid];
    if (n % 2 == 0) {
        const double lowerMax =
                *std::max_element(vals.begin(), vals.begin() + static_cast<ptrdiff_t>(mid));
        med = 0.5 * (lowerMax + med);
    }
    return med;
}

} // namespace

// ============================================================================
// Public Static Methods
// ============================================================================

QwtPlotCurve *AdvancedSignalProc::iirFilter(ApplicationWindow *app, Graph *g,
                                            const QString &curveTitle,
                                            IirDesign design, PassType pass, int order,
                                            double cutoffLowHz, double cutoffHighHz,
                                            bool zeroPhase, double rippleDb)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 4)
        return nullptr;

    const std::vector<Biquad> sos =
            designDigitalIirSOS(design, pass, order, fs, cutoffLowHz, cutoffHighHz, rippleDb);
    if (sos.empty())
        return nullptr;

    const int n = static_cast<int>(y.size());
    std::vector<double> filtered;

    if (zeroPhase && n >= 4) {
        const int padLen = std::min(n - 1, std::max(6, 6 * static_cast<int>(sos.size())));
        std::vector<double> work(static_cast<size_t>(n + 2 * padLen), 0.0);

        // Odd-symmetric boundary reflection preserves value and slope at endpoints
        for (int k = 0; k < padLen; ++k)
            work[static_cast<size_t>(k)] = 2.0 * y[0] - y[static_cast<size_t>(padLen - k)];
        for (int k = 0; k < n; ++k)
            work[static_cast<size_t>(padLen + k)] = y[static_cast<size_t>(k)];
        for (int k = 0; k < padLen; ++k)
            work[static_cast<size_t>(padLen + n + k)] =
                    2.0 * y[static_cast<size_t>(n - 1)] - y[static_cast<size_t>(n - 2 - k)];

        applyCascadeForward(sos, work);
        std::reverse(work.begin(), work.end());
        applyCascadeForward(sos, work);
        std::reverse(work.begin(), work.end());

        filtered.assign(work.begin() + padLen, work.begin() + padLen + n);
    } else {
        filtered = y;
        applyCascadeForward(sos, filtered);
    }

    Table *tbl = buildSignalTable(app, "IIRFilter",
                                  QStringList() << "Time" << "FilteredY",
                                  QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                                  { t, filtered });
    if (!tbl)
        return nullptr;

    if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, n - 1))
        return nullptr;

    QwtPlotCurve *resCurve = g->curve(g->curves() - 1);
    if (resCurve)
        resCurve->setPen(QPen(QColor(0, 114, 178), 2.0, Qt::SolidLine));
    g->updatePlot();

    app->updateLog(QString("[%1] IIR Digital Filter (order=%2, zeroPhase=%3, fs=%4 Hz) -> %5\n")
                           .arg(QLocale().toString(QDateTime::currentDateTime()))
                           .arg(order)
                           .arg(zeroPhase ? "true" : "false")
                           .arg(fs, 0, 'g', 6)
                           .arg(tbl->name()));
    return resCurve;
}

Matrix *AdvancedSignalProc::computeSTFT(ApplicationWindow *app, Graph *g,
                                        const QString &curveTitle,
                                        int windowSize, int hopSize, bool plotSpectrogram)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 4)
        return nullptr;

    const int n = static_cast<int>(y.size());
    const int W = std::max(8, windowSize);
    const int H = std::max(1, hopSize);

    // Hann window
    std::vector<double> win(static_cast<size_t>(W), 1.0);
    double winSum = 0.0;
    for (int m = 0; m < W; ++m) {
        win[static_cast<size_t>(m)] =
                0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(m) / static_cast<double>(W - 1)));
        winSum += win[static_cast<size_t>(m)];
    }
    const double normFactor = (winSum > 1e-12) ? (2.0 / winSum) : (1.0 / static_cast<double>(W));

    int nFrames = 1;
    if (n > W)
        nFrames = (n - W) / H + 1;
    const int nFreqs = W / 2 + 1;

    gsl_fft_real_wavetable *realWt = gsl_fft_real_wavetable_alloc(static_cast<size_t>(W));
    gsl_fft_real_workspace *realWs = gsl_fft_real_workspace_alloc(static_cast<size_t>(W));
    if (!realWt || !realWs) {
        if (realWt)
            gsl_fft_real_wavetable_free(realWt);
        if (realWs)
            gsl_fft_real_workspace_free(realWs);
        return nullptr;
    }

    Matrix *mat = app->newMatrix(uniqueWinName(app, "STFT"), nFreqs, nFrames);
    if (!mat) {
        gsl_fft_real_wavetable_free(realWt);
        gsl_fft_real_workspace_free(realWs);
        return nullptr;
    }

    const double dt = 1.0 / fs;
    const double tStart = t.front() + 0.5 * std::min(n - 1, W - 1) * dt;
    const double tEnd = (nFrames > 1) ? (tStart + static_cast<double>(nFrames - 1) * H * dt)
                                      : t.back();
    mat->setCoordinates(tStart, std::max(tStart + dt, tEnd), 0.0, 0.5 * fs);

    std::vector<double> frameBuf(static_cast<size_t>(W), 0.0);
    std::vector<double> unpacked(static_cast<size_t>(2 * W), 0.0);

    mat->d_future_matrix->beginMacro(trSig("Compute STFT"));
    for (int f = 0; f < nFrames; ++f) {
        const int startIdx = f * H;
        for (int m = 0; m < W; ++m) {
            const int srcIdx = startIdx + m;
            const double sample = (srcIdx < n) ? y[static_cast<size_t>(srcIdx)] : 0.0;
            frameBuf[static_cast<size_t>(m)] = sample * win[static_cast<size_t>(m)];
        }

        gsl_fft_real_transform(frameBuf.data(), 1, static_cast<size_t>(W), realWt, realWs);
        gsl_fft_halfcomplex_unpack(frameBuf.data(), unpacked.data(), 1, static_cast<size_t>(W));

        for (int k = 0; k < nFreqs; ++k) {
            const double re = unpacked[static_cast<size_t>(2 * k)];
            const double im = unpacked[static_cast<size_t>(2 * k + 1)];
            double mag = std::hypot(re, im) * normFactor;
            if (k == 0 || (W % 2 == 0 && k == W / 2))
                mag *= 0.5;
            mat->setCell(k, f, mag);
        }
    }
    mat->d_future_matrix->endMacro();

    gsl_fft_real_wavetable_free(realWt);
    gsl_fft_real_workspace_free(realWs);

    if (plotSpectrogram) {
        MultiLayer *ml = app->plotSpectrogram(mat, Graph::ColorMap);
        if (ml && ml->activeGraph()) {
            ml->activeGraph()->setXAxisTitle(trSig("Time"));
            ml->activeGraph()->setYAxisTitle(trSig("Frequency (Hz)"));
            ml->activeGraph()->replot();
        }
    }

    return mat;
}

QwtPlotCurve *AdvancedSignalProc::hilbertEnvelope(ApplicationWindow *app, Graph *g,
                                                  const QString &curveTitle,
                                                  bool plotLowerEnvelope, Table **outTable)
{
    if (outTable)
        *outTable = nullptr;
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 2)
        return nullptr;

    const size_t n = y.size();
    std::vector<double> z(2 * n, 0.0);
    for (size_t i = 0; i < n; ++i)
        z[2 * i] = y[i];

    // Forward FFT -> X[k]
    runComplexFft1D(z, n, false);

    // Exact Marple (1999) discrete analytic spectrum:
    // Z[0] = X[0]; Z[k] = 2*X[k] for 0 < k < (N+1)/2; Z[N/2] = X[N/2] (if N even); Z[k] = 0 for k > N/2
    const size_t posLimit = (n + 1) / 2;
    for (size_t k = 1; k < posLimit; ++k) {
        z[2 * k] *= 2.0;
        z[2 * k + 1] *= 2.0;
    }
    const size_t zeroStart = (n / 2) + 1;
    for (size_t k = zeroStart; k < n; ++k) {
        z[2 * k] = 0.0;
        z[2 * k + 1] = 0.0;
    }

    // Inverse FFT -> z(t) = y(t) + j * H{y}(t)
    runComplexFft1D(z, n, true);

    std::vector<double> upperEnv(n), lowerEnv(n), imagPart(n), instPhase(n), instFreq(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        const double re = z[2 * i];
        const double im = z[2 * i + 1];
        const double env = std::hypot(re, im);
        upperEnv[i] = env;
        lowerEnv[i] = -env;
        imagPart[i] = im;
        instPhase[i] = std::atan2(im, re);
    }

    // Unwrap phase
    for (size_t i = 1; i < n; ++i) {
        double diff = instPhase[i] - instPhase[i - 1];
        while (diff > M_PI) {
            instPhase[i] -= 2.0 * M_PI;
            diff -= 2.0 * M_PI;
        }
        while (diff < -M_PI) {
            instPhase[i] += 2.0 * M_PI;
            diff += 2.0 * M_PI;
        }
    }

    // Instantaneous frequency f(t) = (1 / 2pi) * d(phi)/dt
    const double dt = 1.0 / fs;
    if (n >= 2) {
        instFreq[0] = (instPhase[1] - instPhase[0]) / (2.0 * M_PI * dt);
        for (size_t i = 1; i + 1 < n; ++i)
            instFreq[i] = (instPhase[i + 1] - instPhase[i - 1]) / (4.0 * M_PI * dt);
        instFreq[n - 1] = (instPhase[n - 1] - instPhase[n - 2]) / (2.0 * M_PI * dt);
    }

    QStringList cols;
    cols << "Time" << "UpperEnvelope" << "LowerEnvelope" << "HilbertImag" << "InstPhaseRad"
         << "InstFreqHz";
    QList<SciDAVis::PlotDesignation> pds;
    pds << SciDAVis::X << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y;

    Table *tbl = buildSignalTable(app, "HilbertEnv", cols, pds,
                                  { t, upperEnv, lowerEnv, imagPart, instPhase, instFreq });
    if (!tbl)
        return nullptr;

    if (outTable)
        *outTable = tbl;

    const int nRows = static_cast<int>(n);
    if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, nRows - 1))
        return nullptr;
    QwtPlotCurve *upperCurve = g->curve(g->curves() - 1);
    if (upperCurve)
        upperCurve->setPen(QPen(QColor(213, 94, 0), 2.0, Qt::SolidLine));

    if (plotLowerEnvelope) {
        if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(2), Graph::Line, 0, nRows - 1)) {
            if (QwtPlotCurve *lowCurve = g->curve(g->curves() - 1))
                lowCurve->setPen(QPen(QColor(213, 94, 0, 200), 1.6, Qt::DashLine));
        }
    }
    g->updatePlot();
    return upperCurve;
}

Matrix *AdvancedSignalProc::waveletCWT(ApplicationWindow *app, Graph *g,
                                       const QString &curveTitle,
                                       int numScales, WaveletType wtype, bool plotScalogram)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 4)
        return nullptr;

    const size_t n = y.size();
    const int nScales = std::clamp(numScales, 2, 512);

    // Zero-mean detrend
    const double meanY = std::accumulate(y.begin(), y.end(), 0.0) / static_cast<double>(n);

    // Zero-pad to power-of-2 >= 2*N for wrap-free FFT wavelet convolution
    size_t nFft = 1;
    while (nFft < 2 * n)
        nFft <<= 1;

    std::vector<double> sigFft(2 * nFft, 0.0);
    for (size_t i = 0; i < n; ++i)
        sigFft[2 * i] = y[i] - meanY;

    gsl_fft_complex_wavetable *wt = gsl_fft_complex_wavetable_alloc(nFft);
    gsl_fft_complex_workspace *ws = gsl_fft_complex_workspace_alloc(nFft);
    if (!wt || !ws) {
        if (wt)
            gsl_fft_complex_wavetable_free(wt);
        if (ws)
            gsl_fft_complex_workspace_free(ws);
        return nullptr;
    }

    gsl_fft_complex_forward(sigFft.data(), 1, nFft, wt, ws);

    Matrix *mat = app->newMatrix(uniqueWinName(app, "WaveletCWT"), nScales, static_cast<int>(n));
    if (!mat) {
        gsl_fft_complex_wavetable_free(wt);
        gsl_fft_complex_workspace_free(ws);
        return nullptr;
    }

    const double sMin = 1.0;
    const double sMax = std::max(4.0, 0.30 * static_cast<double>(n));
    mat->setCoordinates(t.front(), t.back(), sMin, sMax);

    std::vector<double> omega(nFft, 0.0);
    for (size_t m = 0; m < nFft; ++m) {
        if (m <= nFft / 2)
            omega[m] = 2.0 * M_PI * static_cast<double>(m) / static_cast<double>(nFft);
        else
            omega[m] = -2.0 * M_PI * static_cast<double>(nFft - m) / static_cast<double>(nFft);
    }

    const double piPowNegQuarter = std::pow(M_PI, -0.25);
    const double omega0 = 6.0;
    std::vector<double> prodBuf(2 * nFft, 0.0);

    mat->d_future_matrix->beginMacro(trSig("Compute Wavelet CWT"));
    for (int r = 0; r < nScales; ++r) {
        const double frac = static_cast<double>(r) / static_cast<double>(std::max(1, nScales - 1));
        const double scale = sMin * std::pow(sMax / sMin, frac);

        for (size_t m = 0; m < nFft; ++m) {
            const double sw = scale * omega[m];
            cdbl psiHatConj(0.0, 0.0);

            if (wtype == Morlet) {
                if (omega[m] > 0.0) {
                    const double diff = sw - omega0;
                    const double gVal = std::sqrt(2.0 * M_PI * scale) * piPowNegQuarter
                            * std::exp(-0.5 * diff * diff);
                    psiHatConj = cdbl(gVal, 0.0);
                }
            } else if (wtype == RickerMexicanHat) {
                const double gVal = std::sqrt(2.0 * M_PI * scale) * (2.0 / std::sqrt(3.0))
                        * piPowNegQuarter * (sw * sw) * std::exp(-0.5 * sw * sw);
                psiHatConj = cdbl(gVal, 0.0);
            } else {
                // Continuous Haar wavelet in Fourier domain
                if (std::fabs(sw) > 1e-12) {
                    const double q = 0.25 * sw;
                    const double val = std::sqrt(scale) * (std::sin(q) * std::sin(q) / q);
                    psiHatConj = cdbl(0.0, val);
                }
            }

            const cdbl Ym(sigFft[2 * m], sigFft[2 * m + 1]);
            const cdbl Wm = Ym * psiHatConj;
            prodBuf[2 * m] = Wm.real();
            prodBuf[2 * m + 1] = Wm.imag();
        }

        gsl_fft_complex_inverse(prodBuf.data(), 1, nFft, wt, ws);

        for (size_t c = 0; c < n; ++c) {
            const double mag = std::hypot(prodBuf[2 * c], prodBuf[2 * c + 1]);
            mat->setCell(r, static_cast<int>(c), mag);
        }
    }
    mat->d_future_matrix->endMacro();

    gsl_fft_complex_wavetable_free(wt);
    gsl_fft_complex_workspace_free(ws);

    if (plotScalogram) {
        MultiLayer *ml = app->plotSpectrogram(mat, Graph::ColorMap);
        if (ml && ml->activeGraph()) {
            ml->activeGraph()->setXAxisTitle(trSig("Time"));
            ml->activeGraph()->setYAxisTitle(trSig("Wavelet Scale"));
            ml->activeGraph()->replot();
        }
    }

    return mat;
}

QwtPlotCurve *AdvancedSignalProc::waveletDenoiseDWT(ApplicationWindow *app, Graph *g,
                                                    const QString &curveTitle,
                                                    WaveletType wtype, int levels,
                                                    bool softThreshold, double thresholdScale)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 4)
        return nullptr;

    const int n = static_cast<int>(y.size());
    const std::vector<double> &h = waveletLowpassFilter(wtype);
    const int K = static_cast<int>(h.size());
    std::vector<double> gHigh(static_cast<size_t>(K), 0.0);
    for (int k = 0; k < K; ++k) {
        const double sign = (k % 2 == 0) ? 1.0 : -1.0;
        gHigh[static_cast<size_t>(k)] = sign * h[static_cast<size_t>(K - 1 - k)];
    }

    // Symmetric boundary extension + pad to multiple of 2^L
    const int L = std::clamp(levels, 1, 10);
    const int padLeft = std::min(n - 1, std::max(K, 16));
    const int block = 1 << L;
    const int minPadded = std::max(block * std::max(1, K), n + 2 * padLeft);
    const int P = ((minPadded + block - 1) / block) * block;

    std::vector<double> approx(static_cast<size_t>(P), 0.0);
    auto mirrorSample = [&](int idx) -> double {
        if (n == 1)
            return y[0];
        const int period = 2 * (n - 1);
        int m = idx % period;
        if (m < 0)
            m += period;
        if (m >= n)
            m = period - m;
        return y[static_cast<size_t>(m)];
    };

    for (int i = 0; i < P; ++i)
        approx[static_cast<size_t>(i)] = mirrorSample(i - padLeft);

    // Forward pyramid DWT
    std::vector<std::vector<double>> details(static_cast<size_t>(L));
    int actualLevels = 0;
    for (int lev = 0; lev < L; ++lev) {
        const int M = static_cast<int>(approx.size());
        if (M < 2 || M < K || (M % 2 != 0))
            break;
        const int half = M / 2;
        std::vector<double> nextApprox(static_cast<size_t>(half), 0.0);
        std::vector<double> det(static_cast<size_t>(half), 0.0);

        for (int m = 0; m < half; ++m) {
            double sa = 0.0, sd = 0.0;
            for (int k = 0; k < K; ++k) {
                const double val = approx[static_cast<size_t>((2 * m + k) % M)];
                sa += h[static_cast<size_t>(k)] * val;
                sd += gHigh[static_cast<size_t>(k)] * val;
            }
            nextApprox[static_cast<size_t>(m)] = sa;
            det[static_cast<size_t>(m)] = sd;
        }
        approx.swap(nextApprox);
        details[static_cast<size_t>(lev)] = std::move(det);
        ++actualLevels;
    }

    if (actualLevels > 0 && !details[0].empty()) {
        // Estimate noise sigma via MAD on finest-level central detail coefficients
        const int d1Start = std::clamp(padLeft / 2, 0, static_cast<int>(details[0].size()) - 1);
        const int d1Count = std::clamp(n / 2, 1, static_cast<int>(details[0].size()) - d1Start);
        std::vector<double> coreD1(details[0].begin() + d1Start,
                                   details[0].begin() + d1Start + d1Count);
        const double medD1 = computeMedian(coreD1);
        for (double &v : coreD1)
            v = std::fabs(v - medD1);
        const double mad = computeMedian(coreD1);
        const double sigmaHat = mad / 0.6744897501960817;
        const double scaleMult = std::max(0.0, thresholdScale);
        const double lambda = scaleMult * sigmaHat
                * std::sqrt(2.0 * std::log(static_cast<double>(std::max(2, n))));

        for (int lev = 0; lev < actualLevels; ++lev) {
            for (double &d : details[static_cast<size_t>(lev)]) {
                const double ad = std::fabs(d);
                if (softThreshold) {
                    d = (ad > lambda) ? std::copysign(ad - lambda, d) : 0.0;
                } else {
                    if (ad < lambda)
                        d = 0.0;
                }
            }
        }
    }

    // Exact orthogonal transpose inverse pyramid IDWT
    for (int lev = actualLevels - 1; lev >= 0; --lev) {
        const std::vector<double> &det = details[static_cast<size_t>(lev)];
        const int half = static_cast<int>(approx.size());
        const int M = 2 * half;
        std::vector<double> recon(static_cast<size_t>(M), 0.0);

        for (int m = 0; m < half; ++m) {
            const double ca = approx[static_cast<size_t>(m)];
            const double cd = det[static_cast<size_t>(m)];
            for (int k = 0; k < K; ++k) {
                const size_t dst = static_cast<size_t>((2 * m + k) % M);
                recon[dst] += h[static_cast<size_t>(k)] * ca + gHigh[static_cast<size_t>(k)] * cd;
            }
        }
        approx.swap(recon);
    }

    std::vector<double> denoised(approx.begin() + padLeft, approx.begin() + padLeft + n);

    Table *tbl = buildSignalTable(app, "WaveletDenoise",
                                  QStringList() << "Time" << "DenoisedY",
                                  QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                                  { t, denoised });
    if (!tbl)
        return nullptr;

    if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, n - 1))
        return nullptr;

    QwtPlotCurve *resCurve = g->curve(g->curves() - 1);
    if (resCurve)
        resCurve->setPen(QPen(QColor(0, 158, 115), 2.0, Qt::SolidLine));
    g->updatePlot();
    return resCurve;
}

Matrix *AdvancedSignalProc::fft2D(ApplicationWindow *app, Matrix *srcMatrix,
                                  bool inverse, bool shiftCenter)
{
    if (!app || !srcMatrix)
        return nullptr;

    const int rows = srcMatrix->numRows();
    const int cols = srcMatrix->numCols();
    if (rows <= 0 || cols <= 0)
        return nullptr;

    const size_t R = static_cast<size_t>(rows);
    const size_t C = static_cast<size_t>(cols);

    std::vector<double> grid(2 * R * C, 0.0);
    for (size_t r = 0; r < R; ++r) {
        for (size_t c = 0; c < C; ++c) {
            const double val = srcMatrix->cell(static_cast<int>(r), static_cast<int>(c));
            grid[2 * (r * C + c)] = std::isfinite(val) ? val : 0.0;
        }
    }

    // If inverse and input was centered, undo shift prior to 2D IFFT
    if (inverse && shiftCenter) {
        std::vector<double> unshifted(2 * R * C, 0.0);
        const size_t sr = (R + 1) / 2;
        const size_t sc = (C + 1) / 2;
        for (size_t r = 0; r < R; ++r) {
            for (size_t c = 0; c < C; ++c) {
                const size_t srcIdx = 2 * (((r + sr) % R) * C + ((c + sc) % C));
                const size_t dstIdx = 2 * (r * C + c);
                unshifted[dstIdx] = grid[srcIdx];
                unshifted[dstIdx + 1] = grid[srcIdx + 1];
            }
        }
        grid.swap(unshifted);
    }

    // 1D complex FFT along each row
    if (C > 1) {
        gsl_fft_complex_wavetable *wtC = gsl_fft_complex_wavetable_alloc(C);
        gsl_fft_complex_workspace *wsC = gsl_fft_complex_workspace_alloc(C);
        if (wtC && wsC) {
            for (size_t r = 0; r < R; ++r) {
                double *rowPtr = grid.data() + 2 * r * C;
                if (inverse)
                    gsl_fft_complex_inverse(rowPtr, 1, C, wtC, wsC);
                else
                    gsl_fft_complex_forward(rowPtr, 1, C, wtC, wsC);
            }
        }
        if (wtC)
            gsl_fft_complex_wavetable_free(wtC);
        if (wsC)
            gsl_fft_complex_workspace_free(wsC);
    }

    // 1D complex FFT along each column (stride = C complex elements)
    if (R > 1) {
        gsl_fft_complex_wavetable *wtR = gsl_fft_complex_wavetable_alloc(R);
        gsl_fft_complex_workspace *wsR = gsl_fft_complex_workspace_alloc(R);
        if (wtR && wsR) {
            for (size_t c = 0; c < C; ++c) {
                double *colPtr = grid.data() + 2 * c;
                if (inverse)
                    gsl_fft_complex_inverse(colPtr, C, R, wtR, wsR);
                else
                    gsl_fft_complex_forward(colPtr, C, R, wtR, wsR);
            }
        }
        if (wtR)
            gsl_fft_complex_wavetable_free(wtR);
        if (wsR)
            gsl_fft_complex_workspace_free(wsR);
    }

    // Quadrant fftshift on forward FFT when requested
    if (!inverse && shiftCenter) {
        std::vector<double> shifted(2 * R * C, 0.0);
        const size_t sr = R / 2;
        const size_t sc = C / 2;
        for (size_t r = 0; r < R; ++r) {
            for (size_t c = 0; c < C; ++c) {
                const size_t dstR = (r + sr) % R;
                const size_t dstC = (c + sc) % C;
                const size_t srcIdx = 2 * (r * C + c);
                const size_t dstIdx = 2 * (dstR * C + dstC);
                shifted[dstIdx] = grid[srcIdx];
                shifted[dstIdx + 1] = grid[srcIdx + 1];
            }
        }
        grid.swap(shifted);
    }

    Matrix *outMat = app->newMatrix(uniqueWinName(app, inverse ? "IFFT2D" : "FFT2D"), rows, cols);
    if (!outMat)
        return nullptr;

    const double dx = (cols > 1) ? std::fabs(srcMatrix->xEnd() - srcMatrix->xStart())
                    / static_cast<double>(cols - 1)
                                 : 1.0;
    const double dy = (rows > 1) ? std::fabs(srcMatrix->yEnd() - srcMatrix->yStart())
                    / static_cast<double>(rows - 1)
                                 : 1.0;
    const double fxMax = (dx > 1e-15) ? (0.5 / dx) : 0.5;
    const double fyMax = (dy > 1e-15) ? (0.5 / dy) : 0.5;
    if (!inverse) {
        if (shiftCenter)
            outMat->setCoordinates(-fxMax, fxMax, -fyMax, fyMax);
        else
            outMat->setCoordinates(0.0, 2.0 * fxMax, 0.0, 2.0 * fyMax);
    } else {
        outMat->setCoordinates(srcMatrix->xStart(), srcMatrix->xEnd(),
                               srcMatrix->yStart(), srcMatrix->yEnd());
    }

    outMat->d_future_matrix->beginMacro(inverse ? trSig("2D IFFT") : trSig("2D FFT"));
    for (size_t r = 0; r < R; ++r) {
        for (size_t c = 0; c < C; ++c) {
            const size_t idx = 2 * (r * C + c);
            const double val = std::hypot(grid[idx], grid[idx + 1]);
            outMat->setCell(static_cast<int>(r), static_cast<int>(c), val);
        }
    }
    outMat->d_future_matrix->endMacro();

    return outMat;
}

Table *AdvancedSignalProc::welchPSD(ApplicationWindow *app, Graph *g,
                                    const QString &curveTitle,
                                    int segmentLen, bool plotResult)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 4)
        return nullptr;

    const int n = static_cast<int>(y.size());
    const int L = std::clamp(segmentLen, 4, n);
    const int H = std::max(1, L / 2);

    std::vector<double> win(static_cast<size_t>(L), 1.0);
    double winEnergy = 0.0;
    for (int m = 0; m < L; ++m) {
        win[static_cast<size_t>(m)] = (L > 1)
                ? (0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(m)
                                         / static_cast<double>(L - 1))))
                : 1.0;
        winEnergy += win[static_cast<size_t>(m)] * win[static_cast<size_t>(m)];
    }
    if (winEnergy <= 1e-30)
        winEnergy = static_cast<double>(L);

    const int nBins = L / 2 + 1;
    std::vector<double> psdAccum(static_cast<size_t>(nBins), 0.0);

    gsl_fft_real_wavetable *wt = gsl_fft_real_wavetable_alloc(static_cast<size_t>(L));
    gsl_fft_real_workspace *ws = gsl_fft_real_workspace_alloc(static_cast<size_t>(L));
    if (!wt || !ws) {
        if (wt)
            gsl_fft_real_wavetable_free(wt);
        if (ws)
            gsl_fft_real_workspace_free(ws);
        return nullptr;
    }

    std::vector<double> segBuf(static_cast<size_t>(L), 0.0);
    std::vector<double> unpacked(static_cast<size_t>(2 * L), 0.0);
    int nSegs = 0;

    for (int start = 0; start + L <= n; start += H) {
        double segMean = 0.0;
        for (int m = 0; m < L; ++m)
            segMean += y[static_cast<size_t>(start + m)];
        segMean /= static_cast<double>(L);

        for (int m = 0; m < L; ++m) {
            segBuf[static_cast<size_t>(m)] =
                    (y[static_cast<size_t>(start + m)] - segMean) * win[static_cast<size_t>(m)];
        }

        gsl_fft_real_transform(segBuf.data(), 1, static_cast<size_t>(L), wt, ws);
        gsl_fft_halfcomplex_unpack(segBuf.data(), unpacked.data(), 1, static_cast<size_t>(L));

        for (int k = 0; k < nBins; ++k) {
            const double re = unpacked[static_cast<size_t>(2 * k)];
            const double im = unpacked[static_cast<size_t>(2 * k + 1)];
            double p = (re * re + im * im) / (fs * winEnergy);
            if (k > 0 && !(L % 2 == 0 && k == L / 2))
                p *= 2.0;
            psdAccum[static_cast<size_t>(k)] += p;
        }
        ++nSegs;
    }

    gsl_fft_real_wavetable_free(wt);
    gsl_fft_real_workspace_free(ws);

    if (nSegs == 0)
        return nullptr;

    std::vector<double> freqs(static_cast<size_t>(nBins));
    std::vector<double> psd(static_cast<size_t>(nBins));
    std::vector<double> psdDb(static_cast<size_t>(nBins));

    for (int k = 0; k < nBins; ++k) {
        freqs[static_cast<size_t>(k)] = static_cast<double>(k) * fs / static_cast<double>(L);
        const double val = psdAccum[static_cast<size_t>(k)] / static_cast<double>(nSegs);
        psd[static_cast<size_t>(k)] = val;
        psdDb[static_cast<size_t>(k)] = 10.0 * std::log10(std::max(val, 1e-300));
    }

    Table *tbl = buildSignalTable(
            app, "WelchPSD", QStringList() << "FrequencyHz" << "PSD" << "PSDdB",
            QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y << SciDAVis::Y,
            { freqs, psd, psdDb });
    if (!tbl)
        return nullptr;

    if (plotResult) {
        MultiLayer *ml = app->multilayerPlot(tbl, QStringList() << tbl->colName(1), Graph::Line, 0,
                                             nBins - 1);
        if (ml && ml->activeGraph()) {
            ml->activeGraph()->setXAxisTitle(trSig("Frequency (Hz)"));
            ml->activeGraph()->setYAxisTitle(trSig("Power Spectral Density"));
            ml->activeGraph()->replot();
        }
    }

    return tbl;
}

Table *AdvancedSignalProc::crossCoherence(ApplicationWindow *app, Graph *g,
                                          const QString &curveA, const QString &curveB,
                                          int segmentLen, bool plotResult)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> tA, yA, tB, yB;
    double fsA = 1.0, fsB = 1.0;
    if (!extractUniformSignal(g, curveA, tA, yA, fsA)
        || !extractUniformSignal(g, curveB, tB, yB, fsB))
        return nullptr;

    const double tLo = std::max(tA.front(), tB.front());
    const double tHi = std::min(tA.back(), tB.back());
    if (!(tHi > tLo))
        return nullptr;

    const int nGrid = std::max(static_cast<int>(tA.size()), static_cast<int>(tB.size()));
    if (nGrid < 8)
        return nullptr;

    const double dt = (tHi - tLo) / static_cast<double>(nGrid - 1);
    const double fs = 1.0 / dt;

    auto resampleAt = [](const std::vector<double> &ts, const std::vector<double> &ys,
                         double tq) -> double {
        if (tq <= ts.front())
            return ys.front();
        if (tq >= ts.back())
            return ys.back();
        auto it = std::lower_bound(ts.begin(), ts.end(), tq);
        if (it == ts.begin())
            return ys.front();
        const size_t hi = static_cast<size_t>(it - ts.begin());
        const size_t lo = hi - 1;
        const double d = ts[hi] - ts[lo];
        return (d > 1e-300) ? (ys[lo] + ((tq - ts[lo]) / d) * (ys[hi] - ys[lo])) : ys[lo];
    };

    std::vector<double> sigA(static_cast<size_t>(nGrid)), sigB(static_cast<size_t>(nGrid));
    for (int i = 0; i < nGrid; ++i) {
        const double tq = (i == nGrid - 1) ? tHi : (tLo + static_cast<double>(i) * dt);
        sigA[static_cast<size_t>(i)] = resampleAt(tA, yA, tq);
        sigB[static_cast<size_t>(i)] = resampleAt(tB, yB, tq);
    }

    // Ensure at least 2 overlapped segments when nGrid >= 16 so coherence is non-degenerate
    int maxSeg = (nGrid >= 16) ? std::max(8, (2 * nGrid) / 3) : nGrid;
    const int L = std::clamp(segmentLen, 4, maxSeg);
    const int H = std::max(1, L / 2);

    std::vector<double> win(static_cast<size_t>(L), 1.0);
    for (int m = 0; m < L; ++m) {
        win[static_cast<size_t>(m)] = (L > 1)
                ? (0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(m)
                                         / static_cast<double>(L - 1))))
                : 1.0;
    }

    const int nBins = L / 2 + 1;
    std::vector<double> pxx(static_cast<size_t>(nBins), 0.0);
    std::vector<double> pyy(static_cast<size_t>(nBins), 0.0);
    std::vector<cdbl> pxy(static_cast<size_t>(nBins), cdbl(0.0, 0.0));

    gsl_fft_real_wavetable *wt = gsl_fft_real_wavetable_alloc(static_cast<size_t>(L));
    gsl_fft_real_workspace *ws = gsl_fft_real_workspace_alloc(static_cast<size_t>(L));
    if (!wt || !ws) {
        if (wt)
            gsl_fft_real_wavetable_free(wt);
        if (ws)
            gsl_fft_real_workspace_free(ws);
        return nullptr;
    }

    std::vector<double> bufA(static_cast<size_t>(L)), bufB(static_cast<size_t>(L));
    std::vector<double> unpA(static_cast<size_t>(2 * L)), unpB(static_cast<size_t>(2 * L));
    int nSegs = 0;

    for (int start = 0; start + L <= nGrid; start += H) {
        double meanA = 0.0, meanB = 0.0;
        for (int m = 0; m < L; ++m) {
            meanA += sigA[static_cast<size_t>(start + m)];
            meanB += sigB[static_cast<size_t>(start + m)];
        }
        meanA /= static_cast<double>(L);
        meanB /= static_cast<double>(L);

        for (int m = 0; m < L; ++m) {
            bufA[static_cast<size_t>(m)] =
                    (sigA[static_cast<size_t>(start + m)] - meanA) * win[static_cast<size_t>(m)];
            bufB[static_cast<size_t>(m)] =
                    (sigB[static_cast<size_t>(start + m)] - meanB) * win[static_cast<size_t>(m)];
        }

        gsl_fft_real_transform(bufA.data(), 1, static_cast<size_t>(L), wt, ws);
        gsl_fft_halfcomplex_unpack(bufA.data(), unpA.data(), 1, static_cast<size_t>(L));

        gsl_fft_real_transform(bufB.data(), 1, static_cast<size_t>(L), wt, ws);
        gsl_fft_halfcomplex_unpack(bufB.data(), unpB.data(), 1, static_cast<size_t>(L));

        for (int k = 0; k < nBins; ++k) {
            const cdbl Xk(unpA[static_cast<size_t>(2 * k)], unpA[static_cast<size_t>(2 * k + 1)]);
            const cdbl Yk(unpB[static_cast<size_t>(2 * k)], unpB[static_cast<size_t>(2 * k + 1)]);
            pxx[static_cast<size_t>(k)] += std::norm(Xk);
            pyy[static_cast<size_t>(k)] += std::norm(Yk);
            pxy[static_cast<size_t>(k)] += Xk * std::conj(Yk);
        }
        ++nSegs;
    }

    gsl_fft_real_wavetable_free(wt);
    gsl_fft_real_workspace_free(ws);

    if (nSegs == 0)
        return nullptr;

    std::vector<double> freqs(static_cast<size_t>(nBins));
    std::vector<double> coh(static_cast<size_t>(nBins), 0.0);
    std::vector<double> phaseDeg(static_cast<size_t>(nBins), 0.0);

    for (int k = 0; k < nBins; ++k) {
        freqs[static_cast<size_t>(k)] = static_cast<double>(k) * fs / static_cast<double>(L);
        const double denom = pxx[static_cast<size_t>(k)] * pyy[static_cast<size_t>(k)];
        if (denom > 1e-300) {
            coh[static_cast<size_t>(k)] =
                    std::clamp(std::norm(pxy[static_cast<size_t>(k)]) / denom, 0.0, 1.0);
        }
        phaseDeg[static_cast<size_t>(k)] = std::arg(pxy[static_cast<size_t>(k)]) * (180.0 / M_PI);
    }

    Table *tbl = buildSignalTable(
            app, "Coherence", QStringList() << "FrequencyHz" << "Coherence" << "PhaseDeg",
            QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y << SciDAVis::Y,
            { freqs, coh, phaseDeg });
    if (!tbl)
        return nullptr;

    if (plotResult) {
        MultiLayer *ml = app->multilayerPlot(tbl, QStringList() << tbl->colName(1), Graph::Line, 0,
                                             nBins - 1);
        if (ml && ml->activeGraph()) {
            ml->activeGraph()->setXAxisTitle(trSig("Frequency (Hz)"));
            ml->activeGraph()->setYAxisTitle(trSig("Magnitude-Squared Coherence"));
            ml->activeGraph()->setScale(QwtPlot::yLeft, 0.0, 1.05);
            ml->activeGraph()->replot();
        }
    }

    return tbl;
}

// ============================================================================
// OriginPro Signal Processing Dialogs
// ============================================================================

void AdvancedSignalProc::showIirFilterDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trSig("IIR Digital Filter"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);

    QComboBox *cbDesign = new QComboBox(&dlg);
    cbDesign->addItem(trSig("Butterworth"), static_cast<int>(Butterworth));
    cbDesign->addItem(trSig("Chebyshev Type I"), static_cast<int>(ChebyshevI));
    cbDesign->addItem(trSig("Bessel"), static_cast<int>(Bessel));

    QComboBox *cbPass = new QComboBox(&dlg);
    cbPass->addItem(trSig("Low Pass"), static_cast<int>(LowPass));
    cbPass->addItem(trSig("High Pass"), static_cast<int>(HighPass));
    cbPass->addItem(trSig("Band Pass"), static_cast<int>(BandPass));
    cbPass->addItem(trSig("Band Stop (Notch)"), static_cast<int>(BandStop));

    QSpinBox *sbOrder = new QSpinBox(&dlg);
    sbOrder->setRange(1, 12);
    sbOrder->setValue(4);

    std::vector<double> t, y;
    double fs = 100.0;
    extractUniformSignal(g, curves.first(), t, y, fs);

    QDoubleSpinBox *sbLow = new QDoubleSpinBox(&dlg);
    sbLow->setRange(1e-6, 1e12);
    sbLow->setDecimals(4);
    sbLow->setValue(std::max(0.01, 0.10 * fs));

    QDoubleSpinBox *sbHigh = new QDoubleSpinBox(&dlg);
    sbHigh->setRange(1e-6, 1e12);
    sbHigh->setDecimals(4);
    sbHigh->setValue(std::max(0.02, 0.25 * fs));
    sbHigh->setEnabled(false);

    QDoubleSpinBox *sbRipple = new QDoubleSpinBox(&dlg);
    sbRipple->setRange(0.01, 20.0);
    sbRipple->setDecimals(2);
    sbRipple->setValue(1.0);
    sbRipple->setEnabled(false);

    QCheckBox *chkZeroPhase = new QCheckBox(trSig("Zero-Phase Forward-Backward (filtfilt)"), &dlg);
    chkZeroPhase->setChecked(true);

    QObject::connect(cbPass, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int idx) {
                         const int p = cbPass->itemData(idx).toInt();
                         sbHigh->setEnabled(p == BandPass || p == BandStop);
                     });
    QObject::connect(cbDesign, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int idx) {
                         sbRipple->setEnabled(cbDesign->itemData(idx).toInt() == ChebyshevI);
                     });

    form->addRow(trSig("Curve:"), cbCurve);
    form->addRow(trSig("Prototype Design:"), cbDesign);
    form->addRow(trSig("Pass Type:"), cbPass);
    form->addRow(trSig("Filter Order:"), sbOrder);
    form->addRow(trSig("Cutoff / Low Frequency (Hz):"), sbLow);
    form->addRow(trSig("Upper Cutoff Frequency (Hz):"), sbHigh);
    form->addRow(trSig("Passband Ripple (dB):"), sbRipple);
    form->addRow(QString(), chkZeroPhase);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        iirFilter(app, g, cbCurve->currentText(),
                  static_cast<IirDesign>(cbDesign->currentData().toInt()),
                  static_cast<PassType>(cbPass->currentData().toInt()), sbOrder->value(),
                  sbLow->value(), sbHigh->value(), chkZeroPhase->isChecked(), sbRipple->value());
    }
}

void AdvancedSignalProc::showStftDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trSig("Short-Time Fourier Transform (STFT)"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);
    QSpinBox *sbWin = new QSpinBox(&dlg);
    sbWin->setRange(8, 16384);
    sbWin->setSingleStep(16);
    sbWin->setValue(64);
    QSpinBox *sbHop = new QSpinBox(&dlg);
    sbHop->setRange(1, 8192);
    sbHop->setSingleStep(8);
    sbHop->setValue(16);
    QCheckBox *chkPlot = new QCheckBox(trSig("Plot Time-Frequency Spectrogram"), &dlg);
    chkPlot->setChecked(true);

    form->addRow(trSig("Curve:"), cbCurve);
    form->addRow(trSig("Window Length (Samples):"), sbWin);
    form->addRow(trSig("Hop Size (Samples):"), sbHop);
    form->addRow(QString(), chkPlot);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted)
        computeSTFT(app, g, cbCurve->currentText(), sbWin->value(), sbHop->value(),
                    chkPlot->isChecked());
}

void AdvancedSignalProc::showHilbertDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trSig("Hilbert Transform & Envelope"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);
    QCheckBox *chkLower = new QCheckBox(trSig("Also Plot Lower Envelope (-|z(t)|)"), &dlg);
    chkLower->setChecked(true);

    form->addRow(trSig("Curve:"), cbCurve);
    form->addRow(QString(), chkLower);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted)
        hilbertEnvelope(app, g, cbCurve->currentText(), chkLower->isChecked(), nullptr);
}

void AdvancedSignalProc::showWaveletDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trSig("Wavelet Analysis (CWT & DWT Denoising)"));
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    QTabWidget *tabs = new QTabWidget(&dlg);

    // Tab 1: Continuous Wavelet Transform
    QWidget *tabCwt = new QWidget(&dlg);
    QFormLayout *fCwt = new QFormLayout(tabCwt);
    QComboBox *cbCwtCurve = new QComboBox(tabCwt);
    cbCwtCurve->addItems(curves);
    QComboBox *cbCwtType = new QComboBox(tabCwt);
    cbCwtType->addItem(trSig("Morlet (omega0 = 6)"), static_cast<int>(Morlet));
    cbCwtType->addItem(trSig("Ricker / Mexican Hat"), static_cast<int>(RickerMexicanHat));
    cbCwtType->addItem(trSig("Haar"), static_cast<int>(Haar));
    QSpinBox *sbScales = new QSpinBox(tabCwt);
    sbScales->setRange(4, 512);
    sbScales->setValue(32);
    QCheckBox *chkScalogram = new QCheckBox(trSig("Plot CWT Scalogram"), tabCwt);
    chkScalogram->setChecked(true);
    fCwt->addRow(trSig("Curve:"), cbCwtCurve);
    fCwt->addRow(trSig("Mother Wavelet:"), cbCwtType);
    fCwt->addRow(trSig("Number of Scales:"), sbScales);
    fCwt->addRow(QString(), chkScalogram);
    tabs->addTab(tabCwt, trSig("Continuous Wavelet (CWT)"));

    // Tab 2: DWT VisuShrink Denoise
    QWidget *tabDwt = new QWidget(&dlg);
    QFormLayout *fDwt = new QFormLayout(tabDwt);
    QComboBox *cbDwtCurve = new QComboBox(tabDwt);
    cbDwtCurve->addItems(curves);
    QComboBox *cbDwtType = new QComboBox(tabDwt);
    cbDwtType->addItem(trSig("Daubechies-4 (db4)"), static_cast<int>(Daubechies4));
    cbDwtType->addItem(trSig("Haar (db1)"), static_cast<int>(Haar));
    QSpinBox *sbLevels = new QSpinBox(tabDwt);
    sbLevels->setRange(1, 10);
    sbLevels->setValue(4);
    QCheckBox *chkSoft = new QCheckBox(trSig("Soft Thresholding (VisuShrink)"), tabDwt);
    chkSoft->setChecked(true);
    QDoubleSpinBox *sbThreshScale = new QDoubleSpinBox(tabDwt);
    sbThreshScale->setRange(0.05, 10.0);
    sbThreshScale->setDecimals(2);
    sbThreshScale->setSingleStep(0.1);
    sbThreshScale->setValue(1.0);
    fDwt->addRow(trSig("Curve:"), cbDwtCurve);
    fDwt->addRow(trSig("Wavelet Family:"), cbDwtType);
    fDwt->addRow(trSig("Decomposition Levels:"), sbLevels);
    fDwt->addRow(trSig("Threshold Scale:"), sbThreshScale);
    fDwt->addRow(QString(), chkSoft);
    tabs->addTab(tabDwt, trSig("DWT Denoise"));

    vbox->addWidget(tabs);
    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        if (tabs->currentIndex() == 0) {
            waveletCWT(app, g, cbCwtCurve->currentText(), sbScales->value(),
                       static_cast<WaveletType>(cbCwtType->currentData().toInt()),
                       chkScalogram->isChecked());
        } else {
            waveletDenoiseDWT(app, g, cbDwtCurve->currentText(),
                              static_cast<WaveletType>(cbDwtType->currentData().toInt()),
                              sbLevels->value(), chkSoft->isChecked(), sbThreshScale->value());
        }
    }
}

void AdvancedSignalProc::showPsdCoherenceDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trSig("Welch PSD & Cross-Coherence"));
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    QTabWidget *tabs = new QTabWidget(&dlg);

    QWidget *tabPsd = new QWidget(&dlg);
    QFormLayout *fPsd = new QFormLayout(tabPsd);
    QComboBox *cbPsdCurve = new QComboBox(tabPsd);
    cbPsdCurve->addItems(curves);
    QSpinBox *sbPsdSeg = new QSpinBox(tabPsd);
    sbPsdSeg->setRange(8, 65536);
    sbPsdSeg->setSingleStep(32);
    sbPsdSeg->setValue(128);
    QCheckBox *chkPsdPlot = new QCheckBox(trSig("Plot PSD Result"), tabPsd);
    chkPsdPlot->setChecked(true);
    fPsd->addRow(trSig("Curve:"), cbPsdCurve);
    fPsd->addRow(trSig("Segment Length:"), sbPsdSeg);
    fPsd->addRow(QString(), chkPsdPlot);
    tabs->addTab(tabPsd, trSig("Welch PSD"));

    QWidget *tabCoh = new QWidget(&dlg);
    QFormLayout *fCoh = new QFormLayout(tabCoh);
    QComboBox *cbCohA = new QComboBox(tabCoh);
    cbCohA->addItems(curves);
    QComboBox *cbCohB = new QComboBox(tabCoh);
    cbCohB->addItems(curves);
    if (curves.size() >= 2)
        cbCohB->setCurrentIndex(1);
    QSpinBox *sbCohSeg = new QSpinBox(tabCoh);
    sbCohSeg->setRange(8, 65536);
    sbCohSeg->setSingleStep(32);
    sbCohSeg->setValue(128);
    QCheckBox *chkCohPlot = new QCheckBox(trSig("Plot Coherence Result"), tabCoh);
    chkCohPlot->setChecked(true);
    fCoh->addRow(trSig("Signal A:"), cbCohA);
    fCoh->addRow(trSig("Signal B:"), cbCohB);
    fCoh->addRow(trSig("Segment Length:"), sbCohSeg);
    fCoh->addRow(QString(), chkCohPlot);
    tabs->addTab(tabCoh, trSig("Cross-Coherence"));

    vbox->addWidget(tabs);
    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        if (tabs->currentIndex() == 0) {
            welchPSD(app, g, cbPsdCurve->currentText(), sbPsdSeg->value(), chkPsdPlot->isChecked());
        } else {
            crossCoherence(app, g, cbCohA->currentText(), cbCohB->currentText(), sbCohSeg->value(),
                           chkCohPlot->isChecked());
        }
    }
}
