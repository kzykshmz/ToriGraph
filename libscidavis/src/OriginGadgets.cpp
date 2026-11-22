/***************************************************************************
    File                 : OriginGadgets.cpp
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Description          : OriginPro-style Interactive ROI Gadgets & Vertical Cursor
 ***************************************************************************/

#include "OriginGadgets.h"

#include "ApplicationWindow.h"
#include "FunctionCurve.h"
#include "Graph.h"
#include "Legend.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "Spectrogram.h"
#include "Table.h"
#include "core/column/Column.h"

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_item.h>
#include <qwt_plot_spectrogram.h>
#include <qwt_scale_div.h>
#include <qwt_scale_map.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintDevice>
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
#include <gsl/gsl_version.h>

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

template <typename Vec, typename GetX>
static bool sequenceHasXReversals(const Vec &pts, GetX getX)
{
    if (pts.size() < 3)
        return false;
    double minX = getX(pts[0]);
    double maxX = minX;
    for (size_t i = 1; i < pts.size(); ++i) {
        const double x = getX(pts[i]);
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
    }
    const double tol = 1e-12 * std::max(1.0, maxX - minX);
    bool hasPos = false;
    bool hasNeg = false;
    for (size_t i = 1; i < pts.size(); ++i) {
        const double dx = getX(pts[i]) - getX(pts[i - 1]);
        if (dx > tol)
            hasPos = true;
        else if (dx < -tol)
            hasNeg = true;
        if (hasPos && hasNeg)
            return true;
    }
    return false;
}

static ApplicationWindow *resolveAppWindowFromGraph(Graph *g)
{
    for (QWidget *w = g; w != nullptr; w = w->parentWidget()) {
        if (auto *app = qobject_cast<ApplicationWindow *>(w))
            return app;
    }
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget *w : tops) {
        if (auto *app = qobject_cast<ApplicationWindow *>(w))
            return app;
    }
    return nullptr;
}

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

static bool isExportingOrPrinting(const Graph *g, const QPainter *painter)
{
    if (g && (g->isExporting() || g->property("isExporting").toBool()))
        return true;
    if (!painter || !painter->device())
        return false;
    const int dt = painter->device()->devType();
    if (dt == QInternal::CustomRaster || dt == QInternal::Printer
        || dt == QInternal::Picture)
        return true;
    if (dt != QInternal::Widget && dt != QInternal::Pixmap && dt != QInternal::Image
        && dt != QInternal::OpenGL)
        return true;
    return false;
}

static bool isAxisLog10(const Plot *plot, int axisId)
{
    if (!plot)
        return false;
    const QwtScaleMap sm = plot->canvasMap(axisId);
    return sm.transformation() && sm.transformation()->type() == QwtScaleTransformation::Log10;
}

static void primaryAxesForGraph(Graph *g, int &xAxisOut, int &yAxisOut)
{
    xAxisOut = int(QwtPlot::xBottom);
    yAxisOut = int(QwtPlot::yLeft);
    if (!g)
        return;
    const QStringList analysable = g->analysableCurvesList();
    for (const QString &name : analysable) {
        if (QwtPlotCurve *c = g->curve(name)) {
            if (c->isVisible()) {
                xAxisOut = c->xAxis();
                yAxisOut = c->yAxis();
                return;
            }
        }
    }
    for (int i = 0; i < g->curves(); ++i) {
        if (QwtPlotCurve *c = g->curve(i)) {
            if (c->isVisible()) {
                xAxisOut = c->xAxis();
                yAxisOut = c->yAxis();
                return;
            }
        }
    }
}

