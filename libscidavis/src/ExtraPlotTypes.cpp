/***************************************************************************
    File                 : ExtraPlotTypes.cpp
    Project              : ToriGraph
    Description          : E11 graph types (error band, colour-mapped scatter,
                           histogram + cumulative %, lognormal / Gumbel probability plots,
                           2D density / 2D histogram, XRD pole figure)
 ***************************************************************************/
#include "ExtraPlotTypes.h"

#include "ApplicationWindow.h"
#include "Graph.h"
#include "Grid.h"
#include "I18nJapanese.h"
#include "Legend.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "MyWidget.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"
#include "QwtHistogram.h"
#include "ScaleDraw.h"
#include "Spectrogram.h"
#include "Table.h"
#include "core/column/Column.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QLineEdit>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

#include <qwt_plot_grid.h>
#include <qwt_plot_item.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_map.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>

#include <gsl/gsl_cdf.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>

namespace {

void registerE11Translations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        // actions / menus / ribbon
        reg("Add Error &Band...", "エラーバンドを追加(&B)...");
        reg("Error Band...", "エラーバンド...");
        reg("Error Band", "エラーバンド");
        reg("Shade the area between Y - error and Y + error from error columns",
            "誤差列から Y - 誤差 と Y + 誤差 の間を半透明で塗ります");
        reg("Color-Mapped &Scatter (Z)...", "カラーマップ散布図(Z 値で色分け)(&S)...");
        reg("Color-Mapped Scatter (Z)", "カラーマップ散布図(Z 値で色分け)");
        reg("Scatter plot whose symbol color follows a third column (and size a fourth), with a color bar",
            "第 3 列の値でシンボルの色を(第 4 列で大きさを)変える散布図。カラーバー付き");
        reg("Histogram + &Cumulative %...", "ヒストグラム + 累積 %(&C)...");
        reg("Histogram + Cumulative %", "ヒストグラム + 累積 %");
        reg("Frequency histogram with the cumulative percentage on the right axis",
            "度数のヒストグラムに、右軸の累積 % を重ねます");
        reg("&Lognormal Probability Plot...", "対数正規確率プロット(&L)...");
        reg("Lognormal Probability Plot", "対数正規確率プロット");
        reg("Lognormal Probability", "対数正規確率");
        reg("Lognormal probability paper: rank plotting positions and a straight-line fit (mu, sigma)",
            "対数正規確率紙: 順位法でプロットし、直線を当てはめて μ・σ を求めます");
        reg("&Gumbel (Extreme Value) Probability Plot...", "Gumbel(極値)確率プロット(&G)...");
        reg("Gumbel (Extreme Value) Probability Plot", "Gumbel(極値)確率プロット");
        reg("Gumbel Probability", "Gumbel 確率");
        reg("Probability Plots", "確率プロット");
        reg("Gumbel extreme value plot (e.g. maximum defect size): location, scale and return-period value",
            "Gumbel 極値プロット(最大欠陥寸法など): 位置・尺度と再現期間の推定値");
        // dialogs
        reg("Curve:", "曲線:");
        reg("Error columns:", "誤差の列:");
        reg("Symmetric (one ± column)", "対称(± の 1 列)");
        reg("Asymmetric (+ and − columns)", "非対称(+ と − の 2 列)");
        reg("± or + error column:", "± または + 誤差の列:");
        reg("− error column:", "− 誤差の列:");
        reg("Fill opacity (%):", "塗りの不透明度 (%):");
        reg("The active graph has no data curve to add an error band to.",
            "アクティブなグラフに、エラーバンドを付けられるデータ曲線がありません。");
        reg("The error band could not be added (check the error columns).",
            "エラーバンドを追加できませんでした(誤差の列を確認してください)。");
        reg("Source Table:", "元のワークシート:");
        reg("X column:", "X 列:");
        reg("Y column:", "Y 列:");
        reg("Color (Z) column:", "色 (Z) の列:");
        reg("Size column (optional):", "大きさの列(任意):");
        reg("(None)", "(なし)");
        reg("Color map:", "カラーマップ:");
        reg("Reverse color map", "カラーマップを反転");
        reg("Data column:", "データの列:");
        reg("Bin width (0 = automatic):", "ビン幅 (0 = 自動):");
        reg("Bin start:", "ビンの開始値:");
        reg("Plotting position:", "プロット位置(順位法):");
        reg("Median rank (i−0.3)/(n+0.4)", "メジアンランク (i−0.3)/(n+0.4)");
        reg("Mean rank i/(n+1)", "平均ランク i/(n+1)");
        reg("Hazen (i−0.5)/n", "Hazen (i−0.5)/n");
        reg("Blom (i−0.375)/(n+0.25)", "Blom (i−0.375)/(n+0.25)");
        reg("Return period T (0 = none):", "再現期間 T (0 = なし):");
        reg("The colour-mapped scatter needs numeric X, Y and Z columns.",
            "カラーマップ散布図には数値の X・Y・Z 列が必要です。");
        reg("No numeric data in the selected column.", "選んだ列に数値データがありません。");
        reg("At least 3 values are needed; only %1 remained.",
            "3 個以上の値が必要です(残ったのは %1 個)。");
        reg("All values are identical, so no line can be fitted.",
            "すべての値が等しいため、直線を当てはめられません。");
        reg("%1 value(s) <= 0 were excluded (the lognormal plot needs positive values).",
            "0 以下の値 %1 個を除外しました(対数正規プロットには正の値が必要です)。");
        reg("Cumulative probability, F (%)", "累積確率 F (%)");
        reg("Cumulative (%)", "累積 (%)");
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("ExtraPlotTypes"), QStringLiteral("Count"),
                QString::fromUtf8("度数"));
        reg("Add Error Band", "エラーバンドを追加");
        reg("Automatic (from data minimum)", "自動 (データ最小値から開始)");
        reg("Too many bins (at most 10,000); use a larger bin width.",
            "ビンが多すぎます(最大 10,000)。ビン幅を大きくしてください。");
        reg("Lognormal fit", "対数正規フィット");
        reg("Gumbel fit", "Gumbel フィット");
        // E11b: 2D density and pole figure
        reg("2D &Density (2D Histogram)...", "2 次元密度図(2 次元ヒストグラム)(&D)...");
        reg("2D Density (2D Histogram)", "2 次元密度図(2 次元ヒストグラム)");
        reg("2D Density", "2 次元密度図");
        reg("Count X-Y points on a grid and show the counts as a heat map with a color bar (fast for 100,000+ points)",
            "X・Y の点を格子ごとに数え、度数をカラーバー付きのヒートマップで表示します(10 万点以上でも高速)");
        reg("&Pole Figure (XRD)...", "極点図(XRD)(&P)...");
        reg("Pole Figure (XRD)", "極点図(XRD)");
        reg("Pole Figure", "極点図");
        reg("XRD pole figure from alpha, beta, intensity columns or an alpha x beta matrix: equal-area (Schmidt) or equal-angle (Wulff) projection with contours",
            "α・β・強度の 3 列、または α×β の行列から XRD 極点図を描きます。等面積(Schmidt)/等角(Wulff)投影、等強度線付き");
        reg("Bins in X (0 = automatic):", "X のビン数 (0 = 自動):");
        reg("Bins in Y (0 = automatic):", "Y のビン数 (0 = 自動):");
        reg("Bin width in X (0 = use bins):", "X のビン幅 (0 = ビン数で指定):");
        reg("Bin width in Y (0 = use bins):", "Y のビン幅 (0 = ビン数で指定):");
        reg("Logarithmic color scale", "対数カラースケール");
        reg("Flat cell blocks (no smoothing)", "セルごとに塗りつぶす (補間なし)");
        reg("Reverse color map", "カラーマップを反転");
        reg("Update active plot (uncheck to create new)", "現在のグラフを更新 (外すと新規作成)");
        reg("Current / Custom", "現在 / カスタム");
        reg("The 2D density needs numeric X and Y columns.", "2 次元密度図には数値の X・Y 列が必要です。");
        reg("No point has both a numeric X and Y value.", "X と Y の両方が数値の点がありません。");
        reg("Too many bins (at most 4096 per axis); use a larger bin width.",
            "ビンが多すぎます(1 軸あたり最大 4096)。ビン幅を大きくしてください。");
        reg("2D density of %1: %2 vs %3 (%4 x %5 bins)", "%1 の 2 次元密度: %2 対 %3 (%4 × %5 ビン)");
        reg("Source:", "元データ:");
        reg("Alpha (tilt) column:", "α(傾き)の列:");
        reg("Beta (rotation) column:", "β(回転)の列:");
        reg("Intensity column:", "強度の列:");
        reg("Alpha range (deg):", "α 範囲 (°):");
        reg("Beta range (deg):", "β 範囲 (°):");
        reg("Sample reference axes:", "試料基準軸タイトル:");
        reg("RD / TD (Rolled sheet)", "RD / TD (圧延材)");
        reg("ED / TD (Extruded)", "ED / TD (押出材)");
        reg("LD / TD (Longitudinal / Transverse)", "LD / TD (長手 / 横方向)");
        reg("Custom...", "カスタム...");
        reg("Custom axis labels:", "カスタム軸タイトル:");
        reg("Primary axis direction:", "主基準軸の向き:");
        reg("Top (secondary right)", "上 (副軸は右)");
        reg("Right (secondary top)", "右 (副軸は上)");
        reg("Show 30° and 60° tilt circles", "30°・60° 補助同心円を表示");
        reg("Projection:", "投影:");
        reg("Equal-area (Schmidt)", "等面積(Schmidt)");
        reg("Equal-angle (Wulff)", "等角(Wulff)");
        reg("Normalize to MRD (multiples of random)", "MRD(ランダム比)に規格化");
        reg("Alpha = 90° at the centre (Rigaku style)", "α = 90° を中心とする(リガク方式)");
        reg("Beta = 0° at the top", "β = 0° を上にする");
        reg("Beta increases clockwise", "β を時計回りに増やす");
        reg("RD direction:", "RD の向き:");
        reg("Top (TD right)", "上(TD は右)");
        reg("Right (TD top)", "右(TD は上)");
        reg("Contour levels:", "等強度線の本数:");
        reg("Label (e.g. (111)):", "ラベル(例: (111)):");
        reg("Matrix: rows = alpha (Y coordinates), columns = beta (X coordinates)",
            "行列: 行 = α(傾き)、列 = β(回転)");
        reg("Intensity", "強度");
        reg("The pole figure needs numeric alpha, beta and intensity columns.",
            "極点図には数値の α・β・強度の列が必要です。");
        reg("The data are not on a regular alpha-beta grid.", "データが α-β の格子上にありません。");
        reg("Alpha must lie between 0° and 90°.", "α は 0°〜90° の範囲で指定してください。");
        reg("The mean intensity is not positive, so the MRD cannot be computed.",
            "平均強度が正でないため、MRD を計算できません。");
        reg("Projected pole figure of %1 (%2)", "%1 の極点図の投影 (%2)");
        reg("measured alpha <= %1°", "測定範囲 α ≤ %1°");
    });
}

QString trE11(const char *s)
{
    registerE11Translations();
    return QObject::tr(s);
}

QString trE11Count()
{
    registerE11Translations();
    return QCoreApplication::translate("ExtraPlotTypes", "Count");
}

bool isHeadless(const ApplicationWindow *app)
{
    if (qApp && qApp->property("torigraph_noninteractive").toBool())
        return true;
    return app && app->isHeadlessMode();
}

Column *resolveColumn(Table *t, const QString &spec)
{
    if (!t || spec.trimmed().isEmpty())
        return nullptr;
    if (Column *c = t->column(spec.trimmed()))
        return c;
    const QString prefix = t->name() + QStringLiteral("_");
    if (spec.startsWith(prefix))
        return t->column(spec.mid(prefix.size()));
    return nullptr;
}

QString fullColumnName(Table *t, Column *c)
{
    return (t && c) ? t->name() + QStringLiteral("_") + c->name() : QString();
}

bool readNumber(const Column *col, int row, double &val)
{
    if (!col || row < 0 || row >= col->rowCount() || col->isInvalid(row) || col->isMasked(row))
        return false;
    if (col->columnMode() != SciDAVis::ColumnMode::Numeric)
        return false;
    const double v = col->valueAt(row);
    if (!std::isfinite(v))
        return false;
    val = v;
    return true;
}

QString columnAxisTitle(Column *c, const Graph *g = nullptr)
{
    if (!c)
        return QString();
    const Column::UnitFormat fmt = g ? g->axisUnitFormat() : Column::UnitFormat::ISIJSlash;
    if (!c->longName().trimmed().isEmpty())
        return c->formattedAxisTitle(fmt);
    QString t = c->comment().section(QLatin1Char('\n'), 0, 0).trimmed();
    if (!t.isEmpty()) {
        const QString u = c->units().trimmed();
        if (!u.isEmpty() && !t.contains(u)) {
            if (fmt == Column::UnitFormat::ISIJSlash)
                t += QStringLiteral(" / ") + u;
            else
                t += QStringLiteral(" (") + u + QStringLiteral(")");
        }
        return t;
    }
    return c->formattedAxisTitle(fmt);
}

QList<Table *> projectTables(ApplicationWindow *app)
{
    QList<Table *> list;
    if (!app)
        return list;
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("Table"))
            list.append(static_cast<Table *>(w));
    }
    return list;
}

Table *resolveSourceTableFromGraph(ApplicationWindow *app, Graph *g)
{
    if (!g)
        return nullptr;
    const QString selTitle = g->selectedCurveTitle();
    if (!selTitle.isEmpty()) {
        if (auto *dc = dynamic_cast<DataCurve *>(g->curve(selTitle))) {
            if (dc->table())
                return dc->table();
        }
    }
    Table *fallbackTbl = nullptr;
    for (int i = 0; i < g->curves(); ++i) {
        auto *dc = dynamic_cast<DataCurve *>(g->curve(i));
        if (!dc || !dc->table())
            continue;
        const QString tName = dc->table()->name();
        if (!tName.startsWith(QLatin1String("GadgetFit_")))
            return dc->table();
        if (!fallbackTbl)
            fallbackTbl = dc->table();
    }
    if (fallbackTbl)
        return fallbackTbl;
    if (app) {
        for (const QString &kind : { QStringLiteral("ToriDensity2D"), QStringLiteral("ToriPoleFigure") }) {
            const QString tag = ExtraPlotTypes::graphTag(g, kind);
            if (!tag.isEmpty()) {
                static const QRegularExpression rxTbl(QStringLiteral("table=\"([^\"]+)\""));
                const QRegularExpressionMatch m = rxTbl.match(tag);
                if (m.hasMatch()) {
                    if (Table *t = app->table(m.captured(1)))
                        return t;
                }
            }
        }
    }
    return nullptr;
}

Table *activeTable(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    if (QMdiSubWindow *sub = app->d_workspace.activeSubWindow()) {
        if (sub->inherits("Table"))
            return static_cast<Table *>(sub);
        if (QWidget *w = sub->widget()) {
            if (w->inherits("Table"))
                return static_cast<Table *>(w);
        }
        MultiLayer *ml = nullptr;
        if (sub->inherits("MultiLayer"))
            ml = static_cast<MultiLayer *>(sub);
        else if (sub->widget() && sub->widget()->inherits("MultiLayer"))
            ml = static_cast<MultiLayer *>(sub->widget());
        if (ml) {
            if (Table *gt = resolveSourceTableFromGraph(app, ml->activeGraph()))
                return gt;
            for (Graph *lg : ml->layersList()) {
                if (Table *gt = resolveSourceTableFromGraph(app, lg))
                    return gt;
            }
        }
    }
    const QList<Table *> t = projectTables(app);
    return t.isEmpty() ? nullptr : t.first();
}

Graph *activeGraph(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    QMdiSubWindow *sub = app->d_workspace.activeSubWindow();
    MultiLayer *ml = nullptr;
    if (sub) {
        if (sub->inherits("MultiLayer"))
            ml = static_cast<MultiLayer *>(sub);
        else if (sub->widget() && sub->widget()->inherits("MultiLayer"))
            ml = static_cast<MultiLayer *>(sub->widget());
    }
    return ml ? ml->activeGraph() : nullptr;
}

void fillPresetCombo(QComboBox *cb)
{
    struct P {
        const char *name;
        int preset;
    };
    static const P kPresets[] = { { "Viridis", Spectrogram::PresetViridis },
                                  { "Plasma", Spectrogram::PresetPlasma },
                                  { "Inferno", Spectrogram::PresetInferno },
                                  { "Magma", Spectrogram::PresetMagma },
                                  { "Cividis", Spectrogram::PresetCividis },
                                  { "Jet", Spectrogram::PresetJet },
                                  { "Cool-Warm", Spectrogram::PresetCoolWarm },
                                  { "Blue-White-Red", Spectrogram::PresetBlueWhiteRed },
                                  { "Gray", Spectrogram::PresetGrayScale } };
    for (const P &p : kPresets)
        cb->addItem(QString::fromLatin1(p.name), p.preset);
}

