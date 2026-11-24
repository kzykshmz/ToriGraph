/***************************************************************************
    File                 : GraphInspectorDock.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Copyright            : (C) 2026 ToriGraph Project
    Description          : OriginPro-Style Non-Modal Object Manager &
                           Context-Sensitive Live Property Inspector Dock
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

#include "GraphInspectorDock.h"
#include "GraphInspectorDock_Internal.h"

#include "ApplicationWindow.h"
#include "ArrowMarker.h"
#include "AxesDialog.h"
#include "FunctionCurve.h"
#include "GraphMiniToolbar.h"
#include "Grid.h"
#include "I18nJapanese.h"
#include "IconLoader.h"
#include "ImageMarker.h"
#include "Legend.h"
#include "ModernRibbonAndHud.h"
#include "ModernUiTheme.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "CanvasInlineTextEditor.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"
#include "ScaleDraw.h"
#include "Spectrogram.h"
#include "PlotStyleLimits.h"
#include "SymbolBox.h"
#include "Table.h"
#include "ImportASCIIDialog.h"
#include "Workbook.h"
#include "XlsxImport.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTextCursor>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyleOptionComboBox>
#include <QStylePainter>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVector>
#include <QWidgetAction>

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>

namespace {

QString mapToUnicodeSubscript(const QString &src)
{
    QString out;
    out.reserve(src.size());
    for (QChar ch : src) {
        switch (ch.unicode()) {
        case '0': out += QChar(0x2080); break;
        case '1': out += QChar(0x2081); break;
        case '2': out += QChar(0x2082); break;
        case '3': out += QChar(0x2083); break;
        case '4': out += QChar(0x2084); break;
        case '5': out += QChar(0x2085); break;
        case '6': out += QChar(0x2086); break;
        case '7': out += QChar(0x2087); break;
        case '8': out += QChar(0x2088); break;
        case '9': out += QChar(0x2089); break;
        case '+': out += QChar(0x208A); break;
        case '-': out += QChar(0x208B); break;
        case '=': out += QChar(0x208C); break;
        case '(': out += QChar(0x208D); break;
        case ')': out += QChar(0x208E); break;
        case 'a': out += QChar(0x2090); break;
        case 'e': out += QChar(0x2091); break;
        case 'h': out += QChar(0x2095); break;
        case 'i': out += QChar(0x1D62); break;
        case 'j': out += QChar(0x2C7C); break;
        case 'k': out += QChar(0x2096); break;
        case 'l': out += QChar(0x2097); break;
        case 'm': out += QChar(0x2098); break;
        case 'n': out += QChar(0x2099); break;
        case 'o': out += QChar(0x2092); break;
        case 'p': out += QChar(0x209A); break;
        case 'r': out += QChar(0x1D63); break;
        case 's': out += QChar(0x209B); break;
        case 't': out += QChar(0x209C); break;
        case 'u': out += QChar(0x1D64); break;
        case 'v': out += QChar(0x1D65); break;
        case 'x': out += QChar(0x2093); break;
        default: out += ch; break;
        }
    }
    return out;
}

QString mapToUnicodeSuperscript(const QString &src)
{
    QString out;
    out.reserve(src.size());
    for (QChar ch : src) {
        switch (ch.unicode()) {
        case '0': out += QChar(0x2070); break;
        case '1': out += QChar(0x00B9); break;
        case '2': out += QChar(0x00B2); break;
        case '3': out += QChar(0x00B3); break;
        case '4': out += QChar(0x2074); break;
        case '5': out += QChar(0x2075); break;
        case '6': out += QChar(0x2076); break;
        case '7': out += QChar(0x2077); break;
        case '8': out += QChar(0x2078); break;
        case '9': out += QChar(0x2079); break;
        case '+': out += QChar(0x207A); break;
        case '-': out += QChar(0x207B); break;
        case '=': out += QChar(0x207C); break;
        case '(': out += QChar(0x207D); break;
        case ')': out += QChar(0x207E); break;
        case 'i': out += QChar(0x2071); break;
        case 'n': out += QChar(0x207F); break;
        default: out += ch; break;
        }
    }
    return out;
}

bool tableHasPlottableXY(Table *t)
{
    if (!t || t->numCols() < 2 || t->noXColumn() || t->noYColumn())
        return false;
    const int nCols = t->numCols();
    for (int c = 0; c < nCols; ++c) {
        if (t->colPlotDesignation(c) != SciDAVis::Y)
            continue;
        Column *yc = t->column(c);
        const int xIdx = t->colX(c);
        Column *xc = (xIdx >= 0) ? t->column(xIdx) : nullptr;
        if (!yc || !xc)
            continue;
        const int rows = qMin(yc->rowCount(), xc->rowCount());
        for (int r = 0; r < rows; ++r) {
            if (yc->isInvalid(r) || xc->isInvalid(r))
                continue;
            const bool yOk = (yc->columnMode() == SciDAVis::ColumnMode::Numeric)
                    ? std::isfinite(yc->valueAt(r))
                    : !yc->textAt(r).trimmed().isEmpty();
            if (!yOk)
                continue;
            const bool xOk = (xc->columnMode() == SciDAVis::ColumnMode::Numeric)
                    ? std::isfinite(xc->valueAt(r))
                    : !xc->textAt(r).trimmed().isEmpty();
            if (xOk)
                return true;
        }
    }
    return false;
}

QString detectSmartAsciiSeparator(const QString &filePath, const QString &fallbackSep)
{
    QFile f(filePath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray sample = f.read(4096);
        f.close();
        const QList<QByteArray> lines = sample.split('\n');
        for (const QByteArray &raw : lines) {
            const QByteArray ln = raw.trimmed();
            if (ln.isEmpty() || ln.startsWith('#') || ln.startsWith('%'))
                continue;
            if (ln.contains('\t'))
                return QStringLiteral("\t");
            if (ln.contains(','))
                return QStringLiteral(",");
            if (ln.contains(';'))
                return QStringLiteral(";");
            break;
        }
    }
    if (filePath.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive))
        return QStringLiteral(",");
    if (filePath.endsWith(QLatin1String(".tsv"), Qt::CaseInsensitive))
        return QStringLiteral("\t");
    return fallbackSep.isEmpty() ? QStringLiteral("\t") : fallbackSep;
}

class ActivePageStackedWidget : public QStackedWidget
{
public:
    explicit ActivePageStackedWidget(QWidget *parent = nullptr) : QStackedWidget(parent) { }

    QSize sizeHint() const override
    {
        if (QWidget *w = currentWidget())
            return w->sizeHint();
        return QStackedWidget::sizeHint();
    }

    QSize minimumSizeHint() const override
    {
        if (QWidget *w = currentWidget()) {
            const QSize ms = w->minimumSizeHint();
            return QSize(qMin(ms.width(), 220), ms.height());
        }
        return QStackedWidget::minimumSizeHint();
    }
};

static void registerInspectorExtraTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("Overall", "全体");
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("GraphInspectorDock"), QStringLiteral("Axis"),
                QString::fromUtf8("軸"));
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("GraphInspectorDock"), QStringLiteral("Free"),
                QString::fromUtf8("自由"));
        reg("Slide (18pt)", "スライド (18pt)");
        reg("Unlock Aspect Ratio (Free Resize)", "縦横比の固定を解除 (自由サイズ変更)");
        reg("Open the Add Error Bars dialog to select an error column or percentage",
            "エラーバーの追加ダイアログを開き、誤差列や割合を指定します");
        reg("Scope:", "対象:");
        reg("Active Layer", "現在のレイヤー");
        reg("All Panels", "全パネル一括");
        reg("Apply Axis/Frame/Grid edits only to the active layer",
            "軸・枠・グリッドの変更を現在のレイヤーのみに適用します");
        reg("Apply Axis/Frame/Grid edits to all primary panels at once",
            "軸・枠・グリッドの変更を全パネルへ一括適用します");
        reg("Reverse Axis", "軸を反転");
        reg("Minor Ticks:", "補助目盛り:");
        reg("0 (None)", "0 (なし)");
        reg("1 (2 subdiv)", "1 (2分割)");
        reg("4 (5 subdiv)", "4 (5分割)");
        reg("9 (10 subdiv)", "9 (10分割)");
        reg("%1 (%2 subdiv)", "%1 (%2分割)");
        reg("Format:", "数値表記:");
        reg("Automatic", "自動");
        reg("Decimal (0.00)", "小数 (0.00)");
        reg("Scientific (10ˣ)", "指数表記 (10ˣ)");
        reg("Scientific (1E3)", "科学表記 (1E3)");
        reg("Engineering", "工学単位換算 (10³)");
        reg("Precision:", "桁数:");
        reg("Auto", "自動");
        reg("Divide by:", "除算係数:");
        reg("Change Tick Format", "目盛り数値表記の変更");
        reg("Change Tick Divide Factor", "目盛り除算係数の変更");
        reg("Apply Font Size to All 4 Axes", "全4軸のフォントサイズを連動変更");
        reg("Units / Symbols", "単位・記号");
        reg("Units", "単位");
        reg("Insert Symbol, Unit, or Journal Axis Template",
            "記号・単位・論文用軸タイトル定型を入力");
        reg("Symbols", "科学記号");
        reg("Journal Templates", "論文用軸タイトル定型");
        reg("Common Units", "よく使う単位");
        reg("Spline", "スプライン曲線");
        reg("Horizontal Steps", "水平ステップ");
        reg("Vertical Steps", "垂直ステップ");
        reg("Drop Lines", "ドロップライン");
        reg("Opacity:", "不透明度:");
        reg("Change Curve Opacity", "曲線の不透明度変更");
        reg("Link Symbol Color to Line", "シンボルの色を線の色に連動");
        reg("Edge Color:", "枠線の色:");
        reg("Fill Color:", "塗り色:");
        reg("Edge Width:", "枠線の太さ:");
        reg("Select Symbol Edge Color", "シンボルの枠線色を選択");
        reg("Select Symbol Fill Color", "シンボルの塗り色を選択");
        reg("Change Symbol Edge Color", "シンボルの枠線色変更");
        reg("Change Symbol Fill Color", "シンボルの塗り色変更");
        reg("Change Symbol Edge Width", "シンボルの枠線幅変更");
        reg("Link Symbol Color", "シンボル色を線の色に連動");
        reg("Plot on Right-Y", "右Y軸にプロット");
        reg("Match Right-Y Color", "右Y軸の色を曲線に合わせる");
        reg("Color Right-Y axis backbone, ticks, and title to match this curve",
            "右Y軸の軸線・目盛り・軸タイトル色をこの曲線の色に合わせます");
        reg("Match Right-Y Axis Color", "右Y軸の色を曲線に合わせる");
        reg("Cycle Symbol Shapes (● ■ ▲ ▼ ◆)", "シンボル形状を順に切替 (● ■ ▲ ▼ ◆)");
        reg("Paired Filled / Hollow (● ○ ■ □)", "塗り・白抜きペア切替 (● ○ ■ □)");
        reg("Error Bar Style", "エラーバー外観");
        reg("More below", "下に続きあり");
        reg("Bar Width:", "線の太さ:");
        reg("Cap Length:", "キャップ長:");
        reg("Direction:", "方向:");
        reg("Both (±)", "両方向 (±)");
        reg("Plus (+)", "正方向のみ (+)");
        reg("Minus (-)", "負方向のみ (-)");
        reg("Match Curve", "曲線色に合わせる");
        reg("Select Error Bar Color", "エラーバーの色を選択");
        reg("Change Error Bar Width", "エラーバー太さの変更");
        reg("Change Error Bar Cap", "エラーバーのキャップ長変更");
        reg("Change Error Bar Direction", "エラーバー方向の変更");
        reg("Change Error Bar Color", "エラーバー色の変更");
        reg("Remove Error Bars", "エラーバーの削除");
        reg("Toggle Error Bar Visibility", "エラーバー表示の切替");
        reg("±X Err: %1", "±X誤差: %1");
        reg("±Y Err: %1", "±Y誤差: %1");
        reg("Heatmap: %1", "ヒートマップ: %1");
        reg("Heatmap", "ヒートマップ");
        reg("Color Scale & Contours", "カラースケール・等高線");
        reg("Colormap:", "カラーマップ:");
        reg("Reverse Colormap", "カラーマップを反転");
        reg("Show Colorbar", "カラーバーを表示");
        reg("Colorbar Width:", "カラーバー幅:");
        reg("Show Contour Lines", "等高線を表示");
        reg("Contour Levels:", "等高線レベル数:");
        reg("Custom Z Range", "Z範囲を手動指定");
        reg("Z Min:", "Z最小値:");
        reg("Z Max:", "Z最大値:");
        reg("Change Heatmap Colormap", "カラーマップの変更");
        reg("Toggle Colorbar", "カラーバー表示の切替");
        reg("Change Colorbar Width", "カラーバー幅の変更");
        reg("Toggle Contour Lines", "等高線表示の切替");
        reg("Change Contour Levels", "等高線レベル数の変更");
        reg("Change Heatmap Z Range", "ヒートマップZ範囲の変更");
        reg("Toggle Heatmap Visibility", "ヒートマップ表示の切替");
        reg("Default", "標準");
        reg("GrayScale", "グレースケール");
        reg("Blue-White-Red", "青-白-赤");
        reg("Cool-Warm", "クール-ウォーム");
        reg("Matplotlib (tab10)", "Matplotlib 既定色 (tab10)");
        reg("+ Link Top-X Axis (Arrhenius)", "+ 上X軸リンク (アレニウス等)");
        reg("Show Tick Numbers", "目盛り数値を表示");
        reg("Show Axis", "この軸を表示");
        reg("Tick Labels", "目盛り数値を表示");
        reg("Toggle Tick Numbers", "目盛り数値の表示切替");
        reg("Unit Format:", "単位表記:");
        reg("Unit Format", "単位表記");
        reg("Name (Unit)", "量名 (単位)");
        reg("Name / Unit (ISIJ)", "物理量 / 単位 (ISIJ)");
        reg("Name [Unit]", "量名 [単位]");
        reg("Change Unit Format", "単位表記の変更");
        reg("Symbol Skip:", "シンボルの間引き:");
        reg("Symbol Skip", "シンボルの間引き");
        reg("Skip symbols to avoid crowding on dense data (0 = Auto, 1 = All points, N = show 1 in N)",
            "データ点が多くてシンボルが重なるときに間引いて表示します (0 = 自動、1 = 全点、N = N点ごとに1個表示)");
        reg("Change Symbol Skip", "シンボルの間引き間隔変更");
        reg("Colorbar Title:", "カラーバータイトル:");
        reg("Colorbar Title", "カラーバータイトル");
        reg("Enter colorbar title...", "カラーバータイトルを入力...");
        reg("Edit Colorbar Title", "カラーバータイトルの編集");
        reg("Quick Start Guide", "クイックスタートガイド");
        reg("Create publication-ready graphs in 3 easy steps",
            "3ステップで論文品質のグラフを作成できます");
        reg("Enter data in Table or import CSV",
            "ワークシートにデータを入力、またはCSVを読込");
        reg("Click 'Line+Sym' on the Worksheet bar, or pick a graph type from the 'Plot' tab",
            "ワークシート上部の「線+シンボル」、または上部の「作図」タブからグラフ種類を選択");
        reg("Enter X and Y numeric data in the worksheet first to plot",
            "ワークシートに X 列と Y 列の数値を入力すると作図できます");
        reg("Guide (F1)", "ガイド (F1)");
        reg("Open the ToriGraph Quick Guide (F1)", "ToriGraph クイックガイドを開きます (F1)");
        reg("Drag graph frame to resize; click axes or legend to edit",
            "グラフの枠ドラッグでサイズ変更・クリックで軸や凡例を編集");
        reg("Import CSV / Data...", "CSV / データ読込...");
        reg("New 2D Graph", "新規2Dグラフ");
        reg("Sample Gallery / Plot", "サンプル作図ギャラリー");
        reg("Aspect Ratio & Resizing", "縦横比とリサイズ");
        reg("Lock Aspect Ratio", "縦横比を固定");
        reg("1:1 Square", "1:1 正方形");
        reg("4:3 Landscape", "4:3 横長");
        reg("16:9 Wide", "16:9 ワイド");
        reg("3:4 Portrait", "3:4 縦長");
        reg("Free Resize (Unlock Aspect Ratio)", "自由リサイズ (縦横比ロック解除)");
        reg("Toggle Aspect Ratio Lock", "縦横比固定の切替");
        reg("Apply Aspect Ratio Preset", "縦横比プリセット適用");
        // Clean markup tooltips (no raw HTML tags)
        reg("Subscript (X\xE2\x82\x82)", "下付き文字 (X\xE2\x82\x82)");
        reg("Superscript (X\xC2\xB2)", "上付き文字 (X\xC2\xB2)");
        reg("Italic (slanted)", "斜体 (イタリック)");
        // A5-2: 5 missing symbol menu items
        reg("\xC2\xB1 (plus/minus)", "\xC2\xB1 (プラスマイナス)");
        reg("\xC3\x97 (times)", "\xC3\x97 (乗算記号)");
        reg("\xC2\xB7 (cdot)", "\xC2\xB7 (中黒・積)");
        reg("\xC2\xB0 (degree)", "\xC2\xB0 (度)");
        reg("\xC3\x85 (Angstrom)", "\xC3\x85 (オングストローム)");
        // Page unit, Fit to Window, and Inset Layer controls
        reg("Unit:", "単位:");
        reg("Fit", "全体表示");
        reg("Fit graph page to the current window viewport",
            "グラフページを現在のウィンドウ幅に合わせて表示します");
        reg("Inset Zoom Region", "拡大インセット範囲");
        reg("X Min:", "X 最小:");
        reg("X Max:", "X 最大:");
        reg("Y Min:", "Y 最小:");
        reg("Y Max:", "Y 最大:");
        reg("Show Connector Lines", "引出線を表示");
        reg("Toggle Inset Connectors", "インセット引出線の切替");
        reg("Edit Inset Zoom Region", "拡大インセット範囲の編集");
        reg("[Inset]", "[インセット]");
        // Axis title live preview
        reg("Preview: %1", "プレビュー: %1");
        reg("Preview: (empty)", "プレビュー: (空)");
        // Right-Y cleanup (B1-5)
        reg("Enable Right-Y Axis", "右Y軸を有効化");
        reg("A linked Right-Y overlay layer already owns the right Y axis. Add or edit curves on that layer instead.",
            "リンク済みの右Y軸オーバーレイレイヤーが右Y軸を使用しています。そのレイヤー上で曲線を編集してください。");
        // Spectrogram Log / Cell blocks (B2-7)
        reg("Log Color Scale", "対数カラースケール");
        reg("Cell Blocks (No Smoothing)", "セルブロック表示 (平滑化なし)");
        reg("Toggle Log Color Scale", "対数カラースケールの切替");
        reg("Toggle Cell Blocks", "セルブロック表示の切替");
        // Per-curve legend labels & rebuild confirmation (A3-4)
        reg("Curve Legend Labels", "凡例の各曲線ラベル");
        reg("Reset to automatic curve name", "自動の曲線名に戻す");
        reg("Edit Legend Label", "凡例ラベルの編集");
        reg("Rebuild Legend", "凡例を再構築");
        reg("Rebuilding the legend will overwrite your custom legend labels and header. Continue?",
            "凡例を再構築すると、手編集した凡例ラベルや見出しが上書きされます。続けますか?");
        // Arrow / Image / Plot Title editors (A3-5, C2-3)
        reg("Arrow / Line Annotation", "矢印・直線注釈");
        reg("Arrow Style", "矢印スタイル");
        reg("Width:", "幅:");
        reg("Arrow at Start", "始点に矢尻");
        reg("Arrow at End", "終点に矢尻");
        reg("Head Length:", "矢尻の長さ:");
        reg("Head Angle:", "矢尻の角度:");
        reg("Filled Head", "矢尻を塗りつぶす");
        reg("Select Arrow Color", "矢印の色を選択");
        reg("Change Arrow Style", "矢印スタイルの変更");
        reg("Image Annotation", "画像注釈");
        reg("Image Geometry", "画像の位置とサイズ");
        reg("File: %1", "ファイル: %1");
        reg("File: (none)", "ファイル: (なし)");
        reg("X:", "X:");
        reg("Y:", "Y:");
        reg("Replace Image...", "画像を差し替え...");
        reg("Select Replacement Image", "差し替える画像を選択");
        reg("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.gif *.svg);;All Files (*)",
            "画像ファイル (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.gif *.svg);;すべてのファイル (*)");
        reg("Replace Image", "画像の差し替え");
        reg("Change Image Geometry", "画像位置・サイズの変更");
        reg("Plot Title", "グラフタイトル");
        reg("Plot Title: %1", "グラフタイトル: %1");
        reg("Title Appearance", "グラフタイトルの書式");
        reg("Title Text:", "タイトル文字:");
        reg("Enter plot title...", "グラフタイトルを入力...");
        reg("Bold", "太字");
        reg("Text Color:", "文字色:");
        reg("Select Plot Title Color", "グラフタイトル色を選択");
        reg("Edit Plot Title", "グラフタイトルの編集");
        reg("Change Plot Title Font", "グラフタイトルフォントの変更");
        reg("Change Plot Title Color", "グラフタイトル色の変更");
        // Color swatch popup (A3-6)
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("GraphInspectorDock"), QStringLiteral("ToriGraph Standard"),
                QString::fromUtf8("ToriGraph 標準"));
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("GraphInspectorDock"), QStringLiteral("ISIJ 1-Col (85 mm)"),
                QString::fromUtf8("ISIJ 1段 (85 mm)"));
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("GraphInspectorDock"), QStringLiteral("Large Slide (185 mm)"),
                QString::fromUtf8("大判スライド (185 mm)"));
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("GraphInspectorDock"), QStringLiteral("Journal 2-Col (175 mm)"),
                QString::fromUtf8("論文 2 段幅 (175 mm)"));
        reg("Okabe-Ito", "Okabe-Ito (色覚バリアフリー)");
        reg("Basic & Grayscale", "基本色・グレースケール");
        reg("Custom Color...", "カスタム色...");
        // Object Manager context menu actions (B1-9)
        reg("Copy Axis Scale Range", "軸スケール範囲をコピー");
        reg("Paste Axis Scale Range", "軸スケール範囲を貼り付け");
        reg("Copy Curve Color & Symbol", "曲線の色とシンボルをコピー");
        reg("Paste Curve Color & Symbol", "曲線の色とシンボルを貼り付け");
        reg("Edit X/Y Data Columns...", "X/Y データ列を編集...");
        reg("Edit Data Row Range...", "データ行範囲を編集...");
        reg("Edit Axis Properties...", "軸の詳細設定...");
        reg("Add/Remove Curves...", "曲線の追加・削除...");
        reg("Show/Hide Object", "オブジェクト表示の切替");
        reg("Paste Curve Style", "曲線の色とシンボルを貼り付け");
        reg("Enter values and press Enter to apply (amber highlight = unsaved edit)",
            "値を入力して Enter で反映します (黄色の枠は未確定の編集)");
        // H3: registered here too, because MultiLayer registers it lazily and the
        // inspector sets its button text before that happens.
        reg("Manage Style Presets...", "グラフプリセットの編集・追加...");
        // H7: tooltips
        reg("Use the column to the right of the Y column as ± error bars (click again to remove)",
            "Y 列のすぐ右の列を ± エラーバーとして使います (もう一度押すと外します)");
        reg("Add the standard deviation of all Y values as the same ± error bar on every point "
            "(click again to remove)",
            "Y 列全体の標準偏差を、すべての点に同じ ± エラーバーとして付けます (もう一度押すと外します)");
        reg("Add ± 5% of each Y value as error bars (click again to remove)",
            "各点の Y 値の ± 5% をエラーバーとして付けます (もう一度押すと外します)");
        reg("Move the legend to the %1 corner inside the plot frame",
            "凡例を枠内の%1へ移動します");
        reg("Move the legend to the top centre inside the plot frame", "凡例を枠内の上中央へ移動します");
        reg("Move the legend outside the plot frame, to the right of the layer",
            "凡例を枠の外 (レイヤーの右側) へ移動します");
        reg("Move the legend to the corner or edge position that overlaps the data least",
            "データとの重なりが最も少ない位置 (四隅か上下の中央) へ凡例を移動します");
        reg("Rebuild the legend text from the current curves (custom text is replaced)",
            "今の曲線から凡例の文字を作り直します (書き換えた文字は置き換わります)");
        reg("Click to choose a color (now %1)", "クリックして色を選びます (現在 %1)");
        // C2: "Details..." / its tooltip are also registered by ToriUx::registerUxTranslations(),
        // which skips words that already have a translation ("Details" -> "詳細..."). Pin the
        // inspector's wording for this context so it does not depend on registration order.
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("GraphInspectorDock"), QStringLiteral("Details..."),
                QString::fromUtf8("詳細設定…"));
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("GraphInspectorDock"),
                QStringLiteral("Open the dialog with every setting for this section"),
                QString::fromUtf8("この節のすべての設定があるダイアログを開く"));
        reg("Open the dialog with every setting for this section",
            "この節のすべての設定があるダイアログを開く");
        reg("Color %1", "色 %1");
        reg("Edit the text. Each line of a multi-line label is one line here.",
            "文字を編集します。複数行のテキストは、ここでも 1 行ずつ改行して書きます。");
        reg("Number of minor ticks between major ticks. On a log axis only 0, 2, 4 or 8 "
            "are meaningful (8 = 2, 3, ..., 9).",
            "主目盛りの間の補助目盛りの数。対数軸では 0・2・4・8 のどれか"
            "(8 は 2, 3, ..., 9 の位置) を選びます。");
    });
}

static QString minorTickItemLabel(int n)
{
    if (n <= 0)
        return QCoreApplication::translate("GraphInspectorDock", "0 (None)");
    return QCoreApplication::translate("GraphInspectorDock", "%1 (%2 subdiv)").arg(n).arg(n + 1);
}

} // namespace

namespace GraphInspectorInternal {

//! Strip scientific HTML formatting tags (<i>, <b>, <u>, <sub>, <sup>, <span>, <font>, <br>)
//! into clean readable Unicode text for tree items, combos, labels, and tooltips (A3-2).
QString stripHtmlToCleanUnicode(const QString &raw)
{
    if (raw.isEmpty())
        return raw;
    if (!raw.contains(QLatin1Char('<')) && !raw.contains(QLatin1Char('&')))
        return raw;

    QString s = raw;
    static const QRegularExpression rxSub(QStringLiteral("<sub\\b[^>]*>(.*?)</sub>"),
                                          QRegularExpression::CaseInsensitiveOption
                                                  | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression rxSup(QStringLiteral("<sup\\b[^>]*>(.*?)</sup>"),
                                          QRegularExpression::CaseInsensitiveOption
                                                  | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression rxBr(QStringLiteral("<br\\s*/?>"),
                                         QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rxSafeTags(
            QStringLiteral("</?(?:i|b|u|em|strong|span|font|sub|sup|p|div|nobr)(?:\\s+[^>]*)?>"),
            QRegularExpression::CaseInsensitiveOption);

    // Replace <sub>...</sub> and <sup>...</sup> with clean Unicode characters.
    QRegularExpressionMatch m;
    while ((m = rxSub.match(s)).hasMatch()) {
        const QString inner = stripHtmlToCleanUnicode(m.captured(1));
        s.replace(m.capturedStart(), m.capturedLength(), mapToUnicodeSubscript(inner));
    }
    while ((m = rxSup.match(s)).hasMatch()) {
        const QString inner = stripHtmlToCleanUnicode(m.captured(1));
        s.replace(m.capturedStart(), m.capturedLength(), mapToUnicodeSuperscript(inner));
    }
    s.replace(rxBr, QStringLiteral(" "));
    s.remove(rxSafeTags);
    s.replace(QLatin1String("&nbsp;"), QLatin1String(" "));
    s.replace(QLatin1String("&deg;"), QString::fromUtf8("\xC2\xB0"));
    s.replace(QLatin1String("&mu;"), QString::fromUtf8("\xCE\xBC"));
    s.replace(QLatin1String("&plusmn;"), QString::fromUtf8("\xC2\xB1"));
    s.replace(QLatin1String("&times;"), QString::fromUtf8("\xC3\x97"));
    s.replace(QLatin1String("&lt;"), QLatin1String("<"));
    s.replace(QLatin1String("&gt;"), QLatin1String(">"));
    s.replace(QLatin1String("&amp;"), QLatin1String("&"));
    return s;
}

