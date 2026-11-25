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
#include "ExtraPlotTypes.h"
#include "Graph.h"
#include "I18nJapanese.h"
#include "Fit.h"
#include "Legend.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "MyParser.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "QwtHistogram.h"
#include "RecalcEngine.h"
#include "ScaleDraw.h"
#include "Spectrogram.h"
#include "Table.h"
#include "core/column/Column.h"

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>

#include <gsl/gsl_blas.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_eigen.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_multifit_nlin.h>
#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_version.h>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
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
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <tuple>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kFwhmFactor = 2.3548200450309493820231386529194; // 2 * sqrt(2 * ln(2))

bool isNonInteractiveApp(const ApplicationWindow *app)
{
    if (qApp && qApp->property("torigraph_noninteractive").toBool())
        return true;
    return app && app->isHeadlessMode();
}

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

bool readNumericColumnCell(const Column *col, int row, double &val)
{
    if (!col || row < 0 || row >= col->rowCount() || col->isInvalid(row) || col->isMasked(row))
        return false;
    if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
        const double v = col->valueAt(row);
        if (!std::isfinite(v))
            return false;
        val = v;
        return true;
    }
    if (col->asStringColumn()) {
        const QString s = col->asStringColumn()->textAt(row).trimmed();
        if (s.isEmpty())
            return false;
        bool ok = false;
        double v = s.toDouble(&ok);
        if (!ok)
            v = QLocale::c().toDouble(s, &ok);
        if (ok && std::isfinite(v)) {
            val = v;
            return true;
        }
    }
    return false;
}

bool columnHasNumericValues(const Column *col)
{
    if (!col)
        return false;
    if (col->columnMode() == SciDAVis::ColumnMode::Numeric)
        return true;
    double dummy = 0.0;
    const int n = col->rowCount();
    for (int r = 0; r < n; ++r) {
        if (readNumericColumnCell(col, r, dummy))
            return true;
    }
    return false;
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
    auto tableFromGraph = [&](Graph *g) -> Table * {
        if (!g)
            return nullptr;
        const QString sel = g->selectedCurveTitle();
        if (!sel.isEmpty()) {
            if (auto *dc = dynamic_cast<DataCurve *>(g->curve(sel))) {
                if (dc->table())
                    return dc->table();
            }
        }
        Table *fallback = nullptr;
        for (int i = 0; i < g->curves(); ++i) {
            auto *dc = dynamic_cast<DataCurve *>(g->curve(i));
            if (!dc || !dc->table())
                continue;
            if (!dc->table()->name().startsWith(QLatin1String("GadgetFit_")))
                return dc->table();
            if (!fallback)
                fallback = dc->table();
        }
        if (fallback)
            return fallback;
        for (const QString &kind : { QStringLiteral("ToriDensity2D"), QStringLiteral("ToriPoleFigure") }) {
            const QString tag = ExtraPlotTypes::graphTag(g, kind);
            if (!tag.isEmpty()) {
                static const QRegularExpression rxTbl(QStringLiteral("table=\"([^\"]+)\""));
                const QRegularExpressionMatch m = rxTbl.match(tag);
                if (m.hasMatch()) {
                    if (Table *t = app->table(m.captured(1)))
                        return t;
                }
            }
        }
        return nullptr;
    };
    QMdiSubWindow *sub = app->d_workspace.activeSubWindow();
    if (sub) {
        if (sub->inherits("Table"))
            return static_cast<Table *>(sub);
        if (QWidget *w = sub->widget()) {
            if (w->inherits("Table"))
                return static_cast<Table *>(w);
        }
        MultiLayer *ml = nullptr;
        if (sub->inherits("MultiLayer"))
            ml = static_cast<MultiLayer *>(sub);
        else if (sub->widget() && sub->widget()->inherits("MultiLayer"))
            ml = static_cast<MultiLayer *>(sub->widget());
        if (ml) {
            if (Table *gt = tableFromGraph(ml->activeGraph()))
                return gt;
            for (Graph *lg : ml->layersList()) {
                if (Table *gt = tableFromGraph(lg))
                    return gt;
            }
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

void applyArialGraphFonts(Graph *g)
{
    if (!g)
        return;
    auto normArial = [](QFont f, double fallbackPt) -> QFont {
        const double pt = (f.pointSizeF() > 0.0)
                ? f.pointSizeF()
                : ((f.pointSize() > 0) ? double(f.pointSize()) : fallbackPt);
        QFont out(QStringLiteral("Arial"));
        out.setPointSizeF(pt);
        out.setWeight(QFont::Normal);
        out.setBold(false);
        out.setStyleStrategy(f.styleStrategy());
        return out;
    };
    if (g->plotWidget()) {
        QwtText topT = g->plotWidget()->title();
        topT.setFont(normArial(topT.font(), 11));
        g->plotWidget()->setTitle(topT);
        for (int ax = 0; ax < QwtPlot::axisCnt; ++ax) {
            g->plotWidget()->setAxisFont(ax, normArial(g->plotWidget()->axisFont(ax), 10));
            QwtText axTitle = g->plotWidget()->axisTitle(ax);
            axTitle.setFont(normArial(axTitle.font(), 11));
            g->plotWidget()->setAxisTitle(ax, axTitle);
        }
    }
    if (g->legend())
        g->legend()->setFont(normArial(g->legend()->font(), 9));
    g->setTextMarkerDefaults(g->textMarkerDefaultFrame(),
                             normArial(g->defaultTextMarkerFont(), 9),
                             g->textMarkerDefaultColor(),
                             g->textMarkerDefaultBackground());
}

Legend *addTextLabelOnGraph(Graph *g, double x, double y, const QString &text,
                            const QColor &color = QColor(20, 20, 20), bool bold = false,
                            int pointSizeDelta = 0)
{
    Q_UNUSED(bold);
    if (!g)
        return nullptr;
    Legend *lbl = g->newLegend(text);
    if (!lbl)
        return nullptr;
    lbl->setFrameStyle(Legend::None);
    lbl->setBackgroundColor(QColor(255, 255, 255, 0));
    lbl->setTextColor(color);
    QFont f = g->defaultTextMarkerFont();
    double ptSize = (f.pointSizeF() > 0.0)
            ? f.pointSizeF()
            : ((f.pointSize() > 0) ? double(f.pointSize()) : 0.0);
    if (ptSize <= 0.0 && g->plotWidget()) {
        QFont axF = g->plotWidget()->axisFont(QwtPlot::xBottom);
        ptSize = (axF.pointSizeF() > 0.0) ? axF.pointSizeF()
                                          : ((axF.pointSize() > 0) ? double(axF.pointSize()) : 10.0);
        if (f.family().isEmpty())
            f.setFamily(axF.family());
    }
    if (ptSize <= 0.0)
        ptSize = 10.0;
    if (f.family().isEmpty())
        f.setFamily(QStringLiteral("Arial"));
    if (pointSizeDelta != 0 && ptSize + pointSizeDelta >= 6.0)
        ptSize += pointSizeDelta;
    f.setPointSizeF(ptSize);
    f.setWeight(QFont::Normal);
    f.setBold(false);
    lbl->setFont(f);
    lbl->setOriginCoord(x, y);
    return lbl;
}

double statsClipSegmentLengthInRect(double x0, double y0, double x1, double y1, const QRectF &r,
                                    bool *outHit = nullptr)
{
    if (outHit)
        *outHit = false;
    double t0 = 0.0, t1 = 1.0;
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    auto clipTest = [&](double p, double q) -> bool {
        if (std::fabs(p) <= 1e-12)
            return q >= 0.0;
        const double t = q / p;
        if (p < 0.0) {
            if (t > t1)
                return false;
            if (t > t0)
                t0 = t;
        } else {
            if (t < t0)
                return false;
            if (t < t1)
                t1 = t;
        }
        return true;
    };
    if (!clipTest(-dx, x0 - r.left()) || !clipTest(dx, r.right() - x0)
        || !clipTest(-dy, y0 - r.top()) || !clipTest(dy, r.bottom() - y0)) {
        return 0.0;
    }
    if (t1 < t0)
        return 0.0;
    if (outHit)
        *outHit = true;
    return std::hypot(dx * (t1 - t0), dy * (t1 - t0));
}

void placeStatsCalloutAutoAvoid(Graph *g, Legend *box, int marginPx = 14)
{
    if (!g || !box)
        return;
    Plot *pw = g->plotWidget();
    if (!pw || !pw->canvas())
        return;

    g->replot();
    QRect cRect = pw->canvas()->rect();
    QRect bRect = box->rect();
    const int majTickLen = std::max(0, g->majorTickLength());
    const int pad = std::max({ 14, marginPx, majTickLen + 6 });
    QFont fnt = box->font();
    if (fnt.bold()) {
        fnt.setBold(false);
        box->setFont(fnt);
    }
    while (cRect.width() > 40 && bRect.width() > cRect.width() - 2 * pad && fnt.pointSize() > 7) {
        fnt.setPointSize(fnt.pointSize() - 1);
        box->setFont(fnt);
        g->replot();
        bRect = box->rect();
    }

    auto solvePlacement = [&](int &outBestL, int &outBestT, double &outBestScore) -> bool {
        cRect = pw->canvas()->rect();
        bRect = box->rect();
        const int bw = bRect.width();
        const int bh = bRect.height();
        const int cw = cRect.width();
        const int ch = cRect.height();
        if (bw <= 0 || bh <= 0 || cw <= 2 * pad + 4 || ch <= 2 * pad + 4)
            return false;

        const int minL = pad;
        const int maxL = std::max(minL, cw - bw - pad);
        const int minT = pad;
        const int maxT = std::max(minT, ch - bh - pad);

        struct CandPos {
            int left = 0;
            int top = 0;
            double prefPenalty = 0.0;
        };
        QVector<CandPos> cands;
        cands.reserve(40);
        cands.append({ minL, minT, 0.0 });
        cands.append({ maxL, maxT, 0.1 });
        cands.append({ minL, maxT, 0.2 });
        cands.append({ maxL, minT, 0.3 });
        cands.append({ maxL, (minT + maxT) / 2, 0.5 });
        cands.append({ minL, (minT + maxT) / 2, 0.6 });
        cands.append({ (minL + maxL) / 2, minT, 0.7 });
        cands.append({ (minL + maxL) / 2, maxT, 0.8 });
        for (int gy = 0; gy < 6; ++gy) {
            for (int gx = 0; gx < 6; ++gx) {
                const int lx = minL + qRound((maxL - minL) * (double(gx) / 5.0));
                const int ty = minT + qRound((maxT - minT) * (double(gy) / 5.0));
                cands.append({ lx, ty, 1.0 + 0.08 * std::min(gx, 5 - gx) + 0.08 * std::min(gy, 5 - gy) });
            }
        }

        const Legend *mainLeg = (g->isLegendVisible() ? g->legend() : nullptr);
        const QRectF legRect =
                (mainLeg && mainLeg != box) ? QRectF(mainLeg->rect()).adjusted(-5, -5, 5, 5) : QRectF();

        auto evalCost = [&](int lx, int ty) -> double {
            double cost = 0.0;
            const QRectF testBox(lx - 4.0, ty - 4.0, bw + 8.0, bh + 8.0);
            if (legRect.isValid()) {
                const QRectF inter = testBox.intersected(legRect);
                if (!inter.isEmpty())
                    cost += 200000.0 + inter.width() * inter.height() * 60.0;
            }
            for (int tid : g->textMarkerKeys()) {
                Legend *tm = g->textMarker(tid);
                if (!tm || tm == box || tm == mainLeg || !tm->isVisible())
                    continue;
                const QRectF inter = testBox.intersected(QRectF(tm->rect()).adjusted(-3, -3, 3, 3));
                if (!inter.isEmpty())
                    cost += 80000.0 + inter.width() * inter.height() * 30.0;
            }
            for (int aid : g->lineMarkerKeys()) {
                ArrowMarker *am = g->arrow(aid);
                if (!am)
                    continue;
                const QwtScaleMap xMap = pw->canvasMap(am->xAxis());
                const QwtScaleMap yMap = pw->canvasMap(am->yAxis());
                const QPointF sp = am->startPointCoord();
                const QPointF ep = am->endPointCoord();
                bool hit = false;
                const double len = statsClipSegmentLengthInRect(
                        xMap.xTransform(sp.x()), yMap.xTransform(sp.y()),
                        xMap.xTransform(ep.x()), yMap.xTransform(ep.y()), testBox, &hit);
                if (hit)
                    cost += 60.0 + len * 3.5;
            }
            for (const Graph::ReferenceLineSpec &rl : g->referenceLines()) {
                if (!rl.visible)
                    continue;
                if (rl.orientation == 0) {
                    const double py = pw->canvasMap(QwtPlot::yLeft).xTransform(rl.value);
                    if (py >= testBox.top() && py <= testBox.bottom())
                        cost += 4000.0;
                } else {
                    const double px = pw->canvasMap(QwtPlot::xBottom).xTransform(rl.value);
                    if (px >= testBox.left() && px <= testBox.right())
                        cost += 4000.0;
                }
            }
            for (int i = 0; i < g->curves(); ++i) {
                QwtPlotCurve *c = g->curve(i);
                if (!c || !c->isVisible() || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
                    continue;
                const int nPts = c->dataSize();
                if (nPts <= 0)
                    continue;
                const QwtScaleMap xMap = pw->canvasMap(c->xAxis());
                const QwtScaleMap yMap = pw->canvasMap(c->yAxis());
                const bool hasSym = (c->symbol().style() != QwtSymbol::NoSymbol);
                const bool hasLine = (c->style() != QwtPlotCurve::NoCurve);
                const int stride = (nPts > 4000) ? std::max(1, nPts / 2000) : 1;
                bool hasPrev = false;
                double ppx = 0.0, ppy = 0.0;
                for (int p = 0; p < nPts; p += stride) {
                    const double vx = c->x(p), vy = c->y(p);
                    if (!std::isfinite(vx) || !std::isfinite(vy)) {
                        hasPrev = false;
                        continue;
                    }
                    const double px = xMap.xTransform(vx);
                    const double py = yMap.xTransform(vy);
                    if (!std::isfinite(px) || !std::isfinite(py)) {
                        hasPrev = false;
                        continue;
                    }
                    if (hasSym && testBox.contains(QPointF(px, py)))
                        cost += 90.0 * stride;
                    if (hasLine && hasPrev) {
                        bool hit = false;
                        const double len = statsClipSegmentLengthInRect(ppx, ppy, px, py, testBox, &hit);
                        if (hit)
                            cost += (25.0 + len * 4.5) * stride;
                    }
                    ppx = px;
                    ppy = py;
                    hasPrev = true;
                }
            }
            return cost;
        };

        outBestScore = std::numeric_limits<double>::infinity();
        outBestL = std::clamp(bRect.left(), minL, maxL);
        outBestT = std::clamp(bRect.top(), minT, maxT);
        for (const CandPos &cp : cands) {
            const double sc = evalCost(cp.left, cp.top) + cp.prefPenalty;
            if (sc < outBestScore) {
                outBestScore = sc;
                outBestL = cp.left;
                outBestT = cp.top;
            }
        }
        return true;
    };

    int bestL = 0, bestT = 0;
    double bestScore = 0.0;
    if (!solvePlacement(bestL, bestT, bestScore))
        return;

    while (bestScore >= 25.0 && fnt.pointSize() > 7) {
        fnt.setPointSize(fnt.pointSize() - 1);
        box->setFont(fnt);
        g->replot();
        if (!solvePlacement(bestL, bestT, bestScore))
            break;
    }

    const double newX = pw->invTransform(QwtPlot::xBottom, bestL);
    const double newY = pw->invTransform(QwtPlot::yLeft, bestT);
    if (std::isfinite(newX) && std::isfinite(newY))
        box->setOriginCoord(newX, newY);
}

void attachStatsCalloutAutoAvoid(Graph *g, Legend *box, int marginPx = 14)
{
    if (!g || !box)
        return;
    placeStatsCalloutAutoAvoid(g, box, marginPx);
    int markerId = -1;
    for (int tid : g->textMarkerKeys()) {
        if (g->textMarker(tid) == box) {
            markerId = tid;
            break;
        }
    }
    if (markerId >= 0) {
        auto guard = std::make_shared<bool>(false);
        QPointer<Graph> gPtr(g);
        QObject::connect(g, &Graph::modifiedGraph, g, [gPtr, markerId, marginPx, guard]() {
            if (*guard || !gPtr)
                return;
            Legend *liveBox = gPtr->textMarker(markerId);
            if (!liveBox || !liveBox->isVisible())
                return;
            *guard = true;
            placeStatsCalloutAutoAvoid(gPtr.data(), liveBox, marginPx);
            *guard = false;
        });
    }
}

Legend *addFramedBoxOnGraph(Graph *g, double x, double y, const QString &text,
                            const QColor &textColor = QColor(20, 30, 50), int ptSize = 9)
{
    if (!g)
        return nullptr;
    Legend *box = g->newLegend(text);
    if (!box)
        return nullptr;
    const int basePt = (g->legend() && g->legend()->font().pointSize() > 0)
            ? g->legend()->font().pointSize()
            : ptSize;
    QFont f(QStringLiteral("Arial"), std::clamp(basePt, 8, 22), QFont::Normal);
    f.setBold(false);
    box->setFrameStyle(Legend::Line);
    box->setBackgroundColor(QColor(255, 255, 255, 232));
    box->setTextColor(textColor);
    box->setFont(f);
    box->setOriginCoord(x, y);
    return box;
}

enum class BracketLabelFormat { StarsOnly = 0, StarsAndPValue = 1, PValueOnly = 2 };

BracketLabelFormat parseBracketLabelFormat(const Table *srcTable)
{
    if (!srcTable)
        return BracketLabelFormat::StarsOnly;
    const QString fmt =
            srcTable->property("torigraph_anova_bracket_format").toString().trimmed().toLower();
    if (fmt == QStringLiteral("stars_p") || fmt == QStringLiteral("stars_and_p")
        || fmt == QStringLiteral("both"))
        return BracketLabelFormat::StarsAndPValue;
    if (fmt == QStringLiteral("p") || fmt == QStringLiteral("pvalue")
        || fmt == QStringLiteral("p_only"))
        return BracketLabelFormat::PValueOnly;
    return BracketLabelFormat::StarsOnly;
}

void annotateBracketsOnGraph(Graph *annotateGraph, int K, double yMinAll, double yMaxAll,
                             const std::vector<std::tuple<int, int, double>> &sigPairs,
                             const QStringList &groupNames = QStringList(),
                             Table *srcTable = nullptr,
                             BracketLabelFormat labelFormat = BracketLabelFormat::StarsOnly);

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
    mutable bool evalFailed = false;
    mutable QString parserError;

    bool ensureParser() const
    {
        if (model != GraphStatsAndSurfaceFit::Custom2D)
            return true;
        if (evalFailed)
            return false;
        if (parserReady && parser)
            return true;
        if (customExpr.trimmed().isEmpty()) {
            parserError = QStringLiteral("Empty custom 2D formula");
            evalFailed = true;
            parserReady = false;
            return false;
        }
        parser = std::make_unique<MyParser>();
        varParams.assign(static_cast<size_t>(paramNames.size()), 1.0);
        varX = !x.empty() ? x.front() : (0.5 * (xMin + xMax));
        varY = !y.empty() ? y.front() : (0.5 * (yMin + yMax));
        try {
            parser->DefineVar(toString<mu::string_type>(QStringLiteral("x")), &varX);
            parser->DefineVar(toString<mu::string_type>(QStringLiteral("y")), &varY);
            for (int j = 0; j < paramNames.size(); ++j) {
                parser->DefineVar(toString<mu::string_type>(paramNames[j]),
                                  &varParams[static_cast<size_t>(j)]);
            }
            parser->SetExpr(customExpr);
            (void)parser->Eval();
            parserReady = true;
            return true;
        } catch (const mu::ParserError &e) {
            parserError = QStringFromString(e.GetMsg());
            evalFailed = true;
            parserReady = false;
            return false;
        } catch (...) {
            parserError = QStringLiteral("Invalid custom 2D formula");
            evalFailed = true;
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
            const double xLo = std::min(xMin, xMax);
            const double xHi = std::max(xMin, xMax);
            const double yLo = std::min(yMin, yMax);
            const double yHi = std::max(yMin, yMax);
            const double spanX = (xHi > xLo) ? (xHi - xLo) : 1.0;
            const double spanY = (yHi > yLo) ? (yHi - yLo) : 1.0;
            const double tx = 2.0 * (px - xLo) / spanX - 1.0;
            const double ty = 2.0 * (py - yLo) / spanY - 1.0;
            const double T1x = tx;
            const double T2x = 2.0 * tx * tx - 1.0;
            const double T1y = ty;
            const double T2y = 2.0 * ty * ty - 1.0;
            return p[0] + p[1] * T1x + p[2] * T1y + p[3] * T2x + p[4] * T1x * T1y + p[5] * T2y;
        }

        case GraphStatsAndSurfaceFit::Custom2D: {
            if (!ensureParser())
                return std::numeric_limits<double>::quiet_NaN();
            varX = px;
            varY = py;
            for (size_t j = 0; j < varParams.size(); ++j)
                varParams[j] = p[j];
            try {
                return parser->Eval();
            } catch (const mu::ParserError &e) {
                parserError = QStringFromString(e.GetMsg());
                evalFailed = true;
                return std::numeric_limits<double>::quiet_NaN();
            } catch (...) {
                evalFailed = true;
                return std::numeric_limits<double>::quiet_NaN();
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
        if (ctx->evalFailed || !std::isfinite(zHat))
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
        for (size_t i = 0; i < n; ++i) {
            fPlus[i] = ctx->evalPoint(ctx->x[i], ctx->y[i], par.data());
            if (ctx->evalFailed || !std::isfinite(fPlus[i]))
                return GSL_EDOM;
        }

        par[j] = pj - h;
        for (size_t i = 0; i < n; ++i) {
            double fMinus = ctx->evalPoint(ctx->x[i], ctx->y[i], par.data());
            if (ctx->evalFailed || !std::isfinite(fMinus))
                return GSL_EDOM;
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

    const double xLo = std::min(ctx.xMin, ctx.xMax);
    const double xHi = std::max(ctx.xMin, ctx.xMax);
    const double yLo = std::min(ctx.yMin, ctx.yMax);
    const double yHi = std::max(ctx.yMin, ctx.yMax);
    const double spanX = std::max(1e-6, xHi - xLo);
    const double spanY = std::max(1e-6, yHi - yLo);

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
        const double chebSpanX = (xHi > xLo) ? (xHi - xLo) : 1.0;
        const double chebSpanY = (yHi > yLo) ? (yHi - yLo) : 1.0;
        std::vector<std::vector<double>> basis(n, std::vector<double>(6));
        for (size_t i = 0; i < n; ++i) {
            double tx = 2.0 * (ctx.x[i] - xLo) / chebSpanX - 1.0;
            double ty = 2.0 * (ctx.y[i] - yLo) / chebSpanY - 1.0;
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
    Fit::registerJapaneseStrings();
    app->setProperty("torigraph_last_surface_fit_error", QVariant());

    std::vector<double> initParams;
    buildInitialSurfaceParameters(ctx, initParams);
    const size_t p = static_cast<size_t>(ctx.paramNames.size());
    const size_t n = ctx.z.size();
    if (p == 0 || n < p)
        return out;
    if (ctx.model == GraphStatsAndSurfaceFit::Custom2D && !ctx.ensureParser()) {
        app->setProperty("torigraph_last_surface_fit_error",
                         ctx.parserError.isEmpty() ? QStringLiteral("Invalid custom 2D formula")
                                                   : ctx.parserError);
        if (app->writeFitResultsToLog && !isNonInteractiveApp(app))
            app->updateLog(QStringLiteral("[3D Surface Fit Error: ") + baseName
                           + QStringLiteral("] ") + ctx.parserError + QStringLiteral("\n"));
        return out;
    }

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
    bool hitMaxIterations = false;
    const int kMaxSurfaceIterations = 400;
    if (status == GSL_SUCCESS) {
        bool deltaConverged = false;
        for (iter = 0; iter < kMaxSurfaceIterations; ++iter) {
            status = gsl_multifit_fdfsolver_iterate(solver);
            if (ctx.evalFailed) {
                status = GSL_EDOM;
                break;
            }
            if (status != GSL_SUCCESS && status != GSL_CONTINUE)
                break;
            // Relative step test (Fit::lmStepConverged): the absolute part 1e-8 of
            // gsl_multifit_test_delta alone took any step of parameters of size 1e-22 for
            // convergence.
            if (Fit::lmStepConverged(solver, solver->dx, solver->x, 1e-8)) {
                status = GSL_SUCCESS;
                deltaConverged = true;
                break;
            }
        }
        // Running out of iterations is not convergence, however good R^2 looks.
        if (!deltaConverged && iter >= kMaxSurfaceIterations
            && (status == GSL_SUCCESS || status == GSL_CONTINUE)) {
            hitMaxIterations = true;
            status = GSL_SUCCESS;
        }
    }
    out.iterations = iter;

    std::vector<double> bestP(p, 0.0);
    for (size_t j = 0; j < p; ++j)
        bestP[j] = gsl_vector_get(solver->x, j);

    double jacMaxAbs = 0.0;
    gsl_matrix *covar = gsl_matrix_alloc(p, p);
    gsl_matrix_set_zero(covar);
    const bool covarComputed = (status != GSL_EDOM && status != GSL_EINVAL && !ctx.evalFailed);
    if (covarComputed) {
#if GSL_MAJOR_VERSION < 2
        for (size_t r = 0; r < n; ++r) {
            for (size_t c = 0; c < p; ++c) {
                double jv = std::abs(gsl_matrix_get(solver->J, r, c));
                if (std::isfinite(jv))
                    jacMaxAbs = std::max(jacMaxAbs, jv);
            }
        }
        gsl_multifit_covar(solver->J, 1e-8, covar);
#else
        gsl_matrix *J = gsl_matrix_alloc(n, p);
        gsl_multifit_fdfsolver_jac(solver, J);
        for (size_t r = 0; r < n; ++r) {
            for (size_t c = 0; c < p; ++c) {
                double jv = std::abs(gsl_matrix_get(J, r, c));
                if (std::isfinite(jv))
                    jacMaxAbs = std::max(jacMaxAbs, jv);
            }
        }
        gsl_multifit_covar(J, 1e-8, covar);
        gsl_matrix_free(J);
#endif
    }

    double rss = 0.0;
    const double meanZ = std::accumulate(ctx.z.begin(), ctx.z.end(), 0.0) / static_cast<double>(n);
    double tss = 0.0;
    double sumSqZ = 0.0;
    bool allEvalFinite = !ctx.evalFailed;
    for (size_t i = 0; i < n; ++i) {
        double zHat = ctx.evalPoint(ctx.x[i], ctx.y[i], bestP.data());
        if (!std::isfinite(zHat) || ctx.evalFailed) {
            allEvalFinite = false;
            break;
        }
        double r = ctx.z[i] - zHat;
        rss += r * r;
        double dz = ctx.z[i] - meanZ;
        tss += dz * dz;
        sumSqZ += ctx.z[i] * ctx.z[i];
    }

    if (!allEvalFinite || !std::isfinite(rss)) {
        gsl_matrix_free(covar);
        gsl_multifit_fdfsolver_free(solver);
        gsl_vector_free(xInit);
        app->setProperty("torigraph_last_surface_fit_error",
                         ctx.parserError.isEmpty()
                                 ? QStringLiteral("Surface formula evaluation failed")
                                 : ctx.parserError);
        out.converged = false;
        out.rSquare = 0.0;
        out.adjRSquare = 0.0;
        return out;
    }

    // "tss = 0" / "rss = 0" relative to the size of the data (1e-30 / 1e-24 made every surface
    // of size 1e-22 a perfect fit)
    const bool tssZero = Fit::sumOfSquaresNegligible(tss, sumSqZ);
    const bool rssZero = Fit::sumOfSquaresNegligible(rss, sumSqZ);
    const bool nonTrivialJacobian = (jacMaxAbs > 1e-30) || rssZero;
    bool noProgress = false;
    if ((status == GSL_ETOLF || status == GSL_ETOLX || status == GSL_ETOLG) && nonTrivialJacobian) {
        status = GSL_SUCCESS;
    } else if (status == GSL_ENOPROG && nonTrivialJacobian) {
        // ENOPROG only counts as converged at a stationary point of the residual norm.
        if (!Fit::lmGradientIsStationary(solver))
            noProgress = true;
        status = GSL_SUCCESS;
    } else if (status == GSL_SUCCESS && !nonTrivialJacobian) {
        status = GSL_ETOLG;
    }

    const double nanVal = std::numeric_limits<double>::quiet_NaN();
    const bool hasDof = (n > p);
    const double dof = hasDof ? static_cast<double>(n - p) : 0.0;
    out.reducedChi2 = hasDof ? (rss / dof) : nanVal;
    out.rSquare = !tssZero ? std::max(0.0, 1.0 - rss / tss) : (rssZero ? 1.0 : nanVal);
    out.adjRSquare = hasDof
            ? (std::isfinite(out.rSquare)
                       ? (!tssZero
                                  ? (1.0 - (1.0 - out.rSquare) * static_cast<double>(n - 1) / dof)
                                  : out.rSquare)
                       : nanVal)
            : nanVal;
    out.converged = nonTrivialJacobian && status == GSL_SUCCESS && !hitMaxIterations && !noProgress;
    if (hitMaxIterations)
        out.convergenceNote = QObject::tr(
                "Not converged (maximum iterations reached; check the initial values)");
    else if (noProgress)
        out.convergenceNote =
                QObject::tr("Not converged (no further improvement; check the initial values)");
    else if (!out.converged)
        out.convergenceNote = QObject::tr("Fit failed (%1)").arg(QString::fromLatin1(gsl_strerror(status)));
    out.paramNames = ctx.paramNames;
    out.params.resize(static_cast<int>(p));
    out.errors.resize(static_cast<int>(p));

    for (size_t j = 0; j < p; ++j) {
        out.params[static_cast<int>(j)] = bestP[j];
        if (!hasDof || !covarComputed) {
            out.errors[static_cast<int>(j)] = nanVal;
        } else {
            double cjj = gsl_matrix_get(covar, j, j);
            if (!std::isfinite(cjj) || cjj <= 0.0 || !std::isfinite(out.reducedChi2)
                || out.reducedChi2 < 0.0) {
                out.errors[static_cast<int>(j)] = nanVal;
            } else {
                double se = std::sqrt(out.reducedChi2 * cjj);
                out.errors[static_cast<int>(j)] = std::isfinite(se) ? se : nanVal;
            }
        }
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
        pt->setHeader(QStringList() << QStringLiteral("Parameter") << QStringLiteral("Value")
                                    << QStringLiteral("StdError"));
        const QStringList longNames = { QObject::tr("Parameter"), QObject::tr("Value"),
                                        QObject::tr("StdError") };
        pt->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        pt->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        pt->column(2)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        pt->column(2)->setPlotDesignation(SciDAVis::yErr);
        for (int c = 0; c < 3; ++c)
            pt->column(c)->setLongName(longNames[c]);

        auto setValOrInvalid = [&](int row, int col, double val) {
            if (std::isfinite(val))
                pt->column(col)->setValueAt(row, val);
            else
                pt->column(col)->setInvalid(row, true);
        };
        for (int j = 0; j < static_cast<int>(p); ++j) {
            pt->column(0)->setTextAt(j, out.paramNames[j]);
            setValOrInvalid(j, 1, out.params[j]);
            setValOrInvalid(j, 2, out.errors[j]);
        }
        int baseRow = static_cast<int>(p);
        if (!out.convergenceNote.isEmpty())
            pt->setWindowLabel(out.convergenceNote);
        pt->column(0)->setTextAt(baseRow, QStringLiteral("R^2"));
        setValOrInvalid(baseRow, 1, out.rSquare);
        pt->column(2)->setValueAt(baseRow, 0.0);

        pt->column(0)->setTextAt(baseRow + 1, QStringLiteral("Adj_R^2"));
        setValOrInvalid(baseRow + 1, 1, out.adjRSquare);
        pt->column(2)->setValueAt(baseRow + 1, 0.0);

        pt->column(0)->setTextAt(baseRow + 2, QStringLiteral("Reduced_Chi^2"));
        setValOrInvalid(baseRow + 2, 1, out.reducedChi2);
        pt->column(2)->setValueAt(baseRow + 2, 0.0);
        out.paramTable = pt;
    }

    if (app->writeFitResultsToLog) {
        QString log = QStringLiteral("[3D Surface Fit: ") + baseName + QStringLiteral("] ")
                + (out.convergenceNote.isEmpty() ? QString()
                                                 : (QStringLiteral("*** ") + out.convergenceNote
                                                    + QStringLiteral(" *** ")))
                + QStringLiteral("R^2 = ") + QString::number(out.rSquare, 'f', 6)
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

void computeShapiroWilkRoyston(const std::vector<double> &sortedX, double ss,
                               double &wOut, double &pOut)
{
    const int nFull = static_cast<int>(sortedX.size());
    if (nFull < 3 || ss <= 1e-30) {
        wOut = 0.0;
        pOut = 0.0;
        return;
    }

    // Subsample evenly if n > 5000 (AS R94 domain is 3 <= n <= 5000)
    std::vector<double> xSub;
    const std::vector<double> *xPtr = &sortedX;
    if (nFull > 5000) {
        xSub.resize(5000);
        for (int i = 0; i < 5000; ++i) {
            const size_t idx = static_cast<size_t>(
                    std::round(static_cast<double>(i) * static_cast<double>(nFull - 1) / 4999.0));
            xSub[static_cast<size_t>(i)] = sortedX[std::min(sortedX.size() - 1, idx)];
        }
        xPtr = &xSub;
    }

    const std::vector<double> &x = *xPtr;
    const int n = static_cast<int>(x.size());

    double ssUse = ss;
    if (n != nFull) {
        const double mSub = std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(n);
        ssUse = 0.0;
        for (double v : x) {
            const double d = v - mSub;
            ssUse += d * d;
        }
        if (ssUse <= 1e-30) {
            wOut = 0.0;
            pOut = 0.0;
            return;
        }
    }

    if (n == 3) {
        const double a1 = 0.70710678118654752440;
        const double num = a1 * (x[2] - x[0]);
        double w = (num * num) / ssUse;
        w = std::clamp(w, 0.75, 1.0);
        wOut = w;
        const double p = (6.0 / kPi) * (std::asin(std::sqrt(w)) - std::asin(std::sqrt(0.75)));
        pOut = std::clamp(p, 0.0, 1.0);
        return;
    }

    std::vector<double> m(static_cast<size_t>(n), 0.0);
    double sumM2 = 0.0;
    for (int i = 0; i < n; ++i) {
        const double prob = (static_cast<double>(i + 1) - 0.375) / (static_cast<double>(n) + 0.25);
        m[static_cast<size_t>(i)] = gsl_cdf_ugaussian_Pinv(std::clamp(prob, 1e-12, 1.0 - 1e-12));
        sumM2 += m[static_cast<size_t>(i)] * m[static_cast<size_t>(i)];
    }
    const double normM = std::sqrt(std::max(1e-30, sumM2));
    const double cn = m[static_cast<size_t>(n - 1)] / normM;
    const double cn1 = m[static_cast<size_t>(n - 2)] / normM;
    const double u = 1.0 / std::sqrt(static_cast<double>(n));
    const double u2 = u * u;
    const double u3 = u2 * u;
    const double u4 = u3 * u;
    const double u5 = u4 * u;

    std::vector<double> a(static_cast<size_t>(n), 0.0);
    const double an = -2.706056 * u5 + 4.434685 * u4 - 2.071190 * u3 - 0.147981 * u2
            + 0.221157 * u + cn;
    a[static_cast<size_t>(n - 1)] = an;
    a[0] = -an;

    if (n <= 5) {
        const double mn = m[static_cast<size_t>(n - 1)];
        const double phi = (sumM2 - 2.0 * mn * mn) / std::max(1e-12, 1.0 - 2.0 * an * an);
        const double invSqrtPhi = 1.0 / std::sqrt(std::max(1e-30, phi));
        for (int i = 1; i <= n - 2; ++i)
            a[static_cast<size_t>(i)] = m[static_cast<size_t>(i)] * invSqrtPhi;
    } else {
        const double an1 = -3.582633 * u5 + 5.682633 * u4 - 1.752461 * u3 - 0.293762 * u2
                + 0.042981 * u + cn1;
        a[static_cast<size_t>(n - 2)] = an1;
        a[1] = -an1;
        const double mn = m[static_cast<size_t>(n - 1)];
        const double mn1 = m[static_cast<size_t>(n - 2)];
        const double phi = (sumM2 - 2.0 * mn * mn - 2.0 * mn1 * mn1)
                / std::max(1e-12, 1.0 - 2.0 * an * an - 2.0 * an1 * an1);
        const double invSqrtPhi = 1.0 / std::sqrt(std::max(1e-30, phi));
        for (int i = 2; i <= n - 3; ++i)
            a[static_cast<size_t>(i)] = m[static_cast<size_t>(i)] * invSqrtPhi;
    }

    double sumA2 = 0.0;
    for (double ai : a)
        sumA2 += ai * ai;
    if (sumA2 > 1e-30) {
        const double invNormA = 1.0 / std::sqrt(sumA2);
        for (double &ai : a)
            ai *= invNormA;
    }

    double linComb = 0.0;
    for (int i = 0; i < n; ++i)
        linComb += a[static_cast<size_t>(i)] * x[static_cast<size_t>(i)];

    double w = (linComb * linComb) / ssUse;
    w = std::clamp(w, 1e-15, 1.0);
    wOut = w;

    if (1.0 - w <= 1e-15) {
        pOut = 1.0;
        return;
    }

    const double dn = static_cast<double>(n);
    if (n <= 11) {
        const double gamma = 0.459 * dn - 2.273;
        const double arg = gamma - std::log(1.0 - w);
        if (arg <= 1e-12) {
            pOut = 0.0;
            return;
        }
        const double y = -std::log(arg);
        const double mu = -0.0006714 * dn * dn * dn + 0.025054 * dn * dn - 0.39978 * dn + 0.5440;
        const double sigma = std::exp(-0.0020322 * dn * dn * dn + 0.062767 * dn * dn
                                      - 0.77857 * dn + 1.3822);
        const double z = (y - mu) / std::max(1e-12, sigma);
        pOut = std::clamp(gsl_cdf_ugaussian_Q(z), 0.0, 1.0);
    } else {
        const double lnN = std::log(dn);
        const double y = std::log(1.0 - w);
        const double mu = 0.0038915 * lnN * lnN * lnN - 0.083751 * lnN * lnN - 0.31082 * lnN
                - 1.5861;
        const double sigma = std::exp(0.0030302 * lnN * lnN - 0.082676 * lnN - 0.4803);
        const double z = (y - mu) / std::max(1e-12, sigma);
        pOut = std::clamp(gsl_cdf_ugaussian_Q(z), 0.0, 1.0);
    }
}

void computeLillieforsKS(const std::vector<double> &sortedX, double mean, double sd,
                         double &dOut, double &pOut)
{
    const int n = static_cast<int>(sortedX.size());
    if (n < 3 || sd <= 1e-30) {
        dOut = 1.0;
        pOut = 0.0;
        return;
    }

    const double dn = static_cast<double>(n);
    double maxD = 0.0;
    for (int i = 0; i < n; ++i) {
        const double z = (sortedX[static_cast<size_t>(i)] - mean) / sd;
        const double cdf = gsl_cdf_ugaussian_P(z);
        const double dPlus = (static_cast<double>(i + 1) / dn) - cdf;
        const double dMinus = cdf - (static_cast<double>(i) / dn);
        maxD = std::max({ maxD, dPlus, dMinus });
    }
    dOut = std::clamp(maxD, 0.0, 1.0);

    // Dallal & Wilkinson (1986) analytic Lilliefors p-value approximation
    double dAdj = dOut;
    double nEff = std::max(5.0, dn);
    if (dn > 100.0) {
        dAdj = dOut * std::pow(dn / 100.0, 0.49);
        nEff = 100.0;
    }
    const double sTerm = std::sqrt(nEff + 2.78019);
    const double peakD = 2.99587 / (2.0 * 7.01256 * sTerm);
    if (dAdj <= peakD) {
        pOut = 1.0;
        return;
    }
    const double expo = -7.01256 * dAdj * dAdj * (nEff + 2.78019) + 2.99587 * dAdj * sTerm
            - 0.122119 + 0.974598 / std::sqrt(nEff) + 1.67997 / nEff;
    pOut = std::clamp(std::exp(std::clamp(expo, -700.0, 50.0)), 0.0, 1.0);
}

void computeAndersonDarlingStephens(const std::vector<double> &sortedX, double mean, double sd,
                                    double &a2StarOut, double &pOut)
{
    const int n = static_cast<int>(sortedX.size());
    if (n < 3 || sd <= 1e-30) {
        a2StarOut = 1e6;
        pOut = 0.0;
        return;
    }

    std::vector<double> F(static_cast<size_t>(n), 0.5);
    for (int i = 0; i < n; ++i) {
        const double z = (sortedX[static_cast<size_t>(i)] - mean) / sd;
        F[static_cast<size_t>(i)] = std::clamp(gsl_cdf_ugaussian_P(z), 1e-15, 1.0 - 1e-15);
    }

    const double dn = static_cast<double>(n);
    double sumS = 0.0;
    for (int i = 0; i < n; ++i) {
        const double coef = static_cast<double>(2 * i + 1) / dn;
        sumS += coef * (std::log(F[static_cast<size_t>(i)])
                        + std::log(1.0 - F[static_cast<size_t>(n - 1 - i)]));
    }
    const double a2 = std::max(0.0, -dn - sumS);
    const double a2Star = a2 * (1.0 + 0.75 / dn + 2.25 / (dn * dn));
    a2StarOut = a2Star;

    double p = 1.0;
    if (a2Star >= 0.6) {
        p = std::exp(std::clamp(1.2937 - 5.709 * a2Star + 0.0186 * a2Star * a2Star, -700.0, 0.0));
    } else if (a2Star > 0.34) {
        p = std::exp(std::clamp(0.9177 - 4.279 * a2Star - 1.38 * a2Star * a2Star, -700.0, 0.0));
    } else if (a2Star > 0.2) {
        p = 1.0 - std::exp(std::clamp(-8.318 + 42.796 * a2Star - 59.938 * a2Star * a2Star,
                                      -700.0, 0.0));
    } else {
        p = 1.0 - std::exp(std::clamp(-13.436 + 101.14 * a2Star - 223.73 * a2Star * a2Star,
                                      -700.0, 0.0));
    }
    pOut = std::clamp(p, 0.0, 1.0);
}

bool solveOlsRssAndRank(const std::vector<std::vector<double>> &designRows,
                        const std::vector<double> &y, double &rssOut, int &rankOut)
{
    rssOut = 0.0;
    rankOut = 0;
    const size_t n = y.size();
    if (n == 0 || designRows.size() != n || designRows[0].empty())
        return false;
    const size_t p = designRows[0].size();
    if (n < p)
        return false;

    gsl_matrix *X = gsl_matrix_alloc(n, p);
    gsl_vector *yv = gsl_vector_alloc(n);
    gsl_vector *c = gsl_vector_alloc(p);
    gsl_matrix *cov = gsl_matrix_alloc(p, p);
    gsl_multifit_linear_workspace *ws = gsl_multifit_linear_alloc(n, p);
    if (!X || !yv || !c || !cov || !ws) {
        if (ws)
            gsl_multifit_linear_free(ws);
        if (cov)
            gsl_matrix_free(cov);
        if (c)
            gsl_vector_free(c);
        if (yv)
            gsl_vector_free(yv);
        if (X)
            gsl_matrix_free(X);
        return false;
    }

    for (size_t i = 0; i < n; ++i) {
        gsl_vector_set(yv, i, y[i]);
        for (size_t j = 0; j < p; ++j)
            gsl_matrix_set(X, i, j, designRows[i][j]);
    }

    double chisq = 0.0;
    size_t rk = 0;
    const int st = gsl_multifit_linear_tsvd(X, yv, 1e-10, c, cov, &chisq, &rk, ws);
    rssOut = std::max(0.0, chisq);
    rankOut = static_cast<int>(rk);

    gsl_multifit_linear_free(ws);
    gsl_matrix_free(cov);
    gsl_vector_free(c);
    gsl_vector_free(yv);
    gsl_matrix_free(X);
    return st == GSL_SUCCESS;
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

    const double nanVal = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> rawZ(static_cast<size_t>(R * C), nanVal);
    std::vector<uint8_t> validMask(static_cast<size_t>(R * C), 0);
    std::vector<double> allValid;
    allValid.reserve(static_cast<size_t>(R * C));

    for (int r = 0; r < R; ++r) {
        for (int c = 0; c < C; ++c) {
            const size_t idx = static_cast<size_t>(r * C + c);
            const double v = srcMatrix->cell(r, c);
            if (!std::isfinite(v))
                continue;
            rawZ[idx] = v;
            validMask[idx] = 1;
            allValid.push_back(v);
        }
    }
    if (allValid.size() < 6)
        return nullptr;

    // Fit planar background on the lowest 30% of valid pixels so bright peaks don't bias the plane
    std::vector<double> sortedValid = allValid;
    std::sort(sortedValid.begin(), sortedValid.end());
    const double bgCutoff =
            sortedValid[std::min(sortedValid.size() - 1, (sortedValid.size() * 3) / 10)];

    std::vector<std::vector<double>> bgBasis;
    std::vector<double> bgZ;
    for (int r = 0; r < R; ++r) {
        const double py = y0 + r * dyStep;
        for (int c = 0; c < C; ++c) {
            const size_t idx = static_cast<size_t>(r * C + c);
            if (!validMask[idx])
                continue;
            const double v = rawZ[idx];
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

    std::vector<double> resid(static_cast<size_t>(R * C), nanVal);
    double rMin = std::numeric_limits<double>::max();
    double rMax = -std::numeric_limits<double>::max();
    for (int r = 0; r < R; ++r) {
        const double py = y0 + r * dyStep;
        for (int c = 0; c < C; ++c) {
            const size_t idx = static_cast<size_t>(r * C + c);
            if (!validMask[idx])
                continue;
            const double px = x0 + c * dxStep;
            const double bg = planeC[0] + planeC[1] * px + planeC[2] * py;
            const double rv = rawZ[idx] - bg;
            resid[idx] = rv;
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
            const size_t cIdx = static_cast<size_t>(r * C + c);
            if (!validMask[cIdx])
                continue;
            const double center = resid[cIdx];
            if (center <= thresh)
                continue;
            bool isMax = true;
            bool strictlyGreater = false;
            for (int dr = -1; dr <= 1 && isMax; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (dr == 0 && dc == 0)
                        continue;
                    const size_t nIdx = static_cast<size_t>((r + dr) * C + (c + dc));
                    if (!validMask[nIdx])
                        continue;
                    double nb = resid[nIdx];
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
                const size_t pIdx = static_cast<size_t>(rr * C + cc);
                if (!validMask[pIdx])
                    continue;
                double px = x0 + cc * dxStep;
                patch.x.push_back(px);
                patch.y.push_back(py);
                patch.z.push_back(std::max(0.0, resid[pIdx]));
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
                    if ((st != GSL_SUCCESS && st != GSL_CONTINUE)
                        || Fit::lmStepConverged(s, s->dx, s->x, 1e-6))
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

    pt->setHeader(QStringList() << QStringLiteral("Peak") << QStringLiteral("X_Center")
                                << QStringLiteral("Y_Center") << QStringLiteral("Amplitude")
                                << QStringLiteral("Sigma_X") << QStringLiteral("Sigma_Y")
                                << QStringLiteral("FWHM_X") << QStringLiteral("FWHM_Y")
                                << QStringLiteral("Volume"));
    const QStringList peakLongNames = {
        QObject::tr("Peak"),      QObject::tr("X_Center"), QObject::tr("Y_Center"),
        QObject::tr("Amplitude"), QObject::tr("Sigma_X"),  QObject::tr("Sigma_Y"),
        QObject::tr("FWHM_X"),    QObject::tr("FWHM_Y"),   QObject::tr("Volume")
    };
    for (int col = 0; col < 9; ++col) {
        pt->column(col)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        pt->column(col)->setPlotDesignation(col == 1 ? SciDAVis::X : SciDAVis::Y);
        pt->column(col)->setLongName(peakLongNames[col]);
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
    backing->setProperty("_tg_internal_helper", true);

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

namespace {

double stdNormalIntervalProb(double a, double b)
{
    if (b <= a)
        return 0.0;
    constexpr double kInvSqrt2 = 0.70710678118654752440084436210485;
    if (a >= 0.0)
        return std::max(0.0, 0.5 * (std::erfc(a * kInvSqrt2) - std::erfc(b * kInvSqrt2)));
    if (b <= 0.0)
        return std::max(0.0, 0.5 * (std::erfc(-b * kInvSqrt2) - std::erfc(-a * kInvSqrt2)));
    if (std::abs(a) < 1.0 && std::abs(b) < 1.0)
        return std::max(0.0, 0.5 * (std::erf(b * kInvSqrt2) - std::erf(a * kInvSqrt2)));
    return std::clamp(1.0 - 0.5 * (std::erfc(b * kInvSqrt2) + std::erfc(-a * kInvSqrt2)), 0.0, 1.0);
}

double normalRangeSurvival(double w, int kGroups)
{
    if (!std::isfinite(w) || w <= 0.0)
        return 1.0;
    if (kGroups <= 1)
        return 0.0;
    if (kGroups == 2)
        return std::clamp(std::erfc(0.5 * w), 0.0, 1.0);
    if (w >= 20.0)
        return 0.0;

    constexpr double kInvSqrt2 = 0.70710678118654752440084436210485;
    constexpr double kInvSqrt2Pi = 0.39894228040143267793994605993438;
    constexpr int nSteps = 96;
    const double zLo = -9.0;
    const double zHi = 9.0;
    const double h = (zHi - zLo) / static_cast<double>(nSteps);

    auto integrand = [&](double z) -> double {
        const double phi = kInvSqrt2Pi * std::exp(-0.5 * z * z);
        const double A = 0.5 * std::erfc(z * kInvSqrt2);
        const double B = 0.5 * std::erfc((z + w) * kInvSqrt2);
        const double D = stdNormalIntervalProb(z, z + w);
        double polySum = 0.0;
        double powA = 1.0;
        for (int r = 0; r <= kGroups - 2; ++r) {
            polySum += powA * std::pow(D, kGroups - 2 - r);
            powA *= A;
        }
        return phi * B * polySum;
    };

    double acc = integrand(zLo) + integrand(zHi);
    for (int i = 1; i < nSteps; ++i) {
        const double z = zLo + static_cast<double>(i) * h;
        acc += ((i & 1) ? 4.0 : 2.0) * integrand(z);
    }
    return std::clamp(static_cast<double>(kGroups) * (h / 3.0) * acc, 0.0, 1.0);
}

double studentizedRangeUpperP(double q, int kGroups, double df)
{
    if (!std::isfinite(q) || q <= 0.0 || kGroups < 2 || !std::isfinite(df) || df <= 0.0)
        return 1.0;
    if (kGroups == 2)
        return std::clamp(2.0 * gsl_cdf_tdist_Q(q / std::sqrt(2.0), df), 0.0, 1.0);
    if (df > 10000.0)
        return normalRangeSurvival(q, kGroups);

    const double halfNu = 0.5 * df;
    const double logNormConst =
            std::log(2.0) + halfNu * std::log(halfNu) - gsl_sf_lngamma(halfNu);
    const double sMode = (df > 1.0) ? std::sqrt((df - 1.0) / df) : 0.0;
    const double sSpread = 12.0 / std::sqrt(std::max(0.5, 2.0 * df));
    const double sLo = std::max(1e-6, sMode - sSpread);
    const double sHi = std::max(sLo + 1.0, sMode + sSpread);

    constexpr int nSteps = 96;
    const double h = (sHi - sLo) / static_cast<double>(nSteps);

    auto densityS = [&](double s) -> double {
        const double lp = logNormConst + (df - 1.0) * std::log(s) - halfNu * s * s;
        return (lp > -740.0) ? std::exp(lp) : 0.0;
    };

    double numAcc = 0.0;
    double denAcc = 0.0;
    for (int i = 0; i <= nSteps; ++i) {
        const double wSimp = (i == 0 || i == nSteps) ? 1.0 : ((i & 1) ? 4.0 : 2.0);
        const double s = sLo + static_cast<double>(i) * h;
        const double gs = densityS(s);
        if (gs > 0.0) {
            numAcc += wSimp * gs * normalRangeSurvival(q * s, kGroups);
            denAcc += wSimp * gs;
        }
    }
    if (denAcc <= 0.0)
        return normalRangeSurvival(q, kGroups);
    return std::clamp(numAcc / denAcc, 0.0, 1.0);
}

double dunnettConditionalSurvival(double cs, const std::vector<double> &aVec,
                                  const std::vector<double> &bVec)
{
    if (!std::isfinite(cs) || cs <= 0.0)
        return 1.0;
    if (aVec.empty())
        return 0.0;
    if (cs >= 20.0)
        return 0.0;

    constexpr double kInvSqrt2 = 0.70710678118654752440084436210485;
    constexpr double kInvSqrt2Pi = 0.39894228040143267793994605993438;
    constexpr int nSteps = 96;
    const double zLo = -9.0;
    const double zHi = 9.0;
    const double h = (zHi - zLo) / static_cast<double>(nSteps);

    auto integrand = [&](double z) -> double {
        const double phi = kInvSqrt2Pi * std::exp(-0.5 * z * z);
        double surv = 0.0;
        for (size_t l = 0; l < aVec.size(); ++l) {
            const double invA = 1.0 / aVec[l];
            const double u1 = (cs + bVec[l] * z) * invA * kInvSqrt2;
            const double u2 = (cs - bVec[l] * z) * invA * kInvSqrt2;
            const double ql = std::clamp(0.5 * (std::erfc(u1) + std::erfc(u2)), 0.0, 1.0);
            surv += (1.0 - surv) * ql;
        }
        return phi * surv;
    };

    double acc = integrand(zLo) + integrand(zHi);
    for (int i = 1; i < nSteps; ++i) {
        const double z = zLo + static_cast<double>(i) * h;
        acc += ((i & 1) ? 4.0 : 2.0) * integrand(z);
    }
    return std::clamp((h / 3.0) * acc, 0.0, 1.0);
}

double dunnettTwoSidedUpperP(double absT, double df, const std::vector<double> &treatSizes,
                             double nCtrl)
{
    if (!std::isfinite(absT) || absT <= 0.0 || !std::isfinite(df) || df <= 0.0 || nCtrl <= 0.0)
        return 1.0;
    if (treatSizes.size() <= 1)
        return std::clamp(2.0 * gsl_cdf_tdist_Q(absT, df), 0.0, 1.0);

    std::vector<double> aVec(treatSizes.size());
    std::vector<double> bVec(treatSizes.size());
    for (size_t l = 0; l < treatSizes.size(); ++l) {
        const double nl = std::max(1.0, treatSizes[l]);
        const double denom = nl + nCtrl;
        bVec[l] = std::sqrt(nl / denom);
        aVec[l] = std::sqrt(std::max(1e-12, nCtrl / denom));
    }

    if (df > 10000.0)
        return dunnettConditionalSurvival(absT, aVec, bVec);

    const double halfNu = 0.5 * df;
    const double logNormConst =
            std::log(2.0) + halfNu * std::log(halfNu) - gsl_sf_lngamma(halfNu);
    const double sMode = (df > 1.0) ? std::sqrt((df - 1.0) / df) : 0.0;
    const double sSpread = 12.0 / std::sqrt(std::max(0.5, 2.0 * df));
    const double sLo = std::max(1e-6, sMode - sSpread);
    const double sHi = std::max(sLo + 1.0, sMode + sSpread);

    constexpr int nSteps = 96;
    const double h = (sHi - sLo) / static_cast<double>(nSteps);

    auto densityS = [&](double s) -> double {
        const double lp = logNormConst + (df - 1.0) * std::log(s) - halfNu * s * s;
        return (lp > -740.0) ? std::exp(lp) : 0.0;
    };

    double numAcc = 0.0;
    double denAcc = 0.0;
    for (int i = 0; i <= nSteps; ++i) {
        const double wSimp = (i == 0 || i == nSteps) ? 1.0 : ((i & 1) ? 4.0 : 2.0);
        const double s = sLo + static_cast<double>(i) * h;
        const double gs = densityS(s);
        if (gs > 0.0) {
            numAcc += wSimp * gs * dunnettConditionalSurvival(absT * s, aVec, bVec);
            denAcc += wSimp * gs;
        }
    }
    if (denAcc <= 0.0)
        return dunnettConditionalSurvival(absT, aVec, bVec);
    return std::clamp(numAcc / denAcc, 0.0, 1.0);
}

} // namespace

Table *GraphStatsAndSurfaceFit::runGroupComparisonAndAnnotate(ApplicationWindow *app,
                                                              Table *srcTable,
                                                              const QStringList &colNames,
                                                              Graph *annotateGraph)
{
    if (!app || !srcTable || colNames.size() < 2)
        return nullptr;

    const QString posthocRaw =
            srcTable->property("torigraph_anova_posthoc").toString().trimmed().toLower();
    const QString controlRaw =
            srcTable->property("torigraph_anova_control").toString().trimmed();

    enum class PostHocMode { HolmWelch, TukeyHSD, GamesHowell, Dunnett };
    PostHocMode posthocMode = PostHocMode::HolmWelch;
    if (posthocRaw == QStringLiteral("tukey") || posthocRaw == QStringLiteral("tukey_hsd")
        || posthocRaw == QStringLiteral("tukey_kramer")
        || posthocRaw == QStringLiteral("tukey-kramer") || posthocRaw == QStringLiteral("hsd")) {
        posthocMode = PostHocMode::TukeyHSD;
    } else if (posthocRaw == QStringLiteral("games_howell")
               || posthocRaw == QStringLiteral("games-howell")
               || posthocRaw == QStringLiteral("gameshowell")
               || posthocRaw == QStringLiteral("gh")) {
        posthocMode = PostHocMode::GamesHowell;
    } else if (posthocRaw == QStringLiteral("dunnett")
               || posthocRaw == QStringLiteral("dunnetts")) {
        posthocMode = PostHocMode::Dunnett;
    }

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
            double v = 0.0;
            if (readNumericColumnCell(col, r, v))
                gd.vals.push_back(v);
        }
        // A group of one value enters the ANOVA (it adds to the between-group sum of squares
        // and to N - k, not to the within-group one), as in the textbook test; it used to be
        // dropped without a word.
        if (gd.vals.empty())
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
        gd.var = (nk > 1) ? ss / static_cast<double>(nk - 1) : 0.0;
        groups.push_back(std::move(gd));
    }

    const int K = static_cast<int>(groups.size());
    if (K < 2)
        return nullptr;
    {
        size_t nAll = 0;
        for (const GroupData &g : groups)
            nAll += g.vals.size();
        if (nAll <= static_cast<size_t>(K))
            return nullptr; // every group has one value: no within-group degrees of freedom
    }

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

    // Post-hoc pairwise / vs-control comparisons
    struct PairResult
    {
        int i = 0;
        int j = 1;
        QString label;
        double statVal = 0.0;
        double dfNum = 1.0;
        double dfDen = 1.0;
        double pRaw = 1.0;
        double pAdj = 1.0;
        double meanDiff = 0.0;
    };
    std::vector<PairResult> pairs;

    if (posthocMode == PostHocMode::Dunnett) {
        int ctrlIdx = 0;
        if (!controlRaw.isEmpty()) {
            bool matched = false;
            for (int k = 0; k < K; ++k) {
                if (groups[static_cast<size_t>(k)].label == controlRaw) {
                    ctrlIdx = k;
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                for (int k = 0; k < K; ++k) {
                    if (groups[static_cast<size_t>(k)].label.compare(controlRaw,
                                                                     Qt::CaseInsensitive)
                        == 0) {
                        ctrlIdx = k;
                        matched = true;
                        break;
                    }
                }
            }
            if (!matched) {
                if (Column *cCol = resolveTableColumn(srcTable, controlRaw)) {
                    for (int k = 0; k < K; ++k) {
                        if (groups[static_cast<size_t>(k)].label == cCol->name()) {
                            ctrlIdx = k;
                            matched = true;
                            break;
                        }
                    }
                }
            }
            if (!matched) {
                bool okIdx = false;
                int parsedIdx = controlRaw.toInt(&okIdx);
                if (okIdx) {
                    if (parsedIdx >= 0 && parsedIdx < K)
                        ctrlIdx = parsedIdx;
                    else if (parsedIdx >= 1 && parsedIdx <= K)
                        ctrlIdx = parsedIdx - 1;
                }
            }
        }

        const GroupData &gCtrl = groups[static_cast<size_t>(ctrlIdx)];
        const double nCtrl = static_cast<double>(gCtrl.vals.size());
        std::vector<double> treatSizes;
        treatSizes.reserve(static_cast<size_t>(K - 1));
        for (int k = 0; k < K; ++k) {
            if (k != ctrlIdx)
                treatSizes.push_back(static_cast<double>(groups[static_cast<size_t>(k)].vals.size()));
        }

        for (int i = 0; i < K; ++i) {
            if (i == ctrlIdx)
                continue;
            const GroupData &gi = groups[static_cast<size_t>(i)];
            const double ni = static_cast<double>(gi.vals.size());
            const double sePooled = std::sqrt(msWithin * (1.0 / ni + 1.0 / nCtrl));

            PairResult pr;
            pr.i = i;
            pr.j = ctrlIdx;
            pr.label = QStringLiteral("%1 vs %2").arg(gi.label, gCtrl.label);
            pr.meanDiff = gi.mean - gCtrl.mean;
            pr.dfNum = static_cast<double>(K - 1);
            pr.dfDen = df2;
            if (sePooled > 1e-30 && df2 > 0.0) {
                pr.statVal = pr.meanDiff / sePooled;
                pr.pRaw = std::clamp(2.0 * gsl_cdf_tdist_Q(std::abs(pr.statVal), df2), 0.0, 1.0);
                pr.pAdj = dunnettTwoSidedUpperP(std::abs(pr.statVal), df2, treatSizes, nCtrl);
            } else if (std::abs(pr.meanDiff) > 1e-30) {
                pr.statVal = (pr.meanDiff > 0.0) ? 1e6 : -1e6;
                pr.pRaw = 0.0;
                pr.pAdj = 0.0;
            }
            pairs.push_back(pr);
        }
    } else {
        for (int i = 0; i < K; ++i) {
            for (int j = i + 1; j < K; ++j) {
                const GroupData &gi = groups[static_cast<size_t>(i)];
                const GroupData &gj = groups[static_cast<size_t>(j)];
                const double ni = static_cast<double>(gi.vals.size());
                const double nj = static_cast<double>(gj.vals.size());
                // A group of one value has no variance of its own: Welch / Games-Howell take the
                // pooled within-group variance for it, with its N - k degrees of freedom. When
                // both groups have one value they share that one estimate, so its two parts form
                // a single Satterthwaite term with N - k degrees of freedom (as two independent
                // terms they gave 2 (N - k), and p came out too small: A = [10], B = [13],
                // C = [1..5] gave df 8, p 0.217 instead of the pooled t with df 4, p 0.251; at
                // t = 4 that is p 0.0039 instead of 0.016).
                const double vi = ((ni > 1.0) ? gi.var : msWithin) / ni;
                const double vj = ((nj > 1.0) ? gj.var : msWithin) / nj;
                const double vOwn_i = (ni > 1.0) ? vi : 0.0;
                const double vOwn_j = (nj > 1.0) ? vj : 0.0;
                const double vPool = vi + vj - vOwn_i - vOwn_j;
                const double vSum = vi + vj;
                const double welchDenom = ((ni > 1.0) ? vOwn_i * vOwn_i / (ni - 1.0) : 0.0)
                        + ((nj > 1.0) ? vOwn_j * vOwn_j / (nj - 1.0) : 0.0)
                        + ((vPool > 0.0 && df2 > 0.0) ? vPool * vPool / df2 : 0.0);

                PairResult pr;
                pr.i = i;
                pr.j = j;
                pr.label = QStringLiteral("%1 vs %2").arg(gi.label, gj.label);
                pr.meanDiff = gi.mean - gj.mean;

                if (posthocMode == PostHocMode::TukeyHSD) {
                    const double seQ = std::sqrt(0.5 * msWithin * (1.0 / ni + 1.0 / nj));
                    pr.dfNum = static_cast<double>(K);
                    pr.dfDen = df2;
                    if (seQ > 1e-30 && df2 > 0.0) {
                        pr.statVal = std::abs(pr.meanDiff) / seQ;
                        const double tPooled = pr.statVal / std::sqrt(2.0);
                        pr.pRaw = std::clamp(2.0 * gsl_cdf_tdist_Q(tPooled, df2), 0.0, 1.0);
                        pr.pAdj = studentizedRangeUpperP(pr.statVal, K, df2);
                    } else if (std::abs(pr.meanDiff) > 1e-30) {
                        pr.statVal = 1e6;
                        pr.pRaw = 0.0;
                        pr.pAdj = 0.0;
                    }
                } else if (posthocMode == PostHocMode::GamesHowell) {
                    const double denom = welchDenom;
                    const double dfWelch =
                            (denom > 1e-30) ? ((vSum * vSum) / denom) : (ni + nj - 2.0);
                    pr.dfNum = static_cast<double>(K);
                    pr.dfDen = dfWelch;
                    if (vSum > 1e-30 && dfWelch > 0.0) {
                        const double seQ = std::sqrt(0.5 * vSum);
                        pr.statVal = std::abs(pr.meanDiff) / seQ;
                        const double tWelch = std::abs(pr.meanDiff) / std::sqrt(vSum);
                        pr.pRaw = std::clamp(2.0 * gsl_cdf_tdist_Q(tWelch, dfWelch), 0.0, 1.0);
                        pr.pAdj = studentizedRangeUpperP(pr.statVal, K, dfWelch);
                    } else if (std::abs(pr.meanDiff) > 1e-30) {
                        pr.statVal = 1e6;
                        pr.pRaw = 0.0;
                        pr.pAdj = 0.0;
                    }
                } else {
                    // Default: Welch t-test + Holm-Bonferroni step-down adjustment
                    pr.dfDen = df2;
                    if (vSum > 1e-30) {
                        pr.statVal = pr.meanDiff / std::sqrt(vSum);
                        const double denom = welchDenom;
                        pr.dfNum = (denom > 1e-30) ? ((vSum * vSum) / denom) : (ni + nj - 2.0);
                        pr.pRaw = std::clamp(2.0 * gsl_cdf_tdist_Q(std::abs(pr.statVal), pr.dfNum),
                                             0.0, 1.0);
                    } else if (std::abs(pr.meanDiff) > 1e-30) {
                        pr.statVal = (pr.meanDiff > 0.0) ? 1e6 : -1e6;
                        pr.dfNum = ni + nj - 2.0;
                        pr.pRaw = 0.0;
                    } else {
                        pr.dfNum = ni + nj - 2.0;
                    }
                    pr.pAdj = pr.pRaw;
                }
                pairs.push_back(pr);
            }
        }

        if (posthocMode == PostHocMode::HolmWelch && pairs.size() > 1) {
            const size_t mCount = pairs.size();
            std::vector<size_t> order(mCount);
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(),
                      [&](size_t a, size_t b) { return pairs[a].pRaw < pairs[b].pRaw; });
            double runningMax = 0.0;
            for (size_t rank = 0; rank < mCount; ++rank) {
                size_t idx = order[rank];
                double mult = static_cast<double>(mCount - rank);
                double adj = std::min(1.0, pairs[idx].pRaw * mult);
                runningMax = std::max(runningMax, adj);
                pairs[idx].pAdj = runningMax;
            }
        }
    }

    const size_t M = pairs.size();
    const int nRows = 1 + static_cast<int>(M);
    QString tblName =
            app->generateUniqueName(srcTable->name() + QStringLiteral("_GroupStats"), false);
    QString posthocTitle;
    switch (posthocMode) {
    case PostHocMode::TukeyHSD:
        posthocTitle = QObject::tr("One-Way ANOVA & Tukey HSD Post-Hoc Summary");
        break;
    case PostHocMode::GamesHowell:
        posthocTitle = QObject::tr("One-Way ANOVA & Games-Howell Post-Hoc Summary");
        break;
    case PostHocMode::Dunnett:
        posthocTitle = QObject::tr("One-Way ANOVA & Dunnett Post-Hoc Summary");
        break;
    case PostHocMode::HolmWelch:
    default:
        posthocTitle = QObject::tr("One-Way ANOVA & Holm-Welch Post-Hoc Summary");
        break;
    }
    Table *resTbl = app->newTable(nRows, 8, tblName, posthocTitle);
    if (!resTbl)
        return nullptr;

    resTbl->setHeader(QStringList() << QStringLiteral("Comparison") << QStringLiteral("Statistic")
                                    << QStringLiteral("df1") << QStringLiteral("df2")
                                    << QStringLiteral("p_value") << QStringLiteral("p_adj")
                                    << QStringLiteral("MeanDiff_Eta2")
                                    << QStringLiteral("Significance"));
    const QStringList anovaLongNames = {
        QObject::tr("Comparison"), QObject::tr("Statistic"),     QObject::tr("df1"),
        QObject::tr("df2"),        QObject::tr("p_value"),       QObject::tr("p_adj"),
        QObject::tr("MeanDiff_Eta2"), QObject::tr("Significance")
    };
    resTbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
    for (int c = 1; c <= 6; ++c)
        resTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    resTbl->column(7)->setColumnMode(SciDAVis::ColumnMode::Text);
    for (int c = 0; c < 8; ++c)
        resTbl->column(c)->setLongName(anovaLongNames[c]);

    // Row 0: Omnibus One-Way ANOVA
    resTbl->column(0)->setTextAt(0, QStringLiteral("One-Way ANOVA"));
    resTbl->column(1)->setValueAt(0, fStat);
    resTbl->column(2)->setValueAt(0, df1);
    resTbl->column(3)->setValueAt(0, df2);
    resTbl->column(4)->setValueAt(0, pAnova);
    resTbl->column(5)->setValueAt(0, pAnova);
    resTbl->column(6)->setValueAt(0, etaSq);
    resTbl->column(7)->setTextAt(0, starRatingForP(pAnova));

    // Rows 1..M: Post-hoc comparisons
    for (size_t m = 0; m < M; ++m) {
        const int r = static_cast<int>(m) + 1;
        const PairResult &pr = pairs[m];
        resTbl->column(0)->setTextAt(r, pr.label);
        resTbl->column(1)->setValueAt(r, pr.statVal);
        resTbl->column(2)->setValueAt(r, pr.dfNum);
        resTbl->column(3)->setValueAt(r, pr.dfDen);
        resTbl->column(4)->setValueAt(r, pr.pRaw);
        resTbl->column(5)->setValueAt(r, pr.pAdj);
        resTbl->column(6)->setValueAt(r, pr.meanDiff);
        resTbl->column(7)->setTextAt(r, starRatingForP(pr.pAdj));
    }

    // Annotate significance brackets on annotateGraph if provided
    if (annotateGraph && !pairs.empty()) {
        double yMinAll = groups.front().minV;
        double yMaxAll = groups.front().maxV;
        QStringList groupLabels;
        groupLabels.reserve(K);
        for (const GroupData &gd : groups) {
            yMinAll = std::min(yMinAll, gd.minV);
            yMaxAll = std::max(yMaxAll, gd.maxV);
            groupLabels << gd.label;
        }

        const QVariant showNsProp = srcTable->property("torigraph_anova_show_ns");
        const bool showNsExplicit = showNsProp.isValid() && !showNsProp.isNull();
        const bool showNs = showNsExplicit ? showNsProp.toBool() : false;

        std::vector<std::tuple<int, int, double>> sigToDraw;
        if (showNsExplicit) {
            for (const PairResult &pr : pairs) {
                if (showNs || pr.pAdj < 0.05)
                    sigToDraw.emplace_back(pr.i, pr.j, pr.pAdj);
            }
        } else if (pairs.size() == 1) {
            sigToDraw.emplace_back(pairs.front().i, pairs.front().j, pairs.front().pAdj);
        } else {
            for (const PairResult &pr : pairs) {
                if (pr.pAdj < 0.05)
                    sigToDraw.emplace_back(pr.i, pr.j, pr.pAdj);
            }
            if (sigToDraw.empty())
                sigToDraw.emplace_back(pairs.front().i, pairs.front().j, pairs.front().pAdj);
        }
        if (!sigToDraw.empty()) {
            annotateBracketsOnGraph(annotateGraph, K, yMinAll, yMaxAll, sigToDraw, groupLabels,
                                    srcTable, parseBracketLabelFormat(srcTable));
        }
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
        std::vector<double> distinctStates;
        for (int r = 0; r < nRows; ++r) {
            double st = 0.0, sc = 0.0;
            if (readNumericColumnCell(cState, r, st) && readNumericColumnCell(cScore, r, sc)) {
                rawStates.push_back(st);
                rawScores.push_back(sc);
                bool seen = false;
                for (double ds : distinctStates) {
                    if (std::fabs(st - ds) <= 1e-12 * std::max(1.0, std::fabs(ds))) {
                        seen = true;
                        break;
                    }
                }
                if (!seen)
                    distinctStates.push_back(st);
            }
        }
        if (rawStates.size() < 2)
            return nullptr;
        // Reject non-binary / continuous numeric state columns instead of silently thresholding (E4-10)
        if (distinctStates.size() != 2) {
            const QString errMsg = QObject::tr(
                    "ROC state column '%1' must contain exactly 2 distinct numeric classes "
                    "(e.g. 0 and 1); found %2 distinct value(s).")
                                           .arg(cState->name())
                                           .arg(static_cast<int>(distinctStates.size()));
            app->setProperty("torigraph_last_roc_error", errMsg);
            if (app->writeFitResultsToLog)
                app->updateLog(QStringLiteral("[ROC Error] ") + errMsg + QLatin1Char('\n'));
            if (!isNonInteractiveApp(app))
                QMessageBox::warning(app, QObject::tr("ROC Curve Analysis"), errMsg);
            return nullptr;
        }
        const double negVal = std::min(distinctStates[0], distinctStates[1]);
        const double posVal = std::max(distinctStates[0], distinctStates[1]);
        if (posVal <= negVal)
            return nullptr;
        for (size_t i = 0; i < rawStates.size(); ++i) {
            const int lbl =
                    (std::fabs(rawStates[i] - posVal) < std::fabs(rawStates[i] - negVal)) ? 1 : 0;
            obs.push_back({ lbl, rawScores[i] });
        }
    } else {
        struct RawTextObs
        {
            QString txt;
            double score = 0.0;
        };
        std::vector<RawTextObs> rawObs;
        QStringList levels;
        auto isPos = [](const QString &low) -> bool {
            return low == QStringLiteral("1") || low == QStringLiteral("true")
                    || low == QStringLiteral("yes") || low == QStringLiteral("pos")
                    || low == QStringLiteral("positive") || low == QStringLiteral("case")
                    || low == QStringLiteral("event") || low == QStringLiteral("disease")
                    || low == QStringLiteral("treated") || low == QStringLiteral("pass")
                    || low == QStringLiteral("\u967d\u6027");
        };
        auto isNeg = [](const QString &low) -> bool {
            return low == QStringLiteral("0") || low == QStringLiteral("false")
                    || low == QStringLiteral("no") || low == QStringLiteral("none")
                    || low == QStringLiteral("neg") || low == QStringLiteral("negative")
                    || low == QStringLiteral("control") || low == QStringLiteral("healthy")
                    || low == QStringLiteral("normal") || low == QStringLiteral("\u9670\u6027");
        };
        for (int r = 0; r < nRows; ++r) {
            if (cState->isInvalid(r) || cState->isMasked(r))
                continue;
            double sc = 0.0;
            if (!readNumericColumnCell(cScore, r, sc))
                continue;
            const QString txt = cState->textAt(r).trimmed();
            if (txt.isEmpty())
                continue;
            if (!levels.contains(txt))
                levels.append(txt);
            rawObs.push_back({ txt, sc });
        }
        if (levels.size() == 2) {
            const QString &L0 = levels[0];
            const QString &L1 = levels[1];
            const QString low0 = L0.toLower();
            const QString low1 = L1.toLower();
            const bool p0 = isPos(low0), p1 = isPos(low1);
            const bool n0 = isNeg(low0), n1 = isNeg(low1);
            const QString posLevel = ((p0 && !p1) || (n1 && !n0)) ? L0 : L1;
            obs.reserve(rawObs.size());
            for (const RawTextObs &ro : rawObs)
                obs.push_back({ (ro.txt == posLevel) ? 1 : 0, ro.score });
        } else if (levels.size() > 2) {
            for (const RawTextObs &ro : rawObs) {
                const QString low = ro.txt.toLower();
                if (isPos(low) == isNeg(low))
                    return nullptr;
            }
            obs.reserve(rawObs.size());
            for (const RawTextObs &ro : rawObs)
                obs.push_back({ isPos(ro.txt.toLower()) ? 1 : 0, ro.score });
        } else {
            return nullptr;
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

    rocTbl->setHeader(QStringList() << QStringLiteral("FPR_1MinusSpec") << QStringLiteral("TPR_Sens")
                                    << QStringLiteral("RefDiag") << QStringLiteral("Cutoff")
                                    << QStringLiteral("Specificity") << QStringLiteral("Youden_J"));
    const QStringList rocLongNames = {
        QObject::tr("FPR_1MinusSpec"), QObject::tr("TPR_Sens"),    QObject::tr("RefDiag"),
        QObject::tr("Cutoff"),         QObject::tr("Specificity"), QObject::tr("Youden_J")
    };
    for (int c = 0; c < 6; ++c) {
        rocTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        rocTbl->column(c)->setPlotDesignation(c == 0 ? SciDAVis::X : SciDAVis::Y);
        rocTbl->column(c)->setLongName(rocLongNames[c]);
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
        applyArialGraphFonts(g);
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
        if (cEvent->isInvalid(r) || cEvent->isMasked(r))
            continue;
        if (cGroup && (cGroup->isInvalid(r) || cGroup->isMasked(r)))
            continue;
        double tv = 0.0;
        if (!readNumericColumnCell(cTime, r, tv) || tv < 0.0)
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
        applyArialGraphFonts(g);
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
            double v = 0.0;
            if (!readNumericColumnCell(c, r, v)) {
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
        g->setXAxisTitle(QObject::tr("PC1 (%1%)").arg(explainedPct[0], 0, 'f', 1));
        g->setYAxisTitle(QObject::tr("PC2 (%1%)").arg(explainedPct[1], 0, 'f', 1));
        g->setTitle(QObject::tr("PCA Biplot (PC1 + PC2 = %1%)")
                            .arg(explainedPct[0] + explainedPct[1], 0, 'f', 1));
        if (g->legend())
            g->legend()->setText(g->legendText());
        applyArialGraphFonts(g);
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
    if (!app || isNonInteractiveApp(app))
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
    if (!app || isNonInteractiveApp(app))
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
    if (!app || isNonInteractiveApp(app))
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
    if (!app || isNonInteractiveApp(app))
        return;
    QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    Graph *targetG = resolveActiveGraph(app, g);

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Group Comparison (ANOVA / Post-Hoc / Nonparametric)"));
    dlg.setMinimumWidth(460);
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

    // Unified parametric / auto smart-switch / nonparametric selector (B2-10)
    QComboBox *modeCombo = new QComboBox(&dlg);
    modeCombo->addItem(QObject::tr("Parametric (One-Way ANOVA / t-Test)"), 0);
    modeCombo->addItem(QObject::tr("Auto Smart-Switch (Shapiro-Wilk -> Parametric / Nonparametric)"), 1);
    modeCombo->addItem(QObject::tr("Nonparametric (Kruskal-Wallis + Dunn / Mann-Whitney U)"), 2);
    modeCombo->addItem(QObject::tr("Nonparametric Paired (Wilcoxon Signed-Rank, 2 Groups)"), 3);
    form->addRow(QObject::tr("Comparison Mode:"), modeCombo);

    // Post-hoc method selector matching stats.anova_oneway defaults (Tukey HSD default, B2-3)
    QComboBox *posthocCombo = new QComboBox(&dlg);
    posthocCombo->addItem(QObject::tr("Tukey HSD (Tukey-Kramer)"), QStringLiteral("tukey"));
    posthocCombo->addItem(QObject::tr("Holm-Welch (Welch t + Holm)"), QStringLiteral("holm_welch"));
    posthocCombo->addItem(QObject::tr("Games-Howell (Unequal Variances)"),
                          QStringLiteral("games_howell"));
    posthocCombo->addItem(QObject::tr("Dunnett (Treatments vs Control)"),
                          QStringLiteral("dunnett"));
    form->addRow(QObject::tr("Post-Hoc Test:"), posthocCombo);

    QComboBox *controlCombo = new QComboBox(&dlg);
    form->addRow(QObject::tr("Control Group (Dunnett):"), controlCombo);

    QCheckBox *annotCheck =
            new QCheckBox(QObject::tr("Add Significance Brackets (*/**/***) to Graph"), &dlg);
    annotCheck->setChecked(targetG != nullptr);
    annotCheck->setEnabled(targetG != nullptr);
    form->addRow(QString(), annotCheck);

    QComboBox *bracketFmtCombo = new QComboBox(&dlg);
    bracketFmtCombo->addItem(QObject::tr("Asterisks (*, **, ***)"), QStringLiteral("stars"));
    bracketFmtCombo->addItem(QObject::tr("Asterisks + p-value (* (p = ...))"),
                             QStringLiteral("stars_p"));
    bracketFmtCombo->addItem(QObject::tr("p-value only (p = ...)"), QStringLiteral("p"));
    form->addRow(QObject::tr("Bracket Label:"), bracketFmtCombo);

    QCheckBox *showNsCheck =
            new QCheckBox(QObject::tr("Show 'ns' Brackets for Non-Significant Pairs"), &dlg);
    showNsCheck->setChecked(false);
    form->addRow(QString(), showNsCheck);
    lay->addLayout(form);

    QLabel *wilcoxonNoteLbl = new QLabel(
            QObject::tr("Note: Wilcoxon Signed-Rank compares only the first 2 selected columns; extra columns will be ignored."),
            &dlg);
    wilcoxonNoteLbl->setWordWrap(true);
    wilcoxonNoteLbl->setStyleSheet(QStringLiteral("color: #B45309;"));
    wilcoxonNoteLbl->setVisible(false);
    lay->addWidget(wilcoxonNoteLbl);

    auto refreshControlGroups = [&]() {
        const QString prevCtrl = controlCombo->currentText();
        controlCombo->clear();
        for (QListWidgetItem *it : colList->selectedItems())
            controlCombo->addItem(it->text());
        if (controlCombo->count() == 0) {
            for (int r = 0; r < colList->count(); ++r)
                controlCombo->addItem(colList->item(r)->text());
        }
        const int idx = controlCombo->findText(prevCtrl);
        if (idx >= 0)
            controlCombo->setCurrentIndex(idx);
    };

    auto syncControlStates = [&]() {
        const int mode = modeCombo->currentData().toInt();
        const bool isParametric = (mode == 0);
        const bool isDunnett =
                isParametric && (posthocCombo->currentData().toString() == QStringLiteral("dunnett"));
        posthocCombo->setEnabled(isParametric);
        controlCombo->setEnabled(isDunnett);
        const bool brkOn = (targetG != nullptr) && annotCheck->isChecked();
        bracketFmtCombo->setEnabled(brkOn);
        showNsCheck->setEnabled(brkOn);
        wilcoxonNoteLbl->setVisible(mode == 3 && colList->selectedItems().size() > 2);
    };

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
        refreshControlGroups();
    };
    populateCols();
    syncControlStates();

    QObject::connect(tableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) {
                         populateCols();
                         syncControlStates();
                     });
    QObject::connect(colList, &QListWidget::itemSelectionChanged, &dlg, [&]() {
        refreshControlGroups();
        syncControlStates();
    });
    QObject::connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { syncControlStates(); });
    QObject::connect(posthocCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { syncControlStates(); });
    QObject::connect(annotCheck, &QCheckBox::toggled, &dlg, [&](bool) { syncControlStates(); });

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

    Table *chosenTbl = tables[tableCombo->currentIndex()];
    Graph *annTarget = annotCheck->isChecked() ? targetG : nullptr;

    struct ScopedDialogAnovaProps {
        Table *tbl = nullptr;
        QVariant oldPosthoc, oldControl, oldShowNs, oldFmt;
        ScopedDialogAnovaProps(Table *t, const QString &ph, const QString &ctrl, bool showNs,
                               const QString &fmt)
            : tbl(t)
        {
            if (!tbl)
                return;
            oldPosthoc = tbl->property("torigraph_anova_posthoc");
            oldControl = tbl->property("torigraph_anova_control");
            oldShowNs = tbl->property("torigraph_anova_show_ns");
            oldFmt = tbl->property("torigraph_anova_bracket_format");
            tbl->setProperty("torigraph_anova_posthoc", ph);
            tbl->setProperty("torigraph_anova_control", ctrl);
            tbl->setProperty("torigraph_anova_show_ns", showNs);
            tbl->setProperty("torigraph_anova_bracket_format", fmt);
        }
        ~ScopedDialogAnovaProps()
        {
            if (!tbl)
                return;
            tbl->setProperty("torigraph_anova_posthoc", oldPosthoc);
            tbl->setProperty("torigraph_anova_control", oldControl);
            tbl->setProperty("torigraph_anova_show_ns", oldShowNs);
            tbl->setProperty("torigraph_anova_bracket_format", oldFmt);
        }
    } scopedProps(chosenTbl, posthocCombo->currentData().toString(), controlCombo->currentText(),
                  showNsCheck->isChecked(), bracketFmtCombo->currentData().toString());

    const int mode = modeCombo->currentData().toInt();
    if (mode == 0) {
        runGroupComparisonAndAnnotate(app, chosenTbl, selected, annTarget);
    } else if (mode == 1) {
        runNonparametricComparison(app, chosenTbl, selected, TestAutoSmartSwitch, annTarget);
    } else if (mode == 2) {
        const NonparamMethod nm =
                (selected.size() == 2) ? TestMannWhitneyU : TestKruskalWallisDunn;
        runNonparametricComparison(app, chosenTbl, selected, nm, annTarget);
    } else {
        runNonparametricComparison(app, chosenTbl, selected, TestWilcoxonSignedRank, annTarget);
    }
}

void GraphStatsAndSurfaceFit::showROCDialog(ApplicationWindow *app)
{
    if (!app || isNonInteractiveApp(app))
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
    if (!app || isNonInteractiveApp(app))
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
    if (!app || isNonInteractiveApp(app))
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

    QLabel *hintLbl = new QLabel(
            QObject::tr("Select at least 2 numeric columns (X columns are excluded by default)."),
            &dlg);
    hintLbl->setWordWrap(true);
    hintLbl->setStyleSheet(QStringLiteral("QLabel { color: #b45309; font-size: 11px; }"));
    lay->addWidget(hintLbl);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QPushButton *okBtn = btns->button(QDialogButtonBox::Ok);
    auto updateSelectionState = [&]() {
        const bool ok = (colList->selectedItems().size() >= 2);
        if (okBtn)
            okBtn->setEnabled(ok);
        hintLbl->setVisible(!ok);
    };

    auto populateCols = [&]() {
        QSignalBlocker blk(colList);
        colList->clear();
        int idx = tableCombo->currentIndex();
        if (idx < 0 || idx >= tables.size()) {
            updateSelectionState();
            return;
        }
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (columnHasNumericValues(col)) {
                QListWidgetItem *it = new QListWidgetItem(col->name(), colList);
                const bool isX = (col && col->plotDesignation() == SciDAVis::X);
                it->setSelected(!isX);
            }
        }
        updateSelectionState();
    };
    populateCols();
    QObject::connect(tableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { populateCols(); });
    QObject::connect(colList, &QListWidget::itemSelectionChanged, &dlg, updateSelectionState);

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

// ============================================================================
// Normality Tests + Histogram PDF Overlay, Bland-Altman Plot, & Two-Way ANOVA
// ============================================================================

GraphStatsAndSurfaceFit::NormalityResult
GraphStatsAndSurfaceFit::runNormalityTest(ApplicationWindow *app, Table *srcTable,
                                          const QString &colName,
                                          Graph *histogramGraphToOverlay)
{
    NormalityResult res;
    if (!srcTable)
        return res;

    Column *col = resolveTableColumn(srcTable, colName);
    if (!col)
        return res;

    std::vector<double> vals;
    vals.reserve(static_cast<size_t>(col->rowCount()));
    for (int r = 0; r < col->rowCount(); ++r) {
        double v = 0.0;
        if (readNumericColumnCell(col, r, v))
            vals.push_back(v);
    }

    const int n = static_cast<int>(vals.size());
    res.n = n;
    if (n < 3) {
        res.isNormalAt005 = false;
        return res;
    }

    std::sort(vals.begin(), vals.end());
    const double mean = std::accumulate(vals.begin(), vals.end(), 0.0) / static_cast<double>(n);
    double ss = 0.0;
    for (double v : vals) {
        const double d = v - mean;
        ss += d * d;
    }
    const double sd = std::sqrt(ss / static_cast<double>(n - 1));

    res.mean = mean;
    res.sd = sd;

    if (sd <= 1e-30) {
        res.shapiroWilkW = 0.0;
        res.shapiroWilkP = 0.0;
        res.ksD = 1.0;
        res.ksP = 0.0;
        res.andersonDarlingA2 = 1e6;
        res.isNormalAt005 = false;
        return res;
    }

    computeShapiroWilkRoyston(vals, ss, res.shapiroWilkW, res.shapiroWilkP);
    computeLillieforsKS(vals, mean, sd, res.ksD, res.ksP);
    double adP = 1.0;
    computeAndersonDarlingStephens(vals, mean, sd, res.andersonDarlingA2, adP);
    res.isNormalAt005 = (res.shapiroWilkP >= 0.05);

    if (!app)
        return res;

    // Create Normality Summary Table
    QString tblName =
            app->generateUniqueName(srcTable->name() + QStringLiteral("_Normality"), false);
    Table *repTbl = app->newTable(4, 4, tblName, QObject::tr("Normality Test Report"));
    if (repTbl) {
        repTbl->setHeader(QStringList() << QStringLiteral("Test") << QStringLiteral("Statistic")
                                        << QStringLiteral("p_value")
                                        << QStringLiteral("Decision_005"));
        const QStringList normLongNames = { QObject::tr("Test"), QObject::tr("Statistic"),
                                            QObject::tr("p_value"), QObject::tr("Decision_005") };
        repTbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        repTbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        repTbl->column(2)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        repTbl->column(3)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 0; c < 4; ++c)
            repTbl->column(c)->setLongName(normLongNames[c]);

        const QString normalYes = QObject::tr("Normal (p >= 0.05)");
        const QString normalNo = QObject::tr("Reject Normality (p < 0.05)");

        repTbl->column(0)->setTextAt(0, QObject::tr("Sample Summary (N=%1, SD=%2)")
                                                .arg(n)
                                                .arg(sd, 0, 'g', 5));
        repTbl->column(1)->setValueAt(0, mean);
        repTbl->column(2)->setValueAt(0, sd);
        repTbl->column(3)->setTextAt(0, res.isNormalAt005 ? normalYes : normalNo);

        repTbl->column(0)->setTextAt(1, QObject::tr("Shapiro-Wilk W"));
        repTbl->column(1)->setValueAt(1, res.shapiroWilkW);
        repTbl->column(2)->setValueAt(1, res.shapiroWilkP);
        repTbl->column(3)->setTextAt(1, (res.shapiroWilkP >= 0.05) ? normalYes : normalNo);

        repTbl->column(0)->setTextAt(2, QObject::tr("Kolmogorov-Smirnov (Lilliefors) D"));
        repTbl->column(1)->setValueAt(2, res.ksD);
        repTbl->column(2)->setValueAt(2, res.ksP);
        repTbl->column(3)->setTextAt(2, (res.ksP >= 0.05) ? normalYes : normalNo);

        repTbl->column(0)->setTextAt(3, QObject::tr("Anderson-Darling A*2"));
        repTbl->column(1)->setValueAt(3, res.andersonDarlingA2);
        repTbl->column(2)->setValueAt(3, adP);
        repTbl->column(3)->setTextAt(3, (adP >= 0.05) ? normalYes : normalNo);

        res.reportTable = repTbl;
    }

    // Optional Normal PDF curve + Arial summary callout overlaid on histogramGraphToOverlay
    if (histogramGraphToOverlay) {
        double areaScale = 1.0;
        double xDomainMin = std::min(vals.front(), mean - 3.8 * sd);
        double xDomainMax = std::max(vals.back(), mean + 3.8 * sd);
        double peakY = 0.0;

        for (int k = 0; k < histogramGraphToOverlay->curves(); ++k) {
            QwtPlotCurve *cv = histogramGraphToOverlay->curve(k);
            if (!cv)
                continue;
            if (auto *hist = dynamic_cast<QwtHistogram *>(cv)) {
                const double bw = hist->binSize();
                double sumCounts = 0.0;
                for (int j = 0; j < hist->dataSize(); ++j)
                    sumCounts += std::max(0.0, hist->y(j));
                if (bw > 0.0 && sumCounts > 0.0)
                    areaScale = sumCounts * bw;
                else if (bw > 0.0)
                    areaScale = static_cast<double>(n) * bw;
                xDomainMin = std::min(xDomainMin, hist->begin());
                xDomainMax = std::max(xDomainMax, hist->end());
                break;
            }
            if (cv->dataSize() >= 2) {
                const double dx = std::fabs(cv->x(1) - cv->x(0));
                double sumH = 0.0;
                for (int j = 0; j < cv->dataSize(); ++j)
                    sumH += std::max(0.0, cv->y(j));
                if (dx > 0.0 && sumH > 0.0)
                    areaScale = sumH * dx;
                break;
            }
        }

        constexpr int kGrid = 201;
        std::vector<double> gx(kGrid), gy(kGrid);
        Column *colX = new Column(QStringLiteral("X"), SciDAVis::ColumnMode::Numeric);
        Column *colY = new Column(QStringLiteral("NormalPDF"), SciDAVis::ColumnMode::Numeric);
        colX->setPlotDesignation(SciDAVis::X);
        colY->setPlotDesignation(SciDAVis::Y);

        const double normConst = areaScale / (sd * std::sqrt(2.0 * kPi));
        for (int i = 0; i < kGrid; ++i) {
            const double xq = xDomainMin
                    + (xDomainMax - xDomainMin) * static_cast<double>(i)
                            / static_cast<double>(kGrid - 1);
            const double z = (xq - mean) / sd;
            const double yq = normConst * std::exp(-0.5 * z * z);
            gx[static_cast<size_t>(i)] = xq;
            gy[static_cast<size_t>(i)] = yq;
            peakY = std::max(peakY, yq);
            colX->setValueAt(i, xq);
            colY->setValueAt(i, yq);
        }

        QString hidName = app->generateUniqueName(QStringLiteral("NormalOverlay"));
        Table *backing = app->newHiddenTable(
                hidName, QObject::tr("Normal PDF Overlay (%1)").arg(col->name()),
                QList<Column *>() << colX << colY);
        if (backing) {
            DataCurve *pdfCurve = new DataCurve(backing, hidName + "_" + colX->name(),
                                                hidName + "_" + colY->name());
            pdfCurve->setData(gx.data(), gy.data(), kGrid);
            pdfCurve->setPen(QPen(QColor(200, 30, 35), 2, Qt::SolidLine));
            histogramGraphToOverlay->insertPlotItem(pdfCurve, Graph::Line);
            histogramGraphToOverlay->addFitCurve(pdfCurve);
            res.normalOverlayCurve = pdfCurve;
        }

        const QString annot = QObject::tr("Normality (%1, N = %2)\n"
                                          "Mean = %3, SD = %4\n"
                                          "Shapiro-Wilk W = %5 (p = %6)\n"
                                          "KS D = %7, AD A*2 = %8")
                                      .arg(col->name())
                                      .arg(n)
                                      .arg(mean, 0, 'g', 5)
                                      .arg(sd, 0, 'g', 5)
                                      .arg(res.shapiroWilkW, 0, 'f', 4)
                                      .arg(res.shapiroWilkP, 0, 'g', 4)
                                      .arg(res.ksD, 0, 'f', 4)
                                      .arg(res.andersonDarlingA2, 0, 'f', 4);

        const double boxX = xDomainMin + 0.56 * (xDomainMax - xDomainMin);
        const double boxY = std::max(0.1, 0.88 * peakY);
        Legend *normBox =
                addFramedBoxOnGraph(histogramGraphToOverlay, boxX, boxY, annot, QColor(20, 30, 50), 9);
        if (normBox)
            attachStatsCalloutAutoAvoid(histogramGraphToOverlay, normBox, 14);
        histogramGraphToOverlay->updatePlot();
    }

    return res;
}

MultiLayer *GraphStatsAndSurfaceFit::plotBlandAltman(ApplicationWindow *app, Table *srcTable,
                                                     const QString &methodACol,
                                                     const QString &methodBCol,
                                                     double *meanBiasOut,
                                                     double *loaLowerOut,
                                                     double *loaUpperOut)
{
    if (meanBiasOut)
        *meanBiasOut = 0.0;
    if (loaLowerOut)
        *loaLowerOut = 0.0;
    if (loaUpperOut)
        *loaUpperOut = 0.0;
    if (!app || !srcTable)
        return nullptr;

    Column *colA = resolveTableColumn(srcTable, methodACol);
    Column *colB = resolveTableColumn(srcTable, methodBCol);
    if (!colA || !colB)
        return nullptr;

    const int maxRows = std::min(colA->rowCount(), colB->rowCount());
    std::vector<double> means;
    std::vector<double> diffs;
    means.reserve(static_cast<size_t>(maxRows));
    diffs.reserve(static_cast<size_t>(maxRows));

    for (int r = 0; r < maxRows; ++r) {
        double a = 0.0, b = 0.0;
        if (readNumericColumnCell(colA, r, a) && readNumericColumnCell(colB, r, b)) {
            means.push_back(0.5 * (a + b));
            diffs.push_back(a - b);
        }
    }

    const int n = static_cast<int>(diffs.size());
    if (n < 2)
        return nullptr;

    const double meanBias =
            std::accumulate(diffs.begin(), diffs.end(), 0.0) / static_cast<double>(n);
    double ssDiff = 0.0;
    for (double d : diffs) {
        const double dev = d - meanBias;
        ssDiff += dev * dev;
    }
    const double sdDiff = std::sqrt(ssDiff / static_cast<double>(n - 1));
    const double loaUpper = meanBias + 1.96 * sdDiff;
    const double loaLower = meanBias - 1.96 * sdDiff;

    if (meanBiasOut)
        *meanBiasOut = meanBias;
    if (loaLowerOut)
        *loaLowerOut = loaLower;
    if (loaUpperOut)
        *loaUpperOut = loaUpper;

    QString tblName =
            app->generateUniqueName(srcTable->name() + QStringLiteral("_BlandAltman"), false);
    Table *baTbl = app->newTable(n, 5, tblName, QObject::tr("Bland-Altman Method Comparison"));
    if (!baTbl)
        return nullptr;

    baTbl->setHeader(QStringList() << QStringLiteral("Mean_AB") << QStringLiteral("Diff_A_minus_B")
                                   << QStringLiteral("MeanBias") << QStringLiteral("UpperLoA")
                                   << QStringLiteral("LowerLoA"));
    for (int c = 0; c < 5; ++c) {
        baTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        baTbl->column(c)->setPlotDesignation(c == 0 ? SciDAVis::X : SciDAVis::Y);
    }

    double mMin = means.front(), mMax = means.front();
    double dMin = diffs.front(), dMax = diffs.front();
    for (int i = 0; i < n; ++i) {
        const double mv = means[static_cast<size_t>(i)];
        const double dv = diffs[static_cast<size_t>(i)];
        mMin = std::min(mMin, mv);
        mMax = std::max(mMax, mv);
        dMin = std::min(dMin, dv);
        dMax = std::max(dMax, dv);
        baTbl->column(0)->setValueAt(i, mv);
        baTbl->column(1)->setValueAt(i, dv);
        baTbl->column(2)->setValueAt(i, meanBias);
        baTbl->column(3)->setValueAt(i, loaUpper);
        baTbl->column(4)->setValueAt(i, loaLower);
    }

    MultiLayer *ml = app->newGraph(
            app->generateUniqueName(QStringLiteral("BlandAltman_") + srcTable->name(), false));
    if (!ml)
        return nullptr;

    Graph *g = ml->activeGraph();
    if (g) {
        if (g->insertCurve(baTbl, baTbl->colName(0), baTbl->colName(1), Graph::Scatter, 0, n - 1)) {
            const int cIdx = g->curves() - 1;
            QwtSymbol sym(QwtSymbol::Ellipse, QBrush(QColor(35, 105, 195, 195)),
                          QPen(QColor(20, 65, 135), 1), QSize(8, 8));
            g->setCurveSymbol(cIdx, sym);
        }

        const double xSpan = std::max(1e-6, mMax - mMin);
        const double xLo = mMin - 0.08 * xSpan;
        const double xHi = mMax + 0.24 * xSpan;

        const double yRawMin = std::min(dMin, loaLower);
        const double yRawMax = std::max(dMax, loaUpper);
        const double ySpan = std::max(1e-6, yRawMax - yRawMin);
        const double yLo = yRawMin - 0.18 * ySpan;
        const double yHi = yRawMax + 0.20 * ySpan;

        g->setScale(QwtPlot::xBottom, xLo, xHi);
        g->setScale(QwtPlot::yLeft, yLo, yHi);
        g->setXAxisTitle(QObject::tr("Mean of %1 and %2").arg(colA->name(), colB->name()));
        g->setYAxisTitle(QObject::tr("Difference (%1 - %2)").arg(colA->name(), colB->name()));
        g->setTitle(QObject::tr("Bland-Altman Plot (%1 vs %2)").arg(colA->name(), colB->name()));
        if (g->legend())
            g->legend()->setText(g->legendText());
        applyArialGraphFonts(g);
        g->replot();

        const QColor biasColor(20, 90, 200);
        const QColor loaColor(205, 35, 35);

        // Solid blue Mean Bias horizontal line & dashed red +/- 1.96 SD LoA horizontal lines
        addLineSegmentOnGraph(g, xLo, meanBias, xHi, meanBias, biasColor, 2, Qt::SolidLine, false);
        addLineSegmentOnGraph(g, xLo, loaUpper, xHi, loaUpper, loaColor, 2, Qt::DashLine, false);
        addLineSegmentOnGraph(g, xLo, loaLower, xHi, loaLower, loaColor, 2, Qt::DashLine, false);

        const double calloutX = mMax + 0.02 * xSpan;
        const double dyLabel = 0.055 * ySpan;
        addTextLabelOnGraph(g, calloutX, meanBias + dyLabel,
                            QObject::tr("Mean = %1").arg(meanBias, 0, 'g', 5), biasColor, true, -1);
        addTextLabelOnGraph(g, calloutX, loaUpper + dyLabel,
                            QObject::tr("+1.96 SD = %1").arg(loaUpper, 0, 'g', 5), loaColor, true,
                            -1);
        addTextLabelOnGraph(g, calloutX, loaLower + dyLabel,
                            QObject::tr("-1.96 SD = %1").arg(loaLower, 0, 'g', 5), loaColor, true,
                            -1);
        g->replot();
    }

    return ml;
}

GraphStatsAndSurfaceFit::TwoWayANOVAResult
GraphStatsAndSurfaceFit::runTwoWayANOVA(ApplicationWindow *app, Table *srcTable,
                                        const QString &factorACol,
                                        const QString &factorBCol,
                                        const QString &responseCol,
                                        bool plotInteraction)
{
    TwoWayANOVAResult res;
    if (!app || !srcTable)
        return res;

    Column *colA = resolveTableColumn(srcTable, factorACol);
    Column *colB = resolveTableColumn(srcTable, factorBCol);
    Column *colY = resolveTableColumn(srcTable, responseCol);
    if (!colA || !colB || !colY)
        return res;

    auto readFactorLevel = [](Column *c, int r, QString &out) -> bool {
        if (!c || r < 0 || r >= c->rowCount() || c->isInvalid(r) || c->isMasked(r))
            return false;
        if (c->columnMode() == SciDAVis::ColumnMode::Numeric) {
            const double v = c->valueAt(r);
            if (!std::isfinite(v))
                return false;
            out = QString::number(v, 'g', 10);
            return true;
        }
        out = c->asStringColumn()->textAt(r).trimmed();
        return !out.isEmpty();
    };

    struct AnovaObs
    {
        int iA = 0;
        int jB = 0;
        double y = 0.0;
    };

    const int maxRows = std::min({ colA->rowCount(), colB->rowCount(), colY->rowCount() });
    QStringList levelsA;
    QStringList levelsB;
    std::vector<std::tuple<QString, QString, double>> rawTriples;
    rawTriples.reserve(static_cast<size_t>(maxRows));

    for (int r = 0; r < maxRows; ++r) {
        double yv = 0.0;
        if (!readNumericColumnCell(colY, r, yv))
            continue;
        QString sA, sB;
        if (!readFactorLevel(colA, r, sA) || !readFactorLevel(colB, r, sB))
            continue;
        if (!levelsA.contains(sA))
            levelsA.append(sA);
        if (!levelsB.contains(sB))
            levelsB.append(sB);
        rawTriples.emplace_back(sA, sB, yv);
    }

    const int I = levelsA.size();
    const int J = levelsB.size();
    const int N = static_cast<int>(rawTriples.size());
    if (I < 2 || J < 2 || N <= I * J)
        return res;

    std::vector<AnovaObs> obs(static_cast<size_t>(N));
    std::vector<double> yVec(static_cast<size_t>(N), 0.0);
    for (int k = 0; k < N; ++k) {
        obs[static_cast<size_t>(k)].iA = levelsA.indexOf(std::get<0>(rawTriples[static_cast<size_t>(k)]));
        obs[static_cast<size_t>(k)].jB = levelsB.indexOf(std::get<1>(rawTriples[static_cast<size_t>(k)]));
        obs[static_cast<size_t>(k)].y = std::get<2>(rawTriples[static_cast<size_t>(k)]);
        yVec[static_cast<size_t>(k)] = obs[static_cast<size_t>(k)].y;
    }

    // Sum-to-zero effect-coded OLS design matrices (exact for balanced & Type III unbalanced)
    const int nA = I - 1;
    const int nB = J - 1;
    const int nAB = nA * nB;

    auto buildDesign = [&](bool incA, bool incB, bool incAB) {
        const size_t pCols = static_cast<size_t>(1 + (incA ? nA : 0) + (incB ? nB : 0)
                                                 + (incAB ? nAB : 0));
        std::vector<std::vector<double>> X(static_cast<size_t>(N), std::vector<double>(pCols, 0.0));
        for (int k = 0; k < N; ++k) {
            const int ia = obs[static_cast<size_t>(k)].iA;
            const int jb = obs[static_cast<size_t>(k)].jB;
            size_t colIdx = 0;
            X[static_cast<size_t>(k)][colIdx++] = 1.0;

            if (incA) {
                for (int u = 0; u < nA; ++u) {
                    X[static_cast<size_t>(k)][colIdx++] =
                            (ia == u) ? 1.0 : ((ia == I - 1) ? -1.0 : 0.0);
                }
            }
            if (incB) {
                for (int v = 0; v < nB; ++v) {
                    X[static_cast<size_t>(k)][colIdx++] =
                            (jb == v) ? 1.0 : ((jb == J - 1) ? -1.0 : 0.0);
                }
            }
            if (incAB) {
                for (int u = 0; u < nA; ++u) {
                    const double ca = (ia == u) ? 1.0 : ((ia == I - 1) ? -1.0 : 0.0);
                    for (int v = 0; v < nB; ++v) {
                        const double cb = (jb == v) ? 1.0 : ((jb == J - 1) ? -1.0 : 0.0);
                        X[static_cast<size_t>(k)][colIdx++] = ca * cb;
                    }
                }
            }
        }
        return X;
    };

    double rssFull = 0.0, rssNoAB = 0.0, rssNoA = 0.0, rssNoB = 0.0;
    int rkFull = 0, rkNoAB = 0, rkNoA = 0, rkNoB = 0;
    if (!solveOlsRssAndRank(buildDesign(true, true, true), yVec, rssFull, rkFull))
        return res;
    if (!solveOlsRssAndRank(buildDesign(true, true, false), yVec, rssNoAB, rkNoAB))
        return res;
    if (!solveOlsRssAndRank(buildDesign(false, true, true), yVec, rssNoA, rkNoA))
        return res;
    if (!solveOlsRssAndRank(buildDesign(true, false, true), yVec, rssNoB, rkNoB))
        return res;

    const int dfErrorInt = N - rkFull;
    if (dfErrorInt < 1)
        return res;

    double ssA = std::max(0.0, rssNoA - rssFull);
    int dfAInt = rkFull - rkNoA;
    double ssB = std::max(0.0, rssNoB - rssFull);
    int dfBInt = rkFull - rkNoB;
    const double ssAB = std::max(0.0, rssNoAB - rssFull);
    const int dfABInt = std::max(1, rkFull - rkNoAB);

    // Fallback to Type II main effects if an empty cell made Type III main-effect contrast aliased
    if (dfAInt < 1) {
        double rssOnlyB = 0.0;
        int rkOnlyB = 0;
        if (solveOlsRssAndRank(buildDesign(false, true, false), yVec, rssOnlyB, rkOnlyB)) {
            ssA = std::max(0.0, rssOnlyB - rssNoAB);
            dfAInt = std::max(1, rkNoAB - rkOnlyB);
        } else {
            dfAInt = nA;
        }
    }
    if (dfBInt < 1) {
        double rssOnlyA = 0.0;
        int rkOnlyA = 0;
        if (solveOlsRssAndRank(buildDesign(true, false, false), yVec, rssOnlyA, rkOnlyA)) {
            ssB = std::max(0.0, rssOnlyA - rssNoAB);
            dfBInt = std::max(1, rkNoAB - rkOnlyA);
        } else {
            dfBInt = nB;
        }
    }

    const double grandMean = std::accumulate(yVec.begin(), yVec.end(), 0.0) / static_cast<double>(N);
    double ssTotal = 0.0;
    for (double yv : yVec) {
        const double d = yv - grandMean;
        ssTotal += d * d;
    }

    const double dfA = static_cast<double>(dfAInt);
    const double dfB = static_cast<double>(dfBInt);
    const double dfAB = static_cast<double>(dfABInt);
    const double dfE = static_cast<double>(dfErrorInt);

    const double msA = ssA / dfA;
    const double msB = ssB / dfB;
    const double msAB = ssAB / dfAB;
    const double msE = rssFull / dfE;

    auto evalFAndP = [&](double msEff, double dfEff, double &fOut, double &pOut) {
        if (msE > 1e-30) {
            fOut = msEff / msE;
            pOut = (fOut > 0.0) ? std::clamp(gsl_cdf_fdist_Q(fOut, dfEff, dfE), 0.0, 1.0) : 1.0;
        } else if (msEff > 1e-30) {
            fOut = 1e12;
            pOut = 0.0;
        } else {
            fOut = 0.0;
            pOut = 1.0;
        }
    };

    evalFAndP(msA, dfA, res.fFactorA, res.pFactorA);
    evalFAndP(msB, dfB, res.fFactorB, res.pFactorB);
    evalFAndP(msAB, dfAB, res.fInteraction, res.pInteraction);
    res.valid = true;

    // Build Two-Way ANOVA Summary Table
    QString repName =
            app->generateUniqueName(srcTable->name() + QStringLiteral("_TwoWayANOVA"), false);
    Table *repTbl = app->newTable(5, 7, repName, QObject::tr("Two-Way ANOVA Summary"));
    if (repTbl) {
        repTbl->setHeader(QStringList() << QStringLiteral("Source") << QStringLiteral("SS")
                                        << QStringLiteral("df") << QStringLiteral("MS")
                                        << QStringLiteral("F_value") << QStringLiteral("p_value")
                                        << QStringLiteral("Significance"));
        const QStringList twoWayLongNames = {
            QObject::tr("Source"),  QObject::tr("SS"),      QObject::tr("df"),
            QObject::tr("MS"),      QObject::tr("F_value"), QObject::tr("p_value"),
            QObject::tr("Significance")
        };
        repTbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 1; c <= 5; ++c)
            repTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        repTbl->column(6)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 0; c < 7; ++c)
            repTbl->column(c)->setLongName(twoWayLongNames[c]);

        auto setRow = [&](int r, const QString &src, double ssVal, double dfVal, double msVal,
                          double fVal, double pVal, bool hasF) {
            repTbl->column(0)->setTextAt(r, src);
            repTbl->column(1)->setValueAt(r, ssVal);
            repTbl->column(2)->setValueAt(r, dfVal);
            if (std::isfinite(msVal))
                repTbl->column(3)->setValueAt(r, msVal);
            else
                repTbl->column(3)->setInvalid(r, true);
            if (hasF) {
                repTbl->column(4)->setValueAt(r, fVal);
                repTbl->column(5)->setValueAt(r, pVal);
                repTbl->column(6)->setTextAt(r, starRatingForP(pVal));
            } else {
                repTbl->column(4)->setInvalid(r, true);
                repTbl->column(5)->setInvalid(r, true);
                repTbl->column(6)->setTextAt(r, QString());
            }
        };

        setRow(0, colA->name(), ssA, dfA, msA, res.fFactorA, res.pFactorA, true);
        setRow(1, colB->name(), ssB, dfB, msB, res.fFactorB, res.pFactorB, true);
        setRow(2, QStringLiteral("%1 x %2").arg(colA->name(), colB->name()), ssAB, dfAB, msAB,
               res.fInteraction, res.pInteraction, true);
        setRow(3, QObject::tr("Error (Within)"), rssFull, dfE, msE, 0.0, 1.0, false);
        setRow(4, QObject::tr("Total"), ssTotal, static_cast<double>(N - 1),
               std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0, false);
        res.reportTable = repTbl;
    }

    // Optional Mean +/- SE Interaction Plot MultiLayer
    if (plotInteraction) {
        QString intTblName = app->generateUniqueName(
                srcTable->name() + QStringLiteral("_InteractionCells"), false);
        const int nColsInt = 2 + 2 * J;
        Table *intTbl =
                app->newTable(I, nColsInt, intTblName, QObject::tr("Two-Way Interaction Means"));
        if (intTbl) {
            QStringList hdr;
            hdr << QStringLiteral("FactorA_Index") << colA->name();
            for (int j = 0; j < J; ++j) {
                QString cleanB = levelsB[j];
                cleanB.replace(' ', '_');
                hdr << QStringLiteral("Mean_%1").arg(cleanB)
                    << QStringLiteral("SE_%1").arg(cleanB);
            }
            intTbl->setHeader(hdr);
            intTbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            intTbl->column(0)->setPlotDesignation(SciDAVis::X);
            intTbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Text);

            for (int j = 0; j < J; ++j) {
                intTbl->column(2 + 2 * j)->setColumnMode(SciDAVis::ColumnMode::Numeric);
                intTbl->column(2 + 2 * j)->setPlotDesignation(SciDAVis::Y);
                intTbl->column(2 + 2 * j + 1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
                intTbl->column(2 + 2 * j + 1)->setPlotDesignation(SciDAVis::yErr);
            }

            double yMinPlot = std::numeric_limits<double>::max();
            double yMaxPlot = -std::numeric_limits<double>::max();

            for (int i = 0; i < I; ++i) {
                intTbl->column(0)->setValueAt(i, static_cast<double>(i + 1));
                intTbl->column(1)->setTextAt(i, levelsA[i]);
                for (int j = 0; j < J; ++j) {
                    std::vector<double> cellVals;
                    for (const AnovaObs &o : obs) {
                        if (o.iA == i && o.jB == j)
                            cellVals.push_back(o.y);
                    }
                    const int meanCol = 2 + 2 * j;
                    const int seCol = meanCol + 1;
                    if (!cellVals.empty()) {
                        const double cm =
                                std::accumulate(cellVals.begin(), cellVals.end(), 0.0)
                                / static_cast<double>(cellVals.size());
                        double cse = 0.0;
                        if (cellVals.size() >= 2) {
                            double css = 0.0;
                            for (double cv : cellVals) {
                                const double d = cv - cm;
                                css += d * d;
                            }
                            cse = std::sqrt(css
                                            / (static_cast<double>(cellVals.size())
                                               * static_cast<double>(cellVals.size() - 1)));
                        }
                        intTbl->column(meanCol)->setValueAt(i, cm);
                        intTbl->column(seCol)->setValueAt(i, cse);
                        yMinPlot = std::min(yMinPlot, cm - cse);
                        yMaxPlot = std::max(yMaxPlot, cm + cse);
                    } else {
                        intTbl->column(meanCol)->setInvalid(i, true);
                        intTbl->column(seCol)->setInvalid(i, true);
                    }
                }
            }

            MultiLayer *ml = app->newGraph(app->generateUniqueName(
                    QStringLiteral("Interaction_") + srcTable->name(), false));
            if (ml) {
                Graph *g = ml->activeGraph();
                if (g) {
                    static const QColor kTraceColors[] = {
                        QColor(25, 95, 195),  QColor(210, 45, 45),  QColor(25, 150, 75),
                        QColor(155, 55, 180), QColor(225, 130, 20), QColor(20, 155, 175)
                    };
                    static const QwtSymbol::Style kSyms[] = {
                        QwtSymbol::Ellipse, QwtSymbol::Rect,     QwtSymbol::Triangle,
                        QwtSymbol::Diamond, QwtSymbol::UTriangle, QwtSymbol::DTriangle
                    };

                    for (int j = 0; j < J; ++j) {
                        const int meanCol = 2 + 2 * j;
                        const int seCol = meanCol + 1;
                        const QColor tc = kTraceColors[j % 6];
                        if (g->insertCurve(intTbl, intTbl->colName(0), intTbl->colName(meanCol),
                                           Graph::LineSymbols, 0, I - 1)) {
                            const int cIdx = g->curves() - 1;
                            g->setCurvePen(cIdx, QPen(tc, 2, Qt::SolidLine));
                            QwtSymbol sym(kSyms[j % 6], QBrush(tc), QPen(tc.darker(125), 1),
                                          QSize(8, 8));
                            g->setCurveSymbol(cIdx, sym);
                            g->addErrorBars(intTbl->colName(0), intTbl->colName(meanCol), intTbl,
                                            intTbl->colName(seCol), 1, 1, 6, tc, true, true, true);
                        }
                    }

                    g->setLabelsTextFormat(Graph::Bottom, intTbl->column(1), 0, I - 1);
                    const double ySpan = (yMaxPlot > yMinPlot) ? (yMaxPlot - yMinPlot) : 1.0;
                    g->setScale(QwtPlot::xBottom, 0.6, static_cast<double>(I) + 0.4, 1.0);
                    g->setScale(QwtPlot::yLeft, yMinPlot - 0.15 * ySpan, yMaxPlot + 0.22 * ySpan);
                    g->setXAxisTitle(colA->name());
                    g->setYAxisTitle(QObject::tr("%1 (Mean ± SE)").arg(colY->name()));
                    g->setTitle(QObject::tr("Two-Way Interaction Plot (%1 × %2, p_AB = %3)")
                                        .arg(colA->name(), colB->name())
                                        .arg(res.pInteraction, 0, 'g', 4));
                    if (g->legend())
                        g->legend()->setText(g->legendText());
                    applyArialGraphFonts(g);
                    g->replot();
                }
                res.interactionGraph = ml;
            }
        }
    }

    return res;
}

void GraphStatsAndSurfaceFit::showNormalityDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || isNonInteractiveApp(app))
        return;
    QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    Graph *targetG = resolveActiveGraph(app, g);

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Normality Test (Shapiro-Wilk / KS / Anderson-Darling)"));
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

    QComboBox *colCombo = new QComboBox(&dlg);
    form->addRow(QObject::tr("Data Column:"), colCombo);

    QCheckBox *overlayCheck =
            new QCheckBox(QObject::tr("Overlay Fitted Normal PDF Curve on Active Graph"), &dlg);
    overlayCheck->setChecked(targetG != nullptr);
    overlayCheck->setEnabled(targetG != nullptr);
    form->addRow(QString(), overlayCheck);
    lay->addLayout(form);

    auto refreshCols = [&]() {
        colCombo->clear();
        int idx = tableCombo->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            if (columnHasNumericValues(t->column(c)))
                colCombo->addItem(t->column(c)->name());
        }
        if (colCombo->count() == 0) {
            for (int c = 0; c < t->numCols(); ++c)
                colCombo->addItem(t->column(c)->name());
        }
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

    runNormalityTest(app, tables[tableCombo->currentIndex()], colCombo->currentText(),
                     overlayCheck->isChecked() ? targetG : nullptr);
}

void GraphStatsAndSurfaceFit::showBlandAltmanDialog(ApplicationWindow *app)
{
    if (!app || isNonInteractiveApp(app))
        return;
    QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Bland-Altman Method Agreement Plot"));
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

    QComboBox *colACombo = new QComboBox(&dlg);
    QComboBox *colBCombo = new QComboBox(&dlg);
    form->addRow(QObject::tr("Method A Column:"), colACombo);
    form->addRow(QObject::tr("Method B Column:"), colBCombo);
    lay->addLayout(form);

    auto refreshCols = [&]() {
        colACombo->clear();
        colBCombo->clear();
        int idx = tableCombo->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            if (!columnHasNumericValues(t->column(c)))
                continue;
            const QString cn = t->column(c)->name();
            colACombo->addItem(cn);
            colBCombo->addItem(cn);
        }
        if (colACombo->count() == 0) {
            for (int c = 0; c < t->numCols(); ++c) {
                const QString cn = t->column(c)->name();
                colACombo->addItem(cn);
                colBCombo->addItem(cn);
            }
        }
        if (colBCombo->count() >= 2)
            colBCombo->setCurrentIndex(1);
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

    plotBlandAltman(app, tables[tableCombo->currentIndex()], colACombo->currentText(),
                    colBCombo->currentText());
}

void GraphStatsAndSurfaceFit::showTwoWayANOVADialog(ApplicationWindow *app)
{
    if (!app || isNonInteractiveApp(app))
        return;
    QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Two-Way ANOVA & Interaction Plot"));
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

    QComboBox *factorACombo = new QComboBox(&dlg);
    QComboBox *factorBCombo = new QComboBox(&dlg);
    QComboBox *respCombo = new QComboBox(&dlg);
    form->addRow(QObject::tr("Factor A Column:"), factorACombo);
    form->addRow(QObject::tr("Factor B Column:"), factorBCombo);
    form->addRow(QObject::tr("Response Column:"), respCombo);

    QCheckBox *plotCheck =
            new QCheckBox(QObject::tr("Create Mean ± SE Interaction Plot"), &dlg);
    plotCheck->setChecked(true);
    form->addRow(QString(), plotCheck);
    lay->addLayout(form);

    auto refreshCols = [&]() {
        factorACombo->clear();
        factorBCombo->clear();
        respCombo->clear();
        int idx = tableCombo->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            const QString cn = t->column(c)->name();
            factorACombo->addItem(cn);
            factorBCombo->addItem(cn);
            if (columnHasNumericValues(t->column(c)))
                respCombo->addItem(cn);
        }
        if (respCombo->count() == 0) {
            for (int c = 0; c < t->numCols(); ++c)
                respCombo->addItem(t->column(c)->name());
        }
        if (factorBCombo->count() >= 2)
            factorBCombo->setCurrentIndex(1);
        if (respCombo->count() >= 1)
            respCombo->setCurrentIndex(respCombo->count() - 1);
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

    runTwoWayANOVA(app, tables[tableCombo->currentIndex()], factorACombo->currentText(),
                   factorBCombo->currentText(), respCombo->currentText(), plotCheck->isChecked());
}

// ============================================================================
// Phase 5: Weibull Probability Plot, Outlier Tests, Nonparametric Smart-Switch,
//          Paired Plot, Correlation Matrix Heatmap, K-Means & Dendrogram
// ============================================================================

namespace {

void registerPhase5StatsTranslationsOnce()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(
                QString::fromUtf8(en), QString::fromUtf8(ja));
    };
    reg("Weibull Probability Plot", "ワイブル確率プロット");
    reg("Weibull Probability Plot...", "ワイブル確率プロット...");
    reg("Outlier Test (Grubbs / ESD / Dixon)", "外れ値検定 (Grubbs / ESD / Dixon)");
    reg("Outlier Test (Grubbs / ESD / Dixon)...", "外れ値検定 (Grubbs / ESD / Dixon)...");
    reg("Nonparametric & Smart-Switch Group Comparison", "ノンパラメトリック検定 / 自動切替群間比較");
    reg("Nonparametric Tests (Mann-Whitney / Kruskal-Wallis / Auto)...",
        "ノンパラメトリック検定 (Mann-Whitney / Kruskal-Wallis / 自動)...");
    reg("Paired Comparison Plot (Before-After)", "対応のある前後比較プロット (Before-After)");
    reg("Paired Comparison Plot (Before-After)...", "対応のある前後比較プロット (Before-After)...");
    reg("Correlation Matrix Heatmap", "相関行列ヒートマップ");
    reg("Correlation Matrix Heatmap...", "相関行列ヒートマップ...");
    reg("Cluster Analysis (K-Means & Hierarchical Dendrogram)",
        "クラスタ解析 (K-Means・階層型デンドログラム)");
    reg("Clustering (K-Means / Dendrogram)...", "クラスタ解析 (K-Means / デンドログラム)...");
    reg("Strength / Lifetime Column:", "強度 / 寿命データ列:");
    reg("Right-Censoring Column (Optional):", "右側打ち切り列 (任意):");
    reg("(None — Complete Failures)", "(なし — 全数破壊データ)");
    reg("Censoring Coding:", "打ち切り符号化方式:");
    reg("1 = Failure/Event, 0 = Censored [Kaplan-Meier Standard]",
        "1 = 破壊/イベント, 0 = 打ち切り [Kaplan-Meier標準]");
    reg("0 = Failure, 1 = Censored (Suspension)",
        "0 = 破壊, 1 = 打ち切り (サスペンション)");
    reg("3-Parameter Weibull (Estimate Threshold γ)",
        "3母数ワイブル解析 (閾値 γ を推定)");
    reg("ln(Strength / Life)", "ln(強度 / 寿命)");
    reg("ln(Strength / Life - γ)", "ln(強度 / 寿命 - γ)");
    reg("3-Parameter Weibull Probability Plot", "3母数ワイブル確率プロット");
    reg("Weibit ln(-ln(1 - F))", "ワイビット ln(-ln(1 - F))");
    reg("Weibull Report", "ワイブル解析レポート");
    reg("Outlier Detection Method:", "外れ値検出手法:");
    reg("Two-Sided Grubbs' Test (Single Outlier)", "両側Grubbs検定 (単一外れ値)");
    reg("Generalized ESD / Rosner Test (Up to k Outliers)", "一般化ESD / Rosner検定 (最大k個)");
    reg("Dixon's Q Test (Small Sample Extreme)", "DixonのQ検定 (小標本極値)");
    reg("Significance Level α:", "有意水準 α:");
    reg("Max Outliers k (Generalized ESD):", "最大外れ値数 k (ESD):");
    reg("Mask Detected Outliers in Source Worksheet", "検出された外れ値を元ワークシート上でマスクする");
    reg("Outlier Test Report", "外れ値検定レポート");
    reg("Test Method:", "検定手法:");
    reg("Auto Smart-Switch (Shapiro-Wilk -> Parametric / Nonparametric)",
        "自動スマート切替 (Shapiro-Wilk正規性判定 -> パラメトリック/ノンパラメトリック)");
    reg("Mann-Whitney U Test (2 Independent Groups)", "Mann-Whitney U検定 (独立2群)");
    reg("Wilcoxon Signed-Rank Test (2 Paired Columns)", "Wilcoxon符号付順位検定 (対応のある2群)");
    reg("Kruskal-Wallis H + Holm-Dunn Post-Hoc", "Kruskal-Wallis H検定 + Holm補正Dunn多重比較");
    reg("Draw Significance Brackets on Active Graph", "アクティブグラフに有意差ブラケットを描画");
    reg("Nonparametric Comparison Summary", "ノンパラメトリック比較サマリー");
    reg("Before Column:", "処理前列 (Before):");
    reg("After Column:", "処理後列 (After):");
    reg("Use Nonparametric Wilcoxon Signed-Rank (instead of Paired t)",
        "対応のあるt検定の代わりにWilcoxon符号付順位検定を使用");
    reg("Value", "測定値");
    reg("Correlation Type:", "相関係数の種類:");
    reg("Pearson Linear Correlation (r)", "Pearson積率相関係数 (r)");
    reg("Spearman Rank Correlation (ρ)", "Spearman順位相関係数 (ρ)");
    reg("Correlation Matrix Heatmap (Spearman ρ)", "相関行列ヒートマップ (Spearman ρ)");
    reg("Correlation Matrix Heatmap (Pearson r)", "相関行列ヒートマップ (Pearson r)");
    reg("Number of Clusters (k):", "クラスタ数 (k):");
    reg("Create 2D Cluster Scatter Plot", "2Dクラスタ散布図を作成");
    reg("K-Means Clustering", "K-Meansクラスタリング");
    reg("K-Means Clustering (k = %1, Silhouette = %2)",
        "K-Meansクラスタリング (k = %1, シルエット係数 = %2)");
    reg("Hierarchical Dendrogram", "階層型デンドログラム");
    reg("Hierarchical Dendrogram (%1 Linkage)", "階層型デンドログラム (%1法)");
    reg("Linkage Criterion:", "連結基準 (Linkage):");
    reg("Ward's Minimum Variance", "Ward法 (最小分散)");
    reg("Complete Linkage (Furthest Neighbor)", "完全連結法 (最遠隣)");
    reg("Average Linkage (UPGMA)", "群平均法 (UPGMA)");
    reg("Dissimilarity Height", "非類似度距離 (Height)");
    reg("Ward", "Ward");
    // "Complete" / "Average" are generic words; their linkage meaning is registered only for
    // the GraphStatsAndSurfaceFit context in i18n/ja_dictionary.tsv (H1).
    reg("Observation", "観測個体");
    reg("Dendrogram", "デンドログラム");
    reg("Passing-Bablok & Deming Regression", "Passing-Bablok / Deming回帰 (測定法比較)");
    reg("Passing-Bablok & Deming Regression...", "Passing-Bablok / Deming回帰 (測定法比較)...");
    reg("Passing-Bablok & Deming Method Comparison", "Passing-Bablok / Deming法による測定法比較");
    reg("Reference Method X Column:", "基準法 X 列:");
    reg("Test Method Y Column:", "評価法 Y 列:");
    reg("Deming Error Variance Ratio λ (σx² / σy²):", "Deming誤差分散比 λ (σx² / σy²):");
    reg("Create Method-Comparison Regression Plot", "測定法比較回帰プロットを作成");
    reg("Passing-Bablok & Deming Regression (%1 vs %2)", "Passing-Bablok / Deming回帰 (%1 vs %2)");
    reg("PB 95% CI", "PB 95%信頼帯");
    reg("Parameter Estimator:", "パラメータ推定法:");
    reg("Median Rank Regression (MRR — Benard)", "メジアンランク回帰法 (MRR — Benard近似)");
    reg("Maximum Likelihood (MLE — JIS R 1625 / ISO 20501)",
        "最尤推定法 (MLE — JIS R 1625 / ISO 20501)");
    reg("Failure probability, <i>P</i><sub>f</sub> (%)", "破壊確率 <i>P</i><sub>f</sub> (%)");
    // F8 / F9: exclusion counts, failure reasons and the estimation method actually used
    reg("Weibull: %1 of %2 row(s) excluded (%3).", "ワイブル: %2 行中 %1 行を除外しました (%3)。");
    reg("%1 non-numeric", "数値でない %1 行");
    reg("%1 with value <= 0 (ln undefined)", "値が 0 以下で対数を取れない %1 行");
    reg("Weibull analysis: the source table or the data column was not found.",
        "ワイブル解析: 元のワークシートまたはデータ列が見つかりません。");
    reg("Weibull analysis needs at least 2 failures (uncensored values > 0); only %1 remained.",
        "ワイブル解析には破壊データ (打切りでない 0 より大きい値) が 2 件以上必要です。残ったのは %1 件です。");
    reg("Weibull analysis failed: all failure values are identical, so the slope cannot be determined.",
        "ワイブル解析に失敗しました: 破壊データがすべて同じ値のため、傾きを決められません。");
    reg("Median rank regression (Benard), 2-parameter", "メジアンランク回帰 (Benard 近似)、2 母数");
    reg("Median rank regression (Benard), 3-parameter: \u03B3 chosen to minimise the regression residual",
        "メジアンランク回帰 (Benard 近似)、3 母数: \u03B3 は回帰の残差が最小になる値");
    reg("Maximum likelihood, 2-parameter", "最尤推定、2 母数");
    reg("3-parameter: \u03B3 estimated by MRR; m and \u03C3\u2080 by MLE conditional on that \u03B3 (their SE does not include the uncertainty of \u03B3)",
        "3 母数: \u03B3 は MRR で推定、m と \u03C3\u2080 はその \u03B3 を固定した条件付き MLE (標準誤差に \u03B3 の不確かさは含まない)");
    reg("MLE did not converge; median rank regression values are reported instead.",
        "最尤推定が収束しなかったため、メジアンランク回帰の値を出しています。");
    reg("m bias-corrected by JIS R 1625 (B_n = %1, n = %2): m = %3 \u2192 %4; \u03C3\u2080 is the uncorrected MLE.",
        "m を JIS R 1625 で不偏化しました (B_n = %1、n = %2): m = %3 \u2192 %4。\u03C3\u2080 は補正前の MLE の値です。");
    reg("JIS R 1625 bias correction not applied: it is defined only for the 2-parameter MLE of complete (uncensored) data.",
        "JIS R 1625 の不偏化は行いませんでした: 打切りのない 2 母数の最尤推定にだけ定められています。");
    reg("JIS R 1625 6.2 c) sets B_n = 1 for n > 120, so m is not reduced (n = %1).",
        "JIS R 1625 の 6.2 c) では n > 120 のとき B_n = 1 と定めているため、m は補正していません (n = %1)。");
    reg("Goodness of fit: ln L = %1 (MLE). R\u00B2 = %2 refers to the median-rank regression line, not to the MLE.",
        "当てはまり: ln L = %1 (MLE)。R\u00B2 = %2 はメジアンランク回帰直線の値で、MLE の指標ではありません。");
    reg("Weibull (%1): method = %2", "ワイブル (%1): 推定法 = %2");
    reg("Unbias m by JIS R 1625 (MLE, complete data, 2-parameter)",
        "m を JIS R 1625 で不偏化 (MLE・打切りなし・2 母数)");
}

void computeMidRanksWithTies(const std::vector<double> &vals, std::vector<double> &ranksOut,
                             double &tieSumCubeMinusLinearOut)
{
    const size_t n = vals.size();
    ranksOut.assign(n, 0.0);
    tieSumCubeMinusLinearOut = 0.0;
    if (n == 0)
        return;

    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](size_t a, size_t b) { return vals[a] < vals[b]; });

    size_t i = 0;
    while (i < n) {
        size_t j = i + 1;
        while (j < n && std::fabs(vals[order[j]] - vals[order[i]]) <= 1e-14 * std::max(1.0, std::fabs(vals[order[i]])))
            ++j;
        const double avgRank = 0.5 * (static_cast<double>(i + 1) + static_cast<double>(j));
        for (size_t k = i; k < j; ++k)
            ranksOut[order[k]] = avgRank;
        const double t = static_cast<double>(j - i);
        if (t > 1.0)
            tieSumCubeMinusLinearOut += (t * t * t - t);
        i = j;
    }
}

void annotateBracketsOnGraph(Graph *annotateGraph, int K, double yMinAll, double yMaxAll,
                             const std::vector<std::tuple<int, int, double>> &sigPairs,
                             const QStringList &groupNames, Table *srcTable,
                             BracketLabelFormat labelFormat)
{
    if (!annotateGraph || K < 2 || sigPairs.empty())
        return;

    Plot *pw = annotateGraph->plotWidget();
    if (!pw)
        return;

    // Record Undo snapshot before modifying graph markers/scales (B2-2)
    MultiLayer *mlParent = annotateGraph->multiLayerParent();
    const QString beforeUndoSnap = mlParent ? mlParent->captureUndoSnapshot() : QString();

    // Preserve existing graph fonts/styles (no applyArialGraphFonts overwrite, B2-2)
    annotateGraph->replot();

    const double ySpan = std::max(1e-6, yMaxAll - yMinAll);
    std::vector<double> groupX(static_cast<size_t>(K));
    for (int k = 0; k < K; ++k)
        groupX[static_cast<size_t>(k)] = static_cast<double>(k + 1);

    // Collect candidate plotted data curves, excluding ErrorBars / Function / Spectrogram / Pie curves (B2-2)
    struct CurveCandidate {
        double meanX = 0.0;
        QString fullName;
        QString shortName;
        bool fromSrcTable = false;
    };
    std::vector<CurveCandidate> dataCurves;
    const int nCurves = annotateGraph->curves();
    dataCurves.reserve(static_cast<size_t>(std::max(0, nCurves)));
    for (int ci = 0; ci < nCurves; ++ci) {
        const int cType = annotateGraph->curveType(ci);
        if (cType == Graph::ErrorBars || cType == Graph::Function || cType == Graph::ColorMap
            || cType == Graph::GrayMap || cType == Graph::ContourMap || cType == Graph::Pie
            || cType == Graph::VectXYXY || cType == Graph::VectXYAM) {
            continue;
        }
        QwtPlotCurve *cv = annotateGraph->curve(ci);
        if (!cv || cv->dataSize() <= 0)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(cv);
        if (!dc)
            continue;
        double sx = 0.0;
        int validPts = 0;
        for (int i = 0; i < cv->dataSize(); ++i) {
            const double xv = cv->x(i);
            if (std::isfinite(xv)) {
                sx += xv;
                ++validPts;
            }
        }
        if (validPts <= 0)
            continue;
        CurveCandidate cand;
        cand.meanX = sx / static_cast<double>(validPts);
        cand.fullName = dc->yColumnName().trimmed();
        cand.shortName = cand.fullName;
        if (Table *dt = dc->table()) {
            const QString prefix = dt->name() + QLatin1Char('_');
            if (cand.fullName.startsWith(prefix))
                cand.shortName = cand.fullName.mid(prefix.size());
            cand.fromSrcTable = (srcTable != nullptr && dt == srcTable);
        }
        dataCurves.push_back(std::move(cand));
    }

    bool resolvedByName = false;
    if (groupNames.size() == K) {
        // 1) Try matching each group name against DataCurve yColumnName / short column name
        std::vector<double> matchedX(static_cast<size_t>(K), 0.0);
        std::vector<bool> matchedFlag(static_cast<size_t>(K), false);
        int nMatched = 0;
        for (int k = 0; k < K; ++k) {
            const QString gn = groupNames[k].trimmed();
            const QString fullSrc =
                    srcTable ? (srcTable->name() + QLatin1Char('_') + gn) : QString();
            int bestIdx = -1;
            int bestScore = -1;
            for (size_t ci = 0; ci < dataCurves.size(); ++ci) {
                const CurveCandidate &cand = dataCurves[ci];
                int score = -1;
                if (!fullSrc.isEmpty() && cand.fullName == fullSrc)
                    score = 100;
                else if (cand.shortName == gn || cand.fullName == gn)
                    score = cand.fromSrcTable ? 90 : 80;
                else if (cand.shortName.compare(gn, Qt::CaseInsensitive) == 0
                         || cand.fullName.compare(gn, Qt::CaseInsensitive) == 0)
                    score = cand.fromSrcTable ? 70 : 60;
                if (score > bestScore) {
                    bestScore = score;
                    bestIdx = static_cast<int>(ci);
                }
            }
            if (bestIdx >= 0) {
                matchedX[static_cast<size_t>(k)] = dataCurves[static_cast<size_t>(bestIdx)].meanX;
                matchedFlag[static_cast<size_t>(k)] = true;
                ++nMatched;
            }
        }
        if (nMatched == K) {
            bool distinct = true;
            for (int a = 0; a < K && distinct; ++a) {
                for (int b = a + 1; b < K; ++b) {
                    if (std::fabs(matchedX[static_cast<size_t>(a)]
                                  - matchedX[static_cast<size_t>(b)])
                        < 1e-6) {
                        distinct = false;
                        break;
                    }
                }
            }
            if (distinct) {
                groupX = matchedX;
                resolvedByName = true;
            }
        }

        // 2) Try matching group names against text-category X-axis tick labels (QwtTextScaleDraw)
        if (!resolvedByName) {
            if (const auto *tsd =
                        dynamic_cast<const QwtTextScaleDraw *>(pw->axisScaleDraw(QwtPlot::xBottom))) {
                const QMap<int, QString> lmap = tsd->labelsMap();
                std::vector<double> axisMatchedX(static_cast<size_t>(K), 0.0);
                int nAxisMatched = 0;
                for (int k = 0; k < K; ++k) {
                    const QString gn = groupNames[k].trimmed();
                    bool found = false;
                    for (auto it = lmap.constBegin(); it != lmap.constEnd(); ++it) {
                        const QString lbl = it.value().trimmed();
                        if (lbl == gn || lbl.compare(gn, Qt::CaseInsensitive) == 0) {
                            axisMatchedX[static_cast<size_t>(k)] = static_cast<double>(it.key());
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        ++nAxisMatched;
                }
                if (nAxisMatched == K) {
                    bool distinct = true;
                    for (int a = 0; a < K && distinct; ++a) {
                        for (int b = a + 1; b < K; ++b) {
                            if (std::fabs(axisMatchedX[static_cast<size_t>(a)]
                                          - axisMatchedX[static_cast<size_t>(b)])
                                < 1e-6) {
                                distinct = false;
                                break;
                            }
                        }
                    }
                    if (distinct) {
                        groupX = axisMatchedX;
                        resolvedByName = true;
                    }
                }
            }
        }
    }

    // 3) Fallback only when curve count matches K (or no names provided): use distinct data curve mean X
    if (!resolvedByName && static_cast<int>(dataCurves.size()) == K) {
        std::vector<double> curveMeans;
        bool distinctMeans = true;
        for (int k = 0; k < K; ++k) {
            const double mx = dataCurves[static_cast<size_t>(k)].meanX;
            for (double prev : curveMeans) {
                if (std::fabs(mx - prev) < 1e-6)
                    distinctMeans = false;
            }
            curveMeans.push_back(mx);
        }
        if (distinctMeans && static_cast<int>(curveMeans.size()) == K)
            groupX = curveMeans;
    }

    // Canonicalize each pair to (minIdx, maxIdx) in X-order and sort narrowest span -> widest span, then left -> right
    std::vector<std::tuple<int, int, double>> orderedPairs;
    orderedPairs.reserve(sigPairs.size());
    for (const auto &tp : sigPairs) {
        int a = std::clamp(std::get<0>(tp), 0, K - 1);
        int b = std::clamp(std::get<1>(tp), 0, K - 1);
        if (a == b)
            continue;
        if (groupX[static_cast<size_t>(a)] > groupX[static_cast<size_t>(b)])
            std::swap(a, b);
        orderedPairs.emplace_back(a, b, std::get<2>(tp));
    }
    if (orderedPairs.empty())
        return;
    std::stable_sort(orderedPairs.begin(), orderedPairs.end(),
                     [&](const std::tuple<int, int, double> &lhs,
                         const std::tuple<int, int, double> &rhs) {
                         const double spanL = std::fabs(groupX[size_t(std::get<1>(lhs))]
                                                        - groupX[size_t(std::get<0>(lhs))]);
                         const double spanR = std::fabs(groupX[size_t(std::get<1>(rhs))]
                                                        - groupX[size_t(std::get<0>(rhs))]);
                         if (std::fabs(spanL - spanR) > 1e-6)
                             return spanL < spanR;
                         return groupX[size_t(std::get<0>(lhs))]
                                 < groupX[size_t(std::get<0>(rhs))];
                     });

    // Pack horizontally non-overlapping brackets into the same vertical tier (B2-2)
    const double xMinG = *std::min_element(groupX.begin(), groupX.end());
    const double xMaxG = *std::max_element(groupX.begin(), groupX.end());
    const double minGroupGap = (K > 1) ? std::max(1e-6, (xMaxG - xMinG) / double(K - 1)) : 1.0;
    const double horizPad = 0.08 * minGroupGap;

    std::vector<std::vector<std::pair<double, double>>> laneIntervals;
    std::vector<int> pairLane(orderedPairs.size(), 0);
    for (size_t b = 0; b < orderedPairs.size(); ++b) {
        const double xl = std::min(groupX[static_cast<size_t>(std::get<0>(orderedPairs[b]))],
                                   groupX[static_cast<size_t>(std::get<1>(orderedPairs[b]))]);
        const double xr = std::max(groupX[static_cast<size_t>(std::get<0>(orderedPairs[b]))],
                                   groupX[static_cast<size_t>(std::get<1>(orderedPairs[b]))]);
        int chosenLane = -1;
        for (size_t ln = 0; ln < laneIntervals.size(); ++ln) {
            bool overlaps = false;
            for (const auto &iv : laneIntervals[ln]) {
                if (!(xr + horizPad < iv.first || xl - horizPad > iv.second)) {
                    overlaps = true;
                    break;
                }
            }
            if (!overlaps) {
                chosenLane = static_cast<int>(ln);
                break;
            }
        }
        if (chosenLane < 0) {
            chosenLane = static_cast<int>(laneIntervals.size());
            laneIntervals.emplace_back();
        }
        laneIntervals[static_cast<size_t>(chosenLane)].emplace_back(xl, xr);
        pairLane[b] = chosenLane;
    }
    const int numLanes = std::max(1, static_cast<int>(laneIntervals.size()));

    // Inherit existing graph font family & pointSizeF without altering graph fonts
    QFont starFont = annotateGraph->defaultTextMarkerFont();
    double basePt = (starFont.pointSizeF() > 0.0)
            ? starFont.pointSizeF()
            : ((starFont.pointSize() > 0) ? double(starFont.pointSize()) : 0.0);
    if (basePt <= 0.0) {
        QFont axF = pw->axisFont(QwtPlot::xBottom);
        basePt = (axF.pointSizeF() > 0.0) ? axF.pointSizeF()
                                          : ((axF.pointSize() > 0) ? double(axF.pointSize()) : 10.0);
        if (starFont.family().isEmpty())
            starFont.setFamily(axF.family());
    }
    if (starFont.family().isEmpty())
        starFont.setFamily(QStringLiteral("Arial"));
    starFont.setPointSizeF(basePt);
    starFont.setWeight(QFont::Normal);
    starFont.setBold(false);
    const QFontMetricsF starFm(starFont);

    const int canvasH = (pw->canvas() && pw->canvas()->height() >= 60) ? pw->canvas()->height() : 220;
    const double minStepPx = starFm.height() + 11.0;
    const double approxStepFrac =
            minStepPx / std::max(40.0, double(canvasH) - minStepPx * (double(numLanes) + 1.3));
    const double stepY = std::max(0.145 * ySpan, std::min(0.46 * ySpan, approxStepFrac * ySpan));
    const double tickH = 0.28 * stepY;
    const double baseTop = yMaxAll + 0.10 * ySpan;
    const double neededYMax = baseTop + (static_cast<double>(numLanes) + 1.05) * stepY;

    const QwtScaleDiv *xDiv = pw->axisScaleDiv(QwtPlot::xBottom);
    const QwtScaleDiv *yDiv = pw->axisScaleDiv(QwtPlot::yLeft);
    const QList<Graph::AxisType> axTypes = annotateGraph->axesType();
    const bool xIsTextCat = (QwtPlot::xBottom < axTypes.size()
                             && axTypes[QwtPlot::xBottom] == Graph::AxisType::Txt);

    const double curXLo = xDiv ? std::min(xDiv->lowerBound(), xDiv->upperBound()) : 0.5;
    const double curXHi = xDiv ? std::max(xDiv->lowerBound(), xDiv->upperBound())
                               : (static_cast<double>(K) + 0.5);
    const double xLo = xIsTextCat ? std::min(curXLo, std::min(0.5, xMinG - 0.5))
                                  : std::min(curXLo, xMinG - 0.55);
    const double xHi = xIsTextCat
            ? std::max(curXHi, std::max(static_cast<double>(K) + 0.5, xMaxG + 0.5))
            : std::max(curXHi, xMaxG + 0.55);

    const double curYLo = yDiv ? std::min(yDiv->lowerBound(), yDiv->upperBound())
                               : (yMinAll - 0.05 * ySpan);
    const double curYHi = yDiv ? std::max(yDiv->lowerBound(), yDiv->upperBound()) : neededYMax;
    const double yLo = (curYLo == 0.0 && yMinAll >= 0.0)
            ? 0.0
            : std::min(curYLo, yMinAll - 0.05 * ySpan);
    double yHi = std::max(curYHi, neededYMax);

    if (xIsTextCat) {
        annotateGraph->setScale(QwtPlot::xBottom, xLo, xHi, 1.0,
                                std::max(K + 2, static_cast<int>(std::ceil(xHi - xLo)) + 1), 0);
        pw->setMinorTicksType(QwtPlot::xBottom, ScaleDraw::None);
    } else {
        annotateGraph->setScale(QwtPlot::xBottom, xLo, xHi);
    }
    annotateGraph->setScale(QwtPlot::yLeft, yLo, yHi);
    annotateGraph->setAxisUserLocked(QwtPlot::yLeft, true);
    annotateGraph->replot();

    // Ensure topmost bracket label clears top spine (and inward ticks) by at least 12 px
    {
        const double topBarY = baseTop + static_cast<double>(numLanes - 1) * stepY;
        const double topBarPx = pw->transform(QwtPlot::yLeft, topBarY);
        const double estOriginTopPx = topBarPx - 4.0 - starFm.ascent() - 4.0;
        if (estOriginTopPx < 12.0 && canvasH > 50) {
            const double extraPx = 14.0 - estOriginTopPx;
            const double pxToData = (yHi - yLo) / double(std::max(40, canvasH));
            yHi += extraPx * pxToData * 1.30;
            annotateGraph->setScale(QwtPlot::yLeft, yLo, yHi);
            annotateGraph->setAxisUserLocked(QwtPlot::yLeft, true);
            annotateGraph->replot();
        }
    }

    auto formatBracketText = [](double pAdj, BracketLabelFormat fmt) -> QString {
        const QString stars = starRatingForP(pAdj);
        if (fmt == BracketLabelFormat::StarsOnly)
            return stars;
        const QString pStr = (pAdj < 0.0001)
                ? QStringLiteral("p < 0.0001")
                : QStringLiteral("p = %1").arg(pAdj, 0, 'f', (pAdj < 0.01) ? 4 : 3);
        if (fmt == BracketLabelFormat::PValueOnly)
            return pStr;
        return QStringLiteral("%1 (%2)").arg(stars, pStr);
    };

    struct BracketLabelBinding {
        int markerId = -1;
        double xMid = 0.0;
        double yBar = 0.0;
        QString labelText;
    };
    QVector<BracketLabelBinding> bindings;
    bindings.reserve(static_cast<int>(orderedPairs.size()));

    const QColor bracketColor(30, 30, 30);
    for (size_t b = 0; b < orderedPairs.size(); ++b) {
        const int gi = std::get<0>(orderedPairs[b]);
        const int gj = std::get<1>(orderedPairs[b]);
        const double pAdj = std::get<2>(orderedPairs[b]);
        const double xLeft =
                std::min(groupX[static_cast<size_t>(gi)], groupX[static_cast<size_t>(gj)]);
        const double xRight =
                std::max(groupX[static_cast<size_t>(gi)], groupX[static_cast<size_t>(gj)]);
        const double yBar = baseTop + static_cast<double>(pairLane[b]) * stepY;

        addLineSegmentOnGraph(annotateGraph, xLeft, yBar - tickH, xLeft, yBar, bracketColor, 1);
        addLineSegmentOnGraph(annotateGraph, xLeft, yBar, xRight, yBar, bracketColor, 1);
        addLineSegmentOnGraph(annotateGraph, xRight, yBar, xRight, yBar - tickH, bracketColor, 1);

        const QString lblTxt = formatBracketText(pAdj, labelFormat);
        Legend *lbl = addTextLabelOnGraph(annotateGraph, 0.5 * (xLeft + xRight), yBar, lblTxt,
                                          bracketColor, false, 0);
        if (lbl) {
            int foundId = -1;
            for (int tid : annotateGraph->textMarkerKeys()) {
                if (annotateGraph->textMarker(tid) == lbl) {
                    foundId = tid;
                    break;
                }
            }
            if (foundId >= 0)
                bindings.append({ foundId, 0.5 * (xLeft + xRight), yBar, lblTxt });
        }
    }

    auto syncBracketLabels = [gPtr = QPointer<Graph>(annotateGraph), bindings]() {
        if (!gPtr || !gPtr->plotWidget())
            return;
        Plot *plotW = gPtr->plotWidget();
        const QwtScaleDiv *yd = plotW->axisScaleDiv(QwtPlot::yLeft);
        if (yd && !bindings.isEmpty() && plotW->canvas() && plotW->canvas()->height() > 50) {
            int topIdx = 0;
            for (int bi = 1; bi < bindings.size(); ++bi) {
                if (bindings[bi].yBar > bindings[topIdx].yBar)
                    topIdx = bi;
            }
            const BracketLabelBinding &topB = bindings[topIdx];
            if (Legend *topLbl = gPtr->textMarker(topB.markerId)) {
                QFont tf = topLbl->font();
                tf.setBold(false);
                const QFontMetricsF tfm(tf);
                const double topBarPx = plotW->transform(QwtPlot::yLeft, topB.yBar);
                const double lift = (topB.labelText == QLatin1String("ns")) ? 4.0 : 2.0;
                const double neededTopPx = topBarPx - 4.0 - tfm.ascent() - lift;
                if (neededTopPx < 10.0) {
                    const double curLo = std::min(yd->lowerBound(), yd->upperBound());
                    const double curHi = std::max(yd->lowerBound(), yd->upperBound());
                    const double pxToData =
                            (curHi - curLo) / double(std::max(40, plotW->canvas()->height()));
                    gPtr->setScale(QwtPlot::yLeft, curLo,
                                   curHi + (12.0 - neededTopPx) * pxToData * 1.25);
                    gPtr->setAxisUserLocked(QwtPlot::yLeft, true);
                    gPtr->replot();
                }
            }
        }
        for (const BracketLabelBinding &bb : bindings) {
            Legend *lbl = gPtr->textMarker(bb.markerId);
            if (!lbl)
                continue;
            QFont lf = lbl->font();
            if (lf.bold()) {
                lf.setBold(false);
                lbl->setFont(lf);
            }
            const QFontMetricsF fm(lf);
            const double textW = fm.horizontalAdvance(bb.labelText);
            const double xMidPx = plotW->transform(QwtPlot::xBottom, bb.xMid);
            const double yBarPx = plotW->transform(QwtPlot::yLeft, bb.yBar);
            const double originPxX = xMidPx - 6.0 - 0.5 * textW;
            const double liftPx = (bb.labelText == QLatin1String("ns")) ? 4.0 : 2.0;
            const double originPxY = std::max(3.0, yBarPx - 4.0 - fm.ascent() - liftPx);
            const double dataX = plotW->invTransform(QwtPlot::xBottom, qRound(originPxX));
            const double dataY = plotW->invTransform(QwtPlot::yLeft, qRound(originPxY));
            if (std::isfinite(dataX) && std::isfinite(dataY))
                lbl->setOriginCoord(dataX, dataY);
        }
    };

    syncBracketLabels();

    if (!bindings.isEmpty()) {
        auto guard = std::make_shared<bool>(false);
        QObject::connect(annotateGraph, &Graph::modifiedGraph, annotateGraph,
                         [syncBracketLabels, guard]() {
                             if (*guard)
                                 return;
                             *guard = true;
                             syncBracketLabels();
                             *guard = false;
                         });
    }

    if (annotateGraph->isLegendVisible() && annotateGraph->legend())
        annotateGraph->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
    annotateGraph->replot();

    if (mlParent && !beforeUndoSnap.isEmpty())
        mlParent->pushUndoSnapshot(QObject::tr("Add Significance Brackets"), beforeUndoSnap);
}

double dixonQ10CritTable(int n, int alphaIdx /*0:0.10, 1:0.05, 2:0.01*/)
{
    // Exact Rorabacher (1991) two-tailed / standard Dixon Q10 critical values for n = 3..30
    static const double q90[28] = {
        0.941, 0.765, 0.642, 0.560, 0.507, 0.468, 0.437, 0.412, 0.392, 0.376,
        0.361, 0.349, 0.338, 0.329, 0.320, 0.313, 0.306, 0.300, 0.295, 0.290,
        0.285, 0.281, 0.277, 0.273, 0.269, 0.266, 0.263, 0.260
    };
    static const double q95[28] = {
        0.970, 0.829, 0.710, 0.625, 0.568, 0.526, 0.493, 0.466, 0.444, 0.426,
        0.410, 0.396, 0.384, 0.374, 0.365, 0.356, 0.349, 0.342, 0.337, 0.331,
        0.326, 0.321, 0.317, 0.312, 0.308, 0.305, 0.301, 0.298
    };
    static const double q99[28] = {
        0.994, 0.926, 0.821, 0.740, 0.680, 0.634, 0.598, 0.568, 0.542, 0.522,
        0.503, 0.488, 0.475, 0.463, 0.452, 0.442, 0.433, 0.425, 0.418, 0.411,
        0.404, 0.399, 0.393, 0.388, 0.384, 0.380, 0.376, 0.372
    };
    const double *arr = (alphaIdx == 0) ? q90 : ((alphaIdx == 1) ? q95 : q99);
    if (n <= 3)
        return arr[0];
    if (n <= 30)
        return arr[n - 3];
    return arr[27] * std::sqrt(std::log(30.0) / std::log(static_cast<double>(n)));
}

void evalDixonQCritAndP(int n, double qObs, double alpha, double &qCritOut, double &pValOut)
{
    const double c10 = dixonQ10CritTable(n, 0);
    const double c05 = dixonQ10CritTable(n, 1);
    const double c01 = dixonQ10CritTable(n, 2);

    const double la = std::log(std::clamp(alpha, 1e-4, 0.50));
    const double l10 = std::log(0.10);
    const double l05 = std::log(0.05);
    const double l01 = std::log(0.01);

    if (alpha >= 0.05) {
        const double t = (la - l10) / (l05 - l10);
        qCritOut = std::clamp(c10 + t * (c05 - c10), 0.05, 0.9999);
    } else {
        const double t = (la - l05) / (l01 - l05);
        qCritOut = std::clamp(c05 + t * (c01 - c05), 0.05, 0.9999);
    }

    if (qObs <= 0.0) {
        pValOut = 1.0;
    } else if (qObs < c10) {
        pValOut = std::clamp(1.0 - 0.90 * (qObs / std::max(1e-9, c10)), 0.10, 1.0);
    } else if (qObs <= c05) {
        const double u = (qObs - c10) / std::max(1e-9, c05 - c10);
        pValOut = std::exp(l10 + u * (l05 - l10));
    } else if (qObs <= c01) {
        const double u = (qObs - c05) / std::max(1e-9, c01 - c05);
        pValOut = std::exp(l05 + u * (l01 - l05));
    } else {
        const double slope = (l01 - l05) / std::max(1e-6, c01 - c05);
        pValOut = std::clamp(std::exp(std::max(-50.0, l01 + slope * (qObs - c01))), 0.0, 0.01);
    }
}

void evalWilcoxonSignedRank(const std::vector<double> &diffs, double &wStatOut, double &pValOut)
{
    std::vector<double> absD;
    std::vector<int> signs;
    absD.reserve(diffs.size());
    signs.reserve(diffs.size());
    for (double d : diffs) {
        if (std::isfinite(d) && std::fabs(d) > 1e-15) {
            absD.push_back(std::fabs(d));
            signs.push_back(d > 0.0 ? 1 : -1);
        }
    }
    const size_t m = absD.size();
    if (m == 0) {
        wStatOut = 0.0;
        pValOut = 1.0;
        return;
    }
    std::vector<double> ranks;
    double tieSum = 0.0;
    computeMidRanksWithTies(absD, ranks, tieSum);

    double wPlus = 0.0;
    double wMinus = 0.0;
    for (size_t i = 0; i < m; ++i) {
        if (signs[i] > 0)
            wPlus += ranks[i];
        else
            wMinus += ranks[i];
    }
    wStatOut = std::min(wPlus, wMinus);
    const double dm = static_cast<double>(m);
    const double mu = 0.25 * dm * (dm + 1.0);
    const double varW = std::max(0.0, (dm * (dm + 1.0) * (2.0 * dm + 1.0) - 0.5 * tieSum) / 24.0);
    if (varW <= 1e-30) {
        pValOut = 1.0;
        return;
    }
    const double z = std::max(0.0, std::fabs(wPlus - mu) - 0.5) / std::sqrt(varW);
    pValOut = std::clamp(2.0 * gsl_cdf_ugaussian_Q(z), 0.0, 1.0);
}

class WeibullProbScaleDraw : public ScaleDraw
{
public:
    explicit WeibullProbScaleDraw(const ScaleDraw &other) : ScaleDraw(other) {}
    WeibullProbScaleDraw() : ScaleDraw() {}

    QwtText label(double weibitVal) const override
    {
        if (!std::isfinite(weibitVal) || weibitVal < -25.0 || weibitVal > 6.0)
            return QwtText();
        const double pfPct = 100.0 * (1.0 - std::exp(-std::exp(weibitVal)));
        if (!(pfPct > 0.0 && pfPct < 100.0))
            return QwtText();
        // Snap standard probability paper levels (1, 5, 10, 20, 50, 63.2, 90, 99%)
        static const double kStdPct[] = { 0.1, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 30.0,
                                          50.0, 63.21205588, 80.0, 90.0, 95.0, 99.0, 99.9 };
        static const char *kStdLbl[] = { "0.1", "0.5", "1",  "2",    "5",  "10", "20", "30",
                                         "50",  "63.2", "80", "90",  "95", "99", "99.9" };
        for (size_t k = 0; k < sizeof(kStdPct) / sizeof(kStdPct[0]); ++k) {
            const double wTarget = std::log(-std::log(1.0 - 0.01 * kStdPct[k]));
            if (std::fabs(weibitVal - wTarget) <= 1e-3)
                return QwtText(QString::fromLatin1(kStdLbl[k]));
        }
        return QwtText(QString::number(pfPct, 'g', 3));
    }
};

void syncWeibullProbRightAxis(Graph *parentG, Graph *linkedG)
{
    if (!parentG || !linkedG || !parentG->plotWidget() || !linkedG->plotWidget())
        return;
    Plot *pPlot = parentG->plotWidget();
    Plot *cPlot = linkedG->plotWidget();
    const QwtScaleDiv *yDiv = pPlot->axisScaleDiv(QwtPlot::yLeft);
    if (!yDiv)
        return;
    const double wLo = std::min(yDiv->lowerBound(), yDiv->upperBound());
    const double wHi = std::max(yDiv->lowerBound(), yDiv->upperBound());
    if (!(wHi > wLo) || !std::isfinite(wLo) || !std::isfinite(wHi))
        return;

    // Keep 50% on minor ticks so '50' (-0.3665 weibits) never collides vertically with '63.2' (0.0 weibits)
    static const double kMajorProbsPct[] = { 1.0, 5.0, 10.0, 20.0, 63.21205588, 90.0, 99.0 };
    static const double kMinorProbsPct[] = { 0.5, 2.0, 3.0, 15.0, 30.0, 50.0, 80.0, 95.0 };

    QwtValueList majTicks, minTicks, medTicks;
    for (double pPct : kMajorProbsPct) {
        const double w = std::log(-std::log(1.0 - 0.01 * pPct));
        if (w >= wLo - 1e-6 && w <= wHi + 1e-6)
            majTicks.append(w);
    }
    for (double pPct : kMinorProbsPct) {
        const double w = std::log(-std::log(1.0 - 0.01 * pPct));
        if (w >= wLo - 1e-6 && w <= wHi + 1e-6)
            minTicks.append(w);
    }
    QwtValueList ticks[QwtScaleDiv::NTickTypes];
    ticks[QwtScaleDiv::MinorTick] = minTicks;
    ticks[QwtScaleDiv::MajorTick] = majTicks;
    QwtScaleDiv rightDiv(wLo, wHi, ticks);
    cPlot->setAxisScaleDiv(QwtPlot::yRight, rightDiv);
    pPlot->setMajorTicksType(QwtPlot::yRight, ScaleDraw::None);
    pPlot->setMinorTicksType(QwtPlot::yRight, ScaleDraw::None);
    if (QwtScaleDraw *pSdR = pPlot->axisScaleDraw(QwtPlot::yRight))
        pSdR->enableComponent(QwtAbstractScaleDraw::Labels, false);
}

} // namespace

double GraphStatsAndSurfaceFit::jisR1625UnbiasingFactor(int n)
{
    // JIS R 1625:2010, 6.2 c) formula (3) and Annex A: m = B_n * m_t with
    //   B_n = (1 + 2.04 n^-1.1)^-1  for n <= 120,
    //   B_n = 1                     for n > 120,
    // as stated in the text of 6.2 c) (checked against the kikakurui.com transcription, where
    // the second condition is printed as "n < 120", an evident misprint for n > 120). The Annex A
    // worked example gives B_30 = 0.9538. The standard itself switches to 1 above 120, so m
    // steps by about 1 % between n = 120 (B = 0.9896) and n = 121; this is kept on purpose to
    // reproduce JIS-conforming values.
    if (n < 2)
        return 1.0;
    if (n > 120)
        return 1.0;
    return 1.0 / (1.0 + 2.04 * std::pow(static_cast<double>(n), -1.1));
}

GraphStatsAndSurfaceFit::WeibullPlotResult GraphStatsAndSurfaceFit::plotWeibullProbability(
        ApplicationWindow *app, Table *srcTable, const QString &dataCol, const QString &censorCol,
        int censorCoding, bool fitThreeParam, bool useMle, bool createPlot, bool jisUnbiasMle)
{
    registerPhase5StatsTranslationsOnce();
    WeibullPlotResult res;
    if (!srcTable) {
        res.error = QObject::tr("Weibull analysis: the source table or the data column was not found.");
        return res;
    }

    Column *cData = resolveTableColumn(srcTable, dataCol);
    if (!cData) {
        res.error = QObject::tr("Weibull analysis: the source table or the data column was not found.");
        return res;
    }
    Column *cCensor = censorCol.trimmed().isEmpty() ? nullptr
                                                    : resolveTableColumn(srcTable, censorCol);

    struct WeibItem {
        double x = 0.0;
        bool censored = false;
    };
    std::vector<WeibItem> items;
    const int nRows = cData->rowCount();
    items.reserve(static_cast<size_t>(std::max(0, nRows)));

    int nConsidered = 0, nNonNumeric = 0, nNonPositive = 0;
    for (int r = 0; r < nRows; ++r) {
        if (cData->isInvalid(r) || cData->isMasked(r))
            continue; // empty or deliberately masked row
        ++nConsidered;
        double xv = 0.0;
        if (!readNumericColumnCell(cData, r, xv)) {
            ++nNonNumeric;
            continue;
        }
        if (xv <= 0.0) {
            ++nNonPositive;
            continue;
        }
        bool isCens = false;
        if (cCensor && r < cCensor->rowCount() && !cCensor->isInvalid(r) && !cCensor->isMasked(r)) {
            double cv = 0.0;
            if (readNumericColumnCell(cCensor, r, cv)) {
                if (censorCoding == 0) {
                    // Kaplan-Meier standard: 1 = Failure/Event, 0 = Right-Censored
                    isCens = (std::round(cv) == 0.0);
                } else {
                    // Suspension coding: 0 = Failure, 1 = Right-Censored
                    isCens = (std::round(cv) != 0.0);
                }
            } else if (cCensor->asStringColumn()) {
                const QString s = cCensor->asStringColumn()->textAt(r).trimmed().toLower();
                if (s == QLatin1String("c") || s.startsWith(QLatin1String("cens"))
                    || s.startsWith(QLatin1String("susp")) || s == QLatin1String("right")) {
                    isCens = true;
                } else if (s == QLatin1String("f") || s.startsWith(QLatin1String("fail"))
                           || s.startsWith(QLatin1String("event"))
                           || s == QLatin1String("dead")) {
                    isCens = false;
                } else if (censorCoding == 0) {
                    isCens = (s == QLatin1String("0") || s == QLatin1String("false")
                              || s == QLatin1String("no"));
                } else {
                    isCens = (s == QLatin1String("1") || s == QLatin1String("true")
                              || s == QLatin1String("yes"));
                }
            }
        }
        items.push_back({ xv, isCens });
    }

    int nCens = 0;
    for (const WeibItem &it : items) {
        if (it.censored)
            ++nCens;
    }
    res.censoredCount = nCens;

    res.excludedCount = nNonNumeric + nNonPositive;
    if (res.excludedCount > 0) {
        QStringList why;
        if (nNonNumeric > 0)
            why << QObject::tr("%1 non-numeric").arg(nNonNumeric);
        if (nNonPositive > 0)
            why << QObject::tr("%1 with value <= 0 (ln undefined)").arg(nNonPositive);
        res.notes << QObject::tr("Weibull: %1 of %2 row(s) excluded (%3).")
                             .arg(res.excludedCount)
                             .arg(nConsidered)
                             .arg(why.join(QStringLiteral(", ")));
    }
    auto flushWeibullLog = [&]() {
        if (!app)
            return;
        QString txt = QStringLiteral("[%1] ").arg(QLocale().toString(QDateTime::currentDateTime()));
        txt += QObject::tr("Weibull (%1): method = %2")
                       .arg(srcTable->name() + QLatin1Char('_') + cData->name())
                       .arg(res.methodDescription.isEmpty() ? QStringLiteral("-")
                                                            : res.methodDescription)
                + QLatin1Char('\n');
        for (const QString &nt : res.notes)
            txt += QStringLiteral("  ") + nt + QLatin1Char('\n');
        if (!res.error.isEmpty())
            txt += QStringLiteral("  ") + res.error + QLatin1Char('\n');
        app->updateLog(txt);
    };
    auto failWith = [&](const QString &why) {
        res.valid = false;
        res.error = why;
        flushWeibullLog();
        return res;
    };

    if (items.size() < 2) {
        return failWith(QObject::tr(
                "Weibull analysis needs at least 2 failures (uncensored values > 0); only %1 remained.")
                                .arg(static_cast<int>(items.size()) - nCens));
    }

    std::sort(items.begin(), items.end(), [](const WeibItem &a, const WeibItem &b) {
        if (a.x != b.x)
            return a.x < b.x;
        return !a.censored && b.censored;
    });

    // Herd-Johnson / Benard median rank order numbers
    const int nTotal = static_cast<int>(items.size());
    std::vector<double> failX;
    std::vector<double> failProb;
    std::vector<double> uLnX;
    std::vector<double> wWeibit;
    failX.reserve(items.size());
    failProb.reserve(items.size());
    uLnX.reserve(items.size());
    wWeibit.reserve(items.size());

    double prevOrder = 0.0;
    for (int idx = 0; idx < nTotal; ++idx) {
        if (items[static_cast<size_t>(idx)].censored)
            continue;
        const double remainingPlusOne = static_cast<double>(1 + (nTotal - idx));
        const double orderNum = prevOrder
                + (static_cast<double>(nTotal + 1) - prevOrder) / remainingPlusOne;
        prevOrder = orderNum;

        const double F = std::clamp((orderNum - 0.3) / (static_cast<double>(nTotal) + 0.4),
                                    1e-9, 1.0 - 1e-9);
        const double xv = items[static_cast<size_t>(idx)].x;
        failX.push_back(xv);
        failProb.push_back(F);
        uLnX.push_back(std::log(xv));
        wWeibit.push_back(std::log(-std::log(1.0 - F)));
    }

    const int mFail = static_cast<int>(failX.size());
    if (mFail < 2) {
        return failWith(QObject::tr(
                "Weibull analysis needs at least 2 failures (uncensored values > 0); only %1 remained.")
                                .arg(mFail));
    }

    const double meanW =
            std::accumulate(wWeibit.begin(), wWeibit.end(), 0.0) / static_cast<double>(mFail);
    double sww = 0.0;
    for (int i = 0; i < mFail; ++i) {
        const double dw = wWeibit[static_cast<size_t>(i)] - meanW;
        sww += dw * dw;
    }

    auto evalGammaStats = [&](double gamma, double &betaOut, double &interceptOut, double &meanUOut,
                              double &suuOut, double &rssOut) -> bool {
        double sumU = 0.0;
        for (int i = 0; i < mFail; ++i) {
            const double shifted = failX[static_cast<size_t>(i)] - gamma;
            if (!(shifted > 0.0))
                return false;
            sumU += std::log(shifted);
        }
        meanUOut = sumU / static_cast<double>(mFail);
        suuOut = 0.0;
        double suw = 0.0;
        for (int i = 0; i < mFail; ++i) {
            const double du = std::log(failX[static_cast<size_t>(i)] - gamma) - meanUOut;
            const double dw = wWeibit[static_cast<size_t>(i)] - meanW;
            suuOut += du * du;
            suw += du * dw;
        }
        if (suuOut <= 1e-30 || suw <= 0.0)
            return false;
        betaOut = suw / suuOut;
        interceptOut = meanW - betaOut * meanUOut;
        rssOut = 0.0;
        for (int i = 0; i < mFail; ++i) {
            const double ui = std::log(failX[static_cast<size_t>(i)] - gamma);
            const double err = wWeibit[static_cast<size_t>(i)] - (interceptOut + betaOut * ui);
            rssOut += err * err;
        }
        return std::isfinite(rssOut);
    };

    double bestGamma = 0.0;
    bool usedThreeParam = false;
    if (fitThreeParam && mFail >= 3) {
        // Bound threshold gamma below all observations (including early right-censored points)
        // so shifted lifetimes x_i - gamma remain strictly positive for right-censored MLE (E4-9).
        double minAllX = failX.front();
        for (const WeibItem &it : items)
            minAllX = std::min(minAllX, it.x);
        const double gammaMax = 0.999 * minAllX;
        if (gammaMax > 0.0) {
            auto objectiveRss = [&](double gCand) -> double {
                double bTmp = 0.0, aTmp = 0.0, muTmp = 0.0, suuTmp = 0.0, rssTmp = 0.0;
                if (!evalGammaStats(gCand, bTmp, aTmp, muTmp, suuTmp, rssTmp))
                    return std::numeric_limits<double>::infinity();
                return rssTmp;
            };

            constexpr int kScan = 128;
            int bestIdx = 0;
            double bestObj = objectiveRss(0.0);
            for (int s = 1; s <= kScan; ++s) {
                const double gCand =
                        gammaMax * static_cast<double>(s) / static_cast<double>(kScan);
                const double fVal = objectiveRss(gCand);
                if (fVal < bestObj) {
                    bestObj = fVal;
                    bestIdx = s;
                }
            }

            const double stepG = gammaMax / static_cast<double>(kScan);
            double aBrk = std::max(0.0, static_cast<double>(bestIdx - 1) * stepG);
            double bBrk = std::min(gammaMax, static_cast<double>(bestIdx + 1) * stepG);

            // Brent's parabolic + golden-section minimization on [aBrk, bBrk]
            constexpr double kGold = 0.3819660112501051518;
            double x = std::clamp(static_cast<double>(bestIdx) * stepG, aBrk, bBrk);
            double w = x, v = x;
            double fx = objectiveRss(x), fw = fx, fv = fx;
            double dStep = 0.0, eStep = 0.0;
            const double tol = std::max(1e-12, 1e-11 * gammaMax);

            for (int iter = 0; iter < 64; ++iter) {
                const double xm = 0.5 * (aBrk + bBrk);
                const double tol1 = tol * std::fabs(x) + 1e-15;
                const double tol2 = 2.0 * tol1;
                if (std::fabs(x - xm) <= (tol2 - 0.5 * (bBrk - aBrk)))
                    break;

                bool usedParabolic = false;
                if (std::fabs(eStep) > tol1) {
                    const double r = (x - w) * (fx - fv);
                    double q = (x - v) * (fx - fw);
                    double p = (x - v) * q - (x - w) * r;
                    q = 2.0 * (q - r);
                    if (q > 0.0)
                        p = -p;
                    q = std::fabs(q);
                    const double eTemp = eStep;
                    eStep = dStep;
                    if (std::fabs(p) < std::fabs(0.5 * q * eTemp) && p > q * (aBrk - x)
                        && p < q * (bBrk - x)) {
                        dStep = p / q;
                        const double uTry = x + dStep;
                        if ((uTry - aBrk) < tol2 || (bBrk - uTry) < tol2)
                            dStep = (x < xm) ? tol1 : -tol1;
                        usedParabolic = true;
                    }
                }
                if (!usedParabolic) {
                    eStep = (x >= xm) ? (aBrk - x) : (bBrk - x);
                    dStep = kGold * eStep;
                }
                const double u = (std::fabs(dStep) >= tol1)
                        ? (x + dStep)
                        : (x + ((dStep > 0.0) ? tol1 : -tol1));
                const double fu = objectiveRss(u);
                if (fu <= fx) {
                    if (u >= x)
                        aBrk = x;
                    else
                        bBrk = x;
                    v = w;
                    fv = fw;
                    w = x;
                    fw = fx;
                    x = u;
                    fx = fu;
                } else {
                    if (u < x)
                        aBrk = u;
                    else
                        bBrk = u;
                    if (fu <= fw || w == x) {
                        v = w;
                        fv = fw;
                        w = u;
                        fw = fu;
                    } else if (fu <= fv || v == x || v == w) {
                        v = u;
                        fv = fu;
                    }
                }
            }
            if (std::isfinite(fx)) {
                bestGamma = std::clamp(x, 0.0, gammaMax);
                usedThreeParam = true;
            }
        }
    }

    double betaM = 0.0, interceptA = 0.0, meanU = 0.0, suu = 0.0, rss = 0.0;
    if (!evalGammaStats(bestGamma, betaM, interceptA, meanU, suu, rss)) {
        return failWith(QObject::tr(
                "Weibull analysis failed: all failure values are identical, so the slope cannot be determined."));
    }
    for (int i = 0; i < mFail; ++i)
        uLnX[static_cast<size_t>(i)] = std::log(failX[static_cast<size_t>(i)] - bestGamma);

    const double r2 = (sww > 1e-30) ? std::clamp(1.0 - rss / sww, 0.0, 1.0) : 1.0;
    const int residDf = usedThreeParam ? std::max(1, mFail - 3) : (mFail - 2);
    const bool hasSe = usedThreeParam ? (mFail > 3) : (mFail > 2);
    const double sResid = hasSe
            ? std::sqrt(std::max(0.0, rss / static_cast<double>(residDf)))
            : 0.0;
    // No residual degrees of freedom (2 points for the 2-parameter line, 3 for the 3-parameter
    // one): the standard errors are undefined, not 0 (they were reported as 0, i.e. exact).
    // NaN goes out as null, as the Arrhenius Ea SE of 2 points does.
    constexpr double kNoSe = std::numeric_limits<double>::quiet_NaN();
    double betaSE = hasSe ? (sResid / std::sqrt(suu)) : kNoSe;

    const double lnSigma0 = -interceptA / betaM;
    const double sigma0 = std::exp(std::clamp(lnSigma0, -500.0, 500.0));
    const double lnSigma0SE = (hasSe && std::fabs(betaM) > 1e-30)
            ? ((sResid / std::fabs(betaM))
               * std::sqrt(1.0 / static_cast<double>(mFail)
                           + ((lnSigma0 - meanU) * (lnSigma0 - meanU)) / suu))
            : kNoSe;

    // Right-censored Newton-Raphson Maximum Likelihood Estimator (JIS R 1625 / ISO 20501)
    double mleM = betaM;
    double mleMSE = betaSE;
    double mleSigma0 = sigma0;
    double mleSigma0SE = sigma0 * lnSigma0SE;
    double mleVarInvM = 0.0, mleVarLnS0 = 0.0, mleCovLnS0InvM = 0.0;
    bool mleConverged = false;
    {
        std::vector<double> allLnShifted;
        allLnShifted.reserve(items.size());
        double sumFailLn = 0.0;
        double maxLnShifted = -std::numeric_limits<double>::infinity();
        bool allShiftedPositive = true;
        for (const WeibItem &it : items) {
            const double sh = it.x - bestGamma;
            if (!(sh > 0.0)) {
                allShiftedPositive = false;
                break;
            }
            const double lx = std::log(sh);
            allLnShifted.push_back(lx);
            maxLnShifted = std::max(maxLnShifted, lx);
            if (!it.censored)
                sumFailLn += lx;
        }
        if (allShiftedPositive && !allLnShifted.empty() && mFail >= 2) {
            const double meanFailLn = sumFailLn / static_cast<double>(mFail);
            double mCur = std::clamp(betaM, 0.05, 250.0);
            for (int iter = 0; iter < 48; ++iter) {
                double sumW = 0.0, sumWL = 0.0, sumWLL = 0.0;
                for (double lx : allLnShifted) {
                    const double w = std::exp(std::clamp(mCur * (lx - maxLnShifted), -700.0, 0.0));
                    sumW += w;
                    sumWL += w * lx;
                    sumWLL += w * lx * lx;
                }
                if (sumW <= 1e-300)
                    break;
                const double ratioL = sumWL / sumW;
                const double gVal = ratioL - (1.0 / mCur) - meanFailLn;
                const double varL = std::max(0.0, (sumWLL / sumW) - ratioL * ratioL);
                const double gDeriv = varL + 1.0 / (mCur * mCur);
                if (gDeriv <= 1e-30)
                    break;
                double step = gVal / gDeriv;
                // Guard step so mCur stays strictly positive
                while (mCur - step <= 0.1 * mCur)
                    step *= 0.5;
                const double mNext = std::clamp(mCur - step, 1e-3, 1000.0);
                mCur = mNext;
                if (std::fabs(step) <= 1e-11 * mCur) {
                    mleConverged = true;
                    break;
                }
                if (iter >= 8 && std::fabs(gVal) <= 1e-9) {
                    mleConverged = true;
                    break;
                }
            }
            double sumW = 0.0;
            for (double lx : allLnShifted)
                sumW += std::exp(std::clamp(mCur * (lx - maxLnShifted), -700.0, 0.0));
            if (mleConverged && sumW > 1e-300 && std::isfinite(mCur) && mCur > 0.0) {
                const double lnS0Mle =
                        maxLnShifted + std::log(sumW / static_cast<double>(mFail)) / mCur;
                mleM = mCur;
                mleSigma0 = std::exp(std::clamp(lnS0Mle, -500.0, 500.0));

                // Observed Fisher information matrix in (m, u0 = ln(sigma0)) -> exact asymptotic SEs
                double sumZ2ExpZ = 0.0, sumZExpZ = 0.0;
                for (double lx : allLnShifted) {
                    const double z = mCur * (lx - lnS0Mle);
                    const double ez = std::exp(std::clamp(z, -700.0, 500.0));
                    sumZExpZ += z * ez;
                    sumZ2ExpZ += z * z * ez;
                }
                const double rD = static_cast<double>(mFail);
                const double iMM = (rD + sumZ2ExpZ) / (mCur * mCur);
                const double iMU0 = -sumZExpZ; // d^2(-l)/(dm d(ln s0))
                const double iU0U0 = rD * mCur * mCur;
                const double detI = iMM * iU0U0 - iMU0 * iMU0;
                if (detI > 1e-30 && std::isfinite(detI)) {
                    const double varM = std::max(0.0, iU0U0 / detI);
                    const double varU0 = std::max(0.0, iMM / detI);
                    const double covMU0 = -iMU0 / detI;
                    mleMSE = std::sqrt(varM);
                    mleSigma0SE = mleSigma0 * std::sqrt(varU0);
                    mleVarLnS0 = varU0;
                    mleVarInvM = varM / std::pow(mCur, 4);
                    mleCovLnS0InvM = -covMU0 / (mCur * mCur);
                }
            }
        }
    }

    res.mleModulusM = mleM;
    res.mleModulusMSE = mleMSE;
    res.mleScaleSigma0 = mleSigma0;
    res.mleScaleSigma0SE = mleSigma0SE;
    res.usedMle = (useMle && mleConverged);

    // Log-likelihood of the (right-censored) Weibull model at the ML estimate. With the MLE this,
    // not the R^2 of the median-rank regression line, describes the fit.
    if (mleConverged && mleM > 0.0 && mleSigma0 > 0.0) {
        double lnL = 0.0;
        const double lnS0 = std::log(mleSigma0);
        bool ok = true;
        for (const WeibItem &it : items) {
            const double z = it.x - bestGamma;
            if (!(z > 0.0)) {
                ok = false;
                break;
            }
            const double lz = std::log(z);
            const double zr = std::exp(std::clamp(mleM * (lz - lnS0), -700.0, 700.0));
            if (!it.censored)
                lnL += std::log(mleM) - lnS0 + (mleM - 1.0) * (lz - lnS0);
            lnL -= zr;
        }
        if (ok && std::isfinite(lnL))
            res.logLikelihood = lnL;
    }

    if (useMle && !mleConverged)
        res.notes << QObject::tr("MLE did not converge; median rank regression values are reported instead.");

    if (res.usedMle) {
        betaM = mleM;
        betaSE = mleMSE;
        interceptA = -mleM * std::log(std::max(1e-300, mleSigma0));
    }
    res.modulusMUncorrected = betaM;

    // Estimation method actually used (F9)
    if (res.usedMle) {
        res.methodDescription = usedThreeParam
                ? QObject::tr("3-parameter: \u03B3 estimated by MRR; m and \u03C3\u2080 by MLE conditional on that \u03B3 (their SE does not include the uncertainty of \u03B3)")
                : QObject::tr("Maximum likelihood, 2-parameter");
    } else {
        res.methodDescription = usedThreeParam
                ? QObject::tr("Median rank regression (Benard), 3-parameter: \u03B3 chosen to minimise the regression residual")
                : QObject::tr("Median rank regression (Benard), 2-parameter");
    }

    // JIS R 1625:2010 bias correction of the ML modulus (complete data, 2-parameter only)
    if (jisUnbiasMle) {
        if (res.usedMle && !usedThreeParam && nCens == 0) {
            const double bn = jisR1625UnbiasingFactor(mFail);
            res.unbiased = true;
            res.unbiasingFactor = bn;
            const double mRaw = betaM;
            betaM *= bn;
            betaSE *= bn;
            // sigma0 stays the ML estimate; keep the plotted line through it
            interceptA = -betaM * std::log(std::max(1e-300, mleSigma0));
            res.notes << QObject::tr(
                    "m bias-corrected by JIS R 1625 (B_n = %1, n = %2): m = %3 \u2192 %4; \u03C3\u2080 is the uncorrected MLE.")
                                 .arg(bn, 0, 'f', 4)
                                 .arg(mFail)
                                 .arg(mRaw, 0, 'f', 3)
                                 .arg(betaM, 0, 'f', 3);
            if (mFail > 120) {
                res.notes << QObject::tr(
                        "JIS R 1625 6.2 c) sets B_n = 1 for n > 120, so m is not reduced (n = %1).")
                                     .arg(mFail);
            }
        } else {
            res.notes << QObject::tr(
                    "JIS R 1625 bias correction not applied: it is defined only for the 2-parameter MLE of complete (uncensored) data.");
        }
    }

    const double effSigma0 = res.usedMle ? mleSigma0 : sigma0;
    const double effSigma0SE = res.usedMle ? mleSigma0SE : (sigma0 * lnSigma0SE);

    res.valid = true;
    res.modulusM = betaM;
    res.modulusMSE = betaSE;
    res.scaleSigma0 = effSigma0;
    res.scaleSigma0SE = effSigma0SE;
    res.thresholdGamma = bestGamma;
    res.threeParameter = usedThreeParam;
    res.b10Life = bestGamma + effSigma0 * std::pow(-std::log(0.90), 1.0 / betaM);
    res.b50Life = bestGamma + effSigma0 * std::pow(std::log(2.0), 1.0 / betaM);
    res.rSquare = r2;
    if (res.usedMle && std::isfinite(res.logLikelihood)) {
        res.notes << QObject::tr(
                "Goodness of fit: ln L = %1 (MLE). R\u00B2 = %2 refers to the median-rank regression line, not to the MLE.")
                             .arg(res.logLikelihood, 0, 'f', 3)
                             .arg(r2, 0, 'f', 4);
    }
    flushWeibullLog();

    if (!app)
        return res;

    const double tCrit = hasSe
            ? gsl_cdf_tdist_Pinv(0.975, static_cast<double>(residDf))
            : 0.0;

    const QString tblName = app->generateUniqueName(
            srcTable->name() + QStringLiteral("_Weibull"), false);
    Table *repTbl = app->newTable(mFail, 7, tblName, QObject::tr("Weibull Report"));
    res.reportTable = repTbl;
    if (!repTbl)
        return res;

    repTbl->setHeader(QStringList() << QStringLiteral("LnValue") << QStringLiteral("Strength_Life")
                                    << QStringLiteral("FailureProb_Pct") << QStringLiteral("Weibit")
                                    << QStringLiteral("WeibullFit") << QStringLiteral("CI95Lower")
                                    << QStringLiteral("CI95Upper"));
    for (int c = 0; c < 7; ++c)
        repTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    repTbl->column(0)->setPlotDesignation(SciDAVis::X);
    repTbl->column(1)->setPlotDesignation(SciDAVis::noDesignation);
    repTbl->column(2)->setPlotDesignation(SciDAVis::noDesignation);
    for (int c = 3; c <= 6; ++c)
        repTbl->column(c)->setPlotDesignation(SciDAVis::Y);

    std::vector<double> ciLoVec(static_cast<size_t>(mFail), 0.0);
    std::vector<double> ciHiVec(static_cast<size_t>(mFail), 0.0);
    const double effLnS0 = std::log(std::max(1e-300, effSigma0));
    for (int i = 0; i < mFail; ++i) {
        const double uv = uLnX[static_cast<size_t>(i)];
        const double wh = interceptA + betaM * uv;
        double hb = 0.0;
        if (res.usedMle && mleVarLnS0 > 0.0) {
            // Delta-method 95% confidence band on W(u) = m * (u - ln(sigma0))
            const double du = uv - effLnS0;
            const double varM = mleMSE * mleMSE;
            const double covMU0 = -mleCovLnS0InvM * betaM * betaM;
            const double varW = std::max(
                    0.0, du * du * varM + betaM * betaM * mleVarLnS0 - 2.0 * du * betaM * covMU0);
            hb = 1.959963984540054 * std::sqrt(varW);
        } else if (hasSe) {
            hb = tCrit * sResid
                    * std::sqrt(1.0 / static_cast<double>(mFail)
                                + ((uv - meanU) * (uv - meanU)) / suu);
        }
        ciLoVec[static_cast<size_t>(i)] = wh - hb;
        ciHiVec[static_cast<size_t>(i)] = wh + hb;

        repTbl->column(0)->setValueAt(i, uv);
        repTbl->column(1)->setValueAt(i, failX[static_cast<size_t>(i)]);
        repTbl->column(2)->setValueAt(i, 100.0 * failProb[static_cast<size_t>(i)]);
        repTbl->column(3)->setValueAt(i, wWeibit[static_cast<size_t>(i)]);
        repTbl->column(4)->setValueAt(i, wh);
        repTbl->column(5)->setValueAt(i, wh - hb);
        repTbl->column(6)->setValueAt(i, wh + hb);
    }
    if (Column *c3 = repTbl->column(3))
        c3->setComment(QStringLiteral("Empirical"));
    if (Column *c4 = repTbl->column(4))
        c4->setComment(res.usedMle ? QStringLiteral("MLE fit") : QStringLiteral("MRR fit"));

    if (!createPlot)
        return res;

    MultiLayer *ml = app->newGraph(
            app->generateUniqueName(QStringLiteral("WeibullPlot_") + srcTable->name(), false));
    res.weibullPlot = ml;
    if (ml) {
        if (Graph *g = ml->activeGraph()) {
            // Non-legend shaded 95% confidence band (replaces cluttered dashed CI curves)
            if ((hasSe && sResid > 0.0) || (res.usedMle && mleVarLnS0 > 0.0)) {
                QVector<double> vx(mFail), vyLo(mFail), vyHi(mFail);
                for (int i = 0; i < mFail; ++i) {
                    vx[i] = uLnX[static_cast<size_t>(i)];
                    vyLo[i] = ciLoVec[static_cast<size_t>(i)];
                    vyHi[i] = ciHiVec[static_cast<size_t>(i)];
                }
                if (QwtPlotItem *band =
                            g->addShadedBand(vx, vyLo, vyHi, QColor(205, 35, 45, 38), QString())) {
                    band->setZ(1.0);
                    band->setItemAttribute(QwtPlotItem::Legend, false);
                }
            }

            if (g->insertCurve(repTbl, repTbl->colName(0), repTbl->colName(3), Graph::Scatter, 0,
                               mFail - 1)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, QPen(QColor(25, 70, 145), 1.5));
                g->setCurveSymbol(idx,
                                  QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(35, 100, 190)),
                                            QPen(QColor(15, 45, 100), 1.2), QSize(7, 7)));
            }
            if (g->insertCurve(repTbl, repTbl->colName(0), repTbl->colName(4), Graph::Line, 0,
                               mFail - 1)) {
                g->setCurvePen(g->curves() - 1, QPen(QColor(205, 35, 45), 2.1, Qt::SolidLine));
            }

            // Characteristic strength reference W = 0 (P_f = 63.2%)
            Graph::ReferenceLineSpec wZeroRef;
            wZeroRef.orientation = 0; // Horizontal Y
            wZeroRef.value = 0.0;
            wZeroRef.color = QColor(120, 130, 145, 180);
            wZeroRef.style = Qt::DotLine;
            wZeroRef.width = 1.1;
            g->addReferenceLine(wZeroRef);

            const double uSpan = std::max(0.08, uLnX.back() - uLnX.front());
            const double xLo = uLnX.front() - 0.08 * uSpan;
            const double xHi = uLnX.back() + 0.08 * uSpan;
            const double wLo = std::min({ wWeibit.front() - 0.35, ciLoVec.front() - 0.15, -4.65 });
            const double wHi = std::max({ wWeibit.back() + 0.30, ciHiVec.back() + 0.15, 1.60 });

            g->applyFramePreset(Graph::FrameBoxInwardTicks);
            const QString uStr = cData->units().trimmed();
            if (usedThreeParam) {
                g->setXAxisTitle(uStr.isEmpty()
                                         ? QStringLiteral("ln(<i>\u03C3</i> \u2212 <i>\u03B3</i>)")
                                         : QStringLiteral("ln((<i>\u03C3</i> \u2212 <i>\u03B3</i>) / %1)")
                                                   .arg(uStr));
            } else {
                g->setXAxisTitle(uStr.isEmpty()
                                         ? QStringLiteral("ln(<i>\u03C3</i>)")
                                         : QStringLiteral("ln(<i>\u03C3</i> / %1)").arg(uStr));
            }
            g->setYAxisTitle(QStringLiteral("ln(\u2212ln(1 \u2212 <i>P</i><sub>f</sub>))"));
            g->setTitle(QString());
            if (g->legend())
                g->legend()->setText(g->legendText());
            g->setScale(QwtPlot::xBottom, xLo, xHi);
            g->setScale(QwtPlot::yLeft, wLo, wHi);
            applyArialGraphFonts(g);

            // Linked Top-X Stress/Strength axis (exp(x) or gamma + exp(x)) & Right-Y P_f (%) axis
            const QString topFormula = usedThreeParam
                    ? QStringLiteral("%1 + exp(x)").arg(bestGamma, 0, 'g', 12)
                    : QStringLiteral("exp(x)");
            Graph *linked = ml->addLinkedLayer(g, true, false, topFormula, QString());
            if (linked && linked->plotWidget()) {
                linked->setAxisTitle(
                        QwtPlot::xTop,
                        (uStr.isEmpty() || uStr == QLatin1String("-"))
                                ? QStringLiteral("<i>\u03C3</i>")
                                : QStringLiteral("<i>\u03C3</i> / %1").arg(uStr));
                linked->enableAxis(QwtPlot::yRight, true);
                linked->setAxisBackboneVisible(QwtPlot::yRight, true);
                linked->setAxisTitle(
                        QwtPlot::yRight,
                        QObject::tr("Failure probability, <i>P</i><sub>f</sub> / %"));
                linked->setWeibullProbabilityAxis(QwtPlot::yRight, true);
                Plot *cPlot = linked->plotWidget();
                if (!linked->hasWeibullProbabilityAxis(QwtPlot::yRight)) {
                    WeibullProbScaleDraw *probSd = nullptr;
                    if (auto *origSd = dynamic_cast<ScaleDraw *>(cPlot->axisScaleDraw(QwtPlot::yRight)))
                        probSd = new WeibullProbScaleDraw(*origSd);
                    else
                        probSd = new WeibullProbScaleDraw();
                    probSd->setMajorTicksStyle(ScaleDraw::In);
                    probSd->setMinorTicksStyle(ScaleDraw::In);
                    probSd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                    cPlot->setAxisScaleDraw(QwtPlot::yRight, probSd);
                }
                cPlot->setMajorTicksType(QwtPlot::yRight, ScaleDraw::In);
                cPlot->setMinorTicksType(QwtPlot::yRight, ScaleDraw::In);
                applyArialGraphFonts(linked);
                syncWeibullProbRightAxis(g, linked);

                QPointer<Graph> pG(g);
                QPointer<Graph> cG(linked);
                QObject::connect(g, &Graph::modifiedGraph, linked, [pG, cG]() {
                    if (pG && cG)
                        syncWeibullProbRightAxis(pG.data(), cG.data());
                });
            }

            ml->arrangeLayers(false, false);
            g->syncChildLinkedLayers();
            if (linked)
                syncWeibullProbRightAxis(g, linked);
            g->replot();
            if (linked)
                linked->replot();
            ml->setActiveGraph(g);

            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);

            QString estTag = res.usedMle
                    ? (usedThreeParam ? QStringLiteral("cond. MLE, \u03B3: MRR") : QStringLiteral("MLE"))
                    : QStringLiteral("MRR");
            if (res.unbiased)
                estTag += QStringLiteral(", JIS R 1625");
            const QString fitQual = (res.usedMle && std::isfinite(res.logLikelihood))
                    ? QStringLiteral("ln <i>L</i> = %1").arg(res.logLikelihood, 0, 'f', 2)
                    : QStringLiteral("<i>R</i><sup>2</sup> = %1").arg(res.rSquare, 0, 'f', 4);
            // An undefined standard error (no residual degrees of freedom) reads N/A.
            auto seText = [](double v, char fmt, int prec) {
                return std::isfinite(v) ? QString::number(v, fmt, prec) : QStringLiteral("N/A");
            };
            QString summary;
            if (usedThreeParam) {
                summary = QStringLiteral(
                        "<i>m</i> (%1) = %2 \u00B1 %3\n"
                        "<i>\u03C3</i><sub>0</sub> = %4 \u00B1 %5%11\n"
                        "<i>\u03B3</i> = %6, <i>B</i><sub>10</sub> = %7\n"
                        "%8 (<i>n</i> = %9, cens = %10)")
                        .arg(estTag)
                        .arg(res.modulusM, 0, 'f', 2)
                        .arg(seText(res.modulusMSE, 'f', 2))
                        .arg(res.scaleSigma0, 0, 'g', 4)
                        .arg(seText(res.scaleSigma0SE, 'g', 3))
                        .arg(res.thresholdGamma, 0, 'g', 4)
                        .arg(res.b10Life, 0, 'g', 4)
                        .arg(fitQual)
                        .arg(mFail)
                        .arg(res.censoredCount)
                        .arg(uStr.isEmpty() ? QString() : (QStringLiteral(" ") + uStr));
            } else {
                summary = QStringLiteral(
                        "<i>m</i> (%1) = %2 \u00B1 %3\n"
                        "<i>\u03C3</i><sub>0</sub> = %4 \u00B1 %5%10\n"
                        "<i>B</i><sub>10</sub> = %6, <i>B</i><sub>50</sub> = %7\n"
                        "%8 (<i>n</i> = %9%11)")
                        .arg(estTag)
                        .arg(res.modulusM, 0, 'f', 2)
                        .arg(seText(res.modulusMSE, 'f', 2))
                        .arg(res.scaleSigma0, 0, 'g', 4)
                        .arg(seText(res.scaleSigma0SE, 'g', 3))
                        .arg(res.b10Life, 0, 'g', 4)
                        .arg(res.b50Life, 0, 'g', 4)
                        .arg(fitQual)
                        .arg(mFail)
                        .arg(uStr.isEmpty() ? QString() : (QStringLiteral(" ") + uStr))
                        .arg(res.censoredCount > 0
                                     ? QStringLiteral(", cens = %1").arg(res.censoredCount)
                                     : QString());
            }
            if (Legend *callout = addFramedBoxOnGraph(
                        g, xLo + 0.05 * (xHi - xLo), wHi - 0.12 * (wHi - wLo), summary)) {
                attachStatsCalloutAutoAvoid(g, callout, 14);
            }
            g->replot();
            if (linked)
                linked->replot();
        }
    }
    return res;
}

GraphStatsAndSurfaceFit::OutlierTestResult GraphStatsAndSurfaceFit::runOutlierTest(
        ApplicationWindow *app, Table *srcTable, const QString &colName, int method, double alpha,
        int maxOutliers, bool maskDetectedOutliers)
{
    registerPhase5StatsTranslationsOnce();
    OutlierTestResult res;
    if (!srcTable)
        return res;

    Column *col = resolveTableColumn(srcTable, colName);
    if (!col)
        return res;

    struct Obs {
        int row = 0;
        double val = 0.0;
    };
    std::vector<Obs> obs;
    obs.reserve(static_cast<size_t>(std::max(0, col->rowCount())));
    for (int r = 0; r < col->rowCount(); ++r) {
        double v = 0.0;
        if (readNumericColumnCell(col, r, v))
            obs.push_back({ r, v });
    }

    const int n = static_cast<int>(obs.size());
    res.n = n;
    if (n < 3)
        return res;

    const double sigAlpha = std::clamp(alpha, 1e-6, 0.50);
    const int effMethod = std::clamp(method, 0, 2);

    auto grubbsCritAndP = [](int m, double gObs, double a, double &gCrit, double &pVal) {
        const double dm = static_cast<double>(m);
        const double tc = gsl_cdf_tdist_Pinv(1.0 - a / (2.0 * dm), dm - 2.0);
        gCrit = ((dm - 1.0) / std::sqrt(dm)) * std::sqrt((tc * tc) / (dm - 2.0 + tc * tc));
        const double gMaxSq = ((dm - 1.0) * (dm - 1.0)) / dm;
        if (gObs <= 0.0) {
            pVal = 1.0;
        } else if (gObs * gObs >= gMaxSq - 1e-14) {
            pVal = 0.0;
        } else {
            const double tSq = ((dm - 2.0) * gObs * gObs) / (gMaxSq - gObs * gObs);
            const double tVal = std::sqrt(std::max(0.0, tSq));
            pVal = std::clamp(2.0 * dm * gsl_cdf_tdist_Q(tVal, dm - 2.0), 0.0, 1.0);
        }
    };

    std::vector<double> outlierCritVals;
    std::vector<double> outlierStepPs;

    QString methodLabel;
    if (effMethod == 0) {
        methodLabel = QStringLiteral("Grubbs Two-Sided Test");
        double mean = 0.0;
        for (const Obs &o : obs)
            mean += o.val;
        mean /= static_cast<double>(n);

        double ss = 0.0;
        int bestIdx = 0;
        double maxDev = -1.0;
        for (int i = 0; i < n; ++i) {
            const double dev = std::fabs(obs[static_cast<size_t>(i)].val - mean);
            ss += dev * dev;
            if (dev > maxDev) {
                maxDev = dev;
                bestIdx = i;
            }
        }
        const double sd = std::sqrt(ss / static_cast<double>(n - 1));
        if (sd <= 1e-30)
            return res;

        const double gStat = maxDev / sd;
        double gCrit = 0.0, pVal = 1.0;
        grubbsCritAndP(n, gStat, sigAlpha, gCrit, pVal);
        res.valid = true;
        res.primaryStatistic = gStat;
        res.testStatistic = gStat;
        res.criticalValue = gCrit;
        res.pValue = pVal;
        if (gStat > gCrit) {
            res.outlierRows.append(obs[static_cast<size_t>(bestIdx)].row);
            res.outlierValues.append(obs[static_cast<size_t>(bestIdx)].val);
            outlierCritVals.push_back(gCrit);
            outlierStepPs.push_back(pVal);
        }
    } else if (effMethod == 1) {
        methodLabel = QStringLiteral("Generalized ESD (Rosner) Test");
        const int rMax = std::clamp(maxOutliers, 1, std::max(1, (n - 3) / 2));
        std::vector<Obs> pool = obs;
        struct EsdStep {
            Obs removed;
            double rStat = 0.0;
            double lambdaCrit = 0.0;
            double stepP = 1.0;
        };
        std::vector<EsdStep> steps;
        steps.reserve(static_cast<size_t>(rMax));

        for (int k = 0; k < rMax && static_cast<int>(pool.size()) >= 3; ++k) {
            const int m = static_cast<int>(pool.size());
            double mean = 0.0;
            for (const Obs &o : pool)
                mean += o.val;
            mean /= static_cast<double>(m);
            double ss = 0.0;
            int maxIdx = 0;
            double maxDev = -1.0;
            for (int i = 0; i < m; ++i) {
                const double dev = std::fabs(pool[static_cast<size_t>(i)].val - mean);
                ss += dev * dev;
                if (dev > maxDev) {
                    maxDev = dev;
                    maxIdx = i;
                }
            }
            const double sd = std::sqrt(ss / static_cast<double>(m - 1));
            if (sd <= 1e-30)
                break;

            const double rStat = maxDev / sd;
            double lam = 0.0, pStep = 1.0;
            grubbsCritAndP(m, rStat, sigAlpha, lam, pStep);
            steps.push_back({ pool[static_cast<size_t>(maxIdx)], rStat, lam, pStep });
            pool.erase(pool.begin() + maxIdx);
        }

        if (steps.empty())
            return res;

        int numOut = 0;
        for (int i = static_cast<int>(steps.size()) - 1; i >= 0; --i) {
            if (steps[static_cast<size_t>(i)].rStat > steps[static_cast<size_t>(i)].lambdaCrit) {
                numOut = i + 1;
                break;
            }
        }

        const size_t reportIdx = (numOut > 0) ? static_cast<size_t>(numOut - 1) : 0;
        res.valid = true;
        res.primaryStatistic = steps[reportIdx].rStat;
        res.testStatistic = steps[reportIdx].rStat;
        res.criticalValue = steps[reportIdx].lambdaCrit;
        res.pValue = steps[reportIdx].stepP;

        for (int i = 0; i < numOut; ++i) {
            const EsdStep &st = steps[static_cast<size_t>(i)];
            res.outlierRows.append(st.removed.row);
            res.outlierValues.append(st.removed.val);
            outlierCritVals.push_back(st.lambdaCrit);
            outlierStepPs.push_back(st.stepP);
        }
    } else {
        methodLabel = QStringLiteral("Dixon Q Test");
        std::vector<Obs> sorted = obs;
        std::sort(sorted.begin(), sorted.end(),
                  [](const Obs &a, const Obs &b) { return a.val < b.val; });
        const double span = sorted.back().val - sorted.front().val;
        if (span <= 1e-30)
            return res;

        const double qLow = (sorted[1].val - sorted[0].val) / span;
        const double qHigh = (sorted[static_cast<size_t>(n - 1)].val
                              - sorted[static_cast<size_t>(n - 2)].val) / span;
        const bool highExtreme = (qHigh >= qLow);
        const double qStat = highExtreme ? qHigh : qLow;
        const Obs cand = highExtreme ? sorted.back() : sorted.front();

        double qCrit = 0.0, pVal = 1.0;
        evalDixonQCritAndP(n, qStat, sigAlpha, qCrit, pVal);
        res.valid = true;
        res.primaryStatistic = qStat;
        res.testStatistic = qStat;
        res.criticalValue = qCrit;
        res.pValue = pVal;
        if (qStat > qCrit) {
            res.outlierRows.append(cand.row);
            res.outlierValues.append(cand.val);
            outlierCritVals.push_back(qCrit);
            outlierStepPs.push_back(pVal);
        }
    }

    if (maskDetectedOutliers && !res.outlierRows.isEmpty()) {
        for (int r : res.outlierRows)
            col->setMasked(r, true);
    }

    if (!app)
        return res;

    const int nRepRows = 1 + res.outlierRows.size();
    const QString tblName = app->generateUniqueName(
            srcTable->name() + QStringLiteral("_Outliers"), false);
    Table *repTbl = app->newTable(nRepRows, 6, tblName, QObject::tr("Outlier Test Report"));
    res.reportTable = repTbl;
    if (repTbl) {
        repTbl->setHeader(QStringList() << QStringLiteral("Item") << QStringLiteral("Row_or_N")
                                        << QStringLiteral("Value_or_Stat")
                                        << QStringLiteral("CriticalValue")
                                        << QStringLiteral("p_value")
                                        << QStringLiteral("Decision"));
        repTbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 1; c <= 4; ++c)
            repTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        repTbl->column(5)->setColumnMode(SciDAVis::ColumnMode::Text);

        repTbl->column(0)->setTextAt(0, methodLabel);
        repTbl->column(1)->setValueAt(0, static_cast<double>(n));
        repTbl->column(2)->setValueAt(0, res.testStatistic);
        repTbl->column(3)->setValueAt(0, res.criticalValue);
        repTbl->column(4)->setValueAt(0, res.pValue);
        repTbl->column(5)->setTextAt(0, res.outlierRows.isEmpty()
                                                ? QStringLiteral("No Outliers")
                                                : QStringLiteral("%1 Outlier(s)")
                                                          .arg(res.outlierRows.size()));

        for (int k = 0; k < res.outlierRows.size(); ++k) {
            const int r = k + 1;
            const double stepCrit = (static_cast<size_t>(k) < outlierCritVals.size())
                    ? outlierCritVals[static_cast<size_t>(k)]
                    : res.criticalValue;
            const double stepPVal = (static_cast<size_t>(k) < outlierStepPs.size())
                    ? outlierStepPs[static_cast<size_t>(k)]
                    : res.pValue;
            repTbl->column(0)->setTextAt(r, QStringLiteral("Outlier #%1").arg(k + 1));
            repTbl->column(1)->setValueAt(r, static_cast<double>(res.outlierRows[k] + 1));
            repTbl->column(2)->setValueAt(r, res.outlierValues[k]);
            repTbl->column(3)->setValueAt(r, stepCrit);
            repTbl->column(4)->setValueAt(r, stepPVal);
            repTbl->column(5)->setTextAt(r, maskDetectedOutliers ? QStringLiteral("Masked")
                                                                 : QStringLiteral("Detected"));
        }
    }
    return res;
}

GraphStatsAndSurfaceFit::NonparamResult GraphStatsAndSurfaceFit::runNonparametricComparison(
        ApplicationWindow *app, Table *srcTable, const QStringList &colNames,
        NonparamMethod method, Graph *annotateGraph)
{
    registerPhase5StatsTranslationsOnce();
    NonparamResult res;
    if (!srcTable || colNames.size() < 2) {
        res.message = QObject::tr("At least 2 columns are needed.");
        return res;
    }

    // Special paired path for Wilcoxon Signed-Rank
    if (method == TestWilcoxonSignedRank) {
        Column *cA = resolveTableColumn(srcTable, colNames[0]);
        Column *cB = resolveTableColumn(srcTable, colNames[1]);
        if (!cA || !cB)
            return res;
        std::vector<double> diffs;
        std::vector<double> allVals;
        const int nRows = std::min(cA->rowCount(), cB->rowCount());
        for (int r = 0; r < nRows; ++r) {
            double va = 0.0, vb = 0.0;
            if (readNumericColumnCell(cA, r, va) && readNumericColumnCell(cB, r, vb)) {
                diffs.push_back(vb - va);
                allVals.push_back(va);
                allVals.push_back(vb);
            }
        }
        if (diffs.size() < 2) {
            res.message = QObject::tr(
                    "Wilcoxon Signed-Rank needs at least 2 rows with values in both columns (%1 found).")
                                  .arg(int(diffs.size()));
            return res;
        }

        double wStat = 0.0, pVal = 1.0;
        evalWilcoxonSignedRank(diffs, wStat, pVal);
        res.valid = true;
        res.chosenTestName = QStringLiteral("Wilcoxon Signed-Rank");
        res.statistic = wStat;
        res.pValue = pVal;

        QString multiColNote;
        if (colNames.size() > 2) {
            multiColNote = QObject::tr(
                    "Wilcoxon Signed-Rank compares only the first 2 selected columns (%1 vs %2); %3 additional column(s) ignored.")
                                   .arg(cA->name(), cB->name())
                                   .arg(colNames.size() - 2);
        }

        if (app) {
            if (!multiColNote.isEmpty()) {
                app->updateLog(QStringLiteral("[Wilcoxon] ") + multiColNote + QLatin1Char('\n'));
                if (!isNonInteractiveApp(app) && app->statusBar())
                    app->statusBar()->showMessage(multiColNote, 8000);
            }
            const QString tblName = app->generateUniqueName(
                    srcTable->name() + QStringLiteral("_Wilcoxon"), false);
            const int nRep = multiColNote.isEmpty() ? 1 : 2;
            Table *rep = app->newTable(nRep, 5, tblName,
                                       QObject::tr("Nonparametric Comparison Summary"));
            if (rep) {
                rep->setHeader(QStringList() << QStringLiteral("Comparison")
                                             << QStringLiteral("Test")
                                             << QStringLiteral("Statistic")
                                             << QStringLiteral("p_value")
                                             << QStringLiteral("Significance"));
                rep->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
                rep->column(1)->setColumnMode(SciDAVis::ColumnMode::Text);
                rep->column(2)->setColumnMode(SciDAVis::ColumnMode::Numeric);
                rep->column(3)->setColumnMode(SciDAVis::ColumnMode::Numeric);
                rep->column(4)->setColumnMode(SciDAVis::ColumnMode::Text);
                rep->column(0)->setTextAt(0, QStringLiteral("%1 vs %2")
                                                     .arg(cA->name(), cB->name()));
                rep->column(1)->setTextAt(0, res.chosenTestName);
                rep->column(2)->setValueAt(0, wStat);
                rep->column(3)->setValueAt(0, pVal);
                rep->column(4)->setTextAt(0, starRatingForP(pVal));
                if (!multiColNote.isEmpty()) {
                    rep->setWindowLabel(multiColNote);
                    rep->column(0)->setTextAt(1, QObject::tr("Note"));
                    rep->column(1)->setTextAt(1, multiColNote);
                    rep->column(2)->setInvalid(1, true);
                    rep->column(3)->setInvalid(1, true);
                    rep->column(4)->setTextAt(1, QString());
                }
                res.reportTable = rep;
            }
        }
        if (annotateGraph && !allVals.empty()) {
            const QVariant showNsProp = srcTable->property("torigraph_anova_show_ns");
            const bool showNsExplicit = showNsProp.isValid() && !showNsProp.isNull();
            const bool showNs = showNsExplicit ? showNsProp.toBool() : true;
            if (showNs || pVal < 0.05) {
                const auto mm = std::minmax_element(allVals.begin(), allVals.end());
                annotateBracketsOnGraph(annotateGraph, 2, *mm.first, *mm.second,
                                        { std::make_tuple(0, 1, pVal) },
                                        QStringList() << cA->name() << cB->name(), srcTable,
                                        parseBracketLabelFormat(srcTable));
            }
        }
        return res;
    }

    struct GroupSample {
        QString name;
        std::vector<double> vals;
        double mean = 0.0;
        double var = 0.0;
        double minV = 0.0;
        double maxV = 0.0;
    };
    std::vector<GroupSample> groups;
    for (const QString &cn : colNames) {
        Column *col = resolveTableColumn(srcTable, cn);
        if (!col)
            continue;
        GroupSample gs;
        gs.name = col->name();
        for (int r = 0; r < col->rowCount(); ++r) {
            double v = 0.0;
            if (readNumericColumnCell(col, r, v))
                gs.vals.push_back(v);
        }
        // A group of one value has a rank (Mann-Whitney, Kruskal-Wallis): it is kept, as in
        // the textbook tests. It used to be dropped without a word (A(5), B(5), C(1) gave the
        // H of A and B alone).
        if (gs.vals.empty())
            continue;
        gs.mean = std::accumulate(gs.vals.begin(), gs.vals.end(), 0.0)
                / static_cast<double>(gs.vals.size());
        double ss = 0.0;
        gs.minV = gs.vals.front();
        gs.maxV = gs.vals.front();
        for (double v : gs.vals) {
            const double d = v - gs.mean;
            ss += d * d;
            gs.minV = std::min(gs.minV, v);
            gs.maxV = std::max(gs.maxV, v);
        }
        gs.var = (gs.vals.size() > 1) ? ss / static_cast<double>(gs.vals.size() - 1) : 0.0;
        groups.push_back(std::move(gs));
    }

    const int K = static_cast<int>(groups.size());
    if (K < 2) {
        res.message = QObject::tr(
                "At least 2 groups with valid numeric values are needed (%1 found).")
                              .arg(K);
        return res;
    }

    double yMinAll = groups.front().minV;
    double yMaxAll = groups.front().maxV;
    for (const GroupSample &gs : groups) {
        yMinAll = std::min(yMinAll, gs.minV);
        yMaxAll = std::max(yMaxAll, gs.maxV);
    }

    NonparamMethod effMethod = method;
    if (method == TestAutoSmartSwitch) {
        bool allNormal = true;
        for (const GroupSample &gs : groups) {
            const NormalityResult nr = runNormalityTest(nullptr, srcTable, gs.name, nullptr);
            if (nr.n < 3 || nr.shapiroWilkP < 0.05) {
                allNormal = false;
                break;
            }
        }

        if (allNormal) {
            // Parametric delegation: Welch t-Test (K==2) or One-Way ANOVA + Tukey-Kramer (K>=3)
            size_t totalN = 0;
            double wSum = 0.0;
            for (const GroupSample &gs : groups) {
                totalN += gs.vals.size();
                wSum += static_cast<double>(gs.vals.size()) * gs.mean;
            }
            const double grandMean = wSum / static_cast<double>(totalN);
            double ssBetween = 0.0, ssWithin = 0.0;
            for (const GroupSample &gs : groups) {
                const double d = gs.mean - grandMean;
                ssBetween += static_cast<double>(gs.vals.size()) * d * d;
                ssWithin += static_cast<double>(gs.vals.size() - 1) * gs.var;
            }
            const double df1 = static_cast<double>(K - 1);
            const double df2 = static_cast<double>(totalN - static_cast<size_t>(K));
            const double msBetween = ssBetween / df1;
            const double msWithin = (df2 > 0.0) ? (ssWithin / df2) : 0.0;

            if (K == 2) {
                const double n0 = static_cast<double>(groups[0].vals.size());
                const double n1 = static_cast<double>(groups[1].vals.size());
                const double v0 = groups[0].var / n0;
                const double v1 = groups[1].var / n1;
                const double vSum = v0 + v1;
                double tStat = 0.0, pWelch = 1.0, dfW = n0 + n1 - 2.0;
                if (vSum > 1e-30) {
                    tStat = (groups[0].mean - groups[1].mean) / std::sqrt(vSum);
                    const double den = (v0 * v0) / (n0 - 1.0) + (v1 * v1) / (n1 - 1.0);
                    dfW = (den > 1e-30) ? ((vSum * vSum) / den) : (n0 + n1 - 2.0);
                    pWelch = std::clamp(2.0 * gsl_cdf_tdist_Q(std::fabs(tStat), dfW), 0.0, 1.0);
                } else if (std::fabs(groups[0].mean - groups[1].mean) > 1e-30) {
                    tStat = (groups[0].mean > groups[1].mean) ? 1e6 : -1e6;
                    pWelch = 0.0;
                }
                res.valid = true;
                res.chosenTestName = QStringLiteral("Welch t-Test (Auto Parametric)");
                res.statistic = tStat;
                res.pValue = pWelch;
                // Emit a consistent Welch t-Test report table (not equal-variance One-Way ANOVA, E4-6)
                if (app) {
                    const QString tblName = app->generateUniqueName(
                            srcTable->name() + QStringLiteral("_WelchTTest"), false);
                    Table *rep = app->newTable(1, 7, tblName,
                                               QObject::tr("Welch t-Test Summary (Auto Parametric)"));
                    if (rep) {
                        rep->setHeader(QStringList() << QStringLiteral("Comparison")
                                                     << QStringLiteral("Test")
                                                     << QStringLiteral("Statistic")
                                                     << QStringLiteral("df")
                                                     << QStringLiteral("p_value")
                                                     << QStringLiteral("MeanDiff")
                                                     << QStringLiteral("Significance"));
                        rep->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
                        rep->column(1)->setColumnMode(SciDAVis::ColumnMode::Text);
                        for (int c = 2; c <= 5; ++c)
                            rep->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
                        rep->column(6)->setColumnMode(SciDAVis::ColumnMode::Text);
                        rep->column(0)->setTextAt(0, QStringLiteral("%1 vs %2")
                                                             .arg(groups[0].name, groups[1].name));
                        rep->column(1)->setTextAt(0, QStringLiteral("Welch t-Test"));
                        rep->column(2)->setValueAt(0, tStat);
                        rep->column(3)->setValueAt(0, dfW);
                        rep->column(4)->setValueAt(0, pWelch);
                        rep->column(5)->setValueAt(0, groups[0].mean - groups[1].mean);
                        rep->column(6)->setTextAt(0, starRatingForP(pWelch));
                        res.reportTable = rep;
                    }
                }
                if (annotateGraph) {
                    const QVariant showNsProp = srcTable->property("torigraph_anova_show_ns");
                    const bool showNsExplicit = showNsProp.isValid() && !showNsProp.isNull();
                    const bool showNs = showNsExplicit ? showNsProp.toBool() : true;
                    if (showNs || pWelch < 0.05) {
                        annotateBracketsOnGraph(annotateGraph, 2, yMinAll, yMaxAll,
                                                { std::make_tuple(0, 1, pWelch) },
                                                QStringList() << groups[0].name << groups[1].name,
                                                srcTable, parseBracketLabelFormat(srcTable));
                    }
                }
                return res;
            } else {
                double fStat = 0.0, pAnova = 1.0;
                if (msWithin > 1e-30) {
                    fStat = msBetween / msWithin;
                    pAnova = (fStat > 0.0 && df2 > 0.0) ? gsl_cdf_fdist_Q(fStat, df1, df2) : 1.0;
                } else if (ssBetween > 1e-30) {
                    fStat = 1e12;
                    pAnova = 0.0;
                }
                res.valid = true;
                res.chosenTestName = QStringLiteral("One-Way ANOVA + Tukey (Auto Parametric)");
                res.statistic = fStat;
                res.pValue = pAnova;
                QStringList validNames;
                for (const GroupSample &gs : groups)
                    validNames << gs.name;
                if (app) {
                    // Explicitly run Tukey HSD to match the declared chosenTestName (E4-6)
                    const QVariant prevPosthoc = srcTable->property("torigraph_anova_posthoc");
                    srcTable->setProperty("torigraph_anova_posthoc", QStringLiteral("tukey"));
                    res.reportTable = runGroupComparisonAndAnnotate(app, srcTable, validNames,
                                                                    annotateGraph);
                    srcTable->setProperty("torigraph_anova_posthoc", prevPosthoc);
                }
                return res;
            }
        }

        effMethod = (K == 2) ? TestMannWhitneyU : TestKruskalWallisDunn;
    }

    if (effMethod == TestMannWhitneyU) {
        const GroupSample &g0 = groups[0];
        const GroupSample &g1 = groups[1];
        const size_t n0 = g0.vals.size();
        const size_t n1 = g1.vals.size();
        std::vector<double> pooled;
        pooled.reserve(n0 + n1);
        pooled.insert(pooled.end(), g0.vals.begin(), g0.vals.end());
        pooled.insert(pooled.end(), g1.vals.begin(), g1.vals.end());

        std::vector<double> ranks;
        double tieSum = 0.0;
        computeMidRanksWithTies(pooled, ranks, tieSum);

        double r0 = 0.0;
        for (size_t i = 0; i < n0; ++i)
            r0 += ranks[i];
        const double dn0 = static_cast<double>(n0);
        const double dn1 = static_cast<double>(n1);
        const double dN = dn0 + dn1;
        const double u0 = r0 - 0.5 * dn0 * (dn0 + 1.0);
        const double u1 = dn0 * dn1 - u0;
        const double uStat = std::min(u0, u1);
        const double muU = 0.5 * dn0 * dn1;
        const double varU = (dn0 * dn1 / 12.0)
                * ((dN + 1.0) - tieSum / std::max(1.0, dN * (dN - 1.0)));
        double pVal = 1.0;
        if (varU > 1e-30) {
            const double z = std::max(0.0, std::fabs(u0 - muU) - 0.5) / std::sqrt(varU);
            pVal = std::clamp(2.0 * gsl_cdf_ugaussian_Q(z), 0.0, 1.0);
        }

        res.valid = true;
        res.chosenTestName = QStringLiteral("Mann-Whitney U");
        res.statistic = uStat;
        res.pValue = pVal;

        if (app) {
            const QString tblName = app->generateUniqueName(
                    srcTable->name() + QStringLiteral("_MannWhitney"), false);
            Table *rep = app->newTable(1, 5, tblName,
                                       QObject::tr("Nonparametric Comparison Summary"));
            if (rep) {
                rep->setHeader(QStringList() << QStringLiteral("Comparison")
                                             << QStringLiteral("Test")
                                             << QStringLiteral("Statistic")
                                             << QStringLiteral("p_value")
                                             << QStringLiteral("Significance"));
                rep->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
                rep->column(1)->setColumnMode(SciDAVis::ColumnMode::Text);
                rep->column(2)->setColumnMode(SciDAVis::ColumnMode::Numeric);
                rep->column(3)->setColumnMode(SciDAVis::ColumnMode::Numeric);
                rep->column(4)->setColumnMode(SciDAVis::ColumnMode::Text);
                rep->column(0)->setTextAt(0, QStringLiteral("%1 vs %2").arg(g0.name, g1.name));
                rep->column(1)->setTextAt(0, res.chosenTestName);
                rep->column(2)->setValueAt(0, uStat);
                rep->column(3)->setValueAt(0, pVal);
                rep->column(4)->setTextAt(0, starRatingForP(pVal));
                res.reportTable = rep;
            }
        }
        if (annotateGraph) {
            const QVariant showNsProp = srcTable->property("torigraph_anova_show_ns");
            const bool showNsExplicit = showNsProp.isValid() && !showNsProp.isNull();
            const bool showNs = showNsExplicit ? showNsProp.toBool() : true;
            if (showNs || pVal < 0.05) {
                annotateBracketsOnGraph(
                        annotateGraph, 2, std::min(g0.minV, g1.minV), std::max(g0.maxV, g1.maxV),
                        { std::make_tuple(0, 1, pVal) }, QStringList() << g0.name << g1.name,
                        srcTable, parseBracketLabelFormat(srcTable));
            }
        }
        return res;
    }

    // Kruskal-Wallis H + Holm-adjusted Dunn post-hoc
    std::vector<double> pooled;
    std::vector<int> grpOf;
    for (int k = 0; k < K; ++k) {
        for (double v : groups[static_cast<size_t>(k)].vals) {
            pooled.push_back(v);
            grpOf.push_back(k);
        }
    }
    const size_t N = pooled.size();
    if (N < 3) {
        res.message = QObject::tr(
                "Kruskal-Wallis needs at least 3 values in all (%1 found).").arg(int(N));
        return res;
    }

    std::vector<double> ranks;
    double tieSum = 0.0;
    computeMidRanksWithTies(pooled, ranks, tieSum);

    std::vector<double> rankSums(static_cast<size_t>(K), 0.0);
    for (size_t i = 0; i < N; ++i)
        rankSums[static_cast<size_t>(grpOf[i])] += ranks[i];

    const double dN = static_cast<double>(N);
    double sumRkSqOverNk = 0.0;
    for (int k = 0; k < K; ++k) {
        const double nk = static_cast<double>(groups[static_cast<size_t>(k)].vals.size());
        const double rk = rankSums[static_cast<size_t>(k)];
        sumRkSqOverNk += (rk * rk) / nk;
    }
    const double hRaw = (12.0 / (dN * (dN + 1.0))) * sumRkSqOverNk - 3.0 * (dN + 1.0);
    const double tieFactor = 1.0 - tieSum / std::max(1.0, dN * dN * dN - dN);
    const double hStat = std::max(0.0, (tieFactor > 1e-12) ? (hRaw / tieFactor) : hRaw);
    const double pKw = std::clamp(gsl_cdf_chisq_Q(hStat, static_cast<double>(K - 1)), 0.0, 1.0);

    struct DunnPair {
        int i = 0;
        int j = 1;
        QString label;
        double zStat = 0.0;
        double pRaw = 1.0;
        double pAdj = 1.0;
    };
    std::vector<DunnPair> pairs;
    const double dunnVarBase =
            std::max(1e-12, (dN * (dN + 1.0)) / 12.0 - tieSum / (12.0 * std::max(1.0, dN - 1.0)));

    for (int i = 0; i < K; ++i) {
        for (int j = i + 1; j < K; ++j) {
            const double ni = static_cast<double>(groups[static_cast<size_t>(i)].vals.size());
            const double nj = static_cast<double>(groups[static_cast<size_t>(j)].vals.size());
            const double meanRi = rankSums[static_cast<size_t>(i)] / ni;
            const double meanRj = rankSums[static_cast<size_t>(j)] / nj;
            const double se = std::sqrt(dunnVarBase * (1.0 / ni + 1.0 / nj));
            const double z = (se > 1e-30) ? (std::fabs(meanRi - meanRj) / se) : 0.0;
            const double pr = std::clamp(2.0 * gsl_cdf_ugaussian_Q(z), 0.0, 1.0);
            pairs.push_back({ i, j,
                              QStringLiteral("%1 vs %2")
                                      .arg(groups[static_cast<size_t>(i)].name,
                                           groups[static_cast<size_t>(j)].name),
                              z, pr, pr });
        }
    }

    // Holm step-down adjustment
    const size_t M = pairs.size();
    if (M > 1) {
        std::vector<size_t> order(M);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](size_t a, size_t b) { return pairs[a].pRaw < pairs[b].pRaw; });
        double runningMax = 0.0;
        for (size_t r = 0; r < M; ++r) {
            const size_t idx = order[r];
            const double adj = std::min(1.0, pairs[idx].pRaw * static_cast<double>(M - r));
            runningMax = std::max(runningMax, adj);
            pairs[idx].pAdj = runningMax;
        }
    }

    res.valid = true;
    res.chosenTestName = QStringLiteral("Kruskal-Wallis H + Dunn");
    res.statistic = hStat;
    res.pValue = pKw;

    if (app) {
        const int nRep = 1 + static_cast<int>(pairs.size());
        const QString tblName = app->generateUniqueName(
                srcTable->name() + QStringLiteral("_KruskalDunn"), false);
        Table *rep = app->newTable(nRep, 6, tblName,
                                   QObject::tr("Nonparametric Comparison Summary"));
        if (rep) {
            rep->setHeader(QStringList() << QStringLiteral("Comparison") << QStringLiteral("Test")
                                         << QStringLiteral("Statistic") << QStringLiteral("p_raw")
                                         << QStringLiteral("p_adj")
                                         << QStringLiteral("Significance"));
            rep->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
            rep->column(1)->setColumnMode(SciDAVis::ColumnMode::Text);
            rep->column(2)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            rep->column(3)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            rep->column(4)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            rep->column(5)->setColumnMode(SciDAVis::ColumnMode::Text);

            rep->column(0)->setTextAt(0, QStringLiteral("Omnibus (%1 Groups)").arg(K));
            rep->column(1)->setTextAt(0, QStringLiteral("Kruskal-Wallis H"));
            rep->column(2)->setValueAt(0, hStat);
            rep->column(3)->setValueAt(0, pKw);
            rep->column(4)->setValueAt(0, pKw);
            rep->column(5)->setTextAt(0, starRatingForP(pKw));

            for (size_t m = 0; m < M; ++m) {
                const int r = static_cast<int>(m) + 1;
                rep->column(0)->setTextAt(r, pairs[m].label);
                rep->column(1)->setTextAt(r, QStringLiteral("Dunn (Holm)"));
                rep->column(2)->setValueAt(r, pairs[m].zStat);
                rep->column(3)->setValueAt(r, pairs[m].pRaw);
                rep->column(4)->setValueAt(r, pairs[m].pAdj);
                rep->column(5)->setTextAt(r, starRatingForP(pairs[m].pAdj));
            }
            res.reportTable = rep;
        }
    }

    if (annotateGraph && !pairs.empty()) {
        QStringList groupLabels;
        groupLabels.reserve(K);
        for (const GroupSample &gs : groups)
            groupLabels << gs.name;

        const QVariant showNsProp = srcTable->property("torigraph_anova_show_ns");
        const bool showNsExplicit = showNsProp.isValid() && !showNsProp.isNull();
        const bool showNs = showNsExplicit ? showNsProp.toBool() : false;

        std::vector<std::tuple<int, int, double>> sigToDraw;
        if (showNsExplicit) {
            for (const DunnPair &dp : pairs) {
                if (showNs || dp.pAdj < 0.05)
                    sigToDraw.emplace_back(dp.i, dp.j, dp.pAdj);
            }
        } else if (K == 2) {
            sigToDraw.emplace_back(pairs[0].i, pairs[0].j, pairs[0].pAdj);
        } else {
            for (const DunnPair &dp : pairs) {
                if (dp.pAdj < 0.05)
                    sigToDraw.emplace_back(dp.i, dp.j, dp.pAdj);
            }
            if (sigToDraw.empty())
                sigToDraw.emplace_back(pairs[0].i, pairs[0].j, pairs[0].pAdj);
        }
        if (!sigToDraw.empty()) {
            annotateBracketsOnGraph(annotateGraph, K, yMinAll, yMaxAll, sigToDraw, groupLabels,
                                    srcTable, parseBracketLabelFormat(srcTable));
        }
    }

    return res;
}

namespace {

struct SplitMix64Rng {
    uint64_t state = 0x9E3779B97F4A7C15ULL;
    explicit SplitMix64Rng(uint64_t seed = 0x9E3779B97F4A7C15ULL) : state(seed) {}
    uint64_t nextU64()
    {
        uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double nextUnit()
    {
        return static_cast<double>(nextU64() >> 11) * (1.0 / 9007199254740992.0);
    }
};

} // namespace

MultiLayer *GraphStatsAndSurfaceFit::plotPairedComparison(
        ApplicationWindow *app, Table *srcTable, const QString &beforeCol, const QString &afterCol,
        bool useWilcoxon, double *pValueOut)
{
    registerPhase5StatsTranslationsOnce();
    if (pValueOut)
        *pValueOut = 1.0;
    if (!srcTable)
        return nullptr;

    Column *cBefore = resolveTableColumn(srcTable, beforeCol);
    Column *cAfter = resolveTableColumn(srcTable, afterCol);
    if (!cBefore || !cAfter)
        return nullptr;

    std::vector<double> bef, aft, diffs;
    const int nRows = std::min(cBefore->rowCount(), cAfter->rowCount());
    for (int r = 0; r < nRows; ++r) {
        double vb = 0.0, va = 0.0;
        if (readNumericColumnCell(cBefore, r, vb) && readNumericColumnCell(cAfter, r, va)) {
            bef.push_back(vb);
            aft.push_back(va);
            diffs.push_back(va - vb);
        }
    }

    const int m = static_cast<int>(diffs.size());
    if (m < 2)
        return nullptr;

    double pVal = 1.0;
    double testStat = 0.0;
    if (useWilcoxon) {
        evalWilcoxonSignedRank(diffs, testStat, pVal);
    } else {
        const double meanD = std::accumulate(diffs.begin(), diffs.end(), 0.0) / static_cast<double>(m);
        double ssD = 0.0;
        for (double d : diffs)
            ssD += (d - meanD) * (d - meanD);
        const double sdD = std::sqrt(ssD / static_cast<double>(m - 1));
        const double seD = sdD / std::sqrt(static_cast<double>(m));
        if (seD > 1e-30) {
            testStat = meanD / seD;
            pVal = std::clamp(2.0 * gsl_cdf_tdist_Q(std::fabs(testStat), static_cast<double>(m - 1)),
                              0.0, 1.0);
        } else if (std::fabs(meanD) > 1e-30) {
            testStat = (meanD > 0.0) ? 1e6 : -1e6;
            pVal = 0.0;
        }
    }
    if (pValueOut)
        *pValueOut = pVal;
    if (!app)
        return nullptr;

    const double meanB = std::accumulate(bef.begin(), bef.end(), 0.0) / static_cast<double>(m);
    const double meanA = std::accumulate(aft.begin(), aft.end(), 0.0) / static_cast<double>(m);
    double ssB = 0.0, ssA = 0.0;
    double yMin = std::min(bef.front(), aft.front());
    double yMax = std::max(bef.front(), aft.front());
    for (int i = 0; i < m; ++i) {
        ssB += (bef[static_cast<size_t>(i)] - meanB) * (bef[static_cast<size_t>(i)] - meanB);
        ssA += (aft[static_cast<size_t>(i)] - meanA) * (aft[static_cast<size_t>(i)] - meanA);
        yMin = std::min({ yMin, bef[static_cast<size_t>(i)], aft[static_cast<size_t>(i)] });
        yMax = std::max({ yMax, bef[static_cast<size_t>(i)], aft[static_cast<size_t>(i)] });
    }
    const double seB = std::sqrt(ssB / (static_cast<double>(m) * static_cast<double>(m - 1)));
    const double seA = std::sqrt(ssA / (static_cast<double>(m) * static_cast<double>(m - 1)));

    const QString tblName = app->generateUniqueName(
            srcTable->name() + QStringLiteral("_PairedData"), false);
    Table *tbl = app->newTable(std::max(m, 2), 8, tblName,
                               QObject::tr("Paired Comparison Plot (Before-After)"));
    if (!tbl)
        return nullptr;

    tbl->setHeader(QStringList() << QStringLiteral("X_Before") << QStringLiteral("Before")
                                 << QStringLiteral("X_After") << QStringLiteral("After")
                                 << QStringLiteral("SummaryX") << QStringLiteral("MeanY")
                                 << QStringLiteral("SE") << QStringLiteral("GroupLabel"));
    for (int c = 0; c < 7; ++c)
        tbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    tbl->column(7)->setColumnMode(SciDAVis::ColumnMode::Text);
    tbl->column(0)->setPlotDesignation(SciDAVis::X);
    tbl->column(1)->setPlotDesignation(SciDAVis::Y);
    tbl->column(2)->setPlotDesignation(SciDAVis::X);
    tbl->column(3)->setPlotDesignation(SciDAVis::Y);
    tbl->column(4)->setPlotDesignation(SciDAVis::X);
    tbl->column(5)->setPlotDesignation(SciDAVis::Y);
    tbl->column(6)->setPlotDesignation(SciDAVis::yErr);

    for (int i = 0; i < m; ++i) {
        tbl->column(0)->setValueAt(i, 1.0);
        tbl->column(1)->setValueAt(i, bef[static_cast<size_t>(i)]);
        tbl->column(2)->setValueAt(i, 2.0);
        tbl->column(3)->setValueAt(i, aft[static_cast<size_t>(i)]);
    }
    tbl->column(4)->setValueAt(0, 0.86);
    tbl->column(5)->setValueAt(0, meanB);
    tbl->column(6)->setValueAt(0, seB);
    tbl->column(7)->setTextAt(0, cBefore->name());

    tbl->column(4)->setValueAt(1, 2.14);
    tbl->column(5)->setValueAt(1, meanA);
    tbl->column(6)->setValueAt(1, seA);
    tbl->column(7)->setTextAt(1, cAfter->name());
    for (int r = 2; r < tbl->numRows(); ++r) {
        tbl->column(4)->setInvalid(r, true);
        tbl->column(5)->setInvalid(r, true);
        tbl->column(6)->setInvalid(r, true);
        tbl->column(7)->setInvalid(r, true);
    }

    MultiLayer *ml = app->newGraph(
            app->generateUniqueName(QStringLiteral("PairedPlot_") + srcTable->name(), false));
    if (!ml)
        return nullptr;

    if (Graph *g = ml->activeGraph()) {
        for (int i = 0; i < m; ++i) {
            addLineSegmentOnGraph(g, 1.0, bef[static_cast<size_t>(i)], 2.0,
                                  aft[static_cast<size_t>(i)], QColor(125, 140, 165, 165), 1);
        }
        if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Scatter, 0, m - 1)) {
            const int idx = g->curves() - 1;
            g->setCurveSymbol(idx,
                              QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(35, 100, 190)),
                                        QPen(QColor(15, 50, 110), 1), QSize(7, 7)));
        }
        if (g->insertCurve(tbl, tbl->colName(2), tbl->colName(3), Graph::Scatter, 0, m - 1)) {
            const int idx = g->curves() - 1;
            g->setCurveSymbol(idx,
                              QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(210, 50, 50)),
                                        QPen(QColor(125, 20, 20), 1), QSize(7, 7)));
        }
        if (g->insertCurve(tbl, tbl->colName(4), tbl->colName(5), Graph::LineSymbols, 0, 1)) {
            const int idx = g->curves() - 1;
            g->setCurvePen(idx, QPen(QColor(20, 25, 35), 2.2, Qt::SolidLine));
            g->setCurveSymbol(idx,
                              QwtSymbol(QwtSymbol::Diamond, QBrush(QColor(20, 25, 35)),
                                        QPen(QColor(20, 25, 35), 1), QSize(10, 10)));
            g->addErrorBars(tbl->colName(4), tbl->colName(5), tbl, tbl->colName(6), 1, 2, 7,
                            QColor(20, 25, 35), true, true, true);
        }

        const double ySpan = std::max(1e-6, yMax - yMin);
        const double yBar = yMax + 0.12 * ySpan;
        const double tickH = 0.035 * ySpan;
        addLineSegmentOnGraph(g, 1.0, yBar - tickH, 1.0, yBar, QColor(25, 25, 25), 2);
        addLineSegmentOnGraph(g, 1.0, yBar, 2.0, yBar, QColor(25, 25, 25), 2);
        addLineSegmentOnGraph(g, 2.0, yBar, 2.0, yBar - tickH, QColor(25, 25, 25), 2);
        const QString bracketTxt = QStringLiteral("%1 (%2, p = %3)")
                .arg(starRatingForP(pVal))
                .arg(useWilcoxon ? QStringLiteral("Wilcoxon") : QStringLiteral("Paired t"))
                .arg(pVal, 0, 'g', 4);
        addTextLabelOnGraph(g, 1.5, yBar + 0.06 * ySpan, bracketTxt, QColor(25, 25, 25), true, 0);

        g->setLabelsTextFormat(Graph::Bottom, tbl->column(7), 0, 1);
        g->setScale(QwtPlot::xBottom, 0.55, 2.45, 1.0);
        g->setScale(QwtPlot::yLeft, yMin - 0.12 * ySpan, yMax + 0.28 * ySpan);
        g->setXAxisTitle(QString());
        g->setYAxisTitle(QObject::tr("Value"));
        g->setTitle(QObject::tr("Paired Comparison Plot (Before-After)"));
        if (g->legend())
            g->legend()->setText(g->legendText());
        applyArialGraphFonts(g);
        g->replot();
    }
    return ml;
}

MultiLayer *GraphStatsAndSurfaceFit::plotCorrelationMatrixHeatmap(
        ApplicationWindow *app, Table *srcTable, const QStringList &colNames, bool spearmanRank,
        Matrix **corrMatrixOut)
{
    registerPhase5StatsTranslationsOnce();
    if (corrMatrixOut)
        *corrMatrixOut = nullptr;
    if (!app || !srcTable)
        return nullptr;

    std::vector<Column *> cols;
    QStringList namesToUse = colNames;
    if (namesToUse.isEmpty()) {
        for (int c = 0; c < srcTable->numCols(); ++c) {
            if (columnHasNumericValues(srcTable->column(c)))
                namesToUse << srcTable->column(c)->name();
        }
    }
    for (const QString &cn : namesToUse) {
        if (Column *c = resolveTableColumn(srcTable, cn))
            cols.push_back(c);
    }
    const int P = static_cast<int>(cols.size());
    if (P < 2)
        return nullptr;

    std::vector<double> rMat(static_cast<size_t>(P * P), 0.0);
    std::vector<double> pMat(static_cast<size_t>(P * P), 1.0);
    for (int d = 0; d < P; ++d) {
        rMat[static_cast<size_t>(d * P + d)] = 1.0;
        pMat[static_cast<size_t>(d * P + d)] = 0.0;
    }

    for (int i = 0; i < P; ++i) {
        for (int j = i + 1; j < P; ++j) {
            Column *ci = cols[static_cast<size_t>(i)];
            Column *cj = cols[static_cast<size_t>(j)];
            const int nRows = std::min(ci->rowCount(), cj->rowCount());
            std::vector<double> xi, xj;
            for (int r = 0; r < nRows; ++r) {
                double vi = 0.0, vj = 0.0;
                if (readNumericColumnCell(ci, r, vi) && readNumericColumnCell(cj, r, vj)) {
                    xi.push_back(vi);
                    xj.push_back(vj);
                }
            }

            double rVal = 0.0;
            double pVal = 1.0;
            if (xi.size() >= 2) {
                if (spearmanRank) {
                    std::vector<double> ri, rj;
                    double t1 = 0.0, t2 = 0.0;
                    computeMidRanksWithTies(xi, ri, t1);
                    computeMidRanksWithTies(xj, rj, t2);
                    xi.swap(ri);
                    xj.swap(rj);
                }
                const double m = static_cast<double>(xi.size());
                const double mi = std::accumulate(xi.begin(), xi.end(), 0.0) / m;
                const double mj = std::accumulate(xj.begin(), xj.end(), 0.0) / m;
                double sii = 0.0, sjj = 0.0, sij = 0.0;
                for (size_t k = 0; k < xi.size(); ++k) {
                    const double di = xi[k] - mi;
                    const double dj = xj[k] - mj;
                    sii += di * di;
                    sjj += dj * dj;
                    sij += di * dj;
                }
                if (sii > 1e-30 && sjj > 1e-30) {
                    rVal = std::clamp(sij / std::sqrt(sii * sjj), -1.0, 1.0);
                    if (xi.size() > 2) {
                        const double oneMinusR2 = std::max(0.0, 1.0 - rVal * rVal);
                        if (oneMinusR2 <= 1e-15) {
                            pVal = 0.0;
                        } else {
                            const double tStat = std::fabs(rVal)
                                    * std::sqrt((m - 2.0) / oneMinusR2);
                            pVal = std::clamp(2.0 * gsl_cdf_tdist_Q(tStat, m - 2.0), 0.0, 1.0);
                        }
                    }
                }
            }
            rMat[static_cast<size_t>(i * P + j)] = rVal;
            rMat[static_cast<size_t>(j * P + i)] = rVal;
            pMat[static_cast<size_t>(i * P + j)] = pVal;
            pMat[static_cast<size_t>(j * P + i)] = pVal;
        }
    }

    const QString matName = app->generateUniqueName(
            srcTable->name() + (spearmanRank ? QStringLiteral("_SpearmanCorr")
                                             : QStringLiteral("_PearsonCorr")),
            false);
    Matrix *corrMat = app->newMatrix(matName, P, P);
    if (!corrMat)
        return nullptr;

    // Cell-aligned [0.5, P + 0.5] x [0.5, P + 0.5] bounds so standard Spectrogram / MatrixData
    // places cell (i, j) on [j + 0.5, j + 1.5] x [i + 0.5, i + 1.5] centered at integer tick
    // (j + 1, i + 1) identically both before and after .sciprj reload.
    corrMat->setCoordinates(0.5, static_cast<double>(P) + 0.5, 0.5, static_cast<double>(P) + 0.5);
    for (int i = 0; i < P; ++i) {
        for (int j = 0; j < P; ++j)
            corrMat->setCell(i, j, rMat[static_cast<size_t>(i * P + j)]);
    }
    if (corrMatrixOut)
        *corrMatrixOut = corrMat;

    MultiLayer *ml = app->plotSpectrogram(corrMat, Graph::ColorMap);
    if (!ml)
        return nullptr;

    if (Graph *g = ml->activeGraph()) {
        auto divergingColorForR = [](double rv) -> QColor {
            const double c = std::clamp(rv, -1.0, 1.0);
            const QColor negCol(33, 102, 172);
            const QColor zeroCol(247, 247, 247);
            const QColor posCol(178, 24, 43);
            if (c <= 0.0) {
                const double t = c + 1.0;
                return QColor(qRound(negCol.red() + t * (zeroCol.red() - negCol.red())),
                              qRound(negCol.green() + t * (zeroCol.green() - negCol.green())),
                              qRound(negCol.blue() + t * (zeroCol.blue() - negCol.blue())));
            }
            const double t = c;
            return QColor(qRound(zeroCol.red() + t * (posCol.red() - zeroCol.red())),
                          qRound(zeroCol.green() + t * (posCol.green() - zeroCol.green())),
                          qRound(zeroCol.blue() + t * (posCol.blue() - zeroCol.blue())));
        };

        if (g->plotWidget()) {
            const QwtPlotItemList items = g->plotWidget()->itemList();
            for (QwtPlotItem *it : items) {
                if (auto *sp = dynamic_cast<Spectrogram *>(it)) {
                    const QwtDoubleInterval zRange = sp->data().range();
                    const double zMin = zRange.isValid() ? zRange.minValue() : -1.0;
                    const double zMax = zRange.isValid() ? zRange.maxValue() : 1.0;
                    QwtLinearColorMap cmap(divergingColorForR(zMin), divergingColorForR(zMax));
                    if (zMin < -1e-9 && zMax > 1e-9) {
                        cmap.addColorStop((0.0 - zMin) / (zMax - zMin), QColor(247, 247, 247));
                    }
                    sp->setCustomColorMap(cmap);
                    sp->showColorScale(QwtPlot::yRight, true);
                    break;
                }
            }
        }

        Column *colLblX = new Column(QStringLiteral("VarX"), SciDAVis::ColumnMode::Text);
        Column *colLblY = new Column(QStringLiteral("VarY"), SciDAVis::ColumnMode::Text);
        for (int k = 0; k < P; ++k) {
            colLblX->setTextAt(k, cols[static_cast<size_t>(k)]->name());
            colLblY->setTextAt(k, cols[static_cast<size_t>(k)]->name());
        }
        const QString hidName = app->generateUniqueName(QStringLiteral("CorrAxisLabels"));
        Table *lblTbl = app->newHiddenTable(hidName, QStringLiteral("Correlation Labels"),
                                            QList<Column *>() << colLblX << colLblY);
        if (lblTbl) {
            g->setLabelsTextFormat(Graph::Bottom, colLblX, 0, P - 1);
            g->setLabelsTextFormat(Graph::Left, colLblY, 0, P - 1);
        }

        g->setScale(QwtPlot::xBottom, 0.5, static_cast<double>(P) + 0.5, 1.0);
        g->setScale(QwtPlot::yLeft, 0.5, static_cast<double>(P) + 0.5, 1.0);
        g->setXAxisTitle(QString());
        g->setYAxisTitle(QString());
        g->setTitle(spearmanRank ? QObject::tr("Correlation Matrix Heatmap (Spearman ρ)")
                                 : QObject::tr("Correlation Matrix Heatmap (Pearson r)"));

        for (int i = 0; i < P; ++i) {
            for (int j = 0; j < P; ++j) {
                const double rv = rMat[static_cast<size_t>(i * P + j)];
                const double pv = pMat[static_cast<size_t>(i * P + j)];
                QString cellTxt = QString::number(rv, 'f', 2);
                if (i != j && pv < 0.05)
                    cellTxt += QStringLiteral("\n") + starRatingForP(pv);
                const QColor txtCol = (std::fabs(rv) > 0.62) ? QColor(255, 255, 255)
                                                             : QColor(20, 25, 35);
                addTextLabelOnGraph(g, static_cast<double>(j) + 1.0,
                                    static_cast<double>(i) + 1.0, cellTxt, txtCol, true,
                                    (P > 6) ? -2 : -1);
            }
        }
        applyArialGraphFonts(g);
        g->replot();
    }
    return ml;
}

GraphStatsAndSurfaceFit::KMeansResult GraphStatsAndSurfaceFit::runKMeansClustering(
        ApplicationWindow *app, Table *srcTable, const QStringList &colNames, int k,
        bool plotClusters)
{
    registerPhase5StatsTranslationsOnce();
    KMeansResult res;
    if (!srcTable)
        return res;

    std::vector<Column *> cols;
    QStringList namesToUse = colNames;
    if (namesToUse.isEmpty()) {
        for (int c = 0; c < srcTable->numCols(); ++c) {
            if (columnHasNumericValues(srcTable->column(c)))
                namesToUse << srcTable->column(c)->name();
        }
    }
    for (const QString &cn : namesToUse) {
        if (Column *c = resolveTableColumn(srcTable, cn))
            cols.push_back(c);
    }
    const int D = static_cast<int>(cols.size());
    if (D < 1)
        return res;

    int maxRows = cols[0]->rowCount();
    for (Column *c : cols)
        maxRows = std::min(maxRows, c->rowCount());

    std::vector<int> validRows;
    std::vector<std::vector<double>> rawX;
    for (int r = 0; r < maxRows; ++r) {
        bool okRow = true;
        std::vector<double> pt(static_cast<size_t>(D), 0.0);
        for (int d = 0; d < D; ++d) {
            Column *cd = cols[static_cast<size_t>(d)];
            double v = 0.0;
            if (!readNumericColumnCell(cd, r, v)) {
                okRow = false;
                break;
            }
            pt[static_cast<size_t>(d)] = v;
        }
        if (okRow) {
            validRows.push_back(r);
            rawX.push_back(std::move(pt));
        }
    }

    const int N = static_cast<int>(rawX.size());
    if (N < 3)
        return res;

    const int K = std::clamp(k, 2, N - 1);
    res.k = K;

    // Standardize features
    std::vector<double> featMean(static_cast<size_t>(D), 0.0);
    std::vector<double> featSd(static_cast<size_t>(D), 1.0);
    for (int d = 0; d < D; ++d) {
        double s = 0.0;
        for (int i = 0; i < N; ++i)
            s += rawX[static_cast<size_t>(i)][static_cast<size_t>(d)];
        featMean[static_cast<size_t>(d)] = s / static_cast<double>(N);
        double ss = 0.0;
        for (int i = 0; i < N; ++i) {
            const double dev =
                    rawX[static_cast<size_t>(i)][static_cast<size_t>(d)] - featMean[static_cast<size_t>(d)];
            ss += dev * dev;
        }
        const double sd = std::sqrt(ss / static_cast<double>(N - 1));
        featSd[static_cast<size_t>(d)] = (sd > 1e-15) ? sd : 1.0;
    }

    std::vector<std::vector<double>> Z(static_cast<size_t>(N),
                                       std::vector<double>(static_cast<size_t>(D), 0.0));
    for (int i = 0; i < N; ++i) {
        for (int d = 0; d < D; ++d) {
            Z[static_cast<size_t>(i)][static_cast<size_t>(d)] =
                    (rawX[static_cast<size_t>(i)][static_cast<size_t>(d)]
                     - featMean[static_cast<size_t>(d)])
                    / featSd[static_cast<size_t>(d)];
        }
    }

    auto distSq = [&](const std::vector<double> &a, const std::vector<double> &b) {
        double acc = 0.0;
        for (int d = 0; d < D; ++d) {
            const double diff = a[static_cast<size_t>(d)] - b[static_cast<size_t>(d)];
            acc += diff * diff;
        }
        return acc;
    };

    // Deterministic multi-start K-Means++
    SplitMix64Rng rng(0x9E3779B97F4A7C15ULL);
    double bestWcss = std::numeric_limits<double>::max();
    std::vector<int> bestAssign(static_cast<size_t>(N), 0);
    std::vector<std::vector<double>> bestCenters(
            static_cast<size_t>(K), std::vector<double>(static_cast<size_t>(D), 0.0));

    constexpr int kRestarts = 12;
    for (int restart = 0; restart < kRestarts; ++restart) {
        std::vector<std::vector<double>> centers(
                static_cast<size_t>(K), std::vector<double>(static_cast<size_t>(D), 0.0));
        const int firstIdx = static_cast<int>(rng.nextU64() % static_cast<uint64_t>(N));
        centers[0] = Z[static_cast<size_t>(firstIdx)];

        std::vector<double> minD2(static_cast<size_t>(N), std::numeric_limits<double>::max());
        for (int c = 1; c < K; ++c) {
            double sumD2 = 0.0;
            for (int i = 0; i < N; ++i) {
                minD2[static_cast<size_t>(i)] =
                        std::min(minD2[static_cast<size_t>(i)],
                                 distSq(Z[static_cast<size_t>(i)], centers[static_cast<size_t>(c - 1)]));
                sumD2 += minD2[static_cast<size_t>(i)];
            }
            int chosen = 0;
            if (sumD2 > 1e-30) {
                const double target = rng.nextUnit() * sumD2;
                double cum = 0.0;
                chosen = N - 1;
                for (int i = 0; i < N; ++i) {
                    cum += minD2[static_cast<size_t>(i)];
                    if (cum >= target) {
                        chosen = i;
                        break;
                    }
                }
            } else {
                chosen = c % N;
            }
            centers[static_cast<size_t>(c)] = Z[static_cast<size_t>(chosen)];
        }

        std::vector<int> assign(static_cast<size_t>(N), -1);
        for (int iter = 0; iter < 200; ++iter) {
            bool changed = false;
            for (int i = 0; i < N; ++i) {
                int bestC = 0;
                double bestD = distSq(Z[static_cast<size_t>(i)], centers[0]);
                for (int c = 1; c < K; ++c) {
                    const double d2 = distSq(Z[static_cast<size_t>(i)], centers[static_cast<size_t>(c)]);
                    if (d2 < bestD) {
                        bestD = d2;
                        bestC = c;
                    }
                }
                if (assign[static_cast<size_t>(i)] != bestC) {
                    assign[static_cast<size_t>(i)] = bestC;
                    changed = true;
                }
            }
            if (!changed && iter > 0)
                break;

            std::vector<std::vector<double>> newCenters(
                    static_cast<size_t>(K), std::vector<double>(static_cast<size_t>(D), 0.0));
            std::vector<int> counts(static_cast<size_t>(K), 0);
            for (int i = 0; i < N; ++i) {
                const int c = assign[static_cast<size_t>(i)];
                counts[static_cast<size_t>(c)]++;
                for (int d = 0; d < D; ++d)
                    newCenters[static_cast<size_t>(c)][static_cast<size_t>(d)] +=
                            Z[static_cast<size_t>(i)][static_cast<size_t>(d)];
            }
            for (int c = 0; c < K; ++c) {
                if (counts[static_cast<size_t>(c)] > 0) {
                    for (int d = 0; d < D; ++d)
                        newCenters[static_cast<size_t>(c)][static_cast<size_t>(d)] /=
                                static_cast<double>(counts[static_cast<size_t>(c)]);
                } else {
                    // Rescue empty cluster with furthest sample
                    int farIdx = 0;
                    double farD = -1.0;
                    for (int i = 0; i < N; ++i) {
                        const double d2 = distSq(
                                Z[static_cast<size_t>(i)],
                                centers[static_cast<size_t>(assign[static_cast<size_t>(i)])]);
                        if (d2 > farD) {
                            farD = d2;
                            farIdx = i;
                        }
                    }
                    newCenters[static_cast<size_t>(c)] = Z[static_cast<size_t>(farIdx)];
                }
            }
            centers.swap(newCenters);
        }

        double wcss = 0.0;
        for (int i = 0; i < N; ++i)
            wcss += distSq(Z[static_cast<size_t>(i)],
                           centers[static_cast<size_t>(assign[static_cast<size_t>(i)])]);
        if (wcss < bestWcss) {
            bestWcss = wcss;
            bestAssign = assign;
            bestCenters = centers;
        }
    }

    // Canonicalize cluster labels by first coordinate of centroid
    std::vector<int> clusterOrder(static_cast<size_t>(K));
    std::iota(clusterOrder.begin(), clusterOrder.end(), 0);
    std::sort(clusterOrder.begin(), clusterOrder.end(),
              [&](int a, int b) { return bestCenters[static_cast<size_t>(a)][0] < bestCenters[static_cast<size_t>(b)][0]; });
    std::vector<int> remap(static_cast<size_t>(K), 0);
    std::vector<std::vector<double>> orderedCenters(static_cast<size_t>(K));
    for (int newId = 0; newId < K; ++newId) {
        const int oldId = clusterOrder[static_cast<size_t>(newId)];
        remap[static_cast<size_t>(oldId)] = newId;
        orderedCenters[static_cast<size_t>(newId)] = bestCenters[static_cast<size_t>(oldId)];
    }
    for (int &cid : bestAssign)
        cid = remap[static_cast<size_t>(cid)];

    // Exact Silhouette score on standardized Euclidean distances
    std::vector<double> silPoint(static_cast<size_t>(N), 0.0);
    std::vector<int> cSizes(static_cast<size_t>(K), 0);
    for (int cid : bestAssign)
        cSizes[static_cast<size_t>(cid)]++;

    for (int i = 0; i < N; ++i) {
        const int ci = bestAssign[static_cast<size_t>(i)];
        if (cSizes[static_cast<size_t>(ci)] <= 1) {
            silPoint[static_cast<size_t>(i)] = 0.0;
            continue;
        }
        std::vector<double> sumDistToC(static_cast<size_t>(K), 0.0);
        for (int j = 0; j < N; ++j) {
            if (i == j)
                continue;
            const double d = std::sqrt(distSq(Z[static_cast<size_t>(i)], Z[static_cast<size_t>(j)]));
            sumDistToC[static_cast<size_t>(bestAssign[static_cast<size_t>(j)])] += d;
        }
        const double ai = sumDistToC[static_cast<size_t>(ci)]
                / static_cast<double>(cSizes[static_cast<size_t>(ci)] - 1);
        double bi = std::numeric_limits<double>::max();
        for (int c = 0; c < K; ++c) {
            if (c != ci && cSizes[static_cast<size_t>(c)] > 0) {
                bi = std::min(bi,
                              sumDistToC[static_cast<size_t>(c)]
                                      / static_cast<double>(cSizes[static_cast<size_t>(c)]));
            }
        }
        if (bi == std::numeric_limits<double>::max()) {
            silPoint[static_cast<size_t>(i)] = 0.0;
        } else {
            const double denom = std::max(ai, bi);
            silPoint[static_cast<size_t>(i)] = (denom > 1e-30) ? ((bi - ai) / denom) : 0.0;
        }
    }

    res.valid = true;
    res.silhouetteScore = std::accumulate(silPoint.begin(), silPoint.end(), 0.0)
            / static_cast<double>(N);

    const int assignLen = validRows.back() + 1;
    res.assignments.fill(-1, assignLen);
    for (int i = 0; i < N; ++i)
        res.assignments[validRows[static_cast<size_t>(i)]] = bestAssign[static_cast<size_t>(i)];

    if (!app)
        return res;

    const QString tblName = app->generateUniqueName(
            srcTable->name() + QStringLiteral("_KMeans"), false);
    const int nOutCols = D + 2;
    Table *resTbl = app->newTable(N, nOutCols, tblName, QObject::tr("K-Means Clustering"));
    res.resultTable = resTbl;
    if (resTbl) {
        QStringList hdr;
        for (int d = 0; d < D; ++d)
            hdr << cols[static_cast<size_t>(d)]->name();
        hdr << QStringLiteral("Cluster") << QStringLiteral("Silhouette");
        resTbl->setHeader(hdr);
        for (int c = 0; c < nOutCols; ++c)
            resTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        resTbl->column(0)->setPlotDesignation(SciDAVis::X);
        if (D >= 2)
            resTbl->column(1)->setPlotDesignation(SciDAVis::Y);

        for (int i = 0; i < N; ++i) {
            for (int d = 0; d < D; ++d)
                resTbl->column(d)->setValueAt(
                        i, rawX[static_cast<size_t>(i)][static_cast<size_t>(d)]);
            resTbl->column(D)->setValueAt(
                    i, static_cast<double>(bestAssign[static_cast<size_t>(i)] + 1));
            resTbl->column(D + 1)->setValueAt(i, silPoint[static_cast<size_t>(i)]);
        }
    }

    if (plotClusters) {
        // Compute 2D coordinates (original X1,X2 for D==2; PCA PC1,PC2 for D>2)
        std::vector<double> pX(static_cast<size_t>(N), 0.0);
        std::vector<double> pY(static_cast<size_t>(N), 0.0);
        std::vector<double> cX(static_cast<size_t>(K), 0.0);
        std::vector<double> cY(static_cast<size_t>(K), 0.0);
        QString xAxisLbl = cols[0]->name();
        QString yAxisLbl = (D >= 2) ? cols[1]->name() : QStringLiteral("Index");

        if (D == 1) {
            for (int i = 0; i < N; ++i) {
                pX[static_cast<size_t>(i)] = rawX[static_cast<size_t>(i)][0];
                pY[static_cast<size_t>(i)] = static_cast<double>(i + 1);
            }
            for (int c = 0; c < K; ++c) {
                cX[static_cast<size_t>(c)] =
                        featMean[0] + orderedCenters[static_cast<size_t>(c)][0] * featSd[0];
                cY[static_cast<size_t>(c)] = 0.5 * static_cast<double>(N + 1);
            }
        } else if (D == 2) {
            for (int i = 0; i < N; ++i) {
                pX[static_cast<size_t>(i)] = rawX[static_cast<size_t>(i)][0];
                pY[static_cast<size_t>(i)] = rawX[static_cast<size_t>(i)][1];
            }
            for (int c = 0; c < K; ++c) {
                cX[static_cast<size_t>(c)] =
                        featMean[0] + orderedCenters[static_cast<size_t>(c)][0] * featSd[0];
                cY[static_cast<size_t>(c)] =
                        featMean[1] + orderedCenters[static_cast<size_t>(c)][1] * featSd[1];
            }
        } else {
            gsl_matrix *covM = gsl_matrix_calloc(static_cast<size_t>(D), static_cast<size_t>(D));
            for (int a = 0; a < D; ++a) {
                for (int b = a; b < D; ++b) {
                    double s = 0.0;
                    for (int i = 0; i < N; ++i)
                        s += Z[static_cast<size_t>(i)][static_cast<size_t>(a)]
                                * Z[static_cast<size_t>(i)][static_cast<size_t>(b)];
                    const double cv = s / static_cast<double>(N - 1);
                    gsl_matrix_set(covM, static_cast<size_t>(a), static_cast<size_t>(b), cv);
                    gsl_matrix_set(covM, static_cast<size_t>(b), static_cast<size_t>(a), cv);
                }
            }
            gsl_vector *eval = gsl_vector_alloc(static_cast<size_t>(D));
            gsl_matrix *evec = gsl_matrix_alloc(static_cast<size_t>(D), static_cast<size_t>(D));
            gsl_eigen_symmv_workspace *ws = gsl_eigen_symmv_alloc(static_cast<size_t>(D));
            if (covM && eval && evec && ws && gsl_eigen_symmv(covM, eval, evec, ws) == 0) {
                gsl_eigen_symmv_sort(eval, evec, GSL_EIGEN_SORT_VAL_DESC);
                for (int i = 0; i < N; ++i) {
                    double u1 = 0.0, u2 = 0.0;
                    for (int d = 0; d < D; ++d) {
                        u1 += Z[static_cast<size_t>(i)][static_cast<size_t>(d)]
                                * gsl_matrix_get(evec, static_cast<size_t>(d), 0);
                        u2 += Z[static_cast<size_t>(i)][static_cast<size_t>(d)]
                                * gsl_matrix_get(evec, static_cast<size_t>(d), 1);
                    }
                    pX[static_cast<size_t>(i)] = u1;
                    pY[static_cast<size_t>(i)] = u2;
                }
                for (int c = 0; c < K; ++c) {
                    double u1 = 0.0, u2 = 0.0;
                    for (int d = 0; d < D; ++d) {
                        u1 += orderedCenters[static_cast<size_t>(c)][static_cast<size_t>(d)]
                                * gsl_matrix_get(evec, static_cast<size_t>(d), 0);
                        u2 += orderedCenters[static_cast<size_t>(c)][static_cast<size_t>(d)]
                                * gsl_matrix_get(evec, static_cast<size_t>(d), 1);
                    }
                    cX[static_cast<size_t>(c)] = u1;
                    cY[static_cast<size_t>(c)] = u2;
                }
                xAxisLbl = QStringLiteral("PC1");
                yAxisLbl = QStringLiteral("PC2");
            }
            if (ws)
                gsl_eigen_symmv_free(ws);
            if (evec)
                gsl_matrix_free(evec);
            if (eval)
                gsl_vector_free(eval);
            if (covM)
                gsl_matrix_free(covM);
        }

        // Hidden plot table with 2*K cluster columns + 2 centroid columns
        QList<Column *> plotCols;
        std::vector<int> clusRows(static_cast<size_t>(K), 0);
        for (int c = 0; c < K; ++c) {
            Column *cx = new Column(QStringLiteral("C%1_X").arg(c + 1),
                                    SciDAVis::ColumnMode::Numeric);
            Column *cy = new Column(QStringLiteral("Cluster_%1").arg(c + 1),
                                    SciDAVis::ColumnMode::Numeric);
            cx->setPlotDesignation(SciDAVis::X);
            cy->setPlotDesignation(SciDAVis::Y);
            int rIdx = 0;
            for (int i = 0; i < N; ++i) {
                if (bestAssign[static_cast<size_t>(i)] == c) {
                    cx->setValueAt(rIdx, pX[static_cast<size_t>(i)]);
                    cy->setValueAt(rIdx, pY[static_cast<size_t>(i)]);
                    ++rIdx;
                }
            }
            clusRows[static_cast<size_t>(c)] = rIdx;
            plotCols << cx << cy;
        }
        Column *centColX = new Column(QStringLiteral("Centroid_X"), SciDAVis::ColumnMode::Numeric);
        Column *centColY = new Column(QStringLiteral("Centroids"), SciDAVis::ColumnMode::Numeric);
        centColX->setPlotDesignation(SciDAVis::X);
        centColY->setPlotDesignation(SciDAVis::Y);
        for (int c = 0; c < K; ++c) {
            centColX->setValueAt(c, cX[static_cast<size_t>(c)]);
            centColY->setValueAt(c, cY[static_cast<size_t>(c)]);
        }
        plotCols << centColX << centColY;

        const QString hidName = app->generateUniqueName(QStringLiteral("KMeansPlotData"));
        Table *plotTbl = app->newHiddenTable(hidName, QStringLiteral("K-Means Plot Backing"),
                                             plotCols);
        MultiLayer *ml = app->newGraph(
                app->generateUniqueName(QStringLiteral("KMeansPlot_") + srcTable->name(), false));
        res.clusterPlot = ml;
        if (ml && plotTbl) {
            if (Graph *g = ml->activeGraph()) {
                static const QColor kPalette[] = {
                    QColor(31, 119, 180), QColor(214, 39, 40),  QColor(44, 160, 44),
                    QColor(148, 103, 189), QColor(255, 127, 14), QColor(23, 190, 207),
                    QColor(227, 119, 194), QColor(140, 86, 75)
                };
                static const QwtSymbol::Style kSymbols[] = {
                    QwtSymbol::Ellipse,  QwtSymbol::Rect,      QwtSymbol::Triangle,
                    QwtSymbol::Diamond,  QwtSymbol::UTriangle, QwtSymbol::DTriangle,
                    QwtSymbol::Hexagon,  QwtSymbol::Star2
                };

                for (int c = 0; c < K; ++c) {
                    const int cnt = clusRows[static_cast<size_t>(c)];
                    if (cnt <= 0)
                        continue;
                    const QColor col = kPalette[c % 8];
                    if (g->insertCurve(plotTbl, plotTbl->colName(2 * c),
                                       plotTbl->colName(2 * c + 1), Graph::Scatter, 0, cnt - 1)) {
                        const int idx = g->curves() - 1;
                        g->setCurveSymbol(idx,
                                          QwtSymbol(kSymbols[c % 8], QBrush(col),
                                                    QPen(col.darker(130), 1.1), QSize(8, 8)));
                    }
                }
                if (g->insertCurve(plotTbl, plotTbl->colName(2 * K), plotTbl->colName(2 * K + 1),
                                   Graph::Scatter, 0, K - 1)) {
                    const int idx = g->curves() - 1;
                    g->setCurveSymbol(idx,
                                      QwtSymbol(QwtSymbol::Cross, QBrush(QColor(15, 15, 15)),
                                                QPen(QColor(15, 15, 15), 2.4), QSize(13, 13)));
                }

                g->setXAxisTitle(xAxisLbl);
                g->setYAxisTitle(yAxisLbl);
                g->setTitle(QObject::tr("K-Means Clustering (k = %1, Silhouette = %2)")
                                    .arg(K)
                                    .arg(res.silhouetteScore, 0, 'f', 3));
                if (g->legend())
                    g->legend()->setText(g->legendText());
                applyArialGraphFonts(g);
                g->setAutoScale();
                g->replot();
            }
        }
    }

    return res;
}

MultiLayer *GraphStatsAndSurfaceFit::plotHierarchicalDendrogram(
        ApplicationWindow *app, Table *srcTable, const QStringList &colNames, int linkage)
{
    registerPhase5StatsTranslationsOnce();
    if (!app || !srcTable)
        return nullptr;

    std::vector<Column *> cols;
    QStringList namesToUse = colNames;
    if (namesToUse.isEmpty()) {
        for (int c = 0; c < srcTable->numCols(); ++c) {
            if (columnHasNumericValues(srcTable->column(c)))
                namesToUse << srcTable->column(c)->name();
        }
    }
    for (const QString &cn : namesToUse) {
        if (Column *c = resolveTableColumn(srcTable, cn))
            cols.push_back(c);
    }
    const int D = static_cast<int>(cols.size());
    if (D < 1)
        return nullptr;

    int maxRows = cols[0]->rowCount();
    for (Column *c : cols)
        maxRows = std::min(maxRows, c->rowCount());

    std::vector<int> validRows;
    std::vector<std::vector<double>> rawX;
    for (int r = 0; r < maxRows; ++r) {
        bool okRow = true;
        std::vector<double> pt(static_cast<size_t>(D), 0.0);
        for (int d = 0; d < D; ++d) {
            Column *cd = cols[static_cast<size_t>(d)];
            double v = 0.0;
            if (!readNumericColumnCell(cd, r, v)) {
                okRow = false;
                break;
            }
            pt[static_cast<size_t>(d)] = v;
        }
        if (okRow) {
            validRows.push_back(r);
            rawX.push_back(std::move(pt));
        }
    }

    const int N = static_cast<int>(rawX.size());
    if (N < 2)
        return nullptr;

    // Standardize features
    std::vector<double> meanF(static_cast<size_t>(D), 0.0);
    std::vector<double> sdF(static_cast<size_t>(D), 1.0);
    for (int d = 0; d < D; ++d) {
        double s = 0.0;
        for (int i = 0; i < N; ++i)
            s += rawX[static_cast<size_t>(i)][static_cast<size_t>(d)];
        meanF[static_cast<size_t>(d)] = s / static_cast<double>(N);
        double ss = 0.0;
        for (int i = 0; i < N; ++i) {
            const double dev =
                    rawX[static_cast<size_t>(i)][static_cast<size_t>(d)] - meanF[static_cast<size_t>(d)];
            ss += dev * dev;
        }
        const double sd = std::sqrt(ss / static_cast<double>(N - 1));
        sdF[static_cast<size_t>(d)] = (sd > 1e-15) ? sd : 1.0;
    }

    const int effLinkage = std::clamp(linkage, 0, 2);
    const int totalNodes = 2 * N - 1;
    struct DendNode {
        int left = -1;
        int right = -1;
        int count = 1;
        double height = 0.0;
        double xPos = 0.0;
        bool active = false;
    };
    std::vector<DendNode> nodes(static_cast<size_t>(totalNodes));
    for (int i = 0; i < N; ++i)
        nodes[static_cast<size_t>(i)].active = true;

    // Dissimilarity matrix across all 2*N-1 nodes (squared Euclidean for Ward, Euclidean otherwise)
    std::vector<double> distMat(static_cast<size_t>(totalNodes * totalNodes), 0.0);
    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            double d2 = 0.0;
            for (int d = 0; d < D; ++d) {
                const double diff = (rawX[static_cast<size_t>(i)][static_cast<size_t>(d)]
                                     - rawX[static_cast<size_t>(j)][static_cast<size_t>(d)])
                        / sdF[static_cast<size_t>(d)];
                d2 += diff * diff;
            }
            const double val = (effLinkage == 0) ? d2 : std::sqrt(d2);
            distMat[static_cast<size_t>(i * totalNodes + j)] = val;
            distMat[static_cast<size_t>(j * totalNodes + i)] = val;
        }
    }

    for (int step = 0; step < N - 1; ++step) {
        int bestA = -1, bestB = -1;
        double minD = std::numeric_limits<double>::max();
        const int curMax = N + step;
        for (int a = 0; a < curMax; ++a) {
            if (!nodes[static_cast<size_t>(a)].active)
                continue;
            for (int b = a + 1; b < curMax; ++b) {
                if (!nodes[static_cast<size_t>(b)].active)
                    continue;
                const double dab = distMat[static_cast<size_t>(a * totalNodes + b)];
                if (dab < minD) {
                    minD = dab;
                    bestA = a;
                    bestB = b;
                }
            }
        }
        if (bestA < 0 || bestB < 0)
            break;

        const int newNode = N + step;
        const int nA = nodes[static_cast<size_t>(bestA)].count;
        const int nB = nodes[static_cast<size_t>(bestB)].count;
        const double rawHeight = (effLinkage == 0) ? std::sqrt(std::max(0.0, minD))
                                                   : std::max(0.0, minD);
        const double childMaxH = std::max(nodes[static_cast<size_t>(bestA)].height,
                                          nodes[static_cast<size_t>(bestB)].height);

        nodes[static_cast<size_t>(newNode)].left = bestA;
        nodes[static_cast<size_t>(newNode)].right = bestB;
        nodes[static_cast<size_t>(newNode)].count = nA + nB;
        nodes[static_cast<size_t>(newNode)].height = std::max(rawHeight, childMaxH);
        nodes[static_cast<size_t>(newNode)].active = true;
        nodes[static_cast<size_t>(bestA)].active = false;
        nodes[static_cast<size_t>(bestB)].active = false;

        for (int c = 0; c < newNode; ++c) {
            if (!nodes[static_cast<size_t>(c)].active)
                continue;
            const double dAC = distMat[static_cast<size_t>(bestA * totalNodes + c)];
            const double dBC = distMat[static_cast<size_t>(bestB * totalNodes + c)];
            const int nC = nodes[static_cast<size_t>(c)].count;
            double dNew = 0.0;
            if (effLinkage == 0) {
                // Lance-Williams exact Ward update on squared distances
                const double denom = static_cast<double>(nA + nB + nC);
                dNew = ((static_cast<double>(nA + nC) * dAC)
                        + (static_cast<double>(nB + nC) * dBC)
                        - (static_cast<double>(nC) * minD))
                        / denom;
                dNew = std::max(0.0, dNew);
            } else if (effLinkage == 1) {
                dNew = std::max(dAC, dBC);
            } else {
                dNew = (static_cast<double>(nA) * dAC + static_cast<double>(nB) * dBC)
                        / static_cast<double>(nA + nB);
            }
            distMat[static_cast<size_t>(newNode * totalNodes + c)] = dNew;
            distMat[static_cast<size_t>(c * totalNodes + newNode)] = dNew;
        }
    }

    // Crossing-free in-order leaf traversal
    std::vector<int> leafOrder;
    leafOrder.reserve(static_cast<size_t>(N));
    auto assignCoords = [&](auto &self, int u) -> void {
        if (u < 0)
            return;
        DendNode &nd = nodes[static_cast<size_t>(u)];
        if (nd.left < 0 && nd.right < 0) {
            leafOrder.push_back(u);
            nd.xPos = static_cast<double>(leafOrder.size());
            return;
        }
        self(self, nd.left);
        self(self, nd.right);
        nd.xPos = 0.5 * (nodes[static_cast<size_t>(nd.left)].xPos
                         + nodes[static_cast<size_t>(nd.right)].xPos);
    };
    assignCoords(assignCoords, totalNodes - 1);

    Column *colSegX = new Column(QStringLiteral("DendX"), SciDAVis::ColumnMode::Numeric);
    Column *colSegY = new Column(QStringLiteral("Dendrogram"), SciDAVis::ColumnMode::Numeric);
    Column *colLeafLbl = new Column(QStringLiteral("LeafLabel"), SciDAVis::ColumnMode::Text);
    colSegX->setPlotDesignation(SciDAVis::X);
    colSegY->setPlotDesignation(SciDAVis::Y);

    double maxHeight = 0.0;
    const double nanVal = std::numeric_limits<double>::quiet_NaN();
    for (int step = 0; step < N - 1; ++step) {
        const DendNode &nd = nodes[static_cast<size_t>(N + step)];
        const DendNode &lc = nodes[static_cast<size_t>(nd.left)];
        const DendNode &rc = nodes[static_cast<size_t>(nd.right)];
        const int base = 5 * step;
        colSegX->setValueAt(base + 0, lc.xPos);
        colSegY->setValueAt(base + 0, lc.height);
        colSegX->setValueAt(base + 1, lc.xPos);
        colSegY->setValueAt(base + 1, nd.height);
        colSegX->setValueAt(base + 2, rc.xPos);
        colSegY->setValueAt(base + 2, nd.height);
        colSegX->setValueAt(base + 3, rc.xPos);
        colSegY->setValueAt(base + 3, rc.height);
        const int sepRow = base + 4;
        colSegX->setValueAt(sepRow, nanVal);
        colSegY->setValueAt(sepRow, nanVal);
        colSegX->setInvalid(sepRow, true);
        colSegY->setInvalid(sepRow, true);
        maxHeight = std::max(maxHeight, nd.height);
    }

    for (size_t k = 0; k < leafOrder.size(); ++k) {
        const int origRow = validRows[static_cast<size_t>(leafOrder[k])];
        colLeafLbl->setTextAt(static_cast<int>(k), QStringLiteral("#%1").arg(origRow + 1));
    }

    const QString hidName = app->generateUniqueName(QStringLiteral("DendrogramData"));
    Table *backing = app->newHiddenTable(hidName, QStringLiteral("Dendrogram Segments"),
                                         QList<Column *>() << colSegX << colSegY << colLeafLbl);
    if (!backing)
        return nullptr;

    MultiLayer *ml = app->newGraph(
            app->generateUniqueName(QStringLiteral("Dendrogram_") + srcTable->name(), false));
    if (!ml)
        return nullptr;

    if (Graph *g = ml->activeGraph()) {
        const QPen bracketPen(QColor(25, 65, 130), 1.8, Qt::SolidLine);
        for (int step = 0; step < N - 1; ++step) {
            const int base = 5 * step;
            if (g->insertCurve(backing, backing->colName(0), backing->colName(1), Graph::Line, base,
                               base + 3)) {
                const int idx = g->curves() - 1;
                g->setCurvePen(idx, bracketPen);
            }
        }

        if (N <= 40)
            g->setLabelsTextFormat(Graph::Bottom, colLeafLbl, 0, N - 1);
        g->setScale(QwtPlot::xBottom, 0.4, static_cast<double>(N) + 0.6, (N <= 30) ? 1.0 : 0.0);
        g->setScale(QwtPlot::yLeft, 0.0, std::max(1e-6, maxHeight * 1.12));

        const QString linkTitle = (effLinkage == 0)
                ? QCoreApplication::translate("GraphStatsAndSurfaceFit", "Ward")
                : ((effLinkage == 1)
                           ? QCoreApplication::translate("GraphStatsAndSurfaceFit", "Complete")
                           : QCoreApplication::translate("GraphStatsAndSurfaceFit", "Average"));
        g->setXAxisTitle(QCoreApplication::translate("GraphStatsAndSurfaceFit", "Observation"));
        g->setYAxisTitle(QObject::tr("Dissimilarity Height"));
        g->setTitle(QObject::tr("Hierarchical Dendrogram (%1 Linkage)").arg(linkTitle));
        if (g->legend()) {
            g->legend()->setText(
                    QStringLiteral("\\c{1}")
                    + QCoreApplication::translate("GraphStatsAndSurfaceFit", "Dendrogram"));
        }
        applyArialGraphFonts(g);
        g->replot();
    }
    return ml;
}

// ============================================================================
// Passing-Bablok Nonparametric & Deming Method-Comparison Regression
// ============================================================================

GraphStatsAndSurfaceFit::PassingBablokDemingResult
GraphStatsAndSurfaceFit::runPassingBablokDeming(
        ApplicationWindow *app, Table *srcTable, const QString &colXName, const QString &colYName,
        double errorVarianceRatioLambda, bool createPlot)
{
    registerPhase5StatsTranslationsOnce();
    PassingBablokDemingResult res;
    if (!srcTable)
        return res;

    Column *cX = resolveTableColumn(srcTable, colXName);
    Column *cY = resolveTableColumn(srcTable, colYName);
    if (!cX || !cY)
        return res;

    std::vector<std::pair<double, double>> pts;
    const int maxRows = std::min(cX->rowCount(), cY->rowCount());
    pts.reserve(static_cast<size_t>(std::max(0, maxRows)));
    for (int r = 0; r < maxRows; ++r) {
        double xv = 0.0, yv = 0.0;
        if (readNumericColumnCell(cX, r, xv) && readNumericColumnCell(cY, r, yv))
            pts.emplace_back(xv, yv);
    }

    const int n = static_cast<int>(pts.size());
    if (n < 3)
        return res;

    std::stable_sort(pts.begin(), pts.end(),
                     [](const std::pair<double, double> &a, const std::pair<double, double> &b) {
                         return a.first < b.first;
                     });

    auto medianOfVec = [](std::vector<double> v) -> double {
        if (v.empty())
            return 0.0;
        std::sort(v.begin(), v.end());
        const size_t sz = v.size();
        return (sz % 2 == 1) ? v[sz / 2] : 0.5 * (v[sz / 2 - 1] + v[sz / 2]);
    };

    // 1. Passing-Bablok (1983) shifted median of pairwise slopes S_ij = (y_j - y_i) / (x_j - x_i)
    constexpr double kVertSlopeLimit = 1e100;
    auto isVerticalSlope = [](double b) -> bool {
        return !std::isfinite(b) || std::fabs(b) >= kVertSlopeLimit;
    };
    const double nanVal = std::numeric_limits<double>::quiet_NaN();
    const double infVal = std::numeric_limits<double>::infinity();

    std::vector<double> slopes;
    slopes.reserve(static_cast<size_t>(n) * static_cast<size_t>(n - 1) / 2);
    int kLessMinusOne = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const double dx = pts[static_cast<size_t>(j)].first - pts[static_cast<size_t>(i)].first;
            const double dy =
                    pts[static_cast<size_t>(j)].second - pts[static_cast<size_t>(i)].second;
            if (std::fabs(dx) <= 1e-15 && std::fabs(dy) <= 1e-15)
                continue; // identical pair omitted per Passing & Bablok (1983)
            double s = 0.0;
            if (std::fabs(dx) <= 1e-15) {
                s = (dy > 0.0) ? infVal : -infVal;
            } else {
                s = dy / dx;
                if (std::fabs(s + 1.0) <= 1e-15)
                    continue; // S_ij == -1 omitted per Passing & Bablok (1983)
            }
            if (s < -1.0)
                ++kLessMinusOne;
            slopes.push_back(s);
        }
    }

    const int M = static_cast<int>(slopes.size());
    if (M < 1)
        return res;

    std::sort(slopes.begin(), slopes.end());
    auto clampedSlopeAt = [&](long long idx0) -> double {
        const long long cIdx = std::clamp<long long>(idx0, 0, static_cast<long long>(M - 1));
        return slopes[static_cast<size_t>(cIdx)];
    };

    double pbSlope = nanVal;
    if (M % 2 == 1) {
        const double rawS = clampedSlopeAt(static_cast<long long>((M - 1) / 2) + kLessMinusOne);
        pbSlope = isVerticalSlope(rawS) ? nanVal : rawS;
    } else {
        const double s1 = clampedSlopeAt(static_cast<long long>(M / 2 - 1) + kLessMinusOne);
        const double s2 = clampedSlopeAt(static_cast<long long>(M / 2) + kLessMinusOne);
        pbSlope = (isVerticalSlope(s1) || isVerticalSlope(s2)) ? nanVal : (0.5 * (s1 + s2));
    }

    const double dn = static_cast<double>(n);
    const double cGamma = 1.959963984540054 * std::sqrt(dn * (dn - 1.0) * (2.0 * dn + 5.0) / 18.0);
    const long long m1Rank = std::llround(0.5 * (static_cast<double>(M) - cGamma));
    const long long m2Rank = static_cast<long long>(M) - m1Rank + 1;
    const long long loSlopeIdx =
            std::clamp<long long>(m1Rank + kLessMinusOne - 1, 0, static_cast<long long>(M - 1));
    const long long hiSlopeIdx =
            std::clamp<long long>(m2Rank + kLessMinusOne - 1, 0, static_cast<long long>(M - 1));
    const double rawSlopeLo = slopes[static_cast<size_t>(loSlopeIdx)];
    const double rawSlopeHi = slopes[static_cast<size_t>(hiSlopeIdx)];
    const double pbSlopeLo = isVerticalSlope(rawSlopeLo) ? nanVal : rawSlopeLo;
    const double pbSlopeHi = isVerticalSlope(rawSlopeHi) ? nanVal : rawSlopeHi;

    auto interceptForSlope = [&](double b) -> double {
        if (isVerticalSlope(b))
            return nanVal;
        std::vector<double> aVec;
        aVec.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            const double ai = pts[static_cast<size_t>(i)].second
                    - b * pts[static_cast<size_t>(i)].first;
            if (!std::isfinite(ai))
                return nanVal;
            aVec.push_back(ai);
        }
        const double med = medianOfVec(std::move(aVec));
        return std::isfinite(med) ? med : nanVal;
    };

    const double pbIntercept = interceptForSlope(pbSlope);
    const double aAtHi = interceptForSlope(pbSlopeHi);
    const double aAtLo = interceptForSlope(pbSlopeLo);
    const bool hasFiniteIntCI = std::isfinite(aAtHi) && std::isfinite(aAtLo);
    const double pbInterceptLo = hasFiniteIntCI ? std::min(aAtHi, aAtLo) : nanVal;
    const double pbInterceptHi = hasFiniteIntCI ? std::max(aAtHi, aAtLo) : nanVal;

    // 2. Passing-Bablok Cusum test for linearity
    struct CusumItem {
        double scoreD = 0.0;
        double resid = 0.0;
    };
    const bool pbHasFiniteLine = std::isfinite(pbSlope) && std::isfinite(pbIntercept);
    double cusumH = pbHasFiniteLine ? 0.0 : nanVal;
    double cusumP = pbHasFiniteLine ? 1.0 : nanVal;
    if (pbHasFiniteLine) {
        std::vector<CusumItem> cusumItems(static_cast<size_t>(n));
        int nPos = 0, nNeg = 0;
        for (int i = 0; i < n; ++i) {
            const double xi = pts[static_cast<size_t>(i)].first;
            const double yi = pts[static_cast<size_t>(i)].second;
            const double ri = yi - (pbIntercept + pbSlope * xi);
            const double di = (pbSlope >= 0.0)
                    ? ((yi + xi / std::max(1e-12, pbSlope) - pbIntercept) / 2.0)
                    : (xi + pbSlope * (yi - pbIntercept));
            cusumItems[static_cast<size_t>(i)] = { di, ri };
            if (ri > 1e-14)
                ++nPos;
            else if (ri < -1e-14)
                ++nNeg;
        }
        std::stable_sort(cusumItems.begin(), cusumItems.end(),
                         [](const CusumItem &a, const CusumItem &b) {
                             return a.scoreD < b.scoreD;
                         });

        if (nPos > 0 && nNeg > 0) {
            const double wPos = std::sqrt(static_cast<double>(nNeg) / static_cast<double>(nPos));
            const double wNeg = -std::sqrt(static_cast<double>(nPos) / static_cast<double>(nNeg));
            double running = 0.0;
            double maxAbsCum = 0.0;
            for (const CusumItem &ci : cusumItems) {
                if (ci.resid > 1e-14)
                    running += wPos;
                else if (ci.resid < -1e-14)
                    running += wNeg;
                maxAbsCum = std::max(maxAbsCum, std::fabs(running));
            }
            cusumH = maxAbsCum / std::sqrt(static_cast<double>(nPos + nNeg));
            if (cusumH > 1e-12) {
                double ksSum = 0.0;
                for (int j = 1; j <= 100; ++j) {
                    const double dj = static_cast<double>(j);
                    const double term = std::exp(-2.0 * dj * dj * cusumH * cusumH);
                    ksSum += ((j % 2 == 1) ? 1.0 : -1.0) * term;
                    if (term < 1e-16)
                        break;
                }
                cusumP = std::clamp(2.0 * ksSum, 0.0, 1.0);
            }
        }
    }

    // 3. Deming Regression (lambda = sigma_x^2 / sigma_y^2) + Leave-One-Out Jackknife SEs
    const double lambda = std::max(0.0, std::isfinite(errorVarianceRatioLambda)
                                                ? errorVarianceRatioLambda
                                                : 1.0);
    auto evalDemingPair = [&](int skipIdx, double &slopeOut, double &interceptOut) -> bool {
        int cnt = 0;
        double sx = 0.0, sy = 0.0;
        for (int i = 0; i < n; ++i) {
            if (i == skipIdx)
                continue;
            sx += pts[static_cast<size_t>(i)].first;
            sy += pts[static_cast<size_t>(i)].second;
            ++cnt;
        }
        if (cnt < 2)
            return false;
        const double mx = sx / static_cast<double>(cnt);
        const double my = sy / static_cast<double>(cnt);
        double sxx = 0.0, syy = 0.0, sxy = 0.0;
        for (int i = 0; i < n; ++i) {
            if (i == skipIdx)
                continue;
            const double dx = pts[static_cast<size_t>(i)].first - mx;
            const double dy = pts[static_cast<size_t>(i)].second - my;
            sxx += dx * dx;
            syy += dy * dy;
            sxy += dx * dy;
        }
        if (sxx <= 1e-30 && syy <= 1e-30)
            return false;
        // Numerically stable rationalized Deming root for lambda = sigma_x^2 / sigma_y^2
        const double u = lambda * syy - sxx;
        const double rad = std::sqrt(u * u + 4.0 * lambda * sxy * sxy);
        double beta = 0.0;
        if (u >= 0.0) {
            const double denom = 2.0 * lambda * sxy;
            if (std::fabs(denom) > 1e-30)
                beta = (u + rad) / denom;
            else if (sxx > 1e-30)
                beta = sxy / sxx;
        } else {
            const double denom = rad - u;
            beta = (denom > 1e-30) ? ((2.0 * sxy) / denom) : 0.0;
        }
        slopeOut = beta;
        interceptOut = my - beta * mx;
        return std::isfinite(slopeOut) && std::isfinite(interceptOut);
    };

    double demSlope = 1.0, demIntercept = 0.0;
    if (!evalDemingPair(-1, demSlope, demIntercept))
        return res;

    std::vector<double> jkSlopes, jkIntercepts;
    jkSlopes.reserve(static_cast<size_t>(n));
    jkIntercepts.reserve(static_cast<size_t>(n));
    for (int k = 0; k < n; ++k) {
        double bs = 0.0, ba = 0.0;
        if (evalDemingPair(k, bs, ba)) {
            jkSlopes.push_back(bs);
            jkIntercepts.push_back(ba);
        }
    }
    double demSlopeSE = 0.0, demInterceptSE = 0.0;
    if (jkSlopes.size() >= 2) {
        const double mJk = static_cast<double>(jkSlopes.size());
        const double meanBs =
                std::accumulate(jkSlopes.begin(), jkSlopes.end(), 0.0) / mJk;
        const double meanBa =
                std::accumulate(jkIntercepts.begin(), jkIntercepts.end(), 0.0) / mJk;
        double ssBs = 0.0, ssBa = 0.0;
        for (size_t i = 0; i < jkSlopes.size(); ++i) {
            const double dBs = jkSlopes[i] - meanBs;
            const double dBa = jkIntercepts[i] - meanBa;
            ssBs += dBs * dBs;
            ssBa += dBa * dBa;
        }
        demSlopeSE = std::sqrt(((mJk - 1.0) / mJk) * ssBs);
        demInterceptSE = std::sqrt(((mJk - 1.0) / mJk) * ssBa);
    }

    res.valid = true;
    res.n = n;
    res.pbSlope = pbSlope;
    res.pbSlopeCILow = pbSlopeLo;
    res.pbSlopeCIHigh = pbSlopeHi;
    res.pbIntercept = pbIntercept;
    res.pbInterceptCILow = pbInterceptLo;
    res.pbInterceptCIHigh = pbInterceptHi;
    res.cusumStat = cusumH;
    res.cusumLinearityP = cusumP;
    res.demingSlope = demSlope;
    res.demingSlopeSE = demSlopeSE;
    res.demingIntercept = demIntercept;
    res.demingInterceptSE = demInterceptSE;

    if (!app)
        return res;

    // 4. Summary Worksheet
    const double dfDem = static_cast<double>(std::max(1, n - 2));
    const double tCrit = gsl_cdf_tdist_Pinv(0.975, dfDem);
    const QString sumName = app->generateUniqueName(
            srcTable->name() + QStringLiteral("_PassingBablokDeming"), false);
    Table *sumTbl = app->newTable(
            6, 7, sumName, QObject::tr("Passing-Bablok & Deming Method Comparison"));
    res.summaryTable = sumTbl;
    if (sumTbl) {
        sumTbl->setHeader(QStringList() << QStringLiteral("Parameter") << QStringLiteral("Method")
                                        << QStringLiteral("Estimate") << QStringLiteral("SE")
                                        << QStringLiteral("CI95_Low") << QStringLiteral("CI95_High")
                                        << QStringLiteral("p_Value"));
        sumTbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        sumTbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Text);
        for (int c = 2; c < 7; ++c)
            sumTbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);

        auto markInvalidCols = [&](int row, std::initializer_list<int> colsToMark) {
            for (int c : colsToMark)
                sumTbl->column(c)->setInvalid(row, true);
        };

        sumTbl->column(0)->setTextAt(0, QStringLiteral("N"));
        sumTbl->column(1)->setTextAt(0, QStringLiteral("Paired Valid"));
        sumTbl->column(2)->setValueAt(0, static_cast<double>(n));
        markInvalidCols(0, { 3, 4, 5, 6 });

        sumTbl->column(0)->setTextAt(1, QStringLiteral("Slope"));
        sumTbl->column(1)->setTextAt(1, QStringLiteral("Passing-Bablok"));
        sumTbl->column(2)->setValueAt(1, pbSlope);
        sumTbl->column(4)->setValueAt(1, pbSlopeLo);
        sumTbl->column(5)->setValueAt(1, pbSlopeHi);
        markInvalidCols(1, { 3, 6 });

        sumTbl->column(0)->setTextAt(2, QStringLiteral("Intercept"));
        sumTbl->column(1)->setTextAt(2, QStringLiteral("Passing-Bablok"));
        sumTbl->column(2)->setValueAt(2, pbIntercept);
        sumTbl->column(4)->setValueAt(2, pbInterceptLo);
        sumTbl->column(5)->setValueAt(2, pbInterceptHi);
        markInvalidCols(2, { 3, 6 });

        sumTbl->column(0)->setTextAt(3, QStringLiteral("Cusum_Linearity"));
        sumTbl->column(1)->setTextAt(3, QStringLiteral("Passing-Bablok"));
        sumTbl->column(2)->setValueAt(3, cusumH);
        sumTbl->column(6)->setValueAt(3, cusumP);
        markInvalidCols(3, { 3, 4, 5 });

        sumTbl->column(0)->setTextAt(4, QStringLiteral("Slope"));
        sumTbl->column(1)->setTextAt(4, QStringLiteral("Deming (λ=%1)").arg(lambda, 0, 'g', 4));
        sumTbl->column(2)->setValueAt(4, demSlope);
        sumTbl->column(3)->setValueAt(4, demSlopeSE);
        sumTbl->column(4)->setValueAt(4, demSlope - tCrit * demSlopeSE);
        sumTbl->column(5)->setValueAt(4, demSlope + tCrit * demSlopeSE);
        markInvalidCols(4, { 6 });

        sumTbl->column(0)->setTextAt(5, QStringLiteral("Intercept"));
        sumTbl->column(1)->setTextAt(5, QStringLiteral("Deming (λ=%1)").arg(lambda, 0, 'g', 4));
        sumTbl->column(2)->setValueAt(5, demIntercept);
        sumTbl->column(3)->setValueAt(5, demInterceptSE);
        sumTbl->column(4)->setValueAt(5, demIntercept - tCrit * demInterceptSE);
        sumTbl->column(5)->setValueAt(5, demIntercept + tCrit * demInterceptSE);
        markInvalidCols(5, { 6 });
    }

    // 5. Method-Comparison Plot (Y = X identity + Passing-Bablok + 95% CI band + Deming)
    if (createPlot) {
        double xMin = pts.front().first, xMax = pts.back().first;
        double yMin = pts.front().second, yMax = pts.front().second;
        for (const auto &pr : pts) {
            yMin = std::min(yMin, pr.second);
            yMax = std::max(yMax, pr.second);
        }
        const double xSpan = std::max(1e-6, xMax - xMin);
        const double xLineMin = xMin - 0.04 * xSpan;
        const double xLineMax = xMax + 0.04 * xSpan;

        Column *colObsX = new Column(QStringLiteral("Ref_X"), SciDAVis::ColumnMode::Numeric);
        Column *colObsY = new Column(QStringLiteral("Observed"), SciDAVis::ColumnMode::Numeric);
        Column *colLineX = new Column(QStringLiteral("Fit_X"), SciDAVis::ColumnMode::Numeric);
        Column *colIdY = new Column(QStringLiteral("Identity_Y_eq_X"),
                                    SciDAVis::ColumnMode::Numeric);
        Column *colPbY = new Column(QStringLiteral("Passing_Bablok"),
                                    SciDAVis::ColumnMode::Numeric);
        Column *colDemY = new Column(QStringLiteral("Deming"), SciDAVis::ColumnMode::Numeric);
        colObsX->setPlotDesignation(SciDAVis::X);
        colObsY->setPlotDesignation(SciDAVis::Y);
        colLineX->setPlotDesignation(SciDAVis::X);
        colIdY->setPlotDesignation(SciDAVis::Y);
        colPbY->setPlotDesignation(SciDAVis::Y);
        colDemY->setPlotDesignation(SciDAVis::Y);

        for (int i = 0; i < n; ++i) {
            colObsX->setValueAt(i, pts[static_cast<size_t>(i)].first);
            colObsY->setValueAt(i, pts[static_cast<size_t>(i)].second);
        }
        colLineX->setValueAt(0, xLineMin);
        colLineX->setValueAt(1, xLineMax);
        colIdY->setValueAt(0, xLineMin);
        colIdY->setValueAt(1, xLineMax);
        if (pbHasFiniteLine) {
            colPbY->setValueAt(0, pbIntercept + pbSlope * xLineMin);
            colPbY->setValueAt(1, pbIntercept + pbSlope * xLineMax);
        } else {
            colPbY->setInvalid(0, true);
            colPbY->setInvalid(1, true);
        }
        colDemY->setValueAt(0, demIntercept + demSlope * xLineMin);
        colDemY->setValueAt(1, demIntercept + demSlope * xLineMax);
        for (int r = 2; r < n; ++r) {
            colLineX->setInvalid(r, true);
            colIdY->setInvalid(r, true);
            colPbY->setInvalid(r, true);
            colDemY->setInvalid(r, true);
        }

        const QString hidName = app->generateUniqueName(QStringLiteral("PBPlotData"));
        Table *plotTbl = app->newHiddenTable(
                hidName, QStringLiteral("Passing-Bablok & Deming Plot Data"),
                QList<Column *>() << colObsX << colObsY << colLineX << colIdY << colPbY << colDemY);

        MultiLayer *ml = app->newGraph(app->generateUniqueName(
                QStringLiteral("PassingBablok_") + srcTable->name(), false));
        res.comparisonPlot = ml;
        if (ml && plotTbl) {
            if (Graph *g = ml->activeGraph()) {
                // Build exact rank-envelope 95% CI band over x in [xLineMin, xLineMax]
                std::vector<std::pair<double, double>> ciLines;
                if (std::isfinite(pbSlopeLo) && std::isfinite(aAtLo) && std::fabs(pbSlopeLo) < 1e12)
                    ciLines.emplace_back(pbSlopeLo, aAtLo);
                if (std::isfinite(pbSlopeHi) && std::isfinite(aAtHi) && std::fabs(pbSlopeHi) < 1e12)
                    ciLines.emplace_back(pbSlopeHi, aAtHi);
                if (pbHasFiniteLine && std::fabs(pbSlope) < 1e12)
                    ciLines.emplace_back(pbSlope, pbIntercept);
                const long long idxSpan = std::max<long long>(1, hiSlopeIdx - loSlopeIdx);
                const long long stepIdx = std::max<long long>(1, idxSpan / 24);
                for (long long k = loSlopeIdx; k <= hiSlopeIdx; k += stepIdx) {
                    const double bk = slopes[static_cast<size_t>(k)];
                    if (std::isfinite(bk) && std::fabs(bk) < 1e12) {
                        const double ak = interceptForSlope(bk);
                        if (std::isfinite(ak))
                            ciLines.emplace_back(bk, ak);
                    }
                }

                if (!ciLines.empty()) {
                    constexpr int kGrid = 65;
                    QVector<double> bx(kGrid), bLo(kGrid), bHi(kGrid);
                    for (int gi = 0; gi < kGrid; ++gi) {
                        const double xg = xLineMin
                                + (xLineMax - xLineMin) * static_cast<double>(gi)
                                        / static_cast<double>(kGrid - 1);
                        double loY = std::numeric_limits<double>::max();
                        double hiY = -std::numeric_limits<double>::max();
                        for (const auto &ab : ciLines) {
                            const double yg = ab.second + ab.first * xg;
                            loY = std::min(loY, yg);
                            hiY = std::max(hiY, yg);
                        }
                        bx[gi] = xg;
                        bLo[gi] = loY;
                        bHi[gi] = hiY;
                    }
                    g->addShadedBand(bx, bLo, bHi, QColor(195, 30, 45, 44),
                                     QObject::tr("PB 95% CI"));
                }

                if (g->insertCurve(plotTbl, plotTbl->colName(0), plotTbl->colName(1),
                                   Graph::Scatter, 0, n - 1)) {
                    const int idx = g->curves() - 1;
                    g->setCurveSymbol(idx,
                                      QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(35, 95, 175)),
                                                QPen(QColor(15, 45, 100), 1), QSize(7, 7)));
                }
                if (g->insertCurve(plotTbl, plotTbl->colName(2), plotTbl->colName(3), Graph::Line,
                                   0, 1)) {
                    const int idx = g->curves() - 1;
                    g->setCurvePen(idx, QPen(QColor(115, 120, 130), 1.5, Qt::DashLine));
                }
                if (g->insertCurve(plotTbl, plotTbl->colName(2), plotTbl->colName(4), Graph::Line,
                                   0, 1)) {
                    const int idx = g->curves() - 1;
                    g->setCurvePen(idx, QPen(QColor(195, 30, 45), 2.2, Qt::SolidLine));
                }
                if (g->insertCurve(plotTbl, plotTbl->colName(2), plotTbl->colName(5), Graph::Line,
                                   0, 1)) {
                    const int idx = g->curves() - 1;
                    g->setCurvePen(idx, QPen(QColor(15, 135, 120), 2.0, Qt::DashDotLine));
                }

                double allMinY = std::min(yMin, xLineMin);
                double allMaxY = std::max(yMax, xLineMax);
                if (pbHasFiniteLine) {
                    allMinY = std::min({ allMinY, pbIntercept + pbSlope * xLineMin,
                                         pbIntercept + pbSlope * xLineMax });
                    allMaxY = std::max({ allMaxY, pbIntercept + pbSlope * xLineMin,
                                         pbIntercept + pbSlope * xLineMax });
                }
                const double ySpan = std::max(1e-6, allMaxY - allMinY);
                auto fmtPb = [](double v, int prec = 4) -> QString {
                    return std::isfinite(v) ? QString::number(v, 'g', prec)
                                            : QStringLiteral("undefined (vertical)");
                };
                const QString boxText =
                        QStringLiteral("Passing-Bablok (n = %1)\n"
                                       "  Slope = %2 [%3, %4]\n"
                                       "  Intercept = %5 [%6, %7]\n"
                                       "  Cusum p = %8\n"
                                       "Deming (λ = %9)\n"
                                       "  Y = %10 + %11 X")
                                .arg(n)
                                .arg(fmtPb(pbSlope))
                                .arg(fmtPb(pbSlopeLo))
                                .arg(fmtPb(pbSlopeHi))
                                .arg(fmtPb(pbIntercept))
                                .arg(fmtPb(pbInterceptLo))
                                .arg(fmtPb(pbInterceptHi))
                                .arg(fmtPb(cusumP, 3))
                                .arg(lambda, 0, 'g', 4)
                                .arg(demIntercept, 0, 'g', 4)
                                .arg(demSlope, 0, 'g', 4);
                g->setXAxisTitle(cX->name());
                g->setYAxisTitle(cY->name());
                g->setTitle(QString());
                if (g->legend())
                    g->legend()->setText(g->legendText());
                applyArialGraphFonts(g);
                g->setAutoScale();
                g->replot();
                g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
                if (Legend *pbBox = addFramedBoxOnGraph(
                            g, xLineMin + 0.03 * xSpan, allMaxY - 0.04 * ySpan, boxText)) {
                    attachStatsCalloutAutoAvoid(g, pbBox, 14);
                }
                g->replot();
            }
        }
    }

    return res;
}

// ============================================================================
// Phase 5 Interactive Statistical & Clustering Dialogs
// ============================================================================

void GraphStatsAndSurfaceFit::showWeibullDialog(ApplicationWindow *app)
{
    if (!app || isNonInteractiveApp(app))
        return;
    registerPhase5StatsTranslationsOnce();
    const QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Weibull Probability Plot"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == actTbl)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }
    QComboBox *cbData = new QComboBox(&dlg);
    QComboBox *cbCensor = new QComboBox(&dlg);
    QComboBox *cbCensorCoding = new QComboBox(&dlg);
    cbCensorCoding->addItem(
            QObject::tr("1 = Failure/Event, 0 = Censored [Kaplan-Meier Standard]"), 0);
    cbCensorCoding->addItem(QObject::tr("0 = Failure, 1 = Censored (Suspension)"), 1);
    cbCensorCoding->setEnabled(false);

    QComboBox *cbEstimator = new QComboBox(&dlg);
    cbEstimator->addItem(QObject::tr("Median Rank Regression (MRR — Benard)"), false);
    cbEstimator->addItem(QObject::tr("Maximum Likelihood (MLE — JIS R 1625 / ISO 20501)"), true);

    QCheckBox *chkThreeParam =
            new QCheckBox(QObject::tr("3-Parameter Weibull (Estimate Threshold γ)"), &dlg);
    QCheckBox *chkJisUnbias = new QCheckBox(
            QObject::tr("Unbias m by JIS R 1625 (MLE, complete data, 2-parameter)"), &dlg);
    auto syncJis = [&]() {
        const bool ok = cbEstimator->currentData().toBool() && !chkThreeParam->isChecked();
        chkJisUnbias->setEnabled(ok);
    };
    QObject::connect(cbEstimator, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { syncJis(); });
    QObject::connect(chkThreeParam, &QCheckBox::toggled, &dlg, [&](bool) { syncJis(); });
    syncJis();

    auto refreshCols = [&]() {
        cbData->clear();
        cbCensor->clear();
        cbCensor->addItem(QObject::tr("(None — Complete Failures)"), QString());
        const int idx = cbTable->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!col)
                continue;
            const QString cn = col->name();
            if (columnHasNumericValues(col))
                cbData->addItem(cn);
            cbCensor->addItem(cn, cn);
        }
        if (cbData->count() == 0) {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c))
                    cbData->addItem(t->column(c)->name());
            }
        }
    };
    refreshCols();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refreshCols(); });
    QObject::connect(cbCensor, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int idx) {
                         cbCensorCoding->setEnabled(!cbCensor->itemData(idx).toString().isEmpty());
                     });

    form->addRow(QObject::tr("Source Table:"), cbTable);
    form->addRow(QObject::tr("Strength / Lifetime Column:"), cbData);
    form->addRow(QObject::tr("Right-Censoring Column (Optional):"), cbCensor);
    form->addRow(QObject::tr("Censoring Coding:"), cbCensorCoding);
    form->addRow(QObject::tr("Parameter Estimator:"), cbEstimator);
    form->addRow(QString(), chkThreeParam);
    form->addRow(QString(), chkJisUnbias);
    lay->addLayout(form);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        const bool mleRequested = cbEstimator->currentData().toBool();
        const WeibullPlotResult wr = plotWeibullProbability(
                app, tables[cbTable->currentIndex()], cbData->currentText(),
                cbCensor->currentData().toString(), cbCensorCoding->currentData().toInt(),
                chkThreeParam->isChecked(), mleRequested, true,
                chkJisUnbias->isEnabled() && chkJisUnbias->isChecked());
        if (!wr.valid) {
            QString msg = wr.error;
            if (!wr.notes.isEmpty())
                msg += QStringLiteral("\n\n") + wr.notes.join(QLatin1Char('\n'));
            QMessageBox::warning(app, QObject::tr("Weibull Probability Plot"), msg);
        } else if (mleRequested && !wr.usedMle) {
            QString msg = !wr.notes.isEmpty()
                    ? wr.notes.join(QLatin1Char('\n'))
                    : QObject::tr("MLE did not converge; median rank regression values are reported instead.");
            QMessageBox::warning(app, QObject::tr("Weibull Probability Plot"), msg);
        }
    }
}

void GraphStatsAndSurfaceFit::showPassingBablokDemingDialog(ApplicationWindow *app)
{
    if (!app || isNonInteractiveApp(app))
        return;
    registerPhase5StatsTranslationsOnce();
    const QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Passing-Bablok & Deming Regression"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == actTbl)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }

    QComboBox *cbColX = new QComboBox(&dlg);
    QComboBox *cbColY = new QComboBox(&dlg);
    auto refreshCols = [&]() {
        cbColX->clear();
        cbColY->clear();
        const int idx = cbTable->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            if (columnHasNumericValues(t->column(c))) {
                const QString cn = t->column(c)->name();
                cbColX->addItem(cn);
                cbColY->addItem(cn);
            }
        }
        if (cbColX->count() == 0) {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c)) {
                    const QString cn = t->column(c)->name();
                    cbColX->addItem(cn);
                    cbColY->addItem(cn);
                }
            }
        }
        if (cbColY->count() >= 2)
            cbColY->setCurrentIndex(1);
    };
    refreshCols();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refreshCols(); });

    QDoubleSpinBox *sbLambda = new QDoubleSpinBox(&dlg);
    sbLambda->setRange(0.0001, 10000.0);
    sbLambda->setDecimals(4);
    sbLambda->setSingleStep(0.1);
    sbLambda->setValue(1.0);

    QCheckBox *chkPlot =
            new QCheckBox(QObject::tr("Create Method-Comparison Regression Plot"), &dlg);
    chkPlot->setChecked(true);

    form->addRow(QObject::tr("Source Table:"), cbTable);
    form->addRow(QObject::tr("Reference Method X Column:"), cbColX);
    form->addRow(QObject::tr("Test Method Y Column:"), cbColY);
    form->addRow(QObject::tr("Deming Error Variance Ratio λ (σx² / σy²):"), sbLambda);
    form->addRow(QString(), chkPlot);
    lay->addLayout(form);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        runPassingBablokDeming(app, tables[cbTable->currentIndex()], cbColX->currentText(),
                               cbColY->currentText(), sbLambda->value(), chkPlot->isChecked());
    }
}

void GraphStatsAndSurfaceFit::showOutlierTestDialog(ApplicationWindow *app)
{
    if (!app || isNonInteractiveApp(app))
        return;
    registerPhase5StatsTranslationsOnce();
    const QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Outlier Test (Grubbs / ESD / Dixon)"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == actTbl)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }
    QComboBox *cbCol = new QComboBox(&dlg);
    auto refreshCols = [&]() {
        cbCol->clear();
        const int idx = cbTable->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            if (columnHasNumericValues(t->column(c)))
                cbCol->addItem(t->column(c)->name());
        }
        if (cbCol->count() == 0) {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c))
                    cbCol->addItem(t->column(c)->name());
            }
        }
    };
    refreshCols();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refreshCols(); });

    QComboBox *cbMethod = new QComboBox(&dlg);
    cbMethod->addItem(QObject::tr("Two-Sided Grubbs' Test (Single Outlier)"), 0);
    cbMethod->addItem(QObject::tr("Generalized ESD / Rosner Test (Up to k Outliers)"), 1);
    cbMethod->addItem(QObject::tr("Dixon's Q Test (Small Sample Extreme)"), 2);

    QDoubleSpinBox *sbAlpha = new QDoubleSpinBox(&dlg);
    sbAlpha->setRange(0.001, 0.25);
    sbAlpha->setDecimals(3);
    sbAlpha->setSingleStep(0.01);
    sbAlpha->setValue(0.05);

    QSpinBox *sbMaxK = new QSpinBox(&dlg);
    sbMaxK->setRange(1, 50);
    sbMaxK->setValue(5);
    sbMaxK->setEnabled(false);
    QObject::connect(cbMethod, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int idx) { sbMaxK->setEnabled(cbMethod->itemData(idx).toInt() == 1); });

    QCheckBox *chkMask =
            new QCheckBox(QObject::tr("Mask Detected Outliers in Source Worksheet"), &dlg);

    form->addRow(QObject::tr("Source Table:"), cbTable);
    form->addRow(QObject::tr("Data Column:"), cbCol);
    form->addRow(QObject::tr("Outlier Detection Method:"), cbMethod);
    form->addRow(QObject::tr("Significance Level α:"), sbAlpha);
    form->addRow(QObject::tr("Max Outliers k (Generalized ESD):"), sbMaxK);
    form->addRow(QString(), chkMask);
    lay->addLayout(form);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        runOutlierTest(app, tables[cbTable->currentIndex()], cbCol->currentText(),
                       cbMethod->currentData().toInt(), sbAlpha->value(), sbMaxK->value(),
                       chkMask->isChecked());
    }
}

void GraphStatsAndSurfaceFit::showNonparametricDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || isNonInteractiveApp(app))
        return;
    registerPhase5StatsTranslationsOnce();
    const QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    Graph *targetG = resolveActiveGraph(app, g);

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Nonparametric & Smart-Switch Group Comparison"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == actTbl)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }

    QListWidget *colList = new QListWidget(&dlg);
    colList->setSelectionMode(QAbstractItemView::MultiSelection);
    auto populateCols = [&]() {
        colList->clear();
        const int idx = cbTable->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            if (columnHasNumericValues(t->column(c))) {
                QListWidgetItem *it = new QListWidgetItem(t->column(c)->name(), colList);
                it->setSelected(true);
            }
        }
    };
    populateCols();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { populateCols(); });

    QComboBox *cbMethod = new QComboBox(&dlg);
    cbMethod->addItem(
            QObject::tr("Auto Smart-Switch (Shapiro-Wilk -> Parametric / Nonparametric)"),
            static_cast<int>(TestAutoSmartSwitch));
    cbMethod->addItem(QObject::tr("Mann-Whitney U Test (2 Independent Groups)"),
                      static_cast<int>(TestMannWhitneyU));
    cbMethod->addItem(QObject::tr("Wilcoxon Signed-Rank Test (2 Paired Columns)"),
                      static_cast<int>(TestWilcoxonSignedRank));
    cbMethod->addItem(QObject::tr("Kruskal-Wallis H + Holm-Dunn Post-Hoc"),
                      static_cast<int>(TestKruskalWallisDunn));

    QCheckBox *chkAnnotate =
            new QCheckBox(QObject::tr("Draw Significance Brackets on Active Graph"), &dlg);
    chkAnnotate->setChecked(targetG != nullptr);
    chkAnnotate->setEnabled(targetG != nullptr);

    QComboBox *cbBracketFmt = new QComboBox(&dlg);
    cbBracketFmt->addItem(QObject::tr("Asterisks (*, **, ***)"), QStringLiteral("stars"));
    cbBracketFmt->addItem(QObject::tr("Asterisks + p-value (* (p = ...))"),
                          QStringLiteral("stars_p"));
    cbBracketFmt->addItem(QObject::tr("p-value only (p = ...)"), QStringLiteral("p"));

    QCheckBox *chkShowNs =
            new QCheckBox(QObject::tr("Show 'ns' Brackets for Non-Significant Pairs"), &dlg);
    chkShowNs->setChecked(false);

    auto syncBrkOpts = [&]() {
        const bool on = (targetG != nullptr) && chkAnnotate->isChecked();
        cbBracketFmt->setEnabled(on);
        chkShowNs->setEnabled(on);
    };
    syncBrkOpts();
    QObject::connect(chkAnnotate, &QCheckBox::toggled, &dlg, [&](bool) { syncBrkOpts(); });

    form->addRow(QObject::tr("Source Table:"), cbTable);
    form->addRow(QObject::tr("Variables (2+ Numeric Columns):"), colList);
    form->addRow(QObject::tr("Test Method:"), cbMethod);
    form->addRow(QString(), chkAnnotate);
    form->addRow(QObject::tr("Bracket Label:"), cbBracketFmt);
    form->addRow(QString(), chkShowNs);
    lay->addLayout(form);

    QLabel *wilcoxonNoteLbl = new QLabel(
            QObject::tr("Note: Wilcoxon Signed-Rank compares only the first 2 selected columns; extra columns will be ignored."),
            &dlg);
    wilcoxonNoteLbl->setWordWrap(true);
    wilcoxonNoteLbl->setStyleSheet(QStringLiteral("color: #B45309;"));
    auto syncWilcoxonNote = [&]() {
        const bool isWilcoxon =
                (cbMethod->currentData().toInt() == static_cast<int>(TestWilcoxonSignedRank));
        wilcoxonNoteLbl->setVisible(isWilcoxon && colList->selectedItems().size() > 2);
    };
    syncWilcoxonNote();
    QObject::connect(cbMethod, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { syncWilcoxonNote(); });
    QObject::connect(colList, &QListWidget::itemSelectionChanged, &dlg,
                     [&]() { syncWilcoxonNote(); });
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { syncWilcoxonNote(); });
    lay->addWidget(wilcoxonNoteLbl);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList selected;
    for (QListWidgetItem *it : colList->selectedItems())
        selected << it->text();
    if (selected.size() < 2)
        return;

    Table *chosenTbl = tables[cbTable->currentIndex()];
    const QVariant prevShowNs = chosenTbl->property("torigraph_anova_show_ns");
    const QVariant prevFmt = chosenTbl->property("torigraph_anova_bracket_format");
    chosenTbl->setProperty("torigraph_anova_show_ns", chkShowNs->isChecked());
    chosenTbl->setProperty("torigraph_anova_bracket_format", cbBracketFmt->currentData().toString());
    runNonparametricComparison(app, chosenTbl, selected,
                               static_cast<NonparamMethod>(cbMethod->currentData().toInt()),
                               chkAnnotate->isChecked() ? targetG : nullptr);
    chosenTbl->setProperty("torigraph_anova_show_ns", prevShowNs);
    chosenTbl->setProperty("torigraph_anova_bracket_format", prevFmt);
}

void GraphStatsAndSurfaceFit::showPairedComparisonDialog(ApplicationWindow *app)
{
    if (!app || isNonInteractiveApp(app))
        return;
    registerPhase5StatsTranslationsOnce();
    const QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Paired Comparison Plot (Before-After)"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == actTbl)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }

    QComboBox *cbBefore = new QComboBox(&dlg);
    QComboBox *cbAfter = new QComboBox(&dlg);
    auto refreshCols = [&]() {
        cbBefore->clear();
        cbAfter->clear();
        const int idx = cbTable->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            if (columnHasNumericValues(t->column(c))) {
                const QString cn = t->column(c)->name();
                cbBefore->addItem(cn);
                cbAfter->addItem(cn);
            }
        }
        if (cbBefore->count() == 0) {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c)) {
                    const QString cn = t->column(c)->name();
                    cbBefore->addItem(cn);
                    cbAfter->addItem(cn);
                }
            }
        }
        if (cbAfter->count() >= 2)
            cbAfter->setCurrentIndex(1);
    };
    refreshCols();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refreshCols(); });

    QCheckBox *chkWilcoxon = new QCheckBox(
            QObject::tr("Use Nonparametric Wilcoxon Signed-Rank (instead of Paired t)"), &dlg);

    form->addRow(QObject::tr("Source Table:"), cbTable);
    form->addRow(QObject::tr("Before Column:"), cbBefore);
    form->addRow(QObject::tr("After Column:"), cbAfter);
    form->addRow(QString(), chkWilcoxon);
    lay->addLayout(form);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        plotPairedComparison(app, tables[cbTable->currentIndex()], cbBefore->currentText(),
                             cbAfter->currentText(), chkWilcoxon->isChecked());
    }
}

void GraphStatsAndSurfaceFit::showCorrelationMatrixDialog(ApplicationWindow *app)
{
    if (!app || isNonInteractiveApp(app))
        return;
    registerPhase5StatsTranslationsOnce();
    const QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Correlation Matrix Heatmap"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == actTbl)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }

    QListWidget *colList = new QListWidget(&dlg);
    colList->setSelectionMode(QAbstractItemView::MultiSelection);
    auto populateCols = [&]() {
        colList->clear();
        const int idx = cbTable->currentIndex();
        if (idx < 0 || idx >= tables.size())
            return;
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            if (columnHasNumericValues(t->column(c))) {
                QListWidgetItem *it = new QListWidgetItem(t->column(c)->name(), colList);
                it->setSelected(true);
            }
        }
    };
    populateCols();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { populateCols(); });

    QComboBox *cbType = new QComboBox(&dlg);
    cbType->addItem(QObject::tr("Pearson Linear Correlation (r)"), false);
    cbType->addItem(QObject::tr("Spearman Rank Correlation (ρ)"), true);

    form->addRow(QObject::tr("Source Table:"), cbTable);
    form->addRow(QObject::tr("Variables (2+ Numeric Columns):"), colList);
    form->addRow(QObject::tr("Correlation Type:"), cbType);
    lay->addLayout(form);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList selected;
    for (QListWidgetItem *it : colList->selectedItems())
        selected << it->text();
    if (selected.size() < 2)
        return;

    plotCorrelationMatrixHeatmap(app, tables[cbTable->currentIndex()], selected,
                                 cbType->currentData().toBool());
}

void GraphStatsAndSurfaceFit::showClusteringDialog(ApplicationWindow *app)
{
    if (!app || isNonInteractiveApp(app))
        return;
    registerPhase5StatsTranslationsOnce();
    const QList<Table *> tables = allProjectTables(app);
    if (tables.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Cluster Analysis (K-Means & Hierarchical Dendrogram)"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *topForm = new QFormLayout();

    QComboBox *cbTable = new QComboBox(&dlg);
    Table *actTbl = resolveActiveTable(app);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == actTbl)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }

    QListWidget *colList = new QListWidget(&dlg);
    colList->setSelectionMode(QAbstractItemView::MultiSelection);
    topForm->addRow(QObject::tr("Source Table:"), cbTable);
    topForm->addRow(QObject::tr("Variables (2+ Numeric Columns):"), colList);
    lay->addLayout(topForm);

    QLabel *hintLbl = new QLabel(
            QObject::tr("Select at least 2 numeric columns (X columns are excluded by default)."),
            &dlg);
    hintLbl->setWordWrap(true);
    hintLbl->setStyleSheet(QStringLiteral("QLabel { color: #b45309; font-size: 11px; }"));
    lay->addWidget(hintLbl);

    QTabWidget *tabs = new QTabWidget(&dlg);

    // Tab 0: K-Means
    QWidget *tabKM = new QWidget(&dlg);
    QFormLayout *fKM = new QFormLayout(tabKM);
    QSpinBox *sbK = new QSpinBox(tabKM);
    sbK->setRange(2, 50);
    sbK->setValue(3);
    QCheckBox *chkPlotKM = new QCheckBox(QObject::tr("Create 2D Cluster Scatter Plot"), tabKM);
    chkPlotKM->setChecked(true);
    fKM->addRow(QObject::tr("Number of Clusters (k):"), sbK);
    fKM->addRow(QString(), chkPlotKM);
    tabs->addTab(tabKM, QObject::tr("K-Means Clustering"));

    // Tab 1: Hierarchical Dendrogram
    QWidget *tabHC = new QWidget(&dlg);
    QFormLayout *fHC = new QFormLayout(tabHC);
    QComboBox *cbLinkage = new QComboBox(tabHC);
    cbLinkage->addItem(QObject::tr("Ward's Minimum Variance"), 0);
    cbLinkage->addItem(QObject::tr("Complete Linkage (Furthest Neighbor)"), 1);
    cbLinkage->addItem(QObject::tr("Average Linkage (UPGMA)"), 2);
    fHC->addRow(QObject::tr("Linkage Criterion:"), cbLinkage);
    tabs->addTab(tabHC, QObject::tr("Hierarchical Dendrogram"));

    lay->addWidget(tabs);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QPushButton *okBtn = btns->button(QDialogButtonBox::Ok);
    auto updateSelectionState = [&]() {
        const bool ok = (colList->selectedItems().size() >= 2);
        if (okBtn)
            okBtn->setEnabled(ok);
        hintLbl->setVisible(!ok);
    };
    auto populateCols = [&]() {
        QSignalBlocker blk(colList);
        colList->clear();
        const int idx = cbTable->currentIndex();
        if (idx < 0 || idx >= tables.size()) {
            updateSelectionState();
            return;
        }
        Table *t = tables[idx];
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (columnHasNumericValues(col)) {
                QListWidgetItem *it = new QListWidgetItem(col->name(), colList);
                const bool isX = (col && col->plotDesignation() == SciDAVis::X);
                it->setSelected(!isX);
            }
        }
        updateSelectionState();
    };
    populateCols();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { populateCols(); });
    QObject::connect(colList, &QListWidget::itemSelectionChanged, &dlg, updateSelectionState);

    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList selected;
    for (QListWidgetItem *it : colList->selectedItems())
        selected << it->text();
    if (selected.size() < 2)
        return;

    if (tabs->currentIndex() == 0) {
        runKMeansClustering(app, tables[cbTable->currentIndex()], selected, sbK->value(),
                            chkPlotKM->isChecked());
    } else {
        plotHierarchicalDendrogram(app, tables[cbTable->currentIndex()], selected,
                                   cbLinkage->currentData().toInt());
    }
}
