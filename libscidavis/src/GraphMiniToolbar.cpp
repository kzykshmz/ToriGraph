/***************************************************************************
    File                 : GraphMiniToolbar.cpp
    Project              : ToriGraph / SciDAVis
    Description          : OriginPro-style slimmed zero-flash floating mini-toolbar
 ***************************************************************************/
#include "GraphMiniToolbar.h"
#include "ApplicationWindow.h"
#include "AxesDialog.h"
#include "CanvasInlineTextEditor.h"
#include "ExtraPlotTypes.h"
#include "Graph.h"
#include "GraphInspectorDock.h"
#include "Grid.h"
#include "I18nJapanese.h"
#include "IconLoader.h"
#include "Legend.h"
#include "ModernRibbonAndHud.h"
#include "ModernUiTheme.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "PlotStyleLimits.h"
#include "QwtErrorPlotCurve.h"
#include "ScaleDraw.h"
#include "Table.h"
#include "future/core/column/Column.h"

#include <QApplication>
#include <QColorDialog>
#include <QCoreApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMessageBox>
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
#include <QVBoxLayout>
#include <QVector>
#include <QWidgetAction>

#include <algorithm>
#include <cmath>

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
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
    // Shape names are generic English words ("Square" is also a pen cap style, "Cross" a
    // fill pattern), so they are registered for this toolbar's tr() context only (H1).
    auto regCtx = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("GraphMiniToolbar"), QString::fromUtf8(en), QString::fromUtf8(ja));
    };
    reg("Alt+F10: move the keyboard focus into this toolbar (Tab: next button, Esc: back to the graph)",
        "Alt+F10: キーボードでこのツールバーに移動 (Tab: 次のボタン、Esc: グラフに戻る)");
    reg("Select Symbol Shape", "シンボル形状を選択");
    reg("Change Symbol Shape", "シンボル形状を変更");
    regCtx("Circle", "丸 (●)");
    regCtx("Square", "四角 (■)");
    regCtx("Up Triangle", "上三角 (▲)");
    regCtx("Down Triangle", "下三角 (▼)");
    regCtx("Diamond", "ひし形 (◆)");
    regCtx("Star", "星 (★)");
    regCtx("Cross", "バツ (×)");
    regCtx("Mean", "平均");
    regCtx("Off", "なし");
    reg("None (No Symbol)", "なし (シンボル非表示)");
    reg("None (Hide Line)", "なし (線を非表示)");
    reg("Decrease Line Width", "線幅を細くする");
    reg("Increase Line Width", "線幅を太くする");
    reg("Decrease Line Width (current: %1 pt)", "線幅を細くする (現在: %1 pt)");
    reg("Increase Line Width (current: %1 pt)", "線幅を太くする (現在: %1 pt)");
    reg("Line Width: %1 pt — click to choose a preset", "線幅: %1 pt (クリックでプリセットを選択)");
    reg("Line Width: Off (Scatter mode) — click to show line",
        "線幅: なし (散布図モード・クリックで線を表示)");
    reg("Line is hidden (Scatter mode) — click + or the width readout to show line",
        "線は非表示です (散布図モード)。+ または幅の数値を押すと線を表示します");
    reg("Show Line (currently hidden in Scatter mode)", "線を表示 (現在、散布図モードで非表示)");
    reg("Decrease Symbol Size", "シンボルを小さくする");
    reg("Increase Symbol Size", "シンボルを大きくする");
    reg("Decrease Symbol Size (current: %1 pt)", "シンボルを小さくする (現在: %1 pt)");
    reg("Increase Symbol Size (current: %1 pt)", "シンボルを大きくする (現在: %1 pt)");
    reg("Symbol Size: %1 pt — click to choose a preset",
        "シンボルサイズ: %1 pt (クリックでプリセットを選択)");
    reg("Symbol Size: Off — click to show symbols", "シンボルサイズ: なし (クリックでシンボルを表示)");
    reg("Symbols are hidden — click + or the size readout to show symbols",
        "シンボルは非表示です。+ またはサイズの数値を押すとシンボルを表示します");
    reg("Show Symbols (currently hidden)", "シンボルを表示 (現在非表示)");
    reg("Match Y-Right Axis Color", "右Y軸の色をこの曲線に合わせる");
    reg("Match Right-Y Axis Color", "右Y軸の色を曲線に合わせる");
    reg("Clear Masked Points", "マスク点をすべて解除");
    reg("Hide Error Bars", "エラーバーを非表示");
    reg("Show Error Bars", "エラーバーを表示");
    reg("Attach Adjacent Error Column (%1)", "隣接するエラー列 (%1) をエラーバーとして追加");
    reg("Add Error Bars / Error Band...", "エラーバー / エラーバンドを追加...");
    reg("Add Error Band...", "エラーバンドを追加...");
    reg("No adjacent Y-error column found. Open Add Error Bars... to choose an error column or percentage.",
        "隣接する Y エラー列が見つかりません。「エラーバーの追加...」から列または割合を指定してください。");
    reg("Rescale This Axis", "この軸を自動スケール");
    reg("Reverse Axis", "軸を反転");
    reg("Scale: Log10 — click for Linear", "スケール: 対数 (Log10) (クリックで線形)");
    reg("Scale: Linear — click for Log10", "スケール: 線形 (クリックで対数 Log10)");
    reg("Reverse Axis: On — click for Normal", "軸を反転: オン (クリックで通常)");
    reg("Reverse Axis: Off — click to Reverse", "軸を反転: オフ (クリックで反転)");
    reg("Ticks: Inward — click for Outward", "目盛り: 内向き (クリックで外向き)");
    reg("Ticks: Outward — click for Inward", "目盛り: 外向き (クリックで内向き)");
    reg("Opposite Axis Line: On — click to Hide", "対向軸線: オン (クリックで非表示)");
    reg("Opposite Axis Line: Off — click to Show", "対向軸線: オフ (クリックで表示)");
    reg("Toggle y = 0 Reference Line", "y = 0 基準線の表示切替");
    reg("Toggle x = 0 Reference Line", "x = 0 基準線の表示切替");
    reg("y = 0 Reference Line: On — click to Remove", "y = 0 基準線: オン (クリックで削除)");
    reg("y = 0 Reference Line: Off — click to Add", "y = 0 基準線: オフ (クリックで追加)");
    reg("x = 0 Reference Line: On — click to Remove", "x = 0 基準線: オン (クリックで削除)");
    reg("x = 0 Reference Line: Off — click to Add", "x = 0 基準線: オフ (クリックで追加)");
    reg("Add Mean Reference Line", "平均値の基準線を追加");
    reg("Add Reference Line...", "基準線を追加...");
    reg("Rescale All (Ctrl+R)", "全軸を自動スケール (Ctrl+R)");
    reg("Box Inward Frame", "四辺枠 (内向き目盛り)");
    reg("Open-L Frame", "L字枠");
    reg("Select Text Color", "文字色を選択");
    reg("Change Text Color", "文字色を変更");
    reg("Grayscale & Basic", "グレースケール・基本色");
    reg("Custom Color...", "カスタム色...");
    reg("Toggle Subscript", "下付き文字を切替");
    reg("Toggle Superscript", "上付き文字を切替");
    reg("Subscript (x\u2082)", "下付き文字 (x\u2082)");
    reg("Superscript (x\u00B2)", "上付き文字 (x\u00B2)");
    reg("Edit Labels", "項目を編集");
    reg("Edit individual legend labels inline (double-click any line for sub/superscripts)",
        "凡例の各行を個別にインライン編集します (上付き・下付きも対応)");
    reg("Line %1: %2", "行 %1: %2");
    reg("Reset the legend to the default curve names? Manual edits to legend labels will be overwritten.",
        "凡例を既定の曲線名に戻しますか? 手動で編集した凡例テキストは上書きされます。");
    reg("Decrease Text Font Size", "文字サイズを小さくする");
    reg("Increase Text Font Size", "文字サイズを大きくする");
    reg("Lock Aspect", "縦横比固定");
    reg("Lock / Unlock Aspect Ratio (Free Resize)", "縦横比固定 / 自由リサイズを切替");
    reg("Toggle Aspect Ratio Lock", "縦横比固定の切替");
    reg("Open full Axis settings dialog", "軸の詳細設定ダイアログを開く");
    reg("Open full Curve settings dialog", "曲線の詳細設定ダイアログを開く");
    reg("Open full Legend / Text settings dialog", "凡例・テキストの詳細設定ダイアログを開く");
    reg("Open full Layer / Page settings dialog", "レイヤー・ページの詳細設定ダイアログを開く");
}

QString formatCompactPt(double val)
{
    const double rounded = std::round(val * 10.0) / 10.0;
    if (std::fabs(rounded - std::round(rounded)) < 1e-4)
        return QString::number(static_cast<int>(std::lround(rounded)));
    return QString::number(rounded, 'f', 1);
}

