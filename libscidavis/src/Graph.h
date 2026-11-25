/***************************************************************************
    File                 : Graph.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Graph widget

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor,                    *
 *   Boston, MA  02110-1301  USA                                           *
 *                                                                         *
 ***************************************************************************/
#ifndef GRAPH_H
#define GRAPH_H

#include <QList>
#include <QPointer>
#include <QPrinter>
#include <QVector>
#include <QEvent>
#include <QMap>
#include <functional>

#include <qwt_plot.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_curve.h>
#include <qwt_scale_map.h>

#include "Plot.h"
#include "Table.h"
#include "PlotToolInterface.h"
#include "core/column/Column.h"

class QwtPlotCurve;
class QwtPlotZoomer;
class QwtPieCurve;
class Table;
class Legend;
class ArrowMarker;
class ImageMarker;
class TitlePicker;
class ScalePicker;
class CanvasPicker;
class ApplicationWindow;
class Matrix;
class SelectionMoveResizer;
class RangeSelectorTool;
class DataCurve;
class PlotCurve;
class QwtErrorPlotCurve;
class MultiLayer;

//! Structure containing curve layout parameters
typedef struct CurveLayout_struct
{
    unsigned int lCol = 0; //!< line color
    double lWidth = 1.0; //!< line width
    int lStyle = 0; //!< line style
    int lCapStyle = 0; //!< line CapStyle
    int lJoinStyle = 0x40; //!< line JoinStyle
    QString lCustomDash;
    int filledArea = 0; //!< flag: toggles area filling under curve
    unsigned int aCol = 0; //!< curve area color
    int aStyle = 0; //!< area filling style
    unsigned int symCol = 0; //!< symbol outline color
    bool symbolFill = true; //!< flag: toggles symbol filling
    unsigned int fillCol = 0; //!< symbol fill color
    double penWidth = 1.0; //!< symbol outline width
    int sSize = 7; //!< symbol size
    int sType = 0; //!< symbol type (shape)
    int connectType = 1; //!< symbol connection type
} CurveLayout;

/**
 * \brief A 2D-plotting widget.
 *
 * Graphs are managed by a MultiLayer, where they are sometimes referred to as "graphs" and
 * sometimes as "layers". Other parts of the code also call them "plots", regardless of the fact
 * that there's also a class Plot. Within the user interface, they are quite consistently called
 * "layers".
 *
 * Each graph owns a Plot called #d_plot, which handles parts of the curve, axis and marker
 * management (similarly to QwtPlot), as well as the pickers #d_zoomer (a QwtPlotZoomer),
 * #titlePicker (a TitlePicker), #scalePicker (a ScalePicker) and #cp (a CanvasPicker), which handle
 * various parts of the user interaction.
 *
 * Graph contains support for various curve types (see #CurveType),
 * some of them relying on SciDAVis-specific QwtPlotCurve subclasses for parts of the functionality.
 *
 * %Note that some of Graph's methods are implemented in analysis.cpp.
 *
 * \section future_plans Future Plans
 * Merge with Plot and CanvasPicker.
 * Think about merging in TitlePicker and ScalePicker.
 * On the other hand, things like range selection, peak selection or (re)moving data points could be
 * split out into tool classes like QwtPlotZoomer or SelectionMoveResizer.
 *
 * What definitely should be split out are plot types like histograms and pie charts (TODO: which
 * others?). We need a generic framework for this in any case so that new plot types can be
 * implemented in plugins, and Graph could do with a little diet. Especially after merging in Plot
 * and CanvasPicker. [ Framework needs to support plug-ins; assigned to knut ]
 *
 * Add support for floating-point line widths of curves and axes (request 2300).
 * [ assigned to thzs ]
 */

class Graph : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool isExporting READ isExporting)

public:
    Graph(QWidget *parent = 0, QString name = QString(), Qt::WindowFlags f = Qt::Widget);
    ~Graph();

    bool isExporting() const { return d_is_exporting; }

    enum Axis { Left, Right, Bottom, Top };
    enum class AxisType : int {
        Numeric = 0,
        Txt = 1,
        Day = 2,
        Month = 3,
        Time = 4,
        Date = 5,
        ColHeader = 6,
        DateTime = 22
    };
    enum MarkerType { None = -1, Text = 0, Arrow = 1, Image = 2 };

    struct GraphSelection {
        enum Kind {
            None = 0,
            Page,
            Layer,
            Axis,
            AxisTitle,
            PlotTitle,
            Curve,
            ErrorBars,
            Spectrogram,
            LegendBox,
            TextAnnotation,
            ArrowAnnotation,
            ImageAnnotation,
            ReferenceLine
        };
        Kind kind = None;
        int primaryId = -1;
        int secondaryId = -1;
        bool operator==(const GraphSelection &o) const
        {
            return kind == o.kind && primaryId == o.primaryId && secondaryId == o.secondaryId;
        }
        bool operator!=(const GraphSelection &o) const { return !(*this == o); }
    };

    enum LegendAnchor {
        LegendTopRight = 0,
        LegendTopLeft = 1,
        LegendBottomRight = 2,
        LegendBottomLeft = 3,
        LegendTopCenter = 4,
        LegendBottomCenter = 5,
        LegendOutsideRight = 6,
        LegendAutoAvoidData = 7
    };

    enum SeriesStyleScheme {
        SchemeBW2to3Group = 0,
        SchemeBlueFilledRedHollow = 1,
        SchemeMutedCategorical = 2,
        SchemeCoolToWarmStep = 3,
        SchemeViridisSequential = 4,
        SchemeOriginClassic = 5
    };
    enum CurveType {
        Line,
        Scatter,
        LineSymbols,
        VerticalBars,
        Area,
        Pie,
        VerticalDropLines,
        Spline,
        HorizontalSteps,
        Histogram,
        HorizontalBars,
        VectXYXY,
        ErrorBars,
        Box,
        VectXYAM,
        VerticalSteps,
        ColorMap,
        GrayMap,
        ContourMap,
        Function
    };

    Plot *d_plot;
    QwtPlotZoomer *d_zoomer[2];
    TitlePicker *titlePicker;
    ScalePicker *scalePicker;
    CanvasPicker *cp;

    static int mapToQwtAxis(int axis);

    //! Returns the name of the parent MultiLayer object.
    QString parentPlotName();

    //! Change the active tool, deleting the old one if it exists.
    void setActiveTool(PlotToolInterface *tool);
    //! Return the active tool, or NULL if none is active.
    PlotToolInterface *activeTool() const { return d_active_tool; }

    Grid *grid() { return d_plot->grid(); };

    void exportPainter(QPaintDevice &paintDevice, bool keepAspect = false, QRect rect = QRect());
    void exportPainter(QPainter &painter, bool keepAspect = false, QRect rect = QRect(),
                       QSize size = QSize());
