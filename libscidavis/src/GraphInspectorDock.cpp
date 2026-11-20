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

#include "ApplicationWindow.h"
#include "ArrowMarker.h"
#include "FunctionCurve.h"
#include "GraphMiniToolbar.h"
#include "Grid.h"
#include "I18nJapanese.h"
#include "IconLoader.h"
#include "ImageMarker.h"
#include "Legend.h"
#include "ModernUiTheme.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"
#include "ScaleDraw.h"
#include "Spectrogram.h"
#include "SymbolBox.h"
#include "Table.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
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

enum TreeItemRoles {
    RoleSelKind = Qt::UserRole + 1,
    RolePrimaryId = Qt::UserRole + 2,
    RoleSecondaryId = Qt::UserRole + 3,
    RoleLayerIndex = Qt::UserRole + 4,
    RoleIsGroupHeader = Qt::UserRole + 5
};

constexpr int kMergePageDims = 3101;
constexpr int kMergeAxisScale = 3201;
constexpr int kMergeAxisBreak = 3202;
constexpr int kMergeAxisTitle = 3203;
constexpr int kMergeAxisTitlePt = 3204;
constexpr int kMergeAxisTickPt = 3205;
constexpr int kMergeTickPrec = 3206;
constexpr int kMergeCurveWidth = 3301;
constexpr int kMergeSymSize = 3302;
constexpr int kMergeCurveOpacity = 3303;
constexpr int kMergeSymEdgeWidth = 3304;
constexpr int kMergeErrWidth = 3305;
constexpr int kMergeErrCap = 3306;
constexpr int kMergeSpecCbWidth = 3307;
constexpr int kMergeSpecLevels = 3308;
constexpr int kMergeSymSkip = 3309;
constexpr int kMergeSpecCbTitle = 3310;
constexpr int kMergeLegendHeader = 3401;
constexpr int kMergeLegendCols = 3402;
constexpr int kMergeLegendFontPt = 3403;

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

static QWidget *createMatSciRichTextBar(QWidget *parent, QLineEdit *targetEdit)
{
    QWidget *wrap = new QWidget(parent);
    QVBoxLayout *vlay = new QVBoxLayout(wrap);
    vlay->setContentsMargins(0, 1, 0, 1);
    vlay->setSpacing(2);

    auto insertToken = [targetEdit](const QString &tok) {
        if (!targetEdit || !targetEdit->isEnabled())
            return;
        if (targetEdit->hasSelectedText()) {
            const int start = targetEdit->selectionStart();
            const int len = targetEdit->selectedText().length();
            QString txt = targetEdit->text();
            txt.replace(start, len, tok);
            targetEdit->setText(txt);
            targetEdit->setCursorPosition(start + tok.length());
        } else {
            const int pos = targetEdit->cursorPosition();
            QString txt = targetEdit->text();
            txt.insert(pos, tok);
            targetEdit->setText(txt);
            targetEdit->setCursorPosition(pos + tok.length());
        }
        targetEdit->setModified(true);
        targetEdit->setFocus();
        emit targetEdit->editingFinished();
    };

    auto wrapMarkup = [targetEdit](const QString &openTag, const QString &closeTag) {
        if (!targetEdit || !targetEdit->isEnabled())
            return;
        QString txt = targetEdit->text();
        if (targetEdit->hasSelectedText()) {
            const int start = targetEdit->selectionStart();
            const QString sel = targetEdit->selectedText();
            const QString rep = openTag + sel + closeTag;
            txt.replace(start, sel.length(), rep);
            targetEdit->setText(txt);
            targetEdit->setSelection(start + openTag.length(), sel.length());
        } else {
            const int pos = targetEdit->cursorPosition();
            txt.insert(pos, openTag + closeTag);
            targetEdit->setText(txt);
            targetEdit->setCursorPosition(pos + openTag.length());
        }
        targetEdit->setModified(true);
        targetEdit->setFocus();
        emit targetEdit->editingFinished();
    };

    auto makeBtn = [&](QHBoxLayout *row, const QString &label, const QString &tip) -> QToolButton * {
        QToolButton *b = new QToolButton(wrap);
        b->setText(label);
        b->setToolTip(tip);
        b->setFixedHeight(21);
        b->setMinimumWidth(22);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(b);
        return b;
    };

    QHBoxLayout *row1 = new QHBoxLayout();
    row1->setContentsMargins(0, 0, 0, 0);
    row1->setSpacing(2);
    const QList<QPair<QString, QString>> row1Toks = {
        { QString::fromUtf8("\xCF\x83"), QString::fromUtf8("\xCF\x83") },
        { QString::fromUtf8("\xCE\xB5"), QString::fromUtf8("\xCE\xB5") },
        { QString::fromUtf8("2\xCE\xB8"), QString::fromUtf8("2\xCE\xB8") },
        { QString::fromUtf8("\xCE\xBC" "m"), QString::fromUtf8("\xCE\xBC" "m") },
        { QString::fromUtf8("\xC2\xB0" "C"), QString::fromUtf8("\xC2\xB0" "C") },
        { QString::fromUtf8("\xC3\x85"), QString::fromUtf8("\xC3\x85") },
    };
    for (const auto &pr : row1Toks) {
        QToolButton *b = makeBtn(row1, pr.first, QObject::tr("Insert %1").arg(pr.first));
        const QString tok = pr.second;
        QObject::connect(b, &QToolButton::clicked, wrap, [insertToken, tok]() { insertToken(tok); });
    }
    vlay->addLayout(row1);

    QHBoxLayout *row2 = new QHBoxLayout();
    row2->setContentsMargins(0, 0, 0, 0);
    row2->setSpacing(2);
    {
        QToolButton *bPm = makeBtn(row2, QString::fromUtf8("\xC2\xB1"),
                                   QObject::tr("Insert %1").arg(QString::fromUtf8("\xC2\xB1")));
        QObject::connect(bPm, &QToolButton::clicked, wrap,
                         [insertToken]() { insertToken(QString::fromUtf8("\xC2\xB1")); });
        QToolButton *bDeg = makeBtn(row2, QString::fromUtf8("\xC2\xB0"),
                                    QObject::tr("Insert %1").arg(QString::fromUtf8("\xC2\xB0")));
        QObject::connect(bDeg, &QToolButton::clicked, wrap,
                         [insertToken]() { insertToken(QString::fromUtf8("\xC2\xB0")); });
        QToolButton *bCm = makeBtn(row2, QString::fromUtf8("cm\xE2\x81\xBB\xC2\xB9"),
                                   QObject::tr("Insert %1").arg(QString::fromUtf8("cm\xE2\x81\xBB\xC2\xB9")));
        QObject::connect(bCm, &QToolButton::clicked, wrap,
                         [insertToken]() { insertToken(QStringLiteral("cm<sup>-1</sup>")); });
        QToolButton *bSub = makeBtn(row2, QString::fromUtf8("x\xE2\x82\x82"),
                                    QObject::tr("Subscript (<sub>...</sub>)"));
        QObject::connect(bSub, &QToolButton::clicked, wrap,
                         [wrapMarkup]() { wrapMarkup(QStringLiteral("<sub>"), QStringLiteral("</sub>")); });
        QToolButton *bSup = makeBtn(row2, QString::fromUtf8("x\xC2\xB2"),
                                    QObject::tr("Superscript (<sup>...</sup>)"));
        QObject::connect(bSup, &QToolButton::clicked, wrap,
                         [wrapMarkup]() { wrapMarkup(QStringLiteral("<sup>"), QStringLiteral("</sup>")); });
        QToolButton *bIt = makeBtn(row2, QStringLiteral("I"), QObject::tr("Italic (<i>...</i>)"));
        QFont itF = bIt->font();
        itF.setItalic(true);
        bIt->setFont(itF);
        QObject::connect(bIt, &QToolButton::clicked, wrap,
                         [wrapMarkup]() { wrapMarkup(QStringLiteral("<i>"), QStringLiteral("</i>")); });
    }
    vlay->addLayout(row2);
    return wrap;
}

static void registerInspectorExtraTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("Overall", "全体");
        reg("X/Y", "軸");
        reg("Scope:", "対象:");
        reg("Active Layer", "現在のレイヤ");
        reg("All Panels", "全パネル一括");
        reg("Apply Axis/Frame/Grid edits only to the active layer",
            "軸・枠・グリッドの変更を現在のレイヤのみに適用します");
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
        reg("Insert Symbol, Unit, or Journal Axis Template",
            "記号・単位・論文用軸ラベル定型を入力");
        reg("Symbols", "科学記号");
        reg("Journal Templates", "論文用軸ラベル定型");
        reg("Common Units", "よく使う単位");
        reg("Spline", "スプライン曲線");
        reg("Horizontal Steps", "水平ステップ");
        reg("Vertical Steps", "垂直ステップ");
        reg("Drop Lines", "ドロップライン");
        reg("Opacity:", "不透明度:");
        reg("Change Curve Opacity", "曲線の不透明度変更");
        reg("Link Symbol Color to Line", "記号の色を線の色に連動");
        reg("Edge Color:", "枠線の色:");
        reg("Fill Color:", "塗り色:");
        reg("Edge Width:", "枠線の太さ:");
        reg("Select Symbol Edge Color", "記号の枠線色を選択");
        reg("Select Symbol Fill Color", "記号の塗り色を選択");
        reg("Change Symbol Edge Color", "記号の枠線色変更");
        reg("Change Symbol Fill Color", "記号の塗り色変更");
        reg("Change Symbol Edge Width", "記号の枠線幅変更");
        reg("Link Symbol Color", "記号色を線の色に連動");
        reg("Plot on Right-Y", "右Y軸にプロット");
        reg("Match Right-Y Color", "右Y軸の色を曲線に合わせる");
        reg("Color Right-Y axis backbone, ticks, and title to match this curve",
            "右Y軸の軸線・目盛り・軸タイトル色をこの曲線の色に合わせます");
        reg("Match Right-Y Axis Color", "右Y軸の色を曲線に合わせる");
        reg("Cycle Symbol Shapes (● ■ ▲ ▼ ◆)", "記号形状を順に切替 (● ■ ▲ ▼ ◆)");
        reg("Paired Filled / Hollow (● ○ ■ □)", "塗り・白抜きペア切替 (● ○ ■ □)");
        reg("Error Bar Style", "エラーバー外観");
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
        reg("+ Link Top-X Axis (Arrhenius)", "+ 上X軸リンク (アレニウス等)");
        reg("Show Tick Numbers", "目盛り数値を表示");
        reg("Toggle Tick Numbers", "目盛り数値の表示切替");
        reg("Unit Format:", "単位表記:");
        reg("Unit Format", "単位表記");
        reg("Name (Unit)", "量名 (単位)");
        reg("Name / Unit (ISIJ)", "量名 / 単位 (ISIJ)");
        reg("Name [Unit]", "量名 [単位]");
        reg("Change Unit Format", "単位表記の変更");
        reg("Symbol Skip:", "記号の間引き:");
        reg("Symbol Skip", "記号の間引き");
        reg("Every %1 pts (0 = Auto)", "%1点ごと (0 = 自動間引き)");
        reg("Change Symbol Skip", "記号の間引き間隔変更");
        reg("Colorbar Title:", "カラーバータイトル:");
        reg("Colorbar Title", "カラーバータイトル");
        reg("Enter colorbar title...", "カラーバータイトルを入力...");
        reg("Edit Colorbar Title", "カラーバータイトルの編集");
    });
}

static void syncLinkedChildrenOf(MultiLayer *ml, Graph *parentG)
{
    if (!ml || !parentG)
        return;
    for (Graph *cand : ml->layersList()) {
        if (cand && cand->linkedParentLayer() == parentG)
            cand->syncLinkedAxesFromParent();
    }
}

static Spectrogram *effectiveSelectedSpectrogram(Graph *g, int *outIdx = nullptr)
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

static QVector<QwtErrorPlotCurve *> effectiveSelectedErrorBars(Graph *g)
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

static QString formatColumnLabelWithMeta(Table *t, int c, const QString &roleTag)
{
    if (!t || c < 0 || c >= t->numCols())
        return QString();
    const QString colShort = t->colLabel(c);
    const QString longName = t->colLongName(c).trimmed();
    const QString units = t->colUnits(c).trimmed();
    QString meta;
    if (!longName.isEmpty() && longName != colShort && !units.isEmpty())
        meta = QStringLiteral(" [%1 (%2)]").arg(longName, units);
    else if (!longName.isEmpty() && longName != colShort)
        meta = QStringLiteral(" [%1]").arg(longName);
    else if (!units.isEmpty())
        meta = QStringLiteral(" [(%1)]").arg(units);
    return QStringLiteral("%1%2 %3").arg(colShort, meta, roleTag);
}

static bool isPaletteDark()
{
    const QColor win = QApplication::palette().color(QPalette::Window);
    const double lum = 0.2126 * win.redF() + 0.7152 * win.greenF() + 0.0722 * win.blueF();
    return lum < 0.45;
}

static QString friendlyCurveDisplayName(Graph *g, int curveIdx)
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
    if (cv->yAxis() == QwtPlot::yRight)
        label += QCoreApplication::translate("GraphInspectorDock", " [R-Y]");
    return label;
}

static int resolveMasterCurveIndex(Graph *g, int curveIdx)
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

static int effectiveSelectedCurveIndex(Graph *g, bool fallbackToFirstDataCurve = true)
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

static int findSmartAdjacentYErrColumn(Table *tbl, int colIdx)
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

    bool hasAnyYErr = false;
    for (int c = 0; c < tbl->numCols(); ++c) {
        if (tbl->colPlotDesignation(c) == SciDAVis::yErr) {
            hasAnyYErr = true;
            break;
        }
    }
    if (!hasAnyYErr && colIdx + 1 < tbl->numCols())
        return colIdx + 1;
    return -1;
}

static double physicalValueToMm(double val, MultiLayer::PageUnit unit)
{
    switch (unit) {
    case MultiLayer::UnitPx:
        return (val / 96.0) * 25.4;
    case MultiLayer::UnitMm:
        return val;
    case MultiLayer::UnitCm:
        return val * 10.0;
    case MultiLayer::UnitInch:
        return val * 25.4;
    }
    return val;
}

static bool isLegendEntryLine(const QString &line)
{
    const QString t = line.trimmed();
    return t.startsWith(QStringLiteral("\\c{")) || t.startsWith(QStringLiteral("\\l("))
            || t.startsWith(QStringLiteral("\\p{"));
}

static QLabel *makeSectionHeaderLabel(QWidget *parent)
{
    QLabel *lbl = new QLabel(parent);
    lbl->setObjectName(QStringLiteral("InspectorSectionHeader"));
    return lbl;
}

static void updateColorSwatchButton(QPushButton *btn, const QColor &c)
{
    if (!btn)
        return;
    const QColor safe = c.isValid() ? c : QColor(Qt::black);
    QPixmap px(32, 14);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(120, 130, 145), 1.0));
    p.setBrush(safe);
    p.drawRoundedRect(QRectF(1.0, 1.0, 30.0, 12.0), 3.0, 3.0);
    p.end();
    btn->setIcon(QIcon(px));
    btn->setIconSize(QSize(32, 14));
    btn->setProperty("swatchColor", safe);
}

} // namespace

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
}

GraphInspectorDock::~GraphInspectorDock() = default;

Graph *GraphInspectorDock::activeLayer() const
{
    if (!d_target_ml)
        return nullptr;
    Graph *ag = d_target_ml->activeGraph();
    return ag ? ag : d_target_ml->layer(1);
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
    if (event && event->spontaneous())
        emit closeRequestedByUser();
    QDockWidget::closeEvent(event);
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
    emit userPinToggled(pinned);
}

void GraphInspectorDock::setCollapsedRail(bool collapsed)
{
    const bool changed = (d_collapsed_rail != collapsed);
    d_collapsed_rail = collapsed;

    if (d_mode_stack)
        d_mode_stack->setCurrentIndex(collapsed ? 1 : 0);

    if (collapsed) {
        setMinimumWidth(48);
        setMaximumWidth(48);
        resize(48, height());
        if (d_app) {
            d_app->resizeDocks({ this }, { 48 }, Qt::Horizontal);
        }
    } else {
        setMinimumWidth(252);
        setMaximumWidth(360);
        if (width() < 252 || width() > 360)
            resize(288, height());
        if (d_app) {
            d_app->resizeDocks({ this }, { 288 }, Qt::Horizontal);
        }
    }

    if (changed)
        emit collapsedStateChanged(collapsed);
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
}

GraphInspectorDock::InspectorPageIndex GraphInspectorDock::currentInspectorPage() const
{
    if (!d_prop_stack)
        return PageTabPageLayer;
    return static_cast<InspectorPageIndex>(qBound(0, d_prop_stack->currentIndex(), 3));
}