void setLineEditPendingCue(QLineEdit *ed, bool pending)
{
    if (!ed)
        return;
    if (ed->property("pendingEdit").toBool() == pending)
        return;
    ed->setProperty("pendingEdit", pending);
    if (QStyle *st = ed->style()) {
        st->unpolish(ed);
        st->polish(ed);
    }
    ed->update();
}

void wirePendingEditCue(QLineEdit *ed)
{
    if (!ed)
        return;
    QObject::connect(ed, &QLineEdit::textEdited, ed, [ed](const QString &txt) {
        const QString synced = ed->property("_syncedText").toString();
        const bool dirty = ed->isModified() && (txt.trimmed() != synced);
        setLineEditPendingCue(ed, dirty);
    });
}

bool isUsableInspectorTable(ApplicationWindow *app, MyWidget *w)
{
    Table *tbl = qobject_cast<Table *>(w);
    if (!app || !tbl)
        return false;
    if (tbl->name().startsWith(QLatin1String("RpcFitCurve")))
        return false;
    const bool isHidden = (tbl->status() == MyWidget::Hidden || app->hiddenWindows.contains(tbl));
    if (!isHidden)
        return true;
    WorkbookManager *wb = WorkbookManager::existing(app);
    if (!wb)
        return false;
    const QString book = wb->bookOf(tbl);
    if (book.isEmpty())
        return false;
    Table *cur = wb->currentSheet(book);
    return cur && cur->status() != MyWidget::Hidden && !app->hiddenWindows.contains(cur);
}

void syncLinkedChildrenOf(MultiLayer *ml, Graph *parentG)
{
    if (!ml || !parentG)
        return;
    for (Graph *cand : ml->layersList()) {
        if (cand && cand->linkedParentLayer() == parentG)
            cand->syncLinkedAxesFromParent();
    }
}

Spectrogram *effectiveSelectedSpectrogram(Graph *g, int *outIdx)
{
    if (outIdx)
        *outIdx = -1;
    if (!g)
        return nullptr;
    const Graph::GraphSelection sel = g->currentSelection();
    if (sel.kind == Graph::GraphSelection::Spectrogram && sel.primaryId >= 0
        && sel.primaryId < g->curves()) {
        if (auto *sp = dynamic_cast<Spectrogram *>(g->plotItem(sel.primaryId))) {
            if (outIdx)
                *outIdx = sel.primaryId;
            return sp;
        }
    }
    // Fallback if layer has a spectrogram and no regular QwtPlotCurves
    bool hasRegularCurve = false;
    Spectrogram *firstSpec = nullptr;
    int firstSpecIdx = -1;
    for (int i = 0; i < g->curves(); ++i) {
        if (g->curve(i) && g->curveType(i) != Graph::ErrorBars) {
            hasRegularCurve = true;
        } else if (!firstSpec) {
            if (auto *sp = dynamic_cast<Spectrogram *>(g->plotItem(i))) {
                firstSpec = sp;
                firstSpecIdx = i;
            }
        }
    }
    if (!hasRegularCurve && firstSpec) {
        if (outIdx)
            *outIdx = firstSpecIdx;
        return firstSpec;
    }
    return nullptr;
}

QVector<QwtErrorPlotCurve *> effectiveSelectedErrorBars(Graph *g)
{
    QVector<QwtErrorPlotCurve *> result;
    if (!g)
        return result;
    const Graph::GraphSelection sel = g->currentSelection();
    if (sel.kind == Graph::GraphSelection::ErrorBars && sel.primaryId >= 0
        && sel.primaryId < g->curves()) {
        if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(g->curve(sel.primaryId))) {
            result.append(er);
            return result;
        }
    }
    int ci = -1;
    if (sel.kind == Graph::GraphSelection::Curve)
        ci = sel.primaryId;
    else
        ci = g->selectedCurveIndex();
    if (ci < 0 || ci >= g->curves() || g->curveType(ci) == Graph::ErrorBars) {
        for (int k = 0; k < g->curves(); ++k) {
            if (g->curve(k) && g->curveType(k) != Graph::ErrorBars) {
                ci = k;
                break;
            }
        }
    }
    if (ci >= 0 && ci < g->curves()) {
        if (auto *dc = dynamic_cast<DataCurve *>(g->curve(ci))) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(eb))
                    result.append(er);
            }
        }
    }
    return result;
}

