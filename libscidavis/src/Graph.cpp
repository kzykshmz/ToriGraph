#include "OleServer.h"
#include "MultiLayer.h"
/***************************************************************************
    File                 : Graph.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Graph widget (core lifecycle, events, selection, zoom, frame, menus, export, reference lines, mask undo)

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
#include "RecalcEngine.h"
#include "EmfPaintDevice.h"
#include "MyParser.h"
#include <algorithm>
#include <limits>
#include <memory>
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

namespace {

struct ScopedExportFlag
{
    bool &target;
    bool prev;
    explicit ScopedExportFlag(bool &flag) : target(flag), prev(flag) { target = true; }
    ~ScopedExportFlag() { target = prev; }
};

class GraphSelectionOverlayWidget : public QWidget
{
public:
    explicit GraphSelectionOverlayWidget(Graph *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setFocusPolicy(Qt::NoFocus);
        hide();
    }

    void setDashed(bool dashed)
    {
        if (d_dashed != dashed) {
            d_dashed = dashed;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Graph *g = qobject_cast<const Graph *>(parentWidget());
        if (!g || g->isExporting() || g->property("isExporting").toBool())
            return;
        if (width() < 3 || height() < 3)
            return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 195), 3.0, Qt::SolidLine));
        p.drawRoundedRect(r, 2.0, 2.0);
        p.setPen(QPen(QColor(QStringLiteral("#2563EB")), 1.5,
                      d_dashed ? Qt::DashLine : Qt::SolidLine));
        p.drawRoundedRect(r, 2.0, 2.0);
    }

private:
    bool d_dashed = false;
};

} // namespace

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
    defaultMarkerFont.setHintingPreference(QFont::PreferNoHinting);
    auxMrkFont = QFont("Arial", 12, QFont::Normal, false);
    auxMrkFont.setHintingPreference(QFont::PreferNoHinting);
    defaultMarkerFrame = 1;
    defaultTextMarkerColor = QColor(Qt::black);
    defaultTextMarkerBackground = QColor(255, 255, 255, 255);
    for (int i = 0; i < 4; ++i)
        d_axis_backbone[i] = true;
    d_axis_unit_format = Column::UnitFormat::ISIJSlash;

    d_user_step = QVector<double>(QwtPlot::axisCnt);
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        axisType << AxisType::Numeric;
        axesFormatInfo << QString();
        axesFormulas << QString();
        d_user_step[i] = 0.0;
    }

    d_plot = new Plot(this);
    {
        QFont initTitleFnt("Arial", 18, QFont::Normal, false);
        initTitleFnt.setHintingPreference(QFont::PreferNoHinting);
        QFont initNumFnt("Arial", 16, QFont::Normal, false);
        initNumFnt.setHintingPreference(QFont::PreferNoHinting);
        initFonts(initTitleFnt, initNumFnt);

        QwtText t = d_plot->title();
        QFont plotTitleFnt("Arial", 18, QFont::Normal, false);
        plotTitleFnt.setHintingPreference(QFont::PreferNoHinting);
        t.setFont(plotTitleFnt);
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

    d_selection_overlay = new GraphSelectionOverlayWidget(this);

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

void Graph::copyImage()
{
    ScopedUnzoomForExport unzoomGuard(multiLayerParent()); // copy as at 100 %
    MultiLayer *ml = multiLayerParent();
    const int pageDpi = (ml && ml->pageDpi() > 0) ? ml->pageDpi() : 96;
    const double scaleFactor = double(pageDpi) / 96.0;
    const QSize baseSize = (d_plot && !d_plot->size().isEmpty()) ? d_plot->size() : size();
    const QSize targetSize(qMax(1, qRound(baseSize.width() * scaleFactor)),
                           qMax(1, qRound(baseSize.height() * scaleFactor)));
    QImage image(targetSize, QImage::Format_ARGB32);
    const QColor bgCol = d_plot ? d_plot->paletteBackgroundColor() : QColor(Qt::white);
    const bool transparent = bgCol.isValid() && bgCol.alpha() == 0;
    image.fill(transparent ? Qt::transparent : Qt::white);
    {
        QPainter p(&image);
        exportPainter(p, false, QRect(QPoint(0, 0), baseSize), targetSize);
        p.end();
    }
    const int dpm = qRound(double(pageDpi) / 0.0254);
    image.setDotsPerMeterX(dpm);
    image.setDotsPerMeterY(dpm);
    QApplication::clipboard()->setImage(image);
}

bool Graph::exportToFileChecked(const QString &fileName, QString *errOut)
{
    if (fileName.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }

    const QString lowerName = fileName.toLower();
    if (lowerName.endsWith(".eps") || lowerName.endsWith(".pdf") || lowerName.endsWith(".ps")) {
        return exportVectorChecked(fileName, 0, true, true, QPageSize(QPageSize::Custom),
                                   QPageLayout::Portrait, errOut);
    } else if (lowerName.endsWith(".svg")) {
        return exportSVGChecked(fileName, errOut);
    } else if (lowerName.endsWith(".emf")) {
        return exportImageChecked(fileName, -1, false, 0, errOut);
    } else {
        QList<QByteArray> list = QImageWriter::supportedImageFormats();
        for (int i = 0; i < list.count(); i++) {
            if (lowerName.endsWith("." + QString::fromLatin1(list[i]).toLower())) {
                return exportImageChecked(fileName, -1, false, 0, errOut);
            }
        }
        if (errOut)
            *errOut = tr("File format not handled, operation aborted!");
        return false;
    }
}

void Graph::exportToFile(const QString &fileName)
{
    QString err;
    if (!exportToFileChecked(fileName, &err)) {
        if (!ApplicationWindow::isNonInteractiveContext(this))
            QMessageBox::critical(this, tr("Error"),
                                  err.isEmpty() ? tr("Export failed.") : err);
    }
}

bool Graph::exportImageChecked(const QString &fileName, int quality, bool transparent, int dpi,
                               QString *errOut)
{
    if (fileName.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }

    if (fileName.endsWith(".emf", Qt::CaseInsensitive)) {
#ifdef Q_OS_WIN
        OleRenderedGraph emf = SciDAVisEmf::renderMultiLayer(multiLayerParent(), this, false);
        if (emf.emfBytes.isEmpty()) {
            if (errOut)
                *errOut = tr("Failed to render EMF graphics.");
            return false;
        }
        QFile f(fileName);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (errOut)
                *errOut = f.errorString().isEmpty()
                        ? tr("Could not open file for writing: %1").arg(fileName)
                        : f.errorString();
            return false;
        }
        const qint64 written = f.write(emf.emfBytes);
        const bool flushOk = f.flush();
        const QFile::FileError ferr = f.error();
        const QString ferrStr = f.errorString();
        f.close();
        if (written != emf.emfBytes.size() || !flushOk || ferr != QFile::NoError) {
            if (errOut)
                *errOut = ferrStr.isEmpty() ? tr("Failed to write complete EMF file.") : ferrStr;
            return false;
        }
        return true;
#else
        if (errOut)
            *errOut = tr("EMF export is only supported on Windows.");
        return false;
#endif
    }

    // The layer is exported as at 100 % whatever the Ctrl+wheel view zoom is.
    ScopedUnzoomForExport unzoomGuard(multiLayerParent());
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
    const bool isPng = fileName.endsWith(".png", Qt::CaseInsensitive);
    if (quality >= 0 && !(isPng && quality >= 95))
        writer.setQuality(quality);
    if (!writer.write(image)) {
        if (errOut) {
            *errOut = writer.errorString().isEmpty()
                    ? tr("Failed to write image file: %1").arg(fileName)
                    : writer.errorString();
        }
        return false;
    }
    return true;
}

void Graph::exportImage(const QString &fileName, int quality, bool transparent, int dpi)
{
    (void)exportImageChecked(fileName, quality, transparent, dpi, nullptr);
}

bool Graph::exportVectorChecked(const QString &fileName, int res, bool color, bool keepAspect,
                                QPageSize pageSize, QPageLayout::Orientation orientation,
                                QString *errOut)
{
    if (fileName.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }

    if (fileName.endsWith(".emf", Qt::CaseInsensitive)) {
        return exportImageChecked(fileName, -1, false, 0, errOut);
    }

    ScopedUnzoomForExport unzoomGuard(multiLayerParent()); // export as at 100 %
    QPrinter printer;
    printer.setCreator("ToriGraph");
    printer.setFullPage(true);

    QString outPath = fileName;
    printer.setOutputFileName(outPath);
    if (fileName.endsWith(".eps", Qt::CaseInsensitive)
        || fileName.endsWith(".ps", Qt::CaseInsensitive)) {
#if QT_VERSION >= 0x050000
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("Output in postscript format is not available for Qt5, using PDF"));
        }
        printer.setOutputFormat(QPrinter::PdfFormat);
        outPath = fileName + ".pdf";
        printer.setOutputFileName(outPath);
#else
        printer.setOutputFormat(QPrinter::PostScriptFormat);
#endif
    }

    if (printer.outputFormat() == QPrinter::PdfFormat)
        printer.setResolution(96);
    else if (res > 0)
        printer.setResolution(res);

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
    QPainter p;
    if (!p.begin(&printer)) {
        if (errOut)
            *errOut = tr("Could not open vector output file for writing: %1").arg(outPath);
        return false;
    }
    exportPainter(p, keepAspect, QRect(QPoint(0, 0), basePx),
                  QSize(printer.width(), printer.height()));
    const bool endOk = p.end();
    QFileInfo fi(outPath);
    if (!endOk || !fi.exists() || fi.size() <= 0) {
        if (errOut)
            *errOut = tr("Failed to write vector output file: %1").arg(outPath);
        return false;
    }
    return true;
}

void Graph::exportVector(const QString &fileName, int res, bool color, bool keepAspect,
                         QPageSize pageSize, QPageLayout::Orientation orientation)
{
    QString err;
    if (!exportVectorChecked(fileName, res, color, keepAspect, pageSize, orientation, &err)) {
        if (!ApplicationWindow::isNonInteractiveContext(this) && fileName.isEmpty())
            QMessageBox::critical(this, tr("Error"), err);
    }
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
        // Print as at 100 % (only once the dialog is accepted, so the view does not shrink
        // while the dialog is open).
        ScopedUnzoomForExport unzoomGuard(multiLayerParent());
        // The aspect of the layer as at 100 % (the zoomed rectangle is rounded differently).
        aspect = double(d_plot->width()) / double(qMax(1, d_plot->height()));
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

bool Graph::exportSVGChecked(const QString &fname, QString *errOut)
{
    if (fname.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }
#if QT_VERSION >= 0x040300
    ScopedUnzoomForExport unzoomGuard(multiLayerParent()); // export as at 100 %
    QFile outFile(fname);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errOut) {
            *errOut = outFile.errorString().isEmpty()
                    ? tr("Could not open SVG file for writing: %1").arg(fname)
                    : outFile.errorString();
        }
        return false;
    }
    QSvgGenerator svg;
    svg.setOutputDevice(&outFile);
#if QT_VERSION >= 0x040500
    svg.setSize(d_plot->size());
    svg.setViewBox(d_plot->rect());
    svg.setResolution(96); // FIXME hardcored
    svg.setTitle(this->objectName());
#endif
    QPainter p;
    if (!p.begin(&svg)) {
        outFile.close();
        if (errOut)
            *errOut = tr("Failed to initialize SVG painter.");
        return false;
    }
    exportPainter(p, false, QRect(), QSize(svg.width(), svg.height()));
    const bool endOk = p.end();
    const bool flushOk = outFile.flush();
    const QFile::FileError ferr = outFile.error();
    const QString ferrStr = outFile.errorString();
    const qint64 writtenBytes = outFile.size();
    outFile.close();
    if (!endOk || !flushOk || ferr != QFile::NoError || writtenBytes <= 0) {
        if (errOut)
            *errOut = ferrStr.isEmpty() ? tr("Failed to write SVG file: %1").arg(fname) : ferrStr;
        return false;
    }
    return true;
#else
    if (errOut)
        *errOut = tr("SVG export is not supported in this build.");
    return false;
#endif
}

void Graph::exportSVG(const QString &fname)
{
    (void)exportSVGChecked(fname, nullptr);
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

    ScopedExportFlag exportGuard(d_is_exporting);
    painter.save();
    painter.scale((double)size.width() / (double)rect.width(),
                  (double)size.height() / (double)rect.height());

    print(&painter, rect);
    painter.restore();
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

void Graph::contextMenuEvent(QContextMenuEvent *e)
{
    if (selectedMarker < 0
        && (d_selection.kind == GraphSelection::LegendBox
            || d_selection.kind == GraphSelection::TextAnnotation
            || d_selection.kind == GraphSelection::ArrowAnnotation
            || d_selection.kind == GraphSelection::ImageAnnotation)
        && d_selection.primaryId >= 0 && d_plot && d_plot->marker(d_selection.primaryId)) {
        selectedMarker = d_selection.primaryId;
    }
    if (selectedMarker >= 0) {
        emit showMarkerPopupMenu();
        e->accept();
        return;
    }

    QPoint pos = d_plot->canvas()->mapFrom(d_plot, e->pos());
    int dist, point;
    long curve = d_plot->closestCurve(pos.x(), pos.y(), dist, point);
    QwtPlotCurve *c = d_plot->curve(curve);
    if (QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(c)) {
        if (DataCurve *mc = er->masterCurve()) {
            const int mcIdx = curveIndex(mc);
            if (mcIdx >= 0) {
                curve = curveKey(mcIdx);
                c = mc;
            }
        }
    }

    const double dpr = qMax(1.0, double(d_plot ? d_plot->devicePixelRatioF() : 1.0));
    const int hitTolerance = qMax(10, qRound(10.0 * dpr));
    if (c && dist < hitTolerance)
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
    if (stepZoomHistory(-1))
        return;

    d_zoomer[0]->zoom(-1);
    d_zoomer[1]->zoom(-1);

    updateSecondaryAxis(QwtPlot::xTop);
    updateSecondaryAxis(QwtPlot::yRight);
    syncChildLinkedLayers();
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

void Graph::showIntensityTable()
{
    ImageMarker *mrk = (ImageMarker *)d_plot->marker(selectedMarker);
    if (!mrk)
        return;

    emit createIntensityTable(mrk->fileName());
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
    updateSelectionOverlayGeometry();
    // The canvas has its new size: the markers' pixel caches follow (no data change).
    d_plot->updateLayout();
    refreshMarkerPixelCaches();
}

void Graph::scaleFonts(double factor)
{
    if (!std::isfinite(factor) || factor <= 0.0 || std::fabs(factor - 1.0) < 1e-6)
        return;

    auto scaledFont = [factor](QFont font) -> QFont {
        double curPt = font.pointSizeF();
        if (curPt <= 0.0)
            curPt = (font.pointSize() > 0) ? double(font.pointSize()) : 10.0;
        const double nextPt = qBound(4.0, factor * curPt, 144.0);
        font.setPointSizeF(nextPt);
        font.setHintingPreference(QFont::PreferNoHinting);
        return font;
    };

    for (int i = 0; i < (int)d_texts.size(); i++) {
        Legend *mrk = (Legend *)d_plot->marker(d_texts[i]);
        if (mrk)
            mrk->setFont(scaledFont(mrk->font()));
    }
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_plot->setAxisFont(i, scaledFont(axisFont(i)));

        QwtText title = d_plot->axisTitle(i);
        title.setFont(scaledFont(title.font()));
        d_plot->setAxisTitle(i, title);
    }

    QwtText title = d_plot->title();
    title.setFont(scaledFont(title.font()));
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

    const bool opaque = (color.alpha() > 0);
    setAutoFillBackground(opaque);
    d_plot->setAutoFillBackground(opaque);
    emit modifiedGraph();
}

void Graph::setCanvasBackground(const QColor &color)
{
    d_plot->setCanvasBackground(color);
    emit modifiedGraph();
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
    QAction *actEditInline = titleMenu.addAction(
            IconLoader::duotoneIcon(QStringLiteral("text-label")), tr("&Edit Text Inline"), this,
            [this]() {
                if (!d_plot || !d_plot->titleLabel())
                    return;
                QwtTextLabel *tl = d_plot->titleLabel();
                const QRect gRect(tl->mapToGlobal(QPoint(0, 0)), tl->size());
                CanvasInlineTextEditor::openForPlotTitle(this, gRect);
            });
    actEditInline->setShortcut(QKeySequence(Qt::Key_F2));
    titleMenu.addSeparator();
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
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;
    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    d_axis_title_auto[axis] = false;
    QwtText t = d_plot->axisTitle(axis);
    t.setText(QString());
    d_plot->setAxisTitle(axis, t);
    if (d_selection.kind == GraphSelection::AxisTitle && d_selection.primaryId == axis)
        setCurrentSelection({ GraphSelection::None, -1, -1 });
    d_plot->replot();
    updateSelectionOverlayGeometry();
    emit modifiedGraph();
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Remove Axis Title"), beforeSnap);
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
    QApplication::clipboard()->setText(axisTitle(axis), QClipboard::Clipboard);
}

void Graph::showAxisTitleMenu(int axis)
{
    selectedAxis = axis;
    const int qwtAxis = Plot::scaleAlignmentToAxis(axis);

    QMenu titleMenu(this);
    QAction *actEditInline = titleMenu.addAction(
            IconLoader::duotoneIcon(QStringLiteral("text-label")), tr("&Edit Text Inline"), this,
            [this, qwtAxis]() {
                if (!d_plot || qwtAxis < 0 || qwtAxis >= QwtPlot::axisCnt)
                    return;
                QwtScaleWidget *sw = d_plot->axisWidget(qwtAxis);
                if (!sw)
                    return;
                QRect trLocal = scalePicker ? scalePicker->titleRect(sw) : sw->rect();
                if (!trLocal.isValid() || trLocal.isEmpty())
                    trLocal = sw->rect();
                const QRect gRect(sw->mapToGlobal(trLocal.topLeft()), trLocal.size());
                CanvasInlineTextEditor::openForAxisTitle(this, qwtAxis, gRect);
            });
    actEditInline->setShortcut(QKeySequence(Qt::Key_F2));
    titleMenu.addSeparator();
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
    const int qwtAxis = Plot::scaleAlignmentToAxis(axis);
    const QPoint globalClick = QCursor::pos();
    const QPoint canvasPos = (d_plot && d_plot->canvas())
            ? d_plot->canvas()->mapFromGlobal(globalClick)
            : QPoint();

    QMenu menu(this);
    if (qwtAxis >= 0 && qwtAxis < QwtPlot::axisCnt) {
        QAction *rescaleThisAct = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("zoom-auto")), tr("Rescale &This Axis"));
        connect(rescaleThisAct, &QAction::triggered, this, [this, qwtAxis]() {
            MultiLayer *ml = multiLayerParent();
            const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
            setAutoScaleAxis(qwtAxis);
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Rescale Axis"), beforeSnap);
        });
    }
    QAction *rescaleAllAct = menu.addAction(IconLoader::duotoneIcon(QStringLiteral("zoom-auto")),
                                            tr("&Rescale to show all"));
    rescaleAllAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    connect(rescaleAllAct, &QAction::triggered, this, [this]() {
        MultiLayer *ml = multiLayerParent();
        const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
        setAutoScale();
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(tr("Autoscale Axes"), beforeSnap);
    });

    if (qwtAxis >= 0 && qwtAxis < QwtPlot::axisCnt && d_plot && !isPiePlot() && !isTernaryPlot()) {
        QAction *addRefAct =
                menu.addAction(IconLoader::duotoneIcon(QStringLiteral("draw-line")),
                               tr("Add &Reference Line Here"));
        connect(addRefAct, &QAction::triggered, this, [this, qwtAxis, canvasPos]() {
            const bool isHoriz = (qwtAxis == QwtPlot::yLeft || qwtAxis == QwtPlot::yRight);
            const int mapAxis = isHoriz ? QwtPlot::yLeft : QwtPlot::xBottom;
            const QwtScaleMap map = d_plot->canvasMap(mapAxis);
            double val = map.invTransform(isHoriz ? canvasPos.y() : canvasPos.x());
            if (!std::isfinite(val)) {
                const QwtScaleDiv *div = d_plot->axisScaleDiv(mapAxis);
                val = div ? 0.5 * (div->lowerBound() + div->upperBound()) : 0.0;
            }
            MultiLayer *ml = multiLayerParent();
            const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
            ReferenceLineSpec spec;
            spec.orientation = isHoriz ? 0 : 1;
            spec.value = val;
            spec.style = Qt::DashLine;
            spec.width = 1.5;
            spec.color = QColor(30, 102, 184);
            spec.label = QStringLiteral("%1 = %2")
                                 .arg(isHoriz ? QStringLiteral("Y") : QStringLiteral("X"))
                                 .arg(QLocale::c().toString(val, 'g', 4));
            addReferenceLine(spec);
            setCurrentSelection(
                    { GraphSelection::ReferenceLine, int(d_ref_lines.size()) - 1, -1 });
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Add Reference Line"), beforeSnap);
        });

        int matchCurveIdx = -1;
        if (d_selection.kind == GraphSelection::Curve && d_selection.primaryId >= 0
            && d_selection.primaryId < n_curves) {
            if (QwtPlotItem *selIt = plotItem(d_selection.primaryId)) {
                if (selIt->xAxis() == qwtAxis || selIt->yAxis() == qwtAxis)
                    matchCurveIdx = d_selection.primaryId;
            }
        }
        if (matchCurveIdx < 0) {
            for (int ci = 0; ci < n_curves; ++ci) {
                if (ci < c_type.size() && c_type[ci] == ErrorBars)
                    continue;
                QwtPlotItem *it = plotItem(ci);
                if (it && it->isVisible() && (it->xAxis() == qwtAxis || it->yAxis() == qwtAxis)) {
                    matchCurveIdx = ci;
                    break;
                }
            }
        }
        if (matchCurveIdx >= 0) {
            QAction *colorMatchAct =
                    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("palette")),
                                   tr("&Color Axis to Match Curve"));
            connect(colorMatchAct, &QAction::triggered, this, [this, qwtAxis, matchCurveIdx]() {
                MultiLayer *ml = multiLayerParent();
                const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
                syncAxisColorToCurve(qwtAxis, matchCurveIdx);
                if (ml && !beforeSnap.isEmpty())
                    ml->pushUndoSnapshot(tr("Color Axis to Match Curve"), beforeSnap);
            });
        }
    }

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
    menu.exec(globalClick);
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

    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    d_plot->enableAxis(axis, false);
    scalePicker->refresh();
    if (d_selection.kind == GraphSelection::Axis && d_selection.primaryId == axis)
        setCurrentSelection({ GraphSelection::None, -1, -1 });
    emit modifiedGraph();
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Hide Axis"), beforeSnap);
}

void Graph::showGrids()
{
    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    showGrid(selectedAxis);
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Toggle Grids"), beforeSnap);
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
    if (!d_range_selector->selectedCurve()) {
        delete d_range_selector;
        return false;
    }
    connect(d_range_selector, SIGNAL(changed()), this, SIGNAL(dataRangeChanged()));
    return true;
}

QString Graph::parentPlotName()
{
    if (MultiLayer *ml = multiLayerParent())
        return QString(ml->objectName());
    QObject *p = parent();
    while (p) {
        if (QWidget *w = qobject_cast<QWidget *>(p)) {
            if (!w->objectName().isEmpty() && w->objectName() != QLatin1String("MultiLayerPageHost")
                && w->objectName() != QLatin1String("MultiLayerCanvasScroll")
                && w->objectName() != QLatin1String("qt_scrollarea_viewport"))
                return QString(w->objectName());
        }
        p = p->parent();
    }
    return QString();
}

Graph::~Graph()
{
    CanvasInlineTextEditor::cancelActiveForGraph(this);
    cancelInlineScaleEditor();
    detachAndDeleteMarkerSelector(d_markers_selector);
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

    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotItem *c = d_plot->plotItem(c_keys[i]);
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

bool Graph::focusNextPrevChild(bool next)
{
    QVector<GraphSelection> targets;
    targets.reserve(n_curves + d_texts.size() + d_lines.size() + d_images.size()
                    + d_ref_lines.size() + 10);

    // 0. Visible plot title (B4-8)
    if (d_plot && d_plot->titleLabel() && d_plot->titleLabel()->isVisible()
        && !d_plot->title().text().trimmed().isEmpty()) {
        targets.append({ GraphSelection::PlotTitle, -1, -1 });
    }

    // 1. Visible curves
    for (int i = 0; i < n_curves; ++i) {
        if (i < c_type.size() && c_type[i] == ErrorBars)
            continue;
        QwtPlotItem *it = plotItem(i);
        if (it && it->isVisible())
            targets.append({ GraphSelection::Curve, i, -1 });
    }

    // 2. Legend
    if (legendMarkerID >= 0 && isLegendVisible()) {
        if (Legend *leg = legend()) {
            if (leg->isVisible())
                targets.append({ GraphSelection::LegendBox, int(legendMarkerID), -1 });
        }
    }

    // 3. Text, Arrow/Line, and Image annotations
    for (int id : d_texts) {
        if (id == legendMarkerID)
            continue;
        if (Legend *tm = textMarker(id)) {
            if (tm->isVisible())
                targets.append({ GraphSelection::TextAnnotation, id, -1 });
        }
    }
    for (int id : d_lines) {
        if (ArrowMarker *am = arrow(id)) {
            if (am->isVisible())
                targets.append({ GraphSelection::ArrowAnnotation, id, -1 });
        }
    }
    for (int id : d_images) {
        if (ImageMarker *im = imageMarker(id)) {
            if (im->isVisible())
                targets.append({ GraphSelection::ImageAnnotation, id, -1 });
        }
    }

    // 4. Reference lines
    for (int r = 0; r < d_ref_lines.size(); ++r)
        targets.append({ GraphSelection::ReferenceLine, r, -1 });

    // 5. Enabled axes and their visible titles (B4-8)
    if (d_plot) {
        const int axisOrder[4] = { QwtPlot::xBottom, QwtPlot::yLeft, QwtPlot::xTop,
                                   QwtPlot::yRight };
        for (int axisId : axisOrder) {
            if (!d_plot->axisEnabled(axisId))
                continue;
            QwtScaleWidget *sw = d_plot->axisWidget(axisId);
            if (!sw || !sw->isVisible())
                continue;
            targets.append({ GraphSelection::Axis, axisId, -1 });
            if (!axisTitle(axisId).trimmed().isEmpty())
                targets.append({ GraphSelection::AxisTitle, axisId, -1 });
        }
    }

    auto focusExternalPane = [&]() -> bool {
        QWidget *boundary = multiLayerParent() ? static_cast<QWidget *>(multiLayerParent())
                                               : static_cast<QWidget *>(this);
        QWidget *win = boundary ? boundary->window() : nullptr;
        QWidget *cur = QApplication::focusWidget();
        if (!cur || !boundary || (cur != boundary && !boundary->isAncestorOf(cur)))
            cur = boundary;
        if (!cur || !win)
            return false;
        QWidget *cand = next ? cur->nextInFocusChain() : cur->previousInFocusChain();
        QWidget *firstCand = cand;
        int guard = 0;
        while (cand && guard++ < 4096) {
            if (cand != boundary && !boundary->isAncestorOf(cand) && cand->window() == win
                && cand->isVisible() && cand->isEnabled()
                && (cand->focusPolicy() & Qt::TabFocus)) {
                deselect();
                cand->setFocus(next ? Qt::TabFocusReason : Qt::BacktabFocusReason);
                return true;
            }
            cand = next ? cand->nextInFocusChain() : cand->previousInFocusChain();
            if (cand == firstCand || cand == cur)
                break;
        }
        return false;
    };

    if (targets.isEmpty())
        return focusExternalPane();

    int curIdx = -1;
    for (int k = 0; k < targets.size(); ++k) {
        if (targets[k].kind == d_selection.kind && targets[k].primaryId == d_selection.primaryId) {
            curIdx = k;
            break;
        }
    }
    if (curIdx < 0 && selectedMarker >= 0) {
        for (int k = 0; k < targets.size(); ++k) {
            if ((targets[k].kind == GraphSelection::LegendBox
                 || targets[k].kind == GraphSelection::TextAnnotation
                 || targets[k].kind == GraphSelection::ArrowAnnotation
                 || targets[k].kind == GraphSelection::ImageAnnotation)
                && targets[k].primaryId == selectedMarker) {
                curIdx = k;
                break;
            }
        }
    }

    const int n = targets.size();
    // Allow Tab / Shift+Tab at the last / first element to leave the graph to adjacent docks/panes (B4-8).
    if (curIdx >= 0 && ((next && curIdx == n - 1) || (!next && curIdx == 0))) {
        if (focusExternalPane())
            return true;
    }

    const int nextIdx = (curIdx < 0) ? (next ? 0 : (n - 1))
                                     : (next ? ((curIdx + 1) % n) : ((curIdx - 1 + n) % n));

    if (cp)
        cp->disableEditing();

    const Qt::FocusReason reason = next ? Qt::TabFocusReason : Qt::BacktabFocusReason;
    const GraphSelection &chosen = targets[nextIdx];
    if (chosen.kind == GraphSelection::PlotTitle) {
        if (titlePicker)
            titlePicker->setSelected(true);
        if (d_plot && d_plot->titleLabel())
            d_plot->titleLabel()->setFocus(reason);
        setCurrentSelection(chosen);
    } else if (chosen.kind == GraphSelection::Axis) {
        if (scalePicker && d_plot) {
            if (QwtScaleWidget *sw = d_plot->axisWidget(chosen.primaryId)) {
                scalePicker->selectLabels(sw, true);
                sw->setFocus(reason);
            }
        }
        setCurrentSelection(chosen);
    } else if (chosen.kind == GraphSelection::AxisTitle) {
        if (scalePicker && d_plot) {
            if (QwtScaleWidget *sw = d_plot->axisWidget(chosen.primaryId)) {
                scalePicker->selectTitle(sw, true);
                sw->setFocus(reason);
            }
        }
        setCurrentSelection(chosen);
    } else if (chosen.kind == GraphSelection::LegendBox
               || chosen.kind == GraphSelection::TextAnnotation
               || chosen.kind == GraphSelection::ArrowAnnotation
               || chosen.kind == GraphSelection::ImageAnnotation) {
        setSelectedMarker(chosen.primaryId);
        setCurrentSelection(chosen);
        if (d_plot && d_plot->canvas() && !d_markers_selector)
            d_plot->canvas()->setFocus(reason);
    } else {
        setCurrentSelection(chosen);
        if (d_plot && d_plot->canvas())
            d_plot->canvas()->setFocus(reason);
    }
    return true;
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

void Graph::print(QPainter *painter, const QRect &plotRect, const QwtPlotPrintFilter &pfilter)
{
    ScopedExportFlag exportGuard(d_is_exporting);
    struct ScopedTransientItemsHider {
        QVector<QwtPlotItem *> hiddenItems;
        ScopedTransientItemsHider(Plot *plot, bool exporting)
        {
            if (!plot || !exporting)
                return;
            const QwtPlotItemList &items = plot->itemList();
            for (QwtPlotItem *it : items) {
                if (it && it->isVisible()
                    && it->title().text().startsWith(QLatin1String("__tg_transient_ui_"))) {
                    it->setVisible(false);
                    hiddenItems.append(it);
                }
            }
        }
        ~ScopedTransientItemsHider()
        {
            for (QwtPlotItem *it : hiddenItems) {
                if (it)
                    it->setVisible(true);
            }
        }
    } transientGuard(d_plot, d_is_exporting);
    if (d_plot) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            QFont af = d_plot->axisFont(a);
            if (af.hintingPreference() != QFont::PreferNoHinting) {
                af.setHintingPreference(QFont::PreferNoHinting);
                d_plot->setAxisFont(a, af);
            }
            QwtText at = d_plot->axisTitle(a);
            QFont atf = at.font();
            if (atf.hintingPreference() != QFont::PreferNoHinting) {
                atf.setHintingPreference(QFont::PreferNoHinting);
                at.setFont(atf);
                d_plot->setAxisTitle(a, at);
            }
        }
        QwtText pt = d_plot->title();
        QFont ptf = pt.font();
        if (ptf.hintingPreference() != QFont::PreferNoHinting) {
            ptf.setHintingPreference(QFont::PreferNoHinting);
            pt.setFont(ptf);
            d_plot->setTitle(pt);
        }
        for (int tid : d_texts) {
            if (Legend *mrk = dynamic_cast<Legend *>(d_plot->marker(tid))) {
                QFont mf = mrk->font();
                if (mf.hintingPreference() != QFont::PreferNoHinting)
                    mrk->setFont(mf);
            }
        }
    }
    d_plot->print(painter, plotRect, pfilter);
}

void Graph::deselect()
{
    deselectMarker();
    if (scalePicker)
        scalePicker->deselect();
    if (titlePicker)
        titlePicker->setSelected(false);
    if (d_plot)
        d_plot->setHighlightedCurveKey(-1);
    setCurrentSelection({ GraphSelection::None, -1, -1 });

    // A deselected title/axis must not keep keyboard focus: otherwise Delete / arrow keys
    // still reach the (invisibly) focused title or scale widget after Esc (B4-3).
    if (d_plot) {
        bool hadFocus = false;
        if (QwtTextLabel *title = d_plot->titleLabel()) {
            if (title->hasFocus()) {
                title->clearFocus();
                hadFocus = true;
            }
        }
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            if (QwtScaleWidget *sw = d_plot->axisWidget(a)) {
                if (sw->hasFocus()) {
                    sw->clearFocus();
                    hadFocus = true;
                }
            }
        }
        if (hadFocus && d_plot->canvas())
            d_plot->canvas()->setFocus(Qt::OtherFocusReason);
    }
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

        const bool logX = xMap.transformation()
                && xMap.transformation()->type() == QwtScaleTransformation::Log10;
        const bool logY = yMap.transformation()
                && yMap.transformation()->type() == QwtScaleTransformation::Log10;

        painter->save();
        for (const Graph::ReferenceLineSpec &spec : lines) {
            if (!spec.visible || !spec.fillToAxis || !std::isfinite(spec.value))
                continue;
            if (spec.orientation == 0) {
                if (logY && spec.value <= 0.0)
                    continue;
                const double yPx = yMap.xTransform(spec.value);
                const double yLo = qMin(yMap.s1(), yMap.s2());
                const double yHi = qMax(yMap.s1(), yMap.s2());
                const double basePx = (!logY && 0.0 >= yLo && 0.0 <= yHi)
                        ? yMap.xTransform(0.0)
                        : double(canvasRect.bottom());
                if (!std::isfinite(yPx) || !std::isfinite(basePx))
                    continue;
                QRectF fillRect(canvasRect.left(), qMin(yPx, basePx), canvasRect.width(),
                                qAbs(basePx - yPx));
                fillRect = fillRect.intersected(QRectF(canvasRect));
                if (!fillRect.isEmpty())
                    painter->fillRect(fillRect, spec.fillColor);
            } else {
                if (logX && spec.value <= 0.0)
                    continue;
                const double xPx = xMap.xTransform(spec.value);
                const double xLo = qMin(xMap.s1(), xMap.s2());
                const double xHi = qMax(xMap.s1(), xMap.s2());
                const double basePx = (!logX && 0.0 >= xLo && 0.0 <= xHi)
                        ? xMap.xTransform(0.0)
                        : double(canvasRect.left());
                if (!std::isfinite(xPx) || !std::isfinite(basePx))
                    continue;
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
        setZ(18.0);
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

        const QwtMetricsMap savedMetricsMap = QwtPainter::metricsMap();
        struct ScopedResetMetricsMap {
            QwtMetricsMap prev;
            ~ScopedResetMetricsMap() { QwtPainter::setMetricsMap(prev); }
        } metricsGuard{ savedMetricsMap };
        QwtPainter::resetMetricsMap();

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing,
                               const_cast<Graph *>(d_graph)->antialiasing());

        const QFont axNumFnt = d_graph->axisNumbersFont(QwtPlot::yLeft);
        const double baseAxPt = axNumFnt.pointSizeF() > 0.0
                ? axNumFnt.pointSizeF()
                : (axNumFnt.pointSize() > 0 ? double(axNumFnt.pointSize()) : 10.0);
        const double refPt = qMax(8.0, baseAxPt - 1.0);
        QFont labelFont(QStringLiteral("Arial"));
        labelFont.setPointSizeF(refPt);
        labelFont.setWeight(QFont::Normal);
        labelFont.setBold(false);
        labelFont.setHintingPreference(QFont::PreferNoHinting);
        const QFontMetricsF fm(labelFont, painter->device());

        const Plot *pl = d_graph->plotWidget();
        const int cw = (pl && pl->canvas() && pl->canvas()->width() > 0)
                ? pl->canvas()->width()
                : qMax(1, canvasRect.width());
        const int ch = (pl && pl->canvas() && pl->canvas()->height() > 0)
                ? pl->canvas()->height()
                : qMax(1, canvasRect.height());
        const double sx = double(qMax(1, canvasRect.width())) / double(qMax(1, cw));
        const double sy = double(qMax(1, canvasRect.height())) / double(qMax(1, ch));

        double richScaleX = 1.0;
        double richScaleY = 1.0;
        if (painter->device() && pl) {
            const int devDpiX = painter->device()->logicalDpiX();
            const int devDpiY = painter->device()->logicalDpiY();
            const int scrDpiX = pl->logicalDpiX();
            const int scrDpiY = pl->logicalDpiY();
            if (devDpiX > 0 && scrDpiX > 0)
                richScaleX = double(devDpiX) / double(scrDpiX);
            if (devDpiY > 0 && scrDpiY > 0)
                richScaleY = double(devDpiY) / double(scrDpiY);
        }

        const int majTickLen = pl ? qMax(0, pl->majorTickLength()) : 6;
        const double tickInsetX = (double(majTickLen) + 6.0) * sx;
        const double tickInsetY = (double(majTickLen) + 6.0) * sy;

        auto mapWidgetCanvasRect = [&](const QRect &wr) -> QRectF {
            if (wr.isEmpty())
                return QRectF();
            return QRectF(double(canvasRect.left()) + double(wr.left()) * sx,
                          double(canvasRect.top()) + double(wr.top()) * sy,
                          double(wr.width()) * sx, double(wr.height()) * sy);
        };

        QVector<QPointF> curvePts;
        if (pl) {
            for (int ci = 0; ci < d_graph->curves(); ++ci) {
                const QwtPlotCurve *cv = d_graph->curve(ci);
                if (!cv || !cv->isVisible() || cv->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
                    continue;
                const DataCurve *dc = dynamic_cast<const DataCurve *>(cv);
                const bool useDirectX = (cv->xAxis() == QwtPlot::xBottom);
                const bool useDirectY = (cv->yAxis() == QwtPlot::yLeft);
                const QwtScaleMap cxMap = useDirectX ? xMap : pl->canvasMap(cv->xAxis());
                const QwtScaleMap cyMap = useDirectY ? yMap : pl->canvasMap(cv->yAxis());
                const int n = cv->dataSize();
                const int stride = qMax(1, n / 300);
                Column *dcXCol = nullptr;
                Column *dcYCol = nullptr;
                const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
                if (dcHasMasks)
                    dc->resolveColumns(dcXCol, dcYCol);
                for (int k = 0; k < n; k += stride) {
                    if (dcHasMasks && dc->isPointMaskedFast(k, dcXCol, dcYCol))
                        continue;
                    const double rawPx = cxMap.xTransform(cv->x(k));
                    const double rawPy = cyMap.xTransform(cv->y(k));
                    if (!std::isfinite(rawPx) || !std::isfinite(rawPy))
                        continue;
                    const double px = useDirectX ? rawPx
                                                 : (double(canvasRect.left()) + rawPx * sx);
                    const double py = useDirectY ? rawPy
                                                 : (double(canvasRect.top()) + rawPy * sy);
                    if (std::isfinite(px) && std::isfinite(py))
                        curvePts.append(QPointF(px, py));
                }
            }
        }

        QVector<QRectF> avoidBoxes;
        QVector<QRectF> textMaskBoxes;
        ::Legend *mainLeg = const_cast<Graph *>(d_graph)->legend();
        if (d_graph->isLegendVisible() && mainLeg && !mainLeg->text().trimmed().isEmpty()) {
            const QRectF lb = mapWidgetCanvasRect(mainLeg->rect());
            if (!lb.isEmpty()) {
                avoidBoxes.append(lb);
                textMaskBoxes.append(lb.adjusted(-2.0 * sx, -1.5 * sy, 2.0 * sx, 1.5 * sy));
            }
        }
        const QVector<int> txtKeys = const_cast<Graph *>(d_graph)->textMarkerKeys();
        for (int tid : txtKeys) {
            ::Legend *tm = const_cast<Graph *>(d_graph)->textMarker(tid);
            if (!tm || !tm->isVisible() || tm == mainLeg || tm->text().trimmed().isEmpty())
                continue;
            const QRectF tb = mapWidgetCanvasRect(tm->rect());
            if (!tb.isEmpty()) {
                avoidBoxes.append(tb);
                textMaskBoxes.append(tb.adjusted(-2.0 * sx, -1.5 * sy, 2.0 * sx, 1.5 * sy));
            }
        }
        const QVector<int> lineKeys = const_cast<Graph *>(d_graph)->lineMarkerKeys();
        for (int lid : lineKeys) {
            ArrowMarker *am = const_cast<Graph *>(d_graph)->arrow(lid);
            if (!am || !am->isVisible())
                continue;
            const double ax0 = xMap.xTransform(am->startPointCoord().x());
            const double ay0 = yMap.xTransform(am->startPointCoord().y());
            const double ax1 = xMap.xTransform(am->endPointCoord().x());
            const double ay1 = yMap.xTransform(am->endPointCoord().y());
            if (std::isfinite(ax0) && std::isfinite(ay0) && std::isfinite(ax1)
                && std::isfinite(ay1)) {
                avoidBoxes.append(QRectF(QPointF(qMin(ax0, ax1) - 5.0 * sx, qMin(ay0, ay1) - 5.0 * sy),
                                         QPointF(qMax(ax0, ax1) + 5.0 * sx, qMax(ay0, ay1) + 5.0 * sy)));
            }
        }
        if (MultiLayer *ml = d_graph->multiLayerParent()) {
            const QPoint myCanvasOrigin =
                    d_graph->pos() + (pl ? pl->plotLayout()->canvasRect().topLeft() : QPoint(0, 0));
            const QList<Graph *> allGraphs = ml->layersList();
            for (Graph *ins : allGraphs) {
                if (!ins || ins == d_graph || ins->isHidden() || !ml->isInsetLayer(ins)
                    || ml->insetSourceLayer(ins) != d_graph)
                    continue;
                const QSize insSz = ins->plotWidget() ? ins->plotWidget()->size() : ins->size();
                const QRect insLocal(ins->pos() - myCanvasOrigin, insSz);
                const QRectF ib = mapWidgetCanvasRect(insLocal);
                if (!ib.isEmpty())
                    avoidBoxes.append(ib);
            }
        }

        auto scorePill = [&](const QRectF &pill) -> int {
            int hits = 0;
            const QRectF testRc = pill.adjusted(-5.0 * sx, -4.0 * sy, 5.0 * sx, 4.0 * sy);
            for (const QRectF &ab : avoidBoxes) {
                if (!ab.isEmpty() && ab.intersects(testRc))
                    hits += 1000;
            }
            for (const QPointF &pt : curvePts) {
                if (testRc.contains(pt))
                    ++hits;
            }
            return hits;
        };

        static const QRegularExpression kRichTagRx(
                QStringLiteral("<\\s*(?:i|sub|sup|b|u|span|font|br)\\b"),
                QRegularExpression::CaseInsensitiveOption);
        const bool logX = xMap.transformation()
                && xMap.transformation()->type() == QwtScaleTransformation::Log10;
        const bool logY = yMap.transformation()
                && yMap.transformation()->type() == QwtScaleTransformation::Log10;

        for (const Graph::ReferenceLineSpec &spec : lines) {
            if (!spec.visible || !std::isfinite(spec.value))
                continue;

            QString displayLabel = spec.label.trimmed();
            if (displayLabel.isEmpty() && !spec.statMode.trimmed().isEmpty()
                && spec.statMode.trimmed().compare("Constant", Qt::CaseInsensitive) != 0) {
                displayLabel = QString("%1 = %2")
                                       .arg(localizedStatModeLabel(spec.statMode))
                                       .arg(QLocale::c().toString(spec.value, 'g', 4));
            }

            auto makeLabelQwtText = [&](double &twOut, double &thOut) -> QwtText {
                const bool isRich = displayLabel.contains(kRichTagRx);
                QString txtStr = displayLabel;
                if (isRich && !txtStr.contains(QLatin1Char('\n'))
                    && !txtStr.contains(QLatin1String("<br"), Qt::CaseInsensitive)) {
                    txtStr = QStringLiteral("<span style=\"white-space:nowrap;\">") + txtStr
                            + QStringLiteral("</span>");
                }
                QwtText qt(txtStr, isRich ? QwtText::RichText : QwtText::PlainText);
                qt.setFont(labelFont);
                qt.setColor(spec.color.darker(115));
                qt.setRenderFlags(Qt::AlignLeft | Qt::AlignVCenter);
                if (isRich) {
                    const QSize sz = qt.textSize(labelFont);
                    twOut = qMax(8.0 * sx, double(sz.width()) * richScaleX);
                    thOut = qMax(double(fm.height()), double(sz.height()) * richScaleY);
                } else {
                    twOut = qMax(8.0 * sx, fm.horizontalAdvance(displayLabel) + 2.0 * sx);
                    thOut = qMax(10.0 * sy, fm.height());
                }
                return qt;
            };

            const double padX = 3.0 * sx;
            const double padY = 1.0 * sy;
            const double offY = 4.0 * sy;
            const double offX = 6.0 * sx;

            const double devSc = Plot::printDeviceScale();
            auto drawMaskedHLine = [&](double yPx) {
                QVector<QPair<double, double>> spans;
                spans.append(qMakePair(double(canvasRect.left()), double(canvasRect.right())));
                for (const QRectF &mb : textMaskBoxes) {
                    if (yPx < mb.top() || yPx > mb.bottom())
                        continue;
                    QVector<QPair<double, double>> next;
                    for (const auto &sp : spans) {
                        if (mb.right() <= sp.first || mb.left() >= sp.second) {
                            next.append(sp);
                        } else {
                            if (mb.left() - sp.first > 1.5 * sx)
                                next.append(qMakePair(sp.first, mb.left()));
                            if (sp.second - mb.right() > 1.5 * sx)
                                next.append(qMakePair(mb.right(), sp.second));
                        }
                    }
                    spans.swap(next);
                }
                for (const auto &sp : spans)
                    painter->drawLine(QPointF(sp.first, yPx), QPointF(sp.second, yPx));
            };
            auto drawMaskedVLine = [&](double xPx) {
                QVector<QPair<double, double>> spans;
                spans.append(qMakePair(double(canvasRect.top()), double(canvasRect.bottom())));
                for (const QRectF &mb : textMaskBoxes) {
                    if (xPx < mb.left() || xPx > mb.right())
                        continue;
                    QVector<QPair<double, double>> next;
                    for (const auto &sp : spans) {
                        if (mb.bottom() <= sp.first || mb.top() >= sp.second) {
                            next.append(sp);
                        } else {
                            if (mb.top() - sp.first > 1.5 * sy)
                                next.append(qMakePair(sp.first, mb.top()));
                            if (sp.second - mb.bottom() > 1.5 * sy)
                                next.append(qMakePair(mb.bottom(), sp.second));
                        }
                    }
                    spans.swap(next);
                }
                for (const auto &sp : spans)
                    painter->drawLine(QPointF(xPx, sp.first), QPointF(xPx, sp.second));
            };

            if (spec.orientation == 0) {
                if (logY && spec.value <= 0.0)
                    continue;
                const double yPx = yMap.xTransform(spec.value);
                if (std::isfinite(yPx) && yPx >= canvasRect.top() - 2
                    && yPx <= canvasRect.bottom() + 2) {
                    QPen pen(spec.color, qMax(0.8, spec.width) * devSc, spec.style);
                    pen.setCosmetic(false);
                    painter->setPen(pen);
                    drawMaskedHLine(yPx);

                    if (!displayLabel.isEmpty()) {
                        double tw = 0.0, th = 0.0;
                        QwtText qt = makeLabelQwtText(tw, th);
                        const double minTx = canvasRect.left() + tickInsetX;
                        const double maxTx = qMax(minTx, canvasRect.right() - tw - tickInsetX);
                        const double yAbove = yPx - th - offY;
                        const double yBelow = yPx + offY;
                        const bool canAbove = (yAbove >= canvasRect.top() + 3.0 * sy);
                        const bool canBelow = (yBelow + th <= canvasRect.bottom() - 3.0 * sy);

                        struct CandPos {
                            double x;
                            double y;
                        };
                        QVector<CandPos> cands;
                        if (canAbove) {
                            cands.append({ maxTx, yAbove });
                            cands.append({ minTx, yAbove });
                        }
                        if (canBelow) {
                            cands.append({ maxTx, yBelow });
                            cands.append({ minTx, yBelow });
                        }
                        if (cands.isEmpty())
                            cands.append({ maxTx, qBound(double(canvasRect.top()) + 2.0 * sy, yAbove,
                                                         double(canvasRect.bottom()) - th - 2.0 * sy) });

                        CandPos best = cands.first();
                        int bestScore = scorePill(
                                QRectF(best.x - padX, best.y - padY, tw + 2.0 * padX, th + 2.0 * padY));
                        for (int ci = 1; ci < cands.size(); ++ci) {
                            const int sc = scorePill(QRectF(cands[ci].x - padX, cands[ci].y - padY,
                                                            tw + 2.0 * padX, th + 2.0 * padY));
                            if (sc < bestScore) {
                                bestScore = sc;
                                best = cands[ci];
                            }
                        }

                        const QRectF pill(best.x - padX, best.y - padY, tw + 2.0 * padX, th + 2.0 * padY);
                        painter->fillRect(pill, QColor(255, 255, 255, 215));
                        avoidBoxes.append(pill);
                        qt.draw(painter, QRectF(best.x, best.y, tw + 4.0 * sx, th).toRect());
                    }
                }
            } else {
                if (logX && spec.value <= 0.0)
                    continue;
                const double xPx = xMap.xTransform(spec.value);
                if (std::isfinite(xPx) && xPx >= canvasRect.left() - 2
                    && xPx <= canvasRect.right() + 2) {
                    QPen pen(spec.color, qMax(0.8, spec.width) * devSc, spec.style);
                    pen.setCosmetic(false);
                    painter->setPen(pen);
                    drawMaskedVLine(xPx);

                    if (!displayLabel.isEmpty()) {
                        double tw = 0.0, th = 0.0;
                        QwtText qt = makeLabelQwtText(tw, th);
                        const double minTx = canvasRect.left() + tickInsetX;
                        const double maxTx = qMax(minTx, canvasRect.right() - tw - tickInsetX);
                        const double tyTop = canvasRect.top() + tickInsetY;
                        const double tyBot = qMax(tyTop, canvasRect.bottom() - th - tickInsetY);

                        QVector<QPointF> cands;
                        if (xPx + offX + tw <= canvasRect.right() - tickInsetX + 2.0 * sx)
                            cands.append(QPointF(qBound(minTx, xPx + offX, maxTx), tyTop));
                        if (xPx - offX - tw >= canvasRect.left() + tickInsetX - 2.0 * sx)
                            cands.append(QPointF(qBound(minTx, xPx - tw - offX, maxTx), tyTop));
                        if (xPx + offX + tw <= canvasRect.right() - tickInsetX + 2.0 * sx)
                            cands.append(QPointF(qBound(minTx, xPx + offX, maxTx), tyBot));
                        if (xPx - offX - tw >= canvasRect.left() + tickInsetX - 2.0 * sx)
                            cands.append(QPointF(qBound(minTx, xPx - tw - offX, maxTx), tyBot));
                        if (cands.isEmpty())
                            cands.append(QPointF(qBound(minTx, xPx + 5.0 * sx, maxTx), tyTop));

                        QPointF best = cands.first();
                        int bestScore = scorePill(QRectF(best.x() - padX, best.y() - padY,
                                                         tw + 2.0 * padX, th + 2.0 * padY));
                        for (int ci = 1; ci < cands.size(); ++ci) {
                            const int sc = scorePill(QRectF(cands[ci].x() - padX, cands[ci].y() - padY,
                                                            tw + 2.0 * padX, th + 2.0 * padY));
                            if (sc < bestScore) {
                                bestScore = sc;
                                best = cands[ci];
                            }
                        }

                        const QRectF pill(best.x() - padX, best.y() - padY, tw + 2.0 * padX, th + 2.0 * padY);
                        painter->fillRect(pill, QColor(255, 255, 255, 215));
                        avoidBoxes.append(pill);
                        qt.draw(painter, QRectF(best.x(), best.y(), tw + 4.0 * sx, th).toRect());
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

void Graph::applyFramePreset(FramePreset preset)
{
    if (isPoleFigurePlot()) {
        enableAxis(QwtPlot::xBottom, false);
        enableAxis(QwtPlot::yLeft, false);
        enableAxis(QwtPlot::xTop, false);
        if (!hasColorScaleOnAxis(QwtPlot::yRight))
            enableAxis(QwtPlot::yRight, false);
        drawCanvasFrame(false, 0);
        drawAxesBackbones(false);
        for (QwtPlotItem *it : d_plot->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                static_cast<Spectrogram *>(it)->syncColorScale();
                break;
            }
        }
        if (scalePicker)
            scalePicker->refresh();
        d_plot->replot();
        emit modifiedGraph();
        return;
    }

    if (isTernaryPlot() || isPolarPlot() || isPiePlot()) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            enableAxis(a, false);
        drawAxesBackbones(false);
        if (scalePicker)
            scalePicker->refresh();
        d_plot->replot();
        emit modifiedGraph();
        return;
    }

    const bool rightIsColorBar = hasColorScaleOnAxis(QwtPlot::yRight);
    const bool topIsColorBar = hasColorScaleOnAxis(QwtPlot::xTop);

    bool hasRightCurve = d_weibull_prob_axis[QwtPlot::yRight]
            || !d_custom_tick_positions[QwtPlot::yRight].isEmpty()
            || !d_custom_category_labels[QwtPlot::yRight].isEmpty();
    bool hasTopCurve = d_weibull_prob_axis[QwtPlot::xTop]
            || !d_custom_tick_positions[QwtPlot::xTop].isEmpty()
            || !d_custom_category_labels[QwtPlot::xTop].isEmpty();
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
        setTickLabelsOnAxis(QwtPlot::yRight, rightIsColorBar || hasRightCurve);
        setTickLabelsOnAxis(QwtPlot::xTop, topIsColorBar || hasTopCurve);
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            if ((a == QwtPlot::yRight && rightIsColorBar) || (a == QwtPlot::xTop && topIsColorBar))
                continue;
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
        setTickLabelsOnAxis(QwtPlot::yRight, rightIsColorBar || hasRightCurve);
        setTickLabelsOnAxis(QwtPlot::xTop, topIsColorBar || hasTopCurve);
        d_plot->setMajorTicksType(QwtPlot::yLeft, ScaleDraw::In);
        d_plot->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::In);
        d_plot->setMajorTicksType(QwtPlot::xBottom, ScaleDraw::In);
        d_plot->setMinorTicksType(QwtPlot::xBottom, ScaleDraw::In);
        if (!rightIsColorBar) {
            d_plot->setMajorTicksType(QwtPlot::yRight,
                                      hasRightCurve ? ScaleDraw::In : ScaleDraw::None);
            d_plot->setMinorTicksType(QwtPlot::yRight,
                                      hasRightCurve ? ScaleDraw::In : ScaleDraw::None);
        }
        if (!topIsColorBar) {
            d_plot->setMajorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::In : ScaleDraw::None);
            d_plot->setMinorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::In : ScaleDraw::None);
        }
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
        break;
    }
    case FrameOpenL: {
        enableAxis(QwtPlot::yLeft, true);
        enableAxis(QwtPlot::xBottom, true);
        enableAxis(QwtPlot::yRight, rightIsColorBar || hasRightCurve);
        enableAxis(QwtPlot::xTop, topIsColorBar || hasTopCurve);
        setAxisBackboneVisible(QwtPlot::yLeft, true);
        setAxisBackboneVisible(QwtPlot::xBottom, true);
        setAxisBackboneVisible(QwtPlot::yRight, rightIsColorBar || hasRightCurve);
        setAxisBackboneVisible(QwtPlot::xTop, topIsColorBar || hasTopCurve);
        setTickLabelsOnAxis(QwtPlot::yLeft, true);
        setTickLabelsOnAxis(QwtPlot::xBottom, true);
        setTickLabelsOnAxis(QwtPlot::yRight, rightIsColorBar || hasRightCurve);
        setTickLabelsOnAxis(QwtPlot::xTop, topIsColorBar || hasTopCurve);
        d_plot->setMajorTicksType(QwtPlot::yLeft, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::Out);
        d_plot->setMajorTicksType(QwtPlot::xBottom, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::xBottom, ScaleDraw::Out);
        if (!rightIsColorBar) {
            d_plot->setMajorTicksType(QwtPlot::yRight,
                                      hasRightCurve ? ScaleDraw::Out : ScaleDraw::None);
            d_plot->setMinorTicksType(QwtPlot::yRight,
                                      hasRightCurve ? ScaleDraw::Out : ScaleDraw::None);
        }
        if (!topIsColorBar) {
            d_plot->setMajorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::Out : ScaleDraw::None);
            d_plot->setMinorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::Out : ScaleDraw::None);
        }
        break;
    }
    case FrameZeroCrossed: {
        enableAxis(QwtPlot::yLeft, true);
        enableAxis(QwtPlot::xBottom, true);
        enableAxis(QwtPlot::yRight, rightIsColorBar);
        enableAxis(QwtPlot::xTop, topIsColorBar);
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

    if (rightIsColorBar) {
        enableAxis(QwtPlot::yRight, true);
        setTickLabelsOnAxis(QwtPlot::yRight, true);
        d_plot->setMajorTicksType(QwtPlot::yRight, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::yRight, ScaleDraw::None);
        if (QwtScaleDraw *sd = d_plot->axisScaleDraw(QwtPlot::yRight))
            sd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
        if (QwtScaleWidget *sw = d_plot->axisWidget(QwtPlot::yRight)) {
            sw->setMargin(6);
            sw->setSpacing(0);
            if (ScaleDraw *s = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(QwtPlot::yRight)))
                s->setColorBarBorder(true, sw->colorBarWidth(), qMax(1, sw->penWidth()));
        }
    }
    if (topIsColorBar) {
        enableAxis(QwtPlot::xTop, true);
        setTickLabelsOnAxis(QwtPlot::xTop, true);
        d_plot->setMajorTicksType(QwtPlot::xTop, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::xTop, ScaleDraw::None);
        if (QwtScaleDraw *sd = d_plot->axisScaleDraw(QwtPlot::xTop))
            sd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
        if (QwtScaleWidget *sw = d_plot->axisWidget(QwtPlot::xTop)) {
            sw->setMargin(6);
            sw->setSpacing(0);
            if (ScaleDraw *s = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(QwtPlot::xTop)))
                s->setColorBarBorder(true, sw->colorBarWidth(), qMax(1, sw->penWidth()));
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
    if (d_selection.kind == GraphSelection::ReferenceLine)
        setCurrentSelection({ GraphSelection::None, -1, -1 });
    d_plot->replot();
    emit modifiedGraph();
}

int Graph::referenceLineAtCanvasPos(const QPoint &pos, int tolPx) const
{
    if (!d_plot || !d_plot->canvas() || d_ref_lines.isEmpty())
        return -1;
    const int effTol = qMax(2, tolPx);
    const QRect cr = d_plot->canvas()->rect();
    if (!cr.adjusted(-effTol, -effTol, effTol, effTol).contains(pos))
        return -1;

    const QwtScaleMap xMap = d_plot->canvasMap(QwtPlot::xBottom);
    const QwtScaleMap yMap = d_plot->canvasMap(QwtPlot::yLeft);

    double bestDist = double(effTol) + 0.001;
    int bestIdx = -1;
    for (int i = d_ref_lines.size() - 1; i >= 0; --i) {
        const ReferenceLineSpec &spec = d_ref_lines[i];
        if (!spec.visible || !std::isfinite(spec.value))
            continue;
        if (spec.orientation == 0) {
            const double yPx = yMap.transform(spec.value);
            if (!std::isfinite(yPx) || yPx < cr.top() - effTol || yPx > cr.bottom() + effTol)
                continue;
            const double d = std::fabs(double(pos.y()) - yPx);
            if (d < bestDist) {
                bestDist = d;
                bestIdx = i;
            }
        } else {
            const double xPx = xMap.transform(spec.value);
            if (!std::isfinite(xPx) || xPx < cr.left() - effTol || xPx > cr.right() + effTol)
                continue;
            const double d = std::fabs(double(pos.x()) - xPx);
            if (d < bestDist) {
                bestDist = d;
                bestIdx = i;
            }
        }
    }
    return bestIdx;
}

void Graph::updateReferenceLine(int index, const ReferenceLineSpec &spec)
{
    if (index < 0 || index >= d_ref_lines.size())
        return;
    if (!d_ref_fill_item) {
        d_ref_fill_item = new OriginReferenceFillItem(this);
        d_ref_fill_item->attach(d_plot);
    }
    if (!d_ref_line_item) {
        d_ref_line_item = new OriginReferenceLineItem(this);
        d_ref_line_item->attach(d_plot);
    }
    d_ref_lines[index] = spec;
    updateStatReferenceLines();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::removeReferenceLine(int index)
{
    if (index < 0 || index >= d_ref_lines.size())
        return;
    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    d_ref_lines.remove(index);
    if (d_selection.kind == GraphSelection::ReferenceLine) {
        if (d_selection.primaryId == index)
            setCurrentSelection({ GraphSelection::None, -1, -1 });
        else if (d_selection.primaryId > index)
            setCurrentSelection(
                    { GraphSelection::ReferenceLine, d_selection.primaryId - 1, -1 });
    }
    d_plot->replot();
    emit modifiedGraph();
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Delete Reference Line"), beforeSnap);
}

bool Graph::editReferenceLineDialog(int index)
{
    if (index < 0 || index >= d_ref_lines.size())
        return false;

    ReferenceLineSpec spec = d_ref_lines[index];

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit Reference Line"));
    dlg.setModal(true);
    dlg.setFont(QFont(QStringLiteral("Arial"), 10));

    QVBoxLayout *mainLay = new QVBoxLayout(&dlg);
    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);

    QComboBox *orientBox = new QComboBox(&dlg);
    orientBox->addItem(tr("Horizontal (Y = const)"), 0);
    orientBox->addItem(tr("Vertical (X = const)"), 1);
    orientBox->setCurrentIndex(spec.orientation == 1 ? 1 : 0);

    QLineEdit *valEdit = new QLineEdit(QLocale::c().toString(spec.value, 'g', 10), &dlg);
    QLineEdit *labelEdit = new QLineEdit(spec.label, &dlg);
    labelEdit->setPlaceholderText(tr("Optional label (e.g. Y = 0)"));

    QComboBox *styleBox = new QComboBox(&dlg);
    styleBox->addItem(tr("Solid"), int(Qt::SolidLine));
    styleBox->addItem(tr("Dash"), int(Qt::DashLine));
    styleBox->addItem(tr("Dot"), int(Qt::DotLine));
    styleBox->addItem(tr("Dash-Dot"), int(Qt::DashDotLine));
    int styleIdx = styleBox->findData(int(spec.style));
    styleBox->setCurrentIndex(styleIdx >= 0 ? styleIdx : 1);

    QDoubleSpinBox *widthSpin = new QDoubleSpinBox(&dlg);
    widthSpin->setRange(0.5, 12.0);
    widthSpin->setSingleStep(0.5);
    widthSpin->setDecimals(1);
    widthSpin->setValue(qBound(0.5, spec.width, 12.0));

    QColor chosenColor = spec.color.isValid() ? spec.color : QColor(30, 102, 184);
    QPushButton *colorBtn = new QPushButton(tr("Choose Color..."), &dlg);
    auto updateColorBtnStyle = [colorBtn](const QColor &c) {
        colorBtn->setStyleSheet(
                QStringLiteral("QPushButton { background-color: %1; color: %2; padding: 4px 10px; "
                               "border: 1px solid #888; border-radius: 3px; }")
                        .arg(c.name(), c.lightness() < 140 ? QStringLiteral("#ffffff")
                                                           : QStringLiteral("#111111")));
    };
    updateColorBtnStyle(chosenColor);
    connect(colorBtn, &QPushButton::clicked, &dlg, [&dlg, &chosenColor, updateColorBtnStyle]() {
        const QColor c = QColorDialog::getColor(chosenColor, &dlg, tr("Reference Line Color"));
        if (c.isValid()) {
            chosenColor = c;
            updateColorBtnStyle(chosenColor);
        }
    });

    QCheckBox *fillCheck = new QCheckBox(tr("Shade to zero / axis baseline"), &dlg);
    fillCheck->setChecked(spec.fillToAxis);

    grid->addWidget(new QLabel(tr("Orientation:"), &dlg), 0, 0);
    grid->addWidget(orientBox, 0, 1);
    grid->addWidget(new QLabel(tr("Value:"), &dlg), 1, 0);
    grid->addWidget(valEdit, 1, 1);
    grid->addWidget(new QLabel(tr("Label:"), &dlg), 2, 0);
    grid->addWidget(labelEdit, 2, 1);
    grid->addWidget(new QLabel(tr("Line Style:"), &dlg), 3, 0);
    grid->addWidget(styleBox, 3, 1);
    grid->addWidget(new QLabel(tr("Line Width:"), &dlg), 4, 0);
    grid->addWidget(widthSpin, 4, 1);
    grid->addWidget(new QLabel(tr("Color:"), &dlg), 5, 0);
    grid->addWidget(colorBtn, 5, 1);
    grid->addWidget(fillCheck, 6, 0, 1, 2);
    mainLay->addLayout(grid);

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *deleteBtn = new QPushButton(tr("Delete"), &dlg);
    bool deleted = false;
    connect(deleteBtn, &QPushButton::clicked, &dlg, [&dlg, &deleted]() {
        deleted = true;
        dlg.accept();
    });
    btnRow->addWidget(deleteBtn);
    btnRow->addStretch(1);
    QDialogButtonBox *bbox =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    btnRow->addWidget(bbox);
    mainLay->addLayout(btnRow);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    if (deleted) {
        removeReferenceLine(index);
        return true;
    }

    bool valOk = false;
    double newVal = QLocale::c().toDouble(valEdit->text().trimmed(), &valOk);
    if (!valOk)
        newVal = QLocale().toDouble(valEdit->text().trimmed(), &valOk);
    if (!valOk || !std::isfinite(newVal))
        return false;

    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();

    if (qAbs(newVal - spec.value) > 1e-15
        || orientBox->currentData().toInt() != spec.orientation) {
        spec.statMode.clear();
    }
    spec.orientation = orientBox->currentData().toInt();
    spec.value = newVal;
    spec.label = labelEdit->text().trimmed();
    spec.style = static_cast<Qt::PenStyle>(styleBox->currentData().toInt());
    spec.width = widthSpin->value();
    spec.color = chosenColor;
    spec.fillToAxis = fillCheck->isChecked();
    QColor fillCol = chosenColor;
    fillCol.setAlpha(32);
    spec.fillColor = fillCol;

    updateReferenceLine(index, spec);
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Edit Reference Line"), beforeSnap);
    return true;
}

namespace {
bool axisViewportStatesEqual(const Graph::AxisViewportState &a, const Graph::AxisViewportState &b)
{
    if (a.autoscale != b.autoscale)
        return false;
    for (int i = 0; i < 4; ++i) {
        if (a.axisEnabled[i] != b.axisEnabled[i] || a.isLog[i] != b.isLog[i]
            || a.inverted[i] != b.inverted[i]) {
            return false;
        }
        const double span = qMax(1e-15, qAbs(a.upper[i] - a.lower[i]));
        if (qAbs(a.lower[i] - b.lower[i]) > 1e-9 * span
            || qAbs(a.upper[i] - b.upper[i]) > 1e-9 * span) {
            return false;
        }
    }
    return true;
}
} // namespace

Graph::AxisViewportState Graph::captureAxisViewportState() const
{
    AxisViewportState st;
    st.autoscale = m_autoscale;
    if (!d_plot)
        return st;
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        st.axisEnabled[a] = d_plot->axisEnabled(a);
        const QwtScaleDiv *div = d_plot->axisScaleDiv(a);
        const QwtScaleEngine *se = d_plot->axisScaleEngine(a);
        const double b1 = div ? div->lowerBound() : 0.0;
        const double b2 = div ? div->upperBound() : 1.0;
        st.lower[a] = qMin(b1, b2);
        st.upper[a] = qMax(b1, b2);
        st.userStep[a] = (a < d_user_step.size()) ? d_user_step[a] : 0.0;
        st.maxMajor[a] = d_plot->axisMaxMajor(a);
        st.maxMinor[a] = d_plot->axisMaxMinor(a);
        st.isLog[a] = d_axis_breaks[a].enabled
                ? (d_axis_breaks[a].log10Before && d_axis_breaks[a].log10After)
                : Plot::isLog10ScaleEngine(se);
        st.inverted[a] =
                (se && se->testAttribute(QwtScaleEngine::Inverted)) || (div && b1 > b2);
        st.userLocked[a] = isToriAxisUserLocked(d_plot, a);
    }
    return st;
}

void Graph::applyAxisViewportState(const AxisViewportState &st)
{
    if (!d_plot)
        return;
    const MarkerAxisState axesBefore = markerAxisState();
    const bool prevApplying = d_applying_zoom_history;
    d_applying_zoom_history = true;
    m_autoscale = st.autoscale;

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        QwtScaleEngine *se = d_plot->axisScaleEngine(a);
        if (!se || st.upper[a] <= st.lower[a])
            continue;

        const bool hasBreak = d_axis_breaks[a].enabled;
        const bool curLog = hasBreak
                ? (d_axis_breaks[a].log10Before && d_axis_breaks[a].log10After)
                : Plot::isLog10ScaleEngine(se);

        if (!hasBreak && curLog != st.isLog[a]) {
            se = st.isLog[a] ? static_cast<QwtScaleEngine *>(new QwtLog10ScaleEngine())
                             : static_cast<QwtScaleEngine *>(new QwtLinearScaleEngine());
            d_plot->setAxisScaleEngine(a, se);
        }

        double effStep = 0.0;
        if (!st.isLog[a] && st.userStep[a] > 0.0) {
            const double nMaj = (st.upper[a] - st.lower[a]) / st.userStep[a];
            if (nMaj >= 1.5 && nMaj <= 25.0)
                effStep = st.userStep[a];
        }
        int maxMaj = qMax(2, st.maxMajor[a]);
        if (hasBreak && axisFont(a).pointSize() >= 20)
            maxMaj = qMin(maxMaj, 5);
        const int minT = st.maxMinor[a];
        const int maxMinInt = (minT == 1) ? (hasBreak ? 2 : 3) : ((minT > 1) ? (minT + 1) : 0);

        QwtScaleDiv div = se->divideScale(st.lower[a], st.upper[a], maxMaj, maxMinInt, effStep);
        if (st.inverted[a]) {
            se->setAttribute(QwtScaleEngine::Inverted, true);
            if (div.lowerBound() < div.upperBound())
                div.invert();
        } else {
            se->setAttribute(QwtScaleEngine::Inverted, false);
        }
        d_plot->setAxisScaleDiv(a, div);
        setToriAxisUserLocked(d_plot, a, st.userLocked[a]);

        if (hasBreak) {
            const double bf = qMin(d_axis_breaks[a].breakFrom, d_axis_breaks[a].breakTo);
            const double bt = qMax(d_axis_breaks[a].breakFrom, d_axis_breaks[a].breakTo);
            const bool straddles = (bf > st.lower[a] && bt < st.upper[a] && bt > bf + 1e-15);
            if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(a)))
                sd->setAxisBreakIndicator(straddles, bf, bt, d_axis_breaks[a].log10Before,
                                          d_axis_breaks[a].log10After);
        }
    }

    d_plot->syncMirrorAxisIfNeeded(QwtPlot::xBottom);
    d_plot->syncMirrorAxisIfNeeded(QwtPlot::yLeft);
    refreshAxisBreakIndicator(QwtPlot::xTop);
    refreshAxisBreakIndicator(QwtPlot::yRight);
    updateMarkersBoundingRect(axesBefore);
    updateStatReferenceLines();
    if (d_linked_parent)
        syncLinkedAxesFromParent();
    syncChildLinkedLayers();
    d_plot->replot();
    updateSelectionOverlayGeometry();
    d_applying_zoom_history = prevApplying;
    emit modifiedGraph();
}

void Graph::recordZoomHistorySnapshot()
{
    if (d_applying_zoom_history || !d_plot || isPiePlot() || isTernaryPlot())
        return;

    const AxisViewportState cur = captureAxisViewportState();
    if (d_zoom_history_pos < d_zoom_history.size())
        d_zoom_history.resize(d_zoom_history_pos + 1);

    if (d_zoom_history.isEmpty() || !axisViewportStatesEqual(d_zoom_history.last(), cur)) {
        if (d_zoom_history.size() >= 32)
            d_zoom_history.removeFirst();
        d_zoom_history.append(cur);
    }
    d_zoom_history_pos = d_zoom_history.size();
}

bool Graph::stepZoomHistory(int delta)
{
    if (delta == 0 || d_zoom_history.isEmpty() || !d_plot || isPiePlot() || isTernaryPlot())
        return false;

    if (delta < 0) {
        if (d_zoom_history_pos >= d_zoom_history.size()) {
            const AxisViewportState cur = captureAxisViewportState();
            if (!axisViewportStatesEqual(d_zoom_history.last(), cur)) {
                if (d_zoom_history.size() >= 32)
                    d_zoom_history.removeFirst();
                d_zoom_history.append(cur);
            }
            d_zoom_history_pos = d_zoom_history.size() - 1;
        }
        const int target = d_zoom_history_pos + delta;
        if (target < 0 || target >= d_zoom_history.size())
            return false;
        d_zoom_history_pos = target;
        applyAxisViewportState(d_zoom_history[d_zoom_history_pos]);
        return true;
    }

    // Redo (delta > 0)
    if (d_zoom_history_pos >= d_zoom_history.size() - 1)
        return false;
    const int target = qMin(int(d_zoom_history.size()) - 1, d_zoom_history_pos + delta);
    if (target == d_zoom_history_pos)
        return false;
    d_zoom_history_pos = target;
    applyAxisViewportState(d_zoom_history[d_zoom_history_pos]);
    return true;
}

bool Graph::handleEscapeKey()
{
    if (scalePicker && scalePicker->hasActiveInlineScaleEditor()) {
        cancelInlineScaleEditor();
        return true;
    }
    if (cp && cp->cancelActiveInteraction())
        return true;

    if (zoomOn() || activeTool() || rangeSelectorsEnabled() || drawLineActive()
        || drawTextActive()) {
        const bool wasDrawText = drawTextActive();
        disableTools();
        if (wasDrawText) {
            drawText(false);
            emit drawTextOff();
        }
        if (MultiLayer *ml = multiLayerParent())
            emit ml->setPointerCursor();
        if (d_plot && d_plot->canvas())
            d_plot->canvas()->setCursor(Qt::ArrowCursor);
        return true;
    }

    if (d_selection.kind != GraphSelection::None || markerSelected() || titleSelected()) {
        deselect();
        return true;
    }
    return false;
}

bool Graph::nudgeSelectedElement(int dx, int dy)
{
    if ((dx == 0 && dy == 0) || !d_plot)
        return false;

    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    const double zoom = qMax(0.05, d_plot->displayFontScale());
    bool changed = false;

    if (d_selection.kind == GraphSelection::Axis && d_selection.primaryId >= 0
        && d_selection.primaryId < QwtPlot::axisCnt) {
        const int axis = d_selection.primaryId;
        if (QwtScaleWidget *sw = d_plot->axisWidget(axis)) {
            int deltaNormal = 0;
            int deltaParallel = 0;
            if (axis == QwtPlot::yLeft) {
                deltaNormal = -dx;
                deltaParallel = dy;
            } else if (axis == QwtPlot::yRight) {
                deltaNormal = dx;
                deltaParallel = dy;
            } else if (axis == QwtPlot::xBottom) {
                deltaNormal = dy;
                deltaParallel = dx;
            } else if (axis == QwtPlot::xTop) {
                deltaNormal = -dy;
                deltaParallel = dx;
            }
            if (deltaNormal != 0) {
                const double curNorm = sw->property("_toriAxisNormalShift").toDouble();
                sw->setProperty("_toriAxisNormalShift",
                                qBound(-500.0, curNorm + double(deltaNormal) / zoom, 500.0));
                changed = true;
            }
            if (deltaParallel != 0) {
                const double curPar = sw->property("_toriAxisParallelShift").toDouble();
                sw->setProperty("_toriAxisParallelShift",
                                qBound(-500.0, curPar + double(deltaParallel) / zoom, 500.0));
                changed = true;
            }
            if (changed)
                d_plot->updateLayout();
        }
    } else if (d_selection.kind == GraphSelection::AxisTitle && d_selection.primaryId >= 0
               && d_selection.primaryId < QwtPlot::axisCnt) {
        const int axis = d_selection.primaryId;
        if (QwtScaleWidget *sw = d_plot->axisWidget(axis)) {
            const QPointF cur = sw->property("_toriAxisTitleShift").toPointF();
            const QPointF next(qBound(-500.0, cur.x() + double(dx) / zoom, 500.0),
                               qBound(-500.0, cur.y() + double(dy) / zoom, 500.0));
            sw->setProperty("_toriAxisTitleShift", next);
            d_plot->updateLayout();
            changed = true;
        }
    } else if (d_selection.kind == GraphSelection::PlotTitle || titleSelected()) {
        const QPointF cur = d_plot->property("_toriPlotTitleShift").toPointF();
        const QPointF next(qBound(-500.0, cur.x() + double(dx) / zoom, 500.0),
                           qBound(-500.0, cur.y() + double(dy) / zoom, 500.0));
        d_plot->setProperty("_toriPlotTitleShift", next);
        d_plot->updateLayout();
        changed = true;
    } else if (d_selection.kind == GraphSelection::LegendBox
               || d_selection.kind == GraphSelection::TextAnnotation) {
        Legend *mrk = nullptr;
        if (d_selection.primaryId >= 0)
            mrk = dynamic_cast<Legend *>(d_plot->marker(d_selection.primaryId));
        if (!mrk && d_selection.kind == GraphSelection::LegendBox)
            mrk = legend();
        if (!mrk && selectedMarker >= 0)
            mrk = dynamic_cast<Legend *>(d_plot->marker(selectedMarker));
        if (mrk) {
            mrk->setOrigin(mrk->rect().topLeft() + QPoint(dx, dy));
            if (mrk == legend())
                d_plot->setProperty("_toriLegendAnchor", QVariant());
            if (d_markers_selector)
                d_markers_selector->recalcBoundingRect();
            changed = true;
        }
    } else if (d_selection.kind == GraphSelection::ArrowAnnotation) {
        ArrowMarker *am = (d_selection.primaryId >= 0)
                ? arrow(d_selection.primaryId)
                : ((selectedMarker >= 0) ? arrow(selectedMarker) : nullptr);
        if (am) {
            am->setStartPoint(am->startPoint() + QPoint(dx, dy));
            am->setEndPoint(am->endPoint() + QPoint(dx, dy));
            if (d_markers_selector)
                d_markers_selector->recalcBoundingRect();
            changed = true;
        }
    } else if (d_selection.kind == GraphSelection::ImageAnnotation) {
        ImageMarker *im = (d_selection.primaryId >= 0)
                ? imageMarker(d_selection.primaryId)
                : ((selectedMarker >= 0) ? imageMarker(selectedMarker) : nullptr);
        if (im) {
            im->setOrigin(im->rect().topLeft() + QPoint(dx, dy));
            if (d_markers_selector)
                d_markers_selector->recalcBoundingRect();
            changed = true;
        }
    }

    if (changed) {
        d_plot->replot();
        d_plot->update();
        updateSelectionOverlayGeometry();
        if (ml) {
            if (!beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Move Label / Scale"), beforeSnap, 703);
            ml->modifiedPlot();
        }
    }
    return changed;
}

bool Graph::cycleSelectableCurve(int step)
{
    if (step == 0 || n_curves <= 0)
        return false;

    QVector<int> visCurves;
    visCurves.reserve(n_curves);
    for (int i = 0; i < n_curves; ++i) {
        if (i < c_type.size() && c_type[i] == ErrorBars)
            continue;
        QwtPlotItem *it = plotItem(i);
        if (it && it->isVisible())
            visCurves.append(i);
    }
    if (visCurves.isEmpty())
        return false;

    int curPos = -1;
    if (d_selection.kind == GraphSelection::Curve)
        curPos = visCurves.indexOf(d_selection.primaryId);

    const int m = visCurves.size();
    const int nextPos = (curPos < 0)
            ? (step > 0 ? 0 : (m - 1))
            : (((curPos + (step > 0 ? 1 : -1)) % m + m) % m);
    setCurrentSelection({ GraphSelection::Curve, visCurves[nextPos], -1 });
    return true;
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
        Column *dcXCol = nullptr;
        Column *dcYCol = nullptr;
        const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
        if (dcHasMasks)
            dc->resolveColumns(dcXCol, dcYCol);
        for (int j = 0; j < n; ++j) {
            if (dcHasMasks && dc->isPointMaskedFast(j, dcXCol, dcYCol))
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

// ============================================================================
// OriginPro Phase 5: Piecewise Axis Breaks, Tick Transforms, Shaded Bands,
// Non-Destructive Stack Offsets & Lossless Phase 5 Persistence
// ============================================================================

namespace {

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

        const bool logX = xMap.transformation()
                && xMap.transformation()->type() == QwtScaleTransformation::Log10;
        const bool logY = yMap.transformation()
                && yMap.transformation()->type() == QwtScaleTransformation::Log10;
        const double logYFloor = logY ? qMax(1e-300, 0.1 * qMin(qMax(1e-300, yMap.s1()),
                                                                qMax(1e-300, yMap.s2())))
                                      : 0.0;

        auto isValidBandPt = [&](int idx) -> bool {
            if (!std::isfinite(d_x[idx]) || !std::isfinite(d_yLow[idx])
                || !std::isfinite(d_yHigh[idx])) {
                return false;
            }
            if (logX && d_x[idx] <= 0.0)
                return false;
            if (logY && qMax(d_yLow[idx], d_yHigh[idx]) <= 0.0)
                return false;
            return true;
        };

        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(d_color));

        int i = 0;
        while (i < n) {
            while (i < n && !isValidBandPt(i))
                ++i;
            const int segStart = i;
            while (i < n && isValidBandPt(i))
                ++i;
            const int segEnd = i - 1;
            if (segEnd - segStart >= 1) {
                QPolygonF poly;
                poly.reserve(2 * (segEnd - segStart + 1));
                for (int k = segStart; k <= segEnd; ++k) {
                    const double yHiVal = logY ? qMax(d_yHigh[k], logYFloor) : d_yHigh[k];
                    const double px = xMap.xTransform(d_x[k]);
                    const double py = yMap.xTransform(yHiVal);
                    if (std::isfinite(px) && std::isfinite(py))
                        poly.append(QPointF(px, py));
                }
                for (int k = segEnd; k >= segStart; --k) {
                    const double yLoVal = logY ? qMax(d_yLow[k], logYFloor) : d_yLow[k];
                    const double px = xMap.xTransform(d_x[k]);
                    const double py = yMap.xTransform(yLoVal);
                    if (std::isfinite(px) && std::isfinite(py))
                        poly.append(QPointF(px, py));
                }
                if (poly.size() >= 3)
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

} // namespace

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

void Graph::clearShadedBands()
{
    if (d_shaded_bands.isEmpty())
        return;
    for (ShadedBandSpec &spec : d_shaded_bands) {
        if (spec.item) {
            spec.item->detach();
            delete spec.item;
            spec.item = nullptr;
        }
    }
    d_shaded_bands.clear();
    if (d_plot)
        d_plot->replot();
    emit modifiedGraph();
}

void Graph::removeShadedBandByTitle(const QString &title)
{
    if (title.isEmpty() || d_shaded_bands.isEmpty())
        return;
    bool removed = false;
    for (int i = d_shaded_bands.size() - 1; i >= 0; --i) {
        if (d_shaded_bands[i].title == title) {
            if (d_shaded_bands[i].item) {
                d_shaded_bands[i].item->detach();
                delete d_shaded_bands[i].item;
            }
            d_shaded_bands.removeAt(i);
            removed = true;
        }
    }
    if (removed) {
        if (d_plot)
            d_plot->replot();
        emit modifiedGraph();
    }
}

// ============================================================================
// Slice 1: Unified Selection, Curve Z-Order & Reordering, Hollow Symbols,
//          Series Style Schemes, Single-Axis Autoscale, & Smart Legend Anchor
// ============================================================================

void Graph::setCurrentSelection(const GraphSelection &sel, bool notify)
{
    const bool changed = (d_selection != sel);
    d_selection = sel;

    if (sel.kind != GraphSelection::None) {
        if (MultiLayer *ml = multiLayerParent())
            ml->clearLayerSelector();
    }

    if (sel.kind != GraphSelection::Axis && sel.kind != GraphSelection::AxisTitle) {
        if (scalePicker)
            scalePicker->deselect();
    }
    if (sel.kind != GraphSelection::PlotTitle) {
        if (titlePicker)
            titlePicker->setSelected(false);
    }

    if (sel.kind == GraphSelection::Curve || sel.kind == GraphSelection::ErrorBars) {
        deselectMarker();
        const int key = (sel.primaryId >= 0 && sel.primaryId < n_curves) ? c_keys[sel.primaryId]
                                                                         : -1;
        if (d_plot)
            d_plot->setHighlightedCurveKey(key);
    } else if (sel.kind == GraphSelection::ArrowAnnotation) {
        if (d_plot)
            d_plot->setHighlightedCurveKey(-1);
        selectedMarker = sel.primaryId;
        if (d_markers_selector) {
            ArrowMarker *am = arrow(sel.primaryId);
            if (!am || !d_markers_selector->contains(am))
                detachAndDeleteMarkerSelector(d_markers_selector);
        }
    } else if (sel.kind == GraphSelection::LegendBox
               || sel.kind == GraphSelection::TextAnnotation
               || sel.kind == GraphSelection::ImageAnnotation) {
        if (d_plot)
            d_plot->setHighlightedCurveKey(-1);
        if (sel.kind == GraphSelection::LegendBox) {
            Legend *targetLeg = (sel.primaryId >= 0 && d_plot)
                    ? dynamic_cast<Legend *>(d_plot->marker(sel.primaryId))
                    : legend();
            if (!targetLeg)
                targetLeg = legend();
            if (d_markers_selector && (!targetLeg || !d_markers_selector->contains(targetLeg)))
                detachAndDeleteMarkerSelector(d_markers_selector);
            if (sel.primaryId >= 0)
                selectedMarker = sel.primaryId;
            else if (legendMarkerID >= 0)
                selectedMarker = legendMarkerID;
        } else if (sel.kind == GraphSelection::TextAnnotation) {
            Legend *targetTxt = (sel.primaryId >= 0 && d_plot)
                    ? dynamic_cast<Legend *>(d_plot->marker(sel.primaryId))
                    : nullptr;
            if (d_markers_selector && (!targetTxt || !d_markers_selector->contains(targetTxt)))
                detachAndDeleteMarkerSelector(d_markers_selector);
            if (sel.primaryId >= 0)
                selectedMarker = sel.primaryId;
        } else {
            ImageMarker *targetImg = (sel.primaryId >= 0) ? imageMarker(sel.primaryId) : nullptr;
            if (d_markers_selector && (!targetImg || !d_markers_selector->contains(targetImg)))
                detachAndDeleteMarkerSelector(d_markers_selector);
            if (sel.primaryId >= 0)
                selectedMarker = sel.primaryId;
        }
    } else {
        if (d_plot)
            d_plot->setHighlightedCurveKey(-1);
        deselectMarker();
    }

    updateSelectionOverlayGeometry();
    if (changed && d_plot && d_plot->canvas())
        d_plot->canvas()->update();

    if (notify && changed) {
        emit selectionChanged(this, d_selection);
        if (MultiLayer *ml = multiLayerParent())
            emit ml->selectionChanged(ml, this, d_selection);
    }
}

void Graph::updateSelectionOverlayGeometry()
{
    auto *overlay = static_cast<GraphSelectionOverlayWidget *>(d_selection_overlay);
    if (!overlay)
        return;

    if (!d_plot || isExporting() || property("isExporting").toBool()) {
        overlay->hide();
        return;
    }

    auto showOverlayRect = [&](const QRect &graphRect, bool dashed) {
        const QRect clamped = graphRect.intersected(rect());
        if (clamped.width() < 4 || clamped.height() < 4) {
            overlay->hide();
            return;
        }
        overlay->setDashed(dashed);
        overlay->setGeometry(clamped);
        overlay->show();
        overlay->raise();
    };

    switch (d_selection.kind) {
    case GraphSelection::PlotTitle: {
        QwtTextLabel *tl = d_plot->titleLabel();
        if (tl && tl->isVisible() && !d_plot->title().text().trimmed().isEmpty()) {
            const QPoint topLeft = tl->mapTo(this, QPoint(0, 0));
            showOverlayRect(QRect(topLeft, tl->size()).adjusted(-2, -1, 2, 1), false);
            return;
        }
        break;
    }
    case GraphSelection::AxisTitle: {
        const int axis = d_selection.primaryId;
        if (axis >= 0 && axis < QwtPlot::axisCnt && d_plot->axisEnabled(axis) && scalePicker) {
            QwtScaleWidget *sw = d_plot->axisWidget(axis);
            if (sw && sw->isVisible() && !axisTitle(axis).trimmed().isEmpty()) {
                const QRect trLocal = scalePicker->titleRect(sw);
                if (trLocal.isValid() && !trLocal.isEmpty()) {
                    const QPoint topLeft = sw->mapTo(this, trLocal.topLeft());
                    showOverlayRect(QRect(topLeft, trLocal.size()).adjusted(-3, -2, 3, 2), false);
                    return;
                }
            }
        }
        break;
    }
    case GraphSelection::Axis: {
        const int axis = d_selection.primaryId;
        if (axis >= 0 && axis < QwtPlot::axisCnt && d_plot->axisEnabled(axis) && scalePicker) {
            QwtScaleWidget *sw = d_plot->axisWidget(axis);
            if (sw && sw->isVisible()) {
                const QRect srLocal = scalePicker->scaleRect(sw);
                if (srLocal.isValid() && !srLocal.isEmpty()) {
                    const QPoint topLeft = sw->mapTo(this, srLocal.topLeft());
                    showOverlayRect(QRect(topLeft, srLocal.size()).adjusted(-2, -2, 2, 2), false);
                    return;
                }
            }
        }
        break;
    }
    case GraphSelection::LegendBox: {
        Legend *leg = legend();
        QwtPlotCanvas *cv = d_plot->canvas();
        if (leg && leg->isVisible() && cv) {
            const QRect lr = leg->rect();
            const bool outsideCanvas = (lr.right() > cv->width() || lr.left() < 0
                                        || lr.bottom() > cv->height() || lr.top() < 0);
            if (outsideCanvas) {
                if (d_markers_selector && !d_markers_selector->isInKeyNudge())
                    detachAndDeleteMarkerSelector(d_markers_selector);
                const QPoint topLeft = cv->mapTo(this, lr.topLeft());
                showOverlayRect(QRect(topLeft, lr.size()).adjusted(-2, -2, 2, 2), true);
                return;
            }
            if (!d_markers_selector) {
                const QPoint topLeft = cv->mapTo(this, lr.topLeft());
                showOverlayRect(QRect(topLeft, lr.size()).adjusted(-2, -2, 2, 2), true);
                return;
            }
        }
        break;
    }
    default:
        break;
    }

    overlay->hide();
}

namespace {

bool isLiveCurveOnGraph(Graph *g, const QwtPlotCurve *c)
{
    if (!g || !c)
        return false;
    for (int i = 0; i < g->curves(); ++i) {
        if (g->curve(i) == c)
            return true;
    }
    return false;
}

ApplicationWindow *findGraphAppWindow(const QObject *from)
{
    for (const QObject *p = from; p != nullptr; p = p->parent()) {
        if (auto *aw = qobject_cast<ApplicationWindow *>(const_cast<QObject *>(p)))
            return aw;
    }
    if (qApp) {
        for (QWidget *tlw : QApplication::topLevelWidgets()) {
            if (auto *aw = qobject_cast<ApplicationWindow *>(tlw))
                return aw;
        }
    }
    return nullptr;
}

class GraphColumnMaskUndoCommand final : public QUndoCommand
{
public:
    GraphColumnMaskUndoCommand(Graph *graph, MultiLayer *ml, int layerIndex, const QString &label,
                               QVector<Graph::ColumnMaskSnapshot> snapshots)
        : QUndoCommand(label),
          d_graph(graph),
          d_ml(ml),
          d_layer_index(layerIndex),
          d_initial_push_done(false)
    {
        d_snapshots.reserve(snapshots.size());
        for (Graph::ColumnMaskSnapshot &s : snapshots) {
            auto ptr = std::make_shared<Graph::ColumnMaskSnapshot>(std::move(s));
            d_snapshots.append(ptr);
            if (Column *col = ptr->column.data()) {
                std::weak_ptr<Graph::ColumnMaskSnapshot> weakSnap(ptr);
                d_connections.append(QObject::connect(
                        col, &AbstractColumn::rowsAboutToBeInserted,
                        [weakSnap](const AbstractColumn *, int before, int count) {
                            if (auto sp = weakSnap.lock()) {
                                if (count > 0 && before >= 0) {
                                    sp->addedMasks.insertRows(before, count);
                                    sp->removedMasks.insertRows(before, count);
                                }
                            }
                        }));
                d_connections.append(QObject::connect(
                        col, &AbstractColumn::rowsAboutToBeRemoved,
                        [weakSnap](const AbstractColumn *, int first, int count) {
                            if (auto sp = weakSnap.lock()) {
                                if (count > 0 && first >= 0) {
                                    sp->addedMasks.removeRows(first, count);
                                    sp->removedMasks.removeRows(first, count);
                                }
                            }
                        }));
            }
        }
    }

    ~GraphColumnMaskUndoCommand() override
    {
        for (const QMetaObject::Connection &conn : d_connections)
            QObject::disconnect(conn);
        d_connections.clear();
    }

    void undo() override { applyState(true); }

    void redo() override
    {
        if (!d_initial_push_done) {
            d_initial_push_done = true;
            return;
        }
        applyState(false);
    }

private:
    void applyState(bool isUndo)
    {
        if (d_snapshots.isEmpty())
            return;

        Project *proj = nullptr;
        for (const std::shared_ptr<Graph::ColumnMaskSnapshot> &snapPtr : d_snapshots) {
            if (snapPtr && snapPtr->table && snapPtr->table->d_future_table
                && snapPtr->table->d_future_table->project()) {
                proj = snapPtr->table->d_future_table->project();
                break;
            }
        }
        ScopedProjectUndoSuppressor suppressProjUndo(proj);
        RecalcManager::ScopedDeferAutoRecalc deferRecalc(RecalcManager::existing(findGraphAppWindow(
                d_ml ? static_cast<QObject *>(d_ml.data()) : static_cast<QObject *>(d_graph.data()))));

        for (const std::shared_ptr<Graph::ColumnMaskSnapshot> &snapPtr : d_snapshots) {
            if (!snapPtr)
                continue;
            const Graph::ColumnMaskSnapshot &snap = *snapPtr;
            if (!snap.table || !snap.table->d_future_table)
                continue;
            Column *col = snap.column.data();
            if (!col || col->parentAspect() != snap.table->d_future_table.data())
                continue;
            const int rowCount = col->rowCount();
            if (rowCount <= 0)
                continue;
            const int maxRow = rowCount - 1;
            const IntervalAttribute<bool> &toUnmask =
                    isUndo ? snap.addedMasks : snap.removedMasks;
            const IntervalAttribute<bool> &toMask =
                    isUndo ? snap.removedMasks : snap.addedMasks;
            bool colModified = false;
            for (const Interval<int> &iv : toUnmask.intervals()) {
                const Interval<int> clamped(qMax(0, iv.start()), qMin(maxRow, iv.end()));
                if (clamped.isValid()) {
                    col->setMasked(clamped, false);
                    colModified = true;
                }
            }
            for (const Interval<int> &iv : toMask.intervals()) {
                const Interval<int> clamped(qMax(0, iv.start()), qMin(maxRow, iv.end()));
                if (clamped.isValid()) {
                    col->setMasked(clamped, true);
                    colModified = true;
                }
            }
            if (colModified) {
                const QString fullName = snap.table->name() + QLatin1Char('_') + col->name();
                emit snap.table->modifiedData(snap.table.data(), fullName);
            }
        }

        Graph *g = d_graph.data();
        if ((!g || !g->parent()) && d_ml) {
            const QList<Graph *> layers = d_ml->layersList();
            if (d_layer_index >= 0 && d_layer_index < layers.size())
                g = layers.at(d_layer_index);
            else
                g = d_ml->activeGraph();
        }
        if (g) {
            g->updateStatReferenceLines();
            g->syncChildLinkedLayers();
            g->replot();
            emit g->modifiedGraph();
        } else if (d_ml) {
            for (Graph *lg : d_ml->layersList()) {
                if (lg) {
                    lg->updateStatReferenceLines();
                    lg->syncChildLinkedLayers();
                    lg->replot();
                }
            }
            emit d_ml->modifiedPlot();
        }
    }

    QPointer<Graph> d_graph;
    QPointer<MultiLayer> d_ml;
    int d_layer_index = -1;
    QVector<std::shared_ptr<Graph::ColumnMaskSnapshot>> d_snapshots;
    QVector<QMetaObject::Connection> d_connections;
    bool d_initial_push_done = false;
};

} // namespace

bool Graph::runWithColumnMaskUndo(const QString &undoLabel, const QList<DataCurve *> &curves,
                                  const std::function<void()> &editFn)
{
    RecalcManager::ScopedDeferAutoRecalc deferRecalc(
            RecalcManager::existing(findGraphAppWindow(this)));

    struct PendingMaskSnapshot {
        QPointer<Table> table;
        QPointer<Column> column;
        QString colName;
        QList<Interval<int>> beforeIntervals;
    };
    QVector<PendingMaskSnapshot> pending;
    QSet<const Column *> seenCols;
    for (DataCurve *dc : curves) {
        if (!dc || !dc->table() || !dc->table()->d_future_table)
            continue;
        Column *xCol = nullptr;
        Column *yCol = nullptr;
        dc->resolveColumns(xCol, yCol);
        for (Column *col : { xCol, yCol }) {
            if (!col || seenCols.contains(col))
                continue;
            seenCols.insert(col);
            PendingMaskSnapshot snap;
            snap.table = dc->table();
            snap.column = col;
            snap.colName = dc->table()->name() + QLatin1Char('_') + col->name();
            snap.beforeIntervals = col->maskedIntervals();
            pending.append(std::move(snap));
        }
    }

    MultiLayer *ml = multiLayerParent();
    QUndoStack *mlStack = (ml && !ml->isRestoringSnapshot() && !ml->isUndoSuppressed())
            ? ml->undoStack()
            : nullptr;

    if (editFn) {
        if (mlStack) {
            Project *proj = nullptr;
            for (const PendingMaskSnapshot &snap : pending) {
                if (snap.table && snap.table->d_future_table
                    && snap.table->d_future_table->project()) {
                    proj = snap.table->d_future_table->project();
                    break;
                }
            }
            ScopedProjectUndoSuppressor suppressProjUndo(proj);
            editFn();
        } else {
            editFn();
        }
    }

    QVector<ColumnMaskSnapshot> changed;
    changed.reserve(pending.size());
    for (PendingMaskSnapshot &snap : pending) {
        if (!snap.table || !snap.table->d_future_table || !snap.column)
            continue;
        if (snap.column->parentAspect() != snap.table->d_future_table.data())
            continue;
        const QList<Interval<int>> afterIntervals = snap.column->maskedIntervals();
        IntervalAttribute<bool> addedMasks(afterIntervals);
        for (const Interval<int> &iv : snap.beforeIntervals)
            addedMasks.setValue(iv, false);
        IntervalAttribute<bool> removedMasks(snap.beforeIntervals);
        for (const Interval<int> &iv : afterIntervals)
            removedMasks.setValue(iv, false);
        if (addedMasks.intervals().isEmpty() && removedMasks.intervals().isEmpty())
            continue;
        ColumnMaskSnapshot delta;
        delta.table = snap.table;
        delta.column = snap.column;
        delta.colName = std::move(snap.colName);
        delta.addedMasks = std::move(addedMasks);
        delta.removedMasks = std::move(removedMasks);
        changed.append(std::move(delta));
    }

    if (changed.isEmpty())
        return false;

    if (mlStack) {
        const int layerIdx = ml->layersList().indexOf(this);
        mlStack->push(new GraphColumnMaskUndoCommand(
                this, ml, layerIdx, undoLabel, std::move(changed)));
        if (ApplicationWindow *aw = findGraphAppWindow(this))
            aw->updateUndoRedoActionsForActiveWindow();
    }
    return true;
}

int Graph::clearAllCurveMasksWithUndo()
{
    QList<DataCurve *> targetCurves;
    int clearedCurves = 0;
    for (int i = 0; i < n_curves; ++i) {
        if (DataCurve *dc = dynamic_cast<DataCurve *>(curve(i))) {
            Column *xCol = nullptr;
            Column *yCol = nullptr;
            dc->resolveColumns(xCol, yCol);
            const bool hasY = (yCol && !yCol->maskedIntervals().isEmpty());
            bool hasXInCurve = false;
            if (xCol && xCol != yCol && !xCol->maskedIntervals().isEmpty()) {
                const int n = dc->dataSize();
                for (int pt = 0; pt < n; ++pt) {
                    const int row = dc->tableRow(pt);
                    if (row >= 0 && xCol->isMasked(row)) {
                        hasXInCurve = true;
                        break;
                    }
                }
            }
            if (hasY || hasXInCurve) {
                targetCurves.append(dc);
                ++clearedCurves;
            }
        }
    }
    if (targetCurves.isEmpty())
        return 0;

    runWithColumnMaskUndo(tr("Clear All Masks on Graph"), targetCurves, [&]() {
        for (DataCurve *dc : targetCurves) {
            if (isLiveCurveOnGraph(this, dc))
                dc->clearMasks();
        }
    });
    updateStatReferenceLines();
    replot();
    notifyChanges();
    return clearedCurves;
}