void GraphInspectorDock::setTargetMultiLayer(MultiLayer *ml, bool forceRefresh)
{
    if (d_target_ml == ml && !forceRefresh) {
        if (d_header_target_lbl && ml) {
            const QString winName = ml->name();
            if (d_header_target_lbl->text() != winName)
                d_header_target_lbl->setText(winName);
        }
        syncInspectorFromSelection();
        return;
    }

    if (d_coalesce_refresh_timer && d_coalesce_refresh_timer->isActive())
        d_coalesce_refresh_timer->stop();

    if (d_target_ml && d_target_ml != ml)
        disconnect(d_target_ml, nullptr, this, nullptr);
    for (const QPointer<Graph> &gPtr : d_bound_layers) {
        if (gPtr)
            disconnect(gPtr.data(), nullptr, this, nullptr);
    }
    d_bound_layers.clear();

    d_target_ml = ml;
    if (ml)
        connectMultiLayerSignals(ml);

    refreshAll();
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
    connect(ml, &QObject::destroyed, this, [this](QObject *) {
        if (d_coalesce_refresh_timer && d_coalesce_refresh_timer->isActive())
            d_coalesce_refresh_timer->stop();
        d_target_ml = nullptr;
        d_bound_layers.clear();
        refreshAll();
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

    d_mode_stack = new QStackedWidget(d_root_container);
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

    // Multi-Panel Scope Switch ([Active Layer | All Panels]), shown when layers() > 1
    d_scope_strip = new QFrame(d_expanded_panel);
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
    expLayout->addWidget(d_scope_strip);

    // Vertical Splitter: Top Object Manager + Bottom Property Inspector
    d_splitter = new QSplitter(Qt::Vertical, d_expanded_panel);
    d_splitter->setChildrenCollapsible(false);
    d_splitter->setHandleWidth(4);

    // --- Top Pane: Object Manager ---
    QWidget *topPane = new QWidget(d_splitter);
    topPane->setMinimumHeight(135);
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

    d_btn_add_curve = makeTreeBtn(QString(), QStringLiteral("plus"));
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
    topLayout->addWidget(d_object_tree, 1);

    d_empty_hint_lbl = new QLabel(topPane);
    d_empty_hint_lbl->setWordWrap(true);
    d_empty_hint_lbl->setAlignment(Qt::AlignCenter);
    d_empty_hint_lbl->hide();
    topLayout->addWidget(d_empty_hint_lbl);

    d_splitter->addWidget(topPane);

    // --- Bottom Pane: Context Tabs + Scrollable Property Stack ---
    QWidget *bottomPane = new QWidget(d_splitter);
    bottomPane->setMinimumHeight(200);
    QVBoxLayout *botLayout = new QVBoxLayout(bottomPane);
    botLayout->setContentsMargins(0, 0, 0, 0);
    botLayout->setSpacing(0);

    d_tab_strip = new QFrame(bottomPane);
    d_tab_strip->setObjectName(QStringLiteral("InspectorTabStrip"));
    QHBoxLayout *tabRow = new QHBoxLayout(d_tab_strip);
    tabRow->setContentsMargins(6, 3, 6, 3);
    tabRow->setSpacing(3);

    d_page_tab_group = new QButtonGroup(this);
    d_page_tab_group->setExclusive(true);

    auto makePageTabBtn = [&](int idx) -> QToolButton * {
        QToolButton *b = new QToolButton(d_tab_strip);
        b->setCheckable(true);
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
                if (idx == PageTabPageLayer)
                    syncPageLayerControls();
                else if (idx == PageTabAxis)
                    syncAxisControls();
                else if (idx == PageTabCurve)
                    syncCurveControls();
                else if (idx == PageTabLegend)
                    syncLegendControls();
            });

    botLayout->addWidget(d_tab_strip);

    d_prop_scroll = new QScrollArea(bottomPane);
    d_prop_scroll->setWidgetResizable(true);
    d_prop_scroll->setFrameShape(QFrame::NoFrame);
    d_prop_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    d_prop_stack = new ActivePageStackedWidget(d_prop_scroll);
    d_prop_stack->addWidget(buildPageLayerInspector());
    d_prop_stack->addWidget(buildAxisInspector());
    d_prop_stack->addWidget(buildCurveInspector());
    d_prop_stack->addWidget(buildLegendInspector());
    d_prop_scroll->setWidget(d_prop_stack);

    botLayout->addWidget(d_prop_scroll, 1);
    d_splitter->addWidget(bottomPane);
    d_splitter->setStretchFactor(0, 38);
    d_splitter->setStretchFactor(1, 62);

    expLayout->addWidget(d_splitter, 1);
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

QWidget *GraphInspectorDock::buildPageLayerInspector()
{
    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 12, 10);
    lay->setSpacing(6);

    // 1) Style Presets & 70% Square Frame
    d_sec_presets_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_presets_lbl);

    d_btn_preset_isij = new QPushButton(page);
    d_btn_preset_isij->setIcon(IconLoader::duotoneIcon(QStringLiteral("preset-isij"), 15));
    connect(d_btn_preset_isij, &QPushButton::clicked, this, [this]() {
        if (!d_target_ml)
            return;
        d_target_ml->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        refreshAll();
    });
    lay->addWidget(d_btn_preset_isij);

    d_btn_preset_slide28 = new QPushButton(page);
    d_btn_preset_slide28->setIcon(IconLoader::duotoneIcon(QStringLiteral("preset-slide"), 15));
    connect(d_btn_preset_slide28, &QPushButton::clicked, this, [this]() {
        if (!d_target_ml)
            return;
        d_target_ml->applyStylePreset(MultiLayer::StylePresentationSlide28pt);
        refreshAll();
    });
    lay->addWidget(d_btn_preset_slide28);

    d_btn_square_frame70 = new QPushButton(page);
    d_btn_square_frame70->setIcon(IconLoader::duotoneIcon(QStringLiteral("aspect-ratio"), 15));
    connect(d_btn_square_frame70, &QPushButton::clicked, this, [this]() {
        if (!d_target_ml)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->applySquareCanvasFrame70Percent();
        d_target_ml->pushUndoSnapshot(tr("Square Frame (70%)"), before);
        syncPageLayerControls();
    });
    lay->addWidget(d_btn_square_frame70);

    // 2) Physical Page Size & Zoom
    d_sec_page_size_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_page_size_lbl);

    QGridLayout *sizeGrid = new QGridLayout();
    sizeGrid->setContentsMargins(0, 0, 0, 0);
    sizeGrid->setHorizontalSpacing(6);
    sizeGrid->setVerticalSpacing(4);

    d_lbl_page_w = new QLabel(page);
    d_spin_page_w_mm = new QDoubleSpinBox(page);
    d_spin_page_w_mm->setRange(20.0, 1000.0);
    d_spin_page_w_mm->setDecimals(1);
    d_spin_page_w_mm->setSingleStep(5.0);
    d_spin_page_w_mm->setSuffix(QStringLiteral(" mm"));

    d_lbl_page_h = new QLabel(page);
    d_spin_page_h_mm = new QDoubleSpinBox(page);
    d_spin_page_h_mm->setRange(20.0, 1000.0);
    d_spin_page_h_mm->setDecimals(1);
    d_spin_page_h_mm->setSingleStep(5.0);
    d_spin_page_h_mm->setSuffix(QStringLiteral(" mm"));

    d_lbl_page_dpi = new QLabel(page);
    d_spin_page_dpi = new QSpinBox(page);
    d_spin_page_dpi->setRange(72, 1200);
    d_spin_page_dpi->setSingleStep(50);
    d_spin_page_dpi->setValue(300);

    sizeGrid->addWidget(d_lbl_page_w, 0, 0);
    sizeGrid->addWidget(d_spin_page_w_mm, 0, 1);
    sizeGrid->addWidget(d_lbl_page_h, 1, 0);
    sizeGrid->addWidget(d_spin_page_h_mm, 1, 1);
    sizeGrid->addWidget(d_lbl_page_dpi, 2, 0);
    sizeGrid->addWidget(d_spin_page_dpi, 2, 1);

    auto applyPageSize = [this]() {
        if (d_syncing_props || !d_target_ml)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->setPageSizePhysical(d_spin_page_w_mm->value(), d_spin_page_h_mm->value(),
                                         MultiLayer::UnitMm, d_spin_page_dpi->value());
        d_target_ml->pushUndoSnapshot(tr("Resize Page"), before, kMergePageDims);
    };
    connect(d_spin_page_w_mm, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [applyPageSize](double) { applyPageSize(); });
    connect(d_spin_page_h_mm, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [applyPageSize](double) { applyPageSize(); });
    connect(d_spin_page_dpi, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [applyPageSize](int) { applyPageSize(); });

    d_chk_page_view = new QCheckBox(page);
    connect(d_chk_page_view, &QCheckBox::toggled, this, [this](bool checked) {
        if (d_syncing_props || !d_target_ml)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->setPageViewMode(checked);
        d_target_ml->pushUndoSnapshot(tr("Toggle Page View"), before);
        if (d_app)
            d_app->syncGraphPageAndToolbarMenuState();
    });
    sizeGrid->addWidget(d_chk_page_view, 3, 0, 1, 2);

    d_lbl_page_zoom = new QLabel(page);
    d_combo_page_zoom = new QComboBox(page);
    const QList<int> zoomPcts = { 50, 75, 100, 125, 150, 200 };
    for (int pct : zoomPcts)
        d_combo_page_zoom->addItem(QStringLiteral("%1%").arg(pct), pct);
    d_combo_page_zoom->setCurrentIndex(2); // 100%
    connect(d_combo_page_zoom, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                if (d_syncing_props || !d_target_ml || idx < 0)
                    return;
                const int pct = d_combo_page_zoom->itemData(idx).toInt();
                if (pct > 0) {
                    const QString before = d_target_ml->captureUndoSnapshot();
                    d_target_ml->setPageZoom(double(pct) / 100.0);
                    d_target_ml->pushUndoSnapshot(tr("Page Zoom"), before, kMergePageDims);
                }
            });
    sizeGrid->addWidget(d_lbl_page_zoom, 4, 0);
    sizeGrid->addWidget(d_combo_page_zoom, 4, 1);
    lay->addLayout(sizeGrid);

    // 3) Frame, Linked Axes & Panel Labels
    d_sec_layer_frame_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_layer_frame_lbl);

    d_btn_box_inward = new QPushButton(page);
    d_btn_box_inward->setIcon(IconLoader::duotoneIcon(QStringLiteral("frame-box"), 15));
    connect(d_btn_box_inward, &QPushButton::clicked, this, [this]() {
        const QVector<Graph *> targets = targetLayersForScope();
        if (!d_target_ml || targets.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (Graph *lg : targets) {
            if (lg)
                lg->applyFramePreset(Graph::FrameBoxInwardTicks);
        }
        d_target_ml->pushUndoSnapshot(tr("Box Frame + Inward Ticks"), before);
        syncAxisControls();
    });
    lay->addWidget(d_btn_box_inward);

    d_btn_link_right_y = new QPushButton(page);
    d_btn_link_right_y->setIcon(IconLoader::duotoneIcon(QStringLiteral("plot-double-y"), 15));
    connect(d_btn_link_right_y, &QPushButton::clicked, this, [this]() {
        Graph *parentLayer = activeLayer();
        if (!d_target_ml || !parentLayer)
            return;
        if (parentLayer->linkedParentLayer())
            parentLayer = parentLayer->linkedParentLayer();

        Graph *existingChild = nullptr;
        for (Graph *cand : d_target_ml->layersList()) {
            if (!cand || cand->linkedParentLayer() != parentLayer || !cand->plotWidget())
                continue;
            Plot *cp = cand->plotWidget();
            const QwtScaleDraw *sdTop = cp->axisScaleDraw(QwtPlot::xTop);
            const QwtScaleDraw *sdRight = cp->axisScaleDraw(QwtPlot::yRight);
            const bool topHasLabels = sdTop && sdTop->hasComponent(QwtAbstractScaleDraw::Labels);
            const bool rightHasLabels =
                    sdRight && sdRight->hasComponent(QwtAbstractScaleDraw::Labels);
            if (cp->axisEnabled(QwtPlot::yRight)
                && (!cp->axisEnabled(QwtPlot::xTop) || (rightHasLabels && !topHasLabels))) {
                existingChild = cand;
                break;
            }
        }

        const QString before = d_target_ml->captureUndoSnapshot();
        Graph *child = existingChild;
        if (child) {
            child->setLinkedParentLayer(parentLayer, 1, QStringLiteral("x"), 0,
                                        QStringLiteral("y"));
            d_target_ml->setActiveGraph(child);
        } else {
            child = d_target_ml->addLinkedLayer(parentLayer, true, false, QStringLiteral("x"),
                                                QStringLiteral("y"));
        }
        if (child && child->plotWidget()) {
            Plot *cp = child->plotWidget();
            if (QwtScaleDraw *sdTop = cp->axisScaleDraw(QwtPlot::xTop))
                sdTop->enableComponent(QwtAbstractScaleDraw::Labels, false);
            if (QwtScaleDraw *sdRight = cp->axisScaleDraw(QwtPlot::yRight))
                sdRight->enableComponent(QwtAbstractScaleDraw::Labels, true);
            cp->setMajorTicksType(QwtPlot::xTop, ScaleDraw::None);
            cp->setMinorTicksType(QwtPlot::xTop, ScaleDraw::None);
            cp->setMajorTicksType(QwtPlot::yRight, ScaleDraw::In);
            cp->setMinorTicksType(QwtPlot::yRight, ScaleDraw::In);
            child->syncLinkedAxesFromParent();
            d_target_ml->applySquareCanvasFrame70Percent();
        }
        d_target_ml->pushUndoSnapshot(tr("Link Right-Y Axis"), before);
        refreshAll();
    });
    lay->addWidget(d_btn_link_right_y);

    d_btn_link_top_x = new QToolButton(page);
    d_btn_link_top_x->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    d_btn_link_top_x->setPopupMode(QToolButton::InstantPopup);
    d_btn_link_top_x->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_link_top_x->setIcon(IconLoader::duotoneIcon(QStringLiteral("axis-scale"), 15));
    d_menu_link_top_x = new QMenu(d_btn_link_top_x);
    d_btn_link_top_x->setMenu(d_menu_link_top_x);

    auto addTopXFormulaLink = [this](const QString &formula, const QString &defaultTitle) {
        Graph *parentLayer = activeLayer();
        const QString trimmedFormula = formula.trimmed();
        if (!d_target_ml || !parentLayer || trimmedFormula.isEmpty())
            return;
        if (parentLayer->linkedParentLayer())
            parentLayer = parentLayer->linkedParentLayer();
        const QString before = d_target_ml->captureUndoSnapshot();

        QString normExpr = trimmedFormula.toLower();
        normExpr.remove(QLatin1Char(' '));
        const bool singularAtZero = normExpr.contains(QStringLiteral("/x"))
                || normExpr.contains(QStringLiteral("/(x)"));
        const bool singularAtAbsZeroC = normExpr.contains(QStringLiteral("/(x+273.15)"));

        if (Plot *pp = parentLayer->plotWidget()) {
            if (singularAtZero || singularAtAbsZeroC) {
                const double pole = singularAtZero ? 0.0 : -273.15;
                if (const QwtScaleDiv *xDiv = pp->axisScaleDiv(QwtPlot::xBottom)) {
                    const double rawLo = xDiv->lowerBound();
                    const double rawHi = xDiv->upperBound();
                    const double lo = qMin(rawLo, rawHi);
                    const double hi = qMax(rawLo, rawHi);
                    if (lo <= pole && hi > pole) {
                        const double spanAbovePole = hi - pole;
                        double clampedLo = pole
                                + ((spanAbovePole >= 100.0) ? qMax(1.0, spanAbovePole * 0.05)
                                                            : (0.1 * spanAbovePole));
                        double minDataAbovePole = std::numeric_limits<double>::infinity();
                        for (int ci = 0; ci < parentLayer->curves(); ++ci) {
                            if (parentLayer->curveType(ci) == Graph::ErrorBars)
                                continue;
                            QwtPlotCurve *cv = parentLayer->curve(ci);
                            if (!cv)
                                continue;
                            for (int p = 0; p < cv->dataSize(); ++p) {
                                const double xv = cv->x(p);
                                if (std::isfinite(xv) && xv > pole && xv < minDataAbovePole)
                                    minDataAbovePole = xv;
                            }
                        }
                        if (std::isfinite(minDataAbovePole) && minDataAbovePole < clampedLo) {
                            clampedLo = pole + 0.9 * (minDataAbovePole - pole);
                        }
                        int scaleType = 0;
                        bool inv = (rawLo > rawHi);
                        if (const QwtScaleEngine *eng = pp->axisScaleEngine(QwtPlot::xBottom)) {
                            inv = eng->testAttribute(QwtScaleEngine::Inverted);
                            if (QwtScaleTransformation *xf = eng->transformation()) {
                                if (xf->type() == QwtScaleTransformation::Log10)
                                    scaleType = 1;
                                delete xf;
                            }
                        }
                        parentLayer->enableAutoscaling(false);
                        parentLayer->setScale(QwtPlot::xBottom, clampedLo, hi,
                                              parentLayer->axisStep(QwtPlot::xBottom),
                                              pp->axisMaxMajor(QwtPlot::xBottom),
                                              pp->axisMaxMinor(QwtPlot::xBottom), scaleType, inv);
                    }
                }
            }
            if (QwtScaleDraw *psdTop = pp->axisScaleDraw(QwtPlot::xTop))
                psdTop->enableComponent(QwtAbstractScaleDraw::Labels, false);
            pp->setMajorTicksType(QwtPlot::xTop, ScaleDraw::None);
            pp->setMinorTicksType(QwtPlot::xTop, ScaleDraw::None);
        }

        Graph *existingChild = nullptr;
        for (Graph *cand : d_target_ml->layersList()) {
            if (!cand || cand->linkedParentLayer() != parentLayer || !cand->plotWidget())
                continue;
            Plot *cp = cand->plotWidget();
            const QwtScaleDraw *sdTop = cp->axisScaleDraw(QwtPlot::xTop);
            const QwtScaleDraw *sdRight = cp->axisScaleDraw(QwtPlot::yRight);
            const bool topHasLabels = sdTop && sdTop->hasComponent(QwtAbstractScaleDraw::Labels);
            const bool rightHasLabels =
                    sdRight && sdRight->hasComponent(QwtAbstractScaleDraw::Labels);
            if (cp->axisEnabled(QwtPlot::xTop)
                && (!cp->axisEnabled(QwtPlot::yRight) || topHasLabels || !rightHasLabels
                    || cand->xLinkMode() == 2)) {
                existingChild = cand;
                break;
            }
        }

        Graph *child = existingChild;
        const int xMode =
                (trimmedFormula.compare(QStringLiteral("x"), Qt::CaseInsensitive) == 0
                 || trimmedFormula.compare(QStringLiteral("x1"), Qt::CaseInsensitive) == 0)
                ? 1
                : 2;
        if (child) {
            const int yMode = (child->yLinkMode() > 0) ? child->yLinkMode() : 1;
            const QString yExpr = child->yLinkExpr().trimmed().isEmpty()
                    ? QStringLiteral("y")
                    : child->yLinkExpr();
            child->setLinkedParentLayer(parentLayer, xMode, trimmedFormula, yMode, yExpr);
            d_target_ml->setActiveGraph(child);
        } else {
            child = d_target_ml->addLinkedLayer(parentLayer, true, true, trimmedFormula,
                                                QStringLiteral("y"));
        }
        if (child && child->plotWidget()) {
            Plot *cp = child->plotWidget();
            if (QwtScaleDraw *sdTop = cp->axisScaleDraw(QwtPlot::xTop))
                sdTop->enableComponent(QwtAbstractScaleDraw::Labels, true);
            if (QwtScaleDraw *sdRight = cp->axisScaleDraw(QwtPlot::yRight))
                sdRight->enableComponent(QwtAbstractScaleDraw::Labels, false);
            cp->setMajorTicksType(QwtPlot::xTop, ScaleDraw::In);
            cp->setMinorTicksType(QwtPlot::xTop, ScaleDraw::In);
            cp->setMajorTicksType(QwtPlot::yRight, ScaleDraw::None);
            cp->setMinorTicksType(QwtPlot::yRight, ScaleDraw::None);
            if (!defaultTitle.isEmpty()) {
                QFont tf = parentLayer->axisTitleFont(QwtPlot::xBottom);
                if (tf.family().trimmed().isEmpty())
                    tf.setFamily(QStringLiteral("Arial"));
                tf.setBold(false);
                child->setAxisTitleFont(QwtPlot::xTop, tf);
                child->setAxisTitleAuto(QwtPlot::xTop, false);
                child->setTopAxisTitle(defaultTitle);
            } else if (xMode == 1) {
                child->setTopAxisTitle(QString());
            }
            child->syncLinkedAxesFromParent();
            d_target_ml->applySquareCanvasFrame70Percent();
        }
        d_target_ml->pushUndoSnapshot(tr("Link Top-X Axis"), before);
        refreshAll();
    };

    connect(d_menu_link_top_x, &QMenu::aboutToShow, this, [this, addTopXFormulaLink]() {
        d_menu_link_top_x->clear();
        d_menu_link_top_x->addAction(tr("1:1 Linear Link (x)"), this, [addTopXFormulaLink]() {
            addTopXFormulaLink(QStringLiteral("x"), QString());
        });
        d_menu_link_top_x->addAction(
                tr("Arrhenius: K -> 1000/T"), this, [addTopXFormulaLink]() {
                    addTopXFormulaLink(QStringLiteral("1000.0/x"),
                                       QStringLiteral("1000 / <i>T</i> (K<sup>-1</sup>)"));
                });
        d_menu_link_top_x->addAction(
                tr("Arrhenius: °C -> 1000/T"), this, [addTopXFormulaLink]() {
                    addTopXFormulaLink(QStringLiteral("1000.0/(x+273.15)"),
                                       QStringLiteral("1000 / <i>T</i> (K<sup>-1</sup>)"));
                });
        d_menu_link_top_x->addAction(
                tr("Inverse Arrhenius: 1000/T -> °C"), this, [this, addTopXFormulaLink]() {
                    addTopXFormulaLink(QStringLiteral("(1000.0/x)-273.15"),
                                       tr("Temperature (°C)"));
                });
        d_menu_link_top_x->addSeparator();
        d_menu_link_top_x->addAction(tr("Custom Formula..."), this, [this, addTopXFormulaLink]() {
            bool ok = false;
            const QString expr = QInputDialog::getText(
                    this, tr("Link Top-X Formula"),
                    tr("Formula in terms of bottom X (variable 'x'):"), QLineEdit::Normal,
                    QStringLiteral("1000.0/x"), &ok);
            if (ok && !expr.trimmed().isEmpty())
                addTopXFormulaLink(expr.trimmed(), QString());
        });
    });
    lay->addWidget(d_btn_link_top_x);

    QHBoxLayout *panelRow = new QHBoxLayout();
    panelRow->setSpacing(4);
    d_chk_panel_labels = new QCheckBox(page);
    d_combo_panel_pos = new QComboBox(page);
    d_combo_panel_pos->addItem(QString(), int(MultiLayer::PanelOutsideTopLeft));
    d_combo_panel_pos->addItem(QString(), int(MultiLayer::PanelInsideTopLeft));
    panelRow->addWidget(d_chk_panel_labels, 1);
    panelRow->addWidget(d_combo_panel_pos);
    lay->addLayout(panelRow);

    auto applyPanelLabelsFromUi = [this]() {
        if (d_syncing_props || !d_target_ml)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        const auto pos = static_cast<MultiLayer::PanelLabelPlacement>(
                d_combo_panel_pos->currentData().toInt());
        d_target_ml->setPanelLabels(d_chk_panel_labels->isChecked(), pos,
                                    d_target_ml->panelLabelPattern(),
                                    d_target_ml->panelLabelFontPt());
        d_target_ml->pushUndoSnapshot(tr("Panel Labels (a), (b)"), before);
    };
    connect(d_chk_panel_labels, &QCheckBox::toggled, this,
            [applyPanelLabelsFromUi](bool) { applyPanelLabelsFromUi(); });
    connect(d_combo_panel_pos, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [applyPanelLabelsFromUi](int) { applyPanelLabelsFromUi(); });

    // 4) Export & OLE Copy
    d_sec_export_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_export_lbl);

    QHBoxLayout *expBtnRow = new QHBoxLayout();
    expBtnRow->setSpacing(6);
    d_btn_copy_ole = new QPushButton(page);
    d_btn_copy_ole->setIcon(IconLoader::duotoneIcon(QStringLiteral("copy-ole"), 15));
    connect(d_btn_copy_ole, &QPushButton::clicked, this, [this]() {
        if (d_app)
            d_app->copyGraphAsOleObject();
    });
    expBtnRow->addWidget(d_btn_copy_ole);

    d_btn_export_img_pdf = new QPushButton(page);
    d_btn_export_img_pdf->setIcon(IconLoader::duotoneIcon(QStringLiteral("export"), 15));
    connect(d_btn_export_img_pdf, &QPushButton::clicked, this, [this]() {
        if (d_app)
            d_app->exportGraph();
    });
    expBtnRow->addWidget(d_btn_export_img_pdf);
    lay->addLayout(expBtnRow);

    lay->addStretch(1);
    return page;
}

QWidget *GraphInspectorDock::buildAxisInspector()
{
    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 12, 10);
    lay->setSpacing(6);

    // Axis Selector [Bottom X] [Left Y] [Top X] [Right Y]
    QHBoxLayout *axisSelRow = new QHBoxLayout();
    axisSelRow->setSpacing(3);
    d_axis_sel_group = new QButtonGroup(this);
    d_axis_sel_group->setExclusive(true);

    auto makeAxisBtn = [&](int qwtAxis) -> QToolButton * {
        QToolButton *b = new QToolButton(page);
        b->setCheckable(true);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setFixedHeight(24);
        b->setObjectName(QStringLiteral("InspectorAxisPillBtn"));
        d_axis_sel_group->addButton(b, qwtAxis);
        axisSelRow->addWidget(b);
        return b;
    };

    d_btn_axis_bottom = makeAxisBtn(QwtPlot::xBottom);
    d_btn_axis_left = makeAxisBtn(QwtPlot::yLeft);
    d_btn_axis_top = makeAxisBtn(QwtPlot::xTop);
    d_btn_axis_right = makeAxisBtn(QwtPlot::yRight);
    d_btn_axis_bottom->setChecked(true);

    connect(d_axis_sel_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this,
            [this](int qwtAxis) {
                d_active_axis = qBound(0, qwtAxis, 3);
                if (Graph *g = activeLayer()) {
                    GraphMiniToolbar::instance()->cancelScheduledAndHide();
                    Graph::GraphSelection sel;
                    sel.kind = Graph::GraphSelection::Axis;
                    sel.primaryId = d_active_axis;
                    g->setCurrentSelection(sel, false);
                }
                syncAxisControls();
            });
    lay->addLayout(axisSelRow);

    d_chk_axis_visible = new QCheckBox(page);
    connect(d_chk_axis_visible, &QCheckBox::toggled, this, [this](bool vis) {
        const QVector<Graph *> targets = targetLayersForScope();
        if (d_syncing_props || !d_target_ml || targets.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (Graph *lg : targets) {
            if (!lg)
                continue;
            lg->enableAxis(d_active_axis, vis);
            lg->setAxisBackboneVisible(d_active_axis, vis);
            lg->replot();
        }
        d_target_ml->pushUndoSnapshot(tr("Toggle Axis Visibility"), before);
        syncAxisControls();
    });
    lay->addWidget(d_chk_axis_visible);

    d_chk_tick_labels = new QCheckBox(page);
    connect(d_chk_tick_labels, &QCheckBox::toggled, this, [this](bool on) {
        const QVector<Graph *> targets = targetLayersForScope();
        if (d_syncing_props || !d_target_ml || targets.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (Graph *lg : targets) {
            if (!lg)
                continue;
            lg->setTickLabelsOnAxis(d_active_axis, on);
            lg->replot();
            emit lg->modifiedGraph();
        }
        d_target_ml->pushUndoSnapshot(tr("Toggle Tick Numbers"), before);
        syncAxisControls();
    });
    lay->addWidget(d_chk_tick_labels);

    // Range & Scale
    d_sec_axis_range_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_axis_range_lbl);

    QGridLayout *rngGrid = new QGridLayout();
    rngGrid->setContentsMargins(0, 0, 0, 0);
    rngGrid->setHorizontalSpacing(6);
    rngGrid->setVerticalSpacing(4);

    d_lbl_axis_min = new QLabel(page);
    d_edit_axis_min = new QLineEdit(page);
    d_lbl_axis_max = new QLabel(page);
    d_edit_axis_max = new QLineEdit(page);
    d_lbl_axis_step = new QLabel(page);
    d_edit_axis_step = new QLineEdit(page);

    rngGrid->addWidget(d_lbl_axis_min, 0, 0);
    rngGrid->addWidget(d_edit_axis_min, 0, 1);
    rngGrid->addWidget(d_lbl_axis_max, 1, 0);
    rngGrid->addWidget(d_edit_axis_max, 1, 1);
    rngGrid->addWidget(d_lbl_axis_step, 2, 0);
    rngGrid->addWidget(d_edit_axis_step, 2, 1);

    connect(d_edit_axis_min, &QLineEdit::editingFinished, this,
            [this]() { applyAxisScaleFromUi(kMergeAxisScale); });
    connect(d_edit_axis_max, &QLineEdit::editingFinished, this,
            [this]() { applyAxisScaleFromUi(kMergeAxisScale); });
    connect(d_edit_axis_step, &QLineEdit::editingFinished, this,
            [this]() { applyAxisScaleFromUi(kMergeAxisScale); });

    d_btn_axis_autoscale = new QPushButton(page);
    d_btn_axis_autoscale->setIcon(IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 14));
    connect(d_btn_axis_autoscale, &QPushButton::clicked, this, [this]() {
        const QVector<Graph *> targets = targetLayersForScope();
        if (!d_target_ml || targets.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (Graph *lg : targets) {
            if (!lg)
                continue;
            lg->setAxisUserLocked(d_active_axis, false);
            lg->setAutoScaleAxis(d_active_axis);
            syncLinkedChildrenOf(d_target_ml, lg);
        }
        d_target_ml->pushUndoSnapshot(tr("Auto Range Axis"), before);
        syncAxisControls();
    });
    rngGrid->addWidget(d_btn_axis_autoscale, 3, 0, 1, 2);

    d_lbl_axis_scale_type = new QLabel(page);
    QHBoxLayout *scaleTypeRow = new QHBoxLayout();
    scaleTypeRow->setContentsMargins(0, 0, 0, 0);
    scaleTypeRow->setSpacing(4);
    d_combo_axis_scale = new QComboBox(page);
    d_combo_axis_scale->addItem(QString(), 0); // Linear
    d_combo_axis_scale->addItem(QString(), 1); // Log10
    connect(d_combo_axis_scale, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (!d_syncing_props)
                    applyAxisScaleFromUi(-1);
            });
    d_chk_axis_reverse = new QCheckBox(page);
    connect(d_chk_axis_reverse, &QCheckBox::toggled, this, [this](bool) {
        if (!d_syncing_props)
            applyAxisScaleFromUi(-1);
    });
    scaleTypeRow->addWidget(d_combo_axis_scale, 1);
    scaleTypeRow->addWidget(d_chk_axis_reverse);
    rngGrid->addWidget(d_lbl_axis_scale_type, 4, 0);
    rngGrid->addLayout(scaleTypeRow, 4, 1);

    d_lbl_minor_subdiv = new QLabel(page);
    d_combo_minor_subdiv = new QComboBox(page);
    d_combo_minor_subdiv->addItem(QString(), 0);
    d_combo_minor_subdiv->addItem(QString(), 1);
    d_combo_minor_subdiv->addItem(QString(), 4);
    d_combo_minor_subdiv->addItem(QString(), 9);
    connect(d_combo_minor_subdiv, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (!d_syncing_props)
                    applyAxisScaleFromUi(-1);
            });
    rngGrid->addWidget(d_lbl_minor_subdiv, 5, 0);
    rngGrid->addWidget(d_combo_minor_subdiv, 5, 1);
    lay->addLayout(rngGrid);

    // Axis Break
    d_chk_axis_break = new QCheckBox(page);
    lay->addWidget(d_chk_axis_break);

    QGridLayout *brkGrid = new QGridLayout();
    brkGrid->setContentsMargins(0, 0, 0, 0);
    brkGrid->setHorizontalSpacing(6);
    brkGrid->setVerticalSpacing(4);
    d_lbl_break_from = new QLabel(page);
    d_edit_break_from = new QLineEdit(page);
    d_lbl_break_to = new QLabel(page);
    d_edit_break_to = new QLineEdit(page);
    brkGrid->addWidget(d_lbl_break_from, 0, 0);
    brkGrid->addWidget(d_edit_break_from, 0, 1);
    brkGrid->addWidget(d_lbl_break_to, 1, 0);
    brkGrid->addWidget(d_edit_break_to, 1, 1);
    lay->addLayout(brkGrid);

    connect(d_chk_axis_break, &QCheckBox::toggled, this, [this](bool on) {
        d_edit_break_from->setEnabled(on);
        d_edit_break_to->setEnabled(on);
        if (!d_syncing_props)
            applyAxisBreakFromUi();
    });
    connect(d_edit_break_from, &QLineEdit::editingFinished, this,
            &GraphInspectorDock::applyAxisBreakFromUi);
    connect(d_edit_break_to, &QLineEdit::editingFinished, this,
            &GraphInspectorDock::applyAxisBreakFromUi);

    // Ticks, Notation & Grids
    d_sec_axis_ticks_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_axis_ticks_lbl);

    QGridLayout *tickGrid = new QGridLayout();
    tickGrid->setContentsMargins(0, 0, 0, 0);
    tickGrid->setHorizontalSpacing(6);
    tickGrid->setVerticalSpacing(4);

    d_lbl_tick_dir = new QLabel(page);
    d_combo_tick_dir = new QComboBox(page);
    d_combo_tick_dir->addItem(QString(), int(ScaleDraw::In));
    d_combo_tick_dir->addItem(QString(), int(ScaleDraw::Out));
    d_combo_tick_dir->addItem(QString(), int(ScaleDraw::Both));
    d_combo_tick_dir->addItem(QString(), int(ScaleDraw::None));
    connect(d_combo_tick_dir, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                const QVector<Graph *> targets = targetLayersForScope();
                if (d_syncing_props || !d_target_ml || targets.isEmpty() || idx < 0)
                    return;
                const int style = d_combo_tick_dir->itemData(idx).toInt();
                const QString before = d_target_ml->captureUndoSnapshot();
                for (Graph *lg : targets) {
                    if (!lg)
                        continue;
                    lg->setAxisTicksLength(d_active_axis, style, style, lg->minorTickLength(),
                                           lg->majorTickLength());
                    lg->replot();
                }
                d_target_ml->pushUndoSnapshot(tr("Change Tick Direction"), before);
            });
    tickGrid->addWidget(d_lbl_tick_dir, 0, 0);
    tickGrid->addWidget(d_combo_tick_dir, 0, 1);

    d_lbl_tick_format = new QLabel(page);
    d_combo_tick_format = new QComboBox(page);
    d_combo_tick_format->addItem(QString(), int(Plot::Automatic));    // 0: Automatic
    d_combo_tick_format->addItem(QString(), int(Plot::Decimal));      // 1: Decimal (0.00)
    d_combo_tick_format->addItem(QString(), int(Plot::Superscripts)); // 3: Scientific (10^x)
    d_combo_tick_format->addItem(QString(), int(Plot::Scientific));   // 2: Scientific (1E3)
    d_combo_tick_format->addItem(QString(), 4);                       // 4: Engineering
    connect(d_combo_tick_format, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (!d_syncing_props)
                    applyTickFormatFromUi(-1);
            });
    tickGrid->addWidget(d_lbl_tick_format, 1, 0);
    tickGrid->addWidget(d_combo_tick_format, 1, 1);

    d_lbl_tick_prec = new QLabel(page);
    d_spin_tick_prec = new QSpinBox(page);
    d_spin_tick_prec->setRange(-1, 9);
    d_spin_tick_prec->setValue(-1);
    connect(d_spin_tick_prec, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!d_syncing_props)
            applyTickFormatFromUi(kMergeTickPrec);
    });
    tickGrid->addWidget(d_lbl_tick_prec, 2, 0);
    tickGrid->addWidget(d_spin_tick_prec, 2, 1);

    d_lbl_tick_divide = new QLabel(page);
    d_combo_tick_divide = new QComboBox(page);
    d_combo_tick_divide->setEditable(true);
    d_combo_tick_divide->addItems(QStringList()
                                  << QStringLiteral("1") << QStringLiteral("1000")
                                  << QStringLiteral("1e6") << QStringLiteral("0.01"));
    connect(d_combo_tick_divide, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        if (!d_syncing_props)
            applyTickDivideFromUi();
    });
    if (QLineEdit *divEdit = d_combo_tick_divide->lineEdit()) {
        connect(divEdit, &QLineEdit::editingFinished, this, [this]() {
            if (!d_syncing_props)
                applyTickDivideFromUi();
        });
    }
    tickGrid->addWidget(d_lbl_tick_divide, 3, 0);
    tickGrid->addWidget(d_combo_tick_divide, 3, 1);

    d_lbl_category_col = new QLabel(page);
    d_combo_category_col = new QComboBox(page);
    connect(d_combo_category_col, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        if (!d_syncing_props)
            applyCategoryColumnFromUi();
    });
    tickGrid->addWidget(d_lbl_category_col, 4, 0);
    tickGrid->addWidget(d_combo_category_col, 4, 1);

    d_chk_major_grid = new QCheckBox(page);
    d_chk_minor_grid = new QCheckBox(page);
    auto applyGridToggle = [this]() {
        const QVector<Graph *> targets = targetLayersForScope();
        if (d_syncing_props || !d_target_ml || targets.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        const bool isX = (d_active_axis == QwtPlot::xBottom || d_active_axis == QwtPlot::xTop);
        for (Graph *lg : targets) {
            if (!lg || !lg->grid())
                continue;
            Grid *gr = lg->grid();
            if (isX) {
                gr->enableX(d_chk_major_grid->isChecked());
                gr->enableXMin(d_chk_minor_grid->isChecked());
            } else {
                gr->enableY(d_chk_major_grid->isChecked());
                gr->enableYMin(d_chk_minor_grid->isChecked());
            }
            lg->replot();
            emit lg->modifiedGraph();
        }
        d_target_ml->pushUndoSnapshot(tr("Toggle Axis Grid"), before);
    };
    connect(d_chk_major_grid, &QCheckBox::toggled, this,
            [applyGridToggle](bool) { applyGridToggle(); });
    connect(d_chk_minor_grid, &QCheckBox::toggled, this,
            [applyGridToggle](bool) { applyGridToggle(); });

    tickGrid->addWidget(d_chk_major_grid, 5, 0);
    tickGrid->addWidget(d_chk_minor_grid, 5, 1);
    lay->addLayout(tickGrid);

    // Axis Title & Scientific Rich-Text Toolbar + Arial Font Sizes
    d_sec_axis_title_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_axis_title_lbl);

    {
        QHBoxLayout *ufRow = new QHBoxLayout();
        ufRow->setContentsMargins(0, 0, 0, 0);
        ufRow->setSpacing(6);
        d_lbl_unit_format = new QLabel(page);
        d_combo_unit_format = new QComboBox(page);
        d_combo_unit_format->addItem(QString(), 0); // Name (Unit)
        d_combo_unit_format->addItem(QString(), 1); // Name / Unit (ISIJ)
        d_combo_unit_format->addItem(QString(), 2); // Name [Unit]
        connect(d_combo_unit_format, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int idx) {
                    const QVector<Graph *> targets = targetLayersForScope();
                    if (d_syncing_props || !d_target_ml || targets.isEmpty() || idx < 0)
                        return;
                    const int mode = d_combo_unit_format->itemData(idx).toInt();
                    const QString before = d_target_ml->captureUndoSnapshot();
                    static const QRegularExpression rxParen(
                            QStringLiteral("^(.*\\S)\\s+\\(([^()\\[\\]]+)\\)\\s*$"));
                    static const QRegularExpression rxSlash(
                            QStringLiteral("^(.*\\S)\\s+/\\s+([^/()\\[\\]]+)\\s*$"));
                    static const QRegularExpression rxBracket(
                            QStringLiteral("^(.*\\S)\\s+\\[([^\\[\\]()]+)\\]\\s*$"));
                    auto rewriteManualTitle = [&](const QString &raw, int dstMode) -> QString {
                        QString base, unit;
                        QRegularExpressionMatch m = rxParen.match(raw);
                        if (m.hasMatch()) {
                            base = m.captured(1);
                            unit = m.captured(2);
                        } else if ((m = rxBracket.match(raw)).hasMatch()) {
                            base = m.captured(1);
                            unit = m.captured(2);
                        } else if ((m = rxSlash.match(raw)).hasMatch()) {
                            base = m.captured(1);
                            unit = m.captured(2);
                        } else {
                            return raw;
                        }
                        const QString uTrim = unit.trimmed();
                        if (dstMode == 1) {
                            if (uTrim == QLatin1String("%") || uTrim == QLatin1String("-")
                                || uTrim == QString::fromUtf8("\xE2\x80\x94")
                                || uTrim.compare(QLatin1String("a.u."), Qt::CaseInsensitive) == 0
                                || uTrim.compare(QLatin1String("arb. units"), Qt::CaseInsensitive)
                                        == 0) {
                                return base + QStringLiteral(" (") + uTrim + QStringLiteral(")");
                            }
                            return base + QStringLiteral(" / ") + uTrim;
                        }
                        if (dstMode == 2)
                            return base + QStringLiteral(" [") + uTrim + QStringLiteral("]");
                        return base + QStringLiteral(" (") + uTrim + QStringLiteral(")");
                    };
                    for (Graph *lg : targets) {
                        if (!lg)
                            continue;
                        if (mode == 2) {
                            lg->setProperty("_toriBracketUnitFmt", true);
                            lg->setAxisUnitFormat(Column::UnitFormat::Paren);
                            lg->refreshAutoAxisTitlesFromColumns();
                            for (int a = 0; a < 4; ++a) {
                                const QString cur = lg->axisTitle(a);
                                const QString rew = rewriteManualTitle(cur, 2);
                                if (rew != cur) {
                                    const bool wasAuto = lg->isAxisTitleAuto(a);
                                    lg->setAxisTitle(a, rew);
                                    if (wasAuto)
                                        lg->setProperty(QStringLiteral("_toriBracketAuto_%1").arg(a).toLatin1().constData(), true);
                                }
                            }
                        } else {
                            const bool wasBracket = lg->property("_toriBracketUnitFmt").toBool();
                            lg->setProperty("_toriBracketUnitFmt", false);
                            for (int a = 0; a < 4; ++a) {
                                const QByteArray key = QStringLiteral("_toriBracketAuto_%1").arg(a).toLatin1();
                                if (wasBracket && lg->property(key.constData()).toBool()) {
                                    lg->setProperty(key.constData(), false);
                                    lg->setAxisTitleAuto(a, true);
                                } else if (!lg->isAxisTitleAuto(a)) {
                                    const QString cur = lg->axisTitle(a);
                                    const QString rew = rewriteManualTitle(cur, mode);
                                    if (rew != cur)
                                        lg->setAxisTitle(a, rew);
                                }
                            }
                            lg->setAxisUnitFormat(mode == 1 ? Column::UnitFormat::ISIJSlash
                                                            : Column::UnitFormat::Paren);
                            lg->refreshAutoAxisTitlesFromColumns();
                        }
                        lg->replot();
                        emit lg->modifiedGraph();
                    }
                    d_target_ml->pushUndoSnapshot(tr("Change Unit Format"), before);
                    syncAxisControls();
                });
        ufRow->addWidget(d_lbl_unit_format);
        ufRow->addWidget(d_combo_unit_format, 1);
        lay->addLayout(ufRow);
    }

    d_edit_axis_title = new QLineEdit(page);
    connect(d_edit_axis_title, &QLineEdit::editingFinished, this, [this]() {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const QString nextText = d_edit_axis_title->text();
        if (g->axisTitle(d_active_axis) == nextText)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (Graph *lg : targetLayersForScope()) {
            if (!lg)
                continue;
            lg->setAxisTitleAuto(d_active_axis, false);
            QFont f = lg->axisTitleFont(d_active_axis);
            if (f.family().trimmed().isEmpty())
                f.setFamily(QStringLiteral("Arial"));
            lg->setAxisTitleFont(d_active_axis, f);
            lg->setAxisTitle(d_active_axis, nextText);
            lg->replot();
        }
        d_target_ml->pushUndoSnapshot(tr("Edit Axis Title"), before, kMergeAxisTitle);
    });
    lay->addWidget(d_edit_axis_title);

    QHBoxLayout *fmtRow = new QHBoxLayout();
    fmtRow->setSpacing(3);
    auto makeFmtBtn = [&](const QString &label) -> QToolButton * {
        QToolButton *b = new QToolButton(page);
        b->setText(label);
        b->setFixedHeight(23);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        fmtRow->addWidget(b);
        return b;
    };

    d_btn_fmt_italic = makeFmtBtn(QStringLiteral("I"));
    QFont itFont = d_btn_fmt_italic->font();
    itFont.setItalic(true);
    itFont.setBold(true);
    d_btn_fmt_italic->setFont(itFont);
    connect(d_btn_fmt_italic, &QToolButton::clicked, this, [this]() {
        applyAxisTitleMarkup(QStringLiteral("<i>"), QStringLiteral("</i>"));
    });

    d_btn_fmt_sub = makeFmtBtn(QString::fromUtf8("X\xE2\x82\x82")); // X₂
    connect(d_btn_fmt_sub, &QToolButton::clicked, this, [this]() {
        applyAxisTitleMarkup(QStringLiteral("<sub>"), QStringLiteral("</sub>"));
    });

    d_btn_fmt_sup = makeFmtBtn(QString::fromUtf8("X\xC2\xB2")); // X²
    connect(d_btn_fmt_sup, &QToolButton::clicked, this, [this]() {
        applyAxisTitleMarkup(QStringLiteral("<sup>"), QStringLiteral("</sup>"));
    });

    d_btn_fmt_greek = makeFmtBtn(QString());
    d_btn_fmt_greek->setPopupMode(QToolButton::InstantPopup);
    d_menu_greek = new QMenu(d_btn_fmt_greek);
    rebuildGreekSymbolMenu();
    d_btn_fmt_greek->setMenu(d_menu_greek);

    d_btn_fmt_unit = makeFmtBtn(QString());
    d_btn_fmt_unit->setPopupMode(QToolButton::InstantPopup);
    d_menu_units = new QMenu(d_btn_fmt_unit);
    rebuildUnitsSymbolMenu();
    d_btn_fmt_unit->setMenu(d_menu_units);
    lay->addLayout(fmtRow);

    d_chk_lock_all_axes_font = new QCheckBox(page);
    d_chk_lock_all_axes_font->setChecked(true); // Default checked per OriginPro ergonomics!
    lay->addWidget(d_chk_lock_all_axes_font);

    QGridLayout *fntGrid = new QGridLayout();
    fntGrid->setContentsMargins(0, 0, 0, 0);
    fntGrid->setHorizontalSpacing(6);
    fntGrid->setVerticalSpacing(4);

    d_lbl_title_pt = new QLabel(page);
    d_spin_axis_title_pt = new QSpinBox(page);
    d_spin_axis_title_pt->setRange(6, 72);
    d_spin_axis_title_pt->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_axis_title_pt, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int pt) {
                Graph *g = activeLayer();
                const QVector<Graph *> targets = targetLayersForScope();
                if (d_syncing_props || !d_target_ml || !g || targets.isEmpty())
                    return;
                const bool lockAll4 = d_chk_lock_all_axes_font && d_chk_lock_all_axes_font->isChecked();
                const int oldTitlePt =
                        qMax(6, g->axisTitleFont(d_active_axis).pointSize() > 0
                                     ? g->axisTitleFont(d_active_axis).pointSize()
                                     : 14);
                const int oldTickPt =
                        qMax(6, g->axisFont(d_active_axis).pointSize() > 0
                                     ? g->axisFont(d_active_axis).pointSize()
                                     : 12);
                const int scaledTickPt =
                        qBound(6, qRound(double(oldTickPt) * double(pt) / double(oldTitlePt)), 72);

                const QString before = d_target_ml->captureUndoSnapshot();
                auto applyLayerFonts = [&](Graph *lg) {
                    if (!lg)
                        return;
                    if (lockAll4) {
                        for (int a = 0; a < 4; ++a) {
                            QFont tf = lg->axisTitleFont(a);
                            if (tf.family().trimmed().isEmpty())
                                tf.setFamily(QStringLiteral("Arial"));
                            tf.setPointSize(pt);
                            lg->setAxisTitleFont(a, tf);

                            QFont nf = lg->axisFont(a);
                            if (nf.family().trimmed().isEmpty())
                                nf.setFamily(QStringLiteral("Arial"));
                            nf.setPointSize(scaledTickPt);
                            lg->setAxisFont(a, nf);
                        }
                        for (Graph *cand : d_target_ml->layersList()) {
                            if (cand && cand->linkedParentLayer() == lg) {
                                for (int a = 0; a < 4; ++a) {
                                    QFont ctf = cand->axisTitleFont(a);
                                    if (ctf.family().trimmed().isEmpty())
                                        ctf.setFamily(QStringLiteral("Arial"));
                                    ctf.setPointSize(pt);
                                    cand->setAxisTitleFont(a, ctf);

                                    QFont cnf = cand->axisFont(a);
                                    if (cnf.family().trimmed().isEmpty())
                                        cnf.setFamily(QStringLiteral("Arial"));
                                    cnf.setPointSize(scaledTickPt);
                                    cand->setAxisFont(a, cnf);
                                }
                                cand->replot();
                            }
                        }
                    } else {
                        QFont tf = lg->axisTitleFont(d_active_axis);
                        if (tf.family().trimmed().isEmpty())
                            tf.setFamily(QStringLiteral("Arial"));
                        tf.setPointSize(pt);
                        lg->setAxisTitleFont(d_active_axis, tf);
                    }
                    lg->replot();
                };
                for (Graph *lg : targets)
                    applyLayerFonts(lg);

                if (lockAll4 && d_spin_axis_tick_pt) {
                    QSignalBlocker bTick(d_spin_axis_tick_pt);
                    d_spin_axis_tick_pt->setValue(scaledTickPt);
                }
                d_target_ml->pushUndoSnapshot(tr("Axis Title Font Size"), before,
                                              kMergeAxisTitlePt);
            });

    d_lbl_tick_pt = new QLabel(page);
    d_spin_axis_tick_pt = new QSpinBox(page);
    d_spin_axis_tick_pt->setRange(6, 72);
    d_spin_axis_tick_pt->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_axis_tick_pt, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int pt) {
                const QVector<Graph *> targets = targetLayersForScope();
                if (d_syncing_props || !d_target_ml || targets.isEmpty())
                    return;
                const bool lockAll4 = d_chk_lock_all_axes_font && d_chk_lock_all_axes_font->isChecked();
                const QString before = d_target_ml->captureUndoSnapshot();
                for (Graph *lg : targets) {
                    if (!lg)
                        continue;
                    if (lockAll4) {
                        for (int a = 0; a < 4; ++a) {
                            QFont f = lg->axisFont(a);
                            if (f.family().trimmed().isEmpty())
                                f.setFamily(QStringLiteral("Arial"));
                            f.setPointSize(pt);
                            lg->setAxisFont(a, f);
                        }
                        for (Graph *cand : d_target_ml->layersList()) {
                            if (cand && cand->linkedParentLayer() == lg) {
                                for (int a = 0; a < 4; ++a) {
                                    QFont cf = cand->axisFont(a);
                                    if (cf.family().trimmed().isEmpty())
                                        cf.setFamily(QStringLiteral("Arial"));
                                    cf.setPointSize(pt);
                                    cand->setAxisFont(a, cf);
                                }
                                cand->replot();
                            }
                        }
                    } else {
                        QFont f = lg->axisFont(d_active_axis);
                        if (f.family().trimmed().isEmpty())
                            f.setFamily(QStringLiteral("Arial"));
                        f.setPointSize(pt);
                        lg->setAxisFont(d_active_axis, f);
                    }
                    lg->replot();
                }
                d_target_ml->pushUndoSnapshot(tr("Axis Tick Font Size"), before, kMergeAxisTickPt);
            });

    fntGrid->addWidget(d_lbl_title_pt, 0, 0);
    fntGrid->addWidget(d_spin_axis_title_pt, 0, 1);
    fntGrid->addWidget(d_lbl_tick_pt, 1, 0);
    fntGrid->addWidget(d_spin_axis_tick_pt, 1, 1);
    lay->addLayout(fntGrid);

    lay->addStretch(1);
    return page;
}