bool legendTextAllLinesEndWithTag(const QString &text, const QString &closeTag)
{
    static const QRegularExpression reTokens(
            QStringLiteral("^(\\s*(?:\\\\[cp]\\{[^}]*\\}|\\\\l\\([^)]*\\))\\s*)*"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    bool anyLabel = false;
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        const QRegularExpressionMatch m = reTokens.match(line);
        const int start = m.hasMatch() ? m.capturedLength(0) : 0;
        const QString label = line.mid(start).trimmed();
        if (label.isEmpty())
            continue;
        anyLabel = true;
        if (!label.endsWith(closeTag))
            return false;
    }
    return anyLabel;
}

QString readableLegendLinePreview(Graph *g, const QString &rawLine)
{
    static const QRegularExpression reTok(
            QStringLiteral("^(\\s*(?:\\\\[cp]\\{(\\d+)\\}|\\\\l\\((\\d+)\\))\\s*)*"));
    static const QRegularExpression rePlaceholder(QStringLiteral("^%\\((\\d+)\\)$"));
    QString line = rawLine;
    if (line.endsWith(QLatin1Char('\r')))
        line.chop(1);
    const QRegularExpressionMatch m = reTok.match(line);
    QString label = m.hasMatch() ? line.mid(m.capturedLength(0)).trimmed() : line.trimmed();
    const QRegularExpressionMatch pm = rePlaceholder.match(label);
    if (pm.hasMatch() && g) {
        const int idx = pm.captured(1).toInt() - 1;
        if (idx >= 0 && idx < g->curves() && g->curve(idx))
            label = g->curve(idx)->title().text();
    }
    label.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
    return label.trimmed();
}

double effectivePenWidthF(const QPen &pen, double fallback = 1.0)
{
    if (pen.widthF() > 0.0)
        return pen.widthF();
    if (pen.width() > 0)
        return double(pen.width());
    return fallback;
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
QPointer<GraphMiniToolbar> GraphMiniToolbar::s_instance = nullptr;
QPointer<QCoreApplication> GraphMiniToolbar::s_bound_app = nullptr;
bool GraphMiniToolbar::s_shutting_down = false;

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
    setToolTip(tr("Alt+F10: move the keyboard focus into this toolbar (Tab: next button, Esc: back to the graph)"));

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
    if (s_instance.data() == this)
        s_instance.clear();
}

void GraphMiniToolbar::shutdownForQuit()
{
    d_in_modal_dialog = false;
    d_keyboard_mode = false;
    d_activating = false;
    d_popup_pending = false;
    if (d_popup_timer && d_popup_timer->isActive())
        d_popup_timer->stop();
    if (d_filter_installed && qApp) {
        qApp->removeEventFilter(this);
        d_filter_installed = false;
    }
    if (isVisible())
        hide();
    if (s_instance.data() == this)
        s_instance.clear();
    deleteLater();
}

GraphMiniToolbar *GraphMiniToolbar::instance()
{
    if (!qApp || QCoreApplication::closingDown())
        return nullptr;
    if (s_bound_app.data() != qApp) {
        s_bound_app = qApp;
        s_shutting_down = false;
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, []() {
            s_shutting_down = true;
            if (GraphMiniToolbar *bar = s_instance.data())
                bar->shutdownForQuit();
        });
    }
    if (s_shutting_down)
        return nullptr;
    if (!s_instance)
        s_instance = new GraphMiniToolbar(nullptr);
    return s_instance.data();
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
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 600;"
            "  color: %3;"
            "  background-color: %4;"
            "  border: 1px solid %5;"
            "  border-radius: 5px;"
            "  padding: 3px 6px;"
            "  min-width: 26px;"
            "  min-height: 26px;"
            "}"
            "QToolButton[miniStepper=\"true\"] {"
            "  min-width: 20px;"
            "  padding: 2px 4px;"
            "}"
            "QToolButton[miniReadout=\"true\"] {"
            "  font-size: 9.5pt;"
            "  font-weight: 600;"
            "  min-width: 40px;"
            "  padding: 2px 6px;"
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
            "}"
            "QToolButton:disabled {"
            "  color: #94A3B8;"
            "  background-color: #F1F5F9;"
            "  border-color: %5;"
            "}"
            "QLabel[miniReadoutLabel=\"true\"] {"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 9.5pt;"
            "  font-weight: 600;"
            "  color: %3;"
            "  padding: 0 3px;"
            "  background: transparent;"
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
    // H5: while the activation moves from the main window to the bar (Alt+F10), the MDI area
    // reports "no active sub-window" and the application window asks the bar to hide.
    if (d_keyboard_mode && d_activating && d_graph && d_graph->isVisible())
        return;
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
    btn->setFocusPolicy(Qt::TabFocus);
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
                    // When a split pane is narrower than the toolbar, clamping both horizontal
                    // edges to innerMl would push maxX=avail.left() into GraphInspectorDock.
                    if (innerMl.width() >= width()) {
                        avail.setLeft(std::max(avail.left(), innerMl.left()));
                        avail.setRight(std::min(avail.right(), innerMl.right()));
                    } else if (innerMl.right() < avail.right()
                               && innerMl.right() - width() >= avail.left()) {
                        avail.setRight(innerMl.right());
                    }
                    if (innerMl.height() >= height()) {
                        avail.setTop(std::max(avail.top(), innerMl.top()));
                        avail.setBottom(std::min(avail.bottom(), innerMl.bottom()));
                    }
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
    QPoint refGlobal = d_last_anchor_global;
    if (refGlobal.isNull()) {
        if (d_graph && d_graph->isVisible())
            refGlobal = d_graph->mapToGlobal(d_graph->rect().center());
        else if (!pos().isNull())
            refGlobal = pos();
        else
            refGlobal = QCursor::pos();
    }
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
    // H5: a normal popup never takes the focus from the canvas
    if (!d_keyboard_mode && !(windowFlags() & Qt::WindowDoesNotAcceptFocus))
        setAcceptsFocus(false);
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
    if (d_keyboard_mode) {
        // H5: the focused button is about to be replaced; keep the keyboard inside the bar
        QTimer::singleShot(0, this, [this]() {
            if (d_keyboard_mode && isVisible() && !isAncestorOf(QApplication::focusWidget()))
                enterKeyboardFocus();
        });
    }
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

        // 5. Line width: - [readout pt] +
        const double curW = effectivePenWidthF(c->pen(), 1.0);
        const QString wStr = formatCompactPt(curW);
        QToolButton *wDecBtn = addActionBtn(
                QStringLiteral("\u2212"),
                hasLine ? tr("Decrease Line Width (current: %1 pt)").arg(wStr)
                        : tr("Line is hidden (Scatter mode) — click + or the width readout to show line"),
                SLOT(onCurveLineWidthDec()));
        wDecBtn->setProperty("miniStepper", true);
        wDecBtn->setEnabled(hasLine);

        QToolButton *wReadoutBtn = addActionBtn(
                hasLine ? QStringLiteral("%1 pt").arg(wStr) : tr("Off"),
                hasLine ? tr("Line Width: %1 pt — click to choose a preset").arg(wStr)
                        : tr("Line Width: Off (Scatter mode) — click to show line"),
                SLOT(onCurvePickLineWidth()));
        wReadoutBtn->setProperty("miniReadout", true);

        QToolButton *wIncBtn = addActionBtn(
                QStringLiteral("+"),
                hasLine ? tr("Increase Line Width (current: %1 pt)").arg(wStr)
                        : tr("Show Line (currently hidden in Scatter mode)"),
                SLOT(onCurveLineWidthInc()));
        wIncBtn->setProperty("miniStepper", true);

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

        // 8. Symbol size: - [readout pt] +
        const int curSymSz = hasSym ? qMax(1, c->symbol().size().width()) : 0;
        const QString sStr = QString::number(curSymSz);
        QToolButton *sDecBtn = addActionBtn(
                QStringLiteral("\u2212"),
                hasSym ? tr("Decrease Symbol Size (current: %1 pt)").arg(sStr)
                       : tr("Symbols are hidden — click + or the size readout to show symbols"),
                SLOT(onCurveSymbolSizeDec()));
        sDecBtn->setProperty("miniStepper", true);
        sDecBtn->setEnabled(hasSym);

        QToolButton *sReadoutBtn = addActionBtn(
                hasSym ? QStringLiteral("%1 pt").arg(sStr) : tr("Off"),
                hasSym ? tr("Symbol Size: %1 pt — click to choose a preset").arg(sStr)
                       : tr("Symbol Size: Off — click to show symbols"),
                SLOT(onCurvePickSymbolSize()));
        sReadoutBtn->setProperty("miniReadout", true);

        QToolButton *sIncBtn = addActionBtn(
                QStringLiteral("+"),
                hasSym ? tr("Increase Symbol Size (current: %1 pt)").arg(sStr)
                       : tr("Show Symbols (currently hidden)"),
                SLOT(onCurveSymbolSizeInc()));
        sIncBtn->setProperty("miniStepper", true);

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

        // 10. Error Bars toggle / safe menu
        bool errOn = false;
        bool hasAttachedErr = false;
        QString adjacentErrLabel;
        int maskedPts = 0;
        if (auto *errSelf = dynamic_cast<QwtErrorPlotCurve *>(c)) {
            hasAttachedErr = true;
            errOn = errSelf->isVisible();
        } else if (auto *dc = dynamic_cast<DataCurve *>(c)) {
            maskedPts = dc->maskedPointCount();
            const QList<DataCurve *> errs = dc->errorBarsList();
            hasAttachedErr = !errs.isEmpty();
            for (DataCurve *ec : errs) {
                if (ec && ec->isVisible()) {
                    errOn = true;
                    break;
                }
            }
            if (!hasAttachedErr && dc->table()) {
                const int colIdx = dc->table()->colIndex(dc->yColumnName());
                const int explicitIdx = findExplicitYErrColumnForCurve(dc->table(), colIdx);
                if (explicitIdx >= 0)
                    adjacentErrLabel = dc->table()->colLabel(explicitIdx);
            }
        }
        QString errTip;
        if (errOn)
            errTip = tr("Hide Error Bars");
        else if (hasAttachedErr)
            errTip = tr("Show Error Bars");
        else if (!adjacentErrLabel.isEmpty())
            errTip = tr("Attach Adjacent Error Column (%1)").arg(adjacentErrLabel);
        else
            errTip = tr("Add Error Bars / Error Band...");
        addActionBtn(QString(), errTip, SLOT(onCurveToggleErrorBars()),
                     true, errOn, QStringLiteral("error-bars"));

        // 11. Conditional Clear Mask button
        if (maskedPts > 0) {
            addSeparatorLine();
            addActionBtn(QString(), tr("Clear Masked Points"), SLOT(onCurveClearMask()),
                         false, false, QStringLiteral("unmask-points"));
        }

        // 12. Full Curve settings dialog
        addSeparatorLine();
        addActionBtn(tr("Details..."), tr("Open full Curve settings dialog"),
                     SLOT(onOpenDetails()), false, false, QStringLiteral("settings"));
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
        // 2. Log10 / Linear toggle (tooltip reports current state)
        addActionBtn(QString(),
                     isLog ? tr("Scale: Log10 — click for Linear")
                           : tr("Scale: Linear — click for Log10"),
                     SLOT(onAxisToggleLog()), true, isLog, QStringLiteral("axis-log"));
        // 3. Reverse Axis toggle (tooltip reports current state)
        QToolButton *revBtn = addActionBtn(
                QString(),
                isReversed ? tr("Reverse Axis: On — click for Normal")
                           : tr("Reverse Axis: Off — click to Reverse"),
                SLOT(onAxisToggleReverse()), true, isReversed);
        revBtn->setIcon(makeReverseAxisIcon(pal.textPrimary));
        revBtn->setIconSize(QSize(16, 16));
        revBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        // 4. Ticks In / Out toggle (tooltip reports current state)
        addActionBtn(QString(),
                     ticksIn ? tr("Ticks: Inward — click for Outward")
                             : tr("Ticks: Outward — click for Inward"),
                     SLOT(onAxisToggleTicksInOut()), true, ticksIn,
                     QStringLiteral("axis-ticks-in"));
        // 5. Opposite Axis Line toggle (tooltip reports current state)
        addActionBtn(QString(),
                     oppVisible ? tr("Opposite Axis Line: On — click to Hide")
                                : tr("Opposite Axis Line: Off — click to Show"),
                     SLOT(onAxisToggleOppositeLine()), true, oppVisible,
                     QStringLiteral("frame-box-plain"));
        addSeparatorLine();
        // 6. Zero Reference Lines (y = 0, x = 0), Mean, and + Reference Line...
        const bool hasZeroY = GraphInspectorDock::hasZeroReferenceLine(d_graph, 0);
        const bool hasZeroX = GraphInspectorDock::hasZeroReferenceLine(d_graph, 1);
        addActionBtn(QStringLiteral("y=0"),
                     hasZeroY ? tr("y = 0 Reference Line: On — click to Remove")
                              : tr("y = 0 Reference Line: Off — click to Add"),
                     SLOT(onAxisToggleZeroYLine()), true, hasZeroY);
        addActionBtn(QStringLiteral("x=0"),
                     hasZeroX ? tr("x = 0 Reference Line: On — click to Remove")
                              : tr("x = 0 Reference Line: Off — click to Add"),
                     SLOT(onAxisToggleZeroXLine()), true, hasZeroX);
        addActionBtn(tr("Mean"), tr("Add Mean Reference Line"), SLOT(onAxisAddMeanLine()),
                     false, false, QStringLiteral("mean-line"));
        addActionBtn(QString(), tr("Add Reference Line..."), SLOT(onAxisAddRefLineDialog()),
                     false, false, QStringLiteral("curve-add"));

        addSeparatorLine();
        // 7-8. Font - [readout pt] +
        const double axPt = PlotStyleLimits::effectiveFontPointSizeF(p->axisFont(d_axis_id), 12.0);
        addActionBtn(QString(), tr("Decrease Axis Font Size"), SLOT(onAxisFontDec()),
                     false, false, QStringLiteral("font-dec"));
        QLabel *axFontLbl = new QLabel(QStringLiteral("%1 pt").arg(formatCompactPt(axPt)), this);
        axFontLbl->setProperty("miniReadoutLabel", true);
        axFontLbl->setFocusPolicy(Qt::NoFocus);
        d_layout->addWidget(axFontLbl);
        addActionBtn(QString(), tr("Increase Axis Font Size"), SLOT(onAxisFontInc()),
                     false, false, QStringLiteral("font-inc"));

        // 9. Full Axis settings dialog (opens on exact QwtPlot::Axis via setCurrentQwtAxis)
        addSeparatorLine();
        addActionBtn(tr("Details..."), tr("Open full Axis settings dialog"),
                     SLOT(onOpenDetails()), false, false, QStringLiteral("settings"));
    } else if (d_target == TargetCanvas) {
        const bool hasBottomLeft = d_graph->isAxisBackboneVisible(QwtPlot::xBottom)
                && d_graph->isAxisBackboneVisible(QwtPlot::yLeft);
        const bool hasTopRight = d_graph->isAxisBackboneVisible(QwtPlot::xTop)
                && d_graph->isAxisBackboneVisible(QwtPlot::yRight);
        bool hasTopCurve = false;
        bool hasRightCurve = false;
        for (int i = 0; i < d_graph->curves(); ++i) {
            if (QwtPlotCurve *cv = d_graph->curve(i)) {
                if (cv->xAxis() == QwtPlot::xTop)
                    hasTopCurve = true;
                if (cv->yAxis() == QwtPlot::yRight)
                    hasRightCurve = true;
            }
        }
        const bool topExpectedOpenL = hasTopCurve || d_graph->hasColorScaleOnAxis(QwtPlot::xTop);
        const bool rightExpectedOpenL =
                hasRightCurve || d_graph->hasColorScaleOnAxis(QwtPlot::yRight);
        Plot *p = d_graph->plotWidget();
        const int xMaj = (p && QwtPlot::xBottom < p->getMajorTicksType().size())
                ? p->getMajorTicksType().at(QwtPlot::xBottom)
                : (int)ScaleDraw::Out;
        const int yMaj = (p && QwtPlot::yLeft < p->getMajorTicksType().size())
                ? p->getMajorTicksType().at(QwtPlot::yLeft)
                : (int)ScaleDraw::Out;
        const bool boxInwardOn = hasBottomLeft && hasTopRight
                && (xMaj == (int)ScaleDraw::In || xMaj == (int)ScaleDraw::Both)
                && (yMaj == (int)ScaleDraw::In || yMaj == (int)ScaleDraw::Both);
        const bool gridZeroOn = d_graph->grid()
                && (d_graph->grid()->xZeroLineEnabled() || d_graph->grid()->yZeroLineEnabled());
        const bool openLOn = hasBottomLeft && !gridZeroOn
                && (d_graph->isAxisBackboneVisible(QwtPlot::xTop) == topExpectedOpenL)
                && (d_graph->isAxisBackboneVisible(QwtPlot::yRight) == rightExpectedOpenL)
                && !(topExpectedOpenL && rightExpectedOpenL);
        const bool gridOn = d_graph->grid()
                && d_graph->grid()->xEnabled() && d_graph->grid()->yEnabled();

        // 1. Rescale All (Ctrl+R)
        addActionBtn(QString(), tr("Rescale All (Ctrl+R)"), SLOT(onCanvasRescaleAll()),
                     false, false, QStringLiteral("zoom-auto"));
        addSeparatorLine();
        // 2-3. Box Inward / Open-L Frame quick buttons
        addActionBtn(QString(), tr("Box Inward Frame"), SLOT(onCanvasSetBoxInward()),
                     true, boxInwardOn, QStringLiteral("frame-box-inward"));
        addActionBtn(QString(), tr("Open-L Frame"), SLOT(onCanvasSetOpenL()),
                     true, openLOn, QStringLiteral("frame-open-l"));
        // 4. Major Grid
        addActionBtn(QString(), tr("Toggle Major Grid"), SLOT(onCanvasToggleMajorGrid()),
                     true, gridOn, QStringLiteral("axis-grid"));
        addSeparatorLine();
        // 5-7. Lock Aspect Ratio toggle, Square Frame & Fit Page
        MultiLayer *ml = d_graph->multiLayerParent();
        const bool aspectLocked = ml ? ml->keepAspectRatio() : true;
        QToolButton *lockAspectBtn = new QToolButton(this);
        lockAspectBtn->setObjectName(QStringLiteral("MiniToolbarLockAspectBtn"));
        lockAspectBtn->setText(tr("Lock Aspect"));
        lockAspectBtn->setToolTip(tr("Lock / Unlock Aspect Ratio (Free Resize)"));
        lockAspectBtn->setFocusPolicy(Qt::TabFocus);
        lockAspectBtn->setCheckable(true);
        lockAspectBtn->setChecked(aspectLocked);
        const QIcon lockIco = miniToolbarDuotoneIcon(QStringLiteral("lock"), true);
        if (!lockIco.isNull()) {
            lockAspectBtn->setIcon(lockIco);
            lockAspectBtn->setIconSize(QSize(16, 16));
            lockAspectBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        }
        connect(lockAspectBtn, &QToolButton::clicked, this, [this]() {
            if (!d_graph)
                return;
            MultiLayer *targetMl = d_graph->multiLayerParent();
            if (!targetMl)
                return;
            const QString beforeSnap = targetMl->captureUndoSnapshot();
            targetMl->setKeepAspectRatio(!targetMl->keepAspectRatio());
            targetMl->pushUndoSnapshot(tr("Toggle Aspect Ratio Lock"), beforeSnap);
            if (ApplicationWindow *app = findAppWindow(d_graph)) {
                if (GraphInspectorDock *dock = app->graphInspectorDock())
                    dock->syncInspectorFromSelection();
                if (ModernRibbonBar *rib = app->modernRibbonBar())
                    rib->syncCheckableStates();
            }
            rebuildCurrentButtons();
            adjustSize();
            reclampToAvailableBounds();
        });
        d_layout->addWidget(lockAspectBtn);

        addActionBtn(QStringLiteral("1:1"), tr("Square Frame (70%)"),
                     SLOT(onCanvasAspectSquare()));
        addActionBtn(QString(), tr("Fit Page to Layers"), SLOT(onCanvasFitPageToLayers()),
                     false, false, QStringLiteral("layer-arrange"));

        // 8. Full Layer / Page settings dialog
        addSeparatorLine();
        addActionBtn(tr("Details..."), tr("Open full Layer / Page settings dialog"),
                     SLOT(onOpenDetails()), false, false, QStringLiteral("settings"));
    } else if (d_target == TargetLegend) {
        if (!isLegendValid())
            return;

        const bool isMainLegend = (d_legend == d_graph->legend());
        const bool framed = (d_legend->frameStyle() != Legend::None);
        const bool transp = (d_legend->backgroundColor().alpha() == 0);
        const int nCols = qMax(1, d_legend->numColumns());
        const QString curTxt = d_legend->text();
        int nonEmptyLineCount = 0;
        for (const QString &ln : curTxt.split(QLatin1Char('\n'))) {
            if (!ln.trimmed().isEmpty())
                ++nonEmptyLineCount;
        }
        const bool isMultiLineMainLegend = isMainLegend && (nonEmptyLineCount > 1);
        const bool hasSub = legendTextAllLinesEndWithTag(curTxt, QStringLiteral("</sub>"));
        const bool hasSup = legendTextAllLinesEndWithTag(curTxt, QStringLiteral("</sup>"));

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

        // A3-4: Multi-line primary legends edit individual lines inline instead of applying
        // whole-legend all-line sub/superscript toggles; single-line legends and TextAnnotations
        // keep direct x2 / x^2 toggles.
        if (isMultiLineMainLegend) {
            addActionBtn(
                    tr("Edit Labels"),
                    tr("Edit individual legend labels inline (double-click any line for sub/superscripts)"),
                    SLOT(onLegendEditInline()), false, false, QStringLiteral("text-label"));
        } else {
            addActionBtn(QStringLiteral("x\u2082"), tr("Subscript (x\u2082)"),
                         SLOT(onLegendToggleSubscript()), true, hasSub);
            addActionBtn(QStringLiteral("x\u00B2"), tr("Superscript (x\u00B2)"),
                         SLOT(onLegendToggleSuperscript()), true, hasSup);
        }

        addSeparatorLine();

        // Font - [readout pt] +
        const double legPt = PlotStyleLimits::effectiveFontPointSizeF(d_legend->font(), 12.0);
        addActionBtn(QString(),
                     isMainLegend ? tr("Decrease Legend Font Size") : tr("Decrease Text Font Size"),
                     SLOT(onLegendFontDec()), false, false, QStringLiteral("font-dec"));
        QLabel *legFontLbl = new QLabel(QStringLiteral("%1 pt").arg(formatCompactPt(legPt)), this);
        legFontLbl->setProperty("miniReadoutLabel", true);
        legFontLbl->setFocusPolicy(Qt::NoFocus);
        d_layout->addWidget(legFontLbl);
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

        // Full Legend / Text settings dialog
        addSeparatorLine();
        addActionBtn(tr("Details..."), tr("Open full Legend / Text settings dialog"),
                     SLOT(onOpenDetails()), false, false, QStringLiteral("settings"));
    }
}

