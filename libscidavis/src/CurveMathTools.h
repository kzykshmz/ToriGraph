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

class ApplicationWindow;
class Graph;
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
    static QVector<QPointF> findCurveIntersections(Graph *g, const QString &curveA,
                                                   const QString &curveB,
                                                   bool markOnGraph = true,
                                                   ApplicationWindow *app = nullptr,
                                                   Table **outTable = nullptr);
    static QVector<double> findXFromY(Graph *g, const QString &curveTitle, double targetY);
    static double findYFromX(Graph *g, const QString &curveTitle, double targetX, bool *ok = nullptr);
    static double polygonArea(Graph *g, const QString &curveTitle, bool mathematicalSigned = false);
    static double curveArcLength(Graph *g, const QString &curveTitle);

    struct ArrheniusResult {
        bool valid = false;
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

    static void showCurveMathDialog(ApplicationWindow *app, Graph *g);
    static void showSubtractLineDialog(ApplicationWindow *app, Graph *g);
    static void showAverageCurvesDialog(ApplicationWindow *app, Graph *g);
    static void showNormalizeDialog(ApplicationWindow *app, Graph *g);
    static void showIntersectionLookupDialog(ApplicationWindow *app, Graph *g);
    static void showArrheniusDialog(ApplicationWindow *app);
    static void showTaucDialog(ApplicationWindow *app);
    static void showFillBetweenDialog(ApplicationWindow *app, Graph *g);
    static void showEISAnalyzerDialog(ApplicationWindow *app);
};

#endif // CURVEMATHTOOLS_H