QWidget *GraphInspectorDock::buildCurveInspector()
{
    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 12, 10);
    lay->setSpacing(6);

    d_lbl_active_curve_name = new QLabel(page);
    d_lbl_active_curve_name->setObjectName(QStringLiteral("InspectorSubTitle"));
    d_lbl_active_curve_name->setWordWrap(true);
    lay->addWidget(d_lbl_active_curve_name);

    // =========================================================================
    // Regular 2D Curve / ErrorBar / Group Auto-Style Container
    // =========================================================================
    d_curve_regular_wrap = new QWidget(page);
    QVBoxLayout *regLay = new QVBoxLayout(d_curve_regular_wrap);
    regLay->setContentsMargins(0, 0, 0, 0);
    regLay->setSpacing(6);

    QGridLayout *styleGrid = new QGridLayout();
    styleGrid->setContentsMargins(0, 0, 0, 0);
    styleGrid->setHorizontalSpacing(6);
    styleGrid->setVerticalSpacing(4);

    d_lbl_plot_style = new QLabel(d_curve_regular_wrap);
    d_combo_curve_style = new QComboBox(d_curve_regular_wrap);
    d_combo_curve_style->addItem(QString(), int(Graph::Line));
    d_combo_curve_style->addItem(QString(), int(Graph::Scatter));
    d_combo_curve_style->addItem(QString(), int(Graph::LineSymbols));
    d_combo_curve_style->addItem(QString(), int(Graph::Spline));
    d_combo_curve_style->addItem(QString(), int(Graph::HorizontalSteps));
    d_combo_curve_style->addItem(QString(), int(Graph::VerticalSteps));
    d_combo_curve_style->addItem(QString(), int(Graph::VerticalDropLines));
    d_combo_curve_style->addItem(QString(), int(Graph::VerticalBars));
    d_combo_curve_style->addItem(QString(), int(Graph::HorizontalBars));
    d_combo_curve_style->addItem(QString(), int(Graph::Area));
    connect(d_combo_curve_style, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g || idx < 0)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                if (ci < 0 || ci >= g->curves())
                    return;
                QwtPlotCurve *cv = g->curve(ci);
                if (!cv)
                    return;

                const auto targetType =
                        static_cast<Graph::CurveType>(d_combo_curve_style->itemData(idx).toInt());
                const QString before = d_target_ml->captureUndoSnapshot();
                const QPen savedPen = cv->pen();
                const QwtSymbol savedSym = cv->symbol();
                const int savedYAxis = cv->yAxis();

                int resolvedCi = ci;
                if (Graph::canConvertTo(cv, targetType)) {
                    if (d_app)
                        d_app->invalidateModelessGraphDialogs(d_target_ml);
                    int ebBefore = 0;
                    if (DataCurve *dc = dynamic_cast<DataCurve *>(cv)) {
                        const int oldType = g->curveType(ci);
                        const bool barTransition =
                                ((targetType == Graph::VerticalBars
                                  || targetType == Graph::HorizontalBars)
                                 != (oldType == Graph::VerticalBars
                                     || oldType == Graph::HorizontalBars));
                        if (barTransition) {
                            for (DataCurve *eb : dc->errorBarsList()) {
                                const int ebi = g->curveIndex(eb);
                                if (ebi >= 0 && ebi < ci)
                                    ++ebBefore;
                            }
                        }
                    }
                    g->setCurveType(ci, targetType, true);
                    resolvedCi = qBound(0, ci - ebBefore, qMax(0, g->curves() - 1));
                    Graph::GraphSelection sel;
                    sel.kind = Graph::GraphSelection::Curve;
                    sel.primaryId = resolvedCi;
                    g->setCurrentSelection(sel, false);
                    cv = g->curve(resolvedCi);
                }
                if (cv) {
                    cv->setYAxis(savedYAxis);
                    const bool usesLine =
                            (targetType == Graph::Line || targetType == Graph::LineSymbols
                             || targetType == Graph::Spline || targetType == Graph::HorizontalSteps
                             || targetType == Graph::VerticalSteps
                             || targetType == Graph::VerticalDropLines
                             || targetType == Graph::Area);
                    if (usesLine) {
                        QPen p = savedPen;
                        if (p.style() == Qt::NoPen)
                            p.setStyle(Qt::SolidLine);
                        if (p.width() <= 0)
                            p.setWidth(2);
                        cv->setPen(p);
                    }
                    if (targetType == Graph::Scatter || targetType == Graph::LineSymbols
                        || targetType == Graph::Spline) {
                        if (targetType == Graph::Scatter || targetType == Graph::LineSymbols) {
                            QwtSymbol s = savedSym;
                            if (s.style() == QwtSymbol::NoSymbol) {
                                const QColor col = savedPen.color().isValid() ? savedPen.color()
                                                                              : QColor(Qt::black);
                                s.setStyle(QwtSymbol::Ellipse);
                                s.setSize(QSize(8, 8));
                                s.setPen(QPen(col, qMax(1, savedPen.width())));
                                s.setBrush(d_chk_sym_hollow->isChecked() ? QBrush(QColor(Qt::white))
                                                                         : QBrush(col));
                            }
                            cv->setSymbol(s);
                        } else if (savedSym.style() != QwtSymbol::NoSymbol) {
                            cv->setSymbol(savedSym);
                        }
                    } else if (targetType == Graph::Line || targetType == Graph::HorizontalSteps
                               || targetType == Graph::VerticalSteps
                               || targetType == Graph::VerticalDropLines) {
                        QwtSymbol s = cv->symbol();
                        s.setStyle(QwtSymbol::NoSymbol);
                        cv->setSymbol(s);
                    }
                    if (targetType == Graph::Area) {
                        QColor fillCol = cv->pen().color();
                        fillCol.setAlpha(65);
                        cv->setBrush(QBrush(fillCol));
                    }
                }
                g->reconstructLegend();
                g->updatePlot();
                d_target_ml->pushUndoSnapshot(tr("Change Curve Plot Style"), before);
                refreshObjectTree();
                syncCurveControls();
            });
    styleGrid->addWidget(d_lbl_plot_style, 0, 0);
    styleGrid->addWidget(d_combo_curve_style, 0, 1);
    regLay->addLayout(styleGrid);

    // Line Appearance
    d_sec_line_style_lbl = makeSectionHeaderLabel(d_curve_regular_wrap);
    regLay->addWidget(d_sec_line_style_lbl);

    QGridLayout *lineGrid = new QGridLayout();
    lineGrid->setContentsMargins(0, 0, 0, 0);
    lineGrid->setHorizontalSpacing(6);
    lineGrid->setVerticalSpacing(4);

    d_lbl_line_color = new QLabel(d_curve_regular_wrap);
    d_btn_line_color = new QPushButton(d_curve_regular_wrap);
    updateColorSwatchButton(d_btn_line_color, QColor(Qt::black));
    connect(d_btn_line_color, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;
        const bool hasLine =
                (cv->style() != QwtPlotCurve::NoCurve && cv->pen().style() != Qt::NoPen);
        const QColor cur = (hasLine && cv->pen().color().isValid()) ? cv->pen().color()
                                                                    : cv->symbol().pen().color();
        QColor chosen = pickColorInteractive(this, cur, tr("Select Series Color"));
        if (!chosen.isValid() || chosen == cur)
            return;

        if (d_spin_curve_opacity && chosen.alpha() == 255 && d_spin_curve_opacity->value() < 100)
            chosen.setAlpha(qRound(d_spin_curve_opacity->value() * 2.55));

        const QString before = d_target_ml->captureUndoSnapshot();
        QPen lp = cv->pen();
        lp.setColor(chosen);
        cv->setPen(lp);

        const bool linkSym = !d_chk_link_sym_color || d_chk_link_sym_color->isChecked();
        if (linkSym) {
            QwtSymbol sym = cv->symbol();
            const bool wasHollow = Graph::isSymbolWhiteHollow(sym)
                    || (d_chk_sym_hollow && d_chk_sym_hollow->isChecked());
            QPen sp = sym.pen();
            QColor edgeCol = chosen;
            edgeCol.setAlpha(chosen.alpha());
            sp.setColor(edgeCol);
            sym.setPen(sp);
            if (wasHollow)
                sym.setBrush(QBrush(QColor(Qt::white)));
            else if (sym.brush().style() != Qt::NoBrush)
                sym.setBrush(QBrush(chosen));
            cv->setSymbol(sym);
        }

        if (cv->brush().style() != Qt::NoBrush) {
            QColor areaCol = chosen;
            const int curAlpha = cv->brush().color().alpha();
            areaCol.setAlpha(curAlpha > 0 && curAlpha < 255 ? curAlpha : 65);
            cv->setBrush(QBrush(areaCol));
        }
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Series Color"), before);
        refreshObjectTree();
        syncCurveControls();
    });

    d_lbl_line_width = new QLabel(d_curve_regular_wrap);
    d_spin_line_width = new QSpinBox(d_curve_regular_wrap);
    d_spin_line_width->setRange(1, 16);
    d_spin_line_width->setSuffix(QStringLiteral(" px"));
    connect(d_spin_line_width, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int w) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        QPen lp = cv->pen();
        lp.setWidth(w);
        cv->setPen(lp);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Line Width"), before, kMergeCurveWidth);
        refreshObjectTree();
    });

    d_lbl_line_pen = new QLabel(d_curve_regular_wrap);
    d_combo_line_pen = new QComboBox(d_curve_regular_wrap);
    d_combo_line_pen->addItem(QString(), int(Qt::SolidLine));
    d_combo_line_pen->addItem(QString(), int(Qt::DashLine));
    d_combo_line_pen->addItem(QString(), int(Qt::DotLine));
    d_combo_line_pen->addItem(QString(), int(Qt::DashDotLine));
    d_combo_line_pen->addItem(QString(), int(Qt::NoPen));
    connect(d_combo_line_pen, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g || idx < 0)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
                if (!cv)
                    return;
                const auto ps = static_cast<Qt::PenStyle>(d_combo_line_pen->itemData(idx).toInt());
                const QString before = d_target_ml->captureUndoSnapshot();
                QPen lp = cv->pen();
                lp.setStyle(ps);
                cv->setPen(lp);
                if (ps == Qt::NoPen)
                    cv->setStyle(QwtPlotCurve::NoCurve);
                else if (cv->style() == QwtPlotCurve::NoCurve)
                    cv->setStyle(QwtPlotCurve::Lines);
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Line Pen Style"), before);
                refreshObjectTree();
            });

    d_lbl_curve_opacity = new QLabel(d_curve_regular_wrap);
    d_spin_curve_opacity = new QSpinBox(d_curve_regular_wrap);
    d_spin_curve_opacity->setRange(0, 100);
    d_spin_curve_opacity->setSingleStep(5);
    d_spin_curve_opacity->setValue(100);
    d_spin_curve_opacity->setSuffix(QStringLiteral("%"));
    connect(d_spin_curve_opacity, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int pct) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
                if (!cv)
                    return;
                const int alpha = qBound(0, qRound(pct * 2.55), 255);
                const QString before = d_target_ml->captureUndoSnapshot();
                QPen lp = cv->pen();
                QColor lc = lp.color();
                lc.setAlpha(alpha);
                lp.setColor(lc);
                cv->setPen(lp);

                if (cv->brush().style() != Qt::NoBrush) {
                    QColor bc = cv->brush().color();
                    bc.setAlpha(alpha);
                    cv->setBrush(QBrush(bc));
                }

                if (!d_chk_link_sym_color || d_chk_link_sym_color->isChecked()) {
                    QwtSymbol sym = cv->symbol();
                    QPen sp = sym.pen();
                    QColor sc = sp.color();
                    sc.setAlpha(alpha);
                    sp.setColor(sc);
                    sym.setPen(sp);
                    if (!Graph::isSymbolWhiteHollow(sym) && sym.brush().style() != Qt::NoBrush) {
                        QColor sfc = sym.brush().color();
                        sfc.setAlpha(alpha);
                        sym.setBrush(QBrush(sfc));
                    }
                    cv->setSymbol(sym);
                }
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Curve Opacity"), before,
                                              kMergeCurveOpacity);
                refreshObjectTree();
            });

    lineGrid->addWidget(d_lbl_line_color, 0, 0);
    lineGrid->addWidget(d_btn_line_color, 0, 1);
    lineGrid->addWidget(d_lbl_line_width, 1, 0);
    lineGrid->addWidget(d_spin_line_width, 1, 1);
    lineGrid->addWidget(d_lbl_line_pen, 2, 0);
    lineGrid->addWidget(d_combo_line_pen, 2, 1);
    lineGrid->addWidget(d_lbl_curve_opacity, 3, 0);
    lineGrid->addWidget(d_spin_curve_opacity, 3, 1);
    regLay->addLayout(lineGrid);

    // Symbol Appearance & Hollow + Independent Edge/Fill Colors + Right-Y Binding
    d_sec_symbol_style_lbl = makeSectionHeaderLabel(d_curve_regular_wrap);
    regLay->addWidget(d_sec_symbol_style_lbl);

    QGridLayout *symGrid = new QGridLayout();
    symGrid->setContentsMargins(0, 0, 0, 0);
    symGrid->setHorizontalSpacing(6);
    symGrid->setVerticalSpacing(4);

    d_lbl_sym_shape = new QLabel(d_curve_regular_wrap);
    d_combo_sym_shape = new QComboBox(d_curve_regular_wrap);
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Ellipse));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Rect));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Diamond));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::UTriangle));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::DTriangle));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::LTriangle));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::RTriangle));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Cross));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::XCross));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Star1));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::Hexagon));
    d_combo_sym_shape->addItem(QString(), int(QwtSymbol::NoSymbol));
    connect(d_combo_sym_shape, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g || idx < 0)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
                if (!cv)
                    return;
                const auto st =
                        static_cast<QwtSymbol::Style>(d_combo_sym_shape->itemData(idx).toInt());
                const QString before = d_target_ml->captureUndoSnapshot();
                QwtSymbol sym = cv->symbol();
                const bool hasLine =
                        (cv->style() != QwtPlotCurve::NoCurve && cv->pen().style() != Qt::NoPen);
                const QColor baseCol = (hasLine && cv->pen().color().isValid())
                        ? cv->pen().color()
                        : sym.pen().color();
                sym.setStyle(st);
                if (st != QwtSymbol::NoSymbol) {
                    if (sym.size().width() <= 0)
                        sym.setSize(QSize(d_spin_sym_size->value(), d_spin_sym_size->value()));
                    QPen sp = sym.pen();
                    if (!sp.color().isValid())
                        sp.setColor(baseCol);
                    if (sp.widthF() <= 0.0)
                        sp.setWidth(qMax(1, cv->pen().width()));
                    sym.setPen(sp);
                    if (d_chk_sym_hollow->isChecked()) {
                        sym.setBrush(QBrush(QColor(Qt::white)));
                    } else if (!d_chk_link_sym_color || d_chk_link_sym_color->isChecked()
                               || sym.brush().style() == Qt::NoBrush
                               || Graph::isSymbolWhiteHollow(sym)) {
                        sym.setBrush(QBrush(sp.color()));
                    }
                }
                cv->setSymbol(sym);
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Symbol Shape"), before);
                refreshObjectTree();
            });

    d_lbl_sym_size = new QLabel(d_curve_regular_wrap);
    d_spin_sym_size = new QSpinBox(d_curve_regular_wrap);
    d_spin_sym_size->setRange(2, 36);
    d_spin_sym_size->setValue(8);
    d_spin_sym_size->setSuffix(QStringLiteral(" px"));
    connect(d_spin_sym_size, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int sz) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        QwtSymbol sym = cv->symbol();
        sym.setSize(QSize(sz, sz));
        cv->setSymbol(sym);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Symbol Size"), before, kMergeSymSize);
        refreshObjectTree();
    });

    d_lbl_sym_skip = new QLabel(d_curve_regular_wrap);
    d_spin_sym_skip = new QSpinBox(d_curve_regular_wrap);
    d_spin_sym_skip->setRange(0, 500);
    d_spin_sym_skip->setValue(0);
    connect(d_spin_sym_skip, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int skip) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        PlotCurve *pc =
                (ci >= 0 && ci < g->curves()) ? dynamic_cast<PlotCurve *>(g->curve(ci)) : nullptr;
        if (!pc || pc->symbolSkip() == skip)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        pc->setSymbolSkip(skip);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Symbol Skip"), before, kMergeSymSkip);
    });

    symGrid->addWidget(d_lbl_sym_shape, 0, 0);
    symGrid->addWidget(d_combo_sym_shape, 0, 1);
    symGrid->addWidget(d_lbl_sym_size, 1, 0);
    symGrid->addWidget(d_spin_sym_size, 1, 1);
    symGrid->addWidget(d_lbl_sym_skip, 2, 0);
    symGrid->addWidget(d_spin_sym_skip, 2, 1);
    regLay->addLayout(symGrid);

    d_chk_sym_hollow = new QCheckBox(d_curve_regular_wrap);
    connect(d_chk_sym_hollow, &QCheckBox::toggled, this, [this](bool hollow) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        if (ci < 0 || ci >= g->curves())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        g->setCurveSymbolHollow(ci, hollow);
        d_target_ml->pushUndoSnapshot(tr("Toggle Hollow Symbol"), before);
        refreshObjectTree();
        syncCurveControls();
    });
    regLay->addWidget(d_chk_sym_hollow);

    d_chk_link_sym_color = new QCheckBox(d_curve_regular_wrap);
    d_chk_link_sym_color->setChecked(true);
    regLay->addWidget(d_chk_link_sym_color);

    d_sym_custom_colors_wrap = new QWidget(d_curve_regular_wrap);
    QGridLayout *symColGrid = new QGridLayout(d_sym_custom_colors_wrap);
    symColGrid->setContentsMargins(8, 2, 0, 2);
    symColGrid->setHorizontalSpacing(6);
    symColGrid->setVerticalSpacing(4);

    d_lbl_sym_edge_color = new QLabel(d_sym_custom_colors_wrap);
    d_btn_sym_edge_color = new QPushButton(d_sym_custom_colors_wrap);
    updateColorSwatchButton(d_btn_sym_edge_color, QColor(Qt::black));
    connect(d_btn_sym_edge_color, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;
        QwtSymbol sym = cv->symbol();
        const QColor cur = sym.pen().color().isValid() ? sym.pen().color() : cv->pen().color();
        const QColor chosen = pickColorInteractive(this, cur, tr("Select Symbol Edge Color"));
        if (!chosen.isValid() || chosen == cur)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        QPen sp = sym.pen();
        sp.setColor(chosen);
        sym.setPen(sp);
        cv->setSymbol(sym);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Symbol Edge Color"), before);
        refreshObjectTree();
        syncCurveControls();
    });

    d_lbl_sym_fill_color = new QLabel(d_sym_custom_colors_wrap);
    d_btn_sym_fill_color = new QPushButton(d_sym_custom_colors_wrap);
    updateColorSwatchButton(d_btn_sym_fill_color, QColor(Qt::white));
    connect(d_btn_sym_fill_color, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;
        QwtSymbol sym = cv->symbol();
        const QColor cur =
                (sym.brush().style() != Qt::NoBrush && sym.brush().color().isValid())
                ? sym.brush().color()
                : QColor(Qt::white);
        const QColor chosen = pickColorInteractive(this, cur, tr("Select Symbol Fill Color"));
        if (!chosen.isValid() || chosen == cur)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        sym.setBrush(QBrush(chosen));
        cv->setSymbol(sym);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Symbol Fill Color"), before);
        refreshObjectTree();
        syncCurveControls();
    });

    d_lbl_sym_edge_width = new QLabel(d_sym_custom_colors_wrap);
    d_spin_sym_edge_width = new QDoubleSpinBox(d_sym_custom_colors_wrap);
    d_spin_sym_edge_width->setRange(0.5, 4.0);
    d_spin_sym_edge_width->setSingleStep(0.5);
    d_spin_sym_edge_width->setDecimals(1);
    d_spin_sym_edge_width->setValue(1.5);
    d_spin_sym_edge_width->setSuffix(QStringLiteral(" px"));
    connect(d_spin_sym_edge_width, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double w) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g)
                    return;
                const int ci = effectiveSelectedCurveIndex(g);
                QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
                if (!cv)
                    return;
                const QString before = d_target_ml->captureUndoSnapshot();
                QwtSymbol sym = cv->symbol();
                QPen sp = sym.pen();
                sp.setWidthF(w);
                sym.setPen(sp);
                cv->setSymbol(sym);
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Symbol Edge Width"), before,
                                              kMergeSymEdgeWidth);
                refreshObjectTree();
            });

    symColGrid->addWidget(d_lbl_sym_edge_color, 0, 0);
    symColGrid->addWidget(d_btn_sym_edge_color, 0, 1);
    symColGrid->addWidget(d_lbl_sym_fill_color, 1, 0);
    symColGrid->addWidget(d_btn_sym_fill_color, 1, 1);
    symColGrid->addWidget(d_lbl_sym_edge_width, 2, 0);
    symColGrid->addWidget(d_spin_sym_edge_width, 2, 1);
    d_sym_custom_colors_wrap->hide();
    regLay->addWidget(d_sym_custom_colors_wrap);

    connect(d_chk_link_sym_color, &QCheckBox::toggled, this, [this](bool linked) {
        if (d_sym_custom_colors_wrap)
            d_sym_custom_colors_wrap->setVisible(!linked);
        if (d_syncing_props || !linked)
            return;
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv || !cv->pen().color().isValid())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        QwtSymbol sym = cv->symbol();
        const bool wasHollow = Graph::isSymbolWhiteHollow(sym)
                || (d_chk_sym_hollow && d_chk_sym_hollow->isChecked());
        QPen sp = sym.pen();
        sp.setColor(cv->pen().color());
        sym.setPen(sp);
        sym.setBrush(wasHollow ? QBrush(QColor(Qt::white)) : QBrush(cv->pen().color()));
        cv->setSymbol(sym);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Link Symbol Color"), before);
        refreshObjectTree();
        syncCurveControls();
    });

    d_chk_curve_right_y = new QCheckBox(d_curve_regular_wrap);
    connect(d_chk_curve_right_y, &QCheckBox::toggled, this, [this](bool rightY) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const int ci = effectiveSelectedCurveIndex(g);
        QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
        if (!cv)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        const int targetAxis = rightY ? QwtPlot::yRight : QwtPlot::yLeft;
        cv->setYAxis(targetAxis);
        if (DataCurve *dc = dynamic_cast<DataCurve *>(cv)) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->setYAxis(targetAxis);
            }
        }
        if (rightY) {
            g->enableAxis(QwtPlot::yRight, true);
            g->setAxisBackboneVisible(QwtPlot::yRight, true);
            if (Plot *pw = g->plotWidget()) {
                if (QwtScaleDraw *sd = pw->axisScaleDraw(QwtPlot::yRight))
                    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                pw->setMajorTicksType(QwtPlot::yRight, ScaleDraw::In);
                pw->setMinorTicksType(QwtPlot::yRight, ScaleDraw::In);
            }
        }
        if (!g->isAxisUserLocked(targetAxis))
            g->setAutoScaleAxis(targetAxis);
        g->refreshAutoAxisTitlesFromColumns();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Assign Curve to Right-Y"), before);
        refreshObjectTree();
        syncCurveControls();
    });
    d_btn_match_right_y_color = new QPushButton(d_curve_regular_wrap);
    d_btn_match_right_y_color->setIcon(IconLoader::duotoneIcon(QStringLiteral("palette"), 13));
    connect(d_btn_match_right_y_color, &QPushButton::clicked, this,
            &GraphInspectorDock::matchRightYAxisColorToCurve);
    regLay->addWidget(d_chk_curve_right_y);
    regLay->addWidget(d_btn_match_right_y_color);

    // Quick Error Bars + Live Error Bar Styling
    d_sec_err_bars_lbl = makeSectionHeaderLabel(d_curve_regular_wrap);
    regLay->addWidget(d_sec_err_bars_lbl);

    QHBoxLayout *errRow = new QHBoxLayout();
    errRow->setSpacing(4);
    d_btn_err_adjacent = new QPushButton(d_curve_regular_wrap);
    d_btn_err_sd = new QPushButton(d_curve_regular_wrap);
    d_btn_err_5pct = new QPushButton(d_curve_regular_wrap);
    connect(d_btn_err_adjacent, &QPushButton::clicked, this,
            [this]() { applyQuickErrorBars(0); });
    connect(d_btn_err_sd, &QPushButton::clicked, this, [this]() { applyQuickErrorBars(1); });
    connect(d_btn_err_5pct, &QPushButton::clicked, this, [this]() { applyQuickErrorBars(2); });
    errRow->addWidget(d_btn_err_adjacent);
    errRow->addWidget(d_btn_err_sd);
    errRow->addWidget(d_btn_err_5pct);
    regLay->addLayout(errRow);

    d_err_style_wrap = new QWidget(d_curve_regular_wrap);
    QVBoxLayout *errStyleLay = new QVBoxLayout(d_err_style_wrap);
    errStyleLay->setContentsMargins(0, 2, 0, 0);
    errStyleLay->setSpacing(4);

    d_sec_err_style_lbl = makeSectionHeaderLabel(d_err_style_wrap);
    errStyleLay->addWidget(d_sec_err_style_lbl);

    QGridLayout *errGrid = new QGridLayout();
    errGrid->setContentsMargins(0, 0, 0, 0);
    errGrid->setHorizontalSpacing(6);
    errGrid->setVerticalSpacing(4);

    d_lbl_err_width = new QLabel(d_err_style_wrap);
    d_spin_err_width = new QSpinBox(d_err_style_wrap);
    d_spin_err_width->setRange(1, 10);
    d_spin_err_width->setSuffix(QStringLiteral(" px"));
    connect(d_spin_err_width, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int w) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
        if (errs.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (QwtErrorPlotCurve *er : errs)
            er->setWidth(w);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Error Bar Width"), before, kMergeErrWidth);
    });

    d_lbl_err_cap = new QLabel(d_err_style_wrap);
    d_spin_err_cap = new QSpinBox(d_err_style_wrap);
    d_spin_err_cap->setRange(0, 32);
    d_spin_err_cap->setSingleStep(2);
    d_spin_err_cap->setSuffix(QStringLiteral(" px"));
    connect(d_spin_err_cap, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int cap) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
        if (errs.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (QwtErrorPlotCurve *er : errs)
            er->setCapLength(cap);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Error Bar Cap"), before, kMergeErrCap);
    });

    d_lbl_err_dir = new QLabel(d_err_style_wrap);
    d_combo_err_dir = new QComboBox(d_err_style_wrap);
    d_combo_err_dir->addItem(QString(), 0); // Both (±)
    d_combo_err_dir->addItem(QString(), 1); // Plus (+)
    d_combo_err_dir->addItem(QString(), 2); // Minus (-)
    connect(d_combo_err_dir, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g || idx < 0)
                    return;
                const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
                if (errs.isEmpty())
                    return;
                const int mode = d_combo_err_dir->itemData(idx).toInt();
                const QString before = d_target_ml->captureUndoSnapshot();
                for (QwtErrorPlotCurve *er : errs) {
                    er->drawPlusSide(mode != 2);
                    er->drawMinusSide(mode != 1);
                }
                g->updatePlot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Error Bar Direction"), before);
            });

    d_lbl_err_color = new QLabel(d_err_style_wrap);
    QHBoxLayout *errColRow = new QHBoxLayout();
    errColRow->setContentsMargins(0, 0, 0, 0);
    errColRow->setSpacing(4);
    d_btn_err_color = new QPushButton(d_err_style_wrap);
    updateColorSwatchButton(d_btn_err_color, QColor(Qt::black));
    connect(d_btn_err_color, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
        if (errs.isEmpty())
            return;
        const QColor cur = errs.first()->color();
        const QColor chosen = pickColorInteractive(this, cur, tr("Select Error Bar Color"));
        if (!chosen.isValid() || chosen == cur)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (QwtErrorPlotCurve *er : errs)
            er->setColor(chosen);
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Error Bar Color"), before);
        syncCurveControls();
    });
    d_btn_err_match_curve = new QPushButton(d_err_style_wrap);
    connect(d_btn_err_match_curve, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
        if (errs.isEmpty())
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        for (QwtErrorPlotCurve *er : errs) {
            if (DataCurve *mc = er->masterCurve()) {
                const bool hasL =
                        (mc->style() != QwtPlotCurve::NoCurve && mc->pen().style() != Qt::NoPen);
                const QColor mcCol =
                        (hasL && mc->pen().color().isValid()) ? mc->pen().color()
                                                              : mc->symbol().pen().color();
                if (mcCol.isValid())
                    er->setColor(mcCol);
            }
        }
        g->updatePlot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Error Bar Color"), before);
        syncCurveControls();
    });
    errColRow->addWidget(d_btn_err_color);
    errColRow->addWidget(d_btn_err_match_curve, 1);

    errGrid->addWidget(d_lbl_err_width, 0, 0);
    errGrid->addWidget(d_spin_err_width, 0, 1);
    errGrid->addWidget(d_lbl_err_cap, 1, 0);
    errGrid->addWidget(d_spin_err_cap, 1, 1);
    errGrid->addWidget(d_lbl_err_dir, 2, 0);
    errGrid->addWidget(d_combo_err_dir, 2, 1);
    errGrid->addWidget(d_lbl_err_color, 3, 0);
    errGrid->addLayout(errColRow, 3, 1);
    errStyleLay->addLayout(errGrid);
    d_err_style_wrap->hide();
    regLay->addWidget(d_err_style_wrap);

    // Group Series Style Scheme (OriginPro Palette + Symbol Shape Cycle + Paired Hollow)
    d_sec_series_scheme_lbl = makeSectionHeaderLabel(d_curve_regular_wrap);
    regLay->addWidget(d_sec_series_scheme_lbl);

    d_combo_series_scheme = new QComboBox(d_curve_regular_wrap);
    refreshPaletteSchemeCombo();
    regLay->addWidget(d_combo_series_scheme);

    d_chk_cycle_sym_shapes = new QCheckBox(d_curve_regular_wrap);
    d_chk_cycle_sym_shapes->setChecked(true);
    regLay->addWidget(d_chk_cycle_sym_shapes);

    d_chk_paired_hollow = new QCheckBox(d_curve_regular_wrap);
    d_chk_paired_hollow->setChecked(false);
    regLay->addWidget(d_chk_paired_hollow);

    d_btn_apply_scheme_all = new QPushButton(d_curve_regular_wrap);
    d_btn_apply_scheme_all->setIcon(IconLoader::duotoneIcon(QStringLiteral("palette"), 14));
    connect(d_btn_apply_scheme_all, &QPushButton::clicked, this,
            &GraphInspectorDock::applyGroupAutoStyleFromUi);
    regLay->addWidget(d_btn_apply_scheme_all);

    // Non-Destructive Y-Offset Stack
    d_sec_y_stack_lbl = makeSectionHeaderLabel(d_curve_regular_wrap);
    regLay->addWidget(d_sec_y_stack_lbl);

    QHBoxLayout *stkGapRow = new QHBoxLayout();
    d_lbl_stack_gap = new QLabel(d_curve_regular_wrap);
    d_spin_stack_gap = new QDoubleSpinBox(d_curve_regular_wrap);
    d_spin_stack_gap->setRange(0.0, 200.0);
    d_spin_stack_gap->setDecimals(0);
    d_spin_stack_gap->setSingleStep(5.0);
    d_spin_stack_gap->setValue(15.0);
    d_spin_stack_gap->setSuffix(QStringLiteral("%"));
    stkGapRow->addWidget(d_lbl_stack_gap);
    stkGapRow->addWidget(d_spin_stack_gap);
    regLay->addLayout(stkGapRow);

    QHBoxLayout *stkBtnRow = new QHBoxLayout();
    stkBtnRow->setSpacing(4);
    d_btn_apply_stack = new QPushButton(d_curve_regular_wrap);
    connect(d_btn_apply_stack, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        g->setStackOffsets(1 /*AutoGap*/, d_spin_stack_gap->value(), 0.0, false);
        d_target_ml->pushUndoSnapshot(tr("Apply Y-Offset Stack"), before);
        syncAxisControls();
    });
    d_btn_reset_stack = new QPushButton(d_curve_regular_wrap);
    connect(d_btn_reset_stack, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        g->resetStackOffsets();
        d_target_ml->pushUndoSnapshot(tr("Reset Stack Offsets"), before);
        syncAxisControls();
    });
    stkBtnRow->addWidget(d_btn_apply_stack);
    stkBtnRow->addWidget(d_btn_reset_stack);
    regLay->addLayout(stkBtnRow);

    lay->addWidget(d_curve_regular_wrap);

    // =========================================================================
    // Spectrogram / Heatmap Live Controls Container
    // =========================================================================
    d_spectrogram_wrap = new QWidget(page);
    QVBoxLayout *specLay = new QVBoxLayout(d_spectrogram_wrap);
    specLay->setContentsMargins(0, 0, 0, 0);
    specLay->setSpacing(6);

    d_sec_spec_lbl = makeSectionHeaderLabel(d_spectrogram_wrap);
    specLay->addWidget(d_sec_spec_lbl);

    QGridLayout *specGrid = new QGridLayout();
    specGrid->setContentsMargins(0, 0, 0, 0);
    specGrid->setHorizontalSpacing(6);
    specGrid->setVerticalSpacing(4);

    d_lbl_spec_cmap = new QLabel(d_spectrogram_wrap);
    d_combo_spec_cmap = new QComboBox(d_spectrogram_wrap);
    d_combo_spec_cmap->addItem(QString(), int(Spectrogram::PresetLegacyDefault));
    d_combo_spec_cmap->addItem(QStringLiteral("Viridis"), int(Spectrogram::PresetViridis));
    d_combo_spec_cmap->addItem(QStringLiteral("Magma"), int(Spectrogram::PresetMagma));
    d_combo_spec_cmap->addItem(QStringLiteral("Inferno"), int(Spectrogram::PresetInferno));
    d_combo_spec_cmap->addItem(QStringLiteral("Plasma"), int(Spectrogram::PresetPlasma));
    d_combo_spec_cmap->addItem(QStringLiteral("Cividis"), int(Spectrogram::PresetCividis));
    d_combo_spec_cmap->addItem(QString(), int(Spectrogram::PresetCoolWarm));
    d_combo_spec_cmap->addItem(QString(), int(Spectrogram::PresetBlueWhiteRed));
    d_combo_spec_cmap->addItem(QStringLiteral("Jet"), int(Spectrogram::PresetJet));
    d_combo_spec_cmap->addItem(QString(), int(Spectrogram::PresetGrayScale));

    d_chk_spec_reverse = new QCheckBox(d_spectrogram_wrap);

    auto applySpecColormap = [this]() {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const auto preset =
                static_cast<Spectrogram::ColorMapPreset>(d_combo_spec_cmap->currentData().toInt());
        const bool rev = d_chk_spec_reverse->isChecked();
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setColorMapPreset(preset, rev);
        sp->syncColorScale();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Heatmap Colormap"), before);
        refreshObjectTree();
    };
    connect(d_combo_spec_cmap, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [applySpecColormap](int) { applySpecColormap(); });
    connect(d_chk_spec_reverse, &QCheckBox::toggled, this,
            [applySpecColormap](bool) { applySpecColormap(); });

    specGrid->addWidget(d_lbl_spec_cmap, 0, 0);
    specGrid->addWidget(d_combo_spec_cmap, 0, 1);
    specGrid->addWidget(d_chk_spec_reverse, 1, 0, 1, 2);

    d_chk_spec_colorbar = new QCheckBox(d_spectrogram_wrap);
    connect(d_chk_spec_colorbar, &QCheckBox::toggled, this, [this](bool on) {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        const int axis = (sp->colorScaleAxis() >= 0 && sp->colorScaleAxis() < 4)
                ? sp->colorScaleAxis()
                : int(QwtPlot::yRight);
        sp->showColorScale(axis, on);
        if (on)
            sp->enforceColorBarArialFont();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Toggle Colorbar"), before);
        syncCurveControls();
    });
    specGrid->addWidget(d_chk_spec_colorbar, 2, 0, 1, 2);

    d_lbl_spec_cb_title = new QLabel(d_spectrogram_wrap);
    d_edit_spec_cb_title = new QLineEdit(d_spectrogram_wrap);
    connect(d_edit_spec_cb_title, &QLineEdit::editingFinished, this, [this]() {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const QString nextTitle = d_edit_spec_cb_title->text();
        if (sp->colorBarTitle() == nextTitle)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setColorBarTitle(nextTitle);
        sp->enforceColorBarArialFont();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Edit Colorbar Title"), before, kMergeSpecCbTitle);
    });
    d_spec_cb_sym_bar = createMatSciRichTextBar(d_spectrogram_wrap, d_edit_spec_cb_title);
    specGrid->addWidget(d_lbl_spec_cb_title, 3, 0);
    specGrid->addWidget(d_edit_spec_cb_title, 3, 1);
    specGrid->addWidget(d_spec_cb_sym_bar, 4, 0, 1, 2);

    d_lbl_spec_cb_width = new QLabel(d_spectrogram_wrap);
    d_spin_spec_cb_width = new QSpinBox(d_spectrogram_wrap);
    d_spin_spec_cb_width->setRange(6, 64);
    d_spin_spec_cb_width->setSuffix(QStringLiteral(" px"));
    connect(d_spin_spec_cb_width, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int w) {
                Graph *g = activeLayer();
                Spectrogram *sp = effectiveSelectedSpectrogram(g);
                if (d_syncing_props || !d_target_ml || !g || !sp)
                    return;
                const QString before = d_target_ml->captureUndoSnapshot();
                sp->setColorBarWidth(w);
                g->replot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Colorbar Width"), before,
                                              kMergeSpecCbWidth);
            });
    specGrid->addWidget(d_lbl_spec_cb_width, 5, 0);
    specGrid->addWidget(d_spin_spec_cb_width, 5, 1);

    d_chk_spec_contours = new QCheckBox(d_spectrogram_wrap);
    connect(d_chk_spec_contours, &QCheckBox::toggled, this, [this](bool on) {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setDisplayMode(QwtPlotSpectrogram::ContourMode, on);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Toggle Contour Lines"), before);
        syncCurveControls();
    });
    specGrid->addWidget(d_chk_spec_contours, 6, 0, 1, 2);

    d_lbl_spec_levels = new QLabel(d_spectrogram_wrap);
    d_spin_spec_levels = new QSpinBox(d_spectrogram_wrap);
    d_spin_spec_levels->setRange(2, 50);
    connect(d_spin_spec_levels, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int lv) {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setLevelsNumber(lv);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Contour Levels"), before, kMergeSpecLevels);
    });
    specGrid->addWidget(d_lbl_spec_levels, 7, 0);
    specGrid->addWidget(d_spin_spec_levels, 7, 1);

    d_chk_spec_custom_z = new QCheckBox(d_spectrogram_wrap);
    d_lbl_spec_zmin = new QLabel(d_spectrogram_wrap);
    d_edit_spec_zmin = new QLineEdit(d_spectrogram_wrap);
    d_lbl_spec_zmax = new QLabel(d_spectrogram_wrap);
    d_edit_spec_zmax = new QLineEdit(d_spectrogram_wrap);

    auto applySpecZRange = [this]() {
        Graph *g = activeLayer();
        Spectrogram *sp = effectiveSelectedSpectrogram(g);
        if (d_syncing_props || !d_target_ml || !g || !sp)
            return;
        const bool custom = d_chk_spec_custom_z->isChecked();
        d_edit_spec_zmin->setEnabled(custom);
        d_edit_spec_zmax->setEnabled(custom);
        bool okMin = false, okMax = false;
        double zMin = d_edit_spec_zmin->text().trimmed().toDouble(&okMin);
        double zMax = d_edit_spec_zmax->text().trimmed().toDouble(&okMax);
        if (custom && (!okMin || !okMax || !std::isfinite(zMin) || !std::isfinite(zMax)))
            return;
        if (custom && qAbs(zMax - zMin) < 1e-30)
            zMax = zMin + 1.0;
        const QString before = d_target_ml->captureUndoSnapshot();
        sp->setCustomValueRange(custom, zMin, zMax);
        sp->syncColorScale();
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Heatmap Z Range"), before);
        syncCurveControls();
    };
    connect(d_chk_spec_custom_z, &QCheckBox::toggled, this,
            [applySpecZRange](bool) { applySpecZRange(); });
    connect(d_edit_spec_zmin, &QLineEdit::editingFinished, this, applySpecZRange);
    connect(d_edit_spec_zmax, &QLineEdit::editingFinished, this, applySpecZRange);

    specGrid->addWidget(d_chk_spec_custom_z, 8, 0, 1, 2);
    specGrid->addWidget(d_lbl_spec_zmin, 9, 0);
    specGrid->addWidget(d_edit_spec_zmin, 9, 1);
    specGrid->addWidget(d_lbl_spec_zmax, 10, 0);
    specGrid->addWidget(d_edit_spec_zmax, 10, 1);

    specLay->addLayout(specGrid);
    d_spectrogram_wrap->hide();
    lay->addWidget(d_spectrogram_wrap);

    lay->addStretch(1);
    return page;
}