QKeySequence GraphMiniToolbar::keyboardFocusShortcut()
{
    // F6 (previous window) and Ctrl+Shift+M (mask points) are taken; F10 is the Windows key
    // family for "move the keyboard into a bar" (F10 menu bar, Shift+F10 context menu).
    return QKeySequence(Qt::ALT | Qt::Key_F10);
}

void GraphMiniToolbar::setAcceptsFocus(bool accept)
{
    const bool wasVisible = isVisible();
    const QPoint p = pos();
    d_changing_flags = true;
    // Changing window flags re-creates (and hides) the native window.
    setWindowFlag(Qt::WindowDoesNotAcceptFocus, !accept);
    setAttribute(Qt::WA_ShowWithoutActivating, !accept);
    if (wasVisible) {
        move(p);
        show();
        raise();
    }
    d_changing_flags = false;
}

bool GraphMiniToolbar::enterKeyboardFocus()
{
    if (!isVisible())
        return false;
    if (!d_keyboard_mode) {
        QWidget *fw = QApplication::focusWidget();
        if (!fw && d_graph && d_graph->window())
            fw = d_graph->window()->focusWidget();
        if (fw && fw != this && !isAncestorOf(fw))
            d_return_focus = fw;
        else if (d_graph)
            d_return_focus = d_graph.data();
        d_keyboard_mode = true;
        d_activating = true;
        QTimer::singleShot(400, this, [this]() { d_activating = false; });
        setAcceptsFocus(true);
    }
    activateWindow();
    for (int i = 0; d_layout && i < d_layout->count(); ++i) {
        QToolButton *b = qobject_cast<QToolButton *>(d_layout->itemAt(i)->widget());
        if (b && !b->isHidden() && b->isEnabled()) {
            b->setFocus(Qt::TabFocusReason);
            break;
        }
    }
    return true;
}

