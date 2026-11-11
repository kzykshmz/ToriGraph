/***************************************************************************
    File                 : PiecewiseAndRobustFit.h
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Description          : Piecewise Breakpoint Fit, Robust/Quantile Regression,
                           Dataset Comparison F-Test, and Multiple Linear Regression
 ***************************************************************************/

#ifndef PIECEWISE_AND_ROBUST_FIT_H
#define PIECEWISE_AND_ROBUST_FIT_H

#include <QString>
#include <QStringList>
#include <QVector>

class ApplicationWindow;
class Graph;
class MultiLayer;
class QwtPlotCurve;
class Table;

class PiecewiseAndRobustFit
{
public:
    enum RobustMethod { OrdinaryLS = 0, HuberM = 1, TukeyBisquare = 2, QuantileReg = 3 };

    struct PiecewiseResult {
        bool converged = false;
        int segments = 2;
        QVector<double> breakpointsX;
        QVector<double> breakpointsXSE;
        QVector<double> breakpointsY;
        QVector<double> slopes;
        double intercept0 = 0.0;
        double rSquare = 0.0;
        double aic = 0.0;
        QwtPlotCurve *fittedCurve = nullptr;
        Table *reportTable = nullptr;
    };

    struct RobustFitResult {
        bool converged = false;
        double intercept = 0.0;
        double slope = 0.0;
        double interceptSE = 0.0;
        double slopeSE = 0.0;
        double scaleMAD = 0.0;
        QwtPlotCurve *fittedCurve = nullptr;
        Table *reportTable = nullptr;
    };

    struct DatasetCompareResult {
        bool valid = false;
        double rssShared = 0.0;
        int dfShared = 0;
        double rssSeparate = 0.0;
        int dfSeparate = 0;
        double fStatistic = 0.0;
        double pValue = 1.0;
        double deltaAICc = 0.0;
        bool preferSeparate = false;
        Table *reportTable = nullptr;
    };

    struct MLRResult {
        bool valid = false;
        QVector<double> betas;
        QVector<double> stdErrors;
        QVector<double> tValues;
        QVector<double> pValues;
        QVector<double> vif;
        double rSquare = 0.0;
        double adjRSquare = 0.0;
        double fStatistic = 0.0;
        double fPValue = 1.0;
        Table *reportTable = nullptr;
        MultiLayer *diagnosticGraph = nullptr;
    };

    struct LogisticResult {
        bool converged = false;
        QVector<double> betas;      // beta_0, beta_1..beta_k
        QVector<double> stdErrors;
        QVector<double> waldZ;
        QVector<double> pValues;
        QVector<double> oddsRatios; // exp(beta_j) for j >= 1
        double logLikelihood = 0.0;
        double lrChi2 = 0.0;
        double lrPValue = 1.0;
        double mcFaddenR2 = 0.0;
        double aic = 0.0;
        Table *reportTable = nullptr;
        MultiLayer *probGraph = nullptr;
    };

    static PiecewiseResult fitPiecewiseLinear(ApplicationWindow *app, Graph *g,
                                              const QString &curveTitle, int numSegments = 2,
                                              bool annotateGraph = true);
    static RobustFitResult fitRobustOrQuantileLinear(ApplicationWindow *app, Graph *g,
                                                     const QString &curveTitle,
                                                     RobustMethod method = TukeyBisquare,
                                                     double quantileTau = 0.5,
                                                     bool annotateGraph = true);
    static DatasetCompareResult compareDatasets(ApplicationWindow *app, Graph *g,
                                                const QStringList &curveTitles,
                                                const QString &modelName = QString("ExpDec1"));
    static MLRResult fitMultipleLinearRegression(ApplicationWindow *app, Table *srcTable,
                                                 const QString &yColName,
                                                 const QStringList &xColNames,
                                                 bool plotDiagnostics = true);
    static LogisticResult runLogisticRegression(ApplicationWindow *app, Table *srcTable,
                                                const QString &yBinaryCol,
                                                const QStringList &xColNames,
                                                bool plotProbCurve = true);

    static void showPiecewiseFitDialog(ApplicationWindow *app, Graph *g);
    static void showRobustFitDialog(ApplicationWindow *app, Graph *g);
    static void showCompareDatasetsDialog(ApplicationWindow *app, Graph *g);
    static void showMLRDialog(ApplicationWindow *app);
    static void showLogisticRegressionDialog(ApplicationWindow *app);
};

#endif // PIECEWISE_AND_ROBUST_FIT_H
