/***************************************************************************
    File                 : MultiLayer.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief,
                           Tilman Benkert,
                           Knut Franke
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net,
                           knut.franke*gmx.de
    Description          : Multi layer widget

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
#ifndef MULTILAYER_H
#define MULTILAYER_H

#include "MyWidget.h"
#include "Graph.h"
#include <QPushButton>
#include <QLayout>
#include <QPointer>
#include <QHash>
#include <QMap>
#include <QPair>
#include <QRectF>
#include <QImage>
#include "core/column/Column.h"

class QWidget;
class QFrame;
class QLabel;
class QToolButton;
class LayerButton;
class SelectionMoveResizer;
class ApplicationWindow;
class QUndoStack;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QShowEvent;
class QTimer;

/**
 * \brief An MDI window (MyWidget) managing one or more Graph objects.
 *
 * %Note that several parts of the code, as well as the user interface, refer to MultiLayer as
 * "graph" or "plot", practically guaranteeing confusion with the classes Graph and Plot.
 *
 * \section future_plans Future Plans
 * Manage any QWidget instead of only Graph.
 * This would allow 3D graphs to be added as well, so you could produce mixed 2D/3D arrangements.
 * It would also allow text labels to be added directly instead of having to complicate things by
 * wrapping them up in a Graph (see documentation of ImageMarker for details) (see documentation of
 * ImageMarker for details).
 *
 * The main problem to be figured out for this is how Graph would interface with the rest of the
 * project. A possible solution is outlined in the documentation of ApplicationWindow: If MultiLayer
 * exposes its parent Project to the widgets it manages, they could handle things like creating
 * tables by calling methods of Project instead of sending signals.
 */
class MultiLayer : public MyWidget
{
    Q_OBJECT

public:
    MultiLayer(const QString &label, QWidget *parent = 0, const QString name = QString(),
               Qt::WindowFlags f = Qt::Widget);
    QWidgetList graphPtrs() { return graphsList; };
    QList<Graph *> layersList() const;
    Graph *layer(int num);
    QWidgetList layerWidgets() const { return graphsList; }
    LayerButton *addLayerButton();
    void copy(ApplicationWindow *parent, MultiLayer *ml);

    enum HorAlignement { HCenter, Left, Right };
    enum VertAlignement { VCenter, Top, Bottom };
    enum PageUnit { UnitPx = 0, UnitMm = 1, UnitCm = 2, UnitInch = 3 };
    enum StylePreset {
        StyleISIJJournal85mm = 0,
        StylePresentationSlide28pt = 1
    };

    struct GraphStylePresetSpec {
        QString id;
        QString name;
        bool builtIn = false;
        double pageWidthMm = 85.0;
        double pageHeightMm = 85.0;
        bool keepAspectRatio = true;
        int targetDpi = 600;
        int marginPx = 6;
        int framePreset = 0; // Graph::FramePreset
        int axisLineWidth = 2;
        int minorTickLen = 4;
        int majorTickLen = 8;
        QString fontFamily = QStringLiteral("Arial");
        double axisTitlePt = 11.0;
        double tickLabelPt = 10.0;
        double legendPt = 9.0;
        bool boldTitle = false;
        bool boldTick = false;
        bool boldLegend = false;
        //! Target curve line width (px) / symbol size (px). Since E2 these are set values, not
        //! minimums: switching presets moves them up or down. Curves whose width/size the user
        //! changed by hand (i.e. not equal to the app default or any preset value) are left alone.
        //! The member names are kept for Python/JSON compatibility.
        int minCurveWidth = 2;
        double curveWidthF = 0.0; //!< Fractional curve width (0.0 = use minCurveWidth)
        int minSymbolSize = 7;
        int legendFrame = 0; // Legend::FrameStyle
        QColor pageBackground = Qt::white;
        bool formatIsijUnitSlash = true;
        bool largeSlideLayout = false;
        //! Size on the slide when the graph is sent to PowerPoint, in percent of the page size
        //! (E8; the lab slide preset is made to be pasted at 140 %). 0 = not set (100 %; a
        //! user copy of a built-in preset inherits the built-in value).
        double slidePastePercent = 0.0;
        //! Presets read from the shared (lab) folder are read-only in this installation.
        bool readOnly = false;
        QString sourcePath;
    };