static double estimateFwhm(const std::vector<double> &x, const std::vector<double> &y,
                           double yBase, int peakIdx)
{
    const int n = static_cast<int>(x.size());
    if (n < 2 || peakIdx < 0 || peakIdx >= n)
        return (n >= 2) ? std::max(1e-9, (x.back() - x.front()) * 0.25) : 1.0;

    const double yTop = y[size_t(peakIdx)];
    const double diffTop = yTop - yBase;
    if (std::fabs(diffTop) <= 1e-30)
        return std::max(1e-9, (x.back() - x.front()) * 0.25);
    const double yHalf = yBase + 0.5 * diffTop;

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
                        double xRef, std::vector<double> &pInOut,
                        std::vector<double> *seOut = nullptr)
{
    const size_t n = x.size();
    const size_t p = pInOut.size();
    if (seOut)
        seOut->assign(p, 0.0);
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
        if (seOut) {
            const double fnrm = gsl_blas_dnrm2(solver->f);
            const double s2 = (n > p) ? ((fnrm * fnrm) / double(n - p)) : 0.0;
            gsl_matrix *cov = gsl_matrix_alloc(p, p);
#if GSL_MAJOR_VERSION < 2
            gsl_multifit_covar(solver->J, 0.0, cov);
#else
            gsl_matrix *J = gsl_matrix_alloc(n, p);
            if (gsl_multifit_fdfsolver_jac(solver, J) == GSL_SUCCESS)
                gsl_multifit_covar(J, 0.0, cov);
            else
                gsl_matrix_set_zero(cov);
            gsl_matrix_free(J);
#endif
            for (size_t j = 0; j < p; ++j) {
                const double cjj = gsl_matrix_get(cov, j, j);
                if (std::isfinite(cjj) && cjj > 0.0 && s2 > 0.0)
                    (*seOut)[j] = std::sqrt(cjj * s2);
            }
            gsl_matrix_free(cov);
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
    QFont fnt("Arial", 9);
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

static QwtPlotSpectrogram *findSpectrogramOnGraph(Graph *g, Spectrogram **outSciSpec = nullptr)
{
    if (outSciSpec)
        *outSciSpec = nullptr;
    if (!g || !g->plotWidget())
        return nullptr;

    const QwtPlotItemList &items = g->plotWidget()->itemList();
    for (QwtPlotItem *it : items) {
        if (!it || !it->isVisible())
            continue;
        if (auto *sciSpec = dynamic_cast<Spectrogram *>(it)) {
            if (outSciSpec)
                *outSciSpec = sciSpec;
            return sciSpec;
        }
        if (auto *qwtSpec = dynamic_cast<QwtPlotSpectrogram *>(it))
            return qwtSpec;
    }
    return nullptr;
}

static double safeMatrixCell(Matrix *m, int r, int c)
{
    if (!m || r < 0 || c < 0 || r >= m->numRows() || c >= m->numCols())
        return 0.0;
    const double v = m->cell(r, c);
    return std::isfinite(v) ? v : 0.0;
}

static double evalMatrixBilinear(Matrix *m, double x, double y)
{
    if (!m || m->numRows() <= 0 || m->numCols() <= 0)
        return 0.0;

    const int rows = m->numRows();
    const int cols = m->numCols();
    const double xs = m->xStart();
    const double xe = m->xEnd();
    const double ys = m->yStart();
    const double ye = m->yEnd();

    const double fc = (cols > 1 && std::fabs(xe - xs) > 1e-30)
            ? std::clamp((x - xs) / (xe - xs), 0.0, 1.0) * double(cols - 1)
            : 0.0;
    const double fr = (rows > 1 && std::fabs(ye - ys) > 1e-30)
            ? std::clamp((y - ys) / (ye - ys), 0.0, 1.0) * double(rows - 1)
            : 0.0;

    const int c0 = std::clamp(int(std::floor(fc)), 0, cols - 1);
    const int c1 = std::min(cols - 1, c0 + 1);
    const double tx = std::clamp(fc - double(c0), 0.0, 1.0);

    const int r0 = std::clamp(int(std::floor(fr)), 0, rows - 1);
    const int r1 = std::min(rows - 1, r0 + 1);
    const double ty = std::clamp(fr - double(r0), 0.0, 1.0);

    const double z00 = safeMatrixCell(m, r0, c0);
    const double z01 = safeMatrixCell(m, r0, c1);
    const double z10 = safeMatrixCell(m, r1, c0);
    const double z11 = safeMatrixCell(m, r1, c1);

    const double z0 = (1.0 - tx) * z00 + tx * z01;
    const double z1 = (1.0 - tx) * z10 + tx * z11;
    return (1.0 - ty) * z0 + ty * z1;
}

static void evalSavitzkyGolayAtPoint(const std::vector<double> &ux, const std::vector<double> &uy,
                                     double xQuery, int sgWindow, double &dydxOut,
                                     double &d2ydx2Out)
{
    dydxOut = 0.0;
    d2ydx2Out = 0.0;
    const int m = static_cast<int>(ux.size());
    if (m < 2 || static_cast<int>(uy.size()) != m)
        return;

    if (m == 2) {
        const double dx = ux[1] - ux[0];
        dydxOut = (std::fabs(dx) > 1e-15) ? ((uy[1] - uy[0]) / dx) : 0.0;
        d2ydx2Out = 0.0;
        return;
    }

    auto it = std::lower_bound(ux.begin(), ux.end(), xQuery);
    int nearIdx = 0;
    if (it == ux.end()) {
        nearIdx = m - 1;
    } else if (it == ux.begin()) {
        nearIdx = 0;
    } else {
        const int idxHi = static_cast<int>(std::distance(ux.begin(), it));
        const int idxLo = idxHi - 1;
        nearIdx = (std::fabs(ux[size_t(idxHi)] - xQuery) < std::fabs(ux[size_t(idxLo)] - xQuery))
                ? idxHi
                : idxLo;
    }

    int reqWin = std::max(3, sgWindow);
    if (reqWin % 2 == 0)
        ++reqWin;
    const int W = std::min(m, reqWin);
    const int start = std::clamp(nearIdx - W / 2, 0, m - W);
    const int polyOrder = std::min(3, W - 1); // p=3 on 5 pts, p=2 on 3..4 pts -> exact for quadratics & cubics
    const int nCoeffs = polyOrder + 1;

    double h = 0.0;
    for (int k = 0; k < W; ++k)
        h = std::max(h, std::fabs(ux[size_t(start + k)] - xQuery));
    if (h <= 1e-15)
        return;

    gsl_matrix *X = gsl_matrix_alloc(size_t(W), size_t(nCoeffs));
    gsl_vector *Y = gsl_vector_alloc(size_t(W));
    gsl_vector *C = gsl_vector_alloc(size_t(nCoeffs));
    gsl_matrix *cov = gsl_matrix_alloc(size_t(nCoeffs), size_t(nCoeffs));
    gsl_multifit_linear_workspace *ws = gsl_multifit_linear_alloc(size_t(W), size_t(nCoeffs));

    for (int k = 0; k < W; ++k) {
        const double u = (ux[size_t(start + k)] - xQuery) / h;
        double upow = 1.0;
        for (int j = 0; j < nCoeffs; ++j) {
            gsl_matrix_set(X, size_t(k), size_t(j), upow);
            upow *= u;
        }
        gsl_vector_set(Y, size_t(k), uy[size_t(start + k)]);
    }

    double chisq = 0.0;
    if (gsl_multifit_linear(X, Y, C, cov, &chisq, ws) == GSL_SUCCESS) {
        const double b1 = (nCoeffs > 1) ? gsl_vector_get(C, 1) : 0.0;
        const double b2 = (nCoeffs > 2) ? gsl_vector_get(C, 2) : 0.0;
        if (std::isfinite(b1))
            dydxOut = b1 / h;
        if (std::isfinite(b2))
            d2ydx2Out = (2.0 * b2) / (h * h);
    }

    gsl_multifit_linear_free(ws);
    gsl_matrix_free(cov);
    gsl_vector_free(C);
    gsl_vector_free(Y);
    gsl_matrix_free(X);
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
        const Graph *g = d_owner ? d_owner->d_graph_guard.data() : nullptr;
        if (!g && plot())
            g = qobject_cast<const Graph *>(plot()->parent());
        if (isExportingOrPrinting(g, painter))
            return;
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

        const bool exporting = isExportingOrPrinting(d_owner->d_graph_guard, painter);

        // 1. Yellow translucent ROI rectangle (publication-clean border on export, handles hidden)
        painter->setBrush(QColor(255, 235, 59, exporting ? 35 : 55));
        painter->setPen(
                QPen(QColor(210, 140, 0), exporting ? 1.1 : 1.6, exporting ? Qt::DashLine : Qt::SolidLine));
        painter->drawRect(pxRect);

        // 8 resize handles (corners + mid-edges) — hidden during export/print
        if (!exporting) {
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
        }

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
                const bool isTrough = (pk.height < 0.0);
                painter->setPen(QPen(isTrough ? QColor(20, 95, 195, 170) : QColor(200, 30, 30, 170),
                                     1.2, Qt::DotLine));
                painter->drawLine(pkx, pky, pkx, pkb);

                QPolygon tri;
                if (isTrough) {
                    tri << QPoint(pkx, pky + 2) << QPoint(pkx - 5, pky + 11)
                        << QPoint(pkx + 5, pky + 11);
                    painter->setBrush(QColor(25, 105, 210));
                    painter->setPen(QPen(QColor(10, 50, 120), 1.0));
                } else {
                    tri << QPoint(pkx, pky - 2) << QPoint(pkx - 5, pky - 11)
                        << QPoint(pkx + 5, pky - 11);
                    painter->setBrush(QColor(220, 30, 30));
                    painter->setPen(QPen(QColor(130, 0, 0), 1.0));
                }
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

        if (mode == RoiGadgetTool::Differentiate) {
            if (d_owner->d_diff_preview_pts.size() >= 2) {
                QPolygonF diffPx;
                diffPx.reserve(d_owner->d_diff_preview_pts.size());
                for (const QPointF &pt : d_owner->d_diff_preview_pts)
                    diffPx.append(QPointF(xMap.transform(pt.x()), yMap.transform(pt.y())));
                painter->setBrush(Qt::NoBrush);
                painter->setPen(QPen(QColor(142, 36, 170), 2.0, Qt::SolidLine));
                painter->drawPolyline(diffPx);
            }
            if (d_owner->d_has_diff_marker) {
                const int mx = xMap.transform(d_owner->d_diff_max_x);
                const int my = yMap.transform(d_owner->d_diff_max_y);
                painter->setPen(QPen(QColor(142, 36, 170), 1.3, Qt::DashLine));
                painter->drawLine(mx, pxRect.top(), mx, pxRect.bottom());
                painter->drawLine(pxRect.left(), my, pxRect.right(), my);
                painter->setBrush(QColor(255, 215, 0));
                painter->setPen(QPen(QColor(142, 36, 170), 2.0));
                painter->drawEllipse(QPoint(mx, my), 5, 5);
            }
        }

        if (mode == RoiGadgetTool::Intersect && !d_owner->d_intersections.isEmpty()) {
            QFont calloutFnt("Arial", 8, QFont::Bold);
            painter->setFont(calloutFnt);
            const QFontMetrics cfm(calloutFnt);
            for (int k = 0; k < d_owner->d_intersections.size(); ++k) {
                const auto &ir = d_owner->d_intersections[k];
                const int ix = xMap.transform(ir.x);
                const int iy = yMap.transform(ir.y);

                QPolygon diamond;
                diamond << QPoint(ix, iy - 6) << QPoint(ix + 6, iy) << QPoint(ix, iy + 6)
                        << QPoint(ix - 6, iy);
                painter->setBrush(QColor(0, 229, 255, 235));
                painter->setPen(QPen(QColor(198, 40, 40), 1.8));
                painter->drawPolygon(diamond);

                if (k < 12) {
                    const QString lbl =
                            QString("(%1, %2)").arg(ir.x, 0, 'g', 5).arg(ir.y, 0, 'g', 5);
                    const int tw = cfm.horizontalAdvance(lbl) + 8;
                    const int th = cfm.height() + 4;
                    int lx = ix + 8;
                    int ly = iy - th - 4;
                    if (lx + tw > canvasRect.right() - 2)
                        lx = ix - tw - 8;
                    if (ly < canvasRect.top() + 2)
                        ly = iy + 6;
                    const QRect pill(lx, ly, tw, th);
                    painter->setBrush(QColor(255, 255, 255, 225));
                    painter->setPen(QPen(QColor(198, 40, 40), 1.1));
                    painter->drawRoundedRect(pill, 3.0, 3.0);
                    painter->setPen(QColor(25, 25, 25));
                    painter->drawText(pill.left() + 4, pill.top() + 2 + cfm.ascent(), lbl);
                }
            }
        }

        if (mode == RoiGadgetTool::Integrate2D && d_owner->d_has_2d_markers) {
            const int cx = xMap.transform(d_owner->d_centroid_x);
            const int cy = yMap.transform(d_owner->d_centroid_y);
            painter->setPen(QPen(QColor(0, 120, 215), 1.4, Qt::DashLine));
            painter->drawLine(cx - 10, cy, cx + 10, cy);
            painter->drawLine(cx, cy - 10, cx, cy + 10);
            painter->setBrush(QColor(255, 255, 255, 220));
            painter->setPen(QPen(QColor(0, 120, 215), 2.0));
            painter->drawEllipse(QPoint(cx, cy), 5, 5);

            const int pkx = xMap.transform(d_owner->d_peak2d_x);
            const int pky = yMap.transform(d_owner->d_peak2d_y);
            QPolygon pkDia;
            pkDia << QPoint(pkx, pky - 5) << QPoint(pkx + 5, pky) << QPoint(pkx, pky + 5)
                  << QPoint(pkx - 5, pky);
            painter->setBrush(QColor(255, 82, 82, 230));
            painter->setPen(QPen(QColor(140, 0, 0), 1.4));
            painter->drawPolygon(pkDia);
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
    Spectrogram *sciSpec = nullptr;
    QwtPlotSpectrogram *qwtSpec = findSpectrogramOnGraph(d_graph_guard, &sciSpec);

    const int xAxis = c ? c->xAxis() : (qwtSpec ? qwtSpec->xAxis() : int(QwtPlot::xBottom));
    const int yAxis = c ? c->yAxis() : (qwtSpec ? qwtSpec->yAxis() : int(QwtPlot::yLeft));
    if (!c && qwtSpec && d_overlay_item)
        d_overlay_item->setAxis(xAxis, yAxis);

    double xMin = 0.0, xMax = 0.0, yMin = 0.0, yMax = 0.0;
    if (const QwtScaleDiv *xDiv = plot->axisScaleDiv(xAxis)) {
        xMin = std::min(xDiv->lowerBound(), xDiv->upperBound());
        xMax = std::max(xDiv->lowerBound(), xDiv->upperBound());
    }
    if (const QwtScaleDiv *yDiv = plot->axisScaleDiv(yAxis)) {
        yMin = std::min(yDiv->lowerBound(), yDiv->upperBound());
        yMax = std::max(yDiv->lowerBound(), yDiv->upperBound());
    }

    if (sciSpec && sciSpec->matrix()) {
        Matrix *m = sciSpec->matrix();
        const double mXMin = std::min(m->xStart(), m->xEnd());
        const double mXMax = std::max(m->xStart(), m->xEnd());
        const double mYMin = std::min(m->yStart(), m->yEnd());
        const double mYMax = std::max(m->yStart(), m->yEnd());
        if (d_mode == Integrate2D || !(xMax > xMin)) {
            if (mXMax > mXMin) {
                xMin = mXMin;
                xMax = mXMax;
            }
        }
        if (d_mode == Integrate2D || !(yMax > yMin)) {
            if (mYMax > mYMin) {
                yMin = mYMin;
                yMax = mYMax;
            }
        }
    } else if (c && c->dataSize() > 0) {
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

    const bool logX = isAxisLog10(plot, xAxis);
    const bool logY = isAxisLog10(plot, yAxis);

    double rx1 = 0.0, rx2 = 1.0, ry1 = 0.0, ry2 = 1.0;
    if (logX && xMax > 0.0) {
        const double safeXMin = (xMin > 0.0) ? xMin : std::max(1e-100, xMax * 1e-4);
        const double lxMin = std::log10(safeXMin);
        const double lxMax = std::log10(xMax);
        rx1 = std::pow(10.0, lxMin + 0.20 * (lxMax - lxMin));
        rx2 = std::pow(10.0, lxMin + 0.80 * (lxMax - lxMin));
    } else {
        const double dx = xMax - xMin;
        rx1 = xMin + 0.20 * dx;
        rx2 = xMin + 0.80 * dx;
    }

    if (logY && yMax > 0.0) {
        const double safeYMin = (yMin > 0.0) ? yMin : std::max(1e-100, yMax * 1e-4);
        const double lyMin = std::log10(safeYMin);
        const double lyMax = std::log10(yMax);
        ry1 = std::pow(10.0, lyMin + 0.08 * (lyMax - lyMin));
        ry2 = std::pow(10.0, lyMin + 0.92 * (lyMax - lyMin));
    } else {
        const double dy = yMax - yMin;
        ry1 = yMin + 0.08 * dy;
        ry2 = yMin + 0.92 * dy;
    }

    d_roi_rect = QRectF(QPointF(rx1, ry1), QPointF(rx2, ry2)).normalized();
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

void RoiGadgetTool::setIntegrateBaseline(IntegrateBaseline b)
{
    d_integrate_baseline = b;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void RoiGadgetTool::setDetectNegativePeaks(bool on)
{
    d_detect_negative_peaks = on;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void RoiGadgetTool::setSavGolWindow(int windowSize)
{
    int w = std::max(3, windowSize);
    if (w % 2 == 0)
        ++w;
    d_sg_window = w;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void RoiGadgetTool::keepFittedCurveOnGraph()
{
    if (!d_graph_guard || d_overlay_curve_pts.size() < 2)
        return;

    if (!d_app)
        d_app = resolveAppWindowFromGraph(d_graph_guard);
    if (!d_app)
        return;

    const int nPts = d_overlay_curve_pts.size();
    std::vector<double> xs(static_cast<size_t>(nPts)), ys(static_cast<size_t>(nPts));
    for (int i = 0; i < nPts; ++i) {
        xs[size_t(i)] = d_overlay_curve_pts[i].x();
        ys[size_t(i)] = d_overlay_curve_pts[i].y();
    }

    QwtPlotCurve *target = resolveTargetCurve();
    const bool hasTarget = (target != nullptr);
    const int targetXAxis = hasTarget ? target->xAxis() : int(QwtPlot::xBottom);
    const int targetYAxis = hasTarget ? target->yAxis() : int(QwtPlot::yLeft);
    QString cleanBaseTitle;
    if (hasTarget && d_graph_guard) {
        const int tIdx = d_graph_guard->curveIndex(target);
        if (tIdx >= 0)
            cleanBaseTitle = d_graph_guard->resolvedAutoCurveLabel(tIdx).trimmed();
    }
    QString baseTitle = !cleanBaseTitle.isEmpty()
            ? cleanBaseTitle
            : (!d_curve_title.isEmpty()
                       ? d_curve_title
                       : (hasTarget && !target->title().text().isEmpty()
                                  ? target->title().text()
                                  : QStringLiteral("Curve")));
    if (cleanBaseTitle.isEmpty()) {
        if (DataCurve *tdc = dynamic_cast<DataCurve *>(target)) {
            if (Table *ttbl = tdc->table()) {
                const QString pfx = ttbl->objectName() + QLatin1Char('_');
                if (baseTitle.startsWith(pfx))
                    baseTitle = baseTitle.mid(pfx.length());
            }
        } else {
            const int us = baseTitle.indexOf(QLatin1Char('_'));
            if (us > 0
                && baseTitle.left(us).startsWith(QStringLiteral("Table"), Qt::CaseInsensitive))
                baseTitle = baseTitle.mid(us + 1);
        }
    }

    MultiLayer *ml = d_graph_guard->multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();

    QString uniqueTableName = d_app->generateUniqueName(QStringLiteral("GadgetFit_"));
    uniqueTableName.replace(QLatin1Char(' '), QLatin1Char('_'));

    QString sanitizedYName;
    sanitizedYName.reserve(baseTitle.size() + 8);
    for (const QChar &ch : baseTitle) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_'))
            sanitizedYName.append(ch);
        else if (!sanitizedYName.endsWith(QLatin1Char('_')))
            sanitizedYName.append(QLatin1Char('_'));
    }
    while (sanitizedYName.startsWith(QLatin1Char('_')))
        sanitizedYName.remove(0, 1);
    while (sanitizedYName.endsWith(QLatin1Char('_')))
        sanitizedYName.chop(1);
    if (sanitizedYName.isEmpty()
        || sanitizedYName.compare(QStringLiteral("X"), Qt::CaseInsensitive) == 0) {
        sanitizedYName = QStringLiteral("FitY");
    } else {
        sanitizedYName = QStringLiteral("Fit_") + sanitizedYName;
    }

    const QString curveName = QStringLiteral("QuickFit_%1").arg(baseTitle);
    Column *colX = new Column(QStringLiteral("X"), SciDAVis::ColumnMode::Numeric);
    Column *colY = new Column(sanitizedYName, SciDAVis::ColumnMode::Numeric);
    colX->setPlotDesignation(SciDAVis::X);
    colY->setPlotDesignation(SciDAVis::Y);
    if (DataCurve *tdc = dynamic_cast<DataCurve *>(target)) {
        if (Table *ttbl = tdc->table()) {
            const int txIdx = ttbl->colIndex(tdc->xColumnName());
            const int tyIdx = ttbl->colIndex(tdc->yColumnName());
            if (Column *txc = (txIdx >= 0 ? ttbl->column(txIdx) : nullptr)) {
                colX->setLongName(txc->longName());
                colX->setUnits(txc->units());
            }
            if (Column *tyc = (tyIdx >= 0 ? ttbl->column(tyIdx) : nullptr)) {
                colY->setLongName(tyc->longName());
                colY->setUnits(tyc->units());
            }
        }
    }
    for (int i = 0; i < nPts; ++i) {
        colX->setValueAt(i, xs[size_t(i)]);
        colY->setValueAt(i, ys[size_t(i)]);
    }

    Table *fitTbl = d_app->newHiddenTable(uniqueTableName, curveName,
                                          QList<Column *>() << colX << colY);
    if (!fitTbl) {
        delete colX;
        delete colY;
        return;
    }
    fitTbl->setColPlotDesignation(0, SciDAVis::X);
    fitTbl->setColName(0, QStringLiteral("X"));
    fitTbl->setColPlotDesignation(1, SciDAVis::Y);
    fitTbl->setColName(1, sanitizedYName);
    d_app->hideWindow(fitTbl);

    const bool hadMainLegend = (d_graph_guard->legend() != nullptr);
    const QString prevMainLegendText =
            hadMainLegend ? d_graph_guard->legend()->text() : QString();

    if (d_graph_guard->insertCurve(fitTbl, fitTbl->colName(0), fitTbl->colName(1), Graph::Line)) {
        const int newIdx = d_graph_guard->curves() - 1;
        if (QwtPlotCurve *added = (newIdx >= 0) ? d_graph_guard->curve(newIdx) : nullptr) {
            added->setItemAttribute(QwtPlotItem::Legend, false);
            if (hasTarget)
                added->setAxis(targetXAxis, targetYAxis);
            added->setPen(QPen(QColor(214, 39, 40), 2.0, Qt::SolidLine));
            added->setRenderHint(QwtPlotItem::RenderAntialiased, true);
        }
        if (!hadMainLegend) {
            d_graph_guard->removeLegend();
        } else if (Legend *mainLeg = d_graph_guard->legend()) {
            mainLeg->setText(prevMainLegendText);
        }
    }

    if (!d_summary_text.isEmpty()) {
        if (Legend *mrk = d_graph_guard->newLegend(d_summary_text)) {
            const int fontPt = qMax(9, d_graph_guard->legendFont().pointSize());
            QFont calloutFont(QStringLiteral("Arial"), fontPt, QFont::Normal);
            calloutFont.setHintingPreference(QFont::PreferNoHinting);
            mrk->setFont(calloutFont);
            mrk->setFrameStyle(Legend::Line);
            mrk->setBackgroundColor(QColor(255, 255, 255, 235));
            const QRectF roi = d_roi_rect.normalized();
            mrk->setOriginCoord(roi.center().x(), roi.bottom() - 0.12 * roi.height());
        }
    }

    d_graph_guard->updatePlot();
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Keep Fitted Curve on Graph"), beforeSnap);
    if (d_app)
        d_app->modifiedProject();
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
    d_diff_rows.clear();
    d_diff_preview_pts.clear();
    d_intersections.clear();
    d_has_baseline = false;
    d_has_rise_markers = false;
    d_has_interp_marker = false;
    d_has_ec50_marker = false;
    d_has_diff_marker = false;
    d_has_2d_markers = false;
    d_primary_metric = 0.0;
    d_secondary_metric = 0.0;

    const QRectF roi = d_roi_rect.normalized();

    // Multi-curve Intersect mode inspects ALL visible curves on d_graph_guard
    if (d_mode == Intersect) {
        if (!d_graph_guard) {
            d_secondary_metric = std::numeric_limits<double>::quiet_NaN();
            d_summary_text = tr("ROI Intersect: No graph");
            d_recalculating = false;
            return;
        }

        struct Seg {
            double x1, y1, x2, y2;
        };
        struct CurveSegSet {
            QString title;
            int xAxis = QwtPlot::xBottom;
            int yAxis = QwtPlot::yLeft;
            std::vector<Seg> segs;
        };

        std::vector<CurveSegSet> curveSets;
        bool axisSynced = false;
        for (int ci = 0; ci < d_graph_guard->curves(); ++ci) {
            const int ctype = d_graph_guard->curveType(ci);
            if (ctype == Graph::ErrorBars || ctype == Graph::Pie || ctype == Graph::Box)
                continue;
            QwtPlotCurve *c = d_graph_guard->curve(ci);
            if (!c || !c->isVisible() || c->rtti() != QwtPlotItem::Rtti_PlotCurve
                || c->dataSize() < 2)
                continue;
            if (!axisSynced && d_overlay_item) {
                d_overlay_item->setAxis(c->xAxis(), c->yAxis());
                axisSynced = true;
            }

            const DataCurve *dcCurve = dynamic_cast<const DataCurve *>(c);
            CurveSegSet cs;
            cs.title = c->title().text();
            cs.xAxis = c->xAxis();
            cs.yAxis = c->yAxis();
            bool hasPrev = false;
            double prevX = 0.0, prevY = 0.0;
            const int mPts = c->dataSize();
            for (int k = 0; k < mPts; ++k) {
                if (dcCurve && dcCurve->isPointMasked(k)) {
                    hasPrev = false;
                    continue;
                }
                const double px = c->x(k);
                const double py = c->y(k);
                if (!std::isfinite(px) || !std::isfinite(py)) {
                    hasPrev = false;
                    continue;
                }
                if (hasPrev)
                    cs.segs.push_back({ prevX, prevY, px, py });
                prevX = px;
                prevY = py;
                hasPrev = true;
            }
            if (!cs.segs.empty())
                curveSets.push_back(std::move(cs));
        }

        Plot *plotPtr = d_graph_guard->plotWidget();
        const int ovXAxis = d_overlay_item ? d_overlay_item->xAxis() : int(QwtPlot::xBottom);
        const int ovYAxis = d_overlay_item ? d_overlay_item->yAxis() : int(QwtPlot::yLeft);
        const double tolTU = 1e-9;

        for (size_t a = 0; a < curveSets.size(); ++a) {
            for (size_t b = a + 1; b < curveSets.size(); ++b) {
                // Skip curve pairs that do not share the exact same (xAxis, yAxis) coordinate system
                if (curveSets[a].xAxis != curveSets[b].xAxis
                    || curveSets[a].yAxis != curveSets[b].yAxis)
                    continue;

                QRectF pairRoi = roi;
                if (plotPtr && (curveSets[a].xAxis != ovXAxis || curveSets[a].yAxis != ovYAxis)) {
                    const int pxL = plotPtr->transform(ovXAxis, roi.left());
                    const int pxR = plotPtr->transform(ovXAxis, roi.right());
                    const int pyT = plotPtr->transform(ovYAxis, roi.top());
                    const int pyB = plotPtr->transform(ovYAxis, roi.bottom());
                    const double rx1 = plotPtr->invTransform(curveSets[a].xAxis, pxL);
                    const double rx2 = plotPtr->invTransform(curveSets[a].xAxis, pxR);
                    const double ry1 = plotPtr->invTransform(curveSets[a].yAxis, pyT);
                    const double ry2 = plotPtr->invTransform(curveSets[a].yAxis, pyB);
                    pairRoi = QRectF(QPointF(std::min(rx1, rx2), std::min(ry1, ry2)),
                                     QPointF(std::max(rx1, rx2), std::max(ry1, ry2)));
                }

                const double epsX = 1e-9 * std::max(1.0, pairRoi.width());
                const double epsY = 1e-9 * std::max(1.0, pairRoi.height());
                const double dupTolX = 1e-7 * std::max(1.0, pairRoi.width());
                const double dupTolY = 1e-7 * std::max(1.0, pairRoi.height());

                QVector<IntersectionRow> pairHits;
                for (const Seg &sa : curveSets[a].segs) {
                    const double saMinX = std::min(sa.x1, sa.x2);
                    const double saMaxX = std::max(sa.x1, sa.x2);
                    const double saMinY = std::min(sa.y1, sa.y2);
                    const double saMaxY = std::max(sa.y1, sa.y2);
                    if (saMaxX < pairRoi.left() - epsX || saMinX > pairRoi.right() + epsX
                        || saMaxY < pairRoi.top() - epsY || saMinY > pairRoi.bottom() + epsY)
                        continue;

                    const double dx1 = sa.x2 - sa.x1;
                    const double dy1 = sa.y2 - sa.y1;
                    for (const Seg &sb : curveSets[b].segs) {
                        const double sbMinX = std::min(sb.x1, sb.x2);
                        const double sbMaxX = std::max(sb.x1, sb.x2);
                        const double sbMinY = std::min(sb.y1, sb.y2);
                        const double sbMaxY = std::max(sb.y1, sb.y2);
                        if (sbMaxX < saMinX - epsX || sbMinX > saMaxX + epsX
                            || sbMaxY < saMinY - epsY || sbMinY > saMaxY + epsY)
                            continue;

                        const double dx2 = sb.x2 - sb.x1;
                        const double dy2 = sb.y2 - sb.y1;
                        const double denom = dx1 * dy2 - dy1 * dx2;
                        const double segScale =
                                (std::fabs(dx1) + std::fabs(dy1)) * (std::fabs(dx2) + std::fabs(dy2));
                        if (std::fabs(denom) <= 1e-13 * std::max(1.0, segScale))
                            continue;

                        const double qpx = sb.x1 - sa.x1;
                        const double qpy = sb.y1 - sa.y1;
                        const double t = (qpx * dy2 - qpy * dx2) / denom;
                        const double u = (qpx * dy1 - qpy * dx1) / denom;
                        if (t < -tolTU || t > 1.0 + tolTU || u < -tolTU || u > 1.0 + tolTU)
                            continue;

                        const double tc = std::clamp(t, 0.0, 1.0);
                        const double ix = sa.x1 + tc * dx1;
                        const double iy = sa.y1 + tc * dy1;
                        if (ix < pairRoi.left() - epsX || ix > pairRoi.right() + epsX
                            || iy < pairRoi.top() - epsY || iy > pairRoi.bottom() + epsY)
                            continue;

                        bool duplicate = false;
                        for (const auto &eh : pairHits) {
                            if (std::fabs(eh.x - ix) <= dupTolX
                                && std::fabs(eh.y - iy) <= dupTolY) {
                                duplicate = true;
                                break;
                            }
                        }
                        if (!duplicate)
                            pairHits.append({ curveSets[a].title, curveSets[b].title, ix, iy });
                    }
                }
                for (const auto &ph : pairHits)
                    d_intersections.append(ph);
            }
        }

        std::stable_sort(d_intersections.begin(), d_intersections.end(),
                         [](const IntersectionRow &lhs, const IntersectionRow &rhs) {
                             if (std::fabs(lhs.x - rhs.x) > 1e-12)
                                 return lhs.x < rhs.x;
                             return lhs.y < rhs.y;
                         });

        d_primary_metric = double(d_intersections.size());
        d_secondary_metric = d_intersections.isEmpty()
                ? std::numeric_limits<double>::quiet_NaN()
                : d_intersections.front().x;

        d_output_metrics << qMakePair(QString("Intersection_Count"), d_primary_metric);
        if (!d_intersections.isEmpty()) {
            d_output_metrics << qMakePair(QString("First_X"), d_intersections.front().x)
                             << qMakePair(QString("First_Y"), d_intersections.front().y);
            d_summary_text = tr("Curve Intersection (N = %1)\nFirst: X = %2, Y = %3\n%4 × %5")
                                     .arg(d_intersections.size())
                                     .arg(d_intersections.front().x, 0, 'g', 6)
                                     .arg(d_intersections.front().y, 0, 'g', 6)
                                     .arg(d_intersections.front().curveA)
                                     .arg(d_intersections.front().curveB);
        } else {
            d_summary_text = tr("Curve Intersection\n0 intersections in ROI");
        }

        if (d_app)
            d_app->setStatusBarText(d_summary_text.simplified());
        d_recalculating = false;
        return;
    }

    // 2D Surface / Spectrogram / Matrix Integration mode
    if (d_mode == Integrate2D) {
        Spectrogram *sciSpec = nullptr;
        QwtPlotSpectrogram *qwtSpec = findSpectrogramOnGraph(d_graph_guard, &sciSpec);
        if (qwtSpec) {
            if (d_overlay_item)
                d_overlay_item->setAxis(qwtSpec->xAxis(), qwtSpec->yAxis());

            Matrix *m = sciSpec ? sciSpec->matrix() : nullptr;
            QRectF surfRect;
            if (m && m->numRows() > 0 && m->numCols() > 0) {
                surfRect = QRectF(QPointF(m->xStart(), m->yStart()),
                                  QPointF(m->xEnd(), m->yEnd()))
                                   .normalized();
            } else {
                const QwtDoubleRect br = qwtSpec->boundingRect();
                surfRect = QRectF(br.left(), br.top(), br.width(), br.height()).normalized();
            }

            const QRectF dom = (surfRect.width() > 0.0 && surfRect.height() > 0.0)
                    ? roi.intersected(surfRect)
                    : roi;
            if (dom.isEmpty() || dom.width() <= 0.0 || dom.height() <= 0.0) {
                d_summary_text = tr("2D Integration\nROI outside matrix surface");
                d_recalculating = false;
                return;
            }

            auto evalZ = [&](double x, double y) -> double {
                if (m && m->numRows() > 0 && m->numCols() > 0)
                    return evalMatrixBilinear(m, x, y);
                const double v = qwtSpec->data().value(x, y);
                return std::isfinite(v) ? v : 0.0;
            };

            const double x0 = dom.left();
            const double x1 = dom.right();
            const double y0 = dom.top();
            const double y1 = dom.bottom();

            std::vector<double> gx;
            std::vector<double> gy;
            const int baseSub = 64;
            gx.reserve(size_t(baseSub + (m ? m->numCols() : 0) + 4));
            gy.reserve(size_t(baseSub + (m ? m->numRows() : 0) + 4));
            for (int k = 0; k <= baseSub; ++k) {
                gx.push_back(x0 + (x1 - x0) * double(k) / double(baseSub));
                gy.push_back(y0 + (y1 - y0) * double(k) / double(baseSub));
            }
            if (m && m->numCols() > 1) {
                const double xs = m->xStart(), xe = m->xEnd();
                for (int c = 0; c < m->numCols(); ++c) {
                    const double cx = xs + double(c) * (xe - xs) / double(m->numCols() - 1);
                    if (cx > x0 && cx < x1)
                        gx.push_back(cx);
                }
            }
            if (m && m->numRows() > 1) {
                const double ys = m->yStart(), ye = m->yEnd();
                for (int r = 0; r < m->numRows(); ++r) {
                    const double ry = ys + double(r) * (ye - ys) / double(m->numRows() - 1);
                    if (ry > y0 && ry < y1)
                        gy.push_back(ry);
                }
            }

            auto sortDedup = [](std::vector<double> &vec, double span) {
                std::sort(vec.begin(), vec.end());
                std::vector<double> clean;
                clean.reserve(vec.size());
                const double tol = 1e-12 * std::max(1.0, span);
                for (double v : vec) {
                    if (clean.empty() || std::fabs(v - clean.back()) > tol)
                        clean.push_back(v);
                }
                vec.swap(clean);
            };
            sortDedup(gx, x1 - x0);
            sortDedup(gy, y1 - y0);

            const size_t nx = gx.size();
            const size_t ny = gy.size();
            const double z00Corner = evalZ(x0, y0);
            const double z10Corner = evalZ(x1, y0);
            const double z01Corner = evalZ(x0, y1);
            const double z11Corner = evalZ(x1, y1);

            std::vector<double> zGrid(nx * ny, 0.0);
            std::vector<double> zNetGrid(nx * ny, 0.0);
            double zMax = -std::numeric_limits<double>::infinity();
            double peakX = 0.5 * (x0 + x1);
            double peakY = 0.5 * (y0 + y1);

            for (size_t j = 0; j < ny; ++j) {
                const double beta = (y1 > y0) ? ((gy[j] - y0) / (y1 - y0)) : 0.0;
                for (size_t i = 0; i < nx; ++i) {
                    const double alpha = (x1 > x0) ? ((gx[i] - x0) / (x1 - x0)) : 0.0;
                    const double zv = evalZ(gx[i], gy[j]);
                    const double zBase = (1.0 - alpha) * (1.0 - beta) * z00Corner
                            + alpha * (1.0 - beta) * z10Corner
                            + (1.0 - alpha) * beta * z01Corner
                            + alpha * beta * z11Corner;
                    zGrid[j * nx + i] = zv;
                    zNetGrid[j * nx + i] = zv - zBase;
                    if (zv > zMax) {
                        zMax = zv;
                        peakX = gx[i];
                        peakY = gy[j];
                    }
                }
            }

            double totalVolume = 0.0;
            double netVolume = 0.0;
            double absMass = 0.0;
            double momX = 0.0;
            double momY = 0.0;

            for (size_t j = 0; j + 1 < ny; ++j) {
                const double dyCell = gy[j + 1] - gy[j];
                const double ycCell = 0.5 * (gy[j] + gy[j + 1]);
                for (size_t i = 0; i + 1 < nx; ++i) {
                    const double dxCell = gx[i + 1] - gx[i];
                    const double xcCell = 0.5 * (gx[i] + gx[i + 1]);
                    const double dA = dxCell * dyCell;
                    const double zAvg = 0.25
                            * (zGrid[j * nx + i] + zGrid[j * nx + (i + 1)]
                               + zGrid[(j + 1) * nx + i] + zGrid[(j + 1) * nx + (i + 1)]);
                    const double zNetAvg = 0.25
                            * (zNetGrid[j * nx + i] + zNetGrid[j * nx + (i + 1)]
                               + zNetGrid[(j + 1) * nx + i] + zNetGrid[(j + 1) * nx + (i + 1)]);
                    totalVolume += zAvg * dA;
                    netVolume += zNetAvg * dA;

                    const double w = std::fabs(zAvg) * dA;
                    absMass += w;
                    momX += xcCell * w;
                    momY += ycCell * w;
                }
            }

            const double centX = (absMass > 1e-24) ? (momX / absMass) : (0.5 * (x0 + x1));
            const double centY = (absMass > 1e-24) ? (momY / absMass) : (0.5 * (y0 + y1));
            if (!std::isfinite(zMax))
                zMax = 0.0;

            d_primary_metric = totalVolume;
            d_secondary_metric = netVolume;
            d_centroid_x = centX;
            d_centroid_y = centY;
            d_peak2d_x = peakX;
            d_peak2d_y = peakY;
            d_has_2d_markers = true;

            d_output_metrics << qMakePair(QString("Total_Volume"), totalVolume)
                             << qMakePair(QString("Net_Volume"), netVolume)
                             << qMakePair(QString("Peak_Z_Max"), zMax)
                             << qMakePair(QString("Peak_X"), peakX)
                             << qMakePair(QString("Peak_Y"), peakY)
                             << qMakePair(QString("Centroid_X"), centX)
                             << qMakePair(QString("Centroid_Y"), centY)
                             << qMakePair(QString("ROI_Area"), (x1 - x0) * (y1 - y0));

            const QString surfLabel = m ? m->name() : qwtSpec->title().text();
            d_summary_text =
                    tr("2D Integrate [%1]\nVol = %2, Net Vol = %3\nZmax = %4, Centroid = (%5, %6)")
                            .arg(surfLabel)
                            .arg(totalVolume, 0, 'g', 6)
                            .arg(netVolume, 0, 'g', 6)
                            .arg(zMax, 0, 'g', 5)
                            .arg(centX, 0, 'g', 5)
                            .arg(centY, 0, 'g', 5);
            if (d_app)
                d_app->setStatusBarText(d_summary_text.simplified());
            d_recalculating = false;
            return;
        }
    }

    QwtPlotCurve *curve = resolveTargetCurve();
    if (!curve || curve->dataSize() <= 0) {
        d_summary_text = tr("ROI Gadget: No curve data");
        d_recalculating = false;
        return;
    }

    DataCurve *dc = dynamic_cast<DataCurve *>(curve);
    const int nTotal = curve->dataSize();

    std::vector<XYPoint> pts;
    pts.reserve(size_t(nTotal));

    const double roiEpsX = 1e-9 * std::max(1.0, roi.width());
    const double roiEpsY = 1e-9 * std::max(1.0, roi.height());
    for (int i = 0; i < nTotal; ++i) {
        const double px = curve->x(i);
        const double py = curve->y(i);
        if (!std::isfinite(px) || !std::isfinite(py))
            continue;
        if (px < roi.left() - roiEpsX || px > roi.right() + roiEpsX)
            continue;
        // Integrate uses the ROI's X range only (as Origin's Integrate gadget and the Delta
        // Cursor do); dropping points by Y would cut peaks that stick out of the box.
        if (d_mode != Integrate && (py < roi.top() - roiEpsY || py > roi.bottom() + roiEpsY))
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

    QString curveDisplayLabel;
    if (d_graph_guard) {
        const int cIdx = d_graph_guard->curveIndex(curve);
        if (cIdx >= 0)
            curveDisplayLabel = d_graph_guard->resolvedAutoCurveLabel(cIdx).trimmed();
    }
    if (curveDisplayLabel.isEmpty()) {
        curveDisplayLabel = curve->title().text().trimmed();
        if (dc && dc->table()) {
            const QString pfx = dc->table()->objectName() + QLatin1Char('_');
            if (curveDisplayLabel.startsWith(pfx))
                curveDisplayLabel = curveDisplayLabel.mid(pfx.length());
        } else {
            const int us = curveDisplayLabel.indexOf(QLatin1Char('_'));
            if (us > 0
                && curveDisplayLabel.left(us).startsWith(QStringLiteral("Table"),
                                                         Qt::CaseInsensitive))
                curveDisplayLabel = curveDisplayLabel.mid(us + 1);
        }
    }

    if (pts.empty()) {
        if (d_mode == UnmaskRegion) {
            d_summary_text = QString("%1 [%2]\n%3")
                                     .arg(tr("Unmask ROI"))
                                     .arg(curveDisplayLabel)
                                     .arg(tr("0 masked points in ROI"));
        } else {
            d_summary_text = QString("%1 [%2]\n%3")
                                     .arg(tr("ROI Gadget"))
                                     .arg(curveDisplayLabel)
                                     .arg(tr("No unmasked points in ROI"));
        }
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
        d_summary_text = tr("%1 [%2]\nN = %3, Mean Y = %4\nMean X = %5")
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

    // Sort by X ascending for 1D curve gadgets ONLY when the curve is not cyclic / parametric
    const bool hasXReversals =
            sequenceHasXReversals(pts, [](const XYPoint &p) { return p.x; });
    if (!hasXReversals) {
        std::stable_sort(pts.begin(), pts.end(),
                         [](const XYPoint &a, const XYPoint &b) { return a.x < b.x; });
    }

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

        d_summary_text = QString("%1 [%2] (N=%3)\n%4 = %5, %6 = %7\n%8 = %9, %10 = %11")
                                 .arg(tr("Region Stats"))
                                 .arg(curve->title().text())
                                 .arg(n)
                                 .arg(tr("Mean"))
                                 .arg(meanY, 0, 'g', 6)
                                 .arg(tr("SD"))
                                 .arg(sd, 0, 'g', 6)
                                 .arg(tr("Min"))
                                 .arg(minY, 0, 'g', 6)
                                 .arg(tr("Max"))
                                 .arg(maxY, 0, 'g', 6);
        break;
    }

    case QuickFit: {
        const int nGrid = 120;
        const double xStart = roi.left();
        const double xEnd = roi.right();
        Plot *plotPtr = d_graph_guard ? d_graph_guard->plotWidget() : nullptr;
        const bool logX = isAxisLog10(plotPtr, curve->xAxis()) && xStart > 0.0 && xEnd > xStart;
        const bool logY = isAxisLog10(plotPtr, curve->yAxis());
        auto gridX = [&](int k) -> double {
            const double t = double(k) / double(nGrid - 1);
            if (logX) {
                const double lx0 = std::log10(xStart);
                const double lx1 = std::log10(xEnd);
                return std::pow(10.0, lx0 + t * (lx1 - lx0));
            }
            return xStart + (xEnd - xStart) * t;
        };
        auto appendCurvePt = [&](double gx, double gy) {
            if (!std::isfinite(gx) || !std::isfinite(gy))
                return;
            if (logY && gy <= 0.0)
                gy = std::max(1e-100, minY > 0.0 ? minY * 0.1 : 1e-12);
            d_overlay_curve_pts.append(QPointF(gx, gy));
        };

        std::vector<double> yHat(n, meanY);
        QString modelLabel;
        QString paramLine;

        if (d_fit_model == FitLinear && n >= 2) {
            double c0 = 0.0, c1 = 0.0, cov00 = 0.0, cov01 = 0.0, cov11 = 0.0, sumsq = 0.0;
            gsl_fit_linear(xv.data(), 1, yv.data(), 1, n, &c0, &c1, &cov00, &cov01, &cov11, &sumsq);
            const double se0 = (n > 2 && cov00 > 0.0) ? std::sqrt(cov00) : 0.0;
            const double se1 = (n > 2 && cov11 > 0.0) ? std::sqrt(cov11) : 0.0;
            for (size_t i = 0; i < n; ++i)
                yHat[i] = c0 + c1 * xv[i];
            const double r2 = computeR2(yv, yHat);
            d_primary_metric = r2;
            d_secondary_metric = c1;
            modelLabel = tr("Linear");
            paramLine = QString("%1 = %2 ± %3, %4 = %5 ± %6")
                                .arg(tr("Slope"))
                                .arg(c1, 0, 'g', 5)
                                .arg(se1, 0, 'g', 3)
                                .arg(tr("Intercept"))
                                .arg(c0, 0, 'g', 5)
                                .arg(se0, 0, 'g', 3);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("Slope"), c1)
                             << qMakePair(QString("Slope_SE"), se1)
                             << qMakePair(QString("Intercept"), c0)
                             << qMakePair(QString("Intercept_SE"), se0)
                             << qMakePair(QString("RSS"), sumsq);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = gridX(k);
                appendCurvePt(gx, c0 + c1 * gx);
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
            const double se0 = (n > 3 && gsl_matrix_get(cov, 0, 0) > 0.0)
                    ? std::sqrt(gsl_matrix_get(cov, 0, 0))
                    : 0.0;
            const double se1 = (n > 3 && gsl_matrix_get(cov, 1, 1) > 0.0)
                    ? std::sqrt(gsl_matrix_get(cov, 1, 1))
                    : 0.0;
            const double se2 = (n > 3 && gsl_matrix_get(cov, 2, 2) > 0.0)
                    ? std::sqrt(gsl_matrix_get(cov, 2, 2))
                    : 0.0;
            gsl_matrix_free(X);
            gsl_vector_free(Y);
            gsl_vector_free(C);
            gsl_matrix_free(cov);

            for (size_t i = 0; i < n; ++i)
                yHat[i] = c0 + c1 * xv[i] + c2 * xv[i] * xv[i];
            const double r2 = computeR2(yv, yHat);
            d_primary_metric = r2;
            d_secondary_metric = c2;
            modelLabel = tr("Polynomial (Order 2)");
            paramLine = QString("c2 = %1 ± %2, c1 = %3 ± %4")
                                .arg(c2, 0, 'g', 4)
                                .arg(se2, 0, 'g', 3)
                                .arg(c1, 0, 'g', 4)
                                .arg(se1, 0, 'g', 3);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("c0"), c0)
                             << qMakePair(QString("c0_SE"), se0)
                             << qMakePair(QString("c1"), c1)
                             << qMakePair(QString("c1_SE"), se1)
                             << qMakePair(QString("c2"), c2)
                             << qMakePair(QString("c2_SE"), se2);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = gridX(k);
                appendCurvePt(gx, c0 + c1 * gx + c2 * gx * gx);
            }
        } else if (d_fit_model == FitGauss && n >= 4) {
            const double fwhmGuess = estimateFwhm(xv, yv, minY, maxIdx);
            const double kFwhm = std::sqrt(2.0 * std::log(2.0));
            const double wGuess = std::max(1e-9, fwhmGuess / kFwhm);
            std::vector<double> p = { minY, std::max(1e-9, maxY - minY), xAtMax, wGuess };
            std::vector<double> se;
            runLmSolver(2, xv, yv, xv.front(), p, &se);
            p[3] = std::fabs(p[3]);
            for (size_t i = 0; i < n; ++i) {
                const double u = (xv[i] - p[2]) / std::max(1e-12, p[3]);
                yHat[i] = p[0] + p[1] * std::exp(std::max(-500.0, -2.0 * u * u));
            }
            const double r2 = computeR2(yv, yHat);
            const double fwhm = p[3] * kFwhm;
            const double seXc = (se.size() > 2) ? se[2] : 0.0;
            const double seFwhm = (se.size() > 3) ? (se[3] * kFwhm) : 0.0;
            const double seAmp = (se.size() > 1) ? se[1] : 0.0;
            const double seY0 = (se.size() > 0) ? se[0] : 0.0;
            d_primary_metric = r2;
            d_secondary_metric = p[2]; // xc
            modelLabel = tr("Gauss");
            paramLine = QString("xc = %1 ± %2, FWHM = %3 ± %4")
                                .arg(p[2], 0, 'g', 5)
                                .arg(seXc, 0, 'g', 3)
                                .arg(fwhm, 0, 'g', 4)
                                .arg(seFwhm, 0, 'g', 3);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("xc"), p[2])
                             << qMakePair(QString("xc_SE"), seXc)
                             << qMakePair(QString("FWHM"), fwhm)
                             << qMakePair(QString("FWHM_SE"), seFwhm)
                             << qMakePair(QString("Amplitude"), p[1])
                             << qMakePair(QString("Amplitude_SE"), seAmp)
                             << qMakePair(QString("y0"), p[0])
                             << qMakePair(QString("y0_SE"), seY0);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = gridX(k);
                const double u = (gx - p[2]) / std::max(1e-12, p[3]);
                appendCurvePt(gx, p[0] + p[1] * std::exp(std::max(-500.0, -2.0 * u * u)));
            }
        } else if (d_fit_model == FitLorentz && n >= 4) {
            const double fwhmGuess = estimateFwhm(xv, yv, minY, maxIdx);
            std::vector<double> p = { minY, std::max(1e-9, maxY - minY), xAtMax, fwhmGuess };
            std::vector<double> se;
            runLmSolver(3, xv, yv, xv.front(), p, &se);
            p[3] = std::fabs(p[3]);
            for (size_t i = 0; i < n; ++i) {
                const double dx = xv[i] - p[2];
                const double w2 = p[3] * p[3];
                yHat[i] = p[0] + p[1] * w2 / std::max(1e-30, 4.0 * dx * dx + w2);
            }
            const double r2 = computeR2(yv, yHat);
            const double seXc = (se.size() > 2) ? se[2] : 0.0;
            const double seFwhm = (se.size() > 3) ? se[3] : 0.0;
            const double seH = (se.size() > 1) ? se[1] : 0.0;
            const double seY0 = (se.size() > 0) ? se[0] : 0.0;
            d_primary_metric = r2;
            d_secondary_metric = p[2]; // xc
            modelLabel = tr("Lorentz");
            paramLine = QString("xc = %1 ± %2, FWHM = %3 ± %4")
                                .arg(p[2], 0, 'g', 5)
                                .arg(seXc, 0, 'g', 3)
                                .arg(p[3], 0, 'g', 4)
                                .arg(seFwhm, 0, 'g', 3);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("xc"), p[2])
                             << qMakePair(QString("xc_SE"), seXc)
                             << qMakePair(QString("FWHM"), p[3])
                             << qMakePair(QString("FWHM_SE"), seFwhm)
                             << qMakePair(QString("Height"), p[1])
                             << qMakePair(QString("Height_SE"), seH)
                             << qMakePair(QString("y0"), p[0])
                             << qMakePair(QString("y0_SE"), seY0);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = gridX(k);
                const double dx = gx - p[2];
                const double w2 = p[3] * p[3];
                appendCurvePt(gx, p[0] + p[1] * w2 / std::max(1e-30, 4.0 * dx * dx + w2));
            }
        } else if (d_fit_model == FitExpDec1 && n >= 3) {
            const double xRef = xv.front();
            const double span = std::max(1e-9, xv.back() - xv.front());
            std::vector<double> p = { yv.back(), yv.front() - yv.back(), span / 3.0 };
            std::vector<double> se;
            runLmSolver(4, xv, yv, xRef, p, &se);
            for (size_t i = 0; i < n; ++i) {
                const double u = std::clamp((xv[i] - xRef) / p[2], -500.0, 500.0);
                yHat[i] = p[0] + p[1] * std::exp(-u);
            }
            const double r2 = computeR2(yv, yHat);
            const double seT1 = (se.size() > 2) ? se[2] : 0.0;
            const double seA = (se.size() > 1) ? se[1] : 0.0;
            const double seY0 = (se.size() > 0) ? se[0] : 0.0;
            d_primary_metric = r2;
            d_secondary_metric = p[2]; // t1
            modelLabel = tr("Exponential Decay (ExpDec1)");
            paramLine = QString("t1 = %1 ± %2, A = %3 ± %4")
                                .arg(p[2], 0, 'g', 5)
                                .arg(seT1, 0, 'g', 3)
                                .arg(p[1], 0, 'g', 4)
                                .arg(seA, 0, 'g', 3);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("t1"), p[2])
                             << qMakePair(QString("t1_SE"), seT1)
                             << qMakePair(QString("A"), p[1])
                             << qMakePair(QString("A_SE"), seA)
                             << qMakePair(QString("y0"), p[0])
                             << qMakePair(QString("y0_SE"), seY0);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = gridX(k);
                const double u = std::clamp((gx - xRef) / p[2], -500.0, 500.0);
                appendCurvePt(gx, p[0] + p[1] * std::exp(-u));
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
            std::vector<double> se;
            runLmSolver(5, xv, yv, xv.front(), p, &se);
            for (size_t i = 0; i < n; ++i) {
                const double u = std::clamp((xv[i] - p[2]) / p[3], -500.0, 500.0);
                yHat[i] = p[1] + (p[0] - p[1]) / (1.0 + std::exp(u));
            }
            const double r2 = computeR2(yv, yHat);
            const double seX0 = (se.size() > 2) ? se[2] : 0.0;
            const double seDx = (se.size() > 3) ? se[3] : 0.0;
            const double seA1 = (se.size() > 0) ? se[0] : 0.0;
            const double seA2 = (se.size() > 1) ? se[1] : 0.0;
            d_primary_metric = r2;
            d_secondary_metric = p[2]; // x0
            modelLabel = tr("Boltzmann Sigmoidal");
            paramLine = QString("x0 = %1 ± %2, dx = %3 ± %4")
                                .arg(p[2], 0, 'g', 5)
                                .arg(seX0, 0, 'g', 3)
                                .arg(p[3], 0, 'g', 4)
                                .arg(seDx, 0, 'g', 3);
            d_output_metrics << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("x0"), p[2])
                             << qMakePair(QString("x0_SE"), seX0)
                             << qMakePair(QString("dx"), p[3])
                             << qMakePair(QString("dx_SE"), seDx)
                             << qMakePair(QString("A1"), p[0])
                             << qMakePair(QString("A1_SE"), seA1)
                             << qMakePair(QString("A2"), p[1])
                             << qMakePair(QString("A2_SE"), seA2);
            for (int k = 0; k < nGrid; ++k) {
                const double gx = gridX(k);
                const double u = std::clamp((gx - p[2]) / p[3], -500.0, 500.0);
                appendCurvePt(gx, p[1] + (p[0] - p[1]) / (1.0 + std::exp(u)));
            }
        } else {
            d_summary_text = QString("%1 [%2]\n%3 (N=%4)")
                                     .arg(tr("Quick Fit"))
                                     .arg(curveDisplayLabel)
                                     .arg(tr("Need more points in ROI"))
                                     .arg(n);
            break;
        }

        d_summary_text = QString("%1 (%2)\n%3 = %4\n%5")
                                 .arg(modelLabel)
                                 .arg(curveDisplayLabel)
                                 .arg(tr("R\u00B2"))
                                 .arg(QLocale().toString(d_primary_metric, 'f', 4))
                                 .arg(paramLine);
        break;
    }

    case QuickPeaks: {
        std::vector<double> sortedY = yv;
        std::sort(sortedY.begin(), sortedY.end());
        const double yBase = d_detect_negative_peaks
                ? sortedY[std::min(n - 1, (9 * n) / 10)]
                : sortedY[std::min(n - 1, n / 10)];
        d_has_baseline = true;
        d_baseline_y0 = yBase;
        d_baseline_y1 = yBase;

        std::vector<double> ys = yv;
        if (n >= 5) {
            for (size_t i = 1; i + 1 < n; ++i)
                ys[i] = 0.25 * yv[i - 1] + 0.5 * yv[i] + 0.25 * yv[i + 1];
        }

        const double promThresh = d_detect_negative_peaks
                ? std::max(1e-12, 0.08 * (yBase - minY))
                : std::max(1e-12, 0.08 * (maxY - yBase));
        const int minSep = std::max(1, static_cast<int>(n / 25));
        struct Cand {
            int idx;
            double prom;
        };
        std::vector<Cand> cands;
        for (int i = 1; i + 1 < static_cast<int>(n); ++i) {
            if (d_detect_negative_peaks) {
                const bool localMin = (ys[size_t(i)] <= ys[size_t(i - 1)]
                                       && ys[size_t(i)] <= ys[size_t(i + 1)]
                                       && (ys[size_t(i)] < ys[size_t(i - 1)]
                                           || ys[size_t(i)] < ys[size_t(i + 1)]));
                const double prom = yBase - yv[size_t(i)];
                if (localMin && prom >= promThresh)
                    cands.push_back({ i, prom });
            } else {
                const bool localMax = (ys[size_t(i)] >= ys[size_t(i - 1)]
                                       && ys[size_t(i)] >= ys[size_t(i + 1)]
                                       && (ys[size_t(i)] > ys[size_t(i - 1)]
                                           || ys[size_t(i)] > ys[size_t(i + 1)]));
                const double prom = yv[size_t(i)] - yBase;
                if (localMax && prom >= promThresh)
                    cands.push_back({ i, prom });
            }
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

        double maxPeakCenter = d_detect_negative_peaks ? xAtMin : xAtMax;
        double maxPeakMag = -1.0;
        for (int idx : chosenIdx) {
            DetectedPeak pk;
            pk.xc = xv[size_t(idx)];
            pk.yTop = yv[size_t(idx)];
            pk.yBase = yBase;
            pk.height = pk.yTop - yBase;
            pk.fwhm = estimateFwhm(xv, yv, yBase, idx);
            pk.area = 1.064467 * pk.height * pk.fwhm;
            d_detected_peaks.append(pk);
            if (std::fabs(pk.height) > maxPeakMag) {
                maxPeakMag = std::fabs(pk.height);
                maxPeakCenter = pk.xc;
            }
        }

        d_primary_metric = double(d_detected_peaks.size());
        d_secondary_metric = (d_detected_peaks.isEmpty()) ? 0.0 : maxPeakCenter;

        d_output_metrics << qMakePair(QString("Peak_Count"), d_primary_metric)
                         << qMakePair(QString("Max_Peak_Center"), d_secondary_metric)
                         << qMakePair(QString("Baseline"), yBase);

        const QString peakHeader =
                d_detect_negative_peaks ? tr("Quick Troughs") : tr("Quick Peaks");
        d_summary_text = QString("%1 [%2]\n%3 = %4, xc = %5\n%6 = %7")
                                 .arg(peakHeader)
                                 .arg(curve->title().text())
                                 .arg(tr("Count"))
                                 .arg(d_detected_peaks.size())
                                 .arg(d_secondary_metric, 0, 'g', 6)
                                 .arg(tr("Baseline"))
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
            std::vector<double> se;
            runLmSolver(5, xv, yv, xv.front(), p, &se);

            std::vector<double> yHat(n);
            for (size_t i = 0; i < n; ++i) {
                const double u = std::clamp((xv[i] - p[2]) / p[3], -500.0, 500.0);
                yHat[i] = p[1] + (p[0] - p[1]) / (1.0 + std::exp(u));
            }
            const double r2 = computeR2(yv, yHat);
            const double seEc50 = (se.size() > 2) ? se[2] : 0.0;
            d_primary_metric = p[2]; // EC50
            d_secondary_metric = r2;
            d_ec50_x = p[2];
            d_ec50_y = 0.5 * (p[0] + p[1]);
            d_has_ec50_marker = true;

            Plot *plotPtr = d_graph_guard ? d_graph_guard->plotWidget() : nullptr;
            const bool logX = isAxisLog10(plotPtr, curve->xAxis()) && roi.left() > 0.0
                    && roi.right() > roi.left();
            const bool logY = isAxisLog10(plotPtr, curve->yAxis());
            const int nGrid = 120;
            for (int k = 0; k < nGrid; ++k) {
                const double t = double(k) / double(nGrid - 1);
                const double gx = logX
                        ? std::pow(10.0,
                                   std::log10(roi.left())
                                           + t * (std::log10(roi.right()) - std::log10(roi.left())))
                        : (roi.left() + roi.width() * t);
                const double u = std::clamp((gx - p[2]) / p[3], -500.0, 500.0);
                double gy = p[1] + (p[0] - p[1]) / (1.0 + std::exp(u));
                if (logY && gy <= 0.0)
                    gy = std::max(1e-100, minY > 0.0 ? minY * 0.1 : 1e-12);
                d_overlay_curve_pts.append(QPointF(gx, gy));
            }

            d_output_metrics << qMakePair(QString("EC50"), p[2])
                             << qMakePair(QString("EC50_SE"), seEc50)
                             << qMakePair(QString("R2"), r2)
                             << qMakePair(QString("A1_Bottom"), p[0])
                             << qMakePair(QString("A2_Top"), p[1])
                             << qMakePair(QString("dx_SlopeWidth"), p[3]);

            d_summary_text = QString("%1 [%2]\nEC50 = %3, R² = %4\nA1 = %5, A2 = %6")
                                     .arg(tr("Quick Sigmoidal"))
                                     .arg(curve->title().text())
                                     .arg(p[2], 0, 'g', 6)
                                     .arg(r2, 0, 'f', 5)
                                     .arg(p[0], 0, 'g', 5)
                                     .arg(p[1], 0, 'g', 5);
        } else {
            d_summary_text = QString("%1 [%2]\n%3")
                                     .arg(tr("Quick Sigmoidal"))
                                     .arg(curve->title().text())
                                     .arg(tr("Need >= 4 points in ROI"));
        }
        break;
    }

    case Integrate: {
        // Endpoints: linearly interpolate the curve at roi.left() / roi.right() (clamped to the
        // data range) exactly like the Delta Cursor, instead of starting and ending at the first
        // and last sample inside the box. Only for curves monotonic in X.
        std::vector<double> ixv = xv;
        std::vector<double> iyv = yv;
        if (!hasXReversals) {
            std::vector<std::pair<double, double>> full;
            full.reserve(size_t(nTotal));
            for (int i = 0; i < nTotal; ++i) {
                const double px = curve->x(i);
                const double py = curve->y(i);
                if (!std::isfinite(px) || !std::isfinite(py))
                    continue;
                if (dc && dc->isPointMasked(i))
                    continue;
                full.emplace_back(px, py);
            }
            const bool fullRev =
                    sequenceHasXReversals(full, [](const std::pair<double, double> &p) { return p.first; });
            if (!fullRev && full.size() >= 2) {
                std::stable_sort(full.begin(), full.end(),
                                 [](const auto &p, const auto &q) { return p.first < q.first; });
                const double lo = std::max(roi.left(), full.front().first);
                const double hi = std::min(roi.right(), full.back().first);
                auto interpFull = [&](double xq) -> double {
                    auto it = std::lower_bound(full.begin(), full.end(), xq,
                                               [](const auto &pt, double v) { return pt.first < v; });
                    if (it == full.begin())
                        return full.front().second;
                    if (it == full.end())
                        return full.back().second;
                    const auto &p1 = *it;
                    const auto &p0 = *(it - 1);
                    const double dx = p1.first - p0.first;
                    return (std::fabs(dx) > 1e-300)
                            ? (p0.second + (xq - p0.first) / dx * (p1.second - p0.second))
                            : p1.second;
                };
                if (hi > lo) {
                    ixv.clear();
                    iyv.clear();
                    const double tolX = 1e-12 * std::max(1.0, hi - lo);
                    ixv.push_back(lo);
                    iyv.push_back(interpFull(lo));
                    for (const auto &p : full) {
                        if (p.first > lo + tolX && p.first < hi - tolX) {
                            ixv.push_back(p.first);
                            iyv.push_back(p.second);
                        }
                    }
                    ixv.push_back(hi);
                    iyv.push_back(interpFull(hi));
                }
            }
        }
        const size_t nI = ixv.size();
        double iMinY = iyv.front(), iMaxY = iyv.front(), iXAtMax = ixv.front();
        int iMaxIdx = 0;
        for (size_t i = 1; i < nI; ++i) {
            iMinY = std::min(iMinY, iyv[i]);
            if (iyv[i] > iMaxY) {
                iMaxY = iyv[i];
                iXAtMax = ixv[i];
                iMaxIdx = static_cast<int>(i);
            }
        }

        const double xFront = ixv.front();
        const double xBack = ixv.back();
        const double yFront = iyv.front();
        const double yBack = iyv.back();
        const double xSpan = xBack - xFront;

        auto evalBaseline = [&](double xVal) -> double {
            if (d_integrate_baseline == BaseEndpointsLine) {
                if (std::fabs(xSpan) <= 1e-30)
                    return 0.5 * (yFront + yBack);
                return yFront + ((yBack - yFront) / xSpan) * (xVal - xFront);
            }
            if (d_integrate_baseline == BaseRoiMin)
                return iMinY;
            return 0.0;
        };

        double area = 0.0;
        double absArea = 0.0;
        double absMomX = 0.0;
        for (size_t i = 0; i + 1 < nI; ++i) {
            const double dx = ixv[i + 1] - ixv[i];
            if (!hasXReversals && dx <= 0.0)
                continue;
            const double adx = std::fabs(dx);
            if (adx <= 0.0)
                continue;
            const double h0 = iyv[i] - evalBaseline(ixv[i]);
            const double h1 = iyv[i + 1] - evalBaseline(ixv[i + 1]);
            area += 0.5 * (h0 + h1) * dx;

            if (h0 * h1 >= 0.0) {
                const double a0 = std::fabs(h0);
                const double a1 = std::fabs(h1);
                const double segAbs = 0.5 * (a0 + a1) * adx;
                absArea += segAbs;
                const double denom = a0 + a1;
                const double xSegCent = (denom > 1e-30)
                        ? (ixv[i] + dx * (a0 + 2.0 * a1) / (3.0 * denom))
                        : (0.5 * (ixv[i] + ixv[i + 1]));
                absMomX += xSegCent * segAbs;
            } else {
                const double a0 = std::fabs(h0);
                const double a1 = std::fabs(h1);
                const double tZero = a0 / std::max(1e-30, a0 + a1);
                const double xZero = ixv[i] + tZero * dx;
                const double segAbsL = 0.5 * a0 * std::fabs(xZero - ixv[i]);
                const double segAbsR = 0.5 * a1 * std::fabs(ixv[i + 1] - xZero);
                absArea += segAbsL + segAbsR;
                absMomX += ((2.0 * ixv[i] + xZero) / 3.0) * segAbsL
                        + ((xZero + 2.0 * ixv[i + 1]) / 3.0) * segAbsR;
            }
        }

        const double centroidX =
                (absArea > 1e-30) ? (absMomX / absArea) : (0.5 * (xFront + xBack));
        const double fwhm = (nI >= 3) ? estimateFwhm(ixv, iyv, evalBaseline(iXAtMax), iMaxIdx) : 0.0;
        d_primary_metric = area;
        d_secondary_metric = fwhm;

        if (d_integrate_baseline != BaseYZero) {
            d_has_baseline = true;
            d_baseline_y0 = evalBaseline(roi.left());
            d_baseline_y1 = evalBaseline(roi.right());
        }

        Plot *plotPtr = d_graph_guard ? d_graph_guard->plotWidget() : nullptr;
        const bool logY = isAxisLog10(plotPtr, curve->yAxis());
        auto safePolyY = [&](double yVal) -> double {
            if (logY && yVal <= 0.0)
                return std::max(1e-100, iMinY > 0.0 ? iMinY * 0.5 : std::max(1e-12, roi.top()));
            return yVal;
        };

        if (nI >= 2) {
            d_integrate_poly_pts.reserve(static_cast<int>(nI) + 2);
            d_integrate_poly_pts.append(QPointF(xFront, safePolyY(evalBaseline(xFront))));
            for (size_t i = 0; i < nI; ++i)
                d_integrate_poly_pts.append(QPointF(ixv[i], safePolyY(iyv[i])));
            d_integrate_poly_pts.append(QPointF(xBack, safePolyY(evalBaseline(xBack))));
        }

        d_output_metrics << qMakePair(QString("Area"), area)
                         << qMakePair(QString("Absolute_Area"), absArea)
                         << qMakePair(QString("Centroid_X"), centroidX)
                         << qMakePair(QString("FWHM"), fwhm)
                         << qMakePair(QString("Peak_Height"), iMaxY - evalBaseline(iXAtMax))
                         << qMakePair(QString("Peak_X"), iXAtMax);

        d_summary_text =
                QString("%1 [%2]\n%3 = %4, %5 = %6\n%7 = %8, FWHM = %9")
                        .arg(tr("Integrate"))
                        .arg(curve->title().text())
                        .arg(tr("Area"))
                        .arg(area, 0, 'g', 6)
                        .arg(tr("Abs Area"))
                        .arg(absArea, 0, 'g', 6)
                        .arg(tr("Centroid X"))
                        .arg(centroidX, 0, 'g', 6)
                        .arg(fwhm, 0, 'g', 5);
        break;
    }

    case Interpolate: {
        Plot *plotPtr = d_graph_guard ? d_graph_guard->plotWidget() : nullptr;
        const bool logX = isAxisLog10(plotPtr, curve->xAxis()) && roi.left() > 0.0
                && roi.right() > 0.0;
        const double rawMid =
                logX ? std::sqrt(roi.left() * roi.right()) : (0.5 * (roi.left() + roi.right()));
        double xMid = rawMid;
        double midY = yv.front();
        double slope = 0.0;

        if (hasXReversals) {
            const auto mm = std::minmax_element(xv.begin(), xv.end());
            xMid = std::clamp(rawMid, *mm.first, *mm.second);
            bool foundCross = false;
            for (size_t i = 0; i + 1 < n; ++i) {
                const double x0 = xv[i], x1 = xv[i + 1];
                if ((x0 <= xMid && xMid <= x1) || (x1 <= xMid && xMid <= x0)) {
                    const double dx = x1 - x0;
                    slope = (std::fabs(dx) > 1e-15) ? ((yv[i + 1] - yv[i]) / dx) : 0.0;
                    midY = yv[i] + slope * (xMid - x0);
                    foundCross = true;
                    break;
                }
            }
            if (!foundCross) {
                size_t bestIdx = 0;
                double bestDist = std::fabs(xv[0] - xMid);
                for (size_t i = 1; i < n; ++i) {
                    const double d = std::fabs(xv[i] - xMid);
                    if (d < bestDist) {
                        bestDist = d;
                        bestIdx = i;
                    }
                }
                midY = yv[bestIdx];
            }
            for (size_t i = 0; i < n; ++i)
                d_overlay_curve_pts.append(QPointF(xv[i], yv[i]));
        } else {
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

            xMid = std::clamp(rawMid, ux.front(), ux.back());
            midY = uy.front();
            if (ux.size() >= 4) {
                gsl_interp_accel *acc = gsl_interp_accel_alloc();
                gsl_spline *spline = gsl_spline_alloc(gsl_interp_cspline, ux.size());
                gsl_spline_init(spline, ux.data(), uy.data(), ux.size());
                midY = gsl_spline_eval(spline, xMid, acc);
                slope = gsl_spline_eval_deriv(spline, xMid, acc);

                const int nGrid = 100;
                for (int k = 0; k < nGrid; ++k) {
                    const double gx =
                            ux.front() + (ux.back() - ux.front()) * double(k) / double(nGrid - 1);
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

        d_summary_text = QString("%1 [%2]\nX = %3 -> Y = %4\ndY/dX = %5")
                                 .arg(tr("Interpolate"))
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

            d_summary_text = QString("%1 [%2]\n%3 = %4 Hz\n%5 = %6")
                                     .arg(tr("FFT Preview"))
                                     .arg(curve->title().text())
                                     .arg(tr("Dom Freq"))
                                     .arg(domFreq, 0, 'g', 6)
                                     .arg(tr("Peak Mag"))
                                     .arg(peakMag, 0, 'g', 6);
        } else {
            d_summary_text = QString("%1 [%2]\n%3")
                                     .arg(tr("FFT Preview"))
                                     .arg(curve->title().text())
                                     .arg(tr("Need >= 4 points in ROI"));
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

        d_summary_text = QString("%1 [%2]\n%3 = %4\n%5 = %6")
                                 .arg(tr("Rise Time"))
                                 .arg(curve->title().text())
                                 .arg(tr("10-90% Rise"))
                                 .arg(riseTime, 0, 'g', 6)
                                 .arg(tr("90-10% Fall"))
                                 .arg(fallTime, 0, 'g', 6);
        break;
    }

    case Differentiate: {
        // Build sorted distinct-X curve reference from all finite unmasked curve points
        // so Savitzky-Golay derivative is accurate even at ROI boundary points
        std::vector<std::pair<double, double>> fullPts;
        fullPts.reserve(size_t(nTotal));
        for (int k = 0; k < nTotal; ++k) {
            if (dc && dc->isPointMasked(k))
                continue;
            const double fx = curve->x(k);
            const double fy = curve->y(k);
            if (std::isfinite(fx) && std::isfinite(fy))
                fullPts.emplace_back(fx, fy);
        }
        const bool fullHasReversals =
                sequenceHasXReversals(fullPts, [](const auto &p) { return p.first; });
        if (!fullHasReversals) {
            std::stable_sort(fullPts.begin(), fullPts.end(),
                             [](const auto &a, const auto &b) { return a.first < b.first; });
        }

        std::vector<double> ux, uy;
        ux.reserve(fullPts.size());
        uy.reserve(fullPts.size());
        std::vector<int> dupCount;
        for (const auto &fp : fullPts) {
            if (!fullHasReversals && !ux.empty() && std::fabs(fp.first - ux.back()) <= 1e-14) {
                const int cNew = dupCount.back() + 1;
                uy.back() = (uy.back() * double(dupCount.back()) + fp.second) / double(cNew);
                dupCount.back() = cNew;
            } else {
                ux.push_back(fp.first);
                uy.push_back(fp.second);
                dupCount.push_back(1);
            }
        }

        d_diff_rows.reserve(static_cast<int>(n));
        double maxAbsSlope = -1.0;
        double signedSlopeAtMax = 0.0;
        double xAtMaxSlope = xv.front();
        double yAtMaxSlope = yv.front();
        double maxAbsD2 = 0.0;
        double xAtMaxD2 = xv.front();
        double minD1 = std::numeric_limits<double>::infinity();
        double maxD1 = -std::numeric_limits<double>::infinity();

        for (size_t i = 0; i < n; ++i) {
            double d1 = 0.0, d2 = 0.0;
            if (hasXReversals) {
                if (n >= 3 && i > 0 && i + 1 < n) {
                    const double dx = xv[i + 1] - xv[i - 1];
                    d1 = (std::fabs(dx) > 1e-15) ? ((yv[i + 1] - yv[i - 1]) / dx) : 0.0;
                    const double dx1 = xv[i] - xv[i - 1];
                    const double dx2 = xv[i + 1] - xv[i];
                    if (std::fabs(dx1) > 1e-15 && std::fabs(dx2) > 1e-15 && std::fabs(dx) > 1e-15) {
                        const double s1 = (yv[i] - yv[i - 1]) / dx1;
                        const double s2 = (yv[i + 1] - yv[i]) / dx2;
                        d2 = 2.0 * (s2 - s1) / dx;
                    }
                } else if (n >= 2) {
                    const size_t i0 = (i == 0) ? 0 : (n - 2);
                    const double dx = xv[i0 + 1] - xv[i0];
                    d1 = (std::fabs(dx) > 1e-15) ? ((yv[i0 + 1] - yv[i0]) / dx) : 0.0;
                }
            } else {
                evalSavitzkyGolayAtPoint(ux, uy, xv[i], d_sg_window, d1, d2);
            }
            d_diff_rows.append({ xv[i], yv[i], d1, d2 });

            const double absD1 = std::fabs(d1);
            if (absD1 > maxAbsSlope) {
                maxAbsSlope = absD1;
                signedSlopeAtMax = d1;
                xAtMaxSlope = xv[i];
                yAtMaxSlope = yv[i];
            }
            if (std::fabs(d2) > maxAbsD2) {
                maxAbsD2 = std::fabs(d2);
                xAtMaxD2 = xv[i];
            }
            minD1 = std::min(minD1, d1);
            maxD1 = std::max(maxD1, d1);
        }
        if (maxAbsSlope < 0.0)
            maxAbsSlope = 0.0;

        d_diff_preview_pts.reserve(static_cast<int>(n));
        Plot *plotPtr = d_graph_guard ? d_graph_guard->plotWidget() : nullptr;
        const bool logY = isAxisLog10(plotPtr, curve->yAxis()) && roi.top() > 0.0
                && roi.bottom() > roi.top();
        const double yLoPreview = roi.top() + 0.12 * roi.height();
        const double ySpanPreview = 0.76 * roi.height();
        for (const auto &dr : d_diff_rows) {
            double pyScaled = logY ? std::sqrt(roi.top() * roi.bottom()) : roi.center().y();
            if (maxD1 > minD1 + 1e-30 && roi.height() > 0.0) {
                const double frac = (dr.dydx - minD1) / (maxD1 - minD1);
                if (logY) {
                    const double ly0 = std::log10(roi.top());
                    const double ly1 = std::log10(roi.bottom());
                    pyScaled = std::pow(10.0, ly0 + (0.12 + 0.76 * frac) * (ly1 - ly0));
                } else {
                    pyScaled = yLoPreview + frac * ySpanPreview;
                }
            }
            d_diff_preview_pts.append(QPointF(dr.x, pyScaled));
        }

        d_primary_metric = maxAbsSlope;
        d_secondary_metric = xAtMaxSlope;
        d_diff_max_x = xAtMaxSlope;
        d_diff_max_y = yAtMaxSlope;
        d_diff_signed_slope = signedSlopeAtMax;
        d_has_diff_marker = (n >= 2);

        d_output_metrics << qMakePair(QString("Max_Abs_dY_dX"), maxAbsSlope)
                         << qMakePair(QString("Signed_Slope_at_Max"), signedSlopeAtMax)
                         << qMakePair(QString("X_at_Max_Slope"), xAtMaxSlope)
                         << qMakePair(QString("Max_Abs_d2Y_dX2"), maxAbsD2)
                         << qMakePair(QString("X_at_Max_d2Y_dX2"), xAtMaxD2)
                         << qMakePair(QString("SG_Window"), double(d_sg_window));

        d_summary_text =
                tr("Differentiate (SG-%1) [%2]\nMax |dY/dX| = %3 at X = %4\nMax |d²Y/dX²| = %5")
                        .arg(d_sg_window)
                        .arg(curve->title().text())
                        .arg(maxAbsSlope, 0, 'g', 6)
                        .arg(xAtMaxSlope, 0, 'g', 6)
                        .arg(maxAbsD2, 0, 'g', 5);
        break;
    }

    case Integrate2D: {
        // Fallback when no Spectrogram/Matrix is present on Graph: 2D scatter/curve trapezoidal
        double area = 0.0;
        double baseArea = 0.0;
        if (n >= 2) {
            for (size_t i = 0; i + 1 < n; ++i)
                area += 0.5 * (yv[i] + yv[i + 1]) * (xv[i + 1] - xv[i]);
            baseArea = 0.5 * (yv.front() + yv.back()) * (xv.back() - xv.front());
        }
        const double netArea = area - baseArea;
        double wSum = 0.0, wxSum = 0.0, wySum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double w = std::max(1e-15, std::fabs(yv[i]));
            wSum += w;
            wxSum += w * xv[i];
            wySum += w * yv[i];
        }
        const double centX = (wSum > 0.0) ? (wxSum / wSum) : roi.center().x();
        const double centY = (wSum > 0.0) ? (wySum / wSum) : roi.center().y();

        d_primary_metric = area;
        d_secondary_metric = netArea;
        d_centroid_x = centX;
        d_centroid_y = centY;
        d_peak2d_x = xAtMax;
        d_peak2d_y = maxY;
        d_has_2d_markers = true;

        d_output_metrics << qMakePair(QString("Total_Volume"), area)
                         << qMakePair(QString("Net_Volume"), netArea)
                         << qMakePair(QString("Peak_Z_Max"), maxY)
                         << qMakePair(QString("Centroid_X"), centX)
                         << qMakePair(QString("Centroid_Y"), centY);

        d_summary_text = tr("2D Integrate [%1]\nIntegral = %2, Net = %3\nPeak = %4, Centroid = (%5, %6)")
                                 .arg(curve->title().text())
                                 .arg(area, 0, 'g', 6)
                                 .arg(netArea, 0, 'g', 6)
                                 .arg(maxY, 0, 'g', 5)
                                 .arg(centX, 0, 'g', 5)
                                 .arg(centY, 0, 'g', 5);
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

    if (d_mode == Differentiate && !d_diff_rows.isEmpty()) {
        const int rows = d_diff_rows.size();
        Table *t = d_app->newTable(rows, 4, baseName, tr("ROI Differentiate Output"));
        if (!t)
            return nullptr;
        t->setColName(0, "X");
        t->setColName(1, "Y");
        t->setColName(2, "dY/dX");
        t->setColName(3, "d2Y/dX2");
        t->setColComment(0, tr("X"));
        t->setColComment(1, tr("Y"));
        t->setColComment(2, tr("1st Derivative (dY/dX)"));
        t->setColComment(3, tr("2nd Derivative (d2Y/dX2)"));
        t->setColPlotDesignation(0, SciDAVis::X);
        t->setColPlotDesignation(1, SciDAVis::Y);
        t->setColPlotDesignation(2, SciDAVis::Y);
        t->setColPlotDesignation(3, SciDAVis::Y);
        for (int r = 0; r < rows; ++r) {
            const auto &dr = d_diff_rows[r];
            t->setCell(r, 0, dr.x);
            t->setCell(r, 1, dr.y);
            t->setCell(r, 2, dr.dydx);
            t->setCell(r, 3, dr.d2ydx2);
        }
        return t;
    }

    if (d_mode == Intersect) {
        const int rows = std::max(1, d_intersections.size());
        Table *t = d_app->newTable(rows, 4, baseName, tr("ROI Curve Intersections"));
        if (!t)
            return nullptr;
        t->setColName(0, "CurveA");
        t->setColumnType(0, SciDAVis::ColumnMode::Text);
        t->setColName(1, "CurveB");
        t->setColumnType(1, SciDAVis::ColumnMode::Text);
        t->setColName(2, "X");
        t->setColName(3, "Y");
        t->setColComment(0, tr("Curve A"));
        t->setColComment(1, tr("Curve B"));
        t->setColComment(2, tr("X"));
        t->setColComment(3, tr("Y"));
        t->setColPlotDesignation(2, SciDAVis::X);
        t->setColPlotDesignation(3, SciDAVis::Y);
        for (int r = 0; r < d_intersections.size(); ++r) {
            const auto &ir = d_intersections[r];
            t->setText(r, 0, ir.curveA);
            t->setText(r, 1, ir.curveB);
            t->setCell(r, 2, ir.x);
            t->setCell(r, 3, ir.y);
        }
        return t;
    }

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
        t->setColComment(0, tr("Peak Index"));
        t->setColComment(1, tr("Center X"));
        t->setColComment(2, tr("Height"));
        t->setColComment(3, tr("FWHM"));
        t->setColComment(4, tr("Area"));
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
        t->setColComment(0, tr("Frequency"));
        t->setColComment(1, tr("Magnitude"));
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
    t->setColComment(0, tr("Metric"));
    t->setColComment(1, tr("Value"));
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
        && (g->activeTool()->rtti() == RoiGadget || g->activeTool()->rtti() == VerticalCursor
            || g->activeTool()->rtti() == DeltaCursor
            || g->activeTool()->rtti() == ImageProfileGadget)) {
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
        { RegionStats, QT_TR_NOOP("Region Statistics") },
        { QuickFit, QT_TR_NOOP("Quick Fit") },
        { QuickPeaks, QT_TR_NOOP("Quick Peaks") },
        { QuickSigmoidal, QT_TR_NOOP("Quick Sigmoidal Fit") },
        { Integrate, QT_TR_NOOP("Integrate") },
        { Interpolate, QT_TR_NOOP("Interpolate") },
        { Differentiate, QT_TR_NOOP("Differentiate (dY/dX)") },
        { Intersect, QT_TR_NOOP("Curve Intersection") },
        { Integrate2D, QT_TR_NOOP("2D Integration (Surface/Matrix)") },
        { FFT, QT_TR_NOOP("FFT Preview") },
        { RiseTime, QT_TR_NOOP("Rise Time (10-90%)") },
        { Cluster, QT_TR_NOOP("Cluster Selector") },
        { MaskRegion, QT_TR_NOOP("Mask Region") },
        { UnmaskRegion, QT_TR_NOOP("Unmask Region") }
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
        { FitLinear, QT_TR_NOOP("Linear") },
        { FitPoly2, QT_TR_NOOP("Polynomial (Order 2)") },
        { FitGauss, QT_TR_NOOP("Gaussian") },
        { FitLorentz, QT_TR_NOOP("Lorentzian") },
        { FitExpDec1, QT_TR_NOOP("Exponential Decay (ExpDec1)") },
        { FitBoltzmann, QT_TR_NOOP("Boltzmann Sigmoidal") }
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

    QMenu *baseMenu = menu.addMenu(tr("Integrate Baseline"));
    struct BaseItem {
        IntegrateBaseline base;
        const char *label;
    };
    const BaseItem baseItems[] = {
        { BaseYZero, QT_TR_NOOP("Y = 0") },
        { BaseEndpointsLine, QT_TR_NOOP("Straight Line (Endpoints)") },
        { BaseRoiMin, QT_TR_NOOP("ROI Minimum") }
    };
    for (const auto &bi : baseItems) {
        QAction *act = baseMenu->addAction(tr(bi.label));
        act->setCheckable(true);
        act->setChecked(d_integrate_baseline == bi.base);
        connect(act, &QAction::triggered, this, [this, b = bi.base]() {
            d_mode = Integrate;
            setIntegrateBaseline(b);
        });
    }

    QAction *actNegPeaks = menu.addAction(tr("Detect Negative Peaks (Troughs)"));
    actNegPeaks->setCheckable(true);
    actNegPeaks->setChecked(d_detect_negative_peaks);
    connect(actNegPeaks, &QAction::triggered, this, [this](bool on) {
        d_mode = QuickPeaks;
        setDetectNegativePeaks(on);
    });

    QMenu *sgMenu = menu.addMenu(tr("Savitzky-Golay Window"));
    const int sgWindows[] = { 5, 9, 15, 25 };
    for (int w : sgWindows) {
        QAction *act = sgMenu->addAction(tr("%1 Points").arg(w));
        act->setCheckable(true);
        act->setChecked(d_sg_window == w);
        connect(act, &QAction::triggered, this, [this, w]() {
            d_mode = Differentiate;
            setSavGolWindow(w);
        });
    }

    menu.addSeparator();
    if ((d_mode == QuickFit || d_mode == QuickSigmoidal) && d_overlay_curve_pts.size() >= 2) {
        QAction *actKeepFit = menu.addAction(tr("Keep Fitted Curve on Graph"));
        connect(actKeepFit, &QAction::triggered, this, [this]() { keepFittedCurveOnGraph(); });
    }
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
            const double dir = (ke->key() == Qt::Key_Right) ? 1.0 : -1.0;
            const QRectF roi = d_roi_rect.normalized();
            if (isAxisLog10(plot, xAxis) && roi.left() > 0.0 && roi.right() > roi.left()) {
                const double ratio = std::pow(roi.right() / roi.left(), dir * 0.02);
                setRoiRect(QRectF(QPointF(roi.left() * ratio, roi.top()),
                                  QPointF(roi.right() * ratio, roi.bottom())));
            } else {
                const double step = dir * 0.02 * std::max(1e-9, roi.width());
                setRoiRect(roi.translated(step, 0.0));
            }
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
        const Graph *g = d_layer_graph ? d_layer_graph.data() : nullptr;
        if (!g && plot())
            g = qobject_cast<const Graph *>(plot()->parent());
        if (isExportingOrPrinting(g, painter))
            return;
        if (!painter || !d_owner || !d_layer_graph)
            return;

        const double cx = d_owner->cursorX();
        if (!std::isfinite(cx))
            return;

        int primXAxis = int(QwtPlot::xBottom);
        int primYAxis = int(QwtPlot::yLeft);
        primaryAxesForGraph(d_layer_graph, primXAxis, primYAxis);
        const QwtScaleMap &primXMap = d_layer_graph->plotWidget()
                ? d_layer_graph->plotWidget()->canvasMap(primXAxis)
                : xMap;

        const int px = primXMap.transform(cx);
        const bool exporting = isExportingOrPrinting(d_layer_graph, painter);
        painter->save();
        painter->setClipRect(canvasRect);
        painter->setRenderHint(QPainter::Antialiasing, true);

        // Crisp OriginPro vertical red cursor line
        painter->setPen(QPen(QColor(215, 25, 28), 1.6, Qt::SolidLine));
        painter->drawLine(px, canvasRect.top(), px, canvasRect.bottom());

        // Top small triangular cursor handle on canvas border (hidden during export/print)
        if (!exporting) {
            QPolygon topTri;
            topTri << QPoint(px, canvasRect.top() + 7) << QPoint(px - 5, canvasRect.top())
                   << QPoint(px + 5, canvasRect.top());
            painter->setBrush(QColor(215, 25, 28));
            painter->drawPolygon(topTri);
        }

        // Draw interpolated target rings on each visible curve in this layer
        const QVector<QPair<QString, double>> layerVals =
                d_owner->interpolatedValuesForGraph(d_layer_graph);
        QStringList tagLines;
        tagLines << QString("X = %1").arg(cx, 0, 'g', 6);

        for (const auto &kv : layerVals) {
            QwtPlotCurve *c = d_layer_graph->curve(kv.first);
            const QwtScaleMap &cxMap =
                    (c && d_layer_graph->plotWidget()) ? d_layer_graph->plotWidget()->canvasMap(c->xAxis())
                                                       : primXMap;
            const QwtScaleMap &cyMap =
                    (c && d_layer_graph->plotWidget()) ? d_layer_graph->plotWidget()->canvasMap(c->yAxis())
                                                       : yMap;
            const int cpx = cxMap.transform(cx);
            const int py = cyMap.transform(kv.second);
            painter->setBrush(QColor(255, 255, 255, 220));
            painter->setPen(QPen(QColor(215, 25, 28), 2.0));
            painter->drawEllipse(QPoint(cpx, py), 4, 4);
            painter->setBrush(QColor(215, 25, 28));
            painter->drawEllipse(QPoint(cpx, py), 1, 1);
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
        int xAxis = int(QwtPlot::xBottom);
        int yAxis = int(QwtPlot::yLeft);
        primaryAxesForGraph(d_graph_guard, xAxis, yAxis);
        double xMin = 0.0, xMax = 0.0;
        if (const QwtScaleDiv *xDiv = plot->axisScaleDiv(xAxis)) {
            xMin = std::min(xDiv->lowerBound(), xDiv->upperBound());
            xMax = std::max(xDiv->lowerBound(), xDiv->upperBound());
        }
        if (!(xMax > xMin) && d_graph_guard->curves() > 0) {
            if (QwtPlotCurve *c = d_graph_guard->curve(0)) {
                xMin = c->minXValue();
                xMax = c->maxXValue();
            }
        }
        if (isAxisLog10(plot, xAxis) && xMin > 0.0 && xMax > xMin)
            d_cursor_x = std::sqrt(xMin * xMax);
        else
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

    if (sequenceHasXReversals(pts, [](const auto &p) { return p.first; })) {
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            const double x0 = pts[i].first;
            const double x1 = pts[i + 1].first;
            if ((x0 <= xQuery && xQuery <= x1) || (x1 <= xQuery && xQuery <= x0)) {
                const double dx = x1 - x0;
                yOut = (std::fabs(dx) > 1e-15)
                        ? (pts[i].second + ((xQuery - x0) / dx) * (pts[i + 1].second - pts[i].second))
                        : pts[i].second;
                return true;
            }
        }
        size_t bestIdx = 0;
        double bestDist = std::fabs(pts[0].first - xQuery);
        for (size_t i = 1; i < pts.size(); ++i) {
            const double d = std::fabs(pts[i].first - xQuery);
            if (d < bestDist) {
                bestDist = d;
                bestIdx = i;
            }
        }
        yOut = pts[bestIdx].second;
        return true;
    }

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
    t->setColComment(0, tr("Curve"));
    t->setColComment(1, tr("X"));
    t->setColComment(2, tr("Interpolated Y"));
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
    int xAxis = int(QwtPlot::xBottom);
    int yAxis = int(QwtPlot::yLeft);
    primaryAxesForGraph(eventGraph, xAxis, yAxis);

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
            setCursorX(plot->invTransform(xAxis, me->pos().x()));
            me->accept();
            return true;
        }
        break;
    }

    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (d_dragging && (me->buttons() & Qt::LeftButton)) {
            setCursorX(plot->invTransform(xAxis, me->pos().x()));
            me->accept();
            return true;
        } else {
            const int cPx = plot->transform(xAxis, d_cursor_x);
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
            if (const QwtScaleDiv *xDiv = plot->axisScaleDiv(xAxis)) {
                xMin = std::min(xDiv->lowerBound(), xDiv->upperBound());
                xMax = std::max(xDiv->lowerBound(), xDiv->upperBound());
            }
            const double frac = (ke->modifiers() & Qt::ShiftModifier) ? 0.05 : 0.01;
            const double dir = (ke->key() == Qt::Key_Right) ? 1.0 : -1.0;
            if (isAxisLog10(plot, xAxis) && xMin > 0.0 && xMax > xMin && d_cursor_x > 0.0) {
                const double ratio = std::pow(xMax / xMin, dir * frac);
                setCursorX(d_cursor_x * ratio);
            } else {
                const double span = std::max(1e-9, xMax - xMin);
                setCursorX(d_cursor_x + dir * frac * span);
            }
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

// ============================================================================
// DeltaCursorOverlayItem — Dual Crosshair / Delta Cursor Overlay
// ============================================================================

class DeltaCursorOverlayItem : public QwtPlotItem
{
public:
    explicit DeltaCursorOverlayItem(DeltaCursorTool *owner)
        : QwtPlotItem(QwtText("OriginDeltaCursorOverlay")), d_owner(owner)
    {
        setZ(962.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    ~DeltaCursorOverlayItem() override
    {
        if (d_owner && d_owner->d_overlay_item == this)
            d_owner->d_overlay_item = nullptr;
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 43; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        const Graph *g = d_owner ? d_owner->d_graph_guard.data() : nullptr;
        if (!g && plot())
            g = qobject_cast<const Graph *>(plot()->parent());
        if (isExportingOrPrinting(g, painter))
            return;
        if (!painter || !d_owner)
            return;

        const auto &ro = d_owner->d_readout;
        if (!std::isfinite(ro.x1) || !std::isfinite(ro.x2))
            return;

        const int px1 = xMap.transform(ro.x1);
        const int px2 = xMap.transform(ro.x2);
        const int py1 = std::isfinite(ro.y1) ? yMap.transform(ro.y1) : canvasRect.center().y();
        const int py2 = std::isfinite(ro.y2) ? yMap.transform(ro.y2) : canvasRect.center().y();
        const bool exporting = isExportingOrPrinting(d_owner->d_graph_guard, painter);

        painter->save();
        painter->setClipRect(canvasRect);
        painter->setRenderHint(QPainter::Antialiasing, true);

        // 1. Shaded span between C1 and C2
        const int spanL = std::min(px1, px2);
        const int spanR = std::max(px1, px2);
        if (spanR > spanL) {
            const QRect spanRect(spanL, canvasRect.top(), spanR - spanL, canvasRect.height());
            painter->fillRect(spanRect, QColor(33, 150, 243, exporting ? 22 : 34));
        }

        if (d_owner->d_span_poly_pts.size() >= 3) {
            QPolygonF polyPx;
            polyPx.reserve(d_owner->d_span_poly_pts.size());
            for (const QPointF &pt : d_owner->d_span_poly_pts)
                polyPx.append(QPointF(xMap.transform(pt.x()), yMap.transform(pt.y())));
            painter->setBrush(QColor(25, 118, 210, 48));
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(polyPx);
        }

        const QColor colC1(25, 105, 210); // Solid blue
        const QColor colC2(210, 30, 45);  // Solid crimson

        // 2. Horizontal crosshairs when enabled (or in FreeScreenCrosshair mode)
        if (d_owner->d_horiz_crosshair
            || d_owner->d_snap_mode == DeltaCursorTool::FreeScreenCrosshair) {
            painter->setPen(QPen(QColor(25, 105, 210, 170), 1.2, Qt::DashLine));
            painter->drawLine(canvasRect.left(), py1, canvasRect.right(), py1);
            painter->setPen(QPen(QColor(210, 30, 45, 170), 1.2, Qt::DashLine));
            painter->drawLine(canvasRect.left(), py2, canvasRect.right(), py2);
        }

        // 3. Vertical cursor lines C1 (solid blue) and C2 (solid crimson)
        const double wC1 = (d_owner->d_active_cursor == 1) ? 2.3 : 1.6;
        const double wC2 = (d_owner->d_active_cursor == 2) ? 2.3 : 1.6;
        painter->setPen(QPen(colC1, wC1, Qt::SolidLine));
        painter->drawLine(px1, canvasRect.top(), px1, canvasRect.bottom());

        painter->setPen(QPen(colC2, wC2, Qt::SolidLine));
        painter->drawLine(px2, canvasRect.top(), px2, canvasRect.bottom());

        // Interactive top handle triangles (hidden during export/print)
        if (!exporting) {
            QPolygon tri1;
            tri1 << QPoint(px1, canvasRect.top() + 8) << QPoint(px1 - 5, canvasRect.top())
                 << QPoint(px1 + 5, canvasRect.top());
            painter->setBrush(colC1);
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(tri1);

            QPolygon tri2;
            tri2 << QPoint(px2, canvasRect.top() + 8) << QPoint(px2 - 5, canvasRect.top())
                 << QPoint(px2 + 5, canvasRect.top());
            painter->setBrush(colC2);
            painter->drawPolygon(tri2);
        }

        // 4. Target rings + C1 / C2 badges in Arial
        painter->setBrush(QColor(255, 255, 255, 230));
        painter->setPen(QPen(colC1, 2.0));
        painter->drawEllipse(QPoint(px1, py1), 4, 4);
        painter->setPen(QPen(colC2, 2.0));
        painter->setBrush(QColor(255, 255, 255, 230));
        painter->drawEllipse(QPoint(px2, py2), 4, 4);

        // Chord connecting (x1, y1) -> (x2, y2)
        painter->setPen(QPen(QColor(70, 70, 70, 175), 1.2, Qt::DotLine));
        painter->drawLine(px1, py1, px2, py2);

        QFont tagFnt("Arial", 8, QFont::Bold);
        painter->setFont(tagFnt);
        const QFontMetrics tfm(tagFnt);
        auto drawBadge = [&](int xPx, const QString &lbl, const QColor &col, bool isActive) {
            const int bw = tfm.horizontalAdvance(lbl) + 8;
            const int bh = tfm.height() + 2;
            const int bx = std::clamp(xPx + 4, canvasRect.left() + 2, canvasRect.right() - bw - 2);
            const int by = canvasRect.top() + 10;
            const QRect r(bx, by, bw, bh);
            painter->setBrush(col);
            painter->setPen((!exporting && isActive) ? QPen(QColor(255, 215, 0), 1.8)
                                                     : QPen(Qt::NoPen));
            painter->drawRoundedRect(r, 3.0, 3.0);
            painter->setPen(Qt::white);
            painter->drawText(r.left() + 4, r.top() + 1 + tfm.ascent(), lbl);
        };
        drawBadge(px1, QStringLiteral("C1"), colC1, d_owner->d_active_cursor == 1);
        drawBadge(px2, QStringLiteral("C2"), colC2, d_owner->d_active_cursor == 2);

        // 5. Arial readout box
        const QPoint anchor(std::max(px1, px2), canvasRect.top() + 28);
        drawReadoutBox(painter, canvasRect, anchor, d_owner->d_readout_text, QColor(25, 105, 210),
                       QColor(255, 255, 255, 238), true);

        painter->restore();
    }

    DeltaCursorTool *d_owner;
};

// ============================================================================
// DeltaCursorTool Implementation
// ============================================================================

DeltaCursorTool::DeltaCursorTool(ApplicationWindow *app, Graph *graph, SnapMode mode)
    : QObject(nullptr),
      PlotToolInterface(graph),
      d_app(app ? app : resolveAppWindowFromGraph(graph)),
      d_graph_guard(graph),
      d_snap_mode(mode),
      d_horiz_crosshair(true)
{
    if (d_graph_guard && d_graph_guard->plotWidget()) {
        d_overlay_item = new DeltaCursorOverlayItem(this);
        d_overlay_item->attach(d_graph_guard->plotWidget());
        if (QwtPlotCanvas *canvas = d_graph_guard->plotWidget()->canvas()) {
            canvas->setMouseTracking(true);
            canvas->installEventFilter(this);
            canvas->setFocus();
        }
    }
    initDefaultPositions();
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

DeltaCursorTool::~DeltaCursorTool()
{
    if (d_graph_guard && d_graph_guard->plotWidget()) {
        if (QwtPlotCanvas *canvas = d_graph_guard->plotWidget()->canvas()) {
            canvas->removeEventFilter(this);
            canvas->unsetCursor();
        }
    }
    if (d_overlay_item) {
        d_overlay_item->d_owner = nullptr;
        d_overlay_item->detach();
        delete d_overlay_item;
        d_overlay_item = nullptr;
    }
}

QwtPlotCurve *DeltaCursorTool::resolveTargetCurve()
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
            connect(dc->table(), &Table::modifiedData, this,
                    &DeltaCursorTool::onSourceTableModified, Qt::UniqueConnection);
        }
    }
    if (chosen && d_overlay_item)
        d_overlay_item->setAxis(chosen->xAxis(), chosen->yAxis());

    return chosen;
}

void DeltaCursorTool::onSourceTableModified(Table *, const QString &)
{
    d_snap_idx1 = -1;
    d_snap_idx2 = -1;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void DeltaCursorTool::initDefaultPositions()
{
    d_snap_idx1 = -1;
    d_snap_idx2 = -1;
    if (!d_graph_guard || !d_graph_guard->plotWidget()) {
        d_x1 = 0.25;
        d_x2 = 0.75;
        d_y1 = 0.0;
        d_y2 = 0.0;
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

    if (isAxisLog10(plot, xAxis) && xMax > 0.0) {
        const double sxMin = (xMin > 0.0) ? xMin : std::max(1e-100, xMax * 1e-4);
        const double lx0 = std::log10(sxMin);
        const double lx1 = std::log10(xMax);
        d_x1 = std::pow(10.0, lx0 + 0.30 * (lx1 - lx0));
        d_x2 = std::pow(10.0, lx0 + 0.70 * (lx1 - lx0));
    } else {
        d_x1 = xMin + 0.30 * (xMax - xMin);
        d_x2 = xMin + 0.70 * (xMax - xMin);
    }

    if (isAxisLog10(plot, yAxis) && yMax > 0.0) {
        const double syMin = (yMin > 0.0) ? yMin : std::max(1e-100, yMax * 1e-4);
        const double ly0 = std::log10(syMin);
        const double ly1 = std::log10(yMax);
        d_y1 = std::pow(10.0, ly0 + 0.35 * (ly1 - ly0));
        d_y2 = std::pow(10.0, ly0 + 0.65 * (ly1 - ly0));
    } else {
        d_y1 = yMin + 0.35 * (yMax - yMin);
        d_y2 = yMin + 0.65 * (yMax - yMin);
    }
}

void DeltaCursorTool::setCursorPositions(double x1, double x2)
{
    if (!std::isfinite(x1) || !std::isfinite(x2))
        return;
    d_x1 = x1;
    d_x2 = x2;
    d_snap_idx1 = -1;
    d_snap_idx2 = -1;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void DeltaCursorTool::setCursorPoints2D(double x1, double y1, double x2, double y2)
{
    if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2))
        return;
    d_x1 = x1;
    d_y1 = y1;
    d_x2 = x2;
    d_y2 = y2;
    d_snap_idx1 = -1;
    d_snap_idx2 = -1;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void DeltaCursorTool::setActiveCursor(int idx)
{
    d_active_cursor = (idx == 2) ? 2 : 1;
    if (d_graph_guard)
        d_graph_guard->replot();
}

void DeltaCursorTool::setHorizontalCrosshairEnabled(bool on)
{
    d_horiz_crosshair = on;
    if (d_graph_guard)
        d_graph_guard->replot();
}

void DeltaCursorTool::setSnapMode(SnapMode mode)
{
    d_snap_mode = mode;
    d_snap_idx1 = -1;
    d_snap_idx2 = -1;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void DeltaCursorTool::setTargetCurve(const QString &curveTitle)
{
    d_curve_title = curveTitle;
    d_snap_idx1 = -1;
    d_snap_idx2 = -1;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void DeltaCursorTool::recalculate()
{
    d_span_poly_pts.clear();

    // FreeScreenCrosshair mode: strictly screen/axis coordinates with zero curve contamination
    if (d_snap_mode == FreeScreenCrosshair) {
        if (d_graph_guard && d_overlay_item) {
            int xAx = int(QwtPlot::xBottom), yAx = int(QwtPlot::yLeft);
            primaryAxesForGraph(d_graph_guard, xAx, yAx);
            d_overlay_item->setAxis(xAx, yAx);
        }
        d_readout.x1 = d_x1;
        d_readout.y1 = d_y1;
        d_readout.x2 = d_x2;
        d_readout.y2 = d_y2;
        d_readout.dx = d_x2 - d_x1;
        d_readout.dy = d_y2 - d_y1;
        d_readout.slope =
                (std::fabs(d_readout.dx) > 1e-30) ? (d_readout.dy / d_readout.dx) : 0.0;
        d_readout.ratio = (std::fabs(d_y1) > 1e-30) ? (d_y2 / d_y1) : 0.0;
        d_readout.area = 0.0;
        d_readout.rms = 0.0;
        d_readout.minY = std::min(d_y1, d_y2);
        d_readout.maxY = std::max(d_y1, d_y2);

        d_readout_text = tr("Delta Cursor [%1]\n"
                            "C1: (%2, %3) | C2: (%4, %5)\n"
                            "ΔX = %6, ΔY = %7, Slope = %8")
                                 .arg(tr("Screen"))
                                 .arg(d_readout.x1, 0, 'g', 6)
                                 .arg(d_readout.y1, 0, 'g', 6)
                                 .arg(d_readout.x2, 0, 'g', 6)
                                 .arg(d_readout.y2, 0, 'g', 6)
                                 .arg(d_readout.dx, 0, 'g', 6)
                                 .arg(d_readout.dy, 0, 'g', 6)
                                 .arg(d_readout.slope, 0, 'g', 6);
        if (d_app)
            d_app->setStatusBarText(d_readout_text.simplified());
        return;
    }

    QwtPlotCurve *c = resolveTargetCurve();

    std::vector<std::pair<double, double>> pts;
    bool hasXReversals = false;
    if (c && c->dataSize() > 0) {
        const DataCurve *dc = dynamic_cast<const DataCurve *>(c);
        const int n = c->dataSize();
        pts.reserve(size_t(n));
        for (int i = 0; i < n; ++i) {
            if (dc && dc->isPointMasked(i))
                continue;
            const double px = c->x(i);
            const double py = c->y(i);
            if (std::isfinite(px) && std::isfinite(py))
                pts.emplace_back(px, py);
        }
        hasXReversals = sequenceHasXReversals(pts, [](const auto &p) { return p.first; });
        if (!hasXReversals) {
            std::stable_sort(pts.begin(), pts.end(),
                             [](const auto &a, const auto &b) { return a.first < b.first; });
        }
    }

    auto interpAt = [&](double xq) -> double {
        if (pts.empty())
            return 0.0;
        if (pts.size() == 1)
            return pts.front().second;
        if (hasXReversals) {
            for (size_t i = 0; i + 1 < pts.size(); ++i) {
                const double x0 = pts[i].first;
                const double x1 = pts[i + 1].first;
                if ((x0 <= xq && xq <= x1) || (x1 <= xq && xq <= x0)) {
                    const double dx = x1 - x0;
                    return (std::fabs(dx) > 1e-15)
                            ? (pts[i].second + ((xq - x0) / dx) * (pts[i + 1].second - pts[i].second))
                            : pts[i].second;
                }
            }
            size_t bestIdx = 0;
            double bestDist = std::fabs(pts[0].first - xq);
            for (size_t i = 1; i < pts.size(); ++i) {
                const double d = std::fabs(pts[i].first - xq);
                if (d < bestDist) {
                    bestDist = d;
                    bestIdx = i;
                }
            }
            return pts[bestIdx].second;
        }
        if (xq <= pts.front().first)
            return pts.front().second;
        if (xq >= pts.back().first)
            return pts.back().second;
        auto it = std::lower_bound(pts.begin(), pts.end(), xq,
                                   [](const auto &pt, double val) { return pt.first < val; });
        if (it != pts.end() && std::fabs(it->first - xq) <= 1e-15)
            return it->second;
        const size_t idx = (it == pts.begin()) ? 0 : size_t(std::distance(pts.begin(), it) - 1);
        const double x0 = pts[idx].first;
        const double y0 = pts[idx].second;
        const double x1 = pts[idx + 1].first;
        const double y1 = pts[idx + 1].second;
        const double dx = x1 - x0;
        return (std::fabs(dx) > 1e-15) ? (y0 + ((xq - x0) / dx) * (y1 - y0)) : y0;
    };

    auto nearestIdx = [&](double xq) -> int {
        if (pts.empty())
            return -1;
        size_t bestIdx = 0;
        double bestDist = std::fabs(pts[0].first - xq);
        for (size_t i = 1; i < pts.size(); ++i) {
            const double d = std::fabs(pts[i].first - xq);
            if (d < bestDist) {
                bestDist = d;
                bestIdx = i;
            }
        }
        return static_cast<int>(bestIdx);
    };

    double rx1 = d_x1, ry1 = d_y1, rx2 = d_x2, ry2 = d_y2;
    if (!pts.empty()) {
        if (d_snap_mode == SnapInterpolateCurve) {
            ry1 = interpAt(rx1);
            ry2 = interpAt(rx2);
            d_y1 = ry1;
            d_y2 = ry2;
        } else if (d_snap_mode == SnapNearestPoint) {
            if (d_snap_idx1 < 0 || d_snap_idx1 >= static_cast<int>(pts.size())
                || std::fabs(pts[size_t(d_snap_idx1)].first - d_x1) > 1e-12) {
                d_snap_idx1 = nearestIdx(rx1);
            }
            if (d_snap_idx2 < 0 || d_snap_idx2 >= static_cast<int>(pts.size())
                || std::fabs(pts[size_t(d_snap_idx2)].first - d_x2) > 1e-12) {
                d_snap_idx2 = nearestIdx(rx2);
            }
            rx1 = pts[size_t(d_snap_idx1)].first;
            ry1 = pts[size_t(d_snap_idx1)].second;
            rx2 = pts[size_t(d_snap_idx2)].first;
            ry2 = pts[size_t(d_snap_idx2)].second;
            d_x1 = rx1;
            d_y1 = ry1;
            d_x2 = rx2;
            d_y2 = ry2;
        }
    }

    d_readout.x1 = rx1;
    d_readout.y1 = ry1;
    d_readout.x2 = rx2;
    d_readout.y2 = ry2;
    d_readout.dx = rx2 - rx1;
    d_readout.dy = ry2 - ry1;
    d_readout.slope = (std::fabs(d_readout.dx) > 1e-30) ? (d_readout.dy / d_readout.dx) : 0.0;
    d_readout.ratio = (std::fabs(ry1) > 1e-30) ? (ry2 / ry1) : 0.0;

    const double xLo = std::min(rx1, rx2);
    const double xHi = std::max(rx1, rx2);
    if (!pts.empty()) {
        std::vector<std::pair<double, double>> spanPts;
        if (hasXReversals) {
            const int idxA = (d_snap_mode == SnapNearestPoint && d_snap_idx1 >= 0)
                    ? d_snap_idx1
                    : nearestIdx(rx1);
            const int idxB = (d_snap_mode == SnapNearestPoint && d_snap_idx2 >= 0)
                    ? d_snap_idx2
                    : nearestIdx(rx2);
            const int iStart = std::max(0, std::min(idxA, idxB));
            const int iEnd = std::min(static_cast<int>(pts.size()) - 1, std::max(idxA, idxB));
            spanPts.reserve(size_t(iEnd - iStart + 1));
            for (int i = iStart; i <= iEnd; ++i)
                spanPts.push_back(pts[size_t(i)]);
        } else {
            spanPts.reserve(pts.size() + 2);
            const double yLo = interpAt(xLo);
            const double yHi = interpAt(xHi);
            spanPts.emplace_back(xLo, yLo);
            for (const auto &pt : pts) {
                if (pt.first > xLo + 1e-15 && pt.first < xHi - 1e-15)
                    spanPts.push_back(pt);
            }
            if (xHi > xLo + 1e-15)
                spanPts.emplace_back(xHi, yHi);
        }

        double area = 0.0;
        double y2Integral = 0.0;
        double absPathX = 0.0;
        double minY = spanPts.front().second;
        double maxY = spanPts.front().second;
        for (size_t i = 0; i < spanPts.size(); ++i) {
            minY = std::min(minY, spanPts[i].second);
            maxY = std::max(maxY, spanPts[i].second);
            if (i + 1 < spanPts.size()) {
                const double segDx = spanPts[i + 1].first - spanPts[i].first;
                const double ya = spanPts[i].second;
                const double yb = spanPts[i + 1].second;
                if (hasXReversals) {
                    const double adx = std::fabs(segDx);
                    area += 0.5 * (ya + yb) * segDx;
                    y2Integral += (adx / 3.0) * (ya * ya + ya * yb + yb * yb);
                    absPathX += adx;
                } else if (segDx > 0.0) {
                    area += 0.5 * (ya + yb) * segDx;
                    y2Integral += (segDx / 3.0) * (ya * ya + ya * yb + yb * yb);
                    absPathX += segDx;
                }
            }
        }
        d_readout.area = area;
        d_readout.rms = (absPathX > 1e-30)
                ? std::sqrt(std::max(0.0, y2Integral / absPathX))
                : std::fabs(0.5 * (ry1 + ry2));
        d_readout.minY = minY;
        d_readout.maxY = maxY;

        Plot *plotPtr = d_graph_guard ? d_graph_guard->plotWidget() : nullptr;
        const bool logY = c && isAxisLog10(plotPtr, c->yAxis());
        const double baseY = logY ? std::max(1e-100, minY > 0.0 ? minY * 0.5 : 1e-12) : 0.0;
        if (spanPts.size() >= 2) {
            d_span_poly_pts.reserve(static_cast<int>(spanPts.size()) + 2);
            d_span_poly_pts.append(QPointF(spanPts.front().first, baseY));
            for (const auto &sp : spanPts)
                d_span_poly_pts.append(
                        QPointF(sp.first, logY ? std::max(1e-100, sp.second) : sp.second));
            d_span_poly_pts.append(QPointF(spanPts.back().first, baseY));
        }
    } else {
        d_readout.area = 0.0;
        d_readout.rms = 0.0;
        d_readout.minY = std::min(ry1, ry2);
        d_readout.maxY = std::max(ry1, ry2);
    }

    const QString titleLabel = d_curve_title.isEmpty() ? tr("Screen") : d_curve_title;
    d_readout_text = tr("Delta Cursor [%1]\n"
                        "C1: (%2, %3) | C2: (%4, %5)\n"
                        "ΔX = %6, ΔY = %7, Slope = %8\n"
                        "Area = %9, RMS = %10")
                             .arg(titleLabel)
                             .arg(d_readout.x1, 0, 'g', 6)
                             .arg(d_readout.y1, 0, 'g', 6)
                             .arg(d_readout.x2, 0, 'g', 6)
                             .arg(d_readout.y2, 0, 'g', 6)
                             .arg(d_readout.dx, 0, 'g', 6)
                             .arg(d_readout.dy, 0, 'g', 6)
                             .arg(d_readout.slope, 0, 'g', 6)
                             .arg(d_readout.area, 0, 'g', 6)
                             .arg(d_readout.rms, 0, 'g', 6);

    if (d_app)
        d_app->setStatusBarText(d_readout_text.simplified());
}

void DeltaCursorTool::stampCalloutOnGraph()
{
    if (!d_graph_guard)
        return;

    recalculate();
    const QString callout = (d_snap_mode == FreeScreenCrosshair)
            ? tr("ΔX = %1, ΔY = %2\nSlope = %3\nC1(%4, %5) -> C2(%6, %7)")
                      .arg(d_readout.dx, 0, 'g', 6)
                      .arg(d_readout.dy, 0, 'g', 6)
                      .arg(d_readout.slope, 0, 'g', 6)
                      .arg(d_readout.x1, 0, 'g', 5)
                      .arg(d_readout.y1, 0, 'g', 5)
                      .arg(d_readout.x2, 0, 'g', 5)
                      .arg(d_readout.y2, 0, 'g', 5)
            : tr("ΔX = %1, ΔY = %2\nSlope = %3, Area = %4\nC1(%5, %6) -> C2(%7, %8)")
                      .arg(d_readout.dx, 0, 'g', 6)
                      .arg(d_readout.dy, 0, 'g', 6)
                      .arg(d_readout.slope, 0, 'g', 6)
                      .arg(d_readout.area, 0, 'g', 6)
                      .arg(d_readout.x1, 0, 'g', 5)
                      .arg(d_readout.y1, 0, 'g', 5)
                      .arg(d_readout.x2, 0, 'g', 5)
                      .arg(d_readout.y2, 0, 'g', 5);

    MultiLayer *ml = d_graph_guard->multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    if (Legend *mrk = d_graph_guard->newLegend(callout)) {
        mrk->setFont(QFont("Arial", 9));
        mrk->setFrameStyle(Legend::Line);
        mrk->setBackgroundColor(QColor(255, 255, 255, 238));
        mrk->setOriginCoord(0.5 * (d_readout.x1 + d_readout.x2), d_readout.maxY);
        d_graph_guard->replot();
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(tr("Stamp Callout on Graph"), beforeSnap);
        if (d_app)
            d_app->modifiedProject();
    }
}

Table *DeltaCursorTool::createReadoutTable()
{
    if (!d_app)
        return nullptr;
    recalculate();
    const QString name = d_app->generateUniqueName("DeltaCursor");
    Table *t = d_app->newTable(12, 2, name, tr("Delta Cursor Readout"));
    if (!t)
        return nullptr;
    t->setColName(0, QCoreApplication::translate("DeltaCursorTool", "Metric"));
    t->setColumnType(0, SciDAVis::ColumnMode::Text);
    t->setColName(1, QCoreApplication::translate("DeltaCursorTool", "Value"));

    const QPair<QString, double> rows[12] = {
        { "X1", d_readout.x1 },       { "Y1", d_readout.y1 },
        { "X2", d_readout.x2 },       { "Y2", d_readout.y2 },
        { "Delta_X", d_readout.dx },  { "Delta_Y", d_readout.dy },
        { "Slope", d_readout.slope }, { "Ratio_Y2_Y1", d_readout.ratio },
        { "Area", d_readout.area },   { "RMS", d_readout.rms },
        { "Min_Y", d_readout.minY },  { "Max_Y", d_readout.maxY }
    };
    for (int r = 0; r < 12; ++r) {
        t->setText(r, 0, rows[r].first);
        t->setCell(r, 1, rows[r].second);
    }
    return t;
}

DeltaCursorTool *DeltaCursorTool::attach(ApplicationWindow *app, Graph *g, SnapMode mode)
{
    if (!g)
        return nullptr;
    g->setActiveTool(nullptr);
    DeltaCursorTool *tool = new DeltaCursorTool(app, g, mode);
    g->setActiveTool(tool);
    g->replot();
    return tool;
}

DeltaCursorTool::DragTarget DeltaCursorTool::hitTestPixelPos(const QPoint &pos) const
{
    if (!d_graph_guard || !d_graph_guard->plotWidget())
        return DragNone;

    Plot *plot = d_graph_guard->plotWidget();
    const int xAxis = d_overlay_item ? d_overlay_item->xAxis() : int(QwtPlot::xBottom);
    const int px1 = plot->transform(xAxis, d_readout.x1);
    const int px2 = plot->transform(xAxis, d_readout.x2);

    const int d1 = std::abs(pos.x() - px1);
    const int d2 = std::abs(pos.x() - px2);
    const int tol = 7;
    if (d1 <= tol && d1 <= d2)
        return DragCursor1;
    if (d2 <= tol)
        return DragCursor2;
    if (pos.x() > std::min(px1, px2) + tol && pos.x() < std::max(px1, px2) - tol)
        return DragSpan;
    return DragNone;
}

void DeltaCursorTool::updateCursorForPos(const QPoint &pos)
{
    if (!d_graph_guard || !d_graph_guard->plotWidget() || !d_graph_guard->plotWidget()->canvas())
        return;
    QWidget *canvas = d_graph_guard->plotWidget()->canvas();
    switch (hitTestPixelPos(pos)) {
    case DragCursor1:
    case DragCursor2:
        canvas->setCursor(d_snap_mode == FreeScreenCrosshair ? Qt::CrossCursor : Qt::SplitHCursor);
        break;
    case DragSpan:
        canvas->setCursor(Qt::SizeAllCursor);
        break;
    default:
        canvas->setCursor(Qt::CrossCursor);
        break;
    }
}

void DeltaCursorTool::showContextMenu(const QPoint &globalPos)
{
    if (!d_graph_guard)
        return;

    QMenu menu;
    QAction *actSnapCurve = menu.addAction(tr("Snap to Curve"));
    actSnapCurve->setCheckable(true);
    actSnapCurve->setChecked(d_snap_mode == SnapInterpolateCurve);
    connect(actSnapCurve, &QAction::triggered, this,
            [this]() { setSnapMode(SnapInterpolateCurve); });

    QAction *actSnapNearest = menu.addAction(tr("Nearest Point"));
    actSnapNearest->setCheckable(true);
    actSnapNearest->setChecked(d_snap_mode == SnapNearestPoint);
    connect(actSnapNearest, &QAction::triggered, this, [this]() { setSnapMode(SnapNearestPoint); });

    QAction *actFree = menu.addAction(tr("Free Crosshair"));
    actFree->setCheckable(true);
    actFree->setChecked(d_snap_mode == FreeScreenCrosshair);
    connect(actFree, &QAction::triggered, this, [this]() { setSnapMode(FreeScreenCrosshair); });

    menu.addSeparator();
    QAction *actHoriz = menu.addAction(tr("Horizontal Crosshair"));
    actHoriz->setCheckable(true);
    actHoriz->setChecked(d_horiz_crosshair);
    connect(actHoriz, &QAction::triggered, this,
            [this](bool on) { setHorizontalCrosshairEnabled(on); });

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
    QAction *actStamp = menu.addAction(tr("Stamp ΔX/ΔY Callout on Graph"));
    connect(actStamp, &QAction::triggered, this, [this]() { stampCalloutOnGraph(); });

    QAction *actTable = menu.addAction(tr("Output Readout to Worksheet"));
    connect(actTable, &QAction::triggered, this, [this]() { createReadoutTable(); });

    QAction *actCopy = menu.addAction(tr("Copy Readout to Clipboard"));
    connect(actCopy, &QAction::triggered, this,
            [this]() { QApplication::clipboard()->setText(d_readout_text); });

    menu.addSeparator();
    QAction *actClose = menu.addAction(tr("Close Cursor"));
    QPointer<Graph> gPtr = d_graph_guard;
    connect(actClose, &QAction::triggered, this, [gPtr]() {
        if (gPtr)
            QTimer::singleShot(0, gPtr, [gPtr]() { RoiGadgetTool::clearGadget(gPtr); });
    });

    menu.exec(globalPos);
}

bool DeltaCursorTool::eventFilter(QObject *obj, QEvent *event)
{
    if (!d_graph_guard || !d_graph_guard->plotWidget()
        || obj != d_graph_guard->plotWidget()->canvas())
        return QObject::eventFilter(obj, event);

    Plot *plot = d_graph_guard->plotWidget();
    const int xAxis = d_overlay_item ? d_overlay_item->xAxis() : int(QwtPlot::xBottom);
    const int yAxis = d_overlay_item ? d_overlay_item->yAxis() : int(QwtPlot::yLeft);

    switch (event->type()) {
    case QEvent::ShortcutOverride: {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab || ke->key() == Qt::Key_Left
            || ke->key() == Qt::Key_Right
            || (d_snap_mode == FreeScreenCrosshair
                && (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down))) {
            ke->accept();
            return true;
        }
        break;
    }

    case QEvent::ContextMenu: {
        auto *ce = static_cast<QContextMenuEvent *>(event);
        showContextMenu(ce->globalPos());
        ce->accept();
        return true;
    }

    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::RightButton) {
            showContextMenu(me->globalPos());
            me->accept();
            return true;
        }
        if (me->button() == Qt::LeftButton && !(me->modifiers() & Qt::AltModifier)) {
            DragTarget hit = hitTestPixelPos(me->pos());
            if (hit == DragNone) {
                const int px1 = plot->transform(xAxis, d_readout.x1);
                const int px2 = plot->transform(xAxis, d_readout.x2);
                hit = (std::abs(me->pos().x() - px1) <= std::abs(me->pos().x() - px2))
                        ? DragCursor1
                        : DragCursor2;
                const double nx = plot->invTransform(xAxis, me->pos().x());
                const double ny = plot->invTransform(yAxis, me->pos().y());
                if (hit == DragCursor1) {
                    d_x1 = nx;
                    d_snap_idx1 = -1;
                    if (d_snap_mode == FreeScreenCrosshair)
                        d_y1 = ny;
                } else {
                    d_x2 = nx;
                    d_snap_idx2 = -1;
                    if (d_snap_mode == FreeScreenCrosshair)
                        d_y2 = ny;
                }
                recalculate();
                d_graph_guard->replot();
            }
            if (hit == DragCursor1)
                d_active_cursor = 1;
            else if (hit == DragCursor2)
                d_active_cursor = 2;
            d_drag_target = hit;
            d_drag_start_pos = me->pos();
            d_drag_start_x1 = d_readout.x1;
            d_drag_start_y1 = d_readout.y1;
            d_drag_start_x2 = d_readout.x2;
            d_drag_start_y2 = d_readout.y2;
            plot->canvas()->setFocus();
            d_graph_guard->replot();
            me->accept();
            return true;
        }
        break;
    }

    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (d_drag_target == DragNone) {
            updateCursorForPos(me->pos());
            return false;
        }
        if (!(me->buttons() & Qt::LeftButton)) {
            d_drag_target = DragNone;
            updateCursorForPos(me->pos());
            return false;
        }

        if (d_drag_target == DragCursor1) {
            d_x1 = plot->invTransform(xAxis, me->pos().x());
            d_snap_idx1 = -1;
            if (d_snap_mode == FreeScreenCrosshair)
                d_y1 = plot->invTransform(yAxis, me->pos().y());
        } else if (d_drag_target == DragCursor2) {
            d_x2 = plot->invTransform(xAxis, me->pos().x());
            d_snap_idx2 = -1;
            if (d_snap_mode == FreeScreenCrosshair)
                d_y2 = plot->invTransform(yAxis, me->pos().y());
        } else if (d_drag_target == DragSpan) {
            const QPoint delta = me->pos() - d_drag_start_pos;
            const int px1Orig = plot->transform(xAxis, d_drag_start_x1);
            const int px2Orig = plot->transform(xAxis, d_drag_start_x2);
            d_x1 = plot->invTransform(xAxis, px1Orig + delta.x());
            d_x2 = plot->invTransform(xAxis, px2Orig + delta.x());
            d_snap_idx1 = -1;
            d_snap_idx2 = -1;
            if (d_snap_mode == FreeScreenCrosshair) {
                const int py1Orig = plot->transform(yAxis, d_drag_start_y1);
                const int py2Orig = plot->transform(yAxis, d_drag_start_y2);
                d_y1 = plot->invTransform(yAxis, py1Orig + delta.y());
                d_y2 = plot->invTransform(yAxis, py2Orig + delta.y());
            }
        }
        recalculate();
        d_graph_guard->replot();
        me->accept();
        return true;
    }

    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (d_drag_target != DragNone && me->button() == Qt::LeftButton) {
            d_drag_target = DragNone;
            updateCursorForPos(me->pos());
            me->accept();
            return true;
        }
        break;
    }

    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
            setActiveCursor(d_active_cursor == 1 ? 2 : 1);
            ke->accept();
            return true;
        } else if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right) {
            const int stepDir = (ke->key() == Qt::Key_Right) ? 1 : -1;
            const bool stepBoth = (ke->modifiers() & Qt::ShiftModifier) != 0;

            if (d_snap_mode == SnapNearestPoint) {
                QwtPlotCurve *c = resolveTargetCurve();
                std::vector<std::pair<double, double>> pts;
                if (c && c->dataSize() > 0) {
                    const DataCurve *dc = dynamic_cast<const DataCurve *>(c);
                    const int n = c->dataSize();
                    pts.reserve(size_t(n));
                    for (int i = 0; i < n; ++i) {
                        if (dc && dc->isPointMasked(i))
                            continue;
                        const double px = c->x(i);
                        const double py = c->y(i);
                        if (std::isfinite(px) && std::isfinite(py))
                            pts.emplace_back(px, py);
                    }
                    if (!sequenceHasXReversals(pts, [](const auto &p) { return p.first; })) {
                        std::stable_sort(pts.begin(), pts.end(), [](const auto &a, const auto &b) {
                            return a.first < b.first;
                        });
                    }
                }

                if (!pts.empty()) {
                    const int M = static_cast<int>(pts.size());
                    auto resolveIdx = [&](int cachedIdx, double xq) -> int {
                        if (cachedIdx >= 0 && cachedIdx < M)
                            return cachedIdx;
                        int best = 0;
                        double bestD = std::fabs(pts[0].first - xq);
                        for (int i = 1; i < M; ++i) {
                            const double d = std::fabs(pts[size_t(i)].first - xq);
                            if (d < bestD) {
                                bestD = d;
                                best = i;
                            }
                        }
                        return best;
                    };
                    d_snap_idx1 = resolveIdx(d_snap_idx1, d_x1);
                    d_snap_idx2 = resolveIdx(d_snap_idx2, d_x2);
                    if (stepBoth || d_active_cursor == 1) {
                        d_snap_idx1 = std::clamp(d_snap_idx1 + stepDir, 0, M - 1);
                        d_x1 = pts[size_t(d_snap_idx1)].first;
                        d_y1 = pts[size_t(d_snap_idx1)].second;
                    }
                    if (stepBoth || d_active_cursor == 2) {
                        d_snap_idx2 = std::clamp(d_snap_idx2 + stepDir, 0, M - 1);
                        d_x2 = pts[size_t(d_snap_idx2)].first;
                        d_y2 = pts[size_t(d_snap_idx2)].second;
                    }
                    recalculate();
                    d_graph_guard->replot();
                    ke->accept();
                    return true;
                }
            }

            const int stepPx =
                    std::max(1, int(std::round(0.01 * std::max(100, plot->canvas()->width()))));
            if (stepBoth || d_active_cursor == 1) {
                const int px1 = plot->transform(xAxis, d_x1);
                d_x1 = plot->invTransform(xAxis, px1 + stepDir * stepPx);
                d_snap_idx1 = -1;
            }
            if (stepBoth || d_active_cursor == 2) {
                const int px2 = plot->transform(xAxis, d_x2);
                d_x2 = plot->invTransform(xAxis, px2 + stepDir * stepPx);
                d_snap_idx2 = -1;
            }
            recalculate();
            d_graph_guard->replot();
            ke->accept();
            return true;
        } else if (d_snap_mode == FreeScreenCrosshair
                   && (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down)) {
            const int stepDir = (ke->key() == Qt::Key_Up) ? -1 : 1; // canvas Y increases downward
            const bool stepBoth = (ke->modifiers() & Qt::ShiftModifier) != 0;
            const int stepPx =
                    std::max(1, int(std::round(0.01 * std::max(100, plot->canvas()->height()))));
            if (stepBoth || d_active_cursor == 1) {
                const int py1 = plot->transform(yAxis, d_y1);
                d_y1 = plot->invTransform(yAxis, py1 + stepDir * stepPx);
            }
            if (stepBoth || d_active_cursor == 2) {
                const int py2 = plot->transform(yAxis, d_y2);
                d_y2 = plot->invTransform(yAxis, py2 + stepDir * stepPx);
            }
            recalculate();
            d_graph_guard->replot();
            ke->accept();
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

// ============================================================================
// ImageProfileOverlayItem — 2D Heatmap Cross-Section Line Profile Overlay
// ============================================================================

class ImageProfileOverlayItem : public QwtPlotItem
{
public:
    explicit ImageProfileOverlayItem(ImageProfileGadgetTool *owner) : d_owner(owner)
    {
        setZ(1.0e6 + 3.0);
        setItemAttribute(QwtPlotItem::Legend, false);
        setItemAttribute(QwtPlotItem::AutoScale, false);
    }

    ~ImageProfileOverlayItem() override
    {
        if (d_owner && d_owner->d_overlay_item == this)
            d_owner->d_overlay_item = nullptr;
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 704; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        const Graph *g = d_owner ? d_owner->d_graph_guard.data() : nullptr;
        if (!g && plot())
            g = qobject_cast<const Graph *>(plot()->parent());
        if (isExportingOrPrinting(g, painter))
            return;
        if (!painter || !d_owner)
            return;

        if (!std::isfinite(d_owner->d_x0) || !std::isfinite(d_owner->d_y0))
            return;

        const int px0 = xMap.transform(d_owner->d_x0);
        const int py0 = yMap.transform(d_owner->d_y0);
        const bool exporting = isExportingOrPrinting(d_owner->d_graph_guard, painter);

        painter->save();
        painter->setClipRect(canvasRect);
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QColor colHoriz(0, 114, 178);  // Okabe-Ito Blue for Horizontal X-slice (y = y0)
        const QColor colVert(213, 94, 0);    // Okabe-Ito Vermilion for Vertical Y-slice (x = x0)

        // 1. Overlaid Horizontal X-slice profile curve in top 20% canvas band
        const int bandH = std::clamp(int(std::round(0.20 * canvasRect.height())), 26, 110);
        const int hBandTop = canvasRect.top() + 6;
        const int hBandBottom = hBandTop + bandH;
        if (d_owner->d_horiz_profile.size() >= 2 && hBandBottom < canvasRect.bottom() - 10) {
            painter->setPen(QPen(QColor(0, 114, 178, 90), 1.0, Qt::DotLine));
            painter->drawLine(canvasRect.left() + 2, hBandBottom, canvasRect.right() - 2,
                              hBandBottom);

            const double zSpan = d_owner->d_horiz_max - d_owner->d_horiz_min;
            auto mapHZ = [&](double z) -> double {
                const double t = (zSpan > 1e-30) ? ((z - d_owner->d_horiz_min) / zSpan) : 0.5;
                return double(hBandBottom) - std::clamp(t, 0.0, 1.0) * double(bandH);
            };

            QPolygonF hCurvePts;
            QPolygonF hFillPts;
            hCurvePts.reserve(d_owner->d_horiz_profile.size());
            hFillPts.reserve(d_owner->d_horiz_profile.size() + 2);
            const double xFirstPx = xMap.transform(d_owner->d_horiz_profile.front().x());
            const double xLastPx = xMap.transform(d_owner->d_horiz_profile.back().x());
            hFillPts.append(QPointF(xFirstPx, double(hBandBottom)));
            for (const QPointF &pt : d_owner->d_horiz_profile) {
                const QPointF pPx(xMap.transform(pt.x()), mapHZ(pt.y()));
                hCurvePts.append(pPx);
                hFillPts.append(pPx);
            }
            hFillPts.append(QPointF(xLastPx, double(hBandBottom)));

            painter->setBrush(QColor(0, 114, 178, exporting ? 30 : 45));
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(hFillPts);

            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(colHoriz, 1.8, Qt::SolidLine));
            painter->drawPolyline(hCurvePts);

            if (d_owner->d_horiz_fwhm > 0.0) {
                const double yHalfPx =
                        mapHZ(d_owner->d_horiz_min + 0.5 * (d_owner->d_horiz_max - d_owner->d_horiz_min));
                const double lxPx = xMap.transform(d_owner->d_horiz_fwhm_left);
                const double rxPx = xMap.transform(d_owner->d_horiz_fwhm_right);
                painter->setPen(QPen(colHoriz, 1.2, Qt::DashLine));
                painter->drawLine(QPointF(lxPx, yHalfPx), QPointF(rxPx, yHalfPx));
            }
        }

        // 2. Overlaid Vertical Y-slice profile curve in right 20% canvas band
        const int bandW = std::clamp(int(std::round(0.20 * canvasRect.width())), 26, 110);
        const int vBandRight = canvasRect.right() - 6;
        const int vBandLeft = vBandRight - bandW;
        if (d_owner->d_vert_profile.size() >= 2 && vBandLeft > canvasRect.left() + 10) {
            painter->setPen(QPen(QColor(213, 94, 0, 90), 1.0, Qt::DotLine));
            painter->drawLine(vBandRight, canvasRect.top() + 2, vBandRight,
                              canvasRect.bottom() - 2);

            const double zSpan = d_owner->d_vert_max - d_owner->d_vert_min;
            auto mapVZ = [&](double z) -> double {
                const double t = (zSpan > 1e-30) ? ((z - d_owner->d_vert_min) / zSpan) : 0.5;
                return double(vBandRight) - std::clamp(t, 0.0, 1.0) * double(bandW);
            };

            QPolygonF vCurvePts;
            QPolygonF vFillPts;
            vCurvePts.reserve(d_owner->d_vert_profile.size());
            vFillPts.reserve(d_owner->d_vert_profile.size() + 2);
            const double yFirstPx = yMap.transform(d_owner->d_vert_profile.front().x());
            const double yLastPx = yMap.transform(d_owner->d_vert_profile.back().x());
            vFillPts.append(QPointF(double(vBandRight), yFirstPx));
            for (const QPointF &pt : d_owner->d_vert_profile) {
                const QPointF pPx(mapVZ(pt.y()), yMap.transform(pt.x()));
                vCurvePts.append(pPx);
                vFillPts.append(pPx);
            }
            vFillPts.append(QPointF(double(vBandRight), yLastPx));

            painter->setBrush(QColor(213, 94, 0, exporting ? 30 : 45));
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(vFillPts);

            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(colVert, 1.8, Qt::SolidLine));
            painter->drawPolyline(vCurvePts);

            if (d_owner->d_vert_fwhm > 0.0) {
                const double xHalfPx =
                        mapVZ(d_owner->d_vert_min + 0.5 * (d_owner->d_vert_max - d_owner->d_vert_min));
                const double lyPx = yMap.transform(d_owner->d_vert_fwhm_left);
                const double ryPx = yMap.transform(d_owner->d_vert_fwhm_right);
                painter->setPen(QPen(colVert, 1.2, Qt::DashLine));
                painter->drawLine(QPointF(xHalfPx, lyPx), QPointF(xHalfPx, ryPx));
            }
        }

        // 3. Draggable Horizontal (y = y0) & Vertical (x = x0) crosshair lines
        painter->setPen(QPen(QColor(255, 255, 255, 175), 3.0, Qt::SolidLine));
        painter->drawLine(canvasRect.left(), py0, canvasRect.right(), py0);
        painter->drawLine(px0, canvasRect.top(), px0, canvasRect.bottom());

        painter->setPen(QPen(colHoriz, 1.8, Qt::DashLine));
        painter->drawLine(canvasRect.left(), py0, canvasRect.right(), py0);

        painter->setPen(QPen(colVert, 1.8, Qt::DashLine));
        painter->drawLine(px0, canvasRect.top(), px0, canvasRect.bottom());

        // Interactive edge handles (hidden during export/print)
        if (!exporting) {
            QPolygon hTri;
            hTri << QPoint(canvasRect.left() + 8, py0) << QPoint(canvasRect.left(), py0 - 5)
                 << QPoint(canvasRect.left(), py0 + 5);
            painter->setBrush(colHoriz);
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(hTri);

            QPolygon vTri;
            vTri << QPoint(px0, canvasRect.bottom() - 8) << QPoint(px0 - 5, canvasRect.bottom())
                 << QPoint(px0 + 5, canvasRect.bottom());
            painter->setBrush(colVert);
            painter->drawPolygon(vTri);
        }

        // 4. Center intersection target ring + Arial H/V badges
        painter->setBrush(QColor(255, 255, 255, 235));
        painter->setPen(QPen(QColor(20, 20, 20), 2.0));
        painter->drawEllipse(QPoint(px0, py0), 5, 5);
        painter->setBrush(colVert);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(QPoint(px0, py0), 2, 2);

        QFont tagFnt("Arial", 8, QFont::Bold);
        painter->setFont(tagFnt);
        const QFontMetrics tfm(tagFnt);
        auto drawBadge = [&](int bx, int by, const QString &lbl, const QColor &col) {
            const int bw = tfm.horizontalAdvance(lbl) + 8;
            const int bh = tfm.height() + 2;
            const QRect r(std::clamp(bx, canvasRect.left() + 2, canvasRect.right() - bw - 2),
                          std::clamp(by, canvasRect.top() + 2, canvasRect.bottom() - bh - 2), bw,
                          bh);
            painter->setBrush(col);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(r, 3.0, 3.0);
            painter->setPen(Qt::white);
            painter->drawText(r.left() + 4, r.top() + 1 + tfm.ascent(), lbl);
        };
        drawBadge(canvasRect.left() + 10, py0 - tfm.height() - 4, QStringLiteral("H"), colHoriz);
        drawBadge(px0 + 4, canvasRect.bottom() - tfm.height() - 6, QStringLiteral("V"), colVert);

        // 5. Arial readout box positioned away from top/right profile bands
        const QRect safeRect(canvasRect.left() + 4,
                             std::min(canvasRect.bottom() - 20, hBandBottom + 4),
                             std::max(80, vBandLeft - canvasRect.left() - 8),
                             std::max(40, canvasRect.bottom() - hBandBottom - 8));
        const QPoint anchor(safeRect.left() + 2, safeRect.bottom() - 80);
        drawReadoutBox(painter, safeRect.width() > 140 ? safeRect : canvasRect, anchor,
                       d_owner->d_readout_text, QColor(0, 114, 178), QColor(255, 255, 255, 236),
                       true);

        painter->restore();
    }

    ImageProfileGadgetTool *d_owner;
};

