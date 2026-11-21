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
        int minCurveWidth = 2;
        int minSymbolSize = 7;
        int legendFrame = 0; // Legend::FrameStyle
        QColor pageBackground = Qt::white;
        bool formatIsijUnitSlash = true;
        bool largeSlideLayout = false;
    };

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
    void setPageSizePhysical(double widthVal, double heightVal, PageUnit unit, int dpi = 300);
    int pageDpi() const;
    PageUnit pageUnit() const;
    double pageWidthPhysical() const { return d_page_width_val; }
    double pageHeightPhysical() const { return d_page_height_val; }
    QColor pageBackgroundColor() const { return d_page_bg_color; }
    void setPageBackgroundColor(const QColor &color);
    void fitPageToLayers(int marginPx = 14);
    void fitLayersToPage();
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
    QString captureUndoSnapshot();
    void pushUndoSnapshot(const QString &label, const QString &beforeSnapshot, int mergeId = -1);
    bool restoreSnapshotInPlace(ApplicationWindow *app, const QString &snapshot,
                                int preferredActiveLayer = -1, bool restoreGeometry = false);

    void setPanelLabels(bool enabled, PanelLabelPlacement placement = PanelOutsideTopLeft,
                        const QString &pattern = QString("(%a)"), int fontPt = 0);
    bool hasPanelLabels() const { return d_panel_labels_enabled; }
    bool panelLabelsEnabled() const { return d_panel_labels_enabled; }
    QRect panelLabelsBoundingRect() const;
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

    Graph *activeGraph() { return active_graph; };
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
    QImage copyPageAsImage(int dpi = 300);
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
    void resizeLayers(const QResizeEvent *re);
    void resizeLayers(const QSize &size, const QSize &oldSize, bool scaleFonts);
    QSize lastSize; // workaround for resize layers after hide/minimize

    Graph *active_graph;
    //! Used for resizing of layers.
    int graphs, cols, rows, graph_width, graph_height, colsSpace, rowsSpace;
    int left_margin, right_margin, top_margin, bottom_margin;
    int l_canvas_width, l_canvas_height, hor_align, vert_align;
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

    void syncChildOverlayAndInsetGeometries();
    void openLayerButtonContextMenu(LayerButton *btn, const QPoint &globalPos);
    void repositionCanvasStatusPill();
    void openCanvasStatusQuickMenu();
    void openQuickLayerColumnsPopup();
    void drawPanelLabels(QPainter *painter, const QPoint &originOffset = QPoint(0, 0)) const;
    void drawInsetConnectors(QPainter *painter, const QPoint &originOffset = QPoint(0, 0)) const;
    void syncCanvasOverlayVisibility();
    QList<Graph *> orderedLayersForRender() const;
    Graph *resolveColumnDropTarget(const QPoint &canvasPt, const QPoint &widgetPos,
                                   bool &rightYZoneOut) const;
    void installLayerEventFilters(Graph *g);

    bool d_syncing_inset_geometries = false;
    bool d_forwarding_mouse = false;
    QPointer<QWidget> d_passthrough_source;
    QPointer<QWidget> d_passthrough_target;

    QPointer<SelectionMoveResizer> d_layers_selector;
    QUndoStack *d_undo_stack = nullptr;
    bool d_is_restoring_snapshot = false;
    bool d_panel_labels_enabled = false;
    PanelLabelPlacement d_panel_label_placement = PanelOutsideTopLeft;
    QString d_panel_label_pattern = QString("(%a)");
    int d_panel_label_font_pt = 0;
    QWidget *d_drop_zone_overlay = nullptr;
    QToolButton *d_quick_columns_btn = nullptr;
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

#endif
