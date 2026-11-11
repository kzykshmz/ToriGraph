#include "ApplicationWindow.h"
#include "MultiLayer.h"
#include "Graph.h"
#include "Plot.h"
#include "Legend.h"
#include "Table.h"
#include "MultiPeakFit.h"
#include "NonLinearFit.h"
#include "fit_gsl.h"
#include "BoxCurve.h"
#include "PlotCurve.h"
#include "EmfPaintDevice.h"
#include "OleServer.h"
#include "core/column/Column.h"
#include "OriginGadgets.h"
#include "DataPickerTool.h"
#include "PolynomialFit.h"
#include "CurveMathTools.h"
#include "AdvancedSignalProc.h"
#include "SmoothFilter.h"
#include "GraphStatsAndSurfaceFit.h"
#include "RecalcEngine.h"
#include "Matrix.h"
#include "FunctionCurve.h"

#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_div.h>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>
#include <cmath>
#include <iostream>
#include <wrl/client.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using Microsoft::WRL::ComPtr;

struct EmfRecordStats {
    int totalRecords = 0;
    int vectorPathOrLineRecords = 0;
    int textRecords = 0;
    int bitmapRecords = 0;
};

static int CALLBACK enumEmfProc(HDC, HANDLETABLE *, const ENHMETARECORD *rec, int, LPARAM data)
{
    auto *stats = reinterpret_cast<EmfRecordStats *>(data);
    if (!stats || !rec)
        return 0;
    stats->totalRecords++;
    switch (rec->iType) {
    case EMR_LINETO:
    case EMR_MOVETOEX:
    case EMR_POLYLINE:
    case EMR_POLYLINE16:
    case EMR_POLYGON:
    case EMR_POLYGON16:
    case EMR_POLYPOLYGON:
    case EMR_POLYPOLYGON16:
    case EMR_RECTANGLE:
    case EMR_ELLIPSE:
    case EMR_BEGINPATH:
    case EMR_ENDPATH:
    case EMR_FILLPATH:
    case EMR_STROKEPATH:
    case EMR_STROKEANDFILLPATH:
        stats->vectorPathOrLineRecords++;
        break;
    case EMR_EXTTEXTOUTW:
    case EMR_EXTTEXTOUTA:
        stats->textRecords++;
        break;
    case EMR_STRETCHDIBITS:
    case EMR_BITBLT:
        stats->bitmapRecords++;
        break;
    default:
        break;
    }
    return 1;
}

#define CHECK_COND(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")\n"; \
            return 1; \
        } \
    } while (0)

static bool relErrWithin(double actual, double expected, double tolRatio = 0.02)
{
    double denom = std::max(std::fabs(expected), 1e-9);
    return std::fabs(actual - expected) / denom <= tolRatio;
}