// ============================================================================
// ImageProfileGadgetTool Implementation
// ============================================================================

namespace {

static void computeSliceStatsAndFwhm(const QVector<QPointF> &prof, double &minOut, double &maxOut,
                                     double &peakCoordOut, double &fwhmOut, double &leftHalfOut,
                                     double &rightHalfOut)
{
    minOut = 0.0;
    maxOut = 0.0;
    peakCoordOut = 0.0;
    fwhmOut = 0.0;
    leftHalfOut = 0.0;
    rightHalfOut = 0.0;

    const int n = prof.size();
    if (n <= 0)
        return;

    minOut = prof[0].y();
    maxOut = prof[0].y();
    peakCoordOut = prof[0].x();
    int peakIdx = 0;
    for (int i = 1; i < n; ++i) {
        const double z = prof[i].y();
        if (z < minOut)
            minOut = z;
        if (z > maxOut) {
            maxOut = z;
            peakCoordOut = prof[i].x();
            peakIdx = i;
        }
    }

    leftHalfOut = peakCoordOut;
    rightHalfOut = peakCoordOut;
    const double amp = maxOut - minOut;
    if (n < 2 || !(amp > 1e-15))
        return;

    const double yHalf = minOut + 0.5 * amp;

    bool foundLeft = false;
    double xLeft = prof.front().x();
    for (int i = peakIdx; i > 0; --i) {
        const double z0 = prof[i - 1].y();
        const double z1 = prof[i].y();
        if ((z0 <= yHalf && z1 >= yHalf) || (z0 >= yHalf && z1 <= yHalf)) {
            const double dz = z1 - z0;
            const double frac = (std::fabs(dz) > 1e-15) ? ((yHalf - z0) / dz) : 0.5;
            xLeft = prof[i - 1].x() + frac * (prof[i].x() - prof[i - 1].x());
            foundLeft = true;
            break;
        }
    }

    bool foundRight = false;
    double xRight = prof.back().x();
    for (int i = peakIdx; i + 1 < n; ++i) {
        const double z0 = prof[i].y();
        const double z1 = prof[i + 1].y();
        if ((z0 >= yHalf && z1 <= yHalf) || (z0 <= yHalf && z1 >= yHalf)) {
            const double dz = z1 - z0;
            const double frac = (std::fabs(dz) > 1e-15) ? ((yHalf - z0) / dz) : 0.5;
            xRight = prof[i].x() + frac * (prof[i + 1].x() - prof[i].x());
            foundRight = true;
            break;
        }
    }

    if (foundLeft || foundRight) {
        leftHalfOut = std::min(xLeft, xRight);
        rightHalfOut = std::max(xLeft, xRight);
        fwhmOut = std::max(0.0, rightHalfOut - leftHalfOut);
    }
}

} // namespace