Legend *addSummaryBox(Graph *g, double x, double y, const QString &text)
{
    if (!g)
        return nullptr;
    Legend *box = g->newLegend(text);
    if (!box)
        return nullptr;
    QFont f(QStringLiteral("Arial"), 9, QFont::Normal);
    box->setFrameStyle(Legend::Line);
    box->setBackgroundColor(QColor(255, 255, 255, 232));
    box->setTextColor(QColor(20, 30, 50));
    box->setFont(f);
    box->setOriginCoord(x, y);
    return box;
}

void refreshLegend(Graph *g)
{
    if (g && g->legend())
        g->legend()->setText(g->legendText());
}

//! Action owner: keeps the E11 actions translated when the UI language switches.
class E11ActionHub : public QObject
{
public:
    explicit E11ActionHub(ApplicationWindow *app) : QObject(app), d_app(app)
    {
        setObjectName(QStringLiteral("E11ActionHub"));
        if (qApp)
            qApp->installEventFilter(this);
    }
    QAction *actions[ExtraPlotTypes::ActCount] = {};

    void retranslate()
    {
        static const char *const kText[ExtraPlotTypes::ActCount] = {
            "Add Error &Band...", "Color-Mapped &Scatter (Z)...", "Histogram + &Cumulative %...",
            "&Lognormal Probability Plot...", "&Gumbel (Extreme Value) Probability Plot...",
            "2D &Density (2D Histogram)...", "&Pole Figure (XRD)..."
        };
        static const char *const kTip[ExtraPlotTypes::ActCount] = {
            "Shade the area between Y - error and Y + error from error columns",
            "Scatter plot whose symbol color follows a third column (and size a fourth), with a color bar",
            "Frequency histogram with the cumulative percentage on the right axis",
            "Lognormal probability paper: rank plotting positions and a straight-line fit (mu, sigma)",
            "Gumbel extreme value plot (e.g. maximum defect size): location, scale and return-period value",
            "Count X-Y points on a grid and show the counts as a heat map with a color bar (fast for 100,000+ points)",
            "XRD pole figure from alpha, beta, intensity columns or an alpha x beta matrix: equal-area (Schmidt) or equal-angle (Wulff) projection with contours"
        };
        for (int i = 0; i < ExtraPlotTypes::ActCount; ++i) {
            if (!actions[i])
                continue;
            actions[i]->setText(trE11(kText[i]));
            actions[i]->setToolTip(trE11(kTip[i]));
            actions[i]->setStatusTip(trE11(kTip[i]));
        }
    }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (ev && ev->type() == QEvent::LanguageChange && obj == qApp)
            retranslate();
        return QObject::eventFilter(obj, ev);
    }

private:
    QPointer<ApplicationWindow> d_app;
};

E11ActionHub *actionHub(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    if (auto *hub = app->findChild<E11ActionHub *>(QStringLiteral("E11ActionHub"),
                                                    Qt::FindDirectChildrenOnly))
        return hub;
    return new E11ActionHub(app);
}

//! Least-squares line u = a + b v.
bool fitLine(const QVector<double> &v, const QVector<double> &u, double &a, double &b, double &r2)
{
    const int n = qMin(v.size(), u.size());
    if (n < 2)
        return false;
    double mv = 0.0, mu = 0.0;
    for (int i = 0; i < n; ++i) {
        mv += v[i];
        mu += u[i];
    }
    mv /= n;
    mu /= n;
    double svv = 0.0, suv = 0.0, suu = 0.0;
    for (int i = 0; i < n; ++i) {
        svv += (v[i] - mv) * (v[i] - mv);
        suv += (v[i] - mv) * (u[i] - mu);
        suu += (u[i] - mu) * (u[i] - mu);
    }
    if (!(svv > 0.0) || !(suu > 0.0))
        return false;
    b = suv / svv;
    a = mu - b * mv;
    r2 = (suv * suv) / (svv * suu);
    return true;
}

double probabilityVariate(ExtraPlotTypes::ProbabilityDistribution dist, double p)
{
    if (dist == ExtraPlotTypes::LogNormal)
        return gsl_cdf_ugaussian_Pinv(p);
    return -std::log(-std::log(p));
}

} // namespace

// ============================================================================
// Actions & translations
// ============================================================================

void ExtraPlotTypes::registerTranslations()
{
    registerE11Translations();
}

QAction *ExtraPlotTypes::action(ApplicationWindow *app, ActionId id)
{
    if (!app || id < 0 || id >= ActCount)
        return nullptr;
    registerE11Translations();
    E11ActionHub *hub = actionHub(app);
    if (hub->actions[id])
        return hub->actions[id];
    static const char *const kObj[ActCount] = { "actionE11ErrorBand", "actionE11ColorScatter",
                                                "actionE11HistogramCumulative",
                                                "actionE11LognormalProbability",
                                                "actionE11GumbelProbability",
                                                "actionE11Density2D", "actionE11PoleFigure" };
    QAction *a = new QAction(app);
    a->setObjectName(QString::fromLatin1(kObj[id]));
    QPointer<ApplicationWindow> pApp(app);
    QObject::connect(a, &QAction::triggered, app, [pApp, id]() {
        if (!pApp)
            return;
        switch (id) {
        case ActErrorBand:
            showErrorBandDialog(pApp);
            break;
        case ActColorScatter:
            showColorScatterDialog(pApp);
            break;
        case ActHistogramCumulative:
            showHistogramCumulativeDialog(pApp);
            break;
        case ActLognormalProbability:
            showProbabilityPlotDialog(pApp, LogNormal);
            break;
        case ActGumbelProbability:
            showProbabilityPlotDialog(pApp, Gumbel);
            break;
        case ActDensity2D:
            showDensity2DDialog(pApp);
            break;
        case ActPoleFigure:
            showPoleFigureDialog(pApp);
            break;
        default:
            break;
        }
    });
    hub->actions[id] = a;
    hub->retranslate();
    return a;
}

// ============================================================================
// 1. Error band
// ============================================================================

int ExtraPlotTypes::addErrorBand(Graph *g, const QString &yColumn, Table *errTable,
                                 const QString &plusErrCol, const QString &minusErrCol, int alpha,
                                 const QColor &color)
{
    if (!g || !errTable || yColumn.isEmpty())
        return 0;
    DataCurve *master = nullptr;
    for (int i = 0; i < g->curves(); ++i) {
        if (g->curveType(i) == Graph::ErrorBars)
            continue;
        auto *dc = dynamic_cast<DataCurve *>(g->curve(i));
        if (dc && dc->table() && dc->title().text() == yColumn) {
            master = dc;
            break;
        }
    }
    if (!master)
        return 0;
    Column *cPlus = resolveColumn(errTable, plusErrCol);
    Column *cMinus = minusErrCol.isEmpty() ? nullptr : resolveColumn(errTable, minusErrCol);
    if (!cPlus || (!minusErrCol.isEmpty() && !cMinus))
        return 0;
    const QColor col = color.isValid() ? color : master->pen().color();
    const QString xName = master->xColumnName();

    auto addOne = [&](Column *c, bool plus, bool minus) -> bool {
        const int before = g->curves();
        if (!g->addErrorBars(xName, yColumn, errTable, fullColumnName(errTable, c),
                             QwtErrorPlotCurve::Vertical, 1, 0, col, false, minus, plus))
            return false;
        if (g->curves() <= before)
            return false;
        auto *er = dynamic_cast<QwtErrorPlotCurve *>(g->curve(g->curves() - 1));
        if (!er)
            return false;
        er->setBand(true);
        er->setBandAlpha(alpha);
        return true;
    };

    int added = 0;
    if (!cMinus) {
        added += addOne(cPlus, true, true) ? 1 : 0;
    } else {
        added += addOne(cPlus, true, false) ? 1 : 0;
        added += addOne(cMinus, false, true) ? 1 : 0;
    }
    if (added > 0) {
        g->syncCurveZOrder();
        refreshLegend(g);
        g->replot();
        emit g->modifiedGraph();
    }
    return added;
}

void ExtraPlotTypes::showErrorBandDialog(ApplicationWindow *app)
{
    if (!app || isHeadless(app))
        return;
    registerE11Translations();
    Graph *g = activeGraph(app);
    QList<DataCurve *> curves;
    if (g) {
        for (int i = 0; i < g->curves(); ++i) {
            if (g->curveType(i) == Graph::ErrorBars)
                continue;
            auto *dc = dynamic_cast<DataCurve *>(g->curve(i));
            if (dc && dc->table())
                curves << dc;
        }
    }
    if (curves.isEmpty()) {
        QMessageBox::information(app, trE11("Error Band"),
                                 trE11("The active graph has no data curve to add an error band to."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(trE11("Error Band"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();
    auto *cbCurve = new QComboBox(&dlg);
    for (DataCurve *dc : curves)
        cbCurve->addItem(dc->title().text());
    auto *cbMode = new QComboBox(&dlg);
    cbMode->addItem(trE11("Symmetric (one ± column)"));
    cbMode->addItem(trE11("Asymmetric (+ and − columns)"));
    auto *cbPlus = new QComboBox(&dlg);
    auto *cbMinus = new QComboBox(&dlg);
    auto *sbAlpha = new QSpinBox(&dlg);
    sbAlpha->setRange(5, 90);
    sbAlpha->setValue(25);

    auto refreshCols = [&]() {
        cbPlus->clear();
        cbMinus->clear();
        const int idx = cbCurve->currentIndex();
        if (idx < 0 || idx >= curves.size())
            return;
        Table *t = curves[idx]->table();
        const QString yName = curves[idx]->title().text();
        int prefer = -1;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!col || col->columnMode() != SciDAVis::ColumnMode::Numeric)
                continue;
            const QString full = fullColumnName(t, col);
            if (full == yName || full == curves[idx]->xColumnName())
                continue;
            cbPlus->addItem(col->name(), full);
            cbMinus->addItem(col->name(), full);
            if (prefer < 0 && col->plotDesignation() == SciDAVis::yErr)
                prefer = cbPlus->count() - 1;
        }
        if (prefer >= 0)
            cbPlus->setCurrentIndex(prefer);
        if (cbMinus->count() > 1)
            cbMinus->setCurrentIndex(qMin(cbPlus->currentIndex() + 1, cbMinus->count() - 1));
    };
    refreshCols();
    QObject::connect(cbCurve, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refreshCols(); });
    auto syncMode = [&]() { cbMinus->setEnabled(cbMode->currentIndex() == 1); };
    QObject::connect(cbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { syncMode(); });
    syncMode();

    form->addRow(trE11("Curve:"), cbCurve);
    form->addRow(trE11("Error columns:"), cbMode);
    form->addRow(trE11("± or + error column:"), cbPlus);
    form->addRow(trE11("− error column:"), cbMinus);
    form->addRow(trE11("Fill opacity (%):"), sbAlpha);
    lay->addLayout(form);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const int idx = cbCurve->currentIndex();
    if (idx < 0 || idx >= curves.size() || cbPlus->currentIndex() < 0)
        return;
    const QString minusCol =
            cbMode->currentIndex() == 1 ? cbMinus->currentData().toString() : QString();
    const int alpha = qRound(sbAlpha->value() * 255.0 / 100.0);
    MultiLayer *ml = g ? g->multiLayerParent() : nullptr;
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    if (addErrorBand(g, curves[idx]->title().text(), curves[idx]->table(),
                     cbPlus->currentData().toString(), minusCol, alpha)
        == 0) {
        QMessageBox::warning(app, trE11("Error Band"),
                             trE11("The error band could not be added (check the error columns)."));
    } else {
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(trE11("Add Error Band"), beforeSnap);
        app->modifiedProject();
        app->updateUndoRedoActionsForActiveWindow();
    }
}

// ============================================================================
// 2. Colour-mapped scatter
// ============================================================================

MultiLayer *ExtraPlotTypes::plotColorMappedScatter(ApplicationWindow *app, Table *t,
                                                   const QString &xCol, const QString &yCol,
                                                   const QString &zCol, const QString &sizeCol,
                                                   int preset, bool reverse)
{
    registerE11Translations();
    if (!app || !t)
        return nullptr;
    Column *cx = resolveColumn(t, xCol);
    Column *cy = resolveColumn(t, yCol);
    Column *cz = resolveColumn(t, zCol);
    Column *cs = sizeCol.isEmpty() ? nullptr : resolveColumn(t, sizeCol);
    auto numeric = [](Column *c) {
        return c && c->columnMode() == SciDAVis::ColumnMode::Numeric;
    };
    if (!numeric(cx) || !numeric(cy) || !numeric(cz))
        return nullptr;

    MultiLayer *ml = app->newGraph(
            app->generateUniqueName(QStringLiteral("ColorScatter_") + t->name(), false));
    Graph *g = ml ? ml->activeGraph() : nullptr;
    if (!g)
        return ml;
    const QString xFull = fullColumnName(t, cx);
    const QString yFull = fullColumnName(t, cy);
    if (!g->insertCurve(t, xFull, yFull, Graph::Scatter))
        return ml;
    const int idx = g->curves() - 1;
    auto *dc = dynamic_cast<DataCurve *>(g->curve(idx));
    if (!dc)
        return ml;
    QwtSymbol sym = dc->symbol();
    if (sym.style() == QwtSymbol::NoSymbol)
        sym.setStyle(QwtSymbol::Ellipse);
    sym.setSize(QSize(8, 8));
    // Legend swatch: the middle colour of the map.
    const QwtLinearColorMap map =
            Spectrogram::presetColorMap(static_cast<Spectrogram::ColorMapPreset>(preset), reverse);
    const QColor mid(map.rgb(QwtDoubleInterval(0.0, 1.0), 0.5));
    sym.setBrush(QBrush(mid));
    sym.setPen(QPen(mid.darker(150), 1.0));
    g->setCurveSymbol(idx, sym);

    DataCurve::PointColorMap pm;
    pm.enabled = true;
    pm.zColumn = fullColumnName(t, cz);
    pm.sizeColumn = cs ? fullColumnName(t, cs) : QString();
    pm.preset = preset;
    pm.reverse = reverse;
    pm.autoRange = true;
    pm.colorBarTitle = columnAxisTitle(cz, g);
    dc->setPointColorMap(pm);

    g->setXAxisTitle(columnAxisTitle(cx, g));
    g->setYAxisTitle(columnAxisTitle(cy, g));
    g->syncPointColorBar();
    refreshLegend(g);
    g->replot();
    emit g->modifiedGraph();
    return ml;
}

void ExtraPlotTypes::showColorScatterDialog(ApplicationWindow *app)
{
    if (!app || isHeadless(app))
        return;
    registerE11Translations();
    const QList<Table *> tables = projectTables(app);
    if (tables.isEmpty())
        return;
    Table *act = activeTable(app);

    QDialog dlg(app);
    dlg.setWindowTitle(trE11("Color-Mapped Scatter (Z)"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();
    auto *cbTable = new QComboBox(&dlg);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == act)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }
    auto *cbX = new QComboBox(&dlg);
    auto *cbY = new QComboBox(&dlg);
    auto *cbZ = new QComboBox(&dlg);
    auto *cbS = new QComboBox(&dlg);
    auto *cbMap = new QComboBox(&dlg);
    fillPresetCombo(cbMap);
    auto *chkRev = new QCheckBox(trE11("Reverse color map"), &dlg);

    auto refresh = [&]() {
        cbX->clear();
        cbY->clear();
        cbZ->clear();
        cbS->clear();
        cbS->addItem(trE11("(None)"), QString());
        const int ti = cbTable->currentIndex();
        if (ti < 0 || ti >= tables.size())
            return;
        Table *t = tables[ti];
        QStringList numericCols;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!col || col->columnMode() != SciDAVis::ColumnMode::Numeric)
                continue;
            numericCols << col->name();
            cbX->addItem(col->name());
            cbY->addItem(col->name());
            cbZ->addItem(col->name());
            cbS->addItem(col->name(), col->name());
        }
        // Pre-select from the column selection (X, Y, Z[, size]) or the first columns.
        QStringList sel;
        for (const QString &full : t->selectedColumns()) {
            const QString nm = full.startsWith(t->name() + QStringLiteral("_"))
                    ? full.mid(t->name().size() + 1)
                    : full;
            if (numericCols.contains(nm))
                sel << nm;
        }
        if (sel.size() < 3)
            sel = numericCols;
        if (sel.size() > 0)
            cbX->setCurrentText(sel[0]);
        if (sel.size() > 1)
            cbY->setCurrentText(sel[1]);
        if (sel.size() > 2)
            cbZ->setCurrentText(sel[2]);
        if (sel.size() > 3 && sel != numericCols)
            cbS->setCurrentText(sel[3]);
    };
    refresh();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refresh(); });

    form->addRow(trE11("Source Table:"), cbTable);
    form->addRow(trE11("X column:"), cbX);
    form->addRow(trE11("Y column:"), cbY);
    form->addRow(trE11("Color (Z) column:"), cbZ);
    form->addRow(trE11("Size column (optional):"), cbS);
    form->addRow(trE11("Color map:"), cbMap);
    form->addRow(QString(), chkRev);
    lay->addLayout(form);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted)
        return;
    Table *t = tables.value(cbTable->currentIndex());
    if (!plotColorMappedScatter(app, t, cbX->currentText(), cbY->currentText(),
                                cbZ->currentText(), cbS->currentData().toString(),
                                cbMap->currentData().toInt(), chkRev->isChecked())) {
        QMessageBox::warning(app, trE11("Color-Mapped Scatter (Z)"),
                             trE11("The colour-mapped scatter needs numeric X, Y and Z columns."));
    }
}

