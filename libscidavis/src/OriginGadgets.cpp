/***************************************************************************
    File                 : OriginGadgets.cpp
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Description          : OriginPro-style Interactive ROI Gadgets & Vertical Cursor
 ***************************************************************************/

#include "OriginGadgets.h"

#include "ApplicationWindow.h"
#include "Graph.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "Table.h"
#include "core/column/Column.h"

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_item.h>
#include <qwt_scale_div.h>
#include <qwt_scale_map.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QTimer>

#include <gsl/gsl_blas.h>
#include <gsl/gsl_fft_complex.h>
#include <gsl/gsl_fit.h>
#include <gsl/gsl_interp.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_multifit_nlin.h>
#include <gsl/gsl_spline.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

struct XYPoint {
    double x = 0.0;
    double y = 0.0;
    int curveIndex = -1;
};

static double computeR2(const std::vector<double> &yObs, const std::vector<double> &yHat)
{
    const size_t n = yObs.size();
    if (n == 0 || yHat.size() != n)
        return 0.0;
    double meanY = 0.0;
    for (size_t i = 0; i < n; ++i)
        meanY += yObs[i];
    meanY /= double(n);

    double rss = 0.0;
    double tss = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double r = yObs[i] - yHat[i];
        const double d = yObs[i] - meanY;
        rss += r * r;
        tss += d * d;
    }
    if (tss <= 1e-30)
        return (rss <= 1e-24) ? 1.0 : 0.0;
    double r2 = 1.0 - (rss / tss);
    if (r2 < 0.0 && rss <= 1e-24)
        r2 = 1.0;
    return r2;
}

static double estimateFwhm(const std::vector<double> &x, const std::vector<double> &y,
                           double yBase, int peakIdx)
{
    const int n = static_cast<int>(x.size());
    if (n < 2 || peakIdx < 0 || peakIdx >= n)
        return (n >= 2) ? std::max(1e-9, (x.back() - x.front()) * 0.25) : 1.0;

    const double yTop = y[size_t(peakIdx)];
    const double yHalf = yBase + 0.5 * (yTop - yBase);
    if (yTop <= yBase)
        return std::max(1e-9, (x.back() - x.front()) * 0.25);

    double xLeft = x.front();
    for (int i = peakIdx; i > 0; --i) {
        if ((y[size_t(i - 1)] <= yHalf && y[size_t(i)] >= yHalf)
            || (y[size_t(i - 1)] >= yHalf && y[size_t(i)] <= yHalf)) {
            const double dy = y[size_t(i)] - y[size_t(i - 1)];
            const double frac = (std::fabs(dy) > 1e-15) ? ((yHalf - y[size_t(i - 1)]) / dy) : 0.5;
            xLeft = x[size_t(i - 1)] + frac * (x[size_t(i)] - x[size_t(i - 1)]);
            break;
        }
    }

    double xRight = x.back();
    for (int i = peakIdx; i < n - 1; ++i) {
        if ((y[size_t(i)] >= yHalf && y[size_t(i + 1)] <= yHalf)
            || (y[size_t(i)] <= yHalf && y[size_t(i + 1)] >= yHalf)) {
            const double dy = y[size_t(i + 1)] - y[size_t(i)];
            const double frac = (std::fabs(dy) > 1e-15) ? ((yHalf - y[size_t(i)]) / dy) : 0.5;
            xRight = x[size_t(i)] + frac * (x[size_t(i + 1)] - x[size_t(i)]);
            break;
        }
    }

    const double w = std::fabs(xRight - xLeft);
    return (w > 1e-12) ? w : std::max(1e-9, (x.back() - x.front()) * 0.2);
}

struct NlFitContext {
    const double *x = nullptr;
    const double *y = nullptr;
    size_t n = 0;
    double xRef = 0.0;
    int modelType = 0; // 2=Gauss, 3=Lorentz, 4=ExpDec1, 5=Boltzmann
};

static int nlFitF(const gsl_vector *params, void *userData, gsl_vector *f)
{
    const auto *ctx = static_cast<const NlFitContext *>(userData);
    for (size_t i = 0; i < ctx->n; ++i) {
        const double xi = ctx->x[i];
        double yiHat = 0.0;
        if (ctx->modelType == 2) {
            // Gauss: y0 + A * exp(-2 * ((x - xc) / w)^2)
            const double y0 = gsl_vector_get(params, 0);
            const double A = gsl_vector_get(params, 1);
            const double xc = gsl_vector_get(params, 2);
            double w = gsl_vector_get(params, 3);
            if (std::fabs(w) < 1e-12)
                w = (w >= 0.0) ? 1e-12 : -1e-12;
            const double u = (xi - xc) / w;
            const double arg = std::max(-500.0, -2.0 * u * u);
            yiHat = y0 + A * std::exp(arg);
        } else if (ctx->modelType == 3) {
            // Lorentz: y0 + H * w^2 / (4*(x - xc)^2 + w^2)
            const double y0 = gsl_vector_get(params, 0);
            const double H = gsl_vector_get(params, 1);
            const double xc = gsl_vector_get(params, 2);
            double w = gsl_vector_get(params, 3);
            if (std::fabs(w) < 1e-12)
                w = 1e-12;
            const double dx = xi - xc;
            const double w2 = w * w;
            const double denom = std::max(1e-30, 4.0 * dx * dx + w2);
            yiHat = y0 + H * (w2 / denom);
        } else if (ctx->modelType == 4) {
            // ExpDec1: y0 + A * exp(-(x - xRef) / t1)
            const double y0 = gsl_vector_get(params, 0);
            const double A = gsl_vector_get(params, 1);
            double t1 = gsl_vector_get(params, 2);
            if (std::fabs(t1) < 1e-12)
                t1 = (t1 >= 0.0) ? 1e-12 : -1e-12;
            const double u = std::clamp((xi - ctx->xRef) / t1, -500.0, 500.0);
            yiHat = y0 + A * std::exp(-u);
        } else {
            // Boltzmann: A2 + (A1 - A2) / (1 + exp((x - x0) / dx))
            const double A1 = gsl_vector_get(params, 0);
            const double A2 = gsl_vector_get(params, 1);
            const double x0 = gsl_vector_get(params, 2);
            double dx = gsl_vector_get(params, 3);
            if (std::fabs(dx) < 1e-12)
                dx = (dx >= 0.0) ? 1e-12 : -1e-12;
            const double u = std::clamp((xi - x0) / dx, -500.0, 500.0);
            const double s = 1.0 / (1.0 + std::exp(u));
            yiHat = A2 + (A1 - A2) * s;
        }
        gsl_vector_set(f, i, yiHat - ctx->y[i]);
    }
    return GSL_SUCCESS;
}

static int nlFitDf(const gsl_vector *params, void *userData, gsl_matrix *J)
{
    const auto *ctx = static_cast<const NlFitContext *>(userData);
    for (size_t i = 0; i < ctx->n; ++i) {
        const double xi = ctx->x[i];
        if (ctx->modelType == 2) {
            const double A = gsl_vector_get(params, 1);
            const double xc = gsl_vector_get(params, 2);
            double w = gsl_vector_get(params, 3);
            if (std::fabs(w) < 1e-12)
                w = (w >= 0.0) ? 1e-12 : -1e-12;
            const double u = (xi - xc) / w;
            const double E = std::exp(std::max(-500.0, -2.0 * u * u));
            gsl_matrix_set(J, i, 0, 1.0);
            gsl_matrix_set(J, i, 1, E);
            gsl_matrix_set(J, i, 2, A * E * (4.0 * u / w));
            gsl_matrix_set(J, i, 3, A * E * (4.0 * u * u / w));
        } else if (ctx->modelType == 3) {
            const double H = gsl_vector_get(params, 1);
            const double xc = gsl_vector_get(params, 2);
            double w = gsl_vector_get(params, 3);
            if (std::fabs(w) < 1e-12)
                w = 1e-12;
            const double dx = xi - xc;
            const double w2 = w * w;
            const double D = std::max(1e-30, 4.0 * dx * dx + w2);
            gsl_matrix_set(J, i, 0, 1.0);
            gsl_matrix_set(J, i, 1, w2 / D);
            gsl_matrix_set(J, i, 2, H * w2 * (8.0 * dx) / (D * D));
            gsl_matrix_set(J, i, 3, H * (8.0 * w * dx * dx) / (D * D));
        } else if (ctx->modelType == 4) {
            const double A = gsl_vector_get(params, 1);
            double t1 = gsl_vector_get(params, 2);
            if (std::fabs(t1) < 1e-12)
                t1 = (t1 >= 0.0) ? 1e-12 : -1e-12;
            const double u = std::clamp((xi - ctx->xRef) / t1, -500.0, 500.0);
            const double E = std::exp(-u);
            gsl_matrix_set(J, i, 0, 1.0);
            gsl_matrix_set(J, i, 1, E);
            gsl_matrix_set(J, i, 2, A * E * (u / t1));
        } else {
            const double A1 = gsl_vector_get(params, 0);
            const double A2 = gsl_vector_get(params, 1);
            const double x0 = gsl_vector_get(params, 2);
            double dx = gsl_vector_get(params, 3);
            if (std::fabs(dx) < 1e-12)
                dx = (dx >= 0.0) ? 1e-12 : -1e-12;
            const double u = std::clamp((xi - x0) / dx, -500.0, 500.0);
            const double E = std::exp(u);
            const double s = 1.0 / (1.0 + E);
            const double ds = s * (1.0 - s);
            gsl_matrix_set(J, i, 0, s);
            gsl_matrix_set(J, i, 1, 1.0 - s);
            gsl_matrix_set(J, i, 2, (A1 - A2) * ds / dx);
            gsl_matrix_set(J, i, 3, (A1 - A2) * ds * (u / dx));
        }
    }
    return GSL_SUCCESS;
}

static int nlFitFdf(const gsl_vector *params, void *userData, gsl_vector *f, gsl_matrix *J)
{
    nlFitF(params, userData, f);
    nlFitDf(params, userData, J);
    return GSL_SUCCESS;
}

static bool runLmSolver(int modelType, const std::vector<double> &x, const std::vector<double> &y,
                        double xRef, std::vector<double> &pInOut)
{
    const size_t n = x.size();
    const size_t p = pInOut.size();
    if (n < p || p == 0)
        return false;

    NlFitContext ctx { x.data(), y.data(), n, xRef, modelType };
    gsl_multifit_function_fdf func;
    func.f = &nlFitF;
    func.df = &nlFitDf;
    func.fdf = &nlFitFdf;
    func.n = n;
    func.p = p;
    func.params = &ctx;

    gsl_vector *initVec = gsl_vector_alloc(p);
    for (size_t j = 0; j < p; ++j)
        gsl_vector_set(initVec, j, pInOut[j]);

    gsl_multifit_fdfsolver *solver =
            gsl_multifit_fdfsolver_alloc(gsl_multifit_fdfsolver_lmsder, n, p);
    if (!solver) {
        gsl_vector_free(initVec);
        return false;
    }

    int status = gsl_multifit_fdfsolver_set(solver, &func, initVec);
    if (status == GSL_SUCCESS) {
        for (int iter = 0; iter < 250; ++iter) {
            status = gsl_multifit_fdfsolver_iterate(solver);
            if (status != GSL_SUCCESS && status != GSL_CONTINUE && status != GSL_ENOPROG)
                break;
            if (gsl_multifit_test_delta(solver->dx, solver->x, 1e-8, 1e-8) == GSL_SUCCESS)
                break;
        }
        for (size_t j = 0; j < p; ++j) {
            const double v = gsl_vector_get(solver->x, j);
            if (std::isfinite(v))
                pInOut[j] = v;
        }
    }

    gsl_multifit_fdfsolver_free(solver);
    gsl_vector_free(initVec);
    return true;
}