public slots:
    //! Accessor method for #d_plot.
    Plot *plotWidget() const { return d_plot; };
    void copy(ApplicationWindow *parent, Graph *g);

    //! \name Pie Curves
    //@{
    //! Returns true if this Graph is a pie plot, false otherwise.
    bool isPiePlot() const { return (c_type.count() == 1 && c_type[0] == Pie); };
    void plotPie(Table *w, const QString &name, int startRow = 0, int endRow = -1);
    //! Used when restoring a pie plot from a project file
    void plotPie(Table *w, const QString &name, const QPen &pen, int brush, int size,
                 int firstColor, int startRow = 0, int endRow = -1, bool visible = true);
    void removePie();
    QString pieLegendText();
    QString savePieCurveLayout();
    //@}

    //! xColName が空でなければ、各 Y 列の X をその列に固定する（空なら表の X 指定列から探す）。
    bool insertCurvesList(Table *w, const QStringList &names, int style, int lWidth, int sSize,
                          int startRow = 0, int endRow = -1,
                          const QString &xColName = QString());
    bool insertCurve(Table *w, const QString &name, int style, int startRow = 0, int endRow = -1);
    bool insertCurve(Table *w, int xcol, const QString &name, int style);
    bool insertCurve(Table *w, const QString &xColName, const QString &yColName, int style,
                     int startRow = 0, int endRow = -1);
    void insertPlotItem(QwtPlotItem *i, int type);

    //! Shows/Hides a curve defined by its index.
    void showCurve(int index, bool visible = true);
    int visibleCurves();

    //! Removes a curve defined by its index.
    void removeCurve(int index);
    /**
     * \brief Removes a curve defined by its title string s.
     */
    void removeCurve(const QString &s);
    /**
     * \brief Removes all curves defined by the title/plot association string s.
     */
    void removeCurves(const QString &s);

public:
    //! Curves taken out of a layer (not deleted) by takeCurvesUsingColumns(), so that an undo
    //! command can put them back with restoreTakenCurves() and take them again with
    //! retakeCurves(). While they are out (\c out) the holder owns them and deletes them with
    //! discardTakenCurves().
    struct TakenCurve
    {
        QwtPlotItem *item = nullptr;
        int type = 0;
        int index = -1; //!< its index when it was taken
        QString legendLine; //!< its legend line (with \c{index+1}), empty if none
        bool fitCurve = false;
    };
    struct TakenCurves
    {
        QList<TakenCurve> curves; //!< in the layer's order
        //! Every item of the layer just before the take, in order; only compared, never used,
        //! to put each curve back after the item that preceded it.
        QList<QwtPlotItem *> order;
        bool out = false;
    };
    //! Takes out the curves removeCurves() would remove for any of \a names, with the error
    //! bars of those curves, keeping them whole (style, error bar links, legend line).
    TakenCurves takeCurvesUsingColumns(const QStringList &names);
    //! Puts the taken curves back, each after the item that preceded it (if still here) and
    //! with its legend line; the rest of the layer stays as it is now.
    void restoreTakenCurves(TakenCurves &tc);
    //! Takes the restored curves out again (those the user has removed since are skipped).
    void retakeCurves(TakenCurves &tc);
    static void discardTakenCurves(TakenCurves &tc);

