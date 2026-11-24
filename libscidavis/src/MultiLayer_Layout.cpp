/***************************************************************************
    File                 : MultiLayer_Layout.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Multi layer widget (layout, page setup, zoom, linked/inset layers, status pill)

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

namespace {
int computeOutsidePanelTopExtra(const QList<Graph *> &primaryGraphs, bool panelLabelsEnabled,
                                MultiLayer::PanelLabelPlacement placement, int panelLabelFontPt)
{
    if (!panelLabelsEnabled || placement != MultiLayer::PanelOutsideTopLeft
        || primaryGraphs.isEmpty()) {
        return 0;
    }
    double maxLabelPt = (panelLabelFontPt > 0) ? double(panelLabelFontPt) : 12.0;
    if (panelLabelFontPt <= 0) {
        for (Graph *gr : primaryGraphs) {
            if (!gr)
                continue;
            const QFont tf = gr->axisTitleFont(QwtPlot::yLeft);
            const double titlePt =
                    (tf.pointSizeF() > 0.0) ? tf.pointSizeF() : double(tf.pointSize());
            const double pt = (titlePt > 0.0)
                    ? qBound(9.0, titlePt >= 24.0 ? titlePt : titlePt + 1.0, 40.0)
                    : 12.0;
            maxLabelPt = qMax(maxLabelPt, pt);
        }
    }
    QFont lblFont(QStringLiteral("Arial"));
    lblFont.setPointSizeF(maxLabelPt);
    lblFont.setWeight(QFont::Normal);
    lblFont.setItalic(false);
    lblFont.setHintingPreference(QFont::PreferNoHinting);
    if (Plot *firstPlot = primaryGraphs.first() ? primaryGraphs.first()->plotWidget() : nullptr)
        lblFont = firstPlot->displayFont(lblFont);
    const QFontMetrics lfm(lblFont);
    return qMax(22, lfm.height() + 12);
}
} // namespace

namespace MultiLayerInternal {

void syncMultiLayerPageHost(MultiLayer *ml, QWidget *mainWidget, QWidget *canvas,
                            MultiLayer::PageUnit pageUnit, bool pageViewMode,
                            const QSize &basePagePx, double pageZoom, const QColor &pageBgColor)
{
    Q_UNUSED(ml);
    const QColor pageFill = pageBgColor.isValid() ? pageBgColor : QColor(Qt::white);
    if (canvas) {
        QPalette cpal = canvas->palette();
        cpal.setColor(QPalette::Window, pageFill);
        canvas->setPalette(cpal);
        canvas->setAutoFillBackground(true);
    }

    if (!mainWidget)
        return;

    QScrollArea *canvasScroll =
            mainWidget->findChild<QScrollArea *>(QStringLiteral("MultiLayerCanvasScroll"));
    QWidget *pageHost =
            mainWidget->findChild<QWidget *>(QStringLiteral("MultiLayerPageHost"));
    const bool freePixelMode =
            (pageUnit == MultiLayer::UnitPx && !pageViewMode && qAbs(pageZoom - 1.0) <= 1e-4);

    if (!canvasScroll || !pageHost) {
        QPalette wpal = mainWidget->palette();
        wpal.setColor(QPalette::Window,
                      freePixelMode ? pageFill : QColor(QStringLiteral("#EAEEF3")));
        mainWidget->setPalette(wpal);
        return;
    }

    if (freePixelMode) {
        canvasScroll->setWidgetResizable(true);
        canvasScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        canvasScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QPalette hpal = pageHost->palette();
        hpal.setColor(QPalette::Window, pageFill);
        pageHost->setPalette(hpal);
        pageHost->setAutoFillBackground(true);

        if (QWidget *vp = canvasScroll->viewport()) {
            QPalette vpal = vp->palette();
            vpal.setColor(QPalette::Window, pageFill);
            vp->setPalette(vpal);
            vp->setAutoFillBackground(true);
        }
        QPalette wpal = mainWidget->palette();
        wpal.setColor(QPalette::Window, pageFill);
        mainWidget->setPalette(wpal);

        pageHost->setMinimumSize(0, 0);
        pageHost->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

        if (QWidget *vp = canvasScroll->viewport()) {
            if (canvasScroll->isVisible() && vp->width() >= 80 && vp->height() >= 80
                && pageHost->size() != vp->size()) {
                pageHost->resize(vp->size());
            }
        }
        if ((pageHost->width() < 80 || pageHost->height() < 80) && basePagePx.width() >= 80
            && basePagePx.height() >= 80) {
            pageHost->resize(basePagePx);
        }
        if (canvas && !pageHost->rect().isEmpty())
            canvas->setGeometry(pageHost->rect());
        pageHost->update();
    } else {
        const QSize effBase =
                (basePagePx.isValid() && basePagePx.width() >= 50 && basePagePx.height() >= 50)
                ? basePagePx
                : QSize(520, 520);
        const QSize targetCanvasSize(qMax(80, qRound(effBase.width() * pageZoom)),
                                     qMax(80, qRound(effBase.height() * pageZoom)));
        const int pad = 8;
        const QColor deskBg(QStringLiteral("#EAEEF3"));

        canvasScroll->setWidgetResizable(false);
        canvasScroll->setAlignment(Qt::AlignCenter);
        canvasScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        canvasScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        QPalette hpal = pageHost->palette();
        hpal.setColor(QPalette::Window, deskBg);
        pageHost->setPalette(hpal);
        pageHost->setAutoFillBackground(true);

        if (QWidget *vp = canvasScroll->viewport()) {
            QPalette vpal = vp->palette();
            vpal.setColor(QPalette::Window, deskBg);
            vp->setPalette(vpal);
            vp->setAutoFillBackground(true);
        }
        QPalette wpal = mainWidget->palette();
        wpal.setColor(QPalette::Window, deskBg);
        mainWidget->setPalette(wpal);

        pageHost->setFixedSize(targetCanvasSize.width() + 2 * pad,
                               targetCanvasSize.height() + 2 * pad);
        if (canvas)
            canvas->setGeometry(pad, pad, targetCanvasSize.width(), targetCanvasSize.height());
        pageHost->update();
    }

    if (canvas) {
        if (QWidget *ov = pageHost->findChild<QWidget *>(
                    QStringLiteral("MultiLayerPanelLabelOverlay"))) {
            ov->setGeometry(canvas->geometry());
        }
        if (QWidget *dz = pageHost->findChild<QWidget *>(
                    QStringLiteral("MultiLayerDropZoneOverlay"))) {
            dz->setGeometry(canvas->geometry());
        }
    }
}

void resizeMultiLayerSubWindowForCanvas(MultiLayer *ml, QWidget *mainW, QFrame *pillFrame, int cw,
                                        int ch, bool freePixelMode)
{
    if (!ml || ml->isMaximized() || ml->isMinimized())
        return;
    ApplicationWindow *app = findMultiLayerApp(ml);
    const bool isSplit = app && app->modernRibbonBar()
            && app->modernRibbonBar()->workspaceLayoutMode()
                    == ModernRibbonBar::LayoutSplitTableGraph;
    const bool isTabbed = (ml->mdiArea() && ml->mdiArea()->viewMode() == QMdiArea::TabbedView);
    if (isSplit || isTabbed)
        return;
    const int hostPad = freePixelMode ? 0 : 16;
    const int topBarH = (pillFrame && !pillFrame->isHidden())
            ? qMax(LayerButton::btnSize() + 4, pillFrame->sizeHint().height() + 6)
            : (LayerButton::btnSize() + 4);
    const int styleFrameW = ml->mdiArea()
            ? qMax(4, ml->style()->pixelMetric(QStyle::PM_MdiSubWindowFrameWidth, nullptr, ml))
            : 0;
    const int styleTitleH = ml->mdiArea()
            ? qMax(24, ml->style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, ml))
            : 0;
    const int frameDW = (mainW && ml->width() > mainW->width() && mainW->width() >= 60)
            ? qBound(0, ml->width() - mainW->width(), 32)
            : (ml->mdiArea() ? qBound(8, 2 * styleFrameW + 4, 32) : 0);
    const int frameDH = (mainW && ml->height() > mainW->height() && mainW->height() >= 60)
            ? qBound(0, ml->height() - mainW->height(), 56)
            : (ml->mdiArea() ? qBound(28, styleTitleH + 2 * styleFrameW + 4, 56) : 0);
    int wantW = cw + hostPad + frameDW;
    int wantH = ch + hostPad + topBarH + frameDH;
    if (ml->mdiArea() && ml->mdiArea()->viewport() && ml->mdiArea()->viewport()->isVisible()
        && ml->mdiArea()->viewport()->width() >= 160
        && ml->mdiArea()->viewport()->height() >= 160) {
        const int vpW = ml->mdiArea()->viewport()->width();
        const int vpH = ml->mdiArea()->viewport()->height();
        wantW = qMin(wantW, vpW);
        wantH = qMin(wantH, vpH);
        const int nx = qMax(0, qMin(ml->x(), vpW - wantW));
        const int ny = qMax(0, qMin(ml->y(), vpH - wantH));
        if (nx != ml->x() || ny != ml->y())
            ml->move(nx, ny);
    }
    ml->resize(wantW, wantH);
}

void clampGraphCalloutMarkersInCanvas(Graph *gr)
{
    if (!gr || gr->isPiePlot() || gr->isTernaryPlot())
        return;
    Plot *plot = gr->plotWidget();
    if (!plot || !plot->canvas())
        return;

    plot->updateAxes();
    plot->updateLayout();
    const QRect layoutCRect = plot->plotLayout()->canvasRect();
    const int cw = (plot->canvas()->width() > 20) ? plot->canvas()->width() : layoutCRect.width();
    const int ch = (plot->canvas()->height() > 20) ? plot->canvas()->height() : layoutCRect.height();
    if (cw < 40 || ch < 40)
        return;

    const QRect cRect(0, 0, cw, ch);
    const QRect baseSafe = cRect.adjusted(6, 6, -6, -6);
    if (baseSafe.width() < 20 || baseSafe.height() < 20)
        return;

    Legend *mainLeg = gr->legend();
    const QVector<int> txtKeys = gr->textMarkerKeys();
    bool changedAny = false;

    for (int key : txtKeys) {
        Legend *mrk = gr->textMarker(key);
        if (!mrk || !mrk->isVisible() || mrk == mainLeg)
            continue;
        const QString rawTxt = mrk->text();
        if (rawTxt.trimmed().isEmpty()
            || rawTxt.contains(QLatin1String("\\c{"))
            || rawTxt.contains(QLatin1String("\\p{"))
            || rawTxt.contains(QLatin1String("\\l("))) {
            continue;
        }

        QRect mr = mrk->rect();
        if (mr.isEmpty())
            continue;
        const double zoomScale = qMax(1.0, plot->displayFontScale());
        const int edgeTol = qRound(30.0 * zoomScale);
        const int intersectTol = qRound(12.0 * zoomScale);
        if (!cRect.adjusted(-edgeTol, -edgeTol, edgeTol, edgeTol).contains(mr.topLeft())
            && !mr.intersects(
                    cRect.adjusted(-intersectTol, -intersectTol, intersectTol, intersectTol))) {
            continue;
        }

        QFont fnt = mrk->font();
        double pt = (fnt.pointSizeF() > 0.0) ? fnt.pointSizeF() : double(qMax(8, fnt.pointSize()));
        const double minPt = (cw < 240) ? 6.5 : 8.0;
        const int maxAllowedW = qMax(30, qRound(0.88 * double(cRect.width())));
        const int maxAllowedH = qMax(24, qRound(0.85 * double(cRect.height())));
        int guard = 0;
        while ((mr.width() > maxAllowedW || mr.height() > maxAllowedH) && pt > minPt && guard < 24) {
            pt = qMax(minPt, pt - (pt > 14.0 ? 1.5 : 0.5));
            fnt.setPointSizeF(pt);
            fnt.setFamily(QStringLiteral("Arial"));
            fnt.setBold(false);
            fnt.setHintingPreference(QFont::PreferNoHinting);
            mrk->setFont(fnt);
            mr = mrk->rect();
            changedAny = true;
            ++guard;
        }

        const double zoomScaleFrame = qMax(1.0, plot->displayFontScale());
        const int desiredFramePad = qMax(
                qRound(12.0 * zoomScaleFrame),
                qRound((double(plot->majorTickLength()) + 5.0) * zoomScaleFrame));
        const int fallbackPad = qRound(6.0 * zoomScaleFrame);
        const int framePad = (mrk->frameStyle() != Legend::None
                              && mr.width() + 2 * desiredFramePad <= cRect.width()
                              && mr.height() + 2 * desiredFramePad <= cRect.height())
                ? desiredFramePad
                : fallbackPad;
        const QRect safeRect = cRect.adjusted(framePad, framePad, -framePad, -framePad);
        const int minX = safeRect.left();
        const int minY = safeRect.top();
        const int maxX = qMax(minX, safeRect.right() - mr.width());
        const int maxY = qMax(minY, safeRect.bottom() - mr.height());
        const int clampedX = qBound(minX, mr.left(), maxX);
        const int clampedY = qBound(minY, mr.top(), maxY);
        if (clampedX != mr.left() || clampedY != mr.top()) {
            const QwtScaleMap &xMap = plot->canvasMap(mrk->xAxis());
            const QwtScaleMap &yMap = plot->canvasMap(mrk->yAxis());
            mrk->setOriginCoord(xMap.invTransform(clampedX), yMap.invTransform(clampedY));
            changedAny = true;
        }
    }

    if (changedAny) {
        plot->updateLayout();
        plot->replot();
    }
}

} // namespace MultiLayerInternal

QSize MultiLayer::arrangeLayers(bool userSize)
{
    static thread_local QSet<const MultiLayer *> s_activeArrange;
    if (s_activeArrange.contains(this))
        return QSize(l_canvas_width, l_canvas_height);
    struct ScopedArrangeGuard {
        QSet<const MultiLayer *> &activeSet;
        const MultiLayer *self;
        ScopedArrangeGuard(QSet<const MultiLayer *> &s, const MultiLayer *m) : activeSet(s), self(m)
        {
            activeSet.insert(self);
        }
        ~ScopedArrangeGuard() { activeSet.remove(self); }
    } arrangeGuard(s_activeArrange, this);

    if (isVisible() && widget() && widget()->layout())
        widget()->layout()->activate();
    if (d_page_unit != UnitPx || d_page_view_mode || qAbs(d_page_zoom - 1.0) > 1e-4) {
        syncMultiLayerPageHost(this, widget(), canvas, d_page_unit, d_page_view_mode,
                               d_base_page_px, d_page_zoom, d_page_bg_color);
    }
    QRect rect = canvas ? canvas->rect() : QRect();
    if (rect.width() < 100 || rect.height() < 100) {
        rect = QRect(0, 0, qMax(200, width() - 16),
                     qMax(200, height() - LayerButton::btnSize() - 36));
    }

    QList<Graph *> primaryGraphs;
    primaryGraphs.reserve(graphs);
    for (int k = 0; k < graphs; ++k) {
        Graph *gr = (Graph *)graphsList.at(k);
        if (!gr)
            continue;
        if (gr->linkedParentLayer() || d_inset_specs.contains(gr))
            continue;
        primaryGraphs.append(gr);
    }
    if (primaryGraphs.isEmpty()) {
        syncChildOverlayAndInsetGeometries();
        return QSize(l_canvas_width, l_canvas_height);
    }

    const int nPrimary = primaryGraphs.size();
    const int nRows = qMax(1, rows);
    const int nCols = qMax(1, cols);

    QVector<int> topHeights(nPrimary, 0);
    QVector<int> bottomHeights(nPrimary, 0);
    QVector<int> leftWidths(nPrimary, 0);
    QVector<int> rightWidths(nPrimary, 0);
    QVector<int> reqTopHeights(nPrimary, 0);
    QVector<int> reqBottomHeights(nPrimary, 0);
    QVector<int> reqLeftWidths(nPrimary, 0);
    QVector<int> reqRightWidths(nPrimary, 0);
    QVector<int> maxTopH(nRows, 0);
    QVector<int> maxBottomH(nRows, 0);
    QVector<int> maxLeftW(nCols, 0);
    QVector<int> maxRightW(nCols, 0);

    const int panelTopExtra = computeOutsidePanelTopExtra(
            primaryGraphs, d_panel_labels_enabled, d_panel_label_placement, d_panel_label_font_pt);

    int i;
    for (i = 0; i < nPrimary; i++) {
        Graph *gr = primaryGraphs.at(i);
        QwtPlot *plot = gr->plotWidget();
        plot->updateLayout();
        QwtPlotLayout *plotLayout = plot->plotLayout();
        QRect cRect = plotLayout->canvasRect();

        const int topHeight = qMax(0, cRect.top());
        const int bottomHeight = qMax(0, plot->height() - (cRect.top() + cRect.height()));
        const int leftWidth = qMax(0, cRect.left());
        const int rightWidth = qMax(0, plot->width() - (cRect.left() + cRect.width()));

        topHeights[i] = topHeight;
        bottomHeights[i] = bottomHeight;
        leftWidths[i] = leftWidth;
        rightWidths[i] = rightWidth;

        int effTop = topHeight;
        int effBottom = bottomHeight;
        int effLeft = leftWidth;
        int effRight = rightWidth;
        for (int k = 0; k < graphsList.count(); ++k) {
            Graph *ch = qobject_cast<Graph *>(graphsList.at(k));
            if (!ch || ch->linkedParentLayer() != gr || !ch->plotWidget())
                continue;
            Plot *chPlot = ch->plotWidget();
            chPlot->updateLayout();
            const QRect chRect = chPlot->plotLayout()->canvasRect();
            effTop = qMax(effTop, qMax(0, chRect.top()));
            effBottom = qMax(effBottom,
                             qMax(0, chPlot->height() - (chRect.top() + chRect.height())));
            effLeft = qMax(effLeft, qMax(0, chRect.left()));
            effRight = qMax(effRight,
                            qMax(0, chPlot->width() - (chRect.left() + chRect.width())));
        }
        reqTopHeights[i] = effTop + panelTopExtra;
        reqBottomHeights[i] = effBottom;
        reqLeftWidths[i] = effLeft;
        reqRightWidths[i] = effRight;

        int row = i / nCols;
        if (row >= nRows)
            row = nRows - 1;
        int col = i % nCols;

        if (reqTopHeights[i] > maxTopH[row])
            maxTopH[row] = reqTopHeights[i];
        if (effBottom > maxBottomH[row])
            maxBottomH[row] = effBottom;
        if (effLeft > maxLeftW[col])
            maxLeftW[col] = effLeft;
        if (effRight > maxRightW[col])
            maxRightW[col] = effRight;
    }

    int sumMarginH = 0;
    for (i = 0; i < nRows; i++)
        sumMarginH += maxTopH[i] + maxBottomH[i];

    int sumMarginW = 0;
    for (i = 0; i < nCols; i++)
        sumMarginW += maxLeftW[i] + maxRightW[i];

    if (!userSize) {
        const double aspect = (d_keep_aspect_ratio && d_target_aspect_ratio > 0.0)
                ? d_target_aspect_ratio
                : ((l_canvas_width > 0 && l_canvas_height > 0)
                           ? (double)l_canvas_width / (double)l_canvas_height
                           : 1.0);
        int availW = (rect.width() - (nCols - 1) * colsSpace - right_margin - left_margin
                      - sumMarginW)
                / nCols;
        int availH = (rect.height() - (nRows - 1) * rowsSpace - top_margin - bottom_margin
                      - sumMarginH)
                / nRows;
        if (d_keep_aspect_ratio) {
            int fitW = availW;
            int fitH = qRound(fitW / aspect);
            if (fitH > availH) {
                fitH = availH;
                fitW = qRound(fitH * aspect);
            }
            l_canvas_width = qMax(50, fitW);
            l_canvas_height = qMax(50, fitH);
        } else {
            l_canvas_width = qMax(50, availW);
            l_canvas_height = qMax(50, availH);
        }
    }

    QSize size = QSize(l_canvas_width, l_canvas_height);

    QVector<int> colX(nCols, 0);
    int accumW = 0;
    for (i = 0; i < nCols; i++) {
        colX[i] = accumW;
        accumW += maxLeftW[i] + l_canvas_width + maxRightW[i];
    }

    QVector<int> rowY(nRows, 0);
    int accumH = 0;
    for (i = 0; i < nRows; i++) {
        rowY[i] = accumH;
        accumH += maxTopH[i] + l_canvas_height + maxBottomH[i];
    }

    const int totalW = accumW + (nCols - 1) * colsSpace + left_margin + right_margin;
    const int totalH = accumH + (nRows - 1) * rowsSpace + top_margin + bottom_margin;
    const int offsetX = userSize ? 0 : qMax(0, (rect.width() - totalW) / 2);
    const int offsetY = userSize ? 0 : qMax(0, (rect.height() - totalH) / 2);

    for (i = 0; i < nPrimary; i++) {
        int row = i / nCols;
        if (row >= nRows)
            row = nRows - 1;

        int col = i % nCols;

        const int w = l_canvas_width + leftWidths[i] + rightWidths[i];
        const int h = l_canvas_height + topHeights[i] + bottomHeights[i];

        int x = left_margin + offsetX + col * colsSpace + colX[col];
        if (hor_align == HCenter)
            x += maxLeftW[col] - leftWidths[i];
        else if (hor_align == Right)
            x += (maxLeftW[col] - leftWidths[i]) + (maxRightW[col] - rightWidths[i]);
        else
            x += qMax(0, reqLeftWidths[i] - leftWidths[i]);

        int y = top_margin + offsetY + row * rowsSpace + rowY[row];
        if (vert_align == VCenter)
            y += maxTopH[row] - topHeights[i];
        else if (vert_align == Bottom)
            y += (maxTopH[row] - topHeights[i]) + (maxBottomH[row] - bottomHeights[i]);
        else
            y += qMax(0, reqTopHeights[i] - topHeights[i]);

        Graph *gr = primaryGraphs.at(i);
        bool autoscaleFonts = false;
        if (!userSize) {
            autoscaleFonts = gr->autoscaleFonts();
            gr->setAutoscaleFonts(false);
        }

        gr->setGeometry(QRect(x, y, w, h));
        gr->plotWidget()->resize(QSize(w, h));
        gr->plotWidget()->updateLayout();

        if (!userSize)
            gr->setAutoscaleFonts(autoscaleFonts);
    }

    syncChildOverlayAndInsetGeometries();
    return size;
}

void MultiLayer::findBestLayout(int &rows, int &cols)
{
    int NumGraph = 0;
    for (int k = 0; k < graphs; ++k) {
        Graph *gr = (Graph *)graphsList.at(k);
        if (gr && !gr->linkedParentLayer() && !d_inset_specs.contains(gr))
            ++NumGraph;
    }
    if (NumGraph <= 0)
        NumGraph = qMax(1, graphs);
    if (NumGraph % 2 == 0) // NumGraph is an even number
    {
        if (NumGraph <= 2)
            cols = NumGraph / 2 + 1;
        else if (NumGraph > 2)
            cols = NumGraph / 2;

        if (NumGraph < 8)
            rows = NumGraph / 4 + 1;
        if (NumGraph >= 8)
            rows = NumGraph / 4;
    } else if (NumGraph % 2 != 0) // NumGraph is an odd number
    {
        int Num = NumGraph + 1;

        if (Num <= 2)
            cols = 1;
        else if (Num > 2)
            cols = Num / 2;

        if (Num < 8)
            rows = Num / 4 + 1;
        if (Num >= 8)
            rows = Num / 4;
    }
}

void MultiLayer::arrangeLayers(bool fit, bool userSize)
{
    if (!graphs)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (d_layers_selector)
        clearLayerSelector();

    if (fit)
        findBestLayout(rows, cols);

    // the canvas sizes of all layers become equal only after several
    // resize iterations, due to the way Qwt handles the plot layout
    int iterations = 0;
    QSize size = arrangeLayers(userSize);
    QSize canvas_size = QSize(1, 1);
    while ((canvas_size != size || iterations < 1) && iterations < 10) {
        iterations++;
        canvas_size = size;
        size = arrangeLayers(userSize);
    }

    if (userSize) { // resize window
        bool ignoreResize = active_graph ? active_graph->ignoresResizeEvents() : false;
        for (int i = 0; i < (int)graphsList.count(); i++) {
            Graph *gr = (Graph *)graphsList.at(i);
            if (gr)
                gr->setIgnoreResizeEvents(true);
        }

        this->showNormal();
        QSize size = canvas->childrenRect().size();
        const bool freePx =
                (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4);
        resizeMultiLayerSubWindowForCanvas(this, widget(), d_layer_pill_frame,
                                           size.width() + right_margin,
                                           size.height() + bottom_margin, freePx);

        for (int i = 0; i < (int)graphsList.count(); i++) {
            Graph *gr = (Graph *)graphsList.at(i);
            if (gr)
                gr->setIgnoreResizeEvents(ignoreResize);
        }
    }

    emit modifiedPlot();
    QApplication::restoreOverrideCursor();
}

void MultiLayer::setCols(int c)
{
    if (cols != c)
        cols = c;
}

void MultiLayer::setRows(int r)
{
    if (rows != r)
        rows = r;
}

void MultiLayer::setMargins(int lm, int rm, int tm, int bm)
{
    if (left_margin != lm)
        left_margin = lm;
    if (right_margin != rm)
        right_margin = rm;
    if (top_margin != tm)
        top_margin = tm;
    if (bottom_margin != bm)
        bottom_margin = bm;
}

void MultiLayer::setSpacing(int rgap, int cgap)
{
    if (rowsSpace != rgap)
        rowsSpace = rgap;
    if (colsSpace != cgap)
        colsSpace = cgap;
}

void MultiLayer::setLayerCanvasSize(int w, int h)
{
    if (l_canvas_width != w)
        l_canvas_width = w;
    if (l_canvas_height != h)
        l_canvas_height = h;
    if (w > 0 && h > 0)
        d_target_aspect_ratio = (double)w / (double)h;
}

void MultiLayer::setKeepAspectRatio(bool keep)
{
    if (d_keep_aspect_ratio == keep)
        return;
    d_keep_aspect_ratio = keep;
    if (d_keep_aspect_ratio && d_target_aspect_ratio <= 0.05) {
        if (l_canvas_width > 0 && l_canvas_height > 0)
            d_target_aspect_ratio = (double)l_canvas_width / (double)l_canvas_height;
        else
            d_target_aspect_ratio = 1.0;
    }
    if (graphs > 0)
        arrangeLayers(false, false);
    updateCanvasStatusPill();
    if (ApplicationWindow *app = findMultiLayerApp(this)) {
        if (ModernRibbonBar *rb = app->modernRibbonBar())
            rb->syncCheckableStates();
        if (GraphInspectorDock *dock = app->graphInspectorDock()) {
            if (dock->targetMultiLayer() == this)
                dock->setTargetMultiLayer(this, true);
        }
    }
    emit modifiedPlot();
}

void MultiLayer::setTargetAspectRatio(double ratio)
{
    if (ratio <= 0.05 || ratio > 20.0)
        return;
    d_target_aspect_ratio = ratio;
    if (l_canvas_height <= 0)
        l_canvas_height = 360;
    l_canvas_width = qMax(50, qRound(l_canvas_height * d_target_aspect_ratio));
    if (graphs > 0) {
        const bool prevKeep = d_keep_aspect_ratio;
        d_keep_aspect_ratio = true;
        arrangeLayers(false, false);
        d_keep_aspect_ratio = prevKeep;
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
            if (gr && !gr->linkedParentLayer() && !d_inset_specs.contains(gr))
                clampGraphCalloutMarkersInCanvas(gr);
        }
    }
    updateCanvasStatusPill();
    if (ApplicationWindow *app = findMultiLayerApp(this)) {
        if (ModernRibbonBar *rb = app->modernRibbonBar())
            rb->syncCheckableStates();
        if (GraphInspectorDock *dock = app->graphInspectorDock()) {
            if (dock->targetMultiLayer() == this)
                dock->setTargetMultiLayer(this, true);
        }
    }
    emit modifiedPlot();
}

void MultiLayer::applyAspectRatioPreset(double ratio, bool keepLocked)
{
    if (ratio <= 0.05 || ratio > 20.0)
        return;
    d_keep_aspect_ratio = keepLocked;
    setTargetAspectRatio(ratio);
}

void MultiLayer::syncAspectRatioAfterManualResize(double newRatio, int canvasW, int canvasH,
                                                  bool unlockAspect)
{
    if (unlockAspect)
        d_keep_aspect_ratio = false;
    if (newRatio > 0.05 && newRatio <= 20.0)
        d_target_aspect_ratio = newRatio;
    if (canvasW >= 50 && canvasH >= 50) {
        l_canvas_width = canvasW;
        l_canvas_height = canvasH;
    }
    syncChildOverlayAndInsetGeometries();
    updateCanvasStatusPill();
    if (ApplicationWindow *app = findMultiLayerApp(this)) {
        if (ModernRibbonBar *rb = app->modernRibbonBar())
            rb->syncCheckableStates();
        if (GraphInspectorDock *dock = app->graphInspectorDock()) {
            if (dock->targetMultiLayer() == this)
                dock->setTargetMultiLayer(this, true);
        }
    }
    emit modifiedPlot();
}

void MultiLayer::setAlignement(int ha, int va)
{
    if (hor_align != ha)
        hor_align = ha;

    if (vert_align != va)
        vert_align = va;
}

// ============================================================================
// OriginPro Page View, Physical Page Sizing, Zoom, Linked Axes & Inset Layers
// ============================================================================

void MultiLayer::syncChildOverlayAndInsetGeometries()
{
    const bool prevSync = d_syncing_inset_geometries;
    d_syncing_inset_geometries = true;

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr)
            continue;
        if (Graph *parentLayer = gr->linkedParentLayer()) {
            Plot *pPlot = parentLayer->plotWidget();
            Plot *cPlot = gr->plotWidget();
            if (!pPlot || !cPlot)
                continue;
            pPlot->updateLayout();
            QRect pCanvas = pPlot->plotLayout()->canvasRect();
            if (pCanvas.width() >= 20 && pCanvas.height() >= 20) {
                QRect targetInner = pCanvas.translated(parentLayer->pos());
                if (!prevSync && !d_is_restoring_snapshot) {
                    cPlot->updateLayout();
                    const QRect cCanvas0 = cPlot->plotLayout()->canvasRect();
                    const int cL0 = qMax(0, cCanvas0.left());
                    const int cT0 = qMax(0, cCanvas0.top());
                    const int cR0 =
                            qMax(0, cPlot->width() - (cCanvas0.left() + cCanvas0.width()));
                    const int cB0 =
                            qMax(0, cPlot->height() - (cCanvas0.top() + cCanvas0.height()));
                    QRect pGeo = parentLayer->geometry();
                    int shiftTop = 0;
                    if (targetInner.top() - cT0 < 4
                        && pGeo.height() - (4 - (targetInner.top() - cT0)) >= 80) {
                        shiftTop = 4 - (targetInner.top() - cT0);
                    }
                    int shiftLeft = 0;
                    if (targetInner.left() - cL0 < 4
                        && pGeo.width() - (4 - (targetInner.left() - cL0)) >= 80) {
                        shiftLeft = 4 - (targetInner.left() - cL0);
                    }
                    int trimRight = 0;
                    int trimBottom = 0;
                    if (canvas && canvas->width() >= 140) {
                        const int overR =
                                (targetInner.left() + targetInner.width() + shiftLeft + cR0)
                                - (canvas->width() - 4);
                        if (overR > 0 && pGeo.width() - shiftLeft - overR >= 80)
                            trimRight = overR;
                    }
                    if (canvas && canvas->height() >= 140) {
                        const int overB =
                                (targetInner.top() + targetInner.height() + shiftTop + cB0)
                                - (canvas->height() - 4);
                        if (overB > 0 && pGeo.height() - shiftTop - overB >= 80)
                            trimBottom = overB;
                    }
                    if (shiftTop > 0 || shiftLeft > 0 || trimRight > 0 || trimBottom > 0) {
                        const bool prevPIgnore = parentLayer->ignoresResizeEvents();
                        const bool prevPAuto = parentLayer->autoscaleFonts();
                        parentLayer->setIgnoreResizeEvents(true);
                        parentLayer->setAutoscaleFonts(false);
                        pGeo = QRect(pGeo.x() + shiftLeft, pGeo.y() + shiftTop,
                                     pGeo.width() - shiftLeft - trimRight,
                                     pGeo.height() - shiftTop - trimBottom);
                        parentLayer->setGeometry(pGeo);
                        pPlot->resize(pGeo.size());
                        pPlot->updateLayout();
                        parentLayer->setAutoscaleFonts(prevPAuto);
                        parentLayer->setIgnoreResizeEvents(prevPIgnore);
                        pCanvas = pPlot->plotLayout()->canvasRect();
                        targetInner = pCanvas.translated(parentLayer->pos());
                    }
                }
                for (int pass = 0; pass < 2; ++pass) {
                    cPlot->updateLayout();
                    const QRect cCanvas = cPlot->plotLayout()->canvasRect();
                    const int cL = qMax(0, cCanvas.left());
                    const int cT = qMax(0, cCanvas.top());
                    const int cR = qMax(0, cPlot->width() - (cCanvas.left() + cCanvas.width()));
                    const int cB = qMax(0, cPlot->height() - (cCanvas.top() + cCanvas.height()));
                    const QRect targetGeo(targetInner.left() - cL, targetInner.top() - cT,
                                          qMax(40, targetInner.width() + cL + cR),
                                          qMax(40, targetInner.height() + cT + cB));
                    if (gr->geometry() != targetGeo) {
                        gr->setGeometry(targetGeo);
                        cPlot->resize(targetGeo.size());
                    }
                }
                cPlot->updateLayout();
            } else {
                const QRect pGeo = parentLayer->geometry();
                if (gr->geometry() != pGeo) {
                    gr->setGeometry(pGeo);
                    cPlot->resize(pGeo.size());
                    cPlot->updateLayout();
                }
            }
            installLayerEventFilters(gr);
        } else if (d_inset_specs.contains(gr)) {
            Graph *srcLayer = d_inset_specs[gr].sourceLayer.data();
            const QRectF rel = d_inset_specs[gr].normGeo;
            if (srcLayer) {
                const QRect srcGeo = srcLayer->geometry();
                int iw = qMax(40, qRound(rel.width() * srcGeo.width()));
                int ih = qMax(40, qRound(rel.height() * srcGeo.height()));
                int ix = srcGeo.x() + qRound(rel.x() * srcGeo.width());
                int iy = srcGeo.y() + qRound(rel.y() * srcGeo.height());
                const QRect pageBounds =
                        (canvas && canvas->width() >= 60 && canvas->height() >= 60)
                        ? canvas->rect().adjusted(4, 4, -4, -4)
                        : srcGeo;
                if (pageBounds.width() > 60 && pageBounds.height() > 60) {
                    iw = qMin(iw, qMax(60, pageBounds.width() - 8));
                    ih = qMin(ih, qMax(50, pageBounds.height() - 8));
                    ix = qBound(pageBounds.left(), ix,
                                qMax(pageBounds.left(), pageBounds.right() - iw + 1));
                    iy = qBound(pageBounds.top(), iy,
                                qMax(pageBounds.top(), pageBounds.bottom() - ih + 1));
                }
                const QRect targetGeo(ix, iy, iw, ih);
                if (gr->geometry() != targetGeo) {
                    gr->setGeometry(targetGeo);
                    if (gr->plotWidget()) {
                        gr->plotWidget()->resize(targetGeo.size());
                        gr->plotWidget()->updateLayout();
                    }
                }
                installLayerEventFilters(gr);
            }
        }
    }

    d_syncing_inset_geometries = prevSync;
    enforceLayerZOrder();
    syncCanvasOverlayVisibility();
}

void MultiLayer::openLayerButtonContextMenu(LayerButton *btn, const QPoint &globalPos)
{
    if (btn)
        activateGraph(btn);

    QMenu menu(this);
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("layer-arrange"), 18, false),
                   tr("Layer Properties..."), this, [this]() { emit showPlotDialog(-1); });
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("layer-linked"), 18, false),
                   tr("Link Axes Scales..."), this, [this]() {
        LayerDialog dlg(this);
        dlg.setMultiLayer(this);
        dlg.exec();
        refreshLayerButtonBadges();
    });
    menu.addSeparator();
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("plot-double-y"), 18, false),
                   tr("Enable Right-Y Axis on Active Layer (Double-Y)"), this, [this]() {
        Graph *g = activeGraph() ? activeGraph() : layer(1);
        if (!g || !g->plotWidget())
            return;
        const QString snap = captureUndoSnapshot();
        g->enableAxis(QwtPlot::yRight, true);
        g->setAxisBackboneVisible(QwtPlot::yRight, true);
        if (QwtScaleDraw *sd = g->plotWidget()->axisScaleDraw(QwtPlot::yRight))
            sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        g->plotWidget()->setMajorTicksType(QwtPlot::yRight, ScaleDraw::In);
        g->plotWidget()->setMinorTicksType(QwtPlot::yRight, ScaleDraw::In);
        if (g->axisTitle(QwtPlot::yRight).trimmed().isEmpty()) {
            g->setAxisTitleAuto(QwtPlot::yRight, true);
            g->refreshAutoAxisTitlesFromColumns();
        }
        if (d_page_unit != UnitPx && d_keep_aspect_ratio
            && qFuzzyCompare(d_target_aspect_ratio, 1.0)) {
            applySquareCanvasFrame70Percent();
        } else {
            g->plotWidget()->updateLayout();
            g->plotWidget()->replot();
        }
        refreshLayerButtonBadges();
        emit modifiedPlot();
        pushUndoSnapshot(tr("Enable Right-Y Axis"), snap);
    });
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("layer-linked"), 18, false),
                   tr("Add Linked Overlay Layer (Twin-Axis)"), this, [this]() {
        const QString snap = captureUndoSnapshot();
        Graph *g = addLinkedLayer(activeGraph(), true, false, QString("x"), QString("y"));
        if (g && g->plotWidget()) {
            g->enableAxis(QwtPlot::yRight, true);
            g->enableAxis(QwtPlot::xTop, false);
            g->setAxisBackboneVisible(QwtPlot::xTop, false);
            g->setAxisBackboneVisible(QwtPlot::yRight, true);
            if (QwtScaleDraw *sd = g->plotWidget()->axisScaleDraw(QwtPlot::yRight))
                sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
            g->plotWidget()->setMajorTicksType(QwtPlot::yRight, ScaleDraw::In);
            g->plotWidget()->setMinorTicksType(QwtPlot::yRight, ScaleDraw::In);
            g->plotWidget()->replot();
            setActiveGraph(g);
        }
        refreshLayerButtonBadges();
        pushUndoSnapshot(tr("Add Linked Layer"), snap);
    });
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("layer-linked"), 18, false),
                   tr("Add Linked Top-X / Right-Y Layer"), this, [this]() {
        const QString snap = captureUndoSnapshot();
        Graph *g = addLinkedLayer(activeGraph(), true, true, QString("x"), QString("y"));
        if (g)
            setActiveGraph(g);
        refreshLayerButtonBadges();
        pushUndoSnapshot(tr("Add Linked Layer"), snap);
    });
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("layer-inset"), 18, false),
                   tr("Add Zoom Inset Layer"), this, [this]() {
        const QString snap = captureUndoSnapshot();
        addInsetLayer(activeGraph());
        refreshLayerButtonBadges();
        pushUndoSnapshot(tr("Add Inset Layer"), snap);
    });
    Graph *actGr = activeGraph();
    if (actGr && d_inset_specs.contains(actGr)) {
        menu.addSeparator();
        QAction *actConn = menu.addAction(tr("Show Zoom Connectors"));
        actConn->setCheckable(true);
        actConn->setChecked(insetShowConnectors(actGr));
        QPointer<Graph> safeAct(actGr);
        connect(actConn, &QAction::toggled, this, [this, safeAct](bool checked) {
            if (!safeAct || !d_inset_specs.contains(safeAct.data()))
                return;
            const QString snap = captureUndoSnapshot();
            setInsetShowConnectors(safeAct.data(), checked);
            pushUndoSnapshot(tr("Toggle Zoom Connectors"), snap);
        });
    }
    menu.addSeparator();
    QAction *actRemove = menu.addAction(
            IconLoader::duotoneIcon(QStringLiteral("delete"), 18, false),
            tr("Remove Layer"), this, [this]() {
                const QString snap = captureUndoSnapshot();
                const int prevGraphs = graphs;
                if (graphs > 1)
                    confirmRemoveLayer();
                else if (graphs == 1)
                    removeLayer();
                refreshLayerButtonBadges();
                if (graphs != prevGraphs && graphs > 0)
                    pushUndoSnapshot(tr("Remove Layer"), snap);
            });
    actRemove->setEnabled(graphs > 0);
    menu.exec(globalPos);
}

void MultiLayer::setPageViewMode(bool enabled)
{
    if (enabled && d_page_unit == UnitPx && canvas && canvas->width() >= 80
        && canvas->height() >= 80) {
        d_base_page_px = QSize(qMax(80, qRound(canvas->width() / d_page_zoom)),
                               qMax(80, qRound(canvas->height() / d_page_zoom)));
    }
    d_page_view_mode = enabled;
    setPageBackgroundColor(d_page_view_mode && !d_page_bg_color.isValid() ? QColor("#FFFFFF")
                                                                          : d_page_bg_color);
}

bool MultiLayer::pageViewMode() const
{
    return d_page_view_mode;
}

void MultiLayer::setPageBackgroundColor(const QColor &color)
{
    d_page_bg_color = color.isValid() ? color : QColor("#FFFFFF");
    syncMultiLayerPageHost(this, widget(), canvas, d_page_unit, d_page_view_mode, d_base_page_px,
                           d_page_zoom, d_page_bg_color);
    if (canvas)
        canvas->update();
    repositionCanvasStatusPill();
    emit modifiedPlot();
}

void MultiLayer::setPageZoom(double factor)
{
    if (!d_internal_zoom_change)
        d_user_zoomed = true;
    clearCtrlFontGestureBaseline();

    const double clamped = qBound(0.25, factor, 4.0);
    if (qAbs(clamped - d_page_zoom) < 1e-6)
        return;

    if (!d_base_page_px.isValid() || d_base_page_px.width() < 50 || d_base_page_px.height() < 50) {
        const QSize cur = (canvas && canvas->width() >= 50 && canvas->height() >= 50)
                ? canvas->size()
                : QSize(graph_width, graph_height);
        d_base_page_px = QSize(qMax(80, qRound(cur.width() / d_page_zoom)),
                               qMax(80, qRound(cur.height() / d_page_zoom)));
    }

    const double prevZoom = d_page_zoom;
    d_page_zoom = clamped;

    const QSize newCanvasSize(qMax(80, qRound(d_base_page_px.width() * d_page_zoom)),
                              qMax(80, qRound(d_base_page_px.height() * d_page_zoom)));

    const bool prevSync = d_syncing_inset_geometries;
    d_syncing_inset_geometries = true;

    QVector<bool> savedIgnore(graphsList.count(), false);
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr) {
            savedIgnore[i] = gr->ignoresResizeEvents();
            gr->setIgnoreResizeEvents(true);
        }
    }

    lastSize = newCanvasSize;
    const bool prevSuppressResizeLayout = d_suppress_canvas_resize_layout;
    if (graphs > 0)
        d_suppress_canvas_resize_layout = true;
    syncMultiLayerPageHost(this, widget(), canvas, d_page_unit, d_page_view_mode, d_base_page_px,
                           d_page_zoom, d_page_bg_color);
    const bool freePx =
            (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4);
    resizeMultiLayerSubWindowForCanvas(this, widget(), d_layer_pill_frame, newCanvasSize.width(),
                                       newCanvasSize.height(), freePx);
    d_suppress_canvas_resize_layout = prevSuppressResizeLayout;

    // Every zoom is computed from the layer rectangles at 100 % (kept as doubles), never from the
    // previous zoomed rectangle: chaining qRound(x * ratio) shifted layers by a pixel per zoom
    // step (and per export, which unzooms to 100 % and back), and the 60 px minimum of a small
    // inset at 50 % stuck (100x80 -> 60x60 -> 120x120). A layer whose rectangle is no longer the
    // one the last zoom gave it (moved, resized, added, loaded, undone) takes its 100 % rectangle
    // from where it is now.
    auto baseRect = [prevZoom](const QRect &r) {
        return QRectF(r.x() / prevZoom, r.y() / prevZoom, r.width() / prevZoom, r.height() / prevZoom);
    };
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr)
            continue;
        const QRect gRect = gr->geometry();
        const QVariant appliedV = gr->property(kZoomAppliedGeomProp);
        const QVariant baseV = gr->property(kZoomBaseGeomProp);
        QRectF base = baseRect(gRect);
        if (appliedV.isValid() && baseV.isValid()) {
            const QRect applied = appliedV.toRect();
            if (applied == gRect) {
                base = baseV.toRectF();
            } else if (applied.size() == gRect.size()) {
                // Only moved: keep the 100 % size (the view may show it at the 60 px minimum).
                base.setSize(baseV.toRectF().size());
            }
        }
        gr->setProperty(kZoomBaseGeomProp, base);
        // The 60 px minimum is for the view only; the 100 % rectangle keeps its own size.
        const QRect scaledRect(qRound(base.x() * d_page_zoom), qRound(base.y() * d_page_zoom),
                               qMax(60, qRound(base.width() * d_page_zoom)),
                               qMax(60, qRound(base.height() * d_page_zoom)));
        gr->setGeometry(scaledRect);
        gr->plotWidget()->resize(scaledRect.size());
        // The text grows with the page like under a magnifier; the stored pt stay as they are.
        gr->plotWidget()->setDisplayFontScale(d_page_zoom);
        gr->plotWidget()->updateAxes();
        gr->plotWidget()->replot();
        gr->setIgnoreResizeEvents(savedIgnore[i]);
    }
    {
        const QSize cur(l_canvas_width, l_canvas_height);
        if (!(d_zoom_applied_lcanvas == cur && d_zoom_base_lcanvas.isValid()))
            d_zoom_base_lcanvas = QSizeF(cur.width() / prevZoom, cur.height() / prevZoom);
        l_canvas_width = qMax(50, qRound(d_zoom_base_lcanvas.width() * d_page_zoom));
        l_canvas_height = qMax(50, qRound(d_zoom_base_lcanvas.height() * d_page_zoom));
        d_zoom_applied_lcanvas = QSize(l_canvas_width, l_canvas_height);
    }
    d_syncing_inset_geometries = prevSync;

    syncChildOverlayAndInsetGeometries();
    // What this zoom left each layer at (after the overlay / inset sync), to recognise later
    // whether the layer was moved since.
    // The markers' pixel caches follow the new canvas (their axis coordinates stay).
    for (int i = 0; i < graphsList.count(); ++i) {
        if (Graph *gr = (Graph *)graphsList.at(i)) {
            gr->setProperty(kZoomAppliedGeomProp, gr->geometry());
            if (Plot *pl = gr->plotWidget()) {
                // A hidden window gets no resize events: the plot of an inset moved by the sync
                // above may still have its old size.
                if (pl->size() != gr->size())
                    pl->resize(gr->size());
                pl->updateLayout();
                gr->refreshMarkerPixelCaches();
            }
        }
    }
    lastSize = canvas ? canvas->size() : newCanvasSize;
    repositionCanvasStatusPill();
    updateCanvasStatusPill();
    syncCanvasOverlayVisibility();
    emit pageZoomChanged(d_page_zoom);
}

double MultiLayer::pageZoom() const
{
    return d_page_zoom;
}

double MultiLayer::fitPageZoomFactor() const
{
    double basePxW = 0.0;
    double basePxH = 0.0;
    if (d_page_unit != UnitPx && d_page_width_val > 0.0 && d_page_height_val > 0.0) {
        auto toPx96 = [](double val, PageUnit u) -> double {
            switch (u) {
            case UnitMm:
                return (val / 25.4) * 96.0;
            case UnitCm:
                return (val / 2.54) * 96.0;
            case UnitInch:
                return val * 96.0;
            case UnitPx:
            default:
                return val;
            }
        };
        basePxW = std::max(120.0, toPx96(d_page_width_val, d_page_unit));
        basePxH = std::max(120.0, toPx96(d_page_height_val, d_page_unit));
    } else if (d_base_page_px.isValid() && d_base_page_px.width() >= 50
               && d_base_page_px.height() >= 50) {
        basePxW = double(d_base_page_px.width());
        basePxH = double(d_base_page_px.height());
    } else {
        const double z = qMax(0.25, d_page_zoom);
        const int cw = (canvas && canvas->width() >= 50) ? qRound(canvas->width() / z) : graph_width;
        const int ch = (canvas && canvas->height() >= 50) ? qRound(canvas->height() / z)
                                                          : graph_height;
        basePxW = std::max(120.0, double(cw));
        basePxH = std::max(120.0, double(ch));
    }

    ApplicationWindow *app = findMultiLayerApp(const_cast<MultiLayer *>(this));
    QMdiArea *mdi = mdiArea();
    const bool splitMode = app && app->modernRibbonBar()
            && app->modernRibbonBar()->workspaceLayoutMode()
                    == ModernRibbonBar::LayoutSplitTableGraph;
    const bool tabbedMode = (mdi && mdi->viewMode() == QMdiArea::TabbedView);
    const bool constrainedPane = isMaximized() || tabbedMode || splitMode || !mdi;

    int availW = 0;
    int availH = 0;
    QScrollArea *cs = widget()
            ? widget()->findChild<QScrollArea *>(QStringLiteral("MultiLayerCanvasScroll"))
            : nullptr;
    QWidget *csVp = cs ? cs->viewport() : nullptr;

    const int topBarH = (d_layer_pill_frame && !d_layer_pill_frame->isHidden())
            ? qMax(LayerButton::btnSize() + 4, d_layer_pill_frame->sizeHint().height() + 6)
            : (LayerButton::btnSize() + 4);
    const int styleFrameW = mdi
            ? qMax(4, style()->pixelMetric(QStyle::PM_MdiSubWindowFrameWidth, nullptr, this))
            : 0;
    const int styleTitleH = mdi
            ? qMax(24, style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, this))
            : 0;

    if (constrainedPane) {
        if (csVp && csVp->isVisible() && csVp->width() >= 80 && csVp->height() >= 80) {
            availW = csVp->width() - 28;
            availH = csVp->height() - 28;
        } else if (splitMode && mdi && mdi->viewport() && mdi->viewport()->isVisible()
                   && mdi->viewport()->width() >= 200 && mdi->viewport()->height() >= 120) {
            const double leftRatio =
                    qBound(0.15, app->modernRibbonBar()->splitLeftRatio(), 0.85);
            const int leftPaneW = qMax(120, qRound(mdi->viewport()->width() * leftRatio));
            const int rightPaneW = qMax(120, mdi->viewport()->width() - leftPaneW);
            const int paneH = mdi->viewport()->height();
            availW = rightPaneW - (2 * styleFrameW + 28);
            availH = paneH - (styleTitleH + 2 * styleFrameW + topBarH + 28);
        } else if (mdi && mdi->viewport() && mdi->viewport()->isVisible()
                   && mdi->viewport()->width() >= 160 && mdi->viewport()->height() >= 160) {
            availW = mdi->viewport()->width() - (tabbedMode ? 28 : (2 * styleFrameW + 28));
            availH = mdi->viewport()->height()
                    - (tabbedMode ? (topBarH + 28) : (styleTitleH + 2 * styleFrameW + topBarH + 28));
        }
    } else {
        if (mdi && mdi->viewport() && mdi->viewport()->isVisible()
            && mdi->viewport()->width() >= 160 && mdi->viewport()->height() >= 160) {
            const int vpW = mdi->viewport()->width();
            const int vpH = mdi->viewport()->height();
            availW = vpW - (2 * styleFrameW + 20);
            availH = vpH - (styleTitleH + 2 * styleFrameW + topBarH + 20);
        } else if (csVp && csVp->isVisible() && csVp->width() >= 80 && csVp->height() >= 80) {
            availW = csVp->width() - 28;
            availH = csVp->height() - 28;
        }
    }

    if (availW >= 60 && availH >= 60 && basePxW > 0.0 && basePxH > 0.0) {
        const double fitZoom = std::min(double(availW) / basePxW, double(availH) / basePxH);
        return qBound(0.35, fitZoom, 1.6);
    }

    if (d_page_unit != UnitPx) {
        double wMm = d_page_width_val;
        double hMm = d_page_height_val;
        if (d_page_unit == UnitCm) {
            wMm *= 10.0;
            hMm *= 10.0;
        } else if (d_page_unit == UnitInch) {
            wMm *= 25.4;
            hMm *= 25.4;
        }
        return (wMm <= 110.5 && hMm <= 110.5) ? 1.6 : 1.0;
    }
    return 1.0;
}

double MultiLayer::autoFitTargetZoom() const
{
    double desiredUpper = 1.0;
    if (d_page_unit != UnitPx) {
        double wMm = d_page_width_val;
        double hMm = d_page_height_val;
        if (d_page_unit == UnitCm) {
            wMm *= 10.0;
            hMm *= 10.0;
        } else if (d_page_unit == UnitInch) {
            wMm *= 25.4;
            hMm *= 25.4;
        }
        desiredUpper = (wMm <= 110.5 && hMm <= 110.5) ? 1.6 : 1.0;
    }
    return qBound(0.35, std::min(desiredUpper, fitPageZoomFactor()), 1.6);
}

void MultiLayer::autoFitPageZoomToViewport()
{
    if (d_user_zoomed || (d_page_unit == UnitPx && !d_page_view_mode))
        return;
    const double target = autoFitTargetZoom();
    ApplicationWindow *app = findMultiLayerApp(this);
    const bool splitMode = app && app->modernRibbonBar()
            && app->modernRibbonBar()->workspaceLayoutMode()
                    == ModernRibbonBar::LayoutSplitTableGraph;
    const bool tabbedMode = (mdiArea() && mdiArea()->viewMode() == QMdiArea::TabbedView);
    const bool keepFreeMdiGeo = property("_tg_has_saved_window_geo").toBool()
            && !isMaximized() && !tabbedMode && !splitMode;
    const QRect savedGeo = geometry();
    const bool prevInternal = d_internal_zoom_change;
    d_internal_zoom_change = true;
    setPageZoom(target);
    d_internal_zoom_change = prevInternal;
    if (keepFreeMdiGeo && savedGeo.width() >= 260 && savedGeo.height() >= 220
        && geometry() != savedGeo) {
        setGeometry(savedGeo);
    }
}

void MultiLayer::fitPageZoomToViewport()
{
    const double target = fitPageZoomFactor();
    const bool prevInternal = d_internal_zoom_change;
    d_internal_zoom_change = true;
    setPageZoom(target);
    d_internal_zoom_change = prevInternal;
}

QString MultiLayer::appliedStylePresetId() const
{
    return d_applied_style_preset_id;
}

void MultiLayer::setPageSizePhysical(double widthVal, double heightVal, PageUnit unit, int dpi)
{
    if (widthVal <= 0.0 || heightVal <= 0.0)
        return;

    d_page_width_val = widthVal;
    d_page_height_val = heightVal;
    d_page_unit = unit;
    d_page_dpi = qBound(36, dpi, 2400);

    const double screenPpi = 96.0;
    double pxW = widthVal;
    double pxH = heightVal;
    switch (unit) {
    case UnitPx:
        pxW = widthVal;
        pxH = heightVal;
        break;
    case UnitMm:
        pxW = (widthVal / 25.4) * screenPpi;
        pxH = (heightVal / 25.4) * screenPpi;
        break;
    case UnitCm:
        pxW = (widthVal / 2.54) * screenPpi;
        pxH = (heightVal / 2.54) * screenPpi;
        break;
    case UnitInch:
        pxW = widthVal * screenPpi;
        pxH = heightVal * screenPpi;
        break;
    }

    d_base_page_px = QSize(qMax(120, qRound(pxW)), qMax(120, qRound(pxH)));
    graph_width = d_base_page_px.width();
    graph_height = d_base_page_px.height();
    if (!d_keep_aspect_ratio)
        d_target_aspect_ratio = pxW / qMax(1.0, pxH);
    else if (qAbs(pxW - pxH) <= 2.0)
        d_target_aspect_ratio = 1.0;

    const QSize targetCanvasSize(qMax(80, qRound(d_base_page_px.width() * d_page_zoom)),
                                 qMax(80, qRound(d_base_page_px.height() * d_page_zoom)));

    QVector<bool> savedIgnore(graphsList.count(), false);
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr) {
            savedIgnore[i] = gr->ignoresResizeEvents();
            gr->setIgnoreResizeEvents(true);
        }
    }

    lastSize = targetCanvasSize;
    const bool prevSuppressResizeLayout = d_suppress_canvas_resize_layout;
    if (graphs > 0)
        d_suppress_canvas_resize_layout = true;
    syncMultiLayerPageHost(this, widget(), canvas, d_page_unit, d_page_view_mode, d_base_page_px,
                           d_page_zoom, d_page_bg_color);
    const bool freePx =
            (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4);
    resizeMultiLayerSubWindowForCanvas(this, widget(), d_layer_pill_frame,
                                       targetCanvasSize.width(), targetCanvasSize.height(), freePx);
    d_suppress_canvas_resize_layout = prevSuppressResizeLayout;

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr)
            gr->setIgnoreResizeEvents(savedIgnore[i]);
    }

    if (graphs > 0)
        fitLayersToPage();
    lastSize = canvas ? canvas->size() : targetCanvasSize;
    repositionCanvasStatusPill();
    emit modifiedPlot();
}

int MultiLayer::pageDpi() const
{
    return d_page_dpi;
}

MultiLayer::PageUnit MultiLayer::pageUnit() const
{
    return d_page_unit;
}

void MultiLayer::fitPageToLayers(int marginPx)
{
    if (graphsList.isEmpty())
        return;

    QRect unionRect;
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr)
            continue;
        const QSize sz = gr->plotWidget() ? gr->plotWidget()->size() : gr->size();
        unionRect = unionRect.united(QRect(gr->pos(), sz));
    }
    if (unionRect.isEmpty())
        return;

    const int m = qMax(0, marginPx);
    const QPoint shift(m - unionRect.left(), m - unionRect.top());
    const QSize tightSize(qMax(100, unionRect.width() + 2 * m),
                          qMax(100, unionRect.height() + 2 * m));

    QVector<bool> savedIgnore(graphsList.count(), false);
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr) {
            savedIgnore[i] = gr->ignoresResizeEvents();
            gr->setIgnoreResizeEvents(true);
            gr->move(gr->pos() + shift);
        }
    }

    d_base_page_px = QSize(qMax(80, qRound(tightSize.width() / d_page_zoom)),
                           qMax(80, qRound(tightSize.height() / d_page_zoom)));
    graph_width = d_base_page_px.width();
    graph_height = d_base_page_px.height();
    switch (d_page_unit) {
    case UnitPx:
        d_page_width_val = d_base_page_px.width();
        d_page_height_val = d_base_page_px.height();
        break;
    case UnitMm:
        d_page_width_val = (double(d_base_page_px.width()) / 96.0) * 25.4;
        d_page_height_val = (double(d_base_page_px.height()) / 96.0) * 25.4;
        break;
    case UnitCm:
        d_page_width_val = (double(d_base_page_px.width()) / 96.0) * 2.54;
        d_page_height_val = (double(d_base_page_px.height()) / 96.0) * 2.54;
        break;
    case UnitInch:
        d_page_width_val = double(d_base_page_px.width()) / 96.0;
        d_page_height_val = double(d_base_page_px.height()) / 96.0;
        break;
    }

    lastSize = tightSize;
    syncMultiLayerPageHost(this, widget(), canvas, d_page_unit, d_page_view_mode, d_base_page_px,
                           d_page_zoom, d_page_bg_color);
    if (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4) {
        if (canvas)
            canvas->resize(tightSize);
    }
    const bool freePx =
            (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4);
    resizeMultiLayerSubWindowForCanvas(this, widget(), d_layer_pill_frame, tightSize.width(),
                                       tightSize.height(), freePx);
    lastSize = canvas ? canvas->size() : tightSize;

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr)
            gr->setIgnoreResizeEvents(savedIgnore[i]);
    }

    repositionCanvasStatusPill();
    emit modifiedPlot();
}

void MultiLayer::fitLayersToPage()
{
    if (graphs <= 0)
        return;

    const bool savedKeepAspect = d_keep_aspect_ratio;
    const double savedTargetAspect = d_target_aspect_ratio;
    arrangeLayers(false, false);
    d_keep_aspect_ratio = savedKeepAspect;
    if (savedKeepAspect && savedTargetAspect > 0.05)
        d_target_aspect_ratio = savedTargetAspect;
    else if (l_canvas_width > 0 && l_canvas_height > 0)
        d_target_aspect_ratio = double(l_canvas_width) / double(l_canvas_height);
    syncChildOverlayAndInsetGeometries();
    emit modifiedPlot();
}

void MultiLayer::showPageSetupDialog()
{
    QPointer<MultiLayer> self(this);
    const QString beforeSnap = captureUndoSnapshot();
    bool anyApplied = false;

    QWidget *dlgParent = (window() && window() != this) ? window() : nullptr;
    QDialog dlg(dlgParent);
    dlg.setWindowTitle(tr("Page Setup"));
    dlg.setMinimumWidth(420);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);

    QGroupBox *grpPreset = new QGroupBox(tr("Page Size"), &dlg);
    QGridLayout *grid = new QGridLayout(grpPreset);

    grid->addWidget(new QLabel(tr("Preset:"), grpPreset), 0, 0);
    QComboBox *comboPreset = new QComboBox(grpPreset);
    comboPreset->addItem(tr("Custom"));
    comboPreset->addItem(tr("Journal 1-Column (85 x 85 mm)"));
    comboPreset->addItem(tr("Journal 1.5-Column (120 x 90 mm)"));
    comboPreset->addItem(tr("Journal 2-Column (170 x 120 mm)"));
    comboPreset->addItem(tr("Presentation 16:9 (254 x 143 mm)"));
    comboPreset->addItem(tr("A4 Landscape"));
    grid->addWidget(comboPreset, 0, 1, 1, 2);

    grid->addWidget(new QLabel(tr("Units:"), grpPreset), 1, 0);
    QComboBox *comboUnit = new QComboBox(grpPreset);
    comboUnit->addItem(QStringLiteral("px"), int(UnitPx));
    comboUnit->addItem(QStringLiteral("mm"), int(UnitMm));
    comboUnit->addItem(QStringLiteral("cm"), int(UnitCm));
    comboUnit->addItem(tr("inch"), int(UnitInch));
    comboUnit->setCurrentIndex(int(d_page_unit));
    grid->addWidget(comboUnit, 1, 1, 1, 2);

    grid->addWidget(new QLabel(tr("Width:"), grpPreset), 2, 0);
    QDoubleSpinBox *spinW = new QDoubleSpinBox(grpPreset);
    spinW->setRange(1.0, 10000.0);
    spinW->setDecimals(2);
    spinW->setValue(d_page_width_val);
    grid->addWidget(spinW, 2, 1, 1, 2);

    grid->addWidget(new QLabel(tr("Height:"), grpPreset), 3, 0);
    QDoubleSpinBox *spinH = new QDoubleSpinBox(grpPreset);
    spinH->setRange(1.0, 10000.0);
    spinH->setDecimals(2);
    spinH->setValue(d_page_height_val);
    grid->addWidget(spinH, 3, 1, 1, 2);

    grid->addWidget(new QLabel(tr("Resolution (DPI):"), grpPreset), 4, 0);
    QSpinBox *spinDpi = new QSpinBox(grpPreset);
    spinDpi->setRange(72, 2400);
    spinDpi->setSingleStep(50);
    spinDpi->setValue(d_page_dpi);
    grid->addWidget(spinDpi, 4, 1, 1, 2);

    mainLayout->addWidget(grpPreset);

    QGroupBox *grpView = new QGroupBox(tr("Page View Mode"), &dlg);
    QGridLayout *vGrid = new QGridLayout(grpView);

    QCheckBox *chkPageView = new QCheckBox(tr("Page View Mode"), grpView);
    chkPageView->setChecked(d_page_view_mode);
    vGrid->addWidget(chkPageView, 0, 0, 1, 2);

    vGrid->addWidget(new QLabel(tr("Zoom") + QStringLiteral(" (%):"), grpView), 1, 0);
    QDoubleSpinBox *spinZoom = new QDoubleSpinBox(grpView);
    spinZoom->setRange(25.0, 400.0);
    spinZoom->setDecimals(0);
    spinZoom->setSingleStep(10.0);
    spinZoom->setValue(d_page_zoom * 100.0);
    vGrid->addWidget(spinZoom, 1, 1);

    vGrid->addWidget(new QLabel(tr("Background Color:"), grpView), 2, 0);
    ColorButton *btnBg = new ColorButton(grpView);
    btnBg->setColor(d_page_bg_color);
    vGrid->addWidget(btnBg, 2, 1);

    QHBoxLayout *fitRow = new QHBoxLayout();
    QPushButton *btnFitPage = new QPushButton(tr("Fit Page to Layers"), grpView);
    QPushButton *btnFitLayers = new QPushButton(tr("Fit Layers to Page"), grpView);
    fitRow->addWidget(btnFitPage);
    fitRow->addWidget(btnFitLayers);
    vGrid->addLayout(fitRow, 3, 0, 1, 2);

    mainLayout->addWidget(grpView);

    auto toMm = [](double val, PageUnit u) -> double {
        switch (u) {
        case UnitPx:
            return (val / 96.0) * 25.4;
        case UnitMm:
            return val;
        case UnitCm:
            return val * 10.0;
        case UnitInch:
            return val * 25.4;
        }
        return val;
    };
    auto fromMm = [](double mm, PageUnit u) -> double {
        switch (u) {
        case UnitPx:
            return (mm / 25.4) * 96.0;
        case UnitMm:
            return mm;
        case UnitCm:
            return mm / 10.0;
        case UnitInch:
            return mm / 25.4;
        }
        return mm;
    };

    PageUnit lastUnit = d_page_unit;
    QObject::connect(comboUnit, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int idx) {
                         const PageUnit newUnit = PageUnit(comboUnit->itemData(idx).toInt());
                         const double wMm = toMm(spinW->value(), lastUnit);
                         const double hMm = toMm(spinH->value(), lastUnit);
                         spinW->blockSignals(true);
                         spinH->blockSignals(true);
                         spinW->setValue(fromMm(wMm, newUnit));
                         spinH->setValue(fromMm(hMm, newUnit));
                         spinW->blockSignals(false);
                         spinH->blockSignals(false);
                         lastUnit = newUnit;
                     });

    QObject::connect(comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int idx) {
                         double wMm = 0.0, hMm = 0.0;
                         if (idx == 1) {
                             wMm = 85.0;
                             hMm = 85.0;
                         } else if (idx == 2) {
                             wMm = 120.0;
                             hMm = 90.0;
                         } else if (idx == 3) {
                             wMm = 170.0;
                             hMm = 120.0;
                         } else if (idx == 4) {
                             wMm = 254.0;
                             hMm = 143.0;
                         } else if (idx == 5) {
                             wMm = 297.0;
                             hMm = 210.0;
                         } else {
                             return;
                         }
                         comboUnit->setCurrentIndex(int(UnitMm));
                         lastUnit = UnitMm;
                         spinW->setValue(wMm);
                         spinH->setValue(hMm);
                     });

    QObject::connect(btnFitPage, &QPushButton::clicked, &dlg, [&]() {
        if (!self)
            return;
        fitPageToLayers(14);
        anyApplied = true;
    });
    QObject::connect(btnFitLayers, &QPushButton::clicked, &dlg, [&]() {
        if (!self)
            return;
        fitLayersToPage();
        anyApplied = true;
    });

    auto applyNow = [&]() {
        if (!self)
            return;
        const PageUnit u = PageUnit(comboUnit->currentData().toInt());
        setPageBackgroundColor(btnBg->color());
        setPageViewMode(chkPageView->isChecked());
        setPageZoom(spinZoom->value() / 100.0);
        setPageSizePhysical(spinW->value(), spinH->value(), u, spinDpi->value());
        anyApplied = true;
    };

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *btnApply = new QPushButton(tr("&Apply"), &dlg);
    QPushButton *btnOk = new QPushButton(tr("&OK"), &dlg);
    QPushButton *btnCancel = new QPushButton(tr("&Cancel"), &dlg);
    btnOk->setDefault(true);
    btnLayout->addWidget(btnApply);
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    QObject::connect(btnApply, &QPushButton::clicked, &dlg, applyNow);
    QObject::connect(btnOk, &QPushButton::clicked, &dlg, [&]() {
        applyNow();
        dlg.accept();
    });
    QObject::connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    dlg.exec();
    if (self && anyApplied)
        pushUndoSnapshot(tr("Page Setup"), beforeSnap);
}

Graph *MultiLayer::addLinkedLayer(Graph *parentLayer, bool linkX, bool linkY,
                                  const QString &xFormula, const QString &yFormula)
{
    if (!parentLayer)
        parentLayer = active_graph ? active_graph.data() : layer(1);
    if (!parentLayer)
        return addLayer();

    const QRect pGeo = parentLayer->geometry();
    Graph *child = addLayer(pGeo.x(), pGeo.y(), pGeo.width(), pGeo.height());
    if (!child)
        return nullptr;

    child->setIgnoreResizeEvents(true);
    child->removeLegend();
    child->setTitle("");
    child->setFrame(0);
    child->drawCanvasFrame(false, 0);
    child->setBackgroundColor(QColor(255, 255, 255, 0));
    child->setCanvasBackground(QColor(255, 255, 255, 0));

    Plot *cPlot = child->plotWidget();
    if (cPlot) {
        cPlot->setAutoFillBackground(false);
        cPlot->setAttribute(Qt::WA_TranslucentBackground, true);
        if (cPlot->canvas())
            cPlot->canvas()->setAutoFillBackground(false);
        cPlot->setMargin(parentLayer->plotWidget() ? parentLayer->plotWidget()->margin() : 14);
    }
    child->setAttribute(Qt::WA_TranslucentBackground, true);

    const bool useTop = linkX;
    const bool useRight = linkY || !linkX;

    child->enableAxis(QwtPlot::xBottom, false);
    child->enableAxis(QwtPlot::yLeft, false);
    child->enableAxis(QwtPlot::xTop, useTop);
    child->enableAxis(QwtPlot::yRight, useRight);

    child->setAxisBackboneVisible(QwtPlot::xBottom, false);
    child->setAxisBackboneVisible(QwtPlot::yLeft, false);
    child->setAxisBackboneVisible(QwtPlot::xTop, useTop);
    child->setAxisBackboneVisible(QwtPlot::yRight, useRight);

    if (Plot *parentPlot = parentLayer->plotWidget()) {
        if (linkX) {
            parentPlot->setMajorTicksType(QwtPlot::xTop, ScaleDraw::None);
            parentPlot->setMinorTicksType(QwtPlot::xTop, ScaleDraw::None);
        }
        if (linkY) {
            parentPlot->setMajorTicksType(QwtPlot::yRight, ScaleDraw::None);
            parentPlot->setMinorTicksType(QwtPlot::yRight, ScaleDraw::None);
        }
    }

    if (cPlot) {
        if (QwtScaleDraw *sdTop = cPlot->axisScaleDraw(QwtPlot::xTop))
            sdTop->enableComponent(QwtAbstractScaleDraw::Labels, linkX);
        if (QwtScaleDraw *sdRight = cPlot->axisScaleDraw(QwtPlot::yRight))
            sdRight->enableComponent(QwtAbstractScaleDraw::Labels, linkY || !linkX);
        cPlot->setMajorTicksType(QwtPlot::xTop, linkX ? ScaleDraw::In : ScaleDraw::None);
        cPlot->setMinorTicksType(QwtPlot::xTop, linkX ? ScaleDraw::In : ScaleDraw::None);
        cPlot->setMajorTicksType(QwtPlot::yRight,
                                 (linkY || !linkX) ? ScaleDraw::In : ScaleDraw::None);
        cPlot->setMinorTicksType(QwtPlot::yRight,
                                 (linkY || !linkX) ? ScaleDraw::In : ScaleDraw::None);
        if (QwtScaleWidget *swTop = cPlot->axisWidget(QwtPlot::xTop))
            swTop->setSpacing(5);
        if (QwtScaleWidget *swRight = cPlot->axisWidget(QwtPlot::yRight))
            swRight->setSpacing(6);
    }

    for (int a = 0; a < QwtPlot::axisCnt; ++a)
        child->setAxisTitleAuto(a, false);
    child->setAxisTitle(QwtPlot::xBottom, QString());
    child->setAxisTitle(QwtPlot::yLeft, QString());
    child->setAxisTitle(QwtPlot::xTop, QString());
    child->setAxisTitle(QwtPlot::yRight, QString());
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        child->setAxisFont(a, parentLayer->axisFont(a));
        child->setAxisTitleFont(a, parentLayer->axisTitleFont(a));
    }

    auto classifyMode = [](bool enabled, const QString &expr, const QString &ident1,
                           const QString &ident2) -> int {
        if (!enabled)
            return 0;
        const QString e = expr.trimmed();
        if (e.isEmpty() || e.compare(ident1, Qt::CaseInsensitive) == 0
            || e.compare(ident2, Qt::CaseInsensitive) == 0)
            return 1;
        return 2;
    };

    const int xMode = classifyMode(linkX, xFormula, QString("x"), QString("x1"));
    const int yMode = classifyMode(linkY, yFormula, QString("y"), QString("y1"));

    child->setLinkedParentLayer(parentLayer, xMode, xFormula, yMode, yFormula);
    syncChildOverlayAndInsetGeometries();
    installLayerEventFilters(child);
    setActiveGraph(parentLayer);
    enforceLayerZOrder();
    refreshLayerButtonBadges();
    emit modifiedPlot();
    return child;
}

bool MultiLayer::isInsetLayer(const Graph *g) const
{
    return g && d_inset_specs.contains(const_cast<Graph *>(g));
}

Graph *MultiLayer::insetSourceLayer(const Graph *g) const
{
    if (!g)
        return nullptr;
    auto it = d_inset_specs.constFind(const_cast<Graph *>(g));
    return (it != d_inset_specs.constEnd()) ? it.value().sourceLayer.data() : nullptr;
}

QRectF MultiLayer::insetNormGeo(const Graph *g) const
{
    if (!g)
        return QRectF();
    auto it = d_inset_specs.constFind(const_cast<Graph *>(g));
    return (it != d_inset_specs.constEnd()) ? it.value().normGeo : QRectF();
}

QRectF MultiLayer::insetZoomDataRect(const Graph *g) const
{
    if (!g)
        return QRectF();
    auto it = d_inset_specs.constFind(const_cast<Graph *>(g));
    return (it != d_inset_specs.constEnd()) ? it.value().zoomDataRect : QRectF();
}

bool MultiLayer::insetShowConnectors(const Graph *g) const
{
    if (!g)
        return false;
    auto it = d_inset_specs.constFind(const_cast<Graph *>(g));
    return (it != d_inset_specs.constEnd()) ? it.value().showConnectors : false;
}

void MultiLayer::setInsetShowConnectors(Graph *insetLayer, bool show)
{
    if (!insetLayer || !d_inset_specs.contains(insetLayer))
        return;
    if (d_inset_specs[insetLayer].showConnectors == show)
        return;
    d_inset_specs[insetLayer].showConnectors = show;
    if (canvas)
        canvas->update();
    update();
    emit modifiedPlot();
}

void MultiLayer::setInsetZoomDataRect(Graph *insetLayer, const QRectF &roi)
{
    if (!insetLayer || !d_inset_specs.contains(insetLayer))
        return;
    const QRectF norm = roi.normalized();
    if (norm.width() <= 1e-15 && norm.height() <= 1e-15)
        return;

    Plot *inPlot = insetLayer->plotWidget();
    Graph *srcLayer = d_inset_specs[insetLayer].sourceLayer.data();
    Plot *srcPlot = srcLayer ? srcLayer->plotWidget() : nullptr;

    d_inset_specs[insetLayer].zoomDataRect = norm;
    insetLayer->enableAutoscaling(false);

    auto scaleTypeForAxis = [&](int axisId) -> int {
        if (inPlot && Plot::isLog10ScaleEngine(inPlot->axisScaleEngine(axisId)))
            return 1;
        if (srcPlot && Plot::isLog10ScaleEngine(srcPlot->axisScaleEngine(axisId)))
            return 1;
        return 0;
    };
    auto invertedForAxis = [&](int axisId) -> bool {
        if (inPlot && inPlot->axisScaleEngine(axisId))
            return inPlot->axisScaleEngine(axisId)->testAttribute(QwtScaleEngine::Inverted);
        if (srcPlot && srcPlot->axisScaleEngine(axisId))
            return srcPlot->axisScaleEngine(axisId)->testAttribute(QwtScaleEngine::Inverted);
        return false;
    };

    const int xType = scaleTypeForAxis(QwtPlot::xBottom);
    const int yType = scaleTypeForAxis(QwtPlot::yLeft);
    const bool xInv = invertedForAxis(QwtPlot::xBottom);
    const bool yInv = invertedForAxis(QwtPlot::yLeft);

    if (norm.width() > 1e-15) {
        const double xLo = qMin(norm.left(), norm.right());
        const double xHi = qMax(norm.left(), norm.right());
        if (xType != 1 || (xLo > 0.0 && xHi > xLo)) {
            const int inXMaj = (insetLayer->width() < 105) ? 2 : 3;
            insetLayer->setScale(QwtPlot::xBottom, xLo, xHi, 0.0, inXMaj, 1, xType, xInv);
            insetLayer->setScale(QwtPlot::xTop, xLo, xHi, 0.0, inXMaj, 1, xType, xInv);
        }
    }
    if (norm.height() > 1e-15) {
        const double yLo = qMin(norm.top(), norm.bottom());
        const double yHi = qMax(norm.top(), norm.bottom());
        if (yType != 1 || (yLo > 0.0 && yHi > yLo)) {
            insetLayer->setScale(QwtPlot::yLeft, yLo, yHi, 0.0, 3, 1, yType, yInv);
            insetLayer->setScale(QwtPlot::yRight, yLo, yHi, 0.0, 3, 1, yType, yInv);
        }
    }
    if (inPlot) {
        inPlot->updateAxes();
        inPlot->updateLayout();
        inPlot->replot();
    }
    syncCanvasOverlayVisibility();
    if (canvas)
        canvas->update();
    update();
    emit modifiedPlot();
}

Graph *MultiLayer::addInsetLayer(Graph *sourceLayer, const QRectF &roiDataRect, bool showConnectors)
{
    if (!sourceLayer)
        sourceLayer = active_graph ? active_graph.data() : layer(1);
    if (!sourceLayer)
        return addLayer();

    Plot *srcPlot = sourceLayer->plotWidget();

    auto scanYInXWindow = [&](double xMin, double xMax, double &yMinOut, double &yMaxOut) -> bool {
        bool hasY = false;
        yMinOut = 0.0;
        yMaxOut = 0.0;
        auto considerY = [&](double yv) {
            if (!std::isfinite(yv))
                return;
            if (!hasY) {
                yMinOut = yMaxOut = yv;
                hasY = true;
            } else {
                if (yv < yMinOut)
                    yMinOut = yv;
                if (yv > yMaxOut)
                    yMaxOut = yv;
            }
        };
        auto scanCurve = [&](int cIdx) {
            if (cIdx < 0 || cIdx >= sourceLayer->curves())
                return;
            const int cType = sourceLayer->curveType(cIdx);
            if (cType == Graph::ErrorBars || cType == Graph::Pie || cType == Graph::Box)
                return;
            QwtPlotCurve *cv = sourceLayer->curve(cIdx);
            if (!cv || !cv->isVisible())
                return;
            const int n = cv->dataSize();
            for (int k = 0; k < n; ++k) {
                const double xv = cv->x(k);
                const double yv = cv->y(k);
                if (std::isfinite(xv) && xv >= xMin && xv <= xMax)
                    considerY(yv);
                if (k + 1 < n) {
                    const double xNext = cv->x(k + 1);
                    const double yNext = cv->y(k + 1);
                    if (std::isfinite(xv) && std::isfinite(xNext) && std::isfinite(yv)
                        && std::isfinite(yNext) && qAbs(xNext - xv) > 1e-18) {
                        const double segLo = qMin(xv, xNext);
                        const double segHi = qMax(xv, xNext);
                        if (xMin >= segLo && xMin <= segHi) {
                            const double t = (xMin - xv) / (xNext - xv);
                            considerY(yv + t * (yNext - yv));
                        }
                        if (xMax >= segLo && xMax <= segHi) {
                            const double t = (xMax - xv) / (xNext - xv);
                            considerY(yv + t * (yNext - yv));
                        }
                    }
                }
            }
            if (auto *dc = dynamic_cast<DataCurve *>(cv)) {
                for (DataCurve *ebBase : dc->errorBarsList()) {
                    auto *eb = dynamic_cast<QwtErrorPlotCurve *>(ebBase);
                    if (!eb || !eb->isVisible() || eb->direction() != QwtErrorPlotCurve::Vertical)
                        continue;
                    const int en = qMin(n, eb->dataSize());
                    for (int k = 0; k < en; ++k) {
                        const double xv = cv->x(k);
                        const double yv = cv->y(k);
                        const double ev = eb->errorValue(k);
                        if (std::isfinite(xv) && xv >= xMin && xv <= xMax && std::isfinite(yv)
                            && std::isfinite(ev)) {
                            if (eb->minusSide())
                                considerY(yv - ev);
                            if (eb->plusSide())
                                considerY(yv + ev);
                        }
                    }
                }
            }
        };

        const int selIdx = sourceLayer->selectedCurveIndex();
        if (selIdx >= 0 && selIdx < sourceLayer->curves()
            && sourceLayer->curveType(selIdx) != Graph::ErrorBars) {
            scanCurve(selIdx);
        }
        if (!hasY) {
            for (int k = 0; k < sourceLayer->curves(); ++k)
                scanCurve(k);
        }
        return hasY;
    };

    auto fallbackAxisSubRange = [&](int axisId, double frac0, double frac1, double &loOut,
                                    double &hiOut) {
        double aLo = 0.0;
        double aHi = 1.0;
        if (srcPlot) {
            if (const QwtScaleDiv *sd = srcPlot->axisScaleDiv(axisId)) {
                aLo = qMin(sd->lowerBound(), sd->upperBound());
                aHi = qMax(sd->lowerBound(), sd->upperBound());
            }
        }
        if (!(aHi > aLo + 1e-15)) {
            aLo = 0.0;
            aHi = 1.0;
        }
        if (srcPlot && Plot::isLog10ScaleEngine(srcPlot->axisScaleEngine(axisId)) && aLo > 0.0
            && aHi > aLo) {
            const double l0 = std::log10(aLo);
            const double l1 = std::log10(aHi);
            loOut = std::pow(10.0, l0 + frac0 * (l1 - l0));
            hiOut = std::pow(10.0, l0 + frac1 * (l1 - l0));
        } else {
            loOut = aLo + frac0 * (aHi - aLo);
            hiOut = aLo + frac1 * (aHi - aLo);
        }
    };

    QRectF effectiveRoi = roiDataRect;
    if (effectiveRoi.isEmpty()) {
        if (RoiGadgetTool *roiTool = dynamic_cast<RoiGadgetTool *>(sourceLayer->activeTool())) {
            const QRectF r = roiTool->roiRect();
            if (qAbs(r.width()) > 1e-15 || qAbs(r.height()) > 1e-15)
                effectiveRoi = r.normalized();
        } else if (sourceLayer->rangeSelectorsEnabled()) {
            const double x0 = sourceLayer->selectedXStartValue();
            const double x1 = sourceLayer->selectedXEndValue();
            const double xMin = qMin(x0, x1);
            const double xMax = qMax(x0, x1);
            if (xMax > xMin + 1e-15) {
                double yMin = 0.0, yMax = 0.0;
                if (scanYInXWindow(xMin, xMax, yMin, yMax)) {
                    const double padY = (yMax > yMin) ? 0.08 * (yMax - yMin)
                                                      : (qAbs(yMax) * 0.05 + 1e-6);
                    effectiveRoi = QRectF(QPointF(xMin, yMin - padY), QPointF(xMax, yMax + padY));
                } else {
                    double fy0 = 0.0, fy1 = 1.0;
                    fallbackAxisSubRange(QwtPlot::yLeft, 0.30, 0.70, fy0, fy1);
                    effectiveRoi = QRectF(QPointF(xMin, fy0), QPointF(xMax, fy1));
                }
            }
        }

        // Default auto-zoom into central [30%..70%] data range when no active ROI is present
        if (effectiveRoi.isEmpty()) {
            double dataXMin = 0.0, dataXMax = 0.0;
            bool hasDataX = false;
            for (int k = 0; k < sourceLayer->curves(); ++k) {
                const int cType = sourceLayer->curveType(k);
                if (cType == Graph::ErrorBars || cType == Graph::Pie || cType == Graph::Box)
                    continue;
                QwtPlotCurve *cv = sourceLayer->curve(k);
                if (!cv || !cv->isVisible())
                    continue;
                const int n = cv->dataSize();
                for (int p = 0; p < n; ++p) {
                    const double xv = cv->x(p);
                    if (!std::isfinite(xv))
                        continue;
                    if (!hasDataX) {
                        dataXMin = dataXMax = xv;
                        hasDataX = true;
                    } else {
                        if (xv < dataXMin)
                            dataXMin = xv;
                        if (xv > dataXMax)
                            dataXMax = xv;
                    }
                }
            }

            double zX0 = 0.0, zX1 = 1.0;
            const bool logX = srcPlot
                    && Plot::isLog10ScaleEngine(srcPlot->axisScaleEngine(QwtPlot::xBottom));
            if (hasDataX && dataXMax > dataXMin + 1e-15) {
                if (logX && dataXMin > 0.0) {
                    const double l0 = std::log10(dataXMin);
                    const double l1 = std::log10(dataXMax);
                    zX0 = std::pow(10.0, l0 + 0.30 * (l1 - l0));
                    zX1 = std::pow(10.0, l0 + 0.70 * (l1 - l0));
                } else {
                    zX0 = dataXMin + 0.30 * (dataXMax - dataXMin);
                    zX1 = dataXMin + 0.70 * (dataXMax - dataXMin);
                }
            } else {
                fallbackAxisSubRange(QwtPlot::xBottom, 0.30, 0.70, zX0, zX1);
            }

            double yMin = 0.0, yMax = 0.0;
            double zY0 = 0.0, zY1 = 1.0;
            const bool logY = srcPlot
                    && Plot::isLog10ScaleEngine(srcPlot->axisScaleEngine(QwtPlot::yLeft));
            if (scanYInXWindow(zX0, zX1, yMin, yMax)) {
                if (logY && yMin > 0.0 && yMax > 0.0) {
                    const double ly0 = std::log10(yMin);
                    const double ly1 = std::log10(yMax);
                    const double lSpan = qMax(0.05, ly1 - ly0);
                    zY0 = std::pow(10.0, ly0 - 0.08 * lSpan);
                    zY1 = std::pow(10.0, ly1 + 0.08 * lSpan);
                } else {
                    const double padY = (yMax > yMin + 1e-15) ? 0.08 * (yMax - yMin)
                                                              : qMax(1e-6, qAbs(yMax) * 0.08);
                    zY0 = yMin - padY;
                    zY1 = yMax + padY;
                }
            } else {
                fallbackAxisSubRange(QwtPlot::yLeft, 0.30, 0.70, zY0, zY1);
            }
            effectiveRoi = QRectF(QPointF(zX0, zY0), QPointF(zX1, zY1)).normalized();
        }
    }

    const QRect srcGeo = sourceLayer->geometry();
    QRectF initialNormGeo(0.54, 0.08, 0.38, 0.38);
    int iw = qMax(40, qRound(initialNormGeo.width() * srcGeo.width()));
    int ih = qMax(40, qRound(initialNormGeo.height() * srcGeo.height()));
    int ix = srcGeo.x() + qRound(initialNormGeo.x() * srcGeo.width());
    int iy = srcGeo.y() + qRound(initialNormGeo.y() * srcGeo.height());
    if (srcPlot) {
        srcPlot->updateLayout();
        const QRect pCanvas = srcPlot->plotLayout()->canvasRect().translated(srcGeo.topLeft());
        if (pCanvas.width() > 60 && pCanvas.height() > 60) {
            iw = qBound(60, qRound(0.42 * pCanvas.width()), qMax(60, pCanvas.width() - 12));
            ih = qBound(50, qRound(0.42 * pCanvas.height()), qMax(50, pCanvas.height() - 12));
            ix = qMax(pCanvas.left() + 6, pCanvas.right() - iw - 10);
            iy = pCanvas.top() + 10;
            iy = qBound(pCanvas.top() + 6, iy, qMax(pCanvas.top() + 6, pCanvas.bottom() - ih - 6));
            if (srcGeo.width() >= 20 && srcGeo.height() >= 20) {
                initialNormGeo = QRectF(double(ix - srcGeo.x()) / double(srcGeo.width()),
                                        double(iy - srcGeo.y()) / double(srcGeo.height()),
                                        double(iw) / double(srcGeo.width()),
                                        double(ih) / double(srcGeo.height()));
            }
        }
    }

    const bool prevSync = d_syncing_inset_geometries;
    d_syncing_inset_geometries = true;

    Graph *inset = addLayer(ix, iy, iw, ih);
    if (!inset) {
        d_syncing_inset_geometries = prevSync;
        return nullptr;
    }

    InsetSpec spec;
    spec.sourceLayer = QPointer<Graph>(sourceLayer);
    spec.normGeo = initialNormGeo;
    spec.zoomDataRect = effectiveRoi.normalized();
    spec.showConnectors = showConnectors;
    d_inset_specs[inset] = spec;

    inset->setIgnoreResizeEvents(true);
    if (!effectiveRoi.isNull()
        && (qAbs(effectiveRoi.width()) > 1e-15 || qAbs(effectiveRoi.height()) > 1e-15)) {
        inset->enableAutoscaling(false);
    }
    inset->removeLegend();
    inset->setTitle("");
    inset->setBackgroundColor(QColor(Qt::white));
    inset->setCanvasBackground(QColor("#FFFFFF"));
    {
        QPalette insetPal = inset->palette();
        insetPal.setColor(QPalette::Window, QColor(Qt::white));
        inset->setPalette(insetPal);
        inset->setAutoFillBackground(true);
    }
    if (inset->plotWidget()) {
        QPalette plotPal = inset->plotWidget()->palette();
        plotPal.setColor(QPalette::Window, QColor(Qt::white));
        inset->plotWidget()->setPalette(plotPal);
        inset->plotWidget()->setAutoFillBackground(true);
        inset->plotWidget()->setMargin(4);
    }

    for (int i = 0; i < sourceLayer->curves(); ++i) {
        QwtPlotItem *it = sourceLayer->plotItem(i);
        if (!it || it->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;
        const int cType = sourceLayer->curveType(i);
        if (cType == Graph::ErrorBars || cType == Graph::Pie || cType == Graph::Box)
            continue;
        QwtPlotCurve *srcCurve = sourceLayer->curve(i);
        if (!srcCurve)
            continue;

        DataCurve *dc = dynamic_cast<DataCurve *>(srcCurve);
        if (dc && dc->table()) {
            inset->insertCurve(dc->table(), dc->xColumnName(), dc->yColumnName(), cType,
                               dc->startRow(), dc->endRow());
            if (QwtPlotCurve *dstCurve = inset->curve(inset->curves() - 1)) {
                dstCurve->setPen(srcCurve->pen());
                dstCurve->setSymbol(srcCurve->symbol());
                dstCurve->setBrush(srcCurve->brush());
                dstCurve->setStyle(srcCurve->style());
                if (PlotCurve *dstPc = dynamic_cast<PlotCurve *>(dstCurve))
                    dstPc->setSymbolSkip(dc->symbolSkip());
            }
        } else if (auto *srcFc = dynamic_cast<FunctionCurve *>(srcCurve)) {
            const int n = srcFc->dataSize();
            if (n > 0) {
                auto *dstFc = new FunctionCurve(findMultiLayerApp(this), srcFc->functionType(),
                                                srcFc->title().text());
                dstFc->copy(srcFc);
                QVector<double> xv(n), yv(n);
                for (int j = 0; j < n; ++j) {
                    xv[j] = srcFc->x(j);
                    yv[j] = srcFc->y(j);
                }
                dstFc->setData(xv.constData(), yv.constData(), n);
                dstFc->setPen(srcFc->pen());
                dstFc->setSymbol(srcFc->symbol());
                dstFc->setBrush(srcFc->brush());
                dstFc->setStyle(srcFc->style());
                dstFc->setSymbolSkip(srcFc->symbolSkip());
                inset->insertPlotItem(dstFc, Graph::Function);
            }
        } else {
            const int n = srcCurve->dataSize();
            if (n > 0) {
                const int safeType = (cType == Graph::Function) ? Graph::Line : cType;
                PlotCurve *pc = new PlotCurve(srcCurve->title().text());
                pc->setType(safeType);
                QVector<double> xv(n), yv(n);
                for (int j = 0; j < n; ++j) {
                    xv[j] = srcCurve->x(j);
                    yv[j] = srcCurve->y(j);
                }
                pc->setData(xv.constData(), yv.constData(), n);
                pc->setPen(srcCurve->pen());
                pc->setSymbol(srcCurve->symbol());
                pc->setBrush(srcCurve->brush());
                pc->setStyle(srcCurve->style());
                if (PlotCurve *srcPc = dynamic_cast<PlotCurve *>(srcCurve))
                    pc->setSymbolSkip(srcPc->symbolSkip());
                inset->insertPlotItem(pc, safeType);
            }
        }
    }

    // Clone attached error bar curves onto the inset layer
    for (int i = 0; i < sourceLayer->curves(); ++i) {
        if (sourceLayer->curveType(i) != Graph::ErrorBars)
            continue;
        auto *srcErr = dynamic_cast<QwtErrorPlotCurve *>(sourceLayer->curve(i));
        if (!srcErr || !srcErr->table() || !srcErr->masterCurve())
            continue;
        inset->addErrorBars(srcErr->masterCurve()->title().text(), srcErr->table(),
                            srcErr->title().text(), srcErr->direction(), qMax(1, srcErr->width()),
                            srcErr->capLength(), srcErr->color(), srcErr->throughSymbol(),
                            srcErr->minusSide(), srcErr->plusSide());
        if (QwtPlotCurve *dstErr = inset->curve(inset->curves() - 1))
            dstErr->setVisible(srcErr->isVisible());
    }

    inset->removeLegend();
    const int srcTickPt = sourceLayer->axisFont(QwtPlot::yLeft).pointSize();
    const int insetTickPt = (srcTickPt >= 20) ? qBound(12, srcTickPt / 2, 16) : 8;
    QFont insetFont("Arial", insetTickPt, QFont::Normal, false);
    insetFont.setHintingPreference(QFont::PreferNoHinting);
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        inset->setAxisTitleAuto(a, false);
        inset->setAxisTitle(a, QString());
        inset->setAxisFont(a, insetFont);
        inset->setAxisTitleFont(a, insetFont);
    }
    inset->applyFramePreset(Graph::FrameBoxInwardTicks);
    inset->setTicksLength(2, 4);
    inset->setAxesLinewidth(1);

    const int xScaleType = (srcPlot && Plot::isLog10ScaleEngine(srcPlot->axisScaleEngine(QwtPlot::xBottom)))
            ? 1
            : 0;
    const int yScaleType = (srcPlot && Plot::isLog10ScaleEngine(srcPlot->axisScaleEngine(QwtPlot::yLeft)))
            ? 1
            : 0;
    const bool xInv = srcPlot && srcPlot->axisScaleEngine(QwtPlot::xBottom)
            && srcPlot->axisScaleEngine(QwtPlot::xBottom)->testAttribute(QwtScaleEngine::Inverted);
    const bool yInv = srcPlot && srcPlot->axisScaleEngine(QwtPlot::yLeft)
            && srcPlot->axisScaleEngine(QwtPlot::yLeft)->testAttribute(QwtScaleEngine::Inverted);

    if (!effectiveRoi.isNull()
        && (qAbs(effectiveRoi.width()) > 1e-15 || qAbs(effectiveRoi.height()) > 1e-15)) {
        inset->enableAutoscaling(false);
        if (qAbs(effectiveRoi.width()) > 1e-15) {
            const double xLo = qMin(effectiveRoi.left(), effectiveRoi.right());
            const double xHi = qMax(effectiveRoi.left(), effectiveRoi.right());
            const int inXMaj = (iw < 105) ? 2 : 3;
            inset->setScale(QwtPlot::xBottom, xLo, xHi, 0.0, inXMaj, 1, xScaleType, xInv);
            inset->setScale(QwtPlot::xTop, xLo, xHi, 0.0, inXMaj, 1, xScaleType, xInv);
        }
        if (qAbs(effectiveRoi.height()) > 1e-15) {
            const double yLo = qMin(effectiveRoi.top(), effectiveRoi.bottom());
            const double yHi = qMax(effectiveRoi.top(), effectiveRoi.bottom());
            inset->setScale(QwtPlot::yLeft, yLo, yHi, 0.0, 3, 1, yScaleType, yInv);
            inset->setScale(QwtPlot::yRight, yLo, yHi, 0.0, 3, 1, yScaleType, yInv);
        }
    } else {
        inset->setAutoScale();
    }

    inset->setGeometry(ix, iy, iw, ih);
    if (inset->plotWidget()) {
        inset->plotWidget()->resize(QSize(iw, ih));
        inset->plotWidget()->updateLayout();
        inset->plotWidget()->replot();
    }
    d_inset_specs[inset].zoomDataRect = effectiveRoi.normalized();
    d_syncing_inset_geometries = prevSync;

    installLayerEventFilters(inset);
    setActiveGraph(sourceLayer);
    enforceLayerZOrder();
    refreshLayerButtonBadges();
    syncCanvasOverlayVisibility();
    emit modifiedPlot();
    return inset;
}

void MultiLayer::updateCanvasStatusPill()
{
    if (!d_canvas_status_pill)
        return;

    double wDisp = d_page_width_val;
    double hDisp = d_page_height_val;
    QString unitStr = QStringLiteral("mm");
    int decimals = 1;

    switch (d_page_unit) {
    case UnitPx: {
        const double zoom = qMax(0.25, d_page_zoom);
        const double unzoomedW = (canvas && canvas->width() >= 50)
                ? (double(canvas->width()) / zoom)
                : double(qMax(120, d_base_page_px.width()));
        const double unzoomedH = (canvas && canvas->height() >= 50)
                ? (double(canvas->height()) / zoom)
                : double(qMax(120, d_base_page_px.height()));
        wDisp = (unzoomedW / 96.0) * 25.4;
        hDisp = (unzoomedH / 96.0) * 25.4;
        unitStr = QStringLiteral("mm");
        decimals = 1;
        break;
    }
    case UnitMm:
        unitStr = QStringLiteral("mm");
        decimals = 1;
        break;
    case UnitCm:
        unitStr = QStringLiteral("cm");
        decimals = 2;
        break;
    case UnitInch:
        unitStr = QStringLiteral("in");
        decimals = 2;
        break;
    }

    QString fontFam = QStringLiteral("Arial");
    Graph *g = active_graph ? active_graph.data() : layer(1);
    if (g) {
        const QString af = g->axisFont(QwtPlot::xBottom).family().trimmed();
        if (!af.isEmpty())
            fontFam = af;
    }

    double liveAspect = (d_target_aspect_ratio > 0.05) ? d_target_aspect_ratio : 1.0;
    if (!d_keep_aspect_ratio) {
        if (g && g->plotWidget() && g->plotWidget()->plotLayout()) {
            const QRect cr = g->plotWidget()->plotLayout()->canvasRect();
            if (cr.width() > 20 && cr.height() > 20)
                liveAspect = double(cr.width()) / double(cr.height());
            else if (l_canvas_width > 0 && l_canvas_height > 0)
                liveAspect = double(l_canvas_width) / double(l_canvas_height);
        } else if (l_canvas_width > 0 && l_canvas_height > 0) {
            liveAspect = double(l_canvas_width) / double(l_canvas_height);
        }
    }

    auto formatRatioStr = [](double r) -> QString {
        if (qAbs(r - 1.0) < 0.02)
            return QStringLiteral("1:1");
        if (qAbs(r - (4.0 / 3.0)) < 0.02)
            return QStringLiteral("4:3");
        if (qAbs(r - (16.0 / 9.0)) < 0.02)
            return QStringLiteral("16:9");
        if (qAbs(r - (3.0 / 4.0)) < 0.02)
            return QStringLiteral("3:4");
        return QStringLiteral("%1:1").arg(QString::number(r, 'f', 2));
    };
    const QString ratioStr = formatRatioStr(liveAspect);
    const QString aspectBadge = d_keep_aspect_ratio
            ? QStringLiteral("%1 %2").arg(ratioStr, tr("Lock"))
            : QStringLiteral("%1 (%2)").arg(tr("Free"), ratioStr);

    const int zoomPct = qRound(d_page_zoom * 100.0);
    const QString sep = QString(QChar(0x00B7));
    const QString times = QString(QChar(0x00D7));
    auto fmtDim = [&](double v) -> QString {
        QString s = QString::number(v, 'f', decimals);
        if (decimals > 0) {
            while (s.endsWith(QLatin1Char('0')))
                s.chop(1);
            if (s.endsWith(QLatin1Char('.')))
                s.chop(1);
        }
        return s;
    };
    const QString wStr = fmtDim(wDisp);
    const QString hStr = fmtDim(hDisp);
    const QString dpiStr = QString::number(d_page_dpi);
    const QString fullStatusText = (zoomPct == 100)
            ? QStringLiteral("%1%2%3 %4 %5 %6 DPI %5 %7")
                      .arg(wStr, times, hStr, unitStr, sep, dpiStr, fontFam)
            : QStringLiteral("%1%2%3 %4 %5 %6 DPI %5 %7 %5 %8%")
                      .arg(wStr, times, hStr, unitStr, sep, dpiStr, fontFam,
                           QString::number(zoomPct));
    const QString compactStatusText =
            QStringLiteral("%1%2%3 %4 %5 %6 %5 %7").arg(wStr, times, hStr, unitStr, sep, dpiStr, fontFam);
    const QString shortStatusText =
            QStringLiteral("%1%2%3 %4 %5 %6 DPI").arg(wStr, times, hStr, unitStr, sep, dpiStr);
    const QString miniStatusText =
            QStringLiteral("%1%2%3%4 %5 %6 %5 %7").arg(wStr, times, hStr, unitStr, sep, dpiStr, fontFam);
    const QString microStatusText = QStringLiteral("%1%2%3 %4").arg(wStr, times, hStr, unitStr);
    const QString picoStatusText = QStringLiteral("%1 DPI").arg(dpiStr);

    const QString detailTipText =
            QStringLiteral("%1%2%3 %4 %5 %6 %5 %7 DPI %5 %8 %5 %9%")
                    .arg(wStr, times, hStr, unitStr, sep, aspectBadge, dpiStr, fontFam,
                         QString::number(zoomPct));

    d_canvas_status_pill->setProperty("_fullStatusText", fullStatusText);
    d_canvas_status_pill->setProperty("_compactStatusText", compactStatusText);
    d_canvas_status_pill->setProperty("_shortStatusText", shortStatusText);
    d_canvas_status_pill->setProperty("_miniStatusText", miniStatusText);
    d_canvas_status_pill->setProperty("_microStatusText", microStatusText);
    d_canvas_status_pill->setProperty("_picoStatusText", picoStatusText);
    d_canvas_status_pill->setText(fullStatusText);
    d_canvas_status_pill->setToolTip(
            tr("Page Size, Aspect Ratio, DPI, Font & Zoom — Click for Quick Presets")
            + QLatin1Char('\n') + detailTipText);
    repositionCanvasStatusPill();
}

void MultiLayer::repositionCanvasStatusPill()
{
    if (!widget())
        return;

    if (canvas) {
        if (QWidget *ov = widget()->findChild<QWidget *>(
                    QStringLiteral("MultiLayerPanelLabelOverlay"))) {
            ov->setGeometry(canvas->geometry());
            const bool showOv = d_panel_labels_enabled || !d_inset_specs.isEmpty();
            if (showOv) {
                ov->show();
                ov->raise();
            } else {
                ov->hide();
            }
        }
        if (d_drop_zone_overlay && d_drop_zone_overlay->isVisible()) {
            d_drop_zone_overlay->setGeometry(canvas->geometry());
            d_drop_zone_overlay->raise();
        }
    }

    if (!d_canvas_status_pill)
        return;

    const QSize hostSz = widget()->size();
    const QSize refSz = (hostSz.width() >= 140 && hostSz.height() >= 90) ? hostSz : size();
    bool anyExporting = property("isExporting").toBool();
    if (!anyExporting) {
        for (int i = 0; i < graphsList.count(); ++i) {
            if (const Graph *gr = qobject_cast<const Graph *>(graphsList.at(i))) {
                if (gr->isExporting()) {
                    anyExporting = true;
                    break;
                }
            }
        }
    }
    if (refSz.width() < 260 || refSz.height() < 200 || anyExporting
        || d_canvas_status_pill->text().trimmed().isEmpty()) {
        d_canvas_status_pill->hide();
        return;
    }

    int sbRight = 0;
    int sbBottom = 0;
    if (QScrollArea *cs = widget()->findChild<QScrollArea *>(
                QStringLiteral("MultiLayerCanvasScroll"))) {
        if (cs->verticalScrollBar() && cs->verticalScrollBar()->isVisible())
            sbRight = cs->verticalScrollBar()->width();
        if (cs->horizontalScrollBar() && cs->horizontalScrollBar()->isVisible())
            sbBottom = cs->horizontalScrollBar()->height();
    }
    const QString fullTxt = d_canvas_status_pill->property("_fullStatusText").toString();
    const QString compactTxt = d_canvas_status_pill->property("_compactStatusText").toString();
    const QString shortTxt = d_canvas_status_pill->property("_shortStatusText").toString();
    const QString miniTxt = d_canvas_status_pill->property("_miniStatusText").toString();
    const QString microTxt = d_canvas_status_pill->property("_microStatusText").toString();
    const QString picoTxt = d_canvas_status_pill->property("_picoStatusText").toString();
    auto measurePillWidth = [&](const QString &txt) -> int {
        const QFontMetrics fm(d_canvas_status_pill->font());
        const int textW = fm.horizontalAdvance(txt) + 26;
        d_canvas_status_pill->setText(txt);
        d_canvas_status_pill->adjustSize();
        const QSize sh = d_canvas_status_pill->sizeHint().expandedTo(
                d_canvas_status_pill->minimumSizeHint());
        return qMax(textW, sh.width());
    };
    if (!fullTxt.isEmpty() && d_canvas_status_pill->text() != fullTxt)
        d_canvas_status_pill->setText(fullTxt);

    d_canvas_status_pill->adjustSize();
    QSize pillSz =
            d_canvas_status_pill->sizeHint().expandedTo(d_canvas_status_pill->minimumSizeHint());
    const int fullW = !fullTxt.isEmpty() ? measurePillWidth(fullTxt) : pillSz.width();
    int pw = qMin(fullW, qMax(80, refSz.width() - 16 - sbRight));
    int ph = qMax(22, pillSz.height());
    int x = qMax(8, refSz.width() - pw - 10 - sbRight);
    int y = qMax(LayerButton::btnSize() + 8, refSz.height() - ph - 8 - sbBottom);

    // Always keep the status pill anchored consistently in the top-right header strip
    // aligned with the layer switcher pill (prevents jumping to bottom-right on compact presets).
    {
        int leftReserved = 8;
        if (d_layer_pill_frame && d_layer_pill_frame->isVisible()) {
            if (QLayout *pl = d_layer_pill_frame->layout())
                pl->activate();
            const int frameW = qMax(d_layer_pill_frame->width(),
                                    d_layer_pill_frame->sizeHint().width());
            const QWidget *pillParent = d_canvas_status_pill->parentWidget();
            const int leftEdge = (pillParent && pillParent->isAncestorOf(d_layer_pill_frame))
                    ? d_layer_pill_frame->mapTo(pillParent, QPoint(0, 0)).x()
                    : d_layer_pill_frame->x();
            leftReserved = qMax(8, leftEdge + frameW + 10);
        }
        const int availTopW = refSz.width() - leftReserved - 8 - sbRight;
        if (availTopW < 64) {
            d_canvas_status_pill->hide();
            return;
        }
        if (availTopW < fullW) {
            QString chosen = fullTxt;
            int chosenW = fullW;
            for (const QString &cand : { compactTxt, shortTxt, miniTxt, microTxt, picoTxt }) {
                if (cand.isEmpty())
                    continue;
                chosen = cand;
                chosenW = measurePillWidth(cand);
                if (chosenW <= availTopW)
                    break;
            }
            if (chosenW > availTopW && !chosen.isEmpty()) {
                const QFontMetrics fm(d_canvas_status_pill->font());
                const QString rightElided = fm.elidedText(chosen, Qt::ElideRight, qMax(36, availTopW - 26));
                chosenW = measurePillWidth(rightElided);
            }
            pw = qMin(chosenW, availTopW);
            pillSz = d_canvas_status_pill->sizeHint().expandedTo(
                    d_canvas_status_pill->minimumSizeHint());
            ph = qMax(20, qMin(LayerButton::btnSize(), pillSz.height()));
        } else {
            pw = fullW;
            ph = qMax(20, qMin(LayerButton::btnSize(), ph));
        }
        x = qMax(leftReserved, refSz.width() - pw - 8 - sbRight);
        y = qMax(1, (LayerButton::btnSize() + 4 - ph) / 2);
    }

    d_canvas_status_pill->setGeometry(x, y, pw, ph);
    if (!d_canvas_status_pill->isVisible())
        d_canvas_status_pill->show();
    d_canvas_status_pill->raise();
}

void MultiLayer::openCanvasStatusQuickMenu()
{
    if (d_canvas_status_pill && d_canvas_status_pill->menu())
        d_canvas_status_pill->showMenu();
}

void MultiLayer::applySquareCanvasFrame70Percent()
{
    if (graphs <= 0 || graphsList.isEmpty())
        return;

    QList<Graph *> primaryGraphs;
    primaryGraphs.reserve(graphs);
    for (int k = 0; k < graphsList.count(); ++k) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(k));
        if (!gr || !gr->plotWidget())
            continue;
        if (gr->linkedParentLayer() || d_inset_specs.contains(gr))
            continue;
        primaryGraphs.append(gr);
    }
    if (primaryGraphs.isEmpty())
        return;

    if (isVisible() && widget() && widget()->layout())
        widget()->layout()->activate();
    if (d_page_unit != UnitPx || d_page_view_mode || qAbs(d_page_zoom - 1.0) > 1e-4) {
        syncMultiLayerPageHost(this, widget(), canvas, d_page_unit, d_page_view_mode,
                               d_base_page_px, d_page_zoom, d_page_bg_color);
    }

    QSize pageSz = (canvas && canvas->width() >= 100 && canvas->height() >= 100)
            ? canvas->size()
            : QSize(qMax(160, qRound(d_base_page_px.width() * d_page_zoom)),
                    qMax(160, qRound(d_base_page_px.height() * d_page_zoom)));
    if (pageSz.width() < 120 || pageSz.height() < 120)
        pageSz = QSize(520, 520);

    const int nPrimary = primaryGraphs.size();
    const int nRows = qMax(1, rows);
    const int nCols = qMax(1, cols);
    const int outerPad = qMax(4, qMin(left_margin, top_margin));
    const int gapX = (nCols > 1) ? qMax(0, colsSpace) : 0;
    const int gapY = (nRows > 1) ? qMax(0, rowsSpace) : 0;
    const int panelTopExtra = computeOutsidePanelTopExtra(
            primaryGraphs, d_panel_labels_enabled, d_panel_label_placement, d_panel_label_font_pt);

    const bool prevSync = d_syncing_inset_geometries;
    d_syncing_inset_geometries = true;

    QVector<bool> savedIgnore(graphsList.count(), false);
    QVector<bool> savedAutoFonts(graphsList.count(), false);
    for (int k = 0; k < graphsList.count(); ++k) {
        if (Graph *gr = qobject_cast<Graph *>(graphsList.at(k))) {
            savedIgnore[k] = gr->ignoresResizeEvents();
            savedAutoFonts[k] = gr->autoscaleFonts();
            gr->setIgnoreResizeEvents(true);
            gr->setAutoscaleFonts(false);
        }
    }

    int targetSide = qMax(60, qRound(0.70 * qMin(double(pageSz.width()) / nCols,
                                                 double(pageSz.height()) / nRows)));

    auto capMultiPanelTicks = [&](int side) {
        const double z = qMax(0.25, d_page_zoom);
        const int baseSide = qMax(60, qRound(double(side) / z));
        const bool isMultiPanel = (nRows >= 2 || nCols >= 2);
        const int capX = isMultiPanel ? qBound(3, baseSide / 38, 6) : qBound(4, baseSide / 42, 6);
        const int capY = isMultiPanel ? qBound(3, baseSide / 28, 6) : 10;
        for (Graph *gr : primaryGraphs) {
            Plot *pl = gr ? gr->plotWidget() : nullptr;
            if (!pl || gr->isTernaryPlot() || gr->isPolarPlot() || gr->isPoleFigurePlot()
                || gr->isPiePlot())
                continue;
            const QwtTextScaleDraw *xCatSd =
                    dynamic_cast<const QwtTextScaleDraw *>(pl->axisScaleDraw(QwtPlot::xBottom));
            const bool xIsDiscreteOrCustom =
                    (xCatSd != nullptr) || gr->hasCustomTickLabels(QwtPlot::xBottom);
            bool xLabelsCrowded = false;
            int safeXCaps = capX;
            if (const QwtScaleDiv *xsd = pl->axisScaleDiv(QwtPlot::xBottom)) {
                const QwtValueList mTicks = xsd->ticks(QwtScaleDiv::MajorTick);
                if (mTicks.size() >= 3) {
                    const QFontMetrics xfm(gr->axisFont(QwtPlot::xBottom));
                    int sumW = 0;
                    for (int ti = 0; ti < mTicks.size(); ++ti) {
                        const QString lbl = pl->axisScaleDraw(QwtPlot::xBottom)
                                ? pl->axisScaleDraw(QwtPlot::xBottom)->label(mTicks[ti]).text()
                                : QString::number(mTicks[ti], 'g', 6);
                        sumW += xfm.horizontalAdvance(lbl) + 8;
                    }
                    if (xIsDiscreteOrCustom) {
                        if (xCatSd && gr->labelsRotation(QwtPlot::xBottom) == 0
                            && sumW > qRound(0.84 * double(baseSide))) {
                            gr->setAxisLabelRotation(QwtPlot::xBottom, -35);
                            pl->updateLayout();
                        }
                    } else {
                        const double crowdThresh =
                                (gr->axisStep(QwtPlot::xBottom) > 0.0) ? 0.90 : 0.82;
                        if (sumW > qRound(crowdThresh * double(baseSide))) {
                            xLabelsCrowded = true;
                            const int avgCell = qMax(22, (sumW / mTicks.size()) + 4);
                            safeXCaps = qBound(
                                    2, qRound(0.80 * double(baseSide) / double(avgCell)), 5);
                        }
                    }
                }
            }
            bool yLabelsCrowded = false;
            int safeYCaps = capY;
            if (!gr->axisBreak(QwtPlot::yLeft).enabled && !gr->hasColorScaleOnAxis(QwtPlot::yLeft)
                && !gr->hasWeibullProbabilityAxis(QwtPlot::yLeft)
                && !gr->hasCustomTickLabels(QwtPlot::yLeft)) {
                const QFont yTickFnt = gr->axisFont(QwtPlot::yLeft);
                const QFontMetricsF yfm(yTickFnt);
                const double yPt = (yTickFnt.pointSizeF() > 0.0)
                        ? yTickFnt.pointSizeF()
                        : double(qMax(1, yTickFnt.pointSize()));
                const int yLineH = qMax(10, static_cast<int>(std::ceil(yfm.height())));
                const QwtScaleDiv *ysd = pl->axisScaleDiv(QwtPlot::yLeft);
                const int yTickCnt = ysd ? ysd->ticks(QwtScaleDiv::MajorTick).size() : 0;
                const int reqH = yTickCnt * qMax(16, qRound(double(yLineH) * 1.65));
                const int minStepPx = qMax(24, qRound(double(yLineH) * 1.95));
                const int estYCap = qBound(3, qRound(double(baseSide) / double(minStepPx)), 6);
                if ((yPt >= 16.0 && yTickCnt > estYCap + 1)
                    || (yTickCnt >= 7 && reqH > qRound(0.95 * double(baseSide)))) {
                    yLabelsCrowded = true;
                    safeYCaps = estYCap;
                }
            }
            if (!isMultiPanel && !xLabelsCrowded && !yLabelsCrowded)
                continue;
            const int axesToCap[2] = { QwtPlot::xBottom, QwtPlot::yLeft };
            const bool capAxis[2] = { isMultiPanel || xLabelsCrowded,
                                      isMultiPanel || yLabelsCrowded };
            const int caps[2] = { xLabelsCrowded ? qMin(capX, safeXCaps) : capX,
                                  yLabelsCrowded ? qMin(capY, safeYCaps) : capY };
            bool changedAny = false;
            for (int ai = 0; ai < 2; ++ai) {
                if (!capAxis[ai])
                    continue;
                const int a = axesToCap[ai];
                const int cap = caps[ai];
                if (gr->hasColorScaleOnAxis(a) || gr->hasWeibullProbabilityAxis(a)
                    || gr->hasCustomTickLabels(a)
                    || dynamic_cast<const QwtTextScaleDraw *>(pl->axisScaleDraw(a))) {
                    continue;
                }
                if (pl->axisAutoScale(a)) {
                    if (pl->axisMaxMajor(a) > cap) {
                        pl->setAxisMaxMajor(a, cap);
                        changedAny = true;
                    }
                    continue;
                }
                const bool forceUnstepX = (a == QwtPlot::xBottom && xLabelsCrowded);
                if (!gr->axisBreak(a).enabled && (gr->axisStep(a) <= 0.0 || forceUnstepX)) {
                    const QwtScaleEngine *se = pl->axisScaleEngine(a);
                    const QwtScaleDiv *sd = pl->axisScaleDiv(a);
                    const bool tooManyTicks =
                            sd && sd->ticks(QwtScaleDiv::MajorTick).size() > cap + 1;
                    if (se && sd && !Plot::isLog10ScaleEngine(se)
                        && (tooManyTicks || forceUnstepX
                            || (a == QwtPlot::xBottom && pl->axisMaxMajor(a) > cap))) {
                        const double lo = qMin(sd->lowerBound(), sd->upperBound());
                        const double hi = qMax(sd->lowerBound(), sd->upperBound());
                        const bool isInv = se->testAttribute(QwtScaleEngine::Inverted);
                        const int minT = pl->axisMaxMinor(a);
                        const int maxMinInt = (minT >= 1) ? (minT + 1) : 0;
                        if (pl->axisMaxMajor(a) > cap)
                            pl->setAxisMaxMajor(a, cap);
                        QwtLinearScaleEngine *newEng = new QwtLinearScaleEngine();
                        QwtScaleDiv newDiv = newEng->divideScale(lo, hi, cap, maxMinInt, 0.0);
                        if (minT == 1 && newDiv.ticks(QwtScaleDiv::MinorTick).isEmpty()
                            && newDiv.ticks(QwtScaleDiv::MediumTick).isEmpty()) {
                            const QwtValueList mTicks = newDiv.ticks(QwtScaleDiv::MajorTick);
                            if (mTicks.size() >= 2) {
                                const double s = std::fabs(mTicks[1] - mTicks[0]);
                                const double half = 0.5 * s;
                                const double dLo = qMin(newDiv.lowerBound(), newDiv.upperBound());
                                const double dHi = qMax(newDiv.lowerBound(), newDiv.upperBound());
                                const double eps = 1e-9 * s;
                                double fM = mTicks.first();
                                double lM = mTicks.last();
                                if (fM > lM)
                                    qSwap(fM, lM);
                                QwtValueList mids;
                                if (fM - half >= dLo - eps)
                                    mids += (fM - half);
                                for (int mi = 0; mi + 1 < mTicks.size(); ++mi) {
                                    const double mv = 0.5 * (mTicks[mi] + mTicks[mi + 1]);
                                    if (mv >= dLo - eps && mv <= dHi + eps)
                                        mids += mv;
                                }
                                if (lM + half <= dHi + eps)
                                    mids += (lM + half);
                                if (!mids.isEmpty())
                                    newDiv.setTicks(QwtScaleDiv::MediumTick, mids);
                            }
                        }
                        if (isInv) {
                            newEng->setAttribute(QwtScaleEngine::Inverted);
                            newDiv.invert();
                        }
                        pl->setAxisScale(a, isInv ? hi : lo, isInv ? lo : hi, 0.0);
                        pl->setAxisScaleEngine(a, newEng);
                        pl->setAxisScaleDiv(a, newDiv);
                        changedAny = true;
                    }
                }
            }
            if (changedAny) {
                pl->updateAxes();
                gr->updateSecondaryAxis(QwtPlot::xTop);
                gr->updateSecondaryAxis(QwtPlot::yRight);
                gr->syncChildLinkedLayers();
            }
        }
    };

    capMultiPanelTicks(targetSide);

    for (int iter = 0; iter < 8; ++iter) {
        QVector<int> ownL(nPrimary, 0), ownR(nPrimary, 0), ownT(nPrimary, 0), ownB(nPrimary, 0);
        QVector<int> reqL(nPrimary, 0), reqR(nPrimary, 0), reqT(nPrimary, 0), reqB(nPrimary, 0);
        QVector<int> colMaxL(nCols, 0), colMaxR(nCols, 0);
        QVector<int> rowMaxT(nRows, 0), rowMaxB(nRows, 0);

        auto measureExactAndBudgetChrome = [](Graph *g, int &exactL, int &exactR, int &exactT,
                                              int &exactB, int &budgetL, int &budgetR, int &budgetT,
                                              int &budgetB) {
            exactL = exactR = exactT = exactB = 0;
            budgetL = budgetR = budgetT = budgetB = 4;
            Plot *pl = g ? g->plotWidget() : nullptr;
            if (!pl)
                return;
            pl->updateLayout();
            const QRect cr = pl->plotLayout()->canvasRect();
            const bool validLayout = (cr.width() > 0 && cr.height() > 0 && pl->width() >= 20
                                      && pl->height() >= 20);
            if (validLayout) {
                exactL = qMax(0, cr.left());
                exactR = qMax(0, pl->width() - (cr.left() + cr.width()));
                exactT = qMax(0, cr.top());
                exactB = qMax(0, pl->height() - (cr.top() + cr.height()));
            } else {
                if (pl->axisEnabled(QwtPlot::yLeft) && pl->axisWidget(QwtPlot::yLeft))
                    exactL = qMax(0, pl->axisWidget(QwtPlot::yLeft)->minimumSizeHint().width());
                if (pl->axisEnabled(QwtPlot::yRight) && pl->axisWidget(QwtPlot::yRight))
                    exactR = qMax(0, pl->axisWidget(QwtPlot::yRight)->minimumSizeHint().width());
                if (pl->axisEnabled(QwtPlot::xTop) && pl->axisWidget(QwtPlot::xTop))
                    exactT = qMax(0, pl->axisWidget(QwtPlot::xTop)->minimumSizeHint().height());
                if (pl->axisEnabled(QwtPlot::xBottom) && pl->axisWidget(QwtPlot::xBottom))
                    exactB = qMax(0, pl->axisWidget(QwtPlot::xBottom)->minimumSizeHint().height());
            }

            budgetL = qMax(4, exactL + 4);
            budgetR = qMax(4, exactR + 4);
            budgetT = qMax(4, exactT + 4);
            budgetB = qMax(4, exactB + 4);

            if (pl->axisEnabled(QwtPlot::yLeft)) {
                if (const QwtScaleWidget *sw = pl->axisWidget(QwtPlot::yLeft))
                    budgetL = qMax(budgetL, sw->minimumSizeHint().width() + 4);
            }
            if (pl->axisEnabled(QwtPlot::yRight)) {
                if (const QwtScaleWidget *sw = pl->axisWidget(QwtPlot::yRight))
                    budgetR = qMax(budgetR, sw->minimumSizeHint().width() + 4);
            }
            if (pl->axisEnabled(QwtPlot::xTop)) {
                if (const QwtScaleWidget *sw = pl->axisWidget(QwtPlot::xTop))
                    budgetT = qMax(budgetT, sw->minimumSizeHint().height() + 4);
            }
            if (pl->axisEnabled(QwtPlot::xBottom)) {
                if (const QwtScaleWidget *sw = pl->axisWidget(QwtPlot::xBottom))
                    budgetB = qMax(budgetB, sw->minimumSizeHint().height() + 4);
            }
        };

        for (int i = 0; i < nPrimary; ++i) {
            Graph *gr = primaryGraphs.at(i);
            measureExactAndBudgetChrome(gr, ownL[i], ownR[i], ownT[i], ownB[i], reqL[i], reqR[i],
                                        reqT[i], reqB[i]);
            reqT[i] += panelTopExtra;

            // Include linked Double-Y / Top-X child overlay extents in outer margin budgets only!
            for (int k = 0; k < graphsList.count(); ++k) {
                Graph *ch = qobject_cast<Graph *>(graphsList.at(k));
                if (!ch || ch->linkedParentLayer() != gr)
                    continue;
                int ceL = 0, ceR = 0, ceT = 0, ceB = 0;
                int cbL = 0, cbR = 0, cbT = 0, cbB = 0;
                measureExactAndBudgetChrome(ch, ceL, ceR, ceT, ceB, cbL, cbR, cbT, cbB);
                reqL[i] = qMax(reqL[i], cbL);
                reqR[i] = qMax(reqR[i], cbR);
                reqT[i] = qMax(reqT[i], cbT + panelTopExtra);
                reqB[i] = qMax(reqB[i], cbB);
            }

            const int r = qMin(nRows - 1, i / nCols);
            const int c = i % nCols;
            colMaxL[c] = qMax(colMaxL[c], reqL[i]);
            colMaxR[c] = qMax(colMaxR[c], reqR[i]);
            rowMaxT[r] = qMax(rowMaxT[r], reqT[i]);
            rowMaxB[r] = qMax(rowMaxB[r], reqB[i]);
        }

        int sumChromeW = 0;
        for (int c = 0; c < nCols; ++c)
            sumChromeW += colMaxL[c] + colMaxR[c];
        int sumChromeH = 0;
        for (int r = 0; r < nRows; ++r)
            sumChromeH += rowMaxT[r] + rowMaxB[r];

        const int cellBudgetW = (pageSz.width() - 2 * outerPad - (nCols - 1) * gapX) / nCols;
        const int cellBudgetH = (pageSz.height() - 2 * outerPad - (nRows - 1) * gapY) / nRows;
        const int ideal70 = qRound(0.70 * qMin(cellBudgetW, cellBudgetH));

        const int maxFitW =
                (pageSz.width() - 2 * outerPad - (nCols - 1) * gapX - sumChromeW) / nCols;
        const int maxFitH =
                (pageSz.height() - 2 * outerPad - (nRows - 1) * gapY - sumChromeH) / nRows;

        const int nextSide = qMax(50, qMin(ideal70, qMin(maxFitW, maxFitH)));
        if (iter == 0 && nextSide != targetSide)
            capMultiPanelTicks(nextSide);

        QVector<int> colOriginX(nCols, 0);
        int accW = 0;
        for (int c = 0; c < nCols; ++c) {
            colOriginX[c] = accW;
            accW += colMaxL[c] + nextSide + colMaxR[c];
        }
        QVector<int> rowOriginY(nRows, 0);
        int accH = 0;
        for (int r = 0; r < nRows; ++r) {
            rowOriginY[r] = accH;
            accH += rowMaxT[r] + nextSide + rowMaxB[r];
        }

        const int totalBlockW = accW + (nCols - 1) * gapX;
        const int totalBlockH = accH + (nRows - 1) * gapY;
        const int startX = qMax(outerPad, (pageSz.width() - totalBlockW) / 2);
        const int startY = qMax(outerPad, (pageSz.height() - totalBlockH) / 2);

        bool allCanvasesConverged = true;
        for (int i = 0; i < nPrimary; ++i) {
            Graph *gr = primaryGraphs.at(i);
            Plot *pl = gr->plotWidget();
            const int r = qMin(nRows - 1, i / nCols);
            const int c = i % nCols;

            // Align every cell's inner square canvas at (innerX, innerY)
            const int innerX = startX + c * gapX + colOriginX[c] + colMaxL[c];
            const int innerY = startY + r * gapY + rowOriginY[r] + rowMaxT[r];
            const int gx = innerX - ownL[i];
            const int gy = innerY - ownT[i];
            const int gw = ownL[i] + nextSide + ownR[i];
            const int gh = ownT[i] + nextSide + ownB[i];

            gr->setGeometry(QRect(gx, gy, gw, gh));
            pl->resize(QSize(gw, gh));
            pl->updateLayout();

            const QRect crNow = pl->plotLayout()->canvasRect();
            if (qAbs(crNow.width() - nextSide) > 1 || qAbs(crNow.height() - nextSide) > 1)
                allCanvasesConverged = false;
        }

        syncChildOverlayAndInsetGeometries();
        if (iter >= 2 && qAbs(nextSide - targetSide) <= 1 && allCanvasesConverged) {
            targetSide = nextSide;
            break;
        }
        targetSide = nextSide;
    }

    // 1-step residual trim so every primary plot's canvasRect() is exact targetSide x targetSide
    for (int i = 0; i < nPrimary; ++i) {
        Graph *gr = primaryGraphs.at(i);
        Plot *pl = gr ? gr->plotWidget() : nullptr;
        if (!pl)
            continue;
        pl->updateLayout();
        const QRect crNow = pl->plotLayout()->canvasRect();
        if (crNow.width() > 0 && crNow.height() > 0
            && (crNow.width() != targetSide || crNow.height() != targetSide)) {
            QRect geo = gr->geometry();
            const int gw = qMax(40, geo.width() + (targetSide - crNow.width()));
            const int gh = qMax(40, geo.height() + (targetSide - crNow.height()));
            gr->setGeometry(QRect(geo.x(), geo.y(), gw, gh));
            pl->resize(QSize(gw, gh));
            pl->updateLayout();
        }
    }

    l_canvas_width = targetSide;
    l_canvas_height = targetSide;
    d_keep_aspect_ratio = true;
    d_target_aspect_ratio = 1.0;

    for (int k = 0; k < graphsList.count(); ++k) {
        if (Graph *gr = qobject_cast<Graph *>(graphsList.at(k))) {
            gr->setAutoscaleFonts(savedAutoFonts[k]);
            gr->setIgnoreResizeEvents(savedIgnore[k]);
            if (gr->plotWidget())
                gr->plotWidget()->replot();
        }
    }

    syncChildOverlayAndInsetGeometries();
    d_syncing_inset_geometries = prevSync;

    for (int i = 0; i < nPrimary; ++i) {
        Graph *gr = primaryGraphs.at(i);
        if (!gr || !gr->plotWidget())
            continue;
        clampGraphCalloutMarkersInCanvas(gr);
        if (!gr->legend() || !gr->isLegendVisible())
            continue;
        const QVariant ancProp = gr->plotWidget()->property("_toriLegendAnchor");
        if (ancProp.isValid()) {
            gr->placeLegendAnchor(static_cast<Graph::LegendAnchor>(ancProp.toInt()));
            gr->plotWidget()->updateLayout();
            gr->plotWidget()->replot();
        }
    }

    repositionCanvasStatusPill();
    emit modifiedPlot();
}