static void drawReadoutBox(QPainter *painter, const QRect &canvasRect, const QPoint &anchorPos,
                           const QString &text, const QColor &borderColor, const QColor &bgColor,
                           bool preferRightOfAnchor = true)
{
    if (!painter || text.isEmpty())
        return;

    painter->save();
    QFont fnt = painter->font();
    fnt.setFamily("Arial");
    fnt.setPointSize(9);
    fnt.setBold(false);
    painter->setFont(fnt);

    const QFontMetrics fm(fnt);
    const QStringList lines = text.split('\n');
    int maxW = 0;
    for (const QString &ln : lines)
        maxW = std::max(maxW, fm.horizontalAdvance(ln));

    const int padX = 8;
    const int padY = 6;
    const int lineH = fm.height() + 2;
    const int boxW = maxW + 2 * padX;
    const int boxH = static_cast<int>(lines.size()) * lineH + 2 * padY - 2;

    int bx = preferRightOfAnchor ? (anchorPos.x() + 8) : (anchorPos.x() - boxW - 8);
    int by = anchorPos.y();
    if (bx + boxW > canvasRect.right() - 4)
        bx = anchorPos.x() - boxW - 8;
    if (bx < canvasRect.left() + 4)
        bx = canvasRect.left() + 4;
    if (bx + boxW > canvasRect.right() - 4)
        bx = std::max(canvasRect.left() + 2, canvasRect.right() - boxW - 4);
    if (by + boxH > canvasRect.bottom() - 4)
        by = canvasRect.bottom() - boxH - 4;
    if (by < canvasRect.top() + 4)
        by = canvasRect.top() + 4;

    const QRect boxRect(bx, by, boxW, boxH);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(borderColor, 1.3));
    painter->setBrush(bgColor);
    painter->drawRoundedRect(boxRect, 4.0, 4.0);

    painter->setPen(QColor(25, 25, 25));
    int curY = boxRect.top() + padY + fm.ascent();
    for (int i = 0; i < lines.size(); ++i) {
        if (i == 0 && lines.size() > 1) {
            QFont boldFnt = fnt;
            boldFnt.setBold(true);
            painter->setFont(boldFnt);
            painter->drawText(boxRect.left() + padX, curY, lines[i]);
            painter->setFont(fnt);
        } else {
            painter->drawText(boxRect.left() + padX, curY, lines[i]);
        }
        curY += lineH;
    }
    painter->restore();
}

} // anonymous namespace

// ============================================================================
// RoiOverlayItem — Custom QwtPlotItem for RoiGadgetTool
// ============================================================================

class RoiOverlayItem : public QwtPlotItem
{
public:
    explicit RoiOverlayItem(RoiGadgetTool *owner)
        : QwtPlotItem(QwtText("OriginRoiGadgetOverlay")), d_owner(owner)
    {
        setZ(950.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    ~RoiOverlayItem() override
    {
        if (d_owner && d_owner->d_overlay_item == this)
            d_owner->d_overlay_item = nullptr;
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 41; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_owner)
            return;

        const QRectF roi = d_owner->d_roi_rect.normalized();
        if (!roi.isValid() && roi.width() <= 0.0 && roi.height() <= 0.0)
            return;

        const int px1 = xMap.transform(roi.left());
        const int px2 = xMap.transform(roi.right());
        const int py1 = yMap.transform(roi.top());
        const int py2 = yMap.transform(roi.bottom());
        const QRect pxRect = QRect(QPoint(std::min(px1, px2), std::min(py1, py2)),
                                   QPoint(std::max(px1, px2), std::max(py1, py2)));

        painter->save();
        painter->setClipRect(canvasRect);
        painter->setRenderHint(QPainter::Antialiasing, true);

        // 1. Yellow translucent ROI rectangle
        painter->setBrush(QColor(255, 235, 59, 55));
        painter->setPen(QPen(QColor(210, 140, 0), 1.6, Qt::SolidLine));
        painter->drawRect(pxRect);

        // 8 resize handles (corners + mid-edges)
        const int hs = 3;
        painter->setBrush(QColor(255, 255, 255, 230));
        painter->setPen(QPen(QColor(180, 110, 0), 1.2));
        const QPoint handleCenters[8] = {
            pxRect.topLeft(),
            pxRect.topRight(),
            pxRect.bottomLeft(),
            pxRect.bottomRight(),
            QPoint(pxRect.center().x(), pxRect.top()),
            QPoint(pxRect.center().x(), pxRect.bottom()),
            QPoint(pxRect.left(), pxRect.center().y()),
            QPoint(pxRect.right(), pxRect.center().y())
        };
        for (const QPoint &hc : handleCenters)
            painter->drawRect(QRect(hc.x() - hs, hc.y() - hs, 2 * hs, 2 * hs));

        // 2. Mode-specific live graphics
        const RoiGadgetTool::GadgetMode mode = d_owner->d_mode;

        if (mode == RoiGadgetTool::Integrate && d_owner->d_integrate_poly_pts.size() >= 3) {
            QPolygonF polyPx;
            polyPx.reserve(d_owner->d_integrate_poly_pts.size());
            for (const QPointF &pt : d_owner->d_integrate_poly_pts)
                polyPx.append(QPointF(xMap.transform(pt.x()), yMap.transform(pt.y())));
            painter->setBrush(QColor(33, 150, 243, 65));
            painter->setPen(QPen(QColor(25, 118, 210), 1.5));
            painter->drawPolygon(polyPx);
        }

        if (d_owner->d_has_baseline) {
            const int by0 = yMap.transform(d_owner->d_baseline_y0);
            const int by1 = yMap.transform(d_owner->d_baseline_y1);
            painter->setPen(QPen(QColor(90, 90, 90), 1.3, Qt::DashLine));
            painter->drawLine(px1, by0, px2, by1);
        }

        if (!d_owner->d_overlay_curve_pts.isEmpty()) {
            QPolygonF curvePx;
            curvePx.reserve(d_owner->d_overlay_curve_pts.size());
            for (const QPointF &pt : d_owner->d_overlay_curve_pts)
                curvePx.append(QPointF(xMap.transform(pt.x()), yMap.transform(pt.y())));
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(QColor(220, 20, 20), 2.2, Qt::SolidLine));
            painter->drawPolyline(curvePx);
        }

        if (mode == RoiGadgetTool::QuickPeaks && !d_owner->d_detected_peaks.isEmpty()) {
            for (const auto &pk : d_owner->d_detected_peaks) {
                const int pkx = xMap.transform(pk.xc);
                const int pky = yMap.transform(pk.yTop);
                const int pkb = yMap.transform(pk.yBase);
                painter->setPen(QPen(QColor(200, 30, 30, 170), 1.2, Qt::DotLine));
                painter->drawLine(pkx, pky, pkx, pkb);

                QPolygon tri;
                tri << QPoint(pkx, pky - 2) << QPoint(pkx - 5, pky - 11)
                    << QPoint(pkx + 5, pky - 11);
                painter->setBrush(QColor(220, 30, 30));
                painter->setPen(QPen(QColor(130, 0, 0), 1.0));
                painter->drawPolygon(tri);
            }
        }

        if (mode == RoiGadgetTool::QuickSigmoidal && d_owner->d_has_ec50_marker) {
            const int ex = xMap.transform(d_owner->d_ec50_x);
            const int ey = yMap.transform(d_owner->d_ec50_y);
            painter->setPen(QPen(QColor(0, 120, 215), 1.3, Qt::DashLine));
            painter->drawLine(pxRect.left(), ey, pxRect.right(), ey);
            painter->drawLine(ex, pxRect.top(), ex, pxRect.bottom());
            painter->setBrush(QColor(255, 255, 255));
            painter->setPen(QPen(QColor(220, 20, 20), 2.0));
            painter->drawEllipse(QPoint(ex, ey), 4, 4);
        }

        if (mode == RoiGadgetTool::Interpolate && d_owner->d_has_interp_marker) {
            const int ix = xMap.transform(d_owner->d_interp_x);
            const int iy = yMap.transform(d_owner->d_interp_y);
            painter->setPen(QPen(QColor(0, 130, 180), 1.3, Qt::DashLine));
            painter->drawLine(ix, pxRect.top(), ix, pxRect.bottom());
            painter->drawLine(pxRect.left(), iy, pxRect.right(), iy);
            painter->setBrush(QColor(255, 235, 59));
            painter->setPen(QPen(QColor(200, 20, 20), 2.0));
            painter->drawEllipse(QPoint(ix, iy), 5, 5);
        }

        if (mode == RoiGadgetTool::RiseTime && d_owner->d_has_rise_markers) {
            const int y10Px = yMap.transform(d_owner->d_thresh_y10);
            const int y90Px = yMap.transform(d_owner->d_thresh_y90);
            painter->setPen(QPen(QColor(0, 145, 75), 1.5, Qt::DashLine));
            painter->drawLine(pxRect.left(), y10Px, pxRect.right(), y10Px);
            painter->drawLine(pxRect.left(), y90Px, pxRect.right(), y90Px);

            if (d_owner->d_primary_metric > 0.0) {
                const int x10Px = xMap.transform(d_owner->d_rise_x10);
                const int x90Px = xMap.transform(d_owner->d_rise_x90);
                painter->setPen(QPen(QColor(210, 40, 40), 1.4, Qt::DotLine));
                painter->drawLine(x10Px, pxRect.top(), x10Px, pxRect.bottom());
                painter->drawLine(x90Px, pxRect.top(), x90Px, pxRect.bottom());
                const int midYPx = (y10Px + y90Px) / 2;
                painter->setPen(QPen(QColor(210, 40, 40), 1.8));
                painter->drawLine(x10Px, midYPx, x90Px, midYPx);
            }
        }

        if ((mode == RoiGadgetTool::Cluster || mode == RoiGadgetTool::MaskRegion
             || mode == RoiGadgetTool::UnmaskRegion)
            && !d_owner->d_cluster_pts.isEmpty()) {
            QColor haloCol(0, 140, 255, 190);
            if (mode == RoiGadgetTool::MaskRegion)
                haloCol = QColor(220, 30, 30, 190);
            else if (mode == RoiGadgetTool::UnmaskRegion)
                haloCol = QColor(30, 160, 70, 190);
            painter->setPen(QPen(haloCol, 1.8));
            painter->setBrush(QColor(haloCol.red(), haloCol.green(), haloCol.blue(), 45));
            for (const QPointF &pt : d_owner->d_cluster_pts) {
                const int cx = xMap.transform(pt.x());
                const int cy = yMap.transform(pt.y());
                painter->drawEllipse(QPoint(cx, cy), 5, 5);
            }
        }

        if (mode == RoiGadgetTool::FFT && d_owner->d_fft_spectrum_pts.size() >= 2
            && pxRect.width() > 50 && pxRect.height() > 40) {
            // Draw mini FFT magnitude spectrum inset along bottom of ROI
            const QRect inset(pxRect.left() + 6, pxRect.bottom() - std::min(48, pxRect.height() / 3) - 4,
                              pxRect.width() - 12, std::min(48, pxRect.height() / 3));
            painter->setBrush(QColor(255, 255, 255, 185));
            painter->setPen(QPen(QColor(120, 120, 120), 1.0));
            painter->drawRect(inset);

            double fMax = d_owner->d_fft_spectrum_pts.back().x();
            double mMax = 0.0;
            for (const QPointF &fp : d_owner->d_fft_spectrum_pts)
                mMax = std::max(mMax, fp.y());
            if (fMax > 0.0 && mMax > 0.0) {
                QPolygonF specPoly;
                for (const QPointF &fp : d_owner->d_fft_spectrum_pts) {
                    const double sx = inset.left() + 2
                            + (fp.x() / fMax) * std::max(1, inset.width() - 4);
                    const double sy = inset.bottom() - 2
                            - (fp.y() / mMax) * std::max(1, inset.height() - 4);
                    specPoly.append(QPointF(sx, sy));
                }
                painter->setPen(QPen(QColor(200, 30, 30), 1.4));
                painter->setBrush(Qt::NoBrush);
                painter->drawPolyline(specPoly);
            }
        }

        // 3. Top-right live readout box above/inside ROI
        const QPoint anchor(pxRect.right(), std::max(canvasRect.top() + 4, pxRect.top() - 4));
        drawReadoutBox(painter, canvasRect, anchor, d_owner->d_summary_text, QColor(190, 130, 10),
                       QColor(255, 253, 235, 238), false);

        painter->restore();
    }

