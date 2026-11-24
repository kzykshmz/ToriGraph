/***************************************************************************
    File                 : MultiLayer_ExportAndUndo.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Multi layer widget (export, print, panel labels, phase5 restore, undo snapshots)

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
#include <QVector>
#include <QWidgetList>
#include <QPrinter>
#include <QPrintDialog>
#include <QDateTime>
#include <QApplication>
#include <QMessageBox>
#include <QBitmap>
#include <QImageWriter>
#include <QPainter>
#include <QPicture>
#include <QClipboard>
#include <QMouseEvent>
#include <QAbstractEventDispatcher>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#include <QVBoxLayout>
#include <limits>

#if QT_VERSION >= 0x040300
#include <QSvgGenerator>
#endif

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>
#include <qwt_text_label.h>

#include "MultiLayer.h"
#include "ApplicationWindow.h"
#ifdef Q_OS_WIN
#include "OleServer.h"
#include "EmfPaintDevice.h"
#endif
#include "Plot.h"
#include "IconLoader.h"
#include "Legend.h"
#include "ArrowMarker.h"
#include "ImageMarker.h"
#include "SelectionMoveResizer.h"
#include "LayerDialog.h"
#include "PlotCurve.h"
#include "FunctionCurve.h"
#include "ColorButton.h"
#include "ScaleDraw.h"
#include "ScalePicker.h"
#include "Spectrogram.h"
#include "GraphInspectorDock.h"
#include "GraphMiniToolbar.h"
#include "CanvasInlineTextEditor.h"
#include "ModernRibbonAndHud.h"
#include "ModernUiTheme.h"
#include "I18nJapanese.h"
#include <cmath>
#include "ExtraPlotTypes.h"
#include "OriginGadgets.h"

#include "QwtErrorPlotCurve.h"
#include "QwtBarCurve.h"
#include "QwtHistogram.h"
#include "BoxCurve.h"
#include "QwtPieCurve.h"
#include "RecalcEngine.h"
#include "Table.h"
#include "globals.h"

#include <qwt_symbol.h>
#include <QFile>
#include <QFrame>
#include <QRegularExpression>
#include <QToolButton>
#include <QMenu>
#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QUndoStack>
#include <QTimer>
#include <QSignalBlocker>
#include <QPointer>
#include <QSettings>
#include <QStatusBar>
#include <QFileInfo>
#include <QUndoCommand>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QTreeWidget>
#include <QHeaderView>
#include <QScrollBar>
#include <QScrollArea>
#include <QMdiArea>
#include <QSet>
#include <QSignalBlocker>
#include <QPainterPath>
#include <cmath>

#include <gsl/gsl_vector.h>

#include <iostream>
using namespace std;
#include "MultiLayer_Internal.h"
using namespace MultiLayerInternal;

ScopedUnzoomForExport::ScopedUnzoomForExport(MultiLayer *target) : ml(target)
{
    if (ml && qAbs(ml->pageZoom() - 1.0) > 1e-4) {
        prevZoom = ml->pageZoom();
        prevWinPos = ml->pos();
        prevWinSize = ml->size();
        for (Graph *gr : ml->layersList()) {
            if (!gr || !gr->plotWidget())
                continue;
            prevLayers << QPointer<Graph>(gr);
            prevLayerGeometry << gr->geometry();
            prevPlotSize << gr->plotWidget()->size();
        }
        prevLayerCanvasW = ml->l_canvas_width;
        prevLayerCanvasH = ml->l_canvas_height;
        prevMlUpdates = !ml->testAttribute(Qt::WA_UpdatesDisabled);
        if (QWidget *w = ml->widget()) {
            // Query child's own WA_UpdatesDisabled BEFORE disabling updates on parent ml
            // (ml->setUpdatesEnabled(false) sets WA_ForceUpdatesDisabled on children, which
            // would make w->updatesEnabled() return false and permanently freeze w on restore!).
            prevWidgetUpdates = !w->testAttribute(Qt::WA_UpdatesDisabled);
            w->setUpdatesEnabled(false);
        }
        ml->setUpdatesEnabled(false);
        QSignalBlocker sb(ml);
        const bool prevInternalZoom = ml->d_internal_zoom_change;
        ml->d_internal_zoom_change = true;
        ml->setPageZoom(1.0);
        ml->d_internal_zoom_change = prevInternalZoom;
        active = true;
    }
}

void ScopedUnzoomForExport::restoreNow()
{
    if (!active)
        return;
    active = false;
    if (!ml)
        return;
    {
        QSignalBlocker sb(ml);
        const bool prevInternalZoom = ml->d_internal_zoom_change;
        ml->d_internal_zoom_change = true;
        ml->setPageZoom(prevZoom);
        ml->d_internal_zoom_change = prevInternalZoom;
        if (!ml->isMaximized() && !ml->isMinimized()) {
            if (prevWinSize.isValid() && ml->size() != prevWinSize)
                ml->resize(prevWinSize);
            if (ml->pos() != prevWinPos)
                ml->move(prevWinPos);
        }
        // Put every layer back on its exact rectangle (see prevLayerGeometry).
        bool moved = false;
        for (int i = 0; i < prevLayers.size(); ++i) {
            Graph *gr = prevLayers.at(i);
            if (!gr || !gr->plotWidget())
                continue;
            if (gr->geometry() == prevLayerGeometry.at(i)
                && gr->plotWidget()->size() == prevPlotSize.at(i))
                continue;
            const bool ign = gr->ignoresResizeEvents();
            gr->setIgnoreResizeEvents(true);
            gr->setGeometry(prevLayerGeometry.at(i));
            gr->plotWidget()->resize(prevPlotSize.at(i));
            gr->plotWidget()->updateLayout();
            gr->plotWidget()->replot();
            gr->setIgnoreResizeEvents(ign);
            moved = true;
        }
        ml->l_canvas_width = prevLayerCanvasW;
        ml->l_canvas_height = prevLayerCanvasH;
        if (moved)
            ml->repositionCanvasStatusPill();
    }
    if (QWidget *w = ml->widget()) {
        w->setUpdatesEnabled(prevWidgetUpdates);
        if (prevWidgetUpdates)
            w->update();
    }
    ml->setUpdatesEnabled(prevMlUpdates);
    if (prevMlUpdates)
        ml->update();
}

ScopedUnzoomForExport::~ScopedUnzoomForExport()
{
    restoreNow();
}

namespace {
struct ExportPageMetrics
{
    QRect layersRect;
    QRect baseRect;
    QRect srcRect;
    bool hasPhysicalPage = false;
    bool expandedBeyondBase = false;
    double physWMm = 0.0;
    double physHMm = 0.0;
};

ExportPageMetrics computeExportPageMetrics(MultiLayer *ml)
{
    ExportPageMetrics m;
    const QWidgetList graphs = ml->graphPtrs();
    for (QWidget *w : graphs) {
        if (Graph *gr = qobject_cast<Graph *>(w)) {
            if (gr->plotWidget())
                m.layersRect = m.layersRect.united(QRect(gr->pos(), gr->plotWidget()->size()));
        }
    }
    QWidget *cv = ml->canvasWidget();
    const QRect cvRect = (cv && !cv->rect().isEmpty()) ? cv->rect() : QRect(0, 0, 520, 520);
    if (!m.layersRect.isEmpty()) {
        m.layersRect.adjust(-ml->leftMargin(), -ml->topMargin(), ml->rightMargin(),
                            ml->bottomMargin());
    } else {
        m.layersRect = cvRect;
    }
    if (ml->panelLabelsEnabled()) {
        const QRect lblRect = ml->panelLabelsBoundingRect();
        if (!lblRect.isEmpty())
            m.layersRect = m.layersRect.united(lblRect);
    }

    m.hasPhysicalPage = (ml->pageUnit() != MultiLayer::UnitPx);
    m.baseRect = ((m.hasPhysicalPage || ml->pageViewMode()) && cv && !cv->rect().isEmpty())
            ? cv->rect()
            : m.layersRect;
    m.srcRect = m.baseRect;
    if (ml->panelLabelsEnabled()) {
        const QRect lblRect = ml->panelLabelsBoundingRect();
        if (!lblRect.isEmpty() && !m.srcRect.contains(lblRect)) {
            m.srcRect = m.srcRect.united(lblRect);
            m.expandedBeyondBase = true;
        }
    }
    if (m.hasPhysicalPage) {
        auto toMm = [](double val, MultiLayer::PageUnit u) -> double {
            switch (u) {
            case MultiLayer::UnitMm:
                return val;
            case MultiLayer::UnitCm:
                return val * 10.0;
            case MultiLayer::UnitInch:
                return val * 25.4;
            case MultiLayer::UnitPx:
            default:
                return (val / 96.0) * 25.4;
            }
        };
        const double baseWMm = qMax(5.0, toMm(ml->pageWidthPhysical(), ml->pageUnit()));
        const double baseHMm = qMax(5.0, toMm(ml->pageHeightPhysical(), ml->pageUnit()));
        const double widthFactor = (m.baseRect.width() > 0)
                ? double(m.srcRect.width()) / double(m.baseRect.width())
                : 1.0;
        const double heightFactor = (m.baseRect.height() > 0)
                ? double(m.srcRect.height()) / double(m.baseRect.height())
                : 1.0;
        m.physWMm = baseWMm * widthFactor;
        m.physHMm = baseHMm * heightFactor;
    }
    return m;
}
} // namespace

bool MultiLayer::exportToFileChecked(const QString &fileName, QString *errOut)
{
    if (fileName.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }

    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (ext == QLatin1String("emf")) {
#ifdef Q_OS_WIN
        const OleRenderedGraph emf = SciDAVisEmf::renderMultiLayer(this, nullptr, false);
        if (emf.emfBytes.isEmpty()) {
            if (errOut)
                *errOut = tr("Failed to render EMF graphics.");
            return false;
        }
        QFile f(fileName);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (errOut) {
                *errOut = f.errorString().isEmpty()
                        ? tr("Could not open file for writing: %1").arg(fileName)
                        : f.errorString();
            }
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
            *errOut = tr("File format not handled, operation aborted!");
        return false;
#endif
    }

    if (ext == QLatin1String("eps") || ext == QLatin1String("pdf") || ext == QLatin1String("ps")) {
        return exportVectorChecked(fileName, 0, true, true, QPageSize(QPageSize::Custom),
                                   QPageLayout::Portrait, errOut);
    }
    if (ext == QLatin1String("svg"))
        return exportSVGChecked(fileName, errOut);

    const QList<QByteArray> list = QImageWriter::supportedImageFormats();
    for (const QByteArray &fmt : list) {
        if (ext == QString::fromLatin1(fmt).toLower())
            return exportImageChecked(fileName, -1, false, 0, errOut);
    }
    if (errOut)
        *errOut = tr("File format not handled, operation aborted!");
    return false;
}

void MultiLayer::exportToFile(const QString &fileName)
{
    QString err;
    if (!exportToFileChecked(fileName, &err)) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Error"),
                                  err.isEmpty() ? tr("File format not handled, operation aborted!")
                                                : err);
        }
    }
}

QImage MultiLayer::renderPageToImage(int effectiveDpi, bool transparent, QString *errOut)
{
    ScopedUnzoomForExport unzoomGuard(this);
    const ExportPageMetrics m = computeExportPageMetrics(this);

    QSize targetSize;
    if (m.hasPhysicalPage) {
        targetSize = QSize(qMax(1, qRound((m.physWMm / 25.4) * double(effectiveDpi))),
                           qMax(1, qRound((m.physHMm / 25.4) * double(effectiveDpi))));
    } else {
        const double scale = double(effectiveDpi) / 96.0;
        targetSize = QSize(qMax(1, qRound(double(m.srcRect.width()) * scale)),
                           qMax(1, qRound(double(m.srcRect.height()) * scale)));
    }

    const qint64 pxCount = qint64(targetSize.width()) * qint64(targetSize.height());
    if (targetSize.width() <= 0 || targetSize.height() <= 0 || pxCount > 200000000LL) {
        if (errOut)
            *errOut = tr("Image size is too large to allocate for export.");
        return QImage();
    }

    const QColor pageBg = d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white);
    const bool useAlphaFormat = transparent || (pageBg.alpha() < 255);
    // Paint onto Format_ARGB32 even for opaque exports so Qt5's Windows raster engine uses
    // grayscale font AA instead of ClearType LCD RGB subpixel fringes on publication images.
    QImage image(targetSize, QImage::Format_ARGB32);
    if (image.isNull()) {
        if (errOut)
            *errOut = tr("Image size is too large to allocate for export.");
        return QImage();
    }
    image.fill(useAlphaFormat ? Qt::transparent : pageBg);

    QVector<QColor> savedBg(graphsList.count()), savedCanvasBg(graphsList.count());
    if (transparent) {
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
            if (!gr || !gr->plotWidget())
                continue;
            savedBg[i] = gr->plotWidget()->palette().color(QPalette::Window);
            savedCanvasBg[i] = gr->plotWidget()->canvasBackground();
            QColor bg = savedBg[i];
            bg.setAlpha(0);
            QPalette pal = gr->plotWidget()->palette();
            pal.setColor(QPalette::Window, bg);
            gr->plotWidget()->setPalette(pal);
            QColor cbg = savedCanvasBg[i];
            cbg.setAlpha(0);
            gr->plotWidget()->setCanvasBackground(cbg);
        }
    }

    bool paintOk = false;
    {
        QPainter p;
        if (p.begin(&image)) {
            QFont baseFnt(QStringLiteral("Arial"), 10);
            baseFnt.setHintingPreference(QFont::PreferNoHinting);
            p.setFont(baseFnt);
            p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                             | QPainter::SmoothPixmapTransform);
            const double sx = double(targetSize.width()) / double(qMax(1, m.srcRect.width()));
            const double sy = double(targetSize.height()) / double(qMax(1, m.srcRect.height()));
            if (m.hasPhysicalPage) {
                const double s = qMin(sx, sy);
                const double dx =
                        0.5 * (double(targetSize.width()) - double(m.srcRect.width()) * s);
                const double dy =
                        0.5 * (double(targetSize.height()) - double(m.srcRect.height()) * s);
                p.translate(dx, dy);
                p.scale(s, s);
            } else {
                p.scale(sx, sy);
            }
            if (!transparent && pageBg.alpha() > 0)
                p.fillRect(QRect(QPoint(0, 0), m.srcRect.size()), QBrush(pageBg));
            const QList<Graph *> ordered = orderedLayersForRender();
            for (Graph *gr : ordered) {
                if (!gr || !gr->plotWidget())
                    continue;
                Plot *myPlot = gr->plotWidget();
                const QPoint pos = gr->pos() - m.srcRect.topLeft();
                gr->exportPainter(p, false, QRect(pos, myPlot->size()));
            }
            p.setClipping(false);
            drawInsetConnectors(&p, -m.srcRect.topLeft());
            drawPanelLabels(&p, -m.srcRect.topLeft());
            paintOk = p.end();
        }
    }

    if (transparent) {
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
            if (!gr || !gr->plotWidget())
                continue;
            QPalette pal = gr->plotWidget()->palette();
            pal.setColor(QPalette::Window, savedBg[i]);
            gr->plotWidget()->setPalette(pal);
            gr->plotWidget()->setCanvasBackground(savedCanvasBg[i]);
        }
    }

    if (!paintOk) {
        if (errOut)
            *errOut = tr("Failed to initialize image painter.");
        return QImage();
    }

    if (!useAlphaFormat) {
        QImage rgb = image.convertToFormat(QImage::Format_RGB32);
        if (!rgb.isNull())
            image = std::move(rgb);
    }

    const int dpm = qRound(double(effectiveDpi) / 0.0254);
    image.setDotsPerMeterX(dpm);
    image.setDotsPerMeterY(dpm);
    return image;
}

QImage MultiLayer::copyPageAsImage(int dpi)
{
    const int effectiveDpi = (dpi > 0)
            ? qBound(36, dpi, 4800)
            : ((d_page_unit != UnitPx && d_page_dpi > 0) ? qBound(36, d_page_dpi, 4800) : 300);

    QString err;
    QImage image = renderPageToImage(effectiveDpi, false, &err);
    if (image.isNull()) {
        if (!ApplicationWindow::isNonInteractiveContext(this) && !err.isEmpty())
            QMessageBox::warning(this, tr("Warning"), err);
        return image;
    }

    if (QClipboard *cb = QApplication::clipboard()) {
        bool clipboardOk = false;
        for (int attempt = 0; attempt < 10; ++attempt) {
            cb->setImage(image);
            if (cb->image().size() == image.size()) {
                clipboardOk = true;
                break;
            }
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 15);
            QThread::msleep(20);
        }
        if (!clipboardOk && !ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("Could not copy image to clipboard."));
        }
    }

    return image;
}

bool MultiLayer::exportImageChecked(const QString &fileName, int quality, bool transparent, int dpi,
                                    QString *errOut)
{
    if (fileName.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }

    if (QFileInfo(fileName).suffix().toLower() == QLatin1String("emf"))
        return exportToFileChecked(fileName, errOut);

    const bool hasPhysicalPage = (d_page_unit != UnitPx);
    const int effectiveDpi = (dpi > 0)
            ? qBound(36, dpi, 4800)
            : (hasPhysicalPage ? qBound(36, d_page_dpi, 4800) : 96);

    QImage image = renderPageToImage(effectiveDpi, transparent, errOut);
    if (image.isNull())
        return false;

    QImageWriter writer(fileName);
    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (ext == QLatin1String("tif") || ext == QLatin1String("tiff"))
        writer.setCompression(1); // LZW
    const bool isPng = (ext == QLatin1String("png"));
    if (isPng && quality >= 95
        && qint64(image.width()) * qint64(image.height()) >= 1500000LL) {
        writer.setQuality(75); // Fast lossless zlib level 2 (~3.5x faster than level 6 on >= 1.5 MP)
    } else if (quality >= 0 && !(isPng && quality >= 95)) {
        writer.setQuality(quality);
    }
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

void MultiLayer::exportImage(const QString &fileName, int quality, bool transparent, int dpi)
{
    (void)exportImageChecked(fileName, quality, transparent, dpi, nullptr);
}

void MultiLayer::exportPDF(const QString &fname)
{
    exportVector(fname);
}

bool MultiLayer::exportVectorImpl(const QString &fileName, int res, bool color, bool keepAspect,
                                  QPageSize pageSize, QPageLayout::Orientation orientation,
                                  bool allowQt5PsFallback, QString *errOut)
{
    if (fileName.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }

    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (ext == QLatin1String("emf"))
        return exportToFileChecked(fileName, errOut);

    ScopedUnzoomForExport unzoomGuard(this);

    QPrinter printer;
    printer.setDocName(this->name());
    printer.setCreator("ToriGraph");
    printer.setFullPage(true);

    QString outPath = fileName;
    printer.setOutputFileName(outPath);
    if (ext == QLatin1String("eps") || ext == QLatin1String("ps")) {
#if QT_VERSION >= 0x050000
        if (!allowQt5PsFallback) {
            if (errOut) {
                *errOut = tr("EPS/PostScript export is not supported in Qt5 builds; "
                             "export as PDF or SVG instead.");
            }
            return false;
        }
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

    // E6 (investigated): the PDF device must stay at 96 dpi. exportPainter() scales the
    // on-screen layout to the device size *and* Qwt converts point sizes with the device DPI,
    // so a 600-dpi PDF device draws text and lines about 4x too heavy (measured: inked area
    // 0.019 at 96 dpi vs 0.078 at 600 dpi). Text and lines are vectors in the PDF anyway; the
    // page size is set in mm below.
    if (printer.outputFormat() == QPrinter::PdfFormat)
        printer.setResolution(96);
    else if (res > 0)
        printer.setResolution(res);
    else if (d_page_unit != UnitPx && d_page_dpi > 0)
        printer.setResolution(d_page_dpi);

    if (color)
        printer.setColorMode(QPrinter::Color);
    else
        printer.setColorMode(QPrinter::GrayScale);

    QRect srcRect;
    if (pageSize == QPageSize(QPageSize::Custom)) {
        const ExportPageMetrics m = computeExportPageMetrics(this);
        if (m.hasPhysicalPage) {
            printer.setPageSize(QPageSize(QSizeF(m.physWMm, m.physHMm), QPageSize::Millimeter));
            srcRect = m.srcRect;
        } else {
            const QSizeF ptSize(m.srcRect.width() * 72.0 / 96.0,
                                m.srcRect.height() * 72.0 / 96.0);
            printer.setPageSize(QPageSize(ptSize, QPageSize::Point));
            srcRect = m.srcRect;
        }
    } else {
        printer.setPageOrientation(orientation);
        printer.setPageSize(pageSize);
    }

    QPainter p;
    if (!p.begin(&printer)) {
        if (errOut)
            *errOut = tr("Could not open vector output file for writing: %1").arg(outPath);
        return false;
    }
    exportPainter(p, keepAspect, srcRect, QSize(printer.width(), printer.height()));
    const bool endOk = p.end();
    const QFileInfo fi(outPath);
    if (!endOk || !fi.exists() || fi.size() <= 0) {
        if (errOut)
            *errOut = tr("Failed to write vector output file: %1").arg(outPath);
        return false;
    }
    return true;
}

bool MultiLayer::exportVectorChecked(const QString &fileName, int res, bool color, bool keepAspect,
                                     QPageSize pageSize, QPageLayout::Orientation orientation,
                                     QString *errOut)
{
    return exportVectorImpl(fileName, res, color, keepAspect, pageSize, orientation, false, errOut);
}

void MultiLayer::exportVector(const QString &fileName, int res, bool color, bool keepAspect,
                              QPageSize pageSize, QPageLayout::Orientation orientation)
{
    QString err;
    if (!exportVectorImpl(fileName, res, color, keepAspect, pageSize, orientation, true, &err)) {
        if (!ApplicationWindow::isNonInteractiveContext(this) && fileName.isEmpty() && !err.isEmpty())
            QMessageBox::critical(this, tr("Error"), err);
    }
}

bool MultiLayer::exportSVGChecked(const QString &fname, QString *errOut)
{
    if (fname.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }
#if QT_VERSION >= 0x040300
    ScopedUnzoomForExport unzoomGuard(this);
    const ExportPageMetrics m = computeExportPageMetrics(this);
    QSize svgPxSize = m.srcRect.size();
    if (m.hasPhysicalPage && d_base_page_px.isValid() && !m.expandedBeyondBase)
        svgPxSize = d_base_page_px;

    QFile outFile(fname);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errOut) {
            *errOut = outFile.errorString().isEmpty()
                    ? tr("Could not open SVG file for writing: %1").arg(fname)
                    : outFile.errorString();
        }
        return false;
    }

    bool endOk = false;
    {
        QSvgGenerator generator;
        generator.setOutputDevice(&outFile);
#if QT_VERSION >= 0x040500
        generator.setSize(svgPxSize);
        generator.setViewBox(QRect(QPoint(0, 0), svgPxSize));
        generator.setResolution(96);
        generator.setTitle(this->name());
#endif
        QPainter p;
        if (!p.begin(&generator)) {
            outFile.close();
            if (errOut)
                *errOut = tr("Failed to initialize SVG painter.");
            return false;
        }
        exportPainter(p, false, m.srcRect, svgPxSize);
        endOk = p.end();
    }
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

    // E6: QSvgGenerator writes integer px / 96 dpi, so rewrite the root <svg> physical size in mm.
    if (m.hasPhysicalPage && m.physWMm > 1.0 && m.physHMm > 1.0) {
        QFile f(fname);
        if (!f.open(QIODevice::ReadOnly)) {
            if (errOut)
                *errOut = tr("Failed to update SVG dimensions in %1").arg(fname);
            return false;
        }
        QByteArray svg = f.readAll();
        f.close();
        const int rootPos = svg.indexOf("<svg");
        const int rootEnd = (rootPos >= 0) ? svg.indexOf('>', rootPos) : -1;
        if (rootPos >= 0 && rootEnd > rootPos) {
            QString root = QString::fromUtf8(svg.mid(rootPos, rootEnd - rootPos));
            static const QRegularExpression kWRx(QStringLiteral("\\swidth=\"[^\"]*\""));
            static const QRegularExpression kHRx(QStringLiteral("\\sheight=\"[^\"]*\""));
            root.replace(kWRx, QStringLiteral(" width=\"%1mm\"").arg(m.physWMm, 0, 'f', 3));
            root.replace(kHRx, QStringLiteral(" height=\"%1mm\"").arg(m.physHMm, 0, 'f', 3));
            svg.replace(rootPos, rootEnd - rootPos, root.toUtf8());
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                if (errOut)
                    *errOut = tr("Failed to update SVG dimensions in %1").arg(fname);
                return false;
            }
            const qint64 rw = f.write(svg);
            const bool rFlush = f.flush();
            const QFile::FileError rErr = f.error();
            f.close();
            if (rw != svg.size() || !rFlush || rErr != QFile::NoError) {
                if (errOut)
                    *errOut = tr("Failed to update SVG dimensions in %1").arg(fname);
                return false;
            }
        }
    }
    return true;
#else
    if (errOut)
        *errOut = tr("SVG export is not supported in this build.");
    return false;
#endif
}

void MultiLayer::exportSVG(const QString &fname)
{
    (void)exportSVGChecked(fname, nullptr);
}

void MultiLayer::exportPainter(QPaintDevice &paintDevice, bool keepAspect, QRect rect)
{
    QPainter p(&paintDevice);
    exportPainter(p, keepAspect, rect, QSize(paintDevice.width(), paintDevice.height()));
    p.end();
}

void MultiLayer::exportPainter(QPainter &painter, bool keepAspect, QRect rect, QSize size)
{
    QFont baseFnt(QStringLiteral("Arial"), 10);
    baseFnt.setHintingPreference(QFont::PreferNoHinting);
    painter.setFont(baseFnt);

    if (rect == QRect())
        rect = computeExportPageMetrics(this).layersRect;
    if (size == QSize())
        size = rect.size();
    if (keepAspect) {
        QSize scaled = rect.size();
        scaled.scale(size, Qt::KeepAspectRatio);
        size = scaled;
    }
    painter.scale((double)size.width() / (double)qMax(1, rect.width()),
                  (double)size.height() / (double)qMax(1, rect.height()));

    const QColor pageBg = d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white);
    if (pageBg.alpha() > 0)
        painter.fillRect(QRect(QPoint(0, 0), rect.size()), QBrush(pageBg));

    const QList<Graph *> ordered = orderedLayersForRender();
    for (Graph *gr : ordered) {
        if (!gr || !gr->plotWidget())
            continue;
        Plot *myPlot = (Plot *)gr->plotWidget();

        QPoint pos = gr->pos() - rect.topLeft();
        gr->exportPainter(painter, false, QRect(pos, myPlot->size()));
    }
    painter.setClipping(false);
    drawInsetConnectors(&painter, -rect.topLeft());
    drawPanelLabels(&painter, -rect.topLeft());
}

void MultiLayer::copyAllLayers()
{
#ifdef Q_OS_WIN
    // The editable object, or (if OLE is unavailable) an EMF + high-resolution PNG picture.
    const SciDAVisOle::OleCopyResult res = SciDAVisOle::copyGraphForPowerPoint(
            qobject_cast<ApplicationWindow *>(window()), this, nullptr);
    SciDAVisOle::reportCopyResult(qobject_cast<ApplicationWindow *>(window()), res, false);
    if (res.ok || res.staticFallbackOnClipboard)
        return;
#endif
    ScopedUnzoomForExport unzoomGuard(this);
    const QRect layersRect = computeExportPageMetrics(this).layersRect;
    const QColor pageBg = d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white);
    QImage image(layersRect.size(), QImage::Format_ARGB32);
    image.fill(pageBg.alpha() < 255 ? Qt::transparent : pageBg);
    exportPainter(image, false, layersRect);
    QApplication::clipboard()->setImage(image);
}

void MultiLayer::printActiveLayer()
{
    if (active_graph) {
        // Graph::print() puts the page at 100 % once its print dialog is accepted.
        active_graph->setScaleOnPrint(d_scale_on_print);
        active_graph->printCropmarks(d_print_cropmarks);
        active_graph->print();
    }
}

void MultiLayer::print()
{
    QPrinter printer;
    printer.setColorMode(QPrinter::Color);
    printer.setFullPage(true);
    QRect canvasRect = canvas->rect();
    double aspect = double(canvasRect.width()) / double(canvasRect.height());
    if (aspect < 1)
        printer.setPageOrientation(QPageLayout::Portrait);
    else
        printer.setPageOrientation(QPageLayout::Landscape);

    QPrintDialog printDialog(&printer);
    if (printDialog.exec() == QDialog::Accepted) {
        ScopedUnzoomForExport unzoomGuard(this);
        QPainter paint(&printer);
        printAllLayers(&paint);
        paint.end();
    }
}

void MultiLayer::printAllLayers(QPainter *painter)
{
    if (!painter)
        return;
    ScopedUnzoomForExport unzoomGuard(this); // also reached from "Print All Plots"

    QPrinter *printer = (QPrinter *)painter->device();
    QRect canvasRect = canvas->rect();
    QRect pageRect = printer->pageLayout().fullRectPixels(printer->resolution());
    QRect cr = canvasRect; // cropmarks rectangle

    const QList<Graph *> ordered = orderedLayersForRender();
    if (d_scale_on_print) {
        int margin = (int)((1 / 2.54) * printer->logicalDpiY()); // 1 cm margins
        double scaleFactorX = (double)(pageRect.width() - 2 * margin) / (double)qMax(1, canvasRect.width());
        double scaleFactorY =
                (double)(pageRect.height() - 2 * margin) / (double)qMax(1, canvasRect.height());

        if (d_print_cropmarks) {
            cr.moveTo(QPoint(margin + int(cr.x() * scaleFactorX),
                             margin + int(cr.y() * scaleFactorY)));
            cr.setWidth(int(cr.width() * scaleFactorX));
            cr.setHeight(int(cr.height() * scaleFactorX));
        }

        for (Graph *gr : ordered) {
            if (!gr || !gr->plotWidget())
                continue;
            Plot *myPlot = gr->plotWidget();

            QPoint pos = gr->pos();
            pos = QPoint(margin + int(pos.x() * scaleFactorX),
                         margin + int(pos.y() * scaleFactorY));

            int width = int(myPlot->frameGeometry().width() * scaleFactorX);
            int height = int(myPlot->frameGeometry().height() * scaleFactorY);

            gr->print(painter, QRect(pos, QSize(width, height)));
        }

        painter->save();
        painter->setClipping(false);
        painter->translate(margin, margin);
        painter->scale(scaleFactorX, scaleFactorY);
        drawInsetConnectors(painter, QPoint(0, 0));
        drawPanelLabels(painter, QPoint(0, 0));
        painter->restore();
    } else {
        int x_margin = (pageRect.width() - canvasRect.width()) / 2;
        int y_margin = (pageRect.height() - canvasRect.height()) / 2;

        if (d_print_cropmarks)
            cr.moveTo(x_margin, y_margin);

        for (Graph *gr : ordered) {
            if (!gr || !gr->plotWidget())
                continue;
            Plot *myPlot = (Plot *)gr->plotWidget();

            QPoint pos = gr->pos();
            pos = QPoint(x_margin + pos.x(), y_margin + pos.y());
            gr->print(painter, QRect(pos, myPlot->size()));
        }

        painter->save();
        painter->setClipping(false);
        drawInsetConnectors(painter, QPoint(x_margin, y_margin));
        drawPanelLabels(painter, QPoint(x_margin, y_margin));
        painter->restore();
    }
    if (d_print_cropmarks) {
        cr.adjust(-1, -1, 2, 2);
        painter->save();
        painter->setPen(QPen(QColor(Qt::black), 0.5, Qt::DashLine));
        painter->drawLine(pageRect.left(), cr.top(), pageRect.right(), cr.top());
        painter->drawLine(pageRect.left(), cr.bottom(), pageRect.right(), cr.bottom());
        painter->drawLine(cr.left(), pageRect.top(), cr.left(), pageRect.bottom());
        painter->drawLine(cr.right(), pageRect.top(), cr.right(), pageRect.bottom());
        painter->restore();
    }
}

bool MultiLayer::restorePhase5Tag(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith("<Tori"))
        return false;

    auto parseAttrs = [](const QString &tagLine) -> QMap<QString, QString> {
        QMap<QString, QString> attrs;
        static const QRegularExpression rx("([A-Za-z0-9_]+)=\"([^\"]*)\"");
        auto it = rx.globalMatch(tagLine);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            QString val = m.captured(2);
            val.replace("&#10;", "\n");
            val.replace("&#13;", "\r");
            val.replace("&quot;", "\"");
            val.replace("&lt;", "<");
            val.replace("&gt;", ">");
            val.replace("&amp;", "&");
            attrs.insert(m.captured(1), val);
        }
        return attrs;
    };

    if (trimmed.startsWith("<ToriWinSize"))
        return true;

    if (trimmed.startsWith("<ToriPageSetup")) {
        const QMap<QString, QString> a = parseAttrs(trimmed);
        d_page_view_mode = (a.value("view", "0").toInt() != 0);
        d_page_zoom = qBound(0.25, a.value("zoom", "1").toDouble(), 4.0);
        // Layers that already exist (undo restored in place) draw their text at the restored
        // zoom; layers created after this tag pick it up in installLayerEventFilters().
        for (int i = 0; i < graphsList.count(); ++i) {
            if (Graph *gr = qobject_cast<Graph *>(graphsList.at(i)))
                if (gr->plotWidget())
                    gr->plotWidget()->setDisplayFontScale(d_page_zoom);
        }
        d_page_unit = static_cast<PageUnit>(qBound(0, a.value("unit", "0").toInt(), 3));
        d_page_dpi = qBound(36, a.value("dpi", "300").toInt(), 2400);
        d_page_width_val = qMax(1.0, a.value("w", "520").toDouble());
        d_page_height_val = qMax(1.0, a.value("h", "520").toDouble());
        const QColor bg(a.value("bg", "#ffffffff"));
        setPageBackgroundColor(bg.isValid() ? bg : QColor(Qt::white));

        const double screenPpi = 96.0;
        double pxW = d_page_width_val;
        double pxH = d_page_height_val;
        switch (d_page_unit) {
        case UnitPx:
            break;
        case UnitMm:
            pxW = (d_page_width_val / 25.4) * screenPpi;
            pxH = (d_page_height_val / 25.4) * screenPpi;
            break;
        case UnitCm:
            pxW = (d_page_width_val / 2.54) * screenPpi;
            pxH = (d_page_height_val / 2.54) * screenPpi;
            break;
        case UnitInch:
            pxW = d_page_width_val * screenPpi;
            pxH = d_page_height_val * screenPpi;
            break;
        }
        d_base_page_px = QSize(qMax(120, qRound(pxW)), qMax(120, qRound(pxH)));
        graph_width = d_base_page_px.width();
        graph_height = d_base_page_px.height();
        syncMultiLayerPageHost(this, widget(), canvas, d_page_unit, d_page_view_mode,
                               d_base_page_px, d_page_zoom, d_page_bg_color);
        if (d_page_unit != UnitPx || d_page_view_mode || qAbs(d_page_zoom - 1.0) > 1e-4) {
            const QSize targetCanvasSize(qMax(80, qRound(d_base_page_px.width() * d_page_zoom)),
                                         qMax(80, qRound(d_base_page_px.height() * d_page_zoom)));
            lastSize = targetCanvasSize;
        }
        if (canvas)
            canvas->update();
        repositionCanvasStatusPill();
        updateCanvasStatusPill();
        return true;
    }

    if (trimmed.startsWith("<ToriAspectRatio")) {
        const QMap<QString, QString> a = parseAttrs(trimmed);
        d_keep_aspect_ratio = (a.value("keep", "0").toInt() != 0);
        const double r = a.value("ratio", "1").toDouble();
        if (r > 0.05 && r <= 20.0)
            d_target_aspect_ratio = r;
        return true;
    }

    if (trimmed.startsWith("<ToriInset\t")) {
        const QStringList parts = trimmed.split(QLatin1Char('\t'));
        if (parts.size() >= 7) {
            auto cleanToken = [](QString tok) -> QString {
                tok = tok.trimmed();
                if (tok.endsWith(QLatin1String("/>")))
                    tok.chop(2);
                else if (tok.endsWith(QLatin1Char('>')))
                    tok.chop(1);
                return tok.trimmed();
            };
            PendingInsetSpec ps;
            ps.childIdx = cleanToken(parts.at(1)).toInt();
            ps.sourceIdx = cleanToken(parts.at(2)).toInt();
            ps.relRect = QRectF(cleanToken(parts.at(3)).toDouble(),
                                cleanToken(parts.at(4)).toDouble(),
                                cleanToken(parts.at(5)).toDouble(),
                                cleanToken(parts.at(6)).toDouble());
            if (parts.size() >= 8)
                ps.showConnectors = (cleanToken(parts.at(7)).toInt() != 0);
            if (parts.size() >= 12) {
                ps.zoomDataRect = QRectF(cleanToken(parts.at(8)).toDouble(),
                                         cleanToken(parts.at(9)).toDouble(),
                                         cleanToken(parts.at(10)).toDouble(),
                                         cleanToken(parts.at(11)).toDouble());
            }
            if (ps.childIdx >= 0 && ps.sourceIdx >= 0) {
                d_pending_inset_specs.append(ps);
                if (ps.childIdx < graphsList.count() && ps.sourceIdx < graphsList.count()) {
                    Graph *cGr = (Graph *)graphsList.at(ps.childIdx);
                    Graph *sGr = (Graph *)graphsList.at(ps.sourceIdx);
                    if (cGr && sGr && cGr != sGr) {
                        InsetSpec spec;
                        spec.sourceLayer = QPointer<Graph>(sGr);
                        spec.normGeo = ps.relRect;
                        spec.zoomDataRect = ps.zoomDataRect;
                        spec.showConnectors = ps.showConnectors;
                        d_inset_specs[cGr] = spec;
                    }
                }
            }
        }
        return true;
    }

    if (trimmed.startsWith("<ToriInsetSpec") || trimmed.startsWith("<ToriInset ")) {
        const QMap<QString, QString> a = parseAttrs(trimmed);
        PendingInsetSpec ps;
        ps.childIdx = a.value("child", "-1").toInt();
        ps.sourceIdx = a.contains("source") ? a.value("source", "-1").toInt()
                                            : a.value("src", "-1").toInt();
        ps.relRect = QRectF(a.value("x", "0.54").toDouble(),
                            a.value("y", "0.08").toDouble(),
                            a.value("w", "0.38").toDouble(),
                            a.value("h", "0.38").toDouble());
        ps.showConnectors = (a.value("connectors", "1").toInt() != 0);
        if (a.contains("rx") && a.contains("ry") && a.contains("rw") && a.contains("rh")) {
            ps.zoomDataRect = QRectF(a.value("rx", "0").toDouble(),
                                     a.value("ry", "0").toDouble(),
                                     a.value("rw", "0").toDouble(),
                                     a.value("rh", "0").toDouble());
        }
        if (ps.childIdx >= 0 && ps.sourceIdx >= 0) {
            d_pending_inset_specs.append(ps);
            if (ps.childIdx < graphsList.count() && ps.sourceIdx < graphsList.count()) {
                Graph *cGr = (Graph *)graphsList.at(ps.childIdx);
                Graph *sGr = (Graph *)graphsList.at(ps.sourceIdx);
                if (cGr && sGr && cGr != sGr) {
                    InsetSpec spec;
                    spec.sourceLayer = QPointer<Graph>(sGr);
                    spec.normGeo = ps.relRect;
                    spec.zoomDataRect = ps.zoomDataRect;
                    spec.showConnectors = ps.showConnectors;
                    d_inset_specs[cGr] = spec;
                }
            }
        }
        return true;
    }

    if (trimmed.startsWith("<ToriActiveLayer")) {
        const QMap<QString, QString> a = parseAttrs(trimmed);
        bool okIdx = false;
        const int idx = a.value("index", "-1").toInt(&okIdx);
        if (okIdx && idx >= 0)
            setProperty("_tori_pending_active_layer", idx);
        return true;
    }

    if (trimmed.startsWith("<ToriPanelLabels")) {
        const QMap<QString, QString> a = parseAttrs(trimmed);
        d_panel_labels_enabled = (a.value("enabled", "0").toInt() != 0);
        d_panel_label_placement = static_cast<PanelLabelPlacement>(
                qBound(0, a.value("placement", "0").toInt(), 1));
        const QString pat = a.value("pattern", "(%a)");
        d_panel_label_pattern = pat.isEmpty() ? QStringLiteral("(%a)") : pat;
        d_panel_label_font_pt = qMax(0, a.value("fontPt", "0").toInt());
        syncCanvasOverlayVisibility();
        return true;
    }

    if (trimmed.startsWith("<ToriAppliedStylePreset")) {
        const QMap<QString, QString> a = parseAttrs(trimmed);
        d_applied_style_preset_id = a.value(QStringLiteral("id")).trimmed();
        setProperty("toriAppliedStylePreset",
                    d_applied_style_preset_id.isEmpty() ? QVariant()
                                                        : QVariant(d_applied_style_preset_id));
        return true;
    }

    if (trimmed.startsWith("<ToriRecalcLinks")) {
        const QMap<QString, QString> a = parseAttrs(trimmed);
        bool okLayer = false;
        const int layerIdx = a.value(QStringLiteral("layer"), QStringLiteral("-1")).toInt(&okLayer);
        const QString linksXml = a.value(QStringLiteral("xml"));
        if (okLayer && layerIdx >= 0 && !linksXml.trimmed().isEmpty()) {
            PendingRecalcLinkSpec spec;
            spec.layerIndex = layerIdx;
            spec.xml = linksXml;
            d_pending_recalc_links.append(spec);
        }
        return true;
    }

    return false;
}

void MultiLayer::finalizePhase5Restore()
{
    for (const PendingInsetSpec &ps : d_pending_inset_specs) {
        if (ps.childIdx >= 0 && ps.childIdx < graphsList.count()
            && ps.sourceIdx >= 0 && ps.sourceIdx < graphsList.count()) {
            Graph *cGr = (Graph *)graphsList.at(ps.childIdx);
            Graph *sGr = (Graph *)graphsList.at(ps.sourceIdx);
            if (cGr && sGr && cGr != sGr) {
                InsetSpec spec;
                spec.sourceLayer = QPointer<Graph>(sGr);
                spec.normGeo = ps.relRect;
                spec.showConnectors = ps.showConnectors;
                spec.zoomDataRect = ps.zoomDataRect;
                if (spec.zoomDataRect.isEmpty() && cGr->plotWidget()) {
                    const QwtScaleDiv *xsd = cGr->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
                    const QwtScaleDiv *ysd = cGr->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
                    if (xsd && ysd) {
                        const double xLo = qMin(xsd->lowerBound(), xsd->upperBound());
                        const double xHi = qMax(xsd->lowerBound(), xsd->upperBound());
                        const double yLo = qMin(ysd->lowerBound(), ysd->upperBound());
                        const double yHi = qMax(ysd->lowerBound(), ysd->upperBound());
                        spec.zoomDataRect = QRectF(QPointF(xLo, yLo), QPointF(xHi, yHi));
                    }
                }
                d_inset_specs[cGr] = spec;
                cGr->setIgnoreResizeEvents(true);
                cGr->setBackgroundColor(QColor(Qt::white));
                {
                    QPalette insetPal = cGr->palette();
                    insetPal.setColor(QPalette::Window, QColor(Qt::white));
                    cGr->setPalette(insetPal);
                    cGr->setAutoFillBackground(true);
                }
                if (cGr->plotWidget()) {
                    QPalette plotPal = cGr->plotWidget()->palette();
                    plotPal.setColor(QPalette::Window, QColor(Qt::white));
                    cGr->plotWidget()->setPalette(plotPal);
                    cGr->plotWidget()->setAutoFillBackground(true);
                }
                installLayerEventFilters(cGr);
            }
        }
    }
    d_pending_inset_specs.clear();

    const QList<Graph *> allLayers = layersList();
    for (Graph *gr : allLayers) {
        if (!gr)
            continue;
        const int pIdx = gr->pendingLinkedParentIndex();
        if (pIdx >= 0 && pIdx < allLayers.size()) {
            Graph *parentLayer = allLayers.at(pIdx);
            if (parentLayer && parentLayer != gr) {
                gr->setIgnoreResizeEvents(true);
                gr->setBackgroundColor(QColor(255, 255, 255, 0));
                gr->setCanvasBackground(QColor(255, 255, 255, 0));
                if (Plot *cPlot = gr->plotWidget()) {
                    cPlot->setAutoFillBackground(false);
                    cPlot->setAttribute(Qt::WA_TranslucentBackground, true);
                    if (cPlot->canvas())
                        cPlot->canvas()->setAutoFillBackground(false);
                }
                gr->setAttribute(Qt::WA_TranslucentBackground, true);
                gr->setLinkedParentLayer(parentLayer, gr->xLinkMode(), gr->xLinkExpr(),
                                         gr->yLinkMode(), gr->yLinkExpr());
                installLayerEventFilters(gr);
            }
            gr->clearPendingLinkedParentIndex();
        }
    }

    syncChildOverlayAndInsetGeometries();
    if (!d_pending_recalc_links.isEmpty()) {
        ApplicationWindow *app = findMultiLayerApp(this);
        if (RecalcManager *rm = RecalcManager::instance(app)) {
            for (const PendingRecalcLinkSpec &ls : d_pending_recalc_links) {
                if (ls.layerIndex >= 0 && ls.layerIndex < graphsList.count()) {
                    if (Graph *gr = qobject_cast<Graph *>(graphsList.at(ls.layerIndex)))
                        rm->restoreLinkXmlForGraph(app, gr, ls.xml);
                }
            }
        }
        d_pending_recalc_links.clear();
    }
    const QVariant pendingActiveVar = property("_tori_pending_active_layer");
    if (pendingActiveVar.isValid()) {
        const int activeIdx = pendingActiveVar.toInt();
        setProperty("_tori_pending_active_layer", QVariant());
        if (activeIdx >= 0 && activeIdx < graphsList.count()) {
            if (Graph *ag = static_cast<Graph *>(graphsList.at(activeIdx)))
                setActiveGraph(ag);
        }
    }
    refreshLayerButtonBadges();
    syncCanvasOverlayVisibility();
    updateCanvasStatusPill();
}

namespace {

class MultiLayerSnapshotUndoCommand : public QUndoCommand
{
public:
    MultiLayerSnapshotUndoCommand(MultiLayer *ml, ApplicationWindow *app, const QString &label,
                                  const QString &beforeSnap, const QString &afterSnap,
                                  int activeLayerIdx, int mergeId)
        : QUndoCommand(label),
          d_ml(ml),
          d_app(app),
          d_before(beforeSnap),
          d_after(afterSnap),
          d_before_layer(activeLayerIdx),
          d_after_layer(activeLayerIdx),
          d_merge_id(mergeId),
          d_timestamp_ms(QDateTime::currentMSecsSinceEpoch()),
          d_initial_push_done(false)
    {
    }

    int id() const override { return d_merge_id; }

    bool mergeWith(const QUndoCommand *other) override
    {
        if (d_merge_id < 0 || !other || other->id() != d_merge_id)
            return false;
        const auto *o = static_cast<const MultiLayerSnapshotUndoCommand *>(other);
        if (o->d_ml != d_ml)
            return false;
        if (qAbs(o->d_timestamp_ms - d_timestamp_ms) > 350)
            return false;
        d_after = o->d_after;
        d_after_layer = o->d_after_layer;
        d_timestamp_ms = o->d_timestamp_ms;
        return true;
    }

    void undo() override
    {
        if (!d_ml)
            return;
        ApplicationWindow *app = d_app ? d_app.data() : findMultiLayerApp(d_ml);
        if (app)
            d_ml->restoreSnapshotInPlace(app, d_before, d_before_layer, false);
    }

    void redo() override
    {
        if (!d_initial_push_done) {
            d_initial_push_done = true;
            return;
        }
        if (!d_ml)
            return;
        ApplicationWindow *app = d_app ? d_app.data() : findMultiLayerApp(d_ml);
        if (app)
            d_ml->restoreSnapshotInPlace(app, d_after, d_after_layer, false);
    }

    template <typename RemapFn>
    void remapSnapshots(RemapFn &&fn)
    {
        if (!d_before.isEmpty())
            d_before = fn(d_before);
        if (!d_after.isEmpty())
            d_after = fn(d_after);
    }

private:
    QPointer<MultiLayer> d_ml;
    QPointer<ApplicationWindow> d_app;
    QString d_before;
    QString d_after;
    int d_before_layer;
    int d_after_layer;
    int d_merge_id;
    qint64 d_timestamp_ms;
    bool d_initial_push_done;
};

QString formatPanelLabel(const QString &pattern, int zeroBasedIndex)
{
    QString out = pattern.trimmed().isEmpty() ? QStringLiteral("(%a)") : pattern;
    const QChar lowerChar = QChar('a' + (zeroBasedIndex % 26));
    const QChar upperChar = QChar('A' + (zeroBasedIndex % 26));
    const QString numStr = QString::number(zeroBasedIndex + 1);
    bool replaced = false;
    if (out.contains(QLatin1String("%a"))) {
        out.replace(QLatin1String("%a"), QString(lowerChar));
        replaced = true;
    }
    if (out.contains(QLatin1String("%A"))) {
        out.replace(QLatin1String("%A"), QString(upperChar));
        replaced = true;
    }
    if (out.contains(QLatin1String("%1"))) {
        out.replace(QLatin1String("%1"), numStr);
        replaced = true;
    }
    if (out.contains(QLatin1String("%d"))) {
        out.replace(QLatin1String("%d"), numStr);
        replaced = true;
    }
    if (!replaced)
        out = QStringLiteral("(%1)").arg(lowerChar);
    return out;
}

} // namespace

void MultiLayer::beginSuppressUndo()
{
    ++d_undo_suppress_depth;
}

void MultiLayer::endSuppressUndo()
{
    if (d_undo_suppress_depth > 0)
        --d_undo_suppress_depth;
}

QString MultiLayer::captureUndoSnapshot()
{
    if (d_is_restoring_snapshot || d_undo_suppress_depth > 0)
        return QString();
    return saveToString(QString());
}

void MultiLayer::pushUndoSnapshot(const QString &label, const QString &beforeSnapshot, int mergeId)
{
    if (d_is_restoring_snapshot || d_undo_suppress_depth > 0 || beforeSnapshot.isEmpty()
        || !d_undo_stack)
        return;

    const QString afterSnapshot = saveToString(QString());
    if (afterSnapshot == beforeSnapshot)
        return;

    const int activeIdx = active_graph ? graphsList.indexOf(active_graph) : 0;
    ApplicationWindow *app = findMultiLayerApp(this);
    d_undo_stack->push(new MultiLayerSnapshotUndoCommand(
            this, app, label, beforeSnapshot, afterSnapshot, qMax(0, activeIdx), mergeId));
    if (app)
        app->updateUndoRedoActionsForActiveWindow();
}

namespace {

enum class SnapshotRenameKind { TableRename, ColRename, MatrixRename };

struct XmlAttrSpan
{
    QString name;
    QString value;
    int valStart = 0;
    int valLen = 0;
};

QString unescapeSnapshotXmlAttr(const QString &raw)
{
    if (!raw.contains(QLatin1Char('&')))
        return raw;
    QString out;
    out.reserve(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        if (raw.at(i) == QLatin1Char('&')) {
            const int semi = raw.indexOf(QLatin1Char(';'), i + 1);
            if (semi > i + 1 && semi - i <= 12) {
                const QString ent = raw.mid(i + 1, semi - i - 1);
                if (ent == QLatin1String("amp")) {
                    out += QLatin1Char('&');
                    i = semi;
                    continue;
                }
                if (ent == QLatin1String("quot")) {
                    out += QLatin1Char('"');
                    i = semi;
                    continue;
                }
                if (ent == QLatin1String("lt")) {
                    out += QLatin1Char('<');
                    i = semi;
                    continue;
                }
                if (ent == QLatin1String("gt")) {
                    out += QLatin1Char('>');
                    i = semi;
                    continue;
                }
                if (ent == QLatin1String("apos")) {
                    out += QLatin1Char('\'');
                    i = semi;
                    continue;
                }
                if (ent.startsWith(QLatin1Char('#'))) {
                    bool ok = false;
                    uint code = 0;
                    if (ent.size() > 2
                        && (ent.at(1) == QLatin1Char('x') || ent.at(1) == QLatin1Char('X'))) {
                        code = ent.mid(2).toUInt(&ok, 16);
                    } else {
                        code = ent.mid(1).toUInt(&ok, 10);
                    }
                    if (ok && code > 0 && code <= 0xFFFFu) {
                        out += QChar(ushort(code));
                        i = semi;
                        continue;
                    }
                }
            }
        }
        out += raw.at(i);
    }
    return out;
}

QString escapeSnapshotXmlAttr(const QString &val)
{
    QString out;
    out.reserve(val.size());
    for (QChar ch : val) {
        const ushort u = ch.unicode();
        if (ch == QLatin1Char('&'))
            out += QLatin1String("&amp;");
        else if (ch == QLatin1Char('"'))
            out += QLatin1String("&quot;");
        else if (ch == QLatin1Char('<'))
            out += QLatin1String("&lt;");
        else if (ch == QLatin1Char('>'))
            out += QLatin1String("&gt;");
        else if (ch == QLatin1Char('\n'))
            out += QLatin1String("&#10;");
        else if (ch == QLatin1Char('\r'))
            out += QLatin1String("&#13;");
        else if (ch == QLatin1Char('\t'))
            out += QLatin1String("&#9;");
        else if (u < 32)
            out += QStringLiteral("&#%1;").arg(int(u));
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
                    QStringLiteral("&#%1;").arg(int(qtok.at(0).unicode())) + qtok.mid(1);
            out.replace(qtok, repl);
        }
    }
    return out;
}

QVector<XmlAttrSpan> parseSnapshotXmlAttrs(const QString &line)
{
    QVector<XmlAttrSpan> spans;
    const int lt = line.indexOf(QLatin1Char('<'));
    if (lt < 0)
        return spans;
    int i = lt + 1;
    while (i < line.size() && !line.at(i).isSpace() && line.at(i) != QLatin1Char('>')
           && line.at(i) != QLatin1Char('/')) {
        ++i;
    }
    while (i < line.size()) {
        while (i < line.size() && line.at(i).isSpace())
            ++i;
        if (i >= line.size() || line.at(i) == QLatin1Char('>') || line.at(i) == QLatin1Char('/'))
            break;
        const int nameStart = i;
        while (i < line.size() && !line.at(i).isSpace() && line.at(i) != QLatin1Char('=')
               && line.at(i) != QLatin1Char('>') && line.at(i) != QLatin1Char('/')) {
            ++i;
        }
        const QString attrName = line.mid(nameStart, i - nameStart);
        while (i < line.size() && line.at(i).isSpace())
            ++i;
        if (i >= line.size() || line.at(i) != QLatin1Char('='))
            continue;
        ++i;
        while (i < line.size() && line.at(i).isSpace())
            ++i;
        if (i >= line.size() || line.at(i) != QLatin1Char('"'))
            continue;
        const int valStart = i + 1;
        const int valEnd = line.indexOf(QLatin1Char('"'), valStart);
        if (valEnd < 0)
            break;
        XmlAttrSpan sp;
        sp.name = attrName;
        sp.valStart = valStart;
        sp.valLen = valEnd - valStart;
        sp.value = unescapeSnapshotXmlAttr(line.mid(valStart, sp.valLen));
        spans.append(sp);
        i = valEnd + 1;
    }
    return spans;
}

QString remapFullColRef(const QString &ref, SnapshotRenameKind kind, const QString &oldName,
                        const QString &newName)
{
    if (kind == SnapshotRenameKind::TableRename) {
        const QString pfx = oldName + QLatin1Char('_');
        if (ref.startsWith(pfx))
            return newName + QLatin1Char('_') + ref.mid(pfx.size());
    } else if (kind == SnapshotRenameKind::ColRename) {
        if (ref == oldName)
            return newName;
    }
    return ref;
}

QString remapSnapshotText(const QString &snapshot, SnapshotRenameKind kind, const QString &oldName,
                          const QString &newName, const QString &ownerTableHint = QString())
{
    if (snapshot.isEmpty() || oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return snapshot;

    QString colOwnerTable;
    QString oldShortCol;
    QString newShortCol;
    if (kind == SnapshotRenameKind::ColRename) {
        if (!ownerTableHint.isEmpty()) {
            const QString pfx = ownerTableHint + QLatin1Char('_');
            if (oldName.startsWith(pfx) && newName.startsWith(pfx)) {
                colOwnerTable = ownerTableHint;
                oldShortCol = oldName.mid(pfx.size());
                newShortCol = newName.mid(pfx.size());
            }
        }
        if (colOwnerTable.isEmpty()) {
            // Find longest shared '<tablePrefix>_' (supports workbook sheets like Book1_Sheet1_Col)
            int bestSplit = -1;
            const int maxShared = std::min(oldName.size(), newName.size());
            for (int i = 0; i < maxShared; ++i) {
                if (oldName.at(i) != newName.at(i))
                    break;
                if (oldName.at(i) == QLatin1Char('_') && i > 0 && i + 1 < oldName.size()
                    && i + 1 < newName.size()) {
                    bestSplit = i;
                }
            }
            if (bestSplit < 0)
                bestSplit = oldName.indexOf(QLatin1Char('_'));
            if (bestSplit > 0) {
                colOwnerTable = oldName.left(bestSplit);
                oldShortCol = oldName.mid(bestSplit + 1);
                const QString pfx = colOwnerTable + QLatin1Char('_');
                if (newName.startsWith(pfx))
                    newShortCol = newName.mid(pfx.size());
            }
        }
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const auto keepEmpty = Qt::KeepEmptyParts;
#else
    const auto keepEmpty = QString::KeepEmptyParts;
#endif

    QStringList lines = snapshot.split(QLatin1Char('\n'), keepEmpty);
    bool anyChanged = false;
    bool inSpectrogram = false;
    bool graphCurveChanged = false;
    int layerCurveIndex = 0;
    QHash<int, QPair<QString, QString>> changedLayerCurves;

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

    for (int li = 0; li < lines.size(); ++li) {
        QString line = lines[li];
        const bool hadCr = line.endsWith(QLatin1Char('\r'));
        if (hadCr)
            line.chop(1);

        const QString trimmed = line.trimmed();
        if (trimmed == QLatin1String("<graph>") || trimmed == QLatin1String("</graph>")) {
            inSpectrogram = false;
            graphCurveChanged = false;
            layerCurveIndex = 0;
            changedLayerCurves.clear();
            continue;
        }
        if (trimmed == QLatin1String("<spectrogram>")) {
            inSpectrogram = true;
            ++layerCurveIndex;
            continue;
        }
        if (trimmed == QLatin1String("</spectrogram>")) {
            inSpectrogram = false;
            continue;
        }

        bool lineChanged = false;

        if (inSpectrogram && kind == SnapshotRenameKind::MatrixRename) {
            if (trimmed.startsWith(QLatin1String("<matrix>"))
                && trimmed.endsWith(QLatin1String("</matrix>"))) {
                const QString matName = trimmed.mid(8, trimmed.size() - 17);
                if (matName == oldName) {
                    const int ltPos = line.indexOf(QLatin1Char('<'));
                    line = line.left(qMax(0, ltPos)) + QStringLiteral("<matrix>") + newName
                            + QStringLiteral("</matrix>");
                    lineChanged = true;
                }
            }
        } else if (!inSpectrogram && line.startsWith(QLatin1String("FunctionCurve\t"))) {
            ++layerCurveIndex;
        } else if (kind != SnapshotRenameKind::MatrixRename
                   && line.startsWith(QLatin1String("curve\t"))) {
            ++layerCurveIndex;
            QStringList parts = line.split(QLatin1Char('\t'), keepEmpty);
            if (parts.size() >= 6) {
                const int plotType = parts[3].toInt();
                if (plotType != Graph::Box) {
                    const QString nextX = remapFullColRef(parts[1], kind, oldName, newName);
                    if (nextX != parts[1]) {
                        parts[1] = nextX;
                        lineChanged = true;
                    }
                }
                const QString prevY = parts[2];
                const QString nextY = remapFullColRef(prevY, kind, oldName, newName);
                if (nextY != prevY) {
                    parts[2] = nextY;
                    changedLayerCurves.insert(layerCurveIndex, qMakePair(prevY, nextY));
                    lineChanged = true;
                }
                if ((plotType == Graph::VectXYXY || plotType == Graph::VectXYAM)
                    && parts.size() >= 29) {
                    for (int vi : { 23, 24 }) {
                        if (vi < parts.size()) {
                            const QString nextV =
                                    remapFullColRef(parts[vi], kind, oldName, newName);
                            if (nextV != parts[vi]) {
                                parts[vi] = nextV;
                                lineChanged = true;
                            }
                        }
                    }
                }
                if (lineChanged) {
                    line = parts.join(QLatin1Char('\t'));
                    graphCurveChanged = true;
                }
            }
        } else if (kind != SnapshotRenameKind::MatrixRename
                   && line.startsWith(QLatin1String("PieCurve\t"))) {
            ++layerCurveIndex;
            QStringList parts = line.split(QLatin1Char('\t'), keepEmpty);
            if (parts.size() >= 3) {
                const QString prevCol = parts[1];
                const QString nextCol = remapFullColRef(prevCol, kind, oldName, newName);
                if (nextCol != prevCol) {
                    parts[1] = nextCol;
                    changedLayerCurves.insert(layerCurveIndex, qMakePair(prevCol, nextCol));
                    line = parts.join(QLatin1Char('\t'));
                    lineChanged = true;
                    graphCurveChanged = true;
                }
            }
        } else if (kind != SnapshotRenameKind::MatrixRename
                   && line.startsWith(QLatin1String("ErrorBars\t"))) {
            ++layerCurveIndex;
            QStringList parts = line.split(QLatin1Char('\t'), keepEmpty);
            if (parts.size() >= 5) {
                for (int ei : { 2, 3, 4 }) {
                    const QString nextRef = remapFullColRef(parts[ei], kind, oldName, newName);
                    if (nextRef != parts[ei]) {
                        parts[ei] = nextRef;
                        lineChanged = true;
                    }
                }
                if (lineChanged) {
                    line = parts.join(QLatin1Char('\t'));
                    graphCurveChanged = true;
                }
            }
        } else if (kind != SnapshotRenameKind::MatrixRename && graphCurveChanged
                   && ((trimmed.startsWith(QLatin1String("<legend>\t"))
                        && trimmed.endsWith(QLatin1String("</legend>")))
                       || line.startsWith(QLatin1String("Legend\t")))) {
            const QString closeLegendTag = QStringLiteral("</legend>");
            const bool hasCloseTag = line.endsWith(closeLegendTag);
            const QString rawBody =
                    hasCloseTag ? line.left(line.size() - closeLegendTag.size()) : line;
            QStringList parts = rawBody.split(QLatin1Char('\t'), keepEmpty);
            for (int ti = 14; ti < parts.size(); ++ti) {
                const QString legLine = parts[ti];
                int prefixEnd = -1;
                int oneBased = -1;
                const int cPos = legLine.indexOf(QLatin1String("\\c{"));
                if (cPos >= 0 && legLine.left(cPos).trimmed().isEmpty()) {
                    const int closeBrace = legLine.indexOf(QLatin1Char('}'), cPos + 3);
                    if (closeBrace > cPos + 3) {
                        bool ok = false;
                        const int parsed =
                                legLine.mid(cPos + 3, closeBrace - (cPos + 3)).toInt(&ok);
                        if (ok && parsed >= 1) {
                            oneBased = parsed;
                            prefixEnd = closeBrace + 1;
                        }
                    }
                } else {
                    const int lPos = legLine.indexOf(QLatin1String("\\l("));
                    if (lPos >= 0 && legLine.left(lPos).trimmed().isEmpty()) {
                        const int closeParen = legLine.indexOf(QLatin1Char(')'), lPos + 3);
                        if (closeParen > lPos + 3) {
                            bool ok = false;
                            const int parsed =
                                    legLine.mid(lPos + 3, closeParen - (lPos + 3)).toInt(&ok);
                            if (ok && parsed >= 1) {
                                oneBased = parsed;
                                prefixEnd = closeParen + 1;
                            }
                        }
                    }
                }

                if (oneBased >= 1 && prefixEnd > 0) {
                    if (!changedLayerCurves.contains(oneBased))
                        continue;
                    const QPair<QString, QString> pair = changedLayerCurves.value(oneBased);
                    const QString cmdPrefix = legLine.left(prefixEnd);
                    const QString rest = legLine.mid(prefixEnd);
                    int leadSpaces = 0;
                    while (leadSpaces < rest.size() && rest.at(leadSpaces) == QLatin1Char(' '))
                        ++leadSpaces;
                    const QString coreRest = rest.mid(leadSpaces);
                    QString newRest = rest;
                    if (coreRest == pair.first) {
                        newRest = rest.left(leadSpaces) + pair.second;
                    } else if (kind == SnapshotRenameKind::ColRename && !oldShortCol.isEmpty()
                               && !newShortCol.isEmpty() && coreRest == oldShortCol) {
                        newRest = rest.left(leadSpaces) + newShortCol;
                    } else if (kind == SnapshotRenameKind::ColRename && !oldShortCol.isEmpty()
                               && !newShortCol.isEmpty() && coreRest.trimmed() == oldShortCol) {
                        const int idx = rest.indexOf(oldShortCol);
                        newRest = rest.left(idx) + newShortCol + rest.mid(idx + oldShortCol.size());
                    } else {
                        newRest = replaceExactToken(rest, pair.first, pair.second);
                    }
                    if (newRest != rest) {
                        parts[ti] = cmdPrefix + newRest;
                        lineChanged = true;
                    }
                } else if (!changedLayerCurves.isEmpty()) {
                    QString updated = legLine;
                    for (auto it = changedLayerCurves.constBegin();
                         it != changedLayerCurves.constEnd(); ++it) {
                        updated = replaceExactToken(updated, it.value().first, it.value().second);
                    }
                    if (updated != legLine) {
                        parts[ti] = updated;
                        lineChanged = true;
                    }
                }
            }
            if (lineChanged) {
                line = parts.join(QLatin1Char('\t'));
                if (hasCloseTag)
                    line += closeLegendTag;
            }
        } else if (kind != SnapshotRenameKind::MatrixRename
                   && line.startsWith(QLatin1String("AxisType\t"))) {
            QStringList parts = line.split(QLatin1Char('\t'), keepEmpty);
            for (int ai = 1; ai < parts.size(); ++ai) {
                const int semi = parts[ai].indexOf(QLatin1Char(';'));
                if (semi <= 0)
                    continue;
                const int fmtCode = parts[ai].left(semi).toInt();
                const QString payload = parts[ai].mid(semi + 1);
                if (fmtCode == static_cast<int>(Graph::AxisType::Txt)) {
                    const QString nextRef = remapFullColRef(payload, kind, oldName, newName);
                    if (nextRef != payload) {
                        parts[ai] = QString::number(fmtCode) + QLatin1Char(';') + nextRef;
                        lineChanged = true;
                    }
                } else if (fmtCode == static_cast<int>(Graph::AxisType::ColHeader)
                           && kind == SnapshotRenameKind::TableRename) {
                    if (payload == oldName) {
                        parts[ai] = QString::number(fmtCode) + QLatin1Char(';') + newName;
                        lineChanged = true;
                    }
                }
            }
            if (lineChanged)
                line = parts.join(QLatin1Char('\t'));
        } else if (trimmed.startsWith(QLatin1String("<ToriErrorBand"))
                   || trimmed.startsWith(QLatin1String("<ToriPointColorMap"))
                   || trimmed.startsWith(QLatin1String("<ToriDensity2D"))
                   || trimmed.startsWith(QLatin1String("<ToriPoleFigure"))
                   || trimmed.startsWith(QLatin1String("<ToriBackbones"))) {
            QVector<XmlAttrSpan> spans = parseSnapshotXmlAttrs(line);
            if (!spans.isEmpty()) {
                auto attrValue = [&](const QLatin1String &key) -> QString {
                    for (const XmlAttrSpan &sp : spans) {
                        if (sp.name == key)
                            return sp.value;
                    }
                    return QString();
                };

                QVector<QString> newVals(spans.size());
                for (int si = 0; si < spans.size(); ++si)
                    newVals[si] = spans[si].value;

                if (trimmed.startsWith(QLatin1String("<ToriErrorBand"))) {
                    if (kind != SnapshotRenameKind::MatrixRename) {
                        for (int si = 0; si < spans.size(); ++si) {
                            const QString &an = spans[si].name;
                            if (an == QLatin1String("x") || an == QLatin1String("y")
                                || an == QLatin1String("err")) {
                                newVals[si] = remapFullColRef(spans[si].value, kind, oldName,
                                                              newName);
                            }
                        }
                    }
                } else if (trimmed.startsWith(QLatin1String("<ToriPointColorMap"))) {
                    if (kind != SnapshotRenameKind::MatrixRename) {
                        const QString ownerPfx = colOwnerTable + QLatin1Char('_');
                        const bool sameOwner = !colOwnerTable.isEmpty()
                                && (attrValue(QLatin1String("y")).startsWith(ownerPfx)
                                    || attrValue(QLatin1String("x")).startsWith(ownerPfx));
                        for (int si = 0; si < spans.size(); ++si) {
                            const QString &an = spans[si].name;
                            if (an == QLatin1String("x") || an == QLatin1String("y")) {
                                newVals[si] = remapFullColRef(spans[si].value, kind, oldName,
                                                              newName);
                            } else if (an == QLatin1String("z") || an == QLatin1String("size")) {
                                QString nv = remapFullColRef(spans[si].value, kind, oldName,
                                                             newName);
                                if (nv == spans[si].value && kind == SnapshotRenameKind::ColRename
                                    && sameOwner && !oldShortCol.isEmpty() && !newShortCol.isEmpty()
                                    && spans[si].value == oldShortCol) {
                                    nv = newShortCol;
                                }
                                newVals[si] = nv;
                            }
                        }
                    }
                } else if (trimmed.startsWith(QLatin1String("<ToriDensity2D"))) {
                    const QString tagTbl = attrValue(QLatin1String("table"));
                    for (int si = 0; si < spans.size(); ++si) {
                        const QString &an = spans[si].name;
                        if (kind == SnapshotRenameKind::MatrixRename
                            && an == QLatin1String("matrix")) {
                            if (spans[si].value == oldName)
                                newVals[si] = newName;
                        } else if (kind == SnapshotRenameKind::TableRename) {
                            if (an == QLatin1String("table") && spans[si].value == oldName) {
                                newVals[si] = newName;
                            } else if (an == QLatin1String("x") || an == QLatin1String("y")) {
                                newVals[si] = remapFullColRef(spans[si].value, kind, oldName,
                                                              newName);
                            }
                        } else if (kind == SnapshotRenameKind::ColRename
                                   && !colOwnerTable.isEmpty() && tagTbl == colOwnerTable
                                   && (an == QLatin1String("x") || an == QLatin1String("y"))) {
                            if (spans[si].value == oldName)
                                newVals[si] = newName;
                            else if (!oldShortCol.isEmpty() && !newShortCol.isEmpty()
                                     && spans[si].value == oldShortCol)
                                newVals[si] = newShortCol;
                        }
                    }
                } else if (trimmed.startsWith(QLatin1String("<ToriPoleFigure"))) {
                    const QString tagTbl = attrValue(QLatin1String("table"));
                    for (int si = 0; si < spans.size(); ++si) {
                        const QString &an = spans[si].name;
                        if (kind == SnapshotRenameKind::MatrixRename
                            && (an == QLatin1String("matrix")
                                || an == QLatin1String("sourceMatrix"))) {
                            if (spans[si].value == oldName)
                                newVals[si] = newName;
                        } else if (kind == SnapshotRenameKind::TableRename) {
                            if (an == QLatin1String("table") && spans[si].value == oldName) {
                                newVals[si] = newName;
                            } else if (an == QLatin1String("a") || an == QLatin1String("b")
                                       || an == QLatin1String("i")) {
                                newVals[si] = remapFullColRef(spans[si].value, kind, oldName,
                                                              newName);
                            }
                        } else if (kind == SnapshotRenameKind::ColRename
                                   && !colOwnerTable.isEmpty() && tagTbl == colOwnerTable
                                   && (an == QLatin1String("a") || an == QLatin1String("b")
                                       || an == QLatin1String("i"))) {
                            if (spans[si].value == oldName)
                                newVals[si] = newName;
                            else if (!oldShortCol.isEmpty() && !newShortCol.isEmpty()
                                     && spans[si].value == oldShortCol)
                                newVals[si] = newShortCol;
                        }
                    }
                } else if (trimmed.startsWith(QLatin1String("<ToriBackbones"))
                           && kind != SnapshotRenameKind::MatrixRename) {
                    for (int si = 0; si < spans.size(); ++si) {
                        if (spans[si].name != QLatin1String("curveWidths"))
                            continue;
                        QStringList entries = spans[si].value.split(QLatin1Char(';'), keepEmpty);
                        bool cwChanged = false;
                        for (int ei = 0; ei < entries.size(); ++ei) {
                            const QString &ent = entries[ei];
                            const int colon = ent.indexOf(QLatin1Char(':'));
                            if (colon <= 0)
                                continue;
                            const int c1 = ent.indexOf(QLatin1Char(','), colon + 1);
                            if (c1 <= colon)
                                continue;
                            const int c2 = ent.indexOf(QLatin1Char(','), c1 + 1);
                            if (c2 <= c1)
                                continue;
                            const QString b64 = ent.mid(c2 + 1);
                            if (b64.isEmpty())
                                continue;
                            const QString decTitle = QString::fromUtf8(
                                    QByteArray::fromBase64(b64.toLatin1()));
                            const QString nextTitle =
                                    remapFullColRef(decTitle, kind, oldName, newName);
                            if (nextTitle != decTitle) {
                                const QString newB64 = QString::fromLatin1(
                                        nextTitle.toUtf8().toBase64());
                                entries[ei] = ent.left(c2 + 1) + newB64;
                                cwChanged = true;
                            }
                        }
                        if (cwChanged)
                            newVals[si] = entries.join(QLatin1Char(';'));
                    }
                }

                for (int si = spans.size() - 1; si >= 0; --si) {
                    if (newVals[si] != spans[si].value) {
                        line.replace(spans[si].valStart, spans[si].valLen,
                                     escapeSnapshotXmlAttr(newVals[si]));
                        lineChanged = true;
                    }
                }
            }
        }

        if (lineChanged) {
            lines[li] = hadCr ? (line + QLatin1Char('\r')) : line;
            anyChanged = true;
        }
    }

    return anyChanged ? lines.join(QLatin1Char('\n')) : snapshot;
}

void remapUndoCommandTree(QUndoCommand *cmd, SnapshotRenameKind kind, const QString &oldName,
                          const QString &newName, const QString &ownerTableHint = QString())
{
    if (!cmd)
        return;
    if (auto *snapCmd = dynamic_cast<MultiLayerSnapshotUndoCommand *>(cmd)) {
        snapCmd->remapSnapshots([&](const QString &s) {
            return remapSnapshotText(s, kind, oldName, newName, ownerTableHint);
        });
    }
    const int nChildren = cmd->childCount();
    for (int i = 0; i < nChildren; ++i) {
        remapUndoCommandTree(const_cast<QUndoCommand *>(cmd->child(i)), kind, oldName, newName,
                             ownerTableHint);
    }
}

} // namespace

void MultiLayer::remapUndoSnapshotsForTableRename(const QString &oldName, const QString &newName)
{
    if (!d_undo_stack || oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return;
    const int cnt = d_undo_stack->count();
    for (int i = 0; i < cnt; ++i) {
        remapUndoCommandTree(const_cast<QUndoCommand *>(d_undo_stack->command(i)),
                             SnapshotRenameKind::TableRename, oldName, newName);
    }
}

void MultiLayer::remapUndoSnapshotsForColRename(const QString &oldColName,
                                                const QString &newColName)
{
    if (!d_undo_stack || oldColName.isEmpty() || newColName.isEmpty()
        || oldColName == newColName)
        return;
    QString ownerHint;
    if (ApplicationWindow *app = findMultiLayerApp(this)) {
        auto checkOwnerList = [&](const QList<MyWidget *> &wins) {
            for (MyWidget *w : wins) {
                if (!w || !w->inherits("Table"))
                    continue;
                const QString tn = w->name();
                const QString pfx = tn + QLatin1Char('_');
                if (oldColName.startsWith(pfx) && newColName.startsWith(pfx)
                    && tn.size() > ownerHint.size()) {
                    ownerHint = tn;
                }
            }
        };
        checkOwnerList(app->windowsList());
        checkOwnerList(app->hiddenWindows);
    }
    const int cnt = d_undo_stack->count();
    for (int i = 0; i < cnt; ++i) {
        remapUndoCommandTree(const_cast<QUndoCommand *>(d_undo_stack->command(i)),
                             SnapshotRenameKind::ColRename, oldColName, newColName, ownerHint);
    }
}

void MultiLayer::remapUndoSnapshotsForMatrixRename(const QString &oldName, const QString &newName)
{
    if (!oldName.isEmpty() && !newName.isEmpty() && oldName != newName) {
        for (int i = 0; i < graphsList.count(); ++i) {
            if (Graph *gr = qobject_cast<Graph *>(graphsList.at(i)))
                ExtraPlotTypes::updateGraphTagsForRename(gr, oldName, newName, false, true);
        }
    }
    if (!d_undo_stack || oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return;
    const int cnt = d_undo_stack->count();
    for (int i = 0; i < cnt; ++i) {
        remapUndoCommandTree(const_cast<QUndoCommand *>(d_undo_stack->command(i)),
                             SnapshotRenameKind::MatrixRename, oldName, newName);
    }
}

bool MultiLayer::restoreSnapshotInPlace(ApplicationWindow *app, const QString &snapshot,
                                        int preferredActiveLayer, bool restoreGeometry)
{
    if (!app)
        app = findMultiLayerApp(this);
    if (!app || snapshot.trimmed().isEmpty() || d_is_restoring_snapshot)
        return false;

    struct SnapshotRestoreGuard {
        MultiLayer *ml;
        ApplicationWindow *app;
        int prevFileVersion;
        QStringList prevRenamedTables;
        bool prevAutoscale2D;

        SnapshotRestoreGuard(MultiLayer *m, ApplicationWindow *a)
            : ml(m),
              app(a),
              prevFileVersion(a->d_file_version),
              prevRenamedTables(a->renamedTables),
              prevAutoscale2D(a->autoscale2DPlots)
        {
            ml->d_is_restoring_snapshot = true;
            app->d_file_version = SciDAVis::version();
            app->renamedTables.clear();
            app->autoscale2DPlots = false;
        }
        ~SnapshotRestoreGuard()
        {
            app->d_file_version = prevFileVersion;
            app->renamedTables = prevRenamedTables;
            app->autoscale2DPlots = prevAutoscale2D;
            ml->d_is_restoring_snapshot = false;
        }
    } guard(this, app);

    CanvasInlineTextEditor::cancelActiveForMultiLayer(this);
    for (QWidget *gw : graphsList) {
        if (Graph *gr = qobject_cast<Graph *>(gw))
            gr->cancelInlineScaleEditor();
    }
    if (GraphMiniToolbar *mt = GraphMiniToolbar::instance())
        mt->cancelScheduledAndHide();

    app->invalidateModelessGraphDialogs(this);

    if (d_layers_selector)
        clearLayerSelector();

    if (addTextOn) {
        QApplication::restoreOverrideCursor();
        if (canvas)
            canvas->releaseMouse();
        addTextOn = false;
        emit drawTextOff();
    }

    d_inset_specs.clear();
    d_pending_inset_specs.clear();
    d_pending_recalc_links.clear();
    d_inset_drag_graph.clear();
    d_inset_drag_active = false;
    d_inset_drag_before_snap.clear();
    clearCtrlFontGestureBaseline();

    if (RecalcManager *rm = RecalcManager::instance(app)) {
        for (QWidget *gw : graphsList) {
            if (Graph *oldGr = qobject_cast<Graph *>(gw))
                rm->removeLinksForGraph(oldGr);
        }
    }

    const QString preservedName = this->name();
    const QRect preservedWindowGeo = this->geometry();

    // Detach old LayerButtons safely without synchronous delete
    const QWidgetList oldButtons = buttonsList;
    buttonsList.clear();
    for (QWidget *bw : oldButtons) {
        if (!bw)
            continue;
        bw->blockSignals(true);
        bw->disconnect();
        if (layerButtonsBox)
            layerButtonsBox->removeWidget(bw);
        bw->hide();
        bw->setParent(nullptr);
        bw->deleteLater();
    }

    // Detach old Graphs safely (setParent(nullptr) so canvas->childrenRect() excludes them!)
    const QWidgetList oldGraphs = graphsList;
    graphsList.clear();
    graphs = 0;
    active_graph = nullptr;
    for (QWidget *gw : oldGraphs) {
        if (!gw)
            continue;
        if (Graph *gr = qobject_cast<Graph *>(gw)) {
            gr->cancelInlineScaleEditor();
            gr->disableTools();
            gr->deselectMarker();
        }
        gw->blockSignals(true);
        gw->disconnect();
        gw->hide();
        gw->setParent(nullptr);
        gw->deleteLater();
    }

    // Reset all omitted-by-default MultiLayer fields BEFORE parsing header tags
    cols = 1;
    rows = 1;
    left_margin = 5;
    right_margin = 5;
    top_margin = 5;
    bottom_margin = 5;
    rowsSpace = 5;
    colsSpace = 5;
    l_canvas_width = 360;
    l_canvas_height = 360;
    hor_align = HCenter;
    vert_align = VCenter;
    d_page_unit = UnitPx;
    d_page_view_mode = false;
    d_page_zoom = 1.0;
    d_page_dpi = 300;
    d_page_width_val = 520.0;
    d_page_height_val = 520.0;
    d_base_page_px = QSize(520, 520);
    d_page_bg_color = QColor(Qt::white);
    d_keep_aspect_ratio = false;
    d_target_aspect_ratio = 0.0;
    d_panel_labels_enabled = false;
    d_panel_label_placement = PanelOutsideTopLeft;
    d_panel_label_pattern = QStringLiteral("(%a)");
    d_panel_label_font_pt = 0;
    d_applied_style_preset_id.clear();
    setProperty("toriAppliedStylePreset", QVariant());

    const QStringList lines = snapshot.split(QLatin1Char('\n'));
    bool expectHeaderRow = false;
    QSize savedNormalWinSz;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;

        if (trimmed.startsWith(QLatin1String("<ToriWinSize"))) {
            static const QRegularExpression wRx(QStringLiteral("w=\"(\\d+)\""));
            static const QRegularExpression hRx(QStringLiteral("h=\"(\\d+)\""));
            const QRegularExpressionMatch mw = wRx.match(trimmed);
            const QRegularExpressionMatch mh = hRx.match(trimmed);
            if (mw.hasMatch() && mh.hasMatch())
                savedNormalWinSz = QSize(mw.captured(1).toInt(), mh.captured(1).toInt());
            continue;
        }

        if (trimmed == QLatin1String("<multiLayer>")) {
            expectHeaderRow = true;
            continue;
        }
        if (trimmed.startsWith(QLatin1String("<multiLayer>\t"))) {
            const QStringList tf = line.split(QLatin1Char('\t'));
            if (tf.size() >= 3) {
                rows = qMax(1, tf[1].toInt());
                cols = qMax(1, tf[2].toInt());
            }
            expectHeaderRow = false;
            continue;
        }
        if (trimmed == QLatin1String("</multiLayer>"))
            break;

        if (expectHeaderRow) {
            expectHeaderRow = false;
            const QStringList hf = line.split(QLatin1Char('\t'));
            if (hf.size() >= 3) {
                cols = qMax(1, hf[1].toInt());
                rows = qMax(1, hf[2].toInt());
            }
            continue;
        }

        if (trimmed == QLatin1String("<graph>")) {
            QStringList graphLines;
            ++i;
            for (; i < lines.size(); ++i) {
                QString gl = lines.at(i);
                if (gl.endsWith(QLatin1Char('\r')))
                    gl.chop(1);
                graphLines.append(gl);
                if (gl.trimmed() == QLatin1String("</graph>"))
                    break;
            }
            if (graphLines.isEmpty()
                || graphLines.last().trimmed() != QLatin1String("</graph>")) {
                graphLines.append(QStringLiteral("</graph>"));
            }
            app->openGraph(app, this, graphLines);
            continue;
        }

        if (restorePhase5Tag(trimmed))
            continue;

        const QStringList f = line.split(QLatin1Char('\t'));
        if (f.isEmpty())
            continue;
        const QString key = f.at(0);

        if (key == QLatin1String("geometry")) {
            if (restoreGeometry && app)
                app->restoreWindowGeometry(app, this, line);
        } else if (key == QLatin1String("WindowLabel") && f.size() >= 3) {
            QString wl = f.at(1);
            wl.replace(QLatin1String("&#9;"), QLatin1String("\t"));
            wl.replace(QLatin1String("<br>"), QLatin1String("\n"));
            setWindowLabel(wl);
            setCaptionPolicy(static_cast<MyWidget::CaptionPolicy>(f.at(2).toInt()));
        } else if (key == QLatin1String("Margins") && f.size() >= 5) {
            setMargins(f.at(1).toInt(), f.at(2).toInt(), f.at(3).toInt(), f.at(4).toInt());
        } else if (key == QLatin1String("Spacing") && f.size() >= 3) {
            setSpacing(f.at(1).toInt(), f.at(2).toInt());
        } else if (key == QLatin1String("LayerCanvasSize") && f.size() >= 3) {
            l_canvas_width = qMax(50, f.at(1).toInt());
            l_canvas_height = qMax(50, f.at(2).toInt());
        } else if (key == QLatin1String("Alignement") && f.size() >= 3) {
            setAlignement(f.at(1).toInt(), f.at(2).toInt());
        }
    }

    if (!preservedName.isEmpty() && this->name() != preservedName)
        this->setName(preservedName);

    setPageBackgroundColor(d_page_bg_color);

    const QSize targetCanvasSz(qMax(80, qRound(d_base_page_px.width() * d_page_zoom)),
                               qMax(80, qRound(d_base_page_px.height() * d_page_zoom)));
    if (canvas && targetCanvasSz.isValid()) {
        QVector<bool> savedIgn(graphsList.count(), false);
        for (int k = 0; k < graphsList.count(); ++k) {
            if (Graph *gr = qobject_cast<Graph *>(graphsList.at(k))) {
                savedIgn[k] = gr->ignoresResizeEvents();
                gr->setIgnoreResizeEvents(true);
            }
        }
        lastSize = targetCanvasSz;
        syncMultiLayerPageHost(this, widget(), canvas, d_page_unit, d_page_view_mode,
                               d_base_page_px, d_page_zoom, d_page_bg_color);
        if (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4) {
            if (canvas->size() != targetCanvasSz)
                canvas->resize(targetCanvasSz);
        }
        if (!restoreGeometry && !isMaximized() && parentWidget()) {
            const bool freePx =
                    (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4);
            if (!freePx) {
                resizeMultiLayerSubWindowForCanvas(this, widget(), d_layer_pill_frame,
                                                   targetCanvasSz.width(), targetCanvasSz.height(),
                                                   false);
            } else {
                if (preservedWindowGeo.isValid() && this->pos() != preservedWindowGeo.topLeft())
                    this->move(preservedWindowGeo.topLeft());
                if (savedNormalWinSz.width() >= 120 && savedNormalWinSz.height() >= 120) {
                    if (this->size() != savedNormalWinSz)
                        this->resize(savedNormalWinSz);
                } else {
                    resizeMultiLayerSubWindowForCanvas(this, widget(), d_layer_pill_frame,
                                                       targetCanvasSz.width(),
                                                       targetCanvasSz.height(), true);
                }
            }
        }
        for (int k = 0; k < graphsList.count(); ++k) {
            if (Graph *gr = qobject_cast<Graph *>(graphsList.at(k)))
                gr->setIgnoreResizeEvents(savedIgn[k]);
        }
    }

    finalizePhase5Restore();

    if (!graphsList.isEmpty()) {
        const int idx = (preferredActiveLayer >= 0 && preferredActiveLayer < graphsList.count())
                ? preferredActiveLayer
                : 0;
        if (Graph *ag = qobject_cast<Graph *>(graphsList.at(idx)))
            setActiveGraph(ag);
    }

    for (int k = 0; k < graphsList.count(); ++k) {
        if (Graph *gr = qobject_cast<Graph *>(graphsList.at(k))) {
            Plot *pl = gr->plotWidget();
            if (pl && pl->property("_toriRestoredAutoscale").toBool())
                pl->setProperty("_toriRestoredAutoscale", QVariant());
            else
                gr->enableAutoscaling(guard.prevAutoscale2D);
            if (pl) {
                pl->updateLayout();
                pl->replot();
            }
        }
    }

    repositionCanvasStatusPill();
    updateCanvasStatusPill();
    notifyOleDirtyIfEmbedded();

    if (GraphInspectorDock *dock = app->graphInspectorDock())
        dock->setTargetMultiLayer(this, true);

    app->modifiedProject();
    app->updateUndoRedoActionsForActiveWindow();
    return true;
}

void MultiLayer::notifyOleDirtyIfEmbedded()
{
    if (d_is_restoring_snapshot)
        return;
    if (ApplicationWindow *app = findMultiLayerApp(this)) {
        if (app->isOleEmbeddedMode())
            app->saveProject();
    }
}

void MultiLayer::setPanelLabels(bool enabled, PanelLabelPlacement placement,
                                const QString &pattern, int fontPt)
{
    const bool layoutChanged =
            (d_panel_labels_enabled != enabled || d_panel_label_placement != placement);
    d_panel_labels_enabled = enabled;
    d_panel_label_placement = placement;
    d_panel_label_pattern = pattern.trimmed().isEmpty() ? QStringLiteral("(%a)") : pattern;
    d_panel_label_font_pt = qMax(0, fontPt);

    if (layoutChanged && graphs > 0 && !d_is_restoring_snapshot) {
        if (d_keep_aspect_ratio && qFuzzyCompare(d_target_aspect_ratio, 1.0))
            applySquareCanvasFrame70Percent();
        else if (rows > 1 || cols > 1)
            arrangeLayers(false, false);
    }

    syncCanvasOverlayVisibility();
    repositionCanvasStatusPill();
    emit modifiedPlot();
}

QRect MultiLayer::panelLabelsBoundingRect() const
{
    if (!d_panel_labels_enabled || graphsList.isEmpty())
        return QRect();

    auto segIntersectsBox = [](const QLineF &seg, const QRectF &box) -> bool {
        if (box.isEmpty())
            return false;
        const QRectF padBox = box.adjusted(-3.0, -3.0, 3.0, 3.0);
        if (padBox.contains(seg.p1()) || padBox.contains(seg.p2())
            || padBox.contains(seg.pointAt(0.5))) {
            return true;
        }
        const QLineF edges[4] = {
            QLineF(padBox.topLeft(), padBox.topRight()),
            QLineF(padBox.topRight(), padBox.bottomRight()),
            QLineF(padBox.bottomRight(), padBox.bottomLeft()),
            QLineF(padBox.bottomLeft(), padBox.topLeft()),
        };
        for (const QLineF &e : edges) {
            QPointF ip;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            if (seg.intersects(e, &ip) == QLineF::BoundedIntersection)
                return true;
#else
            if (seg.intersect(e, &ip) == QLineF::BoundedIntersection)
                return true;
#endif
        }
        return false;
    };

    struct PanelInfo {
        Graph *gr = nullptr;
        QRect innerCanvas;
        QFont fnt;
        QString text;
        int textW = 12;
        int ascent = 10;
        int descent = 3;
        int height = 13;
        int cx = 0;
        int cy = 0;
        int col = 0;
        int row = 0;
    };

    QVector<PanelInfo> panels;
    const int effCols = qMax(1, cols);
    int panelIdx = 0;
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (!gr || !gr->plotWidget())
            continue;
        if (gr->linkedParentLayer() || d_inset_specs.contains(gr))
            continue;

        Plot *plot = gr->plotWidget();
        plot->updateLayout();
        const QRect innerCanvas = plot->plotLayout()->canvasRect().translated(gr->pos());
        if (innerCanvas.width() < 20 || innerCanvas.height() < 20) {
            ++panelIdx;
            continue;
        }

        double pt = double(d_panel_label_font_pt);
        if (pt <= 0.0) {
            const QFont tf = gr->axisTitleFont(QwtPlot::yLeft);
            const double titlePt =
                    (tf.pointSizeF() > 0.0) ? tf.pointSizeF() : double(tf.pointSize());
            pt = (titlePt > 0.0) ? qBound(9.0, titlePt >= 24.0 ? titlePt : titlePt + 1.0, 40.0)
                                 : 12.0;
        }

        QFont fnt(QStringLiteral("Arial"));
        fnt.setPointSizeF(pt);
        fnt.setWeight(QFont::Normal);
        fnt.setItalic(false);
        fnt.setBold(false);
        fnt.setHintingPreference(QFont::PreferNoHinting);
        fnt = plot->displayFont(fnt);
        const QFontMetrics fm(fnt);
        const QString text = formatPanelLabel(d_panel_label_pattern, panelIdx);
        const int textW = qMax(12, fm.horizontalAdvance(text));

        PanelInfo pi;
        pi.gr = gr;
        pi.innerCanvas = innerCanvas;
        pi.fnt = fnt;
        pi.text = text;
        pi.textW = textW;
        pi.ascent = fm.ascent();
        pi.descent = fm.descent();
        pi.height = fm.height();
        pi.col = (effCols > 1) ? (panelIdx % effCols) : 0;
        pi.row = (effCols > 1) ? (panelIdx / effCols) : panelIdx;
        pi.cx = innerCanvas.left() + 8;
        pi.cy = innerCanvas.top() + 6 + pi.ascent;

        if (d_panel_label_placement == PanelOutsideTopLeft) {
            const int minOutsideDx = qMax(32, textW + 6);
            pi.cx = qMin(gr->pos().x() + 2, innerCanvas.left() - minOutsideDx);
            int baseCy = innerCanvas.top() - qMax(8, pi.descent + 4);
            if (plot->axisEnabled(QwtPlot::yLeft)) {
                const int yTitleSpan = plot->axisTitle(QwtPlot::yLeft)
                                               .textSize(plot->displayFont(
                                                       gr->axisTitleFont(QwtPlot::yLeft)))
                                               .width();
                if (yTitleSpan > 0 && pi.cx + textW >= gr->pos().x() - 6) {
                    const int yTitleTop = innerCanvas.center().y() - (yTitleSpan + 1) / 2;
                    baseCy = qMin(baseCy, yTitleTop - qMax(6, pi.descent + 3));
                }
            }
            if (plot->axisEnabled(QwtPlot::xTop) && plot->axisScaleDraw(QwtPlot::xTop)
                && (plot->axisScaleDraw(QwtPlot::xTop)->hasComponent(QwtAbstractScaleDraw::Labels)
                    || !gr->axisTitle(QwtPlot::xTop).trimmed().isEmpty())) {
                baseCy = qMin(baseCy, gr->pos().y() - qMax(4, pi.descent + 2));
            }
            pi.cy = baseCy;
        } else if (d_panel_label_placement == PanelInsideTopLeft) {
            const QRectF srcCanvasRectF(innerCanvas);
            const QwtScaleMap xMap = plot->canvasMap(QwtPlot::xBottom);
            const QwtScaleMap yMap = plot->canvasMap(QwtPlot::yLeft);
            QVector<QRectF> avoidBoxes;
            QVector<QLineF> avoidSegs;
            if (gr->isLegendVisible()) {
                if (Legend *leg = gr->legend()) {
                    const QRect lr = leg->rect();
                    if (!lr.isEmpty())
                        avoidBoxes.append(QRectF(lr.translated(innerCanvas.topLeft())));
                }
            }
            const QVector<int> txtKeys = gr->textMarkerKeys();
            for (int tid : txtKeys) {
                Legend *tm = gr->textMarker(tid);
                if (!tm || !tm->isVisible() || tm == gr->legend())
                    continue;
                const QRect tr = tm->rect();
                if (!tr.isEmpty())
                    avoidBoxes.append(QRectF(tr.translated(innerCanvas.topLeft())));
            }
            for (auto it = d_inset_specs.constBegin(); it != d_inset_specs.constEnd(); ++it) {
                Graph *inset = it.key();
                const InsetSpec &spec = it.value();
                if (!inset || inset->isHidden() || spec.sourceLayer.data() != gr)
                    continue;
                Plot *inPlot = inset->plotWidget();
                if (inPlot) {
                    inPlot->updateLayout();
                    avoidBoxes.append(QRectF(inset->pos(), inPlot->size()));
                }
                QRectF zoomData = spec.zoomDataRect;
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
                if (zoomData.isEmpty() && qAbs(zoomData.width()) <= 1e-15
                    && qAbs(zoomData.height()) <= 1e-15)
                    continue;
                const double rx0 = innerCanvas.left() + xMap.xTransform(zoomData.left());
                const double rx1 = innerCanvas.left() + xMap.xTransform(zoomData.right());
                const double ry0 = innerCanvas.top() + yMap.xTransform(zoomData.top());
                const double ry1 = innerCanvas.top() + yMap.xTransform(zoomData.bottom());
                if (!std::isfinite(rx0) || !std::isfinite(rx1) || !std::isfinite(ry0)
                    || !std::isfinite(ry1))
                    continue;
                const QRectF rawRoi(QPointF(qMin(rx0, rx1), qMin(ry0, ry1)),
                                    QPointF(qMax(rx0, rx1), qMax(ry0, ry1)));
                const QRectF roiBox = rawRoi.intersected(srcCanvasRectF);
                if (roiBox.width() < 2.0 || roiBox.height() < 2.0)
                    continue;
                avoidBoxes.append(roiBox);

                if (spec.showConnectors && inPlot) {
                    const QRect inCanvasLocal = inPlot->plotLayout()->canvasRect();
                    if (inCanvasLocal.width() >= 12 && inCanvasLocal.height() >= 12) {
                        const QRectF insetFrame(inCanvasLocal.translated(inset->pos()));
                        const QPointF rC[4] = { roiBox.topLeft(), roiBox.topRight(),
                                                roiBox.bottomRight(), roiBox.bottomLeft() };
                        const QPointF iC[4] = { insetFrame.topLeft(), insetFrame.topRight(),
                                                insetFrame.bottomRight(), insetFrame.bottomLeft() };
                        struct PairIdx { int r1, i1, r2, i2; };
                        static const PairIdx kP[6] = { { 1, 0, 2, 3 }, { 0, 1, 3, 2 },
                                                       { 0, 3, 1, 2 }, { 3, 0, 2, 1 },
                                                       { 0, 0, 2, 2 }, { 3, 3, 1, 1 } };
                        int bestP = 0;
                        double bestScore = 1e30;
                        for (int p = 0; p < 6; ++p) {
                            const QLineF l1(rC[kP[p].r1], iC[kP[p].i1]);
                            const QLineF l2(rC[kP[p].r2], iC[kP[p].i2]);
                            QPointF ip;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                            if (l1.intersects(l2, &ip) == QLineF::BoundedIntersection
                                && QLineF(ip, l1.p1()).length() > 0.75
                                && QLineF(ip, l1.p2()).length() > 0.75)
                                continue;
#else
                            if (l1.intersect(l2, &ip) == QLineF::BoundedIntersection
                                && QLineF(ip, l1.p1()).length() > 0.75
                                && QLineF(ip, l1.p2()).length() > 0.75)
                                continue;
#endif
                            const int hits = (segIntersectsBox(l1, roiBox.adjusted(2, 2, -2, -2)) ? 1 : 0)
                                    + (segIntersectsBox(l1, insetFrame.adjusted(2, 2, -2, -2)) ? 1 : 0)
                                    + (segIntersectsBox(l2, roiBox.adjusted(2, 2, -2, -2)) ? 1 : 0)
                                    + (segIntersectsBox(l2, insetFrame.adjusted(2, 2, -2, -2)) ? 1 : 0);
                            const double sc = hits * 1e6 + l1.length() + l2.length();
                            if (sc < bestScore) {
                                bestScore = sc;
                                bestP = p;
                            }
                        }
                        avoidSegs.append(QLineF(rC[kP[bestP].r1], iC[kP[bestP].i1]));
                        avoidSegs.append(QLineF(rC[kP[bestP].r2], iC[kP[bestP].i2]));
                    }
                }
            }

            auto hitsAny = [&](int testCx, int testCy) -> bool {
                const QRectF cBox(testCx - 4, testCy - pi.ascent - 4, textW + 8, pi.height + 8);
                for (const QRectF &b : avoidBoxes) {
                    if (b.intersects(cBox))
                        return true;
                }
                for (const QLineF &sg : avoidSegs) {
                    if (segIntersectsBox(sg, cBox))
                        return true;
                }
                return false;
            };

            if (hitsAny(pi.cx, pi.cy)) {
                bool placed = false;
                const int minX = innerCanvas.left() + 8;
                const int maxX = qMax(minX, innerCanvas.right() - textW - 8);
                const int minY = innerCanvas.top() + 6 + pi.ascent;
                const int maxY = qMax(minY, innerCanvas.bottom() - 8);
                for (int tx = minX; tx <= maxX && !placed; tx += 12) {
                    if (!hitsAny(tx, pi.cy)) {
                        pi.cx = tx;
                        placed = true;
                    }
                }
                for (int ty = minY + 12; ty <= maxY && !placed; ty += 12) {
                    if (!hitsAny(minX, ty)) {
                        pi.cx = minX;
                        pi.cy = ty;
                        placed = true;
                    }
                }
                for (int ty = minY; ty <= maxY && !placed; ty += 14) {
                    for (int tx = minX; tx <= maxX && !placed; tx += 14) {
                        if (!hitsAny(tx, ty)) {
                            pi.cx = tx;
                            pi.cy = ty;
                            placed = true;
                        }
                    }
                }
                if (!placed && !hitsAny(maxX, maxY)) {
                    pi.cx = maxX;
                    pi.cy = maxY;
                }
            }
        }

        panels.append(pi);
        ++panelIdx;
    }

    if (d_panel_label_placement == PanelOutsideTopLeft && panels.size() > 1) {
        QMap<int, int> colMinCx;
        QMap<int, int> rowMinCy;
        for (const PanelInfo &pi : panels) {
            colMinCx[pi.col] = colMinCx.contains(pi.col) ? qMin(colMinCx[pi.col], pi.cx) : pi.cx;
            rowMinCy[pi.row] = rowMinCy.contains(pi.row) ? qMin(rowMinCy[pi.row], pi.cy) : pi.cy;
        }
        for (PanelInfo &pi : panels) {
            pi.cx = colMinCx[pi.col];
            pi.cy = rowMinCy[pi.row];
        }
    }

    QRect bounds;
    for (const PanelInfo &pi : panels) {
        const QRect glyphBox(pi.cx - 4, pi.cy - pi.ascent - 4, pi.textW + 8, pi.height + 8);
        bounds = bounds.isEmpty() ? glyphBox : bounds.united(glyphBox);
    }
    return bounds;
}

void MultiLayer::drawPanelLabels(QPainter *painter, const QPoint &originOffset) const
{
    if (!d_panel_labels_enabled || !painter || graphsList.isEmpty())
        return;

    auto segIntersectsBox = [](const QLineF &seg, const QRectF &box) -> bool {
        if (box.isEmpty())
            return false;
        const QRectF padBox = box.adjusted(-3.0, -3.0, 3.0, 3.0);
        if (padBox.contains(seg.p1()) || padBox.contains(seg.p2())
            || padBox.contains(seg.pointAt(0.5))) {
            return true;
        }
        const QLineF edges[4] = {
            QLineF(padBox.topLeft(), padBox.topRight()),
            QLineF(padBox.topRight(), padBox.bottomRight()),
            QLineF(padBox.bottomRight(), padBox.bottomLeft()),
            QLineF(padBox.bottomLeft(), padBox.topLeft()),
        };
        for (const QLineF &e : edges) {
            QPointF ip;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            if (seg.intersects(e, &ip) == QLineF::BoundedIntersection)
                return true;
#else
            if (seg.intersect(e, &ip) == QLineF::BoundedIntersection)
                return true;
#endif
        }
        return false;
    };

    struct PanelDrawItem {
        QFont fnt;
        QString text;
        int ascent = 10;
        int cx = 0;
        int cy = 0;
        int col = 0;
        int row = 0;
    };

    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    QVector<PanelDrawItem> items;
    const int effCols = qMax(1, cols);
    int panelIdx = 0;
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (!gr || !gr->plotWidget())
            continue;
        if (gr->linkedParentLayer() || d_inset_specs.contains(gr))
            continue;

        Plot *plot = gr->plotWidget();
        if (painter->device() && painter->device()->devType() != QInternal::Widget)
            plot->updateLayout();
        const QRect innerCanvas = plot->plotLayout()->canvasRect().translated(gr->pos());
        if (innerCanvas.width() < 20 || innerCanvas.height() < 20) {
            ++panelIdx;
            continue;
        }

        double pt = double(d_panel_label_font_pt);
        if (pt <= 0.0) {
            const QFont tf = gr->axisTitleFont(QwtPlot::yLeft);
            const double titlePt =
                    (tf.pointSizeF() > 0.0) ? tf.pointSizeF() : double(tf.pointSize());
            pt = (titlePt > 0.0) ? qBound(9.0, titlePt >= 24.0 ? titlePt : titlePt + 1.0, 40.0)
                                 : 12.0;
        }

        QFont fnt(QStringLiteral("Arial"));
        fnt.setPointSizeF(pt);
        fnt.setWeight(QFont::Normal);
        fnt.setItalic(false);
        fnt.setBold(false);
        fnt.setHintingPreference(QFont::PreferNoHinting);
        // Exports run at 100 % (ScopedUnzoomForExport), so this only enlarges the screen view.
        fnt = plot->displayFont(fnt);

        const QString text = formatPanelLabel(d_panel_label_pattern, panelIdx);
        const QFontMetrics fm(fnt);
        const int textW = qMax(12, fm.horizontalAdvance(text));

        PanelDrawItem item;
        item.fnt = fnt;
        item.text = text;
        item.ascent = fm.ascent();
        item.col = (effCols > 1) ? (panelIdx % effCols) : 0;
        item.row = (effCols > 1) ? (panelIdx / effCols) : panelIdx;
        item.cx = innerCanvas.left() + 8;
        item.cy = innerCanvas.top() + 6 + fm.ascent();

        if (d_panel_label_placement == PanelOutsideTopLeft) {
            const int minOutsideDx = qMax(32, textW + 6);
            item.cx = qMin(gr->pos().x() + 2, innerCanvas.left() - minOutsideDx);
            int baseCy = innerCanvas.top() - qMax(8, fm.descent() + 4);
            if (plot->axisEnabled(QwtPlot::yLeft)) {
                const int yTitleSpan = plot->axisTitle(QwtPlot::yLeft)
                                               .textSize(plot->displayFont(
                                                       gr->axisTitleFont(QwtPlot::yLeft)))
                                               .width();
                if (yTitleSpan > 0 && item.cx + textW >= gr->pos().x() - 6) {
                    const int yTitleTop = innerCanvas.center().y() - (yTitleSpan + 1) / 2;
                    baseCy = qMin(baseCy, yTitleTop - qMax(6, fm.descent() + 3));
                }
            }
            if (plot->axisEnabled(QwtPlot::xTop) && plot->axisScaleDraw(QwtPlot::xTop)
                && (plot->axisScaleDraw(QwtPlot::xTop)->hasComponent(QwtAbstractScaleDraw::Labels)
                    || !gr->axisTitle(QwtPlot::xTop).trimmed().isEmpty())) {
                baseCy = qMin(baseCy, gr->pos().y() - qMax(4, fm.descent() + 2));
            }
            item.cy = baseCy;
        } else if (d_panel_label_placement == PanelInsideTopLeft) {
            const QRectF srcCanvasRectF(innerCanvas);
            const QwtScaleMap xMap = plot->canvasMap(QwtPlot::xBottom);
            const QwtScaleMap yMap = plot->canvasMap(QwtPlot::yLeft);
            QVector<QRectF> avoidBoxes;
            QVector<QLineF> avoidSegs;
            if (gr->isLegendVisible()) {
                if (Legend *leg = gr->legend()) {
                    const QRect lr = leg->rect();
                    if (!lr.isEmpty())
                        avoidBoxes.append(QRectF(lr.translated(innerCanvas.topLeft())));
                }
            }
            const QVector<int> txtKeys = gr->textMarkerKeys();
            for (int tid : txtKeys) {
                Legend *tm = gr->textMarker(tid);
                if (!tm || !tm->isVisible() || tm == gr->legend())
                    continue;
                const QRect tr = tm->rect();
                if (!tr.isEmpty())
                    avoidBoxes.append(QRectF(tr.translated(innerCanvas.topLeft())));
            }
            for (auto it = d_inset_specs.constBegin(); it != d_inset_specs.constEnd(); ++it) {
                Graph *inset = it.key();
                const InsetSpec &spec = it.value();
                if (!inset || inset->isHidden() || spec.sourceLayer.data() != gr)
                    continue;
                Plot *inPlot = inset->plotWidget();
                if (inPlot) {
                    if (painter->device() && painter->device()->devType() != QInternal::Widget)
                        inPlot->updateLayout();
                    avoidBoxes.append(QRectF(inset->pos(), inPlot->size()));
                }
                QRectF zoomData = spec.zoomDataRect;
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
                if (zoomData.isEmpty() && qAbs(zoomData.width()) <= 1e-15
                    && qAbs(zoomData.height()) <= 1e-15)
                    continue;
                const double rx0 = innerCanvas.left() + xMap.xTransform(zoomData.left());
                const double rx1 = innerCanvas.left() + xMap.xTransform(zoomData.right());
                const double ry0 = innerCanvas.top() + yMap.xTransform(zoomData.top());
                const double ry1 = innerCanvas.top() + yMap.xTransform(zoomData.bottom());
                if (!std::isfinite(rx0) || !std::isfinite(rx1) || !std::isfinite(ry0)
                    || !std::isfinite(ry1))
                    continue;
                const QRectF rawRoi(QPointF(qMin(rx0, rx1), qMin(ry0, ry1)),
                                    QPointF(qMax(rx0, rx1), qMax(ry0, ry1)));
                const QRectF roiBox = rawRoi.intersected(srcCanvasRectF);
                if (roiBox.width() < 2.0 || roiBox.height() < 2.0)
                    continue;
                avoidBoxes.append(roiBox);

                if (spec.showConnectors && inPlot) {
                    const QRect inCanvasLocal = inPlot->plotLayout()->canvasRect();
                    if (inCanvasLocal.width() >= 12 && inCanvasLocal.height() >= 12) {
                        const QRectF insetFrame(inCanvasLocal.translated(inset->pos()));
                        const QPointF rC[4] = { roiBox.topLeft(), roiBox.topRight(),
                                                roiBox.bottomRight(), roiBox.bottomLeft() };
                        const QPointF iC[4] = { insetFrame.topLeft(), insetFrame.topRight(),
                                                insetFrame.bottomRight(), insetFrame.bottomLeft() };
                        struct PairIdx { int r1, i1, r2, i2; };
                        static const PairIdx kP[6] = { { 1, 0, 2, 3 }, { 0, 1, 3, 2 },
                                                       { 0, 3, 1, 2 }, { 3, 0, 2, 1 },
                                                       { 0, 0, 2, 2 }, { 3, 3, 1, 1 } };
                        int bestP = 0;
                        double bestScore = 1e30;
                        for (int p = 0; p < 6; ++p) {
                            const QLineF l1(rC[kP[p].r1], iC[kP[p].i1]);
                            const QLineF l2(rC[kP[p].r2], iC[kP[p].i2]);
                            QPointF ip;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                            if (l1.intersects(l2, &ip) == QLineF::BoundedIntersection
                                && QLineF(ip, l1.p1()).length() > 0.75
                                && QLineF(ip, l1.p2()).length() > 0.75)
                                continue;
#else
                            if (l1.intersect(l2, &ip) == QLineF::BoundedIntersection
                                && QLineF(ip, l1.p1()).length() > 0.75
                                && QLineF(ip, l1.p2()).length() > 0.75)
                                continue;
#endif
                            const int hits = (segIntersectsBox(l1, roiBox.adjusted(2, 2, -2, -2)) ? 1 : 0)
                                    + (segIntersectsBox(l1, insetFrame.adjusted(2, 2, -2, -2)) ? 1 : 0)
                                    + (segIntersectsBox(l2, roiBox.adjusted(2, 2, -2, -2)) ? 1 : 0)
                                    + (segIntersectsBox(l2, insetFrame.adjusted(2, 2, -2, -2)) ? 1 : 0);
                            const double sc = hits * 1e6 + l1.length() + l2.length();
                            if (sc < bestScore) {
                                bestScore = sc;
                                bestP = p;
                            }
                        }
                        avoidSegs.append(QLineF(rC[kP[bestP].r1], iC[kP[bestP].i1]));
                        avoidSegs.append(QLineF(rC[kP[bestP].r2], iC[kP[bestP].i2]));
                    }
                }
            }

            auto hitsAny = [&](int testCx, int testCy) -> bool {
                const QRectF cBox(testCx - 4, testCy - fm.ascent() - 4, textW + 8, fm.height() + 8);
                for (const QRectF &b : avoidBoxes) {
                    if (b.intersects(cBox))
                        return true;
                }
                for (const QLineF &sg : avoidSegs) {
                    if (segIntersectsBox(sg, cBox))
                        return true;
                }
                return false;
            };

            if (hitsAny(item.cx, item.cy)) {
                bool placed = false;
                const int minX = innerCanvas.left() + 8;
                const int maxX = qMax(minX, innerCanvas.right() - textW - 8);
                const int minY = innerCanvas.top() + 6 + fm.ascent();
                const int maxY = qMax(minY, innerCanvas.bottom() - 8);
                for (int tx = minX; tx <= maxX && !placed; tx += 12) {
                    if (!hitsAny(tx, item.cy)) {
                        item.cx = tx;
                        placed = true;
                    }
                }
                for (int ty = minY + 12; ty <= maxY && !placed; ty += 12) {
                    if (!hitsAny(minX, ty)) {
                        item.cx = minX;
                        item.cy = ty;
                        placed = true;
                    }
                }
                for (int ty = minY; ty <= maxY && !placed; ty += 14) {
                    for (int tx = minX; tx <= maxX && !placed; tx += 14) {
                        if (!hitsAny(tx, ty)) {
                            item.cx = tx;
                            item.cy = ty;
                            placed = true;
                        }
                    }
                }
                if (!placed && !hitsAny(maxX, maxY)) {
                    item.cx = maxX;
                    item.cy = maxY;
                }
            }
        }

        items.append(item);
        ++panelIdx;
    }

    if (d_panel_label_placement == PanelOutsideTopLeft && items.size() > 1) {
        QMap<int, int> colMinCx;
        QMap<int, int> rowMinCy;
        for (const PanelDrawItem &it : items) {
            colMinCx[it.col] = colMinCx.contains(it.col) ? qMin(colMinCx[it.col], it.cx) : it.cx;
            rowMinCy[it.row] = rowMinCy.contains(it.row) ? qMin(rowMinCy[it.row], it.cy) : it.cy;
        }
        for (PanelDrawItem &it : items) {
            it.cx = colMinCx[it.col];
            it.cy = rowMinCy[it.row];
        }
    }

    painter->setPen(QColor(Qt::black));
    for (const PanelDrawItem &it : items) {
        painter->setFont(it.fnt);
        const int tx = qMax(2, it.cx + originOffset.x());
        const int ty = qMax(it.ascent + 2, it.cy + originOffset.y());
        painter->drawText(QPoint(tx, ty), it.text);
    }

    painter->restore();
}

void MultiLayer::drawInsetConnectors(QPainter *painter, const QPoint &originOffset) const
{
    if (d_inset_specs.isEmpty() || !painter)
        return;

    auto linesCrossBounded = [](const QLineF &a, const QLineF &b) -> bool {
        QPointF ip;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const QLineF::IntersectionType t = a.intersects(b, &ip);
#else
        const QLineF::IntersectionType t = a.intersect(b, &ip);
#endif
        if (t != QLineF::BoundedIntersection)
            return false;
        auto nearEndpoint = [](const QPointF &p, const QLineF &ln) {
            return QLineF(p, ln.p1()).length() < 0.75 || QLineF(p, ln.p2()).length() < 0.75;
        };
        if (nearEndpoint(ip, a) && nearEndpoint(ip, b))
            return false;
        return true;
    };

    auto segmentHitsRectInterior = [&](const QLineF &seg, const QRectF &box) -> bool {
        if (box.width() <= 3.0 || box.height() <= 3.0)
            return false;
        const QRectF inner = box.adjusted(1.0, 1.0, -1.0, -1.0);
        if (inner.contains(seg.pointAt(0.25)) || inner.contains(seg.pointAt(0.5))
            || inner.contains(seg.pointAt(0.75)))
            return true;
        const QPointF tl = inner.topLeft();
        const QPointF tr = inner.topRight();
        const QPointF br = inner.bottomRight();
        const QPointF bl = inner.bottomLeft();
        return linesCrossBounded(seg, QLineF(tl, tr)) || linesCrossBounded(seg, QLineF(tr, br))
                || linesCrossBounded(seg, QLineF(br, bl)) || linesCrossBounded(seg, QLineF(bl, tl));
    };

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QList<Graph *> ordered = orderedLayersForRender();
    for (Graph *inset : ordered) {
        if (!inset || !d_inset_specs.contains(inset) || inset->isHidden())
            continue;
        const InsetSpec &spec = d_inset_specs[inset];
        if (!spec.showConnectors)
            continue;
        Graph *src = spec.sourceLayer.data();
        if (!src || src == inset || src->isHidden())
            continue;

        Plot *srcPlot = src->plotWidget();
        Plot *inPlot = inset->plotWidget();
        if (!srcPlot || !inPlot)
            continue;

        if (painter->device() && painter->device()->devType() != QInternal::Widget) {
            srcPlot->updateLayout();
            inPlot->updateLayout();
        }

        const QRect srcCanvasLocal = srcPlot->plotLayout()->canvasRect();
        const QRect inCanvasLocal = inPlot->plotLayout()->canvasRect();
        if (srcCanvasLocal.width() < 12 || srcCanvasLocal.height() < 12
            || inCanvasLocal.width() < 12 || inCanvasLocal.height() < 12)
            continue;

        QRectF zoomData = spec.zoomDataRect;
        if (const QwtScaleDiv *ixSd = inPlot->axisScaleDiv(QwtPlot::xBottom)) {
            if (const QwtScaleDiv *iySd = inPlot->axisScaleDiv(QwtPlot::yLeft)) {
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
        if (zoomData.isEmpty()
            && qAbs(zoomData.width()) <= 1e-15 && qAbs(zoomData.height()) <= 1e-15)
            continue;

        const QPoint srcOrigin = src->pos() + srcCanvasLocal.topLeft() + originOffset;
        const QRectF srcCanvasRectF(srcOrigin, srcCanvasLocal.size());
        const QwtScaleMap xMap = srcPlot->canvasMap(QwtPlot::xBottom);
        const QwtScaleMap yMap = srcPlot->canvasMap(QwtPlot::yLeft);

        const double rx0 = srcOrigin.x() + xMap.xTransform(zoomData.left());
        const double rx1 = srcOrigin.x() + xMap.xTransform(zoomData.right());
        const double ry0 = srcOrigin.y() + yMap.xTransform(zoomData.top());
        const double ry1 = srcOrigin.y() + yMap.xTransform(zoomData.bottom());
        if (!std::isfinite(rx0) || !std::isfinite(rx1) || !std::isfinite(ry0) || !std::isfinite(ry1))
            continue;

        const QRectF rawRoi(QPointF(qMin(rx0, rx1), qMin(ry0, ry1)),
                            QPointF(qMax(rx0, rx1), qMax(ry0, ry1)));
        const QRectF roiBox = rawRoi.intersected(srcCanvasRectF);
        if (roiBox.width() < 2.0 || roiBox.height() < 2.0)
            continue;

        const QRectF insetFrame(inCanvasLocal.translated(inset->pos() + originOffset));
        const QRectF insetOuterGeo(inset->pos() + originOffset, inPlot->size());

        // 1. Draw crisp 1 px source ROI rectangle on sourceLayer, masked under insetOuterGeo
        painter->save();
        QPainterPath roiClip;
        roiClip.addRect(srcCanvasRectF.adjusted(-1.0, -1.0, 1.0, 1.0));
        QPainterPath insetMask;
        insetMask.addRect(insetOuterGeo);
        painter->setClipPath(roiClip.subtracted(insetMask));
        QPen roiPen(QColor(51, 65, 85, 230), 1.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        roiPen.setCosmetic(false);
        painter->setPen(roiPen);
        painter->setBrush(QColor(37, 99, 235, 14));
        painter->drawRect(roiBox);
        painter->restore();

        // 2. Select optimal non-crossing symmetric pair of dashed zoom leader lines
        // Corner indices: 0 = TL, 1 = TR, 2 = BR, 3 = BL
        const QPointF rC[4] = { roiBox.topLeft(), roiBox.topRight(), roiBox.bottomRight(),
                                roiBox.bottomLeft() };
        const QPointF iC[4] = { insetFrame.topLeft(), insetFrame.topRight(),
                                insetFrame.bottomRight(), insetFrame.bottomLeft() };

        struct CandidatePair {
            int r1, i1, r2, i2;
        };
        static const CandidatePair kPairs[6] = {
            { 1, 0, 2, 3 }, // Facing Right:  TR->TL, BR->BL
            { 0, 1, 3, 2 }, // Facing Left:   TL->TR, BL->BR
            { 0, 3, 1, 2 }, // Facing Above:  TL->BL, TR->BR
            { 3, 0, 2, 1 }, // Facing Below:  BL->TL, BR->TR
            { 0, 0, 2, 2 }, // Frustum NE/SW: TL->TL, BR->BR
            { 3, 3, 1, 1 }  // Frustum NW/SE: BL->BL, TR->TR
        };

        int bestIdx = -1;
        int bestHits = 999;
        double bestLen = 1e30;
        for (int p = 0; p < 6; ++p) {
            const QLineF s1(rC[kPairs[p].r1], iC[kPairs[p].i1]);
            const QLineF s2(rC[kPairs[p].r2], iC[kPairs[p].i2]);
            if (linesCrossBounded(s1, s2))
                continue;
            const int hits = (segmentHitsRectInterior(s1, roiBox) ? 1 : 0)
                    + (segmentHitsRectInterior(s1, insetFrame) ? 1 : 0)
                    + (segmentHitsRectInterior(s2, roiBox) ? 1 : 0)
                    + (segmentHitsRectInterior(s2, insetFrame) ? 1 : 0);
            const double totalLen = s1.length() + s2.length();
            if (hits < bestHits || (hits == bestHits && totalLen < bestLen)) {
                bestHits = hits;
                bestLen = totalLen;
                bestIdx = p;
            }
        }

        if (bestIdx >= 0) {
            painter->save();
            QPainterPath leaderClip;
            const QRectF unionBound = srcCanvasRectF.united(insetOuterGeo).adjusted(-8, -8, 8, 8);
            leaderClip.addRect(unionBound);
            QPainterPath innerInsetMask;
            innerInsetMask.addRect(insetFrame.adjusted(0.5, 0.5, -0.5, -0.5));
            painter->setClipPath(leaderClip.subtracted(innerInsetMask));

            QPen leaderPen(QColor(51, 65, 85, 205), 1.0, Qt::DashLine, Qt::FlatCap);
            leaderPen.setDashPattern({ 4.0, 3.0 });
            painter->setPen(leaderPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawLine(QLineF(rC[kPairs[bestIdx].r1], iC[kPairs[bestIdx].i1]));
            painter->drawLine(QLineF(rC[kPairs[bestIdx].r2], iC[kPairs[bestIdx].i2]));
            painter->restore();
        }
    }

    painter->restore();
}
