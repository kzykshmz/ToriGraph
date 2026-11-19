/***************************************************************************
    File                 : GraphStatsAndSurfaceFit.h
    Project              : ToriGraph (SciDAVis OriginPro 2021 Parity)
    Description          : 3D Surface Fitting, 2D Peak Analysis, and Graph
                           Statistical Analysis (Confidence Ellipse, ANOVA/t-Test
                           with Significance Brackets, ROC, Kaplan-Meier, PCA Biplot)
 ***************************************************************************/
#ifndef GRAPH_STATS_AND_SURFACE_FIT_H
#define GRAPH_STATS_AND_SURFACE_FIT_H

#include <QString>
#include <QStringList>
#include <QVector>

class ApplicationWindow;
class Table;
class Matrix;
class Graph;
class MultiLayer;
class QwtPlotCurve;

class GraphStatsAndSurfaceFit
{
public:
    enum SurfaceModel {
        Plane2D = 0,
        Poly2DQuadratic = 1,
        Gauss2D = 2,
        Lorentz2D = 3,
        PseudoVoigt2D = 4,
        Exponential2D = 5,
        Chebyshev2D = 6,
        Custom2D = 7
    };

    struct SurfaceFitResult
    {
        bool converged = false;
        double rSquare = 0.0;
        double adjRSquare = 0.0;
        double reducedChi2 = 0.0;
        QStringList paramNames;
        QVector<double> params;
        QVector<double> errors;
        Matrix *fittedMatrix = nullptr;
        Table *paramTable = nullptr;
    };

    struct NormalityResult
    {
        int n = 0;
        double mean = 0.0;
        double sd = 0.0;
        double shapiroWilkW = 0.0;
        double shapiroWilkP = 1.0;
        double ksD = 0.0;
        double ksP = 1.0;
        double andersonDarlingA2 = 0.0;
        bool isNormalAt005 = true;
        Table *reportTable = nullptr;
        QwtPlotCurve *normalOverlayCurve = nullptr;
    };

    struct TwoWayANOVAResult
    {
        bool valid = false;
        double fFactorA = 0.0, pFactorA = 1.0;
        double fFactorB = 0.0, pFactorB = 1.0;
        double fInteraction = 0.0, pInteraction = 1.0;
        Table *reportTable = nullptr;
        MultiLayer *interactionGraph = nullptr;
    };

    struct WeibullPlotResult {
        bool valid = false;
        double modulusM = 0.0;        // shape beta / Weibull modulus m
        double modulusMSE = 0.0;
        double scaleSigma0 = 0.0;     // characteristic strength/life eta (63.2%)
        double scaleSigma0SE = 0.0;
        double thresholdGamma = 0.0;
        bool threeParameter = false;
        int censoredCount = 0;
        double b10Life = 0.0;
        double b50Life = 0.0;
        double rSquare = 0.0;
        double mleModulusM = 0.0;
        double mleModulusMSE = 0.0;
        double mleScaleSigma0 = 0.0;
        double mleScaleSigma0SE = 0.0;
        bool usedMle = false;
        Table *reportTable = nullptr;
        MultiLayer *weibullPlot = nullptr;
    };
    struct OutlierTestResult {
        bool valid = false;
        int n = 0;
        QVector<int> outlierRows;
        QVector<double> outlierValues;
        double primaryStatistic = 0.0; // Grubbs G, ESD R, or Dixon Q
        double testStatistic = 0.0;
        double criticalValue = 0.0;
        double pValue = 1.0;
        Table *reportTable = nullptr;
    };
    struct PassingBablokDemingResult {
        bool valid = false;
        int n = 0;
        double pbSlope = 0.0, pbSlopeCILow = 0.0, pbSlopeCIHigh = 0.0;
        double pbIntercept = 0.0, pbInterceptCILow = 0.0, pbInterceptCIHigh = 0.0;
        double cusumStat = 0.0, cusumLinearityP = 1.0;
        double demingSlope = 0.0, demingSlopeSE = 0.0, demingIntercept = 0.0, demingInterceptSE = 0.0;
        Table *summaryTable = nullptr;
        MultiLayer *comparisonPlot = nullptr;
    };
    enum NonparamMethod { TestMannWhitneyU = 0, TestWilcoxonSignedRank = 1,
                          TestKruskalWallisDunn = 2, TestAutoSmartSwitch = 3 };
    struct NonparamResult {
        bool valid = false;
        QString chosenTestName;
        double statistic = 0.0; // U, W, H, or F/t
        double pValue = 1.0;
        Table *reportTable = nullptr;
    };
    struct KMeansResult {
        bool valid = false;
        int k = 2;
        double silhouetteScore = 0.0;
        QVector<int> assignments;
        Table *resultTable = nullptr;
        MultiLayer *clusterPlot = nullptr;
    };

    static SurfaceFitResult fitSurfaceXYZ(ApplicationWindow *app, Table *srcTable,
                                          const QString &xCol, const QString &yCol,
                                          const QString &zCol, SurfaceModel model = Gauss2D,
                                          const QString &customExpr = QString(),
                                          int gridSide = 40);
    static SurfaceFitResult fitSurfaceMatrix(ApplicationWindow *app, Matrix *srcMatrix,
                                             SurfaceModel model = Gauss2D,
                                             const QString &customExpr = QString());
    static Table *analyze2DPeaks(ApplicationWindow *app, Matrix *srcMatrix,
                                 int maxPeaks = 10, double prominenceFrac = 0.10,
                                 Graph *overlayGraph = nullptr);

