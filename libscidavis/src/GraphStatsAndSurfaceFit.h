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
};

#endif // GRAPH_STATS_AND_SURFACE_FIT_H
