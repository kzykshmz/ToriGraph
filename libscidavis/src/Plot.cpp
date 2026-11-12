/***************************************************************************
    File                 : Plot.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Plot window class

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
#include "Plot.h"
#include "Graph.h"
#include "Grid.h"
#include "Legend.h"
#include "MultiLayer.h"
#include "ScaleDraw.h"
#include "Spectrogram.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"

#include <qwt_plot.h>
#include <qwt_painter.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_map.h>
#include <qwt_scale_engine.h>
#include <qwt_text_label.h>
#include <qwt_symbol.h>

#include <QAbstractSpinBox>
#include <QApplication>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QVariant>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>

#include <cmath>
#include <iostream>
#include <memory>
using namespace std;

void setToriSuppressScaleBackboneInPrint(bool suppress);
namespace {
static double s_toriPrintDeviceScale = 1.0;
} // namespace

Plot::Plot(QWidget *parent, QString) : QwtPlot(parent)
{
    setAutoReplot(false);

    marker_key = 0;
    curve_key = 0;
    d_is_panning = false;
    d_space_pressed = false;
    d_z_pressed = false;
    d_x_pressed = false;
    d_y_pressed = false;
    d_highlighted_curve_key = -1;

    minTickLength = 5;
    majTickLength = 9;

    QFont defaultTickFont("Arial", 12, QFont::Normal, false);
    defaultTickFont.setHintingPreference(QFont::PreferNoHinting);
    QFont defaultTitleFont("Arial", 14, QFont::Bold, false);
    defaultTitleFont.setHintingPreference(QFont::PreferNoHinting);
    QFont defaultAxisTitleFont("Arial", 14, QFont::Normal, false);
    defaultAxisTitleFont.setHintingPreference(QFont::PreferNoHinting);
    setFont(defaultTickFont);
    if (titleLabel()) {
        titleLabel()->setFont(defaultTitleFont);
        QwtText pTitle = title();
        pTitle.setFont(defaultTitleFont);
        setTitle(pTitle);
    }

    setGeometry(QRect(0, 0, 500, 500));
    {
        QwtText yTitle(tr("Y Axis Title"));
        yTitle.setFont(defaultAxisTitleFont);
        yTitle.setColor(Qt::black);
        setAxisTitle(QwtPlot::yLeft, yTitle);

        QwtText xTitle(tr("X Axis Title"));
        xTitle.setFont(defaultAxisTitleFont);
        xTitle.setColor(Qt::black);
        setAxisTitle(QwtPlot::xBottom, xTitle);
    }

    // grid
    d_grid = new Grid;
    d_grid->enableX(false);
    d_grid->enableY(false);
    d_grid->setMajPenX(QPen(Qt::blue, 0, Qt::SolidLine));
    d_grid->setMinPenX(QPen(Qt::gray, 0, Qt::DotLine));
    d_grid->attach(this);

    // custom scale
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        setAxisMaxMinor(i, 2);
        setAxisFont(i, defaultTickFont);
        QwtScaleWidget *scale = (QwtScaleWidget *)axisWidget(i);
        if (scale) {
            scale->setFont(defaultTickFont);
            scale->setMargin(0);
            scale->setSpacing(6);
            scale->setPenWidth(2);

            // the axis title color & Arial font must be initialized...
            QwtText title = scale->title();
            title.setFont(defaultAxisTitleFont);
            title.setColor(Qt::black);
            scale->setTitle(title);

            //...same for axis color
            QPalette pal = scale->palette();
            pal.setColor(QPalette::WindowText, QColor(Qt::black));
            scale->setPalette(pal);

            ScaleDraw *sd = new ScaleDraw();
            sd->setTickLength(QwtScaleDiv::MinorTick, 0);
            sd->setTickLength(QwtScaleDiv::MediumTick, 0);
            sd->setTickLength(QwtScaleDiv::MajorTick, minTickLength);
            if (i == QwtPlot::yRight || i == QwtPlot::xTop) {
                sd->enableComponent(QwtAbstractScaleDraw::Labels, false);
                sd->enableComponent(QwtAbstractScaleDraw::Ticks, false);
                if (i == QwtPlot::yRight)
                    sd->setMinimumExtent(20);
                else if (i == QwtPlot::xTop)
                    sd->setMinimumExtent(8);
            } else if (i == QwtPlot::xBottom) {
                scale->setMinBorderDist(10, 28);
                scale->setBorderDist(10, 28);
            }

            setAxisScaleDraw(i, sd);
        }
    }

    enableAxis(QwtPlot::yRight, true);
    enableAxis(QwtPlot::xTop, true);

    // set tick labels alignment [ScaleDraw() defaults to Qt::AlignLeft | Qt::AlignVCenter, good only for yLeft axis]
    setAxisLabelAlignment(QwtPlot::yRight, Qt::AlignRight | Qt::AlignVCenter);
    setAxisLabelAlignment(QwtPlot::xBottom, Qt::AlignHCenter | Qt::AlignBottom);
    setAxisLabelAlignment(QwtPlot::xTop, Qt::AlignHCenter | Qt::AlignTop);

    QwtPlotLayout *pLayout = plotLayout();
    pLayout->setCanvasMargin(0);
    pLayout->setAlignCanvasToScales(true);

    QwtPlotCanvas *plCanvas = canvas();
    plCanvas->setFocusPolicy(Qt::StrongFocus);
    plCanvas->setFocusIndicator(QwtPlotCanvas::ItemFocusIndicator);
    plCanvas->setFocus();
    plCanvas->setFrameShadow(QwtPlot::Plain);
    plCanvas->setCursor(Qt::ArrowCursor);
    plCanvas->setLineWidth(0);
    plCanvas->setPaintAttribute(QwtPlotCanvas::PaintCached, false);
    plCanvas->setPaintAttribute(QwtPlotCanvas::PaintPacked, false);
    plCanvas->installEventFilter(this);
    this->installEventFilter(this);
    if (parent)
        parent->installEventFilter(this);

    QColor background = QColor(Qt::white);
    background.setAlpha(255);

    QPalette cg;
    cg.setColor(QPalette::Window, background);
    setPalette(cg);

    setAutoFillBackground(true);

    setCanvasBackground(background);
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(plCanvas);
    setFrameShape(QFrame::Box);
    setLineWidth(0);
    setMargin(14);
    updateAxes();
    updateLayout();
}

QColor Plot::frameColor()
{
    return palette().color(QPalette::Active, QPalette::WindowText);
}

void Plot::printFrame(QPainter *painter, const QRect &rect) const
{
    painter->save();

    const Graph *g = qobject_cast<const Graph *>(parent());
    const bool anyBreak = g
            && (g->axisBreak(QwtPlot::yLeft).enabled || g->axisBreak(QwtPlot::yRight).enabled
                || g->axisBreak(QwtPlot::xBottom).enabled || g->axisBreak(QwtPlot::xTop).enabled);

    int lw = lineWidth();
    if (lw && !anyBreak) {
        QColor color = palette().color(QPalette::Active, QPalette::WindowText);
        painter->setPen(QPen(color, lw, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    } else
        painter->setPen(QPen(Qt::NoPen));

    painter->setBrush(paletteBackgroundColor());

    int lw2 = lw / 2;
    if (lw % 2)
        painter->drawRect(rect.adjusted(lw2, lw2, -(lw2 + 1), -(lw2 + 1)));
    else
        painter->drawRect(rect.adjusted(lw2, lw2, -lw2, -lw2));

    painter->restore();
}

void Plot::drawItems(QPainter *painter, const QRect &rect, const QwtScaleMap map[axisCnt],
                     const QwtPlotPrintFilter &pfilter) const
{
    Graph *g = qobject_cast<Graph *>(parent());
    if (d_highlighted_curve_key >= 0
        && (!g || (!g->isExporting() && !g->property("isExporting").toBool()))) {
        QwtPlotItem *hitItem = d_curves.value(d_highlighted_curve_key, nullptr);
        QwtPlotCurve *hc = dynamic_cast<QwtPlotCurve *>(hitItem);
        if (hc && hc->isVisible() && hc->dataSize() > 0) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setClipRect(rect);
            QColor haloCol = hc->pen().color().isValid() ? hc->pen().color() : QColor(30, 102, 184);
            haloCol.setAlpha(68);
            const int haloWidth = qMax(5, hc->pen().width() + 5);
            painter->setPen(QPen(haloCol, haloWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            const QwtScaleMap &xMap = map[hc->xAxis()];
            const QwtScaleMap &yMap = map[hc->yAxis()];
            const DataCurve *dc = dynamic_cast<const DataCurve *>(hc);
            if (hc->style() != QwtPlotCurve::NoCurve && hc->dataSize() >= 2) {
                QPolygonF poly;
                for (int i = 0; i < hc->dataSize(); ++i) {
                    if (dc && dc->isPointMasked(i)) {
                        if (poly.size() >= 2)
                            painter->drawPolyline(poly);
                        poly.clear();
                        continue;
                    }
                    const double px = xMap.xTransform(hc->x(i));
                    const double py = yMap.xTransform(hc->y(i));
                    if (std::isfinite(px) && std::isfinite(py))
                        poly.append(QPointF(px, py));
                }
                if (poly.size() >= 2)
                    painter->drawPolyline(poly);
            } else {
                painter->setBrush(haloCol);
                painter->setPen(Qt::NoPen);
                const double r = qMax(6.0, 0.5 * hc->symbol().size().width() + 4.0);
                for (int i = 0; i < hc->dataSize(); ++i) {
                    if (dc && dc->isPointMasked(i))
                        continue;
                    const double px = xMap.xTransform(hc->x(i));
                    const double py = yMap.xTransform(hc->y(i));
                    if (std::isfinite(px) && std::isfinite(py))
                        painter->drawEllipse(QPointF(px, py), r, r);
                }
            }
            painter->restore();
        }
    }

    QwtPlot::drawItems(painter, rect, map, pfilter);

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        if (!axisEnabled(i))
            continue;

        if (i == QwtPlot::yRight && g && g->hasColorScaleOnAxis(QwtPlot::yRight)) {
            if (g->axesBackbones() && axisEnabled(QwtPlot::yLeft)) {
                ScaleDraw *leftSd = (ScaleDraw *)axisScaleDraw(QwtPlot::yLeft);
                const int lMajType = leftSd ? leftSd->majorTicksStyle() : ScaleDraw::None;
                const int lMinType = leftSd ? leftSd->minorTicksStyle() : ScaleDraw::None;
                const bool lMin = (lMinType == ScaleDraw::In || lMinType == ScaleDraw::Both);
                const bool lMaj = (lMajType == ScaleDraw::In || lMajType == ScaleDraw::Both);
                const QwtScaleDiv *lScDiv = axisScaleDiv(QwtPlot::yLeft);
                if ((lMin || lMaj) && lScDiv) {
                    const QColor color = axisWidget(QwtPlot::yLeft)
                            ? axisWidget(QwtPlot::yLeft)
                                      ->palette()
                                      .color(QPalette::Active, QPalette::WindowText)
                            : Qt::black;
                    const double tickScale = qMax(1.0, s_toriPrintDeviceScale);
                    const double tickPw = qMax(1.0, double(axesLinewidth()) * tickScale);
                    const int effMinLen = qMax(1, qRound(double(minTickLength) * tickScale));
                    const int effMajLen = qMax(1, qRound(double(majTickLength) * tickScale));
                    painter->save();
                    painter->setPen(QPen(color, tickPw, Qt::SolidLine));
                    const int x = rect.right();
                    const int low = rect.top() + (axisEnabled(QwtPlot::xTop) ? 1 : 0);
                    const int high = rect.bottom() - (axisEnabled(QwtPlot::xBottom) ? 1 : 0);
                    if (lMin) {
                        const QwtValueList minTickList = lScDiv->ticks(QwtScaleDiv::MinorTick);
                        for (int j = 0; j < (int)minTickList.count(); ++j) {
                            const int y = map[QwtPlot::yLeft].transform(minTickList[j]);
                            if (y >= low && y <= high)
                                QwtPainter::drawLine(painter, x + 1, y, x - effMinLen, y);
                        }
                        const QwtValueList medTickList = lScDiv->ticks(QwtScaleDiv::MediumTick);
                        for (int j = 0; j < (int)medTickList.count(); ++j) {
                            const int y = map[QwtPlot::yLeft].transform(medTickList[j]);
                            if (y >= low && y <= high)
                                QwtPainter::drawLine(painter, x + 1, y, x - effMinLen, y);
                        }
                    }
                    if (lMaj) {
                        const QwtValueList majTickList = lScDiv->ticks(QwtScaleDiv::MajorTick);
                        for (int j = 0; j < (int)majTickList.count(); ++j) {
                            const int y = map[QwtPlot::yLeft].transform(majTickList[j]);
                            if (y >= low && y <= high)
                                QwtPainter::drawLine(painter, x + 1, y, x - effMajLen, y);
                        }
                    }
                    painter->restore();
                }
            }
            continue;
        }

        if (i == QwtPlot::xTop && g && g->hasColorScaleOnAxis(QwtPlot::xTop)) {
            if (g->axesBackbones() && axisEnabled(QwtPlot::xBottom)) {
                ScaleDraw *botSd = (ScaleDraw *)axisScaleDraw(QwtPlot::xBottom);
                const int bMajType = botSd ? botSd->majorTicksStyle() : ScaleDraw::None;
                const int bMinType = botSd ? botSd->minorTicksStyle() : ScaleDraw::None;
                const bool bMin = (bMinType == ScaleDraw::In || bMinType == ScaleDraw::Both);
                const bool bMaj = (bMajType == ScaleDraw::In || bMajType == ScaleDraw::Both);
                const QwtScaleDiv *bScDiv = axisScaleDiv(QwtPlot::xBottom);
                if ((bMin || bMaj) && bScDiv) {
                    const QColor color = axisWidget(QwtPlot::xBottom)
                            ? axisWidget(QwtPlot::xBottom)
                                      ->palette()
                                      .color(QPalette::Active, QPalette::WindowText)
                            : Qt::black;
                    const double tickScale = qMax(1.0, s_toriPrintDeviceScale);
                    const double tickPw = qMax(1.0, double(axesLinewidth()) * tickScale);
                    const int effMinLen = qMax(1, qRound(double(minTickLength) * tickScale));
                    const int effMajLen = qMax(1, qRound(double(majTickLength) * tickScale));
                    painter->save();
                    painter->setPen(QPen(color, tickPw, Qt::SolidLine));
                    const int y = rect.top();
                    const int low = rect.left() + (axisEnabled(QwtPlot::yLeft) ? 1 : 0);
                    const int high = rect.right() - (axisEnabled(QwtPlot::yRight) ? 1 : 0);
                    if (bMin) {
                        const QwtValueList minTickList = bScDiv->ticks(QwtScaleDiv::MinorTick);
                        for (int j = 0; j < (int)minTickList.count(); ++j) {
                            const int x = map[QwtPlot::xBottom].transform(minTickList[j]);
                            if (x >= low && x <= high)
                                QwtPainter::drawLine(painter, x, y, x, y + effMinLen);
                        }
                        const QwtValueList medTickList = bScDiv->ticks(QwtScaleDiv::MediumTick);
                        for (int j = 0; j < (int)medTickList.count(); ++j) {
                            const int x = map[QwtPlot::xBottom].transform(medTickList[j]);
                            if (x >= low && x <= high)
                                QwtPainter::drawLine(painter, x, y, x, y + effMinLen);
                        }
                    }
                    if (bMaj) {
                        const QwtValueList majTickList = bScDiv->ticks(QwtScaleDiv::MajorTick);
                        for (int j = 0; j < (int)majTickList.count(); ++j) {
                            const int x = map[QwtPlot::xBottom].transform(majTickList[j]);
                            if (x >= low && x <= high)
                                QwtPainter::drawLine(painter, x, y, x, y + effMajLen);
                        }
                    }
                    painter->restore();
                }
            }
            continue;
        }

        ScaleDraw *sd = (ScaleDraw *)axisScaleDraw(i);
        int majorTicksType = sd->majorTicksStyle();
        int minorTicksType = sd->minorTicksStyle();

        bool min = (minorTicksType == ScaleDraw::In || minorTicksType == ScaleDraw::Both);
        bool maj = (majorTicksType == ScaleDraw::In || majorTicksType == ScaleDraw::Both);

        if (min || maj)
            drawInwardTicks(painter, rect, map[i], i, min, maj);
    }
    if (g && !g->isTernaryPlot() && !g->isPiePlot() && g->axesBackbones()) {
        const double devScale = qMax(1.0, s_toriPrintDeviceScale);
        const double lw = qMax(1.0, double(axesLinewidth()) * devScale);
        bool clp = painter->hasClipping();
        painter->save();
        painter->setPen(QPen(canvas()->palette().color(QPalette::Active, QPalette::WindowText),
                             lw, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        painter->setClipping(false);
        const QRectF br = QRectF(rect).adjusted(-0.5 * lw, -0.5 * lw, 0.5 * lw, 0.5 * lw);
        const bool forceFrame = (pfilter.options() & QwtPlotPrintFilter::PrintFrameWithScales);
        const bool showLeft = g->isAxisBackboneVisible(QwtPlot::yLeft) || forceFrame;
        const bool showRight = g->isAxisBackboneVisible(QwtPlot::yRight) || forceFrame;
        const bool showBottom = g->isAxisBackboneVisible(QwtPlot::xBottom) || forceFrame;
        const bool showTop = g->isAxisBackboneVisible(QwtPlot::xTop) || forceFrame;

        QColor gapBg = canvasBackground();
        if (!gapBg.isValid() || gapBg.alpha() == 0)
            gapBg = canvas()->palette().color(QPalette::Window);
        if (!gapBg.isValid() || gapBg.alpha() == 0)
            gapBg = Qt::white;
        const double eraseW = qMax(lw, double(canvas()->lineWidth()) * devScale) + 3.0 * devScale;
        const double gap = 4.0 * devScale;
        const double slashDx = 5.0 * devScale;
        const double slashDy = 3.0 * devScale;

        auto drawVEdge = [&](double xEdge, int axisId, int mirrorAxisId) {
            Graph::AxisBreakSpec brk = g->axisBreak(axisId);
            int mapAxis = axisId;
            if (!brk.enabled && !hasVisibleItemOnAxis(axisId) && g->axisBreak(mirrorAxisId).enabled) {
                brk = g->axisBreak(mirrorAxisId);
                mapAxis = mirrorAxisId;
            }
            const double y0 = br.top();
            const double y1 = br.bottom();
            if (brk.enabled) {
                const double brkPx = map[mapAxis].xTransform(brk.breakFrom);
                if (brkPx > y0 + gap + 4.0 * devScale && brkPx < y1 - gap - 4.0 * devScale) {
                    painter->save();
                    painter->setPen(QPen(gapBg, eraseW, Qt::SolidLine, Qt::FlatCap));
                    painter->drawLine(QPointF(xEdge, brkPx - gap), QPointF(xEdge, brkPx + gap));
                    painter->restore();
                    painter->drawLine(QPointF(xEdge, y0), QPointF(xEdge, brkPx - gap));
                    painter->drawLine(QPointF(xEdge, brkPx + gap), QPointF(xEdge, y1));
                    painter->drawLine(QPointF(xEdge - slashDx, brkPx - gap + slashDy),
                                      QPointF(xEdge + slashDx, brkPx - gap - slashDy));
                    painter->drawLine(QPointF(xEdge - slashDx, brkPx + gap + slashDy),
                                      QPointF(xEdge + slashDx, brkPx + gap - slashDy));
                    return;
                }
            }
            painter->drawLine(QPointF(xEdge, y0), QPointF(xEdge, y1));
        };

        auto drawHEdge = [&](double yEdge, int axisId, int mirrorAxisId) {
            Graph::AxisBreakSpec brk = g->axisBreak(axisId);
            int mapAxis = axisId;
            if (!brk.enabled && !hasVisibleItemOnAxis(axisId) && g->axisBreak(mirrorAxisId).enabled) {
                brk = g->axisBreak(mirrorAxisId);
                mapAxis = mirrorAxisId;
            }
            const double x0 = br.left();
            const double x1 = br.right();
            if (brk.enabled) {
                const double brkPx = map[mapAxis].xTransform(brk.breakFrom);
                if (brkPx > x0 + gap + 4.0 * devScale && brkPx < x1 - gap - 4.0 * devScale) {
                    painter->save();
                    painter->setPen(QPen(gapBg, eraseW, Qt::SolidLine, Qt::FlatCap));
                    painter->drawLine(QPointF(brkPx - gap, yEdge), QPointF(brkPx + gap, yEdge));
                    painter->restore();
                    painter->drawLine(QPointF(x0, yEdge), QPointF(brkPx - gap, yEdge));
                    painter->drawLine(QPointF(brkPx + gap, yEdge), QPointF(x1, yEdge));
                    painter->drawLine(QPointF(brkPx - gap - slashDy, yEdge + slashDx),
                                      QPointF(brkPx - gap + slashDy, yEdge - slashDx));
                    painter->drawLine(QPointF(brkPx + gap - slashDy, yEdge + slashDx),
                                      QPointF(brkPx + gap + slashDy, yEdge - slashDx));
                    return;
                }
            }
            painter->drawLine(QPointF(x0, yEdge), QPointF(x1, yEdge));
        };

        const bool anyBreak = g->axisBreak(QwtPlot::yLeft).enabled
                || g->axisBreak(QwtPlot::yRight).enabled
                || g->axisBreak(QwtPlot::xBottom).enabled
                || g->axisBreak(QwtPlot::xTop).enabled;

        if (showLeft && showRight && showBottom && showTop && !anyBreak) {
            painter->drawRect(br);
        } else {
            if (showLeft)
                drawVEdge(br.left(), QwtPlot::yLeft, QwtPlot::yRight);
            if (showRight)
                drawVEdge(br.right(), QwtPlot::yRight, QwtPlot::yLeft);
            if (showTop)
                drawHEdge(br.top(), QwtPlot::xTop, QwtPlot::xBottom);
            if (showBottom)
                drawHEdge(br.bottom(), QwtPlot::xBottom, QwtPlot::xTop);
        }
        painter->restore();
        painter->setClipping(clp);
    }
}

void Plot::drawInwardTicks(QPainter *painter, const QRect &rect, const QwtScaleMap &map, int axis,
                           bool min, bool maj) const
{
    int x1 = rect.left();
    int x2 = rect.right();
    int y1 = rect.top();
    int y2 = rect.bottom();

    QPalette pal = axisWidget(axis)->palette();
    QColor color = pal.color(QPalette::Active, QPalette::WindowText);

    const double tickScale = qMax(1.0, s_toriPrintDeviceScale);
    const double tickPw = qMax(1.0, double(axesLinewidth()) * tickScale);
    const int effMinTick = qMax(1, qRound(double(minTickLength) * tickScale));
    const int effMajTick = qMax(1, qRound(double(majTickLength) * tickScale));

    painter->save();
    painter->setPen(QPen(color, tickPw, Qt::SolidLine));

    QwtScaleDiv *scDiv = (QwtScaleDiv *)axisScaleDiv(axis);
    const QwtValueList minTickList = scDiv->ticks(QwtScaleDiv::MinorTick);
    int minTicks = (int)minTickList.count();

    const QwtValueList medTickList = scDiv->ticks(QwtScaleDiv::MediumTick);
    int medTicks = (int)medTickList.count();

    const QwtValueList majTickList = scDiv->ticks(QwtScaleDiv::MajorTick);
    int majTicks = (int)majTickList.count();

    int j, x, y, low, high;
    switch (axis) {
    case QwtPlot::yLeft:
        x = x1;
        low = y1;
        if (axisEnabled(QwtPlot::xTop))
            low += 1;
        high = y2;
        if (axisEnabled(QwtPlot::xBottom))
            high -= 1;
        if (min) {
            for (j = 0; j < minTicks; j++) {
                y = map.transform(minTickList[j]);
                if (y >= low && y <= high)
                    QwtPainter::drawLine(painter, x, y, x + effMinTick, y);
            }
            for (j = 0; j < medTicks; j++) {
                y = map.transform(medTickList[j]);
                if (y >= low && y <= high)
                    QwtPainter::drawLine(painter, x, y, x + effMinTick, y);
            }
        }

        if (maj) {
            for (j = 0; j < majTicks; j++) {
                y = map.transform(majTickList[j]);
                if (y >= low && y <= high)
                    QwtPainter::drawLine(painter, x, y, x + effMajTick, y);
            }
        }
        break;

    case QwtPlot::yRight: {
        x = x2;
        low = y1;
        if (axisEnabled(QwtPlot::xTop))
            low += 1;
        high = y2;
        if (axisEnabled(QwtPlot::xBottom))
            high -= 1;
        if (min) {
            for (j = 0; j < minTicks; j++) {
                y = map.transform(minTickList[j]);
                if (y >= low && y <= high)
                    QwtPainter::drawLine(painter, x + 1, y, x - effMinTick, y);
            }
            for (j = 0; j < medTicks; j++) {
                y = map.transform(medTickList[j]);
                if (y >= low && y <= high)
                    QwtPainter::drawLine(painter, x + 1, y, x - effMinTick, y);
            }
        }

        if (maj) {
            for (j = 0; j < majTicks; j++) {
                y = map.transform(majTickList[j]);
                if (y >= low && y <= high)
                    QwtPainter::drawLine(painter, x + 1, y, x - effMajTick, y);
            }
        }
    } break;

    case QwtPlot::xBottom:
        y = y2;
        low = x1;
        if (axisEnabled(QwtPlot::yLeft))
            low += 1;
        high = x2;
        if (axisEnabled(QwtPlot::yRight))
            high -= 1;
        if (min) {
            for (j = 0; j < minTicks; j++) {
                x = map.transform(minTickList[j]);
                if (x >= low && x <= high)
                    QwtPainter::drawLine(painter, x, y + 1, x, y - effMinTick);
            }
            for (j = 0; j < medTicks; j++) {
                x = map.transform(medTickList[j]);
                if (x >= low && x <= high)
                    QwtPainter::drawLine(painter, x, y + 1, x, y - effMinTick);
            }
        }

        if (maj) {
            for (j = 0; j < majTicks; j++) {
                x = map.transform(majTickList[j]);
                if (x >= low && x <= high)
                    QwtPainter::drawLine(painter, x, y + 1, x, y - effMajTick);
            }
        }
        break;

    case QwtPlot::xTop:
        y = y1;
        low = x1;
        if (axisEnabled(QwtPlot::yLeft))
            low += 1;
        high = x2;
        if (axisEnabled(QwtPlot::yRight))
            high -= 1;

        if (min) {
            for (j = 0; j < minTicks; j++) {
                x = map.transform(minTickList[j]);
                if (x >= low && x <= high)
                    QwtPainter::drawLine(painter, x, y, x, y + effMinTick);
            }
            for (j = 0; j < medTicks; j++) {
                x = map.transform(medTickList[j]);
                if (x >= low && x <= high)
                    QwtPainter::drawLine(painter, x, y, x, y + effMinTick);
            }
        }

        if (maj) {
            for (j = 0; j < majTicks; j++) {
                x = map.transform(majTickList[j]);
                if (x >= low && x <= high)
                    QwtPainter::drawLine(painter, x, y, x, y + effMajTick);
            }
        }
        break;
    }
    painter->restore();
}

void Plot::setAxesLinewidth(int width)
{
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)this->axisWidget(i);
        if (scale) {
            scale->setPenWidth(width);
            scale->repaint();
        }
    }
}

int Plot::axesLinewidth() const
{
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        const QwtScaleWidget *scale = this->axisWidget(axis);
        if (scale)
            return scale->penWidth();
    }
    return 0;
}

int Plot::minorTickLength() const
{
    return minTickLength;
}

int Plot::majorTickLength() const
{
    return majTickLength;
}

void Plot::setTickLength(int minLength, int majLength)
{
    if (majTickLength == majLength && minTickLength == minLength)
        return;

    majTickLength = majLength;
    minTickLength = minLength;
}

void Plot::updateLayout()
{
    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        if (!axisScaleDiv(axis) || !axisScaleDiv(axis)->isValid()) {
            updateAxes();
            break;
        }
    }

    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        QwtScaleWidget *sw = axisWidget(axis);
        if (!sw || !sw->scaleDraw())
            continue;
        if (sw->scaleDraw()->hasComponent(QwtAbstractScaleDraw::Labels)) {
            if (axis == QwtPlot::xBottom)
                sw->setMinBorderDist(10, 28);
            int sDist = 0, eDist = 0;
            sw->getBorderDistHint(sDist, eDist);
            sw->setBorderDist(sDist, eDist);
        } else {
            sw->setMinBorderDist(0, 0);
            sw->setBorderDist(0, 0);
        }
    }

    int rightOverhang = 30;
    if (axisEnabled(QwtPlot::xBottom) && axisWidget(QwtPlot::xBottom)) {
        int s = 0, e = 0;
        axisWidget(QwtPlot::xBottom)->getBorderDistHint(s, e);
        rightOverhang = qMax(rightOverhang, e + 6);
    }
    if (axisEnabled(QwtPlot::xTop) && axisWidget(QwtPlot::xTop)) {
        int s = 0, e = 0;
        axisWidget(QwtPlot::xTop)->getBorderDistHint(s, e);
        rightOverhang = qMax(rightOverhang, e + 6);
    }
    if (axisWidget(QwtPlot::yRight) && axisWidget(QwtPlot::yRight)->scaleDraw()) {
        if (axisWidget(QwtPlot::yRight)->isColorBarEnabled()) {
            axisWidget(QwtPlot::yRight)->scaleDraw()->setMinimumExtent(0);
        } else {
            const int minRightDim = qMax(20, rightOverhang - margin() + 2);
            axisWidget(QwtPlot::yRight)->scaleDraw()->setMinimumExtent(minRightDim);
        }
    }

    int topOverhang = 18;
    if (axisEnabled(QwtPlot::yLeft) && axisWidget(QwtPlot::yLeft)) {
        int s = 0, e = 0;
        axisWidget(QwtPlot::yLeft)->getBorderDistHint(s, e);
        topOverhang = qMax(topOverhang, s + 4);
    }
    if (axisEnabled(QwtPlot::yRight) && axisWidget(QwtPlot::yRight)) {
        int s = 0, e = 0;
        axisWidget(QwtPlot::yRight)->getBorderDistHint(s, e);
        topOverhang = qMax(topOverhang, s + 4);
    }
    if (axisWidget(QwtPlot::xTop) && axisWidget(QwtPlot::xTop)->scaleDraw()) {
        if (axisWidget(QwtPlot::xTop)->isColorBarEnabled()) {
            axisWidget(QwtPlot::xTop)->scaleDraw()->setMinimumExtent(0);
        } else {
            const int minTopDim = qMax(8, topOverhang - margin() + 2);
            axisWidget(QwtPlot::xTop)->scaleDraw()->setMinimumExtent(minTopDim);
        }
    }

    const int outsideRightMargin = property("_outsideRightMargin").toInt();
    if (outsideRightMargin > 0) {
        if (contentsMargins().right() != outsideRightMargin)
            setContentsMargins(0, 0, outsideRightMargin, 0);
    } else if (contentsMargins().right() != 0) {
        setContentsMargins(0, 0, 0, 0);
    }

    QwtPlot::updateLayout();

    if (axisWidget(QwtPlot::xBottom))
        axisWidget(QwtPlot::xBottom)->clearMask();
    if (axisWidget(QwtPlot::xTop))
        axisWidget(QwtPlot::xTop)->clearMask();

    if (Graph *g = qobject_cast<Graph *>(parent()))
        g->updateSelectionOverlayGeometry();
}

void Plot::replot()
{
    updateAxes();
    updateLayout();
    QwtPlot::replot();
    if (property("_outsideRightMargin").toInt() > 0)
        update();
}

void Plot::print(QPainter *painter, const QRect &plotRect, const QwtPlotPrintFilter &pfilter) const
{
    const_cast<Plot *>(this)->updateLayout();
    printFrame(painter, plotRect);
    const int m = qMax(0, margin());
    int mLeft = m;
    int mTop = m;
    int mRight = m;
    int mBottom = m;

    if (!axisEnabled(QwtPlot::yRight)) {
        int s = 0, e = 0;
        if (axisEnabled(QwtPlot::xBottom) && axisWidget(QwtPlot::xBottom))
            axisWidget(QwtPlot::xBottom)->getBorderDistHint(s, e);
        mRight = qMax(mRight, e + 6);
    }
    const int outsideRightMargin = property("_outsideRightMargin").toInt();
    if (outsideRightMargin > 0)
        mRight += outsideRightMargin;
    if (!axisEnabled(QwtPlot::xTop) && title().isEmpty()) {
        int s = 0, e = 0;
        if (axisEnabled(QwtPlot::yLeft) && axisWidget(QwtPlot::yLeft))
            axisWidget(QwtPlot::yLeft)->getBorderDistHint(s, e);
        mTop = qMax(mTop, s + 6);
    }

    // QwtPlot::print() already subtracts plotLayout()->margin() (== m) on all 4 sides
    // when PrintMargin is set in pfilter. Only pre-inset by extra overhang beyond m so
    // exported canvasRect matches on-screen QwtPlot::updateLayout() 1:1.
    const int baseM = (pfilter.options() & QwtPlotPrintFilter::PrintMargin) ? m : 0;
    const int adjL = qMax(0, mLeft - baseM);
    const int adjT = qMax(0, mTop - baseM);
    const int adjR = qMax(0, mRight - baseM);
    const int adjB = qMax(0, mBottom - baseM);

    const QRect innerRect =
            (plotRect.width() > (adjL + adjR + 2 * baseM)
             && plotRect.height() > (adjT + adjB + 2 * baseM))
            ? plotRect.adjusted(adjL, adjT, -adjR, -adjB)
            : plotRect;

    Graph *g = qobject_cast<Graph *>(parent());
    const bool suppressScaleBackbone =
            g && !g->isTernaryPlot() && !g->isPiePlot() && g->axesBackbones();
    const double savedScale = s_toriPrintDeviceScale;
    double devScale = 1.0;
    if (painter && painter->device() && logicalDpiX() > 0) {
        const int devDpi = painter->device()->logicalDpiX();
        if (devDpi > 0)
            devScale = qMax(1.0, double(devDpi) / double(logicalDpiX()));
    }
    struct ScopedPrintFrameState {
        bool suppress;
        double prevScale;
        ScopedPrintFrameState(bool sup, double prev, double cur) : suppress(sup), prevScale(prev)
        {
            s_toriPrintDeviceScale = cur;
            if (suppress)
                setToriSuppressScaleBackboneInPrint(true);
        }
        ~ScopedPrintFrameState()
        {
            if (suppress)
                setToriSuppressScaleBackboneInPrint(false);
            s_toriPrintDeviceScale = prevScale;
        }
    } frameGuard(suppressScaleBackbone, savedScale, devScale);

    QwtPlot::print(painter, innerRect, pfilter);
}

QwtPlotCurve *Plot::curve(int index)
{
    QwtPlotItem *it = d_curves.value(index);
    if (!it || it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
        return 0;
    return dynamic_cast<QwtPlotCurve *>(it);
}

bool Plot::hasVisibleItemOnAxis(int axis) const
{
    for (auto it = d_curves.constBegin(); it != d_curves.constEnd(); ++it) {
        const QwtPlotItem *item = it.value();
        if (!item || !item->isVisible())
            continue;
        if (item->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = const_cast<Spectrogram *>(static_cast<const Spectrogram *>(item));
            if (sp->hasColorScale() && sp->colorScaleAxis() == axis)
                return true;
        }
        if (item->xAxis() == axis || item->yAxis() == axis)
            return true;
    }
    return false;
}

void Plot::setHighlightedCurveKey(int key)
{
    if (d_highlighted_curve_key == key)
        return;
    d_highlighted_curve_key = key;
    replot();
}

int Plot::closestCurve(int xpos, int ypos, int &dist, int &point)
{
    QwtScaleMap map[QwtPlot::axisCnt];
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++)
        map[axis] = canvasMap(axis);

    double dmin = 1.0e10;
    int key = -1;
    point = -1;
    const double pxClick = double(xpos);
    const double pyClick = double(ypos);

    for (QMap<int, QwtPlotItem *>::iterator iter = d_curves.begin(); iter != d_curves.end();
         ++iter) {
        QwtPlotItem *item = iter.value();
        if (!item || !item->isVisible() || item->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;

        QwtPlotCurve *qc = dynamic_cast<QwtPlotCurve *>(item);
        if (!qc || !qc->isVisible())
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(item);
        if (pc && pc->type() == Graph::ErrorBars)
            continue;

        const DataCurve *dc = dynamic_cast<const DataCurve *>(item);
        const QwtScaleMap &xMap = map[qc->xAxis()];
        const QwtScaleMap &yMap = map[qc->yAxis()];
        const int n = qc->dataSize();

        for (int i = 0; i < n; i++) {
            if (dc && dc->isPointMasked(i))
                continue;
            const double cx = xMap.xTransform(qc->x(i)) - pxClick;
            const double cy = yMap.xTransform(qc->y(i)) - pyClick;
            if (!std::isfinite(cx) || !std::isfinite(cy))
                continue;
            const double f = qwtSqr(cx) + qwtSqr(cy);
            if (f < dmin) {
                dmin = f;
                key = iter.key();
                point = i;
            }
        }

        if (qc->style() != QwtPlotCurve::NoCurve && n >= 2) {
            for (int i = 0; i + 1 < n; i++) {
                if (dc && (dc->isPointMasked(i) || dc->isPointMasked(i + 1)))
                    continue;
                const double ax = xMap.xTransform(qc->x(i));
                const double ay = yMap.xTransform(qc->y(i));
                const double bx = xMap.xTransform(qc->x(i + 1));
                const double by = yMap.xTransform(qc->y(i + 1));
                if (!std::isfinite(ax) || !std::isfinite(ay) || !std::isfinite(bx)
                    || !std::isfinite(by))
                    continue;
                const double dx = bx - ax;
                const double dy = by - ay;
                const double len2 = dx * dx + dy * dy;
                double segDist2 = 0.0;
                double t = 0.0;
                if (len2 > 1e-12) {
                    t = qBound(0.0, ((pxClick - ax) * dx + (pyClick - ay) * dy) / len2, 1.0);
                    const double projX = ax + t * dx;
                    const double projY = ay + t * dy;
                    segDist2 = qwtSqr(pxClick - projX) + qwtSqr(pyClick - projY);
                } else {
                    segDist2 = qwtSqr(pxClick - ax) + qwtSqr(pyClick - ay);
                }
                if (segDist2 < dmin) {
                    dmin = segDist2;
                    key = iter.key();
                    point = (t <= 0.5) ? i : (i + 1);
                }
            }
        }
    }

    if (dmin >= 100.0) {
        auto segDistSq = [&](double ax, double ay, double bx, double by) -> double {
            const double dx = bx - ax;
            const double dy = by - ay;
            const double len2 = dx * dx + dy * dy;
            if (len2 > 1e-12) {
                const double t = qBound(0.0, ((pxClick - ax) * dx + (pyClick - ay) * dy) / len2, 1.0);
                const double projX = ax + t * dx;
                const double projY = ay + t * dy;
                return qwtSqr(pxClick - projX) + qwtSqr(pyClick - projY);
            }
            return qwtSqr(pxClick - ax) + qwtSqr(pyClick - ay);
        };

        for (QMap<int, QwtPlotItem *>::iterator iter = d_curves.begin(); iter != d_curves.end();
             ++iter) {
            QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(iter.value());
            if (!er || !er->isVisible())
                continue;
            DataCurve *mc = er->masterCurve();
            if (mc && !mc->isVisible())
                continue;
            const QwtScaleMap &xMap = map[er->xAxis()];
            const QwtScaleMap &yMap = map[er->yAxis()];
            const int n = er->dataSize();
            const double halfCap = 0.5 * double(qMax(0, er->capLength()));
            const bool isVert = (er->direction() == QwtErrorPlotCurve::Vertical);
            for (int i = 0; i < n; ++i) {
                if (mc && mc->isPointMasked(i))
                    continue;
                const double xv = er->x(i);
                const double yv = er->y(i);
                const double ev = std::fabs(er->errorValue(i));
                if (!std::isfinite(xv) || !std::isfinite(yv) || !std::isfinite(ev) || ev <= 0.0)
                    continue;
                double d2 = 1.0e30;
                if (isVert) {
                    const double px = xMap.xTransform(xv);
                    const double py1 = yMap.xTransform(er->minusSide() ? (yv - ev) : yv);
                    const double py2 = yMap.xTransform(er->plusSide() ? (yv + ev) : yv);
                    if (!std::isfinite(px) || !std::isfinite(py1) || !std::isfinite(py2))
                        continue;
                    d2 = segDistSq(px, py1, px, py2);
                    if (halfCap > 0.5) {
                        if (er->minusSide())
                            d2 = qMin(d2, segDistSq(px - halfCap, py1, px + halfCap, py1));
                        if (er->plusSide())
                            d2 = qMin(d2, segDistSq(px - halfCap, py2, px + halfCap, py2));
                    }
                } else {
                    const double py = yMap.xTransform(yv);
                    const double px1 = xMap.xTransform(er->minusSide() ? (xv - ev) : xv);
                    const double px2 = xMap.xTransform(er->plusSide() ? (xv + ev) : xv);
                    if (!std::isfinite(py) || !std::isfinite(px1) || !std::isfinite(px2))
                        continue;
                    d2 = segDistSq(px1, py, px2, py);
                    if (halfCap > 0.5) {
                        if (er->minusSide())
                            d2 = qMin(d2, segDistSq(px1, py - halfCap, px1, py + halfCap));
                        if (er->plusSide())
                            d2 = qMin(d2, segDistSq(px2, py - halfCap, px2, py + halfCap));
                    }
                }
                if (d2 < dmin) {
                    dmin = d2;
                    const int mcKey = mc ? d_curves.key(mc, -1) : -1;
                    key = (mcKey >= 0) ? mcKey : iter.key();
                    point = i;
                }
            }
        }
    }

    dist = (key >= 0) ? int(std::sqrt(dmin)) : 99999;
    return key;
}

void Plot::removeMarker(int index)
{
    QwtPlotMarker *m = d_markers[index];
    if (!m)
        return;
    m->detach();
    d_markers.remove(index);
}

int Plot::insertMarker(QwtPlotMarker *m)
{
    marker_key++;
    if (!d_markers.contains(marker_key))
        d_markers.insert(marker_key, m);
    m->setRenderHint(QwtPlotItem::RenderAntialiased, ((Graph *)parent())->antialiasing());
    m->attach(((QwtPlot *)this));
    return marker_key;
}

int Plot::insertCurve(QwtPlotItem *c)
{
    curve_key++;
    if (!d_curves.contains(curve_key))
        d_curves.insert(curve_key, c);
    if (c->rtti() != QwtPlotItem::Rtti_PlotSpectrogram)
        ((QwtPlotCurve *)c)->setPaintAttribute(QwtPlotCurve::PaintFiltered);

    c->setRenderHint(QwtPlotItem::RenderAntialiased, ((Graph *)parent())->antialiasing());
    c->attach(this);
    return curve_key;
}

void Plot::removeCurve(int index)
{
    QwtPlotItem *c = d_curves[index];
    if (!c)
        return;

    if (c->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
        Spectrogram *sp = (Spectrogram *)c;
        QwtScaleWidget *colorAxis = axisWidget(sp->colorScaleAxis());
        if (colorAxis)
            colorAxis->setColorBarEnabled(false);
    }

    if (d_highlighted_curve_key == index)
        d_highlighted_curve_key = -1;
    c->detach();
    d_curves.remove(index);
}

QList<int> Plot::getMajorTicksType()
{
    QList<int> majorTicksType;
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        if (axisEnabled(axis)) {
            ScaleDraw *sd = (ScaleDraw *)axisScaleDraw(axis);
            majorTicksType << sd->majorTicksStyle();
        } else
            majorTicksType << ScaleDraw::In;
    }
    return majorTicksType;
}

void Plot::setMajorTicksType(int axis, int type)
{
    ScaleDraw *sd = (ScaleDraw *)axisScaleDraw(axis);
    if (sd)
        sd->setMajorTicksStyle((ScaleDraw::TicksStyle)type);
}

QList<int> Plot::getMinorTicksType()
{
    QList<int> minorTicksType;
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        if (axisEnabled(axis)) {
            ScaleDraw *sd = (ScaleDraw *)axisScaleDraw(axis);
            minorTicksType << sd->minorTicksStyle();
        } else
            minorTicksType << ScaleDraw::In;
    }
    return minorTicksType;
}

void Plot::setMinorTicksType(int axis, int type)
{
    ScaleDraw *sd = (ScaleDraw *)axisScaleDraw(axis);
    if (sd)
        sd->setMinorTicksStyle((ScaleDraw::TicksStyle)type);
}

int Plot::axisLabelFormat(int axis)
{
    if (axisValid(axis)) {
        int prec;
        char format;

        ScaleDraw *sd = (ScaleDraw *)axisScaleDraw(axis);
        sd->labelFormat(format, prec);

        if (format == 'g')
            return Automatic;
        else if (format == 'e')
            return Scientific;
        else if (format == 'f')
            return Decimal;
        else
            return Superscripts;
    }

    return 0;
}

int Plot::axisLabelPrecision(int axis)
{
    if (axisValid(axis)) {
        ScaleDraw *sd = (ScaleDraw *)axisScaleDraw(axis);
        return sd->labelNumericPrecision();
    }

    // for a bad call we return the default values
    return 4;
}

/*!
  \return the number format for the major scale labels of a specified axis
  \param axis axis index
  \retval f format character
  \retval prec precision
  */
