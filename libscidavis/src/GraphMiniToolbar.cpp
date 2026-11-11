/***************************************************************************
    File                 : GraphMiniToolbar.cpp
    Project              : ToriGraph / SciDAVis
    Description          : OriginPro-style contextual floating mini-toolbar
 ***************************************************************************/
#include "GraphMiniToolbar.h"
#include "Graph.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "Legend.h"
#include "Grid.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"
#include "ScaleDraw.h"

#include <QApplication>
#include <QColorDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QToolButton>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <qwt_symbol.h>

bool GraphMiniToolbar::s_enabled = true;

GraphMiniToolbar::GraphMiniToolbar(QWidget *parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus),
      d_layout(nullptr),
      d_target(TargetCanvas),
      d_graph(nullptr),
      d_axis_id(QwtPlot::xBottom),
      d_curve_index(-1),
      d_legend(nullptr),
      d_filter_installed(false),
      d_in_modal_dialog(false)
{
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);
    setObjectName(QStringLiteral("GraphMiniToolbar"));

    setStyleSheet(QStringLiteral(
            "QFrame#GraphMiniToolbar {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #B0B7C3;"
            "  border-radius: 4px;"
            "}"
            "QToolButton {"
            "  font-family: 'Arial';"
            "  font-size: 8.5pt;"
            "  font-weight: bold;"
            "  color: #1F2937;"
            "  background-color: #F8FAFC;"
            "  border: 1px solid #D1D5DB;"
            "  border-radius: 3px;"
            "  padding: 2px 5px;"
            "  min-height: 20px;"
            "}"
            "QToolButton:hover {"
            "  background-color: #EBF3FC;"
            "  border-color: #1E66B8;"
            "}"
            "QToolButton:checked {"
            "  background-color: #1E66B8;"
            "  color: #FFFFFF;"
            "  border-color: #164E8F;"
            "}"));

    d_layout = new QHBoxLayout(this);
    d_layout->setContentsMargins(4, 3, 4, 3);
    d_layout->setSpacing(3);
}

GraphMiniToolbar::~GraphMiniToolbar()
{
    if (d_filter_installed && qApp) {
        qApp->removeEventFilter(this);
        d_filter_installed = false;
    }
}

GraphMiniToolbar *GraphMiniToolbar::instance()
{
    static GraphMiniToolbar *s_bar = nullptr;
    if (!s_bar && qApp)
        s_bar = new GraphMiniToolbar(nullptr);
    return s_bar;
}

void GraphMiniToolbar::setMiniToolbarEnabled(bool enabled)
{
    s_enabled = enabled;
    if (!enabled) {
        if (GraphMiniToolbar *bar = instance())
            bar->hide();
    }
}

bool GraphMiniToolbar::isMiniToolbarEnabled()
{
    return s_enabled;
}

Graph *GraphMiniToolbar::currentGraph() const
{
    return d_graph.data();
}

void GraphMiniToolbar::clearButtons()
{
    if (!d_layout)
        return;
    QLayoutItem *child = nullptr;
    while ((child = d_layout->takeAt(0)) != nullptr) {
        if (QWidget *w = child->widget())
            delete w;
        delete child;
    }
}

QToolButton *GraphMiniToolbar::addActionBtn(const QString &text, const QString &tooltip,
                                            const char *slotName, bool checkable, bool checked)
{
    QToolButton *btn = new QToolButton(this);
    btn->setText(text);
    btn->setToolTip(tooltip);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCheckable(checkable);
    if (checkable)
        btn->setChecked(checked);
    connect(btn, SIGNAL(clicked()), this, slotName);
    d_layout->addWidget(btn);
    return btn;
}

void GraphMiniToolbar::addSeparatorLine()
{
    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setStyleSheet(QStringLiteral("color: #D8DEE9;"));
    sep->setFixedHeight(18);
    d_layout->addWidget(sep);
}

