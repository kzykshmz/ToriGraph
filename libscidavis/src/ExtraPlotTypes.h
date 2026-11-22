/***************************************************************************
    File                 : ExtraPlotTypes.h
    Project              : ToriGraph
    Description          : E11 graph types for materials labs: error bands from error
                           columns, colour-mapped scatter (Z colour, optional size column),
                           histogram + cumulative %, lognormal and Gumbel (extreme value)
                           probability plots.
 ***************************************************************************/
#ifndef EXTRA_PLOT_TYPES_H
#define EXTRA_PLOT_TYPES_H

#include <QColor>
#include <QString>
#include <QVector>

class ApplicationWindow;
class Graph;
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

    // ---- Dialogs (no-ops in headless / non-interactive mode) ------------------------------
    static void showErrorBandDialog(ApplicationWindow *app);
    static void showColorScatterDialog(ApplicationWindow *app);
    static void showHistogramCumulativeDialog(ApplicationWindow *app);
    static void showProbabilityPlotDialog(ApplicationWindow *app, ProbabilityDistribution dist);
};

#endif // EXTRA_PLOT_TYPES_H
