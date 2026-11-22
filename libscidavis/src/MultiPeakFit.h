/***************************************************************************
    File                 : MultiPeakFit.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : MultiPeakFit module with Lorentz and Gauss peak shapes

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
#ifndef MULTIPEAKFIT_H
#define MULTIPEAKFIT_H

#include <QColor>
#include <QPointF>
#include <QStringList>
#include <QVector>
#include "Fit.h"

class MultiPeakFit : public Fit
{
    Q_OBJECT

public:
    enum PeakProfile {
        Gauss = 0,
        Lorentz = 1,
        GaussAmp = 2,
        PseudoVoigt = 3,
        PearsonVII = 4,
        Voigt = 5,
        BiGaussian = 6,
        EMG = 7,
        LogNormal = 8,
        Asym2Sig = 9,
        DoniachSunjic = 10
    };
    enum BaselineMode {
        BaselineConstant = 0,
        BaselineZero = 1,
        BaselineLinear = 2,
        BaselineALS = 3,
        BaselineAnchors = 4
    };
    struct PeakSummaryRow
    {
        int index = 0;
        double center = 0.0;
        double fwhm = 0.0;
        double height = 0.0;
        double area = 0.0;
        double areaPercent = 0.0;
        double baseline = 0.0;
        double centerSE = 0.0;
        double fwhmSE = 0.0;
        double heightSE = 0.0;
        double areaSE = 0.0;
    };

    MultiPeakFit(ApplicationWindow *parent, Graph *g = 0, PeakProfile profile = Gauss,
                 int peaks = 1);

    int peaks() const { return d_peaks; };
    void setNumPeaks(int n);

    PeakProfile profile() const { return d_profile; }
    void setPeakProfile(PeakProfile profile);

    void setLorentzFraction(double mu);
    double lorentzFraction() const { return d_voigt_mu; }

    void setPearsonShape(double m);
    double pearsonShape() const { return d_pearson_m; }

    void setAsymmetryFactor(double alpha);
    double asymmetryFactor() const { return d_asym_alpha; }

    void setTailFactor(double tau);
    double tailFactor() const { return d_tail_tau; }

    void setDataCurve(int curve, double start, double end) override;

    void setBaselineMode(BaselineMode mode);
    BaselineMode baselineMode() const { return d_baseline_mode; }
    void setBaselineAnchors(const QVector<QPointF> &anchors, bool cubicSpline = true);
    QVector<QPointF> baselineAnchors() const { return d_baseline_anchors; }
    std::vector<double> computeBaseline(const std::vector<double> &x,
                                        const std::vector<double> &y) const;
    std::vector<double> computeBaseline() const;
    bool subtractBaselineFromData();
    PlotCurve *insertBaselineCurve();
    Table *integratePeaksToTable(const QString &tableName = QString());

    void enablePeakCurves(bool on) { generate_peak_curves = on; };
    void setPeakCurvesColor(QColor color)
    {
        d_peaks_color = color;
        d_custom_peaks_color = true;
    };

    static QString generateFormula(int order, PeakProfile profile);
    static QStringList generateParameterList(int order);
    static QStringList generateExplanationList(int order);
    static QString generateFormula(int peaks, PeakProfile profile, double mu, double pearsonM,
                                   double asymAlpha, double tailTau);
    static QStringList generateExplanationList(int peaks, PeakProfile profile,
                                               const QVector<PeakProfile> &perPeak = {});
    //! true when the A parameter of the profile is the integrated area (Gauss, Lorentz, ...)
    static bool profileUsesAreaParameter(PeakProfile profile);

    bool autoDetectPeaks(int maxPeaks = 0, double minProminenceRatio = 0.08);
    int estimatePeakCount(double minProminenceRatio = 0.08) const;
    int autoDetectAndUpdateNumPeaks(int maxPeaks = 10, double minProminenceRatio = 0.08);
    bool seedPeaksFromCenters(const std::vector<double> &pickedCenters,
                              double minProminenceRatio = 0.08);
    QVector<double> detectPeaksSecondDerivative(int sgWindow = 9, double relThreshold = 0.08);

    void setInitialPeakWidths(const QVector<double> &fwhms);
    QVector<double> initialPeakWidths() const { return d_initial_peak_widths; }
    void setNegativePeaksAllowed(bool allow);
    bool negativePeaksAllowed() const { return d_negative_peaks_allowed; }

    void setPerPeakProfiles(const QVector<PeakProfile> &profiles);
    void addPeakDoubletConstraint(int secondaryPeakIdx, int primaryPeakIdx,
                                  double areaRatio, double centerOffset, bool shareWidth = true);
    static Table *batchAnalyzeCurves(ApplicationWindow *app, Graph *g, const QStringList &curveTitles,
                                     PeakProfile profile = Gauss, BaselineMode baseMode = BaselineLinear,
                                     bool allowNegativePeaks = false);
    static void showBatchPeakDialog(ApplicationWindow *app, Graph *g);

    Table *createPeakSummaryTable(const QString &tableName = QString());
    std::vector<PeakSummaryRow> computePeakSummary(const std::vector<double> &res) const;
    std::vector<PeakSummaryRow> initialPeakSummary() const;

    void guessInitialValues() override;

protected:
    bool evaluateModelCurve(const std::vector<double> &solverPar, std::vector<double> &X,
                            std::vector<double> &Y) override;
    double toInternalParameter(int index, double userVal) const override;
    double fromInternalParameter(int index, double internalVal) const override;
    double covarSolverToStoredScale(int parIndex, double solverVal) const override;

private:
    struct DoubletConstraint
    {
        int secIdx = 0;
        int priIdx = 0;
        double areaRatio = 1.0;
        double centerOffset = 0.0;
        bool shareWidth = true;
    };

    PeakProfile peakProfileAt(int peakIndex) const;
    bool usesCustomMultiPeakCallbacks() const;
    void applyDoubletConstraints(std::vector<double> &par) const;
    void applyCustomInitialWidths();
    void evalSinglePeakMetrics(PeakProfile prof, double signedRawA, double w, double &fwhmOut,
                               double &heightOut, double &areaOut) const;
    double estimateConstantBaselineY0(const std::vector<double> &y) const;
    double peakTrueArea(PeakProfile prof, double rawA, double w) const;
    double rawParamFromTrueArea(PeakProfile prof, double targetArea, double w) const;
    double peakFWHMFromW(PeakProfile prof, double w) const;
    double wFromPeakFWHM(PeakProfile prof, double fwhm) const;
    QString buildFormula() const;
    static QString profileDisplayName(PeakProfile profile);

    static int custom_multi_peak_f(const gsl_vector *x, void *params, gsl_vector *f);
    static int custom_multi_peak_df(const gsl_vector *x, void *params, gsl_matrix *J);
    static int custom_multi_peak_fdf(const gsl_vector *x, void *params, gsl_vector *f,
                                     gsl_matrix *J);
    static double custom_multi_peak_d(const gsl_vector *x, void *params);

    void updateProfileFunctions();
    double evalPeakAt(const std::vector<double> &par, int peakIndex, double x,
                      bool isStoredResults = false) const;
    bool calculateFitCurveData(const std::vector<double> &, std::vector<double> &,
                               std::vector<double> &) override;
    QString logFitInfo(const std::vector<double> &, int, int, const QString &) override;
    void generateFitCurve(const std::vector<double> &) override;
    static QString peakFormula(int peakIndex, PeakProfile profile);
    static QString peakFormula(int peakIndex, PeakProfile profile, double mu, double pearsonM,
                               double asymAlpha, double tailTau);
    QString instancePeakFormula(int peakIndex, PeakProfile profile) const;
    void resolveDataUnits(QString &xUnit, QString &yUnit) const;
    static QString areaUnit(const QString &xUnit, const QString &yUnit);
    //! Inserts a peak function curve into the plot
    void insertPeakFunctionCurve(std::vector<double> &x, std::vector<double> &y, int peak);
    std::vector<double> evaluateSubtractedBaselineOnGrid(const std::vector<double> &X) const;
    QColor resolvedPeakComponentColor(int peakIndex) const;
    void storeCustomFitResults(const std::vector<double> &) override;

private:
    //! Number of peaks
    int d_peaks;

    //! Tells weather the peak curves should be displayed together with the best line fit.
    bool generate_peak_curves;

    //! Color index for the peak curves
    QColor d_peaks_color;
    bool d_custom_peaks_color = false;

    //! The peak profile
    PeakProfile d_profile;

    //! Per-peak profile overrides when mixed peak shapes are configured
    QVector<PeakProfile> d_per_peak_profiles;

    //! Coupled doublet constraints across peak pairs
    QVector<DoubletConstraint> d_doublet_constraints;

    //! Baseline mode for Peak Analyzer
    BaselineMode d_baseline_mode = BaselineConstant;

    //! User-defined anchor points for BaselineAnchors mode
    QVector<QPointF> d_baseline_anchors;
    bool d_anchors_cubic_spline = true;

    //! Cached peak centers from last detectPeaksSecondDerivative() call
    std::vector<double> d_detected_2nd_deriv_centers;

    //! Tracks whether subtractBaselineFromData() was applied
    bool d_baseline_subtracted = false;

    //! Stores last computed baseline vector
    std::vector<double> d_last_baseline;

    //! Stores last fitted results for summary table generation
    std::vector<double> d_last_results;

    //! Custom per-peak initial FWHM overrides (<= 0 leaves auto-estimated width)
    QVector<double> d_initial_peak_widths;

    //! Whether inverted / negative trough peaks (A < 0) are permitted
    bool d_negative_peaks_allowed = false;
};

class LorentzFit : public MultiPeakFit
{
    Q_OBJECT

public:
    LorentzFit(ApplicationWindow *parent, Graph *g);
    LorentzFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle);
    LorentzFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
               double end);

private:
    void init();
};

class GaussFit : public MultiPeakFit
{
    Q_OBJECT

public:
    GaussFit(ApplicationWindow *parent, Graph *g);
    GaussFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle);
    GaussFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
             double end);

private:
    void init();
};

class GaussAmpFit : public Fit
{
    Q_OBJECT

public:
    GaussAmpFit(ApplicationWindow *parent, Graph *g);
    GaussAmpFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle);
    GaussAmpFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
                double end);
    void guessInitialValues() override;

private:
    void init();
    bool calculateFitCurveData(const std::vector<double> &, std::vector<double> &,
                               std::vector<double> &) override;
};
#endif