QString friendlyCurveDisplayName(Graph *g, int curveIdx)
{
    if (!g || curveIdx < 0 || curveIdx >= g->curves())
        return QString();
    QwtPlotCurve *cv = g->curve(curveIdx);
    if (!cv)
        return QString::number(curveIdx + 1);

    QString label;
    const QString rich = g->resolvedAutoCurveLabel(curveIdx).trimmed();
    if (!rich.isEmpty()) {
        label = rich;
    } else {
        label = cv->title().text();
        if (DataCurve *dc = dynamic_cast<DataCurve *>(cv)) {
            if (Table *t = dc->table()) {
                const int yIdx = t->colIndex(dc->yColumnName());
                Column *yc = (yIdx >= 0) ? t->column(yIdx) : nullptr;
                if (yc && !yc->longName().trimmed().isEmpty()) {
                    label = yc->longName().trimmed();
                } else {
                    const QString pfx = QString(t->name()) + QStringLiteral("_");
                    if (label.startsWith(pfx) && label.size() > pfx.size())
                        label = label.mid(pfx.size());
                }
            }
        }
    }
    label = stripHtmlToCleanUnicode(label).trimmed();
    if (cv->yAxis() == QwtPlot::yRight)
        label += QCoreApplication::translate("GraphInspectorDock", " [R-Y]");
    return label;
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

int effectiveSelectedCurveIndex(Graph *g, bool fallbackToFirstDataCurve)
{
    if (!g)
        return -1;
    int ci = -1;
    const Graph::GraphSelection sel = g->currentSelection();
    if (sel.kind == Graph::GraphSelection::Curve
        || sel.kind == Graph::GraphSelection::ErrorBars) {
        ci = resolveMasterCurveIndex(g, sel.primaryId);
    } else {
        ci = resolveMasterCurveIndex(g, g->selectedCurveIndex());
    }
    if ((ci < 0 || ci >= g->curves() || g->curveType(ci) == Graph::ErrorBars)
        && fallbackToFirstDataCurve && g->curves() > 0) {
        for (int k = 0; k < g->curves(); ++k) {
            if (g->curveType(k) != Graph::ErrorBars) {
                ci = k;
                break;
            }
        }
    }
    if (ci < 0 || ci >= g->curves() || g->curveType(ci) == Graph::ErrorBars)
        return -1;
    return ci;
}

double pageUnitToMmFactor(MultiLayer::PageUnit unit)
{
    switch (unit) {
    case MultiLayer::UnitCm:
        return 10.0;
    case MultiLayer::UnitInch:
        return 25.4;
    case MultiLayer::UnitPx:
        return 25.4 / 96.0;
    case MultiLayer::UnitMm:
    default:
        return 1.0;
    }
}

bool isLegendEntryLine(const QString &line)
{
    const QString t = line.trimmed();
    return t.startsWith(QStringLiteral("\\c{")) || t.startsWith(QStringLiteral("\\l("))
            || t.startsWith(QStringLiteral("\\p{"));
}

void updateColorSwatchButton(QPushButton *btn, const QColor &c)
{
    if (!btn)
        return;
    const QColor safe = c.isValid() ? c : QColor(Qt::black);
    const double lum = 0.2126 * safe.redF() + 0.7152 * safe.greenF() + 0.0722 * safe.blueF();
    const QString fgHex = (lum > 0.58) ? QStringLiteral("#0F172A") : QStringLiteral("#FFFFFF");
    const QColor borderCol = (lum > 0.88) ? QColor(0xCB, 0xD5, 0xE1) : safe.darker(118);
    const QString bgHex = safe.name(QColor::HexRgb).toUpper();
    const QString borderHex = borderCol.name(QColor::HexRgb).toUpper();

    btn->setObjectName(QStringLiteral("InspectorColorSwatchBtn"));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setMinimumHeight(24);
    btn->setMaximumHeight(26);
    btn->setIcon(QIcon());
    btn->setText(bgHex);
    // H7: a swatch is only a coloured rectangle, so say what clicking it does. Buttons that
    // were given a specific tooltip elsewhere keep it.
    const QString autoTip = btn->property("autoSwatchTip").toString();
    if (btn->toolTip().isEmpty() || btn->toolTip() == autoTip) {
        const QString tip =
                QCoreApplication::translate("GraphInspectorDock", "Click to choose a color (now %1)")
                        .arg(bgHex);
        btn->setToolTip(tip);
        btn->setProperty("autoSwatchTip", tip);
    }
    btn->setAccessibleName(QCoreApplication::translate("GraphInspectorDock", "Color %1").arg(bgHex));
    btn->setStyleSheet(QStringLiteral(
                               "QPushButton#InspectorColorSwatchBtn {"
                               "  background-color: %1;"
                               "  color: %2;"
                               "  border: 1px solid %3;"
                               "  border-radius: 5px;"
                               "  font-family: 'Consolas', 'Segoe UI', monospace;"
                               "  font-size: 9pt;"
                               "  font-weight: 600;"
                               "  padding: 0px 8px;"
                               "  min-height: 24px;"
                               "  max-height: 24px;"
                               "}"
                               "QPushButton#InspectorColorSwatchBtn:hover {"
                               "  border: 2px solid #2563EB;"
                               "}"
                               "QPushButton#InspectorColorSwatchBtn:disabled {"
                               "  background-color: #F1F5F9;"
                               "  color: #5B6B80;"
                               "  border: 1px solid #E2E8F0;"
                               "}")
                               .arg(bgHex, fgHex, borderHex));
    btn->setProperty("swatchColor", safe);
}

// Fills the minor-tick combo with the presets for a linear or log10 axis (shared with
// AxesDialog through PlotStyleLimits) and selects `current`, adding it when it is not a preset.
void fillMinorTickCombo(QComboBox *cb, bool log10Axis, int current)
{
    if (!cb)
        return;
    QSignalBlocker blocker(cb);
    cb->clear();
    for (int v : PlotStyleLimits::minorTickChoices(log10Axis))
        cb->addItem(minorTickItemLabel(v), v);
    cb->setProperty("minorLogMode", log10Axis);
    const int value = qBound(0, current, PlotStyleLimits::kMinorTicksMax);
    int idx = cb->findData(value);
    if (idx < 0) {
        cb->addItem(minorTickItemLabel(value), value);
        idx = cb->count() - 1;
    }
    cb->setCurrentIndex(idx);
}

// Value of the (editable) minor-tick combo: the selected preset, or the leading number of a
// typed-in text. Returns `fallback` when nothing usable is entered.
int minorTickComboValue(const QComboBox *cb, int fallback)
{
    if (!cb)
        return fallback;
    const int idx = cb->currentIndex();
    if (idx >= 0 && cb->itemText(idx) == cb->currentText())
        return cb->itemData(idx).toInt();
    const QString token = cb->currentText().trimmed().section(QLatin1Char(' '), 0, 0);
    bool ok = false;
    const int v = token.toInt(&ok);
    return ok ? qBound(0, v, PlotStyleLimits::kMinorTicksMax) : fallback;
}

} // namespace GraphInspectorInternal

using namespace GraphInspectorInternal;

GraphInspectorDock::GraphInspectorDock(ApplicationWindow *app, QWidget *parent)
    : QDockWidget(parent), d_app(app)
{
    registerInspectorExtraTranslations();

    setObjectName(QStringLiteral("GraphInspectorDock"));
    setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    setFeatures(QDockWidget::NoDockWidgetFeatures);

    // Hide native OS title bar so our compact header & 32px vertical rail render seamlessly
    QWidget *emptyTitle = new QWidget(this);
    emptyTitle->setFixedHeight(0);
    setTitleBarWidget(emptyTitle);

    d_coalesce_refresh_timer = new QTimer(this);
    d_coalesce_refresh_timer->setSingleShot(true);
    d_coalesce_refresh_timer->setInterval(16);
    connect(d_coalesce_refresh_timer, &QTimer::timeout, this, [this]() {
        if (!d_target_ml || d_target_ml->isRestoringSnapshot())
            return;
        for (int i = d_bound_layers.size() - 1; i >= 0; --i) {
            if (d_bound_layers.at(i).isNull())
                d_bound_layers.removeAt(i);
        }
        for (Graph *g : d_target_ml->layersList())
            bindLayerSignals(g);
        if (!d_syncing_props && !d_syncing_tree) {
            refreshObjectTree();
            syncInspectorFromSelection();
        }
    });

    buildUi();
    applyTheme();
    retranslateUi();
    setCollapsedRail(false);
    if (d_app) {
        connect(d_app, &ApplicationWindow::modified, this, [this]() {
            if (isShowingQuickStart())
                updateQuickStartPlotButtonState();
        });
    }
}

GraphInspectorDock::~GraphInspectorDock() = default;

Graph *GraphInspectorDock::activeLayer() const
{
    if (!d_target_ml)
        return nullptr;
    Graph *ag = d_target_ml->activeGraph();
    if (ag && d_target_ml->layersList().contains(ag))
        return ag;
    return d_target_ml->layer(1);
}

QVector<Graph *> GraphInspectorDock::targetLayersForScope() const
{
    QVector<Graph *> out;
    if (!d_target_ml)
        return out;
    if (d_scope_all_panels && d_target_ml->layers() > 1) {
        for (Graph *lg : d_target_ml->layersList()) {
            if (lg && !lg->linkedParentLayer())
                out.append(lg);
        }
        if (!out.isEmpty())
            return out;
    }
    if (Graph *ag = activeLayer())
        out.append(ag);
    return out;
}

void GraphInspectorDock::changeEvent(QEvent *event)
{
    QDockWidget::changeEvent(event);
    if (!event)
        return;
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        refreshAll();
    } else if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        applyTheme();
        refreshObjectTree();
    }
}

void GraphInspectorDock::closeEvent(QCloseEvent *event)
{
    // E5-2: flush any debounced text edits before closing the dock.
    if (d_annot_commit_timer && d_annot_commit_timer->isActive())
        flushPendingAnnotationEdit();
    if (d_axis_title_timer && d_axis_title_timer->isActive())
        flushPendingAxisTitleEdit();
    if (event && event->spontaneous())
        emit closeRequestedByUser();
    QDockWidget::closeEvent(event);
}

void GraphInspectorDock::resizeEvent(QResizeEvent *event)
{
    QDockWidget::resizeEvent(event);
    adjustDefaultSplitterSizes();
    updatePropScrollCue();
}

void GraphInspectorDock::adjustDefaultSplitterSizes()
{
    if (d_user_resized_splitter || !d_splitter || d_collapsed_rail)
        return;
    const int availH = d_splitter->height();
    if (availH < 200)
        return;
    const double ratio = (availH <= 560) ? 0.19 : 0.21;
    const int topH = qBound(84, qRound(availH * ratio), 165);
    const int botH = qMax(140, availH - topH);
    const QList<int> curSizes = d_splitter->sizes();
    if (curSizes.size() == 2 && qAbs(curSizes[0] - topH) <= 2 && qAbs(curSizes[1] - botH) <= 2)
        return;
    QSignalBlocker sb(d_splitter);
    d_splitter->setSizes({ topH, botH });
}

void GraphInspectorDock::updatePropScrollCue()
{
    if (!d_prop_scroll || !d_scroll_more_cue)
        return;
    QWidget *vp = d_prop_scroll->viewport();
    QScrollBar *sb = d_prop_scroll->verticalScrollBar();
    if (!vp || !sb) {
        d_scroll_more_cue->hide();
        return;
    }
    const bool hasMore = sb->maximum() > sb->minimum() && sb->value() < sb->maximum() - 2;
    d_scroll_more_cue->setVisible(hasMore && vp->height() >= 36);
}

bool GraphInspectorDock::eventFilter(QObject *obj, QEvent *event)
{
    if (event) {
        if (obj == d_edit_annot_text && event->type() == QEvent::FocusOut) {
            if (d_annot_commit_timer && d_annot_commit_timer->isActive())
                flushPendingAnnotationEdit();
        } else if (obj == d_empty_quickstart_panel && event->type() == QEvent::Show) {
            updateQuickStartPlotButtonState();
        } else if (d_prop_scroll && obj == d_prop_scroll->viewport()
                   && (event->type() == QEvent::Resize || event->type() == QEvent::Show
                       || event->type() == QEvent::LayoutRequest)) {
            updatePropScrollCue();
        } else if (obj == d_object_tree
                   && (event->type() == QEvent::ShortcutOverride
                       || event->type() == QEvent::KeyPress)) {
            if (d_object_tree && d_object_tree->hasFocus()) {
                auto *ke = static_cast<QKeyEvent *>(event);
                const Qt::KeyboardModifiers mods = ke->modifiers() & ~Qt::KeypadModifier;
                if (ke->key() == Qt::Key_Delete && mods == Qt::NoModifier) {
                    if (event->type() == QEvent::KeyPress)
                        removeSelectedObject();
                    ke->accept();
                    return true;
                }
            }
        } else if (event->type() == QEvent::FocusIn) {
            if (auto *le = qobject_cast<QLineEdit *>(obj)) {
                if (le == d_edit_leg_header) {
                    d_last_legend_focus_edit = le;
                } else {
                    for (const LegendCurveRow &row : d_leg_curve_rows) {
                        if (row.edit == le) {
                            d_last_legend_focus_edit = le;
                            break;
                        }
                    }
                }
            }
        }
    }
    return QDockWidget::eventFilter(obj, event);
}

void GraphInspectorDock::setUserPinned(bool pinned)
{
    if (d_user_pinned == pinned)
        return;
    d_user_pinned = pinned;
    if (d_btn_pin) {
        QSignalBlocker b(d_btn_pin);
        d_btn_pin->setChecked(pinned);
    }
    if (d_collapsed_rail)
        applyCollapsedGeometry();
    emit userPinToggled(pinned);
}

void GraphInspectorDock::applyCollapsedGeometry()
{
    if (d_mode_stack) {
        d_mode_stack->setCurrentIndex(d_collapsed_rail ? 1 : 0);
        d_mode_stack->updateGeometry();
    }

    if (d_collapsed_rail) {
        // Always keep a visible clickable 48px vertical rail when collapsed.
        const int targetW = 48;
        setMinimumWidth(targetW);
        setMaximumWidth(targetW);
        resize(targetW, height());
        if (d_app)
            d_app->resizeDocks({ this }, { targetW }, Qt::Horizontal);
    } else {
        setMinimumWidth(252);
        setMaximumWidth(360);
        if (width() < 252 || width() > 360)
            resize(288, height());
        if (d_app)
            d_app->resizeDocks({ this }, { 288 }, Qt::Horizontal);
        QTimer::singleShot(0, this, [this]() {
            adjustDefaultSplitterSizes();
            updatePropScrollCue();
        });
    }
}

void GraphInspectorDock::setCollapsedRail(bool collapsed)
{
    const bool changed = (d_collapsed_rail != collapsed);
    d_collapsed_rail = collapsed;
    applyCollapsedGeometry();
    if (changed)
        emit collapsedStateChanged(collapsed);
}

void GraphInspectorDock::updateQuickStartPlotButtonState()
{
    if (!d_btn_qs_new_graph)
        return;
    bool canPlot = false;
    if (d_app) {
        if (Table *activeTbl = qobject_cast<Table *>(d_app->activeWindow())) {
            canPlot = tableHasPlottableXY(activeTbl);
        } else if (QList<MyWidget *> *tblList = d_app->tableList()) {
            for (MyWidget *w : *tblList) {
                if (!isUsableInspectorTable(d_app, w))
                    continue;
                if (Table *t = qobject_cast<Table *>(w)) {
                    if (tableHasPlottableXY(t)) {
                        canPlot = true;
                        break;
                    }
                }
            }
        }
    }
    d_btn_qs_new_graph->setEnabled(canPlot);
    d_btn_qs_new_graph->setToolTip(
            canPlot ? tr("Plot the active worksheet as a Line + Symbol graph")
                    : tr("Enter X and Y numeric data in the worksheet first to plot"));
}

void GraphInspectorDock::openAxesDialogForQwtAxis(Graph *g, int qwtAxis, bool showAxesTab)
{
    if (!d_app || !g || ApplicationWindow::isNonInteractiveContext(d_app))
        return;
    MultiLayer *ml = g->multiLayerParent();
    if (!ml)
        ml = d_target_ml.data();
    if (!ml || ml->isEmpty())
        return;
    QPointer<GraphInspectorDock> self(this);
    QPointer<MultiLayer> mlGuard(ml);
    QPointer<Graph> gGuard(g);
    // C2-1: always use AxesDialog::setCurrentQwtAxis(qwtAxis) when passing a QwtPlot::Axis,
    // never setCurrentScale() which expects QwtScaleDraw::Alignment.
    QPointer<AxesDialog> ad(&d_app->addChild<AxesDialog>());
    ad->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
    ad->setGraph(g);
    if (showAxesTab)
        ad->showAxesPage();
    ad->setCurrentQwtAxis(qBound(0, qwtAxis, 3));
    ad->exec();
    if (ad)
        ad->deleteLater();
    if (!self || !mlGuard || !gGuard)
        return;
    self->refreshAll();
}

void GraphInspectorDock::switchToInspectorPage(InspectorPageIndex pageIdx)
{
    if (!d_prop_stack)
        return;
    const int idx = qBound(0, int(pageIdx), d_prop_stack->count() - 1);
    d_prop_stack->setCurrentIndex(idx);
    d_prop_stack->updateGeometry();
    if (d_page_tab_group) {
        if (QAbstractButton *btn = d_page_tab_group->button(idx)) {
            QSignalBlocker b(btn);
            btn->setChecked(true);
        }
    }
    if (d_tab_dirty[idx] && d_target_ml)
        syncTabPage(static_cast<InspectorPageIndex>(idx));
    QTimer::singleShot(0, this, &GraphInspectorDock::updatePropScrollCue);
}

GraphInspectorDock::InspectorPageIndex GraphInspectorDock::currentInspectorPage() const
{
    if (!d_prop_stack)
        return PageTabPageLayer;
    return static_cast<InspectorPageIndex>(qBound(0, d_prop_stack->currentIndex(), 3));
}

bool GraphInspectorDock::isShowingQuickStart() const
{
    return d_content_stack && d_empty_quickstart_panel
            && d_content_stack->currentWidget() == d_empty_quickstart_panel;
}

void GraphInspectorDock::setTargetMultiLayer(MultiLayer *ml, bool forceRefresh)
{
    if (d_target_ml == ml && !forceRefresh) {
        if (d_header_target_lbl && ml) {
            const QString winName = ml->name();
            if (d_header_target_lbl->text() != winName)
                d_header_target_lbl->setText(winName);
        }
        if (!ml)
            updateQuickStartPlotButtonState();
        syncInspectorFromSelection();
        return;
    }

    // E5-2: flush any pending debounced text edits BEFORE mutating d_target_ml so edits
    // commit to the original graph's own MultiLayer Undo stack and never corrupt the next graph.
    if (d_annot_commit_timer && d_annot_commit_timer->isActive())
        flushPendingAnnotationEdit();
    if (d_axis_title_timer && d_axis_title_timer->isActive())
        flushPendingAxisTitleEdit();

    if (d_coalesce_refresh_timer && d_coalesce_refresh_timer->isActive())
        d_coalesce_refresh_timer->stop();

    // E5-5: disconnect previous MultiLayer signals whenever rebinding (including forceRefresh)
    // so signal lambdas never accumulate N-times.
    if (d_target_ml && (d_target_ml != ml || forceRefresh))
        disconnect(d_target_ml, nullptr, this, nullptr);
    for (const QPointer<Graph> &gPtr : d_bound_layers) {
        if (gPtr)
            disconnect(gPtr.data(), nullptr, this, nullptr);
    }
    d_bound_layers.clear();

    d_target_ml = ml;
    d_last_legend_focus_edit = nullptr;
    if (ml)
        connectMultiLayerSignals(ml);

    refreshAll();
    if (d_collapsed_rail) {
        applyCollapsedGeometry();
    } else {
        QTimer::singleShot(0, this, [this]() {
            adjustDefaultSplitterSizes();
            updatePropScrollCue();
        });
    }
}

