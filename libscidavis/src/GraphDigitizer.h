/***************************************************************************
    File                 : GraphDigitizer.h
    Project              : ToriGraph (SciDAVis OriginPro Parity)
    Description          : Interactive Graph Image Digitizer (4-point linear/log10
                           axis calibration, manual picking, RGB color auto-tracing,
                           and worksheet/graph export)
 ***************************************************************************/
#ifndef GRAPH_DIGITIZER_H
#define GRAPH_DIGITIZER_H
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QVector>

class ApplicationWindow;
class MultiLayer;
class Table;

class GraphDigitizer
{
public:
    struct AxisCalibration {
        QPointF pixelX1, pixelX2;
        double valueX1 = 0.0, valueX2 = 1.0;
        bool logX = false;
        QPointF pixelY1, pixelY2;
        double valueY1 = 0.0, valueY2 = 1.0;
        bool logY = false;
    };

    static QPointF mapPixelToData(const QPointF &pixelPt, const AxisCalibration &cal);
    static QPointF mapDataToPixel(const QPointF &dataPt, const AxisCalibration &cal);
    static QVector<QPointF> autoTraceCurveByColor(const QImage &img,
                                                  const AxisCalibration &cal,
                                                  const QColor &targetColor,
                                                  int colorTolerance = 45,
                                                  int numSamplesX = 100,
                                                  const QRect &pixelRoi = QRect());
    static Table *exportPointsToTable(ApplicationWindow *app,
                                      const QVector<QPointF> &dataPts,
                                      const QString &tableName = QString(),
                                      MultiLayer **outPlot = nullptr);
    static void showDigitizerDialog(ApplicationWindow *app,
                                    const QString &imagePath = QString());
};
#endif