public slots:

    void updateCurvesData(Table *w, const QString &yColName);

    int curves() const { return n_curves; };
    bool validCurvesDataSize() const;
    double selectedXStartValue();
    double selectedXEndValue();

    long curveKey(int curve) { return c_keys[curve]; }
    int curveIndex(long key) const { return c_keys.indexOf(key); }
    //! Map curve pointer to index.
    int curveIndex(QwtPlotCurve *c) const;
    //! map curve title to index
    int curveIndex(const QString &title) const { return plotItemsList().indexOf(title); }
    //! get curve by index
    QwtPlotCurve *curve(int index) const;
    //! get curve by name
    QwtPlotCurve *curve(const QString &title) const { return curve(curveIndex(title)); }

    //! Returns the names of all the curves suitable for data analysis, as a string list. The list excludes error bars and spectrograms.
    QStringList analysableCurvesList();
    //! Returns the names of all the QwtPlotCurve items on the plot, as a string list
    QStringList curvesList();
    //! Returns the names of all plot items, including spectrograms, as a string list
    QStringList plotItemsList() const;
    //! get plotted item by index
    QwtPlotItem *plotItem(int index);
    //! get plot item by index
    int plotItemIndex(QwtPlotItem *it) const;

    void updateCurveNames(const QString &oldName, const QString &newName,
                          bool updateTableName = true);

    int curveType(int curveIndex);
    //! Test whether curve can be converted to type using setCurveType().
    static bool canConvertTo(QwtPlotCurve *curve, CurveType type);
    //! Change the type of the given curve.
    /**
     * The option to disable updating is provided so as not to break the project opening code
     * (ApplicationWindow::openGraph()).
     */
    void setCurveType(int curve, CurveType type, bool update = true);
    void setCurveFullRange(int curveIndex);
    void setCurveAxes(int curveIndex, int xAxis, int yAxis);
    //! The legend label of a curve: the user's own label (curveCustomLabel()) when set,
    //! otherwise the label built from the table column (columnCurveLabel()).
    QString resolvedAutoCurveLabel(int curveIndex) const;
    //! The label built from the curve's table column (long name / comment / short name),
    //! ignoring any user label.
    QString columnCurveLabel(int curveIndex) const;
    //! User label of a table curve (DataCurve); empty when none is set.
    QString curveCustomLabel(int curveIndex) const;
    //! Sets (or, with an empty string, clears) the user label of a table curve. The curve
    //! title, i.e. its Y column binding, is left alone. Returns false for curves that are not
    //! bound to a table.
    bool setCurveCustomLabel(int curveIndex, const QString &label);
    //! Index of the first table curve whose user label is  label (exact match first, then
    //! case-insensitive), or -1.
    int curveIndexByCustomLabel(const QString &label) const;

    //! \name Output: Copy/Export/Print
    //@{
    void print();
    void setScaleOnPrint(bool on) { d_scale_on_print = on; };
    void printCropmarks(bool on) { d_print_cropmarks = on; };

    void copyImage();
    //! Provided for convenience in scripts
    void exportToFile(const QString &fileName);
    void exportSVG(const QString &fname);
    void exportVector(const QString &fileName, int res = 0, bool color = true,
                      bool keepAspect = true, QPageSize pageSize = QPageSize(QPageSize::Custom),
                      QPageLayout::Orientation orientation = QPageLayout::Portrait);
    void exportImage(const QString &fileName, int quality = -1, bool transparent = false,
                     int dpi = 0);

    //! Checked export variants returning write status and optional error string (C2-#11).
    bool exportToFileChecked(const QString &fileName, QString *errOut = nullptr);
    bool exportSVGChecked(const QString &fname, QString *errOut = nullptr);
    bool exportVectorChecked(const QString &fileName, int res = 0, bool color = true,
                             bool keepAspect = true,
                             QPageSize pageSize = QPageSize(QPageSize::Custom),
                             QPageLayout::Orientation orientation = QPageLayout::Portrait,
                             QString *errOut = nullptr);
    bool exportImageChecked(const QString &fileName, int quality = -1, bool transparent = false,
                            int dpi = 0, QString *errOut = nullptr);
    //@}

    void replot() { d_plot->replot(); };
    void updatePlot();

    //! \name Error Bars
    //@{
    bool addErrorBars(const QString &xColName, const QString &yColName, Table *errTable,
                      const QString &errColName, int type = 1, int width = 1, int cap = 8,
                      const QColor &color = QColor(Qt::black), bool through = true,
                      bool minus = true, bool plus = true);

    bool addErrorBars(const QString &yColName, Table *errTable, const QString &errColName,
                      int type = 1, int width = 1, int cap = 8,
                      const QColor &color = QColor(Qt::black), bool through = true,
                      bool minus = true, bool plus = true);

    void updateErrorBars(QwtErrorPlotCurve *er, bool xErr, int width, int cap, const QColor &c,
                         bool plus, bool minus, bool through);

    //! Returns a valid master curve for the error bars curve.
    DataCurve *masterCurve(QwtErrorPlotCurve *er);
    //! Returns a valid master curve for a plot association.
    DataCurve *masterCurve(const QString &xColName, const QString &yColName);
    //@}

    //! \name Event Handlers
    //@{
    void contextMenuEvent(QContextMenuEvent *);
    void closeEvent(QCloseEvent *e);
    bool focusNextPrevChild(bool next);
    //@}

    //! Set axis scale
    void setScale(int axis, double start, double end, double step = 0.0, int majorTicks = 5,
                  int minorTicks = 5, int type = 0, bool inverted = false);
    double axisStep(int axis) { return d_user_step[axis]; };
    //! The step that re-divides the axis to its current major ticks when saving (the
    //! <ToriAxisTickStep> tag) or cloning: the user step if re-dividing the range with it and
    //! the max-major count gives them back, otherwise the spacing of those ticks when that
    //! does (ticks set directly with a scale division by a style preset or the legend
    //! headroom). The user step itself is not changed.
    double effectiveMajorStep(int axis) const;

    //! \name Curves Layout
    //@{
    CurveLayout initCurveLayout(int style, int curves = 0);
    static CurveLayout initCurveLayout();
    //! Set layout parameters of the curve given by index.
    void updateCurveLayout(int index, const CurveLayout *cL);
    //! Tries to guess not already used curve color and symbol style
    void guessUniqueCurveLayout(int &colorIndex, int &symbolIndex);
    //@}

    //! \name Zoom
    //@{
    void zoomed(const QwtDoubleRect &);
    void zoom(bool on);
    void zoomOut();
    bool zoomOn();
    //@}

    void setAutoScale();
    void updateScale();

    //! \name Saving to File
    //@{
    QString saveToString(bool saveAsTemplate = false);
    QString saveScale();
    QString saveScaleTitles();
    QString saveFonts();
    QString saveMarkers();
    QString saveCurveLayout(int index);
    QString saveAxesTitleColors();
    QString saveAxesColors();
    QString saveEnabledAxes();
    QString saveCanvas();
    QString saveTitle();
    QString saveAxesTitleAlignement();
    QString saveEnabledTickLabels();
    QString saveTicksType();
    QString saveCurves();
    QString saveLabelsFormat();
    QString saveLabelsRotation();
    QString saveAxesLabelsType();
    QString saveAxesBaseline();
    QString saveAxesFormulas();
    //@}

    //! \name Text Markers
    //@{
    void drawText(bool on);
    bool drawTextActive() { return drawTextOn; };
    long insertTextMarker(Legend *mrk);

    //! Used when opening a project file
    long insertTextMarker(const QStringList &list, int fileVersion);
    void updateTextMarker(const QString &text, int angle, int bkg, const QFont &fnt,
                          const QColor &textColor, const QColor &backgroundColor);

    QFont defaultTextMarkerFont() { return defaultMarkerFont; };
    QColor textMarkerDefaultColor() { return defaultTextMarkerColor; };
    QColor textMarkerDefaultBackground() { return defaultTextMarkerBackground; };
    int textMarkerDefaultFrame() { return defaultMarkerFrame; };
    void setTextMarkerDefaults(int f, const QFont &font, const QColor &textCol,
                               const QColor &backgroundCol);

    void setCopiedMarkerType(Graph::MarkerType type) { selectedMarkerType = type; };
    void setCopiedMarkerEnds(const QPoint &start, const QPoint &end);
    void setCopiedTextOptions(int bkg, const QString &text, const QFont &font, const QColor &color,
                              const QColor &bkgColor);
    void setCopiedArrowOptions(int width, Qt::PenStyle style, const QColor &color, bool start,
                               bool end, int headLength, int headAngle, bool filledHead);
    void setCopiedImageName(const QString &fn) { auxMrkFileName = fn; };
    QRect copiedMarkerRect() { return QRect(auxMrkStart, auxMrkEnd); };
    QVector<int> textMarkerKeys() { return d_texts; };
    Legend *textMarker(long id);

    void addTimeStamp();

    void removeLegend();
    void removeLegendItem(int index);
    void addLegendItem(const QString &colName);
    void insertLegend(const QStringList &lst, int fileVersion);
    Legend *legend();
    Legend *newLegend();
    Legend *newLegend(const QString &text);
    void reconstructLegend();
    bool hasLegend() { return legendMarkerID >= 0; };

    //! Creates a new legend text using the curves titles
    QString legendText();
    //@}

    //! \name Line Markers
    //@{
    ArrowMarker *arrow(long id);
    void addArrow(ArrowMarker *mrk);

    //! Used when opening a project file
    void addArrow(QStringList list, int fileVersion);
    QVector<int> lineMarkerKeys() { return d_lines; };

    //!Draws a line/arrow depending on the value of "arrow"
    void drawLine(bool on, bool arrow = false);
    bool drawArrow() { return drawArrowOn; };
    bool drawLineActive() { return drawLineOn; };

    Qt::PenStyle arrowLineDefaultStyle() { return defaultArrowLineStyle; };
    bool arrowHeadDefaultFill() { return defaultArrowHeadFill; };
    int arrowDefaultWidth() { return defaultArrowLineWidth; };
    int arrowHeadDefaultLength() { return defaultArrowHeadLength; };
    int arrowHeadDefaultAngle() { return defaultArrowHeadAngle; };
    QColor arrowDefaultColor() { return defaultArrowColor; };

    void setArrowDefaults(int lineWidth, const QColor &c, Qt::PenStyle style, int headLength,
                          int headAngle, bool fillHead);
    bool arrowMarkerSelected();
    //@}

    //! \name Image Markers
    //@{
    ImageMarker *imageMarker(long id);
    QVector<int> imageMarkerKeys() { return d_images; };
    ImageMarker *addImage(ImageMarker *mrk);
    ImageMarker *addImage(const QString &fileName);

    void insertImageMarker(const QStringList &lst, int fileVersion);
    bool imageMarkerSelected();
    void updateImageMarker(int x, int y, int width, int height);
    //@}

    //! \name Common to all Markers
    //@{
    void removeMarker();
    void cutMarker();
    void copyMarker();
    void pasteMarker();
    //! Keep the markers on screen each time the scales are modified by adding/removing curves
    void updateMarkersBoundingRect();

