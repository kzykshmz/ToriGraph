/***************************************************************************
    File                 : CurveMathTools.h
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Description          : OriginPro Curve Math, Multi-Curve Averaging,
                           Curve Normalization, Intersections, X/Y Lookup,
                           Polygon Loop Area & Arc Length
 ***************************************************************************/

#ifndef CURVEMATHTOOLS_H
#define CURVEMATHTOOLS_H

#include <QColor>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <limits>
#include <vector>

class ApplicationWindow;
class Graph;
class Legend;
class Matrix;
class MultiLayer;
class QwtPlotCurve;
class Table;

class CurveMathTools
{
public:
    enum MathOp { Add = 0, Subtract = 1, Multiply = 2, Divide = 3 };
    enum NormMode {
        DivideByMax = 0,
        Range0To1 = 1,
        RangeMinus1To1 = 2,
        ZScore = 3,
        UnitArea = 4,
        SumTo100 = 5,
        DivideByRefX = 6
    };

    static QwtPlotCurve *simpleCurveMath(ApplicationWindow *app, Graph *g,
                                         const QString &curveA, const QString &curveB,
                                         MathOp op, bool useUnionGrid = false);
    static QwtPlotCurve *subtractStraightLine(ApplicationWindow *app, Graph *g,
                                              const QString &curveTitle,
                                              QPointF p1, QPointF p2, bool inPlace = false);
    static QwtPlotCurve *averageMultipleCurves(ApplicationWindow *app, Graph *g,
                                               const QStringList &curveTitles,
                                               int numPoints = 200,
                                               bool addSdBand = true,
                                               Table **outTable = nullptr);
    static QwtPlotCurve *normalizeCurve(ApplicationWindow *app, Graph *g,
                                        const QString &curveTitle, NormMode mode,
                                        double refX = 0.0);
    //! Shared normalisation kernel (menu dialog and recalculation engine). xs must be sorted
    //! ascending for UnitArea. refValue is used only by DivideByRefX.
    static std::vector<double> normalizeValues(const std::vector<double> &xs,
                                               const std::vector<double> &ys, NormMode mode,
                                               double refValue = 0.0);
    static QVector<QPointF> findCurveIntersections(Graph *g, const QString &curveA,
                                                   const QString &curveB,
                                                   bool markOnGraph = true,
                                                   ApplicationWindow *app = nullptr,
                                                   Table **outTable = nullptr);
    static QVector<double> findXFromY(Graph *g, const QString &curveTitle, double targetY);
    static double findYFromX(Graph *g, const QString &curveTitle, double targetX, bool *ok = nullptr);
    //! Changes of direction of X in row order (0: monotonic). *nPoints gets the point count.
    static int countXTurns(Graph *g, const QString &curveTitle, int *nPoints = nullptr);
    //! Y at targetX on every branch of a curve whose X goes out and back, in row order
    //! (linear interpolation within each row segment, like findXFromY).
    static QVector<double> findYValuesFromX(Graph *g, const QString &curveTitle, double targetX);
    static double polygonArea(Graph *g, const QString &curveTitle, bool mathematicalSigned = false);
    static double curveArcLength(Graph *g, const QString &curveTitle);

