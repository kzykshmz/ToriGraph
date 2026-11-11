/***************************************************************************
    File                 : GraphStatsAndSurfaceFit.cpp
    Project              : ToriGraph (SciDAVis OriginPro 2021 Parity)
    Description          : 3D Surface Fitting, 2D Peak Analysis, and Graph
                           Statistical Analysis (Confidence Ellipse, ANOVA/t-Test
                           with Significance Brackets, ROC, Kaplan-Meier, PCA Biplot)
 ***************************************************************************/
#include "GraphStatsAndSurfaceFit.h"

#include "ApplicationWindow.h"
#include "ArrowMarker.h"
#include "Graph.h"
#include "Legend.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "MyParser.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "RecalcEngine.h"
#include "Table.h"
#include "core/column/Column.h"

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_symbol.h>

#include <gsl/gsl_blas.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_eigen.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_multifit_nlin.h>
#include <gsl/gsl_version.h>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kFwhmFactor = 2.3548200450309493820231386529194; // 2 * sqrt(2 * ln(2))

Column *resolveTableColumn(Table *t, const QString &colSpec)
{
    if (!t || colSpec.trimmed().isEmpty())
        return nullptr;
    Column *c = t->column(colSpec);
    if (c)
        return c;
    QString trimmed = colSpec.trimmed();
    if (trimmed.startsWith(t->name() + "_"))
        c = t->column(trimmed.mid(t->name().length() + 1));
    if (c)
        return c;
    for (int i = 0; i < t->numCols(); ++i) {
        Column *cand = t->column(i);
        if (!cand)
            continue;
        if (cand->name().compare(trimmed, Qt::CaseInsensitive) == 0
            || t->colName(i).compare(trimmed, Qt::CaseInsensitive) == 0)
            return cand;
    }
    return nullptr;
}

QString shortColumnLabel(Table *t, const QString &colSpec)
{
    Column *c = resolveTableColumn(t, colSpec);
    if (c)
        return c->name();
    if (t && colSpec.startsWith(t->name() + "_"))
        return colSpec.mid(t->name().length() + 1);
    return colSpec;
}

QList<Table *> allProjectTables(ApplicationWindow *app)
{
    QList<Table *> list;
    if (!app)
        return list;
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("Table"))
            list.append(static_cast<Table *>(w));
    }
    return list;
}

QList<Matrix *> allProjectMatrices(ApplicationWindow *app)
{
    QList<Matrix *> list;
    if (!app)
        return list;
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("Matrix"))
            list.append(static_cast<Matrix *>(w));
    }
    return list;
}

Graph *resolveActiveGraph(ApplicationWindow *app, Graph *explicitGraph = nullptr)
{
    if (explicitGraph)
        return explicitGraph;
    if (!app)
        return nullptr;
    QMdiSubWindow *sub = app->d_workspace.activeSubWindow();
    if (sub) {
        if (sub->inherits("MultiLayer"))
            return static_cast<MultiLayer *>(sub)->activeGraph();
        if (QWidget *w = sub->widget()) {
            if (w->inherits("MultiLayer"))
                return static_cast<MultiLayer *>(w)->activeGraph();
        }
    }
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer")) {
            Graph *g = static_cast<MultiLayer *>(w)->activeGraph();
            if (g)
                return g;
        }
    }
    return nullptr;
}

Table *resolveActiveTable(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    QMdiSubWindow *sub = app->d_workspace.activeSubWindow();
    if (sub) {
        if (sub->inherits("Table"))
            return static_cast<Table *>(sub);
        if (QWidget *w = sub->widget()) {
            if (w->inherits("Table"))
                return static_cast<Table *>(w);
        }
    }
    QList<Table *> tbls = allProjectTables(app);
    return tbls.isEmpty() ? nullptr : tbls.first();
}

Matrix *resolveActiveMatrix(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    QMdiSubWindow *sub = app->d_workspace.activeSubWindow();
    if (sub) {
        if (sub->inherits("Matrix"))
            return static_cast<Matrix *>(sub);
        if (QWidget *w = sub->widget()) {
            if (w->inherits("Matrix"))
                return static_cast<Matrix *>(w);
        }
    }
    QList<Matrix *> mats = allProjectMatrices(app);
    return mats.isEmpty() ? nullptr : mats.first();
}

QwtPlotCurve *resolveCurveOnGraph(Graph *g, const QString &curveTitle)
{
    if (!g || g->curves() <= 0)
        return nullptr;
    if (!curveTitle.isEmpty()) {
        QwtPlotCurve *c = g->curve(curveTitle);
        if (c)
            return c;
        for (int i = 0; i < g->curves(); ++i) {
            QwtPlotCurve *cand = g->curve(i);
            if (!cand)
                continue;
            QString t = cand->title().text();
            if (t.compare(curveTitle, Qt::CaseInsensitive) == 0
                || t.endsWith("_" + curveTitle, Qt::CaseInsensitive))
                return cand;
        }
    }
    QString sel = g->selectedCurveTitle();
    if (!sel.isEmpty()) {
        if (QwtPlotCurve *c = g->curve(sel))
            return c;
    }
    QStringList analysable = g->analysableCurvesList();
    if (!analysable.isEmpty())
        return g->curve(analysable.first());
    return g->curve(0);
}

void addLineSegmentOnGraph(Graph *g, double x0, double y0, double x1, double y1,
                           const QColor &color, int width = 2,
                           Qt::PenStyle style = Qt::SolidLine, bool endArrow = false,
                           int headLength = 8, int headAngle = 32)
{
    if (!g)
        return;
    ArrowMarker mrk;
    mrk.setStartPoint(x0, y0);
    mrk.setEndPoint(x1, y1);
    mrk.setBoundingRect(x0, y0, x1, y1);
    mrk.setColor(color);
    mrk.setWidth(width);
    mrk.setStyle(style);
    mrk.drawStartArrow(false);
    mrk.drawEndArrow(endArrow);
    mrk.setHeadLength(headLength);
    mrk.setHeadAngle(headAngle);
    mrk.fillArrowHead(true);
    g->addArrow(&mrk);
}

Legend *addTextLabelOnGraph(Graph *g, double x, double y, const QString &text,
                            const QColor &color = QColor(20, 20, 20), bool bold = true,
                            int pointSizeDelta = 0)
{
    if (!g)
        return nullptr;
    Legend *lbl = g->newLegend(text);
    if (!lbl)
        return nullptr;
    lbl->setFrameStyle(Legend::None);
    lbl->setBackgroundColor(QColor(255, 255, 255, 0));
    lbl->setTextColor(color);
    QFont f = g->defaultTextMarkerFont();
    f.setBold(bold);
    if (pointSizeDelta != 0 && f.pointSize() + pointSizeDelta >= 6)
        f.setPointSize(f.pointSize() + pointSizeDelta);
    lbl->setFont(f);
    lbl->setOriginCoord(x, y);
    return lbl;
}

// ============================================================================
// 2D Surface Model Evaluation & Levenberg-Marquardt Solver
// ============================================================================

struct SurfaceEvalContext
{
    GraphStatsAndSurfaceFit::SurfaceModel model = GraphStatsAndSurfaceFit::Gauss2D;
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> z;
    double xMin = 0.0;
    double xMax = 1.0;
    double yMin = 0.0;
    double yMax = 1.0;
    QString customExpr;
    QStringList paramNames;

    // Reusable parser state for Custom2D
    mutable std::unique_ptr<MyParser> parser;
    mutable double varX = 0.0;
    mutable double varY = 0.0;
    mutable std::vector<double> varParams;
    mutable bool parserReady = false;

    bool ensureParser() const
    {
        if (model != GraphStatsAndSurfaceFit::Custom2D)
            return true;
        if (parserReady && parser)
            return true;
        parser = std::make_unique<MyParser>();
        varParams.assign(static_cast<size_t>(paramNames.size()), 1.0);
        try {
            parser->DefineVar(toString<mu::string_type>(QStringLiteral("x")), &varX);
            parser->DefineVar(toString<mu::string_type>(QStringLiteral("y")), &varY);
            for (int j = 0; j < paramNames.size(); ++j) {
                parser->DefineVar(toString<mu::string_type>(paramNames[j]),
                                  &varParams[static_cast<size_t>(j)]);
            }
            parser->SetExpr(customExpr);
            parserReady = true;
            return true;
        } catch (...) {
            parserReady = false;
            return false;
        }
    }

    double evalPoint(double px, double py, const double *p) const
    {
        switch (model) {
        case GraphStatsAndSurfaceFit::Plane2D:
            return p[0] + p[1] * px + p[2] * py;

        case GraphStatsAndSurfaceFit::Poly2DQuadratic:
            return p[0] + p[1] * px + p[2] * py + p[3] * px * px + p[4] * py * py
                    + p[5] * px * py;

        case GraphStatsAndSurfaceFit::Gauss2D: {
            const double z0 = p[0];
            const double A = p[1];
            const double xc = p[2];
            const double yc = p[3];
            const double sx = std::max(1e-12, std::abs(p[4]));
            const double sy = std::max(1e-12, std::abs(p[5]));
            const double th = p[6];
            const double c = std::cos(th);
            const double s = std::sin(th);
            const double dx = px - xc;
            const double dy = py - yc;
            const double u = dx * c + dy * s;
            const double v = -dx * s + dy * c;
            const double expo = -0.5 * ((u * u) / (sx * sx) + (v * v) / (sy * sy));
            return z0 + A * std::exp(std::max(-700.0, expo));
        }

        case GraphStatsAndSurfaceFit::Lorentz2D: {
            const double z0 = p[0];
            const double A = p[1];
            const double xc = p[2];
            const double yc = p[3];
            const double wx = std::max(1e-12, std::abs(p[4]));
            const double wy = std::max(1e-12, std::abs(p[5]));
            const double th = p[6];
            const double c = std::cos(th);
            const double s = std::sin(th);
            const double dx = px - xc;
            const double dy = py - yc;
            const double u = dx * c + dy * s;
            const double v = -dx * s + dy * c;
            return z0 + A / (1.0 + (u * u) / (wx * wx) + (v * v) / (wy * wy));
        }

        case GraphStatsAndSurfaceFit::PseudoVoigt2D: {
            const double z0 = p[0];
            const double A = p[1];
            const double xc = p[2];
            const double yc = p[3];
            const double sx = std::max(1e-12, std::abs(p[4]));
            const double sy = std::max(1e-12, std::abs(p[5]));
            const double mu = std::clamp(p[6], 0.0, 1.0);
            const double dx = px - xc;
            const double dy = py - yc;
            const double r2 = (dx * dx) / (sx * sx) + (dy * dy) / (sy * sy);
            const double lor = 1.0 / (1.0 + r2);
            const double gau = std::exp(std::max(-700.0, -0.5 * r2));
            return z0 + A * (mu * lor + (1.0 - mu) * gau);
        }

        case GraphStatsAndSurfaceFit::Exponential2D: {
            const double z0 = p[0];
            const double A = p[1];
            const double bx = p[2];
            const double by = p[3];
            const double arg = std::clamp(bx * px + by * py, -500.0, 500.0);
            return z0 + A * std::exp(arg);
        }

        case GraphStatsAndSurfaceFit::Chebyshev2D: {
            const double spanX = (xMax > xMin) ? (xMax - xMin) : 1.0;
            const double spanY = (yMax > yMin) ? (yMax - yMin) : 1.0;
            const double tx = 2.0 * (px - xMin) / spanX - 1.0;
            const double ty = 2.0 * (py - yMin) / spanY - 1.0;
            const double T1x = tx;
            const double T2x = 2.0 * tx * tx - 1.0;
            const double T1y = ty;
            const double T2y = 2.0 * ty * ty - 1.0;
            return p[0] + p[1] * T1x + p[2] * T1y + p[3] * T2x + p[4] * T1x * T1y + p[5] * T2y;
        }

        case GraphStatsAndSurfaceFit::Custom2D: {
            if (!ensureParser())
                return 0.0;
            varX = px;
            varY = py;
            for (size_t j = 0; j < varParams.size(); ++j)
                varParams[j] = p[j];
            try {
                double val = parser->Eval();
                return std::isfinite(val) ? val : 0.0;
            } catch (...) {
                return 0.0;
            }
        }
        }
        return 0.0;
    }
};

int surface_gsl_f(const gsl_vector *parVec, void *dataPtr, gsl_vector *fVec)
{
    const auto *ctx = static_cast<const SurfaceEvalContext *>(dataPtr);
    const size_t n = ctx->z.size();
    const size_t p = static_cast<size_t>(ctx->paramNames.size());
    std::vector<double> par(p, 0.0);
    for (size_t j = 0; j < p; ++j)
        par[j] = gsl_vector_get(parVec, j);

    for (size_t i = 0; i < n; ++i) {
        double zHat = ctx->evalPoint(ctx->x[i], ctx->y[i], par.data());
        if (!std::isfinite(zHat))
            return GSL_EDOM;
        gsl_vector_set(fVec, i, zHat - ctx->z[i]);
    }
    return GSL_SUCCESS;
}

int surface_gsl_df(const gsl_vector *parVec, void *dataPtr, gsl_matrix *J)
{
    const auto *ctx = static_cast<const SurfaceEvalContext *>(dataPtr);
    const size_t n = ctx->z.size();
    const size_t p = static_cast<size_t>(ctx->paramNames.size());
    std::vector<double> par(p, 0.0);
    for (size_t j = 0; j < p; ++j)
        par[j] = gsl_vector_get(parVec, j);

    for (size_t j = 0; j < p; ++j) {
        const double pj = par[j];
        const double h = std::max(1e-6, 1e-6 * std::abs(pj));
        par[j] = pj + h;
        std::vector<double> fPlus(n);
        for (size_t i = 0; i < n; ++i)
            fPlus[i] = ctx->evalPoint(ctx->x[i], ctx->y[i], par.data());

        par[j] = pj - h;
        for (size_t i = 0; i < n; ++i) {
            double fMinus = ctx->evalPoint(ctx->x[i], ctx->y[i], par.data());
            double deriv = (fPlus[i] - fMinus) / (2.0 * h);
            if (!std::isfinite(deriv))
                deriv = 0.0;
            gsl_matrix_set(J, i, j, deriv);
        }
        par[j] = pj;
    }
    return GSL_SUCCESS;
}

int surface_gsl_fdf(const gsl_vector *parVec, void *dataPtr, gsl_vector *fVec, gsl_matrix *J)
{
    int s1 = surface_gsl_f(parVec, dataPtr, fVec);
    if (s1 != GSL_SUCCESS)
        return s1;
    return surface_gsl_df(parVec, dataPtr, J);
}