// ============================================================================
// 3. Histogram + cumulative %
// ============================================================================

ExtraPlotTypes::HistogramCumulativeResult
ExtraPlotTypes::plotHistogramCumulative(ApplicationWindow *app, Table *t, const QString &col,
                                        double binWidth, double begin)
{
    registerE11Translations();
    HistogramCumulativeResult res;
    Column *c = resolveColumn(t, col);
    if (!app || !t || !c || c->columnMode() != SciDAVis::ColumnMode::Numeric) {
        res.error = trE11("No numeric data in the selected column.");
        return res;
    }
    double vMin = 0.0, vMax = 0.0;
    int nValid = 0;
    for (int r = 0; r < c->rowCount(); ++r) {
        double v = 0.0;
        if (!readNumber(c, r, v))
            continue;
        vMin = nValid ? qMin(vMin, v) : v;
        vMax = nValid ? qMax(vMax, v) : v;
        ++nValid;
    }
    if (nValid == 0) {
        res.error = trE11("No numeric data in the selected column.");
        return res;
    }

    constexpr int kMaxBins = 10000;
    if (binWidth > 0.0 && std::isfinite(binWidth)) {
        const double effBegin = (!std::isfinite(begin) || begin > vMin)
                ? std::floor(vMin / binWidth) * binWidth
                : begin;
        const double span = (vMax - effBegin) / binWidth;
        if (!std::isfinite(effBegin) || !std::isfinite(span) || !(span < double(kMaxBins))) {
            res.error = trE11("Too many bins (at most 10,000); use a larger bin width.");
            return res;
        }
    }

    MultiLayer *ml = app->newGraph(
            app->generateUniqueName(QStringLiteral("HistCum_") + t->name(), false));
    auto closeEmptyGraph = [&]() {
        if (ml) {
            ml->askOnCloseEvent(false);
            ml->close();
            ml = nullptr;
        }
        res.plot = nullptr;
    };
    Graph *g = ml ? ml->activeGraph() : nullptr;
    res.plot = ml;
    if (!g) {
        closeEmptyGraph();
        return res;
    }
    const QString full = fullColumnName(t, c);
    if (!g->plotHistogram(t, QStringList(full))) {
        closeEmptyGraph();
        return res;
    }
    auto *h = dynamic_cast<QwtHistogram *>(g->curve(g->curves() - 1));
    if (!h) {
        closeEmptyGraph();
        return res;
    }
    if (binWidth > 0.0 && std::isfinite(binWidth)) {
        if (!std::isfinite(begin) || begin > vMin)
            begin = std::floor(vMin / binWidth) * binWidth;
        int nb = qMax(1, int(std::ceil((vMax - begin) / binWidth)));
        if (begin + nb * binWidth <= vMax)
            ++nb;
        if (nb > kMaxBins) {
            res.error = trE11("Too many bins (at most 10,000); use a larger bin width.");
            closeEmptyGraph();
            return res;
        }
        h->setBinning(false, binWidth, begin, begin + nb * binWidth);
        h->loadData();
    }

    const int bins = h->dataSize();
    if (bins <= 0 || bins > kMaxBins) {
        if (bins > kMaxBins)
            res.error = trE11("Too many bins (at most 10,000); use a larger bin width.");
        closeEmptyGraph();
        return res;
    }
    double w = h->binSize();
    if (!(w > 0.0) && bins >= 2)
        w = h->x(1) - h->x(0);
    double total = 0.0;
    for (int i = 0; i < bins; ++i)
        total += h->y(i);
    if (!(total > 0.0)) {
        closeEmptyGraph();
        return res;
    }

    res.bins = bins;
    res.binWidth = w;
    res.begin = h->x(0);
    res.total = int(std::lround(total));
    res.upperEdges.resize(bins + 1);
    res.cumulativePct.resize(bins + 1);
    res.upperEdges[0] = h->x(0);
    res.cumulativePct[0] = 0.0;
    double acc = 0.0;
    for (int i = 0; i < bins; ++i) {
        acc += h->y(i);
        res.upperEdges[i + 1] = h->x(i) + w;
        res.cumulativePct[i + 1] = (i == bins - 1) ? 100.0 : 100.0 * acc / total;
    }

    Table *tbl = app->newTable(bins + 1, 3,
                               app->generateUniqueName(t->name() + QStringLiteral("_HistCum"),
                                                       false),
                               trE11("Histogram + Cumulative %"));
    res.table = tbl;
    if (!tbl) {
        closeEmptyGraph();
        return res;
    }
    tbl->setHeader(QStringList() << QStringLiteral("UpperEdge") << QStringLiteral("CumCount")
                                 << QStringLiteral("CumulativePct"));
    for (int k = 0; k < 3; ++k)
        tbl->column(k)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    tbl->column(0)->setPlotDesignation(SciDAVis::X);
    tbl->column(1)->setPlotDesignation(SciDAVis::Y);
    tbl->column(2)->setPlotDesignation(SciDAVis::Y);
    tbl->column(2)->setLongName(trE11("Cumulative (%)"));
    acc = 0.0;
    for (int i = 0; i <= bins; ++i) {
        if (i > 0)
            acc += h->y(i - 1);
        tbl->column(0)->setValueAt(i, res.upperEdges[i]);
        tbl->column(1)->setValueAt(i, acc);
        tbl->column(2)->setValueAt(i, res.cumulativePct[i]);
    }

    if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(2), Graph::LineSymbols, 0, bins)) {
        const int idx = g->curves() - 1;
        QwtPlotCurve *cc = g->curve(idx);
        g->setCurvePen(idx, QPen(QColor(205, 35, 45), 1.8));
        g->setCurveSymbol(idx, QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(205, 35, 45)),
                                         QPen(QColor(150, 20, 30), 1.0), QSize(6, 6)));
        if (cc)
            cc->setAxis(QwtPlot::xBottom, QwtPlot::yRight);
    }
    g->enableAxis(QwtPlot::yRight, true);
    if (QwtScaleDraw *sd = g->plotWidget()->axisScaleDraw(QwtPlot::yRight))
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    g->setScale(QwtPlot::yRight, 0.0, 100.0, 20.0, 5, 1);
    g->setAxisTitle(QwtPlot::yRight, trE11("Cumulative (%)"));
    g->setXAxisTitle(columnAxisTitle(c, g));
    g->setYAxisTitle(trE11Count());
    refreshLegend(g);
    g->replot();
    emit g->modifiedGraph();
    res.valid = true;
    return res;
}

void ExtraPlotTypes::showHistogramCumulativeDialog(ApplicationWindow *app)
{
    if (!app || isHeadless(app))
        return;
    registerE11Translations();
    const QList<Table *> tables = projectTables(app);
    if (tables.isEmpty())
        return;
    Table *act = activeTable(app);
    QDialog dlg(app);
    dlg.setWindowTitle(trE11("Histogram + Cumulative %"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();
    auto *cbTable = new QComboBox(&dlg);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == act)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }
    auto *cbCol = new QComboBox(&dlg);
    auto *sbW = new QDoubleSpinBox(&dlg);
    sbW->setDecimals(6);
    sbW->setRange(0.0, 1e12);
    sbW->setValue(0.0);
    auto *chkAutoBegin = new QCheckBox(trE11("Automatic (from data minimum)"), &dlg);
    chkAutoBegin->setChecked(true);
    auto *sbBegin = new QDoubleSpinBox(&dlg);
    sbBegin->setDecimals(6);
    sbBegin->setRange(-1e12, 1e12);
    sbBegin->setValue(0.0);
    sbBegin->setEnabled(false);
    auto *beginRow = new QHBoxLayout();
    beginRow->setContentsMargins(0, 0, 0, 0);
    beginRow->addWidget(chkAutoBegin);
    beginRow->addWidget(sbBegin, 1);

    auto syncMinVal = [&]() {
        Table *t = tables.value(cbTable->currentIndex());
        Column *c = t ? resolveColumn(t, cbCol->currentText()) : nullptr;
        if (!c)
            return;
        double vMin = 0.0;
        bool hasAny = false;
        for (int r = 0; r < c->rowCount(); ++r) {
            double v = 0.0;
            if (readNumber(c, r, v)) {
                vMin = hasAny ? qMin(vMin, v) : v;
                hasAny = true;
            }
        }
        if (hasAny && chkAutoBegin->isChecked())
            sbBegin->setValue(vMin);
    };
    auto refresh = [&]() {
        cbCol->clear();
        Table *t = tables.value(cbTable->currentIndex());
        if (!t)
            return;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (col && col->columnMode() == SciDAVis::ColumnMode::Numeric)
                cbCol->addItem(col->name());
        }
        const QStringList sel = t->selectedColumns();
        if (!sel.isEmpty())
            cbCol->setCurrentText(sel.first().mid(t->name().size() + 1));
        syncMinVal();
    };
    refresh();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refresh(); });
    QObject::connect(cbCol, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { syncMinVal(); });
    QObject::connect(chkAutoBegin, &QCheckBox::toggled, &dlg, [&](bool on) {
        sbBegin->setEnabled(!on);
        if (on)
            syncMinVal();
    });

    form->addRow(trE11("Source Table:"), cbTable);
    form->addRow(trE11("Data column:"), cbCol);
    form->addRow(trE11("Bin width (0 = automatic):"), sbW);
    form->addRow(trE11("Bin start:"), beginRow);
    lay->addLayout(form);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const double beginParam = chkAutoBegin->isChecked()
            ? std::numeric_limits<double>::quiet_NaN()
            : sbBegin->value();
    const HistogramCumulativeResult r = plotHistogramCumulative(
            app, tables.value(cbTable->currentIndex()), cbCol->currentText(), sbW->value(),
            beginParam);
    if (!r.valid)
        QMessageBox::warning(app, trE11("Histogram + Cumulative %"),
                             r.error.isEmpty() ? trE11("No numeric data in the selected column.")
                                               : r.error);
}

// ============================================================================
// 4. Lognormal / Gumbel probability plots
// ============================================================================

double ExtraPlotTypes::plottingPosition(int i, int n, PlottingPosition pos)
{
    const double di = double(i);
    const double dn = double(n);
    switch (pos) {
    case MeanRank:
        return di / (dn + 1.0);
    case Hazen:
        return (di - 0.5) / dn;
    case Blom:
        return (di - 0.375) / (dn + 0.25);
    case MedianRank:
    default:
        return (di - 0.3) / (dn + 0.4);
    }
}

ExtraPlotTypes::ProbabilityPlotResult
ExtraPlotTypes::plotProbability(ApplicationWindow *app, Table *t, const QString &col,
                                ProbabilityDistribution dist, PlottingPosition pos,
                                double returnPeriod, bool createPlot)
{
    registerE11Translations();
    ProbabilityPlotResult res;
    Column *c = resolveColumn(t, col);
    if (!t || !c) {
        res.error = trE11("No numeric data in the selected column.");
        return res;
    }
    QVector<double> vals;
    for (int r = 0; r < c->rowCount(); ++r) {
        double v = 0.0;
        if (!readNumber(c, r, v))
            continue;
        if (dist == LogNormal && v <= 0.0) {
            ++res.excluded;
            continue;
        }
        vals << v;
    }
    std::sort(vals.begin(), vals.end());
    const int n = vals.size();
    res.n = n;
    if (n < 3) {
        res.error = trE11("At least 3 values are needed; only %1 remained.").arg(n);
        return res;
    }
    QVector<double> var(n), u(n), F(n);
    for (int i = 0; i < n; ++i) {
        F[i] = plottingPosition(i + 1, n, pos);
        var[i] = probabilityVariate(dist, F[i]);
        u[i] = (dist == LogNormal) ? std::log(vals[i]) : vals[i];
    }
    double a = 0.0, b = 0.0, r2 = 0.0;
    if (!fitLine(var, u, a, b, r2)) {
        res.error = trE11("All values are identical, so no line can be fitted.");
        return res;
    }
    res.location = a;
    res.scale = b;
    res.rSquare = r2;
    res.sorted = vals;
    res.variate = var;
    if (dist == Gumbel && returnPeriod > 1.0) {
        res.returnPeriod = returnPeriod;
        res.returnValue = a + b * (-std::log(-std::log(1.0 - 1.0 / returnPeriod)));
    }
    res.valid = true;
    if (!app)
        return res;

    // Report table: data, rank, F, variate + a two-point fitted line
    const double vPad = 0.08 * (var[n - 1] - var[0]) + 0.15;
    const double vLo = var[0] - vPad;
    const double vHi = var[n - 1] + vPad;
    const QString suffix = dist == LogNormal ? QStringLiteral("_LognormalProb")
                                             : QStringLiteral("_GumbelProb");
    Table *tbl = app->newTable(n, 6, app->generateUniqueName(t->name() + suffix, false),
                               dist == LogNormal ? trE11("Lognormal Probability Plot")
                                                 : trE11("Gumbel (Extreme Value) Probability Plot"));
    res.table = tbl;
    if (!tbl)
        return res;
    tbl->setHeader(QStringList() << QStringLiteral("Value") << QStringLiteral("Rank")
                                 << QStringLiteral("F_Pct")
                                 << (dist == LogNormal ? QStringLiteral("Probit_z")
                                                       : QStringLiteral("ReducedVariate_y"))
                                 << QStringLiteral("FitValue") << QStringLiteral("FitVariate"));
    for (int k = 0; k < 6; ++k)
        tbl->column(k)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    tbl->column(0)->setPlotDesignation(SciDAVis::X);
    tbl->column(1)->setPlotDesignation(SciDAVis::noDesignation);
    tbl->column(2)->setPlotDesignation(SciDAVis::noDesignation);
    tbl->column(3)->setPlotDesignation(SciDAVis::Y);
    tbl->column(4)->setPlotDesignation(SciDAVis::X);
    tbl->column(5)->setPlotDesignation(SciDAVis::Y);
    QString seriesDisplayName = c->longName().trimmed();
    if (seriesDisplayName.isEmpty())
        seriesDisplayName = c->comment().section(QLatin1Char('\n'), 0, 0).trimmed();
    if (seriesDisplayName.isEmpty())
        seriesDisplayName = c->name().trimmed();
    const QString fitDisplayName =
            (dist == LogNormal) ? trE11("Lognormal fit") : trE11("Gumbel fit");
    tbl->column(3)->setLongName(seriesDisplayName);
    tbl->column(5)->setLongName(fitDisplayName);

    for (int i = 0; i < n; ++i) {
        tbl->column(0)->setValueAt(i, vals[i]);
        tbl->column(1)->setValueAt(i, i + 1);
        tbl->column(2)->setValueAt(i, 100.0 * F[i]);
        tbl->column(3)->setValueAt(i, var[i]);
    }
    const double fitV[2] = { var[0] - 0.6 * vPad, var[n - 1] + 0.6 * vPad };
    for (int k = 0; k < 2; ++k) {
        const double uu = a + b * fitV[k];
        const double fitVal = (dist == LogNormal) ? std::exp(qBound(-700.0, uu, 700.0)) : uu;
        tbl->column(4)->setValueAt(k, fitVal);
        tbl->column(5)->setValueAt(k, fitV[k]);
    }
    if (!createPlot)
        return res;

    MultiLayer *ml = app->newGraph(app->generateUniqueName(
            (dist == LogNormal ? QStringLiteral("LognormalPlot_") : QStringLiteral("GumbelPlot_"))
                    + t->name(),
            false));
    res.plot = ml;
    Graph *g = ml ? ml->activeGraph() : nullptr;
    if (!g)
        return res;
    if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(3), Graph::Scatter, 0, n - 1)) {
        const int idx = g->curves() - 1;
        g->setCurveSymbol(idx, QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(35, 100, 190)),
                                         QPen(QColor(15, 45, 100), 1.2), QSize(7, 7)));
    }
    if (g->insertCurve(tbl, tbl->colName(4), tbl->colName(5), Graph::Line, 0, 1))
        g->setCurvePen(g->curves() - 1, QPen(QColor(205, 35, 45), 2.0));

    g->applyFramePreset(Graph::FrameBoxInwardTicks);
    double xLo = vals[0], xHi = vals[n - 1];
    if (dist == LogNormal) {
        const double l0 = std::log10(xLo), l1 = std::log10(xHi);
        const double pad = qMax(0.05, 0.08 * (l1 - l0));
        xLo = std::pow(10.0, qBound(-300.0, l0 - pad, 300.0));
        xHi = std::pow(10.0, qBound(-300.0, l1 + pad, 300.0));
        if (!(xHi > xLo) || !std::isfinite(xHi) || !(xLo > 0.0)) {
            xLo = qMax(std::numeric_limits<double>::min(), vals[0] * 0.8);
            xHi = qMax(xLo * 1.25, vals[n - 1] * 1.25);
        }
        g->setScale(QwtPlot::xBottom, xLo, xHi, 0.0, 5, 8, 1);
    } else {
        const double pad = qMax(1e-12, 0.08 * (xHi - xLo));
        xLo -= pad;
        xHi += pad;
        g->setScale(QwtPlot::xBottom, xLo, xHi);
    }
    g->setScale(QwtPlot::yLeft, vLo, vHi);

    // Probability (%) labels at the variate positions; empty end ticks keep the axis range.
    static const double kMajor[] = { 0.1, 1, 5, 10, 20, 30, 50, 70, 80, 90, 95, 99, 99.9 };
    static const double kMinor[] = { 0.01, 0.5, 2, 40, 60, 98, 99.5, 99.99 };
    QVector<double> tickPos;
    QStringList tickLbl;
    auto addLevels = [&](const double *lv, int cnt) {
        for (int k = 0; k < cnt; ++k) {
            const double q = probabilityVariate(dist, 0.01 * lv[k]);
            if (q > vLo && q < vHi && !tickPos.contains(q)) {
                tickPos << q;
                tickLbl << QString::number(lv[k], 'g', 4);
            }
        }
    };
    addLevels(kMajor, int(sizeof(kMajor) / sizeof(kMajor[0])));
    if (tickPos.size() < 5)
        addLevels(kMinor, int(sizeof(kMinor) / sizeof(kMinor[0])));
    tickPos << vLo << vHi;
    tickLbl << QString() << QString();
    g->setCustomTickLabels(QwtPlot::yLeft, tickPos, tickLbl);

    g->setXAxisTitle(columnAxisTitle(c, g));
    g->setYAxisTitle(trE11("Cumulative probability, F (%)"));
    g->setTitle(QString());
    refreshLegend(g);
    g->placeLegendAnchor(Graph::LegendTopLeft);

    static const char *const kPos[] = { "median rank", "mean rank i/(n+1)", "Hazen", "Blom" };
    QString summary;
    if (dist == LogNormal) {
        summary = QStringLiteral("Lognormal (LSQ, %1)\n"
                                 "<i>μ</i> = %2, <i>σ</i> = %3 (ln x)\n"
                                 "median = %4\n"
                                 "<i>R</i><sup>2</sup> = %5 (<i>n</i> = %6)")
                          .arg(QString::fromLatin1(kPos[qBound(0, int(pos), 3)]))
                          .arg(a, 0, 'g', 5)
                          .arg(b, 0, 'g', 4)
                          .arg(std::exp(qBound(-700.0, a, 700.0)), 0, 'g', 5)
                          .arg(r2, 0, 'f', 4)
                          .arg(n);
    } else {
        summary = QStringLiteral("Gumbel (LSQ, %1)\n"
                                 "<i>λ</i> = %2, <i>δ</i> = %3\n")
                          .arg(QString::fromLatin1(kPos[qBound(0, int(pos), 3)]))
                          .arg(a, 0, 'g', 5)
                          .arg(b, 0, 'g', 4);
        if (res.returnPeriod > 1.0)
            summary += QStringLiteral("<i>x</i><sub>T</sub> (T = %1) = %2\n")
                               .arg(res.returnPeriod, 0, 'g', 6)
                               .arg(res.returnValue, 0, 'g', 5);
        summary += QStringLiteral("<i>R</i><sup>2</sup> = %1 (<i>n</i> = %2)")
                           .arg(r2, 0, 'f', 4)
                           .arg(n);
    }
    // Ascending probability line goes bottom-left -> top-right: put legend TopLeft and summary
    // box in the empty BottomRight pocket so neither overlaps the other or the data.
    const double bx = (dist == LogNormal)
            ? std::pow(10.0, std::log10(xLo) + 0.56 * (std::log10(xHi) - std::log10(xLo)))
            : xLo + 0.56 * (xHi - xLo);
    const double by = vLo + 0.34 * (vHi - vLo);
    Legend *box = addSummaryBox(g, bx, by, summary);
    if (box && g->plotWidget()) {
        g->plotWidget()->updateLayout();
        const QRect cr = g->plotWidget()->plotLayout()->canvasRect();
        const QSize bsz = box->rect().size();
        if (cr.width() > bsz.width() + 20 && cr.height() > bsz.height() + 20) {
            box->setOrigin(QPoint(cr.width() - bsz.width() - 10,
                                  cr.height() - bsz.height() - 10));
        }
    }
    g->replot();
    emit g->modifiedGraph();
    return res;
}