QWidget *GraphInspectorDock::buildLegendInspector()
{
    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 12, 10);
    lay->setSpacing(6);

    d_lbl_legend_target = new QLabel(page);
    d_lbl_legend_target->setObjectName(QStringLiteral("InspectorSubTitle"));
    lay->addWidget(d_lbl_legend_target);

    d_sec_legend_pos_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_legend_pos_lbl);

    QGridLayout *snapGrid = new QGridLayout();
    snapGrid->setContentsMargins(0, 0, 0, 0);
    snapGrid->setHorizontalSpacing(4);
    snapGrid->setVerticalSpacing(4);

    auto makeSnapBtn = [&](Graph::LegendAnchor anchor, int row, int col) -> QPushButton * {
        QPushButton *b = new QPushButton(page);
        connect(b, &QPushButton::clicked, this, [this, anchor]() {
            Graph *g = activeLayer();
            if (!d_target_ml || !g)
                return;
            const QString before = d_target_ml->captureUndoSnapshot();
            if (!g->hasLegend())
                g->newLegend();
            g->setLegendVisible(true);
            g->placeLegendAnchor(anchor);
            d_target_ml->pushUndoSnapshot(tr("Place Legend"), before);
            refreshObjectTree();
            syncLegendControls();
        });
        snapGrid->addWidget(b, row, col);
        return b;
    };

    d_btn_leg_tl = makeSnapBtn(Graph::LegendTopLeft, 0, 0);
    d_btn_leg_tc = makeSnapBtn(Graph::LegendTopCenter, 0, 1);
    d_btn_leg_tr = makeSnapBtn(Graph::LegendTopRight, 0, 2);
    d_btn_leg_bl = makeSnapBtn(Graph::LegendBottomLeft, 1, 0);
    d_btn_leg_br = makeSnapBtn(Graph::LegendBottomRight, 1, 1);
    d_btn_leg_out_r = makeSnapBtn(Graph::LegendOutsideRight, 1, 2);
    lay->addLayout(snapGrid);

    d_btn_leg_auto_avoid = new QPushButton(page);
    d_btn_leg_auto_avoid->setIcon(IconLoader::duotoneIcon(QStringLiteral("legend"), 15));
    connect(d_btn_leg_auto_avoid, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        if (!g->hasLegend())
            g->newLegend();
        g->setLegendVisible(true);
        g->placeLegendAnchor(Graph::LegendAutoAvoidData);
        d_target_ml->pushUndoSnapshot(tr("Place Legend in Empty Space"), before);
        refreshObjectTree();
        syncLegendControls();
    });
    lay->addWidget(d_btn_leg_auto_avoid);

    d_btn_leg_rebuild = new QPushButton(page);
    d_btn_leg_rebuild->setObjectName(QStringLiteral("InspectorRebuildLegendBtn"));
    d_btn_leg_rebuild->setIcon(IconLoader::duotoneIcon(QStringLiteral("recalc-now"), 15));
    connect(d_btn_leg_rebuild, &QPushButton::clicked, this, [this]() {
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        g->reconstructLegend();
        g->setLegendVisible(true);
        g->replot();
        d_target_ml->pushUndoSnapshot(tr("Reconstruct Legend"), before);
        refreshObjectTree();
        syncLegendControls();
    });
    lay->addWidget(d_btn_leg_rebuild);

    d_sec_legend_style_lbl = makeSectionHeaderLabel(page);
    lay->addWidget(d_sec_legend_style_lbl);

    QGridLayout *legGrid = new QGridLayout();
    legGrid->setContentsMargins(0, 0, 0, 0);
    legGrid->setHorizontalSpacing(6);
    legGrid->setVerticalSpacing(4);

    // Frame modes:
    //  0 -> Borderless (Legend::None, transparent bg)
    //  1 -> Black Box + White Fill (Legend::Line, solid white bg)
    //  2 -> Transparent Background + Box (Legend::Line, transparent bg)
    d_lbl_leg_frame = new QLabel(page);
    d_combo_leg_frame = new QComboBox(page);
    d_combo_leg_frame->addItem(QString(), 0);
    d_combo_leg_frame->addItem(QString(), 1);
    d_combo_leg_frame->addItem(QString(), 2);
    connect(d_combo_leg_frame, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                Graph *g = activeLayer();
                if (d_syncing_props || !d_target_ml || !g || idx < 0)
                    return;
                Legend *target = nullptr;
                const auto selKind = g->currentSelection().kind;
                if (selKind == Graph::GraphSelection::TextAnnotation)
                    target = g->textMarker(g->currentSelection().primaryId);
                else if (selKind != Graph::GraphSelection::ArrowAnnotation
                         && selKind != Graph::GraphSelection::ImageAnnotation)
                    target = g->legend();
                if (!target)
                    return;

                const int mode = d_combo_leg_frame->itemData(idx).toInt();
                const QString before = d_target_ml->captureUndoSnapshot();
                if (mode == 0) {
                    target->setFrameStyle(Legend::None);
                    target->setBackgroundColor(QColor(255, 255, 255, 0));
                } else if (mode == 1) {
                    target->setFrameStyle(Legend::Line);
                    target->setBackgroundColor(QColor(255, 255, 255, 255));
                } else {
                    target->setFrameStyle(Legend::Line);
                    target->setBackgroundColor(QColor(255, 255, 255, 0));
                }
                g->replot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Change Legend Frame"), before);
            });

    d_lbl_leg_header = new QLabel(page);
    d_edit_leg_header = new QLineEdit(page);
    connect(d_edit_leg_header, &QLineEdit::editingFinished, this, [this]() {
        if (d_syncing_props || !d_edit_leg_header || !d_edit_leg_header->isModified())
            return;
        Graph *g = activeLayer();
        if (!d_target_ml || !g)
            return;

        const auto selKind = g->currentSelection().kind;
        Legend *leg = nullptr;
        if (selKind == Graph::GraphSelection::TextAnnotation)
            leg = g->textMarker(g->currentSelection().primaryId);
        else if (selKind != Graph::GraphSelection::ArrowAnnotation
                 && selKind != Graph::GraphSelection::ImageAnnotation)
            leg = g->legend();

        const bool isPrimaryLegend = (g && leg && leg == g->legend())
                || (g && !leg && selKind == Graph::GraphSelection::LegendBox);

        if (!isPrimaryLegend) {
            if (!leg)
                return;
            QStringList lines = leg->text().split('\n');
            if (lines.isEmpty())
                lines.append(d_edit_leg_header->text());
            else
                lines[0] = d_edit_leg_header->text();
            const QString nextTxt = lines.join('\n');
            d_edit_leg_header->setModified(false);
            if (leg->text() == nextTxt)
                return;
            const QString before = d_target_ml->captureUndoSnapshot();
            leg->setText(nextTxt);
            g->replot();
            emit g->modifiedGraph();
            d_target_ml->pushUndoSnapshot(tr("Edit Annotation Text"), before, kMergeLegendHeader);
            refreshObjectTree();
            return;
        }

        if (!g->hasLegend())
            g->newLegend();
        leg = g->legend();
        if (!leg)
            return;

        QStringList lines = leg->text().split('\n');
        while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
            lines.removeLast();

        const QString hdr = d_edit_leg_header->text().trimmed();
        if (!lines.isEmpty() && !isLegendEntryLine(lines.first())) {
            if (hdr.isEmpty())
                lines.removeFirst();
            else
                lines[0] = hdr;
        } else if (!hdr.isEmpty()) {
            lines.prepend(hdr);
        }

        const QString updated = lines.join(QStringLiteral("\n"));
        d_edit_leg_header->setModified(false);
        if (leg->text() == updated)
            return;

        const QString before = d_target_ml->captureUndoSnapshot();
        leg->setText(updated);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Edit Legend Header"), before, kMergeLegendHeader);
    });

    d_lbl_leg_cols = new QLabel(page);
    d_spin_leg_cols = new QSpinBox(page);
    d_spin_leg_cols->setRange(1, 4);
    d_spin_leg_cols->setValue(1);
    connect(d_spin_leg_cols, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int cols) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        Legend *leg = g->legend();
        if (!leg)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        leg->setNumColumns(cols);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Legend Columns"), before, kMergeLegendCols);
    });

    d_lbl_leg_font_pt = new QLabel(page);
    d_spin_leg_font_pt = new QSpinBox(page);
    d_spin_leg_font_pt->setRange(6, 72);
    d_spin_leg_font_pt->setValue(12);
    d_spin_leg_font_pt->setSuffix(QStringLiteral(" pt"));
    connect(d_spin_leg_font_pt, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int pt) {
        Graph *g = activeLayer();
        if (d_syncing_props || !d_target_ml || !g)
            return;
        Legend *target = nullptr;
        const auto selKind = g->currentSelection().kind;
        if (selKind == Graph::GraphSelection::TextAnnotation)
            target = g->textMarker(g->currentSelection().primaryId);
        else if (selKind != Graph::GraphSelection::ArrowAnnotation
                 && selKind != Graph::GraphSelection::ImageAnnotation)
            target = g->legend();
        if (!target)
            return;
        const QString before = d_target_ml->captureUndoSnapshot();
        QFont f = target->font();
        if (f.family().trimmed().isEmpty())
            f.setFamily(QStringLiteral("Arial"));
        f.setPointSize(pt);
        target->setFont(f);
        g->replot();
        emit g->modifiedGraph();
        d_target_ml->pushUndoSnapshot(tr("Change Legend Font Size"), before, kMergeLegendFontPt);
    });

    d_leg_sym_bar = createMatSciRichTextBar(page, d_edit_leg_header);

    legGrid->addWidget(d_lbl_leg_frame, 0, 0);
    legGrid->addWidget(d_combo_leg_frame, 0, 1);
    legGrid->addWidget(d_lbl_leg_header, 1, 0);
    legGrid->addWidget(d_edit_leg_header, 1, 1);
    legGrid->addWidget(d_leg_sym_bar, 2, 0, 1, 2);
    legGrid->addWidget(d_lbl_leg_cols, 3, 0);
    legGrid->addWidget(d_spin_leg_cols, 3, 1);
    legGrid->addWidget(d_lbl_leg_font_pt, 4, 0);
    legGrid->addWidget(d_spin_leg_font_pt, 4, 1);
    lay->addLayout(legGrid);

    lay->addStretch(1);
    return page;
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
        QPen sp(baseCol, qBound(1, linePen.width(), 3), linePen.style());
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
    return QColorDialog::getColor(initial.isValid() ? initial : QColor(Qt::black), parent, title,
                                  QColorDialog::ShowAlphaChannel);
}