    RoiGadgetTool *d_owner;
};

// ============================================================================
// RoiGadgetTool Implementation
// ============================================================================

RoiGadgetTool::RoiGadgetTool(ApplicationWindow *app, Graph *graph, GadgetMode mode)
    : QObject(nullptr),
      PlotToolInterface(graph),
      d_app(app),
      d_graph_guard(graph),
      d_mode(mode),
      d_fit_model(FitLinear),
      d_explicit_curve(false),
      d_primary_metric(0.0),
      d_secondary_metric(0.0),
      d_recalculating(false),
      d_overlay_item(nullptr),
      d_drag_mode(DragNone)
{
    if (d_graph_guard && d_graph_guard->plotWidget()) {
        d_overlay_item = new RoiOverlayItem(this);
        d_overlay_item->attach(d_graph_guard->plotWidget());
        if (d_graph_guard->plotWidget()->canvas()) {
            d_graph_guard->plotWidget()->canvas()->setMouseTracking(true);
            d_graph_guard->plotWidget()->canvas()->installEventFilter(this);
            d_graph_guard->plotWidget()->canvas()->setFocus();
        }
    }
    initDefaultRoi();
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

RoiGadgetTool::~RoiGadgetTool()
{
    if (d_graph_guard && d_graph_guard->plotWidget()) {
        if (d_graph_guard->plotWidget()->canvas()) {
            d_graph_guard->plotWidget()->canvas()->removeEventFilter(this);
            d_graph_guard->plotWidget()->canvas()->unsetCursor();
        }
    }
    if (d_overlay_item) {
        d_overlay_item->d_owner = nullptr;
        d_overlay_item->detach();
        delete d_overlay_item;
        d_overlay_item = nullptr;
    }
}

void RoiGadgetTool::initDefaultRoi()
{
    if (!d_graph_guard || !d_graph_guard->plotWidget()) {
        d_roi_rect = QRectF(0.0, 0.0, 1.0, 1.0);
        return;
    }

    Plot *plot = d_graph_guard->plotWidget();
    QwtPlotCurve *c = resolveTargetCurve();
    const int xAxis = c ? c->xAxis() : int(QwtPlot::xBottom);
    const int yAxis = c ? c->yAxis() : int(QwtPlot::yLeft);

    double xMin = 0.0, xMax = 0.0, yMin = 0.0, yMax = 0.0;
    if (const QwtScaleDiv *xDiv = plot->axisScaleDiv(xAxis)) {
        xMin = std::min(xDiv->lowerBound(), xDiv->upperBound());
        xMax = std::max(xDiv->lowerBound(), xDiv->upperBound());
    }
    if (const QwtScaleDiv *yDiv = plot->axisScaleDiv(yAxis)) {
        yMin = std::min(yDiv->lowerBound(), yDiv->upperBound());
        yMax = std::max(yDiv->lowerBound(), yDiv->upperBound());
    }

    if (c && c->dataSize() > 0) {
        if (!(xMax > xMin)) {
            xMin = c->minXValue();
            xMax = c->maxXValue();
        }
        if (!(yMax > yMin)) {
            yMin = c->minYValue();
            yMax = c->maxYValue();
        }
    }

    if (!(xMax > xMin)) {
        xMin = 0.0;
        xMax = 10.0;
    }
    if (!(yMax > yMin)) {
        yMin = 0.0;
        yMax = 10.0;
    }

    const double dx = xMax - xMin;
    const double dy = yMax - yMin;
    d_roi_rect = QRectF(xMin + 0.20 * dx, yMin + 0.08 * dy, 0.60 * dx, 0.84 * dy).normalized();
}

QwtPlotCurve *RoiGadgetTool::resolveTargetCurve()
{
    if (!d_graph_guard)
        return nullptr;

    QwtPlotCurve *chosen = nullptr;
    if (!d_curve_title.isEmpty())
        chosen = d_graph_guard->curve(d_curve_title);

    if (!chosen) {
        const QStringList analysable = d_graph_guard->analysableCurvesList();
        for (const QString &name : analysable) {
            QwtPlotCurve *c = d_graph_guard->curve(name);
            if (c && c->isVisible() && c->dataSize() > 0) {
                chosen = c;
                d_curve_title = name;
                break;
            }
        }
    }

    if (!chosen) {
        for (int i = 0; i < d_graph_guard->curves(); ++i) {
            QwtPlotCurve *c = d_graph_guard->curve(i);
            if (c && c->isVisible() && c->dataSize() > 0) {
                chosen = c;
                d_curve_title = c->title().text();
                break;
            }
        }
    }

    if (auto *dc = dynamic_cast<DataCurve *>(chosen)) {
        if (dc->table()) {
            connect(dc->table(), &Table::modifiedData, this, &RoiGadgetTool::onSourceTableModified,
                    Qt::UniqueConnection);
        }
        if (d_overlay_item)
            d_overlay_item->setAxis(chosen->xAxis(), chosen->yAxis());
    } else if (chosen && d_overlay_item) {
        d_overlay_item->setAxis(chosen->xAxis(), chosen->yAxis());
    }

    return chosen;
}

DataCurve *RoiGadgetTool::resolveTargetDataCurve()
{
    return dynamic_cast<DataCurve *>(resolveTargetCurve());
}

void RoiGadgetTool::onSourceTableModified(Table *, const QString &)
{
    if (d_recalculating)
        return;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void RoiGadgetTool::setGadgetMode(GadgetMode mode)
{
    d_mode = mode;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void RoiGadgetTool::setQuickFitModel(QuickFitModel model)
{
    d_fit_model = model;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void RoiGadgetTool::setRoiRect(const QRectF &dataRect)
{
    d_roi_rect = dataRect.normalized();
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void RoiGadgetTool::setTargetCurve(const QString &curveTitle)
{
    d_curve_title = curveTitle;
    d_explicit_curve = !curveTitle.isEmpty();
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void RoiGadgetTool::recalculate()
{
    if (d_recalculating)
        return;
    d_recalculating = true;

    d_overlay_curve_pts.clear();
    d_integrate_poly_pts.clear();
    d_detected_peaks.clear();
    d_cluster_pts.clear();
    d_output_metrics.clear();
    d_fft_spectrum_pts.clear();
    d_has_baseline = false;
    d_has_rise_markers = false;
    d_has_interp_marker = false;
    d_has_ec50_marker = false;
    d_primary_metric = 0.0;
    d_secondary_metric = 0.0;

    QwtPlotCurve *curve = resolveTargetCurve();
    if (!curve || curve->dataSize() <= 0) {
        d_summary_text = tr("ROI Gadget: No curve data");
        d_recalculating = false;
        return;
    }

    DataCurve *dc = dynamic_cast<DataCurve *>(curve);
    const QRectF roi = d_roi_rect.normalized();
    const int nTotal = curve->dataSize();

    std::vector<XYPoint> pts;
    pts.reserve(size_t(nTotal));

    for (int i = 0; i < nTotal; ++i) {
        const double px = curve->x(i);
        const double py = curve->y(i);
        if (!std::isfinite(px) || !std::isfinite(py))
            continue;
        if (px < roi.left() || px > roi.right() || py < roi.top() || py > roi.bottom())
            continue;

        const bool masked = (dc != nullptr) && dc->isPointMasked(i);
        if (d_mode == UnmaskRegion) {
            if (!masked)
                continue;
        } else {
            if (masked)
                continue;
        }
        pts.push_back({ px, py, i });
    }

    if (pts.empty()) {
        d_summary_text = QString("%1 [%2]\nNo unmasked points in ROI")
                                 .arg(d_mode == UnmaskRegion ? tr("Unmask ROI") : tr("ROI Gadget"))
                                 .arg(curve->title().text());
        if (d_mode == UnmaskRegion)
            d_summary_text = QString("Unmask ROI [%1]\n0 masked points in ROI")
                                     .arg(curve->title().text());
        d_recalculating = false;
        return;
    }

    // Preserve unsorted cluster points for Cluster / MaskRegion / UnmaskRegion
    if (d_mode == Cluster || d_mode == MaskRegion || d_mode == UnmaskRegion) {
        d_cluster_pts.reserve(static_cast<int>(pts.size()));
        double sumY = 0.0;
        double sumX = 0.0;
        double minY = pts[0].y, maxY = pts[0].y;
        for (const auto &pt : pts) {
            d_cluster_pts.append(QPointF(pt.x, pt.y));
            sumX += pt.x;
            sumY += pt.y;
            minY = std::min(minY, pt.y);
            maxY = std::max(maxY, pt.y);
        }
        const double count = double(pts.size());
        const double meanX = sumX / count;
        const double meanY = sumY / count;
        d_primary_metric = count;
        d_secondary_metric = meanY;

        d_output_metrics << qMakePair(QString("Count"), count)
                         << qMakePair(QString("Mean_X"), meanX)
                         << qMakePair(QString("Mean_Y"), meanY)
                         << qMakePair(QString("Min_Y"), minY)
                         << qMakePair(QString("Max_Y"), maxY);

        const QString titleLabel = (d_mode == Cluster)
                ? tr("Cluster ROI")
                : ((d_mode == MaskRegion) ? tr("Mask Region ROI") : tr("Unmask Region ROI"));
        d_summary_text = QString("%1 [%2]\nN = %3, Mean Y = %4\nMean X = %5")
                                 .arg(titleLabel)
                                 .arg(curve->title().text())
                                 .arg(pts.size())
                                 .arg(meanY, 0, 'g', 6)
                                 .arg(meanX, 0, 'g', 6);
        if (d_app)
            d_app->setStatusBarText(d_summary_text.simplified());
        d_recalculating = false;
        return;
    }

    // Sort by X ascending for 1D curve gadgets
    std::stable_sort(pts.begin(), pts.end(),
                     [](const XYPoint &a, const XYPoint &b) { return a.x < b.x; });

    const size_t n = pts.size();
    std::vector<double> xv(n), yv(n);
    double sumY = 0.0;
    double minY = pts[0].y, maxY = pts[0].y;
    double xAtMin = pts[0].x, xAtMax = pts[0].x;
    int maxIdx = 0;

    for (size_t i = 0; i < n; ++i) {
        xv[i] = pts[i].x;
        yv[i] = pts[i].y;
        sumY += pts[i].y;
        if (pts[i].y < minY) {
            minY = pts[i].y;
            xAtMin = pts[i].x;
        }
        if (pts[i].y > maxY) {
            maxY = pts[i].y;
            xAtMax = pts[i].x;
            maxIdx = static_cast<int>(i);
        }
    }
    const double meanY = sumY / double(n);

    switch (d_mode) {
    case RegionStats: {
        double varSum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double d = yv[i] - meanY;
            varSum += d * d;
        }
        const double sd = (n >= 2) ? std::sqrt(varSum / double(n - 1)) : 0.0;
        const double se = (n >= 1) ? (sd / std::sqrt(double(n))) : 0.0;
        std::vector<double> sortedY = yv;
        std::sort(sortedY.begin(), sortedY.end());
        const double median = (n % 2 == 1)
                ? sortedY[n / 2]
                : 0.5 * (sortedY[n / 2 - 1] + sortedY[n / 2]);

        d_primary_metric = meanY;
        d_secondary_metric = sd;
        d_has_baseline = true;
        d_baseline_y0 = meanY;
        d_baseline_y1 = meanY;

        d_output_metrics << qMakePair(QString("N"), double(n))
                         << qMakePair(QString("Mean"), meanY)
                         << qMakePair(QString("SD"), sd)
                         << qMakePair(QString("SE"), se)
                         << qMakePair(QString("Median"), median)
                         << qMakePair(QString("Min"), minY)
                         << qMakePair(QString("Max"), maxY)
                         << qMakePair(QString("Sum"), sumY)
                         << qMakePair(QString("X_at_Min"), xAtMin)
                         << qMakePair(QString("X_at_Max"), xAtMax);

        d_summary_text = QString("Region Stats [%1] (N=%2)\nMean = %3, SD = %4\nMin = %5, Max = %6")
                                 .arg(curve->title().text())
                                 .arg(n)
                                 .arg(meanY, 0, 'g', 6)
                                 .arg(sd, 0, 'g', 6)
                                 .arg(minY, 0, 'g', 6)
                                 .arg(maxY, 0, 'g', 6);
        break;
    }

    case QuickFit: {
        const int nGrid = 120;
        const double xStart = roi.left();
        const double xEnd = roi.right();
        std::vector<double> yHat(n, meanY);
        QString modelLabel;
        QString paramLine;

        if (d_fit_model == FitLinear && n >= 2) {
            double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0, sumsq = 0.0;
            gsl_fit_linear(xv.data(), 1, yv.data(), 1, n, &c0, &c1, &cov00, &cov01, &cov11, &sumsq);
            for (size_t i = 0; i < n; ++i)
                yHat[i] = c0 + c1 * xv[i];
            const double r2 = computeR2(yv, yHat);
            d_primary_metric = r2;
            d_secondary_metric = c1;
            modelLabel = "Linear";
            paramLine = QString("Slope = %1, Int = %2")
                                .arg(c1, 0, 'g', 6)
                                .arg(c0, 0, 'g', 6);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("Slope"), c1)
                             << qMakePair(QString("Intercept"), c0)
                             << qMakePair(QString("RSS"), sumsq);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = xStart + (xEnd - xStart) * double(k) / double(nGrid - 1);
                d_overlay_curve_pts.append(QPointF(gx, c0 + c1 * gx));
            }
        } else if (d_fit_model == FitPoly2 && n >= 3) {
            gsl_matrix *X = gsl_matrix_alloc(n, 3);
            gsl_vector *Y = gsl_vector_alloc(n);
            gsl_vector *C = gsl_vector_alloc(3);
            gsl_matrix *cov = gsl_matrix_alloc(3, 3);
            for (size_t i = 0; i < n; ++i) {
                gsl_matrix_set(X, i, 0, 1.0);
                gsl_matrix_set(X, i, 1, xv[i]);
                gsl_matrix_set(X, i, 2, xv[i] * xv[i]);
                gsl_vector_set(Y, i, yv[i]);
            }
            double chisq = 0.0;
            gsl_multifit_linear_workspace *ws = gsl_multifit_linear_alloc(n, 3);
            gsl_multifit_linear(X, Y, C, cov, &chisq, ws);
            gsl_multifit_linear_free(ws);

            const double c0 = gsl_vector_get(C, 0);
            const double c1 = gsl_vector_get(C, 1);
            const double c2 = gsl_vector_get(C, 2);
            gsl_matrix_free(X);
            gsl_vector_free(Y);
            gsl_vector_free(C);
            gsl_matrix_free(cov);

            for (size_t i = 0; i < n; ++i)
                yHat[i] = c0 + c1 * xv[i] + c2 * xv[i] * xv[i];
            const double r2 = computeR2(yv, yHat);
            d_primary_metric = r2;
            d_secondary_metric = c2;
            modelLabel = "Poly2";
            paramLine = QString("c2 = %1, c1 = %2, c0 = %3")
                                .arg(c2, 0, 'g', 5)
                                .arg(c1, 0, 'g', 5)
                                .arg(c0, 0, 'g', 5);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("c0"), c0)
                             << qMakePair(QString("c1"), c1)
                             << qMakePair(QString("c2"), c2);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = xStart + (xEnd - xStart) * double(k) / double(nGrid - 1);
                d_overlay_curve_pts.append(QPointF(gx, c0 + c1 * gx + c2 * gx * gx));
            }
        } else if (d_fit_model == FitGauss && n >= 4) {
            const double fwhmGuess = estimateFwhm(xv, yv, minY, maxIdx);
            const double wGuess = std::max(1e-9, fwhmGuess / std::sqrt(2.0 * std::log(2.0)));
            std::vector<double> p = { minY, std::max(1e-9, maxY - minY), xAtMax, wGuess };
            runLmSolver(2, xv, yv, xv.front(), p);
            p[3] = std::fabs(p[3]);
            for (size_t i = 0; i < n; ++i) {
                const double u = (xv[i] - p[2]) / std::max(1e-12, p[3]);
                yHat[i] = p[0] + p[1] * std::exp(std::max(-500.0, -2.0 * u * u));
            }
            const double r2 = computeR2(yv, yHat);
            const double fwhm = p[3] * std::sqrt(2.0 * std::log(2.0));
            d_primary_metric = r2;
            d_secondary_metric = p[2]; // xc
            modelLabel = "Gauss";
            paramLine = QString("xc = %1, FWHM = %2, A = %3")
                                .arg(p[2], 0, 'g', 6)
                                .arg(fwhm, 0, 'g', 5)
                                .arg(p[1], 0, 'g', 5);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("xc"), p[2])
                             << qMakePair(QString("FWHM"), fwhm)
                             << qMakePair(QString("Amplitude"), p[1])
                             << qMakePair(QString("y0"), p[0]);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = xStart + (xEnd - xStart) * double(k) / double(nGrid - 1);
                const double u = (gx - p[2]) / std::max(1e-12, p[3]);
                d_overlay_curve_pts.append(
                        QPointF(gx, p[0] + p[1] * std::exp(std::max(-500.0, -2.0 * u * u))));
            }
        } else if (d_fit_model == FitLorentz && n >= 4) {
            const double fwhmGuess = estimateFwhm(xv, yv, minY, maxIdx);
            std::vector<double> p = { minY, std::max(1e-9, maxY - minY), xAtMax, fwhmGuess };
            runLmSolver(3, xv, yv, xv.front(), p);
            p[3] = std::fabs(p[3]);
            for (size_t i = 0; i < n; ++i) {
                const double dx = xv[i] - p[2];
                const double w2 = p[3] * p[3];
                yHat[i] = p[0] + p[1] * w2 / std::max(1e-30, 4.0 * dx * dx + w2);
            }
            const double r2 = computeR2(yv, yHat);
            d_primary_metric = r2;
            d_secondary_metric = p[2]; // xc
            modelLabel = "Lorentz";
            paramLine = QString("xc = %1, FWHM = %2, H = %3")
                                .arg(p[2], 0, 'g', 6)
                                .arg(p[3], 0, 'g', 5)
                                .arg(p[1], 0, 'g', 5);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("xc"), p[2])
                             << qMakePair(QString("FWHM"), p[3])
                             << qMakePair(QString("Height"), p[1])
                             << qMakePair(QString("y0"), p[0]);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = xStart + (xEnd - xStart) * double(k) / double(nGrid - 1);
                const double dx = gx - p[2];
                const double w2 = p[3] * p[3];
                d_overlay_curve_pts.append(
                        QPointF(gx, p[0] + p[1] * w2 / std::max(1e-30, 4.0 * dx * dx + w2)));
            }
        } else if (d_fit_model == FitExpDec1 && n >= 3) {
            const double xRef = xv.front();
            const double span = std::max(1e-9, xv.back() - xv.front());
            std::vector<double> p = { yv.back(), yv.front() - yv.back(), span / 3.0 };
            runLmSolver(4, xv, yv, xRef, p);
            for (size_t i = 0; i < n; ++i) {
                const double u = std::clamp((xv[i] - xRef) / p[2], -500.0, 500.0);
                yHat[i] = p[0] + p[1] * std::exp(-u);
            }
            const double r2 = computeR2(yv, yHat);
            d_primary_metric = r2;
            d_secondary_metric = p[2]; // t1
            modelLabel = "ExpDec1";
            paramLine = QString("t1 = %1, A = %2, y0 = %3")
                                .arg(p[2], 0, 'g', 6)
                                .arg(p[1], 0, 'g', 5)
                                .arg(p[0], 0, 'g', 5);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("t1"), p[2])
                             << qMakePair(QString("A"), p[1])
                             << qMakePair(QString("y0"), p[0]);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = xStart + (xEnd - xStart) * double(k) / double(nGrid - 1);
                const double u = std::clamp((gx - xRef) / p[2], -500.0, 500.0);
                d_overlay_curve_pts.append(QPointF(gx, p[0] + p[1] * std::exp(-u)));
            }
        } else if (d_fit_model == FitBoltzmann && n >= 4) {
            const double a1Guess = yv.front();
            const double a2Guess = yv.back();
            const double yMid = 0.5 * (a1Guess + a2Guess);
            double x0Guess = 0.5 * (xv.front() + xv.back());
            for (size_t i = 0; i + 1 < n; ++i) {
                if ((yv[i] <= yMid && yv[i + 1] >= yMid)
                    || (yv[i] >= yMid && yv[i + 1] <= yMid)) {
                    const double dy = yv[i + 1] - yv[i];
                    x0Guess = xv[i]
                            + ((std::fabs(dy) > 1e-15) ? ((yMid - yv[i]) / dy) : 0.5)
                                    * (xv[i + 1] - xv[i]);
                    break;
                }
            }
            const double dxGuess = std::max(1e-9, (xv.back() - xv.front()) / 8.0);
            std::vector<double> p = { a1Guess, a2Guess, x0Guess, dxGuess };
            runLmSolver(5, xv, yv, xv.front(), p);
            for (size_t i = 0; i < n; ++i) {
                const double u = std::clamp((xv[i] - p[2]) / p[3], -500.0, 500.0);
                yHat[i] = p[1] + (p[0] - p[1]) / (1.0 + std::exp(u));
            }
            const double r2 = computeR2(yv, yHat);
            d_primary_metric = r2;
            d_secondary_metric = p[2]; // x0
            modelLabel = "Boltzmann";
            paramLine = QString("x0 = %1, dx = %2")
                                .arg(p[2], 0, 'g', 6)
                                .arg(p[3], 0, 'g', 5);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("x0"), p[2])
                             << qMakePair(QString("dx"), p[3])
                             << qMakePair(QString("A1"), p[0])
                             << qMakePair(QString("A2"), p[1]);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = xStart + (xEnd - xStart) * double(k) / double(nGrid - 1);
                const double u = std::clamp((gx - p[2]) / p[3], -500.0, 500.0);
                d_overlay_curve_pts.append(QPointF(gx, p[1] + (p[0] - p[1]) / (1.0 + std::exp(u))));
            }
        } else {
            d_summary_text = QString("Quick Fit [%1]\nNeed more points in ROI (N=%2)")
                                     .arg(curve->title().text())
                                     .arg(n);
            break;
        }

        d_summary_text = QString("Quick Fit (%1) [%2]\nR² = %3\n%4")
                                 .arg(modelLabel)
                                 .arg(curve->title().text())
                                 .arg(d_primary_metric, 0, 'f', 5)
                                 .arg(paramLine);
        break;
    }

    case QuickPeaks: {
        std::vector<double> sortedY = yv;
        std::sort(sortedY.begin(), sortedY.end());
        const double yBase = sortedY[std::min(n - 1, n / 10)];
        d_has_baseline = true;
        d_baseline_y0 = yBase;
        d_baseline_y1 = yBase;

        std::vector<double> ys = yv;
        if (n >= 5) {
            for (size_t i = 1; i + 1 < n; ++i)
                ys[i] = 0.25 * yv[i - 1] + 0.5 * yv[i] + 0.25 * yv[i + 1];
        }

        const double promThresh = std::max(1e-12, 0.08 * (maxY - yBase));
        const int minSep = std::max(1, static_cast<int>(n / 25));
        struct Cand {
            int idx;
            double prom;
        };
        std::vector<Cand> cands;
        for (int i = 1; i + 1 < static_cast<int>(n); ++i) {
            const bool localMax = (ys[size_t(i)] >= ys[size_t(i - 1)]
                                   && ys[size_t(i)] >= ys[size_t(i + 1)]
                                   && (ys[size_t(i)] > ys[size_t(i - 1)]
                                       || ys[size_t(i)] > ys[size_t(i + 1)]));
            const double prom = yv[size_t(i)] - yBase;
            if (localMax && prom >= promThresh)
                cands.push_back({ i, prom });
        }
        std::sort(cands.begin(), cands.end(),
                  [](const Cand &a, const Cand &b) { return a.prom > b.prom; });

        std::vector<int> chosenIdx;
        for (const auto &c : cands) {
            bool tooClose = false;
            for (int existing : chosenIdx) {
                if (std::abs(c.idx - existing) < minSep) {
                    tooClose = true;
                    break;
                }
            }
            if (!tooClose)
                chosenIdx.push_back(c.idx);
        }
        std::sort(chosenIdx.begin(), chosenIdx.end());

        double maxPeakCenter = xAtMax;
        double maxPeakHeight = -1.0;
        for (int idx : chosenIdx) {
            DetectedPeak pk;
            pk.xc = xv[size_t(idx)];
            pk.yTop = yv[size_t(idx)];
            pk.yBase = yBase;
            pk.height = pk.yTop - yBase;
            pk.fwhm = estimateFwhm(xv, yv, yBase, idx);
            pk.area = 1.064467 * pk.height * pk.fwhm;
            d_detected_peaks.append(pk);
            if (pk.height > maxPeakHeight) {
                maxPeakHeight = pk.height;
                maxPeakCenter = pk.xc;
            }
        }

        d_primary_metric = double(d_detected_peaks.size());
        d_secondary_metric = (d_detected_peaks.isEmpty()) ? 0.0 : maxPeakCenter;

        d_output_metrics << qMakePair(QString("Peak_Count"), d_primary_metric)
                         << qMakePair(QString("Max_Peak_Center"), d_secondary_metric)
                         << qMakePair(QString("Baseline"), yBase);

        d_summary_text = QString("Quick Peaks [%1]\nPeaks = %2, Max xc = %3\nBaseline = %4")
                                 .arg(curve->title().text())
                                 .arg(d_detected_peaks.size())
                                 .arg(d_secondary_metric, 0, 'g', 6)
                                 .arg(yBase, 0, 'g', 5);
        break;
    }

    case QuickSigmoidal: {
        if (n >= 4) {
            const double a1Guess = yv.front();
            const double a2Guess = yv.back();
            const double yMidGuess = 0.5 * (a1Guess + a2Guess);
            double x0Guess = 0.5 * (xv.front() + xv.back());
            for (size_t i = 0; i + 1 < n; ++i) {
                if ((yv[i] <= yMidGuess && yv[i + 1] >= yMidGuess)
                    || (yv[i] >= yMidGuess && yv[i + 1] <= yMidGuess)) {
                    const double dy = yv[i + 1] - yv[i];
                    x0Guess = xv[i]
                            + ((std::fabs(dy) > 1e-15) ? ((yMidGuess - yv[i]) / dy) : 0.5)
                                    * (xv[i + 1] - xv[i]);
                    break;
                }
            }
            const double dxGuess = std::max(1e-9, (xv.back() - xv.front()) / 8.0);
            std::vector<double> p = { a1Guess, a2Guess, x0Guess, dxGuess };
            runLmSolver(5, xv, yv, xv.front(), p);

            std::vector<double> yHat(n);
            for (size_t i = 0; i < n; ++i) {
                const double u = std::clamp((xv[i] - p[2]) / p[3], -500.0, 500.0);
                yHat[i] = p[1] + (p[0] - p[1]) / (1.0 + std::exp(u));
            }
            const double r2 = computeR2(yv, yHat);
            d_primary_metric = p[2]; // EC50
            d_secondary_metric = r2;
            d_ec50_x = p[2];
            d_ec50_y = 0.5 * (p[0] + p[1]);
            d_has_ec50_marker = true;

            const int nGrid = 120;
            for (int k = 0; k < nGrid; ++k) {
                const double gx = roi.left() + roi.width() * double(k) / double(nGrid - 1);
                const double u = std::clamp((gx - p[2]) / p[3], -500.0, 500.0);
                d_overlay_curve_pts.append(QPointF(gx, p[1] + (p[0] - p[1]) / (1.0 + std::exp(u))));
            }

            d_output_metrics << qMakePair(QString("EC50"), p[2])
                             << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("A1_Bottom"), p[0])
                             << qMakePair(QString("A2_Top"), p[1])
                             << qMakePair(QString("dx_SlopeWidth"), p[3]);

            d_summary_text = QString("Quick Sigmoidal [%1]\nEC50 = %2, R² = %3\nA1 = %4, A2 = %5")
                                     .arg(curve->title().text())
                                     .arg(p[2], 0, 'g', 6)
                                     .arg(r2, 0, 'f', 5)
                                     .arg(p[0], 0, 'g', 5)
                                     .arg(p[1], 0, 'g', 5);
        } else {
            d_summary_text = QString("Quick Sigmoidal [%1]\nNeed >= 4 points in ROI")
                                     .arg(curve->title().text());
        }
        break;
    }

    case Integrate: {
        double area = 0.0;
        for (size_t i = 0; i + 1 < n; ++i)
            area += 0.5 * (yv[i] + yv[i + 1]) * (xv[i + 1] - xv[i]);

        const double fwhm = (n >= 3) ? estimateFwhm(xv, yv, minY, maxIdx) : 0.0;
        d_primary_metric = area;
        d_secondary_metric = fwhm;

        if (n >= 2) {
            d_integrate_poly_pts.reserve(static_cast<int>(n) + 2);
            d_integrate_poly_pts.append(QPointF(xv.front(), 0.0));
            for (size_t i = 0; i < n; ++i)
                d_integrate_poly_pts.append(QPointF(xv[i], yv[i]));
            d_integrate_poly_pts.append(QPointF(xv.back(), 0.0));
        }

        d_output_metrics << qMakePair(QString("Area"), area)
                         << qMakePair(QString("FWHM"), fwhm)
                         << qMakePair(QString("Peak_Height"), maxY)
                         << qMakePair(QString("Peak_X"), xAtMax);

        d_summary_text = QString("Integrate [%1]\nArea = %2\nFWHM = %3, Peak Y = %4")
                                 .arg(curve->title().text())
                                 .arg(area, 0, 'g', 6)
                                 .arg(fwhm, 0, 'g', 6)
                                 .arg(maxY, 0, 'g', 6);
        break;
    }

    case Interpolate: {
        // Deduplicate strictly increasing X for spline/linear interpolation
        std::vector<double> ux, uy;
        ux.reserve(n);
        uy.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            if (!ux.empty() && std::fabs(xv[i] - ux.back()) <= 1e-14) {
                uy.back() = 0.5 * (uy.back() + yv[i]);
            } else {
                ux.push_back(xv[i]);
                uy.push_back(yv[i]);
            }
        }

        const double xMid = std::clamp(0.5 * (roi.left() + roi.right()), ux.front(), ux.back());
        double midY = uy.front();
        double slope = 0.0;

        if (ux.size() >= 4) {
            gsl_interp_accel *acc = gsl_interp_accel_alloc();
            gsl_spline *spline = gsl_spline_alloc(gsl_interp_cspline, ux.size());
            gsl_spline_init(spline, ux.data(), uy.data(), ux.size());
            midY = gsl_spline_eval(spline, xMid, acc);
            slope = gsl_spline_eval_deriv(spline, xMid, acc);

            const int nGrid = 100;
            for (int k = 0; k < nGrid; ++k) {
                const double gx = ux.front() + (ux.back() - ux.front()) * double(k) / double(nGrid - 1);
                d_overlay_curve_pts.append(QPointF(gx, gsl_spline_eval(spline, gx, acc)));
            }
            gsl_spline_free(spline);
            gsl_interp_accel_free(acc);
        } else if (ux.size() >= 2) {
            auto it = std::lower_bound(ux.begin(), ux.end(), xMid);
            size_t idx = (it == ux.begin()) ? 0 : size_t(std::distance(ux.begin(), it) - 1);
            if (idx + 1 >= ux.size())
                idx = ux.size() - 2;
            const double dx = ux[idx + 1] - ux[idx];
            slope = (std::fabs(dx) > 1e-15) ? ((uy[idx + 1] - uy[idx]) / dx) : 0.0;
            midY = uy[idx] + slope * (xMid - ux[idx]);
            for (size_t i = 0; i < ux.size(); ++i)
                d_overlay_curve_pts.append(QPointF(ux[i], uy[i]));
        }

        d_primary_metric = midY;
        d_secondary_metric = slope;
        d_interp_x = xMid;
        d_interp_y = midY;
        d_interp_slope = slope;
        d_has_interp_marker = true;

        d_output_metrics << qMakePair(QString("Mid_X"), xMid)
                         << qMakePair(QString("Mid_Y"), midY)
                         << qMakePair(QString("dY_dX_Slope"), slope);

        d_summary_text = QString("Interpolate [%1]\nX = %2 -> Y = %3\ndY/dX = %4")
                                 .arg(curve->title().text())
                                 .arg(xMid, 0, 'g', 6)
                                 .arg(midY, 0, 'g', 6)
                                 .arg(slope, 0, 'g', 6);
        break;
    }

    case FFT: {
        if (n >= 4 && xv.back() > xv.front()) {
            // Resample uniformly across [xv.front(), xv.back()] if non-uniform
            const size_t M = n;
            const double dt = (xv.back() - xv.front()) / double(M - 1);
            std::vector<double> packed(2 * M, 0.0);

            double meanUniform = 0.0;
            std::vector<double> uniY(M);
            size_t seg = 0;
            for (size_t k = 0; k < M; ++k) {
                const double tx = xv.front() + double(k) * dt;
                while (seg + 2 < n && xv[seg + 1] < tx)
                    ++seg;
                const double dx = xv[seg + 1] - xv[seg];
                const double frac = (std::fabs(dx) > 1e-15)
                        ? std::clamp((tx - xv[seg]) / dx, 0.0, 1.0)
                        : 0.0;
                uniY[k] = yv[seg] + frac * (yv[seg + 1] - yv[seg]);
                meanUniform += uniY[k];
            }
            meanUniform /= double(M);

            for (size_t k = 0; k < M; ++k)
                packed[2 * k] = uniY[k] - meanUniform;

            gsl_fft_complex_wavetable *wt = gsl_fft_complex_wavetable_alloc(M);
            gsl_fft_complex_workspace *ws = gsl_fft_complex_workspace_alloc(M);
            gsl_fft_complex_forward(packed.data(), 1, M, wt, ws);
            gsl_fft_complex_wavetable_free(wt);
            gsl_fft_complex_workspace_free(ws);

            const double df = 1.0 / (double(M) * dt);
            const size_t halfM = M / 2;
            double domFreq = 0.0;
            double peakMag = 0.0;
            for (size_t k = 1; k <= halfM; ++k) {
                const double re = packed[2 * k];
                const double im = packed[2 * k + 1];
                const double scale = (k < halfM || (M % 2 == 1)) ? (2.0 / double(M)) : (1.0 / double(M));
                const double mag = scale * std::sqrt(re * re + im * im);
                const double fk = double(k) * df;
                d_fft_spectrum_pts.append(QPointF(fk, mag));
                if (mag > peakMag) {
                    peakMag = mag;
                    domFreq = fk;
                }
            }

            d_primary_metric = domFreq;
            d_secondary_metric = peakMag;

            d_output_metrics << qMakePair(QString("Dominant_Freq"), domFreq)
                             << qMakePair(QString("Peak_Magnitude"), peakMag)
                             << qMakePair(QString("Sampling_dt"), dt)
                             << qMakePair(QString("Freq_Resolution_df"), df);

            d_summary_text = QString("FFT Preview [%1]\nDom Freq = %2 Hz\nPeak Mag = %3")
                                     .arg(curve->title().text())
                                     .arg(domFreq, 0, 'g', 6)
                                     .arg(peakMag, 0, 'g', 6);
        } else {
            d_summary_text = QString("FFT Preview [%1]\nNeed >= 4 points in ROI")
                                     .arg(curve->title().text());
        }
        break;
    }

    case RiseTime: {
        const double yLow = minY;
        const double yHigh = maxY;
        const double amp = yHigh - yLow;
        const double y10 = yLow + 0.10 * amp;
        const double y90 = yLow + 0.90 * amp;

        double riseTime = 0.0;
        double fallTime = 0.0;
        double x10Rise = xv.front(), x90Rise = xv.front();

        if (amp > 1e-15 && n >= 2) {
            // 10-90% Rising edge search
            bool foundRise10 = false;
            size_t idxRise10 = 0;
            if (yv[0] == y10) {
                foundRise10 = true;
                x10Rise = xv[0];
                idxRise10 = 0;
            } else {
                for (size_t i = 0; i + 1 < n; ++i) {
                    if (yv[i] <= y10 && yv[i + 1] > y10) {
                        const double dy = yv[i + 1] - yv[i];
                        x10Rise = xv[i] + ((y10 - yv[i]) / dy) * (xv[i + 1] - xv[i]);
                        idxRise10 = i;
                        foundRise10 = true;
                        break;
                    }
                }
            }
            if (foundRise10) {
                for (size_t j = idxRise10; j + 1 < n; ++j) {
                    if (yv[j] < y90 && yv[j + 1] >= y90) {
                        const double dy = yv[j + 1] - yv[j];
                        x90Rise = xv[j]
                                + ((std::fabs(dy) > 1e-15) ? ((y90 - yv[j]) / dy) : 0.0)
                                        * (xv[j + 1] - xv[j]);
                        if (x90Rise >= x10Rise)
                            riseTime = x90Rise - x10Rise;
                        break;
                    }
                }
            }

            // 90-10% Falling edge search
            bool foundFall90 = false;
            double x90Fall = 0.0;
            size_t idxFall90 = 0;
            for (size_t i = 0; i + 1 < n; ++i) {
                if (yv[i] >= y90 && yv[i + 1] < y90) {
                    const double dy = yv[i + 1] - yv[i];
                    x90Fall = xv[i] + ((y90 - yv[i]) / dy) * (xv[i + 1] - xv[i]);
                    idxFall90 = i;
                    foundFall90 = true;
                    break;
                }
            }
            if (foundFall90) {
                for (size_t j = idxFall90; j + 1 < n; ++j) {
                    if (yv[j] > y10 && yv[j + 1] <= y10) {
                        const double dy = yv[j + 1] - yv[j];
                        const double x10Fall = xv[j]
                                + ((std::fabs(dy) > 1e-15) ? ((y10 - yv[j]) / dy) : 0.0)
                                        * (xv[j + 1] - xv[j]);
                        if (x10Fall >= x90Fall)
                            fallTime = x10Fall - x90Fall;
                        break;
                    }
                }
            }
        }

        d_primary_metric = riseTime;
        d_secondary_metric = fallTime;
        d_thresh_y10 = y10;
        d_thresh_y90 = y90;
        d_rise_x10 = x10Rise;
        d_rise_x90 = x90Rise;
        d_has_rise_markers = (amp > 1e-15);

        d_output_metrics << qMakePair(QString("Rise_Time_10_90"), riseTime)
                         << qMakePair(QString("Fall_Time_90_10"), fallTime)
                         << qMakePair(QString("State_Low"), yLow)
                         << qMakePair(QString("State_High"), yHigh)
                         << qMakePair(QString("Threshold_10pct"), y10)
                         << qMakePair(QString("Threshold_90pct"), y90);

        d_summary_text = QString("Rise Time [%1]\n10-90% Rise = %2\n90-10% Fall = %3")
                                 .arg(curve->title().text())
                                 .arg(riseTime, 0, 'g', 6)
                                 .arg(fallTime, 0, 'g', 6);
        break;
    }

    default:
        break;
    }

    if (d_app)
        d_app->setStatusBarText(d_summary_text.simplified());
    d_recalculating = false;
}