void ExtraPlotTypes::showProbabilityPlotDialog(ApplicationWindow *app,
                                               ProbabilityDistribution dist)
{
    if (!app || isHeadless(app))
        return;
    registerE11Translations();
    const QList<Table *> tables = projectTables(app);
    if (tables.isEmpty())
        return;
    Table *act = activeTable(app);
    const QString title = dist == LogNormal ? trE11("Lognormal Probability Plot")
                                            : trE11("Gumbel (Extreme Value) Probability Plot");
    QDialog dlg(app);
    dlg.setWindowTitle(title);
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();
    auto *cbTable = new QComboBox(&dlg);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == act)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }
    auto *cbCol = new QComboBox(&dlg);
    auto refresh = [&]() {
        cbCol->clear();
        Table *t = tables.value(cbTable->currentIndex());
        if (!t)
            return;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (col && col->columnMode() == SciDAVis::ColumnMode::Numeric)
                cbCol->addItem(col->name());
        }
        const QStringList sel = t->selectedColumns();
        if (!sel.isEmpty())
            cbCol->setCurrentText(sel.first().mid(t->name().size() + 1));
    };
    refresh();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refresh(); });
    auto *cbPos = new QComboBox(&dlg);
    cbPos->addItem(trE11("Median rank (i−0.3)/(n+0.4)"), int(MedianRank));
    cbPos->addItem(trE11("Mean rank i/(n+1)"), int(MeanRank));
    cbPos->addItem(trE11("Hazen (i−0.5)/n"), int(Hazen));
    cbPos->addItem(trE11("Blom (i−0.375)/(n+0.25)"), int(Blom));
    // Gumbel / extreme value statistics (ASTM E2283, Murakami) use the mean rank j/(n+1).
    cbPos->setCurrentIndex(dist == Gumbel ? 1 : 0);
    auto *sbT = new QDoubleSpinBox(&dlg);
    sbT->setDecimals(2);
    sbT->setRange(0.0, 1e12);
    sbT->setValue(0.0);
    form->addRow(trE11("Source Table:"), cbTable);
    form->addRow(trE11("Data column:"), cbCol);
    form->addRow(trE11("Plotting position:"), cbPos);
    if (dist == Gumbel)
        form->addRow(trE11("Return period T (0 = none):"), sbT);
    else
        sbT->hide();
    lay->addLayout(form);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const ProbabilityPlotResult r = plotProbability(
            app, tables.value(cbTable->currentIndex()), cbCol->currentText(), dist,
            static_cast<PlottingPosition>(cbPos->currentData().toInt()), sbT->value(), true);
    if (!r.valid) {
        QMessageBox::warning(app, title, r.error);
    } else if (r.excluded > 0) {
        QMessageBox::information(
                app, title,
                trE11("%1 value(s) <= 0 were excluded (the lognormal plot needs positive values).")
                        .arg(r.excluded));
    }
}

// ============================================================================
// E11b helpers: graph tags, spectrogram lookup, pole-figure frame item
// ============================================================================

namespace {

constexpr double kPiE11 = 3.14159265358979323846;
const char *const kExtraTagsProperty = "_toriExtraPlotTags";

QString escTagAttr(const QString &v)
{
    QString e = v;
    e.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    e.replace(QLatin1Char('"'), QLatin1String("&quot;"));
    e.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    e.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    e.replace(QLatin1Char('\n'), QLatin1String("&#10;"));
    return e;
}

QString tagAttr(const char *key, const QString &val)
{
    return QStringLiteral(" ") + QLatin1String(key) + QStringLiteral("=\"") + escTagAttr(val)
            + QStringLiteral("\"");
}

QString tagNum(const char *key, double v)
{
    return tagAttr(key, QString::number(v, 'g', 15));
}

QMap<QString, QString> parseTagAttrs(const QString &line)
{
    QMap<QString, QString> out;
    static const QRegularExpression rx(QStringLiteral("([A-Za-z_][A-Za-z0-9_]*)=\"([^\"]*)\""));
    auto it = rx.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        QString v = m.captured(2);
        v.replace(QLatin1String("&#10;"), QLatin1String("\n"));
        v.replace(QLatin1String("&quot;"), QLatin1String("\""));
        v.replace(QLatin1String("&lt;"), QLatin1String("<"));
        v.replace(QLatin1String("&gt;"), QLatin1String(">"));
        v.replace(QLatin1String("&amp;"), QLatin1String("&"));
        out.insert(m.captured(1), v);
    }
    return out;
}

void storeExtraTag(Graph *g, const QString &kind, const QString &tag)
{
    if (!g || !g->plotWidget())
        return;
    Plot *p = g->plotWidget();
    QStringList tags = p->property(kExtraTagsProperty).toStringList();
    const QString prefix = QStringLiteral("<") + kind;
    for (int i = tags.size() - 1; i >= 0; --i)
        if (tags[i].startsWith(prefix))
            tags.removeAt(i);
    tags << tag;
    p->setProperty(kExtraTagsProperty, tags);
}

Spectrogram *firstSpectrogram(Graph *g)
{
    if (!g)
        return nullptr;
    for (int i = 0; i < g->curves(); ++i) {
        QwtPlotItem *it = g->plotItem(i);
        if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            return static_cast<Spectrogram *>(it);
    }
    return nullptr;
}

QString matrixSafeName(ApplicationWindow *app, const QString &prefix)
{
    QString clean = prefix;
    clean.remove(QLatin1Char('_'));
    clean.remove(QLatin1Char(' '));
    QString name = app->generateUniqueName(clean, false);
    while (app->alreadyUsedName(name))
        name = app->generateUniqueName(clean);
    return name;
}

//! Screen angle (degrees, counter-clockwise from +x) of a pole-figure azimuth beta.
double screenAngleDeg(double betaDeg, bool zeroTop, bool clockwise)
{
    return (zeroTop ? 90.0 : 0.0) + (clockwise ? -betaDeg : betaDeg);
}

//! Outer circle, 30°/90° ticks, 0/90/180/270° labels and RD/TD labels of a pole figure.
class PoleFigureFrameItem : public QwtPlotItem
{
public:
    PoleFigureFrameItem(Graph *g, ExtraPlotTypes::PoleProjection proj, bool zeroTop,
                        bool clockwise, bool rdTop, const QString &ax1, const QString &ax2,
                        bool showTiltCircles)
        : d_graph(g),
          d_proj(proj),
          d_zero_top(zeroTop),
          d_clockwise(clockwise),
          d_rd_top(rdTop),
          d_ax1(ax1),
          d_ax2(ax2),
          d_show_tilt_circles(showTiltCircles)
    {
        setZ(40.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }
    static constexpr int kRtti = QwtPlotItem::Rtti_PlotUserItem + 67;
    int rtti() const override { return kRtti; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &) const override
    {
        if (!painter)
            return;
        const double cx = xMap.xTransform(0.0);
        const double cy = yMap.xTransform(0.0);
        const double rx = std::fabs(xMap.xTransform(1.0) - cx);
        const double ry = std::fabs(yMap.xTransform(1.0) - cy);
        if (!std::isfinite(cx) || !std::isfinite(cy) || rx < 4.0 || ry < 4.0)
            return;
        const double sc = (d_graph && d_graph->plotWidget())
                ? d_graph->plotWidget()->axisDecorationScale(painter)
                : Plot::printDeviceScale();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        if (d_show_tilt_circles) {
            QPen ringPen(QColor(45, 45, 45, 155), 0.9 * sc, Qt::DashLine, Qt::FlatCap,
                         Qt::RoundJoin);
            ringPen.setCosmetic(false);
            painter->setPen(ringPen);
            painter->setBrush(Qt::NoBrush);
            for (int tiltDeg : { 30, 60 }) {
                const double half = 0.5 * double(tiltDeg) * kPiE11 / 180.0;
                const double rf = (d_proj == ExtraPlotTypes::EqualAngle)
                        ? std::tan(half)
                        : std::sqrt(2.0) * std::sin(half);
                painter->drawEllipse(QPointF(cx, cy), rx * rf, ry * rf);
            }
        }

        QPen pen(QColor(0, 0, 0), 1.6 * sc, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
        pen.setCosmetic(false);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), rx, ry);

        QFont f(QStringLiteral("Arial"), 10);
        QFont fAxis(QStringLiteral("Arial"), 11);
        if (d_graph && d_graph->plotWidget()) {
            Plot *pl = d_graph->plotWidget();
            f = pl->displayFont(d_graph->axisFont(QwtPlot::xBottom));
            fAxis = pl->displayFont(d_graph->axisTitleFont(QwtPlot::xBottom));
        } else {
            f.setPixelSize(qMax(8, qRound(12.0 * sc)));
            fAxis.setPixelSize(qMax(9, qRound(14.0 * sc)));
        }
        f.setHintingPreference(QFont::PreferNoHinting);
        fAxis.setHintingPreference(QFont::PreferNoHinting);
        const QFontMetricsF fm(f, painter->device()), fmAxis(fAxis, painter->device());
        const double gap = 3.0 * sc;

        auto dirOf = [&](double betaDeg, double &dx, double &dy) {
            const double th = screenAngleDeg(betaDeg, d_zero_top, d_clockwise) * kPiE11 / 180.0;
            dx = std::cos(th);
            dy = -std::sin(th); // screen y grows downwards
        };
        // text centre just outside distance d along (dx, dy), for a box of size (w, h)
        auto placeText = [&](double dx, double dy, double d, double w, double h) {
            const double px = cx + dx * (rx + d) + dx * 0.5 * w;
            const double py = cy + dy * (ry + d) + dy * 0.5 * h;
            return QRectF(px - 0.5 * w, py - 0.5 * h, w, h);
        };
        for (int deg = 0; deg < 360; deg += 30) {
            const bool major = (deg % 90) == 0;
            double dx = 0.0, dy = 0.0;
            dirOf(deg, dx, dy);
            const double len = (major ? 7.0 : 3.5) * sc;
            painter->setPen(pen);
            painter->drawLine(QPointF(cx + rx * dx, cy + ry * dy),
                              QPointF(cx + (rx + len) * dx, cy + (ry + len) * dy));
            if (!major)
                continue;
            const QString lbl = QString::number(deg) + QChar(0x00B0);
            painter->setFont(f);
            const QRectF br = fm.boundingRect(lbl);
            const QRectF tr = placeText(dx, dy, len + gap, br.width() + 2.0, br.height());
            painter->drawText(tr, Qt::AlignCenter, lbl);
        }
        // Sample reference axes outside the angle labels (screen top and right)
        painter->setFont(fAxis);
        const QString a1 = d_ax1.trimmed().isEmpty() ? QStringLiteral("RD") : d_ax1.trimmed();
        const QString a2 = d_ax2.trimmed().isEmpty() ? QStringLiteral("TD") : d_ax2.trimmed();
        const QString topLbl = d_rd_top ? a1 : a2;
        const QString rightLbl = d_rd_top ? a2 : a1;
        const double angleH = fm.height();
        const double angleW = fm.boundingRect(QStringLiteral("270") + QChar(0x00B0)).width();
        {
            const QRectF br = fmAxis.boundingRect(topLbl);
            const double d = 7.0 * sc + gap + angleH + gap;
            painter->drawText(placeText(0.0, -1.0, d, br.width() + 2.0, br.height()),
                              Qt::AlignCenter, topLbl);
        }
        {
            const QRectF br = fmAxis.boundingRect(rightLbl);
            const double d = 7.0 * sc + gap + angleW + 2.0 * gap;
            painter->drawText(placeText(1.0, 0.0, d, br.width() + 2.0, br.height()),
                              Qt::AlignCenter, rightLbl);
        }
        painter->restore();
    }

private:
    QPointer<Graph> d_graph;
    ExtraPlotTypes::PoleProjection d_proj;
    bool d_zero_top, d_clockwise, d_rd_top;
    QString d_ax1, d_ax2;
    bool d_show_tilt_circles = false;
};