void GraphInspectorDock::refreshAll()
{
    if (d_coalesce_refresh_timer && d_coalesce_refresh_timer->isActive())
        d_coalesce_refresh_timer->stop();

    const bool hasGraph = !d_target_ml.isNull();
    if (d_header_target_lbl) {
        if (!hasGraph) {
            d_header_target_lbl->setText(tr("(No Active Graph)"));
            d_header_target_lbl->setToolTip(QString());
        } else {
            const QString winLbl = d_target_ml->windowLabel().trimmed();
            const QString winName = d_target_ml->name();
            const bool hasExtraLbl = !winLbl.isEmpty() && winLbl != winName && winLbl.length() <= 48
                    && !winLbl.contains(QLatin1Char('/')) && !winLbl.contains(QLatin1Char('\\'));
            d_header_target_lbl->setText(winName);
            d_header_target_lbl->setToolTip(
                    hasExtraLbl ? QStringLiteral("%1 — %2").arg(winName, winLbl) : winName);
        }
    }
    if (d_scope_strip) {
        const bool multiPanel = hasGraph && d_target_ml->layers() > 1;
        d_scope_strip->setVisible(multiPanel);
        if (d_btn_scope_active && d_btn_scope_all) {
            QSignalBlocker b1(d_btn_scope_active);
            QSignalBlocker b2(d_btn_scope_all);
            d_btn_scope_active->setChecked(!d_scope_all_panels);
            d_btn_scope_all->setChecked(d_scope_all_panels);
        }
    }
    if (d_prop_stack)
        d_prop_stack->setEnabled(hasGraph);
    if (d_tree_header_strip)
        d_tree_header_strip->setEnabled(hasGraph);

    refreshPaletteSchemeCombo();
    refreshObjectTree();
    syncInspectorFromSelection();
}

void GraphInspectorDock::refreshObjectTree()
{
    if (!d_object_tree)
        return;

    if (d_scope_strip)
        d_scope_strip->setVisible(d_target_ml && d_target_ml->layers() > 1);

    d_syncing_tree = true;
    QSignalBlocker blocker(d_object_tree);
    d_object_tree->clear();

    if (!d_target_ml) {
        d_object_tree->hide();
        if (d_empty_hint_lbl)
            d_empty_hint_lbl->show();
        d_syncing_tree = false;
        return;
    }

    if (d_empty_hint_lbl)
        d_empty_hint_lbl->hide();
    d_object_tree->show();

    Graph *activeG = activeLayer();
    const Graph::GraphSelection activeSel =
            activeG ? activeG->currentSelection() : Graph::GraphSelection();
    const int activeMasterCurveIdx =
            (activeG && (activeSel.kind == Graph::GraphSelection::Curve
                         || activeSel.kind == Graph::GraphSelection::ErrorBars))
            ? resolveMasterCurveIndex(activeG, activeSel.primaryId)
            : -1;

    QTreeWidgetItem *pageItem = new QTreeWidgetItem(d_object_tree);
    pageItem->setText(0, tr("Page: %1").arg(d_target_ml->name()));
    pageItem->setIcon(0, IconLoader::duotoneIcon(QStringLiteral("page-setup"), 14));
    pageItem->setData(0, RoleSelKind, int(Graph::GraphSelection::Page));
    pageItem->setData(0, RoleLayerIndex, 0);
    pageItem->setExpanded(true);

    QTreeWidgetItem *itemToSelect = nullptr;
    if (activeSel.kind == Graph::GraphSelection::Page)
        itemToSelect = pageItem;

    const QList<Graph *> layers = d_target_ml->layersList();
    for (int li = 0; li < layers.size(); ++li) {
        Graph *g = layers[li];
        if (!g)
            continue;
        bindLayerSignals(g);

        QString layerTitle = tr("Layer %1").arg(li + 1);
        if (g->linkedParentLayer())
            layerTitle += QStringLiteral(" ") + tr("[Linked]");
        QTreeWidgetItem *layerItem = new QTreeWidgetItem(pageItem);
        layerItem->setText(0, layerTitle);
        layerItem->setIcon(0, IconLoader::duotoneIcon(QStringLiteral("layers"), 14));
        layerItem->setData(0, RoleSelKind, int(Graph::GraphSelection::Layer));
        layerItem->setData(0, RoleLayerIndex, li + 1);
        layerItem->setExpanded(true);
        if (g == activeG) {
            QFont bf = layerItem->font(0);
            bf.setBold(true);
            layerItem->setFont(0, bf);
            if (!itemToSelect
                && (activeSel.kind == Graph::GraphSelection::None
                    || activeSel.kind == Graph::GraphSelection::Layer)) {
                itemToSelect = layerItem;
            }
        }

        // Curves Group
        int dataCurveCount = 0;
        for (int c = 0; c < g->curves(); ++c) {
            if (g->curveType(c) != Graph::ErrorBars)
                ++dataCurveCount;
        }
        QTreeWidgetItem *curvesGroup = new QTreeWidgetItem(layerItem);
        curvesGroup->setText(0, tr("Curves (%1)").arg(dataCurveCount));
        curvesGroup->setData(0, RoleSelKind, int(Graph::GraphSelection::Curve));
        curvesGroup->setData(0, RolePrimaryId, 0);
        curvesGroup->setData(0, RoleLayerIndex, li + 1);
        curvesGroup->setData(0, RoleIsGroupHeader, true);
        curvesGroup->setExpanded(true);

        for (int ci = 0; ci < g->curves(); ++ci) {
            if (auto *sp = dynamic_cast<Spectrogram *>(g->plotItem(ci))) {
                QTreeWidgetItem *spItem = new QTreeWidgetItem(curvesGroup);
                const QString matName = sp->matrix() ? QString(sp->matrix()->name())
                                                     : sp->title().text().trimmed();
                spItem->setText(0, matName.isEmpty() ? tr("Heatmap")
                                                     : tr("Heatmap: %1").arg(matName));
                spItem->setIcon(0, buildCurveSwatchIcon(g, ci));
                spItem->setFlags(spItem->flags() | Qt::ItemIsUserCheckable);
                spItem->setCheckState(0, sp->isVisible() ? Qt::Checked : Qt::Unchecked);
                spItem->setData(0, RoleSelKind, int(Graph::GraphSelection::Spectrogram));
                spItem->setData(0, RolePrimaryId, ci);
                spItem->setData(0, RoleLayerIndex, li + 1);
                if (g == activeG && activeSel.kind == Graph::GraphSelection::Spectrogram
                    && activeSel.primaryId == ci) {
                    itemToSelect = spItem;
                }
                continue;
            }
            if (g->curveType(ci) == Graph::ErrorBars)
                continue;
            QTreeWidgetItem *cvItem = new QTreeWidgetItem(curvesGroup);
            QString cvLabel = friendlyCurveDisplayName(g, ci);
            const PlotCurve *pc = dynamic_cast<const PlotCurve *>(g->plotItem(ci));
            if (g->curveType(ci) == Graph::Function || (pc && pc->type() == Graph::Function)
                || dynamic_cast<const FunctionCurve *>(g->plotItem(ci)) != nullptr)
                cvLabel += tr("  [fx]");
            cvItem->setText(0, cvLabel);
            cvItem->setIcon(0, buildCurveSwatchIcon(g, ci));
            cvItem->setFlags(cvItem->flags() | Qt::ItemIsUserCheckable);
            cvItem->setCheckState(0, g->isCurveVisible(ci) ? Qt::Checked : Qt::Unchecked);
            cvItem->setData(0, RoleSelKind, int(Graph::GraphSelection::Curve));
            cvItem->setData(0, RolePrimaryId, ci);
            cvItem->setData(0, RoleLayerIndex, li + 1);
            if (g == activeG && activeSel.kind == Graph::GraphSelection::Curve
                && ci == activeMasterCurveIdx) {
                itemToSelect = cvItem;
            }

            // Show attached QwtErrorPlotCurve items under their parent curve
            if (auto *dc = dynamic_cast<DataCurve *>(g->curve(ci))) {
                for (DataCurve *eb : dc->errorBarsList()) {
                    auto *er = dynamic_cast<QwtErrorPlotCurve *>(eb);
                    const int ebIdx = er ? g->curveIndex(er) : -1;
                    if (!er || ebIdx < 0)
                        continue;
                    QTreeWidgetItem *ebItem = new QTreeWidgetItem(cvItem);
                    QString colShort = er->yColumnName().trimmed();
                    if (er->table()) {
                        const QString pfx = QString(er->table()->name()) + QStringLiteral("_");
                        if (colShort.startsWith(pfx) && colShort.size() > pfx.size())
                            colShort = colShort.mid(pfx.size());
                    }
                    if (colShort.isEmpty())
                        colShort = er->title().text();
                    ebItem->setText(0, (er->direction() == QwtErrorPlotCurve::Horizontal
                                                ? tr("±X Err: %1")
                                                : tr("±Y Err: %1"))
                                               .arg(colShort));
                    ebItem->setIcon(0, buildCurveSwatchIcon(g, ebIdx));
                    ebItem->setFlags(ebItem->flags() | Qt::ItemIsUserCheckable);
                    ebItem->setCheckState(0, g->isCurveVisible(ebIdx) ? Qt::Checked
                                                                      : Qt::Unchecked);
                    ebItem->setData(0, RoleSelKind, int(Graph::GraphSelection::ErrorBars));
                    ebItem->setData(0, RolePrimaryId, ebIdx);
                    ebItem->setData(0, RoleLayerIndex, li + 1);
                    cvItem->setExpanded(true);
                    if (g == activeG && activeSel.kind == Graph::GraphSelection::ErrorBars
                        && activeSel.primaryId == ebIdx) {
                        itemToSelect = ebItem;
                    }
                }
            }
        }

        // Legend Item
        QTreeWidgetItem *legItem = new QTreeWidgetItem(layerItem);
        legItem->setText(0, tr("Legend"));
        legItem->setIcon(0, IconLoader::duotoneIcon(QStringLiteral("legend"), 14));
        legItem->setFlags(legItem->flags() | Qt::ItemIsUserCheckable);
        const bool legVis = g->hasLegend() && g->isLegendVisible();
        legItem->setCheckState(0, legVis ? Qt::Checked : Qt::Unchecked);
        legItem->setData(0, RoleSelKind, int(Graph::GraphSelection::LegendBox));
        legItem->setData(0, RoleLayerIndex, li + 1);
        if (g == activeG && activeSel.kind == Graph::GraphSelection::LegendBox)
            itemToSelect = legItem;

        // Annotations (Text, Arrow, Image)
        const Legend *mainLegend = g->legend();
        QVector<int> textIds;
        for (int tid : g->textMarkerKeys()) {
            Legend *tm = g->textMarker(tid);
            if (tm && tm != mainLegend)
                textIds.append(tid);
        }
        const QVector<int> arrowIds = g->lineMarkerKeys();
        const QVector<int> imageIds = g->imageMarkerKeys();
        const int totalAnn = textIds.size() + arrowIds.size() + imageIds.size();

        if (totalAnn > 0) {
            QTreeWidgetItem *annGroup = new QTreeWidgetItem(layerItem);
            annGroup->setText(0, tr("Annotations (%1)").arg(totalAnn));
            annGroup->setData(0, RoleSelKind, int(Graph::GraphSelection::LegendBox));
            annGroup->setData(0, RoleLayerIndex, li + 1);
            annGroup->setData(0, RoleIsGroupHeader, true);
            annGroup->setExpanded(true);

            for (int tid : textIds) {
                Legend *tm = g->textMarker(tid);
                QString snippet = tm ? tm->text().simplified() : QString();
                if (snippet.size() > 22)
                    snippet = snippet.left(20) + QStringLiteral("...");
                QTreeWidgetItem *tItem = new QTreeWidgetItem(annGroup);
                tItem->setText(0, snippet.isEmpty() ? tr("Text")
                                                    : tr("Text: %1").arg(snippet));
                tItem->setData(0, RoleSelKind, int(Graph::GraphSelection::TextAnnotation));
                tItem->setData(0, RolePrimaryId, tid);
                tItem->setData(0, RoleLayerIndex, li + 1);
                if (g == activeG && activeSel.kind == Graph::GraphSelection::TextAnnotation
                    && activeSel.primaryId == tid) {
                    itemToSelect = tItem;
                }
            }
            for (int k = 0; k < arrowIds.size(); ++k) {
                const int aid = arrowIds[k];
                QTreeWidgetItem *aItem = new QTreeWidgetItem(annGroup);
                aItem->setText(0, tr("Arrow %1").arg(k + 1));
                aItem->setData(0, RoleSelKind, int(Graph::GraphSelection::ArrowAnnotation));
                aItem->setData(0, RolePrimaryId, aid);
                aItem->setData(0, RoleLayerIndex, li + 1);
                if (g == activeG && activeSel.kind == Graph::GraphSelection::ArrowAnnotation
                    && activeSel.primaryId == aid) {
                    itemToSelect = aItem;
                }
            }
            for (int k = 0; k < imageIds.size(); ++k) {
                const int iid = imageIds[k];
                ImageMarker *im = g->imageMarker(iid);
                const QString base = im ? QFileInfo(im->fileName()).fileName() : QString();
                QTreeWidgetItem *iItem = new QTreeWidgetItem(annGroup);
                iItem->setText(0, base.isEmpty() ? tr("Image %1").arg(k + 1)
                                                 : tr("Image: %1").arg(base));
                iItem->setData(0, RoleSelKind, int(Graph::GraphSelection::ImageAnnotation));
                iItem->setData(0, RolePrimaryId, iid);
                iItem->setData(0, RoleLayerIndex, li + 1);
                if (g == activeG && activeSel.kind == Graph::GraphSelection::ImageAnnotation
                    && activeSel.primaryId == iid) {
                    itemToSelect = iItem;
                }
            }
        }
    }

    if (itemToSelect)
        d_object_tree->setCurrentItem(itemToSelect);

    d_syncing_tree = false;
}

void GraphInspectorDock::onTreeItemClicked(QTreeWidgetItem *item, int)
{
    if (d_syncing_tree || !item || !d_target_ml)
        return;

    GraphMiniToolbar::instance()->cancelScheduledAndHide();

    const int layerOneBased = item->data(0, RoleLayerIndex).toInt();
    if (layerOneBased >= 1) {
        if (Graph *targetG = d_target_ml->layer(layerOneBased)) {
            if (d_target_ml->activeGraph() != targetG)
                d_target_ml->setActiveGraph(targetG);
        }
    }

    Graph *g = activeLayer();
    const auto kind =
            static_cast<Graph::GraphSelection::Kind>(item->data(0, RoleSelKind).toInt());
    const int primaryId = item->data(0, RolePrimaryId).toInt();
    const bool isGroupHeader = item->data(0, RoleIsGroupHeader).toBool();

    if (g && !isGroupHeader) {
        Graph::GraphSelection sel;
        sel.kind = kind;
        sel.primaryId = primaryId;
        g->setCurrentSelection(sel, false);
    }

    switch (kind) {
    case Graph::GraphSelection::Page:
    case Graph::GraphSelection::Layer:
    case Graph::GraphSelection::PlotTitle:
        switchToInspectorPage(PageTabPageLayer);
        syncPageLayerControls();
        break;
    case Graph::GraphSelection::Axis:
    case Graph::GraphSelection::AxisTitle:
        d_active_axis = qBound(0, primaryId, 3);
        switchToInspectorPage(PageTabAxis);
        syncAxisControls();
        break;
    case Graph::GraphSelection::Curve:
    case Graph::GraphSelection::ErrorBars:
    case Graph::GraphSelection::Spectrogram:
        switchToInspectorPage(PageTabCurve);
        syncCurveControls();
        break;
    case Graph::GraphSelection::LegendBox:
    case Graph::GraphSelection::TextAnnotation:
    case Graph::GraphSelection::ArrowAnnotation:
    case Graph::GraphSelection::ImageAnnotation:
        switchToInspectorPage(PageTabLegend);
        syncLegendControls();
        break;
    default:
        syncInspectorFromSelection();
        break;
    }
}

void GraphInspectorDock::onTreeItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (d_syncing_tree || !item || !d_target_ml || !d_app
        || ApplicationWindow::isNonInteractiveContext(d_app)) {
        return;
    }
    onTreeItemClicked(item, column);

    const auto kind =
            static_cast<Graph::GraphSelection::Kind>(item->data(0, RoleSelKind).toInt());
    const int primaryId = item->data(0, RolePrimaryId).toInt();
    const bool isGroupHeader = item->data(0, RoleIsGroupHeader).toBool();

    switch (kind) {
    case Graph::GraphSelection::Page:
    case Graph::GraphSelection::Layer:
        d_app->showGeneralPlotDialog();
        break;
    case Graph::GraphSelection::Axis:
    case Graph::GraphSelection::AxisTitle:
        d_app->showScaleDialog();
        break;
    case Graph::GraphSelection::Curve:
    case Graph::GraphSelection::ErrorBars:
    case Graph::GraphSelection::Spectrogram:
        if (!isGroupHeader)
            d_app->showPlotDialog(primaryId);
        else
            d_app->showCurvesDialog();
        break;
    default:
        break;
    }
}

void GraphInspectorDock::onObjectTreeContextMenu(const QPoint &pos)
{
    if (!d_object_tree || !d_target_ml || !d_app
        || ApplicationWindow::isNonInteractiveContext(d_app)) {
        return;
    }

    QTreeWidgetItem *item = d_object_tree->itemAt(pos);
    if (!item)
        return;

    d_object_tree->setCurrentItem(item);
    onTreeItemClicked(item, 0);

    Graph *g = activeLayer();
    const auto kind =
            static_cast<Graph::GraphSelection::Kind>(item->data(0, RoleSelKind).toInt());
    const int primaryId = item->data(0, RolePrimaryId).toInt();
    const bool isGroupHeader = item->data(0, RoleIsGroupHeader).toBool();

    QMenu menu(this);
    if (!isGroupHeader && (item->flags() & Qt::ItemIsUserCheckable)) {
        const bool isVis = (item->checkState(0) == Qt::Checked);
        QAction *actVis = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("eye"), 14),
                isVis ? tr("Hide Object") : tr("Show Object"));
        connect(actVis, &QAction::triggered, this, [item, isVis]() {
            item->setCheckState(0, isVis ? Qt::Unchecked : Qt::Checked);
        });
    }

    if (!isGroupHeader
        && (kind == Graph::GraphSelection::Curve || kind == Graph::GraphSelection::ErrorBars
            || kind == Graph::GraphSelection::Spectrogram)) {
        QAction *actEdit = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("settings"), 14),
                tr("Edit Curve Style..."));
        connect(actEdit, &QAction::triggered, this, [this, primaryId]() {
            if (d_app)
                d_app->showPlotDialog(primaryId);
        });
    } else if (kind == Graph::GraphSelection::Page || kind == Graph::GraphSelection::Layer) {
        QAction *actProp = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("settings"), 14),
                tr("Layer / Plot Properties..."));
        connect(actProp, &QAction::triggered, this, [this]() {
            if (d_app)
                d_app->showGeneralPlotDialog();
        });
    }

    QAction *actAdd = menu.addAction(
            IconLoader::duotoneIcon(QStringLiteral("plus"), 14),
            tr("Add Curve(s)..."));
    connect(actAdd, &QAction::triggered, this, &GraphInspectorDock::openAddCurvePopover);

    if (g && (kind == Graph::GraphSelection::LegendBox || kind == Graph::GraphSelection::Layer)) {
        QAction *actAutoLeg = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("legend"), 14),
                tr("Auto-Place Legend"));
        connect(actAutoLeg, &QAction::triggered, this, [this, g]() {
            if (!d_target_ml || !g)
                return;
            const QString before = d_target_ml->captureUndoSnapshot();
            if (!g->hasLegend())
                g->newLegend();
            g->setLegendVisible(true);
            g->autoPlaceLegend();
            g->replot();
            d_target_ml->pushUndoSnapshot(tr("Auto-Place Legend"), before);
            refreshAll();
        });
    }

    if (!isGroupHeader && kind == Graph::GraphSelection::Curve) {
        menu.addSeparator();
        QAction *actUp = menu.addAction(tr("Move Curve Up"));
        connect(actUp, &QAction::triggered, this, [this]() { moveSelectedCurve(-1); });
        QAction *actDown = menu.addAction(tr("Move Curve Down"));
        connect(actDown, &QAction::triggered, this, [this]() { moveSelectedCurve(+1); });
    }

    if (!isGroupHeader
        && (kind == Graph::GraphSelection::Curve || kind == Graph::GraphSelection::ErrorBars
            || kind == Graph::GraphSelection::LegendBox
            || kind == Graph::GraphSelection::TextAnnotation
            || kind == Graph::GraphSelection::ArrowAnnotation
            || kind == Graph::GraphSelection::ImageAnnotation)) {
        menu.addSeparator();
        QAction *actRem = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("delete"), 14),
                tr("Remove Object"));
        connect(actRem, &QAction::triggered, this, &GraphInspectorDock::removeSelectedObject);
    }

    if (!menu.isEmpty())
        menu.exec(d_object_tree->viewport()->mapToGlobal(pos));
}

void GraphInspectorDock::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (d_syncing_tree || column != 0 || !item || !d_target_ml)
        return;

    const int layerOneBased = item->data(0, RoleLayerIndex).toInt();
    Graph *g = (layerOneBased >= 1) ? d_target_ml->layer(layerOneBased) : activeLayer();
    if (!g)
        return;

    const auto kind =
            static_cast<Graph::GraphSelection::Kind>(item->data(0, RoleSelKind).toInt());
    const bool checked = (item->checkState(0) == Qt::Checked);

    if ((kind == Graph::GraphSelection::Curve || kind == Graph::GraphSelection::ErrorBars)
        && !item->data(0, RoleIsGroupHeader).toBool()) {
        const int ci = item->data(0, RolePrimaryId).toInt();
        if (ci >= 0 && ci < g->curves() && g->isCurveVisible(ci) != checked) {
            const QString before = d_target_ml->captureUndoSnapshot();
            g->showCurve(ci, checked);
            d_target_ml->pushUndoSnapshot(kind == Graph::GraphSelection::ErrorBars
                                                  ? tr("Toggle Error Bar Visibility")
                                                  : tr("Toggle Curve Visibility"),
                                          before);
        }
    } else if (kind == Graph::GraphSelection::Spectrogram) {
        const int ci = item->data(0, RolePrimaryId).toInt();
        if (auto *sp = dynamic_cast<Spectrogram *>(g->plotItem(ci))) {
            if (sp->isVisible() != checked) {
                const QString before = d_target_ml->captureUndoSnapshot();
                sp->setVisible(checked);
                g->replot();
                emit g->modifiedGraph();
                d_target_ml->pushUndoSnapshot(tr("Toggle Heatmap Visibility"), before);
            }
        }
    } else if (kind == Graph::GraphSelection::LegendBox) {
        const QString before = d_target_ml->captureUndoSnapshot();
        if (checked && !g->hasLegend())
            g->newLegend();
        g->setLegendVisible(checked);
        d_target_ml->pushUndoSnapshot(tr("Toggle Legend Visibility"), before);
        syncLegendControls();
    }
}

void GraphInspectorDock::onGraphSelectionChanged(MultiLayer *page, Graph *layer,
                                                 const Graph::GraphSelection &sel)
{
    if (page != d_target_ml)
        return;
    if (layer && d_target_ml && d_target_ml->activeGraph() != layer)
        d_target_ml->setActiveGraph(layer);

    if (sel.kind == Graph::GraphSelection::Axis || sel.kind == Graph::GraphSelection::AxisTitle)
        d_active_axis = qBound(0, sel.primaryId, 3);

    refreshObjectTree();
    syncInspectorFromSelection();
}

