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
#include "SelectionMoveResizer.h"
#include "LayerDialog.h"
#include "PlotCurve.h"
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
#include <cmath>

#include <gsl/gsl_vector.h>

#include <iostream>
using namespace std;

LayerButton::LayerButton(const QString &text, QWidget *parent) : QPushButton(text, parent)
{
    int btn_size = LayerButton::btnSize();

    setCheckable(true);
    setChecked(true);
    setFixedSize(btn_size, btn_size);
    setFont(QFont("Arial", 9, QFont::Bold, false));
    setStyleSheet(
            "QPushButton { background-color: transparent; color: #334155; "
            "border: 1px solid transparent; border-radius: 5px; padding: 0px; margin: 0px; }"
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
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("layer-linked"), 18, false),
                           tr("Add Linked Top-X / Right-Y Layer"), this, [this]() {
            const QString snap = captureUndoSnapshot();
            addLinkedLayer(activeGraph(), true, true, QString("x"), QString("y"));
            pushUndoSnapshot(tr("Add Linked Layer"), snap);
        });
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-double-y"), 18, false),
                           tr("Add Right-Y Layer (Double-Y)"), this, [this]() {
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
            }
            pushUndoSnapshot(tr("Add Right-Y Layer"), snap);
        });
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("layer-inset"), 18, false),
                           tr("Add Zoom Inset Layer"), this, [this]() {
            const QString snap = captureUndoSnapshot();
            addInsetLayer(activeGraph());
            pushUndoSnapshot(tr("Add Inset Layer"), snap);
        });
        addMenu->addSeparator();
        addMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("layer-add"), 18, false),
                           tr("Add Empty Layer"), this, [this]() {
            const QString snap = captureUndoSnapshot();
            Graph *g = addLayer();
            if (g)
                arrangeLayers(true, false);
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
    d_quick_columns_btn->setText(tr("Columns") + QStringLiteral(" \u25BE"));
    d_quick_columns_btn->setToolTip(tr("Toggle Plotted Columns on Active Layer"));
    d_quick_columns_btn->setFocusPolicy(Qt::NoFocus);
    d_quick_columns_btn->setFixedHeight(LayerButton::btnSize());
    d_quick_columns_btn->setStyleSheet(QStringLiteral(
            "QToolButton { background-color: #FFFFFF; color: #334155; "
            "font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif; "
            "font-size: 8pt; font-weight: 600; "
            "border: 1px solid #CBD5E1; border-radius: 5px; padding: 0px 6px; margin: 0px; }"
            "QToolButton:hover { background-color: #EFF6FF; color: #1D4ED8; "
            "border: 1px solid #2563EB; }"));
    connect(d_quick_columns_btn, &QToolButton::clicked, this,
            &MultiLayer::openQuickLayerColumnsPopup);
    pillLayout->addWidget(d_quick_columns_btn);

    hbox->addWidget(d_layer_pill_frame);
    hbox->addStretch();

    canvas = new QWidget();
    canvas->setAutoFillBackground(true);
    {
        QPalette cpal = canvas->palette();
        cpal.setColor(QPalette::Window, QColor("#FFFFFF"));
        canvas->setPalette(cpal);
    }
    canvas->installEventFilter(this);

    QWidget *d_main_widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout();
    d_main_widget->setLayout(layout);
    layout->addLayout(hbox);
    layout->addWidget(canvas, 1);
    layout->setMargin(0);
    layout->setSpacing(0);
    this->setWidget(d_main_widget);
    d_main_widget->installEventFilter(this);

    d_main_widget->setAutoFillBackground(true);
    d_main_widget->setBackgroundRole(QPalette::Window);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(Qt::white));
    d_main_widget->setPalette(pal);

    // Sibling transparent overlay for on-screen panel (a),(b) labels (never a child of canvas!)
    QWidget *panelLabelOverlay = new QWidget(d_main_widget);
    panelLabelOverlay->setObjectName(QStringLiteral("MultiLayerPanelLabelOverlay"));
    panelLabelOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    panelLabelOverlay->setAttribute(Qt::WA_NoSystemBackground, true);
    panelLabelOverlay->hide();
    panelLabelOverlay->installEventFilter(this);

    // Non-exported Drag-and-Drop Column Preview Overlay (sibling of canvas)
    d_drop_zone_overlay = new QWidget(d_main_widget);
    d_drop_zone_overlay->setObjectName(QStringLiteral("MultiLayerDropZoneOverlay"));
    d_drop_zone_overlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    d_drop_zone_overlay->setAttribute(Qt::WA_NoSystemBackground, true);
    d_drop_zone_overlay->hide();
    d_drop_zone_overlay->installEventFilter(this);

    // Non-exported Bottom-Right Interactive Canvas Status Pill (sibling of canvas)
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
            "  border-radius: 11px;"
            "  padding: 2px 10px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 8pt;"
            "  font-weight: 600;"
            "  min-height: 20px;"
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
    updateCanvasStatusPill();
    return g;
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
            active_graph->raise(); // raise layer on top of the layers stack
            button->setChecked(true);
        }
    }
    updateCanvasStatusPill();
}