void GraphMiniToolbar::showNearGlobalPos(const QPoint &globalPos)
{
    adjustSize();
    QPoint pt(globalPos.x() + 10, globalPos.y() - height() - 10);

    QScreen *scr = QGuiApplication::screenAt(globalPos);
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    if (scr) {
        const QRect avail = scr->availableGeometry();
        if (pt.x() + width() > avail.right() - 4)
            pt.setX(qMax(avail.left() + 4, avail.right() - width() - 4));
        if (pt.x() < avail.left() + 4)
            pt.setX(avail.left() + 4);
        if (pt.y() < avail.top() + 4)
            pt.setY(globalPos.y() + 14);
        if (pt.y() + height() > avail.bottom() - 4)
            pt.setY(qMax(avail.top() + 4, avail.bottom() - height() - 4));
    }

    move(pt);
    if (!d_filter_installed && qApp) {
        qApp->installEventFilter(this);
        d_filter_installed = true;
    }
    show();
    raise();
}

void GraphMiniToolbar::popupForCurve(Graph *g, int curveIndex, const QPoint &globalPos)
{
    if (!s_enabled || !g || curveIndex < 0 || curveIndex >= g->curves())
        return;

    QwtPlotCurve *c = g->curve(curveIndex);
    if (!c)
        return;

    d_target = TargetCurve;
    d_graph = g;
    d_curve_index = curveIndex;
    d_legend = nullptr;

    clearButtons();

    QToolButton *colorBtn =
            addActionBtn(tr("Color"), tr("Select Color"), SLOT(onCurvePickColor()));
    QColor curCol = (c->style() != QwtPlotCurve::NoCurve) ? c->pen().color()
                                                          : c->symbol().pen().color();
    QPixmap sw(12, 12);
    sw.fill(curCol.isValid() ? curCol : QColor(Qt::black));
    QPainter sp(&sw);
    sp.setPen(QColor(90, 90, 90));
    sp.drawRect(0, 0, 11, 11);
    sp.end();
    colorBtn->setIcon(QIcon(sw));
    colorBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    addActionBtn(QStringLiteral("W-"), tr("Line width") + QStringLiteral(" -"),
                 SLOT(onCurveLineWidthDec()));
    addActionBtn(QStringLiteral("W+"), tr("Line width") + QStringLiteral(" +"),
                 SLOT(onCurveLineWidthInc()));
    addSeparatorLine();
    addActionBtn(QStringLiteral("S-"), tr("Symbol size") + QStringLiteral(" -"),
                 SLOT(onCurveSymbolSizeDec()));
    addActionBtn(QStringLiteral("S+"), tr("Symbol size") + QStringLiteral(" +"),
                 SLOT(onCurveSymbolSizeInc()));
    addSeparatorLine();

    const bool hasLine = (c->style() != QwtPlotCurve::NoCurve);
    const bool hasSym = (c->symbol().style() != QwtSymbol::NoSymbol);
    addActionBtn(tr("Line"), tr("Line"), SLOT(onCurveStyleLine()), true,
                 hasLine && !hasSym);
    addActionBtn(tr("Scatter"), tr("Scatter"), SLOT(onCurveStyleScatter()), true,
                 !hasLine && hasSym);
    addActionBtn(QStringLiteral("L+S"), tr("Line + Symbol"), SLOT(onCurveStyleLineSymbol()), true,
                 hasLine && hasSym);
    addSeparatorLine();

    addActionBtn(tr("Error Bars"), tr("Error Bars"), SLOT(onCurveToggleErrorBars()));
    addActionBtn(tr("Clear Masks"), tr("Clear Masks"), SLOT(onCurveClearMask()));

    showNearGlobalPos(globalPos);
}