void Plot::axisLabelFormat(int axis, char &f, int &prec) const
{
    if (axisValid(axis)) {
        ScaleDraw *sd = (ScaleDraw *)axisScaleDraw(axis);
        sd->labelFormat(f, prec);
    } else {
        // for a bad call we return the default values
        f = 'g';
        prec = 4;
    }
}

/*!
  Change the number format for the major scale of a selected axis
  \param axis axis index
  \param f format
  \param prec precision
  */
void Plot::setAxisLabelFormat(int axis, char f, int prec)
{
    if (axisValid(axis)) {
        ScaleDraw *sd = (ScaleDraw *)axisScaleDraw(axis);
        sd->setLabelFormat(f, prec);
    }
}

const QColor &Plot::paletteBackgroundColor() const
{
    return palette().color(QPalette::Window);
}

int Plot::scaleAlignmentToAxis(int alignment)
{
    switch (alignment) {
    case QwtScaleDraw::BottomScale:
        return QwtPlot::xBottom;
    case QwtScaleDraw::TopScale:
        return QwtPlot::xTop;
    case QwtScaleDraw::LeftScale:
        return QwtPlot::yLeft;
    case QwtScaleDraw::RightScale:
        return QwtPlot::yRight;
    default:
        return -1;
    }
}

bool Plot::isLog10ScaleEngine(const QwtScaleEngine *se)
{
    std::unique_ptr<QwtScaleTransformation> xf(se ? se->transformation() : nullptr);
    return xf && xf->type() == QwtScaleTransformation::Log10;
}

