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
    std::cout << "[PASS] All OriginPro 2021 parity, Multi-Peak Fit, Table UX, and OLE tests passed!\n";

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