Table *RoiGadgetTool::createOutputTable(const QString &tableName)
{
    if (!d_app)
        return nullptr;

    recalculate();

    QString baseName = tableName.trimmed();
    if (baseName.isEmpty())
        baseName = d_app->generateUniqueName("GadgetResult");
    else if (d_app->table(baseName))
        baseName = d_app->generateUniqueName(baseName);

    if (d_mode == QuickPeaks && !d_detected_peaks.isEmpty()) {
        const int rows = d_detected_peaks.size();
        Table *t = d_app->newTable(rows, 5, baseName, tr("ROI Quick Peaks Output"));
        if (!t)
            return nullptr;
        t->setColName(0, "Peak_Index");
        t->setColName(1, "Center_X");
        t->setColName(2, "Height");
        t->setColName(3, "FWHM");
        t->setColName(4, "Area");
        t->setColPlotDesignation(1, SciDAVis::X);
        for (int r = 0; r < rows; ++r) {
            const auto &pk = d_detected_peaks[r];
            t->setCell(r, 0, double(r + 1));
            t->setCell(r, 1, pk.xc);
            t->setCell(r, 2, pk.height);
            t->setCell(r, 3, pk.fwhm);
            t->setCell(r, 4, pk.area);
        }
        return t;
    }

    if (d_mode == FFT && !d_fft_spectrum_pts.isEmpty()) {
        const int rows = d_fft_spectrum_pts.size();
        Table *t = d_app->newTable(rows, 2, baseName, tr("ROI FFT Spectrum"));
        if (!t)
            return nullptr;
        t->setColName(0, "Frequency");
        t->setColName(1, "Magnitude");
        t->setColPlotDesignation(0, SciDAVis::X);
        t->setColPlotDesignation(1, SciDAVis::Y);
        for (int r = 0; r < rows; ++r) {
            t->setCell(r, 0, d_fft_spectrum_pts[r].x());
            t->setCell(r, 1, d_fft_spectrum_pts[r].y());
        }
        return t;
    }

    const int rows = std::max(1, d_output_metrics.size());
    Table *t = d_app->newTable(rows, 2, baseName, tr("ROI Gadget Summary"));
    if (!t)
        return nullptr;
    t->setColName(0, "Metric");
    t->setColumnType(0, SciDAVis::ColumnMode::Text);
    t->setColName(1, "Value");
    for (int r = 0; r < d_output_metrics.size(); ++r) {
        t->setText(r, 0, d_output_metrics[r].first);
        t->setCell(r, 1, d_output_metrics[r].second);
    }
    return t;
}