void Plot::zoomAxisAroundPos(int axis, int pixelPos, double factor)
{
    if (!axisValid(axis))
        return;

    QwtScaleEngine *se = axisScaleEngine(axis);
    const QwtScaleDiv *scDiv = axisScaleDiv(axis);
    if (!se || !scDiv)
        return;

    const bool inverted = se->testAttribute(QwtScaleEngine::Inverted)
            || (scDiv->lowerBound() > scDiv->upperBound());
    double minVal = std::min(scDiv->lowerBound(), scDiv->upperBound());
    double maxVal = std::max(scDiv->lowerBound(), scDiv->upperBound());
    if (maxVal <= minVal)
        return;

    const QwtScaleMap map = canvasMap(axis);
    Graph *g = qobject_cast<Graph *>(parent());
    if (g && g->axisBreak(axis).enabled) {
        const Graph::AxisBreakSpec brk = g->axisBreak(axis);
        const bool isLog = brk.log10Before && brk.log10After;
        const double p1 = map.p1();
        const double p2 = map.p2();
        double frac = (std::fabs(p2 - p1) > 1.0)
                ? qBound(0.0, (double(pixelPos) - p1) / (p2 - p1), 1.0)
                : 0.5;
        if (inverted)
            frac = 1.0 - frac;
        double newMin = minVal;
        double newMax = maxVal;
        if (isLog) {
            if (minVal <= 0.0 || maxVal <= 0.0)
                return;
            const double logMin = std::log10(minVal);
            const double logMax = std::log10(maxVal);
            const double logCenter = logMin + frac * (logMax - logMin);
            const double newLogMin = logCenter + (logMin - logCenter) * factor;
            const double newLogMax = logCenter + (logMax - logCenter) * factor;
            if (newLogMax - newLogMin < 1e-6 || newLogMin < -300.0 || newLogMax > 300.0)
                return;
            newMin = std::pow(10.0, newLogMin);
            newMax = std::pow(10.0, newLogMax);
        } else {
            const double centerVal = minVal + frac * (maxVal - minVal);
            newMin = centerVal + (minVal - centerVal) * factor;
            newMax = centerVal + (maxVal - centerVal) * factor;
            if (newMax - newMin <= 1e-15)
                return;
        }
        g->setScale(axis, newMin, newMax, isLog ? 0.0 : g->axisStep(axis), axisMaxMajor(axis),
                    axisMaxMinor(axis), isLog ? 1 : 0, inverted);
        return;
    }

    double centerVal = map.invTransform(pixelPos);
    const bool isLog = isLog10ScaleEngine(se);

    double newMin = minVal;
    double newMax = maxVal;
    if (isLog) {
        if (minVal <= 0.0 || maxVal <= 0.0)
            return;
        if (centerVal <= 0.0 || !std::isfinite(centerVal))
            centerVal = std::sqrt(minVal * maxVal);
        centerVal = qBound(minVal, centerVal, maxVal);
        const double logMin = std::log10(minVal);
        const double logMax = std::log10(maxVal);
        const double logCenter = std::log10(centerVal);
        const double newLogMin = logCenter + (logMin - logCenter) * factor;
        const double newLogMax = logCenter + (logMax - logCenter) * factor;
        if (newLogMax - newLogMin < 1e-6 || newLogMin < -300.0 || newLogMax > 300.0)
            return;
        newMin = std::pow(10.0, newLogMin);
        newMax = std::pow(10.0, newLogMax);
    } else {
        if (!std::isfinite(centerVal))
            centerVal = 0.5 * (minVal + maxVal);
        centerVal = qBound(minVal, centerVal, maxVal);
        newMin = centerVal + (minVal - centerVal) * factor;
        newMax = centerVal + (maxVal - centerVal) * factor;
        if (newMax - newMin <= 1e-15)
            return;
    }

    int maxMajor = qMax(axisMaxMajor(axis), 2);
    int minorTicks = axisMaxMinor(axis);
    int maxMinIntervals = minorTicks;
    if (minorTicks == 1)
        maxMinIntervals = 3;
    else if (minorTicks > 1)
        maxMinIntervals = minorTicks + 1;

    QwtScaleDiv div = se->divideScale(newMin, newMax, maxMajor, maxMinIntervals);
    if (inverted) {
        se->setAttribute(QwtScaleEngine::Inverted);
        if (div.lowerBound() < div.upperBound())
            div.invert();
    }
    setAxisScaleDiv(axis, div);
}

