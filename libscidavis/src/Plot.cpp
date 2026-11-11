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
#include "ScaleDraw.h"
#include "Spectrogram.h"
#include "PlotCurve.h"

#include <qwt_plot.h>
#include <qwt_painter.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_map.h>
#include <qwt_scale_engine.h>
#include <qwt_text_label.h>

#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>

#include <cmath>
#include <iostream>
using namespace std;

Plot::Plot(QWidget *parent, QString) : QwtPlot(parent)
{
    setAutoReplot(false);

    marker_key = 0;
    curve_key = 0;
    d_is_panning = false;
    d_space_pressed = false;

    minTickLength = 5;
    majTickLength = 9;

    const QFont defaultTickFont("Arial", 12, QFont::Normal, false);
    const QFont defaultTitleFont("Arial", 14, QFont::Bold, false);
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
        yTitle.setFont(defaultTitleFont);
        yTitle.setColor(Qt::black);
        setAxisTitle(QwtPlot::yLeft, yTitle);

        QwtText xTitle(tr("X Axis Title"));
        xTitle.setFont(defaultTitleFont);
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
            title.setFont(defaultTitleFont);
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

    int lw = lineWidth();
    if (lw) {
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
    QwtPlot::drawItems(painter, rect, map, pfilter);

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        if (!axisEnabled(i))
            continue;

        ScaleDraw *sd = (ScaleDraw *)axisScaleDraw(i);
        int majorTicksType = sd->majorTicksStyle();
        int minorTicksType = sd->minorTicksStyle();

        bool min = (minorTicksType == ScaleDraw::In || minorTicksType == ScaleDraw::Both);
        bool maj = (majorTicksType == ScaleDraw::In || majorTicksType == ScaleDraw::Both);

        if (min || maj)
            drawInwardTicks(painter, rect, map[i], i, min, maj);
    }
    Graph *g = qobject_cast<Graph *>(parent());
    if (g && !g->isTernaryPlot() && !g->isPiePlot() && g->axesBackbones()) {
        int apw2 = axesLinewidth() / 2;
        bool clp = painter->hasClipping();
        painter->save();
        painter->setPen(QPen(canvas()->palette().color(QPalette::Active, QPalette::WindowText),
                             axesLinewidth(), Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->setClipping(false);
        const QRect br = rect.adjusted(-(apw2 + 1), -(apw2 + 1), apw2, apw2);
        const bool forceFrame = (pfilter.options() & QwtPlotPrintFilter::PrintFrameWithScales);
        const bool showLeft = g->isAxisBackboneVisible(QwtPlot::yLeft) || forceFrame;
        const bool showRight = g->isAxisBackboneVisible(QwtPlot::yRight) || forceFrame;
        const bool showBottom = g->isAxisBackboneVisible(QwtPlot::xBottom) || forceFrame;
        const bool showTop = g->isAxisBackboneVisible(QwtPlot::xTop) || forceFrame;
        if (showLeft && showRight && showBottom && showTop) {
            painter->drawRect(br);
        } else {
            if (showLeft)
                painter->drawLine(br.topLeft(), br.bottomLeft());
            if (showRight)
                painter->drawLine(br.topRight(), br.bottomRight());
            if (showTop)
                painter->drawLine(br.topLeft(), br.topRight());
            if (showBottom)
                painter->drawLine(br.bottomLeft(), br.bottomRight());
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

    painter->save();
    painter->setPen(QPen(color, axesLinewidth(), Qt::SolidLine));

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
                    QwtPainter::drawLine(painter, x, y, x + minTickLength, y);
            }
            for (j = 0; j < medTicks; j++) {
                y = map.transform(medTickList[j]);
                if (y >= low && y <= high)
                    QwtPainter::drawLine(painter, x, y, x + minTickLength, y);
            }
        }

        if (maj) {
            for (j = 0; j < majTicks; j++) {
                y = map.transform(majTickList[j]);
                if (y >= low && y <= high)
                    QwtPainter::drawLine(painter, x, y, x + majTickLength, y);
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
                    QwtPainter::drawLine(painter, x + 1, y, x - minTickLength, y);
            }
            for (j = 0; j < medTicks; j++) {
                y = map.transform(medTickList[j]);
                if (y >= low && y <= high)
                    QwtPainter::drawLine(painter, x + 1, y, x - minTickLength, y);
            }
        }

        if (maj) {
            for (j = 0; j < majTicks; j++) {
                y = map.transform(majTickList[j]);
                if (y >= low && y <= high)
                    QwtPainter::drawLine(painter, x + 1, y, x - majTickLength, y);
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
                    QwtPainter::drawLine(painter, x, y + 1, x, y - minTickLength);
            }
            for (j = 0; j < medTicks; j++) {
                x = map.transform(medTickList[j]);
                if (x >= low && x <= high)
                    QwtPainter::drawLine(painter, x, y + 1, x, y - minTickLength);
            }
        }

        if (maj) {
            for (j = 0; j < majTicks; j++) {
                x = map.transform(majTickList[j]);
                if (x >= low && x <= high)
                    QwtPainter::drawLine(painter, x, y + 1, x, y - majTickLength);
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
                    QwtPainter::drawLine(painter, x, y, x, y + minTickLength);
            }
            for (j = 0; j < medTicks; j++) {
                x = map.transform(medTickList[j]);
                if (x >= low && x <= high)
                    QwtPainter::drawLine(painter, x, y, x, y + minTickLength);
            }
        }

        if (maj) {
            for (j = 0; j < majTicks; j++) {
                x = map.transform(majTickList[j]);
                if (x >= low && x <= high)
                    QwtPainter::drawLine(painter, x, y, x, y + majTickLength);
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
        const int minRightDim = qMax(20, rightOverhang - margin() + 2);
        axisWidget(QwtPlot::yRight)->scaleDraw()->setMinimumExtent(minRightDim);
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
        const int minTopDim = qMax(8, topOverhang - margin() + 2);
        axisWidget(QwtPlot::xTop)->scaleDraw()->setMinimumExtent(minTopDim);
    }

    QwtPlot::updateLayout();

    if (axisWidget(QwtPlot::xBottom))
        axisWidget(QwtPlot::xBottom)->clearMask();
    if (axisWidget(QwtPlot::xTop))
        axisWidget(QwtPlot::xTop)->clearMask();
}

void Plot::replot()
{
    updateAxes();
    updateLayout();
    QwtPlot::replot();
}

void Plot::print(QPainter *painter, const QRect &plotRect, const QwtPlotPrintFilter &pfilter) const
{
    const_cast<Plot *>(this)->updateLayout();
    printFrame(painter, plotRect);
    const int m = qMax(margin(), 14);
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
    if (!axisEnabled(QwtPlot::xTop) && title().isEmpty()) {
        int s = 0, e = 0;
        if (axisEnabled(QwtPlot::yLeft) && axisWidget(QwtPlot::yLeft))
            axisWidget(QwtPlot::yLeft)->getBorderDistHint(s, e);
        mTop = qMax(mTop, s + 6);
    }

    const QRect innerRect =
            (plotRect.width() > (mLeft + mRight) && plotRect.height() > (mTop + mBottom))
            ? plotRect.adjusted(mLeft, mTop, -mRight, -mBottom)
            : plotRect;
    QwtPlot::print(painter, innerRect, pfilter);
}

QwtPlotCurve *Plot::curve(int index)
{
    QwtPlotItem *it = d_curves.value(index);
    if (it && it->rtti() != QwtPlotItem::Rtti_PlotSpectrogram)
        return (QwtPlotCurve *)it;
    else
        return 0;
}

int Plot::closestCurve(int xpos, int ypos, int &dist, int &point)
{
    QwtScaleMap map[QwtPlot::axisCnt];
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++)
        map[axis] = canvasMap(axis);

    double dmin = 1.0e10;
    int key = -1;
    for (QMap<int, QwtPlotItem *>::iterator iter = d_curves.begin(); iter != d_curves.end();
         ++iter) {
        QwtPlotItem *item = (QwtPlotItem *)iter.value();
        if (!item)
            continue;

        if (item->rtti() != QwtPlotItem::Rtti_PlotSpectrogram) {
            PlotCurve *c = (PlotCurve *)item;
            for (int i = 0; i < c->dataSize(); i++) {
                double cx = map[c->xAxis()].xTransform(c->x(i)) - double(xpos);
                double cy = map[c->yAxis()].xTransform(c->y(i)) - double(ypos);
                double f = qwtSqr(cx) + qwtSqr(cy);
                if (f < dmin && c->type() != Graph::ErrorBars) {
                    dmin = f;
                    key = iter.key();
                    point = i;
                }
            }
        }
    }
    dist = int(sqrt(dmin));
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
    double centerVal = map.invTransform(pixelPos);
    const bool isLog = (se->transformation()->type() == QwtScaleTransformation::Log10);

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
    const double vStart = map.invTransform(startPixel);
    const double vEnd = map.invTransform(endPixel);
    const bool isLog = (se->transformation()->type() == QwtScaleTransformation::Log10);

    double newMin = minVal;
    double newMax = maxVal;
    double step = 0.0;
    Graph *g = qobject_cast<Graph *>(parent());
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
    if (object == canvas()) {
        Graph *g = qobject_cast<Graph *>(parent());
        if (g && g->isPiePlot())
            return QwtPlot::eventFilter(object, e);

        switch (e->type()) {
        case QEvent::Wheel: {
            if (g && (g->zoomOn() || g->activeTool()))
                return QwtPlot::eventFilter(object, e);

            QWheelEvent *we = static_cast<QWheelEvent *>(e);
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
            const Qt::KeyboardModifiers mods = we->modifiers();
            bool zoomX = true;
            bool zoomY = true;
            if (mods & Qt::ShiftModifier) {
                zoomX = true;
                zoomY = false;
            } else if (mods & (Qt::ControlModifier | Qt::AltModifier)) {
                zoomX = false;
                zoomY = true;
            }

            if (zoomX) {
                zoomAxisAroundPos(QwtPlot::xBottom, pos.x(), factor);
                zoomAxisAroundPos(QwtPlot::xTop, pos.x(), factor);
            }
            if (zoomY) {
                zoomAxisAroundPos(QwtPlot::yLeft, pos.y(), factor);
                zoomAxisAroundPos(QwtPlot::yRight, pos.y(), factor);
            }
            replot();
            if (g)
                g->notifyChanges();
            we->accept();
            return true;
        }

        case QEvent::KeyPress: {
            QKeyEvent *ke = static_cast<QKeyEvent *>(e);
            if (ke->key() == Qt::Key_Space && !ke->isAutoRepeat())
                d_space_pressed = true;
            break;
        }

        case QEvent::KeyRelease: {
            QKeyEvent *ke = static_cast<QKeyEvent *>(e);
            if (ke->key() == Qt::Key_Space && !ke->isAutoRepeat())
                d_space_pressed = false;
            break;
        }

        case QEvent::MouseButtonDblClick: {
            QMouseEvent *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::MiddleButton) {
                d_is_panning = false;
                canvas()->setCursor(Qt::ArrowCursor);
                if (g) {
                    g->setAutoScale();
                } else {
                    for (int a = 0; a < QwtPlot::axisCnt; ++a)
                        setAxisAutoScale(a);
                    replot();
                }
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
                    canvas()->setCursor(Qt::ArrowCursor);
                    return false;
                }
                const QPoint curPos = me->pos();
                panAxisByPixels(QwtPlot::xBottom, d_pan_last_pos.x(), curPos.x());
                panAxisByPixels(QwtPlot::xTop, d_pan_last_pos.x(), curPos.x());
                panAxisByPixels(QwtPlot::yLeft, d_pan_last_pos.y(), curPos.y());
                panAxisByPixels(QwtPlot::yRight, d_pan_last_pos.y(), curPos.y());
                d_pan_last_pos = curPos;
                replot();
                if (g)
                    g->notifyChanges();
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
                    canvas()->setCursor(Qt::ArrowCursor);
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
