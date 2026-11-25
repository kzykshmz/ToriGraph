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
#include "ApplicationWindow.h"
#include "MyParser.h"
#include "fit_gsl.h"
#include "FunctionCurve.h"
#include "ColorButton.h"
#include "Table.h"
#include "future/core/column/Column.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlin.h>
#include <gsl/gsl_version.h>

#include <algorithm>
#include <cmath>
#include <limits>
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
      QT_TRANSLATE_NOOP("NonLinearFit", "Gaussian Peak (Area parameterization)") },
    { "GaussAmp", "Peak Functions", "y0, A, xc, w",
      "y0+A*exp(-(x-xc)*(x-xc)/(2*w*w))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Gaussian Peak (Amplitude parameterization)") },
    { "Lorentz", "Peak Functions", "A, xc, w, y0",
      "y0+2*A/PI*w/(4*(x-xc)^2+w^2)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Lorentzian Peak (Area parameterization)") },
    { "PseudoVoigt1", "Peak Functions", "A, xc, w, mu, y0",
      "y0+A*(mu*2/PI*w/(4*(x-xc)^2+w^2)+(1-mu)*sqrt(4*ln(2))/(sqrt(PI)*w)*exp(-4*ln(2)*((x-xc)/w)^2))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Pseudo-Voigt Peak with shared FWHM w and Lorentz fraction mu") },
    { "PseudoVoigt2", "Peak Functions", "A, xc, wG, wL, mu, y0",
      "y0+A*(mu*2/PI*wL/(4*(x-xc)^2+wL^2)+(1-mu)*sqrt(4*ln(2))/(sqrt(PI)*wG)*exp(-4*ln(2)*((x-xc)/wG)^2))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Pseudo-Voigt Peak with separate Gaussian (wG) and Lorentzian (wL) widths") },
    { "VoigtApprox", "Peak Functions", "A, xc, w, mu, y0",
      "y0+A*(mu*2/PI*w/(4*(x-xc)^2+w^2)+(1-mu)*sqrt(4*ln(2))/(sqrt(PI)*w)*exp(-4*ln(2)*((x-xc)/w)^2))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Pseudo-Voigt approximation to Voigt profile (alias of PseudoVoigt1)") },
    { "PearsonVII", "Peak Functions", "A, xc, w, m, y0",
      "y0+A*(1+(2^(1/m)-1)*(2*(x-xc)/w)^2)^(-m)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Pearson VII XRD Diffraction Peak") },
    { "BiGaussian", "Peak Functions", "y0, A, xc, w1, w2",
      "y0+((x<xc)?(A*exp(-(x-xc)^2/(2*w1^2))):(A*exp(-(x-xc)^2/(2*w2^2))))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Asymmetric Bifurcated Gaussian Peak (left width w1, right width w2)") },
    { "EMG", "Peak Functions", "y0, A, xc, w, t0",
      "y0+(A/(2*t0))*exp(0.5*(w/t0)^2-(x-xc)/t0)*erfc((w/t0-(x-xc)/w)/sqrt(2))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Exponentially Modified Gaussian Peak (Chromatography / Spectroscopy)") },
    { "LogNormal", "Peak Functions", "A, xc, w, y0",
      "y0+A/(sqrt(2*PI)*w*x)*exp(-(ln(x/xc))^2/(2*w^2))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Log-Normal Asymmetric Peak Distribution") },
    { "Asym2Sig", "Peak Functions", "y0, A, xc, w1, w2, w3",
      "y0+A*(1/(1+exp(-(x-xc+w1/2)/w2)))*(1-1/(1+exp(-(x-xc-w1/2)/w3)))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Asymmetric Double Sigmoidal Peak") },
    { "DoniachSunjic", "Peak Functions", "y0, A, xc, w, alpha",
      "y0+A*cos(PI*alpha/2+(1-alpha)*atan((x-xc)/(w/2)))/(1+4*((x-xc)/w)^2)^((1-alpha)/2)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Doniach-Sunjic Asymmetric XPS Core-Level Peak") },
    { "Extreme", "Peak Functions", "y0, A, xc, w",
      "y0+A*exp(-exp(-(x-xc)/w)-(x-xc)/w+1)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Extreme Value (Gumbel) Asymmetric Peak") },
    { "GramCharlier", "Peak Functions", "y0, A, xc, w, a3, a4",
      "y0+A/(sqrt(2*PI)*w)*exp(-0.5*((x-xc)/w)^2)*(1+a3/6*(((x-xc)/w)^3-3*(x-xc)/w)+a4/24*(((x-xc)/w)^4-6*((x-xc)/w)^2+3))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Gram-Charlier Edgeworth Series Peak") },

    // 2. Exponential (9)
    { "ExpDec1", "Exponential", "A, t, y0",
      "y0+A*exp(-x/t)",
      QT_TRANSLATE_NOOP("NonLinearFit", "First-Order Exponential Decay") },
    { "ExpDec2", "Exponential", "A1, t1, A2, t2, y0",
      "y0+A1*exp(-x/t1)+A2*exp(-x/t2)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Second-Order (Two-Phase) Exponential Decay") },
    { "ExpDec3", "Exponential", "A1, t1, A2, t2, A3, t3, y0",
      "y0+A1*exp(-x/t1)+A2*exp(-x/t2)+A3*exp(-x/t3)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Third-Order (Three-Phase) Exponential Decay") },
    { "ExpGrow1", "Exponential", "A, t, y0",
      "y0+A*exp(x/t)",
      QT_TRANSLATE_NOOP("NonLinearFit", "First-Order Exponential Growth") },
    { "ExpGrow2", "Exponential", "y0, A1, t1, A2, t2",
      "y0+A1*exp(x/t1)+A2*exp(x/t2)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Second-Order (Two-Phase) Exponential Growth") },
    { "ExpAssoc", "Exponential", "y0, A1, t1, A2, t2",
      "y0+A1*(1-exp(-x/t1))+A2*(1-exp(-x/t2))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Two-Phase Exponential Association") },
    { "BoxLucas1", "Exponential", "a, b",
      "a*(1-exp(-b*x))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Box-Lucas Exponential Asymptotic Model") },
    { "Strexp (KWW)", "Exponential", "y0, A, tau, beta",
      "y0+A*exp(-(x/tau)^beta)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Stretched Exponential (Kohlrausch-Williams-Watts) Relaxation") },
    { "Chapman", "Exponential", "a, b, c",
      "a*(1-exp(-b*x))^c",
      QT_TRANSLATE_NOOP("NonLinearFit", "Chapman-Richards Exponential Growth") },

    // 3. Sigmoidal / Growth (8)
    { "Boltzmann", "Sigmoidal / Growth", "A1, A2, x0, dx",
      "(A1-A2)/(1+exp((x-x0)/dx))+A2",
      QT_TRANSLATE_NOOP("NonLinearFit", "Boltzmann Sigmoidal Transition Function") },
    { "Logistic", "Sigmoidal / Growth", "A1, A2, x0, p",
      "A2+(A1-A2)/(1+(x/x0)^p)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Four-Parameter Logistic Sigmoidal Dose-Response") },
    { "DoseResp (Hill EC50)", "Sigmoidal / Growth", "A1, A2, logx0, p",
      "A1+(A2-A1)/(1+10^((logx0-x)*p))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Dose-Response Sigmoid with Log(EC50) and Hill Slope") },
    { "Gompertz", "Sigmoidal / Growth", "a, xc, k",
      "a*exp(-exp(-k*(x-xc)))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Gompertz Asymmetric Sigmoidal Growth Model") },
    { "Hill", "Sigmoidal / Growth", "Vmax, k, n",
      "Vmax*x^n/(k^n+x^n)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Cooperative Hill Kinetics / Binding Isotherm") },
    { "Richards", "Sigmoidal / Growth", "a, xc, d, k",
      "a*(1+(d-1)*exp(-k*(x-xc)))^(1/(1-d))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Richards Generalized Logistic Growth Function") },
    { "MMF", "Sigmoidal / Growth", "a, b, c, d",
      "(a*b+c*x^d)/(b+x^d)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Morgan-Mercer-Flodin Sigmoidal Growth Model") },
    { "WeibullGrowth", "Sigmoidal / Growth", "a, b, d, k",
      "a-(a-b)*exp(-(k*x)^d)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Weibull Cumulative Growth Distribution") },

    // 4. Materials / Metallurgy / Physics (13)
    { "Arrhenius", "Materials / Metallurgy / Physics", "A, Ea",
      "A*exp(-Ea/(8.3144626*x))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Arrhenius Thermally Activated Rate (T in K, Ea in J/mol)") },
    { "VTF (Vogel-Tammann-Fulcher)", "Materials / Metallurgy / Physics", "A, B, T0",
      "A*exp(B/(x-T0))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Vogel-Tammann-Fulcher Supercooled Liquid / Glass Relaxation") },
    { "Avrami (JMAK)", "Materials / Metallurgy / Physics", "k, n",
      "1-exp(-k*x^n)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Johnson-Mehl-Avrami-Kolmogorov Phase Transformation Kinetics") },
    { "HallPetch", "Materials / Metallurgy / Physics", "sigma0, ky",
      "sigma0+ky/sqrt(x)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Hall-Petch Grain Size Strengthening Law (d in x)") },
    { "Hollomon", "Materials / Metallurgy / Physics", "K, n",
      "K*x^n",
      QT_TRANSLATE_NOOP("NonLinearFit", "Hollomon Power-Law Strain Hardening (sigma = K * eps^n)") },
    { "Ludwik", "Materials / Metallurgy / Physics", "sigma0, K, n",
      "sigma0+K*x^n",
      QT_TRANSLATE_NOOP("NonLinearFit", "Ludwik Strain Hardening with Yield Stress Offset") },
    { "Voce", "Materials / Metallurgy / Physics", "sigmaS, sigma0, epsC",
      "sigmaS-(sigmaS-sigma0)*exp(-x/epsC)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Voce Exponential Saturation Strain Hardening") },
    { "Swift", "Materials / Metallurgy / Physics", "K, eps0, n",
      "K*(eps0+x)^n",
      QT_TRANSLATE_NOOP("NonLinearFit", "Swift Pre-Strained Power-Law Work Hardening") },
    { "ParisLaw", "Materials / Metallurgy / Physics", "C, m",
      "C*x^m",
      QT_TRANSLATE_NOOP("NonLinearFit", "Paris-Erdogan Fatigue Crack Growth Law (da/dN = C * dK^m)") },
    { "Langmuir", "Materials / Metallurgy / Physics", "qmax, K",
      "qmax*K*x/(1+K*x)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Langmuir Monolayer Adsorption Isotherm") },
    { "Freundlich", "Materials / Metallurgy / Physics", "Kf, n",
      "Kf*x^(1/n)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Freundlich Empirical Adsorption Isotherm") },
    { "MichaelisMenten", "Materials / Metallurgy / Physics", "Vmax, Km",
      "Vmax*x/(Km+x)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Michaelis-Menten Enzyme / Reaction Kinetics") },
    { "Planck", "Materials / Metallurgy / Physics", "A, B, T",
      "A/(x^5*(exp(B/(x*T))-1))",
      QT_TRANSLATE_NOOP("NonLinearFit", "Planck Blackbody Spectral Radiance Law") },

    // 5. Waveform / Periodic (4)
    { "Sine", "Waveform / Periodic", "y0, A, xc, w",
      "y0+A*sin(PI*(x-xc)/w)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Standard Sine Wave Function") },
    { "SineDamp", "Waveform / Periodic", "y0, A, xc, w, t0",
      "y0+A*exp(-x/t0)*sin(PI*(x-xc)/w)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Exponentially Damped Sine Wave") },
    { "SineSq", "Waveform / Periodic", "y0, A, xc, w",
      "y0+A*(sin(PI*(x-xc)/w))^2",
      QT_TRANSLATE_NOOP("NonLinearFit", "Squared Sine Wave Function") },
    { "GaussianEnvelopeCosine", "Waveform / Periodic", "y0, A, xc, w, lambda, phi",
      "y0+A*exp(-((x-xc)/w)^2)*cos(2*PI*(x-xc)/lambda+phi)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Gaussian-Envelope Cosine Wavepacket") },

    // 6. Power / Logarithmic / Rational / Polynomial (10)
    { "Allometric1", "Power / Logarithmic / Rational / Polynomial", "a, b",
      "a*x^b",
      QT_TRANSLATE_NOOP("NonLinearFit", "Classical Allometric Power Scaling Law") },
    { "Allometric2", "Power / Logarithmic / Rational / Polynomial", "y0, a, b",
      "y0+a*x^b",
      QT_TRANSLATE_NOOP("NonLinearFit", "Offset Allometric Power Scaling Law") },
    { "Logarithm", "Power / Logarithmic / Rational / Polynomial", "a, b",
      "a+b*ln(x)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Natural Logarithmic Function") },
    { "Hyperbl", "Power / Logarithmic / Rational / Polynomial", "P1, P2",
      "P1*x/(P2+x)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Two-Parameter Rectangular Hyperbola") },
    { "Rational0", "Power / Logarithmic / Rational / Polynomial", "a, b, c",
      "(b+c*x)/(1+a*x)",
      QT_TRANSLATE_NOOP("NonLinearFit", "First-Order Linear Rational Function") },
    { "Rational1", "Power / Logarithmic / Rational / Polynomial", "a0, a1, b1, b2",
      "(a0+a1*x)/(1+b1*x+b2*x^2)",
      QT_TRANSLATE_NOOP("NonLinearFit", "Linear-over-Quadratic Rational Function") },
    { "Parabola", "Power / Logarithmic / Rational / Polynomial", "a0, a1, a2",
      "a0+a1*x+a2*x^2",
      QT_TRANSLATE_NOOP("NonLinearFit", "Second-Order Polynomial (Parabola)") },
    { "Cubic", "Power / Logarithmic / Rational / Polynomial", "a0, a1, a2, a3",
      "a0+a1*x+a2*x^2+a3*x^3",
      QT_TRANSLATE_NOOP("NonLinearFit", "Third-Order Cubic Polynomial") },
    { "Poly4", "Power / Logarithmic / Rational / Polynomial", "a0, a1, a2, a3, a4",
      "a0+a1*x+a2*x^2+a3*x^3+a4*x^4",
      QT_TRANSLATE_NOOP("NonLinearFit", "Fourth-Order Quartic Polynomial") },
    { "Poly5", "Power / Logarithmic / Rational / Polynomial", "a0, a1, a2, a3, a4, a5",
      "a0+a1*x+a2*x^2+a3*x^3+a4*x^4+a5*x^5",
      QT_TRANSLATE_NOOP("NonLinearFit", "Fifth-Order Quintic Polynomial") }
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
    if (k.compare("Voigt", Qt::CaseInsensitive) == 0
        || k.compare("PseudoVoigt1 (Voigt approx.)", Qt::CaseInsensitive) == 0)
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
        if (!isNonInteractiveFit(parent(), d_silent)) {
            QMessageBox::critical((ApplicationWindow *)parent(), tr("Fit Error"),
                                  tr("You must provide a list containing at least one parameter for "
                                     "this type of fit. Operation aborted!"));
        }
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

    d_param_explain.clear();
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
    return spec ? tr(spec->description) : QString();
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
    // Spans and lower limits follow the data's own scale: absolute floors such as 1e-9 or 1e-6
    // swallowed data recorded in SI units (x ~ 1e-9 m or s). For spans >= 1 the scaled floors
    // equal the former absolute ones.
    double x_span = x_max - x_min;
    if (!(x_span > 0.0))
        x_span = 1e-9;
    const double xFloorScale = std::min(1.0, x_span);
    double x_mid = 0.5 * (x_min + x_max);

    auto minIt = std::min_element(d_y.begin(), d_y.end());
    auto maxIt = std::max_element(d_y.begin(), d_y.end());
    size_t maxIdx = size_t(std::distance(d_y.begin(), maxIt));
    double y_min = *minIt;
    double y_max = *maxIt;
    double y_span = y_max - y_min;
    if (!(y_span > 0.0))
        y_span = 1e-9;
    const double yFloorScale = std::min(1.0, y_span);
    double y_front = d_y.front();
    double y_back = d_y.back();

    // Full width at the level halfY around the extremum at idx (a maximum when up, a minimum
    // otherwise), interpolated between samples; falls back to 0.2 of the X span when the
    // crossings are not found on both sides.
    auto widthAround = [&](size_t idx, double level, bool up) -> double {
        auto beyond = [&](double v) { return up ? (v <= level) : (v >= level); };
        const double dyTiny = 1e-15 * yFloorScale;
        double xL = x_min, xR = x_max;
        for (int k = int(idx); k > 0; --k) {
            if (beyond(d_y[k - 1])) {
                double dy = d_y[k] - d_y[k - 1];
                double frac = (std::abs(dy) > dyTiny) ? ((level - d_y[k - 1]) / dy) : 0.5;
                xL = d_x[k - 1] + std::max(0.0, std::min(1.0, frac)) * (d_x[k] - d_x[k - 1]);
                break;
            }
        }
        for (size_t k = idx; k + 1 < n; ++k) {
            if (beyond(d_y[k + 1])) {
                double dy = d_y[k] - d_y[k + 1];
                double frac = (std::abs(dy) > dyTiny) ? ((d_y[k] - level) / dy) : 0.5;
                xR = d_x[k] + std::max(0.0, std::min(1.0, frac)) * (d_x[k + 1] - d_x[k]);
                break;
            }
        }
        double w = std::max(1e-6 * xFloorScale, std::abs(xR - xL));
        if (w >= 0.95 * x_span)
            w = 0.2 * x_span;
        return w;
    };

    // Peak shape characteristics around maxIdx
    double xc_peak = d_x[maxIdx];
    double halfY = y_min + 0.5 * y_span;
    double fwhm = widthAround(maxIdx, halfY, true);

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

    // 3a. Swift: K * (eps0 + x)^n (no additive y0 offset)
    if (key.compare("Swift", Qt::CaseInsensitive) == 0) {
        double ySum = 0.0;
        for (size_t i = 0; i < n; ++i)
            ySum += d_y[i];
        const double ySign = (ySum < 0.0) ? -1.0 : 1.0;

        auto evalSwiftRss = [&](double pK, double pEps0, double pN) -> double {
            if (!std::isfinite(pK) || !std::isfinite(pEps0) || !std::isfinite(pN))
                return std::numeric_limits<double>::infinity();
            double rss = 0.0;
            for (size_t i = 0; i < n; ++i) {
                const double z = pEps0 + d_x[i];
                if (!(z > 0.0))
                    return std::numeric_limits<double>::infinity();
                const double zn = std::pow(z, pN);
                if (!std::isfinite(zn))
                    return std::numeric_limits<double>::infinity();
                const double r = d_y[i] - pK * zn;
                rss += r * r;
            }
            return std::isfinite(rss) ? rss : std::numeric_limits<double>::infinity();
        };

        const double minMargin = 1e-12 * std::max(1.0, std::abs(x_min));
        double bestEps0 = std::max(-x_min + minMargin, 0.01 * std::max(1e-4, x_span));
        double bestN = 0.25;
        double bestK = ySign * std::max(1e-9, std::abs(y_max));
        double bestRss = evalSwiftRss(bestK, bestEps0, bestN);

        auto tryEps0 = [&](double candEps0) {
            if (!std::isfinite(candEps0) || candEps0 <= -x_min + minMargin)
                return;
            std::vector<double> u, v;
            u.reserve(n);
            v.reserve(n);
            for (size_t i = 0; i < n; ++i) {
                const double z = candEps0 + d_x[i];
                const double ys = ySign * d_y[i];
                if (z > 1e-30 && ys > 1e-30) {
                    u.push_back(std::log(z));
                    v.push_back(std::log(ys));
                }
            }
            auto testN = [&](double candN) {
                if (!std::isfinite(candN))
                    return;
                candN = std::max(-12.0, std::min(12.0, candN));
                double swy = 0.0, sww = 0.0;
                for (size_t i = 0; i < n; ++i) {
                    const double z = candEps0 + d_x[i];
                    if (!(z > 0.0))
                        return;
                    const double w = std::pow(z, candN);
                    if (!std::isfinite(w))
                        return;
                    swy += d_y[i] * w;
                    sww += w * w;
                }
                if (sww > 1e-300 && std::isfinite(swy) && std::isfinite(sww)) {
                    const double kOls = swy / sww;
                    const double rss = evalSwiftRss(kOls, candEps0, candN);
                    if (rss < bestRss) {
                        bestRss = rss;
                        bestK = kOls;
                        bestEps0 = candEps0;
                        bestN = candN;
                    }
                }
            };

            if (u.size() >= 2) {
                double c = 0.0, m = 0.25;
                linearRegression(u, v, c, m);
                const double kLog = ySign * std::exp(std::max(-80.0, std::min(80.0, c)));
                const double mClamp = std::max(-12.0, std::min(12.0, m));
                const double rssLog = evalSwiftRss(kLog, candEps0, mClamp);
                if (rssLog < bestRss) {
                    bestRss = rssLog;
                    bestK = kLog;
                    bestEps0 = candEps0;
                    bestN = mClamp;
                }
                testN(m);
            } else {
                const double nFallbacks[] = { 0.1, 0.25, 0.5, 1.0, 2.0 };
                for (double nf : nFallbacks)
                    testN(nf);
            }
        };

        const double scale = std::max(std::max(1e-9, x_span),
                                      std::max(std::abs(x_min), std::abs(x_max)));
        const double logLo = std::log(1e-7 * scale);
        const double logHi = std::log(50.0 * scale);
        for (int g = 0; g < 96; ++g) {
            const double delta = std::exp(logLo + (logHi - logLo) * (double(g) / 95.0));
            tryEps0(-x_min + delta);
        }
        const double spanBase = std::max(0.0, -x_min);
        const double fracGrid[] = { 1e-5, 1e-4, 5e-4, 1e-3, 2.5e-3, 5e-3, 0.01, 0.015, 0.02, 0.03,
                                    0.05, 0.08, 0.12, 0.2,  0.35,   0.5,  0.75, 1.0,   2.0,  5.0 };
        for (double frac : fracGrid)
            tryEps0(spanBase + frac * x_span);
        if (x_min > minMargin)
            tryEps0(0.0);

        double bestDelta = std::max(minMargin, bestEps0 + x_min);
        for (int g = 0; g < 25; ++g) {
            const double factor = std::pow(2.0, -1.0 + 2.0 * (double(g) / 24.0));
            tryEps0(-x_min + bestDelta * factor);
        }
        bestDelta = std::max(minMargin, bestEps0 + x_min);
        for (int g = 0; g < 21; ++g) {
            const double factor = 0.85 + 0.30 * (double(g) / 20.0);
            tryEps0(-x_min + bestDelta * factor);
        }

        setByName("K", bestK);
        setByName("eps0", bestEps0);
        setByName("n", bestN);
        d_initial_guesses_set = true;
        return;
    }

    // 3b. Ludwik / Allometric2
    if (key.compare("Ludwik", Qt::CaseInsensitive) == 0
        || key.compare("Allometric2", Qt::CaseInsensitive) == 0) {
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

    // 5a. MMF (Morgan-Mercer-Flodin): (a*b + c*x^d) / (b + x^d)
    if (key.compare("MMF", Qt::CaseInsensitive) == 0) {
        double a_g = y_front;
        double c_g = y_back;
        if (std::abs(c_g - a_g) < 0.1 * y_span) {
            a_g = (y_back >= y_front) ? y_min : y_max;
            c_g = (y_back >= y_front) ? y_max : y_min;
        }
        double y_half = 0.5 * (a_g + c_g);
        double x50 = std::max(1e-6, (x_mid > 0.0) ? x_mid : (0.5 * std::max(1e-6, x_max)));
        for (size_t i = 0; i + 1 < n; ++i) {
            if ((d_y[i] - y_half) * (d_y[i + 1] - y_half) <= 0.0) {
                double dy = d_y[i + 1] - d_y[i];
                double frac = (std::abs(dy) > 1e-15) ? ((y_half - d_y[i]) / dy) : 0.5;
                double xc_cand = d_x[i] + std::max(0.0, std::min(1.0, frac)) * (d_x[i + 1] - d_x[i]);
                if (xc_cand > 1e-12) {
                    x50 = xc_cand;
                    break;
                }
            }
        }

        double a_reg = a_g - 0.02 * (c_g - a_g);
        double c_reg = c_g + 0.06 * (c_g - a_g);
        double denom = c_reg - a_reg;
        std::vector<double> u, v;
        if (std::abs(denom) > 1e-12) {
            for (size_t i = 0; i < n; ++i) {
                if (d_x[i] <= 1e-12)
                    continue;
                double frac = (d_y[i] - a_reg) / denom;
                if (frac > 0.04 && frac < 0.96) {
                    u.push_back(std::log(d_x[i]));
                    v.push_back(std::log(frac / (1.0 - frac)));
                }
            }
        }

        auto evalMmfRss = [&](double pa, double pb, double pc, double pd) -> double {
            if (!std::isfinite(pb) || pb <= 1e-30 || !std::isfinite(pd) || pd <= 0.05)
                return std::numeric_limits<double>::infinity();
            double rss = 0.0;
            for (size_t i = 0; i < n; ++i) {
                double xi = std::max(0.0, d_x[i]);
                double xd = (xi > 0.0) ? std::pow(xi, pd) : 0.0;
                if (!std::isfinite(xd))
                    return std::numeric_limits<double>::infinity();
                double yhat = (pa * pb + pc * xd) / (pb + xd);
                double r = d_y[i] - yhat;
                rss += r * r;
            }
            return rss;
        };

        double bestA = a_g;
        double bestD = 2.0;
        double bestB = std::max(1e-12, std::pow(x50, bestD));
        double bestC = c_g;
        double bestRss = evalMmfRss(bestA, bestB, bestC, bestD);

        auto tryCandidate = [&](double ca, double cb, double cc, double cd) {
            double rss = evalMmfRss(ca, cb, cc, cd);
            if (rss < bestRss) {
                bestRss = rss;
                bestA = ca;
                bestB = cb;
                bestC = cc;
                bestD = cd;
            }
        };

        if (u.size() >= 2) {
            double intercept = 0.0, slope = 2.0;
            linearRegression(u, v, intercept, slope);
            double d_est = std::max(0.25, std::min(8.0, slope));
            double b_est = std::exp(std::max(-40.0, std::min(40.0, -intercept)));
            tryCandidate(a_reg, b_est, c_reg, d_est);
            tryCandidate(a_g, b_est, c_g, d_est);
        }
        const double dGrid[] = { 0.75, 1.25, 2.0, 3.0, 4.0 };
        for (double dc : dGrid) {
            double bc = std::max(1e-12, std::pow(x50, dc));
            tryCandidate(a_g, bc, c_g, dc);
            tryCandidate(a_reg, bc, c_reg, dc);
        }

        setByName("a", bestA);
        setByName("b", bestB);
        setByName("c", bestC);
        setByName("d", bestD);
        d_initial_guesses_set = true;
        return;
    }

    // 5b. Gompertz / Richards / WeibullGrowth / Hill
    if (key.compare("Gompertz", Qt::CaseInsensitive) == 0
        || key.compare("Richards", Qt::CaseInsensitive) == 0
        || key.compare("WeibullGrowth", Qt::CaseInsensitive) == 0
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
        setByName("b", y_min);
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
        double A_g = isGrow ? std::max(1e-6 * yFloorScale, (y_back - y_front) * 0.1)
                            : (y_front - y_back);
        if (std::abs(A_g) < 1e-9 * yFloorScale)
            A_g = y_span;
        double t_g = std::max(1e-6 * xFloorScale, 0.35 * x_span);
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

    // 13a. SineSq: y0 + A * (sin(PI * (x - xc) / w))^2
    if (key.compare("SineSq", Qt::CaseInsensitive) == 0) {
        double mean_y = std::accumulate(d_y.begin(), d_y.end(), 0.0) / double(n);
        double y_mid = y_min + 0.5 * y_span;
        int crossings = 0;
        for (size_t i = 0; i + 1 < n; ++i) {
            if ((d_y[i] - y_mid) * (d_y[i + 1] - y_mid) < 0.0)
                crossings++;
        }
        double w_cross = (crossings >= 1) ? (2.0 * x_span / double(crossings)) : x_span;
        double bestY0 = y_min;
        double bestA = y_span;
        double bestW = std::max(1e-9, w_cross);
        double bestXc = xc_peak - 0.5 * bestW;

        auto evalSineSqRss = [&](double py0, double pA, double pxc, double pw) -> double {
            if (!std::isfinite(pw) || std::abs(pw) <= 1e-12)
                return std::numeric_limits<double>::infinity();
            double rss = 0.0;
            for (size_t i = 0; i < n; ++i) {
                double s = std::sin(M_PI * (d_x[i] - pxc) / pw);
                double r = d_y[i] - (py0 + pA * s * s);
                rss += r * r;
            }
            return rss;
        };
        double bestRss = evalSineSqRss(bestY0, bestA, bestXc, bestW);

        std::vector<double> wCands;
        const int maxK = std::min<int>(240, std::max<int>(40, int(n) * 2));
        wCands.reserve(size_t(maxK + 45));
        for (int m = 0; m <= 40; ++m)
            wCands.push_back(bestW * (0.4 + 1.6 * double(m) / 40.0));
        for (int k = 1; k <= maxK; ++k)
            wCands.push_back(x_span / (0.25 * double(k)));

        const double refX = (x_min <= 0.0 && x_max >= 0.0) ? 0.0 : x_min;
        for (double wc : wCands) {
            if (!std::isfinite(wc) || wc <= 1e-12)
                continue;
            const double omega = (2.0 * M_PI) / wc;
            double mCos = 0.0, mSin = 0.0;
            for (size_t i = 0; i < n; ++i) {
                mCos += std::cos(omega * d_x[i]);
                mSin += std::sin(omega * d_x[i]);
            }
            mCos /= double(n);
            mSin /= double(n);

            double scc = 0.0, sss = 0.0, scs = 0.0, scy = 0.0, ssy = 0.0;
            for (size_t i = 0; i < n; ++i) {
                double cc = std::cos(omega * d_x[i]) - mCos;
                double ss = std::sin(omega * d_x[i]) - mSin;
                double yc = d_y[i] - mean_y;
                scc += cc * cc;
                sss += ss * ss;
                scs += cc * ss;
                scy += cc * yc;
                ssy += ss * yc;
            }
            double det = scc * sss - scs * scs;
            if (det <= 1e-18)
                continue;
            double c1 = (sss * scy - scs * ssy) / det;
            double c2 = (-scs * scy + scc * ssy) / det;
            double c0 = mean_y - c1 * mCos - c2 * mSin;
            double ampHalf = std::hypot(c1, c2);
            double Ac = std::max(1e-9, 2.0 * ampHalf);
            double y0c = c0 - ampHalf;
            double phi = std::atan2(-c2, -c1);
            double xcc = (phi / (2.0 * M_PI)) * wc;
            xcc -= wc * std::floor((xcc - refX) / wc);

            double rss = evalSineSqRss(y0c, Ac, xcc, wc);
            if (rss < bestRss) {
                bestRss = rss;
                bestY0 = y0c;
                bestA = Ac;
                bestXc = xcc;
                bestW = wc;
            }
        }

        setByName("y0", bestY0);
        setByName("A", bestA);
        setByName("xc", bestXc);
        setByName("w", bestW);
        d_initial_guesses_set = true;
        return;
    }

    // 13b. Waveform / Periodic
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
    // The peak models are y0 + A*shape(x): a dip (the largest deviation from the baseline is
    // downward) is the same shape with A < 0, the centre at the minimum and y0 at the top.
    bool isPeakModel = false;
    for (int i = 0; i < kNumOriginModels; ++i) {
        if (key.compare(QString::fromLatin1(kOriginModels[i].name), Qt::CaseInsensitive) == 0) {
            isPeakModel = (qstrcmp(kOriginModels[i].category, "Peak Functions") == 0);
            break;
        }
    }
    double y0_g = y_min;
    double ampSign = 1.0;
    if (isPeakModel && dataLooksLikeDip(d_y)) {
        const size_t minIdx = size_t(std::distance(d_y.begin(), minIt));
        xc_peak = d_x[minIdx];
        fwhm = widthAround(minIdx, y_max - 0.5 * y_span, false);
        y0_g = y_max;
        ampSign = -1.0;
    }
    double sigma_g = fwhm / (2.0 * std::sqrt(2.0 * M_LN2));
    double area_g = y_span * fwhm * std::sqrt(M_PI / (4.0 * M_LN2));
    setByName("y0", y0_g);
    setByName("xc", (key.compare("LogNormal", Qt::CaseInsensitive) == 0)
                    ? std::max(1e-6 * xFloorScale, xc_peak)
                    : xc_peak);
    setByName("A", ampSign * (isAreaPeak ? area_g : y_span));
    if (key.compare("GaussAmp", Qt::CaseInsensitive) == 0
        || key.compare("EMG", Qt::CaseInsensitive) == 0
        || key.compare("GramCharlier", Qt::CaseInsensitive) == 0)
        setByName("w", sigma_g);
    else if (key.compare("Gauss", Qt::CaseInsensitive) == 0)
        setByName("w", 2.0 * sigma_g);
    else if (key.compare("LogNormal", Qt::CaseInsensitive) == 0)
        setByName("w", std::max(0.05, fwhm / (2.0 * std::max(1e-3 * xFloorScale, xc_peak))));
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

namespace {

enum GlobalSlotBoundMode {
    GBoundNone = 0,
    GBoundBoth = 1,
    GBoundLower = 2,
    GBoundUpper = 3
};

struct GlobalFitSlot
{
    int paramIdx = 0;
    int curveIdx = -1; // -1 means shared across all curves
    GlobalSlotBoundMode mode = GBoundNone;
    double lo = -std::numeric_limits<double>::infinity();
    double hi = std::numeric_limits<double>::infinity();

    double mapUtoP(double u) const
    {
        switch (mode) {
        case GBoundNone:
            return u;
        case GBoundBoth: {
            double uc = std::max(-16.0, std::min(16.0, u));
            return lo + 0.5 * (hi - lo) * (std::tanh(uc) + 1.0);
        }
        case GBoundLower: {
            double uc = std::max(-1e8, std::min(1e8, u));
            return lo + uc * uc;
        }
        case GBoundUpper: {
            double uc = std::max(-1e8, std::min(1e8, u));
            return hi - uc * uc;
        }
        }
        return u;
    }

    double dPdU(double u) const
    {
        switch (mode) {
        case GBoundNone:
            return 1.0;
        case GBoundBoth: {
            double uc = std::max(-16.0, std::min(16.0, u));
            double ch = std::cosh(uc);
            return 0.5 * (hi - lo) * std::max(1e-13, 1.0 / (ch * ch));
        }
        case GBoundLower: {
            double d = 2.0 * u;
            return (std::abs(d) < 1e-10) ? (u >= 0.0 ? 1e-10 : -1e-10) : d;
        }
        case GBoundUpper: {
            double d = -2.0 * u;
            return (std::abs(d) < 1e-10) ? (u >= 0.0 ? -1e-10 : 1e-10) : d;
        }
        }
        return 1.0;
    }

    //! Reported value: on the bound when within the tolerance the single-curve fit uses.
    double snapP(double p) const { return (mode == GBoundNone) ? p : Fit::snapToBound(p, lo, hi); }
    //! -1 / +1 when the slot (solver value u) is held at its lower / upper bound, else 0.
    int boundSide(double u) const
    {
        return (mode == GBoundNone) ? 0 : Fit::boundSide(mapUtoP(u), lo, hi);
    }
};

//! kktSigns for Fit::lmGradientIsStationary: a slot held at a bound whose gradient points out
//! of the box is a constrained minimum, not a stall (as in the single-curve fit).
std::vector<int> slotKktSigns(const std::vector<GlobalFitSlot> &slotList, const gsl_vector *u)
{
    std::vector<int> signs(slotList.size(), 0);
    for (size_t k = 0; k < slotList.size(); ++k) {
        const double uk = gsl_vector_get(u, k);
        signs[k] = Fit::kktSignForBound(slotList[k].boundSide(uk), slotList[k].dPdU(uk));
    }
    return signs;
}

struct GlobalFitContext
{
    int M = 0;
    int P = 0;
    size_t n_total = 0;
    std::vector<std::vector<double>> curvesX;
    std::vector<std::vector<double>> curvesY;
    std::vector<std::vector<double>> curvesSigma;
    std::vector<size_t> rowOffsets;
    std::vector<std::vector<double>> baseParams; // [M][P]
    std::vector<GlobalFitSlot> freeSlots;
    MyParser *parser = nullptr;
    double *var_x = nullptr;
    std::vector<double> *var_p = nullptr;
    const QElapsedTimer *wallTimer = nullptr;
    qint64 maxWallMs = 0;
    bool timedOut = false;

    bool checkTimedOut()
    {
        if (timedOut)
            return true;
        if (wallTimer && maxWallMs > 0 && wallTimer->isValid() && wallTimer->elapsed() > maxWallMs) {
            timedOut = true;
            return true;
        }
        return false;
    }

    void unpack(const gsl_vector *u, std::vector<std::vector<double>> &outParams) const
    {
        outParams = baseParams;
        for (size_t k = 0; k < freeSlots.size(); ++k) {
            const auto &sl = freeSlots[k];
            double pv = sl.mapUtoP(gsl_vector_get(u, k));
            if (sl.curveIdx < 0) {
                for (int m = 0; m < M; ++m)
                    outParams[size_t(m)][size_t(sl.paramIdx)] = pv;
            } else {
                outParams[size_t(sl.curveIdx)][size_t(sl.paramIdx)] = pv;
            }
        }
    }

    bool evalCurveY(const std::vector<double> &pVec, const std::vector<double> &xv,
                    std::vector<double> &yvOut) const
    {
        for (int j = 0; j < P; ++j)
            (*var_p)[size_t(j)] = pVec[size_t(j)];
        yvOut.resize(xv.size());
        try {
            for (size_t i = 0; i < xv.size(); ++i) {
                *var_x = xv[i];
                double val = parser->Eval();
                if (!std::isfinite(val))
                    val = 1e100;
                yvOut[i] = val;
            }
        } catch (...) {
            return false;
        }
        return true;
    }

    static int gsl_f(const gsl_vector *u, void *data, gsl_vector *f)
    {
        GlobalFitContext *ctx = static_cast<GlobalFitContext *>(data);
        if (ctx->checkTimedOut())
            return GSL_EBADFUNC;
        std::vector<std::vector<double>> params;
        ctx->unpack(u, params);
        std::vector<double> yhat;
        for (int m = 0; m < ctx->M; ++m) {
            if (ctx->checkTimedOut())
                return GSL_EBADFUNC;
            if (!ctx->evalCurveY(params[size_t(m)], ctx->curvesX[size_t(m)], yhat))
                return GSL_EINVAL;
            size_t off = ctx->rowOffsets[size_t(m)];
            for (size_t i = 0; i < yhat.size(); ++i) {
                double s = ctx->curvesSigma[size_t(m)][i];
                gsl_vector_set(f, off + i, (yhat[i] - ctx->curvesY[size_t(m)][i]) / s);
            }
        }
        return GSL_SUCCESS;
    }

    static int gsl_df(const gsl_vector *u, void *data, gsl_matrix *J)
    {
        GlobalFitContext *ctx = static_cast<GlobalFitContext *>(data);
        if (ctx->checkTimedOut())
            return GSL_EBADFUNC;
        gsl_matrix_set_zero(J);
        std::vector<std::vector<double>> params;
        ctx->unpack(u, params);
        std::vector<double> yPlus, yMinus;

        for (size_t k = 0; k < ctx->freeSlots.size(); ++k) {
            if (ctx->checkTimedOut())
                return GSL_EBADFUNC;
            const auto &sl = ctx->freeSlots[k];
            double uk = gsl_vector_get(u, k);
            double pk = sl.mapUtoP(uk);
            double dpdu = sl.dPdU(uk);
            double h = std::max(1e-12, 1e-6 * std::abs(pk));

            int mStart = (sl.curveIdx < 0) ? 0 : sl.curveIdx;
            int mEnd = (sl.curveIdx < 0) ? (ctx->M - 1) : sl.curveIdx;
            for (int m = mStart; m <= mEnd; ++m) {
                std::vector<double> pPlus = params[size_t(m)];
                std::vector<double> pMinus = params[size_t(m)];
                pPlus[size_t(sl.paramIdx)] = pk + h;
                pMinus[size_t(sl.paramIdx)] = pk - h;
                if (!ctx->evalCurveY(pPlus, ctx->curvesX[size_t(m)], yPlus)
                    || !ctx->evalCurveY(pMinus, ctx->curvesX[size_t(m)], yMinus))
                    return GSL_EINVAL;
                size_t off = ctx->rowOffsets[size_t(m)];
                for (size_t i = 0; i < yPlus.size(); ++i) {
                    double s = ctx->curvesSigma[size_t(m)][i];
                    double dfdp = (yPlus[i] - yMinus[i]) / (2.0 * h * s);
                    gsl_matrix_set(J, off + i, k, dfdp * dpdu);
                }
            }
        }
        return GSL_SUCCESS;
    }

    static int gsl_fdf(const gsl_vector *u, void *data, gsl_vector *f, gsl_matrix *J)
    {
        int st = gsl_f(u, data, f);
        if (st != GSL_SUCCESS)
            return st;
        return gsl_df(u, data, J);
    }
};

struct ImplicitODRContext
{
    size_t n = 0;
    int P = 0;
    const std::vector<double> *X = nullptr;
    const std::vector<double> *Y = nullptr;
    std::vector<double> baseP;
    std::vector<GlobalFitSlot> freeSlots;
    MyParser *parser = nullptr;
    double *var_x = nullptr;
    double *var_y = nullptr;
    std::vector<double> *var_p = nullptr;
    const QElapsedTimer *wallTimer = nullptr;
    qint64 maxWallMs = 0;
    bool timedOut = false;
    std::vector<double> workP;
    std::vector<double> workPPlus;
    std::vector<double> workPMinus;
    std::vector<double> workR;
    std::vector<double> workRPlus;
    std::vector<double> workRMinus;

    bool checkTimedOut()
    {
        if (timedOut)
            return true;
        if (wallTimer && maxWallMs > 0 && wallTimer->isValid() && wallTimer->elapsed() > maxWallMs) {
            timedOut = true;
            return true;
        }
        return false;
    }

    void unpack(const gsl_vector *u, std::vector<double> &pOut) const
    {
        pOut = baseP;
        for (size_t k = 0; k < freeSlots.size(); ++k)
            pOut[size_t(freeSlots[k].paramIdx)] = freeSlots[k].mapUtoP(gsl_vector_get(u, k));
    }

    double evalF(double xVal, double yVal, const std::vector<double> &pVec) const
    {
        for (int j = 0; j < P; ++j)
            (*var_p)[size_t(j)] = pVec[size_t(j)];
        *var_x = xVal;
        *var_y = yVal;
        try {
            double v = parser->Eval();
            return std::isfinite(v) ? v : 1e50;
        } catch (...) {
            return 1e50;
        }
    }

    bool evalResiduals(const std::vector<double> &pVec, std::vector<double> &resOut) const
    {
        for (int j = 0; j < P; ++j)
            (*var_p)[size_t(j)] = pVec[size_t(j)];
        resOut.resize(n);
        try {
            for (size_t i = 0; i < n; ++i) {
                double xi = (*X)[i];
                double yi = (*Y)[i];
                double hx = std::max(1e-7, 1e-6 * std::abs(xi));
                double hy = std::max(1e-7, 1e-6 * std::abs(yi));

                *var_x = xi;
                *var_y = yi;
                double F0 = parser->Eval();
                if (!std::isfinite(F0)) {
                    resOut[i] = 1e25;
                    continue;
                }

                *var_x = xi + hx;
                double Fxp = parser->Eval();
                *var_x = xi - hx;
                double Fxm = parser->Eval();
                double Fx = (std::isfinite(Fxp) && std::isfinite(Fxm))
                        ? ((Fxp - Fxm) / (2.0 * hx))
                        : 0.0;

                *var_x = xi;
                *var_y = yi + hy;
                double Fyp = parser->Eval();
                *var_y = yi - hy;
                double Fym = parser->Eval();
                double Fy = (std::isfinite(Fyp) && std::isfinite(Fym))
                        ? ((Fyp - Fym) / (2.0 * hy))
                        : 0.0;

                double gradNorm = std::sqrt(Fx * Fx + Fy * Fy + 1e-16);
                double r = F0 / gradNorm;
                resOut[i] = std::isfinite(r) ? r : 1e25;
            }
        } catch (...) {
            return false;
        }
        return true;
    }

    static int gsl_f(const gsl_vector *u, void *data, gsl_vector *f)
    {
        ImplicitODRContext *ctx = static_cast<ImplicitODRContext *>(data);
        if (ctx->checkTimedOut())
            return GSL_EBADFUNC;
        ctx->unpack(u, ctx->workP);
        if (!ctx->evalResiduals(ctx->workP, ctx->workR))
            return GSL_EINVAL;
        for (size_t i = 0; i < ctx->n; ++i)
            gsl_vector_set(f, i, ctx->workR[i]);
        return GSL_SUCCESS;
    }

    static int gsl_df(const gsl_vector *u, void *data, gsl_matrix *J)
    {
        ImplicitODRContext *ctx = static_cast<ImplicitODRContext *>(data);
        if (ctx->checkTimedOut())
            return GSL_EBADFUNC;
        ctx->unpack(u, ctx->workP);
        ctx->workPPlus = ctx->workP;
        ctx->workPMinus = ctx->workP;
        for (size_t k = 0; k < ctx->freeSlots.size(); ++k) {
            if (ctx->checkTimedOut())
                return GSL_EBADFUNC;
            const auto &sl = ctx->freeSlots[k];
            double uk = gsl_vector_get(u, k);
            double pk = sl.mapUtoP(uk);
            double dpdu = sl.dPdU(uk);
            double h = std::max(1e-12, 1e-5 * std::abs(pk));
            const size_t pIdx = size_t(sl.paramIdx);
            ctx->workPPlus[pIdx] = pk + h;
            ctx->workPMinus[pIdx] = pk - h;
            const bool okEval = ctx->evalResiduals(ctx->workPPlus, ctx->workRPlus)
                    && ctx->evalResiduals(ctx->workPMinus, ctx->workRMinus);
            ctx->workPPlus[pIdx] = ctx->workP[pIdx];
            ctx->workPMinus[pIdx] = ctx->workP[pIdx];
            if (!okEval)
                return GSL_EINVAL;
            const double inv2h = dpdu / (2.0 * h);
            for (size_t i = 0; i < ctx->n; ++i)
                gsl_matrix_set(J, i, k, (ctx->workRPlus[i] - ctx->workRMinus[i]) * inv2h);
        }
        return GSL_SUCCESS;
    }

    static int gsl_fdf(const gsl_vector *u, void *data, gsl_vector *f, gsl_matrix *J)
    {
        int st = gsl_f(u, data, f);
        if (st != GSL_SUCCESS)
            return st;
        return gsl_df(u, data, J);
    }
};

static QString normalizeImplicitExpr(const QString &expr)
{
    QString s = expr.trimmed();
    int eqPos = -1;
    for (int i = 0; i < s.length(); ++i) {
        if (s[i] == QLatin1Char('=')) {
            QChar prev = (i > 0) ? s[i - 1] : QChar();
            QChar next = (i + 1 < s.length()) ? s[i + 1] : QChar();
            if (prev != QLatin1Char('<') && prev != QLatin1Char('>') && prev != QLatin1Char('!')
                && prev != QLatin1Char('=') && next != QLatin1Char('=')) {
                eqPos = i;
                break;
            }
        }
    }
    if (eqPos >= 0) {
        QString lhs = s.left(eqPos).trimmed();
        QString rhs = s.mid(eqPos + 1).trimmed();
        if (!lhs.isEmpty() && !rhs.isEmpty())
            return QString("(%1)-(%2)").arg(lhs, rhs);
    }
    return s;
}

} // namespace

bool NonLinearFit::fitGlobal(const QStringList &curveTitles, const QVector<bool> &sharedFlags,
                             Table **summaryOut)
{
    if (summaryOut)
        *summaryOut = nullptr;
    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (!app || !d_graph || curveTitles.isEmpty() || d_p == 0 || d_formula.trimmed().isEmpty())
        return false;

    const int M = curveTitles.size();
    const int P = int(d_p);

    GlobalFitContext ctx;
    ctx.M = M;
    ctx.P = P;
    ctx.curvesX.resize(size_t(M));
    ctx.curvesY.resize(size_t(M));
    ctx.curvesSigma.resize(size_t(M));
    ctx.rowOffsets.resize(size_t(M), 0);
    ctx.baseParams.assign(size_t(M), std::vector<double>(size_t(P), 1.0));

    d_convergence = Converged;
    d_last_iterations = 0;
    d_fit_notes.clear();
    d_covar_scaled = false;
    d_has_custom_dof = false;
    d_has_custom_rss = false;
    d_has_custom_r2 = false;
    d_result_errors.clear();

    size_t n_total = 0;
    size_t n_active_total = 0;
    bool anyAssociatedErrors = false;
    bool allAssociatedErrors = (M > 0);
    for (int m = 0; m < M; ++m) {
        NonLinearFit loader(app, d_graph);
        loader.d_silent = true;
        loader.setParametersList(d_param_names);
        loader.setFormula(d_formula);
        if (!d_model_name.isEmpty())
            loader.d_model_name = d_model_name;
        if (!loader.setDataFromCurve(curveTitles[m], d_graph) || loader.error() || loader.x().empty())
            return false;

        ctx.curvesX[size_t(m)] = loader.x();
        ctx.curvesY[size_t(m)] = loader.y();
        ctx.curvesSigma[size_t(m)] = loader.d_y_errors.empty()
                ? std::vector<double>(loader.x().size(), 1.0)
                : loader.d_y_errors;
        if (loader.d_y_error_source != UnknownErrors)
            anyAssociatedErrors = true;
        else
            allAssociatedErrors = false;
        for (double s : ctx.curvesSigma[size_t(m)]) {
            if (std::isfinite(s) && s > 0.0 && s < 1e140)
                ++n_active_total;
        }
        ctx.rowOffsets[size_t(m)] = n_total;
        n_total += loader.x().size();

        loader.guessInitialValues();
        for (int j = 0; j < P; ++j) {
            if (d_all_guesses_set || d_explicit_init_vals.contains(j))
                ctx.baseParams[size_t(m)][size_t(j)] = initialGuess(j);
            else
                ctx.baseParams[size_t(m)][size_t(j)] = loader.initialGuess(j);
        }
    }
    ctx.n_total = n_total;

    // Average auto-guessed shared parameters across datasets unless explicitly set
    for (int j = 0; j < P; ++j) {
        bool isShared = (j < sharedFlags.size()) ? sharedFlags[j] : false;
        if (isShared && !d_all_guesses_set && !d_explicit_init_vals.contains(j)) {
            double avg = 0.0;
            for (int m = 0; m < M; ++m)
                avg += ctx.baseParams[size_t(m)][size_t(j)];
            avg /= double(M);
            for (int m = 0; m < M; ++m)
                ctx.baseParams[size_t(m)][size_t(j)] = avg;
        }
    }

    std::vector<double> u0;
    auto appendSlot = [&](int paramIdx, int curveIdx, double pInit) {
        GlobalFitSlot sl;
        sl.paramIdx = paramIdx;
        sl.curveIdx = curveIdx;
        double lo = -std::numeric_limits<double>::infinity();
        double hi = std::numeric_limits<double>::infinity();
        bool bounded = hasParameterBounds(paramIdx, &lo, &hi);
        if (bounded && std::isfinite(lo) && std::isfinite(hi)) {
            if (std::abs(hi - lo) <= 1e-15) {
                double mid = 0.5 * (lo + hi);
                if (curveIdx < 0) {
                    for (int m = 0; m < M; ++m)
                        ctx.baseParams[size_t(m)][size_t(paramIdx)] = mid;
                } else {
                    ctx.baseParams[size_t(curveIdx)][size_t(paramIdx)] = mid;
                }
                return;
            }
            sl.mode = GBoundBoth;
            sl.lo = lo;
            sl.hi = hi;
            double t = std::max(1e-4, std::min(1.0 - 1e-4, (pInit - lo) / (hi - lo)));
            u0.push_back(std::atanh(2.0 * t - 1.0));
        } else if (bounded && std::isfinite(lo)) {
            sl.mode = GBoundLower;
            sl.lo = lo;
            u0.push_back(std::sqrt(std::max(1e-3, pInit - lo)));
        } else if (bounded && std::isfinite(hi)) {
            sl.mode = GBoundUpper;
            sl.hi = hi;
            u0.push_back(std::sqrt(std::max(1e-3, hi - pInit)));
        } else {
            sl.mode = GBoundNone;
            u0.push_back(pInit);
        }
        ctx.freeSlots.push_back(sl);
    };

    for (int j = 0; j < P; ++j) {
        if (isParameterFixed(j))
            continue;
        bool isShared = (j < sharedFlags.size()) ? sharedFlags[j] : false;
        if (isShared) {
            appendSlot(j, -1, ctx.baseParams[0][size_t(j)]);
        } else {
            for (int m = 0; m < M; ++m)
                appendSlot(j, m, ctx.baseParams[size_t(m)][size_t(j)]);
        }
    }

    const size_t n_free = ctx.freeSlots.size();
    if (n_active_total < std::max<size_t>(1, n_free))
        return false;

    MyParser parser;
    double var_x = 0.0;
    std::vector<double> var_p(size_t(P), 0.0);
    parser.DefineVar(toString<mu::string_type>(QString("x")), &var_x);
    for (int j = 0; j < P; ++j)
        parser.DefineVar(toString<mu::string_type>(d_param_names[j]), &var_p[size_t(j)]);

    QString exprLine = d_formula.section('\n', -1).trimmed();
    try {
        parser.SetExpr(exprLine);
        (void)parser.Eval();
    } catch (...) {
        return false;
    }

    ctx.parser = &parser;
    ctx.var_x = &var_x;
    ctx.var_p = &var_p;

    std::vector<std::vector<double>> fittedParams = ctx.baseParams;
    const double nanVal = std::numeric_limits<double>::quiet_NaN();
    std::vector<std::vector<double>> fittedErrors(size_t(M), std::vector<double>(size_t(P), 0.0));
    std::vector<double> curve0UnscaledCov(size_t(P) * size_t(P), 0.0);
    double rss_total = 0.0;

    if (n_free > 0) {
        gsl_vector *u_init = gsl_vector_alloc(n_free);
        for (size_t k = 0; k < n_free; ++k)
            gsl_vector_set(u_init, k, u0[k]);

        gsl_multifit_function_fdf fdf;
        fdf.f = &GlobalFitContext::gsl_f;
        fdf.df = &GlobalFitContext::gsl_df;
        fdf.fdf = &GlobalFitContext::gsl_fdf;
        fdf.n = n_total;
        fdf.p = n_free;
        fdf.params = &ctx;

        QElapsedTimer solverTimer;
        if (isNonInteractiveFit(parent(), d_silent)) {
            solverTimer.start();
            ctx.wallTimer = &solverTimer;
            ctx.maxWallMs = 2500;
        }

        gsl_multifit_fdfsolver *s =
                gsl_multifit_fdfsolver_alloc(gsl_multifit_fdfsolver_lmsder, n_total, n_free);
        int status = gsl_multifit_fdfsolver_set(s, &fdf, u_init);
        if (status != GSL_SUCCESS) {
            d_convergence = NoProgress;
            chi_2 = -1.0;
            gsl_multifit_fdfsolver_free(s);
            gsl_vector_free(u_init);
            return false;
        }

        const int maxIter = std::max(1, d_max_iterations);
        int iter = 0;
        bool deltaConverged = false;
        int hardErrorStatus = GSL_SUCCESS;
        for (iter = 0; iter < maxIter; ++iter) {
            if (ctx.checkTimedOut())
                break;
            status = gsl_multifit_fdfsolver_iterate(s);
            if (ctx.timedOut)
                break;
            if (status) {
                if (status == GSL_ETOLF || status == GSL_ETOLX || status == GSL_ETOLG) {
                    deltaConverged = true;
                } else if (status == GSL_ENOPROG) {
                    const std::vector<int> kkt = slotKktSigns(ctx.freeSlots, s->x);
                    if (!lmGradientIsStationary(s, 1e-4, &kkt))
                        d_convergence = NoProgress;
                    deltaConverged = (d_convergence == Converged);
                } else {
                    d_convergence = NoProgress;
                    hardErrorStatus = status;
                }
                ++iter;
                break;
            }
            const std::vector<int> kkt = slotKktSigns(ctx.freeSlots, s->x);
            if (lmStepConverged(s, s->dx, s->x, d_tolerance, &kkt)) {
                deltaConverged = true;
                ++iter;
                break;
            }
        }
        d_last_iterations = iter;

        if (ctx.timedOut || hardErrorStatus != GSL_SUCCESS) {
            if (ctx.timedOut)
                d_convergence = MaxIterationsReached;
            chi_2 = -1.0;
            d_results.assign(d_p, nanVal);
            gsl_multifit_fdfsolver_free(s);
            gsl_vector_free(u_init);
            return false;
        }
        if (!deltaConverged && d_convergence == Converged)
            d_convergence = MaxIterationsReached;

        ctx.unpack(s->x, fittedParams);
        // A parameter on its bound reports the bound and a note, as in the single-curve fit.
        for (size_t k = 0; k < n_free; ++k) {
            const GlobalFitSlot &sl = ctx.freeSlots[k];
            const double uk = gsl_vector_get(s->x, k);
            if (sl.boundSide(uk) == 0)
                continue;
            const double pv = sl.snapP(sl.mapUtoP(uk));
            if (sl.curveIdx < 0) {
                for (int m = 0; m < M; ++m)
                    fittedParams[size_t(m)][size_t(sl.paramIdx)] = pv;
                addBoundNote(d_param_names[sl.paramIdx], pv);
            } else {
                fittedParams[size_t(sl.curveIdx)][size_t(sl.paramIdx)] = pv;
                addBoundNote(QStringLiteral("%1 (%2)")
                                     .arg(d_param_names[sl.paramIdx], curveTitles[sl.curveIdx]),
                             pv);
            }
        }
        gsl_blas_ddot(s->f, s->f, &rss_total);
        if (!std::isfinite(rss_total) || rss_total < 0.0) {
            d_convergence = NoProgress;
            chi_2 = -1.0;
            d_results.assign(d_p, nanVal);
            gsl_multifit_fdfsolver_free(s);
            gsl_vector_free(u_init);
            return false;
        }

        // Compute UNSCALED covariance (J_p^T J_p)^-1 in physical slot space
        gsl_matrix *J_u = gsl_matrix_alloc(n_total, n_free);
        gsl_matrix *cov_p = gsl_matrix_alloc(n_free, n_free);
#if GSL_MAJOR_VERSION < 2
        gsl_matrix_memcpy(J_u, s->J);
#else
        gsl_multifit_fdfsolver_jac(s, J_u);
#endif
        std::vector<bool> slotPinned(n_free, false);
        for (size_t k = 0; k < n_free; ++k) {
            double dpdu = ctx.freeSlots[k].dPdU(gsl_vector_get(s->x, k));
            if (std::abs(dpdu) > 1e-15) {
                for (size_t i = 0; i < n_total; ++i)
                    gsl_matrix_set(J_u, i, k, gsl_matrix_get(J_u, i, k) / dpdu);
            } else {
                slotPinned[k] = true;
                for (size_t i = 0; i < n_total; ++i)
                    gsl_matrix_set(J_u, i, k, 0.0);
            }
        }
        gsl_multifit_covar(J_u, 0.0, cov_p);

        const int globalDof = int(n_active_total) - int(n_free);
        const bool scaleByResidual = (!allAssociatedErrors || d_scale_errors);
        const double scale = scaleByResidual
                ? ((globalDof > 0) ? (rss_total / double(globalDof)) : nanVal)
                : 1.0;

        for (size_t k = 0; k < n_free; ++k) {
            const auto &sl = ctx.freeSlots[k];
            const double rawVar = gsl_matrix_get(cov_p, k, k);
            const double se = (!slotPinned[k] && std::isfinite(scale) && std::isfinite(rawVar))
                    ? std::sqrt(std::max(0.0, scale * rawVar))
                    : nanVal;
            if (sl.curveIdx < 0) {
                for (int m = 0; m < M; ++m)
                    fittedErrors[size_t(m)][size_t(sl.paramIdx)] = se;
            } else {
                fittedErrors[size_t(sl.curveIdx)][size_t(sl.paramIdx)] = se;
            }
        }

        // Store unscaled submatrix for curve 0 (shared + curve 0 local slots) so Fit::parameterStdError /
        // Fit::errors / Fit::covarianceMatrix apply effectiveCovarScale() exactly once (no double scaling).
        for (size_t a = 0; a < n_free; ++a) {
            const auto &sa = ctx.freeSlots[a];
            if (sa.curveIdx > 0)
                continue;
            for (size_t b = 0; b < n_free; ++b) {
                const auto &sb = ctx.freeSlots[b];
                if (sb.curveIdx > 0)
                    continue;
                curve0UnscaledCov[size_t(sa.paramIdx) * size_t(P) + size_t(sb.paramIdx)] =
                        (a == b && slotPinned[a]) ? nanVal : gsl_matrix_get(cov_p, a, b);
            }
        }

        gsl_matrix_free(J_u);
        gsl_matrix_free(cov_p);
        gsl_multifit_fdfsolver_free(s);
        gsl_vector_free(u_init);
    } else {
        for (int m = 0; m < M; ++m) {
            std::vector<double> yhat;
            if (ctx.evalCurveY(fittedParams[size_t(m)], ctx.curvesX[size_t(m)], yhat)) {
                for (size_t i = 0; i < yhat.size(); ++i) {
                    const double s = ctx.curvesSigma[size_t(m)][i];
                    if (std::isfinite(s) && s > 0.0 && s < 1e140) {
                        const double r = (yhat[i] - ctx.curvesY[size_t(m)][i]) / s;
                        rss_total += r * r;
                    }
                }
            }
        }
    }

    // Compute per-curve R^2 and global R^2, and plot fitted curves on d_graph
    std::vector<double> curveR2(size_t(M), nanVal);
    double tss_total = 0.0;
    double unwt_rss_total = 0.0;
    double sumSqY_total = 0.0;
    size_t r2ActiveTotal = 0;
    bool allEvalOk = true;
    for (int m = 0; m < M; ++m) {
        std::vector<double> yhat;
        const auto &yv = ctx.curvesY[size_t(m)];
        const auto &sv = ctx.curvesSigma[size_t(m)];
        const bool evalOk = ctx.evalCurveY(fittedParams[size_t(m)], ctx.curvesX[size_t(m)], yhat)
                && yhat.size() == yv.size() && sv.size() == yv.size();
        if (evalOk) {
            double sumY = 0.0;
            size_t nActiveM = 0;
            for (size_t i = 0; i < yv.size(); ++i) {
                const double s = sv[i];
                if (!std::isfinite(s) || !(s > 0.0) || s >= 1e140)
                    continue;
                sumY += yv[i];
                ++nActiveM;
            }
            if (nActiveM > 0) {
                const double meanY = sumY / double(nActiveM);
                double tss_m = 0.0, rss_m = 0.0, sumSqY_m = 0.0;
                for (size_t i = 0; i < yv.size(); ++i) {
                    const double s = sv[i];
                    if (!std::isfinite(s) || !(s > 0.0) || s >= 1e140)
                        continue;
                    double dt = yv[i] - meanY;
                    double dr = yv[i] - yhat[i];
                    tss_m += dt * dt;
                    rss_m += dr * dr;
                    sumSqY_m += yv[i] * yv[i];
                }
                // "tss = 0" relative to the size of the data, not below 1e-30 (every curve of
                // size 1e-22 had no R^2)
                curveR2[size_t(m)] = !sumOfSquaresNegligible(tss_m, sumSqY_m)
                        ? (1.0 - rss_m / tss_m)
                        : (sumOfSquaresNegligible(rss_m, sumSqY_m) ? 1.0 : nanVal);
                tss_total += tss_m;
                unwt_rss_total += rss_m;
                sumSqY_total += sumSqY_m;
                r2ActiveTotal += nActiveM;
            } else {
                allEvalOk = false;
            }
        } else {
            allEvalOk = false;
        }

        if (!d_suppress_curve_output && d_graph && !ctx.curvesX[size_t(m)].empty()) {
            int nGrid = std::max(2, d_points);
            double x0 = *std::min_element(ctx.curvesX[size_t(m)].begin(),
                                          ctx.curvesX[size_t(m)].end());
            double x1 = *std::max_element(ctx.curvesX[size_t(m)].begin(),
                                          ctx.curvesX[size_t(m)].end());
            std::vector<double> xg(static_cast<size_t>(nGrid)), yg(static_cast<size_t>(nGrid));
            double step = (x1 - x0) / double(nGrid - 1);
            for (int i = 0; i < nGrid; ++i)
                xg[size_t(i)] = x0 + i * step;
            if (ctx.evalCurveY(fittedParams[size_t(m)], xg, yg) && yg.size() == size_t(nGrid)) {
                QString fitCurveName = d_graph->generateFunctionName(
                        QString("GlobalFit_%1").arg(curveTitles[m]));
                FunctionCurve *fc = new FunctionCurve(app, FunctionCurve::Normal, fitCurveName);
                fc->setPen(QPen(ColorButton::color((m + 1) % 10), 2));
                fc->setData(xg.data(), yg.data(), nGrid);
                fc->setRange(x0, x1);
                // The curve is sampled again from its formula when a project is reopened: the
                // formula carries this curve's parameter values as a single fit's does
                // (Fit::insertFitFunctionCurve). With the bare expression the parameters were
                // undefined there, every point became NaN and the curve vanished (and an
                // automatic axis lost its range).
                QString curveFormula;
                for (int j = 0; j < P && j < d_param_names.size(); ++j)
                    curveFormula += QString("%1=%2\n")
                                            .arg(d_param_names[j])
                                            .arg(fittedParams[size_t(m)][size_t(j)], 0, 'g',
                                                 d_prec);
                curveFormula += "\n";
                curveFormula += exprLine;
                fc->setFormula(curveFormula);
                // 各データ曲線と同じ軸に付ける（右軸の曲線のフィットが左軸に付かないように）。
                if (QwtPlotCurve *src = d_graph->curve(curveTitles[m]))
                    attachToSourceAxes(fc, src);
                d_graph->insertPlotItem(fc, Graph::Line);
                d_graph->addFitCurve(fc);
            }
        }
    }
    if (!d_suppress_curve_output && d_graph)
        d_graph->replot();

    double globalR2 = (allEvalOk && r2ActiveTotal > 0)
            ? (!sumOfSquaresNegligible(tss_total, sumSqY_total)
                       ? (1.0 - unwt_rss_total / tss_total)
                       : (sumOfSquaresNegligible(unwt_rss_total, sumSqY_total) ? 1.0 : nanVal))
            : nanVal;

    // Store curve 0 state & global diagnostics (total active points, global free parameters,
    // global weighted chi^2, global unweighted RSS, and UNSCALED covariance) on this instance.
    setDataFromCurve(curveTitles[0], d_graph);
    d_y_error_source = allAssociatedErrors ? AssociatedErrors : UnknownErrors;
    d_results = fittedParams[0];
    d_result_errors.clear();
    if (!ctx.evalCurveY(d_results, d_x, d_y_fit) || d_y_fit.size() != d_x.size())
        d_y_fit.assign(d_x.size(), nanVal);
    chi_2 = rss_total;
    d_has_custom_rss = true;
    d_custom_rss = unwt_rss_total;
    d_has_custom_dof = true;
    d_custom_n_points = int(n_active_total);
    d_custom_p_free = int(n_free);
    d_has_custom_r2 = true;
    d_custom_r2 = globalR2;
    if (covar) {
        gsl_matrix_set_zero(covar);
        for (int a = 0; a < P; ++a) {
            for (int b = 0; b < P; ++b)
                gsl_matrix_set(covar, a, b, curve0UnscaledCov[size_t(a) * size_t(P) + size_t(b)]);
        }
    }

    // Create Global Fit Summary Table
    if (summaryOut || !d_silent) {
        QString tblName = app->generateUniqueName(tr("GlobalFitSummary"));
        int totalRows = M * P;
        Table *tbl = app->newTable(tblName, totalRows, 7);
        if (tbl) {
            tbl->setHeader(QStringList() << tr("Dataset") << tr("Parameter") << tr("Shared")
                                         << tr("Value") << tr("Error") << tr("Curve_R2")
                                         << tr("Global_R2"));
            tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
            tbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Text);
            tbl->column(2)->setColumnMode(SciDAVis::ColumnMode::Text);
            for (int c = 3; c < 7; ++c)
                tbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);

            int rowIdx = 0;
            for (int m = 0; m < M; ++m) {
                for (int j = 0; j < P; ++j) {
                    bool isShared = (j < sharedFlags.size()) ? sharedFlags[j] : false;
                    tbl->column(0)->setTextAt(rowIdx, curveTitles[m]);
                    tbl->column(1)->setTextAt(rowIdx, d_param_names[j]);
                    tbl->column(2)->setTextAt(rowIdx, isShared ? QString("Yes") : QString("No"));
                    tbl->column(3)->setValueAt(rowIdx, fittedParams[size_t(m)][size_t(j)]);
                    const double errVal = fittedErrors[size_t(m)][size_t(j)];
                    if (std::isfinite(errVal))
                        tbl->column(4)->setValueAt(rowIdx, errVal);
                    else
                        tbl->column(4)->setInvalid(rowIdx, true);
                    if (std::isfinite(curveR2[size_t(m)]))
                        tbl->column(5)->setValueAt(rowIdx, curveR2[size_t(m)]);
                    else
                        tbl->column(5)->setInvalid(rowIdx, true);
                    if (std::isfinite(globalR2))
                        tbl->column(6)->setValueAt(rowIdx, globalR2);
                    else
                        tbl->column(6)->setInvalid(rowIdx, true);
                    ++rowIdx;
                }
            }
            tbl->showNormal();
            if (summaryOut)
                *summaryOut = tbl;
        }
    }

    if (app && app->writeFitResultsToLog && !d_silent && d_graph)
        app->updateLog(logFitInfo(d_results, d_last_iterations, GSL_SUCCESS, d_graph->parentPlotName()));

    return true;
}

