/***************************************************************************
    File                 : GraphMiniToolbar.cpp
    Project              : ToriGraph / SciDAVis
    Description          : OriginPro-style slimmed zero-flash floating mini-toolbar
 ***************************************************************************/
#include "GraphMiniToolbar.h"
#include "ApplicationWindow.h"
#include "Graph.h"
#include "GraphInspectorDock.h"
#include "Grid.h"
#include "IconLoader.h"
#include "Legend.h"
#include "ModernUiTheme.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"
#include "ScaleDraw.h"
#include "Table.h"
#include "future/core/column/Column.h"

#include <QApplication>
#include <QColorDialog>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QSize>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>

#include <cmath>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>

namespace {

constexpr int kPopupDelayMs = 180;
constexpr int kMergeMiniFont = 4101;
constexpr int kMergeMiniWidth = 4102;
constexpr int kMergeMiniSymSize = 4103;

QElapsedTimer &scheduleClock()
{
    static QElapsedTimer s_clock;
    return s_clock;
}

ApplicationWindow *findAppWindow(QWidget *from)
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

int resolveMasterCurveIndex(Graph *g, int curveIdx)
{
    if (g && curveIdx >= 0 && curveIdx < g->curves()) {
        if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(g->curve(curveIdx))) {
            if (DataCurve *mc = er->masterCurve()) {
                const int mi = g->curveIndex(mc);
                if (mi >= 0)
                    return mi;
            }
        }
    }
    return curveIdx;
}

int findExplicitYErrColumnForCurve(Table *tbl, int colIdx)
{
    if (!tbl || colIdx < 0 || colIdx >= tbl->numCols())
        return -1;
    if (colIdx + 1 < tbl->numCols() && tbl->colPlotDesignation(colIdx + 1) == SciDAVis::yErr)
        return colIdx + 1;
    for (int c = colIdx + 1; c < tbl->numCols(); ++c) {
        const int desig = tbl->colPlotDesignation(c);
        if (desig == SciDAVis::Y)
            break;
        if (desig == SciDAVis::yErr)
            return c;
    }
    return -1;
}

QIcon miniToolbarDuotoneIcon(const QString &semanticId, bool checkable)
{
    if (semanticId.isEmpty())
        return QIcon();
    QIcon ico = IconLoader::duotoneIcon(semanticId, 16, false);
    if (checkable) {
        const QPixmap onPx1x = IconLoader::duotonePixmap(semanticId, 16, 1.0, true);
        const QPixmap onPx2x = IconLoader::duotonePixmap(semanticId, 16, 2.0, true);
        if (!onPx1x.isNull())
            ico.addPixmap(onPx1x, QIcon::Normal, QIcon::On);
        if (!onPx2x.isNull())
            ico.addPixmap(onPx2x, QIcon::Normal, QIcon::On);
    }
    return ico;
}

QIcon makeColorSwatchIcon(const QColor &color, bool hollowSymbol)
{
    QIcon icon;
    for (int dpr : { 1, 2 }) {
        QPixmap pm(16 * dpr, 16 * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor c = color.isValid() ? color : QColor(Qt::black);
        p.setBrush(c);
        p.setPen(QPen(QColor(100, 116, 139, 190), 1.0));
        p.drawRoundedRect(QRectF(1.5, 1.5, 13.0, 13.0), 3.2, 3.2);
        if (hollowSymbol) {
            p.setBrush(Qt::white);
            p.setPen(QPen(c.darker(115), 1.3));
            p.drawEllipse(QPointF(8.0, 8.0), 3.3, 3.3);
        }
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

QIcon makeHollowSymbolIcon(const QColor &strokeNormal, const QColor &strokeChecked)
{
    QIcon icon;
    auto buildState = [](const QColor &fg, bool checkedState) {
        QPixmap pm(32, 32);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(checkedState ? QColor(Qt::white) : fg, 1.8));
        p.setBrush(Qt::white);
        p.drawEllipse(QPointF(8.0, 8.0), 5.0, 5.0);
        p.end();
        return pm;
    };
    icon.addPixmap(buildState(strokeNormal, false), QIcon::Normal, QIcon::Off);
    icon.addPixmap(buildState(strokeChecked, true), QIcon::Normal, QIcon::On);
    return icon;
}

QIcon makeRightYAxisIcon(const QColor &strokeNormal)
{
    QIcon icon;
    auto buildState = [](const QColor &fg, const QColor &accent, bool on) {
        QPixmap pm(32, 32);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor base = on ? QColor(Qt::white) : fg;
        const QColor hi = on ? QColor(Qt::white) : accent;
        p.setPen(QPen(base, 1.3));
        p.drawLine(QPointF(2.5, 13.5), QPointF(13.5, 13.5));
        p.drawLine(QPointF(2.5, 2.5), QPointF(2.5, 13.5));
        p.setPen(QPen(hi, 2.1));
        p.drawLine(QPointF(13.0, 2.5), QPointF(13.0, 13.5));
        p.drawLine(QPointF(10.8, 4.5), QPointF(13.0, 4.5));
        p.drawLine(QPointF(10.8, 8.0), QPointF(13.0, 8.0));
        p.drawLine(QPointF(10.8, 11.5), QPointF(13.0, 11.5));
        p.end();
        return pm;
    };
    const QColor accent(0x0F, 0x6C, 0xBD);
    icon.addPixmap(buildState(strokeNormal, accent, false), QIcon::Normal, QIcon::Off);
    icon.addPixmap(buildState(strokeNormal, accent, true), QIcon::Normal, QIcon::On);
    return icon;
}

QIcon makeAutoPlaceLegendIcon(const QColor &strokeNormal)
{
    QIcon icon;
    QPixmap pm(32, 32);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor accent(0x0F, 0x6C, 0xBD);
    p.setPen(QPen(strokeNormal, 1.3));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(2.0, 2.0, 12.0, 12.0), 2.0, 2.0);
    p.setPen(QPen(strokeNormal, 1.2));
    p.drawLine(QPointF(3.5, 12.0), QPointF(7.5, 8.5));
    p.drawLine(QPointF(7.5, 8.5), QPointF(12.2, 10.5));
    p.setPen(QPen(accent, 1.4));
    p.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 55));
    p.drawRoundedRect(QRectF(7.6, 3.4, 5.0, 3.8), 1.0, 1.0);
    p.end();
    icon.addPixmap(pm);
    return icon;
}

