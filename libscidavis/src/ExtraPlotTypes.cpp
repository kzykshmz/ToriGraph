/***************************************************************************
    File                 : ExtraPlotTypes.cpp
    Project              : ToriGraph
    Description          : E11 graph types (error band, colour-mapped scatter,
                           histogram + cumulative %, lognormal / Gumbel probability plots)
 ***************************************************************************/
#include "ExtraPlotTypes.h"

#include "ApplicationWindow.h"
#include "Graph.h"
#include "I18nJapanese.h"
#include "Legend.h"
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
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QPointer>
#include <QSpinBox>
#include <QVBoxLayout>

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
        reg("Count", "度数");
    });
}

QString trE11(const char *s)
{
    registerE11Translations();
    return QObject::tr(s);
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

QString columnAxisTitle(Column *c)
{
    if (!c)
        return QString();
    QString t = c->comment().section(QLatin1Char('\n'), 0, 0).trimmed();
    if (t.isEmpty())
        t = c->name();
    const QString u = c->units().trimmed();
    if (!u.isEmpty() && !t.contains(u))
        t += QStringLiteral(" (") + u + QStringLiteral(")");
    return t;
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

Table *activeTable(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    if (QMdiSubWindow *sub = app->d_workspace.activeSubWindow()) {
        if (sub->inherits("Table"))
            return static_cast<Table *>(sub);
        if (QWidget *w = sub->widget())
            if (w->inherits("Table"))
                return static_cast<Table *>(w);
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
    QAction *actions[ExtraPlotTypes::ActCount] = { nullptr, nullptr, nullptr, nullptr, nullptr };

    void retranslate()
    {
        static const char *const kText[ExtraPlotTypes::ActCount] = {
            "Add Error &Band...", "Color-Mapped &Scatter (Z)...", "Histogram + &Cumulative %...",
            "&Lognormal Probability Plot...", "&Gumbel (Extreme Value) Probability Plot..."
        };
        static const char *const kTip[ExtraPlotTypes::ActCount] = {
            "Shade the area between Y - error and Y + error from error columns",
            "Scatter plot whose symbol color follows a third column (and size a fourth), with a color bar",
            "Frequency histogram with the cumulative percentage on the right axis",
            "Lognormal probability paper: rank plotting positions and a straight-line fit (mu, sigma)",
            "Gumbel extreme value plot (e.g. maximum defect size): location, scale and return-period value"
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
                                                "actionE11GumbelProbability" };
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
    if (addErrorBand(g, curves[idx]->title().text(), curves[idx]->table(),
                     cbPlus->currentData().toString(), minusCol, alpha)
        == 0) {
        QMessageBox::warning(app, trE11("Error Band"),
                             trE11("The error band could not be added (check the error columns)."));
    } else {
        app->modifiedProject();
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
    pm.colorBarTitle = columnAxisTitle(cz);
    dc->setPointColorMap(pm);

    g->setXAxisTitle(columnAxisTitle(cx));
    g->setYAxisTitle(columnAxisTitle(cy));
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

    MultiLayer *ml = app->newGraph(
            app->generateUniqueName(QStringLiteral("HistCum_") + t->name(), false));
    Graph *g = ml ? ml->activeGraph() : nullptr;
    res.plot = ml;
    if (!g)
        return res;
    const QString full = fullColumnName(t, c);
    if (!g->plotHistogram(t, QStringList(full)))
        return res;
    auto *h = dynamic_cast<QwtHistogram *>(g->curve(g->curves() - 1));
    if (!h)
        return res;
    if (binWidth > 0.0 && std::isfinite(binWidth)) {
        if (!std::isfinite(begin) || begin > vMin)
            begin = std::floor(vMin / binWidth) * binWidth;
        int nb = qMax(1, int(std::ceil((vMax - begin) / binWidth)));
        if (begin + nb * binWidth <= vMax)
            ++nb;
        h->setBinning(false, binWidth, begin, begin + nb * binWidth);
        h->loadData();
    }

    const int bins = h->dataSize();
    if (bins <= 0)
        return res;
    double w = h->binSize();
    if (!(w > 0.0) && bins >= 2)
        w = h->x(1) - h->x(0);
    double total = 0.0;
    for (int i = 0; i < bins; ++i)
        total += h->y(i);
    if (!(total > 0.0))
        return res;

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
    if (!tbl)
        return res;
    tbl->setHeader(QStringList() << QStringLiteral("UpperEdge") << QStringLiteral("CumCount")
                                 << QStringLiteral("CumulativePct"));
    for (int k = 0; k < 3; ++k)
        tbl->column(k)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    tbl->column(0)->setPlotDesignation(SciDAVis::X);
    tbl->column(1)->setPlotDesignation(SciDAVis::Y);
    tbl->column(2)->setPlotDesignation(SciDAVis::Y);
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
    g->setXAxisTitle(columnAxisTitle(c));
    g->setYAxisTitle(trE11("Count"));
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
    auto *sbW = new QDoubleSpinBox(&dlg);
    sbW->setDecimals(6);
    sbW->setRange(0.0, 1e12);
    sbW->setValue(0.0);
    auto *sbBegin = new QDoubleSpinBox(&dlg);
    sbBegin->setDecimals(6);
    sbBegin->setRange(-1e12, 1e12);
    sbBegin->setValue(0.0);
    form->addRow(trE11("Source Table:"), cbTable);
    form->addRow(trE11("Data column:"), cbCol);
    form->addRow(trE11("Bin width (0 = automatic):"), sbW);
    form->addRow(trE11("Bin start:"), sbBegin);
    lay->addLayout(form);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const HistogramCumulativeResult r = plotHistogramCumulative(
            app, tables.value(cbTable->currentIndex()), cbCol->currentText(), sbW->value(),
            sbBegin->value());
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
    for (int i = 0; i < n; ++i) {
        tbl->column(0)->setValueAt(i, vals[i]);
        tbl->column(1)->setValueAt(i, i + 1);
        tbl->column(2)->setValueAt(i, 100.0 * F[i]);
        tbl->column(3)->setValueAt(i, var[i]);
    }
    const double fitV[2] = { var[0] - 0.6 * vPad, var[n - 1] + 0.6 * vPad };
    for (int k = 0; k < 2; ++k) {
        const double uu = a + b * fitV[k];
        tbl->column(4)->setValueAt(k, dist == LogNormal ? std::exp(uu) : uu);
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
        xLo = std::pow(10.0, l0 - pad);
        xHi = std::pow(10.0, l1 + pad);
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

    g->setXAxisTitle(columnAxisTitle(c));
    g->setYAxisTitle(trE11("Cumulative probability, F (%)"));
    g->setTitle(QString());
    refreshLegend(g);

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
                          .arg(std::exp(a), 0, 'g', 5)
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
    const double bx = (dist == LogNormal) ? xLo * std::pow(xHi / xLo, 0.04)
                                          : xLo + 0.04 * (xHi - xLo);
    addSummaryBox(g, bx, vHi - 0.04 * (vHi - vLo), summary);
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