QStringList extractCustom2DParameters(const QString &expr)
{
    QSet<QString> reserved = {
        QStringLiteral("x"),         QStringLiteral("y"),         QStringLiteral("pi"),
        QStringLiteral("Pi"),        QStringLiteral("PI"),        QStringLiteral("e"),
        QStringLiteral("abs"),       QStringLiteral("acos"),      QStringLiteral("acosh"),
        QStringLiteral("asin"),      QStringLiteral("asinh"),     QStringLiteral("atan"),
        QStringLiteral("atan2"),     QStringLiteral("atanh"),     QStringLiteral("avg"),
        QStringLiteral("bessel_j0"), QStringLiteral("bessel_j1"), QStringLiteral("bessel_jn"),
        QStringLiteral("bessel_y0"), QStringLiteral("bessel_y1"), QStringLiteral("bessel_yn"),
        QStringLiteral("beta"),      QStringLiteral("cos"),       QStringLiteral("cosh"),
        QStringLiteral("erf"),       QStringLiteral("erfc"),      QStringLiteral("erfz"),
        QStringLiteral("erfq"),      QStringLiteral("exp"),       QStringLiteral("gamma"),
        QStringLiteral("gammaln"),   QStringLiteral("hazard"),    QStringLiteral("ln"),
        QStringLiteral("log"),       QStringLiteral("log2"),      QStringLiteral("log10"),
        QStringLiteral("min"),       QStringLiteral("max"),       QStringLiteral("rint"),
        QStringLiteral("sign"),      QStringLiteral("sin"),       QStringLiteral("sinh"),
        QStringLiteral("sqrt"),      QStringLiteral("sum"),       QStringLiteral("tan"),
        QStringLiteral("tanh"),      QStringLiteral("pow")
    };

    QStringList params;
    QRegularExpression identRe(QStringLiteral("\\b([A-Za-z_][A-Za-z0-9_]*)\\b"));
    auto it = identRe.globalMatch(expr);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        QString tok = m.captured(1);
        if (!reserved.contains(tok) && !params.contains(tok))
            params.append(tok);
    }
    return params;
}

bool solveLinearLeastSquares(const std::vector<std::vector<double>> &basisRows,
                             const std::vector<double> &z, std::vector<double> &coeffsOut)
{
    const size_t n = z.size();
    if (n == 0 || basisRows.size() != n)
        return false;
    const size_t p = basisRows[0].size();
    if (n < p || p == 0)
        return false;

    gsl_matrix *X = gsl_matrix_alloc(n, p);
    gsl_vector *y = gsl_vector_alloc(n);
    gsl_vector *c = gsl_vector_alloc(p);
    gsl_matrix *cov = gsl_matrix_alloc(p, p);
    gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(n, p);

    for (size_t i = 0; i < n; ++i) {
        gsl_vector_set(y, i, z[i]);
        for (size_t j = 0; j < p; ++j)
            gsl_matrix_set(X, i, j, basisRows[i][j]);
    }
    double chisq = 0.0;
    int status = gsl_multifit_linear(X, y, c, cov, &chisq, work);
    coeffsOut.resize(p);
    for (size_t j = 0; j < p; ++j)
        coeffsOut[j] = gsl_vector_get(c, j);

    gsl_multifit_linear_free(work);
    gsl_matrix_free(cov);
    gsl_vector_free(c);
    gsl_vector_free(y);
    gsl_matrix_free(X);
    return status == GSL_SUCCESS;
}

void buildInitialSurfaceParameters(SurfaceEvalContext &ctx, std::vector<double> &initOut)
{
    const size_t n = ctx.z.size();
    std::vector<double> sortedZ = ctx.z;
    std::sort(sortedZ.begin(), sortedZ.end());
    const double zMin = sortedZ.front();
    const double zMax = sortedZ.back();
    const double zMed = sortedZ[n / 2];
    const double zP10 = sortedZ[std::min(n - 1, n / 10)];
    const double zP90 = sortedZ[std::min(n - 1, (n * 9) / 10)];

    const double spanX = std::max(1e-6, ctx.xMax - ctx.xMin);
    const double spanY = std::max(1e-6, ctx.yMax - ctx.yMin);

    switch (ctx.model) {
    case GraphStatsAndSurfaceFit::Plane2D: {
        ctx.paramNames = QStringList{ QStringLiteral("z0"), QStringLiteral("a"),
                                      QStringLiteral("b") };
        std::vector<std::vector<double>> basis(n, std::vector<double>(3));
        for (size_t i = 0; i < n; ++i)
            basis[i] = { 1.0, ctx.x[i], ctx.y[i] };
        if (!solveLinearLeastSquares(basis, ctx.z, initOut))
            initOut = { zMed, 0.0, 0.0 };
        break;
    }
    case GraphStatsAndSurfaceFit::Poly2DQuadratic: {
        ctx.paramNames = QStringList{ QStringLiteral("z0"), QStringLiteral("a"),
                                      QStringLiteral("b"),  QStringLiteral("c"),
                                      QStringLiteral("d"),  QStringLiteral("e") };
        std::vector<std::vector<double>> basis(n, std::vector<double>(6));
        for (size_t i = 0; i < n; ++i) {
            double px = ctx.x[i], py = ctx.y[i];
            basis[i] = { 1.0, px, py, px * px, py * py, px * py };
        }
        if (!solveLinearLeastSquares(basis, ctx.z, initOut))
            initOut = { zMed, 0.0, 0.0, 0.0, 0.0, 0.0 };
        break;
    }
    case GraphStatsAndSurfaceFit::Gauss2D:
    case GraphStatsAndSurfaceFit::Lorentz2D:
    case GraphStatsAndSurfaceFit::PseudoVoigt2D: {
        if (ctx.model == GraphStatsAndSurfaceFit::Gauss2D) {
            ctx.paramNames = QStringList{ QStringLiteral("z0"),      QStringLiteral("A"),
                                          QStringLiteral("xc"),      QStringLiteral("yc"),
                                          QStringLiteral("sigma_x"), QStringLiteral("sigma_y"),
                                          QStringLiteral("theta") };
        } else if (ctx.model == GraphStatsAndSurfaceFit::Lorentz2D) {
            ctx.paramNames = QStringList{ QStringLiteral("z0"),    QStringLiteral("A"),
                                          QStringLiteral("xc"),    QStringLiteral("yc"),
                                          QStringLiteral("wx"),    QStringLiteral("wy"),
                                          QStringLiteral("theta") };
        } else {
            ctx.paramNames = QStringList{ QStringLiteral("z0"),      QStringLiteral("A"),
                                          QStringLiteral("xc"),      QStringLiteral("yc"),
                                          QStringLiteral("sigma_x"), QStringLiteral("sigma_y"),
                                          QStringLiteral("mu") };
        }

        const bool positivePeak = (zMax - zMed) >= (zMed - zMin);
        const double z0 = positivePeak ? zP10 : zP90;
        size_t extIdx = 0;
        for (size_t i = 1; i < n; ++i) {
            if (positivePeak ? (ctx.z[i] > ctx.z[extIdx]) : (ctx.z[i] < ctx.z[extIdx]))
                extIdx = i;
        }
        double A0 = ctx.z[extIdx] - z0;
        if (std::abs(A0) < 1e-12)
            A0 = positivePeak ? 1.0 : -1.0;

        // Weighted centroid & second moments above 20% height
        double sumW = 0.0, sumX = 0.0, sumY = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double rel = (ctx.z[i] - z0) / A0;
            if (rel > 0.5) {
                sumW += rel;
                sumX += rel * ctx.x[i];
                sumY += rel * ctx.y[i];
            }
        }
        double xc0 = (sumW > 0.0) ? (sumX / sumW) : ctx.x[extIdx];
        double yc0 = (sumW > 0.0) ? (sumY / sumW) : ctx.y[extIdx];

        double w2Sum = 0.0, varX = 0.0, varY = 0.0, covXY = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double rel = (ctx.z[i] - z0) / A0;
            if (rel > 0.2) {
                double dx = ctx.x[i] - xc0;
                double dy = ctx.y[i] - yc0;
                w2Sum += rel;
                varX += rel * dx * dx;
                varY += rel * dy * dy;
                covXY += rel * dx * dy;
            }
        }
        double sx0 = 0.18 * spanX;
        double sy0 = 0.18 * spanY;
        double theta0 = 0.0;
        if (w2Sum > 0.0) {
            varX /= w2Sum;
            varY /= w2Sum;
            covXY /= w2Sum;
            double corr = covXY / std::max(1e-12, std::sqrt(varX * varY));
            if (ctx.model != GraphStatsAndSurfaceFit::PseudoVoigt2D && std::abs(corr) > 0.20) {
                theta0 = 0.5 * std::atan2(2.0 * covXY, varX - varY);
                double c = std::cos(theta0), s = std::sin(theta0);
                double uVar = c * c * varX + 2.0 * c * s * covXY + s * s * varY;
                double vVar = s * s * varX - 2.0 * c * s * covXY + c * c * varY;
                sx0 = std::sqrt(std::max(1e-8, uVar / 0.45));
                sy0 = std::sqrt(std::max(1e-8, vVar / 0.45));
            } else {
                sx0 = std::sqrt(std::max(1e-8, varX / 0.45));
                sy0 = std::sqrt(std::max(1e-8, varY / 0.45));
            }
        }
        sx0 = std::clamp(sx0, 0.02 * spanX, 0.8 * spanX);
        sy0 = std::clamp(sy0, 0.02 * spanY, 0.8 * spanY);

        if (ctx.model == GraphStatsAndSurfaceFit::PseudoVoigt2D)
            initOut = { z0, A0, xc0, yc0, sx0, sy0, 0.5 };
        else
            initOut = { z0, A0, xc0, yc0, sx0, sy0, theta0 };
        break;
    }
    case GraphStatsAndSurfaceFit::Exponential2D: {
        ctx.paramNames = QStringList{ QStringLiteral("z0"), QStringLiteral("A"),
                                      QStringLiteral("bx"), QStringLiteral("by") };
        double z0 = zMin - 0.05 * std::max(1e-6, zMax - zMin);
        std::vector<std::vector<double>> basis;
        std::vector<double> logZ;
        for (size_t i = 0; i < n; ++i) {
            double shifted = ctx.z[i] - z0;
            if (shifted > 1e-12) {
                basis.push_back({ 1.0, ctx.x[i], ctx.y[i] });
                logZ.push_back(std::log(shifted));
            }
        }
        std::vector<double> linC;
        if (basis.size() >= 4 && solveLinearLeastSquares(basis, logZ, linC))
            initOut = { z0, std::exp(std::clamp(linC[0], -50.0, 50.0)), linC[1], linC[2] };
        else
            initOut = { zMin, std::max(1e-3, zMax - zMin), 0.1 / spanX, 0.1 / spanY };
        break;
    }
    case GraphStatsAndSurfaceFit::Chebyshev2D: {
        ctx.paramNames = QStringList{ QStringLiteral("c00"), QStringLiteral("c10"),
                                      QStringLiteral("c01"), QStringLiteral("c20"),
                                      QStringLiteral("c11"), QStringLiteral("c02") };
        std::vector<std::vector<double>> basis(n, std::vector<double>(6));
        for (size_t i = 0; i < n; ++i) {
            double tx = 2.0 * (ctx.x[i] - ctx.xMin) / spanX - 1.0;
            double ty = 2.0 * (ctx.y[i] - ctx.yMin) / spanY - 1.0;
            basis[i] = { 1.0, tx, ty, 2.0 * tx * tx - 1.0, tx * ty, 2.0 * ty * ty - 1.0 };
        }
        if (!solveLinearLeastSquares(basis, ctx.z, initOut))
            initOut = { zMed, 0.0, 0.0, 0.0, 0.0, 0.0 };
        break;
    }
    case GraphStatsAndSurfaceFit::Custom2D: {
        ctx.paramNames = extractCustom2DParameters(ctx.customExpr);
        if (ctx.paramNames.isEmpty())
            ctx.paramNames = QStringList{ QStringLiteral("a") };
        initOut.assign(static_cast<size_t>(ctx.paramNames.size()), 1.0);
        for (int j = 0; j < ctx.paramNames.size(); ++j) {
            const QString &nm = ctx.paramNames[j];
            if (nm.compare(QStringLiteral("z0"), Qt::CaseInsensitive) == 0)
                initOut[static_cast<size_t>(j)] = zP10;
            else if (nm.compare(QStringLiteral("A"), Qt::CaseInsensitive) == 0)
                initOut[static_cast<size_t>(j)] = std::max(1e-3, zMax - zP10);
            else if (nm.compare(QStringLiteral("xc"), Qt::CaseInsensitive) == 0)
                initOut[static_cast<size_t>(j)] = 0.5 * (ctx.xMin + ctx.xMax);
            else if (nm.compare(QStringLiteral("yc"), Qt::CaseInsensitive) == 0)
                initOut[static_cast<size_t>(j)] = 0.5 * (ctx.yMin + ctx.yMax);
        }
        break;
    }
    }
}