bool NonLinearFit::fitImplicitODR(const QString &implicitExprFxy, const QStringList &paramNames,
                                  const QVector<double> &initVals)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (d_init_err || d_x.empty() || d_y.size() != d_x.size() || paramNames.isEmpty())
        return false;

    QString normExpr = normalizeImplicitExpr(implicitExprFxy);
    if (normExpr.isEmpty())
        return false;

    // Preserve any pre-set fixed/bound flags across setParametersList
    std::vector<bool> savedFixed = d_param_fixed;
    std::vector<bool> savedBounded = d_param_bounded;
    std::vector<double> savedMin = d_param_min;
    std::vector<double> savedMax = d_param_max;

    setParametersList(paramNames);
    setFormula(normExpr);

    d_param_fixed = savedFixed;
    d_param_bounded = savedBounded;
    d_param_min = savedMin;
    d_param_max = savedMax;

    const int P = int(d_p);
    const size_t n = d_x.size();

    ImplicitODRContext ctx;
    ctx.n = n;
    ctx.P = P;
    ctx.X = &d_x;
    ctx.Y = &d_y;
    ctx.baseP.resize(size_t(P), 1.0);
    ctx.workP.reserve(size_t(P));
    ctx.workPPlus.reserve(size_t(P));
    ctx.workPMinus.reserve(size_t(P));
    ctx.workR.reserve(n);
    ctx.workRPlus.reserve(n);
    ctx.workRMinus.reserve(n);

    const auto xMM = std::minmax_element(d_x.begin(), d_x.end());
    const auto yMM = std::minmax_element(d_y.begin(), d_y.end());
    const double midX = 0.5 * (*xMM.first + *xMM.second);
    const double midY = 0.5 * (*yMM.first + *yMM.second);
    const double halfSpanX = std::max(1e-3, 0.5 * (*xMM.second - *xMM.first));
    const double halfSpanY = std::max(1e-3, 0.5 * (*yMM.second - *yMM.first));

    for (int j = 0; j < P; ++j) {
        double v = 1.0;
        if (j < initVals.size()) {
            v = initVals[j];
        } else {
            const QString pn = paramNames[j].trimmed().toLower();
            if (pn == QLatin1String("x0") || pn == QLatin1String("xc") || pn == QLatin1String("cx"))
                v = midX;
            else if (pn == QLatin1String("y0") || pn == QLatin1String("yc") || pn == QLatin1String("cy"))
                v = midY;
            else if (pn == QLatin1String("a") || pn == QLatin1String("rx"))
                v = halfSpanX;
            else if (pn == QLatin1String("b") || pn == QLatin1String("ry"))
                v = halfSpanY;
            else if (pn == QLatin1String("r") || pn == QLatin1String("r0"))
                v = 0.5 * (halfSpanX + halfSpanY);
        }
        ctx.baseP[size_t(j)] = v;
        gsl_vector_set(d_param_init, j, v);
    }

    std::vector<double> u0;
    for (int j = 0; j < P; ++j) {
        if (isParameterFixed(j))
            continue;
        GlobalFitSlot sl;
        sl.paramIdx = j;
        sl.curveIdx = -1;
        double pInit = ctx.baseP[size_t(j)];
        double lo = -std::numeric_limits<double>::infinity();
        double hi = std::numeric_limits<double>::infinity();
        bool bounded = hasParameterBounds(j, &lo, &hi);
        if (bounded && std::isfinite(lo) && std::isfinite(hi)) {
            if (std::abs(hi - lo) <= 1e-15) {
                ctx.baseP[size_t(j)] = 0.5 * (lo + hi);
                continue;
            }
            sl.mode = GBoundBoth;
            sl.lo = lo;
            sl.hi = hi;
            double t = std::max(1e-4, std::min(1.0 - 1e-4, (pInit - lo) / (hi - lo)));
            u0.push_back(std::atanh(2.0 * t - 1.0));
        } else if (bounded && std::isfinite(lo)) {
            sl.mode = GBoundLower;
            sl.lo = lo;
            u0.push_back(std::sqrt(std::max(1e-3, pInit - lo)));
        } else if (bounded && std::isfinite(hi)) {
            sl.mode = GBoundUpper;
            sl.hi = hi;
            u0.push_back(std::sqrt(std::max(1e-3, hi - pInit)));
        } else {
            sl.mode = GBoundNone;
            u0.push_back(pInit);
        }
        ctx.freeSlots.push_back(sl);
    }

    const size_t n_free = ctx.freeSlots.size();
    if (n < std::max<size_t>(1, n_free))
        return false;

    MyParser parser;
    double var_x = 0.0, var_y = 0.0;
    std::vector<double> var_p(size_t(P), 1.0);
    parser.DefineVar(toString<mu::string_type>(QString("x")), &var_x);
    parser.DefineVar(toString<mu::string_type>(QString("y")), &var_y);
    for (int j = 0; j < P; ++j)
        parser.DefineVar(toString<mu::string_type>(paramNames[j]), &var_p[size_t(j)]);

    try {
        parser.SetExpr(normExpr);
        (void)parser.Eval();
    } catch (...) {
        return false;
    }

    ctx.parser = &parser;
    ctx.var_x = &var_x;
    ctx.var_y = &var_y;
    ctx.var_p = &var_p;

    d_convergence = Converged;
    d_last_iterations = 0;
    d_fit_notes.clear();
    d_covar_scaled = false;
    d_has_custom_dof = false;
    d_has_custom_rss = false;
    d_has_custom_r2 = false;
    d_result_errors.clear();
    // Implicit ODR minimizes unweighted orthogonal geometric distances.
    d_y_error_source = UnknownErrors;
    d_weighting_method = WeightNone;

    std::vector<double> pOpt = ctx.baseP;
    const double nanVal = std::numeric_limits<double>::quiet_NaN();
    double ssr = 0.0;

    if (covar)
        gsl_matrix_set_zero(covar);

    if (n_free > 0) {
        gsl_vector *u_init = gsl_vector_alloc(n_free);
        for (size_t k = 0; k < n_free; ++k)
            gsl_vector_set(u_init, k, u0[k]);

        gsl_multifit_function_fdf fdf;
        fdf.f = &ImplicitODRContext::gsl_f;
        fdf.df = &ImplicitODRContext::gsl_df;
        fdf.fdf = &ImplicitODRContext::gsl_fdf;
        fdf.n = n;
        fdf.p = n_free;
        fdf.params = &ctx;

        QElapsedTimer solverTimer;
        if (isNonInteractiveFit(parent(), d_silent)) {
            solverTimer.start();
            ctx.wallTimer = &solverTimer;
            ctx.maxWallMs = 2500;
        }

        gsl_multifit_fdfsolver *s =
                gsl_multifit_fdfsolver_alloc(gsl_multifit_fdfsolver_lmsder, n, n_free);
        int status = gsl_multifit_fdfsolver_set(s, &fdf, u_init);
        if (status != GSL_SUCCESS) {
            d_convergence = NoProgress;
            chi_2 = -1.0;
            d_results.assign(d_p, nanVal);
            gsl_multifit_fdfsolver_free(s);
            gsl_vector_free(u_init);
            return false;
        }

        const int effMaxIter = std::max(1, d_max_iterations);
        int iter = 0;
        bool deltaConverged = false;
        int hardErrorStatus = GSL_SUCCESS;
        for (iter = 0; iter < effMaxIter; ++iter) {
            if (ctx.checkTimedOut())
                break;
            status = gsl_multifit_fdfsolver_iterate(s);
            if (ctx.timedOut)
                break;
            if (status) {
                if (status == GSL_ETOLF || status == GSL_ETOLX || status == GSL_ETOLG) {
                    deltaConverged = true;
                } else if (status == GSL_ENOPROG) {
                    const std::vector<int> kkt = slotKktSigns(ctx.freeSlots, s->x);
                    if (!lmGradientIsStationary(s, 1e-4, &kkt))
                        d_convergence = NoProgress;
                    deltaConverged = (d_convergence == Converged);
                } else {
                    d_convergence = NoProgress;
                    hardErrorStatus = status;
                }
                ++iter;
                break;
            }
            const std::vector<int> kkt = slotKktSigns(ctx.freeSlots, s->x);
            if (lmStepConverged(s, s->dx, s->x, d_tolerance, &kkt)) {
                deltaConverged = true;
                ++iter;
                break;
            }
        }
        d_last_iterations = iter;

        if (ctx.timedOut || hardErrorStatus != GSL_SUCCESS) {
            if (ctx.timedOut)
                d_convergence = MaxIterationsReached;
            chi_2 = -1.0;
            d_results.assign(d_p, nanVal);
            gsl_multifit_fdfsolver_free(s);
            gsl_vector_free(u_init);
            return false;
        }
        if (!deltaConverged && d_convergence == Converged)
            d_convergence = MaxIterationsReached;

        ctx.unpack(s->x, pOpt);
        for (size_t k = 0; k < n_free; ++k) {
            const GlobalFitSlot &sl = ctx.freeSlots[k];
            const double uk = gsl_vector_get(s->x, k);
            if (sl.boundSide(uk) == 0)
                continue;
            pOpt[size_t(sl.paramIdx)] = sl.snapP(sl.mapUtoP(uk));
            addBoundNote(d_param_names[sl.paramIdx], pOpt[size_t(sl.paramIdx)]);
        }
        gsl_blas_ddot(s->f, s->f, &ssr);
        if (!std::isfinite(ssr) || ssr < 0.0) {
            d_convergence = NoProgress;
            chi_2 = -1.0;
            d_results.assign(d_p, nanVal);
            gsl_multifit_fdfsolver_free(s);
            gsl_vector_free(u_init);
            return false;
        }

        // Store UNSCALED (J_p^T J_p)^-1 in covar so Fit::parameterStdError / Fit::errors /
        // Fit::covarianceMatrix apply effectiveCovarScale() = ssr / dof exactly once.
        gsl_matrix *J_u = gsl_matrix_alloc(n, n_free);
        gsl_matrix *cov_free = gsl_matrix_alloc(n_free, n_free);
#if GSL_MAJOR_VERSION < 2
        gsl_matrix_memcpy(J_u, s->J);
#else
        gsl_multifit_fdfsolver_jac(s, J_u);
#endif
        std::vector<bool> slotPinned(n_free, false);
        for (size_t k = 0; k < n_free; ++k) {
            double dpdu = ctx.freeSlots[k].dPdU(gsl_vector_get(s->x, k));
            if (std::abs(dpdu) > 1e-15) {
                for (size_t i = 0; i < n; ++i)
                    gsl_matrix_set(J_u, i, k, gsl_matrix_get(J_u, i, k) / dpdu);
            } else {
                slotPinned[k] = true;
                for (size_t i = 0; i < n; ++i)
                    gsl_matrix_set(J_u, i, k, 0.0);
            }
        }
        gsl_multifit_covar(J_u, 0.0, cov_free);

        if (covar) {
            gsl_matrix_set_zero(covar);
            for (size_t a = 0; a < n_free; ++a) {
                int ja = ctx.freeSlots[a].paramIdx;
                for (size_t b = 0; b < n_free; ++b) {
                    int jb = ctx.freeSlots[b].paramIdx;
                    gsl_matrix_set(covar, ja, jb,
                                   (a == b && slotPinned[a]) ? nanVal
                                                             : gsl_matrix_get(cov_free, a, b));
                }
            }
        }

        gsl_matrix_free(J_u);
        gsl_matrix_free(cov_free);
        gsl_multifit_fdfsolver_free(s);
        gsl_vector_free(u_init);
    } else {
        std::vector<double> r;
        ctx.evalResiduals(pOpt, r);
        for (double v : r)
            ssr += v * v;
    }

    // Preserve positive sign convention for even-symmetric parameters (e.g., semi-axes a, b in x^2/a^2 + y^2/b^2 - 1 = 0)
    for (int j = 0; j < P; ++j) {
        if (isParameterFixed(j))
            continue;
        double initV = (j < initVals.size()) ? initVals[j] : 1.0;
        if (initV > 0.0 && pOpt[size_t(j)] < 0.0) {
            double lo = -std::numeric_limits<double>::infinity();
            double hi = std::numeric_limits<double>::infinity();
            bool bounded = hasParameterBounds(j, &lo, &hi);
            double flipped = -pOpt[size_t(j)];
            if (!bounded || (flipped >= lo && flipped <= hi)) {
                std::vector<double> pTest = pOpt;
                pTest[size_t(j)] = flipped;
                std::vector<double> rTest;
                if (ctx.evalResiduals(pTest, rTest)) {
                    double ssrTest = 0.0;
                    for (double rv : rTest)
                        ssrTest += rv * rv;
                    if (std::abs(ssrTest - ssr) <= 1e-11 * (1.0 + ssr))
                        pOpt[size_t(j)] = flipped;
                }
            }
        }
    }

    d_results = pOpt;
    d_result_errors.clear();
    chi_2 = ssr;
    d_has_custom_rss = true;
    d_custom_rss = ssr;
    d_has_custom_dof = true;
    d_custom_n_points = int(n);
    d_custom_p_free = int(n_free);

    // Geometric R^2 relative to radial variance around centroid
    double mx = std::accumulate(d_x.begin(), d_x.end(), 0.0) / double(n);
    double my = std::accumulate(d_y.begin(), d_y.end(), 0.0) / double(n);
    double geomTss = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double dx = d_x[i] - mx;
        double dy = d_y[i] - my;
        geomTss += dx * dx + dy * dy;
    }
    d_has_custom_r2 = true;
    d_custom_r2 = (geomTss > 1e-30) ? std::max(0.0, 1.0 - ssr / geomTss) : 1.0;

    // Trace implicit zero-contour F(x, y; pOpt) = 0 via Marching Squares and plot onto d_graph
    if (!d_suppress_curve_output && d_graph) {
        double xMin = *std::min_element(d_x.begin(), d_x.end());
        double xMax = *std::max_element(d_x.begin(), d_x.end());
        double yMin = *std::min_element(d_y.begin(), d_y.end());
        double yMax = *std::max_element(d_y.begin(), d_y.end());
        double xPad = std::max(1e-3, 0.35 * (xMax - xMin));
        double yPad = std::max(1e-3, 0.35 * (yMax - yMin));
        xMin -= xPad;
        xMax += xPad;
        yMin -= yPad;
        yMax += yPad;

        const int G = 96;
        std::vector<double> gx(G), gy(G);
        for (int i = 0; i < G; ++i) {
            gx[i] = xMin + (xMax - xMin) * double(i) / double(G - 1);
            gy[i] = yMin + (yMax - yMin) * double(i) / double(G - 1);
        }
        std::vector<double> Fgrid(G * G, 0.0);
        for (int iy = 0; iy < G; ++iy) {
            for (int ix = 0; ix < G; ++ix)
                Fgrid[iy * G + ix] = ctx.evalF(gx[ix], gy[iy], pOpt);
        }

        auto interpEdge = [&](double x1, double y1, double f1, double x2, double y2,
                              double f2) -> QPointF {
            double denom = f1 - f2;
            double t = (std::abs(denom) > 1e-30) ? (f1 / denom) : 0.5;
            t = std::max(0.0, std::min(1.0, t));
            // Two secant/bisection refinement steps along the edge
            double ta = 0.0, tb = 1.0, fa = f1, fb = f2;
            for (int r = 0; r < 3; ++r) {
                double xm = x1 + t * (x2 - x1);
                double ym = y1 + t * (y2 - y1);
                double fm = ctx.evalF(xm, ym, pOpt);
                if ((fa <= 0.0 && fm >= 0.0) || (fa >= 0.0 && fm <= 0.0)) {
                    tb = t;
                    fb = fm;
                } else {
                    ta = t;
                    fa = fm;
                }
                double df = fa - fb;
                t = (std::abs(df) > 1e-30) ? (ta + (tb - ta) * (fa / df)) : (0.5 * (ta + tb));
                t = std::max(ta, std::min(tb, t));
            }
            return QPointF(x1 + t * (x2 - x1), y1 + t * (y2 - y1));
        };

        struct Seg
        {
            QPointF a, b;
            bool used = false;
        };
        std::vector<Seg> segs;
        for (int iy = 0; iy + 1 < G; ++iy) {
            for (int ix = 0; ix + 1 < G; ++ix) {
                double f00 = Fgrid[iy * G + ix];
                double f10 = Fgrid[iy * G + (ix + 1)];
                double f11 = Fgrid[(iy + 1) * G + (ix + 1)];
                double f01 = Fgrid[(iy + 1) * G + ix];
                int mask = (f00 >= 0.0 ? 1 : 0) | (f10 >= 0.0 ? 2 : 0) | (f11 >= 0.0 ? 4 : 0)
                        | (f01 >= 0.0 ? 8 : 0);
                if (mask == 0 || mask == 15)
                    continue;

                QPointF eBottom, eRight, eTop, eLeft;
                bool hasB = ((mask & 1) != 0) != ((mask & 2) != 0);
                bool hasR = ((mask & 2) != 0) != ((mask & 4) != 0);
                bool hasT = ((mask & 8) != 0) != ((mask & 4) != 0);
                bool hasL = ((mask & 1) != 0) != ((mask & 8) != 0);
                if (hasB)
                    eBottom = interpEdge(gx[ix], gy[iy], f00, gx[ix + 1], gy[iy], f10);
                if (hasR)
                    eRight = interpEdge(gx[ix + 1], gy[iy], f10, gx[ix + 1], gy[iy + 1], f11);
                if (hasT)
                    eTop = interpEdge(gx[ix], gy[iy + 1], f01, gx[ix + 1], gy[iy + 1], f11);
                if (hasL)
                    eLeft = interpEdge(gx[ix], gy[iy], f00, gx[ix], gy[iy + 1], f01);

                QVector<QPointF> pts;
                if (hasB)
                    pts.push_back(eBottom);
                if (hasR)
                    pts.push_back(eRight);
                if (hasT)
                    pts.push_back(eTop);
                if (hasL)
                    pts.push_back(eLeft);
                if (pts.size() == 2) {
                    segs.push_back({ pts[0], pts[1], false });
                } else if (pts.size() == 4) {
                    segs.push_back({ eBottom, eLeft, false });
                    segs.push_back({ eRight, eTop, false });
                }
            }
        }

        if (!segs.empty()) {
            double diag2 = (xMax - xMin) * (xMax - xMin) + (yMax - yMin) * (yMax - yMin);
            double tol2 = 1e-8 * diag2;
            auto dist2 = [](const QPointF &p, const QPointF &q) {
                double dx = p.x() - q.x(), dy = p.y() - q.y();
                return dx * dx + dy * dy;
            };

            std::vector<QVector<QPointF>> chains;
            for (size_t i = 0; i < segs.size(); ++i) {
                if (segs[i].used)
                    continue;
                segs[i].used = true;
                QVector<QPointF> chain;
                chain.push_back(segs[i].a);
                chain.push_back(segs[i].b);

                bool extended = true;
                while (extended) {
                    extended = false;
                    for (size_t j = 0; j < segs.size(); ++j) {
                        if (segs[j].used)
                            continue;
                        if (dist2(chain.back(), segs[j].a) <= tol2) {
                            chain.push_back(segs[j].b);
                            segs[j].used = true;
                            extended = true;
                        } else if (dist2(chain.back(), segs[j].b) <= tol2) {
                            chain.push_back(segs[j].a);
                            segs[j].used = true;
                            extended = true;
                        } else if (dist2(chain.front(), segs[j].a) <= tol2) {
                            chain.prepend(segs[j].b);
                            segs[j].used = true;
                            extended = true;
                        } else if (dist2(chain.front(), segs[j].b) <= tol2) {
                            chain.prepend(segs[j].a);
                            segs[j].used = true;
                            extended = true;
                        }
                    }
                }
                chains.push_back(chain);
            }

            if (chains.size() > 4) {
                std::vector<QVector<QPointF>> nonDegenerate;
                nonDegenerate.reserve(chains.size());
                for (const auto &ch : chains) {
                    if (ch.size() > 2)
                        nonDegenerate.push_back(ch);
                }
                if (!nonDegenerate.empty()) {
                    chains.swap(nonDegenerate);
                } else {
                    // All chains are degenerate 2-point cell slivers on collapsed/collinear data; keep at most 1
                    chains.resize(1);
                }
            }
            if (chains.size() > 4) {
                auto chainLength = [](const QVector<QPointF> &ch) -> double {
                    double len = 0.0;
                    for (int k = 1; k < ch.size(); ++k)
                        len += std::hypot(ch[k].x() - ch[k - 1].x(), ch[k].y() - ch[k - 1].y());
                    return len;
                };
                std::sort(chains.begin(), chains.end(),
                          [&](const QVector<QPointF> &a, const QVector<QPointF> &b) {
                              if (a.size() != b.size())
                                  return a.size() > b.size();
                              return chainLength(a) > chainLength(b);
                          });
                chains.resize(4);
            }

            for (const auto &ch : chains) {
                if (ch.size() < 2)
                    continue;
                std::vector<double> cx(size_t(ch.size())), cy(size_t(ch.size()));
                for (int k = 0; k < ch.size(); ++k) {
                    cx[size_t(k)] = ch[k].x();
                    cy[size_t(k)] = ch[k].y();
                }
                QString title = d_graph->generateFunctionName(tr("ImplicitODRFit"));
                FunctionCurve *fc = new FunctionCurve(app, FunctionCurve::Normal, title);
                fc->setPen(QPen(d_curveColor, 2));
                fc->setData(cx.data(), cy.data(), ch.size());
                fc->setRange(xMin, xMax);
                fc->setFormula(normExpr + " = 0");
                attachToSourceAxes(fc);
                d_graph->insertPlotItem(fc, Graph::Line);
                d_graph->addFitCurve(fc);
            }
            d_graph->replot();
        }
    }

    if (app && app->writeFitResultsToLog && !d_silent && d_graph)
        app->updateLog(logFitInfo(d_results, d_last_iterations, GSL_SUCCESS, d_graph->parentPlotName()));

    return true;
}