void MultiLayer::setActiveGraph(Graph *g)
{
    if (!g || active_graph == g)
        return;

    active_graph = g;
    active_graph->setFocus();

    if (d_layers_selector)
        delete d_layers_selector;

    active_graph->raise(); // raise layer on top of the layers stack

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
    if (isVisible() && widget() && widget()->layout())
        widget()->layout()->activate();
    QRect rect = canvas->geometry();
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

        int row = i / nCols;
        if (row >= nRows)
            row = nRows - 1;
        int col = i % nCols;

        if (topHeight > maxTopH[row])
            maxTopH[row] = topHeight;
        if (bottomHeight > maxBottomH[row])
            maxBottomH[row] = bottomHeight;
        if (leftWidth > maxLeftW[col])
            maxLeftW[col] = leftWidth;
        if (rightWidth > maxRightW[col])
            maxRightW[col] = rightWidth;
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

        int y = top_margin + offsetY + row * rowsSpace + rowY[row];
        if (vert_align == VCenter)
            y += maxTopH[row] - topHeights[i];
        else if (vert_align == Bottom)
            y += (maxTopH[row] - topHeights[i]) + (maxBottomH[row] - bottomHeights[i]);

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
        bool ignoreResize = active_graph->ignoresResizeEvents();
        for (int i = 0; i < (int)graphsList.count(); i++) {
            Graph *gr = (Graph *)graphsList.at(i);
            gr->setIgnoreResizeEvents(true);
        }

        this->showNormal();
        QSize size = canvas->childrenRect().size();
        this->resize(QSize(size.width() + right_margin,
                           size.height() + bottom_margin + LayerButton::btnSize()));

        for (int i = 0; i < (int)graphsList.count(); i++) {
            Graph *gr = (Graph *)graphsList.at(i);
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
        const OleRenderedGraph emf = SciDAVisEmf::renderMultiLayer(this, nullptr);
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

    const bool hasPhysicalPage = (d_page_unit != UnitPx);
    const QRect srcRect = (hasPhysicalPage || d_page_view_mode)
            ? ((canvas && !canvas->rect().isEmpty()) ? canvas->rect() : layersRect)
            : layersRect;

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
        p.scale(double(targetSize.width()) / double(qMax(1, srcRect.width())),
                double(targetSize.height()) / double(qMax(1, srcRect.height())));
        if (d_page_bg_color.isValid() && d_page_bg_color.alpha() > 0)
            p.fillRect(QRect(QPoint(0, 0), srcRect.size()), QBrush(d_page_bg_color));
        for (int i = 0; i < (int)graphsList.count(); i++) {
            Graph *gr = (Graph *)graphsList.at(i);
            if (!gr || !gr->plotWidget())
                continue;
            Plot *myPlot = (Plot *)gr->plotWidget();
            const QPoint pos = gr->pos() - srcRect.topLeft();
            gr->exportPainter(p, false, QRect(pos, myPlot->size()));
        }
        p.setClipping(false);
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

    const bool hasPhysicalPage = (d_page_unit != UnitPx);
    const QRect srcRect = (hasPhysicalPage || d_page_view_mode)
            ? ((canvas && !canvas->rect().isEmpty()) ? canvas->rect() : layersRect)
            : layersRect;

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
        p.scale(double(targetSize.width()) / double(qMax(1, srcRect.width())),
                double(targetSize.height()) / double(qMax(1, srcRect.height())));
        if (!transparent) {
            const QColor pageBg = d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white);
            if (pageBg.alpha() > 0)
                p.fillRect(QRect(QPoint(0, 0), srcRect.size()), QBrush(pageBg));
        }
        for (int i = 0; i < (int)graphsList.count(); i++) {
            Graph *gr = (Graph *)graphsList.at(i);
            if (!gr || !gr->plotWidget())
                continue;
            Plot *myPlot = (Plot *)gr->plotWidget();
            QPoint pos = gr->pos() - srcRect.topLeft();
            gr->exportPainter(p, false, QRect(pos, myPlot->size()));
        }
        p.setClipping(false);
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
    if (quality >= 0)
        writer.setQuality(quality);
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

    QPrinter printer;
    printer.setDocName(this->name());
    printer.setCreator("ToriGraph");
    printer.setFullPage(true);
    if (res > 0)
        printer.setResolution(res);
    else if (d_page_unit != UnitPx && d_page_dpi > 0)
        printer.setResolution(d_page_dpi);

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
            if (canvas && !canvas->rect().isEmpty())
                srcRect = canvas->rect();
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

    const QRect srcRect = (d_page_unit != UnitPx && canvas && !canvas->rect().isEmpty())
            ? canvas->rect()
            : layersRect;
    QSize svgPxSize = srcRect.size();
    if (d_page_unit != UnitPx && d_base_page_px.isValid())
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

    if (rect == QRect())
        rect = layersRect;
    if (size == QSize())
        size = rect.size();
    if (keepAspect) {
        QSize scaled = rect.size();
        scaled.scale(size, Qt::KeepAspectRatio);
        size = scaled;
    }
    painter.scale((double)size.width() / (double)rect.width(),
                  (double)size.height() / (double)rect.height());

    const QColor pageBg = d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white);
    if (pageBg.alpha() > 0)
        painter.fillRect(QRect(QPoint(0, 0), rect.size()), QBrush(pageBg));

    for (int i = 0; i < (int)graphsList.count(); i++) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr || !gr->plotWidget())
            continue;
        Plot *myPlot = (Plot *)gr->plotWidget();

        QPoint pos = gr->pos() - rect.topLeft();
        gr->exportPainter(painter, false, QRect(pos, myPlot->size()));
    }
    painter.setClipping(false);
    drawPanelLabels(&painter, -rect.topLeft());
}

