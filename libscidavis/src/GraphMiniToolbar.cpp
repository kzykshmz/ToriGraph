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
#include "I18nJapanese.h"
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
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QScreen>
#include <QSize>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QVector>

#include <algorithm>
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

QIcon makeSymbolShapeIcon(QwtSymbol::Style symStyle, const QColor &col, bool hollow)
{
    QIcon icon;
    for (int dpr : { 1, 2 }) {
        QPixmap pm(16 * dpr, 16 * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor stroke = col.isValid() ? col : QColor(0x1E, 0x29, 0x3B);
        if (symStyle == QwtSymbol::NoSymbol) {
            p.setPen(QPen(QColor(0x64, 0x74, 0x8B), 1.4));
            p.drawLine(QPointF(3.0, 13.0), QPointF(13.0, 3.0));
            p.drawEllipse(QPointF(8.0, 8.0), 4.6, 4.6);
        } else {
            QwtSymbol sym(symStyle, hollow ? QBrush(Qt::white) : QBrush(stroke),
                          QPen(stroke, 1), QSize(9, 9));
            sym.draw(&p, QPoint(8, 8));
        }
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

QIcon makeMatchRightYIcon(const QColor &curveCol)
{
    QIcon icon;
    for (int dpr : { 1, 2 }) {
        QPixmap pm(16 * dpr, 16 * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor col = curveCol.isValid() ? curveCol : QColor(0x0F, 0x6C, 0xBD);
        p.setPen(QPen(col, 1.5));
        p.drawLine(QPointF(2.0, 11.0), QPointF(6.5, 6.0));
        p.drawLine(QPointF(6.5, 6.0), QPointF(10.0, 8.5));
        p.setPen(QPen(col, 2.2));
        p.drawLine(QPointF(13.0, 2.5), QPointF(13.0, 13.5));
        p.drawLine(QPointF(10.5, 4.5), QPointF(13.0, 4.5));
        p.drawLine(QPointF(10.5, 8.0), QPointF(13.0, 8.0));
        p.drawLine(QPointF(10.5, 11.5), QPointF(13.0, 11.5));
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

QIcon makeReverseAxisIcon(const QColor &strokeNormal)
{
    QIcon icon;
    auto buildState = [](const QColor &fg, bool checkedState) {
        QPixmap pm(32, 32);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor c = checkedState ? QColor(Qt::white) : fg;
        p.setPen(QPen(c, 1.6));
        p.drawLine(QPointF(3.0, 5.5), QPointF(13.0, 5.5));
        p.drawLine(QPointF(10.2, 3.2), QPointF(13.0, 5.5));
        p.drawLine(QPointF(10.2, 7.8), QPointF(13.0, 5.5));
        p.drawLine(QPointF(13.0, 10.5), QPointF(3.0, 10.5));
        p.drawLine(QPointF(5.8, 8.2), QPointF(3.0, 10.5));
        p.drawLine(QPointF(5.8, 12.8), QPointF(3.0, 10.5));
        p.end();
        return pm;
    };
    icon.addPixmap(buildState(strokeNormal, false), QIcon::Normal, QIcon::Off);
    icon.addPixmap(buildState(strokeNormal, true), QIcon::Normal, QIcon::On);
    return icon;
}

void registerMiniToolbarTranslations()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                  QString::fromUtf8(ja));
    };
    reg("Select Symbol Shape", "マーカー形状を選択");
    reg("Change Symbol Shape", "マーカー形状を変更");
    reg("Circle", "丸 (●)");
    reg("Square", "四角 (■)");
    reg("Up Triangle", "上三角 (▲)");
    reg("Down Triangle", "下三角 (▼)");
    reg("Diamond", "ひし形 (◆)");
    reg("Star", "星 (★)");
    reg("Cross", "バツ (×)");
    reg("None (No Symbol)", "なし (マーカー非表示)");
    reg("Decrease Line Width", "線幅を細くする");
    reg("Increase Line Width", "線幅を太くする");
    reg("Decrease Symbol Size", "マーカーを小さくする");
    reg("Increase Symbol Size", "マーカーを大きくする");
    reg("Match Y-Right Axis Color", "右Y軸の色をこの曲線に合わせる");
    reg("Match Right-Y Axis Color", "右Y軸の色を曲線に合わせる");
    reg("Clear Masked Points", "マスク点をすべて解除");
    reg("Rescale This Axis", "この軸を自動スケール");
    reg("Reverse Axis", "軸を反転");
    reg("Add Mean Reference Line", "平均値の参照線を追加");
    reg("Rescale All (Ctrl+R)", "全軸を自動スケール (Ctrl+R)");
    reg("Box Inward Frame", "四辺枠（内向き目盛り）");
    reg("Open-L Frame", "L字枠");
    reg("Select Text Color", "文字色を選択");
    reg("Change Text Color", "文字色を変更");
    reg("Toggle Subscript", "下付き文字を切替");
    reg("Toggle Superscript", "上付き文字を切替");
    reg("Decrease Text Font Size", "文字サイズを小さくする");
    reg("Increase Text Font Size", "文字サイズを大きくする");
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
    registerMiniToolbarTranslations();
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

QRect GraphMiniToolbar::computeAvailableBounds(const QPoint &anchorGlobal) const
{
    QScreen *scr = QGuiApplication::screenAt(anchorGlobal);
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    QRect avail = scr ? scr->availableGeometry().adjusted(4, 4, -4, -4)
                      : QRect(0, 0, 1920, 1080);

    ApplicationWindow *d_app = findAppWindow(d_graph);
    if (d_app && d_app->isVisible()) {
        const QRect appWin = d_app->frameGeometry().adjusted(6, 6, -6, -6).intersected(avail);
        if (appWin.width() >= 80 && appWin.height() >= 48)
            avail = appWin;

        if (d_app->graphInspectorDock() && d_app->graphInspectorDock()->isVisible()
            && !d_app->graphInspectorDock()->isFloating()) {
            const QRect dockGlobal(d_app->graphInspectorDock()->mapToGlobal(QPoint(0, 0)),
                                   d_app->graphInspectorDock()->size());
            if (dockGlobal.left() > avail.left() + 120 && dockGlobal.left() - 6 >= avail.left())
                avail.setRight(std::min(avail.right(), dockGlobal.left() - 6));
            else if (dockGlobal.right() < avail.right() - 120
                     && dockGlobal.right() + 6 <= avail.right())
                avail.setLeft(std::max(avail.left(), dockGlobal.right() + 6));
        }
    }

    if (d_graph) {
        if (MultiLayer *ml = d_graph->multiLayerParent()) {
            if (ml->isVisible()) {
                const QRect mlRect(ml->mapToGlobal(QPoint(0, 0)), ml->size());
                const QRect innerMl = mlRect.adjusted(6, 6, -6, -6).intersected(avail);
                if (innerMl.width() >= width() && innerMl.height() >= height()) {
                    avail = innerMl;
                } else if (!innerMl.isEmpty()) {
                    if (innerMl.right() >= avail.left())
                        avail.setRight(std::min(avail.right(), innerMl.right()));
                    if (innerMl.left() <= avail.right())
                        avail.setLeft(std::max(avail.left(), innerMl.left()));
                }
            }
        }
    }

    if (avail.right() < avail.left())
        avail.setRight(avail.left());
    if (avail.bottom() < avail.top())
        avail.setBottom(avail.top());
    return avail;
}

void GraphMiniToolbar::reclampToAvailableBounds()
{
    const QPoint refGlobal = !d_last_anchor_global.isNull() ? d_last_anchor_global : pos();
    const QRect avail = computeAvailableBounds(refGlobal);
    const int maxX = std::max(avail.left(), avail.right() - width());
    const int maxY = std::max(avail.top(), avail.bottom() - height());

    QPoint pt = pos();
    pt.setX(std::min(std::max(pt.x(), avail.left()), maxX));
    pt.setY(std::min(std::max(pt.y(), avail.top()), maxY));
    if (pt != pos())
        move(pt);
}

void GraphMiniToolbar::showNearGlobalPos(const QPoint &globalPos)
{
    d_last_anchor_global = globalPos;
    applyThemeStyle();
    adjustSize();
    QPoint pt(globalPos.x() + 10, globalPos.y() - height() - 10);

    const QRect avail = computeAvailableBounds(globalPos);
    const int maxX = std::max(avail.left(), avail.right() - width());
    const int maxY = std::max(avail.top(), avail.bottom() - height());

    if (pt.y() < avail.top())
        pt.setY(globalPos.y() + 14);

    pt.setX(std::min(std::max(pt.x(), avail.left()), maxX));
    pt.setY(std::min(std::max(pt.y(), avail.top()), maxY));

    move(pt);
    reclampToAvailableBounds();
    ensureEventFilterInstalled();
    show();
    raise();
}

void GraphMiniToolbar::popupForCanvas(Graph *g, const QPoint &globalPos)
{
    cancelScheduledAndHide();
    d_target = TargetCanvas;
    d_graph = g;
    d_curve_index = -1;
    d_legend = nullptr;
    if (!s_enabled || !g || !g->plotWidget() || ApplicationWindow::isNonInteractiveContext(g))
        return;
    schedulePopup(globalPos);
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
    } else if (d_target == TargetCanvas) {
        if (!d_graph->plotWidget()) {
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
        const bool hasLine = (c->style() != QwtPlotCurve::NoCurve);
        const bool hasSym = (c->symbol().style() != QwtSymbol::NoSymbol);

        // 1. Color swatch
        QToolButton *colorBtn =
                addActionBtn(QString(), tr("Select Curve Color"), SLOT(onCurvePickColor()));
        colorBtn->setIcon(makeColorSwatchIcon(curCol, isHollow));
        colorBtn->setIconSize(QSize(16, 16));
        colorBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        addSeparatorLine();

        // 2-4. Line / Scatter / Line + Symbol
        addActionBtn(QString(), tr("Line"), SLOT(onCurveStyleLine()), true,
                     hasLine && !hasSym, QStringLiteral("plot-line"));
        addActionBtn(QString(), tr("Scatter"), SLOT(onCurveStyleScatter()), true,
                     !hasLine && hasSym, QStringLiteral("plot-scatter"));
        addActionBtn(QString(), tr("Line + Symbol"), SLOT(onCurveStyleLineSymbol()), true,
                     hasLine && hasSym, QStringLiteral("plot-line-symbol"));

        // 5. Line width -/+
        addActionBtn(QStringLiteral("W\u2212"), tr("Decrease Line Width"),
                     SLOT(onCurveLineWidthDec()));
        addActionBtn(QStringLiteral("W+"), tr("Increase Line Width"),
                     SLOT(onCurveLineWidthInc()));

        addSeparatorLine();

        // 6. Symbol shape quick picker (● ■ ▲ ▼ ◆ ★ × None)
        QToolButton *symShapeBtn =
                addActionBtn(QString(), tr("Select Symbol Shape"), SLOT(onCurvePickSymbolShape()));
        symShapeBtn->setIcon(makeSymbolShapeIcon(c->symbol().style(), curCol, isHollow));
        symShapeBtn->setIconSize(QSize(16, 16));
        symShapeBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        // 7. 1-click Hollow / Filled (○ / ●) toggle
        QToolButton *hollowBtn = addActionBtn(QString(), tr("Hollow Symbol (White Fill)"),
                                              SLOT(onCurveToggleHollowSymbol()), true, isHollow);
        hollowBtn->setIcon(makeHollowSymbolIcon(pal.textPrimary, Qt::white));
        hollowBtn->setIconSize(QSize(16, 16));
        hollowBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        // 8. Symbol size -/+
        addActionBtn(QStringLiteral("S\u2212"), tr("Decrease Symbol Size"),
                     SLOT(onCurveSymbolSizeDec()));
        addActionBtn(QStringLiteral("S+"), tr("Increase Symbol Size"),
                     SLOT(onCurveSymbolSizeInc()));

        addSeparatorLine();

        // 9. Right-Y Axis toggle + conditional Match Y-Right Axis Color
        const bool onRightY = (c->yAxis() == QwtPlot::yRight);
        QToolButton *rightYBtn = addActionBtn(QString(), tr("Assign to Right Y Axis"),
                                              SLOT(onCurveToggleRightY()), true, onRightY);
        rightYBtn->setIcon(makeRightYAxisIcon(pal.textPrimary));
        rightYBtn->setIconSize(QSize(16, 16));
        rightYBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        if (onRightY) {
            QToolButton *matchYBtn =
                    addActionBtn(QString(), tr("Match Y-Right Axis Color"),
                                 SLOT(onCurveMatchRightYColor()));
            matchYBtn->setIcon(makeMatchRightYIcon(curCol));
            matchYBtn->setIconSize(QSize(16, 16));
            matchYBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        }

        // 10. Error Bars toggle
        bool errOn = false;
        int maskedPts = 0;
        if (auto *errSelf = dynamic_cast<QwtErrorPlotCurve *>(c)) {
            errOn = errSelf->isVisible();
        } else if (auto *dc = dynamic_cast<DataCurve *>(c)) {
            maskedPts = dc->maskedPointCount();
            for (DataCurve *ec : dc->errorBarsList()) {
                if (ec && ec->isVisible()) {
                    errOn = true;
                    break;
                }
            }
        }
        addActionBtn(QString(), tr("Toggle Error Bars"), SLOT(onCurveToggleErrorBars()),
                     true, errOn, QStringLiteral("error-bars"));

        // 11. Conditional Clear Mask button
        if (maskedPts > 0) {
            addSeparatorLine();
            addActionBtn(QString(), tr("Clear Masked Points"), SLOT(onCurveClearMask()),
                         false, false, QStringLiteral("unmask-points"));
        }
    } else if (d_target == TargetAxis) {
        Plot *p = d_graph->plotWidget();
        if (!p)
            return;

        bool isLog = false;
        bool isReversed = false;
        if (const QwtScaleEngine *eng = p->axisScaleEngine(d_axis_id)) {
            isReversed = eng->testAttribute(QwtScaleEngine::Inverted);
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

        // 1. Rescale This Axis
        addActionBtn(QString(), tr("Rescale This Axis"), SLOT(onAxisAutoRange()),
                     false, false, QStringLiteral("zoom-auto"));
        // 2. Log10 / Linear toggle
        addActionBtn(QString(), tr("Toggle Log10 / Linear Scale"), SLOT(onAxisToggleLog()),
                     true, isLog, QStringLiteral("axis-log"));
        // 3. Reverse Axis toggle
        QToolButton *revBtn = addActionBtn(QString(), tr("Reverse Axis"),
                                           SLOT(onAxisToggleReverse()), true, isReversed);
        revBtn->setIcon(makeReverseAxisIcon(pal.textPrimary));
        revBtn->setIconSize(QSize(16, 16));
        revBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        // 4. Ticks In / Out toggle
        addActionBtn(QString(), tr("Ticks In/Out"), SLOT(onAxisToggleTicksInOut()),
                     true, ticksIn, QStringLiteral("axis-ticks-in"));
        // 5. Opposite Axis Line toggle
        addActionBtn(QString(), tr("Opposite Axis Line"), SLOT(onAxisToggleOppositeLine()),
                     true, oppVisible, QStringLiteral("frame-box-plain"));
        // 6. Add Mean Reference Line
        addActionBtn(QStringLiteral("\u03BC"), tr("Add Mean Reference Line"),
                     SLOT(onAxisAddMeanLine()));

        addSeparatorLine();
        // 7-8. Font -/+
        addActionBtn(QString(), tr("Decrease Axis Font Size"), SLOT(onAxisFontDec()),
                     false, false, QStringLiteral("font-dec"));
        addActionBtn(QString(), tr("Increase Axis Font Size"), SLOT(onAxisFontInc()),
                     false, false, QStringLiteral("font-inc"));
    } else if (d_target == TargetCanvas) {
        const bool boxOn = d_graph->isAxisBackboneVisible(QwtPlot::xTop)
                && d_graph->isAxisBackboneVisible(QwtPlot::yRight);
        const bool gridOn = d_graph->grid()
                && d_graph->grid()->xEnabled() && d_graph->grid()->yEnabled();

        // 1. Rescale All (Ctrl+R)
        addActionBtn(QString(), tr("Rescale All (Ctrl+R)"), SLOT(onCanvasRescaleAll()),
                     false, false, QStringLiteral("zoom-auto"));
        addSeparatorLine();
        // 2-3. Box Inward / Open-L Frame quick buttons
        addActionBtn(QString(), tr("Box Inward Frame"), SLOT(onCanvasSetBoxInward()),
                     true, boxOn, QStringLiteral("frame-box-inward"));
        addActionBtn(QString(), tr("Open-L Frame"), SLOT(onCanvasSetOpenL()),
                     true, !boxOn, QStringLiteral("frame-open-l"));
        // 4. Major Grid
        addActionBtn(QString(), tr("Toggle Major Grid"), SLOT(onCanvasToggleMajorGrid()),
                     true, gridOn, QStringLiteral("grid-major"));
        addSeparatorLine();
        // 5-6. Square Frame & Fit Page
        addActionBtn(QStringLiteral("1:1"), tr("Square Frame (70%)"),
                     SLOT(onCanvasAspectSquare()));
        addActionBtn(QString(), tr("Fit Page to Layers"), SLOT(onCanvasFitPageToLayers()),
                     false, false, QStringLiteral("layer-arrange"));
    } else if (d_target == TargetLegend) {
        if (!isLegendValid())
            return;

        const bool isMainLegend = (d_legend == d_graph->legend());
        const bool framed = (d_legend->frameStyle() != Legend::None);
        const bool transp = (d_legend->backgroundColor().alpha() == 0);
        const int nCols = qMax(1, d_legend->numColumns());
        const QString curTxt = d_legend->text();
        const bool hasSub = curTxt.endsWith(QStringLiteral("</sub>"));
        const bool hasSup = curTxt.endsWith(QStringLiteral("</sup>"));

        if (isMainLegend) {
            QToolButton *autoPlaceBtn =
                    addActionBtn(QString(), tr("Place in Empty Space (Avoid Data)"),
                                 SLOT(onLegendAutoPlace()), false, false);
            autoPlaceBtn->setIcon(makeAutoPlaceLegendIcon(pal.textPrimary));
            autoPlaceBtn->setIconSize(QSize(16, 16));
            autoPlaceBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
            addSeparatorLine();
        }

        // Text Color swatch
        QToolButton *txtColorBtn =
                addActionBtn(QString(), tr("Select Text Color"), SLOT(onLegendPickTextColor()));
        txtColorBtn->setIcon(makeColorSwatchIcon(d_legend->textColor(), false));
        txtColorBtn->setIconSize(QSize(16, 16));
        txtColorBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        // Box Frame & Transparent Background
        addActionBtn(QString(), tr("Box Frame"), SLOT(onLegendToggleFrame()),
                     true, framed, QStringLiteral("frame-box-inward"));
        addActionBtn(QString(), tr("Transparent Background"),
                     SLOT(onLegendToggleTransparent()), true, transp, QStringLiteral("legend"));

        addSeparatorLine();

        // Subscript x₂ & Superscript x² quick toggles
        addActionBtn(QStringLiteral("x\u2082"), tr("Subscript (<sub>...</sub>)"),
                     SLOT(onLegendToggleSubscript()), true, hasSub);
        addActionBtn(QStringLiteral("x\u00B2"), tr("Superscript (<sup>...</sup>)"),
                     SLOT(onLegendToggleSuperscript()), true, hasSup);

        addSeparatorLine();

        // Font -/+
        addActionBtn(QString(),
                     isMainLegend ? tr("Decrease Legend Font Size") : tr("Decrease Text Font Size"),
                     SLOT(onLegendFontDec()), false, false, QStringLiteral("font-dec"));
        addActionBtn(QString(),
                     isMainLegend ? tr("Increase Legend Font Size") : tr("Increase Text Font Size"),
                     SLOT(onLegendFontInc()), false, false, QStringLiteral("font-inc"));

        if (isMainLegend) {
            addSeparatorLine();
            addActionBtn(QString(), tr("Horizontal Legend"), SLOT(onLegendCycleColumns()),
                         true, nCols > 1, QStringLiteral("layer-arrange"));
            addActionBtn(QString(), tr("Reconstruct Legend"), SLOT(onLegendReconstruct()),
                         false, false, QStringLiteral("recalc-now"));
        }
    }
}

void GraphMiniToolbar::retranslateUi()
{
    applyThemeStyle();
    if (isVisible()) {
        rebuildCurrentButtons();
        adjustSize();
        reclampToAvailableBounds();
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

    d_graph->reconstructLegend();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Curve Color"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
}

void GraphMiniToolbar::onCurvePickSymbolShape()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    const QColor curCol = (c->style() != QwtPlotCurve::NoCurve) ? c->pen().color()
                                                                : c->symbol().pen().color();
    const bool isHollow = Graph::isSymbolWhiteHollow(c->symbol());

    struct ShapeChoice {
        QwtSymbol::Style style;
        QString label;
    };
    const ShapeChoice choices[] = {
        { QwtSymbol::Ellipse, tr("Circle") },
        { QwtSymbol::Rect, tr("Square") },
        { QwtSymbol::UTriangle, tr("Up Triangle") },
        { QwtSymbol::DTriangle, tr("Down Triangle") },
        { QwtSymbol::Diamond, tr("Diamond") },
        { QwtSymbol::Star1, tr("Star") },
        { QwtSymbol::XCross, tr("Cross") },
        { QwtSymbol::NoSymbol, tr("None (No Symbol)") }
    };

    QMenu menu(this);
    QAction *currentAct = nullptr;
    for (const auto &ch : choices) {
        QAction *act = menu.addAction(makeSymbolShapeIcon(ch.style, curCol, isHollow), ch.label);
        act->setData(static_cast<int>(ch.style));
        act->setCheckable(true);
        if (c->symbol().style() == ch.style) {
            act->setChecked(true);
            currentAct = act;
        }
    }
    if (currentAct)
        menu.setActiveAction(currentAct);

    QPoint menuPos = mapToGlobal(QPoint(0, height() + 2));
    if (auto *senderBtn = qobject_cast<QWidget *>(sender()))
        menuPos = senderBtn->mapToGlobal(QPoint(0, senderBtn->height() + 2));

    d_in_modal_dialog = true;
    QAction *picked = menu.exec(menuPos);
    d_in_modal_dialog = false;
    if (!picked || !d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;

    c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    const auto nextStyle = static_cast<QwtSymbol::Style>(picked->data().toInt());
    if (nextStyle == c->symbol().style())
        return;

    const QString beforeSnap = beginGraphUndo();
    const long key = d_graph->curveKey(d_curve_index);
    SavedSymbolState st = d_saved_symbol_states.value(key);

    QwtSymbol sym = c->symbol();
    if (nextStyle == QwtSymbol::NoSymbol) {
        if (sym.style() != QwtSymbol::NoSymbol) {
            st.style = (int)sym.style();
            st.size = qMax(5, sym.size().width());
            st.penWidth = qMax(1, sym.pen().width());
            st.hollowWhite = Graph::isSymbolWhiteHollow(sym);
            st.noFill = (!st.hollowWhite && sym.brush().style() == Qt::NoBrush);
            d_saved_symbol_states.insert(key, st);
        }
        sym.setStyle(QwtSymbol::NoSymbol);
        c->setSymbol(sym);
        if (c->style() == QwtPlotCurve::NoCurve) {
            c->setStyle(QwtPlotCurve::Lines);
            QPen lp = c->pen();
            if (lp.style() == Qt::NoPen)
                lp.setStyle(Qt::SolidLine);
            if (lp.width() <= 0)
                lp.setWidth(2);
            c->setPen(lp);
        }
        d_graph->setCurveType(d_curve_index, Graph::Line, false);
    } else {
        const bool wasNoSym = (sym.style() == QwtSymbol::NoSymbol);
        const QColor col = curCol.isValid() ? curCol : QColor(Qt::black);
        sym.setStyle(nextStyle);
        const int sz = wasNoSym ? qMax(7, st.size) : qMax(5, sym.size().width());
        sym.setSize(QSize(sz, sz));
        QPen sp = sym.pen();
        if (wasNoSym || !sp.color().isValid())
            sp.setColor(col);
        if (sp.width() <= 0)
            sp.setWidth(qMax(1, st.penWidth));
        sym.setPen(sp);
        if (wasNoSym) {
            if (st.hollowWhite)
                sym.setBrush(QBrush(Qt::white));
            else if (st.noFill)
                sym.setBrush(Qt::NoBrush);
            else
                sym.setBrush(QBrush(col));
        }
        c->setSymbol(sym);
        if (c->style() != QwtPlotCurve::NoCurve)
            d_graph->setCurveType(d_curve_index, Graph::LineSymbols, false);
        else
            d_graph->setCurveType(d_curve_index, Graph::Scatter, false);
        c->setSymbol(sym);

        st.style = (int)nextStyle;
        st.size = sz;
        st.penWidth = qMax(1, sym.pen().width());
        st.hollowWhite = Graph::isSymbolWhiteHollow(sym);
        st.noFill = (!st.hollowWhite && sym.brush().style() == Qt::NoBrush);
        d_saved_symbol_states.insert(key, st);
    }

    d_graph->reconstructLegend();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Symbol Shape"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
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
    d_graph->reconstructLegend();
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
    d_graph->reconstructLegend();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Line Width"), beforeSnap, kMergeMiniWidth);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
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
    d_graph->reconstructLegend();
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
    const bool wasNoSym = (sym.style() == QwtSymbol::NoSymbol);
    if (wasNoSym) {
        sym.setStyle(QwtSymbol::Ellipse);
        QColor col = c->pen().color();
        sym.setPen(QPen(col, 1));
        sym.setBrush(QBrush(col));
    }
    int sz = qMin(41, qMax(5, sym.size().width() + 2));
    sym.setSize(QSize(sz, sz));
    c->setSymbol(sym);
    d_graph->reconstructLegend();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Symbol Size"), beforeSnap, kMergeMiniSymSize);
    if (wasNoSym) {
        rebuildCurrentButtons();
        adjustSize();
        reclampToAvailableBounds();
    }
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
    reclampToAvailableBounds();
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
    reclampToAvailableBounds();
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
        d_graph->setTickLabelsOnAxis(QwtPlot::yRight, true);
    }
    d_graph->setAutoScaleAxis(targetY);
    d_graph->refreshAutoAxisTitlesFromColumns();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Assign Curve Y Axis"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
}

void GraphMiniToolbar::onCurveMatchRightYColor()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    QColor col = (c->style() != QwtPlotCurve::NoCurve) ? c->pen().color() : c->symbol().pen().color();
    if (!col.isValid())
        col = QColor(Qt::black);

    const QString beforeSnap = beginGraphUndo();
    d_graph->syncAxisColorToCurve(QwtPlot::yRight, d_curve_index);
    commitGraphUndo(tr("Match Right-Y Axis Color"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
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
                reclampToAvailableBounds();
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
                Column *errCol = new Column(uniqueColName, SciDAVis::ColumnMode::Numeric);
                errCol->setPlotDesignation(SciDAVis::yErr);
                const int rows = data->rowCount();
                if (rows > 0) {
                    const int r0 = qMax(0, dc->startRow());
                    const int r1 = (dc->endRow() >= 0) ? qMin(rows - 1, dc->endRow()) : (rows - 1);
                    const int xColIdx = tbl->colIndex(dc->xColumnName());
                    Column *xCol = (xColIdx >= 0) ? tbl->column(xColIdx) : nullptr;
                    QVector<bool> pointMaskedAtRow(rows, false);
                    for (int p = 0; p < dc->dataSize(); ++p) {
                        if (dc->isPointMasked(p)) {
                            const int tr = dc->tableRow(p);
                            if (tr >= 0 && tr < rows)
                                pointMaskedAtRow[tr] = true;
                        }
                    }

                    QVector<int> validRows;
                    validRows.reserve(qMax(0, r1 - r0 + 1));
                    double sum = 0.0;
                    for (int i = r0; i <= r1; ++i) {
                        if (data->isInvalid(i) || data->isMasked(i)
                            || (xCol && xCol->isMasked(i)) || pointMaskedAtRow[i]) {
                            continue;
                        }
                        const double v = data->valueAt(i);
                        if (!std::isfinite(v))
                            continue;
                        validRows.append(i);
                        sum += v;
                    }

                    const int validCount = validRows.size();
                    double sdVal = 0.0;
                    if (validCount >= 2) {
                        const double mean = sum / double(validCount);
                        double sqSum = 0.0;
                        for (int idx : validRows) {
                            const double diff = data->valueAt(idx) - mean;
                            sqSum += diff * diff;
                        }
                        sdVal = std::sqrt(sqSum / double(validCount - 1));
                    }
                    for (int idx : validRows)
                        errCol->setValueAt(idx, sdVal);
                }
                tbl->d_future_table->addChild(errCol);
                const QString fullErrColName =
                        QString(tbl->name()) + QStringLiteral("_") + errCol->name();
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
    reclampToAvailableBounds();
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
        rebuildCurrentButtons();
        adjustSize();
        reclampToAvailableBounds();
    }
}

void GraphMiniToolbar::onAxisAutoRange()
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    const QString beforeSnap = beginGraphUndo();
    d_graph->setAxisUserLocked(d_axis_id, false);
    d_graph->setAutoScaleAxis(d_axis_id);
    d_graph->replot();
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
    const int keepMajor = qMax(2, p->axisMaxMajor(d_axis_id));
    const int keepMinor = (newType == 1) ? 8 : qMax(0, p->axisMaxMinor(d_axis_id));
    d_graph->setScale(d_axis_id, lo, hi, 0.0, keepMajor, keepMinor, newType, inverted);
    d_graph->notifyChanges();
    commitGraphUndo(tr("Toggle Axis Scale Type"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
}

void GraphMiniToolbar::onAxisToggleReverse()
{
    if (!d_graph || d_axis_id < 0 || d_axis_id >= QwtPlot::axisCnt)
        return;
    Plot *p = d_graph->plotWidget();
    if (!p)
        return;

    const QString beforeSnap = beginGraphUndo();
    bool isLog = false;
    bool inverted = false;
    if (const QwtScaleEngine *eng = p->axisScaleEngine(d_axis_id)) {
        inverted = eng->testAttribute(QwtScaleEngine::Inverted);
        if (QwtScaleTransformation *xform = eng->transformation()) {
            isLog = (xform->type() == QwtScaleTransformation::Log10);
            delete xform;
        }
    }
    const QwtScaleDiv *div = p->axisScaleDiv(d_axis_id);
    const double lo = div ? qMin(div->lowerBound(), div->upperBound()) : 0.0;
    const double hi = div ? qMax(div->lowerBound(), div->upperBound()) : 1.0;
    const double step = d_graph->axisStep(d_axis_id);
    const int keepMajor = qMax(2, p->axisMaxMajor(d_axis_id));
    const int keepMinor = qMax(0, p->axisMaxMinor(d_axis_id));
    d_graph->setScale(d_axis_id, lo, hi, step, keepMajor, keepMinor, isLog ? 1 : 0, !inverted);
    d_graph->notifyChanges();
    commitGraphUndo(tr("Reverse Axis"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
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
    reclampToAvailableBounds();
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
    reclampToAvailableBounds();
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
    d_graph->replot();
    d_graph->notifyChanges();
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
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
}

void GraphMiniToolbar::onCanvasSetBoxInward()
{
    if (!d_graph)
        return;
    const QString beforeSnap = beginGraphUndo();
    d_graph->applyFramePreset(Graph::FrameBoxInwardTicks);
    commitGraphUndo(tr("Box Inward Frame"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
}

void GraphMiniToolbar::onCanvasSetOpenL()
{
    if (!d_graph)
        return;
    const QString beforeSnap = beginGraphUndo();
    d_graph->applyFramePreset(Graph::FrameOpenL);
    commitGraphUndo(tr("Open-L Frame"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
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
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
}

void GraphMiniToolbar::onCanvasRescaleAll()
{
    if (!d_graph)
        return;
    const QString beforeSnap = beginGraphUndo();
    for (int ax = 0; ax < QwtPlot::axisCnt; ++ax)
        d_graph->setAxisUserLocked(ax, false);
    d_graph->setAutoScale();
    d_graph->replot();
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

void GraphMiniToolbar::onLegendPickTextColor()
{
    if (!d_graph || !isLegendValid())
        return;
    const QColor initial = d_legend->textColor().isValid() ? d_legend->textColor() : QColor(Qt::black);
    d_in_modal_dialog = true;
    const QColor chosen = QColorDialog::getColor(initial, d_graph, tr("Select Text Color"));
    d_in_modal_dialog = false;
    if (!chosen.isValid() || !d_graph || !isLegendValid())
        return;

    const QString beforeSnap = beginGraphUndo();
    d_legend->setTextColor(chosen);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Text Color"), beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
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
    reclampToAvailableBounds();
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
    reclampToAvailableBounds();
}

void GraphMiniToolbar::applyLegendMarkupToggle(const QString &openTag, const QString &closeTag,
                                               const QString &undoLabel)
{
    if (!d_graph || !isLegendValid())
        return;

    const QString beforeSnap = beginGraphUndo();
    const QString cur = d_legend->text();
    QString updated = cur;

    if (cur.endsWith(closeTag)) {
        const int openPos = cur.lastIndexOf(openTag);
        if (openPos >= 0) {
            const int innerStart = openPos + openTag.length();
            const int innerLen = cur.length() - closeTag.length() - innerStart;
            updated = cur.left(openPos) + cur.mid(innerStart, innerLen);
        }
    } else {
        static const QRegularExpression reDigits(QStringLiteral("^(.+\\S)([+-]?[0-9]+)$"));
        static const QRegularExpression reTwoCharSym(QStringLiteral("^([^\\s<>])([A-Za-z])$"));
        const QRegularExpressionMatch mDigits = reDigits.match(cur);
        const QRegularExpressionMatch mSym = reTwoCharSym.match(cur);
        if (mDigits.hasMatch() && !mDigits.captured(1).endsWith(QLatin1Char('>'))) {
            updated = mDigits.captured(1) + openTag + mDigits.captured(2) + closeTag;
        } else if (mSym.hasMatch()) {
            updated = mSym.captured(1) + openTag + mSym.captured(2) + closeTag;
        } else if (!cur.isEmpty()) {
            updated = openTag + cur + closeTag;
        } else {
            updated = QStringLiteral("x") + openTag + QStringLiteral("2") + closeTag;
        }
    }

    d_legend->setText(updated);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(undoLabel, beforeSnap);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
}

void GraphMiniToolbar::onLegendToggleSubscript()
{
    applyLegendMarkupToggle(QStringLiteral("<sub>"), QStringLiteral("</sub>"),
                            tr("Toggle Subscript"));
}

void GraphMiniToolbar::onLegendToggleSuperscript()
{
    applyLegendMarkupToggle(QStringLiteral("<sup>"), QStringLiteral("</sup>"),
                            tr("Toggle Superscript"));
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
    reclampToAvailableBounds();
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
    reclampToAvailableBounds();
}