    //! How a preset's page size is scaled when the graph has several panels (E7).
    enum MultiPanelSizing {
        PanelSizingPerPanel = 0, //!< page = preset width x columns, preset height x rows
        PanelSizingTwoColumn175 = 1 //!< page width fixed to 175 mm (journal double column)
    };
    static int stylePresetFormatVersion() { return 2; }
    //! Preset applied automatically to newly created graphs ("" or "none" = off).
    static QString newGraphStylePresetId();
    static void setNewGraphStylePresetId(const QString &id);
    //! Optional shared folder whose *.json presets are merged read-only.
    static QString sharedStylePresetsFolder();
    static void setSharedStylePresetsFolder(const QString &folder);
    static MultiPanelSizing multiPanelSizing();
    static void setMultiPanelSizing(MultiPanelSizing mode);
    //! Schedules the new-graph preset for the window owning \a g (called from polishGraph).
    static void requestNewGraphStylePreset(Graph *g);
    //! Applies the new-graph preset now if one is pending; returns true if applied.
    bool applyPendingNewGraphStylePreset();
    //! Warnings from the last preset application (e.g. axis titles wider than the axis).
    QStringList lastStylePresetWarnings() const { return d_last_preset_warnings; }

    void applyStylePreset(StylePreset preset);
    void applyStylePresetSpec(const GraphStylePresetSpec &spec);
    bool applyStylePresetById(const QString &idOrName);
    GraphStylePresetSpec captureStylePresetSpec(const QString &name = QString()) const;

    static QString userStylePresetsFilePath();
    static QVector<GraphStylePresetSpec> defaultBuiltInStylePresets();
    static QVector<GraphStylePresetSpec> listStylePresets();
    static bool findStylePreset(const QString &idOrName, GraphStylePresetSpec *outSpec);
    static bool saveStylePreset(const GraphStylePresetSpec &spec, QString *errOut = nullptr);
    static bool deleteStylePreset(const QString &id, QString *errOut = nullptr);
    static bool resetBuiltInStylePreset(const QString &id, QString *errOut = nullptr);
    static bool exportStylePresetsToJson(const QString &filePath, QString *errOut = nullptr);
    static int importStylePresetsFromJson(const QString &filePath, QString *errOut = nullptr);
    static void showStylePresetManagerDialog(ApplicationWindow *app,
                                             MultiLayer *targetGraph = nullptr);
    bool restorePhase5Tag(const QString &line);
    void finalizePhase5Restore();