int RoiGadgetTool::applyClusterMask(bool mask)
{
    if (!d_graph_guard)
        return 0;

    int totalModified = 0;
    if ((d_mode == MaskRegion || d_mode == UnmaskRegion) && !d_explicit_curve) {
        for (int i = 0; i < d_graph_guard->curves(); ++i) {
            if (auto *dc = dynamic_cast<DataCurve *>(d_graph_guard->curve(i))) {
                if (dc->isVisible())
                    totalModified += dc->maskRegion(d_roi_rect, mask);
            }
        }
    } else if (DataCurve *dc = resolveTargetDataCurve()) {
        totalModified = dc->maskRegion(d_roi_rect, mask);
    }

    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
    if (d_app && totalModified > 0)
        d_app->modifiedProject();
    return totalModified;
}

RoiGadgetTool *RoiGadgetTool::attachGadget(ApplicationWindow *app, Graph *g, GadgetMode mode)
{
    if (!g)
        return nullptr;
    g->setActiveTool(nullptr);
    RoiGadgetTool *tool = new RoiGadgetTool(app, g, mode);
    g->setActiveTool(tool);
    g->replot();
    return tool;
}

void RoiGadgetTool::clearGadget(Graph *g)
{
    if (!g)
        return;
    if (g->activeTool()
        && (g->activeTool()->rtti() == RoiGadget || g->activeTool()->rtti() == VerticalCursor)) {
        g->setActiveTool(nullptr);
    }
    g->replot();
}

