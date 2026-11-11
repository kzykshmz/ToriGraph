/***************************************************************************
    File                 : AdvancedSignalProc.h
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Description          : OriginPro Advanced Signal Processing:
                           IIR Digital Filters (Butterworth/Chebyshev I/Bessel,
                           Zero-Phase filtfilt), STFT Spectrogram, Hilbert
                           Envelope, Wavelet CWT & DWT VisuShrink Denoising,
                           2D Matrix FFT/IFFT, Welch PSD & Cross-Coherence
 ***************************************************************************/

#ifndef ADVANCEDSIGNALPROC_H
#define ADVANCEDSIGNALPROC_H

#include <QString>

class ApplicationWindow;
class Graph;
class Matrix;
class QwtPlotCurve;
class Table;

class AdvancedSignalProc
{
public:
    enum IirDesign { Butterworth = 0, ChebyshevI = 1, Bessel = 2 };
    enum PassType { LowPass = 0, HighPass = 1, BandPass = 2, BandStop = 3 };
    enum WaveletType { Morlet = 0, RickerMexicanHat = 1, Haar = 2, Daubechies4 = 3 };
    enum EnvelopeType { EnvelopeUpper = 0, EnvelopeLower = 1, EnvelopeBoth = 2 };

    static QwtPlotCurve *iirFilter(ApplicationWindow *app, Graph *g, const QString &curveTitle,
                                   IirDesign design, PassType pass, int order,
                                   double cutoffLowHz, double cutoffHighHz = 0.0,
                                   bool zeroPhase = true, double rippleDb = 1.0);
    static Matrix *computeSTFT(ApplicationWindow *app, Graph *g, const QString &curveTitle,
                               int windowSize = 64, int hopSize = 16,
                               bool plotSpectrogram = true);
    static QwtPlotCurve *hilbertEnvelope(ApplicationWindow *app, Graph *g,
                                         const QString &curveTitle,
                                         bool plotLowerEnvelope = false,
                                         Table **outTable = nullptr);
    static Matrix *waveletCWT(ApplicationWindow *app, Graph *g, const QString &curveTitle,
                              int numScales = 32, WaveletType wtype = Morlet,
                              bool plotScalogram = true);
    static QwtPlotCurve *waveletDenoiseDWT(ApplicationWindow *app, Graph *g,
                                           const QString &curveTitle,
                                           WaveletType wtype = Daubechies4,
                                           int levels = 4, bool softThreshold = true,
                                           double thresholdScale = 1.0);
    static Matrix *fft2D(ApplicationWindow *app, Matrix *srcMatrix, bool inverse = false,
                         bool shiftCenter = true);
    static Table *welchPSD(ApplicationWindow *app, Graph *g, const QString &curveTitle,
                           int segmentLen = 128, bool plotResult = true);
    static Table *crossCoherence(ApplicationWindow *app, Graph *g, const QString &curveA,
                                 const QString &curveB, int segmentLen = 128,
                                 bool plotResult = true);

    static QwtPlotCurve *savitzkyGolayDerivative(ApplicationWindow *app, Graph *g,
                                                 const QString &curveTitle,
                                                 int derivOrder = 1, int windowPoints = 9,
                                                 int polyOrder = 3);
    static QwtPlotCurve *computePeakEnvelope(ApplicationWindow *app, Graph *g,
                                             const QString &curveTitle,
                                             EnvelopeType envType = EnvelopeBoth,
                                             int minPeakDistPts = 3,
                                             QwtPlotCurve **lowerCurveOut = nullptr);
    static QwtPlotCurve *decimateCurve(ApplicationWindow *app, Graph *g,
                                       const QString &curveTitle,
                                       int factor = 4, bool firAntiAlias = true);

    static void showIirFilterDialog(ApplicationWindow *app, Graph *g);
    static void showStftDialog(ApplicationWindow *app, Graph *g);
    static void showHilbertDialog(ApplicationWindow *app, Graph *g);
    static void showWaveletDialog(ApplicationWindow *app, Graph *g);
    static void showPsdCoherenceDialog(ApplicationWindow *app, Graph *g);
    static void showSavGolDerivativeDialog(ApplicationWindow *app, Graph *g);
    static void showPeakEnvelopeDialog(ApplicationWindow *app, Graph *g);
    static void showDecimateDialog(ApplicationWindow *app, Graph *g);
};

#endif // ADVANCEDSIGNALPROC_H