void GraphInspectorDock::syncInspectorFromSelection()
{
    if (!d_target_ml)
        return;

    Graph *g = activeLayer();
    const Graph::GraphSelection sel = g ? g->currentSelection() : Graph::GraphSelection();

    switch (sel.kind) {
    case Graph::GraphSelection::Axis:
    case Graph::GraphSelection::AxisTitle:
        d_active_axis = qBound(0, sel.primaryId, 3);
        switchToInspectorPage(PageTabAxis);
        break;
    case Graph::GraphSelection::Curve:
    case Graph::GraphSelection::ErrorBars:
    case Graph::GraphSelection::Spectrogram:
        switchToInspectorPage(PageTabCurve);
        break;
    case Graph::GraphSelection::LegendBox:
    case Graph::GraphSelection::TextAnnotation:
    case Graph::GraphSelection::ArrowAnnotation:
    case Graph::GraphSelection::ImageAnnotation:
        switchToInspectorPage(PageTabLegend);
        break;
    case Graph::GraphSelection::Page:
    case Graph::GraphSelection::Layer:
    case Graph::GraphSelection::PlotTitle:
        switchToInspectorPage(PageTabPageLayer);
        break;
    case Graph::GraphSelection::None:
        break;
    }

    syncPageLayerControls();
    syncAxisControls();
    syncCurveControls();
    syncLegendControls();
}

void GraphInspectorDock::openAddCurvePopover()
{
    if (!d_app || !d_target_ml)
        return;
    Graph *g = activeLayer();
    if (!g)
        return;

    QList<MyWidget *> *tblList = d_app->tableList();
    QVector<QPointer<Table>> tables;
    if (tblList) {
        for (MyWidget *w : *tblList) {
            if (!w || w->status() == MyWidget::Hidden || d_app->hiddenWindows.contains(w))
                continue;
            if (Table *t = qobject_cast<Table *>(w)) {
                if (t->name().startsWith(QLatin1String("RpcFitCurve")))
                    continue;
                tables.append(QPointer<Table>(t));
            }
        }
    }
    if (tables.isEmpty())
        return;

    QMenu menu(this);
    QWidget *panel = new QWidget(&menu);
    panel->setMinimumWidth(292);
    QVBoxLayout *v = new QVBoxLayout(panel);
    v->setContentsMargins(10, 8, 10, 8);
    v->setSpacing(6);

    QLabel *hdr = new QLabel(tr("Add Curve(s) to Layer"), panel);
    QFont hf = hdr->font();
    hf.setBold(true);
    hdr->setFont(hf);
    v->addWidget(hdr);

    QGridLayout *topGrid = new QGridLayout();
    topGrid->setHorizontalSpacing(6);
    topGrid->setVerticalSpacing(4);

    topGrid->addWidget(new QLabel(tr("Table:"), panel), 0, 0);
    QComboBox *comboTable = new QComboBox(panel);
    int defaultTblIdx = 0;
    if (g->curves() > 0) {
        if (DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(0))) {
            for (int ti = 0; ti < tables.size(); ++ti) {
                if (tables[ti].data() == dc->table()) {
                    defaultTblIdx = ti;
                    break;
                }
            }
        }
    }
    for (const QPointer<Table> &tp : tables)
        comboTable->addItem(tp ? tp->name() : QString());
    comboTable->setCurrentIndex(defaultTblIdx);
    topGrid->addWidget(comboTable, 0, 1);

    topGrid->addWidget(new QLabel(tr("X Column:"), panel), 1, 0);
    QComboBox *comboXCol = new QComboBox(panel);
    topGrid->addWidget(comboXCol, 1, 1);

    topGrid->addWidget(new QLabel(tr("Plot Style:"), panel), 2, 0);
    QComboBox *comboStyle = new QComboBox(panel);
    comboStyle->addItem(tr("Line + Symbol"), int(Graph::LineSymbols));
    comboStyle->addItem(tr("Line"), int(Graph::Line));
    comboStyle->addItem(tr("Scatter"), int(Graph::Scatter));
    comboStyle->addItem(tr("Vertical Bars"), int(Graph::VerticalBars));
    comboStyle->addItem(tr("Horizontal Bars"), int(Graph::HorizontalBars));
    comboStyle->addItem(tr("Area"), int(Graph::Area));
    topGrid->addWidget(comboStyle, 2, 1);
    v->addLayout(topGrid);

    v->addWidget(new QLabel(tr("Y Columns (multi-select):"), panel));
    QListWidget *yChecklist = new QListWidget(panel);
    yChecklist->setMinimumHeight(130);
    yChecklist->setMaximumHeight(190);
    v->addWidget(yChecklist);

    QCheckBox *chkRightY = new QCheckBox(tr("Assign to Right-Y Axis"), panel);
    v->addWidget(chkRightY);

    QPushButton *btnAddNow = new QPushButton(tr("Add Selected Curve(s)"), panel);
    v->addWidget(btnAddNow);

    auto populateColumnsForTable = [&](int tblIdx) {
        comboXCol->clear();
        yChecklist->clear();
        if (tblIdx < 0 || tblIdx >= tables.size())
            return;
        Table *t = tables[tblIdx].data();
        if (!t)
            return;
        comboXCol->addItem(tr("(Default Table X)"), QString());
        for (int c = 0; c < t->numCols(); ++c) {
            const QString colShort = t->colLabel(c);
            const QString fullName = QString(t->name()) + QStringLiteral("_") + colShort;
            const int desig = t->colPlotDesignation(c);
            QString roleTag = QStringLiteral("[Y]");
            if (desig == SciDAVis::X)
                roleTag = QStringLiteral("[X]");
            else if (desig == SciDAVis::Z)
                roleTag = QStringLiteral("[Z]");
            else if (desig == SciDAVis::xErr)
                roleTag = QStringLiteral("[xEr]");
            else if (desig == SciDAVis::yErr)
                roleTag = QStringLiteral("[yEr]");
            else if (desig == SciDAVis::noDesignation)
                roleTag = QStringLiteral("[-]");

            const QString disp = formatColumnLabelWithMeta(t, c, roleTag);
            comboXCol->addItem(disp, fullName);
            comboXCol->setItemData(comboXCol->count() - 1, disp, Qt::ToolTipRole);
            if (desig != SciDAVis::X) {
                QListWidgetItem *it = new QListWidgetItem(disp, yChecklist);
                it->setToolTip(disp);
                it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
                it->setCheckState(Qt::Unchecked);
                it->setData(Qt::UserRole, fullName);
            }
        }
    };

    populateColumnsForTable(defaultTblIdx);
    QObject::connect(comboTable, QOverload<int>::of(&QComboBox::currentIndexChanged), panel,
                     populateColumnsForTable);

    QPointer<MultiLayer> safeMl(d_target_ml);
    QPointer<Graph> safeG(g);
    QObject::connect(btnAddNow, &QPushButton::clicked, &menu, [&]() {
        if (!safeMl || !safeG) {
            menu.close();
            return;
        }
        const int tblIdx = comboTable->currentIndex();
        if (tblIdx < 0 || tblIdx >= tables.size())
            return;
        Table *t = tables[tblIdx].data();
        if (!t) {
            menu.close();
            return;
        }
        const QString xOverride = comboXCol->currentData().toString();
        const int plotStyle = comboStyle->currentData().toInt();
        const bool toRightY = chkRightY->isChecked();

        QStringList selectedYCols;
        for (int i = 0; i < yChecklist->count(); ++i) {
            QListWidgetItem *it = yChecklist->item(i);
            if (it && it->checkState() == Qt::Checked)
                selectedYCols.append(it->data(Qt::UserRole).toString());
        }
        if (selectedYCols.isEmpty())
            return;

        const QString before = safeMl->captureUndoSnapshot();
        int addedCount = 0;
        for (const QString &yFull : selectedYCols) {
            const int prevCount = safeG->curves();
            int dataCurveOrder = 0;
            for (int ci = 0; ci < prevCount; ++ci) {
                if (safeG->curveType(ci) != Graph::ErrorBars)
                    ++dataCurveOrder;
            }
            bool ok = false;
            if (!xOverride.isEmpty())
                ok = safeG->insertCurve(t, xOverride, yFull, plotStyle);
            else
                ok = safeG->insertCurve(t, yFull, plotStyle);
            if (ok && safeG->curves() > prevCount) {
                ++addedCount;
                const int newIdx = safeG->curves() - 1;
                CurveLayout cl = safeG->initCurveLayout(
                        static_cast<Graph::CurveType>(plotStyle), dataCurveOrder);
                if (d_app) {
                    cl.sSize = d_app->defaultSymbolSize;
                    cl.lWidth = d_app->defaultCurveLineWidth;
                }
                safeG->updateCurveLayout(newIdx, &cl);
                if (toRightY) {
                    if (QwtPlotCurve *cv = safeG->curve(newIdx))
                        cv->setYAxis(QwtPlot::yRight);
                }
            }
        }

        if (addedCount > 0) {
            if (toRightY) {
                safeG->enableAxis(QwtPlot::yRight, true);
                safeG->setAxisBackboneVisible(QwtPlot::yRight, true);
                if (Plot *pw = safeG->plotWidget()) {
                    if (QwtScaleDraw *sd = pw->axisScaleDraw(QwtPlot::yRight))
                        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                    pw->setMajorTicksType(QwtPlot::yRight, ScaleDraw::In);
                    pw->setMinorTicksType(QwtPlot::yRight, ScaleDraw::In);
                }
                if (!safeG->isAxisUserLocked(QwtPlot::yRight))
                    safeG->setAutoScaleAxis(QwtPlot::yRight);
            }
            if (!safeG->isAxisUserLocked(QwtPlot::xBottom))
                safeG->setAutoScaleAxis(QwtPlot::xBottom);
            if (!toRightY && !safeG->isAxisUserLocked(QwtPlot::yLeft))
                safeG->setAutoScaleAxis(QwtPlot::yLeft);
            safeG->reconstructLegend();
            safeG->refreshAutoAxisTitlesFromColumns();
            safeG->updatePlot();
            safeMl->pushUndoSnapshot(tr("Add Curve(s)"), before);
        }
        menu.close();
        refreshAll();
    });

    QWidgetAction *wa = new QWidgetAction(&menu);
    wa->setDefaultWidget(panel);
    menu.addAction(wa);
    menu.exec(d_btn_add_curve->mapToGlobal(QPoint(0, d_btn_add_curve->height() + 2)));
}

void GraphInspectorDock::moveSelectedCurve(int delta)
{
    if (!d_target_ml || !d_object_tree || delta == 0)
        return;

    Graph *g = activeLayer();
    int curIdx = -1;
    if (QTreeWidgetItem *cur = d_object_tree->currentItem()) {
        if (!cur->data(0, RoleIsGroupHeader).toBool()) {
            const auto kind =
                    static_cast<Graph::GraphSelection::Kind>(cur->data(0, RoleSelKind).toInt());
            if (kind == Graph::GraphSelection::Curve || kind == Graph::GraphSelection::ErrorBars) {
                const int layerOneBased = cur->data(0, RoleLayerIndex).toInt();
                if (layerOneBased >= 1) {
                    if (Graph *lg = d_target_ml->layer(layerOneBased))
                        g = lg;
                }
                curIdx = resolveMasterCurveIndex(g, cur->data(0, RolePrimaryId).toInt());
            }
        }
    }
    if (curIdx < 0 && g)
        curIdx = effectiveSelectedCurveIndex(g, false);
    if (!g || curIdx < 0 || curIdx >= g->curves() || g->curveType(curIdx) == Graph::ErrorBars)
        return;

    QVector<int> dataIndices;
    for (int i = 0; i < g->curves(); ++i) {
        if (g->curveType(i) != Graph::ErrorBars)
            dataIndices.append(i);
    }
    const int pos = dataIndices.indexOf(curIdx);
    if (pos < 0 || (delta < 0 && pos <= 0) || (delta > 0 && pos >= dataIndices.size() - 1))
        return;

    QwtPlotItem *movedCurvePtr = g->curve(curIdx);
    if (!movedCurvePtr)
        return;

    const int neighborMasterIdx = dataIndices[pos + (delta < 0 ? -1 : 1)];
    int toIdx = neighborMasterIdx;
    if (delta > 0) {
        while (toIdx + 1 < g->curves() && g->curveType(toIdx + 1) == Graph::ErrorBars) {
            auto *er = dynamic_cast<QwtErrorPlotCurve *>(g->curve(toIdx + 1));
            if (er && er->masterCurve() == movedCurvePtr)
                break;
            ++toIdx;
        }
    }

    const QString before = d_target_ml->captureUndoSnapshot();
    if (g->moveCurve(curIdx, toIdx)) {
        const int newActualIdx = g->curveIndex(static_cast<QwtPlotCurve *>(movedCurvePtr));
        Graph::GraphSelection sel;
        sel.kind = Graph::GraphSelection::Curve;
        sel.primaryId = (newActualIdx >= 0) ? newActualIdx : toIdx;
        g->setCurrentSelection(sel, false);
        d_target_ml->pushUndoSnapshot(tr("Reorder Curve"), before);
        refreshObjectTree();
        syncCurveControls();
    }
}

void GraphInspectorDock::removeSelectedObject()
{
    if (!d_target_ml || !d_object_tree)
        return;
    QTreeWidgetItem *cur = d_object_tree->currentItem();
    if (!cur || cur->data(0, RoleIsGroupHeader).toBool())
        return;

    const int layerOneBased = cur->data(0, RoleLayerIndex).toInt();
    Graph *g = (layerOneBased >= 1) ? d_target_ml->layer(layerOneBased) : activeLayer();
    if (!g)
        return;

    const auto kind =
            static_cast<Graph::GraphSelection::Kind>(cur->data(0, RoleSelKind).toInt());
    const int primaryId = cur->data(0, RolePrimaryId).toInt();

    if (kind == Graph::GraphSelection::ErrorBars) {
        if (primaryId >= 0 && primaryId < g->curves()) {
            if (d_app && d_target_ml)
                d_app->invalidateModelessGraphDialogs(d_target_ml);
            const QString before = d_target_ml->captureUndoSnapshot();
            g->removeCurve(primaryId);
            g->updatePlot();
            d_target_ml->pushUndoSnapshot(tr("Remove Error Bars"), before);
            refreshAll();
        }
    } else if (kind == Graph::GraphSelection::Curve
               || kind == Graph::GraphSelection::Spectrogram) {
        const int ci = (kind == Graph::GraphSelection::Spectrogram)
                ? primaryId
                : resolveMasterCurveIndex(g, primaryId);
        if (ci >= 0 && ci < g->curves()) {
            if (d_app && d_target_ml)
                d_app->invalidateModelessGraphDialogs(d_target_ml);
            const QString before = d_target_ml->captureUndoSnapshot();
            g->removeCurve(ci);
            g->reconstructLegend();
            g->updatePlot();
            d_target_ml->pushUndoSnapshot(tr("Remove Curve"), before);
            refreshAll();
        }
    } else if (kind == Graph::GraphSelection::LegendBox) {
        if (g->hasLegend()) {
            const QString before = d_target_ml->captureUndoSnapshot();
            g->removeLegend();
            g->replot();
            d_target_ml->pushUndoSnapshot(tr("Remove Legend"), before);
            refreshAll();
        }
    } else if (kind == Graph::GraphSelection::TextAnnotation
               || kind == Graph::GraphSelection::ArrowAnnotation
               || kind == Graph::GraphSelection::ImageAnnotation) {
        const QString before = d_target_ml->captureUndoSnapshot();
        g->setSelectedMarker(primaryId, false);
        g->removeMarker();
        g->replot();
        d_target_ml->pushUndoSnapshot(tr("Remove Annotation"), before);
        refreshAll();
    } else if (kind == Graph::GraphSelection::Layer && d_target_ml->layers() > 1) {
        if (d_app && d_target_ml)
            d_app->invalidateModelessGraphDialogs(d_target_ml);
        const QString before = d_target_ml->captureUndoSnapshot();
        d_target_ml->setActiveGraph(g);
        d_target_ml->removeLayer();
        d_target_ml->pushUndoSnapshot(tr("Remove Layer"), before);
        refreshAll();
    }
}

void GraphInspectorDock::syncPageLayerControls()
{
    if (!d_target_ml)
        return;
    d_syncing_props = true;

    const double wMm =
            physicalValueToMm(d_target_ml->pageWidthPhysical(), d_target_ml->pageUnit());
    const double hMm =
            physicalValueToMm(d_target_ml->pageHeightPhysical(), d_target_ml->pageUnit());

    {
        QSignalBlocker b1(d_spin_page_w_mm);
        QSignalBlocker b2(d_spin_page_h_mm);
        QSignalBlocker b3(d_spin_page_dpi);
        QSignalBlocker b4(d_chk_page_view);
        QSignalBlocker b5(d_combo_page_zoom);
        QSignalBlocker b6(d_chk_panel_labels);
        QSignalBlocker b7(d_combo_panel_pos);

        d_spin_page_w_mm->setValue(wMm);
        d_spin_page_h_mm->setValue(hMm);
        d_spin_page_dpi->setValue(d_target_ml->pageDpi());
        d_chk_page_view->setChecked(d_target_ml->pageViewMode());

        const int curPct = qRound(d_target_ml->pageZoom() * 100.0);
        int bestIdx = 2;
        int bestDiff = 9999;
        for (int i = 0; i < d_combo_page_zoom->count(); ++i) {
            const int diff = qAbs(d_combo_page_zoom->itemData(i).toInt() - curPct);
            if (diff < bestDiff) {
                bestDiff = diff;
                bestIdx = i;
            }
        }
        d_combo_page_zoom->setCurrentIndex(bestIdx);

        d_chk_panel_labels->setChecked(d_target_ml->hasPanelLabels());
        const int pIdx = d_combo_panel_pos->findData(int(d_target_ml->panelLabelPlacement()));
        if (pIdx >= 0)
            d_combo_panel_pos->setCurrentIndex(pIdx);
    }

    d_syncing_props = false;
}

void GraphInspectorDock::syncAxisControls()
{
    Graph *g = activeLayer();
    if (!g || !g->plotWidget())
        return;

    d_syncing_props = true;
    const int ax = qBound(0, d_active_axis, 3);
    Plot *pw = g->plotWidget();

    if (d_axis_sel_group) {
        if (QAbstractButton *ab = d_axis_sel_group->button(ax)) {
            QSignalBlocker b(ab);
            ab->setChecked(true);
        }
    }

    {
        QSignalBlocker bVis(d_chk_axis_visible);
        QSignalBlocker bTickLbl(d_chk_tick_labels);
        QSignalBlocker bUFmt(d_combo_unit_format);
        QSignalBlocker bMin(d_edit_axis_min);
        QSignalBlocker bMax(d_edit_axis_max);
        QSignalBlocker bStep(d_edit_axis_step);
        QSignalBlocker bScl(d_combo_axis_scale);
        QSignalBlocker bRev(d_chk_axis_reverse);
        QSignalBlocker bMinSub(d_combo_minor_subdiv);
        QSignalBlocker bBrk(d_chk_axis_break);
        QSignalBlocker bBrkF(d_edit_break_from);
        QSignalBlocker bBrkT(d_edit_break_to);
        QSignalBlocker bTDir(d_combo_tick_dir);
        QSignalBlocker bTFmt(d_combo_tick_format);
        QSignalBlocker bTPrec(d_spin_tick_prec);
        QSignalBlocker bTDiv(d_combo_tick_divide);
        QSignalBlocker bCat(d_combo_category_col);
        QSignalBlocker bMajG(d_chk_major_grid);
        QSignalBlocker bMinG(d_chk_minor_grid);
        QSignalBlocker bTit(d_edit_axis_title);
        QSignalBlocker bTitPt(d_spin_axis_title_pt);
        QSignalBlocker bTckPt(d_spin_axis_tick_pt);

        d_chk_axis_visible->setChecked(pw->axisEnabled(ax));
        if (d_chk_tick_labels) {
            d_chk_tick_labels->setChecked(g->hasTickLabelsOnAxis(ax));
            d_chk_tick_labels->setEnabled(pw->axisEnabled(ax));
        }
        if (d_combo_unit_format) {
            int ufMode = (g->axisUnitFormat() == Column::UnitFormat::ISIJSlash) ? 1 : 0;
            if (g->property("_toriBracketUnitFmt").toBool()) {
                ufMode = 2;
            } else {
                const QString curTitle = g->axisTitle(ax).trimmed();
                if (curTitle.endsWith(QLatin1Char(']')) && curTitle.contains(QLatin1String(" [")))
                    ufMode = 2;
            }
            const int ufIdx = d_combo_unit_format->findData(ufMode);
            if (ufIdx >= 0)
                d_combo_unit_format->setCurrentIndex(ufIdx);
        }

        const QwtScaleDiv *scDiv = pw->axisScaleDiv(ax);
        const double lo = scDiv ? qMin(scDiv->lowerBound(), scDiv->upperBound()) : 0.0;
        const double hi = scDiv ? qMax(scDiv->lowerBound(), scDiv->upperBound()) : 1.0;
        d_edit_axis_min->setText(QString::number(lo, 'g', 8));
        d_edit_axis_max->setText(QString::number(hi, 'g', 8));
        const double stp = g->axisStep(ax);
        d_edit_axis_step->setText(stp > 0.0 ? QString::number(stp, 'g', 8) : QStringLiteral("0"));

        bool isLog10 = false;
        bool isInverted = (scDiv && scDiv->lowerBound() > scDiv->upperBound());
        if (const QwtScaleEngine *eng = pw->axisScaleEngine(ax)) {
            isInverted = eng->testAttribute(QwtScaleEngine::Inverted);
            QwtScaleTransformation *xform = eng->transformation();
            isLog10 = (xform && xform->type() == QwtScaleTransformation::Log10);
            delete xform;
        }
        d_combo_axis_scale->setCurrentIndex(isLog10 ? 1 : 0);
        if (d_chk_axis_reverse)
            d_chk_axis_reverse->setChecked(isInverted);

        if (d_combo_minor_subdiv) {
            while (d_combo_minor_subdiv->count() > 4)
                d_combo_minor_subdiv->removeItem(d_combo_minor_subdiv->count() - 1);
            const int curMinor = qMax(0, pw->axisMaxMinor(ax));
            int mIdx = d_combo_minor_subdiv->findData(curMinor);
            if (mIdx < 0) {
                d_combo_minor_subdiv->addItem(
                        tr("%1 (%2 subdiv)").arg(curMinor).arg(curMinor + 1), curMinor);
                mIdx = d_combo_minor_subdiv->count() - 1;
            }
            d_combo_minor_subdiv->setCurrentIndex(mIdx);
        }

        const Graph::AxisBreakSpec brk = g->axisBreak(ax);
        d_chk_axis_break->setChecked(brk.enabled);
        d_edit_break_from->setEnabled(brk.enabled);
        d_edit_break_to->setEnabled(brk.enabled);
        const double span = hi - lo;
        const double defFrom = brk.enabled ? brk.breakFrom : (lo + 0.35 * span);
        const double defTo = brk.enabled ? brk.breakTo : (lo + 0.65 * span);
        d_edit_break_from->setText(QString::number(defFrom, 'g', 6));
        d_edit_break_to->setText(QString::number(defTo, 'g', 6));

        int majStyle = int(ScaleDraw::In);
        if (const ScaleDraw *sd = dynamic_cast<const ScaleDraw *>(pw->axisScaleDraw(ax)))
            majStyle = sd->majorTicksStyle();
        const int tdIdx = d_combo_tick_dir->findData(majStyle);
        if (tdIdx >= 0)
            d_combo_tick_dir->setCurrentIndex(tdIdx);

        if (d_combo_tick_format) {
            const int fmt = pw->axisLabelFormat(ax);
            const int fmtIdx = d_combo_tick_format->findData(fmt);
            if (fmtIdx >= 0)
                d_combo_tick_format->setCurrentIndex(fmtIdx);
        }
        if (d_spin_tick_prec) {
            const int fmt = pw->axisLabelFormat(ax);
            const int prec = pw->axisLabelPrecision(ax);
            if (fmt == Plot::Automatic && (prec == 4 || prec == 6))
                d_spin_tick_prec->setValue(-1);
            else
                d_spin_tick_prec->setValue(qBound(-1, prec, 9));
        }
        if (d_combo_tick_divide) {
            const double div = g->tickDivideFactor(ax);
            const QString divTxt = QString::number(div, 'g', 8);
            const int foundDiv = d_combo_tick_divide->findText(divTxt);
            if (foundDiv >= 0)
                d_combo_tick_divide->setCurrentIndex(foundDiv);
            else
                d_combo_tick_divide->setEditText(divTxt);
        }
        refreshCategoryColumnCombo();

        if (Grid *gr = g->grid()) {
            const bool isX = (ax == QwtPlot::xBottom || ax == QwtPlot::xTop);
            d_chk_major_grid->setChecked(isX ? gr->xEnabled() : gr->yEnabled());
            d_chk_minor_grid->setChecked(isX ? gr->xMinEnabled() : gr->yMinEnabled());
        }

        d_edit_axis_title->setText(g->axisTitle(ax));
        const QFont tf = g->axisTitleFont(ax);
        d_spin_axis_title_pt->setValue(tf.pointSize() > 0 ? tf.pointSize() : 14);
        const QFont nf = g->axisFont(ax);
        d_spin_axis_tick_pt->setValue(nf.pointSize() > 0 ? nf.pointSize() : 12);
    }

    d_syncing_props = false;
}