RoiGadgetTool::DragMode RoiGadgetTool::hitTestPixelPos(const QPoint &pos) const
{
    if (!d_graph_guard || !d_graph_guard->plotWidget())
        return DragNone;

    Plot *plot = d_graph_guard->plotWidget();
    const int xAxis = d_overlay_item ? d_overlay_item->xAxis() : int(QwtPlot::xBottom);
    const int yAxis = d_overlay_item ? d_overlay_item->yAxis() : int(QwtPlot::yLeft);
    const QRectF roi = d_roi_rect.normalized();

    const int x1 = plot->transform(xAxis, roi.left());
    const int x2 = plot->transform(xAxis, roi.right());
    const int y1 = plot->transform(yAxis, roi.top());
    const int y2 = plot->transform(yAxis, roi.bottom());
    const int left = std::min(x1, x2);
    const int right = std::max(x1, x2);
    const int top = std::min(y1, y2);
    const int bottom = std::max(y1, y2);

    const int tol = 6;
    const bool nearLeft = std::abs(pos.x() - left) <= tol && pos.y() >= top - tol
            && pos.y() <= bottom + tol;
    const bool nearRight = std::abs(pos.x() - right) <= tol && pos.y() >= top - tol
            && pos.y() <= bottom + tol;
    const bool nearTop = std::abs(pos.y() - top) <= tol && pos.x() >= left - tol
            && pos.x() <= right + tol;
    const bool nearBottom = std::abs(pos.y() - bottom) <= tol && pos.x() >= left - tol
            && pos.x() <= right + tol;

    if (nearTop && nearLeft)
        return DragTopLeft;
    if (nearTop && nearRight)
        return DragTopRight;
    if (nearBottom && nearLeft)
        return DragBottomLeft;
    if (nearBottom && nearRight)
        return DragBottomRight;
    if (nearLeft)
        return DragLeft;
    if (nearRight)
        return DragRight;
    if (nearTop)
        return DragTop;
    if (nearBottom)
        return DragBottom;

    if (QRect(QPoint(left, top), QPoint(right, bottom)).contains(pos))
        return DragMove;

    return DragNone;
}

void RoiGadgetTool::updateCursorForPos(const QPoint &pos)
{
    if (!d_graph_guard || !d_graph_guard->plotWidget() || !d_graph_guard->plotWidget()->canvas())
        return;
    QWidget *canvas = d_graph_guard->plotWidget()->canvas();
    switch (hitTestPixelPos(pos)) {
    case DragTopLeft:
    case DragBottomRight:
        canvas->setCursor(Qt::SizeFDiagCursor);
        break;
    case DragTopRight:
    case DragBottomLeft:
        canvas->setCursor(Qt::SizeBDiagCursor);
        break;
    case DragLeft:
    case DragRight:
        canvas->setCursor(Qt::SizeHorCursor);
        break;
    case DragTop:
    case DragBottom:
        canvas->setCursor(Qt::SizeVerCursor);
        break;
    case DragMove:
        canvas->setCursor(Qt::SizeAllCursor);
        break;
    default:
        canvas->setCursor(Qt::ArrowCursor);
        break;
    }
}

