/***************************************************************************
    File                 : MultiLayer.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Multi layer widget (core, layer lifecycle, events, DnD, save/copy)

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
QString escapeXmlAttr(QString val)
{
    val.replace(QLatin1String("&"), QLatin1String("&amp;"));
    val.replace(QLatin1String("\""), QLatin1String("&quot;"));
    val.replace(QLatin1String("<"), QLatin1String("&lt;"));
    val.replace(QLatin1String(">"), QLatin1String("&gt;"));
    val.replace(QLatin1String("\r"), QLatin1String("&#13;"));
    val.replace(QLatin1String("\n"), QLatin1String("&#10;"));
    return val;
}
} // namespace

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
    active_graph = nullptr;
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
    d_layer_pill_frame->installEventFilter(this);
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
        qDeleteAll(statusMenu->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly));
        statusMenu->clear();
        QAction *actLockAspect = statusMenu->addAction(tr("Lock Aspect Ratio"));
        actLockAspect->setCheckable(true);
        actLockAspect->setChecked(d_keep_aspect_ratio);
        connect(actLockAspect, &QAction::toggled, this, [this](bool checked) {
            const QString snap = captureUndoSnapshot();
            if (checked) {
                double ratio = 1.0;
                Graph *ag = active_graph ? active_graph.data() : layer(1);
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
        const QVector<GraphStylePresetSpec> allPresets = MultiLayer::listStylePresets();
        auto presetDisplayLabel = [this](const GraphStylePresetSpec &sp) -> QString {
            if (sp.id == QLatin1String("isij_85mm"))
                return tr("ISIJ 85mm");
            if (sp.id == QLatin1String("lab_slide_83mm"))
                return tr("Lab Slide (18 pt, 83.3 mm)");
            if (sp.id == QLatin1String("journal_2col_175mm"))
                return tr("Journal 2-Column (175 mm)");
            if (sp.id == QLatin1String("slide_28pt"))
                return tr("Large Slide (28 pt, 185 mm)");
            return sp.name;
        };
        GraphStylePresetSpec legacySlidePreset;
        bool hasLegacySlide = false;
        for (const GraphStylePresetSpec &sp : allPresets) {
            if (sp.id == QLatin1String("slide_28pt")) {
                legacySlidePreset = sp;
                hasLegacySlide = true;
                continue;
            }
            const QString presetId = sp.id;
            const QString iconName = (sp.id == QLatin1String("lab_slide_83mm"))
                    ? QStringLiteral("preset-slide")
                    : QStringLiteral("preset-isij");
            QAction *act = statusMenu->addAction(
                    IconLoader::duotoneIcon(iconName, 18, false), presetDisplayLabel(sp), this,
                    [this, presetId]() { applyStylePresetById(presetId); });
            act->setCheckable(true);
            act->setChecked(d_applied_style_preset_id.compare(presetId, Qt::CaseInsensitive) == 0);
        }
        if (hasLegacySlide) {
            QMenu *moreMenu = statusMenu->addMenu(
                    IconLoader::duotoneIcon(QStringLiteral("preset-slide"), 18, false),
                    tr("More / Legacy Presets"));
            const QString legacyId = legacySlidePreset.id;
            QAction *legacyAct = moreMenu->addAction(
                    IconLoader::duotoneIcon(QStringLiteral("preset-slide"), 18, false),
                    presetDisplayLabel(legacySlidePreset), this,
                    [this, legacyId]() { applyStylePresetById(legacyId); });
            legacyAct->setCheckable(true);
            legacyAct->setChecked(
                    d_applied_style_preset_id.compare(legacyId, Qt::CaseInsensitive) == 0);
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

void MultiLayer::clearLayerSelector()
{
    if (!d_layers_selector)
        return;
    SelectionMoveResizer *old = d_layers_selector.data();
    d_layers_selector = nullptr;
    const bool hadFocus = old->hasFocus();
    old->blockSignals(true);
    old->disconnect();
    if (QWidget *pw = old->parentWidget())
        pw->removeEventFilter(old);
    old->hide();
    old->setParent(nullptr);
    if (hadFocus && active_graph) {
        if (active_graph->plotWidget() && active_graph->plotWidget()->canvas())
            active_graph->plotWidget()->canvas()->setFocus();
        else
            active_graph->setFocus();
    }
    old->deleteLater();
}

void MultiLayer::activateGraph(LayerButton *button)
{
    clearLayerSelector();
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
    if (!g || !graphsList.contains(g))
        return;
    clearLayerSelector();
    if (active_graph == g) {
        enforceLayerZOrder();
        return;
    }

    active_graph = g;
    active_graph->setFocus();

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

void MultiLayer::clearCtrlFontGestureBaseline()
{
    d_ctrl_font_gesture.active = false;
    d_ctrl_font_gesture.baseCanvasH = 0;
    d_ctrl_font_gesture.graphs.clear();
    if (d_ctrl_font_gesture_timer)
        d_ctrl_font_gesture_timer->stop();
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
    if (unlockedByShift)
        d_keep_aspect_ratio = true;
    if (unlockedByShift || scaleFontsWithCtrl) {
        if (ApplicationWindow *app = findMultiLayerApp(this)) {
            if (GraphInspectorDock *dock = app->graphInspectorDock()) {
                if (dock->targetMultiLayer() == this)
                    dock->refreshAll();
            }
        }
    }
}

void MultiLayer::resizeLayers(const QSize &size, const QSize &oldSize, bool scaleFonts)
{
    Q_UNUSED(oldSize);
    if (graphs <= 0) {
        clearCtrlFontGestureBaseline();
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
        if (scaleFonts) {
            if (!d_ctrl_font_gesture.active) {
                d_ctrl_font_gesture.active = true;
                d_ctrl_font_gesture.baseCanvasH = prevCanvasH;
                d_ctrl_font_gesture.graphs.clear();
                for (int i = 0; i < graphsList.count(); ++i) {
                    Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
                    if (!gr || gr->ignoresResizeEvents() || !gr->plotWidget())
                        continue;
                    CtrlFontGestureBaseline::GraphFonts gf;
                    gf.graph = gr;
                    gf.titleFont = gr->plotWidget()->title().font();
                    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                        gf.axisFonts[a] = gr->axisFont(a);
                        gf.axisTitleFonts[a] = gr->axisTitleFont(a);
                    }
                    for (int tid : gr->textMarkerKeys()) {
                        if (Legend *mrk = gr->textMarker(tid))
                            gf.textMarkerFonts.insert(tid, mrk->font());
                    }
                    d_ctrl_font_gesture.graphs.append(gf);
                }
            }
            if (!d_ctrl_font_gesture_timer) {
                d_ctrl_font_gesture_timer = new QTimer(this);
                d_ctrl_font_gesture_timer->setSingleShot(true);
                connect(d_ctrl_font_gesture_timer, &QTimer::timeout, this,
                        [this]() { clearCtrlFontGestureBaseline(); });
            }
            d_ctrl_font_gesture_timer->start(450);
        } else {
            clearCtrlFontGestureBaseline();
        }

        arrangeLayers(false, false);
        if (scaleFonts && l_canvas_height > 0 && d_ctrl_font_gesture.active
            && d_ctrl_font_gesture.baseCanvasH > 0) {
            const double ratio =
                    double(l_canvas_height) / double(d_ctrl_font_gesture.baseCanvasH);
            if (std::isfinite(ratio) && ratio > 0.0) {
                auto scaledBaselineFont = [ratio](QFont fnt) -> QFont {
                    double basePt = fnt.pointSizeF();
                    if (basePt <= 0.0)
                        basePt = (fnt.pointSize() > 0) ? double(fnt.pointSize()) : 10.0;
                    fnt.setPointSizeF(qBound(4.0, basePt * ratio, 144.0));
                    fnt.setHintingPreference(QFont::PreferNoHinting);
                    return fnt;
                };
                for (const CtrlFontGestureBaseline::GraphFonts &gf : d_ctrl_font_gesture.graphs) {
                    Graph *gr = gf.graph.data();
                    if (!gr || gr->ignoresResizeEvents() || !gr->plotWidget())
                        continue;
                    Plot *pl = gr->plotWidget();
                    for (auto it = gf.textMarkerFonts.constBegin();
                         it != gf.textMarkerFonts.constEnd(); ++it) {
                        if (Legend *mrk = gr->textMarker(it.key()))
                            mrk->setFont(scaledBaselineFont(it.value()));
                    }
                    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                        pl->setAxisFont(a, scaledBaselineFont(gf.axisFonts[a]));
                        QwtText axTitle = pl->axisTitle(a);
                        axTitle.setFont(scaledBaselineFont(gf.axisTitleFonts[a]));
                        pl->setAxisTitle(a, axTitle);
                    }
                    QwtText pltTitle = pl->title();
                    pltTitle.setFont(scaledBaselineFont(gf.titleFont));
                    pl->setTitle(pltTitle);
                    pl->replot();
                }
                arrangeLayers(false, false);
            }
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

static void detachClosingLayer(MultiLayer *ml, Graph *g)
{
    if (!ml || !g)
        return;
    g->blockSignals(true);
    g->disconnect(ml);
    g->removeEventFilter(ml);
    if (Plot *cp = g->plotWidget()) {
        cp->removeEventFilter(ml);
        if (cp->canvas())
            cp->canvas()->removeEventFilter(ml);
    }
}

void MultiLayer::removeLayer()
{
    if (!active_graph)
        return;

    clearLayerSelector();
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
    detachClosingLayer(this, closingGraph);
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
            if (g->scalePicker) {
                const QPointF shift = sw->property("_toriAxisTitleShift").toPointF();
                if (std::hypot(shift.x(), shift.y()) > 1e-4
                    && g->scalePicker->titleRect(sw).contains(swLocal)) {
                    if (hitTargetOut) {
                        *hitTargetOut = pl->canvas() ? static_cast<QWidget *>(pl->canvas())
                                                     : static_cast<QWidget *>(pl);
                    }
                    return true;
                }
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

    // Direct drag & click-to-select on Zoom Inset layers
    if (d_inset_drag_graph
        && (e->type() == QEvent::MouseMove || e->type() == QEvent::MouseButtonRelease)) {
        Graph *dragGr = d_inset_drag_graph.data();
        if (!d_inset_specs.contains(dragGr)) {
            d_inset_drag_graph.clear();
            d_inset_drag_before_snap.clear();
            d_inset_drag_active = false;
        } else {
            const auto *me = static_cast<const QMouseEvent *>(e);
            auto finishInsetDrag = [&]() {
                const bool didMove = d_inset_drag_active && dragGr
                        && (dragGr->geometry() != d_inset_drag_start_geo);
                const QString beforeSnap = d_inset_drag_before_snap;
                d_inset_drag_graph.clear();
                d_inset_drag_start_geo = QRect();
                d_inset_drag_start_global = QPoint();
                d_inset_drag_before_snap.clear();
                d_inset_drag_active = false;
                if (dragGr) {
                    setActiveGraph(dragGr);
                    if (d_layers_selector && !d_layers_selector->contains(dragGr))
                        clearLayerSelector();
                    if (!d_layers_selector) {
                        d_layers_selector = new SelectionMoveResizer(dragGr);
                        connect(d_layers_selector, SIGNAL(targetsChanged()), this,
                                SIGNAL(modifiedPlot()));
                    }
                    enforceLayerZOrder();
                    if (didMove) {
                        emit modifiedPlot();
                        pushUndoSnapshot(tr("Move Inset Layer"), beforeSnap);
                    }
                }
            };
            if (e->type() == QEvent::MouseMove) {
                if (!(me->buttons() & Qt::LeftButton)) {
                    finishInsetDrag();
                    return true;
                }
                const QPoint delta = me->globalPos() - d_inset_drag_start_global;
                if (!d_inset_drag_active
                    && delta.manhattanLength() >= QApplication::startDragDistance()) {
                    d_inset_drag_active = true;
                }
                if (d_inset_drag_active) {
                    QRect newGeo = d_inset_drag_start_geo.translated(delta);
                    if (canvas && canvas->width() >= 40 && canvas->height() >= 40) {
                        const QRect bounds = canvas->rect().adjusted(2, 2, -2, -2);
                        const int minX = bounds.left();
                        const int maxX = qMax(minX, bounds.right() - newGeo.width() + 1);
                        const int minY = bounds.top();
                        const int maxY = qMax(minY, bounds.bottom() - newGeo.height() + 1);
                        newGeo.moveLeft(qBound(minX, newGeo.left(), maxX));
                        newGeo.moveTop(qBound(minY, newGeo.top(), maxY));
                    }
                    const bool prevSync = d_syncing_inset_geometries;
                    d_syncing_inset_geometries = true;
                    const bool prevIgnore = dragGr->ignoresResizeEvents();
                    dragGr->setIgnoreResizeEvents(true);
                    dragGr->setGeometry(newGeo);
                    dragGr->setIgnoreResizeEvents(prevIgnore);
                    d_syncing_inset_geometries = prevSync;

                    Graph *srcLayer = d_inset_specs.value(dragGr).sourceLayer.data();
                    if (srcLayer && srcLayer->width() >= 20 && srcLayer->height() >= 20) {
                        const QRect srcGeo = srcLayer->geometry();
                        d_inset_specs[dragGr].normGeo = QRectF(
                                double(newGeo.x() - srcGeo.x()) / double(srcGeo.width()),
                                double(newGeo.y() - srcGeo.y()) / double(srcGeo.height()),
                                double(newGeo.width()) / double(srcGeo.width()),
                                double(newGeo.height()) / double(srcGeo.height()));
                    }
                    syncCanvasOverlayVisibility();
                    if (canvas)
                        canvas->update();
                }
                return true;
            }
            if (e->type() == QEvent::MouseButtonRelease && me->button() == Qt::LeftButton) {
                finishInsetDrag();
                return true;
            }
        }
    }

    if (e->type() == QEvent::MouseButtonPress && !addTextOn) {
        Graph *ownerLayer = owningLayerOfObject(object, graphsList);
        if (ownerLayer && d_inset_specs.contains(ownerLayer)) {
            const auto *me = static_cast<const QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton
                && !(me->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))
                && !(d_layers_selector && d_layers_selector->contains(ownerLayer))
                && !ownerLayer->zoomOn() && !ownerLayer->activeTool()
                && !ownerLayer->drawTextActive() && !ownerLayer->drawLineActive()) {
                d_inset_drag_graph = ownerLayer;
                d_inset_drag_start_global = me->globalPos();
                d_inset_drag_start_geo = ownerLayer->geometry();
                d_inset_drag_before_snap = captureUndoSnapshot();
                d_inset_drag_active = false;
                setActiveGraph(ownerLayer);
                enforceLayerZOrder();
                return true;
            }
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
        if (!d_suppress_canvas_resize_layout)
            resizeLayers((const QResizeEvent *)e);
        updateCanvasStatusPill();
    } else if (e->type() == QEvent::Show && object == (QObject *)canvas) {
        updateCanvasStatusPill();
    } else if (d_layer_pill_frame && object == (QObject *)d_layer_pill_frame
               && (e->type() == QEvent::Resize || e->type() == QEvent::Move
                   || e->type() == QEvent::Show || e->type() == QEvent::LayoutRequest)) {
        repositionCanvasStatusPill();
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
        if (object && object->objectName() == QLatin1String("qt_scrollarea_viewport")) {
            auto isConstrainedPane = [this]() -> bool {
                ApplicationWindow *app = findMultiLayerApp(this);
                const bool splitMode = app && app->modernRibbonBar()
                        && app->modernRibbonBar()->workspaceLayoutMode()
                                == ModernRibbonBar::LayoutSplitTableGraph;
                QMdiArea *mdi = mdiArea();
                const bool tabbedMode = (mdi && mdi->viewMode() == QMdiArea::TabbedView);
                return splitMode || tabbedMode || isMaximized();
            };
            if (isConstrainedPane()) {
                QWidget *vp = qobject_cast<QWidget *>(object);
                const bool sizeChanged = vp
                        && (!d_last_autofit_vp_size.isValid()
                            || qAbs(vp->width() - d_last_autofit_vp_size.width()) > 24
                            || qAbs(vp->height() - d_last_autofit_vp_size.height()) > 24);
                const bool shouldSchedule = (!d_was_tiled_split || sizeChanged)
                        && !d_user_zoomed && (d_page_unit != UnitPx || d_page_view_mode)
                        && !d_is_restoring_snapshot;
                d_was_tiled_split = true;
                if (shouldSchedule) {
                    if (!d_viewport_autofit_timer) {
                        d_viewport_autofit_timer = new QTimer(this);
                        d_viewport_autofit_timer->setSingleShot(true);
                        connect(d_viewport_autofit_timer, &QTimer::timeout, this,
                                [this, isConstrainedPane]() {
                                    if (!isConstrainedPane() || d_user_zoomed
                                        || (d_page_unit == UnitPx && !d_page_view_mode)
                                        || d_is_restoring_snapshot) {
                                        return;
                                    }
                                    QScrollArea *cs = widget()
                                            ? widget()->findChild<QScrollArea *>(
                                                      QStringLiteral("MultiLayerCanvasScroll"))
                                            : nullptr;
                                    QWidget *vpNow = cs ? cs->viewport() : nullptr;
                                    const double target = autoFitTargetZoom();
                                    if (qAbs(target - d_page_zoom) > 0.015)
                                        autoFitPageZoomToViewport();
                                    if (vpNow && vpNow->width() >= 60 && vpNow->height() >= 60)
                                        d_last_autofit_vp_size = vpNow->size();
                                });
                    }
                    d_viewport_autofit_timer->start(20);
                }
            } else {
                d_was_tiled_split = false;
                d_last_autofit_vp_size = QSize();
                if (d_viewport_autofit_timer)
                    d_viewport_autofit_timer->stop();
            }
        }
    }
    return MyWidget::eventFilter(object, e);
}

void MultiLayer::showEvent(QShowEvent *event)
{
    MyWidget::showEvent(event);
    if (!d_initial_viewport_fit_done) {
        d_initial_viewport_fit_done = true;
        if (!d_user_zoomed && (d_page_unit != UnitPx || d_page_view_mode)) {
            if (widget() && widget()->layout())
                widget()->layout()->activate();
            autoFitPageZoomToViewport();
            QPointer<MultiLayer> safeMl(this);
            QTimer::singleShot(0, this, [safeMl]() {
                if (safeMl && safeMl->isVisible() && !safeMl->d_user_zoomed
                    && (safeMl->d_page_unit != UnitPx || safeMl->d_page_view_mode)) {
                    safeMl->autoFitPageZoomToViewport();
                }
            });
        }
    }
    repositionCanvasStatusPill();
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
    if (e->key() == Qt::Key_F12 && (e->modifiers() & Qt::ControlModifier)) {
        if (graphsList.isEmpty())
            return;
        clearLayerSelector();
        int index = graphsList.indexOf((QWidget *)active_graph) + 1;
        if (index >= graphsList.size())
            index = 0;
        Graph *g = (Graph *)graphsList.at(index);
        if (g)
            setActiveGraph(g);
        e->accept();
        return;
    }

    if (e->key() == Qt::Key_F10 && (e->modifiers() & Qt::ControlModifier)) {
        if (graphsList.isEmpty())
            return;
        clearLayerSelector();
        int index = graphsList.indexOf((QWidget *)active_graph) - 1;
        if (index < 0)
            index = graphsList.size() - 1;
        Graph *g = (Graph *)graphsList.at(index);
        if (g)
            setActiveGraph(g);
        e->accept();
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
    if (RecalcManager *rm = RecalcManager::instance(findMultiLayerApp(this))) {
        for (int i = 0; i < graphsList.count(); ++i) {
            if (const Graph *ag = qobject_cast<const Graph *>(graphsList.at(i))) {
                const QString linksXml = rm->saveLinksXmlForGraph(ag);
                if (!linksXml.trimmed().isEmpty()) {
                    s += QString("<ToriRecalcLinks layer=\"%1\" xml=\"%2\"/>\n")
                                 .arg(i)
                                 .arg(escapeXmlAttr(linksXml));
                }
            }
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
        s += QString("<ToriPanelLabels enabled=\"%1\" placement=\"%2\" pattern=\"%3\" fontPt=\"%4\"/>\n")
                     .arg(d_panel_labels_enabled ? 1 : 0)
                     .arg(int(d_panel_label_placement))
                     .arg(escapeXmlAttr(d_panel_label_pattern))
                     .arg(d_panel_label_font_pt);
    }
    if (!d_applied_style_preset_id.trimmed().isEmpty()) {
        s += QString("<ToriAppliedStylePreset id=\"%1\"/>\n")
                     .arg(escapeXmlAttr(d_applied_style_preset_id.trimmed()));
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
    QPoint pos = canvas->mapFrom(this, e->pos());
    // iterate backwards in Z-stack order, so layers on top are preferred for selection
    const QList<Graph *> ordered = orderedLayersForRender();
    for (int idx = ordered.size() - 1; idx >= 0; --idx) {
        Graph *cand = ordered.at(idx);
        if (!cand || !cand->isVisible())
            continue;
        const int margin = d_inset_specs.contains(cand) ? 10 : 5;
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
        clearLayerSelector();
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
                detachClosingLayer(this, g);
                g->setParent(nullptr);
                g->close();
            }
        }
        graphs = n;
        if (!graphs) {
            active_graph = nullptr;
            refreshLayerButtonBadges();
            syncCanvasOverlayVisibility();
            return;
        }

        // check whether the active Graph.has been deleted
        if (graphsList.indexOf(active_graph.data()) == -1)
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
    d_applied_style_preset_id = ml->d_applied_style_preset_id;
    setProperty("toriAppliedStylePreset",
                d_applied_style_preset_id.isEmpty() ? QVariant()
                                                    : QVariant(d_applied_style_preset_id));
    graph_width = ml->graph_width;
    graph_height = ml->graph_height;
    setPageBackgroundColor(ml->d_page_bg_color); // also sizes the page host and the canvas
    if (d_page_unit == UnitPx && !d_page_view_mode && qAbs(d_page_zoom - 1.0) <= 1e-4) {
        if (ml->canvasWidget() && canvas)
            canvas->resize(ml->canvasWidget()->size());
    }
    // The layers below are placed at the source's pixel positions on a canvas of the source's
    // size. Record that size as already laid out (as restoring <ToriPageSetup> does when a
    // project is opened); otherwise the next resize check sees a canvas size different from the
    // unset lastSize and re-arranges the layers with the default margins, moving and resizing
    // the plot area of a physical-size page (isij_85mm, slide_28pt, ...).
    if (canvas)
        lastSize = canvas->size();

    QWidgetList srcGraphs = ml->graphPtrs();
    for (int i = 0; i < srcGraphs.count(); i++) {
        Graph *g = (Graph *)srcGraphs.at(i);
        Graph *g2 = addLayer(g->pos().x(), g->pos().y(), g->width(), g->height());
        g2->copy(parent, g);
        // Gap between each axis line and its tick labels / title: style presets widen it with
        // the font (13 px at 28 pt) and a project saves it (<ToriAxisSpacing>), but Graph::copy()
        // left the clone at the default 8 px, which moved and resized the plot area.
        if (g->plotWidget() && g2->plotWidget()) {
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                const QwtScaleWidget *ssw = g->plotWidget()->axisWidget(a);
                QwtScaleWidget *dsw = g2->plotWidget()->axisWidget(a);
                if (ssw && dsw && dsw->spacing() != ssw->spacing())
                    dsw->setSpacing(ssw->spacing());
            }
        }
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
    if (canvas)
        lastSize = canvas->size();
}

bool MultiLayer::focusNextPrevChild(bool next)
{
    if (!active_graph)
        return true;

    return active_graph->focusNextPrevChild(next);
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
    popup->setObjectName(QStringLiteral("LayerColumnsChecklistPopup"));
    popup->setAttribute(Qt::WA_DeleteOnClose, true);
    const QColor surfaceBg(0xFF, 0xFF, 0xFF);
    const QColor borderStrong(0xC8, 0xD1, 0xDC);
    const QColor textPrimary(0x1E, 0x29, 0x3B);
    const QColor borderSubtle(0xE2, 0xE8, 0xF0);
    const QColor textSecondary(0x64, 0x74, 0x8B);
    popup->setStyleSheet(QStringLiteral(
            "QFrame#LayerColumnsChecklistPopup { background-color: %1; border: 1px solid %2; border-radius: 8px; }"
            "QLabel#LayerColumnsChecklistHeader { color: %3; border: none; font-weight: 600; font-size: 10.8pt; }"
            "QFrame#LayerColumnsChecklistPopup QTreeWidget { background-color: %1; color: %3; border: 1px solid %4; "
            "border-radius: 5px; font-size: 10.8pt; }")
                                 .arg(surfaceBg.name(), borderStrong.name(),
                                      textPrimary.name(), borderSubtle.name()));

    auto *vbox = new QVBoxLayout(popup);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    auto *hdr = new QLabel(tr("Layer Columns Checklist"), popup);
    hdr->setObjectName(QStringLiteral("LayerColumnsChecklistHeader"));
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

    targetGr = active_graph ? active_graph.data() : const_cast<MultiLayer *>(this)->layer(1);
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