void GraphInspectorDock::syncCurveControls()
{
    Graph *g = activeLayer();
    if (!g)
        return;

    d_syncing_props = true;

    // 1) Check if a Spectrogram / Heatmap is selected (or sole item on layer)
    if (Spectrogram *sp = effectiveSelectedSpectrogram(g)) {
        if (d_curve_regular_wrap)
            d_curve_regular_wrap->hide();
        if (d_spectrogram_wrap)
            d_spectrogram_wrap->show();

        const QString matName =
                sp->matrix() ? QString(sp->matrix()->name()) : sp->title().text().trimmed();
        d_lbl_active_curve_name->setText(matName.isEmpty() ? tr("Heatmap")
                                                           : tr("Heatmap: %1").arg(matName));

        QSignalBlocker bs1(d_combo_spec_cmap);
        QSignalBlocker bs2(d_chk_spec_reverse);
        QSignalBlocker bs3(d_chk_spec_colorbar);
        QSignalBlocker bsTitle(d_edit_spec_cb_title);
        QSignalBlocker bs4(d_spin_spec_cb_width);
        QSignalBlocker bs5(d_chk_spec_contours);
        QSignalBlocker bs6(d_spin_spec_levels);
        QSignalBlocker bs7(d_chk_spec_custom_z);
        QSignalBlocker bs8(d_edit_spec_zmin);
        QSignalBlocker bs9(d_edit_spec_zmax);

        const int cmIdx = d_combo_spec_cmap->findData(int(sp->colorMapPreset()));
        if (cmIdx >= 0)
            d_combo_spec_cmap->setCurrentIndex(cmIdx);
        d_chk_spec_reverse->setChecked(sp->reversePalette());

        const bool hasCb = sp->hasColorScale();
        d_chk_spec_colorbar->setChecked(hasCb);
        if (d_edit_spec_cb_title) {
            d_edit_spec_cb_title->setEnabled(hasCb);
            d_edit_spec_cb_title->setText(sp->colorBarTitle());
        }
        if (d_spec_cb_sym_bar)
            d_spec_cb_sym_bar->setEnabled(hasCb);
        d_spin_spec_cb_width->setEnabled(hasCb);
        d_spin_spec_cb_width->setValue(qBound(6, sp->colorBarWidth(), 64));

        const bool hasContours = sp->testDisplayMode(QwtPlotSpectrogram::ContourMode);
        d_chk_spec_contours->setChecked(hasContours);
        d_spin_spec_levels->setEnabled(hasContours);
        d_spin_spec_levels->setValue(qBound(2, sp->levels(), 50));

        const bool customZ = sp->hasCustomValueRange();
        d_chk_spec_custom_z->setChecked(customZ);
        d_edit_spec_zmin->setEnabled(customZ);
        d_edit_spec_zmax->setEnabled(customZ);
        const QwtDoubleInterval effRange = sp->effectiveValueRange();
        const double zLo = customZ ? sp->customZMin() : effRange.minValue();
        const double zHi = customZ ? sp->customZMax() : effRange.maxValue();
        d_edit_spec_zmin->setText(QString::number(zLo, 'g', 6));
        d_edit_spec_zmax->setText(QString::number(zHi, 'g', 6));

        d_syncing_props = false;
        return;
    }

    if (d_spectrogram_wrap)
        d_spectrogram_wrap->hide();
    if (d_curve_regular_wrap)
        d_curve_regular_wrap->show();

    const int ci = effectiveSelectedCurveIndex(g, true);
    QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
    const bool hasCurve = (cv != nullptr);

    d_combo_curve_style->setEnabled(hasCurve);
    d_btn_line_color->setEnabled(hasCurve);
    d_spin_line_width->setEnabled(hasCurve);
    d_combo_line_pen->setEnabled(hasCurve);
    if (d_spin_curve_opacity)
        d_spin_curve_opacity->setEnabled(hasCurve);
    d_combo_sym_shape->setEnabled(hasCurve);
    d_spin_sym_size->setEnabled(hasCurve);
    if (d_spin_sym_skip)
        d_spin_sym_skip->setEnabled(hasCurve);
    d_chk_sym_hollow->setEnabled(hasCurve);
    if (d_chk_link_sym_color)
        d_chk_link_sym_color->setEnabled(hasCurve);
    d_chk_curve_right_y->setEnabled(hasCurve);
    if (d_btn_match_right_y_color)
        d_btn_match_right_y_color->setEnabled(hasCurve);
    d_btn_err_adjacent->setEnabled(hasCurve);
    d_btn_err_sd->setEnabled(hasCurve);
    d_btn_err_5pct->setEnabled(hasCurve);

    if (!cv) {
        if (d_err_style_wrap)
            d_err_style_wrap->hide();
        d_lbl_active_curve_name->setText(tr("(No Curve Selected)"));
        d_syncing_props = false;
        return;
    }

    d_lbl_active_curve_name->setText(friendlyCurveDisplayName(g, ci));

    {
        QSignalBlocker b1(d_combo_curve_style);
        QSignalBlocker b2(d_spin_line_width);
        QSignalBlocker b3(d_combo_line_pen);
        QSignalBlocker bOp(d_spin_curve_opacity);
        QSignalBlocker b4(d_combo_sym_shape);
        QSignalBlocker b5(d_spin_sym_size);
        QSignalBlocker b6(d_chk_sym_hollow);
        QSignalBlocker bLink(d_chk_link_sym_color);
        QSignalBlocker bEdgeW(d_spin_sym_edge_width);
        QSignalBlocker b7(d_chk_curve_right_y);

        while (d_combo_curve_style->count() > 10)
            d_combo_curve_style->removeItem(10);
        const int cType = g->curveType(ci);
        const int ctIdx = d_combo_curve_style->findData(cType);
        const bool canConvertStandard =
                (ctIdx >= 0 && !g->isPolarPlot() && !g->isTernaryPlot()
                 && (cType == Graph::Line || Graph::canConvertTo(cv, Graph::Line)));
        if (canConvertStandard) {
            d_combo_curve_style->setEnabled(true);
            d_combo_curve_style->setCurrentIndex(ctIdx);
        } else {
            QString specLabel;
            if (g->isPolarPlot())
                specLabel = tr("Polar");
            else if (g->isTernaryPlot())
                specLabel = tr("Ternary Plot");
            else if (cType == Graph::Histogram)
                specLabel = tr("Histogram");
            else if (cType == Graph::Box)
                specLabel = tr("Box Plot");
            else if (cType == Graph::Pie)
                specLabel = tr("Pie");
            else if (cType == Graph::VectXYXY || cType == Graph::VectXYAM)
                specLabel = tr("Vectors");
            else if (cType == Graph::ErrorBars)
                specLabel = tr("Error Bars");
            else
                specLabel = tr("Function");
            d_combo_curve_style->addItem(specLabel, cType);
            d_combo_curve_style->setCurrentIndex(10);
            d_combo_curve_style->setEnabled(false);
        }

        const QPen lp = cv->pen();
        const QwtSymbol sym = cv->symbol();
        const bool hasLine = (cv->style() != QwtPlotCurve::NoCurve && lp.style() != Qt::NoPen);
        const QColor col = (hasLine && lp.color().isValid()) ? lp.color() : sym.pen().color();
        updateColorSwatchButton(d_btn_line_color, col);

        d_spin_line_width->setValue(qMax(1, lp.width()));
        const int penIdx = d_combo_line_pen->findData(int(lp.style()));
        if (penIdx >= 0)
            d_combo_line_pen->setCurrentIndex(penIdx);

        if (d_spin_curve_opacity) {
            const int alpha = col.isValid() ? col.alpha() : 255;
            d_spin_curve_opacity->setValue(qBound(0, qRound(alpha * 100.0 / 255.0), 100));
        }

        const int symIdx = d_combo_sym_shape->findData(int(sym.style()));
        if (symIdx >= 0)
            d_combo_sym_shape->setCurrentIndex(symIdx);
        d_spin_sym_size->setValue(sym.size().width() > 0 ? sym.size().width() : 8);
        if (d_spin_sym_skip) {
            QSignalBlocker bSkip(d_spin_sym_skip);
            const PlotCurve *pc = dynamic_cast<const PlotCurve *>(cv);
            d_spin_sym_skip->setValue(pc ? qBound(0, pc->symbolSkip(), 500) : 0);
        }
        const bool isHollow = Graph::isSymbolWhiteHollow(sym);
        d_chk_sym_hollow->setChecked(isHollow);

        if (d_chk_link_sym_color) {
            const quintptr curPtr = reinterpret_cast<quintptr>(cv);
            const quintptr prevPtr = d_chk_link_sym_color->property("boundCurvePtr").value<quintptr>();
            const bool edgeDiffers = (sym.style() != QwtSymbol::NoSymbol && hasLine
                                      && lp.color().isValid() && sym.pen().color().isValid()
                                      && sym.pen().color().rgb() != lp.color().rgb());
            const bool fillDiffers = (sym.style() != QwtSymbol::NoSymbol && !isHollow
                                      && sym.brush().style() != Qt::NoBrush
                                      && sym.brush().color().isValid() && sym.pen().color().isValid()
                                      && sym.brush().color().rgb() != sym.pen().color().rgb());
            bool linked = true;
            if (edgeDiffers || fillDiffers)
                linked = false;
            else if (curPtr == prevPtr)
                linked = d_chk_link_sym_color->isChecked();
            d_chk_link_sym_color->setChecked(linked);
            d_chk_link_sym_color->setProperty("boundCurvePtr", QVariant::fromValue<quintptr>(curPtr));
            if (d_sym_custom_colors_wrap)
                d_sym_custom_colors_wrap->setVisible(!linked);
        }

        updateColorSwatchButton(d_btn_sym_edge_color,
                                sym.pen().color().isValid() ? sym.pen().color() : col);
        updateColorSwatchButton(
                d_btn_sym_fill_color,
                (sym.brush().style() != Qt::NoBrush && sym.brush().color().isValid())
                        ? sym.brush().color()
                        : QColor(Qt::white));
        if (d_spin_sym_edge_width) {
            const double ew = sym.pen().widthF() > 0.0 ? sym.pen().widthF()
                                                       : double(qMax(1, sym.pen().width()));
            d_spin_sym_edge_width->setValue(qBound(0.5, ew, 4.0));
        }

        d_chk_curve_right_y->setChecked(cv->yAxis() == QwtPlot::yRight);
    }

    // 2) Sync attached or selected ErrorBar controls
    const QVector<QwtErrorPlotCurve *> errs = effectiveSelectedErrorBars(g);
    if (d_err_style_wrap) {
        d_err_style_wrap->setVisible(!errs.isEmpty());
        if (!errs.isEmpty()) {
            QwtErrorPlotCurve *er = errs.first();
            QSignalBlocker be1(d_spin_err_width);
            QSignalBlocker be2(d_spin_err_cap);
            QSignalBlocker be3(d_combo_err_dir);
            d_spin_err_width->setValue(qBound(1, er->width(), 10));
            d_spin_err_cap->setValue(qBound(0, er->capLength(), 32));
            int dirMode = 0;
            if (er->plusSide() && !er->minusSide())
                dirMode = 1;
            else if (!er->plusSide() && er->minusSide())
                dirMode = 2;
            const int dIdx = d_combo_err_dir->findData(dirMode);
            if (dIdx >= 0)
                d_combo_err_dir->setCurrentIndex(dIdx);
            updateColorSwatchButton(d_btn_err_color, er->color());
        }
    }

    d_syncing_props = false;
}

void GraphInspectorDock::syncLegendControls()
{
    Graph *g = activeLayer();
    if (!g)
        return;

    d_syncing_props = true;

    const auto selKind = g->currentSelection().kind;
    Legend *leg = nullptr;
    if (selKind == Graph::GraphSelection::TextAnnotation)
        leg = g->textMarker(g->currentSelection().primaryId);
    else if (selKind != Graph::GraphSelection::ArrowAnnotation
             && selKind != Graph::GraphSelection::ImageAnnotation)
        leg = g->legend();

    const bool isPrimaryLegend = (g && leg && leg == g->legend())
            || (g && !leg && selKind == Graph::GraphSelection::LegendBox);
    const bool isTextAnn = (selKind == Graph::GraphSelection::TextAnnotation);
    const bool canEditMarker = isPrimaryLegend || (isTextAnn && leg != nullptr);

    d_lbl_legend_target->setText(isPrimaryLegend ? tr("Graph Legend") : tr("Text Annotation"));
    d_lbl_leg_header->setText(isPrimaryLegend ? tr("Header:") : tr("Text:"));

    if (d_btn_leg_tl)
        d_btn_leg_tl->setEnabled(isPrimaryLegend);
    if (d_btn_leg_tc)
        d_btn_leg_tc->setEnabled(isPrimaryLegend);
    if (d_btn_leg_tr)
        d_btn_leg_tr->setEnabled(isPrimaryLegend);
    if (d_btn_leg_bl)
        d_btn_leg_bl->setEnabled(isPrimaryLegend);
    if (d_btn_leg_br)
        d_btn_leg_br->setEnabled(isPrimaryLegend);
    if (d_btn_leg_out_r)
        d_btn_leg_out_r->setEnabled(isPrimaryLegend);
    if (d_btn_leg_auto_avoid)
        d_btn_leg_auto_avoid->setEnabled(isPrimaryLegend);
    if (d_btn_leg_rebuild)
        d_btn_leg_rebuild->setEnabled(isPrimaryLegend);
    if (d_spin_leg_cols)
        d_spin_leg_cols->setEnabled(isPrimaryLegend);
    if (d_combo_leg_frame)
        d_combo_leg_frame->setEnabled(canEditMarker);
    if (d_edit_leg_header)
        d_edit_leg_header->setEnabled(canEditMarker);
    if (d_leg_sym_bar)
        d_leg_sym_bar->setEnabled(canEditMarker);
    if (d_spin_leg_font_pt)
        d_spin_leg_font_pt->setEnabled(canEditMarker);

    if (leg) {
        QSignalBlocker b1(d_combo_leg_frame);
        QSignalBlocker b2(d_edit_leg_header);
        QSignalBlocker b3(d_spin_leg_cols);
        QSignalBlocker b4(d_spin_leg_font_pt);

        int frameMode = 0;
        if (leg->frameStyle() != Legend::None) {
            frameMode = (leg->backgroundColor().alpha() > 10) ? 1 : 2;
        }
        const int fmIdx = d_combo_leg_frame->findData(frameMode);
        if (fmIdx >= 0)
            d_combo_leg_frame->setCurrentIndex(fmIdx);

        if (!isPrimaryLegend) {
            const QStringList lines = leg->text().split('\n');
            d_edit_leg_header->setText(lines.isEmpty() ? QString() : lines.first());
            d_edit_leg_header->setModified(false);
        } else {
            const QStringList lines = leg->text().split('\n');
            if (!lines.isEmpty() && !lines.first().trimmed().isEmpty()
                && !isLegendEntryLine(lines.first())) {
                d_edit_leg_header->setText(lines.first().trimmed());
            } else {
                d_edit_leg_header->clear();
            }
            d_edit_leg_header->setModified(false);
            d_spin_leg_cols->setValue(qBound(1, leg->numColumns(), 4));
        }

        const QFont lf = leg->font();
        d_spin_leg_font_pt->setValue(lf.pointSize() > 0 ? lf.pointSize() : 12);
    } else {
        QSignalBlocker b(d_edit_leg_header);
        d_edit_leg_header->clear();
        d_edit_leg_header->setModified(false);
    }

    d_syncing_props = false;
}

void GraphInspectorDock::applyAxisScaleFromUi(int mergeId)
{
    Graph *g = activeLayer();
    const QVector<Graph *> targets = targetLayersForScope();
    if (d_syncing_props || !d_target_ml || !g || !g->plotWidget() || targets.isEmpty())
        return;

    bool okMin = false, okMax = false, okStep = false;
    double lo = d_edit_axis_min->text().trimmed().toDouble(&okMin);
    double hi = d_edit_axis_max->text().trimmed().toDouble(&okMax);
    double stp = d_edit_axis_step->text().trimmed().toDouble(&okStep);
    if (!okMin || !okMax || !std::isfinite(lo) || !std::isfinite(hi))
        return;
    if (!okStep || !std::isfinite(stp) || stp < 0.0)
        stp = 0.0;

    const int scaleType = d_combo_axis_scale->currentData().toInt(); // 0=Linear, 1=Log10
    if (scaleType == 1) {
        if (lo <= 0.0 && hi <= 0.0) {
            lo = 0.1;
            hi = 100.0;
        } else if (lo <= 0.0) {
            lo = qMax(1e-6, hi * 1e-4);
        }
    }
    if (qAbs(hi - lo) < 1e-30)
        hi = lo + 1.0;

    Plot *pw = g->plotWidget();
    const QwtScaleEngine *eng = pw->axisScaleEngine(d_active_axis);
    const bool inv = d_chk_axis_reverse
            ? d_chk_axis_reverse->isChecked()
            : (eng ? eng->testAttribute(QwtScaleEngine::Inverted) : false);
    const int minorSubdiv =
            (d_combo_minor_subdiv && d_combo_minor_subdiv->currentIndex() >= 0)
            ? d_combo_minor_subdiv->currentData().toInt()
            : pw->axisMaxMinor(d_active_axis);

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg || !lg->plotWidget())
            continue;
        Plot *lpw = lg->plotWidget();
        lg->enableAutoscaling(false);
        lg->setScale(d_active_axis, lo, hi, stp, lpw->axisMaxMajor(d_active_axis), minorSubdiv,
                     scaleType, inv);
        lg->setAxisUserLocked(d_active_axis, true);
        syncLinkedChildrenOf(d_target_ml, lg);
        lg->replot();
    }
    d_target_ml->pushUndoSnapshot(tr("Modify Axis Scale"), before, mergeId);
    syncAxisControls();
}

void GraphInspectorDock::applyAxisBreakFromUi()
{
    Graph *g = activeLayer();
    const QVector<Graph *> targets = targetLayersForScope();
    if (d_syncing_props || !d_target_ml || !g || targets.isEmpty())
        return;

    const bool enabled = d_chk_axis_break->isChecked();
    const QString before = d_target_ml->captureUndoSnapshot();
    if (!enabled) {
        for (Graph *lg : targets) {
            if (lg)
                lg->clearAxisBreak(d_active_axis);
        }
        d_target_ml->pushUndoSnapshot(tr("Clear Axis Break"), before, kMergeAxisBreak);
        syncAxisControls();
        return;
    }

    bool okFrom = false, okTo = false;
    const double bf = d_edit_break_from->text().trimmed().toDouble(&okFrom);
    const double bt = d_edit_break_to->text().trimmed().toDouble(&okTo);
    if (!okFrom || !okTo || !std::isfinite(bf) || !std::isfinite(bt) || qAbs(bt - bf) < 1e-12)
        return;

    for (Graph *lg : targets) {
        if (!lg)
            continue;
        Graph::AxisBreakSpec spec = lg->axisBreak(d_active_axis);
        spec.enabled = true;
        spec.breakFrom = bf;
        spec.breakTo = bt;
        if (spec.positionPct < 0.10 || spec.positionPct > 0.90)
            spec.positionPct = 0.50;
        lg->setAxisBreak(d_active_axis, spec);
    }
    d_target_ml->pushUndoSnapshot(tr("Set Axis Break"), before, kMergeAxisBreak);
    syncAxisControls();
}

void GraphInspectorDock::applyTickFormatFromUi(int mergeId)
{
    Graph *g = activeLayer();
    const QVector<Graph *> targets = targetLayersForScope();
    if (d_syncing_props || !d_target_ml || !g || !g->plotWidget() || targets.isEmpty())
        return;

    const int rawFmt = d_combo_tick_format ? d_combo_tick_format->currentData().toInt() : 0;
    const int uiPrec = d_spin_tick_prec ? d_spin_tick_prec->value() : -1;

    int qwtFmt = rawFmt;
    bool applyEngineeringFactor = false;
    double engFactor = 1.0;
    if (rawFmt == 4) { // Engineering (power-of-1000 divide factor)
        qwtFmt = int(Plot::Automatic);
        applyEngineeringFactor = true;
        if (const QwtScaleDiv *scDiv = g->plotWidget()->axisScaleDiv(d_active_axis)) {
            const double maxAbs =
                    qMax(std::fabs(scDiv->lowerBound()), std::fabs(scDiv->upperBound()));
            if (std::isfinite(maxAbs) && maxAbs > 0.0) {
                const int exp3 = int(std::floor(std::log10(maxAbs) / 3.0)) * 3;
                if (exp3 != 0)
                    engFactor = std::pow(10.0, double(exp3));
            }
        }
    }

    int effPrec = uiPrec;
    if (effPrec < 0) {
        effPrec = (qwtFmt == int(Plot::Decimal)) ? 2 : ((qwtFmt == int(Plot::Automatic)) ? 6 : 2);
    }

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg)
            continue;
        if (applyEngineeringFactor) {
            lg->setTickLabelTransform(d_active_axis, engFactor, lg->tickPrefix(d_active_axis),
                                      lg->tickSuffix(d_active_axis));
        }
        const QString formula = lg->getAxesFormulas().value(d_active_axis);
        lg->setLabelsNumericFormat(d_active_axis, qwtFmt, effPrec, formula);
        lg->replot();
        emit lg->modifiedGraph();
    }
    if (applyEngineeringFactor && d_combo_tick_divide) {
        QSignalBlocker bDiv(d_combo_tick_divide);
        d_combo_tick_divide->setEditText(QString::number(engFactor, 'g', 8));
    }
    d_target_ml->pushUndoSnapshot(tr("Change Tick Format"), before, mergeId);
}

void GraphInspectorDock::applyTickDivideFromUi()
{
    const QVector<Graph *> targets = targetLayersForScope();
    if (d_syncing_props || !d_target_ml || targets.isEmpty() || !d_combo_tick_divide)
        return;

    bool ok = false;
    const double factor = d_combo_tick_divide->currentText().trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(factor) || std::fabs(factor) <= 1e-300)
        return;

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg)
            continue;
        lg->setTickLabelTransform(d_active_axis, factor, lg->tickPrefix(d_active_axis),
                                  lg->tickSuffix(d_active_axis));
        lg->replot();
        emit lg->modifiedGraph();
    }
    d_target_ml->pushUndoSnapshot(tr("Change Tick Divide Factor"), before);
}

void GraphInspectorDock::refreshCategoryColumnCombo()
{
    if (!d_combo_category_col)
        return;

    QSignalBlocker blocker(d_combo_category_col);
    d_combo_category_col->clear();
    d_combo_category_col->addItem(tr("(Numeric Ticks)"), QString());

    Graph *g = activeLayer();
    QString currentFmtInfo;
    if (g && d_active_axis >= 0 && d_active_axis < 4) {
        const QList<Graph::AxisType> types = g->axesType();
        if (d_active_axis < types.size() && types[d_active_axis] == Graph::AxisType::Txt)
            currentFmtInfo = g->axisFormatInfo(d_active_axis).trimmed();
    }

    int selectedIdx = 0;
    if (d_app) {
        QList<MyWidget *> windows = d_app->windowsList();
        for (MyWidget *w : windows) {
            Table *tbl = qobject_cast<Table *>(w);
            if (!tbl || tbl->status() == MyWidget::Hidden)
                continue;
            if (d_app->hiddenWindows.contains(tbl))
                continue;
            if (tbl->name().startsWith(QLatin1String("RpcFitCurve")))
                continue;
            const int nCols = tbl->numCols();
            for (int c = 0; c < nCols; ++c) {
                const QString shortCol = tbl->colLabel(c);
                const QString fullCol = tbl->name() + QLatin1Char('_') + shortCol;
                QString display = fullCol;
                if (Column *colPtr = tbl->column(c)) {
                    if (colPtr->columnMode() == SciDAVis::ColumnMode::Text)
                        display += QStringLiteral(" [Text]");
                }
                const int itemIdx = d_combo_category_col->count();
                d_combo_category_col->addItem(display, fullCol);
                if (!currentFmtInfo.isEmpty() && fullCol == currentFmtInfo)
                    selectedIdx = itemIdx;
            }
        }
    }

    if (selectedIdx == 0 && g && !g->categoryLabels(d_active_axis).isEmpty()) {
        const QStringList cats = g->categoryLabels(d_active_axis);
        const QString customLabel =
                tr("Custom (%1 labels)").arg(cats.size());
        d_combo_category_col->addItem(customLabel, QStringLiteral("__custom__"));
        selectedIdx = d_combo_category_col->count() - 1;
    }

    d_combo_category_col->setCurrentIndex(selectedIdx);
}

void GraphInspectorDock::applyCategoryColumnFromUi()
{
    const QVector<Graph *> targets = targetLayersForScope();
    if (d_syncing_props || !d_target_ml || targets.isEmpty() || !d_combo_category_col || !d_app)
        return;

    const QString fullCol = d_combo_category_col->currentData().toString();
    if (fullCol == QLatin1String("__custom__"))
        return;

    Table *srcTable = nullptr;
    Column *srcCol = nullptr;
    QString shortColName;
    QStringList labels;

    if (!fullCol.isEmpty()) {
        srcTable = d_app->table(fullCol);
        if (!srcTable) {
            const int us = fullCol.lastIndexOf(QLatin1Char('_'));
            if (us > 0)
                srcTable = d_app->table(fullCol.left(us));
        }
        if (!srcTable)
            return;
        const QString prefix = srcTable->name() + QLatin1Char('_');
        shortColName = fullCol.startsWith(prefix) ? fullCol.mid(prefix.size()) : fullCol;
        const int colIdx = srcTable->colIndex(fullCol);
        srcCol = (colIdx >= 0) ? srcTable->column(colIdx) : srcTable->column(shortColName);
        if (!srcCol)
            return;

        int lastNonEmpty = -1;
        const int nRows = srcCol->rowCount();
        for (int r = 0; r < nRows; ++r) {
            const QString txt = srcTable->text(r, srcTable->colIndex(srcCol->name())).trimmed();
            if (!txt.isEmpty())
                lastNonEmpty = r;
        }
        for (int r = 0; r <= lastNonEmpty; ++r) {
            QString txt;
            if (srcCol->columnMode() == SciDAVis::ColumnMode::Text)
                txt = srcCol->textAt(r).trimmed();
            else
                txt = srcTable->text(r, srcTable->colIndex(srcCol->name())).trimmed();
            labels.append(txt);
        }
    }

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg)
            continue;
        if (fullCol.isEmpty()) {
            lg->setCategoryLabels(d_active_axis, QStringList());
            const QString formula = lg->getAxesFormulas().value(d_active_axis);
            lg->setLabelsNumericFormat(d_active_axis, Plot::Automatic, 6, formula);
        } else if (srcTable && srcCol) {
            lg->setLabelsTextFormat(d_active_axis, srcCol, 0, qMax(0, labels.size() - 1));
            if (!labels.isEmpty())
                lg->setCategoryLabels(d_active_axis, labels);
        }
        lg->replot();
        emit lg->modifiedGraph();
    }
    d_target_ml->pushUndoSnapshot(tr("Change Category Labels"), before);
}

void GraphInspectorDock::applyAxisTitleMarkup(const QString &openTag, const QString &closeTag)
{
    if (!d_edit_axis_title)
        return;
    QString txt = d_edit_axis_title->text();
    if (d_edit_axis_title->hasSelectedText()) {
        const int start = d_edit_axis_title->selectionStart();
        const QString sel = d_edit_axis_title->selectedText();
        txt.replace(start, sel.length(), openTag + sel + closeTag);
        d_edit_axis_title->setText(txt);
    } else {
        const int pos = d_edit_axis_title->cursorPosition();
        txt.insert(pos, openTag + closeTag);
        d_edit_axis_title->setText(txt);
        d_edit_axis_title->setCursorPosition(pos + openTag.length());
    }
    emit d_edit_axis_title->editingFinished();
}

void GraphInspectorDock::insertAxisTitleToken(const QString &token)
{
    if (!d_edit_axis_title || token.isEmpty())
        return;
    QString txt = d_edit_axis_title->text();
    const int pos = d_edit_axis_title->cursorPosition();
    txt.insert(pos, token);
    d_edit_axis_title->setText(txt);
    d_edit_axis_title->setCursorPosition(pos + token.length());
    emit d_edit_axis_title->editingFinished();
}

void GraphInspectorDock::rebuildGreekSymbolMenu()
{
    if (!d_menu_greek)
        return;
    d_menu_greek->clear();
    const QList<QPair<QString, QString>> greekList = {
        { tr("\xCE\xB1 (alpha)"), QString::fromUtf8("\xCE\xB1") },
        { tr("\xCE\xB2 (beta)"), QString::fromUtf8("\xCE\xB2") },
        { tr("\xCE\xB3 (gamma)"), QString::fromUtf8("\xCE\xB3") },
        { tr("\xCE\xB4 (delta)"), QString::fromUtf8("\xCE\xB4") },
        { tr("\xCE\x94 (Delta)"), QString::fromUtf8("\xCE\x94") },
        { tr("\xCE\xB5 (epsilon)"), QString::fromUtf8("\xCE\xB5") },
        { tr("\xCE\xB8 (theta)"), QString::fromUtf8("\xCE\xB8") },
        { tr("\xCE\xBB (lambda)"), QString::fromUtf8("\xCE\xBB") },
        { tr("\xCE\xBC (mu)"), QString::fromUtf8("\xCE\xBC") },
        { tr("\xCF\x81 (rho)"), QString::fromUtf8("\xCF\x81") },
        { tr("\xCF\x83 (sigma)"), QString::fromUtf8("\xCF\x83") },
        { tr("\xCF\x84 (tau)"), QString::fromUtf8("\xCF\x84") },
        { tr("\xCF\x86 (phi)"), QString::fromUtf8("\xCF\x86") },
        { tr("\xCE\xA9 (Omega)"), QString::fromUtf8("\xCE\xA9") }
    };
    for (const auto &pr : greekList) {
        const QString sym = pr.second;
        d_menu_greek->addAction(pr.first, this, [this, sym]() { insertAxisTitleToken(sym); });
    }
}