    void setPageViewMode(bool enabled);
    bool pageViewMode() const;
    void setPageZoom(double factor);
    double pageZoom() const;
    Q_INVOKABLE void fitPageZoomToViewport();
    Q_INVOKABLE double fitPageZoomFactor() const;
    Q_INVOKABLE QString appliedStylePresetId() const;
    Q_INVOKABLE void setInsetZoomDataRect(Graph *insetLayer, const QRectF &roi);
    bool exportToFileChecked(const QString &fileName, QString *errOut = nullptr);
    bool exportImageChecked(const QString &fileName, int quality = -1, bool transparent = false,
                            int dpi = 0, QString *errOut = nullptr);
    bool exportSVGChecked(const QString &fname, QString *errOut = nullptr);
    bool exportVectorChecked(const QString &fileName, int res = 0, bool color = true,
                             bool keepAspect = true,
                             QPageSize pageSize = QPageSize(QPageSize::Custom),
                             QPageLayout::Orientation orientation = QPageLayout::Portrait,
                             QString *errOut = nullptr);
    void setPageSizePhysical(double widthVal, double heightVal, PageUnit unit, int dpi = 300);
    int pageDpi() const;
    PageUnit pageUnit() const;
    double pageWidthPhysical() const { return d_page_width_val; }
    double pageHeightPhysical() const { return d_page_height_val; }
    QColor pageBackgroundColor() const { return d_page_bg_color; }
    void setPageBackgroundColor(const QColor &color);
    void fitPageToLayers(int marginPx = 14);
    //! Page size (in the page unit) arrangeLayers last widened the page from, or an empty size;
    //! cleared by takeLayoutPageGrowth().
    QSizeF takeLayoutPageGrowth();
    //! While on, arrangeLayers may enlarge the page when the grid cannot fit even at the
    //! minimum plot-area size (only explicit layout requests such as graph.arrange_layers turn
    //! it on; page size changes, presets, rescaling and window resizes never enlarge the page).
    void setAllowPageGrowthForLayout(bool on) { d_allow_page_growth_for_layout = on; }
    bool allowPageGrowthForLayout() const { return d_allow_page_growth_for_layout; }
    //! One status-bar line saying the page was enlarged from @p from (in the page unit) to the
    //! current page size so that every panel fits.
    QString layoutPageGrowthMessage(const QSizeF &from) const;
    void fitLayersToPage();
    void syncChildOverlayAndInsetGeometries();
    void showPageSetupDialog();

    Graph *addLinkedLayer(Graph *parentLayer = nullptr, bool linkX = true, bool linkY = false,
                          const QString &xFormula = QString("x"),
                          const QString &yFormula = QString("y"));
    Graph *addInsetLayer(Graph *sourceLayer = nullptr, const QRectF &roiDataRect = QRectF(),
                         bool showConnectors = true);
    bool isInsetLayer(const Graph *g) const;
    Graph *insetSourceLayer(const Graph *g) const;
    QRectF insetNormGeo(const Graph *g) const;
    QRectF insetZoomDataRect(const Graph *g) const;
    bool insetShowConnectors(const Graph *g) const;
    void setInsetShowConnectors(Graph *insetLayer, bool show);

    //! \name Event Handlers
    //@{
    void mousePressEvent(QMouseEvent *);
    void contextMenuEvent(QContextMenuEvent *);
    void wheelEvent(QWheelEvent *);
    void keyPressEvent(QKeyEvent *);
    bool eventFilter(QObject *object, QEvent *);
    void releaseLayer();
    void clearLayerSelector();

    bool focusNextPrevChild(bool next);
    //@}

    bool scaleLayersOnPrint() { return d_scale_on_print; };
    void setScaleLayersOnPrint(bool on) { d_scale_on_print = on; };

    bool printCropmarksEnabled() { return d_print_cropmarks; };
    void printCropmarks(bool on) { d_print_cropmarks = on; };

    bool keepAspectRatio() const { return d_keep_aspect_ratio; }
    double targetAspectRatio() const { return d_target_aspect_ratio; }

    QFrame *layerSwitcherPill() const { return d_layer_pill_frame; }
    QToolButton *canvasStatusPill() const { return d_canvas_status_pill; }
    void updateCanvasStatusPill();

    enum PanelLabelPlacement { PanelOutsideTopLeft = 0, PanelInsideTopLeft = 1 };
    static constexpr const char *kColumnMimeType = "application/x-torigraph-columns";

    QUndoStack *undoStack() const { return d_undo_stack; }
    bool isRestoringSnapshot() const { return d_is_restoring_snapshot; }
    void beginSuppressUndo();
    void endSuppressUndo();
    bool isUndoSuppressed() const { return d_undo_suppress_depth > 0; }
    QString captureUndoSnapshot();
    void pushUndoSnapshot(const QString &label, const QString &beforeSnapshot, int mergeId = -1);