public:
    //! The axis scales as the markers see them, taken before a scale change.
    struct MarkerAxisState {
        //! False while the scale division is invalid (Qwt's setAxisScale() until the replot).
        bool valid[QwtPlot::axisCnt] = { false, false, false, false };
        //! Canvas map (range, transformation incl. axis breaks / formula, pixel interval).
        QwtScaleMap map[QwtPlot::axisCnt];
    };
    MarkerAxisState markerAxisState() const;
    /*! Same as updateMarkersBoundingRect(), but against the scales in \a before (taken with
     * markerAxisState() before the change):
     * - a marker whose axes map every value as before keeps its axis coordinates, so refreshes
     *   such as updatePlot() before a copy or export cannot move it;
     * - otherwise text markers and pictures keep their place on the canvas, computed with the
     *   previous scales on the current canvas (not with the pixel cache of their placement,
     *   which is out of date after a view zoom or a window resize); arrows follow the data.
     * Afterwards every marker's pixel cache matches where it is drawn. */
    void updateMarkersBoundingRect(const MarkerAxisState &before);
    //! updateScale() against scales taken before the change (see above).
    void updateScale(const MarkerAxisState &before);
    //! Bring the pixel caches of all markers up to date with the current canvas (no data change).
    void refreshMarkerPixelCaches();

