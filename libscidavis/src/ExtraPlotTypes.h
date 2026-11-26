/***************************************************************************
    File                 : ExtraPlotTypes.h
    Project              : ToriGraph
    Description          : E11 graph types for materials labs: error bands from error
                           columns, colour-mapped scatter (Z colour, optional size column),
                           histogram + cumulative %, lognormal and Gumbel (extreme value)
                           probability plots, 2D density (2D histogram) and XRD pole figures.
 ***************************************************************************/
#ifndef EXTRA_PLOT_TYPES_H
#define EXTRA_PLOT_TYPES_H

#include <QColor>
#include <QString>
#include <QVector>

class ApplicationWindow;
class Graph;
class Matrix;
class MultiLayer;
class QAction;
class Table;

class ExtraPlotTypes
{
public:
    enum ActionId {
        ActErrorBand = 0,
        ActColorScatter,
        ActHistogramCumulative,
        ActLognormalProbability,
        ActGumbelProbability,
        ActDensity2D,
        ActPoleFigure,
        ActCount
    };

    //! Application-owned action (created on first use, object name "actionE11..."), retranslated
    //! on every language change. Safe to add to several menus.
    static QAction *action(ApplicationWindow *app, ActionId id);
    static void registerTranslations();

    // ---- 1. Error band -----------------------------------------------------------------
    //! Adds a translucent error band to the data curve whose Y column is \a yColumn (full name
    //! "Table1_B"). \a plusErrCol alone: symmetric ± band; with \a minusErrCol: asymmetric band
    //! (+ and − errors from separate columns). Returns the number of band curves added (0 on
    //! failure). An invalid \a color uses the curve's line colour.
    static int addErrorBand(Graph *g, const QString &yColumn, Table *errTable,
                            const QString &plusErrCol, const QString &minusErrCol = QString(),
                            int alpha = 64, const QColor &color = QColor());

    // ---- 2. Colour-mapped scatter ---------------------------------------------------------
    static MultiLayer *plotColorMappedScatter(ApplicationWindow *app, Table *t,
                                              const QString &xCol, const QString &yCol,
                                              const QString &zCol,
                                              const QString &sizeCol = QString(),
                                              int preset = 2 /*Spectrogram::PresetViridis*/,
                                              bool reverse = false);

    // ---- 3. Histogram + cumulative % ------------------------------------------------------
    struct HistogramCumulativeResult {
        bool valid = false;
        QString error;
        int bins = 0;
        double binWidth = 0.0;
        double begin = 0.0;
        int total = 0;
        QVector<double> upperEdges;    //!< begin, begin+w, ... (bins + 1 values)
        QVector<double> cumulativePct; //!< 0 ... 100 (bins + 1 values)
        Table *table = nullptr;
        MultiLayer *plot = nullptr;
    };
    //! \a binWidth <= 0 uses the histogram's automatic binning.
    static HistogramCumulativeResult plotHistogramCumulative(ApplicationWindow *app, Table *t,
                                                             const QString &col,
                                                             double binWidth = 0.0,
                                                             double begin = 0.0);

    // ---- 4. Probability plots -------------------------------------------------------------
    enum ProbabilityDistribution { LogNormal = 0, Gumbel = 1 };
    enum PlottingPosition {
        MedianRank = 0, //!< (i - 0.3) / (n + 0.4)  (Benard)
        MeanRank = 1,   //!< i / (n + 1)  (ASTM E2283 / Murakami extreme value)
        Hazen = 2,      //!< (i - 0.5) / n
        Blom = 3        //!< (i - 0.375) / (n + 0.25)
    };
    struct ProbabilityPlotResult {
        bool valid = false;
        QString error;
        int n = 0;
        int excluded = 0;
        //! LogNormal: mu and sigma of ln(x). Gumbel: location lambda and scale delta.
        double location = 0.0;
        double scale = 0.0;
        double rSquare = 0.0;
        double returnPeriod = 0.0;  //!< Gumbel: T used for returnValue (0 = none)
        double returnValue = 0.0;   //!< Gumbel: x_T = lambda + delta * (-ln(-ln(1 - 1/T)))
        QVector<double> sorted;     //!< sorted values used
        QVector<double> variate;    //!< z (probit) or y (reduced variate) of each value
        Table *table = nullptr;
        MultiLayer *plot = nullptr;
    };
    static double plottingPosition(int i /*1-based*/, int n, PlottingPosition pos);
    static ProbabilityPlotResult plotProbability(ApplicationWindow *app, Table *t,
                                                 const QString &col,
                                                 ProbabilityDistribution dist,
                                                 PlottingPosition pos,
                                                 double returnPeriod = 0.0,
                                                 bool createPlot = true);