    //! A layer as a snapshot restore rebuilds it: its id (Graph::layerId()), the last curve
    //! key it gave out and, in order, the key and Graph::curveIdentity() of each curve.
    struct LayerIdentity
    {
        int id = 0;
        int lastKey = 0;
        QVector<int> keys;
        QStringList curves;
    };
    //! The identities of the layers now, in order.
    QVector<LayerIdentity> layerIdentities();
    //! Rebuilds every layer from \a snapshot. The rebuilt layers stand for the layers the
    //! snapshot was taken of: they get their ids and their curves the keys they had then
    //! (\a ids: those layers' identities; if not given, those of a recent captureUndoSnapshot()
    //! of the same text, or else the layers now when there are as many), so that curves held
    //! out of a layer (column removal, table removal) find it again.
    bool restoreSnapshotInPlace(ApplicationWindow *app, const QString &snapshot,
                                int preferredActiveLayer = -1, bool restoreGeometry = false,
                                const QVector<LayerIdentity> *ids = nullptr);
    //! The layer identities recorded with a recent captureUndoSnapshot() that gave \a snapshot,
    //! or null.
    const QVector<LayerIdentity> *capturedLayerIdentities(const QString &snapshot) const;
    //! `tableColNames`: columns of the renamed table; when given, only references to those
    //! columns (`oldName_<column>`) are rewritten (not those of a table named oldName_...).
    void remapUndoSnapshotsForTableRename(const QString &oldName, const QString &newName,
                                          const QStringList *tableColNames = nullptr);
    void remapUndoSnapshotsForColRename(const QString &oldColName, const QString &newColName);
    void remapUndoSnapshotsForMatrixRename(const QString &oldName, const QString &newName);

    //!  makeRoom: for outside labels on a multi-panel layout, move panels down (and grow the
    //! page) when the existing top margin / row gap cannot hold the labels. Callers that
    //! re-arrange the layers right afterwards pass false.
    void setPanelLabels(bool enabled, PanelLabelPlacement placement = PanelOutsideTopLeft,
                        const QString &pattern = QString("(%a)"), int fontPt = 0,
                        bool makeRoom = true);
    bool hasPanelLabels() const { return d_panel_labels_enabled; }
    bool panelLabelsEnabled() const { return d_panel_labels_enabled; }
    QRect panelLabelsBoundingRect() const;
    struct PanelLabelBox
    {
        Graph *graph = nullptr;
        int row = 0; //!< row of the panel on the page (panelLabelGridCells())
        QRect box;   //!< glyph box in canvas pixels, as panelLabelsBoundingRect() unites them
    };
    //! The panel labels, one per labelled primary panel, in layer order. Empty when panel
    //! labels are off.
    QVector<PanelLabelBox> panelLabelBoxes() const;
    PanelLabelPlacement panelLabelPlacement() const { return d_panel_label_placement; }
    QString panelLabelPattern() const { return d_panel_label_pattern; }
    int panelLabelFontPt() const { return d_panel_label_font_pt; }
    void applySquareCanvasFrame70Percent();
    void notifyOleDirtyIfEmbedded();
    void enforceLayerZOrder();
    void refreshLayerButtonBadges();

    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dropEvent(QDropEvent *e) override;

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

public slots:
    void setKeepAspectRatio(bool keep);
    void setTargetAspectRatio(double ratio);
    void applyAspectRatioPreset(double ratio, bool keepLocked = true);
    void syncAspectRatioAfterManualResize(double newRatio, int canvasW = 0, int canvasH = 0,
                                          bool unlockAspect = true);
    Graph *addLayer(int x = 0, int y = 0, int width = 0, int height = 0);
    void setLayersNumber(int n);

    bool isEmpty();
    void removeLayer();
    void confirmRemoveLayer();

    /*!\brief Start adding a text layer.
     *
     * This works by having #canvas grab the mouse, remembering that we are in the midst of adding
     * text in #addTextOn and dispatching the next mouse click to addTextLayer(const QPoint&) in
     * eventFilter().
     *
     * \sa #defaultTextMarkerFont, #defaultTextMarkerFrame, #defaultTextMarkerColor,
     * #defaultTextMarkerBackground
     */
    void addTextLayer(int f, const QFont &font, const QColor &textCol, const QColor &backgroundCol);
    /*!\brief Finish adding a text layer.
     *
     * An empty Graph is created and added to me.
     * Legend, title and axes are removed and a new Legend is added with a placeholder text.
     *
     * \sa #defaultTextMarkerFont, #defaultTextMarkerFrame, #defaultTextMarkerColor,
     * #defaultTextMarkerBackground, addTextLayer(int,const QFont&,const QColor&,const QColor&)
     */
    void addTextLayer(const QPoint &pos);

