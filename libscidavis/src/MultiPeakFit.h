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
        BaselineALS = 3
    };
    struct PeakSummaryRow
    {
        int index;
        double center;
        double fwhm;
        double height;
        double area;
        double areaPercent;
        double baseline;
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

    void setBaselineMode(BaselineMode mode);
    BaselineMode baselineMode() const { return d_baseline_mode; }
    std::vector<double> computeBaseline(const std::vector<double> &x,
                                        const std::vector<double> &y) const;
    std::vector<double> computeBaseline() const;
    bool subtractBaselineFromData();
    PlotCurve *insertBaselineCurve();
    Table *integratePeaksToTable(const QString &tableName = QString());

    void enablePeakCurves(bool on) { generate_peak_curves = on; };
    void setPeakCurvesColor(QColor color) { d_peaks_color = color; };

    static QString generateFormula(int order, PeakProfile profile);
    static QStringList generateParameterList(int order);
    static QStringList generateExplanationList(int order);

    bool autoDetectPeaks(int maxPeaks = 0, double minProminenceRatio = 0.08);
    int estimatePeakCount(double minProminenceRatio = 0.08) const;
    int autoDetectAndUpdateNumPeaks(int maxPeaks = 10, double minProminenceRatio = 0.08);
    bool seedPeaksFromCenters(const std::vector<double> &pickedCenters,
                              double minProminenceRatio = 0.08);

    Table *createPeakSummaryTable(const QString &tableName = QString());
    std::vector<PeakSummaryRow> computePeakSummary(const std::vector<double> &res) const;
    std::vector<PeakSummaryRow> initialPeakSummary() const;

    void guessInitialValues() override;

private:
    void updateProfileFunctions();
    double evalPeakAt(const std::vector<double> &par, int peakIndex, double x,
                      bool isStoredResults = false) const;
    bool calculateFitCurveData(const std::vector<double> &, std::vector<double> &,
                               std::vector<double> &) override;
    QString logFitInfo(const std::vector<double> &, int, int, const QString &) override;
    void generateFitCurve(const std::vector<double> &) override;
    static QString peakFormula(int peakIndex, PeakProfile profile);
    //! Inserts a peak function curve into the plot
    void insertPeakFunctionCurve(std::vector<double> &x, std::vector<double> &y, int peak);
    void storeCustomFitResults(const std::vector<double> &) override;

private:
    //! Number of peaks
    int d_peaks;

    //! Tells weather the peak curves should be displayed together with the best line fit.
    bool generate_peak_curves;

    //! Color index for the peak curves
    QColor d_peaks_color;

    //! The peak profile
    PeakProfile d_profile;

    //! Baseline mode for Peak Analyzer
    BaselineMode d_baseline_mode = BaselineConstant;

    //! Tracks whether subtractBaselineFromData() was applied
    bool d_baseline_subtracted = false;

    //! Stores last computed baseline vector
    std::vector<double> d_last_baseline;

    //! Stores last fitted results for summary table generation
    std::vector<double> d_last_results;
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
