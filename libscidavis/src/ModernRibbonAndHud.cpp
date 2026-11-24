/***************************************************************************
    File                 : ModernRibbonAndHud.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Description          : Hybrid Modern Contextual Single-Row Slim Ribbon,
                           Ctrl+K Command Palette, Split Workspace Controller,
                           and F11 Non-Redundant 2-Stage Focus Canvas HUD
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

#include "ModernRibbonAndHud.h"
#include "UxNavigation.h"
#include "Workbook.h"
#include "XlsxImport.h"

#include <QToolTip>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QIdentityProxyModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QListWidgetItem>
#include <QLocale>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QMimeData>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "ApplicationWindow.h"
#include "CurveMathTools.h"
#include "ExtraPlotTypes.h"
#include "SettingsTransfer.h"
#include "Graph.h"
#include "GraphInspectorDock.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "MyWidget.h"
#include "Note.h"
#include "PlotCurve.h"
#include "RecalcEngine.h"
#include "Table.h"
#include "Graph3D.h"
#include "OleServer.h"
#include "PptxExport.h"
#include "I18nJapanese.h"
#include "future/core/column/Column.h"
#include "future/table/TableModel.h"
#include "future/table/future_Table.h"

#include <QDateTime>
#include <QShortcut>
#include <mutex>

namespace {

enum PaletteItemRoles {
    RoleEntryIndex = Qt::UserRole + 1,
    RoleShortcutText = Qt::UserRole + 2,
    RoleCategoryText = Qt::UserRole + 3,
    RoleDisabledEntry = Qt::UserRole + 4,
    RoleDisabledReason = Qt::UserRole + 5
};

static QString cleanMnemonicText(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        if (raw.at(i) == QLatin1Char('&')) {
            if (i + 1 < raw.size() && raw.at(i + 1) == QLatin1Char('&')) {
                out.append(QLatin1Char('&'));
                ++i;
            }
            continue;
        }
        out.append(raw.at(i));
    }
    return out.trimmed();
}

static Table *sourceTableFromMultiLayer(MultiLayer *ml,
                                        const QSet<QObject *> &ignored = QSet<QObject *>())
{
    if (!ml)
        return nullptr;

    auto extractFromGraph = [&ignored](Graph *g) -> Table * {
        if (!g || g->curves() <= 0)
            return nullptr;
        const int selIdx = g->selectedCurveIndex();
        if (selIdx >= 0 && selIdx < g->curves()) {
            if (DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(selIdx))) {
                if (dc->table() && !ignored.contains(dc->table()))
                    return dc->table();
            }
        }
        for (int i = 0; i < g->curves(); ++i) {
            if (DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(i))) {
                if (dc->table() && !ignored.contains(dc->table()))
                    return dc->table();
            }
        }
        return nullptr;
    };

    if (Table *t = extractFromGraph(ml->activeGraph()))
        return t;

    const QList<Graph *> layers = ml->layersList();
    for (Graph *g : layers) {
        if (Table *t = extractFromGraph(g))
            return t;
    }
    return nullptr;
}

static bool multiLayerUsesTable(MultiLayer *ml, Table *targetTable)
{
    if (!ml || !targetTable)
        return false;
    const QList<Graph *> layers = ml->layersList();
    for (Graph *g : layers) {
        if (!g)
            continue;
        for (int i = 0; i < g->curves(); ++i) {
            if (DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(i))) {
                if (dc->table() == targetTable)
                    return true;
            }
        }
    }
    return false;
}

class CommandPaletteItemDelegate : public QStyledItemDelegate
{
public:
    explicit CommandPaletteItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        const QString reason = index.data(RoleDisabledReason).toString();
        return QSize(480, reason.isEmpty() ? 38 : 54);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect rowRect = option.rect.adjusted(4, 2, -4, -2);
        const bool selected = (option.state & QStyle::State_Selected);
        const bool hovered = (option.state & QStyle::State_MouseOver);

        if (selected) {
            painter->setPen(QPen(QColor(37, 99, 235, 90), 1.0));
            painter->setBrush(QColor(239, 246, 255));
            painter->drawRoundedRect(rowRect, 7.0, 7.0);
        } else if (hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(248, 250, 252));
            painter->drawRoundedRect(rowRect, 7.0, 7.0);
        }

        const QString shortcut = index.data(RoleShortcutText).toString();
        const QString category = index.data(RoleCategoryText).toString();
        const QString title = index.data(Qt::DisplayRole).toString();
        const QString reason = index.data(RoleDisabledReason).toString();
        const bool hasReason = !reason.isEmpty();
        const bool dimmed = index.data(RoleDisabledEntry).toBool();

        if (dimmed)
            painter->setOpacity(0.68);

        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const int iconPx = 20;
        const QRect iconRect(rowRect.left() + 10,
                             rowRect.top() + (rowRect.height() - iconPx) / 2, iconPx, iconPx);
        if (!icon.isNull())
            icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);

        const int topH = hasReason ? 26 : rowRect.height();
        int rightEdge = rowRect.right() - 10;
        if (!shortcut.isEmpty()) {
            QFont badgeFont(QStringLiteral("Segoe UI"), 10);
            badgeFont.setWeight(QFont::DemiBold);
            painter->setFont(badgeFont);
            const QFontMetrics bfm(badgeFont);
            const int bw = bfm.horizontalAdvance(shortcut) + 14;
            const int bh = 22;
            const QRect badgeRect(rightEdge - bw,
                                  rowRect.top() + (topH - bh) / 2, bw, bh);
            painter->setPen(QPen(QColor(203, 213, 225), 1.0));
            painter->setBrush(selected ? QColor(255, 255, 255) : QColor(241, 245, 249));
            painter->drawRoundedRect(badgeRect, 5.0, 5.0);
            painter->setPen(QColor(71, 85, 105));
            painter->drawText(badgeRect, Qt::AlignCenter, shortcut);
            rightEdge = badgeRect.left() - 8;
        }

        if (!category.isEmpty()) {
            QFont catFont(QStringLiteral("Segoe UI"), 10);
            painter->setFont(catFont);
            const QFontMetrics cfm(catFont);
            const int cw = qMin(148, cfm.horizontalAdvance(category) + 6);
            const QRect catRect(rightEdge - cw, rowRect.top(), cw, topH);
            painter->setPen(selected ? QColor(37, 99, 235) : QColor(100, 116, 139));
            painter->drawText(catRect, Qt::AlignRight | Qt::AlignVCenter,
                              cfm.elidedText(category, Qt::ElideRight, cw));
            rightEdge = catRect.left() - 8;
        }

        QFont titleFont(QStringLiteral("Segoe UI"), 11);
        titleFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        painter->setFont(titleFont);
        painter->setPen(dimmed ? QColor(71, 85, 105)
                               : (selected ? QColor(29, 78, 216) : QColor(15, 23, 42)));
        const int textLeft = iconRect.right() + 10;
        const int availW = qMax(40, rightEdge - textLeft);
        const QFontMetrics tfm(titleFont);
        const QRect textRect(textLeft, rowRect.top(), availW, topH);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          tfm.elidedText(title, Qt::ElideRight, availW));

        if (hasReason) {
            painter->setOpacity(1.0);
            QFont reasonFont(QStringLiteral("Segoe UI"), 9);
            painter->setFont(reasonFont);
            painter->setPen(selected ? QColor(51, 65, 85) : QColor(71, 85, 105));
            const int reasonW = qMax(60, (rowRect.right() - 10) - textLeft);
            const QFontMetrics rfm(reasonFont);
            const QRect reasonRect(textLeft, rowRect.top() + 24, reasonW,
                                   qMax(16, rowRect.height() - 24));
            painter->drawText(reasonRect, Qt::AlignLeft | Qt::AlignVCenter,
                              rfm.elidedText(reason, Qt::ElideRight, reasonW));
        }

        painter->restore();
    }
};

static QString escapeTabMnemonicText(const QString &raw)
{
    QString out;
    out.reserve(raw.size() + 4);
    for (int i = 0; i < raw.size(); ++i) {
        const QChar ch = raw.at(i);
        if (ch == QLatin1Char('&')) {
            if (i + 1 < raw.size() && raw.at(i + 1) == QLatin1Char('&')) {
                out.append(QStringLiteral("&&"));
                ++i;
            } else {
                out.append(QStringLiteral("&&"));
            }
        } else {
            out.append(ch);
        }
    }
    return out;
}

static QString unescapeTabMnemonicText(const QString &raw)
{
    QString out = raw;
    out.replace(QStringLiteral("&&"), QStringLiteral("&"));
    return out;
}

class RibbonHorizontalScrollArea : public QScrollArea
{
public:
    explicit RibbonHorizontalScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setWidgetResizable(true);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setFocusPolicy(Qt::NoFocus);

        const QString chevronStyle = QStringLiteral(
                "QToolButton {"
                "  background-color: rgba(248, 250, 252, 242);"
                "  color: #1E293B;"
                "  border: 1px solid #CBD5E1;"
                "  border-radius: 6px;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', sans-serif;"
                "  font-size: 9pt;"
                "  font-weight: 700;"
                "  padding: 0px;"
                "}"
                "QToolButton:hover {"
                "  background-color: #EFF6FF;"
                "  color: #1D4ED8;"
                "  border: 1px solid #60A5FA;"
                "}"
                "QToolButton:disabled {"
                "  color: #CBD5E1;"
                "  background-color: rgba(248, 250, 252, 180);"
                "}");

        d_btn_left = new QToolButton(this);
        d_btn_left->setText(QString::fromUtf8("\xE2\x80\xB9"));
        d_btn_left->setCursor(Qt::PointingHandCursor);
        d_btn_left->setFocusPolicy(Qt::NoFocus);
        d_btn_left->setAutoRepeat(true);
        d_btn_left->setStyleSheet(chevronStyle);
        d_btn_left->hide();

        d_btn_right = new QToolButton(this);
        d_btn_right->setText(QString::fromUtf8("\xE2\x80\xBA"));
        d_btn_right->setCursor(Qt::PointingHandCursor);
        d_btn_right->setFocusPolicy(Qt::NoFocus);
        d_btn_right->setAutoRepeat(true);
        d_btn_right->setStyleSheet(chevronStyle);
        d_btn_right->hide();

        connect(d_btn_left, &QToolButton::clicked, this, [this]() {
            if (QScrollBar *hbar = horizontalScrollBar())
                hbar->setValue(qMax(hbar->minimum(), hbar->value() - 180));
        });
        connect(d_btn_right, &QToolButton::clicked, this, [this]() {
            if (QScrollBar *hbar = horizontalScrollBar())
                hbar->setValue(qMin(hbar->maximum(), hbar->value() + 180));
        });

        if (QScrollBar *hbar = horizontalScrollBar()) {
            connect(hbar, &QScrollBar::rangeChanged, this,
                    [this](int, int) { updateScrollChevrons(); });
            connect(hbar, &QScrollBar::valueChanged, this,
                    [this](int) { updateScrollChevrons(); });
        }
    }

    void updateScrollChevrons()
    {
        if (d_updating_chevrons)
            return;
        QScrollBar *hbar = horizontalScrollBar();
        if (!hbar || !d_btn_left || !d_btn_right)
            return;

        d_updating_chevrons = true;
        const bool hasOverflow = hbar->maximum() > hbar->minimum();
        if (!hasOverflow) {
            d_btn_left->hide();
            d_btn_right->hide();
            d_updating_chevrons = false;
            return;
        }

        const int btnW = 18;
        const int availH = qMax(24, height() - (hbar->isVisible() ? hbar->height() : 6) - 6);
        const int btnY = 3;
        d_btn_left->setGeometry(2, btnY, btnW, availH);
        d_btn_right->setGeometry(qMax(2, width() - btnW - 2), btnY, btnW, availH);
        const bool canLeft = hbar->value() > hbar->minimum();
        const bool canRight = hbar->value() < hbar->maximum();
        d_btn_left->setEnabled(canLeft);
        d_btn_right->setEnabled(canRight);
        d_btn_left->setVisible(canLeft);
        d_btn_right->setVisible(canRight);
        if (canLeft)
            d_btn_left->raise();
        if (canRight)
            d_btn_right->raise();
        d_updating_chevrons = false;
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QScrollArea::resizeEvent(event);
        updateScrollChevrons();
    }

    void showEvent(QShowEvent *event) override
    {
        QScrollArea::showEvent(event);
        updateScrollChevrons();
    }

    void wheelEvent(QWheelEvent *event) override
    {
        if (QScrollBar *hbar = horizontalScrollBar()) {
            const int delta = (event->angleDelta().x() != 0) ? event->angleDelta().x()
                                                             : event->angleDelta().y();
            if (delta != 0 && hbar->maximum() > hbar->minimum()) {
                hbar->setValue(hbar->value() - (delta / 2));
                event->accept();
                return;
            }
        }
        QScrollArea::wheelEvent(event);
    }

private:
    QToolButton *d_btn_left = nullptr;
    QToolButton *d_btn_right = nullptr;
    bool d_updating_chevrons = false;
};

// C14: Japanese for the ribbon group names (shown on folded groups) and the minimize hint
void registerRibbonCollapseTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("File", "ファイル");
        reg("Undo / Redo", "元に戻す/やり直し");
        reg("Clipboard", "クリップボード");
        reg("Create", "新規作成");
        reg("Recalculation", "再計算");
        reg("Panels", "パネル");
        reg("2D Plots", "2D グラフ");
        reg("More Plots", "その他のグラフ");
        reg("Add to Graph", "グラフに追加");
        reg("Layers", "レイヤー");
        reg("Peaks", "ピーク");
        reg("Curve Fitting", "曲線フィット");
        reg("Graph Tools", "グラフツール");
        reg("Gadgets", "ガジェット");
        reg("Masks", "マスク");
        reg("Signal Processing", "信号処理");
        reg("Materials Analysis", "材料解析");
        reg("Statistics", "統計");
        reg("Journal Presets", "論文プリセット");
        reg("Export", "エクスポート");
        reg("Style", "スタイル");
        reg("Python Scripts", "Python スクリプト");
        reg("Settings", "設定");
        reg("Canvas Tools", "キャンバス操作");
        reg("Math & Tools", "曲線演算・ツール");
        reg("Column Role", "列の役割");
        reg("Formula & Data", "数式・データ");
        reg("Columns & Source", "列・データ更新");
        reg("Search commands...", "コマンドを検索...");
        reg("Guide", "ガイド");
        reg("Open Quick Guide (F1)", "かんたんガイドを開く (F1)");
        reg("Fit", "画面に合わせる");
        reg("Fit to Window", "画面に合わせる");
        reg("Fit graph page zoom to the current window", "グラフページの拡大率を現在のウィンドウ幅に合わせます");
        reg("Page Zoom: %1% (click to change or fit to window)", "ページ拡大率: %1% (クリックで倍率変更・画面合わせ)");
        reg("Graph Canvas", "グラフキャンバス");
        reg("Fit Page Zoom to Window", "ページ拡大率を画面に合わせる");
        reg("Rescale All Axes", "すべての軸を自動スケール");
        reg("Autoscale Active Axis", "選択中の軸を自動スケール");
        reg("Auto-Place Legend (Avoid Curves)", "凡例を空き領域へ自動配置");
        reg("Edit Plot Title...", "グラフタイトルを編集...");
        reg("Edit Axis Properties...", "軸の詳細設定を開く...");
        reg("Stacked Bars", "積み上げ棒グラフ");
        reg("Plot Stacked Vertical Bar Chart", "積み上げ縦棒グラフを作成");
        reg("Data Polar Plot (r, theta)", "極座標プロット (データ列)");
        reg("Plot Polar Graph from Worksheet Columns (Angle, Radius)", "ワークシートの角度・半径列から極座標グラフを作成");
        reg("More Layers", "レイヤー操作");
        reg("Arrange Layers...", "レイヤー配置...");
        reg("Add Layer", "レイヤーを追加");
        reg("2 Vertical Panels", "上下2段パネル");
        reg("4-Panel Grid (2x2)", "4分割パネル (2x2)");
        reg("Arrange multiple layers, merge open graphs, or create multi-panel figures",
            "レイヤーの配置、開いているグラフの結合、複数パネル図の作成");
        reg("Analyze", "解析");
        reg("Peaks, ROI Statistics, Integration, Quick Fit & 2D Profile",
            "ピーク解析、ROI統計、積分、クイックフィット、2Dプロファイル");
        reg("Presets", "プリセット");
        reg("Apply ISIJ 85mm, Lab Slide (18pt), or Custom Style Presets",
            "ISIJ 85mm、研究室スライド (18pt)、カスタムスタイルプリセットを適用");
        reg("Search", "検索");
        reg("Advanced MatSci", "専門材料解析");
        reg("Tauc Optical Bandgap & EIS Impedance Analysis", "Tauc光学バンドギャップとEISインピーダンス解析");
        reg("Group Compare", "群間比較");
        reg("Parametric ANOVA, Nonparametric Tests, Normality & Outlier Tests",
            "一元配置分散分析 (ANOVA)、ノンパラメトリック検定、正規性検定、外れ値検定");
        reg("Group ANOVA (Parametric)", "群間比較 ANOVA (パラメトリック)");
        reg("Nonparametric (Mann-Whitney / Kruskal-Wallis)", "ノンパラメトリック検定 (Mann-Whitney / Kruskal-Wallis)");
        reg("Apply Preset to Multiple Graphs...", "スタイルプリセットを複数グラフへ一括適用...");
        reg("Apply a style preset to several open graph windows at once",
            "開いている複数のグラフウィンドウへスタイルプリセットを一括適用します");
        reg("Apply Template (.qpt) to Table...", "テンプレート (.qpt) をアクティブ表へ適用...");
        reg("Apply a saved graph template (.qpt) to the active worksheet",
            "保存済みのグラフテンプレート (.qpt) を現在のワークシートへ適用します");
        reg("Worksheet", "ワークシート");
        reg("Set X", "X列に設定");
        reg("Set selected column(s) as X axis data", "選択列をX軸データに設定");
        reg("Set Y", "Y列に設定");
        reg("Set selected column(s) as Y axis data", "選択列をY軸データに設定");
        reg("Set ±Y", "±Y誤差に設定");
        reg("Set selected column(s) as Y error bars (yErr)", "選択列をYエラーバー (yErr) に設定");
        reg("Set Label", "ラベルに設定");
        reg("Set selected column(s) as text labels", "選択列をテキストラベル列に設定");
        reg("Formula F(x)=", "数式 F(x)=");
        reg("Edit column formula, long name, units, and numeric format",
            "列の数式、長い名前、単位、数値書式を編集");
        reg("Sort...", "並べ替え...");
        reg("Sort worksheet columns or rows", "ワークシートの列や行を並べ替え");
        reg("Fill Down", "下方向へフィル");
        reg("Copy the top row of the selection down to the remaining selected cells (Ctrl+D)",
            "選択範囲の先頭行の値を下の選択セルへコピーします (Ctrl+D)");
        reg("Find & Replace", "検索と置換");
        reg("Find or replace values in the active worksheet (Ctrl+F / Ctrl+H)",
            "アクティブなワークシート内の値を検索・置換します (Ctrl+F / Ctrl+H)");
        reg("Re-import", "再読み込み");
        reg("Reload data from the original CSV / Excel source file",
            "元の CSV / Excel ファイルからデータを再読み込みします");
        reg("Copy w/ Headers", "見出し付きコピー");
        reg("Copy selected cells together with Long Name and Units headers",
            "選択セルを長い名前・単位の見出し行ごとコピーします");
        reg("To Matrix", "行列へ変換");
        reg("Convert the active worksheet into a 2D numeric matrix",
            "アクティブなワークシートを2D数値行列へ変換します");
        reg("Create or select a graph window first to copy for PowerPoint.",
            "PowerPoint 用にコピーするには、先にグラフを作成または選択してください。");
        reg("Click a worksheet first to add a column.",
            "列を追加するには、先にワークシートを選択してください。");
        reg("Plot from Worksheet", "ワークシートから作図");
        reg("How should this worksheet be converted for %1?",
            "このワークシートを%1用にどのように変換して作図しますか?");
        reg("Grid XYZ columns into a 2D Matrix (recommended for X, Y, Z tables)",
            "X, Y, Z の3列を2D行列へ格子化して作図 (XYZデータ向け)");
        reg("Treat worksheet cells directly as a 2D Z-value grid (Convert to Matrix)",
            "ワークシート全体をそのまま2DのZ値格子として行列変換して作図");
        reg("2D Density Heatmap (count X-Y points on a grid)",
            "2D密度ヒートマップ (X-Y点密度を格子集計して表示)");
        reg("X column:", "X列:");
        reg("Y column:", "Y列:");
        reg("Z column:", "Z列:");
        reg("Grid size:", "格子分割数:");
        reg("Click a Matrix or Worksheet first to create a Heatmap, Contour, or 3D Surface.",
            "ヒートマップ・等高線・3D曲面を作成するには、先に行列またはワークシートを選択してください。");
        reg("No valid finite XYZ numeric rows found in the selected columns.",
            "選択した列に有効な数値の XYZ データが見つかりません。");
        reg("York, Global, Piecewise, Robust, Logistic, 3D Surface & Model Compare",
            "York、グローバル、区分線形、ロバスト、ロジスティック、3D曲面フィットとモデル比較");
        reg("York, Global, Piecewise, Robust, 3D Surface & Model Compare",
            "York、グローバル、区分線形、ロバスト、3D曲面フィットとモデル比較");
        reg("Draw Arrow, Draw Line, Add Image, Text, Legend, Error Bars & Reference Lines",
            "矢印、直線、画像、テキスト、凡例、エラーバー、基準線をグラフに追加");
        reg("Draw Arrow", "矢印を描く");
        reg("Draw an arrow annotation on the active graph", "アクティブなグラフ上に矢印注釈を描画");
        reg("Draw Line", "直線を描く");
        reg("Draw a straight line annotation on the active graph", "アクティブなグラフ上に直線注釈を描画");
        reg("Add a New Plot Layer to the Active Graph", "アクティブなグラフに新しいプロットレイヤーを追加");
        reg("Configure Multi-Panel Grid Layout & Layer Spacing", "複数パネルの格子レイアウトとレイヤー間隔を設定");
        reg("Create Multi-Sheet Workbook, Matrix, Add Column, or Apply Template",
            "複数シートブック、行列、列の追加、またはテンプレート適用");
        reg("Select a worksheet with X, Y, and Z columns first for 3D plotting.",
            "3D作図には X・Y・Z 列を含むワークシートを先に選択してください。");
        reg("%1: the window is too narrow to show this group, so its commands are listed here",
            "%1: ウィンドウの幅が足りないため、このグループのコマンドをここにまとめています");
        reg("Double-click a tab to minimize or restore the ribbon (Ctrl+F1)",
            "タブをダブルクリックするとリボンを最小化/元に戻します (Ctrl+F1)");
        reg("Free", "フリー");
        reg("Paste", "貼り付け");
        reg("Paste (Ctrl+V)", "貼り付け");
        reg("Paste clipboard data or table cells into worksheet (Ctrl+V)",
            "クリップボードやExcelの表データをワークシートに貼り付け (Ctrl+V)");
        reg("Clipboard is empty. Copy cells in Excel or TSV/CSV text first, then press Ctrl+V.",
            "クリップボードが空です。ExcelのセルやCSV/TSVテキストをコピーしてからCtrl+Vで貼り付けてください。");
        reg("Start Analyzing Data in Seconds", "すぐにデータ解析・作図を始めましょう");
        reg("Paste from Excel, drop a CSV/XLSX/XRD/OPJU file here, or open a sample project.",
            "Excelからの貼り付け、CSV/XLSX/XRD/OPJUファイルのドロップ、またはサンプルを開いて開始できます。");
        reg("Drop CSV / XLSX / XRD / TXT / OPJU files anywhere here",
            "CSV / XLSX / XRD / TXT / OPJU ファイルをここにドロップ");
        reg("Paste from Excel (Ctrl+V)", "Excelから貼り付け (Ctrl+V)");
        reg("Open File...", "ファイルを開く...");
        reg("Recent", "最近使ったファイル");
        reg("No recent files", "最近使ったファイルはありません");
        reg("New Worksheet", "新しいワークシート");
        reg("Quick Guide", "クイックガイド");
        reg("Show 10+ Sample Templates (XRD / Raman / Tensile / Cyclic / Peak Fit)",
            "10種類以上のサンプルを開く (XRD / ラマン / 引張 / サイクル / ピークフィット)");
        reg("Requires an open worksheet (click or create a worksheet)",
            "ワークシートが必要です（選択または新規作成してください）");
        reg("Requires a numeric or selected worksheet column",
            "数値データまたは選択されたワークシート列が必要です");
        reg("Requires a graph window (click a graph or plot data first)",
            "グラフウィンドウが必要です（グラフを選択するか先に作図してください）");
        reg("Requires a plotted curve on the active graph",
            "アクティブなグラフ上にプロット済み曲線が必要です");
        reg("Requires a selected graph object or layer",
            "グラフ上のオブジェクトまたはレイヤーを選択してください");
        reg("Requires a matrix window", "行列ウィンドウが必要です");
        reg("Requires a 3D graph window", "3Dグラフウィンドウが必要です");
        reg("Requires an open project window", "開いているウィンドウが必要です");
        reg("Currently unavailable in this view context",
            "現在の表示コンテキストでは実行できません");
    });
}

} // namespace

namespace {

static QString foldKanaAndSearchText(const QString &raw)
{
    if (raw.isEmpty())
        return QString();
    const QString norm = raw.normalized(QString::NormalizationForm_KC).toLower();
    QString out;
    out.reserve(norm.size());
    for (int i = 0; i < norm.size(); ++i) {
        const QChar ch = norm.at(i);
        const ushort uc = ch.unicode();
        if (uc >= 0x30A1 && uc <= 0x30F6)
            out.append(QChar(uc - 0x60));
        else
            out.append(ch);
    }
    return out;
}

static QString romajiToHiragana(const QString &asciiLower)
{
    if (asciiLower.size() < 2)
        return QString();
    for (int i = 0; i < asciiLower.size(); ++i) {
        const ushort u = asciiLower.at(i).unicode();
        if (!((u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') || u == '-'))
            return QString();
    }

    struct MapEntry {
        const char *rom;
        const ushort hira[2];
    };
    static const MapEntry kRomaji[] = {
        { "kya", { 0x304D, 0x3083 } }, { "kyu", { 0x304D, 0x3085 } },
        { "kyo", { 0x304D, 0x3087 } }, { "sha", { 0x3057, 0x3083 } },
        { "shi", { 0x3057, 0 } },      { "shu", { 0x3057, 0x3085 } },
        { "sho", { 0x3057, 0x3087 } }, { "chi", { 0x3061, 0 } },
        { "cha", { 0x3061, 0x3083 } }, { "chu", { 0x3061, 0x3085 } },
        { "cho", { 0x3061, 0x3087 } }, { "tsu", { 0x3064, 0 } },
        { "nya", { 0x306B, 0x3083 } }, { "nyu", { 0x306B, 0x3085 } },
        { "nyo", { 0x306B, 0x3087 } }, { "hya", { 0x3072, 0x3083 } },
        { "hyu", { 0x3072, 0x3085 } }, { "hyo", { 0x3072, 0x3087 } },
        { "mya", { 0x307F, 0x3083 } }, { "myu", { 0x307F, 0x3085 } },
        { "myo", { 0x307F, 0x3087 } }, { "rya", { 0x308A, 0x3083 } },
        { "ryu", { 0x308A, 0x3085 } }, { "ryo", { 0x308A, 0x3087 } },
        { "gya", { 0x304E, 0x3083 } }, { "gyu", { 0x304E, 0x3085 } },
        { "gyo", { 0x304E, 0x3087 } }, { "bya", { 0x3073, 0x3083 } },
        { "byu", { 0x3073, 0x3085 } }, { "byo", { 0x3073, 0x3087 } },
        { "pya", { 0x3074, 0x3083 } }, { "pyu", { 0x3074, 0x3085 } },
        { "pyo", { 0x3074, 0x3087 } }, { "fya", { 0x3075, 0x3083 } },
        { "fyu", { 0x3075, 0x3085 } }, { "fyo", { 0x3075, 0x3087 } },
        { "ka", { 0x304B, 0 } },       { "ki", { 0x304D, 0 } },
        { "ku", { 0x304F, 0 } },       { "ke", { 0x3051, 0 } },
        { "ko", { 0x3053, 0 } },       { "sa", { 0x3055, 0 } },
        { "si", { 0x3057, 0 } },       { "su", { 0x3059, 0 } },
        { "se", { 0x305B, 0 } },       { "so", { 0x305D, 0 } },
        { "ta", { 0x305F, 0 } },       { "ti", { 0x3061, 0 } },
        { "tu", { 0x3064, 0 } },       { "te", { 0x3066, 0 } },
        { "to", { 0x3068, 0 } },       { "na", { 0x306A, 0 } },
        { "ni", { 0x306B, 0 } },       { "nu", { 0x306C, 0 } },
        { "ne", { 0x306D, 0 } },       { "no", { 0x306E, 0 } },
        { "ha", { 0x306F, 0 } },       { "hi", { 0x3072, 0 } },
        { "hu", { 0x3075, 0 } },       { "fu", { 0x3075, 0 } },
        { "he", { 0x3078, 0 } },       { "ho", { 0x307B, 0 } },
        { "fa", { 0x3075, 0x3041 } },  { "fi", { 0x3075, 0x3043 } },
        { "fe", { 0x3075, 0x3047 } },  { "fo", { 0x3075, 0x3049 } },
        { "ma", { 0x307E, 0 } },       { "mi", { 0x307F, 0 } },
        { "mu", { 0x3080, 0 } },       { "me", { 0x3081, 0 } },
        { "mo", { 0x3082, 0 } },       { "ya", { 0x3084, 0 } },
        { "yu", { 0x3086, 0 } },       { "yo", { 0x3088, 0 } },
        { "ra", { 0x3089, 0 } },       { "ri", { 0x308A, 0 } },
        { "ru", { 0x308B, 0 } },       { "re", { 0x308C, 0 } },
        { "ro", { 0x308D, 0 } },       { "wa", { 0x308F, 0 } },
        { "wo", { 0x3092, 0 } },       { "ga", { 0x304C, 0 } },
        { "gi", { 0x304E, 0 } },       { "gu", { 0x3050, 0 } },
        { "ge", { 0x3052, 0 } },       { "go", { 0x3054, 0 } },
        { "za", { 0x3056, 0 } },       { "zi", { 0x3058, 0 } },
        { "ji", { 0x3058, 0 } },       { "zu", { 0x305A, 0 } },
        { "ze", { 0x305C, 0 } },       { "zo", { 0x305E, 0 } },
        { "ja", { 0x3058, 0x3083 } },  { "ju", { 0x3058, 0x3085 } },
        { "jo", { 0x3058, 0x3087 } },  { "da", { 0x3060, 0 } },
        { "di", { 0x3062, 0 } },       { "du", { 0x3065, 0 } },
        { "de", { 0x3067, 0 } },       { "do", { 0x3069, 0 } },
        { "ba", { 0x3070, 0 } },       { "bi", { 0x3073, 0 } },
        { "bu", { 0x3076, 0 } },       { "be", { 0x3079, 0 } },
        { "bo", { 0x307C, 0 } },       { "pa", { 0x3071, 0 } },
        { "pi", { 0x3074, 0 } },       { "pu", { 0x3077, 0 } },
        { "pe", { 0x307A, 0 } },       { "po", { 0x307D, 0 } },
        { "nn", { 0x3093, 0 } },
        { "a", { 0x3042, 0 } },        { "i", { 0x3044, 0 } },
        { "u", { 0x3046, 0 } },        { "e", { 0x3048, 0 } },
        { "o", { 0x304A, 0 } },
    };

    QString out;
    out.reserve(asciiLower.size());
    int idx = 0;
    while (idx < asciiLower.size()) {
        const QChar c = asciiLower.at(idx);
        if (c == QLatin1Char('-')) {
            out.append(QChar(0x30FC));
            ++idx;
            continue;
        }
        if (idx + 1 < asciiLower.size() && c == asciiLower.at(idx + 1) && c >= QLatin1Char('a')
            && c <= QLatin1Char('z') && c != QLatin1Char('a') && c != QLatin1Char('i')
            && c != QLatin1Char('u') && c != QLatin1Char('e') && c != QLatin1Char('o')
            && c != QLatin1Char('n')) {
            out.append(QChar(0x3063)); // small tsu
            ++idx;
            continue;
        }
        bool matched = false;
        for (const MapEntry &m : kRomaji) {
            const int len = static_cast<int>(qstrlen(m.rom));
            if (idx + len <= asciiLower.size()
                && QStringView(asciiLower).mid(idx, len) == QLatin1String(m.rom)) {
                out.append(QChar(m.hira[0]));
                if (m.hira[1] != 0)
                    out.append(QChar(m.hira[1]));
                idx += len;
                matched = true;
                break;
            }
        }
        if (matched)
            continue;
        if (c == QLatin1Char('n')) {
            if (idx + 1 >= asciiLower.size()) {
                out.append(QChar(0x3093));
                ++idx;
                continue;
            }
            const QChar next = asciiLower.at(idx + 1);
            if (next != QLatin1Char('a') && next != QLatin1Char('i') && next != QLatin1Char('u')
                && next != QLatin1Char('e') && next != QLatin1Char('o')
                && next != QLatin1Char('y')) {
                out.append(QChar(0x3093));
                ++idx;
                continue;
            }
        }
        return QString();
    }
    return out;
}

static QString spacedObjectNameKeywords(const QString &objName)
{
    if (objName.isEmpty())
        return QString();
    QString raw = objName;
    if (raw.startsWith(QLatin1String("action")) && raw.size() > 6
        && raw.at(6).isUpper()) {
        raw = raw.mid(6);
    }
    QString spaced;
    spaced.reserve(raw.size() + 8);
    for (int i = 0; i < raw.size(); ++i) {
        const QChar c = raw.at(i);
        if (c == QLatin1Char('_') || c == QLatin1Char('-')) {
            spaced.append(QLatin1Char(' '));
            continue;
        }
        if (i > 0 && c.isUpper() && !raw.at(i - 1).isUpper())
            spaced.append(QLatin1Char(' '));
        spaced.append(c);
    }
    return spaced.toLower();
}

static QString actionParentMenuHint(const QAction *act)
{
    if (!act)
        return QString();
    for (QWidget *w : act->associatedWidgets()) {
        if (QMenu *m = qobject_cast<QMenu *>(w)) {
            const QString mt = cleanMnemonicText(m->title()).trimmed();
            if (!mt.isEmpty())
                return mt;
        }
    }
    return QString();
}

static QString computeActionDisabledReason(const QAction *act, ApplicationWindow *app,
                                           ModernRibbonBar *ribbon)
{
    if (!app)
        return CommandPalettePopup::tr("Currently unavailable in this view context");
    if (app->d_workspace.subWindowList().isEmpty())
        return CommandPalettePopup::tr("Requires an open project window");

    QWidget *aw = app->d_workspace.activeSubWindow();
    Table *activeTable = qobject_cast<Table *>(aw);
    Table *wsTarget = ribbon ? ribbon->resolvedWorksheetTarget() : activeTable;
    MultiLayer *activeMl = qobject_cast<MultiLayer *>(aw);
    if (!activeMl && ribbon && ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph)
        activeMl = qobject_cast<MultiLayer *>(ribbon->splitPlotTarget());
    Matrix *activeMatrix = qobject_cast<Matrix *>(aw);
    Graph3D *active3d = qobject_cast<Graph3D *>(aw);

    int activeCurves = 0;
    bool hasSelectedGraphObj = false;
    if (activeMl) {
        if (Graph *g = activeMl->activeGraph() ? activeMl->activeGraph() : activeMl->layer(1)) {
            activeCurves = g->plotItemsList().size();
            hasSelectedGraphObj =
                    (g->currentSelection().kind != Graph::GraphSelection::None);
        }
    }

    const QString obj = act ? act->objectName().toLower() : QString();
    const QString clean = act ? cleanMnemonicText(act->text()).toLower() : QString();
    const QString tip = act ? cleanMnemonicText(act->toolTip()).toLower() : QString();
    const QString comb = obj + QLatin1Char(' ') + clean + QLatin1Char(' ') + tip;

    if ((comb.contains(QLatin1String("matrix"))
         && !comb.contains(QLatin1String("to matrix"))
         && !comb.contains(QLatin1String("convert"))
         && !comb.contains(QLatin1String("correlation"))
         && !comb.contains(QString::fromUtf8("\xE8\xA1\x8C\xE5\x88\x97\xE3\x81\xB8\xE5\xA4\x89\xE6\x8F\x9B")))
        || comb.contains(QLatin1String("spatial filter"))
        || comb.contains(QLatin1String("2d spatial"))) {
        if (!activeMatrix)
            return CommandPalettePopup::tr("Requires a matrix window");
    }

    if (comb.contains(QLatin1String("3d"))
        && (comb.contains(QLatin1String("rotation")) || comb.contains(QLatin1String("surface3d"))
            || comb.contains(QLatin1String("perspective"))
            || comb.contains(QLatin1String("floor")))) {
        if (!active3d)
            return CommandPalettePopup::tr("Requires a 3D graph window");
    }

    const bool isPlotAction =
            (obj.startsWith(QLatin1String("actionplot"))
             || comb.contains(QLatin1String("error bar"))
             || comb.contains(QString::fromUtf8("\xE3\x82\xA8\xE3\x83\xA9\xE3\x83\xBC\xE3\x83\x90\xE3\x83\xBC")));
    const bool isTableAction =
            comb.contains(QLatin1String("stacked bar"))
            || comb.contains(QLatin1String("polar"))
            || comb.contains(QLatin1String("double-y"))
            || comb.contains(QLatin1String("y-offset"))
            || comb.contains(QLatin1String("violin"))
            || comb.contains(QLatin1String("pareto"))
            || comb.contains(QLatin1String("q-q"))
            || comb.contains(QLatin1String("ternary"))
            || comb.contains(QLatin1String("plot table"))
            || comb.contains(QLatin1String("column"))
            || comb.contains(QLatin1String("worksheet"))
            || comb.contains(QLatin1String("sort"))
            || comb.contains(QLatin1String("formula"))
            || comb.contains(QLatin1String("fill"))
            || comb.contains(QLatin1String("norm"))
            || comb.contains(QLatin1String("re-import"))
            || comb.contains(QLatin1String("reimport"))
            || comb.contains(QLatin1String("to matrix"))
            || comb.contains(QString::fromUtf8("\xE5\x88\x97"))
            || comb.contains(QString::fromUtf8("\xE3\x83\xAF\xE3\x83\xBC\xE3\x82\xAF\xE3\x82\xB7\xE3\x83\xBC\xE3\x83\x88"));

    if (isPlotAction || isTableAction) {
        if (!wsTarget && !activeTable) {
            return CommandPalettePopup::tr(
                    "Requires an open worksheet (click or create a worksheet)");
        }
        return CommandPalettePopup::tr("Requires a numeric or selected worksheet column");
    }

    const bool isCurveAction =
            comb.contains(QLatin1String("fit")) || comb.contains(QLatin1String("smooth"))
            || comb.contains(QLatin1String("diff")) || comb.contains(QLatin1String("integ"))
            || comb.contains(QLatin1String("fft")) || comb.contains(QLatin1String("peak"))
            || comb.contains(QLatin1String("interp")) || comb.contains(QLatin1String("filter"))
            || comb.contains(QLatin1String("wavelet")) || comb.contains(QLatin1String("psd"))
            || comb.contains(QLatin1String("baseline")) || comb.contains(QLatin1String("translate"))
            || comb.contains(QLatin1String("roc")) || comb.contains(QLatin1String("kaplan"))
            || comb.contains(QString::fromUtf8("\xE3\x83\x95\xE3\x82\xA3\xE3\x83\x83\xE3\x83\x88"))
            || comb.contains(QString::fromUtf8("\xE5\xB9\xB3\xE6\xBB\x91"))
            || comb.contains(QString::fromUtf8("\xE7\xA9\x8D\xE5\x88\x86"))
            || comb.contains(QString::fromUtf8("\xE5\xBE\xAE\xE5\x88\x86"))
            || comb.contains(QString::fromUtf8("\xE3\x83\x94\xE3\x83\xBC\xE3\x82\xAF"));

    if (isCurveAction) {
        if (!activeMl) {
            return CommandPalettePopup::tr(
                    "Requires a graph window (click a graph or plot data first)");
        }
        if (activeCurves == 0)
            return CommandPalettePopup::tr("Requires a plotted curve on the active graph");
    }

    if (comb.contains(QLatin1String("graph")) || comb.contains(QLatin1String("layer"))
        || comb.contains(QLatin1String("axis")) || comb.contains(QLatin1String("axes"))
        || comb.contains(QLatin1String("legend")) || comb.contains(QLatin1String("grid"))
        || comb.contains(QLatin1String("page")) || comb.contains(QLatin1String("ole"))
        || comb.contains(QLatin1String("powerpoint")) || comb.contains(QLatin1String("python"))
        || comb.contains(QLatin1String("style")) || comb.contains(QLatin1String("aspect"))
        || comb.contains(QLatin1String("frame"))
        || comb.contains(QString::fromUtf8("\xE3\x82\xB0\xE3\x83\xA9\xE3\x83\x95"))
        || comb.contains(QString::fromUtf8("\xE8\xBB\xB8"))
        || comb.contains(QString::fromUtf8("\xE5\x87\xA1\xE4\xBE\x8B"))
        || comb.contains(QString::fromUtf8("\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC"))) {
        if (!activeMl) {
            return CommandPalettePopup::tr(
                    "Requires a graph window (click a graph or plot data first)");
        }
        if (!hasSelectedGraphObj)
            return CommandPalettePopup::tr("Requires a selected graph object or layer");
    }

    if (activeMl && !hasSelectedGraphObj)
        return CommandPalettePopup::tr("Requires a selected graph object or layer");
    if (activeTable)
        return CommandPalettePopup::tr("Requires a numeric or selected worksheet column");

    return CommandPalettePopup::tr("Currently unavailable in this view context");
}

} // namespace

// ============================================================================
// CommandPalettePopup
// ============================================================================

CommandPalettePopup::CommandPalettePopup(ApplicationWindow *app, ModernRibbonBar *ribbon,
                                         QWidget *parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
      d_app(app),
      d_ribbon(ribbon)
{
    setObjectName(QStringLiteral("ToriCommandPalettePopup"));
    setAttribute(Qt::WA_TranslucentBackground, false);
    setModal(false);
    setFixedSize(580, 410);

    setStyleSheet(QStringLiteral(
            "QDialog#ToriCommandPalettePopup {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 12px;"
            "}"
            "QLineEdit#CommandPaletteSearchEdit {"
            "  background-color: #F8FAFC;"
            "  color: #0F172A;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 8px;"
            "  padding: 6px 10px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 12pt;"
            "}"
            "QLineEdit#CommandPaletteSearchEdit:focus {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #2563EB;"
            "}"
            "QListWidget#CommandPaletteList {"
            "  background-color: #FFFFFF;"
            "  border: none;"
            "  outline: none;"
            "}"
            "QLabel#CommandPaletteHintLabel {"
            "  color: #64748B;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  padding: 2px 4px;"
            "}"));

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 10);
    rootLayout->setSpacing(8);

    QHBoxLayout *searchRow = new QHBoxLayout();
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(8);

    QLabel *searchIconLbl = new QLabel(this);
    searchIconLbl->setPixmap(
            IconLoader::duotonePixmap(QStringLiteral("search-command"), 20, 0.0, false));
    searchIconLbl->setFixedSize(22, 22);
    searchIconLbl->setAlignment(Qt::AlignCenter);
    searchRow->addWidget(searchIconLbl);

    d_search_edit = new QLineEdit(this);
    d_search_edit->setObjectName(QStringLiteral("CommandPaletteSearchEdit"));
    d_search_edit->setClearButtonEnabled(true);
    d_search_edit->installEventFilter(this);
    searchRow->addWidget(d_search_edit, 1);

    rootLayout->addLayout(searchRow);

    d_list_widget = new QListWidget(this);
    d_list_widget->setObjectName(QStringLiteral("CommandPaletteList"));
    d_list_widget->setItemDelegate(new CommandPaletteItemDelegate(d_list_widget));
    d_list_widget->setUniformItemSizes(false);
    d_list_widget->setMouseTracking(true);
    d_list_widget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d_list_widget->installEventFilter(this);
    rootLayout->addWidget(d_list_widget, 1);

    d_hint_label = new QLabel(this);
    d_hint_label->setObjectName(QStringLiteral("CommandPaletteHintLabel"));
    rootLayout->addWidget(d_hint_label);

    connect(d_search_edit, &QLineEdit::textChanged, this,
            &CommandPalettePopup::rebuildFilteredList);
    connect(d_list_widget, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item)
            executeItemAt(d_list_widget->row(item));
    });
    connect(d_list_widget, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item)
            executeItemAt(d_list_widget->row(item));
    });

    retranslateUi();
}

CommandPalettePopup::~CommandPalettePopup() = default;

void CommandPalettePopup::retranslateUi()
{
    setWindowTitle(tr("Command Palette"));
    if (d_search_edit)
        d_search_edit->setPlaceholderText(tr("Type a command, plot, fit, or preset..."));
    if (d_hint_label)
        d_hint_label->setText(tr("Up/Down to navigate  ·  Enter to run  ·  Esc to close"));
    refreshCommands();
}

void CommandPalettePopup::refreshCommands()
{
    d_entries.clear();

    auto addQuickEntry = [this](const QString &id, const QString &title, const QString &category,
                                const QString &shortcut, const QString &keywords,
                                const QString &iconId, std::function<void()> fn, int boost = 80) {
        CommandEntry e;
        e.id = id;
        e.title = title;
        e.category = category;
        e.shortcutText = shortcut;
        e.searchKey = foldKanaAndSearchText(title + QLatin1Char(' ') + category
                                            + QLatin1Char(' ') + shortcut + QLatin1Char(' ')
                                            + keywords);
        e.iconId = iconId;
        e.callback = std::move(fn);
        e.priorityBoost = boost;
        d_entries.append(std::move(e));
    };

    const QString catQuick = tr("Quick Preset");
    const QString catWorkspace = tr("Workspace");
    const QString catTools = tr("Tools");

    addQuickEntry(
            QStringLiteral("quick.isij85"), tr("ISIJ 85mm Journal Preset"), catQuick, QString(),
            QStringLiteral("isij 85mm journal style preset 600dpi"),
            QStringLiteral("preset-isij"),
            [this]() {
                if (d_app)
                    d_app->applyStyleISIJJournal();
            },
            100);

    addQuickEntry(
            QStringLiteral("quick.lab_slide"), tr("Lab Slide (18pt) Preset"), catQuick,
            QString(),
            QString::fromUtf8("lab slide 18pt 83.3mm 12.9pt 140% powerpoint style preset "
                              "\xE7\xA0\x94\xE7\xA9\xB6\xE5\xAE\xA4\xE3\x82\xB9\xE3\x83\xA9\xE3\x82\xA4\xE3\x83\x89"),
            QStringLiteral("preset-slide"),
            [this]() {
                if (!d_app)
                    return;
                if (MultiLayer *ml =
                            qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
                    ml->applyStylePresetById(QStringLiteral("lab_slide_83mm"));
                    d_app->modifiedProject();
                    if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                        insp->refreshAll();
                } else {
                    d_app->showUxHint(
                            ModernRibbonBar::tr("Click or create a graph window first."));
                }
            },
            99);

    addQuickEntry(
            QStringLiteral("quick.apply_preset_multi"),
            tr("Apply Preset to Multiple Graphs..."), catQuick, QString(),
            QString::fromUtf8("apply style preset multiple batch graphs isij lab slide "
                              "\xE8\xA4\x87\xE6\x95\xB0\xE3\x82\xB0\xE3\x83\xA9\xE3\x83\x95 "
                              "\xE4\xB8\x80\xE6\x8B\xAC\xE9\x81\xA9\xE7\x94\xA8"),
            QStringLiteral("preset-isij"),
            [this]() {
                if (!d_app)
                    return;
                if (!QMetaObject::invokeMethod(
                            d_app.data(), "showApplyStylePresetToMultipleGraphsDialog",
                            Qt::DirectConnection)) {
                    d_app->showStylePresetManagerDialog();
                }
            },
            95);

    addQuickEntry(
            QStringLiteral("quick.apply_template_table"),
            tr("Apply Template (.qpt) to Table..."), catQuick, QString(),
            QString::fromUtf8("apply template qpt table worksheet "
                              "\xE3\x83\x86\xE3\x83\xB3\xE3\x83\x97\xE3\x83\xAC\xE3\x83\xBC\xE3\x83\x88"),
            QStringLiteral("page-setup"),
            [this]() {
                if (!d_app)
                    return;
                if (!QMetaObject::invokeMethod(d_app.data(), "applyTemplateToActiveTable",
                                               Qt::DirectConnection)) {
                    d_app->openTemplate();
                }
            },
            86);

    addQuickEntry(
            QStringLiteral("quick.sample_gallery"),
            tr("Sample Project Gallery..."), catQuick, QString(),
            QString::fromUtf8("sample gallery demo template example dataset project "
                              "\xE3\x82\xB5\xE3\x83\xB3\xE3\x83\x97\xE3\x83\xAB "
                              "\xE3\x82\xAE\xE3\x83\xA3\xE3\x83\xA9\xE3\x83\xAA\xE3\x83\xBC "
                              "\xE4\xBE\x8B\xE9\xA1\x8C"),
            QStringLiteral("sample-gallery"),
            [this]() {
                if (d_app)
                    CurveMathTools::showSampleGalleryDialog(d_app);
            },
            99);

    addQuickEntry(
            QStringLiteral("quick.tensile_yield"),
            tr("Tensile / 0.2% Yield Analysis..."), catQuick, QString(),
            QString::fromUtf8("tensile stress strain 0.2% offset yield proof youngs modulus uts "
                              "\xE5\xBC\x95\xE5\xBC\xB5 \xE8\x80\x90\xE5\x8A\x9B "
                              "\xE5\xBF\x9C\xE5\x8A\x9B \xE3\x81\xB2\xE3\x81\x9A\xE3\x81\xBF "
                              "\xE3\x83\xA4\xE3\x83\xB3\xE3\x82\xB0\xE7\x8E\x87"),
            QStringLiteral("curvemath"),
            [this]() {
                if (d_app)
                    CurveMathTools::showTensileYieldDialog(d_app);
            },
            97);

    addQuickEntry(
            QStringLiteral("quick.kissinger_tds"),
            tr("Kissinger / Thermal Desorption (TDS)..."), catQuick, QString(),
            QString::fromUtf8("kissinger tds thermal desorption dsc activation energy heating rate "
                              "\xE6\x98\x87\xE6\xB8\xA9\xE8\x84\xB1\xE9\x9B\xA2 "
                              "\xE6\xB4\xBB\xE6\x80\xA7\xE5\x8C\x96\xE3\x82\xA8\xE3\x83\x8D\xE3\x83\xAB\xE3\x82\xAE\xE3\x83\xBC"),
            QStringLiteral("kissinger-tds"),
            [this]() {
                if (d_app)
                    CurveMathTools::showKissingerDialog(d_app);
            },
            95);

    addQuickEntry(
            QStringLiteral("quick.work_hardening"),
            tr("Work Hardening / Considere Analysis..."), catQuick, QString(),
            QString::fromUtf8("work hardening considere Hollomon Crussard Jaoul Kocks Mecking true stress strain "
                              "\xE5\x8A\xA0\xE5\xB7\xA5\xE7\xA1\xAC\xE5\x8C\x96 "
                              "\xE7\x9C\x9F\xE5\xBF\x9C\xE5\x8A\x9B \xE7\x9C\x9F\xE3\x81\xB2\xE3\x81\x9A\xE3\x81\xBF"),
            QStringLiteral("work-hardening"),
            [this]() {
                if (d_app)
                    CurveMathTools::showWorkHardeningDialog(d_app);
            },
            94);

    addQuickEntry(
            QStringLiteral("quick.xrd_analyzer"),
            tr("XRD Peak & Williamson-Hall Analyzer..."), catQuick, QString(),
            QString::fromUtf8("xrd diffraction scherrer williamson hall crystallite microstrain ka2 "
                              "X\xE7\xB7\x9A\xE5\x9B\x9E\xE6\x8A\x98 \xE7\xB5\x90\xE6\x99\xB6\xE5\xAD\x90"),
            QStringLiteral("xrd-peaks"),
            [this]() {
                if (d_app)
                    CurveMathTools::showXRDAnalyzerDialog(d_app);
            },
            94);

    addQuickEntry(
            QStringLiteral("quick.vasp_plotter"),
            tr("VASP Electronic Band & DOS Plotter..."), catQuick, QString(),
            QString::fromUtf8("vasp band structure doscar eigenval fermi electronic density of states "
                              "\xE3\x83\x90\xE3\x83\xB3\xE3\x83\x89\xE6\xA7\x8B\xE9\x80\xA0 "
                              "\xE7\x8A\xB6\xE6\x85\x8B\xE5\xAF\x86\xE5\xBA\xA6 \xE9\x9B\xBB\xE5\xAD\x90\xE7\x8A\xB6\xE6\x85\x8B"),
            QStringLiteral("vasp-bands"),
            [this]() {
                if (d_app)
                    CurveMathTools::showVaspPlotterDialog(d_app);
            },
            93);

    addQuickEntry(
            QStringLiteral("quick.export_python"),
            tr("Export Active Graph to Python Script..."), catTools, QString(),
            QString::fromUtf8("export python script code reproduce matplotlib torigraph generate py "
                              "Python\xE5\x87\xBA\xE5\x8A\x9B \xE3\x82\xB9\xE3\x82\xAF\xE3\x83\xAA\xE3\x83\x97\xE3\x83\x88\xE7\x94\x9F\xE6\x88\x90"),
            QStringLiteral("code-export"),
            [this]() {
                if (d_app)
                    d_app->exportActiveGraphToPython();
            },
            91);

    addQuickEntry(
            QStringLiteral("quick.merge_graphs"), tr("Merge Graph Windows..."), catWorkspace,
            QString(),
            QStringLiteral("merge graph windows multi panel figure a b c d combine layout"),
            QStringLiteral("merge-graphs"),
            [this]() {
                if (d_app)
                    d_app->mergeOpenGraphsDialog();
            },
            95);

    addQuickEntry(
            QStringLiteral("quick.split_view"), tr("Split View (Table + Graph)"), catWorkspace,
            QString(), QStringLiteral("split view side by side table graph workspace"),
            QStringLiteral("split-view"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
            },
            94);

    addQuickEntry(
            QStringLiteral("quick.tabbed_view"), tr("Tabbed Workspace View"), catWorkspace,
            QString(), QStringLiteral("tabbed tabs workspace view"), QStringLiteral("tab-view"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutTabbed);
            },
            90);

    addQuickEntry(
            QStringLiteral("quick.free_mdi"), tr("Free MDI Workspace View"), catWorkspace,
            QString(), QStringLiteral("free mdi cascade subwindow workspace view"),
            QStringLiteral("mdi-view"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
            },
            86);

    addQuickEntry(
            QStringLiteral("quick.focus_hud"), tr("Toggle Focus HUD Mode (F11)"), catWorkspace,
            QStringLiteral("F11"), QStringLiteral("focus hud canvas fullscreen f11"),
            QStringLiteral("focus-hud"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->toggleFocusHudMode();
            },
            92);

    addQuickEntry(
            QStringLiteral("quick.graph_inspector"),
            tr("Toggle Graph Inspector Dock (Ctrl+Shift+I)"), catWorkspace,
            QStringLiteral("Ctrl+Shift+I"),
            QStringLiteral("graph inspector dock object manager property sidebar rail ctrl+shift+i"),
            QStringLiteral("table-drawer"),
            [this]() {
                if (d_app)
                    d_app->toggleGraphInspectorDock();
            },
            93);

    addQuickEntry(
            QStringLiteral("quick.python_console"), tr("Open Python Console"), catTools,
            QStringLiteral("F9"), QStringLiteral("python console repl script automation"),
            QStringLiteral("python-console"),
            [this]() {
                if (d_ribbon)
                    emit d_ribbon->openPythonConsoleRequested();
            },
            88);

    addQuickEntry(
            QStringLiteral("quick.python_run"), tr("Run Python Script..."), catTools, QString(),
            QStringLiteral("run python script py file automation"), QStringLiteral("python-run"),
            [this]() {
                if (d_ribbon)
                    emit d_ribbon->runPythonScriptRequested();
            },
            84);

    addQuickEntry(
            QStringLiteral("quick.check_updates"), tr("Check for Updates..."), catTools,
            QString(), QStringLiteral("check updates github release version upgrade"),
            QStringLiteral("update-check"),
            [this]() {
                if (d_ribbon)
                    emit d_ribbon->checkForUpdatesRequested();
            },
            82);

    addQuickEntry(
            QStringLiteral("quick.switch_language"), tr("Switch Language (Japanese / English)"),
            catTools, QString(), QStringLiteral("language japanese english i18n locale ja en"),
            QStringLiteral("language-ja-en"),
            [this]() {
                if (!d_app)
                    return;
                if (!QMetaObject::invokeMethod(d_app.data(), "toggleLanguage",
                                               Qt::DirectConnection)) {
                    if (d_app->appLanguage.startsWith(QLatin1String("ja"), Qt::CaseInsensitive))
                        d_app->switchToEnglish();
                    else
                        d_app->switchToJapanese();
                }
            },
            85);

    // B4-10: Index canvas context actions so Ctrl+K can invoke them directly.
    if (d_app) {
        const QString catCanvas = tr("Graph Canvas");
        MultiLayer *activeMl = qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow());
        if (!activeMl && d_ribbon
            && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
            activeMl = qobject_cast<MultiLayer *>(d_ribbon->splitPlotTarget());
        }
        auto addCanvasEntry = [&](const QString &id, const QString &title, const QString &keywords,
                                  const QString &iconId, std::function<void(MultiLayer *, Graph *)> fn,
                                  int boost = 72) {
            CommandEntry e;
            e.id = id;
            e.title = title;
            e.category = catCanvas;
            e.searchKey = foldKanaAndSearchText(title + QLatin1Char(' ') + catCanvas
                                                + QLatin1Char(' ') + keywords);
            e.iconId = iconId;
            e.priorityBoost = activeMl ? boost : -35;
            if (!activeMl) {
                e.disabled = true;
                e.disabledReason = tr("Requires a graph window (click a graph or plot data first)");
            } else {
                QPointer<ApplicationWindow> appPtr(d_app);
                e.callback = [appPtr, fn = std::move(fn)]() {
                    if (!appPtr)
                        return;
                    if (MultiLayer *ml =
                                qobject_cast<MultiLayer *>(appPtr->d_workspace.activeSubWindow())) {
                        Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->layer(1);
                        if (g)
                            fn(ml, g);
                    }
                };
            }
            d_entries.append(std::move(e));
        };

        addCanvasEntry(
                QStringLiteral("canvas.fit_zoom"), tr("Fit Page Zoom to Window"),
                QString::fromUtf8("zoom fit window page scale 100% "
                                  "\xE7\x94\xBB\xE9\x9D\xA2\xE3\x81\xAB\xE5\x90\x88\xE3\x82\x8F\xE3\x81\x9B\xE3\x82\x8B "
                                  "\xE6\x8B\xA1\xE5\xA4\xA7\xE7\x8E\x87"),
                QStringLiteral("zoom-auto"),
                [](MultiLayer *ml, Graph *) {
                    if (!QMetaObject::invokeMethod(ml, "fitPageZoomToViewport",
                                                   Qt::DirectConnection)) {
                        ml->setPageZoom(1.0);
                    }
                },
                90);
        addCanvasEntry(
                QStringLiteral("canvas.rescale_all"), tr("Rescale All Axes"),
                QString::fromUtf8("autoscale rescale reset axes range "
                                  "\xE8\xBB\xB8 \xE8\x87\xAA\xE5\x8B\x95\xE3\x82\xB9\xE3\x82\xB1\xE3\x83\xBC\xE3\x83\xAB"),
                QStringLiteral("zoom-auto"),
                [this](MultiLayer *ml, Graph *g) {
                    const QString before = ml->captureUndoSnapshot();
                    g->setAutoScale();
                    ml->pushUndoSnapshot(tr("Rescale All Axes"), before);
                    if (d_app)
                        d_app->modifiedProject();
                },
                82);
        addCanvasEntry(
                QStringLiteral("canvas.autoscale_axis"), tr("Autoscale Active Axis"),
                QString::fromUtf8("autoscale active axis range "
                                  "\xE9\x81\xB8\xE6\x8A\x9E\xE8\xBB\xB8 \xE8\x87\xAA\xE5\x8B\x95"),
                QStringLiteral("zoom-auto"),
                [this](MultiLayer *ml, Graph *g) {
                    const auto sel = g->currentSelection();
                    const int ax = (sel.kind == Graph::GraphSelection::Axis
                                    || sel.kind == Graph::GraphSelection::AxisTitle)
                                    && sel.primaryId >= 0 && sel.primaryId < QwtPlot::axisCnt
                            ? sel.primaryId
                            : QwtPlot::xBottom;
                    const QString before = ml->captureUndoSnapshot();
                    g->setAxisUserLocked(ax, false);
                    g->enableAutoscaling(true);
                    g->setAutoScale();
                    ml->pushUndoSnapshot(tr("Autoscale Active Axis"), before);
                    if (d_app)
                        d_app->modifiedProject();
                },
                76);
        addCanvasEntry(
                QStringLiteral("canvas.ref_zero_y"), tr("y = 0 Reference Line"),
                QString::fromUtf8("reference line zero y=0 baseline "
                                  "\xE5\x9F\xBA\xE6\xBA\x96\xE7\xB7\x9A"),
                QStringLiteral("mean-line"),
                [this](MultiLayer *, Graph *g) {
                    GraphInspectorDock::toggleZeroReferenceLine(g, 0);
                    if (d_app) {
                        if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                            insp->refreshAll();
                    }
                },
                78);
        addCanvasEntry(
                QStringLiteral("canvas.ref_zero_x"), tr("x = 0 Reference Line"),
                QString::fromUtf8("reference line zero x=0 vertical "
                                  "\xE5\x9F\xBA\xE6\xBA\x96\xE7\xB7\x9A"),
                QStringLiteral("mean-line"),
                [this](MultiLayer *, Graph *g) {
                    GraphInspectorDock::toggleZeroReferenceLine(g, 1);
                    if (d_app) {
                        if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                            insp->refreshAll();
                    }
                },
                75);
        addCanvasEntry(
                QStringLiteral("canvas.ref_custom"), tr("Add Reference Line..."),
                QString::fromUtf8("reference line custom constant mean threshold "
                                  "\xE5\x9F\xBA\xE6\xBA\x96\xE7\xB7\x9A\xE3\x82\x92\xE8\xBF\xBD\xE5\x8A\xA0"),
                QStringLiteral("curve-add"),
                [](MultiLayer *, Graph *g) {
                    GraphInspectorDock::addReferenceLineInteractive(g, 0);
                },
                80);
        addCanvasEntry(
                QStringLiteral("canvas.auto_legend"), tr("Auto-Place Legend (Avoid Curves)"),
                QString::fromUtf8("legend auto place avoid curves position "
                                  "\xE5\x87\xA1\xE4\xBE\x8B \xE8\x87\xAA\xE5\x8B\x95\xE9\x85\x8D\xE7\xBD\xAE"),
                QStringLiteral("legend"),
                [this](MultiLayer *ml, Graph *g) {
                    const QString before = ml->captureUndoSnapshot();
                    g->autoPlaceLegend();
                    ml->pushUndoSnapshot(tr("Auto-Place Legend (Avoid Curves)"), before);
                    if (d_app)
                        d_app->modifiedProject();
                },
                76);
        addCanvasEntry(
                QStringLiteral("canvas.edit_title"), tr("Edit Plot Title..."),
                QString::fromUtf8("title edit plot graph heading "
                                  "\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB\xE7\xB7\xA8\xE9\x9B\x86"),
                QStringLiteral("text-label"),
                [this](MultiLayer *, Graph *) {
                    if (d_app)
                        d_app->showTitleDialog();
                },
                74);
    }

    if (d_app) {
        QSet<QString> seenTitles;
        QSet<QString> coveredObjectNames = {
            QStringLiteral("actionTensileYieldDialog"),
            QStringLiteral("actionMergeGraphs"),
            QStringLiteral("actionWorkspaceLayoutSplit"),
            QStringLiteral("actionWorkspaceLayoutTabbed"),
            QStringLiteral("actionWorkspaceLayoutMdi"),
            QStringLiteral("actionToggleFocusHudMode"),
            QStringLiteral("actionToggleGraphInspector"),
            QStringLiteral("actionOpenPythonConsole"),
            QStringLiteral("actionRunPythonScript"),
            QStringLiteral("actionCheckUpdates"),
            QStringLiteral("actionCommandPalette")
        };
        const char *const kCoveredAppTexts[] = {
            "&ISIJ Journal (85 mm, 600 DPI)",
            "&Lab Slide (18 pt @ 140%, 83.3 mm)",
            "Apply Style Preset to &Multiple Graphs...",
            "Plot Table Using &Template...",
            "Sample Project &Gallery...",
            "&Kissinger / Thermal Desorption (TDS)...",
            "&Work Hardening && Considere Analysis...",
            "&XRD Peak && Williamson-Hall Analyzer...",
            "&VASP Electronic Band && DOS Plotter...",
            "Export Reproducible &Python Script...",
            "&Tabbed Workspace View"
        };
        QSet<const QAction *> seenActions;
        for (const CommandEntry &ce : d_entries)
            seenTitles.insert(ce.title.toLower());
        for (const char *raw : kCoveredAppTexts) {
            const QString rawClean = cleanMnemonicText(QString::fromUtf8(raw)).trimmed().toLower();
            if (!rawClean.isEmpty())
                seenTitles.insert(rawClean);
            const QString trClean = cleanMnemonicText(ApplicationWindow::tr(raw)).trimmed().toLower();
            if (!trClean.isEmpty())
                seenTitles.insert(trClean);
        }

        const QList<QAction *> actions = d_app->findChildren<QAction *>();
        const QString catCommand = tr("Command");
        for (QAction *act : actions) {
            if (!act || act->isSeparator() || act->menu() != nullptr)
                continue;
            if (seenActions.contains(act)
                || (!act->objectName().isEmpty()
                    && coveredObjectNames.contains(act->objectName()))) {
                continue;
            }
            // Hidden actions (e.g. other-field features switched off) stay out of the list;
            // disabled ones are listed dimmed with per-action reasons (C12).
            if (!act->isVisible())
                continue;

            const QString cleanTitle = cleanMnemonicText(act->text()).trimmed();
            if (cleanTitle.isEmpty())
                continue;
            if (cleanTitle.size() == 1 && !cleanTitle.at(0).isLetter())
                continue;

            QString displayTitle = cleanTitle;
            const QString lowerTitle = cleanTitle.toLower();
            const QString menuHint = actionParentMenuHint(act);
            if (seenTitles.contains(lowerTitle)) {
                if (menuHint.isEmpty() || seenTitles.contains((cleanTitle + QStringLiteral(" (") + menuHint + QLatin1Char(')')).toLower()))
                    continue;
                displayTitle = cleanTitle + QStringLiteral(" (") + menuHint + QLatin1Char(')');
            }
            seenTitles.insert(displayTitle.toLower());
            seenActions.insert(act);

            CommandEntry e;
            e.id = act->objectName();
            e.title = displayTitle;
            e.category = !menuHint.isEmpty() ? menuHint : catCommand;
            e.shortcutText = act->shortcut().toString(QKeySequence::NativeText);
            const QString tip = cleanMnemonicText(act->toolTip());
            e.searchKey = foldKanaAndSearchText(
                    displayTitle + QLatin1Char(' ') + tip + QLatin1Char(' ') + menuHint
                    + QLatin1Char(' ') + spacedObjectNameKeywords(act->objectName())
                    + QLatin1Char(' ') + act->objectName() + QLatin1Char(' ') + e.shortcutText);
            // Use the action's own duotone id (the one shown in menus and the ribbon); the old
            // keyword guess from the object name gave many entries an unrelated icon. Without
            // an id the palette falls back to the action's icon.
            e.iconId = act->property("_tg_duotone_id").toString();
            if (e.iconId.isEmpty())
                e.iconId = ModernUiTheme::semanticIconForAction(act);
            e.action = act;
            e.priorityBoost = 0;
            if (!act->isEnabled()) {
                e.disabled = true;
                e.disabledReason = computeActionDisabledReason(act, d_app.data(), d_ribbon.data());
                e.priorityBoost = -40;
            }
            d_entries.append(std::move(e));
        }
    }

    rebuildFilteredList(d_search_edit ? d_search_edit->text() : QString());
}

void CommandPalettePopup::setFilterText(const QString &query)
{
    if (!d_search_edit)
        return;
    if (d_search_edit->text() == query)
        rebuildFilteredList(query);
    else
        d_search_edit->setText(query);
}

QString CommandPalettePopup::filterText() const
{
    return d_search_edit ? d_search_edit->text() : QString();
}

int CommandPalettePopup::visibleCommandCount() const
{
    return d_list_widget ? d_list_widget->count() : 0;
}

int CommandPalettePopup::fuzzyMatchScore(const QString &needleLower, const CommandEntry &entry)
{
    if (needleLower.isEmpty())
        return 100 + entry.priorityBoost;

    const QString titleFolded = foldKanaAndSearchText(entry.title);
    QStringList variants;
    variants << needleLower;
    const QString romHira = romajiToHiragana(needleLower);
    if (!romHira.isEmpty() && romHira != needleLower)
        variants << romHira;

    int bestScore = -1;
    for (int v = 0; v < variants.size(); ++v) {
        const QString &q = variants.at(v);
        const int romPenalty = (v == 0) ? 0 : -40;
        if (titleFolded == q) {
            bestScore = qMax(bestScore, 1200 + romPenalty + entry.priorityBoost);
            continue;
        }
        if (titleFolded.startsWith(q)) {
            bestScore = qMax(bestScore, 900 + romPenalty + entry.priorityBoost);
            continue;
        }
        const int subIdx = titleFolded.indexOf(q);
        if (subIdx >= 0) {
            bestScore = qMax(bestScore, 700 - qMin(120, subIdx * 3) + romPenalty + entry.priorityBoost);
            continue;
        }

        const QStringList tokens = q.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() > 1) {
            bool allTokensFound = true;
            int tokenBonus = 0;
            for (const QString &tok : tokens) {
                if (!entry.searchKey.contains(tok)) {
                    allTokensFound = false;
                    break;
                }
                if (titleFolded.contains(tok))
                    tokenBonus += 40;
            }
            if (allTokensFound) {
                bestScore = qMax(bestScore, 520 + tokenBonus + romPenalty + entry.priorityBoost);
                continue;
            }
        }

        if (entry.searchKey.contains(q)) {
            bestScore = qMax(bestScore, 420 + romPenalty + entry.priorityBoost);
            continue;
        }

        auto subseqScore = [&](const QString &haystack, int baseScore) -> int {
            int pos = 0;
            int streak = 0;
            int score = baseScore;
            for (int i = 0; i < q.size(); ++i) {
                const QChar ch = q.at(i);
                if (ch.isSpace())
                    continue;
                const int found = haystack.indexOf(ch, pos);
                if (found < 0)
                    return -1;
                if (found == pos) {
                    streak += 1;
                    score += 18 + streak * 6;
                } else {
                    streak = 0;
                    score -= qMin(25, found - pos);
                }
                pos = found + 1;
            }
            return qMax(1, score + romPenalty + (entry.priorityBoost / 2));
        };

        int ss = subseqScore(titleFolded, 200);
        if (ss < 0)
            ss = subseqScore(entry.searchKey, 120);
        if (ss > 0)
            bestScore = qMax(bestScore, ss);
    }

    return bestScore;
}

void CommandPalettePopup::rebuildFilteredList(const QString &query)
{
    if (!d_list_widget)
        return;

    const QString needle = foldKanaAndSearchText(query.trimmed());
    struct ScoredIdx
    {
        int index;
        int score;
    };
    QVector<ScoredIdx> matches;
    matches.reserve(d_entries.size());

    for (int i = 0; i < d_entries.size(); ++i) {
        const int sc = fuzzyMatchScore(needle, d_entries.at(i));
        if (sc > 0)
            matches.append({ i, sc });
    }

    std::stable_sort(matches.begin(), matches.end(),
                     [](const ScoredIdx &a, const ScoredIdx &b) { return a.score > b.score; });

    d_list_widget->clear();
    d_visible_entry_indices.clear();

    const int maxItems = qMin(matches.size(), 80);
    d_visible_entry_indices.reserve(maxItems);
    for (int i = 0; i < maxItems; ++i) {
        const int entryIdx = matches.at(i).index;
        const CommandEntry &ce = d_entries.at(entryIdx);
        d_visible_entry_indices.append(entryIdx);

        QIcon itemIcon = (!ce.iconId.isEmpty())
                ? IconLoader::duotoneIcon(ce.iconId, 20, false)
                : (ce.action ? ce.action->icon() : QIcon());
        if (itemIcon.isNull())
            itemIcon = IconLoader::duotoneIcon(QStringLiteral("search-command"), 20, false);

        QListWidgetItem *item = new QListWidgetItem(itemIcon, ce.title, d_list_widget);
        item->setData(RoleEntryIndex, entryIdx);
        item->setData(RoleShortcutText, ce.shortcutText);
        item->setData(RoleCategoryText, ce.category);
        if (ce.disabled) {
            item->setData(RoleDisabledEntry, true);
            item->setData(RoleDisabledReason, ce.disabledReason);
            item->setToolTip(ce.disabledReason);
        }
    }

    if (d_list_widget->count() > 0)
        d_list_widget->setCurrentRow(0);
}

bool CommandPalettePopup::executeCurrentSelection()
{
    if (!d_list_widget || d_list_widget->count() <= 0)
        return false;
    const int row = qMax(0, d_list_widget->currentRow());
    return executeItemAt(row);
}

bool CommandPalettePopup::executeItemAt(int row)
{
    if (row < 0 || row >= d_visible_entry_indices.size())
        return false;

    const int entryIdx = d_visible_entry_indices.at(row);
    if (entryIdx < 0 || entryIdx >= d_entries.size())
        return false;

    const QPointer<QAction> act = d_entries.at(entryIdx).action;
    const std::function<void()> cb = d_entries.at(entryIdx).callback;

    if (d_entries.at(entryIdx).disabled && !(act && act->isEnabled())) {
        if (d_list_widget) {
            const QRect r = d_list_widget->visualItemRect(d_list_widget->item(row));
            QToolTip::showText(d_list_widget->viewport()->mapToGlobal(r.bottomLeft()),
                               d_entries.at(entryIdx).disabledReason, d_list_widget);
        }
        return false;
    }

    hide();

    if (cb) {
        cb();
        return true;
    }
    if (act && act->isEnabled()) {
        act->trigger();
        return true;
    }
    return false;
}

bool CommandPalettePopup::eventFilter(QObject *watched, QEvent *event)
{
    if (event && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (watched == d_search_edit || watched == d_list_widget) {
            if (ke->key() == Qt::Key_Down) {
                if (d_list_widget && d_list_widget->count() > 0) {
                    const int cnt = d_list_widget->count();
                    const int cur = d_list_widget->currentRow();
                    const int next = (cur < 0) ? 0 : ((cur + 1) % cnt);
                    d_list_widget->setCurrentRow(next);
                }
                return true;
            }
            if (ke->key() == Qt::Key_Up) {
                if (d_list_widget && d_list_widget->count() > 0) {
                    const int cnt = d_list_widget->count();
                    const int cur = d_list_widget->currentRow();
                    const int prev = (cur < 0) ? (cnt - 1) : ((cur - 1 + cnt) % cnt);
                    d_list_widget->setCurrentRow(prev);
                }
                return true;
            }
            if (ke->key() == Qt::Key_PageDown) {
                if (d_list_widget && d_list_widget->count() > 0) {
                    const int next = qMin(d_list_widget->count() - 1,
                                          qMax(0, d_list_widget->currentRow()) + 8);
                    d_list_widget->setCurrentRow(next);
                }
                return true;
            }
            if (ke->key() == Qt::Key_PageUp) {
                if (d_list_widget && d_list_widget->count() > 0) {
                    const int prev = qMax(0, d_list_widget->currentRow() - 8);
                    d_list_widget->setCurrentRow(prev);
                }
                return true;
            }
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                executeCurrentSelection();
                return true;
            }
            if (ke->key() == Qt::Key_Escape
                || (ke->key() == Qt::Key_K && (ke->modifiers() & Qt::ControlModifier))) {
                hide();
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandPalettePopup::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void CommandPalettePopup::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (d_search_edit) {
        d_search_edit->setFocus(Qt::PopupFocusReason);
        d_search_edit->selectAll();
    }
}

// ============================================================================
// FocusTableDrawer
// ============================================================================

namespace {

static QString stripDrawerInlineRichTags(const QString &raw)
{
    if (!raw.contains(QLatin1Char('<')))
        return raw;
    static const QRegularExpression kInlineTags(
            QStringLiteral("</?(?:i|b|sub|sup|u|em|strong)>"),
            QRegularExpression::CaseInsensitiveOption);
    QString out = raw;
    out.remove(kInlineTags);
    return out.trimmed();
}

static QString formatDrawerColumnHeader(Table *tbl, int c, QString *outFormula = nullptr)
{
    if (!tbl || c < 0 || c >= tbl->numCols())
        return QString();

    QString roleTag;
    switch (tbl->colPlotDesignation(c)) {
    case SciDAVis::X:
        roleTag = QStringLiteral("[X]");
        break;
    case SciDAVis::Y:
        roleTag = QStringLiteral("[Y]");
        break;
    case SciDAVis::Z:
        roleTag = QStringLiteral("[Z]");
        break;
    case SciDAVis::xErr:
        roleTag = QStringLiteral("[dX]");
        break;
    case SciDAVis::yErr:
        roleTag = QStringLiteral("[dY]");
        break;
    default:
        break;
    }

    Column *colObj = tbl->column(c);
    QString colFormula;
    if (colObj && !colObj->formulaIntervals().isEmpty()) {
        const Interval<int> iv = colObj->formulaIntervals().first();
        colFormula = colObj->formula(iv.start()).trimmed();
    }
    if (outFormula)
        *outFormula = colFormula;

    const QString longName = stripDrawerInlineRichTags(tbl->colLongName(c).trimmed());
    QString units = stripDrawerInlineRichTags(tbl->colUnits(c).trimmed());
    if (units.size() >= 2
        && ((units.startsWith(QLatin1Char('(')) && units.endsWith(QLatin1Char(')')))
            || (units.startsWith(QLatin1Char('[')) && units.endsWith(QLatin1Char(']'))))) {
        units = units.mid(1, units.size() - 2).trimmed();
    }

    const QString titleLine = longName.isEmpty() ? tbl->colLabel(c) : longName;
    QStringList metaParts;
    if (!roleTag.isEmpty())
        metaParts << roleTag;
    if (!colFormula.isEmpty())
        metaParts << FocusTableDrawer::tr("[fx]");
    if (!units.isEmpty())
        metaParts << QStringLiteral("(%1)").arg(units);
    return metaParts.isEmpty() ? titleLine
                               : (titleLine + QLatin1Char('\n') + metaParts.join(QLatin1Char(' ')));
}

static void adjustDrawerColumnWidths(QTableView *view, Table *tbl)
{
    if (!view || !tbl)
        return;
    QHeaderView *hh = view->horizontalHeader();
    if (!hh)
        return;
    hh->ensurePolished();
    QFont secFont = hh->font();
    if (secFont.pointSizeF() < 11.0 && secFont.pointSize() < 11)
        secFont.setPointSize(11);
    if (secFont.weight() < QFont::DemiBold)
        secFont.setWeight(QFont::DemiBold);
    const QFontMetrics fmBase(hh->font());
    const QFontMetrics fmSec(secFont);
    const int cols = tbl->numCols();
    for (int c = 0; c < cols; ++c) {
        const QString h = formatDrawerColumnHeader(tbl, c);
        int headerTextWidth = 0;
        const QStringList lines = h.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            headerTextWidth = qMax(
                    headerTextWidth,
                    qMax(fmBase.horizontalAdvance(line), fmSec.horizontalAdvance(line)));
        }
        hh->resizeSection(c, qBound(108, headerTextWidth + 28, 220));
    }
}

class FocusDrawerProxyModel : public QIdentityProxyModel
{
public:
    explicit FocusDrawerProxyModel(FocusTableDrawer *drawer)
        : QIdentityProxyModel(drawer), d_drawer(drawer)
    {
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid() || !d_drawer)
            return Qt::NoItemFlags;
        Table *tbl = d_drawer->currentTable();
        if (!tbl)
            return Qt::NoItemFlags;
        Column *colObj = tbl->column(index.column());
        const bool isFormula =
                (colObj && index.row() < tbl->numRows()
                 && !colObj->formula(index.row()).trimmed().isEmpty());
        Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (!isFormula)
            f |= Qt::ItemIsEditable;
        return f;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || !d_drawer)
            return QVariant();
        Table *tbl = d_drawer->currentTable();
        if (!tbl || index.row() < 0 || index.row() >= tbl->numRows() || index.column() < 0
            || index.column() >= tbl->numCols()) {
            return QVariant();
        }

        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return tbl->text(index.row(), index.column());

        if (role == Qt::BackgroundRole || role == Qt::ForegroundRole || role == Qt::ToolTipRole) {
            Column *colObj = tbl->column(index.column());
            const QString cellFormula =
                    colObj ? colObj->formula(index.row()).trimmed() : QString();
            if (!cellFormula.isEmpty()) {
                if (role == Qt::BackgroundRole)
                    return QColor(241, 245, 249);
                if (role == Qt::ForegroundRole)
                    return QColor(30, 64, 175);
                if (role == Qt::ToolTipRole)
                    return FocusTableDrawer::tr("Formula (read-only): %1").arg(cellFormula);
            }
            return QVariant();
        }

        return QIdentityProxyModel::data(index, role);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (!d_drawer)
            return QVariant();
        Table *tbl = d_drawer->currentTable();
        if (!tbl)
            return QVariant();

        if (orientation == Qt::Horizontal) {
            if (section < 0 || section >= tbl->numCols())
                return QVariant();
            if (role == Qt::DecorationRole)
                return QVariant();
            if (role == Qt::DisplayRole)
                return formatDrawerColumnHeader(tbl, section);
            if (role == Qt::ToolTipRole) {
                QString colFormula;
                formatDrawerColumnHeader(tbl, section, &colFormula);
                if (!colFormula.isEmpty())
                    return FocusTableDrawer::tr("Formula (read-only): %1").arg(colFormula);
                return QVariant();
            }
            return QIdentityProxyModel::headerData(section, orientation, role);
        }

        if (orientation == Qt::Vertical && role == Qt::DisplayRole)
            return QString::number(section + 1);

        return QIdentityProxyModel::headerData(section, orientation, role);
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        if (role != Qt::EditRole || !index.isValid() || !d_drawer)
            return false;
        if (!d_drawer->setCellValueFromDrawer(index.row(), index.column(), value.toString()))
            return false;
        emit dataChanged(index, index);
        return true;
    }

    void notifyAllHeadersChanged()
    {
        const int cols = columnCount();
        if (cols > 0)
            emit headerDataChanged(Qt::Horizontal, 0, cols - 1);
    }

private:
    FocusTableDrawer *d_drawer = nullptr;
};

} // namespace

FocusTableDrawer::FocusTableDrawer(ApplicationWindow *app, QWidget *parent)
    : QFrame(parent), d_app(app)
{
    setObjectName(QStringLiteral("FocusTableDrawerCard"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(255, 255, 255));
        pal.setColor(QPalette::Base, QColor(255, 255, 255));
        setPalette(pal);
    }
    setFixedWidth(396);
    setStyleSheet(QStringLiteral(
            "QFrame#FocusTableDrawerCard {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 12px;"
            "}"
            "QLabel#FocusDrawerTitle {"
            "  color: #0F172A;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 700;"
            "}"
            "QLabel#FocusDrawerEmpty {"
            "  color: #64748B;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  padding: 12px;"
            "}"
            "QToolButton.FocusDrawerBtn {"
            "  background-color: #F8FAFC;"
            "  color: #1E293B;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 6px;"
            "  padding: 3px 6px;"
            "  font-size: 11pt;"
            "}"
            "QToolButton.FocusDrawerBtn:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "}"
            "QTableView#FocusDrawerGrid, QTableWidget#FocusDrawerCompatGrid {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #E2E8F0;"
            "  border-radius: 8px;"
            "  gridline-color: #E2E8F0;"
            "  selection-background-color: #DBEAFE;"
            "  selection-color: #0F172A;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "}"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    QHBoxLayout *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(6);

    QLabel *iconLbl = new QLabel(this);
    iconLbl->setPixmap(IconLoader::duotonePixmap(QStringLiteral("table-drawer"), 18, 0.0, false));
    iconLbl->setFixedSize(20, 20);
    headerRow->addWidget(iconLbl);

    d_title_label = new QLabel(this);
    d_title_label->setObjectName(QStringLiteral("FocusDrawerTitle"));
    headerRow->addWidget(d_title_label, 1);

    d_btn_new_table = new QToolButton(this);
    d_btn_new_table->setProperty("class", QStringLiteral("FocusDrawerBtn"));
    d_btn_new_table->setIcon(IconLoader::duotoneIcon(QStringLiteral("new-table"), 16, false));
    d_btn_new_table->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(d_btn_new_table);

    d_btn_add_col = new QToolButton(this);
    d_btn_add_col->setProperty("class", QStringLiteral("FocusDrawerBtn"));
    d_btn_add_col->setIcon(IconLoader::duotoneIcon(QStringLiteral("add-column"), 16, false));
    d_btn_add_col->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(d_btn_add_col);

    d_btn_close = new QToolButton(this);
    d_btn_close->setProperty("class", QStringLiteral("FocusDrawerBtn"));
    d_btn_close->setIcon(IconLoader::duotoneIcon(QStringLiteral("clear"), 16, false));
    d_btn_close->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(d_btn_close);

    mainLayout->addLayout(headerRow);

    d_table_combo = new QComboBox(this);
    d_table_combo->setMinimumHeight(28);
    mainLayout->addWidget(d_table_combo);

    d_empty_label = new QLabel(this);
    d_empty_label->setObjectName(QStringLiteral("FocusDrawerEmpty"));
    d_empty_label->setAlignment(Qt::AlignCenter);
    d_empty_label->setWordWrap(true);
    mainLayout->addWidget(d_empty_label);

    d_proxy_model = new FocusDrawerProxyModel(this);
    d_table_view = new QTableView(this);
    d_table_view->setObjectName(QStringLiteral("FocusDrawerGrid"));
    d_table_view->setModel(d_proxy_model);
    d_table_view->setAlternatingRowColors(true);
    d_table_view->horizontalHeader()->setDefaultSectionSize(108);
    d_table_view->horizontalHeader()->setMinimumSectionSize(54);
    d_table_view->verticalHeader()->setDefaultSectionSize(24);
    d_table_view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    mainLayout->addWidget(d_table_view, 1);

    connect(d_proxy_model, &QAbstractItemModel::columnsInserted, this,
            [this](const QModelIndex &, int, int) {
                if (!d_shutting_down && !isHidden() && d_current_table)
                    adjustDrawerColumnWidths(d_table_view, d_current_table.data());
            });

    connect(d_btn_new_table, &QToolButton::clicked, this, [this]() {
        if (d_shutting_down || !d_app)
            return;
        Table *t = d_app->newTable();
        refreshTableList(false);
        if (t)
            setCurrentTable(t);
    });

    connect(d_btn_add_col, &QToolButton::clicked, this, [this]() {
        if (d_shutting_down || !d_current_table)
            return;
        d_current_table->addCol(SciDAVis::Y);
        syncGridFromTable();
    });

    connect(d_btn_close, &QToolButton::clicked, this, &FocusTableDrawer::closeDrawerRequested);

    connect(d_table_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                if (d_shutting_down || d_syncing || !d_app || idx < 0)
                    return;
                const QString tblName = d_table_combo->itemData(idx).toString();
                Table *target = d_app->table(tblName);
                if (target && !d_dying_tables.contains(target)) {
                    if (WorkbookManager *wb = WorkbookManager::existing(d_app)) {
                        const QString book = wb->bookOf(target);
                        if (!book.isEmpty() && wb->currentSheet(book) != target)
                            wb->setCurrentSheet(target, false);
                    }
                    setCurrentTable(target);
                }
            });

    if (d_app) {
        connect(&d_app->folders, &QObject::destroyed, this,
                &FocusTableDrawer::prepareForAppShutdown);
    }

    installEventFilter(this);
    if (d_table_view)
        d_table_view->installEventFilter(this);
    if (d_table_combo)
        d_table_combo->installEventFilter(this);

    retranslateUi();
}

FocusTableDrawer::~FocusTableDrawer()
{
    prepareForAppShutdown();
}

void FocusTableDrawer::prepareForAppShutdown()
{
    d_shutting_down = true;
    d_syncing = true;

    removeEventFilter(this);
    if (d_table_view)
        d_table_view->removeEventFilter(this);
    if (d_table_combo)
        d_table_combo->removeEventFilter(this);

    if (d_proxy_model)
        d_proxy_model->setSourceModel(nullptr);

    if (d_current_table) {
        disconnect(d_current_table.data(), nullptr, this, nullptr);
        d_current_table.clear();
    }
    d_dying_tables.clear();

    if (d_compat_grid)
        disconnect(d_compat_grid, nullptr, this, nullptr);
    if (d_table_combo)
        disconnect(d_table_combo, nullptr, this, nullptr);
    if (d_btn_new_table)
        disconnect(d_btn_new_table, nullptr, this, nullptr);
    if (d_btn_add_col)
        disconnect(d_btn_add_col, nullptr, this, nullptr);
    if (d_btn_close)
        disconnect(d_btn_close, nullptr, this, nullptr);

    d_app.clear();
}

Table *FocusTableDrawer::currentTable() const
{
    return d_current_table.data();
}

QTableWidget *FocusTableDrawer::tableWidget() const
{
    syncCompatGrid();
    return d_compat_grid;
}

void FocusTableDrawer::retranslateUi()
{
    if (d_shutting_down)
        return;
    if (d_title_label)
        d_title_label->setText(tr("Linked Worksheet"));
    if (d_btn_new_table)
        d_btn_new_table->setToolTip(tr("New Table"));
    if (d_btn_add_col)
        d_btn_add_col->setToolTip(tr("Add Column"));
    if (d_btn_close)
        d_btn_close->setToolTip(tr("Close Worksheet Drawer"));
    if (d_empty_label)
        d_empty_label->setText(tr("No worksheet available in the current project."));
    d_compat_grid_dirty = true;
    syncGridFromTable();
}

namespace {
bool isWorkbookManagedSheet(ApplicationWindow *app, MyWidget *mw)
{
    Table *tbl = qobject_cast<Table *>(mw);
    if (!app || !tbl)
        return false;
    WorkbookManager *wb = WorkbookManager::existing(app);
    if (!wb)
        return false;
    const QString book = wb->bookOf(tbl);
    if (book.isEmpty())
        return false;
    Table *cur = wb->currentSheet(book);
    return cur && cur->status() != MyWidget::Hidden && !app->hiddenWindows.contains(cur);
}

bool isUsableRibbonWorksheet(ApplicationWindow *app, MyWidget *mw)
{
    Table *tbl = qobject_cast<Table *>(mw);
    if (!app || !tbl)
        return false;
    if (tbl->name().startsWith(QLatin1String("RpcFitCurve")))
        return false;
    if (tbl->status() != MyWidget::Hidden && !app->hiddenWindows.contains(tbl))
        return true;
    return isWorkbookManagedSheet(app, tbl);
}

MultiLayer *activeMultiLayerOrUxHint(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    if (MultiLayer *ml = qobject_cast<MultiLayer *>(app->d_workspace.activeSubWindow()))
        return ml;
    app->showUxHint(ModernRibbonBar::tr("Click or create a graph window first."));
    return nullptr;
}

void hideBackgroundMdiWindowsForFocus(ApplicationWindow *app)
{
    if (!app)
        return;
    static bool s_busy = false;
    if (s_busy)
        return;
    s_busy = true;

    auto isEligibleVisible = [app](MyWidget *mw) -> bool {
        if (!mw || mw->isHidden() || mw->status() == MyWidget::Hidden)
            return false;
        if (app->hiddenWindows.contains(mw) || app->outWindows.contains(mw))
            return false;
        if (Folder *cur = app->currentFolder()) {
            if (!cur->windowsList().contains(mw))
                return false;
        }
        return true;
    };

    MyWidget *focusTarget = nullptr;
    if (MyWidget *aw = qobject_cast<MyWidget *>(app->d_workspace.activeSubWindow())) {
        if (isEligibleVisible(aw)
            && (qobject_cast<MultiLayer *>(aw) || qobject_cast<Graph3D *>(aw))) {
            focusTarget = aw;
        }
    }
    if (!focusTarget) {
        const QList<QMdiSubWindow *> ordered =
                app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
        for (int i = ordered.size() - 1; i >= 0; --i) {
            MyWidget *mw = qobject_cast<MyWidget *>(ordered.at(i));
            if (isEligibleVisible(mw)
                && (qobject_cast<MultiLayer *>(mw) || qobject_cast<Graph3D *>(mw))) {
                focusTarget = mw;
                break;
            }
        }
    }
    if (!focusTarget) {
        if (MyWidget *aw = qobject_cast<MyWidget *>(app->d_workspace.activeSubWindow())) {
            if (isEligibleVisible(aw))
                focusTarget = aw;
        }
    }
    if (!focusTarget) {
        const QList<QMdiSubWindow *> ordered =
                app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
        for (int i = ordered.size() - 1; i >= 0; --i) {
            MyWidget *mw = qobject_cast<MyWidget *>(ordered.at(i));
            if (isEligibleVisible(mw)) {
                focusTarget = mw;
                break;
            }
        }
    }

    if (focusTarget) {
        focusTarget->setProperty("_tg_hidden_for_focus_mdi", QVariant());
        if (app->d_workspace.activeSubWindow() != focusTarget)
            app->d_workspace.setActiveSubWindow(focusTarget);

        const QList<QMdiSubWindow *> subs = app->d_workspace.subWindowList();
        for (QMdiSubWindow *sw : subs) {
            MyWidget *mw = qobject_cast<MyWidget *>(sw);
            if (!mw || mw == focusTarget)
                continue;
            if (!isEligibleVisible(mw))
                continue;
            mw->setProperty("_tg_hidden_for_focus_mdi", true);
            mw->hide();
        }
    }

    s_busy = false;
}

void restoreBackgroundMdiWindowsAfterFocus(ApplicationWindow *app)
{
    if (!app)
        return;
    static bool s_busy = false;
    if (s_busy)
        return;
    s_busy = true;

    QMdiSubWindow *keepActive = app->d_workspace.activeSubWindow();
    const QList<QMdiSubWindow *> subs = app->d_workspace.subWindowList(QMdiArea::CreationOrder);
    for (QMdiSubWindow *sw : subs) {
        MyWidget *mw = qobject_cast<MyWidget *>(sw);
        if (!mw || !mw->property("_tg_hidden_for_focus_mdi").toBool())
            continue;
        mw->setProperty("_tg_hidden_for_focus_mdi", QVariant());
        if (mw->status() == MyWidget::Hidden || app->hiddenWindows.contains(mw)
            || app->outWindows.contains(mw)) {
            continue;
        }
        if (Folder *cur = app->currentFolder()) {
            if (!cur->windowsList().contains(mw))
                continue;
        }
        if (mw->status() == MyWidget::Minimized)
            mw->showMinimized();
        else if (mw->status() == MyWidget::Maximized)
            mw->showMaximized();
        else
            mw->show();
    }
    if (keepActive && !keepActive->isHidden()) {
        app->d_workspace.setActiveSubWindow(keepActive);
        keepActive->raise();
    }

    s_busy = false;
}
} // namespace

Table *FocusTableDrawer::detectPreferredTable(const QSet<QObject *> &ignoredObjects) const
{
    if (d_shutting_down || !d_app)
        return nullptr;

    QSet<QObject *> effectiveIgnored = ignoredObjects;
    for (MyWidget *mw : d_app->hiddenWindows) {
        if (mw && !isWorkbookManagedSheet(d_app, mw))
            effectiveIgnored.insert(mw);
    }

    auto isUsableTable = [this, &effectiveIgnored](Table *tbl) -> bool {
        if (!tbl || effectiveIgnored.contains(tbl))
            return false;
        if (!isUsableRibbonWorksheet(d_app, tbl))
            return false;
        if (Folder *curFolder = d_app->currentFolder()) {
            if (!curFolder->windowsList().contains(tbl))
                return false;
        }
        return true;
    };

    QMdiSubWindow *activeSub = d_app->d_workspace.activeSubWindow();
    if (activeSub && !effectiveIgnored.contains(activeSub)) {
        if (Table *activeTbl = qobject_cast<Table *>(activeSub)) {
            if (isUsableTable(activeTbl))
                return activeTbl;
        }
        if (MultiLayer *activeMl = qobject_cast<MultiLayer *>(activeSub)) {
            if (Table *srcTbl = sourceTableFromMultiLayer(activeMl, effectiveIgnored)) {
                if (isUsableTable(srcTbl))
                    return srcTbl;
            }
        }
    }

    const QList<QMdiSubWindow *> ordered =
            d_app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
    for (int i = ordered.size() - 1; i >= 0; --i) {
        QMdiSubWindow *sw = ordered.at(i);
        if (!sw || effectiveIgnored.contains(sw))
            continue;
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(sw)) {
            if (Folder *curFolder = d_app->currentFolder()) {
                if (!curFolder->windowsList().contains(ml))
                    continue;
            }
            if (Table *srcTbl = sourceTableFromMultiLayer(ml, effectiveIgnored)) {
                if (isUsableTable(srcTbl))
                    return srcTbl;
            }
        }
    }
    for (int i = ordered.size() - 1; i >= 0; --i) {
        QMdiSubWindow *sw = ordered.at(i);
        if (!sw || effectiveIgnored.contains(sw))
            continue;
        if (Table *tbl = qobject_cast<Table *>(sw)) {
            if (isUsableTable(tbl))
                return tbl;
        }
    }
    return nullptr;
}

void FocusTableDrawer::refreshTableList(bool autoSelectActiveCurveTable)
{
    if (d_shutting_down)
        return;
    refreshTableListInternal(autoSelectActiveCurveTable, d_dying_tables);
}

void FocusTableDrawer::refreshTableListInternal(bool autoSelectActiveCurveTable,
                                                const QSet<QObject *> &ignoredObjects)
{
    if (d_shutting_down || !d_table_combo)
        return;

    QVector<Table *> liveTables;
    if (d_app) {
        auto appendIfUserTable = [this, &ignoredObjects, &liveTables](MyWidget *mw) {
            if (!mw || ignoredObjects.contains(mw))
                return;
            if (!isUsableRibbonWorksheet(d_app, mw))
                return;
            if (Table *tbl = qobject_cast<Table *>(mw)) {
                if (!liveTables.contains(tbl))
                    liveTables.append(tbl);
            }
        };

        if (Folder *curFolder = d_app->currentFolder()) {
            const QList<MyWidget *> folderWins = curFolder->windowsList();
            for (MyWidget *mw : folderWins)
                appendIfUserTable(mw);
        }
        const QList<QMdiSubWindow *> subs = d_app->d_workspace.subWindowList();
        for (QMdiSubWindow *sw : subs)
            appendIfUserTable(qobject_cast<MyWidget *>(sw));
        for (MyWidget *mw : d_app->outWindows)
            appendIfUserTable(mw);
    }

    Table *desired = nullptr;
    if (autoSelectActiveCurveTable)
        desired = detectPreferredTable(ignoredObjects);
    if (!desired && d_current_table && !ignoredObjects.contains(d_current_table.data())
        && liveTables.contains(d_current_table.data())) {
        desired = d_current_table.data();
    }
    if (!desired && !liveTables.isEmpty())
        desired = liveTables.first();

    {
        QSignalBlocker blocker(d_table_combo);
        d_table_combo->clear();
        int selectIdx = -1;
        for (int i = 0; i < liveTables.size(); ++i) {
            Table *t = liveTables.at(i);
            const QString lbl = t->windowLabel().trimmed();
            const QString disp = lbl.isEmpty()
                    ? t->name()
                    : QStringLiteral("%1 — %2").arg(t->name(), lbl);
            d_table_combo->addItem(
                    IconLoader::duotoneIcon(QStringLiteral("new-table"), 16, false), disp,
                    t->name());
            if (t == desired)
                selectIdx = i;
        }
        if (selectIdx >= 0)
            d_table_combo->setCurrentIndex(selectIdx);
    }

    setCurrentTable(desired);
}

void FocusTableDrawer::bindTableSignals(Table *table)
{
    if (d_shutting_down) {
        if (d_current_table) {
            disconnect(d_current_table.data(), nullptr, this, nullptr);
            d_current_table.clear();
        }
        return;
    }

    if (d_current_table == table)
        return;

    if (d_current_table)
        disconnect(d_current_table.data(), nullptr, this, nullptr);

    d_current_table = table;

    if (d_current_table) {
        connect(d_current_table.data(), &QObject::destroyed, this,
                &FocusTableDrawer::onCurrentTableDestroyed, Qt::UniqueConnection);
        connect(d_current_table.data(), &Table::modifiedData, this,
                [this](Table *, const QString &) {
                    if (!d_shutting_down && !d_syncing)
                        syncGridFromTable();
                });
        connect(d_current_table.data(), &Table::changedColHeader, this,
                [this](const QString &, const QString &) {
                    if (!d_shutting_down && !d_syncing)
                        syncGridFromTable();
                });
        connect(d_current_table.data(), &Table::removedCol, this, [this](const QString &) {
            if (!d_shutting_down && !d_syncing)
                syncGridFromTable();
        });
    }
}

void FocusTableDrawer::setCurrentTable(Table *table)
{
    if (d_shutting_down)
        return;

    if (table && d_dying_tables.contains(table))
        table = nullptr;

    bindTableSignals(table);

    if (d_table_combo && d_current_table) {
        const int idx = d_table_combo->findData(d_current_table->name());
        if (idx >= 0 && d_table_combo->currentIndex() != idx) {
            QSignalBlocker blocker(d_table_combo);
            d_table_combo->setCurrentIndex(idx);
        }
    }

    if (d_btn_add_col)
        d_btn_add_col->setEnabled(!d_current_table.isNull());

    syncGridFromTable();
}

void FocusTableDrawer::onTableClosed(Table *table)
{
    if (d_shutting_down || !d_app || !table)
        return;
    const bool wasCurrent = (d_current_table == table);
    d_dying_tables.insert(table);
    if (wasCurrent)
        bindTableSignals(nullptr);
    refreshTableListInternal(wasCurrent, d_dying_tables);
    d_dying_tables.remove(table);
}

void FocusTableDrawer::onCurrentTableDestroyed(QObject *destroyedObj)
{
    if (d_shutting_down || !d_app) {
        d_current_table.clear();
        return;
    }
    if (destroyedObj)
        d_dying_tables.insert(destroyedObj);
    d_current_table.clear();
    refreshTableListInternal(true, d_dying_tables);
    if (destroyedObj)
        d_dying_tables.remove(destroyedObj);
}

void FocusTableDrawer::syncGridFromTable()
{
    if (d_shutting_down || !d_table_view || !d_proxy_model || !d_empty_label)
        return;

    d_compat_grid_dirty = true;

    Table *tbl = d_current_table.data();
    if (!tbl || d_dying_tables.contains(tbl)) {
        d_proxy_model->setSourceModel(nullptr);
        d_table_view->hide();
        d_empty_label->show();
        if (d_compat_grid) {
            QSignalBlocker blocker(d_compat_grid);
            d_compat_grid->clear();
            d_compat_grid->setRowCount(0);
            d_compat_grid->setColumnCount(0);
            d_compat_grid_dirty = false;
        }
        return;
    }

    d_empty_label->hide();
    d_table_view->show();

    if (isHidden()) {
        // Defer binding while drawer is closed so zero signals/layout work run when hidden
        d_dirty = true;
        if (d_proxy_model->sourceModel() != nullptr)
            d_proxy_model->setSourceModel(nullptr);
        return;
    }

    d_dirty = false;
    TableModel *srcModel = tbl->tableModel();
    if (d_proxy_model->sourceModel() != srcModel)
        d_proxy_model->setSourceModel(srcModel);
    if (auto *fp = static_cast<FocusDrawerProxyModel *>(d_proxy_model))
        fp->notifyAllHeadersChanged();
    adjustDrawerColumnWidths(d_table_view, tbl);
    if (d_table_view->viewport())
        d_table_view->viewport()->update();

    if (d_compat_grid && tbl->numRows() <= 200)
        syncCompatGrid();
}

void FocusTableDrawer::syncCompatGrid() const
{
    if (d_shutting_down)
        return;

    FocusTableDrawer *self = const_cast<FocusTableDrawer *>(this);
    if (!d_compat_grid) {
        d_compat_grid = new QTableWidget(self);
        d_compat_grid->setObjectName(QStringLiteral("FocusDrawerCompatGrid"));
        d_compat_grid->hide();
        connect(d_compat_grid, &QTableWidget::cellChanged, self,
                &FocusTableDrawer::onGridCellChanged);
        d_compat_grid_dirty = true;
    }

    if (!d_compat_grid_dirty)
        return;

    d_compat_grid_dirty = false;
    QSignalBlocker blocker(d_compat_grid);

    Table *tbl = d_current_table.data();
    if (!tbl || d_dying_tables.contains(tbl)) {
        d_compat_grid->clear();
        d_compat_grid->setRowCount(0);
        d_compat_grid->setColumnCount(0);
        return;
    }

    const int rows = qBound(0, tbl->numRows(), 200);
    const int cols = qMax(0, tbl->numCols());
    d_compat_grid->setRowCount(rows);
    d_compat_grid->setColumnCount(cols);

    for (int c = 0; c < cols; ++c) {
        QString colFormula;
        const QString h = formatDrawerColumnHeader(tbl, c, &colFormula);
        QTableWidgetItem *hdrItem = new QTableWidgetItem(h);
        if (!colFormula.isEmpty())
            hdrItem->setToolTip(tr("Formula (read-only): %1").arg(colFormula));
        d_compat_grid->setHorizontalHeaderItem(c, hdrItem);
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Column *colObj = tbl->column(c);
            const QString cellFormula = colObj ? colObj->formula(r).trimmed() : QString();
            const bool isFormulaCell = !cellFormula.isEmpty();
            const QString val = tbl->text(r, c);
            QTableWidgetItem *it = d_compat_grid->item(r, c);
            if (!it) {
                it = new QTableWidgetItem(val);
                d_compat_grid->setItem(r, c, it);
            } else if (it->text() != val) {
                it->setText(val);
            }

            if (isFormulaCell) {
                it->setFlags((it->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable)
                             & ~Qt::ItemIsEditable);
                it->setBackground(QColor(241, 245, 249));
                it->setForeground(QColor(30, 64, 175));
                it->setToolTip(tr("Formula (read-only): %1").arg(cellFormula));
            } else {
                it->setFlags(it->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable
                             | Qt::ItemIsEditable);
                it->setData(Qt::BackgroundRole, QVariant());
                it->setData(Qt::ForegroundRole, QVariant());
                it->setToolTip(QString());
            }
        }
    }
}

void FocusTableDrawer::onGridCellChanged(int row, int col)
{
    if (d_shutting_down || d_syncing || !d_current_table || !d_compat_grid)
        return;
    QTableWidgetItem *it = d_compat_grid->item(row, col);
    if (!it || !(it->flags() & Qt::ItemIsEditable))
        return;
    setCellValueFromDrawer(row, col, it->text());
}

bool FocusTableDrawer::setCellValueFromDrawer(int row, int col, const QString &text)
{
    if (d_shutting_down)
        return false;
    Table *tbl = d_current_table.data();
    if (!tbl || d_dying_tables.contains(tbl))
        return false;
    if (row < 0 || col < 0 || col >= tbl->numCols())
        return false;
    if (Column *colObj = tbl->column(col)) {
        if (row < tbl->numRows() && !colObj->formula(row).trimmed().isEmpty())
            return false;
        if (row >= tbl->numRows() && !colObj->formulaIntervals().isEmpty()) {
            const Interval<int> lastIv = colObj->formulaIntervals().last();
            if (tbl->numRows() > 0 && lastIv.end() >= tbl->numRows() - 1
                && !colObj->formula(lastIv.start()).trimmed().isEmpty()) {
                return false;
            }
        }
    }
    if (row >= tbl->numRows())
        tbl->setNumRows(row + 1);

    d_syncing = true;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        tbl->clearCell(row, col);
    } else if (tbl->columnType(col) == SciDAVis::ColumnMode::Numeric) {
        bool ok = false;
        double numericVal = QLocale().toDouble(trimmed, &ok);
        if (!ok)
            numericVal = trimmed.toDouble(&ok);
        if (ok) {
            tbl->setCell(row, col, numericVal);
        } else {
            Column *colObj = tbl->column(col);
            const QString lower = trimmed.toLower();
            const bool isNullSentinel =
                    (trimmed == QLatin1String("-") || trimmed == QLatin1String("--")
                     || lower == QLatin1String("nan") || lower == QLatin1String("na")
                     || lower == QLatin1String("n/a") || lower == QLatin1String("null"));
            bool hasAnyFinite = false;
            if (colObj && !isNullSentinel && colObj->formulaIntervals().isEmpty()) {
                const int nRows = colObj->rowCount();
                for (int r = 0; r < nRows; ++r) {
                    if (!colObj->isInvalid(r) && std::isfinite(colObj->valueAt(r))) {
                        hasAnyFinite = true;
                        break;
                    }
                }
                if (!hasAnyFinite) {
                    colObj->setColumnMode(SciDAVis::ColumnMode::Text);
                    colObj->setTextAt(row, trimmed);
                } else {
                    tbl->setText(row, col, trimmed);
                }
            } else {
                tbl->setText(row, col, trimmed);
            }
        }
    } else {
        tbl->setText(row, col, text);
    }
    d_syncing = false;

    d_compat_grid_dirty = true;
    syncGridFromTable();
    return true;
}

void FocusTableDrawer::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void FocusTableDrawer::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    raise();
    if (!d_shutting_down && (d_dirty || !d_proxy_model || !d_proxy_model->sourceModel())) {
        syncGridFromTable();
    }
}

void FocusTableDrawer::hideEvent(QHideEvent *event)
{
    QFrame::hideEvent(event);
    if (d_proxy_model && d_proxy_model->sourceModel() != nullptr) {
        d_proxy_model->setSourceModel(nullptr);
        d_dirty = true;
    }
}

bool FocusTableDrawer::eventFilter(QObject *watched, QEvent *event)
{
    if (!d_shutting_down && event && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            emit closeDrawerRequested();
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

// ============================================================================
// FocusCanvasHudOverlay
// ============================================================================

static void syncFocusHudOleButton(QToolButton *btnOle, ApplicationWindow *app,
                                  ModernRibbonBar *ribbon = nullptr)
{
    if (!btnOle)
        return;
    bool hasGraph = false;
    QString tip;
    if (app) {
        if (QAction *aCopyOle = app->findChild<QAction *>(QStringLiteral("actionCopyGraphOle"))) {
            hasGraph = aCopyOle->isEnabled();
            tip = aCopyOle->toolTip();
        } else {
            QMdiSubWindow *sw = app->d_workspace.activeSubWindow();
            hasGraph = (qobject_cast<MultiLayer *>(sw) != nullptr
                        || qobject_cast<Graph3D *>(sw) != nullptr);
        }
        if (!hasGraph && ribbon
            && ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
            MyWidget *splitPlot = ribbon->splitPlotTarget();
            hasGraph = (qobject_cast<MultiLayer *>(splitPlot) != nullptr
                        || qobject_cast<Graph3D *>(splitPlot) != nullptr);
        }
    }
    if (!hasGraph) {
        const QString disabledHint = FocusCanvasHudOverlay::tr(
                "Create or select a graph window first to copy for PowerPoint.");
        if (tip.isEmpty() || !tip.contains(disabledHint)) {
            tip = ToriGraphOleUi::copyForPowerPointToolTip() + QStringLiteral("\n")
                    + disabledHint;
        }
    } else if (tip.isEmpty()) {
        tip = ToriGraphOleUi::copyForPowerPointToolTip();
    }
    btnOle->setEnabled(hasGraph);
    btnOle->setToolTip(tip);
}

FocusCanvasHudOverlay::FocusCanvasHudOverlay(ApplicationWindow *app, ModernRibbonBar *ribbon,
                                             QWidget *viewportParent)
    : QFrame(viewportParent), d_app(app), d_ribbon(ribbon), d_viewport(viewportParent)
{
    setObjectName(QStringLiteral("FocusCanvasDarkPill"));
    setStyleSheet(QStringLiteral(
            "QFrame#FocusCanvasDarkPill {"
            "  background-color: rgba(15, 23, 42, 235);"
            "  border: 1px solid rgba(148, 163, 184, 95);"
            "  border-radius: 14px;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton {"
            "  background-color: transparent;"
            "  color: #F8FAFC;"
            "  border: 1px solid transparent;"
            "  border-radius: 9px;"
            "  padding: 4px 8px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 600;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton:hover:enabled {"
            "  background-color: rgba(59, 130, 246, 75);"
            "  border: 1px solid rgba(96, 165, 250, 140);"
            "  color: #FFFFFF;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton:disabled {"
            "  color: #CBD5E1;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
            "}"));

    QHBoxLayout *pillLayout = new QHBoxLayout(this);
    pillLayout->setContentsMargins(8, 4, 8, 4);
    pillLayout->setSpacing(3);

    auto makeDarkBtn = [this, pillLayout](const QString &iconId,
                                          std::function<void()> onClick) -> QToolButton * {
        QToolButton *btn = new QToolButton(this);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setIcon(IconLoader::duotoneIcon(iconId, 18, true));
        btn->setIconSize(QSize(18, 18));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::TabFocus);
        if (onClick)
            connect(btn, &QToolButton::clicked, this, std::move(onClick));
        pillLayout->addWidget(btn);
        return btn;
    };

    auto addPillSep = [this, pillLayout]() {
        QFrame *sep = new QFrame(this);
        sep->setFixedSize(1, 18);
        sep->setStyleSheet(QStringLiteral("background-color: rgba(148, 163, 184, 75);"));
        pillLayout->addWidget(sep);
    };

    d_btn_plot = makeDarkBtn(QStringLiteral("plot-line-symbol"), nullptr);
    d_btn_plot->setPopupMode(QToolButton::InstantPopup);
    d_plot_menu = new QMenu(d_btn_plot);
    connect(d_plot_menu, &QMenu::aboutToShow, this, &FocusCanvasHudOverlay::populateQuickPlotMenu);
    d_btn_plot->setMenu(d_plot_menu);

    d_btn_analyze = makeDarkBtn(QStringLiteral("peak-analyzer"), nullptr);
    d_btn_analyze->setPopupMode(QToolButton::InstantPopup);
    d_analyze_menu = new QMenu(d_btn_analyze);
    connect(d_analyze_menu, &QMenu::aboutToShow, this, &FocusCanvasHudOverlay::populateAnalyzeMenu);
    d_btn_analyze->setMenu(d_analyze_menu);

    addPillSep();

    d_btn_presets = makeDarkBtn(QStringLiteral("preset-slide"), nullptr);
    d_btn_presets->setPopupMode(QToolButton::InstantPopup);
    d_presets_menu = new QMenu(d_btn_presets);
    connect(d_presets_menu, &QMenu::aboutToShow, this, &FocusCanvasHudOverlay::populatePresetsMenu);
    d_btn_presets->setMenu(d_presets_menu);

    d_btn_ole = makeDarkBtn(QStringLiteral("copy-ole"), [this]() {
        if (d_shutting_down || !d_app)
            return;
        QMdiSubWindow *sw = d_app->d_workspace.activeSubWindow();
        if (qobject_cast<MultiLayer *>(sw) == nullptr && qobject_cast<Graph3D *>(sw) == nullptr
            && d_ribbon
            && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
            if (MyWidget *splitPlot = d_ribbon->splitPlotTarget())
                d_app->activateSubWindow(splitPlot);
        }
        d_app->copyGraphAsOleObject();
    });

    addPillSep();

    d_btn_palette = makeDarkBtn(QStringLiteral("search-command"), [this]() {
        if (!d_shutting_down && d_ribbon)
            d_ribbon->openCommandPalette();
    });

    const QString edgePillStyle = QStringLiteral(
            "QToolButton {"
            "  background-color: rgba(15, 23, 42, 230);"
            "  color: #F8FAFC;"
            "  border: 1px solid rgba(148, 163, 184, 95);"
            "  border-radius: 12px;"
            "  padding: 5px 11px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 600;"
            "}"
            "QToolButton:hover {"
            "  background-color: rgba(37, 99, 235, 225);"
            "  border: 1px solid #60A5FA;"
            "  color: #FFFFFF;"
            "}"
            "QToolButton:checked {"
            "  background-color: #2563EB;"
            "  border: 1px solid #93C5FD;"
            "  color: #FFFFFF;"
            "}"
            "QToolButton:disabled {"
            "  color: #CBD5E1;"
            "}");

    d_btn_worksheet = new QToolButton(viewportParent);
    d_btn_worksheet->setObjectName(QStringLiteral("FocusHudWorksheetButton"));
    d_btn_worksheet->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_worksheet->setIcon(IconLoader::duotoneIcon(QStringLiteral("table-drawer"), 18, true));
    d_btn_worksheet->setIconSize(QSize(18, 18));
    d_btn_worksheet->setCheckable(true);
    d_btn_worksheet->setCursor(Qt::PointingHandCursor);
    d_btn_worksheet->setFocusPolicy(Qt::TabFocus);
    d_btn_worksheet->setStyleSheet(edgePillStyle);

    d_btn_exit_focus = new QToolButton(viewportParent);
    d_btn_exit_focus->setObjectName(QStringLiteral("FocusHudExitButton"));
    d_btn_exit_focus->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_exit_focus->setIcon(IconLoader::duotoneIcon(QStringLiteral("exit-focus-hud"), 18, true));
    d_btn_exit_focus->setIconSize(QSize(18, 18));
    d_btn_exit_focus->setCursor(Qt::PointingHandCursor);
    d_btn_exit_focus->setFocusPolicy(Qt::TabFocus);
    d_btn_exit_focus->setStyleSheet(edgePillStyle);

    d_table_drawer = new FocusTableDrawer(app, viewportParent);

    connect(d_btn_worksheet.data(), &QToolButton::toggled, this,
            &FocusCanvasHudOverlay::setTableDrawerVisible);
    connect(d_table_drawer.data(), &FocusTableDrawer::closeDrawerRequested, this,
            [this]() { setTableDrawerVisible(false); });
    connect(d_btn_exit_focus.data(), &QToolButton::clicked, this, [this]() {
        if (!d_shutting_down && d_ribbon)
            d_ribbon->setFocusHudMode(false);
    });

    if (d_viewport)
        d_viewport->installEventFilter(this);

    hide();
    d_btn_worksheet->hide();
    d_btn_exit_focus->hide();
    d_table_drawer->hide();

    retranslateUi();
}

FocusCanvasHudOverlay::~FocusCanvasHudOverlay()
{
    prepareForAppShutdown();
    if (d_btn_worksheet)
        delete d_btn_worksheet.data();
    if (d_btn_exit_focus)
        delete d_btn_exit_focus.data();
    if (d_table_drawer)
        delete d_table_drawer.data();
}

void FocusCanvasHudOverlay::prepareForAppShutdown()
{
    d_shutting_down = true;
    d_active = false;

    if (d_viewport) {
        d_viewport->removeEventFilter(this);
        d_viewport.clear();
    }
    if (d_table_drawer) {
        disconnect(d_table_drawer.data(), nullptr, this, nullptr);
        d_table_drawer->prepareForAppShutdown();
    }
    if (d_btn_worksheet)
        disconnect(d_btn_worksheet.data(), nullptr, this, nullptr);
    if (d_btn_exit_focus)
        disconnect(d_btn_exit_focus.data(), nullptr, this, nullptr);
    if (d_plot_menu)
        disconnect(d_plot_menu, nullptr, this, nullptr);
    if (d_analyze_menu)
        disconnect(d_analyze_menu, nullptr, this, nullptr);
    if (d_presets_menu)
        disconnect(d_presets_menu, nullptr, this, nullptr);

    d_app.clear();
    d_ribbon.clear();
}

FocusTableDrawer *FocusCanvasHudOverlay::tableDrawer() const
{
    return d_table_drawer.data();
}

QToolButton *FocusCanvasHudOverlay::worksheetToggleButton() const
{
    return d_btn_worksheet.data();
}

QToolButton *FocusCanvasHudOverlay::exitFocusButton() const
{
    return d_btn_exit_focus.data();
}

void FocusCanvasHudOverlay::populateQuickPlotMenu()
{
    if (d_shutting_down || !d_plot_menu)
        return;
    d_plot_menu->clear();
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-line"), 18, false),
                           tr("Line"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::Line);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-scatter"), 18, false),
                           tr("Scatter"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::Scatter);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-line-symbol"), 18, false),
                           tr("Line + Symbol"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::LineSymbols);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-bars"), 18, false),
                           tr("Bar Chart"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::VerticalBars);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-bars"), 18, false),
                           tr("Stacked Bars"), this, [this]() {
                               if (!d_shutting_down && d_app) {
                                   if (!QMetaObject::invokeMethod(d_app.data(), "plotStackedBars",
                                                                  Qt::DirectConnection)
                                       && d_ribbon) {
                                       d_ribbon->triggerPlot2D(Graph::VerticalBars);
                                   }
                               }
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-box"), 18, false),
                           tr("Box Plot"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::Box);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-violin"), 18, false),
                           tr("Violin Plot"), this, [this]() {
                               if (!d_shutting_down && d_app)
                                   d_app->plotViolin();
                           });
    d_plot_menu->addSeparator();
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-heatmap"), 18, false),
                           tr("Heatmap"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerMatrixOrXyzPlot(0);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-contour"), 18, false),
                           tr("Contour"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerMatrixOrXyzPlot(1);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-3d-surface"), 18, false),
                           tr("3D Surface"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerMatrixOrXyzPlot(2);
                           });
}

void FocusCanvasHudOverlay::populateAnalyzeMenu()
{
    if (d_shutting_down || !d_analyze_menu)
        return;
    d_analyze_menu->clear();
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("peak-analyzer"), 18, false),
                              tr("Peak Analyzer"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->showPeakAnalyzerDialog(0);
                              });
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("gadget-roi"), 18, false),
                              tr("ROI Stats"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->attachRegionStatsGadget();
                              });
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("gadget-integrate"), 18, false),
                              tr("Integrate"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->attachIntegrateGadget();
                              });
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("gadget-quick-fit"), 18, false),
                              tr("Quick Fit"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->attachQuickFitGadget();
                              });
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("gadget-profile-2d"), 18, false),
                              tr("2D Profile"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->attachImageProfileGadget();
                              });
    d_analyze_menu->addSeparator();
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("gadget-clear"), 18, false),
                              tr("Clear Gadget"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->clearActiveGadget();
                              });
}

void FocusCanvasHudOverlay::populatePresetsMenu()
{
    if (d_shutting_down || !d_presets_menu)
        return;
    d_presets_menu->clear();
    d_presets_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("preset-isij"), 18, false),
                              tr("ISIJ 85mm Preset"), this, [this]() {
                                  if (!d_shutting_down && d_app && activeMultiLayerOrUxHint(d_app))
                                      d_app->applyStyleISIJJournal();
                              });
    d_presets_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("preset-slide"), 18, false),
                              tr("Lab Slide (18pt)"), this, [this]() {
                                  if (!d_shutting_down && d_app) {
                                      if (MultiLayer *ml = activeMultiLayerOrUxHint(d_app)) {
                                          ml->applyStylePresetById(QStringLiteral("lab_slide_83mm"));
                                          d_app->modifiedProject();
                                          if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                                              insp->refreshAll();
                                      }
                                  }
                              });
    d_presets_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("export-pptx"), 18, false),
                              ToriGraphPptx::sendToPowerPointLabel(), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      ToriGraphPptx::sendGraphsToPowerPoint(d_app);
                              });
    d_presets_menu->addSeparator();
    d_presets_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 18, false),
                              tr("Fit to Window"), this, [this]() {
                                  if (!d_shutting_down && d_app) {
                                      if (MultiLayer *ml = activeMultiLayerOrUxHint(d_app)) {
                                          if (!QMetaObject::invokeMethod(ml, "fitPageZoomToViewport",
                                                                         Qt::DirectConnection)) {
                                              ml->setPageZoom(1.0);
                                          }
                                      }
                                  }
                              });
    d_presets_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("page-setup"), 18, false),
                              tr("Manage Style Presets..."), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->showStylePresetManagerDialog();
                              });
}

void FocusCanvasHudOverlay::retranslateUi()
{
    if (d_shutting_down)
        return;
    const QString kCaret = QString::fromUtf8(" \xE2\x96\xBE");
    if (d_btn_plot) {
        d_btn_plot->setText(tr("Plot") + kCaret);
        d_btn_plot->setToolTip(tr("Quick Plot from Active Worksheet"));
    }
    if (d_btn_analyze) {
        d_btn_analyze->setText(tr("Analyze") + kCaret);
        d_btn_analyze->setToolTip(tr("Peaks, ROI Statistics, Integration, Quick Fit & 2D Profile"));
    }
    if (d_btn_presets) {
        d_btn_presets->setText(tr("Presets") + kCaret);
        d_btn_presets->setToolTip(tr("Apply ISIJ 85mm, Lab Slide (18pt), or Custom Style Presets"));
    }
    if (d_btn_ole) {
        d_btn_ole->setText(ToriGraphOleUi::copyForPowerPointLabel());
        syncFocusHudOleButton(d_btn_ole, d_app.data(), d_ribbon.data());
    }
    if (d_btn_palette) {
        d_btn_palette->setText(tr("Search"));
        d_btn_palette->setToolTip(tr("Open Command Palette (Ctrl+K)"));
    }
    if (d_btn_worksheet) {
        d_btn_worksheet->setText(tr("Worksheet"));
        d_btn_worksheet->setToolTip(tr("Toggle Slide-Over Linked Worksheet Drawer"));
    }
    if (d_btn_exit_focus) {
        d_btn_exit_focus->setText(tr("Exit Focus (F11)"));
        d_btn_exit_focus->setToolTip(tr("Exit Focus HUD Mode and Restore Ribbon (F11)"));
    }
    if (d_table_drawer)
        d_table_drawer->retranslateUi();

    populateQuickPlotMenu();
    populateAnalyzeMenu();
    populatePresetsMenu();
    if (d_active)
        repositionOverlayWidgets();
}

void FocusCanvasHudOverlay::setHostViewport(QWidget *host)
{
    if (d_shutting_down || !host || d_viewport == host)
        return;

    if (d_viewport)
        d_viewport->removeEventFilter(this);

    d_viewport = host;
    d_viewport->installEventFilter(this);

    auto reparentKeepVis = [host](QWidget *w) {
        if (!w || w->parentWidget() == host)
            return;
        const bool wasVis = !w->isHidden();
        w->setParent(host);
        w->setVisible(wasVis);
    };

    reparentKeepVis(this);
    reparentKeepVis(d_btn_worksheet.data());
    reparentKeepVis(d_btn_exit_focus.data());
    reparentKeepVis(d_table_drawer.data());

    if (d_active)
        repositionOverlayWidgets();
}

void FocusCanvasHudOverlay::setOverlayActive(bool active)
{
    if (d_shutting_down) {
        d_active = false;
        return;
    }
    d_active = active;
    if (!d_active) {
        hide();
        if (d_btn_worksheet) {
            QSignalBlocker blocker(d_btn_worksheet.data());
            d_btn_worksheet->setChecked(false);
            d_btn_worksheet->hide();
        }
        if (d_btn_exit_focus)
            d_btn_exit_focus->hide();
        if (d_table_drawer)
            d_table_drawer->hide();
        if (d_ribbon && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph
            && !d_ribbon->isFocusHudMode()) {
            d_ribbon->applySplitWorkspaceGeometry();
        }
        return;
    }

    if (d_app) {
        QWidget *preferredHost = d_app->centralWidget() ? d_app->centralWidget()
                                                        : d_app->d_workspace.viewport();
        if (preferredHost)
            setHostViewport(preferredHost);
    }

    if (d_table_drawer && isTableDrawerVisible())
        d_table_drawer->refreshTableList(true);

    syncFocusHudOleButton(d_btn_ole, d_app.data(), d_ribbon.data());

    show();
    if (d_btn_worksheet)
        d_btn_worksheet->show();
    if (d_btn_exit_focus)
        d_btn_exit_focus->show();

    repositionOverlayWidgets();
    if (d_ribbon && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph)
        d_ribbon->applySplitWorkspaceGeometry();
}

void FocusCanvasHudOverlay::setTableDrawerVisible(bool visible)
{
    if (d_shutting_down)
        return;

    if (d_btn_worksheet && d_btn_worksheet->isChecked() != visible) {
        QSignalBlocker blocker(d_btn_worksheet.data());
        d_btn_worksheet->setChecked(visible);
    }
    if (!d_table_drawer)
        return;

    if (visible && d_active) {
        d_table_drawer->refreshTableList(true);
        d_table_drawer->show();
        d_table_drawer->raise();
        repositionOverlayWidgets();
    } else {
        const bool hadDrawerFocus = d_table_drawer->isAncestorOf(QApplication::focusWidget())
                || QApplication::focusWidget() == d_table_drawer;
        d_table_drawer->hide();
        if (hadDrawerFocus && d_app && d_ribbon) {
            if (MyWidget *splitPlot = d_ribbon->splitPlotTarget())
                splitPlot->setFocus();
        }
    }
}

bool FocusCanvasHudOverlay::isTableDrawerVisible() const
{
    return d_table_drawer && !d_table_drawer->isHidden();
}

void FocusCanvasHudOverlay::refreshActiveContext()
{
    if (d_shutting_down)
        return;
    syncFocusHudOleButton(d_btn_ole, d_app.data(), d_ribbon.data());
    if (d_active) {
        if (isTableDrawerVisible() && d_table_drawer)
            d_table_drawer->refreshTableList(true);
        repositionOverlayWidgets();
    }
}

void FocusCanvasHudOverlay::repositionOverlayWidgets()
{
    if (d_shutting_down || !d_active)
        return;

    const QSize vpSz = (d_viewport && d_viewport->width() >= 160 && d_viewport->height() >= 100)
            ? d_viewport->size()
            : (d_app ? d_app->size() : QSize(1024, 720));

    int topY = 10;
    if (d_app && d_ribbon && d_viewport == d_app->centralWidget()) {
        if (d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutTabbed) {
            int tabBarH = 0;
            if (QTabBar *mdiTb = d_app->d_workspace.findChild<QTabBar *>()) {
                if (!mdiTb->isHidden() && mdiTb->height() > 0 && mdiTb->height() < 64)
                    tabBarH = mdiTb->height();
            }
            topY = tabBarH + 8;
            if (MultiLayer *ml =
                        qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
                // Offset below MultiLayer's top header strip ([1][+][Plot Columns] & dimension badge)
                topY = qMax(topY, tabBarH + 40);
                if (QFrame *pill = ml->findChild<QFrame *>(
                            QStringLiteral("MultiLayerPillContainer"))) {
                    if (!pill->isHidden() && pill->height() > 0 && d_viewport) {
                        const int pillBot =
                                pill->mapTo(d_viewport.data(), QPoint(0, pill->height())).y();
                        if (pillBot > 0 && pillBot < 140)
                            topY = qMax(topY, pillBot + 6);
                    }
                }
            }
        } else if (d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph
                   || d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutFreeMdi) {
            topY = 6;
        }
    }

    int wsW = 100;
    if (d_btn_worksheet) {
        d_btn_worksheet->adjustSize();
        const QSize wsSz = d_btn_worksheet->sizeHint();
        wsW = qMax(100, wsSz.width());
        d_btn_worksheet->setGeometry(12, topY, wsW, qMax(32, wsSz.height()));
        d_btn_worksheet->raise();
    }

    int ew = 110;
    if (d_btn_exit_focus) {
        d_btn_exit_focus->adjustSize();
        const QSize exSz = d_btn_exit_focus->sizeHint();
        ew = qMax(110, exSz.width());
        const int eh = qMax(32, exSz.height());
        d_btn_exit_focus->setGeometry(qMax(12, vpSz.width() - ew - 12), topY, ew, eh);
        d_btn_exit_focus->raise();
    }

    const int leftLimit = 12 + wsW + 10;
    const int rightLimit = qMax(leftLimit + 160, vpSz.width() - 12 - ew - 10);
    const int availW = qMax(160, rightLimit - leftLimit);

    QToolButton *pillBtns[] = { d_btn_plot, d_btn_analyze, d_btn_presets, d_btn_ole,
                                d_btn_palette };

    auto setPillButtonStyle = [&pillBtns, this](Qt::ToolButtonStyle st) {
        bool changed = false;
        for (QToolButton *pb : pillBtns) {
            if (pb && pb->toolButtonStyle() != st) {
                pb->setToolButtonStyle(st);
                changed = true;
            }
        }
        if (changed && layout())
            layout()->activate();
    };

    // A4-3: Keep readable text labels even on 911px/1080px screens thanks to the compact 5-button
    // pill; only degrade less-critical buttons (or all) to icon-only if space truly runs out.
    setPillButtonStyle(Qt::ToolButtonTextBesideIcon);
    adjustSize();
    if (sizeHint().width() > availW) {
        if (d_btn_palette)
            d_btn_palette->setToolButtonStyle(Qt::ToolButtonIconOnly);
        if (d_btn_ole)
            d_btn_ole->setToolButtonStyle(Qt::ToolButtonIconOnly);
        if (layout())
            layout()->activate();
        adjustSize();
        if (sizeHint().width() > availW)
            setPillButtonStyle(Qt::ToolButtonIconOnly);
    }

    adjustSize();
    const QSize pillSz = sizeHint();
    const int pw = qMin(pillSz.width(), availW);
    const int ph = qMax(36, pillSz.height());
    const int centeredPx = (vpSz.width() - pw) / 2;
    const int px = qBound(leftLimit, centeredPx, qMax(leftLimit, rightLimit - pw));
    setGeometry(px, topY, pw, ph);
    raise();

    if (d_table_drawer && !d_table_drawer->isHidden()) {
        const int drawerTop = topY + ph + 10;
        const int drawerH = qMax(200, vpSz.height() - drawerTop - 14);
        const int drawerW =
                qBound(240, qMin(396, qRound(vpSz.width() * 0.36)), qMax(240, vpSz.width() - 24));
        d_table_drawer->setFixedWidth(drawerW);
        d_table_drawer->setGeometry(12, drawerTop, drawerW, drawerH);
        d_table_drawer->raise();
    }
}

bool FocusCanvasHudOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (d_shutting_down)
        return QFrame::eventFilter(watched, event);
    if (d_active && watched == d_viewport && event) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show
            || event->type() == QEvent::ChildAdded) {
            repositionOverlayWidgets();
        }
    }
    return QFrame::eventFilter(watched, event);
}

void FocusCanvasHudOverlay::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}

// ============================================================================
// ModernRibbonBar
// ============================================================================

ModernRibbonBar::ModernRibbonBar(ApplicationWindow *app, QWidget *parent)
    : QFrame(parent), d_app(app)
{
    registerRibbonCollapseTranslations();
    setObjectName(QStringLiteral("ModernRibbonBarRoot"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(120);
    setStyleSheet(QStringLiteral(
            "QFrame#ModernRibbonBarRoot {"
            "  background-color: #F8FAFC;"
            "  border-bottom: 1px solid #CBD5E1;"
            "}"
            "QWidget#ModernRibbonTopStrip {"
            "  background-color: #F1F5F9;"
            "  border-bottom: 1px solid #E2E8F0;"
            "}"
            "QTabBar#ModernRibbonTabBar {"
            "  background: transparent;"
            "  border: none;"
            "}"
            "QTabBar#ModernRibbonTabBar::tab {"
            "  background: transparent;"
            "  color: #475569;"
            "  border: 1px solid transparent;"
            "  border-top-left-radius: 7px;"
            "  border-top-right-radius: 7px;"
            "  padding: 5px 11px;"
            "  margin-right: 1px;"
            "  margin-top: 3px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 600;"
            "}"
            "QTabBar#ModernRibbonTabBar::tab:hover {"
            "  background-color: #E2E8F0;"
            "  color: #0F172A;"
            "}"
            "QTabBar#ModernRibbonTabBar::tab:selected {"
            "  background-color: #FFFFFF;"
            "  color: #2563EB;"
            "  border: 1px solid #CBD5E1;"
            "  border-bottom: 1px solid #FFFFFF;"
            "}"
            "QStackedWidget#ModernRibbonTabStack {"
            "  background-color: #FFFFFF;"
            "}"
            "#ModernRibbonTabStack QScrollBar:horizontal {"
            "  height: 6px;"
            "  margin: 0px;"
            "  background: #F1F5F9;"
            "  border: none;"
            "}"
            "#ModernRibbonTabStack QScrollBar::handle:horizontal {"
            "  background: #CBD5E1;"
            "  border-radius: 3px;"
            "  min-width: 24px;"
            "  margin: 0px;"
            "}"
            "#ModernRibbonTabStack QScrollBar::handle:horizontal:hover {"
            "  background: #94A3B8;"
            "}"
            "#ModernRibbonTabStack QScrollBar::add-line:horizontal,"
            "#ModernRibbonTabStack QScrollBar::sub-line:horizontal,"
            "#ModernRibbonTabStack QScrollBar::add-page:horizontal,"
            "#ModernRibbonTabStack QScrollBar::sub-page:horizontal {"
            "  width: 0px;"
            "  height: 0px;"
            "  background: none;"
            "}"
            "QToolButton.RibbonActionBtn {"
            "  background-color: transparent;"
            "  color: #1E293B;"
            "  border: 1px solid transparent;"
            "  border-radius: 6px;"
            "  padding: 3px 6px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 500;"
            "  min-height: 30px;"
            "}"
            "QToolButton.RibbonActionBtn::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
            "}"
            "QToolButton.RibbonActionBtn:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #BFDBFE;"
            "}"
            "QToolButton.RibbonActionBtn:checked {"
            "  background-color: #DBEAFE;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "  font-weight: 600;"
            "}"
            "QToolButton.RibbonActionBtn:disabled {"
            "  color: #5B6B80;"
            "}"
            "QToolButton.RibbonPresetBtn {"
            "  background-color: #FFF7ED;"
            "  color: #9A3412;"
            "  border: 1px solid #FDBA74;"
            "  border-radius: 7px;"
            "  padding: 3px 8px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 700;"
            "  min-height: 30px;"
            "}"
            "QToolButton.RibbonPresetBtn::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
            "}"
            "QToolButton.RibbonPresetBtn:hover {"
            "  background-color: #FFEDD5;"
            "  color: #7C2D12;"
            "  border: 1px solid #EA580C;"
            "}"
            "QMenu.RibbonDropdownMenu {"
            "  background-color: #FFFFFF;"
            "  color: #0F172A;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 8px;"
            "  padding: 4px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "}"
            "QMenu.RibbonDropdownMenu::item {"
            "  padding: 6px 22px 6px 10px;"
            "  border-radius: 5px;"
            "}"
            "QMenu.RibbonDropdownMenu::item:selected {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "}"
            "QMenu.RibbonDropdownMenu::separator {"
            "  height: 1px;"
            "  background: #E2E8F0;"
            "  margin: 4px 8px;"
            "}"
            "QToolButton#RibbonCommandSearchPill {"
            "  background-color: #F8FAFC;"
            "  color: #64748B;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 13px;"
            "  padding: 2px 12px;"
            "  min-height: 24px;"
            "  text-align: left;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 500;"
            "}"
            "QToolButton#RibbonCommandSearchPill:hover {"
            "  background-color: #FFFFFF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "}"
            "QFrame#RibbonLayoutSegmentTrack {"
            "  background-color: #F1F5F9;"
            "  border: 1px solid #E2E8F0;"
            "  border-radius: 7px;"
            "}"
            "QToolButton.RibbonSegmentBtn {"
            "  background-color: transparent;"
            "  color: #475569;"
            "  border: 1px solid transparent;"
            "  border-radius: 5px;"
            "  padding: 2px 7px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 600;"
            "}"
            "QToolButton.RibbonSegmentBtn:hover {"
            "  background-color: rgba(255, 255, 255, 180);"
            "  color: #0F172A;"
            "}"
            "QToolButton.RibbonSegmentBtn:checked {"
            "  background-color: #FFFFFF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #CBD5E1;"
            "}"
            "QToolButton.RibbonHeaderPill {"
            "  background-color: #FFFFFF;"
            "  color: #334155;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 6px;"
            "  padding: 2px 8px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 600;"
            "}"
            "QToolButton.RibbonHeaderPill:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #93C5FD;"
            "}"
            "QToolButton.RibbonHeaderPill:checked {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #93C5FD;"
            "}"
            "QLabel#RibbonContextBadge {"
            "  background-color: #DBEAFE;"
            "  color: #1D4ED8;"
            "  border: 1px solid #93C5FD;"
            "  border-radius: 10px;"
            "  padding: 2px 10px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 700;"
            "}"));

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Top Header Row: 7 Contextual Tabs + Command Search Pill + Segmented Workspace Mode + Focus + Language + Context Badge
    QWidget *topStrip = new QWidget(this);
    topStrip->setObjectName(QStringLiteral("ModernRibbonTopStrip"));
    QHBoxLayout *topLayout = new QHBoxLayout(topStrip);
    topLayout->setContentsMargins(6, 2, 6, 0);
    topLayout->setSpacing(6);

    d_tab_bar = new QTabBar(topStrip);
    d_tab_bar->setObjectName(QStringLiteral("ModernRibbonTabBar"));
    d_tab_bar->setFont(QFont(QStringLiteral("Yu Gothic UI"), 11, QFont::DemiBold));
    d_tab_bar->setExpanding(false);
    d_tab_bar->setDrawBase(false);
    d_tab_bar->setUsesScrollButtons(true);
    d_tab_bar->setMinimumWidth(80);
    d_tab_bar->setFocusPolicy(Qt::TabFocus);
    topLayout->addWidget(d_tab_bar, 1);

    d_btn_cmd_palette = new QToolButton(topStrip);
    d_btn_cmd_palette->setObjectName(QStringLiteral("RibbonCommandSearchPill"));
    d_btn_cmd_palette->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_cmd_palette->setIcon(IconLoader::duotoneIcon(QStringLiteral("search-command"), 16, false));
    d_btn_cmd_palette->setIconSize(QSize(14, 14));
    d_btn_cmd_palette->setMinimumWidth(132);
    d_btn_cmd_palette->setMaximumWidth(168);
    d_btn_cmd_palette->setCursor(Qt::PointingHandCursor);
    d_btn_cmd_palette->setFocusPolicy(Qt::TabFocus);
    connect(d_btn_cmd_palette, &QToolButton::clicked, this, &ModernRibbonBar::openCommandPalette);
    topLayout->addWidget(d_btn_cmd_palette);

    d_layout_btn_group = new QButtonGroup(this);
    d_layout_btn_group->setExclusive(true);

    d_layout_segment_frame = new QFrame(topStrip);
    d_layout_segment_frame->setObjectName(QStringLiteral("RibbonLayoutSegmentTrack"));
    QHBoxLayout *segLayout = new QHBoxLayout(d_layout_segment_frame);
    segLayout->setContentsMargins(2, 2, 2, 2);
    segLayout->setSpacing(2);

    auto makeSegmentBtn = [this](const QString &iconId) -> QToolButton * {
        QToolButton *b = new QToolButton(d_layout_segment_frame);
        b->setProperty("class", QStringLiteral("RibbonSegmentBtn"));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setIcon(IconLoader::duotoneIcon(iconId, 16, false));
        b->setIconSize(QSize(14, 14));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::TabFocus);
        return b;
    };

    d_btn_layout_tabbed = makeSegmentBtn(QStringLiteral("tab-view"));
    d_btn_layout_split = makeSegmentBtn(QStringLiteral("split-view"));
    d_btn_layout_mdi = makeSegmentBtn(QStringLiteral("mdi-view"));

    d_layout_btn_group->addButton(d_btn_layout_tabbed, int(LayoutTabbed));
    d_layout_btn_group->addButton(d_btn_layout_split, int(LayoutSplitTableGraph));
    d_layout_btn_group->addButton(d_btn_layout_mdi, int(LayoutFreeMdi));
    d_btn_layout_tabbed->setChecked(true);

    segLayout->addWidget(d_btn_layout_tabbed);
    segLayout->addWidget(d_btn_layout_split);
    segLayout->addWidget(d_btn_layout_mdi);
    topLayout->addWidget(d_layout_segment_frame);

    auto makeHeaderPill = [topStrip](const QString &iconId, bool checkable) -> QToolButton * {
        QToolButton *b = new QToolButton(topStrip);
        b->setProperty("class", QStringLiteral("RibbonHeaderPill"));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setIcon(IconLoader::duotoneIcon(iconId, 16, false));
        b->setIconSize(QSize(14, 14));
        b->setCheckable(checkable);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::TabFocus);
        return b;
    };

    connect(d_layout_btn_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this,
            [this](int modeId) { setWorkspaceLayoutMode(WorkspaceLayoutMode(modeId)); });

    d_btn_focus_mode = makeHeaderPill(QStringLiteral("focus-hud"), false);
    connect(d_btn_focus_mode, &QToolButton::clicked, this,
            [this]() { setFocusHudMode(true); });
    topLayout->addWidget(d_btn_focus_mode);

    d_btn_lang_switch = makeHeaderPill(QStringLiteral("language-ja-en"), false);
    d_btn_lang_switch->setObjectName(QStringLiteral("RibbonLanguageSwitchBtn"));
    connect(d_btn_lang_switch, &QToolButton::clicked, this,
            &ModernRibbonBar::invokeAppToggleLanguage);
    topLayout->addWidget(d_btn_lang_switch);

    d_btn_help_guide = makeHeaderPill(QStringLiteral("help"), false);
    d_btn_help_guide->setObjectName(QStringLiteral("RibbonQuickGuideBtn"));
    connect(d_btn_help_guide, &QToolButton::clicked, this, [this]() {
        if (!d_app)
            return;
        if (QAction *a = d_app->findChild<QAction *>(QStringLiteral("actionShowHelp")))
            a->trigger();
        else
            QMetaObject::invokeMethod(d_app.data(), "showHelp", Qt::DirectConnection);
    });
    topLayout->addWidget(d_btn_help_guide);

    d_context_pill = new QLabel(topStrip);
    d_context_pill->setObjectName(QStringLiteral("RibbonContextBadge"));
    d_context_pill->setAlignment(Qt::AlignCenter);
    d_context_pill->setMinimumWidth(0);
    d_context_pill->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    topLayout->addWidget(d_context_pill);

    rootLayout->addWidget(topStrip);

    // Single-Row Slim Contextual Action Strip
    d_tab_stack = new QStackedWidget(this);
    d_tab_stack->setObjectName(QStringLiteral("ModernRibbonTabStack"));
    rootLayout->addWidget(d_tab_stack);

    connect(d_tab_bar, &QTabBar::currentChanged, this, [this](int idx) {
        if (d_tab_stack)
            d_tab_stack->setCurrentIndex(idx);
        updateTabStackHeight();
    });

    buildAllTabs();
    initStatusBarZoomControls();
    updateTabStackHeight();

    if (d_app) {
        connect(&d_app->folders, &QObject::destroyed, this,
                [this]() { prepareForAppShutdown(); });

        if (QWidget *vp = d_app->d_workspace.viewport())
            vp->installEventFilter(this);
        QWidget *hudHost = d_app->centralWidget() ? d_app->centralWidget()
                                                  : d_app->d_workspace.viewport();
        if (hudHost)
            d_focus_hud_overlay = new FocusCanvasHudOverlay(d_app, this, hudHost);
        d_layout_mode = d_app->isTabbedWorkspaceView() ? LayoutTabbed : LayoutFreeMdi;
        if (QAbstractButton *b = d_layout_btn_group->button(int(d_layout_mode)))
            b->setChecked(true);

        connect(&d_app->d_workspace, &QMdiArea::subWindowActivated, this,
                [this](QMdiSubWindow *sw) {
                    if (d_shutting_down)
                        return;
                    MyWidget *mw = qobject_cast<MyWidget *>(sw);
                    updateActiveContext(mw);
                    if (d_layout_mode == LayoutTabbed && mw && !mw->isHidden()
                        && mw->status() != MyWidget::Hidden
                        && !d_app->hiddenWindows.contains(mw)
                        && !d_app->outWindows.contains(mw) && !mw->isMaximized()) {
                        mw->showMaximized();
                    }
                    if (d_layout_mode == LayoutSplitTableGraph && !d_applying_split_geometry)
                        applySplitWorkspaceGeometry();
                    if (d_focus_hud_mode && d_layout_mode == LayoutFreeMdi && mw && !mw->isHidden()
                        && (qobject_cast<MultiLayer *>(mw) || qobject_cast<Graph3D *>(mw))) {
                        hideBackgroundMdiWindowsForFocus(d_app);
                        if (!mw->isMaximized() && mw->y() < 48)
                            mw->move(mw->x(), 48);
                    }
                });
    }

    this->installEventFilter(this);

    // C14: double-click a tab (or Ctrl+F1) to show only the tab row, as in Office
    d_tab_bar->installEventFilter(this);
    QShortcut *minimizeShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F1), this);
    minimizeShortcut->setContext(Qt::WindowShortcut);
    connect(minimizeShortcut, &QShortcut::activated, this,
            [this]() { setRibbonMinimized(!d_minimized); });
    if (d_app && !d_app->isHeadlessMode()
        && ApplicationWindow::getSettings()
                   .value(QStringLiteral("/UX/RibbonMinimized"), false)
                   .toBool()) {
        setRibbonMinimized(true);
    }

    retranslateUi();
    updateActiveContext(d_app ? qobject_cast<MyWidget *>(d_app->d_workspace.activeSubWindow())
                              : nullptr);
}

ModernRibbonBar::~ModernRibbonBar()
{
    prepareForAppShutdown();
}

QSize ModernRibbonBar::minimumSizeHint() const
{
    return QSize(120, sizeHint().height());
}

void ModernRibbonBar::updateTabStackHeight()
{
    if (d_shutting_down || !d_tab_stack)
        return;

    const int fmH = fontMetrics().height();
    const int btnH = qMax(34, fmH + 14);

    int maxContentH = 0;
    for (int i = 0; i < d_tab_stack->count(); ++i) {
        if (QScrollArea *sa = qobject_cast<QScrollArea *>(d_tab_stack->widget(i))) {
            if (QWidget *cw = sa->widget()) {
                cw->ensurePolished();
                if (cw->layout())
                    cw->layout()->activate();
                maxContentH = qMax(maxContentH,
                                   qMax(cw->sizeHint().height(), cw->minimumSizeHint().height()));
            }
        }
    }

    // Sleek single-row action strip (~46-54px tall, zero vertical clipping)
    const int baseH = qMax(46, qMax(btnH + 10, maxContentH + 4));

    bool hScrollActive = false;
    if (QScrollArea *sa = qobject_cast<QScrollArea *>(d_tab_stack->currentWidget())) {
        if (QScrollBar *hb = sa->horizontalScrollBar())
            hScrollActive = hb->isVisible() || (hb->maximum() > hb->minimum());
    }
    const int targetH = hScrollActive ? (baseH + 8) : baseH;
    if (d_tab_stack->height() != targetH || d_tab_stack->minimumHeight() != targetH
        || d_tab_stack->maximumHeight() != targetH) {
        d_tab_stack->setFixedHeight(targetH);
        if (layout())
            layout()->activate();
        updateGeometry();
    }
    if (QScrollArea *curSa = qobject_cast<QScrollArea *>(d_tab_stack->currentWidget())) {
        if (QWidget *cw = curSa->widget()) {
            if (cw->layout())
                cw->layout()->activate();
        }
    }
}

void ModernRibbonBar::updateHeaderResponsiveness()
{
    if (d_shutting_down)
        return;
    const int w = width();
    if (w <= 0)
        return;

    int tabsNeededW = 0;
    if (d_tab_bar) {
        d_tab_bar->ensurePolished();
        for (int i = 0; i < d_tab_bar->count(); ++i)
            tabsNeededW += d_tab_bar->tabRect(i).width() + 1;
        if (d_tab_bar->count() > 0) {
            const int spanW = d_tab_bar->tabRect(d_tab_bar->count() - 1).right()
                    - d_tab_bar->tabRect(0).left() + 2;
            tabsNeededW = qMax(tabsNeededW, spanW);
        }
        tabsNeededW = qMax(tabsNeededW, d_tab_bar->sizeHint().width());
    }
    if (tabsNeededW <= 0)
        tabsNeededW = 560;

    struct HeaderTier
    {
        bool showLang;
        bool shortCmdFocus;
        bool compactLayout;
        bool langIconOnly;
        bool focusIconOnly;
        bool modeIconOnly;
        bool cmdIconOnly;
        bool showCtx;
    };

    // Progressive degradation so 1366x768 (EN & JA) keeps Search, Tabs|Split|Free, Focus, and
    // context badge legible with labels instead of collapsing all 6 header pills to icons.
    const HeaderTier kTiers[] = {
        { true, false, false, false, false, false, false, true }, // Ultra-wide: full labels
        { true, true, false, false, false, false, false, true },  // Wide (~1500px): compact cmd/focus + full mode labels
        { true, true, true, true, false, false, false, true },    // Laptop (~1366px): Tabs|Split|Free, Focus, icon Lang/Guide, Search + ctx
        { true, true, true, true, true, false, false, true },     // ~1280px with ctx: icon Focus + labeled mode pills
        { true, true, true, true, true, false, false, false },    // ~1150px: hide ctx before collapsing mode labels
        { true, true, true, true, true, true, false, false },     // Compact: search pill + icon modes
        { true, true, true, true, true, true, true, false }       // Minimal icon-only fallback
    };

    auto applyButtonsForTier = [this](const HeaderTier &t) {
        if (d_btn_cmd_palette) {
            const QString wantCmd = tr("Search commands...");
            if (d_btn_cmd_palette->text() != wantCmd)
                d_btn_cmd_palette->setText(wantCmd);
            const Qt::ToolButtonStyle searchStyle = t.cmdIconOnly
                    ? Qt::ToolButtonIconOnly
                    : Qt::ToolButtonTextBesideIcon;
            if (d_btn_cmd_palette->toolButtonStyle() != searchStyle)
                d_btn_cmd_palette->setToolButtonStyle(searchStyle);
            const int minW = t.cmdIconOnly ? 0 : 124;
            const int maxW = t.cmdIconOnly ? 36 : (t.shortCmdFocus ? 168 : 216);
            if (d_btn_cmd_palette->minimumWidth() != minW)
                d_btn_cmd_palette->setMinimumWidth(minW);
            if (d_btn_cmd_palette->maximumWidth() != maxW)
                d_btn_cmd_palette->setMaximumWidth(maxW);
        }
        if (d_btn_focus_mode) {
            const QString wantFocus = t.shortCmdFocus ? tr("Focus") : tr("Focus (F11)");
            if (d_btn_focus_mode->text() != wantFocus)
                d_btn_focus_mode->setText(wantFocus);
        }
        if (d_btn_layout_tabbed) {
            const QString wantTab = t.compactLayout ? tr("Tabs") : tr("Tabbed");
            if (d_btn_layout_tabbed->text() != wantTab)
                d_btn_layout_tabbed->setText(wantTab);
        }
        if (d_btn_layout_split) {
            const QString wantSplit = t.compactLayout ? tr("Split") : tr("Split View");
            if (d_btn_layout_split->text() != wantSplit)
                d_btn_layout_split->setText(wantSplit);
        }
        if (d_btn_layout_mdi) {
            const QString wantMdi = t.compactLayout ? tr("Free") : tr("Free MDI");
            if (d_btn_layout_mdi->text() != wantMdi)
                d_btn_layout_mdi->setText(wantMdi);
        }
        if (d_btn_help_guide) {
            const QString wantGuide = tr("Guide");
            if (d_btn_help_guide->text() != wantGuide)
                d_btn_help_guide->setText(wantGuide);
            const Qt::ToolButtonStyle helpStyle = (t.shortCmdFocus || t.langIconOnly)
                    ? Qt::ToolButtonIconOnly
                    : Qt::ToolButtonTextBesideIcon;
            if (d_btn_help_guide->toolButtonStyle() != helpStyle)
                d_btn_help_guide->setToolButtonStyle(helpStyle);
        }

        const Qt::ToolButtonStyle segStyle =
                t.modeIconOnly ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon;
        QToolButton *segPills[] = { d_btn_layout_tabbed, d_btn_layout_split, d_btn_layout_mdi };
        for (QToolButton *hb : segPills) {
            if (hb && hb->toolButtonStyle() != segStyle)
                hb->setToolButtonStyle(segStyle);
        }
        if (d_btn_focus_mode) {
            const Qt::ToolButtonStyle focusStyle = (t.modeIconOnly || t.focusIconOnly)
                    ? Qt::ToolButtonIconOnly
                    : Qt::ToolButtonTextBesideIcon;
            if (d_btn_focus_mode->toolButtonStyle() != focusStyle)
                d_btn_focus_mode->setToolButtonStyle(focusStyle);
        }
        if (d_btn_lang_switch) {
            const Qt::ToolButtonStyle langStyle = (t.modeIconOnly || t.langIconOnly)
                    ? Qt::ToolButtonIconOnly
                    : Qt::ToolButtonTextBesideIcon;
            if (d_btn_lang_switch->toolButtonStyle() != langStyle)
                d_btn_lang_switch->setToolButtonStyle(langStyle);
        }
    };

    auto measureTierRightW = [this](const HeaderTier &t) -> int {
        int rw = 0;
        auto addW = [&rw](QWidget *wgt, bool vis) {
            if (!wgt || !vis)
                return;
            wgt->ensurePolished();
            rw += qMax(wgt->minimumWidth(), wgt->sizeHint().width()) + 6;
        };
        addW(d_btn_cmd_palette, true);
        if (d_layout_segment_frame) {
            d_layout_segment_frame->ensurePolished();
            if (d_layout_segment_frame->layout())
                d_layout_segment_frame->layout()->invalidate();
            rw += d_layout_segment_frame->sizeHint().width() + 6;
        }
        addW(d_btn_focus_mode, true);
        addW(d_btn_lang_switch, t.showLang);
        addW(d_btn_help_guide, true);
        addW(d_context_pill, t.showCtx);
        return rw;
    };

    const int nTiers = static_cast<int>(sizeof(kTiers) / sizeof(kTiers[0]));
    HeaderTier chosen = kTiers[nTiers - 1];
    int chosenRightW = 0;
    bool found = false;

    for (int idx = 0; idx < nTiers; ++idx) {
        const HeaderTier &cand = kTiers[idx];
        applyButtonsForTier(cand);
        const int rw = measureTierRightW(cand);
        if (tabsNeededW + rw + 24 <= w || idx == nTiers - 1) {
            chosen = cand;
            chosenRightW = rw;
            found = true;
            break;
        }
    }
    if (!found) {
        applyButtonsForTier(chosen);
        chosenRightW = measureTierRightW(chosen);
    }

    if (d_btn_lang_switch && d_btn_lang_switch->isVisible() != chosen.showLang)
        d_btn_lang_switch->setVisible(chosen.showLang);
    if (d_context_pill && d_context_pill->isVisible() != chosen.showCtx)
        d_context_pill->setVisible(chosen.showCtx);

    if (d_tab_bar) {
        const int maxAvailTabW = qMax(80, w - chosenRightW - 18);
        const int wantMinTabW = qMin(tabsNeededW + 10, maxAvailTabW);
        if (d_tab_bar->minimumWidth() != wantMinTabW)
            d_tab_bar->setMinimumWidth(wantMinTabW);
        if (QWidget *ts = d_tab_bar->parentWidget()) {
            if (ts->layout())
                ts->layout()->activate();
        }
    }
}

void ModernRibbonBar::prepareForAppShutdown()
{
    const bool wasShuttingDown = d_shutting_down;
    d_shutting_down = true;
    d_saved_focus_visibility.clear();
    d_last_active_window.clear();
    if (d_split_table)
        d_split_table->removeEventFilter(this);
    if (d_split_plot)
        d_split_plot->removeEventFilter(this);
    d_split_table.clear();
    d_split_plot.clear();

    if (!wasShuttingDown && d_app) {
        disconnect(&d_app->d_workspace, nullptr, this, nullptr);
        if (QWidget *vp = d_app->d_workspace.viewport())
            vp->removeEventFilter(this);
        if (QStatusBar *sb = d_app->statusBar())
            sb->removeEventFilter(this);
    }
    if (d_watched_empty_graph_pane) {
        d_watched_empty_graph_pane->removeEventFilter(this);
        d_watched_empty_graph_pane.clear();
    }
    if (d_welcome_hub_card) {
        d_welcome_hub_card->removeEventFilter(this);
        delete d_welcome_hub_card.data();
    }

    if (d_zoom_bound_ml) {
        disconnect(d_zoom_bound_ml.data(), nullptr, this, nullptr);
        d_zoom_bound_ml.clear();
    }
    if (d_status_zoom_btn) {
        if (d_app && d_app->statusBar())
            d_app->statusBar()->removeWidget(d_status_zoom_btn.data());
        delete d_status_zoom_btn.data();
    }
    if (d_status_fit_btn) {
        if (d_app && d_app->statusBar())
            d_app->statusBar()->removeWidget(d_status_fit_btn.data());
        delete d_status_fit_btn.data();
    }

    if (d_focus_hud_overlay) {
        d_focus_hud_overlay->prepareForAppShutdown();
        delete d_focus_hud_overlay.data();
    }
    if (d_command_palette)
        delete d_command_palette.data();

    d_app.clear();
}

QWidget *ModernRibbonBar::createRibbonScrollPage(QHBoxLayout **outRowLayout)
{
    RibbonHorizontalScrollArea *scroll = new RibbonHorizontalScrollArea(d_tab_stack);
    QWidget *container = new QWidget(scroll);
    container->setStyleSheet(QStringLiteral("background: #FFFFFF;"));
    QHBoxLayout *row = new QHBoxLayout(container);
    row->setContentsMargins(4, 4, 4, 4);
    row->setSpacing(2);
    scroll->setWidget(container);
    if (QScrollBar *hbar = scroll->horizontalScrollBar()) {
        connect(hbar, &QScrollBar::rangeChanged, this,
                [this](int, int) { updateTabStackHeight(); });
    }
    if (outRowLayout)
        *outRowLayout = row;
    return scroll;
}

QWidget *ModernRibbonBar::createRibbonGroup(QHBoxLayout *pageRow, QHBoxLayout **outBtnLayout,
                                            bool addDividerAfter,
                                            std::function<QString()> titleFn,
                                            int collapsePriority)
{
    QWidget *grpWidget = new QWidget();
    QHBoxLayout *btnRow = new QHBoxLayout(grpWidget);
    btnRow->setContentsMargins(2, 0, 2, 0);
    btnRow->setSpacing(2);

    pageRow->addWidget(grpWidget, 0, Qt::AlignVCenter);

    // C14: stand-in button "<group> ▾" that lists the group's commands when the ribbon is too
    // narrow to show them all. Hidden while the group itself fits.
    if (titleFn) {
        QToolButton *folded = new QToolButton();
        folded->setObjectName(QStringLiteral("RibbonCollapsedGroupBtn"));
        folded->setProperty("class", QStringLiteral("RibbonActionBtn"));
        folded->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        folded->setIconSize(QSize(16, 16));
        folded->setPopupMode(QToolButton::InstantPopup);
        folded->setCursor(Qt::PointingHandCursor);
        folded->setFocusPolicy(Qt::TabFocus);
        QMenu *menu = new QMenu(folded);
        menu->setProperty("class", QStringLiteral("RibbonDropdownMenu"));
        menu->setToolTipsVisible(true);
        folded->setMenu(menu);
        const int groupIndex = d_groups.size();
        connect(menu, &QMenu::aboutToShow, this,
                [this, groupIndex]() { populateCollapsedGroupMenu(groupIndex); });
        pageRow->addWidget(folded, 0, Qt::AlignVCenter);
        folded->setVisible(false);

        RibbonGroupInfo info;
        info.page = pageRow->parentWidget();
        info.group = grpWidget;
        info.collapsedButton = folded;
        info.titleFn = std::move(titleFn);
        info.priority = collapsePriority;
        d_groups.append(info);
    }

    if (addDividerAfter) {
        QFrame *div = new QFrame();
        div->setFixedSize(1, 28);
        div->setStyleSheet(QStringLiteral("background-color: #CBD5E1; margin: 0px 3px;"));
        pageRow->addWidget(div, 0, Qt::AlignVCenter);
    }

    if (outBtnLayout)
        *outBtnLayout = btnRow;
    return grpWidget;
}

QToolButton *ModernRibbonBar::addRibbonButton(QHBoxLayout *groupLayout, const QString &iconId,
                                              std::function<QString()> textFn,
                                              std::function<QString()> tooltipFn,
                                              std::function<void()> onClicked, bool checkable,
                                              bool accentPreset)
{
    QToolButton *btn = new QToolButton();
    btn->setProperty("class",
                     accentPreset ? QStringLiteral("RibbonPresetBtn")
                                  : QStringLiteral("RibbonActionBtn"));
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setIcon(IconLoader::duotoneIcon(iconId, 20, false));
    btn->setIconSize(QSize(16, 16));
    btn->setCheckable(checkable);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::TabFocus);

    if (onClicked)
        connect(btn, &QToolButton::clicked, this, std::move(onClicked));

    groupLayout->addWidget(btn);
    d_button_bindings.append(
            { btn, iconId, std::move(textFn), std::move(tooltipFn), accentPreset });
    return btn;
}

QToolButton *ModernRibbonBar::addRibbonMenuButton(QHBoxLayout *groupLayout, const QString &iconId,
                                                  std::function<QString()> textFn,
                                                  std::function<QString()> tooltipFn,
                                                  QMenu **outMenu, bool accentPreset)
{
    QToolButton *btn = new QToolButton();
    btn->setProperty("class",
                     accentPreset ? QStringLiteral("RibbonPresetBtn")
                                  : QStringLiteral("RibbonActionBtn"));
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setIcon(IconLoader::duotoneIcon(iconId, 20, false));
    btn->setIconSize(QSize(16, 16));
    btn->setPopupMode(QToolButton::InstantPopup);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::TabFocus);

    QMenu *menu = new QMenu(btn);
    menu->setProperty("class", QStringLiteral("RibbonDropdownMenu"));
    menu->setToolTipsVisible(true);
    btn->setMenu(menu);

    groupLayout->addWidget(btn);
    d_button_bindings.append(
            { btn, iconId, std::move(textFn), std::move(tooltipFn), accentPreset });
    if (outMenu)
        *outMenu = menu;
    return btn;
}

QAction *ModernRibbonBar::addRibbonMenuAction(QMenu *menu, const QString &iconId,
                                              std::function<QString()> textFn,
                                              std::function<QString()> tooltipFn,
                                              std::function<void()> onTriggered, bool checkable)
{
    if (!menu)
        return nullptr;
    QAction *act = new QAction(menu);
    act->setCheckable(checkable);
    if (!iconId.isEmpty())
        act->setIcon(IconLoader::duotoneIcon(iconId, 18, false));
    if (onTriggered)
        connect(act, &QAction::triggered, this, std::move(onTriggered));
    menu->addAction(act);
    d_menu_action_bindings.append({ act, iconId, std::move(textFn), std::move(tooltipFn) });
    return act;
}

void ModernRibbonBar::buildAllTabs()
{
    auto addTabPage = [this](std::function<QString()> titleFn, QWidget *page) {
        const int idx = d_tab_bar->addTab(QString());
        Q_UNUSED(idx);
        d_tab_stack->addWidget(page);
        d_tab_title_fns.append(std::move(titleFn));
    };

    const QString kCaret = QString::fromUtf8(" \xE2\x96\xBE");

    // ------------------------------------------------------------------------
    // Tab 1: Home & Data
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gFile = nullptr;
        QHBoxLayout *gCreate = nullptr;
        QHBoxLayout *gRecalc = nullptr;
        QHBoxLayout *gPanels = nullptr;

        createRibbonGroup(row, &gFile, true, [this]() { return tr("File"); }, 90);
        setShortcutHintAction(addRibbonButton(
                gFile, QStringLiteral("file-new"), [this]() { return tr("New Project"); },
                [this]() { return tr("Create a New Project"); },
                [this]() {
                    if (d_app)
                        d_app->newProject();
                }), QStringLiteral("actionNewProject"));
        setShortcutHintAction(addRibbonButton(
                gFile, QStringLiteral("file-open"), [this]() { return tr("Open"); },
                [this]() { return tr("Open Project File"); },
                [this]() {
                    if (d_app)
                        d_app->open();
                }), QStringLiteral("actionOpen"));
        setShortcutHintAction(addRibbonButton(
                gFile, QStringLiteral("file-save"), [this]() { return tr("Save"); },
                [this]() { return tr("Save Current Project"); },
                [this]() {
                    if (d_app)
                        d_app->saveProject();
                }), QStringLiteral("actionSaveProject"));
        QMenu *importMenu = nullptr;
        addRibbonMenuButton(
                gFile, QStringLiteral("import-csv"),
                [this, kCaret]() { return tr("Import") + kCaret; },
                [this]() { return tr("Import Data or Origin Project"); }, &importMenu);
        addRibbonMenuAction(
                importMenu, QStringLiteral("import-csv"),
                [this]() { return tr("Import CSV / ASCII..."); },
                [this]() { return tr("Import ASCII / CSV Data into Worksheet"); },
                [this]() {
                    if (d_app)
                        d_app->importASCII();
                });
        addRibbonMenuAction(
                importMenu, QStringLiteral("import-xlsx"),
                [this]() {
                    XlsxImport::registerTranslations();
                    return tr("Import Excel Workbook (.xlsx)...");
                },
                [this]() { return tr("Import every sheet of an Excel workbook (.xlsx) as worksheets"); },
                [this]() {
                    if (d_app)
                        XlsxImport::importWithDialog(d_app);
                });
        addRibbonMenuAction(
                importMenu, QStringLiteral("import-opj"),
                [this]() { return tr("Import Origin OPJ..."); },
                [this]() { return tr("Import Origin OPJ Project"); },
                [this]() { importOpjInteractive(); });
        addRibbonButton(
                gFile, QStringLiteral("sample-gallery"), [this]() { return tr("Samples"); },
                [this]() { return tr("Open Built-In Scientific Sample Project Gallery"); },
                [this]() {
                    if (d_app)
                        CurveMathTools::showSampleGalleryDialog(d_app);
                },
                false, false);

        // C4: Undo / Redo on Home (icon only: Home must fit 1280 px without scrolling)
        QHBoxLayout *gEdit = nullptr;
        createRibbonGroup(row, &gEdit, true, [this]() { return tr("Undo / Redo"); }, 70);
        for (QToolButton *tb :
             { addActionButton(
                       gEdit, QStringLiteral("undo"), [this]() { return tr("Undo"); },
                       [this]() { return tr("Undo the last change in the active window (Ctrl+Z)"); },
                       QStringLiteral("actionUndo")),
               addActionButton(
                       gEdit, QStringLiteral("redo"), [this]() { return tr("Redo"); },
                       [this]() { return tr("Redo the last undone change (Ctrl+Y)"); },
                       QStringLiteral("actionRedo")) }) {
            tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
        }

        // C4/B5: "Copy for PowerPoint" & "Paste (Ctrl+V)" on Home.
        QHBoxLayout *gClipboard = nullptr;
        createRibbonGroup(row, &gClipboard, true, [this]() { return tr("Clipboard"); }, 85);
        QToolButton *pasteBtn = addRibbonButton(
                gClipboard, QStringLiteral("paste"),
                [this]() { return tr("Paste (Ctrl+V)"); },
                [this]() {
                    return tr("Paste clipboard data or table cells into worksheet (Ctrl+V)");
                },
                [this]() { pasteFromClipboardSmart(); });
        pasteBtn->setObjectName(QStringLiteral("RibbonHomePasteBtn"));
        addActionButton(
                gClipboard, QStringLiteral("copy-ole"),
                []() { return ToriGraphOleUi::copyForPowerPointLabel(); },
                []() { return ToriGraphOleUi::copyForPowerPointToolTip(); },
                QStringLiteral("actionCopyGraphOle"))
                ->setObjectName(QStringLiteral("RibbonHomeCopyForPowerPointBtn"));

        createRibbonGroup(row, &gCreate, true, [this]() { return tr("Create"); }, 80);
        setShortcutHintAction(addRibbonButton(
                gCreate, QStringLiteral("new-table"), [this]() { return tr("New Table"); },
                [this]() { return tr("Create New Worksheet Table"); },
                [this]() {
                    if (d_app)
                        d_app->newTable();
                }), QStringLiteral("actionNewTable"));
        setShortcutHintAction(addRibbonButton(
                gCreate, QStringLiteral("new-graph"), [this]() { return tr("New Graph"); },
                [this]() { return tr("Create New 2D Graph Window"); },
                [this]() {
                    if (d_app)
                        d_app->newGraph();
                }), QStringLiteral("actionNewGraph"));
        QMenu *moreNewMenu = nullptr;
        addRibbonMenuButton(
                gCreate, QStringLiteral("new-matrix"),
                [this, kCaret]() { return tr("More New") + kCaret; },
                [this]() {
                    return tr("Create Multi-Sheet Workbook, Matrix, Add Column, or Apply Template");
                },
                &moreNewMenu);
        addRibbonMenuAction(
                moreNewMenu, QStringLiteral("new-table"),
                [this]() { return tr("New Workbook"); },
                [this]() { return tr("Create a New Multi-Sheet Workbook"); },
                [this]() {
                    if (d_app) {
                        if (QAction *wbAct = WorkbookManager::instance(d_app)->newWorkbookAction())
                            wbAct->trigger();
                    }
                });
        addRibbonMenuAction(
                moreNewMenu, QStringLiteral("new-matrix"), [this]() { return tr("New Matrix"); },
                [this]() { return tr("Create New 2D Data Matrix"); },
                [this]() {
                    if (d_app)
                        d_app->newMatrix();
                });
        d_act_add_column = addRibbonMenuAction(
                moreNewMenu, QStringLiteral("add-column"), [this]() { return tr("Add Column"); },
                [this]() { return tr("Append Y Column to Active Worksheet"); },
                [this]() {
                    if (!d_app)
                        return;
                    Table *tbl = resolvedWorksheetTarget();
                    if (!tbl) {
                        d_app->showUxHint(tr("Click a worksheet first to add a column."));
                        return;
                    }
                    tbl->addCol();
                });
        moreNewMenu->addSeparator();
        addRibbonMenuAction(
                moreNewMenu, QStringLiteral("page-setup"),
                [this]() { return tr("Apply Template (.qpt) to Table..."); },
                [this]() { return tr("Apply a saved graph template (.qpt) to the active worksheet"); },
                [this]() {
                    if (!d_app)
                        return;
                    if (!QMetaObject::invokeMethod(d_app.data(), "applyTemplateToActiveTable",
                                                   Qt::DirectConnection)) {
                        d_app->openTemplate();
                    }
                });

        createRibbonGroup(row, &gRecalc, true, [this]() { return tr("Recalculation"); }, 20);
        d_btn_auto_recalc = addRibbonButton(
                gRecalc, QStringLiteral("recalc-auto"), [this]() { return tr("Auto"); },
                [this]() { return tr("Toggle Automatic Analysis Recalculation"); },
                [this]() {
                    if (!d_app)
                        return;
                    if (d_btn_auto_recalc && d_btn_auto_recalc->isChecked())
                        d_app->setRecalcModeAuto();
                    else
                        d_app->setRecalcModeManual();
                    syncCheckableStates();
                },
                true);
        addRibbonButton(
                gRecalc, QStringLiteral("recalc-now"), [this]() { return tr("Recalc"); },
                [this]() { return tr("Recalculate Pending Analysis Links Now"); },
                [this]() {
                    if (d_app)
                        d_app->recalculatePendingLinksNow();
                });

        createRibbonGroup(row, &gPanels, false, [this]() { return tr("Panels"); }, 30);
        QMenu *panelsMenu = nullptr;
        addRibbonMenuButton(
                gPanels, QStringLiteral("table-drawer"),
                [this, kCaret]() { return tr("Panels") + kCaret; },
                [this]() { return tr("Show or Hide Explorer, Inspector & Results Log Panels"); },
                &panelsMenu);
        d_act_explorer = addRibbonMenuAction(
                panelsMenu, QStringLiteral("explorer"), [this]() { return tr("Explorer"); },
                [this]() { return tr("Show or Hide Project Explorer Panel"); },
                [this]() {
                    if (!d_app)
                        return;
                    d_app->explorerWindow.setVisible(d_app->explorerWindow.isHidden());
                    syncCheckableStates();
                },
                true);
        d_act_inspector = addRibbonMenuAction(
                panelsMenu, QStringLiteral("table-drawer"),
                [this]() { return tr("Inspector"); },
                [this]() { return tr("Toggle Graph Inspector Dock (Ctrl+Shift+I)"); },
                [this]() {
                    if (!d_app)
                        return;
                    d_app->toggleGraphInspectorDock();
                    syncCheckableStates();
                },
                true);
        d_act_results_log = addRibbonMenuAction(
                panelsMenu, QStringLiteral("log-window"),
                [this]() { return tr("Results Log"); },
                [this]() { return tr("Show or Hide Results Log Panel"); },
                [this]() {
                    if (!d_app)
                        return;
                    d_app->logWindow.setVisible(d_app->logWindow.isHidden());
                    syncCheckableStates();
                },
                true);

        row->addStretch(1);
        addTabPage([this]() { return tr("Home"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 2: Plot 2D/3D (Decluttered: 1-click primary 2D + Specialty + 3D + Layers)
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *g2d = nullptr;
        QHBoxLayout *gMore = nullptr;
        QHBoxLayout *gLayer = nullptr;

        createRibbonGroup(row, &g2d, true, [this]() { return tr("2D Plots"); }, 90);
        addRibbonButton(
                g2d, QStringLiteral("plot-line"), [this]() { return tr("Line"); },
                [this]() { return tr("Plot Line Graph"); },
                [this]() { triggerPlot2D(Graph::Line); });
        addRibbonButton(
                g2d, QStringLiteral("plot-line-symbol"), [this]() { return tr("Line + Symbol"); },
                [this]() { return tr("Plot Line and Symbol Graph"); },
                [this]() { triggerPlot2D(Graph::LineSymbols); });
        addRibbonButton(
                g2d, QStringLiteral("plot-scatter"), [this]() { return tr("Scatter"); },
                [this]() { return tr("Plot Scatter Graph"); },
                [this]() { triggerPlot2D(Graph::Scatter); });
        addRibbonButton(
                g2d, QStringLiteral("plot-bars"), [this]() { return tr("Bar Chart"); },
                [this]() { return tr("Plot Vertical Bar Chart"); },
                [this]() { triggerPlot2D(Graph::VerticalBars); });
        addRibbonButton(
                g2d, QStringLiteral("plot-double-y"), [this]() { return tr("Double-Y"); },
                [this]() { return tr("Plot Dual Left/Right Y-Axis Graph"); },
                [this]() {
                    if (!d_app)
                        return;
                    if (!d_app->d_workspace.activeSubWindow()) {
                        d_app->showUxHint(
                                tr("Click a worksheet (with 2+ Y columns) or a graph window first."));
                        return;
                    }
                    d_app->plotDoubleY();
                });

        createRibbonGroup(row, &gMore, true, [this]() { return tr("More Plots"); }, 55);
        QMenu *specialtyMenu = nullptr;
        addRibbonMenuButton(
                gMore, QStringLiteral("plot-box"),
                [this, kCaret]() { return tr("Specialty") + kCaret; },
                [this]() { return tr("Specialized 2D, Statistical & Matrix Plots"); },
                &specialtyMenu);
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-spline"), [this]() { return tr("Spline"); },
                [this]() { return tr("Plot Cubic Spline Curve"); },
                [this]() { triggerPlot2D(Graph::Spline); });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-bars"),
                [this]() { return tr("Stacked Bars"); },
                [this]() { return tr("Plot Stacked Vertical Bar Chart"); },
                [this]() {
                    if (!d_app)
                        return;
                    if (!QMetaObject::invokeMethod(d_app.data(), "plotStackedBars",
                                                   Qt::DirectConnection)) {
                        triggerPlot2D(Graph::VerticalBars);
                    }
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-spline"),
                [this]() { return tr("Data Polar Plot (r, theta)"); },
                [this]() {
                    return tr("Plot Polar Graph from Worksheet Columns (Angle, Radius)");
                },
                [this]() {
                    if (!d_app)
                        return;
                    if (!QMetaObject::invokeMethod(d_app.data(), "plotPolarData",
                                                   Qt::DirectConnection)) {
                        d_app->showFunctionDialog();
                    }
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-box"), [this]() { return tr("Box Plot"); },
                [this]() { return tr("Plot Statistical Box-and-Whisker Chart"); },
                [this]() { triggerPlot2D(Graph::Box); });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-violin"),
                [this]() { return tr("Violin Plot"); },
                [this]() { return tr("Plot Kernel Density Violin Chart"); },
                [this]() {
                    if (d_app)
                        d_app->plotViolin();
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-histogram"),
                [this]() { return tr("Histogram"); },
                [this]() { return tr("Plot Frequency Histogram"); },
                [this]() { triggerPlot2D(Graph::Histogram); });
        // E11: histogram with a cumulative % curve, colour-mapped scatter
        ExtraPlotTypes::registerTranslations();
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-histogram"),
                [this]() { return tr("Histogram + Cumulative %"); },
                [this]() { return tr("Frequency histogram with the cumulative percentage on the right axis"); },
                [this]() {
                    if (d_app)
                        ExtraPlotTypes::showHistogramCumulativeDialog(d_app);
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-scatter"),
                [this]() { return tr("Color-Mapped Scatter (Z)"); },
                [this]() {
                    return tr("Scatter plot whose symbol color follows a third column (and size a fourth), with a color bar");
                },
                [this]() {
                    if (d_app)
                        ExtraPlotTypes::showColorScatterDialog(d_app);
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-waterfall"),
                [this]() { return tr("Waterfall / Y-Stack"); },
                [this]() { return tr("Plot Y-Offset Stacked Waterfall Graph"); },
                [this]() {
                    if (d_app)
                        d_app->plotYOffsetStack();
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-ternary"),
                [this]() { return tr("Ternary Plot (A-B-C)"); },
                [this]() { return tr("Plot Ternary Composition Diagram (A-B-C)"); },
                [this]() {
                    if (d_app)
                        d_app->plotTernary();
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-spline"),
                [this]() { return tr("Polar / Function Plot..."); },
                [this]() { return tr("Plot Polar, Parametric, or Explicit Mathematical Function"); },
                [this]() {
                    if (d_app)
                        d_app->showFunctionDialog();
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-vectors"),
                [this]() { return tr("Vector Field (XYXY)"); },
                [this]() { return tr("Plot 2D Vector Field (XYXY)"); },
                [this]() {
                    if (d_app)
                        d_app->plotVectXYXY();
                });
        specialtyMenu->addSeparator();
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-heatmap"),
                [this]() { return tr("Heatmap (Color Map)"); },
                [this]() { return tr("Plot Matrix Color Map / Heatmap"); },
                [this]() { triggerMatrixOrXyzPlot(0); });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-contour"),
                [this]() { return tr("Contour Lines"); },
                [this]() { return tr("Plot Matrix Contour Lines"); },
                [this]() { triggerMatrixOrXyzPlot(1); });
        // E11b: 2D density (2D histogram) of two columns and the XRD pole figure
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-heatmap"),
                [this]() { return tr("2D Density (2D Histogram)"); },
                [this]() {
                    return tr("Count X-Y points on a grid and show the counts as a heat map with a color bar (fast for 100,000+ points)");
                },
                [this]() {
                    if (d_app)
                        ExtraPlotTypes::showDensity2DDialog(d_app);
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-contour"),
                [this]() { return tr("Pole Figure (XRD)"); },
                [this]() {
                    return tr("XRD pole figure from alpha, beta, intensity columns or an alpha x beta matrix: equal-area (Schmidt) or equal-angle (Wulff) projection with contours");
                },
                [this]() {
                    if (d_app)
                        ExtraPlotTypes::showPoleFigureDialog(d_app);
                });

        QMenu *plot3dMenu = nullptr;
        d_btn_plot_3d = addRibbonMenuButton(
                gMore, QStringLiteral("plot-3d-surface"),
                [this, kCaret]() { return tr("3D Plots") + kCaret; },
                [this]() { return tr("3D Surface, Scatter & Bar Plots"); }, &plot3dMenu);
        addRibbonMenuAction(
                plot3dMenu, QStringLiteral("plot-3d-surface"),
                [this]() { return tr("3D Surface"); }, [this]() { return tr("Plot 3D Surface Mesh"); },
                [this]() { triggerMatrixOrXyzPlot(2); });
        addRibbonMenuAction(
                plot3dMenu, QStringLiteral("plot-3d-scatter"),
                [this]() { return tr("3D Scatter"); },
                [this]() { return tr("Plot 3D Scatter Points"); },
                [this]() { triggerTable3DPlot(Graph3D::Scatter); });
        addRibbonMenuAction(
                plot3dMenu, QStringLiteral("plot-3d-bars"), [this]() { return tr("3D Bars"); },
                [this]() { return tr("Plot 3D Bars"); },
                [this]() { triggerTable3DPlot(Graph3D::Bars); });

        // C1-1: Canvas interaction modes (Pointer / Zoom / Data Reader) belong on Plot, not Analysis.
        QHBoxLayout *gCanvasTools = nullptr;
        createRibbonGroup(row, &gCanvasTools, true, [this]() { return tr("Canvas Tools"); }, 65);
        for (QToolButton *tb :
             { addActionButton(
                       gCanvasTools, QStringLiteral("pointer"), [this]() { return tr("Pointer"); },
                       [this]() { return tr("Pointer: leave zoom, reader and gadget modes (Esc)"); },
                       QStringLiteral("btnPointer"), true),
               addActionButton(
                       gCanvasTools, QStringLiteral("zoom-in"), [this]() { return tr("Zoom"); },
                       [this]() { return tr("Zoom into a dragged rectangle (Ctrl++)"); },
                       QStringLiteral("btnZoomIn"), true),
               addActionButton(
                       gCanvasTools, QStringLiteral("data-reader"), [this]() { return tr("Reader"); },
                       [this]() { return tr("Read data values by clicking points (Ctrl+D)"); },
                       QStringLiteral("btnCursor"), true) }) {
            tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
        }

        // C4/A3-5/B5-9: graph additions (Text, Legend, Arrow, Line, Image, Error Bars, Reference Lines).
        QHBoxLayout *gAdd = nullptr;
        createRibbonGroup(row, &gAdd, true, [this]() { return tr("Add to Graph"); }, 45);
        QMenu *addMenu = nullptr;
        addRibbonMenuButton(
                gAdd, QStringLiteral("text-label"), [this, kCaret]() { return tr("Add") + kCaret; },
                [this]() {
                    return tr("Draw Arrow, Draw Line, Add Image, Text, Legend, Error Bars & Reference Lines");
                },
                &addMenu);
        addMenu->setObjectName(QStringLiteral("RibbonAddToGraphMenu"));
        if (d_app) {
            for (const char *name : { "actionAddText", "actionNewLegend" }) {
                if (QAction *a = d_app->findChild<QAction *>(QString::fromLatin1(name)))
                    addMenu->addAction(a);
            }
            addRibbonMenuAction(
                    addMenu, QStringLiteral("line-arrow"), [this]() { return tr("Draw Arrow"); },
                    [this]() { return tr("Draw an arrow annotation on the active graph"); },
                    [this]() {
                        if (activeMultiLayerOrUxHint(d_app))
                            d_app->drawArrow();
                    });
            addRibbonMenuAction(
                    addMenu, QStringLiteral("plot-line"), [this]() { return tr("Draw Line"); },
                    [this]() { return tr("Draw a straight line annotation on the active graph"); },
                    [this]() {
                        if (activeMultiLayerOrUxHint(d_app))
                            d_app->drawLine();
                    });
            if (QAction *aImg = d_app->findChild<QAction *>(QStringLiteral("actionAddImage")))
                addMenu->addAction(aImg);
            addMenu->addSeparator();
            if (QAction *a = d_app->findChild<QAction *>(QStringLiteral("actionAddErrorBars")))
                addMenu->addAction(a);
            addMenu->addAction(ExtraPlotTypes::action(d_app, ExtraPlotTypes::ActErrorBand));
            addMenu->addSeparator();
            addRibbonMenuAction(
                    addMenu, QStringLiteral("mean-line"),
                    [this]() { return tr("y = 0 Reference Line"); },
                    [this]() { return tr("Toggle horizontal dashed y = 0 reference line"); },
                    [this]() {
                        if (MultiLayer *ml = activeMultiLayerOrUxHint(d_app)) {
                            if (Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->layer(1)) {
                                GraphInspectorDock::toggleZeroReferenceLine(g, 0);
                                if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                                    insp->refreshAll();
                            }
                        }
                    });
            addRibbonMenuAction(
                    addMenu, QStringLiteral("mean-line"),
                    [this]() { return tr("x = 0 Reference Line"); },
                    [this]() { return tr("Toggle vertical dashed x = 0 reference line"); },
                    [this]() {
                        if (MultiLayer *ml = activeMultiLayerOrUxHint(d_app)) {
                            if (Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->layer(1)) {
                                GraphInspectorDock::toggleZeroReferenceLine(g, 1);
                                if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                                    insp->refreshAll();
                            }
                        }
                    });
            addRibbonMenuAction(
                    addMenu, QStringLiteral("curve-add"),
                    [this]() { return tr("Add Reference Line..."); },
                    [this]() { return tr("Add a custom constant or statistical reference line"); },
                    [this]() {
                        if (MultiLayer *ml = activeMultiLayerOrUxHint(d_app)) {
                            if (Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->layer(1))
                                GraphInspectorDock::addReferenceLineInteractive(g, 0);
                        }
                    });
        }
        addRibbonButton(
                gAdd, QStringLiteral("error-bars"), [this]() { return tr("Error Bars"); },
                [this]() { return tr("Add Error Bars from Column, SD, or Percentage"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app)) {
                        if (QAction *a = d_app->findChild<QAction *>(
                                    QStringLiteral("actionAddErrorBars"))) {
                            a->trigger();
                        }
                    }
                });

        createRibbonGroup(row, &gLayer, false, [this]() { return tr("Layers"); }, 75);
        addRibbonButton(
                gLayer, QStringLiteral("layer-linked"), [this]() { return tr("Linked Layer"); },
                [this]() { return tr("Add Linked Top-X / Right-Y Layer"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->addLinkedTopRightLayer();
                });
        addRibbonButton(
                gLayer, QStringLiteral("layer-inset"), [this]() { return tr("Zoom Inset"); },
                [this]() { return tr("Add Magnified Zoom Inset Layer"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->addZoomInsetLayer();
                });
        QMenu *moreLayersMenu = nullptr;
        addRibbonMenuButton(
                gLayer, QStringLiteral("layer-arrange"),
                [this, kCaret]() { return tr("More Layers") + kCaret; },
                [this]() {
                    return tr("Arrange multiple layers, merge open graphs, or create multi-panel figures");
                },
                &moreLayersMenu);
        addRibbonMenuAction(
                moreLayersMenu, QStringLiteral("merge-graphs"),
                [this]() { return tr("Merge Graphs"); },
                [this]() { return tr("Merge Multiple Open Graph Windows into Multi-Panel Figure"); },
                [this]() {
                    if (d_app)
                        d_app->mergeOpenGraphsDialog();
                });
        addRibbonMenuAction(
                moreLayersMenu, QStringLiteral("layer-arrange"),
                [this]() { return tr("Arrange Layers..."); },
                [this]() { return tr("Configure Multi-Panel Grid Layout & Layer Spacing"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->showLayerDialog();
                });
        addRibbonMenuAction(
                moreLayersMenu, QStringLiteral("layer-add"),
                [this]() { return tr("Add Layer"); },
                [this]() { return tr("Add a New Plot Layer to the Active Graph"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app)) {
                        if (QAction *a = d_app->findChild<QAction *>(QStringLiteral("actionAddLayer")))
                            a->trigger();
                        else
                            d_app->addLayer();
                    }
                });
        moreLayersMenu->addSeparator();
        if (d_app) {
            if (QAction *a2v = d_app->findChild<QAction *>(QStringLiteral("actionPlot2VerticalLayers")))
                moreLayersMenu->addAction(a2v);
            if (QAction *a4 = d_app->findChild<QAction *>(QStringLiteral("actionPlot4Layers")))
                moreLayersMenu->addAction(a4);
        }

        row->addStretch(1);
        addTabPage([this]() { return tr("Plot"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 3 (index 2): Materials (Pure Materials Science & Domain Analysis)
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gDomain = nullptr;

        createRibbonGroup(row, &gDomain, false, [this]() { return tr("Materials Analysis"); }, 90);
        addRibbonButton(
                gDomain, QStringLiteral("curvemath"),
                [this]() { return tr("Tensile / 0.2% Yield"); },
                [this]() {
                    return tr("Tensile Stress-Strain: Young's Modulus E, 0.2% Proof Stress & UTS");
                },
                [this]() {
                    if (d_app)
                        CurveMathTools::showTensileYieldDialog(d_app);
                });
        addRibbonButton(
                gDomain, QStringLiteral("work-hardening"),
                [this]() { return tr("Work Hardening"); },
                [this]() {
                    return tr("True Stress-Strain, Hollomon n, Crussard-Jaoul & Considere Necking");
                },
                [this]() {
                    if (d_app)
                        CurveMathTools::showWorkHardeningDialog(d_app);
                });
        addRibbonButton(
                gDomain, QStringLiteral("kissinger-tds"),
                [this]() { return tr("Kissinger TDS"); },
                [this]() {
                    return tr("Kissinger Thermal Desorption (TDS / DSC) Activation Energy Analysis");
                },
                [this]() {
                    if (d_app)
                        CurveMathTools::showKissingerDialog(d_app);
                });
        addRibbonButton(
                gDomain, QStringLiteral("xrd-peaks"), [this]() { return tr("XRD / W-H"); },
                [this]() {
                    return tr("Powder XRD K-alpha2 Stripping, Scherrer & Williamson-Hall Analysis");
                },
                [this]() {
                    if (d_app)
                        CurveMathTools::showXRDAnalyzerDialog(d_app);
                });
        ExtraPlotTypes::registerTranslations(); // E11b: XRD pole figure
        addRibbonButton(
                gDomain, QStringLiteral("plot-contour"), [this]() { return tr("Pole Figure"); },
                [this]() {
                    return tr("XRD pole figure from alpha, beta, intensity columns or an alpha x beta matrix: equal-area (Schmidt) or equal-angle (Wulff) projection with contours");
                },
                [this]() {
                    if (d_app)
                        ExtraPlotTypes::showPoleFigureDialog(d_app);
                });
        addRibbonButton(
                gDomain, QStringLiteral("weibull"), [this]() { return tr("Weibull"); },
                [this]() { return tr("2- and 3-Parameter Weibull Reliability Probability Plot"); },
                [this]() {
                    if (d_app)
                        d_app->showWeibullPlotDialog();
                });
        {
            ExtraPlotTypes::registerTranslations();
            QMenu *probMenu = nullptr;
            addRibbonMenuButton(
                    gDomain, QStringLiteral("weibull"),
                    [this, kCaret]() { return tr("Probability Plots") + kCaret; },
                    [this]() {
                        return tr("Gumbel extreme value plot (e.g. maximum defect size): location, scale and return-period value");
                    },
                    &probMenu);
            addRibbonMenuAction(
                    probMenu, QStringLiteral("weibull"),
                    [this]() { return tr("Gumbel (Extreme Value) Probability Plot"); },
                    [this]() {
                        return tr("Gumbel extreme value plot (e.g. maximum defect size): location, scale and return-period value");
                    },
                    [this]() {
                        if (d_app)
                            ExtraPlotTypes::showProbabilityPlotDialog(d_app, ExtraPlotTypes::Gumbel);
                    });
            addRibbonMenuAction(
                    probMenu, QStringLiteral("weibull"),
                    [this]() { return tr("Lognormal Probability Plot"); },
                    [this]() {
                        return tr("Lognormal probability paper: rank plotting positions and a straight-line fit (mu, sigma)");
                    },
                    [this]() {
                        if (d_app)
                            ExtraPlotTypes::showProbabilityPlotDialog(d_app, ExtraPlotTypes::LogNormal);
                    });
        }
        addRibbonButton(
                gDomain, QStringLiteral("arrhenius"), [this]() { return tr("Arrhenius"); },
                [this]() { return tr("Arrhenius ln(k) vs 1000/T Activation Energy Analysis"); },
                [this]() {
                    if (d_app)
                        d_app->showArrheniusPlotDialog();
                });
        addRibbonButton(
                gDomain, QStringLiteral("vasp-bands"), [this]() { return tr("VASP Bands/DOS"); },
                [this]() {
                    return tr("VASP EIGENVAL / DOSCAR Electronic Band Structure & DOS Plotter");
                },
                [this]() {
                    if (d_app)
                        CurveMathTools::showVaspPlotterDialog(d_app);
                });
        QMenu *advMatMenu = nullptr;
        QToolButton *advMatBtn = addRibbonMenuButton(
                gDomain, QStringLiteral("tauc"),
                [this, kCaret]() { return tr("Advanced MatSci") + kCaret; },
                [this]() { return tr("Tauc Optical Bandgap & EIS Impedance Analysis"); },
                &advMatMenu);
        if (advMatBtn)
            advMatBtn->setProperty("_tg_advanced_domain", true);
        addRibbonMenuAction(
                advMatMenu, QStringLiteral("tauc"), [this]() { return tr("Tauc Bandgap"); },
                [this]() { return tr("Tauc Optical Bandgap Extrapolation"); },
                [this]() {
                    if (d_app)
                        d_app->showTaucPlotDialog();
                });
        addRibbonMenuAction(
                advMatMenu, QStringLiteral("eis-nyquist"),
                [this]() { return tr("EIS Analyzer"); },
                [this]() { return tr("Electrochemical Impedance Spectroscopy Nyquist/Bode Fit"); },
                [this]() {
                    if (d_app)
                        d_app->showEISAnalyzerDialog();
                });

        row->addStretch(1);
        addTabPage([this]() { return tr("Materials"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 4 (index 3): Analysis (Unified Peaks, Fitting, Math, Gadgets, Signal & Statistics)
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gFit = nullptr;
        QHBoxLayout *gMath = nullptr;
        QHBoxLayout *gGadgets = nullptr;
        QHBoxLayout *gSignal = nullptr;
        QHBoxLayout *gStats = nullptr;

        createRibbonGroup(row, &gFit, true, [this]() { return tr("Curve Fitting"); }, 90);
        addRibbonButton(
                gFit, QStringLiteral("peak-analyzer"), [this]() { return tr("Peak Analyzer"); },
                [this]() { return tr("Interactive Baseline & Multi-Peak Deconvolution"); },
                [this]() {
                    if (!d_app)
                        return;
                    if (Table *tbl = qobject_cast<Table *>(d_app->d_workspace.activeSubWindow())) {
                        if (!tbl->drawableColumnSelection().isEmpty()) {
                            d_app->showPeakAnalyzerDialog(0);
                            return;
                        }
                    }
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->showPeakAnalyzerDialog(0);
                });
        QMenu *morePeaksMenu = nullptr;
        addRibbonMenuButton(
                gFit, QStringLiteral("peak-voigt"),
                [this, kCaret]() { return tr("Peaks") + kCaret; },
                [this]() { return tr("Auto Voigt, Batch & 2D Matrix Peak Analysis"); },
                &morePeaksMenu);
        addRibbonMenuAction(
                morePeaksMenu, QStringLiteral("peak-voigt"),
                [this]() { return tr("Auto Voigt Peaks"); },
                [this]() { return tr("Automatic Pseudo-Voigt Peak Detection & Fit"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->fitAutoPeakPseudoVoigt();
                });
        addRibbonMenuAction(
                morePeaksMenu, QStringLiteral("peak-batch"),
                [this]() { return tr("Batch Peaks"); },
                [this]() { return tr("Batch Multi-Spectrum Peak Analysis"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->showBatchPeakAnalysisDialog();
                });
        addRibbonMenuAction(
                morePeaksMenu, QStringLiteral("peak-2d"), [this]() { return tr("2D Peaks"); },
                [this]() { return tr("Matrix 2D Peak Detection & Fitting"); },
                [this]() {
                    if (d_app)
                        d_app->show2DPeakAnalysisDialog();
                });
        addRibbonButton(
                gFit, QStringLiteral("fit-linear"), [this]() { return tr("Linear Fit"); },
                [this]() { return tr("Linear Regression Fit"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->fitLinear();
                });
        addRibbonButton(
                gFit, QStringLiteral("fit-nonlinear"), [this]() { return tr("Nonlinear Fit"); },
                [this]() { return tr("Nonlinear Curve Fit Wizard"); },
                [this]() {
                    if (!d_app)
                        return;
                    if (Table *tbl = qobject_cast<Table *>(d_app->d_workspace.activeSubWindow())) {
                        if (!tbl->drawableColumnSelection().isEmpty()) {
                            d_app->showFitDialog();
                            return;
                        }
                    }
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->showFitDialog();
                });
        QMenu *advFitMenu = nullptr;
        addRibbonMenuButton(
                gFit, QStringLiteral("fit-global"),
                [this, kCaret]() { return tr("Advanced Fit") + kCaret; },
                [this]() {
                    const bool showAdv =
                            d_app ? d_app->showsAdvancedDomainFeatures() : d_advanced_domain_visible;
                    return showAdv
                            ? tr("York, Global, Piecewise, Robust, Logistic, 3D Surface & Model Compare")
                            : tr("York, Global, Piecewise, Robust, 3D Surface & Model Compare");
                },
                &advFitMenu);
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-poly"), [this]() { return tr("Polynomial"); },
                [this]() { return tr("Polynomial Regression Fit"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->showFitPolynomDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-york"), [this]() { return tr("York XY Fit"); },
                [this]() { return tr("York Linear Regression with Both X and Y Errors"); },
                [this]() {
                    if (d_app)
                        d_app->showYorkLinearFitDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-global"), [this]() { return tr("Global Fit"); },
                [this]() { return tr("Multi-Curve Global Fit with Shared Parameters"); },
                [this]() {
                    if (d_app)
                        d_app->showGlobalCurveFitDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-piecewise"),
                [this]() { return tr("Piecewise Fit"); },
                [this]() { return tr("Piecewise Segmented Linear Fit"); },
                [this]() {
                    if (d_app)
                        d_app->showPiecewiseLinearFitDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-robust"), [this]() { return tr("Robust Fit"); },
                [this]() { return tr("Huber / Bisquare Outlier-Resistant Robust Fit"); },
                [this]() {
                    if (d_app)
                        d_app->showRobustLinearFitDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-logistic"),
                [this]() { return tr("Logistic Fit"); },
                [this]() { return tr("Binary Logistic Regression"); },
                [this]() {
                    if (d_app)
                        d_app->showLogisticRegressionDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-surface-3d"),
                [this]() { return tr("3D Surface Fit"); },
                [this]() { return tr("2D / 3D Surface Regression Fit"); },
                [this]() {
                    if (d_app)
                        d_app->showSurfaceFit3DDialog();
                });
        advFitMenu->addSeparator();
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-compare"),
                [this]() { return tr("Model Compare"); },
                [this]() { return tr("Compare Candidate Fit Models (AIC / BIC / F-Test)"); },
                [this]() {
                    if (d_app)
                        d_app->showModelComparisonDialog();
                });

        // C1-1: Curve Math, Fill Between, Digitizer & Masks belong in Analysis (not Materials).
        createRibbonGroup(row, &gMath, true, [this]() { return tr("Math & Tools"); }, 65);
        QMenu *mathToolsMenu = nullptr;
        addRibbonMenuButton(
                gMath, QStringLiteral("curvemath"),
                [this, kCaret]() { return tr("Math & Tools") + kCaret; },
                [this]() { return tr("Curve Math, Fill Between Curves & Graph Digitizer"); },
                &mathToolsMenu);
        addRibbonMenuAction(
                mathToolsMenu, QStringLiteral("curvemath"), [this]() { return tr("Curve Math"); },
                [this]() { return tr("Arithmetic Operations Between Curves"); },
                [this]() {
                    if (d_app)
                        d_app->showSimpleCurveMathDialog();
                });
        addRibbonMenuAction(
                mathToolsMenu, QStringLiteral("curve-fill"),
                [this]() { return tr("Fill Between"); },
                [this]() { return tr("Shade Area Between Two Curves"); },
                [this]() {
                    if (d_app)
                        d_app->showFillBetweenCurvesDialog();
                });
        addRibbonMenuAction(
                mathToolsMenu, QStringLiteral("digitizer"), [this]() { return tr("Digitizer"); },
                [this]() { return tr("Extract Numerical Data from Graph Image"); },
                [this]() {
                    if (d_app)
                        d_app->showGraphDigitizerDialog();
                });
        mathToolsMenu->addSeparator();
        addRibbonMenuAction(
                mathToolsMenu, QStringLiteral("mask-points"),
                [this]() { return tr("Mask Points"); },
                [this]() { return tr("Mask Individual Outlier Data Points"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->maskDataPointsTool();
                });
        addRibbonMenuAction(
                mathToolsMenu, QStringLiteral("range-selector"),
                [this]() { return tr("Mask ROI"); },
                [this]() { return tr("Mask Rectangular Data Region"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->maskRegionGadget();
                });
        addRibbonMenuAction(
                mathToolsMenu, QStringLiteral("clear-masks"),
                [this]() { return tr("Clear Masks"); },
                [this]() { return tr("Clear All Data Masks on Active Graph"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->clearAllMasksOnGraph();
                });

        createRibbonGroup(row, &gGadgets, true, [this]() { return tr("Gadgets"); }, 60);
        addRibbonButton(
                gGadgets, QStringLiteral("gadget-roi"), [this]() { return tr("ROI Stats"); },
                [this]() { return tr("Interactive Region-of-Interest Statistics Gadget"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->attachRegionStatsGadget();
                });
        addRibbonButton(
                gGadgets, QStringLiteral("gadget-integrate"), [this]() { return tr("Integrate"); },
                [this]() { return tr("Interactive Peak / Curve Area Integration Gadget"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->attachIntegrateGadget();
                });
        addRibbonButton(
                gGadgets, QStringLiteral("gadget-quick-fit"), [this]() { return tr("Quick Fit"); },
                [this]() { return tr("Interactive ROI Quick Fit Gadget"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->attachQuickFitGadget();
                });
        QMenu *moreGadgetsMenu = nullptr;
        addRibbonMenuButton(
                gGadgets, QStringLiteral("gadget-delta"),
                [this, kCaret]() { return tr("More Gadgets") + kCaret; },
                [this]() {
                    return tr("Vertical Cursor, Quick Peaks, Delta Cursor, 2D Profile, Differentiate & Clear");
                },
                &moreGadgetsMenu);
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-cursor"),
                [this]() { return tr("Vertical Cursor"); },
                [this]() { return tr("Vertical Crosshair Curve Readout Gadget"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->attachVerticalCursorGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-quick-peaks"),
                [this]() { return tr("Quick Peaks"); },
                [this]() { return tr("Interactive ROI Quick Peak Finder Gadget"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->attachQuickPeaksGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-delta"),
                [this]() { return tr("Delta Cursor"); },
                [this]() { return tr("Dual-Cursor Delta X / Delta Y / Slope Gadget"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->attachDeltaCursorGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-profile-2d"),
                [this]() { return tr("2D Profile"); },
                [this]() { return tr("Interactive Heatmap Horizontal/Vertical Profile Gadget"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->attachImageProfileGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-diff"),
                [this]() { return tr("Differentiate"); },
                [this]() { return tr("Interactive Derivative Gadget"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->attachDifferentiateGadget();
                });
        moreGadgetsMenu->addSeparator();
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-clear"),
                [this]() { return tr("Clear Gadget"); },
                [this]() { return tr("Remove Active Gadget from Graph"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->clearActiveGadget();
                });

        createRibbonGroup(row, &gSignal, true, [this]() { return tr("Signal Processing"); }, 50);
        QMenu *filterMenu = nullptr;
        addRibbonMenuButton(
                gSignal, QStringLiteral("signal-savgol"),
                [this, kCaret]() { return tr("Filter") + kCaret; },
                [this]() { return tr("LOWESS, Whittaker, IIR & Matrix 2D Filters"); },
                &filterMenu);
        addRibbonMenuAction(
                filterMenu, QStringLiteral("signal-savgol"),
                [this]() { return tr("Savitzky-Golay Smooth"); },
                [this]() { return tr("Savitzky-Golay Smoothing & Derivative"); },
                [this]() {
                    if (d_app)
                        d_app->showSavGolSmoothDerivativeDialog();
                });
        addRibbonMenuAction(
                filterMenu, QStringLiteral("signal-lowess"),
                [this]() { return tr("LOWESS Smooth"); },
                [this]() { return tr("Locally Weighted Scatterplot Smoothing (LOWESS)"); },
                [this]() {
                    if (d_app)
                        d_app->showSmoothLOWESSDialog();
                });
        addRibbonMenuAction(
                filterMenu, QStringLiteral("signal-whittaker"),
                [this]() { return tr("Whittaker Smooth"); },
                [this]() { return tr("Penalized Least-Squares Whittaker Smoothing"); },
                [this]() {
                    if (d_app)
                        d_app->showSmoothWhittakerDialog();
                });
        addRibbonMenuAction(
                filterMenu, QStringLiteral("signal-iir"), [this]() { return tr("IIR Filter"); },
                [this]() { return tr("Zero-Phase Butterworth / Chebyshev / Bessel IIR Filter"); },
                [this]() {
                    if (d_app)
                        d_app->showIirDigitalFilterDialog();
                });
        addRibbonMenuAction(
                filterMenu, QStringLiteral("matrix-filter2d"),
                [this]() { return tr("Matrix 2D Filter"); },
                [this]() { return tr("2D Matrix Gaussian / Median / Detrend Filter"); },
                [this]() {
                    if (d_app)
                        d_app->showMatrixFilter2DDialog();
                });

        QMenu *spectralMenu = nullptr;
        addRibbonMenuButton(
                gSignal, QStringLiteral("signal-fft"),
                [this, kCaret]() { return tr("Spectral") + kCaret; },
                [this]() {
                    const bool showAdv =
                            d_app ? d_app->showsAdvancedDomainFeatures() : d_advanced_domain_visible;
                    return showAdv
                            ? tr("STFT Spectrogram, Wavelet, Welch PSD & FSD Deconvolution")
                            : tr("Welch PSD & FSD Deconvolution");
                },
                &spectralMenu);
        addRibbonMenuAction(
                spectralMenu, QStringLiteral("signal-fft"), [this]() { return tr("FFT Spectrum"); },
                [this]() { return tr("Fast Fourier Transform (FFT)"); },
                [this]() {
                    if (d_app)
                        d_app->showFFTDialog();
                });
        addRibbonMenuAction(
                spectralMenu, QStringLiteral("signal-stft"),
                [this]() { return tr("STFT Spectrogram"); },
                [this]() { return tr("Short-Time Fourier Transform Spectrogram"); },
                [this]() {
                    if (d_app)
                        d_app->showStftSpectrogramDialog();
                });
        addRibbonMenuAction(
                spectralMenu, QStringLiteral("signal-wavelet"),
                [this]() { return tr("Wavelet Transform"); },
                [this]() { return tr("Continuous & Discrete Wavelet Denoising / CWT"); },
                [this]() {
                    if (d_app)
                        d_app->showWaveletTransformDialog();
                });
        addRibbonMenuAction(
                spectralMenu, QStringLiteral("signal-psd"), [this]() { return tr("Welch PSD"); },
                [this]() { return tr("Welch Power Spectral Density & Coherence"); },
                [this]() {
                    if (d_app)
                        d_app->showWelchPsdCoherenceDialog();
                });
        addRibbonMenuAction(
                spectralMenu, QStringLiteral("signal-fsd"),
                [this]() { return tr("Deconvolve FSD"); },
                [this]() { return tr("Fourier Self-Deconvolution (Resolution Enhancement)"); },
                [this]() {
                    if (d_app)
                        d_app->showFourierSelfDeconvolveDialog();
                });

        // B2-10: Unified Group Compare (Parametric ANOVA + Nonparametric + Hypothesis Tests)
        createRibbonGroup(row, &gStats, false, [this]() { return tr("Statistics"); }, 40);
        QMenu *groupCmpMenu = nullptr;
        addRibbonMenuButton(
                gStats, QStringLiteral("stats-anova"),
                [this, kCaret]() { return tr("Group Compare") + kCaret; },
                [this]() {
                    return tr("Parametric ANOVA, Nonparametric Tests, Normality & Outlier Tests");
                },
                &groupCmpMenu);
        addRibbonMenuAction(
                groupCmpMenu, QStringLiteral("stats-anova"),
                [this]() { return tr("Group ANOVA (Parametric)"); },
                [this]() { return tr("One-Way ANOVA & Tukey / Dunnett Post-Hoc Comparison"); },
                [this]() {
                    if (d_app)
                        d_app->showGroupComparisonDialog();
                });
        addRibbonMenuAction(
                groupCmpMenu, QStringLiteral("stats-nonparam"),
                [this]() { return tr("Nonparametric (Mann-Whitney / Kruskal-Wallis)"); },
                [this]() { return tr("Mann-Whitney / Wilcoxon / Kruskal-Wallis Tests"); },
                [this]() {
                    if (d_app)
                        d_app->showNonparametricTestsDialog();
                });
        addRibbonMenuAction(
                groupCmpMenu, QStringLiteral("stats-twoway"), [this]() { return tr("Two-Way ANOVA"); },
                [this]() { return tr("Two-Way Factorial ANOVA & Interaction Plot"); },
                [this]() {
                    if (d_app)
                        d_app->showTwoWayANOVAPlotDialog();
                });
        groupCmpMenu->addSeparator();
        addRibbonMenuAction(
                groupCmpMenu, QStringLiteral("stats-normality"),
                [this]() { return tr("Normality Test"); },
                [this]() { return tr("Shapiro-Wilk / Anderson-Darling Normality Test"); },
                [this]() {
                    if (d_app)
                        d_app->showNormalityTestDialog();
                });
        addRibbonMenuAction(
                groupCmpMenu, QStringLiteral("stats-outlier"), [this]() { return tr("Outlier Test"); },
                [this]() { return tr("Grubbs / Generalized ESD Outlier Detection"); },
                [this]() {
                    if (d_app)
                        d_app->showOutlierTestDialog();
                });
        groupCmpMenu->addSeparator();
        addRibbonMenuAction(
                groupCmpMenu, QStringLiteral("stats-paired"), [this]() { return tr("Paired Plot"); },
                [this]() { return tr("Paired Before-After Comparison Plot"); },
                [this]() {
                    if (d_app)
                        d_app->showPairedComparisonPlotDialog();
                });
        addRibbonMenuAction(
                groupCmpMenu, QStringLiteral("stats-bland-altman"),
                [this]() { return tr("Bland-Altman Plot"); },
                [this]() { return tr("Bland-Altman Method Agreement Plot"); },
                [this]() {
                    if (d_app)
                        d_app->showBlandAltmanPlotDialog();
                });
        addRibbonMenuAction(
                groupCmpMenu, QStringLiteral("stats-bablok"),
                [this]() { return tr("Passing-Bablok Fit"); },
                [this]() { return tr("Passing-Bablok & Deming Method Comparison Regression"); },
                [this]() {
                    if (d_app)
                        d_app->showPassingBablokDemingDialog();
                });

        QMenu *relMultiMenu = nullptr;
        addRibbonMenuButton(
                gStats, QStringLiteral("stats-pca"),
                [this, kCaret]() { return tr("Multivariate") + kCaret; },
                [this]() {
                    const bool showAdv =
                            d_app ? d_app->showsAdvancedDomainFeatures() : d_advanced_domain_visible;
                    return showAdv
                            ? tr("PCA Biplot, Correlation Matrix, Clustering, Kaplan-Meier & ROC Curve")
                            : tr("PCA Biplot, Correlation Matrix & Clustering");
                },
                &relMultiMenu);
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-pca"), [this]() { return tr("PCA Biplot"); },
                [this]() { return tr("Principal Component Analysis (PCA) & Biplot"); },
                [this]() {
                    if (d_app)
                        d_app->showPCABiplotDialog();
                });
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-corr"), [this]() { return tr("Correlation"); },
                [this]() { return tr("Pearson / Spearman Correlation Matrix Heatmap"); },
                [this]() {
                    if (d_app)
                        d_app->showCorrelationMatrixDialog();
                });
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-cluster"),
                [this]() { return tr("Clustering"); },
                [this]() { return tr("K-Means & Hierarchical Cluster Analysis"); },
                [this]() {
                    if (d_app)
                        d_app->showClusteringDialog();
                });
        relMultiMenu->addSeparator();
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-survival"),
                [this]() { return tr("Kaplan-Meier Survival"); },
                [this]() { return tr("Kaplan-Meier Survival Curve & Log-Rank Test"); },
                [this]() {
                    if (d_app)
                        d_app->showKaplanMeierDialog();
                });
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-roc"), [this]() { return tr("ROC Curve"); },
                [this]() { return tr("Receiver Operating Characteristic (ROC) Curve & AUC"); },
                [this]() {
                    if (d_app)
                        d_app->showROCCurveDialog();
                });

        row->addStretch(1);
        addTabPage([this]() { return tr("Analysis"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 5 (index 4): Worksheet (C1-7 Contextual Table Operations)
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gRoles = nullptr;
        QHBoxLayout *gFormula = nullptr;
        QHBoxLayout *gCols = nullptr;

        auto trackTblBtn = [this](QToolButton *b) -> QToolButton * {
            if (b)
                d_table_context_widgets.append(b);
            return b;
        };

        createRibbonGroup(row, &gRoles, true, [this]() { return tr("Column Role"); }, 90);
        trackTblBtn(addRibbonButton(
                gRoles, QStringLiteral("new-table"), [this]() { return tr("Set X"); },
                [this]() { return tr("Set selected column(s) as X axis data"); },
                [this]() { applyWorksheetColumnRole(int(SciDAVis::X)); }));
        trackTblBtn(addRibbonButton(
                gRoles, QStringLiteral("plot-line"), [this]() { return tr("Set Y"); },
                [this]() { return tr("Set selected column(s) as Y axis data"); },
                [this]() { applyWorksheetColumnRole(int(SciDAVis::Y)); }));
        trackTblBtn(addRibbonButton(
                gRoles, QStringLiteral("error-bars"), [this]() { return tr("Set ±Y"); },
                [this]() { return tr("Set selected column(s) as Y error bars (yErr)"); },
                [this]() { applyWorksheetColumnRole(int(SciDAVis::yErr)); }));
        trackTblBtn(addRibbonButton(
                gRoles, QStringLiteral("text-label"), [this]() { return tr("Set Label"); },
                [this]() { return tr("Set selected column(s) as text labels"); },
                [this]() { applyWorksheetColumnRole(-100); }));

        createRibbonGroup(row, &gFormula, true, [this]() { return tr("Formula & Data"); }, 80);
        trackTblBtn(addRibbonButton(
                gFormula, QStringLiteral("formula"), [this]() { return tr("Formula F(x)="); },
                [this]() { return tr("Edit column formula, long name, units, and numeric format"); },
                [this]() {
                    if (Table *tbl = resolvedWorksheetTarget()) {
                        const int col = qMax(0, tbl->firstSelectedColumn());
                        tbl->showColumnPropertiesDialog(col);
                    }
                }));
        trackTblBtn(addRibbonButton(
                gFormula, QStringLiteral("sort-table"), [this]() { return tr("Sort..."); },
                [this]() { return tr("Sort worksheet columns or rows"); },
                [this]() {
                    if (Table *tbl = resolvedWorksheetTarget()) {
                        if (tbl->d_future_table) {
                            QList<Column *> cols;
                            for (int c = 0; c < tbl->numCols(); ++c) {
                                if (Column *col = tbl->column(c))
                                    cols.append(col);
                            }
                            if (!cols.isEmpty())
                                tbl->d_future_table->sortDialog(cols);
                        }
                    }
                }));
        trackTblBtn(addRibbonButton(
                gFormula, QStringLiteral("add-column"), [this]() { return tr("Fill Down"); },
                [this]() {
                    return tr("Copy the top row of the selection down to the remaining selected cells (Ctrl+D)");
                },
                [this]() {
                    if (Table *tbl = resolvedWorksheetTarget())
                        tbl->fillSelectionDown();
                }));
        trackTblBtn(addRibbonButton(
                gFormula, QStringLiteral("find"), [this]() { return tr("Find & Replace"); },
                [this]() {
                    return tr("Find or replace values in the active worksheet (Ctrl+F / Ctrl+H)");
                },
                [this]() {
                    if (Table *tbl = resolvedWorksheetTarget())
                        tbl->showFindReplaceDialog(true);
                }));

        createRibbonGroup(row, &gCols, false, [this]() { return tr("Columns & Source"); }, 60);
        trackTblBtn(addRibbonButton(
                gCols, QStringLiteral("add-column"), [this]() { return tr("Add Column"); },
                [this]() { return tr("Append Y Column to Active Worksheet"); },
                [this]() {
                    if (Table *tbl = resolvedWorksheetTarget())
                        tbl->addCol();
                }));
        d_btn_ws_reimport = addRibbonButton(
                gCols, QStringLiteral("recalc-now"), [this]() { return tr("Re-import"); },
                [this]() { return tr("Reload data from the original CSV / Excel source file"); },
                [this]() {
                    if (Table *tbl = resolvedWorksheetTarget()) {
                        if (tbl->hasImportSource())
                            tbl->reimportFromSource();
                    }
                });
        trackTblBtn(addRibbonButton(
                gCols, QStringLiteral("copy-ole"), [this]() { return tr("Copy w/ Headers"); },
                [this]() {
                    return tr("Copy selected cells together with Long Name and Units headers");
                },
                [this]() {
                    if (Table *tbl = resolvedWorksheetTarget())
                        tbl->copySelectionWithHeaders();
                }));
        trackTblBtn(addRibbonButton(
                gCols, QStringLiteral("new-matrix"), [this]() { return tr("To Matrix"); },
                [this]() { return tr("Convert the active worksheet into a 2D numeric matrix"); },
                [this]() {
                    if (!d_app)
                        return;
                    if (Table *tbl = resolvedWorksheetTarget()) {
                        if (d_app->d_workspace.activeSubWindow() != tbl)
                            d_app->activateSubWindow(tbl);
                        d_app->convertTableToMatrix();
                    }
                }));

        row->addStretch(1);
        addTabPage([this]() { return tr("Worksheet"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 6 (index 5): Style & Export
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gPresets = nullptr;
        QHBoxLayout *gExport = nullptr;
        QHBoxLayout *gStyle = nullptr;

        createRibbonGroup(row, &gPresets, true, [this]() { return tr("Journal Presets"); }, 70);
        addRibbonButton(
                gPresets, QStringLiteral("preset-isij"),
                [this]() { return tr("ISIJ 85mm Fig"); },
                [this]() { return tr("Apply ISIJ 1-Column 85x85 mm Journal Preset (600 DPI)"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->applyStyleISIJJournal();
                },
                false, true);
        addRibbonButton(
                gPresets, QStringLiteral("preset-slide"),
                [this]() { return tr("Lab Slide (18pt)"); },
                [this]() {
                    return tr("83.3 mm square, 12.9 pt Arial. Becomes 18 pt on the slide with 'Send to PowerPoint' (140% paste).");
                },
                [this]() {
                    if (MultiLayer *ml = activeMultiLayerOrUxHint(d_app)) {
                        ml->applyStylePresetById(QStringLiteral("lab_slide_83mm"));
                        d_app->modifiedProject();
                        if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                            insp->refreshAll();
                    }
                },
                false, true);
        QMenu *customPresetMenu = nullptr;
        addRibbonMenuButton(
                gPresets, QStringLiteral("page-setup"),
                [this]() { return tr("Presets..."); },
                [this]() { return tr("Apply User Presets or Edit / Add Custom Graph Style Presets"); },
                &customPresetMenu, true);
        if (customPresetMenu) {
            connect(customPresetMenu, &QMenu::aboutToShow, this, [this, customPresetMenu]() {
                customPresetMenu->clear();
                const QVector<MultiLayer::GraphStylePresetSpec> presets =
                        MultiLayer::listStylePresets();
                for (const auto &sp : presets) {
                    const QString pid = sp.id;
                    const QString dispName = sp.builtIn
                            ? QCoreApplication::translate("GraphInspectorDock", sp.name.toUtf8().constData())
                            : sp.name;
                    QAction *act = customPresetMenu->addAction(
                            IconLoader::duotoneIcon(
                                    sp.builtIn ? QStringLiteral("preset-isij")
                                               : QStringLiteral("page-setup"),
                                    16),
                            dispName);
                    connect(act, &QAction::triggered, this, [this, pid]() {
                        if (MultiLayer *ml = activeMultiLayerOrUxHint(d_app)) {
                            ml->applyStylePresetById(pid);
                            d_app->modifiedProject();
                            if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                                insp->refreshAll();
                        }
                    });
                }
                customPresetMenu->addSeparator();
                QAction *multiAct = customPresetMenu->addAction(
                        IconLoader::duotoneIcon(QStringLiteral("batch-plot"), 16),
                        tr("Apply Preset to Multiple Graphs..."));
                connect(multiAct, &QAction::triggered, this, [this]() {
                    if (!d_app)
                        return;
                    if (!QMetaObject::invokeMethod(
                                d_app.data(), "showApplyStylePresetToMultipleGraphsDialog",
                                Qt::DirectConnection)) {
                        d_app->showStylePresetManagerDialog();
                    }
                });
                QAction *mgrAct = customPresetMenu->addAction(
                        IconLoader::duotoneIcon(QStringLiteral("page-setup"), 16),
                        tr("Manage Style Presets..."));
                connect(mgrAct, &QAction::triggered, this, [this]() {
                    if (d_app)
                        d_app->showStylePresetManagerDialog();
                });
            });
        }
        d_btn_lock_aspect = addRibbonButton(
                gPresets, QStringLiteral("lock"),
                [this]() { return tr("Lock Aspect"); },
                [this]() {
                    return tr("Lock or Unlock Active Graph Aspect Ratio on Window Resize");
                },
                [this]() {
                    if (!d_app || !d_btn_lock_aspect)
                        return;
                    if (MultiLayer *ml = activeMultiLayerOrUxHint(d_app)) {
                        const QString before = ml->captureUndoSnapshot();
                        ml->setKeepAspectRatio(d_btn_lock_aspect->isChecked());
                        ml->pushUndoSnapshot(tr("Toggle Lock Aspect Ratio"), before);
                        d_app->modifiedProject();
                        if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                            insp->refreshAll();
                    }
                    syncCheckableStates();
                },
                true);
        d_btn_lock_aspect->setObjectName(QStringLiteral("RibbonLockAspectRatioBtn"));

        createRibbonGroup(row, &gExport, true, [this]() { return tr("Export"); }, 90);
        // A1-6: Match Home tab Copy for PowerPoint behavior (works for 2D & 3D graphs, disabled when no graph)
        d_btn_export_copy_ole = addRibbonButton(
                gExport, QStringLiteral("copy-ole"),
                []() { return ToriGraphOleUi::copyForPowerPointLabel(); },
                []() { return ToriGraphOleUi::copyForPowerPointToolTip(); },
                [this]() {
                    if (d_app)
                        d_app->copyGraphAsOleObject();
                });
        // A3-7/A4-7/C1-6/D3-7: Distinct export-pptx icon & single ribbon entry (no duplicate in More Export).
        QToolButton *btnSendPptx = addRibbonButton(
                gExport, QStringLiteral("export-pptx"),
                []() { return ToriGraphPptx::sendToPowerPointLabel(); },
                []() { return ToriGraphPptx::sendToPowerPointToolTip(); },
                [this]() {
                    if (d_app)
                        ToriGraphPptx::sendGraphsToPowerPoint(d_app);
                });
        if (btnSendPptx) {
            btnSendPptx->setObjectName(QStringLiteral("RibbonSendToPowerPointBtn"));
            d_act_ribbon_send_pptx = new QAction(btnSendPptx);
            d_act_ribbon_send_pptx->setText(ToriGraphPptx::sendToPowerPointLabel());
            d_act_ribbon_send_pptx->setToolTip(ToriGraphPptx::sendToPowerPointToolTip());
            d_act_ribbon_send_pptx->setIcon(
                    IconLoader::duotoneIcon(QStringLiteral("export-pptx"), 18, false));
            connect(d_act_ribbon_send_pptx, &QAction::triggered, this, [this]() {
                if (d_app)
                    ToriGraphPptx::sendGraphsToPowerPoint(d_app);
            });
        }
        addRibbonButton(
                gExport, QStringLiteral("export-image"), [this]() { return tr("Export"); },
                [this]() { return tr("Export Graph to PNG / TIFF / SVG / EMF"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->exportGraph();
                });
        QMenu *moreExportMenu = nullptr;
        addRibbonMenuButton(
                gExport, QStringLiteral("export-pdf"),
                [this, kCaret]() { return tr("More Export") + kCaret; },
                [this]() { return tr("Copy High-DPI Image, Export PDF, or Export Python Script"); },
                &moreExportMenu);
        addRibbonMenuAction(
                moreExportMenu, QStringLiteral("copy-page-image"),
                [this]() { return tr("Copy High-DPI Image"); },
                [this]() { return tr("Copy High-Resolution Bitmap of Graph Page to Clipboard"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->copyPageAsHighDpiImage();
                });
        addRibbonMenuAction(
                moreExportMenu, QStringLiteral("export-pdf"),
                [this]() { return tr("Export PDF"); },
                [this]() { return tr("Export Vector PDF Document"); },
                [this]() {
                    if (d_app)
                        d_app->exportPDF();
                });
        addRibbonMenuAction(
                moreExportMenu, QStringLiteral("code-export"),
                [this]() { return tr("Export Python"); },
                [this]() { return tr("Generate Reproducible Python Script from Active Graph"); },
                [this]() {
                    if (d_app)
                        d_app->exportActiveGraphToPython();
                });

        createRibbonGroup(row, &gStyle, false, [this]() { return tr("Style"); }, 50);
        QMenu *framePageMenu = nullptr;
        addRibbonMenuButton(
                gStyle, QStringLiteral("frame-box-inward"),
                [this, kCaret]() { return tr("Frame & Page") + kCaret; },
                [this]() {
                    return tr("Frame Style, Page Size & DPI, Trim Page, Square 70% & Color Palette");
                },
                &framePageMenu);
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("frame-box-inward"),
                [this]() { return tr("Box Inward"); },
                [this]() { return tr("Apply Full Box Frame with Inward Ticks"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->applyFrameBoxInward();
                });
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("frame-open-l"),
                [this]() { return tr("Open-L Frame"); },
                [this]() { return tr("Apply Open Bottom-Left L-Frame"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->applyFrameOpenL();
                });
        framePageMenu->addSeparator();
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("zoom-auto"),
                [this]() { return tr("Fit to Window"); },
                [this]() { return tr("Fit graph page zoom to the current window"); },
                [this]() {
                    if (MultiLayer *ml = activeMultiLayerOrUxHint(d_app)) {
                        if (!QMetaObject::invokeMethod(ml, "fitPageZoomToViewport",
                                                       Qt::DirectConnection)) {
                            ml->setPageZoom(1.0);
                        }
                        updateStatusBarZoomIndicator(ml);
                    }
                });
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("page-setup"),
                [this]() { return tr("Size & DPI"); },
                [this]() { return tr("Configure Physical Page Dimensions, Units & DPI"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->showPageSetupDialog();
                });
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("fit-page"), [this]() { return tr("Trim Page"); },
                [this]() { return tr("Crop Page Tightly Around Plot Layers"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->fitPageToLayers();
                });
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("aspect-square"),
                [this]() { return tr("Square 70%"); },
                [this]() { return tr("Apply 70% Square Inner Canvas Frame"); },
                [this]() {
                    if (MultiLayer *ml = activeMultiLayerOrUxHint(d_app)) {
                        const QString before = ml->captureUndoSnapshot();
                        ml->applySquareCanvasFrame70Percent();
                        ml->pushUndoSnapshot(tr("Apply 70% Square Canvas Frame"), before);
                        d_app->modifiedProject();
                    }
                });
        framePageMenu->addSeparator();
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("color-palette"),
                [this]() { return tr("Palette"); },
                [this]() { return tr("Apply Publication Color Palette to Curves"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->showColorPaletteEditorDialog();
                });

        QMenu *formatBatchMenu = nullptr;
        addRibbonMenuButton(
                gStyle, QStringLiteral("format-copy"),
                [this, kCaret]() { return tr("Format & Batch") + kCaret; },
                [this]() { return tr("Copy/Paste Graph Format & Batch Plot"); }, &formatBatchMenu);
        addRibbonMenuAction(
                formatBatchMenu, QStringLiteral("format-copy"),
                [this]() { return tr("Copy Format"); },
                [this]() { return tr("Copy Visual Style & Axes Format of Active Graph"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->copyGraphFormat();
                });
        addRibbonMenuAction(
                formatBatchMenu, QStringLiteral("format-paste"),
                [this]() { return tr("Paste Format"); },
                [this]() { return tr("Paste Copied Visual Style onto Active Graph"); },
                [this]() {
                    if (activeMultiLayerOrUxHint(d_app))
                        d_app->pasteGraphFormat();
                });
        formatBatchMenu->addSeparator();
        addRibbonMenuAction(
                formatBatchMenu, QStringLiteral("batch-plot"),
                [this]() { return tr("Batch Plot"); },
                [this]() { return tr("Clone Active Graph Template Across Multiple Worksheets"); },
                [this]() {
                    if (d_app)
                        d_app->showBatchPlotDialog();
                });
        addRibbonMenuAction(
                formatBatchMenu, QStringLiteral("preset-isij"),
                [this]() { return tr("Apply Preset to Multiple Graphs..."); },
                [this]() {
                    return tr("Apply a style preset to several open graph windows at once");
                },
                [this]() {
                    if (!d_app)
                        return;
                    if (!QMetaObject::invokeMethod(
                                d_app.data(), "showApplyStylePresetToMultipleGraphsDialog",
                                Qt::DirectConnection)) {
                        d_app->showStylePresetManagerDialog();
                    }
                });
        addRibbonMenuAction(
                formatBatchMenu, QStringLiteral("page-setup"),
                [this]() { return tr("Apply Template (.qpt) to Table..."); },
                [this]() {
                    return tr("Apply a saved graph template (.qpt) to the active worksheet");
                },
                [this]() {
                    if (!d_app)
                        return;
                    if (!QMetaObject::invokeMethod(d_app.data(), "applyTemplateToActiveTable",
                                                   Qt::DirectConnection)) {
                        d_app->openTemplate();
                    }
                });
        formatBatchMenu->addSeparator();
        addRibbonMenuAction(
                formatBatchMenu, QStringLiteral("merge-graphs"),
                [this]() { return tr("Merge Graphs"); },
                [this]() { return tr("Merge Multiple Open Graph Windows into Multi-Panel Figure"); },
                [this]() {
                    if (d_app)
                        d_app->mergeOpenGraphsDialog();
                });

        row->addStretch(1);
        addTabPage([this]() { return tr("Format & Export"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 7 (index 6): Python & Tools
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gPython = nullptr;
        QHBoxLayout *gSettings = nullptr;

        createRibbonGroup(row, &gPython, true, [this]() { return tr("Python Scripts"); }, 50);
        addRibbonButton(
                gPython, QStringLiteral("python-console"),
                [this]() { return tr("Python Console"); },
                [this]() { return tr("Open Interactive In-Process Python Console"); },
                [this]() { emit openPythonConsoleRequested(); });
        addRibbonButton(
                gPython, QStringLiteral("python-run"),
                [this]() { return tr("Run Python Script"); },
                [this]() { return tr("Select and Execute a .py Automation Script"); },
                [this]() { emit runPythonScriptRequested(); });
        addRibbonButton(
                gPython, QStringLiteral("code-export"),
                [this]() { return tr("Export Python"); },
                [this]() { return tr("Generate Reproducible Python Script from Active Graph"); },
                [this]() {
                    if (d_app)
                        d_app->exportActiveGraphToPython();
                });

        createRibbonGroup(row, &gSettings, false, [this]() { return tr("Settings"); }, 60);
        addRibbonButton(
                gSettings, QStringLiteral("settings"), [this]() { return tr("Preferences"); },
                [this]() { return tr("Open Application Preferences Dialog"); },
                [this]() {
                    if (d_app)
                        d_app->showPreferencesDialog();
                });
        if (d_app) { // I10/C1-9: export / import / reset all settings (with caret)
            QMenu *settingsFileMenu = nullptr;
            addRibbonMenuButton(
                    gSettings, QStringLiteral("settings"),
                    [this, kCaret]() { return tr("Settings File") + kCaret; },
                    [this]() { return tr("Export, import or reset all settings"); },
                    &settingsFileMenu);
            for (int i = 0; i < SettingsTransfer::ActCount; ++i)
                settingsFileMenu->addAction(
                        SettingsTransfer::action(d_app, SettingsTransfer::ActionId(i)));
        }
        addRibbonButton(
                gSettings, QStringLiteral("update-check"),
                [this]() { return tr("Check for Updates"); },
                [this]() { return tr("Check GitHub Releases for New ToriGraph Versions"); },
                [this]() { emit checkForUpdatesRequested(); });
        d_btn_classic_toolbars = addRibbonButton(
                gSettings, QStringLiteral("classic-toolbars"),
                [this]() { return tr("Classic Toolbars"); },
                [this]() { return tr("Show or Hide Legacy Classic Icon Toolbars"); },
                [this]() { setClassicToolbarsVisible(!d_classic_toolbars_visible); }, true);
        d_btn_classic_toolbars->setObjectName(QStringLiteral("RibbonClassicToolbarsButton"));

        row->addStretch(1);
        addTabPage([this]() { return tr("Tools"); }, page);
    }
}

void ModernRibbonBar::moveTabPage(int from, int to)
{
    if (!d_tab_bar || !d_tab_stack || from == to || from < 0 || to < 0
        || from >= d_tab_bar->count() || to >= d_tab_bar->count())
        return;
    QWidget *page = d_tab_stack->widget(from);
    d_tab_stack->removeWidget(page);
    d_tab_stack->insertWidget(to, page);
    d_tab_title_fns.move(from, to);
    QSignalBlocker blocker(d_tab_bar);
    d_tab_bar->moveTab(from, to);
    d_tab_stack->setCurrentIndex(d_tab_bar->currentIndex());
}

void ModernRibbonBar::refreshButtonIcons()
{
    for (const RibbonButtonBinding &b : d_button_bindings) {
        if (b.button && !b.iconId.isEmpty())
            b.button->setIcon(IconLoader::duotoneIcon(b.iconId, 20, false));
    }
    for (const RibbonMenuActionBinding &ab : d_menu_action_bindings) {
        if (ab.action && !ab.iconId.isEmpty())
            ab.action->setIcon(IconLoader::duotoneIcon(ab.iconId, 18, false));
    }
    if (d_btn_cmd_palette)
        d_btn_cmd_palette->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("search-command"), 16, false));
    if (d_btn_layout_tabbed)
        d_btn_layout_tabbed->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("tab-view"), 16, false));
    if (d_btn_layout_split)
        d_btn_layout_split->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("split-view"), 16, false));
    if (d_btn_layout_mdi)
        d_btn_layout_mdi->setIcon(IconLoader::duotoneIcon(QStringLiteral("mdi-view"), 16, false));
    if (d_btn_focus_mode)
        d_btn_focus_mode->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("focus-hud"), 16, false));
    if (d_btn_lang_switch)
        d_btn_lang_switch->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("language-ja-en"), 16, false));
    if (d_btn_help_guide)
        d_btn_help_guide->setIcon(IconLoader::duotoneIcon(QStringLiteral("help"), 16, false));
    if (d_status_fit_btn)
        d_status_fit_btn->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 14, false));
    if (d_act_ribbon_send_pptx)
        d_act_ribbon_send_pptx->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("export-pptx"), 18, false));
}

void ModernRibbonBar::retranslateUi()
{
    if (d_shutting_down)
        return;

    refreshButtonIcons();

    if (d_tab_bar) {
        for (int i = 0; i < d_tab_title_fns.size() && i < d_tab_bar->count(); ++i)
            d_tab_bar->setTabText(i, escapeTabMnemonicText(d_tab_title_fns.at(i)()));
    }

    for (const RibbonButtonBinding &bb : d_button_bindings) {
        if (!bb.button)
            continue;
        if (bb.textFn)
            bb.button->setText(escapeTabMnemonicText(bb.textFn()));
        if (bb.tooltipFn) {
            QString tip = bb.tooltipFn();
            if (!bb.shortcutActionName.isEmpty() && d_app) {
                if (QAction *a = d_app->findChild<QAction *>(bb.shortcutActionName)) {
                    const QString key = a->shortcut().toString(QKeySequence::NativeText);
                    if (!key.isEmpty() && !tip.contains(key))
                        tip += QStringLiteral(" (%1)").arg(key);
                }
            }
            bb.button->setToolTip(tip);
        }
    }

    for (const RibbonMenuActionBinding &ab : d_menu_action_bindings) {
        if (!ab.action)
            continue;
        if (ab.textFn)
            ab.action->setText(escapeTabMnemonicText(ab.textFn()));
        if (ab.tooltipFn)
            ab.action->setToolTip(ab.tooltipFn());
    }

    if (d_btn_cmd_palette) {
        d_btn_cmd_palette->setText(tr("Search commands..."));
        d_btn_cmd_palette->setToolTip(tr("Open Fuzzy-Search Command Palette (Ctrl+K)"));
    }
    if (d_btn_layout_tabbed) {
        d_btn_layout_tabbed->setText(tr("Tabbed"));
        d_btn_layout_tabbed->setToolTip(tr("Tabbed Workspace View"));
    }
    if (d_btn_layout_split) {
        d_btn_layout_split->setText(tr("Split View"));
        d_btn_layout_split->setToolTip(tr("Side-by-Side Split View (40% Worksheet / 60% Graph)"));
    }
    if (d_btn_layout_mdi) {
        d_btn_layout_mdi->setText(tr("Free MDI"));
        d_btn_layout_mdi->setToolTip(tr("Free Subwindow MDI Arrangement"));
    }
    if (d_btn_focus_mode) {
        d_btn_focus_mode->setText(tr("Focus (F11)"));
        d_btn_focus_mode->setToolTip(tr("Enter Full-Canvas Focus HUD Mode (F11)"));
    }
    if (d_btn_lang_switch) {
        d_btn_lang_switch->setText(tr("Language"));
        d_btn_lang_switch->setToolTip(tr("Switch Interface Language (Japanese / English)"));
    }
    if (d_btn_help_guide) {
        d_btn_help_guide->setText(tr("Guide"));
        d_btn_help_guide->setToolTip(tr("Open Quick Guide (F1)"));
    }
    if (d_act_ribbon_send_pptx) {
        d_act_ribbon_send_pptx->setText(ToriGraphPptx::sendToPowerPointLabel());
        d_act_ribbon_send_pptx->setToolTip(ToriGraphPptx::sendToPowerPointToolTip());
    }
    if (d_status_fit_btn) {
        d_status_fit_btn->setText(tr("Fit"));
        d_status_fit_btn->setToolTip(tr("Fit graph page zoom to the current window"));
    }

    updateActiveContext(d_last_active_window.data());

    if (d_command_palette)
        d_command_palette->retranslateUi();
    if (d_focus_hud_overlay)
        d_focus_hud_overlay->retranslateUi();
    retranslateWelcomeHub();

    // C14: folded-group buttons and the minimize hint on the tab row
    registerRibbonCollapseTranslations();
    const QString caret = QString::fromUtf8(" \xE2\x96\xBE");
    for (const RibbonGroupInfo &g : d_groups) {
        if (!g.collapsedButton || !g.titleFn)
            continue;
        const QString title = g.titleFn();
        g.collapsedButton->setText(escapeTabMnemonicText(title) + caret);
        g.collapsedButton->setToolTip(
                tr("%1: the window is too narrow to show this group, so its commands are listed here")
                        .arg(title));
    }
    if (d_tab_bar)
        d_tab_bar->setToolTip(tr("Double-click a tab to minimize or restore the ribbon (Ctrl+F1)"));

    updateHeaderResponsiveness();
    updateGroupCollapse();
    updateTabStackHeight();
}

void ModernRibbonBar::syncCheckableStates()
{
    if (d_shutting_down || !d_app)
        return;

    if (d_btn_auto_recalc) {
        QSignalBlocker blocker(d_btn_auto_recalc);
        const bool isAuto = (RecalcManager::instance(d_app)->recalcMode() == RecalcManager::Auto);
        d_btn_auto_recalc->setChecked(isAuto);
    }
    if (d_act_explorer) {
        QSignalBlocker blocker(d_act_explorer);
        d_act_explorer->setChecked(!d_app->explorerWindow.isHidden());
    }
    if (d_act_results_log) {
        QSignalBlocker blocker(d_act_results_log);
        d_act_results_log->setChecked(!d_app->logWindow.isHidden());
    }
    if (d_btn_lock_aspect) {
        QSignalBlocker blocker(d_btn_lock_aspect);
        MultiLayer *activeMl =
                qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow());
        d_btn_lock_aspect->setEnabled(activeMl != nullptr);
        d_btn_lock_aspect->setChecked(activeMl && activeMl->keepAspectRatio());
    }
    if (d_btn_classic_toolbars) {
        QSignalBlocker blocker(d_btn_classic_toolbars);
        const bool anyClassicVisible =
                (d_app->file_tools && !d_app->file_tools->isHidden())
                || (d_app->edit_tools && !d_app->edit_tools->isHidden())
                || (d_app->table_tools && !d_app->table_tools->isHidden())
                || (d_app->plot_tools && !d_app->plot_tools->isHidden())
                || (d_app->graph_tools && !d_app->graph_tools->isHidden())
                || (d_app->matrix_plot_tools && !d_app->matrix_plot_tools->isHidden())
                || (d_app->graph_3D_tools && !d_app->graph_3D_tools->isHidden());
        d_classic_toolbars_visible = anyClassicVisible;
        d_btn_classic_toolbars->setChecked(d_classic_toolbars_visible);
    }

    const GraphInspectorDock *inspDock = d_app->graphInspectorDock();
    const bool inspectorChecked = inspDock && !inspDock->isHidden() && !inspDock->isCollapsedRail();
    if (d_act_inspector) {
        QSignalBlocker blocker(d_act_inspector);
        d_act_inspector->setChecked(inspectorChecked);
    }
}

Table *ModernRibbonBar::resolvedWorksheetTarget() const
{
    if (d_shutting_down || !d_app)
        return nullptr;

    if (Table *activeTbl = qobject_cast<Table *>(d_app->d_workspace.activeSubWindow())) {
        if (!d_app->hiddenWindows.contains(activeTbl) && !d_app->outWindows.contains(activeTbl)
            && activeTbl->status() != MyWidget::Hidden) {
            return activeTbl;
        }
    }

    if (d_layout_mode == LayoutSplitTableGraph) {
        auto isEligibleTable = [this](Table *tbl) -> bool {
            if (!tbl || tbl->status() == MyWidget::Hidden || tbl->isHidden())
                return false;
            if (d_app->hiddenWindows.contains(tbl) || d_app->outWindows.contains(tbl))
                return false;
            if (tbl->name().startsWith(QLatin1String("RpcFitCurve")))
                return false;
            return true;
        };

        if (isEligibleTable(d_split_table.data()))
            return d_split_table.data();

        const QList<QMdiSubWindow *> ordered =
                d_app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
        for (int i = ordered.size() - 1; i >= 0; --i) {
            if (Table *t = qobject_cast<Table *>(ordered.at(i))) {
                if (isEligibleTable(t))
                    return t;
            }
        }
    }

    return nullptr;
}

void ModernRibbonBar::pasteFromClipboardSmart()
{
    if (d_shutting_down || !d_app)
        return;

    QWidget *aw = d_app->d_workspace.activeSubWindow();
    if (qobject_cast<Matrix *>(aw) || qobject_cast<Note *>(aw)) {
        d_app->pasteSelection();
        return;
    }

    const QClipboard *cb = QApplication::clipboard();
    const QMimeData *mime = cb ? cb->mimeData() : nullptr;
    if (!mime || (!mime->hasText() && !mime->hasHtml()
                  && !mime->hasFormat(QStringLiteral("text/tab-separated-values"))
                  && !mime->hasFormat(QStringLiteral("text/csv"))
                  && !mime->hasUrls())) {
        d_app->showUxHint(
                tr("Clipboard is empty. Copy cells in Excel or TSV/CSV text first, then press Ctrl+V."),
                4500);
        return;
    }

    if (mime->hasUrls() && !mime->hasText()) {
        for (const QUrl &u : mime->urls()) {
            if (u.isLocalFile()) {
                d_app->open(u.toLocalFile());
                return;
            }
        }
    }

    Table *tbl = qobject_cast<Table *>(aw);
    if (!tbl)
        tbl = resolvedWorksheetTarget();
    if (!tbl)
        tbl = d_app->newTable();
    if (tbl) {
        if (d_app->d_workspace.activeSubWindow() != tbl)
            d_app->activateSubWindow(tbl);
        tbl->pasteSelection();
        updateEmptyWelcomeHub();
    } else {
        d_app->pasteSelection();
    }
}

void ModernRibbonBar::retranslateWelcomeHub()
{
    if (d_shutting_down || !d_welcome_hub_card)
        return;
    if (d_welcome_title_lbl)
        d_welcome_title_lbl->setText(tr("Start Analyzing Data in Seconds"));
    if (d_welcome_subtitle_lbl) {
        d_welcome_subtitle_lbl->setText(
                tr("Paste from Excel, drop a CSV/XLSX/XRD/OPJU file here, or open a sample project."));
    }
    if (d_welcome_drop_lbl) {
        d_welcome_drop_lbl->setText(
                tr("Drop CSV / XLSX / XRD / TXT / OPJU files anywhere here"));
    }
    if (d_welcome_paste_btn)
        d_welcome_paste_btn->setText(tr("Paste from Excel (Ctrl+V)"));
    if (d_welcome_open_btn)
        d_welcome_open_btn->setText(tr("Open File..."));
    if (d_welcome_recent_btn)
        d_welcome_recent_btn->setText(tr("Recent"));
    if (d_welcome_sample_btn) {
        d_welcome_sample_btn->setText(tr("Samples"));
        d_welcome_sample_btn->setToolTip(
                tr("Show 10+ Sample Templates (XRD / Raman / Tensile / Cyclic / Peak Fit)"));
    }
    if (d_welcome_new_table_btn)
        d_welcome_new_table_btn->setText(tr("New Worksheet"));
    if (d_welcome_guide_btn)
        d_welcome_guide_btn->setText(tr("Quick Guide"));
}

void ModernRibbonBar::updateEmptyWelcomeHub()
{
    if (d_shutting_down || !d_app || d_updating_welcome_hub)
        return;

    struct ReentrancyGuard {
        bool &flag;
        explicit ReentrancyGuard(bool &f) : flag(f) { flag = true; }
        ~ReentrancyGuard() { flag = false; }
    } guard(d_updating_welcome_hub);

    QWidget *vp = d_app->d_workspace.viewport();
    if (vp) {
        QWidget *emptyPane = vp->findChild<QWidget *>(QStringLiteral("emptyGraphPaneCard"));
        if (emptyPane && emptyPane != d_watched_empty_graph_pane) {
            if (d_watched_empty_graph_pane)
                d_watched_empty_graph_pane->removeEventFilter(this);
            d_watched_empty_graph_pane = emptyPane;
            emptyPane->installEventFilter(this);
        }
    }

    int visibleWindows = 0;
    if (Folder *curFolder = d_app->currentFolder()) {
        for (MyWidget *mw : curFolder->windowsList()) {
            if (!mw || mw->status() == MyWidget::Hidden)
                continue;
            if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                continue;
            if (mw->isHidden())
                continue;
            ++visibleWindows;
        }
    } else {
        for (QMdiSubWindow *sw : d_app->d_workspace.subWindowList()) {
            if (sw && !sw->isHidden())
                ++visibleWindows;
        }
    }

    const bool hasTableOnlySplit = (d_layout_mode == LayoutSplitTableGraph && d_split_table
                                    && !d_split_plot && !d_split_table->isHidden());
    if (d_watched_empty_graph_pane && (visibleWindows == 0 || hasTableOnlySplit))
        d_watched_empty_graph_pane->hide();

    const bool suppressedEnv = !vp || !vp->isVisible() || d_app->isHeadlessMode()
            || d_app->isOleEmbeddedMode() || d_app->batchMode() || d_focus_hud_mode;
    const bool showHub =
            !suppressedEnv && (visibleWindows == 0) && vp->width() >= 260 && vp->height() >= 200;

    if (!showHub) {
        if (d_welcome_hub_card)
            d_welcome_hub_card->hide();
        return;
    }

    if (!d_welcome_hub_card) {
        QFrame *card = new QFrame(vp);
        card->setObjectName(QStringLiteral("CenterWelcomeHubCard"));
        card->setAcceptDrops(true);
        card->setFocusPolicy(Qt::StrongFocus);
        card->installEventFilter(this);
        card->setStyleSheet(QStringLiteral(
                "QFrame#CenterWelcomeHubCard {"
                "  background-color: #FFFFFF;"
                "  border: 1px solid #CBD5E1;"
                "  border-radius: 14px;"
                "}"
                "QLabel#WelcomeHubTitle {"
                "  color: #0F172A;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
                "  font-size: 14pt;"
                "  font-weight: 700;"
                "}"
                "QLabel#WelcomeHubSubtitle {"
                "  color: #475569;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
                "  font-size: 10pt;"
                "}"
                "QLabel#WelcomeHubDropZone {"
                "  background-color: #F8FAFC;"
                "  color: #334155;"
                "  border: 1.5px dashed #94A3B8;"
                "  border-radius: 10px;"
                "  padding: 12px 14px;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
                "  font-size: 10pt;"
                "  font-weight: 600;"
                "}"
                "QPushButton#WelcomeHubPasteBtn {"
                "  background-color: #2563EB;"
                "  color: #FFFFFF;"
                "  border: 1px solid #1D4ED8;"
                "  border-radius: 8px;"
                "  padding: 7px 14px;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
                "  font-size: 10pt;"
                "  font-weight: 600;"
                "}"
                "QPushButton#WelcomeHubPasteBtn:hover {"
                "  background-color: #1D4ED8;"
                "}"
                "QPushButton.WelcomeHubSecondaryBtn {"
                "  background-color: #F8FAFC;"
                "  color: #0F172A;"
                "  border: 1px solid #CBD5E1;"
                "  border-radius: 8px;"
                "  padding: 7px 12px;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
                "  font-size: 10pt;"
                "  font-weight: 600;"
                "}"
                "QPushButton.WelcomeHubSecondaryBtn:hover {"
                "  background-color: #EFF6FF;"
                "  border: 1px solid #2563EB;"
                "  color: #1D4ED8;"
                "}"));

        QVBoxLayout *vlay = new QVBoxLayout(card);
        vlay->setContentsMargins(24, 22, 24, 22);
        vlay->setSpacing(12);

        d_welcome_title_lbl = new QLabel(card);
        d_welcome_title_lbl->setObjectName(QStringLiteral("WelcomeHubTitle"));
        d_welcome_title_lbl->setAlignment(Qt::AlignCenter);
        d_welcome_title_lbl->setWordWrap(true);
        vlay->addWidget(d_welcome_title_lbl);

        d_welcome_subtitle_lbl = new QLabel(card);
        d_welcome_subtitle_lbl->setObjectName(QStringLiteral("WelcomeHubSubtitle"));
        d_welcome_subtitle_lbl->setAlignment(Qt::AlignCenter);
        d_welcome_subtitle_lbl->setWordWrap(true);
        vlay->addWidget(d_welcome_subtitle_lbl);

        d_welcome_drop_lbl = new QLabel(card);
        d_welcome_drop_lbl->setObjectName(QStringLiteral("WelcomeHubDropZone"));
        d_welcome_drop_lbl->setAlignment(Qt::AlignCenter);
        d_welcome_drop_lbl->setWordWrap(true);
        vlay->addWidget(d_welcome_drop_lbl);

        QGridLayout *grid = new QGridLayout();
        grid->setSpacing(8);

        auto makeBtn = [card](const QString &iconId, bool primary) -> QPushButton * {
            QPushButton *btn = new QPushButton(card);
            btn->setIcon(IconLoader::duotoneIcon(iconId, 18, primary));
            btn->setIconSize(QSize(18, 18));
            btn->setMinimumHeight(36);
            btn->setCursor(Qt::PointingHandCursor);
            if (!primary)
                btn->setProperty("class", QStringLiteral("WelcomeHubSecondaryBtn"));
            return btn;
        };

        d_welcome_paste_btn = makeBtn(QStringLiteral("paste"), true);
        d_welcome_paste_btn->setObjectName(QStringLiteral("WelcomeHubPasteBtn"));
        connect(d_welcome_paste_btn, &QPushButton::clicked, this,
                &ModernRibbonBar::pasteFromClipboardSmart);
        grid->addWidget(d_welcome_paste_btn, 0, 0, 1, 2);

        d_welcome_open_btn = makeBtn(QStringLiteral("file-open"), false);
        connect(d_welcome_open_btn, &QPushButton::clicked, this, [this]() {
            if (d_app)
                d_app->open();
        });
        grid->addWidget(d_welcome_open_btn, 1, 0);

        d_welcome_recent_btn = makeBtn(QStringLiteral("folder-open"), false);
        d_welcome_recent_menu = new QMenu(d_welcome_recent_btn);
        d_welcome_recent_menu->setProperty("class", QStringLiteral("RibbonDropdownMenu"));
        connect(d_welcome_recent_menu, &QMenu::aboutToShow, this, [this]() {
            if (!d_welcome_recent_menu || !d_app)
                return;
            d_welcome_recent_menu->clear();
            int added = 0;
            for (const QString &fn : d_app->recentProjects) {
                if (fn.trimmed().isEmpty())
                    continue;
                const QFileInfo fi(fn);
                QAction *act = d_welcome_recent_menu->addAction(
                        fi.fileName().isEmpty() ? fn : fi.fileName(), this,
                        [this, fn]() {
                            if (d_app)
                                d_app->open(fn);
                        });
                act->setToolTip(fn);
                if (++added >= 10)
                    break;
            }
            if (added == 0) {
                QAction *emptyAct = d_welcome_recent_menu->addAction(tr("No recent files"));
                emptyAct->setEnabled(false);
            }
        });
        d_welcome_recent_btn->setMenu(d_welcome_recent_menu);
        grid->addWidget(d_welcome_recent_btn, 1, 1);

        d_welcome_sample_btn = makeBtn(QStringLiteral("sample-gallery"), false);
        connect(d_welcome_sample_btn, &QPushButton::clicked, this, [this]() {
            if (d_app)
                CurveMathTools::showSampleGalleryDialog(d_app);
        });
        grid->addWidget(d_welcome_sample_btn, 2, 0);

        d_welcome_new_table_btn = makeBtn(QStringLiteral("new-table"), false);
        connect(d_welcome_new_table_btn, &QPushButton::clicked, this, [this]() {
            if (d_app)
                d_app->newTable();
        });
        grid->addWidget(d_welcome_new_table_btn, 2, 1);

        d_welcome_guide_btn = makeBtn(QStringLiteral("help"), false);
        connect(d_welcome_guide_btn, &QPushButton::clicked, this, [this]() {
            if (!d_app)
                return;
            if (QAction *a = d_app->findChild<QAction *>(QStringLiteral("actionShowHelp")))
                a->trigger();
            else
                QMetaObject::invokeMethod(d_app.data(), "showHelp", Qt::DirectConnection);
        });
        grid->addWidget(d_welcome_guide_btn, 3, 0, 1, 2);

        vlay->addLayout(grid);
        d_welcome_hub_card = card;
        retranslateWelcomeHub();
    }

    const int cardW = qBound(240, qMin(520, vp->width() - 32), qMax(240, vp->width() - 16));
    if (d_welcome_hub_card->width() != cardW) {
        d_welcome_hub_card->setFixedWidth(cardW);
        d_welcome_hub_card->adjustSize();
    }
    const int cardH = qMin(qMax(220, vp->height() - 24),
                           qMax(280, d_welcome_hub_card->sizeHint().height()));
    const int x = qMax(8, (vp->width() - cardW) / 2);
    const int y = qMax(8, (vp->height() - cardH) / 2);
    const QRect wantRect(x, y, cardW, cardH);
    if (d_welcome_hub_card->geometry() != wantRect)
        d_welcome_hub_card->setGeometry(wantRect);
    if (d_welcome_hub_card->isHidden()) {
        d_welcome_hub_card->show();
        d_welcome_hub_card->raise();
    }
}

void ModernRibbonBar::updateActiveContext(MyWidget *activeWindow)
{
    if (d_shutting_down)
        return;

    d_last_active_window = activeWindow;

    if (d_context_pill) {
        QString label;
        QString pillStyle;
        if (qobject_cast<Table *>(activeWindow)) {
            label = tr("Worksheet");
            pillStyle = QStringLiteral(
                    "background-color: #DBEAFE; color: #1D4ED8; border: 1px solid #93C5FD;");
        } else if (qobject_cast<MultiLayer *>(activeWindow)) {
            label = tr("2D Graph");
            pillStyle = QStringLiteral(
                    "background-color: #E0E7FF; color: #3730A3; border: 1px solid #A5B4FC;");
        } else if (qobject_cast<Graph3D *>(activeWindow)) {
            label = tr("3D Graph");
            pillStyle = QStringLiteral(
                    "background-color: #EDE9FE; color: #5B21B6; border: 1px solid #C4B5FD;");
        } else if (qobject_cast<Matrix *>(activeWindow)) {
            label = tr("Matrix");
            pillStyle = QStringLiteral(
                    "background-color: #D1FAE5; color: #065F46; border: 1px solid #6EE7B7;");
        } else if (qobject_cast<Note *>(activeWindow)) {
            label = tr("Notes");
            pillStyle = QStringLiteral(
                    "background-color: #FEF3C7; color: #92400E; border: 1px solid #FCD34D;");
        } else {
            label = tr("Project");
            pillStyle = QStringLiteral(
                    "background-color: #E2E8F0; color: #334155; border: 1px solid #CBD5E1;");
        }

        d_context_pill->setText(label);
        d_context_pill->setStyleSheet(
                QStringLiteral("QLabel#RibbonContextBadge { %1 border-radius: 10px; "
                               "padding: 2px 10px; font-family: 'Yu Gothic UI', 'Segoe UI', "
                               "'Meiryo UI', sans-serif; font-size: 10pt; font-weight: 700; }")
                        .arg(pillStyle));
        updateHeaderResponsiveness();
    }

    if (d_app) {
        if (QStatusBar *sb = d_app->statusBar()) {
            if (QToolButton *cmdHint =
                        sb->findChild<QToolButton *>(QStringLiteral("StatusBarCommandHintBtn"))) {
                sb->removeWidget(cmdHint);
                cmdHint->hide();
                cmdHint->deleteLater();
            }
        }
    }

    Table *activeTable = qobject_cast<Table *>(activeWindow);
    Table *wsTarget = activeTable ? activeTable : resolvedWorksheetTarget();
    MultiLayer *activeMl = qobject_cast<MultiLayer *>(activeWindow);
    const bool hasGraph = (activeMl != nullptr || qobject_cast<Graph3D *>(activeWindow) != nullptr);

    if (activeMl) {
        if (Table *srcTbl = sourceTableFromMultiLayer(activeMl))
            ToriUx::dismissPlotThisDataBar(srcTbl);
    }

    // Gate Table actions to active Table OR visible paired Split Table.
    if (d_act_add_column)
        d_act_add_column->setEnabled(wsTarget != nullptr);
    for (const QPointer<QWidget> &w : d_table_context_widgets) {
        if (w)
            w->setEnabled(wsTarget != nullptr);
    }
    if (d_btn_ws_reimport)
        d_btn_ws_reimport->setEnabled(wsTarget != nullptr && wsTarget->hasImportSource());

    // A1-8: Disable Copy for PowerPoint when no graph window is active, while keeping the
    // double-click re-editing hint in the tooltip.
    const QString copyOleTip = hasGraph
            ? ToriGraphOleUi::copyForPowerPointToolTip()
            : (ToriGraphOleUi::copyForPowerPointToolTip() + QStringLiteral("\n")
               + tr("Create or select a graph window first to copy for PowerPoint."));
    if (d_app) {
        if (QAction *aCopyOle = d_app->findChild<QAction *>(QStringLiteral("actionCopyGraphOle"))) {
            aCopyOle->setEnabled(hasGraph);
            aCopyOle->setToolTip(copyOleTip);
        }
    }
    if (d_btn_export_copy_ole) {
        d_btn_export_copy_ole->setEnabled(hasGraph);
        d_btn_export_copy_ole->setToolTip(copyOleTip);
    }

    updateStatusBarZoomIndicator(activeMl);

    syncActionBoundButtons();
    syncCheckableStates();
    if (d_focus_hud_overlay)
        d_focus_hud_overlay->refreshActiveContext();
    updateEmptyWelcomeHub();
}

void ModernRibbonBar::setWorkspaceLayoutMode(WorkspaceLayoutMode mode)
{
    if (d_shutting_down)
        return;

    d_layout_mode = mode;

    if (d_layout_btn_group) {
        if (QAbstractButton *btn = d_layout_btn_group->button(int(mode))) {
            QSignalBlocker blocker(d_layout_btn_group);
            btn->setChecked(true);
        }
    }

    if (d_app) {
        d_updating_layout_mode = true;
        auto unmaximizeCurrentFolderWindows = [this]() {
            if (Folder *curFolder = d_app->currentFolder()) {
                const QList<MyWidget *> folderWins = curFolder->windowsList();
                for (MyWidget *mw : folderWins) {
                    if (!mw || mw->status() == MyWidget::Hidden)
                        continue;
                    if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                        continue;
                    if (mw->isHidden())
                        continue;
                    if (mw->isMaximized()) {
                        mw->setWindowState(mw->windowState() & ~Qt::WindowMaximized);
                        mw->showNormal();
                    }
                }
            }
        };

        switch (mode) {
        case LayoutTabbed: {
            restoreBackgroundMdiWindowsAfterFocus(d_app);
            d_app->setTabbedWorkspaceView(true);
            d_app->d_workspace.setTabsClosable(true);
            d_app->d_workspace.setTabsMovable(true);
            if (QTabBar *tb = d_app->d_workspace.findChild<QTabBar *>()) {
                const QList<QMdiSubWindow *> creationOrder =
                        d_app->d_workspace.subWindowList(QMdiArea::CreationOrder);
                for (int i = 0; i < tb->count(); ++i) {
                    MyWidget *mw = (i < creationOrder.size())
                            ? qobject_cast<MyWidget *>(creationOrder.at(i))
                            : nullptr;
                    const bool hideTab = mw
                            && (d_app->hiddenWindows.contains(mw)
                                || d_app->outWindows.contains(mw)
                                || mw->status() == MyWidget::Hidden);
                    tb->setTabVisible(i, !hideTab);
                }
            }
            if (QMdiSubWindow *activeSw = d_app->d_workspace.activeSubWindow()) {
                if (MyWidget *mw = qobject_cast<MyWidget *>(activeSw)) {
                    if (!mw->isHidden() && mw->status() != MyWidget::Hidden
                        && !d_app->hiddenWindows.contains(mw)
                        && !d_app->outWindows.contains(mw) && !mw->isMaximized()) {
                        mw->showMaximized();
                    }
                }
            }
            break;
        }

        case LayoutSplitTableGraph:
            restoreBackgroundMdiWindowsAfterFocus(d_app);
            d_app->setTabbedWorkspaceView(false);
            unmaximizeCurrentFolderWindows();
            applySplitWorkspaceGeometry();
            break;

        case LayoutFreeMdi:
            restoreBackgroundMdiWindowsAfterFocus(d_app);
            d_app->setTabbedWorkspaceView(false);
            unmaximizeCurrentFolderWindows();
            if (QScrollBar *hsb = d_app->d_workspace.horizontalScrollBar())
                hsb->setValue(0);
            if (QScrollBar *vsb = d_app->d_workspace.verticalScrollBar())
                vsb->setValue(0);
            if (Folder *curFolder = d_app->currentFolder()) {
                const bool focusActive = d_focus_hud_mode
                        || (d_focus_hud_overlay && d_focus_hud_overlay->isOverlayActive());
                const int minTopY = focusActive ? 44 : 8;
                int cascadeIdx = 0;
                for (MyWidget *mw : curFolder->windowsList()) {
                    if (!mw || mw->status() == MyWidget::Hidden || mw->isHidden())
                        continue;
                    if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                        continue;
                    if (mw->x() < 8 || mw->y() < minTopY) {
                        const int offX = 12 + (cascadeIdx % 6) * 28;
                        const int offY = minTopY + (cascadeIdx % 6) * 22;
                        mw->move(qMax(offX, mw->x()), qMax(offY, mw->y()));
                        ++cascadeIdx;
                    }
                }
            }
            if (d_focus_hud_mode)
                hideBackgroundMdiWindowsForFocus(d_app);
            if (QScrollBar *hsb = d_app->d_workspace.horizontalScrollBar())
                hsb->setValue(0);
            if (QScrollBar *vsb = d_app->d_workspace.verticalScrollBar())
                vsb->setValue(0);
            break;
        }
        d_updating_layout_mode = false;
    }

    if (d_focus_hud_overlay && d_focus_hud_mode)
        d_focus_hud_overlay->repositionOverlayWidgets();
    updateEmptyWelcomeHub();

    emit workspaceLayoutModeChanged(mode);
}

void ModernRibbonBar::syncWorkspaceModeButtons(WorkspaceLayoutMode mode)
{
    if (d_shutting_down || d_updating_layout_mode || d_applying_split_geometry)
        return;

    d_layout_mode = mode;
    if (d_layout_btn_group) {
        if (QAbstractButton *btn = d_layout_btn_group->button(int(mode))) {
            QSignalBlocker blocker(d_layout_btn_group);
            btn->setChecked(true);
        }
    }
}

void ModernRibbonBar::syncWorkspaceModeButtons(bool tabbed)
{
    if (d_shutting_down || d_updating_layout_mode || d_applying_split_geometry)
        return;

    if (tabbed) {
        syncWorkspaceModeButtons(LayoutTabbed);
    } else if (d_layout_mode == LayoutTabbed) {
        syncWorkspaceModeButtons(LayoutFreeMdi);
    }
}

bool ModernRibbonBar::setWorkspaceLayoutModeByName(const QString &modeName)
{
    const QString m = modeName.trimmed().toLower();
    if (m == QLatin1String("tabbed") || m == QLatin1String("tabs")) {
        setWorkspaceLayoutMode(LayoutTabbed);
        return true;
    }
    if (m == QLatin1String("split") || m == QLatin1String("split_table_graph")
        || m == QLatin1String("split-view")) {
        setWorkspaceLayoutMode(LayoutSplitTableGraph);
        return true;
    }
    if (m == QLatin1String("mdi") || m == QLatin1String("free") || m == QLatin1String("free_mdi")
        || m == QLatin1String("subwindow")) {
        setWorkspaceLayoutMode(LayoutFreeMdi);
        return true;
    }
    return false;
}

QString ModernRibbonBar::workspaceLayoutModeName() const
{
    switch (d_layout_mode) {
    case LayoutTabbed:
        return QStringLiteral("tabbed");
    case LayoutSplitTableGraph:
        return QStringLiteral("split");
    case LayoutFreeMdi:
        return QStringLiteral("mdi");
    }
    return QStringLiteral("tabbed");
}

void ModernRibbonBar::setSplitLeftRatio(double ratio)
{
    if (!std::isfinite(ratio))
        return;
    const double clamped = qBound(0.15, ratio, 0.85);
    if (qFuzzyCompare(d_split_left_ratio, clamped))
        return;
    d_split_left_ratio = clamped;
    if (d_layout_mode == LayoutSplitTableGraph && !d_applying_split_geometry)
        applySplitWorkspaceGeometry();
}

void ModernRibbonBar::applySplitWorkspaceGeometry()
{
    if (d_shutting_down || !d_app || d_applying_split_geometry)
        return;

    d_applying_split_geometry = true;

    if (d_app->d_workspace.viewMode() != QMdiArea::SubWindowView)
        d_app->setTabbedWorkspaceView(false);

    if (d_app->layout())
        d_app->layout()->activate();

    QWidget *vp = d_app->d_workspace.viewport();
    QRect vpRect = vp ? vp->rect() : d_app->d_workspace.rect();
    if (vpRect.width() < 120 || vpRect.height() < 80) {
        const QSize fallback = d_app->size().expandedTo(QSize(800, 560));
        vpRect = QRect(0, 0, qMax(320, fallback.width() - 16), qMax(240, fallback.height() - 110));
    }

    QSet<MyWidget *> eligibleWins;
    if (Folder *curFolder = d_app->currentFolder()) {
        const QList<MyWidget *> folderWins = curFolder->windowsList();
        for (MyWidget *mw : folderWins) {
            if (!mw || mw->status() == MyWidget::Hidden)
                continue;
            if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                continue;
            if (mw->isHidden())
                continue;
            eligibleWins.insert(mw);
        }
    }

    QMdiSubWindow *activeSub = d_app->d_workspace.activeSubWindow();
    MyWidget *activeMw = qobject_cast<MyWidget *>(activeSub);
    if (activeMw && !eligibleWins.contains(activeMw))
        activeMw = nullptr;

    const QList<QMdiSubWindow *> ordered =
            d_app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);

    Table *targetTable = qobject_cast<Table *>(activeMw);
    MyWidget *targetPlot = qobject_cast<MultiLayer *>(activeMw);
    if (!targetPlot)
        targetPlot = qobject_cast<Graph3D *>(activeMw);

    if (!targetTable && targetPlot) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(targetPlot)) {
            QSet<QObject *> ignored;
            for (MyWidget *mw : d_app->hiddenWindows) {
                if (mw && !isWorkbookManagedSheet(d_app, mw))
                    ignored.insert(mw);
            }
            if (Table *srcTbl = sourceTableFromMultiLayer(ml, ignored)) {
                if (eligibleWins.contains(srcTbl)) {
                    targetTable = srcTbl;
                } else if (isWorkbookManagedSheet(d_app, srcTbl)) {
                    if (WorkbookManager *wb = WorkbookManager::existing(d_app)) {
                        const QString book = wb->bookOf(srcTbl);
                        if (Table *prevCur = wb->currentSheet(book))
                            eligibleWins.remove(prevCur);
                        wb->setCurrentSheet(srcTbl, false);
                    }
                    eligibleWins.insert(srcTbl);
                    targetTable = srcTbl;
                }
            }
        }
    }

    if (!targetTable) {
        for (int i = ordered.size() - 1; i >= 0; --i) {
            if (Table *t = qobject_cast<Table *>(ordered.at(i))) {
                if (eligibleWins.contains(t)) {
                    targetTable = t;
                    break;
                }
            }
        }
    }

    if (!targetPlot && targetTable) {
        for (int i = ordered.size() - 1; i >= 0; --i) {
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(ordered.at(i))) {
                if (eligibleWins.contains(ml) && multiLayerUsesTable(ml, targetTable)) {
                    targetPlot = ml;
                    break;
                }
            }
        }
    }

    if (!targetPlot) {
        for (int i = ordered.size() - 1; i >= 0; --i) {
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(ordered.at(i))) {
                if (eligibleWins.contains(ml)) {
                    targetPlot = ml;
                    break;
                }
            }
            if (Graph3D *g3 = qobject_cast<Graph3D *>(ordered.at(i))) {
                if (eligibleWins.contains(g3)) {
                    targetPlot = g3;
                    break;
                }
            }
        }
    }

    auto prepareTileWindow = [](MyWidget *w) {
        if (!w || w->isHidden())
            return;
        if (w->isMaximized() || w->isMinimized()) {
            w->setWindowState(w->windowState() & ~(Qt::WindowMaximized | Qt::WindowMinimized));
            w->showNormal();
        }
    };

    const bool focusHudActive =
            d_focus_hud_mode || (d_focus_hud_overlay && d_focus_hud_overlay->isOverlayActive());

    for (MyWidget *mw : eligibleWins) {
        if (mw && mw != targetTable && mw != targetPlot) {
            if (mw->isMaximized())
                prepareTileWindow(mw);
            // 05_focus_hud.png fix: hide non-split background subwindows (e.g. Table1 behind
            // Table2) while F11 Focus HUD is active so they never bleed through above tileTopY.
            if (focusHudActive) {
                mw->setProperty("_tg_hidden_for_focus_mdi", true);
                mw->hide();
            }
        }
    }

    const int tileTopY = focusHudActive ? 48 : 0;
    const double splitRatio = qBound(0.15, d_split_left_ratio, 0.85);
    const int minPaneW = (vpRect.width() < 1000) ? qMax(80, qMin(120, vpRect.width() / 4)) : 120;
    const int leftW = qBound(minPaneW, qRound(vpRect.width() * splitRatio),
                             qMax(minPaneW, vpRect.width() - minPaneW));
    const int fullW = qMax(minPaneW, vpRect.width());
    const int fullH = qMax(100, vpRect.height() - tileTopY);

    if (d_split_table && d_split_table != targetTable)
        d_split_table->removeEventFilter(this);
    if (d_split_plot && d_split_plot != targetPlot)
        d_split_plot->removeEventFilter(this);
    d_split_table = targetTable;
    d_split_plot = targetPlot;
    if (d_split_table) {
        d_split_table->removeEventFilter(this);
        d_split_table->installEventFilter(this);
    }
    if (d_split_plot) {
        d_split_plot->removeEventFilter(this);
        d_split_plot->installEventFilter(this);
    }

    if (QScrollBar *hsb = d_app->d_workspace.horizontalScrollBar())
        hsb->setValue(0);
    if (QScrollBar *vsb = d_app->d_workspace.verticalScrollBar())
        vsb->setValue(0);

    if (targetTable && targetPlot) {
        prepareTileWindow(targetTable);
        prepareTileWindow(targetPlot);
        if (targetTable->minimumWidth() > leftW)
            targetTable->setMinimumWidth(leftW);
        targetTable->setGeometry(0, tileTopY, leftW, fullH);
        const int actualLeftW = qBound(minPaneW, targetTable->geometry().width(),
                                       qMax(minPaneW, vpRect.width() - minPaneW));
        if (actualLeftW != targetTable->geometry().width())
            targetTable->setGeometry(0, tileTopY, actualLeftW, fullH);
        const int actualRightW = qMax(minPaneW, vpRect.width() - actualLeftW);
        if (targetPlot->minimumWidth() > actualRightW)
            targetPlot->setMinimumWidth(actualRightW);
        targetPlot->setGeometry(actualLeftW, tileTopY, actualRightW, fullH);
        if (activeMw == targetTable) {
            targetPlot->raise();
            targetTable->raise();
        } else {
            targetTable->raise();
            targetPlot->raise();
        }
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(targetPlot)) {
            if (multiLayerUsesTable(ml, targetTable))
                ToriUx::dismissPlotThisDataBar(targetTable);
        }
    } else if (targetTable) {
        prepareTileWindow(targetTable);
        if (targetTable->minimumWidth() > fullW)
            targetTable->setMinimumWidth(fullW);
        targetTable->setGeometry(0, tileTopY, fullW, fullH);
        targetTable->raise();
        for (MyWidget *mw : eligibleWins) {
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(mw)) {
                if (multiLayerUsesTable(ml, targetTable)) {
                    ToriUx::dismissPlotThisDataBar(targetTable);
                    break;
                }
            }
        }
    } else if (targetPlot) {
        prepareTileWindow(targetPlot);
        if (targetPlot->minimumWidth() > fullW)
            targetPlot->setMinimumWidth(fullW);
        targetPlot->setGeometry(0, tileTopY, fullW, fullH);
        targetPlot->raise();
    }

    if (activeMw && activeMw != targetTable && activeMw != targetPlot) {
        prepareTileWindow(activeMw);
        if (activeMw->width() < 120 || activeMw->height() < 90) {
            const int fw = qBound(220, int(vpRect.width() * 0.55), qMax(220, vpRect.width() - 32));
            const int fh = qBound(160, int(fullH * 0.60), qMax(160, fullH - 32));
            activeMw->setGeometry(qMax(16, (vpRect.width() - fw) / 2),
                                  qMax(tileTopY + 12, tileTopY + (fullH - fh) / 2), fw, fh);
        } else if (focusHudActive && activeMw->y() < tileTopY) {
            activeMw->move(activeMw->x(), tileTopY);
        }
        activeMw->raise();
    }

    d_applying_split_geometry = false;

    Table *wsTarget = resolvedWorksheetTarget();
    if (d_act_add_column)
        d_act_add_column->setEnabled(wsTarget != nullptr);
    for (const QPointer<QWidget> &w : d_table_context_widgets) {
        if (w)
            w->setEnabled(wsTarget != nullptr);
    }
    if (d_btn_ws_reimport)
        d_btn_ws_reimport->setEnabled(wsTarget != nullptr && wsTarget->hasImportSource());

    if (d_focus_hud_overlay && focusHudActive)
        d_focus_hud_overlay->repositionOverlayWidgets();
    updateEmptyWelcomeHub();
}

void ModernRibbonBar::setFocusHudMode(bool enabled)
{
    if (d_shutting_down)
        return;

    if (d_focus_hud_mode == enabled)
        return;

    d_focus_hud_mode = enabled;

    if (enabled) {
        d_saved_focus_visibility.clear();

        if (d_app) {
            QToolBar *ribbonTb = d_app->modernRibbonToolBar();
            const QList<QToolBar *> toolbars = d_app->findChildren<QToolBar *>();
            for (QToolBar *tb : toolbars) {
                if (!tb || tb == ribbonTb
                    || tb->objectName() == QStringLiteral("ModernRibbonToolBar"))
                    continue;
                d_saved_focus_visibility.append(qMakePair(QPointer<QWidget>(tb), !tb->isHidden()));
                tb->hide();
            }

            const QList<QDockWidget *> docks = d_app->findChildren<QDockWidget *>();
            for (QDockWidget *dw : docks) {
                if (!dw)
                    continue;
                d_saved_focus_visibility.append(qMakePair(QPointer<QWidget>(dw), !dw->isHidden()));
                dw->hide();
            }

            if (QStatusBar *sb = d_app->statusBar()) {
                d_saved_focus_visibility.append(qMakePair(QPointer<QWidget>(sb), !sb->isHidden()));
                sb->hide();
            }

            if (QMenuBar *mb = d_app->menuBar()) {
                d_saved_focus_visibility.append(qMakePair(QPointer<QWidget>(mb), !mb->isHidden()));
                mb->hide();
            }
        }

        if (QToolBar *rtb = d_app ? d_app->modernRibbonToolBar() : nullptr)
            rtb->setVisible(!enabled);
        this->setVisible(!enabled);

        if (d_app) {
            if (QWidget *cw = d_app->centralWidget()) {
                if (cw->layout())
                    cw->layout()->activate();
            }
            if (d_app->layout())
                d_app->layout()->activate();
        }

        if (d_focus_hud_overlay)
            d_focus_hud_overlay->setOverlayActive(true);
    } else {
        if (d_focus_hud_overlay)
            d_focus_hud_overlay->setOverlayActive(false);

        if (QToolBar *rtb = d_app ? d_app->modernRibbonToolBar() : nullptr)
            rtb->setVisible(!enabled);
        this->setVisible(!enabled);

        for (const auto &saved : d_saved_focus_visibility) {
            if (saved.first)
                saved.first->setVisible(saved.second);
        }
        d_saved_focus_visibility.clear();

        if (d_app) {
            if (QWidget *cw = d_app->centralWidget()) {
                if (cw->layout())
                    cw->layout()->activate();
            }
            if (d_app->layout())
                d_app->layout()->activate();
        }

        syncCheckableStates();
    }

    if (!enabled && d_app)
        restoreBackgroundMdiWindowsAfterFocus(d_app);

    if (d_layout_mode == LayoutSplitTableGraph) {
        applySplitWorkspaceGeometry();
    } else if (enabled && d_layout_mode == LayoutFreeMdi && d_app) {
        hideBackgroundMdiWindowsForFocus(d_app);
        if (Folder *curFolder = d_app->currentFolder()) {
            for (MyWidget *mw : curFolder->windowsList()) {
                if (!mw || mw->status() == MyWidget::Hidden || mw->isHidden())
                    continue;
                if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                    continue;
                if (!mw->isMaximized() && mw->y() < 48)
                    mw->move(mw->x(), 48);
            }
        }
        if (d_focus_hud_overlay)
            d_focus_hud_overlay->repositionOverlayWidgets();
    }

    updateEmptyWelcomeHub();
    emit focusHudModeChanged(d_focus_hud_mode);
}

void ModernRibbonBar::setClassicToolbarsVisible(bool visible)
{
    if (d_shutting_down)
        return;

    d_classic_toolbars_visible = visible;
    if (d_btn_classic_toolbars && d_btn_classic_toolbars->isChecked() != visible) {
        QSignalBlocker blocker(d_btn_classic_toolbars);
        d_btn_classic_toolbars->setChecked(visible);
    }

    if (d_app && d_focus_hud_mode && !visible) {
        // C15: the focus HUD hid every toolbar and restores the saved states when it closes;
        // make that restore keep the classic toolbars closed (their switches may be hidden).
        const QToolBar *const legacyBars[] = { d_app->file_tools,  d_app->edit_tools,
                                               d_app->table_tools, d_app->plot_tools,
                                               d_app->graph_tools, d_app->matrix_plot_tools,
                                               d_app->graph_3D_tools };
        for (auto &saved : d_saved_focus_visibility) {
            for (const QToolBar *tb : legacyBars) {
                if (tb && saved.first.data() == tb)
                    saved.second = false;
            }
        }
    }
    if (d_app && !d_focus_hud_mode) {
        QToolBar *legacyBars[] = { d_app->file_tools,        d_app->edit_tools,
                                   d_app->table_tools,       d_app->plot_tools,
                                   d_app->graph_tools,       d_app->matrix_plot_tools,
                                   d_app->graph_3D_tools };
        if (!visible) {
            for (QToolBar *tb : legacyBars) {
                if (tb)
                    tb->hide();
            }
        } else {
            if (d_app->file_tools) {
                if (QToolBar *rtb = d_app->modernRibbonToolBar())
                    d_app->insertToolBar(d_app->file_tools, rtb);
                d_app->insertToolBarBreak(d_app->file_tools);
                d_app->file_tools->show();
            }
            if (d_app->edit_tools)
                d_app->edit_tools->show();
            MyWidget *aw = qobject_cast<MyWidget *>(d_app->d_workspace.activeSubWindow());
            d_app->customToolBars(aw);
            if (qobject_cast<Table *>(aw)) {
                if (d_app->table_tools)
                    d_app->table_tools->show();
                if (d_app->plot_tools)
                    d_app->plot_tools->show();
            } else if (qobject_cast<MultiLayer *>(aw)) {
                if (d_app->graph_tools)
                    d_app->graph_tools->show();
                if (d_app->plot_tools)
                    d_app->plot_tools->show();
            } else if (qobject_cast<Matrix *>(aw)) {
                if (d_app->matrix_plot_tools)
                    d_app->matrix_plot_tools->show();
            } else if (qobject_cast<Graph3D *>(aw)) {
                if (d_app->graph_3D_tools)
                    d_app->graph_3D_tools->show();
            }
        }
    }

    emit classicToolbarsToggled(visible);
}

QToolButton *ModernRibbonBar::addActionButton(QHBoxLayout *groupLayout, const QString &iconId,
                                              std::function<QString()> textFn,
                                              std::function<QString()> tooltipFn,
                                              const QString &actionObjectName, bool checkable)
{
    QToolButton *btn = addRibbonButton(
            groupLayout, iconId, std::move(textFn), std::move(tooltipFn),
            [this, actionObjectName]() {
                QAction *a = d_app ? d_app->findChild<QAction *>(actionObjectName) : nullptr;
                if (a && a->isEnabled())
                    a->trigger();
                syncActionBoundButtons();
            },
            checkable);
    d_action_buttons.insert(actionObjectName, btn);
    setShortcutHintAction(btn, actionObjectName);
    if (QAction *a = d_app ? d_app->findChild<QAction *>(actionObjectName) : nullptr) {
        // C4: the button follows its QAction (enabled for the active window, checked mode).
        connect(a, &QAction::changed, this, [this]() { syncActionBoundButtons(); });
        connect(a, &QAction::toggled, this, [this](bool) { syncActionBoundButtons(); });
    }
    return btn;
}

void ModernRibbonBar::setShortcutHintAction(QToolButton *button, const QString &actionObjectName)
{
    for (RibbonButtonBinding &bb : d_button_bindings) {
        if (bb.button == button) {
            bb.shortcutActionName = actionObjectName;
            break;
        }
    }
}

QToolButton *ModernRibbonBar::actionBoundButton(const QString &actionObjectName) const
{
    return d_action_buttons.value(actionObjectName).data();
}

void ModernRibbonBar::syncActionBoundButtons()
{
    if (d_shutting_down || !d_app)
        return;
    for (auto it = d_action_buttons.constBegin(); it != d_action_buttons.constEnd(); ++it) {
        QToolButton *btn = it.value().data();
        QAction *a = d_app->findChild<QAction *>(it.key());
        if (!btn || !a)
            continue;
        if (btn->isEnabled() != a->isEnabled())
            btn->setEnabled(a->isEnabled());
        if (btn->isCheckable() && btn->isChecked() != a->isChecked()) {
            QSignalBlocker blocker(btn);
            btn->setChecked(a->isChecked());
        }
        const bool syncTip = (it.key() == QLatin1String("actionCopyGraphOle")
                              || it.key() == QLatin1String("actionUndo")
                              || it.key() == QLatin1String("actionRedo"));
        if (syncTip && !a->toolTip().isEmpty() && btn->toolTip() != a->toolTip())
            btn->setToolTip(a->toolTip());
    }
}

int ModernRibbonBar::tabIndexOfPage(const QWidget *pageContainer) const
{
    if (!d_tab_stack || !pageContainer)
        return -1;
    for (int i = 0; i < d_tab_stack->count(); ++i) {
        if (QScrollArea *sa = qobject_cast<QScrollArea *>(d_tab_stack->widget(i))) {
            if (sa->widget() == pageContainer)
                return i;
        }
    }
    return -1;
}

QStringList ModernRibbonBar::groupTitles(int tabIndex) const
{
    QStringList out;
    for (const RibbonGroupInfo &g : d_groups) {
        if (g.titleFn && tabIndexOfPage(g.page) == tabIndex)
            out << g.titleFn();
    }
    return out;
}

QStringList ModernRibbonBar::collapsedGroupTitles(int tabIndex) const
{
    QStringList out;
    for (const RibbonGroupInfo &g : d_groups) {
        if (g.titleFn && g.collapsed && tabIndexOfPage(g.page) == tabIndex)
            out << g.titleFn();
    }
    return out;
}

QToolButton *ModernRibbonBar::collapsedGroupButton(int tabIndex, const QString &groupTitle) const
{
    for (const RibbonGroupInfo &g : d_groups) {
        if (g.titleFn && tabIndexOfPage(g.page) == tabIndex && g.titleFn() == groupTitle)
            return g.collapsedButton.data();
    }
    return nullptr;
}

void ModernRibbonBar::populateCollapsedGroupMenu(int groupIndex)
{
    if (groupIndex < 0 || groupIndex >= d_groups.size())
        return;
    const RibbonGroupInfo &g = d_groups.at(groupIndex);
    QToolButton *folded = g.collapsedButton.data();
    QMenu *menu = folded ? folded->menu() : nullptr;
    if (!menu || !g.group || !g.group->layout())
        return;
    // The sub-menus belong to the group's own buttons; clear() only detaches them.
    menu->clear();
    const QString caret = QString::fromUtf8(" \xE2\x96\xBE");
    QLayout *lay = g.group->layout();
    for (int i = 0; i < lay->count(); ++i) {
        QToolButton *b = qobject_cast<QToolButton *>(lay->itemAt(i)->widget());
        if (!b || b->isHidden())
            continue;
        QString text = b->text();
        text.remove(caret);
        text = text.trimmed();
        if (QMenu *sub = b->menu()) {
            sub->setTitle(text);
            sub->setIcon(b->icon());
            QAction *ma = menu->addMenu(sub);
            ma->setToolTip(b->toolTip());
            ma->setEnabled(b->isEnabled());
        } else {
            QAction *a = menu->addAction(b->icon(), text);
            a->setToolTip(b->toolTip());
            a->setEnabled(b->isEnabled());
            if (b->isCheckable()) {
                a->setCheckable(true);
                a->setChecked(b->isChecked());
            }
            QPointer<QToolButton> pb(b);
            connect(a, &QAction::triggered, this, [pb]() {
                if (pb && pb->isEnabled())
                    pb->click();
            });
        }
    }
}

void ModernRibbonBar::updateGroupCollapse()
{
    if (d_shutting_down || d_updating_collapse || !d_tab_stack || d_groups.isEmpty())
        return;
    // The tab pages span the whole ribbon (root layout without margins). The ribbon's own
    // width is already final when its Resize event arrives, the stacked widget's is not.
    const int avail = contentsRect().width();
    if (avail < 80)
        return;
    d_updating_collapse = true;

    auto widthOf = [](QWidget *w) -> int {
        int v = qMax(w->minimumSizeHint().width(), w->minimumWidth());
        if (!(w->sizePolicy().horizontalPolicy() & QSizePolicy::ShrinkFlag))
            v = qMax(v, w->sizeHint().width());
        return v;
    };

    QVector<QWidget *> pages;
    for (const RibbonGroupInfo &g : d_groups) {
        if (g.page && !pages.contains(g.page.data()))
            pages.append(g.page.data());
    }

    bool changed = false;
    for (QWidget *page : pages) {
        QBoxLayout *row = qobject_cast<QBoxLayout *>(page->layout());
        if (!row)
            continue;
        page->ensurePolished();

        QVector<int> gis;
        QHash<QWidget *, int> groupPos;
        QHash<QWidget *, int> foldPos;
        for (int i = 0; i < d_groups.size(); ++i) {
            const RibbonGroupInfo &g = d_groups.at(i);
            if (g.page != page || !g.group || !g.collapsedButton)
                continue;
            groupPos.insert(g.group.data(), gis.size());
            foldPos.insert(g.collapsedButton.data(), gis.size());
            // the folded button shows the group's first icon
            if (g.collapsedButton->icon().isNull()) {
                if (QLayout *gl = g.group->layout()) {
                    for (int k = 0; k < gl->count(); ++k) {
                        if (QToolButton *b = qobject_cast<QToolButton *>(gl->itemAt(k)->widget())) {
                            g.collapsedButton->setIcon(b->icon());
                            break;
                        }
                    }
                }
            }
            gis.append(i);
        }
        if (gis.isEmpty())
            continue;

        // Minimum width of the row for a given fold state (what QScrollArea compares with
        // its viewport to decide on the horizontal scroll bar).
        auto need = [&](const QVector<bool> &coll) -> int {
            const QMargins mg = row->contentsMargins();
            int total = mg.left() + mg.right();
            int n = 0;
            for (int i = 0; i < row->count(); ++i) {
                QLayoutItem *it = row->itemAt(i);
                QWidget *w = it ? it->widget() : nullptr;
                if (!w) {
                    if (it && !it->isEmpty()) {
                        total += it->minimumSize().width();
                        ++n;
                    }
                    continue;
                }
                if (groupPos.contains(w)) {
                    if (coll.at(groupPos.value(w)))
                        continue;
                } else if (foldPos.contains(w)) {
                    if (!coll.at(foldPos.value(w)))
                        continue;
                } else if (w->isHidden()) {
                    continue;
                }
                total += widthOf(w);
                ++n;
            }
            return total + qMax(0, n - 1) * qMax(0, row->spacing());
        };

        QVector<int> order(gis.size());
        for (int k = 0; k < order.size(); ++k)
            order[k] = k;
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            return d_groups.at(gis.at(a)).priority < d_groups.at(gis.at(b)).priority;
        });

        const int slack = 2;
        QVector<bool> coll(gis.size(), false);
        for (int oi = 0; oi < order.size() && need(coll) + slack > avail; ++oi) {
            const RibbonGroupInfo &g = d_groups.at(gis.at(order.at(oi)));
            // folding only helps when the drop-down is clearly narrower than the group (a
            // group that is a single drop-down already, like Panels, stays as it is)
            if (widthOf(g.collapsedButton.data()) + 12 > widthOf(g.group.data()))
                continue;
            coll[order.at(oi)] = true;
        }

        for (int k = 0; k < gis.size(); ++k) {
            RibbonGroupInfo &g = d_groups[gis.at(k)];
            const bool fold = coll.at(k);
            g.collapsed = fold;
            if (g.group->isHidden() != fold) {
                g.group->setVisible(!fold);
                changed = true;
            }
            if (g.collapsedButton->isHidden() == fold) {
                g.collapsedButton->setVisible(fold);
                changed = true;
            }
        }
        if (changed && row)
            row->activate();
    }
    d_updating_collapse = false;
    if (changed)
        updateTabStackHeight();
}

void ModernRibbonBar::setRibbonMinimized(bool minimized)
{
    if (d_shutting_down || d_minimized == minimized)
        return;
    d_minimized = minimized;
    if (d_tab_stack) {
        d_tab_stack->setVisible(!minimized);
        if (!minimized) {
            updateGroupCollapse();
            updateTabStackHeight();
        }
    }
    if (layout())
        layout()->activate();
    updateGeometry();
    if (d_app && !d_app->isHeadlessMode())
        ApplicationWindow::getSettings().setValue(QStringLiteral("/UX/RibbonMinimized"), minimized);
}

void ModernRibbonBar::setAdvancedDomainFeaturesVisible(bool visible)
{
    d_advanced_domain_visible = visible;
    // Ribbon entries of the features listed in ApplicationWindow::advancedDomainActionNames().
    static const QSet<QString> kAdvancedIcons = {
        QStringLiteral("fit-logistic"),       QStringLiteral("signal-wavelet"),
        QStringLiteral("tauc"),               QStringLiteral("eis-nyquist"),
        QStringLiteral("stats-bland-altman"), QStringLiteral("stats-bablok"),
        QStringLiteral("stats-survival"),     QStringLiteral("stats-roc"),
        QStringLiteral("signal-stft"),
    };
    for (const RibbonMenuActionBinding &ab : d_menu_action_bindings) {
        if (ab.action && kAdvancedIcons.contains(ab.iconId))
            ab.action->setVisible(visible);
    }
    if (d_btn_plot_3d)
        d_btn_plot_3d->setVisible(visible);
    for (const RibbonButtonBinding &bb : d_button_bindings) {
        if (!bb.button)
            continue;
        if (bb.button->property("_tg_advanced_domain").toBool())
            bb.button->setVisible(visible);
        if (bb.tooltipFn) {
            if (bb.button == d_btn_export_copy_ole && !bb.button->isEnabled())
                continue;
            QString tip = bb.tooltipFn();
            if (!bb.shortcutActionName.isEmpty() && d_app) {
                if (QAction *a = d_app->findChild<QAction *>(bb.shortcutActionName)) {
                    const QString key = a->shortcut().toString(QKeySequence::NativeText);
                    if (!key.isEmpty() && !tip.contains(key))
                        tip += QStringLiteral(" (%1)").arg(key);
                }
            }
            bb.button->setToolTip(tip);
        }
    }
    syncActionBoundButtons();
    updateGroupCollapse();
}

void ModernRibbonBar::setLegacyUiVisible(bool visible)
{
    if (d_shutting_down)
        return;
    // C15: the classic icon toolbars duplicate the ribbon; their switch is legacy UI.
    if (!visible)
        setClassicToolbarsVisible(false);
    if (d_btn_classic_toolbars) {
        // always explicit: before the ribbon is first shown every child still reports hidden
        d_btn_classic_toolbars->setVisible(visible);
        updateGroupCollapse();
    }
}

void ModernRibbonBar::triggerPlot2D(int curveType)
{
    if (d_shutting_down || !d_app)
        return;

    MyWidget *active = qobject_cast<MyWidget *>(d_app->d_workspace.activeSubWindow());
    Table *tbl = qobject_cast<Table *>(active);
    bool fromGraph = false;
    if (!tbl && d_focus_hud_mode && d_focus_hud_overlay
        && d_focus_hud_overlay->isTableDrawerVisible() && d_focus_hud_overlay->tableDrawer()) {
        tbl = d_focus_hud_overlay->tableDrawer()->currentTable();
    }
    MultiLayer *activeMl = qobject_cast<MultiLayer *>(active);
    if (!tbl && activeMl) {
        // B6: a graph is active. The plot buttons still mean "plot worksheet data", so use the
        // worksheet behind this graph (formerly only in the F11 HUD mode; elsewhere the click
        // silently re-styled the last curve).
        QSet<QObject *> ignored;
        for (MyWidget *mw : d_app->hiddenWindows) {
            if (mw && !isWorkbookManagedSheet(d_app, mw))
                ignored.insert(mw);
        }
        if (Table *srcTbl = sourceTableFromMultiLayer(activeMl, ignored)) {
            if (isUsableRibbonWorksheet(d_app, srcTbl)) {
                tbl = srcTbl;
                fromGraph = true;
            }
        }
    }

    if (!tbl) {
        d_app->showUxHint(activeMl
                                  ? tr("The plot buttons make a new graph from worksheet columns. "
                                       "This graph has no source worksheet (function or image "
                                       "data); click a worksheet first.")
                                  : tr("The plot buttons make a graph from worksheet columns. "
                                       "Click a worksheet (or paste data from Excel) first."));
        return;
    }

    if (fromGraph) {
        if (d_app->plotColumnsFromTable(tbl, curveType)) {
            ToriUx::dismissPlotThisDataBar(tbl);
            d_app->showUxHint(
                    tr("Created a new graph from \"%1\", the worksheet of the active graph.")
                            .arg(tbl->name()),
                    5000);
        }
        if (d_layout_mode == LayoutSplitTableGraph)
            applySplitWorkspaceGeometry();
        return;
    }

    const bool singleColPlot = (curveType == Graph::Histogram || curveType == Graph::Pie
                                || curveType == Graph::Box);
    const bool tableIsActive = (tbl == active);
    if (singleColPlot && tbl->selectedColumns().isEmpty()) {
        const QStringList yCols = tbl->YColumns();
        const QStringList allCols = tbl->columnsList();
        const QStringList pick = !yCols.isEmpty()
                ? QStringList { yCols.first() }
                : (!allCols.isEmpty() ? QStringList { allCols.last() } : QStringList());
        if (!pick.isEmpty() && d_app->plotBlockerForTable(tbl, curveType).isEmpty()) {
            if (d_app->multilayerPlot(tbl, pick, curveType))
                ToriUx::dismissPlotThisDataBar(tbl);
            if (d_layout_mode == LayoutSplitTableGraph)
                applySplitWorkspaceGeometry();
            return;
        }
    } else if (!singleColPlot && tbl->selectedYColumns().isEmpty() && !tbl->noXColumn()
               && d_app->plotBlockerForTable(tbl, curveType).isEmpty()) {
        const QStringList yCols = tbl->YColumns();
        if (!yCols.isEmpty()) {
            if (d_app->multilayerPlot(tbl, yCols, curveType))
                ToriUx::dismissPlotThisDataBar(tbl);
            if (d_layout_mode == LayoutSplitTableGraph)
                applySplitWorkspaceGeometry();
            return;
        }
    }

    // Validation with messages that say where to fix things (B7), then the normal plot path.
    if (tableIsActive) {
        QMetaObject::invokeMethod(d_app, "selectPlotType", Qt::DirectConnection,
                                  Q_ARG(int, curveType));
        if (Folder *curFolder = d_app->currentFolder()) {
            for (MyWidget *mw : curFolder->windowsList()) {
                if (MultiLayer *ml = qobject_cast<MultiLayer *>(mw)) {
                    if (multiLayerUsesTable(ml, tbl)) {
                        ToriUx::dismissPlotThisDataBar(tbl);
                        break;
                    }
                }
            }
        }
    } else if (d_app->plotColumnsFromTable(tbl, curveType)) {
        ToriUx::dismissPlotThisDataBar(tbl);
    }
    if (d_layout_mode == LayoutSplitTableGraph)
        applySplitWorkspaceGeometry();
}

void ModernRibbonBar::importOpjInteractive()
{
    if (d_shutting_down || !d_app)
        return;
    const QString fn = QFileDialog::getOpenFileName(
            d_app, tr("Import Origin OPJ / OPJU Project"), d_app->workingDir,
            tr("Origin Projects (*.opj *.OPJ *.opju *.OPJU);;All Files (*)"));
    if (!fn.isEmpty())
        d_app->importOPJ(fn);
}

void ModernRibbonBar::invokeAppToggleLanguage()
{
    if (d_shutting_down || !d_app)
        return;
    if (!QMetaObject::invokeMethod(d_app.data(), "toggleLanguage", Qt::DirectConnection)) {
        if (d_app->appLanguage.startsWith(QLatin1String("ja"), Qt::CaseInsensitive))
            d_app->switchToEnglish();
        else
            d_app->switchToJapanese();
    }
}

void ModernRibbonBar::applyWorksheetColumnRole(int roleCode)
{
    if (d_shutting_down || !d_app)
        return;
    Table *tbl = resolvedWorksheetTarget();
    if (!tbl || !tbl->d_future_table || tbl->numCols() <= 0)
        return;

    QList<int> colIndices;
    const QStringList selNames = tbl->selectedColumns();
    for (const QString &name : selNames) {
        const int idx = tbl->colIndex(name);
        if (idx >= 0 && !colIndices.contains(idx))
            colIndices.append(idx);
    }
    if (colIndices.isEmpty()) {
        const int firstSel = tbl->firstSelectedColumn();
        if (firstSel >= 0 && firstSel < tbl->numCols())
            colIndices.append(firstSel);
        else
            colIndices.append(0);
    }

    tbl->d_future_table->beginMacro(
            tr("%1: set column plot designation").arg(tbl->name()));
    for (int ci : colIndices) {
        if (ci < 0 || ci >= tbl->numCols())
            continue;
        Column *c = tbl->column(ci);
        if (!c)
            continue;
        if (roleCode == -100) {
            c->setProperty("_tg_label_role", true);
            if (c->columnMode() != SciDAVis::ColumnMode::Text)
                c->setColumnMode(SciDAVis::ColumnMode::Text);
            c->setPlotDesignation(SciDAVis::noDesignation);
        } else {
            c->setProperty("_tg_label_role", false);
            c->setPlotDesignation(static_cast<SciDAVis::PlotDesignation>(roleCode));
        }
    }
    tbl->d_future_table->endMacro();
    tbl->notifyChanges();
}

void ModernRibbonBar::initStatusBarZoomControls()
{
    if (d_shutting_down || !d_app || !d_app->statusBar())
        return;
    QStatusBar *sb = d_app->statusBar();

    const QString statusZoomStyle = QStringLiteral(
            "QToolButton {"
            "  background-color: #FFFFFF;"
            "  color: #334155;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 5px;"
            "  padding: 1px 6px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 9.5pt;"
            "  font-weight: 600;"
            "}"
            "QToolButton:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #93C5FD;"
            "}"
            "QToolButton::menu-indicator { image: none; width: 0px; }");

    d_status_zoom_btn = new QToolButton(sb);
    d_status_zoom_btn->setObjectName(QStringLiteral("StatusBarPageZoomBtn"));
    d_status_zoom_btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    d_status_zoom_btn->setPopupMode(QToolButton::InstantPopup);
    d_status_zoom_btn->setCursor(Qt::PointingHandCursor);
    d_status_zoom_btn->setFocusPolicy(Qt::TabFocus);
    d_status_zoom_btn->setStyleSheet(statusZoomStyle);
    QMenu *zoomMenu = new QMenu(d_status_zoom_btn);
    connect(zoomMenu, &QMenu::aboutToShow, this, [this, zoomMenu]() {
        zoomMenu->clear();
        QAction *fitAct = zoomMenu->addAction(
                IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 16, false),
                tr("Fit to Window"));
        connect(fitAct, &QAction::triggered, this, [this]() {
            if (!d_app)
                return;
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
                if (!QMetaObject::invokeMethod(ml, "fitPageZoomToViewport", Qt::DirectConnection))
                    ml->setPageZoom(1.0);
                updateStatusBarZoomIndicator(ml);
            }
        });
        zoomMenu->addSeparator();
        for (int pct : { 50, 75, 100, 125, 150, 200 }) {
            QAction *pa = zoomMenu->addAction(QStringLiteral("%1%").arg(pct));
            connect(pa, &QAction::triggered, this, [this, pct]() {
                if (!d_app)
                    return;
                if (MultiLayer *ml =
                            qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
                    ml->setPageZoom(double(pct) / 100.0);
                    updateStatusBarZoomIndicator(ml);
                }
            });
        }
    });
    d_status_zoom_btn->setMenu(zoomMenu);

    d_status_fit_btn = new QToolButton(sb);
    d_status_fit_btn->setObjectName(QStringLiteral("StatusBarFitWindowBtn"));
    d_status_fit_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_status_fit_btn->setIcon(IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 14, false));
    d_status_fit_btn->setIconSize(QSize(13, 13));
    d_status_fit_btn->setCursor(Qt::PointingHandCursor);
    d_status_fit_btn->setFocusPolicy(Qt::TabFocus);
    d_status_fit_btn->setStyleSheet(statusZoomStyle);
    connect(d_status_fit_btn, &QToolButton::clicked, this, [this]() {
        if (!d_app)
            return;
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
            if (!QMetaObject::invokeMethod(ml, "fitPageZoomToViewport", Qt::DirectConnection))
                ml->setPageZoom(1.0);
            updateStatusBarZoomIndicator(ml);
        }
    });

    sb->addPermanentWidget(d_status_zoom_btn);
    sb->addPermanentWidget(d_status_fit_btn);
    d_status_zoom_btn->hide();
    d_status_fit_btn->hide();

    // Command search is already prominent at top-right of the Ribbon; drop redundant bottom Ctrl+K pill.
    sb->installEventFilter(this);
    if (QToolButton *cmdHint =
                sb->findChild<QToolButton *>(QStringLiteral("StatusBarCommandHintBtn"))) {
        sb->removeWidget(cmdHint);
        cmdHint->hide();
        cmdHint->deleteLater();
    }
}

void ModernRibbonBar::updateStatusBarZoomIndicator(MultiLayer *ml)
{
    if (d_shutting_down)
        return;

    if (d_zoom_bound_ml && d_zoom_bound_ml.data() != ml) {
        disconnect(d_zoom_bound_ml.data(), nullptr, this, nullptr);
        d_zoom_bound_ml.clear();
    }
    if (ml && d_zoom_bound_ml.data() != ml) {
        d_zoom_bound_ml = ml;
        QPointer<MultiLayer> mlPtr(ml);
        connect(ml, &MultiLayer::modifiedPlot, this, [this, mlPtr]() {
            if (!d_shutting_down && mlPtr && d_zoom_bound_ml == mlPtr)
                updateStatusBarZoomIndicator(mlPtr.data());
        });
    }

    const bool showZoom = (ml != nullptr);
    if (d_status_zoom_btn) {
        if (showZoom) {
            const int pct = qMax(10, qRound(ml->pageZoom() * 100.0));
            d_status_zoom_btn->setText(QStringLiteral("%1%").arg(pct));
            d_status_zoom_btn->setToolTip(
                    tr("Page Zoom: %1% (click to change or fit to window)").arg(pct));
        }
        d_status_zoom_btn->setVisible(showZoom);
    }
    if (d_status_fit_btn) {
        d_status_fit_btn->setText(tr("Fit"));
        d_status_fit_btn->setToolTip(tr("Fit graph page zoom to the current window"));
        d_status_fit_btn->setVisible(showZoom);
    }
}

void ModernRibbonBar::triggerMatrixOrXyzPlot(int kind)
{
    if (d_shutting_down || !d_app)
        return;

    MyWidget *aw = qobject_cast<MyWidget *>(d_app->d_workspace.activeSubWindow());
    if (Matrix *mat = qobject_cast<Matrix *>(aw)) {
        if (kind == 0)
            d_app->plotColorMap(mat);
        else if (kind == 1)
            d_app->plotContour(mat);
        else
            d_app->plot3DWireSurface();
        if (d_layout_mode == LayoutSplitTableGraph)
            applySplitWorkspaceGeometry();
        return;
    }

    Table *tbl = qobject_cast<Table *>(aw);
    if (!tbl && d_focus_hud_mode && d_focus_hud_overlay
        && d_focus_hud_overlay->isTableDrawerVisible() && d_focus_hud_overlay->tableDrawer()) {
        tbl = d_focus_hud_overlay->tableDrawer()->currentTable();
    }
    if (!tbl) {
        if (MultiLayer *activeMl = qobject_cast<MultiLayer *>(aw))
            tbl = sourceTableFromMultiLayer(activeMl);
    }
    if (!tbl) {
        d_app->showUxHint(tr("Click a Matrix or Worksheet first to create a Heatmap, Contour, or 3D Surface."));
        return;
    }

    // 2-column worksheet for Heatmap / Contour -> launch 2D Density (2D Histogram) directly.
    if (tbl->numCols() < 3 && kind != 2) {
        ExtraPlotTypes::showDensity2DDialog(d_app);
        return;
    }

    auto plotFromMatrix = [this, kind, tbl](Matrix *m) {
        if (!m || !d_app)
            return;
        if (kind == 0) {
            d_app->plotColorMap(m);
        } else if (kind == 1) {
            d_app->plotContour(m);
        } else {
            d_app->activateSubWindow(m);
            d_app->plot3DWireSurface();
        }
        if (tbl)
            ToriUx::dismissPlotThisDataBar(tbl);
        if (d_layout_mode == LayoutSplitTableGraph)
            applySplitWorkspaceGeometry();
    };

    auto gridXyzColumnsToMatrix = [this, tbl](int xCol, int yCol, int zCol, int gridN) -> Matrix * {
        if (!tbl || !d_app || xCol < 0 || yCol < 0 || zCol < 0 || xCol >= tbl->numCols()
            || yCol >= tbl->numCols() || zCol >= tbl->numCols()) {
            return nullptr;
        }
        QVector<double> xs, ys, zs;
        const int nRows = tbl->numRows();
        xs.reserve(nRows);
        ys.reserve(nRows);
        zs.reserve(nRows);
        double xMin = 0.0, xMax = 0.0, yMin = 0.0, yMax = 0.0;
        bool hasBounds = false;
        for (int r = 0; r < nRows; ++r) {
            if (tbl->text(r, xCol).trimmed().isEmpty() || tbl->text(r, yCol).trimmed().isEmpty()
                || tbl->text(r, zCol).trimmed().isEmpty()) {
                continue;
            }
            const double xv = tbl->cell(r, xCol);
            const double yv = tbl->cell(r, yCol);
            const double zv = tbl->cell(r, zCol);
            if (!std::isfinite(xv) || !std::isfinite(yv) || !std::isfinite(zv))
                continue;
            xs.append(xv);
            ys.append(yv);
            zs.append(zv);
            if (!hasBounds) {
                xMin = xMax = xv;
                yMin = yMax = yv;
                hasBounds = true;
            } else {
                xMin = qMin(xMin, xv);
                xMax = qMax(xMax, xv);
                yMin = qMin(yMin, yv);
                yMax = qMax(yMax, yv);
            }
        }
        if (!hasBounds || xs.isEmpty()) {
            d_app->showUxHint(tr("No valid finite XYZ numeric rows found in the selected columns."));
            return nullptr;
        }
        if (qFuzzyCompare(xMin, xMax)) {
            xMin -= 0.5;
            xMax += 0.5;
        }
        if (qFuzzyCompare(yMin, yMax)) {
            yMin -= 0.5;
            yMax += 0.5;
        }

        const int nx = qBound(4, gridN, 200);
        const int ny = qBound(4, gridN, 200);
        Matrix *m = d_app->newMatrix(
                d_app->generateUniqueName(tbl->name() + QStringLiteral("_Grid")), ny, nx);
        if (!m)
            return nullptr;
        m->setCoordinates(xMin, xMax, yMin, yMax);
        const double dx = (xMax - xMin) / double(nx - 1);
        const double dy = (yMax - yMin) / double(ny - 1);
        const double invScaleX = 1.0 / qMax(1e-12, xMax - xMin);
        const double invScaleY = 1.0 / qMax(1e-12, yMax - yMin);
        const int nPts = xs.size();
        for (int r = 0; r < ny; ++r) {
            const double gy = yMin + double(r) * dy;
            for (int c = 0; c < nx; ++c) {
                const double gx = xMin + double(c) * dx;
                double wSum = 0.0;
                double vzSum = 0.0;
                bool exact = false;
                double exactVal = 0.0;
                for (int i = 0; i < nPts; ++i) {
                    const double ux = (gx - xs.at(i)) * invScaleX;
                    const double uy = (gy - ys.at(i)) * invScaleY;
                    const double d2 = ux * ux + uy * uy;
                    if (d2 < 1e-18) {
                        exact = true;
                        exactVal = zs.at(i);
                        break;
                    }
                    const double w = 1.0 / d2;
                    wSum += w;
                    vzSum += w * zs.at(i);
                }
                m->setCell(r, c, exact ? exactVal : (wSum > 0.0 ? vzSum / wSum : 0.0));
            }
        }
        return m;
    };

    // Detect default X, Y, Z columns
    int defX = -1, defY = -1, defZ = -1;
    for (int c = 0; c < tbl->numCols(); ++c) {
        const auto pd = tbl->colPlotDesignation(c);
        if (pd == SciDAVis::X && defX < 0)
            defX = c;
        else if (pd == SciDAVis::Y && defY < 0)
            defY = c;
        else if (pd == SciDAVis::Z && defZ < 0)
            defZ = c;
    }
    if (defX < 0 && tbl->numCols() >= 1)
        defX = 0;
    if (defY < 0 && tbl->numCols() >= 2)
        defY = (defX == 0 ? 1 : 0);
    if (defZ < 0 && tbl->numCols() >= 3) {
        for (int c = 0; c < tbl->numCols(); ++c) {
            if (c != defX && c != defY) {
                defZ = c;
                break;
            }
        }
    }

    // Non-interactive callers (tests/scripts): convert automatically without blocking dialog.
    if (qApp && qApp->property("torigraph_noninteractive").toBool()) {
        if (defX >= 0 && defY >= 0 && defZ >= 0) {
            if (Matrix *m = gridXyzColumnsToMatrix(defX, defY, defZ, 32))
                plotFromMatrix(m);
        } else {
            d_app->activateSubWindow(tbl);
            if (Matrix *m = d_app->convertTableToMatrix())
                plotFromMatrix(m);
        }
        return;
    }

    const QString kindTitle = (kind == 0) ? tr("Heatmap")
            : (kind == 1)                 ? tr("Contour")
                                          : tr("3D Surface");
    QDialog dlg(d_app);
    dlg.setWindowTitle(tr("Plot from Worksheet"));
    QVBoxLayout *vlay = new QVBoxLayout(&dlg);
    QLabel *prompt = new QLabel(tr("How should this worksheet be converted for %1?").arg(kindTitle),
                                &dlg);
    prompt->setWordWrap(true);
    vlay->addWidget(prompt);

    QRadioButton *rbXyz = new QRadioButton(
            tr("Grid XYZ columns into a 2D Matrix (recommended for X, Y, Z tables)"), &dlg);
    QRadioButton *rbDirect = new QRadioButton(
            tr("Treat worksheet cells directly as a 2D Z-value grid (Convert to Matrix)"), &dlg);
    QRadioButton *rbDensity = nullptr;
    if (kind != 2) {
        rbDensity = new QRadioButton(
                tr("2D Density Heatmap (count X-Y points on a grid)"), &dlg);
    }

    const bool hasXyzCols = (tbl->numCols() >= 3);
    rbXyz->setEnabled(hasXyzCols);
    if (hasXyzCols)
        rbXyz->setChecked(true);
    else
        rbDirect->setChecked(true);

    vlay->addWidget(rbXyz);
    QWidget *xyzPanel = new QWidget(&dlg);
    QFormLayout *flay = new QFormLayout(xyzPanel);
    flay->setContentsMargins(20, 2, 4, 4);
    QComboBox *cbX = new QComboBox(xyzPanel);
    QComboBox *cbY = new QComboBox(xyzPanel);
    QComboBox *cbZ = new QComboBox(xyzPanel);
    for (int c = 0; c < tbl->numCols(); ++c) {
        const QString lbl = tbl->colLabel(c);
        cbX->addItem(lbl, c);
        cbY->addItem(lbl, c);
        cbZ->addItem(lbl, c);
    }
    if (defX >= 0)
        cbX->setCurrentIndex(defX);
    if (defY >= 0)
        cbY->setCurrentIndex(defY);
    if (defZ >= 0)
        cbZ->setCurrentIndex(defZ);
    QSpinBox *sbGrid = new QSpinBox(xyzPanel);
    sbGrid->setRange(8, 200);
    sbGrid->setValue(40);
    flay->addRow(tr("X column:"), cbX);
    flay->addRow(tr("Y column:"), cbY);
    flay->addRow(tr("Z column:"), cbZ);
    flay->addRow(tr("Grid size:"), sbGrid);
    xyzPanel->setEnabled(rbXyz->isChecked());
    connect(rbXyz, &QRadioButton::toggled, xyzPanel, &QWidget::setEnabled);
    vlay->addWidget(xyzPanel);

    vlay->addWidget(rbDirect);
    if (rbDensity)
        vlay->addWidget(rbDensity);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vlay->addWidget(bb);

    if (dlg.exec() != QDialog::Accepted)
        return;

    if (rbDensity && rbDensity->isChecked()) {
        ExtraPlotTypes::showDensity2DDialog(d_app);
    } else if (rbXyz->isChecked()) {
        if (Matrix *m = gridXyzColumnsToMatrix(cbX->currentData().toInt(),
                                               cbY->currentData().toInt(),
                                               cbZ->currentData().toInt(), sbGrid->value())) {
            plotFromMatrix(m);
        }
    } else {
        d_app->activateSubWindow(tbl);
        if (Matrix *m = d_app->convertTableToMatrix())
            plotFromMatrix(m);
    }
}

void ModernRibbonBar::triggerTable3DPlot(int graph3dType)
{
    if (d_shutting_down || !d_app)
        return;

    MyWidget *aw = qobject_cast<MyWidget *>(d_app->d_workspace.activeSubWindow());
    if (qobject_cast<Matrix *>(aw)) {
        if (graph3dType == Graph3D::Bars)
            d_app->plot3DBars();
        else
            d_app->plot3DScatter();
        return;
    }

    Table *tbl = qobject_cast<Table *>(aw);
    if (!tbl) {
        d_app->showUxHint(tr("Select a worksheet with X, Y, and Z columns first for 3D plotting."));
        return;
    }

    // C3-1: Accept 1 selected Z column, (X, Y, Z) 3-column selection, or automatic Z column lookup.
    int zCol = -1;
    const QStringList sel = tbl->selectedColumns();
    for (const QString &name : sel) {
        const int idx = tbl->colIndex(name);
        if (idx >= 0 && tbl->colPlotDesignation(idx) == SciDAVis::Z) {
            zCol = idx;
            break;
        }
    }
    if (zCol < 0 && sel.size() == 3) {
        const int idx = tbl->colIndex(sel.last());
        if (idx >= 0) {
            tbl->setColPlotDesignation(idx, SciDAVis::Z);
            zCol = idx;
        }
    }
    if (zCol < 0) {
        for (int c = 0; c < tbl->numCols(); ++c) {
            if (tbl->colPlotDesignation(c) == SciDAVis::Z) {
                zCol = c;
                break;
            }
        }
    }
    if (zCol < 0 && tbl->numCols() >= 3) {
        zCol = 2;
        tbl->setColPlotDesignation(zCol, SciDAVis::Z);
    }
    if (zCol >= 0) {
        d_app->dataPlotXYZ(tbl, tbl->colName(zCol), graph3dType);
        ToriUx::dismissPlotThisDataBar(tbl);
        if (d_layout_mode == LayoutSplitTableGraph)
            applySplitWorkspaceGeometry();
        return;
    }

    if (graph3dType == Graph3D::Bars)
        d_app->plot3DBars();
    else
        d_app->plot3DScatter();
}

int ModernRibbonBar::tabCount() const
{
    return d_tab_bar ? d_tab_bar->count() : 0;
}

int ModernRibbonBar::currentTabIndex() const
{
    return d_tab_bar ? d_tab_bar->currentIndex() : -1;
}

void ModernRibbonBar::setCurrentTabIndex(int index)
{
    if (d_tab_bar && index >= 0 && index < d_tab_bar->count())
        d_tab_bar->setCurrentIndex(index);
}

QString ModernRibbonBar::tabTitle(int index) const
{
    return (d_tab_bar && index >= 0 && index < d_tab_bar->count())
            ? unescapeTabMnemonicText(d_tab_bar->tabText(index))
            : QString();
}

QString ModernRibbonBar::activeContextText() const
{
    return d_context_pill ? d_context_pill->text() : QString();
}

CommandPalettePopup *ModernRibbonBar::commandPalette()
{
    if (d_shutting_down)
        return nullptr;
    if (!d_command_palette)
        d_command_palette = new CommandPalettePopup(d_app, this, d_app);
    return d_command_palette.data();
}

FocusCanvasHudOverlay *ModernRibbonBar::focusHudOverlay() const
{
    return d_focus_hud_overlay.data();
}

FocusTableDrawer *ModernRibbonBar::focusTableDrawer() const
{
    return d_focus_hud_overlay ? d_focus_hud_overlay->tableDrawer() : nullptr;
}

void ModernRibbonBar::openCommandPalette()
{
    if (d_shutting_down) {
        if (d_command_palette)
            d_command_palette->hide();
        return;
    }

    // B4-10: Pressing Ctrl+K while already open toggles the palette closed.
    if (d_command_palette && d_command_palette->isVisible()) {
        d_command_palette->hide();
        return;
    }

    CommandPalettePopup *popup = commandPalette();
    if (!popup)
        return;

    popup->refreshCommands();
    popup->setFilterText(QString());

    if (d_app) {
        const QPoint topCenter = d_app->mapToGlobal(QPoint(d_app->width() / 2, 76));
        int px = topCenter.x() - popup->width() / 2;
        int py = topCenter.y();
        if (QScreen *scr = d_app->screen()) {
            const QRect avail = scr->availableGeometry();
            px = qBound(avail.left() + 8, px, qMax(avail.left() + 8, avail.right() - popup->width() - 8));
            py = qBound(avail.top() + 8, py, qMax(avail.top() + 8, avail.bottom() - popup->height() - 8));
        }
        popup->move(px, py);
    }

    popup->show();
    popup->raise();
    popup->activateWindow();
}

bool ModernRibbonBar::eventFilter(QObject *watched, QEvent *event)
{
    if (d_shutting_down)
        return QFrame::eventFilter(watched, event);
    if (watched == this && event
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateHeaderResponsiveness();
        updateGroupCollapse();
        updateTabStackHeight();
    }
    if (watched == d_tab_bar && event) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                // The first click of this double-click already restored the ribbon.
                const bool justRestored = d_restored_by_click_ms >= 0
                        && now - d_restored_by_click_ms
                                <= qint64(QApplication::doubleClickInterval()) + 100;
                d_restored_by_click_ms = -1;
                if (!justRestored)
                    setRibbonMinimized(!d_minimized);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonPress && d_minimized) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && d_tab_bar->tabAt(me->pos()) >= 0) {
                // a single click on a tab of the minimized ribbon brings the commands back
                setRibbonMinimized(false);
                d_restored_by_click_ms = QDateTime::currentMSecsSinceEpoch();
            }
        }
    }
    if (d_app && watched == d_app->statusBar() && event
        && (event->type() == QEvent::ChildAdded || event->type() == QEvent::Show)) {
        if (QStatusBar *sb = d_app->statusBar()) {
            if (QToolButton *cmdHint =
                        sb->findChild<QToolButton *>(QStringLiteral("StatusBarCommandHintBtn"))) {
                sb->removeWidget(cmdHint);
                cmdHint->hide();
                cmdHint->deleteLater();
            }
        }
    }
    if (d_app && watched == d_app->d_workspace.viewport() && event) {
        const QEvent::Type et = event->type();
        if (et == QEvent::Resize && d_layout_mode == LayoutSplitTableGraph
            && !d_applying_split_geometry) {
            applySplitWorkspaceGeometry();
        }
        if (et == QEvent::Resize || et == QEvent::Show) {
            updateEmptyWelcomeHub();
        } else if (et == QEvent::ChildAdded || et == QEvent::ChildRemoved) {
            QChildEvent *ce = static_cast<QChildEvent *>(event);
            if (!d_updating_welcome_hub && ce->child() != d_welcome_hub_card.data())
                updateEmptyWelcomeHub();
        }
    }
    if (d_watched_empty_graph_pane && watched == d_watched_empty_graph_pane && event
        && (event->type() == QEvent::Show || event->type() == QEvent::Resize)) {
        const bool hasTableOnlySplit = (d_layout_mode == LayoutSplitTableGraph && d_split_table
                                        && !d_split_plot && !d_split_table->isHidden());
        if ((d_welcome_hub_card && !d_welcome_hub_card->isHidden()) || hasTableOnlySplit) {
            d_watched_empty_graph_pane->hide();
            return true;
        }
    }
    if (d_welcome_hub_card && watched == d_welcome_hub_card && event) {
        const QEvent::Type et = event->type();
        if (et == QEvent::DragEnter || et == QEvent::Drop) {
            if (d_app)
                QApplication::sendEvent(d_app.data(), event);
            if (et == QEvent::Drop)
                updateEmptyWelcomeHub();
            return true;
        }
        if (et == QEvent::DragMove) {
            QDragMoveEvent *dme = static_cast<QDragMoveEvent *>(event);
            if (dme->mimeData()
                && (dme->mimeData()->hasUrls()
                    || dme->mimeData()->hasFormat(QLatin1String(MultiLayer::kColumnMimeType)))) {
                dme->acceptProposedAction();
                return true;
            }
        }
        if (et == QEvent::ShortcutOverride || et == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_V && (ke->modifiers() & Qt::ControlModifier)) {
                if (et == QEvent::KeyPress)
                    pasteFromClipboardSmart();
                ke->accept();
                return true;
            }
        }
    }
    if (d_app && event && d_layout_mode == LayoutSplitTableGraph && !d_applying_split_geometry
        && d_split_table && d_split_plot && !d_split_table->isMaximized()
        && !d_split_plot->isMaximized()
        && (watched == d_split_table || watched == d_split_plot)) {
        const QEvent::Type et = event->type();
        if (et == QEvent::Resize || et == QEvent::Move) {
            QWidget *vp = d_app->d_workspace.viewport();
            const QRect vpRect = vp ? vp->rect() : d_app->d_workspace.rect();
            if (vpRect.width() >= 280 && vpRect.height() >= 120) {
                const bool focusHudActive = d_focus_hud_mode
                        || (d_focus_hud_overlay && d_focus_hud_overlay->isOverlayActive());
                const int tileTopY = focusHudActive ? 48 : 0;
                const int fullH = qMax(100, vpRect.height() - tileTopY);
                const int minPaneW = 120;
                const int maxLeftW = qMax(minPaneW, vpRect.width() - minPaneW);
                int newSplitX = -1;
                if (watched == d_split_table && et == QEvent::Resize) {
                    const int cand = d_split_table->geometry().right() + 1;
                    if (cand >= minPaneW && cand <= maxLeftW
                        && qAbs(cand - d_split_plot->geometry().left()) >= 2) {
                        newSplitX = cand;
                    }
                } else if (watched == d_split_plot) {
                    const int cand = d_split_plot->geometry().left();
                    if (cand >= minPaneW && cand <= maxLeftW
                        && qAbs(cand - (d_split_table->geometry().right() + 1)) >= 2) {
                        newSplitX = cand;
                    }
                }
                if (newSplitX >= minPaneW && newSplitX <= maxLeftW) {
                    d_applying_split_geometry = true;
                    d_split_left_ratio =
                            qBound(0.15, double(newSplitX) / double(vpRect.width()), 0.85);
                    if (d_split_table->minimumWidth() > newSplitX)
                        d_split_table->setMinimumWidth(newSplitX);
                    d_split_table->setGeometry(0, tileTopY, newSplitX, fullH);
                    d_split_plot->setGeometry(newSplitX, tileTopY,
                                              qMax(minPaneW, vpRect.width() - newSplitX), fullH);
                    d_applying_split_geometry = false;
                }
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

void ModernRibbonBar::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}