int main(int argc, char **argv)
{
    std::cout << std::unitbuf << "[INFO] Starting scidavis_ole_test...\n";
    QApplication app(argc, argv);
    QStringList args = app.arguments();

    QString errMsg;
    CHECK_COND(SciDAVisOle::ensureRegistryRegistered(&errMsg), "ensureRegistryRegistered failed");
    CHECK_COND(SciDAVisOle::initializeOleServer(false), "initializeOleServer failed");

    ApplicationWindow *mw = new ApplicationWindow();
    mw->applyUserSettings();
    mw->saveSettings();

    // =========================================================================
    // 1. OriginPro 2021 Reference Scatter Plot & Visual Parity
    // =========================================================================
    struct ScatterPt { double x; double y; int series; };
    const ScatterPt pts[] = {
        // H18 (series 1)
        { 194.0, 110.0, 1 }, { 202.0, 250.0, 1 }, { 204.0, 260.0, 1 },
        { 203.0, 315.0, 1 }, { 202.0, 830.0, 1 }, { 201.0, 1900.0, 1 },
        { 199.0, 4800.0, 1 }, { 191.0, 6500.0, 1 }, { 178.0, 17500.0, 1 },
        // 240°C-3h (series 2)
        { 112.0, 102.0, 2 }, { 119.0, 85.0, 2 }, { 122.0, 410.0, 2 },
        { 126.0, 540.0, 2 }, { 128.0, 1120.0, 2 }, { 131.0, 1480.0, 2 },
        { 133.0, 2550.0, 2 }, { 134.0, 3200.0, 2 }, { 141.0, 4700.0, 2 },
        { 148.0, 6800.0, 2 }, { 153.0, 10300.0, 2 }, { 158.0, 15800.0, 2 },
        { 160.0, 20500.0, 2 },
        // 260°C-3h (series 3)
        { 98.0, 245.0, 3 }, { 102.0, 178.0, 3 }, { 107.0, 290.0, 3 },
        { 110.0, 485.0, 3 }, { 115.0, 910.0, 3 }, { 119.0, 1380.0, 3 },
        { 124.0, 2580.0, 3 }, { 127.0, 2750.0, 3 }, { 138.0, 5350.0, 3 },
        { 145.0, 8100.0, 3 }, { 147.0, 10800.0, 3 }, { 149.0, 14500.0, 3 }
    };
    const int nScatterRows = int(sizeof(pts) / sizeof(pts[0]));
    Table *t = mw->newTable("TestTable", nScatterRows, 4);
    CHECK_COND(t != nullptr, "Failed to create Table");
    t->setColName(0, "stress");
    t->setColName(1, "H18");
    t->setColName(2, QString::fromUtf8("240\xC2\xB0" "C-3h"));
    t->setColName(3, QString::fromUtf8("260\xC2\xB0" "C-3h"));
    for (int r = 0; r < nScatterRows; ++r) {
        t->setCell(r, 0, pts[r].x);
        for (int c = 1; c <= 3; ++c) {
            if (pts[r].series == c)
                t->setCell(r, c, pts[r].y);
            else
                t->clearCell(r, c);
        }
    }

    QStringList cols;
    cols << (t->name() + "_H18")
         << (t->name() + "_" + QString::fromUtf8("240\xC2\xB0" "C-3h"))
         << (t->name() + "_" + QString::fromUtf8("260\xC2\xB0" "C-3h"));
    MultiLayer *ml = mw->multilayerPlot(t, cols, Graph::Scatter);
    CHECK_COND(ml != nullptr, "Failed to create MultiLayer plot");
    Graph *ag = ml->activeGraph();
    CHECK_COND(ag != nullptr, "Active Graph is null");
    ag->setXAxisTitle("Local flow stress, <i>&sigma;</i><sub>f</sub> / MPa");
    ag->setYAxisTitle("<i>N</i><sub>v</sub> / mm<sup>-3</sup>");
    ag->setScale(QwtPlot::xBottom, 50.0, 250.0, 50.0, 5, 1, 0, false);
    ag->setScale(QwtPlot::yLeft, 10.0, 100000.0, 0.0, 8, 8, 1, false);
    if (ag->legend()) {
        ag->legend()->setOriginCoord(162.0, 110.0);
    }

    // Verify 1:1 square plot canvas after non-square window resizes
    ml->show();
    QApplication::processEvents();
    ml->resize(800, 480);
    ml->canvasWidget()->resize(780, 420);
    QApplication::processEvents();
    QRect cRectWide = ag->plotWidget()->plotLayout()->canvasRect();
    std::cout << "[INFO] After wide resize (800x480): canvasRect=" << cRectWide.width() << "x"
              << cRectWide.height() << "\n";
    CHECK_COND(qAbs(cRectWide.width() - cRectWide.height()) <= 2,
               "Plot canvas did not preserve 1:1 aspect ratio on wide resize");

    ml->resize(480, 800);
    ml->canvasWidget()->resize(460, 740);
    QApplication::processEvents();
    QRect cRectTall = ag->plotWidget()->plotLayout()->canvasRect();
    std::cout << "[INFO] After tall resize (480x800): canvasRect=" << cRectTall.width() << "x"
              << cRectTall.height() << "\n";
    CHECK_COND(qAbs(cRectTall.width() - cRectTall.height()) <= 2,
               "Plot canvas did not preserve 1:1 aspect ratio on tall resize");

    ml->resize(520, 520);
    ml->canvasWidget()->resize(500, 460);
    QApplication::processEvents();
    ml->arrangeLayers(false, false);
    if (args.contains("--save-preview"))
        ml->exportImage(QCoreApplication::applicationDirPath() + "/style_preview.png", 100);

    // =========================================================================
    // 2. Test Mouse Wheel Zoom & Pan on Linear and Log10 Axes
    // =========================================================================
    Plot *plotW = ag->plotWidget();
    QWidget *canvasW = plotW->canvas();
    CHECK_COND(canvasW != nullptr, "Plot canvas is null");
    QPoint centerPt(canvasW->width() / 2, canvasW->height() / 2);

    double xMinBefore = plotW->axisScaleDiv(QwtPlot::xBottom)->lowerBound();
    double xMaxBefore = plotW->axisScaleDiv(QwtPlot::xBottom)->upperBound();
    double yMinBefore = plotW->axisScaleDiv(QwtPlot::yLeft)->lowerBound();
    double yMaxBefore = plotW->axisScaleDiv(QwtPlot::yLeft)->upperBound();

    // Send wheel zoom-in event (delta = +120)
    QWheelEvent wheelIn(centerPt, canvasW->mapToGlobal(centerPt), 120, Qt::NoButton,
                        Qt::NoModifier, Qt::Vertical);
    QCoreApplication::sendEvent(canvasW, &wheelIn);

    double xMinZoom = plotW->axisScaleDiv(QwtPlot::xBottom)->lowerBound();
    double xMaxZoom = plotW->axisScaleDiv(QwtPlot::xBottom)->upperBound();
    double yMinZoom = plotW->axisScaleDiv(QwtPlot::yLeft)->lowerBound();
    double yMaxZoom = plotW->axisScaleDiv(QwtPlot::yLeft)->upperBound();
    std::cout << "[INFO] After wheel zoom-in: X=[" << xMinZoom << ", " << xMaxZoom
              << "], Y(Log10)=[" << yMinZoom << ", " << yMaxZoom << "]\n";
    CHECK_COND((xMaxZoom - xMinZoom) < (xMaxBefore - xMinBefore),
               "Linear X axis range did not narrow on wheel zoom-in");
    CHECK_COND(yMinZoom > yMinBefore && yMaxZoom < yMaxBefore && yMinZoom > 0.0,
               "Log10 Y axis range did not narrow positively on wheel zoom-in");

    // Test Middle-Button Drag Pan on canvas
    QPoint dragStart = centerPt;
    QPoint dragEnd(centerPt.x() + 40, centerPt.y() - 30);
    QMouseEvent panPress(QEvent::MouseButtonPress, dragStart, Qt::MiddleButton,
                         Qt::MiddleButton, Qt::NoModifier);
    QMouseEvent panMove(QEvent::MouseMove, dragEnd, Qt::NoButton,
                        Qt::MiddleButton, Qt::NoModifier);
    QMouseEvent panRelease(QEvent::MouseButtonRelease, dragEnd, Qt::MiddleButton,
                           Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(canvasW, &panPress);
    QCoreApplication::sendEvent(canvasW, &panMove);
    QCoreApplication::sendEvent(canvasW, &panRelease);

    double xMinPan = plotW->axisScaleDiv(QwtPlot::xBottom)->lowerBound();
    double yMinPan = plotW->axisScaleDiv(QwtPlot::yLeft)->lowerBound();
    std::cout << "[INFO] After middle-drag pan: xMin=" << xMinPan << ", yMin=" << yMinPan << "\n";
    CHECK_COND(std::fabs(xMinPan - xMinZoom) > 1e-3 && yMinPan > 0.0,
               "Middle-drag pan did not shift axis ranges or broke Log10 positivity");

    // Restore reference scales
    ag->setScale(QwtPlot::xBottom, 50.0, 250.0, 50.0, 5, 1, 0, false);
    ag->setScale(QwtPlot::yLeft, 10.0, 100000.0, 0.0, 8, 8, 1, false);
    plotW->replot();

    // =========================================================================
    // 3. Test Single-Peak & Multi-Peak Auto-Detection & Fitting
    // =========================================================================
    const int nPts = 201;
    Table *peakTable = mw->newTable("PeakData", nPts, 4);
    CHECK_COND(peakTable != nullptr, "Failed to create PeakData table");
    peakTable->setColName(0, "X");
    peakTable->setColName(1, "Gauss2");
    peakTable->setColName(2, "Lorentz3");
    peakTable->setColName(3, "PseudoVoigt2");

    // Ground truth 2-peak Gauss (Area parameterization):
    // y = y0 + sum A_i * sqrt(2/pi) / w_i * exp(-2 * ((x - xc_i) / w_i)^2)
    const double g_y0 = 5.0;
    const double g_xc[2] = { 30.0, 68.0 };
    const double g_w[2] = { 6.0, 8.0 };
    const double g_A[2] = { 250.0, 420.0 };

    // Ground truth 3-peak Lorentz (Area parameterization):
    // y = y0 + sum (2 * A_i / pi) * w_i / (4 * (x - xc_i)^2 + w_i^2)
    const double l_y0 = 2.0;
    const double l_xc[3] = { 20.0, 50.0, 80.0 };
    const double l_w[3] = { 4.5, 6.0, 5.0 };
    const double l_A[3] = { 180.0, 320.0, 240.0 };

    // Ground truth 2-peak Pseudo-Voigt (mu = 0.5, w_i = FWHM, A_i = Area):
    const double pv_y0 = 3.5;
    const double pv_mu = 0.5;
    const double pv_xc[2] = { 36.0, 56.0 };
    const double pv_w[2] = { 13.0, 15.0 };
    const double pv_A[2] = { 420.0, 620.0 };

    const double cGaussNorm = std::sqrt(4.0 * std::log(2.0) / M_PI);
    const double cGaussExp = 4.0 * std::log(2.0);

    for (int i = 0; i < nPts; ++i) {
        double x = 0.5 * i; // 0.0 .. 100.0
        double yg = g_y0;
        for (int k = 0; k < 2; ++k) {
            double z = (x - g_xc[k]) / g_w[k];
            yg += g_A[k] * std::sqrt(2.0 / M_PI) / g_w[k] * std::exp(-2.0 * z * z);
        }

        double yl = l_y0;
        for (int k = 0; k < 3; ++k) {
            double dx = x - l_xc[k];
            yl += (2.0 * l_A[k] / M_PI) * l_w[k] / (4.0 * dx * dx + l_w[k] * l_w[k]);
        }

        double ypv = pv_y0;
        for (int k = 0; k < 2; ++k) {
            double dx = x - pv_xc[k];
            double w = pv_w[k];
            double L = (2.0 / M_PI) * w / (4.0 * dx * dx + w * w);
            double G = (cGaussNorm / w) * std::exp(-cGaussExp * dx * dx / (w * w));
            ypv += pv_A[k] * (pv_mu * L + (1.0 - pv_mu) * G);
        }

        peakTable->setCell(i, 0, x);
        peakTable->setCell(i, 1, yg);
        peakTable->setCell(i, 2, yl);
        peakTable->setCell(i, 3, ypv);
    }

    // 3a. 2-Peak Gaussian Auto-Detection & Fit
    MultiLayer *mlGauss = mw->multilayerPlot(
            peakTable, QStringList() << (peakTable->name() + "_Gauss2"), Graph::Scatter);
    CHECK_COND(mlGauss != nullptr, "Failed to create Gauss2 plot");
    Graph *gGauss = mlGauss->activeGraph();
    MultiPeakFit *gaussFit = new MultiPeakFit(mw, gGauss, MultiPeakFit::Gauss, 2);
    gaussFit->enablePeakCurves(true);
    CHECK_COND(gaussFit->setDataFromCurve(peakTable->name() + "_Gauss2"),
               "gaussFit->setDataFromCurve failed");
    gaussFit->fit(); // Automatically invokes guessInitialValues() -> autoDetectPeaks()

    const std::vector<double> &gRes = gaussFit->results();
    CHECK_COND(gRes.size() == 7, "Expected 7 parameters for 2-peak Gauss fit");
    std::cout << "[INFO] 2-Peak Gauss Fit: R^2=" << gaussFit->rSquare()
              << ", AdjR^2=" << gaussFit->adjustedRSquare()
              << ", chi^2=" << gaussFit->chiSquare()
              << ", Peak1(A=" << gRes[0] << ", xc=" << gRes[1] << ", w=" << gRes[2] << ")"
              << ", Peak2(A=" << gRes[3] << ", xc=" << gRes[4] << ", w=" << gRes[5] << ")"
              << ", y0=" << gRes[6] << "\n";
    CHECK_COND(gaussFit->rSquare() > 0.999, "2-Peak Gauss R^2 < 0.999");
    CHECK_COND(gaussFit->adjustedRSquare() > 0.999, "2-Peak Gauss Adjusted R^2 < 0.999");
    CHECK_COND(relErrWithin(gRes[0], g_A[0]) && relErrWithin(gRes[1], g_xc[0])
                       && relErrWithin(gRes[2], g_w[0]),
               "2-Peak Gauss Peak 1 parameters exceeded 2% tolerance");
    CHECK_COND(relErrWithin(gRes[3], g_A[1]) && relErrWithin(gRes[4], g_xc[1])
                       && relErrWithin(gRes[5], g_w[1]) && relErrWithin(gRes[6], g_y0),
               "2-Peak Gauss Peak 2 / baseline parameters exceeded 2% tolerance");
    // Data curve + total fit curve + 2 individual peak curves = 4 curves
    CHECK_COND(gGauss->curves() >= 4, "Expected individual deconvoluted peak curves on Gauss graph");

    Table *gaussSummary = gaussFit->createPeakSummaryTable("GaussPeakSummary");
    CHECK_COND(gaussSummary != nullptr && gaussSummary->numRows() == 2,
               "createPeakSummaryTable failed for 2-Peak Gauss");
    CHECK_COND(relErrWithin(gaussSummary->cell(0, 1), g_xc[0])
                       && relErrWithin(gaussSummary->cell(1, 1), g_xc[1]),
               "GaussPeakSummary centers mismatch");
    CHECK_COND(relErrWithin(gaussSummary->cell(0, 4) + gaussSummary->cell(1, 4),
                            g_A[0] + g_A[1]),
               "GaussPeakSummary total area mismatch");
    CHECK_COND(std::fabs((gaussSummary->cell(0, 5) + gaussSummary->cell(1, 5)) - 100.0) < 0.1,
               "GaussPeakSummary AreaPercent did not sum to 100%");

    Table *gaussResiduals = gaussFit->createResidualsTable("GaussResiduals");
    CHECK_COND(gaussResiduals != nullptr && gaussResiduals->numRows() == nPts,
               "createResidualsTable failed for 2-Peak Gauss");
    CHECK_COND(std::fabs(gaussResiduals->cell(50, 3)) < 1e-2,
               "Residual value in GaussResiduals table is too large");
    delete gaussFit;

    // 3b. 3-Peak Lorentzian Auto-Count Detection & Fit
    MultiLayer *mlLorentz = mw->multilayerPlot(
            peakTable, QStringList() << (peakTable->name() + "_Lorentz3"), Graph::Scatter);
    CHECK_COND(mlLorentz != nullptr, "Failed to create Lorentz3 plot");
    Graph *gLorentz = mlLorentz->activeGraph();
    MultiPeakFit *lorentzFit = new MultiPeakFit(mw, gLorentz, MultiPeakFit::Lorentz, 1);
    lorentzFit->enablePeakCurves(true);
    CHECK_COND(lorentzFit->setDataFromCurve(peakTable->name() + "_Lorentz3"),
               "lorentzFit->setDataFromCurve failed");
    int detectedLorentzPeaks = lorentzFit->autoDetectAndUpdateNumPeaks(10, 0.08);
    std::cout << "[INFO] Auto-detected Lorentz peak count = " << detectedLorentzPeaks << "\n";
    CHECK_COND(detectedLorentzPeaks == 3, "Expected autoDetectAndUpdateNumPeaks to find 3 peaks");
    lorentzFit->fit();

    const std::vector<double> &lRes = lorentzFit->results();
    CHECK_COND(lRes.size() == 10, "Expected 10 parameters for 3-peak Lorentz fit");
    std::cout << "[INFO] 3-Peak Lorentz Fit: R^2=" << lorentzFit->rSquare()
              << ", Peak1(A=" << lRes[0] << ", xc=" << lRes[1] << ", w=" << lRes[2] << ")"
              << ", Peak2(A=" << lRes[3] << ", xc=" << lRes[4] << ", w=" << lRes[5] << ")"
              << ", Peak3(A=" << lRes[6] << ", xc=" << lRes[7] << ", w=" << lRes[8] << ")"
              << ", y0=" << lRes[9] << "\n";
    CHECK_COND(lorentzFit->rSquare() > 0.999, "3-Peak Lorentz R^2 < 0.999");
    for (int k = 0; k < 3; ++k) {
        CHECK_COND(relErrWithin(lRes[3 * k + 0], l_A[k])
                           && relErrWithin(lRes[3 * k + 1], l_xc[k])
                           && relErrWithin(lRes[3 * k + 2], l_w[k]),
                   "3-Peak Lorentz parameter exceeded 2% tolerance");
    }
    CHECK_COND(gLorentz->curves() >= 5,
               "Expected 3 deconvoluted peak curves + total fit curve on Lorentz graph");
    delete lorentzFit;

    // 3c. 2-Peak Pseudo-Voigt Auto-Detection & Fit
    MultiLayer *mlPV = mw->multilayerPlot(
            peakTable, QStringList() << (peakTable->name() + "_PseudoVoigt2"), Graph::Scatter);
    CHECK_COND(mlPV != nullptr, "Failed to create PseudoVoigt2 plot");
    Graph *gPV = mlPV->activeGraph();
    gPV->setXAxisTitle("Diffraction angle, 2<i>&theta;</i> / deg");
    gPV->setYAxisTitle("Intensity / a.u.");
    MultiPeakFit *pvFit = new MultiPeakFit(mw, gPV, MultiPeakFit::PseudoVoigt, 2);
    pvFit->setLorentzFraction(pv_mu);
    pvFit->enablePeakCurves(true);
    CHECK_COND(pvFit->setDataFromCurve(peakTable->name() + "_PseudoVoigt2"),
               "pvFit->setDataFromCurve failed");
    pvFit->fit();

    const std::vector<double> &pvRes = pvFit->results();
    CHECK_COND(pvRes.size() == 7, "Expected 7 parameters for 2-peak Pseudo-Voigt fit");
    std::cout << "[INFO] 2-Peak Pseudo-Voigt Fit: R^2=" << pvFit->rSquare()
              << ", Peak1(A=" << pvRes[0] << ", xc=" << pvRes[1] << ", w=" << pvRes[2] << ")"
              << ", Peak2(A=" << pvRes[3] << ", xc=" << pvRes[4] << ", w=" << pvRes[5] << ")"
              << ", y0=" << pvRes[6] << "\n";
    CHECK_COND(pvFit->rSquare() > 0.999, "2-Peak Pseudo-Voigt R^2 < 0.999");
    for (int k = 0; k < 2; ++k) {
        CHECK_COND(relErrWithin(pvRes[3 * k + 0], pv_A[k])
                           && relErrWithin(pvRes[3 * k + 1], pv_xc[k])
                           && relErrWithin(pvRes[3 * k + 2], pv_w[k]),
                   "2-Peak Pseudo-Voigt parameter exceeded 2% tolerance");
    }
    if (args.contains("--save-preview"))
        mlPV->exportImage(QCoreApplication::applicationDirPath() + "/peak_fit_preview.png", 100);
    delete pvFit;

    // =========================================================================
    // 4. Test OriginPro 2021 Worksheet (Table) Auto-Expanding Paste & Keys
    // =========================================================================
    Table *pasteTbl = mw->newTable("PasteTest", 2, 2);
    CHECK_COND(pasteTbl != nullptr, "Failed to create PasteTest table");
    pasteTbl->setCellsSelected(0, 0, 0, 0, true);
    QApplication::clipboard()->setText(
            "1.5\t1.25e-3\t10.0\n"
            "2.5\t--\t20.0\n"
            "3.5\t3.5E+04\tNaN\n"
            "4.5\t7.0\t40.0\n");
    pasteTbl->pasteSelection();

    std::cout << "[INFO] After 4x3 paste into 2x2 table: rows=" << pasteTbl->numRows()
              << ", cols=" << pasteTbl->numCols() << "\n";
    CHECK_COND(pasteTbl->numRows() == 4 && pasteTbl->numCols() == 3,
               "Table::pasteSelection did not auto-expand rows and columns to 4x3");
    CHECK_COND(relErrWithin(pasteTbl->cell(0, 1), 1.25e-3, 1e-6),
               "Scientific notation 1.25e-3 was not parsed accurately");
    CHECK_COND(relErrWithin(pasteTbl->cell(2, 1), 3.5e4, 1e-6),
               "Scientific notation 3.5E+04 was not parsed accurately");
    CHECK_COND(pasteTbl->column(1)->isInvalid(1) && pasteTbl->column(2)->isInvalid(2),
               "Missing tokens '--' and 'NaN' were not marked invalid/empty");

    // Test Delete key clearing selected cell without shrinking rows
    pasteTbl->deselectAll();
    pasteTbl->setCellsSelected(0, 0, 0, 0, true);
    QKeyEvent delKey(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(pasteTbl, &delKey);
    CHECK_COND(pasteTbl->column(0)->isInvalid(0) && pasteTbl->numRows() == 4,
               "Delete key did not clear cell(0,0) or unexpectedly altered row count");

    // Test Return/Enter key at bottom row auto-appending a new row
    pasteTbl->deselectAll();
    pasteTbl->setCellsSelected(3, 0, 3, 0, true);
    QKeyEvent enterKey(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(pasteTbl, &enterKey);
    CHECK_COND(pasteTbl->numRows() == 5,
               "Pressing Return on bottom row did not auto-append row 5");

    // =========================================================================
    // 5. Test All OriginPro Peak Functions (Voigt, BiGaussian, EMG, LogNormal, DoniachSunjic)
    //    & Built-in Nonlinear Functions (Arrhenius, Boltzmann, Hollomon + guessBuiltInParameters)
    // =========================================================================
    Table *extPeakTable = mw->newTable("ExtPeakData", nPts, 6);
    CHECK_COND(extPeakTable != nullptr, "Failed to create ExtPeakData table");
    extPeakTable->setColName(0, "X");
    extPeakTable->setColName(1, "Voigt1");
    extPeakTable->setColName(2, "BiGauss1");
    extPeakTable->setColName(3, "EMG1");
    extPeakTable->setColName(4, "LogNorm1");
    extPeakTable->setColName(5, "Doniach1");

    const double v_y0 = 4.0, v_A = 350.0, v_xc = 45.0, v_w = 10.0, v_mu = 0.4;
    const double bg_y0 = 3.0, bg_A = 80.0, bg_xc = 40.0, bg_w = 14.0, bg_alpha = 1.5;
    const double emg_y0 = 2.5, emg_A = 400.0, emg_xc = 42.0, emg_w = 8.0, emg_tau = 1.2;
    const double ln_y0 = 2.0, ln_A = 90.0, ln_xc = 50.0, ln_w = 12.0, ln_asym = 1.5;
    const double ds_y0 = 3.0, ds_A = 75.0, ds_xc = 52.0, ds_w = 10.0, ds_alpha = 0.15;

    for (int i = 0; i < nPts; ++i) {
        double x = 0.5 * i; // 0.0 .. 100.0
        extPeakTable->setCell(i, 0, x);
        extPeakTable->setCell(i, 1,
                              v_y0 + eval_origin_peak_profile(MultiPeakFit::Voigt, v_A, v_xc, v_w,
                                                              x, v_mu, 2.0, 1.5, 1.2));
        extPeakTable->setCell(i, 2,
                              bg_y0 + eval_origin_peak_profile(MultiPeakFit::BiGaussian, bg_A, bg_xc,
                                                               bg_w, x, 0.5, 2.0, bg_alpha, 1.2));
        extPeakTable->setCell(i, 3,
                              emg_y0 + eval_origin_peak_profile(MultiPeakFit::EMG, emg_A, emg_xc,
                                                                emg_w, x, 0.5, 2.0, 1.5, emg_tau));
        extPeakTable->setCell(i, 4,
                              ln_y0 + eval_origin_peak_profile(MultiPeakFit::LogNormal, ln_A, ln_xc,
                                                               ln_w, x, 0.5, 2.0, ln_asym, 1.2));
        extPeakTable->setCell(i, 5,
                              ds_y0 + eval_origin_peak_profile(MultiPeakFit::DoniachSunjic, ds_A,
                                                               ds_xc, ds_w, x, 0.5, 2.0, ds_alpha,
                                                               1.2));
    }

    QStringList extCols;
    extCols << (extPeakTable->name() + "_Voigt1")
            << (extPeakTable->name() + "_BiGauss1")
            << (extPeakTable->name() + "_EMG1")
            << (extPeakTable->name() + "_LogNorm1")
            << (extPeakTable->name() + "_Doniach1");
    MultiLayer *mlExtPeak = mw->multilayerPlot(extPeakTable, extCols, Graph::Scatter);
    CHECK_COND(mlExtPeak != nullptr, "Failed to create ExtPeakData plot");
    Graph *gExtPeak = mlExtPeak->activeGraph();

    // 5a. Voigt Peak Fit
    {
        MultiPeakFit *fitV = new MultiPeakFit(mw, gExtPeak, MultiPeakFit::Voigt, 1);
        fitV->setLorentzFraction(v_mu);
        CHECK_COND(fitV->setDataFromCurve(extPeakTable->name() + "_Voigt1"), "Voigt setDataFromCurve failed");
        fitV->fit();
        const auto &res = fitV->results();
        std::cout << "[INFO] Voigt Fit: R^2=" << fitV->rSquare()
                  << ", A=" << res[0] << ", xc=" << res[1] << ", w=" << res[2] << ", y0=" << res[3] << "\n";
        CHECK_COND(fitV->rSquare() > 0.999 && relErrWithin(res[0], v_A) && relErrWithin(res[1], v_xc)
                           && relErrWithin(res[2], v_w) && relErrWithin(res[3], v_y0),
                   "Voigt peak fit failed accuracy check");
        delete fitV;
    }

    // 5b. BiGaussian Peak Fit
    {
        MultiPeakFit *fitBG = new MultiPeakFit(mw, gExtPeak, MultiPeakFit::BiGaussian, 1);
        fitBG->setAsymmetryFactor(bg_alpha);
        CHECK_COND(fitBG->setDataFromCurve(extPeakTable->name() + "_BiGauss1"), "BiGaussian setDataFromCurve failed");
        fitBG->fit();
        const auto &res = fitBG->results();
        std::cout << "[INFO] BiGaussian Fit: R^2=" << fitBG->rSquare()
                  << ", A=" << res[0] << ", xc=" << res[1] << ", w=" << res[2] << ", y0=" << res[3] << "\n";
        CHECK_COND(fitBG->rSquare() > 0.999 && relErrWithin(res[0], bg_A) && relErrWithin(res[1], bg_xc)
                           && relErrWithin(res[2], bg_w) && relErrWithin(res[3], bg_y0),
                   "BiGaussian peak fit failed accuracy check");
        delete fitBG;
    }

    // 5c. EMG Peak Fit
    {
        MultiPeakFit *fitEMG = new MultiPeakFit(mw, gExtPeak, MultiPeakFit::EMG, 1);
        fitEMG->setTailFactor(emg_tau);
        CHECK_COND(fitEMG->setDataFromCurve(extPeakTable->name() + "_EMG1"), "EMG setDataFromCurve failed");
        fitEMG->fit();
        const auto &res = fitEMG->results();
        std::cout << "[INFO] EMG Fit: R^2=" << fitEMG->rSquare()
                  << ", A=" << res[0] << ", xc=" << res[1] << ", w=" << res[2] << ", y0=" << res[3] << "\n";
        CHECK_COND(fitEMG->rSquare() > 0.999 && relErrWithin(res[0], emg_A, 0.03)
                           && relErrWithin(res[1], emg_xc, 0.03) && relErrWithin(res[2], emg_w, 0.03),
                   "EMG peak fit failed accuracy check");
        delete fitEMG;
    }

    // 5d. LogNormal Peak Fit
    {
        MultiPeakFit *fitLN = new MultiPeakFit(mw, gExtPeak, MultiPeakFit::LogNormal, 1);
        fitLN->setAsymmetryFactor(ln_asym);
        CHECK_COND(fitLN->setDataFromCurve(extPeakTable->name() + "_LogNorm1"), "LogNormal setDataFromCurve failed");
        fitLN->fit();
        const auto &res = fitLN->results();
        std::cout << "[INFO] LogNormal Fit: R^2=" << fitLN->rSquare()
                  << ", A=" << res[0] << ", xc=" << res[1] << ", w=" << res[2] << ", y0=" << res[3] << "\n";
        CHECK_COND(fitLN->rSquare() > 0.999 && relErrWithin(res[0], ln_A, 0.03)
                           && relErrWithin(res[1], ln_xc, 0.03) && relErrWithin(res[2], ln_w, 0.03),
                   "LogNormal peak fit failed accuracy check");
        delete fitLN;
    }

    // 5e. Doniach-Sunjic Peak Fit
    {
        MultiPeakFit *fitDS = new MultiPeakFit(mw, gExtPeak, MultiPeakFit::DoniachSunjic, 1);
        fitDS->setAsymmetryFactor(ds_alpha);
        CHECK_COND(fitDS->setDataFromCurve(extPeakTable->name() + "_Doniach1"), "DoniachSunjic setDataFromCurve failed");
        fitDS->fit();
        const auto &res = fitDS->results();
        std::cout << "[INFO] DoniachSunjic Fit: R^2=" << fitDS->rSquare()
                  << ", A=" << res[0] << ", xc=" << res[1] << ", w=" << res[2] << ", y0=" << res[3] << "\n";
        CHECK_COND(fitDS->rSquare() > 0.999 && relErrWithin(res[0], ds_A, 0.03)
                           && relErrWithin(res[1], ds_xc, 0.03) && relErrWithin(res[2], ds_w, 0.03),
                   "DoniachSunjic peak fit failed accuracy check");
        delete fitDS;
    }

    // 5f. Built-in Nonlinear Functions (Arrhenius, Boltzmann, Hollomon with guessBuiltInParameters)
    QStringList builtInModels = NonLinearFit::builtInModelNames();
    std::cout << "[INFO] Built-in Nonlinear Models count = " << builtInModels.size() << "\n";
    CHECK_COND(builtInModels.size() >= 50, "Expected at least 50 built-in OriginPro nonlinear models");

    const int nNlfPts = 31;
    Table *nlfTable = mw->newTable("NlfData", nNlfPts, 6);
    CHECK_COND(nlfTable != nullptr, "Failed to create NlfData table");
    nlfTable->setColName(0, "T_K");
    nlfTable->setColPlotDesignation(0, SciDAVis::X);
    nlfTable->setColName(1, "ArrheniusY");
    nlfTable->setColPlotDesignation(1, SciDAVis::Y);
    nlfTable->setColName(2, "BoltzX");
    nlfTable->setColPlotDesignation(2, SciDAVis::X);
    nlfTable->setColName(3, "BoltzmannY");
    nlfTable->setColPlotDesignation(3, SciDAVis::Y);
    nlfTable->setColName(4, "Strain");
    nlfTable->setColPlotDesignation(4, SciDAVis::X);
    nlfTable->setColName(5, "HollomonY");
    nlfTable->setColPlotDesignation(5, SciDAVis::Y);

    const double arr_A = 1.5e6, arr_Ea = 42000.0;
    const double blz_A1 = 10.0, blz_A2 = 95.0, blz_x0 = 48.0, blz_dx = 6.5;
    const double hol_K = 680.0, hol_n = 0.24;

    for (int i = 0; i < nNlfPts; ++i) {
        double T = 350.0 + 15.0 * i; // 350 .. 800 K
        double yArr = arr_A * std::exp(-arr_Ea / (8.3144626 * T));
        double xb = (100.0 * i) / (nNlfPts - 1);
        double yBlz = (blz_A1 - blz_A2) / (1.0 + std::exp((xb - blz_x0) / blz_dx)) + blz_A2;
        double eps = 0.02 + 0.012 * i;
        double yHol = hol_K * std::pow(eps, hol_n);

        nlfTable->setCell(i, 0, T);
        nlfTable->setCell(i, 1, yArr);
        nlfTable->setCell(i, 2, xb);
        nlfTable->setCell(i, 3, yBlz);
        nlfTable->setCell(i, 4, eps);
        nlfTable->setCell(i, 5, yHol);
    }

    MultiLayer *mlNlf = mw->multilayerPlot(nlfTable, QStringList() << (nlfTable->name() + "_ArrheniusY"), Graph::Scatter);
    Graph *gNlf = mlNlf->activeGraph();
    gNlf->insertCurve(nlfTable, nlfTable->name() + "_BoltzX", nlfTable->name() + "_BoltzmannY", Graph::Scatter);
    gNlf->insertCurve(nlfTable, nlfTable->name() + "_Strain", nlfTable->name() + "_HollomonY", Graph::Scatter);

    {
        NonLinearFit *nlfArr = new NonLinearFit(mw, gNlf);
        CHECK_COND(nlfArr->setBuiltInModel("Arrhenius"), "setBuiltInModel(Arrhenius) failed");
        CHECK_COND(nlfArr->setDataFromCurve(nlfTable->name() + "_ArrheniusY"), "Arrhenius setDataFromCurve failed");
        nlfArr->guessBuiltInParameters();
        nlfArr->fit();
        const auto &res = nlfArr->results();
        std::cout << "[INFO] Arrhenius NLFit: R^2=" << nlfArr->rSquare()
                  << ", A=" << res[0] << ", Ea=" << res[1] << "\n";
        CHECK_COND(nlfArr->rSquare() > 0.999 && relErrWithin(res[0], arr_A, 0.02)
                           && relErrWithin(res[1], arr_Ea, 0.02),
                   "Arrhenius nonlinear fit failed accuracy check");
        delete nlfArr;
    }

    {
        NonLinearFit *nlfBlz = new NonLinearFit(mw, gNlf);
        CHECK_COND(nlfBlz->setBuiltInModel("Boltzmann"), "setBuiltInModel(Boltzmann) failed");
        CHECK_COND(nlfBlz->setDataFromCurve(nlfTable->name() + "_BoltzmannY"), "Boltzmann setDataFromCurve failed");
        nlfBlz->guessBuiltInParameters();
        nlfBlz->fit();
        const auto &res = nlfBlz->results();
        std::cout << "[INFO] Boltzmann NLFit: R^2=" << nlfBlz->rSquare()
                  << ", A1=" << res[0] << ", A2=" << res[1] << ", x0=" << res[2] << ", dx=" << res[3] << "\n";
        CHECK_COND(nlfBlz->rSquare() > 0.999 && relErrWithin(res[0], blz_A1, 0.02)
                           && relErrWithin(res[1], blz_A2, 0.02) && relErrWithin(res[2], blz_x0, 0.02)
                           && relErrWithin(res[3], blz_dx, 0.02),
                   "Boltzmann nonlinear fit failed accuracy check");
        delete nlfBlz;
    }

    {
        NonLinearFit *nlfHol = new NonLinearFit(mw, gNlf);
        CHECK_COND(nlfHol->setBuiltInModel("Hollomon"), "setBuiltInModel(Hollomon) failed");
        CHECK_COND(nlfHol->setDataFromCurve(nlfTable->name() + "_HollomonY"), "Hollomon setDataFromCurve failed");
        nlfHol->guessBuiltInParameters();
        nlfHol->fit();
        const auto &res = nlfHol->results();
        std::cout << "[INFO] Hollomon NLFit: R^2=" << nlfHol->rSquare()
                  << ", K=" << res[0] << ", n=" << res[1] << "\n";
        CHECK_COND(nlfHol->rSquare() > 0.999 && relErrWithin(res[0], hol_K, 0.02)
                           && relErrWithin(res[1], hol_n, 0.02),
                   "Hollomon nonlinear fit failed accuracy check");
        delete nlfHol;
    }

    // =========================================================================
    // 6. Test Peak Analyzer (BaselineLinear + BaselineALS + Subtraction + Integration)
    // =========================================================================
    Table *baseTable = mw->newTable("BaselineData", nPts, 3);
    CHECK_COND(baseTable != nullptr, "Failed to create BaselineData table");
    baseTable->setColName(0, "X");
    baseTable->setColName(1, "LinBaseSpec");
    baseTable->setColName(2, "ALSBaseSpec");

    const double bPeak_xc[2] = { 32.0, 68.0 };
    const double bPeak_w[2] = { 5.0, 6.0 };
    const double bPeak_A[2] = { 220.0, 340.0 };

    for (int i = 0; i < nPts; ++i) {
        double x = 0.5 * i; // 0..100
        double peaksVal = 0.0;
        for (int k = 0; k < 2; ++k) {
            double z = (x - bPeak_xc[k]) / bPeak_w[k];
            peaksVal += bPeak_A[k] * std::sqrt(2.0 / M_PI) / bPeak_w[k] * std::exp(-2.0 * z * z);
        }
        double linBase = 15.0 + 0.35 * x;
        double alsBase = 12.0 + 16.0 * std::exp(-x / 40.0) + 0.08 * x;
        baseTable->setCell(i, 0, x);
        baseTable->setCell(i, 1, linBase + peaksVal);
        baseTable->setCell(i, 2, alsBase + peaksVal);
    }

    MultiLayer *mlBase = mw->multilayerPlot(
            baseTable,
            QStringList() << (baseTable->name() + "_LinBaseSpec") << (baseTable->name() + "_ALSBaseSpec"),
            Graph::Line);
    Graph *gBase = mlBase->activeGraph();

    // 6a. BaselineLinear + insertBaselineCurve + subtractBaselineFromData + integratePeaksToTable
    {
        MultiPeakFit *mpfLin = new MultiPeakFit(mw, gBase, MultiPeakFit::Gauss, 2);
        mpfLin->setBaselineMode(MultiPeakFit::BaselineLinear);
        CHECK_COND(mpfLin->baselineMode() == MultiPeakFit::BaselineLinear, "baselineMode() mismatch");
        CHECK_COND(mpfLin->setDataFromCurve(baseTable->name() + "_LinBaseSpec"), "LinBaseSpec setDataFromCurve failed");

        std::vector<double> compLin = mpfLin->computeBaseline();
        CHECK_COND(compLin.size() == size_t(nPts), "computeBaseline size mismatch");
        CHECK_COND(std::fabs(compLin[100] - (15.0 + 0.35 * 50.0)) < 1.0,
                   "BaselineLinear midpoint deviated from ground-truth linear background");

        PlotCurve *baseCurve = mpfLin->insertBaselineCurve();
        CHECK_COND(baseCurve != nullptr, "insertBaselineCurve returned nullptr");

        CHECK_COND(mpfLin->subtractBaselineFromData(), "subtractBaselineFromData failed");
        mpfLin->fit();
        const auto &res = mpfLin->results();
        std::cout << "[INFO] BaselineLinear Subtracted 2-Peak Fit: R^2=" << mpfLin->rSquare()
                  << ", A1=" << res[0] << ", xc1=" << res[1] << ", A2=" << res[3] << ", xc2=" << res[4]
                  << ", y0=" << res[6] << "\n";
        CHECK_COND(mpfLin->rSquare() > 0.998 && relErrWithin(res[0], bPeak_A[0], 0.03)
                           && relErrWithin(res[3], bPeak_A[1], 0.03) && std::fabs(res[6]) < 1.0,
                   "BaselineLinear subtracted fit failed accuracy check");

        Table *intTbl = mpfLin->integratePeaksToTable("PeakIntLinear");
        CHECK_COND(intTbl != nullptr && intTbl->numRows() == 2 && intTbl->numCols() == 7,
                   "integratePeaksToTable failed");
        std::cout << "[INFO] Integrated Peak 1 Area=" << intTbl->cell(0, 4)
                  << ", Peak 2 Area=" << intTbl->cell(1, 4)
                  << ", Sum%=" << (intTbl->cell(0, 5) + intTbl->cell(1, 5)) << "\n";
        CHECK_COND(relErrWithin(intTbl->cell(0, 4), bPeak_A[0], 0.05)
                           && relErrWithin(intTbl->cell(1, 4), bPeak_A[1], 0.05)
                           && std::fabs((intTbl->cell(0, 5) + intTbl->cell(1, 5)) - 100.0) < 0.1,
                   "integratePeaksToTable areas or percentages out of tolerance");
        delete mpfLin;
    }

    // 6b. BaselineALS curved baseline subtraction & fit
    {
        MultiPeakFit *mpfALS = new MultiPeakFit(mw, gBase, MultiPeakFit::Gauss, 2);
        mpfALS->setBaselineMode(MultiPeakFit::BaselineALS);
        CHECK_COND(mpfALS->setDataFromCurve(baseTable->name() + "_ALSBaseSpec"), "ALSBaseSpec setDataFromCurve failed");
        std::vector<double> compALS = mpfALS->computeBaseline();
        std::cout << "[INFO] BaselineALS midpoint (x=50): computed=" << compALS[100]
                  << ", expected=" << (12.0 + 16.0 * std::exp(-50.0 / 40.0) + 4.0) << "\n";
        CHECK_COND(compALS.size() == size_t(nPts) && compALS[100] > 18.5 && compALS[100] < 23.5,
                   "BaselineALS did not track curved background envelope");
        CHECK_COND(mpfALS->subtractBaselineFromData(), "ALS subtractBaselineFromData failed");
        mpfALS->fit();
        std::cout << "[INFO] BaselineALS Subtracted 2-Peak Fit: R^2=" << mpfALS->rSquare() << "\n";
        CHECK_COND(mpfALS->rSquare() > 0.99, "BaselineALS subtracted fit R^2 < 0.99");
        delete mpfALS;
    }

    // =========================================================================
    // 7. Test OriginPro Plot Types (Y-Offset Stack, Violin+Jitter, Ternary, Pareto, Double-Y)
    // =========================================================================
    // 7a. Waterfall / Y-Offset Stack
    {
        QwtPlotCurve *c0 = gBase->curve(0);
        QwtPlotCurve *c1 = gBase->curve(1);
        CHECK_COND(c0 && c1, "Missing curves on gBase before applyYOffsetStack");
        double y1Before = c1->y(0);
        gBase->applyYOffsetStack(50.0, false);
        double y1After = c1->y(0);
        std::cout << "[INFO] Y-Offset Stack: curve1 y(0) before=" << y1Before << ", after=" << y1After << "\n";
        CHECK_COND(relErrWithin(y1After - y1Before, 50.0, 1e-4), "applyYOffsetStack did not offset curve 1 by 50.0");
    }

    // 7b. Violin + Jittered Data BoxCurve
    {
        MultiLayer *mlBox = mw->multilayerPlot(
                peakTable, QStringList() << (peakTable->name() + "_Gauss2"), Graph::Box);
        CHECK_COND(mlBox != nullptr && mlBox->activeGraph() != nullptr, "Failed to create Box plot");
        Graph *gBox = mlBox->activeGraph();
        BoxCurve *bc = dynamic_cast<BoxCurve *>(gBox->curve(0));
        CHECK_COND(bc != nullptr, "Curve 0 on Box plot is not a BoxCurve");
        bc->setViolinMode(true);
        bc->setShowDataPoints(true);
        CHECK_COND(bc->violinMode() && bc->showDataPoints(), "BoxCurve violinMode/showDataPoints getters failed");
        gBox->replot();
        QApplication::processEvents();
    }

    // 7c. Ternary Plot & ternaryToCartesian
    {
        QPointF pA = Graph::ternaryToCartesian(1.0, 0.0, 0.0);
        QPointF pB = Graph::ternaryToCartesian(0.0, 1.0, 0.0);
        QPointF pC = Graph::ternaryToCartesian(0.0, 0.0, 1.0);
        QPointF pCenter = Graph::ternaryToCartesian(1.0, 1.0, 1.0);
        CHECK_COND(std::fabs(pA.x() - 0.0) < 1e-6 && std::fabs(pA.y() - 0.0) < 1e-6, "ternaryToCartesian(A) mismatch");
        CHECK_COND(std::fabs(pB.x() - 1.0) < 1e-6 && std::fabs(pB.y() - 0.0) < 1e-6, "ternaryToCartesian(B) mismatch");
        CHECK_COND(std::fabs(pC.x() - 0.5) < 1e-6 && std::fabs(pC.y() - std::sqrt(3.0) * 0.5) < 1e-6,
                   "ternaryToCartesian(C) mismatch");
        CHECK_COND(std::fabs(pCenter.x() - 0.5) < 1e-6 && std::fabs(pCenter.y() - std::sqrt(3.0) / 6.0) < 1e-6,
                   "ternaryToCartesian(center) mismatch");

        Table *ternSrc = mw->newTable("TernaryInput", 4, 3);
        ternSrc->setColName(0, "Fe");
        ternSrc->setColName(1, "Cr");
        ternSrc->setColName(2, "Ni");
        const double ternRows[4][3] = {
            { 70.0, 18.0, 12.0 }, { 50.0, 25.0, 25.0 }, { 20.0, 50.0, 30.0 }, { 10.0, 20.0, 70.0 }
        };
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 3; ++c)
                ternSrc->setCell(r, c, ternRows[r][c]);
        mw->activateSubWindow(ternSrc);
        mw->plotTernary();
        MultiLayer *mlTern = qobject_cast<MultiLayer *>(mw->windowsList().last());
        CHECK_COND(mlTern != nullptr && mlTern->activeGraph() != nullptr, "plotTernary did not create MultiLayer");
        CHECK_COND(!mlTern->activeGraph()->plotWidget()->axisEnabled(QwtPlot::xBottom),
                   "setupTernaryPlot did not hide rectangular xBottom axis");
    }

    // 7d. Pareto Chart (plotPareto)
    {
        Table *paretoSrc = mw->newTable("ParetoInput", 5, 2);
        paretoSrc->setColName(0, "DefectId");
        paretoSrc->setColName(1, "Count");
        const double counts[5] = { 15.0, 80.0, 35.0, 50.0, 20.0 };
        for (int r = 0; r < 5; ++r) {
            paretoSrc->setCell(r, 0, r + 1);
            paretoSrc->setCell(r, 1, counts[r]);
        }
        paretoSrc->deselectAll();
        paretoSrc->setCellsSelected(0, 1, 4, 1, true);
        mw->activateSubWindow(paretoSrc);
        mw->plotPareto();
        MultiLayer *mlPar = qobject_cast<MultiLayer *>(mw->windowsList().last());
        CHECK_COND(mlPar != nullptr && mlPar->activeGraph() != nullptr, "plotPareto did not create MultiLayer");
        Graph *gPar = mlPar->activeGraph();
        CHECK_COND(gPar->curves() >= 2, "Expected 2 curves (bars + cumulative %) on Pareto chart");
        CHECK_COND(gPar->curve(1)->yAxis() == QwtPlot::yRight
                           && gPar->plotWidget()->axisEnabled(QwtPlot::yRight),
                   "Pareto cumulative curve is not bound to enabled yRight axis");
        CHECK_COND(relErrWithin(gPar->curve(1)->y(4), 100.0, 1e-4),
                   "Pareto final cumulative percentage is not 100%");
    }

    // 7e. Double-Y Plot (plotDoubleY)
    {
        Table *dySrc = mw->newTable("DoubleYInput", 5, 3);
        dySrc->setColName(0, "Time");
        dySrc->setColPlotDesignation(0, SciDAVis::X);
        dySrc->setColName(1, "Temperature");
        dySrc->setColPlotDesignation(1, SciDAVis::Y);
        dySrc->setColName(2, "Pressure");
        dySrc->setColPlotDesignation(2, SciDAVis::Y);
        for (int r = 0; r < 5; ++r) {
            dySrc->setCell(r, 0, r * 10.0);
            dySrc->setCell(r, 1, 300.0 + r * 50.0);
            dySrc->setCell(r, 2, 1.0 + r * 2.5);
        }
        mw->activateSubWindow(dySrc);
        mw->plotDoubleY();
        MultiLayer *mlDY = qobject_cast<MultiLayer *>(mw->windowsList().last());
        CHECK_COND(mlDY != nullptr && mlDY->activeGraph() != nullptr, "plotDoubleY did not create MultiLayer");
        Graph *gDY = mlDY->activeGraph();
        CHECK_COND(gDY->curves() >= 2 && gDY->curve(0)->yAxis() == QwtPlot::yLeft
                           && gDY->curve(1)->yAxis() == QwtPlot::yRight,
                   "plotDoubleY did not bind curve 0 to yLeft and curve 1 to yRight");
    }

    // =========================================================================
    // 8. Test OriginPro Color Palettes (builtInPaletteNames, interpolatePalette, applyColorPaletteByName)
    // =========================================================================
    QStringList palNames = Graph::builtInPaletteNames();
    std::cout << "[INFO] Built-in Color Palettes count = " << palNames.size() << "\n";
    CHECK_COND(palNames.size() >= 10 && palNames.contains("Viridis")
                       && palNames.contains("Origin Classic"),
               "builtInPaletteNames missing required OriginPro presets");

    QList<QColor> anchors;
    anchors << QColor("#000000") << QColor("#ffffff");
    QList<QColor> interp = Graph::interpolatePalette(anchors, 5);
    CHECK_COND(interp.size() == 5 && interp.first() == QColor("#000000")
                       && interp.last() == QColor("#ffffff"),
               "Graph::interpolatePalette failed");

    ag->applyColorPaletteByName("Viridis", false);
    QList<QColor> viridisCols = Graph::paletteColors("Viridis");
    CHECK_COND(!viridisCols.isEmpty() && ag->curve(0)->pen().color() == viridisCols[0],
               "applyColorPaletteByName(Viridis) did not update curve 0 color");

    ag->applyColorPaletteByName("Origin Classic", false);
    QList<QColor> classicCols = Graph::paletteColors("Origin Classic");
    CHECK_COND(!classicCols.isEmpty() && ag->curve(0)->pen().color() == classicCols[0],
               "applyColorPaletteByName(Origin Classic) did not update curve 0 color");

    // =========================================================================
    // 9. Test Configurable Graph Aspect Ratio & X-Axis '1000' Label Clipping Fix
    // =========================================================================
    ml->resize(720, 540);
    ml->canvasWidget()->resize(700, 480);
    ml->applyAspectRatioPreset(4.0 / 3.0, true);
    QApplication::processEvents();
    QRect cRect43 = ag->plotWidget()->plotLayout()->canvasRect();
    double ratio43 = double(cRect43.width()) / double(qMax(1, cRect43.height()));
    std::cout << "[INFO] Aspect preset 4:3 -> canvasRect=" << cRect43.width() << "x"
              << cRect43.height() << " (ratio=" << ratio43 << ")\n";
    CHECK_COND(std::fabs(ratio43 - 4.0 / 3.0) < 0.05, "applyAspectRatioPreset(4:3) failed");

    ml->applyAspectRatioPreset(16.0 / 9.0, true);
    QApplication::processEvents();
    QRect cRect169 = ag->plotWidget()->plotLayout()->canvasRect();
    double ratio169 = double(cRect169.width()) / double(qMax(1, cRect169.height()));
    std::cout << "[INFO] Aspect preset 16:9 -> canvasRect=" << cRect169.width() << "x"
              << cRect169.height() << " (ratio=" << ratio169 << ")\n";
    CHECK_COND(std::fabs(ratio169 - 16.0 / 9.0) < 0.06, "applyAspectRatioPreset(16:9) failed");

    ml->setKeepAspectRatio(false);
    CHECK_COND(!ml->keepAspectRatio(), "setKeepAspectRatio(false) failed");
    ml->resize(820, 440);
    ml->canvasWidget()->resize(800, 380);
    ml->arrangeLayers(false, false);
    QApplication::processEvents();
    QRect cRectFree = ag->plotWidget()->plotLayout()->canvasRect();
    std::cout << "[INFO] Unlocked free resize (800x380) -> canvasRect=" << cRectFree.width() << "x"
              << cRectFree.height() << "\n";
    CHECK_COND(cRectFree.width() > cRectFree.height() + 100,
               "Unlocked keepAspectRatio(false) did not allow free wide canvas resize");

    // Restore 1:1 square aspect ratio and verify X-axis 0..1000 right margin >= 28 px
    ml->applyAspectRatioPreset(1.0, true);
    ml->resize(520, 520);
    ml->canvasWidget()->resize(500, 460);
    ag->setScale(QwtPlot::xBottom, 0.0, 1000.0, 200.0, 5, 1, 0, false);
    plotW->updateLayout();
    ml->arrangeLayers(false, false);
    QApplication::processEvents();
    int rightMargin1000 = plotW->width() - plotW->plotLayout()->canvasRect().right();
    std::cout << "[INFO] X-axis 0..1000 right margin (Plot::width - canvasRect.right) = "
              << rightMargin1000 << " px\n";
    CHECK_COND(rightMargin1000 >= 28,
               "Right margin on 0..1000 X-axis is < 28 px; rightmost label '1000' would clip!");
    ag->setScale(QwtPlot::xBottom, 50.0, 250.0, 50.0, 5, 1, 0, false);
    plotW->replot();

    // =========================================================================
    // 10. Test Workspace Management (Project Explorer Filtering & TabbedView Toggle)
    // =========================================================================
    mw->setTabbedWorkspaceView(true);
    CHECK_COND(mw->isTabbedWorkspaceView(), "setTabbedWorkspaceView(true) did not enable TabbedView");
    mw->setTabbedWorkspaceView(false);
    CHECK_COND(!mw->isTabbedWorkspaceView(), "setTabbedWorkspaceView(false) did not restore SubWindowView");

    mw->setExplorerSearchText("PeakData");
    mw->setExplorerTypeFilter(1); // Tables only
    mw->setExplorerTypeFilter(0); // All windows
    mw->setExplorerSearchText("");

    // =========================================================================
    // 11. Test Japanese / English GUI Language Switcher (日本語 / English)
    // =========================================================================
    mw->switchToLanguage("ja");
    QString jaFileMenu = QApplication::translate("ApplicationWindow", "&File");
    QString jaAnalysisMenu = QApplication::translate("ApplicationWindow", "&Analysis");
    std::cout << "[INFO] Switched to 'ja': appLanguage=" << mw->appLanguage.toStdString()
              << ", tr(&File)=" << jaFileMenu.toUtf8().constData()
              << ", tr(&Analysis)=" << jaAnalysisMenu.toUtf8().constData() << "\n";
    CHECK_COND(mw->appLanguage == "ja"
                       && jaFileMenu == QString::fromUtf8("ファイル(&F)")
                       && jaAnalysisMenu == QString::fromUtf8("解析(&A)"),
               "Japanese GUI translation was not activated by switchToLanguage(\"ja\")");

    mw->switchToLanguage("en");
    QString enFileMenu = QApplication::translate("ApplicationWindow", "&File");
    std::cout << "[INFO] Switched to 'en': appLanguage=" << mw->appLanguage.toStdString()
              << ", tr(&File)=" << enFileMenu.toStdString() << "\n";
    CHECK_COND(mw->appLanguage == "en" && enFileMenu == "&File",
               "English GUI strings were not restored by switchToLanguage(\"en\")");

    mw->savedProject();

    // =========================================================================
    // 12. Test EMF Vector Rendering & OLE Storage Round-Trip + Clipboard
    // =========================================================================
    OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml, nullptr);
    CHECK_COND(!rg.emfBytes.isEmpty(), "EMF bytes are empty");
    CHECK_COND(!rg.dibBytes.isEmpty(), "DIB bytes are empty");
    CHECK_COND(rg.extentHm.cx > 1000 && rg.extentHm.cy > 1000, "Invalid HIMETRIC extent");

    HENHMETAFILE hemf = SciDAVisEmf::createHemfFromBytes(rg.emfBytes);
    CHECK_COND(hemf != nullptr, "createHemfFromBytes failed");
    ENHMETAHEADER emh{};
    emh.nSize = sizeof(emh);
    CHECK_COND(GetEnhMetaFileHeader(hemf, sizeof(emh), &emh) > 0, "GetEnhMetaFileHeader failed");

    EmfRecordStats stats{};
    EnumEnhMetaFile(nullptr, hemf, enumEmfProc, &stats, nullptr);
    DeleteEnhMetaFile(hemf);

    std::cout << "[INFO] EMF size=" << rg.emfBytes.size() << " bytes, totalRecords=" << stats.totalRecords
              << ", vectorRecords=" << stats.vectorPathOrLineRecords
              << ", textRecords=" << stats.textRecords
              << ", bitmapRecords=" << stats.bitmapRecords
              << ", extentHm=(" << rg.extentHm.cx << ", " << rg.extentHm.cy << ")\n";

    CHECK_COND(stats.totalRecords > 20, "Too few EMF records");
    CHECK_COND(stats.vectorPathOrLineRecords > 5, "Expected vector path/line records in EMF");
    CHECK_COND(stats.textRecords > 0, "Expected vector text records in EMF");

    SciDAVisOle::OleGraphPayload payload;
    payload.projectBytes = mw->saveProjectToByteArray();
    payload.targetWindow = ml->name();
    payload.targetLayer = 0;
    payload.emfBytes = rg.emfBytes;
    payload.dibBytes = rg.dibBytes;
    payload.extentHm = rg.extentHm;
    payload.pixelSize = rg.pixelSize;

    CHECK_COND(!payload.projectBytes.isEmpty(), "saveProjectToByteArray returned empty");

    ComPtr<IOleObject> oleObj;
    HRESULT hr = SciDAVisOle::createGraphObject(payload, IID_PPV_ARGS(&oleObj));
    CHECK_COND(SUCCEEDED(hr) && oleObj, "createGraphObject failed");

    ComPtr<IPersistStorage> persistStg;
    CHECK_COND(SUCCEEDED(oleObj.As(&persistStg)), "QueryInterface(IPersistStorage) failed");

    ComPtr<IViewObject2> viewObj;
    CHECK_COND(SUCCEEDED(oleObj.As(&viewObj)), "QueryInterface(IViewObject2) failed");

    SIZEL ext{};
    CHECK_COND(SUCCEEDED(viewObj->GetExtent(DVASPECT_CONTENT, -1, nullptr, &ext)), "IViewObject2::GetExtent failed");
    CHECK_COND(ext.cx == rg.extentHm.cx && ext.cy == rg.extentHm.cy, "Extent mismatch");

    // Save to compound file storage
    QString stgPath = QDir::tempPath() + "/scidavis_ole_test.stg";
    QFile::remove(stgPath);
    ComPtr<IStorage> stg;
    hr = StgCreateStorageEx( reinterpret_cast<const wchar_t *>(stgPath.utf16()),
                             STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                             STGFMT_STORAGE, 0, nullptr, nullptr, IID_PPV_ARGS(&stg) );
    CHECK_COND(SUCCEEDED(hr) && stg, "StgCreateStorageEx failed");

    CHECK_COND(SUCCEEDED(persistStg->Save(stg.Get(), FALSE)), "IPersistStorage::Save failed");
    CHECK_COND(SUCCEEDED(persistStg->SaveCompleted(stg.Get())), "IPersistStorage::SaveCompleted failed"); persistStg->HandsOffStorage();
    stg.Reset();

    // Reopen storage and load into a blank SciDAVisGraphObject
    hr = StgOpenStorageEx( reinterpret_cast<const wchar_t *>(stgPath.utf16()),
                           STGM_READ | STGM_SHARE_EXCLUSIVE,
                           STGFMT_STORAGE, 0, nullptr, nullptr, IID_PPV_ARGS(&stg) );
    CHECK_COND(SUCCEEDED(hr) && stg, "StgOpenStorageEx failed");

    SciDAVisOle::OleGraphPayload blankPayload;
    ComPtr<IPersistStorage> loadedPersist;
    CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(blankPayload, IID_PPV_ARGS(&loadedPersist))),
               "createGraphObject(blank) failed");
    CHECK_COND(SUCCEEDED(loadedPersist->Load(stg.Get())), "IPersistStorage::Load failed"); loadedPersist->HandsOffStorage();

    ComPtr<IDataObject> loadedData;
    CHECK_COND(SUCCEEDED(loadedPersist.As(&loadedData)), "QueryInterface(IDataObject) failed");

    FORMATETC fmtEmf{ CF_ENHMETAFILE, nullptr, DVASPECT_CONTENT, -1, TYMED_ENHMF };
    STGMEDIUM medEmf{};
    CHECK_COND(SUCCEEDED(loadedData->GetData(&fmtEmf, &medEmf)), "GetData(CF_ENHMETAFILE) after Load failed");
    CHECK_COND(medEmf.tymed == TYMED_ENHMF && medEmf.hEnhMetaFile != nullptr, "Invalid HENHMETAFILE from loaded object");
    ReleaseStgMedium(&medEmf);

    stg.Reset();
    QFile::remove(stgPath);

    // Test Clipboard Copy via MultiLayer::copyAllLayers()
    ml->copyAllLayers();

    ComPtr<IDataObject> clipData;
    for (int k = 0; k < 40; ++k) { QCoreApplication::processEvents(); hr = OleGetClipboard(&clipData); if (SUCCEEDED(hr) && clipData) break; Sleep(25); } if (FAILED(hr)) { std::cerr << "OleGetClipboard hr=0x" << std::hex << hr << std::dec << "\n"; } CHECK_COND(SUCCEEDED(hr) && clipData, "OleGetClipboard failed");

    CLIPFORMAT cfEmbed = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"Embed Source"));
    CLIPFORMAT cfDesc = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"Object Descriptor"));

    FORMATETC fmtEmbed{ cfEmbed, nullptr, DVASPECT_CONTENT, -1, TYMED_ISTORAGE };
    CHECK_COND(clipData->QueryGetData(&fmtEmbed) == S_OK, "Clipboard missing Embed Source");

    FORMATETC fmtDesc{ cfDesc, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    CHECK_COND(clipData->QueryGetData(&fmtDesc) == S_OK, "Clipboard missing Object Descriptor");
    STGMEDIUM medDesc{};
    for (int k = 0; k < 40; ++k) { QCoreApplication::processEvents(); hr = clipData->GetData(&fmtDesc, &medDesc); if (SUCCEEDED(hr)) break; Sleep(25); } if (FAILED(hr)) { std::cerr << "GetData(Object Descriptor) hr=0x" << std::hex << hr << std::dec << "\n"; } CHECK_COND(SUCCEEDED(hr), "GetData(Object Descriptor) failed");
    auto *objDesc = static_cast<OBJECTDESCRIPTOR *>(GlobalLock(medDesc.hGlobal));
    CHECK_COND(objDesc != nullptr && objDesc->clsid == SciDAVisOle::graph_clsid, "Clipboard Object Descriptor CLSID mismatch!");
    GlobalUnlock(medDesc.hGlobal);
    ReleaseStgMedium(&medDesc);

    CHECK_COND(clipData->QueryGetData(&fmtEmf) == S_OK, "Clipboard missing CF_ENHMETAFILE");

    FORMATETC fmtDib{ CF_DIB, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    CHECK_COND(clipData->QueryGetData(&fmtDib) == S_OK, "Clipboard missing CF_DIB");

    clipData.Reset();

    // =========================================================================
    // 13. Interactive ROI Gadgets & Vertical Cursor (Domain 1)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 13: Interactive ROI Gadgets & Vertical Cursor...\n";
        const int nGad = 101;
        Table *gadTbl = mw->newTable("GadgetData", nGad, 3);
        CHECK_COND(gadTbl != nullptr, "Failed to create GadgetData table");
        gadTbl->setColName(0, "X");
        gadTbl->setColPlotDesignation(0, SciDAVis::X);
        gadTbl->setColName(1, "LinearRamp");
        gadTbl->setColPlotDesignation(1, SciDAVis::Y);
        gadTbl->setColName(2, "StepPulse");
        gadTbl->setColPlotDesignation(2, SciDAVis::Y);
        for (int r = 0; r < nGad; ++r) {
            double x = double(r);
            double yLin = 2.0 * x + 10.0; // exact line y = 2*x + 10
            double yStep = (x <= 50.0) ? 0.0 : ((x >= 70.0) ? 100.0 : 5.0 * (x - 50.0));
            gadTbl->setCell(r, 0, x);
            gadTbl->setCell(r, 1, yLin);
            gadTbl->setCell(r, 2, yStep);
        }
        QStringList gadCols;
        gadCols << (gadTbl->name() + "_LinearRamp") << (gadTbl->name() + "_StepPulse");
        MultiLayer *mlGad = mw->multilayerPlot(gadTbl, gadCols, Graph::LineSymbols);
        CHECK_COND(mlGad != nullptr && mlGad->activeGraph() != nullptr, "Failed to create Gadget plot");
        Graph *gGad = mlGad->activeGraph();
        gGad->setScale(QwtPlot::xBottom, 0.0, 100.0);
        gGad->setScale(QwtPlot::yLeft, -10.0, 220.0);
        mlGad->show();
        QApplication::processEvents();

        const QString cLinTitle = gadTbl->name() + "_LinearRamp";
        const QString cStepTitle = gadTbl->name() + "_StepPulse";

        // RegionStats on x in [10, 30] -> y = 30..70 (21 points, exact mean = 50.0)
        RoiGadgetTool *roi = RoiGadgetTool::attachGadget(mw, gGad, RoiGadgetTool::RegionStats);
        CHECK_COND(roi != nullptr && roi->rtti() == PlotToolInterface::RoiGadget,
                   "attachGadget(RegionStats) failed");
        roi->setTargetCurve(cLinTitle);
        roi->setRoiRect(QRectF(10.0, 0.0, 20.0, 120.0));
        std::cout << "[INFO] ROI RegionStats [10..30]: mean=" << roi->primaryMetric()
                  << ", sd=" << roi->secondaryMetric() << "\n";
        CHECK_COND(relErrWithin(roi->primaryMetric(), 50.0, 1e-6),
                   "RoiGadgetTool(RegionStats) mean mismatch on [10, 30]");

        // Move ROI rectangle to [20, 40] -> y = 50..90 (exact mean = 70.0)
        roi->setRoiRect(QRectF(20.0, 0.0, 20.0, 120.0));
        std::cout << "[INFO] ROI RegionStats moved [20..40]: mean=" << roi->primaryMetric() << "\n";
        CHECK_COND(relErrWithin(roi->primaryMetric(), 70.0, 1e-6),
                   "RoiGadgetTool(RegionStats) mean did not update after moving ROI");

        // QuickFit (FitLinear) on [10, 30] -> R^2 = 1.0, slope = 2.0
        roi->setRoiRect(QRectF(10.0, 0.0, 20.0, 120.0));
        roi->setGadgetMode(RoiGadgetTool::QuickFit);
        roi->setQuickFitModel(RoiGadgetTool::FitLinear);
        std::cout << "[INFO] ROI QuickFit(Linear): R^2=" << roi->primaryMetric()
                  << ", slope=" << roi->secondaryMetric() << "\n";
        CHECK_COND(roi->primaryMetric() > 0.999999 && relErrWithin(roi->secondaryMetric(), 2.0, 1e-5),
                   "RoiGadgetTool(QuickFit) R^2 or slope mismatch");

        // Integrate on [10, 30] -> integral_{10}^{30} (2x + 10) dx = 0.5*(30 + 70)*20 = 1000.0
        roi->setGadgetMode(RoiGadgetTool::Integrate);
        std::cout << "[INFO] ROI Integrate [10..30]: area=" << roi->primaryMetric() << "\n";
        CHECK_COND(relErrWithin(roi->primaryMetric(), 1000.0, 1e-5),
                   "RoiGadgetTool(Integrate) trapezoidal area mismatch");

        // RiseTime on StepPulse (0 -> 100 ramp over x in [50, 70] -> 10%=52, 90%=68 -> riseTime = 16.0)
        roi->setTargetCurve(cStepTitle);
        roi->setRoiRect(QRectF(40.0, -5.0, 45.0, 115.0));
        roi->setGadgetMode(RoiGadgetTool::RiseTime);
        std::cout << "[INFO] ROI RiseTime [40..85]: 10-90% riseTime=" << roi->primaryMetric() << "\n";
        CHECK_COND(relErrWithin(roi->primaryMetric(), 16.0, 1e-4),
                   "RoiGadgetTool(RiseTime) 10-90% rise time mismatch");

        // Cluster on LinearRamp for x in [10, 20], y in [25, 55] -> 11 points (x=10..20, y=30..50), meanY = 40.0
        roi->setTargetCurve(cLinTitle);
        roi->setRoiRect(QRectF(10.0, 25.0, 10.0, 30.0));
        roi->setGadgetMode(RoiGadgetTool::Cluster);
        std::cout << "[INFO] ROI Cluster [10..20]: count=" << roi->primaryMetric()
                  << ", meanY=" << roi->secondaryMetric() << "\n";
        CHECK_COND(std::fabs(roi->primaryMetric() - 11.0) < 1e-6
                           && relErrWithin(roi->secondaryMetric(), 40.0, 1e-5),
                   "RoiGadgetTool(Cluster) count or meanY mismatch");

        Table *gadOut = roi->createOutputTable("RoiClusterSummary");
        CHECK_COND(gadOut != nullptr && gadOut->numRows() >= 1,
                   "RoiGadgetTool::createOutputTable failed");

        // VerticalCursorTool synchronized readout & exact linear interpolation
        VerticalCursorTool *vcur = VerticalCursorTool::attachCursor(mw, gGad, true);
        CHECK_COND(vcur != nullptr && vcur->rtti() == PlotToolInterface::VerticalCursor,
                   "VerticalCursorTool::attachCursor failed");
        vcur->setCursorX(60.5);
        QVector<QPair<QString, double>> ivals = vcur->interpolatedValues();
        std::cout << "[INFO] VerticalCursor at X=60.5: curves=" << ivals.size() << "\n";
        CHECK_COND(ivals.size() >= 2, "VerticalCursorTool::interpolatedValues missing curves");
        bool foundLin = false, foundStep = false;
        for (const auto &pr : ivals) {
            if (pr.first.contains("LinearRamp")) {
                foundLin = relErrWithin(pr.second, 2.0 * 60.5 + 10.0, 1e-6); // 131.0
            } else if (pr.first.contains("StepPulse")) {
                foundStep = relErrWithin(pr.second, 5.0 * (60.5 - 50.0), 1e-6); // 52.5
            }
        }
        CHECK_COND(foundLin && foundStep,
                   "VerticalCursorTool::interpolatedValues exact Y readout mismatch");
        RoiGadgetTool::clearGadget(gGad);
        CHECK_COND(gGad->activeTool() == nullptr, "RoiGadgetTool::clearGadget did not detach tool");
    }

    // =========================================================================
    // 14. Non-Destructive Data Point Masking (Domain 1 / Filter Integration)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 14: Non-Destructive Data Point Masking...\n";
        const int nMask = 21;
        Table *maskTbl = mw->newTable("MaskData", nMask, 2);
        maskTbl->setColName(0, "X");
        maskTbl->setColPlotDesignation(0, SciDAVis::X);
        maskTbl->setColName(1, "Y");
        maskTbl->setColPlotDesignation(1, SciDAVis::Y);
        for (int r = 0; r < nMask; ++r) {
            double x = double(r);
            maskTbl->setCell(r, 0, x);
            maskTbl->setCell(r, 1, 3.5 * x - 4.0);
        }
        // Inject extreme outlier y = +9999 at row 10 (x = 10)
        maskTbl->setCell(10, 1, 9999.0);

        MultiLayer *mlMask = mw->multilayerPlot(
                maskTbl, QStringList() << (maskTbl->name() + "_Y"), Graph::Scatter);
        CHECK_COND(mlMask != nullptr && mlMask->activeGraph() != nullptr,
                   "Failed to create MaskData graph");
        Graph *gMask = mlMask->activeGraph();
        DataCurve *dcMask = dynamic_cast<DataCurve *>(gMask->curve(0));
        CHECK_COND(dcMask != nullptr, "MaskData curve 0 is not a DataCurve");

        // Unmasked LinearFit should be severely distorted by +9999 outlier
        {
            LinearFit lfRaw(mw, gMask, dcMask->title().text());
            lfRaw.fit();
            std::cout << "[INFO] Unmasked outlier LinearFit: R^2=" << lfRaw.rSquare()
                      << ", slope=" << lfRaw.results()[1] << "\n";
            CHECK_COND(lfRaw.rSquare() < 0.25, "Outlier +9999 did not distort unmasked LinearFit");
        }

        // Mask the outlier non-destructively via DataCurve::maskPoint
        dcMask->maskPoint(10, true);
        CHECK_COND(dcMask->isPointMasked(10) && dcMask->maskedPointCount() == 1,
                   "DataCurve::maskPoint(10, true) did not mark point as masked");
        CHECK_COND(std::fabs(maskTbl->cell(10, 1) - 9999.0) < 1e-9,
                   "Masking destructively altered Table cell value instead of preserving 9999!");

        LinearFit lfMasked(mw, gMask, dcMask->title().text());
        lfMasked.fit();
        std::cout << "[INFO] Masked outlier LinearFit: R^2=" << lfMasked.rSquare()
                  << ", slope=" << lfMasked.results()[1]
                  << ", intercept=" << lfMasked.results()[0]
                  << ", tableCell(10,1)=" << maskTbl->cell(10, 1) << "\n";
        CHECK_COND(lfMasked.rSquare() > 0.999999
                           && relErrWithin(lfMasked.results()[1], 3.5, 1e-6)
                           && relErrWithin(lfMasked.results()[0], -4.0, 1e-6)
                           && std::fabs(maskTbl->cell(10, 1) - 9999.0) < 1e-9,
                   "LinearFit on masked curve failed to recover exact slope/R^2 while keeping cell=9999");

        dcMask->clearMasks();
        CHECK_COND(!dcMask->isPointMasked(10) && dcMask->maskedPointCount() == 0
                           && std::fabs(maskTbl->cell(10, 1) - 9999.0) < 1e-9,
                   "DataCurve::clearMasks() failed or modified underlying Table cell");
    }

    // =========================================================================
    // 15. Advanced Curve Fitting (Domain 2)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 15: Advanced Curve Fitting...\n";

        // 15a. Parameter Fixing & Box Bounds on NonLinearFit & MultiPeakFit
        {
            Table *conTbl = mw->newTable("ConstrainedFitData", 41, 2);
            conTbl->setColName(0, "X");
            conTbl->setColPlotDesignation(0, SciDAVis::X);
            conTbl->setColName(1, "Y");
            conTbl->setColPlotDesignation(1, SciDAVis::Y);
            for (int r = 0; r < 41; ++r) {
                double x = r * 1.5;
                // True unconstrained: y0 = 5, A = 100, t1 = 12
                conTbl->setCell(r, 0, x);
                conTbl->setCell(r, 1, 5.0 + 100.0 * std::exp(-x / 12.0));
            }
            MultiLayer *mlCon = mw->multilayerPlot(
                    conTbl, QStringList() << (conTbl->name() + "_Y"), Graph::Scatter);
            Graph *gCon = mlCon->activeGraph();
            const QString cTitle = conTbl->name() + "_Y";

            NonLinearFit nlf(mw, gCon, cTitle);
            nlf.setParametersList(QStringList() << "y0" << "A" << "t1");
            nlf.setFormula("y0 + A * exp(-x / t1)");
            nlf.setInitialGuess(0, 8.0);
            nlf.setInitialGuess(1, 95.0);
            nlf.setInitialGuess(2, 18.0);
            nlf.setParameterFixed(0, true);              // Fix y0 = 8.0
            nlf.setParameterBounds(2, 15.0, 25.0);       // Box-bound t1 in [15, 25]
            nlf.fit();
            std::cout << "[INFO] Constrained NonLinearFit: y0(fixed=8)=" << nlf.results()[0]
                      << ", A=" << nlf.results()[1] << ", t1(bounds 15..25)=" << nlf.results()[2] << "\n";
            CHECK_COND(nlf.isParameterFixed(0) && std::fabs(nlf.results()[0] - 8.0) < 1e-9,
                       "NonLinearFit::setParameterFixed did not hold y0 == 8.0");
            CHECK_COND(nlf.results()[2] >= 14.999999 && nlf.results()[2] <= 25.000001,
                       "NonLinearFit::setParameterBounds [15, 25] violated");

            // MultiPeakFit fixing & bounds (Reuse PeakData curve 0: Gauss peaks at xc1=30, xc2=68, y0=5)
            MultiLayer *mlMpfCon = mw->multilayerPlot(
                    peakTable, QStringList() << (peakTable->name() + "_Gauss2"), Graph::Scatter);
            Graph *gMpfCon = mlMpfCon->activeGraph();
            MultiPeakFit mpfCon(mw, gMpfCon, MultiPeakFit::Gauss, 2);
            CHECK_COND(mpfCon.setDataFromCurve(peakTable->name() + "_Gauss2"),
                       "mpfCon.setDataFromCurve failed");
            mpfCon.autoDetectPeaks(2);
            mpfCon.setInitialGuess(6, 7.5);              // y0 index = 3*2 = 6
            mpfCon.setParameterFixed(6, true);           // Fix y0 = 7.5
            mpfCon.setInitialGuess(1, 32.5);             // xc1 index = 1
            mpfCon.setParameterBounds(1, 31.5, 35.0);    // Bound xc1 >= 31.5 (true is 30.0)
            mpfCon.fit();
            std::cout << "[INFO] Constrained MultiPeakFit: y0(fixed=7.5)=" << mpfCon.results()[6]
                      << ", xc1(bounds 31.5..35)=" << mpfCon.results()[1] << "\n";
            CHECK_COND(std::fabs(mpfCon.results()[6] - 7.5) < 1e-8
                               && mpfCon.results()[1] >= 31.499999
                               && mpfCon.results()[1] <= 35.000001,
                       "MultiPeakFit parameter fixing or box bounds violated");
        }

        // 15b. Confidence/Prediction Bands + AIC/BIC + Fit::compareModels
        {
            Table *bandTbl = mw->newTable("BandFitData", 31, 3);
            bandTbl->setColName(0, "X");
            bandTbl->setColPlotDesignation(0, SciDAVis::X);
            bandTbl->setColName(1, "QuadNoisy");
            bandTbl->setColPlotDesignation(1, SciDAVis::Y);
            bandTbl->setColName(2, "ExpClean");
            bandTbl->setColPlotDesignation(2, SciDAVis::Y);
            for (int r = 0; r < 31; ++r) {
                double x = r * 0.5; // 0..15
                double jitter = 0.35 * std::sin(1.7 * r + 0.3);
                bandTbl->setCell(r, 0, x);
                bandTbl->setCell(r, 1, 2.0 + 1.5 * x + 0.4 * x * x + jitter);
                bandTbl->setCell(r, 2, 5.0 + 80.0 * std::exp(-x / 4.0) + 0.05 * jitter);
            }
            MultiLayer *mlBand = mw->multilayerPlot(
                    bandTbl, QStringList() << (bandTbl->name() + "_QuadNoisy"), Graph::Scatter);
            Graph *gBand = mlBand->activeGraph();
            QString qCurve = bandTbl->name() + "_QuadNoisy";

            PolynomialFit pf(mw, gBand, qCurve, 2, false);
            pf.enableConfidenceBand(true, 0.95);
            pf.enablePredictionBand(true, 0.95);
            pf.fit();

            QwtPlotCurve *cConfUp = gBand->curve(qCurve + "_ConfUpper");
            QwtPlotCurve *cConfLo = gBand->curve(qCurve + "_ConfLower");
            QwtPlotCurve *cPredUp = gBand->curve(qCurve + "_PredUpper");
            QwtPlotCurve *cPredLo = gBand->curve(qCurve + "_PredLower");
            CHECK_COND(cConfUp && cConfLo && cPredUp && cPredLo,
                       "Confidence or Prediction band curves were not generated on Graph");
            int midIdx = cConfUp->dataSize() / 2;
            double confSpan = cConfUp->y(midIdx) - cConfLo->y(midIdx);
            double predSpan = cPredUp->y(midIdx) - cPredLo->y(midIdx);
            std::cout << "[INFO] Poly2 Bands & Diagnostics: confSpan=" << confSpan
                      << ", predSpan=" << predSpan << ", AIC=" << pf.aic()
                      << ", BIC=" << pf.bic() << ", F=" << pf.anovaFValue() << "\n";
            CHECK_COND(confSpan > 0.0 && predSpan > confSpan,
                       "Prediction band must be strictly wider than Confidence band");
            CHECK_COND(std::isfinite(pf.aic()) && std::isfinite(pf.bic())
                               && pf.anovaFValue() > 100.0 && pf.anovaPValue() < 1e-6,
                       "AIC/BIC/ANOVA diagnostics invalid");

            MultiLayer *mlCmp = mw->multilayerPlot(
                    bandTbl, QStringList() << (bandTbl->name() + "_ExpClean"), Graph::Scatter);
            Graph *gCmp = mlCmp->activeGraph();
            Table *cmpTbl = Fit::compareModels(
                    mw, gCmp, bandTbl->name() + "_ExpClean",
                    QStringList() << "ExpDec1" << "GaussAmp" << "Boltzmann");
            CHECK_COND(cmpTbl != nullptr && cmpTbl->numRows() >= 3,
                       "Fit::compareModels failed to produce ranked table");
            std::cout << "[INFO] compareModels Rank #1 = "
                      << cmpTbl->column(0)->textAt(0).toStdString()
                      << ", AIC=" << cmpTbl->cell(0, 5)
                      << ", Weight=" << cmpTbl->cell(0, 9) << "\n";
            CHECK_COND(cmpTbl->column(0)->textAt(0).contains("ExpDec1", Qt::CaseInsensitive),
                       "Fit::compareModels did not rank true ExpDec1 model #1 by AIC");
        }

        // 15c. York Linear Fit with X & Y Errors + Apparent Linear Fit on Log10 Axes
        {
            Table *yorkTbl = mw->newTable("YorkFitData", 12, 4);
            yorkTbl->setColName(0, "X");
            yorkTbl->setColPlotDesignation(0, SciDAVis::X);
            yorkTbl->setColName(1, "Y");
            yorkTbl->setColPlotDesignation(1, SciDAVis::Y);
            yorkTbl->setColName(2, "sX");
            yorkTbl->setColPlotDesignation(2, SciDAVis::xErr);
            yorkTbl->setColName(3, "sY");
            yorkTbl->setColPlotDesignation(3, SciDAVis::yErr);
            for (int r = 0; r < 12; ++r) {
                double x = 1.0 + r * 2.0;
                double y = 2.5 * x + 3.0; // exact slope = 2.5, intercept = 3.0
                double sx = 0.05, sy = 0.08;
                if (r == 11) {
                    // Huge uncertainty noisy point that pulls unweighted OLS off course
                    y += 35.0;
                    sx = 80.0;
                    sy = 80.0;
                }
                yorkTbl->setCell(r, 0, x);
                yorkTbl->setCell(r, 1, y);
                yorkTbl->setCell(r, 2, sx);
                yorkTbl->setCell(r, 3, sy);
            }
            MultiLayer *mlYork = mw->multilayerPlot(
                    yorkTbl, QStringList() << (yorkTbl->name() + "_Y"), Graph::Scatter);
            Graph *gYork = mlYork->activeGraph();
            LinearFit lfYork(mw, gYork, yorkTbl->name() + "_Y");
            CHECK_COND(lfYork.fitWithXYErrors("sX", "sY"), "LinearFit::fitWithXYErrors returned false");
            std::cout << "[INFO] York Regression: slope=" << lfYork.yorkSlope()
                      << " +/- " << lfYork.yorkSlopeError()
                      << ", intercept=" << lfYork.yorkIntercept() << "\n";
            CHECK_COND(relErrWithin(lfYork.yorkSlope(), 2.5, 0.01)
                               && relErrWithin(lfYork.yorkIntercept(), 3.0, 0.02)
                               && lfYork.yorkSlopeError() > 0.0,
                       "York regression failed to recover exact weighted slope & intercept");

            // Apparent Fit on Log10-Log10 axes: y = 10 * x^2 -> log10(y) = 1.0 + 2.0 * log10(x)
            const double logXs[7] = { 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0 };
            Table *appTbl = mw->newTable("ApparentLogData", 7, 2);
            appTbl->setColName(0, "X");
            appTbl->setColPlotDesignation(0, SciDAVis::X);
            appTbl->setColName(1, "Y");
            appTbl->setColPlotDesignation(1, SciDAVis::Y);
            for (int r = 0; r < 7; ++r) {
                appTbl->setCell(r, 0, logXs[r]);
                appTbl->setCell(r, 1, 10.0 * logXs[r] * logXs[r]);
            }
            MultiLayer *mlApp = mw->multilayerPlot(
                    appTbl, QStringList() << (appTbl->name() + "_Y"), Graph::Scatter);
            Graph *gApp = mlApp->activeGraph();
            gApp->setScale(QwtPlot::xBottom, 1.0, 100.0, 0.0, 5, 5, 1, false);
            gApp->setScale(QwtPlot::yLeft, 10.0, 100000.0, 0.0, 5, 5, 1, false);
            LinearFit lfApp(mw, gApp, appTbl->name() + "_Y");
            lfApp.setApparentFit(true);
            lfApp.fit();
            std::cout << "[INFO] Apparent Log10 LinearFit: logInt=" << lfApp.results()[0]
                      << ", logSlope=" << lfApp.results()[1] << ", R^2=" << lfApp.rSquare() << "\n";
            CHECK_COND(relErrWithin(lfApp.results()[0], 1.0, 1e-4)
                               && relErrWithin(lfApp.results()[1], 2.0, 1e-4)
                               && lfApp.rSquare() > 0.99999,
                       "Apparent LinearFit on Log10 axes failed to fit in log-transformed space");
        }

        // 15d. Global Curve Fit (shared decay constant) & Implicit ODR Fit (ellipse x^2/a^2 + y^2/b^2 - 1 = 0)
        {
            Table *globTbl = mw->newTable("GlobalFitData", 31, 3);
            globTbl->setColName(0, "X");
            globTbl->setColPlotDesignation(0, SciDAVis::X);
            globTbl->setColName(1, "Decay1");
            globTbl->setColPlotDesignation(1, SciDAVis::Y);
            globTbl->setColName(2, "Decay2");
            globTbl->setColPlotDesignation(2, SciDAVis::Y);
            for (int r = 0; r < 31; ++r) {
                double x = r * 2.0; // 0..60, shared tau = 18.0
                globTbl->setCell(r, 0, x);
                globTbl->setCell(r, 1, 2.0 + 100.0 * std::exp(-x / 18.0));
                globTbl->setCell(r, 2, 10.0 + 250.0 * std::exp(-x / 18.0));
            }
            QStringList globCurves;
            globCurves << (globTbl->name() + "_Decay1") << (globTbl->name() + "_Decay2");
            MultiLayer *mlGlob = mw->multilayerPlot(globTbl, globCurves, Graph::Scatter);
            Graph *gGlob = mlGlob->activeGraph();

            NonLinearFit nlGlob(mw, gGlob, globCurves[0]);
            nlGlob.setParametersList(QStringList() << "y0" << "A" << "t1");
            nlGlob.setFormula("y0 + A * exp(-x / t1)");
            nlGlob.setInitialGuess(0, 5.0);
            nlGlob.setInitialGuess(1, 150.0);
            nlGlob.setInitialGuess(2, 14.0);
            Table *globSummary = nullptr;
            QVector<bool> sharedFlags;
            sharedFlags << false << false << true; // share t1 across both curves
            CHECK_COND(nlGlob.fitGlobal(globCurves, sharedFlags, &globSummary) && globSummary != nullptr,
                       "NonLinearFit::fitGlobal failed");
            double tauCurve1 = globSummary->cell(2, 3);
            double tauCurve2 = globSummary->cell(5, 3);
            double ampCurve1 = globSummary->cell(1, 3);
            double ampCurve2 = globSummary->cell(4, 3);
            std::cout << "[INFO] Global Fit: A1=" << ampCurve1 << ", A2=" << ampCurve2
                      << ", shared t1=(" << tauCurve1 << ", " << tauCurve2 << "), R^2="
                      << nlGlob.rSquare() << "\n";
            CHECK_COND(std::fabs(tauCurve1 - tauCurve2) < 1e-12
                               && relErrWithin(tauCurve1, 18.0, 0.005)
                               && relErrWithin(ampCurve1, 100.0, 0.005)
                               && relErrWithin(ampCurve2, 250.0, 0.005),
                       "NonLinearFit::fitGlobal did not recover shared decay constant & per-curve amplitudes");

            // Implicit ODR Fit: ellipse x^2/a^2 + y^2/b^2 - 1 = 0 with true a = 6.0, b = 3.5
            const int nEll = 36;
            Table *odrTbl = mw->newTable("ImplicitEllipseData", nEll, 2);
            odrTbl->setColName(0, "X");
            odrTbl->setColPlotDesignation(0, SciDAVis::X);
            odrTbl->setColName(1, "Y");
            odrTbl->setColPlotDesignation(1, SciDAVis::Y);
            for (int r = 0; r < nEll; ++r) {
                double th = 2.0 * M_PI * double(r) / double(nEll);
                odrTbl->setCell(r, 0, 6.0 * std::cos(th));
                odrTbl->setCell(r, 1, 3.5 * std::sin(th));
            }
            MultiLayer *mlOdr = mw->multilayerPlot(
                    odrTbl, QStringList() << (odrTbl->name() + "_Y"), Graph::Scatter);
            Graph *gOdr = mlOdr->activeGraph();
            NonLinearFit nlOdr(mw, gOdr, odrTbl->name() + "_Y");
            CHECK_COND(nlOdr.fitImplicitODR("x^2/(a^2) + y^2/(b^2) - 1",
                                            QStringList() << "a" << "b",
                                            QVector<double>() << 4.8 << 2.6),
                       "NonLinearFit::fitImplicitODR returned false");
            double aFit = std::fabs(nlOdr.results()[0]);
            double bFit = std::fabs(nlOdr.results()[1]);
            std::cout << "[INFO] Implicit ODR Ellipse Fit: a=" << aFit << " (expected 6.0), b="
                      << bFit << " (expected 3.5)\n";
            CHECK_COND(relErrWithin(aFit, 6.0, 0.01) && relErrWithin(bFit, 3.5, 0.01),
                       "fitImplicitODR failed to recover ellipse semi-axes a=6.0, b=3.5 within <1%");
        }
    }

    // =========================================================================
    // 16. Advanced Peak Analyzer (Domain 3)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 16: Advanced Peak Analyzer...\n";
        const int nSpec = 201;
        Table *paTbl = mw->newTable("AdvPeakData", nSpec, 5);
        paTbl->setColName(0, "X");
        paTbl->setColPlotDesignation(0, SciDAVis::X);
        paTbl->setColName(1, "SplineBaseSpec");
        paTbl->setColPlotDesignation(1, SciDAVis::Y);
        paTbl->setColName(2, "ShoulderSpec");
        paTbl->setColPlotDesignation(2, SciDAVis::Y);
        paTbl->setColName(3, "DoubletMixed");
        paTbl->setColPlotDesignation(3, SciDAVis::Y);
        paTbl->setColName(4, "BatchSpec2");
        paTbl->setColPlotDesignation(4, SciDAVis::Y);

        auto cubicBase = [](double x) {
            return 5.0 + 0.12 * x + 0.0015 * x * x - 0.000012 * x * x * x;
        };
        auto gaussArea = [](double x, double A, double xc, double w) {
            return std::sqrt(2.0 / M_PI) * (A / w) * std::exp(-2.0 * (x - xc) * (x - xc) / (w * w));
        };
        auto lorentzArea = [](double x, double A, double xc, double fwhm) {
            return (2.0 * A / M_PI) * fwhm / (4.0 * (x - xc) * (x - xc) + fwhm * fwhm);
        };

        // Mixed-profile spin-orbit doublet: Peak 0 = Gauss(A=300, xc=38, w=5 -> FWHM=5*sqrt(2*ln2)=5.88705),
        // Peak 1 = Lorentz(Area = 0.5 * 300 = 150, xc = 38 + 14 = 52, shared FWHM = 5.8870501)
        const double fwhmShared = 5.0 * std::sqrt(2.0 * std::log(2.0));
        for (int r = 0; r < nSpec; ++r) {
            double x = r * 0.5; // 0..100
            paTbl->setCell(r, 0, x);
            paTbl->setCell(r, 1, cubicBase(x) + gaussArea(x, 220.0, 30.0, 5.0)
                                 + gaussArea(x, 320.0, 70.0, 6.0));
            // Closely overlapping primary peak (xc=45, H=100) + hidden shoulder (xc=52.5, H=42)
            double yShoulder = 100.0 * std::exp(-0.5 * std::pow((x - 45.0) / 3.5, 2.0))
                               + 42.0 * std::exp(-0.5 * std::pow((x - 52.5) / 2.6, 2.0));
            paTbl->setCell(r, 2, yShoulder);
            paTbl->setCell(r, 3, 2.0 + gaussArea(x, 300.0, 38.0, 5.0)
                                 + lorentzArea(x, 150.0, 52.0, fwhmShared));
            paTbl->setCell(r, 4, 3.0 + 0.05 * x + gaussArea(x, 180.0, 28.0, 5.0)
                                 + gaussArea(x, 260.0, 66.0, 5.5));
        }

        // 16a. BaselineAnchors cubic spline baseline subtraction & fit
        MultiLayer *mlPA1 = mw->multilayerPlot(
                paTbl, QStringList() << (paTbl->name() + "_SplineBaseSpec"), Graph::Line);
        Graph *gPA1 = mlPA1->activeGraph();
        MultiPeakFit mpfSpline(mw, gPA1, MultiPeakFit::Gauss, 2);
        mpfSpline.setDataFromCurve(paTbl->name() + "_SplineBaseSpec");
        mpfSpline.setBaselineMode(MultiPeakFit::BaselineAnchors);
        QVector<QPointF> anchors;
        for (double ax : { 0.0, 10.0, 50.0, 90.0, 100.0 })
            anchors << QPointF(ax, cubicBase(ax));
        mpfSpline.setBaselineAnchors(anchors, true);
        std::vector<double> spBase = mpfSpline.computeBaseline();
        CHECK_COND(spBase.size() == size_t(nSpec)
                           && relErrWithin(spBase[60], cubicBase(30.0), 0.005)
                           && relErrWithin(spBase[140], cubicBase(70.0), 0.005),
                   "BaselineAnchors cubic spline baseline deviated from exact cubic polynomial");
        CHECK_COND(mpfSpline.subtractBaselineFromData(), "BaselineAnchors subtractBaselineFromData failed");
        mpfSpline.fit();
        std::cout << "[INFO] BaselineAnchors Subtracted Fit: R^2=" << mpfSpline.rSquare()
                  << ", A1=" << mpfSpline.results()[0] << ", A2=" << mpfSpline.results()[3] << "\n";
        CHECK_COND(mpfSpline.rSquare() > 0.9999 && relErrWithin(mpfSpline.results()[0], 220.0, 0.02)
                           && relErrWithin(mpfSpline.results()[3], 320.0, 0.02),
                   "BaselineAnchors subtracted peak fit failed");

        // 16b. 2nd-Derivative Hidden Shoulder Peak Detection
        MultiLayer *mlPA2 = mw->multilayerPlot(
                paTbl, QStringList() << (paTbl->name() + "_ShoulderSpec"), Graph::Line);
        Graph *gPA2 = mlPA2->activeGraph();
        MultiPeakFit mpfD2(mw, gPA2, MultiPeakFit::Gauss, 2);
        mpfD2.setDataFromCurve(paTbl->name() + "_ShoulderSpec");
        QVector<double> centersD2 = mpfD2.detectPeaksSecondDerivative(11, 0.10);
        std::cout << "[INFO] 2nd-Derivative Detected Centers count=" << centersD2.size();
        for (double c : centersD2)
            std::cout << " " << c;
        std::cout << "\n";
        CHECK_COND(centersD2.size() == 2 && std::fabs(centersD2[0] - 45.0) < 1.2
                           && std::fabs(centersD2[1] - 52.5) < 1.5,
                   "detectPeaksSecondDerivative failed to resolve overlapping shoulder peak");

        // 16c. Mixed Peak Profiles (Gauss + Lorentz) & Doublet Ratio/Offset Constraint
        MultiLayer *mlPA3 = mw->multilayerPlot(
                paTbl, QStringList() << (paTbl->name() + "_DoubletMixed"), Graph::Line);
        Graph *gPA3 = mlPA3->activeGraph();
        MultiPeakFit mpfDbl(mw, gPA3, MultiPeakFit::Gauss, 2);
        mpfDbl.setDataFromCurve(paTbl->name() + "_DoubletMixed");
        QVector<MultiPeakFit::PeakProfile> mixedProfs;
        mixedProfs << MultiPeakFit::Gauss << MultiPeakFit::Lorentz;
        mpfDbl.setPerPeakProfiles(mixedProfs);
        mpfDbl.setInitialGuess(0, 270.0);
        mpfDbl.setInitialGuess(1, 37.5);
        mpfDbl.setInitialGuess(2, 4.6);
        mpfDbl.setInitialGuess(6, 1.5);
        mpfDbl.addPeakDoubletConstraint(1, 0, 0.5, 14.0, true);
        mpfDbl.fit();
        auto dblRows = mpfDbl.computePeakSummary(mpfDbl.results());
        std::cout << "[INFO] Mixed-Profile Doublet Fit: R^2=" << mpfDbl.rSquare()
                  << ", Peak1(xc=" << dblRows[0].center << ", Area=" << dblRows[0].area << ")"
                  << ", Peak2(xc=" << dblRows[1].center << ", Area=" << dblRows[1].area << ")\n";
        CHECK_COND(mpfDbl.rSquare() > 0.9999
                           && relErrWithin(dblRows[0].area, 300.0, 0.01)
                           && relErrWithin(dblRows[1].area, 150.0, 0.01)
                           && std::fabs((dblRows[1].center - dblRows[0].center) - 14.0) < 1e-6,
                   "Mixed-profile doublet constraint failed");

        // 16d. Batch Peak Analysis across multiple curves
        QStringList batchCurves;
        batchCurves << (paTbl->name() + "_SplineBaseSpec") << (paTbl->name() + "_BatchSpec2");
        MultiLayer *mlBatch = mw->multilayerPlot(paTbl, batchCurves, Graph::Line);
        Table *batchTbl = MultiPeakFit::batchAnalyzeCurves(
                mw, mlBatch->activeGraph(), batchCurves, MultiPeakFit::Gauss,
                MultiPeakFit::BaselineALS);
        CHECK_COND(batchTbl != nullptr && batchTbl->numRows() >= 4,
                   "MultiPeakFit::batchAnalyzeCurves failed to analyze all peaks across curves");
        std::cout << "[INFO] Batch Peak Analysis rows=" << batchTbl->numRows()
                  << ", row0 R^2=" << batchTbl->cell(0, 8) << "\n";
        CHECK_COND(batchTbl->cell(0, 8) > 0.99 && batchTbl->cell(batchTbl->numRows() - 1, 8) > 0.99,
                   "Batch Peak Analysis produced poor R^2");
    }

    // =========================================================================
    // 17. Curve Math & Advanced Signal Processing (Domains 4 & 5)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 17: Curve Math & Advanced Signal Processing...\n";

        // 17a. CurveMathTools (simpleCurveMath on mismatched grids, averageMultipleCurves,
        //      normalizeCurve, findCurveIntersections, polygonArea)
        {
            Table *cmTblA = mw->newTable("CurveMathA", 11, 2);
            cmTblA->setColName(0, "XA");
            cmTblA->setColPlotDesignation(0, SciDAVis::X);
            cmTblA->setColName(1, "YA");
            cmTblA->setColPlotDesignation(1, SciDAVis::Y);
            for (int r = 0; r < 11; ++r) {
                double x = r * 2.0; // coarse grid 0, 2, ..., 20
                cmTblA->setCell(r, 0, x);
                cmTblA->setCell(r, 1, 3.0 * x + 1.0);
            }
            Table *cmTblB = mw->newTable("CurveMathB", 21, 4);
            cmTblB->setColName(0, "XB");
            cmTblB->setColPlotDesignation(0, SciDAVis::X);
            cmTblB->setColName(1, "YB");
            cmTblB->setColPlotDesignation(1, SciDAVis::Y);
            cmTblB->setColName(2, "Rep2");
            cmTblB->setColPlotDesignation(2, SciDAVis::Y);
            cmTblB->setColName(3, "Rep3");
            cmTblB->setColPlotDesignation(3, SciDAVis::Y);
            for (int r = 0; r < 21; ++r) {
                double x = double(r); // fine grid 0, 1, ..., 20
                cmTblB->setCell(r, 0, x);
                cmTblB->setCell(r, 1, x + 5.0);
                cmTblB->setCell(r, 2, x + 7.0);
                cmTblB->setCell(r, 3, x + 9.0);
            }
            MultiLayer *mlCM = mw->multilayerPlot(
                    cmTblA, QStringList() << (cmTblA->name() + "_YA"), Graph::Line);
            Graph *gCM = mlCM->activeGraph();
            gCM->insertCurve(cmTblB, cmTblB->colName(0), cmTblB->colName(1), Graph::Line);

            QwtPlotCurve *sumCurve = CurveMathTools::simpleCurveMath(
                    mw, gCM, cmTblA->name() + "_YA", cmTblB->name() + "_YB",
                    CurveMathTools::Add, true);
            CHECK_COND(sumCurve != nullptr && sumCurve->dataSize() >= 21,
                       "CurveMathTools::simpleCurveMath(Add) failed on mismatched X grids");
            double ySumAt10 = CurveMathTools::findYFromX(gCM, sumCurve->title().text(), 10.0);
            std::cout << "[INFO] simpleCurveMath(Add) at x=10: " << ySumAt10 << " (expected 46)\n";
            CHECK_COND(relErrWithin(ySumAt10, 46.0, 1e-5), "simpleCurveMath(Add) value mismatch");

            // Average Multiple Curves (YB=x+5, Rep2=x+7, Rep3=x+9 -> Mean=x+7, SD=2.0)
            MultiLayer *mlAvg = mw->multilayerPlot(
                    cmTblB,
                    QStringList() << (cmTblB->name() + "_YB") << (cmTblB->name() + "_Rep2")
                                  << (cmTblB->name() + "_Rep3"),
                    Graph::Line);
            Graph *gAvg = mlAvg->activeGraph();
            int curvesBeforeAvg = gAvg->curves();
            Table *avgTbl = nullptr;
            QwtPlotCurve *meanCurve = CurveMathTools::averageMultipleCurves(
                    mw, gAvg,
                    QStringList() << (cmTblB->name() + "_YB") << (cmTblB->name() + "_Rep2")
                                  << (cmTblB->name() + "_Rep3"),
                    41, true, &avgTbl);
            CHECK_COND(meanCurve != nullptr && avgTbl != nullptr
                               && gAvg->curves() >= curvesBeforeAvg + 3,
                       "averageMultipleCurves did not add Mean + SD band curves");
            double meanAt10 = CurveMathTools::findYFromX(gAvg, meanCurve->title().text(), 10.0);
            CHECK_COND(relErrWithin(meanAt10, 17.0, 1e-4)
                               && relErrWithin(avgTbl->cell(20, 2), 2.0, 1e-4),
                       "averageMultipleCurves Mean or SD mismatch");

            // Normalize Curve (Range0To1 and UnitArea)
            QwtPlotCurve *norm01 = CurveMathTools::normalizeCurve(
                    mw, gAvg, cmTblB->name() + "_YB", CurveMathTools::Range0To1);
            CHECK_COND(norm01 != nullptr && std::fabs(norm01->minYValue() - 0.0) < 1e-6
                               && std::fabs(norm01->maxYValue() - 1.0) < 1e-6,
                       "normalizeCurve(Range0To1) failed");
            QwtPlotCurve *normArea = CurveMathTools::normalizeCurve(
                    mw, gAvg, cmTblB->name() + "_YB", CurveMathTools::UnitArea);
            CHECK_COND(normArea != nullptr, "normalizeCurve(UnitArea) returned null");
            double trapArea = 0.0;
            for (int i = 0; i + 1 < normArea->dataSize(); ++i)
                trapArea += 0.5 * (normArea->y(i) + normArea->y(i + 1))
                            * (normArea->x(i + 1) - normArea->x(i));
            CHECK_COND(relErrWithin(trapArea, 1.0, 1e-4),
                       "normalizeCurve(UnitArea) integral is not 1.0");

            // Curve Intersections & Closed Polygon Loop Area
            Table *ixTbl = mw->newTable("IntersectPolyData", 5, 4);
            ixTbl->setColName(0, "X");
            ixTbl->setColPlotDesignation(0, SciDAVis::X);
            ixTbl->setColName(1, "LineUp");
            ixTbl->setColPlotDesignation(1, SciDAVis::Y);
            ixTbl->setColName(2, "LineDown");
            ixTbl->setColPlotDesignation(2, SciDAVis::Y);
            ixTbl->setColName(3, "LoopY");
            ixTbl->setColPlotDesignation(3, SciDAVis::Y);
            for (int r = 0; r < 5; ++r) {
                double x = r * 2.5; // 0..10
                ixTbl->setCell(r, 0, x);
                ixTbl->setCell(r, 1, x);
                ixTbl->setCell(r, 2, 10.0 - x);
                ixTbl->setCell(r, 3, 0.0);
            }
            MultiLayer *mlIx = mw->multilayerPlot(
                    ixTbl, QStringList() << (ixTbl->name() + "_LineUp")
                                         << (ixTbl->name() + "_LineDown"),
                    Graph::Line);
            Graph *gIx = mlIx->activeGraph();
            QVector<QPointF> xpts = CurveMathTools::findCurveIntersections(
                    gIx, ixTbl->name() + "_LineUp", ixTbl->name() + "_LineDown", true, mw);
            CHECK_COND(xpts.size() == 1 && relErrWithin(xpts[0].x(), 5.0, 1e-6)
                               && relErrWithin(xpts[0].y(), 5.0, 1e-6),
                       "findCurveIntersections failed to locate exact (5, 5) crossing");

            Table *loopTbl = mw->newTable("ClosedLoop", 5, 2);
            loopTbl->setColName(0, "LX");
            loopTbl->setColPlotDesignation(0, SciDAVis::X);
            loopTbl->setColName(1, "LY");
            loopTbl->setColPlotDesignation(1, SciDAVis::Y);
            const double loopPts[5][2] = {
                { 0.0, 0.0 }, { 4.0, 0.0 }, { 4.0, 3.0 }, { 0.0, 3.0 }, { 0.0, 0.0 }
            };
            for (int r = 0; r < 5; ++r) {
                loopTbl->setCell(r, 0, loopPts[r][0]);
                loopTbl->setCell(r, 1, loopPts[r][1]);
            }
            MultiLayer *mlLoop = mw->multilayerPlot(
                    loopTbl, QStringList() << (loopTbl->name() + "_LY"), Graph::Line);
            double polyA = CurveMathTools::polygonArea(
                    mlLoop->activeGraph(), loopTbl->name() + "_LY", false);
            std::cout << "[INFO] polygonArea of 4x3 rectangle = " << polyA << "\n";
            CHECK_COND(relErrWithin(polyA, 12.0, 1e-6), "polygonArea exact Shoelace loop area mismatch");
        }

        // 17b. SmoothFilter (LOWESS & Whittaker) + AdvancedSignalProc (IIR, STFT, Hilbert, CWT, DWT, 2D FFT)
        {
            const int nSig = 512;
            const double fs = 500.0;
            const double dt = 1.0 / fs;
            Table *sigTbl = mw->newTable("SignalProcData", nSig, 4);
            sigTbl->setColName(0, "Time");
            sigTbl->setColPlotDesignation(0, SciDAVis::X);
            sigTbl->setColName(1, "TwoTone");
            sigTbl->setColPlotDesignation(1, SciDAVis::Y);
            sigTbl->setColName(2, "AMSignal");
            sigTbl->setColPlotDesignation(2, SciDAVis::Y);
            sigTbl->setColName(3, "NoisyWave");
            sigTbl->setColPlotDesignation(3, SciDAVis::Y);

            double noisyMse = 0.0;
            for (int i = 0; i < nSig; ++i) {
                double tVal = i * dt;
                double cleanLow = std::sin(2.0 * M_PI * 5.0 * tVal);
                double twoTone = cleanLow + 0.85 * std::sin(2.0 * M_PI * 120.0 * tVal);
                double amEnv = 1.0 + 0.5 * std::cos(2.0 * M_PI * 3.0 * tVal);
                double amSig = amEnv * std::cos(2.0 * M_PI * 60.0 * tVal);
                // Deterministic high-frequency pseudo-Gaussian sum of incommensurate sines
                double noise = 0.22 * (std::sin(37.3 * i + 0.1) + std::cos(71.9 * i + 1.2)
                                       + std::sin(113.7 * i + 2.4)) / std::sqrt(1.5);
                double cleanSmooth = std::sin(2.0 * M_PI * 2.0 * tVal)
                                     + 0.5 * std::cos(2.0 * M_PI * 4.0 * tVal);
                noisyMse += noise * noise;

                sigTbl->setCell(i, 0, tVal);
                sigTbl->setCell(i, 1, twoTone);
                sigTbl->setCell(i, 2, amSig);
                sigTbl->setCell(i, 3, cleanSmooth + noise);
            }
            noisyMse /= double(nSig);

            MultiLayer *mlSig = mw->multilayerPlot(
                    sigTbl,
                    QStringList() << (sigTbl->name() + "_TwoTone")
                                  << (sigTbl->name() + "_AMSignal")
                                  << (sigTbl->name() + "_NoisyWave"),
                    Graph::Line);
            Graph *gSig = mlSig->activeGraph();

            // SmoothFilter::LOWESS & SmoothFilter::Whittaker
            int beforeSmooth = gSig->curves();
            SmoothFilter sfLowess(mw, gSig, sigTbl->name() + "_NoisyWave", SmoothFilter::LOWESS);
            sfLowess.setLowessSpan(0.08);
            sfLowess.run();
            CHECK_COND(gSig->curves() == beforeSmooth + 1, "SmoothFilter::LOWESS did not add curve");
            QwtPlotCurve *cLowess = gSig->curve(gSig->curves() - 1);

            SmoothFilter sfWhit(mw, gSig, sigTbl->name() + "_NoisyWave", SmoothFilter::Whittaker);
            sfWhit.setWhittakerLambda(200.0);
            sfWhit.run();
            CHECK_COND(gSig->curves() == beforeSmooth + 2, "SmoothFilter::Whittaker did not add curve");
            QwtPlotCurve *cWhit = gSig->curve(gSig->curves() - 1);

            double mseLowess = 0.0, mseWhit = 0.0;
            for (int i = 30; i < nSig - 30; ++i) {
                double tVal = i * dt;
                double clean = std::sin(2.0 * M_PI * 2.0 * tVal)
                               + 0.5 * std::cos(2.0 * M_PI * 4.0 * tVal);
                double dL = cLowess->y(i) - clean;
                double dW = cWhit->y(i) - clean;
                mseLowess += dL * dL;
                mseWhit += dW * dW;
            }
            mseLowess /= double(nSig - 60);
            mseWhit /= double(nSig - 60);
            std::cout << "[INFO] Smoothing MSE: raw=" << noisyMse << ", LOWESS=" << mseLowess
                      << ", Whittaker=" << mseWhit << "\n";
            CHECK_COND(mseLowess < 0.35 * noisyMse && mseWhit < 0.35 * noisyMse,
                       "LOWESS or Whittaker smoothing did not significantly reduce noise MSE");

            // Zero-phase Butterworth LowPass IIR Filter (cutoff 25 Hz removes 120 Hz, preserves 5 Hz!)
            QwtPlotCurve *cLp = AdvancedSignalProc::iirFilter(
                    mw, gSig, sigTbl->name() + "_TwoTone", AdvancedSignalProc::Butterworth,
                    AdvancedSignalProc::LowPass, 4, 25.0, 0.0, true);
            CHECK_COND(cLp != nullptr && cLp->dataSize() == nSig, "iirFilter returned null/wrong size");
            double rmsLpErr = 0.0;
            for (int i = 75; i < nSig - 75; ++i) {
                double clean5Hz = std::sin(2.0 * M_PI * 5.0 * (i * dt));
                double e = cLp->y(i) - clean5Hz;
                rmsLpErr += e * e;
            }
            rmsLpErr = std::sqrt(rmsLpErr / double(nSig - 150));
            std::cout << "[INFO] Zero-phase Butterworth LP RMS error vs 5Hz sine = " << rmsLpErr << "\n";
            CHECK_COND(rmsLpErr < 0.03,
                       "Zero-phase Butterworth LP failed to remove 120Hz tone while preserving 5Hz sine");

            // STFT Spectrogram Matrix
            Matrix *stftMat = AdvancedSignalProc::computeSTFT(
                    mw, gSig, sigTbl->name() + "_TwoTone", 64, 16, false);
            CHECK_COND(stftMat != nullptr && stftMat->numRows() >= 16 && stftMat->numCols() >= 16,
                       "AdvancedSignalProc::computeSTFT failed");

            // Hilbert Envelope on AM Signal: envelope = 1.0 + 0.5*cos(2*pi*3*t)
            QwtPlotCurve *cEnv = AdvancedSignalProc::hilbertEnvelope(
                    mw, gSig, sigTbl->name() + "_AMSignal", false);
            CHECK_COND(cEnv != nullptr && cEnv->dataSize() == nSig,
                       "AdvancedSignalProc::hilbertEnvelope failed");
            double rmsEnvErr = 0.0;
            for (int i = 60; i < nSig - 60; ++i) {
                double trueEnv = 1.0 + 0.5 * std::cos(2.0 * M_PI * 3.0 * (i * dt));
                double e = cEnv->y(i) - trueEnv;
                rmsEnvErr += e * e;
            }
            rmsEnvErr = std::sqrt(rmsEnvErr / double(nSig - 120));
            std::cout << "[INFO] Hilbert AM Envelope RMS error = " << rmsEnvErr << "\n";
            CHECK_COND(rmsEnvErr < 0.035, "Hilbert envelope failed to recover exact AM modulation");

            // Continuous Wavelet Transform (Morlet CWT)
            Matrix *cwtMat = AdvancedSignalProc::waveletCWT(
                    mw, gSig, sigTbl->name() + "_TwoTone", 24, AdvancedSignalProc::Morlet, false);
            CHECK_COND(cwtMat != nullptr && cwtMat->numRows() == 24 && cwtMat->numCols() == nSig,
                       "AdvancedSignalProc::waveletCWT failed");

            // Discrete Wavelet Denoising (Daubechies4 DWT SNR improvement check)
            QwtPlotCurve *cDwt = AdvancedSignalProc::waveletDenoiseDWT(
                    mw, gSig, sigTbl->name() + "_NoisyWave", AdvancedSignalProc::Daubechies4, 4,
                    true, 1.0);
            CHECK_COND(cDwt != nullptr && cDwt->dataSize() == nSig,
                       "AdvancedSignalProc::waveletDenoiseDWT failed");
            double dwtMse = 0.0;
            for (int i = 20; i < nSig - 20; ++i) {
                double tVal = i * dt;
                double clean = std::sin(2.0 * M_PI * 2.0 * tVal)
                               + 0.5 * std::cos(2.0 * M_PI * 4.0 * tVal);
                double d = cDwt->y(i) - clean;
                dwtMse += d * d;
            }
            dwtMse /= double(nSig - 40);
            std::cout << "[INFO] DWT(db4) Denoise MSE: before=" << noisyMse
                      << ", after=" << dwtMse << "\n";
            CHECK_COND(dwtMse < 0.45 * noisyMse,
                       "waveletDenoiseDWT did not improve SNR over noisy signal");

            // 2D Matrix FFT
            Matrix *srcMat2D = mw->newMatrix("Cosine2DMat", 32, 32);
            for (int r = 0; r < 32; ++r)
                for (int c = 0; c < 32; ++c)
                    srcMat2D->setCell(r, c, std::cos(2.0 * M_PI * (3.0 * c / 32.0 + 5.0 * r / 32.0)));
            Matrix *fftMat2D = AdvancedSignalProc::fft2D(mw, srcMat2D, false, true);
            CHECK_COND(fftMat2D != nullptr && fftMat2D->numRows() == 32 && fftMat2D->numCols() == 32,
                       "AdvancedSignalProc::fft2D failed");
            double maxSpec2D = 0.0;
            for (int r = 0; r < 32; ++r)
                for (int c = 0; c < 32; ++c)
                    maxSpec2D = std::max(maxSpec2D, fftMat2D->cell(r, c));
            CHECK_COND(maxSpec2D > 100.0 && fftMat2D->cell(16, 16) < 0.05 * maxSpec2D,
                       "fft2D centered magnitude spectrum missing sharp off-DC spatial frequency peak");
        }
    }

    // =========================================================================
    // 18. 3D Surface Fitting, 2D Peak Analysis, & Graph Statistics (Domain 6)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 18: 3D Surface Fit, 2D Peaks, & Graph Statistics...\n";

        // 18a. fitSurfaceXYZ & fitSurfaceMatrix (Gauss2D) + analyze2DPeaks
        {
            const int side = 25;
            Table *xyzTbl = mw->newTable("SurfXYZ", side * side, 3);
            xyzTbl->setColName(0, "X");
            xyzTbl->setColPlotDesignation(0, SciDAVis::X);
            xyzTbl->setColName(1, "Y");
            xyzTbl->setColPlotDesignation(1, SciDAVis::Y);
            xyzTbl->setColName(2, "Z");
            xyzTbl->setColPlotDesignation(2, SciDAVis::Z);

            Matrix *surfMat = mw->newMatrix("SurfGaussMat", side, side);
            surfMat->setCoordinates(-5.0, 5.0, -5.0, 5.0);

            int rowIdx = 0;
            for (int r = 0; r < side; ++r) {
                double y = -5.0 + 10.0 * double(r) / double(side - 1);
                for (int c = 0; c < side; ++c) {
                    double x = -5.0 + 10.0 * double(c) / double(side - 1);
                    double z = 2.0
                               + 50.0 * std::exp(-0.5 * (std::pow((x - 1.2) / 1.1, 2.0)
                                                         + std::pow((y + 0.8) / 1.6, 2.0)));
                    xyzTbl->setCell(rowIdx, 0, x);
                    xyzTbl->setCell(rowIdx, 1, y);
                    xyzTbl->setCell(rowIdx, 2, z);
                    surfMat->setCell(r, c, z);
                    ++rowIdx;
                }
            }

            auto resXYZ = GraphStatsAndSurfaceFit::fitSurfaceXYZ(
                    mw, xyzTbl, "X", "Y", "Z", GraphStatsAndSurfaceFit::Gauss2D, QString(), 25);
            std::cout << "[INFO] fitSurfaceXYZ(Gauss2D): converged=" << resXYZ.converged
                      << ", R^2=" << resXYZ.rSquare << ", xc=" << resXYZ.params.value(2)
                      << ", yc=" << resXYZ.params.value(3) << ", A=" << resXYZ.params.value(1) << "\n";
            CHECK_COND(resXYZ.converged && resXYZ.rSquare > 0.9999
                               && resXYZ.fittedMatrix != nullptr && resXYZ.paramTable != nullptr
                               && relErrWithin(resXYZ.params[1], 50.0, 0.005)
                               && std::fabs(resXYZ.params[2] - 1.2) < 0.02
                               && std::fabs(resXYZ.params[3] - (-0.8)) < 0.02,
                       "fitSurfaceXYZ(Gauss2D) failed to recover 2D Gaussian parameters");

            auto resMat = GraphStatsAndSurfaceFit::fitSurfaceMatrix(
                    mw, surfMat, GraphStatsAndSurfaceFit::Gauss2D);
            double minSig = std::min(resMat.params.value(4), resMat.params.value(5));
            double maxSig = std::max(resMat.params.value(4), resMat.params.value(5));
            std::cout << "[INFO] fitSurfaceMatrix(Gauss2D): R^2=" << resMat.rSquare
                      << ", sigmas=(" << minSig << ", " << maxSig << ")\n";
            CHECK_COND(resMat.converged && resMat.rSquare > 0.9999
                               && relErrWithin(minSig, 1.1, 0.01)
                               && relErrWithin(maxSig, 1.6, 0.01),
                       "fitSurfaceMatrix(Gauss2D) failed to recover widths");

            // 2D Peak Analysis on 41x41 Matrix with 2 separated 2D Gaussian peaks
            Matrix *pkMat2D = mw->newMatrix("TwoPeaks2DMat", 41, 41);
            pkMat2D->setCoordinates(-10.0, 10.0, -10.0, 10.0);
            for (int r = 0; r < 41; ++r) {
                double y = -10.0 + 20.0 * double(r) / 40.0;
                for (int c = 0; c < 41; ++c) {
                    double x = -10.0 + 20.0 * double(c) / 40.0;
                    double z = 1.5 + 0.05 * x
                               + 80.0 * std::exp(-0.5 * ((x + 4.0) * (x + 4.0) / 1.44
                                                         + (y + 3.0) * (y + 3.0) / 1.44))
                               + 55.0 * std::exp(-0.5 * ((x - 4.5) * (x - 4.5) / 1.69
                                                         + (y - 3.5) * (y - 3.5) / 1.69));
                    pkMat2D->setCell(r, c, z);
                }
            }
            Table *pk2DTbl = GraphStatsAndSurfaceFit::analyze2DPeaks(mw, pkMat2D, 5, 0.15);
            CHECK_COND(pk2DTbl != nullptr && pk2DTbl->numRows() == 2,
                       "analyze2DPeaks did not detect exactly 2 peaks");
            std::cout << "[INFO] analyze2DPeaks: Peak1=(" << pk2DTbl->cell(0, 1) << ", "
                      << pk2DTbl->cell(0, 2) << "), Peak2=(" << pk2DTbl->cell(1, 1) << ", "
                      << pk2DTbl->cell(1, 2) << ")\n";
            CHECK_COND(std::fabs(pk2DTbl->cell(0, 1) - (-4.0)) < 0.25
                               && std::fabs(pk2DTbl->cell(0, 2) - (-3.0)) < 0.25
                               && std::fabs(pk2DTbl->cell(1, 1) - 4.5) < 0.25
                               && std::fabs(pk2DTbl->cell(1, 2) - 3.5) < 0.25,
                       "analyze2DPeaks 2D peak center coordinates inaccurate");
        }

        // 18b. Confidence Ellipse, ANOVA + Significance Brackets, ROC Curve, Kaplan-Meier, PCA Biplot
        {
            // Confidence Ellipse
            Table *ellTbl = mw->newTable("BivarData", 25, 2);
            ellTbl->setColName(0, "X");
            ellTbl->setColPlotDesignation(0, SciDAVis::X);
            ellTbl->setColName(1, "Y");
            ellTbl->setColPlotDesignation(1, SciDAVis::Y);
            for (int r = 0; r < 25; ++r) {
                double u = -2.0 + 4.0 * double(r) / 24.0;
                double v = 0.6 * std::sin(2.1 * r);
                ellTbl->setCell(r, 0, 10.0 + 2.0 * u);
                ellTbl->setCell(r, 1, 20.0 + 1.4 * u + v);
            }
            MultiLayer *mlEll = mw->multilayerPlot(
                    ellTbl, QStringList() << (ellTbl->name() + "_Y"), Graph::Scatter);
            Graph *gEll = mlEll->activeGraph();
            QwtPlotCurve *cEll = GraphStatsAndSurfaceFit::addConfidenceEllipse(
                    mw, gEll, ellTbl->name() + "_Y", 0.95, false);
            CHECK_COND(cEll != nullptr && cEll->dataSize() == 121
                               && std::fabs(cEll->x(0) - cEll->x(120)) < 1e-6,
                       "addConfidenceEllipse failed to plot closed 121-point 95% ellipse");

            // One-Way ANOVA + Pairwise Significance Brackets on Graph
            Table *grpTbl = mw->newTable("GroupAnovaData", 12, 3);
            grpTbl->setColName(0, "Control");
            grpTbl->setColName(1, "LowDose");
            grpTbl->setColName(2, "HighDose");
            for (int r = 0; r < 12; ++r) {
                double spread = 0.5 * std::sin(1.9 * r);
                grpTbl->setCell(r, 0, 10.0 + spread);
                grpTbl->setCell(r, 1, 14.5 + spread);
                grpTbl->setCell(r, 2, 26.0 + spread);
            }
            MultiLayer *mlGrp = mw->newGraph("AnovaBracketGraph");
            Graph *gGrp = mlGrp->activeGraph();
            int arrowsBefore = gGrp->lineMarkerKeys().size();
            Table *anovaTbl = GraphStatsAndSurfaceFit::runGroupComparisonAndAnnotate(
                    mw, grpTbl, QStringList() << "Control" << "LowDose" << "HighDose", gGrp);
            CHECK_COND(anovaTbl != nullptr && anovaTbl->numRows() == 4,
                       "runGroupComparisonAndAnnotate failed");
            double pAnova = anovaTbl->cell(0, 4);
            QString sigStars = anovaTbl->column(7)->textAt(0);
            std::cout << "[INFO] One-Way ANOVA: F=" << anovaTbl->cell(0, 1) << ", p=" << pAnova
                      << " (" << sigStars.toStdString() << "), arrows added="
                      << (gGrp->lineMarkerKeys().size() - arrowsBefore) << "\n";
            CHECK_COND(pAnova < 1e-8 && sigStars == "***"
                               && gGrp->lineMarkerKeys().size() >= arrowsBefore + 3,
                       "ANOVA p-value or graph significance brackets missing");

            // ROC Curve & AUC
            Table *rocSrc = mw->newTable("RocInput", 30, 2);
            rocSrc->setColName(0, "State");
            rocSrc->setColName(1, "Score");
            for (int r = 0; r < 30; ++r) {
                int state = (r >= 15) ? 1 : 0;
                double score = (state == 1 ? 2.5 : 0.5) + 0.6 * std::sin(1.3 * r);
                rocSrc->setCell(r, 0, double(state));
                rocSrc->setCell(r, 1, score);
            }
            double aucVal = 0.0;
            MultiLayer *mlRoc = GraphStatsAndSurfaceFit::plotROCCurve(
                    mw, rocSrc, "State", "Score", &aucVal);
            std::cout << "[INFO] ROC Curve AUC = " << aucVal << "\n";
            CHECK_COND(mlRoc != nullptr && mlRoc->activeGraph() != nullptr
                               && aucVal > 0.95 && aucVal <= 1.0,
                       "plotROCCurve failed or returned unexpected AUC");

            // Kaplan-Meier Survival & Mantel-Cox Log-Rank p-value
            Table *kmSrc = mw->newTable("KmInput", 24, 3);
            kmSrc->setColName(0, "Time");
            kmSrc->setColName(1, "Event");
            kmSrc->setColName(2, "Group");
            kmSrc->setColumnType(2, SciDAVis::ColumnMode::Text);
            for (int r = 0; r < 12; ++r) {
                kmSrc->setCell(r, 0, 2.0 + r * 0.8);      // Placebo: early events t = 2..10.8
                kmSrc->setCell(r, 1, 1.0);
                kmSrc->setText(r, 2, "Placebo");

                kmSrc->setCell(12 + r, 0, 20.0 + r * 3.0); // Drug: much longer survival t = 20..53
                kmSrc->setCell(12 + r, 1, (r % 3 == 0) ? 0.0 : 1.0);
                kmSrc->setText(12 + r, 2, "Drug");
            }
            double logRankP = 1.0;
            MultiLayer *mlKm = GraphStatsAndSurfaceFit::plotKaplanMeier(
                    mw, kmSrc, "Time", "Event", "Group", &logRankP);
            std::cout << "[INFO] Kaplan-Meier Log-Rank p = " << logRankP << "\n";
            CHECK_COND(mlKm != nullptr && mlKm->activeGraph() != nullptr
                               && mlKm->activeGraph()->curves() >= 2 && logRankP < 0.001,
                       "plotKaplanMeier failed or Log-Rank p-value not significant");

            // Principal Component Analysis (PCA) & Biplot
            Table *pcaSrc = mw->newTable("PcaInput", 24, 4);
            pcaSrc->setColName(0, "Var1");
            pcaSrc->setColName(1, "Var2");
            pcaSrc->setColName(2, "Var3");
            pcaSrc->setColName(3, "Var4");
            for (int r = 0; r < 24; ++r) {
                double f1 = std::cos(0.45 * r);
                double f2 = std::sin(0.73 * r);
                pcaSrc->setCell(r, 0, 2.0 * f1 + 0.1 * f2);
                pcaSrc->setCell(r, 1, 1.9 * f1 - 0.15 * f2);
                pcaSrc->setCell(r, 2, 0.1 * f1 + 1.8 * f2);
                pcaSrc->setCell(r, 3, -0.1 * f1 + 1.75 * f2);
            }
            Table *pcaScores = nullptr;
            MultiLayer *mlPca = GraphStatsAndSurfaceFit::runPCAAndBiplot(
                    mw, pcaSrc, QStringList() << "Var1" << "Var2" << "Var3" << "Var4", &pcaScores);
            CHECK_COND(mlPca != nullptr && pcaScores != nullptr && mlPca->activeGraph() != nullptr,
                       "runPCAAndBiplot failed");
            double cumVarPC2 = pcaScores->cell(1, 5);
            std::cout << "[INFO] PCA Biplot: PC1+PC2 cumulative variance = " << cumVarPC2
                      << "%, loading arrows = " << mlPca->activeGraph()->lineMarkerKeys().size() << "\n";
            CHECK_COND(cumVarPC2 > 98.0 && mlPca->activeGraph()->lineMarkerKeys().size() >= 4,
                       "PCA Biplot cumulative variance or loading vectors invalid");
        }
    }

    // =========================================================================
    // 19. Dynamic Recalculation ("Green Lock"), Dialog Themes, & Bilingual Menus (Domain 7)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 19: Dynamic Recalculation, Themes, & Bilingual Menus...\n";

        Table *recTbl = mw->newTable("LiveRecalcData", 11, 2);
        recTbl->setColName(0, "X");
        recTbl->setColPlotDesignation(0, SciDAVis::X);
        recTbl->setColName(1, "Y");
        recTbl->setColPlotDesignation(1, SciDAVis::Y);
        for (int r = 0; r < 11; ++r) {
            double x = double(r);
            recTbl->setCell(r, 0, x);
            recTbl->setCell(r, 1, 2.5 * x + 1.0); // initial slope = 2.5
        }
        MultiLayer *mlRec = mw->multilayerPlot(
                recTbl, QStringList() << (recTbl->name() + "_Y"), Graph::Scatter);
        Graph *gRec = mlRec->activeGraph();

        RecalcManager *rm = RecalcManager::instance(mw);
        CHECK_COND(rm != nullptr, "RecalcManager::instance returned null");
        rm->setRecalcMode(RecalcManager::Auto);

        int linkId = rm->registerLink(mw, gRec, recTbl, recTbl->name() + "_Y",
                                      "LiveAutoLinearFit", RecalcManager::OpLinearFit);
        CHECK_COND(linkId > 0 && rm->activeLinkCount(gRec) >= 1,
                   "RecalcManager::registerLink failed");

        QwtPlotCurve *fitBefore = gRec->curve("LiveAutoLinearFit");
        CHECK_COND(fitBefore != nullptr && fitBefore->dataSize() >= 2,
                   "Auto RecalcManager did not materialize initial LiveAutoLinearFit curve");
        int lastPt = fitBefore->dataSize() - 1;
        double slopeBefore = (fitBefore->y(lastPt) - fitBefore->y(0))
                             / (fitBefore->x(lastPt) - fitBefore->x(0));
        std::cout << "[INFO] Live Recalc initial slope = " << slopeBefore << "\n";
        CHECK_COND(relErrWithin(slopeBefore, 2.5, 1e-5), "Initial live linear fit slope mismatch");

        // Double all Y values in source Table WITHOUT manually invoking LinearFit or recalculateAll!
        for (int r = 0; r < 11; ++r)
            recTbl->setCell(r, 1, 2.0 * recTbl->cell(r, 1)); // new line: y = 5.0 * x + 2.0
        QApplication::processEvents();

        QwtPlotCurve *fitAfter = gRec->curve("LiveAutoLinearFit");
        CHECK_COND(fitAfter != nullptr && fitAfter->dataSize() >= 2,
                   "LiveAutoLinearFit curve missing after source Table update");
        int lastPtAfter = fitAfter->dataSize() - 1;
        double slopeAfter = (fitAfter->y(lastPtAfter) - fitAfter->y(0))
                            / (fitAfter->x(lastPtAfter) - fitAfter->x(0));
        std::cout << "[INFO] Live Recalc slope after doubling Y = " << slopeAfter
                  << " (expected 5.0)\n";
        CHECK_COND(relErrWithin(slopeAfter, 5.0, 1e-5) && !rm->hasDirtyLinks(gRec),
                   "Auto RecalcManager did not automatically double fitted slope on Graph!");
        rm->removeLinksForGraph(gRec);

        // DialogThemeManager save / load / themeNames / delete round-trip
        QVariantMap themeIn;
        themeIn["model"] = QString("Boltzmann");
        themeIn["tolerance"] = 1e-9;
        themeIn["maxIterations"] = 600;
        themeIn["confidenceBand"] = true;
        DialogThemeManager::saveTheme("NonLinearFit", "OriginParityTheme", themeIn);
        CHECK_COND(DialogThemeManager::themeNames("NonLinearFit").contains("OriginParityTheme"),
                   "DialogThemeManager::themeNames missing saved theme");
        QVariantMap themeOut = DialogThemeManager::loadTheme("NonLinearFit", "OriginParityTheme");
        CHECK_COND(themeOut.value("model").toString() == "Boltzmann"
                           && std::fabs(themeOut.value("tolerance").toDouble() - 1e-9) < 1e-15
                           && themeOut.value("maxIterations").toInt() == 600
                           && themeOut.value("confidenceBand").toBool() == true,
                   "DialogThemeManager::loadTheme round-trip mismatch");
        DialogThemeManager::deleteTheme("NonLinearFit", "OriginParityTheme");
        CHECK_COND(!DialogThemeManager::themeNames("NonLinearFit").contains("OriginParityTheme"),
                   "DialogThemeManager::deleteTheme did not remove theme");

        // Bilingual Japanese menu verification (&Gadgets -> ガジェット(&G))
        mw->switchToLanguage("ja");
        QString jaGadgets = QApplication::translate("ApplicationWindow", "&Gadgets");
        std::cout << "[INFO] Bilingual check (ja): tr(&Gadgets) = "
                  << jaGadgets.toUtf8().constData() << "\n";
        CHECK_COND(jaGadgets == QString::fromUtf8("ガジェット(&G)"),
                   "switchToLanguage(\"ja\") did not translate &Gadgets to ガジェット(&G)");
        if (mw->gadgetsMenu) {
            CHECK_COND(mw->gadgetsMenu->title() == QString::fromUtf8("ガジェット(&G)"),
                       "ApplicationWindow::gadgetsMenu title was not updated to ガジェット(&G)");
        }
        mw->switchToLanguage("en");
        CHECK_COND(QApplication::translate("ApplicationWindow", "&Gadgets") == "&Gadgets",
                   "switchToLanguage(\"en\") did not restore &Gadgets");
    }

    mw->savedProject();
    std::cout << "[PASS] All 19 OriginPro 2021 Graph Analysis, Fitting, Signal, Stats, Gadgets, Recalc, and OLE suites passed!\n";

    // If --keep-clipboard-ms <ms> is passed, run the event loop for <ms> so external COM clients (e.g. PowerPoint) can paste
    int idx = args.indexOf("--keep-clipboard-ms");
    if (idx >= 0 && idx + 1 < args.size()) {
        int waitMs = args[idx + 1].toInt();
        if (waitMs > 0) {
            std::cout << "[INFO] Keeping OLE clipboard alive for " << waitMs << " ms...\n" << std::flush;
            QTimer::singleShot(waitMs, &app, &QCoreApplication::quit);
            app.exec();
        }
    }

    mw->setOleEmbeddedMode(true);
    delete mw;
    SciDAVisOle::shutdownOleServer();
    return 0;
}