void MultiLayer::copyAllLayers()
{
#ifdef Q_OS_WIN
    if (SciDAVisOle::copyGraphAsOle(qobject_cast<ApplicationWindow *>(window()), this, nullptr))
        return;
#endif
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
    QImage image(layersRect.size(), QImage::Format_ARGB32);
    image.fill(d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white));
    exportPainter(image);
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

    if (d_scale_on_print) {
        int margin = (int)((1 / 2.54) * printer->logicalDpiY()); // 1 cm margins
        double scaleFactorX = (double)(pageRect.width() - 2 * margin) / (double)canvasRect.width();
        double scaleFactorY =
                (double)(pageRect.height() - 2 * margin) / (double)canvasRect.height();

        if (d_print_cropmarks) {
            cr.moveTo(QPoint(margin + int(cr.x() * scaleFactorX),
                             margin + int(cr.y() * scaleFactorY)));
            cr.setWidth(int(cr.width() * scaleFactorX));
            cr.setHeight(int(cr.height() * scaleFactorX));
        }

        for (int i = 0; i < (int)graphsList.count(); i++) {
            Graph *gr = (Graph *)graphsList.at(i);
            Plot *myPlot = gr->plotWidget();

            QPoint pos = gr->pos();
            pos = QPoint(margin + int(pos.x() * scaleFactorX),
                         margin + int(pos.y() * scaleFactorY));

            int width = int(myPlot->frameGeometry().width() * scaleFactorX);
            int height = int(myPlot->frameGeometry().height() * scaleFactorY);

            gr->print(painter, QRect(pos, QSize(width, height)));
        }
    } else {
        int x_margin = (pageRect.width() - canvasRect.width()) / 2;
        int y_margin = (pageRect.height() - canvasRect.height()) / 2;

        if (d_print_cropmarks)
            cr.moveTo(x_margin, y_margin);

        for (int i = 0; i < (int)graphsList.count(); i++) {
            Graph *gr = (Graph *)graphsList.at(i);
            Plot *myPlot = (Plot *)gr->plotWidget();

            QPoint pos = gr->pos();
            pos = QPoint(x_margin + pos.x(), y_margin + pos.y());
            gr->print(painter, QRect(pos, myPlot->size()));
        }
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
    connect(g, &Graph::modifiedGraph, this, [this]() {
        if (d_panel_labels_enabled && widget()) {
            if (QWidget *ov = widget()->findChild<QWidget *>(
                        QStringLiteral("MultiLayerPanelLabelOverlay"))) {
                ov->update();
            }
        }
    });
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

bool MultiLayer::eventFilter(QObject *object, QEvent *e)
{
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
    } else if ((e->type() == QEvent::Resize || e->type() == QEvent::Show)
               && object == (QObject *)widget()) {
        repositionCanvasStatusPill();
    } else if (e->type() == QEvent::Paint && object
               && object->objectName() == QLatin1String("MultiLayerPanelLabelOverlay")) {
        if (d_panel_labels_enabled) {
            QPainter p(static_cast<QWidget *>(object));
            p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);
            drawPanelLabels(&p, QPoint(0, 0));
        }
        return true;
    } else if (e->type() == QEvent::Paint && object == (QObject *)d_drop_zone_overlay) {
        if (d_drop_zone_overlay && d_drop_zone_overlay->isVisible()) {
            QPainter p(d_drop_zone_overlay);
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
        }
        return true;
    } else if (e->type() == QEvent::Paint && object == (QObject *)canvas && d_page_view_mode) {
        const bool ret = MyWidget::eventFilter(object, e);
        QPainter p(canvas);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(QColor("#B8BEC6"), 1, Qt::SolidLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(canvas->rect().adjusted(0, 0, -1, -1));
        return ret;
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
            d_quick_columns_btn->setText(tr("Columns") + QStringLiteral(" \u25BE"));
            d_quick_columns_btn->setToolTip(tr("Toggle Plotted Columns on Active Layer"));
        }
        for (int i = 0; i < buttonsList.count(); ++i) {
            if (LayerButton *btn = qobject_cast<LayerButton *>(buttonsList.at(i)))
                btn->setToolTip(tr("Layer %1").arg(i + 1));
        }
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
    QApplication::setOverrideCursor(Qt::WaitCursor);

    bool resize = false;
    QPoint aux;
    QSize intSize;
    Graph *resize_graph = 0;
    // Get the position of the mouse
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    int xMouse = e->position().x();
    int yMouse = e->position().y();
#else
    int xMouse = e->x();
    int yMouse = e->y();
#endif
    if (d_layers_selector != nullptr) {
        for (int i = 0; i < (int)graphsList.count(); i++) {
            Graph *gr = (Graph *)graphsList.at(i);
            intSize = gr->plotWidget()->size();
            aux = gr->pos();
            if (xMouse > aux.x() && xMouse < (aux.x() + intSize.width())) {
                if (yMouse > aux.y() && yMouse < (aux.y() + intSize.height())) {
                    resize_graph = gr;
                    resize = true;
                }
            }
        }
    }
    if (!resize && (e->modifiers() & Qt::ControlModifier)) {
        const int dy = e->angleDelta().y();
        if (dy != 0) {
            const double factor = (dy > 0) ? 1.1 : (1.0 / 1.1);
            setPageZoom(d_page_zoom * factor);
            e->accept();
        }
        QApplication::restoreOverrideCursor();
        return;
    }
    if (resize && d_layers_selector != nullptr
        && (e->modifiers() == Qt::AltModifier || e->modifiers() == Qt::ControlModifier
            || e->modifiers() == Qt::ShiftModifier)) {
        intSize = resize_graph->plotWidget()->size();
        int delta = (e->angleDelta().y() > 0) ? 5 : ((e->angleDelta().y() < 0) ? -5 : 0);
        int deltaW = delta;
        int deltaH = d_keep_aspect_ratio && d_target_aspect_ratio > 0.05
                ? qRound(delta / d_target_aspect_ratio)
                : delta;
        if (delta != 0 && intSize.width() + deltaW >= 100 && intSize.height() + deltaH >= 100) {
            intSize.rwidth() += deltaW;
            intSize.rheight() += deltaH;
        }

        aux = resize_graph->pos();
        resize_graph->setGeometry(QRect(QPoint(aux.x(), aux.y()), intSize));
        resize_graph->plotWidget()->resize(intSize);
        resize_graph->plotWidget()->updateLayout();

        emit modifiedPlot();
    }
    QApplication::restoreOverrideCursor();
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
    s += "WindowLabel\t" + windowLabel() + "\t" + QString::number(captionPolicy()) + "\n";
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
        Graph *srcGr = it.value().first.data();
        const int childIdx = graphsList.indexOf(insetGr);
        const int srcIdx = graphsList.indexOf(srcGr);
        if (childIdx >= 0 && srcIdx >= 0) {
            const QRectF rel = it.value().second;
            s += QString("<ToriInsetSpec child=\"%1\" source=\"%2\" x=\"%3\" y=\"%4\" w=\"%5\" h=\"%6\"/>\n")
                         .arg(childIdx)
                         .arg(srcIdx)
                         .arg(rel.x(), 0, 'g', 12)
                         .arg(rel.y(), 0, 'g', 12)
                         .arg(rel.width(), 0, 'g', 12)
                         .arg(rel.height(), 0, 'g', 12);
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
    // iterate backwards, so layers on top are preferred for selection
    QList<QWidget *>::iterator i = graphsList.end();
    while (i != graphsList.begin()) {
        --i;
        QRect igeo = (*i)->frameGeometry();
        igeo.adjust(-margin, -margin, margin, margin);
        if (igeo.contains(pos)) {
            if (e->modifiers() & Qt::ShiftModifier) {
                if (d_layers_selector)
                    d_layers_selector->add(*i);
                else {
                    d_layers_selector = new SelectionMoveResizer(*i);
                    connect(d_layers_selector, SIGNAL(targetsChanged()), this,
                            SIGNAL(modifiedPlot()));
                }
            } else {
                setActiveGraph((Graph *)(*i));
                active_graph->raise();
                if (!d_layers_selector) {
                    d_layers_selector = new SelectionMoveResizer(*i);
                    connect(d_layers_selector, SIGNAL(targetsChanged()), this,
                            SIGNAL(modifiedPlot()));
                }
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
                graphsList.removeLast();
                g->setParent(nullptr);
                g->close();
            }
        }
        graphs = n;
        if (!graphs) {
            active_graph = 0;
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
    if (ml->canvasWidget() && canvas)
        canvas->resize(ml->canvasWidget()->size());

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
            Graph *srcSource = ml->d_inset_specs[srcG].first.data();
            const int sIdx = srcGraphs.indexOf(srcSource);
            Graph *dstSource = (sIdx >= 0) ? (Graph *)graphsList.value(sIdx, nullptr) : nullptr;
            if (dstSource)
                d_inset_specs[dstG] = qMakePair(QPointer<Graph>(dstSource), ml->d_inset_specs[srcG].second);
        }
    }
    syncChildOverlayAndInsetGeometries();
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
            const QRect pCanvas = pPlot->plotLayout()->canvasRect();
            if (pCanvas.width() >= 20 && pCanvas.height() >= 20) {
                const QRect targetInner = pCanvas.translated(parentLayer->pos());
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
        } else if (d_inset_specs.contains(gr)) {
            Graph *srcLayer = d_inset_specs[gr].first.data();
            const QRectF rel = d_inset_specs[gr].second;
            if (srcLayer) {
                const QRect srcGeo = srcLayer->geometry();
                const int ix = srcGeo.x() + qRound(rel.x() * srcGeo.width());
                const int iy = srcGeo.y() + qRound(rel.y() * srcGeo.height());
                const int iw = qMax(40, qRound(rel.width() * srcGeo.width()));
                const int ih = qMax(40, qRound(rel.height() * srcGeo.height()));
                const QRect targetGeo(ix, iy, iw, ih);
                if (gr->geometry() != targetGeo) {
                    gr->setGeometry(targetGeo);
                    gr->plotWidget()->resize(targetGeo.size());
                    gr->plotWidget()->updateLayout();
                }
                gr->raise();
            }
        }
    }
    if (widget()) {
        if (QWidget *ov = widget()->findChild<QWidget *>(
                    QStringLiteral("MultiLayerPanelLabelOverlay"))) {
            ov->setVisible(d_panel_labels_enabled);
            if (d_panel_labels_enabled)
                ov->update();
        }
    }
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
        pushUndoSnapshot(tr("Link Axes Scales"), snap);
    });
    menu.addSeparator();
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("layer-linked"), 18, false),
                   tr("Add Linked Top-X / Right-Y Layer"), this, [this]() {
        const QString snap = captureUndoSnapshot();
        addLinkedLayer(activeGraph(), true, true, QString("x"), QString("y"));
        pushUndoSnapshot(tr("Add Linked Layer"), snap);
    });
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("layer-inset"), 18, false),
                   tr("Add Inset Layer"), this, [this]() {
        const QString snap = captureUndoSnapshot();
        addInsetLayer(activeGraph());
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
                if (graphs != prevGraphs && graphs > 0)
                    pushUndoSnapshot(tr("Remove Layer"), snap);
            });
    actRemove->setEnabled(graphs > 0);
    menu.exec(globalPos);
}

