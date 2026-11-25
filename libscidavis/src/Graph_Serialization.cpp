#include "OleServer.h"
#include "MultiLayer.h"
/***************************************************************************
    File                 : Graph_Serialization.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Graph XML serialization, layer copy, and Phase 5 tag restoration

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
#include "ExtraPlotTypes.h"
#include "Grid.h"
#include "CanvasPicker.h"
#include "CanvasInlineTextEditor.h"
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
#include "DataPickerTool.h"
#include "RangeSelectorTool.h"
#include "PlotCurve.h"
#include "ApplicationWindow.h"
#include "I18nJapanese.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "core/Project.h"
#include "core/column/Column.h"
#include "MultiLayer.h"
#include "EmfPaintDevice.h"
#include "MyParser.h"
#include <algorithm>
#include <limits>
#include <mutex>

#include <QApplication>
#include <QFontMetricsF>
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
#include <QRegularExpression>
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
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QMainWindow>
#include <QStatusBar>
#include <QTextDocument>
#include <QUndoCommand>
#include <QUndoStack>
#include <utility>

#if QT_VERSION >= 0x040300
#include <QSvgGenerator>
#endif

#include <qwt_painter.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_zoomer.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_text.h>
#include <qwt_text_label.h>
#include <qwt_color_map.h>

#include <cmath>
#include <stdexcept>

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
using namespace std;

#include "Graph_Internal.h"
using namespace GraphInternal;

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
            s += QString::number(readAxisBaselineDist(scale)) + "\t";
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

QString Graph::saveEnabledAxes()
{
    QString list = "EnabledAxes\t";
    for (int i = 0; i < QwtPlot::axisCnt; i++)
        list += QString::number(d_plot->axisEnabled(i)) + "\t";

    list += "\n";
    return list;
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
    // Exact size after the legacy integer field (12.9 pt presets); older readers ignore it.
    list << QString::number(f.pointSizeF() > 0.0 ? f.pointSizeF() : 0.0, 'g', 6);
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
        list[7] = QString::number(f.pointSizeF() > 0.0 ? f.pointSizeF() : 0.0, 'g', 6);
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
        list[7] = QString::number(f.pointSizeF() > 0.0 ? f.pointSizeF() : 0.0, 'g', 6);
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
        const int savedScaleType = Plot::isLog10ScaleEngine(sc_eng) ? 1 : 0;
        s += QString::number(savedScaleType) + "\t";
        s += QString::number(sc_eng ? sc_eng->testAttribute(QwtScaleEngine::Inverted) : false)
                + "\n";
    }
    return s;
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

namespace {
QString sanitizeProjectLineField(const QString &raw, bool newlineToBr)
{
    QString out = raw;
    out.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    out.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (newlineToBr)
        out.replace(QLatin1Char('\n'), QLatin1String("<br>"));
    out.replace(QLatin1Char('\t'), QLatin1String("&#9;"));
    static const char *const kProtectedTags[] = {
        "<Tori",       "</graph>",  "<graph>",    "</multiLayer>", "<multiLayer>",
        "</text>",     "<text>",    "</legend>",  "<legend>",      "</line>",
        "</image>",    "</curve>",  "</formula>"
    };
    for (const char *tag : kProtectedTags) {
        const QString qtag = QString::fromLatin1(tag);
        if (out.contains(qtag, Qt::CaseInsensitive)) {
            QRegularExpression rx(QRegularExpression::escape(qtag),
                                  QRegularExpression::CaseInsensitiveOption);
            out.replace(rx, QStringLiteral("&#60;") + qtag.mid(1));
        }
    }
    return out;
}
} // namespace

QString Graph::saveTitle()
{
    QString s = "PlotTitle\t";
    const QString escTitle = sanitizeProjectLineField(d_plot->title().text(), true);
    s += escTitle + "\t";
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
        const QString title = axisTitle(a);
        if (!title.isEmpty())
            s += sanitizeProjectLineField(title, true) + "\t";
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
        const int savedLw = (c->pen().widthF() > 0.0) ? qMax(1, qRound(c->pen().widthF()))
                                                      : c->pen().width();
        s += QString::number(savedLw) + "\t";

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
        if (style <= LineSymbols || style == Box) {
            const int savedSw = (symbol.pen().widthF() > 0.0)
                    ? qMax(1, qRound(symbol.pen().widthF()))
                    : symbol.pen().width();
            s += QString::number(savedSw) + "\t";
        }
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
        // Bin width/range in the shortest exact round-trip form (default is 6 digits).
        s += QString::number(h->binSize(), 'g', QLocale::FloatingPointShortest) + "\t";
        s += QString::number(h->begin(), 'g', QLocale::FloatingPointShortest) + "\t";
        s += QString::number(h->end(), 'g', QLocale::FloatingPointShortest) + "\t";
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

            PlotCurve *c = dynamic_cast<PlotCurve *>(it);
            if (!c)
                continue;
            if (c->type() != ErrorBars) {
                FunctionCurve *fc = (c->type() == Function) ? dynamic_cast<FunctionCurve *>(c) : nullptr;
                DataCurve *dc = dynamic_cast<DataCurve *>(c);
                if (fc) {
                    s += "FunctionCurve\t";
                    s += QString::number(fc->functionType()) + ",";
                    s += c->title().text() + ",";
                    s += fc->variable() + ",";
                    s += QString::number(fc->startRange(), 'g', 15) + ",";
                    s += QString::number(fc->endRange(), 'g', 15) + "\t";
                    s += QString::number(fc->dataSize()) + "\t\t\t";
                    // the 2 last tabs are legacy code, kept for compatibility with old project
                    // files
                } else if (c->type() == Box) {
                    s += "curve\t" + QString::number(c->x(0)) + "\t" + c->title().text() + "\t";
                } else if (dc) {
                    s += "curve\t" + dc->xColumnName() + "\t" + c->title().text() + "\t";
                } else {
                    continue;
                }

                s += saveCurveLayout(i);
                s += QString::number(c->xAxis()) + "\t" + QString::number(c->yAxis()) + "\t";
                const int startR = dc ? dc->startRow() : 0;
                const int endR = dc ? dc->endRow() : qMax(0, c->dataSize() - 1);
                s += QString::number(startR) + "\t" + QString::number(endR) + "\t";
                s += QString::number(c->isVisible()) + "\n";
                if (fc) {
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
                QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(it);
                if (!er || !er->masterCurve())
                    continue;
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
                s += QString::number(er->minusSide()) + "\t";
                s += QString::number(er->isVisible() ? 1 : 0) + "\n";
            }
        }
    }
    return s;
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

        s += QString::number(mrkL->widthF(), 'g', 15) + "\t";
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
        QString normMrk = mrk->text();
        normMrk.replace(QLatin1String("\r\n"), QLatin1String("\n"));
        normMrk.replace(QLatin1Char('\r'), QLatin1Char('\n'));
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList textList = normMrk.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
#else
        QStringList textList = normMrk.split(QLatin1Char('\n'), QString::KeepEmptyParts);
#endif
        for (QString &ln : textList)
            ln = sanitizeProjectLineField(ln, false);
        s += textList.join("\t");
        if (d_texts[i] != legendMarkerID)
            s += "</text>\n";
        else
            s += "</legend>\n";
    }
    return s;
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
        QStringList textPtParts; // exact pt of legend / text fonts that are not whole numbers
        for (int ti = 0; ti < d_texts.size(); ++ti) {
            Legend *tm = dynamic_cast<Legend *>(d_plot->marker(d_texts[ti]));
            if (tm && tm->numColumns() > 1)
                textColsParts << QString("%1:%2").arg(ti).arg(tm->numColumns());
            if (tm) {
                const double tpt = tm->font().pointSizeF();
                if (tpt > 0.0 && std::fabs(tpt - std::round(tpt)) > 1e-9)
                    textPtParts << QString("%1:%2").arg(ti).arg(QString::number(tpt, 'g', 6));
            }
        }

        QStringList alphaParts;
        QStringList widthParts;
        QStringList stackParts;
        for (int ci = 0; ci < n_curves; ++ci) {
            QwtPlotCurve *cv = curve(ci);
            if (!cv)
                continue;
            const double lw = cv->pen().widthF();
            const double sw = cv->symbol().pen().widthF();
            if (lw > 0.0 || sw > 0.0) {
                const QString titleB64 =
                        QString::fromLatin1(cv->title().text().toUtf8().toBase64());
                widthParts << QString("%1:%2,%3,%4")
                                      .arg(ci)
                                      .arg(QString::number(lw, 'g', 8))
                                      .arg(QString::number(sw, 'g', 8))
                                      .arg(titleB64);
            }
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

        QString legAnchorAttr =
                (d_plot && d_plot->property("_toriLegendAnchor").isValid())
                ? QString(" legendAnchor=\"%1\"").arg(d_plot->property("_toriLegendAnchor").toInt())
                : QString();
        // <legend>/<text> lines keep the integer pt for older readers; the exact value rides here.
        if (!textPtParts.isEmpty())
            legAnchorAttr += QString(" textPts=\"%1\"").arg(textPtParts.join(";"));
        // <curve> lines keep integer line/symbol widths for older readers; exact fractional px ride here.
        if (!widthParts.isEmpty())
            legAnchorAttr += QString(" curveWidths=\"%1\"").arg(widthParts.join(";"));

        s += QString("<ToriBackbones draw=\"%1\" b0=\"%2\" b1=\"%3\" b2=\"%4\" b3=\"%5\" "
                     "legendCols=\"%6\" legendVisible=\"%7\" outsideRightMargin=\"%8\" "
                     "textCols=\"%9\" curveAlphas=\"%10\" stackOffsets=\"%11\"%12/>\n")
                     .arg(drawAxesBackbone ? 1 : 0)
                     .arg(d_axis_backbone[0] ? 1 : 0)
                     .arg(d_axis_backbone[1] ? 1 : 0)
                     .arg(d_axis_backbone[2] ? 1 : 0)
                     .arg(d_axis_backbone[3] ? 1 : 0)
                     .arg(legCols)
                     .arg(d_legend_visible ? 1 : 0)
                     .arg(d_plot ? d_plot->property("_outsideRightMargin").toInt() : 0)
                     .arg(textColsParts.join(";"))
                     .arg(alphaParts.join(";"))
                     .arg(stackParts.join(";"))
                     .arg(legAnchorAttr);

        if (d_plot) {
            QStringList shiftAttrs;
            const QPointF ptShift = d_plot->property("_toriPlotTitleShift").toPointF();
            if (std::fabs(ptShift.x()) > 1e-6 || std::fabs(ptShift.y()) > 1e-6) {
                shiftAttrs << QString("titleDx=\"%1\"").arg(QString::number(ptShift.x(), 'g', 17));
                shiftAttrs << QString("titleDy=\"%1\"").arg(QString::number(ptShift.y(), 'g', 17));
            }
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                if (const QwtScaleWidget *sw = d_plot->axisWidget(a)) {
                    const QPointF atShift = sw->property("_toriAxisTitleShift").toPointF();
                    if (std::fabs(atShift.x()) > 1e-6 || std::fabs(atShift.y()) > 1e-6) {
                        shiftAttrs << QString("ax%1TitleDx=\"%2\"")
                                              .arg(a)
                                              .arg(QString::number(atShift.x(), 'g', 17));
                        shiftAttrs << QString("ax%1TitleDy=\"%2\"")
                                              .arg(a)
                                              .arg(QString::number(atShift.y(), 'g', 17));
                    }
                    const double normShift = sw->property("_toriAxisNormalShift").toDouble();
                    if (std::fabs(normShift) > 1e-6) {
                        shiftAttrs << QString("ax%1Norm=\"%2\"")
                                              .arg(a)
                                              .arg(QString::number(normShift, 'g', 17));
                    }
                    const double parShift = sw->property("_toriAxisParallelShift").toDouble();
                    if (std::fabs(parShift) > 1e-6) {
                        shiftAttrs << QString("ax%1Par=\"%2\"")
                                              .arg(a)
                                              .arg(QString::number(parShift, 'g', 17));
                    }
                }
            }
            if (!shiftAttrs.isEmpty())
                s += QString("<ToriLabelShifts %1/>\n").arg(shiftAttrs.join(' '));
        }

        for (int ci = 0; ci < n_curves; ++ci) {
            if (const PlotCurve *pc = dynamic_cast<const PlotCurve *>(curve(ci))) {
                if (pc->symbolSkip() > 0) {
                    s += QString("<ToriCurveAlpha idx=\"%1\" symSkip=\"%2\"/>\n")
                                 .arg(ci)
                                 .arg(pc->symbolSkip());
                }
            }
        }

        int lockMask = 0;
        int autoMask = 0;
        for (int a = 0; a < 4; ++a) {
            if (isAxisUserLocked(a))
                lockMask |= (1 << a);
            if (d_plot && d_plot->axisAutoScale(a))
                autoMask |= (1 << a);
        }
        s += QString("<ToriAxisLocks mask=\"%1\" autoscale=\"%2\" autoMask=\"%3\"/>\n")
                     .arg(lockMask)
                     .arg(m_autoscale ? 1 : 0)
                     .arg(autoMask);

        s += QString("<ToriAutoscaleFonts val=\"%1\"/>\n").arg(autoScaleFonts ? 1 : 0);

        const QString unitFmtStr = (d_axis_unit_format == Column::UnitFormat::ISIJSlash)
                ? QStringLiteral("ISIJSlash")
                : ((d_axis_unit_format == kUnitFormatBracket) ? QStringLiteral("Bracket")
                                                              : QStringLiteral("Paren"));
        s += QString("<ToriAutoAxisTitles unitFormat=\"%1\" a0=\"%2\" a1=\"%3\" a2=\"%4\" a3=\"%5\"/>\n")
                     .arg(unitFmtStr)
                     .arg(d_axis_title_auto[0] ? 1 : 0)
                     .arg(d_axis_title_auto[1] ? 1 : 0)
                     .arg(d_axis_title_auto[2] ? 1 : 0)
                     .arg(d_axis_title_auto[3] ? 1 : 0);

        for (const ReferenceLineSpec &rl : d_ref_lines) {
            s += QString("<ToriRefLine orient=\"%1\" value=\"%2\" stat=\"%3\" color=\"%4\" "
                         "style=\"%5\" width=\"%6\" label=\"%7\" fill=\"%8\" fillColor=\"%9\"")
                         .arg(rl.orientation)
                         .arg(QString::number(rl.value, 'g', 15))
                         .arg(escAttr(rl.statMode))
                         .arg(rl.color.name(QColor::HexArgb))
                         .arg(int(rl.style))
                         .arg(QString::number(rl.width, 'g', 8))
                         .arg(escAttr(rl.label))
                         .arg(rl.fillToAxis ? 1 : 0)
                         .arg(rl.fillColor.name(QColor::HexArgb))
                 + QStringLiteral(" visible=\"%1\"/>\n").arg(rl.visible ? 1 : 0);
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

            // E11: error bands and colour-mapped scatter symbols (restored after the curves)
            // (attributes are concatenated, not chained through arg(), so a '%' in a column
            // name or title can never be re-substituted)
            auto attr = [&escAttr](const char *key, const QString &val) -> QString {
                return QStringLiteral(" ") + QLatin1String(key) + QStringLiteral("=\"")
                        + escAttr(val) + QStringLiteral("\"");
            };
            for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); ++i) {
                QwtPlotItem *pit = d_plot->plotItem(c_keys[i]);
                if (c_type[i] == ErrorBars) {
                    auto *er = dynamic_cast<QwtErrorPlotCurve *>(pit);
                    if (er && er->isBand() && er->masterCurve()) {
                        s += QStringLiteral("<ToriErrorBand")
                                + attr("x", er->masterCurve()->xColumnName())
                                + attr("y", er->masterCurve()->title().text())
                                + attr("err", er->title().text())
                                + attr("plus", QString::number(er->plusSide() ? 1 : 0))
                                + attr("minus", QString::number(er->minusSide() ? 1 : 0))
                                + attr("alpha", QString::number(er->bandAlpha()))
                                + QStringLiteral("/>\n");
                    }
                    continue;
                }
                auto *dc = dynamic_cast<DataCurve *>(pit);
                if (!dc || !dc->hasPointColorMap())
                    continue;
                const DataCurve::PointColorMap &pm = dc->pointColorMap();
                s += QStringLiteral("<ToriPointColorMap") + attr("x", dc->xColumnName())
                        + attr("y", dc->title().text()) + attr("z", pm.zColumn)
                        + attr("size", pm.sizeColumn) + attr("preset", QString::number(pm.preset))
                        + attr("reverse", QString::number(pm.reverse ? 1 : 0))
                        + attr("auto", QString::number(pm.autoRange ? 1 : 0))
                        + attr("zmin", QString::number(pm.zMin, 'g', 15))
                        + attr("zmax", QString::number(pm.zMax, 'g', 15))
                        + attr("smin", QString::number(pm.sizeMinPx, 'g', 8))
                        + attr("smax", QString::number(pm.sizeMaxPx, 'g', 8))
                        + attr("cb", QString::number(pm.showColorBar ? 1 : 0))
                        + attr("title", pm.colorBarTitle) + QStringLiteral("/>\n");
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
            const bool brkSuspended =
                    !d_axis_breaks[a].enabled && d_suspended_axis_breaks[a].enabled;
            const AxisBreakSpec &ab =
                    brkSuspended ? d_suspended_axis_breaks[a] : d_axis_breaks[a];
            if (ab.enabled) {
                s += QString("<ToriAxisBreak axis=\"%1\" enabled=\"1\" from=\"%2\" to=\"%3\" "
                             "pos=\"%4\" stepBefore=\"%5\" stepAfter=\"%6\" logBefore=\"%7\" "
                             "logAfter=\"%8\"%9/>\n")
                             .arg(a)
                             .arg(QString::number(ab.breakFrom, 'g', 15))
                             .arg(QString::number(ab.breakTo, 'g', 15))
                             .arg(QString::number(ab.positionPct, 'g', 8))
                             .arg(QString::number(ab.stepBefore, 'g', 15))
                             .arg(QString::number(ab.stepAfter, 'g', 15))
                             .arg(ab.log10Before ? 1 : 0)
                             .arg(ab.log10After ? 1 : 0)
                             .arg(brkSuspended ? QStringLiteral(" suspended=\"1\"") : QString());
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

        QStringList stackedBarIndices;
        int stackedGap = 25;
        int stackedOffset = 0;
        bool haveStackedRef = false;
        for (int bi = 0; bi < n_curves; ++bi) {
            if (const QwtBarCurve *bc = dynamic_cast<const QwtBarCurve *>(curve(bi))) {
                if (bc->isStacked()) {
                    stackedBarIndices << QString::number(bi);
                    if (!haveStackedRef) {
                        stackedGap = bc->gap();
                        stackedOffset = bc->offset();
                        haveStackedRef = true;
                    }
                }
            }
        }
        if (!stackedBarIndices.isEmpty()) {
            s += QString("<ToriStackedBars indices=\"%1\" gap=\"%2\" offset=\"%3\"/>\n")
                         .arg(stackedBarIndices.join(","))
                         .arg(stackedGap)
                         .arg(stackedOffset);
        }

        for (int a = 0; a < 4; ++a) {
            if (!d_custom_tick_positions[a].isEmpty()
                && d_custom_tick_positions[a].size() == d_custom_tick_labels[a].size()) {
                QStringList posStrs;
                posStrs.reserve(d_custom_tick_positions[a].size());
                for (double pv : d_custom_tick_positions[a])
                    posStrs << QString::number(pv, 'g', 14);
                s += QString("<ToriCategoryAxis axis=\"%1\" positions=\"%2\" labels=\"%3\" "
                             "rot=\"%4\"/>\n")
                             .arg(a)
                             .arg(posStrs.join(","))
                             .arg(escAttr(d_custom_tick_labels[a].join(QChar(0x1F))))
                             .arg(QString::number(d_custom_tick_rotation[a], 'g', 8));
            } else if (!d_custom_category_labels[a].isEmpty()) {
                s += QString("<ToriCategoryAxis axis=\"%1\" labels=\"%2\"/>\n")
                             .arg(a)
                             .arg(escAttr(d_custom_category_labels[a].join(QChar(0x1F))));
            }
            if (d_weibull_prob_axis[a]) {
                s += QString("<ToriWeibullAxis axis=\"%1\" enabled=\"1\"/>\n").arg(a);
            }
        }

        if (saveAsTemplate) {
            auto appendTemplateStyleTag = [&](int outIdx, const TemplateCurveStyle &st) {
                s += QString("<ToriTemplateCurveStyle idx=\"%1\" type=\"%2\" penStyle=\"%3\" "
                             "lineWidth=\"%4\" color=\"%5\" brushStyle=\"%6\" brushColor=\"%7\" "
                             "symbol=\"%8\" symSize=\"%9\" symWidth=\"%10\" symColor=\"%11\" "
                             "symFill=\"%12\" hollow=\"%13\" symSkip=\"%14\" barGap=\"%15\" "
                             "barOffset=\"%16\" xAxis=\"%17\" yAxis=\"%18\"/>\n")
                             .arg(outIdx)
                             .arg(st.curveType)
                             .arg(st.penStyle)
                             .arg(QString::number(st.lineWidth, 'g', 8))
                             .arg(st.lineColor.name(QColor::HexArgb))
                             .arg(st.brushStyle)
                             .arg(st.brushColor.name(QColor::HexArgb))
                             .arg(st.symbolStyle)
                             .arg(st.symbolSize)
                             .arg(QString::number(st.symbolPenWidth, 'g', 8))
                             .arg(st.symbolEdgeColor.name(QColor::HexArgb))
                             .arg(st.symbolFillColor.name(QColor::HexArgb))
                             .arg(st.symbolHollow ? 1 : 0)
                             .arg(st.symbolSkip)
                             .arg(st.barGap)
                             .arg(st.barOffset)
                             .arg(st.xAxis)
                             .arg(st.yAxis);
            };
            int emitted = 0;
            for (int ci = 0; ci < n_curves; ++ci) {
                const int ct = (ci >= 0 && ci < c_type.size()) ? c_type[ci] : int(LineSymbols);
                if (ct == ErrorBars)
                    continue;
                QwtPlotCurve *cv = curve(ci);
                if (!cv)
                    continue;
                TemplateCurveStyle st;
                st.curveType = ct;
                st.penStyle = int(cv->pen().style());
                st.lineWidth = cv->pen().widthF();
                st.lineColor = cv->pen().color();
                st.brushStyle = int(cv->brush().style());
                st.brushColor = cv->brush().color();
                const QwtSymbol sym = cv->symbol();
                st.symbolStyle = int(sym.style());
                st.symbolSize = sym.size().width();
                st.symbolPenWidth = sym.pen().widthF();
                st.symbolEdgeColor = sym.pen().color();
                st.symbolFillColor = sym.brush().color();
                st.symbolHollow = isSymbolWhiteHollow(sym);
                if (const PlotCurve *pc = dynamic_cast<const PlotCurve *>(cv))
                    st.symbolSkip = pc->symbolSkip();
                if (const QwtBarCurve *bc = dynamic_cast<const QwtBarCurve *>(cv)) {
                    st.barGap = bc->gap();
                    st.barOffset = bc->offset();
                }
                st.xAxis = cv->xAxis();
                st.yAxis = cv->yAxis();
                appendTemplateStyleTag(emitted++, st);
            }
            if (emitted == 0 && !d_template_curve_styles.isEmpty()) {
                for (int si = 0; si < d_template_curve_styles.size(); ++si)
                    appendTemplateStyleTag(si, d_template_curve_styles[si]);
            }
        }

        if (d_is_polar) {
            s += QString("<ToriPolarPlot enabled=\"1\" rmax=\"%1\"/>\n")
                         .arg(QString::number(d_polar_rmax, 'g', 15));
            if (!saveAsTemplate) {
                for (const PolarSeriesSpec &ps : d_polar_series) {
                    const int m = qMin(ps.x.size(), ps.y.size());
                    if (m <= 0)
                        continue;
                    QStringList xs, ys;
                    xs.reserve(m);
                    ys.reserve(m);
                    for (int k = 0; k < m; ++k) {
                        xs << QString::number(ps.x[k], 'g', 12);
                        ys << QString::number(ps.y[k], 'g', 12);
                    }
                    s += QString("<ToriPolarSeries label=\"%1\" color=\"%2\" width=\"%3\" "
                                 "x=\"%4\" y=\"%5\"/>\n")
                                 .arg(escAttr(ps.label))
                                 .arg(ps.color.name(QColor::HexArgb))
                                 .arg(QString::number(ps.width, 'g', 8))
                                 .arg(xs.join(","))
                                 .arg(ys.join(","));
                }
            }
        }
    }

    // E11b: 2D density / pole figure specifications (synced with live Spectrogram state)
    if (!saveAsTemplate) {
        const QStringList extraTags = ExtraPlotTypes::syncedGraphTagsForState(this);
        for (const QString &tag : extraTags)
            s += tag + "\n";
    }

    s += "</graph>\n";
    return s;
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

    QVector<int> srcToDstIdx(g->curves(), -1);
    for (int pass = 0; pass < 2; ++pass) {
        for (i = 0; i < g->curves(); i++) {
            QwtPlotItem *it = (QwtPlotItem *)g->plotItem(i);
            if (!it)
                continue;
            if (it->rtti() == QwtPlotItem::Rtti_PlotCurve) {
                PlotCurve *srcPc = dynamic_cast<PlotCurve *>(it);
                if (!srcPc)
                    continue;
                DataCurve *cv = dynamic_cast<DataCurve *>(it);
                int n = srcPc->dataSize();
                int style = srcPc->type();
                if ((pass == 0 && style == ErrorBars) || (pass == 1 && style != ErrorBars))
                    continue;

                QVector<double> x(n);
                QVector<double> y(n);
                for (int j = 0; j < n; j++) {
                    x[j] = srcPc->x(j);
                    y[j] = srcPc->y(j);
                }

                PlotCurve *c = nullptr;
                if (style == Pie && cv) {
                    c = new QwtPieCurve(cv->table(), cv->title().text(), cv->startRow(),
                                        cv->endRow());
                    ((QwtPieCurve *)c)->setRay(((QwtPieCurve *)cv)->ray());
                    ((QwtPieCurve *)c)->setFirstColor(((QwtPieCurve *)cv)->firstColor());
                } else if (style == Function || !cv) {
                    if (FunctionCurve *fcSrc = dynamic_cast<FunctionCurve *>(it)) {
                        c = new FunctionCurve(parent, srcPc->title().text());
                        static_cast<FunctionCurve *>(c)->copy(fcSrc);
                    } else {
                        c = new PlotCurve(srcPc->title().text());
                        c->setType(Function);
                    }
                } else if (style == VerticalBars || style == HorizontalBars) {
                    c = new QwtBarCurve(((QwtBarCurve *)cv)->orientation(), cv->table(),
                                        cv->xColumnName(), cv->title().text(), cv->startRow(),
                                        cv->endRow());
                    ((QwtBarCurve *)c)->copy((const QwtBarCurve *)cv);
                } else if (style == ErrorBars) {
                    QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(cv);
                    DataCurve *master_curve = er ? masterCurve(er) : nullptr;
                    if (master_curve) {
                        c = new QwtErrorPlotCurve(cv->table(), cv->title().text());
                        ((QwtErrorPlotCurve *)c)->copy(er);
                        ((QwtErrorPlotCurve *)c)->setMasterCurve(master_curve);
                    }
                } else if (style == Histogram) {
                    c = new QwtHistogram(cv->table(), cv->title().text(), cv->startRow(),
                                         cv->endRow());
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
                    if (n > 0) {
                        QwtSingleArrayData dat(x[0], y, n);
                        c->setData(dat);
                    }
                } else {
                    c = new DataCurve(cv->table(), cv->xColumnName(), cv->title().text(),
                                      cv->startRow(), cv->endRow());
                }

                if (!c)
                    continue;

                const int dstIdx = n_curves;
                c_keys.resize(++n_curves);
                c_keys[dstIdx] = d_plot->insertCurve(c);

                c_type.resize(n_curves);
                c_type[dstIdx] = (!cv && style != Function) ? int(Function) : g->curveType(i);
                srcToDstIdx[i] = dstIdx;

                if (c_type[dstIdx] != Box && c_type[dstIdx] != ErrorBars) {
                    c->setData(x.data(), y.data(), n);
                    // The copied points keep their table rows (rows with invalid cells were
                    // skipped), so error bars, masks and the data picker map them correctly.
                    if (DataCurve *dstDc = dynamic_cast<DataCurve *>(c)) {
                        if (cv && cv->rowMapping().size() == n)
                            dstDc->setRowMapping(cv->rowMapping(), cv->gapBeforeIndices());
                    }
                }

                c->setPen(srcPc->pen());
                c->setBrush(srcPc->brush());
                c->setStyle(srcPc->style());
                c->setSymbol(srcPc->symbol());

                if (srcPc->testCurveAttribute(QwtPlotCurve::Fitted))
                    c->setCurveAttribute(QwtPlotCurve::Fitted, true);
                else if (srcPc->testCurveAttribute(QwtPlotCurve::Inverted))
                    c->setCurveAttribute(QwtPlotCurve::Inverted, true);

                c->setSymbolSkip(srcPc->symbolSkip());
                if (c_type[dstIdx] != Box && c_type[dstIdx] != ErrorBars) {
                    // x / y above are the source's displayed points, i.e. its stack offset and
                    // 0-1 normalisation are already applied: take the transform state over as is
                    // instead of applying it a second time.
                    c->copyStackTransform(srcPc);
                }

                c->setAxis(srcPc->xAxis(), srcPc->yAxis());
                c->setVisible(srcPc->isVisible());
                if (cv && cv->hasPointColorMap()) {
                    if (DataCurve *dcDst = dynamic_cast<DataCurve *>(c))
                        dcDst->setPointColorMap(cv->pointColorMap());
                }

                QList<QwtPlotCurve *> lst = g->fitCurvesList();
                if (lst.contains((QwtPlotCurve *)it))
                    d_fit_curves << c;
            } else if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                if (pass != 0)
                    continue;
                Spectrogram *sp = ((Spectrogram *)it)->copy();
                const int dstIdx = n_curves;
                c_keys.resize(++n_curves);
                c_keys[dstIdx] = d_plot->insertCurve(sp);

                sp->showColorScale(((Spectrogram *)it)->colorScaleAxis(),
                                   ((Spectrogram *)it)->hasColorScale());
                sp->setColorBarWidth(((Spectrogram *)it)->colorBarWidth());
                if (!((Spectrogram *)it)->colorBarTitle().isEmpty())
                    sp->setColorBarTitle(((Spectrogram *)it)->colorBarTitle());
                sp->setVisible(it->isVisible());

                c_type.resize(n_curves);
                c_type[dstIdx] = g->curveType(i);
                srcToDstIdx[i] = dstIdx;
            }
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

        d_axis_breaks[i] = g->axisBreak(i);
        d_suspended_axis_breaks[i] = g->suspendedAxisBreak(i);
        const int primaryMirror = (i == QwtPlot::xTop)
                ? QwtPlot::xBottom
                : ((i == QwtPlot::yRight) ? QwtPlot::yLeft : -1);
        const bool useMirrorBreak = (!d_axis_breaks[i].enabled && primaryMirror >= 0
                                     && g->axisBreak(primaryMirror).enabled
                                     && !g->hasColorScaleOnAxis(i));
        const AxisBreakSpec &activeBreak =
                d_axis_breaks[i].enabled ? d_axis_breaks[i] : g->axisBreak(qMax(0, primaryMirror));

        QwtScaleEngine *sc_engine = 0;
        if (d_axis_breaks[i].enabled || useMirrorBreak)
            sc_engine = new PiecewiseBreakScaleEngine(activeBreak);
        else if (Plot::isLog10ScaleEngine(se))
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

        QwtScaleDiv div;
        if (d_axis_breaks[i].enabled || useMirrorBreak) {
            div = *sd;
            if (se->testAttribute(QwtScaleEngine::Inverted))
                sc_engine->setAttribute(QwtScaleEngine::Inverted);
        } else {
            int max_min_intervals = minorTicks;
            if (minorTicks == 1)
                max_min_intervals = 3;
            else if (minorTicks > 1)
                max_min_intervals = minorTicks + 1;
#if QWT_VERSION >= 0x050200
            div = sc_engine->divideScale(qMin(sd->lowerBound(), sd->upperBound()),
                                         qMax(sd->lowerBound(), sd->upperBound()),
                                         majorTicks, max_min_intervals, d_user_step[i]);
#else
            div = sc_engine->divideScale(qMin(sd->lBound(), sd->hBound()),
                                         qMax(sd->lBound(), sd->hBound()), majorTicks,
                                         max_min_intervals, d_user_step[i]);
#endif
            if (se->testAttribute(QwtScaleEngine::Inverted)) {
                sc_engine->setAttribute(QwtScaleEngine::Inverted);
                div.invert();
            }
        }

        d_plot->setAxisScaleEngine(i, sc_engine);
        d_plot->setAxisScaleDiv(i, div);

        if (ScaleDraw *dstSd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(i))) {
            if (d_axis_breaks[i].enabled || useMirrorBreak) {
                dstSd->setAxisBreakIndicator(true, qMin(activeBreak.breakFrom, activeBreak.breakTo),
                                             qMax(activeBreak.breakFrom, activeBreak.breakTo),
                                             activeBreak.log10Before, activeBreak.log10After);
            } else {
                dstSd->setAxisBreakIndicator(false, 0.0);
            }
        }
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

        const long dstKey = insertTextMarker(mrk);
        if (txtMrkKeys[i] == g->legendMarkerID)
            legendMarkerID = dstKey;
        if (Legend *dstMrk = textMarker(dstKey)) {
            const QString remapped = remapLegendCurveTokens(dstMrk->text(), srcToDstIdx);
            if (remapped != dstMrk->text())
                dstMrk->setText(remapped);
        }
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
    if (g->d_point_colorbar_active)
        syncPointColorBar();

    d_is_ternary = g->d_is_ternary;
    for (int k = 0; k < 3; ++k)
        d_ternary_labels[k] = g->d_ternary_labels[k];
    if (d_is_ternary)
        setupTernaryPlot(d_ternary_labels[0], d_ternary_labels[1], d_ternary_labels[2]);

    for (int bi = 0; bi < srcToDstIdx.size(); ++bi) {
        const int dstIdx = srcToDstIdx[bi];
        if (dstIdx < 0 || dstIdx >= n_curves)
            continue;
        const QwtBarCurve *srcBc = dynamic_cast<const QwtBarCurve *>(g->curve(bi));
        QwtBarCurve *dstBc = dynamic_cast<QwtBarCurve *>(curve(dstIdx));
        if (srcBc && dstBc && srcBc->isStacked()) {
            dstBc->setStacked(true);
            dstBc->setGap(srcBc->gap());
            dstBc->setOffset(0);
        }
    }

    m_autoscale = g->m_autoscale;
    for (i = 0; i < QwtPlot::axisCnt; ++i)
        setAxisUserLocked(i, g->isAxisUserLocked(i));

    for (int a = 0; a < 4; ++a) {
        if (!g->d_custom_tick_positions[a].isEmpty()) {
            setCustomTickLabels(a, g->d_custom_tick_positions[a], g->d_custom_tick_labels[a],
                                g->d_custom_tick_rotation[a]);
        } else if (!g->d_custom_category_labels[a].isEmpty()) {
            setCategoryLabels(a, g->d_custom_category_labels[a]);
        }
        if (g->d_weibull_prob_axis[a])
            setWeibullProbabilityAxis(a, true);
    }

    if (g->d_is_polar) {
        d_polar_rmax = g->d_polar_rmax;
        setupPolarPlot(QVector<double>(), QVector<double>(), QString(), true);
        d_polar_series = g->d_polar_series;
        const double rMax = qMax(1e-6, d_polar_rmax);
        const int nPolarRows = qMax(1, qMax(int(d_polar_series.size()), n_curves));
        const int extraRows = qMax(0, nPolarRows - 1);
        const double yLo = -1.44 * rMax;
        const double yHi = (1.64 + 0.36 * double(extraRows)) * rMax;
        const double halfX = 0.5 * (yHi - yLo);
        setScale(QwtPlot::xBottom, -halfX, halfX, 0.0, 5, 0, 0, false);
        setScale(QwtPlot::yLeft, yLo, yHi, 0.0, 5, 0, 0, false);
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            enableAxis(a, false);
    }

    d_axis_unit_format = g->axisUnitFormat();
    for (int a = 0; a < 4; ++a)
        d_axis_title_auto[a] = g->isAxisTitleAuto(a);
    d_template_curve_styles = g->d_template_curve_styles;

    d_legend_visible = g->d_legend_visible;
    if (Legend *leg = legend())
        leg->setVisible(d_legend_visible);
    if (g->d_plot && g->d_plot->property("_outsideRightMargin").toInt() > 0)
        d_plot->setProperty("_outsideRightMargin", g->d_plot->property("_outsideRightMargin"));
    if (g->d_plot && g->d_plot->property("_toriLegendAnchor").isValid())
        d_plot->setProperty("_toriLegendAnchor", g->d_plot->property("_toriLegendAnchor"));
    if (g->d_plot && g->d_plot->property("_toriOutsideRightGapPx").isValid())
        d_plot->setProperty("_toriOutsideRightGapPx",
                            g->d_plot->property("_toriOutsideRightGapPx"));
    if (g->d_plot && g->d_plot->property("_toriOutsideRightTopPx").isValid())
        d_plot->setProperty("_toriOutsideRightTopPx",
                            g->d_plot->property("_toriOutsideRightTopPx"));
    if (g->d_plot) {
        if (g->d_plot->property("_toriPlotTitleShift").isValid())
            d_plot->setProperty("_toriPlotTitleShift", g->d_plot->property("_toriPlotTitleShift"));
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            const QwtScaleWidget *srcSw = g->d_plot->axisWidget(a);
            QwtScaleWidget *dstSw = d_plot->axisWidget(a);
            if (!srcSw || !dstSw)
                continue;
            if (srcSw->property("_toriAxisTitleShift").isValid())
                dstSw->setProperty("_toriAxisTitleShift", srcSw->property("_toriAxisTitleShift"));
            if (srcSw->property("_toriAxisNormalShift").isValid())
                dstSw->setProperty("_toriAxisNormalShift", srcSw->property("_toriAxisNormalShift"));
            if (srcSw->property("_toriAxisParallelShift").isValid())
                dstSw->setProperty("_toriAxisParallelShift",
                                   srcSw->property("_toriAxisParallelShift"));
        }
    }

    if (g->d_plot && g->d_plot->property("_toriExtraPlotTags").isValid()) {
        for (const QString &tag : ExtraPlotTypes::syncedGraphTagsForState(g))
            ExtraPlotTypes::restoreGraphTag(this, tag);
    }

    if (!d_is_ternary && !d_is_polar && !isPoleFigurePlot() && !isPiePlot()) {
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
    }

    syncCurveZOrder();
    d_plot->replot();

    if (isPiePlot()) {
        drawCanvasFrame(false, 0);
        setFrame(0, Qt::black);
        drawAxesBackbones(false);
        if (QwtPieCurve *c = (QwtPieCurve *)curve(0))
            c->updateBoundingRect();
    }
}

// ============================================================================
// OriginPro Phase 5: Piecewise Axis Breaks, Tick Transforms, Shaded Bands,
// Non-Destructive Stack Offsets & Lossless Phase 5 Persistence
// ============================================================================

namespace {

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

bool Graph::restorePhase5Tag(const QString &line)
{
    const QString s = line.trimmed();
    if (!s.startsWith("<Tori"))
        return false;

    const QMap<QString, QString> attrs = parsePhase5XmlAttrs(s);

    if (s.startsWith("<ToriDensity2D") || s.startsWith("<ToriPoleFigure")) // E11b
        return ExtraPlotTypes::restoreGraphTag(this, s);

    if (s.startsWith("<ToriLabelShifts")) {
        if (d_plot) {
            const double titleDx = attrs.value("titleDx").toDouble();
            const double titleDy = attrs.value("titleDy").toDouble();
            if (std::fabs(titleDx) > 1e-6 || std::fabs(titleDy) > 1e-6)
                d_plot->setProperty("_toriPlotTitleShift",
                                    QPointF(qBound(-500.0, titleDx, 500.0),
                                            qBound(-500.0, titleDy, 500.0)));
            else
                d_plot->setProperty("_toriPlotTitleShift", QVariant());

            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                QwtScaleWidget *sw = d_plot->axisWidget(a);
                if (!sw)
                    continue;
                const double axTDx = attrs.value(QString("ax%1TitleDx").arg(a)).toDouble();
                const double axTDy = attrs.value(QString("ax%1TitleDy").arg(a)).toDouble();
                if (std::fabs(axTDx) > 1e-6 || std::fabs(axTDy) > 1e-6)
                    sw->setProperty("_toriAxisTitleShift",
                                    QPointF(qBound(-500.0, axTDx, 500.0),
                                            qBound(-500.0, axTDy, 500.0)));
                else
                    sw->setProperty("_toriAxisTitleShift", QVariant());

                const double axNorm = attrs.value(QString("ax%1Norm").arg(a)).toDouble();
                if (std::fabs(axNorm) > 1e-6)
                    sw->setProperty("_toriAxisNormalShift", qBound(-500.0, axNorm, 500.0));
                else
                    sw->setProperty("_toriAxisNormalShift", QVariant());

                const double axPar = attrs.value(QString("ax%1Par").arg(a)).toDouble();
                if (std::fabs(axPar) > 1e-6)
                    sw->setProperty("_toriAxisParallelShift", qBound(-500.0, axPar, 500.0));
                else
                    sw->setProperty("_toriAxisParallelShift", QVariant());
            }
            d_plot->updateLayout();
        }
        return true;
    }

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
        if (attrs.contains("outsideRightMargin")) {
            const int orm = qMax(0, attrs.value("outsideRightMargin").toInt());
            d_plot->setProperty("_outsideRightMargin", orm);
        }
        if (attrs.contains("legendAnchor")) {
            d_plot->setProperty("_toriLegendAnchor", attrs.value("legendAnchor").toInt());
        }
        if (attrs.contains("legendVisible")) {
            d_legend_visible = (attrs.value("legendVisible").toInt() != 0);
            if (Legend *leg = legend())
                leg->setVisible(d_legend_visible);
            if (!d_legend_visible && d_plot->property("_outsideRightMargin").toInt() > 0) {
                d_plot->setProperty("_savedOutsideRightMargin",
                                    d_plot->property("_outsideRightMargin").toInt());
                d_plot->setProperty("_outsideRightMargin", 0);
            }
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
        const QString textPts = attrs.value("textPts");
        if (!textPts.isEmpty()) {
            for (const QString &pair : textPts.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = pair.split(':');
                if (kv.size() != 2)
                    continue;
                const int ti = kv[0].toInt();
                const double tpt = kv[1].toDouble();
                if (ti >= 0 && ti < d_texts.size() && tpt > 0.0 && tpt <= 1000.0) {
                    if (Legend *tm = dynamic_cast<Legend *>(d_plot->marker(d_texts[ti]))) {
                        QFont tf = tm->font();
                        tf.setPointSizeF(tpt);
                        tm->setFont(tf);
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
        const QString curveWidths = attrs.value("curveWidths");
        if (!curveWidths.isEmpty()) {
            for (const QString &entry : curveWidths.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = entry.split(':');
                if (kv.size() == 2) {
                    const int ci = kv[0].toInt();
                    const QStringList wv = kv[1].split(',');
                    if (wv.size() < 2)
                        continue;
                    QwtPlotCurve *cv = nullptr;
                    int resolvedIdx = -1;
                    bool titleMatched = false;
                    const bool hasTitleKey = (wv.size() >= 3 && !wv[2].isEmpty());
                    if (hasTitleKey) {
                        const QString wantTitle =
                                QString::fromUtf8(QByteArray::fromBase64(wv[2].toLatin1()));
                        if (QwtPlotCurve *cand = curve(ci)) {
                            if (cand->title().text() == wantTitle) {
                                cv = cand;
                                resolvedIdx = ci;
                                titleMatched = true;
                            }
                        }
                        if (!cv) {
                            for (int k = 0; k < curves(); ++k) {
                                if (QwtPlotCurve *cand = curve(k)) {
                                    if (cand->title().text() == wantTitle) {
                                        cv = cand;
                                        resolvedIdx = k;
                                        titleMatched = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (!cv) {
                        // Fallback when no title key is stored or appendProject renamed the source table
                        cv = curve(ci);
                        resolvedIdx = ci;
                    }
                    if (cv) {
                        bool okL = false, okS = false;
                        const double lw = wv[0].toDouble(&okL);
                        const double sw = wv[1].toDouble(&okS);
                        const int ct = curveType(resolvedIdx);
                        const bool symWidthInCurveLine = (ct <= LineSymbols || ct == Box);
                        if (okL && lw > 0.0 && lw <= 200.0
                            && (titleMatched || std::abs(qRound(lw) - cv->pen().width()) <= 1)) {
                            QPen p = cv->pen();
                            p.setWidthF(lw);
                            cv->setPen(p);
                        }
                        if (okS && sw > 0.0 && sw <= 200.0
                            && (titleMatched || !symWidthInCurveLine
                                || std::abs(qRound(sw) - cv->symbol().pen().width()) <= 1)) {
                            QwtSymbol sym = cv->symbol();
                            QPen sp = sym.pen();
                            sp.setWidthF(sw);
                            sym.setPen(sp);
                            cv->setSymbol(sym);
                        }
                    }
                }
            }
        }
        const QString stackOffsets = attrs.value("stackOffsets");
        if (!stackOffsets.isEmpty()) {
            // The saved indices refer to the curve order at save time. Applying an offset
            // reloads the curve's error bars, which may remove one of them and shift the curve
            // indices; resolve every index to its (never reused) curve key first.
            QList<QPair<int, QStringList>> pending;
            for (const QString &entry : stackOffsets.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = entry.split(':');
                if (kv.size() == 2) {
                    bool okIdx = false;
                    const int ci = kv[0].toInt(&okIdx);
                    if (okIdx && ci >= 0 && ci < n_curves && ci < c_keys.size())
                        pending.append(qMakePair(c_keys[ci], kv[1].split(',')));
                }
            }
            for (const QPair<int, QStringList> &p : pending) {
                const int ci = curveIndex(long(p.first));
                if (ci < 0)
                    continue;
                const QStringList &vals = p.second;
                PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(ci));
                if (pc && vals.size() >= 3) {
                    pc->setStackNormalize01(vals[2].toInt() != 0);
                    pc->setStackOffset(vals[0].toDouble(), vals[1].toDouble());
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
        syncCurveZOrder();
        d_plot->replot();
        return true;
    }

    if (s.startsWith("<ToriCurveAlpha")) {
        const int ci = attrs.contains("idx") ? attrs.value("idx", "-1").toInt()
                                             : attrs.value("curve", "-1").toInt();
        if (ci >= 0 && ci < n_curves) {
            if (PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(ci))) {
                if (attrs.contains("symSkip"))
                    pc->setSymbolSkip(qMax(0, attrs.value("symSkip").toInt()));
            }
        }
        return true;
    }

    if (s.startsWith("<ToriTemplateCurveStyle")) {
        TemplateCurveStyle st;
        st.curveType = attrs.value("type", QString::number(int(LineSymbols))).toInt();
        st.penStyle = attrs.value("penStyle", QString::number(int(Qt::SolidLine))).toInt();
        st.lineWidth = attrs.value("lineWidth", "1.5").toDouble();
        if (attrs.contains("color")) {
            QColor c(attrs.value("color"));
            if (c.isValid())
                st.lineColor = c;
        }
        st.brushStyle = attrs.value("brushStyle", QString::number(int(Qt::NoBrush))).toInt();
        if (attrs.contains("brushColor")) {
            QColor c(attrs.value("brushColor"));
            if (c.isValid())
                st.brushColor = c;
        }
        st.symbolStyle = attrs.value("symbol", QString::number(int(QwtSymbol::NoSymbol))).toInt();
        st.symbolSize = attrs.value("symSize", "7").toInt();
        st.symbolPenWidth = attrs.value("symWidth", "1.0").toDouble();
        if (attrs.contains("symColor")) {
            QColor c(attrs.value("symColor"));
            if (c.isValid())
                st.symbolEdgeColor = c;
        }
        if (attrs.contains("symFill")) {
            QColor c(attrs.value("symFill"));
            if (c.isValid())
                st.symbolFillColor = c;
        }
        st.symbolHollow = attrs.value("hollow", "0").toInt() != 0;
        st.symbolSkip = qMax(0, attrs.value("symSkip", "0").toInt());
        st.barGap = attrs.value("barGap", "0").toInt();
        st.barOffset = attrs.value("barOffset", "0").toInt();
        st.xAxis = attrs.value("xAxis", QString::number(int(QwtPlot::xBottom))).toInt();
        st.yAxis = attrs.value("yAxis", QString::number(int(QwtPlot::yLeft))).toInt();

        const int idx = attrs.value("idx", "-1").toInt();
        if (idx >= 0) {
            if (idx >= d_template_curve_styles.size())
                d_template_curve_styles.resize(idx + 1);
            d_template_curve_styles[idx] = st;
            if (idx < n_curves)
                applyTemplateCurveStyle(idx, idx);
        } else {
            d_template_curve_styles.append(st);
        }
        return true;
    }

    if (s.startsWith("<ToriAutoAxisTitles")) {
        const QString uf = attrs.value("unitFormat");
        if (uf == QStringLiteral("Paren"))
            d_axis_unit_format = Column::UnitFormat::Paren;
        else if (uf == QStringLiteral("Bracket"))
            d_axis_unit_format = kUnitFormatBracket;
        else
            d_axis_unit_format = Column::UnitFormat::ISIJSlash;
        for (int a = 0; a < 4; ++a) {
            const QString key = QString("a%1").arg(a);
            if (attrs.contains(key))
                d_axis_title_auto[a] = (attrs.value(key).toInt() != 0);
        }
        if (curves() > 0)
            refreshAutoAxisTitlesFromColumns();
        return true;
    }

    if (s.startsWith("<ToriErrorBand")) {
        const QString xName = attrs.value("x");
        const QString yName = attrs.value("y");
        const QString errName = attrs.value("err");
        for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); ++i) {
            if (c_type[i] != ErrorBars)
                continue;
            auto *er = dynamic_cast<QwtErrorPlotCurve *>(d_plot->plotItem(c_keys[i]));
            if (!er || er->isBand() || !er->masterCurve() || er->title().text() != errName
                || er->masterCurve()->title().text() != yName
                || er->masterCurve()->xColumnName() != xName)
                continue;
            er->setBand(true);
            er->setBandAlpha(attrs.value("alpha", "64").toInt());
            er->drawPlusSide(attrs.value("plus", "1").toInt() != 0);
            er->drawMinusSide(attrs.value("minus", "1").toInt() != 0);
            break;
        }
        syncCurveZOrder();
        return true;
    }

    if (s.startsWith("<ToriPointColorMap")) {
        const QString xName = attrs.value("x");
        const QString yName = attrs.value("y");
        for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); ++i) {
            if (c_type[i] == ErrorBars)
                continue;
            auto *dc = dynamic_cast<DataCurve *>(d_plot->plotItem(c_keys[i]));
            if (!dc || dc->hasPointColorMap() || dc->title().text() != yName
                || dc->xColumnName() != xName)
                continue;
            DataCurve::PointColorMap pm;
            pm.enabled = true;
            pm.zColumn = attrs.value("z");
            pm.sizeColumn = attrs.value("size");
            pm.preset = attrs.value("preset", "2").toInt();
            pm.reverse = attrs.value("reverse", "0").toInt() != 0;
            pm.autoRange = attrs.value("auto", "1").toInt() != 0;
            pm.zMin = attrs.value("zmin", "0").toDouble();
            pm.zMax = attrs.value("zmax", "1").toDouble();
            pm.sizeMinPx = attrs.value("smin", "4").toDouble();
            pm.sizeMaxPx = attrs.value("smax", "16").toDouble();
            pm.showColorBar = (attrs.value("cb", "1") != "0");
            pm.colorBarTitle = attrs.value("title");
            dc->setPointColorMap(pm);
            break;
        }
        syncPointColorBar();
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
        rl.visible = (attrs.value("visible", "1") != "0");
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
            if (ab.enabled && attrs.value("suspended", "0").toInt() != 0) {
                if (ab.breakFrom > ab.breakTo)
                    std::swap(ab.breakFrom, ab.breakTo);
                ab.positionPct = qBound(0.10, ab.positionPct, 0.90);
                d_axis_breaks[axis].enabled = false;
                d_suspended_axis_breaks[axis] = ab;
            } else {
                setAxisBreak(axis, ab);
            }
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
        if (d_is_ternary) {
            m_autoscale = false;
            if (!d_ternary_item) {
                d_ternary_item = new TernaryDiagramItem(this);
                d_ternary_item->attach(d_plot);
            }
            for (int a = 0; a < QwtPlot::axisCnt; ++a)
                enableAxis(a, false);
            drawAxesBackbones(false);
        }
        return true;
    }

    if (s.startsWith("<ToriStackedBars")) {
        const QStringList idxs = attrs.value("indices").split(',', Qt::SkipEmptyParts);
        int savedGap = attrs.value("gap", "-1").toInt();
        const int savedOffset = attrs.value("offset", "0").toInt();
        if (savedGap < 0) {
            for (const QString &tok : idxs) {
                bool ok = false;
                const int bi = tok.trimmed().toInt(&ok);
                if (ok && bi >= 0 && bi < n_curves) {
                    if (const QwtBarCurve *bc = dynamic_cast<const QwtBarCurve *>(curve(bi))) {
                        savedGap = bc->gap();
                        break;
                    }
                }
            }
            if (savedGap < 0)
                savedGap = 30;
        }
        for (const QString &tok : idxs) {
            bool ok = false;
            const int bi = tok.trimmed().toInt(&ok);
            if (ok && bi >= 0 && bi < n_curves) {
                if (QwtBarCurve *bc = dynamic_cast<QwtBarCurve *>(curve(bi))) {
                    bc->setStacked(true);
                    setBarsGap(bi, savedGap, savedOffset);
                }
            }
        }
        if (d_plot)
            d_plot->replot();
        return true;
    }

    if (s.startsWith("<ToriWeibullAxis")) {
        const int axis = attrs.value("axis", "-1").toInt();
        const bool en = attrs.value("enabled", "1").toInt() != 0;
        if (axis >= 0 && axis < 4)
            setWeibullProbabilityAxis(axis, en);
        return true;
    }

    if (s.startsWith("<ToriCategoryAxis")) {
        const int axis = attrs.value("axis", "-1").toInt();
        const QString rawLabels = attrs.value("labels");
        const QString rawPositions = attrs.value("positions");
        if (axis >= 0 && axis < 4 && !rawLabels.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            const QStringList lbls = rawLabels.split(QChar(0x1F), Qt::KeepEmptyParts);
#else
            const QStringList lbls = rawLabels.split(QChar(0x1F), QString::KeepEmptyParts);
#endif
            if (!rawPositions.isEmpty()) {
                const QStringList posStrs = rawPositions.split(',', Qt::SkipEmptyParts);
                QVector<double> posVec;
                posVec.reserve(posStrs.size());
                for (const QString &ps : posStrs)
                    posVec.append(ps.toDouble());
                const double rot = attrs.value("rot", "0").toDouble();
                setCustomTickLabels(axis, posVec, lbls, rot);
            } else {
                setCategoryLabels(axis, lbls);
            }
            if (!d_is_ternary && !d_is_polar && !isPoleFigurePlot() && !isPiePlot()) {
                updateSecondaryAxis(QwtPlot::xTop);
                updateSecondaryAxis(QwtPlot::yRight);
            }
        }
        return true;
    }

    if (s.startsWith("<ToriPolarPlot")) {
        const bool en = attrs.value("enabled", "1").toInt() != 0;
        if (en) {
            d_polar_rmax = qMax(1e-6, attrs.value("rmax", "1").toDouble());
            setupPolarPlot(QVector<double>(), QVector<double>(), QString(), true);
        }
        return true;
    }

    if (s.startsWith("<ToriPolarSeries")) {
        const QStringList xs = attrs.value("x").split(',', Qt::SkipEmptyParts);
        const QStringList ys = attrs.value("y").split(',', Qt::SkipEmptyParts);
        const int m = qMin(xs.size(), ys.size());
        if (m >= 1) {
            PolarSeriesSpec ps;
            ps.x.resize(m);
            ps.y.resize(m);
            for (int k = 0; k < m; ++k) {
                ps.x[k] = xs[k].toDouble();
                ps.y[k] = ys[k].toDouble();
            }
            ps.label = attrs.value("label", tr("Polar"));
            ps.color = QColor(attrs.value("color", "#ff0072b2"));
            if (!ps.color.isValid())
                ps.color = QColor(0, 114, 178);
            ps.width = qMax(1.0, attrs.value("width", "2").toDouble());

            PlotCurve *pc = new PlotCurve(ps.label);
            pc->setType(Function);
            pc->setData(ps.x.constData(), ps.y.constData(), m);
            QPen p(ps.color, ps.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setCosmetic(false);
            pc->setPen(p);

            c_type.resize(++n_curves);
            c_type[n_curves - 1] = Function;
            c_keys.resize(n_curves);
            c_keys[n_curves - 1] = d_plot->insertCurve(pc);
            syncCurveZOrder();
            d_polar_series.append(ps);

            const double rMax = qMax(1e-6, d_polar_rmax);
            const int nPolarRows = qMax(1, qMax(int(d_polar_series.size()), n_curves));
            const int extraRows = qMax(0, nPolarRows - 1);
            const double yLo = -1.44 * rMax;
            const double yHi = (1.64 + 0.36 * double(extraRows)) * rMax;
            const double halfX = 0.5 * (yHi - yLo);
            setScale(QwtPlot::xBottom, -halfX, halfX, 0.0, 5, 0, 0, false);
            setScale(QwtPlot::yLeft, yLo, yHi, 0.0, 5, 0, 0, false);
            for (int a = 0; a < QwtPlot::axisCnt; ++a)
                enableAxis(a, false);
            if (hasLegend())
                placeLegendAnchor(LegendTopRight);
        }
        return true;
    }

    if (s.startsWith("<ToriAutoscaleFonts")) {
        autoScaleFonts = (attrs.value("val", "0").toInt() != 0);
        return true;
    }

    if (s.startsWith("<ToriAxisLocks")) {
        const int mask = attrs.value("mask", "0").toInt();
        for (int a = 0; a < 4; ++a)
            setAxisUserLocked(a, (mask & (1 << a)) != 0);
        if (attrs.contains("autoscale")) {
            m_autoscale = (attrs.value("autoscale").toInt() != 0);
            if (d_plot)
                d_plot->setProperty("_toriRestoredAutoscale", true);
        }
        if (attrs.contains("autoMask") && d_plot && !isTernaryPlot() && !isPolarPlot()
            && !isPiePlot() && !isPoleFigurePlot()) {
            const int autoMask = attrs.value("autoMask", "0").toInt();
            bool restoredAnyAuto = false;
            for (int a = 0; a < 4; ++a) {
                if ((autoMask & (1 << a)) != 0 && (mask & (1 << a)) == 0 && !axisBreak(a).enabled
                    && !hasColorScaleOnAxis(a) && !hasWeibullProbabilityAxis(a)
                    && !hasCustomTickLabels(a)) {
                    d_plot->setAxisAutoScale(a);
                    restoredAnyAuto = true;
                }
            }
            if (restoredAnyAuto) {
                d_plot->updateAxes();
                if (d_plot->axisEnabled(QwtPlot::xTop) && !isAxisUserLocked(QwtPlot::xTop))
                    updateSecondaryAxis(QwtPlot::xTop);
                if (d_plot->axisEnabled(QwtPlot::yRight) && !isAxisUserLocked(QwtPlot::yRight))
                    updateSecondaryAxis(QwtPlot::yRight);
            }
        }
        if (isPiePlot()) {
            drawCanvasFrame(false, 0);
            setFrame(0, Qt::black);
            drawAxesBackbones(false);
        }
        return true;
    }

    return false;
}