void Plot::panAxisByPixels(int axis, int startPixel, int endPixel)
{
    if (startPixel == endPixel || !axisValid(axis))
        return;

    QwtScaleEngine *se = axisScaleEngine(axis);
    const QwtScaleDiv *scDiv = axisScaleDiv(axis);
    if (!se || !scDiv)
        return;

    const bool inverted = se->testAttribute(QwtScaleEngine::Inverted)
            || (scDiv->lowerBound() > scDiv->upperBound());
    double minVal = std::min(scDiv->lowerBound(), scDiv->upperBound());
    double maxVal = std::max(scDiv->lowerBound(), scDiv->upperBound());
    if (maxVal <= minVal)
        return;

    const QwtScaleMap map = canvasMap(axis);
    Graph *g = qobject_cast<Graph *>(parent());
    if (g && g->axisBreak(axis).enabled) {
        const Graph::AxisBreakSpec brk = g->axisBreak(axis);
        const bool isLog = brk.log10Before && brk.log10After;
        const double p1 = map.p1();
        const double p2 = map.p2();
        if (std::fabs(p2 - p1) <= 1.0)
            return;
        double fracDelta = double(startPixel - endPixel) / (p2 - p1);
        if (inverted)
            fracDelta = -fracDelta;
        double newMin = minVal;
        double newMax = maxVal;
        if (isLog) {
            if (minVal <= 0.0 || maxVal <= 0.0)
                return;
            const double logMin = std::log10(minVal);
            const double logMax = std::log10(maxVal);
            const double logDelta = fracDelta * (logMax - logMin);
            const double newLogMin = logMin + logDelta;
            const double newLogMax = logMax + logDelta;
            if (newLogMin < -300.0 || newLogMax > 300.0)
                return;
            newMin = std::pow(10.0, newLogMin);
            newMax = std::pow(10.0, newLogMax);
        } else {
            const double delta = fracDelta * (maxVal - minVal);
            if (!std::isfinite(delta))
                return;
            newMin = minVal + delta;
            newMax = maxVal + delta;
        }
        g->setScale(axis, newMin, newMax, isLog ? 0.0 : g->axisStep(axis), axisMaxMajor(axis),
                    axisMaxMinor(axis), isLog ? 1 : 0, inverted);
        return;
    }

    const double vStart = map.invTransform(startPixel);
    const double vEnd = map.invTransform(endPixel);
    const bool isLog = isLog10ScaleEngine(se);

    double newMin = minVal;
    double newMax = maxVal;
    double step = 0.0;
    if (g && !isLog)
        step = g->axisStep(axis);

    if (isLog) {
        if (minVal <= 0.0 || maxVal <= 0.0 || vStart <= 0.0 || vEnd <= 0.0)
            return;
        const double logDelta = std::log10(vStart) - std::log10(vEnd);
        const double newLogMin = std::log10(minVal) + logDelta;
        const double newLogMax = std::log10(maxVal) + logDelta;
        if (newLogMin < -300.0 || newLogMax > 300.0)
            return;
        newMin = std::pow(10.0, newLogMin);
        newMax = std::pow(10.0, newLogMax);
    } else {
        const double delta = vStart - vEnd;
        if (!std::isfinite(delta))
            return;
        newMin = minVal + delta;
        newMax = maxVal + delta;
    }

    int maxMajor = qMax(axisMaxMajor(axis), 2);
    int minorTicks = axisMaxMinor(axis);
    int maxMinIntervals = minorTicks;
    if (minorTicks == 1)
        maxMinIntervals = 3;
    else if (minorTicks > 1)
        maxMinIntervals = minorTicks + 1;

    QwtScaleDiv div = se->divideScale(newMin, newMax, maxMajor, maxMinIntervals, step);
    if (inverted) {
        se->setAttribute(QwtScaleEngine::Inverted);
        if (div.lowerBound() < div.upperBound())
            div.invert();
    }
    setAxisScaleDiv(axis, div);
}