GraphStatsAndSurfaceFit::SurfaceFitResult
runSurfaceFitCore(ApplicationWindow *app, SurfaceEvalContext &ctx, const QString &baseName,
                  int outRows, int outCols)
{
    GraphStatsAndSurfaceFit::SurfaceFitResult out;
    if (!app || ctx.z.size() < 4)
        return out;

    std::vector<double> initParams;
    buildInitialSurfaceParameters(ctx, initParams);
    const size_t p = static_cast<size_t>(ctx.paramNames.size());
    const size_t n = ctx.z.size();
    if (p == 0 || n < p)
        return out;
    if (ctx.model == GraphStatsAndSurfaceFit::Custom2D && !ctx.ensureParser())
        return out;

    gsl_multifit_function_fdf fdf;
    fdf.f = &surface_gsl_f;
    fdf.df = &surface_gsl_df;
    fdf.fdf = &surface_gsl_fdf;
    fdf.n = n;
    fdf.p = p;
    fdf.params = &ctx;

    gsl_vector *xInit = gsl_vector_alloc(p);
    for (size_t j = 0; j < p; ++j)
        gsl_vector_set(xInit, j, initParams[j]);

    gsl_multifit_fdfsolver *solver =
            gsl_multifit_fdfsolver_alloc(gsl_multifit_fdfsolver_lmsder, n, p);
    int status = gsl_multifit_fdfsolver_set(solver, &fdf, xInit);
    int iter = 0;
    if (status == GSL_SUCCESS) {
        for (iter = 0; iter < 400; ++iter) {
            status = gsl_multifit_fdfsolver_iterate(solver);
            int deltaStatus = gsl_multifit_test_delta(solver->dx, solver->x, 1e-8, 1e-8);
            if (deltaStatus == GSL_SUCCESS) {
                status = GSL_SUCCESS;
                break;
            }
            if (status != GSL_SUCCESS && status != GSL_CONTINUE) {
                if (status == GSL_ENOPROG || status == GSL_ETOLF || status == GSL_ETOLX
                    || status == GSL_ETOLG)
                    status = GSL_SUCCESS;
                break;
            }
        }
        if (status == GSL_CONTINUE)
            status = GSL_SUCCESS;
    }

    std::vector<double> bestP(p, 0.0);
    for (size_t j = 0; j < p; ++j)
        bestP[j] = gsl_vector_get(solver->x, j);

    gsl_matrix *covar = gsl_matrix_alloc(p, p);
#if GSL_MAJOR_VERSION < 2
    gsl_multifit_covar(solver->J, 1e-8, covar);
#else
    {
        gsl_matrix *J = gsl_matrix_alloc(n, p);
        gsl_multifit_fdfsolver_jac(solver, J);
        gsl_multifit_covar(J, 1e-8, covar);
        gsl_matrix_free(J);
    }
#endif

    double rss = 0.0;
    const double meanZ = std::accumulate(ctx.z.begin(), ctx.z.end(), 0.0) / static_cast<double>(n);
    double tss = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double zHat = ctx.evalPoint(ctx.x[i], ctx.y[i], bestP.data());
        double r = ctx.z[i] - zHat;
        rss += r * r;
        double dz = ctx.z[i] - meanZ;
        tss += dz * dz;
    }

    const double dof = (n > p) ? static_cast<double>(n - p) : 1.0;
    out.reducedChi2 = rss / dof;
    out.rSquare = (tss > 1e-30) ? std::max(0.0, 1.0 - rss / tss) : 1.0;
    out.adjRSquare = (n > p && tss > 1e-30)
            ? (1.0 - (1.0 - out.rSquare) * static_cast<double>(n - 1) / dof)
            : out.rSquare;
    out.converged = (status == GSL_SUCCESS) || (out.rSquare > 0.99);
    out.paramNames = ctx.paramNames;
    out.params.resize(static_cast<int>(p));
    out.errors.resize(static_cast<int>(p));

    for (size_t j = 0; j < p; ++j) {
        out.params[static_cast<int>(j)] = bestP[j];
        double cjj = gsl_matrix_get(covar, j, j);
        double se = (std::isfinite(cjj) && cjj > 0.0) ? std::sqrt(out.reducedChi2 * cjj) : 0.0;
        out.errors[static_cast<int>(j)] = std::isfinite(se) ? se : 0.0;
    }

    // Canonicalize positive widths and rotation angle in [-pi/4, pi/4] for Gauss2D / Lorentz2D
    if (ctx.model == GraphStatsAndSurfaceFit::Gauss2D
        || ctx.model == GraphStatsAndSurfaceFit::Lorentz2D) {
        out.params[4] = std::abs(out.params[4]);
        out.params[5] = std::abs(out.params[5]);
        double th = std::fmod(out.params[6], kPi);
        if (th > 0.5 * kPi)
            th -= kPi;
        else if (th <= -0.5 * kPi)
            th += kPi;
        if (th > 0.25 * kPi) {
            th -= 0.5 * kPi;
            std::swap(out.params[4], out.params[5]);
            std::swap(out.errors[4], out.errors[5]);
        } else if (th < -0.25 * kPi) {
            th += 0.5 * kPi;
            std::swap(out.params[4], out.params[5]);
            std::swap(out.errors[4], out.errors[5]);
        }
        if (std::abs(out.params[4] - out.params[5]) < 1e-9 * std::max(1.0, out.params[4]))
            th = 0.0;
        out.params[6] = th;
        for (size_t j = 0; j < p; ++j)
            bestP[j] = out.params[static_cast<int>(j)];
    } else if (ctx.model == GraphStatsAndSurfaceFit::PseudoVoigt2D) {
        out.params[4] = std::abs(out.params[4]);
        out.params[5] = std::abs(out.params[5]);
        out.params[6] = std::clamp(out.params[6], 0.0, 1.0);
        for (size_t j = 0; j < p; ++j)
            bestP[j] = out.params[static_cast<int>(j)];
    }

    gsl_matrix_free(covar);
    gsl_multifit_fdfsolver_free(solver);
    gsl_vector_free(xInit);

    // Create fitted Matrix
    const int nRows = std::max(2, outRows);
    const int nCols = std::max(2, outCols);
    QString matName = app->generateUniqueName(baseName + QStringLiteral("_SurfaceFit"), false);
    Matrix *fm = app->newMatrix(matName, nRows, nCols);
    if (fm) {
        fm->setCoordinates(ctx.xMin, ctx.xMax, ctx.yMin, ctx.yMax);
        for (int r = 0; r < nRows; ++r) {
            double py = ctx.yMin
                    + (nRows > 1 ? r * (ctx.yMax - ctx.yMin) / static_cast<double>(nRows - 1)
                                 : 0.0);
            for (int c = 0; c < nCols; ++c) {
                double px = ctx.xMin
                        + (nCols > 1 ? c * (ctx.xMax - ctx.xMin) / static_cast<double>(nCols - 1)
                                     : 0.0);
                fm->setCell(r, c, ctx.evalPoint(px, py, bestP.data()));
            }
        }
        out.fittedMatrix = fm;
    }

    // Create Parameter Summary Table
    const int tblRows = static_cast<int>(p) + 3;
    QString tblName = app->generateUniqueName(baseName + QStringLiteral("_SurfParams"), false);
    Table *pt = app->newTable(tblRows, 3, tblName, QObject::tr("2D Surface Fit Parameters"));
    if (pt) {
        pt->setHeader(QStringList() << QObject::tr("Parameter") << QObject::tr("Value")
                                    << QObject::tr("StdError"));
        pt->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        pt->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        pt->column(2)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        pt->column(2)->setPlotDesignation(SciDAVis::yErr);

        for (int j = 0; j < static_cast<int>(p); ++j) {
            pt->column(0)->setTextAt(j, out.paramNames[j]);
            pt->column(1)->setValueAt(j, out.params[j]);
            pt->column(2)->setValueAt(j, out.errors[j]);
        }
        int baseRow = static_cast<int>(p);
        pt->column(0)->setTextAt(baseRow, QStringLiteral("R^2"));
        pt->column(1)->setValueAt(baseRow, out.rSquare);
        pt->column(2)->setValueAt(baseRow, 0.0);

        pt->column(0)->setTextAt(baseRow + 1, QStringLiteral("Adj_R^2"));
        pt->column(1)->setValueAt(baseRow + 1, out.adjRSquare);
        pt->column(2)->setValueAt(baseRow + 1, 0.0);

        pt->column(0)->setTextAt(baseRow + 2, QStringLiteral("Reduced_Chi^2"));
        pt->column(1)->setValueAt(baseRow + 2, out.reducedChi2);
        pt->column(2)->setValueAt(baseRow + 2, 0.0);
        out.paramTable = pt;
    }

    if (app->writeFitResultsToLog) {
        QString log = QStringLiteral("[3D Surface Fit: ") + baseName
                + QStringLiteral("] R^2 = ") + QString::number(out.rSquare, 'f', 6)
                + QStringLiteral(", Adj. R^2 = ") + QString::number(out.adjRSquare, 'f', 6)
                + QStringLiteral(", Red. Chi^2 = ") + QString::number(out.reducedChi2, 'g', 6)
                + QStringLiteral("\n");
        for (int j = 0; j < out.paramNames.size(); ++j) {
            log += QStringLiteral("  %1 = %2 +/- %3\n")
                           .arg(out.paramNames[j])
                           .arg(out.params[j], 0, 'g', 8)
                           .arg(out.errors[j], 0, 'g', 6);
        }
        app->updateLog(log);
    }

    return out;
}

QString starRatingForP(double p)
{
    if (p < 0.001)
        return QStringLiteral("***");
    if (p < 0.01)
        return QStringLiteral("**");
    if (p < 0.05)
        return QStringLiteral("*");
    return QStringLiteral("ns");
}

} // namespace

// ============================================================================
// Public Implementation of GraphStatsAndSurfaceFit
// ============================================================================

GraphStatsAndSurfaceFit::SurfaceFitResult GraphStatsAndSurfaceFit::fitSurfaceXYZ(
        ApplicationWindow *app, Table *srcTable, const QString &xCol, const QString &yCol,
        const QString &zCol, SurfaceModel model, const QString &customExpr, int gridSide)
{
    SurfaceFitResult empty;
    if (!app || !srcTable)
        return empty;

    Column *cx = resolveTableColumn(srcTable, xCol);
    Column *cy = resolveTableColumn(srcTable, yCol);
    Column *cz = resolveTableColumn(srcTable, zCol);
    if (!cx || !cy || !cz)
        return empty;

    const int maxRows = std::min({ cx->rowCount(), cy->rowCount(), cz->rowCount() });
    SurfaceEvalContext ctx;
    ctx.model = model;
    ctx.customExpr = customExpr;
    ctx.xMin = std::numeric_limits<double>::max();
    ctx.xMax = -std::numeric_limits<double>::max();
    ctx.yMin = std::numeric_limits<double>::max();
    ctx.yMax = -std::numeric_limits<double>::max();

    for (int r = 0; r < maxRows; ++r) {
        if (cx->isInvalid(r) || cy->isInvalid(r) || cz->isInvalid(r) || cx->isMasked(r)
            || cy->isMasked(r) || cz->isMasked(r))
            continue;
        double vx = cx->valueAt(r);
        double vy = cy->valueAt(r);
        double vz = cz->valueAt(r);
        if (!std::isfinite(vx) || !std::isfinite(vy) || !std::isfinite(vz))
            continue;
        ctx.x.push_back(vx);
        ctx.y.push_back(vy);
        ctx.z.push_back(vz);
        ctx.xMin = std::min(ctx.xMin, vx);
        ctx.xMax = std::max(ctx.xMax, vx);
        ctx.yMin = std::min(ctx.yMin, vy);
        ctx.yMax = std::max(ctx.yMax, vy);
    }

    if (ctx.z.size() < 4)
        return empty;

    const int side = std::clamp(gridSide, 4, 512);
    return runSurfaceFitCore(app, ctx, srcTable->name(), side, side);
}

GraphStatsAndSurfaceFit::SurfaceFitResult GraphStatsAndSurfaceFit::fitSurfaceMatrix(
        ApplicationWindow *app, Matrix *srcMatrix, SurfaceModel model, const QString &customExpr)
{
    SurfaceFitResult empty;
    if (!app || !srcMatrix)
        return empty;

    const int rows = srcMatrix->numRows();
    const int cols = srcMatrix->numCols();
    if (rows < 2 || cols < 2)
        return empty;

    SurfaceEvalContext ctx;
    ctx.model = model;
    ctx.customExpr = customExpr;
    ctx.xMin = srcMatrix->xStart();
    ctx.xMax = srcMatrix->xEnd();
    ctx.yMin = srcMatrix->yStart();
    ctx.yMax = srcMatrix->yEnd();

    ctx.x.reserve(static_cast<size_t>(rows * cols));
    ctx.y.reserve(static_cast<size_t>(rows * cols));
    ctx.z.reserve(static_cast<size_t>(rows * cols));

    for (int r = 0; r < rows; ++r) {
        const double py = ctx.yMin
                + (rows > 1 ? r * (ctx.yMax - ctx.yMin) / static_cast<double>(rows - 1) : 0.0);
        for (int c = 0; c < cols; ++c) {
            const double px = ctx.xMin
                    + (cols > 1 ? c * (ctx.xMax - ctx.xMin) / static_cast<double>(cols - 1) : 0.0);
            const double pz = srcMatrix->cell(r, c);
            if (!std::isfinite(pz))
                continue;
            ctx.x.push_back(px);
            ctx.y.push_back(py);
            ctx.z.push_back(pz);
        }
    }

    if (ctx.z.size() < 4)
        return empty;

    return runSurfaceFitCore(app, ctx, srcMatrix->name(), rows, cols);
}

