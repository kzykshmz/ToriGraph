#include "OleServer.h"
#include "MultiLayer.h"
/***************************************************************************
    File                 : Graph.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Graph widget

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

#include <QVarLengthArray>

#include "Graph.h"
#include "Grid.h"
#include "CanvasPicker.h"
#include "QwtErrorPlotCurve.h"
#include "Legend.h"
#include "ArrowMarker.h"
#include "ScalePicker.h"
#include "TitlePicker.h"
#include "QwtPieCurve.h"
#include "ImageMarker.h"
#include "QwtBarCurve.h"
#include "BoxCurve.h"
#include "QwtHistogram.h"
#include "VectorCurve.h"
#include "ScaleDraw.h"
#include "ColorButton.h"
#include "PatternBox.h"
#include "SymbolBox.h"
#include "FunctionCurve.h"
#include "Spectrogram.h"
#include "SelectionMoveResizer.h"
#include "RangeSelectorTool.h"
#include "PlotCurve.h"
#include "ApplicationWindow.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "core/column/Column.h"
#include "MultiLayer.h"
#include "EmfPaintDevice.h"
#include "MyParser.h"
#include <algorithm>

#include <QApplication>
#include <QBitmap>
#include <QClipboard>
#include <QCursor>
#include <QFile>
#include <QImage>
#include <QMessageBox>
#include <QPainterPath>
#include <QPixmap>
#include <QPainter>
#include <QMenu>
#include <QTextStream>
#include <QLocale>
#include <QPrintDialog>
#include <QImageWriter>
#include <QFileInfo>
#include <QRegExp>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QColorDialog>
#include <QInputDialog>
#include <QSettings>

#if QT_VERSION >= 0x040300
#include <QSvgGenerator>
#endif

#include <qwt_painter.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_zoomer.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_engine.h>
#include <qwt_text.h>
#include <qwt_text_label.h>
#include <qwt_color_map.h>

#include <stdexcept>

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
using namespace std;

Graph::Graph(QWidget *parent, QString name, Qt::WindowFlags f) : QWidget(parent, f)
{
    if (name.isEmpty())
        setObjectName("graph");
    else
        setObjectName(name);

    n_curves = 0;
    d_active_tool = NULL;
    widthLine = 2;
    selectedMarker = -1;
    drawTextOn = false;
    drawLineOn = false;
    drawArrowOn = false;
    ignoreResize = true;
    drawAxesBackbone = true;
    m_autoscale = true;
    d_is_ternary = false;
    autoScaleFonts = false;
    hidden_size = QSize();
    d_antialiasing = true;
    d_scale_on_print = true;
    d_print_cropmarks = false;

    defaultArrowLineWidth = 1;
    defaultArrowColor = QColor(Qt::black);
    defaultArrowLineStyle = Qt::SolidLine;
    defaultArrowHeadLength = 4;
    defaultArrowHeadAngle = 45;
    defaultArrowHeadFill = true;

    defaultMarkerFont = QFont("Arial", 14, QFont::Normal, false);
    auxMrkFont = QFont("Arial", 12, QFont::Normal, false);
    defaultMarkerFrame = 1;
    defaultTextMarkerColor = QColor(Qt::black);
    defaultTextMarkerBackground = QColor(255, 255, 255, 255);
    for (int i = 0; i < 4; ++i)
        d_axis_backbone[i] = true;

    d_user_step = QVector<double>(QwtPlot::axisCnt);
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        axisType << AxisType::Numeric;
        axesFormatInfo << QString();
        axesFormulas << QString();
        d_user_step[i] = 0.0;
    }

    d_plot = new Plot(this);
    initFonts(QFont("Arial", 18, QFont::Normal, false), QFont("Arial", 16, QFont::Normal, false));
    {
        QwtText t = d_plot->title();
        t.setFont(QFont("Arial", 18, QFont::Bold, false));
        d_plot->setTitle(t);
    }
    cp = new CanvasPicker(this);

    titlePicker = new TitlePicker(d_plot);
    scalePicker = new ScalePicker(d_plot);

    d_zoomer[0] = new QwtPlotZoomer(QwtPlot::xBottom, QwtPlot::yLeft,
                                    QwtPicker::DragSelection | QwtPicker::CornerToCorner,
                                    QwtPicker::AlwaysOff, d_plot->canvas());
    d_zoomer[0]->setRubberBandPen(QPen(Qt::black));
    d_zoomer[1] = new QwtPlotZoomer(QwtPlot::xTop, QwtPlot::yRight,
                                    QwtPicker::DragSelection | QwtPicker::CornerToCorner,
                                    QwtPicker::AlwaysOff, d_plot->canvas());
    zoom(false);

    setGeometry(0, 0, 500, 500);
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(d_plot);
    setMouseTracking(true);

    legendMarkerID = -1; // no legend for an empty graph
    d_texts = QVector<int>();
    c_type = QVector<int>();
    c_keys = QVector<int>();

    connect(cp, SIGNAL(selectPlot()), this, SLOT(activateGraph()));
    connect(cp, SIGNAL(drawTextOff()), this, SIGNAL(drawTextOff()));
    connect(cp, SIGNAL(viewImageDialog()), this, SIGNAL(viewImageDialog()));
    connect(cp, SIGNAL(viewTextDialog()), this, SIGNAL(viewTextDialog()));
    connect(cp, SIGNAL(viewLineDialog()), this, SIGNAL(viewLineDialog()));
    connect(cp, SIGNAL(showPlotDialog(int)), this, SIGNAL(showPlotDialog(int)));
    connect(cp, SIGNAL(showMarkerPopupMenu()), this, SIGNAL(showMarkerPopupMenu()));
    connect(cp, SIGNAL(modified()), this, SIGNAL(modifiedGraph()));

    connect(titlePicker, SIGNAL(showTitleMenu()), this, SLOT(showTitleContextMenu()));
    connect(titlePicker, SIGNAL(doubleClicked()), this, SIGNAL(viewTitleDialog()));
    connect(titlePicker, SIGNAL(removeTitle()), this, SLOT(removeTitle()));
    connect(titlePicker, SIGNAL(clicked()), this, SLOT(selectTitle()));

    connect(scalePicker, SIGNAL(clicked()), this, SLOT(activateGraph()));
    connect(scalePicker, SIGNAL(clicked()), this, SLOT(deselectMarker()));
    connect(scalePicker, SIGNAL(axisDblClicked(int)), this, SIGNAL(axisDblClicked(int)));
    connect(scalePicker, SIGNAL(axisTitleRightClicked(int)), this, SLOT(showAxisTitleMenu(int)));
    connect(scalePicker, SIGNAL(axisRightClicked(int)), this, SLOT(showAxisContextMenu(int)));
    connect(scalePicker, SIGNAL(xAxisTitleDblClicked()), this, SIGNAL(xAxisTitleDblClicked()));
    connect(scalePicker, SIGNAL(yAxisTitleDblClicked()), this, SIGNAL(yAxisTitleDblClicked()));
    connect(scalePicker, SIGNAL(rightAxisTitleDblClicked()), this,
            SIGNAL(rightAxisTitleDblClicked()));
    connect(scalePicker, SIGNAL(topAxisTitleDblClicked()), this, SIGNAL(topAxisTitleDblClicked()));

    connect(d_zoomer[0], SIGNAL(zoomed(const QwtDoubleRect &)), this,
            SLOT(zoomed(const QwtDoubleRect &)));
}

void Graph::notifyChanges()
{
    updateStatReferenceLines();
    syncChildLinkedLayers();
    emit modifiedGraph();
}

void Graph::activateGraph()
{
    emit selectedGraph(this);
    setFocus();
}

void Graph::deselectMarker()
{
    selectedMarker = -1;
    if (d_markers_selector)
        delete d_markers_selector;
}

long Graph::selectedMarkerKey()
{
    return selectedMarker;
}

QwtPlotMarker *Graph::selectedMarkerPtr()
{
    return d_plot->marker(selectedMarker);
}

void Graph::setSelectedMarker(long mrk, bool add)
{
    selectedMarker = mrk;
    if (add) {
        if (d_markers_selector) {
            if (d_texts.contains(mrk))
                d_markers_selector->add((Legend *)d_plot->marker(mrk));
            else if (d_lines.contains(mrk))
                d_markers_selector->add((ArrowMarker *)d_plot->marker(mrk));
            else if (d_images.contains(mrk))
                d_markers_selector->add((ImageMarker *)d_plot->marker(mrk));
        } else {
            if (d_texts.contains(mrk))
                d_markers_selector = new SelectionMoveResizer((Legend *)d_plot->marker(mrk));
            else if (d_lines.contains(mrk))
                d_markers_selector = new SelectionMoveResizer((ArrowMarker *)d_plot->marker(mrk));
            else if (d_images.contains(mrk))
                d_markers_selector = new SelectionMoveResizer((ImageMarker *)d_plot->marker(mrk));
            else
                return;
            connect(d_markers_selector, SIGNAL(targetsChanged()), this, SIGNAL(modifiedGraph()));
        }
    } else {
        if (d_texts.contains(mrk)) {
            if (d_markers_selector) {
                if (d_markers_selector->contains((Legend *)d_plot->marker(mrk)))
                    return;
                delete d_markers_selector;
            }
            d_markers_selector = new SelectionMoveResizer((Legend *)d_plot->marker(mrk));
        } else if (d_lines.contains(mrk)) {
            if (d_markers_selector) {
                if (d_markers_selector->contains((ArrowMarker *)d_plot->marker(mrk)))
                    return;
                delete d_markers_selector;
            }
            d_markers_selector = new SelectionMoveResizer((ArrowMarker *)d_plot->marker(mrk));
        } else if (d_images.contains(mrk)) {
            if (d_markers_selector) {
                if (d_markers_selector->contains((ImageMarker *)d_plot->marker(mrk)))
                    return;
                delete d_markers_selector;
            }
            d_markers_selector = new SelectionMoveResizer((ImageMarker *)d_plot->marker(mrk));
        } else
            return;
        connect(d_markers_selector, SIGNAL(targetsChanged()), this, SIGNAL(modifiedGraph()));
    }
}

void Graph::initFonts(const QFont &scaleTitleFnt, const QFont &numbersFnt)
{
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_plot->setAxisFont(i, numbersFnt);
        QwtText t = d_plot->axisTitle(i);
        t.setFont(scaleTitleFnt);
        d_plot->setAxisTitle(i, t);
    }
}

void Graph::setAxisFont(int axis, const QFont &fnt)
{
    d_plot->setAxisFont(axis, fnt);
    d_plot->replot();
    emit modifiedGraph();
}

QFont Graph::axisFont(int axis)
{
    return d_plot->axisFont(axis);
}

void Graph::enableAxis(int axis, bool on)
{
    d_plot->enableAxis(axis, on);
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale)
        scale->setMargin(0);

    scalePicker->refresh();
}

void Graph::enableAxes(const QStringList &list)
{
    int i;
    for (i = 0; i < QwtPlot::axisCnt; i++) {
        bool ok = list[i + 1].toInt();
        d_plot->enableAxis(i, ok);
    }

    for (i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale)
            scale->setMargin(0);
    }
    scalePicker->refresh();
}

void Graph::enableAxes(QVector<bool> axesOn)
{
    for (int i = 0; i < QwtPlot::axisCnt; i++)
        d_plot->enableAxis(i, axesOn[i]);

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale)
            scale->setMargin(0);
    }
    scalePicker->refresh();
}

QVector<bool> Graph::enabledAxes()
{
    QVector<bool> axesOn(4);
    for (int i = 0; i < QwtPlot::axisCnt; i++)
        axesOn[i] = d_plot->axisEnabled(i);
    return axesOn;
}

QList<int> Graph::axesBaseline()
{
    QList<int> baselineDist;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale)
            baselineDist << scale->margin();
        else
            baselineDist << 0;
    }
    return baselineDist;
}

void Graph::setAxesBaseline(const QList<int> &lst)
{
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale)
            scale->setMargin(lst[i]);
    }
}

void Graph::setAxesBaseline(QStringList &lst)
{
    lst.removeAll(lst.first());
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale)
            scale->setMargin((lst[i]).toInt());
    }
}

QList<Graph::AxisType> Graph::axesType()
{
    return axisType;
}

void Graph::setLabelsNumericFormat(int axis, int format, int prec, const QString &formula)
{
    axisType[axis] = AxisType::Numeric;
    axesFormulas[axis] = formula;

    ScaleDraw *sd_old = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const QwtScaleDiv div = sd_old->scaleDiv();

    if (format == Plot::Superscripts) {
        QwtSupersciptsScaleDraw *sd = new QwtSupersciptsScaleDraw(
                *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                formula.toUtf8().constData());
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->setLabelFormat('s', prec);
        sd->setScaleDiv(div);
        d_plot->setAxisScaleDraw(axis, sd);
    } else {
        ScaleDraw *sd = new ScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                                      formula.toUtf8().constData());
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->setScaleDiv(div);

        if (format == Plot::Automatic)
            sd->setLabelFormat('g', prec);
        else if (format == Plot::Scientific)
            sd->setLabelFormat('e', prec);
        else if (format == Plot::Decimal)
            sd->setLabelFormat('f', prec);

        d_plot->setAxisScaleDraw(axis, sd);
    }
}

void Graph::setLabelsNumericFormat(int axis, const QStringList &l)
{
    QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
    if (!sd->hasComponent(QwtAbstractScaleDraw::Labels) || axisType[axis] != AxisType::Numeric)
        return;

    int format = l[2 * axis].toInt();
    int prec = l[2 * axis + 1].toInt();
    setLabelsNumericFormat(axis, format, prec, axesFormulas[axis]);
}

void Graph::setLabelsNumericFormat(const QStringList &l)
{
    for (int axis = 0; axis < 4; axis++)
        setLabelsNumericFormat(axis, l);
}

QString Graph::saveAxesLabelsType()
{
    QString s = "AxisType\t";
    for (int i = 0; i < 4; i++) {
        auto type = axisType[i];
        s += QString::number(static_cast<int>(type));
        if (type == AxisType::Time || type == AxisType::Date || type == AxisType::DateTime
            || type == AxisType::Txt || type == AxisType::ColHeader || type == AxisType::Day
            || type == AxisType::Month)
            s += ";" + axesFormatInfo[i];
        s += "\t";
    };

    return s + "\n";
}

QString Graph::saveTicksType()
{
    QList<int> ticksTypeList = d_plot->getMajorTicksType();
    QString s = "MajorTicks\t";
    int i;
    for (i = 0; i < 4; i++)
        s += QString::number(ticksTypeList[i]) + "\t";
    s += "\n";

    ticksTypeList = d_plot->getMinorTicksType();
    s += "MinorTicks\t";
    for (i = 0; i < 4; i++)
        s += QString::number(ticksTypeList[i]) + "\t";

    return s + "\n";
}

QStringList Graph::enabledTickLabels()
{
    QStringList lst;
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        const QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
        lst << QString::number(sd->hasComponent(QwtAbstractScaleDraw::Labels));
    }
    return lst;
}

QString Graph::saveEnabledTickLabels()
{
    QString s = "EnabledTickLabels\t";
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        const QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
        s += QString::number(sd->hasComponent(QwtAbstractScaleDraw::Labels)) + "\t";
    }
    return s + "\n";
}

QString Graph::saveLabelsFormat()
{
    QString s = "LabelsFormat\t";
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        s += QString::number(d_plot->axisLabelFormat(axis)) + "\t";
        s += QString::number(d_plot->axisLabelPrecision(axis)) + "\t";
    }
    return s + "\n";
}

QString Graph::saveAxesBaseline()
{
    QString s = "AxesBaseline\t";
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale)
            s += QString::number(scale->margin()) + "\t";
        else
            s += "0\t";
    }
    return s + "\n";
}

QString Graph::saveLabelsRotation()
{
    QString s = "LabelsRotation\t";
    s += QString::number(labelsRotation(QwtPlot::xBottom)) + "\t";
    s += QString::number(labelsRotation(QwtPlot::xTop)) + "\n";
    return s;
}

void Graph::setEnabledTickLabels(const QStringList &labelsOn)
{
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        QwtScaleWidget *sc = d_plot->axisWidget(axis);
        if (sc) {
            QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
            sd->enableComponent(QwtAbstractScaleDraw::Labels, labelsOn[axis] == "1");
        }
    }
}

void Graph::setMajorTicksType(const QList<int> &lst)
{
    if (d_plot->getMajorTicksType() == lst)
        return;

    for (int i = 0; i < (int)lst.count(); i++) {
        ScaleDraw *sd = (ScaleDraw *)d_plot->axisScaleDraw(i);
        if (lst[i] == ScaleDraw::None || lst[i] == ScaleDraw::In)
            sd->enableComponent(QwtAbstractScaleDraw::Ticks, false);
        else {
            sd->enableComponent(QwtAbstractScaleDraw::Ticks);
            sd->setTickLength(QwtScaleDiv::MinorTick, d_plot->minorTickLength());
            sd->setTickLength(QwtScaleDiv::MediumTick, d_plot->minorTickLength());
            sd->setTickLength(QwtScaleDiv::MajorTick, d_plot->majorTickLength());
        }
        sd->setMajorTicksStyle((ScaleDraw::TicksStyle)lst[i]);
    }
}

void Graph::setMajorTicksType(const QStringList &lst)
{
    for (int i = 0; i < (int)lst.count(); i++)
        d_plot->setMajorTicksType(i, lst[i].toInt());
}

void Graph::setMinorTicksType(const QList<int> &lst)
{
    if (d_plot->getMinorTicksType() == lst)
        return;

    for (int i = 0; i < (int)lst.count(); i++)
        d_plot->setMinorTicksType(i, lst[i]);
}

void Graph::setMinorTicksType(const QStringList &lst)
{
    for (int i = 0; i < (int)lst.count(); i++)
        d_plot->setMinorTicksType(i, lst[i].toInt());
}

int Graph::minorTickLength()
{
    return d_plot->minorTickLength();
}

int Graph::majorTickLength()
{
    return d_plot->majorTickLength();
}

void Graph::setAxisTicksLength(int axis, int majTicksType, int minTicksType, int minLength,
                               int majLength)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (!scale)
        return;

    d_plot->setTickLength(minLength, majLength);

    ScaleDraw *sd = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    sd->setMajorTicksStyle((ScaleDraw::TicksStyle)majTicksType);
    sd->setMinorTicksStyle((ScaleDraw::TicksStyle)minTicksType);

    if (majTicksType == ScaleDraw::None && minTicksType == ScaleDraw::None)
        sd->enableComponent(QwtAbstractScaleDraw::Ticks, false);
    else if (!sd->hasComponent(QwtAbstractScaleDraw::Labels)
             && (majTicksType == ScaleDraw::None || majTicksType == ScaleDraw::In)
             && (minTicksType == ScaleDraw::None || minTicksType == ScaleDraw::In))
        sd->enableComponent(QwtAbstractScaleDraw::Ticks, false);
    else
        sd->enableComponent(QwtAbstractScaleDraw::Ticks);

    if (majTicksType == ScaleDraw::None || majTicksType == ScaleDraw::In)
        majLength = minLength;
    if (minTicksType == ScaleDraw::None || minTicksType == ScaleDraw::In)
        minLength = 0;

    sd->setTickLength(QwtScaleDiv::MinorTick, minLength);
    sd->setTickLength(QwtScaleDiv::MediumTick, minLength);
    sd->setTickLength(QwtScaleDiv::MajorTick, majLength);
}

void Graph::setTicksLength(int minLength, int majLength)
{
    QList<int> majTicksType = d_plot->getMajorTicksType();
    QList<int> minTicksType = d_plot->getMinorTicksType();

    for (int i = 0; i < 4; i++)
        setAxisTicksLength(i, majTicksType[i], minTicksType[i], minLength, majLength);
}

void Graph::changeTicksLength(int minLength, int majLength)
{
    if (d_plot->minorTickLength() == minLength && d_plot->majorTickLength() == majLength)
        return;

    setTicksLength(minLength, majLength);

    d_plot->hide();
    for (int i = 0; i < 4; i++) {
        if (d_plot->axisEnabled(i)) {
            d_plot->enableAxis(i, false);
            d_plot->enableAxis(i, true);
        }
    }
    d_plot->replot();
    d_plot->show();

    emit modifiedGraph();
}

void Graph::showAxis(int axis, AxisType type, const QString &formatInfo, Table *table, bool axisOn,
                     int majTicksType, int minTicksType, bool labelsOn, const QColor &c, int format,
                     int prec, int rotation, int baselineDist, const QString &formula,
                     const QColor &labelsColor)
{
    d_plot->enableAxis(axis, axisOn);
    if (!axisOn)
        return;

    QList<int> majTicksTypeList = d_plot->getMajorTicksType();
    QList<int> minTicksTypeList = d_plot->getMinorTicksType();

    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);

    if (d_plot->axisEnabled(axis) == axisOn && majTicksTypeList[axis] == majTicksType
        && minTicksTypeList[axis] == minTicksType && axesColors()[axis] == COLORNAME(c)
        && axesNumColors()[axis] == COLORNAME(labelsColor)
        && prec == d_plot->axisLabelPrecision(axis) && format == d_plot->axisLabelFormat(axis)
        && labelsRotation(axis) == rotation && axisType[axis] == type
        && axesFormatInfo[axis] == formatInfo && axesFormulas[axis] == formula
        && scale->margin() == baselineDist
        && sclDraw->hasComponent(QwtAbstractScaleDraw::Labels) == labelsOn)
        return;

    scale->setMargin(baselineDist);
    QPalette pal = scale->palette();
    if (pal.color(QPalette::Active, QPalette::WindowText) != c)
        pal.setColor(QPalette::WindowText, c);
    if (pal.color(QPalette::Active, QPalette::Text) != labelsColor)
        pal.setColor(QPalette::Text, labelsColor);
    scale->setPalette(pal);

    if (!labelsOn)
        sclDraw->enableComponent(QwtAbstractScaleDraw::Labels, false);
    else {
        switch (type) {
        case AxisType::Numeric:
            setLabelsNumericFormat(axis, format, prec, formula);
            break;
        case AxisType::Day:
            setLabelsDayFormat(axis, format);
            break;
        case AxisType::Month:
            setLabelsMonthFormat(axis, format);
            break;
        case AxisType::Time:
        case AxisType::Date:
        case AxisType::DateTime:
            setLabelsDateTimeFormat(axis, type, formatInfo);
            break;
        case AxisType::Txt:
            setLabelsTextFormat(axis, table, formatInfo);
            break;
        case AxisType::ColHeader:
            setLabelsColHeaderFormat(axis, table);
            break;
        }

        setAxisLabelRotation(axis, rotation);
    }

    sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const bool backboneOn = drawAxesBackbone && (axis >= 0 && axis < 4 ? d_axis_backbone[axis] : true);
    sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);

    setAxisTicksLength(axis, majTicksType, minTicksType, d_plot->minorTickLength(),
                       d_plot->majorTickLength());

    if (axisOn && (axis == QwtPlot::xTop || axis == QwtPlot::yRight))
        updateSecondaryAxis(axis); // synchronize scale divisions

    scalePicker->refresh();
    d_plot->updateLayout(); // This is necessary in order to enable/disable tick labels
    scale->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setLabelsDayFormat(int axis, int format)
{
    axisType[axis] = AxisType::Day;
    axesFormatInfo[axis] = QString::number(format);

    ScaleDraw *sd_old = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const QwtScaleDiv div = sd_old->scaleDiv();

    WeekDayScaleDraw *sd =
            new WeekDayScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                                 (WeekDayScaleDraw::NameFormat)format);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    sd->setScaleDiv(div);
    d_plot->setAxisScaleDraw(axis, sd);
}

void Graph::setLabelsMonthFormat(int axis, int format)
{
    axisType[axis] = AxisType::Month;
    axesFormatInfo[axis] = QString::number(format);

    ScaleDraw *sd_old = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const QwtScaleDiv div = sd_old->scaleDiv();

    MonthScaleDraw *sd =
            new MonthScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                               (MonthScaleDraw::NameFormat)format);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    sd->setScaleDiv(div);
    d_plot->setAxisScaleDraw(axis, sd);
}

void Graph::setLabelsTextFormat(int axis, const Column *column, int startRow, int endRow)
{
    // TODO: The whole text labels functionality is very limited. Specifying
    // only one column for the labels will always mean that the labels
    // correspond to 1, 2, 3, 4, etc.. Other label mappings such as
    // a -> 1.5, b -> 4.5, c -> 16.5 (with step set to 0.5) or similar
    // require to have an additional column with numeric values.
    // This should be supported in our new plotting framework.
    if (!column)
        return;
    future::Table *table = qobject_cast<future::Table *>(column->parentAspect());
    if (!table)
        return;
    if (axis < 0 || axis > 3)
        return;

    axisType[axis] = AxisType::Txt;
    axesFormatInfo[axis] = table->name() + "_" + column->name();

    QMap<int, QString> list;
    for (int row = startRow; row <= endRow; row++)
        if (!column->isInvalid(row))
            list.insert(row + 1, column->textAt(row));
    QwtTextScaleDraw *sd = new QwtTextScaleDraw(
            *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)), list);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    d_plot->setAxisScaleDraw(axis, sd);
}

void Graph::setLabelsTextFormat(int axis, Table *table, const QString &columnName)
{
    Column *col = 0;
    if (!table || !(col = table->column(columnName))) {
        ApplicationWindow *aw = nullptr;
        for (QObject *p = parent(); p && !aw; p = p->parent())
            aw = qobject_cast<ApplicationWindow *>(p);
        if (!aw) {
            for (QWidget *tlw : QApplication::topLevelWidgets()) {
                if ((aw = qobject_cast<ApplicationWindow *>(tlw)) != nullptr)
                    break;
            }
        }
        if (aw && (aw->isHeadlessMode() || aw->isOleEmbeddedMode() || aw->batchMode()))
            return;

        QMessageBox::critical(
                this, tr("Internal Error"),
                tr("<html>Failed to set axis labels on Graph %1. Maybe you're trying to open a "
                   "corrupted"
                   " project file; or there's some problem within SciDAVis. Please report this"
                   " as a bug (together with detailed instructions how to reproduce this message or"
                   " the corrupted file).<p>"
                   "<a href=\"https://sourceforge.net/tracker/?group_id=199120&atid=968214>\">"
                   "bug tracker: "
                   "https://sourceforge.net/tracker/?group_id=199120&atid=968214</a></html>")
                        .arg(objectName()));
        return;
    }
    setLabelsTextFormat(axis, col, 0, col->rowCount() - 1);
}

void Graph::setLabelsColHeaderFormat(int axis, Table *table)
{
    if (!table)
        return;

    axisType[axis] = AxisType::ColHeader;
    axesFormatInfo[axis] = table->name();

    QMap<int, QString> list;
    for (int col = 0; col < table->columnCount(); col++)
        if (table->colPlotDesignation(col) == SciDAVis::Y)
            list.insert(col, table->colLabel(col));
    QwtTextScaleDraw *sd = new QwtTextScaleDraw(
            *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)), list);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    d_plot->setAxisScaleDraw(axis, sd);
}

void Graph::setLabelsDateTimeFormat(int axis, AxisType type, const QString &formatInfo)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList list = formatInfo.split(";", Qt::KeepEmptyParts);
#else
    QStringList list = formatInfo.split(";", QString::KeepEmptyParts);
#endif
    if ((int)list.count() < 2 || list[1].isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("Couldn't change the axis type to the requested format!"));
        }
        return;
    }

    const bool backboneOn = drawAxesBackbone && (axis >= 0 && axis < 4 ? d_axis_backbone[axis] : true);
    switch (type) {
    case AxisType::Time : {
        TimeScaleDraw *sd = new TimeScaleDraw(
                *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                list[0].isEmpty() ? QTime(12, 0, 0, 0) : QTime::fromString(list[0].right(8)),
                list[1]);
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);
        d_plot->setAxisScaleDraw(axis, sd);
        break;
    }
    case AxisType::Date: {
        DateScaleDraw *sd =
                new DateScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                                  QDate::fromString(list[0].left(10), "yyyy-MM-dd"), list[1]);
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);
        d_plot->setAxisScaleDraw(axis, sd);
        break;
    }
    case AxisType::DateTime: {
        DateTimeScaleDraw *sd = new DateTimeScaleDraw(
                *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                QDateTime::fromString(list[0], "yyyy-MM-ddThh:mm:ss"), list[1]);
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);
        d_plot->setAxisScaleDraw(axis, sd);
        break;
    }
    default:
        // safeguard - such an argument is invalid for this method
        return;
    }

    axisType[axis] = type;
    axesFormatInfo[axis] = formatInfo;
}

void Graph::setAxisLabelRotation(int axis, int rotation)
{
    if (axis == QwtPlot::xBottom) {
        if (rotation > 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignRight | Qt::AlignVCenter);
        else if (rotation < 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignLeft | Qt::AlignVCenter);
        else if (rotation == 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignHCenter | Qt::AlignBottom);
    } else if (axis == QwtPlot::xTop) {
        if (rotation > 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignLeft | Qt::AlignVCenter);
        else if (rotation < 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignRight | Qt::AlignVCenter);
        else if (rotation == 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignHCenter | Qt::AlignTop);
    }
    d_plot->setAxisLabelRotation(axis, (double)rotation);
}

int Graph::labelsRotation(int axis)
{
    ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    return (int)sclDraw->labelRotation();
}

void Graph::setYAxisTitleFont(const QFont &fnt)
{
    QwtText t = d_plot->axisTitle(QwtPlot::yLeft);
    t.setFont(fnt);
    d_plot->setAxisTitle(QwtPlot::yLeft, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setXAxisTitleFont(const QFont &fnt)
{
    QwtText t = d_plot->axisTitle(QwtPlot::xBottom);
    t.setFont(fnt);
    d_plot->setAxisTitle(QwtPlot::xBottom, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setRightAxisTitleFont(const QFont &fnt)
{
    QwtText t = d_plot->axisTitle(QwtPlot::yRight);
    t.setFont(fnt);
    d_plot->setAxisTitle(QwtPlot::yRight, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTopAxisTitleFont(const QFont &fnt)
{
    QwtText t = d_plot->axisTitle(QwtPlot::xTop);
    t.setFont(fnt);
    d_plot->setAxisTitle(QwtPlot::xTop, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setAxisTitleFont(int axis, const QFont &fnt)
{
    QwtText t = d_plot->axisTitle(axis);
    t.setFont(fnt);
    d_plot->setAxisTitle(axis, t);
    d_plot->replot();
    emit modifiedGraph();
}

QFont Graph::axisTitleFont(int axis)
{
    return d_plot->axisTitle(axis).font();
}

QColor Graph::axisTitleColor(int axis)
{
    QColor c;
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale)
        c = scale->title().color();
    return c;
}

void Graph::setAxesNumColors(const QStringList &colors)
{
    for (int i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            QPalette pal = scale->palette();
            pal.setColor(QPalette::Text, QColor(COLORVALUE(colors[i])));
            scale->setPalette(pal);
        }
    }
}

void Graph::setAxesColors(const QStringList &colors)
{
    for (int i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            QPalette pal = scale->palette();
            pal.setColor(QPalette::WindowText, QColor(COLORVALUE(colors[i])));
            scale->setPalette(pal);
        }
    }
}

QString Graph::saveAxesColors()
{
    QString s = "AxesColors\t";
    QStringList colors, numColors;
    QPalette pal;
    int i;
    for (i = 0; i < 4; i++) {
        colors << COLORNAME(QColor(Qt::black));
        numColors << COLORNAME(QColor(Qt::black));
    }

    for (i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            pal = scale->palette();
            colors[i] = COLORNAME(pal.color(QPalette::Active, QPalette::WindowText));
            numColors[i] = COLORNAME(pal.color(QPalette::Active, QPalette::Text));
        }
    }
    s += colors.join("\t") + "\n";
    s += "AxesNumberColors\t" + numColors.join("\t") + "\n";
    return s;
}

QStringList Graph::axesColors()
{
    QStringList colors;
    QPalette pal;
    int i;
    for (i = 0; i < 4; i++)
        colors << COLORNAME(QColor(Qt::black));

    for (i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            pal = scale->palette();
            colors[i] = COLORNAME(pal.color(QPalette::Active, QPalette::WindowText));
        }
    }
    return colors;
}

QColor Graph::axisColor(int axis)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale)
        return scale->palette().color(QPalette::Active, QPalette::WindowText);
    else
        return QColor(Qt::black);
}

QColor Graph::axisNumbersColor(int axis)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale)
        return scale->palette().color(QPalette::Active, QPalette::Text);
    else
        return QColor(Qt::black);
}

QStringList Graph::axesNumColors()
{
    QStringList colors;
    QPalette pal;
    int i;
    for (i = 0; i < 4; i++)
        colors << COLORNAME(QColor(Qt::black));

    for (i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            pal = scale->palette();
            colors[i] = COLORNAME(pal.color(QPalette::Active, QPalette::Text));
        }
    }
    return colors;
}

void Graph::setTitleColor(const QColor &c)
{
    QwtText t = d_plot->title();
    t.setColor(c);
    d_plot->setTitle(t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTitleAlignment(int align)
{
    QwtText t = d_plot->title();
    t.setRenderFlags(align);
    d_plot->setTitle(t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTitleFont(const QFont &fnt)
{
    QwtText t = d_plot->title();
    t.setFont(fnt);
    d_plot->setTitle(t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setYAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::yLeft, text);
}

void Graph::setXAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::xBottom, text);
}

void Graph::setRightAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::yRight, text);
}

void Graph::setTopAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::xTop, text);
}

int Graph::axisTitleAlignment(int axis)
{
    return d_plot->axisTitle(axis).renderFlags();
}

void Graph::setAxesTitlesAlignment(const QStringList &align)
{
    for (int i = 0; i < 4; i++) {
        QwtText t = d_plot->axisTitle(i);
        t.setRenderFlags(align[i + 1].toInt());
        d_plot->setAxisTitle(i, t);
    }
}

void Graph::setXAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::xBottom);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::xBottom, t);

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setYAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::yLeft);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::yLeft, t);

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTopAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::xTop);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::xTop, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setRightAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::yRight);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::yRight, t);

    d_plot->replot();
    emit modifiedGraph();
}

QString Graph::resolvedAutoAxisTitle(int axis, Column::UnitFormat fmt) const
{
    if (axis < 0 || axis >= 4 || isPiePlot() || isTernaryPlot())
        return QString();

    QList<Column *> xBottomCols, xTopCols, yLeftCols, yRightCols;
    int detectedStyle = -1;

    for (int i = 0; i < n_curves; ++i) {
        QwtPlotCurve *pc = curve(i);
        if (!pc || pc->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        const int cType = (i < c_type.size()) ? c_type[i] : -1;
        if (cType == ErrorBars)
            continue;
        if (detectedStyle < 0)
            detectedStyle = cType;

        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (!dc || !dc->table())
            continue;
        Table *t = dc->table();
        const int xIdx = t->colIndex(dc->xColumnName());
        const int yIdx = t->colIndex(dc->yColumnName());
        if (Column *xc = (xIdx >= 0 ? t->column(xIdx) : nullptr)) {
            QList<Column *> &dst = (pc->xAxis() == QwtPlot::xTop) ? xTopCols : xBottomCols;
            if (!dst.contains(xc))
                dst << xc;
        }
        if (Column *yc = (yIdx >= 0 ? t->column(yIdx) : nullptr)) {
            QList<Column *> &dst = (pc->yAxis() == QwtPlot::yRight) ? yRightCols : yLeftCols;
            if (!dst.contains(yc))
                dst << yc;
        }
    }

    auto cleanColAxisTitle = [fmt](Column *col) -> QString {
        if (!col)
            return QString();
        QString t = col->formattedAxisTitle(fmt).trimmed();
        if (t.isEmpty())
            t = col->name().trimmed();
        return t;
    };

    auto resolveMultiCol = [&](const QList<Column *> &cols) -> QString {
        if (cols.isEmpty())
            return QString();
        if (cols.size() == 1)
            return cleanColAxisTitle(cols.first());
        const QString firstFull = cleanColAxisTitle(cols.first());
        bool allSameFull = !firstFull.isEmpty();
        const QString firstUnits = cols.first()->units().trimmed();
        bool allSameNonEmptyUnits = !firstUnits.isEmpty();
        for (int i = 1; i < cols.size(); ++i) {
            if (cleanColAxisTitle(cols[i]) != firstFull)
                allSameFull = false;
            if (cols[i]->units().trimmed() != firstUnits)
                allSameNonEmptyUnits = false;
        }
        if (allSameFull || allSameNonEmptyUnits)
            return firstFull;
        return QString();
    };

    if (detectedStyle == Histogram) {
        return (axis == QwtPlot::xBottom) ? resolveMultiCol(yLeftCols) : QString();
    }
    if (detectedStyle == Box) {
        return (axis == QwtPlot::yLeft) ? resolveMultiCol(yLeftCols) : QString();
    }
    if (detectedStyle == HorizontalBars) {
        if (axis == QwtPlot::xBottom)
            return resolveMultiCol(yLeftCols);
        if (axis == QwtPlot::yLeft)
            return resolveMultiCol(xBottomCols);
        return QString();
    }

    switch (axis) {
    case QwtPlot::xBottom:
        return resolveMultiCol(xBottomCols);
    case QwtPlot::xTop:
        return resolveMultiCol(xTopCols);
    case QwtPlot::yLeft:
        return resolveMultiCol(yLeftCols);
    case QwtPlot::yRight:
        return resolveMultiCol(yRightCols);
    default:
        return QString();
    }
}

bool Graph::matchesAutoAxisTitleCandidate(int axis, const QString &text) const
{
    QString plain = text.trimmed();
    if (plain.startsWith("<b>") && plain.endsWith("</b>") && plain.length() >= 7)
        plain = plain.mid(3, plain.length() - 7).trimmed();

    if (plain.isEmpty() || plain == QStringLiteral("X Axis Title")
        || plain == QStringLiteral("Y Axis Title") || plain == tr("X Axis Title")
        || plain == tr("Y Axis Title"))
        return true;

    const QString parenCandidate = resolvedAutoAxisTitle(axis, Column::UnitFormat::Paren);
    if (!parenCandidate.isEmpty() && plain == parenCandidate)
        return true;

    const QString slashCandidate = resolvedAutoAxisTitle(axis, Column::UnitFormat::ISIJSlash);
    if (!slashCandidate.isEmpty() && plain == slashCandidate)
        return true;

    for (int i = 0; i < n_curves; ++i) {
        DataCurve *dc = dynamic_cast<DataCurve *>(curve(i));
        if (!dc || !dc->table())
            continue;
        Table *t = dc->table();
        const QString colName = (axis == QwtPlot::xBottom || axis == QwtPlot::xTop)
                ? dc->xColumnName()
                : dc->yColumnName();
        const int idx = t->colIndex(colName);
        if (Column *col = (idx >= 0 ? t->column(idx) : nullptr)) {
            if (plain == col->name().trimmed() || plain == col->longName().trimmed())
                return true;
        }
    }
    return false;
}

void Graph::setAxisTitleAuto(int axis, bool autoSync)
{
    if (axis >= 0 && axis < 4)
        d_axis_title_auto[axis] = autoSync;
}

bool Graph::isAxisTitleAuto(int axis) const
{
    return (axis >= 0 && axis < 4) ? d_axis_title_auto[axis] : false;
}

void Graph::setAxisUnitFormat(Column::UnitFormat fmt)
{
    if (d_axis_unit_format == fmt)
        return;
    d_axis_unit_format = fmt;
    refreshAutoAxisTitlesFromColumns();
}

void Graph::refreshAutoAxisTitlesFromColumns()
{
    if (!d_plot || isPiePlot() || isTernaryPlot())
        return;

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        QFont af = axisTitleFont(a);
        if (af.family().trimmed().isEmpty()) {
            af.setFamily(QStringLiteral("Arial"));
            setAxisTitleFont(a, af);
        }
    }

    QSet<int> colorBarAxes;
    const QwtPlotItemList &items = d_plot->itemList();
    for (QwtPlotItem *it : items) {
        if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = static_cast<Spectrogram *>(it);
            if (sp->hasColorScale())
                colorBarAxes.insert(sp->colorScaleAxis());
        }
    }

    const bool prevRefreshing = d_refreshing_auto_titles;
    d_refreshing_auto_titles = true;
    bool anyChanged = false;

    for (int a = 0; a < 4; ++a) {
        if (!d_axis_title_auto[a] || colorBarAxes.contains(a))
            continue;
        if ((a == QwtPlot::xTop || a == QwtPlot::yRight) && !d_plot->axisEnabled(a))
            continue;

        QString nextTitle = resolvedAutoAxisTitle(a, d_axis_unit_format);
        if (nextTitle.isEmpty())
            continue;

        const QString curTitle = d_plot->axisTitle(a).text();
        const QString trimmedCur = curTitle.trimmed();
        if (trimmedCur.startsWith("<b>") && trimmedCur.endsWith("</b>"))
            nextTitle = QStringLiteral("<b>") + nextTitle + QStringLiteral("</b>");

        if (curTitle != nextTitle) {
            QwtText t = d_plot->axisTitle(a);
            QFont f = t.font();
            if (f.family().trimmed().isEmpty())
                f = QFont(QStringLiteral("Arial"), 14, QFont::Bold, false);
            t.setFont(f);
            t.setText(nextTitle);
            d_plot->setAxisTitle(a, t);
            anyChanged = true;
        }
    }

    if (Legend *leg = legend()) {
        QStringList lines = leg->text().split('\n');
        bool legChanged = false;
        for (int li = 0; li < lines.size(); ++li) {
            const QString line = lines[li];
            if (!line.startsWith("\\c{"))
                continue;
            const int closeBrace = line.indexOf('}');
            if (closeBrace <= 3)
                continue;
            bool ok = false;
            const int oneBased = line.mid(3, closeBrace - 3).toInt(&ok);
            if (!ok || oneBased < 1 || oneBased > n_curves)
                continue;
            const int ci = oneBased - 1;
            DataCurve *dc = dynamic_cast<DataCurve *>(curve(ci));
            if (!dc || !dc->table())
                continue;
            Table *t = dc->table();
            const int yIdx = t->colIndex(dc->yColumnName());
            Column *yc = (yIdx >= 0) ? t->column(yIdx) : nullptr;
            if (!yc)
                continue;

            const QString currentLabel = line.mid(closeBrace + 1);
            const QString colShort = yc->name().trimmed();
            const QString colLong = yc->longName().trimmed();
            const QString desiredLabel = !colLong.isEmpty() ? colLong : colShort;
            const QString prevAuto = d_last_auto_curve_labels.value(ci);

            const QString pfx = QString(t->name()) + "_";
            const QString yColFull = dc->yColumnName();
            const bool hasStrippedTitle =
                    yColFull.startsWith(pfx) && yColFull.length() > pfx.length();
            const QString strippedTitle = hasStrippedTitle ? yColFull.mid(pfx.length()) : QString();

            if (currentLabel == colShort || (hasStrippedTitle && currentLabel == strippedTitle)
                || (!colLong.isEmpty() && currentLabel == yColFull)
                || (!prevAuto.isEmpty() && currentLabel == prevAuto)) {
                d_last_auto_curve_labels[ci] = desiredLabel;
                const QString updatedLine =
                        QStringLiteral("\\c{%1}%2").arg(oneBased).arg(desiredLabel);
                if (lines[li] != updatedLine) {
                    lines[li] = updatedLine;
                    legChanged = true;
                }
            } else if (currentLabel == desiredLabel) {
                d_last_auto_curve_labels[ci] = desiredLabel;
            }
        }
        if (legChanged) {
            leg->setText(lines.join("\n"));
            anyChanged = true;
        }
    }

    d_refreshing_auto_titles = prevRefreshing;
    if (anyChanged) {
        d_plot->replot();
        emit modifiedGraph();
    }
}

void Graph::setAxisTitle(int axis, const QString &text)
{
    QString finalText = text;
    if (axis >= 0 && axis < 4 && !d_refreshing_auto_titles) {
        d_axis_title_auto[axis] = matchesAutoAxisTitleCandidate(axis, text);
        if (d_axis_title_auto[axis] && d_axis_unit_format == Column::UnitFormat::ISIJSlash) {
            const QString slashTitle = resolvedAutoAxisTitle(axis, Column::UnitFormat::ISIJSlash);
            if (!slashTitle.isEmpty()) {
                const QString trimmed = text.trimmed();
                finalText = (trimmed.startsWith("<b>") && trimmed.endsWith("</b>"))
                        ? (QStringLiteral("<b>") + slashTitle + QStringLiteral("</b>"))
                        : slashTitle;
            }
        }
    }

    QwtText t = d_plot->axisTitle(axis);
    QFont f = t.font();
    if (f.family().trimmed().isEmpty())
        f = QFont("Arial", 14, QFont::Bold, false);
    t.setFont(f);
    t.setText(finalText);
    d_plot->setAxisTitle(axis, t);
    d_plot->replot();
    emit modifiedGraph();
}

QStringList Graph::scalesTitles()
{
    QStringList scaleTitles;
    int axis;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        switch (i) {
        case 0:
            axis = 2;
            scaleTitles << d_plot->axisTitle(axis).text();
            break;

        case 1:
            axis = 0;
            scaleTitles << d_plot->axisTitle(axis).text();
            break;

        case 2:
            axis = 3;
            scaleTitles << d_plot->axisTitle(axis).text();
            break;

        case 3:
            axis = 1;
            scaleTitles << d_plot->axisTitle(axis).text();
            break;
        }
    }
    return scaleTitles;
}

void Graph::updateSecondaryAxis(int axis)
{
    for (int i = 0; i < n_curves; i++) {
        QwtPlotItem *it = plotItem(i);
        if (!it)
            continue;

        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = (Spectrogram *)it;
            if (sp->colorScaleAxis() == axis)
                return;
        }

        if ((axis == QwtPlot::yRight && it->yAxis() == QwtPlot::yRight)
            || (axis == QwtPlot::xTop && it->xAxis() == QwtPlot::xTop))
            return;
    }

    int a = QwtPlot::xBottom;
    if (axis == QwtPlot::yRight)
        a = QwtPlot::yLeft;

    if (!d_plot->axisEnabled(a))
        return;

    QwtScaleEngine *se = d_plot->axisScaleEngine(a);
    const QwtScaleDiv *sd = d_plot->axisScaleDiv(a);

    QwtScaleEngine *sc_engine = 0;
    if (se->transformation()->type() == QwtScaleTransformation::Log10)
        sc_engine = new QwtLog10ScaleEngine();
    else
        sc_engine = new QwtLinearScaleEngine();

    if (se->testAttribute(QwtScaleEngine::Inverted))
        sc_engine->setAttribute(QwtScaleEngine::Inverted);

    d_plot->setAxisScaleEngine(axis, sc_engine);
    d_plot->setAxisScaleDiv(axis, *sd);

    if (d_axis_breaks[a].enabled && !d_axis_breaks[axis].enabled) {
        ScaleDraw *mirrorSd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis));
        if (mirrorSd)
            mirrorSd->setAxisBreakIndicator(true, d_axis_breaks[a].breakFrom);
    }

    d_user_step[axis] = d_user_step[a];

    QwtScaleWidget *scale = d_plot->axisWidget(a);
    int start = scale->startBorderDist();
    int end = scale->endBorderDist();

    scale = d_plot->axisWidget(axis);
    scale->setMinBorderDist(start, end);
}

void Graph::setAutoScale()
{
    for (int i = 0; i < QwtPlot::axisCnt; i++)
        d_plot->setAxisAutoScale(i);

    d_plot->replot();
    d_zoomer[0]->setZoomBase();
    d_zoomer[1]->setZoomBase();
    updateScale();
    if (d_linked_parent)
        syncLinkedAxesFromParent();
    syncChildLinkedLayers();

    emit modifiedGraph();
}

void Graph::setScale(int axis, double start, double end, double step, int majorTicks,
                     int minorTicks, int type, bool inverted)
{
    QwtScaleEngine *sc_engine = 0;
    if (type)
        sc_engine = new QwtLog10ScaleEngine();
    else
        sc_engine = new QwtLinearScaleEngine();

    int max_min_intervals = minorTicks;
    if (minorTicks == 1)
        max_min_intervals = 3;
    if (minorTicks > 1)
        max_min_intervals = minorTicks + 1;

    QwtScaleDiv div = sc_engine->divideScale(qMin(start, end), qMax(start, end), majorTicks,
                                             max_min_intervals, step);
    d_plot->setAxisMaxMajor(axis, majorTicks);
    d_plot->setAxisMaxMinor(axis, minorTicks);

    if (inverted) {
        sc_engine->setAttribute(QwtScaleEngine::Inverted);
        div.invert();
    }

    d_plot->setAxisScaleEngine(axis, sc_engine);
    d_plot->setAxisScaleDiv(axis, div);

    if (type && axisType[axis] == AxisType::Numeric
        && d_plot->axisLabelFormat(axis) == Plot::Automatic
        && d_plot->axisScaleDraw(axis)->hasComponent(QwtAbstractScaleDraw::Labels)) {
        setLabelsNumericFormat(axis, Plot::Superscripts, 0, axesFormulas[axis]);
    } else if (!type && axisType[axis] == AxisType::Numeric
               && d_plot->axisLabelFormat(axis) == Plot::Superscripts
               && d_plot->axisScaleDraw(axis)->hasComponent(QwtAbstractScaleDraw::Labels)) {
        setLabelsNumericFormat(axis, Plot::Automatic, 4, axesFormulas[axis]);
    }

    if (!type && d_axis_breaks[axis].enabled) {
        setAxisBreak(axis, d_axis_breaks[axis]);
    }

    d_zoomer[0]->setZoomBase();
    d_zoomer[1]->setZoomBase();

    d_user_step[axis] = step;

    if (axis == QwtPlot::xBottom || axis == QwtPlot::yLeft) {
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
    }

    d_plot->replot();
    if (!ignoreResize && parent() && parent()->parent()) {
        MultiLayer *ml = qobject_cast<MultiLayer *>(parent()->parent()->parent());
        if (ml && ml->keepAspectRatio())
            ml->arrangeLayers(false, false);
    }
    // keep markers on canvas area
    updateMarkersBoundingRect();
    d_plot->replot();
    syncChildLinkedLayers();
}

QStringList Graph::analysableCurvesList()
{
    QStringList cList;
    QList<int> keys = d_plot->curveKeys();
    for (int i = 0; i < (int)keys.count(); i++) {
        QwtPlotCurve *c = d_plot->curve(keys[i]);
        if (c && c_type[i] != ErrorBars)
            cList << c->title().text();
    }
    return cList;
}

QStringList Graph::curvesList()
{
    QStringList cList;
    QList<int> keys = d_plot->curveKeys();
    for (int i = 0; i < (int)keys.count(); i++) {
        QwtPlotCurve *c = d_plot->curve(keys[i]);
        if (c)
            cList << c->title().text();
    }
    return cList;
}

QStringList Graph::plotItemsList() const
{
    QStringList cList;
    QList<int> keys = d_plot->curveKeys();
    for (int i = 0; i < (int)keys.count(); i++) {
        QwtPlotItem *it = d_plot->plotItem(keys[i]);
        if (it)
            cList << it->title().text();
    }
    return cList;
}

void Graph::copyImage()
{
    QImage image(d_plot->size(), QImage::Format_ARGB32);
    exportPainter(image);
    QApplication::clipboard()->setImage(image);
}

void Graph::exportToFile(const QString &fileName)
{
    if (fileName.isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(this))
            QMessageBox::critical(this, tr("Error"), tr("Please provide a valid file name!"));
        return;
    }

    const QString lowerName = fileName.toLower();
    if (lowerName.endsWith(".eps") || lowerName.endsWith(".pdf") || lowerName.endsWith(".ps")) {
        exportVector(fileName);
        return;
    } else if (lowerName.endsWith(".svg")) {
        exportSVG(fileName);
        return;
    } else if (lowerName.endsWith(".emf")) {
        exportImage(fileName);
        return;
    } else {
        QList<QByteArray> list = QImageWriter::supportedImageFormats();
        for (int i = 0; i < list.count(); i++) {
            if (lowerName.endsWith("." + QString::fromLatin1(list[i]).toLower())) {
                exportImage(fileName);
                return;
            }
        }
        if (!ApplicationWindow::isNonInteractiveContext(this))
            QMessageBox::critical(this, tr("Error"), tr("File format not handled, operation aborted!"));
    }
}

void Graph::exportImage(const QString &fileName, int quality, bool transparent, int dpi)
{
    if (fileName.endsWith(".emf", Qt::CaseInsensitive)) {
#ifdef Q_OS_WIN
        OleRenderedGraph emf = SciDAVisEmf::renderMultiLayer(multiLayerParent(), this);
        if (!emf.emfBytes.isEmpty()) {
            QFile f(fileName);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(emf.emfBytes);
                f.close();
            }
        }
#endif
        return;
    }

    const int effectiveDpi = (dpi > 0) ? dpi : 96;
    const double scaleFactor = double(effectiveDpi) / 96.0;
    const QSize baseSize = (d_plot && !d_plot->size().isEmpty()) ? d_plot->size() : size();
    const QSize targetSize(qMax(1, qRound(baseSize.width() * scaleFactor)),
                           qMax(1, qRound(baseSize.height() * scaleFactor)));

    QImage image(targetSize, QImage::Format_ARGB32);
    image.fill(transparent ? Qt::transparent : Qt::white);

    QPalette origPlotPal;
    bool origAutoFill = false;
    QColor origCanvasBg;
    if (transparent && d_plot) {
        origPlotPal = d_plot->palette();
        origAutoFill = d_plot->autoFillBackground();
        origCanvasBg = d_plot->canvasBackground();
        QPalette transPal = origPlotPal;
        QColor winCol = transPal.color(QPalette::Window);
        winCol.setAlpha(0);
        transPal.setColor(QPalette::Window, winCol);
        d_plot->setPalette(transPal);
        d_plot->setAutoFillBackground(false);
        QColor cbg = origCanvasBg;
        cbg.setAlpha(0);
        d_plot->setCanvasBackground(cbg);
    }

    {
        QPainter p(&image);
        exportPainter(p, false, QRect(QPoint(0, 0), baseSize), targetSize);
        p.end();
    }

    if (transparent && d_plot) {
        d_plot->setPalette(origPlotPal);
        d_plot->setAutoFillBackground(origAutoFill);
        d_plot->setCanvasBackground(origCanvasBg);
    }

    const int dpm = qRound(double(effectiveDpi) / 0.0254);
    image.setDotsPerMeterX(dpm);
    image.setDotsPerMeterY(dpm);

    QImageWriter writer(fileName);
    if (fileName.endsWith(".tif", Qt::CaseInsensitive)
        || fileName.endsWith(".tiff", Qt::CaseInsensitive)) {
        writer.setCompression(1);
    }
    if (quality >= 0)
        writer.setQuality(quality);
    writer.write(image);
}

void Graph::exportVector(const QString &fileName, int res, bool color, bool keepAspect,
                         QPageSize pageSize, QPageLayout::Orientation orientation)
{
    if (fileName.isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(this))
            QMessageBox::critical(this, tr("Error"), tr("Please provide a valid file name!"));
        return;
    }

    QPrinter printer;
    printer.setCreator("ToriGraph");
    printer.setFullPage(true);
    if (res > 0)
        printer.setResolution(res);

    printer.setOutputFileName(fileName);
    if (fileName.contains(".eps")) {
#if QT_VERSION >= 0x050000
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("Output in postscript format is not available for Qt5, using PDF"));
        }
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName + ".pdf");
#else
        printer.setOutputFormat(QPrinter::PostScriptFormat);
#endif
    }

    if (color)
        printer.setColorMode(QPrinter::Color);
    else
        printer.setColorMode(QPrinter::GrayScale);

    if (pageSize == QPageSize(QPageSize::Custom)) {
        const QSize basePx = (d_plot && !d_plot->size().isEmpty()) ? d_plot->size() : size();
        const QSizeF ptSize(basePx.width() * 72.0 / 96.0, basePx.height() * 72.0 / 96.0);
        printer.setPageSize(QPageSize(ptSize, QPageSize::Point));
    } else {
        printer.setPageOrientation(orientation);
        printer.setPageSize(pageSize);
    }

    const QSize basePx = (d_plot && !d_plot->size().isEmpty()) ? d_plot->size() : size();
    exportPainter(printer, keepAspect, QRect(QPoint(0, 0), basePx));
}

void Graph::print()
{
    QPrinter printer;
    printer.setColorMode(QPrinter::Color);
    printer.setFullPage(true);

    // printing should preserve plot aspect ratio, if possible
    double aspect = double(d_plot->width()) / double(d_plot->height());
    if (aspect < 1)
        printer.setPageOrientation(QPageLayout::Portrait);
    else
        printer.setPageOrientation(QPageLayout::Landscape);

    QPrintDialog printDialog(&printer);
    if (printDialog.exec() == QDialog::Accepted) {
        QRect plotRect = d_plot->rect();
        QRect paperRect = printer.pageLayout().fullRectPixels(printer.resolution());
        if (d_scale_on_print) {
            int dpiy = printer.logicalDpiY();
            int margin = (int)((2 / 2.54) * dpiy); // 2 cm margins

            int width = qRound(aspect * printer.height()) - 2 * margin;
            int x = qRound(abs(printer.width() - width) * 0.5);

            plotRect = QRect(x, margin, width, printer.height() - 2 * margin);
            if (x < margin) {
                plotRect.setLeft(margin);
                plotRect.setWidth(printer.width() - 2 * margin);
            }
        } else {
            int x_margin = (paperRect.width() - plotRect.width()) / 2;
            int y_margin = (paperRect.height() - plotRect.height()) / 2;
            plotRect.moveTo(x_margin, y_margin);
        }

        QPainter paint(&printer);
        if (d_print_cropmarks) {
            QRect cr = plotRect; // cropmarks rectangle
            cr.adjust(-1, -1, 2, 2);
            paint.save();
            paint.setPen(QPen(QColor(Qt::black), 0.5, Qt::DashLine));
            paint.drawLine(paperRect.left(), cr.top(), paperRect.right(), cr.top());
            paint.drawLine(paperRect.left(), cr.bottom(), paperRect.right(), cr.bottom());
            paint.drawLine(cr.left(), paperRect.top(), cr.left(), paperRect.bottom());
            paint.drawLine(cr.right(), paperRect.top(), cr.right(), paperRect.bottom());
            paint.restore();
        }

        print(&paint, plotRect);
    }
}

void Graph::exportSVG(const QString &fname)
{
#if QT_VERSION >= 0x040300
    QSvgGenerator svg;
    svg.setFileName(fname);
#if QT_VERSION >= 0x040500
    svg.setSize(d_plot->size());
    svg.setViewBox(d_plot->rect());
    svg.setResolution(96); // FIXME hardcored
    svg.setTitle(this->objectName());
#endif
    exportPainter(svg);
#endif
}

void Graph::exportPainter(QPaintDevice &paintDevice, bool keepAspect, QRect rect)
{
    QPainter p(&paintDevice);
    exportPainter(p, keepAspect, rect, QSize(paintDevice.width(), paintDevice.height()));
}

void Graph::exportPainter(QPainter &painter, bool keepAspect, QRect rect, QSize size)
{
    if (size == QSize())
        size = d_plot->size();
    if (rect == QRect())
        rect = QRect(QPoint(0, 0), size);
    if (keepAspect) {
        QSize scaled = rect.size();
        scaled.scale(size, Qt::KeepAspectRatio);
        size = scaled;
    }

    const bool prevExporting = d_is_exporting;
    d_is_exporting = true;
    painter.save();
    painter.scale((double)size.width() / (double)rect.width(),
                  (double)size.height() / (double)rect.height());

    print(&painter, rect);
    painter.restore();
    d_is_exporting = prevExporting;
}

int Graph::selectedCurveID()
{
    if (d_range_selector)
        return curveKey(curveIndex(d_range_selector->selectedCurve()));
    else
        return -1;
}

QString Graph::selectedCurveTitle()
{
    if (d_range_selector)
        return d_range_selector->selectedCurve()->title().text();
    else
        return {};
}

bool Graph::markerSelected()
{
    return (selectedMarker >= 0);
}

void Graph::removeMarker()
{
    if (selectedMarker >= 0) {
        if (d_markers_selector) {
            if (d_texts.contains(selectedMarker))
                d_markers_selector->removeAll((Legend *)d_plot->marker(selectedMarker));
            else if (d_lines.contains(selectedMarker))
                d_markers_selector->removeAll((ArrowMarker *)d_plot->marker(selectedMarker));
            else if (d_images.contains(selectedMarker))
                d_markers_selector->removeAll((ImageMarker *)d_plot->marker(selectedMarker));
        }
        d_plot->removeMarker(selectedMarker);
        d_plot->replot();
        emit modifiedGraph();

        if (selectedMarker == legendMarkerID)
            legendMarkerID = -1;

        if (d_lines.contains(selectedMarker)) {
            int index = d_lines.indexOf(selectedMarker);
            int last_line_marker = (int)d_lines.size() - 1;
            for (int i = index; i < last_line_marker; i++)
                d_lines[i] = d_lines[i + 1];
            d_lines.resize(last_line_marker);
        } else if (d_texts.contains(selectedMarker)) {
            int index = d_texts.indexOf(selectedMarker);
            int last_text_marker = d_texts.size() - 1;
            for (int i = index; i < last_text_marker; i++)
                d_texts[i] = d_texts[i + 1];
            d_texts.resize(last_text_marker);
        } else if (d_images.contains(selectedMarker)) {
            int index = d_images.indexOf(selectedMarker);
            int last_image_marker = d_images.size() - 1;
            for (int i = index; i < last_image_marker; i++)
                d_images[i] = d_images[i + 1];
            d_images.resize(last_image_marker);
        }
        selectedMarker = -1;
    }
}

void Graph::cutMarker()
{
    copyMarker();
    removeMarker();
}

bool Graph::arrowMarkerSelected()
{
    return (d_lines.contains(selectedMarker));
}

bool Graph::imageMarkerSelected()
{
    return (d_images.contains(selectedMarker));
}

void Graph::copyMarker()
{
    if (selectedMarker < 0) {
        selectedMarkerType = None;
        return;
    }

    if (d_lines.contains(selectedMarker)) {
        ArrowMarker *mrkL = (ArrowMarker *)d_plot->marker(selectedMarker);
        auxMrkStart = mrkL->startPoint();
        auxMrkEnd = mrkL->endPoint();
        selectedMarkerType = Arrow;
    } else if (d_images.contains(selectedMarker)) {
        ImageMarker *mrkI = (ImageMarker *)d_plot->marker(selectedMarker);
        auxMrkStart = mrkI->origin();
        QRect rect = mrkI->rect();
        auxMrkEnd = rect.bottomRight();
        auxMrkFileName = mrkI->fileName();
        selectedMarkerType = Image;
    } else
        selectedMarkerType = Text;
}

void Graph::pasteMarker()
{
    if (selectedMarkerType == Arrow) {
        ArrowMarker *mrkL = new ArrowMarker();
        int linesOnPlot = (int)d_lines.size();
        d_lines.resize(++linesOnPlot);
        d_lines[linesOnPlot - 1] = d_plot->insertMarker(mrkL);

        mrkL->setColor(auxMrkColor);
        mrkL->setWidth(auxMrkWidth);
        mrkL->setStyle(auxMrkStyle);
        mrkL->setStartPoint(QPoint(auxMrkStart.x() + 10, auxMrkStart.y() + 10));
        mrkL->setEndPoint(QPoint(auxMrkEnd.x() + 10, auxMrkEnd.y() + 10));
        mrkL->drawStartArrow(startArrowOn);
        mrkL->drawEndArrow(endArrowOn);
        mrkL->setHeadLength(auxArrowHeadLength);
        mrkL->setHeadAngle(auxArrowHeadAngle);
        mrkL->fillArrowHead(auxFilledArrowHead);
    } else if (selectedMarkerType == Image) {
        ImageMarker *mrk = new ImageMarker(auxMrkFileName);
        int imagesOnPlot = d_images.size();
        d_images.resize(++imagesOnPlot);
        d_images[imagesOnPlot - 1] = d_plot->insertMarker(mrk);

        QPoint o = d_plot->canvas()->mapFromGlobal(QCursor::pos());
        if (!d_plot->canvas()->contentsRect().contains(o))
            o = QPoint(auxMrkStart.x() + 20, auxMrkStart.y() + 20);
        mrk->setOrigin(o);
        mrk->setSize(QRect(auxMrkStart, auxMrkEnd).size());
    } else {
        Legend *mrk = new Legend(d_plot);
        int texts = d_texts.size();
        d_texts.resize(++texts);
        d_texts[texts - 1] = d_plot->insertMarker(mrk);

        QPoint o = d_plot->canvas()->mapFromGlobal(QCursor::pos());
        if (!d_plot->canvas()->contentsRect().contains(o))
            o = QPoint(auxMrkStart.x() + 20, auxMrkStart.y() + 20);
        mrk->setOrigin(o);
        mrk->setAngle(auxMrkAngle);
        mrk->setFrameStyle(auxMrkBkg);
        mrk->setFont(auxMrkFont);
        mrk->setText(auxMrkText);
        mrk->setTextColor(auxMrkColor);
        mrk->setBackgroundColor(auxMrkBkgColor);
    }

    d_plot->replot();
    deselectMarker();
}

void Graph::setCopiedMarkerEnds(const QPoint &start, const QPoint &end)
{
    auxMrkStart = start;
    auxMrkEnd = end;
}

void Graph::setCopiedTextOptions(int bkg, const QString &text, const QFont &font,
                                 const QColor &color, const QColor &bkgColor)
{
    auxMrkBkg = bkg;
    auxMrkText = text;
    auxMrkFont = font;
    auxMrkColor = color;
    auxMrkBkgColor = bkgColor;
}

void Graph::setCopiedArrowOptions(int width, Qt::PenStyle style, const QColor &color, bool start,
                                  bool end, int headLength, int headAngle, bool filledHead)
{
    auxMrkWidth = width;
    auxMrkStyle = style;
    auxMrkColor = color;
    startArrowOn = start;
    endArrowOn = end;
    auxArrowHeadLength = headLength;
    auxArrowHeadAngle = headAngle;
    auxFilledArrowHead = filledHead;
}

bool Graph::titleSelected()
{
    return d_plot->titleLabel()->hasFocus();
}

void Graph::selectTitle()
{
    if (!d_plot->hasFocus()) {
        emit selectedGraph(this);
        QwtTextLabel *title = d_plot->titleLabel();
        title->setFocus();
    }

    deselectMarker();
}

void Graph::setTitle(const QString &t)
{
    QwtText cur = d_plot->title();
    QFont f = cur.font();
    if (f.family().trimmed().isEmpty())
        f = QFont("Arial", 16, QFont::Bold, false);
    cur.setFont(f);
    cur.setText(t);
    d_plot->setTitle(cur);
    emit modifiedGraph();
}

void Graph::removeTitle()
{
    if (d_plot->titleLabel()->hasFocus()) {
        setTitle(" ");
    }
}

void Graph::initTitle(bool on, const QFont &fnt)
{
    if (on) {
        QwtText t = d_plot->title();
        t.setFont(fnt);
        t.setText(tr("Title"));
        d_plot->setTitle(t);
    }
}

void Graph::removeLegend()
{
    if (legendMarkerID >= 0) {
        int index = d_texts.indexOf(legendMarkerID);
        int texts = d_texts.size();
        for (int i = index; i < texts - 1; i++)
            d_texts[i] = d_texts[i + 1];
        d_texts.resize(--texts);

        d_plot->removeMarker(legendMarkerID);
        legendMarkerID = -1;
    }
}

void Graph::updateImageMarker(int x, int y, int w, int h)
{
    ImageMarker *mrk = (ImageMarker *)d_plot->marker(selectedMarker);
    mrk->setRect(x, y, w, h);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::updateTextMarker(const QString &text, int angle, int bkg, const QFont &fnt,
                             const QColor &textColor, const QColor &backgroundColor)
{
    Legend *mrkL = (Legend *)d_plot->marker(selectedMarker);
    mrkL->setText(text);
    mrkL->setAngle(angle);
    mrkL->setTextColor(textColor);
    mrkL->setBackgroundColor(backgroundColor);
    mrkL->setFont(fnt);
    mrkL->setFrameStyle(bkg);

    d_plot->replot();
    emit modifiedGraph();
}

Legend *Graph::legend()
{
    if (legendMarkerID >= 0)
        return (Legend *)d_plot->marker(legendMarkerID);
    else
        return 0;
}

QString Graph::legendText()
{
    QString text = "";
    for (int i = 0; i < n_curves; i++) {
        const QwtPlotCurve *c = curve(i);
        if (c && c->rtti() != QwtPlotItem::Rtti_PlotSpectrogram && c_type[i] != ErrorBars) {
            text += "\\c{";
            text += QString::number(i + 1);
            text += "}";
            const FunctionCurve *fc = dynamic_cast<const FunctionCurve *>(c);
            if (fc)
                text += fc->legend();
            else {
                QString label = c->title().text();
                DataCurve *dc = dynamic_cast<DataCurve *>(const_cast<QwtPlotCurve *>(c));
                if (dc && dc->table()) {
                    const int yIdx = dc->table()->colIndex(dc->yColumnName());
                    Column *yc = (yIdx >= 0) ? dc->table()->column(yIdx) : nullptr;
                    if (yc && !yc->longName().trimmed().isEmpty()) {
                        label = yc->longName().trimmed();
                    } else {
                        const QString prefix = QString(dc->table()->name()) + "_";
                        if (label.startsWith(prefix) && label.length() > prefix.length())
                            label = label.mid(prefix.length());
                    }
                    d_last_auto_curve_labels[i] = label;
                }
                text += label;
            }
            text += "\n";
        }
    }
    return text;
}

QString Graph::pieLegendText()
{
    QString text = "";
    QList<int> keys = d_plot->curveKeys();
    const QwtPlotCurve *curve = (QwtPlotCurve *)d_plot->curve(keys[0]);
    if (curve) {
        for (int i = 0; i < int(curve->dataSize()); i++) {
            text += "\\p{";
            text += QString::number(i + 1);
            text += "} ";
            text += QString::number(i + 1);
            text += "\n";
        }
    }
    return text;
}

void Graph::updateCurvesData(Table *w, const QString &colName)
{
    QList<int> keys = d_plot->curveKeys();
    int updated_curves = 0;
    QList<PlotCurve *> to_remove; // curves which changed too much to be kept
    for (int i = 0; i < (int)keys.count(); i++) {
        QwtPlotItem *it = d_plot->plotItem(keys[i]);
        if (!it)
            continue;
        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        if (static_cast<PlotCurve *>(it)->type() == Function)
            continue;
        DataCurve *c = static_cast<DataCurve *>(it);
        if (c->table() == w && (c->xColumnName() == colName || c->yColumnName() == colName)) {
            Column *changedCol = w ? w->column(colName) : nullptr;
            if (!changedCol) {
                if (c->updateData(w, colName))
                    updated_curves++;
                continue;
            }
            auto colType = changedCol->columnMode();
            AxisType atype;
            if (c->xColumnName() == colName) {
                if (c->type() == HorizontalBars)
                    atype = (AxisType)axisType[QwtPlot::yLeft];
                else
                    atype = (AxisType)axisType[QwtPlot::xBottom];
            } else {
                if (c->type() == HorizontalBars)
                    atype = (AxisType)axisType[QwtPlot::xBottom];
                else
                    atype = (AxisType)axisType[QwtPlot::yLeft];
            }
            // compare setLabels*Format() calls in insertCurve()
            if ((colType == SciDAVis::ColumnMode::Text && atype != AxisType::Txt)
                || (colType == SciDAVis::ColumnMode::DateTime
                    && ((atype != AxisType::Time) && (atype != AxisType::Date)
                        && (atype != AxisType::DateTime))))
                to_remove << c;
            else if (c->updateData(w, colName))
                updated_curves++;
        } else if (((DataCurve *)it)->updateData(w, colName))
            updated_curves++;
    }
    foreach (PlotCurve *c, to_remove) {
        removeCurve(curveIndex(c));
        updated_curves++;
    }
    if (updated_curves) {
        updateStatReferenceLines();
        if (isPiePlot())
            updatePlot();
        else {
            if (m_autoscale) {
                for (int i = 0; i < QwtPlot::axisCnt; i++)
                    d_plot->setAxisAutoScale(i);
            }
            if (d_linked_parent)
                syncLinkedAxesFromParent();
            d_plot->replot();
            syncChildLinkedLayers();
        }
    }
}

QString Graph::saveEnabledAxes()
{
    QString list = "EnabledAxes\t";
    for (int i = 0; i < QwtPlot::axisCnt; i++)
        list += QString::number(d_plot->axisEnabled(i)) + "\t";

    list += "\n";
    return list;
}

bool Graph::framed()
{
    return d_plot->lineWidth() > 0;
}

QColor Graph::canvasFrameColor()
{
    QwtPlotCanvas *canvas = (QwtPlotCanvas *)d_plot->canvas();
    QPalette pal = canvas->palette();
    return pal.color(QPalette::Active, QPalette::WindowText);
}

int Graph::canvasFrameWidth()
{
    QwtPlotCanvas *canvas = (QwtPlotCanvas *)d_plot->canvas();
    return canvas->lineWidth();
}

void Graph::drawCanvasFrame(const QStringList &frame)
{
    d_plot->setLineWidth((frame[1]).toInt());
    if ((frame[1]).toInt())
        d_plot->setFrameStyle(QFrame::Box);

    QPalette pal = d_plot->palette();
    pal.setColor(QPalette::WindowText, QColor(COLORVALUE(frame[2])));
    d_plot->setPalette(pal);
}

void Graph::drawCanvasFrame(bool frameOn, int width, const QColor &color)
{
    QPalette pal = d_plot->palette();

    if (frameOn && d_plot->lineWidth() == width
        && pal.color(QPalette::Active, QPalette::WindowText) == color)
        return;

    if (frameOn) {
        d_plot->setLineWidth(width);
        pal.setColor(QPalette::WindowText, color);
        d_plot->setPalette(pal);
    } else {
        d_plot->setLineWidth(0);
        pal.setColor(QPalette::WindowText, QColor(Qt::black));
        d_plot->setPalette(pal);
    }
    emit modifiedGraph();
}

void Graph::drawCanvasFrame(bool frameOn, int width)
{
    if (frameOn) {
        d_plot->setLineWidth(width);
    }
}

void Graph::drawAxesBackbones(bool yes)
{
    drawAxesBackbone = yes;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_axis_backbone[i] = yes;
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(i);
            sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, yes);
            scale->repaint();
        }
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::loadAxesOptions(const QString &s)
{
    if (s == "1")
        return;

    drawAxesBackbone = false;

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_axis_backbone[i] = false;
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(i);
            sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, false);
            scale->repaint();
        }
    }
}

void Graph::setAxesLinewidth(int width)
{
    if (d_plot->axesLinewidth() == width)
        return;

    d_plot->setAxesLinewidth(width);

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            scale->setPenWidth(width);
            scale->repaint();
        }
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::loadAxesLinewidth(int width)
{
    d_plot->setAxesLinewidth(width);
}

QString Graph::saveCanvas()
{
    QString s = "";
    int w = d_plot->canvas()->lineWidth();
    if (w > 0) {
        s += "CanvasFrame\t" + QString::number(w) + "\t";
        s += COLORNAME(canvasFrameColor()) + "\n";
    }
    s += "CanvasBackground\t" + COLORNAME(d_plot->canvasBackground()) + "\t";
    s += QString::number(d_plot->canvasBackground().alpha()) + "\n";
    return s;
}

QString Graph::saveFonts()
{
    int i;
    QString s;
    QStringList list, axesList;
    QFont f;
    list << "TitleFont";
    f = d_plot->title().font();
    list << f.family();
    list << QString::number(f.pointSize());
    list << QString::number(f.weight());
    list << QString::number(f.italic());
    list << QString::number(f.underline());
    list << QString::number(f.strikeOut());
    s = list.join("\t") + "\n";

    for (i = 0; i < d_plot->axisCnt; i++) {
        f = d_plot->axisTitle(i).font();
        list[0] = "ScaleFont" + QString::number(i);
        list[1] = f.family();
        list[2] = QString::number(f.pointSize());
        list[3] = QString::number(f.weight());
        list[4] = QString::number(f.italic());
        list[5] = QString::number(f.underline());
        list[6] = QString::number(f.strikeOut());
        s += list.join("\t") + "\n";
    }

    for (i = 0; i < d_plot->axisCnt; i++) {
        f = d_plot->axisFont(i);
        list[0] = "AxisFont" + QString::number(i);
        list[1] = f.family();
        list[2] = QString::number(f.pointSize());
        list[3] = QString::number(f.weight());
        list[4] = QString::number(f.italic());
        list[5] = QString::number(f.underline());
        list[6] = QString::number(f.strikeOut());
        s += list.join("\t") + "\n";
    }
    return s;
}

QString Graph::saveAxesFormulas()
{
    QString s;
    for (int i = 0; i < 4; i++)
        if (!axesFormulas[i].isEmpty()) {
            s += "<AxisFormula pos=\"" + QString::number(i) + "\">\n";
            s += axesFormulas[i];
            s += "\n</AxisFormula>\n";
        }
    return s;
}

QString Graph::saveScale()
{
    QString s;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        s += "scale\t" + QString::number(i) + "\t";

        const QwtScaleDiv *scDiv = d_plot->axisScaleDiv(i);
        QwtValueList lst = scDiv->ticks(QwtScaleDiv::MajorTick);

#if QWT_VERSION >= 0x050200
        s += QString::number(qMin(scDiv->lowerBound(), scDiv->upperBound()), 'g', 15) + "\t";
        s += QString::number(qMax(scDiv->lowerBound(), scDiv->upperBound()), 'g', 15) + "\t";
#else
        s += QString::number(qMin(scDiv->lBound(), scDiv->hBound()), 'g', 15) + "\t";
        s += QString::number(qMax(scDiv->lBound(), scDiv->hBound()), 'g', 15) + "\t";
#endif
        s += QString::number(d_user_step[i], 'g', 15) + "\t";
        s += QString::number(d_plot->axisMaxMajor(i)) + "\t";
        s += QString::number(d_plot->axisMaxMinor(i)) + "\t";

        const QwtScaleEngine *sc_eng = d_plot->axisScaleEngine(i);
        QwtScaleTransformation *tr = sc_eng->transformation();
        const int savedScaleType =
                (tr && tr->type() == QwtScaleTransformation::Log10) ? 1 : 0;
        s += QString::number(savedScaleType) + "\t";
        s += QString::number(sc_eng->testAttribute(QwtScaleEngine::Inverted)) + "\n";
    }
    return s;
}

void Graph::setXAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::xBottom);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setYAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::yLeft);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setRightAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::yRight);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setTopAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::xTop);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setAxesTitleColor(QStringList l)
{
    for (int i = 0; i < int(l.count() - 1); i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            QwtText title = scale->title();
            title.setColor(QColor(COLORVALUE(l[i + 1])));
            scale->setTitle(title);
        }
    }
}

QString Graph::saveAxesTitleColors()
{
    QString s = "AxesTitleColors\t";
    for (int i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        QColor c;
        if (scale)
            c = scale->title().color();
        else
            c = QColor(Qt::black);

        s += COLORNAME(c) + "\t";
    }
    return s + "\n";
}

QString Graph::saveTitle()
{
    QString s = "PlotTitle\t";
    s += d_plot->title().text().replace("\n", "<br>") + "\t";
    s += COLORNAME(d_plot->title().color()) + "\t";
    s += QString::number(d_plot->title().renderFlags()) + "\n";
    return s;
}

QString Graph::saveScaleTitles()
{
    int a{};
    QString s = "";
    for (int i = 0; i < 4; i++) {
        switch (i) {
        case 0:
            a = 2;
            break;
        case 1:
            a = 0;
            break;
        case 2:
            a = 3;
            break;
        case 3:
            a = 1;
            break;
        }
        QString title = d_plot->axisTitle(a).text();
        if (!title.isEmpty())
            s += title.replace("\n", "<br>") + "\t";
        else
            s += "\t";
    }
    return s + "\n";
}

QString Graph::saveAxesTitleAlignement()
{
    QString s = "AxesTitleAlignment\t";
    QStringList axes;
    int i;
    for (i = 0; i < 4; i++)
        axes << QString::number(Qt::AlignHCenter);

    for (i = 0; i < 4; i++) {

        if (d_plot->axisEnabled(i))
            axes[i] = QString::number(d_plot->axisTitle(i).renderFlags());
    }

    s += axes.join("\t") + "\n";
    return s;
}

QString Graph::savePieCurveLayout()
{
    QString s = "PieCurve\t";

    QwtPieCurve *pieCurve = (QwtPieCurve *)curve(0);
    s += pieCurve->title().text() + "\t";
    QPen pen = pieCurve->pen();

    s += QString::number(pen.width()) + "\t";
    s += COLORNAME(pen.color()) + "\t";
    s += penStyleName(pen.style()) + "\t";

    Qt::BrushStyle pattern = pieCurve->pattern();
    int index;
    if (pattern == Qt::SolidPattern)
        index = 0;
    else if (pattern == Qt::HorPattern)
        index = 1;
    else if (pattern == Qt::VerPattern)
        index = 2;
    else if (pattern == Qt::CrossPattern)
        index = 3;
    else if (pattern == Qt::BDiagPattern)
        index = 4;
    else if (pattern == Qt::FDiagPattern)
        index = 5;
    else if (pattern == Qt::DiagCrossPattern)
        index = 6;
    else if (pattern == Qt::Dense1Pattern)
        index = 7;
    else if (pattern == Qt::Dense2Pattern)
        index = 8;
    else if (pattern == Qt::Dense3Pattern)
        index = 9;
    else if (pattern == Qt::Dense4Pattern)
        index = 10;
    else if (pattern == Qt::Dense5Pattern)
        index = 11;
    else if (pattern == Qt::Dense6Pattern)
        index = 12;
    else if (pattern == Qt::Dense7Pattern)
        index = 13;
    else
        throw std::runtime_error("pattern out of range");

    s += QString::number(index) + "\t";
    s += QString::number(pieCurve->ray()) + "\t";
    s += QString::number(pieCurve->firstColor()) + "\t";
    s += QString::number(pieCurve->startRow()) + "\t" + QString::number(pieCurve->endRow()) + "\t";
    s += QString::number(pieCurve->isVisible()) + "\n";
    return s;
}

QString Graph::saveCurveLayout(int index)
{
    QString s;
    int style = c_type[index];
    QwtPlotCurve *c = (QwtPlotCurve *)curve(index);
    if (c) {
        s += QString::number(style) + "\t";
        if (style == Spline)
            s += "5\t";
        else if (style == VerticalSteps)
            s += "6\t";
        else
            s += QString::number(c->style()) + "\t";
        s += COLORNAME(c->pen().color()) + "\t";
        s += QString::number(c->pen().style() - 1) + "\t";
        s += QString::number(c->pen().width()) + "\t";

        const QwtSymbol symbol = c->symbol();
        s += QString::number(symbol.size().width()) + "\t";
        s += QString::number(SymbolBox::symbolIndex(symbol.style())) + "\t";
        s += COLORNAME(symbol.pen().color()) + "\t";
        if (symbol.brush().style() != Qt::NoBrush)
            s += COLORNAME(symbol.brush().color()) + "\t";
        else
            s += QString::number(-1) + "\t";

        bool filled = c->brush().style() == Qt::NoBrush ? false : true;
        s += QString::number(filled) + "\t";

        s += COLORNAME(c->brush().color()) + "\t";
        s += QString::number(PatternBox::patternIndex(c->brush().style())) + "\t";
        if (style <= LineSymbols || style == Box)
            s += QString::number(symbol.pen().width()) + "\t";
        // extension for custom dash pattern
        s += QString::number(static_cast<int>(c->pen().capStyle())) + "\t";
        s += QString::number(static_cast<int>(c->pen().joinStyle())) + "\t";
        QStringList customDash;
        for (auto v : c->pen().dashPattern())
            customDash << QString::number(v);
        s += customDash.join(" ") + "\t";
    }

    if (style == VerticalBars || style == HorizontalBars || style == Histogram) {
        QwtBarCurve *b = (QwtBarCurve *)c;
        s += QString::number(b->gap()) + "\t";
        s += QString::number(b->offset()) + "\t";
    }

    if (style == Histogram) {
        QwtHistogram *h = (QwtHistogram *)c;
        s += QString::number(h->autoBinning()) + "\t";
        s += QString::number(h->binSize()) + "\t";
        s += QString::number(h->begin()) + "\t";
        s += QString::number(h->end()) + "\t";
    } else if (style == VectXYXY || style == VectXYAM) {
        VectorCurve *v = (VectorCurve *)c;
        s += COLORNAME(v->color()) + "\t";
        s += QString::number(v->width()) + "\t";
        s += QString::number(v->headLength()) + "\t";
        s += QString::number(v->headAngle()) + "\t";
        s += QString::number(v->filledArrowHead()) + "\t";

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList colsList = v->plotAssociation().split(",", Qt::SkipEmptyParts);
#else
        QStringList colsList = v->plotAssociation().split(",", QString::SkipEmptyParts);
#endif
        s += colsList[2].remove("(X)").remove("(A)") + "\t";
        s += colsList[3].remove("(Y)").remove("(M)");
        if (style == VectXYAM)
            s += "\t" + QString::number(v->position());
        s += "\t";
    } else if (style == Box) {
        BoxCurve *b = (BoxCurve *)c;
        s += QString::number(SymbolBox::symbolIndex(b->maxStyle())) + "\t";
        s += QString::number(SymbolBox::symbolIndex(b->p99Style())) + "\t";
        s += QString::number(SymbolBox::symbolIndex(b->meanStyle())) + "\t";
        s += QString::number(SymbolBox::symbolIndex(b->p1Style())) + "\t";
        s += QString::number(SymbolBox::symbolIndex(b->minStyle())) + "\t";
        s += QString::number(b->boxStyle()) + "\t";
        s += QString::number(b->boxWidth()) + "\t";
        s += QString::number(b->boxRangeType()) + "\t";
        s += QString::number(b->boxRange()) + "\t";
        s += QString::number(b->whiskersRangeType()) + "\t";
        s += QString::number(b->whiskersRange()) + "\t";
        s += QString::number(b->violinMode() ? 1 : 0) + "\t";
        s += QString::number(b->showDataPoints() ? 1 : 0) + "\t";
    }

    return s;
}

QString Graph::saveCurves()
{
    QString s;
    if (isPiePlot())
        s += savePieCurveLayout();
    else {
        for (int i = 0; i < n_curves; i++) {
            QwtPlotItem *it = plotItem(i);
            if (!it)
                continue;

            if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                s += ((Spectrogram *)it)->saveToString();
                continue;
            }

            PlotCurve *c = static_cast<PlotCurve *>(it);
            if (c->type() != ErrorBars) {
                if (c->type() == Function) {
                    s += "FunctionCurve\t";
                    s += QString::number(static_cast<FunctionCurve *>(c)->functionType()) + ",";
                    s += c->title().text() + ",";
                    s += static_cast<FunctionCurve *>(c)->variable() + ",";
                    s += QString::number(static_cast<FunctionCurve *>(c)->startRange(), 'g', 15)
                            + ",";
                    s += QString::number(static_cast<FunctionCurve *>(c)->endRange(), 'g', 15)
                            + "\t";
                    s += QString::number(static_cast<FunctionCurve *>(c)->dataSize()) + "\t\t\t";
                    // the 2 last tabs are legacy code, kept for compatibility with old project
                    // files
                } else if (c->type() == Box)
                    s += "curve\t" + QString::number(c->x(0)) + "\t" + c->title().text() + "\t";
                else
                    s += "curve\t" + static_cast<DataCurve *>(c)->xColumnName() + "\t"
                            + c->title().text() + "\t";

                s += saveCurveLayout(i);
                s += QString::number(c->xAxis()) + "\t" + QString::number(c->yAxis()) + "\t";
                s += QString::number(static_cast<DataCurve *>(c)->startRow()) + "\t"
                        + QString::number(static_cast<DataCurve *>(c)->endRow()) + "\t";
                s += QString::number(c->isVisible()) + "\n";
                if (c->type() == Function) {
                    FunctionCurve *fc = static_cast<FunctionCurve *>(c);
                    s += "<formula>\n"
                            + fc->formulas().join(
                                      "\n</formula>\n<formula>\n")
                            + "\n</formula>\n";
                    if (fc->dataSize() > 0 && fc->dataSize() <= 20000) {
                        s += "<sampled_xy>\n";
                        for (int k = 0; k < fc->dataSize(); ++k) {
                            s += QString::number(fc->x(k), 'g', 15) + "\t"
                                    + QString::number(fc->y(k), 'g', 15) + "\n";
                        }
                        s += "</sampled_xy>\n";
                    }
                }
            } else if (c->type() == ErrorBars) {
                QwtErrorPlotCurve *er = (QwtErrorPlotCurve *)it;
                s += "ErrorBars\t";
                s += QString::number(er->direction()) + "\t";
                s += er->masterCurve()->xColumnName() + "\t";
                s += er->masterCurve()->title().text() + "\t";
                s += er->title().text() + "\t";
                s += QString::number(er->width()) + "\t";
                s += QString::number(er->capLength()) + "\t";
                s += COLORNAME(er->color()) + "\t";
                s += QString::number(er->throughSymbol()) + "\t";
                s += QString::number(er->plusSide()) + "\t";
                s += QString::number(er->minusSide()) + "\n";
            }
        }
    }
    return s;
}

Legend *Graph::newLegend()
{
    Legend *mrk = new Legend(d_plot);
    mrk->setOrigin(QPoint(16, 16));

    if (isPiePlot())
        mrk->setText(pieLegendText());
    else
        mrk->setText(legendText());

    mrk->setFrameStyle(defaultMarkerFrame);
    mrk->setFont(defaultMarkerFont);
    mrk->setTextColor(defaultTextMarkerColor);
    mrk->setBackgroundColor(defaultTextMarkerBackground);

    legendMarkerID = d_plot->insertMarker(mrk);
    int texts = d_texts.size();
    d_texts.resize(++texts);
    d_texts[texts - 1] = legendMarkerID;

    emit modifiedGraph();
    d_plot->replot();
    return mrk;
}

void Graph::addTimeStamp()
{
    Legend *mrk = newLegend(QLocale().toString(QDateTime::currentDateTime()));
    mrk->setOrigin(QPoint(d_plot->canvas()->width() / 2, 10));
    emit modifiedGraph();
    d_plot->replot();
}

Legend *Graph::newLegend(const QString &text)
{
    Legend *mrk = new Legend(d_plot);
    selectedMarker = d_plot->insertMarker(mrk);
    if (d_markers_selector)
        delete d_markers_selector;

    int texts = d_texts.size();
    d_texts.resize(++texts);
    d_texts[texts - 1] = selectedMarker;

    mrk->setOrigin(QPoint(5, 5));
    mrk->setText(text);
    mrk->setFrameStyle(defaultMarkerFrame);
    mrk->setFont(defaultMarkerFont);
    mrk->setTextColor(defaultTextMarkerColor);
    mrk->setBackgroundColor(defaultTextMarkerBackground);
    return mrk;
}

void Graph::insertLegend(const QStringList &lst, int fileVersion)
{
    legendMarkerID = insertTextMarker(lst, fileVersion);
}

long Graph::insertTextMarker(const QStringList &list, int fileVersion)
{
    QStringList fList = list;
    Legend *mrk = new Legend(d_plot);
    long key = d_plot->insertMarker(mrk);

    int texts = d_texts.size();
    d_texts.resize(++texts);
    d_texts[texts - 1] = key;

    if (fileVersion < 86)
        mrk->setOrigin(QPoint(fList[1].toInt(), fList[2].toInt()));
    else
        mrk->setOriginCoord(fList[1].toDouble(), fList[2].toDouble());

    QFont fnt = QFont(fList[3], fList[4].toInt(), fList[5].toInt(), fList[6].toInt());
    fnt.setUnderline(fList[7].toInt());
    fnt.setStrikeOut(fList[8].toInt());
    mrk->setFont(fnt);

    mrk->setAngle(fList[11].toInt());

    QString text = QString();
    if (fileVersion < 71) {
        int bkg = fList[10].toInt();
        if (bkg <= 2)
            mrk->setFrameStyle(bkg);
        else if (bkg == 3) {
            mrk->setFrameStyle(0);
            mrk->setBackgroundColor(QColor(255, 255, 255));
        } else if (bkg == 4) {
            mrk->setFrameStyle(0);
            mrk->setBackgroundColor(QColor(Qt::black));
        }

        int n = (int)fList.count();
        for (int i = 0; i < n - 12; i++)
            text += fList[12 + i] + "\n";
    } else if (fileVersion < 90) {
        mrk->setTextColor(QColor(COLORVALUE(fList[9])));
        mrk->setFrameStyle(fList[10].toInt());
        mrk->setBackgroundColor(QColor(COLORVALUE(fList[12])));

        int n = (int)fList.count();
        for (int i = 0; i < n - 13; i++)
            text += fList[13 + i] + "\n";
    } else {
        mrk->setTextColor(QColor(COLORVALUE(fList[9])));
        mrk->setFrameStyle(fList[10].toInt());
        QColor c = QColor(COLORVALUE(fList[12]));
        c.setAlpha(fList[13].toInt());
        mrk->setBackgroundColor(c);

        int n = (int)fList.count();
        for (int i = 0; i < n - 14; i++)
            text += fList[14 + i] + "\n";
    }
    mrk->setText(text.trimmed());
    return key;
}

void Graph::addArrow(QStringList list, int fileVersion)
{
    ArrowMarker *mrk = new ArrowMarker();
    long mrkID = d_plot->insertMarker(mrk);
    int linesOnPlot = (int)d_lines.size();
    d_lines.resize(++linesOnPlot);
    d_lines[linesOnPlot - 1] = mrkID;

    if (fileVersion < 86) {
        mrk->setStartPoint(QPoint(list[1].toInt(), list[2].toInt()));
        mrk->setEndPoint(QPoint(list[3].toInt(), list[4].toInt()));
    } else
        mrk->setBoundingRect(list[1].toDouble(), list[2].toDouble(), list[3].toDouble(),
                             list[4].toDouble());

    mrk->setWidth(list[5].toInt());
    mrk->setColor(QColor(COLORVALUE(list[6])));
    mrk->setStyle(getPenStyle(list[7]));
    mrk->drawEndArrow(list[8] == "1");
    mrk->drawStartArrow(list[9] == "1");
    if (list.count() > 10) {
        mrk->setHeadLength(list[10].toInt());
        mrk->setHeadAngle(list[11].toInt());
        mrk->fillArrowHead(list[12] == "1");
    }
    if (list.count() > 13) {
        mrk->setCapStyle(static_cast<Qt::PenCapStyle>(list[13].toInt()));
        mrk->setJoinStyle(static_cast<Qt::PenJoinStyle>(list[14].toInt()));
        QVector<qreal> customDash;
        for (auto v : list[15].split(" "))
            customDash << v.toDouble();
        QPen pen = mrk->linePen();
        if (mrk->style() == Qt::CustomDashLine)
            pen.setDashPattern(customDash);
        mrk->setLinePen(pen);
    }
}

void Graph::addArrow(ArrowMarker *mrk)
{
    ArrowMarker *aux = new ArrowMarker();
    int linesOnPlot = (int)d_lines.size();
    d_lines.resize(++linesOnPlot);
    d_lines[linesOnPlot - 1] = d_plot->insertMarker(aux);

    aux->setBoundingRect(mrk->startPointCoord().x(), mrk->startPointCoord().y(),
                         mrk->endPointCoord().x(), mrk->endPointCoord().y());
    aux->setWidth(mrk->width());
    aux->setColor(mrk->color());
    aux->setStyle(mrk->style());
    aux->drawEndArrow(mrk->hasEndArrow());
    aux->drawStartArrow(mrk->hasStartArrow());
    aux->setHeadLength(mrk->headLength());
    aux->setHeadAngle(mrk->headAngle());
    aux->fillArrowHead(mrk->filledArrowHead());
}

ArrowMarker *Graph::arrow(long id)
{
    return (ArrowMarker *)d_plot->marker(id);
}

ImageMarker *Graph::imageMarker(long id)
{
    return (ImageMarker *)d_plot->marker(id);
}

Legend *Graph::textMarker(long id)
{
    return (Legend *)d_plot->marker(id);
}

long Graph::insertTextMarker(Legend *mrk)
{
    Legend *aux = new Legend(d_plot);
    selectedMarker = d_plot->insertMarker(aux);
    if (d_markers_selector)
        delete d_markers_selector;

    int texts = d_texts.size();
    d_texts.resize(++texts);
    d_texts[texts - 1] = selectedMarker;

    aux->setTextColor(mrk->textColor());
    aux->setBackgroundColor(mrk->backgroundColor());
    aux->setOriginCoord(mrk->xValue(), mrk->yValue());
    aux->setFont(mrk->font());
    aux->setFrameStyle(mrk->frameStyle());
    aux->setAngle(mrk->angle());
    aux->setNumColumns(mrk->numColumns());
    aux->setText(mrk->text());
    return selectedMarker;
}

QString Graph::saveMarkers()
{
    QString s;
    int t = d_texts.size(), l = d_lines.size(), im = d_images.size();
    for (int i = 0; i < im; i++) {
        ImageMarker *mrkI = (ImageMarker *)d_plot->marker(d_images[i]);
        s += "<image>\t";
        s += mrkI->fileName() + "\t";
        s += QString::number(mrkI->xValue(), 'g', 15) + "\t";
        s += QString::number(mrkI->yValue(), 'g', 15) + "\t";
        s += QString::number(mrkI->right(), 'g', 15) + "\t";
        s += QString::number(mrkI->bottom(), 'g', 15) + "</image>\n";
    }

    for (int i = 0; i < l; i++) {
        ArrowMarker *mrkL = (ArrowMarker *)d_plot->marker(d_lines[i]);
        s += "<line>\t";

        QwtDoublePoint sp = mrkL->startPointCoord();
        s += (QString::number(sp.x(), 'g', 15)) + "\t";
        s += (QString::number(sp.y(), 'g', 15)) + "\t";

        QwtDoublePoint ep = mrkL->endPointCoord();
        s += (QString::number(ep.x(), 'g', 15)) + "\t";
        s += (QString::number(ep.y(), 'g', 15)) + "\t";

        s += QString::number(mrkL->width()) + "\t";
        s += COLORNAME(mrkL->color()) + "\t";
        s += penStyleName(mrkL->style()) + "\t";
        s += QString::number(mrkL->hasEndArrow()) + "\t";
        s += QString::number(mrkL->hasStartArrow()) + "\t";
        s += QString::number(mrkL->headLength()) + "\t";
        s += QString::number(mrkL->headAngle()) + "\t";
        s += QString::number(mrkL->filledArrowHead()) + "\t";
        s += QString::number(static_cast<int>(mrkL->capStyle())) + "\t";
        s += QString::number(static_cast<int>(mrkL->joinStyle())) + "\t";
        QStringList customDash;
        for (auto v : mrkL->linePen().dashPattern())
            customDash << QString::number(v);
        s += customDash.join(" ");
        s += "</line>\n";
    }

    for (int i = 0; i < t; i++) {
        Legend *mrk = (Legend *)d_plot->marker(d_texts[i]);
        if (d_texts[i] != legendMarkerID)
            s += "<text>\t";
        else
            s += "<legend>\t";

        s += QString::number(mrk->xValue(), 'g', 15) + "\t";
        s += QString::number(mrk->yValue(), 'g', 15) + "\t";

        QFont f = mrk->font();
        s += f.family() + "\t";
        s += QString::number(f.pointSize()) + "\t";
        s += QString::number(f.weight()) + "\t";
        s += QString::number(f.italic()) + "\t";
        s += QString::number(f.underline()) + "\t";
        s += QString::number(f.strikeOut()) + "\t";
        s += COLORNAME(mrk->textColor()) + "\t";
        s += QString::number(mrk->frameStyle()) + "\t";
        s += QString::number(mrk->angle()) + "\t";
        s += COLORNAME(mrk->backgroundColor()) + "\t";
        s += QString::number(mrk->backgroundColor().alpha()) + "\t";
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList textList = mrk->text().split("\n", Qt::KeepEmptyParts);
#else
        QStringList textList = mrk->text().split("\n", QString::KeepEmptyParts);
#endif
        s += textList.join("\t");
        if (d_texts[i] != legendMarkerID)
            s += "</text>\n";
        else
            s += "</legend>\n";
    }
    return s;
}

double Graph::selectedXStartValue()
{
    if (d_range_selector)
        return d_range_selector->minXValue();
    else
        return 0;
}

double Graph::selectedXEndValue()
{
    if (d_range_selector)
        return d_range_selector->maxXValue();
    else
        return 0;
}

QwtPlotItem *Graph::plotItem(int index)
{
    if (!n_curves || index >= n_curves || index < 0)
        return 0;

    return d_plot->plotItem(c_keys[index]);
}

int Graph::plotItemIndex(QwtPlotItem *it) const
{
    for (int i = 0; i < n_curves; i++) {
        if (d_plot->plotItem(c_keys[i]) == it)
            return i;
    }
    return -1;
}

QwtPlotCurve *Graph::curve(int index) const
{
    if (!n_curves || index >= n_curves || index < 0)
        return 0;

    return d_plot->curve(c_keys[index]);
}

int Graph::curveIndex(QwtPlotCurve *c) const
{
    return plotItemIndex(c);
}

int Graph::range(const int index, double& start, double& end) const
{
    if (d_range_selector && d_range_selector->selectedCurve() == curve(index)) {
        start = d_range_selector->minXValue();
        end = d_range_selector->maxXValue();
        return d_range_selector->dataSize();
    } else {
    QwtPlotCurve *c = curve(index);
        if (!c) return 0;
        start = c->minXValue();
        end = c->maxXValue();
        return c->dataSize();
    }
}

CurveLayout Graph::initCurveLayout()
{
    CurveLayout cl;
    cl.connectType = 1;
    cl.lStyle = 0;
    cl.lWidth = 2;
    cl.sSize = 9;
    cl.sType = 1;
    cl.filledArea = 0;
    cl.aCol = 0;
    cl.aStyle = 0;
    cl.lCol = QColor("#1f77b4").rgba();
    cl.penWidth = 2;
    cl.symCol = QColor("#1f77b4").rgba();
    cl.fillCol = QColor("#1f77b4").rgba();
    cl.symbolFill = true;
    return cl;
}

CurveLayout Graph::initCurveLayout(int style, int curves)
{
    int i = n_curves - 1;

    CurveLayout cl = initCurveLayout();
    int color;
    guessUniqueCurveLayout(color, cl.sType);

    cl.lCol = color;
    cl.symCol = color;
    cl.fillCol = color;

    if (style == Graph::Line)
        cl.sType = 0;
    else if (style == Graph::VerticalDropLines)
        cl.connectType = 2;
    else if (style == Graph::HorizontalSteps || style == Graph::VerticalSteps) {
        cl.connectType = 3;
        cl.sType = 0;
    } else if (style == Graph::Spline)
        cl.connectType = 5;
    else if (curves && (style == Graph::VerticalBars || style == Graph::HorizontalBars)) {
        cl.filledArea = 1;
        cl.lCol = 0; // black color pen
        cl.aCol = i + 1;
        cl.sType = 0;
        if (style == Graph::VerticalBars || style == Graph::HorizontalBars) {
            QwtBarCurve *b = (QwtBarCurve *)curve(i);
            if (b) {
                b->setGap(qRound(100 * (1 - 1.0 / (double)curves)));
                b->setOffset(-50 * (curves - 1) + i * 100);
            }
        }
    } else if (style == Graph::Histogram) {
        cl.filledArea = 1;
        cl.lCol = i + 1; // start with red color pen
        cl.aCol = i + 1; // start with red fill color
        cl.aStyle = 4;
        cl.sType = 0;
    } else if (style == Graph::Area) {
        cl.filledArea = 1;
        cl.aCol = color;
        cl.sType = 0;
    }
    return cl;
}

bool Graph::canConvertTo(QwtPlotCurve *c, CurveType type)
{
    if (!c)
        return false;
    // conversion between VectXYXY and VectXYAM is possible, but not implemented
    if (dynamic_cast<VectorCurve *>(c))
        return false;
    // conversion between Pie, Histogram and Box should be possible (all of them take one input
    // column), but lots of special-casing in ApplicationWindow and Graph makes this very difficult
    if (dynamic_cast<QwtPieCurve *>(c) || dynamic_cast<QwtHistogram *>(c)
        || dynamic_cast<BoxCurve *>(c))
        return false;
    // converting error bars doesn't make sense
    if (dynamic_cast<QwtErrorPlotCurve *>(c))
        return false;
    // line/symbol, area and bar curves can be converted to each other
    if (dynamic_cast<DataCurve *>(c))
        return type == Line || type == Scatter || type == LineSymbols || type == VerticalBars
                || type == HorizontalBars || type == HorizontalSteps || type == VerticalSteps
                || type == Area || type == VerticalDropLines || type == Spline;
    return false;
}

void Graph::setCurveType(int curve_index, CurveType type, bool update)
{
    if (curve_index >= c_type.size())
        return;
    CurveType old_type = static_cast<CurveType>(c_type[curve_index]);
    // nothing changed
    if (type == old_type)
        return;
    // not all types can be modified cleanly
    if (type != Line && type != Scatter && type != LineSymbols && type != VerticalBars
        && type != Area && type != VerticalDropLines && type != Spline && type != HorizontalSteps
        && type != HorizontalBars && type != VerticalSteps)
        return;
    if (old_type != Line && old_type != Scatter && old_type != LineSymbols
        && old_type != VerticalBars && old_type != Area && old_type != VerticalDropLines
        && old_type != Spline && old_type != HorizontalSteps && old_type != HorizontalBars
        && old_type != VerticalSteps)
        return;

    if ((type == VerticalBars || type == HorizontalBars)
        != (old_type == VerticalBars || old_type == HorizontalBars)) {
        // bar curves are represented by a different class than the other curve types, which makes
        // changing from/to horizontal or vertical bars more difficult
        DataCurve *old_curve = static_cast<DataCurve *>(curve(curve_index));
        DataCurve *new_curve = 0;
        switch (type) {
        case Histogram:
        case Box:
        case Pie:
            return;
        case VerticalBars:
            new_curve = new QwtBarCurve(QwtBarCurve::Vertical, old_curve->table(),
                                        old_curve->xColumnName(), old_curve->yColumnName(),
                                        old_curve->startRow(), old_curve->endRow());
            new_curve->setStyle(QwtPlotCurve::UserCurve);
            break;
        case HorizontalBars:
            new_curve = new QwtBarCurve(QwtBarCurve::Horizontal, old_curve->table(),
                                        old_curve->xColumnName(), old_curve->yColumnName(),
                                        old_curve->startRow(), old_curve->endRow());
            new_curve->setStyle(QwtPlotCurve::UserCurve);
            break;
        default:
            new_curve = new DataCurve(old_curve->table(), old_curve->xColumnName(),
                                      old_curve->yColumnName(), old_curve->startRow(),
                                      old_curve->endRow());
            break;
        }

        old_curve->clearErrorBars();
        if (int i = d_fit_curves.indexOf(old_curve) >= 0)
            d_fit_curves.replace(i, new_curve);

        d_plot->removeCurve(c_keys[curve_index]);
        c_keys[curve_index] = d_plot->insertCurve(new_curve);
        new_curve->loadData();

        if (d_range_selector && old_curve == d_range_selector->selectedCurve())
            d_range_selector->setSelectedCurve(new_curve);
    }

    c_type[curve_index] = type;
    if (!update)
        return;

    CurveLayout cl = initCurveLayout(type, 1);
    updateCurveLayout(curve_index, &cl);
    updatePlot();
}

void Graph::updateCurveLayout(int index, const CurveLayout *cL)
{
    DataCurve *c = (DataCurve *)this->curve(index);
    if (!c)
        return;
    if (c_type.isEmpty() || (int)c_type.size() < index)
        return;

    QPen pen = QPen(ColorButton::color(cL->symCol), cL->penWidth, Qt::SolidLine);
    if (cL->symbolFill)
        c->setSymbol(QwtSymbol(SymbolBox::style(cL->sType), QBrush(ColorButton::color(cL->fillCol)),
                               pen, QSize(cL->sSize, cL->sSize)));
    else
        c->setSymbol(
                QwtSymbol(SymbolBox::style(cL->sType), QBrush(), pen, QSize(cL->sSize, cL->sSize)));

    // custom dash pattern
    pen = QPen(ColorButton::color(cL->lCol), cL->lWidth, getPenStyle(cL->lStyle));
    pen.setCapStyle(static_cast<Qt::PenCapStyle>(cL->lCapStyle));
    pen.setJoinStyle(static_cast<Qt::PenJoinStyle>(cL->lJoinStyle));
    QVector<qreal> customDash;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    for (auto v : (cL->lCustomDash).split(" ", Qt::SkipEmptyParts))
#else
    for (auto v : (cL->lCustomDash).split(" ", QString::SkipEmptyParts))
#endif
        customDash << v.toDouble();
    if (pen.style() == Qt::CustomDashLine)
        pen.setDashPattern(customDash);
    c->setPen(pen);

    int style = c_type[index];
    if (style == Scatter)
        c->setStyle(QwtPlotCurve::NoCurve);
    else if (style == Spline) {
        c->setStyle(QwtPlotCurve::Lines);
        c->setCurveAttribute(QwtPlotCurve::Fitted, true);
    } else if (style == VerticalSteps) {
        c->setStyle(QwtPlotCurve::Steps);
        c->setCurveAttribute(QwtPlotCurve::Inverted, true);
    } else
        c->setStyle((QwtPlotCurve::CurveStyle)cL->connectType);

    QBrush brush = QBrush(ColorButton::color(cL->aCol));
    if (cL->filledArea)
        brush.setStyle(getBrushStyle(cL->aStyle));
    else
        brush.setStyle(Qt::NoBrush);
    c->setBrush(brush);
}

void Graph::updateErrorBars(QwtErrorPlotCurve *er, bool xErr, int width, int cap, const QColor &c,
                            bool plus, bool minus, bool through)
{
    if (!er)
        return;

    if (er->width() == width && er->capLength() == cap && er->color() == c && er->plusSide() == plus
        && er->minusSide() == minus && er->throughSymbol() == through && er->xErrors() == xErr)
        return;

    er->setWidth(width);
    er->setCapLength(cap);
    er->setColor(c);
    er->setXErrors(xErr);
    er->drawThroughSymbol(through);
    er->drawPlusSide(plus);
    er->drawMinusSide(minus);

    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::addErrorBars(const QString &yColName, Table *errTable, const QString &errColName,
                         int type, int width, int cap, const QColor &color, bool through,
                         bool minus, bool plus)
{
    QList<int> keys = d_plot->curveKeys();
    for (int i = 0; i < n_curves; i++) {
        DataCurve *c = (DataCurve *)d_plot->curve(keys[i]);
        if (c && c->title().text() == yColName && c_type[i] != ErrorBars) {
            return addErrorBars(c->xColumnName(), yColName, errTable, errColName, type, width, cap,
                                color, through, minus, plus);
        }
    }
    return false;
}

bool Graph::addErrorBars(const QString &xColName, const QString &yColName, Table *errTable,
                         const QString &errColName, int type, int width, int cap,
                         const QColor &color, bool through, bool minus, bool plus)
{
    DataCurve *master_curve = masterCurve(xColName, yColName);
    if (!master_curve)
        return false;

    QwtErrorPlotCurve *er = new QwtErrorPlotCurve(type, errTable, errColName);
    er->setMasterCurve(master_curve);
    er->setCapLength(cap);
    er->setColor(color);
    er->setWidth(width);
    er->drawPlusSide(plus);
    er->drawMinusSide(minus);
    er->drawThroughSymbol(through);

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = ErrorBars;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(er);

    updatePlot();
    return true;
}

void Graph::plotPie(Table *w, const QString &name, const QPen &pen, int brush, int size,
                    int firstColor, int startRow, int endRow, bool visible)
{
    if (endRow < 0)
        endRow = w->numRows() - 1;

    QwtPieCurve *pieCurve = new QwtPieCurve(w, name, startRow, endRow);
    pieCurve->loadData();
    pieCurve->setPen(pen);
    pieCurve->setRay(size);
    pieCurve->setFirstColor(firstColor);
    pieCurve->setBrushStyle(getBrushStyle(brush));
    pieCurve->setVisible(visible);

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(pieCurve);

    c_type.resize(n_curves);
    c_type[n_curves - 1] = Pie;
}

void Graph::plotPie(Table *w, const QString &name, int startRow, int endRow)
{
    int ycol = w->colIndex(name);
    int size = 0;
    double sum = 0.0;

    Column *y_col_ptr = w->column(ycol);
    auto yColType = w->columnType(ycol);

    if (endRow < 0)
        endRow = w->numRows() - 1;

    for (int i = 0; i < QwtPlot::axisCnt; i++)
        d_plot->enableAxis(i, false);
    scalePicker->refresh();

    d_plot->setTitle(QString());

    static_cast<QwtPlotCanvas *>(d_plot->canvas())->setLineWidth(1);

    QVarLengthArray<double> Y(abs(endRow - startRow) + 1);
    for (int row = startRow; row <= endRow && row < y_col_ptr->rowCount(); row++) {
        if (!y_col_ptr->isInvalid(row)) {
            if (yColType == SciDAVis::ColumnMode::Text) {
                QString yval = y_col_ptr->textAt(row);
                bool valid_data = true;
                Y[size] = QLocale().toDouble(yval, &valid_data);
                if (!valid_data)
                    continue;
            } else
                Y[size] = y_col_ptr->valueAt(row);

            sum += Y[size];
            size++;
        }
    }
    if (!size)
        return;
    Y.resize(size);

    QwtPieCurve *pieCurve = new QwtPieCurve(w, name, startRow, endRow);
    pieCurve->setData(Y.data(), Y.data(), size);

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(pieCurve);

    c_type.resize(n_curves);
    c_type[n_curves - 1] = Pie;

    // This has to be synced with QwtPieCurve::drawPie() for now... until we have a clean solution.
    QRect canvas_rect = d_plot->plotLayout()->canvasRect();
    float radius = 0.45 * qMin(canvas_rect.width(), canvas_rect.height());

    double PI = 4 * atan(1.0);
    double angle = 90;

    for (int i = 0; i < size; i++) {
        const double value = Y[i] / sum * 360;
        double alabel = (angle - value * 0.5) * PI / 180.0;

        Legend *aux = new Legend(d_plot);
        aux->setFrameStyle(0);
        aux->setText(QString::number(Y[i] / sum * 100, 'g', 2) + "%");

        int texts = d_texts.size();
        d_texts.resize(++texts);
        d_texts[texts - 1] = d_plot->insertMarker(aux);

        aux->setOrigin(canvas_rect.center()
                       + QPoint(int(radius * cos(alabel) - 0.5 * aux->rect().width()),
                                int(-radius * sin(alabel) - 0.5 * aux->rect().height())));

        angle -= value;
    }

    if (legendMarkerID >= 0) {
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk) {
            QString text = "";
            for (int i = 0; i < size; i++) {
                text += "\\p{";
                text += QString::number(i + 1);
                text += "} ";
                text += QString::number(i + 1);
                text += "\n";
            }
            mrk->setText(text);
        }
    }

    d_plot->replot();
    updateScale();
}

bool Graph::plotHistogram(Table *w, QStringList names, int startRow, int endRow)
{
    if (!w)
        return false;
    if (endRow < 0 || endRow >= w->numRows())
        endRow = w->numRows() - 1;

    bool success = false;
    foreach (QString col, names) {
        Column *col_ptr = w->column(col);
        if (!col_ptr || col_ptr->columnMode() != SciDAVis::ColumnMode::Numeric)
            continue;

        QwtHistogram *c = new QwtHistogram(w, col, startRow, endRow);
        c->loadData();
        c->setStyle(QwtPlotCurve::UserCurve);

        c_type.resize(++n_curves);
        c_type[n_curves - 1] = Histogram;
        c_keys.resize(n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(c);

        CurveLayout cl = initCurveLayout(Histogram, names.size());
        updateCurveLayout(n_curves - 1, &cl);

        addLegendItem(col);

        success = true;
    }

    return success;
}

void Graph::insertPlotItem(QwtPlotItem *i, int type)
{
    c_type.resize(++n_curves);
    c_type[n_curves - 1] = type;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(i);

    if (i->rtti() != QwtPlotItem::Rtti_PlotSpectrogram)
        addLegendItem(i->title().text());
}

bool Graph::insertCurvesList(Table *w, const QStringList &names, int style, int lWidth, int sSize,
                             int startRow, int endRow)
{
    if (style == Graph::Pie)
        plotPie(w, names[0], startRow, endRow);
    else if (style == Box)
        plotBoxDiagram(w, names, startRow, endRow);
    else if (style == Histogram)
        plotHistogram(w, names, startRow, endRow);
    else if (style == Graph::VectXYXY || style == Graph::VectXYAM) {
        const int prevCurves = n_curves;
        plotVectorCurve(w, names, style, startRow, endRow);
        if (n_curves <= prevCurves)
            return false;
    } else {
        QStringList errorCurves, otherCurves;
        foreach (QString col, names) {
            int colIndex = w->colIndex(col);
            if (colIndex < 0)
                continue;
            switch (w->colPlotDesignation(colIndex)) {
            case SciDAVis::xErr:
            case SciDAVis::yErr:
                errorCurves << col;
                break;
            default:
                otherCurves << col;
                break;
            }
        }
        QStringList lst = otherCurves + errorCurves;

        for (int i = 0; i < lst.size(); i++) {
            CurveType type_of_i;
            int j = w->colIndex(lst[i]);
            if (j < 0)
                continue;
            bool ok = false;
            if (i >= otherCurves.size()) {
                type_of_i = ErrorBars;
                int ycol = -1;
                for (int k = j - 1; k >= 0; k--) {
                    if (w->colPlotDesignation(k) == SciDAVis::Y) {
                        ycol = k;
                        break;
                    }
                }
                if (ycol < 0)
                    ycol = w->colY(w->colIndex(lst[i]));
                if (ycol < 0)
                    return false;

                if (w->colPlotDesignation(j) == SciDAVis::xErr)
                    ok = addErrorBars(w->colName(ycol), w, lst[i],
                                      (int)QwtErrorPlotCurve::Horizontal);
                else
                    ok = addErrorBars(w->colName(ycol), w, lst[i]);
            } else {
                type_of_i = (CurveType)style;
                ok = insertCurve(w, lst[i], style, startRow, endRow);
            }

            if (ok) {
                CurveLayout cl = initCurveLayout(type_of_i, otherCurves.size());
                cl.sSize = sSize;
                cl.lWidth = lWidth;
                updateCurveLayout(i, &cl);
            } else
                return false;
        }
    }
    updatePlot();
    return true;
}

bool Graph::insertCurve(Table *w, const QString &name, int style, int startRow, int endRow)
{ // provided for convenience
    int ycol = w->colIndex(name);
    int xcol = w->colX(ycol);

    bool succes = insertCurve(w, w->colName(xcol), w->colName(ycol), style, startRow, endRow);
    if (succes)
        emit modifiedGraph();
    return succes;
}

bool Graph::insertCurve(Table *w, int xcol, const QString &name, int style)
{
    return insertCurve(w, w->colName(xcol), w->colName(w->colIndex(name)), style);
}

bool Graph::insertCurve(Table *w, const QString &xColName, const QString &yColName, int style,
                        int startRow, int endRow)
{
    if (!w)
        return false;
    DataCurve *c = 0;

    switch (style) {
    case Histogram:
    case Box:
    case Pie:
        return false;
    case VerticalBars:
        c = new QwtBarCurve(QwtBarCurve::Vertical, w, xColName, yColName, startRow, endRow);
        c->setStyle(QwtPlotCurve::UserCurve);
        break;
    case HorizontalBars:
        c = new QwtBarCurve(QwtBarCurve::Horizontal, w, xColName, yColName, startRow, endRow);
        c->setStyle(QwtPlotCurve::UserCurve);
        break;
    default:
        c = new DataCurve(w, xColName, yColName, startRow, endRow);
        break;
    };

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = style;
    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(c);

    c->setPen(QPen(Qt::black, widthLine));

    if (!c->loadData())
        return false;

    addLegendItem(yColName);
    updatePlot();

    return true;
}

void Graph::plotVectorCurve(Table *w, const QStringList &colList, int style, int startRow,
                            int endRow)
{
    if (!w || !w->d_future_table || colList.count() != 4)
        return;

    auto resolveColIdx = [w](const QString &spec) -> int {
        if (spec.isEmpty())
            return -1;
        const int numCols = w->numCols();
        const QString tblPrefix = QString(w->name()) + QLatin1Char('_');
        if (spec.startsWith(tblPrefix)) {
            const QString shortName = spec.mid(tblPrefix.size());
            for (int i = 0; i < numCols; ++i) {
                if (const Column *col = w->column(i)) {
                    if (col->name() == shortName)
                        return i;
                }
            }
        }
        for (int i = 0; i < numCols; ++i) {
            if (const Column *col = w->column(i)) {
                if (col->name() == spec)
                    return i;
            }
        }
        const int fallback = w->colIndex(spec);
        return (fallback >= 0 && fallback < numCols && w->column(fallback) != nullptr) ? fallback
                                                                                       : -1;
    };

    QStringList canonicalCols;
    canonicalCols.reserve(4);
    for (int k = 0; k < 4; ++k) {
        const int idx = resolveColIdx(colList[k]);
        if (idx < 0)
            return;
        canonicalCols.append(w->colName(idx));
    }

    if (endRow < 0)
        endRow = w->numRows() - 1;

    VectorCurve *v = 0;
    if (style == VectXYAM)
        v = new VectorCurve(VectorCurve::XYAM, w, canonicalCols[0], canonicalCols[1],
                            canonicalCols[2], canonicalCols[3], startRow, endRow);
    else
        v = new VectorCurve(VectorCurve::XYXY, w, canonicalCols[0], canonicalCols[1],
                            canonicalCols[2], canonicalCols[3], startRow, endRow);

    if (!v)
        return;

    if (!v->loadData()) {
        delete v;
        return;
    }
    v->setStyle(QwtPlotCurve::NoCurve);

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = style;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(v);

    addLegendItem(canonicalCols[1]);
    updatePlot();
}

void Graph::updateVectorsLayout(int curve, const QColor &color, int width, int arrowLength,
                                int arrowAngle, bool filled, int position,
                                const QString &xEndColName, const QString &yEndColName)
{
    VectorCurve *vect = (VectorCurve *)this->curve(curve);
    if (!vect)
        return;

    vect->setColor(color);
    vect->setWidth(width);
    vect->setHeadLength(arrowLength);
    vect->setHeadAngle(arrowAngle);
    vect->fillArrowHead(filled);
    vect->setPosition(position);

    if (!xEndColName.isEmpty() && !yEndColName.isEmpty())
        vect->setVectorEnd(xEndColName, yEndColName);

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::updatePlot()
{
    if (d_is_ternary) {
        const double h = sqrt(3.0) / 2.0;
        d_plot->setAxisScale(QwtPlot::xBottom, -0.14, 1.14, 0.2);
        d_plot->setAxisScale(QwtPlot::yLeft, -0.12, h + 0.14, 0.2);
        for (int i = 0; i < QwtPlot::axisCnt; i++)
            d_plot->enableAxis(i, false);
    } else if (m_autoscale && !zoomOn() && d_active_tool == NULL) {
        for (int i = 0; i < QwtPlot::axisCnt; i++)
            d_plot->setAxisAutoScale(i);
    }

    d_plot->replot();
    updateMarkersBoundingRect();
    if (!d_is_ternary) {
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
    }

    if (isPiePlot()) {
        QwtPieCurve *c = (QwtPieCurve *)curve(0);
        c->updateBoundingRect();
    }

    d_plot->replot();
    d_zoomer[0]->setZoomBase();
    d_zoomer[1]->setZoomBase();
    updateStatReferenceLines();
    if (d_linked_parent)
        syncLinkedAxesFromParent();
    syncChildLinkedLayers();
}

void Graph::updateScale()
{
    const QwtScaleDiv *scDiv = d_plot->axisScaleDiv(QwtPlot::xBottom);
    QwtValueList lst = scDiv->ticks(QwtScaleDiv::MajorTick);

    double step = (lst.count() >= 2) ? fabs(lst[1] - lst[0]) : 0.0;

    if (!m_autoscale)
#if QWT_VERSION >= 0x050200
        d_plot->setAxisScale(QwtPlot::xBottom, scDiv->lowerBound(), scDiv->upperBound(), step);
#else
        d_plot->setAxisScale(QwtPlot::xBottom, scDiv->lBound(), scDiv->hBound(), step);
#endif

    scDiv = d_plot->axisScaleDiv(QwtPlot::yLeft);
    lst = scDiv->ticks(QwtScaleDiv::MajorTick);

    step = (lst.count() >= 2) ? fabs(lst[1] - lst[0]) : 0.0;

    if (!m_autoscale)
#if QWT_VERSION >= 0x050200
        d_plot->setAxisScale(QwtPlot::yLeft, scDiv->lowerBound(), scDiv->upperBound(), step);
#else
        d_plot->setAxisScale(QwtPlot::yLeft, scDiv->lBound(), scDiv->hBound(), step);
#endif

    d_plot->replot();
    updateMarkersBoundingRect();
    updateSecondaryAxis(QwtPlot::xTop);
    updateSecondaryAxis(QwtPlot::yRight);

    d_plot->replot(); // TODO: avoid 2nd replot!
    syncChildLinkedLayers();
}

void Graph::setBarsGap(int curve, int gapPercent, int offset)
{
    QwtBarCurve *bars = (QwtBarCurve *)this->curve(curve);
    if (!bars)
        return;

    if (bars->gap() == gapPercent && bars->offset() == offset)
        return;

    bars->setGap(gapPercent);
    bars->setOffset(offset);
}

void Graph::removePie()
{
    if (legendMarkerID >= 0) {
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk)
            mrk->setText({});
    }

    d_plot->removeCurve(c_keys[0]);
    d_plot->replot();

    c_keys.resize(0);
    c_type.resize(0);
    n_curves = 0;
    emit modifiedGraph();
}

void Graph::removeCurves(const QString &s)
{
    QList<int> keys = d_plot->curveKeys();
    for (int i = 0; i < (int)keys.count(); i++) {
        QwtPlotItem *it = d_plot->plotItem(keys[i]);
        if (!it)
            continue;

        if (it->title().text() == s) {
            removeCurve(i);
            continue;
        }

        if (it->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;
        if (((PlotCurve *)it)->type() == Function)
            continue;

        if (((DataCurve *)it)->plotAssociation().contains(QRegExp("\b" + s + "\b")))
            removeCurve(i);
    }
    d_plot->replot();
}

void Graph::removeCurve(const QString &s)
{
    removeCurve(plotItemsList().indexOf(s));
}

void Graph::removeCurve(int index)
{
    if (index < 0 || index >= n_curves)
        return;

    QwtPlotItem *it = plotItem(index);
    if (!it)
        return;

    removeLegendItem(index);

    if (it->rtti() != QwtPlotItem::Rtti_PlotSpectrogram) {
        if (((PlotCurve *)it)->type() == ErrorBars)
            ((QwtErrorPlotCurve *)it)->detachFromMasterCurve();
        else if (((PlotCurve *)it)->type() != Function)
            ((DataCurve *)it)->clearErrorBars();

        if (d_fit_curves.contains((QwtPlotCurve *)it)) {
            int i = d_fit_curves.indexOf((QwtPlotCurve *)it);
            if (i >= 0 && i < d_fit_curves.size())
                d_fit_curves.removeAt(i);
        }
    }

    if (d_range_selector && curve(index) == d_range_selector->selectedCurve()) {
        if (n_curves > 1 && (index - 1) >= 0)
            d_range_selector->setSelectedCurve(curve(index - 1));
        else if (n_curves > 1 && index + 1 < n_curves)
            d_range_selector->setSelectedCurve(curve(index + 1));
        else
            delete d_range_selector;
    }

    d_plot->removeCurve(c_keys[index]);
    n_curves--;

    for (int i = index; i < n_curves; i++) {
        c_type[i] = c_type[i + 1];
        c_keys[i] = c_keys[i + 1];
    }
    c_type.resize(n_curves);
    c_keys.resize(n_curves);
    emit modifiedGraph();
}

void Graph::removeLegendItem(int index)
{
    if (legendMarkerID < 0 || c_type[index] == ErrorBars)
        return;

    Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
    if (!mrk)
        return;

    if (isPiePlot()) {
        mrk->setText({});
        return;
    }

    QString text = mrk->text();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList items = text.split("\n", Qt::SkipEmptyParts);
#else
    QStringList items = text.split("\n", QString::SkipEmptyParts);
#endif

    if (index >= (int)items.count())
        return;

    QStringList l = items.filter("\\c{" + QString::number(index + 1) + "}");
    if (!l.isEmpty())
        items.removeAll(l[0]); // remove the corresponding legend string
    text = items.join("\n") + "\n";

    QRegExp itemCmd("\\\\c\\{(\\d+)\\}");
    int pos = 0;
    while ((pos = itemCmd.indexIn(text, pos)) != -1) {
        int nr = itemCmd.cap(1).toInt();
        if (nr > index) {
            QString subst = QString("\\c{") + QString::number(nr - 1) + "}";
            text.replace(pos, itemCmd.matchedLength(), subst);
            pos += subst.length();
        } else
            pos += itemCmd.matchedLength();
    }

    mrk->setText(text);
}

void Graph::addLegendItem(const QString &colName)
{
    if (legendMarkerID >= 0) {
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk) {
            QString label = colName;
            if (n_curves > 0) {
                DataCurve *dc = dynamic_cast<DataCurve *>(curve(n_curves - 1));
                if (dc && dc->table()) {
                    const QString prefix = QString(dc->table()->name()) + "_";
                    if (label.startsWith(prefix) && label.length() > prefix.length())
                        label = label.mid(prefix.length());
                    d_last_auto_curve_labels[n_curves - 1] = label;
                }
            }
            QString text = mrk->text();
            if (text.isEmpty() || text.endsWith("\n", Qt::CaseSensitive))
                text.append("\\c{" + QString::number(curves()) + "}" + label + "\n");
            else
                text.append("\n\\c{" + QString::number(curves()) + "}" + label + "\n");

            mrk->setText(text);
        }
    }
}

void Graph::reconstructLegend()
{
    Legend *mrk = legend();
    if (!mrk) {
        for (int i = 0; i < d_texts.size(); ++i) {
            Legend *cand = dynamic_cast<Legend *>(d_plot->marker(d_texts[i]));
            if (cand
                && (cand->text().contains("\\c{") || cand->text().contains("\\p{")
                    || cand->text().contains("%("))) {
                legendMarkerID = d_texts[i];
                mrk = cand;
                break;
            }
        }
    }

    if (!mrk) {
        newLegend();
        return;
    }

    if (isPiePlot())
        mrk->setText(pieLegendText());
    else
        mrk->setText(legendText());

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::contextMenuEvent(QContextMenuEvent *e)
{
    if (selectedMarker >= 0) {
        emit showMarkerPopupMenu();
        return;
    }

    QPoint pos = d_plot->canvas()->mapFrom(d_plot, e->pos());
    int dist, point;
    const long curve = d_plot->closestCurve(pos.x(), pos.y(), dist, point);
    const QwtPlotCurve *c = (QwtPlotCurve *)d_plot->curve(curve);

    if (c && dist < 10) // 10 pixels tolerance
        emit showCurveContextMenu(curve);
    else
        emit showContextMenu();

    e->accept();
}

void Graph::closeEvent(QCloseEvent *e)
{
    emit closedGraph();
    e->accept();
}

bool Graph::zoomOn()
{
    return (d_zoomer[0]->isEnabled() || d_zoomer[1]->isEnabled());
}

void Graph::zoomed(const QwtDoubleRect &)
{
    syncChildLinkedLayers();
    emit modifiedGraph();
}

void Graph::zoom(bool on)
{
    d_zoomer[0]->setEnabled(on);
    d_zoomer[1]->setEnabled(on);
    for (int i = 0; i < n_curves; i++) {
        Spectrogram *sp = (Spectrogram *)this->curve(i);
        if (sp && sp->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            if (sp->colorScaleAxis() == QwtPlot::xBottom || sp->colorScaleAxis() == QwtPlot::yLeft)
                d_zoomer[0]->setEnabled(false);
            else
                d_zoomer[1]->setEnabled(false);
        }
    }

    QCursor cursor = QCursor(QPixmap(":/lens.xpm"), -1, -1);
    if (on)
        d_plot->canvas()->setCursor(cursor);
    else
        d_plot->canvas()->setCursor(Qt::ArrowCursor);
}

void Graph::zoomOut()
{
    d_zoomer[0]->zoom(-1);
    d_zoomer[1]->zoom(-1);

    updateSecondaryAxis(QwtPlot::xTop);
    updateSecondaryAxis(QwtPlot::yRight);
    syncChildLinkedLayers();
}

void Graph::drawText(bool on)
{
    QCursor c = QCursor(Qt::IBeamCursor);
    if (on)
        d_plot->canvas()->setCursor(c);
    else
        d_plot->canvas()->setCursor(Qt::ArrowCursor);

    drawTextOn = on;
}

ImageMarker *Graph::addImage(ImageMarker *mrk)
{
    if (!mrk)
        return 0;

    ImageMarker *mrk2 = new ImageMarker(mrk->fileName());

    int imagesOnPlot = d_images.size();
    d_images.resize(++imagesOnPlot);
    d_images[imagesOnPlot - 1] = d_plot->insertMarker(mrk2);

    mrk2->setBoundingRect(mrk->xValue(), mrk->yValue(), mrk->right(), mrk->bottom());
    return mrk;
}

ImageMarker *Graph::addImage(const QString &fileName)
{
    if (fileName.isEmpty() || !QFile::exists(fileName)) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(
                    0, tr("File open error"),
                    tr("Image file: <p><b> %1 </b><p>does not exist anymore!").arg(fileName));
        }
        return 0;
    }

    ImageMarker *mrk = new ImageMarker(fileName);
    int imagesOnPlot = d_images.size();
    d_images.resize(++imagesOnPlot);
    d_images[imagesOnPlot - 1] = d_plot->insertMarker(mrk);

    QSize picSize = mrk->pixmap().size();
    int w = d_plot->canvas()->width();
    if (picSize.width() > w)
        picSize.setWidth(w);

    int h = d_plot->canvas()->height();
    if (picSize.height() > h)
        picSize.setHeight(h);

    mrk->setSize(picSize);
    d_plot->replot();

    emit modifiedGraph();
    return mrk;
}

void Graph::insertImageMarker(const QStringList &lst, int fileVersion)
{
    QString fn = lst[1];
    if (!QFile::exists(fn)) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(0, tr("File open error"),
                                 tr("Image file: <p><b> %1 </b><p>does not exist anymore!").arg(fn));
        }
    } else {
        ImageMarker *mrk = new ImageMarker(fn);
        if (!mrk)
            return;

        int imagesOnPlot = d_images.size();
        d_images.resize(++imagesOnPlot);
        d_images[imagesOnPlot - 1] = d_plot->insertMarker(mrk);

        if (fileVersion < 86) {
            mrk->setOrigin(QPoint(lst[2].toInt(), lst[3].toInt()));
            mrk->setSize(QSize(lst[4].toInt(), lst[5].toInt()));
        } else if (fileVersion < 90) {
            double left = lst[2].toDouble();
            double right = left + lst[4].toDouble();
            double top = lst[3].toDouble();
            double bottom = top - lst[5].toDouble();
            mrk->setBoundingRect(left, top, right, bottom);
        } else
            mrk->setBoundingRect(lst[2].toDouble(), lst[3].toDouble(), lst[4].toDouble(),
                                 lst[5].toDouble());
    }
}

void Graph::drawLine(bool on, bool arrow)
{
    drawLineOn = on;
    drawArrowOn = arrow;
    if (!on)
        emit drawLineEnded(true);
}

bool Graph::modifyFunctionCurve(ApplicationWindow *parent, int curve, int type,
                                const QStringList &formulas, const QString &var,
                                QList<double> &ranges, int points)
{
    FunctionCurve *c = (FunctionCurve *)this->curve(curve);
    if (!c)
        return false;

    if (c->functionType() == type && c->variable() == var && c->formulas() == formulas
        && c->startRange() == ranges[0] && c->endRange() == ranges[1] && c->dataSize() == points)
        return true;

    FunctionCurve backup(parent);
    backup.copy(c);

    c->setFunctionType((FunctionCurve::FunctionType)type);
    c->setRange(ranges[0], ranges[1]);
    c->setFormulas(formulas);
    c->setVariable(var);
    if (!c->loadData(points)) {
        c->copy(&backup);
        c->loadData(points);
        return false;
    }

    if (legendMarkerID >= 0) { // update the legend marker
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk) {
            QString text = (mrk->text()).replace(backup.legend(), c->legend());
            mrk->setText(text);
        }
    }
    updatePlot();
    emit modifiedGraph();
    return true;
}

QString Graph::generateFunctionName(const QString &name)
{
    int index = 1;
    QString newName = name + QString::number(index);

    QStringList lst;
    for (int i = 0; i < n_curves; i++) {
        PlotCurve *c = (PlotCurve *)this->curve(i);
        if (!c)
            continue;

        if (c->type() == Function)
            lst << c->title().text();
    }

    while (lst.contains(newName)) {
        newName = name + QString::number(++index);
    }
    return newName;
}

bool Graph::addFunctionCurve(ApplicationWindow *parent, int type, const QStringList &formulas,
                             const QString &var, QList<double> &ranges, int points,
                             const QString &title)
{
    QString name;
    if (!title.isEmpty())
        name = title;
    else
        name = generateFunctionName();

    FunctionCurve *c = new FunctionCurve(parent, (FunctionCurve::FunctionType)type, name);
    c->setPen(QPen(QColor(Qt::black), widthLine));
    c->setRange(ranges[0], ranges[1]);
    c->setFormulas(formulas);
    c->setVariable(var);
    if (!c->loadData(points)) {
        delete c;
        return false;
    }

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = Line;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(c);

    addLegendItem(c->legend());
    auto ml = qobject_cast<MultiLayer *>(parent->d_workspace.activeSubWindow());
    if (ml &&  ml->status() != MyWidget::Minimized) {
        updatePlot();

        emit modifiedGraph();
    }
    return true;
}

bool Graph::insertFunctionCurve(ApplicationWindow *parent, const QStringList &func_spec, int points,
                                int fileVersion)
{
    int type = FunctionCurve::Normal;
    QStringList formulas;
    QString var, name;
    QList<double> ranges;

    QStringList curve = func_spec[0].split(",");
    if (fileVersion >= 0x000105) {
        // SciDAVis 0.1.4 and 0.2.0 crash when trying to save a function curve;
        // thus, it's safe to assume every version from 0.1.5 on uses the new format,
        // even though 0.2.0 doesn't actually contain the revised code yet
        if (curve.size() < 5)
            return false;
        type = curve[0].toInt();
        name = curve.mid(1, curve.size() - 4).join(",");
        var = curve[curve.size() - 3];
        ranges += curve[curve.size() - 2].toDouble();
        ranges += curve[curve.size() - 1].toDouble();

        if (func_spec.size() > 1)
            formulas << func_spec[1];
        else
            formulas << QString();
        if (type != FunctionCurve::Normal)
            formulas << (func_spec.size() > 2 ? func_spec[2] : QString());

        FunctionCurve *c = new FunctionCurve(parent, (FunctionCurve::FunctionType)type, name);
        c->setSuppressScriptErrors(true);
        c->setPen(QPen(QColor(Qt::black), widthLine));
        c->setRange(ranges[0], ranges[1]);
        c->setFormulas(formulas);
        c->setVariable(var);
        if (!c->loadData(points)) {
            delete c;
            return false;
        }
        c->setSuppressScriptErrors(false);

        c_type.resize(++n_curves);
        c_type[n_curves - 1] = Line;

        c_keys.resize(n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(c);

        addLegendItem(c->legend());
        auto ml = parent ? qobject_cast<MultiLayer *>(parent->d_workspace.activeSubWindow()) : nullptr;
        if (ml && ml->status() != MyWidget::Minimized) {
            updatePlot();
            emit modifiedGraph();
        }
        return true;

    } else if (fileVersion < 87) {
        if (curve[0][0] == 'f') {
            type = FunctionCurve::Normal;
            formulas += curve[0].section('=', 1, 1);
            var = curve[1];
            ranges += curve[2].toDouble();
            ranges += curve[3].toDouble();
        } else if (curve[0][0] == 'X') {
            type = FunctionCurve::Parametric;
            formulas += curve[0].section('=', 1, 1);
            formulas += curve[1].section('=', 1, 1);
            var = curve[2];
            ranges += curve[3].toDouble();
            ranges += curve[4].toDouble();
        } else if (curve[0][0] == 'R') {
            type = FunctionCurve::Polar;
            formulas += curve[0].section('=', 1, 1);
            formulas += curve[1].section('=', 1, 1);
            var = curve[2];
            ranges += curve[3].toDouble();
            ranges += curve[4].toDouble();
        }
    } else {
        type = curve[0].toInt();
        name = curve[1];

        if (type == FunctionCurve::Normal) {
            formulas << curve[2];
            var = curve[3];
            ranges += curve[4].toDouble();
            ranges += curve[5].toDouble();
        } else if (type == FunctionCurve::Polar || type == FunctionCurve::Parametric) {
            formulas << curve[2];
            formulas << curve[3];
            var = curve[4];
            ranges += curve[5].toDouble();
            ranges += curve[6].toDouble();
        }
    }
    return addFunctionCurve(parent, type, formulas, var, ranges, points, name);
}

void Graph::createTable(const QString &curveName)
{
    if (curveName.isEmpty())
        return;

    const QwtPlotCurve *cv = curve(curveName);
    if (!cv)
        return;

    createTable(cv);
}

void Graph::createTable(const QwtPlotCurve *curve)
{
    if (!curve)
        return;

    int size = curve->dataSize();

    Column *xCol = new Column(tr("1", "curve data table x column name"), SciDAVis::ColumnMode::Numeric);
    Column *yCol = new Column(tr("2", "curve data table y column name"), SciDAVis::ColumnMode::Numeric);
    xCol->setPlotDesignation(SciDAVis::X);
    yCol->setPlotDesignation(SciDAVis::Y);
    for (int i = 0; i < size; i++) {
        xCol->setValueAt(i, curve->x(i));
        yCol->setValueAt(i, curve->y(i));
    }
    QString legend = tr("Data set generated from curve") + ": " + curve->title().text();
    emit createTable(tr("Curve data %1").arg(1), legend, QList<Column *>() << xCol << yCol);
}

QString Graph::saveToString(bool saveAsTemplate)
{
    QString s = "<graph>\n";
    s += "ggeometry\t";
    s += QString::number(this->pos().x()) + "\t";
    s += QString::number(this->pos().y()) + "\t";
    s += QString::number(this->frameGeometry().width()) + "\t";
    s += QString::number(this->frameGeometry().height()) + "\n";
    s += saveTitle();
    s += "<Antialiasing>" + QString::number(d_antialiasing) + "</Antialiasing>\n";
    s += "Background\t" + COLORNAME(d_plot->paletteBackgroundColor()) + "\t";
    s += QString::number(d_plot->paletteBackgroundColor().alpha()) + "\n";
    s += "Margin\t" + QString::number(d_plot->margin()) + "\n";
    s += "Border\t" + QString::number(d_plot->lineWidth()) + "\t" + COLORNAME(d_plot->frameColor())
            + "\n";
    s += grid()->saveToString();
    s += saveEnabledAxes();
    s += "AxesTitles\t" + saveScaleTitles();
    s += saveAxesTitleColors();
    s += saveAxesTitleAlignement();
    s += saveFonts();
    s += saveEnabledTickLabels();
    s += saveAxesColors();
    s += saveAxesBaseline();
    s += saveCanvas();

    if (!saveAsTemplate)
        s += saveCurves();

    s += saveScale();
    s += saveAxesFormulas();
    s += saveLabelsFormat();
    s += saveAxesLabelsType();
    s += saveTicksType();
    s += "TicksLength\t" + QString::number(minorTickLength()) + "\t"
            + QString::number(majorTickLength()) + "\n";
    s += "DrawAxesBackbone\t" + QString::number(drawAxesBackbone) + "\n";
    s += "AxesLineWidth\t" + QString::number(d_plot->axesLinewidth()) + "\n";
    s += saveLabelsRotation();
    s += saveMarkers();

    // Phase 5 lossless persistence tags
    {
        auto escAttr = [](const QString &raw) -> QString {
            QString out;
            out.reserve(raw.size() + 8);
            for (QChar ch : raw) {
                const ushort u = ch.unicode();
                if (ch == '&')
                    out += "&amp;";
                else if (ch == '"')
                    out += "&quot;";
                else if (ch == '<')
                    out += "&lt;";
                else if (ch == '>')
                    out += "&gt;";
                else if (ch == '\n')
                    out += "&#10;";
                else if (ch == '\r')
                    out += "&#13;";
                else if (ch == '\t')
                    out += "&#9;";
                else if (u < 32)
                    out += QString("&#%1;").arg(int(u));
                else
                    out += ch;
            }
            static const char *const kLegacyTokens[] = {
                "ggeometry",         "EnabledAxes",      "AxesBaseline",     "EnabledTicks",
                "MajorTicks",        "MinorTicks",       "TicksLength",      "EnabledTickLabels",
                "AxesColors",        "AxesNumberColors", "PieCurve",         "FunctionCurve",
                "ErrorBars",         "PlotTitle",        "TitleFont",        "AxesTitles",
                "AxesTitleColors",   "AxesTitleAlignment","ScaleFont",       "AxisFont",
                "AxesFormulas",      "LabelsFormat",     "LabelsRotation",   "DrawAxesBackbone",
                "AxesLineWidth",     "CanvasFrame",      "CanvasBackground", "Legend",
                "textMarker",        "lineMarker",       "ImageMarker",      "AxisType",
                "AxesTickLabelsCol"
            };
            for (const char *tok : kLegacyTokens) {
                const QString qtok = QString::fromLatin1(tok);
                if (out.contains(qtok)) {
                    const QString repl =
                            QString("&#%1;").arg(int(qtok.at(0).unicode())) + qtok.mid(1);
                    out.replace(qtok, repl);
                }
            }
            return out;
        };

        int legCols = 1;
        if (Legend *leg = legend())
            legCols = leg->numColumns();

        QStringList textColsParts;
        for (int ti = 0; ti < d_texts.size(); ++ti) {
            Legend *tm = dynamic_cast<Legend *>(d_plot->marker(d_texts[ti]));
            if (tm && tm->numColumns() > 1)
                textColsParts << QString("%1:%2").arg(ti).arg(tm->numColumns());
        }

        QStringList alphaParts;
        QStringList stackParts;
        for (int ci = 0; ci < n_curves; ++ci) {
            QwtPlotCurve *cv = curve(ci);
            if (!cv)
                continue;
            const int la = cv->pen().color().alpha();
            const int ba = cv->brush().color().alpha();
            const int spa = cv->symbol().pen().color().alpha();
            const int sba = cv->symbol().brush().color().alpha();
            if (la < 255 || ba < 255 || spa < 255 || sba < 255) {
                alphaParts << QString("%1:%2,%3,%4,%5")
                                      .arg(ci)
                                      .arg(la)
                                      .arg(ba)
                                      .arg(spa)
                                      .arg(sba);
            }
            if (!saveAsTemplate) {
                PlotCurve *pc = dynamic_cast<PlotCurve *>(cv);
                if (pc
                    && (qAbs(pc->stackOffsetX()) > 1e-15 || qAbs(pc->stackOffsetY()) > 1e-15
                        || pc->stackNormalize01())) {
                    stackParts << QString("%1:%2,%3,%4")
                                          .arg(ci)
                                          .arg(QString::number(pc->stackOffsetX(), 'g', 15))
                                          .arg(QString::number(pc->stackOffsetY(), 'g', 15))
                                          .arg(pc->stackNormalize01() ? 1 : 0);
                }
            }
        }

        s += QString("<ToriBackbones draw=\"%1\" b0=\"%2\" b1=\"%3\" b2=\"%4\" b3=\"%5\" "
                     "legendCols=\"%6\" textCols=\"%7\" curveAlphas=\"%8\" stackOffsets=\"%9\"/>\n")
                     .arg(drawAxesBackbone ? 1 : 0)
                     .arg(d_axis_backbone[0] ? 1 : 0)
                     .arg(d_axis_backbone[1] ? 1 : 0)
                     .arg(d_axis_backbone[2] ? 1 : 0)
                     .arg(d_axis_backbone[3] ? 1 : 0)
                     .arg(legCols)
                     .arg(textColsParts.join(";"))
                     .arg(alphaParts.join(";"))
                     .arg(stackParts.join(";"));

        s += QString("<ToriAutoAxisTitles unitFormat=\"%1\" a0=\"%2\" a1=\"%3\" a2=\"%4\" a3=\"%5\"/>\n")
                     .arg(d_axis_unit_format == Column::UnitFormat::ISIJSlash
                                  ? QStringLiteral("ISIJSlash")
                                  : QStringLiteral("Paren"))
                     .arg(d_axis_title_auto[0] ? 1 : 0)
                     .arg(d_axis_title_auto[1] ? 1 : 0)
                     .arg(d_axis_title_auto[2] ? 1 : 0)
                     .arg(d_axis_title_auto[3] ? 1 : 0);

        for (const ReferenceLineSpec &rl : d_ref_lines) {
            s += QString("<ToriRefLine orient=\"%1\" value=\"%2\" stat=\"%3\" color=\"%4\" "
                         "style=\"%5\" width=\"%6\" label=\"%7\" fill=\"%8\" fillColor=\"%9\"/>\n")
                         .arg(rl.orientation)
                         .arg(QString::number(rl.value, 'g', 15))
                         .arg(escAttr(rl.statMode))
                         .arg(rl.color.name(QColor::HexArgb))
                         .arg(int(rl.style))
                         .arg(QString::number(rl.width, 'g', 8))
                         .arg(escAttr(rl.label))
                         .arg(rl.fillToAxis ? 1 : 0)
                         .arg(rl.fillColor.name(QColor::HexArgb));
        }

        if (!saveAsTemplate) {
            for (const ShadedBandSpec &bs : d_shaded_bands) {
                QStringList xStr, ylStr, yhStr;
                const int m = qMin(bs.x.size(), qMin(bs.yLow.size(), bs.yHigh.size()));
                xStr.reserve(m);
                ylStr.reserve(m);
                yhStr.reserve(m);
                for (int k = 0; k < m; ++k) {
                    xStr << QString::number(bs.x[k], 'g', 12);
                    ylStr << QString::number(bs.yLow[k], 'g', 12);
                    yhStr << QString::number(bs.yHigh[k], 'g', 12);
                }
                s += QString("<ToriRefLine kind=\"band\" color=\"%1\" title=\"%2\" x=\"%3\" "
                             "yLow=\"%4\" yHigh=\"%5\"/>\n")
                             .arg(bs.fillColor.name(QColor::HexArgb))
                             .arg(escAttr(bs.title))
                             .arg(xStr.join(","))
                             .arg(ylStr.join(","))
                             .arg(yhStr.join(","));
            }
        }

        if (d_linked_parent) {
            int parentIdx = -1;
            if (MultiLayer *ml = multiLayerParent())
                parentIdx = ml->layersList().indexOf(d_linked_parent.data());
            if (parentIdx >= 0) {
                s += QString("<ToriLinkedLayer parentIdx=\"%1\" xMode=\"%2\" xExpr=\"%3\" "
                             "yMode=\"%4\" yExpr=\"%5\"/>\n")
                             .arg(parentIdx)
                             .arg(d_x_link_mode)
                             .arg(escAttr(d_x_link_expr))
                             .arg(d_y_link_mode)
                             .arg(escAttr(d_y_link_expr));
            }
        }

        for (int a = 0; a < 4; ++a) {
            const AxisBreakSpec &ab = d_axis_breaks[a];
            if (ab.enabled) {
                s += QString("<ToriAxisBreak axis=\"%1\" enabled=\"1\" from=\"%2\" to=\"%3\" "
                             "pos=\"%4\" stepBefore=\"%5\" stepAfter=\"%6\" logBefore=\"%7\" "
                             "logAfter=\"%8\"/>\n")
                             .arg(a)
                             .arg(QString::number(ab.breakFrom, 'g', 15))
                             .arg(QString::number(ab.breakTo, 'g', 15))
                             .arg(QString::number(ab.positionPct, 'g', 8))
                             .arg(QString::number(ab.stepBefore, 'g', 15))
                             .arg(QString::number(ab.stepAfter, 'g', 15))
                             .arg(ab.log10Before ? 1 : 0)
                             .arg(ab.log10After ? 1 : 0);
            }
            if (qAbs(d_tick_divide_factor[a] - 1.0) > 1e-15 || !d_tick_prefix[a].isEmpty()
                || !d_tick_suffix[a].isEmpty()) {
                s += QString("<ToriTickTransform axis=\"%1\" factor=\"%2\" prefix=\"%3\" "
                             "suffix=\"%4\"/>\n")
                             .arg(a)
                             .arg(QString::number(d_tick_divide_factor[a], 'g', 15))
                             .arg(escAttr(d_tick_prefix[a]))
                             .arg(escAttr(d_tick_suffix[a]));
            }
        }

        if (d_is_ternary) {
            s += QString("<ToriTernary enabled=\"1\" a=\"%1\" b=\"%2\" c=\"%3\"/>\n")
                         .arg(escAttr(d_ternary_labels[0]))
                         .arg(escAttr(d_ternary_labels[1]))
                         .arg(escAttr(d_ternary_labels[2]));
        }
    }

    s += "</graph>\n";
    return s;
}

void Graph::showIntensityTable()
{
    ImageMarker *mrk = (ImageMarker *)d_plot->marker(selectedMarker);
    if (!mrk)
        return;

    emit createIntensityTable(mrk->fileName());
}

void Graph::updateMarkersBoundingRect()
{
    if (d_is_ternary) {
        d_plot->replot();
        return;
    }
    for (int i = 0; i < (int)d_lines.size(); i++) {
        ArrowMarker *mrkL = (ArrowMarker *)d_plot->marker(d_lines[i]);
        if (mrkL)
            mrkL->updateBoundingRect();
    }
    for (int i = 0; i < (int)d_texts.size(); i++) {
        Legend *mrkT = (Legend *)d_plot->marker(d_texts[i]);
        if (mrkT)
            mrkT->updateOrigin();
    }

    for (int i = 0; i < (int)d_images.size(); i++) {
        ImageMarker *mrk = (ImageMarker *)d_plot->marker(d_images[i]);
        if (mrk)
            mrk->updateBoundingRect();
    }
    d_plot->replot();
}
void Graph::hideEvent(QHideEvent *e)
{
    Q_UNUSED(e);
    hidden_size = size();
}

void Graph::resizeEvent(QResizeEvent *e)
{
    if (ignoreResize)
        return;
    QSize oldSize = e->oldSize();
    if (!this->isVisible() || !oldSize.isValid())
        oldSize = hidden_size;
    if (!oldSize.isValid())
        return;

    if (autoScaleFonts) {
        QSize size = e->size();

        double ratio = (double)size.height() / (double)oldSize.height();
        scaleFonts(ratio);
    }

    d_plot->resize(e->size());
}

void Graph::scaleFonts(double factor)
{
    for (int i = 0; i < (int)d_texts.size(); i++) {
        Legend *mrk = (Legend *)d_plot->marker(d_texts[i]);
        QFont font = mrk->font();
        font.setPointSizeF(factor * font.pointSizeF());
        mrk->setFont(font);
    }
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QFont font = axisFont(i);
        font.setPointSizeF(factor * font.pointSizeF());
        d_plot->setAxisFont(i, font);

        QwtText title = d_plot->axisTitle(i);
        font = title.font();
        font.setPointSizeF(factor * font.pointSizeF());
        title.setFont(font);
        d_plot->setAxisTitle(i, title);
    }

    QwtText title = d_plot->title();
    QFont font = title.font();
    font.setPointSizeF(factor * font.pointSizeF());
    title.setFont(font);
    d_plot->setTitle(title);

    d_plot->replot();
}

void Graph::setMargin(int d)
{
    if (d_plot->margin() == d)
        return;

    d_plot->setMargin(d);
    emit modifiedGraph();
}

void Graph::setFrame(int width, const QColor &color)
{
    if (d_plot->frameColor() == color && width == d_plot->lineWidth())
        return;

    QPalette pal = d_plot->palette();
    pal.setColor(QPalette::WindowText, color);
    d_plot->setPalette(pal);

    d_plot->setLineWidth(width);
}

void Graph::setBackgroundColor(const QColor &color)
{
    QPalette p = d_plot->palette();
    p.setColor(QPalette::Window, color);
    d_plot->setPalette(p);

    d_plot->setAutoFillBackground(true);
    emit modifiedGraph();
}

void Graph::setCanvasBackground(const QColor &color)
{
    d_plot->setCanvasBackground(color);
    emit modifiedGraph();
}

Qt::BrushStyle Graph::getBrushStyle(int style)
{
    Qt::BrushStyle brushStyle;
    switch (style) {
    case 0:
        brushStyle = Qt::SolidPattern;
        break;
    case 1:
        brushStyle = Qt::HorPattern;
        break;
    case 2:
        brushStyle = Qt::VerPattern;
        break;
    case 3:
        brushStyle = Qt::CrossPattern;
        break;
    case 4:
        brushStyle = Qt::BDiagPattern;
        break;
    case 5:
        brushStyle = Qt::FDiagPattern;
        break;
    case 6:
        brushStyle = Qt::DiagCrossPattern;
        break;
    case 7:
        brushStyle = Qt::Dense1Pattern;
        break;
    case 8:
        brushStyle = Qt::Dense2Pattern;
        break;
    case 9:
        brushStyle = Qt::Dense3Pattern;
        break;
    case 10:
        brushStyle = Qt::Dense4Pattern;
        break;
    case 11:
        brushStyle = Qt::Dense5Pattern;
        break;
    case 12:
        brushStyle = Qt::Dense6Pattern;
        break;
    case 13:
        brushStyle = Qt::Dense7Pattern;
        break;
    default:
        throw runtime_error("invalid brush style");
    }
    return brushStyle;
}

QString Graph::penStyleName(Qt::PenStyle style)
{
    if (style == Qt::SolidLine)
        return "SolidLine";
    else if (style == Qt::DashLine)
        return "DashLine";
    else if (style == Qt::DotLine)
        return "DotLine";
    else if (style == Qt::DashDotLine)
        return "DashDotLine";
    else if (style == Qt::DashDotDotLine)
        return "DashDotDotLine";
    else if (style == Qt::CustomDashLine)
        return "CustomDashLine";
    else
        return "SolidLine";
}

Qt::PenStyle Graph::getPenStyle(int style)
{
    Qt::PenStyle linePen;
    switch (style) {
    case 0:
        linePen = Qt::SolidLine;
        break;
    case 1:
        linePen = Qt::DashLine;
        break;
    case 2:
        linePen = Qt::DotLine;
        break;
    case 3:
        linePen = Qt::DashDotLine;
        break;
    case 4:
        linePen = Qt::DashDotDotLine;
        break;
    case 5:
        linePen = Qt::CustomDashLine;
        break;
    default:
        throw runtime_error("invalid pen style");
    }
    return linePen;
}

Qt::PenStyle Graph::getPenStyle(const QString &s)
{
    Qt::PenStyle style = Qt::SolidLine;
    if (s == "SolidLine")
        style = Qt::SolidLine;
    else if (s == "DashLine")
        style = Qt::DashLine;
    else if (s == "DotLine")
        style = Qt::DotLine;
    else if (s == "DashDotLine")
        style = Qt::DashDotLine;
    else if (s == "DashDotDotLine")
        style = Qt::DashDotDotLine;
    else if (s == "CustomDashLine")
        style = Qt::CustomDashLine;
    return style;
}

int Graph::obsoleteSymbolStyle(int type)
{
    if (type <= 4)
        return type + 1;
    else
        return type + 2;
}

int Graph::curveType(int curveIndex)
{
    if (curveIndex < (int)c_type.size() && curveIndex >= 0)
        return c_type[curveIndex];
    else
        return -1;
}

void Graph::showPlotErrorMessage(QWidget *parent, const QStringList &emptyColumns)
{
    QApplication::restoreOverrideCursor();
    if (ApplicationWindow::isNonInteractiveContext(parent))
        return;

    int n = (int)emptyColumns.count();
    if (n > 1) {
        QString columns;
        for (int i = 0; i < n; i++)
            columns += "<p><b>" + emptyColumns[i] + "</b></p>";

        QMessageBox::warning(parent, tr("Warning"),
                             tr("The columns") + ": " + columns
                                     + tr("are empty and will not be added to the plot!"));
    } else if (n == 1)
        QMessageBox::warning(parent, tr("Warning"),
                             tr("The column") + " <b>" + emptyColumns[0] + "</b> "
                                     + tr("is empty and will not be added to the plot!"));
}

void Graph::showTitleContextMenu()
{
    QMenu titleMenu(this);
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("cut")), tr("&Cut"), this,
                        SLOT(cutTitle()));
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("copy")), tr("&Copy"), this,
                        SLOT(copyTitle()));
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("delete")), tr("&Delete"), this,
                        SLOT(removeTitle()));
    titleMenu.addSeparator();
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")), tr("&Properties..."),
                        this, SIGNAL(viewTitleDialog()));
    ModernUiTheme::upgradeMenuIcons(&titleMenu);
    titleMenu.exec(QCursor::pos());
}

void Graph::cutTitle()
{
    QApplication::clipboard()->setText(d_plot->title().text(), QClipboard::Clipboard);
    removeTitle();
}

void Graph::copyTitle()
{
    QApplication::clipboard()->setText(d_plot->title().text(), QClipboard::Clipboard);
}

void Graph::removeAxisTitle()
{
    int axis = (selectedAxis + 2) % 4; // inconsistent notation in Qwt enumerations between
    // QwtScaleDraw::alignment and QwtPlot::Axis
    d_plot->setAxisTitle(axis, QString());
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::cutAxisTitle()
{
    copyAxisTitle();
    removeAxisTitle();
}

void Graph::copyAxisTitle()
{
    int axis = (selectedAxis + 2) % 4; // unconsistent notation in Qwt enumerations between
    // QwtScaleDraw::alignment and QwtPlot::Axis
    QApplication::clipboard()->setText(d_plot->axisTitle(axis).text(), QClipboard::Clipboard);
}

void Graph::showAxisTitleMenu(int axis)
{
    selectedAxis = axis;

    QMenu titleMenu(this);
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("cut")), tr("&Cut"), this,
                        SLOT(cutAxisTitle()));
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("copy")), tr("&Copy"), this,
                        SLOT(copyAxisTitle()));
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("delete")), tr("&Delete"), this,
                        SLOT(removeAxisTitle()));
    titleMenu.addSeparator();
    switch (axis) {
    case QwtScaleDraw::BottomScale:
        titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")),
                            tr("&Properties..."), this, SIGNAL(xAxisTitleDblClicked()));
        break;

    case QwtScaleDraw::LeftScale:
        titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")),
                            tr("&Properties..."), this, SIGNAL(yAxisTitleDblClicked()));
        break;

    case QwtScaleDraw::TopScale:
        titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")),
                            tr("&Properties..."), this, SIGNAL(topAxisTitleDblClicked()));
        break;

    case QwtScaleDraw::RightScale:
        titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")),
                            tr("&Properties..."), this, SIGNAL(rightAxisTitleDblClicked()));
        break;
    }

    ModernUiTheme::upgradeMenuIcons(&titleMenu);
    titleMenu.exec(QCursor::pos());
}

void Graph::showAxisContextMenu(int axis)
{
    selectedAxis = axis;

    QMenu menu(this);
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("zoom-auto")), tr("&Rescale to show all"),
                   this, SLOT(setAutoScale()), tr("Ctrl+Shift+R"));
    menu.addSeparator();
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("close")), tr("&Hide axis"), this,
                   SLOT(hideSelectedAxis()));

    QAction *gridsID = menu.addAction(IconLoader::duotoneIcon(QStringLiteral("axis-grid")),
                                      tr("&Show grids"), this, SLOT(showGrids()));
    gridsID->setCheckable(true);
    if (axis == QwtScaleDraw::LeftScale || axis == QwtScaleDraw::RightScale) {
        if (d_plot->grid()->yEnabled())
            gridsID->setChecked(true);
    } else {
        if (d_plot->grid()->xEnabled())
            gridsID->setChecked(true);
    }

    menu.addSeparator();
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("axis-scale")), tr("&Scale..."), this,
                   SLOT(showScaleDialog()));
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")), tr("&Properties..."), this,
                   SLOT(showAxisDialog()));
    ModernUiTheme::upgradeMenuIcons(&menu);
    menu.exec(QCursor::pos());
}

void Graph::showAxisDialog()
{
    emit showAxisDialog(selectedAxis);
}

void Graph::showScaleDialog()
{
    emit axisDblClicked(selectedAxis);
}

void Graph::hideSelectedAxis()
{
    int axis = -1;
    if (selectedAxis == QwtScaleDraw::LeftScale || selectedAxis == QwtScaleDraw::RightScale)
        axis = selectedAxis - 2;
    else
        axis = selectedAxis + 2;

    d_plot->enableAxis(axis, false);
    scalePicker->refresh();
    emit modifiedGraph();
}

void Graph::showGrids()
{
    showGrid(selectedAxis);
}

void Graph::showGrid()
{
    showGrid(QwtScaleDraw::LeftScale);
    showGrid(QwtScaleDraw::BottomScale);
}

void Graph::showGrid(int axis)
{
    Grid *grid = d_plot->grid();
    if (!grid)
        return;

    if (axis == QwtScaleDraw::LeftScale || axis == QwtScaleDraw::RightScale) {
        grid->enableY(!grid->yEnabled());
        grid->enableYMin(!grid->yMinEnabled());
    } else if (axis == QwtScaleDraw::BottomScale || axis == QwtScaleDraw::TopScale) {
        grid->enableX(!grid->xEnabled());
        grid->enableXMin(!grid->xMinEnabled());
    } else
        return;

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::copy(ApplicationWindow *parent, Graph *g)
{
    int i;
    Plot *plot = g->plotWidget();
    d_plot->setMargin(plot->margin());

    setAntialiasing(g->antialiasing());
    autoScaleFonts = g->autoscaleFonts();

    setBackgroundColor(plot->paletteBackgroundColor());
    setFrame(plot->lineWidth(), plot->frameColor());
    setCanvasBackground(plot->canvasBackground());

    enableAxes(g->enabledAxes());
    setAxesColors(g->axesColors());
    setAxesNumColors(g->axesNumColors());
    setAxesBaseline(g->axesBaseline());

    grid()->copy(g->grid());

    d_plot->setTitle(g->plotWidget()->title());

    drawCanvasFrame(g->framed(), g->canvasFrameWidth(), g->canvasFrameColor());

    for (i = 0; i < 4; i++) {
        setAxisTitle(i, g->axisTitle(i));
        setAxisFont(i, g->axisFont(i));
    }

    setXAxisTitleColor(g->axisTitleColor(2));
    setXAxisTitleFont(g->axisTitleFont(2));
    setXAxisTitleAlignment(g->axisTitleAlignment(2));

    setYAxisTitleColor(g->axisTitleColor(0));
    setYAxisTitleFont(g->axisTitleFont(0));
    setYAxisTitleAlignment(g->axisTitleAlignment(0));

    setTopAxisTitleColor(g->axisTitleColor(3));
    setTopAxisTitleFont(g->axisTitleFont(3));
    setTopAxisTitleAlignment(g->axisTitleAlignment(3));

    setRightAxisTitleColor(g->axisTitleColor(1));
    setRightAxisTitleFont(g->axisTitleFont(1));
    setRightAxisTitleAlignment(g->axisTitleAlignment(1));

    setAxesLinewidth(plot->axesLinewidth());
    removeLegend();

    for (i = 0; i < g->curves(); i++) {
        QwtPlotItem *it = (QwtPlotItem *)g->plotItem(i);
        if (it->rtti() == QwtPlotItem::Rtti_PlotCurve) {
            DataCurve *cv = (DataCurve *)it;
            int n = cv->dataSize();
            int style = ((PlotCurve *)it)->type();
            QVector<double> x(n);
            QVector<double> y(n);
            for (int j = 0; j < n; j++) {
                x[j] = cv->x(j);
                y[j] = cv->y(j);
            }

            PlotCurve *c = 0;
            if (style == Pie) {
                c = new QwtPieCurve(cv->table(), cv->title().text(), cv->startRow(), cv->endRow());
                ((QwtPieCurve *)c)->setRay(((QwtPieCurve *)cv)->ray());
                ((QwtPieCurve *)c)->setFirstColor(((QwtPieCurve *)cv)->firstColor());
            } else if (style == Function) {
                c = new FunctionCurve(parent, cv->title().text());
                static_cast<FunctionCurve *>(c)->copy(static_cast<FunctionCurve *>(it));
            } else if (style == VerticalBars || style == HorizontalBars) {
                c = new QwtBarCurve(((QwtBarCurve *)cv)->orientation(), cv->table(),
                                    cv->xColumnName(), cv->title().text(), cv->startRow(),
                                    cv->endRow());
                ((QwtBarCurve *)c)->copy((const QwtBarCurve *)cv);
            } else if (style == ErrorBars) {
                QwtErrorPlotCurve *er = (QwtErrorPlotCurve *)cv;
                DataCurve *master_curve = masterCurve(er);
                if (master_curve) {
                    c = new QwtErrorPlotCurve(cv->table(), cv->title().text());
                    ((QwtErrorPlotCurve *)c)->copy(er);
                    ((QwtErrorPlotCurve *)c)->setMasterCurve(master_curve);
                }
            } else if (style == Histogram) {
                c = new QwtHistogram(cv->table(), cv->title().text(), cv->startRow(), cv->endRow());
                ((QwtHistogram *)c)->copy((const QwtHistogram *)cv);
            } else if (style == VectXYXY || style == VectXYAM) {
                VectorCurve::VectorStyle vs = VectorCurve::XYXY;
                if (style == VectXYAM)
                    vs = VectorCurve::XYAM;
                c = new VectorCurve(vs, cv->table(), cv->xColumnName(), cv->title().text(),
                                    ((VectorCurve *)cv)->vectorEndXAColName(),
                                    ((VectorCurve *)cv)->vectorEndYMColName(), cv->startRow(),
                                    cv->endRow());
                ((VectorCurve *)c)->copy((const VectorCurve *)cv);
            } else if (style == Box) {
                c = new BoxCurve(cv->table(), cv->title().text(), cv->startRow(), cv->endRow());
                ((BoxCurve *)c)->copy((const BoxCurve *)cv);
                QwtSingleArrayData dat(x[0], y, n);
                c->setData(dat);
            } else
                c = new DataCurve(cv->table(), cv->xColumnName(), cv->title().text(),
                                  cv->startRow(), cv->endRow());

            c_keys.resize(++n_curves);
            c_keys[i] = d_plot->insertCurve(c);

            c_type.resize(n_curves);
            c_type[i] = g->curveType(i);

            if (c_type[i] != Box && c_type[i] != ErrorBars)
                c->setData(x.data(), y.data(), n);

            c->setPen(cv->pen());
            c->setBrush(cv->brush());
            c->setStyle(cv->style());
            c->setSymbol(cv->symbol());

            if (cv->testCurveAttribute(QwtPlotCurve::Fitted))
                c->setCurveAttribute(QwtPlotCurve::Fitted, true);
            else if (cv->testCurveAttribute(QwtPlotCurve::Inverted))
                c->setCurveAttribute(QwtPlotCurve::Inverted, true);

            if (c && cv && c_type[i] != Box && c_type[i] != ErrorBars) {
                c->setStackNormalize01(cv->stackNormalize01());
                c->setStackOffset(cv->stackOffsetX(), cv->stackOffsetY());
            }

            c->setAxis(cv->xAxis(), cv->yAxis());
            c->setVisible(cv->isVisible());

            QList<QwtPlotCurve *> lst = g->fitCurvesList();
            if (lst.contains((QwtPlotCurve *)it))
                d_fit_curves << c;
        } else if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = ((Spectrogram *)it)->copy();
            c_keys.resize(++n_curves);
            c_keys[i] = d_plot->insertCurve(sp);

            sp->showColorScale(((Spectrogram *)it)->colorScaleAxis(),
                               ((Spectrogram *)it)->hasColorScale());
            sp->setColorBarWidth(((Spectrogram *)it)->colorBarWidth());
            sp->setVisible(it->isVisible());

            c_type.resize(n_curves);
            c_type[i] = g->curveType(i);
        }
    }

    axesFormulas = g->axesFormulas;
    axisType = g->axisType;
    axesFormatInfo = g->axesFormatInfo;
    axisType = g->axisType;

    for (i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *sc = g->plotWidget()->axisWidget(i);
        if (!sc)
            continue;

        QwtScaleDraw *sd = g->plotWidget()->axisScaleDraw(i);
        if (sd->hasComponent(QwtAbstractScaleDraw::Labels)) {
            if (axisType[i] == AxisType::Numeric)
                setLabelsNumericFormat(i, plot->axisLabelFormat(i), plot->axisLabelPrecision(i),
                                       axesFormulas[i]);
            else if (axisType[i] == AxisType::Day)
                setLabelsDayFormat(i, axesFormatInfo[i].toInt());
            else if (axisType[i] == AxisType::Month)
                setLabelsMonthFormat(i, axesFormatInfo[i].toInt());
            else if (axisType[i] == AxisType::Time || axisType[i] == AxisType::Date
                     || axisType[i] == AxisType::DateTime)
                setLabelsDateTimeFormat(i, axisType[i], axesFormatInfo[i]);
            else {
                QwtTextScaleDraw *sd = (QwtTextScaleDraw *)plot->axisScaleDraw(i);
                d_plot->setAxisScaleDraw(i, new QwtTextScaleDraw(sd->labelsMap()));
            }
        } else {
            sd = d_plot->axisScaleDraw(i);
            sd->enableComponent(QwtAbstractScaleDraw::Labels, false);
        }
    }
    for (i = 0; i < QwtPlot::axisCnt; i++) { // set same scales
        const QwtScaleEngine *se = plot->axisScaleEngine(i);
        if (!se)
            continue;

        QwtScaleEngine *sc_engine = 0;
        if (se->transformation()->type() == QwtScaleTransformation::Log10)
            sc_engine = new QwtLog10ScaleEngine();
        else
            sc_engine = new QwtLinearScaleEngine();

        int majorTicks = plot->axisMaxMajor(i);
        int minorTicks = plot->axisMaxMinor(i);
        d_plot->setAxisMaxMajor(i, majorTicks);
        d_plot->setAxisMaxMinor(i, minorTicks);

        const QwtScaleDiv *sd = plot->axisScaleDiv(i);
        QwtValueList lst = sd->ticks(QwtScaleDiv::MajorTick);

        d_user_step[i] = g->axisStep(i);

#if QWT_VERSION >= 0x050200
        QwtScaleDiv div = sc_engine->divideScale(qMin(sd->lowerBound(), sd->upperBound()),
                                                 qMax(sd->lowerBound(), sd->upperBound()),
                                                 majorTicks, minorTicks, d_user_step[i]);
#else
        QwtScaleDiv div = sc_engine->divideScale(qMin(sd->lBound(), sd->hBound()),
                                                 qMax(sd->lBound(), sd->hBound()), majorTicks,
                                                 minorTicks, d_user_step[i]);
#endif

        if (se->testAttribute(QwtScaleEngine::Inverted)) {
            sc_engine->setAttribute(QwtScaleEngine::Inverted);
            div.invert();
        }

        d_plot->setAxisScaleEngine(i, sc_engine);
        d_plot->setAxisScaleDiv(i, div);
    }

    drawAxesBackbones(g->drawAxesBackbone);
    setMajorTicksType(g->plotWidget()->getMajorTicksType());
    setMinorTicksType(g->plotWidget()->getMinorTicksType());
    setTicksLength(g->minorTickLength(), g->majorTickLength());

    setAxisLabelRotation(QwtPlot::xBottom, g->labelsRotation(QwtPlot::xBottom));
    setAxisLabelRotation(QwtPlot::xTop, g->labelsRotation(QwtPlot::xTop));

    QVector<int> imag = g->imageMarkerKeys();
    for (i = 0; i < (int)imag.size(); i++)
        addImage((ImageMarker *)g->imageMarker(imag[i]));

    QVector<int> txtMrkKeys = g->textMarkerKeys();
    for (i = 0; i < (int)txtMrkKeys.size(); i++) {
        Legend *mrk = (Legend *)g->textMarker(txtMrkKeys[i]);
        if (!mrk)
            continue;

        if (txtMrkKeys[i] == g->legendMarkerID)
            legendMarkerID = insertTextMarker(mrk);
        else
            insertTextMarker(mrk);
    }

    QVector<int> l = g->lineMarkerKeys();
    for (i = 0; i < (int)l.size(); i++) {
        ArrowMarker *lmrk = (ArrowMarker *)g->arrow(l[i]);
        if (lmrk)
            addArrow(lmrk);
    }

    for (i = 0; i < QwtPlot::axisCnt; ++i) {
        if (g->drawAxesBackbone)
            setAxisBackboneVisible(i, g->isAxisBackboneVisible(i));
        setTickLabelTransform(i, g->tickDivideFactor(i), g->tickPrefix(i), g->tickSuffix(i));
        if (g->axisBreak(i).enabled)
            setAxisBreak(i, g->axisBreak(i));
    }

    if (!g->referenceLines().isEmpty())
        setReferenceLines(g->referenceLines());

    for (const ShadedBandSpec &bs : g->d_shaded_bands) {
        addShadedBand(bs.x, bs.yLow, bs.yHigh, bs.fillColor, bs.title);
    }

    d_is_ternary = g->d_is_ternary;
    for (int k = 0; k < 3; ++k)
        d_ternary_labels[k] = g->d_ternary_labels[k];

    d_plot->replot();

    if (isPiePlot()) {
        QwtPieCurve *c = (QwtPieCurve *)curve(0);
        c->updateBoundingRect();
    }
}

void Graph::plotBoxDiagram(Table *w, const QStringList &names, int startRow, int endRow)
{
    if (endRow < 0)
        endRow = w->numRows() - 1;

    for (int j = 0; j < (int)names.count(); j++) {
        BoxCurve *c = new BoxCurve(w, names[j], startRow, endRow);
        c->setData(QwtSingleArrayData(double(j + 1), QwtArray<double>(), 0));
        c->loadData();

        c_keys.resize(++n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(c);
        c_type.resize(n_curves);
        c_type[n_curves - 1] = Box;

        c->setPen(QPen(ColorButton::color(j), 1));
        c->setSymbol(QwtSymbol(QwtSymbol::NoSymbol, QBrush(), QPen(ColorButton::color(j), 1),
                               QSize(7, 7)));
    }

    Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
    if (mrk)
        mrk->setText(legendText());

    axisType[QwtPlot::xBottom] = AxisType::ColHeader;
    d_plot->setAxisScaleDraw(QwtPlot::xBottom, new QwtTextScaleDraw(w->selectedYLabels()));
    d_plot->setAxisMaxMajor(QwtPlot::xBottom, names.count() + 1);
    d_plot->setAxisMaxMinor(QwtPlot::xBottom, 0);

    axisType[QwtPlot::xTop] = AxisType::ColHeader;
    d_plot->setAxisScaleDraw(QwtPlot::xTop, new QwtTextScaleDraw(w->selectedYLabels()));
    d_plot->setAxisMaxMajor(QwtPlot::xTop, names.count() + 1);
    d_plot->setAxisMaxMinor(QwtPlot::xTop, 0);

    axesFormatInfo[QwtPlot::xBottom] = w->name();
    axesFormatInfo[QwtPlot::xTop] = w->name();
}

void Graph::setCurveStyle(int index, int s)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    if (s == 5) // ancient spline style in Qwt 4.2.0
    {
        s = QwtPlotCurve::Lines;
        c->setCurveAttribute(QwtPlotCurve::Fitted, true);
        c_type[index] = Spline;
    } else if (s == QwtPlotCurve::Lines)
        c->setCurveAttribute(QwtPlotCurve::Fitted, false);
    else if (s == 6) // Vertical Steps
    {
        s = QwtPlotCurve::Steps;
        c->setCurveAttribute(QwtPlotCurve::Inverted, true);
        c_type[index] = VerticalSteps;
    } else if (s == QwtPlotCurve::Steps) // Horizontal Steps
    {
        c->setCurveAttribute(QwtPlotCurve::Inverted, false);
        c_type[index] = HorizontalSteps;
    }
    c->setStyle((QwtPlotCurve::CurveStyle)s);
}

void Graph::setCurveSymbol(int index, const QwtSymbol &s)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    c->setSymbol(s);
}

void Graph::setCurvePen(int index, const QPen &p)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    c->setPen(p);
}

void Graph::setCurveBrush(int index, const QBrush &b)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    c->setBrush(b);
}

void Graph::openBoxDiagram(Table *w, const QStringList &l, int fileVersion)
{
    if (!w)
        return;

    int s_offset = 0;
    int startRow = 0;
    int endRow = w->numRows() - 1;
    if (fileVersion >= 90) {
        startRow = l[l.count() - 3].toInt();
        endRow = l[l.count() - 2].toInt();
    }

    BoxCurve *c = new BoxCurve(w, l[2], startRow, endRow);
    c->setData(QwtSingleArrayData(l[1].toDouble(), QwtArray<double>(), 0));
    c->setData(QwtSingleArrayData(l[1].toDouble(), QwtArray<double>(), 0));
    c->loadData();

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(c);
    c_type.resize(n_curves);
    c_type[n_curves - 1] = Box;

    if (fileVersion >= 0x011800) // 1.24.0
    {
        s_offset = 3;
    }
    c->setMaxStyle(SymbolBox::style(l[16 + s_offset].toInt()));
    c->setP99Style(SymbolBox::style(l[17 + s_offset].toInt()));
    c->setMeanStyle(SymbolBox::style(l[18 + s_offset].toInt()));
    c->setP1Style(SymbolBox::style(l[19 + s_offset].toInt()));
    c->setMinStyle(SymbolBox::style(l[20 + s_offset].toInt()));

    c->setBoxStyle(l[21 + s_offset].toInt());
    c->setBoxWidth(l[22 + s_offset].toInt());
    c->setBoxRange(l[23 + s_offset].toInt(), l[24 + s_offset].toDouble());
    c->setWhiskersRange(l[25 + s_offset].toInt(), l[26 + s_offset].toDouble());
    if (l.count() > 33 + s_offset) {
        c->setViolinMode(l[27 + s_offset].toInt() != 0);
        c->setShowDataPoints(l[28 + s_offset].toInt() != 0);
    }
}

void Graph::setActiveTool(PlotToolInterface *tool)
{
    if (d_active_tool)
        delete d_active_tool;
    d_active_tool = tool;
    if (d_range_selector) {
        if (tool)
            // we want to use tool now, so disable range selection
            d_range_selector->setEnabled(false);
        else
            // re-enable range selection
            d_range_selector->setEnabled(true);
    }
}

void Graph::disableTools()
{
    if (zoomOn())
        zoom(false);
    if (drawLineActive())
        drawLine(false);
    setActiveTool(NULL);
    if (d_range_selector)
        delete d_range_selector;
}

bool Graph::enableRangeSelectors(const QObject *status_target, const char *status_slot)
{
    // disable other tools, otherwise it's undefined what tool should handle mouse clicks
    disableTools();
    d_range_selector = new RangeSelectorTool(this, status_target, status_slot);
    connect(d_range_selector, SIGNAL(changed()), this, SIGNAL(dataRangeChanged()));
    return true;
}

void Graph::setTextMarkerDefaults(int f, const QFont &font, const QColor &textCol,
                                  const QColor &backgroundCol)
{
    QFont resolved = font;
    if (resolved.family().trimmed().isEmpty())
        resolved = QFont("Arial", 12, QFont::Normal, false);
    defaultMarkerFont = resolved;
    defaultMarkerFrame = f;
    defaultTextMarkerColor = textCol;
    defaultTextMarkerBackground = backgroundCol;
}

void Graph::setArrowDefaults(int lineWidth, const QColor &c, Qt::PenStyle style, int headLength,
                             int headAngle, bool fillHead)
{
    defaultArrowLineWidth = lineWidth;
    defaultArrowColor = c;
    defaultArrowLineStyle = style;
    defaultArrowHeadLength = headLength;
    defaultArrowHeadAngle = headAngle;
    defaultArrowHeadFill = fillHead;
}

QString Graph::parentPlotName()
{
    QWidget *w = (QWidget *)parent()->parent()->parent();
    return QString(w->objectName());
}

void Graph::guessUniqueCurveLayout(int &colorIndex, int &symbolIndex)
{
    static const char *const kPaletteHex[] = {
        "#1f77b4", // blue
        "#ff7f0e", // orange
        "#2ca02c", // green
        "#d62728", // red
        "#9467bd", // purple
        "#8c564b", // brown
        "#e377c2", // pink
        "#7f7f7f", // gray
        "#bcbd22", // olive
        "#17becf"  // cyan
    };
    static const int kSymbolIndices[] = {
        1,  // Ellipse (circle)
        2,  // Rect (square)
        6,  // UTriangle (up-triangle)
        5,  // DTriangle (down-triangle)
        3,  // Diamond
        7,  // LTriangle
        8,  // RTriangle
        15, // Hexagon
        13, // Star1
        9   // Cross
    };
    const int kPaletteCount = int(sizeof(kPaletteHex) / sizeof(kPaletteHex[0]));

    int curve_index = n_curves - 1;
    if (curve_index >= 0
        && c_type[curve_index] == ErrorBars) { // find out the pen color of the master curve
        QwtErrorPlotCurve *er = (QwtErrorPlotCurve *)d_plot->curve(c_keys[curve_index]);
        DataCurve *master_curve = er->masterCurve();
        if (master_curve) {
            colorIndex = ColorButton::colorIndex(master_curve->pen().color());
            symbolIndex = 1;
            return;
        }
    }

    int dataCurves = 0;
    for (int i = 0; i < n_curves; i++) {
        const QwtPlotCurve *c = curve(i);
        if (c && c->rtti() == QwtPlotItem::Rtti_PlotCurve && c_type[i] != ErrorBars)
            dataCurves++;
    }
    int idx = qMax(0, dataCurves - 1) % kPaletteCount;
    colorIndex = int(QColor(kPaletteHex[idx]).rgba());
    symbolIndex = kSymbolIndices[idx];
}

void Graph::addFitCurve(QwtPlotCurve *c)
{
    if (c)
        d_fit_curves << c;
}

void Graph::deleteFitCurves()
{
    QList<int> keys = d_plot->curveKeys();
    foreach (QwtPlotCurve *c, d_fit_curves)
        removeCurve(curveIndex(c));

    d_plot->replot();
}

void Graph::plotSpectrogram(Matrix *m, CurveType type)
{
    if (type != GrayMap && type != ColorMap && type != ContourMap)
        return;

    Spectrogram *d_spectrogram = new Spectrogram(m);
    if (type == GrayMap)
        d_spectrogram->setGrayScale();
    else if (type == ContourMap) {
        d_spectrogram->setDisplayMode(QwtPlotSpectrogram::ImageMode, false);
        d_spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, true);
    } else if (type == ColorMap) {
        d_spectrogram->setDefaultColorMap();
        d_spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, true);
    }

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(d_spectrogram);

    c_type.resize(n_curves);
    c_type[n_curves - 1] = type;

    QwtScaleWidget *rightAxis = d_plot->axisWidget(QwtPlot::yRight);
    rightAxis->setColorBarEnabled(type != ContourMap);
    rightAxis->setColorMap(d_spectrogram->effectiveValueRange(), d_spectrogram->colorMap());

    d_plot->setAxisScale(QwtPlot::xBottom, m->xStart(), m->xEnd());
    d_plot->setAxisScale(QwtPlot::yLeft, m->yStart(), m->yEnd());

    d_plot->setAxisScale(QwtPlot::yRight, d_spectrogram->effectiveValueRange().minValue(),
                         d_spectrogram->effectiveValueRange().maxValue());
    d_plot->enableAxis(QwtPlot::yRight, type != ContourMap);

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        QFont tf = d_plot->axisFont(a);
        tf.setFamily(QStringLiteral("Arial"));
        d_plot->setAxisFont(a, tf);
        QwtText tt = d_plot->axisTitle(a);
        QFont ttf = tt.font();
        ttf.setFamily(QStringLiteral("Arial"));
        tt.setFont(ttf);
        d_plot->setAxisTitle(a, tt);
    }
    d_spectrogram->enforceColorBarArialFont();

    d_plot->replot();
}

void Graph::restoreSpectrogram(ApplicationWindow *app, const QStringList &lst)
{
    QStringList::const_iterator line = lst.begin();
    QString s = (*line).trimmed();
    QString matrixName = s.remove("<matrix>").remove("</matrix>");
    Matrix *m = app->matrix(matrixName);
    if (!m)
        return;

    Spectrogram *sp = new Spectrogram(m);

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = Graph::ColorMap;
    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(sp);

    bool hasPresetTag = false;
    Spectrogram::ColorMapPreset loadedPreset = Spectrogram::PresetLegacyDefault;
    bool revPalette = false;
    bool hasCustomZ = false;
    double zMin = 0.0, zMax = 1.0;
    int pendingLevels = -1;
    bool hasColorBar = false;
    int colorBarAxis = QwtPlot::yRight;
    int colorBarW = 16;
    QString cbTitle;

    for (line++; line != lst.end(); line++) {
        QString s = *line;
        if (s.contains("<ColorPolicy>")) {
            int color_policy = s.remove("<ColorPolicy>").remove("</ColorPolicy>").trimmed().toInt();
            if (color_policy == Spectrogram::GrayScale)
                sp->setGrayScale();
            else if (color_policy == Spectrogram::Default)
                sp->setDefaultColorMap();
        } else if (s.contains("<ColorMap>")) {
            s = *(++line);
            int mode = s.remove("<Mode>").remove("</Mode>").trimmed().toInt();
            s = *(++line);
            QColor color1 =
                    QColor(COLORVALUE(s.remove("<MinColor>").remove("</MinColor>").trimmed()));
            s = *(++line);
            QColor color2 =
                    QColor(COLORVALUE(s.remove("<MaxColor>").remove("</MaxColor>").trimmed()));

            QwtLinearColorMap colorMap = QwtLinearColorMap(color1, color2);
            colorMap.setMode((QwtLinearColorMap::Mode)mode);

            s = *(++line);
            int stops = s.remove("<ColorStops>").remove("</ColorStops>").trimmed().toInt();
            for (int i = 0; i < stops; i++) {
                s = (*(++line)).trimmed();
                QStringList l = s.remove("<Stop>").remove("</Stop>").split("\t");
                colorMap.addColorStop(l[0].toDouble(), QColor(COLORVALUE(l[1])));
            }
            sp->setCustomColorMap(colorMap);
            line++;
        } else if (s.contains("<Image>")) {
            int mode = s.remove("<Image>").remove("</Image>").trimmed().toInt();
            sp->setDisplayMode(QwtPlotSpectrogram::ImageMode, mode);
        } else if (s.contains("<ContourLines>")) {
            int contours = s.remove("<ContourLines>").remove("</ContourLines>").trimmed().toInt();
            sp->setDisplayMode(QwtPlotSpectrogram::ContourMode, contours);
            if (contours) {
                s = (*(++line)).trimmed();
                int levels = s.remove("<Levels>").remove("</Levels>").toInt();
                pendingLevels = levels;
                sp->setLevelsNumber(levels);

                s = (*(++line)).trimmed();
                int defaultPen = s.remove("<DefaultPen>").remove("</DefaultPen>").toInt();
                if (!defaultPen)
                    sp->setDefaultContourPen(Qt::NoPen);
                else {
                    s = (*(++line)).trimmed();
                    QColor c = QColor(COLORVALUE(s.remove("<PenColor>").remove("</PenColor>")));
                    s = (*(++line)).trimmed();
                    int width = s.remove("<PenWidth>").remove("</PenWidth>").toInt();
                    s = (*(++line)).trimmed();
                    int style = s.remove("<PenStyle>").remove("</PenStyle>").toInt();
                    sp->setDefaultContourPen(QPen(c, width, Graph::getPenStyle(style)));
                }
            }
        } else if (s.contains("<ColorBar>")) {
            s = *(++line);
            colorBarAxis = s.remove("<axis>").remove("</axis>").trimmed().toInt();
            s = *(++line);
            colorBarW = s.remove("<width>").remove("</width>").trimmed().toInt();
            hasColorBar = true;
            line++;
        } else if (s.contains("<ColorPreset>")) {
            const int p = s.remove("<ColorPreset>").remove("</ColorPreset>").trimmed().toInt();
            if (p >= static_cast<int>(Spectrogram::PresetLegacyDefault)
                && p <= static_cast<int>(Spectrogram::PresetCustom)) {
                loadedPreset = static_cast<Spectrogram::ColorMapPreset>(p);
                hasPresetTag = true;
            }
        } else if (s.contains("<ReversePalette>")) {
            revPalette =
                    s.remove("<ReversePalette>").remove("</ReversePalette>").trimmed().toInt() != 0;
        } else if (s.contains("<CustomZRange>")) {
            const QStringList parts =
                    s.remove("<CustomZRange>").remove("</CustomZRange>").trimmed().split("\t");
            if (parts.size() >= 3 && parts[0].toInt() != 0) {
                hasCustomZ = true;
                zMin = parts[1].toDouble();
                zMax = parts[2].toDouble();
            }
        } else if (s.contains("<ColorBarTitle>")) {
            QString raw = s.trimmed();
            if (raw.startsWith("<ColorBarTitle>"))
                raw = raw.mid(15);
            if (raw.endsWith("</ColorBarTitle>"))
                raw.chop(16);
            raw.replace("&#10;", "\n")
                    .replace("&gt;", ">")
                    .replace("&lt;", "<")
                    .replace("&amp;", "&");
            cbTitle = raw;
        } else if (s.contains("<Visible>")) {
            int on = s.remove("<Visible>").remove("</Visible>").trimmed().toInt();
            sp->setVisible(on);
        }
    }

    if (hasCustomZ) {
        sp->setCustomValueRange(true, zMin, zMax);
        if (pendingLevels >= 2)
            sp->setLevelsNumber(pendingLevels);
    }
    if (hasPresetTag)
        sp->setColorMapPreset(loadedPreset, revPalette);
    else if (revPalette)
        sp->setReversePalette(true);

    if (hasColorBar) {
        sp->showColorScale(colorBarAxis, true);
        sp->setColorBarWidth(colorBarW);
    } else {
        if (QwtScaleWidget *ca = d_plot->axisWidget(sp->colorScaleAxis()))
            ca->setColorBarEnabled(false);
    }
    if (!cbTitle.isEmpty())
        sp->setColorBarTitle(cbTitle);
    sp->syncColorScale();
    sp->enforceColorBarArialFont();
}

bool Graph::validCurvesDataSize() const
{
    if (!n_curves) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(const_cast<Graph *>(this), tr("Warning"),
                                 tr("There are no curves available on this plot!"));
        }
        return false;
    } else {
        for (int i = 0; i < n_curves; i++) {
            QwtPlotItem *item = curve(i);
            if (item && item->rtti() != QwtPlotItem::Rtti_PlotSpectrogram) {
                QwtPlotCurve *c = (QwtPlotCurve *)item;
                if (c->dataSize() >= 2)
                    return true;
            }
        }
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(const_cast<Graph *>(this), tr("Error"),
                                 tr("There are no curves with more than two points on this plot. "
                                    "Operation aborted!"));
        }
        return false;
    }
}

Graph::~Graph()
{
    setActiveTool(NULL);
    if (d_range_selector)
        delete d_range_selector;
    delete titlePicker;
    delete scalePicker;
    delete cp;
    delete d_plot;
}

void Graph::setAntialiasing(bool on, bool update)
{
    if (d_antialiasing == on)
        return;

    d_antialiasing = on;

    QList<int> curve_keys = d_plot->curveKeys();
    for (int i = 0; i < (int)curve_keys.count(); i++) {
        QwtPlotItem *c = d_plot->curve(curve_keys[i]);
        if (c)
            c->setRenderHint(QwtPlotItem::RenderAntialiased, d_antialiasing);
    }
    QList<int> marker_keys = d_plot->markerKeys();
    for (int i = 0; i < (int)marker_keys.count(); i++) {
        QwtPlotMarker *m = d_plot->marker(marker_keys[i]);
        if (m)
            m->setRenderHint(QwtPlotItem::RenderAntialiased, d_antialiasing);
    }

    if (update)
        d_plot->replot();
}

bool Graph::focusNextPrevChild(bool)
{
    QList<int> mrkKeys = d_plot->markerKeys();
    int n = mrkKeys.size();
    if (n < 2)
        return false;

    int min_key = mrkKeys[0], max_key = mrkKeys[0];
    for (int i = 0; i < n; i++) {
        if (mrkKeys[i] >= max_key)
            max_key = mrkKeys[i];
        if (mrkKeys[i] <= min_key)
            min_key = mrkKeys[i];
    }

    int key = selectedMarker;
    if (key >= 0) {
        key++;
        if (key > max_key)
            key = min_key;
    } else
        key = min_key;

    cp->disableEditing();

    setSelectedMarker(key);
    return true;
}

QString Graph::axisFormatInfo(int axis)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    else
        return axesFormatInfo[axis];
}

void Graph::updateCurveNames(const QString &oldName, const QString &newName, bool updateTableName)
{
    if (oldName.isEmpty() || oldName == newName)
        return;

    struct ChangedCurveInfo
    {
        int curveIdx = -1;
        QString prevTitle;
        QString nextTitle;
        QString oldShort;
        QString newShort;
        QString colLong;
    };
    QHash<int, ChangedCurveInfo> changedByOneBased;
    QList<ChangedCurveInfo> changedList;

    // update plotted curves list
    const QList<int> keys = d_plot->curveKeys();
    for (int i = 0; i < keys.count(); i++) {
        QwtPlotItem *it = d_plot->plotItem(keys[i]);
        if (!it || it->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;

        DataCurve *c = dynamic_cast<DataCurve *>(it);
        if (!c)
            continue;

        const QString prevTitle = c->title().text();
        c->updateColumnNames(oldName, newName, updateTableName);
        const QString nextTitle = c->title().text();
        if (prevTitle != nextTitle) {
            ChangedCurveInfo info;
            info.curveIdx = curveIndex(c);
            info.prevTitle = prevTitle;
            info.nextTitle = nextTitle;
            if (Table *tbl = c->table()) {
                if (!updateTableName) {
                    const QString tblPrefix = QString(tbl->name()) + QLatin1Char('_');
                    if (prevTitle.startsWith(tblPrefix))
                        info.oldShort = prevTitle.mid(tblPrefix.size());
                    if (nextTitle.startsWith(tblPrefix))
                        info.newShort = nextTitle.mid(tblPrefix.size());
                }
                const int yIdx = tbl->colIndex(nextTitle);
                if (Column *yc = (yIdx >= 0 ? tbl->column(yIdx) : nullptr))
                    info.colLong = yc->longName().trimmed();
            }
            changedList.append(info);
            if (info.curveIdx >= 0)
                changedByOneBased.insert(info.curveIdx + 1, info);
        }
    }

    if (Legend *mrk = legend()) {
        auto isIdentChar = [](QChar ch) -> bool {
            return ch.isLetterOrNumber() || ch == QLatin1Char('_');
        };
        auto replaceExactToken = [&](const QString &src, const QString &fromTok,
                                     const QString &toTok) -> QString {
            if (fromTok.isEmpty() || fromTok == toTok)
                return src;
            QString out;
            out.reserve(src.size());
            int pos = 0;
            while (pos < src.size()) {
                const int hit = src.indexOf(fromTok, pos);
                if (hit < 0) {
                    out.append(src.mid(pos));
                    break;
                }
                const bool leftBoundary = (hit == 0) || !isIdentChar(src.at(hit - 1));
                const int after = hit + fromTok.size();
                const bool rightBoundary = (after >= src.size()) || !isIdentChar(src.at(after));
                if (leftBoundary && rightBoundary) {
                    out.append(src.mid(pos, hit - pos));
                    out.append(toTok);
                    pos = after;
                } else {
                    out.append(src.mid(pos, (hit - pos) + 1));
                    pos = hit + 1;
                }
            }
            return out;
        };

        const QString origText = mrk->text();
        const bool trailingNewline = origText.endsWith(QLatin1Char('\n'));
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList lst = origText.split("\n", Qt::SkipEmptyParts);
#else
        QStringList lst = origText.split("\n", QString::SkipEmptyParts);
#endif
        bool anyLegendChanged = false;
        for (int li = 0; li < lst.size(); ++li) {
            const QString line = lst[li];
            int prefixEnd = -1;
            int oneBased = -1;

            const int cPos = line.indexOf(QStringLiteral("\\c{"));
            if (cPos >= 0 && line.left(cPos).trimmed().isEmpty()) {
                const int closeBrace = line.indexOf(QLatin1Char('}'), cPos + 3);
                if (closeBrace > cPos + 3) {
                    bool ok = false;
                    const int parsed = line.mid(cPos + 3, closeBrace - (cPos + 3)).toInt(&ok);
                    if (ok && parsed >= 1) {
                        oneBased = parsed;
                        prefixEnd = closeBrace + 1;
                    }
                }
            } else {
                const int lPos = line.indexOf(QStringLiteral("\\l("));
                if (lPos >= 0 && line.left(lPos).trimmed().isEmpty()) {
                    const int closeParen = line.indexOf(QLatin1Char(')'), lPos + 3);
                    if (closeParen > lPos + 3) {
                        bool ok = false;
                        const int parsed = line.mid(lPos + 3, closeParen - (lPos + 3)).toInt(&ok);
                        if (ok && parsed >= 1) {
                            oneBased = parsed;
                            prefixEnd = closeParen + 1;
                        }
                    }
                }
            }

            if (oneBased >= 1 && prefixEnd > 0) {
                if (!changedByOneBased.contains(oneBased))
                    continue;

                const ChangedCurveInfo &info = changedByOneBased.value(oneBased);
                const QString cmdPrefix = line.left(prefixEnd);
                const QString rest = line.mid(prefixEnd);
                QString newRest = rest;

                if (rest == info.prevTitle) {
                    newRest = info.nextTitle;
                } else if (!updateTableName && !info.oldShort.isEmpty() && rest == info.oldShort) {
                    newRest = !info.colLong.isEmpty() ? info.colLong : info.newShort;
                    if (info.curveIdx >= 0)
                        d_last_auto_curve_labels[info.curveIdx] = newRest;
                } else if (!updateTableName && !info.oldShort.isEmpty()
                           && rest.trimmed() == info.oldShort) {
                    const int idx = rest.indexOf(info.oldShort);
                    const QString target = !info.colLong.isEmpty() ? info.colLong : info.newShort;
                    newRest = rest.left(idx) + target + rest.mid(idx + info.oldShort.size());
                } else {
                    newRest = replaceExactToken(rest, info.prevTitle, info.nextTitle);
                }

                if (newRest != rest) {
                    lst[li] = cmdPrefix + newRest;
                    anyLegendChanged = true;
                }
            } else if (!changedList.isEmpty()) {
                QString updated = line;
                for (const ChangedCurveInfo &info : changedList)
                    updated = replaceExactToken(updated, info.prevTitle, info.nextTitle);
                if (updated != line) {
                    lst[li] = updated;
                    anyLegendChanged = true;
                }
            }
        }

        if (anyLegendChanged) {
            QString joined = lst.join("\n");
            if (trailingNewline && !joined.isEmpty())
                joined += QLatin1Char('\n');
            mrk->setText(joined);
        }
        if (anyLegendChanged || !changedList.isEmpty())
            d_plot->replot();
    } else if (!changedList.isEmpty()) {
        d_plot->replot();
    }
}

void Graph::setCurveFullRange(int curveIndex)
{
    DataCurve *c = (DataCurve *)curve(curveIndex);
    if (c) {
        c->setFullRange();
        updatePlot();
        emit modifiedGraph();
    }
}

DataCurve *Graph::masterCurve(QwtErrorPlotCurve *er)
{
    QList<int> keys = d_plot->curveKeys();
    for (int i = 0; i < (int)keys.count(); i++) {
        QwtPlotItem *it = d_plot->plotItem(keys[i]);
        if (!it)
            continue;
        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        if (((PlotCurve *)it)->type() == Function)
            continue;

        if (((DataCurve *)it)->plotAssociation() == er->masterCurve()->plotAssociation())
            return (DataCurve *)it;
    }
    return 0;
}

DataCurve *Graph::masterCurve(const QString &xColName, const QString &yColName)
{
    QString master_curve = xColName + "(X)," + yColName + "(Y)";
    QList<int> keys = d_plot->curveKeys();
    for (int i = 0; i < (int)keys.count(); i++) {
        QwtPlotItem *it = d_plot->plotItem(keys[i]);
        if (!it)
            continue;
        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        if (((PlotCurve *)it)->type() == Function)
            continue;

        if (((DataCurve *)it)->plotAssociation() == master_curve)
            return (DataCurve *)it;
    }
    return 0;
}

void Graph::showCurve(int index, bool visible)
{
    QwtPlotItem *it = plotItem(index);
    if (it)
        it->setVisible(visible);

    emit modifiedGraph();
}

int Graph::visibleCurves()
{
    int c = 0;
    QList<int> keys = d_plot->curveKeys();
    for (int i = 0; i < (int)keys.count(); i++) {
        QwtPlotItem *it = d_plot->plotItem(keys[i]);
        if (it && it->isVisible())
            c++;
    }
    return c;
}

QPrinter::PageSize Graph::minPageSize(const QPrinter &printer, const QRect &r)
{
    double x_margin = 0.2 / 2.54 * printer.logicalDpiX(); // 2 mm margins
    double y_margin = 0.2 / 2.54 * printer.logicalDpiY();
    double w_mm = 2 * x_margin + (double)(r.width()) / (double)printer.logicalDpiX() * 25.4;
    double h_mm = 2 * y_margin + (double)(r.height()) / (double)printer.logicalDpiY() * 25.4;

    int w, h;
    if (w_mm / h_mm > 1) {
        w = (int)ceil(w_mm);
        h = (int)ceil(h_mm);
    } else {
        h = (int)ceil(w_mm);
        w = (int)ceil(h_mm);
    }

    QPrinter::PageSize size = QPrinter::A5;
    if (w < 45 && h < 32)
        size = QPrinter::B10;
    else if (w < 52 && h < 37)
        size = QPrinter::A9;
    else if (w < 64 && h < 45)
        size = QPrinter::B9;
    else if (w < 74 && h < 52)
        size = QPrinter::A8;
    else if (w < 91 && h < 64)
        size = QPrinter::B8;
    else if (w < 105 && h < 74)
        size = QPrinter::A7;
    else if (w < 128 && h < 91)
        size = QPrinter::B7;
    else if (w < 148 && h < 105)
        size = QPrinter::A6;
    else if (w < 182 && h < 128)
        size = QPrinter::B6;
    else if (w < 210 && h < 148)
        size = QPrinter::A5;
    else if (w < 220 && h < 110)
        size = QPrinter::DLE;
    else if (w < 229 && h < 163)
        size = QPrinter::C5E;
    else if (w < 241 && h < 105)
        size = QPrinter::Comm10E;
    else if (w < 257 && h < 182)
        size = QPrinter::B5;
    else if (w < 279 && h < 216)
        size = QPrinter::Letter;
    else if (w < 297 && h < 210)
        size = QPrinter::A4;
    else if (w < 330 && h < 210)
        size = QPrinter::Folio;
    else if (w < 356 && h < 216)
        size = QPrinter::Legal;
    else if (w < 364 && h < 257)
        size = QPrinter::B4;
    else if (w < 420 && h < 297)
        size = QPrinter::A3;
    else if (w < 515 && h < 364)
        size = QPrinter::B3;
    else if (w < 594 && h < 420)
        size = QPrinter::A2;
    else if (w < 728 && h < 515)
        size = QPrinter::B2;
    else if (w < 841 && h < 594)
        size = QPrinter::A1;
    else if (w < 1030 && h < 728)
        size = QPrinter::B1;
    else if (w < 1189 && h < 841)
        size = QPrinter::A0;
    else if (w < 1456 && h < 1030)
        size = QPrinter::B0;

    return size;
}

int Graph::mapToQwtAxis(int axis)
{
    int a = -1;
    switch (axis) {
    case 0:
        a = QwtPlot::xBottom;
        break;
    case 1:
        a = QwtPlot::yLeft;
        break;
    case 2:
        a = QwtPlot::xTop;
        break;
    case 3:
        a = QwtPlot::yRight;
        break;
    }
    return a;
}

void Graph::print(QPainter *painter, const QRect &plotRect, const QwtPlotPrintFilter &pfilter)
{
    const bool prevExporting = d_is_exporting;
    d_is_exporting = true;
    d_plot->print(painter, plotRect, pfilter);
    d_is_exporting = prevExporting;
}

void Graph::deselect()
{
    deselectMarker();
    scalePicker->deselect();
    titlePicker->setSelected(false);
}

// ============================================================================
// OriginPro Color Palette System & Interactive Palette Editor
// ============================================================================

namespace {
static QMap<QString, QList<QColor>> &customPaletteMemoryStore()
{
    static QMap<QString, QList<QColor>> store;
    return store;
}

static QList<QColor> hexListToColors(const QStringList &hexList)
{
    QList<QColor> out;
    out.reserve(hexList.size());
    for (const QString &h : hexList) {
        QColor c(h.trimmed());
        if (c.isValid())
            out.append(c);
    }
    return out;
}
} // namespace

QStringList Graph::builtInPaletteNames()
{
    return QStringList()
            << "ISIJ / Origin Default"
            << "Origin Classic"
            << "Viridis"
            << "Plasma"
            << "Inferno"
            << "CoolWarm"
            << "Nature (NPG)"
            << "Science (AAAS)"
            << "Okabe-Ito (Colorblind Safe)"
            << "Monochrome / Grayscale";
}

QStringList Graph::customPaletteNames()
{
    QStringList names = customPaletteMemoryStore().keys();
    QSettings settings;
    settings.beginGroup("CustomColorPalettes");
    for (const QString &k : settings.childKeys()) {
        if (!names.contains(k))
            names.append(k);
    }
    settings.endGroup();
    names.sort();
    return names;
}

QStringList Graph::allPaletteNames()
{
    QStringList names = builtInPaletteNames();
    for (const QString &custom : customPaletteNames()) {
        if (!names.contains(custom))
            names.append(custom);
    }
    return names;
}

QList<QColor> Graph::paletteColors(const QString &paletteName)
{
    const QString key = paletteName.trimmed();
    if (key.compare("ISIJ / Origin Default", Qt::CaseInsensitive) == 0
        || key.compare("Origin Default", Qt::CaseInsensitive) == 0
        || key.compare("ISIJ", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd",
                                 "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf" });
    }
    if (key.compare("Origin Classic", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#000000", "#ff0000", "#0000ff", "#008080", "#ff00ff",
                                 "#808000", "#000080", "#800000", "#ff8000", "#800080" });
    }
    if (key.compare("Viridis", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#440154", "#482878", "#3e4989", "#31688e", "#26828e",
                                 "#1f9e89", "#35b779", "#6ece58", "#b5de2b", "#fde725" });
    }
    if (key.compare("Plasma", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#0d0887", "#46039f", "#7201a8", "#9c179e", "#bd3786",
                                 "#d8576b", "#ed7953", "#fb9f3a", "#fdca26", "#f0f921" });
    }
    if (key.compare("Inferno", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#000004", "#1b0c41", "#4a0c6b", "#781c6d", "#a52c60",
                                 "#cf4446", "#ed6925", "#fb9b06", "#f7d13d", "#fcffa4" });
    }
    if (key.compare("CoolWarm", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#3b4cc0", "#5977e3", "#7b9ff9", "#9ebeff", "#c0d4f5",
                                 "#f2cbb7", "#f7a889", "#e87a5f", "#c53334", "#b40426" });
    }
    if (key.compare("Nature (NPG)", Qt::CaseInsensitive) == 0
        || key.compare("Nature", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#e64b35", "#4dbbd5", "#00a087", "#3c5488", "#f39b7f",
                                 "#8491b4", "#91d1c2", "#dc0000", "#7e6148", "#b09c85" });
    }
    if (key.compare("Science (AAAS)", Qt::CaseInsensitive) == 0
        || key.compare("Science", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#3b4992", "#ee0000", "#008b45", "#631879", "#008280",
                                 "#bb0021", "#5f559b", "#a20056", "#808180", "#1b1919" });
    }
    if (key.compare("Okabe-Ito (Colorblind Safe)", Qt::CaseInsensitive) == 0
        || key.compare("Okabe-Ito", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#0072b2", "#e69f00", "#009e73", "#d55e00",
                                 "#cc79a7", "#56b4e9", "#f0e442", "#000000" });
    }
    if (key.compare("Monochrome / Grayscale", Qt::CaseInsensitive) == 0
        || key.compare("Monochrome", Qt::CaseInsensitive) == 0
        || key.compare("Grayscale", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#000000", "#252525", "#454545", "#636363",
                                 "#7f7f7f", "#969696", "#bdbdbd", "#d9d9d9" });
    }

    auto &memStore = customPaletteMemoryStore();
    if (memStore.contains(key))
        return memStore.value(key);

    QSettings settings;
    settings.beginGroup("CustomColorPalettes");
    if (settings.contains(key)) {
        const QStringList hexList = settings.value(key).toStringList();
        settings.endGroup();
        QList<QColor> cols = hexListToColors(hexList);
        if (!cols.isEmpty()) {
            memStore.insert(key, cols);
            return cols;
        }
    } else {
        settings.endGroup();
    }

    // Fallback to ISIJ / Origin Default
    return hexListToColors({ "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd",
                             "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf" });
}

void Graph::saveCustomPalette(const QString &name, const QList<QColor> &colors)
{
    const QString key = name.trimmed();
    if (key.isEmpty() || colors.isEmpty())
        return;

    customPaletteMemoryStore().insert(key, colors);

    QStringList hexList;
    hexList.reserve(colors.size());
    for (const QColor &c : colors) {
        if (c.isValid())
            hexList.append(c.name());
    }
    QSettings settings;
    settings.beginGroup("CustomColorPalettes");
    settings.setValue(key, hexList);
    settings.endGroup();
}

void Graph::removeCustomPalette(const QString &name)
{
    const QString key = name.trimmed();
    if (key.isEmpty())
        return;

    customPaletteMemoryStore().remove(key);
    QSettings settings;
    settings.beginGroup("CustomColorPalettes");
    settings.remove(key);
    settings.endGroup();
}

QList<QColor> Graph::interpolatePalette(const QList<QColor> &anchors, int count)
{
    QList<QColor> result;
    if (count <= 0)
        return result;

    QList<QColor> validAnchors;
    for (const QColor &c : anchors) {
        if (c.isValid())
            validAnchors.append(c);
    }
    if (validAnchors.isEmpty())
        validAnchors = paletteColors("ISIJ / Origin Default");

    result.reserve(count);
    if (validAnchors.size() == 1 || count == 1) {
        for (int i = 0; i < count; ++i)
            result.append(validAnchors.first());
        return result;
    }

    const int nSeg = validAnchors.size() - 1;
    for (int i = 0; i < count; ++i) {
        const double t = double(i) / double(count - 1);
        const double pos = t * double(nSeg);
        const int idx = qMin(int(std::floor(pos)), nSeg - 1);
        const double frac = pos - double(idx);
        const QColor &c0 = validAnchors[idx];
        const QColor &c1 = validAnchors[idx + 1];

        const int r = qBound(0, qRound((1.0 - frac) * c0.red() + frac * c1.red()), 255);
        const int g = qBound(0, qRound((1.0 - frac) * c0.green() + frac * c1.green()), 255);
        const int b = qBound(0, qRound((1.0 - frac) * c0.blue() + frac * c1.blue()), 255);
        const int a = qBound(0, qRound((1.0 - frac) * c0.alpha() + frac * c1.alpha()), 255);
        result.append(QColor(r, g, b, a));
    }
    return result;
}

void Graph::applyColorPalette(const QList<QColor> &colors, bool gradientMode, bool applyToSymbols)
{
    QList<QColor> validColors;
    for (const QColor &c : colors) {
        if (c.isValid())
            validColors.append(c);
    }
    if (validColors.isEmpty() || n_curves <= 0)
        return;

    QList<int> targetIndices;
    for (int i = 0; i < n_curves; ++i) {
        if (c_type[i] == ErrorBars || c_type[i] == ColorMap || c_type[i] == GrayMap
            || c_type[i] == ContourMap || c_type[i] == Pie)
            continue;
        QwtPlotCurve *c = curve(i);
        if (c)
            targetIndices.append(i);
    }
    if (targetIndices.isEmpty())
        return;

    const int m = targetIndices.size();
    const QList<QColor> assignedColors = gradientMode ? interpolatePalette(validColors, m) : validColors;

    for (int k = 0; k < m; ++k) {
        const int idx = targetIndices[k];
        QwtPlotCurve *c = curve(idx);
        if (!c)
            continue;

        const QColor col = assignedColors[k % assignedColors.size()];
        QPen pen = c->pen();
        pen.setColor(col);
        c->setPen(pen);

        if (c_type[idx] == VectXYXY || c_type[idx] == VectXYAM) {
            VectorCurve *vc = dynamic_cast<VectorCurve *>(c);
            if (vc)
                vc->setColor(col);
        }

        if (c->brush().style() != Qt::NoBrush) {
            QBrush br = c->brush();
            QColor fillCol = col;
            if (br.color().alpha() < 255)
                fillCol.setAlpha(br.color().alpha());
            br.setColor(fillCol);
            c->setBrush(br);
        }

        if (applyToSymbols) {
            QwtSymbol sym = c->symbol();
            QPen symPen = sym.pen();
            symPen.setColor(col);
            sym.setPen(symPen);
            if (sym.brush().style() != Qt::NoBrush) {
                QBrush symBrush = sym.brush();
                symBrush.setColor(col);
                sym.setBrush(symBrush);
            }
            c->setSymbol(sym);
        }

        DataCurve *dc = dynamic_cast<DataCurve *>(c);
        if (dc) {
            for (DataCurve *eb : dc->errorBarsList()) {
                QwtErrorPlotCurve *errCurve = dynamic_cast<QwtErrorPlotCurve *>(eb);
                if (errCurve)
                    errCurve->setColor(col);
            }
        }
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::applyColorPaletteByName(const QString &paletteName, bool gradientMode)
{
    const QList<QColor> cols = paletteColors(paletteName);
    if (!cols.isEmpty())
        applyColorPalette(cols, gradientMode, true);
}

bool Graph::showColorPaletteEditor(QWidget *parent, Graph *targetGraph)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(tr("OriginPro Color Palette & Gradient Editor"));
    dlg.setMinimumSize(560, 480);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);

    // 1. Preset / Custom Palette Selector Row
    QGroupBox *presetGroup = new QGroupBox(tr("Color Palette Preset / Custom Palettes"), &dlg);
    QHBoxLayout *presetLayout = new QHBoxLayout(presetGroup);
    QComboBox *comboPalettes = new QComboBox(presetGroup);
    comboPalettes->addItems(allPaletteNames());
    QPushButton *btnSaveCustom = new QPushButton(tr("Save As Custom..."), presetGroup);
    QPushButton *btnDeleteCustom = new QPushButton(tr("Delete Custom"), presetGroup);
    presetLayout->addWidget(new QLabel(tr("Palette:"), presetGroup));
    presetLayout->addWidget(comboPalettes, 1);
    presetLayout->addWidget(btnSaveCustom);
    presetLayout->addWidget(btnDeleteCustom);
    mainLayout->addWidget(presetGroup);

    // 2. Color List & Swatch Editor
    QGroupBox *colorsGroup = new QGroupBox(tr("Palette Colors (Swatches & Hex Codes)"), &dlg);
    QHBoxLayout *colorsLayout = new QHBoxLayout(colorsGroup);
    QListWidget *colorListWidget = new QListWidget(colorsGroup);
    colorListWidget->setIconSize(QSize(36, 18));

    QVBoxLayout *colorBtnLayout = new QVBoxLayout();
    QPushButton *btnAddColor = new QPushButton(tr("Add Color..."), colorsGroup);
    QPushButton *btnEditColor = new QPushButton(tr("Edit Color..."), colorsGroup);
    QPushButton *btnRemoveColor = new QPushButton(tr("Remove Color"), colorsGroup);
    QPushButton *btnMoveUp = new QPushButton(tr("Move Up"), colorsGroup);
    QPushButton *btnMoveDown = new QPushButton(tr("Move Down"), colorsGroup);
    colorBtnLayout->addWidget(btnAddColor);
    colorBtnLayout->addWidget(btnEditColor);
    colorBtnLayout->addWidget(btnRemoveColor);
    colorBtnLayout->addSpacing(6);
    colorBtnLayout->addWidget(btnMoveUp);
    colorBtnLayout->addWidget(btnMoveDown);
    colorBtnLayout->addStretch();

    colorsLayout->addWidget(colorListWidget, 1);
    colorsLayout->addLayout(colorBtnLayout);
    mainLayout->addWidget(colorsGroup, 1);

    // 3. Gradient Generator Group
    QGroupBox *gradGroup = new QGroupBox(tr("Gradient Generator"), &dlg);
    QGridLayout *gradLayout = new QGridLayout(gradGroup);
    ColorButton *btnStartColor = new ColorButton(gradGroup);
    btnStartColor->setColor(QColor("#1f77b4"));
    QCheckBox *chkMidColor = new QCheckBox(tr("Middle Color:"), gradGroup);
    chkMidColor->setChecked(true);
    ColorButton *btnMidColor = new ColorButton(gradGroup);
    btnMidColor->setColor(QColor("#2ca02c"));
    ColorButton *btnEndColor = new ColorButton(gradGroup);
    btnEndColor->setColor(QColor("#d62728"));
    QSpinBox *spinGradSteps = new QSpinBox(gradGroup);
    spinGradSteps->setRange(2, 32);
    spinGradSteps->setValue(10);
    QPushButton *btnGenerateGrad = new QPushButton(tr("Generate Gradient"), gradGroup);

    gradLayout->addWidget(new QLabel(tr("Start Color:"), gradGroup), 0, 0);
    gradLayout->addWidget(btnStartColor, 0, 1);
    gradLayout->addWidget(chkMidColor, 0, 2);
    gradLayout->addWidget(btnMidColor, 0, 3);
    gradLayout->addWidget(new QLabel(tr("End Color:"), gradGroup), 1, 0);
    gradLayout->addWidget(btnEndColor, 1, 1);
    gradLayout->addWidget(new QLabel(tr("Steps (2-32):"), gradGroup), 1, 2);
    gradLayout->addWidget(spinGradSteps, 1, 3);
    gradLayout->addWidget(btnGenerateGrad, 0, 4, 2, 1);
    mainLayout->addWidget(gradGroup);

    // 4. Application Mode Group
    QGroupBox *modeGroup = new QGroupBox(tr("Application Mode"), &dlg);
    QHBoxLayout *modeLayout = new QHBoxLayout(modeGroup);
    QRadioButton *radioCycle = new QRadioButton(tr("Cycle Palette Colors"), modeGroup);
    QRadioButton *radioGradient =
            new QRadioButton(tr("Interpolate Gradient Across Curves"), modeGroup);
    QCheckBox *chkSymbols = new QCheckBox(tr("Apply to Symbols"), modeGroup);
    radioCycle->setChecked(true);
    chkSymbols->setChecked(true);
    modeLayout->addWidget(radioCycle);
    modeLayout->addWidget(radioGradient);
    modeLayout->addStretch();
    modeLayout->addWidget(chkSymbols);
    mainLayout->addWidget(modeGroup);

    // 5. Dialog Action Buttons
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    QPushButton *btnApplyGraph = new QPushButton(tr("Apply to Active Graph"), &dlg);
    QPushButton *btnOk = new QPushButton(tr("OK"), &dlg);
    QPushButton *btnCancel = new QPushButton(tr("Cancel"), &dlg);
    btnOk->setDefault(true);
    bottomLayout->addWidget(btnApplyGraph);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnOk);
    bottomLayout->addWidget(btnCancel);
    mainLayout->addLayout(bottomLayout);

    auto makeSwatchIcon = [](const QColor &c) -> QIcon {
        QPixmap pm(36, 18);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.fillRect(0, 0, 35, 17, c);
        p.setPen(QColor("#303030"));
        p.drawRect(0, 0, 35, 17);
        p.end();
        return QIcon(pm);
    };

    auto populateList = [&](const QList<QColor> &cols) {
        colorListWidget->clear();
        for (int i = 0; i < cols.size(); ++i) {
            const QColor &c = cols[i];
            QListWidgetItem *item = new QListWidgetItem(
                    makeSwatchIcon(c),
                    QString("#%1  %2").arg(i + 1, 2, 10, QChar('0')).arg(c.name().toUpper()));
            item->setData(Qt::UserRole, c);
            colorListWidget->addItem(item);
        }
        if (colorListWidget->count() > 0)
            colorListWidget->setCurrentRow(0);
    };

    auto currentColorsFromList = [&]() -> QList<QColor> {
        QList<QColor> cols;
        for (int i = 0; i < colorListWidget->count(); ++i) {
            QColor c = colorListWidget->item(i)->data(Qt::UserRole).value<QColor>();
            if (c.isValid())
                cols.append(c);
        }
        return cols;
    };

    auto renumberItems = [&]() {
        for (int i = 0; i < colorListWidget->count(); ++i) {
            QListWidgetItem *it = colorListWidget->item(i);
            QColor c = it->data(Qt::UserRole).value<QColor>();
            it->setIcon(makeSwatchIcon(c));
            it->setText(QString("#%1  %2").arg(i + 1, 2, 10, QChar('0')).arg(c.name().toUpper()));
        }
    };

    populateList(paletteColors(comboPalettes->currentText()));
    btnDeleteCustom->setEnabled(!builtInPaletteNames().contains(comboPalettes->currentText()));

    QObject::connect(comboPalettes, &QComboBox::currentTextChanged, &dlg, [&](const QString &name) {
        if (name.isEmpty())
            return;
        populateList(paletteColors(name));
        btnDeleteCustom->setEnabled(!builtInPaletteNames().contains(name));
    });

    QObject::connect(chkMidColor, &QCheckBox::toggled, btnMidColor, &QWidget::setEnabled);

    QObject::connect(btnAddColor, &QPushButton::clicked, &dlg, [&]() {
        QColor c = QColorDialog::getColor(QColor("#1f77b4"), &dlg, tr("Select Color"),
                                          QColorDialog::DontUseNativeDialog);
        if (!c.isValid())
            return;
        QListWidgetItem *item = new QListWidgetItem(makeSwatchIcon(c), c.name().toUpper());
        item->setData(Qt::UserRole, c);
        colorListWidget->addItem(item);
        renumberItems();
        colorListWidget->setCurrentItem(item);
    });

    auto editSelectedColor = [&]() {
        QListWidgetItem *item = colorListWidget->currentItem();
        if (!item)
            return;
        QColor initial = item->data(Qt::UserRole).value<QColor>();
        QColor c = QColorDialog::getColor(initial, &dlg, tr("Edit Color"),
                                          QColorDialog::DontUseNativeDialog);
        if (!c.isValid())
            return;
        item->setData(Qt::UserRole, c);
        renumberItems();
    };

    QObject::connect(btnEditColor, &QPushButton::clicked, &dlg, editSelectedColor);
    QObject::connect(colorListWidget, &QListWidget::itemDoubleClicked, &dlg,
                     [&](QListWidgetItem *) { editSelectedColor(); });

    QObject::connect(btnRemoveColor, &QPushButton::clicked, &dlg, [&]() {
        int row = colorListWidget->currentRow();
        if (row < 0 || colorListWidget->count() <= 1)
            return;
        delete colorListWidget->takeItem(row);
        renumberItems();
    });

    QObject::connect(btnMoveUp, &QPushButton::clicked, &dlg, [&]() {
        int row = colorListWidget->currentRow();
        if (row <= 0)
            return;
        QListWidgetItem *it = colorListWidget->takeItem(row);
        colorListWidget->insertItem(row - 1, it);
        colorListWidget->setCurrentRow(row - 1);
        renumberItems();
    });

    QObject::connect(btnMoveDown, &QPushButton::clicked, &dlg, [&]() {
        int row = colorListWidget->currentRow();
        if (row < 0 || row >= colorListWidget->count() - 1)
            return;
        QListWidgetItem *it = colorListWidget->takeItem(row);
        colorListWidget->insertItem(row + 1, it);
        colorListWidget->setCurrentRow(row + 1);
        renumberItems();
    });

    QObject::connect(btnGenerateGrad, &QPushButton::clicked, &dlg, [&]() {
        QList<QColor> anchors;
        anchors.append(btnStartColor->color());
        if (chkMidColor->isChecked())
            anchors.append(btnMidColor->color());
        anchors.append(btnEndColor->color());
        populateList(interpolatePalette(anchors, spinGradSteps->value()));
    });

    QObject::connect(btnSaveCustom, &QPushButton::clicked, &dlg, [&]() {
        bool ok = false;
        QString name = QInputDialog::getText(&dlg, tr("Save Custom Palette"),
                                             tr("Custom Palette Name:"), QLineEdit::Normal,
                                             tr("My Custom Palette"), &ok)
                               .trimmed();
        if (!ok || name.isEmpty())
            return;
        saveCustomPalette(name, currentColorsFromList());
        comboPalettes->blockSignals(true);
        comboPalettes->clear();
        comboPalettes->addItems(allPaletteNames());
        comboPalettes->setCurrentText(name);
        comboPalettes->blockSignals(false);
        btnDeleteCustom->setEnabled(!builtInPaletteNames().contains(name));
    });

    QObject::connect(btnDeleteCustom, &QPushButton::clicked, &dlg, [&]() {
        const QString name = comboPalettes->currentText();
        if (builtInPaletteNames().contains(name))
            return;
        removeCustomPalette(name);
        comboPalettes->clear();
        comboPalettes->addItems(allPaletteNames());
    });

    bool appliedAny = false;
    auto applyToTarget = [&]() {
        if (!targetGraph)
            return;
        QList<QColor> cols = currentColorsFromList();
        if (!cols.isEmpty()) {
            targetGraph->applyColorPalette(cols, radioGradient->isChecked(),
                                           chkSymbols->isChecked());
            appliedAny = true;
        }
    };

    QObject::connect(btnApplyGraph, &QPushButton::clicked, &dlg, applyToTarget);
    QObject::connect(btnOk, &QPushButton::clicked, &dlg, [&]() {
        applyToTarget();
        dlg.accept();
    });
    QObject::connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    const int res = dlg.exec();
    return (res == QDialog::Accepted) || appliedAny;
}

// ============================================================================
// OriginPro Plot Types Support (Waterfall Y-Offset Stack & Ternary Diagram)
// ============================================================================

void Graph::applyYOffsetStack(double offsetStep, bool autoOffset)
{
    QList<QwtPlotCurve *> visibleDataCurves;
    for (int i = 0; i < n_curves; ++i) {
        if (c_type[i] == ErrorBars || c_type[i] == Pie || c_type[i] == Box
            || c_type[i] == ColorMap || c_type[i] == GrayMap || c_type[i] == ContourMap)
            continue;
        QwtPlotCurve *c = curve(i);
        if (c && c->isVisible() && c->dataSize() > 0)
            visibleDataCurves.append(c);
    }

    if (visibleDataCurves.size() < 2)
        return;

    double deltaY = offsetStep;
    if (autoOffset && qAbs(offsetStep) <= 1e-15) {
        double sumSpan = 0.0;
        int validSpans = 0;
        for (QwtPlotCurve *c : visibleDataCurves) {
            const int n = c->dataSize();
            if (n <= 0)
                continue;
            double minY = c->y(0);
            double maxY = c->y(0);
            for (int j = 1; j < n; ++j) {
                const double yv = c->y(j);
                if (yv < minY)
                    minY = yv;
                if (yv > maxY)
                    maxY = yv;
            }
            sumSpan += (maxY - minY);
            validSpans++;
        }
        const double avgSpan = (validSpans > 0) ? (sumSpan / double(validSpans)) : 0.0;
        deltaY = (avgSpan > 1e-12) ? (0.8 * avgSpan) : 1.0;
    }

    for (int k = 1; k < visibleDataCurves.size(); ++k) {
        QwtPlotCurve *c = visibleDataCurves[k];
        const int n = c->dataSize();
        const double yShift = double(k) * deltaY;
        QVector<double> xVals(n);
        QVector<double> yVals(n);
        for (int j = 0; j < n; ++j) {
            xVals[j] = c->x(j);
            yVals[j] = c->y(j) + yShift;
        }
        c->setData(xVals.constData(), yVals.constData(), n);

        DataCurve *dc = dynamic_cast<DataCurve *>(c);
        if (dc) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->setData(xVals.constData(), yVals.constData(), n);
            }
        }
    }

    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis)
        d_plot->setAxisAutoScale(axis);
    updatePlot();
    emit modifiedGraph();
}

QPointF Graph::ternaryToCartesian(double a, double b, double c)
{
    const double sum = a + b + c;
    const double h = std::sqrt(3.0) * 0.5;
    if (qAbs(sum) <= 1e-15)
        return QPointF(0.5, h / 3.0);

    const double bn = b / sum;
    const double cn = c / sum;
    const double x = bn + 0.5 * cn;
    const double y = h * cn;
    return QPointF(x, y);
}

void Graph::setupTernaryPlot(const QString &labelA, const QString &labelB, const QString &labelC)
{
    d_is_ternary = true;
    d_ternary_labels[0] = labelA.isEmpty() ? QString("A") : labelA;
    d_ternary_labels[1] = labelB.isEmpty() ? QString("B") : labelB;
    d_ternary_labels[2] = labelC.isEmpty() ? QString("C") : labelC;
    m_autoscale = false;

    const double h = std::sqrt(3.0) * 0.5; // ~0.8660254

    // Hide rectangular axes and rectangular grids
    if (grid()) {
        grid()->enableX(false);
        grid()->enableXMin(false);
        grid()->enableY(false);
        grid()->enableYMin(false);
    }
    drawCanvasFrame(false, 0);

    setScale(QwtPlot::xBottom, -0.14, 1.14, 0.2, 6, 0, 0, false);
    setScale(QwtPlot::yLeft, -0.12, h + 0.14, 0.2, 6, 0, 0, false);
    for (int i = 0; i < QwtPlot::axisCnt; ++i)
        enableAxis(i, false);
    d_plot->replot();

    auto addTernaryLine = [&](const QPointF &p1, const QPointF &p2, const QColor &col, int width,
                              Qt::PenStyle penStyle) {
        ArrowMarker *mrk = new ArrowMarker();
        mrk->drawStartArrow(false);
        mrk->drawEndArrow(false);
        mrk->setColor(col);
        mrk->setWidth(width);
        mrk->setStyle(penStyle);
        long id = d_plot->insertMarker(mrk);
        d_lines.append(id);
        mrk->setBoundingRect(p1.x(), p1.y(), p2.x(), p2.y());
    };

    auto addTernaryLabel = [&](double x, double y, const QString &text, int pointSize, bool bold,
                               const QColor &col = QColor(Qt::black)) {
        Legend *txt = new Legend(d_plot);
        txt->setFrameStyle(Legend::None);
        txt->setBackgroundColor(QColor(255, 255, 255, 0));
        txt->setTextColor(col);
        txt->setFont(QFont("Arial", pointSize, bold ? QFont::Bold : QFont::Normal));
        txt->setText(text);
        long id = d_plot->insertMarker(txt);
        d_texts.append(id);
        txt->setXValue(-999.0);
        txt->setOriginCoord(x, y);
    };

    // 1. Interior 20%-step ternary grid lines & exterior ticks
    const QColor gridColor("#b8b8b8");
    const QColor tickColor(Qt::black);
    for (int step = 1; step <= 4; ++step) {
        const double f = 0.2 * double(step);
        const QString pctText = QString::number(step * 20);

        // Constant-A grid line: (f, 1-f, 0) -> (f, 0, 1-f)
        addTernaryLine(ternaryToCartesian(f, 1.0 - f, 0.0), ternaryToCartesian(f, 0.0, 1.0 - f),
                       gridColor, 1, Qt::DashLine);
        // Constant-B grid line: (1-f, f, 0) -> (0, f, 1-f)
        addTernaryLine(ternaryToCartesian(1.0 - f, f, 0.0), ternaryToCartesian(0.0, f, 1.0 - f),
                       gridColor, 1, Qt::DashLine);
        // Constant-C grid line: (1-f, 0, f) -> (0, 1-f, f)
        addTernaryLine(ternaryToCartesian(1.0 - f, 0.0, f), ternaryToCartesian(0.0, 1.0 - f, f),
                       gridColor, 1, Qt::DashLine);

        // Ticks along Base AB (z = 0, x = f)
        const QPointF basePt(f, 0.0);
        addTernaryLine(basePt, QPointF(f, -0.022), tickColor, 1, Qt::SolidLine);
        addTernaryLabel(f - 0.025, -0.032, pctText, 9, false, QColor("#333333"));

        // Ticks along Right Edge BC (a = 0, b = 1-f, c = f)
        const QPointF rightPt = ternaryToCartesian(0.0, 1.0 - f, f);
        addTernaryLine(rightPt, QPointF(rightPt.x() + 0.020, rightPt.y() + 0.011), tickColor, 1,
                       Qt::SolidLine);
        addTernaryLabel(rightPt.x() + 0.028, rightPt.y() + 0.025, pctText, 9, false,
                        QColor("#333333"));

        // Ticks along Left Edge CA (a = 1-f, b = 0, c = f)
        const QPointF leftPt = ternaryToCartesian(1.0 - f, 0.0, f);
        addTernaryLine(leftPt, QPointF(leftPt.x() - 0.020, leftPt.y() + 0.011), tickColor, 1,
                       Qt::SolidLine);
        addTernaryLabel(leftPt.x() - 0.075, leftPt.y() + 0.025, pctText, 9, false,
                        QColor("#333333"));
    }

    // 2. Equilateral triangle perimeter (A=(0,0), B=(1,0), C=(0.5, sqrt(3)/2))
    const QPointF vA(0.0, 0.0);
    const QPointF vB(1.0, 0.0);
    const QPointF vC(0.5, h);
    addTernaryLine(vA, vB, QColor(Qt::black), 2, Qt::SolidLine);
    addTernaryLine(vB, vC, QColor(Qt::black), 2, Qt::SolidLine);
    addTernaryLine(vC, vA, QColor(Qt::black), 2, Qt::SolidLine);

    // 3. Vertex labels A (Bottom-Left), B (Bottom-Right), C (Top Apex)
    addTernaryLabel(-0.09, -0.02, labelA.isEmpty() ? QString("A") : labelA, 12, true);
    addTernaryLabel(1.02, -0.02, labelB.isEmpty() ? QString("B") : labelB, 12, true);
    addTernaryLabel(0.46, h + 0.08, labelC.isEmpty() ? QString("C") : labelC, 12, true);

    d_plot->replot();
    emit modifiedGraph();
}

// ============================================================================
// OriginPro Frame Presets, Per-Side Axis Backbones, Reference Lines & Linked Axes
// ============================================================================

namespace {
QString localizedStatModeLabel(const QString &rawStatMode)
{
    const QString trimmed = rawStatMode.trimmed();
    QString norm = trimmed.toLower();
    norm.remove(' ');
    if (norm == "mean")
        return Graph::tr("Mean");
    if (norm == "median")
        return Graph::tr("Median");
    if (norm == "mean+sd" || norm == "+sd")
        return Graph::tr("+SD");
    if (norm == "mean-sd" || norm == "-sd")
        return Graph::tr("-SD");
    if (norm == "min")
        return Graph::tr("Min");
    if (norm == "max")
        return Graph::tr("Max");
    return trimmed;
}

class OriginReferenceFillItem : public QwtPlotItem
{
public:
    explicit OriginReferenceFillItem(const Graph *graph) : d_graph(graph)
    {
        setZ(14.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 41; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_graph)
            return;
        const QVector<Graph::ReferenceLineSpec> lines = d_graph->referenceLines();
        if (lines.isEmpty())
            return;

        painter->save();
        for (const Graph::ReferenceLineSpec &spec : lines) {
            if (!spec.fillToAxis || !std::isfinite(spec.value))
                continue;
            if (spec.orientation == 0) {
                const double yPx = yMap.transform(spec.value);
                const double yLo = qMin(yMap.s1(), yMap.s2());
                const double yHi = qMax(yMap.s1(), yMap.s2());
                const double basePx = (0.0 >= yLo && 0.0 <= yHi) ? yMap.transform(0.0)
                                                                 : double(canvasRect.bottom());
                QRectF fillRect(canvasRect.left(), qMin(yPx, basePx), canvasRect.width(),
                                qAbs(basePx - yPx));
                fillRect = fillRect.intersected(QRectF(canvasRect));
                if (!fillRect.isEmpty())
                    painter->fillRect(fillRect, spec.fillColor);
            } else {
                const double xPx = xMap.transform(spec.value);
                const double xLo = qMin(xMap.s1(), xMap.s2());
                const double xHi = qMax(xMap.s1(), xMap.s2());
                const double basePx = (0.0 >= xLo && 0.0 <= xHi) ? xMap.transform(0.0)
                                                                 : double(canvasRect.left());
                QRectF fillRect(qMin(xPx, basePx), canvasRect.top(), qAbs(basePx - xPx),
                                canvasRect.height());
                fillRect = fillRect.intersected(QRectF(canvasRect));
                if (!fillRect.isEmpty())
                    painter->fillRect(fillRect, spec.fillColor);
            }
        }
        painter->restore();
    }

private:
    const Graph *d_graph;
};

class OriginReferenceLineItem : public QwtPlotItem
{
public:
    explicit OriginReferenceLineItem(const Graph *graph) : d_graph(graph)
    {
        setZ(65.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 42; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_graph)
            return;
        const QVector<Graph::ReferenceLineSpec> lines = d_graph->referenceLines();
        if (lines.isEmpty())
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing,
                               const_cast<Graph *>(d_graph)->antialiasing());

        const QFont labelFont("Arial", 9, QFont::Bold);
        const QFontMetrics fm(labelFont);

        for (const Graph::ReferenceLineSpec &spec : lines) {
            if (!std::isfinite(spec.value))
                continue;

            QString displayLabel = spec.label.trimmed();
            if (displayLabel.isEmpty() && !spec.statMode.trimmed().isEmpty()
                && spec.statMode.trimmed().compare("Constant", Qt::CaseInsensitive) != 0) {
                displayLabel = QString("%1 = %2")
                                       .arg(localizedStatModeLabel(spec.statMode))
                                       .arg(spec.value, 0, 'g', 4);
            }

            if (spec.orientation == 0) {
                // Horizontal line (Y = spec.value)
                const double yPx = yMap.transform(spec.value);
                if (yPx >= canvasRect.top() - 2 && yPx <= canvasRect.bottom() + 2) {
                    QPen pen(spec.color, qMax(0.5, spec.width), spec.style);
                    painter->setPen(pen);
                    painter->drawLine(QPointF(canvasRect.left(), yPx),
                                      QPointF(canvasRect.right(), yPx));

                    if (!displayLabel.isEmpty()) {
                        painter->setFont(labelFont);
                        const int tw = fm.horizontalAdvance(displayLabel);
                        const int th = fm.height();
                        double tx = canvasRect.right() - tw - 8;
                        if (tx < canvasRect.left() + 4)
                            tx = canvasRect.left() + 4;
                        double ty = yPx - 5;
                        if (ty - fm.ascent() < canvasRect.top() + 2)
                            ty = yPx + fm.ascent() + 4;
                        QRectF pill(tx - 3, ty - fm.ascent() - 1, tw + 6, th + 2);
                        painter->fillRect(pill, QColor(255, 255, 255, 205));
                        painter->setPen(spec.color.darker(115));
                        painter->drawText(QPointF(tx, ty), displayLabel);
                    }
                }
            } else {
                // Vertical line (X = spec.value)
                const double xPx = xMap.transform(spec.value);
                if (xPx >= canvasRect.left() - 2 && xPx <= canvasRect.right() + 2) {
                    QPen pen(spec.color, qMax(0.5, spec.width), spec.style);
                    painter->setPen(pen);
                    painter->drawLine(QPointF(xPx, canvasRect.top()),
                                      QPointF(xPx, canvasRect.bottom()));

                    if (!displayLabel.isEmpty()) {
                        painter->setFont(labelFont);
                        const int tw = fm.horizontalAdvance(displayLabel);
                        const int th = fm.height();
                        double tx = xPx + 5;
                        if (tx + tw > canvasRect.right() - 4)
                            tx = xPx - tw - 5;
                        double ty = canvasRect.top() + fm.ascent() + 6;
                        QRectF pill(tx - 3, ty - fm.ascent() - 1, tw + 6, th + 2);
                        painter->fillRect(pill, QColor(255, 255, 255, 205));
                        painter->setPen(spec.color.darker(115));
                        painter->drawText(QPointF(tx, ty), displayLabel);
                    }
                }
            }
        }

        painter->restore();
    }

private:
    const Graph *d_graph;
};
} // namespace

MultiLayer *Graph::multiLayerParent() const
{
    QObject *p = parent();
    while (p) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(p))
            return ml;
        p = p->parent();
    }
    return nullptr;
}

void Graph::setAxisBackboneVisible(int axis, bool visible)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    d_axis_backbone[axis] = visible;
    bool anyVisible = false;
    for (int i = 0; i < QwtPlot::axisCnt; ++i) {
        if (d_axis_backbone[i])
            anyVisible = true;
    }
    drawAxesBackbone = anyVisible;

    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale) {
        ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);
        if (sclDraw)
            sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, visible);
        scale->repaint();
    }

    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::isAxisBackboneVisible(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return false;
    return drawAxesBackbone && d_axis_backbone[axis];
}

void Graph::applyFramePreset(FramePreset preset)
{
    bool hasRightCurve = false;
    bool hasTopCurve = false;
    for (int i = 0; i < n_curves; ++i) {
        QwtPlotItem *it = plotItem(i);
        if (!it)
            continue;
        if (it->yAxis() == QwtPlot::yRight)
            hasRightCurve = true;
        if (it->xAxis() == QwtPlot::xTop)
            hasTopCurve = true;
    }

    auto setTickLabelsOnAxis = [&](int axis, bool on) {
        QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
        if (sd)
            sd->enableComponent(QwtAbstractScaleDraw::Labels, on);
    };

    if (d_plot->grid()) {
        d_plot->grid()->enableZeroLineX(false);
        d_plot->grid()->enableZeroLineY(false);
    }

    switch (preset) {
    case FrameBoxInwardTicks: {
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            enableAxis(a, true);
        drawAxesBackbones(true);
        setTickLabelsOnAxis(QwtPlot::yLeft, true);
        setTickLabelsOnAxis(QwtPlot::xBottom, true);
        setTickLabelsOnAxis(QwtPlot::yRight, hasRightCurve);
        setTickLabelsOnAxis(QwtPlot::xTop, hasTopCurve);
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            d_plot->setMajorTicksType(a, ScaleDraw::In);
            d_plot->setMinorTicksType(a, ScaleDraw::In);
        }
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
        break;
    }
    case FrameBoxPlainOpposite: {
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            enableAxis(a, true);
        drawAxesBackbones(true);
        setTickLabelsOnAxis(QwtPlot::yLeft, true);
        setTickLabelsOnAxis(QwtPlot::xBottom, true);
        setTickLabelsOnAxis(QwtPlot::yRight, hasRightCurve);
        setTickLabelsOnAxis(QwtPlot::xTop, hasTopCurve);
        d_plot->setMajorTicksType(QwtPlot::yLeft, ScaleDraw::In);
        d_plot->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::In);
        d_plot->setMajorTicksType(QwtPlot::xBottom, ScaleDraw::In);
        d_plot->setMinorTicksType(QwtPlot::xBottom, ScaleDraw::In);
        d_plot->setMajorTicksType(QwtPlot::yRight,
                                  hasRightCurve ? ScaleDraw::In : ScaleDraw::None);
        d_plot->setMinorTicksType(QwtPlot::yRight,
                                  hasRightCurve ? ScaleDraw::In : ScaleDraw::None);
        d_plot->setMajorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::In : ScaleDraw::None);
        d_plot->setMinorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::In : ScaleDraw::None);
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
        break;
    }
    case FrameOpenL: {
        enableAxis(QwtPlot::yLeft, true);
        enableAxis(QwtPlot::xBottom, true);
        enableAxis(QwtPlot::yRight, hasRightCurve);
        enableAxis(QwtPlot::xTop, hasTopCurve);
        setAxisBackboneVisible(QwtPlot::yLeft, true);
        setAxisBackboneVisible(QwtPlot::xBottom, true);
        setAxisBackboneVisible(QwtPlot::yRight, hasRightCurve);
        setAxisBackboneVisible(QwtPlot::xTop, hasTopCurve);
        setTickLabelsOnAxis(QwtPlot::yLeft, true);
        setTickLabelsOnAxis(QwtPlot::xBottom, true);
        setTickLabelsOnAxis(QwtPlot::yRight, hasRightCurve);
        setTickLabelsOnAxis(QwtPlot::xTop, hasTopCurve);
        d_plot->setMajorTicksType(QwtPlot::yLeft, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::Out);
        d_plot->setMajorTicksType(QwtPlot::xBottom, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::xBottom, ScaleDraw::Out);
        d_plot->setMajorTicksType(QwtPlot::yRight,
                                  hasRightCurve ? ScaleDraw::Out : ScaleDraw::None);
        d_plot->setMinorTicksType(QwtPlot::yRight,
                                  hasRightCurve ? ScaleDraw::Out : ScaleDraw::None);
        d_plot->setMajorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::Out : ScaleDraw::None);
        d_plot->setMinorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::Out : ScaleDraw::None);
        break;
    }
    case FrameZeroCrossed: {
        enableAxis(QwtPlot::yLeft, true);
        enableAxis(QwtPlot::xBottom, true);
        enableAxis(QwtPlot::yRight, false);
        enableAxis(QwtPlot::xTop, false);
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            setAxisBackboneVisible(a, false);
        setTickLabelsOnAxis(QwtPlot::yLeft, true);
        setTickLabelsOnAxis(QwtPlot::xBottom, true);
        d_plot->setMajorTicksType(QwtPlot::yLeft, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::Out);
        d_plot->setMajorTicksType(QwtPlot::xBottom, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::xBottom, ScaleDraw::Out);
        if (d_plot->grid()) {
            d_plot->grid()->enableZeroLineX(true);
            d_plot->grid()->enableZeroLineY(true);
        }
        break;
    }
    }

    scalePicker->refresh();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::addReferenceLine(const ReferenceLineSpec &spec)
{
    if (!d_ref_fill_item) {
        d_ref_fill_item = new OriginReferenceFillItem(this);
        d_ref_fill_item->attach(d_plot);
    }
    if (!d_ref_line_item) {
        d_ref_line_item = new OriginReferenceLineItem(this);
        d_ref_line_item->attach(d_plot);
    }
    d_ref_lines.append(spec);
    updateStatReferenceLines();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setReferenceLines(const QVector<ReferenceLineSpec> &specs)
{
    if (!specs.isEmpty()) {
        if (!d_ref_fill_item) {
            d_ref_fill_item = new OriginReferenceFillItem(this);
            d_ref_fill_item->attach(d_plot);
        }
        if (!d_ref_line_item) {
            d_ref_line_item = new OriginReferenceLineItem(this);
            d_ref_line_item->attach(d_plot);
        }
    }
    d_ref_lines = specs;
    updateStatReferenceLines();
    d_plot->replot();
    emit modifiedGraph();
}

QVector<Graph::ReferenceLineSpec> Graph::referenceLines() const
{
    return d_ref_lines;
}

void Graph::clearReferenceLines()
{
    d_ref_lines.clear();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::updateStatReferenceLines()
{
    if (d_ref_lines.isEmpty())
        return;

    bool hasAnyStat = false;
    for (const ReferenceLineSpec &s : d_ref_lines) {
        if (!s.statMode.trimmed().isEmpty()
            && s.statMode.trimmed().compare("Constant", Qt::CaseInsensitive) != 0) {
            hasAnyStat = true;
            break;
        }
    }
    if (!hasAnyStat)
        return;

    QwtPlotCurve *activeCurve = nullptr;
    if (d_range_selector && d_range_selector->selectedCurve()) {
        activeCurve = d_range_selector->selectedCurve();
    }
    if (!activeCurve) {
        for (int i = 0; i < n_curves; ++i) {
            if (c_type[i] == ErrorBars || c_type[i] == Pie || c_type[i] == Box
                || c_type[i] == ColorMap || c_type[i] == GrayMap || c_type[i] == ContourMap)
                continue;
            QwtPlotCurve *c = curve(i);
            if (c && c->isVisible() && c->dataSize() > 0) {
                activeCurve = c;
                break;
            }
        }
    }
    if (!activeCurve) {
        for (int i = 0; i < n_curves; ++i) {
            QwtPlotCurve *c = curve(i);
            if (c && c->dataSize() > 0) {
                activeCurve = c;
                break;
            }
        }
    }
    if (!activeCurve)
        return;

    DataCurve *dc = dynamic_cast<DataCurve *>(activeCurve);

    auto computeStats = [&](int orientation, double &outMean, double &outSD, double &outMedian,
                            double &outMin, double &outMax) -> bool {
        QVector<double> vals;
        const int n = activeCurve->dataSize();
        vals.reserve(n);
        for (int j = 0; j < n; ++j) {
            if (dc && dc->isPointMasked(j))
                continue;
            const double v = (orientation == 0) ? activeCurve->y(j) : activeCurve->x(j);
            if (std::isfinite(v))
                vals.append(v);
        }
        if (vals.isEmpty())
            return false;

        double sum = 0.0;
        for (double v : vals)
            sum += v;
        outMean = sum / double(vals.size());

        double sqSum = 0.0;
        for (double v : vals) {
            const double d = v - outMean;
            sqSum += d * d;
        }
        outSD = (vals.size() > 1) ? std::sqrt(sqSum / double(vals.size() - 1)) : 0.0;

        QVector<double> sorted = vals;
        std::sort(sorted.begin(), sorted.end());
        const int m = sorted.size();
        outMedian = (m % 2 == 1) ? sorted[m / 2] : 0.5 * (sorted[m / 2 - 1] + sorted[m / 2]);
        outMin = sorted.first();
        outMax = sorted.last();
        return true;
    };

    double yMean = 0.0, ySD = 0.0, yMed = 0.0, yMin = 0.0, yMax = 0.0;
    bool yReady = false, yValid = false;
    double xMean = 0.0, xSD = 0.0, xMed = 0.0, xMin = 0.0, xMax = 0.0;
    bool xReady = false, xValid = false;

    for (ReferenceLineSpec &spec : d_ref_lines) {
        QString mode = spec.statMode.trimmed().toLower();
        mode.remove(' ');
        if (mode.isEmpty() || mode == "constant")
            continue;

        double mean = 0.0, sd = 0.0, med = 0.0, minV = 0.0, maxV = 0.0;
        if (spec.orientation == 0) {
            if (!yReady) {
                yValid = computeStats(0, yMean, ySD, yMed, yMin, yMax);
                yReady = true;
            }
            if (!yValid)
                continue;
            mean = yMean;
            sd = ySD;
            med = yMed;
            minV = yMin;
            maxV = yMax;
        } else {
            if (!xReady) {
                xValid = computeStats(1, xMean, xSD, xMed, xMin, xMax);
                xReady = true;
            }
            if (!xValid)
                continue;
            mean = xMean;
            sd = xSD;
            med = xMed;
            minV = xMin;
            maxV = xMax;
        }

        if (mode == "mean")
            spec.value = mean;
        else if (mode == "median")
            spec.value = med;
        else if (mode == "mean+sd" || mode == "+sd")
            spec.value = mean + sd;
        else if (mode == "mean-sd" || mode == "-sd")
            spec.value = mean - sd;
        else if (mode == "sd")
            spec.value = sd;
        else if (mode == "min")
            spec.value = minV;
        else if (mode == "max")
            spec.value = maxV;
    }
}

void Graph::setLinkedParentLayer(Graph *parentLayer, int xLinkMode, const QString &xExpr,
                                 int yLinkMode, const QString &yExpr)
{
    if (parentLayer == this)
        parentLayer = nullptr;

    d_linked_parent = parentLayer;
    d_x_link_mode = xLinkMode;
    d_x_link_expr = xExpr.trimmed().isEmpty() ? QString("x") : xExpr.trimmed();
    d_y_link_mode = yLinkMode;
    d_y_link_expr = yExpr.trimmed().isEmpty() ? QString("y") : yExpr.trimmed();

    if (d_linked_parent)
        syncLinkedAxesFromParent();
}

Graph *Graph::linkedParentLayer() const
{
    return d_linked_parent.data();
}

void Graph::syncLinkedAxesFromParent()
{
    if (d_syncing_link || !d_linked_parent)
        return;

    d_syncing_link = true;
    Plot *parentPlot = d_linked_parent->plotWidget();
    if (!parentPlot) {
        d_syncing_link = false;
        return;
    }

    auto evalEndpoint = [](const QString &expr, double val, bool &ok) -> double {
        ok = false;
        if (expr.isEmpty()) {
            ok = true;
            return val;
        }
        try {
            MyParser parser;
            double v = val;
            parser.DefineVar(_T("x"), &v);
            parser.DefineVar(_T("x1"), &v);
            parser.DefineVar(_T("X"), &v);
            parser.DefineVar(_T("y"), &v);
            parser.DefineVar(_T("y1"), &v);
            parser.DefineVar(_T("Y"), &v);
            parser.SetExpr(expr);
            const double res = parser.Eval();
            if (std::isfinite(res)) {
                ok = true;
                return res;
            }
        } catch (...) {
        }
        return val;
    };

    if (d_x_link_mode == 1 || d_x_link_mode == 2) {
        const int srcAxis = parentPlot->axisEnabled(QwtPlot::xBottom) ? QwtPlot::xBottom
                                                                      : QwtPlot::xTop;
        const QwtScaleDiv *pDiv = parentPlot->axisScaleDiv(srcAxis);
        const QwtScaleEngine *pEng = parentPlot->axisScaleEngine(srcAxis);
        if (pDiv && pEng) {
            const double p1 = pDiv->lowerBound();
            const double p2 = pDiv->upperBound();
            double c1 = p1;
            double c2 = p2;
            bool ok = true;
            if (d_x_link_mode == 2) {
                bool ok1 = false, ok2 = false;
                c1 = evalEndpoint(d_x_link_expr, p1, ok1);
                c2 = evalEndpoint(d_x_link_expr, p2, ok2);
                ok = ok1 && ok2;
            }
            if (ok && qAbs(c2 - c1) > 1e-300) {
                const double lo = qMin(c1, c2);
                const double hi = qMax(c1, c2);
                const int scaleType = (d_x_link_mode == 1
                                       && pEng->transformation()->type()
                                               == QwtScaleTransformation::Log10
                                       && lo > 0.0)
                        ? 1
                        : 0;
                const bool inv = pEng->testAttribute(QwtScaleEngine::Inverted);
                const int maj = parentPlot->axisMaxMajor(srcAxis);
                const int minT = parentPlot->axisMaxMinor(srcAxis);

                auto applyAxis = [&](int targetAxis) {
                    QwtScaleEngine *se = scaleType ? static_cast<QwtScaleEngine *>(new QwtLog10ScaleEngine())
                                                   : static_cast<QwtScaleEngine *>(new QwtLinearScaleEngine());
                    int maxMinInt = minT;
                    if (minT == 1)
                        maxMinInt = 3;
                    else if (minT > 1)
                        maxMinInt = minT + 1;
                    QwtScaleDiv div = se->divideScale(lo, hi, maj, maxMinInt, 0.0);
                    if (inv) {
                        se->setAttribute(QwtScaleEngine::Inverted);
                        div.invert();
                    }
                    d_plot->setAxisMaxMajor(targetAxis, maj);
                    d_plot->setAxisMaxMinor(targetAxis, minT);
                    d_plot->setAxisScaleEngine(targetAxis, se);
                    d_plot->setAxisScaleDiv(targetAxis, div);
                };

                applyAxis(QwtPlot::xTop);
                applyAxis(QwtPlot::xBottom);
            }
        }
    }

    if (d_y_link_mode == 1 || d_y_link_mode == 2) {
        const int srcAxis = parentPlot->axisEnabled(QwtPlot::yLeft) ? QwtPlot::yLeft
                                                                    : QwtPlot::yRight;
        const QwtScaleDiv *pDiv = parentPlot->axisScaleDiv(srcAxis);
        const QwtScaleEngine *pEng = parentPlot->axisScaleEngine(srcAxis);
        if (pDiv && pEng) {
            const double p1 = pDiv->lowerBound();
            const double p2 = pDiv->upperBound();
            double c1 = p1;
            double c2 = p2;
            bool ok = true;
            if (d_y_link_mode == 2) {
                bool ok1 = false, ok2 = false;
                c1 = evalEndpoint(d_y_link_expr, p1, ok1);
                c2 = evalEndpoint(d_y_link_expr, p2, ok2);
                ok = ok1 && ok2;
            }
            if (ok && qAbs(c2 - c1) > 1e-300) {
                const double lo = qMin(c1, c2);
                const double hi = qMax(c1, c2);
                const int scaleType = (d_y_link_mode == 1
                                       && pEng->transformation()->type()
                                               == QwtScaleTransformation::Log10
                                       && lo > 0.0)
                        ? 1
                        : 0;
                const bool inv = pEng->testAttribute(QwtScaleEngine::Inverted);
                const int maj = parentPlot->axisMaxMajor(srcAxis);
                const int minT = parentPlot->axisMaxMinor(srcAxis);

                auto applyAxis = [&](int targetAxis) {
                    QwtScaleEngine *se = scaleType ? static_cast<QwtScaleEngine *>(new QwtLog10ScaleEngine())
                                                   : static_cast<QwtScaleEngine *>(new QwtLinearScaleEngine());
                    int maxMinInt = minT;
                    if (minT == 1)
                        maxMinInt = 3;
                    else if (minT > 1)
                        maxMinInt = minT + 1;
                    QwtScaleDiv div = se->divideScale(lo, hi, maj, maxMinInt, 0.0);
                    if (inv) {
                        se->setAttribute(QwtScaleEngine::Inverted);
                        div.invert();
                    }
                    d_plot->setAxisMaxMajor(targetAxis, maj);
                    d_plot->setAxisMaxMinor(targetAxis, minT);
                    d_plot->setAxisScaleEngine(targetAxis, se);
                    d_plot->setAxisScaleDiv(targetAxis, div);
                };

                applyAxis(QwtPlot::yRight);
                applyAxis(QwtPlot::yLeft);
            }
        }
    }

    d_zoomer[0]->setZoomBase();
    d_zoomer[1]->setZoomBase();
    d_plot->replot();
    d_syncing_link = false;
}

void Graph::syncChildLinkedLayers()
{
    if (d_syncing_link)
        return;

    MultiLayer *ml = multiLayerParent();
    if (!ml)
        return;

    const QList<Graph *> allGraphs = ml->layersList();
    for (Graph *g : allGraphs) {
        if (g && g != this && g->linkedParentLayer() == this) {
            g->syncLinkedAxesFromParent();
        }
    }
}

// ============================================================================
// OriginPro Phase 5: Piecewise Axis Breaks, Tick Transforms, Shaded Bands,
// Non-Destructive Stack Offsets & Lossless Phase 5 Persistence
// ============================================================================

namespace {

class PiecewiseBreakScaleTransformation : public QwtScaleTransformation
{
public:
    explicit PiecewiseBreakScaleTransformation(const Graph::AxisBreakSpec &spec)
        : QwtScaleTransformation(QwtScaleTransformation::Other), d_spec(spec)
    {
    }

    PiecewiseBreakScaleTransformation(const PiecewiseBreakScaleTransformation &other)
        : QwtScaleTransformation(QwtScaleTransformation::Other), d_spec(other.d_spec)
    {
    }

    QwtScaleTransformation *copy() const override
    {
        return new PiecewiseBreakScaleTransformation(*this);
    }

    double xForm(double s, double s1, double s2, double p1, double p2) const override
    {
        const double lo = qMin(s1, s2);
        const double hi = qMax(s1, s2);
        if (hi - lo <= 1e-300)
            return p1;

        const double bf = qMin(d_spec.breakFrom, d_spec.breakTo);
        const double bt = qMax(d_spec.breakFrom, d_spec.breakTo);
        const double pos = qBound(0.10, d_spec.positionPct, 0.90);

        double u = 0.0;
        if (!d_spec.enabled || !(bf > lo && bt < hi && bt > bf + 1e-15)) {
            u = (s - lo) / (hi - lo);
        } else if (s <= bf) {
            if (d_spec.log10Before && lo > 0.0 && bf > lo) {
                const double sc = qMax(s, 1e-300);
                u = pos * (std::log10(sc) - std::log10(lo)) / (std::log10(bf) - std::log10(lo));
            } else {
                u = pos * (s - lo) / (bf - lo);
            }
        } else if (s >= bt) {
            if (d_spec.log10After && bt > 0.0 && hi > bt) {
                const double sc = qMax(s, 1e-300);
                u = pos
                        + (1.0 - pos) * (std::log10(sc) - std::log10(bt))
                                / (std::log10(hi) - std::log10(bt));
            } else {
                u = pos + (1.0 - pos) * (s - bt) / (hi - bt);
            }
        } else {
            u = pos;
        }

        const double t = (s1 <= s2) ? u : (1.0 - u);
        return p1 + t * (p2 - p1);
    }

    double invXForm(double p, double p1, double p2, double s1, double s2) const override
    {
        if (qAbs(p2 - p1) <= 1e-15)
            return s1;

        const double lo = qMin(s1, s2);
        const double hi = qMax(s1, s2);
        const double t = (p - p1) / (p2 - p1);
        const double u = (s1 <= s2) ? t : (1.0 - t);

        const double bf = qMin(d_spec.breakFrom, d_spec.breakTo);
        const double bt = qMax(d_spec.breakFrom, d_spec.breakTo);
        const double pos = qBound(0.10, d_spec.positionPct, 0.90);

        if (!d_spec.enabled || !(bf > lo && bt < hi && bt > bf + 1e-15))
            return lo + u * (hi - lo);

        if (u <= pos) {
            const double f = (pos > 1e-12) ? (u / pos) : 0.0;
            if (d_spec.log10Before && lo > 0.0 && bf > lo)
                return std::pow(10.0, std::log10(lo) + f * (std::log10(bf) - std::log10(lo)));
            return lo + f * (bf - lo);
        }

        const double f = (1.0 - pos > 1e-12) ? ((u - pos) / (1.0 - pos)) : 0.0;
        if (d_spec.log10After && bt > 0.0 && hi > bt)
            return std::pow(10.0, std::log10(bt) + f * (std::log10(hi) - std::log10(bt)));
        return bt + f * (hi - bt);
    }

private:
    Graph::AxisBreakSpec d_spec;
};

class PiecewiseBreakScaleEngine : public QwtLinearScaleEngine
{
public:
    explicit PiecewiseBreakScaleEngine(const Graph::AxisBreakSpec &spec) : d_spec(spec) {}

    QwtScaleTransformation *transformation() const override
    {
        return new PiecewiseBreakScaleTransformation(d_spec);
    }

    QwtScaleDiv divideScale(double x1, double x2, int maxMajSteps, int maxMinSteps,
                            double stepSize = 0.0) const override
    {
        const double lo = qMin(x1, x2);
        const double hi = qMax(x1, x2);
        const double bf = qMin(d_spec.breakFrom, d_spec.breakTo);
        const double bt = qMax(d_spec.breakFrom, d_spec.breakTo);
        const double pos = qBound(0.10, d_spec.positionPct, 0.90);

        if (!d_spec.enabled || !(bf > lo && bt < hi && bt > bf + 1e-15))
            return QwtLinearScaleEngine::divideScale(x1, x2, maxMajSteps, maxMinSteps, stepSize);

        const int maj1 = qMax(1, qRound(qMax(2, maxMajSteps) * pos));
        const int maj2 = qMax(1, qMax(2, maxMajSteps) - maj1);

        QwtScaleDiv div1;
        if (d_spec.log10Before && lo > 0.0) {
            QwtLog10ScaleEngine le1;
            div1 = le1.divideScale(lo, bf, maj1, maxMinSteps,
                                   d_spec.stepBefore > 0.0 ? d_spec.stepBefore : 0.0);
        } else {
            QwtLinearScaleEngine le1;
            div1 = le1.divideScale(lo, bf, maj1, maxMinSteps,
                                   d_spec.stepBefore > 0.0 ? d_spec.stepBefore : stepSize);
        }

        QwtScaleDiv div2;
        if (d_spec.log10After && bt > 0.0) {
            QwtLog10ScaleEngine le2;
            div2 = le2.divideScale(bt, hi, maj2, maxMinSteps,
                                   d_spec.stepAfter > 0.0 ? d_spec.stepAfter : 0.0);
        } else {
            QwtLinearScaleEngine le2;
            div2 = le2.divideScale(bt, hi, maj2, maxMinSteps,
                                   d_spec.stepAfter > 0.0 ? d_spec.stepAfter : stepSize);
        }

        const double guard1 = 0.04 * (bf - lo);
        const double guard2 = 0.04 * (hi - bt);
        QwtValueList mergedTicks[QwtScaleDiv::NTickTypes];
        for (int tt = 0; tt < QwtScaleDiv::NTickTypes; ++tt) {
            const QwtScaleDiv::TickType tickType = static_cast<QwtScaleDiv::TickType>(tt);
            for (double v : div1.ticks(tickType)) {
                if (v >= lo - 1e-12 * (bf - lo) && v <= bf - guard1)
                    mergedTicks[tt].append(v);
            }
            for (double v : div2.ticks(tickType)) {
                if (v >= bt + guard2 && v <= hi + 1e-12 * (hi - bt))
                    mergedTicks[tt].append(v);
            }
        }

        QwtScaleDiv out(lo, hi, mergedTicks);
        if (x1 > x2)
            out.invert();
        return out;
    }

private:
    Graph::AxisBreakSpec d_spec;
};

class ShadedBandPlotItem : public QwtPlotItem
{
public:
    ShadedBandPlotItem(const QVector<double> &x, const QVector<double> &yLow,
                       const QVector<double> &yHigh, const QColor &fillColor,
                       const QString &bandTitle)
        : d_x(x), d_yLow(yLow), d_yHigh(yHigh), d_color(fillColor)
    {
        setZ(15.0);
        setTitle(bandTitle);
        setItemAttribute(QwtPlotItem::AutoScale, true);
        setItemAttribute(QwtPlotItem::Legend, !bandTitle.trimmed().isEmpty());
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 43; }

    QwtDoubleRect boundingRect() const override
    {
        const int n = qMin(d_x.size(), qMin(d_yLow.size(), d_yHigh.size()));
        if (n <= 0)
            return QwtDoubleRect(0.0, 0.0, -1.0, -1.0);
        double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;
        bool first = true;
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(d_x[i]) || !std::isfinite(d_yLow[i]) || !std::isfinite(d_yHigh[i]))
                continue;
            const double lo = qMin(d_yLow[i], d_yHigh[i]);
            const double hi = qMax(d_yLow[i], d_yHigh[i]);
            if (first) {
                minX = maxX = d_x[i];
                minY = lo;
                maxY = hi;
                first = false;
            } else {
                minX = qMin(minX, d_x[i]);
                maxX = qMax(maxX, d_x[i]);
                minY = qMin(minY, lo);
                maxY = qMax(maxY, hi);
            }
        }
        if (first)
            return QwtDoubleRect(0.0, 0.0, -1.0, -1.0);
        return QwtDoubleRect(minX, minY, qMax(1e-15, maxX - minX), qMax(1e-15, maxY - minY));
    }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &) const override
    {
        if (!painter)
            return;
        const int n = qMin(d_x.size(), qMin(d_yLow.size(), d_yHigh.size()));
        if (n < 2)
            return;

        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(d_color));

        int i = 0;
        while (i < n) {
            while (i < n
                   && (!std::isfinite(d_x[i]) || !std::isfinite(d_yLow[i])
                       || !std::isfinite(d_yHigh[i]))) {
                ++i;
            }
            const int segStart = i;
            while (i < n && std::isfinite(d_x[i]) && std::isfinite(d_yLow[i])
                   && std::isfinite(d_yHigh[i])) {
                ++i;
            }
            const int segEnd = i - 1;
            if (segEnd - segStart >= 1) {
                QPolygonF poly;
                poly.reserve(2 * (segEnd - segStart + 1));
                for (int k = segStart; k <= segEnd; ++k)
                    poly.append(QPointF(xMap.xTransform(d_x[k]), yMap.xTransform(d_yHigh[k])));
                for (int k = segEnd; k >= segStart; --k)
                    poly.append(QPointF(xMap.xTransform(d_x[k]), yMap.xTransform(d_yLow[k])));
                painter->drawPolygon(poly);
            }
        }
        painter->restore();
    }

private:
    QVector<double> d_x;
    QVector<double> d_yLow;
    QVector<double> d_yHigh;
    QColor d_color;
};

QString unescapePhase5Attr(const QString &enc)
{
    QString out;
    out.reserve(enc.size());
    int i = 0;
    const int n = enc.size();
    while (i < n) {
        if (enc[i] == '&') {
            const int semi = enc.indexOf(';', i + 1);
            if (semi > i + 1 && semi - i <= 10) {
                const QString ent = enc.mid(i + 1, semi - i - 1);
                if (ent == "amp") {
                    out += '&';
                    i = semi + 1;
                    continue;
                } else if (ent == "quot") {
                    out += '"';
                    i = semi + 1;
                    continue;
                } else if (ent == "apos") {
                    out += '\'';
                    i = semi + 1;
                    continue;
                } else if (ent == "lt") {
                    out += '<';
                    i = semi + 1;
                    continue;
                } else if (ent == "gt") {
                    out += '>';
                    i = semi + 1;
                    continue;
                } else if (ent.startsWith("#x") || ent.startsWith("#X")) {
                    bool ok = false;
                    const uint code = ent.mid(2).toUInt(&ok, 16);
                    if (ok && code <= 0xFFFF) {
                        out += QChar(ushort(code));
                        i = semi + 1;
                        continue;
                    }
                } else if (ent.startsWith('#')) {
                    bool ok = false;
                    const uint code = ent.mid(1).toUInt(&ok, 10);
                    if (ok && code <= 0xFFFF) {
                        out += QChar(ushort(code));
                        i = semi + 1;
                        continue;
                    }
                }
            }
        }
        out += enc[i];
        ++i;
    }
    return out;
}

QMap<QString, QString> parsePhase5XmlAttrs(const QString &line)
{
    QMap<QString, QString> attrs;
    QRegExp rx("([A-Za-z0-9_]+)\\s*=\\s*\"([^\"]*)\"");
    int pos = 0;
    while ((pos = rx.indexIn(line, pos)) != -1) {
        attrs.insert(rx.cap(1), unescapePhase5Attr(rx.cap(2)));
        pos += rx.matchedLength();
    }
    return attrs;
}

} // namespace

void Graph::setAxisBreak(int axis, const AxisBreakSpec &spec)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    AxisBreakSpec norm = spec;
    if (norm.breakFrom > norm.breakTo)
        std::swap(norm.breakFrom, norm.breakTo);
    norm.positionPct = qBound(0.10, norm.positionPct, 0.90);
    d_axis_breaks[axis] = norm;

    if (!norm.enabled) {
        clearAxisBreak(axis);
        return;
    }

    const QwtScaleDiv *oldDiv = d_plot->axisScaleDiv(axis);
    const QwtScaleEngine *oldEng = d_plot->axisScaleEngine(axis);
    const double lo = oldDiv ? qMin(oldDiv->lowerBound(), oldDiv->upperBound()) : 0.0;
    const double hi = oldDiv ? qMax(oldDiv->lowerBound(), oldDiv->upperBound()) : 100.0;
    const bool inv = oldEng ? oldEng->testAttribute(QwtScaleEngine::Inverted) : false;
    const int maj = d_plot->axisMaxMajor(axis);
    const int minT = d_plot->axisMaxMinor(axis);
    int maxMinInt = minT;
    if (minT == 1)
        maxMinInt = 3;
    else if (minT > 1)
        maxMinInt = minT + 1;

    PiecewiseBreakScaleEngine *eng = new PiecewiseBreakScaleEngine(norm);
    QwtScaleDiv div = eng->divideScale(lo, hi, maj, maxMinInt, d_user_step[axis]);
    if (inv) {
        eng->setAttribute(QwtScaleEngine::Inverted);
        div.invert();
    }

    d_plot->setAxisScaleEngine(axis, eng);
    d_plot->setAxisScaleDiv(axis, div);

    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis)))
        sd->setAxisBreakIndicator(true, norm.breakFrom);

    if (axis == QwtPlot::xBottom)
        updateSecondaryAxis(QwtPlot::xTop);
    else if (axis == QwtPlot::yLeft)
        updateSecondaryAxis(QwtPlot::yRight);

    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

Graph::AxisBreakSpec Graph::axisBreak(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return AxisBreakSpec();
    return d_axis_breaks[axis];
}

void Graph::clearAxisBreak(int axis)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    d_axis_breaks[axis].enabled = false;
    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis)))
        sd->setAxisBreakIndicator(false, 0.0);

    const QwtScaleDiv *oldDiv = d_plot->axisScaleDiv(axis);
    const QwtScaleEngine *oldEng = d_plot->axisScaleEngine(axis);
    const double lo = oldDiv ? qMin(oldDiv->lowerBound(), oldDiv->upperBound()) : 0.0;
    const double hi = oldDiv ? qMax(oldDiv->lowerBound(), oldDiv->upperBound()) : 100.0;
    const bool inv = oldEng ? oldEng->testAttribute(QwtScaleEngine::Inverted) : false;

    setScale(axis, lo, hi, d_user_step[axis], d_plot->axisMaxMajor(axis),
             d_plot->axisMaxMinor(axis), 0, inv);

    const int mirrorAxis = (axis == QwtPlot::xBottom)
            ? QwtPlot::xTop
            : ((axis == QwtPlot::yLeft) ? QwtPlot::yRight : -1);
    if (mirrorAxis >= 0 && !d_axis_breaks[mirrorAxis].enabled) {
        if (ScaleDraw *msd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(mirrorAxis)))
            msd->setAxisBreakIndicator(false, 0.0);
    }

    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTickLabelTransform(int axis, double divideByFactor, const QString &prefix,
                                  const QString &suffix)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    if (!std::isfinite(divideByFactor) || qAbs(divideByFactor) <= 1e-300)
        divideByFactor = 1.0;

    d_tick_divide_factor[axis] = divideByFactor;
    d_tick_prefix[axis] = prefix;
    d_tick_suffix[axis] = suffix;

    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
        sd->setTickLabelTransform(divideByFactor, prefix, suffix);
    }
    d_plot->updateLayout();
    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

double Graph::tickDivideFactor(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return 1.0;
    return d_tick_divide_factor[axis];
}

QString Graph::tickPrefix(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    return d_tick_prefix[axis];
}

QString Graph::tickSuffix(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    return d_tick_suffix[axis];
}

QwtPlotItem *Graph::addShadedBand(const QVector<double> &x, const QVector<double> &yLow,
                                  const QVector<double> &yHigh, const QColor &fillColor,
                                  const QString &title)
{
    if (!d_plot)
        return nullptr;
    const int n = qMin(x.size(), qMin(yLow.size(), yHigh.size()));
    if (n < 2)
        return nullptr;

    ShadedBandPlotItem *item = new ShadedBandPlotItem(x, yLow, yHigh, fillColor, title);
    item->attach(d_plot);

    ShadedBandSpec spec;
    spec.x = x;
    spec.yLow = yLow;
    spec.yHigh = yHigh;
    spec.fillColor = fillColor;
    spec.title = title;
    spec.item = item;
    d_shaded_bands.append(spec);

    d_plot->replot();
    emit modifiedGraph();
    return item;
}

void Graph::setStackOffsets(int mode, double yGapPct, double xGapPct, bool normalize01)
{
    QList<PlotCurve *> targetCurves;
    for (int i = 0; i < n_curves; ++i) {
        if (c_type[i] == ErrorBars || c_type[i] == Pie || c_type[i] == Box
            || c_type[i] == ColorMap || c_type[i] == GrayMap || c_type[i] == ContourMap)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(i));
        if (pc && pc->isVisible() && pc->dataSize() > 0)
            targetCurves.append(pc);
    }
    if (targetCurves.isEmpty())
        return;

    // First reset to clean raw unstacked values (with optional 0..1 normalization)
    for (PlotCurve *pc : targetCurves) {
        pc->setStackOffset(0.0, 0.0);
        pc->setStackNormalize01(normalize01);
    }

    if (targetCurves.size() < 2) {
        if (m_autoscale) {
            for (int a = 0; a < QwtPlot::axisCnt; ++a)
                d_plot->setAxisAutoScale(a);
        }
        updatePlot();
        emit modifiedGraph();
        return;
    }

    const int m = targetCurves.size();
    QVector<double> minY(m, 0.0), maxY(m, 0.0), spanY(m, 0.0);
    double sumSpanY = 0.0;
    int validSpansY = 0;
    double maxSpanX = 0.0;

    for (int k = 0; k < m; ++k) {
        PlotCurve *pc = targetCurves[k];
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        const int n = pc->dataSize();
        bool first = true;
        double loX = 0.0, hiX = 0.0, loY = 0.0, hiY = 0.0;
        for (int j = 0; j < n; ++j) {
            if (dc && dc->isPointMasked(j))
                continue;
            const double xv = pc->x(j);
            const double yv = pc->y(j);
            if (!std::isfinite(xv) || !std::isfinite(yv))
                continue;
            if (first) {
                loX = hiX = xv;
                loY = hiY = yv;
                first = false;
            } else {
                loX = qMin(loX, xv);
                hiX = qMax(hiX, xv);
                loY = qMin(loY, yv);
                hiY = qMax(hiY, yv);
            }
        }
        minY[k] = loY;
        maxY[k] = hiY;
        spanY[k] = qMax(0.0, hiY - loY);
        if (!first) {
            sumSpanY += spanY[k];
            validSpansY++;
            maxSpanX = qMax(maxSpanX, hiX - loX);
        }
    }

    const double avgSpanY =
            (validSpansY > 0 && sumSpanY > 1e-12) ? (sumSpanY / double(validSpansY)) : 1.0;
    const double refSpanX = (maxSpanX > 1e-12) ? maxSpanX : 1.0;
    const double stepX =
            (qAbs(xGapPct) > 1e-12) ? (refSpanX * (xGapPct / 100.0)) : 0.0;

    double prevTop = maxY[0];
    for (int k = 1; k < m; ++k) {
        PlotCurve *pc = targetCurves[k];
        const double dx = double(k) * stepX;
        double dy = 0.0;
        if (mode == 1) {
            // AutoGap: each curve sits cleanly above the previous curve with yGapPct% margin
            dy = (prevTop - minY[k]) + (yGapPct / 100.0) * avgSpanY;
            prevTop = maxY[k] + dy;
        } else {
            // Uniform: constant vertical step across all curves
            if (qAbs(yGapPct) <= 1e-12 && qAbs(xGapPct) > 1e-12)
                dy = 0.0;
            else
                dy = double(k) * avgSpanY * (1.0 + yGapPct / 100.0);
        }
        pc->setStackOffset(dx, dy);

        if (DataCurve *dc = dynamic_cast<DataCurve *>(pc)) {
            const int n = dc->dataSize();
            QVector<double> xv(n), yv(n);
            for (int j = 0; j < n; ++j) {
                xv[j] = dc->x(j);
                yv[j] = dc->y(j);
            }
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->setData(xv.constData(), yv.constData(), n);
            }
        }
    }

    for (int a = 0; a < QwtPlot::axisCnt; ++a)
        d_plot->setAxisAutoScale(a);
    updatePlot();
    emit modifiedGraph();
}

void Graph::resetStackOffsets()
{
    bool changed = false;
    for (int i = 0; i < n_curves; ++i) {
        PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(i));
        if (!pc)
            continue;
        if (qAbs(pc->stackOffsetX()) > 1e-15 || qAbs(pc->stackOffsetY()) > 1e-15
            || pc->stackNormalize01()) {
            pc->setStackNormalize01(false);
            pc->setStackOffset(0.0, 0.0);
            changed = true;
            if (DataCurve *dc = dynamic_cast<DataCurve *>(pc)) {
                const int n = dc->dataSize();
                QVector<double> xv(n), yv(n);
                for (int j = 0; j < n; ++j) {
                    xv[j] = dc->x(j);
                    yv[j] = dc->y(j);
                }
                for (DataCurve *eb : dc->errorBarsList()) {
                    if (eb)
                        eb->setData(xv.constData(), yv.constData(), n);
                }
            }
        }
    }
    if (changed) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            d_plot->setAxisAutoScale(a);
        updatePlot();
        emit modifiedGraph();
    }
}

bool Graph::restorePhase5Tag(const QString &line)
{
    const QString s = line.trimmed();
    if (!s.startsWith("<Tori"))
        return false;

    const QMap<QString, QString> attrs = parsePhase5XmlAttrs(s);

    if (s.startsWith("<ToriBackbones")) {
        const bool anyDraw = attrs.value("draw", "1").toInt() != 0;
        drawAxesBackbone = anyDraw;
        for (int a = 0; a < 4; ++a) {
            const QString key = QString("b%1").arg(a);
            if (attrs.contains(key)) {
                const bool vis = anyDraw && (attrs.value(key).toInt() != 0);
                d_axis_backbone[a] = vis;
                if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(a)))
                    sd->enableComponent(QwtAbstractScaleDraw::Backbone, vis);
            }
        }
        if (attrs.contains("legendCols")) {
            const int cols = qMax(1, attrs.value("legendCols").toInt());
            if (Legend *leg = legend())
                leg->setNumColumns(cols);
        }
        const QString textCols = attrs.value("textCols");
        if (!textCols.isEmpty()) {
            for (const QString &pair : textCols.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = pair.split(':');
                if (kv.size() == 2) {
                    const int ti = kv[0].toInt();
                    const int cols = qMax(1, kv[1].toInt());
                    if (ti >= 0 && ti < d_texts.size()) {
                        if (Legend *tm = dynamic_cast<Legend *>(d_plot->marker(d_texts[ti])))
                            tm->setNumColumns(cols);
                    }
                }
            }
        }
        const QString curveAlphas = attrs.value("curveAlphas");
        if (!curveAlphas.isEmpty()) {
            for (const QString &entry : curveAlphas.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = entry.split(':');
                if (kv.size() == 2) {
                    const int ci = kv[0].toInt();
                    const QStringList rgba = kv[1].split(',');
                    QwtPlotCurve *cv = curve(ci);
                    if (cv && rgba.size() == 4) {
                        QPen p = cv->pen();
                        QColor lc = p.color();
                        lc.setAlpha(qBound(0, rgba[0].toInt(), 255));
                        p.setColor(lc);
                        cv->setPen(p);

                        QBrush br = cv->brush();
                        QColor bc = br.color();
                        bc.setAlpha(qBound(0, rgba[1].toInt(), 255));
                        br.setColor(bc);
                        cv->setBrush(br);

                        QwtSymbol sym = cv->symbol();
                        QPen sp = sym.pen();
                        QColor spc = sp.color();
                        spc.setAlpha(qBound(0, rgba[2].toInt(), 255));
                        sp.setColor(spc);
                        sym.setPen(sp);
                        QBrush sb = sym.brush();
                        if (sb.style() != Qt::NoBrush) {
                            QColor sbc = sb.color();
                            sbc.setAlpha(qBound(0, rgba[3].toInt(), 255));
                            sb.setColor(sbc);
                            sym.setBrush(sb);
                        }
                        cv->setSymbol(sym);
                    }
                }
            }
        }
        const QString stackOffsets = attrs.value("stackOffsets");
        if (!stackOffsets.isEmpty()) {
            for (const QString &entry : stackOffsets.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = entry.split(':');
                if (kv.size() == 2) {
                    const int ci = kv[0].toInt();
                    const QStringList vals = kv[1].split(',');
                    PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(ci));
                    if (pc && vals.size() >= 3) {
                        pc->setStackNormalize01(vals[2].toInt() != 0);
                        pc->setStackOffset(vals[0].toDouble(), vals[1].toDouble());
                    }
                }
            }
        }
        for (QwtPlotItem *it : d_plot->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                Spectrogram *sp = static_cast<Spectrogram *>(it);
                sp->syncColorScale();
                sp->enforceColorBarArialFont();
            }
        }
        d_plot->replot();
        return true;
    }

    if (s.startsWith("<ToriAutoAxisTitles")) {
        const QString uf = attrs.value("unitFormat");
        d_axis_unit_format = (uf == QStringLiteral("ISIJSlash")) ? Column::UnitFormat::ISIJSlash
                                                                 : Column::UnitFormat::Paren;
        for (int a = 0; a < 4; ++a) {
            const QString key = QString("a%1").arg(a);
            if (attrs.contains(key))
                d_axis_title_auto[a] = (attrs.value(key).toInt() != 0);
        }
        if (curves() > 0)
            refreshAutoAxisTitlesFromColumns();
        return true;
    }

    if (s.startsWith("<ToriRefLine") || s.startsWith("<ToriShadedBand")) {
        if (s.startsWith("<ToriShadedBand") || attrs.value("kind") == "band") {
            const QStringList xs = attrs.value("x").split(',', Qt::SkipEmptyParts);
            const QStringList yls = attrs.value("yLow").split(',', Qt::SkipEmptyParts);
            const QStringList yhs = attrs.value("yHigh").split(',', Qt::SkipEmptyParts);
            const int m = qMin(xs.size(), qMin(yls.size(), yhs.size()));
            if (m >= 2) {
                QVector<double> xv(m), ylv(m), yhv(m);
                for (int k = 0; k < m; ++k) {
                    xv[k] = xs[k].toDouble();
                    ylv[k] = yls[k].toDouble();
                    yhv[k] = yhs[k].toDouble();
                }
                QColor col(attrs.value("color", "#371e66b8"));
                if (!col.isValid())
                    col = QColor(30, 102, 184, 55);
                addShadedBand(xv, ylv, yhv, col, attrs.value("title"));
            }
            return true;
        }

        ReferenceLineSpec rl;
        rl.orientation = attrs.value("orient", "0").toInt();
        rl.value = attrs.value("value", "0").toDouble();
        rl.statMode = attrs.value("stat");
        const QColor col(attrs.value("color", "#ff1e66b8"));
        if (col.isValid())
            rl.color = col;
        rl.style = static_cast<Qt::PenStyle>(attrs.value("style", "2").toInt());
        rl.width = attrs.value("width", "1.5").toDouble();
        rl.label = attrs.value("label");
        rl.fillToAxis = attrs.value("fill", "0").toInt() != 0;
        const QColor fcol(attrs.value("fillColor", "#201e66b8"));
        if (fcol.isValid())
            rl.fillColor = fcol;
        addReferenceLine(rl);
        return true;
    }

    if (s.startsWith("<ToriLinkedLayer")) {
        d_pending_linked_parent_idx = attrs.value("parentIdx", "-1").toInt();
        d_x_link_mode = attrs.value("xMode", "0").toInt();
        d_x_link_expr = attrs.value("xExpr", "x");
        d_y_link_mode = attrs.value("yMode", "0").toInt();
        d_y_link_expr = attrs.value("yExpr", "y");
        return true;
    }

    if (s.startsWith("<ToriAxisBreak")) {
        const int axis = attrs.value("axis", "-1").toInt();
        if (axis >= 0 && axis < 4) {
            AxisBreakSpec ab;
            ab.enabled = attrs.value("enabled", "1").toInt() != 0;
            ab.breakFrom = attrs.value("from", "0").toDouble();
            ab.breakTo = attrs.value("to", "0").toDouble();
            ab.positionPct = attrs.value("pos", "0.5").toDouble();
            ab.stepBefore = attrs.value("stepBefore", "0").toDouble();
            ab.stepAfter = attrs.value("stepAfter", "0").toDouble();
            ab.log10Before = attrs.value("logBefore", "0").toInt() != 0;
            ab.log10After = attrs.value("logAfter", "0").toInt() != 0;
            setAxisBreak(axis, ab);
        }
        return true;
    }

    if (s.startsWith("<ToriTickTransform")) {
        const int axis = attrs.value("axis", "-1").toInt();
        if (axis >= 0 && axis < 4) {
            setTickLabelTransform(axis, attrs.value("factor", "1").toDouble(),
                                  attrs.value("prefix"), attrs.value("suffix"));
        }
        return true;
    }

    if (s.startsWith("<ToriTernary")) {
        d_is_ternary = attrs.value("enabled", "1").toInt() != 0;
        if (attrs.contains("a"))
            d_ternary_labels[0] = attrs.value("a");
        if (attrs.contains("b"))
            d_ternary_labels[1] = attrs.value("b");
        if (attrs.contains("c"))
            d_ternary_labels[2] = attrs.value("c");
        if (d_is_ternary)
            m_autoscale = false;
        return true;
    }

    return false;
}