void GraphMiniToolbar::popupForAxis(Graph *g, int qwtAxisId, const QPoint &globalPos)
{
    if (!s_enabled || !g || qwtAxisId < 0 || qwtAxisId >= QwtPlot::axisCnt)
        return;

    Plot *p = g->plotWidget();
    if (!p)
        return;

    d_target = TargetAxis;
    d_graph = g;
    d_axis_id = qwtAxisId;
    d_curve_index = -1;
    d_legend = nullptr;

    clearButtons();

    bool isLog = false;
    if (const QwtScaleEngine *eng = p->axisScaleEngine(qwtAxisId)) {
        if (QwtScaleTransformation *xform = eng->transformation()) {
            isLog = (xform->type() == QwtScaleTransformation::Log10);
            delete xform;
        }
    }

    int majTick = (qwtAxisId < p->getMajorTicksType().size())
            ? p->getMajorTicksType().at(qwtAxisId)
            : (int)ScaleDraw::Out;
    bool ticksIn = (majTick == (int)ScaleDraw::In || majTick == (int)ScaleDraw::Both);

    int oppAxis = QwtPlot::xTop;
    if (qwtAxisId == QwtPlot::xTop)
        oppAxis = QwtPlot::xBottom;
    else if (qwtAxisId == QwtPlot::yLeft)
        oppAxis = QwtPlot::yRight;
    else if (qwtAxisId == QwtPlot::yRight)
        oppAxis = QwtPlot::yLeft;
    bool oppVisible = g->isAxisBackboneVisible(oppAxis);

    addActionBtn(isLog ? tr("Log10") : tr("Linear"), tr("Scale"),
                 SLOT(onAxisToggleLog()), true, isLog);
    addActionBtn(tr("Ticks In/Out"), tr("Ticks In/Out"),
                 SLOT(onAxisToggleTicksInOut()), true, ticksIn);
    addActionBtn(tr("Opposite Axis Line"), tr("Opposite Axis Line"),
                 SLOT(onAxisToggleOppositeLine()), true, oppVisible);
    addSeparatorLine();
    addActionBtn(tr("Mean Line"), tr("Add Mean Reference Line"), SLOT(onAxisAddMeanLine()));
    addSeparatorLine();
    addActionBtn(QStringLiteral("A-"), tr("Font") + QStringLiteral(" -"), SLOT(onAxisFontDec()));
    addActionBtn(QStringLiteral("A+"), tr("Font") + QStringLiteral(" +"), SLOT(onAxisFontInc()));

    showNearGlobalPos(globalPos);
}

void GraphMiniToolbar::popupForCanvas(Graph *g, const QPoint &globalPos)
{
    if (!s_enabled || !g)
        return;

    d_target = TargetCanvas;
    d_graph = g;
    d_curve_index = -1;
    d_legend = nullptr;

    clearButtons();

    const bool boxOn = g->isAxisBackboneVisible(QwtPlot::xTop)
            && g->isAxisBackboneVisible(QwtPlot::yRight);
    Grid *grd = g->grid();
    const bool gridOn = grd && (grd->xEnabled() || grd->yEnabled());

    addActionBtn(tr("Box Frame"), tr("Box Frame"), SLOT(onCanvasToggleBoxFrame()),
                 true, boxOn);
    addActionBtn(tr("Major Grid"), tr("Major Grid"), SLOT(onCanvasToggleMajorGrid()), true,
                 gridOn);
    addSeparatorLine();
    addActionBtn(tr("Rescale"), tr("Rescale to Show All"), SLOT(onCanvasRescaleAll()));
    addActionBtn(QStringLiteral("1:1"), tr("1:1 (Square)"), SLOT(onCanvasAspectSquare()));
    addActionBtn(tr("Fit Page"), tr("Fit Page to Layers"), SLOT(onCanvasFitPageToLayers()));

    showNearGlobalPos(globalPos);
}

void GraphMiniToolbar::popupForLegend(Graph *g, Legend *legend, const QPoint &globalPos)
{
    if (!s_enabled || !g || !legend)
        return;

    d_target = TargetLegend;
    d_graph = g;
    d_legend = legend;
    d_curve_index = -1;

    clearButtons();

    const bool framed = (legend->frameStyle() != Legend::None);
    const bool transp = (legend->backgroundColor().alpha() == 0);

    addActionBtn(tr("Box Frame"), tr("Box Frame"), SLOT(onLegendToggleFrame()),
                 true, framed);
    addActionBtn(tr("Transparent"), tr("Transparent Background"),
                 SLOT(onLegendToggleTransparent()), true, transp);
    addSeparatorLine();
    addActionBtn(QStringLiteral("A-"), tr("Font") + QStringLiteral(" -"), SLOT(onLegendFontDec()));
    addActionBtn(QStringLiteral("A+"), tr("Font") + QStringLiteral(" +"), SLOT(onLegendFontInc()));

    showNearGlobalPos(globalPos);
}

bool GraphMiniToolbar::eventFilter(QObject *watched, QEvent *event)
{
    if (!isVisible() || d_in_modal_dialog)
        return QFrame::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::KeyPress: {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            hide();
            return false;
        }
        break;
    }
    case QEvent::MouseButtonDblClick: {
        QWidget *w = qobject_cast<QWidget *>(watched);
        if (w != this && (!w || !isAncestorOf(w)))
            hide();
        break;
    }
    case QEvent::MouseButtonPress: {
        QWidget *w = qobject_cast<QWidget *>(watched);
        if (w && w != this && !isAncestorOf(w))
            hide();
        break;
    }
    case QEvent::Wheel: {
        QWidget *w = qobject_cast<QWidget *>(watched);
        if (w && w != this && !isAncestorOf(w))
            hide();
        break;
    }
    case QEvent::ApplicationDeactivate:
        hide();
        break;
    default:
        break;
    }
    return QFrame::eventFilter(watched, event);
}