Table *GraphStatsAndSurfaceFit::analyze2DPeaks(ApplicationWindow *app, Matrix *srcMatrix,
                                               int maxPeaks, double prominenceFrac,
                                               Graph *overlayGraph)
{
    if (!app || !srcMatrix)
        return nullptr;

    const int R = srcMatrix->numRows();
    const int C = srcMatrix->numCols();
    if (R < 3 || C < 3)
        return nullptr;

    const double x0 = srcMatrix->xStart();
    const double x1 = srcMatrix->xEnd();
    const double y0 = srcMatrix->yStart();
    const double y1 = srcMatrix->yEnd();
    const double dxStep = (C > 1) ? (x1 - x0) / static_cast<double>(C - 1) : 1.0;
    const double dyStep = (R > 1) ? (y1 - y0) / static_cast<double>(R - 1) : 1.0;

    std::vector<double> rawZ(static_cast<size_t>(R * C), 0.0);
    std::vector<double> allValid;
    allValid.reserve(static_cast<size_t>(R * C));

    for (int r = 0; r < R; ++r) {
        for (int c = 0; c < C; ++c) {
            double v = srcMatrix->cell(r, c);
            if (!std::isfinite(v))
                v = 0.0;
            rawZ[static_cast<size_t>(r * C + c)] = v;
            allValid.push_back(v);
        }
    }

    // Fit planar background on the lowest 30% of pixels so bright peaks don't bias the plane
    std::vector<double> sortedValid = allValid;
    std::sort(sortedValid.begin(), sortedValid.end());
    const double bgCutoff =
            sortedValid[std::min(sortedValid.size() - 1, (sortedValid.size() * 3) / 10)];

    std::vector<std::vector<double>> bgBasis;
    std::vector<double> bgZ;
    for (int r = 0; r < R; ++r) {
        const double py = y0 + r * dyStep;
        for (int c = 0; c < C; ++c) {
            const double v = rawZ[static_cast<size_t>(r * C + c)];
            if (v <= bgCutoff || r == 0 || r == R - 1 || c == 0 || c == C - 1) {
                if (v <= sortedValid[std::min(sortedValid.size() - 1, (sortedValid.size() * 6) / 10)]) {
                    bgBasis.push_back({ 1.0, x0 + c * dxStep, py });
                    bgZ.push_back(v);
                }
            }
        }
    }
    std::vector<double> planeC = { sortedValid.front(), 0.0, 0.0 };
    if (bgBasis.size() >= 6)
        solveLinearLeastSquares(bgBasis, bgZ, planeC);

    std::vector<double> resid(static_cast<size_t>(R * C), 0.0);
    double rMin = std::numeric_limits<double>::max();
    double rMax = -std::numeric_limits<double>::max();
    for (int r = 0; r < R; ++r) {
        const double py = y0 + r * dyStep;
        for (int c = 0; c < C; ++c) {
            const double px = x0 + c * dxStep;
            const double bg = planeC[0] + planeC[1] * px + planeC[2] * py;
            const double rv = rawZ[static_cast<size_t>(r * C + c)] - bg;
            resid[static_cast<size_t>(r * C + c)] = rv;
            rMin = std::min(rMin, rv);
            rMax = std::max(rMax, rv);
        }
    }

    const double zSpan = std::max(1e-12, rMax - std::max(0.0, rMin));
    const double thresh = std::max(0.0, prominenceFrac) * zSpan;

    struct CandPeak
    {
        int r = 0;
        int c = 0;
        double h = 0.0;
    };
    std::vector<CandPeak> cands;
    for (int r = 1; r < R - 1; ++r) {
        for (int c = 1; c < C - 1; ++c) {
            const double center = resid[static_cast<size_t>(r * C + c)];
            if (center <= thresh)
                continue;
            bool isMax = true;
            bool strictlyGreater = false;
            for (int dr = -1; dr <= 1 && isMax; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (dr == 0 && dc == 0)
                        continue;
                    double nb = resid[static_cast<size_t>((r + dr) * C + (c + dc))];
                    if (nb > center) {
                        isMax = false;
                        break;
                    }
                    if (center > nb)
                        strictlyGreater = true;
                }
            }
            if (isMax && strictlyGreater)
                cands.push_back({ r, c, center });
        }
    }

    std::sort(cands.begin(), cands.end(),
              [](const CandPeak &a, const CandPeak &b) { return a.h > b.h; });

    // Non-maximum suppression
    const int minSep = std::max(2, std::min(R, C) / 14);
    std::vector<CandPeak> selected;
    const int capPeaks = std::max(1, maxPeaks);
    for (const CandPeak &cp : cands) {
        bool tooClose = false;
        for (const CandPeak &kept : selected) {
            int dr = cp.r - kept.r;
            int dc = cp.c - kept.c;
            if (dr * dr + dc * dc < minSep * minSep) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose) {
            selected.push_back(cp);
            if (static_cast<int>(selected.size()) >= capPeaks)
                break;
        }
    }

    struct Peak2DRecord
    {
        double xc = 0.0;
        double yc = 0.0;
        double amp = 0.0;
        double sx = 0.0;
        double sy = 0.0;
        double volume = 0.0;
        double bg = 0.0;
    };

    std::vector<Peak2DRecord> refined;
    const int hw = std::clamp(std::min(R, C) / 5, 2, 7);

    for (const CandPeak &pk : selected) {
        SurfaceEvalContext patch;
        patch.model = Gauss2D;
        int rStart = std::max(0, pk.r - hw);
        int rEnd = std::min(R - 1, pk.r + hw);
        int cStart = std::max(0, pk.c - hw);
        int cEnd = std::min(C - 1, pk.c + hw);

        patch.xMin = x0 + cStart * dxStep;
        patch.xMax = x0 + cEnd * dxStep;
        patch.yMin = y0 + rStart * dyStep;
        patch.yMax = y0 + rEnd * dyStep;

        for (int rr = rStart; rr <= rEnd; ++rr) {
            double py = y0 + rr * dyStep;
            for (int cc = cStart; cc <= cEnd; ++cc) {
                double px = x0 + cc * dxStep;
                patch.x.push_back(px);
                patch.y.push_back(py);
                patch.z.push_back(std::max(0.0, resid[static_cast<size_t>(rr * C + cc)]));
            }
        }

        double xcGuess = x0 + pk.c * dxStep;
        double ycGuess = y0 + pk.r * dyStep;
        double sxGuess = std::max(1e-6, 1.5 * std::abs(dxStep));
        double syGuess = std::max(1e-6, 1.5 * std::abs(dyStep));
        double ampGuess = pk.h;

        patch.paramNames = QStringList{ QStringLiteral("z0"),      QStringLiteral("A"),
                                        QStringLiteral("xc"),      QStringLiteral("yc"),
                                        QStringLiteral("sigma_x"), QStringLiteral("sigma_y"),
                                        QStringLiteral("theta") };

        if (patch.z.size() >= 9) {
            gsl_multifit_function_fdf fdf;
            fdf.f = &surface_gsl_f;
            fdf.df = &surface_gsl_df;
            fdf.fdf = &surface_gsl_fdf;
            fdf.n = patch.z.size();
            fdf.p = 7;
            fdf.params = &patch;

            gsl_vector *pInit = gsl_vector_alloc(7);
            gsl_vector_set(pInit, 0, 0.0);
            gsl_vector_set(pInit, 1, ampGuess);
            gsl_vector_set(pInit, 2, xcGuess);
            gsl_vector_set(pInit, 3, ycGuess);
            gsl_vector_set(pInit, 4, sxGuess);
            gsl_vector_set(pInit, 5, syGuess);
            gsl_vector_set(pInit, 6, 0.0);

            gsl_multifit_fdfsolver *s =
                    gsl_multifit_fdfsolver_alloc(gsl_multifit_fdfsolver_lmsder, patch.z.size(), 7);
            if (gsl_multifit_fdfsolver_set(s, &fdf, pInit) == GSL_SUCCESS) {
                for (int it = 0; it < 120; ++it) {
                    int st = gsl_multifit_fdfsolver_iterate(s);
                    if (gsl_multifit_test_delta(s->dx, s->x, 1e-6, 1e-6) == GSL_SUCCESS
                        || (st != GSL_SUCCESS && st != GSL_CONTINUE))
                        break;
                }
                double fitA = gsl_vector_get(s->x, 1);
                double fitXc = gsl_vector_get(s->x, 2);
                double fitYc = gsl_vector_get(s->x, 3);
                double fitSx = std::abs(gsl_vector_get(s->x, 4));
                double fitSy = std::abs(gsl_vector_get(s->x, 5));
                double fitTh = std::fmod(gsl_vector_get(s->x, 6), kPi);
                if (fitTh > 0.5 * kPi)
                    fitTh -= kPi;
                else if (fitTh <= -0.5 * kPi)
                    fitTh += kPi;
                if (std::abs(fitTh) > 0.25 * kPi)
                    std::swap(fitSx, fitSy);

                const double loX = std::min(patch.xMin, patch.xMax);
                const double hiX = std::max(patch.xMin, patch.xMax);
                const double loY = std::min(patch.yMin, patch.yMax);
                const double hiY = std::max(patch.yMin, patch.yMax);
                if (fitA > 0.0 && fitXc >= loX && fitXc <= hiX && fitYc >= loY && fitYc <= hiY
                    && fitSx > 0.0 && fitSy > 0.0) {
                    ampGuess = fitA;
                    xcGuess = fitXc;
                    ycGuess = fitYc;
                    sxGuess = fitSx;
                    syGuess = fitSy;
                }
            }
            gsl_multifit_fdfsolver_free(s);
            gsl_vector_free(pInit);
        }

        double bgAtPeak = planeC[0] + planeC[1] * xcGuess + planeC[2] * ycGuess;
        double vol = 2.0 * kPi * ampGuess * sxGuess * syGuess;
        refined.push_back({ xcGuess, ycGuess, ampGuess, sxGuess, syGuess, vol, bgAtPeak });
    }

    const int nOut = std::max(1, static_cast<int>(refined.size()));
    QString tblName = app->generateUniqueName(srcMatrix->name() + QStringLiteral("_2DPeaks"), false);
    Table *pt = app->newTable(nOut, 9, tblName, QObject::tr("2D Peak Summary"));
    if (!pt)
        return nullptr;

    pt->setHeader(QStringList() << QObject::tr("Peak") << QObject::tr("X_Center")
                                << QObject::tr("Y_Center") << QObject::tr("Amplitude")
                                << QObject::tr("Sigma_X") << QObject::tr("Sigma_Y")
                                << QObject::tr("FWHM_X") << QObject::tr("FWHM_Y")
                                << QObject::tr("Volume"));
    for (int col = 0; col < 9; ++col) {
        pt->column(col)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        pt->column(col)->setPlotDesignation(col == 1 ? SciDAVis::X : SciDAVis::Y);
    }

    if (refined.empty()) {
        for (int col = 0; col < 9; ++col)
            pt->column(col)->setInvalid(0, true);
    } else {
        for (int i = 0; i < static_cast<int>(refined.size()); ++i) {
            const Peak2DRecord &rec = refined[static_cast<size_t>(i)];
            pt->column(0)->setValueAt(i, static_cast<double>(i + 1));
            pt->column(1)->setValueAt(i, rec.xc);
            pt->column(2)->setValueAt(i, rec.yc);
            pt->column(3)->setValueAt(i, rec.amp);
            pt->column(4)->setValueAt(i, rec.sx);
            pt->column(5)->setValueAt(i, rec.sy);
            pt->column(6)->setValueAt(i, kFwhmFactor * rec.sx);
            pt->column(7)->setValueAt(i, kFwhmFactor * rec.sy);
            pt->column(8)->setValueAt(i, rec.volume);
        }
    }

    if (overlayGraph && !refined.empty()) {
        if (overlayGraph->insertCurve(pt, pt->colName(1), pt->colName(2), Graph::Scatter)) {
            int idx = overlayGraph->curves() - 1;
            QwtSymbol sym(QwtSymbol::Cross, QBrush(Qt::NoBrush), QPen(QColor(220, 30, 30), 2),
                          QSize(11, 11));
            overlayGraph->setCurveSymbol(idx, sym);
        }
        overlayGraph->replot();
        for (int i = 0; i < static_cast<int>(refined.size()); ++i) {
            const Peak2DRecord &rec = refined[static_cast<size_t>(i)];
            addTextLabelOnGraph(overlayGraph, rec.xc, rec.yc,
                                QStringLiteral("P%1").arg(i + 1), QColor(220, 30, 30), true, -1);
        }
        overlayGraph->replot();
    }

    return pt;
}

QwtPlotCurve *GraphStatsAndSurfaceFit::addConfidenceEllipse(ApplicationWindow *app, Graph *g,
                                                            const QString &curveTitle,
                                                            double confLevel,
                                                            bool predictionEllipse)
{
    if (!app || !g)
        return nullptr;

    QwtPlotCurve *src = resolveCurveOnGraph(g, curveTitle);
    if (!src || src->dataSize() < 3)
        return nullptr;

    DataCurve *dc = dynamic_cast<DataCurve *>(src);
    Column *xSrcCol = (dc && dc->table()) ? resolveTableColumn(dc->table(), dc->xColumnName())
                                          : nullptr;
    Column *ySrcCol = (dc && dc->table()) ? resolveTableColumn(dc->table(), dc->yColumnName())
                                          : nullptr;

    std::vector<double> xs, ys;
    xs.reserve(static_cast<size_t>(src->dataSize()));
    ys.reserve(static_cast<size_t>(src->dataSize()));
    for (int i = 0; i < src->dataSize(); ++i) {
        if (dc) {
            int row = dc->tableRow(i);
            if (row >= 0) {
                if ((xSrcCol && xSrcCol->isMasked(row)) || (ySrcCol && ySrcCol->isMasked(row)))
                    continue;
            }
        }
        double px = src->x(i);
        double py = src->y(i);
        if (std::isfinite(px) && std::isfinite(py)) {
            xs.push_back(px);
            ys.push_back(py);
        }
    }

    const size_t n = xs.size();
    if (n < 3)
        return nullptr;

    const double cl = std::clamp(confLevel, 0.01, 0.9999);
    const double meanX = std::accumulate(xs.begin(), xs.end(), 0.0) / static_cast<double>(n);
    const double meanY = std::accumulate(ys.begin(), ys.end(), 0.0) / static_cast<double>(n);

    double sxx = 0.0, syy = 0.0, sxy = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double dx = xs[i] - meanX;
        double dy = ys[i] - meanY;
        sxx += dx * dx;
        syy += dy * dy;
        sxy += dx * dy;
    }
    const double invDf = 1.0 / static_cast<double>(n - 1);
    sxx *= invDf;
    syy *= invDf;
    sxy *= invDf;

    // Exact 2x2 symmetric covariance eigendecomposition
    const double trace = sxx + syy;
    const double diff = sxx - syy;
    const double disc = std::sqrt(std::max(0.0, 0.25 * diff * diff + sxy * sxy));
    const double lambda1 = std::max(0.0, 0.5 * trace + disc);
    const double lambda2 = std::max(0.0, 0.5 * trace - disc);
    const double phi = 0.5 * std::atan2(2.0 * sxy, diff);

    // Exact Hotelling T^2 / F_{2, n-2} scaling
    const double dn = static_cast<double>(n);
    const double fCrit = gsl_cdf_fdist_Pinv(cl, 2.0, dn - 2.0);
    const double scale = predictionEllipse
            ? std::sqrt(2.0 * (dn - 1.0) * (dn + 1.0) / (dn * (dn - 2.0)) * fCrit)
            : std::sqrt(2.0 * (dn - 1.0) / (dn * (dn - 2.0)) * fCrit);

    const double semiA = scale * std::sqrt(lambda1);
    const double semiB = scale * std::sqrt(lambda2);
    const double cosPhi = std::cos(phi);
    const double sinPhi = std::sin(phi);

    constexpr int kPts = 121;
    std::vector<double> ex(kPts), ey(kPts);
    Column *colX = new Column(QStringLiteral("X"), SciDAVis::ColumnMode::Numeric);
    QString tag = QStringLiteral("%1%_%2")
                          .arg(static_cast<int>(std::round(cl * 100.0)))
                          .arg(predictionEllipse ? QStringLiteral("PredEllipse")
                                                 : QStringLiteral("ConfEllipse"));
    Column *colY = new Column(tag, SciDAVis::ColumnMode::Numeric);
    colX->setPlotDesignation(SciDAVis::X);
    colY->setPlotDesignation(SciDAVis::Y);

    for (int k = 0; k < kPts; ++k) {
        double theta = (2.0 * kPi * static_cast<double>(k)) / static_cast<double>(kPts - 1);
        double u = semiA * std::cos(theta);
        double v = semiB * std::sin(theta);
        ex[static_cast<size_t>(k)] = meanX + u * cosPhi - v * sinPhi;
        ey[static_cast<size_t>(k)] = meanY + u * sinPhi + v * cosPhi;
        colX->setValueAt(k, ex[static_cast<size_t>(k)]);
        colY->setValueAt(k, ey[static_cast<size_t>(k)]);
    }

    QString tblName = app->generateUniqueName(QStringLiteral("Ellipse"));
    Table *backing = app->newHiddenTable(tblName,
                                         tag + QStringLiteral(" of ") + src->title().text(),
                                         QList<Column *>() << colX << colY);
    if (!backing)
        return nullptr;

    DataCurve *ellipseCurve = new DataCurve(backing, tblName + "_" + colX->name(),
                                            tblName + "_" + colY->name());
    ellipseCurve->setData(ex.data(), ey.data(), kPts);
    QColor c = predictionEllipse ? QColor(30, 110, 200) : QColor(210, 40, 40);
    ellipseCurve->setPen(QPen(c, 2, predictionEllipse ? Qt::DashLine : Qt::SolidLine));
    QColor fillC = c;
    fillC.setAlpha(28);
    ellipseCurve->setBrush(QBrush(fillC));
    g->insertPlotItem(ellipseCurve, Graph::Line);
    g->addFitCurve(ellipseCurve);
    g->updatePlot();

    return ellipseCurve;
}