public slots:

    long selectedMarkerKey();
    /*!\brief Set the selected marker.
     * \param mrk key of the marker to be selected.
     * \param add whether the marker is to be added to an existing selection.
     * If <i>add</i> is false (the default) or there is no existing selection, a new
     * SelectionMoveResizer is created and stored in #d_markers_selector.
     */
    void setSelectedMarker(long mrk, bool add = false);
    QwtPlotMarker *selectedMarkerPtr();
    bool markerSelected();
    //! Reset any selection states on markers.
    void deselectMarker();
    MarkerType copiedMarkerType() { return selectedMarkerType; };
    //@}

    //! \name Axes
    //@{
    QList<AxisType> axesType();

    QStringList scalesTitles();
    void setXAxisTitle(const QString &text);
    void setYAxisTitle(const QString &text);
    void setRightAxisTitle(const QString &text);
    void setTopAxisTitle(const QString &text);
    void setAxisTitle(int axis, const QString &text);
    QString axisTitle(int axis) const;
    void refreshAutoAxisTitlesFromColumns();
    void setAxisTitleAuto(int axis, bool autoSync);
    bool isAxisTitleAuto(int axis) const;
    void setAxisUnitFormat(Column::UnitFormat fmt);
    Column::UnitFormat axisUnitFormat() const { return d_axis_unit_format; }

    QFont axisTitleFont(int axis);
    void setXAxisTitleFont(const QFont &fnt);
    void setYAxisTitleFont(const QFont &fnt);
    void setRightAxisTitleFont(const QFont &fnt);
    void setTopAxisTitleFont(const QFont &fnt);
    void setAxisTitleFont(int axis, const QFont &fnt);

    void setAxisFont(int axis, const QFont &fnt);
    QFont axisFont(int axis);
    QFont axisNumbersFont(int axis) const;
    QFont legendFont() const;
    void syncAxisColorToCurve(int qwtAxis, int curveIndex);
    void initFonts(const QFont &scaleTitleFnt, const QFont &numbersFnt);

    QColor axisTitleColor(int axis);
    void setXAxisTitleColor(const QColor &c);
    void setYAxisTitleColor(const QColor &c);
    void setRightAxisTitleColor(const QColor &c);
    void setTopAxisTitleColor(const QColor &c);
    void setAxesTitleColor(QStringList l);

    int axisTitleAlignment(int axis);
    void setXAxisTitleAlignment(int align);
    void setYAxisTitleAlignment(int align);
    void setTopAxisTitleAlignment(int align);
    void setRightAxisTitleAlignment(int align);
    void setAxesTitlesAlignment(const QStringList &align);

    QColor axisColor(int axis);
    QStringList axesColors();
    void setAxesColors(const QStringList &colors);

    QColor axisNumbersColor(int axis);
    QStringList axesNumColors();
    void setAxesNumColors(const QStringList &colors);

    void showAxis(int axis, AxisType type, const QString &formatInfo, Table *table, bool axisOn,
                  int majTicksType, int minTicksType, bool labelsOn, const QColor &c, int format,
                  int prec, int rotation, int baselineDist, const QString &formula,
                  const QColor &labelsColor);

    void enableAxis(int axis, bool on = true);
    QVector<bool> enabledAxes();
    void enableAxes(QVector<bool> axesOn);
    void enableAxes(const QStringList &list);

    int labelsRotation(int axis);
    void setAxisLabelRotation(int axis, int rotation);

    QStringList enabledTickLabels();
    void setEnabledTickLabels(const QStringList &list);
    void setTickLabelsOnAxis(int axis, bool on);
    bool hasTickLabelsOnAxis(int axis) const;

    void setAxesLinewidth(int width);
    //! used when opening a project file
    void loadAxesLinewidth(int width);

    void drawAxesBackbones(bool yes);
    bool axesBackbones() { return drawAxesBackbone; };
    //! used when opening a project file
    void loadAxesOptions(const QString &s);

    QList<int> axesBaseline();
    void setAxesBaseline(const QList<int> &lst);
    void setAxesBaseline(QStringList &lst);

    void setMajorTicksType(const QList<int> &lst);
    void setMajorTicksType(const QStringList &lst);

    void setMinorTicksType(const QList<int> &lst);
    void setMinorTicksType(const QStringList &lst);

    int minorTickLength();
    int majorTickLength();
    void setAxisTicksLength(int axis, int majTicksType, int minTicksType, int minLength,
                            int majLength);
    void setTicksLength(int minLength, int majLength);
    void changeTicksLength(int minLength, int majLength);

    void setLabelsNumericFormat(const QStringList &l);
    void setLabelsNumericFormat(int axis, const QStringList &l);
    void setLabelsNumericFormat(int axis, int format, int prec = 6,
                                const QString &formula = QString());
    void setLabelsDateTimeFormat(int axis, AxisType type, const QString &formatInfo);
    void setLabelsDayFormat(int axis, int format);
    void setLabelsMonthFormat(int axis, int format);

    QString axisFormatInfo(int axis);
    QStringList axesLabelsFormatInfo() { return axesFormatInfo; };

    void setLabelsTextFormat(int axis, const Column *column, int startRow, int endRow);
    void setLabelsTextFormat(int axis, Table *table, const QString &columnName);
    void setLabelsColHeaderFormat(int axis, Table *table);

    QStringList getAxesFormulas() { return axesFormulas; };
    void setAxesFormulas(const QStringList &l) { axesFormulas = l; };
    void setAxisFormula(int pos, const QString &f) { axesFormulas[pos] = f; };
    //@}

    //! \name Canvas Frame
    //@{
    void drawCanvasFrame(bool frameOn, int width);
    void drawCanvasFrame(const QStringList &frame);
    void drawCanvasFrame(bool frameOn, int width, const QColor &color);
    QColor canvasFrameColor();
    int canvasFrameWidth();
    bool framed();
    //@}

    //! \name Plot Title
    //@{
    void setTitle(const QString &t);
    void setTitleFont(const QFont &fnt);
    void setTitleColor(const QColor &c);
    void setTitleAlignment(int align);

    bool titleSelected();
    void selectTitle();

    void removeTitle();
    void initTitle(bool on, const QFont &fnt);
    //@}

    //! \name Modifing insertCurve Data
    //@{
    int selectedCurveID();
    int selectedCurveIndex() { return curveIndex(selectedCurveID()); }
    QString selectedCurveTitle();
    //@}

    void disableTools();

    /*! Enables the data range selector tool.
     *
     * This one is a bit special, because other tools can depend upon an existing selection.
     * Therefore, range selection (like zooming) has to be provided in addition to the generic
     * tool interface.
     */
    bool enableRangeSelectors(const QObject *status_target = NULL, const char *status_slot = "");

    //! Check wether range selectors are currently enabled.
    bool rangeSelectorsEnabled() const { return !d_range_selector.isNull(); }

    //! \name Border and Margin
    //@{
    void setMargin(int d);
    void setFrame(int width = 1, const QColor &color = QColor(Qt::black));
    void setBackgroundColor(const QColor &color);
    void setCanvasBackground(const QColor &color);
    //@}

    void addFitCurve(QwtPlotCurve *c);
    void deleteFitCurves();
    QList<QwtPlotCurve *> fitCurvesList() { return d_fit_curves; };
    /*! Set start and end to to the curve's minimum and maximum.
     *
     * \return the number of total points
     */
    int range(const int index, double& start, double& end) const;



    //!  Used for VerticalBars, HorizontalBars and Histograms
    void setBarsGap(int curve, int gapPercent, int offset);

    //! \name Image Analysis Tools
    //@{
    void showIntensityTable();
    //@}

    //! \name User-defined Functions
    //@{
    bool modifyFunctionCurve(ApplicationWindow *parent, int curve, int type,
                             const QStringList &formulas, const QString &var, QList<double> &ranges,
                             int points);
    bool addFunctionCurve(ApplicationWindow *parent, int type, const QStringList &formulas,
                          const QString &var, QList<double> &ranges, int points,
                          const QString &title = {});
    //! Used when reading from a project file.
    bool insertFunctionCurve(ApplicationWindow *parent, const QStringList &func_spec, int points,
                             int fileVersion);
    //! Returns an unique function name
    QString generateFunctionName(const QString &name = tr("F"));
    //@}

    //! Provided for convenience in scripts.
    void createTable(const QString &curveName);
    void createTable(const QwtPlotCurve *curve);
    void activateGraph();

    //! \name Vector Curves
    //@{
    void plotVectorCurve(Table *w, const QStringList &colList, int style, int startRow = 0,
                         int endRow = -1);
    void updateVectorsLayout(int curve, const QColor &color, int width, int arrowLength,
                             int arrowAngle, bool filled, int position,
                             const QString &xEndColName = QString(),
                             const QString &yEndColName = QString());
    //@}

    //! \name Box Plots
    //@{
    void openBoxDiagram(Table *w, const QStringList &l, int fileVersion);
    void plotBoxDiagram(Table *w, const QStringList &names, int startRow = 0, int endRow = -1);
    //@}

    bool plotHistogram(Table *w, QStringList names, int startRow = 0, int endRow = -1);

    void setCurveSymbol(int index, const QwtSymbol &s);
    void setCurvePen(int index, const QPen &p);
    void setCurveBrush(int index, const QBrush &b);
    void setCurveStyle(int index, int s);

    //! \name Resizing
    //@{
    bool ignoresResizeEvents() { return ignoreResize; };
    void setIgnoreResizeEvents(bool ok) { ignoreResize = ok; };
    void resizeEvent(QResizeEvent *e);
    void hideEvent(QHideEvent *e);
    void scaleFonts(double factor);
    //@}

    void notifyChanges();

    void updateSecondaryAxis(int axis);
    void enableAutoscaling(bool yes) { m_autoscale = yes; };
    bool autoscalingEnabled() const { return m_autoscale; }

    bool autoscaleFonts() { return autoScaleFonts; };
    void setAutoscaleFonts(bool yes) { autoScaleFonts = yes; };

    static int obsoleteSymbolStyle(int type);
    static QString penStyleName(Qt::PenStyle style);
    static Qt::PenStyle getPenStyle(const QString &s);
    static Qt::PenStyle getPenStyle(int style);
    static Qt::BrushStyle getBrushStyle(int style);
    static void showPlotErrorMessage(QWidget *parent, const QStringList &emptyColumns);
    static QPrinter::PageSize minPageSize(const QPrinter &printer, const QRect &r);

    void showTitleContextMenu();
    void copyTitle();
    void cutTitle();

    void removeAxisTitle();
    void cutAxisTitle();
    void copyAxisTitle();
    void showAxisTitleMenu(int axis);
    void showAxisContextMenu(int axis);
    void hideSelectedAxis();
    void showGrids();

    //! Convenience function enabling the grid for QwtScaleDraw::Left and Bottom Scales
    void showGrid();
    //! Convenience function enabling the grid for a user defined axis
    void showGrid(int axis);

    void showAxisDialog();
    void showScaleDialog();

    //! Add a spectrogram to the graph
    void plotSpectrogram(Matrix *m, CurveType type);
    //! Restores a spectrogram. Used when opening a project file.
    void restoreSpectrogram(ApplicationWindow *app, const QStringList &lst);

    bool antialiasing() { return d_antialiasing; };
    //! Enables/Disables antialiasing of plot items.
    void setAntialiasing(bool on = true, bool update = true);

    void deselect();
    void print(QPainter *, const QRect &rect,
               const QwtPlotPrintFilter &pfilter = QwtPlotPrintFilter());

    // OriginPro Color Palette System & Interactive Palette Editor
    static QStringList builtInPaletteNames();
    static QStringList customPaletteNames();
    static QStringList allPaletteNames();
    static QList<QColor> paletteColors(const QString &paletteName);
    //! Canonical palette definitions (single source of truth; E5).
    static const QList<QColor> &okabeItoColors();
    static const QList<QColor> &tab10Colors();
    static const QList<QColor> &originClassicColors();
    //! ToriGraph's own default for new curves: black first, then Okabe-Ito hues (no yellow).
    static const QList<QColor> &toriGraphStandardColors();
    //! Canonical palette names. The tab10 list used to be called "ISIJ / Origin Default";
    //! that name is still accepted as an alias by paletteColors().
    static QString tab10PaletteName() { return QStringLiteral("Matplotlib (tab10)"); }
    static QString originClassicPaletteName() { return QStringLiteral("Origin Classic"); }
    static QString okabeItoPaletteName() { return QStringLiteral("Okabe-Ito (Colorblind Safe)"); }
    static QString toriGraphStandardPaletteName() { return QStringLiteral("ToriGraph Standard"); }
    //! Palette for new curves when the user has not chosen one (ToriGraph Standard).
    static QString builtInDefaultCurvePaletteName() { return toriGraphStandardPaletteName(); }
    //! Maps legacy/alias palette names (e.g. "ISIJ / Origin Default", "Okabe-Ito",
    //! "Cool-Warm") to the canonical registered name. Unknown names are returned trimmed.
    static QString canonicalPaletteName(const QString &name);
    //! Palette used for new curves (setting "/Curves/DefaultPalette"; when the setting is
    //! absent, builtInDefaultCurvePaletteName()). An empty name removes the setting.
    static QString defaultCurvePaletteName();
    static void setDefaultCurvePaletteName(const QString &name);
    //! Forgets the cached default so the next defaultCurvePaletteName() re-reads the setting
    //! (as at start-up).
    static void reloadDefaultCurvePaletteName();
    //! Registers the Japanese names of the built-in palettes (idempotent).
    static void registerPaletteTranslations();
    //! Origin-style symbol increment (SymbolBox indices): square, circle, up/down triangle, ...
    static int defaultCurveSymbolIndex(int seriesIndex);
    //! True for built-in palette names and their aliases (case-insensitive).
    static bool isBuiltInPaletteName(const QString &name);
    //! Saves a user palette. Names of built-in palettes (and aliases) are refused, since the
    //! built-in palette would always win; \a error then gets a message for the user.
    static bool saveCustomPalette(const QString &name, const QList<QColor> &colors,
                                  QString *error = nullptr);
    static void removeCustomPalette(const QString &name);
    static QList<QColor> interpolatePalette(const QList<QColor> &anchors, int count);
    void applyColorPalette(const QList<QColor> &colors, bool gradientMode = false,
                           bool applyToSymbols = true);
    void applyColorPaletteByName(const QString &paletteName, bool gradientMode = false);
    static bool showColorPaletteEditor(QWidget *parent, Graph *targetGraph = nullptr);

    // OriginPro Plot Types Support
    void applyYOffsetStack(double offsetStep = 0.0, bool autoOffset = true);
    void setupTernaryPlot(const QString &labelA = "A", const QString &labelB = "B",
                          const QString &labelC = "C");
    static QPointF ternaryToCartesian(double a, double b, double c);
    bool isTernaryPlot() const { return d_is_ternary; }