void MultiLayer::setPageViewMode(bool enabled)
{
    d_page_view_mode = enabled;
    setPageBackgroundColor(d_page_view_mode && !d_page_bg_color.isValid() ? QColor("#FFFFFF")
                                                                          : d_page_bg_color);
    if (canvas)
        canvas->update();
    emit modifiedPlot();
}

bool MultiLayer::pageViewMode() const
{
    return d_page_view_mode;
}

void MultiLayer::setPageBackgroundColor(const QColor &color)
{
    d_page_bg_color = color.isValid() ? color : QColor("#FFFFFF");
    if (canvas) {
        QPalette cpal = canvas->palette();
        cpal.setColor(QPalette::Window, d_page_bg_color);
        canvas->setPalette(cpal);
        canvas->setAutoFillBackground(true);
        canvas->update();
    }
    if (widget()) {
        QPalette wpal = widget()->palette();
        wpal.setColor(QPalette::Window, d_page_view_mode ? QColor("#EAEEF3") : d_page_bg_color);
        widget()->setPalette(wpal);
    }
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

    QVector<bool> savedIgnore(graphsList.count(), false);
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr) {
            savedIgnore[i] = gr->ignoresResizeEvents();
            gr->setIgnoreResizeEvents(true);
        }
    }

    lastSize = newCanvasSize;
    if (canvas)
        canvas->resize(newCanvasSize);
    this->resize(newCanvasSize.width(), newCanvasSize.height() + LayerButton::btnSize() + 4);

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
    syncChildOverlayAndInsetGeometries();
    lastSize = canvas ? canvas->size() : newCanvasSize;
    emit modifiedPlot();
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
    if (canvas)
        canvas->resize(targetCanvasSize);
    this->resize(targetCanvasSize.width(), targetCanvasSize.height() + LayerButton::btnSize() + 4);

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr)
            gr->setIgnoreResizeEvents(savedIgnore[i]);
    }

    if (graphs > 0)
        fitLayersToPage();
    lastSize = canvas ? canvas->size() : targetCanvasSize;
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
    lastSize = tightSize;
    if (canvas)
        canvas->resize(tightSize);
    this->resize(tightSize.width(), tightSize.height() + LayerButton::btnSize() + 4);
    lastSize = canvas ? canvas->size() : tightSize;

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (gr)
            gr->setIgnoreResizeEvents(savedIgnore[i]);
    }

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
    setActiveGraph(parentLayer);
    child->raise();
    emit modifiedPlot();
    return child;
}