    struct ArrheniusResult {
        bool valid = false;
        QString error;          // reason when valid == false
        int excludedCount = 0;  // rows dropped (non-numeric, T <= 0 K, rate <= 0)
        QStringList notes;      // exclusion summary (also written to the log)
        double activationEnergyKJmol = 0.0;
        double activationEnergyKJmolSE = 0.0;
        double activationEnergyEV = 0.0;
        double activationEnergyEVSE = 0.0;
        double lnPreExponential = 0.0;
        double rSquare = 0.0;
        Table *dataTable = nullptr;
        MultiLayer *plot = nullptr;
    };
    struct TaucResult {
        bool valid = false;
        double bandgapEV = 0.0;
        double bandgapEVSE = 0.0;
        double edgeSlope = 0.0;
        double rSquare = 0.0;
        Table *dataTable = nullptr;
        MultiLayer *plot = nullptr;
    };
    struct EISResult {
        bool valid = false;
        double rs = 0.0, rct = 0.0, qCpe = 0.0, nCpe = 1.0, cEff = 0.0, fPeakHz = 0.0, rmse = 0.0;
        Table *resultTable = nullptr;
        MultiLayer *nyquistPlot = nullptr;
        MultiLayer *bodePlot = nullptr;
    };
    struct TensileResult {
        bool valid = false;
        QString error;
        double youngsModulus = 0.0;
        double elasticSlopeRaw = 0.0;
        double elasticIntercept = 0.0;
        double elasticR2 = 0.0;
        double elasticXMin = 0.0;
        double elasticXMax = 0.0;
        double offsetFraction = 0.002;
        double offsetYieldX = 0.0;
        double offsetYieldY = 0.0;
        double utsX = 0.0;
        double utsY = 0.0;
        double fractureX = 0.0;
        double fractureY = 0.0;
        bool strainInPercent = true;
        // How the strain unit was decided: "user", "column", "axis title",
        // "E range" (5-1000 GPa plausibility) or "max strain" (fallback heuristic).
        QString strainUnitSource;
        // true when the unit could not be decided reliably (neither or both
        // interpretations give E in 5-1000 GPa, or the label contradicts E).
        bool strainUnitAmbiguous = false;
        double youngsModulusIfPercentGPa = std::numeric_limits<double>::quiet_NaN();
        double youngsModulusIfFractionGPa = std::numeric_limits<double>::quiet_NaN();
        // Toe-compensated strain origin x0 (x-axis units). All elongations and
        // true strains are measured from x0.
        double toeStrainX0 = 0.0;
        // Elongations in percent, measured from x0. "Plastic" = total - sigma/E.
        double uniformElongationPct = std::numeric_limits<double>::quiet_NaN();
        double uniformPlasticElongationPct = std::numeric_limits<double>::quiet_NaN();
        double fractureElongationPct = std::numeric_limits<double>::quiet_NaN();
        double fracturePlasticElongationPct = std::numeric_limits<double>::quiet_NaN();
        bool elasticAutoRelaxed = false; // auto window found only with relaxed criteria
        int excludedMaskedPoints = 0;
        int excludedNonFinitePoints = 0;
        QStringList warnings;
        QString stressUnit;
        QString constructionCurveTitle;
        Table *trueStressTable = nullptr;
        MultiLayer *plot = nullptr;
    };

    static ArrheniusResult createArrheniusPlot(ApplicationWindow *app, Table *srcTable,
                                               const QString &tempCol, const QString &rateCol,
                                               bool tempInCelsius = false,
                                               bool use1000OverT = true,
                                               bool sigmaTimesT = false);
    static TaucResult createTaucPlot(ApplicationWindow *app, Table *srcTable,
                                     const QString &waveOrEnergyCol, const QString &alphaCol,
                                     bool xIsWavelengthNm = true,
                                     bool directAllowed = true /* true: (a*hv)^2, false: (a*hv)^0.5 */);
    static QwtPlotCurve *fillBetweenCurves(ApplicationWindow *app, Graph *g,
                                           const QString &curveA, const QString &curveB,
                                           const QColor &fillColor = QColor(30, 102, 184, 55));
    static EISResult analyzeEIS(ApplicationWindow *app, Table *srcTable,
                                const QString &freqCol, const QString &zRealCol,
                                const QString &zImagCol, bool zImagAlreadyNegated = false,
                                bool fitRandlesCPE = true);
    static TensileResult analyzeTensile(ApplicationWindow *app, Graph *g,
                                        const QString &curveTitle = QString(),
                                        double offsetFraction = 0.002,
                                        int strainInPercentMode = -1 /* -1=auto, 0=fraction, 1=percent */,
                                        double elasticXMin = std::numeric_limits<double>::quiet_NaN(),
                                        double elasticXMax = std::numeric_limits<double>::quiet_NaN(),
                                        bool plotConstruction = true,
                                        bool addCallout = true,
                                        bool createTrueStressTable = false);

    static void showCurveMathDialog(ApplicationWindow *app, Graph *g);
    static void showSubtractLineDialog(ApplicationWindow *app, Graph *g);
    static void showAverageCurvesDialog(ApplicationWindow *app, Graph *g);
    static void showNormalizeDialog(ApplicationWindow *app, Graph *g);
    static void showIntersectionLookupDialog(ApplicationWindow *app, Graph *g);
    static void showArrheniusDialog(ApplicationWindow *app);
    static void showTaucDialog(ApplicationWindow *app);
    static void showFillBetweenDialog(ApplicationWindow *app, Graph *g);
    static void showEISAnalyzerDialog(ApplicationWindow *app);
    static void showTensileYieldDialog(ApplicationWindow *app);

    struct SampleDatasetInfo {
        QString id;
        QString title;
        QString category;
        QString description;
    };
    static QVector<SampleDatasetInfo> listSampleDatasets();
    static MultiLayer *loadSampleDataset(ApplicationWindow *app, const QString &datasetId,
                                         bool autoPlot = true, Table **outTable = nullptr,
                                         Matrix **outMatrix = nullptr);
    static void showSampleGalleryDialog(ApplicationWindow *app);