public:
    enum FramePreset {
        FrameBoxInwardTicks = 0,
        FrameBoxPlainOpposite = 1,
        FrameOpenL = 2,
        FrameZeroCrossed = 3
    };

    struct ReferenceLineSpec {
        int orientation = 0; // 0 = Horizontal (Y), 1 = Vertical (X)
        double value = 0.0;
        QString statMode;
        QColor color = QColor(30, 102, 184);
        Qt::PenStyle style = Qt::DashLine;
        double width = 1.5;
        QString label;
        bool fillToAxis = false;
        QColor fillColor = QColor(30, 102, 184, 32);
        bool visible = true;
    };

    void applyFramePreset(FramePreset preset);
    void setAxisBackboneVisible(int axis, bool visible);
    bool isAxisBackboneVisible(int axis) const;
    bool hasColorScaleOnAxis(int axis) const;
    void addReferenceLine(const ReferenceLineSpec &spec);
    void setReferenceLines(const QVector<ReferenceLineSpec> &specs);
    QVector<ReferenceLineSpec> referenceLines() const;
    void clearReferenceLines();
    void updateStatReferenceLines();
    int referenceLineAtCanvasPos(const QPoint &pos, int tolPx = 6) const;
    void updateReferenceLine(int index, const ReferenceLineSpec &spec);
    void removeReferenceLine(int index);
    bool editReferenceLineDialog(int index);

    struct AxisViewportState {
        bool axisEnabled[4] = { false, false, false, false };
        double lower[4] = { 0.0, 0.0, 0.0, 0.0 };
        double upper[4] = { 0.0, 0.0, 0.0, 0.0 };
        double userStep[4] = { 0.0, 0.0, 0.0, 0.0 };
        int maxMajor[4] = { 5, 5, 5, 5 };
        int maxMinor[4] = { 5, 5, 5, 5 };
        bool isLog[4] = { false, false, false, false };
        bool inverted[4] = { false, false, false, false };
        bool userLocked[4] = { false, false, false, false };
        bool autoscale = true;
    };
    void recordZoomHistorySnapshot();
    bool stepZoomHistory(int delta);
    bool handleEscapeKey();
    bool nudgeSelectedElement(int dx, int dy);
    bool cycleSelectableCurve(int step);

    void setLinkedParentLayer(Graph *parentLayer, int xLinkMode, const QString &xExpr,
                              int yLinkMode, const QString &yExpr);
    Graph *linkedParentLayer() const;
    int xLinkMode() const { return d_x_link_mode; }
    int yLinkMode() const { return d_y_link_mode; }
    QString xLinkExpr() const { return d_x_link_expr; }
    QString yLinkExpr() const { return d_y_link_expr; }
    void syncLinkedAxesFromParent();
    void syncChildLinkedLayers();
    MultiLayer *multiLayerParent() const;

    struct AxisBreakSpec {
        bool enabled = false;
        double breakFrom = 0.0;
        double breakTo = 0.0;
        double positionPct = 0.50; // 0.10 .. 0.90 of axis length
        double stepBefore = 0.0;
        double stepAfter = 0.0;
        bool log10Before = false;
        bool log10After = false;
    };

    void setAxisBreak(int axis, const AxisBreakSpec &spec);
    AxisBreakSpec axisBreak(int axis) const;
    void clearAxisBreak(int axis);
    //! Break spec kept while the current axis range cannot show it (drawing suspended).
    //! enabled==true means a suspended break exists and will be restored once valid again.
    AxisBreakSpec suspendedAxisBreak(int axis) const;
    bool axisBreakSuspended(int axis) const;
    //! Re-applies the full 5-argument break indicator (from/to/log flags) of axis
    //! (or of its primary mirror axis) to the axis' ScaleDraw.
    void refreshAxisBreakIndicator(int axis);

    void setTickLabelTransform(int axis, double divideByFactor,
                               const QString &prefix = QString(),
                               const QString &suffix = QString());
    double tickDivideFactor(int axis) const;
    QString tickPrefix(int axis) const;
    QString tickSuffix(int axis) const;

    QwtPlotItem *addShadedBand(const QVector<double> &x, const QVector<double> &yLow,
                               const QVector<double> &yHigh,
                               const QColor &fillColor = QColor(30, 102, 184, 55),
                               const QString &title = QString());
    void removeShadedBandByTitle(const QString &title);
    void setStackOffsets(int mode /*0=Uniform,1=AutoGap*/, double yGapPct = 15.0,
                         double xGapPct = 0.0, bool normalize01 = false);
    void resetStackOffsets();

    GraphSelection currentSelection() const { return d_selection; }
    void setCurrentSelection(const GraphSelection &sel, bool notify = true);
    void updateSelectionOverlayGeometry();
    bool isCurveVisible(int index) const;
    bool moveCurve(int fromIndex, int toIndex);
    void syncCurveZOrder();
    void setAutoScaleAxis(int qwtAxisId);
    bool isAxisUserLocked(int axis) const;
    void setAxisUserLocked(int axis, bool locked);
    void cancelInlineScaleEditor();
    //! Data-independent curve series styling persisted in .qpt templates (B1-4).
    struct TemplateCurveStyle {
        int curveType = LineSymbols;
        int penStyle = int(Qt::SolidLine);
        double lineWidth = 1.5;
        QColor lineColor = Qt::black;
        int brushStyle = int(Qt::NoBrush);
        QColor brushColor = Qt::black;
        int symbolStyle = -1; // QwtSymbol::NoSymbol
        int symbolSize = 7;
        double symbolPenWidth = 1.0;
        QColor symbolEdgeColor = Qt::black;
        QColor symbolFillColor = Qt::black;
        bool symbolHollow = false;
        int symbolSkip = 0;
        int barGap = 0;
        int barOffset = 0;
        int xAxis = QwtPlot::xBottom;
        int yAxis = QwtPlot::yLeft;
    };

    QVector<TemplateCurveStyle> templateCurveStyles() const { return d_template_curve_styles; }
    void setTemplateCurveStyles(const QVector<TemplateCurveStyle> &styles)
    {
        d_template_curve_styles = styles;
    }
    bool applyTemplateCurveStyle(int curveIndex, int styleIndex = -1);
    void applyTemplateCurveStylesToAll();

    QString placeLegendAnchor(LegendAnchor anchor, double *outOverlapScore = nullptr,
                              int marginPx = 12);
    //! Re-anchors an OutsideRight legend relative to the right canvas edge (D1-2).
    bool repositionAnchoredOutsideRightLegend();
    void applySeriesStyleScheme(SeriesStyleScheme scheme);
    static bool isSymbolWhiteHollow(const QwtSymbol &sym);
    void setCurveSymbolHollow(int curveIndex, bool hollowWhite);
    bool isLegendVisible() const;
    void setLegendVisible(bool visible);
    long activeMarkerID() const { return selectedMarker; }

    bool restorePhase5Tag(const QString &line);
    int pendingLinkedParentIndex() const { return d_pending_linked_parent_idx; }
    void clearPendingLinkedParentIndex() { d_pending_linked_parent_idx = -1; }

    void setCategoryLabels(int axis, const QStringList &labels);
    QStringList categoryLabels(int axis) const;
    void setCustomTickLabels(int axis, const QVector<double> &positions,
                             const QStringList &labels, double rotationDeg = 0.0);
    void setWeibullProbabilityAxis(int axis, bool enabled = true);
    bool hasWeibullProbabilityAxis(int axis) const;
    bool hasCustomTickLabels(int axis) const
    {
        return (axis >= 0 && axis < 4) && !d_custom_tick_positions[axis].isEmpty();
    }
    void setupPolarPlot(const QVector<double> &thetaDeg, const QVector<double> &radius,
                        const QString &seriesLabel = QString(), bool thetaInRadians = false);
    bool isPolarPlot() const { return d_is_polar; }
    bool isPoleFigurePlot() const;
    double polarRadiusMax() const { return d_polar_rmax; }
    // View range of a polar plot that differs from the one derived from the radius (set by a
    // style preset that makes room for the legend, or restored from a project). updatePlot()
    // keeps it; adding a series (setupPolarPlot) drops it.
    void setPolarViewRange(double xMin, double xMax, double yMin, double yMax);
    bool hasPolarViewRange() const { return d_polar_view_set; }
    // Range derived from the radius and the number of series (the default view).
    void polarDefaultViewRange(double *xMin, double *xMax, double *yMin, double *yMax) const;
    QString ternaryLabel(int idx) const
    {
        return (idx >= 0 && idx < 3) ? d_ternary_labels[idx] : QString();
    }
    int shadedBandCount() const { return d_shaded_bands.size(); }
    void clearShadedBands();
    //! E11: show (or hide) the right-axis colour bar of the first colour-mapped scatter curve.
    void syncPointColorBar();
    //! E11: first visible curve whose symbols are coloured from a Z column (nullptr if none).
    DataCurve *pointColorMapCurve() const;
    //! E11: number of error-bar curves drawn as translucent bands.
    int errorBandCount() const;
    struct ColumnMaskSnapshot {
        QPointer<Table> table;
        QPointer<Column> column;
        QString colName;
        IntervalAttribute<bool> addedMasks;
        IntervalAttribute<bool> removedMasks;
    };
    bool runWithColumnMaskUndo(const QString &undoLabel, const QList<DataCurve *> &curves,
                               const std::function<void()> &editFn);
    int clearAllCurveMasksWithUndo();
    QString autoPlaceLegend() { return placeLegendAnchor(LegendAutoAvoidData); }
    void setAxisTitleColor(int axis, const QColor &c)
    {
        switch (axis) {
        case QwtPlot::xBottom:
            setXAxisTitleColor(c);
            break;
        case QwtPlot::yLeft:
            setYAxisTitleColor(c);
            break;
        case QwtPlot::yRight:
            setRightAxisTitleColor(c);
            break;
        case QwtPlot::xTop:
            setTopAxisTitleColor(c);
            break;
        default:
            break;
        }
    }