void GraphInspectorDock::connectMultiLayerSignals(MultiLayer *ml)
{
    if (!ml)
        return;
    connect(ml, &QObject::objectNameChanged, this, [this](const QString &newName) {
        if (!d_target_ml || newName.isEmpty())
            return;
        if (d_header_target_lbl) {
            d_header_target_lbl->setText(newName);
            const QString winLbl = d_target_ml->windowLabel().trimmed();
            const bool hasExtraLbl = !winLbl.isEmpty() && winLbl != newName && winLbl.length() <= 48
                    && !winLbl.contains(QLatin1Char('/')) && !winLbl.contains(QLatin1Char('\\'));
            d_header_target_lbl->setToolTip(
                    hasExtraLbl ? QStringLiteral("%1 — %2").arg(newName, winLbl) : newName);
        }
        if (d_object_tree && d_object_tree->topLevelItemCount() > 0) {
            if (QTreeWidgetItem *pageItem = d_object_tree->topLevelItem(0))
                pageItem->setText(0, tr("Page: %1").arg(newName));
        }
    });
    connect(ml, &MultiLayer::selectionChanged, this, &GraphInspectorDock::onGraphSelectionChanged,
            Qt::UniqueConnection);
    connect(ml, &MultiLayer::modifiedPlot, this, [this]() {
        if (!d_target_ml || d_target_ml->isRestoringSnapshot())
            return;
        if (!d_syncing_props && !d_syncing_tree && d_coalesce_refresh_timer) {
            if (!d_coalesce_refresh_timer->isActive())
                d_coalesce_refresh_timer->start();
        }
    });
    connect(ml, &QObject::destroyed, this, [this](QObject *deadObj) {
        if (d_coalesce_refresh_timer && d_coalesce_refresh_timer->isActive())
            d_coalesce_refresh_timer->stop();
        if (d_annot_commit_timer && d_annot_commit_timer->isActive()
            && (!d_annot_edit_layer || d_annot_edit_layer->multiLayerParent() == deadObj)) {
            d_annot_commit_timer->stop();
            d_annot_edit_layer = nullptr;
            d_annot_edit_id = -1;
        }
        if (d_axis_title_timer && d_axis_title_timer->isActive()
            && (!d_axis_title_edit_layer || d_axis_title_edit_layer->multiLayerParent() == deadObj)) {
            d_axis_title_timer->stop();
            d_axis_title_edit_layer = nullptr;
            d_axis_title_edit_axis = -1;
        }
        d_target_ml = nullptr;
        d_bound_layers.clear();
        refreshAll();
        if (d_collapsed_rail)
            applyCollapsedGeometry();
    });

    for (Graph *g : ml->layersList())
        bindLayerSignals(g);
}

void GraphInspectorDock::bindLayerSignals(Graph *g)
{
    if (!g)
        return;
    for (const QPointer<Graph> &existing : d_bound_layers) {
        if (existing.data() == g)
            return;
    }
    d_bound_layers.append(QPointer<Graph>(g));

    connect(g, &Graph::curveOrderChanged, this, [this]() {
        if (!d_syncing_tree)
            refreshObjectTree();
    });
    connect(g, &Graph::curveVisibilityChanged, this, [this](int, bool) {
        if (!d_syncing_tree)
            refreshObjectTree();
    });
    connect(g, &Graph::selectedGraph, this, [this](Graph *) {
        if (!d_syncing_tree && !d_syncing_props) {
            refreshObjectTree();
            syncInspectorFromSelection();
        }
    });
}

void GraphInspectorDock::buildUi()
{
    d_root_container = new QWidget(this);
    d_root_container->setObjectName(QStringLiteral("InspectorRootContainer"));
    QVBoxLayout *rootLayout = new QVBoxLayout(d_root_container);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    d_mode_stack = new ActivePageStackedWidget(d_root_container);
    rootLayout->addWidget(d_mode_stack);

    // =========================================================================
    // Mode 0: Expanded Panel (268 px, min 240 px, max 330 px)
    // =========================================================================
    d_expanded_panel = new QWidget(d_mode_stack);
    QVBoxLayout *expLayout = new QVBoxLayout(d_expanded_panel);
    expLayout->setContentsMargins(0, 0, 0, 0);
    expLayout->setSpacing(0);

    d_header_bar = new QFrame(d_expanded_panel);
    d_header_bar->setObjectName(QStringLiteral("InspectorHeaderBar"));
    QHBoxLayout *hdrLayout = new QHBoxLayout(d_header_bar);
    hdrLayout->setContentsMargins(8, 5, 6, 5);
    hdrLayout->setSpacing(4);

    d_header_title_lbl = new QLabel(d_header_bar);
    d_header_title_lbl->setObjectName(QStringLiteral("InspectorHeaderTitle"));
    hdrLayout->addWidget(d_header_title_lbl);

    d_header_target_lbl = new QLabel(d_header_bar);
    d_header_target_lbl->setObjectName(QStringLiteral("InspectorHeaderTarget"));
    d_header_target_lbl->setMinimumWidth(0);
    d_header_target_lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    hdrLayout->addWidget(d_header_target_lbl, 1);

    d_btn_pin = new QToolButton(d_header_bar);
    d_btn_pin->setCheckable(true);
    d_btn_pin->setAutoRaise(true);
    d_btn_pin->setFixedSize(22, 22);
    d_btn_pin->setIcon(IconLoader::duotoneIcon(QStringLiteral("lock"), 15));
    connect(d_btn_pin, &QToolButton::toggled, this, [this](bool checked) {
        d_user_pinned = checked;
        emit userPinToggled(checked);
    });
    hdrLayout->addWidget(d_btn_pin);

    d_btn_collapse = new QToolButton(d_header_bar);
    d_btn_collapse->setAutoRaise(true);
    d_btn_collapse->setFixedSize(22, 22);
    d_btn_collapse->setText(QString::fromUtf8("\xC2\xBB")); // »
    connect(d_btn_collapse, &QToolButton::clicked, this, [this]() {
        setUserPinned(false);
        setCollapsedRail(true);
    });
    hdrLayout->addWidget(d_btn_collapse);

    expLayout->addWidget(d_header_bar);

    // Content stack: Index 0 = Quick Start Empty State, Index 1 = Active Graph Inspector
    d_content_stack = new ActivePageStackedWidget(d_expanded_panel);
    d_empty_quickstart_panel = buildQuickStartEmptyState();
    d_content_stack->addWidget(d_empty_quickstart_panel);

    d_inspector_active_panel = new QWidget(d_content_stack);
    QVBoxLayout *activeLayout = new QVBoxLayout(d_inspector_active_panel);
    activeLayout->setContentsMargins(0, 0, 0, 0);
    activeLayout->setSpacing(0);

    // Multi-Panel Scope Switch ([Active Layer | All Panels]), shown when layers() > 1
    d_scope_strip = new QFrame(d_inspector_active_panel);
    d_scope_strip->setObjectName(QStringLiteral("InspectorScopeStrip"));
    QHBoxLayout *scopeRow = new QHBoxLayout(d_scope_strip);
    scopeRow->setContentsMargins(8, 3, 8, 3);
    scopeRow->setSpacing(4);

    d_lbl_scope = new QLabel(d_scope_strip);
    d_lbl_scope->setObjectName(QStringLiteral("InspectorHeaderTarget"));
    scopeRow->addWidget(d_lbl_scope);

    d_scope_group = new QButtonGroup(this);
    d_scope_group->setExclusive(true);

    auto makeScopePill = [&](int id) -> QToolButton * {
        QToolButton *b = new QToolButton(d_scope_strip);
        b->setCheckable(true);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setMinimumHeight(22);
        b->setObjectName(QStringLiteral("InspectorScopePillBtn"));
        d_scope_group->addButton(b, id);
        scopeRow->addWidget(b, 1);
        return b;
    };

    d_btn_scope_active = makeScopePill(0);
    d_btn_scope_all = makeScopePill(1);
    d_btn_scope_active->setChecked(true);
    connect(d_scope_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this,
            [this](int id) { d_scope_all_panels = (id == 1); });
    d_scope_strip->hide();
    activeLayout->addWidget(d_scope_strip);

    // Vertical Splitter: Top Object Manager + Bottom Property Inspector
    d_splitter = new QSplitter(Qt::Vertical, d_inspector_active_panel);
    d_splitter->setChildrenCollapsible(true);
    d_splitter->setHandleWidth(4);

    // --- Top Pane: Object Manager ---
    QWidget *topPane = new QWidget(d_splitter);
    topPane->setMinimumHeight(76);
    QVBoxLayout *topLayout = new QVBoxLayout(topPane);
    topLayout->setContentsMargins(6, 4, 6, 4);
    topLayout->setSpacing(4);

    d_tree_header_strip = new QFrame(topPane);
    QHBoxLayout *treeHdr = new QHBoxLayout(d_tree_header_strip);
    treeHdr->setContentsMargins(2, 0, 2, 0);
    treeHdr->setSpacing(3);

    d_obj_mgr_title_lbl = new QLabel(d_tree_header_strip);
    d_obj_mgr_title_lbl->setObjectName(QStringLiteral("InspectorSubTitle"));
    treeHdr->addWidget(d_obj_mgr_title_lbl, 1);

    auto makeTreeBtn = [&](const QString &txt, const QString &iconId) -> QToolButton * {
        QToolButton *b = new QToolButton(d_tree_header_strip);
        b->setAutoRaise(true);
        b->setFixedHeight(22);
        if (!iconId.isEmpty())
            b->setIcon(IconLoader::duotoneIcon(iconId, 14));
        if (!txt.isEmpty()) {
            b->setText(txt);
            b->setToolButtonStyle(iconId.isEmpty() ? Qt::ToolButtonTextOnly
                                                   : Qt::ToolButtonTextBesideIcon);
        }
        return b;
    };

    d_btn_add_curve = makeTreeBtn(QString(), QStringLiteral("curve-add"));
    d_btn_add_curve->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(d_btn_add_curve, &QToolButton::clicked, this, &GraphInspectorDock::openAddCurvePopover);
    treeHdr->addWidget(d_btn_add_curve);

    d_btn_move_up = makeTreeBtn(QString::fromUtf8("\xE2\x96\xB2"), QString()); // ▲
    d_btn_move_up->setFixedWidth(22);
    connect(d_btn_move_up, &QToolButton::clicked, this, [this]() { moveSelectedCurve(-1); });
    treeHdr->addWidget(d_btn_move_up);

    d_btn_move_down = makeTreeBtn(QString::fromUtf8("\xE2\x96\xBC"), QString()); // ▼
    d_btn_move_down->setFixedWidth(22);
    connect(d_btn_move_down, &QToolButton::clicked, this, [this]() { moveSelectedCurve(+1); });
    treeHdr->addWidget(d_btn_move_down);

    d_btn_remove_obj = makeTreeBtn(QString::fromUtf8("\xE2\x88\x92"), QString()); // −
    d_btn_remove_obj->setFixedWidth(22);
    connect(d_btn_remove_obj, &QToolButton::clicked, this,
            &GraphInspectorDock::removeSelectedObject);
    treeHdr->addWidget(d_btn_remove_obj);

    topLayout->addWidget(d_tree_header_strip);

    d_object_tree = new QTreeWidget(topPane);
    d_object_tree->setObjectName(QStringLiteral("GraphObjectManagerTree"));
    d_object_tree->setHeaderHidden(true);
    d_object_tree->setColumnCount(1);
    d_object_tree->setIndentation(14);
    d_object_tree->setUniformRowHeights(true);
    d_object_tree->setIconSize(QSize(22, 14));
    d_object_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    d_object_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(d_object_tree, &QTreeWidget::itemClicked, this,
            &GraphInspectorDock::onTreeItemClicked);
    connect(d_object_tree, &QTreeWidget::itemDoubleClicked, this,
            &GraphInspectorDock::onTreeItemDoubleClicked);
    connect(d_object_tree, &QTreeWidget::customContextMenuRequested, this,
            &GraphInspectorDock::onObjectTreeContextMenu);
    connect(d_object_tree, &QTreeWidget::itemChanged, this,
            &GraphInspectorDock::onTreeItemChanged);
    d_object_tree->installEventFilter(this);
    topLayout->addWidget(d_object_tree, 1);

    d_empty_hint_lbl = new QLabel(topPane);
    d_empty_hint_lbl->setWordWrap(true);
    d_empty_hint_lbl->setAlignment(Qt::AlignCenter);
    d_empty_hint_lbl->hide();
    topLayout->addWidget(d_empty_hint_lbl);

    d_splitter->addWidget(topPane);

    // --- Bottom Pane: Context Tabs + Scrollable Property Stack ---
    QWidget *bottomPane = new QWidget(d_splitter);
    bottomPane->setMinimumHeight(140);
    QVBoxLayout *botLayout = new QVBoxLayout(bottomPane);
    botLayout->setContentsMargins(0, 0, 0, 0);
    botLayout->setSpacing(0);

    d_tab_strip = new QFrame(bottomPane);
    d_tab_strip->setObjectName(QStringLiteral("InspectorTabStrip"));
    QHBoxLayout *tabStripOuter = new QHBoxLayout(d_tab_strip);
    tabStripOuter->setContentsMargins(6, 4, 6, 4);
    tabStripOuter->setSpacing(0);

    QFrame *tabTrack = new QFrame(d_tab_strip);
    tabTrack->setObjectName(QStringLiteral("InspectorSegmentedTrack"));
    QHBoxLayout *tabRow = new QHBoxLayout(tabTrack);
    tabRow->setContentsMargins(2, 2, 2, 2);
    tabRow->setSpacing(2);
    tabStripOuter->addWidget(tabTrack);

    d_page_tab_group = new QButtonGroup(this);
    d_page_tab_group->setExclusive(true);

    auto makePageTabBtn = [&](int idx) -> QToolButton * {
        QToolButton *b = new QToolButton(tabTrack);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setMinimumHeight(26);
        b->setObjectName(QStringLiteral("InspectorContextTabBtn"));
        d_page_tab_group->addButton(b, idx);
        tabRow->addWidget(b);
        return b;
    };

    d_tab_btn_page = makePageTabBtn(PageTabPageLayer);
    d_tab_btn_axis = makePageTabBtn(PageTabAxis);
    d_tab_btn_curve = makePageTabBtn(PageTabCurve);
    d_tab_btn_legend = makePageTabBtn(PageTabLegend);
    d_tab_btn_page->setChecked(true);

    connect(d_page_tab_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this,
            [this](int idx) {
                switchToInspectorPage(static_cast<InspectorPageIndex>(idx));
            });

    botLayout->addWidget(d_tab_strip);

    d_prop_scroll = new QScrollArea(bottomPane);
    d_prop_scroll->setWidgetResizable(true);
    d_prop_scroll->setFrameShape(QFrame::NoFrame);
    d_prop_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d_prop_scroll->setMinimumHeight(48);

    d_prop_stack = new ActivePageStackedWidget(d_prop_scroll);
    d_prop_stack->addWidget(buildPageLayerInspector());
    d_prop_stack->addWidget(buildAxisInspector());
    d_prop_stack->addWidget(buildCurveInspector());
    d_prop_stack->addWidget(buildLegendInspector());
    d_prop_scroll->setWidget(d_prop_stack);

    d_prop_scroll->viewport()->installEventFilter(this);
    connect(d_prop_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { updatePropScrollCue(); });
    connect(d_prop_scroll->verticalScrollBar(), &QScrollBar::rangeChanged, this,
            [this](int, int) { updatePropScrollCue(); });
    connect(d_prop_stack, &QStackedWidget::currentChanged, this, [this](int) {
        QTimer::singleShot(0, this, &GraphInspectorDock::updatePropScrollCue);
    });

    botLayout->addWidget(d_prop_scroll, 1);

    // C2: every inspector section leads to its full dialog (Origin's "Plot Details" habit).
    {
        QFrame *detailsRow = new QFrame(bottomPane);
        detailsRow->setObjectName(QStringLiteral("InspectorDetailsRow"));
        detailsRow->setAttribute(Qt::WA_StyledBackground, true);
        detailsRow->setStyleSheet(QStringLiteral(
                "QFrame#InspectorDetailsRow { border-top: 1px solid #CBD5E1; background-color: #F8FAFC; }"));
        detailsRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        QHBoxLayout *detailsLay = new QHBoxLayout(detailsRow);
        detailsLay->setContentsMargins(8, 4, 8, 4);
        detailsLay->setSpacing(4);

        d_scroll_more_cue = new QFrame(detailsRow);
        d_scroll_more_cue->setObjectName(QStringLiteral("InspectorScrollMoreCue"));
        d_scroll_more_cue->setAttribute(Qt::WA_StyledBackground, true);
        {
            QHBoxLayout *cueLay = new QHBoxLayout(d_scroll_more_cue);
            cueLay->setContentsMargins(6, 2, 6, 2);
            cueLay->setSpacing(4);
            QLabel *cueLbl = new QLabel(
                    QString::fromUtf8("\xE2\x96\xBE ") + tr("More below"), d_scroll_more_cue);
            cueLbl->setStyleSheet(
                    QStringLiteral("font-size: 8.5pt; color: #475569; background: transparent;"));
            cueLay->addWidget(cueLbl);
        }
        d_scroll_more_cue->hide();
        detailsLay->addWidget(d_scroll_more_cue);

        detailsLay->addStretch(1);
        QPushButton *detailsBtn = new QPushButton(tr("Details..."), detailsRow);
        detailsBtn->setObjectName(QStringLiteral("InspectorDetailsButton"));
        detailsBtn->setToolTip(tr("Open the dialog with every setting for this section"));
        detailsBtn->setCursor(Qt::PointingHandCursor);
        detailsLay->addWidget(detailsBtn);
        botLayout->addWidget(detailsRow, 0);
        connect(detailsBtn, &QPushButton::clicked, this, [this]() { openDetailsDialogForPage(); });
    }
    d_splitter->addWidget(bottomPane);
    d_splitter->setStretchFactor(0, 18);
    d_splitter->setStretchFactor(1, 82);
    d_splitter->setSizes({ 105, 360 });
    connect(d_splitter, &QSplitter::splitterMoved, this,
            [this](int, int) { d_user_resized_splitter = true; });

    activeLayout->addWidget(d_splitter, 1);
    d_content_stack->addWidget(d_inspector_active_panel);
    d_content_stack->setCurrentIndex(0);
    expLayout->addWidget(d_content_stack, 1);
    d_mode_stack->addWidget(d_expanded_panel);

    // =========================================================================
    // Mode 1: Collapsed 48 px Vertical Icon + Short Label Rail
    // =========================================================================
    d_rail_panel = new QFrame(d_mode_stack);
    d_rail_panel->setObjectName(QStringLiteral("InspectorRailPanel"));
    QVBoxLayout *railLayout = new QVBoxLayout(d_rail_panel);
    railLayout->setContentsMargins(2, 6, 2, 6);
    railLayout->setSpacing(5);

    auto makeRailBtn = [&](const QString &iconId, const QString &fallbackText) -> QToolButton * {
        QToolButton *b = new QToolButton(d_rail_panel);
        b->setAutoRaise(true);
        if (!iconId.isEmpty()) {
            b->setObjectName(QStringLiteral("InspectorRailTabBtn"));
            b->setFixedSize(42, 40);
            b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            b->setIcon(IconLoader::duotoneIcon(iconId, 16));
            b->setIconSize(QSize(16, 16));
        } else {
            b->setFixedSize(40, 24);
            b->setText(fallbackText);
        }
        railLayout->addWidget(b, 0, Qt::AlignHCenter);
        return b;
    };

    d_rail_expand_btn = makeRailBtn(QString(), QString::fromUtf8("\xC2\xAB")); // «
    connect(d_rail_expand_btn, &QToolButton::clicked, this, [this]() {
        setUserPinned(true);
        setCollapsedRail(false);
    });

    QFrame *railSep = new QFrame(d_rail_panel);
    railSep->setFrameShape(QFrame::HLine);
    railSep->setFixedHeight(1);
    railLayout->addWidget(railSep);

    auto expandToTab = [this](InspectorPageIndex idx) {
        setUserPinned(true);
        setCollapsedRail(false);
        switchToInspectorPage(idx);
        syncInspectorFromSelection();
    };

    d_rail_page_btn = makeRailBtn(QStringLiteral("page-setup"), QString());
    connect(d_rail_page_btn, &QToolButton::clicked, this,
            [expandToTab]() { expandToTab(PageTabPageLayer); });

    d_rail_axis_btn = makeRailBtn(QStringLiteral("axis-scale"), QString());
    connect(d_rail_axis_btn, &QToolButton::clicked, this,
            [expandToTab]() { expandToTab(PageTabAxis); });

    d_rail_curve_btn = makeRailBtn(QStringLiteral("plot-line-symbol"), QString());
    connect(d_rail_curve_btn, &QToolButton::clicked, this,
            [expandToTab]() { expandToTab(PageTabCurve); });

    d_rail_legend_btn = makeRailBtn(QStringLiteral("legend"), QString());
    connect(d_rail_legend_btn, &QToolButton::clicked, this,
            [expandToTab]() { expandToTab(PageTabLegend); });

    railLayout->addStretch(1);
    d_mode_stack->addWidget(d_rail_panel);

    setWidget(d_root_container);
}

