/***************************************************************************
    File                 : OriginGadgets.h
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Description          : OriginPro-style Interactive ROI Gadgets & Vertical Cursor
 ***************************************************************************/

#ifndef ORIGIN_GADGETS_H
#define ORIGIN_GADGETS_H

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "PlotToolInterface.h"

#include <QFrame>
#include <QObject>
#include <QPair>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QString>
#include <QVector>

class ApplicationWindow;
class DataCurve;
class Graph;
class Matrix;
class MultiLayer;
class QwtPlotCurve;
class QwtPlotSpectrogram;
class Spectrogram;
class Table;

class RoiOverlayItem;
class VerticalCursorOverlayItem;
class DeltaCursorOverlayItem;
class ImageProfileOverlayItem;

class RoiGadgetTool : public QObject, public PlotToolInterface
{
    Q_OBJECT
public:
    enum GadgetMode {
        RegionStats = 0,
        QuickFit = 1,
        QuickPeaks = 2,
        QuickSigmoidal = 3,
        Integrate = 4,
        Interpolate = 5,
        FFT = 6,
        RiseTime = 7,
        Cluster = 8,
        MaskRegion = 9,
        UnmaskRegion = 10,
        Differentiate = 11,
        Intersect = 12,
        Integrate2D = 13
    };
    enum QuickFitModel {
        FitLinear = 0,
        FitPoly2 = 1,
        FitGauss = 2,
        FitLorentz = 3,
        FitExpDec1 = 4,
        FitBoltzmann = 5
    };
    enum IntegrateBaseline {
        BaseYZero = 0,
        BaseEndpointsLine = 1,
        BaseRoiMin = 2
    };

    RoiGadgetTool(ApplicationWindow *app, Graph *graph, GadgetMode mode = RegionStats);
    ~RoiGadgetTool() override;
    RTTI rtti() const override { return RoiGadget; }

    GadgetMode gadgetMode() const { return d_mode; }
    void setGadgetMode(GadgetMode mode);
    void setQuickFitModel(QuickFitModel model);
    QuickFitModel quickFitModel() const { return d_fit_model; }

    void setIntegrateBaseline(IntegrateBaseline b);
    IntegrateBaseline integrateBaseline() const { return d_integrate_baseline; }

    void setDetectNegativePeaks(bool on);
    bool detectNegativePeaks() const { return d_detect_negative_peaks; }

    void setSavGolWindow(int windowSize);
    int savGolWindow() const { return d_sg_window; }

    void keepFittedCurveOnGraph();

    QRectF roiRect() const { return d_roi_rect; }
    void setRoiRect(const QRectF &dataRect);
    void setTargetCurve(const QString &curveTitle);
    QString targetCurveTitle() const { return d_curve_title; }

    void recalculate();
    QString summaryText() const { return d_summary_text; }
    double primaryMetric() const { return d_primary_metric; }
    double secondaryMetric() const { return d_secondary_metric; }
    Table *createOutputTable(const QString &tableName = QString());
    int applyClusterMask(bool mask = true);

    static RoiGadgetTool *attachGadget(ApplicationWindow *app, Graph *g, GadgetMode mode);
    static void clearGadget(Graph *g);

    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSourceTableModified(Table *t, const QString &colName);

private:
    friend class RoiOverlayItem;

    enum DragMode {
        DragNone = 0,
        DragMove,
        DragLeft,
        DragRight,
        DragTop,
        DragBottom,
        DragTopLeft,
        DragTopRight,
        DragBottomLeft,
        DragBottomRight,
        DragRubberBand
    };

    struct DetectedPeak {
        double xc = 0.0;
        double yTop = 0.0;
        double yBase = 0.0;
        double height = 0.0;
        double fwhm = 0.0;
        double area = 0.0;
    };

    struct DiffRow {
        double x = 0.0;
        double y = 0.0;
        double dydx = 0.0;
        double d2ydx2 = 0.0;
    };

    struct IntersectionRow {
        QString curveA;
        QString curveB;
        double x = 0.0;
        double y = 0.0;
    };

    QwtPlotCurve *resolveTargetCurve();
    DataCurve *resolveTargetDataCurve();
    void initDefaultRoi();
    DragMode hitTestPixelPos(const QPoint &pos) const;
    void updateCursorForPos(const QPoint &pos);
    void showRoiContextMenu(const QPoint &globalPos, bool insideRoi = true);
    void setupFloatingBar();
    void updateFloatingBar();

    QPointer<ApplicationWindow> d_app;
    QPointer<Graph> d_graph_guard;
    GadgetMode d_mode;
    QuickFitModel d_fit_model;
    IntegrateBaseline d_integrate_baseline = BaseYZero;
    bool d_detect_negative_peaks = false;
    int d_sg_window = 5;
    QRectF d_roi_rect;
    QString d_curve_title;
    bool d_explicit_curve;

    QString d_summary_text;
    double d_primary_metric;
    double d_secondary_metric;
    bool d_recalculating;