ImageProfileGadgetTool::ImageProfileGadgetTool(Graph *graph, ApplicationWindow *app)
    : QObject(nullptr),
      PlotToolInterface(graph),
      d_app(app ? app : resolveAppWindowFromGraph(graph)),
      d_graph_guard(graph)
{
    if (d_graph_guard) {
        connect(d_graph_guard, &QObject::destroyed, this, &QObject::deleteLater);
        if (d_graph_guard->plotWidget()) {
            d_overlay_item = new ImageProfileOverlayItem(this);
            d_overlay_item->attach(d_graph_guard->plotWidget());
            if (QwtPlotCanvas *canvas = d_graph_guard->plotWidget()->canvas()) {
                canvas->setMouseTracking(true);
                canvas->installEventFilter(this);
                canvas->setFocus();
            }
        }
    }
    initDefaultPosition();
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

ImageProfileGadgetTool::~ImageProfileGadgetTool()
{
    if (d_graph_guard && d_graph_guard->plotWidget()) {
        if (QwtPlotCanvas *canvas = d_graph_guard->plotWidget()->canvas()) {
            canvas->removeEventFilter(this);
            canvas->unsetCursor();
        }
    }
    if (d_overlay_item) {
        d_overlay_item->d_owner = nullptr;
        d_overlay_item->detach();
        delete d_overlay_item;
        d_overlay_item = nullptr;
    }
}

ImageProfileGadgetTool *ImageProfileGadgetTool::attach(Graph *g, ApplicationWindow *app)
{
    if (!g)
        return nullptr;
    g->setActiveTool(nullptr);
    ImageProfileGadgetTool *tool = new ImageProfileGadgetTool(g, app);
    g->setActiveTool(tool);
    g->replot();
    return tool;
}

QList<QwtPlotSpectrogram *> ImageProfileGadgetTool::availableSpectrograms() const
{
    QList<QwtPlotSpectrogram *> list;
    if (!d_graph_guard || !d_graph_guard->plotWidget())
        return list;

    const QwtPlotItemList &items = d_graph_guard->plotWidget()->itemList();
    for (QwtPlotItem *it : items) {
        if (!it || !it->isVisible())
            continue;
        if (auto *qwtSpec = dynamic_cast<QwtPlotSpectrogram *>(it))
            list.append(qwtSpec);
    }
    return list;
}

QwtPlotSpectrogram *
ImageProfileGadgetTool::resolveTargetSpectrogram(Spectrogram **outSciSpec) const
{
    if (outSciSpec)
        *outSciSpec = nullptr;
    const QList<QwtPlotSpectrogram *> specs = availableSpectrograms();
    if (specs.isEmpty())
        return nullptr;
    const int idx = std::clamp(d_spec_index, 0, specs.size() - 1);
    QwtPlotSpectrogram *chosen = specs[idx];
    if (outSciSpec)
        *outSciSpec = dynamic_cast<Spectrogram *>(chosen);
    return chosen;
}

QRectF ImageProfileGadgetTool::spectrogramDomainRect(QwtPlotSpectrogram *qwtSpec, Matrix *m) const
{
    if (m && m->numRows() > 0 && m->numCols() > 0) {
        return QRectF(QPointF(m->xStart(), m->yStart()), QPointF(m->xEnd(), m->yEnd()))
                .normalized();
    }
    if (qwtSpec) {
        const QwtDoubleRect br = qwtSpec->boundingRect();
        const QRectF r = QRectF(br.left(), br.top(), br.width(), br.height()).normalized();
        if (r.width() > 0.0 || r.height() > 0.0)
            return r;
    }
    if (d_graph_guard && d_graph_guard->plotWidget()) {
        Plot *plot = d_graph_guard->plotWidget();
        const QwtScaleDiv *xDiv = plot->axisScaleDiv(QwtPlot::xBottom);
        const QwtScaleDiv *yDiv = plot->axisScaleDiv(QwtPlot::yLeft);
        if (xDiv && yDiv) {
            return QRectF(QPointF(xDiv->lowerBound(), yDiv->lowerBound()),
                          QPointF(xDiv->upperBound(), yDiv->upperBound()))
                    .normalized();
        }
    }
    return QRectF(0.0, 0.0, 1.0, 1.0);
}

void ImageProfileGadgetTool::initDefaultPosition()
{
    Spectrogram *sciSpec = nullptr;
    QwtPlotSpectrogram *qwtSpec = resolveTargetSpectrogram(&sciSpec);
    Matrix *m = sciSpec ? sciSpec->matrix() : nullptr;
    const QRectF dom = spectrogramDomainRect(qwtSpec, m);
    d_x0 = dom.center().x();
    d_y0 = dom.center().y();
}

void ImageProfileGadgetTool::onSourceMatrixModified()
{
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void ImageProfileGadgetTool::setTargetSpectrogramIndex(int idx)
{
    d_spec_index = std::max(0, idx);
    initDefaultPosition();
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void ImageProfileGadgetTool::setCrosshairPosition(double x, double y)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    d_x0 = x;
    d_y0 = y;
    recalculate();
    if (d_graph_guard)
        d_graph_guard->replot();
}

void ImageProfileGadgetTool::recalculate()
{
    d_horiz_profile.clear();
    d_vert_profile.clear();
    d_z0 = 0.0;
    d_horiz_min = 0.0;
    d_horiz_max = 0.0;
    d_horiz_peak_x = d_x0;
    d_horiz_fwhm = 0.0;
    d_horiz_fwhm_left = d_x0;
    d_horiz_fwhm_right = d_x0;
    d_vert_min = 0.0;
    d_vert_max = 0.0;
    d_vert_peak_y = d_y0;
    d_vert_fwhm = 0.0;
    d_vert_fwhm_left = d_y0;
    d_vert_fwhm_right = d_y0;

    Spectrogram *sciSpec = nullptr;
    QwtPlotSpectrogram *qwtSpec = resolveTargetSpectrogram(&sciSpec);
    if (!qwtSpec) {
        d_readout_text = tr("2D Image Profile\nNo Spectrogram / Matrix on Graph");
        if (d_app)
            d_app->setStatusBarText(d_readout_text.simplified());
        return;
    }

    if (d_overlay_item)
        d_overlay_item->setAxis(qwtSpec->xAxis(), qwtSpec->yAxis());

    Matrix *m = sciSpec ? sciSpec->matrix() : nullptr;
    if (m && m != d_connected_matrix) {
        if (d_connected_matrix)
            disconnect(d_connected_matrix, nullptr, this, nullptr);
        d_connected_matrix = m;
        connect(m, &MyWidget::modifiedWindow, this,
                &ImageProfileGadgetTool::onSourceMatrixModified, Qt::UniqueConnection);
    }

    const QRectF dom = spectrogramDomainRect(qwtSpec, m);
    d_x0 = std::clamp(d_x0, dom.left(), dom.right());
    d_y0 = std::clamp(d_y0, dom.top(), dom.bottom());

    auto evalZ = [&](double x, double y) -> double {
        if (m && m->numRows() > 0 && m->numCols() > 0)
            return evalMatrixBilinear(m, x, y);
        const double v = qwtSpec->data().value(x, y);
        return std::isfinite(v) ? v : 0.0;
    };

    d_z0 = evalZ(d_x0, d_y0);

    const int nH = (m && m->numCols() >= 2) ? m->numCols()
                                            : ((m && m->numCols() == 1) ? 1 : 128);
    d_horiz_profile.reserve(nH);
    if (nH == 1) {
        d_horiz_profile.append(QPointF(dom.left(), evalZ(dom.left(), d_y0)));
    } else {
        for (int i = 0; i < nH; ++i) {
            const double x = dom.left() + dom.width() * double(i) / double(nH - 1);
            d_horiz_profile.append(QPointF(x, evalZ(x, d_y0)));
        }
    }

    const int nV = (m && m->numRows() >= 2) ? m->numRows()
                                            : ((m && m->numRows() == 1) ? 1 : 128);
    d_vert_profile.reserve(nV);
    if (nV == 1) {
        d_vert_profile.append(QPointF(dom.top(), evalZ(d_x0, dom.top())));
    } else {
        for (int j = 0; j < nV; ++j) {
            const double y = dom.top() + dom.height() * double(j) / double(nV - 1);
            d_vert_profile.append(QPointF(y, evalZ(d_x0, y)));
        }
    }

    computeSliceStatsAndFwhm(d_horiz_profile, d_horiz_min, d_horiz_max, d_horiz_peak_x,
                             d_horiz_fwhm, d_horiz_fwhm_left, d_horiz_fwhm_right);
    computeSliceStatsAndFwhm(d_vert_profile, d_vert_min, d_vert_max, d_vert_peak_y, d_vert_fwhm,
                             d_vert_fwhm_left, d_vert_fwhm_right);

    const QString srcName = m ? m->name() : qwtSpec->title().text();
    d_readout_text = tr("2D Profile [%1]\n"
                        "(X0, Y0) = (%2, %3), Z = %4\n"
                        "H: Peak X = %5, Min/Max = %6 / %7, FWHM = %8\n"
                        "V: Peak Y = %9, Min/Max = %10 / %11, FWHM = %12")
                             .arg(srcName)
                             .arg(d_x0, 0, 'g', 5)
                             .arg(d_y0, 0, 'g', 5)
                             .arg(d_z0, 0, 'g', 6)
                             .arg(d_horiz_peak_x, 0, 'g', 5)
                             .arg(d_horiz_min, 0, 'g', 4)
                             .arg(d_horiz_max, 0, 'g', 4)
                             .arg(d_horiz_fwhm, 0, 'g', 5)
                             .arg(d_vert_peak_y, 0, 'g', 5)
                             .arg(d_vert_min, 0, 'g', 4)
                             .arg(d_vert_max, 0, 'g', 4)
                             .arg(d_vert_fwhm, 0, 'g', 5);

    if (d_app)
        d_app->setStatusBarText(d_readout_text.simplified());
}

void ImageProfileGadgetTool::stampCalloutOnGraph()
{
    if (!d_graph_guard)
        return;

    recalculate();
    MultiLayer *ml = d_graph_guard->multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    if (Legend *mrk = d_graph_guard->newLegend(d_readout_text)) {
        mrk->setFont(QFont("Arial", 9));
        mrk->setFrameStyle(Legend::Line);
        mrk->setBackgroundColor(QColor(255, 255, 255, 238));
        mrk->setOriginCoord(d_x0, d_y0);
        d_graph_guard->replot();
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(tr("Stamp Profile Readout on Graph"), beforeSnap);
        if (d_app)
            d_app->modifiedProject();
    }
}

Table *ImageProfileGadgetTool::extractProfilesToTableAndGraph(bool createGraph)
{
    if (!d_app)
        d_app = resolveAppWindowFromGraph(d_graph_guard);
    if (!d_app)
        return nullptr;

    recalculate();
    const int nH = d_horiz_profile.size();
    const int nV = d_vert_profile.size();
    const int nRows = std::max({ 1, nH, nV });

    const QString tblName = d_app->generateUniqueName(tr("ImageProfile"), false);
    Table *t = d_app->newTable(nRows, 4, tblName, tr("2D Cross-Section Line Profiles"));
    if (!t)
        return nullptr;

    t->setColName(0, QStringLiteral("H_X"));
    t->setColName(1, QStringLiteral("H_Z"));
    t->setColName(2, QStringLiteral("V_Y"));
    t->setColName(3, QStringLiteral("V_Z"));
    t->setColPlotDesignation(0, SciDAVis::X);
    t->setColPlotDesignation(1, SciDAVis::Y);
    t->setColPlotDesignation(2, SciDAVis::X);
    t->setColPlotDesignation(3, SciDAVis::Y);

    for (int r = 0; r < nRows; ++r) {
        if (r < nH) {
            t->setCell(r, 0, d_horiz_profile[r].x());
            t->setCell(r, 1, d_horiz_profile[r].y());
        } else {
            if (t->column(0))
                t->column(0)->setInvalid(r, true);
            if (t->column(1))
                t->column(1)->setInvalid(r, true);
        }
        if (r < nV) {
            t->setCell(r, 2, d_vert_profile[r].x());
            t->setCell(r, 3, d_vert_profile[r].y());
        } else {
            if (t->column(2))
                t->column(2)->setInvalid(r, true);
            if (t->column(3))
                t->column(3)->setInvalid(r, true);
        }
    }

    if (createGraph && (nH >= 2 || nV >= 2)) {
        MultiLayer *ml = d_app->newGraph(
                d_app->generateUniqueName(tr("ImageProfileGraph"), false));
        if (ml) {
            const QFont titleFnt("Arial", 10, QFont::Normal);
            const QFont axisFnt("Arial", 9, QFont::Normal);
            const QFont numFnt("Arial", 9);
            const QFont legFnt("Arial", 9);

            Graph *g1 = ml->activeGraph();
            Graph *g2 = ml->addLayer();
            if (g2)
                d_app->setPreferences(g2);

            ml->setRows(2);
            ml->setCols(1);
            ml->arrangeLayers(true, false);
            ml->setFonts(titleFnt, axisFnt, numFnt, legFnt);

            if (g1 && nH >= 2) {
                if (g1->insertCurve(t, t->colName(0), t->colName(1), Graph::Line, 0, nH - 1)) {
                    if (QwtPlotCurve *c1 = g1->curve(g1->curves() - 1))
                        c1->setPen(QPen(QColor(0, 114, 178), 2.0, Qt::SolidLine));
                }
                g1->setTitle(tr("Horizontal X-Profile (Y = %1, FWHM = %2)")
                                     .arg(d_y0, 0, 'g', 5)
                                     .arg(d_horiz_fwhm, 0, 'g', 5));
                g1->setTitleFont(titleFnt);
                g1->setXAxisTitle(tr("X"));
                g1->setYAxisTitle(tr("Intensity Z"));
                g1->initFonts(axisFnt, numFnt);
                if (Legend *lg1 = g1->legend())
                    lg1->setFont(legFnt);
                g1->setAutoScale();
                g1->replot();
            }

            if (g2 && nV >= 2) {
                if (g2->insertCurve(t, t->colName(2), t->colName(3), Graph::Line, 0, nV - 1)) {
                    if (QwtPlotCurve *c2 = g2->curve(g2->curves() - 1))
                        c2->setPen(QPen(QColor(213, 94, 0), 2.0, Qt::SolidLine));
                }
                g2->setTitle(tr("Vertical Y-Profile (X = %1, FWHM = %2)")
                                     .arg(d_x0, 0, 'g', 5)
                                     .arg(d_vert_fwhm, 0, 'g', 5));
                g2->setTitleFont(titleFnt);
                g2->setXAxisTitle(tr("Y"));
                g2->setYAxisTitle(tr("Intensity Z"));
                g2->initFonts(axisFnt, numFnt);
                if (Legend *lg2 = g2->legend())
                    lg2->setFont(legFnt);
                g2->setAutoScale();
                g2->replot();
            }
            if (g1)
                ml->setActiveGraph(g1);
        }
    }

    return t;
}

ImageProfileGadgetTool::DragTarget ImageProfileGadgetTool::hitTestPixelPos(const QPoint &pos) const
{
    if (!d_graph_guard || !d_graph_guard->plotWidget())
        return DragNone;

    Plot *plot = d_graph_guard->plotWidget();
    const int xAxis = d_overlay_item ? d_overlay_item->xAxis() : int(QwtPlot::xBottom);
    const int yAxis = d_overlay_item ? d_overlay_item->yAxis() : int(QwtPlot::yLeft);
    const int px0 = plot->transform(xAxis, d_x0);
    const int py0 = plot->transform(yAxis, d_y0);

    const int dx = std::abs(pos.x() - px0);
    const int dy = std::abs(pos.y() - py0);
    const int tol = 7;
    if (dx <= tol + 2 && dy <= tol + 2)
        return DragIntersection;
    if (dy <= tol)
        return DragHorizontal;
    if (dx <= tol)
        return DragVertical;
    return DragNone;
}

void ImageProfileGadgetTool::updateCursorForPos(const QPoint &pos)
{
    if (!d_graph_guard || !d_graph_guard->plotWidget() || !d_graph_guard->plotWidget()->canvas())
        return;
    QWidget *canvas = d_graph_guard->plotWidget()->canvas();
    switch (hitTestPixelPos(pos)) {
    case DragIntersection:
        canvas->setCursor(Qt::SizeAllCursor);
        break;
    case DragHorizontal:
        canvas->setCursor(Qt::SplitVCursor);
        break;
    case DragVertical:
        canvas->setCursor(Qt::SplitHCursor);
        break;
    default:
        canvas->setCursor(Qt::CrossCursor);
        break;
    }
}

void ImageProfileGadgetTool::showContextMenu(const QPoint &globalPos)
{
    if (!d_graph_guard)
        return;

    QMenu menu;
    const QList<QwtPlotSpectrogram *> specs = availableSpectrograms();
    if (specs.size() > 1) {
        QMenu *specMenu = menu.addMenu(tr("Target Matrix / Image"));
        for (int i = 0; i < specs.size(); ++i) {
            auto *sci = dynamic_cast<Spectrogram *>(specs[i]);
            const QString lbl = (sci && sci->matrix()) ? sci->matrix()->name()
                                                       : specs[i]->title().text();
            QAction *act = specMenu->addAction(lbl.isEmpty() ? tr("Image %1").arg(i + 1) : lbl);
            act->setCheckable(true);
            act->setChecked(i == std::clamp(d_spec_index, 0, specs.size() - 1));
            connect(act, &QAction::triggered, this,
                    [this, i]() { setTargetSpectrogramIndex(i); });
        }
        menu.addSeparator();
    }

    QAction *actPeak = menu.addAction(tr("Center Crosshair on Peak Z"));
    connect(actPeak, &QAction::triggered, this, [this]() {
        Spectrogram *sciSpec = nullptr;
        QwtPlotSpectrogram *qwtSpec = resolveTargetSpectrogram(&sciSpec);
        Matrix *m = sciSpec ? sciSpec->matrix() : nullptr;
        if (m && m->numRows() > 0 && m->numCols() > 0) {
            const int rows = m->numRows();
            const int cols = m->numCols();
            double bestZ = -std::numeric_limits<double>::infinity();
            double bestX = d_x0, bestY = d_y0;
            for (int r = 0; r < rows; ++r) {
                const double y = (rows > 1)
                        ? (m->yStart() + double(r) * (m->yEnd() - m->yStart()) / double(rows - 1))
                        : m->yStart();
                for (int c = 0; c < cols; ++c) {
                    const double z = safeMatrixCell(m, r, c);
                    if (z > bestZ) {
                        bestZ = z;
                        bestX = (cols > 1)
                                ? (m->xStart()
                                   + double(c) * (m->xEnd() - m->xStart()) / double(cols - 1))
                                : m->xStart();
                        bestY = y;
                    }
                }
            }
            setCrosshairPosition(bestX, bestY);
        } else if (qwtSpec) {
            const QRectF dom = spectrogramDomainRect(qwtSpec, nullptr);
            setCrosshairPosition(dom.center().x(), dom.center().y());
        }
    });

    QAction *actCenter = menu.addAction(tr("Center Crosshair on Image"));
    connect(actCenter, &QAction::triggered, this, [this]() {
        initDefaultPosition();
        recalculate();
        if (d_graph_guard)
            d_graph_guard->replot();
    });

    menu.addSeparator();
    QAction *actExtractBoth = menu.addAction(tr("Extract X/Y Profiles to Worksheet && Graph"));
    connect(actExtractBoth, &QAction::triggered, this,
            [this]() { extractProfilesToTableAndGraph(true); });

    QAction *actExtractTbl = menu.addAction(tr("Extract X/Y Profiles to Worksheet Only"));
    connect(actExtractTbl, &QAction::triggered, this,
            [this]() { extractProfilesToTableAndGraph(false); });

    QAction *actStamp = menu.addAction(tr("Stamp Profile Readout on Graph"));
    connect(actStamp, &QAction::triggered, this, [this]() { stampCalloutOnGraph(); });

    QAction *actCopy = menu.addAction(tr("Copy Readout to Clipboard"));
    connect(actCopy, &QAction::triggered, this,
            [this]() { QApplication::clipboard()->setText(d_readout_text); });

    menu.addSeparator();
    QAction *actClose = menu.addAction(tr("Close 2D Profile Gadget"));
    QPointer<Graph> gPtr = d_graph_guard;
    QPointer<ImageProfileGadgetTool> selfPtr = this;
    connect(actClose, &QAction::triggered, this, [gPtr, selfPtr]() {
        QTimer::singleShot(0, [gPtr, selfPtr]() {
            if (gPtr && selfPtr && gPtr->activeTool() == selfPtr.data())
                RoiGadgetTool::clearGadget(gPtr);
            else if (selfPtr)
                selfPtr->deleteLater();
        });
    });

    menu.exec(globalPos);
}

bool ImageProfileGadgetTool::eventFilter(QObject *obj, QEvent *event)
{
    if (!d_graph_guard || !d_graph_guard->plotWidget()
        || obj != d_graph_guard->plotWidget()->canvas())
        return QObject::eventFilter(obj, event);

    Plot *plot = d_graph_guard->plotWidget();
    const int xAxis = d_overlay_item ? d_overlay_item->xAxis() : int(QwtPlot::xBottom);
    const int yAxis = d_overlay_item ? d_overlay_item->yAxis() : int(QwtPlot::yLeft);

    switch (event->type()) {
    case QEvent::ShortcutOverride: {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right || ke->key() == Qt::Key_Up
            || ke->key() == Qt::Key_Down) {
            ke->accept();
            return true;
        }
        break;
    }

    case QEvent::ContextMenu: {
        auto *ce = static_cast<QContextMenuEvent *>(event);
        showContextMenu(ce->globalPos());
        ce->accept();
        return true;
    }

    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::RightButton) {
            showContextMenu(me->globalPos());
            me->accept();
            return true;
        }
        if (me->button() == Qt::LeftButton && !(me->modifiers() & Qt::AltModifier)) {
            DragTarget hit = hitTestPixelPos(me->pos());
            if (hit == DragNone) {
                d_x0 = plot->invTransform(xAxis, me->pos().x());
                d_y0 = plot->invTransform(yAxis, me->pos().y());
                hit = DragIntersection;
                recalculate();
                d_graph_guard->replot();
            }
            d_drag_target = hit;
            plot->canvas()->setFocus();
            me->accept();
            return true;
        }
        break;
    }

    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (d_drag_target == DragNone) {
            updateCursorForPos(me->pos());
            return false;
        }
        if (!(me->buttons() & Qt::LeftButton)) {
            d_drag_target = DragNone;
            updateCursorForPos(me->pos());
            return false;
        }

        if (d_drag_target == DragIntersection || d_drag_target == DragVertical)
            d_x0 = plot->invTransform(xAxis, me->pos().x());
        if (d_drag_target == DragIntersection || d_drag_target == DragHorizontal)
            d_y0 = plot->invTransform(yAxis, me->pos().y());

        recalculate();
        d_graph_guard->replot();
        me->accept();
        return true;
    }

    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (d_drag_target != DragNone && me->button() == Qt::LeftButton) {
            d_drag_target = DragNone;
            updateCursorForPos(me->pos());
            me->accept();
            return true;
        }
        break;
    }

    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right || ke->key() == Qt::Key_Up
            || ke->key() == Qt::Key_Down) {
            Spectrogram *sciSpec = nullptr;
            QwtPlotSpectrogram *qwtSpec = resolveTargetSpectrogram(&sciSpec);
            Matrix *m = sciSpec ? sciSpec->matrix() : nullptr;
            const QRectF dom = spectrogramDomainRect(qwtSpec, m);
            const double mult = (ke->modifiers() & Qt::ShiftModifier) ? 5.0 : 1.0;

            const double dxStep = (m && m->numCols() > 1)
                    ? (dom.width() / double(m->numCols() - 1))
                    : std::max(1e-12, 0.01 * dom.width());
            const double dyStep = (m && m->numRows() > 1)
                    ? (dom.height() / double(m->numRows() - 1))
                    : std::max(1e-12, 0.01 * dom.height());

            if (ke->key() == Qt::Key_Left)
                d_x0 -= mult * dxStep;
            else if (ke->key() == Qt::Key_Right)
                d_x0 += mult * dxStep;
            else if (ke->key() == Qt::Key_Up)
                d_y0 += mult * dyStep;
            else if (ke->key() == Qt::Key_Down)
                d_y0 -= mult * dyStep;

            recalculate();
            d_graph_guard->replot();
            ke->accept();
            return true;
        } else if (ke->key() == Qt::Key_Escape) {
            QPointer<Graph> gPtr = d_graph_guard;
            QPointer<ImageProfileGadgetTool> selfPtr = this;
            QTimer::singleShot(0, [gPtr, selfPtr]() {
                if (gPtr && selfPtr && gPtr->activeTool() == selfPtr.data())
                    RoiGadgetTool::clearGadget(gPtr);
                else if (selfPtr)
                    selfPtr->deleteLater();
            });
            return true;
        }
        break;
    }

    default:
        break;
    }

    return QObject::eventFilter(obj, event);
}