struct MiniBarPalette {
    QColor surfaceBg { 0xFF, 0xFF, 0xFF };
    QColor surfaceAltBg { 0xF8, 0xFA, 0xFC };
    QColor borderStrong { 0xC8, 0xD1, 0xDC };
    QColor borderSubtle { 0xE2, 0xE8, 0xF0 };
    QColor textPrimary { 0x1E, 0x29, 0x3B };
    QColor accent { 0x0F, 0x6C, 0xBD };
    QColor accentHover { 0x11, 0x5E, 0xA3 };
    QColor accentSoftBg { 0xEF, 0xF6, 0xFF };
};

MiniBarPalette miniBarPalette()
{
    return MiniBarPalette {};
}

} // namespace

bool GraphMiniToolbar::s_enabled = true;

GraphMiniToolbar::GraphMiniToolbar(QWidget *parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus),
      d_layout(nullptr),
      d_popup_timer(nullptr),
      d_target(TargetCanvas),
      d_graph(nullptr),
      d_axis_id(QwtPlot::xBottom),
      d_curve_index(-1),
      d_legend(nullptr),
      d_popup_pending(false),
      d_filter_installed(false),
      d_in_modal_dialog(false)
{
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);
    setObjectName(QStringLiteral("GraphMiniToolbar"));

    d_layout = new QHBoxLayout(this);
    d_layout->setContentsMargins(5, 4, 5, 4);
    d_layout->setSpacing(3);

    d_popup_timer = new QTimer(this);
    d_popup_timer->setSingleShot(true);
    d_popup_timer->setInterval(kPopupDelayMs);
    connect(d_popup_timer, &QTimer::timeout, this, &GraphMiniToolbar::onPopupTimerTimeout);

    applyThemeStyle();
}

