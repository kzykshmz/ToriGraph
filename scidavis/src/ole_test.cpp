#include "ApplicationWindow.h"
#include "MultiLayer.h"
#include "Graph.h"
#include "Plot.h"
#include "Legend.h"
#include "Table.h"
#include "MultiPeakFit.h"
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

    mw->savedProject();

    // =========================================================================
    // 5. Test EMF Vector Rendering & OLE Storage Round-Trip + Clipboard
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
