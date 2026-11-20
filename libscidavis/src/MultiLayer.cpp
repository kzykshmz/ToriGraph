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
#include "ModernUiTheme.h"
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
} // namespace
#include "QwtErrorPlotCurve.h"
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
    setFont(QFont("Arial", 9, QFont::Bold, false));
    setStyleSheet(
            "QPushButton { background-color: transparent; color: #334155; font-size: 9.5pt; "
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
    const int frameDW = (mainW && ml->width() > mainW->width() && mainW->width() >= 60)
            ? qBound(0, ml->width() - mainW->width(), 32)
            : 0;
    const int frameDH = (mainW && ml->height() > mainW->height() && mainW->height() >= 60)
            ? qBound(0, ml->height() - mainW->height(), 48)
            : 0;
    int wantW = cw + hostPad + frameDW;
    int wantH = ch + hostPad + topBarH + frameDH;
    if (ml->mdiArea() && ml->mdiArea()->viewport() && ml->mdiArea()->viewport()->isVisible()
        && ml->mdiArea()->viewport()->width() >= 160
        && ml->mdiArea()->viewport()->height() >= 160) {
        wantW = qMin(wantW, ml->mdiArea()->viewport()->width());
        wantH = qMin(wantH, ml->mdiArea()->viewport()->height());
    }
    ml->resize(wantW, wantH);
}

struct ScopedUnzoomForExport
{
    QPointer<MultiLayer> ml;
    double prevZoom = 1.0;
    QSize prevWinSize;
    bool prevMlUpdates = true;
    bool prevWidgetUpdates = true;
    bool active = false;

    explicit ScopedUnzoomForExport(MultiLayer *target) : ml(target)
    {
        if (ml && qAbs(ml->pageZoom() - 1.0) > 1e-4) {
            prevZoom = ml->pageZoom();
            prevWinSize = ml->size();
            prevMlUpdates = ml->updatesEnabled();
            ml->setUpdatesEnabled(false);
            if (QWidget *w = ml->widget()) {
                prevWidgetUpdates = w->updatesEnabled();
                w->setUpdatesEnabled(false);
            }
            QSignalBlocker sb(ml);
            ml->setPageZoom(1.0);
            active = true;
        }
    }

    ~ScopedUnzoomForExport()
    {
        if (active && ml) {
            {
                QSignalBlocker sb(ml);
                ml->setPageZoom(prevZoom);
                if (!ml->isMaximized() && !ml->isMinimized() && prevWinSize.isValid()
                    && ml->size() != prevWinSize) {
                    ml->resize(prevWinSize);
                }
            }
            if (QWidget *w = ml->widget())
                w->setUpdatesEnabled(prevWidgetUpdates);
            ml->setUpdatesEnabled(prevMlUpdates);
        }
    }
};

} // namespace

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
            "font-size: 9.5pt; font-weight: 600; "
            "border: 1px solid #CBD5E1; border-radius: 5px; padding: 0px 7px; margin: 0px; }"
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
    d_canvas_status_pill->setStyleSheet(QStringLiteral(
            "QToolButton#MultiLayerStatusPill {"
            "  background-color: rgba(255, 255, 255, 235);"
            "  color: #334155;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 12px;"
            "  padding: 2px 11px;"
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
        statusMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("preset-nature"), 18, false),
                              tr("Nature 89mm"), this,
                              [this]() { applyStylePreset(StyleNatureCompact89mm); });
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

    resizeLayers(size, oldSize, false);
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

    if (QClipboard *cb = QApplication::clipboard())
        cb->setImage(image);

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
    if (fileName.contains(".eps", Qt::CaseInsensitive)) {
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

    QSvgGenerator generator;
    generator.setFileName(fname);
#if QT_VERSION >= 0x040500
    generator.setSize(svgPxSize);
    generator.setViewBox(QRect(QPoint(0, 0), svgPxSize));
    generator.setResolution(96);
    generator.setTitle(this->name());
#endif
    exportPainter(generator, false, srcRect);
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
    if (SciDAVisOle::copyGraphAsOle(qobject_cast<ApplicationWindow *>(window()), this, nullptr))
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
        QPainter paint(&printer);
        printAllLayers(&paint);
        paint.end();
    }
}