void RoiGadgetTool::showRoiContextMenu(const QPoint &globalPos)
{
    if (!d_graph_guard)
        return;

    QMenu menu;
    QMenu *modeMenu = menu.addMenu(tr("Gadget Mode"));
    struct ModeItem {
        GadgetMode mode;
        const char *label;
    };
    const ModeItem modes[] = {
        { RegionStats, "Region Statistics" },
        { QuickFit, "Quick Fit" },
        { QuickPeaks, "Quick Peaks" },
        { QuickSigmoidal, "Quick Sigmoidal Fit" },
        { Integrate, "Integrate" },
        { Interpolate, "Interpolate" },
        { FFT, "FFT Preview" },
        { RiseTime, "Rise Time (10-90%)" },
        { Cluster, "Cluster Selector" },
        { MaskRegion, "Mask Region" },
        { UnmaskRegion, "Unmask Region" }
    };
    for (const auto &mi : modes) {
        QAction *act = modeMenu->addAction(tr(mi.label));
        act->setCheckable(true);
        act->setChecked(d_mode == mi.mode);
        connect(act, &QAction::triggered, this, [this, m = mi.mode]() { setGadgetMode(m); });
    }

    QMenu *fitMenu = menu.addMenu(tr("Quick Fit Model"));
    struct FitItem {
        QuickFitModel model;
        const char *label;
    };
    const FitItem fitModels[] = {
        { FitLinear, "Linear" },
        { FitPoly2, "Polynomial (Order 2)" },
        { FitGauss, "Gaussian" },
        { FitLorentz, "Lorentzian" },
        { FitExpDec1, "Exponential Decay (ExpDec1)" },
        { FitBoltzmann, "Boltzmann Sigmoidal" }
    };
    for (const auto &fi : fitModels) {
        QAction *act = fitMenu->addAction(tr(fi.label));
        act->setCheckable(true);
        act->setChecked(d_fit_model == fi.model);
        connect(act, &QAction::triggered, this, [this, fm = fi.model]() {
            d_mode = QuickFit;
            setQuickFitModel(fm);
        });
    }

    const QStringList curveNames = d_graph_guard->analysableCurvesList();
    if (!curveNames.isEmpty()) {
        QMenu *curveMenu = menu.addMenu(tr("Target Curve"));
        for (const QString &cn : curveNames) {
            QAction *act = curveMenu->addAction(cn);
            act->setCheckable(true);
            act->setChecked(cn == d_curve_title);
            connect(act, &QAction::triggered, this, [this, cn]() { setTargetCurve(cn); });
        }
    }

    menu.addSeparator();
    QAction *actOutTable = menu.addAction(tr("Output Results to Worksheet"));
    connect(actOutTable, &QAction::triggered, this, [this]() { createOutputTable(); });

    QAction *actMask = menu.addAction(tr("Mask Points in ROI"));
    connect(actMask, &QAction::triggered, this, [this]() { applyClusterMask(true); });

    QAction *actUnmask = menu.addAction(tr("Unmask Points in ROI"));
    connect(actUnmask, &QAction::triggered, this, [this]() { applyClusterMask(false); });

    QAction *actCopy = menu.addAction(tr("Copy Summary to Clipboard"));
    connect(actCopy, &QAction::triggered, this,
            [this]() { QApplication::clipboard()->setText(d_summary_text); });

    menu.addSeparator();
    QAction *actClose = menu.addAction(tr("Close Gadget"));
    QPointer<Graph> gPtr = d_graph_guard;
    connect(actClose, &QAction::triggered, this, [gPtr]() {
        if (gPtr)
            QTimer::singleShot(0, gPtr, [gPtr]() { RoiGadgetTool::clearGadget(gPtr); });
    });

    menu.exec(globalPos);
}

bool RoiGadgetTool::eventFilter(QObject *obj, QEvent *event)
{
    if (!d_graph_guard || !d_graph_guard->plotWidget()
        || obj != d_graph_guard->plotWidget()->canvas())
        return QObject::eventFilter(obj, event);

    Plot *plot = d_graph_guard->plotWidget();
    const int xAxis = d_overlay_item ? d_overlay_item->xAxis() : int(QwtPlot::xBottom);
    const int yAxis = d_overlay_item ? d_overlay_item->yAxis() : int(QwtPlot::yLeft);

    switch (event->type()) {
    case QEvent::ContextMenu: {
        auto *ce = static_cast<QContextMenuEvent *>(event);
        if (hitTestPixelPos(ce->pos()) != DragNone) {
            showRoiContextMenu(ce->globalPos());
            ce->accept();
            return true;
        }
        break;
    }

    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::RightButton && hitTestPixelPos(me->pos()) != DragNone) {
            showRoiContextMenu(me->globalPos());
            me->accept();
            return true;
        }
        if (me->button() == Qt::LeftButton) {
            const DragMode hit = hitTestPixelPos(me->pos());
            if (hit != DragNone) {
                d_drag_mode = hit;
                d_drag_start_pos = me->pos();
                d_drag_start_roi = d_roi_rect.normalized();
                plot->canvas()->setFocus();
                me->accept();
                return true;
            }
            if ((me->modifiers() & Qt::ShiftModifier) || d_mode == MaskRegion
                || d_mode == UnmaskRegion || d_mode == Cluster) {
                d_drag_mode = DragRubberBand;
                d_drag_start_pos = me->pos();
                const double x0 = plot->invTransform(xAxis, me->pos().x());
                const double y0 = plot->invTransform(yAxis, me->pos().y());
                d_drag_start_roi = QRectF(x0, y0, 0.0, 0.0);
                plot->canvas()->setFocus();
                me->accept();
                return true;
            }
        }
        break;
    }

    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (d_drag_mode == DragNone) {
            updateCursorForPos(me->pos());
            return false;
        }
        if (!(me->buttons() & Qt::LeftButton)) {
            d_drag_mode = DragNone;
            updateCursorForPos(me->pos());
            return false;
        }

        const int x1Orig = plot->transform(xAxis, d_drag_start_roi.left());
        const int x2Orig = plot->transform(xAxis, d_drag_start_roi.right());
        const int y1Orig = plot->transform(yAxis, d_drag_start_roi.top());
        const int y2Orig = plot->transform(yAxis, d_drag_start_roi.bottom());

        int pLeft = std::min(x1Orig, x2Orig);
        int pRight = std::max(x1Orig, x2Orig);
        int pTop = std::min(y1Orig, y2Orig);
        int pBottom = std::max(y1Orig, y2Orig);

        const QPoint delta = me->pos() - d_drag_start_pos;
        switch (d_drag_mode) {
        case DragMove:
            pLeft += delta.x();
            pRight += delta.x();
            pTop += delta.y();
            pBottom += delta.y();
            break;
        case DragLeft:
            pLeft += delta.x();
            break;
        case DragRight:
            pRight += delta.x();
            break;
        case DragTop:
            pTop += delta.y();
            break;
        case DragBottom:
            pBottom += delta.y();
            break;
        case DragTopLeft:
            pLeft += delta.x();
            pTop += delta.y();
            break;
        case DragTopRight:
            pRight += delta.x();
            pTop += delta.y();
            break;
        case DragBottomLeft:
            pLeft += delta.x();
            pBottom += delta.y();
            break;
        case DragBottomRight:
            pRight += delta.x();
            pBottom += delta.y();
            break;
        case DragRubberBand:
            pLeft = std::min(d_drag_start_pos.x(), me->pos().x());
            pRight = std::max(d_drag_start_pos.x(), me->pos().x());
            pTop = std::min(d_drag_start_pos.y(), me->pos().y());
            pBottom = std::max(d_drag_start_pos.y(), me->pos().y());
            break;
        default:
            break;
        }

        const double nx1 = plot->invTransform(xAxis, pLeft);
        const double nx2 = plot->invTransform(xAxis, pRight);
        const double ny1 = plot->invTransform(yAxis, pTop);
        const double ny2 = plot->invTransform(yAxis, pBottom);
        d_roi_rect = QRectF(QPointF(nx1, ny1), QPointF(nx2, ny2)).normalized();
        recalculate();
        d_graph_guard->replot();
        me->accept();
        return true;
    }

    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (d_drag_mode != DragNone && me->button() == Qt::LeftButton) {
            d_drag_mode = DragNone;
            updateCursorForPos(me->pos());
            me->accept();
            return true;
        }
        break;
    }

    case QEvent::MouseButtonDblClick: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && hitTestPixelPos(me->pos()) != DragNone) {
            if (d_mode == Cluster || d_mode == MaskRegion)
                applyClusterMask(true);
            else if (d_mode == UnmaskRegion)
                applyClusterMask(false);
            me->accept();
            return true;
        }
        break;
    }

    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (d_mode == Cluster || d_mode == MaskRegion) {
                applyClusterMask(true);
                return true;
            }
            if (d_mode == UnmaskRegion) {
                applyClusterMask(false);
                return true;
            }
        } else if (ke->key() == Qt::Key_Escape) {
            QPointer<Graph> gPtr = d_graph_guard;
            if (gPtr)
                QTimer::singleShot(0, gPtr, [gPtr]() { RoiGadgetTool::clearGadget(gPtr); });
            return true;
        } else if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right) {
            const double step = ((ke->key() == Qt::Key_Right) ? 1.0 : -1.0) * 0.02
                    * std::max(1e-9, d_roi_rect.width());
            setRoiRect(d_roi_rect.translated(step, 0.0));
            return true;
        }
        break;
    }

    default:
        break;
    }

    return QObject::eventFilter(obj, event);
}

// ============================================================================
// VerticalCursorOverlayItem — Custom QwtPlotItem per synced Graph layer
// ============================================================================