    RoiOverlayItem *d_overlay_item;
    QPointer<QFrame> d_floating_bar;

    // Live overlay data populated by recalculate()
    QVector<QPointF> d_overlay_curve_pts;
    QVector<QPointF> d_integrate_poly_pts;
    QVector<DetectedPeak> d_detected_peaks;
    QVector<QPointF> d_cluster_pts;
    QVector<QPair<QString, double>> d_output_metrics;
    QVector<QPointF> d_fft_spectrum_pts;
    double d_baseline_y0 = 0.0;
    double d_baseline_y1 = 0.0;
    bool d_has_baseline = false;
    double d_thresh_y10 = 0.0;
    double d_thresh_y90 = 0.0;
    double d_rise_x10 = 0.0;
    double d_rise_x90 = 0.0;
    bool d_has_rise_markers = false;
    double d_interp_x = 0.0;
    double d_interp_y = 0.0;
    double d_interp_slope = 0.0;
    bool d_has_interp_marker = false;
    double d_ec50_x = 0.0;
    double d_ec50_y = 0.0;
    bool d_has_ec50_marker = false;
    QVector<DiffRow> d_diff_rows;
    QVector<QPointF> d_diff_preview_pts;
    double d_diff_max_x = 0.0;
    double d_diff_max_y = 0.0;
    double d_diff_signed_slope = 0.0;
    bool d_has_diff_marker = false;
    QVector<IntersectionRow> d_intersections;
    double d_centroid_x = 0.0;
    double d_centroid_y = 0.0;
    double d_peak2d_x = 0.0;
    double d_peak2d_y = 0.0;
    bool d_has_2d_markers = false;

    // Mouse drag state
    DragMode d_drag_mode;
    QPoint d_drag_start_pos;
    QRectF d_drag_start_roi;
};

class VerticalCursorTool : public QObject, public PlotToolInterface
{
    Q_OBJECT
public:
    VerticalCursorTool(ApplicationWindow *app, Graph *graph, bool syncAllLayers = true);
    ~VerticalCursorTool() override;
    RTTI rtti() const override { return VerticalCursor; }

    double cursorX() const { return d_cursor_x; }
    void setCursorX(double x);
    bool syncAllLayers() const { return d_sync_layers; }
    void setSyncAllLayers(bool sync);
    QString readoutText() const { return d_readout; }
    QVector<QPair<QString, double>> interpolatedValues() const;

    static VerticalCursorTool *attachCursor(ApplicationWindow *app, Graph *g,
                                            bool syncAllLayers = true);

    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    friend class VerticalCursorOverlayItem;

    MultiLayer *findParentMultiLayer() const;
    QList<Graph *> targetGraphs() const;
    void rebuildOverlaysAndFilters();
    void detachAllOverlaysAndFilters();
    void updateReadoutAndReplot();
    static bool interpolateCurveAtX(QwtPlotCurve *c, double xQuery, double &yOut);
    QVector<QPair<QString, double>> interpolatedValuesForGraph(Graph *g) const;
    void showCursorContextMenu(const QPoint &globalPos);
    Table *createReadoutTable();
    void setupFloatingBar();
    void updateFloatingBar();

    QPointer<ApplicationWindow> d_app;
    QPointer<Graph> d_graph_guard;
    double d_cursor_x;
    bool d_sync_layers;
    QString d_readout;
    bool d_dragging;

    QList<QPointer<Graph>> d_filtered_graphs;
    QList<QPointer<VerticalCursorOverlayItem>> d_overlay_items;
    QPointer<QFrame> d_floating_bar;
};

class DeltaCursorTool : public QObject, public PlotToolInterface
{
    Q_OBJECT
public:
    enum SnapMode {
        SnapInterpolateCurve = 0,
        SnapNearestPoint = 1,
        FreeScreenCrosshair = 2
    };
    struct Readout {
        double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;
        double dx = 0.0, dy = 0.0, slope = 0.0, ratio = 0.0;
        double area = 0.0, rms = 0.0, minY = 0.0, maxY = 0.0;
    };

    DeltaCursorTool(ApplicationWindow *app, Graph *graph, SnapMode mode = SnapInterpolateCurve);
    ~DeltaCursorTool() override;
    RTTI rtti() const override { return DeltaCursor; }

    void setCursorPositions(double x1, double x2);
    void setCursorPoints2D(double x1, double y1, double x2, double y2);
    void setActiveCursor(int idx);
    int activeCursor() const { return d_active_cursor; }
    void setHorizontalCrosshairEnabled(bool on);
    bool horizontalCrosshairEnabled() const { return d_horiz_crosshair; }
    void setSnapMode(SnapMode mode);
    SnapMode snapMode() const { return d_snap_mode; }
    void setTargetCurve(const QString &curveTitle);
    QString targetCurveTitle() const { return d_curve_title; }
    Readout currentReadout() const { return d_readout; }
    QString readoutText() const { return d_readout_text; }
    void stampCalloutOnGraph();
    Table *createReadoutTable();