    Graph *activeGraph() { return active_graph.data(); };
    void setActiveGraph(Graph *g);
    void activateGraph(LayerButton *button);

    void setGraphGeometry(int x, int y, int w, int h);

    void findBestLayout(int &rows, int &cols);

    QSize arrangeLayers(bool userSize);
    void arrangeLayers(bool fit, bool userSize);

    int getRows() { return rows; };
    void setRows(int r);

    int getCols() { return cols; };
    void setCols(int c);

    int colsSpacing() { return colsSpace; };
    int rowsSpacing() { return rowsSpace; };
    void setSpacing(int rgap, int cgap);

    int leftMargin() { return left_margin; };
    int rightMargin() { return right_margin; };
    int topMargin() { return top_margin; };
    int bottomMargin() { return bottom_margin; };
    void setMargins(int lm, int rm, int tm, int bm);

    QSize layerCanvasSize() { return QSize(l_canvas_width, l_canvas_height); };
    void setLayerCanvasSize(int w, int h);

    int horizontalAlignement() { return hor_align; };
    int verticalAlignement() { return vert_align; };
    void setAlignement(int ha, int va);

    int layers() { return graphs; };
    QWidget *canvasWidget() const { return canvas; };

    //! \name Print and Export
    //@{
    void exportToFile(const QString &fileName);
    void exportImage(const QString &fileName, int quality = -1, bool transparent = false,
                     int dpi = 0);
    void exportSVG(const QString &fname);
    void exportPDF(const QString &fname);
    void exportVector(const QString &fileName, int res = 0, bool color = true,
                      bool keepAspect = true, QPageSize pageSize = QPageSize(QPageSize::Custom),
                      QPageLayout::Orientation orientation = QPageLayout::Portrait);
    void exportPainter(QPaintDevice &paintDevice, bool keepAspect = false, QRect rect = QRect());
    void exportPainter(QPainter &painter, bool keepAspect = false, QRect rect = QRect(),
                       QSize size = QSize());

    void copyAllLayers();
    QImage copyPageAsImage(int dpi = 0);
    void print();
    void printAllLayers(QPainter *painter);
    void printActiveLayer();
    //@}

    void setFonts(const QFont &titleFnt, const QFont &scaleFnt, const QFont &numbersFnt,
                  const QFont &legendFnt);

    void connectLayer(Graph *g);

    QString saveToString(const QString &geometry);
    QString saveAsTemplate(const QString &geometryInfo);

signals:
    void showTextDialog();
    void showPlotDialog(int);
    void showAxisDialog(int);
    void showScaleDialog(int);
    void showGraphContextMenu();
    void showLayerButtonContextMenu();
    void showCurveContextMenu(int);
    void showWindowContextMenu();
    void showCurvesDialog();
    void drawTextOff();
    void drawLineEnded(bool);
    void showXAxisTitleDialog();
    void showYAxisTitleDialog();
    void showTopAxisTitleDialog();
    void showRightAxisTitleDialog();
    void showMarkerPopupMenu();
    void modifiedPlot();
    void pageZoomChanged(double zoom);
    void cursorInfo(const QString &);
    void showImageDialog();
    void showLineDialog();
    void viewTitleDialog();
    void createTable(const QString &, const QString &, QList<Column *>);
    void showGeometryDialog();
    void pasteMarker();
    void createIntensityTable(const QString &);
    void setPointerCursor();
    void selectionChanged(MultiLayer *page, Graph *layer, const Graph::GraphSelection &sel);

private:
    friend class ScopedUnzoomForExport;
    void resizeLayers(const QResizeEvent *re);
    void resizeLayers(const QSize &size, const QSize &oldSize, bool scaleFonts);
    QSize lastSize; // workaround for resize layers after hide/minimize