    struct KissingerResult {
        bool valid = false;
        QString error;
        double activationEnergyKJmol = 0.0;
        double activationEnergyKJmolSE = 0.0;
        double activationEnergyEV = 0.0;
        double activationEnergyEVSE = 0.0;
        double preExponentialSInv = 0.0;
        double slope = 0.0;
        double intercept = 0.0;
        double rSquare = 0.0;
        QVector<double> heatingRatesKmin;
        QVector<double> peakTempsK;
        int excludedCount = 0; // pairs dropped (non-finite, rate <= 0, Tp <= 0 K)
        QStringList notes;     // exclusion summary (also written to the log)
        Table *resultTable = nullptr;
        MultiLayer *plot = nullptr;
    };
    //! Peak centre picked from a peak-fit result table (PeakSummary / PeakIntegration /
    //! BatchPeakSummary) for the Kissinger analysis.
    struct PeakFitCenter {
        QString source; // table name, or curve name for batch tables
        double center = 0.0;
        bool found = true; // false when the source has no peak N (center is then undefined)
    };
    //! True when the table looks like a peak-fit result table (has "Peak" and "Center" columns).
    static bool isPeakFitResultTable(Table *t);
    //! Returns, for every source (one per table, or one per curve of a batch table), the centre
    //! of the peakNumber-th peak counted from the lowest centre (1-based). Sources lacking that
    //! peak are listed in *missingSources.
    static QVector<PeakFitCenter> collectPeakFitCenters(const QList<Table *> &tables,
                                                        int peakNumber,
                                                        QStringList *missingSources = nullptr);
    //! Like collectPeakFitCenters, but keeps every source in order (one per table, or one per
    //! curve of a batch table; a table without any peak row counts as one source). Sources
    //! lacking peak N have found = false. This is the list the heating rates are paired with.
    static QVector<PeakFitCenter> collectPeakFitSources(const QList<Table *> &tables,
                                                        int peakNumber);
    //! Pairs one heating rate per source (same order as collectPeakFitSources). Returns false,
    //! with *error set, when the counts differ. Sources without peak N are skipped together with
    //! their own rate and listed in *skipped, so the remaining rates never shift.
    static bool pairKissingerFitRates(const QVector<double> &rates,
                                      const QVector<PeakFitCenter> &sources,
                                      QVector<double> *outRates, QVector<double> *outTemps,
                                      QString *error = nullptr, QStringList *skipped = nullptr);
    static KissingerResult analyzeKissinger(ApplicationWindow *app,
                                            const QVector<double> &heatingRates,
                                            const QVector<double> &peakTemps,
                                            bool rateInKPerHour = false,
                                            bool tempInCelsius = true,
                                            bool topAxisInCelsius = true);
    static void showKissingerDialog(ApplicationWindow *app);

    struct WorkHardeningResult {
        bool valid = false;
        QString error;
        double considereTrueStrain = 0.0;
        double considereTrueStress = 0.0;
        double considereEngStrainPct = 0.0;
        double hollomonK = 0.0;
        double hollomonN = 0.0;
        // NaN when the Stage-III fit fails; the reason is in voceNote.
        double voceSigmaS = std::numeric_limits<double>::quiet_NaN();
        QString voceNote;
        // true when no persistent theta = sigma_t crossing was found and the
        // maximum-load (UTS) point was used instead; reason in considereNote.
        bool considereFromUts = false;
        QString considereNote;
        int postNeckRowsMasked = 0;   // rows after the Considere point, masked in the table
        int mergedDuplicatePoints = 0; // equal-strain points averaged after sorting
        bool strainInPercent = true;  // unit used (inherited from the tensile analysis)
        double youngsModulus = 0.0;
        double toeStrainX0 = 0.0;
        QStringList warnings;
        Table *resultTable = nullptr;
        MultiLayer *plot = nullptr;
    };
    static WorkHardeningResult analyzeWorkHardening(ApplicationWindow *app, Graph *g,
                                                    const QString &curveTitle = QString(),
                                                    int smoothWindow = 11,
                                                    bool createPlot = true,
                                                    bool kocksMeckingPlot = false,
                                                    int strainInPercentMode = -1,
                                                    double offsetFraction = 0.002,
                                                    double elasticXMin = std::numeric_limits<double>::quiet_NaN(),
                                                    double elasticXMax = std::numeric_limits<double>::quiet_NaN());
    static void showWorkHardeningDialog(ApplicationWindow *app);

    //! Place a callout / summary Legend box inside the canvas while avoiding plotted data curves.
    static void placeCalloutAutoAvoid(Graph *g, Legend *box, int marginPx = 14,
                                      bool allowScaleExpand = true,
                                      bool preferUpperRight = false);