GraphMiniToolbar::~GraphMiniToolbar()
{
    if (d_popup_timer)
        d_popup_timer->stop();
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
            bar->cancelScheduledAndHide();
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

void GraphMiniToolbar::applyThemeStyle()
{
    static bool s_applying = false;
    if (s_applying)
        return;
    s_applying = true;
    const auto pal = miniBarPalette();
    const QString qss = QStringLiteral(
            "QFrame#GraphMiniToolbar {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 8px;"
            "}"
            "QToolButton {"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 8.5pt;"
            "  font-weight: 600;"
            "  color: %3;"
            "  background-color: %4;"
            "  border: 1px solid %5;"
            "  border-radius: 5px;"
            "  padding: 3px 5px;"
            "  min-width: 22px;"
            "  min-height: 22px;"
            "}"
            "QToolButton:hover {"
            "  background-color: %6;"
            "  color: %7;"
            "  border-color: %7;"
            "}"
            "QToolButton:checked {"
            "  background-color: %7;"
            "  color: #FFFFFF;"
            "  border-color: %8;"
            "}")
                          .arg(pal.surfaceBg.name(), pal.borderStrong.name(),
                               pal.textPrimary.name(), pal.surfaceAltBg.name(),
                               pal.borderSubtle.name(), pal.accentSoftBg.name(),
                               pal.accent.name(), pal.accentHover.name());
    if (styleSheet() != qss)
        setStyleSheet(qss);
    s_applying = false;
}

void GraphMiniToolbar::cancelScheduledAndHide()
{
    d_popup_pending = false;
    if (d_popup_timer && d_popup_timer->isActive())
        d_popup_timer->stop();
    if (isVisible()) {
        hide();
    } else if (d_filter_installed && qApp && !d_in_modal_dialog) {
        qApp->removeEventFilter(this);
        d_filter_installed = false;
    }
}

void GraphMiniToolbar::ensureEventFilterInstalled()
{
    if (!d_filter_installed && qApp) {
        qApp->installEventFilter(this);
        d_filter_installed = true;
    }
}

void GraphMiniToolbar::schedulePopup(const QPoint &globalPos)
{
    d_scheduled_global_pos = globalPos;
    d_press_origin_global = globalPos;
    d_popup_pending = true;
    scheduleClock().start();
    ensureEventFilterInstalled();
    d_popup_timer->start(kPopupDelayMs);
}

void GraphMiniToolbar::clearButtons()
{
    if (!d_layout)
        return;
    QLayoutItem *child = nullptr;
    while ((child = d_layout->takeAt(0)) != nullptr) {
        if (QWidget *w = child->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete child;
    }
}

QToolButton *GraphMiniToolbar::addActionBtn(const QString &text, const QString &tooltip,
                                            const char *slotName, bool checkable, bool checked,
                                            const QString &semanticIconId)
{
    QToolButton *btn = new QToolButton(this);
    btn->setText(text);
    btn->setToolTip(tooltip);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCheckable(checkable);
    if (checkable)
        btn->setChecked(checked);
    if (!semanticIconId.isEmpty()) {
        const QIcon ico = miniToolbarDuotoneIcon(semanticIconId, checkable);
        if (!ico.isNull()) {
            btn->setIcon(ico);
            btn->setIconSize(QSize(16, 16));
            btn->setToolButtonStyle(text.isEmpty() ? Qt::ToolButtonIconOnly
                                                   : Qt::ToolButtonTextBesideIcon);
        }
    }
    connect(btn, SIGNAL(clicked()), this, slotName);
    d_layout->addWidget(btn);
    return btn;
}

void GraphMiniToolbar::addSeparatorLine()
{
    const auto pal = miniBarPalette();
    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setStyleSheet(QStringLiteral("color: %1;").arg(pal.borderSubtle.name()));
    sep->setFixedHeight(16);
    d_layout->addWidget(sep);
}

void GraphMiniToolbar::showNearGlobalPos(const QPoint &globalPos)
{
    applyThemeStyle();
    adjustSize();
    QPoint pt(globalPos.x() + 10, globalPos.y() - height() - 10);

    QScreen *scr = QGuiApplication::screenAt(globalPos);
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    QRect avail = scr ? scr->availableGeometry().adjusted(4, 4, -4, -4)
                      : QRect(0, 0, 1920, 1080);

    ApplicationWindow *d_app = findAppWindow(d_graph);
    if (d_app && d_app->graphInspectorDock() && d_app->graphInspectorDock()->isVisible()
        && !d_app->graphInspectorDock()->isFloating()) {
        const QRect dockGlobal(d_app->graphInspectorDock()->mapToGlobal(QPoint(0, 0)),
                               d_app->graphInspectorDock()->size());
        if (dockGlobal.left() > avail.left() + 200)
            avail.setRight(qMin(avail.right(), dockGlobal.left() - 6));
        else if (dockGlobal.right() < avail.right() - 200)
            avail.setLeft(qMax(avail.left(), dockGlobal.right() + 6));
    }

    if (d_graph) {
        if (MultiLayer *ml = d_graph->multiLayerParent()) {
            if (ml->isVisible()) {
                const QRect mlRect(ml->mapToGlobal(QPoint(0, 0)), ml->size());
                const QRect innerMl = mlRect.adjusted(6, 6, -6, -6).intersected(avail);
                if (innerMl.width() >= width() && innerMl.height() >= height())
                    avail = innerMl;
                else if (mlRect.width() > 60)
                    avail.setRight(qMin(avail.right(), mlRect.right() - 4));
            }
        }
    }

    if (pt.x() + width() > avail.right())
        pt.setX(qMax(avail.left(), avail.right() - width()));
    if (pt.x() < avail.left())
        pt.setX(avail.left());
    if (pt.y() < avail.top())
        pt.setY(globalPos.y() + 14);
    if (pt.y() + height() > avail.bottom())
        pt.setY(qMax(avail.top(), avail.bottom() - height()));

    move(pt);
    ensureEventFilterInstalled();
    show();
    raise();
}

void GraphMiniToolbar::popupForCanvas(Graph *g, const QPoint &globalPos)
{
    Q_UNUSED(globalPos);
    // Clean no-op per UX specification: never pop up floating chrome on empty canvas clicks,
    // while recording target state for programmatic callers.
    cancelScheduledAndHide();
    d_target = TargetCanvas;
    d_graph = g;
    d_curve_index = -1;
    d_legend = nullptr;
}

void GraphMiniToolbar::popupForCurve(Graph *g, int curveIndex, const QPoint &globalPos)
{
    cancelScheduledAndHide();
    if (!s_enabled || !g || curveIndex < 0 || curveIndex >= g->curves())
        return;
    if (ApplicationWindow::isNonInteractiveContext(g))
        return;
    const int resolvedIdx = resolveMasterCurveIndex(g, curveIndex);
    if (resolvedIdx < 0 || resolvedIdx >= g->curves() || !g->curve(resolvedIdx))
        return;

    d_target = TargetCurve;
    d_graph = g;
    d_curve_index = resolvedIdx;
    d_legend = nullptr;
    schedulePopup(globalPos);
}

void GraphMiniToolbar::popupForAxis(Graph *g, int qwtAxisId, const QPoint &globalPos)
{
    cancelScheduledAndHide();
    if (!s_enabled || !g || qwtAxisId < 0 || qwtAxisId >= QwtPlot::axisCnt)
        return;
    if (ApplicationWindow::isNonInteractiveContext(g))
        return;
    if (!g->plotWidget())
        return;

    d_target = TargetAxis;
    d_graph = g;
    d_axis_id = qwtAxisId;
    d_curve_index = -1;
    d_legend = nullptr;
    schedulePopup(globalPos);
}

void GraphMiniToolbar::popupForLegend(Graph *g, Legend *legend, const QPoint &globalPos)
{
    cancelScheduledAndHide();
    if (!s_enabled || !g || !legend)
        return;
    if (ApplicationWindow::isNonInteractiveContext(g))
        return;

    d_target = TargetLegend;
    d_graph = g;
    d_legend = legend;
    d_curve_index = -1;
    schedulePopup(globalPos);
}

void GraphMiniToolbar::onPopupTimerTimeout()
{
    if (!d_popup_pending)
        return;
    d_popup_pending = false;

    if (!s_enabled || !d_graph || !d_graph->isVisible()
        || ApplicationWindow::isNonInteractiveContext(d_graph)) {
        cancelScheduledAndHide();
        return;
    }

    if (MultiLayer *ml = d_graph->multiLayerParent()) {
        if (!ml->isVisible() || ml->isRestoringSnapshot()) {
            cancelScheduledAndHide();
            return;
        }
    }

    if (d_target == TargetCurve) {
        d_curve_index = resolveMasterCurveIndex(d_graph, d_curve_index);
        if (d_curve_index < 0 || d_curve_index >= d_graph->curves()
            || !d_graph->curve(d_curve_index)) {
            cancelScheduledAndHide();
            return;
        }
    } else if (d_target == TargetAxis) {
        if (d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt || !d_graph->plotWidget()) {
            cancelScheduledAndHide();
            return;
        }
    } else if (d_target == TargetLegend) {
        if (!isLegendValid()) {
            cancelScheduledAndHide();
            return;
        }
    } else {
        cancelScheduledAndHide();
        return;
    }

    rebuildCurrentButtons();
    showNearGlobalPos(d_scheduled_global_pos);
}

void GraphMiniToolbar::rebuildCurrentButtons()
{
    clearButtons();
    if (!d_graph)
        return;

    const auto pal = miniBarPalette();

    if (d_target == TargetCurve) {
        d_curve_index = resolveMasterCurveIndex(d_graph, d_curve_index);
        QwtPlotCurve *c = (d_curve_index >= 0 && d_curve_index < d_graph->curves())
                ? d_graph->curve(d_curve_index)
                : nullptr;
        if (!c)
            return;

        const QColor curCol = (c->style() != QwtPlotCurve::NoCurve) ? c->pen().color()
                                                                    : c->symbol().pen().color();
        const bool isHollow = Graph::isSymbolWhiteHollow(c->symbol());

        // 1. Color swatch
        QToolButton *colorBtn =
                addActionBtn(QString(), tr("Select Curve Color"), SLOT(onCurvePickColor()));
        colorBtn->setIcon(makeColorSwatchIcon(curCol, isHollow));
        colorBtn->setIconSize(QSize(16, 16));
        colorBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        addSeparatorLine();

        // 2-4. Line / Scatter / Line + Symbol
        const bool hasLine = (c->style() != QwtPlotCurve::NoCurve);
        const bool hasSym = (c->symbol().style() != QwtSymbol::NoSymbol);
        addActionBtn(QString(), tr("Line"), SLOT(onCurveStyleLine()), true,
                     hasLine && !hasSym, QStringLiteral("plot-line"));
        addActionBtn(QString(), tr("Scatter"), SLOT(onCurveStyleScatter()), true,
                     !hasLine && hasSym, QStringLiteral("plot-scatter"));
        addActionBtn(QString(), tr("Line + Symbol"), SLOT(onCurveStyleLineSymbol()), true,
                     hasLine && hasSym, QStringLiteral("plot-line-symbol"));

        addSeparatorLine();

        // 5. Hollow Symbol toggle
        QToolButton *hollowBtn = addActionBtn(QString(), tr("Hollow Symbol (White Fill)"),
                                              SLOT(onCurveToggleHollowSymbol()), true, isHollow);
        hollowBtn->setIcon(makeHollowSymbolIcon(pal.textPrimary, Qt::white));
        hollowBtn->setIconSize(QSize(16, 16));
        hollowBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        // 6. Right-Y Axis toggle
        const bool onRightY = (c->yAxis() == QwtPlot::yRight);
        QToolButton *rightYBtn = addActionBtn(QString(), tr("Assign to Right Y Axis"),
                                              SLOT(onCurveToggleRightY()), true, onRightY);
        rightYBtn->setIcon(makeRightYAxisIcon(pal.textPrimary));
        rightYBtn->setIconSize(QSize(16, 16));
        rightYBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        // 7. Error Bars toggle
        bool errOn = false;
        if (auto *errSelf = dynamic_cast<QwtErrorPlotCurve *>(c)) {
            errOn = errSelf->isVisible();
        } else if (auto *dc = dynamic_cast<DataCurve *>(c)) {
            for (DataCurve *ec : dc->errorBarsList()) {
                if (ec && ec->isVisible()) {
                    errOn = true;
                    break;
                }
            }
        }
        addActionBtn(QString(), tr("Toggle Error Bars"), SLOT(onCurveToggleErrorBars()),
                     true, errOn, QStringLiteral("error-bars"));
    } else if (d_target == TargetAxis) {
        Plot *p = d_graph->plotWidget();
        if (!p)
            return;

        bool isLog = false;
        if (const QwtScaleEngine *eng = p->axisScaleEngine(d_axis_id)) {
            if (QwtScaleTransformation *xform = eng->transformation()) {
                isLog = (xform->type() == QwtScaleTransformation::Log10);
                delete xform;
            }
        }

        const int majTick = (d_axis_id < p->getMajorTicksType().size())
                ? p->getMajorTicksType().at(d_axis_id)
                : (int)ScaleDraw::Out;
        const bool ticksIn = (majTick == (int)ScaleDraw::In || majTick == (int)ScaleDraw::Both);

        int oppAxis = QwtPlot::xTop;
        if (d_axis_id == QwtPlot::xTop)
            oppAxis = QwtPlot::xBottom;
        else if (d_axis_id == QwtPlot::yLeft)
            oppAxis = QwtPlot::yRight;
        else if (d_axis_id == QwtPlot::yRight)
            oppAxis = QwtPlot::yLeft;
        const bool oppVisible = d_graph->isAxisBackboneVisible(oppAxis);

        // 1. Single-Axis Auto Range
        addActionBtn(QString(), tr("Auto Range This Axis"), SLOT(onAxisAutoRange()),
                     false, false, QStringLiteral("zoom-auto"));
        // 2. Log10 / Linear toggle
        addActionBtn(QString(), tr("Toggle Log10 / Linear Scale"), SLOT(onAxisToggleLog()),
                     true, isLog, QStringLiteral("axis-log"));
        // 3. Ticks In / Out toggle
        addActionBtn(QString(), tr("Ticks In/Out"), SLOT(onAxisToggleTicksInOut()),
                     true, ticksIn, QStringLiteral("axis-ticks-in"));
        // 4. Opposite Axis Line toggle
        addActionBtn(QString(), tr("Opposite Axis Line"), SLOT(onAxisToggleOppositeLine()),
                     true, oppVisible, QStringLiteral("frame-box-plain"));
        addSeparatorLine();
        // 5-6. Font -/+
        addActionBtn(QString(), tr("Decrease Axis Font Size"), SLOT(onAxisFontDec()),
                     false, false, QStringLiteral("font-dec"));
        addActionBtn(QString(), tr("Increase Axis Font Size"), SLOT(onAxisFontInc()),
                     false, false, QStringLiteral("font-inc"));
    } else if (d_target == TargetLegend) {
        if (!isLegendValid())
            return;

        const bool framed = (d_legend->frameStyle() != Legend::None);
        const bool transp = (d_legend->backgroundColor().alpha() == 0);
        const int nCols = qMax(1, d_legend->numColumns());

        // 1. Place in Empty Space (Avoid Data)
        QToolButton *autoPlaceBtn =
                addActionBtn(QString(), tr("Place in Empty Space (Avoid Data)"),
                             SLOT(onLegendAutoPlace()), false, false);
        autoPlaceBtn->setIcon(makeAutoPlaceLegendIcon(pal.textPrimary));
        autoPlaceBtn->setIconSize(QSize(16, 16));
        autoPlaceBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        addSeparatorLine();

        // 2-3. Box Frame & Transparent Background
        addActionBtn(QString(), tr("Box Frame"), SLOT(onLegendToggleFrame()),
                     true, framed, QStringLiteral("frame-box-inward"));
        addActionBtn(QString(), tr("Transparent Background"),
                     SLOT(onLegendToggleTransparent()), true, transp, QStringLiteral("legend"));

        addSeparatorLine();

        // 4-5. Font -/+
        addActionBtn(QString(), tr("Decrease Legend Font Size"), SLOT(onLegendFontDec()),
                     false, false, QStringLiteral("font-dec"));
        addActionBtn(QString(), tr("Increase Legend Font Size"), SLOT(onLegendFontInc()),
                     false, false, QStringLiteral("font-inc"));

        addSeparatorLine();

        // 6-7. Multi-column toggle & Reconstruct Legend
        addActionBtn(QString(), tr("Horizontal Legend"), SLOT(onLegendCycleColumns()),
                     true, nCols > 1, QStringLiteral("layer-arrange"));
        addActionBtn(QString(), tr("Reconstruct Legend"), SLOT(onLegendReconstruct()),
                     false, false, QStringLiteral("recalc-now"));
    }
}

void GraphMiniToolbar::retranslateUi()
{
    applyThemeStyle();
    if (isVisible()) {
        rebuildCurrentButtons();
        adjustSize();
    }
}

void GraphMiniToolbar::changeEvent(QEvent *event)
{
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
    else if (event && event->type() == QEvent::PaletteChange)
        applyThemeStyle();
    QFrame::changeEvent(event);
}

bool GraphMiniToolbar::eventFilter(QObject *watched, QEvent *event)
{
    if (d_in_modal_dialog || (!d_popup_pending && !isVisible()))
        return QFrame::eventFilter(watched, event);

    QWidget *w = qobject_cast<QWidget *>(watched);
    const bool insideSelf = (w != nullptr && (w == this || isAncestorOf(w)));

    switch (event->type()) {
    case QEvent::MouseButtonDblClick:
        if (!insideSelf)
            cancelScheduledAndHide();
        break;
    case QEvent::MouseMove: {
        if (!insideSelf) {
            auto *me = static_cast<QMouseEvent *>(event);
            const int dist = (me->globalPos() - d_press_origin_global).manhattanLength();
            if (me->buttons() != Qt::NoButton && dist >= QApplication::startDragDistance()) {
                cancelScheduledAndHide();
            } else if (d_popup_pending && dist >= qMax(28, QApplication::startDragDistance() * 4)) {
                cancelScheduledAndHide();
            }
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        if (!insideSelf) {
            auto *me = static_cast<QMouseEvent *>(event);
            const bool isPropagatingSchedulingClick = d_popup_pending
                    && scheduleClock().isValid() && scheduleClock().elapsed() < 25
                    && (me->globalPos() - d_press_origin_global).manhattanLength() <= 4;
            if (!isPropagatingSchedulingClick)
                cancelScheduledAndHide();
        }
        break;
    }
    case QEvent::Wheel:
        if (!insideSelf)
            cancelScheduledAndHide();
        break;
    case QEvent::KeyPress:
        if (!insideSelf)
            cancelScheduledAndHide();
        break;
    case QEvent::WindowDeactivate:
        if (w && (w == d_graph || qobject_cast<QMdiSubWindow *>(w)
                  || (d_graph && w == d_graph->multiLayerParent()))) {
            cancelScheduledAndHide();
        }
        break;
    case QEvent::Hide:
    case QEvent::Close:
        if (w && d_graph && (w == d_graph || w == d_graph->multiLayerParent()))
            cancelScheduledAndHide();
        break;
    case QEvent::ApplicationDeactivate:
        cancelScheduledAndHide();
        break;
    default:
        break;
    }
    return QFrame::eventFilter(watched, event);
}

void GraphMiniToolbar::hideEvent(QHideEvent *event)
{
    if (!d_popup_pending && d_filter_installed && qApp && !d_in_modal_dialog) {
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

QString GraphMiniToolbar::beginGraphUndo() const
{
    if (d_graph) {
        if (MultiLayer *ml = d_graph->multiLayerParent())
            return ml->captureUndoSnapshot();
    }
    return QString();
}

void GraphMiniToolbar::commitGraphUndo(const QString &label, const QString &beforeSnapshot,
                                       int mergeId)
{
    if (d_graph && !beforeSnapshot.isEmpty()) {
        if (MultiLayer *ml = d_graph->multiLayerParent())
            ml->pushUndoSnapshot(label, beforeSnapshot, mergeId);
    }
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
    QColor chosen = QColorDialog::getColor(initial, d_graph, tr("Select Curve Color"));
    d_in_modal_dialog = false;
    if (!chosen.isValid() || !d_graph)
        return;

    c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    const QString beforeSnap = beginGraphUndo();

    QPen pen = c->pen();
    pen.setColor(chosen);
    c->setPen(pen);

    QwtSymbol sym = c->symbol();
    const bool preserveHollowWhite = Graph::isSymbolWhiteHollow(sym);
    QPen spen = sym.pen();
    spen.setColor(chosen);
    if (spen.width() <= 0)
        spen.setWidth(1);
    sym.setPen(spen);
    if (preserveHollowWhite) {
        sym.setBrush(QBrush(Qt::white));
    } else if (sym.brush().style() != Qt::NoBrush) {
        QBrush sbr = sym.brush();
        sbr.setColor(chosen);
        sym.setBrush(sbr);
    }
    c->setSymbol(sym);

    if (c->brush().style() != Qt::NoBrush) {
        QBrush areaBr = c->brush();
        QColor areaCol = chosen;
        areaCol.setAlpha(c->brush().color().alpha() > 0 ? c->brush().color().alpha() : 70);
        areaBr.setColor(areaCol);
        c->setBrush(areaBr);
    }

    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Curve Color"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
}

void GraphMiniToolbar::onCurveLineWidthDec()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;
    const QString beforeSnap = beginGraphUndo();
    QPen pen = c->pen();
    int w = qMax(1, (pen.width() <= 0 ? 1 : pen.width()) - 1);
    pen.setWidth(w);
    c->setPen(pen);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Line Width"), beforeSnap, kMergeMiniWidth);
}

void GraphMiniToolbar::onCurveLineWidthInc()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;
    const QString beforeSnap = beginGraphUndo();
    if (c->style() == QwtPlotCurve::NoCurve)
        c->setStyle(QwtPlotCurve::Lines);
    QPen pen = c->pen();
    int w = qMin(20, (pen.width() <= 0 ? 1 : pen.width()) + 1);
    pen.setWidth(w);
    c->setPen(pen);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Line Width"), beforeSnap, kMergeMiniWidth);
}

void GraphMiniToolbar::onCurveSymbolSizeDec()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;
    const QString beforeSnap = beginGraphUndo();
    QwtSymbol sym = c->symbol();
    int sz = qMax(3, sym.size().width() - 2);
    sym.setSize(QSize(sz, sz));
    c->setSymbol(sym);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Symbol Size"), beforeSnap, kMergeMiniSymSize);
}

void GraphMiniToolbar::onCurveSymbolSizeInc()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;
    const QString beforeSnap = beginGraphUndo();
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
    commitGraphUndo(tr("Change Symbol Size"), beforeSnap, kMergeMiniSymSize);
}

void GraphMiniToolbar::applyCurveLineSymbolMode(int mode)
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    const QString beforeSnap = beginGraphUndo();
    const long key = d_graph->curveKey(d_curve_index);

    const QPen origPen = c->pen();
    const QwtSymbol origSym = c->symbol();
    const int origXAxis = c->xAxis();
    const int origYAxis = c->yAxis();

    QColor col = (c->style() != QwtPlotCurve::NoCurve) ? origPen.color() : origSym.pen().color();
    if (!col.isValid())
        col = QColor(Qt::black);

    SavedSymbolState st = d_saved_symbol_states.value(key);
    if (origSym.style() != QwtSymbol::NoSymbol) {
        st.style = (int)origSym.style();
        st.size = qMax(5, origSym.size().width());
        st.penWidth = qMax(1, origSym.pen().width());
        st.hollowWhite = Graph::isSymbolWhiteHollow(origSym);
        st.noFill = (!st.hollowWhite && origSym.brush().style() == Qt::NoBrush);
        d_saved_symbol_states.insert(key, st);
    } else if (st.style == (int)QwtSymbol::NoSymbol) {
        st.style = (int)QwtSymbol::Ellipse;
        st.size = 7;
        st.penWidth = 1;
    }

    if (ApplicationWindow *app = findAppWindow(d_graph)) {
        if (MultiLayer *ml = d_graph->multiLayerParent())
            app->invalidateModelessGraphDialogs(ml);
    }
    int ebBefore = 0;
    if (DataCurve *dc = dynamic_cast<DataCurve *>(c)) {
        const int oldType = d_graph->curveType(d_curve_index);
        if (oldType == Graph::VerticalBars || oldType == Graph::HorizontalBars) {
            for (DataCurve *eb : dc->errorBarsList()) {
                const int ebi = d_graph->curveIndex(eb);
                if (ebi >= 0 && ebi < d_curve_index)
                    ++ebBefore;
            }
        }
    }

    if (mode == 0) { // Line
        d_graph->setCurveType(d_curve_index, Graph::Line, true);
        d_curve_index = qBound(0, d_curve_index - ebBefore, qMax(0, d_graph->curves() - 1));
        c = d_graph->curve(d_curve_index);
        if (c) {
            c->setAxis(origXAxis, origYAxis);
            c->setStyle(QwtPlotCurve::Lines);
            QPen pen = origPen;
            pen.setColor(col);
            if (pen.style() == Qt::NoPen)
                pen.setStyle(Qt::SolidLine);
            if (pen.width() <= 0)
                pen.setWidth(2);
            c->setPen(pen);
            QwtSymbol sym = origSym;
            sym.setStyle(QwtSymbol::NoSymbol);
            c->setSymbol(sym);
        }
    } else if (mode == 1 || mode == 2) { // Scatter or Line + Symbol
        d_graph->setCurveType(d_curve_index, mode == 1 ? Graph::Scatter : Graph::LineSymbols, true);
        d_curve_index = qBound(0, d_curve_index - ebBefore, qMax(0, d_graph->curves() - 1));
        c = d_graph->curve(d_curve_index);
        if (c) {
            c->setAxis(origXAxis, origYAxis);
            if (mode == 1) {
                c->setStyle(QwtPlotCurve::NoCurve);
            } else {
                c->setStyle(QwtPlotCurve::Lines);
                QPen pen = origPen;
                pen.setColor(col);
                if (pen.style() == Qt::NoPen)
                    pen.setStyle(Qt::SolidLine);
                if (pen.width() <= 0)
                    pen.setWidth(2);
                c->setPen(pen);
            }
            QwtSymbol sym;
            sym.setStyle(static_cast<QwtSymbol::Style>(st.style));
            sym.setSize(QSize(st.size, st.size));
            sym.setPen(QPen(col, st.penWidth));
            if (st.hollowWhite)
                sym.setBrush(QBrush(Qt::white));
            else if (st.noFill)
                sym.setBrush(Qt::NoBrush);
            else
                sym.setBrush(QBrush(col));
            c->setSymbol(sym);
        }
    }

    d_graph->reconstructLegend();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Plot Style"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
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

void GraphMiniToolbar::onCurveToggleHollowSymbol()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    const QString beforeSnap = beginGraphUndo();
    if (c->symbol().style() == QwtSymbol::NoSymbol) {
        const long key = d_graph->curveKey(d_curve_index);
        SavedSymbolState st = d_saved_symbol_states.value(key);
        const QColor col = c->pen().color().isValid() ? c->pen().color() : QColor(Qt::black);
        QwtSymbol sym(st.style != (int)QwtSymbol::NoSymbol
                              ? static_cast<QwtSymbol::Style>(st.style)
                              : QwtSymbol::Ellipse,
                      QBrush(Qt::white), QPen(col, qMax(1, st.penWidth)),
                      QSize(qMax(7, st.size), qMax(7, st.size)));
        c->setSymbol(sym);
        if (c->style() != QwtPlotCurve::NoCurve)
            d_graph->setCurveType(d_curve_index, Graph::LineSymbols, false);
        c->setSymbol(sym);
        d_graph->reconstructLegend();
        d_graph->replot();
        d_graph->notifyChanges();
    } else {
        const bool nextHollow = !Graph::isSymbolWhiteHollow(c->symbol());
        d_graph->setCurveSymbolHollow(d_curve_index, nextHollow);
        d_graph->reconstructLegend();
    }
    const long key = d_graph->curveKey(d_curve_index);
    if (QwtPlotCurve *after = d_graph->curve(d_curve_index)) {
        SavedSymbolState st = d_saved_symbol_states.value(key);
        st.style = (int)after->symbol().style();
        st.size = qMax(5, after->symbol().size().width());
        st.penWidth = qMax(1, after->symbol().pen().width());
        st.hollowWhite = Graph::isSymbolWhiteHollow(after->symbol());
        st.noFill = false;
        d_saved_symbol_states.insert(key, st);
    }
    commitGraphUndo(tr("Toggle Hollow Symbol"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
}

void GraphMiniToolbar::onCurveToggleRightY()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    const QString beforeSnap = beginGraphUndo();
    const int targetY = (c->yAxis() == QwtPlot::yRight) ? QwtPlot::yLeft : QwtPlot::yRight;
    c->setYAxis(targetY);
    if ( auto *dc = dynamic_cast<DataCurve *>(c) ) {
        for (DataCurve *ec : dc->errorBarsList()) {
            if (ec)
                ec->setYAxis(targetY);
        }
    }
    if (targetY == QwtPlot::yRight) {
        d_graph->enableAxis(QwtPlot::yRight, true);
        d_graph->setAxisBackboneVisible(QwtPlot::yRight, true);
        if (Plot *p = d_graph->plotWidget()) {
            if (QwtScaleWidget *sw = p->axisWidget(QwtPlot::yRight)) {
                if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(sw->scaleDraw()))
                    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
            }
        }
    }
    d_graph->setAutoScaleAxis(targetY);
    d_graph->refreshAutoAxisTitlesFromColumns();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Assign Curve Y Axis"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
}