void attachPoleFrame(Graph *g, ExtraPlotTypes::PoleProjection proj, bool zeroTop, bool clockwise,
                     bool rdTop, const QString &ax1 = QString(), const QString &ax2 = QString(),
                     bool showTiltCircles = false)
{
    if (!g || !g->plotWidget())
        return;
    Plot *p = g->plotWidget();
    const QwtPlotItemList items = p->itemList();
    for (QwtPlotItem *it : items) {
        if (it && it->rtti() == PoleFigureFrameItem::kRtti) {
            it->detach();
            delete it;
        }
    }
    auto *frame = new PoleFigureFrameItem(g, proj, zeroTop, clockwise, rdTop, ax1, ax2,
                                          showTiltCircles);
    frame->attach(p);
}

//! Pole-figure page set-up shared by creation and project restore: hidden XY axes, no frame /
//! grid, equal X / Y ranges around the unit circle.
void setupPoleFigurePage(Graph *g)
{
    if (!g)
        return;
    const int hide[3] = { QwtPlot::xBottom, QwtPlot::yLeft, QwtPlot::xTop };
    for (int a : hide)
        g->enableAxis(a, false);
    g->drawCanvasFrame(false, 0);
    if (Grid *gr = g->grid()) {
        gr->enableX(false);
        gr->enableXMin(false);
        gr->enableY(false);
        gr->enableYMin(false);
    }
    // no box around the circle; the colour bar keeps its own outline (re-enabled by the sync)
    g->drawAxesBackbones(false);
    const double lim = ExtraPlotTypes::poleFigureScaleLimit(g);
    g->setScale(QwtPlot::xBottom, -lim, lim);
    g->setScale(QwtPlot::yLeft, -lim, lim);
    if (Spectrogram *sp = firstSpectrogram(g))
        sp->syncColorScale();
}

// ---- pole-figure grid ----------------------------------------------------------------------
struct PoleGrid {
    QVector<double> alpha; //!< tilt, 0 = centre, ascending
    QVector<double> beta;  //!< 0 <= beta < 360, ascending
    QVector<double> val;   //!< [ia * nBeta + ib], NaN = missing
    double maxBetaGap = 360.0;
};

qint64 angleKey(double v)
{
    return qint64(std::llround(v * 1.0e6));
}

bool buildPoleGrid(const QVector<double> &a, const QVector<double> &b, const QVector<double> &v,
                   bool alphaFromRim, PoleGrid &g, QString &err)
{
    const int n = qMin(a.size(), qMin(b.size(), v.size()));
    QVector<double> aa, bb, vv;
    aa.reserve(n);
    bb.reserve(n);
    vv.reserve(n);
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(a[i]) || !std::isfinite(b[i]) || !std::isfinite(v[i]))
            continue;
        double t = alphaFromRim ? 90.0 - a[i] : a[i];
        if (t < -1e-6 || t > 90.0 + 1e-6) {
            err = trE11("Alpha must lie between 0° and 90°.");
            return false;
        }
        t = qBound(0.0, t, 90.0);
        double be = std::fmod(b[i], 360.0);
        if (be < 0.0)
            be += 360.0;
        if (be >= 360.0 - 1e-6)
            be = 0.0;
        aa << t;
        bb << be;
        vv << v[i];
    }
    if (aa.isEmpty()) {
        err = trE11("The pole figure needs numeric alpha, beta and intensity columns.");
        return false;
    }
    QMap<qint64, double> au, bu;
    for (int i = 0; i < aa.size(); ++i) {
        au.insert(angleKey(aa[i]), aa[i]);
        bu.insert(angleKey(bb[i]), bb[i]);
    }
    g.alpha = au.values().toVector();
    g.beta = bu.values().toVector();
    const int nA = g.alpha.size(), nB = g.beta.size();
    if (double(nA) * double(nB) > 4.0 * aa.size() + 16.0 || double(nA) * double(nB) > 4.0e6) {
        err = trE11("The data are not on a regular alpha-beta grid.");
        return false;
    }
    QHash<qint64, int> ai, bi;
    for (int i = 0; i < nA; ++i)
        ai.insert(angleKey(g.alpha[i]), i);
    for (int j = 0; j < nB; ++j)
        bi.insert(angleKey(g.beta[j]), j);
    QVector<double> sum(nA * nB, 0.0);
    QVector<int> cnt(nA * nB, 0);
    for (int i = 0; i < aa.size(); ++i) {
        const int k = ai.value(angleKey(aa[i])) * nB + bi.value(angleKey(bb[i]));
        sum[k] += vv[i];
        cnt[k] += 1;
    }
    g.val.resize(nA * nB);
    int filled = 0;
    for (int k = 0; k < nA * nB; ++k) {
        g.val[k] = cnt[k] > 0 ? sum[k] / cnt[k] : std::numeric_limits<double>::quiet_NaN();
        filled += cnt[k] > 0 ? 1 : 0;
    }
    // a scan is a (nearly) full alpha x beta grid; scattered orientations are not
    if (nA * nB > 4 && filled < (nA * nB + 1) / 2) {
        err = trE11("The data are not on a regular alpha-beta grid.");
        return false;
    }
    // largest regular beta step (a wider gap is not interpolated across)
    if (nB >= 2) {
        QVector<double> steps;
        for (int j = 0; j < nB; ++j)
            steps << (j + 1 < nB ? g.beta[j + 1] - g.beta[j] : g.beta[0] + 360.0 - g.beta[j]);
        QVector<double> sorted = steps;
        std::sort(sorted.begin(), sorted.end());
        g.maxBetaGap = 2.5 * sorted[(sorted.size() - 1) / 2] + 1e-9;
    }
    return true;
}

double poleInterp(const PoleGrid &g, double a, double b)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const int nA = g.alpha.size(), nB = g.beta.size();
    if (nA == 0 || nB == 0)
        return nan;
    if (a < g.alpha.front() - 1e-9) {
        const double step = nA > 1 ? g.alpha[1] - g.alpha[0] : 0.0;
        if (g.alpha.front() - a > 0.5 * step + 1e-9)
            return nan; // centre not measured
        a = g.alpha.front();
    }
    if (a > g.alpha.back() + 1e-9)
        return nan; // high tilt not measured
    int i0 = int(std::upper_bound(g.alpha.begin(), g.alpha.end(), a) - g.alpha.begin()) - 1;
    i0 = qBound(0, i0, nA - 1);
    const int i1 = qMin(i0 + 1, nA - 1);
    const double ta = (i1 > i0) ? (a - g.alpha[i0]) / (g.alpha[i1] - g.alpha[i0]) : 0.0;

    int j0 = 0, j1 = 0;
    double tb = 0.0;
    if (nB > 1) {
        const int j = int(std::upper_bound(g.beta.begin(), g.beta.end(), b) - g.beta.begin()) - 1;
        double b0 = 0.0, b1 = 0.0;
        if (j < 0) {
            j0 = nB - 1;
            j1 = 0;
            b0 = g.beta[nB - 1] - 360.0;
            b1 = g.beta[0];
        } else if (j >= nB - 1) {
            j0 = nB - 1;
            j1 = 0;
            b0 = g.beta[nB - 1];
            b1 = g.beta[0] + 360.0;
        } else {
            j0 = j;
            j1 = j + 1;
            b0 = g.beta[j0];
            b1 = g.beta[j1];
        }
        if (b1 - b0 > g.maxBetaGap)
            return nan; // beta range not measured
        tb = (b1 > b0) ? (b - b0) / (b1 - b0) : 0.0;
    }
    const double v00 = g.val[i0 * nB + j0], v01 = g.val[i0 * nB + j1];
    const double v10 = g.val[i1 * nB + j0], v11 = g.val[i1 * nB + j1];
    if (!(std::isfinite(v00) && std::isfinite(v01) && std::isfinite(v10) && std::isfinite(v11)))
        return g.val[(ta < 0.5 ? i0 : i1) * nB + (tb < 0.5 ? j0 : j1)];
    const double top = v00 + tb * (v01 - v00);
    const double bot = v10 + tb * (v11 - v10);
    return top + ta * (bot - top);
}

//! Solid-angle weighted mean over the measured nodes (ring bands between mid-points).
double poleMeanIntensity(const PoleGrid &g)
{
    const int nA = g.alpha.size(), nB = g.beta.size();
    const double d2r = kPiE11 / 180.0;
    double sw = 0.0, swv = 0.0;
    for (int i = 0; i < nA; ++i) {
        const double lo = (i == 0) ? g.alpha[0] : 0.5 * (g.alpha[i - 1] + g.alpha[i]);
        const double hi = (i == nA - 1) ? g.alpha[nA - 1] : 0.5 * (g.alpha[i] + g.alpha[i + 1]);
        double wa = std::cos(lo * d2r) - std::cos(hi * d2r);
        if (nA == 1)
            wa = 1.0;
        for (int j = 0; j < nB; ++j) {
            double wb = 360.0;
            if (nB > 1) {
                const double prev = (j == 0) ? g.beta[nB - 1] - 360.0 : g.beta[j - 1];
                const double next = (j == nB - 1) ? g.beta[0] + 360.0 : g.beta[j + 1];
                const double dPrev = g.beta[j] - prev;
                const double dNext = next - g.beta[j];
                const double halfPrev = (dPrev <= g.maxBetaGap) ? 0.5 * dPrev : 0.0;
                const double halfNext = (dNext <= g.maxBetaGap) ? 0.5 * dNext : 0.0;
                wb = halfPrev + halfNext;
                if (!(wb > 0.0))
                    wb = qMin(dPrev, dNext);
            }
            const double v = g.val[i * nB + j];
            if (!std::isfinite(v) || !(wa * wb > 0.0))
                continue;
            sw += wa * wb;
            swv += wa * wb * v;
        }
    }
    return sw > 0.0 ? swv / sw : std::numeric_limits<double>::quiet_NaN();
}

const char *projectionName(ExtraPlotTypes::PoleProjection p)
{
    return p == ExtraPlotTypes::EqualAngle ? "Wulff (equal-angle)" : "Schmidt (equal-area)";
}

} // namespace

// ============================================================================
// 5. 2D density (2D histogram)
// ============================================================================

ExtraPlotTypes::Density2DResult ExtraPlotTypes::binDensity2D(const QVector<double> &x,
                                                             const QVector<double> &y, int binsX,
                                                             int binsY, double widthX,
                                                             double widthY)
{
    registerE11Translations();
    Density2DResult r;
    const int n = qMin(x.size(), y.size());
    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
    int nv = 0;
    for (int i = 0; i < n; ++i) {
        const double xv = x[i], yv = y[i];
        if (!std::isfinite(xv) || !std::isfinite(yv)) {
            ++r.excluded;
            continue;
        }
        if (nv == 0) {
            xmin = xmax = xv;
            ymin = ymax = yv;
        } else {
            xmin = qMin(xmin, xv);
            xmax = qMax(xmax, xv);
            ymin = qMin(ymin, yv);
            ymax = qMax(ymax, yv);
        }
        ++nv;
    }
    if (nv == 0) {
        r.error = trE11("No point has both a numeric X and Y value.");
        return r;
    }
    const int nAuto = qBound(5, int(std::ceil(2.0 * std::cbrt(double(nv)))), 256);
    constexpr int kMaxBins = 4096;
    auto axis = [&](double lo, double hi, int bins, double width, double &start, double &w,
                    int &nb) -> bool {
        if (width > 0.0 && std::isfinite(width)) {
            w = width;
            start = std::floor(lo / w) * w;
            if (!std::isfinite(start))
                return false;
            const double span = (hi - start) / w;
            if (!(span < double(kMaxBins)) || !std::isfinite(span))
                return false;
            nb = qMax(1, int(std::ceil(span)));
            if (start + nb * w <= hi) // [a, b) bins: a value on the last edge needs one more
                ++nb;
            return nb <= kMaxBins;
        }
        nb = bins > 0 ? qMin(bins, kMaxBins) : nAuto;
        if (hi > lo && std::isfinite(hi - lo)) {
            start = lo;
            w = (hi - lo) / nb;
        } else {
            w = (lo != 0.0) ? 0.1 * std::fabs(lo) : 1.0;
            if (!(w > 0.0) || !std::isfinite(w))
                w = 1.0;
            start = lo - 0.5 * w * nb;
        }
        if (!(w > 0.0) || !std::isfinite(w) || !std::isfinite(1.0 / w)) {
            w = 1.0;
            start = lo - 0.5 * w * nb;
        }
        return true;
    };
    int nbx = 0, nby = 0;
    double x0 = 0.0, y0 = 0.0, wx = 1.0, wy = 1.0;
    if (!axis(xmin, xmax, binsX, widthX, x0, wx, nbx) || !axis(ymin, ymax, binsY, widthY, y0, wy, nby)) {
        r.error = trE11("Too many bins (at most 4096 per axis); use a larger bin width.");
        return r;
    }
    r.counts.fill(0.0, nbx * nby);
    double *c = r.counts.data();
    const double ix = 1.0 / wx, iy = 1.0 / wy;
    for (int i = 0; i < n; ++i) {
        const double xv = x[i], yv = y[i];
        if (!std::isfinite(xv) || !std::isfinite(yv))
            continue;
        const double fx = (xv - x0) * ix;
        const double fy = (yv - y0) * iy;
        const int bx = (!std::isfinite(fx) || fx < 0.0)
                ? 0
                : (fx >= double(nbx) ? nbx - 1 : int(std::floor(fx)));
        const int by = (!std::isfinite(fy) || fy < 0.0)
                ? 0
                : (fy >= double(nby) ? nby - 1 : int(std::floor(fy)));
        c[by * nbx + bx] += 1.0;
    }
    r.n = nv;
    r.binsX = nbx;
    r.binsY = nby;
    r.x0 = x0;
    r.y0 = y0;
    r.widthX = wx;
    r.widthY = wy;
    r.maxCount = *std::max_element(r.counts.constBegin(), r.counts.constEnd());
    r.valid = true;
    return r;
}

namespace {

bool readColumnPairs(Table *t, Column *cx, Column *cy, QVector<double> &xs, QVector<double> &ys,
                     int &excluded)
{
    excluded = 0;
    if (!t || !cx || !cy)
        return false;
    const int rows = qMax(cx->rowCount(), cy->rowCount());
    xs.reserve(rows);
    ys.reserve(rows);
    for (int r = 0; r < rows; ++r) {
        double xv = 0.0, yv = 0.0;
        const bool okx = readNumber(cx, r, xv);
        const bool oky = readNumber(cy, r, yv);
        if (okx && oky) {
            xs << xv;
            ys << yv;
        } else if (okx || oky) {
            ++excluded;
        }
    }
    return true;
}

void fillDensityMatrix(Matrix *m, const ExtraPlotTypes::Density2DResult &r)
{
    if (!m)
        return;
    if (m->numRows() != r.binsY || m->numCols() != r.binsX)
        m->setDimensions(r.binsY, r.binsX);
    m->setCoordinates(r.x0 + 0.5 * r.widthX, r.x0 + (r.binsX - 0.5) * r.widthX,
                      r.y0 + 0.5 * r.widthY, r.y0 + (r.binsY - 0.5) * r.widthY);
    QVector<qreal> cells(r.binsX * r.binsY);
    int k = 0;
    for (int col = 0; col < r.binsX; ++col) // Matrix::setCells is column-major
        for (int row = 0; row < r.binsY; ++row)
            cells[k++] = r.counts[row * r.binsX + col];
    m->setCells(cells);
}

QString density2DLabel(Table *t, Column *cx, Column *cy, const ExtraPlotTypes::Density2DResult &r)
{
    return trE11("2D density of %1: %2 vs %3 (%4 x %5 bins)")
            .arg(t->name(), cy->name(), cx->name())
            .arg(r.binsX)
            .arg(r.binsY);
}

QString buildDensity2DTag(Matrix *m, Table *t, Column *cx, Column *cy, int binsX, int binsY,
                          double widthX, double widthY, bool logColor, bool cellBlocks,
                          const ExtraPlotTypes::Density2DResult &r)
{
    return QStringLiteral("<ToriDensity2D") + tagAttr("matrix", m ? m->name() : QString())
            + tagAttr("table", t ? t->name() : QString())
            + tagAttr("x", cx ? cx->name() : QString())
            + tagAttr("y", cy ? cy->name() : QString()) + tagAttr("bx", QString::number(binsX))
            + tagAttr("by", QString::number(binsY)) + tagNum("wx", widthX) + tagNum("wy", widthY)
            + tagAttr("log", QString::number(logColor ? 1 : 0))
            + tagAttr("blocks", QString::number(cellBlocks ? 1 : 0))
            + tagAttr("nbx", QString::number(r.binsX))
            + tagAttr("nby", QString::number(r.binsY))
            + tagAttr("n", QString::number(r.n)) + QStringLiteral("/>");
}

void applyDensity2DSpectrogramSettings(Spectrogram *sp, int preset, bool reverse, bool logColor,
                                       bool cellBlocks, Matrix *mForUpdate = nullptr)
{
    if (!sp)
        return;
    sp->setCellBlocks(cellBlocks);
    sp->setLogColorScale(logColor);
    if (preset == int(Spectrogram::PresetCustom)) {
        if (sp->reversePalette() != reverse)
            sp->setReversePalette(reverse);
    } else {
        sp->setColorMapPreset(static_cast<Spectrogram::ColorMapPreset>(preset), reverse);
    }
    if (mForUpdate)
        sp->updateData(mForUpdate);
    sp->setColorBarTitle(trE11Count());
    sp->syncColorScale();
}

} // namespace