    static DeltaCursorTool *attach(ApplicationWindow *app, Graph *g,
                                   SnapMode mode = SnapInterpolateCurve);

    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSourceTableModified(Table *t, const QString &colName);

private:
    friend class DeltaCursorOverlayItem;

    enum DragTarget {
        DragNone = 0,
        DragCursor1,
        DragCursor2,
        DragSpan
    };

    QwtPlotCurve *resolveTargetCurve();
    void initDefaultPositions();
    void recalculate();
    DragTarget hitTestPixelPos(const QPoint &pos) const;
    void updateCursorForPos(const QPoint &pos);
    void showContextMenu(const QPoint &globalPos);
    void setupFloatingBar();
    void updateFloatingBar();

    QPointer<ApplicationWindow> d_app;
    QPointer<Graph> d_graph_guard;
    SnapMode d_snap_mode = SnapInterpolateCurve;
    int d_active_cursor = 1;
    int d_snap_idx1 = -1;
    int d_snap_idx2 = -1;
    bool d_horiz_crosshair = true;
    QString d_curve_title;
    double d_x1 = 0.0;
    double d_y1 = 0.0;
    double d_x2 = 0.0;
    double d_y2 = 0.0;
    Readout d_readout;
    QString d_readout_text;
    QVector<QPointF> d_span_poly_pts;
    DeltaCursorOverlayItem *d_overlay_item = nullptr;
    QPointer<QFrame> d_floating_bar;

    DragTarget d_drag_target = DragNone;
    QPoint d_drag_start_pos;
    double d_drag_start_x1 = 0.0;
    double d_drag_start_y1 = 0.0;
    double d_drag_start_x2 = 0.0;
    double d_drag_start_y2 = 0.0;
};

class ImageProfileGadgetTool : public QObject, public PlotToolInterface
{
    Q_OBJECT
public:
    ImageProfileGadgetTool(Graph *graph, ApplicationWindow *app = nullptr);
    ~ImageProfileGadgetTool() override;
    RTTI rtti() const override { return ImageProfileGadget; }

    void setCrosshairPosition(double x, double y);
    QPointF crosshairPosition() const { return QPointF(d_x0, d_y0); }
    double currentZ() const { return d_z0; }
    double horizontalFwhm() const { return d_horiz_fwhm; }
    double verticalFwhm() const { return d_vert_fwhm; }
    QVector<QPointF> horizontalProfile() const { return d_horiz_profile; }
    QVector<QPointF> verticalProfile() const { return d_vert_profile; }
    Table *extractProfilesToTableAndGraph(bool createGraph = true);

    void setTargetSpectrogramIndex(int idx);
    int targetSpectrogramIndex() const { return d_spec_index; }
    QString readoutText() const { return d_readout_text; }
    void stampCalloutOnGraph();

    static ImageProfileGadgetTool *attach(Graph *g, ApplicationWindow *app = nullptr);

    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSourceMatrixModified();

private:
    friend class ImageProfileOverlayItem;

    enum DragTarget {
        DragNone = 0,
        DragIntersection,
        DragHorizontal,
        DragVertical
    };

    QwtPlotSpectrogram *resolveTargetSpectrogram(Spectrogram **outSciSpec = nullptr) const;
    QList<QwtPlotSpectrogram *> availableSpectrograms() const;
    QRectF spectrogramDomainRect(QwtPlotSpectrogram *qwtSpec, Matrix *m) const;
    void initDefaultPosition();
    void recalculate();
    DragTarget hitTestPixelPos(const QPoint &pos) const;
    void updateCursorForPos(const QPoint &pos);
    void showContextMenu(const QPoint &globalPos);
    void setupFloatingBar();

    QPointer<ApplicationWindow> d_app;
    QPointer<Graph> d_graph_guard;
    QPointer<Matrix> d_connected_matrix;
    int d_spec_index = 0;
    double d_x0 = 0.0;
    double d_y0 = 0.0;
    double d_z0 = 0.0;
    double d_horiz_min = 0.0;
    double d_horiz_max = 0.0;
    double d_horiz_peak_x = 0.0;
    double d_horiz_fwhm = 0.0;
    double d_horiz_fwhm_left = 0.0;
    double d_horiz_fwhm_right = 0.0;
    double d_vert_min = 0.0;
    double d_vert_max = 0.0;
    double d_vert_peak_y = 0.0;
    double d_vert_fwhm = 0.0;
    double d_vert_fwhm_left = 0.0;
    double d_vert_fwhm_right = 0.0;
    QVector<QPointF> d_horiz_profile;
    QVector<QPointF> d_vert_profile;
    QString d_readout_text;
    ImageProfileOverlayItem *d_overlay_item = nullptr;
    QPointer<QFrame> d_floating_bar;
    DragTarget d_drag_target = DragNone;
};

#endif // ORIGIN_GADGETS_H