QWidget *GraphInspectorDock::buildQuickStartEmptyState()
{
    QWidget *outer = new QWidget(d_content_stack);
    outer->setObjectName(QStringLiteral("InspectorQuickStartPanel"));
    QVBoxLayout *outerLay = new QVBoxLayout(outer);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    QScrollArea *scroll = new QScrollArea(outer);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumHeight(80);

    QWidget *content = new QWidget(scroll);
    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->setContentsMargins(12, 14, 12, 14);
    lay->setSpacing(10);

    // Welcome card header
    QFrame *heroCard = new QFrame(content);
    heroCard->setObjectName(QStringLiteral("QuickStartHeroCard"));
    QVBoxLayout *heroLay = new QVBoxLayout(heroCard);
    heroLay->setContentsMargins(12, 12, 12, 12);
    heroLay->setSpacing(4);

    d_qs_title_lbl = new QLabel(heroCard);
    d_qs_title_lbl->setObjectName(QStringLiteral("QuickStartTitle"));
    d_qs_title_lbl->setWordWrap(true);
    heroLay->addWidget(d_qs_title_lbl);

    d_qs_subtitle_lbl = new QLabel(heroCard);
    d_qs_subtitle_lbl->setObjectName(QStringLiteral("QuickStartSubtitle"));
    d_qs_subtitle_lbl->setWordWrap(true);
    heroLay->addWidget(d_qs_subtitle_lbl);
    lay->addWidget(heroCard);

    // 3 Clear Step Cards
    auto addStepCard = [&](const QString &num, QLabel *&textLbl) {
        QFrame *card = new QFrame(content);
        card->setObjectName(QStringLiteral("QuickStartCard"));
        QHBoxLayout *row = new QHBoxLayout(card);
        row->setContentsMargins(10, 9, 10, 9);
        row->setSpacing(9);

        QLabel *badge = new QLabel(num, card);
        badge->setObjectName(QStringLiteral("QuickStartStepBadge"));
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(22, 22);
        row->addWidget(badge, 0, Qt::AlignTop);

        textLbl = new QLabel(card);
        textLbl->setObjectName(QStringLiteral("QuickStartStepText"));
        textLbl->setWordWrap(true);
        row->addWidget(textLbl, 1);

        lay->addWidget(card);
    };

    addStepCard(QStringLiteral("1"), d_qs_step1_lbl);
    addStepCard(QStringLiteral("2"), d_qs_step2_lbl);
    addStepCard(QStringLiteral("3"), d_qs_step3_lbl);

    // 1-Click Quick Action Buttons
    d_btn_qs_import_csv = new QPushButton(content);
    d_btn_qs_import_csv->setObjectName(QStringLiteral("QuickStartPrimaryBtn"));
    d_btn_qs_import_csv->setIcon(IconLoader::duotoneIcon(QStringLiteral("import-csv"), 16));
    d_btn_qs_import_csv->setMinimumHeight(32);
    d_btn_qs_import_csv->setCursor(Qt::PointingHandCursor);
    connect(d_btn_qs_import_csv, &QPushButton::clicked, this, [this]() {
        QPointer<GraphInspectorDock> self(this);
        QPointer<ApplicationWindow> appGuard(d_app);
        if (!appGuard || ApplicationWindow::isNonInteractiveContext(appGuard))
            return;
        const QString startDir = !appGuard->asciiDirPath.isEmpty() ? appGuard->asciiDirPath
                                                                   : appGuard->workingDir;
        const QString filter = tr(
                "Excel & CSV / Data (*.xlsx *.xlsm *.csv *.tsv *.txt *.dat);;"
                "Excel Workbooks (*.xlsx *.xlsm);;"
                "CSV / Text Data (*.csv *.tsv *.txt *.dat);;"
                "All Files (*)");
        const QStringList files = QFileDialog::getOpenFileNames(
                appGuard, tr("Import Excel (.xlsx) / CSV"), startDir, filter);
        if (!self || !appGuard || files.isEmpty())
            return;
        appGuard->asciiDirPath = QFileInfo(files.first()).absolutePath();
        QStringList summaries;
        for (const QString &fn : files) {
            if (!appGuard)
                break;
            if (fn.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive)
                || fn.endsWith(QLatin1String(".xlsm"), Qt::CaseInsensitive)) {
                QString summary;
                if (XlsxImport::importDroppedFile(appGuard, fn, &summary) && !summary.isEmpty())
                    summaries << summary;
            } else if (fn.endsWith(QLatin1String(".sciprj"), Qt::CaseInsensitive)
                       || fn.endsWith(QLatin1String(".sciprj.gz"), Qt::CaseInsensitive)
                       || fn.endsWith(QLatin1String(".qti"), Qt::CaseInsensitive)
                       || fn.endsWith(QLatin1String(".opj"), Qt::CaseInsensitive)
                       || fn.endsWith(QLatin1String(".opju"), Qt::CaseInsensitive)) {
                appGuard->open(fn);
            } else {
                const QString sep = detectSmartAsciiSeparator(fn, appGuard->columnSeparator);
                appGuard->importASCII(QStringList { fn }, ImportASCIIDialog::NewTables, sep, 0,
                                      true, true, false, true, QLocale::c());
            }
        }
        if (!self || !appGuard)
            return;
        if (!summaries.isEmpty() && appGuard->statusBar())
            appGuard->statusBar()->showMessage(summaries.join(QStringLiteral(" | ")), 7000);
        self->refreshAll();
    });
    lay->addWidget(d_btn_qs_import_csv);

    d_btn_qs_new_graph = new QPushButton(content);
    d_btn_qs_new_graph->setObjectName(QStringLiteral("QuickStartSecondaryBtn"));
    d_btn_qs_new_graph->setIcon(IconLoader::duotoneIcon(QStringLiteral("plot-line-symbol"), 16));
    d_btn_qs_new_graph->setMinimumHeight(30);
    d_btn_qs_new_graph->setCursor(Qt::PointingHandCursor);
    connect(d_btn_qs_new_graph, &QPushButton::clicked, this, [this]() {
        if (!d_app)
            return;
        if (ModernRibbonBar *rib = d_app->modernRibbonBar())
            rib->triggerPlot2D(Graph::LineSymbols);
        else
            d_app->newGraph();
    });
    lay->addWidget(d_btn_qs_new_graph);

    d_btn_qs_sample_gallery = new QPushButton(content);
    d_btn_qs_sample_gallery->setObjectName(QStringLiteral("QuickStartSecondaryBtn"));
    d_btn_qs_sample_gallery->setIcon(IconLoader::duotoneIcon(QStringLiteral("plot-line-symbol"), 16));
    d_btn_qs_sample_gallery->setMinimumHeight(30);
    d_btn_qs_sample_gallery->setCursor(Qt::PointingHandCursor);
    connect(d_btn_qs_sample_gallery, &QPushButton::clicked, this, [this]() {
        if (d_app)
            d_app->showSampleGalleryDialog();
    });
    lay->addWidget(d_btn_qs_sample_gallery);

    // A1-8: compact ? Guide (F1) button in Quick Start
    d_btn_qs_help = new QPushButton(content);
    d_btn_qs_help->setObjectName(QStringLiteral("QuickStartSecondaryBtn"));
    d_btn_qs_help->setIcon(IconLoader::duotoneIcon(QStringLiteral("help"), 15));
    d_btn_qs_help->setMinimumHeight(28);
    d_btn_qs_help->setCursor(Qt::PointingHandCursor);
    connect(d_btn_qs_help, &QPushButton::clicked, this, [this]() {
        if (!d_app)
            return;
        QMetaObject::invokeMethod(d_app.data(), "showHelp", Qt::DirectConnection);
    });
    lay->addWidget(d_btn_qs_help);

    lay->addStretch(1);
    scroll->setWidget(content);
    outerLay->addWidget(scroll);
    outer->installEventFilter(this);
    return outer;
}

QIcon GraphInspectorDock::buildCurveSwatchIcon(Graph *g, int curveIndex)
{
    QPixmap px(22, 14);
    px.fill(Qt::transparent);
    if (!g || curveIndex < 0 || curveIndex >= g->curves())
        return QIcon(px);

    if (auto *sp = dynamic_cast<Spectrogram *>(g->plotItem(curveIndex))) {
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing, true);
        QLinearGradient grad(2.0, 7.0, 20.0, 7.0);
        const QwtDoubleInterval unitIv(0.0, 1.0);
        grad.setColorAt(0.0, sp->colorMap().color(unitIv, 0.05));
        grad.setColorAt(0.5, sp->colorMap().color(unitIv, 0.50));
        grad.setColorAt(1.0, sp->colorMap().color(unitIv, 0.95));
        p.setPen(QPen(QColor(110, 120, 135), 1.0));
        p.setBrush(grad);
        p.drawRoundedRect(QRectF(1.5, 2.5, 19.0, 9.0), 2.0, 2.0);
        p.end();
        return QIcon(px);
    }

    if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(g->curve(curveIndex))) {
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor ec = er->color().isValid() ? er->color() : QColor(Qt::black);
        p.setPen(QPen(ec, 1.4));
        p.drawLine(QPointF(11.0, 2.5), QPointF(11.0, 11.5));
        p.drawLine(QPointF(7.5, 2.5), QPointF(14.5, 2.5));
        p.drawLine(QPointF(7.5, 11.5), QPointF(14.5, 11.5));
        p.end();
        return QIcon(px);
    }

    QwtPlotCurve *cv = g->curve(curveIndex);
    if (!cv)
        return QIcon(px);

    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPen linePen = cv->pen();
    const QwtSymbol sym = cv->symbol();
    const bool hasLine = (cv->style() != QwtPlotCurve::NoCurve && linePen.style() != Qt::NoPen);
    const QColor baseCol = (hasLine && linePen.color().isValid())
            ? linePen.color()
            : (sym.pen().color().isValid() ? sym.pen().color() : QColor(Qt::black));

    if (cv->style() != QwtPlotCurve::NoCurve && linePen.style() != Qt::NoPen) {
        const double rawW = linePen.widthF() > 0.0 ? linePen.widthF()
                                                   : double(qMax(1, linePen.width()));
        QPen sp(baseCol, qBound(1.0, rawW, 3.0), linePen.style());
        p.setPen(sp);
        p.drawLine(QPointF(1.0, 7.0), QPointF(21.0, 7.0));
    }

    if (sym.style() != QwtSymbol::NoSymbol) {
        const bool hollow = Graph::isSymbolWhiteHollow(sym);
        const QColor symBorder = sym.pen().color().isValid() ? sym.pen().color() : baseCol;
        const QBrush symBrush = hollow
                ? QBrush(QColor(Qt::white))
                : ((sym.brush().style() != Qt::NoBrush && sym.brush().color().isValid())
                           ? sym.brush()
                           : QBrush(symBorder));
        p.setPen(QPen(symBorder, 1.3));
        p.setBrush(symBrush);

        const QRectF r(7.5, 3.5, 7.0, 7.0);
        switch (sym.style()) {
        case QwtSymbol::Rect:
            p.drawRect(r);
            break;
        case QwtSymbol::Diamond: {
            QPolygonF poly;
            poly << QPointF(11.0, 3.0) << QPointF(15.0, 7.0) << QPointF(11.0, 11.0)
                 << QPointF(7.0, 7.0);
            p.drawPolygon(poly);
            break;
        }
        case QwtSymbol::UTriangle: {
            QPolygonF poly;
            poly << QPointF(11.0, 3.2) << QPointF(15.0, 10.6) << QPointF(7.0, 10.6);
            p.drawPolygon(poly);
            break;
        }
        case QwtSymbol::DTriangle: {
            QPolygonF poly;
            poly << QPointF(7.0, 3.4) << QPointF(15.0, 3.4) << QPointF(11.0, 10.8);
            p.drawPolygon(poly);
            break;
        }
        case QwtSymbol::Cross:
            p.drawLine(QPointF(11.0, 3.2), QPointF(11.0, 10.8));
            p.drawLine(QPointF(7.2, 7.0), QPointF(14.8, 7.0));
            break;
        case QwtSymbol::XCross:
            p.drawLine(QPointF(7.5, 3.5), QPointF(14.5, 10.5));
            p.drawLine(QPointF(14.5, 3.5), QPointF(7.5, 10.5));
            break;
        default:
            p.drawEllipse(r);
            break;
        }
    }
    p.end();
    return QIcon(px);
}