ExtraPlotTypes::Density2DResult
ExtraPlotTypes::plotDensity2D(ApplicationWindow *app, Table *t, const QString &xCol,
                              const QString &yCol, int binsX, int binsY, double widthX,
                              double widthY, bool logColor, int preset, bool createPlot)
{
    registerE11Translations();
    Density2DResult r;
    Column *cx = resolveColumn(t, xCol);
    Column *cy = resolveColumn(t, yCol);
    if (!t || !cx || !cy || cx->columnMode() != SciDAVis::ColumnMode::Numeric
        || cy->columnMode() != SciDAVis::ColumnMode::Numeric) {
        r.error = trE11("The 2D density needs numeric X and Y columns.");
        return r;
    }
    QVector<double> xs, ys;
    int excluded = 0;
    readColumnPairs(t, cx, cy, xs, ys, excluded);
    r = binDensity2D(xs, ys, binsX, binsY, widthX, widthY);
    r.excluded += excluded;
    if (!r.valid || !app)
        return r;

    Matrix *m = app->newMatrix(matrixSafeName(app, QStringLiteral("Density2D") + t->name()),
                               r.binsY, r.binsX);
    r.matrix = m;
    if (!m)
        return r;
    fillDensityMatrix(m, r);
    m->setWindowLabel(density2DLabel(t, cx, cy, r));
    if (!createPlot)
        return r;

    MultiLayer *ml =
            app->newGraph(app->generateUniqueName(QStringLiteral("Density2D_") + t->name(), false));
    r.plot = ml;
    Graph *g = ml ? ml->activeGraph() : nullptr;
    if (!g)
        return r;
    g->plotSpectrogram(m, Graph::ColorMap);
    applyDensity2DSpectrogramSettings(firstSpectrogram(g), preset, false, logColor, true);
    g->setXAxisTitle(columnAxisTitle(cx, g));
    g->setYAxisTitle(columnAxisTitle(cy, g));
    g->setScale(QwtPlot::xBottom, r.x0, r.x0 + r.binsX * r.widthX);
    g->setScale(QwtPlot::yLeft, r.y0, r.y0 + r.binsY * r.widthY);
    if (g->legend())
        g->removeLegend();
    storeExtraTag(g, QStringLiteral("ToriDensity2D"),
                  buildDensity2DTag(m, t, cx, cy, binsX, binsY, widthX, widthY, logColor, true, r));
    g->replot();
    emit g->modifiedGraph();
    return r;
}

ExtraPlotTypes::Density2DResult
ExtraPlotTypes::updateDensity2D(ApplicationWindow *app, Graph *g, Table *t, const QString &xCol,
                                const QString &yCol, int binsX, int binsY, double widthX,
                                double widthY, bool logColor, int preset, bool cellBlocks,
                                bool reverse)
{
    registerE11Translations();
    Density2DResult r;
    if (!app || !g)
        return r;
    Column *cx = resolveColumn(t, xCol);
    Column *cy = resolveColumn(t, yCol);
    if (!t || !cx || !cy || cx->columnMode() != SciDAVis::ColumnMode::Numeric
        || cy->columnMode() != SciDAVis::ColumnMode::Numeric) {
        r.error = trE11("The 2D density needs numeric X and Y columns.");
        return r;
    }
    QVector<double> xs, ys;
    int excluded = 0;
    readColumnPairs(t, cx, cy, xs, ys, excluded);
    r = binDensity2D(xs, ys, binsX, binsY, widthX, widthY);
    r.excluded += excluded;
    if (!r.valid)
        return r;

    const QMap<QString, QString> prevAttrs =
            parseTagAttrs(graphTag(g, QStringLiteral("ToriDensity2D")));
    Spectrogram *sp = firstSpectrogram(g);
    Matrix *m = app->matrix(prevAttrs.value(QStringLiteral("matrix")));
    if (!m && sp)
        m = sp->matrix();
    if (!m) {
        m = app->newMatrix(matrixSafeName(app, QStringLiteral("Density2D") + t->name()),
                           r.binsY, r.binsX);
    }
    r.matrix = m;
    r.plot = g->multiLayerParent();
    if (!m)
        return r;

    fillDensityMatrix(m, r);
    m->setWindowLabel(density2DLabel(t, cx, cy, r));
    if (!sp) {
        g->plotSpectrogram(m, Graph::ColorMap);
        sp = firstSpectrogram(g);
        applyDensity2DSpectrogramSettings(sp, preset, reverse, logColor, cellBlocks);
    } else {
        applyDensity2DSpectrogramSettings(sp, preset, reverse, logColor, cellBlocks, m);
    }
    g->setXAxisTitle(columnAxisTitle(cx, g));
    g->setYAxisTitle(columnAxisTitle(cy, g));
    g->setScale(QwtPlot::xBottom, r.x0, r.x0 + r.binsX * r.widthX);
    g->setScale(QwtPlot::yLeft, r.y0, r.y0 + r.binsY * r.widthY);
    storeExtraTag(g, QStringLiteral("ToriDensity2D"),
                  buildDensity2DTag(m, t, cx, cy, binsX, binsY, widthX, widthY, logColor,
                                    cellBlocks, r));
    g->replot();
    emit g->modifiedGraph();
    return r;
}

bool ExtraPlotTypes::rebuildDensity2D(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return false;
    const QString tag = graphTag(g, QStringLiteral("ToriDensity2D"));
    if (tag.isEmpty())
        return false;
    const QMap<QString, QString> a = parseTagAttrs(tag);
    Table *t = app->table(a.value(QStringLiteral("table")));
    Matrix *m = app->matrix(a.value(QStringLiteral("matrix")));
    if (!t || !m)
        return false;
    Column *cx = resolveColumn(t, a.value(QStringLiteral("x")));
    Column *cy = resolveColumn(t, a.value(QStringLiteral("y")));
    if (!cx || !cy)
        return false;
    QVector<double> xs, ys;
    int excluded = 0;
    readColumnPairs(t, cx, cy, xs, ys, excluded);
    const int binsX = a.value(QStringLiteral("bx")).toInt();
    const int binsY = a.value(QStringLiteral("by")).toInt();
    const double widthX = a.value(QStringLiteral("wx")).toDouble();
    const double widthY = a.value(QStringLiteral("wy")).toDouble();
    const Density2DResult r = binDensity2D(xs, ys, binsX, binsY, widthX, widthY);
    if (!r.valid)
        return false;
    fillDensityMatrix(m, r);
    m->setWindowLabel(density2DLabel(t, cx, cy, r));
    Spectrogram *sp = firstSpectrogram(g);
    const bool logColor = sp ? sp->logColorScale() : (a.value(QStringLiteral("log")).toInt() != 0);
    const bool cellBlocks = sp
            ? sp->cellBlocks()
            : (a.value(QStringLiteral("blocks"), QStringLiteral("1")).toInt() != 0);
    if (sp) {
        sp->setCellBlocks(cellBlocks);
        sp->setLogColorScale(logColor);
        sp->updateData(m);
        sp->syncColorScale();
    }
    g->setScale(QwtPlot::xBottom, r.x0, r.x0 + r.binsX * r.widthX);
    g->setScale(QwtPlot::yLeft, r.y0, r.y0 + r.binsY * r.widthY);
    storeExtraTag(g, QStringLiteral("ToriDensity2D"),
                  buildDensity2DTag(m, t, cx, cy, binsX, binsY, widthX, widthY, logColor,
                                    cellBlocks, r));
    g->replot();
    emit g->modifiedGraph();
    return true;
}

// ============================================================================
// 6. Pole figure
// ============================================================================

bool ExtraPlotTypes::poleProjection(double alphaDeg, double betaDeg, PoleProjection proj,
                                    double *x, double *y, bool betaZeroAtTop, bool clockwise)
{
    if (!std::isfinite(alphaDeg) || !std::isfinite(betaDeg) || alphaDeg < -1e-9
        || alphaDeg > 90.0 + 1e-9)
        return false;
    const double half = 0.5 * qBound(0.0, alphaDeg, 90.0) * kPiE11 / 180.0;
    const double r = (proj == EqualAngle) ? std::tan(half) : std::sqrt(2.0) * std::sin(half);
    const double th = screenAngleDeg(betaDeg, betaZeroAtTop, clockwise) * kPiE11 / 180.0;
    if (x)
        *x = r * std::cos(th);
    if (y)
        *y = r * std::sin(th);
    return true;
}

bool ExtraPlotTypes::inversePoleProjection(double x, double y, PoleProjection proj,
                                           double *alphaDeg, double *betaDeg, bool betaZeroAtTop,
                                           bool clockwise)
{
    const double r = std::hypot(x, y);
    if (!std::isfinite(r) || r > 1.0 + 1e-12)
        return false;
    const double rc = qMin(r, 1.0);
    const double half = (proj == EqualAngle) ? std::atan(rc) : std::asin(rc / std::sqrt(2.0));
    if (alphaDeg)
        *alphaDeg = 2.0 * half * 180.0 / kPiE11;
    if (betaDeg) {
        const double th = std::atan2(y, x) * 180.0 / kPiE11;
        double be = clockwise ? (betaZeroAtTop ? 90.0 : 0.0) - th : th - (betaZeroAtTop ? 90.0 : 0.0);
        be = std::fmod(be, 360.0);
        if (be < 0.0)
            be += 360.0;
        *betaDeg = be;
    }
    return true;
}

namespace {

ExtraPlotTypes::PoleFigureResult buildPoleFigure(ApplicationWindow *app, const PoleGrid &grid,
                                                 const ExtraPlotTypes::PoleFigureOptions &opt,
                                                 bool createPlot, const QString &sourceName,
                                                 const QString &sourceTagAttrs)
{
    using EPT = ExtraPlotTypes;
    EPT::PoleFigureResult res;
    res.nAlpha = grid.alpha.size();
    res.nBeta = grid.beta.size();
    res.alphaMin = grid.alpha.front();
    res.alphaMax = grid.alpha.back();
    for (double v : grid.val)
        if (!std::isfinite(v))
            ++res.missing;
    res.meanIntensity = poleMeanIntensity(grid);
    double norm = 1.0;
    if (opt.normalizeMRD) {
        if (!(res.meanIntensity > 0.0) || !std::isfinite(res.meanIntensity)) {
            res.error = trE11("The mean intensity is not positive, so the MRD cannot be computed.");
            return res;
        }
        norm = 1.0 / res.meanIntensity;
    }
    bool haveMax = false;
    for (int i = 0; i < res.nAlpha; ++i)
        for (int j = 0; j < res.nBeta; ++j) {
            const double v = grid.val[i * res.nBeta + j];
            if (std::isfinite(v) && (!haveMax || v * norm > res.maxValue)) {
                haveMax = true;
                res.maxValue = v * norm;
                res.maxAlpha = grid.alpha[i];
                res.maxBeta = grid.beta[j];
            }
        }
    EPT::poleProjection(res.maxAlpha, res.maxBeta, opt.projection, &res.maxX, &res.maxY,
                        opt.betaZeroAtTop, opt.clockwise);
    res.valid = true;
    if (!app)
        return res;

    int N = qBound(51, opt.gridSize, 801);
    if (N % 2 == 0)
        ++N;
    Matrix *m = app->newMatrix(matrixSafeName(app, QStringLiteral("PoleFigure") + sourceName), N, N);
    res.matrix = m;
    if (!m)
        return res;
    m->setCoordinates(-1.0, 1.0, -1.0, 1.0);
    QVector<qreal> cells(N * N);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    int k = 0;
    for (int col = 0; col < N; ++col) {
        const double x = -1.0 + 2.0 * col / double(N - 1);
        for (int row = 0; row < N; ++row) {
            const double y = -1.0 + 2.0 * row / double(N - 1);
            double a = 0.0, b = 0.0;
            double v = nan;
            if (EPT::inversePoleProjection(x, y, opt.projection, &a, &b, opt.betaZeroAtTop,
                                           opt.clockwise))
                v = poleInterp(grid, a, b) * norm;
            cells[k++] = std::isfinite(v) ? v : nan;
        }
    }
    m->setCells(cells);
    m->setWindowLabel(trE11("Projected pole figure of %1 (%2)")
                              .arg(sourceName, QString::fromLatin1(projectionName(opt.projection))));
    if (!createPlot)
        return res;

    MultiLayer *ml = app->newGraph(
            app->generateUniqueName(QStringLiteral("PoleFigure_") + sourceName, false));
    res.plot = ml;
    Graph *g = ml ? ml->activeGraph() : nullptr;
    if (!g)
        return res;
    g->plotSpectrogram(m, Graph::ColorMap);
    if (Spectrogram *sp = firstSpectrogram(g)) {
        sp->setMaskInvalidCells(true);
        sp->setColorMapPreset(static_cast<Spectrogram::ColorMapPreset>(opt.preset), false);
        sp->setDisplayMode(QwtPlotSpectrogram::ContourMode, true);
        sp->setDefaultContourPen(QPen(QColor(25, 25, 25), 1));
        sp->setLevelsNumber(qBound(2, opt.levels, 50));
        sp->setColorBarTitle(opt.normalizeMRD ? QStringLiteral("MRD") : trE11("Intensity"));
        sp->syncColorScale();
    }
    if (g->legend())
        g->removeLegend();
    setupPoleFigurePage(g);
    attachPoleFrame(g, opt.projection, opt.betaZeroAtTop, opt.clockwise, opt.rdTop,
                    opt.axis1Label, opt.axis2Label, opt.showTiltCircles);
    if (ml)
        ml->setLayerCanvasSize(420, 420);

    QString summary;
    if (!opt.label.trimmed().isEmpty())
        summary += opt.label.trimmed() + QStringLiteral(" ");
    summary += QString::fromLatin1(projectionName(opt.projection)) + QStringLiteral("\n");
    summary += QStringLiteral("max = %1 %2\n")
                       .arg(res.maxValue, 0, 'f', opt.normalizeMRD ? 2 : 0)
                       .arg(opt.normalizeMRD ? QStringLiteral("MRD") : QString());
    summary += QStringLiteral("(<i>α</i> = %1°, <i>β</i> = %2°)")
                       .arg(res.maxAlpha, 0, 'g', 4)
                       .arg(res.maxBeta, 0, 'g', 4);
    if (res.alphaMax < 90.0 - 1e-6)
        summary += QStringLiteral("\n") + trE11("measured alpha <= %1°").arg(res.alphaMax, 0, 'g', 4);
    addSummaryBox(g, -1.36, 1.36, summary);

    QString extraPoleAttrs;
    if (!opt.axis1Label.trimmed().isEmpty())
        extraPoleAttrs += tagAttr("ax1", opt.axis1Label.trimmed());
    if (!opt.axis2Label.trimmed().isEmpty())
        extraPoleAttrs += tagAttr("ax2", opt.axis2Label.trimmed());
    if (opt.showTiltCircles)
        extraPoleAttrs += tagAttr("rings", QStringLiteral("1"));
    storeExtraTag(g, QStringLiteral("ToriPoleFigure"),
                  QStringLiteral("<ToriPoleFigure") + tagAttr("matrix", m->name()) + sourceTagAttrs
                          + tagAttr("proj", QString::number(int(opt.projection)))
                          + tagAttr("mrd", QString::number(opt.normalizeMRD ? 1 : 0))
                          + tagAttr("rim", QString::number(opt.alphaFromRim ? 1 : 0))
                          + tagAttr("zeroTop", QString::number(opt.betaZeroAtTop ? 1 : 0))
                          + tagAttr("cw", QString::number(opt.clockwise ? 1 : 0))
                          + tagAttr("rdTop", QString::number(opt.rdTop ? 1 : 0))
                          + extraPoleAttrs
                          + tagAttr("label", opt.label) + tagNum("max", res.maxValue)
                          + tagNum("maxa", res.maxAlpha) + tagNum("maxb", res.maxBeta)
                          + tagNum("mean", res.meanIntensity) + QStringLiteral("/>"));
    g->replot();
    emit g->modifiedGraph();
    return res;
}

} // namespace

