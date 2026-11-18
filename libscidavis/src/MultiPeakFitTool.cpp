/***************************************************************************
    File                 : MultiPeakFitTool.cpp
    Project              : SciDAVis / ToriGraph
    --------------------------------------------------------------------
    Copyright            : (C) 2006,2007 by Ion Vasilief,
                           Tilman Benkert, Knut Franke
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net,
                           knut.franke*gmx.de
    Description          : Plot tool for doing multi-peak fitting.

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
#include "MultiPeakFitTool.h"
#include "ApplicationWindow.h"
#include "DataPickerTool.h"
#include "Plot.h"
#include <qwt_plot_curve.h>
#include <qwt_symbol.h>
#include <qwt_text.h>
#include <QApplication>
#include <QKeyEvent>
#include <QLocale>

MultiPeakFitTool::MultiPeakFitTool(Graph *graph, ApplicationWindow *app,
                                   MultiPeakFit::PeakProfile profile, int num_peaks,
                                   const QObject *status_target, const char *status_slot)
    : PlotToolInterface(graph), d_profile(profile), d_num_peaks(num_peaks)
{
    d_selected_peaks = 0;
    d_curve = 0;

    d_fit = new MultiPeakFit(app, d_graph, d_profile, d_num_peaks);
    d_fit->enablePeakCurves(app->generatePeakCurves);
    d_fit->setPeakCurvesColor(app->peakCurvesColor);
    d_fit->generateFunction(app->generateUniformFitPoints, app->fitPoints);

    if (status_target)
        connect(this, SIGNAL(statusText(const QString &)), status_target, status_slot);
    d_picker_tool = new DataPickerTool(d_graph, app, DataPickerTool::Display, this,
                                       SIGNAL(statusText(const QString &)));
    connect(d_picker_tool, SIGNAL(selected(QwtPlotCurve *, int)), this,
            SLOT(selectPeak(QwtPlotCurve *, int)));
    if (d_graph && d_graph->plotWidget() && d_graph->plotWidget()->canvas()) {
        d_graph->plotWidget()->canvas()->installEventFilter(this);
        d_graph->plotWidget()->installEventFilter(this);
        d_graph->plotWidget()->canvas()->grabMouse();
    }
    emit statusText(
            tr("Peak 0/%1 selected - double-click/Enter on a peak point, or press 'Space' to "
               "auto-detect remaining peaks!")
                    .arg(QString::number(d_num_peaks)));
}

MultiPeakFitTool::~MultiPeakFitTool()
{
    if (d_graph && d_graph->plotWidget()) {
        if (d_graph->plotWidget()->canvas())
            d_graph->plotWidget()->canvas()->removeEventFilter(this);
        d_graph->plotWidget()->removeEventFilter(this);
    }
    removePeakMarkers();
    if (d_picker_tool)
        delete d_picker_tool;
    if (d_fit)
        delete d_fit;
    if (d_graph && d_graph->plotWidget() && d_graph->plotWidget()->canvas())
        d_graph->plotWidget()->canvas()->unsetCursor();
}

void MultiPeakFitTool::removePeakMarkers()
{
    if (!d_graph || !d_graph->plotWidget())
        return;
    for (int key : d_marker_keys)
        d_graph->plotWidget()->removeMarker(key);
    d_marker_keys.clear();
}

bool MultiPeakFitTool::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Space) {
            finalize();
            return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            && (!d_picker_tool || !d_picker_tool->selectedCurve())) {
            finalize();
            return true;
        }
        if (ke->key() == Qt::Key_Escape) {
            if (d_graph && d_graph->plotWidget() && d_graph->plotWidget()->canvas())
                d_graph->plotWidget()->canvas()->releaseMouse();
            removePeakMarkers();
            if (d_graph && d_graph->plotWidget())
                d_graph->plotWidget()->replot();
            if (d_graph)
                d_graph->setActiveTool(NULL);
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void MultiPeakFitTool::selectPeak(QwtPlotCurve *curve, int point_index)
{
    if (!curve || !d_graph || d_graph->curveIndex(curve) < 0 || point_index < 0
        || point_index >= curve->dataSize())
        return;
    if (d_curve && d_graph->curveIndex(d_curve) < 0)
        d_curve = nullptr;
    if (d_curve && d_curve != curve)
        return;
    d_curve = curve;

    double xc = curve->x(point_index);
    double yc = curve->y(point_index);
    d_picked_centers.push_back(xc);

    d_fit->setInitialGuess(3 * d_selected_peaks, yc);
    d_fit->setInitialGuess(3 * d_selected_peaks + 1, xc);

    QwtPlotMarker *m = new QwtPlotMarker();
    m->setLineStyle(QwtPlotMarker::VLine);
    m->setLinePen(QPen(Qt::darkGreen, 2, Qt::DashLine));
    m->setSymbol(QwtSymbol(QwtSymbol::DTriangle, QBrush(QColor(0, 180, 0, 180)),
                           QPen(Qt::darkGreen, 1), QSize(10, 10)));
    m->setValue(xc, yc);
    QwtText lbl(tr("Peak %1").arg(d_selected_peaks + 1));
    lbl.setColor(Qt::darkGreen);
    m->setLabel(lbl);
    m->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    int key = d_graph->plotWidget()->insertMarker(m);
    d_marker_keys.append(key);
    d_graph->plotWidget()->replot();

    d_selected_peaks++;
    if (d_selected_peaks == d_num_peaks)
        finalize();
    else
        emit statusText(
                tr("Peak %1/%2 selected (x=%3, y=%4)! Select next peak or press 'Space' to "
                   "auto-detect remaining peaks!")
                        .arg(QString::number(d_selected_peaks))
                        .arg(QString::number(d_num_peaks))
                        .arg(QLocale().toString(xc, 'g', 6))
                        .arg(QLocale().toString(yc, 'g', 6)));
}

void MultiPeakFitTool::finalize()
{
    delete d_picker_tool;
    d_picker_tool = NULL;
    if (d_graph && d_graph->plotWidget() && d_graph->plotWidget()->canvas())
        d_graph->plotWidget()->canvas()->releaseMouse();

    if (d_curve && (!d_graph || d_graph->curveIndex(d_curve) < 0))
        d_curve = nullptr;
    if (!d_curve && d_graph) {
        QString selectedTitle = d_graph->selectedCurveTitle();
        if (!selectedTitle.isEmpty())
            d_curve = d_graph->curve(selectedTitle);
        if (!d_curve && d_graph->curves() > 0)
            d_curve = d_graph->curve(0);
    }

    if (d_graph && d_curve && d_graph->curveIndex(d_curve) >= 0 && d_fit
        && d_fit->setDataFromCurve(d_curve->title().text())) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        d_fit->seedPeaksFromCenters(d_picked_centers);
        d_fit->fit();
        delete d_fit;
        d_fit = NULL;
        QApplication::restoreOverrideCursor();
    }

    removePeakMarkers();
    if (d_graph && d_graph->plotWidget())
        d_graph->plotWidget()->replot();

    if (d_graph)
        d_graph->setActiveTool(NULL);
}
