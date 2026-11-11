/***************************************************************************
    File                 : Fit.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Fit base class

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
#ifndef FIT_H
#define FIT_H

#include <QObject>
#include <QMap>
#include <QVector>

#include "Filter.h"
#include "Script.h"

#include <gsl/gsl_multifit_nlin.h>
#include <gsl/gsl_multimin.h>

#include <vector>

class Table;
class Matrix;
class MultiLayer;
class ApplicationWindow;
class Script;

//! Fit base class
class Fit : public Filter, public scripted
{
    Q_OBJECT

public:
    typedef double (*fit_function_simplex)(const gsl_vector *, void *);
    typedef int (*fit_function)(const gsl_vector *, void *, gsl_vector *);
    typedef int (*fit_function_df)(const gsl_vector *, void *, gsl_matrix *);
    typedef int (*fit_function_fdf)(const gsl_vector *, void *, gsl_vector *, gsl_matrix *);

    enum Algorithm { ScaledLevenbergMarquardt, UnscaledLevenbergMarquardt, NelderMeadSimplex };
    enum ErrorSource { UnknownErrors, AssociatedErrors, PoissonErrors, CustomErrors };
    enum WeightingMethod {
        WeightNone = 0,
        WeightInstrumental = 1,
        WeightDirect = 2,
        WeightStatistical = 3,
        WeightProportional = 4,
        WeightVariance = 5
    };

    struct BootstrapResult
    {
        bool valid = false;
        int nSuccess = 0;
        QVector<double> meanParams;
        QVector<double> sdParams;
        QVector<double> ciLow;
        QVector<double> ciHigh;
        Table *reportTable = nullptr;
    };

    Fit(ApplicationWindow *parent, Graph *g = 0, QString name = QString());
    virtual ~Fit();

    //! Actually does the fit. Should be reimplemented in derived classes.
    virtual void fit();

    //! Sets the data set to be used as source of Y errors.
    bool setYErrorSource(ErrorSource err, const QString &colName = {}, bool fail_silently = false);
    bool setWeightingMethod(WeightingMethod m, const QString &colName = QString());
    WeightingMethod weightingMethod() const { return d_weighting_method; }

    bool setLinearConstraintExpr(const QString &exprList);
    QString linearConstraintExpr() const { return d_linear_constraint_expr; }

    BootstrapResult runBootstrapCI(int nSamples = 300, double confLevel = 0.95,
                                   int mode /*0=Residual,1=Wild*/ = 0);
    bool evaluatePreviewCurve(std::vector<double> &X, std::vector<double> &Y, int nPoints = 200);

    void setDataCurve(int curve, double start, double end);

    QString formula() { return d_formula; };
    int numParameters() const { return d_p; }

    void setInitialGuess(int parIndex, double val)
    {
        if (parIndex >= 0) {
            d_explicit_init_vals[parIndex] = val;
            if (d_param_init && unsigned(parIndex) < d_p)
                gsl_vector_set(d_param_init, parIndex, toInternalParameter(parIndex, val));
        }
        d_initial_guesses_set = true;
    };
    void setInitialGuesses(double *x_init);
    double initialGuess(int parIndex) const
    {
        if (d_explicit_init_vals.contains(parIndex))
            return d_explicit_init_vals.value(parIndex);
        if (d_param_init && parIndex >= 0 && unsigned(parIndex) < d_p)
            return fromInternalParameter(parIndex, gsl_vector_get(d_param_init, parIndex));
        return 0.0;
    }

    void setParameterFixed(int index, bool fixed = true);
    bool isParameterFixed(int index) const;
    void setParameterBounds(int index, double minVal, double maxVal);
    void clearParameterBounds(int index);
    bool hasParameterBounds(int index, double *minOut = nullptr, double *maxOut = nullptr) const;

    void enableConfidenceBand(bool on = true, double confLevel = 0.95);
    void enablePredictionBand(bool on = true, double confLevel = 0.95);
    bool confidenceBandEnabled() const { return d_conf_band; }
    bool predictionBandEnabled() const { return d_pred_band; }

    double aic() const;
    double aicc() const;
    double bic() const;
    double anovaFValue() const;
    double anovaPValue() const;

    void insertFitSummaryBoxOnGraph();
    MultiLayer *createResidualPlotWindow();
    static Table *compareModels(ApplicationWindow *app, Graph *g, const QString &curveTitle,
                                const QStringList &modelNames);

    virtual void guessInitialValues() {};

    void setAlgorithm(Algorithm s) { d_solver = s; };

    //! Specifies weather the result of the fit is a function curve
    void generateFunction(bool yes, int points = 100);

    //! Output string added to the plot as a new legend
    virtual QString legendInfo();

    //! Returns a vector with the fit results
    const std::vector<double> &results() const { return d_results; };

    //! Returns a vector with the standard deviations of the results
    const std::vector<double> &errors();

    //! Returns the sum of squares of the residuals from the best-fit line
    double chiSquare() const { return chi_2; };

    //! Returns the reduced chi-square (Chi^2 / doF)
    double reducedChiSquare() const;

    //! Returns the unweighted residual sum of squares (RSS)
    double rss() const;

    //! Returns the coefficient of determination, R^2 (COD)
    double rSquare() const;

    //! Returns the adjusted R^2: 1 - (1 - R^2)*(n - 1)/(n - p)
    double adjustedRSquare() const;

    //! Returns Pearson's correlation coefficient r
    double pearsonR() const;

    //! Specifies wheather the errors must be scaled with sqrt(chi_2/dof)
    void scaleErrors(bool yes = true) { d_scale_errors = yes; };

    Table *parametersTable(const QString &tableName);
    Matrix *covarianceMatrix(const QString &matrixName);
    Table *createResidualsTable(const QString &tableName = QString());

    double voigtLorentzFraction() const { return d_voigt_mu; }
    double pearsonShapeParameter() const { return d_pearson_m; }
    double peakAsymmetryParameter() const { return d_asym_alpha; }
    double peakTailParameter() const { return d_tail_tau; }

    int evaluate_f(const gsl_vector *x, gsl_vector *f);
    double evaluate_d(const gsl_vector *x);
    int evaluate_df(const gsl_vector *x, gsl_matrix *J);
    static double evaluate_df_helper(double x, void *param);

