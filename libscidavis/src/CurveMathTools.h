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

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

class ApplicationWindow;
class Graph;
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

    static void showCurveMathDialog(ApplicationWindow *app, Graph *g);
    static void showSubtractLineDialog(ApplicationWindow *app, Graph *g);
    static void showAverageCurvesDialog(ApplicationWindow *app, Graph *g);
    static void showNormalizeDialog(ApplicationWindow *app, Graph *g);
    static void showIntersectionLookupDialog(ApplicationWindow *app, Graph *g);
};

#endif // CURVEMATHTOOLS_H