void GraphMiniToolbar::onCurveToggleErrorBars()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    d_curve_index = resolveMasterCurveIndex(d_graph, d_curve_index);
    QwtPlotCurve *c = (d_curve_index >= 0 && d_curve_index < d_graph->curves())
            ? d_graph->curve(d_curve_index)
            : nullptr;
    if (!c)
        return;

    MultiLayer *ml = d_graph->multiLayerParent();
    const QString before = ml ? ml->captureUndoSnapshot() : QString();
    bool changed = false;

    if (QwtErrorPlotCurve *errSelf = dynamic_cast<QwtErrorPlotCurve *>(c)) {
        errSelf->setVisible(!errSelf->isVisible());
        changed = true;
    } else if (DataCurve *dc = dynamic_cast<DataCurve *>(c)) {
        const QList<DataCurve *> errs = dc->errorBarsList();
        if (!errs.isEmpty()) {
            bool anyVisible = false;
            for (DataCurve *ec : errs) {
                if (ec && ec->isVisible()) {
                    anyVisible = true;
                    break;
                }
            }
            for (DataCurve *ec : errs) {
                if (ec) {
                    ec->setVisible(!anyVisible);
                    changed = true;
                }
            }
        } else {
            Table *tbl = dc->table();
            ApplicationWindow *app = findAppWindow(d_graph);
            if (!tbl || !tbl->d_future_table || dc->xColumnName().isEmpty()) {
                if (app && app->statusBar()) {
                    app->statusBar()->showMessage(
                            tr("This feature is not available for user defined function curves!"),
                            4000);
                }
                rebuildCurrentButtons();
                adjustSize();
                return;
            }

            const int colIdx = tbl->colIndex(dc->yColumnName());
            const int explicitYErrIdx = findExplicitYErrColumnForCurve(tbl, colIdx);
            if (explicitYErrIdx >= 0) {
                const QString errFull =
                        QString(tbl->name()) + QStringLiteral("_") + tbl->colLabel(explicitYErrIdx);
                changed = d_graph->addErrorBars(dc->xColumnName(), dc->title().text(), tbl, errFull,
                                                QwtErrorPlotCurve::Vertical);
            } else if (Column *data = (colIdx >= 0) ? tbl->column(colIdx) : nullptr) {
                const QString baseLabel = tbl->colLabel(colIdx) + QStringLiteral("_yErr");
                QString uniqueColName = baseLabel;
                int suffix = 2;
                const QStringList existingCols = tbl->colNames();
                while (existingCols.contains(uniqueColName)
                       || tbl->colIndex(QString(tbl->name()) + QStringLiteral("_") + uniqueColName)
                               >= 0) {
                    uniqueColName = baseLabel + QStringLiteral("_") + QString::number(suffix++);
                }
                Column *errors = new Column(uniqueColName, SciDAVis::ColumnMode::Numeric);
                errors->setPlotDesignation(SciDAVis::yErr);
                const int rows = data->rowCount();
                if (rows > 0) {
                    double average = 0.0;
                    for (int i = 0; i < rows; ++i)
                        average += data->valueAt(i);
                    average /= rows;
                    double dev = 0.0;
                    for (int i = 0; i < rows; ++i) {
                        const double d = data->valueAt(i) - average;
                        dev += d * d;
                    }
                    dev = std::sqrt(dev / rows);
                    for (int i = 0; i < rows; ++i)
                        errors->setValueAt(i, dev);
                }
                tbl->d_future_table->addChild(errors);
                const QString fullErrColName =
                        QString(tbl->name()) + QStringLiteral("_") + errors->name();
                changed = d_graph->addErrorBars(dc->xColumnName(), dc->title().text(), tbl,
                                                fullErrColName, QwtErrorPlotCurve::Vertical);
                if (changed && app)
                    emit app->modified();
            }
        }
    }

    if (changed) {
        d_graph->updatePlot();
        d_graph->notifyChanges();
        if (ml)
            ml->pushUndoSnapshot(tr("Toggle Error Bars"), before);
    }
    rebuildCurrentButtons();
    adjustSize();
}