Graph *MultiLayer::addInsetLayer(Graph *sourceLayer, const QRectF &roiDataRect)
{
    if (!sourceLayer)
        sourceLayer = active_graph ? active_graph : layer(1);
    if (!sourceLayer)
        return addLayer();

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
                bool hasY = false;
                auto scanCurveY = [&](QwtPlotCurve *cv) {
                    if (!cv)
                        return;
                    const int n = cv->dataSize();
                    for (int k = 0; k < n; ++k) {
                        const double xv = cv->x(k);
                        const double yv = cv->y(k);
                        if (xv >= xMin && xv <= xMax && std::isfinite(yv)) {
                            if (!hasY) {
                                yMin = yMax = yv;
                                hasY = true;
                            } else {
                                if (yv < yMin)
                                    yMin = yv;
                                if (yv > yMax)
                                    yMax = yv;
                            }
                        }
                    }
                };
                const int selIdx = sourceLayer->selectedCurveIndex();
                if (selIdx >= 0 && selIdx < sourceLayer->curves()) {
                    scanCurveY(sourceLayer->curve(selIdx));
                } else {
                    for (int k = 0; k < sourceLayer->curves(); ++k)
                        scanCurveY(sourceLayer->curve(k));
                }
                if (hasY) {
                    const double padY = (yMax > yMin) ? 0.06 * (yMax - yMin)
                                                      : (qAbs(yMax) * 0.05 + 1e-6);
                    effectiveRoi = QRectF(QPointF(xMin, yMin - padY), QPointF(xMax, yMax + padY));
                } else {
                    effectiveRoi = QRectF(xMin, 0.0, xMax - xMin, 0.0);
                }
            }
        }
    }

    const QRect srcGeo = sourceLayer->geometry();
    const int ix = srcGeo.x() + qRound(0.54 * srcGeo.width());
    const int iy = srcGeo.y() + qRound(0.08 * srcGeo.height());
    const int iw = qMax(40, qRound(0.38 * srcGeo.width()));
    const int ih = qMax(40, qRound(0.38 * srcGeo.height()));

    Graph *inset = addLayer(ix, iy, iw, ih);
    if (!inset)
        return nullptr;

    d_inset_specs[inset] = qMakePair(QPointer<Graph>(sourceLayer), QRectF(0.54, 0.08, 0.38, 0.38));
    inset->setIgnoreResizeEvents(true);
    inset->removeLegend();
    inset->setTitle("");
    inset->setBackgroundColor(QColor("#FFFFFF"));
    inset->setCanvasBackground(QColor("#FFFFFF"));
    if (inset->plotWidget())
        inset->plotWidget()->setMargin(4);

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
            }
        } else {
            const int n = srcCurve->dataSize();
            if (n > 0) {
                PlotCurve *pc = new PlotCurve(srcCurve->title().text());
                pc->setType(cType);
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
                inset->insertPlotItem(pc, cType);
            }
        }
    }

    inset->removeLegend();
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        inset->setAxisTitle(a, QString());
        inset->setAxisFont(a, QFont("Arial", 9, QFont::Normal, false));
        inset->setAxisTitleFont(a, QFont("Arial", 9, QFont::Normal, false));
    }
    inset->applyFramePreset(Graph::FrameBoxInwardTicks);

    if (!effectiveRoi.isNull()
        && (qAbs(effectiveRoi.width()) > 1e-15 || qAbs(effectiveRoi.height()) > 1e-15)) {
        inset->enableAutoscaling(false);
        if (qAbs(effectiveRoi.width()) > 1e-15) {
            inset->setScale(QwtPlot::xBottom, qMin(effectiveRoi.left(), effectiveRoi.right()),
                            qMax(effectiveRoi.left(), effectiveRoi.right()));
        }
        if (qAbs(effectiveRoi.height()) > 1e-15) {
            inset->setScale(QwtPlot::yLeft, qMin(effectiveRoi.top(), effectiveRoi.bottom()),
                            qMax(effectiveRoi.top(), effectiveRoi.bottom()));
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
    inset->raise();
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

    setMargins(marginPx, marginPx, marginPx, marginPx);
    setPageZoom(1.0);
    setPageSizePhysical(pageW, pageH, UnitMm, targetDpi);
    d_keep_aspect_ratio = true;
    d_target_aspect_ratio = 1.0;

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
            gr->setBackgroundColor(QColor(Qt::white));
            gr->setCanvasBackground(QColor(Qt::white));
        }

        gr->setAxesLinewidth(axisLw);
        gr->changeTicksLength(minTickLen, majTickLen);

        QwtPlot *plot = gr->plotWidget();
        QwtText pTitle = plot->title();
        QString cleanPlotTitle = pTitle.text();
        cleanPlotTitle.remove(kBoldTagRx);
        pTitle.setText(cleanPlotTitle);
        pTitle.setFont(titleFnt);
        plot->setTitle(pTitle);

        if (preset == StyleISIJJournal85mm) {
            gr->setAxisUnitFormat(Column::UnitFormat::ISIJSlash);
            gr->refreshAutoAxisTitlesFromColumns();
        }

        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            QFont useTick = isInset
                    ? QFont("Arial", qMax(6, tickFnt.pointSize() - 2), QFont::Normal, false)
                    : tickFnt;
            useTick.setHintingPreference(QFont::PreferNoHinting);
            QFont useTitle = isInset
                    ? QFont("Arial", qMax(7, titleFnt.pointSize() - 2), QFont::Normal, false)
                    : titleFnt;
            useTitle.setHintingPreference(QFont::PreferNoHinting);
            gr->setAxisFont(a, useTick);
            gr->setAxisTitleFont(a, useTitle);
            QString rawTitle = gr->axisTitle(a);
            if (rawTitle.contains(kBoldTagRx)) {
                const bool wasAuto = gr->isAxisTitleAuto(a);
                rawTitle.remove(kBoldTagRx);
                gr->setAxisTitle(a, rawTitle);
                gr->setAxisTitleAuto(a, wasAuto);
            }
        }

        for (QwtPlotItem *it : plot->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
                static_cast<Spectrogram *>(it)->enforceColorBarArialFont();
        }

        gr->setTextMarkerDefaults(legFrame, legFnt, QColor(Qt::black), legBg);
        const QVector<int> txtKeys = gr->textMarkerKeys();
        for (int k = 0; k < txtKeys.size(); ++k) {
            Legend *mrk = gr->textMarker(txtKeys[k]);
            if (!mrk)
                continue;
            const bool isCurveLegend = (mrk == gr->legend() || mrk->text().contains("\\c{")
                                        || mrk->text().contains("\\p{"));
            mrk->setFont(legFnt);
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
        if ((preset == StyleISIJJournal85mm || preset == StylePresentationSlide28pt)
            && !isLinkedOverlay && !gr->isTernaryPlot() && !gr->isPiePlot()) {
            plot->updateAxes();
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                if (gr->hasColorScaleOnAxis(a))
                    continue;
                if ((a == QwtPlot::xTop || a == QwtPlot::yRight)
                    && !gr->plotWidget()->hasVisibleItemOnAxis(a)) {
                    continue;
                }
                const QwtScaleEngine *se = plot->axisScaleEngine(a);
                const QwtScaleDiv *sd = plot->axisScaleDiv(a);
                if (!se || !sd)
                    continue;
                const bool isLog = Plot::isLog10ScaleEngine(se);
                const bool isInv = se->testAttribute(QwtScaleEngine::Inverted);
                const int targetMinTicks = isLog ? 8 : 1;
                const int maxMinInt = isLog ? 9 : 2;
                const double lo = qMin(sd->lowerBound(), sd->upperBound());
                const double hi = qMax(sd->lowerBound(), sd->upperBound());
                const QwtValueList majTicks = sd->ticks(QwtScaleDiv::MajorTick);
                plot->setAxisMaxMinor(a, targetMinTicks);

                if (gr->axisBreak(a).enabled) {
                    gr->setAxisBreak(a, gr->axisBreak(a));
                    continue;
                }

                if (!(hi > lo) || !std::isfinite(lo) || !std::isfinite(hi))
                    continue;

                int maj = plot->axisMaxMajor(a);
                if (preset == StylePresentationSlide28pt)
                    maj = qMin(maj, 5);
                double step = gr->axisStep(a);
                if (step <= 0.0 && !isLog && majTicks.size() >= 2) {
                    if (!(preset == StylePresentationSlide28pt && majTicks.size() > 6))
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

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr || !gr->plotWidget())
            continue;
        const bool isInset = d_inset_specs.contains(gr);
        const bool isLinkedOverlay = (gr->linkedParentLayer() != nullptr);
        if (!isInset && !isLinkedOverlay && gr->hasLegend() && gr->isLegendVisible()) {
            const QVariant ancProp = gr->plotWidget()->property("_toriLegendAnchor");
            if (ancProp.isValid()) {
                gr->placeLegendAnchor(static_cast<Graph::LegendAnchor>(ancProp.toInt()));
            } else if (legendWasDefaultTopLeft[i]) {
                gr->placeLegendAnchor(Graph::LegendAutoAvoidData);
            }
        }
        gr->plotWidget()->updateLayout();
        gr->plotWidget()->replot();
    }
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
        if (d_page_unit != UnitPx || d_page_view_mode || qAbs(d_page_zoom - 1.0) > 1e-4) {
            const QSize targetCanvasSize(qMax(80, qRound(d_base_page_px.width() * d_page_zoom)),
                                         qMax(80, qRound(d_base_page_px.height() * d_page_zoom)));
            lastSize = targetCanvasSize;
            if (canvas)
                canvas->resize(targetCanvasSize);
        }
        if (canvas)
            canvas->update();
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

    if (trimmed.startsWith("<ToriInsetSpec")) {
        const QMap<QString, QString> a = parseAttrs(trimmed);
        PendingInsetSpec ps;
        ps.childIdx = a.value("child", "-1").toInt();
        ps.sourceIdx = a.contains("source") ? a.value("source", "-1").toInt()
                                            : a.value("src", "-1").toInt();
        ps.relRect = QRectF(a.value("x", "0.54").toDouble(),
                            a.value("y", "0.08").toDouble(),
                            a.value("w", "0.38").toDouble(),
                            a.value("h", "0.38").toDouble());
        if (ps.childIdx >= 0 && ps.sourceIdx >= 0) {
            d_pending_inset_specs.append(ps);
            if (ps.childIdx < graphsList.count() && ps.sourceIdx < graphsList.count()) {
                Graph *cGr = (Graph *)graphsList.at(ps.childIdx);
                Graph *sGr = (Graph *)graphsList.at(ps.sourceIdx);
                if (cGr && sGr)
                    d_inset_specs[cGr] = qMakePair(QPointer<Graph>(sGr), ps.relRect);
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
        if (widget()) {
            if (QWidget *ov = widget()->findChild<QWidget *>(
                        QStringLiteral("MultiLayerPanelLabelOverlay"))) {
                ov->setVisible(d_panel_labels_enabled);
                ov->update();
            }
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
                d_inset_specs[cGr] = qMakePair(QPointer<Graph>(sGr), ps.relRect);
                cGr->setIgnoreResizeEvents(true);
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
                gr->raise();
            }
            gr->clearPendingLinkedParentIndex();
        }
    }

    syncChildOverlayAndInsetGeometries();
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
    const QString statusText = QStringLiteral("%1 %2 %3 %4 %5 %6 DPI %5 %7 %5 %8%")
                                       .arg(QString::number(wDisp, 'f', decimals), times,
                                            QString::number(hDisp, 'f', decimals), unitStr, sep,
                                            QString::number(d_page_dpi), fontFam,
                                            QString::number(zoomPct));

    d_canvas_status_pill->setText(statusText);
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
            if (d_panel_labels_enabled) {
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
    d_canvas_status_pill->adjustSize();
    const QSize pillSz =
            d_canvas_status_pill->sizeHint().expandedTo(d_canvas_status_pill->minimumSizeHint());
    const int pw = qMin(pillSz.width(), qMax(80, refSz.width() - 16));
    const int ph = qMax(22, pillSz.height());
    const int x = qMax(8, refSz.width() - pw - 10);
    const int y = qMax(LayerButton::btnSize() + 8, refSz.height() - ph - 8);
    d_canvas_status_pill->setGeometry(x, y, pw, ph);
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
            setWindowLabel(f.at(1));
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
        if (canvas->size() != targetCanvasSz)
            canvas->resize(targetCanvasSz);
        if (!restoreGeometry && !isMaximized() && parentWidget()) {
            const QSize wantWin(targetCanvasSz.width(),
                                targetCanvasSz.height() + LayerButton::btnSize() + 4);
            if (d_page_unit != UnitPx || d_page_view_mode || qAbs(d_page_zoom - 1.0) > 1e-4) {
                this->resize(wantWin);
            } else {
                const QSize unitPxWin =
                        (savedNormalWinSz.width() >= 120 && savedNormalWinSz.height() >= 120)
                        ? savedNormalWinSz
                        : wantWin;
                if (preservedWindowGeo.isValid() && this->pos() != preservedWindowGeo.topLeft())
                    this->move(preservedWindowGeo.topLeft());
                if (this->size() != unitPxWin)
                    this->resize(unitPxWin);
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

    if (widget()) {
        if (QWidget *ov = widget()->findChild<QWidget *>(
                    QStringLiteral("MultiLayerPanelLabelOverlay"))) {
            if (canvas)
                ov->setGeometry(canvas->geometry());
            ov->setVisible(d_panel_labels_enabled);
            if (d_panel_labels_enabled)
                ov->raise();
            ov->update();
        }
    }
    repositionCanvasStatusPill();
    emit modifiedPlot();
}

void MultiLayer::drawPanelLabels(QPainter *painter, const QPoint &originOffset) const
{
    if (!d_panel_labels_enabled || !painter || graphsList.isEmpty())
        return;

    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    int panelIdx = 0;
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (!gr || !gr->plotWidget())
            continue;
        if (gr->linkedParentLayer() || d_inset_specs.contains(gr))
            continue;

        Plot *plot = gr->plotWidget();
        plot->updateLayout();
        const QRect innerRect = plot->plotLayout()->canvasRect().translated(gr->pos() + originOffset);
        if (innerRect.width() < 20 || innerRect.height() < 20) {
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
        painter->setFont(fnt);
        painter->setPen(QColor(Qt::black));

        const QString text = formatPanelLabel(d_panel_label_pattern, panelIdx);
        const QFontMetrics fm(fnt);

        int tx = innerRect.left() + 8;
        int ty = innerRect.top() + 6 + fm.ascent();
        if (d_panel_label_placement == PanelOutsideTopLeft) {
            tx = qMax(originOffset.x() + 3, gr->pos().x() + originOffset.x() + 2);
            ty = qMax(originOffset.y() + fm.ascent() + 2, innerRect.top() - 5);
        }
        painter->drawText(QPoint(tx, ty), text);
        ++panelIdx;
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
    const int panelTopExtra =
            (d_panel_labels_enabled && d_panel_label_placement == PanelOutsideTopLeft) ? 20 : 0;

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
        if (nRows < 2 && nCols < 2)
            return;
        const int capX = qBound(3, side / 36, 6);
        const int capY = qBound(3, side / 28, 6);
        for (Graph *gr : primaryGraphs) {
            Plot *pl = gr ? gr->plotWidget() : nullptr;
            if (!pl || gr->isTernaryPlot() || gr->isPiePlot())
                continue;
            const int axesToCap[2] = { QwtPlot::xBottom, QwtPlot::yLeft };
            const int caps[2] = { capX, capY };
            bool changedAny = false;
            for (int ai = 0; ai < 2; ++ai) {
                const int a = axesToCap[ai];
                const int cap = caps[ai];
                if (gr->hasColorScaleOnAxis(a))
                    continue;
                if (pl->axisAutoScale(a)) {
                    if (pl->axisMaxMajor(a) > cap) {
                        pl->setAxisMaxMajor(a, cap);
                        changedAny = true;
                    }
                    continue;
                }
                if (!gr->axisBreak(a).enabled && gr->axisStep(a) <= 0.0) {
                    const QwtScaleEngine *se = pl->axisScaleEngine(a);
                    const QwtScaleDiv *sd = pl->axisScaleDiv(a);
                    if (se && sd && !Plot::isLog10ScaleEngine(se)
                        && (sd->ticks(QwtScaleDiv::MajorTick).size() > cap + 1
                            || pl->axisMaxMajor(a) > cap)) {
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

    for (int i = 0; i < nPrimary; ++i) {
        Graph *gr = primaryGraphs.at(i);
        if (!gr || !gr->plotWidget() || !gr->legend() || !gr->isLegendVisible())
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
    tree->setMinimumSize(250, 220);
    tree->setMaximumHeight(320);

    auto roleBadge = [](int desig) -> QString {
        switch (desig) {
        case SciDAVis::X:
            return QStringLiteral("[X]");
        case SciDAVis::Y:
            return QStringLiteral("[Y]");
        case SciDAVis::Z:
            return QStringLiteral("[Z]");
        case SciDAVis::xErr:
            return QStringLiteral("[xEr]");
        case SciDAVis::yErr:
            return QStringLiteral("[yEr]");
        default:
            return QObject::tr("[None]");
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
            const QString cmt = t->colComment(col).trimmed();

            auto *cItem = new QTreeWidgetItem(tItem);
            QString disp = QStringLiteral("%1 %2").arg(roleBadge(desig), colShort);
            if (!cmt.isEmpty() && cmt != colShort)
                disp += QStringLiteral(" — %1").arg(cmt.left(28));
            cItem->setText(0, disp);
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
    Graph *targetGr = nullptr;
    for (int i = graphsList.count() - 1; i >= 0; --i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (!gr || !gr->isVisible())
            continue;
        if (gr->geometry().contains(canvasPt)) {
            targetGr = gr->linkedParentLayer() ? gr->linkedParentLayer() : gr;
            break;
        }
    }
    if (!targetGr)
        targetGr = activeGraph() ? activeGraph() : layer(1);

    const QRect zoneRect = targetGr ? targetGr->geometry()
                                    : (canvas ? canvas->rect() : rect());
    bool rightYZone = (canvasPt.x() >= zoneRect.right() - 44);
    if (!rightYZone && targetGr && targetGr->plotWidget()) {
        if (QwtScaleWidget *swRight = targetGr->plotWidget()->axisWidget(QwtPlot::yRight)) {
            if (swRight->isVisible()) {
                const QPoint swPt = swRight->mapFromGlobal(mapToGlobal(e->pos()));
                if (swRight->rect().adjusted(-12, 0, 12, 0).contains(swPt))
                    rightYZone = true;
            }
        }
    }

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
    Graph *targetGr = nullptr;
    for (int i = graphsList.count() - 1; i >= 0; --i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (!gr || !gr->isVisible())
            continue;
        if (gr->geometry().contains(canvasPt)) {
            targetGr = gr->linkedParentLayer() ? gr->linkedParentLayer() : gr;
            break;
        }
    }
    if (!targetGr)
        targetGr = activeGraph() ? activeGraph() : layer(1);

    const QRect zoneRect = targetGr ? targetGr->geometry()
                                    : (canvas ? canvas->rect() : rect());
    bool rightYZone = (canvasPt.x() >= zoneRect.right() - 44);
    if (!rightYZone && targetGr && targetGr->plotWidget()) {
        if (QwtScaleWidget *swRight = targetGr->plotWidget()->axisWidget(QwtPlot::yRight)) {
            if (swRight->isVisible()) {
                const QPoint swPt = swRight->mapFromGlobal(mapToGlobal(e->pos()));
                if (swRight->rect().adjusted(-12, 0, 12, 0).contains(swPt))
                    rightYZone = true;
            }
        }
    }

    const int targetAxisId = rightYZone ? QwtPlot::yRight : QwtPlot::yLeft;
    const bool replaceExisting = (e->keyboardModifiers() & Qt::AltModifier);
    if (ApplicationWindow *app = findMultiLayerApp(this)) {
        if (app->handleColumnMimeDrop(e->mimeData(), this, targetGr, targetAxisId, replaceExisting))
            e->acceptProposedAction();
    }
}


