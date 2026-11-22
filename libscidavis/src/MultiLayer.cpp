/***************************************************************************
    File                 : MultiLayer.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Multi layer widget

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
#include <QSpinBox>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#include <QVBoxLayout>

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
#include "Spectrogram.h"
#include "GraphInspectorDock.h"
#include "GraphMiniToolbar.h"
#include "CanvasInlineTextEditor.h"
#include "ModernRibbonAndHud.h"
#include "ModernUiTheme.h"
#include "I18nJapanese.h"
#include <cmath>
#include "OriginGadgets.h"

namespace {
ApplicationWindow *findMultiLayerApp(QWidget *from)
{
    for (QWidget *w = from; w != nullptr; w = w->parentWidget()) {
        if (auto *app = qobject_cast<ApplicationWindow *>(w))
            return app;
    }
    if (qApp) {
        const QWidgetList tops = QApplication::topLevelWidgets();
        for (QWidget *tw : tops) {
            if (auto *app = qobject_cast<ApplicationWindow *>(tw))
                return app;
        }
    }
    return nullptr;
}

bool isPresetManagedValue(double v, const QVector<int> &managed)
{
    for (int m : managed) {
        if (std::fabs(v - double(m)) < 0.05)
            return true;
    }
    return false;
}

bool isPresetManagedValue(double v, const QVector<double> &managed)
{
    for (double m : managed) {
        if (std::fabs(v - m) < 0.05)
            return true;
    }
    return false;
}
} // namespace
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

LayerButton::LayerButton(const QString &text, QWidget *parent) : QPushButton(text, parent)
{
    int btn_size = LayerButton::btnSize();

    setCheckable(true);
    setChecked(true);
    setMinimumWidth(btn_size);
    setFixedHeight(btn_size);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    setFont(QFont("Arial", 10, QFont::Bold, false));
    setStyleSheet(
            "QPushButton { background-color: transparent; color: #334155; font-size: 10.8pt; "
            "border: 1px solid transparent; border-radius: 5px; padding: 0px 6px; margin: 0px; }"
            "QPushButton:hover { background-color: #EFF6FF; color: #1D4ED8; "
            "border: 1px solid #BFDBFE; }"
            "QPushButton:checked { background-color: #2563EB; color: #FFFFFF; "
            "border: 1px solid #1D4ED8; }");
}

void LayerButton::mousePressEvent(QMouseEvent *event)
{
    if (!isChecked())
        emit clicked(this);
    if (event->button() == Qt::RightButton) {
        emit showContextMenu();
        event->accept();
        return;
    }
    QPushButton::mousePressEvent(event);
}

void LayerButton::mouseDoubleClickEvent(QMouseEvent *)
{
    emit showCurvesDialog();
}

namespace {

class CustomPaintWidget : public QWidget
{
public:
    std::function<void(QPainter &)> onPaint;
    explicit CustomPaintWidget(QWidget *parent = nullptr) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent *ev) override
    {
        QWidget::paintEvent(ev);
        if (onPaint) {
            QPainter p(this);
            onPaint(p);
        }
    }
};

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

} // namespace

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
        ml->setPageZoom(1.0);
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
        ml->setPageZoom(prevZoom);
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

MultiLayer::MultiLayer(const QString &label, QWidget *parent, const QString name, Qt::WindowFlags f)
    : MyWidget(label, parent, name, f)
{
    if (name.isEmpty())
        setObjectName("multilayer plot");

    QDateTime dt = QDateTime::currentDateTime();
    setBirthDate(QLocale().toString(dt));

    graphs = 0;
    cols = 1;
    rows = 1;
    graph_width = 520;
    graph_height = 520;
    colsSpace = 5;
    rowsSpace = 5;
    left_margin = 5;
    right_margin = 5;
    top_margin = 5;
    bottom_margin = 5;
    l_canvas_width = 360;
    l_canvas_height = 360;
    lastSize = QSize(-1, -1);
    hor_align = HCenter;
    vert_align = VCenter;
    active_graph = 0;
    addTextOn = false;
    d_scale_on_print = true;
    d_print_cropmarks = false;
    d_keep_aspect_ratio = true;
    d_target_aspect_ratio = 1.0;

    defaultTextMarkerFrame = 1;
    defaultTextMarkerFont = QFont("Arial", 12, QFont::Normal, false);
    defaultTextMarkerColor = QColor(Qt::black);
    defaultTextMarkerBackground = QColor(Qt::white);

    d_page_view_mode = false;
    d_page_zoom = 1.0;
    d_page_unit = UnitPx;
    d_page_dpi = 300;
    d_page_width_val = graph_width;
    d_page_height_val = graph_height;
    d_base_page_px = QSize(graph_width, graph_height);
    d_page_bg_color = QColor(Qt::white);

    d_undo_stack = new QUndoStack(this);
    d_undo_stack->setUndoLimit(50);
    setAcceptDrops(true);

    layerButtonsBox = new QHBoxLayout();
    layerButtonsBox->setContentsMargins(0, 0, 0, 0);
    layerButtonsBox->setSpacing(2);

    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->setContentsMargins(6, 3, 6, 2);
    hbox->setSpacing(4);

    d_layer_pill_frame = new QFrame();
    d_layer_pill_frame->setObjectName(QStringLiteral("MultiLayerPillContainer"));
    d_layer_pill_frame->setStyleSheet(QStringLiteral(
            "QFrame#MultiLayerPillContainer {"
            "  background-color: #F8FAFC;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 7px;"
            "}"));
    QHBoxLayout *pillLayout = new QHBoxLayout(d_layer_pill_frame);
    pillLayout->setContentsMargins(3, 2, 3, 2);
    pillLayout->setSpacing(2);

    QWidget *strut = new QWidget(d_layer_pill_frame);
    strut->setFixedSize(0, LayerButton::btnSize());
    pillLayout->addWidget(strut);
    pillLayout->addLayout(layerButtonsBox);

    d_add_layer_btn = new QToolButton(d_layer_pill_frame);
    d_add_layer_btn->setText(QStringLiteral("+"));
    d_add_layer_btn->setToolTip(tr("Add Layer"));
    d_add_layer_btn->setFont(QFont("Arial", 10, QFont::Bold, false));
    d_add_layer_btn->setFixedSize(LayerButton::btnSize(), LayerButton::btnSize());
    d_add_layer_btn->setPopupMode(QToolButton::InstantPopup);
    d_add_layer_btn->setStyleSheet(QStringLiteral(
            "QToolButton { background-color: #FFFFFF; color: #2563EB; "
            "border: 1px solid #CBD5E1; border-radius: 5px; padding: 0px; margin: 0px; }"
            "QToolButton:hover { background-color: #EFF6FF; color: #1D4ED8; "
            "border: 1px solid #2563EB; }"
            "QToolButton::menu-indicator { image: none; width: 0px; }"));
    QMenu *addMenu = new QMenu(d_add_layer_btn);
    auto populateAddLayerMenu = [this, addMenu]() {
        addMenu->clear();
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-double-y"), 18, false),
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
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("layer-linked"), 18, false),
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
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("layer-linked"), 18, false),
                           tr("Add Linked Top-X / Right-Y Layer"), this, [this]() {
            const QString snap = captureUndoSnapshot();
            Graph *g = addLinkedLayer(activeGraph(), true, true, QString("x"), QString("y"));
            if (g)
                setActiveGraph(g);
            refreshLayerButtonBadges();
            pushUndoSnapshot(tr("Add Linked Layer"), snap);
        });
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("layer-inset"), 18, false),
                           tr("Add Zoom Inset Layer"), this, [this]() {
            const QString snap = captureUndoSnapshot();
            addInsetLayer(activeGraph());
            refreshLayerButtonBadges();
            pushUndoSnapshot(tr("Add Inset Layer"), snap);
        });
        addMenu->addSeparator();
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("layer-add"), 18, false),
                           tr("Add Empty Layer"), this, [this]() {
            const QString snap = captureUndoSnapshot();
            Graph *g = addLayer();
            if (g)
                arrangeLayers(true, false);
            refreshLayerButtonBadges();
            pushUndoSnapshot(tr("Add Layer"), snap);
        });
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("layer-arrange"), 18, false),
                           tr("Layer Management..."), this, [this]() {
            LayerDialog dlg(this);
            dlg.setMultiLayer(this);
            dlg.exec();
        });
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("page-setup"), 18, false),
                           tr("Page Setup..."), this, [this]() { showPageSetupDialog(); });
    };
    populateAddLayerMenu();
    connect(addMenu, &QMenu::aboutToShow, this, populateAddLayerMenu);
    d_add_layer_btn->setMenu(addMenu);
    pillLayout->addWidget(d_add_layer_btn);

    d_quick_columns_btn = new QToolButton(d_layer_pill_frame);
    d_quick_columns_btn->setText(tr("Plotted Columns") + QStringLiteral(" \u25BE"));
    d_quick_columns_btn->setToolTip(tr("Toggle Plotted Columns on Active Layer"));
    d_quick_columns_btn->setFocusPolicy(Qt::NoFocus);
    d_quick_columns_btn->setFixedHeight(LayerButton::btnSize());
    d_quick_columns_btn->setStyleSheet(QStringLiteral(
            "QToolButton { background-color: #FFFFFF; color: #334155; "
            "font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif; "
            "font-size: 10.8pt; font-weight: 600; "
            "border: 1px solid #CBD5E1; border-radius: 5px; padding: 0px 8px; margin: 0px; }"
            "QToolButton:hover { background-color: #EFF6FF; color: #1D4ED8; "
            "border: 1px solid #2563EB; }"));
    connect(d_quick_columns_btn, &QToolButton::clicked, this,
            &MultiLayer::openQuickLayerColumnsPopup);
    pillLayout->addWidget(d_quick_columns_btn);

    hbox->addWidget(d_layer_pill_frame);
    hbox->addStretch();

    QWidget *d_main_widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout();
    d_main_widget->setLayout(layout);

    QScrollArea *canvasScroll = new QScrollArea(d_main_widget);
    canvasScroll->setObjectName(QStringLiteral("MultiLayerCanvasScroll"));
    canvasScroll->setFrameShape(QFrame::NoFrame);
    canvasScroll->setAlignment(Qt::AlignCenter);
    canvasScroll->setFocusPolicy(Qt::NoFocus);

    CustomPaintWidget *pageHost = new CustomPaintWidget(canvasScroll);
    pageHost->setObjectName(QStringLiteral("MultiLayerPageHost"));
    pageHost->setAutoFillBackground(true);
    pageHost->resize(graph_width, graph_height);
    pageHost->onPaint = [this](QPainter &p) {
        const bool freePixelMode =
                (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4);
        if (!freePixelMode && canvas && canvas->isVisible()) {
            p.setRenderHint(QPainter::Antialiasing, false);
            p.setPen(QPen(QColor(QStringLiteral("#CBD5E1")), 1, Qt::SolidLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(canvas->geometry().adjusted(-1, -1, 0, 0));
        }
    };

    canvas = new QWidget(pageHost);
    canvas->setParent(pageHost);
    canvas->setAutoFillBackground(true);
    {
        QPalette cpal = canvas->palette();
        cpal.setColor(QPalette::Window, QColor("#FFFFFF"));
        canvas->setPalette(cpal);
    }
    canvas->setGeometry(0, 0, graph_width, graph_height);
    canvas->installEventFilter(this);

    // Sibling transparent overlay for on-screen panel (a),(b) labels (never a child of canvas!)
    CustomPaintWidget *panelLabelOverlay = new CustomPaintWidget(pageHost);
    panelLabelOverlay->setObjectName(QStringLiteral("MultiLayerPanelLabelOverlay"));
    panelLabelOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    panelLabelOverlay->setAttribute(Qt::WA_NoSystemBackground, true);
    panelLabelOverlay->hide();
    panelLabelOverlay->onPaint = [this](QPainter &p) {
        if (d_panel_labels_enabled || !d_inset_specs.isEmpty()) {
            p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);
            drawInsetConnectors(&p, QPoint(0, 0));
            drawPanelLabels(&p, QPoint(0, 0));
        }
    };

    // Non-exported Drag-and-Drop Column Preview Overlay (sibling of canvas)
    CustomPaintWidget *dropZoneOverlay = new CustomPaintWidget(pageHost);
    d_drop_zone_overlay = dropZoneOverlay;
    d_drop_zone_overlay->setObjectName(QStringLiteral("MultiLayerDropZoneOverlay"));
    d_drop_zone_overlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    d_drop_zone_overlay->setAttribute(Qt::WA_NoSystemBackground, true);
    d_drop_zone_overlay->hide();
    dropZoneOverlay->onPaint = [this](QPainter &p) {
        if (!d_drop_zone_overlay || !d_drop_zone_overlay->isVisible())
            return;
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);
        const bool rightZone = d_drop_zone_overlay->property("rightYZone").toBool();
        const QRect targetRect = d_drop_zone_overlay->property("targetRect").toRect();
        const QRect zoneRect = targetRect.isValid() ? targetRect : d_drop_zone_overlay->rect();
        const QColor accent(0x0F, 0x6C, 0xBD);
        if (rightZone) {
            const int stripW = qMin(44, qMax(24, zoneRect.width() / 5));
            const QRect rightStrip(zoneRect.right() - stripW + 1, zoneRect.top(), stripW,
                                   zoneRect.height());
            p.fillRect(rightStrip, QColor(accent.red(), accent.green(), accent.blue(), 46));
            p.setPen(QPen(accent, 2.0, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(rightStrip).adjusted(1.0, 1.0, -1.0, -1.0), 4.0, 4.0);
        } else {
            p.fillRect(zoneRect, QColor(accent.red(), accent.green(), accent.blue(), 22));
            p.setPen(QPen(accent, 2.0, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(zoneRect).adjusted(2.0, 2.0, -2.0, -2.0), 6.0, 6.0);
        }
        const QString banner = d_drop_zone_overlay->property("bannerText").toString();
        if (!banner.isEmpty()) {
            QFont bf(QStringLiteral("Segoe UI"), 9, QFont::DemiBold);
            p.setFont(bf);
            const QFontMetrics fm(bf);
            const int bw = qMin(d_drop_zone_overlay->width() - 16,
                                fm.horizontalAdvance(banner) + 28);
            const int bh = qMax(26, fm.height() + 10);
            const QRect pillRect((d_drop_zone_overlay->width() - bw) / 2, 10, bw, bh);
            p.setPen(QPen(accent.darker(115), 1.2));
            p.setBrush(accent);
            p.drawRoundedRect(pillRect, 13.0, 13.0);
            p.setPen(Qt::white);
            p.drawText(pillRect, Qt::AlignCenter, banner);
        }
    };

    canvasScroll->setWidget(pageHost);
    pageHost->installEventFilter(this);
    if (canvasScroll->viewport())
        canvasScroll->viewport()->installEventFilter(this);

    layout->addLayout(hbox);
    layout->addWidget(canvasScroll, 1);
    layout->setMargin(0);
    layout->setSpacing(0);
    this->setWidget(d_main_widget);
    d_main_widget->installEventFilter(this);

    d_main_widget->setAutoFillBackground(true);
    d_main_widget->setBackgroundRole(QPalette::Window);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(Qt::white));
    d_main_widget->setPalette(pal);

    syncMultiLayerPageHost(this, d_main_widget, canvas, d_page_unit, d_page_view_mode,
                           d_base_page_px, d_page_zoom, d_page_bg_color);

    // Non-exported Bottom-Right Interactive Canvas Status Pill (floats on d_main_widget)
    d_canvas_status_pill = new QToolButton(d_main_widget);
    d_canvas_status_pill->setObjectName(QStringLiteral("MultiLayerStatusPill"));
    d_canvas_status_pill->setFocusPolicy(Qt::NoFocus);
    d_canvas_status_pill->setCursor(Qt::PointingHandCursor);
    d_canvas_status_pill->setPopupMode(QToolButton::InstantPopup);
    {
        QFont pillFont(QStringLiteral("Segoe UI"));
        pillFont.setPointSizeF(9.5);
        pillFont.setWeight(QFont::DemiBold);
        d_canvas_status_pill->setFont(pillFont);
    }
    d_canvas_status_pill->setStyleSheet(QStringLiteral(
            "QToolButton#MultiLayerStatusPill {"
            "  background-color: rgba(255, 255, 255, 235);"
            "  color: #334155;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 12px;"
            "  padding: 2px 10px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 9.5pt;"
            "  font-weight: 600;"
            "  min-height: 22px;"
            "}"
            "QToolButton#MultiLayerStatusPill:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "}"
            "QToolButton#MultiLayerStatusPill::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
            "}"));
    QMenu *statusMenu = new QMenu(d_canvas_status_pill);
    auto populateStatusMenu = [this, statusMenu]() {
        statusMenu->clear();
        QAction *actLockAspect = statusMenu->addAction(tr("Lock Aspect Ratio"));
        actLockAspect->setCheckable(true);
        actLockAspect->setChecked(d_keep_aspect_ratio);
        connect(actLockAspect, &QAction::toggled, this, [this](bool checked) {
            const QString snap = captureUndoSnapshot();
            if (checked) {
                double ratio = 1.0;
                Graph *ag = active_graph ? active_graph : layer(1);
                if (ag && ag->plotWidget() && ag->plotWidget()->plotLayout()) {
                    const QRect cr = ag->plotWidget()->plotLayout()->canvasRect();
                    if (cr.width() > 20 && cr.height() > 20)
                        ratio = double(cr.width()) / double(cr.height());
                } else if (l_canvas_width > 0 && l_canvas_height > 0) {
                    ratio = double(l_canvas_width) / double(l_canvas_height);
                }
                const bool squarePage = (d_base_page_px.width() > 0 && d_base_page_px.height() > 0
                                         && qAbs(d_base_page_px.width() - d_base_page_px.height()) <= 4);
                if (squarePage && ratio >= 0.82 && ratio <= 1.18)
                    ratio = 1.0;
                d_target_aspect_ratio = ratio;
            }
            setKeepAspectRatio(checked);
            pushUndoSnapshot(tr("Toggle Aspect Ratio Lock"), snap);
        });

        auto applyAspectWithUndo = [this](double ratio, bool lock, const QString &label) {
            const QString snap = captureUndoSnapshot();
            if (lock)
                applyAspectRatioPreset(ratio, true);
            else
                setKeepAspectRatio(false);
            pushUndoSnapshot(label, snap);
        };
        statusMenu->addAction(tr("1:1 Square"), this, [applyAspectWithUndo]() {
            applyAspectWithUndo(1.0, true, tr("Aspect Ratio 1:1"));
        });
        statusMenu->addAction(tr("4:3 Landscape"), this, [applyAspectWithUndo]() {
            applyAspectWithUndo(4.0 / 3.0, true, tr("Aspect Ratio 4:3"));
        });
        statusMenu->addAction(tr("16:9 Wide"), this, [applyAspectWithUndo]() {
            applyAspectWithUndo(16.0 / 9.0, true, tr("Aspect Ratio 16:9"));
        });
        statusMenu->addAction(tr("3:4 Portrait"), this, [applyAspectWithUndo]() {
            applyAspectWithUndo(3.0 / 4.0, true, tr("Aspect Ratio 3:4"));
        });
        statusMenu->addAction(tr("Free Resize"), this, [applyAspectWithUndo]() {
            applyAspectWithUndo(1.0, false, tr("Free Resize"));
        });
        statusMenu->addSeparator();

        auto zoomWithUndo = [this](double z) {
            const QString snap = captureUndoSnapshot();
            setPageZoom(z);
            pushUndoSnapshot(tr("Page Zoom"), snap);
        };
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("zoom-out"), 18, false),
                              tr("Zoom 75%"), this, [zoomWithUndo]() { zoomWithUndo(0.75); });
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 18, false),
                              tr("Zoom 100%"), this, [zoomWithUndo]() { zoomWithUndo(1.0); });
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("zoom-in"), 18, false),
                              tr("Zoom 125%"), this, [zoomWithUndo]() { zoomWithUndo(1.25); });
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("zoom-in"), 18, false),
                              tr("Zoom 150%"), this, [zoomWithUndo]() { zoomWithUndo(1.5); });
        statusMenu->addSeparator();
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("page-setup"), 18, false),
                              tr("Square Frame (70%)"), this, [this]() {
            const QString snap = captureUndoSnapshot();
            applySquareCanvasFrame70Percent();
            pushUndoSnapshot(tr("Square Frame (70%)"), snap);
        });
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("fit-page"), 18, false),
                              tr("Fit Page to Layers"), this, [this]() {
            const QString snap = captureUndoSnapshot();
            fitPageToLayers();
            pushUndoSnapshot(tr("Fit Page to Layers"), snap);
        });
        statusMenu->addSeparator();
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("preset-isij"), 18, false),
                              tr("ISIJ 85mm"), this,
                              [this]() { applyStylePreset(StyleISIJJournal85mm); });
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("preset-slide"), 18, false),
                              tr("Slide 28pt"), this,
                              [this]() { applyStylePreset(StylePresentationSlide28pt); });
        const QVector<GraphStylePresetSpec> allPresets = MultiLayer::listStylePresets();
        for (const GraphStylePresetSpec &sp : allPresets) {
            if (sp.builtIn)
                continue;
            const QString presetId = sp.id;
            statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("preset-isij"), 18, false),
                                  sp.name, this,
                                  [this, presetId]() { applyStylePresetById(presetId); });
        }
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("page-setup"), 18, false),
                              tr("Manage Style Presets..."), this, [this]() {
            ApplicationWindow *aw = findMultiLayerApp(this);
            MultiLayer::showStylePresetManagerDialog(aw, this);
        });
        statusMenu->addSeparator();
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("page-setup"), 18, false),
                              tr("Page Setup..."), this, [this]() { showPageSetupDialog(); });
    };
    populateStatusMenu();
    connect(statusMenu, &QMenu::aboutToShow, this, populateStatusMenu);
    d_canvas_status_pill->setMenu(statusMenu);

    connect(this, &MultiLayer::modifiedPlot, this, &MultiLayer::updateCanvasStatusPill);
    connect(this, &MultiLayer::modifiedPlot, this, &MultiLayer::notifyOleDirtyIfEmbedded);

    setMinimumSize(160, 160);
    setGeometry(QRect(0, 0, graph_width, graph_height));
    setFocusPolicy(Qt::StrongFocus);
    updateCanvasStatusPill();

    if (QAbstractEventDispatcher *ed = QAbstractEventDispatcher::instance()) {
        connect(ed, &QAbstractEventDispatcher::awake, this, [this]() {
            if (graphs > 0 && canvas && canvas->size() != lastSize && canvas->width() >= 100
                && canvas->height() >= 100) {
                resizeLayers(canvas->size(), lastSize, false);
            }
        });
    }
}

QList<Graph *> MultiLayer::layersList() const
{
    QList<Graph *> res;
    res.reserve(graphsList.size());
    for (QWidget *w : graphsList) {
        if (Graph *g = qobject_cast<Graph *>(w))
            res.append(g);
    }
    return res;
}

Graph *MultiLayer::layer(int num)
{
    if (num < 1 || num > graphsList.size())
        return nullptr;
    return (Graph *)graphsList.at(num - 1);
}

LayerButton *MultiLayer::addLayerButton()
{
    for (int i = 0; i < buttonsList.count(); i++) {
        LayerButton *btn = (LayerButton *)buttonsList.at(i);
        btn->setChecked(false);
    }

    LayerButton *button = new LayerButton(QString::number(++graphs));
    button->setToolTip(tr("Layer %1").arg(graphs));
    connect(button, SIGNAL(clicked(LayerButton *)), this, SLOT(activateGraph(LayerButton *)));
    connect(button, &LayerButton::showContextMenu, this, [this, button]() {
        openLayerButtonContextMenu(button, QCursor::pos());
    });
    connect(button, SIGNAL(showCurvesDialog()), this, SIGNAL(showCurvesDialog()));

    buttonsList.append(button);
    layerButtonsBox->addWidget(button);
    return button;
}

Graph *MultiLayer::addLayer(int x, int y, int width, int height)
{
    Graph *styleTemplate = nullptr;
    if (!d_is_restoring_snapshot) {
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *cand = qobject_cast<Graph *>(graphsList.at(i));
            if (cand && cand->plotWidget() && !cand->linkedParentLayer()
                && !d_inset_specs.contains(cand)) {
                styleTemplate = cand;
                break;
            }
        }
    }

    addLayerButton();
    if (!width && !height) {
        width = graph_width;
        height = graph_height;
    }

    Graph *g = new Graph(canvas);
    g->setAttribute(Qt::WA_DeleteOnClose);
    g->setGeometry(x, y, width, height);
    g->plotWidget()->resize(QSize(width, height));

    if (styleTemplate && styleTemplate->plotWidget() && g->plotWidget()) {
        Plot *p0 = styleTemplate->plotWidget();
        Plot *pg = g->plotWidget();
        g->setAxesLinewidth(p0->axesLinewidth());
        g->changeTicksLength(styleTemplate->minorTickLength(), styleTemplate->majorTickLength());
        g->drawAxesBackbones(styleTemplate->axesBackbones());
        const QVector<bool> en = styleTemplate->enabledAxes();
        const QList<int> majTypes = p0->getMajorTicksType();
        const QList<int> minTypes = p0->getMinorTicksType();
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            if (!styleTemplate->hasColorScaleOnAxis(a)) {
                g->enableAxis(a, en.value(a, true));
                pg->setMajorTicksType(a, majTypes.value(a, ScaleDraw::In));
                pg->setMinorTicksType(a, minTypes.value(a, ScaleDraw::In));
                if (const QwtScaleDraw *sd0 = p0->axisScaleDraw(a)) {
                    if (QwtScaleDraw *sdg = pg->axisScaleDraw(a)) {
                        sdg->enableComponent(QwtAbstractScaleDraw::Labels,
                                             sd0->hasComponent(QwtAbstractScaleDraw::Labels));
                    }
                }
            } else {
                const int refAxis = (a == QwtPlot::yRight) ? QwtPlot::yLeft : QwtPlot::xBottom;
                pg->setMajorTicksType(a, majTypes.value(refAxis, ScaleDraw::In));
                pg->setMinorTicksType(a, minTypes.value(refAxis, ScaleDraw::In));
                if (QwtScaleDraw *sdg = pg->axisScaleDraw(a))
                    sdg->enableComponent(QwtAbstractScaleDraw::Labels, false);
            }
            g->setAxisBackboneVisible(a, styleTemplate->isAxisBackboneVisible(a));
            g->setAxisFont(a, styleTemplate->axisFont(a));
            g->setAxisTitleFont(a, styleTemplate->axisTitleFont(a));
        }
        g->setTitleFont(p0->title().font());
        g->setAxisUnitFormat(styleTemplate->axisUnitFormat());
        g->setTextMarkerDefaults(styleTemplate->textMarkerDefaultFrame(),
                                 styleTemplate->defaultTextMarkerFont(),
                                 styleTemplate->textMarkerDefaultColor(),
                                 styleTemplate->textMarkerDefaultBackground());
    }

    graphsList.append(g);

    active_graph = g;
    g->show();
    connectLayer(g);
    enforceLayerZOrder();
    refreshLayerButtonBadges();
    updateCanvasStatusPill();
    return g;
}

QList<Graph *> MultiLayer::orderedLayersForRender() const
{
    QList<Graph *> bases;
    QList<Graph *> linkedOverlays;
    QList<Graph *> insets;
    bases.reserve(graphsList.count());

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (!gr)
            continue;
        if (d_inset_specs.contains(gr))
            insets.append(gr);
        else if (gr->linkedParentLayer() != nullptr)
            linkedOverlays.append(gr);
        else
            bases.append(gr);
    }

    QList<Graph *> ordered;
    ordered.reserve(bases.size() + linkedOverlays.size() + insets.size());
    ordered.append(bases);
    ordered.append(linkedOverlays);
    ordered.append(insets);
    return ordered;
}

void MultiLayer::enforceLayerZOrder()
{
    Graph *activeBase = nullptr;
    Graph *activeOverlay = nullptr;
    Graph *activeInset = nullptr;

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (!gr)
            continue;
        if (!gr->linkedParentLayer() && !d_inset_specs.contains(gr)) {
            if (gr == active_graph)
                activeBase = gr;
            else
                gr->raise();
        }
    }
    if (activeBase)
        activeBase->raise();

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (!gr)
            continue;
        if (gr->linkedParentLayer() != nullptr && !d_inset_specs.contains(gr)) {
            if (gr == active_graph)
                activeOverlay = gr;
            else
                gr->raise();
        }
    }
    if (activeOverlay)
        activeOverlay->raise();

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (!gr)
            continue;
        if (d_inset_specs.contains(gr)) {
            if (gr == active_graph)
                activeInset = gr;
            else
                gr->raise();
        }
    }
    if (activeInset)
        activeInset->raise();

    if (d_layers_selector)
        d_layers_selector->raise();
}

void MultiLayer::refreshLayerButtonBadges()
{
    for (int i = 0; i < buttonsList.count(); ++i) {
        LayerButton *btn = qobject_cast<LayerButton *>(buttonsList.at(i));
        if (!btn)
            continue;
        const int num = i + 1;
        Graph *gr = (i < graphsList.count()) ? qobject_cast<Graph *>(graphsList.at(i)) : nullptr;
        if (gr && d_inset_specs.contains(gr)) {
            Graph *src = d_inset_specs[gr].sourceLayer.data();
            const int srcNum = src ? (graphsList.indexOf(src) + 1) : 1;
            btn->setText(QStringLiteral("%1\U0001F50D").arg(num));
            btn->setToolTip(tr("Layer %1 — Zoom Inset of Layer %2 (drag to reposition)")
                                    .arg(num)
                                    .arg(qMax(1, srcNum)));
        } else if (gr && gr->linkedParentLayer()) {
            const int pNum = graphsList.indexOf(gr->linkedParentLayer()) + 1;
            btn->setText(QStringLiteral("%1\U0001F517").arg(num));
            btn->setToolTip(tr("Layer %1 — Linked Overlay of Layer %2 (Twin-Axis)")
                                    .arg(num)
                                    .arg(qMax(1, pNum)));
        } else {
            btn->setText(QString::number(num));
            bool hasRightYLabels = false;
            if (gr && gr->plotWidget() && gr->plotWidget()->axisEnabled(QwtPlot::yRight)) {
                if (const QwtScaleDraw *sd = gr->plotWidget()->axisScaleDraw(QwtPlot::yRight))
                    hasRightYLabels = sd->hasComponent(QwtAbstractScaleDraw::Labels);
            }
            if (hasRightYLabels)
                btn->setToolTip(tr("Layer %1 — Primary Layer (Double-Y Active)").arg(num));
            else
                btn->setToolTip(tr("Layer %1 — Primary Layer").arg(num));
        }
        btn->updateGeometry();
    }
}

void MultiLayer::syncCanvasOverlayVisibility()
{
    if (!widget())
        return;
    if (QWidget *ov = widget()->findChild<QWidget *>(
                QStringLiteral("MultiLayerPanelLabelOverlay"))) {
        const bool showOv = d_panel_labels_enabled || !d_inset_specs.isEmpty();
        if (canvas)
            ov->setGeometry(canvas->geometry());
        ov->setVisible(showOv);
        if (showOv) {
            ov->raise();
            if (d_drop_zone_overlay && d_drop_zone_overlay->isVisible())
                d_drop_zone_overlay->raise();
            if (d_canvas_status_pill)
                d_canvas_status_pill->raise();
        }
        ov->update();
    }
}

void MultiLayer::activateGraph(LayerButton *button)
{
    for (int i = 0; i < buttonsList.count(); i++) {
        LayerButton *btn = (LayerButton *)buttonsList.at(i);
        if (btn->isChecked())
            btn->setChecked(false);

        if (btn == button) {
            active_graph = (Graph *)graphsList.at(i);
            active_graph->setFocus();
            button->setChecked(true);
        }
    }
    enforceLayerZOrder();
    updateCanvasStatusPill();
}

void MultiLayer::setActiveGraph(Graph *g)
{
    if (!g)
        return;
    if (active_graph == g) {
        enforceLayerZOrder();
        return;
    }

    active_graph = g;
    active_graph->setFocus();

    if (d_layers_selector)
        delete d_layers_selector;

    enforceLayerZOrder();

    for (int i = 0; i < (int)graphsList.count(); i++) {
        Graph *gr = (Graph *)graphsList.at(i);
        LayerButton *btn = (LayerButton *)buttonsList.at(i);
        if (gr == g)
            btn->setChecked(true);
        else
            btn->setChecked(false);
    }
    updateCanvasStatusPill();
}

void MultiLayer::contextMenuEvent(QContextMenuEvent *e)
{
    emit showWindowContextMenu();
    e->accept();
}

void MultiLayer::resizeLayers(const QResizeEvent *re)
{
    bool unlockedByShift = false;
    if (d_keep_aspect_ratio && (QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
        d_keep_aspect_ratio = false;
        unlockedByShift = true;
    }

    QSize oldSize = re->oldSize();
    QSize size = re->size();

    if (!oldSize.isValid()) {
        if (lastSize.isValid() && isVisible()) {
            oldSize = lastSize;
        } else { // TODO: for maximized windows, the size of the layers should be saved in % of the
                 // workspace area in order to restore
            // the layers properly! For the moment just resize the layers to occupy the whole
            // canvas, although the restored geometry might be altered!
            oldSize = QSize(canvas->childrenRect().width() + left_margin + right_margin,
                            canvas->childrenRect().height() + top_margin + bottom_margin);
        }
    }

    const bool scaleFontsWithCtrl = (QApplication::keyboardModifiers() & Qt::ControlModifier) != 0;
    resizeLayers(size, oldSize, scaleFontsWithCtrl);
    if (unlockedByShift || scaleFontsWithCtrl) {
        if (ApplicationWindow *app = findMultiLayerApp(this)) {
            if (unlockedByShift) {
                if (ModernRibbonBar *rb = app->modernRibbonBar())
                    rb->syncCheckableStates();
            }
            if (GraphInspectorDock *dock = app->graphInspectorDock()) {
                if (dock->targetMultiLayer() == this) {
                    if (unlockedByShift)
                        dock->setTargetMultiLayer(this, true);
                    else
                        dock->refreshAll();
                }
            }
        }
    }
}

void MultiLayer::resizeLayers(const QSize &size, const QSize &oldSize, bool scaleFonts)
{
    Q_UNUSED(oldSize);
    if (graphs <= 0) {
        lastSize = size;
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    bool anyResizable = false;
    for (int i = 0; i < graphsList.count(); i++) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr && !gr->ignoresResizeEvents()) {
            anyResizable = true;
            break;
        }
    }

    if (anyResizable) {
        const int prevCanvasH = qMax(1, l_canvas_height);
        arrangeLayers(false, false);
        if (scaleFonts && l_canvas_height > 0 && l_canvas_height != prevCanvasH) {
            const double ratio = (double)l_canvas_height / (double)prevCanvasH;
            for (int i = 0; i < graphsList.count(); i++) {
                Graph *gr = (Graph *)graphsList.at(i);
                if (gr && !gr->ignoresResizeEvents())
                    gr->scaleFonts(ratio);
            }
            arrangeLayers(false, false);
        }
        if (!d_keep_aspect_ratio && l_canvas_width > 0 && l_canvas_height > 0) {
            d_target_aspect_ratio = double(l_canvas_width) / double(l_canvas_height);
        }
    }

    lastSize = size;
    emit modifiedPlot();
    emit resizedWindow(this);
    QApplication::restoreOverrideCursor();
}

void MultiLayer::confirmRemoveLayer()
{
    if (graphs > 1 && !ApplicationWindow::isNonInteractiveContext(this)) {
        switch (QMessageBox::information(
                this, tr("Guess best layout?"),
                tr("Do you want ToriGraph to rearrange the remaining layers?"), tr("&Yes"),
                tr("&No"), tr("&Cancel"), 0, 2)) {
        case 0:
            removeLayer();
            arrangeLayers(true, false);
            break;

        case 1:
            removeLayer();
            break;

        case 2:
            return;
            break;
        }
    } else
        removeLayer();
}

void MultiLayer::removeLayer()
{
    if (!active_graph)
        return;

    if (ApplicationWindow *app = findMultiLayerApp(this))
        app->invalidateModelessGraphDialogs(this);
    CanvasInlineTextEditor::cancelActiveForGraph(active_graph);
    active_graph->cancelInlineScaleEditor();

    // remove corresponding button cleanly without leaking or polluting layout
    LayerButton *btn = 0;
    int i;
    for (i = 0; i < buttonsList.count(); i++) {
        btn = (LayerButton *)buttonsList.at(i);
        if (btn && btn->isChecked()) {
            buttonsList.removeAll(btn);
            if (layerButtonsBox)
                layerButtonsBox->removeWidget(btn);
            btn->blockSignals(true);
            btn->disconnect();
            btn->hide();
            btn->setParent(nullptr);
            btn->deleteLater();
            break;
        }
    }

    // update the texts of the buttons
    for (i = 0; i < buttonsList.count(); i++) {
        btn = (LayerButton *)buttonsList.at(i);
        if (btn) {
            btn->setText(QString::number(i + 1));
            btn->setToolTip(tr("Layer %1").arg(i + 1));
        }
    }

    if (active_graph->zoomOn() || active_graph->activeTool())
        emit setPointerCursor();

    int index = graphsList.indexOf(active_graph);
    d_inset_specs.remove(active_graph);
    for (auto it = d_inset_specs.begin(); it != d_inset_specs.end();) {
        if (it.value().sourceLayer.isNull() || it.value().sourceLayer.data() == active_graph)
            it = d_inset_specs.erase(it);
        else
            ++it;
    }
    if (index >= 0)
        graphsList.removeAt(index);
    Graph *closingGraph = active_graph;
    graphs = qMax(0, graphs - 1);
    if (index >= graphsList.count())
        index = graphsList.count() - 1;

    active_graph = (graphs > 0 && index >= 0) ? (Graph *)graphsList.at(index) : nullptr;
    closingGraph->setParent(nullptr);
    closingGraph->close();

    if (graphs == 0 || !active_graph) {
        active_graph = nullptr;
        refreshLayerButtonBadges();
        syncCanvasOverlayVisibility();
        updateCanvasStatusPill();
        return;
    }

    for (i = 0; i < (int)graphsList.count(); i++) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr == active_graph && i < buttonsList.count()) {
            LayerButton *button = (LayerButton *)buttonsList.at(i);
            if (button)
                button->setChecked(true);
            break;
        }
    }

    enforceLayerZOrder();
    refreshLayerButtonBadges();
    syncCanvasOverlayVisibility();
    emit modifiedPlot();
}

void MultiLayer::setGraphGeometry(int x, int y, int w, int h)
{
    if (active_graph->pos() == QPoint(x, y) && active_graph->size() == QSize(w, h))
        return;

    active_graph->setGeometry(QRect(QPoint(x, y), QSize(w, h)));
    active_graph->plotWidget()->resize(QSize(w, h));
    emit modifiedPlot();
}

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
        reqTopHeights[i] = effTop;
        reqBottomHeights[i] = effBottom;
        reqLeftWidths[i] = effLeft;
        reqRightWidths[i] = effRight;

        int row = i / nCols;
        if (row >= nRows)
            row = nRows - 1;
        int col = i % nCols;

        if (effTop > maxTopH[row])
            maxTopH[row] = effTop;
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
        delete d_layers_selector;

    if (fit)
        findBestLayout(rows, cols);

    // the canvas sizes of all layers become equal only after several
    // resize iterations, due to the way Qwt handles the plot layout
    int iterations = 0;
    QSize size = arrangeLayers(userSize);
    QSize canvas_size = QSize(1, 1);
    while ((canvas_size != size || iterations < 3) && iterations < 10) {
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

void MultiLayer::exportToFile(const QString &fileName)
{
    if (fileName.isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(this))
            QMessageBox::critical(0, tr("Error"), tr("Please provide a valid file name!"));
        return;
    }

    if (fileName.endsWith(".emf", Qt::CaseInsensitive)) {
#ifdef Q_OS_WIN
        const OleRenderedGraph emf = SciDAVisEmf::renderMultiLayer(this, nullptr, false);
        if (!emf.emfBytes.isEmpty()) {
            QFile f(fileName);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(emf.emfBytes);
                f.close();
                return;
            }
        }
#endif
        if (!ApplicationWindow::isNonInteractiveContext(this))
            QMessageBox::critical(this, tr("Error"), tr("File format not handled, operation aborted!"));
        return;
    }

    if (fileName.contains(".eps", Qt::CaseInsensitive)
        || fileName.contains(".pdf", Qt::CaseInsensitive)
        || fileName.contains(".ps", Qt::CaseInsensitive)) {
        exportVector(fileName);
        return;
    } else if (fileName.contains(".svg", Qt::CaseInsensitive)) {
        exportSVG(fileName);
        return;
    } else {
        QList<QByteArray> list = QImageWriter::supportedImageFormats();
        for (int i = 0; i < list.count(); i++) {
            if (fileName.contains("." + list[i].toLower(), Qt::CaseInsensitive)) {
                exportImage(fileName);
                return;
            }
        }
        if (!ApplicationWindow::isNonInteractiveContext(this))
            QMessageBox::critical(this, tr("Error"), tr("File format not handled, operation aborted!"));
    }
}

QImage MultiLayer::copyPageAsImage(int dpi)
{
    ScopedUnzoomForExport unzoomGuard(this);
    const int effectiveDpi = qBound(36, dpi > 0 ? dpi : 300, 4800);

    QRect layersRect;
    for (int i = 0; i < (int)graphsList.count(); i++) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr && gr->plotWidget())
            layersRect = layersRect.united(QRect(gr->pos(), gr->plotWidget()->size()));
    }
    if (!layersRect.isEmpty())
        layersRect.adjust(-left_margin, -top_margin, right_margin, bottom_margin);
    else
        layersRect = (canvas && !canvas->rect().isEmpty()) ? canvas->rect() : QRect(0, 0, 520, 520);
    if (d_panel_labels_enabled) {
        const QRect lblRect = panelLabelsBoundingRect();
        if (!lblRect.isEmpty())
            layersRect = layersRect.united(lblRect);
    }

    const bool hasPhysicalPage = (d_page_unit != UnitPx);
    QRect srcRect = (hasPhysicalPage || d_page_view_mode)
            ? ((canvas && !canvas->rect().isEmpty()) ? canvas->rect() : layersRect)
            : layersRect;
    if (d_panel_labels_enabled) {
        const QRect lblRect = panelLabelsBoundingRect();
        if (!lblRect.isEmpty())
            srcRect = srcRect.united(lblRect);
    }

    QSize targetSize;
    if (hasPhysicalPage) {
        auto toInches = [](double val, PageUnit u) -> double {
            switch (u) {
            case UnitMm:
                return val / 25.4;
            case UnitCm:
                return val / 2.54;
            case UnitInch:
                return val;
            case UnitPx:
            default:
                return val / 96.0;
            }
        };
        const double wIn = toInches(d_page_width_val, d_page_unit);
        const double hIn = toInches(d_page_height_val, d_page_unit);
        targetSize = QSize(qMax(1, qRound(wIn * double(effectiveDpi))),
                           qMax(1, qRound(hIn * double(effectiveDpi))));
    } else {
        const double scale = double(effectiveDpi) / 96.0;
        targetSize = QSize(qMax(1, qRound(srcRect.width() * scale)),
                           qMax(1, qRound(srcRect.height() * scale)));
    }

    QImage image(targetSize, QImage::Format_RGB32);
    image.fill(Qt::white);

    {
        QPainter p(&image);
        QFont baseFnt(QStringLiteral("Arial"), 10);
        baseFnt.setHintingPreference(QFont::PreferNoHinting);
        p.setFont(baseFnt);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                         | QPainter::SmoothPixmapTransform);
        const double sx = double(targetSize.width()) / double(qMax(1, srcRect.width()));
        const double sy = double(targetSize.height()) / double(qMax(1, srcRect.height()));
        if (hasPhysicalPage) {
            const double s = qMin(sx, sy);
            const double dx = 0.5 * (double(targetSize.width()) - double(srcRect.width()) * s);
            const double dy = 0.5 * (double(targetSize.height()) - double(srcRect.height()) * s);
            p.translate(dx, dy);
            p.scale(s, s);
        } else {
            p.scale(sx, sy);
        }
        if (d_page_bg_color.isValid() && d_page_bg_color.alpha() > 0)
            p.fillRect(QRect(QPoint(0, 0), srcRect.size()), QBrush(d_page_bg_color));
        const QList<Graph *> ordered = orderedLayersForRender();
        for (Graph *gr : ordered) {
            if (!gr || !gr->plotWidget())
                continue;
            Plot *myPlot = (Plot *)gr->plotWidget();
            const QPoint pos = gr->pos() - srcRect.topLeft();
            gr->exportPainter(p, false, QRect(pos, myPlot->size()));
        }
        p.setClipping(false);
        drawInsetConnectors(&p, -srcRect.topLeft());
        drawPanelLabels(&p, -srcRect.topLeft());
        p.end();
    }

    const int dpm = qRound(double(effectiveDpi) / 0.0254);
    image.setDotsPerMeterX(dpm);
    image.setDotsPerMeterY(dpm);

    if (QClipboard *cb = QApplication::clipboard()) {
        for (int attempt = 0; attempt < 10; ++attempt) {
            cb->setImage(image);
            if (cb->image().size() == image.size())
                break;
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 15);
            QThread::msleep(20);
        }
    }

    return image;
}

void MultiLayer::exportImage(const QString &fileName, int quality, bool transparent, int dpi)
{
    if (fileName.endsWith(".emf", Qt::CaseInsensitive)) {
        exportToFile(fileName);
        return;
    }

    ScopedUnzoomForExport unzoomGuard(this);

    QRect layersRect;
    for (int i = 0; i < (int)graphsList.count(); i++) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr && gr->plotWidget())
            layersRect = layersRect.united(QRect(gr->pos(), gr->plotWidget()->size()));
    }
    if (!layersRect.isEmpty())
        layersRect.adjust(-left_margin, -top_margin, right_margin, bottom_margin);
    else
        layersRect = canvas ? canvas->rect() : QRect(0, 0, 520, 520);
    if (d_panel_labels_enabled) {
        const QRect lblRect = panelLabelsBoundingRect();
        if (!lblRect.isEmpty())
            layersRect = layersRect.united(lblRect);
    }

    const bool hasPhysicalPage = (d_page_unit != UnitPx);
    QRect srcRect = (hasPhysicalPage || d_page_view_mode)
            ? ((canvas && !canvas->rect().isEmpty()) ? canvas->rect() : layersRect)
            : layersRect;
    if (d_panel_labels_enabled) {
        const QRect lblRect = panelLabelsBoundingRect();
        if (!lblRect.isEmpty())
            srcRect = srcRect.united(lblRect);
    }

    const int effectiveDpi = (dpi > 0)
            ? qBound(36, dpi, 4800)
            : (hasPhysicalPage ? qBound(36, d_page_dpi, 4800) : 96);

    QSize targetSize;
    if (hasPhysicalPage) {
        auto toInches = [](double val, PageUnit u) -> double {
            switch (u) {
            case UnitMm:
                return val / 25.4;
            case UnitCm:
                return val / 2.54;
            case UnitInch:
                return val;
            case UnitPx:
            default:
                return val / 96.0;
            }
        };
        const double wIn = toInches(d_page_width_val, d_page_unit);
        const double hIn = toInches(d_page_height_val, d_page_unit);
        targetSize = QSize(qMax(1, qRound(wIn * double(effectiveDpi))),
                           qMax(1, qRound(hIn * double(effectiveDpi))));
    } else {
        const double scale = double(effectiveDpi) / 96.0;
        targetSize = QSize(qMax(1, qRound(srcRect.width() * scale)),
                           qMax(1, qRound(srcRect.height() * scale)));
    }

    QImage image(targetSize, QImage::Format_ARGB32);
    image.fill(transparent ? QColor(255, 255, 255, 0)
                           : (d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white)));

    QVector<QColor> savedBg(graphsList.count()), savedCanvasBg(graphsList.count());
    if (transparent) {
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *gr = (Graph *)graphsList.at(i);
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

    {
        QPainter p(&image);
        QFont baseFnt(QStringLiteral("Arial"), 10);
        baseFnt.setHintingPreference(QFont::PreferNoHinting);
        p.setFont(baseFnt);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                         | QPainter::SmoothPixmapTransform);
        const double sx = double(targetSize.width()) / double(qMax(1, srcRect.width()));
        const double sy = double(targetSize.height()) / double(qMax(1, srcRect.height()));
        if (hasPhysicalPage) {
            const double s = qMin(sx, sy);
            const double dx = 0.5 * (double(targetSize.width()) - double(srcRect.width()) * s);
            const double dy = 0.5 * (double(targetSize.height()) - double(srcRect.height()) * s);
            p.translate(dx, dy);
            p.scale(s, s);
        } else {
            p.scale(sx, sy);
        }
        if (!transparent) {
            const QColor pageBg = d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white);
            if (pageBg.alpha() > 0)
                p.fillRect(QRect(QPoint(0, 0), srcRect.size()), QBrush(pageBg));
        }
        const QList<Graph *> ordered = orderedLayersForRender();
        for (Graph *gr : ordered) {
            if (!gr || !gr->plotWidget())
                continue;
            Plot *myPlot = (Plot *)gr->plotWidget();
            QPoint pos = gr->pos() - srcRect.topLeft();
            gr->exportPainter(p, false, QRect(pos, myPlot->size()));
        }
        p.setClipping(false);
        drawInsetConnectors(&p, -srcRect.topLeft());
        drawPanelLabels(&p, -srcRect.topLeft());
        p.end();
    }

    if (transparent) {
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *gr = (Graph *)graphsList.at(i);
            if (!gr || !gr->plotWidget())
                continue;
            QPalette pal = gr->plotWidget()->palette();
            pal.setColor(QPalette::Window, savedBg[i]);
            gr->plotWidget()->setPalette(pal);
            gr->plotWidget()->setCanvasBackground(savedCanvasBg[i]);
        }
    }

    const int dpm = qRound(double(effectiveDpi) / 0.0254);
    image.setDotsPerMeterX(dpm);
    image.setDotsPerMeterY(dpm);

    QImageWriter writer(fileName);
    if (fileName.endsWith(".tif", Qt::CaseInsensitive)
        || fileName.endsWith(".tiff", Qt::CaseInsensitive)) {
        writer.setCompression(1); // LZW
    }
    const bool isPng = fileName.endsWith(".png", Qt::CaseInsensitive);
    if (isPng && quality >= 95
        && qint64(targetSize.width()) * qint64(targetSize.height()) >= 1500000LL) {
        writer.setQuality(75); // Fast lossless zlib level 2 (~3.5x faster than level 6 on >= 1.5 MP)
    } else if (quality >= 0 && !(isPng && quality >= 95)) {
        writer.setQuality(quality);
    }
    writer.write(image);
}

void MultiLayer::exportPDF(const QString &fname)
{
    exportVector(fname);
}

void MultiLayer::exportVector(const QString &fileName, int res, bool color, bool keepAspect,
                              QPageSize pageSize, QPageLayout::Orientation orientation)
{
    if (fileName.isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(this))
            QMessageBox::critical(this, tr("Error"), tr("Please provide a valid file name!"));
        return;
    }

    if (fileName.endsWith(".emf", Qt::CaseInsensitive)) {
        exportToFile(fileName);
        return;
    }

    ScopedUnzoomForExport unzoomGuard(this);

    QPrinter printer;
    printer.setDocName(this->name());
    printer.setCreator("ToriGraph");
    printer.setFullPage(true);

    printer.setOutputFileName(fileName);
    if (fileName.endsWith(".eps", Qt::CaseInsensitive)
        || fileName.endsWith(".ps", Qt::CaseInsensitive)) {
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
        if (d_page_unit != UnitPx) {
            auto toMm = [](double val, PageUnit u) -> double {
                switch (u) {
                case UnitMm:
                    return val;
                case UnitCm:
                    return val * 10.0;
                case UnitInch:
                    return val * 25.4;
                case UnitPx:
                default:
                    return (val / 96.0) * 25.4;
                }
            };
            const double wMm = qMax(5.0, toMm(d_page_width_val, d_page_unit));
            const double hMm = qMax(5.0, toMm(d_page_height_val, d_page_unit));
            printer.setPageSize(QPageSize(QSizeF(wMm, hMm), QPageSize::Millimeter));
            if (canvas && !canvas->rect().isEmpty()) {
                srcRect = canvas->rect();
                if (d_panel_labels_enabled) {
                    const QRect lblRect = panelLabelsBoundingRect();
                    if (!lblRect.isEmpty())
                        srcRect = srcRect.united(lblRect);
                }
            }
        } else {
            QRect layersRect;
            for (int i = 0; i < (int)graphsList.count(); i++) {
                Graph *gr = (Graph *)graphsList.at(i);
                if (gr && gr->plotWidget())
                    layersRect = layersRect.united(QRect(gr->pos(), gr->plotWidget()->size()));
            }
            if (!layersRect.isEmpty())
                layersRect.adjust(-left_margin, -top_margin, right_margin, bottom_margin);
            else
                layersRect = canvas->rect();
            if (d_panel_labels_enabled) {
                const QRect lblRect = panelLabelsBoundingRect();
                if (!lblRect.isEmpty())
                    layersRect = layersRect.united(lblRect);
            }
            const QSizeF ptSize(layersRect.width() * 72.0 / 96.0,
                                layersRect.height() * 72.0 / 96.0);
            printer.setPageSize(QPageSize(ptSize, QPageSize::Point));
            srcRect = layersRect;
        }
    } else {
        printer.setPageOrientation(orientation);
        printer.setPageSize(pageSize);
    }

    exportPainter(printer, keepAspect, srcRect);
}

void MultiLayer::exportSVG(const QString &fname)
{
#if QT_VERSION >= 0x040300
    ScopedUnzoomForExport unzoomGuard(this);
    QRect layersRect;
    for (int i = 0; i < (int)graphsList.count(); i++) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr && gr->plotWidget())
            layersRect = layersRect.united(QRect(gr->pos(), gr->plotWidget()->size()));
    }
    if (!layersRect.isEmpty())
        layersRect.adjust(-left_margin, -top_margin, right_margin, bottom_margin);
    else
        layersRect = canvas->rect();
    if (d_panel_labels_enabled) {
        const QRect lblRect = panelLabelsBoundingRect();
        if (!lblRect.isEmpty())
            layersRect = layersRect.united(lblRect);
    }

    QRect srcRect = (d_page_unit != UnitPx && canvas && !canvas->rect().isEmpty())
            ? canvas->rect()
            : layersRect;
    bool expandedBeyondCanvas = false;
    if (d_panel_labels_enabled) {
        const QRect lblRect = panelLabelsBoundingRect();
        if (!lblRect.isEmpty() && !srcRect.contains(lblRect)) {
            srcRect = srcRect.united(lblRect);
            expandedBeyondCanvas = true;
        }
    }
    QSize svgPxSize = srcRect.size();
    if (d_page_unit != UnitPx && d_base_page_px.isValid() && !expandedBeyondCanvas)
        svgPxSize = d_base_page_px;
    // E6: QSvgGenerator writes the document size as integer px / 96 dpi, so 85 mm became
    // 84.93 mm. Rendering must stay on a 96-dpi device (a higher generator resolution makes
    // Qwt scale fonts and pens twice, see exportVector), so the exact physical size is written
    // into the root element afterwards; the viewBox keeps the drawing coordinates.
    double exactWmm = 0.0, exactHmm = 0.0;
    if (d_page_unit != UnitPx && !expandedBeyondCanvas) {
        exactWmm = d_page_width_val;
        exactHmm = d_page_height_val;
        if (d_page_unit == UnitCm) {
            exactWmm *= 10.0;
            exactHmm *= 10.0;
        } else if (d_page_unit == UnitInch) {
            exactWmm *= 25.4;
            exactHmm *= 25.4;
        }
    }

    {
        QSvgGenerator generator;
        generator.setFileName(fname);
#if QT_VERSION >= 0x040500
        generator.setSize(svgPxSize);
        generator.setViewBox(QRect(QPoint(0, 0), svgPxSize));
        generator.setResolution(96);
        generator.setTitle(this->name());
#endif
        exportPainter(generator, false, srcRect);
    }

    if (exactWmm > 1.0 && exactHmm > 1.0) {
        QFile f(fname);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray svg = f.readAll();
            f.close();
            const int rootPos = svg.indexOf("<svg");
            const int rootEnd = (rootPos >= 0) ? svg.indexOf('>', rootPos) : -1;
            if (rootPos >= 0 && rootEnd > rootPos) {
                QString root = QString::fromUtf8(svg.mid(rootPos, rootEnd - rootPos));
                static const QRegularExpression kWRx(QStringLiteral("\\swidth=\"[^\"]*\""));
                static const QRegularExpression kHRx(QStringLiteral("\\sheight=\"[^\"]*\""));
                root.replace(kWRx, QStringLiteral(" width=\"%1mm\"").arg(exactWmm, 0, 'f', 3));
                root.replace(kHRx, QStringLiteral(" height=\"%1mm\"").arg(exactHmm, 0, 'f', 3));
                svg.replace(rootPos, rootEnd - rootPos, root.toUtf8());
                if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    f.write(svg);
                    f.close();
                }
            }
        }
    }
#endif
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

    QRect layersRect;
    for (int i = 0; i < (int)graphsList.count(); i++) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr && gr->plotWidget())
            layersRect = layersRect.united(QRect(gr->pos(), gr->plotWidget()->size()));
    }
    if (!layersRect.isEmpty())
        layersRect.adjust(-left_margin, -top_margin, right_margin, bottom_margin);
    else
        layersRect = canvas->rect();
    if (d_panel_labels_enabled) {
        const QRect lblRect = panelLabelsBoundingRect();
        if (!lblRect.isEmpty())
            layersRect = layersRect.united(lblRect);
    }

    if (rect == QRect())
        rect = layersRect;
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
    QRect layersRect;
    for (int i = 0; i < (int)graphsList.count(); i++) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr && gr->plotWidget())
            layersRect = layersRect.united(QRect(gr->pos(), gr->plotWidget()->size()));
    }
    if (!layersRect.isEmpty())
        layersRect.adjust(-left_margin, -top_margin, right_margin, bottom_margin);
    else
        layersRect = canvas->rect();
    if (d_panel_labels_enabled) {
        const QRect lblRect = panelLabelsBoundingRect();
        if (!lblRect.isEmpty())
            layersRect = layersRect.united(lblRect);
    }
    QImage image(layersRect.size(), QImage::Format_ARGB32);
    image.fill(d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white));
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

void MultiLayer::setFonts(const QFont &titleFnt, const QFont &scaleFnt, const QFont &numbersFnt,
                          const QFont &legendFnt)
{
    QFont tf = titleFnt;
    tf.setHintingPreference(QFont::PreferNoHinting);
    QFont sf = scaleFnt;
    sf.setHintingPreference(QFont::PreferNoHinting);
    QFont nf = numbersFnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QFont lf = legendFnt;
    lf.setHintingPreference(QFont::PreferNoHinting);

    for (int i = 0; i < (int)graphsList.count(); i++) {
        Graph *gr = (Graph *)graphsList.at(i);
        Plot *plot = gr->plotWidget();

        QwtText text = plot->title();
        text.setFont(tf);
        plot->setTitle(text);
        for (int j = 0; j < QwtPlot::axisCnt; j++) {
            plot->setAxisFont(j, nf);

            text = plot->axisTitle(j);
            text.setFont(sf);
            plot->setAxisTitle(j, text);
        }

        QVector<int> keys = gr->textMarkerKeys();
        for (int k = 0; k < (int)keys.size(); k++) {
            Legend *mrk = (Legend *)gr->textMarker(keys[k]);
            if (mrk)
                mrk->setFont(lf);
        }
        plot->replot();
    }
    emit modifiedPlot();
}

void MultiLayer::installLayerEventFilters(Graph *g)
{
    if (!g)
        return;
    g->removeEventFilter(this);
    g->installEventFilter(this);
    if (Plot *pl = g->plotWidget()) {
        // Called for every layer that joins this page (new, copied, loaded, inset): its text
        // must be drawn at the page zoom like the other layers.
        pl->setDisplayFontScale(d_page_zoom);
        pl->removeEventFilter(this);
        pl->installEventFilter(this);
        if (QWidget *cv = pl->canvas()) {
            cv->removeEventFilter(this);
            cv->installEventFilter(this);
        }
        if (QwtTextLabel *tl = pl->titleLabel()) {
            tl->removeEventFilter(this);
            tl->installEventFilter(this);
        }
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            if (QwtScaleWidget *sw = pl->axisWidget(a)) {
                sw->removeEventFilter(this);
                sw->installEventFilter(this);
            }
        }
    }
}

void MultiLayer::connectLayer(Graph *g)
{
    connect(g, SIGNAL(drawLineEnded(bool)), this, SIGNAL(drawLineEnded(bool)));
    connect(g, SIGNAL(drawTextOff()), this, SIGNAL(drawTextOff()));
    connect(g, SIGNAL(showPlotDialog(int)), this, SIGNAL(showPlotDialog(int)));
    connect(g, SIGNAL(createTable(const QString &, const QString &, QList<Column *>)), this,
            SIGNAL(createTable(const QString &, const QString &, QList<Column *>)));
    connect(g, SIGNAL(viewLineDialog()), this, SIGNAL(showLineDialog()));
    connect(g, SIGNAL(showContextMenu()), this, SIGNAL(showGraphContextMenu()));
    connect(g, SIGNAL(showLayerButtonContextMenu()), this, SIGNAL(showLayerButtonContextMenu()));
    connect(g, SIGNAL(showAxisDialog(int)), this, SIGNAL(showAxisDialog(int)));
    connect(g, SIGNAL(axisDblClicked(int)), this, SIGNAL(showScaleDialog(int)));
    connect(g, SIGNAL(xAxisTitleDblClicked()), this, SIGNAL(showXAxisTitleDialog()));
    connect(g, SIGNAL(yAxisTitleDblClicked()), this, SIGNAL(showYAxisTitleDialog()));
    connect(g, SIGNAL(rightAxisTitleDblClicked()), this, SIGNAL(showRightAxisTitleDialog()));
    connect(g, SIGNAL(topAxisTitleDblClicked()), this, SIGNAL(showTopAxisTitleDialog()));
    connect(g, SIGNAL(showMarkerPopupMenu()), this, SIGNAL(showMarkerPopupMenu()));
    connect(g, SIGNAL(showCurveContextMenu(int)), this, SIGNAL(showCurveContextMenu(int)));
    connect(g, SIGNAL(cursorInfo(const QString &)), this, SIGNAL(cursorInfo(const QString &)));
    connect(g, SIGNAL(viewImageDialog()), this, SIGNAL(showImageDialog()));
    connect(g, SIGNAL(viewTitleDialog()), this, SIGNAL(viewTitleDialog()));
    connect(g, SIGNAL(modifiedGraph()), this, SIGNAL(modifiedPlot()));
    connect(g, SIGNAL(selectedGraph(Graph *)), this, SLOT(setActiveGraph(Graph *)));
    connect(g, SIGNAL(viewTextDialog()), this, SIGNAL(showTextDialog()));
    connect(g, SIGNAL(createIntensityTable(const QString &)), this,
            SIGNAL(createIntensityTable(const QString &)));
    connect(g, &Graph::selectionChanged, this,
            [this](Graph *layer, const Graph::GraphSelection &sel) {
                emit selectionChanged(this, layer, sel);
            });
    QPointer<Graph> safeG(g);
    connect(g, &Graph::modifiedGraph, this, [this, safeG]() {
        if (safeG && d_inset_specs.contains(safeG.data()) && !d_syncing_inset_geometries
            && !d_is_restoring_snapshot) {
            if (Plot *inPlot = safeG->plotWidget()) {
                const QwtScaleDiv *xsd = inPlot->axisScaleDiv(QwtPlot::xBottom);
                const QwtScaleDiv *ysd = inPlot->axisScaleDiv(QwtPlot::yLeft);
                if (xsd && ysd) {
                    const double xLo = qMin(xsd->lowerBound(), xsd->upperBound());
                    const double xHi = qMax(xsd->lowerBound(), xsd->upperBound());
                    const double yLo = qMin(ysd->lowerBound(), ysd->upperBound());
                    const double yHi = qMax(ysd->lowerBound(), ysd->upperBound());
                    if (std::isfinite(xLo) && std::isfinite(xHi) && std::isfinite(yLo)
                        && std::isfinite(yHi) && (xHi > xLo || yHi > yLo)) {
                        d_inset_specs[safeG.data()].zoomDataRect =
                                QRectF(QPointF(xLo, yLo), QPointF(xHi, yHi));
                    }
                }
            }
        }
        refreshLayerButtonBadges();
        syncCanvasOverlayVisibility();
    });
    installLayerEventFilters(g);
}

void MultiLayer::addTextLayer(int f, const QFont &font, const QColor &textCol,
                              const QColor &backgroundCol)
{
    defaultTextMarkerFont = font;
    defaultTextMarkerFrame = f;
    defaultTextMarkerColor = textCol;
    defaultTextMarkerBackground = backgroundCol;

    addTextOn = true;
    QApplication::setOverrideCursor(Qt::IBeamCursor);
    canvas->grabMouse();
}

void MultiLayer::addTextLayer(const QPoint &pos)
{
    Graph *g = addLayer();
    g->removeLegend();
    g->setTitle("");
    QVector<bool> axesOn(4);
    for (int j = 0; j < 4; j++)
        axesOn[j] = false;
    g->enableAxes(axesOn);
    g->setIgnoreResizeEvents(true);
    g->setTextMarkerDefaults(defaultTextMarkerFrame, defaultTextMarkerFont, defaultTextMarkerColor,
                             defaultTextMarkerBackground);
    Legend *mrk = g->newLegend(tr("enter your text here"));
    QSize size = mrk->rect().size();
    setGraphGeometry(pos.x(), pos.y(), size.width() + 10, size.height() + 10);
    g->setIgnoreResizeEvents(false);
    g->show();
    QApplication::restoreOverrideCursor();
    canvas->releaseMouse();
    addTextOn = false;
    emit drawTextOff();
    emit modifiedPlot();
}

namespace {

Graph *owningLayerOfObject(QObject *obj, const QWidgetList &graphsList)
{
    for (QWidget *w = qobject_cast<QWidget *>(obj); w != nullptr; w = w->parentWidget()) {
        if (Graph *g = qobject_cast<Graph *>(w)) {
            if (graphsList.contains(g))
                return g;
        }
    }
    return nullptr;
}

bool graphHasInteractiveHitAtGlobal(Graph *g, const QPoint &globalPos,
                                    QWidget **hitTargetOut = nullptr,
                                    bool includeAxesAndTitle = true)
{
    if (!g || !g->isVisible())
        return false;
    Plot *pl = g->plotWidget();
    if (!pl || !pl->isVisible())
        return false;

    if (includeAxesAndTitle) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            if (!pl->axisEnabled(a))
                continue;
            QwtScaleWidget *sw = pl->axisWidget(a);
            if (!sw || !sw->isVisible())
                continue;
            const QPoint swLocal = sw->mapFromGlobal(globalPos);
            if (sw->rect().contains(swLocal)) {
                if (hitTargetOut)
                    *hitTargetOut = sw;
                return true;
            }
        }
        if (QwtTextLabel *tl = pl->titleLabel()) {
            if (tl->isVisible() && !pl->title().text().trimmed().isEmpty()) {
                const QPoint tlLocal = tl->mapFromGlobal(globalPos);
                if (tl->rect().contains(tlLocal)) {
                    if (hitTargetOut)
                        *hitTargetOut = tl;
                    return true;
                }
            }
        }
    }

    QWidget *cv = pl->canvas();
    if (!cv || !cv->isVisible())
        return false;
    const QPoint cPos = cv->mapFromGlobal(globalPos);
    if (!cv->rect().contains(cPos))
        return false;

    Legend *mainLeg = g->legend();
    if (mainLeg && g->isLegendVisible() && mainLeg->rect().adjusted(-3, -3, 3, 3).contains(cPos)) {
        if (hitTargetOut)
            *hitTargetOut = cv;
        return true;
    }

    const QVector<int> txtKeys = g->textMarkerKeys();
    for (int id : txtKeys) {
        Legend *mrk = g->textMarker(id);
        if (!mrk)
            continue;
        if (mrk == mainLeg && !g->isLegendVisible())
            continue;
        if (mrk->rect().adjusted(-3, -3, 3, 3).contains(cPos)) {
            if (hitTargetOut)
                *hitTargetOut = cv;
            return true;
        }
    }

    const QVector<int> imgKeys = g->imageMarkerKeys();
    for (int id : imgKeys) {
        ImageMarker *im = g->imageMarker(id);
        if (im && im->rect().adjusted(-3, -3, 3, 3).contains(cPos)) {
            if (hitTargetOut)
                *hitTargetOut = cv;
            return true;
        }
    }

    const QVector<int> lineKeys = g->lineMarkerKeys();
    for (int id : lineKeys) {
        ArrowMarker *am = g->arrow(id);
        if (am && am->dist(cPos.x(), cPos.y()) < 10.0) {
            if (hitTargetOut)
                *hitTargetOut = cv;
            return true;
        }
    }

    if (g->referenceLineAtCanvasPos(cPos, 8) >= 0) {
        if (hitTargetOut)
            *hitTargetOut = cv;
        return true;
    }

    int cDist = 999;
    int cVtx = -1;
    const int cKey = pl->closestCurveInteractive(cPos, cDist, cVtx);
    if (cKey >= 0 && cDist < 10) {
        if (hitTargetOut)
            *hitTargetOut = cv;
        return true;
    }

    return false;
}

} // namespace

bool MultiLayer::eventFilter(QObject *object, QEvent *e)
{
    if (!e)
        return false;

    if (d_forwarding_mouse)
        return MyWidget::eventFilter(object, e);

    // Track user drag/resize on Zoom Inset layers so normGeo persists across layouts
    if ((e->type() == QEvent::Move || e->type() == QEvent::Resize) && !d_syncing_inset_geometries
        && !d_is_restoring_snapshot) {
        if (Graph *gr = qobject_cast<Graph *>(object)) {
            if (d_inset_specs.contains(gr)) {
                Graph *srcLayer = d_inset_specs[gr].sourceLayer.data();
                if (srcLayer && srcLayer->width() >= 20 && srcLayer->height() >= 20) {
                    const QRect srcGeo = srcLayer->geometry();
                    const QRect inGeo = gr->geometry();
                    d_inset_specs[gr].normGeo =
                            QRectF(double(inGeo.x() - srcGeo.x()) / double(srcGeo.width()),
                                   double(inGeo.y() - srcGeo.y()) / double(srcGeo.height()),
                                   double(inGeo.width()) / double(srcGeo.width()),
                                   double(inGeo.height()) / double(srcGeo.height()));
                    syncCanvasOverlayVisibility();
                }
            } else if (!d_inset_specs.isEmpty()) {
                syncCanvasOverlayVisibility();
            }
        }
    }

    // Continue forwarding active mouse drag initiated through a transparent linked overlay
    if (d_passthrough_source && object == d_passthrough_source.data()) {
        if (e->type() == QEvent::MouseMove || e->type() == QEvent::MouseButtonRelease) {
            const auto *me = static_cast<const QMouseEvent *>(e);
            if (QWidget *dst = d_passthrough_target.data()) {
                const QPoint globalPos = me->globalPos();
                const QPoint dstLocal = dst->mapFromGlobal(globalPos);
                const QPointF dstWinPos = dst->window()
                        ? QPointF(dst->window()->mapFromGlobal(globalPos))
                        : QPointF(dstLocal);
                QMouseEvent fwd(e->type(), QPointF(dstLocal), dstWinPos, me->screenPos(),
                                me->button(), me->buttons(), me->modifiers(), me->source());
                d_forwarding_mouse = true;
                QCoreApplication::sendEvent(dst, &fwd);
                d_forwarding_mouse = false;
                if (dst->testAttribute(Qt::WA_SetCursor))
                    d_passthrough_source->setCursor(dst->cursor());
            }
            if (e->type() == QEvent::MouseButtonRelease && me->buttons() == Qt::NoButton) {
                d_passthrough_source = nullptr;
                d_passthrough_target = nullptr;
            }
            return true;
        }
    }

    // Transparent linked twin-axis overlay click-through, hover-through & wheel-through
    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonDblClick
        || e->type() == QEvent::MouseMove || e->type() == QEvent::Wheel) {
        Graph *ownerLayer = owningLayerOfObject(object, graphsList);
        if (ownerLayer && ownerLayer->linkedParentLayer() != nullptr
            && !d_inset_specs.contains(ownerLayer) && !addTextOn) {
            const bool overlayHasModalTool =
                    (active_graph == ownerLayer
                     && (ownerLayer->zoomOn() || ownerLayer->activeTool()
                         || ownerLayer->drawTextActive() || ownerLayer->drawLineActive()));
            if (!overlayHasModalTool) {
                if (e->type() == QEvent::Wheel) {
                    if (ownerLayer->plotWidget() && object == ownerLayer->plotWidget()->canvas()) {
                        Graph *baseParent = ownerLayer->linkedParentLayer();
                        if (baseParent && (active_graph == baseParent || ownerLayer->curves() == 0)
                            && baseParent->plotWidget() && baseParent->plotWidget()->canvas()) {
                            const auto *we = static_cast<const QWheelEvent *>(e);
                            QWidget *baseCanvas = baseParent->plotWidget()->canvas();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                            const QPoint globalPos = we->globalPosition().toPoint();
                            const QPointF dstLocal(baseCanvas->mapFromGlobal(globalPos));
                            QWheelEvent fwdWheel(dstLocal, we->globalPosition(), we->pixelDelta(),
                                                 we->angleDelta(), we->buttons(), we->modifiers(),
                                                 we->phase(), we->inverted(), we->source());
#else
                            const QPoint globalPos = we->globalPos();
                            const QPoint dstLocal = baseCanvas->mapFromGlobal(globalPos);
                            QWheelEvent fwdWheel(dstLocal, globalPos, we->pixelDelta(),
                                                 we->angleDelta(), we->delta(), we->orientation(),
                                                 we->buttons(), we->modifiers(), we->phase(),
                                                 we->source(), we->inverted());
#endif
                            d_forwarding_mouse = true;
                            QCoreApplication::sendEvent(baseCanvas, &fwdWheel);
                            d_forwarding_mouse = false;
                            return true;
                        }
                    }
                } else {
                    const auto *me = static_cast<const QMouseEvent *>(e);
                    const bool isPassiveMove =
                            (e->type() == QEvent::MouseMove && me->buttons() == Qt::NoButton);
                    const bool isPressOrDbl = (e->type() == QEvent::MouseButtonPress
                                               || e->type() == QEvent::MouseButtonDblClick);
                    if (isPressOrDbl || isPassiveMove) {
                        auto *srcWidget = qobject_cast<QWidget *>(object);
                        const QPoint globalPos = me->globalPos();
                        if (graphHasInteractiveHitAtGlobal(ownerLayer, globalPos, nullptr, true)) {
                            if (Graph *pLayer = ownerLayer->linkedParentLayer()) {
                                if (pLayer->plotWidget())
                                    pLayer->plotWidget()->clearHoverPoint();
                            }
                            if (isPressOrDbl && active_graph != ownerLayer)
                                setActiveGraph(ownerLayer);
                        } else {
                            QWidget *forwardTarget = nullptr;
                            Graph *targetGraph = nullptr;
                            int hop = 0;
                            for (Graph *pLayer = ownerLayer->linkedParentLayer();
                                 pLayer && hop < 4; pLayer = pLayer->linkedParentLayer(), ++hop) {
                                QWidget *candTarget = nullptr;
                                if (graphHasInteractiveHitAtGlobal(pLayer, globalPos, &candTarget,
                                                                   true)
                                    && candTarget) {
                                    forwardTarget = candTarget;
                                    targetGraph = pLayer;
                                    break;
                                }
                            }
                            if (!forwardTarget && ownerLayer->plotWidget()
                                && object == ownerLayer->plotWidget()->canvas()) {
                                Graph *baseParent = ownerLayer->linkedParentLayer();
                                if (baseParent && active_graph == baseParent
                                    && baseParent->plotWidget()
                                    && baseParent->plotWidget()->canvas()) {
                                    forwardTarget = baseParent->plotWidget()->canvas();
                                    targetGraph = baseParent;
                                }
                            }
                            if (forwardTarget && targetGraph) {
                                if (ownerLayer->plotWidget())
                                    ownerLayer->plotWidget()->clearHoverPoint();
                                if (isPressOrDbl && active_graph != targetGraph)
                                    setActiveGraph(targetGraph);
                                if (e->type() == QEvent::MouseButtonPress && srcWidget) {
                                    d_passthrough_source = srcWidget;
                                    d_passthrough_target = forwardTarget;
                                }
                                const QPoint dstLocal = forwardTarget->mapFromGlobal(globalPos);
                                const QPointF dstWinPos = forwardTarget->window()
                                        ? QPointF(forwardTarget->window()->mapFromGlobal(globalPos))
                                        : QPointF(dstLocal);
                                QMouseEvent fwd(e->type(), QPointF(dstLocal), dstWinPos,
                                                me->screenPos(), me->button(), me->buttons(),
                                                me->modifiers(), me->source());
                                d_forwarding_mouse = true;
                                QCoreApplication::sendEvent(forwardTarget, &fwd);
                                d_forwarding_mouse = false;
                                if (isPassiveMove && srcWidget) {
                                    if (forwardTarget->testAttribute(Qt::WA_SetCursor))
                                        srcWidget->setCursor(forwardTarget->cursor());
                                    else
                                        srcWidget->unsetCursor();
                                }
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (e->type() == QEvent::Wheel) {
        const auto *we = static_cast<const QWheelEvent *>(e);
        if ((we->modifiers() & Qt::ControlModifier) && we->angleDelta().y() != 0
            && (object == (QObject *)canvas
                || (object
                    && (object->objectName() == QLatin1String("MultiLayerPageHost")
                        || object->objectName() == QLatin1String("qt_scrollarea_viewport"))))) {
            wheelEvent(const_cast<QWheelEvent *>(we));
            return true;
        }
    }

    if (e->type() == QEvent::MouseButtonPress && object == (QObject *)canvas) {
        const QMouseEvent *me = (const QMouseEvent *)e;
        if (me->button() == Qt::LeftButton && addTextOn)
            addTextLayer(me->pos());

        return false;
    } else if (e->type() == QEvent::Resize && object == (QObject *)canvas) {
        resizeLayers((const QResizeEvent *)e);
        updateCanvasStatusPill();
    } else if (e->type() == QEvent::Show && object == (QObject *)canvas) {
        updateCanvasStatusPill();
    } else if (e->type() == QEvent::Resize && object
               && object->objectName() == QLatin1String("MultiLayerPageHost")) {
        auto *ph = static_cast<QWidget *>(object);
        const bool freePixelMode =
                (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4);
        if (freePixelMode && canvas && !ph->rect().isEmpty())
            canvas->setGeometry(ph->rect());
        repositionCanvasStatusPill();
    } else if ((e->type() == QEvent::Resize || e->type() == QEvent::Show)
               && (object == (QObject *)widget()
                   || (object
                       && object->objectName() == QLatin1String("qt_scrollarea_viewport")))) {
        repositionCanvasStatusPill();
    }
    return MyWidget::eventFilter(object, e);
}

void MultiLayer::changeEvent(QEvent *event)
{
    MyWidget::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange) {
        if (d_add_layer_btn)
            d_add_layer_btn->setToolTip(tr("Add Layer"));
        if (d_quick_columns_btn) {
            d_quick_columns_btn->setText(tr("Plotted Columns") + QStringLiteral(" \u25BE"));
            d_quick_columns_btn->setToolTip(tr("Toggle Plotted Columns on Active Layer"));
        }
        refreshLayerButtonBadges();
        updateCanvasStatusPill();
    }
}

void MultiLayer::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_F12) {
        if (d_layers_selector)
            delete d_layers_selector;
        int index = graphsList.indexOf((QWidget *)active_graph) + 1;
        if (index >= graphsList.size())
            index = 0;
        Graph *g = (Graph *)graphsList.at(index);
        if (g)
            setActiveGraph(g);
        return;
    }

    if (e->key() == Qt::Key_F10) {
        if (d_layers_selector)
            delete d_layers_selector;
        int index = graphsList.indexOf((QWidget *)active_graph) - 1;
        if (index < 0)
            index = graphsList.size() - 1;
        Graph *g = (Graph *)graphsList.at(index);
        if (g)
            setActiveGraph(g);
        return;
    }

    if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right || e->key() == Qt::Key_Up
        || e->key() == Qt::Key_Down) {
        int step = 2;
        if (e->modifiers() & Qt::ShiftModifier)
            step = 10;
        else if (e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))
            step = 1;
        int dx = 0, dy = 0;
        if (e->key() == Qt::Key_Left)
            dx = -step;
        else if (e->key() == Qt::Key_Right)
            dx = step;
        else if (e->key() == Qt::Key_Up)
            dy = -step;
        else if (e->key() == Qt::Key_Down)
            dy = step;
        if (active_graph && active_graph->nudgeSelectedElement(dx, dy)) {
            e->accept();
            return;
        }
    }

    if (e->key() == Qt::Key_F11) {
        e->ignore();
        MyWidget::keyPressEvent(e);
        return;
    }

    MyWidget::keyPressEvent(e);
}

void MultiLayer::wheelEvent(QWheelEvent *e)
{
    if (!e)
        return;
    if (e->modifiers() & Qt::ControlModifier) {
        const int dy = e->angleDelta().y();
        if (dy != 0) {
            const double factor = (dy > 0) ? 1.1 : (1.0 / 1.1);
            setPageZoom(d_page_zoom * factor);
            emit modifiedPlot();
            e->accept();
            return;
        }
    }
    e->ignore();
}

bool MultiLayer::isEmpty()
{
    if (graphs <= 0)
        return true;
    else
        return false;
}

QString MultiLayer::saveToString(const QString &geometry)
{
    QString s = "<multiLayer>\n";
    s += QString(name()) + "\t";
    s += QString::number(cols) + "\t";
    s += QString::number(rows) + "\t";
    s += birthDate() + "\n";
    s += geometry;
    QString escWl = windowLabel();
    escWl.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    escWl.replace(QLatin1String("\r"), QLatin1String("\n"));
    escWl.replace(QLatin1String("\n"), QLatin1String("<br>"));
    escWl.replace(QLatin1String("\t"), QLatin1String("&#9;"));
    s += "WindowLabel\t" + escWl + "\t" + QString::number(captionPolicy()) + "\n";
    s += "Margins\t" + QString::number(left_margin) + "\t" + QString::number(right_margin) + "\t"
            + QString::number(top_margin) + "\t" + QString::number(bottom_margin) + "\n";
    s += "Spacing\t" + QString::number(rowsSpace) + "\t" + QString::number(colsSpace) + "\n";
    s += "LayerCanvasSize\t" + QString::number(l_canvas_width) + "\t"
            + QString::number(l_canvas_height) + "\n";
    s += "Alignement\t" + QString::number(hor_align) + "\t" + QString::number(vert_align) + "\n";
    s += pageSetupTagsString(true);

    for (int i = 0; i < (int)graphsList.count(); i++) {
        Graph *ag = (Graph *)graphsList.at(i);
        s += ag->saveToString();
    }
    for (auto it = d_inset_specs.constBegin(); it != d_inset_specs.constEnd(); ++it) {
        Graph *insetGr = it.key();
        Graph *srcGr = it.value().sourceLayer.data();
        const int childIdx = graphsList.indexOf(insetGr);
        const int srcIdx = graphsList.indexOf(srcGr);
        if (childIdx >= 0 && srcIdx >= 0) {
            const QRectF rel = it.value().normGeo;
            const QRectF zr = it.value().zoomDataRect;
            s += QString("<ToriInset\t%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9\t%10\t%11\n")
                         .arg(childIdx)
                         .arg(srcIdx)
                         .arg(rel.x(), 0, 'g', 12)
                         .arg(rel.y(), 0, 'g', 12)
                         .arg(rel.width(), 0, 'g', 12)
                         .arg(rel.height(), 0, 'g', 12)
                         .arg(it.value().showConnectors ? 1 : 0)
                         .arg(zr.x(), 0, 'g', 12)
                         .arg(zr.y(), 0, 'g', 12)
                         .arg(zr.width(), 0, 'g', 12)
                         .arg(zr.height(), 0, 'g', 12);
        }
    }
    if (active_graph) {
        const int activeIdx = graphsList.indexOf(active_graph);
        if (activeIdx >= 0)
            s += QString("<ToriActiveLayer index=\"%1\"/>\n").arg(activeIdx);
    }
    return s + "</multiLayer>\n";
}

QString MultiLayer::pageSetupTagsString(bool includeWindowSize) const
{
    QString s;
    double savedPageW = d_page_width_val;
    double savedPageH = d_page_height_val;
    if (d_page_unit == UnitPx && canvas && canvas->width() >= 80 && canvas->height() >= 80) {
        const double z = qMax(0.25, d_page_zoom);
        savedPageW = double(canvas->width()) / z;
        savedPageH = double(canvas->height()) / z;
    }
    s += QString("<ToriPageSetup view=\"%1\" zoom=\"%2\" unit=\"%3\" dpi=\"%4\" w=\"%5\" h=\"%6\" bg=\"%7\"/>\n")
                 .arg(d_page_view_mode ? 1 : 0)
                 .arg(d_page_zoom, 0, 'g', 12)
                 .arg(int(d_page_unit))
                 .arg(d_page_dpi)
                 .arg(savedPageW, 0, 'g', 12)
                 .arg(savedPageH, 0, 'g', 12)
                 .arg((d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white))
                              .name(QColor::HexArgb));
    if (includeWindowSize) {
        const QSize normSz = isMaximized() ? normalGeometry().size() : size();
        if (normSz.width() >= 100 && normSz.height() >= 100) {
            s += QString("<ToriWinSize w=\"%1\" h=\"%2\"/>\n")
                         .arg(normSz.width())
                         .arg(normSz.height());
        }
    }
    s += QString("<ToriAspectRatio keep=\"%1\" ratio=\"%2\"/>\n")
                 .arg(d_keep_aspect_ratio ? 1 : 0)
                 .arg(d_target_aspect_ratio, 0, 'g', 12);
    if (d_panel_labels_enabled || d_panel_label_font_pt > 0
        || d_panel_label_pattern != QLatin1String("(%a)")) {
        QString escPat = d_panel_label_pattern;
        escPat.replace(QLatin1String("&"), QLatin1String("&amp;"));
        escPat.replace(QLatin1String("\""), QLatin1String("&quot;"));
        escPat.replace(QLatin1String("<"), QLatin1String("&lt;"));
        escPat.replace(QLatin1String(">"), QLatin1String("&gt;"));
        s += QString("<ToriPanelLabels enabled=\"%1\" placement=\"%2\" pattern=\"%3\" fontPt=\"%4\"/>\n")
                     .arg(d_panel_labels_enabled ? 1 : 0)
                     .arg(int(d_panel_label_placement))
                     .arg(escPat)
                     .arg(d_panel_label_font_pt);
    }
    return s;
}

QString MultiLayer::saveAsTemplate(const QString &geometryInfo)
{
    QString s = "<multiLayer>\t";
    s += QString::number(rows) + "\t";
    s += QString::number(cols) + "\n";
    s += geometryInfo;
    s += "Margins\t" + QString::number(left_margin) + "\t" + QString::number(right_margin) + "\t"
            + QString::number(top_margin) + "\t" + QString::number(bottom_margin) + "\n";
    s += "Spacing\t" + QString::number(rowsSpace) + "\t" + QString::number(colsSpace) + "\n";
    s += "LayerCanvasSize\t" + QString::number(l_canvas_width) + "\t"
            + QString::number(l_canvas_height) + "\n";
    s += "Alignement\t" + QString::number(hor_align) + "\t" + QString::number(vert_align) + "\n";
    // E4: templates carry the page setup (unit, DPI, size), aspect lock and panel labels.
    s += pageSetupTagsString(false);

    for (int i = 0; i < (int)graphsList.count(); i++) {
        Graph *ag = (Graph *)graphsList.at(i);
        s += ag->saveToString(true);
    }
    for (auto it = d_inset_specs.constBegin(); it != d_inset_specs.constEnd(); ++it) {
        Graph *insetGr = it.key();
        Graph *srcGr = it.value().sourceLayer.data();
        const int childIdx = graphsList.indexOf(insetGr);
        const int srcIdx = graphsList.indexOf(srcGr);
        if (childIdx >= 0 && srcIdx >= 0) {
            const QRectF rel = it.value().normGeo;
            const QRectF zr = it.value().zoomDataRect;
            s += QString("<ToriInset\t%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9\t%10\t%11\n")
                         .arg(childIdx)
                         .arg(srcIdx)
                         .arg(rel.x(), 0, 'g', 12)
                         .arg(rel.y(), 0, 'g', 12)
                         .arg(rel.width(), 0, 'g', 12)
                         .arg(rel.height(), 0, 'g', 12)
                         .arg(it.value().showConnectors ? 1 : 0)
                         .arg(zr.x(), 0, 'g', 12)
                         .arg(zr.y(), 0, 'g', 12)
                         .arg(zr.width(), 0, 'g', 12)
                         .arg(zr.height(), 0, 'g', 12);
        }
    }
    if (active_graph) {
        const int activeIdx = graphsList.indexOf(active_graph);
        if (activeIdx >= 0)
            s += QString("<ToriActiveLayer index=\"%1\"/>\n").arg(activeIdx);
    }
    return s + "</multiLayer>\n";
}

void MultiLayer::mousePressEvent(QMouseEvent *e)
{
    if (!this->widget()->geometry().contains(e->pos())) { // event.pos is in titlebar
        if (e->button() == Qt::RightButton) {
            emit showTitleBarMenu();
            e->accept();
        } else {
            MyWidget::mousePressEvent(e);
        }
        return;
    }
    int margin = 5;
    QPoint pos = canvas->mapFrom(this, e->pos());
    // iterate backwards in Z-stack order, so layers on top are preferred for selection
    const QList<Graph *> ordered = orderedLayersForRender();
    for (int idx = ordered.size() - 1; idx >= 0; --idx) {
        Graph *cand = ordered.at(idx);
        if (!cand || !cand->isVisible())
            continue;
        QRect igeo = cand->frameGeometry();
        igeo.adjust(-margin, -margin, margin, margin);
        if (igeo.contains(pos)) {
            if (e->modifiers() & Qt::ShiftModifier) {
                if (d_layers_selector)
                    d_layers_selector->add(cand);
                else {
                    d_layers_selector = new SelectionMoveResizer(cand);
                    connect(d_layers_selector, SIGNAL(targetsChanged()), this,
                            SIGNAL(modifiedPlot()));
                }
                enforceLayerZOrder();
            } else {
                setActiveGraph(cand);
                if (!d_layers_selector) {
                    d_layers_selector = new SelectionMoveResizer(cand);
                    connect(d_layers_selector, SIGNAL(targetsChanged()), this,
                            SIGNAL(modifiedPlot()));
                }
                enforceLayerZOrder();
            }
            return;
        }
    }
    if (d_layers_selector)
        delete d_layers_selector;
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

namespace {
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
        if (!cRect.adjusted(-30, -30, 30, 30).contains(mr.topLeft())
            && !mr.intersects(cRect.adjusted(-12, -12, 12, 12))) {
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

        const int desiredFramePad = qMax(12, plot->majorTickLength() + 5);
        const int framePad = (mrk->frameStyle() != Legend::None
                              && mr.width() + 2 * desiredFramePad <= cRect.width()
                              && mr.height() + 2 * desiredFramePad <= cRect.height())
                ? desiredFramePad
                : 6;
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
} // namespace

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

void MultiLayer::setLayersNumber(int n)
{
    if (graphs == n)
        return;

    int dn = graphs - n;
    if (dn > 0) {
        if (ApplicationWindow *app = findMultiLayerApp(this))
            app->invalidateModelessGraphDialogs(this);
        for (int i = 0; i < dn; i++) { // remove layer buttons
            LayerButton *btn = buttonsList.isEmpty() ? nullptr : (LayerButton *)buttonsList.last();
            if (btn) {
                buttonsList.removeLast();
                if (layerButtonsBox)
                    layerButtonsBox->removeWidget(btn);
                btn->blockSignals(true);
                btn->disconnect();
                btn->hide();
                btn->setParent(nullptr);
                btn->deleteLater();
            }

            Graph *g = graphsList.isEmpty() ? nullptr : (Graph *)graphsList.last();
            if (g) { // remove layers
                CanvasInlineTextEditor::cancelActiveForGraph(g);
                g->cancelInlineScaleEditor();
                if (g->zoomOn() || g->activeTool())
                    setPointerCursor();
                d_inset_specs.remove(g);
                for (auto it = d_inset_specs.begin(); it != d_inset_specs.end();) {
                    if (it.value().sourceLayer.isNull() || it.value().sourceLayer.data() == g)
                        it = d_inset_specs.erase(it);
                    else
                        ++it;
                }
                graphsList.removeLast();
                g->setParent(nullptr);
                g->close();
            }
        }
        graphs = n;
        if (!graphs) {
            active_graph = 0;
            refreshLayerButtonBadges();
            syncCanvasOverlayVisibility();
            return;
        }

        // check whether the active Graph.has been deleted
        if (graphsList.indexOf(active_graph) == -1)
            active_graph = (Graph *)graphsList.last();
        for (int j = 0; j < (int)graphsList.count(); j++) {
            Graph *gr = (Graph *)graphsList.at(j);
            if (gr == active_graph) {
                LayerButton *button = (LayerButton *)buttonsList.at(j);
                button->setChecked(true);
                break;
            }
        }
    } else {
        for (int i = 0; i < abs(dn); i++)
            addLayer();
    }

    enforceLayerZOrder();
    refreshLayerButtonBadges();
    syncCanvasOverlayVisibility();
    emit modifiedPlot();
}

void MultiLayer::copy(ApplicationWindow *parent, MultiLayer *ml)
{
    hide(); // FIXME: find a better way to avoid a resize event
    resize(ml->size());

    setCols(ml->getCols());
    setRows(ml->getRows());
    setSpacing(ml->rowsSpacing(), ml->colsSpacing());
    setAlignement(ml->horizontalAlignement(), ml->verticalAlignement());
    setMargins(ml->leftMargin(), ml->rightMargin(), ml->topMargin(), ml->bottomMargin());
    setLayerCanvasSize(ml->layerCanvasSize().width(), ml->layerCanvasSize().height());
    d_keep_aspect_ratio = ml->keepAspectRatio();
    d_target_aspect_ratio = ml->targetAspectRatio();
    d_page_view_mode = ml->d_page_view_mode;
    d_page_zoom = ml->d_page_zoom;
    d_page_unit = ml->d_page_unit;
    d_page_dpi = ml->d_page_dpi;
    d_page_width_val = ml->d_page_width_val;
    d_page_height_val = ml->d_page_height_val;
    d_base_page_px = ml->d_base_page_px;
    d_panel_labels_enabled = ml->d_panel_labels_enabled;
    d_panel_label_placement = ml->d_panel_label_placement;
    d_panel_label_pattern = ml->d_panel_label_pattern;
    d_panel_label_font_pt = ml->d_panel_label_font_pt;
    setPageBackgroundColor(ml->d_page_bg_color);
    if (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4) {
        if (ml->canvasWidget() && canvas)
            canvas->resize(ml->canvasWidget()->size());
    }

    QWidgetList srcGraphs = ml->graphPtrs();
    for (int i = 0; i < srcGraphs.count(); i++) {
        Graph *g = (Graph *)srcGraphs.at(i);
        Graph *g2 = addLayer(g->pos().x(), g->pos().y(), g->width(), g->height());
        g2->copy(parent, g);
        g2->setIgnoreResizeEvents(g->ignoresResizeEvents());
        g2->setAutoscaleFonts(g->autoscaleFonts());
    }

    // Rebind linked overlay parent layers and inset specs across cloned layers
    for (int i = 0; i < srcGraphs.count(); ++i) {
        Graph *srcG = (Graph *)srcGraphs.at(i);
        Graph *dstG = (Graph *)graphsList.value(i, nullptr);
        if (!srcG || !dstG)
            continue;
        if (Graph *srcParent = srcG->linkedParentLayer()) {
            const int pIdx = srcGraphs.indexOf(srcParent);
            Graph *dstParent = (pIdx >= 0) ? (Graph *)graphsList.value(pIdx, nullptr) : nullptr;
            if (dstParent) {
                dstG->setBackgroundColor(QColor(255, 255, 255, 0));
                dstG->setCanvasBackground(QColor(255, 255, 255, 0));
                if (Plot *cPlot = dstG->plotWidget()) {
                    cPlot->setAutoFillBackground(false);
                    cPlot->setAttribute(Qt::WA_TranslucentBackground, true);
                    if (cPlot->canvas())
                        cPlot->canvas()->setAutoFillBackground(false);
                }
                dstG->setAttribute(Qt::WA_TranslucentBackground, true);
                dstG->setLinkedParentLayer(dstParent, srcG->xLinkMode(), srcG->xLinkExpr(),
                                           srcG->yLinkMode(), srcG->yLinkExpr());
            }
        }
        if (ml->d_inset_specs.contains(srcG)) {
            Graph *srcSource = ml->d_inset_specs[srcG].sourceLayer.data();
            const int sIdx = srcGraphs.indexOf(srcSource);
            Graph *dstSource = (sIdx >= 0) ? (Graph *)graphsList.value(sIdx, nullptr) : nullptr;
            if (dstSource) {
                InsetSpec spec = ml->d_inset_specs[srcG];
                spec.sourceLayer = QPointer<Graph>(dstSource);
                d_inset_specs[dstG] = spec;
            }
        }
    }
    syncChildOverlayAndInsetGeometries();
    refreshLayerButtonBadges();
    updateCanvasStatusPill();
    show();
}

bool MultiLayer::focusNextPrevChild(bool next)
{
    if (!active_graph)
        return true;

    return active_graph->focusNextPrevChild(next);
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
                if (Plot *sPlot = srcLayer->plotWidget()) {
                    sPlot->updateLayout();
                    const QRect pCanvas =
                            sPlot->plotLayout()->canvasRect().translated(srcLayer->pos());
                    if (pCanvas.width() > 60 && pCanvas.height() > 60) {
                        iw = qMin(iw, qMax(60, pCanvas.width() - 12));
                        ih = qMin(ih, qMax(50, pCanvas.height() - 12));
                        ix = qBound(pCanvas.left() + 6, ix,
                                    qMax(pCanvas.left() + 6, pCanvas.right() - iw - 6));
                        iy = qBound(pCanvas.top() + 6, iy,
                                    qMax(pCanvas.top() + 6, pCanvas.bottom() - ih - 6));
                    }
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
        const QString snap = captureUndoSnapshot();
        LayerDialog dlg(this);
        dlg.setMultiLayer(this);
        dlg.exec();
        refreshLayerButtonBadges();
        pushUndoSnapshot(tr("Link Axes Scales"), snap);
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

namespace {
// Per-layer state of MultiLayer::setPageZoom(): the layer rectangle at 100 % (QRectF) and the
// rectangle the last zoom gave it (QRect).
const char *const kZoomBaseGeomProp = "_toriZoomBaseGeom";
const char *const kZoomAppliedGeomProp = "_toriZoomAppliedGeom";
} // namespace

void MultiLayer::setPageZoom(double factor)
{
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
    syncMultiLayerPageHost(this, widget(), canvas, d_page_unit, d_page_view_mode, d_base_page_px,
                           d_page_zoom, d_page_bg_color);
    const bool freePx =
            (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4);
    resizeMultiLayerSubWindowForCanvas(this, widget(), d_layer_pill_frame, newCanvasSize.width(),
                                       newCanvasSize.height(), freePx);

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
        gr->plotWidget()->updateLayout();
        gr->plotWidget()->replot();
        gr->plotWidget()->updateLayout();
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
}

double MultiLayer::pageZoom() const
{
    return d_page_zoom;
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
    syncMultiLayerPageHost(this, widget(), canvas, d_page_unit, d_page_view_mode, d_base_page_px,
                           d_page_zoom, d_page_bg_color);
    const bool freePx =
            (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4);
    resizeMultiLayerSubWindowForCanvas(this, widget(), d_layer_pill_frame,
                                       targetCanvasSize.width(), targetCanvasSize.height(), freePx);

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
    const QString beforeSnap = captureUndoSnapshot();
    bool anyApplied = false;

    QDialog dlg(this);
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
        fitPageToLayers(14);
        anyApplied = true;
    });
    QObject::connect(btnFitLayers, &QPushButton::clicked, &dlg, [&]() {
        fitLayersToPage();
        anyApplied = true;
    });

    auto applyNow = [&]() {
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
    if (anyApplied)
        pushUndoSnapshot(tr("Page Setup"), beforeSnap);
}

Graph *MultiLayer::addLinkedLayer(Graph *parentLayer, bool linkX, bool linkY,
                                  const QString &xFormula, const QString &yFormula)
{
    if (!parentLayer)
        parentLayer = active_graph ? active_graph : layer(1);
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

Graph *MultiLayer::addInsetLayer(Graph *sourceLayer, const QRectF &roiDataRect, bool showConnectors)
{
    if (!sourceLayer)
        sourceLayer = active_graph ? active_graph : layer(1);
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

void MultiLayer::applyStylePreset(StylePreset preset)
{
    QString id = QStringLiteral("isij_85mm");
    if (preset == StylePresentationSlide28pt)
        id = QStringLiteral("slide_28pt");

    GraphStylePresetSpec spec;
    if (!findStylePreset(id, &spec)) {
        const QVector<GraphStylePresetSpec> defs = defaultBuiltInStylePresets();
        const int idx = qBound(0, static_cast<int>(preset), defs.size() - 1);
        spec = defs.at(idx);
    }
    applyStylePresetSpec(spec);
}

bool MultiLayer::applyStylePresetById(const QString &idOrName)
{
    GraphStylePresetSpec spec;
    if (!findStylePreset(idOrName, &spec))
        return false;
    applyStylePresetSpec(spec);
    return true;
}

void MultiLayer::applyStylePresetSpec(const GraphStylePresetSpec &spec)
{
    const QString beforeSnap = captureUndoSnapshot();
    // Remembered for graph.to_python (re-applied by the generated script); session only.
    setProperty("toriAppliedStylePreset", spec.id);

    const bool isIsijFormat = spec.formatIsijUnitSlash;
    const bool isSlidePreset = spec.largeSlideLayout;
    const QString fontFam = spec.fontFamily.trimmed().isEmpty() ? QStringLiteral("Arial")
                                                                : spec.fontFamily.trimmed();

    d_keep_aspect_ratio = spec.keepAspectRatio;
    d_target_aspect_ratio = (spec.pageHeightMm > 1e-6)
            ? (spec.pageWidthMm / spec.pageHeightMm)
            : 1.0;
    setPageBackgroundColor(spec.pageBackground.isValid() ? spec.pageBackground : QColor(Qt::white));

    double pageW = qBound(20.0, spec.pageWidthMm, 1000.0);
    double pageH = qBound(20.0, spec.pageHeightMm, 1000.0);
    const int targetDpi = qBound(72, spec.targetDpi, 2400);
    const int marginPx = qBound(0, spec.marginPx, 80);
    const Graph::FramePreset framePreset = static_cast<Graph::FramePreset>(
            qBound(0, spec.framePreset, 3));
    const int axisLw = qBound(1, spec.axisLineWidth, 10);
    const int minTickLen = qBound(0, spec.minorTickLen, 20);
    const int majTickLen = qBound(0, spec.majorTickLen, 30);

    QFont titleFnt(fontFam, qMax(5, qRound(spec.axisTitlePt)),
                   spec.boldTitle ? QFont::Bold : QFont::Normal, false);
    titleFnt.setPointSizeF(qBound(5.0, spec.axisTitlePt, 72.0));
    titleFnt.setBold(spec.boldTitle);

    QFont tickFnt(fontFam, qMax(5, qRound(spec.tickLabelPt)),
                  spec.boldTick ? QFont::Bold : QFont::Normal, false);
    tickFnt.setPointSizeF(qBound(5.0, spec.tickLabelPt, 72.0));
    tickFnt.setBold(spec.boldTick);

    QFont legFnt(fontFam, qMax(5, qRound(spec.legendPt)),
                 spec.boldLegend ? QFont::Bold : QFont::Normal, false);
    legFnt.setPointSizeF(qBound(5.0, spec.legendPt, 72.0));
    legFnt.setBold(spec.boldLegend);

    const int legFrame = qBound(0, spec.legendFrame, 2);
    const QColor legBg(255, 255, 255, (legFrame == Legend::None) ? 0 : 235);
    const double targetCurveWidth = (spec.curveWidthF > 0.0)
            ? qBound(0.5, spec.curveWidthF, 20.0)
            : double(qBound(1, spec.minCurveWidth, 20));
    const int minSymSize = qBound(0, spec.minSymbolSize, 100);
    // Values regarded as "not customised by the user": the application defaults used for new
    // curves plus every known preset's value.
    QVector<double> managedCurveWidths;
    QVector<int> managedSymbolSizes;
    {
        ApplicationWindow *mApp = findMultiLayerApp(this);
        managedCurveWidths << double(mApp ? mApp->defaultCurveLineWidth : 2) << 2.0;
        managedSymbolSizes << (mApp ? mApp->defaultSymbolSize : 9) << 9;
        for (const GraphStylePresetSpec &ps : listStylePresets()) {
            const double pw = (ps.curveWidthF > 0.0) ? ps.curveWidthF : double(ps.minCurveWidth);
            if (!isPresetManagedValue(pw, managedCurveWidths))
                managedCurveWidths << pw;
            if (!managedSymbolSizes.contains(ps.minSymbolSize))
                managedSymbolSizes << ps.minSymbolSize;
        }
    }
    const QString undoLabel = spec.name.trimmed().isEmpty()
            ? tr("Apply Style Preset")
            : tr("Apply Preset: %1").arg(spec.name.trimmed());

    titleFnt.setHintingPreference(QFont::PreferNoHinting);
    tickFnt.setHintingPreference(QFont::PreferNoHinting);
    legFnt.setHintingPreference(QFont::PreferNoHinting);

    // E7: multi-panel figures get either "preset size per panel" or an explicit journal
    // double-column width (175 mm). The old 0.85/0.90 shrink factors produced odd sizes such as
    // 153 x 85 mm.
    d_last_preset_warnings.clear();
    const int panelCols = qMax(1, cols);
    const int panelRows = qMax(1, rows);
    if (panelCols > 1 || panelRows > 1) {
        if (multiPanelSizing() == PanelSizingTwoColumn175) {
            const double aspectHW = (pageW > 1e-6) ? (pageH / pageW) : 1.0;
            const double panelW = 175.0 / double(panelCols);
            pageW = 175.0;
            pageH = qBound(20.0, panelW * aspectHW * double(panelRows), 1000.0);
        } else {
            pageW = qBound(20.0, pageW * double(panelCols), 1000.0);
            pageH = qBound(20.0, pageH * double(panelRows), 1000.0);
        }
    }
    if (panelCols <= 1 && panelRows <= 1 && pageW < 110.0) {
        for (int k = 0; k < graphsList.count(); ++k) {
            const Graph *gk = qobject_cast<const Graph *>(graphsList.at(k));
            if (gk && !d_inset_specs.contains(const_cast<Graph *>(gk)) && !gk->linkedParentLayer()
                && gk->isLegendVisible() && gk->plotWidget()
                && gk->plotWidget()->property("_outsideRightMargin").toInt() > 0) {
                pageW = 115.0;
                break;
            }
        }
    }

    setMargins(marginPx, marginPx, marginPx, marginPx);
    double initialViewZoom = (pageW <= 110.0 && pageH <= 110.0) ? 1.6 : 1.0;
    if (pageW <= 110.0 && pageH <= 110.0 && isVisible()) {
        const double basePxW = std::max(120.0, (pageW / 25.4) * 96.0);
        const double basePxH = std::max(120.0, (pageH / 25.4) * 96.0);
        int availW = 0;
        int availH = 0;
        QScrollArea *cs = widget()
                ? widget()->findChild<QScrollArea *>(QStringLiteral("MultiLayerCanvasScroll"))
                : nullptr;
        if (isMaximized() || !mdiArea()) {
            if (cs && cs->viewport() && cs->viewport()->isVisible()
                && cs->viewport()->width() >= 100 && cs->viewport()->height() >= 100) {
                availW = cs->viewport()->width() - 20;
                availH = cs->viewport()->height() - 20;
            }
        } else if (mdiArea() && mdiArea()->viewport() && mdiArea()->viewport()->isVisible()
                   && mdiArea()->viewport()->width() >= 160
                   && mdiArea()->viewport()->height() >= 160) {
            const int vpW = mdiArea()->viewport()->width();
            const int vpH = mdiArea()->viewport()->height();
            const int styleFrameW =
                    qMax(4, style()->pixelMetric(QStyle::PM_MdiSubWindowFrameWidth, nullptr, this));
            const int styleTitleH =
                    qMax(24, style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, this));
            const int topBarH = (d_layer_pill_frame && !d_layer_pill_frame->isHidden())
                    ? qMax(LayerButton::btnSize() + 4, d_layer_pill_frame->sizeHint().height() + 6)
                    : (LayerButton::btnSize() + 4);
            availW = vpW - (2 * styleFrameW + 20);
            availH = vpH - (styleTitleH + 2 * styleFrameW + topBarH + 20);
        }
        if (availW >= 90 && availH >= 90) {
            const double fitZoom = std::min(double(availW) / basePxW, double(availH) / basePxH);
            initialViewZoom = qBound(0.75, std::min(initialViewZoom, fitZoom), 1.6);
        }
    }
    setPageZoom(1.0);
    d_zoom_applied_lcanvas = QSize();
    d_zoom_base_lcanvas = QSizeF();
    for (int i = 0; i < graphsList.count(); ++i) {
        if (Graph *gr = qobject_cast<Graph *>(graphsList.at(i))) {
            gr->setProperty(kZoomAppliedGeomProp, QVariant());
            gr->setProperty(kZoomBaseGeomProp, QVariant());
        }
    }
    setPageSizePhysical(pageW, pageH, UnitMm, targetDpi);
    d_keep_aspect_ratio = spec.keepAspectRatio;
    d_target_aspect_ratio = (pageH > 1.0) ? (pageW / pageH) : 1.0;

    static const QRegularExpression kBoldTagRx(QStringLiteral("</?b\\s*>"),
                                               QRegularExpression::CaseInsensitiveOption);

    QVector<bool> legendWasDefaultTopLeft(graphsList.count(), false);

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr || !gr->plotWidget())
            continue;

        const bool isInset = d_inset_specs.contains(gr);
        const bool isLinkedOverlay = (gr->linkedParentLayer() != nullptr);

        if (!isLinkedOverlay && !isInset) {
            if (Legend *leg = gr->legend()) {
                const QRect cRect = gr->plotWidget()->plotLayout()->canvasRect();
                const QRect lRect = leg->rect();
                if (cRect.width() > 30 && cRect.height() > 30) {
                    if (lRect.left() <= cRect.width() * 0.28
                        && lRect.top() <= cRect.height() * 0.28) {
                        legendWasDefaultTopLeft[i] = true;
                    }
                } else {
                    legendWasDefaultTopLeft[i] = true;
                }
            }
            if (gr->isPoleFigurePlot()) {
                gr->enableAxis(QwtPlot::xBottom, false);
                gr->enableAxis(QwtPlot::yLeft, false);
                gr->enableAxis(QwtPlot::xTop, false);
                if (!gr->hasColorScaleOnAxis(QwtPlot::yRight))
                    gr->enableAxis(QwtPlot::yRight, false);
                gr->drawCanvasFrame(false, 0);
                gr->drawAxesBackbones(false);
            } else if (!gr->isTernaryPlot() && !gr->isPolarPlot() && !gr->isPiePlot()) {
                gr->applyFramePreset(framePreset);
                for (int k = 0; k < graphsList.count(); ++k) {
                    Graph *ch = qobject_cast<Graph *>(graphsList.at(k));
                    if (!ch || ch->linkedParentLayer() != gr)
                        continue;
                    if (Plot *pPlot = gr->plotWidget()) {
                        if (ch->xLinkMode() != 0) {
                            pPlot->setMajorTicksType(QwtPlot::xTop, ScaleDraw::None);
                            pPlot->setMinorTicksType(QwtPlot::xTop, ScaleDraw::None);
                        }
                        if (ch->yLinkMode() != 0) {
                            pPlot->setMajorTicksType(QwtPlot::yRight, ScaleDraw::None);
                            pPlot->setMinorTicksType(QwtPlot::yRight, ScaleDraw::None);
                        }
                    }
                }
            } else {
                for (int a = 0; a < QwtPlot::axisCnt; ++a)
                    gr->enableAxis(a, false);
                gr->drawAxesBackbones(false);
            }
            gr->setBackgroundColor(QColor(Qt::white));
            gr->setCanvasBackground(QColor(Qt::white));
        } else if (isInset) {
            gr->setBackgroundColor(QColor(255, 255, 255, 0));
            gr->setCanvasBackground(QColor(Qt::white));
            gr->setAutoFillBackground(false);
            if (gr->plotWidget()) {
                gr->plotWidget()->setAutoFillBackground(false);
                gr->plotWidget()->setMargin(4);
            }
        }

        if (isInset) {
            gr->setAxesLinewidth(isSlidePreset ? 2 : 1);
            gr->setTicksLength(isSlidePreset ? 3 : 2,
                               isSlidePreset ? 6 : 4);
        } else {
            gr->setAxesLinewidth(axisLw);
            gr->changeTicksLength(minTickLen, majTickLen);
        }

        Plot *plot = gr->plotWidget();
        QwtText pTitle = plot->title();
        QString cleanPlotTitle = pTitle.text();
        cleanPlotTitle.remove(kBoldTagRx);
        pTitle.setText(cleanPlotTitle);
        pTitle.setFont(titleFnt);
        plot->setTitle(pTitle);

        if (isIsijFormat && !isInset) {
            gr->setAxisUnitFormat(Column::UnitFormat::ISIJSlash);
            gr->refreshAutoAxisTitlesFromColumns();
        }

        auto formatPresetAxisTitle = [](const QString &rawIn, bool toISIJSlash) -> QString {
            QString s = rawIn;
            static const QString kNoWrapOpen = QStringLiteral("<span style=\"white-space:nowrap;\">");
            static const QString kNoWrapClose = QStringLiteral("</span>");
            while (s.startsWith(kNoWrapOpen, Qt::CaseInsensitive)
                   && s.endsWith(kNoWrapClose, Qt::CaseInsensitive)
                   && s.size() >= kNoWrapOpen.size() + kNoWrapClose.size()) {
                s = s.mid(kNoWrapOpen.size(),
                          s.size() - kNoWrapOpen.size() - kNoWrapClose.size());
            }
            if (s.trimmed().isEmpty())
                return s;

            if (s.contains(QLatin1Char('^')) || s.contains(QLatin1Char('_'))) {
                static const QRegularExpression kHtmlSplitRx(QStringLiteral("(<[^>]+>)"));
                static const QRegularExpression kBrSupRx(QStringLiteral("\\^\\{([^}]+)\\}"));
                static const QRegularExpression kBrSubRx(QStringLiteral("_\\{([^}]+)\\}"));
                static const QRegularExpression kPlSupRx(QStringLiteral(
                        "([A-Za-z0-9\\)\\]\\x{0370}-\\x{03FF}\\x{00B0}\\x{00C5}])\\^([+\\-\\x{2212}]?\\d+(?:[./]\\d+)?)"));
                static const QRegularExpression kLog10Rx(QStringLiteral("\\blog_10\\b"));
                static const QRegularExpression kSymSubRx(QStringLiteral(
                        "(?<![A-Za-z0-9\\x{0370}-\\x{03FF}])([A-Za-z\\x{0370}-\\x{03FF}])_([A-Za-z0-9+\\-]{1,4}(?:\\.\\d+)?)(?![A-Za-z0-9_])"));

                QString rebuilt;
                rebuilt.reserve(s.size() + 24);
                int subD = 0, supD = 0, pos = 0;
                auto xform = [&](QString ch) -> QString {
                    if (subD > 0 || supD > 0 || ch.isEmpty())
                        return ch;
                    ch.replace(kBrSupRx, QStringLiteral("<sup>\\1</sup>"));
                    ch.replace(kBrSubRx, QStringLiteral("<sub>\\1</sub>"));
                    ch.replace(kPlSupRx, QStringLiteral("\\1<sup>\\2</sup>"));
                    ch.replace(kLog10Rx, QStringLiteral("log<sub>10</sub>"));
                    ch.replace(kSymSubRx, QStringLiteral("\\1<sub>\\2</sub>"));
                    return ch;
                };
                auto it = kHtmlSplitRx.globalMatch(s);
                while (it.hasNext()) {
                    const QRegularExpressionMatch m = it.next();
                    if (m.capturedStart() > pos)
                        rebuilt += xform(s.mid(pos, m.capturedStart() - pos));
                    const QString tag = m.captured(1);
                    const QString tl = tag.toLower();
                    if (tl.startsWith(QLatin1String("<sub")) && !tl.endsWith(QLatin1String("/>")))
                        ++subD;
                    else if (tl.startsWith(QLatin1String("</sub")) && subD > 0)
                        --subD;
                    else if (tl.startsWith(QLatin1String("<sup"))
                             && !tl.endsWith(QLatin1String("/>")))
                        ++supD;
                    else if (tl.startsWith(QLatin1String("</sup")) && supD > 0)
                        --supD;
                    rebuilt += tag;
                    pos = m.capturedEnd();
                }
                if (pos < s.size())
                    rebuilt += xform(s.mid(pos));
                static const QRegularExpression kPostTagSubRx(
                        QStringLiteral(
                                "(</\\s*i\\s*>)_([A-Za-z0-9+\\-]{1,4}(?:\\.\\d+)?)(?![A-Za-z0-9_])"),
                        QRegularExpression::CaseInsensitiveOption);
                static const QRegularExpression kPostTagSupRx(
                        QStringLiteral(
                                "(</\\s*(?:i|sub)\\s*>)\\^([+\\-\\x{2212}]?\\d+(?:[./]\\d+)?)"),
                        QRegularExpression::CaseInsensitiveOption);
                rebuilt.replace(kPostTagSubRx, QStringLiteral("\\1<sub>\\2</sub>"));
                rebuilt.replace(kPostTagSupRx, QStringLiteral("\\1<sup>\\2</sup>"));
                s = rebuilt;
            }

            if (toISIJSlash) {
                const QString tr = s.trimmed();
                if (tr.endsWith(QLatin1Char(')')) && !tr.contains(QLatin1String(" / "))) {
                    int depth = 0;
                    int openIdx = -1;
                    for (int idx = tr.size() - 1; idx >= 0; --idx) {
                        const QChar c = tr.at(idx);
                        if (c == QLatin1Char(')')) {
                            ++depth;
                        } else if (c == QLatin1Char('(')) {
                            --depth;
                            if (depth == 0) {
                                openIdx = idx;
                                break;
                            }
                        }
                    }
                    if (openIdx >= 2 && tr.at(openIdx - 1).isSpace()) {
                        const QString base = tr.left(openIdx).trimmed();
                        const QString unit = tr.mid(openIdx + 1, tr.size() - openIdx - 2).trimmed();
                        QString plainUnit = unit;
                        plainUnit.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
                        const QString uLow = plainUnit.trimmed().toLower();
                        static const QRegularExpression kTemperCodeRx(
                                QStringLiteral("^(?:T\\d+|O|F|H\\d+|RRA)$"));
                        const bool keepParen = base.isEmpty() || unit.isEmpty()
                                || plainUnit == QLatin1String("-")
                                || plainUnit == QStringLiteral("\u2013")
                                || plainUnit == QStringLiteral("\u2014")
                                || plainUnit == QStringLiteral("\u2212")
                                || uLow == QLatin1String("none")
                                || uLow == QLatin1String("dimensionless")
                                || uLow == QLatin1String("a.u.") || uLow == QLatin1String("a.u")
                                || uLow == QLatin1String("au")
                                || uLow == QLatin1String("arb. units")
                                || uLow == QLatin1String("arb. unit")
                                || uLow == QLatin1String("arb.units")
                                || uLow == QLatin1String("arbitrary units")
                                || kTemperCodeRx.match(plainUnit.trimmed()).hasMatch();
                        if (!keepParen)
                            s = base + QStringLiteral(" / ") + unit;
                    }
                }
            }
            return s;
        };

        const int insetPt = isSlidePreset ? 14 : 8;
        const bool applyPresetTitleFmt = !isInset && (isIsijFormat || isSlidePreset);
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            QFont useTick = isInset ? QFont(fontFam, insetPt, QFont::Normal, false) : tickFnt;
            useTick.setHintingPreference(QFont::PreferNoHinting);
            QFont useTitle = isInset ? QFont(fontFam, insetPt, QFont::Normal, false) : titleFnt;
            useTitle.setHintingPreference(QFont::PreferNoHinting);
            gr->setAxisFont(a, useTick);
            gr->setAxisTitleFont(a, useTitle);
            if (QwtScaleWidget *sw = plot->axisWidget(a)) {
                const double pt = (useTick.pointSizeF() > 0.0) ? useTick.pointSizeF() : 12.0;
                sw->setSpacing(qMax(8, qRound(pt * 0.45)));
            }
            QString rawTitle = gr->axisTitle(a);
            QString nextAxisTitle = rawTitle;
            nextAxisTitle.remove(kBoldTagRx);
            if (applyPresetTitleFmt) {
                nextAxisTitle = formatPresetAxisTitle(nextAxisTitle, isIsijFormat);
            }
            if (nextAxisTitle != rawTitle) {
                const bool wasAuto = gr->isAxisTitleAuto(a);
                gr->setAxisTitleAuto(a, false);
                gr->setAxisTitle(a, nextAxisTitle);
                gr->setAxisTitleAuto(a, wasAuto);
                gr->setAxisTitleFont(a, useTitle);
            }
        }

        for (QwtPlotItem *it : plot->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                Spectrogram *sp = static_cast<Spectrogram *>(it);
                if (applyPresetTitleFmt) {
                    QString cbT = sp->colorBarTitle();
                    if (cbT.trimmed().isEmpty() && sp->hasColorScale())
                        cbT = gr->axisTitle(sp->colorScaleAxis());
                    if (!cbT.trimmed().isEmpty()) {
                        cbT.remove(kBoldTagRx);
                        const QString fmtCbT = formatPresetAxisTitle(cbT, isIsijFormat);
                        if (fmtCbT != sp->colorBarTitle())
                            sp->setColorBarTitle(fmtCbT);
                    }
                }
                sp->enforceColorBarArialFont();
                if (sp->hasColorScale()) {
                    QFont useCbTitle =
                            isInset ? QFont("Arial", insetPt, QFont::Normal, false) : titleFnt;
                    useCbTitle.setHintingPreference(QFont::PreferNoHinting);
                    gr->setAxisTitleFont(sp->colorScaleAxis(), useCbTitle);
                }
                if (gr->isPoleFigurePlot())
                    sp->syncColorScale();
            }
        }
        if (DataCurve *pcm = gr->pointColorMapCurve()) {
            if (applyPresetTitleFmt) {
                DataCurve::PointColorMap pm = pcm->pointColorMap();
                QString cbT = pm.colorBarTitle;
                if (cbT.trimmed().isEmpty())
                    cbT = gr->axisTitle(QwtPlot::yRight);
                if (!cbT.trimmed().isEmpty()) {
                    cbT.remove(kBoldTagRx);
                    const QString fmtCbT = formatPresetAxisTitle(cbT, isIsijFormat);
                    if (fmtCbT != pm.colorBarTitle) {
                        pm.colorBarTitle = fmtCbT;
                        pcm->setPointColorMap(pm);
                    }
                }
            }
            gr->syncPointColorBar();
            QFont useCbTitle = isInset ? QFont("Arial", insetPt, QFont::Normal, false) : titleFnt;
            useCbTitle.setHintingPreference(QFont::PreferNoHinting);
            gr->setAxisTitleFont(QwtPlot::yRight, useCbTitle);
        }

        static const QRegularExpression kPanelLabelMarkerRx(
                QStringLiteral("^\\s*\\(?[A-Za-z0-9]{1,2}\\)?\\.?\\s*$"));
        gr->setTextMarkerDefaults(legFrame, legFnt, QColor(Qt::black), legBg);
        const QVector<int> txtKeys = gr->textMarkerKeys();
        for (int k = 0; k < txtKeys.size(); ++k) {
            Legend *mrk = gr->textMarker(txtKeys[k]);
            if (!mrk)
                continue;
            const QString rawMrkText = mrk->text();
            const QString trimmedMrkText = rawMrkText.trimmed();
            const bool isCurveLegend = (mrk == gr->legend()
                                        || rawMrkText.contains(QLatin1String("\\c{"))
                                        || rawMrkText.contains(QLatin1String("\\p{")));
            const bool isPanelLabel = kPanelLabelMarkerRx.match(trimmedMrkText).hasMatch();
            const bool isFitCallout = trimmedMrkText.contains(QStringLiteral("R\u00B2"))
                    || trimmedMrkText.contains(QLatin1String("R^2"))
                    || trimmedMrkText.contains(QLatin1String("Chi^2"))
                    || (trimmedMrkText.contains(QLatin1Char('='))
                        && trimmedMrkText.contains(QStringLiteral("\u00B1")));
            const bool isSigLabel = !isCurveLegend && !isPanelLabel && !isFitCallout
                    && (trimmedMrkText == QLatin1String("*")
                        || trimmedMrkText == QLatin1String("**")
                        || trimmedMrkText == QLatin1String("***")
                        || trimmedMrkText == QLatin1String("****")
                        || trimmedMrkText.compare(QLatin1String("ns"), Qt::CaseInsensitive) == 0
                        || trimmedMrkText.startsWith(QLatin1String("p="), Qt::CaseInsensitive)
                        || trimmedMrkText.startsWith(QLatin1String("p "), Qt::CaseInsensitive)
                        || trimmedMrkText.startsWith(QLatin1String("<i>p</i>"),
                                                     Qt::CaseInsensitive));
            const QFont mf = mrk->font();
            const double curPt = mf.pointSizeF() > 0.0
                    ? mf.pointSizeF()
                    : (mf.pointSize() > 0 ? double(mf.pointSize()) : 0.0);
            const double maxAutoScalePt = isSlidePreset ? 14.0 : 12.0;
            const bool shouldApplyLegFnt = !isSigLabel
                    && (isCurveLegend || isPanelLabel || isFitCallout
                        || (curPt > 0.0 && curPt <= maxAutoScalePt + 1e-6));
            if (isSigLabel) {
                QFont sigFnt = legFnt;
                const double baseLegPt = legFnt.pointSizeF() > 0.0
                        ? legFnt.pointSizeF()
                        : (legFnt.pointSize() > 0 ? double(legFnt.pointSize()) : 9.0);
                sigFnt.setPointSizeF(
                        qBound(8.0, baseLegPt - (isSlidePreset ? 4.0 : 0.0), 16.0));
                sigFnt.setBold(false);
                sigFnt.setHintingPreference(QFont::PreferNoHinting);
                mrk->setFont(sigFnt);
            } else if (shouldApplyLegFnt) {
                mrk->setFont(legFnt);
            } else {
                QFont customFnt = mrk->font();
                customFnt.setFamily(fontFam);
                customFnt.setBold(false);
                customFnt.setHintingPreference(QFont::PreferNoHinting);
                mrk->setFont(customFnt);
            }
            if (isCurveLegend || mrk->textColor().lightness() <= 200)
                mrk->setTextColor(QColor(Qt::black));
            if (isCurveLegend) {
                mrk->setFrameStyle(legFrame);
                mrk->setBackgroundColor(legBg);
            }
        }

        // E2: the preset's curve width / symbol size are set values (so switching presets can
        // also make lines thinner again). Curves the user adjusted by hand keep their values: a
        // value counts as "managed" only if it equals the app default or some preset's value.
        for (int cIdx = 0; cIdx < gr->curves(); ++cIdx) {
            QwtPlotCurve *c = gr->curve(cIdx);
            if (!c)
                continue;
            const bool isFilledType = dynamic_cast<QwtBarCurve *>(c) != nullptr
                    || dynamic_cast<QwtHistogram *>(c) != nullptr
                    || dynamic_cast<BoxCurve *>(c) != nullptr
                    || dynamic_cast<QwtPieCurve *>(c) != nullptr
                    || dynamic_cast<QwtErrorPlotCurve *>(c) != nullptr
                    || c->rtti() != QwtPlotItem::Rtti_PlotCurve;
            QPen pen = c->pen();
            if (isFilledType) {
                // Bar/box outlines and error bars keep their own widths, except that very thin
                // outlines are still raised to 1 px on slides.
                if (pen.style() != Qt::NoPen && isSlidePreset && pen.widthF() < 1.0) {
                    pen.setWidth(1);
                    c->setPen(pen);
                }
            } else if (pen.style() != Qt::NoPen
                       && isPresetManagedValue(pen.widthF(), managedCurveWidths)) {
                pen.setWidthF(targetCurveWidth);
                c->setPen(pen);
            }
            if (!isFilledType && minSymSize > 0 && c->symbol().style() != QwtSymbol::NoSymbol) {
                QwtSymbol sym = c->symbol();
                if (sym.size().width() == sym.size().height()
                    && isPresetManagedValue(double(sym.size().width()), managedSymbolSizes)) {
                    sym.setSize(QSize(minSymSize, minSymSize));
                }
                if (isSlidePreset) {
                    QPen sp = sym.pen();
                    if (sp.widthF() < 2.0)
                        sp.setWidthF(2.0);
                    sym.setPen(sp);
                }
                c->setSymbol(sym);
            }
        }
        if ((isIsijFormat || isSlidePreset || isInset)
            && !isLinkedOverlay && !gr->isTernaryPlot() && !gr->isPolarPlot()
            && !gr->isPoleFigurePlot() && !gr->isPiePlot()) {
            plot->updateAxes();
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                if (gr->hasColorScaleOnAxis(a) || gr->hasWeibullProbabilityAxis(a)
                    || gr->hasCustomTickLabels(a)) {
                    if (gr->hasWeibullProbabilityAxis(a) || gr->hasCustomTickLabels(a))
                        gr->updateSecondaryAxis(a);
                    continue;
                }
                if (const QwtTextScaleDraw *tsd =
                            dynamic_cast<const QwtTextScaleDraw *>(plot->axisScaleDraw(a))) {
                    const QMap<int, QString> catMap = tsd->labelsMap();
                    plot->setAxisMaxMinor(a, 0);
                    gr->plotWidget()->setMinorTicksType(a, ScaleDraw::None);
                    if (!catMap.isEmpty()) {
                        const double minKey = double(catMap.firstKey());
                        const double maxKey = double(catMap.lastKey());
                        const bool isInvCat = plot->axisScaleEngine(a)
                                && plot->axisScaleEngine(a)->testAttribute(
                                        QwtScaleEngine::Inverted);
                        double cLo = minKey - 0.5;
                        double cHi = maxKey + 0.5;
                        if (gr->isAxisUserLocked(a)) {
                            if (const QwtScaleDiv *curDiv = plot->axisScaleDiv(a)) {
                                const double curLo = qMin(curDiv->lowerBound(), curDiv->upperBound());
                                const double curHi = qMax(curDiv->lowerBound(), curDiv->upperBound());
                                if (std::isfinite(curLo) && std::isfinite(curHi) && curHi > curLo
                                    && curLo >= minKey - 1.5 && curLo <= minKey
                                    && curHi >= maxKey && curHi <= maxKey + 4.5) {
                                    cLo = qMin(cLo, curLo);
                                    cHi = qMax(cHi, curHi);
                                }
                            }
                        }
                        QwtValueList catMaj;
                        for (auto it = catMap.constBegin(); it != catMap.constEnd(); ++it)
                            catMaj += double(it.key());
                        QwtValueList emptyTicks;
                        QwtValueList catTicks[QwtScaleDiv::NTickTypes];
                        catTicks[QwtScaleDiv::MinorTick] = emptyTicks;
                        catTicks[QwtScaleDiv::MediumTick] = emptyTicks;
                        catTicks[QwtScaleDiv::MajorTick] = catMaj;
                        QwtScaleEngine *catEng = new QwtLinearScaleEngine();
                        QwtScaleDiv catDiv(cLo, cHi, catTicks);
                        if (isInvCat) {
                            catEng->setAttribute(QwtScaleEngine::Inverted);
                            catDiv.invert();
                        }
                        plot->setAxisMaxMajor(a, qMax(8, catMap.size() + 2));
                        plot->setAxisScale(a, isInvCat ? cHi : cLo, isInvCat ? cLo : cHi, 1.0);
                        plot->setAxisScaleEngine(a, catEng);
                        plot->setAxisScaleDiv(a, catDiv);
                        if (a == QwtPlot::xBottom && gr->labelsRotation(QwtPlot::xBottom) == 0
                            && catMap.size() >= 3) {
                            const QFontMetrics cfm(gr->axisFont(QwtPlot::xBottom));
                            int sumCatW = 0;
                            for (auto it = catMap.constBegin(); it != catMap.constEnd(); ++it)
                                sumCatW += cfm.horizontalAdvance(it.value()) + 8;
                            const int estW = (plot->plotLayout()
                                              && plot->plotLayout()->canvasRect().width() >= 80)
                                    ? plot->plotLayout()->canvasRect().width()
                                    : qMax(140,
                                           qRound(0.70 * qMax(160, canvas ? canvas->width() : 321)));
                            if (sumCatW > qRound(0.84 * double(estW)))
                                gr->setAxisLabelRotation(QwtPlot::xBottom, -35);
                        }
                        const int opp = (a == QwtPlot::xBottom) ? QwtPlot::xTop
                                : (a == QwtPlot::yLeft)         ? QwtPlot::yRight
                                                                : -1;
                        if (opp >= 0 && !gr->plotWidget()->hasVisibleItemOnAxis(opp)
                            && !gr->hasColorScaleOnAxis(opp)
                            && !gr->hasWeibullProbabilityAxis(opp)
                            && !gr->hasCustomTickLabels(opp)) {
                            gr->plotWidget()->setMinorTicksType(opp, ScaleDraw::None);
                            gr->plotWidget()->setMajorTicksType(opp, ScaleDraw::None);
                        }
                    }
                    continue;
                }
                const int insetMaj =
                        (a == QwtPlot::xBottom || a == QwtPlot::xTop) ? 2 : 3;
                if ((a == QwtPlot::xTop || a == QwtPlot::yRight)
                    && !gr->plotWidget()->hasVisibleItemOnAxis(a)) {
                    if (isInset) {
                        plot->setAxisMaxMajor(a, insetMaj);
                        plot->setAxisMaxMinor(a, 1);
                    }
                    continue;
                }
                const QwtScaleEngine *se = plot->axisScaleEngine(a);
                const QwtScaleDiv *sd = plot->axisScaleDiv(a);
                if (!se || !sd)
                    continue;
                const bool isLog = Plot::isLog10ScaleEngine(se);
                const bool isInv = se->testAttribute(QwtScaleEngine::Inverted);
                const int targetMinTicks = isInset ? 1 : (isLog ? 8 : 1);
                const int maxMinInt = isInset ? 2 : (isLog ? 9 : 2);
                const double lo = qMin(sd->lowerBound(), sd->upperBound());
                const double hi = qMax(sd->lowerBound(), sd->upperBound());
                const QwtValueList majTicks = sd->ticks(QwtScaleDiv::MajorTick);
                if (isInset)
                    plot->setAxisMaxMajor(a, insetMaj);
                plot->setAxisMaxMinor(a, targetMinTicks);

                if (gr->axisBreak(a).enabled) {
                    gr->setAxisBreak(a, gr->axisBreak(a));
                    continue;
                }

                if (!(hi > lo) || !std::isfinite(lo) || !std::isfinite(hi))
                    continue;

                bool crowdedHorizTicks = false;
                if (!isInset && !isLog && a == QwtPlot::xBottom && majTicks.size() > 5) {
                    const QFontMetrics tfm(gr->axisFont(a));
                    int sumLabelW = 0;
                    for (int ti = 0; ti < majTicks.size(); ++ti) {
                        const QString lbl = plot->axisScaleDraw(a)
                                ? plot->axisScaleDraw(a)->label(majTicks[ti]).text()
                                : QString::number(majTicks[ti], 'g', 6);
                        sumLabelW += tfm.horizontalAdvance(lbl) + 8;
                    }
                    const int estCanvasW =
                            (plot->plotLayout() && plot->plotLayout()->canvasRect().width() >= 80)
                            ? plot->plotLayout()->canvasRect().width()
                            : qMax(140, qRound(0.70 * qMax(160, canvas ? canvas->width() : 321)));
                    if (sumLabelW > qRound(0.88 * double(estCanvasW)))
                        crowdedHorizTicks = true;
                }

                int maj = plot->axisMaxMajor(a);
                if (isInset)
                    maj = insetMaj;
                else if (isSlidePreset || crowdedHorizTicks)
                    maj = qMin(maj, crowdedHorizTicks ? 4 : 5);
                double step = isInset ? 0.0 : gr->axisStep(a);
                if (!isInset && step <= 0.0 && !isLog && majTicks.size() >= 2) {
                    if (!((isSlidePreset && majTicks.size() > 6)
                          || crowdedHorizTicks))
                        step = std::fabs(majTicks[1] - majTicks[0]);
                }

                QwtScaleEngine *newEng = isLog
                        ? static_cast<QwtScaleEngine *>(new QwtLog10ScaleEngine())
                        : static_cast<QwtScaleEngine *>(new QwtLinearScaleEngine());
                QwtScaleDiv newDiv =
                        newEng->divideScale(lo, hi, maj, maxMinInt, isLog ? 0.0 : step);
                if (!isLog && targetMinTicks == 1
                    && newDiv.ticks(QwtScaleDiv::MinorTick).isEmpty()
                    && newDiv.ticks(QwtScaleDiv::MediumTick).isEmpty()) {
                    const QwtValueList newMaj = newDiv.ticks(QwtScaleDiv::MajorTick);
                    if (newMaj.size() >= 2) {
                        const double s = std::fabs(newMaj[1] - newMaj[0]);
                        const double half = 0.5 * s;
                        const double dLo = qMin(newDiv.lowerBound(), newDiv.upperBound());
                        const double dHi = qMax(newDiv.lowerBound(), newDiv.upperBound());
                        const double eps = 1e-9 * s;
                        double fM = newMaj.first();
                        double lM = newMaj.last();
                        if (fM > lM)
                            qSwap(fM, lM);
                        QwtValueList mids;
                        if (fM - half >= dLo - eps)
                            mids += (fM - half);
                        for (int mi = 0; mi + 1 < newMaj.size(); ++mi) {
                            const double mv = 0.5 * (newMaj[mi] + newMaj[mi + 1]);
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
                plot->setAxisScale(a, isInv ? hi : lo, isInv ? lo : hi, isLog ? 0.0 : step);
                plot->setAxisScaleEngine(a, newEng);
                plot->setAxisScaleDiv(a, newDiv);
            }
            gr->updateSecondaryAxis(QwtPlot::xTop);
            gr->updateSecondaryAxis(QwtPlot::yRight);
            gr->syncChildLinkedLayers();
        }

        plot->updateLayout();
        plot->replot();
    }

    applySquareCanvasFrame70Percent();

    {
        bool anyPostRotated = false;
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *gr = (Graph *)graphsList.at(i);
            if (!gr || !gr->plotWidget() || d_inset_specs.contains(gr) || gr->linkedParentLayer()
                || gr->isTernaryPlot() || gr->isPolarPlot() || gr->isPoleFigurePlot()
                || gr->isPiePlot())
                continue;
            Plot *pl = gr->plotWidget();
            pl->updateLayout();
            if (const QwtTextScaleDraw *xCatSd = dynamic_cast<const QwtTextScaleDraw *>(
                        pl->axisScaleDraw(QwtPlot::xBottom))) {
                const QMap<int, QString> cMap = xCatSd->labelsMap();
                if (cMap.size() >= 2 && gr->labelsRotation(QwtPlot::xBottom) == 0) {
                    const QFontMetrics xfm(gr->axisFont(QwtPlot::xBottom));
                    const int minGapPx = qMax(10, xfm.height() / 2);
                    int sumW = 0;
                    int maxItemW = 0;
                    for (auto it = cMap.constBegin(); it != cMap.constEnd(); ++it) {
                        const int w = xfm.horizontalAdvance(it.value());
                        sumW += w + minGapPx;
                        maxItemW = qMax(maxItemW, w);
                    }
                    const int finalCw = qMax(60, pl->plotLayout()->canvasRect().width());
                    const int slotW = finalCw / qMax(1, cMap.size());
                    if (sumW > qRound(0.82 * double(finalCw)) || maxItemW + minGapPx > slotW) {
                        gr->setAxisLabelRotation(QwtPlot::xBottom, -35);
                        pl->updateLayout();
                        anyPostRotated = true;
                    }
                }
            }
        }
        if (anyPostRotated)
            applySquareCanvasFrame70Percent();
    }

    {
        // E2: axis titles keep the point size given by the preset. A title wider than its axis
        // is reported (status bar + lastStylePresetWarnings()) instead of being silently
        // wrapped onto two lines or shrunk.
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *gr = (Graph *)graphsList.at(i);
            if (!gr || !gr->plotWidget() || d_inset_specs.contains(gr) || gr->linkedParentLayer()
                || gr->isTernaryPlot() || gr->isPolarPlot() || gr->isPoleFigurePlot()
                || gr->isPiePlot())
                continue;
            Plot *pl = gr->plotWidget();
            pl->updateLayout();
            const QRect cr = pl->plotLayout()->canvasRect();
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                if (!pl->axisEnabled(a))
                    continue;
                const int spanPx =
                        (a == QwtPlot::xBottom || a == QwtPlot::xTop) ? cr.width() : cr.height();
                if (spanPx < 50)
                    continue;
                const QwtText curQt = pl->axisTitle(a);
                if (curQt.text().trimmed().isEmpty())
                    continue;
                if (curQt.textSize().width() <= spanPx - 6)
                    continue;
                QString plain = curQt.text();
                plain.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
                const QFont tf = gr->axisTitleFont(a);
                const double pt = (tf.pointSizeF() > 0.0) ? tf.pointSizeF() : double(tf.pointSize());
                d_last_preset_warnings << tr("Axis title \"%1\" is wider than the axis at %2 pt "
                                             "(size kept; shorten the title or enlarge the page).")
                                                  .arg(plain.trimmed())
                                                  .arg(pt, 0, 'g', 3);
            }
        }
        if (!d_last_preset_warnings.isEmpty()) {
            if (ApplicationWindow *wApp = findMultiLayerApp(this)) {
                if (wApp->statusBar())
                    wApp->statusBar()->showMessage(
                            d_last_preset_warnings.join(QStringLiteral("  ")), 12000);
            }
        }
    }

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr || !gr->plotWidget())
            continue;
        const bool isInset = d_inset_specs.contains(gr);
        const bool isLinkedOverlay = (gr->linkedParentLayer() != nullptr);
        if (!isInset && !isLinkedOverlay && !gr->isTernaryPlot() && !gr->isPolarPlot()
            && !gr->isPoleFigurePlot() && !gr->isPiePlot()) {
            Plot *pl = gr->plotWidget();
            pl->updateLayout();
            const QVector<int> lineKeys = gr->lineMarkerKeys();
            const QVector<int> txtKeys = gr->textMarkerKeys();
            if (!lineKeys.isEmpty() && !txtKeys.isEmpty()) {
                int maxSigBoxH = 0;
                for (int tid : txtKeys) {
                    Legend *tm = gr->textMarker(tid);
                    if (!tm || tm == gr->legend() || !tm->isVisible())
                        continue;
                    const QString st = tm->text().trimmed();
                    const bool isSig = (st == QLatin1String("*") || st == QLatin1String("**")
                                        || st == QLatin1String("***")
                                        || st == QLatin1String("****")
                                        || st.compare(QLatin1String("ns"), Qt::CaseInsensitive) == 0
                                        || st.startsWith(QLatin1String("p="), Qt::CaseInsensitive)
                                        || st.startsWith(QLatin1String("p "), Qt::CaseInsensitive)
                                        || st.startsWith(QLatin1String("<i>p</i>"),
                                                         Qt::CaseInsensitive));
                    if (isSig) {
                        QwtText qt(st);
                        qt.setFont(tm->font());
                        maxSigBoxH = qMax(maxSigBoxH, qt.textSize().height() + 4);
                    }
                }

                if (maxSigBoxH > 0 && pl->axisEnabled(QwtPlot::yLeft)
                    && !gr->axisBreak(QwtPlot::yLeft).enabled) {
                    const QwtScaleEngine *yEng = pl->axisScaleEngine(QwtPlot::yLeft);
                    const QwtScaleDiv *yDiv = pl->axisScaleDiv(QwtPlot::yLeft);
                    if (yEng && yDiv && !Plot::isLog10ScaleEngine(yEng)
                        && !yEng->testAttribute(QwtScaleEngine::Inverted)) {
                        double maxHorizBrkY = -1e300;
                        for (int lid : lineKeys) {
                            ArrowMarker *am = gr->arrow(lid);
                            if (!am || !am->isVisible() || am->yAxis() != QwtPlot::yLeft)
                                continue;
                            const QwtDoublePoint sC = am->startPointCoord();
                            const QwtDoublePoint eC = am->endPointCoord();
                            if (std::fabs(sC.y() - eC.y())
                                <= 1e-4 * qMax(1.0, std::fabs(sC.y()) + std::fabs(eC.y()))) {
                                maxHorizBrkY = qMax(maxHorizBrkY, qMax(sC.y(), eC.y()));
                            }
                        }
                        const QwtScaleMap yMap0 = pl->canvasMap(QwtPlot::yLeft);
                        const double chPx = qMax(40.0, std::fabs(yMap0.pDist()));
                        if (maxHorizBrkY > -1e290) {
                            const double topBrkPy = yMap0.xTransform(maxHorizBrkY);
                            const double reqTopPy = double(maxSigBoxH) + 8.0;
                            if (std::isfinite(topBrkPy) && topBrkPy < reqTopPy) {
                                const double yLo = qMin(yDiv->lowerBound(), yDiv->upperBound());
                                const double yHi = qMax(yDiv->lowerBound(), yDiv->upperBound());
                                const double reqFrac = qBound(0.10, reqTopPy / chPx, 0.38);
                                const double newYHi =
                                        qMax(yHi, (maxHorizBrkY - reqFrac * yLo) / (1.0 - reqFrac));
                                if (newYHi > yHi + 1e-9) {
                                    const QwtValueList mTicks =
                                            yDiv->ticks(QwtScaleDiv::MajorTick);
                                    const double step = (mTicks.size() >= 2)
                                            ? std::fabs(mTicks[1] - mTicks[0])
                                            : 0.0;
                                    gr->setScale(QwtPlot::yLeft, yLo, newYHi, step,
                                                 pl->axisMaxMajor(QwtPlot::yLeft),
                                                 pl->axisMaxMinor(QwtPlot::yLeft), 0, false);
                                    // setScale() already kept the markers in place; only the
                                    // secondary axis changes from here.
                                    const Graph::MarkerAxisState axesBefore =
                                            gr->markerAxisState();
                                    gr->updateSecondaryAxis(QwtPlot::yRight);
                                    gr->updateMarkersBoundingRect(axesBefore);
                                    pl->updateLayout();
                                }
                            }
                        }
                    }
                }

                for (int tid : txtKeys) {
                    Legend *tm = gr->textMarker(tid);
                    if (!tm || tm == gr->legend() || !tm->isVisible())
                        continue;
                    const QString st = tm->text().trimmed();
                    const bool isSig = (st == QLatin1String("*") || st == QLatin1String("**")
                                        || st == QLatin1String("***")
                                        || st == QLatin1String("****")
                                        || st.compare(QLatin1String("ns"), Qt::CaseInsensitive) == 0
                                        || st.startsWith(QLatin1String("p="), Qt::CaseInsensitive)
                                        || st.startsWith(QLatin1String("p "), Qt::CaseInsensitive)
                                        || st.startsWith(QLatin1String("<i>p</i>"),
                                                         Qt::CaseInsensitive));
                    if (!isSig)
                        continue;
                    const QwtScaleMap xMap = pl->canvasMap(tm->xAxis());
                    const QwtScaleMap yMap = pl->canvasMap(tm->yAxis());
                    if (std::fabs(xMap.pDist()) < 10.0 || std::fabs(yMap.pDist()) < 10.0)
                        continue;
                    ArrowMarker *bestBrk = nullptr;
                    double bestDyPx = 1e300;
                    const double tmPy = yMap.xTransform(tm->yValue());
                    for (int lid : lineKeys) {
                        ArrowMarker *am = gr->arrow(lid);
                        if (!am || !am->isVisible())
                            continue;
                        const QwtDoublePoint sC = am->startPointCoord();
                        const QwtDoublePoint eC = am->endPointCoord();
                        if (std::fabs(sC.y() - eC.y())
                            > 1e-4 * qMax(1.0, std::fabs(sC.y()) + std::fabs(eC.y())))
                            continue;
                        const double xLo = qMin(sC.x(), eC.x());
                        const double xHi = qMax(sC.x(), eC.x());
                        const double span = qMax(1e-9, xHi - xLo);
                        if (tm->xValue() < xLo - 0.35 * span || tm->xValue() > xHi + 0.35 * span)
                            continue;
                        const double brkPy = yMap.xTransform(sC.y());
                        const double dyPx = std::fabs(brkPy - tmPy);
                        if (dyPx < 48.0 && dyPx < bestDyPx) {
                            bestDyPx = dyPx;
                            bestBrk = am;
                        }
                    }
                    if (bestBrk) {
                        QwtText qt(st);
                        qt.setFont(tm->font());
                        const QSize tsz = qt.textSize();
                        const int boxW = tsz.width() + 8;
                        const int boxH = tsz.height() + 4;
                        const QwtDoublePoint sC = bestBrk->startPointCoord();
                        const QwtDoublePoint eC = bestBrk->endPointCoord();
                        const double midPx =
                                0.5 * (xMap.xTransform(sC.x()) + xMap.xTransform(eC.x()));
                        const double brkPy = yMap.xTransform(sC.y());
                        const double targetTopPy = qMax(2.0, brkPy - double(boxH) - 2.0);
                        tm->setOriginCoord(xMap.invTransform(midPx - 0.5 * double(boxW)),
                                           yMap.invTransform(targetTopPy));
                    }
                }
            }
            clampGraphCalloutMarkersInCanvas(gr);
        }
        if (!isInset && !isLinkedOverlay && gr->isPiePlot()) {
            Plot *pl = gr->plotWidget();
            const QwtPlotCurve *pieCv = gr->curve(0);
            double sumY = 0.0;
            double maxPieY = 1.0;
            if (pieCv) {
                for (int k = 0; k < pieCv->dataSize(); ++k) {
                    const double yk = pieCv->y(k);
                    if (std::isfinite(yk) && yk > 0.0) {
                        sumY += yk;
                        if (yk > maxPieY)
                            maxPieY = yk;
                    }
                }
            }
            gr->setScale(QwtPlot::xBottom, -2.0 * maxPieY, 3.0 * maxPieY, 0.0, 5, 0, 0, false);
            gr->setScale(QwtPlot::yLeft, -1.2 * maxPieY, 3.8 * maxPieY, 0.0, 5, 0, 0, false);
            for (int a = 0; a < QwtPlot::axisCnt; ++a)
                gr->enableAxis(a, false);
            gr->drawAxesBackbones(false);
            gr->drawCanvasFrame(false, 0);
            pl->updateLayout();

            Legend *pieLeg = gr->legend();
            const bool hasVisPieLeg = pieLeg && gr->hasLegend() && gr->isLegendVisible();
            const bool legendShowsPct = hasVisPieLeg
                    && pieLeg->text().contains(QLatin1String("\\p{"))
                    && pieLeg->text().contains(QLatin1Char('%'));
            if (legendShowsPct) {
                static const QRegularExpression kPiePctMarkerRx(
                        QStringLiteral("^\\s*\\d+(?:\\.\\d+)?%\\s*$"));
                const QVector<int> tKeys = gr->textMarkerKeys();
                for (int tid : tKeys) {
                    Legend *tm = gr->textMarker(tid);
                    if (tm && tm != pieLeg && kPiePctMarkerRx.match(tm->text()).hasMatch()) {
                        tm->setText(QString());
                        tm->setVisible(false);
                    }
                }
            }
            if (hasVisPieLeg) {
                const QVariant ancProp = pl->property("_toriLegendAnchor");
                const QRect cr0 = pl->plotLayout()->canvasRect();
                QwtText qtLeg(pieLeg->text());
                qtLeg.setFont(pieLeg->font());
                const int estW = qMax(pieLeg->rect().width(), qtLeg.textSize().width() + 24);
                const bool useTopCenter =
                        (ancProp.isValid() && ancProp.toInt() == int(Graph::LegendTopCenter))
                        || (cr0.width() >= 40 && estW > int(0.34 * double(cr0.width())));
                gr->placeLegendAnchor(useTopCenter ? Graph::LegendTopCenter
                                                   : Graph::LegendTopRight);
                pl->updateLayout();
            }

            const QRect cr = pl->plotLayout()->canvasRect();
            if (pieCv && pieCv->dataSize() > 0 && sumY > 0.0 && cr.width() >= 40
                && cr.height() >= 40 && !legendShowsPct) {
                bool hasAnyPieCallout = false;
                const QVector<int> tKeys = gr->textMarkerKeys();
                for (int tid : tKeys) {
                    Legend *tm = gr->textMarker(tid);
                    if (tm && tm != pieLeg && tm->isVisible()
                        && tm->text().trimmed().endsWith(QLatin1Char('%'))) {
                        hasAnyPieCallout = true;
                        break;
                    }
                }
                if (hasAnyPieCallout) {
                    double pieR = 0.285 * qMin(cr.width(), cr.height());
                    double pCx = double(cr.center().x());
                    double pCy = double(cr.center().y());
                    if (hasVisPieLeg && pieLeg) {
                        const QRect lr = pieLeg->rect();
                        const QVariant ancProp = pl->property("_toriLegendAnchor");
                        const bool topBand =
                                (ancProp.isValid()
                                 && ancProp.toInt() == int(Graph::LegendTopCenter))
                                || (double(lr.width()) > 0.34 * double(cr.width()));
                        if (topBand) {
                            const double legBottomFrac = qBound(
                                    0.08, double(qMax(4, lr.top()) + lr.height()) / double(cr.height()),
                                    0.52);
                            const double legBottom = legBottomFrac * double(cr.height());
                            const double pad = 0.028 * double(cr.height());
                            const double availH = qMax(0.20 * double(cr.height()),
                                                       double(cr.height()) - legBottom - 2.0 * pad);
                            pieR = qMax(10.0, qMin(0.255 * double(cr.width()), 0.34 * availH));
                            pCx = double(cr.center().x());
                            pCy = double(cr.top()) + legBottom + pad + 0.5 * availH;
                        } else {
                            pieR = 0.24 * qMin(cr.width(), cr.height());
                            pCx = double(cr.left()) + 0.36 * double(cr.width());
                            pCy = double(cr.top()) + 0.52 * double(cr.height());
                        }
                    }
                    const double lblR = 1.22 * pieR;
                    constexpr double kPi = 3.14159265358979323846;
                    double ang = 90.0;
                    int sliceOrd = 0;
                    QVector<QRect> placedBoxes;
                    if (hasVisPieLeg && pieLeg && pieLeg->rect().isValid())
                        placedBoxes.append(pieLeg->rect());
                    for (int tid : tKeys) {
                        Legend *tm = gr->textMarker(tid);
                        if (!tm || tm == pieLeg || !tm->isVisible())
                            continue;
                        if (!tm->text().trimmed().endsWith(QLatin1Char('%')))
                            continue;
                        while (sliceOrd < pieCv->dataSize()
                               && !(std::isfinite(pieCv->y(sliceOrd))
                                    && pieCv->y(sliceOrd) > 0.0)) {
                            ++sliceOrd;
                        }
                        if (sliceOrd >= pieCv->dataSize())
                            break;
                        const double pctVal = pieCv->y(sliceOrd) / sumY * 100.0;
                        const double degSpan = pctVal * 3.6;
                        const double aRad = (ang - 0.5 * degSpan) * kPi / 180.0;
                        QwtText qt(tm->text().trimmed());
                        qt.setFont(tm->font());
                        const QSize tsz = qt.textSize();
                        const int bw = tsz.width() + 8;
                        const int bh = tsz.height() + 4;
                        const int ox = qBound(2, qRound(pCx + lblR * std::cos(aRad) - 0.5 * bw),
                                              qMax(2, cr.width() - bw - 2));
                        int oy = qBound(2, qRound(pCy - lblR * std::sin(aRad) - 0.5 * bh),
                                        qMax(2, cr.height() - bh - 2));
                        QRect candBox(ox, oy, bw, bh);
                        bool collides = false;
                        for (const QRect &prev : placedBoxes) {
                            if (candBox.intersects(prev)) {
                                collides = true;
                                break;
                            }
                        }
                        if (collides) {
                            const int dirY = (std::sin(aRad) >= 0.0) ? -1 : 1;
                            for (int step = 1; step <= 4 && collides; ++step) {
                                const int tryY = qBound(2, oy + dirY * step * (bh - 2),
                                                        qMax(2, cr.height() - bh - 2));
                                const QRect tryBox(ox, tryY, bw, bh);
                                bool hitAny = false;
                                for (const QRect &prev : placedBoxes) {
                                    if (tryBox.intersects(prev)) {
                                        hitAny = true;
                                        break;
                                    }
                                }
                                if (!hitAny) {
                                    oy = tryY;
                                    candBox = tryBox;
                                    collides = false;
                                }
                            }
                        }
                        if (collides && pctVal < 6.0) {
                            tm->setText(QString());
                            tm->setVisible(false);
                        } else {
                            tm->setOrigin(QPoint(ox, oy));
                            placedBoxes.append(candBox);
                        }
                        ang -= degSpan;
                        ++sliceOrd;
                    }
                }
            }
        }
        if (!isInset && !isLinkedOverlay && !gr->isPiePlot() && gr->hasLegend()
            && gr->isLegendVisible()) {
            Plot *plLeg = gr->plotWidget();
            plLeg->updateLayout();
            const QRect crLeg = plLeg->plotLayout()->canvasRect();
            Legend *legPtr = gr->legend();
            if (legPtr && !gr->isTernaryPlot() && legPtr->numColumns() <= 1
                && plLeg->property("_outsideRightMargin").toInt() <= 0 && crLeg.width() >= 60) {
                const int maxAllowedW = crLeg.width() - 10;
                QFont lf = legPtr->font();
                double curPt = lf.pointSizeF();
                if (curPt <= 0.0 && lf.pointSize() > 0)
                    curPt = double(lf.pointSize());
                const double floorPt =
                        (isSlidePreset || curPt > 14.0) ? 16.0 : 7.0;
                while (legPtr->rect().width() > maxAllowedW && curPt > floorPt + 0.1) {
                    curPt = qMax(floorPt, curPt - ((curPt > 14.0) ? 1.0 : 0.5));
                    lf.setPointSizeF(curPt);
                    lf.setHintingPreference(QFont::PreferNoHinting);
                    legPtr->setFont(lf);
                }
            }
            const QVariant ancProp = gr->plotWidget()->property("_toriLegendAnchor");
            if (ancProp.isValid()) {
                gr->placeLegendAnchor(static_cast<Graph::LegendAnchor>(ancProp.toInt()));
            } else if (gr->isTernaryPlot() || gr->isPolarPlot() || gr->isPoleFigurePlot()) {
                gr->placeLegendAnchor(Graph::LegendTopRight);
            } else if (legendWasDefaultTopLeft[i] || isSlidePreset || !gr->lineMarkerKeys().isEmpty()) {
                gr->placeLegendAnchor(Graph::LegendAutoAvoidData);
            }
            if (legPtr && !gr->isTernaryPlot()
                && plLeg->property("_outsideRightMargin").toInt() <= 0 && crLeg.width() >= 60) {
                const QRect finalLr = legPtr->rect();
                if (finalLr.right() > crLeg.width() - 4) {
                    legPtr->setOrigin(QPoint(qMax(2, crLeg.width() - finalLr.width() - 4),
                                             qMax(2, finalLr.top())));
                }
            }

            if (gr->isPolarPlot() && gr->legend() && gr->plotWidget()->canvas()) {
                Plot *pl = gr->plotWidget();
                pl->updateLayout();
                const QwtScaleMap yMap = pl->canvasMap(QwtPlot::yLeft);
                const double cy = yMap.xTransform(0.0);
                const double rMax = qMax(1e-6, gr->polarRadiusMax());
                const double ryOuter = std::fabs(yMap.xTransform(rMax) - cy);
                const double lblH = QFontMetricsF(gr->axisFont(QwtPlot::xBottom)).height();
                const double topOf90Px = cy - (ryOuter + 5.5 + 0.95 * lblH);
                const double legBottomPx = double(gr->legend()->rect().bottom() + 4);
                if (ryOuter > 8.0 && legBottomPx > topOf90Px) {
                    const double curYLo = -1.44 * rMax;
                    const double chPx = double(qMax(60, pl->canvas()->height()));
                    const double reqTopFrac =
                            qBound(0.18, (legBottomPx + 6.5 + lblH) / chPx, 0.58);
                    const double newYHi =
                            qMax(1.64 * rMax, (rMax - reqTopFrac * curYLo) / (1.0 - reqTopFrac));
                    const double halfX = 0.5 * (newYHi - curYLo);
                    gr->setScale(QwtPlot::xBottom, -halfX, halfX, 0.0, 5, 0, 0, false);
                    gr->setScale(QwtPlot::yLeft, curYLo, newYHi, 0.0, 5, 0, 0, false);
                    for (int a = 0; a < QwtPlot::axisCnt; ++a)
                        gr->enableAxis(a, false);
                    gr->placeLegendAnchor(Graph::LegendTopRight);
                }
            }
        }
        gr->plotWidget()->updateLayout();
        gr->plotWidget()->replot();
    }
    syncChildOverlayAndInsetGeometries();
    if (std::fabs(initialViewZoom - 1.0) > 1e-4)
        setPageZoom(initialViewZoom);
    emit modifiedPlot();
    pushUndoSnapshot(undoLabel, beforeSnap);
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
    Graph *g = active_graph ? active_graph : layer(1);
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
    const QString fullStatusText =
            QStringLiteral("%1 %2 %3 %4 %5 %6 %5 %7 DPI %5 %8 %5 %9%")
                    .arg(QString::number(wDisp, 'f', decimals), times,
                         QString::number(hDisp, 'f', decimals), unitStr, sep, aspectBadge,
                         QString::number(d_page_dpi), fontFam, QString::number(zoomPct));
    const QString compactStatusText =
            QStringLiteral("%1%2%3 %4 %5 %6 %5 %7 DPI %5 %8")
                    .arg(QString::number(wDisp, 'f', decimals), times,
                         QString::number(hDisp, 'f', decimals), unitStr, sep, aspectBadge,
                         QString::number(d_page_dpi), fontFam);
    const QString shortStatusText =
            QStringLiteral("%1%2%3%4 %5 %6 %5 %7 DPI %5 %8")
                    .arg(QString::number(wDisp, 'f', decimals), times,
                         QString::number(hDisp, 'f', decimals), unitStr, sep, ratioStr,
                         QString::number(d_page_dpi), fontFam);

    d_canvas_status_pill->setProperty("_fullStatusText", fullStatusText);
    d_canvas_status_pill->setProperty("_compactStatusText", compactStatusText);
    d_canvas_status_pill->setProperty("_shortStatusText", shortStatusText);
    d_canvas_status_pill->setText(fullStatusText);
    d_canvas_status_pill->setToolTip(
            tr("Page Size, Aspect Ratio, DPI, Font & Zoom — Click for Quick Presets"));
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
        const int leftReserved = (d_layer_pill_frame && d_layer_pill_frame->isVisible())
                ? (d_layer_pill_frame->geometry().right() + 10)
                : 8;
        const int availTopW = refSz.width() - leftReserved - 8 - sbRight;
        if (availTopW < fullW && !compactTxt.isEmpty()) {
            const int compactW = measurePillWidth(compactTxt);
            if (availTopW < compactW && !shortTxt.isEmpty()) {
                const int shortW = measurePillWidth(shortTxt);
                pw = qMin(shortW, qMax(60, availTopW));
            } else {
                pw = qMin(compactW, qMax(60, availTopW));
            }
            pillSz = d_canvas_status_pill->sizeHint().expandedTo(
                    d_canvas_status_pill->minimumSizeHint());
            ph = qMax(20, qMin(LayerButton::btnSize(), pillSz.height()));
        } else {
            pw = fullW;
            ph = qMax(20, qMin(LayerButton::btnSize(), ph));
        }
        if (availTopW < 95) {
            d_canvas_status_pill->hide();
            return;
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

QString MultiLayer::captureUndoSnapshot()
{
    if (d_is_restoring_snapshot)
        return QString();
    return saveToString(QString());
}

void MultiLayer::pushUndoSnapshot(const QString &label, const QString &beforeSnapshot, int mergeId)
{
    if (d_is_restoring_snapshot || beforeSnapshot.isEmpty() || !d_undo_stack)
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
        delete d_layers_selector;

    if (addTextOn) {
        QApplication::restoreOverrideCursor();
        if (canvas)
            canvas->releaseMouse();
        addTextOn = false;
        emit drawTextOff();
    }

    d_inset_specs.clear();
    d_pending_inset_specs.clear();

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
    const int gapX = (nCols > 1) ? qMax(6, colsSpace) : 0;
    const int gapY = (nRows > 1) ? qMax(6, rowsSpace) : 0;
    int panelTopExtra = 0;
    if (d_panel_labels_enabled && d_panel_label_placement == PanelOutsideTopLeft) {
        double maxLabelPt = (d_panel_label_font_pt > 0) ? double(d_panel_label_font_pt) : 12.0;
        if (d_panel_label_font_pt <= 0) {
            for (Graph *gr : primaryGraphs) {
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
        if (Plot *firstPlot = (!primaryGraphs.isEmpty() && primaryGraphs.first())
                    ? primaryGraphs.first()->plotWidget()
                    : nullptr) {
            lblFont = firstPlot->displayFont(lblFont);
        }
        const QFontMetrics lfm(lblFont);
        panelTopExtra = qMax(22, lfm.height() + 12);
    }

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
            if (!isMultiPanel && !xLabelsCrowded)
                continue;
            const int axesToCap[2] = { QwtPlot::xBottom, QwtPlot::yLeft };
            const int caps[2] = { xLabelsCrowded ? qMin(capX, safeXCaps) : capX, capY };
            bool changedAny = false;
            for (int ai = 0; ai < 2; ++ai) {
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
                    if (se && sd && !Plot::isLog10ScaleEngine(se)
                        && (sd->ticks(QwtScaleDiv::MajorTick).size() > cap + 1
                            || pl->axisMaxMajor(a) > cap || forceUnstepX)) {
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

void MultiLayer::openQuickLayerColumnsPopup()
{
    ApplicationWindow *app = findMultiLayerApp(this);
    Graph *g = activeGraph() ? activeGraph() : layer(1);
    if (!app || !g)
        return;

    QList<Table *> tables;
    QSet<Table *> plottedTables;
    for (int i = 0; i < g->curves(); ++i) {
        if (auto *dc = dynamic_cast<DataCurve *>(g->curve(i))) {
            if (dc->table() && !plottedTables.contains(dc->table())) {
                plottedTables.insert(dc->table());
                tables.append(dc->table());
            }
        }
    }
    for (MyWidget *w : app->windowsList()) {
        if (Table *t = qobject_cast<Table *>(w)) {
            if (!plottedTables.contains(t))
                tables.append(t);
        }
    }

    auto *popup = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose, true);
    const QColor surfaceBg(0xFF, 0xFF, 0xFF);
    const QColor borderStrong(0xC8, 0xD1, 0xDC);
    const QColor textPrimary(0x1E, 0x29, 0x3B);
    const QColor borderSubtle(0xE2, 0xE8, 0xF0);
    const QColor textSecondary(0x64, 0x74, 0x8B);
    popup->setStyleSheet(QStringLiteral(
            "QFrame { background-color: %1; border: 1px solid %2; border-radius: 8px; }"
            "QLabel { color: %3; border: none; font-weight: 600; font-size: 10.8pt; }"
            "QTreeWidget { background-color: %1; color: %3; border: 1px solid %4; "
            "border-radius: 5px; font-size: 10.8pt; }")
                                 .arg(surfaceBg.name(), borderStrong.name(),
                                      textPrimary.name(), borderSubtle.name()));

    auto *vbox = new QVBoxLayout(popup);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    auto *hdr = new QLabel(tr("Layer Columns Checklist"), popup);
    vbox->addWidget(hdr);

    auto *tree = new QTreeWidget(popup);
    tree->setHeaderHidden(true);
    tree->setRootIsDecorated(true);
    tree->setIndentation(14);
    tree->setMinimumSize(280, 220);
    tree->setMaximumHeight(320);

    auto roleBadge = [this](int desig) -> QString {
        switch (desig) {
        case SciDAVis::X:
            return QStringLiteral("[X]");
        case SciDAVis::Y:
            return QStringLiteral("[Y]");
        case SciDAVis::Z:
            return QStringLiteral("[Z]");
        case SciDAVis::xErr:
            return tr("[±xErr]");
        case SciDAVis::yErr:
            return tr("[±yErr]");
        default:
            return tr("[None]");
        }
    };

    QSet<QString> plottedFullNames;
    for (int i = 0; i < g->curves(); ++i) {
        if (auto *eb = dynamic_cast<QwtErrorPlotCurve *>(g->curve(i))) {
            if (eb->table())
                plottedFullNames.insert(eb->title().text());
        } else if (auto *dc = dynamic_cast<DataCurve *>(g->curve(i))) {
            plottedFullNames.insert(dc->title().text());
        }
    }

    for (Table *t : tables) {
        if (!t)
            continue;
        auto *tItem = new QTreeWidgetItem(tree);
        tItem->setText(0, t->name());
        tItem->setFlags(Qt::ItemIsEnabled);
        QFont bf = tItem->font(0);
        bf.setBold(true);
        tItem->setFont(0, bf);

        for (int col = 0; col < t->numCols(); ++col) {
            const int desig = t->colPlotDesignation(col);
            const QString colShort = t->colLabel(col);
            const QString fullCol = t->name() + QStringLiteral("_") + colShort;
            const QString longName = t->colLongName(col).trimmed();
            const QString units = t->colUnits(col).trimmed();
            const QString fullCmt = t->colComment(col).trimmed();
            const QString cmtFirst = fullCmt.section(QLatin1Char('\n'), 0, 0).trimmed();

            auto *cItem = new QTreeWidgetItem(tItem);
            QString disp = QStringLiteral("%1 %2").arg(roleBadge(desig), colShort);
            QString meta;
            if (!longName.isEmpty() && longName != colShort) {
                meta = longName;
                if (!units.isEmpty())
                    meta += QStringLiteral(" [%1]").arg(units);
                if (!cmtFirst.isEmpty() && cmtFirst != longName && cmtFirst != colShort)
                    meta += QStringLiteral(" (%1)").arg(cmtFirst.left(24));
            } else {
                if (!units.isEmpty())
                    meta = QStringLiteral("[%1]").arg(units);
                if (!cmtFirst.isEmpty() && cmtFirst != colShort) {
                    meta = meta.isEmpty() ? cmtFirst.left(28)
                                          : (meta + QStringLiteral(" ") + cmtFirst.left(24));
                }
            }
            if (!meta.isEmpty())
                disp += QStringLiteral(" — %1").arg(meta.left(44));
            cItem->setText(0, disp);

            QStringList tipLines;
            tipLines << fullCol;
            if (!longName.isEmpty())
                tipLines << tr("Long Name: %1").arg(longName);
            if (!units.isEmpty())
                tipLines << tr("Units: %1").arg(units);
            if (!fullCmt.isEmpty())
                tipLines << tr("Comments: %1").arg(fullCmt);
            cItem->setToolTip(0, tipLines.join(QLatin1Char('\n')));

            cItem->setData(0, Qt::UserRole, t->name());
            cItem->setData(0, Qt::UserRole + 1, col);
            cItem->setData(0, Qt::UserRole + 2, fullCol);

            if (desig == SciDAVis::X) {
                cItem->setFlags(Qt::ItemIsEnabled);
                cItem->setForeground(0, QBrush(textSecondary));
            } else {
                cItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
                cItem->setCheckState(0, plottedFullNames.contains(fullCol) ? Qt::Checked
                                                                           : Qt::Unchecked);
            }
        }
        tItem->setExpanded(plottedTables.contains(t) || tables.size() == 1);
    }

    QPointer<Graph> safeGraph(g);
    QPointer<ApplicationWindow> safeApp(app);
    connect(tree, &QTreeWidget::itemChanged, this,
            [this, safeGraph, safeApp](QTreeWidgetItem *item, int column) {
                if (column != 0 || !item || !safeGraph || !safeApp)
                    return;
                const QString tblName = item->data(0, Qt::UserRole).toString();
                const int colIdx = item->data(0, Qt::UserRole + 1).toInt();
                const QString fullCol = item->data(0, Qt::UserRole + 2).toString();
                if (tblName.isEmpty() || fullCol.isEmpty())
                    return;

                Table *tbl = safeApp->table(fullCol);
                if (!tbl) {
                    for (MyWidget *w : safeApp->windowsList()) {
                        if (Table *cand = qobject_cast<Table *>(w)) {
                            if (cand->name() == tblName) {
                                tbl = cand;
                                break;
                            }
                        }
                    }
                }
                if (!tbl || colIdx < 0 || colIdx >= tbl->numCols())
                    return;

                const QString beforeSnap = captureUndoSnapshot();
                const bool wantChecked = (item->checkState(0) == Qt::Checked);
                const int desig = tbl->colPlotDesignation(colIdx);

                if (wantChecked) {
                    if (desig == SciDAVis::yErr || desig == SciDAVis::xErr) {
                        QString targetMaster;
                        for (int k = safeGraph->curves() - 1; k >= 0; --k) {
                            if (auto *dc = dynamic_cast<DataCurve *>(safeGraph->curve(k))) {
                                if (!dynamic_cast<QwtErrorPlotCurve *>(dc) && dc->table() == tbl) {
                                    targetMaster = dc->title().text();
                                    break;
                                }
                            }
                        }
                        if (!targetMaster.isEmpty()) {
                            safeGraph->addErrorBars(
                                    targetMaster, tbl, fullCol,
                                    desig == SciDAVis::xErr ? QwtErrorPlotCurve::Horizontal
                                                            : QwtErrorPlotCurve::Vertical);
                        }
                    } else {
                        int xCol = tbl->firstXCol();
                        if (xCol < 0)
                            xCol = (colIdx == 0 && tbl->numCols() > 1) ? 1 : 0;
                        const QString xFull = tbl->name() + QStringLiteral("_") + tbl->colLabel(xCol);
                        const int style = safeApp->defaultCurveStyle;
                        if (safeGraph->insertCurve(tbl, xFull, fullCol, style)) {
                            const int newIdx = safeGraph->curves() - 1;
                            int dataCount = 0;
                            for (int ci = 0; ci < newIdx; ++ci) {
                                if (safeGraph->curveType(ci) != Graph::ErrorBars)
                                    ++dataCount;
                            }
                            CurveLayout cl = safeGraph->initCurveLayout(style, dataCount);
                            cl.sSize = safeApp->defaultSymbolSize;
                            cl.lWidth = safeApp->defaultCurveLineWidth;
                            safeGraph->updateCurveLayout(newIdx, &cl);
                            if (Plot *pl = safeGraph->plotWidget()) {
                                const int xAx = (!pl->axisEnabled(QwtPlot::xBottom)
                                                 && pl->axisEnabled(QwtPlot::xTop))
                                        ? QwtPlot::xTop
                                        : QwtPlot::xBottom;
                                const int yAx = (!pl->axisEnabled(QwtPlot::yLeft)
                                                 && pl->axisEnabled(QwtPlot::yRight))
                                        ? QwtPlot::yRight
                                        : QwtPlot::yLeft;
                                if (xAx != QwtPlot::xBottom || yAx != QwtPlot::yLeft)
                                    safeGraph->setCurveAxes(newIdx, xAx, yAx);
                            }
                        }
                    }
                } else {
                    safeApp->invalidateModelessGraphDialogs(this);
                    for (int k = safeGraph->curves() - 1; k >= 0; --k) {
                        if (QwtPlotCurve *cv = safeGraph->curve(k)) {
                            if (cv->title().text() == fullCol)
                                safeGraph->removeCurve(k);
                        }
                    }
                }

                safeGraph->updatePlot();
                safeGraph->refreshAutoAxisTitlesFromColumns();
                safeGraph->reconstructLegend();
                safeGraph->replot();
                safeGraph->notifyChanges();
                refreshLayerButtonBadges();
                pushUndoSnapshot(tr("Toggle Plotted Column"), beforeSnap);
                if (GraphInspectorDock *dock = safeApp->graphInspectorDock())
                    dock->setTargetMultiLayer(this, true);
            });

    vbox->addWidget(tree);
    popup->adjustSize();
    const QPoint anchorGlobal = d_quick_columns_btn
            ? d_quick_columns_btn->mapToGlobal(QPoint(0, d_quick_columns_btn->height() + 2))
            : QCursor::pos();
    popup->move(anchorGlobal);
    popup->show();
}

Graph *MultiLayer::resolveColumnDropTarget(const QPoint &canvasPt, const QPoint &widgetPos,
                                           bool &rightYZoneOut) const
{
    rightYZoneOut = false;

    auto isPointInRightYZone = [this, &canvasPt, &widgetPos](Graph *g) -> bool {
        if (!g)
            return false;
        const QRect gGeo = g->geometry();
        if (canvasPt.x() >= gGeo.right() - 48)
            return true;
        if (Plot *pl = g->plotWidget()) {
            if (QwtScaleWidget *swRight = pl->axisWidget(QwtPlot::yRight)) {
                if (swRight->isVisible()) {
                    const QPoint swPt = swRight->mapFromGlobal(mapToGlobal(widgetPos));
                    if (swRight->rect().adjusted(-14, 0, 14, 0).contains(swPt))
                        return true;
                }
            }
        }
        return false;
    };

    Graph *targetGr = nullptr;
    const QList<Graph *> ordered = orderedLayersForRender();
    for (int i = ordered.size() - 1; i >= 0; --i) {
        Graph *gr = ordered.at(i);
        if (!gr || !gr->isVisible())
            continue;
        if (!gr->geometry().contains(canvasPt))
            continue;

        if (Graph *parentLayer = gr->linkedParentLayer()) {
            Plot *cPlot = gr->plotWidget();
            const bool childHasRightY = cPlot && cPlot->axisEnabled(QwtPlot::yRight);
            const bool childHasLeftY = cPlot && cPlot->axisEnabled(QwtPlot::yLeft);
            const bool inRightZone = isPointInRightYZone(gr) || isPointInRightYZone(parentLayer);
            if (childHasRightY && (inRightZone || active_graph == gr)) {
                targetGr = gr;
                rightYZoneOut = inRightZone || !childHasLeftY;
                return targetGr;
            }
            targetGr = parentLayer;
            rightYZoneOut = inRightZone;
            return targetGr;
        }

        targetGr = gr;
        rightYZoneOut = isPointInRightYZone(gr);
        return targetGr;
    }

    targetGr = active_graph ? active_graph : const_cast<MultiLayer *>(this)->layer(1);
    if (targetGr) {
        Plot *pl = targetGr->plotWidget();
        const bool onlyRightY = pl && pl->axisEnabled(QwtPlot::yRight)
                && !pl->axisEnabled(QwtPlot::yLeft);
        rightYZoneOut = onlyRightY || isPointInRightYZone(targetGr);
    } else {
        const QRect fallbackRect = canvas ? canvas->rect() : rect();
        rightYZoneOut = (canvasPt.x() >= fallbackRect.right() - 44);
    }
    return targetGr;
}

void MultiLayer::dragEnterEvent(QDragEnterEvent *e)
{
    if (e && e->mimeData()
        && e->mimeData()->hasFormat(QLatin1String(MultiLayer::kColumnMimeType))) {
        e->acceptProposedAction();
        return;
    }
    MyWidget::dragEnterEvent(e);
}

void MultiLayer::dragMoveEvent(QDragMoveEvent *e)
{
    if (!e || !e->mimeData()
        || !e->mimeData()->hasFormat(QLatin1String(MultiLayer::kColumnMimeType))) {
        MyWidget::dragMoveEvent(e);
        return;
    }

    const QPoint canvasPt = canvas ? canvas->mapFrom(this, e->pos()) : e->pos();
    bool rightYZone = false;
    Graph *targetGr = resolveColumnDropTarget(canvasPt, e->pos(), rightYZone);
    const QRect zoneRect = targetGr ? targetGr->geometry()
                                    : (canvas ? canvas->rect() : rect());

    if (d_drop_zone_overlay && canvas) {
        d_drop_zone_overlay->setGeometry(canvas->geometry());
        d_drop_zone_overlay->setProperty("rightYZone", rightYZone);
        d_drop_zone_overlay->setProperty("targetRect", zoneRect);
        d_drop_zone_overlay->setProperty(
                "bannerText",
                rightYZone ? tr("Drop to add curve(s) on Right Y")
                           : tr("Drop to add curve(s) on Left Y"));
        if (!d_drop_zone_overlay->isVisible())
            d_drop_zone_overlay->show();
        d_drop_zone_overlay->raise();
        d_drop_zone_overlay->update();
    }

    e->acceptProposedAction();
}

void MultiLayer::dragLeaveEvent(QDragLeaveEvent *e)
{
    if (d_drop_zone_overlay)
        d_drop_zone_overlay->hide();
    MyWidget::dragLeaveEvent(e);
}

void MultiLayer::dropEvent(QDropEvent *e)
{
    if (d_drop_zone_overlay)
        d_drop_zone_overlay->hide();

    if (!e || !e->mimeData()
        || !e->mimeData()->hasFormat(QLatin1String(MultiLayer::kColumnMimeType))) {
        MyWidget::dropEvent(e);
        return;
    }

    const QPoint canvasPt = canvas ? canvas->mapFrom(this, e->pos()) : e->pos();
    bool rightYZone = false;
    Graph *targetGr = resolveColumnDropTarget(canvasPt, e->pos(), rightYZone);

    const int targetAxisId = rightYZone ? QwtPlot::yRight : QwtPlot::yLeft;
    const bool replaceExisting = (e->keyboardModifiers() & Qt::AltModifier);
    if (ApplicationWindow *app = findMultiLayerApp(this)) {
        if (app->handleColumnMimeDrop(e->mimeData(), this, targetGr, targetAxisId, replaceExisting)) {
            refreshLayerButtonBadges();
            e->acceptProposedAction();
        }
    }
}

// ============================================================================
// User-Customizable Graph Style Presets (JSON Persistence + Manager Dialog)
// ============================================================================

namespace {

void registerStylePresetTranslationsOnce()
{
    static bool s_done = false;
    if (s_done)
        return;
    s_done = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(
                QString::fromUtf8(en), QString::fromUtf8(ja));
    };
    reg("Manage Style Presets...", "グラフプリセットの編集・追加...");
    reg("Manage Presets...", "プリセット管理...");
    reg("Presets...", "プリセット...");
    reg("Graph Style Preset Manager", "グラフスタイルプリセット管理");
    reg("Apply Preset: %1", "プリセットを適用: %1");
    reg("Built-in", "組み込み");
    reg("Custom", "ユーザー");
    reg("New", "新規追加");
    reg("From Current Graph", "現在のグラフから作成");
    reg("Duplicate", "複製");
    reg("Delete", "削除");
    reg("Reset Default", "初期値に戻す");
    reg("Import JSON...", "JSON 読込...");
    reg("Export JSON...", "JSON 書出...");
    reg("Preset Name:", "プリセット名:");
    reg("Page Width (mm):", "ページ幅 (mm):");
    reg("Page Height (mm):", "ページ高さ (mm):");
    reg("Lock Aspect Ratio", "縦横比を固定");
    reg("Target DPI:", "出力解像度 (DPI):");
    reg("Margin (px):", "余白 (px):");
    reg("Frame & Ticks:", "枠と目盛り:");
    reg("Box + Inward Ticks (ISIJ)", "四辺枠 + 内向き目盛り (ISIJ)");
    reg("Box + Plain Opposite Axes", "四辺枠 (上・右目盛りなし)");
    reg("Open L + Outward Ticks (Nature)", "L字枠 + 外向き目盛り (Nature)");
    reg("Zero-Crossed Axes", "原点交差軸 (0基準)");
    reg("Axis Line Width (px):", "軸線の太さ (px):");
    reg("Minor Tick Length (px):", "副目盛りの長さ (px):");
    reg("Major Tick Length (px):", "主目盛りの長さ (px):");
    reg("Font Family:", "フォント:");
    reg("Axis Title Size (pt):", "軸タイトルサイズ (pt):");
    reg("Tick Label Size (pt):", "目盛り数字サイズ (pt):");
    reg("Legend Size (pt):", "凡例フォントサイズ (pt):");
    reg("Bold", "太字");
    reg("Min Curve Width (px):", "最小曲線幅 (px):");
    reg("Min Symbol Size (px):", "最小シンボルサイズ (px):");
    reg("Legend Frame:", "凡例枠:");
    reg("None (Transparent)", "なし (透過)");
    reg("Line Box", "実線枠");
    reg("Shadow Box", "影付き枠");
    reg("Format axis units as ISIJ slash (Quantity, sym / unit)",
        "軸単位を ISIJ スラッシュ形式 (量, 記号 / 単位) に整形");
    reg("Use large presentation-slide layout rules",
        "スライド向け大フォント・余白レイアウトを使用");
    reg("Save Changes", "変更を保存");
    reg("Save && Apply to Graph", "保存してグラフに適用");
    reg("Close", "閉じる");
    reg("Saved presets to %1", "プリセットを %1 に保存しました");
    reg("Imported %1 preset(s).", "%1 件のプリセットを読み込みました。");
    reg("New Custom Preset", "カスタムプリセット");
    reg("Copy of %1", "%1 のコピー");
    reg("Cannot delete a built-in preset. Use 'Reset Default' to restore its original settings.",
        "組み込みプリセットは削除できません。元に戻すには「初期値に戻す」を使用してください。");
    reg("Custom Preset:", "プリセット:");
    reg("Apply Selected Preset", "選択したプリセットを適用");
    reg("Curve Width (px):", "曲線の太さ (px):");
    reg("Symbol Size (px):", "シンボルサイズ (px):");
    reg("Defaults for New Graphs", "新規グラフの既定");
    reg("Preset applied to new graphs:", "新規グラフに適用するプリセット:");
    reg("Colors for new curves:", "新しい曲線の色:");
    reg("Origin Classic (black, red, blue, ...)", "Origin クラシック (黒・赤・青…)");
    Graph::registerPaletteTranslations();
    reg("Okabe-Ito (Colorblind Safe)", "岡部・伊藤 (色覚バリアフリー)");
    reg("Matplotlib (tab10)", "Matplotlib 既定色 (tab10)");
    reg("Multi-panel page size:", "複数パネルのページ寸法:");
    reg("Preset size per panel (columns x width)", "パネルごとにプリセット寸法 (列数 × 幅)");
    reg("Journal double column (175 mm wide)", "論文 2 段幅 (幅 175 mm)");
    reg("Shared preset folder (read-only):", "共有プリセットフォルダ (読み取り専用):");
    reg("(none)", "(なし)");
    reg("Browse...", "参照...");
    reg("None (keep plain pixel page)", "なし (ピクセル単位のページのまま)");
    reg("Shared", "共有");
    reg("Apply to Graph", "グラフに適用");
    reg("This preset comes from the shared folder and is read-only. Duplicate it to make a personal copy.",
        "このプリセットは共有フォルダのもので読み取り専用です。複製すると自分用のコピーを作れます。");
    reg("%1 was written by a newer ToriGraph (preset format %2; this version reads format %3). Unknown settings are ignored.",
        "%1 は新しい版の ToriGraph で書かれています (プリセット形式 %2、この版が読めるのは形式 %3 まで)。分からない設定は無視しました。");
    reg("Axis title \"%1\" is wider than the axis at %2 pt (size kept; shorten the title or enlarge the page).",
        "軸タイトル「%1」は %2 pt では軸より長くなります (文字サイズは変えていません。タイトルを短くするか、ページを大きくしてください)。");
    reg("Lab Slide (18 pt on slide, 83.3 mm, paste at 140%)",
        "研究室スライド (スライド上で 18 pt、83.3 mm 角、PowerPoint で 140% に拡大)");
    reg("Journal 2-Column (175x85 mm, 600 DPI)", "論文 2 段幅 (175×85 mm, 600 DPI)");
    reg("ISIJ 1-Column (85x85 mm, 600 DPI)", "ISIJ 1段幅 (85×85 mm, 600 DPI)");
    reg("Presentation Slide (28 pt Arial)", "発表スライド (28 pt Arial)");
    reg("Large Slide (28 pt, 185 mm)", "大判スライド (28 pt, 185 mm)");
    reg("PowerPoint Paste Scale (%):", "PowerPoint 貼付倍率 (%):");
    reg("OK", "OK");
    reg("Cancel", "キャンセル");
}

QJsonObject stylePresetToJson(const MultiLayer::GraphStylePresetSpec &s)
{
    QJsonObject o;
    o[QStringLiteral("id")] = s.id;
    o[QStringLiteral("name")] = s.name;
    o[QStringLiteral("built_in")] = s.builtIn;
    o[QStringLiteral("page_width_mm")] = s.pageWidthMm;
    o[QStringLiteral("page_height_mm")] = s.pageHeightMm;
    o[QStringLiteral("keep_aspect_ratio")] = s.keepAspectRatio;
    o[QStringLiteral("target_dpi")] = s.targetDpi;
    o[QStringLiteral("margin_px")] = s.marginPx;
    o[QStringLiteral("frame_preset")] = s.framePreset;
    o[QStringLiteral("axis_line_width")] = s.axisLineWidth;
    o[QStringLiteral("minor_tick_len")] = s.minorTickLen;
    o[QStringLiteral("major_tick_len")] = s.majorTickLen;
    o[QStringLiteral("font_family")] = s.fontFamily;
    o[QStringLiteral("axis_title_pt")] = s.axisTitlePt;
    o[QStringLiteral("tick_label_pt")] = s.tickLabelPt;
    o[QStringLiteral("legend_pt")] = s.legendPt;
    o[QStringLiteral("bold_title")] = s.boldTitle;
    o[QStringLiteral("bold_tick")] = s.boldTick;
    o[QStringLiteral("bold_legend")] = s.boldLegend;
    const double effCw = (s.curveWidthF > 0.0) ? s.curveWidthF : double(s.minCurveWidth);
    o[QStringLiteral("curve_width")] = effCw;
    o[QStringLiteral("symbol_size")] = s.minSymbolSize;
    // legacy keys (read by older ToriGraph builds)
    o[QStringLiteral("min_curve_width")] = qBound(1, qRound(effCw), 20);
    o[QStringLiteral("min_symbol_size")] = s.minSymbolSize;
    o[QStringLiteral("legend_frame")] = s.legendFrame;
    o[QStringLiteral("page_background")] =
            (s.pageBackground.isValid() ? s.pageBackground : QColor(Qt::white)).name();
    o[QStringLiteral("format_isij_unit_slash")] = s.formatIsijUnitSlash;
    o[QStringLiteral("large_slide_layout")] = s.largeSlideLayout;
    if (s.slidePastePercent > 0.0)
        o[QStringLiteral("slide_paste_percent")] = s.slidePastePercent;
    return o;
}

MultiLayer::GraphStylePresetSpec stylePresetFromJson(const QJsonObject &o,
                                                     const MultiLayer::GraphStylePresetSpec &fallback)
{
    MultiLayer::GraphStylePresetSpec s = fallback;
    if (o.contains(QStringLiteral("id")))
        s.id = o.value(QStringLiteral("id")).toString(s.id).trimmed();
    if (o.contains(QStringLiteral("name")))
        s.name = o.value(QStringLiteral("name")).toString(s.name).trimmed();
    if (o.contains(QStringLiteral("page_width_mm")))
        s.pageWidthMm = qBound(20.0, o.value(QStringLiteral("page_width_mm")).toDouble(s.pageWidthMm), 1000.0);
    if (o.contains(QStringLiteral("page_height_mm")))
        s.pageHeightMm = qBound(20.0, o.value(QStringLiteral("page_height_mm")).toDouble(s.pageHeightMm), 1000.0);
    if (o.contains(QStringLiteral("keep_aspect_ratio")))
        s.keepAspectRatio = o.value(QStringLiteral("keep_aspect_ratio")).toBool(s.keepAspectRatio);
    if (o.contains(QStringLiteral("target_dpi")))
        s.targetDpi = qBound(72, o.value(QStringLiteral("target_dpi")).toInt(s.targetDpi), 2400);
    if (o.contains(QStringLiteral("margin_px")))
        s.marginPx = qBound(0, o.value(QStringLiteral("margin_px")).toInt(s.marginPx), 80);
    if (o.contains(QStringLiteral("frame_preset")))
        s.framePreset = qBound(0, o.value(QStringLiteral("frame_preset")).toInt(s.framePreset), 3);
    if (o.contains(QStringLiteral("axis_line_width")))
        s.axisLineWidth = qBound(1, o.value(QStringLiteral("axis_line_width")).toInt(s.axisLineWidth), 10);
    if (o.contains(QStringLiteral("minor_tick_len")))
        s.minorTickLen = qBound(0, o.value(QStringLiteral("minor_tick_len")).toInt(s.minorTickLen), 20);
    if (o.contains(QStringLiteral("major_tick_len")))
        s.majorTickLen = qBound(0, o.value(QStringLiteral("major_tick_len")).toInt(s.majorTickLen), 30);
    if (o.contains(QStringLiteral("font_family"))) {
        const QString ff = o.value(QStringLiteral("font_family")).toString().trimmed();
        if (!ff.isEmpty())
            s.fontFamily = ff;
    }
    if (o.contains(QStringLiteral("axis_title_pt")))
        s.axisTitlePt = qBound(5.0, o.value(QStringLiteral("axis_title_pt")).toDouble(s.axisTitlePt), 72.0);
    if (o.contains(QStringLiteral("tick_label_pt")))
        s.tickLabelPt = qBound(5.0, o.value(QStringLiteral("tick_label_pt")).toDouble(s.tickLabelPt), 72.0);
    if (o.contains(QStringLiteral("legend_pt")))
        s.legendPt = qBound(5.0, o.value(QStringLiteral("legend_pt")).toDouble(s.legendPt), 72.0);
    if (o.contains(QStringLiteral("bold_title")))
        s.boldTitle = o.value(QStringLiteral("bold_title")).toBool(s.boldTitle);
    if (o.contains(QStringLiteral("bold_tick")))
        s.boldTick = o.value(QStringLiteral("bold_tick")).toBool(s.boldTick);
    if (o.contains(QStringLiteral("bold_legend")))
        s.boldLegend = o.value(QStringLiteral("bold_legend")).toBool(s.boldLegend);
    if (o.contains(QStringLiteral("curve_width"))) {
        const double defCw = (s.curveWidthF > 0.0) ? s.curveWidthF : double(s.minCurveWidth);
        const double cw = qBound(0.5, o.value(QStringLiteral("curve_width")).toDouble(defCw), 20.0);
        s.curveWidthF = cw;
        s.minCurveWidth = qBound(1, qRound(cw), 20);
    } else if (o.contains(QStringLiteral("min_curve_width"))) {
        s.minCurveWidth = qBound(1, o.value(QStringLiteral("min_curve_width")).toInt(s.minCurveWidth), 20);
        s.curveWidthF = double(s.minCurveWidth);
    }
    if (o.contains(QStringLiteral("symbol_size")))
        s.minSymbolSize = qBound(0, o.value(QStringLiteral("symbol_size")).toInt(s.minSymbolSize), 100);
    else if (o.contains(QStringLiteral("min_symbol_size")))
        s.minSymbolSize = qBound(0, o.value(QStringLiteral("min_symbol_size")).toInt(s.minSymbolSize), 100);
    if (o.contains(QStringLiteral("legend_frame")))
        s.legendFrame = qBound(0, o.value(QStringLiteral("legend_frame")).toInt(s.legendFrame), 2);
    if (o.contains(QStringLiteral("page_background"))) {
        QColor c(o.value(QStringLiteral("page_background")).toString());
        if (c.isValid())
            s.pageBackground = c;
    }
    if (o.contains(QStringLiteral("format_isij_unit_slash")))
        s.formatIsijUnitSlash = o.value(QStringLiteral("format_isij_unit_slash")).toBool(s.formatIsijUnitSlash);
    if (o.contains(QStringLiteral("large_slide_layout")))
        s.largeSlideLayout = o.value(QStringLiteral("large_slide_layout")).toBool(s.largeSlideLayout);
    if (o.contains(QStringLiteral("slide_paste_percent")))
        s.slidePastePercent = qBound(10.0, o.value(QStringLiteral("slide_paste_percent")).toDouble(s.slidePastePercent), 400.0);
    return s;
}

bool writeUserStylePresetsArray(const QVector<MultiLayer::GraphStylePresetSpec> &userEntries,
                                QString *errOut)
{
    const QString path = MultiLayer::userStylePresetsFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonArray arr;
    for (const auto &s : userEntries)
        arr.append(stylePresetToJson(s));
    QJsonObject root;
    root[QStringLiteral("version")] = MultiLayer::stylePresetFormatVersion();
    root[QStringLiteral("presets")] = arr;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errOut)
            *errOut = QObject::tr("Could not write preset file: %1").arg(path);
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

//! Reads the "version" field of a preset document. Files without one are version 1.
int presetDocumentVersion(const QJsonDocument &doc)
{
    if (!doc.isObject())
        return 1;
    const QJsonValue v = doc.object().value(QStringLiteral("version"));
    return v.isDouble() ? qMax(1, v.toInt(1)) : 1;
}

QVector<MultiLayer::GraphStylePresetSpec> readPresetArrayFromFile(const QString &path,
                                                                  bool readOnly,
                                                                  QString *warnOut = nullptr)
{
    QVector<MultiLayer::GraphStylePresetSpec> out;
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return out;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QJsonArray arr;
    if (doc.isObject() && doc.object().contains(QStringLiteral("presets")))
        arr = doc.object().value(QStringLiteral("presets")).toArray();
    else if (doc.isArray())
        arr = doc.array();
    else if (doc.isObject())
        arr.append(doc.object());
    const int ver = presetDocumentVersion(doc);
    if (ver > MultiLayer::stylePresetFormatVersion() && warnOut) {
        *warnOut = QObject::tr("%1 was written by a newer ToriGraph (preset format %2; this "
                               "version reads format %3). Unknown settings are ignored.")
                           .arg(QFileInfo(path).fileName())
                           .arg(ver)
                           .arg(MultiLayer::stylePresetFormatVersion());
    }
    for (const QJsonValue &v : arr) {
        if (!v.isObject())
            continue;
        MultiLayer::GraphStylePresetSpec base;
        base.builtIn = false;
        MultiLayer::GraphStylePresetSpec parsed = stylePresetFromJson(v.toObject(), base);
        if (parsed.id.isEmpty())
            continue;
        parsed.readOnly = readOnly;
        parsed.sourcePath = readOnly ? path : QString();
        out.append(parsed);
    }
    return out;
}

QVector<MultiLayer::GraphStylePresetSpec> readSharedPresetEntries()
{
    QVector<MultiLayer::GraphStylePresetSpec> out;
    const QString folder = MultiLayer::sharedStylePresetsFolder();
    if (folder.trimmed().isEmpty())
        return out;
    QDir dir(folder);
    if (!dir.exists())
        return out;
    const QStringList files =
            dir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files, QDir::Name);
    for (const QString &fn : files)
        out += readPresetArrayFromFile(dir.filePath(fn), true);
    return out;
}

QVector<MultiLayer::GraphStylePresetSpec> readRawUserPresetEntries()
{
    QVector<MultiLayer::GraphStylePresetSpec> out;
    const QString path = MultiLayer::userStylePresetsFilePath();
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return out;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return out;
    const QJsonArray arr = doc.object().value(QStringLiteral("presets")).toArray();
    for (const QJsonValue &v : arr) {
        if (!v.isObject())
            continue;
        MultiLayer::GraphStylePresetSpec base;
        base.builtIn = false;
        MultiLayer::GraphStylePresetSpec parsed = stylePresetFromJson(v.toObject(), base);
        if (!parsed.id.isEmpty())
            out.append(parsed);
    }
    return out;
}

QString canonicalBuiltInAlias(const QString &raw)
{
    const QString k = raw.trimmed().toLower();
    if (k == QLatin1String("isij") || k == QLatin1String("isij_85mm")
        || k == QLatin1String("isij_journal") || k == QLatin1String("isij85"))
        return QStringLiteral("isij_85mm");
    if (k == QLatin1String("slide") || k == QLatin1String("slide_28pt")
        || k == QLatin1String("large_slide")
        || k == QLatin1String("presentation") || k == QLatin1String("presentation_slide"))
        return QStringLiteral("slide_28pt");
    if (k == QLatin1String("lab_slide") || k == QLatin1String("lab_slide_83mm")
        || k == QLatin1String("slide_140"))
        return QStringLiteral("lab_slide_83mm");
    if (k == QLatin1String("2col") || k == QLatin1String("two_column")
        || k == QLatin1String("journal_2col_175mm") || k == QLatin1String("175mm"))
        return QStringLiteral("journal_2col_175mm");
    return raw.trimmed();
}

} // namespace

QString MultiLayer::newGraphStylePresetId()
{
    QSettings &s = ApplicationWindow::getSettings();
    const QVariant v = s.value(QStringLiteral("/StylePresets/NewGraphPreset"));
    if (!v.isValid())
        return QStringLiteral("isij_85mm");
    const QString id = v.toString().trimmed();
    if (id.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0)
        return QString();
    return id;
}

void MultiLayer::setNewGraphStylePresetId(const QString &id)
{
    QSettings &s = ApplicationWindow::getSettings();
    const QString v = id.trimmed().isEmpty() ? QStringLiteral("none") : id.trimmed();
    s.setValue(QStringLiteral("/StylePresets/NewGraphPreset"), v);
}

QString MultiLayer::sharedStylePresetsFolder()
{
    QSettings &s = ApplicationWindow::getSettings();
    return s.value(QStringLiteral("/StylePresets/SharedFolder"), QString()).toString().trimmed();
}

void MultiLayer::setSharedStylePresetsFolder(const QString &folder)
{
    QSettings &s = ApplicationWindow::getSettings();
    s.setValue(QStringLiteral("/StylePresets/SharedFolder"), folder.trimmed());
}

MultiLayer::MultiPanelSizing MultiLayer::multiPanelSizing()
{
    QSettings &s = ApplicationWindow::getSettings();
    const int v = s.value(QStringLiteral("/StylePresets/MultiPanelSizing"),
                          int(PanelSizingPerPanel))
                          .toInt();
    return (v == int(PanelSizingTwoColumn175)) ? PanelSizingTwoColumn175 : PanelSizingPerPanel;
}

void MultiLayer::setMultiPanelSizing(MultiPanelSizing mode)
{
    QSettings &s = ApplicationWindow::getSettings();
    s.setValue(QStringLiteral("/StylePresets/MultiPanelSizing"), int(mode));
}

void MultiLayer::requestNewGraphStylePreset(Graph *g)
{
    if (!g)
        return;
    MultiLayer *ml = g->multiLayerParent();
    if (!ml || ml->property("_toriNewGraphPresetDone").toBool()
        || ml->property("_toriNewGraphPresetPending").toBool())
        return;
    if (newGraphStylePresetId().isEmpty())
        return;
    // The preset is a convenience for graphs made by hand. Graphs made by scripts, the Python
    // SDK (RPC), headless/batch runs or OLE automation keep the plain defaults, so their output
    // does not depend on a GUI setting; SDK users pick a preset with apply_style_preset().
    if (ApplicationWindow::isNonInteractiveContext(ml))
        return;
    ml->setProperty("_toriNewGraphPresetPending", true);
    // Applied after the creating function has finished (legend, layout and window geometry are
    // in place by then), exactly as if the user had chosen the preset.
    QPointer<MultiLayer> guard(ml);
    QTimer::singleShot(0, ml, [guard]() {
        if (guard)
            guard->applyPendingNewGraphStylePreset();
    });
}

bool MultiLayer::applyPendingNewGraphStylePreset()
{
    if (!property("_toriNewGraphPresetPending").toBool())
        return false;
    setProperty("_toriNewGraphPresetPending", false);
    setProperty("_toriNewGraphPresetDone", true);
    const QString id = newGraphStylePresetId();
    if (id.isEmpty())
        return false;
    bool anyCartesian = false;
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (gr && !gr->isPiePlot() && !gr->isTernaryPlot() && !gr->isPolarPlot()
            && !gr->isPoleFigurePlot())
            anyCartesian = true;
    }
    if (!anyCartesian)
        return false;
    GraphStylePresetSpec spec;
    if (!findStylePreset(id, &spec))
        return false;
    const int undoBefore = d_undo_stack ? d_undo_stack->count() : -1;
    // A window size/position remembered from the last session (restored just before this
    // runs) wins over the preset's automatic window fit; the preset only sets the page.
    const bool keepWindowGeometry =
            ApplicationWindow::getSettings()
                    .value(QStringLiteral("/2DPlots/LastGraphWindow/HasGeometry"), false)
                    .toBool()
            && !isMaximized();
    const QRect geoBefore = geometry();
    applyStylePresetSpec(spec);
    if (keepWindowGeometry && geoBefore.width() >= 100 && geoBefore.height() >= 100
        && geometry() != geoBefore)
        setGeometry(geoBefore);
    // Part of creating the graph, not an edit the user should undo step by step.
    if (d_undo_stack && undoBefore == 0)
        d_undo_stack->clear();
    return true;
}

QString MultiLayer::userStylePresetsFilePath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.torigraph");
    return QDir(base).filePath(QStringLiteral("graph_style_presets.json"));
}

QVector<MultiLayer::GraphStylePresetSpec> MultiLayer::defaultBuiltInStylePresets()
{
    QVector<GraphStylePresetSpec> defs;
    defs.reserve(4);

    GraphStylePresetSpec isij;
    isij.id = QStringLiteral("isij_85mm");
    isij.name = QStringLiteral("ISIJ 1-Column (85x85 mm, 600 DPI)");
    isij.builtIn = true;
    isij.pageWidthMm = 85.0;
    isij.pageHeightMm = 85.0;
    isij.keepAspectRatio = true;
    isij.targetDpi = 600;
    isij.marginPx = 6;
    isij.framePreset = static_cast<int>(Graph::FrameBoxInwardTicks);
    isij.axisLineWidth = 2;
    isij.minorTickLen = 4;
    isij.majorTickLen = 8;
    isij.fontFamily = QStringLiteral("Arial");
    isij.axisTitlePt = 11.0;
    isij.tickLabelPt = 10.0;
    isij.legendPt = 9.0;
    isij.minCurveWidth = 2;
    isij.minSymbolSize = 7;
    isij.legendFrame = Legend::None;
    isij.pageBackground = Qt::white;
    isij.formatIsijUnitSlash = true;
    isij.largeSlideLayout = false;
    defs.append(isij);

    GraphStylePresetSpec slide;
    slide.id = QStringLiteral("slide_28pt");
    slide.name = QStringLiteral("Large Slide (28 pt, 185 mm)");
    slide.builtIn = true;
    slide.pageWidthMm = 185.0;
    slide.pageHeightMm = 185.0;
    slide.keepAspectRatio = true;
    slide.targetDpi = 300;
    slide.marginPx = 10;
    slide.framePreset = static_cast<int>(Graph::FrameBoxInwardTicks);
    slide.axisLineWidth = 3;
    slide.minorTickLen = 6;
    slide.majorTickLen = 12;
    slide.fontFamily = QStringLiteral("Arial");
    slide.axisTitlePt = 28.0;
    slide.tickLabelPt = 28.0;
    slide.legendPt = 24.0;
    slide.minCurveWidth = 3;
    slide.minSymbolSize = 12;
    slide.legendFrame = Legend::None;
    slide.pageBackground = Qt::white;
    slide.formatIsijUnitSlash = false;
    slide.largeSlideLayout = true;
    defs.append(slide);

    // E3: lab slide preset. The lab pastes slide graphs as an 83.3 mm square page (the size of
    // an 8.2 in Origin page copied at 40 %) and enlarges it to 140 % in PowerPoint (11.7 cm
    // square). ToriGraph pastes at the physical page size, so the text is sized for how it should
    // read on the slide after that enlargement: axis titles and tick labels 18 pt / 1.40 = 12.9 pt
    // (18 pt on the slide), legend 11.5 pt (16 pt on the slide; same 12.9 / 11.2 ratio over the
    // former 10 pt). The former 11.2 pt (= Origin 28 pt x 0.40) only read as 15.7 pt on a slide.
    GraphStylePresetSpec lab;
    lab.id = QStringLiteral("lab_slide_83mm");
    lab.name = QStringLiteral("Lab Slide (18 pt on slide, 83.3 mm, paste at 140%)");
    lab.builtIn = true;
    lab.pageWidthMm = 83.3;
    lab.pageHeightMm = 83.3;
    lab.keepAspectRatio = true;
    lab.targetDpi = 600;
    lab.marginPx = 6;
    lab.framePreset = static_cast<int>(Graph::FrameBoxInwardTicks);
    lab.axisLineWidth = 2;
    lab.minorTickLen = 4;
    lab.majorTickLen = 8;
    lab.fontFamily = QStringLiteral("Arial");
    lab.axisTitlePt = 12.9;
    lab.tickLabelPt = 12.9;
    lab.legendPt = 11.5;
    lab.minCurveWidth = 2;
    lab.minSymbolSize = 7;
    lab.legendFrame = Legend::None;
    lab.pageBackground = Qt::white;
    lab.formatIsijUnitSlash = false;
    lab.largeSlideLayout = false;
    lab.slidePastePercent = 140.0; // E8: "Send to PowerPoint" pastes it at this size
    defs.append(lab);

    // E3: journal double-column width (175 mm).
    GraphStylePresetSpec twoCol;
    twoCol.id = QStringLiteral("journal_2col_175mm");
    twoCol.name = QStringLiteral("Journal 2-Column (175x85 mm, 600 DPI)");
    twoCol.builtIn = true;
    twoCol.pageWidthMm = 175.0;
    twoCol.pageHeightMm = 85.0;
    twoCol.keepAspectRatio = true;
    twoCol.targetDpi = 600;
    twoCol.marginPx = 6;
    twoCol.framePreset = static_cast<int>(Graph::FrameBoxInwardTicks);
    twoCol.axisLineWidth = 2;
    twoCol.minorTickLen = 4;
    twoCol.majorTickLen = 8;
    twoCol.fontFamily = QStringLiteral("Arial");
    twoCol.axisTitlePt = 11.0;
    twoCol.tickLabelPt = 10.0;
    twoCol.legendPt = 9.0;
    twoCol.minCurveWidth = 2;
    twoCol.minSymbolSize = 7;
    twoCol.legendFrame = Legend::None;
    twoCol.pageBackground = Qt::white;
    twoCol.formatIsijUnitSlash = true;
    twoCol.largeSlideLayout = false;
    defs.append(twoCol);

    return defs;
}

QVector<MultiLayer::GraphStylePresetSpec> MultiLayer::listStylePresets()
{
    registerStylePresetTranslationsOnce();
    QVector<GraphStylePresetSpec> result = defaultBuiltInStylePresets();
    const QVector<GraphStylePresetSpec> userEntries = readRawUserPresetEntries();

    for (const GraphStylePresetSpec &ue : userEntries) {
        bool merged = false;
        for (int i = 0; i < result.size(); ++i) {
            if (result[i].id.compare(ue.id, Qt::CaseInsensitive) == 0) {
                GraphStylePresetSpec ov = ue;
                ov.id = result[i].id;
                ov.builtIn = true;
                if (ov.name.trimmed().isEmpty())
                    ov.name = result[i].name;
                if (ov.slidePastePercent <= 0.0)
                    ov.slidePastePercent = result[i].slidePastePercent;
                result[i] = ov;
                merged = true;
                break;
            }
        }
        if (!merged) {
            GraphStylePresetSpec custom = ue;
            custom.builtIn = false;
            if (custom.name.trimmed().isEmpty())
                custom.name = custom.id;
            result.append(custom);
        }
    }
    // E4: presets from the shared (lab) folder are merged read-only. Built-in and personal
    // presets win on an id clash so a shared file can never silently change them.
    for (const GraphStylePresetSpec &se : readSharedPresetEntries()) {
        bool clash = false;
        for (const GraphStylePresetSpec &r : result) {
            if (r.id.compare(se.id, Qt::CaseInsensitive) == 0) {
                clash = true;
                break;
            }
        }
        if (clash)
            continue;
        GraphStylePresetSpec shared = se;
        shared.builtIn = false;
        shared.readOnly = true;
        if (shared.name.trimmed().isEmpty())
            shared.name = shared.id;
        result.append(shared);
    }
    return result;
}

bool MultiLayer::findStylePreset(const QString &idOrName, GraphStylePresetSpec *outSpec)
{
    const QString key = canonicalBuiltInAlias(idOrName);
    if (key.isEmpty())
        return false;
    const QVector<GraphStylePresetSpec> all = listStylePresets();
    for (const GraphStylePresetSpec &s : all) {
        if (s.id.compare(key, Qt::CaseInsensitive) == 0) {
            if (outSpec)
                *outSpec = s;
            return true;
        }
    }
    for (const GraphStylePresetSpec &s : all) {
        if (s.name.compare(idOrName.trimmed(), Qt::CaseInsensitive) == 0
            || (s.builtIn
                && QObject::tr(s.name.toUtf8().constData())
                           .compare(idOrName.trimmed(), Qt::CaseInsensitive)
                        == 0)) {
            if (outSpec)
                *outSpec = s;
            return true;
        }
    }
    return false;
}

bool MultiLayer::saveStylePreset(const GraphStylePresetSpec &inSpec, QString *errOut)
{
    GraphStylePresetSpec spec = inSpec;
    spec.id = canonicalBuiltInAlias(spec.id);
    if (spec.id.isEmpty()) {
        spec.id = QStringLiteral("custom_")
                + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    }
    if (spec.name.trimmed().isEmpty())
        spec.name = spec.id;

    // Mark builtIn flag accurately
    spec.builtIn = false;
    for (const GraphStylePresetSpec &d : defaultBuiltInStylePresets()) {
        if (d.id.compare(spec.id, Qt::CaseInsensitive) == 0) {
            spec.id = d.id;
            spec.builtIn = true;
            break;
        }
    }

    QVector<GraphStylePresetSpec> userEntries = readRawUserPresetEntries();
    bool replaced = false;
    for (int i = 0; i < userEntries.size(); ++i) {
        if (userEntries[i].id.compare(spec.id, Qt::CaseInsensitive) == 0) {
            userEntries[i] = spec;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        userEntries.append(spec);
    return writeUserStylePresetsArray(userEntries, errOut);
}

bool MultiLayer::deleteStylePreset(const QString &id, QString *errOut)
{
    const QString key = canonicalBuiltInAlias(id);
    for (const GraphStylePresetSpec &d : defaultBuiltInStylePresets()) {
        if (d.id.compare(key, Qt::CaseInsensitive) == 0) {
            if (errOut)
                *errOut = QObject::tr(
                        "Cannot delete a built-in preset. Use 'Reset Default' to restore its original settings.");
            return false;
        }
    }
    QVector<GraphStylePresetSpec> userEntries = readRawUserPresetEntries();
    bool inUserFile = false;
    for (const GraphStylePresetSpec &u : userEntries) {
        if (u.id.compare(key, Qt::CaseInsensitive) == 0)
            inUserFile = true;
    }
    if (!inUserFile) {
        for (const GraphStylePresetSpec &se : readSharedPresetEntries()) {
            if (se.id.compare(key, Qt::CaseInsensitive) == 0) {
                if (errOut)
                    *errOut = QObject::tr("This preset comes from the shared folder and is "
                                          "read-only. Duplicate it to make a personal copy.");
                return false;
            }
        }
    }
    int removed = 0;
    for (int i = userEntries.size() - 1; i >= 0; --i) {
        if (userEntries[i].id.compare(key, Qt::CaseInsensitive) == 0) {
            userEntries.removeAt(i);
            ++removed;
        }
    }
    if (removed == 0)
        return false;
    return writeUserStylePresetsArray(userEntries, errOut);
}

bool MultiLayer::resetBuiltInStylePreset(const QString &id, QString *errOut)
{
    const QString key = canonicalBuiltInAlias(id);
    QVector<GraphStylePresetSpec> userEntries = readRawUserPresetEntries();
    for (int i = userEntries.size() - 1; i >= 0; --i) {
        if (userEntries[i].id.compare(key, Qt::CaseInsensitive) == 0)
            userEntries.removeAt(i);
    }
    return writeUserStylePresetsArray(userEntries, errOut);
}

bool MultiLayer::exportStylePresetsToJson(const QString &filePath, QString *errOut)
{
    const QVector<GraphStylePresetSpec> all = listStylePresets();
    QJsonArray arr;
    for (const GraphStylePresetSpec &s : all)
        arr.append(stylePresetToJson(s));
    QJsonObject root;
    root[QStringLiteral("version")] = stylePresetFormatVersion();
    root[QStringLiteral("presets")] = arr;
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errOut)
            *errOut = QObject::tr("Could not write file: %1").arg(filePath);
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

int MultiLayer::importStylePresetsFromJson(const QString &filePath, QString *errOut)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errOut)
            *errOut = QObject::tr("Could not open file: %1").arg(filePath);
        return -1;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QJsonArray arr;
    if (doc.isObject() && doc.object().contains(QStringLiteral("presets")))
        arr = doc.object().value(QStringLiteral("presets")).toArray();
    else if (doc.isArray())
        arr = doc.array();
    else if (doc.isObject())
        arr.append(doc.object());
    else {
        if (errOut)
            *errOut = QObject::tr("Invalid JSON preset file.");
        return -1;
    }
    // E4: check the format version. Newer files are still imported (unknown keys are ignored)
    // but the caller gets a warning in errOut.
    const int docVersion = presetDocumentVersion(doc);
    if (docVersion > stylePresetFormatVersion() && errOut) {
        *errOut = QObject::tr("%1 was written by a newer ToriGraph (preset format %2; this "
                              "version reads format %3). Unknown settings are ignored.")
                          .arg(QFileInfo(filePath).fileName())
                          .arg(docVersion)
                          .arg(stylePresetFormatVersion());
    }

    int count = 0;
    for (const QJsonValue &v : arr) {
        if (!v.isObject())
            continue;
        GraphStylePresetSpec base;
        GraphStylePresetSpec parsed = stylePresetFromJson(v.toObject(), base);
        QString saveErr;
        if (saveStylePreset(parsed, &saveErr))
            ++count;
        else if (errOut && !saveErr.isEmpty())
            *errOut = saveErr;
    }
    return count;
}

MultiLayer::GraphStylePresetSpec MultiLayer::captureStylePresetSpec(const QString &name) const
{
    GraphStylePresetSpec s;
    s.id = QStringLiteral("custom_")
            + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    s.name = name.trimmed().isEmpty() ? QObject::tr("New Custom Preset") : name.trimmed();
    s.builtIn = false;

    const double dpi = double(qMax(72, d_page_dpi));
    auto toMm = [&](double val) -> double {
        switch (d_page_unit) {
        case UnitMm:
            return val;
        case UnitCm:
            return val * 10.0;
        case UnitInch:
            return val * 25.4;
        case UnitPx:
        default:
            return val * 25.4 / dpi;
        }
    };
    s.pageWidthMm = qBound(20.0, toMm(d_page_width_val), 1000.0);
    s.pageHeightMm = qBound(20.0, toMm(d_page_height_val), 1000.0);
    s.keepAspectRatio = d_keep_aspect_ratio;
    s.targetDpi = qBound(72, d_page_dpi, 2400);
    s.marginPx = qBound(0, left_margin, 80);
    s.pageBackground = d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white);

    if (Graph *g = active_graph) {
        const QFont tf = g->axisTitleFont(QwtPlot::xBottom);
        const QFont lf = g->axisFont(QwtPlot::xBottom);
        if (!tf.family().isEmpty())
            s.fontFamily = tf.family();
        s.axisTitlePt = (tf.pointSizeF() > 0.0) ? tf.pointSizeF() : double(qMax(6, tf.pointSize()));
        s.tickLabelPt = (lf.pointSizeF() > 0.0) ? lf.pointSizeF() : double(qMax(6, lf.pointSize()));
        s.boldTitle = tf.bold();
        s.boldTick = lf.bold();
        if (Legend *leg = g->legend()) {
            const QFont legF = leg->font();
            s.legendPt = (legF.pointSizeF() > 0.0) ? legF.pointSizeF()
                                                   : double(qMax(6, legF.pointSize()));
            s.boldLegend = legF.bold();
            s.legendFrame = qBound(0, leg->frameStyle(), 2);
        } else {
            s.legendPt = qMax(6.0, s.tickLabelPt - 1.0);
        }
        if (Plot *pl = g->plotWidget())
            s.axisLineWidth = qBound(1, pl->axesLinewidth(), 10);
        s.minorTickLen = qBound(0, g->minorTickLength(), 20);
        s.majorTickLen = qBound(0, g->majorTickLength(), 30);
        s.largeSlideLayout = (s.axisTitlePt >= 20.0 || s.tickLabelPt >= 20.0);
        s.formatIsijUnitSlash = (g->axisUnitFormat() == Column::UnitFormat::ISIJSlash);
        const bool topOn = g->plotWidget() && g->plotWidget()->axisEnabled(QwtPlot::xTop);
        const bool rightOn = g->plotWidget() && g->plotWidget()->axisEnabled(QwtPlot::yRight);
        s.framePreset = (topOn && rightOn) ? static_cast<int>(Graph::FrameBoxInwardTicks)
                                           : static_cast<int>(Graph::FrameOpenL);
        for (int cIdx = 0; cIdx < g->curves(); ++cIdx) {
            QwtPlotCurve *c = g->curve(cIdx);
            if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
                continue;
            const double cw = (c->pen().widthF() > 0.0) ? c->pen().widthF()
                                                        : double(qMax(1, c->pen().width()));
            if (c->pen().style() != Qt::NoPen) {
                s.curveWidthF = qBound(0.5, cw, 20.0);
                s.minCurveWidth = qBound(1, qRound(s.curveWidthF), 20);
            }
            if (c->symbol().style() != QwtSymbol::NoSymbol)
                s.minSymbolSize = qBound(0, c->symbol().size().width(), 100);
            break;
        }
    }
    return s;
}

void MultiLayer::showStylePresetManagerDialog(ApplicationWindow *app, MultiLayer *targetGraph)
{
    registerStylePresetTranslationsOnce();
    if (app && ApplicationWindow::isNonInteractiveContext(app))
        return;

    if (!targetGraph && app)
        targetGraph = qobject_cast<MultiLayer *>(app->d_workspace.activeSubWindow());

    const QString origNewGraphPresetId = newGraphStylePresetId();
    const QString origCurvePalette = Graph::defaultCurvePaletteName();
    const MultiPanelSizing origPanelSizing = multiPanelSizing();
    const QString origSharedFolder = sharedStylePresetsFolder();
    QString pendingNewGraphPresetId = origNewGraphPresetId;
    bool defaultsCommitted = false;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Graph Style Preset Manager"));
    dlg.setMinimumSize(680, 410);
    dlg.resize(750, 480);

    QVBoxLayout *rootLay = new QVBoxLayout(&dlg);
    rootLay->setContentsMargins(8, 8, 8, 8);
    rootLay->setSpacing(6);
    QHBoxLayout *mainSplit = new QHBoxLayout();

    // Left column: Preset list & CRUD buttons
    QVBoxLayout *leftCol = new QVBoxLayout();
    QListWidget *lw = new QListWidget(&dlg);
    lw->setMinimumWidth(235);
    leftCol->addWidget(lw, 1);

    QHBoxLayout *addRow = new QHBoxLayout();
    QPushButton *btnNew = new QPushButton(QObject::tr("New"), &dlg);
    QPushButton *btnFromGraph = new QPushButton(QObject::tr("From Current Graph"), &dlg);
    btnFromGraph->setEnabled(targetGraph != nullptr);
    addRow->addWidget(btnNew);
    addRow->addWidget(btnFromGraph);
    leftCol->addLayout(addRow);

    QHBoxLayout *actRow = new QHBoxLayout();
    QPushButton *btnDup = new QPushButton(QObject::tr("Duplicate"), &dlg);
    QPushButton *btnDel = new QPushButton(QObject::tr("Delete"), &dlg);
    QPushButton *btnReset = new QPushButton(QObject::tr("Reset Default"), &dlg);
    actRow->addWidget(btnDup);
    actRow->addWidget(btnDel);
    actRow->addWidget(btnReset);
    leftCol->addLayout(actRow);

    QHBoxLayout *ioRow = new QHBoxLayout();
    QPushButton *btnImport = new QPushButton(QObject::tr("Import JSON..."), &dlg);
    QPushButton *btnExport = new QPushButton(QObject::tr("Export JSON..."), &dlg);
    ioRow->addWidget(btnImport);
    ioRow->addWidget(btnExport);
    leftCol->addLayout(ioRow);

    mainSplit->addLayout(leftCol, 0);

    // Right column: Scrollable parameter editor + global defaults so 911x512 screens fit cleanly
    QScrollArea *rightScroll = new QScrollArea(&dlg);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    QWidget *rightScrollContent = new QWidget(rightScroll);
    QVBoxLayout *rightColLay = new QVBoxLayout(rightScrollContent);
    rightColLay->setContentsMargins(4, 0, 4, 0);
    rightColLay->setSpacing(8);

    QWidget *editorPane = new QWidget(rightScrollContent);
    QFormLayout *form = new QFormLayout(editorPane);
    form->setContentsMargins(6, 2, 2, 2);

    QLineEdit *leName = new QLineEdit(editorPane);
    form->addRow(QObject::tr("Preset Name:"), leName);

    QHBoxLayout *sizeRow = new QHBoxLayout();
    QDoubleSpinBox *sbW = new QDoubleSpinBox(editorPane);
    sbW->setRange(20.0, 1000.0);
    sbW->setDecimals(1);
    QDoubleSpinBox *sbH = new QDoubleSpinBox(editorPane);
    sbH->setRange(20.0, 1000.0);
    sbH->setDecimals(1);
    QCheckBox *chkAspect = new QCheckBox(QObject::tr("Lock Aspect Ratio"), editorPane);
    sizeRow->addWidget(sbW);
    sizeRow->addWidget(new QLabel(QStringLiteral("\u00D7"), editorPane));
    sizeRow->addWidget(sbH);
    sizeRow->addWidget(chkAspect);
    form->addRow(QObject::tr("Page Width (mm):"), sizeRow);

    QHBoxLayout *dpiRow = new QHBoxLayout();
    QSpinBox *sbDpi = new QSpinBox(editorPane);
    sbDpi->setRange(72, 2400);
    sbDpi->setSingleStep(50);
    QSpinBox *sbMargin = new QSpinBox(editorPane);
    sbMargin->setRange(0, 80);
    dpiRow->addWidget(sbDpi);
    dpiRow->addWidget(new QLabel(QObject::tr("Margin (px):"), editorPane));
    dpiRow->addWidget(sbMargin);
    form->addRow(QObject::tr("Target DPI:"), dpiRow);

    QComboBox *cbFrame = new QComboBox(editorPane);
    cbFrame->addItem(QObject::tr("Box + Inward Ticks (ISIJ)"),
                     static_cast<int>(Graph::FrameBoxInwardTicks));
    cbFrame->addItem(QObject::tr("Box + Plain Opposite Axes"),
                     static_cast<int>(Graph::FrameBoxPlainOpposite));
    cbFrame->addItem(QObject::tr("Open L + Outward Ticks (Nature)"),
                     static_cast<int>(Graph::FrameOpenL));
    cbFrame->addItem(QObject::tr("Zero-Crossed Axes"),
                     static_cast<int>(Graph::FrameZeroCrossed));
    form->addRow(QObject::tr("Frame & Ticks:"), cbFrame);

    QHBoxLayout *tickRow = new QHBoxLayout();
    QSpinBox *sbAxisLw = new QSpinBox(editorPane);
    sbAxisLw->setRange(1, 10);
    QSpinBox *sbMinTick = new QSpinBox(editorPane);
    sbMinTick->setRange(0, 20);
    QSpinBox *sbMajTick = new QSpinBox(editorPane);
    sbMajTick->setRange(0, 30);
    tickRow->addWidget(sbAxisLw);
    tickRow->addWidget(new QLabel(QObject::tr("Minor Tick Length (px):"), editorPane));
    tickRow->addWidget(sbMinTick);
    tickRow->addWidget(new QLabel(QObject::tr("Major Tick Length (px):"), editorPane));
    tickRow->addWidget(sbMajTick);
    form->addRow(QObject::tr("Axis Line Width (px):"), tickRow);

    QFontComboBox *cbFont = new QFontComboBox(editorPane);
    form->addRow(QObject::tr("Font Family:"), cbFont);

    QHBoxLayout *fTitleRow = new QHBoxLayout();
    QDoubleSpinBox *sbTitlePt = new QDoubleSpinBox(editorPane);
    sbTitlePt->setRange(5.0, 72.0);
    sbTitlePt->setDecimals(1);
    QCheckBox *chkBoldTitle = new QCheckBox(QObject::tr("Bold"), editorPane);
    fTitleRow->addWidget(sbTitlePt);
    fTitleRow->addWidget(chkBoldTitle);
    form->addRow(QObject::tr("Axis Title Size (pt):"), fTitleRow);

    QHBoxLayout *fTickRow = new QHBoxLayout();
    QDoubleSpinBox *sbTickPt = new QDoubleSpinBox(editorPane);
    sbTickPt->setRange(5.0, 72.0);
    sbTickPt->setDecimals(1);
    QCheckBox *chkBoldTick = new QCheckBox(QObject::tr("Bold"), editorPane);
    fTickRow->addWidget(sbTickPt);
    fTickRow->addWidget(chkBoldTick);
    form->addRow(QObject::tr("Tick Label Size (pt):"), fTickRow);

    QHBoxLayout *fLegRow = new QHBoxLayout();
    QDoubleSpinBox *sbLegPt = new QDoubleSpinBox(editorPane);
    sbLegPt->setRange(5.0, 72.0);
    sbLegPt->setDecimals(1);
    QCheckBox *chkBoldLeg = new QCheckBox(QObject::tr("Bold"), editorPane);
    fLegRow->addWidget(sbLegPt);
    fLegRow->addWidget(chkBoldLeg);
    form->addRow(QObject::tr("Legend Size (pt):"), fLegRow);

    QHBoxLayout *curveRow = new QHBoxLayout();
    QDoubleSpinBox *sbCurveW = new QDoubleSpinBox(editorPane);
    sbCurveW->setRange(0.5, 20.0);
    sbCurveW->setDecimals(1);
    sbCurveW->setSingleStep(0.5);
    QSpinBox *sbSymSz = new QSpinBox(editorPane);
    sbSymSz->setRange(0, 100);
    curveRow->addWidget(sbCurveW);
    curveRow->addWidget(new QLabel(QObject::tr("Symbol Size (px):"), editorPane));
    curveRow->addWidget(sbSymSz);
    form->addRow(QObject::tr("Curve Width (px):"), curveRow);

    QComboBox *cbLegFrame = new QComboBox(editorPane);
    cbLegFrame->addItem(QObject::tr("None (Transparent)"), static_cast<int>(Legend::None));
    cbLegFrame->addItem(QObject::tr("Line Box"), static_cast<int>(Legend::Line));
    cbLegFrame->addItem(QObject::tr("Shadow Box"), static_cast<int>(Legend::Shadow));
    form->addRow(QObject::tr("Legend Frame:"), cbLegFrame);

    QDoubleSpinBox *sbPastePct = new QDoubleSpinBox(editorPane);
    sbPastePct->setRange(10.0, 400.0);
    sbPastePct->setDecimals(0);
    sbPastePct->setSingleStep(10.0);
    sbPastePct->setSuffix(QStringLiteral(" %"));
    form->addRow(QObject::tr("PowerPoint Paste Scale (%):"), sbPastePct);

    QCheckBox *chkIsij = new QCheckBox(
            QObject::tr("Format axis units as ISIJ slash (Quantity, sym / unit)"), editorPane);
    QCheckBox *chkSlide = new QCheckBox(
            QObject::tr("Use large presentation-slide layout rules"), editorPane);
    form->addRow(QString(), chkIsij);
    form->addRow(QString(), chkSlide);

    rightColLay->addWidget(editorPane);

    // E1/E4/E5/E7: application-wide defaults that belong with the presets (saved on confirm only).
    QGroupBox *defaultsBox = new QGroupBox(QObject::tr("Defaults for New Graphs"), rightScrollContent);
    QFormLayout *defForm = new QFormLayout(defaultsBox);
    QComboBox *cbNewGraphPreset = new QComboBox(defaultsBox);
    QComboBox *cbCurvePalette = new QComboBox(defaultsBox);
    Graph::registerPaletteTranslations();
    cbCurvePalette->addItem(QObject::tr("ToriGraph Standard (black, vermilion, blue, ...)"),
                            Graph::toriGraphStandardPaletteName());
    cbCurvePalette->addItem(QObject::tr("Origin Classic (black, red, blue, ...)"),
                            Graph::originClassicPaletteName());
    cbCurvePalette->addItem(QObject::tr("Okabe-Ito (Colorblind Safe)"),
                            Graph::okabeItoPaletteName());
    cbCurvePalette->addItem(QObject::tr("Matplotlib (tab10)"), Graph::tab10PaletteName());
    {
        // A palette chosen elsewhere (lab defaults ini, Python) is shown as it is instead of
        // silently displaying the first entry.
        int pIdx = cbCurvePalette->findData(origCurvePalette);
        if (pIdx < 0 && !origCurvePalette.isEmpty()) {
            cbCurvePalette->addItem(QObject::tr(origCurvePalette.toUtf8().constData()),
                                    origCurvePalette);
            pIdx = cbCurvePalette->count() - 1;
        }
        cbCurvePalette->setCurrentIndex(pIdx >= 0 ? pIdx : 0);
    }
    QComboBox *cbPanelSizing = new QComboBox(defaultsBox);
    cbPanelSizing->addItem(QObject::tr("Preset size per panel (columns x width)"),
                           int(PanelSizingPerPanel));
    cbPanelSizing->addItem(QObject::tr("Journal double column (175 mm wide)"),
                           int(PanelSizingTwoColumn175));
    cbPanelSizing->setCurrentIndex(origPanelSizing == PanelSizingTwoColumn175 ? 1 : 0);
    QHBoxLayout *sharedRow = new QHBoxLayout();
    QLineEdit *leShared = new QLineEdit(origSharedFolder, defaultsBox);
    leShared->setPlaceholderText(QObject::tr("(none)"));
    QPushButton *btnSharedBrowse = new QPushButton(QObject::tr("Browse..."), defaultsBox);
    sharedRow->addWidget(leShared, 1);
    sharedRow->addWidget(btnSharedBrowse);
    defForm->addRow(QObject::tr("Preset applied to new graphs:"), cbNewGraphPreset);
    defForm->addRow(QObject::tr("Colors for new curves:"), cbCurvePalette);
    defForm->addRow(QObject::tr("Multi-panel page size:"), cbPanelSizing);
    defForm->addRow(QObject::tr("Shared preset folder (read-only):"), sharedRow);
    rightColLay->addWidget(defaultsBox);
    rightColLay->addStretch(1);

    rightScroll->setWidget(rightScrollContent);
    mainSplit->addWidget(rightScroll, 1);
    rootLay->addLayout(mainSplit, 1);

    // Bottom action buttons (no default/autoDefault so Enter inPreset Name never applies & closes)
    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *btnSave = new QPushButton(QObject::tr("Save Changes"), &dlg);
    QPushButton *btnApply = new QPushButton(QObject::tr("Save && Apply to Graph"), &dlg);
    btnApply->setEnabled(targetGraph != nullptr);
    QPushButton *btnOk = new QPushButton(QObject::tr("OK"), &dlg);
    QPushButton *btnCancel = new QPushButton(QObject::tr("Cancel"), &dlg);
    for (QPushButton *b : { btnNew, btnFromGraph, btnDup, btnDel, btnReset, btnImport, btnExport,
                            btnSharedBrowse, btnSave, btnApply, btnOk, btnCancel }) {
        b->setAutoDefault(false);
        b->setDefault(false);
    }
    btnRow->addStretch(1);
    btnRow->addWidget(btnSave);
    btnRow->addWidget(btnApply);
    btnRow->addWidget(btnOk);
    btnRow->addWidget(btnCancel);
    rootLay->addLayout(btnRow);

    QVector<GraphStylePresetSpec> presets;
    bool loadingUi = false;

    auto displayPresetName = [](const GraphStylePresetSpec &s) -> QString {
        return s.builtIn ? QObject::tr(s.name.toUtf8().constData()) : s.name;
    };

    auto readFormIntoSpec = [&](GraphStylePresetSpec &s) {
        const QString enteredName = leName->text().trimmed();
        if (enteredName.isEmpty())
            s.name = s.id;
        else if (!(s.builtIn && enteredName == displayPresetName(s)))
            s.name = enteredName;
        s.pageWidthMm = sbW->value();
        s.pageHeightMm = sbH->value();
        s.keepAspectRatio = chkAspect->isChecked();
        s.targetDpi = sbDpi->value();
        s.marginPx = sbMargin->value();
        s.framePreset = cbFrame->currentData().toInt();
        s.axisLineWidth = sbAxisLw->value();
        s.minorTickLen = sbMinTick->value();
        s.majorTickLen = sbMajTick->value();
        s.fontFamily = cbFont->currentFont().family();
        s.axisTitlePt = sbTitlePt->value();
        s.tickLabelPt = sbTickPt->value();
        s.legendPt = sbLegPt->value();
        s.boldTitle = chkBoldTitle->isChecked();
        s.boldTick = chkBoldTick->isChecked();
        s.boldLegend = chkBoldLeg->isChecked();
        s.curveWidthF = sbCurveW->value();
        s.minCurveWidth = qBound(1, qRound(s.curveWidthF), 20);
        s.minSymbolSize = sbSymSz->value();
        s.legendFrame = cbLegFrame->currentData().toInt();
        const double pct = sbPastePct->value();
        s.slidePastePercent =
                (std::fabs(pct - 100.0) < 1e-6 && s.slidePastePercent <= 0.0) ? 0.0 : pct;
        s.formatIsijUnitSlash = chkIsij->isChecked();
        s.largeSlideLayout = chkSlide->isChecked();
    };

    auto populateForm = [&](int row) {
        if (row < 0 || row >= presets.size())
            return;
        loadingUi = true;
        const GraphStylePresetSpec &s = presets[row];
        leName->setText(displayPresetName(s));
        sbW->setValue(s.pageWidthMm);
        sbH->setValue(s.pageHeightMm);
        chkAspect->setChecked(s.keepAspectRatio);
        sbDpi->setValue(s.targetDpi);
        sbMargin->setValue(s.marginPx);
        const int fIdx = cbFrame->findData(s.framePreset);
        cbFrame->setCurrentIndex(fIdx >= 0 ? fIdx : 0);
        sbAxisLw->setValue(s.axisLineWidth);
        sbMinTick->setValue(s.minorTickLen);
        sbMajTick->setValue(s.majorTickLen);
        cbFont->setCurrentFont(QFont(s.fontFamily));
        sbTitlePt->setValue(s.axisTitlePt);
        sbTickPt->setValue(s.tickLabelPt);
        sbLegPt->setValue(s.legendPt);
        chkBoldTitle->setChecked(s.boldTitle);
        chkBoldTick->setChecked(s.boldTick);
        chkBoldLeg->setChecked(s.boldLegend);
        sbCurveW->setValue((s.curveWidthF > 0.0) ? s.curveWidthF : double(s.minCurveWidth));
        sbSymSz->setValue(s.minSymbolSize);
        const int lIdx = cbLegFrame->findData(s.legendFrame);
        cbLegFrame->setCurrentIndex(lIdx >= 0 ? lIdx : 0);
        sbPastePct->setValue((s.slidePastePercent > 0.0) ? s.slidePastePercent : 100.0);
        chkIsij->setChecked(s.formatIsijUnitSlash);
        chkSlide->setChecked(s.largeSlideLayout);
        btnDel->setEnabled(!s.builtIn && !s.readOnly);
        btnReset->setEnabled(s.builtIn);
        btnSave->setEnabled(!s.readOnly);
        btnApply->setText(s.readOnly ? QObject::tr("Apply to Graph")
                                     : QObject::tr("Save && Apply to Graph"));
        loadingUi = false;
    };

    auto refillNewGraphPresetCombo = [&]() {
        QSignalBlocker blk(cbNewGraphPreset);
        cbNewGraphPreset->clear();
        cbNewGraphPreset->addItem(QObject::tr("None (keep plain pixel page)"), QString());
        for (const GraphStylePresetSpec &s : presets)
            cbNewGraphPreset->addItem(displayPresetName(s), s.id);
        const int idx = cbNewGraphPreset->findData(pendingNewGraphPresetId);
        cbNewGraphPreset->setCurrentIndex(idx >= 0 ? idx : 0);
    };

    auto reloadList = [&](const QString &selectId) {
        presets = listStylePresets();
        lw->clear();
        int targetRow = 0;
        for (int i = 0; i < presets.size(); ++i) {
            const GraphStylePresetSpec &s = presets[i];
            const QString tag = s.builtIn ? QObject::tr("Built-in")
                    : (s.readOnly ? QObject::tr("Shared") : QObject::tr("Custom"));
            QListWidgetItem *it = new QListWidgetItem(
                    QStringLiteral("[%1]  %2").arg(tag, displayPresetName(s)), lw);
            it->setData(Qt::UserRole, s.id);
            if (!selectId.isEmpty() && s.id.compare(selectId, Qt::CaseInsensitive) == 0)
                targetRow = i;
        }
        if (lw->count() > 0) {
            lw->setCurrentRow(targetRow);
            populateForm(targetRow);
        }
        refillNewGraphPresetCombo();
    };
    reloadList(QString());

    QObject::connect(cbNewGraphPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) {
                         pendingNewGraphPresetId = cbNewGraphPreset->currentData().toString();
                     });
    QObject::connect(leShared, &QLineEdit::editingFinished, &dlg, [&]() {
        if (leShared->text().trimmed() == sharedStylePresetsFolder())
            return;
        setSharedStylePresetsFolder(leShared->text());
        reloadList(QString());
    });
    QObject::connect(btnSharedBrowse, &QPushButton::clicked, &dlg, [&]() {
        const QString dir = QFileDialog::getExistingDirectory(
                &dlg, QObject::tr("Shared preset folder (read-only):"),
                leShared->text().trimmed().isEmpty() ? QDir::homePath() : leShared->text());
        if (dir.isEmpty())
            return;
        leShared->setText(QDir::toNativeSeparators(dir));
        setSharedStylePresetsFolder(leShared->text());
        reloadList(QString());
    });

    QObject::connect(lw, &QListWidget::currentRowChanged, &dlg, [&](int row) {
        if (!loadingUi)
            populateForm(row);
    });

    auto commitGlobalDefaults = [&]() {
        setNewGraphStylePresetId(cbNewGraphPreset->currentData().toString());
        Graph::setDefaultCurvePaletteName(cbCurvePalette->currentData().toString());
        if (app && app->graphInspectorDock())
            app->graphInspectorDock()->syncSeriesSchemeToDefaultPalette();
        setMultiPanelSizing(static_cast<MultiPanelSizing>(cbPanelSizing->currentData().toInt()));
        setSharedStylePresetsFolder(leShared->text());
        defaultsCommitted = true;
    };

    auto saveCurrent = [&]() -> bool {
        const int row = lw->currentRow();
        if (row < 0 || row >= presets.size())
            return false;
        GraphStylePresetSpec s = presets[row];
        if (s.readOnly)
            return true; // shared presets are applied as they are, never written
        readFormIntoSpec(s);
        QString err;
        if (!saveStylePreset(s, &err)) {
            QMessageBox::warning(&dlg, dlg.windowTitle(), err);
            return false;
        }
        reloadList(s.id);
        return true;
    };

    QObject::connect(btnNew, &QPushButton::clicked, &dlg, [&]() {
        GraphStylePresetSpec s;
        s.id = QStringLiteral("custom_")
                + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        s.name = QObject::tr("New Custom Preset");
        s.builtIn = false;
        saveStylePreset(s);
        reloadList(s.id);
        leName->setFocus();
        leName->selectAll();
    });

    QObject::connect(btnFromGraph, &QPushButton::clicked, &dlg, [&]() {
        if (!targetGraph)
            return;
        GraphStylePresetSpec s = targetGraph->captureStylePresetSpec(
                QObject::tr("New Custom Preset"));
        saveStylePreset(s);
        reloadList(s.id);
        leName->setFocus();
        leName->selectAll();
    });

    QObject::connect(btnDup, &QPushButton::clicked, &dlg, [&]() {
        const int row = lw->currentRow();
        if (row < 0 || row >= presets.size())
            return;
        GraphStylePresetSpec s = presets[row];
        readFormIntoSpec(s);
        s.id = QStringLiteral("custom_")
                + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        s.builtIn = false;
        s.readOnly = false;
        s.sourcePath.clear();
        s.name = QObject::tr("Copy of %1").arg(displayPresetName(presets[row]));
        saveStylePreset(s);
        reloadList(s.id);
    });

    QObject::connect(btnDel, &QPushButton::clicked, &dlg, [&]() {
        const int row = lw->currentRow();
        if (row < 0 || row >= presets.size())
            return;
        QString err;
        if (!deleteStylePreset(presets[row].id, &err)) {
            if (!err.isEmpty())
                QMessageBox::information(&dlg, dlg.windowTitle(), err);
            return;
        }
        reloadList(QString());
    });

    QObject::connect(btnReset, &QPushButton::clicked, &dlg, [&]() {
        const int row = lw->currentRow();
        if (row < 0 || row >= presets.size() || !presets[row].builtIn)
            return;
        const QString id = presets[row].id;
        resetBuiltInStylePreset(id);
        reloadList(id);
    });

    QObject::connect(btnExport, &QPushButton::clicked, &dlg, [&]() {
        const QString path = QFileDialog::getSaveFileName(
                &dlg, QObject::tr("Export JSON..."),
                QDir::homePath() + QStringLiteral("/torigraph_presets.json"),
                QStringLiteral("JSON (*.json)"));
        if (path.isEmpty())
            return;
        QString err;
        if (!exportStylePresetsToJson(path, &err))
            QMessageBox::warning(&dlg, dlg.windowTitle(), err);
        else
            QMessageBox::information(&dlg, dlg.windowTitle(),
                                     QObject::tr("Saved presets to %1").arg(path));
    });

    QObject::connect(btnImport, &QPushButton::clicked, &dlg, [&]() {
        const QString path = QFileDialog::getOpenFileName(
                &dlg, QObject::tr("Import JSON..."), QDir::homePath(),
                QStringLiteral("JSON (*.json)"));
        if (path.isEmpty())
            return;
        QString err;
        const int n = importStylePresetsFromJson(path, &err);
        if (n < 0)
            QMessageBox::warning(&dlg, dlg.windowTitle(), err);
        else {
            reloadList(QString());
            QString msg = QObject::tr("Imported %1 preset(s).").arg(n);
            if (!err.isEmpty())
                msg += QStringLiteral("\n\n") + err;
            QMessageBox::information(&dlg, dlg.windowTitle(), msg);
        }
    });

    QObject::connect(btnSave, &QPushButton::clicked, &dlg, [&]() {
        if (saveCurrent())
            commitGlobalDefaults();
    });
    QObject::connect(btnApply, &QPushButton::clicked, &dlg, [&]() {
        if (!saveCurrent() || !targetGraph)
            return;
        commitGlobalDefaults();
        const int row = lw->currentRow();
        if (row >= 0 && row < presets.size()) {
            targetGraph->applyStylePresetSpec(presets[row]);
            if (app) {
                app->modifiedProject();
                if (GraphInspectorDock *insp = app->graphInspectorDock())
                    insp->refreshAll();
            }
        }
        dlg.accept();
    });
    QObject::connect(btnOk, &QPushButton::clicked, &dlg, [&]() {
        if (!saveCurrent())
            return;
        commitGlobalDefaults();
        dlg.accept();
    });
    QObject::connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    const int dlgRes = dlg.exec();
    if (!defaultsCommitted && dlgRes != QDialog::Accepted) {
        if (sharedStylePresetsFolder() != origSharedFolder)
            setSharedStylePresetsFolder(origSharedFolder);
    }
    if (app) {
        if (GraphInspectorDock *insp = app->graphInspectorDock())
            insp->refreshAll();
    }
}