protected slots:
    void scriptError(const QString &message, const QString &script_name, int line_number);

protected:
    //! Execute the fit using GSL multidimensional minimization (Nelder-Mead Simplex).
    std::vector<double> fitGslMultimin(int &iterations, int &status);

    //! Execute the fit using GSL non-linear least-squares fitting (Levenberg-Marquardt).
    std::vector<double> fitGslMultifit(int &iterations, int &status);

    //! Customs and stores the fit results according to the derived class specifications. Used by exponential fits.
    virtual void storeCustomFitResults(const std::vector<double> &par) { d_results = par; }

    //! Generates argument values wrt to d_gen_function
    void generateX(std::vector<double> &X) const;

    //! Adds the result curve as a FunctionCurve to the plot, if d_gen_function = true
    void insertFitFunctionCurve(const QString &name, std::vector<double> &, std::vector<double> &,
                                int penWidth = 1);

    //! Adds the result curve to the plot
    virtual void generateFitCurve(const std::vector<double> &);

    //! Calculates the data for the output fit curve and store itin the X an Y vectors
    virtual bool calculateFitCurveData(const std::vector<double> &, std::vector<double> &,
                                       std::vector<double> &)
    {
        return false;
    }

    //! Evaluates model curve directly from internal solver parameters (matches covar coordinates)
    virtual bool evaluateModelCurve(const std::vector<double> &solverPar, std::vector<double> &X,
                                    std::vector<double> &Y)
    {
        return calculateFitCurveData(solverPar, X, Y);
    }

    virtual double toInternalParameter(int index, double userVal) const
    {
        Q_UNUSED(index)
        return userVal;
    }

    virtual double fromInternalParameter(int index, double internalVal) const
    {
        Q_UNUSED(index)
        return internalVal;
    }

    int effectiveFreeParameters() const;
    bool hasActiveConstraints() const;
    void ensureConstraintCapacity(int index);
    void applyExplicitInitialGuesses();
    void addConfidenceAndPredictionBands(const std::vector<double> &solverPar);

    //! Output string added to the result log
    virtual QString logFitInfo(const std::vector<double> &par, int iterations, int status,
                               const QString &plotName);

    fit_function d_f = nullptr;
    fit_function_df d_df = nullptr;
    fit_function_fdf d_fdf = nullptr;
    fit_function_simplex d_fsimplex = nullptr;

    //! Number of fit parameters
    unsigned d_p;

    //! Initial guesses for the fit parameters
    gsl_vector *d_param_init = nullptr;

    /*! \brief Tells whether the fitter uses non-linear/simplex fitting
     * with an initial parameters set, that must be freed in the destructor.
     */
    bool is_non_linear = true;

    //! Standard deviations of Y input data.
    std::vector<double> d_y_errors;

    //! Names of the fit parameters
    QStringList d_param_names;

    //! Stores a list of short explanations for the significance of the fit parameters
    QStringList d_param_explain;

    //! Specifies weather the result curve is a FunctionCurve or a normal curve with the same x values as the fit data
    bool d_gen_function;

    //! Algorithm type
    Algorithm d_solver;

    //! The fit formula
    QString d_formula;

    //! Covariance matrix
    gsl_matrix *covar = nullptr;

    //! Where standard errors of the input data are taken from.
    ErrorSource d_y_error_source;

    //! The name of the dataset containing Y standard errors (if applicable).
    QString d_y_error_dataset;

    //! Stores the result parameters
    std::vector<double> d_results;

    //! Stores standard deviations of the result parameters
    std::vector<double> d_result_errors;

    //! The sum of squares of the residuals from the best-fit line
    double chi_2;

    //! Specifies wheather the errors must be scaled with sqrt(chi_2/dof)
    bool d_scale_errors;

    //! Tracks whether initial parameter guesses have been explicitly set
    bool d_initial_guesses_set = false;
    bool d_all_guesses_set = false;
    QMap<int, double> d_explicit_init_vals;

    //! Parameter fixing and box-constraint storage
    std::vector<bool> d_param_fixed;
    std::vector<bool> d_param_bounded;
    std::vector<double> d_param_min;
    std::vector<double> d_param_max;

    //! OriginPro weighting method and linear equality constraint expression
    WeightingMethod d_weighting_method = WeightNone;
    QString d_linear_constraint_expr;

    //! Confidence and prediction band flags and confidence levels
    bool d_conf_band = false;
    bool d_pred_band = false;
    double d_conf_level = 0.95;
    double d_pred_level = 0.95;

    //! Optional custom R^2 override (e.g. apparent log-space fit or implicit ODR)
    bool d_has_custom_r2 = false;
    double d_custom_r2 = 0.0;

    //! Suppresses plotting curves onto d_graph during model comparison
    bool d_suppress_curve_output = false;

    //! Fitted Y values corresponding to d_x
    std::vector<double> d_y_fit;

    //! Lorentz fraction mu in [0, 1] for Pseudo-Voigt / Voigt profile (default 0.5)
    double d_voigt_mu = 0.5;

    //! Shape parameter m for Pearson VII profile (default 2.0)
    double d_pearson_m = 2.0;

    //! Asymmetry parameter alpha for BiGaussian / LogNormal / Asym2Sig / DoniachSunjic
    double d_asym_alpha = 1.5;

    //! Exponential tail parameter tau for EMG profile (default 1.2)
    double d_tail_tau = 1.2;

    //! Script used to evaluate user-defined functions.
    std::unique_ptr<Script> d_script;
};

#endif