void GraphMiniToolbar::onCurveClearMask()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    if (DataCurve *dc = dynamic_cast<DataCurve *>(d_graph->curve(d_curve_index))) {
        const QString beforeSnap = beginGraphUndo();
        dc->clearMasks();
        d_graph->updateStatReferenceLines();
        d_graph->replot();
        d_graph->notifyChanges();
        commitGraphUndo(tr("Clear Curve Masks"), beforeSnap);
    }
}

void GraphMiniToolbar::onAxisAutoRange()
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    const QString beforeSnap = beginGraphUndo();
    d_graph->setAutoScaleAxis(d_axis_id);
    d_graph->notifyChanges();
    commitGraphUndo(tr("Auto Range Axis"), beforeSnap);
}

void GraphMiniToolbar::onAxisToggleLog()
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    Plot *p = d_graph->plotWidget();
    if (!p)
        return;

    const QString beforeSnap = beginGraphUndo();
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
    const int newType = currentlyLog ? 0 : 1;
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
    commitGraphUndo(tr("Toggle Axis Scale Type"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
}

void GraphMiniToolbar::onAxisToggleTicksInOut()
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    Plot *p = d_graph->plotWidget();
    if (!p)
        return;

    const QString beforeSnap = beginGraphUndo();
    QList<int> maj = p->getMajorTicksType();
    int cur = (d_axis_id < maj.size()) ? maj.at(d_axis_id) : (int)ScaleDraw::Out;
    int next = (cur == (int)ScaleDraw::In) ? (int)ScaleDraw::Out : (int)ScaleDraw::In;
    p->setMajorTicksType(d_axis_id, next);
    p->setMinorTicksType(d_axis_id, next);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Toggle Tick Direction"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
}