Table *GraphStatsAndSurfaceFit::runGroupComparisonAndAnnotate(ApplicationWindow *app,
                                                              Table *srcTable,
                                                              const QStringList &colNames,
                                                              Graph *annotateGraph)
{
    if (!app || !srcTable || colNames.size() < 2)
        return nullptr;

    struct GroupData
    {
        QString label;
        std::vector<double> vals;
        double mean = 0.0;
        double var = 0.0;
        double minV = 0.0;
        double maxV = 0.0;
    };

    std::vector<GroupData> groups;
    for (const QString &cn : colNames) {
        Column *col = resolveTableColumn(srcTable, cn);
        if (!col)
            continue;
        GroupData gd;
        gd.label = col->name();
        for (int r = 0; r < col->rowCount(); ++r) {
            if (col->isInvalid(r) || col->isMasked(r))
                continue;
            double v = col->valueAt(r);
            if (std::isfinite(v))
                gd.vals.push_back(v);
        }
        if (gd.vals.size() < 2)
            continue;
        const size_t nk = gd.vals.size();
        gd.mean = std::accumulate(gd.vals.begin(), gd.vals.end(), 0.0) / static_cast<double>(nk);
        double ss = 0.0;
        gd.minV = gd.vals.front();
        gd.maxV = gd.vals.front();
        for (double v : gd.vals) {
            double d = v - gd.mean;
            ss += d * d;
            gd.minV = std::min(gd.minV, v);
            gd.maxV = std::max(gd.maxV, v);
        }
        gd.var = ss / static_cast<double>(nk - 1);
        groups.push_back(std::move(gd));
    }

    const int K = static_cast<int>(groups.size());
    if (K < 2)
        return nullptr;

    // One-Way ANOVA
    size_t totalN = 0;
    double weightedSum = 0.0;
    for (const GroupData &g : groups) {
        totalN += g.vals.size();
        weightedSum += static_cast<double>(g.vals.size()) * g.mean;
    }
    const double grandMean = weightedSum / static_cast<double>(totalN);

    double ssBetween = 0.0;
    double ssWithin = 0.0;
    for (const GroupData &g : groups) {
        double d = g.mean - grandMean;
        ssBetween += static_cast<double>(g.vals.size()) * d * d;
        ssWithin += static_cast<double>(g.vals.size() - 1) * g.var;
    }

    const double df1 = static_cast<double>(K - 1);
    const double df2 = static_cast<double>(totalN - static_cast<size_t>(K));
    const double msBetween = ssBetween / df1;
    const double msWithin = (df2 > 0.0) ? (ssWithin / df2) : 0.0;
    double fStat = 0.0;
    double pAnova = 1.0;
    if (msWithin > 1e-30) {
        fStat = msBetween / msWithin;
        pAnova = (fStat > 0.0 && df2 > 0.0) ? gsl_cdf_fdist_Q(fStat, df1, df2) : 1.0;
    } else if (ssBetween > 1e-30) {
        fStat = 1e12;
        pAnova = 0.0;
    }
    const double etaSq =
            (ssBetween + ssWithin > 1e-30) ? (ssBetween / (ssBetween + ssWithin)) : 0.0;

    // Pairwise Welch t-tests + Holm-Bonferroni multiplicity correction
    struct PairResult
    {
        int i = 0;
        int j = 1;
        QString label;
        double tStat = 0.0;
        double dfWelch = 1.0;
        double pRaw = 1.0;
        double pAdj = 1.0;
        double meanDiff = 0.0;
    };
    std::vector<PairResult> pairs;
    for (int i = 0; i < K; ++i) {
        for (int j = i + 1; j < K; ++j) {
            const GroupData &gi = groups[static_cast<size_t>(i)];
            const GroupData &gj = groups[static_cast<size_t>(j)];
            const double ni = static_cast<double>(gi.vals.size());
            const double nj = static_cast<double>(gj.vals.size());
            const double vi = gi.var / ni;
            const double vj = gj.var / nj;
            const double vSum = vi + vj;

            PairResult pr;
            pr.i = i;
            pr.j = j;
            pr.label = QStringLiteral("%1 vs %2").arg(gi.label, gj.label);
            pr.meanDiff = gi.mean - gj.mean;
            if (vSum > 1e-30) {
                pr.tStat = pr.meanDiff / std::sqrt(vSum);
                double denom = (vi * vi) / (ni - 1.0) + (vj * vj) / (nj - 1.0);
                pr.dfWelch = (denom > 1e-30) ? ((vSum * vSum) / denom) : (ni + nj - 2.0);
                pr.pRaw = std::clamp(2.0 * gsl_cdf_tdist_Q(std::abs(pr.tStat), pr.dfWelch), 0.0,
                                     1.0);
            } else if (std::abs(pr.meanDiff) > 1e-30) {
                pr.tStat = (pr.meanDiff > 0.0) ? 1e6 : -1e6;
                pr.dfWelch = ni + nj - 2.0;
                pr.pRaw = 0.0;
            }
            pr.pAdj = pr.pRaw;
            pairs.push_back(pr);
        }
    }

    // Holm-Bonferroni step-down adjustment across all pairs
    const size_t M = pairs.size();
    if (M > 1) {
        std::vector<size_t> order(M);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](size_t a, size_t b) { return pairs[a].pRaw < pairs[b].pRaw; });
        double runningMax = 0.0;
        for (size_t rank = 0; rank < M; ++rank) {
            size_t idx = order[rank];
            double mult = static_cast<double>(M - rank);
            double adj = std::min(1.0, pairs[idx].pRaw * mult);
            runningMax = std::max(runningMax, adj);
            pairs[idx].pAdj = runningMax;
        }
    }

    const int nRows = 1 + static_cast<int>(pairs.size());
    QString tblName =
            app->generateUniqueName(srcTable->name() + QStringLiteral("_GroupStats"), false);
    Table *resTbl =
            app->newTable(nRows, 8, tblName, QObject::tr("ANOVA & Pairwise Comparison Summary"));
    if (!resTbl)
        return nullptr;

    resTbl->setHeader(QStringList() << QObject::tr("Comparison") << QObject::tr("Statistic")
                                    << QObject::tr("df1") << QObject::tr("df2")
                                    << QObject::tr("p_value") << QObject::tr("p_adj")
                                    << QObject::tr("MeanDiff_Eta2") << QObject::tr("Significance"));
    resTbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
    for (int c = 1; c <= 6; ++c)
        resTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    resTbl->column(7)->setColumnMode(SciDAVis::ColumnMode::Text);

    // Row 0: Omnibus One-Way ANOVA
    resTbl->column(0)->setTextAt(0, QStringLiteral("One-Way ANOVA"));
    resTbl->column(1)->setValueAt(0, fStat);
    resTbl->column(2)->setValueAt(0, df1);
    resTbl->column(3)->setValueAt(0, df2);
    resTbl->column(4)->setValueAt(0, pAnova);
    resTbl->column(5)->setValueAt(0, pAnova);
    resTbl->column(6)->setValueAt(0, etaSq);
    resTbl->column(7)->setTextAt(0, starRatingForP(pAnova));

    // Rows 1..M: Pairwise Welch t-test comparisons
    for (size_t m = 0; m < M; ++m) {
        const int r = static_cast<int>(m) + 1;
        const PairResult &pr = pairs[m];
        resTbl->column(0)->setTextAt(r, pr.label);
        resTbl->column(1)->setValueAt(r, pr.tStat);
        resTbl->column(2)->setValueAt(r, pr.dfWelch);
        resTbl->column(3)->setValueAt(r, df2);
        resTbl->column(4)->setValueAt(r, pr.pRaw);
        resTbl->column(5)->setValueAt(r, pr.pAdj);
        resTbl->column(6)->setValueAt(r, pr.meanDiff);
        resTbl->column(7)->setTextAt(r, starRatingForP(pr.pAdj));
    }

    // Annotate significance brackets on annotateGraph if provided
    if (annotateGraph) {
        double yMinAll = groups.front().minV;
        double yMaxAll = groups.front().maxV;
        for (const GroupData &gd : groups) {
            yMinAll = std::min(yMinAll, gd.minV);
            yMaxAll = std::max(yMaxAll, gd.maxV);
        }
        const double ySpan = std::max(1e-6, yMaxAll - yMinAll);

        // Determine group X centers on annotateGraph (default 1.0, 2.0, ..., K for box/bar charts)
        std::vector<double> groupX(static_cast<size_t>(K));
        for (int k = 0; k < K; ++k)
            groupX[static_cast<size_t>(k)] = static_cast<double>(k + 1);

        if (annotateGraph->curves() >= K) {
            std::vector<double> curveMeans;
            bool distinctMeans = true;
            for (int k = 0; k < K; ++k) {
                QwtPlotCurve *cv = annotateGraph->curve(k);
                if (!cv || cv->dataSize() <= 0) {
                    distinctMeans = false;
                    break;
                }
                double sx = 0.0;
                for (int i = 0; i < cv->dataSize(); ++i)
                    sx += cv->x(i);
                double mx = sx / static_cast<double>(cv->dataSize());
                for (double prev : curveMeans) {
                    if (std::abs(mx - prev) < 1e-6)
                        distinctMeans = false;
                }
                curveMeans.push_back(mx);
            }
            if (distinctMeans && static_cast<int>(curveMeans.size()) == K)
                groupX = curveMeans;
        }

        // Select pairs to annotate (for K==2 always annotate; for K>2 annotate significant or top pair)
        std::vector<PairResult> toDraw;
        if (K == 2) {
            toDraw.push_back(pairs.front());
        } else {
            for (const PairResult &pr : pairs) {
                if (pr.pAdj < 0.05)
                    toDraw.push_back(pr);
            }
            if (toDraw.empty())
                toDraw.push_back(pairs.front());
        }

        const double stepY = 0.11 * ySpan;
        const double tickH = 0.032 * ySpan;
        const double baseTop = yMaxAll + 0.08 * ySpan;
        const double neededYMax =
                baseTop + static_cast<double>(toDraw.size() + 1) * stepY;

        const QwtScaleDiv *xDiv = annotateGraph->d_plot->axisScaleDiv(QwtPlot::xBottom);
        const QwtScaleDiv *yDiv = annotateGraph->d_plot->axisScaleDiv(QwtPlot::yLeft);
        double xLo = std::min(xDiv ? xDiv->lowerBound() : 0.5, groupX.front() - 0.6);
        double xHi = std::max(xDiv ? xDiv->upperBound() : static_cast<double>(K) + 0.5,
                              groupX.back() + 0.6);
        double yLo = std::min(yDiv ? yDiv->lowerBound() : (yMinAll - 0.1 * ySpan),
                              yMinAll - 0.05 * ySpan);
        double yHi = std::max(yDiv ? yDiv->upperBound() : neededYMax, neededYMax);

        annotateGraph->setScale(QwtPlot::xBottom, xLo, xHi);
        annotateGraph->setScale(QwtPlot::yLeft, yLo, yHi);
        annotateGraph->replot();

        const QColor bracketColor(30, 30, 30);
        for (size_t b = 0; b < toDraw.size(); ++b) {
            const PairResult &pr = toDraw[b];
            const double xLeft = groupX[static_cast<size_t>(pr.i)];
            const double xRight = groupX[static_cast<size_t>(pr.j)];
            const double yBar = baseTop + static_cast<double>(b) * stepY;

            // Left vertical tick, horizontal bracket bar, right vertical tick
            addLineSegmentOnGraph(annotateGraph, xLeft, yBar - tickH, xLeft, yBar, bracketColor, 2);
            addLineSegmentOnGraph(annotateGraph, xLeft, yBar, xRight, yBar, bracketColor, 2);
            addLineSegmentOnGraph(annotateGraph, xRight, yBar, xRight, yBar - tickH, bracketColor,
                                  2);

            QString stars = starRatingForP(pr.pAdj);
            addTextLabelOnGraph(annotateGraph, 0.5 * (xLeft + xRight), yBar + 0.42 * stepY, stars,
                                bracketColor, true, 1);
        }
        annotateGraph->replot();
    }

    return resTbl;
}

MultiLayer *GraphStatsAndSurfaceFit::plotROCCurve(ApplicationWindow *app, Table *srcTable,
                                                  const QString &stateCol, const QString &scoreCol,
                                                  double *aucOut)
{
    if (aucOut)
        *aucOut = 0.0;
    if (!app || !srcTable)
        return nullptr;

    Column *cState = resolveTableColumn(srcTable, stateCol);
    Column *cScore = resolveTableColumn(srcTable, scoreCol);
    if (!cState || !cScore)
        return nullptr;

    const int nRows = std::min(cState->rowCount(), cScore->rowCount());
    struct Obs
    {
        int label = 0;
        double score = 0.0;
    };
    std::vector<Obs> obs;

    if (cState->columnMode() == SciDAVis::ColumnMode::Numeric) {
        std::vector<double> rawStates;
        std::vector<double> rawScores;
        for (int r = 0; r < nRows; ++r) {
            if (cState->isInvalid(r) || cScore->isInvalid(r) || cState->isMasked(r)
                || cScore->isMasked(r))
                continue;
            double st = cState->valueAt(r);
            double sc = cScore->valueAt(r);
            if (std::isfinite(st) && std::isfinite(sc)) {
                rawStates.push_back(st);
                rawScores.push_back(sc);
            }
        }
        if (rawStates.size() < 2)
            return nullptr;
        double minSt = *std::min_element(rawStates.begin(), rawStates.end());
        double maxSt = *std::max_element(rawStates.begin(), rawStates.end());
        if (maxSt <= minSt)
            return nullptr;
        double midSt = 0.5 * (minSt + maxSt);
        for (size_t i = 0; i < rawStates.size(); ++i)
            obs.push_back({ rawStates[i] > midSt ? 1 : 0, rawScores[i] });
    } else {
        for (int r = 0; r < nRows; ++r) {
            if (cState->isInvalid(r) || cScore->isInvalid(r) || cState->isMasked(r)
                || cScore->isMasked(r))
                continue;
            QString st = cState->asStringColumn()->textAt(r).trimmed().toLower();
            double sc = cScore->valueAt(r);
            if (st.isEmpty() || !std::isfinite(sc))
                continue;
            int pos = (st == QStringLiteral("1") || st == QStringLiteral("true")
                       || st == QStringLiteral("pos") || st == QStringLiteral("positive")
                       || st == QStringLiteral("case") || st == QStringLiteral("yes"))
                    ? 1
                    : 0;
            obs.push_back({ pos, sc });
        }
    }

    size_t nPos = 0, nNeg = 0;
    for (const Obs &o : obs) {
        if (o.label == 1)
            ++nPos;
        else
            ++nNeg;
    }
    if (nPos == 0 || nNeg == 0)
        return nullptr;

    std::sort(obs.begin(), obs.end(),
              [](const Obs &a, const Obs &b) { return a.score > b.score; });

    struct RocPoint
    {
        double fpr = 0.0;
        double tpr = 0.0;
        double cutoff = 0.0;
        double youden = 0.0;
    };
    std::vector<RocPoint> pts;
    pts.push_back({ 0.0, 0.0, obs.front().score + 1.0, 0.0 });

    size_t tp = 0, fp = 0;
    size_t idx = 0;
    while (idx < obs.size()) {
        double thr = obs[idx].score;
        while (idx < obs.size() && obs[idx].score == thr) {
            if (obs[idx].label == 1)
                ++tp;
            else
                ++fp;
            ++idx;
        }
        double fpr = static_cast<double>(fp) / static_cast<double>(nNeg);
        double tpr = static_cast<double>(tp) / static_cast<double>(nPos);
        pts.push_back({ fpr, tpr, thr, tpr - fpr });
    }

    // Exact trapezoidal AUC
    double auc = 0.0;
    double bestJ = -1.0;
    double bestCutoff = pts.front().cutoff;
    double bestTPR = 0.0, bestFPR = 0.0;
    for (size_t i = 1; i < pts.size(); ++i) {
        auc += 0.5 * (pts[i].fpr - pts[i - 1].fpr) * (pts[i].tpr + pts[i - 1].tpr);
        if (pts[i].youden > bestJ) {
            bestJ = pts[i].youden;
            bestCutoff = pts[i].cutoff;
            bestTPR = pts[i].tpr;
            bestFPR = pts[i].fpr;
        }
    }
    auc = std::clamp(auc, 0.0, 1.0);
    if (aucOut)
        *aucOut = auc;

    // Hanley-McNeil SE(AUC)
    const double q1 = auc / (2.0 - auc);
    const double q2 = (2.0 * auc * auc) / (1.0 + auc);
    const double dp = static_cast<double>(nPos);
    const double dn = static_cast<double>(nNeg);
    const double varAuc =
            (auc * (1.0 - auc) + (dp - 1.0) * (q1 - auc * auc) + (dn - 1.0) * (q2 - auc * auc))
            / (dp * dn);
    const double seAuc = std::sqrt(std::max(0.0, varAuc));

    const int mRows = static_cast<int>(pts.size());
    QString rocName = app->generateUniqueName(srcTable->name() + QStringLiteral("_ROC"), false);
    Table *rocTbl = app->newTable(mRows, 6, rocName, QObject::tr("ROC Curve Analysis"));
    if (!rocTbl)
        return nullptr;

    rocTbl->setHeader(QStringList() << QObject::tr("FPR_1MinusSpec") << QObject::tr("TPR_Sens")
                                    << QObject::tr("RefDiag") << QObject::tr("Cutoff")
                                    << QObject::tr("Specificity") << QObject::tr("Youden_J"));
    for (int c = 0; c < 6; ++c) {
        rocTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        rocTbl->column(c)->setPlotDesignation(c == 0 ? SciDAVis::X : SciDAVis::Y);
    }
    for (int i = 0; i < mRows; ++i) {
        const RocPoint &rp = pts[static_cast<size_t>(i)];
        rocTbl->column(0)->setValueAt(i, rp.fpr);
        rocTbl->column(1)->setValueAt(i, rp.tpr);
        rocTbl->column(2)->setValueAt(i, rp.fpr);
        rocTbl->column(3)->setValueAt(i, rp.cutoff);
        rocTbl->column(4)->setValueAt(i, 1.0 - rp.fpr);
        rocTbl->column(5)->setValueAt(i, rp.youden);
    }

    MultiLayer *ml =
            app->newGraph(app->generateUniqueName(QStringLiteral("ROC_") + srcTable->name(), false));
    if (!ml)
        return nullptr;
    Graph *g = ml->activeGraph();
    if (g) {
        g->insertCurve(rocTbl, rocTbl->colName(0), rocTbl->colName(1), Graph::Line);
        g->insertCurve(rocTbl, rocTbl->colName(0), rocTbl->colName(2), Graph::Line);
        if (g->curves() >= 2) {
            g->setCurvePen(0, QPen(QColor(25, 90, 185), 2, Qt::SolidLine));
            g->setCurveBrush(0, QBrush(QColor(25, 90, 185, 32)));
            g->setCurvePen(1, QPen(QColor(135, 135, 135), 1, Qt::DashLine));
        }
        g->setXAxisTitle(QObject::tr("1 - Specificity (False Positive Rate)"));
        g->setYAxisTitle(QObject::tr("Sensitivity (True Positive Rate)"));
        g->setTitle(QObject::tr("ROC Curve (AUC = %1 ± %2)").arg(auc, 0, 'f', 4).arg(seAuc, 0, 'f', 4));
        g->setScale(QwtPlot::xBottom, 0.0, 1.0);
        g->setScale(QwtPlot::yLeft, 0.0, 1.02);
        if (g->legend())
            g->legend()->setText(g->legendText());
        g->replot();

        QString summary = QObject::tr("AUC = %1 (SE = %2)\nOptimal J = %3 @ Cutoff = %4\n(Sens = %5, Spec = %6)")
                                  .arg(auc, 0, 'f', 4)
                                  .arg(seAuc, 0, 'f', 4)
                                  .arg(bestJ, 0, 'f', 4)
                                  .arg(bestCutoff, 0, 'g', 5)
                                  .arg(bestTPR, 0, 'f', 3)
                                  .arg(1.0 - bestFPR, 0, 'f', 3);
        addTextLabelOnGraph(g, 0.42, 0.25, summary, QColor(25, 45, 90), false, -1);
        g->replot();
    }

    return ml;
}