void GraphMiniToolbar::leaveKeyboardFocus(bool returnFocus)
{
    const QPointer<QWidget> back = d_return_focus;
    d_return_focus.clear();
    d_keyboard_mode = false;
    d_activating = false;
    cancelScheduledAndHide();
    setAcceptsFocus(false);
    if (returnFocus && back) {
        if (QWidget *win = back->window())
            win->activateWindow();
        back->setFocus(Qt::OtherFocusReason);
    }
}

void GraphMiniToolbar::retranslateUi()
{
    applyThemeStyle();
    setToolTip(tr("Alt+F10: move the keyboard focus into this toolbar (Tab: next button, Esc: back to the graph)"));
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
    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        const int key = ke->key();
        if (insideSelf) {
            // H5: Esc leaves the bar and returns to the canvas
            if (key == Qt::Key_Escape && d_keyboard_mode) {
                leaveKeyboardFocus(true);
                return true;
            }
            break;
        }
        // a bare modifier is the start of Alt+F10 and must not dismiss the bar
        if (key == Qt::Key_Alt || key == Qt::Key_Control || key == Qt::Key_Shift
            || key == Qt::Key_Meta || key == Qt::Key_AltGr)
            break;
        const Qt::KeyboardModifiers mods = ke->modifiers()
                & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
        if (isVisible() && QKeySequence(int(mods) | key) == keyboardFocusShortcut()
            && enterKeyboardFocus()) {
            return true;
        }
        cancelScheduledAndHide();
        break;
    }
    case QEvent::WindowDeactivate:
        if (d_keyboard_mode || d_changing_flags)
            break; // the bar itself took the activation
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
        if (d_keyboard_mode || d_changing_flags) {
            // Moving the activation from the main window to the bar can report a short
            // deactivation; only leave when the application really stays in the background.
            QTimer::singleShot(400, this, [this]() {
                if (d_keyboard_mode
                    && QGuiApplication::applicationState() != Qt::ApplicationActive)
                    leaveKeyboardFocus(false);
            });
            break;
        }
        cancelScheduledAndHide();
        break;
    default:
        break;
    }
    return QFrame::eventFilter(watched, event);
}

