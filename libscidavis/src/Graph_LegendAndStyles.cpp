#include "OleServer.h"
#include "MultiLayer.h"
/***************************************************************************
    File                 : Graph_LegendAndStyles.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Graph title, markers, legend placement, color palettes, and series style schemes

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

void Graph::deselectMarker()
{
    selectedMarker = -1;
    detachAndDeleteMarkerSelector(d_markers_selector);
    if (cp)
        cp->disableEditing();
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
            connect(d_markers_selector, &SelectionMoveResizer::targetsChanged, this, [this]() {
                if (d_plot && legend() && d_markers_selector
                    && d_markers_selector->contains(legend()))
                    d_plot->setProperty("_toriLegendAnchor", QVariant());
            });
            connect(d_markers_selector, SIGNAL(targetsContextMenuRequested()), this,
                    SIGNAL(showMarkerPopupMenu()));
        }
    } else {
        if (d_texts.contains(mrk)) {
            if (d_markers_selector) {
                if (d_markers_selector->contains((Legend *)d_plot->marker(mrk)))
                    return;
                detachAndDeleteMarkerSelector(d_markers_selector);
            }
            d_markers_selector = new SelectionMoveResizer((Legend *)d_plot->marker(mrk));
        } else if (d_lines.contains(mrk)) {
            if (d_markers_selector) {
                if (d_markers_selector->contains((ArrowMarker *)d_plot->marker(mrk)))
                    return;
                detachAndDeleteMarkerSelector(d_markers_selector);
            }
            d_markers_selector = new SelectionMoveResizer((ArrowMarker *)d_plot->marker(mrk));
        } else if (d_images.contains(mrk)) {
            if (d_markers_selector) {
                if (d_markers_selector->contains((ImageMarker *)d_plot->marker(mrk)))
                    return;
                detachAndDeleteMarkerSelector(d_markers_selector);
            }
            d_markers_selector = new SelectionMoveResizer((ImageMarker *)d_plot->marker(mrk));
        } else
            return;
        connect(d_markers_selector, SIGNAL(targetsChanged()), this, SIGNAL(modifiedGraph()));
        connect(d_markers_selector, &SelectionMoveResizer::targetsChanged, this, [this]() {
            if (d_plot && legend() && d_markers_selector && d_markers_selector->contains(legend()))
                d_plot->setProperty("_toriLegendAnchor", QVariant());
        });
        connect(d_markers_selector, SIGNAL(targetsContextMenuRequested()), this,
                SIGNAL(showMarkerPopupMenu()));
    }
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
        if (cp)
            cp->disableEditing();
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
    // Stale keyboard focus on the title label alone no longer counts as "selected":
    // after Esc / blank click the title must not be deleted or nudged (B4-3).
    return d_selection.kind == GraphSelection::PlotTitle
            || (titlePicker && titlePicker->selected());
}

void Graph::selectTitle()
{
    if (!d_plot->hasFocus()) {
        emit selectedGraph(this);
        QwtTextLabel *title = d_plot->titleLabel();
        title->setFocus();
    }

    deselectMarker();
    setCurrentSelection({ GraphSelection::PlotTitle, -1, -1 });
}

void Graph::setTitle(const QString &t)
{
    QString cleanT = t;
    cleanT.replace(QLatin1String("&#9;"), QLatin1String("\t"));
    cleanT.replace(QLatin1String("&#60;"), QLatin1String("<"));
    QwtText cur = d_plot->title();
    QFont f = cur.font();
    if (f.family().trimmed().isEmpty())
        f = QFont("Arial", 16, QFont::Normal, false);
    cur.setFont(f);
    cur.setText(cleanT);
    d_plot->setTitle(cur);
    updateSelectionOverlayGeometry();
    emit modifiedGraph();
}

void Graph::removeTitle()
{
    if (titleSelected() || d_selection.kind == GraphSelection::PlotTitle) {
        MultiLayer *ml = multiLayerParent();
        const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
        setTitle(QString());
        if (titlePicker)
            titlePicker->setSelected(false);
        if (d_selection.kind == GraphSelection::PlotTitle)
            setCurrentSelection({ GraphSelection::None, -1, -1 });
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(tr("Remove Plot Title"), beforeSnap);
    }
}

void Graph::initTitle(bool on, const QFont &fnt)
{
    if (on) {
        QFont nf = fnt;
        nf.setHintingPreference(QFont::PreferNoHinting);
        QwtText t = d_plot->title();
        t.setFont(nf);
        t.setText(tr("Title"));
        d_plot->setTitle(t);
    }
}

void Graph::removeLegend()
{
    if (legendMarkerID >= 0) {
        Legend *leg = legend();
        if (d_markers_selector && leg)
            d_markers_selector->removeAll(leg);
        if (selectedMarker == legendMarkerID)
            deselectMarker();
        const int index = d_texts.indexOf(legendMarkerID);
        if (index >= 0) {
            int texts = d_texts.size();
            for (int i = index; i < texts - 1; i++)
                d_texts[i] = d_texts[i + 1];
            d_texts.resize(--texts);
        }

        d_plot->removeMarker(legendMarkerID);
        legendMarkerID = -1;
        if (d_plot->property("_outsideRightMargin").toInt() != 0)
            d_plot->setProperty("_outsideRightMargin", 0);
        d_plot->setProperty("_savedOutsideRightMargin", 0);
        if (d_selection.kind == GraphSelection::LegendBox)
            setCurrentSelection({ GraphSelection::None, -1, -1 });
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
        if (c && c->rtti() != QwtPlotItem::Rtti_PlotSpectrogram && c_type[i] != ErrorBars
            && c->testItemAttribute(QwtPlotItem::Legend)) {
            text += "\\c{";
            text += QString::number(i + 1);
            text += "}";
            const FunctionCurve *fc = dynamic_cast<const FunctionCurve *>(c);
            if (fc && !d_fit_curves.contains(const_cast<QwtPlotCurve *>(c)))
                text += fc->legend();
            else if (fc)
                text += c->title().text();
            else {
                QString label = resolvedAutoCurveLabel(i);
                if (label.isEmpty())
                    label = c->title().text();
                DataCurve *dc = dynamic_cast<DataCurve *>(const_cast<QwtPlotCurve *>(c));
                if (dc && dc->table())
                    d_last_auto_curve_labels[i] = label;
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
    const QwtPlotCurve *pieCurve = c_keys.isEmpty() ? nullptr : curve(0);
    if (pieCurve) {
        for (int i = 0; i < int(pieCurve->dataSize()); i++) {
            text += "\\p{";
            text += QString::number(i + 1);
            text += "} ";
            text += QString::number(i + 1);
            text += "\n";
        }
    }
    return text;
}

Legend *Graph::newLegend()
{
    d_legend_visible = true;
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

    if (!isPiePlot() && curves() > 0 && d_plot) {
        d_plot->updateLayout();
        if (d_plot->canvas() && d_plot->canvas()->width() >= 40
            && d_plot->canvas()->height() >= 40) {
            d_plot->replot();
            placeLegendAnchor(LegendAutoAvoidData, nullptr, 12);
            return mrk;
        }
    }

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
    detachAndDeleteMarkerSelector(d_markers_selector);

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
    if (Legend *leg = legend())
        leg->setVisible(d_legend_visible);
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
    bool exactText = false;
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

        // The writer joins the lines of the text with tabs, so joining them back gives the text
        // as it was. Trimming it dropped a line break at the end (a legend made by
        // Graph::legendText() ends in one) on the first reopen.
        text = fList.mid(14).join(QLatin1Char('\n'));
        exactText = true;
    }
    QString unescMarker = exactText ? text : text.trimmed();
    unescMarker.replace(QLatin1String("&#9;"), QLatin1String("\t"));
    unescMarker.replace(QLatin1String("&#60;"), QLatin1String("<"));
    mrk->setText(unescMarker);
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

    bool okW = false;
    const double lw = list[5].toDouble(&okW);
    mrk->setWidthF((okW && lw > 0.0) ? lw : double(qMax(1, list[5].toInt())));
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
    aux->setWidthF(mrk->widthF());
    aux->setColor(mrk->color());
    aux->setStyle(mrk->style());
    aux->setCapStyle(mrk->capStyle());
    aux->setJoinStyle(mrk->joinStyle());
    if (mrk->style() == Qt::CustomDashLine) {
        QPen pen = aux->linePen();
        pen.setDashPattern(mrk->linePen().dashPattern());
        aux->setLinePen(pen);
    }
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
    detachAndDeleteMarkerSelector(d_markers_selector);

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

void Graph::removeLegendItem(int index)
{
    if (legendMarkerID < 0 || index < 0)
        return;

    Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
    if (!mrk)
        return;

    if (isPiePlot()) {
        mrk->setText({});
        return;
    }

    // Error bars have a number of their own (\c{n} counts every item of the layer) but no line:
    // the numbers after them move up as well. A curve number past the number of lines is looked
    // for too (a legend line can be missing, and error bars take numbers without lines).
    removeLegendCurveNumber(index);
}

void Graph::removeLegendCurveNumber(int index)
{
    if (legendMarkerID < 0 || index < 0)
        return;
    Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
    if (!mrk)
        return;

    const QString before = mrk->text();
    // Empty lines the user put into the legend stay: the text is split with them and only the
    // line of the removed item goes. That line is the first one whose symbol token is the item's
    // number, written as \c{n} or as the Origin-style \l(n) (Legend draws both).
    QStringList items = before.split(QLatin1Char('\n'));
    const QRegularExpression lineRx(QStringLiteral(R"(\\c\{%1\}|\\l\(%1\))").arg(index + 1));
    for (int i = 0; i < items.size(); ++i) {
        if (lineRx.match(items.at(i)).hasMatch()) {
            items.removeAt(i);
            break;
        }
    }
    QString text = items.join(QLatin1Char('\n'));

    // the numbers of the items after it (\c{n}, \l(n), %(n)) move down by one; lines that are
    // not a curve's keep their text
    static const QRegularExpression numberRx(
            QStringLiteral(R"(\\c\{(\d+)\}|\\l\((\d+)\)|%\((\d+)\))"));
    int highest = 0;
    QRegularExpressionMatchIterator it = numberRx.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        for (int cap = 1; cap <= 3; ++cap) {
            if (!m.captured(cap).isEmpty())
                highest = qMax(highest, m.captured(cap).toInt());
        }
    }
    if (highest > index + 1) {
        QVector<int> oldToNew(highest);
        for (int i = 0; i < highest; ++i)
            oldToNew[i] = (i <= index) ? i : i - 1;
        oldToNew[index] = -1; // a number of the removed item left on another line stays
        text = remapLegendCurveTokens(text, oldToNew);
    }

    if (text != before)
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
                    QString strippedArg = colName;
                    if (strippedArg.startsWith(prefix) && strippedArg.length() > prefix.length())
                        strippedArg = strippedArg.mid(prefix.length());
                    QString strippedYCol = dc->yColumnName();
                    if (strippedYCol.startsWith(prefix) && strippedYCol.length() > prefix.length())
                        strippedYCol = strippedYCol.mid(prefix.length());

                    if (colName == dc->yColumnName() || strippedArg == strippedYCol
                        || colName == dc->title().text()) {
                        const QString richLabel = resolvedAutoCurveLabel(n_curves - 1);
                        if (!richLabel.isEmpty())
                            label = richLabel;
                        else
                            label = strippedYCol;
                    } else if (label.startsWith(prefix) && label.length() > prefix.length()) {
                        label = label.mid(prefix.length());
                    }
                    d_last_auto_curve_labels[n_curves - 1] = label;
                }
            }
            QString text = mrk->text();
            if (text.isEmpty() || text.endsWith("\n", Qt::CaseSensitive))
                text.append("\\c{" + QString::number(curves()) + "}" + label + "\n");
            else
                text.append("\n\\c{" + QString::number(curves()) + "}" + label + "\n");

            if (n_curves >= 2) {
                const bool hadTrailingNewline = text.endsWith(QLatin1Char('\n'));
                QString core = hadTrailingNewline ? text.chopped(1) : text;
                QStringList lines = core.split(QLatin1Char('\n'));
                static const QRegularExpression lineRx(QStringLiteral("^\\\\c\\{(\\d+)\\}(.*)$"));
                bool anyUpdated = false;
                for (int li = 0; li < lines.size(); ++li) {
                    const QRegularExpressionMatch m = lineRx.match(lines[li]);
                    if (!m.hasMatch())
                        continue;
                    const int ci = m.captured(1).toInt() - 1;
                    if (ci < 0 || ci >= n_curves - 1)
                        continue;
                    const QString prevAuto = d_last_auto_curve_labels.value(ci);
                    const QString curBody = m.captured(2);
                    if (!prevAuto.isEmpty() && curBody == prevAuto) {
                        const QString updated = resolvedAutoCurveLabel(ci);
                        if (!updated.isEmpty() && updated != curBody) {
                            lines[li] = QStringLiteral("\\c{%1}%2").arg(ci + 1).arg(updated);
                            d_last_auto_curve_labels[ci] = updated;
                            anyUpdated = true;
                        }
                    }
                }
                if (anyUpdated) {
                    text = lines.join(QLatin1Char('\n'));
                    if (hadTrailingNewline)
                        text.append(QLatin1Char('\n'));
                }
            }

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

Graph::MarkerAxisState Graph::markerAxisState() const
{
    MarkerAxisState st;
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QwtScaleDiv *d = d_plot->axisScaleDiv(a);
        st.valid[a] = d && d->isValid();
        st.map[a] = d_plot->canvasMap(a);
        if (!st.valid[a]) {
            // Qwt's setAxisScale() leaves the scale division invalid until the next replot. The
            // scale widget still draws the previous scale: take range and transformation from
            // its scale draw (the pixel interval of canvasMap() is right either way).
            const QwtScaleWidget *sw = d_plot->axisWidget(a);
            const QwtScaleDraw *sd = sw ? sw->scaleDraw() : nullptr;
            if (sd && sd->map().transformation() && sd->scaleDiv().isValid()
                && sd->map().s1() != sd->map().s2()) {
                st.map[a].setTransformation(sd->map().transformation()->copy());
                st.map[a].setScaleInterval(sd->map().s1(), sd->map().s2());
                st.valid[a] = true;
            }
        }
    }
    return st;
}

namespace {
// True when both maps put every value at the same fraction of their paint interval: same range
// and the same transformation, including axis breaks and formula axes (sampled along the axis).
bool sameAxisMapping(const QwtScaleMap &a, const QwtScaleMap &b)
{
    if (a.s1() != b.s1() || a.s2() != b.s2())
        return false;
    const QwtScaleTransformation *ta = a.transformation();
    const QwtScaleTransformation *tb = b.transformation();
    if (!ta || !tb || ta->type() != tb->type())
        return false;
    const double da = a.p2() - a.p1();
    const double db = b.p2() - b.p1();
    if (da == 0.0 || db == 0.0)
        return da == db;
    constexpr int kSamples = 64;
    for (int k = 0; k <= kSamples; ++k) {
        const double v = a.s1() + (a.s2() - a.s1()) * double(k) / double(kSamples);
        const double fa = (a.xTransform(v) - a.p1()) / da;
        const double fb = (b.xTransform(v) - b.p1()) / db;
        if (std::isnan(fa) && std::isnan(fb))
            continue;
        if (!(std::fabs(fa - fb) <= 1e-9))
            return false;
    }
    return true;
}
} // namespace

void Graph::refreshMarkerPixelCaches()
{
    for (int i = 0; i < (int)d_lines.size(); i++) {
        if (ArrowMarker *mrkL = (ArrowMarker *)d_plot->marker(d_lines[i]))
            mrkL->updateBoundingRect();
    }
    for (int i = 0; i < (int)d_texts.size(); i++) {
        if (Legend *mrkT = (Legend *)d_plot->marker(d_texts[i])) {
            if (mrkT == legend() && repositionAnchoredOutsideRightLegend())
                continue;
            mrkT->refreshPixelOrigin();
        }
    }
    for (int i = 0; i < (int)d_images.size(); i++) {
        if (ImageMarker *mrk = (ImageMarker *)d_plot->marker(d_images[i]))
            mrk->refreshPixelCache();
    }
}

void Graph::updateMarkersBoundingRect(const MarkerAxisState &before)
{
    if (d_is_ternary) {
        updateMarkersBoundingRect();
        return;
    }
    // Settle the new scales and the layout (tick labels of another width move the canvas edges)
    // before taking the new canvas maps; Plot::updateLayout() also settles a setAxisScale() that
    // was not replotted yet.
    d_plot->updateLayout();
    const MarkerAxisState now = markerAxisState();
    bool same[QwtPlot::axisCnt];
    for (int a = 0; a < QwtPlot::axisCnt; ++a)
        same[a] = before.valid[a] && now.valid[a] && sameAxisMapping(before.map[a], now.map[a]);
    auto unchanged = [&](int xAxis, int yAxis) {
        return xAxis >= 0 && xAxis < QwtPlot::axisCnt && yAxis >= 0 && yAxis < QwtPlot::axisCnt
                && same[xAxis] && same[yAxis];
    };
    // The previous scale of an axis (range and transformation) on the current canvas. Not
    // available while the previous scale division was invalid (Qwt's setAxisScale() leaves it
    // so until the next replot): the marker then keeps its cached pixel position.
    auto previousMap = [&](int axis, QwtScaleMap &m) -> bool {
        if (axis < 0 || axis >= QwtPlot::axisCnt || !before.valid[axis] || !now.valid[axis])
            return false;
        m = before.map[axis];
        m.setPaintXInterval(now.map[axis].p1(), now.map[axis].p2());
        return true;
    };

    // Arrows are drawn between data points: they follow the data.
    for (int i = 0; i < (int)d_lines.size(); i++) {
        if (ArrowMarker *mrkL = (ArrowMarker *)d_plot->marker(d_lines[i]))
            mrkL->updateBoundingRect();
    }
    for (int i = 0; i < (int)d_texts.size(); i++) {
        Legend *mrkT = (Legend *)d_plot->marker(d_texts[i]);
        if (!mrkT)
            continue;
        if (mrkT == legend() && repositionAnchoredOutsideRightLegend())
            continue;
        if (!unchanged(mrkT->xAxis(), mrkT->yAxis())) {
            QwtScaleMap xm, ym;
            if (previousMap(mrkT->xAxis(), xm) && previousMap(mrkT->yAxis(), ym))
                mrkT->refreshPixelOrigin(&xm, &ym);
            mrkT->updateOrigin();
        }
        mrkT->refreshPixelOrigin();
    }
    for (int i = 0; i < (int)d_images.size(); i++) {
        ImageMarker *mrk = (ImageMarker *)d_plot->marker(d_images[i]);
        if (!mrk)
            continue;
        if (!unchanged(mrk->xAxis(), mrk->yAxis())) {
            QwtScaleMap xm, ym;
            if (previousMap(mrk->xAxis(), xm) && previousMap(mrk->yAxis(), ym)) {
                // As ImageMarker::rect(), with the previous scales.
                const int x0 = xm.transform(mrk->xValue());
                const int y0 = ym.transform(mrk->yValue());
                const int x1 = xm.transform(mrk->right());
                const int y1 = ym.transform(mrk->bottom());
                mrk->setRect(x0, y0, qAbs(x1 - x0), qAbs(y1 - y0));
            }
            mrk->updateBoundingRect();
        }
        mrk->refreshPixelCache();
    }
    d_plot->replot();
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
        if (!mrkT)
            continue;
        if (mrkT == legend() && repositionAnchoredOutsideRightLegend())
            continue;
        mrkT->updateOrigin();
    }

    for (int i = 0; i < (int)d_images.size(); i++) {
        ImageMarker *mrk = (ImageMarker *)d_plot->marker(d_images[i]);
        if (mrk)
            mrk->updateBoundingRect();
    }
    d_plot->replot();
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

const QList<QColor> &Graph::okabeItoColors()
{
    // Okabe & Ito (2008), blue-first order used throughout ToriGraph. The eighth entry is black
    // (part of the original set); never a non-palette gray.
    static const QList<QColor> kColors = hexListToColors(
            { "#0072b2", "#e69f00", "#009e73", "#d55e00", "#cc79a7", "#56b4e9", "#f0e442",
              "#000000" });
    return kColors;
}

const QList<QColor> &Graph::tab10Colors()
{
    static const QList<QColor> kColors = hexListToColors(
            { "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2",
              "#7f7f7f", "#bcbd22", "#17becf" });
    return kColors;
}

const QList<QColor> &Graph::originClassicColors()
{
    // OriginPro "Classic" color increment list: Black, Red, Blue, Magenta, Olive, Navy,
    // Violet, Purple, Wine, Dark Cyan, Royal, Orange.
    static const QList<QColor> kColors = hexListToColors(
            { "#000000", "#ff0000", "#0000ff", "#ff00ff", "#808000", "#000080", "#8000ff",
              "#800080", "#800000", "#008080", "#0000a0", "#ff8000" });
    return kColors;
}

const QList<QColor> &Graph::toriGraphStandardColors()
{
    // Default for new curves. Black first (the order people know from single-colour journal
    // figures), then colour-vision-safe hues darkened where needed for crisp contrast on white
    // pages and slide projectors: vermilion, blue, bluish green, dark amber (#b07800),
    // deep sky blue (#2b8cc4), reddish purple, gray (#666666).
    // Okabe-Ito yellow (#f0e442) is left out because it is hard to see on a white page.
    static const QList<QColor> kColors = hexListToColors(
            { "#000000", "#d55e00", "#0072b2", "#009e73", "#b07800", "#2b8cc4", "#cc79a7",
              "#666666" });
    return kColors;
}

void Graph::registerPaletteTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("ToriGraph Standard", "ToriGraph 標準 (黒・朱・青…)");
        reg("ToriGraph Standard (black, vermilion, blue, ...)", "ToriGraph 標準 (黒・朱・青…)");
        reg("\"%1\" is the name of a built-in palette. Choose another name.",
            "「%1」は組み込みのパレットの名前です。別の名前にしてください。");
        reg("Enter a palette name and at least one colour.",
            "パレットの名前と、少なくとも 1 つの色を指定してください。");
    });
}

QString Graph::canonicalPaletteName(const QString &name)
{
    const QString key = name.trimmed();
    if (key.compare(QLatin1String("ISIJ / Origin Default"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Origin Default"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("ISIJ"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("tab10"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Matplotlib"), Qt::CaseInsensitive) == 0
        || key.compare(tab10PaletteName(), Qt::CaseInsensitive) == 0)
        return tab10PaletteName();
    if (key.compare(QLatin1String("Origin Classic"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Origin"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("origin_classic"), Qt::CaseInsensitive) == 0)
        return originClassicPaletteName();
    if (key.compare(toriGraphStandardPaletteName(), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("ToriGraph"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("torigraph_standard"), Qt::CaseInsensitive) == 0)
        return toriGraphStandardPaletteName();
    if (key.compare(QLatin1String("Okabe-Ito"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("okabe_ito"), Qt::CaseInsensitive) == 0
        || key.compare(okabeItoPaletteName(), Qt::CaseInsensitive) == 0)
        return okabeItoPaletteName();
    if (key.compare(QLatin1String("Cool-Warm"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("CoolWarm"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("cool_warm"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("CoolWarm");
    return key;
}

namespace {
QString &defaultCurvePaletteCache()
{
    static QString s_name;
    return s_name;
}
} // namespace

QString Graph::defaultCurvePaletteName()
{
    QString &cached = defaultCurvePaletteCache();
    if (cached.isEmpty()) {
        QSettings &settings = ApplicationWindow::getSettings();
        const QString stored =
                settings.value(QStringLiteral("/Curves/DefaultPalette"), QString()).toString();
        // Users who never picked a palette follow the built-in default (ToriGraph Standard
        // since 2026-11; it was Origin Classic before). An explicit choice is kept as is.
        cached = stored.trimmed().isEmpty() ? builtInDefaultCurvePaletteName()
                                            : canonicalPaletteName(stored);
    }
    return cached;
}

void Graph::setDefaultCurvePaletteName(const QString &name)
{
    QSettings &settings = ApplicationWindow::getSettings();
    if (name.trimmed().isEmpty()) {
        // Back to "not chosen": follow the built-in default, also after it changes.
        defaultCurvePaletteCache() = builtInDefaultCurvePaletteName();
        settings.remove(QStringLiteral("/Curves/DefaultPalette"));
        return;
    }
    const QString canon = canonicalPaletteName(name);
    defaultCurvePaletteCache() = canon;
    settings.setValue(QStringLiteral("/Curves/DefaultPalette"), canon);
}

void Graph::reloadDefaultCurvePaletteName()
{
    defaultCurvePaletteCache().clear();
}

int Graph::defaultCurveSymbolIndex(int seriesIndex)
{
    // SymbolBox indices following Origin's symbol increment: square, circle, up triangle,
    // down triangle, diamond, left triangle, right triangle, hexagon, star, cross.
    static const int kSymbolIndices[] = { 2, 1, 6, 5, 3, 7, 8, 15, 13, 9 };
    const int n = int(sizeof(kSymbolIndices) / sizeof(kSymbolIndices[0]));
    return kSymbolIndices[qMax(0, seriesIndex) % n];
}

QStringList Graph::builtInPaletteNames()
{
    registerPaletteTranslations();
    return QStringList()
            << "ToriGraph Standard"
            << "Origin Classic"
            << "Matplotlib (tab10)"
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
    QSettings &settings = ApplicationWindow::getSettings();
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
    const QString key = canonicalPaletteName(paletteName);
    if (key == tab10PaletteName())
        return tab10Colors();
    if (key == originClassicPaletteName())
        return originClassicColors();
    if (key == toriGraphStandardPaletteName())
        return toriGraphStandardColors();
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
    if (key == okabeItoPaletteName())
        return okabeItoColors();
    if (key.compare("Monochrome / Grayscale", Qt::CaseInsensitive) == 0
        || key.compare("Monochrome", Qt::CaseInsensitive) == 0
        || key.compare("Grayscale", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#000000", "#252525", "#454545", "#636363",
                                 "#7f7f7f", "#969696", "#bdbdbd", "#d9d9d9" });
    }

    auto &memStore = customPaletteMemoryStore();
    if (memStore.contains(key))
        return memStore.value(key);

    QSettings &settings = ApplicationWindow::getSettings();
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

    // Fallback: the palette used for new curves.
    const QString defName = defaultCurvePaletteName();
    if (defName == okabeItoPaletteName())
        return okabeItoColors();
    if (defName == tab10PaletteName())
        return tab10Colors();
    if (defName == originClassicPaletteName())
        return originClassicColors();
    return toriGraphStandardColors();
}

bool Graph::isBuiltInPaletteName(const QString &name)
{
    const QString key = name.trimmed();
    if (key.isEmpty())
        return false;
    // Aliases ("ToriGraph", "torigraph_standard", "Okabe-Ito", "ISIJ / Origin Default", ...)
    // resolve to a built-in palette before custom palettes are looked up.
    if (canonicalPaletteName(key) != key)
        return true;
    for (const QString &builtIn : builtInPaletteNames()) {
        if (key.compare(builtIn, Qt::CaseInsensitive) == 0)
            return true;
    }
    // Short names that paletteColors() also maps to built-in lists.
    static const char *const kShort[] = { "Nature", "Science", "Monochrome", "Grayscale" };
    for (const char *s : kShort) {
        if (key.compare(QLatin1String(s), Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool Graph::saveCustomPalette(const QString &name, const QList<QColor> &colors, QString *error)
{
    const QString key = name.trimmed();
    if (key.isEmpty() || colors.isEmpty()) {
        if (error)
            *error = tr("Enter a palette name and at least one colour.");
        return false;
    }
    if (isBuiltInPaletteName(key)) {
        // A custom palette under this name would never be used (the built-in one wins).
        registerPaletteTranslations();
        if (error)
            *error = tr("\"%1\" is the name of a built-in palette. Choose another name.").arg(key);
        return false;
    }

    customPaletteMemoryStore().insert(key, colors);

    QStringList hexList;
    hexList.reserve(colors.size());
    for (const QColor &c : colors) {
        if (c.isValid())
            hexList.append(c.name());
    }
    QSettings &settings = ApplicationWindow::getSettings();
    settings.beginGroup("CustomColorPalettes");
    settings.setValue(key, hexList);
    settings.endGroup();
    return true;
}

void Graph::removeCustomPalette(const QString &name)
{
    const QString key = name.trimmed();
    if (key.isEmpty())
        return;

    customPaletteMemoryStore().remove(key);
    QSettings &settings = ApplicationWindow::getSettings();
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
            const bool wasWhiteHollow = isSymbolWhiteHollow(sym);
            QPen symPen = sym.pen();
            symPen.setColor(col);
            sym.setPen(symPen);
            if (wasWhiteHollow) {
                sym.setBrush(QBrush(Qt::white, Qt::SolidPattern));
            } else if (sym.brush().style() != Qt::NoBrush) {
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
    QPointer<Graph> safeTarget(targetGraph);
    const QString beforeSnap = (safeTarget && safeTarget->multiLayerParent())
            ? safeTarget->multiLayerParent()->captureUndoSnapshot()
            : QString();

    QDialog dlg(parent);
    dlg.setWindowTitle(tr("Color Palette & Gradient Editor"));
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
    QGroupBox *colorsGroup = new QGroupBox(tr("Palette Colors (Swatches && Hex Codes)"), &dlg);
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
        QString saveError;
        if (!saveCustomPalette(name, currentColorsFromList(), &saveError)) {
            QMessageBox::warning(&dlg, tr("Save Custom Palette"), saveError);
            return;
        }
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
        if (!safeTarget)
            return;
        QList<QColor> cols = currentColorsFromList();
        if (!cols.isEmpty()) {
            safeTarget->applyColorPalette(cols, radioGradient->isChecked(),
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
    const bool changed = (res == QDialog::Accepted) || appliedAny;
    if (changed && appliedAny && safeTarget && safeTarget->multiLayerParent()
        && !beforeSnap.isEmpty()) {
        safeTarget->multiLayerParent()->pushUndoSnapshot(tr("Modify Color Bar"), beforeSnap);
    }
    return changed;
}

bool Graph::isLegendVisible() const
{
    if (legendMarkerID < 0 || !d_plot)
        return false;
    const QwtPlotMarker *m = d_plot->marker(legendMarkerID);
    return m ? (d_legend_visible && m->isVisible()) : false;
}

void Graph::setLegendVisible(bool visible)
{
    if (visible && legendMarkerID < 0 && !isPiePlot() && n_curves > 0) {
        newLegend();
        return;
    }

    d_legend_visible = visible;
    Legend *leg = legend();
    if (leg) {
        leg->setVisible(visible);
        if (!visible) {
            if (selectedMarker == legendMarkerID)
                deselectMarker();
            if (d_selection.kind == GraphSelection::LegendBox)
                setCurrentSelection({ GraphSelection::None, -1, -1 });
            const int curOrm = d_plot->property("_outsideRightMargin").toInt();
            if (curOrm > 0) {
                d_plot->setProperty("_savedOutsideRightMargin", curOrm);
                d_plot->setProperty("_outsideRightMargin", 0);
            }
        } else {
            const int savedOrm = d_plot->property("_savedOutsideRightMargin").toInt();
            if (savedOrm > 0 && d_plot->canvas() && leg->rect().right() > d_plot->canvas()->width()) {
                d_plot->setProperty("_outsideRightMargin", savedOrm);
                d_plot->setProperty("_savedOutsideRightMargin", 0);
            }
        }
    }

    d_plot->replot();
    updateSelectionOverlayGeometry();
    emit modifiedGraph();
}

bool Graph::isSymbolWhiteHollow(const QwtSymbol &sym)
{
    return sym.style() != QwtSymbol::NoSymbol
            && sym.brush().style() == Qt::SolidPattern
            && sym.brush().color() == QColor(Qt::white)
            && sym.pen().color() != QColor(Qt::white);
}

void Graph::setCurveSymbolHollow(int curveIndex, bool hollowWhite)
{
    QwtPlotCurve *c = curve(curveIndex);
    if (!c)
        return;

    QwtSymbol sym = c->symbol();
    QColor curveColor = c->pen().color();
    if (!curveColor.isValid() || (hollowWhite && curveColor == QColor(Qt::white))) {
        const QColor spCol = sym.pen().color();
        curveColor = (spCol.isValid() && spCol != QColor(Qt::white)) ? spCol : QColor(Qt::black);
    }

    if (hollowWhite) {
        if (sym.style() == QwtSymbol::NoSymbol) {
            sym.setStyle(QwtSymbol::Ellipse);
            sym.setSize(9);
        } else if (sym.size().width() <= 0) {
            sym.setSize(9);
        }
        QPen sp = sym.pen();
        sp.setColor(curveColor);
        if (sp.widthF() <= 0.0)
            sp.setWidthF(c->pen().widthF() > 0.0 ? c->pen().widthF() : 1.0);
        sym.setPen(sp);
        sym.setBrush(QBrush(Qt::white, Qt::SolidPattern));
        if (curveIndex >= 0 && curveIndex < c_type.size() && c_type[curveIndex] == Line
            && c->style() != QwtPlotCurve::NoCurve) {
            c_type[curveIndex] = LineSymbols;
        }
    } else {
        QPen sp = sym.pen();
        sp.setColor(curveColor);
        sym.setPen(sp);
        sym.setBrush(QBrush(curveColor, Qt::SolidPattern));
    }

    c->setSymbol(sym);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::applySeriesStyleScheme(SeriesStyleScheme scheme)
{
    QList<int> targetIndices;
    for (int i = 0; i < n_curves && i < c_type.size(); ++i) {
        if (!isCurveVisible(i))
            continue;
        const int ct = c_type[i];
        if (ct == ErrorBars || ct == ColorMap || ct == GrayMap || ct == ContourMap || ct == Pie)
            continue;
        QwtPlotCurve *c = curve(i);
        if (c && c->rtti() != QwtPlotItem::Rtti_PlotSpectrogram)
            targetIndices.append(i);
    }
    if (targetIndices.isEmpty())
        return;

    struct SymbolStepSpec
    {
        QColor color;
        QwtSymbol::Style shape;
        bool whiteHollow;
        bool forceSymbol;
    };

    const int m = targetIndices.size();
    QVector<SymbolStepSpec> specs;
    specs.reserve(m);

    switch (scheme) {
    case SchemeBW2to3Group: {
        static const QPair<QwtSymbol::Style, bool> kCycle[] = {
            { QwtSymbol::Ellipse, false },
            { QwtSymbol::Ellipse, true },
            { QwtSymbol::Rect, true },
            { QwtSymbol::UTriangle, true },
            { QwtSymbol::Diamond, false },
            { QwtSymbol::DTriangle, true }
        };
        for (int k = 0; k < m; ++k) {
            const auto &step = kCycle[k % 6];
            specs.push_back({ QColor(QStringLiteral("#000000")), step.first, step.second, true });
        }
        break;
    }
    case SchemeBlueFilledRedHollow: {
        struct Entry { const char *hex; QwtSymbol::Style st; bool hollow; };
        static const Entry kCycle[] = {
            { "#1F77B4", QwtSymbol::Ellipse, false },
            { "#D62728", QwtSymbol::Ellipse, true },
            { "#555555", QwtSymbol::Rect, true },
            { "#2CA02C", QwtSymbol::UTriangle, true },
            { "#9467BD", QwtSymbol::Diamond, false },
            { "#8C564B", QwtSymbol::DTriangle, true }
        };
        for (int k = 0; k < m; ++k) {
            const Entry &e = kCycle[k % 6];
            specs.push_back({ QColor(QString::fromLatin1(e.hex)), e.st, e.hollow, true });
        }
        break;
    }
    case SchemeMutedCategorical: {
        static const char *const kHex[] = {
            "#1F77B4", "#D62728", "#555555", "#2CA02C", "#9467BD", "#8C564B"
        };
        static const QwtSymbol::Style kShapes[] = {
            QwtSymbol::Ellipse, QwtSymbol::Rect, QwtSymbol::UTriangle,
            QwtSymbol::Diamond, QwtSymbol::DTriangle, QwtSymbol::Cross
        };
        for (int k = 0; k < m; ++k) {
            const int cIdx = targetIndices[k];
            const QwtPlotCurve *c = curve(cIdx);
            const int ct = (cIdx >= 0 && cIdx < c_type.size()) ? c_type[cIdx] : LineSymbols;
            const bool hasSym = c
                    && (c->symbol().style() != QwtSymbol::NoSymbol || ct == Scatter
                        || ct == LineSymbols);
            const bool keepHollow = c ? isSymbolWhiteHollow(c->symbol()) : false;
            specs.push_back({ QColor(QString::fromLatin1(kHex[k % 6])),
                              hasSym ? kShapes[k % 6] : QwtSymbol::NoSymbol, keepHollow,
                              hasSym });
        }
        break;
    }
    case SchemeCoolToWarmStep: {
        const QList<QColor> anchors = {
            QColor(QStringLiteral("#2166AC")),
            QColor(QStringLiteral("#4393C3")),
            QColor(QStringLiteral("#68788C")),
            QColor(QStringLiteral("#D6604D")),
            QColor(QStringLiteral("#B2182B"))
        };
        const QList<QColor> interp = interpolatePalette(anchors, m);
        for (int k = 0; k < m; ++k) {
            const QwtPlotCurve *c = curve(targetIndices[k]);
            const QwtSymbol sym = c ? c->symbol() : QwtSymbol();
            specs.push_back({ interp[k % interp.size()], sym.style(), isSymbolWhiteHollow(sym),
                              false });
        }
        break;
    }
    case SchemeViridisSequential: {
        // Viridis t in [0.0, 0.84]: #440154 -> #31688E -> #35B779 -> #6DCD59 (avoids pale yellow)
        const QList<QColor> anchors = {
            QColor(QStringLiteral("#440154")),
            QColor(QStringLiteral("#3B528B")),
            QColor(QStringLiteral("#31688E")),
            QColor(QStringLiteral("#21918C")),
            QColor(QStringLiteral("#35B779")),
            QColor(QStringLiteral("#6DCD59"))
        };
        const QList<QColor> interp = interpolatePalette(anchors, m);
        for (int k = 0; k < m; ++k) {
            const QwtPlotCurve *c = curve(targetIndices[k]);
            const QwtSymbol sym = c ? c->symbol() : QwtSymbol();
            specs.push_back({ interp[k % interp.size()], sym.style(), isSymbolWhiteHollow(sym),
                              false });
        }
        break;
    }
    case SchemeOriginClassic:
    default: {
        const QList<QColor> &originCols = originClassicColors();
        for (int k = 0; k < m; ++k) {
            const QwtPlotCurve *c = curve(targetIndices[k]);
            const QwtSymbol sym = c ? c->symbol() : QwtSymbol();
            specs.push_back({ originCols.at(k % originCols.size()), sym.style(),
                              isSymbolWhiteHollow(sym), false });
        }
        break;
    }
    }

    for (int k = 0; k < m; ++k) {
        const int idx = targetIndices[k];
        QwtPlotCurve *c = curve(idx);
        if (!c)
            continue;
        const SymbolStepSpec &sp = specs[k];

        QPen pen = c->pen();
        pen.setColor(sp.color);
        c->setPen(pen);

        if (c_type[idx] == VectXYXY || c_type[idx] == VectXYAM) {
            if (VectorCurve *vc = dynamic_cast<VectorCurve *>(c))
                vc->setColor(sp.color);
        }

        if (c->brush().style() != Qt::NoBrush) {
            QBrush br = c->brush();
            QColor fillCol = sp.color;
            if (br.color().alpha() < 255)
                fillCol.setAlpha(br.color().alpha());
            br.setColor(fillCol);
            c->setBrush(br);
        }

        QwtSymbol sym = c->symbol();
        if (sp.forceSymbol) {
            const int ct = c_type[idx];
            if (ct == Line || ct == Scatter || ct == LineSymbols || ct == Spline) {
                sym.setStyle(sp.shape);
                if (sym.size().width() <= 0)
                    sym.setSize(9);
                QPen symPen = sym.pen();
                symPen.setColor(sp.color);
                if (symPen.widthF() <= 0.0)
                    symPen.setWidthF(pen.widthF() > 0.0 ? pen.widthF() : 1.0);
                sym.setPen(symPen);
                sym.setBrush(sp.whiteHollow ? QBrush(Qt::white, Qt::SolidPattern)
                                            : QBrush(sp.color, Qt::SolidPattern));
                c->setSymbol(sym);
                if (ct == Line && c->style() != QwtPlotCurve::NoCurve)
                    c_type[idx] = LineSymbols;
            }
        } else if (sym.style() != QwtSymbol::NoSymbol) {
            QPen symPen = sym.pen();
            symPen.setColor(sp.color);
            sym.setPen(symPen);
            if (sp.whiteHollow) {
                sym.setBrush(QBrush(Qt::white, Qt::SolidPattern));
            } else if (sym.brush().style() != Qt::NoBrush) {
                QBrush sb = sym.brush();
                sb.setColor(sp.color);
                sym.setBrush(sb);
            }
            c->setSymbol(sym);
        }

        if (DataCurve *dc = dynamic_cast<DataCurve *>(c)) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (QwtErrorPlotCurve *errCurve = dynamic_cast<QwtErrorPlotCurve *>(eb))
                    errCurve->setColor(sp.color);
            }
        }
    }

    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::applyTemplateCurveStyle(int curveIndex, int styleIndex)
{
    if (d_template_curve_styles.isEmpty() || curveIndex < 0 || curveIndex >= n_curves)
        return false;
    if (curveIndex < c_type.size() && c_type[curveIndex] == ErrorBars)
        return false;
    QwtPlotCurve *cv = curve(curveIndex);
    if (!cv)
        return false;

    const int sIdx = (styleIndex >= 0) ? (styleIndex % d_template_curve_styles.size())
                                       : (curveIndex % d_template_curve_styles.size());
    const TemplateCurveStyle &st = d_template_curve_styles[sIdx];

    QPen pen = cv->pen();
    pen.setStyle(static_cast<Qt::PenStyle>(st.penStyle));
    if (st.lineWidth >= 0.0)
        pen.setWidthF(st.lineWidth);
    if (st.lineColor.isValid())
        pen.setColor(st.lineColor);
    cv->setPen(pen);

    QBrush br = cv->brush();
    br.setStyle(static_cast<Qt::BrushStyle>(st.brushStyle));
    if (st.brushColor.isValid())
        br.setColor(st.brushColor);
    cv->setBrush(br);

    QwtSymbol sym = cv->symbol();
    sym.setStyle(static_cast<QwtSymbol::Style>(st.symbolStyle));
    if (st.symbolSize > 0)
        sym.setSize(st.symbolSize);
    QPen symPen = sym.pen();
    if (st.symbolPenWidth >= 0.0)
        symPen.setWidthF(st.symbolPenWidth);
    if (st.symbolEdgeColor.isValid())
        symPen.setColor(st.symbolEdgeColor);
    sym.setPen(symPen);
    if (st.symbolHollow) {
        sym.setBrush(QBrush(Qt::white));
    } else if (st.symbolFillColor.isValid()) {
        QBrush symBr = sym.brush();
        if (symBr.style() == Qt::NoBrush && st.symbolStyle != int(QwtSymbol::NoSymbol))
            symBr.setStyle(Qt::SolidPattern);
        symBr.setColor(st.symbolFillColor);
        sym.setBrush(symBr);
    }
    cv->setSymbol(sym);

    if (PlotCurve *pc = dynamic_cast<PlotCurve *>(cv))
        pc->setSymbolSkip(st.symbolSkip);
    if (QwtBarCurve *bc = dynamic_cast<QwtBarCurve *>(cv)) {
        bc->setGap(st.barGap);
        bc->setOffset(st.barOffset);
    }
    if (curveIndex < c_type.size()) {
        const int curType = c_type[curveIndex];
        const bool curXY = (curType == Line || curType == Scatter || curType == LineSymbols
                            || curType == Spline || curType == Area || curType == VerticalSteps
                            || curType == HorizontalSteps || curType == VerticalDropLines);
        const bool tplXY = (st.curveType == Line || st.curveType == Scatter
                            || st.curveType == LineSymbols || st.curveType == Spline
                            || st.curveType == Area || st.curveType == VerticalSteps
                            || st.curveType == HorizontalSteps
                            || st.curveType == VerticalDropLines);
        if (curXY && tplXY) {
            c_type[curveIndex] = st.curveType;
            switch (st.curveType) {
            case Scatter:
                cv->setStyle(QwtPlotCurve::NoCurve);
                break;
            case VerticalDropLines:
                cv->setStyle(QwtPlotCurve::Sticks);
                break;
            case VerticalSteps:
                cv->setStyle(QwtPlotCurve::Steps);
                cv->setCurveAttribute(QwtPlotCurve::Inverted, false);
                break;
            case HorizontalSteps:
                cv->setStyle(QwtPlotCurve::Steps);
                cv->setCurveAttribute(QwtPlotCurve::Inverted, true);
                break;
            case Spline:
                cv->setStyle(QwtPlotCurve::Lines);
                cv->setCurveAttribute(QwtPlotCurve::Fitted, true);
                break;
            default:
                cv->setStyle(QwtPlotCurve::Lines);
                cv->setCurveAttribute(QwtPlotCurve::Fitted, false);
                break;
            }
        }
    }
    return true;
}

void Graph::applyTemplateCurveStylesToAll()
{
    if (d_template_curve_styles.isEmpty())
        return;
    int seriesIdx = 0;
    bool anyApplied = false;
    for (int ci = 0; ci < n_curves; ++ci) {
        if (ci < c_type.size() && c_type[ci] == ErrorBars)
            continue;
        if (applyTemplateCurveStyle(ci, seriesIdx++))
            anyApplied = true;
    }
    if (anyApplied && d_plot) {
        d_plot->replot();
        emit modifiedGraph();
    }
}

bool Graph::repositionAnchoredOutsideRightLegend()
{
    if (!d_plot || !d_plot->canvas())
        return false;
    Legend *leg = legend();
    if (!leg || !isLegendVisible() || !leg->isVisible())
        return false;
    const QVariant ancVar = d_plot->property("_toriLegendAnchor");
    if (!ancVar.isValid() || ancVar.toInt() != int(LegendOutsideRight))
        return false;

    int rightAxisSpan = 0;
    if (d_plot->axisEnabled(QwtPlot::yRight) && d_plot->axisWidget(QwtPlot::yRight)) {
        QwtScaleWidget *rw = d_plot->axisWidget(QwtPlot::yRight);
        const QwtScaleDraw *rsd = d_plot->axisScaleDraw(QwtPlot::yRight);
        const bool hasRightContent =
                (rsd && rsd->hasComponent(QwtAbstractScaleDraw::Labels))
                || !d_plot->axisTitle(QwtPlot::yRight).text().trimmed().isEmpty()
                || rw->isColorBarEnabled();
        if (hasRightContent)
            rightAxisSpan = rw->width();
    }

    const double z = qMax(0.25, d_plot->displayFontScale());
    const int baseGap = d_plot->property("_toriOutsideRightGapPx").isValid()
            ? qMax(2, d_plot->property("_toriOutsideRightGapPx").toInt())
            : 12;
    const int baseTop = d_plot->property("_toriOutsideRightTopPx").isValid()
            ? qMax(2, d_plot->property("_toriOutsideRightTopPx").toInt())
            : 12;
    const int gapPx = qMax(2, qRound(baseGap * z));
    const int topPx = qMax(2, qRound(baseTop * z));
    const int cw = d_plot->canvas()->width();
    leg->setOrigin(QPoint(cw + rightAxisSpan + gapPx, topPx));
    return true;
}

QString Graph::placeLegendAnchor(LegendAnchor anchor, double *outOverlapScore, int marginPx)
{
    if (outOverlapScore)
        *outOverlapScore = 0.0;
    if (!d_plot || !d_plot->canvas())
        return QString();

    Legend *leg = legend();
    if (!leg)
        return QString();

    ScopedUnzoomForExport unzoomGuard(multiLayerParent());

    if (!isLegendVisible()) {
        d_legend_visible = true;
        leg->setVisible(true);
    }

    const int m = qMax(2, marginPx);
    d_plot->setProperty("_toriLegendAnchor", int(anchor));

    if (anchor == LegendOutsideRight) {
        d_plot->setProperty("_outsideRightMargin", 0);
        d_plot->setProperty("_savedOutsideRightMargin", 0);
        d_plot->updateAxes();
        d_plot->updateLayout();

        const QRect initialRect = leg->rect();
        const int legW = qMax(16, initialRect.width());
        const int gapPx = qMax(8, m);
        const int neededMargin = legW + gapPx + 6;
        d_plot->setProperty("_outsideRightMargin", neededMargin);
        d_plot->updateLayout();

        d_plot->setProperty("_toriOutsideRightGapPx", gapPx);
        d_plot->setProperty("_toriOutsideRightTopPx", m);
        // Replot first so axes and canvasMap settle at the shrunk _outsideRightMargin width
        // before converting pixel origin -> data coordinates in setOrigin().
        d_plot->replot();
        repositionAnchoredOutsideRightLegend();
        d_plot->replot();
        unzoomGuard.restoreNow();
        updateSelectionOverlayGeometry();
        emit modifiedGraph();
        return QStringLiteral("OutsideRight");
    }

    if (d_plot->property("_outsideRightMargin").toInt() != 0) {
        d_plot->setProperty("_outsideRightMargin", 0);
        d_plot->setProperty("_savedOutsideRightMargin", 0);
    }
    d_plot->setProperty("_toriOutsideRightGapPx", QVariant());
    d_plot->setProperty("_toriOutsideRightTopPx", QVariant());
    d_plot->updateAxes();
    d_plot->updateLayout();

    int cw = d_plot->canvas()->width();
    int ch = d_plot->canvas()->height();

    const int majLen = d_plot->majorTickLength();
    bool framelessSingleCol =
            (leg->frameStyle() == Legend::None && leg->numColumns() <= 1);

    int legW = 16;
    int legH = 12;
    int effLegW = 16;
    QVector<QRectF> rowVisualRects;
    const double dpiRatio =
            qMax(1.0, 96.0 / double(qMax(36, d_plot->logicalDpiX())));

    auto recomputeLegendMetrics = [&]() {
        const QRect lr = leg->rect();
        legW = qMax(16, int(std::ceil(lr.width() * dpiRatio)));
        legH = qMax(12, int(std::ceil(lr.height() * dpiRatio)));
        effLegW = legW;
        rowVisualRects.clear();

        if (!framelessSingleCol)
            return;

        QString norm = leg->text();
        norm.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        norm.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        while (norm.endsWith(QLatin1Char('\n')) && norm.size() > 1)
            norm.chop(1);
        const QStringList rawLines = norm.split(QLatin1Char('\n'));

        const QFont storedLegFnt = leg->font();
        const QFont dispLegFnt = d_plot ? d_plot->displayFont(storedLegFnt) : storedLegFnt;
        const double zoomScale = d_plot ? qMax(0.25, d_plot->displayFontScale()) : 1.0;
        int maxSymW = 0;
        bool allScatter = true;
        bool anyCurveTok = false;
        QStringList visLines;
        for (const QString &ln : rawLines) {
            int cvIdx = -1;
            if (ln.contains(QLatin1String("\\c{"))) {
                const int p1 = ln.indexOf(QLatin1String("\\c{"));
                const int p2 = ln.indexOf(QLatin1Char('}'), p1 + 3);
                if (p1 >= 0 && p2 > p1 + 3)
                    cvIdx = ln.mid(p1 + 3, p2 - (p1 + 3)).toInt() - 1;
            } else if (ln.contains(QLatin1String("\\l("))) {
                const int p1 = ln.indexOf(QLatin1String("\\l("));
                const int p2 = ln.indexOf(QLatin1Char(')'), p1 + 3);
                if (p1 >= 0 && p2 > p1 + 3)
                    cvIdx = ln.mid(p1 + 3, p2 - (p1 + 3)).toInt() - 1;
            }
            if (cvIdx >= 0 && cvIdx < n_curves) {
                QwtPlotItem *it = plotItem(cvIdx);
                if (it && !it->isVisible())
                    continue;
                anyCurveTok = true;
                if (const QwtPlotCurve *cv = curve(cvIdx)) {
                    if (cv->style() != QwtPlotCurve::NoCurve || cv->brush().style() != Qt::NoBrush)
                        allScatter = false;
                    if (cv->symbol().style() != QwtSymbol::NoSymbol)
                        maxSymW = qMax(maxSymW, qBound(3, cv->symbol().size().width(), 15));
                }
            }
            visLines.append(ln);
        }

        const double legPt = storedLegFnt.pointSizeF();
        const bool compactLeg =
                (legPt > 0.0) ? (legPt <= 9.5) : (QFontMetricsF(storedLegFnt).height() <= 16.0);
        const int baseSymLineLen = (anyCurveTok && allScatter)
                ? qMax(maxSymW + (compactLeg ? 2 : 0), compactLeg ? 11 : 14)
                : (compactLeg
                           ? qBound(14,
                                    qMax(maxSymW + 6,
                                         qRound(QFontMetricsF(storedLegFnt).height() * 1.05)),
                                    34)
                           : qBound(16,
                                    qMax(maxSymW + 8,
                                         qRound(QFontMetricsF(storedLegFnt).height() * 1.45)),
                                    34));
        const int sampleOff =
                qMax(8, qRound((baseSymLineLen + (compactLeg ? 5 : 10)) * zoomScale));

        int curTop = qMax(2, qRound(4.0 * zoomScale)); // Legend::top_margin
        double maxRight = 16.0;
        for (const QString &ln : visLines) {
            bool hasTok = false;
            QString parsed = ln;
            if (parsed.contains(QLatin1String("\\c{")) || parsed.contains(QLatin1String("\\p{"))
                || parsed.contains(QLatin1String("\\l("))) {
                hasTok = true;
                int pEnd = parsed.indexOf(parsed.contains(QLatin1String("\\l("))
                                                  ? QLatin1Char(')')
                                                  : QLatin1Char('}'));
                if (pEnd >= 0)
                    parsed = parsed.mid(pEnd + 1);
            }
            int sFrom = 0;
            int pPct = -1;
            while ((pPct = parsed.indexOf(QLatin1String("%("), sFrom)) >= 0) {
                const int pPct2 = parsed.indexOf(QLatin1Char(')'), pPct + 2);
                if (pPct2 < 0)
                    break;
                bool ok = false;
                const int ci = parsed.mid(pPct + 2, pPct2 - pPct - 2).toInt(&ok) - 1;
                if (ok && ci >= 0 && curve(ci)) {
                    const QString repl = curve(ci)->title().text();
                    parsed.replace(pPct, pPct2 - pPct + 1, repl);
                    sFrom = pPct + repl.size();
                } else {
                    sFrom = pPct2 + 1;
                }
            }
            QwtText qt(parsed);
            qt.setFont(dispLegFnt);
            const QSize tsz = qt.textSize();
            const int rw = int(std::ceil((hasTok ? sampleOff : 0) + qMax(4, tsz.width()) * dpiRatio));
            const int rh = qMax(8, int(std::ceil(tsz.height() * dpiRatio)));
            const double leftOff = 6.0 * zoomScale * dpiRatio;
            const QRectF rr(leftOff, double(curTop), double(rw), double(rh));
            rowVisualRects.append(rr);
            maxRight = qMax(maxRight, rr.right());
            curTop += rh;
        }
        if (!rowVisualRects.isEmpty()) {
            effLegW = qBound(16, int(std::ceil(maxRight)), legW);
            legH = qMax(legH, curTop + int(std::ceil(4.0 * zoomScale * dpiRatio)));
        }
    };

    recomputeLegendMetrics();

    if (!isPiePlot() && leg->numColumns() > 1) {
        const int estML = qMax(2, majLen - 4);
        const int estMR = qMax(6, majLen + 5);
        if (legW > qMax(24, cw - estML - estMR)) {
            leg->setNumColumns(1);
            framelessSingleCol = (leg->frameStyle() == Legend::None && leg->numColumns() <= 1);
            d_plot->updateLayout();
            recomputeLegendMetrics();
        }
    }
    if (!isPiePlot() && !d_is_ternary && leg->numColumns() <= 1 && cw >= 60) {
        const int maxFitLegW = qMax(36, cw - 10);
        if (effLegW > maxFitLegW) {
            QFont lf = leg->font();
            double curPt = lf.pointSizeF();
            if (curPt <= 0.0 && lf.pointSize() > 0)
                curPt = double(lf.pointSize());
            const double floorPt = (curPt > 14.0) ? 16.0 : 7.0;
            while (effLegW > maxFitLegW && curPt > floorPt + 0.1) {
                curPt = qMax(floorPt, curPt - ((curPt > 14.0) ? 1.0 : 0.5));
                lf.setPointSizeF(curPt);
                lf.setHintingPreference(QFont::PreferNoHinting);
                leg->setFont(lf);
                recomputeLegendMetrics();
            }
        }
    }

    if (isPiePlot()) {
        if (leg->text().contains(QLatin1String("\\p{"))
            && leg->text().contains(QLatin1Char('%'))) {
            static const QRegularExpression kPiePctRx(
                    QStringLiteral("^\\s*\\d+(?:\\.\\d+)?%\\s*$"));
            for (int tid : d_texts) {
                if (tid == legendMarkerID)
                    continue;
                Legend *tm = dynamic_cast<Legend *>(d_plot->marker(tid));
                if (tm && kPiePctRx.match(tm->text()).hasMatch()) {
                    tm->setText(QString());
                    tm->setVisible(false);
                }
            }
        }
        if ((anchor == LegendTopRight || anchor == LegendAutoAvoidData)
            && effLegW > int(0.34 * double(cw))) {
            anchor = LegendTopCenter;
            d_plot->setProperty("_toriLegendAnchor", int(anchor));
        }
    }

    const bool framelessLeg = (leg->frameStyle() == Legend::None);
    const int mL = framelessSingleCol ? qMax(2, majLen - 4) : qMax(m, majLen + 4);
    const int mR = framelessSingleCol ? qMax(6, majLen + 5) : qMax(m, majLen + 4);
    const int mT = isPiePlot() ? 4 : (framelessLeg ? qMax(3, majLen - 2) : qMax(m, majLen + 4));
    const int mB = framelessLeg ? qMax(3, majLen - 1) : qMax(m, majLen + 4);

    auto originForInsideAnchor = [&](LegendAnchor a) -> QPoint {
        const int xLeft = mL;
        const int xRight = qMax(2, cw - effLegW - mR);
        const int xCenter = qMax(2, (cw - effLegW) / 2);
        const int yTop = mT;
        const int yBottom = qMax(2, ch - legH - mB);
        switch (a) {
        case LegendTopRight:
            return QPoint(xRight, yTop);
        case LegendTopLeft:
            return QPoint(xLeft, yTop);
        case LegendBottomRight:
            return QPoint(xRight, yBottom);
        case LegendBottomLeft:
            return QPoint(xLeft, yBottom);
        case LegendTopCenter:
            return QPoint(xCenter, yTop);
        case LegendBottomCenter:
            return QPoint(xCenter, yBottom);
        default:
            return QPoint(xRight, yTop);
        }
    };

    auto anchorName = [](LegendAnchor a) -> QString {
        switch (a) {
        case LegendTopRight:
            return QStringLiteral("TopRight");
        case LegendTopLeft:
            return QStringLiteral("TopLeft");
        case LegendBottomRight:
            return QStringLiteral("BottomRight");
        case LegendBottomLeft:
            return QStringLiteral("BottomLeft");
        case LegendTopCenter:
            return QStringLiteral("TopCenter");
        case LegendBottomCenter:
            return QStringLiteral("BottomCenter");
        case LegendOutsideRight:
            return QStringLiteral("OutsideRight");
        default:
            return QStringLiteral("TopRight");
        }
    };

    auto liangBarskyClippedLength = [](double x0, double y0, double x1, double y1,
                                       const QRectF &box, bool *outHit) -> double {
        if (outHit)
            *outHit = false;
        const double dx = x1 - x0;
        const double dy = y1 - y0;
        double t0 = 0.0;
        double t1 = 1.0;

        auto clipEdge = [&](double p, double q) -> bool {
            if (std::fabs(p) < 1e-12)
                return q >= 0.0;
            const double r = q / p;
            if (p < 0.0) {
                if (r > t1)
                    return false;
                if (r > t0)
                    t0 = r;
            } else {
                if (r < t0)
                    return false;
                if (r < t1)
                    t1 = r;
            }
            return true;
        };

        if (!clipEdge(-dx, x0 - box.left()))
            return 0.0;
        if (!clipEdge(dx, box.right() - x0))
            return 0.0;
        if (!clipEdge(-dy, y0 - box.top()))
            return 0.0;
        if (!clipEdge(dy, box.bottom() - y0))
            return 0.0;
        if (t0 > t1)
            return 0.0;

        if (outHit)
            *outHit = true;
        return std::hypot((t1 - t0) * dx, (t1 - t0) * dy);
    };

    QList<Graph *> layersToScore;
    layersToScore << this;
    if (MultiLayer *ml = multiLayerParent()) {
        const QList<Graph *> allGraphs = ml->layersList();
        for (Graph *childGr : allGraphs) {
            if (childGr && childGr != this && childGr->linkedParentLayer() == this)
                layersToScore << childGr;
        }
    }

    // Screen geometry of the standard line/symbol curves (part 5 of evaluateCollisionScore),
    // projected once and indexed by a uniform grid of cells. The placement search scores
    // thousands of candidate boxes (25 x 25 grid origins, each box and each legend row), and
    // walking every point of every curve for each of them made plot_xy with many Y columns
    // quadratic in the curve count. A box now only visits the symbols and segments of the
    // cells it touches; each one is scored by the same test as before. The projection is
    // rebuilt whenever an axis scale or the canvas changes (Y headroom, 2-column legend).
    struct StdCurveElem {
        bool isSeg;
        double x0, y0, x1, y1; // symbol: (x0, y0); segment: (x0, y0)-(x1, y1)
        double r; // symbol test radius
        double weight; // symbol: added cost; segment: pen weight
        double stride;
    };
    struct StdCurveGeom {
        QVector<double> signature;
        bool built = false;
        QVector<StdCurveElem> elems;
        QVector<int> outside; // elements not inside the indexed area: always tested
        // per layer (layersToScore order): curves evaluateCollisionScore still walks itself
        QVector<QVector<int>> otherCurves;
        struct Cell {
            QVector<int> singles; // elements (padded box) inside this cell only
            QVector<int> multis; // elements spanning several cells
            double fullSum = 0.0; // what the singles add when the box covers the whole cell
        };
        QVector<Cell> cells;
        QVector<int> stamp;
        int stampId = 0;
        double gx0 = 0.0, gy0 = 0.0, cell = 16.0;
        int nx = 0, ny = 0;
    } stdGeom;
    // Set only while the Y headroom asks what the legend still meets when this layer's yLeft
    // curves are left out (they are what widening the left axis moves).
    bool scoreSkipsYLeftCurves = false;
    auto stdGeomSignature = [&]() {
        QVector<double> sig;
        sig << (scoreSkipsYLeftCurves ? 1.0 : 0.0);
        for (Graph *lg : layersToScore) {
            Plot *lp = lg ? lg->plotWidget() : nullptr;
            if (!lp || !lp->canvas())
                continue;
            sig << lp->canvas()->width() << lp->canvas()->height();
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                const QwtScaleMap m = lp->canvasMap(a);
                sig << m.s1() << m.s2() << m.p1() << m.p2();
            }
        }
        return sig;
    };
    auto ensureStdGeom = [&]() {
        QVector<double> sig = stdGeomSignature();
        if (stdGeom.built && sig == stdGeom.signature)
            return;
        stdGeom.signature = sig;
        stdGeom.built = true;
        stdGeom.elems.clear();
        stdGeom.outside.clear();
        stdGeom.otherCurves = QVector<QVector<int>>(layersToScore.size());
        double maxW = 1.0, maxH = 1.0;
        for (int li = 0; li < layersToScore.size(); ++li) {
            Graph *lg = layersToScore[li];
            Plot *lp = lg ? lg->plotWidget() : nullptr;
            if (!lp)
                continue;
            if (lp->canvas()) {
                maxW = qMax(maxW, double(lp->canvas()->width()));
                maxH = qMax(maxH, double(lp->canvas()->height()));
            }
            for (int i = 0; i < lg->n_curves && i < lg->c_keys.size(); ++i) {
                // Curves that are not plain line/symbol curves (and hidden ones, which cost
                // nothing) stay with evaluateCollisionScore.
                if (!lg->isCurveVisible(i)) {
                    stdGeom.otherCurves[li].append(i);
                    continue;
                }
                QwtPlotItem *it = lg->plotItem(i);
                if (scoreSkipsYLeftCurves && lg == this && it && it->yAxis() == QwtPlot::yLeft)
                    continue;
                if (!it || !it->isVisible() || it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                    stdGeom.otherCurves[li].append(i);
                    continue;
                }
                const int ct = (i < lg->c_type.size()) ? lg->c_type[i] : Line;
                if (ct == ErrorBars || ct == VerticalBars || ct == HorizontalBars
                    || ct == Histogram || ct == Box || ct == VectXYXY || ct == VectXYAM) {
                    stdGeom.otherCurves[li].append(i);
                    continue;
                }
                QwtPlotCurve *c = dynamic_cast<QwtPlotCurve *>(it);
                if (!c)
                    continue; // costs nothing in evaluateCollisionScore either
                const int nPts = c->dataSize();
                if (nPts <= 0)
                    continue;
                const QwtScaleMap xMap = lp->canvasMap(c->xAxis());
                const QwtScaleMap yMap = lp->canvasMap(c->yAxis());
                const DataCurve *dc = dynamic_cast<const DataCurve *>(c);
                const bool hasSymbols =
                        (c->symbol().style() != QwtSymbol::NoSymbol || ct == Scatter);
                const bool hasLines = (c->style() != QwtPlotCurve::NoCurve && ct != Scatter);
                const double symR = hasSymbols
                        ? qMax(4.0,
                               0.5 * qMax(c->symbol().size().width(), c->symbol().size().height()))
                        : 0.0;
                const double penWeight = 1.4 + 0.35 * qMax(1, c->pen().width());
                const int stride = (nPts > 6000) ? qMax(1, nPts / 3000) : 1;
                Column *dcXCol = nullptr;
                Column *dcYCol = nullptr;
                const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
                if (dcHasMasks)
                    dc->resolveColumns(dcXCol, dcYCol);
                bool hasPrev = false;
                double prevPx = 0.0, prevPy = 0.0;
                for (int p = 0; p < nPts; p += stride) {
                    if (dcHasMasks && dc->isPointMaskedFast(p, dcXCol, dcYCol)) {
                        hasPrev = false;
                        continue;
                    }
                    const double vx = c->x(p);
                    const double vy = c->y(p);
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
                    if (hasSymbols)
                        stdGeom.elems.append({ false, px, py, px, py, symR,
                                               (45.0 + symR * 4.0) * stride, double(stride) });
                    if (hasLines && hasPrev)
                        stdGeom.elems.append({ true, prevPx, prevPy, px, py, 0.0, penWeight,
                                               double(stride) });
                    prevPx = px;
                    prevPy = py;
                    hasPrev = true;
                }
            }
        }
        // Index the canvas plus one canvas size on every side; anything reaching beyond that
        // is kept in the always-tested list.
        stdGeom.gx0 = -maxW;
        stdGeom.gy0 = -maxH;
        stdGeom.cell = qMax(16.0, qMax(3.0 * maxW, 3.0 * maxH) / 256.0);
        stdGeom.nx = qMax(1, int(std::ceil(3.0 * maxW / stdGeom.cell)));
        stdGeom.ny = qMax(1, int(std::ceil(3.0 * maxH / stdGeom.cell)));
        stdGeom.cells = QVector<StdCurveGeom::Cell>(stdGeom.nx * stdGeom.ny);
        stdGeom.stamp = QVector<int>(stdGeom.elems.size(), 0);
        stdGeom.stampId = 0;
        const double gx1 = stdGeom.gx0 + stdGeom.nx * stdGeom.cell;
        const double gy1 = stdGeom.gy0 + stdGeom.ny * stdGeom.cell;
        for (int e = 0; e < stdGeom.elems.size(); ++e) {
            const StdCurveElem &el = stdGeom.elems[e];
            const double pad = el.r + 1.0;
            const double l = qMin(el.x0, el.x1) - pad;
            const double r = qMax(el.x0, el.x1) + pad;
            const double t = qMin(el.y0, el.y1) - pad;
            const double b = qMax(el.y0, el.y1) + pad;
            if (l < stdGeom.gx0 || t < stdGeom.gy0 || r >= gx1 || b >= gy1) {
                stdGeom.outside.append(e);
                continue;
            }
            const int cx0 = int((l - stdGeom.gx0) / stdGeom.cell);
            const int cx1 = int((r - stdGeom.gx0) / stdGeom.cell);
            const int cy0 = int((t - stdGeom.gy0) / stdGeom.cell);
            const int cy1 = int((b - stdGeom.gy0) / stdGeom.cell);
            if (cx0 == cx1 && cy0 == cy1) {
                StdCurveGeom::Cell &cl = stdGeom.cells[cy0 * stdGeom.nx + cx0];
                cl.singles.append(e);
                // Inside a box that covers the whole cell a symbol always counts and a segment
                // is never clipped (the same values the tests below give).
                const double segLen = std::hypot(el.x1 - el.x0, el.y1 - el.y0);
                cl.fullSum += el.isSeg ? (6.0 + segLen * el.weight) * el.stride : el.weight;
                continue;
            }
            for (int cy = cy0; cy <= cy1; ++cy)
                for (int cx = cx0; cx <= cx1; ++cx)
                    stdGeom.cells[cy * stdGeom.nx + cx].multis.append(e);
        }
    };
    auto stdGeomCost = [&](const QRectF &box) -> double {
        ensureStdGeom();
        double cost = 0.0;
        auto scoreElem = [&](const StdCurveElem &el) {
            if (!el.isSeg) {
                if (box.adjusted(-el.r, -el.r, el.r, el.r).contains(QPointF(el.x0, el.y0)))
                    cost += el.weight;
            } else {
                bool hit = false;
                const double clipLen =
                        liangBarskyClippedLength(el.x0, el.y0, el.x1, el.y1, box, &hit);
                if (hit)
                    cost += (6.0 + clipLen * el.weight) * el.stride;
            }
        };
        // Elements reaching outside the indexed area are always tested; every other element
        // that meets the box meets the part of the box inside that area, so only the cells
        // under that part are visited (a tall legend box may extend past the area).
        for (int e : stdGeom.outside)
            scoreElem(stdGeom.elems[e]);
        if (++stdGeom.stampId == std::numeric_limits<int>::max()) {
            stdGeom.stamp.fill(0);
            stdGeom.stampId = 1;
        }
        auto cellIndex = [&](double v, double origin, int n) {
            const double c = std::floor((v - origin) / stdGeom.cell);
            return int(qBound(-1.0, c, double(n)));
        };
        const int cx0 = qMax(0, cellIndex(box.left() - 1.0, stdGeom.gx0, stdGeom.nx));
        const int cx1 =
                qMin(stdGeom.nx - 1, cellIndex(box.right() + 1.0, stdGeom.gx0, stdGeom.nx));
        const int cy0 = qMax(0, cellIndex(box.top() - 1.0, stdGeom.gy0, stdGeom.ny));
        const int cy1 =
                qMin(stdGeom.ny - 1, cellIndex(box.bottom() + 1.0, stdGeom.gy0, stdGeom.ny));
        for (int cy = cy0; cy <= cy1; ++cy) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                const StdCurveGeom::Cell &cl = stdGeom.cells[cy * stdGeom.nx + cx];
                const double cellL = stdGeom.gx0 + cx * stdGeom.cell;
                const double cellT = stdGeom.gy0 + cy * stdGeom.cell;
                if (box.left() < cellL && cellL + stdGeom.cell < box.right() && box.top() < cellT
                    && cellT + stdGeom.cell < box.bottom()) {
                    cost += cl.fullSum;
                } else {
                    for (int e : cl.singles)
                        scoreElem(stdGeom.elems[e]);
                }
                for (int e : cl.multis) {
                    if (stdGeom.stamp[e] == stdGeom.stampId)
                        continue;
                    stdGeom.stamp[e] = stdGeom.stampId;
                    scoreElem(stdGeom.elems[e]);
                }
            }
        }
        return cost;
    };

    auto evaluateCollisionScore = [&](const QRectF &candRect) -> double {
        const QRectF box = candRect.adjusted(-4.0, -4.0, 4.0, 4.0);
        double cost = stdGeomCost(box);

        for (int li = 0; li < layersToScore.size(); ++li) {
            Graph *lg = layersToScore[li];
            Plot *lp = lg ? lg->plotWidget() : nullptr;
            if (!lp)
                continue;

            for (int i : stdGeom.otherCurves.value(li)) {
                if (i >= lg->n_curves || i >= lg->c_keys.size())
                    continue;
                if (!lg->isCurveVisible(i))
                    continue;
                QwtPlotItem *it = lg->plotItem(i);
                if (!it || !it->isVisible())
                    continue;

                // 1. Spectrogram hotspots
                if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                    Spectrogram *sp = static_cast<Spectrogram *>(it);
                    const QwtDoubleInterval zRange = sp->effectiveValueRange();
                    const double zSpan = zRange.width();
                    if (zSpan > 1e-30) {
                        constexpr int kGrid = 7;
                        for (int gy = 0; gy < kGrid; ++gy) {
                            const double py = candRect.top()
                                    + (gy + 0.5) / double(kGrid) * candRect.height();
                            const double vy = lp->invTransform(sp->yAxis(), qRound(py));
                            for (int gx = 0; gx < kGrid; ++gx) {
                                const double px = candRect.left()
                                        + (gx + 0.5) / double(kGrid) * candRect.width();
                                const double vx = lp->invTransform(sp->xAxis(), qRound(px));
                                const double zVal = sp->data().value(vx, vy);
                                if (std::isfinite(zVal)) {
                                    const double norm =
                                            qBound(0.0, (zVal - zRange.minValue()) / zSpan, 1.0);
                                    cost += norm * norm * 12.0;
                                }
                            }
                        }
                    }
                    continue;
                }

                const int ct = (i < lg->c_type.size()) ? lg->c_type[i] : Line;

                // 2. Error-bar segments
                if (ct == ErrorBars) {
                    QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(it);
                    if (!er || !er->masterCurve() || !er->masterCurve()->isVisible())
                        continue;
                    const QwtScaleMap xMap = lp->canvasMap(er->xAxis());
                    const QwtScaleMap yMap = lp->canvasMap(er->yAxis());
                    const int nPts = er->dataSize();
                    const int stride = (nPts > 4000) ? qMax(1, nPts / 2000) : 1;
                    for (int p = 0; p < nPts; p += stride) {
                        const double vx = er->x(p);
                        const double vy = er->y(p);
                        const double ev = er->errorValue(p);
                        if (!std::isfinite(vx) || !std::isfinite(vy) || !std::isfinite(ev))
                            continue;
                        bool hit = false;
                        double clipLen = 0.0;
                        if (er->direction() == QwtErrorPlotCurve::Vertical) {
                            const double yLo = er->minusSide() ? (vy - ev) : vy;
                            const double yHi = er->plusSide() ? (vy + ev) : vy;
                            const double px = xMap.xTransform(vx);
                            clipLen = liangBarskyClippedLength(px, yMap.xTransform(yLo), px,
                                                               yMap.xTransform(yHi), box, &hit);
                        } else {
                            const double xLo = er->minusSide() ? (vx - ev) : vx;
                            const double xHi = er->plusSide() ? (vx + ev) : vx;
                            const double py = yMap.xTransform(vy);
                            clipLen = liangBarskyClippedLength(xMap.xTransform(xLo), py,
                                                               xMap.xTransform(xHi), py, box, &hit);
                        }
                        if (hit)
                            cost += (4.0 + clipLen * 1.2) * stride;
                    }
                    continue;
                }

                QwtPlotCurve *c = dynamic_cast<QwtPlotCurve *>(it);
                if (!c)
                    continue;
                const int nPts = c->dataSize();
                if (nPts <= 0)
                    continue;

                const QwtScaleMap xMap = lp->canvasMap(c->xAxis());
                const QwtScaleMap yMap = lp->canvasMap(c->yAxis());
                const DataCurve *dc = dynamic_cast<const DataCurve *>(c);

                // 3. Bars / Histograms
                if (ct == VerticalBars || ct == HorizontalBars || ct == Histogram) {
                    const QwtBarCurve *bc = dynamic_cast<const QwtBarCurve *>(c);
                    double stepPx = 28.0;
                    if (nPts >= 2) {
                        const double p0 = (ct == HorizontalBars) ? yMap.xTransform(c->y(0))
                                                                 : xMap.xTransform(c->x(0));
                        const double p1 = (ct == HorizontalBars) ? yMap.xTransform(c->y(1))
                                                                 : xMap.xTransform(c->x(1));
                        stepPx = qMax(6.0, std::fabs(p1 - p0));
                    } else {
                        stepPx = qMax(12.0,
                                      0.2 * std::fabs((ct == HorizontalBars) ? yMap.pDist()
                                                                             : xMap.pDist()));
                    }
                    const double gapFrac = bc ? (1.0 - double(bc->gap()) * 0.01) : 0.70;
                    const double barW = qBound(3.0, stepPx * gapFrac, 120.0);
                    const double shiftLeft =
                            bc ? ((0.5 - double(bc->offset()) * 0.01) * barW) : (0.5 * barW);
                    const double basePx = (ct == HorizontalBars) ? xMap.xTransform(c->baseline())
                                                                 : yMap.xTransform(c->baseline());
                    Column *barXCol = nullptr;
                    Column *barYCol = nullptr;
                    const bool barHasMasks = dc && dc->hasAnyMaskedPoints();
                    if (barHasMasks)
                        dc->resolveColumns(barXCol, barYCol);
                    for (int p = 0; p < nPts; ++p) {
                        if (barHasMasks && dc->isPointMaskedFast(p, barXCol, barYCol))
                            continue;
                        const double vx = c->x(p);
                        const double vy = c->y(p);
                        if (!std::isfinite(vx) || !std::isfinite(vy))
                            continue;
                        QRectF barRect;
                        if (ct == HorizontalBars) {
                            const double py = yMap.xTransform(vy);
                            const double px = xMap.xTransform(vx);
                            barRect = QRectF(qMin(basePx, px), py - shiftLeft,
                                             std::fabs(px - basePx), barW);
                        } else {
                            const double px = xMap.xTransform(vx);
                            const double py = yMap.xTransform(vy);
                            barRect = QRectF(px - shiftLeft, qMin(basePx, py), barW,
                                             std::fabs(py - basePx));
                        }
                        const QRectF inter = box.intersected(barRect);
                        if (!inter.isEmpty())
                            cost += 160.0 + inter.width() * inter.height() * 1.1;
                    }
                    continue;
                }

                // 4. Box / Violin plots
                if (ct == Box) {
                    double minY = 1e300, maxY = -1e300;
                    for (int p = 0; p < nPts; ++p) {
                        const double vy = c->y(p);
                        if (std::isfinite(vy)) {
                            minY = qMin(minY, vy);
                            maxY = qMax(maxY, vy);
                        }
                    }
                    if (minY <= maxY) {
                        const double px = xMap.xTransform(c->x(0));
                        const double py0 = yMap.xTransform(minY);
                        const double py1 = yMap.xTransform(maxY);
                        const QRectF boxRect(px - 24.0, qMin(py0, py1), 48.0,
                                             std::fabs(py1 - py0));
                        const QRectF inter = box.intersected(boxRect);
                        if (!inter.isEmpty())
                            cost += 40.0 + inter.width() * inter.height() * 0.25;
                    }
                    continue;
                }

                // 4b. Vector curves (shaft + arrowhead tip)
                if (ct == VectXYXY || ct == VectXYAM) {
                    VectorCurve *vc = dynamic_cast<VectorCurve *>(const_cast<QwtPlotCurve *>(c));
                    Table *vt = vc ? vc->table() : nullptr;
                    auto resolveVecCol = [vt](const QString &spec) -> Column * {
                        if (!vt || spec.isEmpty())
                            return nullptr;
                        const QString pfx = QString(vt->name()) + QLatin1Char('_');
                        if (spec.startsWith(pfx)) {
                            const QString sh = spec.mid(pfx.size());
                            for (int ci = 0; ci < vt->numCols(); ++ci) {
                                if (Column *col = vt->column(ci)) {
                                    if (col->name() == sh)
                                        return col;
                                }
                            }
                        }
                        for (int ci = 0; ci < vt->numCols(); ++ci) {
                            if (Column *col = vt->column(ci)) {
                                if (col->name() == spec)
                                    return col;
                            }
                        }
                        const int fb = vt->colIndex(spec);
                        return (fb >= 0 && fb < vt->numCols()) ? vt->column(fb) : nullptr;
                    };
                    Column *ec1 = vc ? resolveVecCol(vc->vectorEndXAColName()) : nullptr;
                    Column *ec2 = vc ? resolveVecCol(vc->vectorEndYMColName()) : nullptr;
                    const int vPos = vc ? vc->position() : int(VectorCurve::Tail);
                    const double headPad =
                            qMax(6.0, vc ? double(qMax(6, vc->headLength())) : 8.0);
                    const QRectF shaftBox = box.adjusted(-3.0, -3.0, 3.0, 3.0);
                    for (int p = 0; p < nPts; ++p) {
                        const double x0 = c->x(p);
                        const double y0 = c->y(p);
                        if (!std::isfinite(x0) || !std::isfinite(y0))
                            continue;
                        double xs = xMap.xTransform(x0);
                        double ys = yMap.xTransform(y0);
                        double xe = xs;
                        double ye = ys;
                        const int r = vc ? vc->tableRow(p) : -1;
                        if (ec1 && ec2 && r >= 0 && r < ec1->rowCount() && r < ec2->rowCount()
                            && !ec1->isInvalid(r) && !ec2->isInvalid(r)) {
                            const double v1 = ec1->valueAt(r);
                            const double v2 = ec2->valueAt(r);
                            if (std::isfinite(v1) && std::isfinite(v2)) {
                                if (ct == VectXYAM) {
                                    const double dx = v2 * std::cos(v1);
                                    const double dy = v2 * std::sin(v1);
                                    if (vPos == int(VectorCurve::Middle)) {
                                        xs = xMap.xTransform(x0 - 0.5 * dx);
                                        ys = yMap.xTransform(y0 - 0.5 * dy);
                                        xe = xMap.xTransform(x0 + 0.5 * dx);
                                        ye = yMap.xTransform(y0 + 0.5 * dy);
                                    } else if (vPos == int(VectorCurve::Head)) {
                                        xs = xMap.xTransform(x0 - dx);
                                        ys = yMap.xTransform(y0 - dy);
                                        xe = xMap.xTransform(x0);
                                        ye = yMap.xTransform(y0);
                                    } else {
                                        xe = xMap.xTransform(x0 + dx);
                                        ye = yMap.xTransform(y0 + dy);
                                    }
                                } else {
                                    xe = xMap.xTransform(v1);
                                    ye = yMap.xTransform(v2);
                                }
                            }
                        }
                        bool hit = false;
                        const double clipLen =
                                liangBarskyClippedLength(xs, ys, xe, ye, shaftBox, &hit);
                        if (hit)
                            cost += 120.0 + clipLen * 3.5;
                        const QRectF tipBox(xe - headPad, ye - headPad, 2.0 * headPad,
                                            2.0 * headPad);
                        if (box.intersects(tipBox))
                            cost += 220.0;
                    }
                    continue;
                }

                // 5. Standard 2D curve symbols and line segments: scored through the
                // grid index above (stdGeomCost), added once per box at the top.
            }

            // 6. Other text markers, image markers, arrow markers, and reference lines
            for (int tid : lg->d_texts) {
                if (lg == this && tid == legendMarkerID)
                    continue;
                Legend *tm = dynamic_cast<Legend *>(lp->marker(tid));
                if (tm && tm->isVisible()) {
                    QRectF tmRect(tm->rect());
                    if (tmRect.width() <= 2.0 || tmRect.height() <= 2.0) {
                        const double tx = lp->canvasMap(tm->xAxis()).xTransform(tm->xValue());
                        const double ty = lp->canvasMap(tm->yAxis()).xTransform(tm->yValue());
                        QwtText qt(tm->text());
                        qt.setFont(tm->font());
                        const QSize tsz = qt.textSize();
                        if (std::isfinite(tx) && std::isfinite(ty))
                            tmRect = QRectF(tx, ty, qMax(8, tsz.width() + 8),
                                            qMax(8, tsz.height() + 6));
                    }
                    const QRectF inter = box.intersected(tmRect);
                    if (!inter.isEmpty())
                        cost += 380.0 + inter.width() * inter.height() * 1.2;
                }
            }
            for (int iid : lg->d_images) {
                ImageMarker *im = dynamic_cast<ImageMarker *>(lp->marker(iid));
                if (im && im->isVisible()) {
                    const QRectF inter = box.intersected(QRectF(im->rect()));
                    if (!inter.isEmpty())
                        cost += 200.0 + inter.width() * inter.height() * 0.5;
                }
            }
            for (int lid : lg->d_lines) {
                ArrowMarker *am = dynamic_cast<ArrowMarker *>(lp->marker(lid));
                if (am && am->isVisible()) {
                    const QwtDoublePoint sC = am->startPointCoord();
                    const QwtDoublePoint eC = am->endPointCoord();
                    double sx0 = lp->canvasMap(am->xAxis()).xTransform(sC.x());
                    double sy0 = lp->canvasMap(am->yAxis()).xTransform(sC.y());
                    double sx1 = lp->canvasMap(am->xAxis()).xTransform(eC.x());
                    double sy1 = lp->canvasMap(am->yAxis()).xTransform(eC.y());
                    if (!std::isfinite(sx0) || !std::isfinite(sy0) || !std::isfinite(sx1)
                        || !std::isfinite(sy1)) {
                        const QPoint sPt = am->startPoint();
                        const QPoint ePt = am->endPoint();
                        sx0 = sPt.x();
                        sy0 = sPt.y();
                        sx1 = ePt.x();
                        sy1 = ePt.y();
                    }
                    bool hit = false;
                    const double clipLen = liangBarskyClippedLength(sx0, sy0, sx1, sy1, box, &hit);
                    if (hit)
                        cost += 450.0 + clipLen * 6.0;
                }
            }
            for (const ReferenceLineSpec &rl : lg->d_ref_lines) {
                if (!rl.visible)
                    continue;
                bool hit = false;
                double clipLen = 0.0;
                if (rl.orientation == 0) {
                    const double py = lp->canvasMap(QwtPlot::yLeft).xTransform(rl.value);
                    clipLen = liangBarskyClippedLength(0.0, py, double(cw), py, box, &hit);
                } else {
                    const double px = lp->canvasMap(QwtPlot::xBottom).xTransform(rl.value);
                    clipLen = liangBarskyClippedLength(px, 0.0, px, double(ch), box, &hit);
                }
                if (hit)
                    cost += 35.0 + clipLen * 0.8;
            }

            if (lg->isTernaryPlot()) {
                const QwtScaleMap txMap = lp->canvasMap(QwtPlot::xBottom);
                const QwtScaleMap tyMap = lp->canvasMap(QwtPlot::yLeft);
                const double th = std::sqrt(3.0) * 0.5;
                const QPointF vA(txMap.xTransform(0.0), tyMap.xTransform(0.0));
                const QPointF vB(txMap.xTransform(1.0), tyMap.xTransform(0.0));
                const QPointF vC(txMap.xTransform(0.5), tyMap.xTransform(th));
                const QPointF edges[3][2] = { { vA, vB }, { vB, vC }, { vC, vA } };
                for (int ei = 0; ei < 3; ++ei) {
                    bool hit = false;
                    const double cl = liangBarskyClippedLength(edges[ei][0].x(), edges[ei][0].y(),
                                                               edges[ei][1].x(), edges[ei][1].y(),
                                                               box, &hit);
                    if (hit)
                        cost += 600.0 + cl * 5.0;
                }
                QPolygonF triPoly(3);
                triPoly[0] = vA;
                triPoly[1] = vB;
                triPoly[2] = vC;
                if (triPoly.containsPoint(box.center(), Qt::OddEvenFill))
                    cost += 900.0;
            } else if (lg->isPolarPlot()) {
                const QwtScaleMap pxMap = lp->canvasMap(QwtPlot::xBottom);
                const QwtScaleMap pyMap = lp->canvasMap(QwtPlot::yLeft);
                const double pcx = pxMap.xTransform(0.0);
                const double pcy = pyMap.xTransform(0.0);
                const double pr = 1.24
                        * std::fabs(pxMap.xTransform(lg->polarRadiusMax()) - pcx);
                const double nearestX = qBound(box.left(), pcx, box.right());
                const double nearestY = qBound(box.top(), pcy, box.bottom());
                const double dist = std::hypot(nearestX - pcx, nearestY - pcy);
                if (dist < pr)
                    cost += 800.0 + (pr - dist) * 6.0;
            } else if (lg->isPiePlot()) {
                double pcx = 0.36 * double(cw);
                double pcy = 0.52 * double(ch);
                double pr = 1.25 * 0.285 * double(qMin(cw, ch));
                if (effLegW > int(0.34 * double(cw))) {
                    const double legBottom =
                            qBound(0.08 * double(ch), double(4 + legH), 0.52 * double(ch));
                    const double pad = 0.028 * double(ch);
                    const double availH = qMax(0.20 * double(ch), double(ch) - legBottom - 2.0 * pad);
                    pcx = 0.50 * double(cw);
                    pcy = legBottom + pad + 0.5 * availH;
                    pr = 1.05 * qMin(0.31 * double(cw), 0.44 * availH);
                }
                const double nearestX = qBound(box.left(), pcx, box.right());
                const double nearestY = qBound(box.top(), pcy, box.bottom());
                const double dist = std::hypot(nearestX - pcx, nearestY - pcy);
                if (dist < pr)
                    cost += 800.0 + (pr - dist) * 6.0;
            }
        }

        // 7. Child inset frames, zoom ROI boxes (+ leader lines), and inside panel labels
        if (MultiLayer *ml = multiLayerParent()) {
            const QPoint myCanvasOrigin = pos() + d_plot->plotLayout()->canvasRect().topLeft();
            const QRectF localCanvasF(0.0, 0.0, double(cw), double(ch));
            const QwtScaleMap myXMap = d_plot->canvasMap(QwtPlot::xBottom);
            const QwtScaleMap myYMap = d_plot->canvasMap(QwtPlot::yLeft);
            QVector<QRectF> insetAndRoiBoxes;

            const QList<Graph *> allGraphs = ml->layersList();
            for (Graph *ins : allGraphs) {
                if (!ins || ins == this || ins->isHidden() || !ml->isInsetLayer(ins)
                    || ml->insetSourceLayer(ins) != this)
                    continue;
                Plot *inPlot = ins->plotWidget();
                const QSize insSz = inPlot ? inPlot->size() : ins->size();
                const QRectF insBox = QRectF(QPointF(ins->pos() - myCanvasOrigin), QSizeF(insSz))
                                              .adjusted(-4.0, -4.0, 4.0, 4.0);
                insetAndRoiBoxes.append(insBox);
                const QRectF insInter = box.intersected(insBox);
                if (!insInter.isEmpty())
                    cost += 2000.0 + insInter.width() * insInter.height() * 2.0;

                QRectF zoomData = ml->insetZoomDataRect(ins);
                if (inPlot) {
                    const QwtScaleDiv *ixSd = inPlot->axisScaleDiv(QwtPlot::xBottom);
                    const QwtScaleDiv *iySd = inPlot->axisScaleDiv(QwtPlot::yLeft);
                    if (ixSd && iySd) {
                        const double xLo = qMin(ixSd->lowerBound(), ixSd->upperBound());
                        const double xHi = qMax(ixSd->lowerBound(), ixSd->upperBound());
                        const double yLo = qMin(iySd->lowerBound(), iySd->upperBound());
                        const double yHi = qMax(iySd->lowerBound(), iySd->upperBound());
                        if (std::isfinite(xLo) && std::isfinite(xHi) && std::isfinite(yLo)
                            && std::isfinite(yHi) && (xHi > xLo + 1e-15 || yHi > yLo + 1e-15)) {
                            zoomData = QRectF(QPointF(xLo, yLo), QPointF(xHi, yHi));
                        }
                    }
                }
                if (!zoomData.isEmpty() || qAbs(zoomData.width()) > 1e-15
                    || qAbs(zoomData.height()) > 1e-15) {
                    const double rx0 = myXMap.xTransform(zoomData.left());
                    const double rx1 = myXMap.xTransform(zoomData.right());
                    const double ry0 = myYMap.xTransform(zoomData.top());
                    const double ry1 = myYMap.xTransform(zoomData.bottom());
                    if (std::isfinite(rx0) && std::isfinite(rx1) && std::isfinite(ry0)
                        && std::isfinite(ry1)) {
                        const QRectF rawRoi(QPointF(qMin(rx0, rx1), qMin(ry0, ry1)),
                                            QPointF(qMax(rx0, rx1), qMax(ry0, ry1)));
                        const QRectF roiBox = rawRoi.intersected(localCanvasF);
                        if (roiBox.width() >= 2.0 && roiBox.height() >= 2.0) {
                            const QRectF padRoi = roiBox.adjusted(-3.0, -3.0, 3.0, 3.0);
                            insetAndRoiBoxes.append(padRoi);
                            const QRectF roiInter = box.intersected(padRoi);
                            if (!roiInter.isEmpty())
                                cost += 800.0 + roiInter.width() * roiInter.height() * 1.5;

                            if (ml->insetShowConnectors(ins) && inPlot) {
                                const QRect inCanvasLocal = inPlot->plotLayout()->canvasRect();
                                if (inCanvasLocal.width() >= 12 && inCanvasLocal.height() >= 12) {
                                    const QRectF insetFrame(
                                            inCanvasLocal.translated(ins->pos() - myCanvasOrigin));
                                    const QPointF rC = roiBox.center();
                                    const QPointF iC = insetFrame.center();
                                    bool lHit = false;
                                    const double clipLen = liangBarskyClippedLength(
                                            rC.x(), rC.y(), iC.x(), iC.y(), box, &lHit);
                                    if (lHit)
                                        cost += 180.0 + clipLen * 2.5;
                                }
                            }
                        }
                    }
                }
            }

            if (ml->panelLabelsEnabled()
                && ml->panelLabelPlacement() == MultiLayer::PanelInsideTopLeft
                && !ml->isInsetLayer(this) && !linkedParentLayer()) {
                double pt = double(ml->panelLabelFontPt());
                if (pt <= 0.0) {
                    const QFont tf = axisTitleFont(QwtPlot::yLeft);
                    const double titlePt = tf.pointSizeF() > 0.0
                            ? tf.pointSizeF()
                            : (tf.pointSize() > 0 ? double(tf.pointSize()) : 0.0);
                    pt = (titlePt > 0.0)
                            ? qBound(9.0, titlePt >= 24.0 ? titlePt : titlePt + 1.0, 40.0)
                            : 12.0;
                }
                QFont pf(QStringLiteral("Arial"));
                pf.setPointSizeF(pt);
                pf.setWeight(QFont::Normal);
                pf.setBold(false);
                pf.setHintingPreference(QFont::PreferNoHinting);
                const QFontMetrics pfm(pf);
                const int lblW = qMax(18, pfm.horizontalAdvance(QStringLiteral("(a)")));
                const int lblH = pfm.height();
                const int minX = 8;
                const int maxX = qMax(minX, cw - lblW - 8);
                const int minY = 6;
                const int maxY = qMax(minY, ch - lblH - 8);

                auto hitsInsetOrRoi = [&](int lx, int ly) -> bool {
                    const QRectF lb(lx - 4, ly - 4, lblW + 8, lblH + 8);
                    for (const QRectF &ob : insetAndRoiBoxes) {
                        if (ob.intersects(lb))
                            return true;
                    }
                    return false;
                };

                int px = minX;
                int py = minY;
                if (hitsInsetOrRoi(px, py)) {
                    bool placed = false;
                    for (int tx = minX; tx <= maxX && !placed; tx += 12) {
                        if (!hitsInsetOrRoi(tx, py)) {
                            px = tx;
                            placed = true;
                        }
                    }
                    for (int ty = minY + 12; ty <= maxY && !placed; ty += 12) {
                        if (!hitsInsetOrRoi(minX, ty)) {
                            px = minX;
                            py = ty;
                            placed = true;
                        }
                    }
                }
                const QRectF panelLblBox(px - 4.0, py - 4.0, lblW + 8.0, lblH + 8.0);
                const QRectF plInter = box.intersected(panelLblBox);
                if (!plInter.isEmpty())
                    cost += 900.0 + plInter.width() * plInter.height() * 1.5;
            }
        }

        return cost;
    };

    // bound: a caller that only keeps a score below it (the grid search) lets the per-row sum
    // stop once it reaches the bound; every cost is >= 0, so such a candidate could not have
    // been chosen, and a score below the bound is computed in full as before.
    auto evaluateOriginScore = [&](int ox, int oy,
                                   double bound =
                                           std::numeric_limits<double>::infinity()) -> double {
        const double fullBoxScore = evaluateCollisionScore(QRectF(ox, oy, legW, legH));
        if (framelessSingleCol && !rowVisualRects.isEmpty()) {
            double sum = 0.0;
            for (const QRectF &rr : rowVisualRects) {
                sum += evaluateCollisionScore(rr.translated(double(ox), double(oy)));
                if (sum + 0.35 * fullBoxScore >= bound)
                    break;
            }
            return sum + 0.35 * fullBoxScore;
        }
        return fullBoxScore;
    };

    auto ensureHorizBarCategoryHeadroom = [&]() -> bool {
        if (d_is_ternary || d_is_polar || isPoleFigurePlot() || isPiePlot()
            || !d_plot->axisEnabled(QwtPlot::yLeft)
            || d_axis_breaks[QwtPlot::yLeft].enabled) {
            return false;
        }
        bool hasHorizBars = false;
        for (int i = 0; i < n_curves && i < c_type.size(); ++i) {
            if (c_type[i] == HorizontalBars) {
                hasHorizBars = true;
                break;
            }
        }
        const QwtTextScaleDraw *yCatSd =
                dynamic_cast<const QwtTextScaleDraw *>(d_plot->axisScaleDraw(QwtPlot::yLeft));
        const QwtScaleDiv *yCatDiv = d_plot->axisScaleDiv(QwtPlot::yLeft);
        const QwtScaleEngine *yCatEng = d_plot->axisScaleEngine(QwtPlot::yLeft);
        if (!hasHorizBars || !yCatSd || !yCatDiv || !yCatEng
            || yCatEng->testAttribute(QwtScaleEngine::Inverted)) {
            return false;
        }
        const QMap<int, QString> catMap = yCatSd->labelsMap();
        if (catMap.isEmpty())
            return false;
        const double minKey = double(catMap.firstKey());
        const double maxKey = double(catMap.lastKey());
        const double curLo =
                qMin(minKey - 0.5, qMin(yCatDiv->lowerBound(), yCatDiv->upperBound()));
        const double curHi =
                qMax(maxKey + 0.5, qMax(yCatDiv->lowerBound(), yCatDiv->upperBound()));
        const double topBarY = maxKey + 0.46;
        const double frac =
                qBound(0.14, double(legH + mT + 8) / double(qMax(60, ch)), 0.52);
        const double neededCatHi =
                qMin(maxKey + 4.2, (topBarY - frac * curLo) / (1.0 - frac));
        if (neededCatHi <= curHi + 0.05)
            return false;
        QwtValueList catMaj;
        for (auto it = catMap.constBegin(); it != catMap.constEnd(); ++it)
            catMaj += double(it.key());
        QwtValueList emptyTicks;
        QwtValueList catTicks[QwtScaleDiv::NTickTypes];
        catTicks[QwtScaleDiv::MinorTick] = emptyTicks;
        catTicks[QwtScaleDiv::MediumTick] = emptyTicks;
        catTicks[QwtScaleDiv::MajorTick] = catMaj;
        QwtScaleEngine *newCatEng = new QwtLinearScaleEngine();
        QwtScaleDiv newCatDiv(curLo, neededCatHi, catTicks);
        setToriAxisUserLocked(d_plot, QwtPlot::yLeft, true);
        d_plot->setAxisMaxMinor(QwtPlot::yLeft, 0);
        d_plot->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::None);
        d_plot->setAxisMaxMajor(QwtPlot::yLeft, qMax(8, catMap.size() + 2));
        d_plot->setAxisScale(QwtPlot::yLeft, curLo, neededCatHi, 1.0);
        d_plot->setAxisScaleEngine(QwtPlot::yLeft, newCatEng);
        d_plot->setAxisScaleDiv(QwtPlot::yLeft, newCatDiv);
        updateSecondaryAxis(QwtPlot::yRight);
        d_zoomer[0]->setZoomBase();
        d_plot->replot();
        d_plot->updateLayout();
        cw = d_plot->canvas()->width();
        ch = d_plot->canvas()->height();
        syncChildLinkedLayers();
        return true;
    };

    LegendAnchor chosenAnchor = anchor;
    double chosenScore = 0.0;
    bool useGridOrigin = false;
    QPoint bestGridOrigin;

    if (anchor == LegendAutoAvoidData) {
        // Tie-break order required by spec: TR -> TL -> BR -> BL -> TC -> BC
        static const LegendAnchor kCandidates[] = {
            LegendTopRight,   LegendTopLeft,   LegendBottomRight,
            LegendBottomLeft, LegendTopCenter, LegendBottomCenter
        };
        const QRectF canvasBoundsF(0.0, 0.0, static_cast<double>(cw), static_cast<double>(ch));
        auto evaluateProximityScore = [&](int ox, int oy) -> double {
            const QRectF baseBox(static_cast<double>(ox), static_cast<double>(oy),
                                 static_cast<double>(effLegW), static_cast<double>(legH));
            const double nearPadX = qMax(14.0, 0.07 * static_cast<double>(cw));
            const double nearPadY = qMax(14.0, 0.08 * static_cast<double>(ch));
            const double farPadX = qMax(28.0, 0.14 * static_cast<double>(cw));
            const double farPadY = qMax(28.0, 0.16 * static_cast<double>(ch));
            const QRectF nearBox =
                    baseBox.adjusted(-nearPadX, -nearPadY, nearPadX, nearPadY).intersected(canvasBoundsF);
            const QRectF farBox =
                    baseBox.adjusted(-farPadX, -farPadY, farPadX, farPadY).intersected(canvasBoundsF);
            return evaluateCollisionScore(nearBox) + 0.15 * evaluateCollisionScore(farBox);
        };
        auto scoreAllCandidates = [&]() {
            chosenAnchor = kCandidates[0];
            chosenScore = 1e300;
            double chosenProx = 1e300;
            useGridOrigin = false;
            for (LegendAnchor cand : kCandidates) {
                const QPoint pt = originForInsideAnchor(cand);
                const double score = evaluateOriginScore(pt.x(), pt.y());
                const double prox = evaluateProximityScore(pt.x(), pt.y());
                const bool strictlyLowerHard = (score < chosenScore - 1e-6);
                const bool tiedHardBetterClearance =
                        (qAbs(score - chosenScore) <= 1e-6 && prox < chosenProx - 1e-6);
                if (strictlyLowerHard || tiedHardBetterClearance) {
                    chosenScore = score;
                    chosenProx = prox;
                    chosenAnchor = cand;
                }
            }

            // 25x25 continuous 2D grid search (including middle-left/right/top/bottom margins)
            // when all 6 standard anchors collide (> 1.0)
            const int xMinGrid = qMin(mL, qMax(2, cw - effLegW - 2));
            const int xMaxGrid = qMax(xMinGrid, cw - effLegW - qMin(mR, 4));
            const int yMinGrid = qMin(mT, qMax(2, ch - legH - 2));
            const int yMaxGrid = qMax(yMinGrid, ch - legH - qMin(mB, 4));
            if (chosenScore > 1.0 && (xMaxGrid > xMinGrid || yMaxGrid > yMinGrid)) {
                double bestGridScore = 1e300;
                QPoint bestPt;
                constexpr int kGridN = 25;
                for (int gy = 0; gy < kGridN; ++gy) {
                    const int py = (yMaxGrid > yMinGrid)
                            ? (yMinGrid
                               + qRound((double(gy) / double(kGridN - 1)) * (yMaxGrid - yMinGrid)))
                            : yMinGrid;
                    for (int gx = 0; gx < kGridN; ++gx) {
                        // Prefer right before left on ties by scanning gx from right to left
                        const int rGx = (kGridN - 1) - gx;
                        const int px = (xMaxGrid > xMinGrid)
                                ? (xMinGrid
                                   + qRound((double(rGx) / double(kGridN - 1))
                                            * (xMaxGrid - xMinGrid)))
                                : xMinGrid;
                        const bool isCorner =
                                (rGx == 0 || rGx == kGridN - 1) && (gy == 0 || gy == kGridN - 1);
                        const double penalty = isCorner ? 0.0 : 0.05;
                        const double sc = evaluateOriginScore(px, py, bestGridScore) + penalty;
                        if (sc < bestGridScore - 1e-9) {
                            bestGridScore = sc;
                            bestPt = QPoint(px, py);
                        }
                    }
                }
                if (bestGridScore + 0.5 < chosenScore) {
                    chosenScore = bestGridScore;
                    bestGridOrigin = bestPt;
                    useGridOrigin = true;
                    // Map nearest standard anchor name for RPC return value
                    double nearestDist2 = 1e300;
                    for (LegendAnchor cand : kCandidates) {
                        const QPoint cp = originForInsideAnchor(cand);
                        const double d2 = double(cp.x() - bestPt.x()) * double(cp.x() - bestPt.x())
                                + double(cp.y() - bestPt.y()) * double(cp.y() - bestPt.y());
                        if (d2 < nearestDist2) {
                            nearestDist2 = d2;
                            chosenAnchor = cand;
                        }
                    }
                }
            }
        };

        scoreAllCandidates();

        // Compact 2-column attempt when >= 4 visible series make a tall single-column legend
        // collide on small canvases (e.g. 6-row spin-polarized DOS on 85 mm ISIJ figures).
        if (chosenScore > 12.0 && leg->numColumns() == 1 && !isPiePlot() && !d_is_ternary
            && !d_is_polar && !isPoleFigurePlot() && legH > int(0.32 * double(qMax(60, ch)))) {
            int visCurves = 0;
            for (int ci = 0; ci < n_curves; ++ci) {
                if (isCurveVisible(ci) && (ci >= c_type.size() || c_type[ci] != ErrorBars))
                    ++visCurves;
            }
            if (visCurves >= 4) {
                const double score1Col = chosenScore;
                const LegendAnchor anchor1Col = chosenAnchor;
                const bool grid1Col = useGridOrigin;
                const QPoint pt1Col = bestGridOrigin;
                leg->setNumColumns(2);
                framelessSingleCol = false;
                d_plot->updateLayout();
                recomputeLegendMetrics();
                if (legW <= cw - mL - mR && legW <= cw - 8) {
                    scoreAllCandidates();
                    if (chosenScore > score1Col - 2.0 && legH * 13 >= ch * 5) {
                        leg->setNumColumns(1);
                        framelessSingleCol = (leg->frameStyle() == Legend::None);
                        d_plot->updateLayout();
                        recomputeLegendMetrics();
                        chosenScore = score1Col;
                        chosenAnchor = anchor1Col;
                        useGridOrigin = grid1Col;
                        bestGridOrigin = pt1Col;
                    }
                } else {
                    leg->setNumColumns(1);
                    framelessSingleCol = (leg->frameStyle() == Legend::None);
                    d_plot->updateLayout();
                    recomputeLegendMetrics();
                    chosenScore = score1Col;
                    chosenAnchor = anchor1Col;
                    useGridOrigin = grid1Col;
                    bestGridOrigin = pt1Col;
                }
            }
        }

        if (chosenScore > 35.0 && ensureHorizBarCategoryHeadroom())
            scoreAllCandidates();

        // Smart Y-Headroom: when every inside candidate AND 25x25 grid pocket collides (> 12.0),
        // expand a linear non-inverted non-broken yLeft upper bound enough to clear the legend
        // height (at least +22%, snapped up to the current major step) so TopRight / TopLeft /
        // TopCenter gets completely curve-free headroom above the data.
        if (chosenScore > 12.0 && !d_is_ternary && !d_is_polar && !isPoleFigurePlot()
            && !isPiePlot() && d_plot->axisEnabled(QwtPlot::yLeft)
            && !d_axis_breaks[QwtPlot::yLeft].enabled
            && !isToriAxisUserLocked(d_plot, QwtPlot::yLeft)
            && d_user_step[QwtPlot::yLeft] <= 0.0) {
            bool hasSpectrogram = false;
            bool hasOtherYCurves = false;
            double maxCurveY = -std::numeric_limits<double>::infinity();
            for (int i = 0; i < n_curves && i < c_keys.size(); ++i) {
                QwtPlotItem *it = plotItem(i);
                if (!it || !it->isVisible())
                    continue;
                if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                    hasSpectrogram = true;
                    break;
                }
                if (it->yAxis() != QwtPlot::yLeft)
                    hasOtherYCurves = true;
                if (const QwtPlotCurve *cv = curve(i)) {
                    if (cv->yAxis() == QwtPlot::yLeft) {
                        const int np = cv->dataSize();
                        for (int k = 0; k < np; ++k) {
                            const double yv = cv->y(k);
                            if (std::isfinite(yv))
                                maxCurveY = std::max(maxCurveY, yv);
                        }
                    }
                }
            }
            // Dual Y: widening the left axis only moves this layer's yLeft curves down. When the
            // curves on the other Y axis still meet the legend at every top position, it cannot
            // clear the legend; it only squeezed the left data into the bottom of the frame, and
            // each later placement (a preset applied again) widened it once more (left
            // 250..650 -> 200..1600 with lab_slide_83mm). The legend then keeps its best spot.
            bool headroomCanClear = true;
            if (hasOtherYCurves && !hasSpectrogram) {
                scoreSkipsYLeftCurves = true;
                double topFloor = std::numeric_limits<double>::infinity();
                for (LegendAnchor cand : { LegendTopRight, LegendTopLeft, LegendTopCenter }) {
                    const QPoint pt = originForInsideAnchor(cand);
                    topFloor = std::min(topFloor, evaluateOriginScore(pt.x(), pt.y()));
                }
                scoreSkipsYLeftCurves = false;
                headroomCanClear = (topFloor <= 12.0);
            }
            const QwtScaleDiv *yDiv = d_plot->axisScaleDiv(QwtPlot::yLeft);
            const QwtScaleEngine *yEng = d_plot->axisScaleEngine(QwtPlot::yLeft);
            if (headroomCanClear && !hasSpectrogram && yDiv && yEng
                && !Plot::isLog10ScaleEngine(yEng)
                && !yEng->testAttribute(QwtScaleEngine::Inverted)) {
                const double yLo = yDiv->lowerBound();
                const double yHi = yDiv->upperBound();
                const double span = yHi - yLo;
                if (span > 1e-30 && std::isfinite(span)) {
                    const QwtValueList majTicks = yDiv->ticks(QwtScaleDiv::MajorTick);
                    const double yStep = (majTicks.size() >= 2)
                            ? std::fabs(majTicks[1] - majTicks[0])
                            : d_user_step[QwtPlot::yLeft];
                    const double dataTop = (std::isfinite(maxCurveY) && maxCurveY > yLo)
                            ? std::min(yHi, maxCurveY)
                            : yHi;
                    const double reqFrac =
                            qBound(0.18, double(legH + mT + 10) / double(qMax(60, ch)), 0.56);
                    const double neededYHi = (dataTop - reqFrac * yLo) / (1.0 - reqFrac);
                    double newYHi = std::max(yHi + 0.22 * span, neededYHi);
                    if (yStep > 1e-30 && std::isfinite(yStep))
                        newYHi = std::ceil((newYHi - 1e-9 * yStep) / yStep) * yStep;

                    const int maj = d_plot->axisMaxMajor(QwtPlot::yLeft);
                    const int minT = d_plot->axisMaxMinor(QwtPlot::yLeft);
                    const int maxMinInt = (minT >= 1) ? (minT + 1) : 0;
                    double useStep = d_user_step[QwtPlot::yLeft];
                    if (useStep <= 0.0 && yStep > 1e-30) {
                        const double nSteps = (newYHi - yLo) / yStep;
                        if (nSteps <= double(qMax(maj + 2, 7)) + 1e-6)
                            useStep = yStep;
                    }
                    QwtLinearScaleEngine *newEng = new QwtLinearScaleEngine();
                    QwtScaleDiv newDiv =
                            newEng->divideScale(yLo, newYHi, qMax(maj, 5), maxMinInt, useStep);
                    if (minT == 1)
                        ensureLinearHalfStepMinorTicks(newDiv);
                    d_plot->setAxisScale(QwtPlot::yLeft, yLo, newYHi, useStep);
                    d_plot->setAxisScaleEngine(QwtPlot::yLeft, newEng);
                    d_plot->setAxisScaleDiv(QwtPlot::yLeft, newDiv);
                    updateSecondaryAxis(QwtPlot::yRight);
                    d_zoomer[0]->setZoomBase();
                    d_plot->replot();
                    d_plot->updateLayout();
                    cw = d_plot->canvas()->width();
                    ch = d_plot->canvas()->height();
                    syncChildLinkedLayers();

                    scoreAllCandidates();
                }
            }
        }

        // Gentle Compact Font Step-Down: when all pockets still collide (> 8.0) and axes are
        // locked (e.g. cramped Dual-Y or long labels), step legend font down by up to 1.5 pt
        // (floored at 7.5 pt for journal / 18 pt for slides) only if it materially cuts overlap.
        if (chosenScore > 8.0) {
            const QFont origLegFnt = leg->font();
            double curPt = origLegFnt.pointSizeF();
            if (curPt <= 0.0 && origLegFnt.pointSize() > 0)
                curPt = double(origLegFnt.pointSize());
            const double minAllowedPt = (curPt > 14.0) ? qMax(18.0, curPt - 3.0) : 7.5;
            if (curPt > minAllowedPt + 0.2) {
                const double prevScore = chosenScore;
                const LegendAnchor prevAnchor = chosenAnchor;
                const bool prevUseGrid = useGridOrigin;
                const QPoint prevGridPt = bestGridOrigin;
                QFont bestFnt = origLegFnt;

                for (int step = 1; step <= 2; ++step) {
                    const double tryPt = qMax(minAllowedPt, curPt - 0.75 * double(step));
                    QFont tryFnt = origLegFnt;
                    tryFnt.setPointSizeF(tryPt);
                    tryFnt.setHintingPreference(QFont::PreferNoHinting);
                    leg->setFont(tryFnt);
                    recomputeLegendMetrics();
                    scoreAllCandidates();
                    bestFnt = tryFnt;
                    if (chosenScore <= 1.0 || tryPt <= minAllowedPt + 1e-6)
                        break;
                }

                if (chosenScore >= prevScore - 4.0) {
                    leg->setFont(origLegFnt);
                    recomputeLegendMetrics();
                    chosenScore = prevScore;
                    chosenAnchor = prevAnchor;
                    useGridOrigin = prevUseGrid;
                    bestGridOrigin = prevGridPt;
                } else {
                    leg->setFont(bestFnt);
                }
            }
        }
    } else {
        QPoint pt = originForInsideAnchor(chosenAnchor);
        chosenScore = evaluateOriginScore(pt.x(), pt.y());
        if (chosenScore > 35.0
            && (chosenAnchor == LegendTopRight || chosenAnchor == LegendTopLeft
                || chosenAnchor == LegendTopCenter)
            && ensureHorizBarCategoryHeadroom()) {
            pt = originForInsideAnchor(chosenAnchor);
            chosenScore = evaluateOriginScore(pt.x(), pt.y());
        }
    }

    if (outOverlapScore)
        *outOverlapScore = chosenScore;

    leg->setOrigin(useGridOrigin ? bestGridOrigin : originForInsideAnchor(chosenAnchor));
    d_plot->replot();
    unzoomGuard.restoreNow();
    updateSelectionOverlayGeometry();
    emit modifiedGraph();
    return anchorName(chosenAnchor);
}