void NonLinearFit::showGlobalFitDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g || isNonInteractiveFit(app))
        return;
    QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty()) {
        QMessageBox::warning(app, tr("Global Curve Fit"),
                             tr("The active graph contains no analysable data curves."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(tr("Global Nonlinear Curve Fit (Shared Parameters)"));
    dlg.resize(540, 480);
    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);

    QGroupBox *gbCurves = new QGroupBox(tr("1. Datasets to Fit Simultaneously"));
    QVBoxLayout *cl = new QVBoxLayout(gbCurves);
    QListWidget *curveList = new QListWidget();
    for (const QString &c : curves) {
        QListWidgetItem *it = new QListWidgetItem(c, curveList);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Checked);
    }
    cl->addWidget(curveList);
    mainLayout->addWidget(gbCurves);

    QGroupBox *gbModel = new QGroupBox(tr("2. Model && Shared Parameter Flags"));
    QFormLayout *fl = new QFormLayout(gbModel);
    QComboBox *modelBox = new QComboBox();
    modelBox->addItem(tr("<Custom Formula>"), QString());
    for (const QString &mName : builtInModelNames()) {
        const QString displayLabel = (mName == QLatin1String("VoigtApprox"))
                ? QStringLiteral("PseudoVoigt1 (Voigt approx.)")
                : mName;
        modelBox->addItem(displayLabel, mName);
    }
    int expIdx = modelBox->findData("ExpDec1");
    if (expIdx >= 0)
        modelBox->setCurrentIndex(expIdx);

    QLineEdit *formulaEdit = new QLineEdit();
    QLineEdit *paramsEdit = new QLineEdit();
    fl->addRow(tr("Model:"), modelBox);
    fl->addRow(tr("Formula y(x):"), formulaEdit);
    fl->addRow(tr("Parameters:"), paramsEdit);

    QTableWidget *paramTable = new QTableWidget(0, 2);
    paramTable->setHorizontalHeaderLabels(QStringList() << tr("Parameter") << tr("Share Across Curves"));
    paramTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    paramTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    fl->addRow(paramTable);
    mainLayout->addWidget(gbModel);

    auto refreshParams = [&]() {
        if (modelBox->currentIndex() > 0) {
            QString m = modelBox->currentData().toString();
            if (m.isEmpty())
                m = modelBox->currentText();
            formulaEdit->setText(builtInModelFormula(m));
            paramsEdit->setText(builtInModelParameters(m).join(", "));
        }
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList pList = paramsEdit->text().split(QRegExp("[,;\\s]+"), Qt::SkipEmptyParts);
#else
        QStringList pList = paramsEdit->text().split(QRegExp("[,;\\s]+"), QString::SkipEmptyParts);
#endif
        paramTable->setRowCount(pList.size());
        for (int i = 0; i < pList.size(); ++i) {
            QTableWidgetItem *it = new QTableWidgetItem(pList[i]);
            it->setFlags(Qt::ItemIsEnabled);
            paramTable->setItem(i, 0, it);
            QCheckBox *cb = new QCheckBox();
            cb->setChecked(i == 1 || pList[i] == "t" || pList[i] == "k" || pList[i] == "w");
            paramTable->setCellWidget(i, 1, cb);
        }
    };
    QObject::connect(modelBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&](int) { refreshParams(); });
    QObject::connect(paramsEdit, &QLineEdit::editingFinished, refreshParams);
    refreshParams();

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    mainLayout->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList chosenCurves;
    for (int i = 0; i < curveList->count(); ++i) {
        if (curveList->item(i)->checkState() == Qt::Checked)
            chosenCurves << curveList->item(i)->text();
    }
    if (chosenCurves.isEmpty())
        return;

    NonLinearFit fitter(app, g);
    if (modelBox->currentIndex() > 0) {
        QString modelKey = modelBox->currentData().toString();
        if (modelKey.isEmpty())
            modelKey = modelBox->currentText();
        fitter.setBuiltInModel(modelKey);
    } else {
        QStringList pNames;
        for (int i = 0; i < paramTable->rowCount(); ++i)
            pNames << paramTable->item(i, 0)->text();
        fitter.setParametersList(pNames);
        fitter.setFormula(formulaEdit->text().trimmed());
    }

    QVector<bool> sharedFlags(paramTable->rowCount(), false);
    for (int i = 0; i < paramTable->rowCount(); ++i) {
        QCheckBox *cb = qobject_cast<QCheckBox *>(paramTable->cellWidget(i, 1));
        sharedFlags[i] = (cb && cb->isChecked());
    }

    Table *summary = nullptr;
    if (!fitter.fitGlobal(chosenCurves, sharedFlags, &summary)) {
        if (!isNonInteractiveFit(app)) {
            QMessageBox::warning(app, tr("Global Curve Fit"),
                                 tr("Global nonlinear curve fit failed."));
        }
    } else if (!fitter.converged() && !isNonInteractiveFit(app)) {
        QMessageBox::warning(app, tr("Global Curve Fit"), fitter.convergenceMessage());
    }
}