    static QwtPlotCurve *addConfidenceEllipse(ApplicationWindow *app, Graph *g,
                                              const QString &curveTitle,
                                              double confLevel = 0.95,
                                              bool predictionEllipse = false);
    static Table *runGroupComparisonAndAnnotate(ApplicationWindow *app, Table *srcTable,
                                                const QStringList &colNames,
                                                Graph *annotateGraph = nullptr);
    static MultiLayer *plotROCCurve(ApplicationWindow *app, Table *srcTable,
                                    const QString &stateCol, const QString &scoreCol,
                                    double *aucOut = nullptr);
    static MultiLayer *plotKaplanMeier(ApplicationWindow *app, Table *srcTable,
                                       const QString &timeCol, const QString &eventCol,
                                       const QString &groupCol = QString(),
                                       double *logRankPOut = nullptr);
    static MultiLayer *runPCAAndBiplot(ApplicationWindow *app, Table *srcTable,
                                       const QStringList &colNames,
                                       Table **scoresTableOut = nullptr);

    static NormalityResult runNormalityTest(ApplicationWindow *app, Table *srcTable,
                                            const QString &colName,
                                            Graph *histogramGraphToOverlay = nullptr);
    static MultiLayer *plotBlandAltman(ApplicationWindow *app, Table *srcTable,
                                       const QString &methodACol, const QString &methodBCol,
                                       double *meanBiasOut = nullptr,
                                       double *loaLowerOut = nullptr,
                                       double *loaUpperOut = nullptr);
    static TwoWayANOVAResult runTwoWayANOVA(ApplicationWindow *app, Table *srcTable,
                                            const QString &factorACol,
                                            const QString &factorBCol,
                                            const QString &responseCol,
                                            bool plotInteraction = true);

    static WeibullPlotResult plotWeibullProbability(
            ApplicationWindow *app, Table *srcTable, const QString &dataCol,
            const QString &censorCol = QString(),
            int censorCoding = 0 /*0: 1=Failure/0=Censored (KM standard); 1: 0=Failure/1=Censored*/,
            bool fitThreeParam = false,
            bool useMle = false,
            bool createPlot = true);
    static PassingBablokDemingResult runPassingBablokDeming(
            ApplicationWindow *app, Table *srcTable, const QString &colXName,
            const QString &colYName, double errorVarianceRatioLambda = 1.0,
            bool createPlot = true);
    static OutlierTestResult runOutlierTest(ApplicationWindow *app, Table *srcTable,
                                            const QString &colName,
                                            int method = 0 /*0=Grubbs, 1=GeneralizedESD, 2=DixonQ*/,
                                            double alpha = 0.05, int maxOutliers = 5,
                                            bool maskDetectedOutliers = false);
    static NonparamResult runNonparametricComparison(ApplicationWindow *app, Table *srcTable,
                                                     const QStringList &colNames,
                                                     NonparamMethod method = TestAutoSmartSwitch,
                                                     Graph *annotateGraph = nullptr);
    static MultiLayer *plotPairedComparison(ApplicationWindow *app, Table *srcTable,
                                            const QString &beforeCol, const QString &afterCol,
                                            bool useWilcoxon = false, double *pValueOut = nullptr);
    static MultiLayer *plotCorrelationMatrixHeatmap(ApplicationWindow *app, Table *srcTable,
                                                    const QStringList &colNames,
                                                    bool spearmanRank = false,
                                                    Matrix **corrMatrixOut = nullptr);
    static KMeansResult runKMeansClustering(ApplicationWindow *app, Table *srcTable,
                                            const QStringList &colNames, int k = 3,
                                            bool plotClusters = true);
    static MultiLayer *plotHierarchicalDendrogram(ApplicationWindow *app, Table *srcTable,
                                                  const QStringList &colNames,
                                                  int linkage = 0 /*0=Ward,1=Complete,2=Average*/);

    static void showSurfaceFitDialog(ApplicationWindow *app);
    static void show2DPeakDialog(ApplicationWindow *app);
    static void showConfidenceEllipseDialog(ApplicationWindow *app, Graph *g = nullptr);
    static void showGroupComparisonDialog(ApplicationWindow *app, Graph *g = nullptr);
    static void showROCDialog(ApplicationWindow *app);
    static void showKaplanMeierDialog(ApplicationWindow *app);
    static void showPCADialog(ApplicationWindow *app);
    static void showNormalityDialog(ApplicationWindow *app, Graph *g = nullptr);
    static void showBlandAltmanDialog(ApplicationWindow *app);
    static void showTwoWayANOVADialog(ApplicationWindow *app);
    static void showWeibullDialog(ApplicationWindow *app);
    static void showPassingBablokDemingDialog(ApplicationWindow *app);
    static void showOutlierTestDialog(ApplicationWindow *app);
    static void showNonparametricDialog(ApplicationWindow *app, Graph *g = nullptr);
    static void showPairedComparisonDialog(ApplicationWindow *app);
    static void showCorrelationMatrixDialog(ApplicationWindow *app);
    static void showClusteringDialog(ApplicationWindow *app);
};

#endif // GRAPH_STATS_AND_SURFACE_FIT_H
