/***************************************************************************
    File                 : LineProfileTool.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006,2007 by Ion Vasilief,
                           Tilman Benkert, Knut Franke
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net,
                           knut.franke*gmx.de
    Description          : Plot tool for calculating intensity profiles of
                           image markers.

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
#include "LineProfileTool.h"
#include "ApplicationWindow.h"
#include "ImageMarker.h"
#include "ArrowMarker.h"
#include "Graph.h"
#include "Plot.h"

#include <QPoint>
#include <QImage>
#include <QMessageBox>
#include <QPainter>
#include <QMouseEvent>
#include <qwt_plot_canvas.h>

LineProfileTool::LineProfileTool(Graph *graph, int average_pixels)
    : QWidget((graph && graph->plotWidget()) ? graph->plotWidget()->canvas() : nullptr),
      PlotToolInterface(graph),
      d_target(nullptr),
      d_target_key(-1),
      d_dragging(false)
{
    d_op_start = d_op_dp = QPoint(0, 0);
    // make sure we average over an odd number of pixels
    if (average_pixels < 1)
        average_pixels = 1;
    d_average_pixels = (average_pixels % 2) ? average_pixels : average_pixels + 1;
    if (d_graph) {
        const long selKey = d_graph->selectedMarkerKey();
        ImageMarker *img = dynamic_cast<ImageMarker *>(d_graph->selectedMarkerPtr());
        if (img && d_graph->imageMarkerKeys().contains(static_cast<int>(selKey))) {
            d_target = img;
            d_target_key = selKey;
        }
    }
    if (!d_target) {
        if (d_graph && !ApplicationWindow::isNonInteractiveContext(d_graph)) {
            QMessageBox::critical(d_graph->window(), tr("Pixel selection warning"),
                                  tr("Please select an image marker first."));
        }
        hide();
        return;
    }
    d_graph->deselectMarker();
    if (parentWidget())
        setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
    show();
    setFocus();
}

ImageMarker *LineProfileTool::resolveTarget() const
{
    if (!d_graph || !d_graph->plotWidget() || !d_target || d_target_key < 0)
        return nullptr;
    if (!d_graph->imageMarkerKeys().contains(static_cast<int>(d_target_key)))
        return nullptr;
    QwtPlotMarker *mrk = d_graph->plotWidget()->marker(static_cast<int>(d_target_key));
    if (!mrk || mrk != d_target)
        return nullptr;
    return dynamic_cast<ImageMarker *>(mrk);
}

bool LineProfileTool::calculateLineProfile(const QPoint &start, const QPoint &end)
{
    ImageMarker *target = resolveTarget();
    if (!target)
        return false;

    QRect rect = target->rect();
    if (!rect.contains(start) || !rect.contains(end)) {
        if (d_graph && !ApplicationWindow::isNonInteractiveContext(d_graph)) {
            QMessageBox::warning(
                    d_graph, tr("Pixel selection warning"),
                    tr("Please select the end line point inside the image rectangle!"));
        }
        return false;
    }

    QPoint o = target->origin();
    QPixmap pic = target->pixmap();
    QImage image = pic.toImage();
    if (image.isNull() || image.width() <= 0 || image.height() <= 0)
        return false;

    int x1 = start.x() - o.x();
    int x2 = end.x() - o.x();
    int y1 = start.y() - o.y();
    int y2 = end.y() - o.y();

    QSize realSize = pic.size();
    QSize actualSize = target->size();
    if (actualSize.width() <= 0 || actualSize.height() <= 0)
        return false;

    if (realSize != actualSize) {
        double ratioX = (double)realSize.width() / (double)actualSize.width();
        double ratioY = (double)realSize.height() / (double)actualSize.height();
        x1 = int(x1 * ratioX);
        x2 = int(x2 * ratioX);
        y1 = int(y1 * ratioY);
        y2 = int(y2 * ratioY);
    }

    x1 = qBound(0, x1, image.width() - 1);
    x2 = qBound(0, x2, image.width() - 1);
    y1 = qBound(0, y1, image.height() - 1);
    y2 = qBound(0, y2, image.height() - 1);

    Column *pixelCol = new Column(tr("pixel"), SciDAVis::ColumnMode::Numeric);
    Column *xCol = new Column(tr("x"), SciDAVis::ColumnMode::Numeric);
    Column *yCol = new Column(tr("y"), SciDAVis::ColumnMode::Numeric);
    Column *intCol = new Column(tr("intensity"), SciDAVis::ColumnMode::Numeric);
    pixelCol->setPlotDesignation(SciDAVis::X);
    xCol->setPlotDesignation(SciDAVis::Y);
    yCol->setPlotDesignation(SciDAVis::Y);
    intCol->setPlotDesignation(SciDAVis::Y);

    // uses the fast Bresenham's line-drawing algorithm
#define sgn(x) ((x < 0) ? -1 : ((x > 0) ? 1 : 0))
    int i, dx, dy, sdx, sdy, dxabs, dyabs, x, y, px, py;

    dx = x2 - x1; // the horizontal distance of the line
    dy = y2 - y1; // the vertical distance of the line
    dxabs = abs(dx);
    dyabs = abs(dy);
    sdx = sgn(dx);
    sdy = sgn(dy);
    x = dyabs >> 1;
    y = dxabs >> 1;
    px = x1;
    py = y1;

    if (dxabs >= dyabs) // the line is more horizontal than vertical
    {
        for (i = 0; i < dxabs; i++) {
            y += dyabs;
            if (y >= dxabs) {
                y -= dxabs;
                py += sdy;
            }
            px += sdx;

            pixelCol->setValueAt(i, i);
            xCol->setValueAt(i, px);
            yCol->setValueAt(i, py);
            intCol->setValueAt(i, averageImagePixel(image, px, py, true));
        }
    } else // the line is more vertical than horizontal
    {
        for (i = 0; i < dyabs; i++) {
            x += dxabs;
            if (x >= dyabs) {
                x -= dyabs;
                px += sdx;
            }
            py += sdy;

            pixelCol->setValueAt(i, i);
            xCol->setValueAt(i, px);
            yCol->setValueAt(i, py);
            intCol->setValueAt(i, averageImagePixel(image, px, py, false));
        }
    }
    QString caption = tr("Line profile %1").arg(1);
    emit createTablePlot(caption, QString(),
                         QList<Column *>() << pixelCol << xCol << yCol << intCol);
    return true;
}

int LineProfileTool::averageImagePixel(const QImage &image, int px, int py, bool moreHorizontal)
{
    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0)
        return 0;

    int sum = 0;
    int count = 0;
    const int middle = (d_average_pixels - 1) / 2;
    if (moreHorizontal) {
        if (px < 0 || px >= w)
            return 0;
        const int yBegin = qMax(0, py - middle);
        const int yEnd = qMin(h - 1, py - middle + d_average_pixels - 1);
        for (int y = yBegin; y <= yEnd; ++y) {
            sum += qGray(image.pixel(px, y));
            ++count;
        }
    } else {
        if (py < 0 || py >= h)
            return 0;
        const int xBegin = qMax(0, px - middle);
        const int xEnd = qMin(w - 1, px - middle + d_average_pixels - 1);
        for (int x = xBegin; x <= xEnd; ++x) {
            sum += qGray(image.pixel(x, py));
            ++count;
        }
    }
    return (count > 0) ? (sum / count) : 0;
}

void LineProfileTool::addLineMarker(const QPoint &start, const QPoint &end)
{
    if (!d_graph || !d_graph->plotWidget())
        return;
    ArrowMarker *mrk = new ArrowMarker();
    mrk->attach(d_graph->plotWidget());

    mrk->setStartPoint(start);
    mrk->setEndPoint(end);
    mrk->setColor(Qt::red);
    mrk->setWidth(1);
    mrk->setStyle(Qt::SolidLine);
    mrk->drawEndArrow(false);
    mrk->drawStartArrow(false);

    d_graph->addArrow(mrk);
    mrk->detach();
    d_graph->replot();
}

void LineProfileTool::paintEvent(QPaintEvent *)
{
    if (!d_dragging || !resolveTarget())
        return;
    QPainter p(this);
    p.setPen(QPen(Qt::red, 1, Qt::SolidLine));
    p.drawLine(d_op_start, d_op_start + d_op_dp);
}

void LineProfileTool::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        e->ignore();
        return;
    }
    if (!resolveTarget()) {
        if (d_graph)
            d_graph->setActiveTool(NULL);
        return;
    }
    d_op_start = e->pos();
    d_op_dp = QPoint(0, 0);
    d_dragging = true;
    e->accept();
}

void LineProfileTool::mouseMoveEvent(QMouseEvent *e)
{
    if (!d_dragging)
        return;
    if (!resolveTarget()) {
        d_dragging = false;
        if (d_graph)
            d_graph->setActiveTool(NULL);
        return;
    }
    d_op_dp = e->pos() - d_op_start;
    repaint();
    e->accept();
}

void LineProfileTool::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        e->ignore();
        return;
    }
    if (!d_dragging)
        return;
    d_dragging = false;
    const QPoint endPos = e->pos();
    if (resolveTarget() && calculateLineProfile(d_op_start, endPos))
        addLineMarker(d_op_start, endPos);
    if (d_graph)
        d_graph->setActiveTool(NULL);
    // attention: I'm now deleted
}