void GraphMiniToolbar::hideEvent(QHideEvent *event)
{
    if (d_filter_installed && qApp) {
        qApp->removeEventFilter(this);
        d_filter_installed = false;
    }
    QFrame::hideEvent(event);
}

bool GraphMiniToolbar::isLegendValid() const
{
    if (!d_graph || !d_legend)
        return false;
    Plot *p = d_graph->plotWidget();
    if (!p)
        return false;
    const QList<int> keys = p->markerKeys();
    for (int k : keys) {
        if (p->marker(k) == d_legend)
            return true;
    }
    return false;
}

void GraphMiniToolbar::onCurvePickColor()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    QColor initial = (c->style() != QwtPlotCurve::NoCurve) ? c->pen().color()
                                                           : c->symbol().pen().color();
    d_in_modal_dialog = true;
    QColor chosen = QColorDialog::getColor(initial, d_graph, tr("Select Color"));
    d_in_modal_dialog = false;
    if (!chosen.isValid() || !d_graph)
        return;

    c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    QPen pen = c->pen();
    pen.setColor(chosen);
    c->setPen(pen);

    QwtSymbol sym = c->symbol();
    QPen spen = sym.pen();
    spen.setColor(chosen);
    sym.setPen(spen);
    if (sym.brush().style() != Qt::NoBrush) {
        QBrush sbr = sym.brush();
        sbr.setColor(chosen);
        sym.setBrush(sbr);
    }
    c->setSymbol(sym);

    d_graph->replot();
    d_graph->notifyChanges();
    popupForCurve(d_graph, d_curve_index, pos() - QPoint(10, -height() - 10));
}

void GraphMiniToolbar::onCurveLineWidthDec()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;
    QPen pen = c->pen();
    int w = qMax(1, (pen.width() <= 0 ? 1 : pen.width()) - 1);
    pen.setWidth(w);
    c->setPen(pen);
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onCurveLineWidthInc()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;
    if (c->style() == QwtPlotCurve::NoCurve)
        c->setStyle(QwtPlotCurve::Lines);
    QPen pen = c->pen();
    int w = qMin(20, (pen.width() <= 0 ? 1 : pen.width()) + 1);
    pen.setWidth(w);
    c->setPen(pen);
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onCurveSymbolSizeDec()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;
    QwtSymbol sym = c->symbol();
    int sz = qMax(3, sym.size().width() - 2);
    sym.setSize(QSize(sz, sz));
    c->setSymbol(sym);
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onCurveSymbolSizeInc()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;
    QwtSymbol sym = c->symbol();
    if (sym.style() == QwtSymbol::NoSymbol) {
        sym.setStyle(QwtSymbol::Ellipse);
        QColor col = c->pen().color();
        sym.setPen(QPen(col, 1));
        sym.setBrush(QBrush(col));
    }
    int sz = qMin(41, qMax(5, sym.size().width() + 2));
    sym.setSize(QSize(sz, sz));
    c->setSymbol(sym);
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::applyCurveLineSymbolMode(int mode)
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    QColor col = (c->style() != QwtPlotCurve::NoCurve) ? c->pen().color()
                                                       : c->symbol().pen().color();
    if (!col.isValid())
        col = QColor(Qt::black);

    if (mode == 0) { // Line
        d_graph->setCurveType(d_curve_index, Graph::Line, true);
        c->setStyle(QwtPlotCurve::Lines);
        QPen pen = c->pen();
        pen.setColor(col);
        if (pen.width() <= 0)
            pen.setWidth(2);
        c->setPen(pen);
        QwtSymbol sym = c->symbol();
        sym.setStyle(QwtSymbol::NoSymbol);
        c->setSymbol(sym);
    } else if (mode == 1) { // Scatter
        d_graph->setCurveType(d_curve_index, Graph::Scatter, true);
        c->setStyle(QwtPlotCurve::NoCurve);
        QwtSymbol sym = c->symbol();
        if (sym.style() == QwtSymbol::NoSymbol)
            sym.setStyle(QwtSymbol::Ellipse);
        if (sym.size().width() < 5)
            sym.setSize(QSize(7, 7));
        sym.setPen(QPen(col, 1));
        sym.setBrush(QBrush(col));
        c->setSymbol(sym);
    } else { // Line + Symbol
        d_graph->setCurveType(d_curve_index, Graph::LineSymbols, true);
        c->setStyle(QwtPlotCurve::Lines);
        QPen pen = c->pen();
        pen.setColor(col);
        if (pen.width() <= 0)
            pen.setWidth(2);
        c->setPen(pen);
        QwtSymbol sym = c->symbol();
        if (sym.style() == QwtSymbol::NoSymbol)
            sym.setStyle(QwtSymbol::Ellipse);
        if (sym.size().width() < 5)
            sym.setSize(QSize(7, 7));
        sym.setPen(QPen(col, 1));
        sym.setBrush(QBrush(col));
        c->setSymbol(sym);
    }
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onCurveStyleLine()
{
    applyCurveLineSymbolMode(0);
}