void GraphMiniToolbar::onAxisToggleOppositeLine()
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    const QString beforeSnap = beginGraphUndo();
    int oppAxis = QwtPlot::xTop;
    if (d_axis_id == QwtPlot::xTop)
        oppAxis = QwtPlot::xBottom;
    else if (d_axis_id == QwtPlot::yLeft)
        oppAxis = QwtPlot::yRight;
    else if (d_axis_id == QwtPlot::yRight)
        oppAxis = QwtPlot::yLeft;

    d_graph->setAxisBackboneVisible(oppAxis, !d_graph->isAxisBackboneVisible(oppAxis));
    commitGraphUndo(tr("Toggle Opposite Axis Line"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
}

void GraphMiniToolbar::onAxisAddMeanLine()
{
    if (!d_graph)
        return;
    const QString beforeSnap = beginGraphUndo();
    Graph::ReferenceLineSpec spec;
    spec.orientation =
            (d_axis_id == QwtPlot::yLeft || d_axis_id == QwtPlot::yRight) ? 0 : 1;
    spec.statMode = QStringLiteral("Mean");
    spec.color = QColor(30, 102, 184);
    spec.style = Qt::DashLine;
    spec.width = 1.5;
    d_graph->addReferenceLine(spec);
    commitGraphUndo(tr("Add Mean Reference Line"), beforeSnap);
}

void GraphMiniToolbar::adjustAxisFont(int deltaPt)
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    Plot *p = d_graph->plotWidget();
    if (!p)
        return;

    const QString beforeSnap = beginGraphUndo();
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
    commitGraphUndo(tr("Adjust Axis Font Size"), beforeSnap, kMergeMiniFont);
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
    const QString beforeSnap = beginGraphUndo();
    const bool boxOn = d_graph->isAxisBackboneVisible(QwtPlot::xTop)
            && d_graph->isAxisBackboneVisible(QwtPlot::yRight);
    d_graph->applyFramePreset(boxOn ? Graph::FrameOpenL : Graph::FrameBoxInwardTicks);
    commitGraphUndo(tr("Toggle Box Frame"), beforeSnap);
}