MultiLayer *GraphStatsAndSurfaceFit::plotKaplanMeier(ApplicationWindow *app, Table *srcTable,
                                                     const QString &timeCol,
                                                     const QString &eventCol,
                                                     const QString &groupCol, double *logRankPOut)
{
    if (logRankPOut)
        *logRankPOut = 1.0;
    if (!app || !srcTable)
        return nullptr;

    Column *cTime = resolveTableColumn(srcTable, timeCol);
    Column *cEvent = resolveTableColumn(srcTable, eventCol);
    Column *cGroup = groupCol.trimmed().isEmpty() ? nullptr : resolveTableColumn(srcTable, groupCol);
    if (!cTime || !cEvent)
        return nullptr;

    const int nRows = std::min(cTime->rowCount(), cEvent->rowCount());
    struct SurvObs
    {
        double t = 0.0;
        int ev = 0;
        QString grp;
    };
    std::vector<SurvObs> obs;
    QStringList groupNames;

    for (int r = 0; r < nRows; ++r) {
        if (cTime->isInvalid(r) || cEvent->isInvalid(r) || cTime->isMasked(r)
            || cEvent->isMasked(r))
            continue;
        if (cGroup && (cGroup->isInvalid(r) || cGroup->isMasked(r)))
            continue;
        double tv = cTime->valueAt(r);
        if (!std::isfinite(tv) || tv < 0.0)
            continue;

        int ev = 0;
        if (cEvent->columnMode() == SciDAVis::ColumnMode::Numeric) {
            double eVal = cEvent->valueAt(r);
            if (!std::isfinite(eVal))
                continue;
            ev = (eVal > 0.5) ? 1 : 0;
        } else {
            QString es = cEvent->asStringColumn()->textAt(r).trimmed().toLower();
            ev = (es == QStringLiteral("1") || es == QStringLiteral("true")
                  || es == QStringLiteral("event") || es == QStringLiteral("dead")
                  || es == QStringLiteral("yes"))
                    ? 1
                    : 0;
        }

        QString grp = QStringLiteral("All");
        if (cGroup && r < cGroup->rowCount()) {
            if (cGroup->columnMode() == SciDAVis::ColumnMode::Numeric) {
                double gv = cGroup->valueAt(r);
                if (!std::isfinite(gv))
                    continue;
                grp = QString::number(gv, 'g', 8);
            } else {
                grp = cGroup->asStringColumn()->textAt(r).trimmed();
                if (grp.isEmpty())
                    continue;
            }
        }
        obs.push_back({ tv, ev, grp });
        if (!groupNames.contains(grp))
            groupNames.append(grp);
    }

    if (obs.size() < 2 || groupNames.isEmpty())
        return nullptr;
    groupNames.sort();
    const int G = groupNames.size();

    // Per-group Kaplan-Meier step survival curves
    struct StepCurve
    {
        QString name;
        std::vector<double> t;
        std::vector<double> s;
        std::vector<double> se;
    };
    std::vector<StepCurve> curves(static_cast<size_t>(G));
    double maxTimeAll = 0.0;

    for (int gIdx = 0; gIdx < G; ++gIdx) {
        const QString &gName = groupNames[gIdx];
        std::vector<SurvObs> gObs;
        for (const SurvObs &o : obs) {
            if (o.grp == gName)
                gObs.push_back(o);
        }
        std::sort(gObs.begin(), gObs.end(),
                  [](const SurvObs &a, const SurvObs &b) { return a.t < b.t; });

        StepCurve &sc = curves[static_cast<size_t>(gIdx)];
        sc.name = gName;
        sc.t.push_back(0.0);
        sc.s.push_back(1.0);
        sc.se.push_back(0.0);

        double curS = 1.0;
        double greenwoodSum = 0.0;
        size_t i = 0;
        while (i < gObs.size()) {
            const double ti = gObs[i].t;
            maxTimeAll = std::max(maxTimeAll, ti);
            const size_t nAtRisk = gObs.size() - i;
            size_t dEvents = 0;
            while (i < gObs.size() && gObs[i].t == ti) {
                if (gObs[i].ev == 1)
                    ++dEvents;
                ++i;
            }
            if (dEvents > 0 && nAtRisk > 0) {
                curS *= (1.0 - static_cast<double>(dEvents) / static_cast<double>(nAtRisk));
                if (nAtRisk > dEvents) {
                    greenwoodSum += static_cast<double>(dEvents)
                            / (static_cast<double>(nAtRisk)
                               * static_cast<double>(nAtRisk - dEvents));
                }
            }
            double curSE = curS * std::sqrt(std::max(0.0, greenwoodSum));
            if (ti > 0.0 || dEvents > 0) {
                sc.t.push_back(ti);
                sc.s.push_back(curS);
                sc.se.push_back(curSE);
            } else {
                sc.s[0] = curS;
                sc.se[0] = curSE;
            }
        }
    }

    // Mantel-Cox Log-Rank Test across G >= 2 groups
    double logRankChi2 = 0.0;
    double logRankP = 1.0;
    int logRankDf = std::max(1, G - 1);

    if (G >= 2) {
        std::vector<double> eventTimes;
        for (const SurvObs &o : obs) {
            if (o.ev == 1)
                eventTimes.push_back(o.t);
        }
        std::sort(eventTimes.begin(), eventTimes.end());
        eventTimes.erase(std::unique(eventTimes.begin(), eventTimes.end()), eventTimes.end());

        const int dim = G - 1;
        std::vector<double> U(static_cast<size_t>(dim), 0.0);
        std::vector<double> V(static_cast<size_t>(dim * dim), 0.0);

        for (double tau : eventTimes) {
            std::vector<double> n_g(static_cast<size_t>(G), 0.0);
            std::vector<double> d_g(static_cast<size_t>(G), 0.0);
            double nTot = 0.0, dTot = 0.0;
            for (const SurvObs &o : obs) {
                int gi = groupNames.indexOf(o.grp);
                if (gi < 0)
                    continue;
                if (o.t >= tau) {
                    n_g[static_cast<size_t>(gi)] += 1.0;
                    nTot += 1.0;
                }
                if (o.t == tau && o.ev == 1) {
                    d_g[static_cast<size_t>(gi)] += 1.0;
                    dTot += 1.0;
                }
            }
            if (nTot <= 1.0 || dTot <= 0.0)
                continue;

            const double alpha = (dTot * (nTot - dTot)) / (nTot * nTot * (nTot - 1.0));
            for (int g = 0; g < dim; ++g) {
                double expected = dTot * (n_g[static_cast<size_t>(g)] / nTot);
                U[static_cast<size_t>(g)] += (d_g[static_cast<size_t>(g)] - expected);
                for (int h = 0; h < dim; ++h) {
                    if (g == h) {
                        V[static_cast<size_t>(g * dim + h)] += alpha * n_g[static_cast<size_t>(g)]
                                * (nTot - n_g[static_cast<size_t>(g)]);
                    } else {
                        V[static_cast<size_t>(g * dim + h)] -= alpha * n_g[static_cast<size_t>(g)]
                                * n_g[static_cast<size_t>(h)];
                    }
                }
            }
        }

        if (dim == 1) {
            if (V[0] > 1e-15)
                logRankChi2 = (U[0] * U[0]) / V[0];
            logRankDf = 1;
        } else {
            gsl_matrix *matV = gsl_matrix_alloc(static_cast<size_t>(dim), static_cast<size_t>(dim));
            for (int r = 0; r < dim; ++r)
                for (int c = 0; c < dim; ++c)
                    gsl_matrix_set(matV, static_cast<size_t>(r), static_cast<size_t>(c),
                                   V[static_cast<size_t>(r * dim + c)]);
            gsl_vector *eval = gsl_vector_alloc(static_cast<size_t>(dim));
            gsl_matrix *evec = gsl_matrix_alloc(static_cast<size_t>(dim), static_cast<size_t>(dim));
            gsl_eigen_symmv_workspace *ws = gsl_eigen_symmv_alloc(static_cast<size_t>(dim));
            gsl_eigen_symmv(matV, eval, evec, ws);
            gsl_eigen_symmv_sort(eval, evec, GSL_EIGEN_SORT_VAL_DESC);

            double maxEv = std::max(0.0, gsl_vector_get(eval, 0));
            int rank = 0;
            logRankChi2 = 0.0;
            for (int k = 0; k < dim; ++k) {
                double lam = gsl_vector_get(eval, static_cast<size_t>(k));
                if (lam > 1e-11 * std::max(1.0, maxEv)) {
                    double proj = 0.0;
                    for (int i = 0; i < dim; ++i)
                        proj += U[static_cast<size_t>(i)]
                                * gsl_matrix_get(evec, static_cast<size_t>(i),
                                                 static_cast<size_t>(k));
                    logRankChi2 += (proj * proj) / lam;
                    ++rank;
                }
            }
            logRankDf = std::max(1, rank);
            gsl_eigen_symmv_free(ws);
            gsl_matrix_free(evec);
            gsl_vector_free(eval);
            gsl_matrix_free(matV);
        }
        logRankP = (logRankChi2 > 0.0)
                ? std::clamp(gsl_cdf_chisq_Q(logRankChi2, static_cast<double>(logRankDf)), 0.0, 1.0)
                : 1.0;
    }

    if (logRankPOut)
        *logRankPOut = logRankP;

    // Build Kaplan-Meier output Table
    int maxSteps = 1;
    for (const StepCurve &sc : curves)
        maxSteps = std::max(maxSteps, static_cast<int>(sc.t.size()));

    QString kmName = app->generateUniqueName(srcTable->name() + QStringLiteral("_KM"), false);
    Table *kmTbl = app->newTable(maxSteps, 3 * G, kmName, QObject::tr("Kaplan-Meier Survival"));
    if (!kmTbl)
        return nullptr;

    QStringList headers;
    for (int gIdx = 0; gIdx < G; ++gIdx) {
        const QString &gn = curves[static_cast<size_t>(gIdx)].name;
        headers << QStringLiteral("Time_%1").arg(gn) << QStringLiteral("Surv_%1").arg(gn)
                << QStringLiteral("SE_%1").arg(gn);
    }
    kmTbl->setHeader(headers);

    for (int gIdx = 0; gIdx < G; ++gIdx) {
        const StepCurve &sc = curves[static_cast<size_t>(gIdx)];
        int baseC = 3 * gIdx;
        kmTbl->column(baseC)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        kmTbl->column(baseC + 1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        kmTbl->column(baseC + 2)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        kmTbl->column(baseC)->setPlotDesignation(SciDAVis::X);
        kmTbl->column(baseC + 1)->setPlotDesignation(SciDAVis::Y);
        kmTbl->column(baseC + 2)->setPlotDesignation(SciDAVis::yErr);

        for (int r = 0; r < maxSteps; ++r) {
            if (r < static_cast<int>(sc.t.size())) {
                kmTbl->column(baseC)->setValueAt(r, sc.t[static_cast<size_t>(r)]);
                kmTbl->column(baseC + 1)->setValueAt(r, sc.s[static_cast<size_t>(r)]);
                kmTbl->column(baseC + 2)->setValueAt(r, sc.se[static_cast<size_t>(r)]);
            } else {
                kmTbl->column(baseC)->setInvalid(r, true);
                kmTbl->column(baseC + 1)->setInvalid(r, true);
                kmTbl->column(baseC + 2)->setInvalid(r, true);
            }
        }
    }

    MultiLayer *ml =
            app->newGraph(app->generateUniqueName(QStringLiteral("KM_") + srcTable->name(), false));
    if (!ml)
        return nullptr;
    Graph *g = ml->activeGraph();
    if (g) {
        static const QColor kColors[] = { QColor(25, 95, 190),  QColor(210, 45, 45),
                                          QColor(30, 150, 75),  QColor(155, 60, 180),
                                          QColor(225, 130, 20), QColor(20, 160, 175) };
        for (int gIdx = 0; gIdx < G; ++gIdx) {
            int baseC = 3 * gIdx;
            if (g->insertCurve(kmTbl, kmTbl->colName(baseC), kmTbl->colName(baseC + 1),
                               Graph::HorizontalSteps)) {
                int cIdx = g->curves() - 1;
                g->setCurvePen(cIdx, QPen(kColors[gIdx % 6], 2, Qt::SolidLine));
            }
        }
        g->setXAxisTitle(QObject::tr("Time"));
        g->setYAxisTitle(QObject::tr("Survival Probability S(t)"));
        if (G >= 2) {
            g->setTitle(QObject::tr("Kaplan-Meier Survival (Log-Rank p = %1)")
                                .arg(logRankP, 0, 'g', 4));
        } else {
            g->setTitle(QObject::tr("Kaplan-Meier Survival Curve"));
        }
        g->setScale(QwtPlot::xBottom, 0.0, std::max(1.0, maxTimeAll * 1.05));
        g->setScale(QwtPlot::yLeft, 0.0, 1.05);
        if (g->legend())
            g->legend()->setText(g->legendText());
        g->replot();

        if (G >= 2) {
            QString lrBox = QObject::tr("Mantel-Cox Log-Rank Test\nChi^2 = %1 (df = %2)\np = %3 (%4)")
                                    .arg(logRankChi2, 0, 'f', 4)
                                    .arg(logRankDf)
                                    .arg(logRankP, 0, 'g', 4)
                                    .arg(starRatingForP(logRankP));
            addTextLabelOnGraph(g, 0.52 * std::max(1.0, maxTimeAll), 0.25, lrBox,
                                QColor(25, 25, 25), false, -1);
            g->replot();
        }
    }

    return ml;
}

MultiLayer *GraphStatsAndSurfaceFit::runPCAAndBiplot(ApplicationWindow *app, Table *srcTable,
                                                     const QStringList &colNames,
                                                     Table **scoresTableOut)
{
    if (scoresTableOut)
        *scoresTableOut = nullptr;
    if (!app || !srcTable || colNames.size() < 2)
        return nullptr;

    std::vector<Column *> cols;
    QStringList varLabels;
    int maxRowCount = std::numeric_limits<int>::max();
    for (const QString &cn : colNames) {
        Column *c = resolveTableColumn(srcTable, cn);
        if (c && c->rowCount() >= 3) {
            cols.push_back(c);
            varLabels.append(c->name());
            maxRowCount = std::min(maxRowCount, c->rowCount());
        }
    }

    const int P = static_cast<int>(cols.size());
    if (P < 2 || maxRowCount < 3)
        return nullptr;

    std::vector<std::vector<double>> rawX;
    for (int r = 0; r < maxRowCount; ++r) {
        bool ok = true;
        std::vector<double> rowVals(static_cast<size_t>(P));
        for (int j = 0; j < P; ++j) {
            Column *c = cols[static_cast<size_t>(j)];
            if (c->isInvalid(r) || c->isMasked(r)) {
                ok = false;
                break;
            }
            double v = c->valueAt(r);
            if (!std::isfinite(v)) {
                ok = false;
                break;
            }
            rowVals[static_cast<size_t>(j)] = v;
        }
        if (ok)
            rawX.push_back(std::move(rowVals));
    }

    const int N = static_cast<int>(rawX.size());
    if (N < 3)
        return nullptr;

    // Standardize variables to zero mean, unit variance
    std::vector<double> means(static_cast<size_t>(P), 0.0);
    std::vector<double> sds(static_cast<size_t>(P), 1.0);
    for (int j = 0; j < P; ++j) {
        double s = 0.0;
        for (int i = 0; i < N; ++i)
            s += rawX[static_cast<size_t>(i)][static_cast<size_t>(j)];
        means[static_cast<size_t>(j)] = s / static_cast<double>(N);

        double ss = 0.0;
        for (int i = 0; i < N; ++i) {
            double d = rawX[static_cast<size_t>(i)][static_cast<size_t>(j)]
                    - means[static_cast<size_t>(j)];
            ss += d * d;
        }
        double sd = std::sqrt(ss / static_cast<double>(N - 1));
        sds[static_cast<size_t>(j)] = (sd > 1e-15) ? sd : 1.0;
    }

    std::vector<std::vector<double>> Z(static_cast<size_t>(N),
                                       std::vector<double>(static_cast<size_t>(P), 0.0));
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < P; ++j) {
            Z[static_cast<size_t>(i)][static_cast<size_t>(j)] =
                    (rawX[static_cast<size_t>(i)][static_cast<size_t>(j)]
                     - means[static_cast<size_t>(j)])
                    / sds[static_cast<size_t>(j)];
        }
    }

    // Correlation matrix R = Z^T Z / (N - 1)
    gsl_matrix *R = gsl_matrix_alloc(static_cast<size_t>(P), static_cast<size_t>(P));
    for (int j1 = 0; j1 < P; ++j1) {
        for (int j2 = j1; j2 < P; ++j2) {
            double dot = 0.0;
            for (int i = 0; i < N; ++i)
                dot += Z[static_cast<size_t>(i)][static_cast<size_t>(j1)]
                        * Z[static_cast<size_t>(i)][static_cast<size_t>(j2)];
            double rVal = dot / static_cast<double>(N - 1);
            gsl_matrix_set(R, static_cast<size_t>(j1), static_cast<size_t>(j2), rVal);
            gsl_matrix_set(R, static_cast<size_t>(j2), static_cast<size_t>(j1), rVal);
        }
    }

    gsl_vector *eval = gsl_vector_alloc(static_cast<size_t>(P));
    gsl_matrix *evec = gsl_matrix_alloc(static_cast<size_t>(P), static_cast<size_t>(P));
    gsl_eigen_symmv_workspace *ws = gsl_eigen_symmv_alloc(static_cast<size_t>(P));
    gsl_eigen_symmv(R, eval, evec, ws);
    gsl_eigen_symmv_sort(eval, evec, GSL_EIGEN_SORT_VAL_DESC);

    std::vector<double> eigVals(static_cast<size_t>(P), 0.0);
    double eigSum = 0.0;
    for (int k = 0; k < P; ++k) {
        eigVals[static_cast<size_t>(k)] = std::max(0.0, gsl_vector_get(eval, static_cast<size_t>(k)));
        eigSum += eigVals[static_cast<size_t>(k)];
        // Deterministic sign orientation: make largest-magnitude loading positive
        int maxIdx = 0;
        double maxAbs = 0.0;
        for (int j = 0; j < P; ++j) {
            double a = std::abs(gsl_matrix_get(evec, static_cast<size_t>(j), static_cast<size_t>(k)));
            if (a > maxAbs) {
                maxAbs = a;
                maxIdx = j;
            }
        }
        if (gsl_matrix_get(evec, static_cast<size_t>(maxIdx), static_cast<size_t>(k)) < 0.0) {
            for (int j = 0; j < P; ++j) {
                double v = gsl_matrix_get(evec, static_cast<size_t>(j), static_cast<size_t>(k));
                gsl_matrix_set(evec, static_cast<size_t>(j), static_cast<size_t>(k), -v);
            }
        }
    }
    if (eigSum <= 0.0)
        eigSum = 1.0;

    std::vector<double> explainedPct(static_cast<size_t>(P), 0.0);
    for (int k = 0; k < P; ++k)
        explainedPct[static_cast<size_t>(k)] = 100.0 * eigVals[static_cast<size_t>(k)] / eigSum;

    std::vector<double> pc1(static_cast<size_t>(N), 0.0);
    std::vector<double> pc2(static_cast<size_t>(N), 0.0);
    double maxScoreAbs = 1e-6;
    for (int i = 0; i < N; ++i) {
        double s1 = 0.0, s2 = 0.0;
        for (int j = 0; j < P; ++j) {
            s1 += Z[static_cast<size_t>(i)][static_cast<size_t>(j)]
                    * gsl_matrix_get(evec, static_cast<size_t>(j), 0);
            s2 += Z[static_cast<size_t>(i)][static_cast<size_t>(j)]
                    * gsl_matrix_get(evec, static_cast<size_t>(j), 1);
        }
        pc1[static_cast<size_t>(i)] = s1;
        pc2[static_cast<size_t>(i)] = s2;
        maxScoreAbs = std::max({ maxScoreAbs, std::abs(s1), std::abs(s2) });
    }

    std::vector<double> load1(static_cast<size_t>(P), 0.0);
    std::vector<double> load2(static_cast<size_t>(P), 0.0);
    double maxLoadLen = 1e-6;
    for (int j = 0; j < P; ++j) {
        load1[static_cast<size_t>(j)] = gsl_matrix_get(evec, static_cast<size_t>(j), 0);
        load2[static_cast<size_t>(j)] = gsl_matrix_get(evec, static_cast<size_t>(j), 1);
        maxLoadLen = std::max(maxLoadLen,
                              std::hypot(load1[static_cast<size_t>(j)],
                                         load2[static_cast<size_t>(j)]));
    }

    gsl_eigen_symmv_free(ws);
    gsl_matrix_free(evec);
    gsl_vector_free(eval);
    gsl_matrix_free(R);

    // Create <Table>_PCAScores Table
    const int tblRows = std::max(N, P);
    QString pcaTblName =
            app->generateUniqueName(srcTable->name() + QStringLiteral("_PCAScores"), false);
    Table *scoresTbl = app->newTable(tblRows, 9, pcaTblName, QObject::tr("PCA Scores & Loadings"));
    if (!scoresTbl)
        return nullptr;

    scoresTbl->setHeader(QStringList()
                         << QStringLiteral("PC1") << QStringLiteral("PC2")
                         << QStringLiteral("Component") << QStringLiteral("Eigenvalue")
                         << QStringLiteral("ExplainedVarPct") << QStringLiteral("CumulativeVarPct")
                         << QStringLiteral("Variable") << QStringLiteral("Loading_PC1")
                         << QStringLiteral("Loading_PC2"));
    scoresTbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    scoresTbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    scoresTbl->column(2)->setColumnMode(SciDAVis::ColumnMode::Text);
    scoresTbl->column(3)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    scoresTbl->column(4)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    scoresTbl->column(5)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    scoresTbl->column(6)->setColumnMode(SciDAVis::ColumnMode::Text);
    scoresTbl->column(7)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    scoresTbl->column(8)->setColumnMode(SciDAVis::ColumnMode::Numeric);

    scoresTbl->column(0)->setPlotDesignation(SciDAVis::X);
    scoresTbl->column(1)->setPlotDesignation(SciDAVis::Y);

    double cumPct = 0.0;
    for (int r = 0; r < tblRows; ++r) {
        if (r < N) {
            scoresTbl->column(0)->setValueAt(r, pc1[static_cast<size_t>(r)]);
            scoresTbl->column(1)->setValueAt(r, pc2[static_cast<size_t>(r)]);
        } else {
            scoresTbl->column(0)->setInvalid(r, true);
            scoresTbl->column(1)->setInvalid(r, true);
        }

        if (r < P) {
            cumPct += explainedPct[static_cast<size_t>(r)];
            scoresTbl->column(2)->setTextAt(r, QStringLiteral("PC%1").arg(r + 1));
            scoresTbl->column(3)->setValueAt(r, eigVals[static_cast<size_t>(r)]);
            scoresTbl->column(4)->setValueAt(r, explainedPct[static_cast<size_t>(r)]);
            scoresTbl->column(5)->setValueAt(r, cumPct);
            scoresTbl->column(6)->setTextAt(r, varLabels[r]);
            scoresTbl->column(7)->setValueAt(r, load1[static_cast<size_t>(r)]);
            scoresTbl->column(8)->setValueAt(r, load2[static_cast<size_t>(r)]);
        } else {
            scoresTbl->column(2)->setInvalid(r, true);
            scoresTbl->column(3)->setInvalid(r, true);
            scoresTbl->column(4)->setInvalid(r, true);
            scoresTbl->column(5)->setInvalid(r, true);
            scoresTbl->column(6)->setInvalid(r, true);
            scoresTbl->column(7)->setInvalid(r, true);
            scoresTbl->column(8)->setInvalid(r, true);
        }
    }

    if (scoresTableOut)
        *scoresTableOut = scoresTbl;

    // Create OriginPro PCA Biplot MultiLayer
    MultiLayer *ml = app->newGraph(
            app->generateUniqueName(QStringLiteral("PCABiplot_") + srcTable->name(), false));
    if (!ml)
        return nullptr;

    Graph *g = ml->activeGraph();
    if (g) {
        if (g->insertCurve(scoresTbl, scoresTbl->colName(0), scoresTbl->colName(1), Graph::Scatter,
                           0, N - 1)) {
            int cIdx = g->curves() - 1;
            QwtSymbol sym(QwtSymbol::Ellipse, QBrush(QColor(30, 100, 195, 190)),
                          QPen(QColor(20, 65, 140), 1), QSize(7, 7));
            g->setCurveSymbol(cIdx, sym);
        }

        const double lim = 1.25 * maxScoreAbs;
        g->setScale(QwtPlot::xBottom, -lim, lim);
        g->setScale(QwtPlot::yLeft, -lim, lim);
        g->setXAxisTitle(QStringLiteral("PC1 (%1%)").arg(explainedPct[0], 0, 'f', 1));
        g->setYAxisTitle(QStringLiteral("PC2 (%1%)").arg(explainedPct[1], 0, 'f', 1));
        g->setTitle(QObject::tr("PCA Biplot (PC1 + PC2 = %1%)")
                            .arg(explainedPct[0] + explainedPct[1], 0, 'f', 1));
        if (g->legend())
            g->legend()->setText(g->legendText());
        g->replot();

        // Zero-reference axes
        addLineSegmentOnGraph(g, -lim, 0.0, lim, 0.0, QColor(170, 170, 170), 1, Qt::DashLine, false);
        addLineSegmentOnGraph(g, 0.0, -lim, 0.0, lim, QColor(170, 170, 170), 1, Qt::DashLine, false);

        // Loading vectors + variable labels
        const double vecScale = (0.82 * maxScoreAbs) / maxLoadLen;
        const QColor loadColor(195, 35, 35);
        for (int j = 0; j < P; ++j) {
            const double vx = load1[static_cast<size_t>(j)] * vecScale;
            const double vy = load2[static_cast<size_t>(j)] * vecScale;
            addLineSegmentOnGraph(g, 0.0, 0.0, vx, vy, loadColor, 2, Qt::SolidLine, true, 9, 30);
            addTextLabelOnGraph(g, 1.08 * vx, 1.08 * vy, varLabels[j], loadColor, true, -1);
        }
        g->replot();
    }

    return ml;
}