ExtraPlotTypes::PoleFigureResult
ExtraPlotTypes::plotPoleFigure(ApplicationWindow *app, Table *t, const QString &alphaCol,
                               const QString &betaCol, const QString &intensityCol,
                               const PoleFigureOptions &opt, bool createPlot)
{
    registerE11Translations();
    PoleFigureResult res;
    Column *ca = resolveColumn(t, alphaCol);
    Column *cb = resolveColumn(t, betaCol);
    Column *ci = resolveColumn(t, intensityCol);
    auto numeric = [](Column *c) { return c && c->columnMode() == SciDAVis::ColumnMode::Numeric; };
    if (!t || !numeric(ca) || !numeric(cb) || !numeric(ci)) {
        res.error = trE11("The pole figure needs numeric alpha, beta and intensity columns.");
        return res;
    }
    QVector<double> a, b, v;
    const int rows = qMax(ca->rowCount(), qMax(cb->rowCount(), ci->rowCount()));
    for (int r = 0; r < rows; ++r) {
        double av = 0.0, bv = 0.0, iv = 0.0;
        if (readNumber(ca, r, av) && readNumber(cb, r, bv) && readNumber(ci, r, iv)) {
            a << av;
            b << bv;
            v << iv;
        }
    }
    PoleGrid grid;
    if (!buildPoleGrid(a, b, v, opt.alphaFromRim, grid, res.error))
        return res;
    return buildPoleFigure(app, grid, opt, createPlot, t->name(),
                           tagAttr("table", t->name()) + tagAttr("a", ca->name())
                                   + tagAttr("b", cb->name()) + tagAttr("i", ci->name()));
}

ExtraPlotTypes::PoleFigureResult
ExtraPlotTypes::plotPoleFigureFromMatrix(ApplicationWindow *app, Matrix *m,
                                         const PoleFigureOptions &opt, bool createPlot)
{
    registerE11Translations();
    PoleFigureResult res;
    if (!m || m->numRows() < 1 || m->numCols() < 1) {
        res.error = trE11("The pole figure needs numeric alpha, beta and intensity columns.");
        return res;
    }
    const int rows = m->numRows(), cols = m->numCols();
    double xs = m->xStart(), xe = m->xEnd(), ys = m->yStart(), ye = m->yEnd();
    if (opt.useCustomMatrixAngles) {
        ys = opt.matrixAlphaMin;
        ye = opt.matrixAlphaMax;
        xs = opt.matrixBetaMin;
        xe = opt.matrixBetaMax;
    }
    QVector<double> a, b, v;
    for (int r = 0; r < rows; ++r) {
        const double av = rows > 1 ? ys + (ye - ys) * r / double(rows - 1) : ys;
        for (int c = 0; c < cols; ++c) {
            const double bv = cols > 1 ? xs + (xe - xs) * c / double(cols - 1) : xs;
            a << av;
            b << bv;
            v << m->cell(r, c);
        }
    }
    PoleGrid grid;
    if (!buildPoleGrid(a, b, v, opt.alphaFromRim, grid, res.error))
        return res;
    return buildPoleFigure(app, grid, opt, createPlot, m->name(),
                           tagAttr("sourceMatrix", m->name()));
}

QString ExtraPlotTypes::graphTag(Graph *g, const QString &kind)
{
    if (!g || !g->plotWidget())
        return QString();
    const QString prefix = QStringLiteral("<") + kind;
    for (const QString &tag : syncedGraphTagsForState(g))
        if (tag.startsWith(prefix))
            return tag;
    return QString();
}

namespace {
QString rebuildTagWithUpdates(const QString &tag, const QString &kind,
                              const QMap<QString, QString> &updates)
{
    if (updates.isEmpty())
        return tag;
    QMap<QString, QString> attrs = parseTagAttrs(tag);
    for (auto it = updates.constBegin(); it != updates.constEnd(); ++it)
        attrs.insert(it.key(), it.value());

    // Preserve existing attribute order while updating modified values, appending any missing keys.
    static const QRegularExpression rx(QStringLiteral("([A-Za-z_][A-Za-z0-9_]*)=\"([^\"]*)\""));
    QString out = QStringLiteral("<") + kind;
    QSet<QString> emitted;
    auto it = rx.globalMatch(tag);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString key = m.captured(1);
        const QString val = attrs.contains(key) ? attrs.value(key) : m.captured(2);
        out += tagAttr(key.toUtf8().constData(), val);
        emitted.insert(key);
    }
    for (auto ui = updates.constBegin(); ui != updates.constEnd(); ++ui) {
        if (!emitted.contains(ui.key()))
            out += tagAttr(ui.key().toUtf8().constData(), ui.value());
    }
    out += QStringLiteral("/>");
    return out;
}
} // namespace

double ExtraPlotTypes::poleFigureScaleLimit(const Graph *g)
{
    double lim = 1.38;
    if (!g || !g->plotWidget())
        return lim;
    const Plot *pl = g->plotWidget();
    const QFont f = pl->axisFont(QwtPlot::xBottom);
    const QFont fAxis = pl->axisTitle(QwtPlot::xBottom).font();
    const QFontMetricsF fm(f);
    const QFontMetricsF fmAxis(fAxis);
    const double tickH = fm.height();
    const double tickW = fm.boundingRect(QStringLiteral("270") + QChar(0x00B0)).width();
    const double axisH = fmAxis.height();
    const double axisW = fmAxis.boundingRect(QStringLiteral("TD")).width();
    const double needPx = qMax(7.0 + 3.0 + tickH + 3.0 + axisH + 4.0,
                               7.0 + 3.0 + tickW + 6.0 + axisW + 4.0);
    const double zoom = qMax(0.25, pl->displayFontScale());
    const int sidePx = qMax(120, qRound(double(qMin(pl->width(), pl->height())) / zoom));
    const double halfSpanPx = 0.5 * double(sidePx);
    if (halfSpanPx > needPx + 24.0) {
        const double reqLim = 1.0 / (1.0 - needPx / halfSpanPx);
        lim = qBound(1.38, reqLim, 1.75);
    }
    return lim;
}

QStringList ExtraPlotTypes::syncedGraphTagsForState(const Graph *g)
{
    if (!g || !g->plotWidget())
        return QStringList();
    QStringList tags = g->plotWidget()->property(kExtraTagsProperty).toStringList();
    Spectrogram *sp = firstSpectrogram(const_cast<Graph *>(g));
    if (!sp)
        return tags;
    for (int i = 0; i < tags.size(); ++i) {
        const QString trimmed = tags[i].trimmed();
        if (trimmed.startsWith(QLatin1String("<ToriDensity2D"))) {
            QMap<QString, QString> updates;
            updates.insert(QStringLiteral("blocks"), sp->cellBlocks() ? QStringLiteral("1")
                                                                      : QStringLiteral("0"));
            updates.insert(QStringLiteral("log"), sp->logColorScale() ? QStringLiteral("1")
                                                                      : QStringLiteral("0"));
            tags[i] = rebuildTagWithUpdates(trimmed, QStringLiteral("ToriDensity2D"), updates);
        }
    }
    return tags;
}

bool ExtraPlotTypes::updateGraphTagsForRename(Graph *g, const QString &oldName,
                                              const QString &newName, bool updateTableName,
                                              bool updateMatrixName)
{
    if (!g || !g->plotWidget() || oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return false;
    Plot *p = g->plotWidget();
    QStringList tags = p->property(kExtraTagsProperty).toStringList();
    if (tags.isEmpty())
        return false;

    const QString oldPrefix = oldName + QLatin1Char('_');
    const QString newPrefix = newName + QLatin1Char('_');
    auto remapPrefixedCol = [&](const QString &val) -> QString {
        if (val.startsWith(oldPrefix))
            return newPrefix + val.mid(oldPrefix.size());
        return val;
    };

    bool anyChanged = false;
    for (int i = 0; i < tags.size(); ++i) {
        const QString trimmed = tags[i].trimmed();
        if (trimmed.startsWith(QLatin1String("<ToriDensity2D"))) {
            const QMap<QString, QString> a = parseTagAttrs(trimmed);
            QMap<QString, QString> updates;
            if (updateMatrixName) {
                if (a.value(QStringLiteral("matrix")) == oldName)
                    updates.insert(QStringLiteral("matrix"), newName);
            } else if (updateTableName) {
                if (a.value(QStringLiteral("table")) == oldName)
                    updates.insert(QStringLiteral("table"), newName);
                for (const char *k : { "x", "y" }) {
                    const QString key = QLatin1String(k);
                    const QString cur = a.value(key);
                    const QString nxt = remapPrefixedCol(cur);
                    if (nxt != cur)
                        updates.insert(key, nxt);
                }
            } else {
                const QString tagTbl = a.value(QStringLiteral("table"));
                const QString tblPfx = tagTbl + QLatin1Char('_');
                if (!tagTbl.isEmpty() && oldName.startsWith(tblPfx) && newName.startsWith(tblPfx)) {
                    const QString oldShort = oldName.mid(tblPfx.size());
                    const QString newShort = newName.mid(tblPfx.size());
                    for (const char *k : { "x", "y" }) {
                        const QString key = QLatin1String(k);
                        const QString cur = a.value(key);
                        if (cur == oldName)
                            updates.insert(key, newName);
                        else if (cur == oldShort)
                            updates.insert(key, newShort);
                    }
                }
            }
            if (!updates.isEmpty()) {
                tags[i] = rebuildTagWithUpdates(trimmed, QStringLiteral("ToriDensity2D"), updates);
                anyChanged = true;
            }
        } else if (trimmed.startsWith(QLatin1String("<ToriPoleFigure"))) {
            const QMap<QString, QString> a = parseTagAttrs(trimmed);
            QMap<QString, QString> updates;
            if (updateMatrixName) {
                for (const char *k : { "matrix", "sourceMatrix" }) {
                    const QString key = QLatin1String(k);
                    if (a.value(key) == oldName)
                        updates.insert(key, newName);
                }
            } else if (updateTableName) {
                if (a.value(QStringLiteral("table")) == oldName)
                    updates.insert(QStringLiteral("table"), newName);
                for (const char *k : { "a", "b", "i" }) {
                    const QString key = QLatin1String(k);
                    const QString cur = a.value(key);
                    const QString nxt = remapPrefixedCol(cur);
                    if (nxt != cur)
                        updates.insert(key, nxt);
                }
            } else {
                const QString tagTbl = a.value(QStringLiteral("table"));
                const QString tblPfx = tagTbl + QLatin1Char('_');
                if (!tagTbl.isEmpty() && oldName.startsWith(tblPfx) && newName.startsWith(tblPfx)) {
                    const QString oldShort = oldName.mid(tblPfx.size());
                    const QString newShort = newName.mid(tblPfx.size());
                    for (const char *k : { "a", "b", "i" }) {
                        const QString key = QLatin1String(k);
                        const QString cur = a.value(key);
                        if (cur == oldName)
                            updates.insert(key, newName);
                        else if (cur == oldShort)
                            updates.insert(key, newShort);
                    }
                }
            }
            if (!updates.isEmpty()) {
                tags[i] = rebuildTagWithUpdates(trimmed, QStringLiteral("ToriPoleFigure"), updates);
                anyChanged = true;
            }
        }
    }
    if (anyChanged)
        p->setProperty(kExtraTagsProperty, tags);
    return anyChanged;
}

bool ExtraPlotTypes::restoreGraphTag(Graph *g, const QString &line)
{
    if (!g)
        return false;
    const QString s = line.trimmed();
    if (s.startsWith(QLatin1String("<ToriDensity2D"))) {
        storeExtraTag(g, QStringLiteral("ToriDensity2D"), s);
        const QMap<QString, QString> a = parseTagAttrs(s);
        if (Spectrogram *sp = firstSpectrogram(g)) {
            if (a.contains(QStringLiteral("blocks")))
                sp->setCellBlocks(a.value(QStringLiteral("blocks")).toInt() != 0);
            if (a.contains(QStringLiteral("log")))
                sp->setLogColorScale(a.value(QStringLiteral("log")).toInt() != 0);
            sp->syncColorScale();
            g->replot();
        }
        return true;
    }
    if (s.startsWith(QLatin1String("<ToriPoleFigure"))) {
        storeExtraTag(g, QStringLiteral("ToriPoleFigure"), s);
        const QMap<QString, QString> a = parseTagAttrs(s);
        setupPoleFigurePage(g);
        const PoleProjection proj = (a.value(QStringLiteral("proj")).toInt() == int(EqualAngle))
                ? EqualAngle
                : EqualArea;
        attachPoleFrame(g, proj, a.value(QStringLiteral("zeroTop")).toInt() != 0,
                        a.value(QStringLiteral("cw")).toInt() != 0,
                        a.value(QStringLiteral("rdTop"), QStringLiteral("1")).toInt() != 0,
                        a.value(QStringLiteral("ax1")), a.value(QStringLiteral("ax2")),
                        a.value(QStringLiteral("rings")).toInt() != 0);
        g->replot();
        return true;
    }
    return false;
}

// ---- dialogs ---------------------------------------------------------------------------------

void ExtraPlotTypes::showDensity2DDialog(ApplicationWindow *app)
{
    if (!app || isHeadless(app))
        return;
    registerE11Translations();
    const QList<Table *> tables = projectTables(app);
    if (tables.isEmpty())
        return;
    Graph *ag = activeGraph(app);
    const QString existingTag = ag ? graphTag(ag, QStringLiteral("ToriDensity2D")) : QString();
    const bool isEditing = !existingTag.isEmpty();
    const QMap<QString, QString> existingAttrs =
            isEditing ? parseTagAttrs(existingTag) : QMap<QString, QString>();
    Spectrogram *existingSp = isEditing ? firstSpectrogram(ag) : nullptr;

    Table *act = isEditing ? app->table(existingAttrs.value(QStringLiteral("table"))) : nullptr;
    if (!act)
        act = activeTable(app);

    QDialog dlg(app);
    dlg.setWindowTitle(trE11("2D Density (2D Histogram)"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();
    auto *cbTable = new QComboBox(&dlg);
    for (Table *t : tables) {
        cbTable->addItem(t->name());
        if (t == act)
            cbTable->setCurrentIndex(cbTable->count() - 1);
    }
    auto *cbX = new QComboBox(&dlg);
    auto *cbY = new QComboBox(&dlg);
    bool applyInitialCols = isEditing;
    auto refresh = [&]() {
        cbX->clear();
        cbY->clear();
        Table *t = tables.value(cbTable->currentIndex());
        if (!t)
            return;
        QStringList numericCols;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (col && col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                numericCols << col->name();
                cbX->addItem(col->name());
                cbY->addItem(col->name());
            }
        }
        if (applyInitialCols && t == act) {
            applyInitialCols = false;
            const QString ex = existingAttrs.value(QStringLiteral("x"));
            const QString ey = existingAttrs.value(QStringLiteral("y"));
            if (numericCols.contains(ex))
                cbX->setCurrentText(ex);
            if (numericCols.contains(ey))
                cbY->setCurrentText(ey);
            return;
        }
        applyInitialCols = false;
        QStringList sel;
        for (const QString &full : t->selectedColumns()) {
            const QString nm = full.startsWith(t->name() + QStringLiteral("_"))
                    ? full.mid(t->name().size() + 1)
                    : full;
            if (numericCols.contains(nm))
                sel << nm;
        }
        if (sel.size() < 2)
            sel = numericCols;
        if (sel.size() > 0)
            cbX->setCurrentText(sel[0]);
        if (sel.size() > 1)
            cbY->setCurrentText(sel[1]);
    };
    refresh();
    QObject::connect(cbTable, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refresh(); });
    auto *sbBx = new QSpinBox(&dlg);
    sbBx->setRange(0, 4096);
    auto *sbBy = new QSpinBox(&dlg);
    sbBy->setRange(0, 4096);
    auto *sbWx = new QDoubleSpinBox(&dlg);
    sbWx->setDecimals(6);
    sbWx->setRange(0.0, 1e12);
    auto *sbWy = new QDoubleSpinBox(&dlg);
    sbWy->setDecimals(6);
    sbWy->setRange(0.0, 1e12);
    auto *cbMap = new QComboBox(&dlg);
    fillPresetCombo(cbMap);
    auto *chkReverse = new QCheckBox(trE11("Reverse color map"), &dlg);
    auto *chkLog = new QCheckBox(trE11("Logarithmic color scale"), &dlg);
    auto *chkCellBlocks = new QCheckBox(trE11("Flat cell blocks (no smoothing)"), &dlg);
    QCheckBox *chkUpdateExisting = nullptr;

    bool initLog = false;
    bool initBlocks = true;
    bool initReverse = false;
    int initPreset = int(Spectrogram::PresetViridis);
    if (isEditing) {
        sbBx->setValue(existingAttrs.value(QStringLiteral("bx")).toInt());
        sbBy->setValue(existingAttrs.value(QStringLiteral("by")).toInt());
        sbWx->setValue(existingAttrs.value(QStringLiteral("wx")).toDouble());
        sbWy->setValue(existingAttrs.value(QStringLiteral("wy")).toDouble());
        initLog = existingSp ? existingSp->logColorScale()
                             : (existingAttrs.value(QStringLiteral("log")).toInt() != 0);
        initBlocks = existingSp
                ? existingSp->cellBlocks()
                : (existingAttrs.value(QStringLiteral("blocks"), QStringLiteral("1")).toInt() != 0);
        if (existingSp) {
            initPreset = int(existingSp->colorMapPreset());
            initReverse = existingSp->reversePalette();
        }
    }
    int mapIdx = cbMap->findData(initPreset);
    if (mapIdx < 0) {
        cbMap->insertItem(0, trE11("Current / Custom"), initPreset);
        mapIdx = 0;
    }
    cbMap->setCurrentIndex(mapIdx);
    chkReverse->setChecked(initReverse);
    chkLog->setChecked(initLog);
    chkCellBlocks->setChecked(initBlocks);

    form->addRow(trE11("Source Table:"), cbTable);
    form->addRow(trE11("X column:"), cbX);
    form->addRow(trE11("Y column:"), cbY);
    form->addRow(trE11("Bins in X (0 = automatic):"), sbBx);
    form->addRow(trE11("Bins in Y (0 = automatic):"), sbBy);
    form->addRow(trE11("Bin width in X (0 = use bins):"), sbWx);
    form->addRow(trE11("Bin width in Y (0 = use bins):"), sbWy);
    form->addRow(trE11("Color map:"), cbMap);
    form->addRow(QString(), chkReverse);
    form->addRow(QString(), chkLog);
    form->addRow(QString(), chkCellBlocks);
    if (isEditing) {
        chkUpdateExisting =
                new QCheckBox(trE11("Update active plot (uncheck to create new)"), &dlg);
        chkUpdateExisting->setChecked(true);
        form->addRow(QString(), chkUpdateExisting);
    }
    lay->addLayout(form);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted)
        return;

    Table *selTable = tables.value(cbTable->currentIndex());
    Density2DResult r;
    if (isEditing && chkUpdateExisting && chkUpdateExisting->isChecked()) {
        r = updateDensity2D(app, ag, selTable, cbX->currentText(), cbY->currentText(),
                            sbBx->value(), sbBy->value(), sbWx->value(), sbWy->value(),
                            chkLog->isChecked(), cbMap->currentData().toInt(),
                            chkCellBlocks->isChecked(), chkReverse->isChecked());
    } else {
        r = plotDensity2D(app, selTable, cbX->currentText(), cbY->currentText(), sbBx->value(),
                          sbBy->value(), sbWx->value(), sbWy->value(), chkLog->isChecked(),
                          cbMap->currentData().toInt(), true);
        if (r.valid && r.plot) {
            if (Graph *ng = r.plot->activeGraph()) {
                if (!chkCellBlocks->isChecked() || chkReverse->isChecked()) {
                    applyDensity2DSpectrogramSettings(firstSpectrogram(ng),
                                                      cbMap->currentData().toInt(),
                                                      chkReverse->isChecked(),
                                                      chkLog->isChecked(),
                                                      chkCellBlocks->isChecked());
                    Column *cx = resolveColumn(selTable, cbX->currentText());
                    Column *cy = resolveColumn(selTable, cbY->currentText());
                    storeExtraTag(ng, QStringLiteral("ToriDensity2D"),
                                  buildDensity2DTag(r.matrix, selTable, cx, cy, sbBx->value(),
                                                    sbBy->value(), sbWx->value(), sbWy->value(),
                                                    chkLog->isChecked(),
                                                    chkCellBlocks->isChecked(), r));
                    ng->replot();
                }
            }
        }
    }
    if (!r.valid)
        QMessageBox::warning(app, trE11("2D Density (2D Histogram)"), r.error);
    else
        app->modifiedProject();
}