    // ---- 5. 2D density (2D histogram) -----------------------------------------------------
    struct Density2DResult {
        bool valid = false;
        QString error;
        int n = 0;          //!< points counted
        int excluded = 0;   //!< rows skipped (missing / non-finite X or Y)
        int binsX = 0, binsY = 0;
        double x0 = 0.0, y0 = 0.0;          //!< lower edge of the first bin
        double widthX = 0.0, widthY = 0.0;  //!< bin widths
        double maxCount = 0.0;
        QVector<double> counts;  //!< row-major [iy * binsX + ix]
        Matrix *matrix = nullptr;
        MultiLayer *plot = nullptr;
    };
    //! Counts X/Y pairs on a regular grid. \a widthX / \a widthY > 0 fix the bin width (edges
    //! at multiples of the width); otherwise \a binsX / \a binsY bins span [min, max]; 0 / 0
    //! picks the bin count automatically (Rice rule 2 n^(1/3), 5 ... 256). No windows created.
    static Density2DResult binDensity2D(const QVector<double> &x, const QVector<double> &y,
                                        int binsX = 0, int binsY = 0, double widthX = 0.0,
                                        double widthY = 0.0);
    //! Builds the count matrix (kept as a matrix window, with the source columns and the bin
    //! settings recorded on the graph) and shows it as a heat map with a colour bar.
    static Density2DResult plotDensity2D(ApplicationWindow *app, Table *t, const QString &xCol,
                                         const QString &yCol, int binsX = 0, int binsY = 0,
                                         double widthX = 0.0, double widthY = 0.0,
                                         bool logColor = false,
                                         int preset = 2 /*Spectrogram::PresetViridis*/,
                                         bool createPlot = true);
    //! Re-counts a 2D density graph from its recorded source columns and settings (after the
    //! data changed). Returns false if \a g is not a 2D density graph or the source is gone.
    static bool rebuildDensity2D(ApplicationWindow *app, Graph *g);
    //! Updates an existing 2D density graph \a g in place with new binning, colour map,
    //! logarithmic colour scale, and cell-block settings.
    static Density2DResult updateDensity2D(ApplicationWindow *app, Graph *g, Table *t,
                                           const QString &xCol, const QString &yCol, int binsX = 0,
                                           int binsY = 0, double widthX = 0.0, double widthY = 0.0,
                                           bool logColor = false,
                                           int preset = 2 /*Spectrogram::PresetViridis*/,
                                           bool cellBlocks = true, bool reverse = false);