void GraphMiniToolbar::onCanvasToggleMajorGrid()
{
    if (!d_graph)
        return;
    Grid *grd = d_graph->grid();
    if (!grd)
        return;
    const QString beforeSnap = beginGraphUndo();
    const bool enable = !(grd->xEnabled() && grd->yEnabled());
    grd->enableX(enable);
    grd->enableY(enable);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Toggle Major Grid"), beforeSnap);
}

void GraphMiniToolbar::onCanvasRescaleAll()
{
    if (!d_graph)
        return;
    const QString beforeSnap = beginGraphUndo();
    d_graph->setAutoScale();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Rescale Axes"), beforeSnap);
}

void GraphMiniToolbar::onCanvasAspectSquare()
{
    if (!d_graph)
        return;
    if (MultiLayer *ml = d_graph->multiLayerParent()) {
        const QString beforeSnap = ml->captureUndoSnapshot();
        ml->applySquareCanvasFrame70Percent();
        ml->pushUndoSnapshot(tr("Square Frame (70%)"), beforeSnap);
    }
}

void GraphMiniToolbar::onCanvasFitPageToLayers()
{
    if (!d_graph)
        return;
    if (MultiLayer *ml = d_graph->multiLayerParent()) {
        const QString beforeSnap = ml->captureUndoSnapshot();
        ml->fitPageToLayers();
        ml->pushUndoSnapshot(tr("Fit Page to Layers"), beforeSnap);
    }
}