    QPointer<Graph> active_graph;
    //! Used for resizing of layers.
    int graphs, cols, rows, graph_width, graph_height, colsSpace, rowsSpace;
    int left_margin, right_margin, top_margin, bottom_margin;
    int l_canvas_width, l_canvas_height, hor_align, vert_align;
    //! setPageZoom(): layer canvas size at 100 % and the size the last zoom set.
    QSizeF d_zoom_base_lcanvas;
    QSize d_zoom_applied_lcanvas;
    bool addTextOn;
    bool d_scale_on_print, d_print_cropmarks;
    bool d_keep_aspect_ratio;
    double d_target_aspect_ratio;

    //! Used when adding text markers on new layers
    int defaultTextMarkerFrame;
    QFont defaultTextMarkerFont;
    QColor defaultTextMarkerColor, defaultTextMarkerBackground;

    QWidgetList buttonsList, graphsList;
    QHBoxLayout *layerButtonsBox;
    QFrame *d_layer_pill_frame = nullptr;
    QToolButton *d_add_layer_btn = nullptr;
    QToolButton *d_canvas_status_pill = nullptr;
    QWidget *canvas;

    bool d_page_view_mode = false;
    double d_page_zoom = 1.0;
    PageUnit d_page_unit = UnitPx;
    int d_page_dpi = 300;
    double d_page_width_val = 520.0;
    double d_page_height_val = 520.0;
    QSize d_base_page_px = QSize(520, 520);
    QColor d_page_bg_color = QColor(Qt::white);
    QString d_applied_style_preset_id;
    QStringList d_last_preset_warnings;
    QString pageSetupTagsString(bool includeWindowSize) const;
    struct PendingRecalcLinkSpec
    {
        int layerIndex = -1;
        QString xml;
    };
    QVector<PendingRecalcLinkSpec> d_pending_recalc_links;
    struct InsetSpec
    {
        QPointer<Graph> sourceLayer;
        QRectF normGeo = QRectF(0.54, 0.08, 0.38, 0.38);
        QRectF zoomDataRect;
        bool showConnectors = true;
    };
    QMap<Graph *, InsetSpec> d_inset_specs;
    struct PendingInsetSpec
    {
        int childIdx = -1;
        int sourceIdx = -1;
        QRectF relRect = QRectF(0.54, 0.08, 0.38, 0.38);
        QRectF zoomDataRect;
        bool showConnectors = true;
    };
    QVector<PendingInsetSpec> d_pending_inset_specs;

    void openLayerButtonContextMenu(LayerButton *btn, const QPoint &globalPos);
    void repositionCanvasStatusPill();
    void openCanvasStatusQuickMenu();
    void openQuickLayerColumnsPopup();
    void drawPanelLabels(QPainter *painter, const QPoint &originOffset = QPoint(0, 0)) const;
    //! Outside panel labels on a multi-panel layout: moves rows of panels down (keeping every
    //! panel's size, so no axis range changes) and grows the page when a label would leave the
    //! page top or overlap the axis band of the panel above.
    void makeRoomForOutsidePanelLabels();
    //! Row (y) and column (x) of each primary panel on the page, from where its canvas actually
    //! is (canvas tops / lefts within a small tolerance share a row / column). The outside panel
    //! labels of one row / column are aligned with each other. Taken from the geometry, not
    //! from the layer order: after a panel of a grid is deleted, or with a layer added outside
    //! the grid, the layer order no longer matches the rows on the page. \a updateLayouts
    //! false uses the plots' current layouts (while painting a widget).
    QHash<Graph *, QPoint> panelLabelGridCells(bool updateLayouts = true) const;
    void drawInsetConnectors(QPainter *painter, const QPoint &originOffset = QPoint(0, 0)) const;
    void syncCanvasOverlayVisibility();
    QList<Graph *> orderedLayersForRender() const;
    Graph *resolveColumnDropTarget(const QPoint &canvasPt, const QPoint &widgetPos,
                                   bool &rightYZoneOut) const;
    void installLayerEventFilters(Graph *g);