class VerticalCursorOverlayItem : public QwtPlotItem
{
public:
    VerticalCursorOverlayItem(VerticalCursorTool *owner, Graph *layerGraph)
        : QwtPlotItem(QwtText("OriginVerticalCursorOverlay")),
          d_owner(owner),
          d_layer_graph(layerGraph)
    {
        setZ(960.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    ~VerticalCursorOverlayItem() override
    {
        if (d_owner)
            d_owner->d_overlay_items.removeAll(this);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 42; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_owner || !d_layer_graph)
            return;

        const double cx = d_owner->cursorX();
        if (!std::isfinite(cx))
            return;

        const int px = xMap.transform(cx);
        painter->save();
        painter->setClipRect(canvasRect);
        painter->setRenderHint(QPainter::Antialiasing, true);

        // Crisp OriginPro vertical red cursor line
        painter->setPen(QPen(QColor(215, 25, 28), 1.6, Qt::SolidLine));
        painter->drawLine(px, canvasRect.top(), px, canvasRect.bottom());

        // Top & bottom small triangular cursor handles on canvas border
        QPolygon topTri;
        topTri << QPoint(px, canvasRect.top() + 7) << QPoint(px - 5, canvasRect.top())
               << QPoint(px + 5, canvasRect.top());
        painter->setBrush(QColor(215, 25, 28));
        painter->drawPolygon(topTri);

        // Draw interpolated target rings on each visible curve in this layer
        const QVector<QPair<QString, double>> layerVals =
                d_owner->interpolatedValuesForGraph(d_layer_graph);
        QStringList tagLines;
        tagLines << QString("X = %1").arg(cx, 0, 'g', 6);

        for (const auto &kv : layerVals) {
            QwtPlotCurve *c = d_layer_graph->curve(kv.first);
            const QwtScaleMap &cyMap =
                    (c && d_layer_graph->plotWidget()) ? d_layer_graph->plotWidget()->canvasMap(c->yAxis())
                                                       : yMap;
            const int py = cyMap.transform(kv.second);
            painter->setBrush(QColor(255, 255, 255, 220));
            painter->setPen(QPen(QColor(215, 25, 28), 2.0));
            painter->drawEllipse(QPoint(px, py), 4, 4);
            painter->setBrush(QColor(215, 25, 28));
            painter->drawEllipse(QPoint(px, py), 1, 1);
            tagLines << QString("%1: %2").arg(kv.first).arg(kv.second, 0, 'g', 6);
        }

        drawReadoutBox(painter, canvasRect, QPoint(px, canvasRect.top() + 10),
                       tagLines.join('\n'), QColor(200, 35, 35), QColor(255, 255, 255, 235), true);

        painter->restore();
    }

    VerticalCursorTool *d_owner;
    QPointer<Graph> d_layer_graph;
};

// ============================================================================
// VerticalCursorTool Implementation
// ============================================================================

VerticalCursorTool::VerticalCursorTool(ApplicationWindow *app, Graph *graph, bool syncAllLayers)
    : QObject(nullptr),
      PlotToolInterface(graph),
      d_app(app),
      d_graph_guard(graph),
      d_cursor_x(0.0),
      d_sync_layers(syncAllLayers),
      d_dragging(false)
{
    if (d_graph_guard && d_graph_guard->plotWidget()) {
        Plot *plot = d_graph_guard->plotWidget();
        double xMin = 0.0, xMax = 0.0;
        if (const QwtScaleDiv *xDiv = plot->axisScaleDiv(QwtPlot::xBottom)) {
            xMin = std::min(xDiv->lowerBound(), xDiv->upperBound());
            xMax = std::max(xDiv->lowerBound(), xDiv->upperBound());
        }
        if (!(xMax > xMin) && d_graph_guard->curves() > 0) {
            if (QwtPlotCurve *c = d_graph_guard->curve(0)) {
                xMin = c->minXValue();
                xMax = c->maxXValue();
            }
        }
        d_cursor_x = 0.5 * (xMin + xMax);
    }

    rebuildOverlaysAndFilters();
    updateReadoutAndReplot();
}

VerticalCursorTool::~VerticalCursorTool()
{
    detachAllOverlaysAndFilters();
}

MultiLayer *VerticalCursorTool::findParentMultiLayer() const
{
    if (!d_graph_guard)
        return nullptr;
    QWidget *w = d_graph_guard->parentWidget();
    while (w) {
        if (auto *ml = qobject_cast<MultiLayer *>(w))
            return ml;
        w = w->parentWidget();
    }
    return nullptr;
}

QList<Graph *> VerticalCursorTool::targetGraphs() const
{
    QList<Graph *> list;
    if (!d_graph_guard)
        return list;

    list.append(d_graph_guard);
    if (d_sync_layers) {
        if (MultiLayer *ml = findParentMultiLayer()) {
            const QWidgetList widgets = ml->layerWidgets();
            for (QWidget *w : widgets) {
                Graph *g = qobject_cast<Graph *>(w);
                if (g && !list.contains(g))
                    list.append(g);
            }
        }
    }
    return list;
}

void VerticalCursorTool::detachAllOverlaysAndFilters()
{
    for (const QPointer<Graph> &gPtr : d_filtered_graphs) {
        if (gPtr && gPtr->plotWidget() && gPtr->plotWidget()->canvas()) {
            gPtr->plotWidget()->canvas()->removeEventFilter(this);
            gPtr->plotWidget()->canvas()->unsetCursor();
        }
    }
    d_filtered_graphs.clear();

    const QList<VerticalCursorOverlayItem *> itemsCopy = d_overlay_items;
    d_overlay_items.clear();
    for (VerticalCursorOverlayItem *item : itemsCopy) {
        if (!item)
            continue;
        item->d_owner = nullptr;
        item->detach();
        delete item;
    }
}

void VerticalCursorTool::rebuildOverlaysAndFilters()
{
    detachAllOverlaysAndFilters();

    const QList<Graph *> graphs = targetGraphs();
    for (Graph *g : graphs) {
        if (!g || !g->plotWidget())
            continue;
        auto *item = new VerticalCursorOverlayItem(this, g);
        item->attach(g->plotWidget());
        d_overlay_items.append(item);

        if (QwtPlotCanvas *canvas = g->plotWidget()->canvas()) {
            canvas->setMouseTracking(true);
            canvas->installEventFilter(this);
            d_filtered_graphs.append(g);
        }
    }
    if (d_graph_guard && d_graph_guard->plotWidget() && d_graph_guard->plotWidget()->canvas())
        d_graph_guard->plotWidget()->canvas()->setFocus();
}

bool VerticalCursorTool::interpolateCurveAtX(QwtPlotCurve *c, double xQuery, double &yOut)
{
    if (!c || !c->isVisible() || c->rtti() != QwtPlotItem::Rtti_PlotCurve || c->dataSize() <= 0)
        return false;

    const DataCurve *dc = dynamic_cast<const DataCurve *>(c);
    const int n = c->dataSize();
    std::vector<std::pair<double, double>> pts;
    pts.reserve(size_t(n));
    for (int i = 0; i < n; ++i) {
        if (dc && dc->isPointMasked(i))
            continue;
        const double px = c->x(i);
        const double py = c->y(i);
        if (std::isfinite(px) && std::isfinite(py))
            pts.emplace_back(px, py);
    }
    if (pts.empty())
        return false;

    std::stable_sort(pts.begin(), pts.end(),
                     [](const auto &a, const auto &b) { return a.first < b.first; });

    if (pts.size() == 1 || xQuery <= pts.front().first) {
        yOut = pts.front().second;
        return true;
    }
    if (xQuery >= pts.back().first) {
        yOut = pts.back().second;
        return true;
    }

    auto it = std::lower_bound(pts.begin(), pts.end(), xQuery,
                               [](const auto &pt, double val) { return pt.first < val; });
    if (it != pts.end() && std::fabs(it->first - xQuery) <= 1e-15) {
        yOut = it->second;
        return true;
    }
    const size_t idx = (it == pts.begin()) ? 0 : size_t(std::distance(pts.begin(), it) - 1);
    const double x0 = pts[idx].first;
    const double y0 = pts[idx].second;
    const double x1 = pts[idx + 1].first;
    const double y1 = pts[idx + 1].second;
    const double dx = x1 - x0;
    yOut = (std::fabs(dx) > 1e-15) ? (y0 + ((xQuery - x0) / dx) * (y1 - y0)) : y0;
    return true;
}

QVector<QPair<QString, double>> VerticalCursorTool::interpolatedValuesForGraph(Graph *g) const
{
    QVector<QPair<QString, double>> out;
    if (!g)
        return out;

    const QStringList analysable = g->analysableCurvesList();
    for (int i = 0; i < g->curves(); ++i) {
        const int ctype = g->curveType(i);
        if (ctype == Graph::ErrorBars || ctype == Graph::Pie || ctype == Graph::Box)
            continue;
        QwtPlotCurve *c = g->curve(i);
        if (!c || !c->isVisible())
            continue;
        if (!analysable.isEmpty() && !analysable.contains(c->title().text())
            && ctype != Graph::Function)
            continue;
        double yVal = 0.0;
        if (interpolateCurveAtX(c, d_cursor_x, yVal))
            out.append(qMakePair(c->title().text(), yVal));
    }
    return out;
}

QVector<QPair<QString, double>> VerticalCursorTool::interpolatedValues() const
{
    QVector<QPair<QString, double>> allVals;
    const QList<Graph *> graphs = targetGraphs();
    for (Graph *g : graphs) {
        const auto layerVals = interpolatedValuesForGraph(g);
        for (const auto &kv : layerVals)
            allVals.append(kv);
    }
    return allVals;
}

void VerticalCursorTool::setCursorX(double x)
{
    if (!std::isfinite(x))
        return;
    d_cursor_x = x;
    updateReadoutAndReplot();
}

void VerticalCursorTool::setSyncAllLayers(bool sync)
{
    if (d_sync_layers == sync)
        return;
    d_sync_layers = sync;
    rebuildOverlaysAndFilters();
    updateReadoutAndReplot();
}

void VerticalCursorTool::updateReadoutAndReplot()
{
    const auto vals = interpolatedValues();
    QStringList parts;
    parts << QString("X = %1").arg(d_cursor_x, 0, 'g', 7);
    for (const auto &kv : vals)
        parts << QString("%1 = %2").arg(kv.first).arg(kv.second, 0, 'g', 7);
    d_readout = parts.join(" | ");

    if (d_app)
        d_app->setStatusBarText(d_readout);

    const QList<Graph *> graphs = targetGraphs();
    for (Graph *g : graphs) {
        if (g)
            g->replot();
    }
}

Table *VerticalCursorTool::createReadoutTable()
{
    if (!d_app)
        return nullptr;
    const auto vals = interpolatedValues();
    const QString name = d_app->generateUniqueName("CursorReadout");
    const int rows = std::max(1, vals.size());
    Table *t = d_app->newTable(rows, 3, name, tr("Vertical Cursor Readout"));
    if (!t)
        return nullptr;
    t->setColName(0, "Curve");
    t->setColumnType(0, SciDAVis::ColumnMode::Text);
    t->setColName(1, "X");
    t->setColName(2, "Interpolated_Y");
    t->setColPlotDesignation(1, SciDAVis::X);
    t->setColPlotDesignation(2, SciDAVis::Y);
    for (int i = 0; i < vals.size(); ++i) {
        t->setText(i, 0, vals[i].first);
        t->setCell(i, 1, d_cursor_x);
        t->setCell(i, 2, vals[i].second);
    }
    return t;
}

void VerticalCursorTool::showCursorContextMenu(const QPoint &globalPos)
{
    QMenu menu;
    QAction *actSync = menu.addAction(tr("Sync Across All Layers"));
    actSync->setCheckable(true);
    actSync->setChecked(d_sync_layers);
    connect(actSync, &QAction::triggered, this, [this](bool on) { setSyncAllLayers(on); });

    QAction *actCopy = menu.addAction(tr("Copy Readout to Clipboard"));
    connect(actCopy, &QAction::triggered, this,
            [this]() { QApplication::clipboard()->setText(d_readout); });

    QAction *actTable = menu.addAction(tr("Output Readout to Worksheet"));
    connect(actTable, &QAction::triggered, this, [this]() { createReadoutTable(); });

    menu.addSeparator();
    QAction *actClose = menu.addAction(tr("Close Vertical Cursor"));
    QPointer<Graph> gPtr = d_graph_guard;
    connect(actClose, &QAction::triggered, this, [gPtr]() {
        if (gPtr)
            QTimer::singleShot(0, gPtr, [gPtr]() { RoiGadgetTool::clearGadget(gPtr); });
    });

    menu.exec(globalPos);
}

VerticalCursorTool *VerticalCursorTool::attachCursor(ApplicationWindow *app, Graph *g,
                                                     bool syncAllLayers)
{
    if (!g)
        return nullptr;
    g->setActiveTool(nullptr);
    VerticalCursorTool *tool = new VerticalCursorTool(app, g, syncAllLayers);
    g->setActiveTool(tool);
    g->replot();
    return tool;
}

bool VerticalCursorTool::eventFilter(QObject *obj, QEvent *event)
{
    Graph *eventGraph = nullptr;
    for (const QPointer<Graph> &gPtr : d_filtered_graphs) {
        if (gPtr && gPtr->plotWidget() && gPtr->plotWidget()->canvas() == obj) {
            eventGraph = gPtr;
            break;
        }
    }
    if (!eventGraph || !eventGraph->plotWidget())
        return QObject::eventFilter(obj, event);

    Plot *plot = eventGraph->plotWidget();

    switch (event->type()) {
    case QEvent::ContextMenu: {
        auto *ce = static_cast<QContextMenuEvent *>(event);
        showCursorContextMenu(ce->globalPos());
        ce->accept();
        return true;
    }

    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && !(me->modifiers() & Qt::AltModifier)) {
            d_dragging = true;
            plot->canvas()->setFocus();
            plot->canvas()->setCursor(Qt::SplitHCursor);
            setCursorX(plot->invTransform(QwtPlot::xBottom, me->pos().x()));
            me->accept();
            return true;
        }
        break;
    }

    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (d_dragging && (me->buttons() & Qt::LeftButton)) {
            setCursorX(plot->invTransform(QwtPlot::xBottom, me->pos().x()));
            me->accept();
            return true;
        } else {
            const int cPx = plot->transform(QwtPlot::xBottom, d_cursor_x);
            if (std::abs(me->pos().x() - cPx) <= 6)
                plot->canvas()->setCursor(Qt::SplitHCursor);
            else
                plot->canvas()->setCursor(Qt::CrossCursor);
        }
        break;
    }

    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (d_dragging && me->button() == Qt::LeftButton) {
            d_dragging = false;
            plot->canvas()->setCursor(Qt::CrossCursor);
            me->accept();
            return true;
        }
        break;
    }

    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right) {
            double xMin = 0.0, xMax = 1.0;
            if (const QwtScaleDiv *xDiv = plot->axisScaleDiv(QwtPlot::xBottom)) {
                xMin = std::min(xDiv->lowerBound(), xDiv->upperBound());
                xMax = std::max(xDiv->lowerBound(), xDiv->upperBound());
            }
            const double span = std::max(1e-9, xMax - xMin);
            const double frac = (ke->modifiers() & Qt::ShiftModifier) ? 0.05 : 0.01;
            const double dir = (ke->key() == Qt::Key_Right) ? 1.0 : -1.0;
            setCursorX(d_cursor_x + dir * frac * span);
            return true;
        } else if (ke->key() == Qt::Key_Escape) {
            QPointer<Graph> gPtr = d_graph_guard;
            if (gPtr)
                QTimer::singleShot(0, gPtr, [gPtr]() { RoiGadgetTool::clearGadget(gPtr); });
            return true;
        }
        break;
    }

    default:
        break;
    }

    return QObject::eventFilter(obj, event);
}