bool Plot::eventFilter(QObject *object, QEvent *e)
{
    Graph *g = qobject_cast<Graph *>(parent());

    if (object == this && e->type() == QEvent::Paint) {
        if (g && !g->isExporting() && g->isLegendVisible()) {
            if (Legend *leg = g->legend()) {
                if (canvas() && leg->isVisible() && leg->rect().right() > canvas()->width()) {
                    QPainter p(this);
                    p.translate(canvas()->pos());
                    leg->draw(&p, canvasMap(leg->xAxis()), canvasMap(leg->yAxis()),
                              canvas()->rect());
                }
            }
        }
        return false;
    }

    if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
        if (QWidget *fw = QApplication::focusWidget()) {
            if (qobject_cast<QLineEdit *>(fw) || qobject_cast<QTextEdit *>(fw)
                || qobject_cast<QPlainTextEdit *>(fw) || qobject_cast<QAbstractSpinBox *>(fw)) {
                return false;
            }
        }
    }

    auto isSecondaryMirroringPrimary = [this](int primaryAxis, int secondaryAxis) -> bool {
        if (!axisEnabled(secondaryAxis) || hasVisibleItemOnAxis(secondaryAxis))
            return false;
        const QwtScaleDraw *secSd = axisScaleDraw(secondaryAxis);
        if (secSd && secSd->hasComponent(QwtAbstractScaleDraw::Labels))
            return false;
        const QwtScaleDiv *divPrim = axisScaleDiv(primaryAxis);
        const QwtScaleDiv *divSec = axisScaleDiv(secondaryAxis);
        return divPrim && divSec
                && qAbs(divPrim->lowerBound() - divSec->lowerBound()) <= 1e-12
                && qAbs(divPrim->upperBound() - divSec->upperBound()) <= 1e-12;
    };

    if (object == canvas() || object == this || object == parent()) {
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(e);
            if (!ke->isAutoRepeat()) {
                if (ke->key() == Qt::Key_Space) {
                    d_space_pressed = true;
                    if (!d_is_panning && canvas()
                        && (!g
                            || (!g->zoomOn() && !g->activeTool() && !g->drawLineActive()
                                && !g->drawTextActive()))) {
                        canvas()->setCursor(Qt::OpenHandCursor);
                    }
                } else if (ke->key() == Qt::Key_Z) {
                    d_z_pressed = true;
                } else if (ke->key() == Qt::Key_X) {
                    d_x_pressed = true;
                } else if (ke->key() == Qt::Key_Y) {
                    d_y_pressed = true;
                }
            }
        } else if (e->type() == QEvent::KeyRelease) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(e);
            if (!ke->isAutoRepeat()) {
                if (ke->key() == Qt::Key_Space) {
                    d_space_pressed = false;
                    if (!d_is_panning && canvas())
                        canvas()->setCursor(Qt::ArrowCursor);
                } else if (ke->key() == Qt::Key_Z) {
                    d_z_pressed = false;
                } else if (ke->key() == Qt::Key_X) {
                    d_x_pressed = false;
                } else if (ke->key() == Qt::Key_Y) {
                    d_y_pressed = false;
                }
            }
        } else if (e->type() == QEvent::FocusOut || e->type() == QEvent::WindowDeactivate) {
            d_space_pressed = false;
            d_z_pressed = false;
            d_x_pressed = false;
            d_y_pressed = false;
            if (!d_is_panning && canvas())
                canvas()->setCursor(Qt::ArrowCursor);
        }
    }

    if (object == canvas()) {
        if (g && g->isPiePlot())
            return QwtPlot::eventFilter(object, e);

        switch (e->type()) {
        case QEvent::Wheel: {
            QWheelEvent *we = static_cast<QWheelEvent *>(e);
            const Qt::KeyboardModifiers mods = we->modifiers();
            if (mods & Qt::ControlModifier) {
                we->ignore();
                return false;
            }
            if (g && (g->zoomOn() || g->activeTool()))
                return QwtPlot::eventFilter(object, e);

            int delta = we->angleDelta().y();
            if (delta == 0)
                delta = we->angleDelta().x();
            if (delta == 0)
                return false;

            const double factor = (delta > 0) ? 0.85 : (1.0 / 0.85);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            const QPoint pos = we->position().toPoint();
#else
            const QPoint pos = we->pos();
#endif
            bool zoomX = true;
            bool zoomY = true;
            if (d_x_pressed && !d_y_pressed && !d_z_pressed) {
                zoomX = true;
                zoomY = false;
            } else if (d_y_pressed && !d_x_pressed && !d_z_pressed) {
                zoomX = false;
                zoomY = true;
            } else if (d_z_pressed) {
                zoomX = true;
                zoomY = true;
            } else if (mods & Qt::ShiftModifier) {
                zoomX = true;
                zoomY = false;
            } else if (mods & Qt::AltModifier) {
                zoomX = false;
                zoomY = true;
            }

            MultiLayer *ml = g ? g->multiLayerParent() : nullptr;
            const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();

            if (zoomX) {
                const bool mirrorTop = isSecondaryMirroringPrimary(QwtPlot::xBottom, QwtPlot::xTop);
                zoomAxisAroundPos(QwtPlot::xBottom, pos.x(), factor);
                if (hasVisibleItemOnAxis(QwtPlot::xTop) || mirrorTop)
                    zoomAxisAroundPos(QwtPlot::xTop, pos.x(), factor);
            }
            if (zoomY) {
                const bool mirrorRight = isSecondaryMirroringPrimary(QwtPlot::yLeft, QwtPlot::yRight);
                zoomAxisAroundPos(QwtPlot::yLeft, pos.y(), factor);
                if (hasVisibleItemOnAxis(QwtPlot::yRight) || mirrorRight)
                    zoomAxisAroundPos(QwtPlot::yRight, pos.y(), factor);
            }
            if (g) {
                g->enableAutoscaling(false);
                g->syncChildLinkedLayers();
                replot();
                g->notifyChanges();
            } else {
                replot();
            }
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Zoom Axes"), beforeSnap, 701);
            we->accept();
            return true;
        }

        case QEvent::MouseButtonDblClick: {
            QMouseEvent *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::MiddleButton) {
                d_is_panning = false;
                canvas()->setCursor(d_space_pressed ? Qt::OpenHandCursor : Qt::ArrowCursor);
                MultiLayer *ml = g ? g->multiLayerParent() : nullptr;
                const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
                if (g) {
                    g->setAutoScale();
                } else {
                    for (int a = 0; a < QwtPlot::axisCnt; ++a)
                        setAxisAutoScale(a);
                    replot();
                }
                if (ml && !beforeSnap.isEmpty())
                    ml->pushUndoSnapshot(tr("Autoscale Axes"), beforeSnap);
                me->accept();
                return true;
            }
            break;
        }

        case QEvent::MouseButtonPress: {
            QMouseEvent *me = static_cast<QMouseEvent *>(e);
            const bool isPanTrigger = (me->button() == Qt::MiddleButton)
                    || (me->button() == Qt::LeftButton
                        && ((me->modifiers() & Qt::AltModifier) || d_space_pressed));
            if (isPanTrigger
                && (!g
                    || (!g->zoomOn() && !g->activeTool() && !g->drawLineActive()
                        && !g->drawTextActive()))) {
                d_is_panning = true;
                d_pan_last_pos = me->pos();
                MultiLayer *ml = g ? g->multiLayerParent() : nullptr;
                setProperty("_panUndoSnap", ml ? ml->captureUndoSnapshot() : QString());
                setProperty("_panMoved", false);
                canvas()->setCursor(Qt::ClosedHandCursor);
                me->accept();
                return true;
            }
            break;
        }

        case QEvent::MouseMove: {
            if (d_is_panning) {
                QMouseEvent *me = static_cast<QMouseEvent *>(e);
                if (!(me->buttons() & (Qt::MiddleButton | Qt::LeftButton))) {
                    d_is_panning = false;
                    canvas()->setCursor(d_space_pressed ? Qt::OpenHandCursor : Qt::ArrowCursor);
                    const QString beforeSnap = property("_panUndoSnap").toString();
                    if (property("_panMoved").toBool() && g) {
                        if (MultiLayer *ml = g->multiLayerParent()) {
                            if (!beforeSnap.isEmpty())
                                ml->pushUndoSnapshot(tr("Pan Axes"), beforeSnap, 701);
                        }
                    }
                    setProperty("_panUndoSnap", QString());
                    setProperty("_panMoved", false);
                    return false;
                }
                const QPoint curPos = me->pos();
                if (curPos != d_pan_last_pos) {
                    setProperty("_panMoved", true);
                    const bool mirrorTop =
                            isSecondaryMirroringPrimary(QwtPlot::xBottom, QwtPlot::xTop);
                    const bool mirrorRight =
                            isSecondaryMirroringPrimary(QwtPlot::yLeft, QwtPlot::yRight);
                    panAxisByPixels(QwtPlot::xBottom, d_pan_last_pos.x(), curPos.x());
                    if (hasVisibleItemOnAxis(QwtPlot::xTop) || mirrorTop)
                        panAxisByPixels(QwtPlot::xTop, d_pan_last_pos.x(), curPos.x());
                    panAxisByPixels(QwtPlot::yLeft, d_pan_last_pos.y(), curPos.y());
                    if (hasVisibleItemOnAxis(QwtPlot::yRight) || mirrorRight)
                        panAxisByPixels(QwtPlot::yRight, d_pan_last_pos.y(), curPos.y());
                    d_pan_last_pos = curPos;
                    if (g) {
                        g->enableAutoscaling(false);
                        g->syncChildLinkedLayers();
                        replot();
                        g->notifyChanges();
                    } else {
                        replot();
                    }
                }
                me->accept();
                return true;
            }
            break;
        }

        case QEvent::MouseButtonRelease: {
            if (d_is_panning) {
                QMouseEvent *me = static_cast<QMouseEvent *>(e);
                if (me->button() == Qt::MiddleButton || me->button() == Qt::LeftButton) {
                    d_is_panning = false;
                    canvas()->setCursor(d_space_pressed ? Qt::OpenHandCursor : Qt::ArrowCursor);
                    const QString beforeSnap = property("_panUndoSnap").toString();
                    if (property("_panMoved").toBool() && g) {
                        if (MultiLayer *ml = g->multiLayerParent()) {
                            if (!beforeSnap.isEmpty())
                                ml->pushUndoSnapshot(tr("Pan Axes"), beforeSnap, 701);
                        }
                    }
                    setProperty("_panUndoSnap", QString());
                    setProperty("_panMoved", false);
                    me->accept();
                    return true;
                }
            }
            break;
        }

        default:
            break;
        }
    }
    return QwtPlot::eventFilter(object, e);
}