    bool d_syncing_inset_geometries = false;
    bool d_forwarding_mouse = false;
    bool d_user_zoomed = false;
    bool d_internal_zoom_change = false;
    bool d_initial_viewport_fit_done = false;
    bool d_was_tiled_split = false;
    bool d_suppress_canvas_resize_layout = false;
    //! Set by arrangeLayers(bool) when the panels need more room than the page has (canvas
    //! px of the whole grid, empty when it fits); arrangeLayers(bool, bool) then widens the page.
    QSize d_arrange_overflow_px;
    bool d_growing_page_for_layout = false;
    bool d_allow_page_growth_for_layout = false;
    QSizeF d_layout_page_grown_from;
    QPointer<QWidget> d_passthrough_source;
    QPointer<QWidget> d_passthrough_target;
    QPointer<Graph> d_inset_drag_graph;
    QPoint d_inset_drag_start_global;
    QRect d_inset_drag_start_geo;
    QString d_inset_drag_before_snap;
    bool d_inset_drag_active = false;

    struct CtrlFontGestureBaseline
    {
        bool active = false;
        int baseCanvasH = 0;
        struct GraphFonts
        {
            QPointer<Graph> graph;
            QFont titleFont;
            QFont axisFonts[QwtPlot::axisCnt];
            QFont axisTitleFonts[QwtPlot::axisCnt];
            QMap<int, QFont> textMarkerFonts;
        };
        QVector<GraphFonts> graphs;
    };
    CtrlFontGestureBaseline d_ctrl_font_gesture;
    QTimer *d_ctrl_font_gesture_timer = nullptr;
    QTimer *d_viewport_autofit_timer = nullptr;
    QSize d_last_autofit_vp_size;

    double autoFitTargetZoom() const;
    void autoFitPageZoomToViewport();
    void clearCtrlFontGestureBaseline();
    QImage renderPageToImage(int effectiveDpi, bool transparent, QString *errOut);
    bool exportVectorImpl(const QString &fileName, int res, bool color, bool keepAspect,
                          QPageSize pageSize, QPageLayout::Orientation orientation,
                          bool allowQt5PsFallback, QString *errOut);

    QPointer<SelectionMoveResizer> d_layers_selector;
    QUndoStack *d_undo_stack = nullptr;
    bool d_is_restoring_snapshot = false;
    //! recent captureUndoSnapshot() texts with the layer identities then (newest first)
    QList<QPair<QString, QVector<LayerIdentity>>> d_captured_identities;
    //! the last curve key each layer id gave out, also of layers no longer here
    QHash<int, int> d_layer_last_keys;
    int d_undo_suppress_depth = 0;
    bool d_panel_labels_enabled = false;
    PanelLabelPlacement d_panel_label_placement = PanelOutsideTopLeft;
    QString d_panel_label_pattern = QString("(%a)");
    int d_panel_label_font_pt = 0;
    QWidget *d_drop_zone_overlay = nullptr;
    QToolButton *d_quick_columns_btn = nullptr;
};

//! While alive, arrangeLayers on @p ml may enlarge a page that cannot hold the grid even at the
//! minimum plot-area size; the previous permission is restored on destruction. Only layouts the
//! user asks for explicitly open one (graph.arrange_layers, the Arrange Layers dialog without a
//! fixed canvas size, the final layout of a graph merge, graph.add_layer re-arranging the grid).
class LayoutPageGrowthScope
{
public:
    explicit LayoutPageGrowthScope(MultiLayer *ml)
        : d_ml(ml), d_prev(ml ? ml->allowPageGrowthForLayout() : false)
    {
        if (d_ml)
            d_ml->setAllowPageGrowthForLayout(true);
    }
    ~LayoutPageGrowthScope()
    {
        if (d_ml)
            d_ml->setAllowPageGrowthForLayout(d_prev);
    }
    LayoutPageGrowthScope(const LayoutPageGrowthScope &) = delete;
    LayoutPageGrowthScope &operator=(const LayoutPageGrowthScope &) = delete;

private:
    QPointer<MultiLayer> d_ml;
    bool d_prev;
};