void GraphMiniToolbar::hideEvent(QHideEvent *event)
{
    if (d_changing_flags) {
        // window re-created by setAcceptsFocus(); it is shown again right away
        QFrame::hideEvent(event);
        return;
    }
    if (d_keyboard_mode) {
        // dismissed by a click elsewhere while it had the keyboard: back to "never take focus"
        d_keyboard_mode = false;
        d_return_focus.clear();
        QTimer::singleShot(0, this, [this]() {
            if (!isVisible() && !d_keyboard_mode)
                setAcceptsFocus(false);
        });
    }
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

QColor GraphMiniToolbar::pickColorWithPalettePopup(QWidget *anchorWidget, const QColor &initial,
                                                   const QString &dialogTitle)
{
    if (!d_graph || ApplicationWindow::isNonInteractiveContext(d_graph))
        return QColor();

    Graph::registerPaletteTranslations();
    const QString defName = Graph::defaultCurvePaletteName();
    const QList<QColor> defColors = Graph::paletteColors(defName);
    const bool defIsOkabe = (Graph::canonicalPaletteName(defName) == Graph::okabeItoPaletteName());
    const QString altName =
            defIsOkabe ? Graph::toriGraphStandardPaletteName() : Graph::okabeItoPaletteName();
    const QList<QColor> altColors =
            defIsOkabe ? Graph::toriGraphStandardColors() : Graph::okabeItoColors();
    const QList<QColor> basicColors = {
        QColor(0x00, 0x00, 0x00), QColor(0x33, 0x41, 0x55), QColor(0x64, 0x74, 0x8B),
        QColor(0x94, 0xA3, 0xB8), QColor(0xFF, 0xFF, 0xFF), QColor(0xDC, 0x26, 0x26),
        QColor(0x1D, 0x4E, 0xD8), QColor(0x15, 0x80, 0x3D)
    };

    QMenu menu;
    QColor chosen;

    QWidget *panel = new QWidget(&menu);
    QVBoxLayout *vlay = new QVBoxLayout(panel);
    vlay->setContentsMargins(8, 6, 8, 8);
    vlay->setSpacing(5);

    auto addRow = [&](const QString &heading, const QList<QColor> &colors) {
        if (colors.isEmpty())
            return;
        QLabel *lbl = new QLabel(heading, panel);
        lbl->setStyleSheet(QStringLiteral(
                "QLabel { font-size: 8.5pt; font-weight: 600; color: #475569; background: transparent; }"));
        vlay->addWidget(lbl);

        QGridLayout *grid = new QGridLayout();
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(4);
        const int maxCols = 8;
        const int count = qMin(colors.size(), 16);
        for (int i = 0; i < count; ++i) {
            const QColor col = colors.at(i);
            QToolButton *sw = new QToolButton(panel);
            sw->setFixedSize(22, 22);
            sw->setAutoRaise(false);
            sw->setToolTip(col.name().toUpper());
            const bool isCur = initial.isValid() && (col.rgb() == initial.rgb());
            const QString borderCss = isCur ? QStringLiteral("2px solid #0F6CBD")
                                            : QStringLiteral("1px solid #94A3B8");
            sw->setStyleSheet(QStringLiteral(
                    "QToolButton { background-color: %1; border: %2; border-radius: 4px; "
                    "min-width: 20px; min-height: 20px; padding: 0; }"
                    "QToolButton:hover { border: 2px solid #0F6CBD; }")
                                      .arg(col.name(), borderCss));
            connect(sw, &QToolButton::clicked, &menu, [&chosen, &menu, col]() {
                chosen = col;
                menu.close();
            });
            grid->addWidget(sw, i / maxCols, i % maxCols);
        }
        vlay->addLayout(grid);
    };

    const QString defDisplay = QCoreApplication::translate("Graph", defName.toUtf8().constData());
    const QString altDisplay = QCoreApplication::translate("Graph", altName.toUtf8().constData());
    addRow(defDisplay, defColors);
    addRow(altDisplay, altColors);
    addRow(tr("Grayscale & Basic"), basicColors);

    QWidgetAction *wa = new QWidgetAction(&menu);
    wa->setDefaultWidget(panel);
    menu.addAction(wa);
    menu.addSeparator();
    QAction *customAct = menu.addAction(tr("Custom Color..."));

    QPoint menuPos = mapToGlobal(QPoint(0, height() + 2));
    if (anchorWidget)
        menuPos = anchorWidget->mapToGlobal(QPoint(0, anchorWidget->height() + 2));

    QPointer<GraphMiniToolbar> self(this);
    QPointer<Graph> safeG(d_graph);
    d_in_modal_dialog = true;
    QAction *pickedAct = menu.exec(menuPos);
    if (!self)
        return QColor();
    if (!chosen.isValid() && pickedAct == customAct && safeG) {
        QWidget *dlgParent = safeG->window();
        if (dlgParent == safeG.data())
            dlgParent = nullptr;
        chosen = QColorDialog::getColor(initial, dlgParent, dialogTitle);
        if (!self)
            return QColor();
    }
    d_in_modal_dialog = false;
    return safeG ? chosen : QColor();
}

void GraphMiniToolbar::onCurvePickColor()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    const QColor initial = (c->style() != QwtPlotCurve::NoCurve) ? c->pen().color()
                                                                 : c->symbol().pen().color();
    const QColor chosen = pickColorWithPalettePopup(
            qobject_cast<QWidget *>(sender()), initial, tr("Select Curve Color"));
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

    QMenu menu;
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

    QPointer<GraphMiniToolbar> self(this);
    QPointer<Graph> safeG(d_graph);
    d_in_modal_dialog = true;
    QAction *picked = menu.exec(menuPos);
    if (!self)
        return;
    d_in_modal_dialog = false;
    if (!picked || !safeG || d_curve_index < 0 || d_curve_index >= safeG->curves())
        return;

    c = safeG->curve(d_curve_index);
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
            st.penWidth = qMax(0.5, effectivePenWidthF(sym.pen(), 1.0));
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
            if (lp.widthF() <= 0.0 && lp.width() <= 0)
                lp.setWidthF(2.0);
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
        if (wasNoSym || (sp.widthF() <= 0.0 && sp.width() <= 0))
            sp.setWidthF(qMax(0.5, st.penWidth));
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
        st.penWidth = qMax(0.5, effectivePenWidthF(sym.pen(), 1.0));
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

void GraphMiniToolbar::onCurvePickLineWidth()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    const bool hasLine = (c->style() != QwtPlotCurve::NoCurve);
    const bool hasSym = (c->symbol().style() != QwtSymbol::NoSymbol);
    const double curW = hasLine ? effectivePenWidthF(c->pen(), 1.0) : 0.0;

    QMenu menu;
    if (hasSym) {
        QAction *offAct = menu.addAction(tr("None (Hide Line)"));
        offAct->setData(0.0);
        offAct->setCheckable(true);
        offAct->setChecked(!hasLine);
        menu.addSeparator();
    }
    const double presets[] = { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0 };
    for (double w : presets) {
        QAction *act = menu.addAction(QStringLiteral("%1 pt").arg(formatCompactPt(w)));
        act->setData(w);
        act->setCheckable(true);
        if (hasLine && std::fabs(curW - w) < 0.05)
            act->setChecked(true);
    }

    QPoint menuPos = mapToGlobal(QPoint(0, height() + 2));
    if (auto *senderBtn = qobject_cast<QWidget *>(sender()))
        menuPos = senderBtn->mapToGlobal(QPoint(0, senderBtn->height() + 2));

    QPointer<GraphMiniToolbar> self(this);
    QPointer<Graph> safeG(d_graph);
    d_in_modal_dialog = true;
    QAction *picked = menu.exec(menuPos);
    if (!self)
        return;
    d_in_modal_dialog = false;
    if (!picked || !safeG || d_curve_index < 0 || d_curve_index >= safeG->curves())
        return;

    c = safeG->curve(d_curve_index);
    if (!c)
        return;

    const double nextW = picked->data().toDouble();
    if (nextW <= 0.0) {
        if (c->symbol().style() != QwtSymbol::NoSymbol)
            applyCurveLineSymbolMode(1); // Scatter
        return;
    }

    const QString beforeSnap = beginGraphUndo();
    const bool wasNoLine = (c->style() == QwtPlotCurve::NoCurve);
    if (wasNoLine) {
        c->setStyle(QwtPlotCurve::Lines);
        const bool symNow = (c->symbol().style() != QwtSymbol::NoSymbol);
        safeG->setCurveType(d_curve_index, symNow ? Graph::LineSymbols : Graph::Line, false);
        c = safeG->curve(d_curve_index);
        if (!c)
            return;
        c->setStyle(QwtPlotCurve::Lines);
    }
    QPen pen = c->pen();
    if (pen.style() == Qt::NoPen)
        pen.setStyle(Qt::SolidLine);
    if (wasNoLine && !pen.color().isValid())
        pen.setColor(c->symbol().pen().color().isValid() ? c->symbol().pen().color()
                                                         : QColor(Qt::black));
    pen.setWidthF(qBound(0.5, nextW, 20.0));
    c->setPen(pen);
    safeG->reconstructLegend();
    safeG->replot();
    safeG->notifyChanges();
    commitGraphUndo(tr("Change Line Width"), beforeSnap, kMergeMiniWidth);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
}