// ============================================================================
// Interactive Dialogs (Bilingual English / Japanese via QObject::tr)
// ============================================================================

void GraphStatsAndSurfaceFit::showSurfaceFitDialog(ApplicationWindow *app)
{
    if (!app)
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("3D Surface Fit (XYZ / Matrix)"));
    dlg.setMinimumWidth(460);
    QVBoxLayout *mainLay = new QVBoxLayout(&dlg);

    QFormLayout *form = new QFormLayout();
    QComboBox *modeCombo = new QComboBox(&dlg);
    modeCombo->addItem(QObject::tr("XYZ Worksheet Columns"));
    modeCombo->addItem(QObject::tr("Matrix Grid"));
    form->addRow(QObject::tr("Data Source:"), modeCombo);

    QComboBox *tableCombo = new QComboBox(&dlg);
    QList<Table *> tables = allProjectTables(app);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        tableCombo->addItem(t->name());
        if (t == actTbl)
            tableCombo->setCurrentIndex(tableCombo->count() - 1);
    }
    form->addRow(QObject::tr("Table:"), tableCombo);

    QComboBox *xCombo = new QComboBox(&dlg);
    QComboBox *yCombo = new QComboBox(&dlg);
    QComboBox *zCombo = new QComboBox(&dlg);
    form->addRow(QObject::tr("X Column:"), xCombo);
    form->addRow(QObject::tr("Y Column:"), yCombo);
    form->addRow(QObject::tr("Z Column:"), zCombo);

    QComboBox *matrixCombo = new QComboBox(&dlg);
    QList<Matrix *> matrices = allProjectMatrices(app);
    Matrix *actMat = resolveActiveMatrix(app);
    for (Matrix *m : matrices) {
        matrixCombo->addItem(m->name());
        if (m == actMat)
            matrixCombo->setCurrentIndex(matrixCombo->count() - 1);
    }
    form->addRow(QObject::tr("Matrix:"), matrixCombo);

    if (actMat && (!actTbl || app->d_workspace.activeSubWindow() == actMat))
        modeCombo->setCurrentIndex(1);

    QComboBox *modelCombo = new QComboBox(&dlg);
    modelCombo->addItems({ QObject::tr("Plane 2D (z0 + a*x + b*y)"),
                           QObject::tr("Quadratic Polynomial 2D"),
                           QObject::tr("2D Gaussian (Rotated Elliptical)"),
                           QObject::tr("2D Lorentzian (Rotated)"),
                           QObject::tr("2D Pseudo-Voigt"),
                           QObject::tr("2D Exponential"),
                           QObject::tr("2D Chebyshev Bivariate"),
                           QObject::tr("Custom 2D Expression f(x, y)") });
    modelCombo->setCurrentIndex(static_cast<int>(Gauss2D));
    form->addRow(QObject::tr("Surface Model:"), modelCombo);

    QLineEdit *customEdit = new QLineEdit(QStringLiteral("z0 + A*exp(-((x-xc)^2 + (y-yc)^2)/w^2)"),
                                          &dlg);
    customEdit->setEnabled(false);
    form->addRow(QObject::tr("Custom Expression:"), customEdit);

    QSpinBox *gridSpin = new QSpinBox(&dlg);
    gridSpin->setRange(8, 256);
    gridSpin->setValue(40);
    form->addRow(QObject::tr("Output Grid Size:"), gridSpin);

    mainLay->addLayout(form);

    auto refreshCols = [&]() {
        xCombo->clear();
        yCombo->clear();
        zCombo->clear();
        int ti = tableCombo->currentIndex();
        if (ti < 0 || ti >= tables.size())
            return;
        Table *t = tables[ti];
        for (int c = 0; c < t->numCols(); ++c) {
            QString cn = t->column(c)->name();
            xCombo->addItem(cn);
            yCombo->addItem(cn);
            zCombo->addItem(cn);
        }
        if (t->numCols() >= 2)
            yCombo->setCurrentIndex(1);
        if (t->numCols() >= 3)
            zCombo->setCurrentIndex(2);
    };
    refreshCols();

    auto updateSourceMode = [&](int idx) {
        bool isXYZ = (idx == 0);
        tableCombo->setEnabled(isXYZ);
        xCombo->setEnabled(isXYZ);
        yCombo->setEnabled(isXYZ);
        zCombo->setEnabled(isXYZ);
        gridSpin->setEnabled(isXYZ);
        matrixCombo->setEnabled(!isXYZ);
    };
    updateSourceMode(modeCombo->currentIndex());

    QObject::connect(tableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&](int) { refreshCols(); });
    QObject::connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     updateSourceMode);
    QObject::connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&](int idx) { customEdit->setEnabled(idx == static_cast<int>(Custom2D)); });

    // Dialog Theme bar
    QHBoxLayout *themeLay = new QHBoxLayout();
    QPushButton *saveThemeBtn = new QPushButton(QObject::tr("Save Theme..."), &dlg);
    QPushButton *loadThemeBtn = new QPushButton(QObject::tr("Load Theme..."), &dlg);
    themeLay->addWidget(saveThemeBtn);
    themeLay->addWidget(loadThemeBtn);
    themeLay->addStretch();
    mainLay->addLayout(themeLay);

    QObject::connect(saveThemeBtn, &QPushButton::clicked, [&]() {
        bool ok = false;
        QString name = QInputDialog::getText(&dlg, QObject::tr("Save Dialog Theme"),
                                             QObject::tr("Theme name:"), QLineEdit::Normal,
                                             QStringLiteral("Default2DFit"), &ok);
        if (ok && !name.trimmed().isEmpty()) {
            QVariantMap m;
            m[QStringLiteral("model")] = modelCombo->currentIndex();
            m[QStringLiteral("customExpr")] = customEdit->text();
            m[QStringLiteral("gridSide")] = gridSpin->value();
            DialogThemeManager::saveTheme(QStringLiteral("SurfaceFit"), name.trimmed(), m);
        }
    });
    QObject::connect(loadThemeBtn, &QPushButton::clicked, [&]() {
        QStringList names = DialogThemeManager::themeNames(QStringLiteral("SurfaceFit"));
        if (names.isEmpty())
            return;
        bool ok = false;
        QString chosen = QInputDialog::getItem(&dlg, QObject::tr("Load Dialog Theme"),
                                               QObject::tr("Theme:"), names, 0, false, &ok);
        if (ok && !chosen.isEmpty()) {
            QVariantMap m = DialogThemeManager::loadTheme(QStringLiteral("SurfaceFit"), chosen);
            if (m.contains(QStringLiteral("model")))
                modelCombo->setCurrentIndex(m.value(QStringLiteral("model")).toInt());
            if (m.contains(QStringLiteral("customExpr")))
                customEdit->setText(m.value(QStringLiteral("customExpr")).toString());
            if (m.contains(QStringLiteral("gridSide")))
                gridSpin->setValue(m.value(QStringLiteral("gridSide")).toInt());
        }
    });

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    mainLay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    SurfaceModel sm = static_cast<SurfaceModel>(modelCombo->currentIndex());
    SurfaceFitResult res;
    if (modeCombo->currentIndex() == 0) {
        int ti = tableCombo->currentIndex();
        if (ti < 0 || ti >= tables.size())
            return;
        res = fitSurfaceXYZ(app, tables[ti], xCombo->currentText(), yCombo->currentText(),
                            zCombo->currentText(), sm, customEdit->text(), gridSpin->value());
    } else {
        int mi = matrixCombo->currentIndex();
        if (mi < 0 || mi >= matrices.size())
            return;
        res = fitSurfaceMatrix(app, matrices[mi], sm, customEdit->text());
    }

    if (res.fittedMatrix)
        app->plotColorMap(res.fittedMatrix);
}