void GraphMiniToolbar::onCurveStyleScatter()
{
    applyCurveLineSymbolMode(1);
}

void GraphMiniToolbar::onCurveStyleLineSymbol()
{
    applyCurveLineSymbolMode(2);
}

void GraphMiniToolbar::onCurveToggleErrorBars()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    if (QwtErrorPlotCurve *errSelf = dynamic_cast<QwtErrorPlotCurve *>(c)) {
        errSelf->setVisible(!errSelf->isVisible());
    } else if (DataCurve *dc = dynamic_cast<DataCurve *>(c)) {
        const QList<DataCurve *> errs = dc->errorBarsList();
        for (DataCurve *ec : errs) {
            if (ec)
                ec->setVisible(!ec->isVisible());
        }
    }
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onCurveClearMask()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    if (DataCurve *dc = dynamic_cast<DataCurve *>(d_graph->curve(d_curve_index))) {
        dc->clearMasks();
        d_graph->updateStatReferenceLines();
        d_graph->replot();
        d_graph->notifyChanges();
    }
}

void GraphMiniToolbar::onAxisToggleLog()
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    Plot *p = d_graph->plotWidget();
    if (!p)
        return;

    bool currentlyLog = false;
    bool inverted = false;
    if (const QwtScaleEngine *eng = p->axisScaleEngine(d_axis_id)) {
        inverted = eng->testAttribute(QwtScaleEngine::Inverted);
        if (QwtScaleTransformation *xform = eng->transformation()) {
            currentlyLog = (xform->type() == QwtScaleTransformation::Log10);
            delete xform;
        }
    }

    const QwtScaleDiv *div = p->axisScaleDiv(d_axis_id);
    double lo = div ? qMin(div->lowerBound(), div->upperBound()) : 0.1;
    double hi = div ? qMax(div->lowerBound(), div->upperBound()) : 100.0;
    int newType = currentlyLog ? 0 : 1;
    if (newType == 1) {
        if (hi <= 0.0) {
            lo = 0.1;
            hi = 100.0;
        } else if (lo <= 0.0) {
            lo = qMax(1e-6, hi * 1e-4);
        }
    }
    d_graph->setScale(d_axis_id, lo, hi, 0.0, 5, 5, newType, inverted);
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onAxisToggleTicksInOut()
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    Plot *p = d_graph->plotWidget();
    if (!p)
        return;

    QList<int> maj = p->getMajorTicksType();
    int cur = (d_axis_id < maj.size()) ? maj.at(d_axis_id) : (int)ScaleDraw::Out;
    int next = (cur == (int)ScaleDraw::In) ? (int)ScaleDraw::Out : (int)ScaleDraw::In;
    p->setMajorTicksType(d_axis_id, next);
    p->setMinorTicksType(d_axis_id, next);
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onAxisToggleOppositeLine()
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    int oppAxis = QwtPlot::xTop;
    if (d_axis_id == QwtPlot::xTop)
        oppAxis = QwtPlot::xBottom;
    else if (d_axis_id == QwtPlot::yLeft)
        oppAxis = QwtPlot::yRight;
    else if (d_axis_id == QwtPlot::yRight)
        oppAxis = QwtPlot::yLeft;

    d_graph->setAxisBackboneVisible(oppAxis, !d_graph->isAxisBackboneVisible(oppAxis));
}