void MultiLayer::printAllLayers(QPainter *painter)
{
    if (!painter)
        return;

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
        QwtPlot *plot = gr->plotWidget();

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
    const QSize normSz = isMaximized() ? normalGeometry().size() : size();
    if (normSz.width() >= 100 && normSz.height() >= 100) {
        s += QString("<ToriWinSize w=\"%1\" h=\"%2\"/>\n")
                     .arg(normSz.width())
                     .arg(normSz.height());
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
    return s + "</multiLayer>\n";
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
    emit modifiedPlot();
}

void MultiLayer::applyAspectRatioPreset(double ratio, bool keepLocked)
{
    if (ratio <= 0.05 || ratio > 20.0)
        return;
    d_keep_aspect_ratio = keepLocked;
    setTargetAspectRatio(ratio);
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

    const double ratio = clamped / d_page_zoom;
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

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr)
            continue;
        const QRect gRect = gr->geometry();
        const QRect scaledRect(qRound(gRect.x() * ratio), qRound(gRect.y() * ratio),
                               qMax(60, qRound(gRect.width() * ratio)),
                               qMax(60, qRound(gRect.height() * ratio)));
        gr->setGeometry(scaledRect);
        gr->plotWidget()->resize(scaledRect.size());
        gr->plotWidget()->updateLayout();
        gr->setIgnoreResizeEvents(savedIgnore[i]);
    }
    l_canvas_width = qMax(50, qRound(l_canvas_width * ratio));
    l_canvas_height = qMax(50, qRound(l_canvas_height * ratio));
    d_syncing_inset_geometries = prevSync;

    syncChildOverlayAndInsetGeometries();
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
    inset->setBackgroundColor(QColor(255, 255, 255, 0));
    inset->setCanvasBackground(QColor("#FFFFFF"));
    inset->setAutoFillBackground(false);
    if (inset->plotWidget()) {
        inset->plotWidget()->setAutoFillBackground(false);
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
    const QString beforeSnap = captureUndoSnapshot();

    d_keep_aspect_ratio = true;
    d_target_aspect_ratio = 1.0;
    setPageBackgroundColor(QColor(Qt::white));

    double pageW = 85.0;
    double pageH = 85.0;
    int targetDpi = 600;
    int marginPx = 6;
    Graph::FramePreset framePreset = Graph::FrameBoxInwardTicks;
    int axisLw = 2;
    int minTickLen = 4;
    int majTickLen = 8;
    QFont titleFnt("Arial", 11, QFont::Normal, false);
    QFont tickFnt("Arial", 10, QFont::Normal, false);
    QFont legFnt("Arial", 10, QFont::Normal, false);
    legFnt.setPointSizeF(9.5);
    int legFrame = Legend::None;
    QColor legBg(255, 255, 255, 0);
    int minCurveWidth = 2;
    int minSymSize = 0;
    QString undoLabel = tr("Apply Style Preset");

    switch (preset) {
    case StyleISIJJournal85mm:
        pageW = 85.0;
        pageH = 85.0;
        targetDpi = 600;
        marginPx = 6;
        framePreset = Graph::FrameBoxInwardTicks;
        axisLw = 2;
        minTickLen = 4;
        majTickLen = 8;
        titleFnt = QFont("Arial", 11, QFont::Normal, false);
        tickFnt = QFont("Arial", 10, QFont::Normal, false);
        legFnt = QFont("Arial", 9, QFont::Normal, false);
        legFrame = Legend::None;
        legBg = QColor(255, 255, 255, 0);
        minCurveWidth = 2;
        minSymSize = 7;
        undoLabel = tr("Apply ISIJ Journal Preset");
        break;

    case StylePresentationSlide28pt:
        pageW = 185.0;
        pageH = 185.0;
        targetDpi = 300;
        marginPx = 10;
        framePreset = Graph::FrameBoxInwardTicks;
        axisLw = 3;
        minTickLen = 6;
        majTickLen = 12;
        titleFnt = QFont("Arial", 28, QFont::Normal, false);
        tickFnt = QFont("Arial", 28, QFont::Normal, false);
        legFnt = QFont("Arial", 24, QFont::Normal, false);
        legFrame = Legend::None;
        legBg = QColor(255, 255, 255, 0);
        minCurveWidth = 3;
        minSymSize = 12;
        undoLabel = tr("Apply Presentation Slide (28 pt) Preset");
        break;

    case StyleNatureCompact89mm:
        pageW = 89.0;
        pageH = 89.0;
        targetDpi = 600;
        marginPx = 6;
        framePreset = Graph::FrameOpenL;
        axisLw = 1;
        minTickLen = 3;
        majTickLen = 6;
        titleFnt = QFont("Arial", 8, QFont::Normal, false);
        tickFnt = QFont("Arial", 7, QFont::Normal, false);
        legFnt = QFont("Arial", 7, QFont::Normal, false);
        legFnt.setPointSizeF(6.5);
        legFrame = Legend::None;
        legBg = QColor(255, 255, 255, 0);
        minCurveWidth = 1;
        minSymSize = 5;
        undoLabel = tr("Apply Nature Compact Preset");
        break;
    }

    titleFnt.setBold(false);
    tickFnt.setBold(false);
    legFnt.setBold(false);
    titleFnt.setHintingPreference(QFont::PreferNoHinting);
    tickFnt.setHintingPreference(QFont::PreferNoHinting);
    legFnt.setHintingPreference(QFont::PreferNoHinting);

    if (rows > 1)
        pageH *= 0.85 * double(rows);
    if (cols > 1 && pageW < 170.0)
        pageW = qMin(175.0, pageW * 0.90 * double(cols));
    if (cols <= 1 && pageW < 110.0) {
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
    setPageZoom(1.0);
    setPageSizePhysical(pageW, pageH, UnitMm, targetDpi);
    d_keep_aspect_ratio = true;
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
            if (!gr->isTernaryPlot() && !gr->isPolarPlot() && !gr->isPiePlot()) {
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
            gr->setAxesLinewidth((preset == StylePresentationSlide28pt) ? 2 : 1);
            gr->setTicksLength((preset == StylePresentationSlide28pt) ? 3 : 2,
                               (preset == StylePresentationSlide28pt) ? 6 : 4);
        } else {
            gr->setAxesLinewidth(axisLw);
            gr->changeTicksLength(minTickLen, majTickLen);
        }

        QwtPlot *plot = gr->plotWidget();
        QwtText pTitle = plot->title();
        QString cleanPlotTitle = pTitle.text();
        cleanPlotTitle.remove(kBoldTagRx);
        pTitle.setText(cleanPlotTitle);
        pTitle.setFont(titleFnt);
        plot->setTitle(pTitle);

        if (preset == StyleISIJJournal85mm && !isInset) {
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
                                || plainUnit == QLatin1String("%")
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

        const int insetPt = (preset == StylePresentationSlide28pt) ? 14 : 8;
        const bool applyPresetTitleFmt = !isInset
                && (preset == StyleISIJJournal85mm || preset == StylePresentationSlide28pt);
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            QFont useTick = isInset ? QFont("Arial", insetPt, QFont::Normal, false) : tickFnt;
            useTick.setHintingPreference(QFont::PreferNoHinting);
            QFont useTitle = isInset ? QFont("Arial", insetPt, QFont::Normal, false) : titleFnt;
            useTitle.setHintingPreference(QFont::PreferNoHinting);
            gr->setAxisFont(a, useTick);
            gr->setAxisTitleFont(a, useTitle);
            QString rawTitle = gr->axisTitle(a);
            QString nextAxisTitle = rawTitle;
            nextAxisTitle.remove(kBoldTagRx);
            if (applyPresetTitleFmt) {
                nextAxisTitle =
                        formatPresetAxisTitle(nextAxisTitle, preset == StyleISIJJournal85mm);
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
                        const QString fmtCbT =
                                formatPresetAxisTitle(cbT, preset == StyleISIJJournal85mm);
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
            }
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
            const int curPt = mrk->font().pointSize();
            const int maxAutoScalePt = (preset == StylePresentationSlide28pt) ? 14 : 12;
            const bool shouldApplyLegFnt = !isSigLabel
                    && (isCurveLegend || isPanelLabel || isFitCallout
                        || (curPt > 0 && curPt <= maxAutoScalePt));
            if (isSigLabel) {
                QFont sigFnt = legFnt;
                sigFnt.setPointSize(
                        qBound(8, legFnt.pointSize() - (preset == StylePresentationSlide28pt ? 4 : 0),
                               16));
                sigFnt.setBold(false);
                sigFnt.setHintingPreference(QFont::PreferNoHinting);
                mrk->setFont(sigFnt);
            } else if (shouldApplyLegFnt) {
                mrk->setFont(legFnt);
            } else {
                QFont customFnt = mrk->font();
                customFnt.setFamily(QStringLiteral("Arial"));
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

        for (int cIdx = 0; cIdx < gr->curves(); ++cIdx) {
            QwtPlotCurve *c = gr->curve(cIdx);
            if (!c)
                continue;
            QPen pen = c->pen();
            if (pen.style() != Qt::NoPen && pen.width() < minCurveWidth
                && pen.widthF() < double(minCurveWidth)) {
                pen.setWidth(minCurveWidth);
                c->setPen(pen);
            }
            if (minSymSize > 0 && c->symbol().style() != QwtSymbol::NoSymbol) {
                QwtSymbol sym = c->symbol();
                if (sym.size().width() < minSymSize || sym.size().height() < minSymSize) {
                    sym.setSize(QSize(qMax(minSymSize, sym.size().width()),
                                      qMax(minSymSize, sym.size().height())));
                }
                if (preset == StylePresentationSlide28pt) {
                    QPen sp = sym.pen();
                    if (sp.width() < 2)
                        sp.setWidth(2);
                    sym.setPen(sp);
                }
                c->setSymbol(sym);
            }
        }
        if ((preset == StyleISIJJournal85mm || preset == StylePresentationSlide28pt || isInset)
            && !isLinkedOverlay && !gr->isTernaryPlot() && !gr->isPolarPlot()
            && !gr->isPiePlot()) {
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
                else if (preset == StylePresentationSlide28pt || crowdedHorizTicks)
                    maj = qMin(maj, crowdedHorizTicks ? 4 : 5);
                double step = isInset ? 0.0 : gr->axisStep(a);
                if (!isInset && step <= 0.0 && !isLog && majTicks.size() >= 2) {
                    if (!((preset == StylePresentationSlide28pt && majTicks.size() > 6)
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
                || gr->isTernaryPlot() || gr->isPolarPlot() || gr->isPiePlot())
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
        bool anyTitleAdjusted = false;
        static const QRegularExpression kRichHtmlTagRx(
                QStringLiteral("<\\s*(?:i|sub|sup|b|u|span|font)\\b"),
                QRegularExpression::CaseInsensitiveOption);
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *gr = (Graph *)graphsList.at(i);
            if (!gr || !gr->plotWidget() || d_inset_specs.contains(gr) || gr->linkedParentLayer()
                || gr->isTernaryPlot() || gr->isPolarPlot() || gr->isPiePlot())
                continue;
            Plot *pl = gr->plotWidget();
            pl->updateLayout();
            const QRect cr = pl->plotLayout()->canvasRect();
            const int minPt = (preset == StylePresentationSlide28pt) ? 16 : 8;
            const int wrapPt = (preset == StylePresentationSlide28pt) ? 18 : 9;

            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                if (!pl->axisEnabled(a))
                    continue;
                const int spanPx =
                        (a == QwtPlot::xBottom || a == QwtPlot::xTop) ? cr.width() : cr.height();
                if (spanPx < 50)
                    continue;
                const int maxTitlePx = spanPx - 6;
                QwtText curQt = pl->axisTitle(a);
                if (curQt.text().trimmed().isEmpty())
                    continue;
                if (curQt.textSize().width() <= maxTitlePx)
                    continue;

                QFont baseFnt = gr->axisTitleFont(a);
                baseFnt.setFamily(QStringLiteral("Arial"));
                baseFnt.setBold(false);
                baseFnt.setWeight(QFont::Normal);
                baseFnt.setHintingPreference(QFont::PreferNoHinting);
                const int startPt = qMax(minPt, baseFnt.pointSize() > 0 ? baseFnt.pointSize() : 11);

                bool fittedSingleLine = false;
                for (int tryPt = startPt - 1; tryPt >= minPt; --tryPt) {
                    QFont tryFnt = baseFnt;
                    tryFnt.setPointSize(tryPt);
                    QwtText tryQt = curQt;
                    tryQt.setFont(tryFnt);
                    if (tryQt.textSize().width() <= maxTitlePx) {
                        gr->setAxisTitleFont(a, tryFnt);
                        anyTitleAdjusted = true;
                        fittedSingleLine = true;
                        break;
                    }
                }
                if (fittedSingleLine)
                    continue;

                QString cleanStr = gr->axisTitle(a).trimmed();
                if (cleanStr.contains(QLatin1Char('\n'))
                    || cleanStr.contains(QLatin1String("<br"), Qt::CaseInsensitive)) {
                    QFont minFnt = baseFnt;
                    minFnt.setPointSize(minPt);
                    gr->setAxisTitleFont(a, minFnt);
                    anyTitleAdjusted = true;
                    continue;
                }

                QFont wrapFnt = baseFnt;
                wrapFnt.setPointSize(qBound(minPt, wrapPt, startPt));
                const bool hasHtml = cleanStr.contains(kRichHtmlTagRx);
                const QString brkSep = hasHtml ? QStringLiteral("<br/>") : QStringLiteral("\n");

                auto fitsLine = [&](const QString &part, const QFont &fnt) -> bool {
                    QwtText qtPart(part);
                    qtPart.setFont(fnt);
                    return qtPart.textSize().width() <= maxTitlePx;
                };

                QString wrapped;
                const int slashPos = cleanStr.lastIndexOf(QLatin1String(" / "));
                if (slashPos > 4) {
                    const QString l1 = cleanStr.left(slashPos).trimmed();
                    const QString l2 = cleanStr.mid(slashPos + 1).trimmed();
                    if (fitsLine(l1, wrapFnt) && fitsLine(l2, wrapFnt))
                        wrapped = l1 + brkSep + l2;
                }
                if (wrapped.isEmpty()) {
                    const int parenPos = cleanStr.lastIndexOf(QLatin1String(" ("));
                    if (parenPos > 4) {
                        const QString l1 = cleanStr.left(parenPos).trimmed();
                        const QString l2 = cleanStr.mid(parenPos + 1).trimmed();
                        if (fitsLine(l1, wrapFnt) && fitsLine(l2, wrapFnt))
                            wrapped = l1 + brkSep + l2;
                    }
                }
                if (wrapped.isEmpty()) {
                    const int mid = cleanStr.size() / 2;
                    int bestSp = -1;
                    int bestDist = cleanStr.size() + 1;
                    int tagDepth = 0;
                    for (int ci = 0; ci < cleanStr.size(); ++ci) {
                        const QChar ch = cleanStr.at(ci);
                        if (ch == QLatin1Char('<'))
                            ++tagDepth;
                        else if (ch == QLatin1Char('>') && tagDepth > 0)
                            --tagDepth;
                        else if (tagDepth == 0 && ch == QLatin1Char(' ') && ci > 2
                                 && ci + 2 < cleanStr.size()) {
                            const int d = std::abs(ci - mid);
                            if (d < bestDist) {
                                bestDist = d;
                                bestSp = ci;
                            }
                        }
                    }
                    if (bestSp > 0) {
                        const QString l1 = cleanStr.left(bestSp).trimmed();
                        const QString l2 = cleanStr.mid(bestSp + 1).trimmed();
                        if (!fitsLine(l1, wrapFnt) || !fitsLine(l2, wrapFnt))
                            wrapFnt.setPointSize(minPt);
                        wrapped = l1 + brkSep + l2;
                    }
                }

                if (!wrapped.isEmpty()) {
                    const bool wasAuto = gr->isAxisTitleAuto(a);
                    QwtText newQt = pl->axisTitle(a);
                    newQt.setFont(wrapFnt);
                    newQt.setText(wrapped);
                    pl->setAxisTitle(a, newQt);
                    gr->setAxisTitleAuto(a, wasAuto);
                } else {
                    wrapFnt.setPointSize(minPt);
                    gr->setAxisTitleFont(a, wrapFnt);
                }
                anyTitleAdjusted = true;
            }
        }
        if (anyTitleAdjusted)
            applySquareCanvasFrame70Percent();
    }

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr || !gr->plotWidget())
            continue;
        const bool isInset = d_inset_specs.contains(gr);
        const bool isLinkedOverlay = (gr->linkedParentLayer() != nullptr);
        if (!isInset && !isLinkedOverlay && !gr->isTernaryPlot() && !gr->isPolarPlot()
            && !gr->isPiePlot()) {
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
                                    gr->updateSecondaryAxis(QwtPlot::yRight);
                                    gr->updateMarkersBoundingRect();
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
                        (preset == StylePresentationSlide28pt || curPt > 14.0) ? 16.0 : 7.0;
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
            } else if (gr->isTernaryPlot() || gr->isPolarPlot()) {
                gr->placeLegendAnchor(Graph::LegendTopRight);
            } else if (legendWasDefaultTopLeft[i] || !gr->lineMarkerKeys().isEmpty()) {
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

    const int zoomPct = qRound(d_page_zoom * 100.0);
    const QString sep = QString(QChar(0x00B7));
    const QString times = QString(QChar(0x00D7));
    const QString fullStatusText =
            QStringLiteral("%1 %2 %3 %4 %5 %6 DPI %5 %7 %5 %8%")
                    .arg(QString::number(wDisp, 'f', decimals), times,
                         QString::number(hDisp, 'f', decimals), unitStr, sep,
                         QString::number(d_page_dpi), fontFam, QString::number(zoomPct));
    const QString compactStatusText =
            QStringLiteral("%1%2%3 %4 %5 %6 DPI %5 %7")
                    .arg(QString::number(wDisp, 'f', decimals), times,
                         QString::number(hDisp, 'f', decimals), unitStr, sep,
                         QString::number(d_page_dpi), fontFam);

    d_canvas_status_pill->setProperty("_fullStatusText", fullStatusText);
    d_canvas_status_pill->setProperty("_compactStatusText", compactStatusText);
    d_canvas_status_pill->setText(fullStatusText);
    d_canvas_status_pill->setToolTip(
            tr("Page Size, DPI, Font & Zoom — Click for Quick Presets"));
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
    if (!fullTxt.isEmpty() && d_canvas_status_pill->text() != fullTxt)
        d_canvas_status_pill->setText(fullTxt);

    d_canvas_status_pill->adjustSize();
    QSize pillSz =
            d_canvas_status_pill->sizeHint().expandedTo(d_canvas_status_pill->minimumSizeHint());
    int pw = qMin(pillSz.width(), qMax(80, refSz.width() - 16 - sbRight));
    int ph = qMax(22, pillSz.height());
    int x = qMax(8, refSz.width() - pw - 10 - sbRight);
    int y = qMax(LayerButton::btnSize() + 8, refSz.height() - ph - 8 - sbBottom);

    bool overlapsCanvas = false;
    if (canvas && canvas->isVisible()) {
        const QPoint canvasTopLeft = canvas->mapTo(widget(), QPoint(0, 0));
        const QRect canvasInHost(canvasTopLeft, canvas->size());
        if (canvasInHost.intersects(QRect(x - 4, y - 4, pw + 8, ph + 8)))
            overlapsCanvas = true;
    }

    if (overlapsCanvas) {
        const int leftReserved = (d_layer_pill_frame && d_layer_pill_frame->isVisible())
                ? (d_layer_pill_frame->geometry().right() + 10)
                : 8;
        const int availTopW = refSz.width() - leftReserved - 8 - sbRight;
        if (availTopW < pw && !compactTxt.isEmpty()) {
            d_canvas_status_pill->setText(compactTxt);
            d_canvas_status_pill->adjustSize();
            pillSz = d_canvas_status_pill->sizeHint().expandedTo(
                    d_canvas_status_pill->minimumSizeHint());
            pw = qMin(pillSz.width(), qMax(60, availTopW));
            ph = qMax(20, qMin(LayerButton::btnSize(), pillSz.height()));
        } else {
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

        int pt = d_panel_label_font_pt;
        if (pt <= 0) {
            const int titlePt = gr->axisTitleFont(QwtPlot::yLeft).pointSize();
            pt = (titlePt > 0) ? qBound(9, titlePt >= 24 ? titlePt : titlePt + 1, 40) : 12;
        }

        QFont fnt(QStringLiteral("Arial"), pt, QFont::Normal, false);
        fnt.setBold(false);
        fnt.setHintingPreference(QFont::PreferNoHinting);
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
                                               .textSize(gr->axisTitleFont(QwtPlot::yLeft))
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

        int pt = d_panel_label_font_pt;
        if (pt <= 0) {
            const int titlePt = gr->axisTitleFont(QwtPlot::yLeft).pointSize();
            pt = (titlePt > 0) ? qBound(9, titlePt >= 24 ? titlePt : titlePt + 1, 40) : 12;
        }

        QFont fnt(QStringLiteral("Arial"), pt, QFont::Normal, false);
        fnt.setBold(false);
        fnt.setHintingPreference(QFont::PreferNoHinting);

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
                                               .textSize(gr->axisTitleFont(QwtPlot::yLeft))
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
        int maxLabelPt = (d_panel_label_font_pt > 0) ? d_panel_label_font_pt : 12;
        if (d_panel_label_font_pt <= 0) {
            for (Graph *gr : primaryGraphs) {
                const int titlePt = gr->axisTitleFont(QwtPlot::yLeft).pointSize();
                const int pt = (titlePt > 0) ? qBound(9, titlePt >= 24 ? titlePt : titlePt + 1, 40)
                                             : 12;
                maxLabelPt = qMax(maxLabelPt, pt);
            }
        }
        const QFont lblFont(QStringLiteral("Arial"), maxLabelPt, QFont::Normal, false);
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
        const bool isMultiPanel = (nRows >= 2 || nCols >= 2);
        const int capX = isMultiPanel ? qBound(3, side / 38, 6) : qBound(4, side / 42, 6);
        const int capY = isMultiPanel ? qBound(3, side / 28, 6) : 10;
        for (Graph *gr : primaryGraphs) {
            Plot *pl = gr ? gr->plotWidget() : nullptr;
            if (!pl || gr->isTernaryPlot() || gr->isPolarPlot() || gr->isPiePlot())
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
                            && sumW > qRound(0.84 * double(side))) {
                            gr->setAxisLabelRotation(QwtPlot::xBottom, -35);
                            pl->updateLayout();
                        }
                    } else {
                        const double crowdThresh =
                                (gr->axisStep(QwtPlot::xBottom) > 0.0) ? 0.90 : 0.82;
                        if (sumW > qRound(crowdThresh * double(side))) {
                            xLabelsCrowded = true;
                            const int avgCell = qMax(22, (sumW / mTicks.size()) + 4);
                            safeXCaps =
                                    qBound(2, qRound(0.80 * double(side) / double(avgCell)), 5);
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
            "QLabel { color: %3; border: none; font-weight: 600; font-size: 8.5pt; }"
            "QTreeWidget { background-color: %1; color: %3; border: 1px solid %4; "
            "border-radius: 5px; font-size: 8.5pt; }")
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