//! Button with layer number
class LayerButton : public QPushButton
{
    Q_OBJECT

public:
    LayerButton(const QString &text = {}, QWidget *parent = 0);
    ~LayerButton() {};

    static int btnSize() { return 26; };

protected:
    void mousePressEvent(QMouseEvent *);
    void mouseDoubleClickEvent(QMouseEvent *);

signals:
    void showCurvesDialog();
    void clicked(LayerButton *);
    void showContextMenu();
};

//! Puts a page at 100 % view zoom for its lifetime and restores the zoom (and window size)
//! afterwards. Every export, copy and print of a page or of one of its layers runs under it, so
//! the output never depends on the Ctrl+wheel view zoom (layer geometry and the zoomed text).
//! Nesting is safe: a guard on a page that is already at 100 % does nothing.
class ScopedUnzoomForExport
{
public:
    explicit ScopedUnzoomForExport(MultiLayer *target);
    ~ScopedUnzoomForExport();
    ScopedUnzoomForExport(const ScopedUnzoomForExport &) = delete;
    ScopedUnzoomForExport &operator=(const ScopedUnzoomForExport &) = delete;
    bool isActive() const { return active; }
    void restoreNow();

private:
    QPointer<MultiLayer> ml;
    double prevZoom = 1.0;
    QPoint prevWinPos;
    QSize prevWinSize;
    // Layer geometry before the unzoom: the 2x / 0.5x round trip of setPageZoom() rounds every
    // layer rectangle, which would shift a layer by a pixel per export.
    QList<QPointer<Graph>> prevLayers;
    QVector<QRect> prevLayerGeometry;
    QVector<QSize> prevPlotSize;
    // Per layer (prevLayers): the axes whose autoscale is paused for the export. Autoscaling
    // adds the symbol size in pixels to the data range (PlotCurve::boundingRect), so redrawing
    // at the smaller 100 % canvas could widen an axis by a tick step (-3..2 became -4..2) and
    // the file would not show the ranges seen on screen.
    QVector<int> pausedAutoAxes;
    QVector<QwtScaleDiv> pausedAutoDivs; // [layer * 4 + axis]: the division held
    int prevLayerCanvasW = 0;
    int prevLayerCanvasH = 0;
    bool prevMlUpdates = true;
    bool prevWidgetUpdates = true;
    bool active = false;
};

//! RAII helper that suppresses MultiLayer undo-stack pushes while in scope.
class ScopedMultiLayerUndoSuppressor
{
public:
    explicit ScopedMultiLayerUndoSuppressor(MultiLayer *ml = nullptr) : d_ml(ml)
    {
        if (d_ml)
            d_ml->beginSuppressUndo();
    }
    ~ScopedMultiLayerUndoSuppressor()
    {
        if (d_ml)
            d_ml->endSuppressUndo();
    }
    ScopedMultiLayerUndoSuppressor(const ScopedMultiLayerUndoSuppressor &) = delete;
    ScopedMultiLayerUndoSuppressor &operator=(const ScopedMultiLayerUndoSuppressor &) = delete;
    ScopedMultiLayerUndoSuppressor(ScopedMultiLayerUndoSuppressor &&other) noexcept
        : d_ml(other.d_ml)
    {
        other.d_ml.clear();
    }
    ScopedMultiLayerUndoSuppressor &operator=(ScopedMultiLayerUndoSuppressor &&other) noexcept
    {
        if (this != &other) {
            if (d_ml)
                d_ml->endSuppressUndo();
            d_ml = other.d_ml;
            other.d_ml.clear();
        }
        return *this;
    }

private:
    QPointer<MultiLayer> d_ml;
};

#endif
