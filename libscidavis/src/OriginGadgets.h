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
class MultiLayer;
class QwtPlotCurve;
class Table;

class RoiOverlayItem;
class VerticalCursorOverlayItem;

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

    RoiGadgetTool(ApplicationWindow *app, Graph *graph, GadgetMode mode = RegionStats);
    ~RoiGadgetTool() override;
    RTTI rtti() const override { return RoiGadget; }

    GadgetMode gadgetMode() const { return d_mode; }
    void setGadgetMode(GadgetMode mode);
    void setQuickFitModel(QuickFitModel model);
    QuickFitModel quickFitModel() const { return d_fit_model; }

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
    void showRoiContextMenu(const QPoint &globalPos);

    QPointer<ApplicationWindow> d_app;
    QPointer<Graph> d_graph_guard;
    GadgetMode d_mode;
    QuickFitModel d_fit_model;
    QRectF d_roi_rect;
    QString d_curve_title;
    bool d_explicit_curve;

    QString d_summary_text;
    double d_primary_metric;
    double d_secondary_metric;
    bool d_recalculating;

    RoiOverlayItem *d_overlay_item;

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

    QPointer<ApplicationWindow> d_app;
    QPointer<Graph> d_graph_guard;
    double d_cursor_x;
    bool d_sync_layers;
    QString d_readout;
    bool d_dragging;

    QList<QPointer<Graph>> d_filtered_graphs;
    QList<VerticalCursorOverlayItem *> d_overlay_items;
};

#endif // ORIGIN_GADGETS_H