    // ---- 6. Pole figure -------------------------------------------------------------------
    enum PoleProjection { EqualArea = 0 /*Schmidt / Lambert*/, EqualAngle = 1 /*Wulff*/ };
    struct PoleFigureOptions {
        PoleProjection projection = EqualArea;
        bool normalizeMRD = true;   //!< divide by the solid-angle weighted mean (measured area)
        bool alphaFromRim = false;  //!< input alpha is 90° at the centre (Rigaku style)
        bool betaZeroAtTop = false; //!< beta = 0° at 12 o'clock (default: 3 o'clock)
        bool clockwise = false;     //!< beta increases clockwise (default: counter-clockwise)
        bool rdTop = true;          //!< primary axis label at top and secondary at right (else swapped)
        bool showTiltCircles = false; //!< draw 30° and 60° auxiliary tilt circles
        QString axis1Label;         //!< primary sample axis label (empty -> "RD")
        QString axis2Label;         //!< secondary sample axis label (empty -> "TD")
        bool useCustomMatrixAngles = false; //!< override Matrix X/Y coordinate range
        double matrixAlphaMin = 0.0;
        double matrixAlphaMax = 90.0;
        double matrixBetaMin = 0.0;
        double matrixBetaMax = 360.0;
        int gridSize = 201;         //!< projected matrix size (odd, 51 ... 801)
        int levels = 8;             //!< contour levels
        int preset = 2;             //!< Spectrogram::PresetViridis
        QString label;              //!< e.g. "(111)"; shown in the summary box
    };
    struct PoleFigureResult {
        bool valid = false;
        QString error;
        int nAlpha = 0, nBeta = 0;
        int missing = 0;           //!< alpha-beta grid nodes without a value
        double alphaMin = 0.0, alphaMax = 0.0; //!< measured tilt range (0° = centre)
        double meanIntensity = 0.0; //!< solid-angle weighted mean of the input intensity
        double maxValue = 0.0;      //!< maximum (MRD when normalised)
        double maxAlpha = 0.0, maxBeta = 0.0; //!< grid node of the maximum
        double maxX = 0.0, maxY = 0.0;        //!< its projected position (unit circle)
        Matrix *matrix = nullptr;   //!< projected grid (x, y in [-1, 1]; NaN = outside / missing)
        MultiLayer *plot = nullptr;
    };
    //! Projected position on the unit circle (alpha = 90° on the rim); false if alpha is
    //! outside 0 ... 90°. Equal-area: r = sqrt(2) sin(alpha / 2); equal-angle: r = tan(alpha / 2).
    static bool poleProjection(double alphaDeg, double betaDeg, PoleProjection proj, double *x,
                               double *y, bool betaZeroAtTop = false, bool clockwise = false);
    static bool inversePoleProjection(double x, double y, PoleProjection proj, double *alphaDeg,
                                      double *betaDeg, bool betaZeroAtTop = false,
                                      bool clockwise = false);
    static PoleFigureOptions defaultPoleFigureOptions();
    //! Input: alpha (tilt), beta (rotation) and intensity columns on a regular alpha-beta grid
    //! (typical XRD pole figure scan; beta 0 and 360 are merged).
    static PoleFigureResult plotPoleFigure(ApplicationWindow *app, Table *t,
                                           const QString &alphaCol, const QString &betaCol,
                                           const QString &intensityCol,
                                           const PoleFigureOptions &opt = defaultPoleFigureOptions(),
                                           bool createPlot = true);
    //! Input matrix: rows = alpha (matrix Y coordinates), columns = beta (matrix X coordinates).
    static PoleFigureResult plotPoleFigureFromMatrix(ApplicationWindow *app, Matrix *m,
                                                     const PoleFigureOptions &opt = defaultPoleFigureOptions(),
                                                     bool createPlot = true);

    //! Restores a <ToriDensity2D .../> or <ToriPoleFigure .../> graph tag (project files).
    static bool restoreGraphTag(Graph *g, const QString &line);
    //! The recorded tag of a 2D density / pole figure graph (empty if \a g is neither).
    static QString graphTag(Graph *g, const QString &kind);
    //! Synchronizes recorded <ToriDensity2D> display toggles (blocks/log) with live Spectrogram state.
    static QStringList syncedGraphTagsForState(const Graph *g);
    //! Updates <ToriDensity2D> / <ToriPoleFigure> tags on table, column, or matrix rename.
    static bool updateGraphTagsForRename(Graph *g, const QString &oldName, const QString &newName,
                                         bool updateTableName, bool updateMatrixName = false);
    //! Scale limit around unit pole-figure circle, expanding beyond 1.38 for large axis fonts.
    static double poleFigureScaleLimit(const Graph *g);

    // ---- Dialogs (no-ops in headless / non-interactive mode) ------------------------------
    static void showErrorBandDialog(ApplicationWindow *app);
    static void showColorScatterDialog(ApplicationWindow *app);
    static void showHistogramCumulativeDialog(ApplicationWindow *app);
    static void showProbabilityPlotDialog(ApplicationWindow *app, ProbabilityDistribution dist);
    static void showDensity2DDialog(ApplicationWindow *app);
    static void showPoleFigureDialog(ApplicationWindow *app);
};

inline ExtraPlotTypes::PoleFigureOptions ExtraPlotTypes::defaultPoleFigureOptions()
{
    return PoleFigureOptions();
}

#endif // EXTRA_PLOT_TYPES_H