void GraphMiniToolbar::onLegendAutoPlace()
{
    if (!d_graph || !isLegendValid())
        return;
    const QString beforeSnap = beginGraphUndo();
    d_graph->placeLegendAnchor(Graph::LegendAutoAvoidData);
    d_legend = d_graph->legend();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Place Legend in Empty Space"), beforeSnap);
}

void GraphMiniToolbar::onLegendToggleFrame()
{
    if (!isLegendValid())
        return;
    const QString beforeSnap = beginGraphUndo();
    int next = (d_legend->frameStyle() == Legend::None) ? Legend::Line : Legend::None;
    d_legend->setFrameStyle(next);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Toggle Legend Frame"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
}

void GraphMiniToolbar::onLegendToggleTransparent()
{
    if (!isLegendValid())
        return;
    const QString beforeSnap = beginGraphUndo();
    QColor bg = d_legend->backgroundColor();
    if (!bg.isValid())
        bg = QColor(Qt::white);
    bg.setAlpha(bg.alpha() == 0 ? 255 : 0);
    d_legend->setBackgroundColor(bg);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Toggle Legend Background"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
}

void GraphMiniToolbar::adjustLegendFont(int deltaPt)
{
    if (!isLegendValid())
        return;
    const QString beforeSnap = beginGraphUndo();
    QFont f = d_legend->font();
    if (f.family().isEmpty())
        f.setFamily(QStringLiteral("Arial"));
    int pt = f.pointSize() > 0 ? f.pointSize() : 12;
    f.setPointSize(qBound(6, pt + deltaPt, 48));
    d_legend->setFont(f);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Adjust Legend Font Size"), beforeSnap, kMergeMiniFont);
}

void GraphMiniToolbar::onLegendFontDec()
{
    adjustLegendFont(-1);
}

void GraphMiniToolbar::onLegendFontInc()
{
    adjustLegendFont(1);
}

void GraphMiniToolbar::onLegendReconstruct()
{
    if (!d_graph)
        return;
    const QString beforeSnap = beginGraphUndo();
    d_graph->reconstructLegend();
    d_legend = d_graph->legend();
    commitGraphUndo(tr("Reconstruct Legend"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
}

void GraphMiniToolbar::onLegendCycleColumns()
{
    if (!isLegendValid())
        return;
    const QString beforeSnap = beginGraphUndo();
    const int cur = d_legend->numColumns();
    int next = 1;
    if (cur <= 1)
        next = qBound(2, d_graph ? qMax(2, d_graph->curves()) : 2, 4);
    else if (cur >= 3 && d_graph && d_graph->curves() > 2)
        next = 2;
    else
        next = 1;
    d_legend->setNumColumns(next);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Legend Columns"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
}