signals:
    void selectedGraph(Graph *);
    void selectionChanged(Graph *layer, const Graph::GraphSelection &sel);
    void curveOrderChanged();
    void curveVisibilityChanged(int index, bool visible);
    void closedGraph();
    void drawTextOff();
    void drawLineEnded(bool);
    void cursorInfo(const QString &);
    void showPlotDialog(int);
    void createTable(const QString &, const QString &, QList<Column *>);

    void viewImageDialog();
    void viewTextDialog();
    void viewLineDialog();
    void viewTitleDialog();
    void modifiedGraph();
    void hiddenPlot(QWidget *);

    void showLayerButtonContextMenu();
    void showContextMenu();
    void showCurveContextMenu(int);
    void showMarkerPopupMenu();

    void showAxisDialog(int);
    void axisDblClicked(int);
    void xAxisTitleDblClicked();
    void yAxisTitleDblClicked();
    void rightAxisTitleDblClicked();
    void topAxisTitleDblClicked();

    void createIntensityTable(const QString &);
    void dataRangeChanged();
    void showFitResults(const QString &);

private:
    //! The title/plot association test of removeCurves() for one curve.
    bool curveUsesColumn(int index, const QString &s);
    //! removeCurve(index), or with \a deleteItem false takes the item out and returns it.
    QwtPlotItem *removeCurveAt(int index, bool deleteItem);
    //! Takes \a items out of the layer into \a tc (see takeCurvesUsingColumns()).
    void takeCurveItems(const QList<QwtPlotItem *> &items, TakenCurves &tc);
    //! List storing pointers to the curves resulting after a fit session, in case the user wants to delete them later on.
    QList<QwtPlotCurve *> d_fit_curves;
    //! Render hint for plot items.
    bool d_antialiasing;
    bool autoScaleFonts;
    QSize hidden_size;
    bool d_scale_on_print, d_print_cropmarks;
    int selectedAxis;
    QStringList axesFormulas;
    //! Stores columns used for axes with text labels or time/date format info
    QStringList axesFormatInfo;
    QList<AxisType> axisType;
    MarkerType selectedMarkerType;
    QwtPlotMarker::LineStyle mrklStyle;

    //! Stores the step the user specified for the four scale. If step = 0.0, the step will be calculated automatically by the Qwt scale engine.
    QVector<double> d_user_step;
    //! Curve types
    QVector<int> c_type;
    //! Curves on plot keys
    QVector<int> c_keys;
    //! Arrows/lines on plot keys
    QVector<int> d_lines;
    //! Images on plot keys
    QVector<int> d_images;
    //! Stores the identifiers (keys) of the text objects on the plot
    QVector<int> d_texts;

    QPen mrkLinePen;
    QFont auxMrkFont, defaultMarkerFont;
    QColor auxMrkColor, auxMrkBkgColor;
    QPoint auxMrkStart, auxMrkEnd;
    Qt::PenStyle auxMrkStyle;
    QString auxMrkFileName, auxMrkText;

    int n_curves;
    int widthLine, defaultMarkerFrame;
    QColor defaultTextMarkerColor, defaultTextMarkerBackground;
    int auxMrkAngle, auxMrkBkg, auxMrkWidth;
    int auxArrowHeadLength, auxArrowHeadAngle;
    long selectedMarker, legendMarkerID;
    bool startArrowOn, endArrowOn, drawTextOn, drawLineOn, drawArrowOn;

    bool auxFilledArrowHead, ignoreResize;
    bool drawAxesBackbone, m_autoscale;
    bool d_is_ternary;

    QColor defaultArrowColor;
    int defaultArrowLineWidth, defaultArrowHeadLength, defaultArrowHeadAngle;
    bool defaultArrowHeadFill;
    Qt::PenStyle defaultArrowLineStyle;

    //! The markers selected for move/resize operations or NULL if none are selected.
    QPointer<SelectionMoveResizer> d_markers_selector;
    //! The current curve selection, or NULL if none is active.
    QPointer<RangeSelectorTool> d_range_selector;
    //! The currently active tool, or NULL for default (pointer).
    PlotToolInterface *d_active_tool;

    bool d_axis_backbone[4];
    QVector<ReferenceLineSpec> d_ref_lines;
    QwtPlotItem *d_ref_line_item = nullptr;
    QwtPlotItem *d_ref_fill_item = nullptr;
    QPointer<Graph> d_linked_parent;
    int d_x_link_mode = 0;
    int d_y_link_mode = 0;
    QString d_x_link_expr = QString("x");
    QString d_y_link_expr = QString("y");
    bool d_syncing_link = false;

    bool d_is_exporting = false;
    AxisBreakSpec d_axis_breaks[4];
    AxisBreakSpec d_suspended_axis_breaks[4];
    QVector<TemplateCurveStyle> d_template_curve_styles;
    double d_tick_divide_factor[4] = { 1.0, 1.0, 1.0, 1.0 };
    QString d_tick_prefix[4];
    QString d_tick_suffix[4];
    QString d_ternary_labels[3] = { QString("A"), QString("B"), QString("C") };
    bool d_is_polar = false;
    double d_polar_rmax = 1.0;
    bool d_polar_view_set = false;
    double d_polar_view[4] = { 0.0, 0.0, 0.0, 0.0 }; // x min, x max, y min, y max
    QwtPlotItem *d_ternary_item = nullptr;
    QwtPlotItem *d_polar_item = nullptr;
    QStringList d_custom_category_labels[4];
    QVector<double> d_custom_tick_positions[4];
    QStringList d_custom_tick_labels[4];
    double d_custom_tick_rotation[4] = { 0.0, 0.0, 0.0, 0.0 };
    bool d_weibull_prob_axis[4] = { false, false, false, false };
    void syncCustomAndWeibullAxes();
    struct PolarSeriesSpec {
        QVector<double> x;
        QVector<double> y;
        QString label;
        QColor color;
        double width = 2.0;
    };
    QVector<PolarSeriesSpec> d_polar_series;
    struct ShadedBandSpec {
        QVector<double> x;
        QVector<double> yLow;
        QVector<double> yHigh;
        QColor fillColor;
        QString title;
        QwtPlotItem *item = nullptr;
    };
    QVector<ShadedBandSpec> d_shaded_bands;
    bool d_point_colorbar_active = false;
    int d_pending_linked_parent_idx = -1;
    bool d_axis_title_auto[4] = { true, true, true, true };
    Column::UnitFormat d_axis_unit_format = Column::UnitFormat::Paren;
    bool d_refreshing_auto_titles = false;
    mutable QMap<int, QString> d_last_auto_curve_labels;
    GraphSelection d_selection;
    QWidget *d_selection_overlay = nullptr;
    bool d_legend_visible = true;
    QVector<AxisViewportState> d_zoom_history;
    int d_zoom_history_pos = 0;
    bool d_applying_zoom_history = false;
    AxisViewportState captureAxisViewportState() const;
    void applyAxisViewportState(const AxisViewportState &st);
    QString resolvedAutoAxisTitle(int axis, Column::UnitFormat fmt) const;
    bool matchesAutoAxisTitleCandidate(int axis, const QString &text) const;
    bool isAuxiliaryOrFitCurve(const QwtPlotCurve *pc, bool skipHiddenTables = true) const;
    void applyZeroSnapOnAutoAxes();
    void applyLogAutoScalePositiveRange();
};
#endif // GRAPH_H