void NonLinearFit::showImplicitFitDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g || isNonInteractiveFit(app))
        return;
    QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty()) {
        QMessageBox::warning(app, tr("Implicit / ODR Fit"),
                             tr("The active graph contains no analysable data curves."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(tr("Implicit / Orthogonal Distance Regression (ODR) Fit"));
    dlg.resize(480, 260);
    QFormLayout *fl = new QFormLayout(&dlg);

    QComboBox *curveBox = new QComboBox();
    curveBox->addItems(curves);
    QLineEdit *exprEdit = new QLineEdit("x^2/a^2 + y^2/b^2 - 1");
    QLineEdit *paramsEdit = new QLineEdit("a, b");
    QLineEdit *initEdit = new QLineEdit("2.0, 1.5");

    fl->addRow(tr("Dataset:"), curveBox);
    fl->addRow(tr("Implicit Equation F(x, y) = 0:"), exprEdit);
    fl->addRow(tr("Parameters:"), paramsEdit);
    fl->addRow(tr("Initial Guesses:"), initEdit);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    fl->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList pNames = paramsEdit->text().split(QRegExp("[,;\\s]+"), Qt::SkipEmptyParts);
    QStringList initStrs = initEdit->text().split(QRegExp("[,;\\s]+"), Qt::SkipEmptyParts);
#else
    QStringList pNames = paramsEdit->text().split(QRegExp("[,;\\s]+"), QString::SkipEmptyParts);
    QStringList initStrs = initEdit->text().split(QRegExp("[,;\\s]+"), QString::SkipEmptyParts);
#endif
    QVector<double> initVals;
    for (int i = 0; i < pNames.size(); ++i) {
        double v = (i < initStrs.size()) ? initStrs[i].toDouble() : 1.0;
        initVals.push_back(v);
    }

    NonLinearFit fitter(app, g, curveBox->currentText());
    if (fitter.fitImplicitODR(exprEdit->text(), pNames, initVals)) {
        fitter.parametersTable(app->generateUniqueName(tr("ImplicitODRParams")));
        if (!fitter.converged() && !isNonInteractiveFit(app))
            QMessageBox::warning(app, tr("Implicit / ODR Fit"), fitter.convergenceMessage());
    } else if (!isNonInteractiveFit(app)) {
        QMessageBox::warning(app, tr("Implicit / ODR Fit"),
                             tr("Implicit ODR curve fit failed. Check formula and initial guesses."));
    }
}

void NonLinearFit::showModelComparisonDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g || isNonInteractiveFit(app))
        return;
    QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty()) {
        QMessageBox::warning(app, tr("Model Comparison (AIC / BIC)"),
                             tr("The active graph contains no analysable data curves."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(tr("Compare Candidate Models (AIC / AICc / BIC Ranking)"));
    dlg.resize(460, 420);
    QVBoxLayout *vl = new QVBoxLayout(&dlg);

    QHBoxLayout *hl = new QHBoxLayout();
    hl->addWidget(new QLabel(tr("Dataset:")));
    QComboBox *curveBox = new QComboBox();
    curveBox->addItems(curves);
    hl->addWidget(curveBox, 1);
    vl->addLayout(hl);

    QListWidget *modelList = new QListWidget();
    QStringList defaults = QStringList() << "Linear"
                                         << "Quadratic"
                                         << "Cubic"
                                         << "Poly4"
                                         << "ExpDec1"
                                         << "ExpDec2"
                                         << "Gauss"
                                         << "Lorentz"
                                         << "PseudoVoigt1"
                                         << "Boltzmann"
                                         << "Hill"
                                         << "Allometric1"
                                         << "Logarithm";
    for (int i = 0; i < defaults.size(); ++i) {
        QListWidgetItem *it = new QListWidgetItem(defaults[i], modelList);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(i < 7 ? Qt::Checked : Qt::Unchecked);
    }
    vl->addWidget(new QLabel(tr("Candidate Models to Fit & Rank:")));
    vl->addWidget(modelList, 1);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vl->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList chosen;
    for (int i = 0; i < modelList->count(); ++i) {
        if (modelList->item(i)->checkState() == Qt::Checked)
            chosen << modelList->item(i)->text();
    }
    if (chosen.isEmpty())
        return;

    Fit::compareModels(app, g, curveBox->currentText(), chosen);
}