void GraphStatsAndSurfaceFit::show2DPeakDialog(ApplicationWindow *app)
{
    if (!app)
        return;
    QList<Matrix *> matrices = allProjectMatrices(app);
    if (matrices.isEmpty()) {
        QMessageBox::information(app, QObject::tr("2D Peak Analysis"),
                                 QObject::tr("Please create or activate a Matrix first."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("2D Peak Analysis (Matrix / Image)"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *matCombo = new QComboBox(&dlg);
    Matrix *actMat = resolveActiveMatrix(app);
    for (Matrix *m : matrices) {
        matCombo->addItem(m->name());
        if (m == actMat)
            matCombo->setCurrentIndex(matCombo->count() - 1);
    }
    form->addRow(QObject::tr("Source Matrix:"), matCombo);

    QSpinBox *maxSpin = new QSpinBox(&dlg);
    maxSpin->setRange(1, 200);
    maxSpin->setValue(10);
    form->addRow(QObject::tr("Maximum Peaks:"), maxSpin);

    QDoubleSpinBox *promSpin = new QDoubleSpinBox(&dlg);
    promSpin->setRange(0.001, 0.95);
    promSpin->setDecimals(3);
    promSpin->setSingleStep(0.02);
    promSpin->setValue(0.10);
    form->addRow(QObject::tr("Prominence Fraction:"), promSpin);

    QCheckBox *plotCheck = new QCheckBox(QObject::tr("Plot Color Map with Peak Overlay"), &dlg);
    plotCheck->setChecked(true);
    form->addRow(QString(), plotCheck);
    lay->addLayout(form);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    Matrix *chosen = matrices[matCombo->currentIndex()];
    Graph *overlay = nullptr;
    if (plotCheck->isChecked()) {
        MultiLayer *ml = app->plotColorMap(chosen);
        if (ml)
            overlay = ml->activeGraph();
    }
    analyze2DPeaks(app, chosen, maxSpin->value(), promSpin->value(), overlay);
}

void GraphStatsAndSurfaceFit::showConfidenceEllipseDialog(ApplicationWindow *app, Graph *g)
{
    if (!app)
        return;
    Graph *targetG = resolveActiveGraph(app, g);
    if (!targetG || targetG->curves() <= 0) {
        QMessageBox::information(app, QObject::tr("2D Confidence Ellipse"),
                                 QObject::tr("Please activate a Graph with a scatter/data curve."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("2D Confidence / Prediction Ellipse"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *curveCombo = new QComboBox(&dlg);
    curveCombo->addItems(targetG->analysableCurvesList());
    if (curveCombo->count() == 0)
        curveCombo->addItems(targetG->curvesList());
    form->addRow(QObject::tr("Target Curve:"), curveCombo);

    QDoubleSpinBox *confSpin = new QDoubleSpinBox(&dlg);
    confSpin->setRange(50.0, 99.9);
    confSpin->setDecimals(1);
    confSpin->setSuffix(QStringLiteral(" %"));
    confSpin->setValue(95.0);
    form->addRow(QObject::tr("Confidence Level:"), confSpin);

    QCheckBox *predCheck =
            new QCheckBox(QObject::tr("Prediction Ellipse (for new observation)"), &dlg);
    predCheck->setChecked(false);
    form->addRow(QString(), predCheck);
    lay->addLayout(form);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    addConfidenceEllipse(app, targetG, curveCombo->currentText(), confSpin->value() / 100.0,
                         predCheck->isChecked());
}

void GraphStatsAndSurfaceFit::showGroupComparisonDialog(ApplicationWindow *app, Graph *g)
{
    if (!app)
        return;
    QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    Graph *targetG = resolveActiveGraph(app, g);

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("t-Test / One-Way ANOVA & Significance Brackets"));
    dlg.setMinimumWidth(420);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *tableCombo = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        tableCombo->addItem(t->name());
        if (t == actTbl)
            tableCombo->setCurrentIndex(tableCombo->count() - 1);
    }
    form->addRow(QObject::tr("Source Table:"), tableCombo);

    QListWidget *colList = new QListWidget(&dlg);
    colList->setSelectionMode(QAbstractItemView::MultiSelection);
    form->addRow(QObject::tr("Group Columns (2+):"), colList);

    QCheckBox *annotCheck =
            new QCheckBox(QObject::tr("Add Significance Brackets (*/**/***) to Graph"), &dlg);
    annotCheck->setChecked(targetG != nullptr);
    annotCheck->setEnabled(targetG != nullptr);
    form->addRow(QString(), annotCheck);
    lay->addLayout(form);

    auto populateCols = [&]() {
        colList->clear();
        int idx = tableCombo->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            QListWidgetItem *it = new QListWidgetItem(t->column(c)->name(), colList);
            if (t->colPlotDesignation(c) == SciDAVis::Y || t->numCols() <= 3)
                it->setSelected(true);
        }
    };
    populateCols();
    QObject::connect(tableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&](int) { populateCols(); });

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList selected;
    for (QListWidgetItem *it : colList->selectedItems())
        selected.append(it->text());
    if (selected.size() < 2)
        return;

    runGroupComparisonAndAnnotate(app, tables[tableCombo->currentIndex()], selected,
                                  annotCheck->isChecked() ? targetG : nullptr);
}

void GraphStatsAndSurfaceFit::showROCDialog(ApplicationWindow *app)
{
    if (!app)
        return;
    QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("ROC Curve Analysis (AUC & Youden Cutoff)"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *tableCombo = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        tableCombo->addItem(t->name());
        if (t == actTbl)
            tableCombo->setCurrentIndex(tableCombo->count() - 1);
    }
    form->addRow(QObject::tr("Source Table:"), tableCombo);

    QComboBox *stateCombo = new QComboBox(&dlg);
    QComboBox *scoreCombo = new QComboBox(&dlg);
    form->addRow(QObject::tr("Binary State Column (0/1):"), stateCombo);
    form->addRow(QObject::tr("Test Score Column:"), scoreCombo);
    lay->addLayout(form);

    auto refreshCols = [&]() {
        stateCombo->clear();
        scoreCombo->clear();
        int idx = tableCombo->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            QString cn = t->column(c)->name();
            stateCombo->addItem(cn);
            scoreCombo->addItem(cn);
        }
        if (t->numCols() >= 2)
            scoreCombo->setCurrentIndex(1);
    };
    refreshCols();
    QObject::connect(tableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&](int) { refreshCols(); });

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    plotROCCurve(app, tables[tableCombo->currentIndex()], stateCombo->currentText(),
                 scoreCombo->currentText());
}

void GraphStatsAndSurfaceFit::showKaplanMeierDialog(ApplicationWindow *app)
{
    if (!app)
        return;
    QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Kaplan-Meier Survival Analysis & Log-Rank Test"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *tableCombo = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        tableCombo->addItem(t->name());
        if (t == actTbl)
            tableCombo->setCurrentIndex(tableCombo->count() - 1);
    }
    form->addRow(QObject::tr("Source Table:"), tableCombo);

    QComboBox *timeCombo = new QComboBox(&dlg);
    QComboBox *eventCombo = new QComboBox(&dlg);
    QComboBox *groupCombo = new QComboBox(&dlg);
    form->addRow(QObject::tr("Time Column:"), timeCombo);
    form->addRow(QObject::tr("Event/Censor Column (1=Event, 0=Censored):"), eventCombo);
    form->addRow(QObject::tr("Group Column (Optional):"), groupCombo);
    lay->addLayout(form);

    auto refreshCols = [&]() {
        timeCombo->clear();
        eventCombo->clear();
        groupCombo->clear();
        groupCombo->addItem(QObject::tr("(None - Single Cohort)"));
        int idx = tableCombo->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            QString cn = t->column(c)->name();
            timeCombo->addItem(cn);
            eventCombo->addItem(cn);
            groupCombo->addItem(cn);
        }
        if (t->numCols() >= 2)
            eventCombo->setCurrentIndex(1);
        if (t->numCols() >= 3)
            groupCombo->setCurrentIndex(3);
    };
    refreshCols();
    QObject::connect(tableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&](int) { refreshCols(); });

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString grpCol = (groupCombo->currentIndex() > 0) ? groupCombo->currentText() : QString();
    plotKaplanMeier(app, tables[tableCombo->currentIndex()], timeCombo->currentText(),
                    eventCombo->currentText(), grpCol);
}

void GraphStatsAndSurfaceFit::showPCADialog(ApplicationWindow *app)
{
    if (!app)
        return;
    QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Principal Component Analysis (PCA Biplot)"));
    dlg.setMinimumWidth(420);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *tableCombo = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        tableCombo->addItem(t->name());
        if (t == actTbl)
            tableCombo->setCurrentIndex(tableCombo->count() - 1);
    }
    form->addRow(QObject::tr("Source Table:"), tableCombo);

    QListWidget *colList = new QListWidget(&dlg);
    colList->setSelectionMode(QAbstractItemView::MultiSelection);
    form->addRow(QObject::tr("Variables (2+ Numeric Columns):"), colList);
    lay->addLayout(form);

    auto populateCols = [&]() {
        colList->clear();
        int idx = tableCombo->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            if (t->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric) {
                QListWidgetItem *it = new QListWidgetItem(t->column(c)->name(), colList);
                it->setSelected(true);
            }
        }
    };
    populateCols();
    QObject::connect(tableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&](int) { populateCols(); });

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList selected;
    for (QListWidgetItem *it : colList->selectedItems())
        selected.append(it->text());
    if (selected.size() < 2)
        return;

    runPCAAndBiplot(app, tables[tableCombo->currentIndex()], selected);
}