void GraphMiniToolbar::onAxisAddMeanLine()
{
    if (!d_graph)
        return;
    Graph::ReferenceLineSpec spec;
    spec.orientation =
            (d_axis_id == QwtPlot::yLeft || d_axis_id == QwtPlot::yRight) ? 0 : 1;
    spec.statMode = QStringLiteral("Mean");
    spec.color = QColor(30, 102, 184);
    spec.style = Qt::DashLine;
    spec.width = 1.5;
    d_graph->addReferenceLine(spec);
}

void GraphMiniToolbar::adjustAxisFont(int deltaPt)
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    Plot *p = d_graph->plotWidget();
    if (!p)
        return;

    QFont tickFont = p->axisFont(d_axis_id);
    if (tickFont.family().isEmpty())
        tickFont.setFamily(QStringLiteral("Arial"));
    int tickPt = tickFont.pointSize() > 0 ? tickFont.pointSize() : 12;
    tickFont.setPointSize(qBound(6, tickPt + deltaPt, 48));
    d_graph->setAxisFont(d_axis_id, tickFont);

    QFont titleFont = d_graph->axisTitleFont(d_axis_id);
    if (titleFont.family().isEmpty())
        titleFont.setFamily(QStringLiteral("Arial"));
    int titlePt = titleFont.pointSize() > 0 ? titleFont.pointSize() : 14;
    titleFont.setPointSize(qBound(6, titlePt + deltaPt, 54));
    d_graph->setAxisTitleFont(d_axis_id, titleFont);

    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onAxisFontDec()
{
    adjustAxisFont(-1);
}

void GraphMiniToolbar::onAxisFontInc()
{
    adjustAxisFont(1);
}

void GraphMiniToolbar::onCanvasToggleBoxFrame()
{
    if (!d_graph)
        return;
    const bool boxOn = d_graph->isAxisBackboneVisible(QwtPlot::xTop)
            && d_graph->isAxisBackboneVisible(QwtPlot::yRight);
    d_graph->applyFramePreset(boxOn ? Graph::FrameOpenL : Graph::FrameBoxInwardTicks);
}

void GraphMiniToolbar::onCanvasToggleMajorGrid()
{
    if (!d_graph)
        return;
    Grid *grd = d_graph->grid();
    if (!grd)
        return;
    const bool enable = !(grd->xEnabled() && grd->yEnabled());
    grd->enableX(enable);
    grd->enableY(enable);
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onCanvasRescaleAll()
{
    if (!d_graph)
        return;
    d_graph->setAutoScale();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onCanvasAspectSquare()
{
    if (!d_graph)
        return;
    if (MultiLayer *ml = d_graph->multiLayerParent())
        ml->applyAspectRatioPreset(1.0, true);
}

void GraphMiniToolbar::onCanvasFitPageToLayers()
{
    if (!d_graph)
        return;
    if (MultiLayer *ml = d_graph->multiLayerParent())
        ml->fitPageToLayers();
}

void GraphMiniToolbar::onLegendToggleFrame()
{
    if (!isLegendValid())
        return;
    int next = (d_legend->frameStyle() == Legend::None) ? Legend::Line : Legend::None;
    d_legend->setFrameStyle(next);
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onLegendToggleTransparent()
{
    if (!isLegendValid())
        return;
    QColor bg = d_legend->backgroundColor();
    if (!bg.isValid())
        bg = QColor(Qt::white);
    bg.setAlpha(bg.alpha() == 0 ? 255 : 0);
    d_legend->setBackgroundColor(bg);
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::adjustLegendFont(int deltaPt)
{
    if (!isLegendValid())
        return;
    QFont f = d_legend->font();
    if (f.family().isEmpty())
        f.setFamily(QStringLiteral("Arial"));
    int pt = f.pointSize() > 0 ? f.pointSize() : 12;
    f.setPointSize(qBound(6, pt + deltaPt, 48));
    d_legend->setFont(f);
    d_graph->replot();
    d_graph->notifyChanges();
}

void GraphMiniToolbar::onLegendFontDec()
{
    adjustLegendFont(-1);
}

void GraphMiniToolbar::onLegendFontInc()
{
    adjustLegendFont(1);
}
