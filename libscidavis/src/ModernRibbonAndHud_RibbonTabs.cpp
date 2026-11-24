/***************************************************************************
    File                 : ModernRibbonAndHud_RibbonTabs.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Description          : Contextual Slim Ribbon Tabs, Group Collapse, Retranslation, and Action Sync
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
#include "ModernRibbonAndHud_Internal.h"
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
#include <QScopedValueRollback>
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

#include <QAbstractSpinBox>
#include <QDateTime>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QTextEdit>
#include <mutex>

namespace {

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

} // namespace

namespace ModernRibbonHudInternal {

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
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("ModernRibbonBar"), QStringLiteral("Fill Down"),
                QString::fromUtf8("下方向へフィル"));
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

} // namespace ModernRibbonHudInternal

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

QWidget *ModernRibbonBar::createRibbonScrollPage(QHBoxLayout **outRowLayout)
{
    RibbonHorizontalScrollArea *scroll = new RibbonHorizontalScrollArea(d_tab_stack);
    QWidget *container = new QWidget(scroll);
    container->setObjectName(QStringLiteral("ModernRibbonPageContainer"));
    container->setStyleSheet(
            QStringLiteral("QWidget#ModernRibbonPageContainer { background: #FFFFFF; }"));
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
        div->setObjectName(QStringLiteral("RibbonGroupDivider"));
        div->setFixedSize(1, 28);
        div->setStyleSheet(
                QStringLiteral("QFrame#RibbonGroupDivider { background-color: #CBD5E1; margin: 0px 3px; }"));
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
                        if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                            d_app->drawArrow();
                    });
            addRibbonMenuAction(
                    addMenu, QStringLiteral("plot-line"), [this]() { return tr("Draw Line"); },
                    [this]() { return tr("Draw a straight line annotation on the active graph"); },
                    [this]() {
                        if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                        if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
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
                        if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
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
                        if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
                            if (Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->layer(1))
                                GraphInspectorDock::addReferenceLineInteractive(g, 0);
                        }
                    });
        }
        addRibbonButton(
                gAdd, QStringLiteral("error-bars"), [this]() { return tr("Error Bars"); },
                [this]() { return tr("Add Error Bars from Column, SD, or Percentage"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->addLinkedTopRightLayer();
                });
        addRibbonButton(
                gLayer, QStringLiteral("layer-inset"), [this]() { return tr("Zoom Inset"); },
                [this]() { return tr("Add Magnified Zoom Inset Layer"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->showLayerDialog();
                });
        addRibbonMenuAction(
                moreLayersMenu, QStringLiteral("layer-add"),
                [this]() { return tr("Add Layer"); },
                [this]() { return tr("Add a New Plot Layer to the Active Graph"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->fitAutoPeakPseudoVoigt();
                });
        addRibbonMenuAction(
                morePeaksMenu, QStringLiteral("peak-batch"),
                [this]() { return tr("Batch Peaks"); },
                [this]() { return tr("Batch Multi-Spectrum Peak Analysis"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->maskDataPointsTool();
                });
        addRibbonMenuAction(
                mathToolsMenu, QStringLiteral("range-selector"),
                [this]() { return tr("Mask ROI"); },
                [this]() { return tr("Mask Rectangular Data Region"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->maskRegionGadget();
                });
        addRibbonMenuAction(
                mathToolsMenu, QStringLiteral("clear-masks"),
                [this]() { return tr("Clear Masks"); },
                [this]() { return tr("Clear All Data Masks on Active Graph"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->clearAllMasksOnGraph();
                });

        createRibbonGroup(row, &gGadgets, true, [this]() { return tr("Gadgets"); }, 60);
        addRibbonButton(
                gGadgets, QStringLiteral("gadget-roi"), [this]() { return tr("ROI Stats"); },
                [this]() { return tr("Interactive Region-of-Interest Statistics Gadget"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->attachRegionStatsGadget();
                });
        addRibbonButton(
                gGadgets, QStringLiteral("gadget-integrate"), [this]() { return tr("Integrate"); },
                [this]() { return tr("Interactive Peak / Curve Area Integration Gadget"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->attachIntegrateGadget();
                });
        addRibbonButton(
                gGadgets, QStringLiteral("gadget-quick-fit"), [this]() { return tr("Quick Fit"); },
                [this]() { return tr("Interactive ROI Quick Fit Gadget"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->attachVerticalCursorGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-quick-peaks"),
                [this]() { return tr("Quick Peaks"); },
                [this]() { return tr("Interactive ROI Quick Peak Finder Gadget"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->attachQuickPeaksGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-delta"),
                [this]() { return tr("Delta Cursor"); },
                [this]() { return tr("Dual-Cursor Delta X / Delta Y / Slope Gadget"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->attachDeltaCursorGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-profile-2d"),
                [this]() { return tr("2D Profile"); },
                [this]() { return tr("Interactive Heatmap Horizontal/Vertical Profile Gadget"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->attachImageProfileGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-diff"),
                [this]() { return tr("Differentiate"); },
                [this]() { return tr("Interactive Derivative Gadget"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->attachDifferentiateGadget();
                });
        moreGadgetsMenu->addSeparator();
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-clear"),
                [this]() { return tr("Clear Gadget"); },
                [this]() { return tr("Remove Active Gadget from Graph"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
                        if (d_app->d_workspace.activeSubWindow() != ml)
                            d_app->d_workspace.setActiveSubWindow(ml);
                        d_app->applyStyleISIJJournal();
                    }
                },
                false, true);
        addRibbonButton(
                gPresets, QStringLiteral("preset-slide"),
                [this]() { return tr("Lab Slide (18pt)"); },
                [this]() {
                    return tr("83.3 mm square, 12.9 pt Arial. Becomes 18 pt on the slide with 'Send to PowerPoint' (140% paste).");
                },
                [this]() {
                    if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
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
                        if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
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
                    if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->applyFrameBoxInward();
                });
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("frame-open-l"),
                [this]() { return tr("Open-L Frame"); },
                [this]() { return tr("Apply Open Bottom-Left L-Frame"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->applyFrameOpenL();
                });
        framePageMenu->addSeparator();
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("zoom-auto"),
                [this]() { return tr("Fit to Window"); },
                [this]() { return tr("Fit graph page zoom to the current window"); },
                [this]() {
                    if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->showPageSetupDialog();
                });
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("fit-page"), [this]() { return tr("Trim Page"); },
                [this]() { return tr("Crop Page Tightly Around Plot Layers"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->fitPageToLayers();
                });
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("aspect-square"),
                [this]() { return tr("Square 70%"); },
                [this]() { return tr("Apply 70% Square Inner Canvas Frame"); },
                [this]() {
                    if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
                        d_app->copyGraphFormat();
                });
        addRibbonMenuAction(
                formatBatchMenu, QStringLiteral("format-paste"),
                [this]() { return tr("Paste Format"); },
                [this]() { return tr("Paste Copied Visual Style onto Active Graph"); },
                [this]() {
                    if (ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app))
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
    ModernRibbonHudInternal::registerRibbonCollapseTranslations();
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