    struct XRDPeakEntry {
        double twoThetaDeg = 0.0;
        double dSpacingAngstrom = 0.0;
        double fwhmDeg = 0.0;
        double intensity = 0.0;
        double scherrerSizeNm = 0.0;
        QString hklLabel;
        double pseudoVoigtEta = 0.5;
    };
    struct XRDAnalysisResult {
        bool valid = false;
        QString error;
        double wavelengthAngstrom = 1.541838;
        double meanScherrerSizeNm = 0.0;
        double whSizeNm = 0.0;     // NaN when W-H has no physical solution (intercept <= 0)
        double whMicrostrain = 0.0; // NaN when the W-H slope is negative (no physical solution)
        double whRSquare = 0.0;
        double whSlopeRaw = 0.0;     // unclipped W-H regression slope (= 4 eps form: beta cos = a + eps * 4 sin)
        double whInterceptRaw = 0.0; // unclipped W-H regression intercept (= K lambda / D)
        bool whPhysical = false;     // true only when slope >= 0 and intercept > 0
        double latticeParamAngstrom = 0.0;
        double latticeParamCAngstrom = 0.0; // HCP only: c0 = a0 * (c/a); 0 for cubic systems
        QString anodeName;            // matched X-ray tube anode ("Cu", "Co", ...) or empty
        double kAlpha2Angstrom = 0.0; // Ka2 wavelength used for stripping (0 = none)
        bool kAlpha2Stripped = false;
        bool anodeApproximate = false; // lambda matched an anode only within the loose 1e-3
        QString kAlpha2Notice; // shown to the user: approximate anode match, or stripping skipped
        QStringList notes;            // warnings / processing notes (also written to the log)
        QVector<XRDPeakEntry> peaks;
        Table *peakTable = nullptr;
        MultiLayer *whPlot = nullptr;
    };
    static XRDAnalysisResult analyzeXRD(ApplicationWindow *app, Graph *g,
                                        const QString &curveTitle = QString(),
                                        double wavelengthAngstrom = 1.541838,
                                        double instrFwhmDeg = 0.05,
                                        double scherrerK = 0.90,
                                        const QString &crystalSystem = QStringLiteral("fcc"),
                                        bool stripKAlpha2 = true,
                                        bool addTopDSpacingAxis = true,
                                        bool createWilliamsonHallPlot = true,
                                        double cOverA = 1.633);
    static void showXRDAnalyzerDialog(ApplicationWindow *app);

    //! Characteristic Ka1 / Ka2 wavelengths (Angstrom) of common X-ray tube anodes.
    struct XRayAnodeLines {
        QString anode;
        double kAlpha1 = 0.0;
        double kAlpha2 = 0.0;
        double kAlphaAvg = 0.0; // intensity-weighted (2 Ka1 + Ka2) / 3
    };
    static QVector<XRayAnodeLines> xrayAnodeTable();
    //! Matches a wavelength to an anode (Ka1 or weighted Ka average, relative tolerance 2e-4).
    //! When nothing matches that closely, the nearest line within a relative 1e-3 is accepted
    //! and *approximate is set (e.g. a rounded 1.54 A for Cu). Returns false when no anode
    //! matches. *matchedAverage tells which line matched.
    static bool matchXRayAnode(double wavelengthAngstrom, XRayAnodeLines *out,
                               bool *matchedAverage = nullptr, bool *approximate = nullptr);
    //! Why the Williamson-Hall line could not be fitted: fewer than 2 resolved peaks, or (for
    //! 2 or more) 4 sin(theta) values that coincide so the slope is undefined.
    static QString williamsonHallUnavailableNote(int resolvedPeakCount);

    struct VaspPlotResult {
        bool valid = false;
        QString error;
        double fermiEnergyEV = 0.0;
        double bandgapEV = 0.0;
        double dBandCenterEV = std::numeric_limits<double>::quiet_NaN();
        Table *dataTable = nullptr;
        MultiLayer *plot = nullptr;
    };
    static VaspPlotResult plotVaspDos(ApplicationWindow *app, Table *srcTable,
                                      const QString &energyCol, const QStringList &dosCols,
                                      const QStringList &downDosCols = QStringList(),
                                      double fermiEnergyEV = 0.0,
                                      bool verticalEnergyAxis = false);
    static VaspPlotResult plotVaspBands(ApplicationWindow *app, Table *srcTable,
                                        const QString &kDistCol, const QStringList &bandCols,
                                        const QVector<double> &kTickPositions = QVector<double>(),
                                        const QStringList &kTickLabels = QStringList(),
                                        double fermiEnergyEV = 0.0,
                                        double energyMinEV = -6.0, double energyMaxEV = 6.0);
    static void showVaspPlotterDialog(ApplicationWindow *app);
};

#endif // CURVEMATHTOOLS_H