void GraphMiniToolbar::onCurveLineWidthDec()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c || c->style() == QwtPlotCurve::NoCurve)
        return;
    const QString beforeSnap = beginGraphUndo();
    QPen pen = c->pen();
    const double curW = effectivePenWidthF(pen, 1.0);
    const double nextW = qBound(0.5, std::round((curW - 0.5) * 10.0) / 10.0, 20.0);
    pen.setWidthF(nextW);
    c->setPen(pen);
    d_graph->reconstructLegend();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Line Width"), beforeSnap, kMergeMiniWidth);
    if (isVisible()) {
        rebuildCurrentButtons();
        adjustSize();
        reclampToAvailableBounds();
    }
}

void GraphMiniToolbar::onCurveLineWidthInc()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;
    const QString beforeSnap = beginGraphUndo();
    const bool wasNoLine = (c->style() == QwtPlotCurve::NoCurve);
    if (wasNoLine) {
        const bool hasSym = (c->symbol().style() != QwtSymbol::NoSymbol);
        d_graph->setCurveType(d_curve_index, hasSym ? Graph::LineSymbols : Graph::Line, false);
        c = d_graph->curve(d_curve_index);
        if (!c)
            return;
        c->setStyle(QwtPlotCurve::Lines);
    }
    QPen pen = c->pen();
    if (pen.style() == Qt::NoPen)
        pen.setStyle(Qt::SolidLine);
    if (wasNoLine && !pen.color().isValid())
        pen.setColor(c->symbol().pen().color().isValid() ? c->symbol().pen().color()
                                                         : QColor(Qt::black));
    const double curW = wasNoLine ? 1.0 : effectivePenWidthF(pen, 1.0);
    const double nextW = qBound(0.5, std::round((curW + 0.5) * 10.0) / 10.0, 20.0);
    pen.setWidthF(nextW);
    c->setPen(pen);
    d_graph->reconstructLegend();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Line Width"), beforeSnap, kMergeMiniWidth);
    if (isVisible()) {
        rebuildCurrentButtons();
        adjustSize();
        reclampToAvailableBounds();
    }
}

void GraphMiniToolbar::onCurvePickSymbolSize()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c)
        return;

    const bool hasLine = (c->style() != QwtPlotCurve::NoCurve);
    const bool hasSym = (c->symbol().style() != QwtSymbol::NoSymbol);
    const int curSz = hasSym ? qMax(1, c->symbol().size().width()) : 0;

    QMenu menu;
    if (hasLine) {
        QAction *offAct = menu.addAction(tr("None (No Symbol)"));
        offAct->setData(0);
        offAct->setCheckable(true);
        offAct->setChecked(!hasSym);
        menu.addSeparator();
    }
    const int presets[] = { 4, 5, 6, 7, 8, 10, 12, 16 };
    for (int sz : presets) {
        QAction *act = menu.addAction(QStringLiteral("%1 pt").arg(sz));
        act->setData(sz);
        act->setCheckable(true);
        if (hasSym && curSz == sz)
            act->setChecked(true);
    }

    QPoint menuPos = mapToGlobal(QPoint(0, height() + 2));
    if (auto *senderBtn = qobject_cast<QWidget *>(sender()))
        menuPos = senderBtn->mapToGlobal(QPoint(0, senderBtn->height() + 2));

    QPointer<GraphMiniToolbar> self(this);
    QPointer<Graph> safeG(d_graph);
    d_in_modal_dialog = true;
    QAction *picked = menu.exec(menuPos);
    if (!self)
        return;
    d_in_modal_dialog = false;
    if (!picked || !safeG || d_curve_index < 0 || d_curve_index >= safeG->curves())
        return;

    c = safeG->curve(d_curve_index);
    if (!c)
        return;

    const int nextSz = picked->data().toInt();
    if (nextSz <= 0) {
        if (c->style() != QwtPlotCurve::NoCurve)
            applyCurveLineSymbolMode(0); // Line only
        return;
    }

    const QString beforeSnap = beginGraphUndo();
    QwtSymbol sym = c->symbol();
    const bool wasNoSym = (sym.style() == QwtSymbol::NoSymbol);
    if (wasNoSym) {
        const long key = safeG->curveKey(d_curve_index);
        const SavedSymbolState st = d_saved_symbol_states.value(key);
        const QColor col = c->pen().color().isValid() ? c->pen().color() : QColor(Qt::black);
        sym.setStyle(st.style != (int)QwtSymbol::NoSymbol
                             ? static_cast<QwtSymbol::Style>(st.style)
                             : QwtSymbol::Ellipse);
        QPen sp(col);
        sp.setWidthF(qMax(0.5, st.penWidth));
        sym.setPen(sp);
        if (st.hollowWhite)
            sym.setBrush(QBrush(Qt::white));
        else if (st.noFill)
            sym.setBrush(Qt::NoBrush);
        else
            sym.setBrush(QBrush(col));
    }
    const int clampedSz = qBound(PlotStyleLimits::kSymbolSizeMin, nextSz,
                                 PlotStyleLimits::kSymbolSizeMax);
    sym.setSize(QSize(clampedSz, clampedSz));
    c->setSymbol(sym);
    if (wasNoSym) {
        const bool lineNow = (c->style() != QwtPlotCurve::NoCurve);
        safeG->setCurveType(d_curve_index, lineNow ? Graph::LineSymbols : Graph::Scatter, false);
        if (QwtPlotCurve *after = safeG->curve(d_curve_index))
            after->setSymbol(sym);
    }
    safeG->reconstructLegend();
    safeG->replot();
    safeG->notifyChanges();
    commitGraphUndo(tr("Change Symbol Size"), beforeSnap, kMergeMiniSymSize);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
}