void ExtraPlotTypes::showPoleFigureDialog(ApplicationWindow *app)
{
    if (!app || isHeadless(app))
        return;
    registerE11Translations();
    const QList<Table *> tables = projectTables(app);
    QList<Matrix *> matrices;
    for (MyWidget *w : app->windowsList())
        if (w && w->inherits("Matrix"))
            matrices << static_cast<Matrix *>(w);
    if (tables.isEmpty() && matrices.isEmpty())
        return;
    Table *act = activeTable(app);
    Matrix *actMatrix = nullptr;
    if (QMdiSubWindow *sub = app->d_workspace.activeSubWindow()) {
        if (sub->inherits("Matrix"))
            actMatrix = static_cast<Matrix *>(sub);
        else if (sub->widget() && sub->widget()->inherits("Matrix"))
            actMatrix = static_cast<Matrix *>(sub->widget());
    }
    QDialog dlg(app);
    dlg.setWindowTitle(trE11("Pole Figure (XRD)"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();
    auto *cbSrc = new QComboBox(&dlg);
    for (Table *t : tables) {
        cbSrc->addItem(t->name(), QStringLiteral("T"));
        if (!actMatrix && t == act)
            cbSrc->setCurrentIndex(cbSrc->count() - 1);
    }
    for (Matrix *m : matrices) {
        cbSrc->addItem(m->name(), QStringLiteral("M"));
        if (m == actMatrix)
            cbSrc->setCurrentIndex(cbSrc->count() - 1);
    }
    auto *cbA = new QComboBox(&dlg);
    auto *cbB = new QComboBox(&dlg);
    auto *cbI = new QComboBox(&dlg);
    auto *lblMatrix = new QLabel(
            trE11("Matrix: rows = alpha (Y coordinates), columns = beta (X coordinates)"), &dlg);
    lblMatrix->setWordWrap(true);

    auto *matAlphaPane = new QWidget(&dlg);
    auto *matAlphaLay = new QHBoxLayout(matAlphaPane);
    matAlphaLay->setContentsMargins(0, 0, 0, 0);
    auto *sbAlphaMin = new QDoubleSpinBox(matAlphaPane);
    sbAlphaMin->setRange(0.0, 90.0);
    sbAlphaMin->setDecimals(2);
    sbAlphaMin->setSuffix(QString::fromUtf8("°"));
    sbAlphaMin->setValue(0.0);
    auto *sbAlphaMax = new QDoubleSpinBox(matAlphaPane);
    sbAlphaMax->setRange(0.0, 90.0);
    sbAlphaMax->setDecimals(2);
    sbAlphaMax->setSuffix(QString::fromUtf8("°"));
    sbAlphaMax->setValue(90.0);
    matAlphaLay->addWidget(sbAlphaMin, 1);
    matAlphaLay->addWidget(new QLabel(QStringLiteral("–"), matAlphaPane));
    matAlphaLay->addWidget(sbAlphaMax, 1);

    auto *matBetaPane = new QWidget(&dlg);
    auto *matBetaLay = new QHBoxLayout(matBetaPane);
    matBetaLay->setContentsMargins(0, 0, 0, 0);
    auto *sbBetaMin = new QDoubleSpinBox(matBetaPane);
    sbBetaMin->setRange(-360.0, 720.0);
    sbBetaMin->setDecimals(2);
    sbBetaMin->setSuffix(QString::fromUtf8("°"));
    sbBetaMin->setValue(0.0);
    auto *sbBetaMax = new QDoubleSpinBox(matBetaPane);
    sbBetaMax->setRange(-360.0, 720.0);
    sbBetaMax->setDecimals(2);
    sbBetaMax->setSuffix(QString::fromUtf8("°"));
    sbBetaMax->setValue(360.0);
    matBetaLay->addWidget(sbBetaMin, 1);
    matBetaLay->addWidget(new QLabel(QStringLiteral("–"), matBetaPane));
    matBetaLay->addWidget(sbBetaMax, 1);

    auto *lblMatAlpha = new QLabel(trE11("Alpha range (deg):"), &dlg);
    auto *lblMatBeta = new QLabel(trE11("Beta range (deg):"), &dlg);
    auto *lblColA = new QLabel(trE11("Alpha (tilt) column:"), &dlg);
    auto *lblColB = new QLabel(trE11("Beta (rotation) column:"), &dlg);
    auto *lblColI = new QLabel(trE11("Intensity column:"), &dlg);

    auto refresh = [&]() {
        cbA->clear();
        cbB->clear();
        cbI->clear();
        const bool isTable = cbSrc->currentData().toString() == QStringLiteral("T");
        lblColA->setVisible(isTable);
        lblColB->setVisible(isTable);
        lblColI->setVisible(isTable);
        cbA->setVisible(isTable);
        cbB->setVisible(isTable);
        cbI->setVisible(isTable);
        lblMatrix->setVisible(!isTable);
        lblMatAlpha->setVisible(!isTable);
        matAlphaPane->setVisible(!isTable);
        lblMatBeta->setVisible(!isTable);
        matBetaPane->setVisible(!isTable);
        if (!isTable) {
            if (Matrix *m = app->matrix(cbSrc->currentText())) {
                const bool isDefaultGridCoords = std::fabs(m->xStart() - 1.0) < 1e-6
                        && std::fabs(m->xEnd() - double(m->numCols())) < 1e-6
                        && std::fabs(m->yStart() - 1.0) < 1e-6
                        && std::fabs(m->yEnd() - double(m->numRows())) < 1e-6;
                const bool validAlpha = m->yStart() >= -1e-6 && m->yEnd() >= -1e-6
                        && m->yStart() <= 90.0 + 1e-6 && m->yEnd() <= 90.0 + 1e-6
                        && std::fabs(m->yEnd() - m->yStart()) > 1e-6;
                if (!isDefaultGridCoords && validAlpha) {
                    sbAlphaMin->setValue(qBound(0.0, m->yStart(), 90.0));
                    sbAlphaMax->setValue(qBound(0.0, m->yEnd(), 90.0));
                    sbBetaMin->setValue(m->xStart());
                    sbBetaMax->setValue(m->xEnd());
                } else {
                    sbAlphaMin->setValue(0.0);
                    sbAlphaMax->setValue(90.0);
                    sbBetaMin->setValue(0.0);
                    sbBetaMax->setValue(360.0);
                }
            }
            return;
        }
        Table *t = app->table(cbSrc->currentText());
        if (!t)
            return;
        QStringList numericCols;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (col && col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                numericCols << col->name();
                cbA->addItem(col->name());
                cbB->addItem(col->name());
                cbI->addItem(col->name());
            }
        }
        if (numericCols.size() > 0)
            cbA->setCurrentIndex(0);
        if (numericCols.size() > 1)
            cbB->setCurrentIndex(1);
        if (numericCols.size() > 2)
            cbI->setCurrentIndex(2);
    };
    refresh();
    QObject::connect(cbSrc, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refresh(); });
    auto *cbProj = new QComboBox(&dlg);
    cbProj->addItem(trE11("Equal-area (Schmidt)"), int(EqualArea));
    cbProj->addItem(trE11("Equal-angle (Wulff)"), int(EqualAngle));
    auto *chkMrd = new QCheckBox(trE11("Normalize to MRD (multiples of random)"), &dlg);
    chkMrd->setChecked(true);
    auto *chkRim = new QCheckBox(trE11("Alpha = 90° at the centre (Rigaku style)"), &dlg);
    auto *chkTop = new QCheckBox(trE11("Beta = 0° at the top"), &dlg);
    auto *chkCw = new QCheckBox(trE11("Beta increases clockwise"), &dlg);
    auto *chkRings = new QCheckBox(trE11("Show 30° and 60° tilt circles"), &dlg);

    auto *cbRefAxes = new QComboBox(&dlg);
    cbRefAxes->addItem(trE11("RD / TD (Rolled sheet)"), QStringLiteral("RD|TD"));
    cbRefAxes->addItem(trE11("ED / TD (Extruded)"), QStringLiteral("ED|TD"));
    cbRefAxes->addItem(trE11("LD / TD (Longitudinal / Transverse)"), QStringLiteral("LD|TD"));
    cbRefAxes->addItem(trE11("Custom..."), QStringLiteral("CUSTOM"));

    auto *customAxesPane = new QWidget(&dlg);
    auto *customAxesLay = new QHBoxLayout(customAxesPane);
    customAxesLay->setContentsMargins(0, 0, 0, 0);
    auto *edAx1 = new QLineEdit(QStringLiteral("RD"), customAxesPane);
    auto *edAx2 = new QLineEdit(QStringLiteral("TD"), customAxesPane);
    customAxesLay->addWidget(edAx1, 1);
    customAxesLay->addWidget(new QLabel(QStringLiteral("/"), customAxesPane));
    customAxesLay->addWidget(edAx2, 1);
    auto *lblCustomAxes = new QLabel(trE11("Custom axis labels:"), &dlg);
    lblCustomAxes->setVisible(false);
    customAxesPane->setVisible(false);
    QObject::connect(cbRefAxes, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) {
                         const bool isCustom =
                                 cbRefAxes->currentData().toString() == QStringLiteral("CUSTOM");
                         lblCustomAxes->setVisible(isCustom);
                         customAxesPane->setVisible(isCustom);
                     });

    auto *cbRd = new QComboBox(&dlg);
    cbRd->addItem(trE11("Top (secondary right)"));
    cbRd->addItem(trE11("Right (secondary top)"));
    auto *sbLevels = new QSpinBox(&dlg);
    sbLevels->setRange(2, 50);
    sbLevels->setValue(8);
    auto *edLabel = new QLineEdit(&dlg);
    auto *cbMap = new QComboBox(&dlg);
    fillPresetCombo(cbMap);
    form->addRow(trE11("Source:"), cbSrc);
    form->addRow(QString(), lblMatrix);
    form->addRow(lblMatAlpha, matAlphaPane);
    form->addRow(lblMatBeta, matBetaPane);
    form->addRow(lblColA, cbA);
    form->addRow(lblColB, cbB);
    form->addRow(lblColI, cbI);
    form->addRow(trE11("Projection:"), cbProj);
    form->addRow(QString(), chkMrd);
    form->addRow(QString(), chkRim);
    form->addRow(QString(), chkTop);
    form->addRow(QString(), chkCw);
    form->addRow(QString(), chkRings);
    form->addRow(trE11("Sample reference axes:"), cbRefAxes);
    form->addRow(lblCustomAxes, customAxesPane);
    form->addRow(trE11("Primary axis direction:"), cbRd);
    form->addRow(trE11("Contour levels:"), sbLevels);
    form->addRow(trE11("Label (e.g. (111)):"), edLabel);
    form->addRow(trE11("Color map:"), cbMap);
    lay->addLayout(form);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted)
        return;
    PoleFigureOptions opt;
    opt.projection = static_cast<PoleProjection>(cbProj->currentData().toInt());
    opt.normalizeMRD = chkMrd->isChecked();
    opt.alphaFromRim = chkRim->isChecked();
    opt.betaZeroAtTop = chkTop->isChecked();
    opt.clockwise = chkCw->isChecked();
    opt.showTiltCircles = chkRings->isChecked();
    opt.rdTop = cbRd->currentIndex() == 0;
    const QString refSpec = cbRefAxes->currentData().toString();
    if (refSpec == QStringLiteral("CUSTOM")) {
        opt.axis1Label = edAx1->text().trimmed();
        opt.axis2Label = edAx2->text().trimmed();
    } else {
        opt.axis1Label = refSpec.section(QLatin1Char('|'), 0, 0);
        opt.axis2Label = refSpec.section(QLatin1Char('|'), 1, 1);
    }
    opt.levels = sbLevels->value();
    opt.label = edLabel->text();
    opt.preset = cbMap->currentData().toInt();
    PoleFigureResult r;
    if (cbSrc->currentData().toString() == QStringLiteral("T")) {
        r = plotPoleFigure(app, app->table(cbSrc->currentText()), cbA->currentText(),
                           cbB->currentText(), cbI->currentText(), opt, true);
    } else {
        opt.useCustomMatrixAngles = true;
        opt.matrixAlphaMin = sbAlphaMin->value();
        opt.matrixAlphaMax = sbAlphaMax->value();
        opt.matrixBetaMin = sbBetaMin->value();
        opt.matrixBetaMax = sbBetaMax->value();
        r = plotPoleFigureFromMatrix(app, app->matrix(cbSrc->currentText()), opt, true);
    }
    if (!r.valid)
        QMessageBox::warning(app, trE11("Pole Figure (XRD)"), r.error);
    else
        app->modifiedProject();
}