QColor GraphInspectorDock::pickColorInteractive(QWidget *parent, const QColor &initial,
                                                const QString &title)
{
    if (ApplicationWindow::isNonInteractiveContext(parent))
        return initial;

    // A3-6: compact ToriGraph Standard + Okabe-Ito + Basic/Grayscale color swatch popup,
    // with a "Custom Color..." button opening QColorDialog::getColor().
    QMenu popup(parent);
    QWidget *panel = new QWidget(&popup);
    QVBoxLayout *vlay = new QVBoxLayout(panel);
    vlay->setContentsMargins(8, 8, 8, 8);
    vlay->setSpacing(6);

    QColor chosen;
    bool openCustom = false;

    auto addSwatchGroup = [&](const QString &heading, const QList<QColor> &colors) {
        if (colors.isEmpty())
            return;
        QLabel *lbl = new QLabel(heading, panel);
        lbl->setStyleSheet(QStringLiteral(
                "QLabel { font-size: 9pt; font-weight: 600; color: #475569; }"));
        vlay->addWidget(lbl);

        QGridLayout *grid = new QGridLayout();
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(4);
        grid->setVerticalSpacing(4);
        const int cols = 8;
        const int count = qMin(colors.size(), 16);
        for (int i = 0; i < count; ++i) {
            const QColor col = colors[i];
            QToolButton *sw = new QToolButton(panel);
            sw->setFixedSize(22, 22);
            sw->setCursor(Qt::PointingHandCursor);
            const QString hex = col.name(QColor::HexRgb).toUpper();
            sw->setToolTip(hex);
            const bool isCurrent = initial.isValid() && (initial.rgb() == col.rgb());
            const QString borderCss = isCurrent
                    ? QStringLiteral("2px solid #2563EB")
                    : QStringLiteral("1px solid #94A3B8");
            sw->setStyleSheet(QStringLiteral(
                                      "QToolButton { background-color: %1; border: %2; "
                                      "border-radius: 4px; }"
                                      "QToolButton:hover { border: 2px solid #2563EB; }")
                                      .arg(hex, borderCss));
            QObject::connect(sw, &QToolButton::clicked, &popup, [&popup, &chosen, col]() {
                chosen = col;
                popup.close();
            });
            grid->addWidget(sw, i / cols, i % cols);
        }
        vlay->addLayout(grid);
    };

    const QString defPalName = Graph::defaultCurvePaletteName();
    addSwatchGroup(QCoreApplication::translate("GraphInspectorDock", defPalName.toUtf8().constData()),
                   Graph::paletteColors(defPalName));

    const QString okabeName = Graph::okabeItoPaletteName();
    if (Graph::canonicalPaletteName(defPalName) != Graph::canonicalPaletteName(okabeName)) {
        addSwatchGroup(QCoreApplication::translate("GraphInspectorDock", "Okabe-Ito"),
                       Graph::paletteColors(okabeName));
    }

    const QList<QColor> basicColors = {
        QColor(0x00, 0x00, 0x00), QColor(0x33, 0x41, 0x55), QColor(0x64, 0x74, 0x8B),
        QColor(0x94, 0xA3, 0xB8), QColor(0xCB, 0xD5, 0xE1), QColor(0xFF, 0xFF, 0xFF),
        QColor(0xDC, 0x26, 0x26), QColor(0x25, 0x63, 0xEB)
    };
    addSwatchGroup(QCoreApplication::translate("GraphInspectorDock", "Basic & Grayscale"),
                   basicColors);

    QPushButton *btnCustom = new QPushButton(
            QCoreApplication::translate("GraphInspectorDock", "Custom Color..."), panel);
    btnCustom->setCursor(Qt::PointingHandCursor);
    QObject::connect(btnCustom, &QPushButton::clicked, &popup, [&popup, &openCustom]() {
        openCustom = true;
        popup.close();
    });
    vlay->addWidget(btnCustom);

    QWidgetAction *wa = new QWidgetAction(&popup);
    wa->setDefaultWidget(panel);
    popup.addAction(wa);

    QPoint popupPos = QCursor::pos();
    if (QWidget *fw = QApplication::focusWidget()) {
        if (fw->objectName() == QLatin1String("InspectorColorSwatchBtn")
            && (!parent || parent->isAncestorOf(fw))) {
            popupPos = fw->mapToGlobal(QPoint(0, fw->height() + 2));
        }
    }
    popup.exec(popupPos);

    if (openCustom) {
        return QColorDialog::getColor(initial.isValid() ? initial : QColor(Qt::black), parent,
                                      title, QColorDialog::ShowAlphaChannel);
    }
    return chosen;
}

void GraphInspectorDock::applyTheme()
{
    static bool s_applying = false;
    if (s_applying)
        return;
    s_applying = true;
    // ToriGraph always uses its light theme (ModernUiTheme pins a light palette), so the
    // inspector has a single light colour set.
    const QString bg = QStringLiteral("#F8FAFC");
    const QString hdrBg = QStringLiteral("#F1F5F9");
    const QString cardBg = QStringLiteral("#FFFFFF");
    const QString border = QStringLiteral("#E2E8F0");
    const QString fg = QStringLiteral("#0F172A");
    const QString subFg = QStringLiteral("#475569");
    const QString accent = QStringLiteral("#2563EB");
    const QString accentSoft = QStringLiteral("#EFF6FF");
    const QString railFg = QStringLiteral("#334155");
    const QString trackBg = QStringLiteral("#F1F5F9");
    const QString segActiveBg = QStringLiteral("#FFFFFF");
    const QString segActiveBorder = QStringLiteral("#CBD5E1");
    const QString chipBg = QStringLiteral("#F8FAFC");
    const QString chipHoverBg = QStringLiteral("#EFF6FF");
    const QString chipHoverBorder = QStringLiteral("#93C5FD");

    QString qss = QStringLiteral(
                          "#InspectorRootContainer { background: %1; color: %5; border-left: 1px solid %4; }"
                          "#InspectorHeaderBar, #InspectorRailPanel, #InspectorScopeStrip { background: %2; border-bottom: 1px solid %4; }"
                          "#InspectorHeaderTitle { font-weight: 600; font-size: 11pt; color: %5; }"
                          "#InspectorHeaderTarget { font-size: 10pt; color: %6; }"
                          "#InspectorSubTitle { font-weight: 600; font-size: 10pt; color: %5; }"
                          "#InspectorSectionHeader { font-weight: 600; font-size: 9pt; color: %6; padding-top: 6px; padding-bottom: 2px; border-bottom: 1px solid %4; }"
                          "#InspectorTabStrip { background: %1; border-top: 1px solid %4; border-bottom: 1px solid %4; }"
                          "QFrame#InspectorSegmentedTrack { background: @TRACK_BG@; border: 1px solid %4; border-radius: 7px; }"
                          "QToolButton#InspectorContextTabBtn, QToolButton#InspectorAxisPillBtn {"
                          "  background: transparent; color: %6; border: 1px solid transparent; border-radius: 5px; font-size: 10pt; font-weight: 500; padding: 1px 4px;"
                          "}"
                          "QToolButton#InspectorContextTabBtn:hover, QToolButton#InspectorAxisPillBtn:hover {"
                          "  background: %8; color: %5;"
                          "}"
                          "QToolButton#InspectorContextTabBtn:checked, QToolButton#InspectorAxisPillBtn:checked {"
                          "  background: @SEG_ACT_BG@; color: %7; border: 1px solid @SEG_ACT_BD@; font-weight: 600;"
                          "}"
                          "QToolButton#InspectorScopePillBtn {"
                          "  background: %3; color: %6; border: 1px solid %4; border-radius: 5px; font-size: 10pt; padding: 2px 5px;"
                          "}"
                          "QToolButton#InspectorScopePillBtn:checked {"
                          "  background: %8; color: %7; border: 1px solid %7; font-weight: 600;"
                          "}"
                          "QToolButton#InspectorSymbolChipBtn {"
                          "  background: @CHIP_BG@; color: %5; border: 1px solid %4; border-radius: 4px; font-size: 9pt; padding: 1px 3px; min-height: 22px; min-width: 22px;"
                          "}"
                          "QToolButton#InspectorMenuChipBtn {"
                          "  background: @CHIP_BG@; color: %5; border: 1px solid %4; border-radius: 4px; font-size: 9pt; padding: 1px 16px 1px 5px; min-height: 22px; min-width: 56px;"
                          "}"
                          "QToolButton#InspectorMenuChipBtn::menu-indicator {"
                          "  subcontrol-origin: padding; subcontrol-position: right center; right: 4px; width: 8px;"
                          "}"
                          "QToolButton#InspectorSymbolChipBtn:hover, QToolButton#InspectorMenuChipBtn:hover {"
                          "  background: @CHIP_HOV_BG@; color: %7; border: 1px solid @CHIP_HOV_BD@;"
                          "}"
                          "QToolButton#InspectorSymbolChipBtn:pressed, QToolButton#InspectorMenuChipBtn:pressed {"
                          "  background: %8; color: %7; border: 1px solid %7;"
                          "}"
                          "QFrame#InspectorDetailsRow {"
                          "  background-color: #F8FAFC; border-top: 1px solid #CBD5E1;"
                          "}"
                          "QFrame#InspectorScrollMoreCue {"
                          "  background: #E2E8F0; border: 1px solid #CBD5E1; border-radius: 4px;"
                          "}"
                          "QToolButton#InspectorRailTabBtn {"
                          "  color: %9; font-size: 9pt; border-radius: 4px; padding: 1px 0px;"
                          "}"
                          "QTreeWidget#GraphObjectManagerTree {"
                          "  background: %3; color: %5; border: 1px solid %4; border-radius: 6px; font-size: 10pt; padding: 2px; outline: 0;"
                          "}"
                          "QTreeWidget#GraphObjectManagerTree::item {"
                          "  height: 26px; padding: 2px 4px; border-radius: 4px;"
                          "}"
                          "QTreeWidget#GraphObjectManagerTree::item:hover {"
                          "  background: @TRACK_BG@;"
                          "}"
                          "QTreeWidget#GraphObjectManagerTree::item:selected {"
                          "  background: %8; color: %7;"
                          "}"
                          "#InspectorQuickStartPanel { background: %1; }"
                          "#QuickStartHeroCard { background: %8; border: 1px solid %4; border-radius: 8px; }"
                          "#QuickStartTitle { font-weight: 700; font-size: 11pt; color: %7; }"
                          "#QuickStartSubtitle { font-size: 10pt; color: %5; }"
                          "#QuickStartCard { background: %3; border: 1px solid %4; border-radius: 7px; }"
                          "#QuickStartStepBadge { background: %7; color: #FFFFFF; font-weight: 700; font-size: 9pt; border-radius: 11px; }"
                          "#QuickStartStepText { font-size: 10pt; color: %5; }"
                          "QPushButton#QuickStartPrimaryBtn {"
                          "  background: %7; color: #FFFFFF; border: 1px solid %7; border-radius: 6px; font-weight: 600; font-size: 10pt; padding: 5px 10px;"
                          "}"
                          "QPushButton#QuickStartSecondaryBtn {"
                          "  background: %3; color: %5; border: 1px solid %4; border-radius: 6px; font-weight: 600; font-size: 10pt; padding: 5px 10px;"
                          "}"
                          "QCheckBox#InspectorKeepAspectCheck { font-weight: 600; color: %5; }"
                          "QLabel#InspectorAxisTitlePreview, QLabel#InspectorRichPreviewLabel {"
                          "  font-size: 9.5pt; color: #334155; background: #F1F5F9;"
                          "  border: 1px solid %4; border-radius: 4px; padding: 3px 6px;"
                          "}"
                          "QLineEdit[pendingEdit=\"true\"] {"
                          "  border: 1.5px solid #D97706; background-color: #FFFBEB;"
                          "}"
                          "QPushButton { font-size: 10pt; padding: 5px 8px; min-height: 26px; }"
                          "QPushButton#InspectorCompactRatioBtn {"
                          "  font-size: 9pt; padding: 3px 2px; min-height: 24px; min-width: 0px;"
                          "}"
                          "QLabel, QCheckBox, QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit { font-size: 10pt; }")
                          .arg(bg, hdrBg, cardBg, border, fg, subFg, accent, accentSoft, railFg);
    qss.replace(QStringLiteral("@TRACK_BG@"), trackBg);
    qss.replace(QStringLiteral("@SEG_ACT_BG@"), segActiveBg);
    qss.replace(QStringLiteral("@SEG_ACT_BD@"), segActiveBorder);
    qss.replace(QStringLiteral("@CHIP_BG@"), chipBg);
    qss.replace(QStringLiteral("@CHIP_HOV_BG@"), chipHoverBg);
    qss.replace(QStringLiteral("@CHIP_HOV_BD@"), chipHoverBorder);

    if (d_root_container && d_root_container->styleSheet() != qss)
        d_root_container->setStyleSheet(qss);
    s_applying = false;
}