void GraphMiniToolbar::onCurveSymbolSizeDec()
{
    if (!d_graph || d_curve_index < 0 || d_curve_index >= d_graph->curves())
        return;
    QwtPlotCurve *c = d_graph->curve(d_curve_index);
    if (!c || c->symbol().style() == QwtSymbol::NoSymbol)
        return;
    const QString beforeSnap = beginGraphUndo();
    QwtSymbol sym = c->symbol();
    const int sz = qMax(3, sym.size().width() - 2);
    sym.setSize(QSize(sz, sz));
    c->setSymbol(sym);
    d_graph->reconstructLegend();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Symbol Size"), beforeSnap, kMergeMiniSymSize);
    if (isVisible()) {
        rebuildCurrentButtons();
        adjustSize();
        reclampToAvailableBounds();
    }
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
        const long key = d_graph->curveKey(d_curve_index);
        const SavedSymbolState st = d_saved_symbol_states.value(key);
        const QColor col = c->pen().color().isValid() ? c->pen().color() : QColor(Qt::black);
        sym.setStyle(st.style != (int)QwtSymbol::NoSymbol
                             ? static_cast<QwtSymbol::Style>(st.style)
                             : QwtSymbol::Ellipse);
        QPen sp(col);
        sp.setWidthF(qMax(0.5, st.penWidth));
        sym.setPen(sp);
        if (st.hollowWhite)
            sym.setBrush(QBrush(Qt::white));
        else if (st.noFill)
            sym.setBrush(Qt::NoBrush);
        else
            sym.setBrush(QBrush(col));
    }
    const int baseSz = wasNoSym ? 5 : sym.size().width();
    const int sz = qBound(5, baseSz + 2, PlotStyleLimits::kSymbolSizeMax);
    sym.setSize(QSize(sz, sz));
    c->setSymbol(sym);
    if (wasNoSym) {
        const bool hasLine = (c->style() != QwtPlotCurve::NoCurve);
        d_graph->setCurveType(d_curve_index, hasLine ? Graph::LineSymbols : Graph::Scatter, false);
        if (QwtPlotCurve *after = d_graph->curve(d_curve_index))
            after->setSymbol(sym);
    }
    d_graph->reconstructLegend();
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Change Symbol Size"), beforeSnap, kMergeMiniSymSize);
    if (isVisible()) {
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
        st.penWidth = qMax(0.5, effectivePenWidthF(origSym.pen(), 1.0));
        st.hollowWhite = Graph::isSymbolWhiteHollow(origSym);
        st.noFill = (!st.hollowWhite && origSym.brush().style() == Qt::NoBrush);
        d_saved_symbol_states.insert(key, st);
    } else if (st.style == (int)QwtSymbol::NoSymbol) {
        st.style = (int)QwtSymbol::Ellipse;
        st.size = 7;
        st.penWidth = 1.0;
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
            QPen sp(col);
            sp.setWidthF(qMax(0.5, st.penWidth));
            sym.setPen(sp);
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
        QPen sp(col);
        sp.setWidthF(qMax(0.5, st.penWidth));
        QwtSymbol sym(st.style != (int)QwtSymbol::NoSymbol
                              ? static_cast<QwtSymbol::Style>(st.style)
                              : QwtSymbol::Ellipse,
                      QBrush(Qt::white), sp,
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
        st.penWidth = qMax(0.5, effectivePenWidthF(after->symbol().pen(), 1.0));
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
            } else {
                // P0 (A3-1): Never silently mutate the source Table or invent fake constant-SD
                // error bars without user choice. Offer explicit Error Bars / Error Band dialogs.
                if (!app || ApplicationWindow::isNonInteractiveContext(d_graph)) {
                    if (app && app->statusBar()) {
                        app->statusBar()->showMessage(
                                tr("No adjacent Y-error column found. Open Add Error Bars... to "
                                   "choose an error column or percentage."),
                                5000);
                    }
                    rebuildCurrentButtons();
                    adjustSize();
                    reclampToAvailableBounds();
                    return;
                }

                QMenu menu;
                QAction *actBars = menu.addAction(
                        IconLoader::duotoneIcon(QStringLiteral("error-bars"), 16),
                        tr("Add Error Bars..."));
                QAction *actBand = menu.addAction(
                        IconLoader::duotoneIcon(QStringLiteral("error-bars"), 16),
                        tr("Add Error Band..."));

                QPoint menuPos = mapToGlobal(QPoint(0, height() + 2));
                if (auto *senderBtn = qobject_cast<QWidget *>(sender()))
                    menuPos = senderBtn->mapToGlobal(QPoint(0, senderBtn->height() + 2));

                QPointer<GraphMiniToolbar> self(this);
                QPointer<Graph> safeG(d_graph);
                QPointer<ApplicationWindow> safeApp(app);
                d_in_modal_dialog = true;
                QAction *picked = menu.exec(menuPos);
                if (!self)
                    return;
                d_in_modal_dialog = false;

                if (picked && safeG && safeApp) {
                    cancelScheduledAndHide();
                    if (MultiLayer *targetMl = safeG->multiLayerParent()) {
                        targetMl->setActiveGraph(safeG.data());
                        safeApp->activateSubWindow(targetMl);
                    }
                    if (picked == actBars)
                        safeApp->addErrorBars();
                    else if (picked == actBand)
                        ExtraPlotTypes::showErrorBandDialog(safeApp.data());
                } else if (safeG && isVisible()) {
                    rebuildCurrentButtons();
                    adjustSize();
                    reclampToAvailableBounds();
                }
                return;
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
        if (d_graph->runWithColumnMaskUndo(tr("Clear Curve Masks"),
                                           QList<DataCurve *>() << dc,
                                           [&]() { dc->clearMasks(); })) {
            d_graph->updateStatReferenceLines();
            d_graph->replot();
            d_graph->notifyChanges();
        }
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

void GraphMiniToolbar::onAxisToggleZeroYLine()
{
    if (!d_graph)
        return;
    GraphInspectorDock::toggleZeroReferenceLine(d_graph, 0);
    rebuildCurrentButtons();
    adjustSize();
    reclampToAvailableBounds();
}

void GraphMiniToolbar::onAxisToggleZeroXLine()
{
    if (!d_graph)
        return;
    GraphInspectorDock::toggleZeroReferenceLine(d_graph, 1);
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
    if (ApplicationWindow *app = findAppWindow(d_graph)) {
        if (GraphInspectorDock *dock = app->graphInspectorDock())
            dock->refreshAll();
    }
}

void GraphMiniToolbar::onAxisAddRefLineDialog()
{
    if (!d_graph)
        return;
    const int orient =
            (d_axis_id == QwtPlot::yLeft || d_axis_id == QwtPlot::yRight) ? 0 : 1;
    hide();
    GraphInspectorDock::addReferenceLineInteractive(d_graph, orient);
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
    const double tickPt = (tickFont.pointSizeF() > 0.0)
            ? tickFont.pointSizeF()
            : (tickFont.pointSize() > 0 ? double(tickFont.pointSize()) : 12.0);
    tickFont.setPointSizeF(qBound(PlotStyleLimits::kFontSizeMin,
                                  std::round((tickPt + double(deltaPt)) * 10.0) / 10.0,
                                  PlotStyleLimits::kFontSizeMax));
    d_graph->setAxisFont(d_axis_id, tickFont);

    QFont titleFont = d_graph->axisTitleFont(d_axis_id);
    if (titleFont.family().isEmpty())
        titleFont.setFamily(QStringLiteral("Arial"));
    const double titlePt = (titleFont.pointSizeF() > 0.0)
            ? titleFont.pointSizeF()
            : (titleFont.pointSize() > 0 ? double(titleFont.pointSize()) : 14.0);
    titleFont.setPointSizeF(qBound(PlotStyleLimits::kFontSizeMin,
                                   std::round((titlePt + double(deltaPt)) * 10.0) / 10.0,
                                   PlotStyleLimits::kFontSizeMax));
    d_graph->setAxisTitleFont(d_axis_id, titleFont);

    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Adjust Axis Font Size"), beforeSnap, kMergeMiniFont);
    if (isVisible()) {
        rebuildCurrentButtons();
        adjustSize();
        reclampToAvailableBounds();
    }
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
    const QColor chosen = pickColorWithPalettePopup(
            qobject_cast<QWidget *>(sender()), initial, tr("Select Text Color"));
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

// C16: the toggle works line by line on the label part of each legend line, i.e. after the
// curve-symbol tokens (\c{n}, \l(n), \p{n}). Wrapping the whole multi-line text put the
// opening tag in front of "\c{1}", where it had no effect, and left stray tags behind.
// Rules per line: if the label already ends with the closing tag the last tagged part is
// unwrapped (only when every non-empty label is tagged, so one press toggles all lines
// consistently); otherwise a trailing number ("x2", "10^-3") or the second character of a
// two-character symbol ("Hv") is wrapped, else the whole label is.
QString GraphMiniToolbar::toggleMarkupPerLine(const QString &text, const QString &openTag,
                                              const QString &closeTag)
{
    static const QRegularExpression reTokens(
            QStringLiteral("^(\\s*(?:\\\\[cp]\\{[^}]*\\}|\\\\l\\([^)]*\\))\\s*)*"));
    static const QRegularExpression reDigits(QStringLiteral("^(.+\\S)([+-]?[0-9]+)$"));
    static const QRegularExpression reTwoCharSym(QStringLiteral("^([^\\s<>])([A-Za-z])$"));

    if (text.trimmed().isEmpty())
        return QStringLiteral("x") + openTag + QStringLiteral("2") + closeTag;

    QStringList lines = text.split(QLatin1Char('\n'));
    QVector<int> labelStart(lines.size(), 0);
    bool anyLabel = false;
    bool allTagged = true;
    for (int i = 0; i < lines.size(); ++i) {
        const QRegularExpressionMatch m = reTokens.match(lines.at(i));
        labelStart[i] = m.hasMatch() ? m.capturedLength(0) : 0;
        QString label = lines.at(i).mid(labelStart[i]);
        if (label.endsWith(QLatin1Char('\r')))
            label.chop(1);
        if (label.trimmed().isEmpty())
            continue;
        anyLabel = true;
        if (!label.trimmed().endsWith(closeTag))
            allTagged = false;
    }
    if (!anyLabel)
        return text;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        QString cr;
        if (line.endsWith(QLatin1Char('\r'))) {
            cr = QStringLiteral("\r");
            line.chop(1);
        }
        const QString prefix = line.left(labelStart[i]);
        QString label = line.mid(labelStart[i]);
        if (label.trimmed().isEmpty())
            continue;
        // Keep trailing spaces outside the markup.
        int end = label.size();
        while (end > 0 && label.at(end - 1).isSpace())
            --end;
        const QString tail = label.mid(end);
        label.truncate(end);

        if (allTagged) {
            const int openPos = label.lastIndexOf(openTag);
            if (openPos >= 0) {
                const int innerStart = openPos + openTag.length();
                const int innerLen = label.length() - closeTag.length() - innerStart;
                label = label.left(openPos) + label.mid(innerStart, qMax(0, innerLen));
            }
        } else if (!label.endsWith(closeTag)) {
            const QRegularExpressionMatch mDigits = reDigits.match(label);
            const QRegularExpressionMatch mSym = reTwoCharSym.match(label);
            if (mDigits.hasMatch() && !mDigits.captured(1).endsWith(QLatin1Char('>')))
                label = mDigits.captured(1) + openTag + mDigits.captured(2) + closeTag;
            else if (mSym.hasMatch())
                label = mSym.captured(1) + openTag + mSym.captured(2) + closeTag;
            else
                label = openTag + label + closeTag;
        }
        lines[i] = prefix + label + tail + cr;
    }
    return lines.join(QLatin1Char('\n'));
}

void GraphMiniToolbar::applyLegendMarkupToggle(const QString &openTag, const QString &closeTag,
                                               const QString &undoLabel)
{
    if (!d_graph || !isLegendValid())
        return;

    const QString cur = d_legend->text();
    const QString updated = toggleMarkupPerLine(cur, openTag, closeTag);
    if (updated == cur)
        return;
    const QString beforeSnap = beginGraphUndo();

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

void GraphMiniToolbar::onLegendEditInline()
{
    if (!d_graph || !isLegendValid())
        return;
    Plot *p = d_graph->plotWidget();
    if (!p || !p->canvas())
        return;

    long markerKey = -1;
    for (int k : p->markerKeys()) {
        if (p->marker(k) == d_legend) {
            markerKey = k;
            break;
        }
    }
    if (markerKey < 0)
        return;

    const bool isMainLegend = (d_legend == d_graph->legend());
    const QRect legRect = d_legend->rect();
    const QRect anchorGlobal(p->canvas()->mapToGlobal(legRect.topLeft()), legRect.size());

    if (!isMainLegend) {
        QPointer<Graph> safeG(d_graph);
        cancelScheduledAndHide();
        if (safeG)
            CanvasInlineTextEditor::openForLegendOrText(safeG.data(), markerKey, -1, anchorGlobal);
        return;
    }

    const QStringList lines = d_legend->text().split(QLatin1Char('\n'));
    QVector<int> nonEmptyIndices;
    for (int i = 0; i < lines.size(); ++i) {
        if (!lines.at(i).trimmed().isEmpty())
            nonEmptyIndices.append(i);
    }

    int chosenLine = nonEmptyIndices.isEmpty() ? 0 : nonEmptyIndices.first();
    if (nonEmptyIndices.size() > 1 && !ApplicationWindow::isNonInteractiveContext(d_graph)) {
        QMenu menu;
        for (int idx : nonEmptyIndices) {
            QString preview = readableLegendLinePreview(d_graph.data(), lines.at(idx));
            if (preview.length() > 36)
                preview = preview.left(35) + QStringLiteral("\u2026");
            if (preview.isEmpty())
                preview = QStringLiteral("#%1").arg(idx + 1);
            QAction *act = menu.addAction(tr("Line %1: %2").arg(idx + 1).arg(preview));
            act->setData(idx);
        }
        QPoint menuPos = mapToGlobal(QPoint(0, height() + 2));
        if (auto *senderBtn = qobject_cast<QWidget *>(sender()))
            menuPos = senderBtn->mapToGlobal(QPoint(0, senderBtn->height() + 2));
        QPointer<GraphMiniToolbar> self(this);
        QPointer<Graph> safeG(d_graph);
        d_in_modal_dialog = true;
        QAction *picked = menu.exec(menuPos);
        if (!self)
            return;
        d_in_modal_dialog = false;
        if (!picked || !safeG)
            return;
        chosenLine = picked->data().toInt();
    }

    QPointer<Graph> safeG(d_graph);
    cancelScheduledAndHide();
    if (safeG)
        CanvasInlineTextEditor::openForLegendOrText(safeG.data(), markerKey, chosenLine,
                                                    anchorGlobal);
}

void GraphMiniToolbar::adjustLegendFont(int deltaPt)
{
    if (!isLegendValid())
        return;
    const QString beforeSnap = beginGraphUndo();
    QFont f = d_legend->font();
    if (f.family().isEmpty())
        f.setFamily(QStringLiteral("Arial"));
    const double pt = (f.pointSizeF() > 0.0)
            ? f.pointSizeF()
            : (f.pointSize() > 0 ? double(f.pointSize()) : 12.0);
    f.setPointSizeF(qBound(PlotStyleLimits::kFontSizeMin,
                           std::round((pt + double(deltaPt)) * 10.0) / 10.0,
                           PlotStyleLimits::kFontSizeMax));
    d_legend->setFont(f);
    d_graph->replot();
    d_graph->notifyChanges();
    commitGraphUndo(tr("Adjust Legend Font Size"), beforeSnap, kMergeMiniFont);
    if (isVisible()) {
        rebuildCurrentButtons();
        adjustSize();
        reclampToAvailableBounds();
    }
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
    const bool hasCustomEdits = isLegendValid()
            && d_legend->text().trimmed() != d_graph->legendText().trimmed();
    if (hasCustomEdits && !ApplicationWindow::isNonInteractiveContext(d_graph)) {
        QPointer<GraphMiniToolbar> self(this);
        QPointer<Graph> safeG(d_graph);
        QWidget *dlgParent = safeG ? safeG->window() : nullptr;
        if (dlgParent == safeG.data())
            dlgParent = nullptr;
        d_in_modal_dialog = true;
        const QMessageBox::StandardButton ans = QMessageBox::question(
                dlgParent, tr("Reconstruct Legend"),
                tr("Reset the legend to the default curve names? Manual edits to legend labels "
                   "will be overwritten."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (!self)
            return;
        d_in_modal_dialog = false;
        if (ans != QMessageBox::Yes || !safeG)
            return;
    }
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

void GraphMiniToolbar::onOpenDetails()
{
    if (!d_graph)
        return;
    ApplicationWindow *app = findAppWindow(d_graph);
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;

    QPointer<Graph> safeG(d_graph);
    QPointer<ApplicationWindow> safeApp(app);
    const TargetType target = d_target;
    const int axisId = d_axis_id;
    const int curveIdx = d_curve_index;
    Legend *leg = d_legend;
    long markerKey = -1;
    if (target == TargetLegend && leg && safeG->plotWidget()) {
        for (int k : safeG->plotWidget()->markerKeys()) {
            if (safeG->plotWidget()->marker(k) == leg) {
                markerKey = k;
                break;
            }
        }
    }

    cancelScheduledAndHide();
    if (!safeG || !safeApp)
        return;

    MultiLayer *ml = safeG->multiLayerParent();
    if (ml) {
        ml->setActiveGraph(safeG.data());
        safeApp->activateSubWindow(ml);
    }

    switch (target) {
    case TargetCanvas:
        safeApp->showGeneralPlotDialog();
        break;
    case TargetAxis: {
        // C2-1: always select the exact QwtPlot::Axis via setCurrentQwtAxis()
        auto &ad = safeApp->addChild<AxesDialog>();
        ad.setAttribute(Qt::WA_DeleteOnClose, true);
        connect(&ad, &QDialog::finished, &ad, &QObject::deleteLater);
        if (ml)
            ad.setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
        ad.setGraph(safeG.data());
        ad.showAxesPage();
        ad.setCurrentQwtAxis(qBound(0, axisId, QwtPlot::axisCnt - 1));
        ad.exec();
        break;
    }
    case TargetCurve: {
        const int resolved = resolveMasterCurveIndex(safeG.data(), curveIdx);
        const int key = (resolved >= 0 && resolved < safeG->curves())
                ? int(safeG->curveKey(resolved))
                : -1;
        safeApp->showPlotDialog(key);
        break;
    }
    case TargetLegend:
        if (markerKey >= 0) {
            safeG->setSelectedMarker(markerKey);
            safeApp->showTextDialog();
        }
        break;
    }
}
