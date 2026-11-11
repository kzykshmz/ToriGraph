/***************************************************************************
    File                 : NonLinearFit.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : NonLinearFit class

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
#include "NonLinearFit.h"
#include "MyParser.h"
#include "fit_gsl.h"

#include <QMessageBox>
#include <algorithm>
#include <cmath>
#include <numeric>
using namespace std;

namespace {

struct OriginModelSpec
{
    const char *name;
    const char *category;
    const char *params;
    const char *formula;
    const char *description;
};

static const OriginModelSpec kOriginModels[] = {
    // 1. Peak Functions (14)
    { "Gauss", "Peak Functions", "A, xc, w, y0",
      "y0+A*sqrt(2/PI)/w*exp(-2*((x-xc)/w)^2)",
      "Gaussian Peak (Area parameterization)" },
    { "GaussAmp", "Peak Functions", "y0, A, xc, w",
      "y0+A*exp(-(x-xc)*(x-xc)/(2*w*w))",
      "Gaussian Peak (Amplitude parameterization)" },
    { "Lorentz", "Peak Functions", "A, xc, w, y0",
      "y0+2*A/PI*w/(4*(x-xc)^2+w^2)",
      "Lorentzian Peak (Area parameterization)" },
    { "PseudoVoigt1", "Peak Functions", "A, xc, w, mu, y0",
      "y0+A*(mu*2/PI*w/(4*(x-xc)^2+w^2)+(1-mu)*sqrt(4*ln(2))/(sqrt(PI)*w)*exp(-4*ln(2)*((x-xc)/w)^2))",
      "Pseudo-Voigt Peak with shared FWHM w and Lorentz fraction mu" },
    { "PseudoVoigt2", "Peak Functions", "A, xc, wG, wL, mu, y0",
      "y0+A*(mu*2/PI*wL/(4*(x-xc)^2+wL^2)+(1-mu)*sqrt(4*ln(2))/(sqrt(PI)*wG)*exp(-4*ln(2)*((x-xc)/wG)^2))",
      "Pseudo-Voigt Peak with separate Gaussian (wG) and Lorentzian (wL) widths" },
    { "VoigtApprox", "Peak Functions", "A, xc, w, mu, y0",
      "y0+A*(mu*2/PI*w/(4*(x-xc)^2+w^2)+(1-mu)*sqrt(4*ln(2))/(sqrt(PI)*w)*exp(-4*ln(2)*((x-xc)/w)^2))",
      "Thompson-Cox-Hastings Voigt Approximation Peak" },
    { "PearsonVII", "Peak Functions", "A, xc, w, m, y0",
      "y0+A*(1+(2^(1/m)-1)*(2*(x-xc)/w)^2)^(-m)",
      "Pearson VII XRD Diffraction Peak" },
    { "BiGaussian", "Peak Functions", "y0, A, xc, w1, w2",
      "y0+((x<xc)?(A*exp(-(x-xc)^2/(2*w1^2))):(A*exp(-(x-xc)^2/(2*w2^2))))",
      "Asymmetric Bifurcated Gaussian Peak (left width w1, right width w2)" },
    { "EMG", "Peak Functions", "y0, A, xc, w, t0",
      "y0+(A/(2*t0))*exp(0.5*(w/t0)^2-(x-xc)/t0)*erfc((w/t0-(x-xc)/w)/sqrt(2))",
      "Exponentially Modified Gaussian Peak (Chromatography / Spectroscopy)" },
    { "LogNormal", "Peak Functions", "A, xc, w, y0",
      "y0+A/(sqrt(2*PI)*w*x)*exp(-(ln(x/xc))^2/(2*w^2))",
      "Log-Normal Asymmetric Peak Distribution" },
    { "Asym2Sig", "Peak Functions", "y0, A, xc, w1, w2, w3",
      "y0+A*(1/(1+exp(-(x-xc+w1/2)/w2)))*(1-1/(1+exp(-(x-xc-w1/2)/w3)))",
      "Asymmetric Double Sigmoidal Peak" },
    { "DoniachSunjic", "Peak Functions", "y0, A, xc, w, alpha",
      "y0+A*cos(PI*alpha/2+(1-alpha)*atan((x-xc)/(w/2)))/(1+4*((x-xc)/w)^2)^((1-alpha)/2)",
      "Doniach-Sunjic Asymmetric XPS Core-Level Peak" },
    { "Extreme", "Peak Functions", "y0, A, xc, w",
      "y0+A*exp(-exp(-(x-xc)/w)-(x-xc)/w+1)",
      "Extreme Value (Gumbel) Asymmetric Peak" },
    { "GramCharlier", "Peak Functions", "y0, A, xc, w, a3, a4",
      "y0+A/(sqrt(2*PI)*w)*exp(-0.5*((x-xc)/w)^2)*(1+a3/6*(((x-xc)/w)^3-3*(x-xc)/w)+a4/24*(((x-xc)/w)^4-6*((x-xc)/w)^2+3))",
      "Gram-Charlier Edgeworth Series Peak" },

    // 2. Exponential (9)
    { "ExpDec1", "Exponential", "A, t, y0",
      "y0+A*exp(-x/t)",
      "First-Order Exponential Decay" },
    { "ExpDec2", "Exponential", "A1, t1, A2, t2, y0",
      "y0+A1*exp(-x/t1)+A2*exp(-x/t2)",
      "Second-Order (Two-Phase) Exponential Decay" },
    { "ExpDec3", "Exponential", "A1, t1, A2, t2, A3, t3, y0",
      "y0+A1*exp(-x/t1)+A2*exp(-x/t2)+A3*exp(-x/t3)",
      "Third-Order (Three-Phase) Exponential Decay" },
    { "ExpGrow1", "Exponential", "A, t, y0",
      "y0+A*exp(x/t)",
      "First-Order Exponential Growth" },
    { "ExpGrow2", "Exponential", "y0, A1, t1, A2, t2",
      "y0+A1*exp(x/t1)+A2*exp(x/t2)",
      "Second-Order (Two-Phase) Exponential Growth" },
    { "ExpAssoc", "Exponential", "y0, A1, t1, A2, t2",
      "y0+A1*(1-exp(-x/t1))+A2*(1-exp(-x/t2))",
      "Two-Phase Exponential Association" },
    { "BoxLucas1", "Exponential", "a, b",
      "a*(1-exp(-b*x))",
      "Box-Lucas Exponential Asymptotic Model" },
    { "Strexp (KWW)", "Exponential", "y0, A, tau, beta",
      "y0+A*exp(-(x/tau)^beta)",
      "Stretched Exponential (Kohlrausch-Williams-Watts) Relaxation" },
    { "Chapman", "Exponential", "a, b, c",
      "a*(1-exp(-b*x))^c",
      "Chapman-Richards Exponential Growth" },

    // 3. Sigmoidal / Growth (8)
    { "Boltzmann", "Sigmoidal / Growth", "A1, A2, x0, dx",
      "(A1-A2)/(1+exp((x-x0)/dx))+A2",
      "Boltzmann Sigmoidal Transition Function" },
    { "Logistic", "Sigmoidal / Growth", "A1, A2, x0, p",
      "A2+(A1-A2)/(1+(x/x0)^p)",
      "Four-Parameter Logistic Sigmoidal Dose-Response" },
    { "DoseResp (Hill EC50)", "Sigmoidal / Growth", "A1, A2, logx0, p",
      "A1+(A2-A1)/(1+10^((logx0-x)*p))",
      "Dose-Response Sigmoid with Log(EC50) and Hill Slope" },
    { "Gompertz", "Sigmoidal / Growth", "a, xc, k",
      "a*exp(-exp(-k*(x-xc)))",
      "Gompertz Asymmetric Sigmoidal Growth Model" },
    { "Hill", "Sigmoidal / Growth", "Vmax, k, n",
      "Vmax*x^n/(k^n+x^n)",
      "Cooperative Hill Kinetics / Binding Isotherm" },
    { "Richards", "Sigmoidal / Growth", "a, xc, d, k",
      "a*(1+(d-1)*exp(-k*(x-xc)))^(1/(1-d))",
      "Richards Generalized Logistic Growth Function" },
    { "MMF", "Sigmoidal / Growth", "a, b, c, d",
      "(a*b+c*x^d)/(b+x^d)",
      "Morgan-Mercer-Flodin Sigmoidal Growth Model" },
    { "WeibullGrowth", "Sigmoidal / Growth", "a, b, d, k",
      "a-(a-b)*exp(-(k*x)^d)",
      "Weibull Cumulative Growth Distribution" },

    // 4. Materials / Metallurgy / Physics (13)
    { "Arrhenius", "Materials / Metallurgy / Physics", "A, Ea",
      "A*exp(-Ea/(8.3144626*x))",
      "Arrhenius Thermally Activated Rate (T in K, Ea in J/mol)" },
    { "VTF (Vogel-Tammann-Fulcher)", "Materials / Metallurgy / Physics", "A, B, T0",
      "A*exp(B/(x-T0))",
      "Vogel-Tammann-Fulcher Supercooled Liquid / Glass Relaxation" },
    { "Avrami (JMAK)", "Materials / Metallurgy / Physics", "k, n",
      "1-exp(-k*x^n)",
      "Johnson-Mehl-Avrami-Kolmogorov Phase Transformation Kinetics" },
    { "HallPetch", "Materials / Metallurgy / Physics", "sigma0, ky",
      "sigma0+ky/sqrt(x)",
      "Hall-Petch Grain Size Strengthening Law (d in x)" },
    { "Hollomon", "Materials / Metallurgy / Physics", "K, n",
      "K*x^n",
      "Hollomon Power-Law Strain Hardening (sigma = K * eps^n)" },
    { "Ludwik", "Materials / Metallurgy / Physics", "sigma0, K, n",
      "sigma0+K*x^n",
      "Ludwik Strain Hardening with Yield Stress Offset" },
    { "Voce", "Materials / Metallurgy / Physics", "sigmaS, sigma0, epsC",
      "sigmaS-(sigmaS-sigma0)*exp(-x/epsC)",
      "Voce Exponential Saturation Strain Hardening" },
    { "Swift", "Materials / Metallurgy / Physics", "K, eps0, n",
      "K*(eps0+x)^n",
      "Swift Pre-Strained Power-Law Work Hardening" },
    { "ParisLaw", "Materials / Metallurgy / Physics", "C, m",
      "C*x^m",
      "Paris-Erdogan Fatigue Crack Growth Law (da/dN = C * dK^m)" },
    { "Langmuir", "Materials / Metallurgy / Physics", "qmax, K",
      "qmax*K*x/(1+K*x)",
      "Langmuir Monolayer Adsorption Isotherm" },
    { "Freundlich", "Materials / Metallurgy / Physics", "Kf, n",
      "Kf*x^(1/n)",
      "Freundlich Empirical Adsorption Isotherm" },
    { "MichaelisMenten", "Materials / Metallurgy / Physics", "Vmax, Km",
      "Vmax*x/(Km+x)",
      "Michaelis-Menten Enzyme / Reaction Kinetics" },
    { "Planck", "Materials / Metallurgy / Physics", "A, B, T",
      "A/(x^5*(exp(B/(x*T))-1))",
      "Planck Blackbody Spectral Radiance Law" },

    // 5. Waveform / Periodic (4)
    { "Sine", "Waveform / Periodic", "y0, A, xc, w",
      "y0+A*sin(PI*(x-xc)/w)",
      "Standard Sine Wave Function" },
    { "SineDamp", "Waveform / Periodic", "y0, A, xc, w, t0",
      "y0+A*exp(-x/t0)*sin(PI*(x-xc)/w)",
      "Exponentially Damped Sine Wave" },
    { "SineSq", "Waveform / Periodic", "y0, A, xc, w",
      "y0+A*(sin(PI*(x-xc)/w))^2",
      "Squared Sine Wave Function" },
    { "GaussianEnvelopeCosine", "Waveform / Periodic", "y0, A, xc, w, lambda, phi",
      "y0+A*exp(-((x-xc)/w)^2)*cos(2*PI*(x-xc)/lambda+phi)",
      "Gaussian-Envelope Cosine Wavepacket" },

    // 6. Power / Logarithmic / Rational / Polynomial (10)
    { "Allometric1", "Power / Logarithmic / Rational / Polynomial", "a, b",
      "a*x^b",
      "Classical Allometric Power Scaling Law" },
    { "Allometric2", "Power / Logarithmic / Rational / Polynomial", "y0, a, b",
      "y0+a*x^b",
      "Offset Allometric Power Scaling Law" },
    { "Logarithm", "Power / Logarithmic / Rational / Polynomial", "a, b",
      "a+b*ln(x)",
      "Natural Logarithmic Function" },
    { "Hyperbl", "Power / Logarithmic / Rational / Polynomial", "P1, P2",
      "P1*x/(P2+x)",
      "Two-Parameter Rectangular Hyperbola" },
    { "Rational0", "Power / Logarithmic / Rational / Polynomial", "a, b, c",
      "(b+c*x)/(1+a*x)",
      "First-Order Linear Rational Function" },
    { "Rational1", "Power / Logarithmic / Rational / Polynomial", "a0, a1, b1, b2",
      "(a0+a1*x)/(1+b1*x+b2*x^2)",
      "Linear-over-Quadratic Rational Function" },
    { "Parabola", "Power / Logarithmic / Rational / Polynomial", "a0, a1, a2",
      "a0+a1*x+a2*x^2",
      "Second-Order Polynomial (Parabola)" },
    { "Cubic", "Power / Logarithmic / Rational / Polynomial", "a0, a1, a2, a3",
      "a0+a1*x+a2*x^2+a3*x^3",
      "Third-Order Cubic Polynomial" },
    { "Poly4", "Power / Logarithmic / Rational / Polynomial", "a0, a1, a2, a3, a4",
      "a0+a1*x+a2*x^2+a3*x^3+a4*x^4",
      "Fourth-Order Quartic Polynomial" },
    { "Poly5", "Power / Logarithmic / Rational / Polynomial", "a0, a1, a2, a3, a4, a5",
      "a0+a1*x+a2*x^2+a3*x^3+a4*x^4+a5*x^5",
      "Fifth-Order Quintic Polynomial" }
};

static const int kNumOriginModels = int(sizeof(kOriginModels) / sizeof(kOriginModels[0]));

static QString normalizeModelKey(const QString &name)
{
    QString k = name.trimmed();
    if (k.compare("ExpDecay1", Qt::CaseInsensitive) == 0)
        return "ExpDec1";
    if (k.compare("ExpDecay2", Qt::CaseInsensitive) == 0)
        return "ExpDec2";
    if (k.compare("ExpDecay3", Qt::CaseInsensitive) == 0)
        return "ExpDec3";
    if (k.compare("ExpGrowth", Qt::CaseInsensitive) == 0)
        return "ExpGrow1";
    if (k.compare("PseudoVoigt", Qt::CaseInsensitive) == 0)
        return "PseudoVoigt1";
    if (k.compare("Voigt", Qt::CaseInsensitive) == 0)
        return "VoigtApprox";
    if (k.compare("Strexp", Qt::CaseInsensitive) == 0)
        return "Strexp (KWW)";
    if (k.compare("DoseResp", Qt::CaseInsensitive) == 0)
        return "DoseResp (Hill EC50)";
    if (k.compare("VTF", Qt::CaseInsensitive) == 0)
        return "VTF (Vogel-Tammann-Fulcher)";
    if (k.compare("Avrami", Qt::CaseInsensitive) == 0
        || k.compare("JMAK", Qt::CaseInsensitive) == 0)
        return "Avrami (JMAK)";
    if (k.compare("Polynomial", Qt::CaseInsensitive) == 0)
        return "Parabola";
    return k;
}

static const OriginModelSpec *findOriginModel(const QString &modelName)
{
    QString key = normalizeModelKey(modelName);
    for (int i = 0; i < kNumOriginModels; ++i) {
        if (key.compare(QLatin1String(kOriginModels[i].name), Qt::CaseInsensitive) == 0)
            return &kOriginModels[i];
    }
    for (int i = 0; i < kNumOriginModels; ++i) {
        if (QString::fromLatin1(kOriginModels[i].name).startsWith(key, Qt::CaseInsensitive)
            && !key.isEmpty())
            return &kOriginModels[i];
    }
    return nullptr;
}

static void linearRegression(const std::vector<double> &u, const std::vector<double> &v,
                             double &intercept, double &slope)
{
    intercept = 0.0;
    slope = 1.0;
    const size_t n = u.size();
    if (n < 2 || v.size() != n)
        return;
    double mu = std::accumulate(u.begin(), u.end(), 0.0) / double(n);
    double mv = std::accumulate(v.begin(), v.end(), 0.0) / double(n);
    double suu = 0.0, suv = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double du = u[i] - mu;
        double dv = v[i] - mv;
        suu += du * du;
        suv += du * dv;
    }
    if (suu > 1e-18) {
        slope = suv / suu;
        intercept = mv - slope * mu;
    } else {
        intercept = mv;
        slope = 0.0;
    }
}

} // namespace

NonLinearFit::NonLinearFit(ApplicationWindow *parent, Graph *g) : Fit(parent, g)
{
    init();
}

NonLinearFit::NonLinearFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle);
}

NonLinearFit::NonLinearFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle,
                           double start, double end)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

void NonLinearFit::init()
{
    setObjectName(tr("NonLinear"));
    d_formula = QString();
    d_f = user_f;
    d_df = user_df;
    d_fdf = user_fdf;
    d_fsimplex = user_d;
    d_explanation = tr("Non-linear");
    d_init_err = false;
}

void NonLinearFit::setFormula(const QString &s)
{
    d_formula = s;
}

void NonLinearFit::setParametersList(const QStringList &lst)
{
    if ((int)lst.count() < 1) {
        QMessageBox::critical((ApplicationWindow *)parent(), tr("Fit Error"),
                              tr("You must provide a list containing at least one parameter for "
                                 "this type of fit. Operation aborted!"));
        d_init_err = true;
        return;
    }

    d_init_err = false;
    d_param_names = lst;

    if (d_p > 0) { // free previously allocated memory
        gsl_vector_free(d_param_init);
        gsl_matrix_free(covar);
    }

    d_p = (int)lst.count();
    d_min_points = d_p;
    d_param_init = gsl_vector_alloc(d_p);
    gsl_vector_set_all(d_param_init, 1.0);

    covar = gsl_matrix_alloc(d_p, d_p);
    d_results.resize(d_p);

    for (unsigned i = 0; i < d_p; i++)
        d_param_explain << "";
    d_initial_guesses_set = false;
}

QStringList NonLinearFit::builtInModelNames()
{
    QStringList names;
    for (int i = 0; i < kNumOriginModels; ++i)
        names << QString::fromLatin1(kOriginModels[i].name);
    return names;
}

QString NonLinearFit::builtInModelFormula(const QString &modelName)
{
    const OriginModelSpec *spec = findOriginModel(modelName);
    return spec ? QString::fromLatin1(spec->formula) : QString();
}

QStringList NonLinearFit::builtInModelParameters(const QString &modelName)
{
    const OriginModelSpec *spec = findOriginModel(modelName);
    if (!spec)
        return QStringList();
    QString raw = QString::fromLatin1(spec->params);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return raw.split(QRegExp("[,;\\s]+"), Qt::SkipEmptyParts);
#else
    return raw.split(QRegExp("[,;\\s]+"), QString::SkipEmptyParts);
#endif
}

QString NonLinearFit::builtInModelCategory(const QString &modelName)
{
    const OriginModelSpec *spec = findOriginModel(modelName);
    return spec ? QString::fromLatin1(spec->category) : QString();
}

QString NonLinearFit::builtInModelDescription(const QString &modelName)
{
    const OriginModelSpec *spec = findOriginModel(modelName);
    return spec ? QString::fromLatin1(spec->description) : QString();
}

bool NonLinearFit::setBuiltInModel(const QString &modelName)
{
    const OriginModelSpec *spec = findOriginModel(modelName);
    if (!spec)
        return false;
    d_model_name = QString::fromLatin1(spec->name);
    d_explanation = d_model_name + " " + tr("Fit");
    setParametersList(builtInModelParameters(d_model_name));
    setFormula(QString::fromLatin1(spec->formula));
    if (!d_x.empty() && d_y.size() == d_x.size())
        guessBuiltInParameters(d_model_name);
    return true;
}

void NonLinearFit::guessInitialValues()
{
    guessBuiltInParameters(d_model_name);
}

void NonLinearFit::guessBuiltInParameters(const QString &modelName)
{
    const size_t n = d_x.size();
    if (n < 2 || d_y.size() != n || d_p == 0 || !d_param_init)
        return;

    QString key = normalizeModelKey(!modelName.isEmpty() ? modelName : d_model_name);
    if (key.isEmpty()) {
        // Infer model from d_formula / d_param_names if setBuiltInModel wasn't called directly
        QString f = d_formula.simplified().remove(' ');
        if (f.contains("exp(-Ea/", Qt::CaseInsensitive) || d_param_names.contains("Ea"))
            key = "Arrhenius";
        else if (f.contains("(A1-A2)/(1+exp((x-x0)/dx))") || (d_param_names.contains("A1") && d_param_names.contains("dx")))
            key = "Boltzmann";
        else if (d_param_names.size() == 2 && d_param_names.contains("K") && d_param_names.contains("n"))
            key = "Hollomon";
        else {
            for (int i = 0; i < kNumOriginModels; ++i) {
                if (f.compare(QString::fromLatin1(kOriginModels[i].formula).remove(' '),
                              Qt::CaseInsensitive) == 0) {
                    key = QString::fromLatin1(kOriginModels[i].name);
                    break;
                }
            }
        }
    }

    auto setByName = [this](const QString &pName, double val) {
        for (unsigned i = 0; i < d_p && int(i) < d_param_names.size(); ++i) {
            if (d_param_names[i].compare(pName, Qt::CaseInsensitive) == 0) {
                gsl_vector_set(d_param_init, i, val);
                return true;
            }
        }
        return false;
    };

    double x_min = *std::min_element(d_x.begin(), d_x.end());
    double x_max = *std::max_element(d_x.begin(), d_x.end());
    double x_span = std::max(1e-9, x_max - x_min);
    double x_mid = 0.5 * (x_min + x_max);

    auto minIt = std::min_element(d_y.begin(), d_y.end());
    auto maxIt = std::max_element(d_y.begin(), d_y.end());
    size_t maxIdx = size_t(std::distance(d_y.begin(), maxIt));
    double y_min = *minIt;
    double y_max = *maxIt;
    double y_span = std::max(1e-9, y_max - y_min);
    double y_front = d_y.front();
    double y_back = d_y.back();

    // Peak shape characteristics around maxIdx
    double xc_peak = d_x[maxIdx];
    double halfY = y_min + 0.5 * y_span;
    double xLeft = x_min, xRight = x_max;
    for (int k = int(maxIdx); k > 0; --k) {
        if (d_y[k - 1] <= halfY) {
            double dy = d_y[k] - d_y[k - 1];
            double frac = (std::abs(dy) > 1e-15) ? ((halfY - d_y[k - 1]) / dy) : 0.5;
            xLeft = d_x[k - 1] + std::max(0.0, std::min(1.0, frac)) * (d_x[k] - d_x[k - 1]);
            break;
        }
    }
    for (size_t k = maxIdx; k + 1 < n; ++k) {
        if (d_y[k + 1] <= halfY) {
            double dy = d_y[k] - d_y[k + 1];
            double frac = (std::abs(dy) > 1e-15) ? ((d_y[k] - halfY) / dy) : 0.5;
            xRight = d_x[k] + std::max(0.0, std::min(1.0, frac)) * (d_x[k + 1] - d_x[k]);
            break;
        }
    }
    double fwhm = std::max(1e-6, std::abs(xRight - xLeft));
    if (fwhm >= 0.95 * x_span)
        fwhm = 0.2 * x_span;

    // 1. Arrhenius: y = A * exp(-Ea / (R * x))
    if (key.compare("Arrhenius", Qt::CaseInsensitive) == 0) {
        double R_eff = 1.0;
        if (d_formula.contains("8.3144626"))
            R_eff = 8.3144626;
        else if (d_formula.contains("8.314"))
            R_eff = 8.314;
        else if (d_formula.contains("1.987"))
            R_eff = 1.987;
        else if (d_formula.contains("8.617"))
            R_eff = 8.617333e-5;

        std::vector<double> u, v;
        for (size_t i = 0; i < n; ++i) {
            if (d_x[i] > 1e-12 && d_y[i] > 1e-30) {
                u.push_back(1.0 / d_x[i]);
                v.push_back(std::log(d_y[i]));
            }
        }
        double c = 0.0, m = -1000.0;
        if (u.size() >= 2)
            linearRegression(u, v, c, m);
        double A_guess = std::exp(std::max(-100.0, std::min(100.0, c)));
        double Ea_guess = -m * R_eff;
        if (!setByName("A", A_guess) && d_p >= 1)
            gsl_vector_set(d_param_init, 0, A_guess);
        if (!setByName("Ea", Ea_guess) && d_p >= 2)
            gsl_vector_set(d_param_init, 1, Ea_guess);
        d_initial_guesses_set = true;
        return;
    }

    // 2. Power laws: Hollomon (K*x^n), Allometric1 (a*x^b), ParisLaw (C*x^m), Freundlich (Kf*x^(1/n))
    if (key.compare("Hollomon", Qt::CaseInsensitive) == 0
        || key.compare("Allometric1", Qt::CaseInsensitive) == 0
        || key.compare("ParisLaw", Qt::CaseInsensitive) == 0
        || key.compare("Freundlich", Qt::CaseInsensitive) == 0) {
        std::vector<double> u, v;
        for (size_t i = 0; i < n; ++i) {
            if (d_x[i] > 1e-12 && d_y[i] > 1e-30) {
                u.push_back(std::log(d_x[i]));
                v.push_back(std::log(d_y[i]));
            }
        }
        double c = 0.0, m = 0.5;
        if (u.size() >= 2)
            linearRegression(u, v, c, m);
        double K_guess = std::exp(std::max(-100.0, std::min(100.0, c)));
        double n_guess = (key.compare("Freundlich", Qt::CaseInsensitive) == 0)
                ? ((std::abs(m) > 1e-6) ? (1.0 / m) : 2.0)
                : m;
        if (d_p >= 1)
            gsl_vector_set(d_param_init, 0, K_guess);
        if (d_p >= 2)
            gsl_vector_set(d_param_init, 1, n_guess);
        setByName("K", K_guess);
        setByName("a", K_guess);
        setByName("C", K_guess);
        setByName("Kf", K_guess);
        setByName("n", n_guess);
        setByName("b", n_guess);
        setByName("m", n_guess);
        d_initial_guesses_set = true;
        return;
    }

    // 3. Ludwik / Allometric2 / Swift
    if (key.compare("Ludwik", Qt::CaseInsensitive) == 0
        || key.compare("Allometric2", Qt::CaseInsensitive) == 0
        || key.compare("Swift", Qt::CaseInsensitive) == 0) {
        double y0_g = y_front;
        std::vector<double> u, v;
        for (size_t i = 0; i < n; ++i) {
            double dy = d_y[i] - y0_g;
            if (d_x[i] > 1e-9 && dy > 1e-9) {
                u.push_back(std::log(d_x[i]));
                v.push_back(std::log(dy));
            }
        }
        double c = std::log(std::max(1.0, y_span)), m = 0.4;
        if (u.size() >= 2)
            linearRegression(u, v, c, m);
        double K_g = std::exp(std::max(-50.0, std::min(50.0, c)));
        setByName("sigma0", y0_g);
        setByName("y0", y0_g);
        setByName("K", K_g);
        setByName("a", K_g);
        setByName("eps0", 0.01 * std::max(1e-4, x_span));
        setByName("n", std::max(0.05, m));
        setByName("b", m);
        d_initial_guesses_set = true;
        return;
    }

    // 4. Boltzmann & Sigmoidal functions
    if (key.compare("Boltzmann", Qt::CaseInsensitive) == 0
        || key.compare("Logistic", Qt::CaseInsensitive) == 0
        || key.compare("DoseResp (Hill EC50)", Qt::CaseInsensitive) == 0) {
        double A1_g = y_front;
        double A2_g = y_back;
        if (std::abs(A1_g - A2_g) < 0.1 * y_span) {
            A1_g = y_min;
            A2_g = y_max;
        }
        double y_half = 0.5 * (A1_g + A2_g);
        double x0_g = x_mid;
        for (size_t i = 0; i + 1 < n; ++i) {
            if ((d_y[i] - y_half) * (d_y[i + 1] - y_half) <= 0.0) {
                double dy = d_y[i + 1] - d_y[i];
                double f = (std::abs(dy) > 1e-15) ? ((y_half - d_y[i]) / dy) : 0.5;
                x0_g = d_x[i] + std::max(0.0, std::min(1.0, f)) * (d_x[i + 1] - d_x[i]);
                break;
            }
        }
        double y25 = A2_g + 0.25 * (A1_g - A2_g);
        double y75 = A2_g + 0.75 * (A1_g - A2_g);
        double x25 = x0_g + 0.1 * x_span, x75 = x0_g - 0.1 * x_span;
        for (size_t i = 0; i + 1 < n; ++i) {
            if ((d_y[i] - y25) * (d_y[i + 1] - y25) <= 0.0)
                x25 = 0.5 * (d_x[i] + d_x[i + 1]);
            if ((d_y[i] - y75) * (d_y[i + 1] - y75) <= 0.0)
                x75 = 0.5 * (d_x[i] + d_x[i + 1]);
        }
        double dx_g = std::max(1e-6, std::abs(x25 - x75) / (2.0 * std::log(3.0)));
        if (dx_g > 0.5 * x_span || dx_g < 1e-6 * x_span)
            dx_g = 0.08 * x_span;

        setByName("A1", A1_g);
        setByName("A2", A2_g);
        setByName("x0", x0_g);
        setByName("logx0", x0_g);
        setByName("dx", dx_g);
        setByName("p", (y_back >= y_front) ? 2.0 : 2.0);
        d_initial_guesses_set = true;
        return;
    }

    // 5. Gompertz / Richards / WeibullGrowth / MMF / Hill
    if (key.compare("Gompertz", Qt::CaseInsensitive) == 0
        || key.compare("Richards", Qt::CaseInsensitive) == 0
        || key.compare("WeibullGrowth", Qt::CaseInsensitive) == 0
        || key.compare("MMF", Qt::CaseInsensitive) == 0
        || key.compare("Hill", Qt::CaseInsensitive) == 0) {
        double y_half = 0.5 * (y_min + y_max);
        double x50 = std::max(1e-6, x_mid);
        for (size_t i = 0; i + 1 < n; ++i) {
            if ((d_y[i] - y_half) * (d_y[i + 1] - y_half) <= 0.0) {
                x50 = 0.5 * (d_x[i] + d_x[i + 1]);
                break;
            }
        }
        setByName("a", y_max);
        setByName("b", (key.compare("MMF", Qt::CaseInsensitive) == 0) ? std::pow(std::max(1e-6, x50), 2.0) : y_min);
        setByName("c", y_max);
        setByName("d", 2.0);
        setByName("xc", x50);
        setByName("k", (key.compare("Hill", Qt::CaseInsensitive) == 0) ? std::max(1e-6, x50) : (4.0 / x_span));
        setByName("Vmax", y_max * 1.05);
        setByName("n", 2.0);
        d_initial_guesses_set = true;
        return;
    }

    // 6. HallPetch: sigma0 + ky / sqrt(x)
    if (key.compare("HallPetch", Qt::CaseInsensitive) == 0) {
        std::vector<double> u, v;
        for (size_t i = 0; i < n; ++i) {
            if (d_x[i] > 1e-12) {
                u.push_back(1.0 / std::sqrt(d_x[i]));
                v.push_back(d_y[i]);
            }
        }
        double s0 = y_min, ky = y_span;
        if (u.size() >= 2)
            linearRegression(u, v, s0, ky);
        setByName("sigma0", s0);
        setByName("ky", ky);
        d_initial_guesses_set = true;
        return;
    }

    // 7. VTF: A * exp(B / (x - T0))
    if (key.compare("VTF (Vogel-Tammann-Fulcher)", Qt::CaseInsensitive) == 0) {
        double T0_g = x_min - 0.15 * x_span;
        std::vector<double> u, v;
        for (size_t i = 0; i < n; ++i) {
            if (d_x[i] > T0_g + 1e-6 && d_y[i] > 1e-30) {
                u.push_back(1.0 / (d_x[i] - T0_g));
                v.push_back(std::log(d_y[i]));
            }
        }
        double c = 0.0, B_g = 100.0;
        if (u.size() >= 2)
            linearRegression(u, v, c, B_g);
        setByName("A", std::exp(std::max(-80.0, std::min(80.0, c))));
        setByName("B", B_g);
        setByName("T0", T0_g);
        d_initial_guesses_set = true;
        return;
    }

    // 8. Avrami (JMAK): 1 - exp(-k * x^n)
    if (key.compare("Avrami (JMAK)", Qt::CaseInsensitive) == 0) {
        std::vector<double> u, v;
        for (size_t i = 0; i < n; ++i) {
            if (d_x[i] > 1e-9 && d_y[i] > 0.01 && d_y[i] < 0.99) {
                u.push_back(std::log(d_x[i]));
                v.push_back(std::log(-std::log(1.0 - d_y[i])));
            }
        }
        double c = -1.0, n_g = 2.0;
        if (u.size() >= 2)
            linearRegression(u, v, c, n_g);
        setByName("k", std::exp(std::max(-50.0, std::min(50.0, c))));
        setByName("n", std::max(0.2, n_g));
        d_initial_guesses_set = true;
        return;
    }

    // 9. Voce: sigmaS - (sigmaS - sigma0) * exp(-x / epsC)
    if (key.compare("Voce", Qt::CaseInsensitive) == 0) {
        setByName("sigmaS", y_max);
        setByName("sigma0", y_front);
        setByName("epsC", 0.3 * x_span);
        d_initial_guesses_set = true;
        return;
    }

    // 10. Langmuir / MichaelisMenten / Hyperbl
    if (key.compare("Langmuir", Qt::CaseInsensitive) == 0
        || key.compare("MichaelisMenten", Qt::CaseInsensitive) == 0
        || key.compare("Hyperbl", Qt::CaseInsensitive) == 0) {
        double ymax_g = y_max * 1.1;
        double x_half = std::max(1e-6, 0.3 * x_max);
        for (size_t i = 0; i + 1 < n; ++i) {
            if ((d_y[i] - 0.5 * y_max) * (d_y[i + 1] - 0.5 * y_max) <= 0.0) {
                x_half = std::max(1e-6, 0.5 * (d_x[i] + d_x[i + 1]));
                break;
            }
        }
        setByName("qmax", ymax_g);
        setByName("K", 1.0 / x_half);
        setByName("Vmax", ymax_g);
        setByName("Km", x_half);
        setByName("P1", ymax_g);
        setByName("P2", x_half);
        d_initial_guesses_set = true;
        return;
    }

    // 11. Exponential functions
    if (key.startsWith("Exp", Qt::CaseInsensitive)
        || key.compare("BoxLucas1", Qt::CaseInsensitive) == 0
        || key.compare("Strexp (KWW)", Qt::CaseInsensitive) == 0
        || key.compare("Chapman", Qt::CaseInsensitive) == 0) {
        bool isGrow = key.contains("Grow", Qt::CaseInsensitive);
        double y0_g = isGrow ? y_front : y_back;
        double A_g = isGrow ? std::max(1e-6, (y_back - y_front) * 0.1) : (y_front - y_back);
        if (std::abs(A_g) < 1e-9)
            A_g = y_span;
        double t_g = std::max(1e-6, 0.35 * x_span);
        setByName("y0", y0_g);
        setByName("A", A_g);
        setByName("t", t_g);
        setByName("A1", 0.6 * A_g);
        setByName("t1", 0.2 * x_span);
        setByName("A2", 0.4 * A_g);
        setByName("t2", 0.6 * x_span);
        setByName("A3", 0.2 * A_g);
        setByName("t3", 1.2 * x_span);
        setByName("a", y_max);
        setByName("b", 1.0 / t_g);
        setByName("c", 1.5);
        setByName("tau", t_g);
        setByName("beta", 0.8);
        d_initial_guesses_set = true;
        return;
    }

    // 12. Logarithm: a + b * ln(x)
    if (key.compare("Logarithm", Qt::CaseInsensitive) == 0) {
        std::vector<double> u, v;
        for (size_t i = 0; i < n; ++i) {
            if (d_x[i] > 1e-12) {
                u.push_back(std::log(d_x[i]));
                v.push_back(d_y[i]);
            }
        }
        double a_g = y_front, b_g = y_span / std::max(1.0, std::log(std::max(2.0, x_max / std::max(1e-6, x_min))));
        if (u.size() >= 2)
            linearRegression(u, v, a_g, b_g);
        setByName("a", a_g);
        setByName("b", b_g);
        d_initial_guesses_set = true;
        return;
    }

    // 13. Waveform / Periodic
    if (key.startsWith("Sine", Qt::CaseInsensitive)
        || key.compare("GaussianEnvelopeCosine", Qt::CaseInsensitive) == 0) {
        double mean_y = std::accumulate(d_y.begin(), d_y.end(), 0.0) / double(n);
        int crossings = 0;
        for (size_t i = 0; i + 1 < n; ++i) {
            if ((d_y[i] - mean_y) * (d_y[i + 1] - mean_y) < 0.0)
                crossings++;
        }
        double w_g = (crossings >= 2) ? (x_span / double(crossings)) : (0.25 * x_span);
        setByName("y0", mean_y);
        setByName("A", 0.5 * y_span);
        setByName("xc", xc_peak);
        setByName("w", w_g);
        setByName("t0", 0.5 * x_span);
        setByName("lambda", 2.0 * w_g);
        setByName("phi", 0.0);
        d_initial_guesses_set = true;
        return;
    }

    // 14. Polynomial / Rational
    if (key.compare("Parabola", Qt::CaseInsensitive) == 0
        || key.compare("Cubic", Qt::CaseInsensitive) == 0
        || key.startsWith("Poly", Qt::CaseInsensitive)
        || key.startsWith("Rational", Qt::CaseInsensitive)) {
        double slope = (y_back - y_front) / x_span;
        double a0_g = y_front - slope * x_min;
        setByName("a0", a0_g);
        setByName("b", a0_g);
        setByName("a1", slope);
        setByName("c", slope);
        setByName("a", 0.0);
        setByName("b1", 0.0);
        setByName("b2", 0.0);
        setByName("a2", 0.0);
        setByName("a3", 0.0);
        setByName("a4", 0.0);
        setByName("a5", 0.0);
        d_initial_guesses_set = true;
        return;
    }

    // 15. Default / Peak Functions (Gauss, GaussAmp, Lorentz, PseudoVoigt1/2, VoigtApprox, PearsonVII, BiGaussian, EMG, LogNormal, Asym2Sig, DoniachSunjic, Extreme, GramCharlier)
    bool isAreaPeak = (key.compare("Gauss", Qt::CaseInsensitive) == 0
                       || key.compare("Lorentz", Qt::CaseInsensitive) == 0
                       || key.startsWith("PseudoVoigt", Qt::CaseInsensitive)
                       || key.compare("VoigtApprox", Qt::CaseInsensitive) == 0
                       || key.compare("EMG", Qt::CaseInsensitive) == 0
                       || key.compare("GramCharlier", Qt::CaseInsensitive) == 0);
    double sigma_g = fwhm / (2.0 * std::sqrt(2.0 * M_LN2));
    double area_g = y_span * fwhm * std::sqrt(M_PI / (4.0 * M_LN2));
    setByName("y0", y_min);
    setByName("xc", (key.compare("LogNormal", Qt::CaseInsensitive) == 0) ? std::max(1e-6, xc_peak) : xc_peak);
    setByName("A", isAreaPeak ? area_g : y_span);
    if (key.compare("GaussAmp", Qt::CaseInsensitive) == 0
        || key.compare("EMG", Qt::CaseInsensitive) == 0
        || key.compare("GramCharlier", Qt::CaseInsensitive) == 0)
        setByName("w", sigma_g);
    else if (key.compare("Gauss", Qt::CaseInsensitive) == 0)
        setByName("w", 2.0 * sigma_g);
    else if (key.compare("LogNormal", Qt::CaseInsensitive) == 0)
        setByName("w", std::max(0.05, fwhm / (2.0 * std::max(1e-3, xc_peak))));
    else
        setByName("w", fwhm);
    setByName("wG", fwhm);
    setByName("wL", fwhm);
    setByName("w1", (key.compare("Asym2Sig", Qt::CaseInsensitive) == 0) ? fwhm : (0.45 * fwhm));
    setByName("w2", (key.compare("Asym2Sig", Qt::CaseInsensitive) == 0) ? (0.15 * fwhm) : (0.55 * fwhm));
    setByName("w3", 0.2 * fwhm);
    setByName("mu", 0.5);
    setByName("m", 2.0);
    setByName("t0", 0.8 * sigma_g);
    setByName("alpha", 0.15);
    setByName("a3", 0.1);
    setByName("a4", 0.05);

    d_initial_guesses_set = true;
}

bool NonLinearFit::calculateFitCurveData(const vector<double> &par, std::vector<double> &X,
                                         std::vector<double> &Y)
{
    for (unsigned i = 0; i < d_p; i++)
        if (!d_script->setDouble(par[i], d_param_names[i].toUtf8()))
            return false;

    bool ok = false;
    generateX(X);

    for (int i = 0; i < d_points; i++) {
        if (!d_script->setDouble(X[i], "x"))
            return false;
        auto y = d_script->eval();
        if (y.isNull())
            return false;
        Y[i] = y.toDouble(&ok);
        if (!ok)
            return false;
    }
    return true;
}