void GraphInspectorDock::retranslateUi()
{
    setWindowTitle(tr("Graph Inspector"));

    if (d_header_title_lbl)
        d_header_title_lbl->setText(tr("Graph Inspector"));
    if (d_btn_pin)
        d_btn_pin->setToolTip(tr("Pin Inspector Open (Prevent Auto-Collapse)"));
    if (d_btn_collapse)
        d_btn_collapse->setToolTip(tr("Collapse to Icon Rail (Ctrl+Shift+I)"));

    if (d_lbl_scope)
        d_lbl_scope->setText(tr("Scope:"));
    if (d_btn_scope_active) {
        d_btn_scope_active->setText(tr("Active Layer"));
        d_btn_scope_active->setToolTip(tr("Apply Axis/Frame/Grid edits only to the active layer"));
    }
    if (d_btn_scope_all) {
        d_btn_scope_all->setText(tr("All Panels"));
        d_btn_scope_all->setToolTip(tr("Apply Axis/Frame/Grid edits to all primary panels at once"));
    }

    if (d_rail_expand_btn)
        d_rail_expand_btn->setToolTip(tr("Expand Graph Inspector (Ctrl+Shift+I)"));
    if (d_rail_page_btn) {
        d_rail_page_btn->setText(tr("Overall"));
        d_rail_page_btn->setToolTip(tr("Page & Layer Properties"));
    }
    if (d_rail_axis_btn) {
        d_rail_axis_btn->setText(tr("Axis"));
        d_rail_axis_btn->setToolTip(tr("Axis Scale & Title Properties"));
    }
    if (d_rail_curve_btn) {
        d_rail_curve_btn->setText(tr("Curve"));
        d_rail_curve_btn->setToolTip(tr("Curve & Series Style Properties"));
    }
    if (d_rail_legend_btn) {
        d_rail_legend_btn->setText(tr("Legend"));
        d_rail_legend_btn->setToolTip(tr("Legend & Annotation Properties"));
    }

    if (d_obj_mgr_title_lbl)
        d_obj_mgr_title_lbl->setText(tr("Object Manager"));
    if (d_btn_add_curve) {
        d_btn_add_curve->setText(tr("+ Add"));
        d_btn_add_curve->setToolTip(tr("Add Curve(s) from Worksheet"));
    }
    if (d_btn_move_up)
        d_btn_move_up->setToolTip(tr("Move Selected Curve Up"));
    if (d_btn_move_down)
        d_btn_move_down->setToolTip(tr("Move Selected Curve Down"));
    if (d_btn_remove_obj)
        d_btn_remove_obj->setToolTip(tr("Remove Selected Object"));
    if (d_empty_hint_lbl)
        d_empty_hint_lbl->setText(tr("Activate or create a 2D graph to inspect its layers, axes, curves, and legend."));

    // Quick Start Empty State
    if (d_qs_title_lbl)
        d_qs_title_lbl->setText(tr("Quick Start Guide"));
    if (d_qs_subtitle_lbl)
        d_qs_subtitle_lbl->setText(tr("Create publication-ready graphs in 3 easy steps"));
    if (d_qs_step1_lbl)
        d_qs_step1_lbl->setText(
                tr("Paste from Excel into the worksheet (Ctrl+V) or drop an Excel (.xlsx) / CSV file"));
    if (d_qs_step2_lbl)
        d_qs_step2_lbl->setText(
                tr("Click 'Line+Sym' on the Worksheet bar, or pick a graph type from the 'Plot' tab"));
    if (d_qs_step3_lbl)
        d_qs_step3_lbl->setText(tr("Drag graph frame to resize; click axes or legend to edit"));
    if (d_btn_qs_import_csv) {
        d_btn_qs_import_csv->setText(tr("Import Excel (.xlsx) / CSV..."));
        d_btn_qs_import_csv->setToolTip(
                tr("Open an Excel workbook (.xlsx/.xlsm) or CSV/TSV data file into a worksheet"));
    }
    if (d_btn_qs_new_graph) {
        d_btn_qs_new_graph->setText(tr("Plot Line + Symbol"));
        d_btn_qs_new_graph->setToolTip(
                tr("Plot the active worksheet as a Line + Symbol graph"));
    }
    if (d_btn_qs_sample_gallery)
        d_btn_qs_sample_gallery->setText(tr("Sample Gallery / Plot"));
    if (d_btn_qs_help) {
        d_btn_qs_help->setText(tr("Guide (F1)"));
        d_btn_qs_help->setToolTip(tr("Open the ToriGraph Quick Guide (F1)"));
    }
    updateQuickStartPlotButtonState();

    // H7: the tab buttons carry the same explanation as the collapsed-rail buttons.
    if (d_tab_btn_page) {
        d_tab_btn_page->setText(tr("Overall"));
        d_tab_btn_page->setToolTip(tr("Page & Layer Properties"));
        d_tab_btn_page->setAccessibleName(tr("Page & Layer Properties"));
    }
    if (d_tab_btn_axis) {
        d_tab_btn_axis->setText(tr("Axis"));
        d_tab_btn_axis->setToolTip(tr("Axis Scale & Title Properties"));
        d_tab_btn_axis->setAccessibleName(tr("Axis Scale & Title Properties"));
    }
    if (d_tab_btn_curve) {
        d_tab_btn_curve->setText(tr("Curve"));
        d_tab_btn_curve->setToolTip(tr("Curve & Series Style Properties"));
        d_tab_btn_curve->setAccessibleName(tr("Curve & Series Style Properties"));
    }
    if (d_tab_btn_legend) {
        d_tab_btn_legend->setText(tr("Legend"));
        d_tab_btn_legend->setToolTip(tr("Legend & Annotation Properties"));
        d_tab_btn_legend->setAccessibleName(tr("Legend & Annotation Properties"));
    }

    // Page 0: Page / Layer
    if (d_sec_presets_lbl)
        d_sec_presets_lbl->setText(tr("1-Click Publication & Slide Presets"));
    if (d_btn_preset_isij) {
        d_btn_preset_isij->setText(tr("ISIJ 85mm"));
        d_btn_preset_isij->setToolTip(
                tr("85 mm single-column journal preset with Arial fonts and inward ticks"));
    }
    if (d_btn_preset_slide28) {
        d_btn_preset_slide28->setText(tr("Slide (18pt)"));
        d_btn_preset_slide28->setToolTip(
                tr("83.3 mm square, 12.9 pt Arial. Becomes 18 pt on the slide with 'Send to PowerPoint' (140% paste)."));
    }
    if (d_btn_apply_user_preset) {
        d_btn_apply_user_preset->setText(tr("Apply"));
        const int w = qMax(42, d_btn_apply_user_preset->fontMetrics().horizontalAdvance(
                                       d_btn_apply_user_preset->text())
                                       + 16);
        d_btn_apply_user_preset->setMaximumWidth(w);
        d_btn_apply_user_preset->setFixedWidth(w);
    }
    if (d_btn_manage_presets)
        d_btn_manage_presets->setToolTip(tr("Manage Style Presets..."));
    if (d_cb_user_presets) {
        const QString prevId = d_cb_user_presets->currentData().toString();
        d_cb_user_presets->blockSignals(true);
        d_cb_user_presets->clear();
        const QVector<MultiLayer::GraphStylePresetSpec> presets = MultiLayer::listStylePresets();
        int selIdx = 0;
        int maxPopupW = qMax(180, d_cb_user_presets->width());
        QAbstractItemView *pv = d_cb_user_presets->view();
        const QFontMetrics pfm = pv ? pv->fontMetrics() : d_cb_user_presets->fontMetrics();
        for (int i = 0; i < presets.size(); ++i) {
            const QString dispName = presets[i].builtIn
                    ? QCoreApplication::translate("GraphInspectorDock",
                                                  presets[i].name.toUtf8().constData())
                    : presets[i].name;
            QString compactName;
            if (presets[i].id == QLatin1String("isij_85mm"))
                compactName = tr("ISIJ 1-Col (85 mm)");
            else if (presets[i].id == QLatin1String("slide_28pt"))
                compactName = tr("Large Slide (185 mm)");
            else if (presets[i].id == QLatin1String("lab_slide_83mm"))
                compactName = tr("Lab Slide (18 pt, 83.3 mm)");
            else if (presets[i].id == QLatin1String("journal_2col_175mm"))
                compactName = tr("Journal 2-Col (175 mm)");
            d_cb_user_presets->addItem(dispName, presets[i].id);
            d_cb_user_presets->setItemData(i, dispName, Qt::ToolTipRole);
            if (!compactName.isEmpty())
                d_cb_user_presets->setItemData(i, compactName, Qt::UserRole + 1);
            maxPopupW = qMax(maxPopupW, pfm.horizontalAdvance(dispName) + 36);
            if (!prevId.isEmpty() && presets[i].id == prevId)
                selIdx = i;
        }
        if (pv)
            pv->setMinimumWidth(qBound(180, maxPopupW, 520));
        if (d_cb_user_presets->count() > 0)
            d_cb_user_presets->setCurrentIndex(selIdx);
        const int curIdx = d_cb_user_presets->currentIndex();
        const QString fullTip = (curIdx >= 0)
                ? d_cb_user_presets->itemData(curIdx, Qt::ToolTipRole).toString()
                : QString();
        d_cb_user_presets->setToolTip(
                fullTip.isEmpty() ? d_cb_user_presets->currentText() : fullTip);
        d_cb_user_presets->blockSignals(false);
    }
    if (d_btn_square_frame70)
        d_btn_square_frame70->setText(tr("Square Frame (70%)"));

    if (d_sec_aspect_lbl)
        d_sec_aspect_lbl->setText(tr("Aspect Ratio & Resizing"));
    if (d_chk_keep_aspect)
        d_chk_keep_aspect->setText(tr("Lock Aspect Ratio"));
    if (d_btn_ratio_1_1) {
        d_btn_ratio_1_1->setText(QStringLiteral("1:1"));
        d_btn_ratio_1_1->setToolTip(tr("1:1 Square"));
    }
    if (d_btn_ratio_4_3) {
        d_btn_ratio_4_3->setText(QStringLiteral("4:3"));
        d_btn_ratio_4_3->setToolTip(tr("4:3 Landscape"));
    }
    if (d_btn_ratio_16_9) {
        d_btn_ratio_16_9->setText(QStringLiteral("16:9"));
        d_btn_ratio_16_9->setToolTip(tr("16:9 Wide"));
    }
    if (d_btn_ratio_3_4) {
        d_btn_ratio_3_4->setText(QStringLiteral("3:4"));
        d_btn_ratio_3_4->setToolTip(tr("3:4 Portrait"));
    }
    if (d_btn_ratio_free) {
        d_btn_ratio_free->setText(tr("Free"));
        d_btn_ratio_free->setToolTip(tr("Unlock Aspect Ratio (Free Resize)"));
    }

    if (d_sec_page_size_lbl)
        d_sec_page_size_lbl->setText(tr("Page Size & View"));
    if (d_lbl_page_unit)
        d_lbl_page_unit->setText(tr("Unit:"));
    if (d_lbl_page_w)
        d_lbl_page_w->setText(tr("Width:"));
    if (d_lbl_page_h)
        d_lbl_page_h->setText(tr("Height:"));
    if (d_lbl_page_dpi)
        d_lbl_page_dpi->setText(tr("DPI:"));
    if (d_chk_page_view)
        d_chk_page_view->setText(tr("Print Page View"));

    const QString pendingEditTip =
            tr("Enter values and press Enter to apply (amber highlight = unsaved edit)");
    if (d_sec_inset_lbl)
        d_sec_inset_lbl->setText(tr("Inset Zoom Region"));
    if (d_lbl_inset_xmin)
        d_lbl_inset_xmin->setText(tr("X Min:"));
    if (d_lbl_inset_xmax)
        d_lbl_inset_xmax->setText(tr("X Max:"));
    if (d_lbl_inset_ymin)
        d_lbl_inset_ymin->setText(tr("Y Min:"));
    if (d_lbl_inset_ymax)
        d_lbl_inset_ymax->setText(tr("Y Max:"));
    for (QLineEdit *le :
         { d_edit_inset_xmin, d_edit_inset_xmax, d_edit_inset_ymin, d_edit_inset_ymax }) {
        if (le)
            le->setToolTip(pendingEditTip);
    }
    if (d_chk_inset_connectors)
        d_chk_inset_connectors->setText(tr("Show Connector Lines"));

    if (d_sec_layer_frame_lbl)
        d_sec_layer_frame_lbl->setText(tr("Frame, Linked Axes & Panels"));
    if (d_btn_box_inward)
        d_btn_box_inward->setText(tr("Box Frame + Inward Ticks"));
    if (d_btn_link_right_y)
        d_btn_link_right_y->setText(tr("+ Link Right-Y Axis"));
    if (d_btn_link_top_x)
        d_btn_link_top_x->setText(tr("+ Link Top-X Axis (Arrhenius)"));
    if (d_chk_panel_labels)
        d_chk_panel_labels->setText(tr("Panel Labels (a), (b)"));
    if (d_combo_panel_pos && d_combo_panel_pos->count() >= 2) {
        d_combo_panel_pos->setItemText(0, tr("Outside Top-Left"));
        d_combo_panel_pos->setItemText(1, tr("Inside Top-Left"));
    }

    // Page 1: Axis
    if (d_btn_axis_bottom)
        d_btn_axis_bottom->setText(tr("Bottom X"));
    if (d_btn_axis_left)
        d_btn_axis_left->setText(tr("Left Y"));
    if (d_btn_axis_top)
        d_btn_axis_top->setText(tr("Top X"));
    if (d_btn_axis_right)
        d_btn_axis_right->setText(tr("Right Y"));
    if (d_chk_axis_visible)
        d_chk_axis_visible->setText(tr("Show Axis"));
    if (d_chk_tick_labels)
        d_chk_tick_labels->setText(tr("Tick Labels"));

    if (d_sec_axis_range_lbl)
        d_sec_axis_range_lbl->setText(tr("Scale & Axis Break"));
    if (d_lbl_axis_min)
        d_lbl_axis_min->setText(tr("Min:"));
    if (d_lbl_axis_max)
        d_lbl_axis_max->setText(tr("Max:"));
    if (d_lbl_axis_step)
        d_lbl_axis_step->setText(tr("Step (0=Auto):"));
    for (QLineEdit *le :
         { d_edit_axis_min, d_edit_axis_max, d_edit_axis_step, d_edit_break_from, d_edit_break_to }) {
        if (le)
            le->setToolTip(pendingEditTip);
    }
    if (d_btn_axis_autoscale)
        d_btn_axis_autoscale->setText(tr("Auto Range"));
    if (d_lbl_axis_scale_type)
        d_lbl_axis_scale_type->setText(tr("Scale:"));
    if (d_combo_axis_scale && d_combo_axis_scale->count() >= 2) {
        d_combo_axis_scale->setItemText(0, tr("Linear"));
        d_combo_axis_scale->setItemText(1, tr("Log10"));
    }
    if (d_chk_axis_reverse)
        d_chk_axis_reverse->setText(tr("Reverse Axis"));
    if (d_lbl_minor_subdiv)
        d_lbl_minor_subdiv->setText(tr("Minor Ticks:"));
    if (d_combo_minor_subdiv) {
        fillMinorTickCombo(d_combo_minor_subdiv,
                           d_combo_minor_subdiv->property("minorLogMode").toBool(),
                           minorTickComboValue(d_combo_minor_subdiv, 0));
        d_combo_minor_subdiv->setToolTip(
                tr("Number of minor ticks between major ticks. On a log axis only 0, 2, 4 or 8 "
                   "are meaningful (8 = 2, 3, ..., 9)."));
    }
    if (d_chk_axis_break)
        d_chk_axis_break->setText(tr("Axis Break (Omit Middle Range)"));
    if (d_lbl_break_from)
        d_lbl_break_from->setText(tr("Break Start:"));
    if (d_lbl_break_to)
        d_lbl_break_to->setText(tr("Break End:"));
    if (d_lbl_break_scale_before)
        d_lbl_break_scale_before->setText(tr("Before Break:"));
    if (d_combo_break_scale_before && d_combo_break_scale_before->count() >= 2) {
        d_combo_break_scale_before->setItemText(0, tr("Linear"));
        d_combo_break_scale_before->setItemText(1, tr("Log10"));
    }
    if (d_lbl_break_scale_after)
        d_lbl_break_scale_after->setText(tr("After Break:"));
    if (d_combo_break_scale_after && d_combo_break_scale_after->count() >= 2) {
        d_combo_break_scale_after->setItemText(0, tr("Linear"));
        d_combo_break_scale_after->setItemText(1, tr("Log10"));
    }
    if (d_sec_ref_lines_lbl)
        d_sec_ref_lines_lbl->setText(tr("Reference Lines"));
    if (d_btn_ref_zero_y) {
        d_btn_ref_zero_y->setText(QStringLiteral("y = 0"));
        d_btn_ref_zero_y->setToolTip(tr("Toggle horizontal dashed y = 0 reference line"));
    }
    if (d_btn_ref_zero_x) {
        d_btn_ref_zero_x->setText(QStringLiteral("x = 0"));
        d_btn_ref_zero_x->setToolTip(tr("Toggle vertical dashed x = 0 reference line"));
    }
    if (d_btn_ref_add_custom) {
        d_btn_ref_add_custom->setText(tr("+ Add..."));
        d_btn_ref_add_custom->setToolTip(
                tr("Add a custom constant or statistical reference line"));
    }

    if (d_sec_axis_ticks_lbl)
        d_sec_axis_ticks_lbl->setText(tr("Ticks & Grids"));
    if (d_lbl_tick_dir)
        d_lbl_tick_dir->setText(tr("Ticks:"));
    if (d_combo_tick_dir && d_combo_tick_dir->count() >= 4) {
        d_combo_tick_dir->setItemText(0, tr("Inward"));
        d_combo_tick_dir->setItemText(1, tr("Outward"));
        d_combo_tick_dir->setItemText(2, tr("Both Sides"));
        d_combo_tick_dir->setItemText(3, tr("None"));
    }
    if (d_lbl_tick_format)
        d_lbl_tick_format->setText(tr("Format:"));
    if (d_combo_tick_format && d_combo_tick_format->count() >= 5) {
        d_combo_tick_format->setItemText(0, tr("Automatic"));
        d_combo_tick_format->setItemText(1, tr("Decimal (0.00)"));
        d_combo_tick_format->setItemText(2, tr("Scientific (10ˣ)"));
        d_combo_tick_format->setItemText(3, tr("Scientific (1E3)"));
        d_combo_tick_format->setItemText(4, tr("Engineering"));
    }
    if (d_lbl_tick_prec)
        d_lbl_tick_prec->setText(tr("Precision:"));
    if (d_spin_tick_prec)
        d_spin_tick_prec->setSpecialValueText(tr("Auto"));
    if (d_lbl_tick_divide)
        d_lbl_tick_divide->setText(tr("Divide by:"));
    if (d_lbl_category_col)
        d_lbl_category_col->setText(tr("Categories:"));
    refreshCategoryColumnCombo();
    if (d_chk_major_grid)
        d_chk_major_grid->setText(tr("Major Grid"));
    if (d_chk_minor_grid)
        d_chk_minor_grid->setText(tr("Minor Grid"));

    if (d_sec_axis_title_lbl)
        d_sec_axis_title_lbl->setText(tr("Axis Label & Font (Arial)"));
    if (d_lbl_unit_format)
        d_lbl_unit_format->setText(tr("Unit Format:"));
    if (d_combo_unit_format && d_combo_unit_format->count() >= 3) {
        d_combo_unit_format->setItemText(0, tr("Name (Unit)"));
        d_combo_unit_format->setItemText(1, tr("Name / Unit (ISIJ)"));
        d_combo_unit_format->setItemText(2, tr("Name [Unit]"));
    }
    if (d_edit_axis_title)
        d_edit_axis_title->setPlaceholderText(tr("Enter axis label..."));
    updateAxisTitlePreview(d_edit_axis_title ? d_edit_axis_title->text() : QString());
    rebuildRichTextBarMenus();
    if (d_chk_lock_all_axes_font) {
        d_chk_lock_all_axes_font->setText(tr("Apply Font Size to All 4 Axes"));
        d_chk_lock_all_axes_font->setToolTip(
                tr("Scale tick number font sizes proportionally and apply to all 4 axes together"));
    }
    if (d_lbl_title_pt)
        d_lbl_title_pt->setText(tr("Label Size:"));
    if (d_lbl_tick_pt)
        d_lbl_tick_pt->setText(tr("Tick Numbers:"));

    // Page 2: Curve / Series / ErrorBars / Heatmap
    if (d_lbl_plot_style)
        d_lbl_plot_style->setText(tr("Plot Style:"));
    if (d_combo_curve_style && d_combo_curve_style->count() >= 10) {
        d_combo_curve_style->setItemText(0, tr("Line"));
        d_combo_curve_style->setItemText(1, tr("Scatter"));
        d_combo_curve_style->setItemText(2, tr("Line + Symbol"));
        d_combo_curve_style->setItemText(3, tr("Spline"));
        d_combo_curve_style->setItemText(4, tr("Horizontal Steps"));
        d_combo_curve_style->setItemText(5, tr("Vertical Steps"));
        d_combo_curve_style->setItemText(6, tr("Drop Lines"));
        d_combo_curve_style->setItemText(7, tr("Vertical Bars"));
        d_combo_curve_style->setItemText(8, tr("Horizontal Bars"));
        d_combo_curve_style->setItemText(9, tr("Area"));
        if (d_combo_curve_style->count() > 10)
            syncCurveControls();
    }
    if (d_sec_line_style_lbl)
        d_sec_line_style_lbl->setText(tr("Line Appearance"));
    if (d_lbl_line_color)
        d_lbl_line_color->setText(tr("Color:"));
    if (d_lbl_line_width)
        d_lbl_line_width->setText(tr("Line Width:"));
    if (d_lbl_line_pen)
        d_lbl_line_pen->setText(tr("Line Style:"));
    if (d_combo_line_pen && d_combo_line_pen->count() >= 5) {
        d_combo_line_pen->setItemText(0, tr("Solid Line"));
        d_combo_line_pen->setItemText(1, tr("Dash"));
        d_combo_line_pen->setItemText(2, tr("Dot"));
        d_combo_line_pen->setItemText(3, tr("Dash-Dot"));
        d_combo_line_pen->setItemText(4, tr("None"));
    }
    if (d_lbl_curve_opacity)
        d_lbl_curve_opacity->setText(tr("Opacity:"));

    if (d_sec_symbol_style_lbl)
        d_sec_symbol_style_lbl->setText(tr("Symbol & Axis Binding"));
    if (d_lbl_sym_shape)
        d_lbl_sym_shape->setText(tr("Shape:"));
    if (d_combo_sym_shape && d_combo_sym_shape->count() >= 12) {
        d_combo_sym_shape->setItemText(0, tr("Circle"));
        d_combo_sym_shape->setItemText(1, tr("Square Symbol"));
        d_combo_sym_shape->setItemText(2, tr("Diamond"));
        d_combo_sym_shape->setItemText(3, tr("Up Triangle"));
        d_combo_sym_shape->setItemText(4, tr("Down Triangle"));
        d_combo_sym_shape->setItemText(5, tr("Left Triangle"));
        d_combo_sym_shape->setItemText(6, tr("Right Triangle"));
        d_combo_sym_shape->setItemText(7, tr("Cross (+)"));
        d_combo_sym_shape->setItemText(8, tr("X-Cross (x)"));
        d_combo_sym_shape->setItemText(9, tr("Star"));
        d_combo_sym_shape->setItemText(10, tr("Hexagon"));
        d_combo_sym_shape->setItemText(11, tr("None"));
    }
    if (d_lbl_sym_size)
        d_lbl_sym_size->setText(tr("Symbol Size:"));
    if (d_spin_sym_size)
        d_spin_sym_size->setSuffix(QStringLiteral(" pt"));
    if (d_lbl_sym_skip)
        d_lbl_sym_skip->setText(tr("Symbol Skip:"));
    if (d_spin_sym_skip) {
        d_spin_sym_skip->setSpecialValueText(tr("Auto"));
        d_spin_sym_skip->setToolTip(
                tr("Skip symbols to avoid crowding on dense data (0 = Auto, 1 = All points, N = show 1 in N)"));
    }
    if (d_chk_sym_hollow)
        d_chk_sym_hollow->setText(tr("Hollow Symbol (White Fill)"));
    if (d_chk_link_sym_color)
        d_chk_link_sym_color->setText(tr("Link Symbol Color to Line"));
    if (d_lbl_sym_edge_color)
        d_lbl_sym_edge_color->setText(tr("Edge Color:"));
    if (d_lbl_sym_fill_color)
        d_lbl_sym_fill_color->setText(tr("Fill Color:"));
    if (d_lbl_sym_edge_width)
        d_lbl_sym_edge_width->setText(tr("Edge Width:"));
    if (d_chk_curve_right_y)
        d_chk_curve_right_y->setText(tr("Plot on Right-Y"));
    if (d_btn_match_right_y_color) {
        d_btn_match_right_y_color->setText(tr("Match Right-Y Color"));
        d_btn_match_right_y_color->setToolTip(
                tr("Color Right-Y axis backbone, ticks, and title to match this curve"));
    }

    if (d_sec_err_bars_lbl)
        d_sec_err_bars_lbl->setText(tr("Error Bars"));
    if (d_btn_err_adjacent) {
        d_btn_err_adjacent->setText(tr("Adjacent Col"));
        d_btn_err_adjacent->setToolTip(
                tr("Use the column to the right of the Y column as ± error bars "
                   "(click again to remove)"));
    }
    if (d_btn_err_dialog) {
        d_btn_err_dialog->setText(tr("Add Error Bars..."));
        d_btn_err_dialog->setToolTip(
                tr("Open the Add Error Bars dialog to select an error column or percentage"));
    }
    if (d_sec_err_style_lbl)
        d_sec_err_style_lbl->setText(tr("Error Bar Style"));
    if (d_lbl_err_width)
        d_lbl_err_width->setText(tr("Bar Width:"));
    if (d_lbl_err_cap)
        d_lbl_err_cap->setText(tr("Cap Length:"));
    if (d_lbl_err_dir)
        d_lbl_err_dir->setText(tr("Direction:"));
    if (d_combo_err_dir && d_combo_err_dir->count() >= 3) {
        d_combo_err_dir->setItemText(0, tr("Both (±)"));
        d_combo_err_dir->setItemText(1, tr("Plus (+)"));
        d_combo_err_dir->setItemText(2, tr("Minus (-)"));
    }
    if (d_lbl_err_color)
        d_lbl_err_color->setText(tr("Color:"));
    if (d_btn_err_match_curve)
        d_btn_err_match_curve->setText(tr("Match Curve"));
    if (d_chk_err_band)
        d_chk_err_band->setText(tr("Draw as Translucent Band"));
    if (d_lbl_err_band_opacity)
        d_lbl_err_band_opacity->setText(tr("Band Opacity:"));

    if (d_sec_pcm_lbl)
        d_sec_pcm_lbl->setText(tr("Scatter Colormap (Z Column)"));
    if (d_lbl_pcm_cmap)
        d_lbl_pcm_cmap->setText(tr("Colormap:"));
    if (d_combo_pcm_cmap && d_combo_pcm_cmap->count() >= 10) {
        d_combo_pcm_cmap->setItemText(0, tr("Default"));
        d_combo_pcm_cmap->setItemText(1, tr("Viridis"));
        d_combo_pcm_cmap->setItemText(2, tr("Magma"));
        d_combo_pcm_cmap->setItemText(3, tr("Inferno"));
        d_combo_pcm_cmap->setItemText(4, tr("Plasma"));
        d_combo_pcm_cmap->setItemText(5, tr("Cividis"));
        d_combo_pcm_cmap->setItemText(6, tr("Cool-Warm"));
        d_combo_pcm_cmap->setItemText(7, tr("Blue-White-Red"));
        d_combo_pcm_cmap->setItemText(8, tr("Jet"));
        d_combo_pcm_cmap->setItemText(9, tr("GrayScale"));
    }
    if (d_chk_pcm_reverse)
        d_chk_pcm_reverse->setText(tr("Reverse Colormap"));
    if (d_chk_pcm_colorbar)
        d_chk_pcm_colorbar->setText(tr("Show Colorbar"));
    if (d_lbl_pcm_cb_title)
        d_lbl_pcm_cb_title->setText(tr("Colorbar Title:"));
    if (d_edit_pcm_cb_title)
        d_edit_pcm_cb_title->setPlaceholderText(tr("Enter colorbar title..."));
    if (d_chk_pcm_custom_z)
        d_chk_pcm_custom_z->setText(tr("Custom Z Range"));
    if (d_lbl_pcm_zmin)
        d_lbl_pcm_zmin->setText(tr("Z Min:"));
    if (d_lbl_pcm_zmax)
        d_lbl_pcm_zmax->setText(tr("Z Max:"));

    if (d_sec_series_scheme_lbl)
        d_sec_series_scheme_lbl->setText(tr("Group Series Style Scheme"));
    refreshPaletteSchemeCombo();
    if (d_chk_cycle_sym_shapes)
        d_chk_cycle_sym_shapes->setText(tr("Cycle Symbol Shapes (● ■ ▲ ▼ ◆)"));
    if (d_chk_paired_hollow)
        d_chk_paired_hollow->setText(tr("Paired Filled / Hollow (● ○ ■ □)"));
    if (d_btn_apply_scheme_all)
        d_btn_apply_scheme_all->setText(tr("Apply to All Curves"));
    if (d_scroll_more_cue) {
        if (QLabel *cueLbl = d_scroll_more_cue->findChild<QLabel *>())
            cueLbl->setText(QString::fromUtf8("\xE2\x96\xBE ") + tr("More below"));
    }

    // Spectrogram / Heatmap
    if (d_sec_spec_lbl)
        d_sec_spec_lbl->setText(tr("Color Scale & Contours"));
    if (d_lbl_spec_cmap)
        d_lbl_spec_cmap->setText(tr("Colormap:"));
    if (d_combo_spec_cmap && d_combo_spec_cmap->count() >= 10) {
        d_combo_spec_cmap->setItemText(0, tr("Default"));
        // Proper colormap names also go through tr() so that the dictionary can add the
        // short description shown in the other colormap lists ("Viridis (知覚均等)", H3).
        d_combo_spec_cmap->setItemText(1, tr("Viridis"));
        d_combo_spec_cmap->setItemText(2, tr("Magma"));
        d_combo_spec_cmap->setItemText(3, tr("Inferno"));
        d_combo_spec_cmap->setItemText(4, tr("Plasma"));
        d_combo_spec_cmap->setItemText(5, tr("Cividis"));
        d_combo_spec_cmap->setItemText(8, tr("Jet"));
        d_combo_spec_cmap->setItemText(6, tr("Cool-Warm"));
        d_combo_spec_cmap->setItemText(7, tr("Blue-White-Red"));
        d_combo_spec_cmap->setItemText(9, tr("GrayScale"));
    }
    if (d_chk_spec_reverse)
        d_chk_spec_reverse->setText(tr("Reverse Colormap"));
    if (d_chk_spec_log_scale)
        d_chk_spec_log_scale->setText(tr("Log Color Scale"));
    if (d_chk_spec_cell_blocks)
        d_chk_spec_cell_blocks->setText(tr("Cell Blocks (No Smoothing)"));
    if (d_chk_spec_colorbar)
        d_chk_spec_colorbar->setText(tr("Show Colorbar"));
    if (d_lbl_spec_cb_title)
        d_lbl_spec_cb_title->setText(tr("Colorbar Title:"));
    if (d_edit_spec_cb_title)
        d_edit_spec_cb_title->setPlaceholderText(tr("Enter colorbar title..."));
    if (d_lbl_spec_cb_width)
        d_lbl_spec_cb_width->setText(tr("Colorbar Width:"));
    if (d_chk_spec_contours)
        d_chk_spec_contours->setText(tr("Show Contour Lines"));
    if (d_lbl_spec_levels)
        d_lbl_spec_levels->setText(tr("Contour Levels:"));
    if (d_chk_spec_custom_z)
        d_chk_spec_custom_z->setText(tr("Custom Z Range"));
    if (d_lbl_spec_zmin)
        d_lbl_spec_zmin->setText(tr("Z Min:"));
    if (d_lbl_spec_zmax)
        d_lbl_spec_zmax->setText(tr("Z Max:"));

    if (d_sec_y_stack_lbl)
        d_sec_y_stack_lbl->setText(tr("Non-Destructive Y-Offset Stack"));
    if (d_lbl_stack_gap)
        d_lbl_stack_gap->setText(tr("Vertical Gap:"));
    if (d_btn_apply_stack)
        d_btn_apply_stack->setText(tr("Stack Curves"));
    if (d_btn_reset_stack)
        d_btn_reset_stack->setText(tr("Reset Offsets"));

    // Page 3: Legend / Annotation
    if (d_sec_legend_pos_lbl)
        d_sec_legend_pos_lbl->setText(tr("1-Click Legend Placement"));
    // H7: placement buttons explain where the legend goes.
    const QString legTip = tr("Move the legend to the %1 corner inside the plot frame");
    if (d_btn_leg_tl) {
        d_btn_leg_tl->setText(tr("Top-Left"));
        d_btn_leg_tl->setToolTip(legTip.arg(tr("Top-Left")));
    }
    if (d_btn_leg_tc) {
        d_btn_leg_tc->setText(tr("Top-Center"));
        d_btn_leg_tc->setToolTip(tr("Move the legend to the top centre inside the plot frame"));
    }
    if (d_btn_leg_tr) {
        d_btn_leg_tr->setText(tr("Top-Right"));
        d_btn_leg_tr->setToolTip(legTip.arg(tr("Top-Right")));
    }
    if (d_btn_leg_bl) {
        d_btn_leg_bl->setText(tr("Bottom-Left"));
        d_btn_leg_bl->setToolTip(legTip.arg(tr("Bottom-Left")));
    }
    if (d_btn_leg_br) {
        d_btn_leg_br->setText(tr("Bottom-Right"));
        d_btn_leg_br->setToolTip(legTip.arg(tr("Bottom-Right")));
    }
    if (d_btn_leg_out_r) {
        d_btn_leg_out_r->setText(tr("Outside Right"));
        d_btn_leg_out_r->setToolTip(
                tr("Move the legend outside the plot frame, to the right of the layer"));
    }
    if (d_btn_leg_auto_avoid) {
        d_btn_leg_auto_avoid->setText(tr("Place in Empty Space (Avoid Data)"));
        d_btn_leg_auto_avoid->setToolTip(
                tr("Move the legend to the corner or edge position that overlaps the data least"));
    }
    if (d_btn_leg_rebuild) {
        d_btn_leg_rebuild->setText(tr("Reconstruct Legend"));
        d_btn_leg_rebuild->setToolTip(
                tr("Rebuild the legend text from the current curves (custom text is replaced)"));
    }

    if (d_sec_legend_style_lbl)
        d_sec_legend_style_lbl->setText(tr("Legend Frame & Typography (Arial)"));
    if (d_lbl_leg_frame)
        d_lbl_leg_frame->setText(tr("Frame:"));
    if (d_combo_leg_frame && d_combo_leg_frame->count() >= 3) {
        d_combo_leg_frame->setItemText(0, tr("Borderless"));
        d_combo_leg_frame->setItemText(1, tr("Black Box + White Fill"));
        d_combo_leg_frame->setItemText(2, tr("Transparent Background"));
    }
    if (d_lbl_leg_header) {
        Graph *g = activeLayer();
        const bool isTextAnn =
                (g && g->currentSelection().kind == Graph::GraphSelection::TextAnnotation);
        d_lbl_leg_header->setText(isTextAnn ? tr("Text:") : tr("Header:"));
    }
    if (d_edit_annot_text)
        d_edit_annot_text->setToolTip(
                tr("Edit the text. Each line of a multi-line label is one line here."));
    if (d_lbl_leg_cols)
        d_lbl_leg_cols->setText(tr("Columns:"));
    if (d_lbl_leg_font_pt)
        d_lbl_leg_font_pt->setText(tr("Font Size:"));
    if (d_sec_leg_entries_lbl)
        d_sec_leg_entries_lbl->setText(tr("Curve Legend Labels"));

    // Arrow / Line Annotation editor (A3-5, C2-3)
    if (d_sec_arrow_lbl)
        d_sec_arrow_lbl->setText(tr("Arrow Style"));
    if (d_lbl_arrow_color)
        d_lbl_arrow_color->setText(tr("Color:"));
    if (d_lbl_arrow_width)
        d_lbl_arrow_width->setText(tr("Width:"));
    if (d_lbl_arrow_style)
        d_lbl_arrow_style->setText(tr("Line Style:"));
    if (d_combo_arrow_style && d_combo_arrow_style->count() >= 4) {
        d_combo_arrow_style->setItemText(0, tr("Solid Line"));
        d_combo_arrow_style->setItemText(1, tr("Dash"));
        d_combo_arrow_style->setItemText(2, tr("Dot"));
        d_combo_arrow_style->setItemText(3, tr("Dash-Dot"));
    }
    if (d_chk_arrow_start)
        d_chk_arrow_start->setText(tr("Arrow at Start"));
    if (d_chk_arrow_end)
        d_chk_arrow_end->setText(tr("Arrow at End"));
    if (d_lbl_arrow_head_len)
        d_lbl_arrow_head_len->setText(tr("Head Length:"));
    if (d_lbl_arrow_head_angle)
        d_lbl_arrow_head_angle->setText(tr("Head Angle:"));
    if (d_chk_arrow_filled)
        d_chk_arrow_filled->setText(tr("Filled Head"));

    // Image Annotation editor (A3-5, C2-3)
    if (d_sec_img_lbl)
        d_sec_img_lbl->setText(tr("Image Geometry"));
    if (d_lbl_img_filename)
        d_lbl_img_filename->setText(tr("File: (none)"));
    if (!d_target_ml) {
        if (d_header_target_lbl)
            d_header_target_lbl->setText(tr("(No Active Graph)"));
        if (d_lbl_active_curve_name)
            d_lbl_active_curve_name->setText(tr("(No Curve Selected)"));
        if (d_lbl_legend_target)
            d_lbl_legend_target->setText(tr("Graph Legend"));
    } else {
        if (currentInspectorPage() != PageTabCurve && d_lbl_active_curve_name)
            d_lbl_active_curve_name->setText(tr("(No Curve Selected)"));
        if (currentInspectorPage() != PageTabLegend && d_lbl_legend_target)
            d_lbl_legend_target->setText(tr("Graph Legend"));
    }
    if (d_lbl_img_x)
        d_lbl_img_x->setText(tr("X:"));
    if (d_lbl_img_y)
        d_lbl_img_y->setText(tr("Y:"));
    if (d_lbl_img_w)
        d_lbl_img_w->setText(tr("Width:"));
    if (d_lbl_img_h)
        d_lbl_img_h->setText(tr("Height:"));
    if (d_btn_img_replace)
        d_btn_img_replace->setText(tr("Replace Image..."));

    // Plot Title editor (A3-5)
    if (d_sec_plot_title_lbl)
        d_sec_plot_title_lbl->setText(tr("Title Appearance"));
    if (d_lbl_plot_title_text)
        d_lbl_plot_title_text->setText(tr("Title Text:"));
    if (d_edit_plot_title)
        d_edit_plot_title->setPlaceholderText(tr("Enter plot title..."));
    if (d_lbl_plot_title_pt)
        d_lbl_plot_title_pt->setText(tr("Font Size:"));
    if (d_chk_plot_title_bold)
        d_chk_plot_title_bold->setText(tr("Bold"));
    if (d_lbl_plot_title_color)
        d_lbl_plot_title_color->setText(tr("Text Color:"));

    for (QToolButton *tb : findChildren<QToolButton *>()) {
        if (!tb)
            continue;
        const QString tipFmt = tb->property("_tg_tip_fmt").toString();
        if (tipFmt.isEmpty())
            continue;
        const QString translated =
                QCoreApplication::translate("QObject", tipFmt.toUtf8().constData());
        const QString tipArg = tb->property("_tg_tip_arg").toString();
        tb->setToolTip(tipArg.isEmpty() ? translated : translated.arg(tipArg));
    }

    // C2 "Details..." button (built once in the constructor, so it is retranslated here).
    if (QPushButton *detailsBtn = findChild<QPushButton *>(QStringLiteral("InspectorDetailsButton"))) {
        detailsBtn->setText(tr("Details..."));
        detailsBtn->setToolTip(tr("Open the dialog with every setting for this section"));
    }
    // H7 colour swatches: the automatic tooltip contains the colour ("... (now #000000)"), so a
    // text lookup cannot translate it after a language switch. Rebuild it from the colour.
    for (QPushButton *sw : findChildren<QPushButton *>(QStringLiteral("InspectorColorSwatchBtn"))) {
        if (!sw)
            continue;
        const QString autoTip = sw->property("autoSwatchTip").toString();
        if (autoTip.isEmpty() || sw->toolTip() != autoTip)
            continue; // a specific tooltip set elsewhere
        const QString hex = sw->text().trimmed();
        const QString tip =
                QCoreApplication::translate("GraphInspectorDock", "Click to choose a color (now %1)")
                        .arg(hex);
        sw->setToolTip(tip);
        sw->setProperty("autoSwatchTip", tip);
        sw->setAccessibleName(QCoreApplication::translate("GraphInspectorDock", "Color %1").arg(hex));
    }

    // A5-1: retranslate Object Manager tree nodes and dynamic labels immediately on language switch.
    refreshObjectTree();
    syncInspectorFromSelection();
}