void GraphInspectorDock::rebuildUnitsSymbolMenu()
{
    if (!d_menu_units)
        return;
    for (QMenu *sub : d_menu_units->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly))
        delete sub;
    d_menu_units->clear();

    // 1) Materials-science symbols
    QMenu *symSub = d_menu_units->addMenu(tr("Symbols"));
    const QList<QPair<QString, QString>> matSymbols = {
        { tr("\xCF\x83 (sigma)"), QString::fromUtf8("\xCF\x83") },
        { tr("\xCE\xB5 (epsilon)"), QString::fromUtf8("\xCE\xB5") },
        { tr("\xCE\x94 (Delta)"), QString::fromUtf8("\xCE\x94") },
        { tr("\xC2\xB1 (plus/minus)"), QString::fromUtf8("\xC2\xB1") },
        { tr("\xC3\x97 (times)"), QString::fromUtf8("\xC3\x97") },
        { tr("\xC2\xB7 (cdot)"), QString::fromUtf8("\xC2\xB7") },
        { tr("\xC2\xB0 (degree)"), QString::fromUtf8("\xC2\xB0") },
        { tr("\xC3\x85 (Angstrom)"), QString::fromUtf8("\xC3\x85") },
        { tr("\xCE\xBC (mu)"), QString::fromUtf8("\xCE\xBC") },
        { tr("\xCE\xB8 (theta)"), QString::fromUtf8("\xCE\xB8") }
    };
    for (const auto &pr : matSymbols) {
        const QString tok = pr.second;
        symSub->addAction(pr.first, this, [this, tok]() { insertAxisTitleToken(tok); });
    }

    // 2) 1-click Journal Axis Templates
    d_menu_units->addSeparator();
    const QList<QPair<QString, QString>> journalTemplates = {
        { QString::fromUtf8("\xCF\x83 (MPa)"), QString::fromUtf8("\xCF\x83 (MPa)") },
        { QString::fromUtf8("\xCF\x83\xE2\x82\x80.\xE2\x82\x82 (MPa)"),
          QString::fromUtf8("\xCF\x83<sub>0.2</sub> (MPa)") },
        { QString::fromUtf8("\xCE\xB5 (%)"), QString::fromUtf8("\xCE\xB5 (%)") },
        { QString::fromUtf8("d\xCF\x83/d\xCE\xB5 (GPa)"),
          QString::fromUtf8("d\xCF\x83/d\xCE\xB5 (GPa)") },
        { QString::fromUtf8("2\xCE\xB8 (deg.)"), QString::fromUtf8("2\xCE\xB8 (deg.)") },
        { QString::fromUtf8("d\xE2\x81\xBB\xC2\xB9/\xC2\xB2 (\xCE\xBC" "m\xE2\x81\xBB\xC2\xB9/\xC2\xB2)"),
          QString::fromUtf8("<i>d</i><sup>-1/2</sup> (\xCE\xBC" "m<sup>-1/2</sup>)") },
        { QString::fromUtf8("10\xE2\x81\xBB\xE2\x81\xB4 s\xE2\x81\xBB\xC2\xB9"),
          QStringLiteral("10<sup>-4</sup> s<sup>-1</sup>") },
        { QString::fromUtf8("mass ppm\xC2\xB7s\xE2\x81\xBB\xC2\xB9"),
          QString::fromUtf8("mass ppm\xC2\xB7s<sup>-1</sup>") },
        { QString::fromUtf8("1000/T (K\xE2\x81\xBB\xC2\xB9)"),
          QStringLiteral("1000/<i>T</i> (K<sup>-1</sup>)") }
    };
    for (int i = 0; i < journalTemplates.size(); ++i) {
        const QString disp = journalTemplates[i].first;
        const QString tmpl = journalTemplates[i].second;
        const bool isUnitOnly = (i == 6 || i == 7);
        d_menu_units->addAction(disp, this, [this, tmpl, isUnitOnly]() {
            if (!d_edit_axis_title)
                return;
            if (d_edit_axis_title->hasSelectedText()) {
                const int start = d_edit_axis_title->selectionStart();
                const int len = d_edit_axis_title->selectedText().length();
                QString txt = d_edit_axis_title->text();
                txt.replace(start, len, tmpl);
                d_edit_axis_title->setText(txt);
            } else if (isUnitOnly && !d_edit_axis_title->text().trimmed().isEmpty()) {
                insertAxisTitleToken(QStringLiteral(" (") + tmpl + QStringLiteral(")"));
                return;
            } else {
                d_edit_axis_title->setText(tmpl);
            }
            emit d_edit_axis_title->editingFinished();
        });
    }

    // 3) Common Units submenu
    d_menu_units->addSeparator();
    QMenu *unitSub = d_menu_units->addMenu(tr("Common Units"));
    const QList<QPair<QString, QString>> unitList = {
        { QStringLiteral("MPa"), QStringLiteral(" (MPa)") },
        { QStringLiteral("GPa"), QStringLiteral(" (GPa)") },
        { QStringLiteral("%"), QStringLiteral(" (%)") },
        { QStringLiteral("K"), QStringLiteral(" (K)") },
        { QString::fromUtf8("\xC2\xB0" "C"), QString::fromUtf8(" (\xC2\xB0" "C)") },
        { QString::fromUtf8("s\xE2\x81\xBB\xC2\xB9"), QStringLiteral(" (s<sup>-1</sup>)") },
        { QStringLiteral("mm"), QStringLiteral(" (mm)") },
        { QString::fromUtf8("\xCE\xBC" "m"), QString::fromUtf8(" (\xCE\xBC" "m)") },
        { QStringLiteral("nm"), QStringLiteral(" (nm)") },
        { QString::fromUtf8("\xC3\x85"), QString::fromUtf8(" (\xC3\x85)") },
        { QString::fromUtf8("cm\xE2\x81\xBB\xC2\xB9"), QStringLiteral(" (cm<sup>-1</sup>)") },
        { QStringLiteral("wt.ppm"), QStringLiteral(" (wt.ppm)") },
        { QStringLiteral("mass%"), QStringLiteral(" (mass%)") },
        { QStringLiteral("at.%"), QStringLiteral(" (at.%)") },
        { QStringLiteral("kJ/mol"), QStringLiteral(" (kJ/mol)") },
        { QStringLiteral("eV"), QStringLiteral(" (eV)") }
    };
    for (const auto &pr : unitList) {
        const QString tok = pr.second;
        unitSub->addAction(pr.first, this, [this, tok]() { insertAxisTitleToken(tok); });
    }
}

void GraphInspectorDock::matchRightYAxisColorToCurve()
{
    Graph *g = activeLayer();
    if (!d_target_ml || !g)
        return;
    const int ci = effectiveSelectedCurveIndex(g);
    QwtPlotCurve *cv = (ci >= 0 && ci < g->curves()) ? g->curve(ci) : nullptr;
    if (!cv)
        return;

    const bool hasLine = (cv->style() != QwtPlotCurve::NoCurve && cv->pen().style() != Qt::NoPen);
    QColor col = (hasLine && cv->pen().color().isValid()) ? cv->pen().color()
                                                          : cv->symbol().pen().color();
    if (!col.isValid())
        return;
    col.setAlpha(255);

    const QString before = d_target_ml->captureUndoSnapshot();
    auto colorizeRightY = [&](Graph *targetG) {
        if (!targetG || !targetG->plotWidget())
            return;
        targetG->enableAxis(QwtPlot::yRight, true);
        targetG->setAxisBackboneVisible(QwtPlot::yRight, true);
        targetG->setRightAxisTitleColor(col);

        QStringList axCols = targetG->axesColors();
        while (axCols.size() < 4)
            axCols.append(QStringLiteral("#000000"));
        axCols[QwtPlot::yRight] = col.name();
        targetG->setAxesColors(axCols);

        QStringList numCols = targetG->axesNumColors();
        while (numCols.size() < 4)
            numCols.append(QStringLiteral("#000000"));
        numCols[QwtPlot::yRight] = col.name();
        targetG->setAxesNumColors(numCols);

        if (QwtScaleWidget *sw = targetG->plotWidget()->axisWidget(QwtPlot::yRight)) {
            QPalette pal = sw->palette();
            pal.setColor(QPalette::WindowText, col);
            pal.setColor(QPalette::Text, col);
            sw->setPalette(pal);
        }
        targetG->replot();
        emit targetG->modifiedGraph();
    };

    colorizeRightY(g);
    for (Graph *cand : d_target_ml->layersList()) {
        if (cand && cand->linkedParentLayer() == g && cand->plotWidget()
            && cand->plotWidget()->axisEnabled(QwtPlot::yRight)) {
            colorizeRightY(cand);
        }
    }

    d_target_ml->pushUndoSnapshot(tr("Match Right-Y Axis Color"), before);
}

void GraphInspectorDock::refreshPaletteSchemeCombo()
{
    if (!d_combo_series_scheme)
        return;
    QSignalBlocker b(d_combo_series_scheme);
    const QString prevName = d_combo_series_scheme->currentData().toString();
    d_combo_series_scheme->clear();
    const QStringList names = Graph::allPaletteNames();
    for (const QString &nm : names)
        d_combo_series_scheme->addItem(tr(nm.toUtf8().constData()), nm);

    int targetIdx = prevName.isEmpty() ? d_combo_series_scheme->findData(QStringLiteral("Okabe-Ito"))
                                       : d_combo_series_scheme->findData(prevName);
    if (targetIdx < 0)
        targetIdx = 0;
    if (d_combo_series_scheme->count() > 0)
        d_combo_series_scheme->setCurrentIndex(targetIdx);
}

void GraphInspectorDock::applyGroupAutoStyleFromUi()
{
    const QVector<Graph *> targets = targetLayersForScope();
    if (!d_target_ml || targets.isEmpty() || !d_combo_series_scheme)
        return;

    const QString palName = d_combo_series_scheme->currentData().toString().isEmpty()
            ? d_combo_series_scheme->currentText()
            : d_combo_series_scheme->currentData().toString();
    QList<QColor> anchors = Graph::paletteColors(palName);
    if (anchors.isEmpty())
        anchors = { QColor(Qt::black) };

    const bool cycleShapes = d_chk_cycle_sym_shapes && d_chk_cycle_sym_shapes->isChecked();
    const bool pairedHollow = d_chk_paired_hollow && d_chk_paired_hollow->isChecked();
    const bool isSequential =
            (palName.compare(QStringLiteral("Viridis"), Qt::CaseInsensitive) == 0
             || palName.compare(QStringLiteral("Plasma"), Qt::CaseInsensitive) == 0
             || palName.compare(QStringLiteral("Cool-Warm"), Qt::CaseInsensitive) == 0);

    static const QVector<QwtSymbol::Style> kShapeCycle = {
        QwtSymbol::Ellipse,   QwtSymbol::Rect,    QwtSymbol::UTriangle, QwtSymbol::DTriangle,
        QwtSymbol::Diamond,   QwtSymbol::Hexagon, QwtSymbol::Star1
    };

    const QString before = d_target_ml->captureUndoSnapshot();
    for (Graph *lg : targets) {
        if (!lg)
            continue;
        const QList<QwtPlotCurve *> fitCurves = lg->fitCurvesList();
        QVector<int> eligibleIndices;
        for (int ci = 0; ci < lg->curves(); ++ci) {
            if (lg->curveType(ci) == Graph::ErrorBars || lg->curveType(ci) == Graph::Function)
                continue;
            QwtPlotCurve *cv = lg->curve(ci);
            if (!cv || fitCurves.contains(cv)
                || dynamic_cast<FunctionCurve *>(lg->plotItem(ci)) != nullptr)
                continue;
            eligibleIndices.append(ci);
        }
        if (eligibleIndices.isEmpty())
            continue;

        const QList<QColor> effColors = (isSequential && eligibleIndices.size() > 1)
                ? Graph::interpolatePalette(anchors, eligibleIndices.size())
                : anchors;

        for (int k = 0; k < eligibleIndices.size(); ++k) {
            const int ci = eligibleIndices[k];
            QwtPlotCurve *cv = lg->curve(ci);
            if (!cv)
                continue;
            const QColor col = effColors[k % effColors.size()];
            const bool makeHollow = pairedHollow ? ((k % 2) == 1)
                                                 : Graph::isSymbolWhiteHollow(cv->symbol());

            QPen lp = cv->pen();
            lp.setColor(col);
            cv->setPen(lp);

            QwtSymbol sym = cv->symbol();
            if (cycleShapes && sym.style() != QwtSymbol::NoSymbol) {
                const int shapeIdx = (pairedHollow ? (k / 2) : k) % kShapeCycle.size();
                sym.setStyle(kShapeCycle[shapeIdx]);
            }
            if (sym.style() != QwtSymbol::NoSymbol) {
                QPen sp = sym.pen();
                sp.setColor(col);
                if (sp.width() <= 0)
                    sp.setWidth(qMax(1, lp.width()));
                sym.setPen(sp);
                sym.setBrush(makeHollow ? QBrush(QColor(Qt::white)) : QBrush(col));
                cv->setSymbol(sym);
            }

            if (cv->brush().style() != Qt::NoBrush) {
                QColor fc = col;
                fc.setAlpha(cv->brush().color().alpha() > 0 ? cv->brush().color().alpha() : 65);
                cv->setBrush(QBrush(fc));
            }

            if (auto *dc = dynamic_cast<DataCurve *>(cv)) {
                for (DataCurve *eb : dc->errorBarsList()) {
                    if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(eb))
                        er->setColor(col);
                }
            }
        }
        lg->reconstructLegend();
        lg->updatePlot();
    }

    d_target_ml->pushUndoSnapshot(tr("Apply Series Style Scheme"), before);
    refreshObjectTree();
    syncCurveControls();
}

void GraphInspectorDock::applyQuickErrorBars(int mode)
{
    Graph *g = activeLayer();
    if (!d_app || !d_target_ml || !g)
        return;
    const int ci = effectiveSelectedCurveIndex(g);
    DataCurve *dc = (ci >= 0 && ci < g->curves()) ? dynamic_cast<DataCurve *>(g->curve(ci))
                                                 : nullptr;
    if (!dc || !dc->table() || !dc->table()->d_future_table) {
        if (d_app && d_app->statusBar()) {
            d_app->statusBar()->showMessage(
                    tr("This feature is not available for user defined function curves!"), 4000);
        }
        syncCurveControls();
        return;
    }

    Table *tbl = dc->table();
    const QString curveName = dc->title().text();
    MultiLayer *ml = g->multiLayerParent();
    if (!ml)
        ml = d_target_ml.data();

    auto removeVerticalErrorBarsOnCurve = [&](DataCurve *master, const QString &onlyMatchingCol) -> int {
        int removed = 0;
        const QList<DataCurve *> errs = master->errorBarsList();
        for (DataCurve *eb : errs) {
            auto *er = dynamic_cast<QwtErrorPlotCurve *>(eb);
            if (!er || er->direction() != QwtErrorPlotCurve::Vertical)
                continue;
            if (!onlyMatchingCol.isEmpty() && er->title().text() != onlyMatchingCol)
                continue;
            const int eIdx = g->curveIndex(er);
            if (eIdx >= 0) {
                if (d_app && ml)
                    d_app->invalidateModelessGraphDialogs(ml);
                g->removeCurve(eIdx);
                ++removed;
            }
        }
        return removed;
    };

    if (mode == 0) {
        const int colIdx = tbl->colIndex(dc->yColumnName());
        const int errColIdx = findSmartAdjacentYErrColumn(tbl, colIdx);
        if (errColIdx < 0) {
            if (d_app && d_app->statusBar()) {
                d_app->statusBar()->showMessage(
                        tr("No adjacent column exists to the right of the selected curve column."),
                        4000);
            }
            syncCurveControls();
            return;
        }

        const QString errFull =
                QString(tbl->name()) + QStringLiteral("_") + tbl->colLabel(errColIdx);
        const QString before = ml ? ml->captureUndoSnapshot() : QString();
        bool changed = false;

        if (removeVerticalErrorBarsOnCurve(dc, errFull) > 0) {
            changed = true;
        } else {
            removeVerticalErrorBarsOnCurve(dc, QString());
            if (g->addErrorBars(dc->xColumnName(), curveName, tbl, errFull,
                                QwtErrorPlotCurve::Vertical)) {
                changed = true;
            }
        }
        if (changed) {
            g->updatePlot();
            if (ml)
                ml->pushUndoSnapshot(tr("Toggle Error Bars"), before);
            refreshAll();
        } else {
            syncCurveControls();
        }
    } else if (mode == 1 || mode == 2) {
        const QString xColName = dc->xColumnName();
        const int srcColIdx = tbl->colIndex(dc->yColumnName());
        Column *data = (srcColIdx >= 0) ? tbl->column(srcColIdx) : nullptr;
        if (xColName.isEmpty() || !data || !tbl->d_future_table) {
            syncCurveControls();
            return;
        }

        const QString detColName = tbl->colLabel(srcColIdx)
                + (mode == 2 ? QStringLiteral("_5pct_yErr") : QStringLiteral("_SD_yErr"));
        const QString detFullName = QString(tbl->name()) + QStringLiteral("_") + detColName;

        // Repeat click on the active quick error-bar mode toggles it OFF cleanly!
        const QString before = ml ? ml->captureUndoSnapshot() : QString();
        if (removeVerticalErrorBarsOnCurve(dc, detFullName) > 0) {
            g->updatePlot();
            if (ml)
                ml->pushUndoSnapshot(tr("Toggle Error Bars"), before);
            if (d_app)
                emit d_app->modified();
            refreshAll();
            return;
        }

        const int rows = data->rowCount();
        QVector<qreal> errVals(qMax(0, rows), 0.0);
        QVector<bool> errValid(qMax(0, rows), false);
        if (rows > 0) {
            const int r0 = qMax(0, dc->startRow());
            const int r1 = (dc->endRow() >= 0) ? qMin(rows - 1, dc->endRow()) : (rows - 1);
            const int xColIdx = tbl->colIndex(xColName);
            Column *xCol = (xColIdx >= 0) ? tbl->column(xColIdx) : nullptr;
            QVector<bool> pointMaskedAtRow(rows, false);
            for (int p = 0; p < dc->dataSize(); ++p) {
                if (dc->isPointMasked(p)) {
                    const int tr = dc->tableRow(p);
                    if (tr >= 0 && tr < rows)
                        pointMaskedAtRow[tr] = true;
                }
            }

            if (mode == 2) {
                for (int i = r0; i <= r1; ++i) {
                    if (data->isInvalid(i) || data->isMasked(i)
                        || (xCol && xCol->isMasked(i)) || pointMaskedAtRow[i]) {
                        continue;
                    }
                    const double v = data->valueAt(i);
                    if (std::isfinite(v)) {
                        errVals[i] = std::abs(v * 0.05);
                        errValid[i] = true;
                    }
                }
            } else {
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
                for (int idx : validRows) {
                    errVals[idx] = sdVal;
                    errValid[idx] = true;
                }
            }
        }

        removeVerticalErrorBarsOnCurve(dc, QString());

        int existingErrIdx = tbl->colIndex(detFullName);
        if (existingErrIdx < 0)
            existingErrIdx = tbl->colIndex(detColName);
        Column *errCol = (existingErrIdx >= 0) ? tbl->column(existingErrIdx) : nullptr;
        if (errCol && errCol->columnMode() == SciDAVis::ColumnMode::Numeric) {
            tbl->d_future_table->beginMacro(tr("%1: change cell value(s)").arg(tbl->name()));
            errCol->setPlotDesignation(SciDAVis::yErr);
            if (!errVals.isEmpty()) {
                errCol->replaceValues(0, errVals);
                for (int r = 0; r < rows; ++r) {
                    if (!errValid[r])
                        errCol->setInvalid(r, true);
                }
            }
            tbl->d_future_table->endMacro();
        } else {
            QString uniqueColName = detColName;
            int suffix = 2;
            const QStringList existingCols = tbl->colNames();
            while (existingCols.contains(uniqueColName)
                   || tbl->colIndex(QString(tbl->name()) + QStringLiteral("_") + uniqueColName) >= 0) {
                uniqueColName = detColName + QStringLiteral("_") + QString::number(suffix++);
            }
            errCol = new Column(uniqueColName, SciDAVis::ColumnMode::Numeric);
            errCol->setPlotDesignation(SciDAVis::yErr);
            if (!errVals.isEmpty()) {
                errCol->replaceValues(0, errVals);
                for (int r = 0; r < rows; ++r) {
                    if (!errValid[r])
                        errCol->setInvalid(r, true);
                }
            }
            tbl->d_future_table->addChild(errCol);
        }

        const QString fullErrColName = QString(tbl->name()) + QStringLiteral("_") + errCol->name();
        const bool changed = g->addErrorBars(xColName, curveName, tbl, fullErrColName,
                                             QwtErrorPlotCurve::Vertical);
        if (changed) {
            g->updatePlot();
            if (ml)
                ml->pushUndoSnapshot(tr("Toggle Error Bars"), before);
            if (d_app)
                emit d_app->modified();
            refreshAll();
        } else {
            syncCurveControls();
        }
    }
}

void GraphInspectorDock::applyTheme()
{
    static bool s_applying = false;
    if (s_applying)
        return;
    s_applying = true;
    const bool dark = isPaletteDark();
    const QString bg = dark ? QStringLiteral("#1E222B") : QStringLiteral("#F8FAFC");
    const QString hdrBg = dark ? QStringLiteral("#171A22") : QStringLiteral("#EEF2F6");
    const QString cardBg = dark ? QStringLiteral("#252A36") : QStringLiteral("#FFFFFF");
    const QString border = dark ? QStringLiteral("#333B4D") : QStringLiteral("#D8E0EA");
    const QString fg = dark ? QStringLiteral("#E6ECF5") : QStringLiteral("#1E293B");
    const QString subFg = dark ? QStringLiteral("#94A3B8") : QStringLiteral("#64748B");
    const QString accent = dark ? QStringLiteral("#38BDF8") : QStringLiteral("#0284C7");
    const QString accentSoft = dark ? QStringLiteral("#1E3A5F") : QStringLiteral("#E0F2FE");

    const QString railFg = dark ? QStringLiteral("#CBD5E1") : QStringLiteral("#334155");

    const QString qss = QStringLiteral(
                                "#InspectorRootContainer { background: %1; color: %5; border-left: 1px solid %4; }"
                                "#InspectorHeaderBar, #InspectorRailPanel, #InspectorScopeStrip { background: %2; border-bottom: 1px solid %4; }"
                                "#InspectorHeaderTitle { font-weight: 600; font-size: 15px; color: %5; }"
                                "#InspectorHeaderTarget { font-size: 14px; color: %6; }"
                                "#InspectorSubTitle { font-weight: 600; font-size: 14px; color: %5; }"
                                "#InspectorSectionHeader { font-weight: 600; font-size: 14px; color: %7; padding-top: 4px; border-bottom: 1px solid %4; }"
                                "#InspectorTabStrip { background: %2; border-top: 1px solid %4; border-bottom: 1px solid %4; }"
                                "QToolButton#InspectorContextTabBtn {"
                                "  background: %3; color: %5; border: 1px solid %4; border-radius: 4px; font-size: 10.8pt; min-height: 30px; padding: 1px 4px;"
                                "}"
                                "QToolButton#InspectorAxisPillBtn, QToolButton#InspectorScopePillBtn {"
                                "  background: %3; color: %5; border: 1px solid %4; border-radius: 4px; font-size: 14px; padding: 2px 5px;"
                                "}"
                                "QToolButton#InspectorContextTabBtn:checked, QToolButton#InspectorAxisPillBtn:checked, QToolButton#InspectorScopePillBtn:checked {"
                                "  background: %8; color: %7; border: 1px solid %7; font-weight: 600;"
                                "}"
                                "QToolButton#InspectorRailTabBtn {"
                                "  color: %9; font-size: 9pt; border-radius: 4px; padding: 1px 0px;"
                                "}"
                                "QTreeWidget#GraphObjectManagerTree {"
                                "  background: %3; color: %5; border: 1px solid %4; border-radius: 4px; font-size: 14px;"
                                "}"
                                "QPushButton { font-size: 14px; padding: 4px 6px; }"
                                "QLabel, QCheckBox, QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit { font-size: 14px; }")
                                .arg(bg, hdrBg, cardBg, border, fg, subFg, accent, accentSoft, railFg);

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
        d_rail_axis_btn->setText(tr("X/Y"));
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

    if (d_tab_btn_page)
        d_tab_btn_page->setText(tr("Overall"));
    if (d_tab_btn_axis)
        d_tab_btn_axis->setText(tr("X/Y"));
    if (d_tab_btn_curve)
        d_tab_btn_curve->setText(tr("Curve"));
    if (d_tab_btn_legend)
        d_tab_btn_legend->setText(tr("Legend"));

    // Page 0: Page / Layer
    if (d_sec_presets_lbl)
        d_sec_presets_lbl->setText(tr("1-Click Publication & Slide Presets"));
    if (d_btn_preset_isij)
        d_btn_preset_isij->setText(tr("ISIJ Journal (85mm)"));
    if (d_btn_preset_slide28)
        d_btn_preset_slide28->setText(tr("Presentation Slide (28pt)"));
    if (d_btn_square_frame70)
        d_btn_square_frame70->setText(tr("Square Frame (70%)"));

    if (d_sec_page_size_lbl)
        d_sec_page_size_lbl->setText(tr("Page Size & View"));
    if (d_lbl_page_w)
        d_lbl_page_w->setText(tr("Width:"));
    if (d_lbl_page_h)
        d_lbl_page_h->setText(tr("Height:"));
    if (d_lbl_page_dpi)
        d_lbl_page_dpi->setText(tr("DPI:"));
    if (d_chk_page_view)
        d_chk_page_view->setText(tr("Print Page View"));
    if (d_lbl_page_zoom)
        d_lbl_page_zoom->setText(tr("Page Zoom:"));

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

    if (d_sec_export_lbl)
        d_sec_export_lbl->setText(tr("Clipboard & Export"));
    if (d_btn_copy_ole)
        d_btn_copy_ole->setText(tr("Copy OLE"));
    if (d_btn_export_img_pdf)
        d_btn_export_img_pdf->setText(tr("Export Image / PDF"));

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
        d_chk_axis_visible->setText(tr("Show Selected Axis"));
    if (d_chk_tick_labels)
        d_chk_tick_labels->setText(tr("Show Tick Numbers"));

    if (d_sec_axis_range_lbl)
        d_sec_axis_range_lbl->setText(tr("Scale & Axis Break"));
    if (d_lbl_axis_min)
        d_lbl_axis_min->setText(tr("Min:"));
    if (d_lbl_axis_max)
        d_lbl_axis_max->setText(tr("Max:"));
    if (d_lbl_axis_step)
        d_lbl_axis_step->setText(tr("Step (0=Auto):"));
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
    if (d_combo_minor_subdiv && d_combo_minor_subdiv->count() >= 4) {
        d_combo_minor_subdiv->setItemText(0, tr("0 (None)"));
        d_combo_minor_subdiv->setItemText(1, tr("1 (2 subdiv)"));
        d_combo_minor_subdiv->setItemText(2, tr("4 (5 subdiv)"));
        d_combo_minor_subdiv->setItemText(3, tr("9 (10 subdiv)"));
    }
    if (d_chk_axis_break)
        d_chk_axis_break->setText(tr("Axis Break (Omit Middle Range)"));
    if (d_lbl_break_from)
        d_lbl_break_from->setText(tr("Break Start:"));
    if (d_lbl_break_to)
        d_lbl_break_to->setText(tr("Break End:"));

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
    if (d_btn_fmt_italic)
        d_btn_fmt_italic->setToolTip(tr("Italic (<i>...</i>)"));
    if (d_btn_fmt_sub)
        d_btn_fmt_sub->setToolTip(tr("Subscript (<sub>...</sub>)"));
    if (d_btn_fmt_sup)
        d_btn_fmt_sup->setToolTip(tr("Superscript (<sup>...</sup>)"));
    if (d_btn_fmt_greek) {
        d_btn_fmt_greek->setText(tr("Greek"));
        d_btn_fmt_greek->setToolTip(tr("Insert Greek Symbol"));
    }
    rebuildGreekSymbolMenu();
    if (d_btn_fmt_unit) {
        d_btn_fmt_unit->setText(tr("Units / Symbols"));
        d_btn_fmt_unit->setToolTip(tr("Insert Symbol, Unit, or Journal Axis Template"));
    }
    rebuildUnitsSymbolMenu();
    if (d_chk_lock_all_axes_font)
        d_chk_lock_all_axes_font->setText(tr("Apply Font Size to All 4 Axes"));
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
        d_spin_sym_size->setSuffix(QStringLiteral(" px"));
    if (d_lbl_sym_skip)
        d_lbl_sym_skip->setText(tr("Symbol Skip:"));
    if (d_spin_sym_skip) {
        d_spin_sym_skip->setSpecialValueText(tr("Auto"));
        d_spin_sym_skip->setToolTip(tr("Every %1 pts (0 = Auto)").arg(QStringLiteral("N")));
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
        d_sec_err_bars_lbl->setText(tr("Quick Error Bars"));
    if (d_btn_err_adjacent)
        d_btn_err_adjacent->setText(tr("Adjacent Col"));
    if (d_btn_err_sd)
        d_btn_err_sd->setText(tr("Std Dev (SD)"));
    if (d_btn_err_5pct)
        d_btn_err_5pct->setText(tr("5%"));
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

    if (d_sec_series_scheme_lbl)
        d_sec_series_scheme_lbl->setText(tr("Group Series Style Scheme"));
    refreshPaletteSchemeCombo();
    if (d_chk_cycle_sym_shapes)
        d_chk_cycle_sym_shapes->setText(tr("Cycle Symbol Shapes (● ■ ▲ ▼ ◆)"));
    if (d_chk_paired_hollow)
        d_chk_paired_hollow->setText(tr("Paired Filled / Hollow (● ○ ■ □)"));
    if (d_btn_apply_scheme_all)
        d_btn_apply_scheme_all->setText(tr("Apply to All Curves"));

    // Spectrogram / Heatmap
    if (d_sec_spec_lbl)
        d_sec_spec_lbl->setText(tr("Color Scale & Contours"));
    if (d_lbl_spec_cmap)
        d_lbl_spec_cmap->setText(tr("Colormap:"));
    if (d_combo_spec_cmap && d_combo_spec_cmap->count() >= 10) {
        d_combo_spec_cmap->setItemText(0, tr("Default"));
        d_combo_spec_cmap->setItemText(6, tr("Cool-Warm"));
        d_combo_spec_cmap->setItemText(7, tr("Blue-White-Red"));
        d_combo_spec_cmap->setItemText(9, tr("GrayScale"));
    }
    if (d_chk_spec_reverse)
        d_chk_spec_reverse->setText(tr("Reverse Colormap"));
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
    if (d_btn_leg_tl)
        d_btn_leg_tl->setText(tr("Top-Left"));
    if (d_btn_leg_tc)
        d_btn_leg_tc->setText(tr("Top-Center"));
    if (d_btn_leg_tr)
        d_btn_leg_tr->setText(tr("Top-Right"));
    if (d_btn_leg_bl)
        d_btn_leg_bl->setText(tr("Bottom-Left"));
    if (d_btn_leg_br)
        d_btn_leg_br->setText(tr("Bottom-Right"));
    if (d_btn_leg_out_r)
        d_btn_leg_out_r->setText(tr("Outside Right"));
    if (d_btn_leg_auto_avoid)
        d_btn_leg_auto_avoid->setText(tr("Place in Empty Space (Avoid Data)"));
    if (d_btn_leg_rebuild)
        d_btn_leg_rebuild->setText(tr("Reconstruct Legend"));

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
    if (d_lbl_leg_cols)
        d_lbl_leg_cols->setText(tr("Columns:"));
    if (d_lbl_leg_font_pt)
        d_lbl_leg_font_pt->setText(tr("Font Size:"));
}
