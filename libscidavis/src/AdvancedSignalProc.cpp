/***************************************************************************
    File                 : AdvancedSignalProc.cpp
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Description          : OriginPro Advanced Signal Processing:
                           IIR Digital Filters (Butterworth/Chebyshev I/Bessel,
                           Zero-Phase filtfilt), STFT Spectrogram, Hilbert
                           Envelope, Wavelet CWT & DWT VisuShrink Denoising,
                           2D Matrix FFT/IFFT, Welch PSD & Cross-Coherence
 ***************************************************************************/

#include "AdvancedSignalProc.h"

#include "ApplicationWindow.h"
#include "Graph.h"
#include "I18nJapanese.h"
#include "Legend.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "RecalcEngine.h"
#include "Spectrogram.h"
#include "Table.h"
#include "core/column/Column.h"

#include <gsl/gsl_fft_complex.h>
#include <gsl/gsl_fft_halfcomplex.h>
#include <gsl/gsl_fft_real.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_poly.h>
#include <gsl/gsl_spline.h>

#include <qwt_plot_curve.h>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numeric>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

using cdbl = std::complex<double>;

inline QString trSig(const char *text)
{
    return QObject::tr(text);
}

void registerSignalProcTranslationsOnce()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Live Preview"), QStringLiteral("ライブプレビュー"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Detected Sampling Rate:"), QStringLiteral("検出サンプリング周波数:"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("%1 Hz (Nyquist: %2 Hz)"), QStringLiteral("%1 Hz (ナイキスト: %2 Hz)"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("IIR Digital Filter"), QStringLiteral("IIRデジタルフィルタ"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Savitzky-Golay Derivative"), QStringLiteral("Savitzky-Golay 微分"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Smooth Peak Envelope"), QStringLiteral("平滑ピーク包絡線"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Signal Decimation / Resampling"),
            QStringLiteral("信号の間引き / リサンプリング"));
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(
                QString::fromUtf8(en), QString::fromUtf8(ja));
    };
    reg("2D Spatial Filter & Plane Leveling", "2D空間フィルタ・平面傾斜補正");
    reg("2D Spatial Filter & Plane Leveling...", "2D空間フィルタ・平面傾斜補正...");
    reg("Source Matrix:", "入力マトリクス:");
    reg("2D Filter Method:", "2Dフィルタ手法:");
    reg("Gaussian Smoothing (2D)", "2Dガウシアン平滑化");
    reg("Median Despike Filter (Cosmic-Ray Removal)",
        "2Dメディアンフィルタ (スパイク・宇宙線ノイズ除去)");
    reg("Laplacian Sharpening (2D)", "2Dラプラシアン先鋭化");
    reg("Sobel Gradient Magnitude (|∇Z|)", "2Dソーベル勾配強度 (|∇Z|)");
    reg("Subtract Least-Squares Planar Tilt (AFM/STM Leveling)",
        "最小二乗平面傾斜減算 (AFM/STMレベリング)");
    reg("Gaussian Sigma (pixels):", "ガウシアン標準偏差 σ (ピクセル):");
    reg("Sharpening Strength:", "先鋭化強度:");
    reg("Parameter:", "パラメータ:");
    reg("Kernel Radius (pixels):", "カーネル半径 (ピクセル):");
    reg("Modify Source Matrix In-Place (Undoable)", "元マトリクスを直接更新 (元に戻す対応)");
    reg("Plot Filtered 2D Heatmap", "フィルタ後の2Dヒートマップを作成");
    reg("Please open a Matrix window first.", "先にマトリクスウィンドウを開いてください。");
    reg("2D Matrix Filter", "2Dマトリクスフィルタ");
    reg("Fourier Self-Deconvolution (Peak Sharpening)", "フーリエ自己デコンボリューション (ピーク先鋭化)");
    reg("Fourier Self-Deconvolution (Peak Sharpening)...",
        "フーリエ自己デコンボリューション (ピーク先鋭化)...");
    reg("Lorentzian FWHM γ (X units):", "ローレンツ半値全幅 γ (X軸単位):");
    reg("Sharpening Factor K (t_max = K / γ):", "先鋭化係数 K (t_max = K / γ):");
    reg("Apodization Function:", "アポダイゼーション関数:");
    reg("Bessel / Kauppinen (1 - |t|/t_max)^2", "ベッセル / Kauppinen型 (1 - |t|/t_max)^2");
    reg("Gaussian Apodization", "ガウシアン・アポダイゼーション");
    reg("Please open a graph with at least one curve first.",
        "1本以上の曲線を含むグラフを開いてください。");
}

class SignalPreviewGuard
{
public:
    explicit SignalPreviewGuard(Graph *g) : d_graph(g) {}
    ~SignalPreviewGuard() { clear(); }

    void setSingle(const std::vector<double> &xs, const std::vector<double> &ys, const QPen &pen)
    {
        if (!d_graph || !d_graph->plotWidget() || xs.size() < 2 || xs.size() != ys.size()) {
            clear();
            return;
        }
        detachCurve(d_curveB);
        if (!d_curveA) {
            d_curveA = new QwtPlotCurve(QStringLiteral("__SignalLivePreviewA__"));
            d_curveA->setItemAttribute(QwtPlotItem::Legend, false);
            d_curveA->setRenderHint(QwtPlotItem::RenderAntialiased, true);
            d_curveA->attach(d_graph->plotWidget());
        }
        d_curveA->setPen(pen);
        d_curveA->setData(xs.data(), ys.data(), static_cast<int>(xs.size()));
        d_graph->plotWidget()->replot();
    }

    void setDual(const std::vector<double> &xs,
                 const std::vector<double> *ysA, const QPen &penA,
                 const std::vector<double> *ysB, const QPen &penB)
    {
        if (!d_graph || !d_graph->plotWidget() || xs.size() < 2) {
            clear();
            return;
        }
        if (ysA && ysA->size() == xs.size()) {
            if (!d_curveA) {
                d_curveA = new QwtPlotCurve(QStringLiteral("__SignalLivePreviewA__"));
                d_curveA->setItemAttribute(QwtPlotItem::Legend, false);
                d_curveA->setRenderHint(QwtPlotItem::RenderAntialiased, true);
                d_curveA->attach(d_graph->plotWidget());
            }
            d_curveA->setPen(penA);
            d_curveA->setData(xs.data(), ysA->data(), static_cast<int>(xs.size()));
        } else {
            detachCurve(d_curveA);
        }

        if (ysB && ysB->size() == xs.size()) {
            if (!d_curveB) {
                d_curveB = new QwtPlotCurve(QStringLiteral("__SignalLivePreviewB__"));
                d_curveB->setItemAttribute(QwtPlotItem::Legend, false);
                d_curveB->setRenderHint(QwtPlotItem::RenderAntialiased, true);
                d_curveB->attach(d_graph->plotWidget());
            }
            d_curveB->setPen(penB);
            d_curveB->setData(xs.data(), ysB->data(), static_cast<int>(xs.size()));
        } else {
            detachCurve(d_curveB);
        }
        d_graph->plotWidget()->replot();
    }

    void clear()
    {
        const bool hadAny = (d_curveA != nullptr || d_curveB != nullptr);
        detachCurve(d_curveA);
        detachCurve(d_curveB);
        if (hadAny && d_graph && d_graph->plotWidget())
            d_graph->plotWidget()->replot();
    }

private:
    static void detachCurve(QwtPlotCurve *&c)
    {
        if (c) {
            c->detach();
            delete c;
            c = nullptr;
        }
    }

    Graph *d_graph = nullptr;
    QwtPlotCurve *d_curveA = nullptr;
    QwtPlotCurve *d_curveB = nullptr;
};

QwtPlotCurve *resolveSignalCurve(Graph *g, const QString &curveTitle)
{
    if (!g || g->curves() <= 0)
        return nullptr;

    if (!curveTitle.isEmpty()) {
        if (QwtPlotCurve *exact = g->curve(curveTitle))
            return exact;

        for (int i = 0; i < g->curves(); ++i) {
            QwtPlotCurve *c = g->curve(i);
            if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
                continue;
            if (c->title().text() == curveTitle)
                return c;
        }

        for (int i = 0; i < g->curves(); ++i) {
            QwtPlotCurve *c = g->curve(i);
            if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
                continue;
            if (auto *dc = dynamic_cast<DataCurve *>(c)) {
                if (Table *tbl = dc->table()) {
                    const QString expected = tbl->name() + QStringLiteral("_") + curveTitle;
                    if (dc->yColumnName() == expected || dc->yColumnName() == curveTitle)
                        return c;
                }
            }
        }

        for (int i = 0; i < g->curves(); ++i) {
            QwtPlotCurve *c = g->curve(i);
            if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
                continue;
            if (c->title().text().compare(curveTitle, Qt::CaseInsensitive) == 0)
                return c;
        }

        for (int i = 0; i < g->curves(); ++i) {
            QwtPlotCurve *c = g->curve(i);
            if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
                continue;
            if (auto *dc = dynamic_cast<DataCurve *>(c)) {
                if (Table *tbl = dc->table()) {
                    const QString expected = tbl->name() + QStringLiteral("_") + curveTitle;
                    if (dc->yColumnName().compare(expected, Qt::CaseInsensitive) == 0
                        || dc->yColumnName().compare(curveTitle, Qt::CaseInsensitive) == 0)
                        return c;
                }
            }
        }

        const QString suffix = QStringLiteral("_") + curveTitle;
        for (int i = 0; i < g->curves(); ++i) {
            QwtPlotCurve *c = g->curve(i);
            if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
                continue;
            const auto *dc = dynamic_cast<const DataCurve *>(c);
            if (dc && dc->table())
                continue;
            const QString title = c->title().text();
            if (title.endsWith(suffix) || title.endsWith(suffix, Qt::CaseInsensitive))
                return c;
        }
        return nullptr;
    }

    const QString sel = g->selectedCurveTitle();
    if (!sel.isEmpty()) {
        if (QwtPlotCurve *c = g->curve(sel))
            return c;
    }
    const QStringList analysable = g->analysableCurvesList();
    if (!analysable.isEmpty())
        return g->curve(analysable.first());
    return g->curve(0);
}

// What extractUniformSignal() did to the input (sorting, duplicate merging, resampling).
// Single GUI thread: read it right after the extraction call.
QString s_lastSignalPrepNote;

void registerSignalPrepNoticeTranslationsOnce()
{
    static bool s_done = false;
    if (s_done)
        return;
    s_done = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                  QString::fromUtf8(ja));
    };
    reg("points re-ordered by X", "点を X の順に並べ替え");
    reg("%1 duplicate X merged (Y averaged)", "重複した X を %1 点まとめた (Y は平均)");
    reg("linearly resampled to %1 equally spaced points (dt = %2)",
        "線形補間で等間隔の %1 点に再サンプリング (dt = %2)");
    reg("%1: input prepared (%2): %3", "%1: 入力を前処理しました (%2): %3");
}

void reportSignalPrep(ApplicationWindow *app, const QString &opName, const QString &curveTitle,
                      const QString &note)
{
    if (!app || note.isEmpty())
        return;
    registerSignalPrepNoticeTranslationsOnce();
    app->updateLog(QStringLiteral("[%1] ").arg(QLocale().toString(QDateTime::currentDateTime()))
                   + QObject::tr("%1: input prepared (%2): %3").arg(opName, curveTitle, note)
                   + QLatin1Char('\n'));
}

bool extractSortedSignalXY(Graph *g, const QString &curveTitle,
                           std::vector<double> &xOut, std::vector<double> &yOut)
{
    xOut.clear();
    yOut.clear();

    QwtPlotCurve *c = resolveSignalCurve(g, curveTitle);
    if (!c || c->dataSize() < 2)
        return false;

    DataCurve *dc = dynamic_cast<DataCurve *>(c);
    Table *tbl = dc ? dc->table() : nullptr;
    Column *xCol = (tbl && dc) ? tbl->column(dc->xColumnName()) : nullptr;
    Column *yCol = (tbl && dc) ? tbl->column(dc->yColumnName()) : nullptr;

    const int nRaw = c->dataSize();
    std::vector<std::pair<double, double>> pts;
    pts.reserve(static_cast<size_t>(nRaw));

    for (int i = 0; i < nRaw; ++i) {
        if (dc && tbl) {
            int row = dc->tableRow(i);
            if (row < 0 && yCol && nRaw == yCol->rowCount())
                row = i;
            if (row >= 0) {
                if ((xCol && xCol->isMasked(row)) || (yCol && yCol->isMasked(row)))
                    continue;
            }
        }
        const double xv = c->x(i);
        const double yv = c->y(i);
        if (std::isfinite(xv) && std::isfinite(yv))
            pts.emplace_back(xv, yv);
    }

    if (pts.size() < 2)
        return false;

    std::stable_sort(pts.begin(), pts.end(),
                     [](const std::pair<double, double> &a, const std::pair<double, double> &b) {
                         return a.first < b.first;
                     });

    const double rawSpan = pts.back().first - pts.front().first;
    if (rawSpan <= 0.0)
        return false;

    const double mergeEps = 1e-13 * std::max(1.0, rawSpan);
    xOut.reserve(pts.size());
    yOut.reserve(pts.size());
    size_t idx = 0;
    while (idx < pts.size()) {
        double sx = pts[idx].first;
        double sy = pts[idx].second;
        int cnt = 1;
        size_t j = idx + 1;
        while (j < pts.size() && std::fabs(pts[j].first - pts[idx].first) <= mergeEps) {
            sx += pts[j].first;
            sy += pts[j].second;
            ++cnt;
            ++j;
        }
        xOut.push_back(sx / cnt);
        yOut.push_back(sy / cnt);
        idx = j;
    }

    return xOut.size() >= 2;
}

bool extractUniformSignal(Graph *g, const QString &curveTitle,
                          std::vector<double> &tOut, std::vector<double> &yOut, double &fsOut)
{
    tOut.clear();
    yOut.clear();
    fsOut = 1.0;
    s_lastSignalPrepNote.clear();
    registerSignalPrepNoticeTranslationsOnce();
    QStringList prepParts;

    QwtPlotCurve *c = resolveSignalCurve(g, curveTitle);
    if (!c || c->dataSize() < 2)
        return false;

    DataCurve *dc = dynamic_cast<DataCurve *>(c);
    Table *tbl = dc ? dc->table() : nullptr;
    Column *xCol = (tbl && dc) ? tbl->column(dc->xColumnName()) : nullptr;
    Column *yCol = (tbl && dc) ? tbl->column(dc->yColumnName()) : nullptr;

    const int nRaw = c->dataSize();
    std::vector<std::pair<double, double>> pts;
    pts.reserve(static_cast<size_t>(nRaw));

    for (int i = 0; i < nRaw; ++i) {
        if (dc && tbl) {
            int row = dc->tableRow(i);
            if (row < 0 && yCol && nRaw == yCol->rowCount())
                row = i;
            if (row >= 0) {
                if ((xCol && xCol->isMasked(row)) || (yCol && yCol->isMasked(row)))
                    continue;
            }
        }
        const double tv = c->x(i);
        const double yv = c->y(i);
        if (std::isfinite(tv) && std::isfinite(yv))
            pts.emplace_back(tv, yv);
    }

    if (pts.size() < 2)
        return false;

    const bool wasSorted = std::is_sorted(
            pts.begin(), pts.end(),
            [](const std::pair<double, double> &a, const std::pair<double, double> &b) {
                return a.first < b.first;
            });
    if (!wasSorted)
        prepParts << QObject::tr("points re-ordered by X");
    std::stable_sort(pts.begin(), pts.end(),
                     [](const std::pair<double, double> &a, const std::pair<double, double> &b) {
                         return a.first < b.first;
                     });

    // Merge duplicate timestamps if any
    const double rawSpan = pts.back().first - pts.front().first;
    if (rawSpan <= 0.0)
        return false;

    const double mergeEps = 1e-13 * std::max(1.0, rawSpan);
    std::vector<double> xs, ys;
    xs.reserve(pts.size());
    ys.reserve(pts.size());
    size_t idx = 0;
    while (idx < pts.size()) {
        double sx = pts[idx].first;
        double sy = pts[idx].second;
        int cnt = 1;
        size_t j = idx + 1;
        while (j < pts.size() && std::fabs(pts[j].first - pts[idx].first) <= mergeEps) {
            sx += pts[j].first;
            sy += pts[j].second;
            ++cnt;
            ++j;
        }
        xs.push_back(sx / cnt);
        ys.push_back(sy / cnt);
        idx = j;
    }

    const size_t n = xs.size();
    if (n < 2)
        return false;
    if (n < pts.size())
        prepParts << QObject::tr("%1 duplicate X merged (Y averaged)").arg(int(pts.size() - n));

    const double dt = (xs.back() - xs.front()) / static_cast<double>(n - 1);
    if (dt <= 0.0 || !std::isfinite(dt))
        return false;
    fsOut = 1.0 / dt;

    bool isUniform = true;
    for (size_t i = 0; i < n; ++i) {
        const double expected = xs.front() + static_cast<double>(i) * dt;
        if (std::fabs(xs[i] - expected) > 1e-3 * dt) {
            isUniform = false;
            break;
        }
    }

    if (isUniform) {
        s_lastSignalPrepNote = prepParts.join(QStringLiteral("; "));
        tOut = std::move(xs);
        yOut = std::move(ys);
        return true;
    }
    prepParts << QObject::tr("linearly resampled to %1 equally spaced points (dt = %2)")
                         .arg(int(n))
                         .arg(dt, 0, 'g', 6);
    s_lastSignalPrepNote = prepParts.join(QStringLiteral("; "));

    // Linearly resample onto uniform grid
    tOut.resize(n);
    yOut.resize(n);
    size_t seg = 0;
    for (size_t i = 0; i < n; ++i) {
        const double tq = (i == n - 1) ? xs.back() : (xs.front() + static_cast<double>(i) * dt);
        tOut[i] = tq;
        while (seg + 1 < n && xs[seg + 1] < tq)
            ++seg;
        if (seg + 1 >= n) {
            yOut[i] = ys.back();
        } else {
            const double dx = xs[seg + 1] - xs[seg];
            const double alpha = (dx > 1e-300) ? ((tq - xs[seg]) / dx) : 0.0;
            yOut[i] = ys[seg] + alpha * (ys[seg + 1] - ys[seg]);
        }
    }
    return true;
}

QString uniqueWinName(ApplicationWindow *app, const QString &prefix)
{
    QString clean = prefix;
    clean.replace('_', "");
    QString name = app->generateUniqueName(clean);
    while (app->alreadyUsedName(name))
        name = app->generateUniqueName(clean);
    return name;
}

Table *buildSignalTable(ApplicationWindow *app, const QString &prefix,
                        const QStringList &colNames,
                        const QList<SciDAVis::PlotDesignation> &pds,
                        const std::vector<std::vector<double>> &cols)
{
    if (!app || cols.empty() || cols[0].empty())
        return nullptr;

    const int nRows = static_cast<int>(cols[0].size());
    const int nCols = static_cast<int>(cols.size());
    Table *t = app->newTable(uniqueWinName(app, prefix), nRows, nCols);
    if (!t)
        return nullptr;

    for (int c = 0; c < nCols; ++c) {
        QString label = (c < colNames.size()) ? colNames[c] : QString("Col%1").arg(c + 1);
        label.replace('_', '-');
        t->setColName(c, label);
        t->setColPlotDesignation(c, (c < pds.size()) ? pds[c]
                                                     : (c == 0 ? SciDAVis::X : SciDAVis::Y));
        if (Column *colPtr = t->column(c)) {
            QVector<qreal> buf(nRows);
            for (int r = 0; r < nRows; ++r)
                buf[r] = cols[static_cast<size_t>(c)][static_cast<size_t>(r)];
            colPtr->replaceValues(0, buf);
        } else {
            for (int r = 0; r < nRows; ++r)
                t->setCell(r, c, cols[static_cast<size_t>(c)][static_cast<size_t>(r)]);
        }
    }
    return t;
}

void runComplexFft1D(std::vector<double> &interleaved, size_t n, bool inverse)
{
    if (n <= 1)
        return;
    gsl_fft_complex_wavetable *wt = gsl_fft_complex_wavetable_alloc(n);
    gsl_fft_complex_workspace *ws = gsl_fft_complex_workspace_alloc(n);
    if (wt && ws) {
        if (inverse)
            gsl_fft_complex_inverse(interleaved.data(), 1, n, wt, ws);
        else
            gsl_fft_complex_forward(interleaved.data(), 1, n, wt, ws);
    }
    if (wt)
        gsl_fft_complex_wavetable_free(wt);
    if (ws)
        gsl_fft_complex_workspace_free(ws);
}

// ============================================================================
// IIR Biquad (SOS) Design Helpers
// ============================================================================
struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0; // a0 = 1.0
};

std::vector<cdbl> computePrototypePoles(AdvancedSignalProc::IirDesign design, int order,
                                        double rippleDb)
{
    const int M = std::clamp(order, 1, 12);
    std::vector<cdbl> poles;
    poles.reserve(static_cast<size_t>(M));

    if (design == AdvancedSignalProc::Butterworth) {
        for (int k = 1; k <= M; ++k) {
            const double angle = M_PI * static_cast<double>(2 * k + M - 1)
                    / static_cast<double>(2 * M);
            poles.emplace_back(std::cos(angle), std::sin(angle));
        }
    } else if (design == AdvancedSignalProc::ChebyshevI) {
        const double rip = std::max(0.01, rippleDb);
        const double eps = std::sqrt(std::max(1e-8, std::pow(10.0, rip / 10.0) - 1.0));
        const double beta = std::asinh(1.0 / eps) / static_cast<double>(M);
        const double sh = std::sinh(beta);
        const double ch = std::cosh(beta);
        for (int k = 1; k <= M; ++k) {
            const double theta = M_PI * static_cast<double>(2 * k - 1) / static_cast<double>(2 * M);
            poles.emplace_back(-sh * std::sin(theta), ch * std::cos(theta));
        }
    } else {
        // Bessel: roots of reverse Bessel polynomial theta_M(s), normalized to -3 dB at Omega = 1
        std::vector<double> coeffs(static_cast<size_t>(M + 1), 1.0);
        coeffs[static_cast<size_t>(M)] = 1.0;
        for (int k = M; k >= 1; --k) {
            coeffs[static_cast<size_t>(k - 1)] = coeffs[static_cast<size_t>(k)]
                    * static_cast<double>((2 * M - k + 1) * k)
                    / static_cast<double>(2 * (M - k + 1));
        }

        std::vector<double> zRoots(static_cast<size_t>(2 * M), 0.0);
        gsl_poly_complex_workspace *ws =
                gsl_poly_complex_workspace_alloc(static_cast<size_t>(M + 1));
        int status = -1;
        if (ws) {
            status = gsl_poly_complex_solve(coeffs.data(), static_cast<size_t>(M + 1), ws,
                                            zRoots.data());
            gsl_poly_complex_workspace_free(ws);
        }

        if (status == 0) {
            for (int k = 0; k < M; ++k) {
                cdbl root(zRoots[static_cast<size_t>(2 * k)],
                          zRoots[static_cast<size_t>(2 * k + 1)]);
                if (root.real() >= -1e-9)
                    root = cdbl(-std::fabs(root.real()) - 0.1, root.imag());
                poles.push_back(root);
            }
            // Normalize -3 dB cutoff frequency to Omega = 1 rad/s
            auto magRatioSq = [&](double w) -> double {
                cdbl prodNum(1.0, 0.0), prodDen(1.0, 0.0);
                for (const cdbl &p : poles) {
                    prodNum *= (-p);
                    prodDen *= (cdbl(0.0, w) - p);
                }
                return std::norm(prodNum) / std::max(1e-300, std::norm(prodDen));
            };
            double lo = 0.0, hi = 20.0;
            while (magRatioSq(hi) > 0.5 && hi < 1e4)
                hi *= 2.0;
            for (int it = 0; it < 60; ++it) {
                const double mid = 0.5 * (lo + hi);
                if (magRatioSq(mid) > 0.5)
                    lo = mid;
                else
                    hi = mid;
            }
            const double w3dB = std::max(1e-6, 0.5 * (lo + hi));
            for (cdbl &p : poles)
                p /= w3dB;
        } else {
            // Fallback to Butterworth if GSL companion solver failed
            for (int k = 1; k <= M; ++k) {
                const double angle = M_PI * static_cast<double>(2 * k + M - 1)
                        / static_cast<double>(2 * M);
                poles.emplace_back(std::cos(angle), std::sin(angle));
            }
        }
    }
    return poles;
}

void splitConjAndReal(const std::vector<cdbl> &roots, std::vector<cdbl> &upperPairs,
                      std::vector<double> &reals)
{
    upperPairs.clear();
    reals.clear();

    std::vector<bool> used(roots.size(), false);
    const double imagEps = 1e-9;

    for (size_t i = 0; i < roots.size(); ++i) {
        if (used[i])
            continue;
        if (std::fabs(roots[i].imag()) <= imagEps) {
            used[i] = true;
            reals.push_back(roots[i].real());
            continue;
        }
        // Find closest conjugate partner
        size_t bestJ = roots.size();
        double bestDist = std::numeric_limits<double>::max();
        const cdbl target = std::conj(roots[i]);
        for (size_t j = i + 1; j < roots.size(); ++j) {
            if (used[j])
                continue;
            const double d = std::abs(roots[j] - target);
            if (d < bestDist) {
                bestDist = d;
                bestJ = j;
            }
        }
        used[i] = true;
        if (bestJ < roots.size() && bestDist < 1e-4) {
            used[bestJ] = true;
            const cdbl avg(0.5 * (roots[i].real() + roots[bestJ].real()),
                           0.5 * (std::fabs(roots[i].imag()) + std::fabs(roots[bestJ].imag())));
            upperPairs.push_back(avg);
        } else {
            reals.push_back(roots[i].real());
        }
    }

    // Sort conjugate poles by proximity to unit circle ascending (lowest Q first for stability)
    std::sort(upperPairs.begin(), upperPairs.end(),
              [](const cdbl &a, const cdbl &b) { return std::abs(a) < std::abs(b); });
    std::sort(reals.begin(), reals.end(),
              [](double a, double b) { return std::fabs(a) < std::fabs(b); });
}

std::vector<Biquad> designDigitalIirSOS(AdvancedSignalProc::IirDesign design,
                                        AdvancedSignalProc::PassType pass, int order,
                                        double fs, double fLow, double fHigh, double rippleDb)
{
    const int M = std::clamp(order, 1, 12);
    const std::vector<cdbl> protoPoles = computePrototypePoles(design, M, rippleDb);

    const double fMin = 0.0005 * fs;
    const double fMax = 0.4995 * fs;
    double fc1 = std::clamp(fLow, fMin, fMax);
    double fc2 = std::clamp(fHigh, fMin, fMax);
    if (pass == AdvancedSignalProc::BandPass || pass == AdvancedSignalProc::BandStop) {
        if (fc2 <= fc1) {
            if (fc2 > fMin)
                std::swap(fc1, fc2);
            if (fc2 <= fc1)
                fc2 = std::min(fMax, fc1 * 1.5 + 0.01 * fs);
        }
    }

    // Pre-warped analog frequencies with T = 2 (s = 2*(z-1)/(z+1))
    const double W1 = 2.0 * std::tan(M_PI * fc1 / fs);
    const double W2 = 2.0 * std::tan(M_PI * fc2 / fs);
    const double W0 = std::sqrt(W1 * W2);
    const double BW = std::max(1e-9, W2 - W1);

    std::vector<cdbl> sPoles;
    std::vector<cdbl> sZeros;

    switch (pass) {
    case AdvancedSignalProc::LowPass:
        for (const cdbl &p : protoPoles)
            sPoles.push_back(W1 * p);
        break;
    case AdvancedSignalProc::HighPass:
        for (const cdbl &p : protoPoles) {
            sPoles.push_back(W1 / p);
            sZeros.emplace_back(0.0, 0.0);
        }
        break;
    case AdvancedSignalProc::BandPass:
        for (const cdbl &p : protoPoles) {
            const cdbl half = 0.5 * BW * p;
            const cdbl disc = std::sqrt(half * half - cdbl(W0 * W0, 0.0));
            sPoles.push_back(half + disc);
            sPoles.push_back(half - disc);
            sZeros.emplace_back(0.0, 0.0);
        }
        break;
    case AdvancedSignalProc::BandStop:
        for (const cdbl &p : protoPoles) {
            const cdbl half = 0.5 * BW / p;
            const cdbl disc = std::sqrt(half * half - cdbl(W0 * W0, 0.0));
            sPoles.push_back(half + disc);
            sPoles.push_back(half - disc);
            sZeros.emplace_back(0.0, W0);
            sZeros.emplace_back(0.0, -W0);
        }
        break;
    }

    // Bilinear transform z = (2 + s) / (2 - s), padding zeros at s = inf -> z = -1
    std::vector<cdbl> zPoles;
    std::vector<cdbl> zZeros;
    zPoles.reserve(sPoles.size());
    zZeros.reserve(sPoles.size());

    for (const cdbl &sp : sPoles) {
        cdbl zp = (cdbl(2.0, 0.0) + sp) / (cdbl(2.0, 0.0) - sp);
        const double mag = std::abs(zp);
        if (mag >= 0.999999)
            zp *= (0.999999 / mag);
        zPoles.push_back(zp);
    }
    for (const cdbl &sz : sZeros)
        zZeros.push_back((cdbl(2.0, 0.0) + sz) / (cdbl(2.0, 0.0) - sz));
    while (zZeros.size() < zPoles.size())
        zZeros.emplace_back(-1.0, 0.0);

    std::vector<cdbl> polePairs, zeroPairs;
    std::vector<double> poleReals, zeroReals;
    splitConjAndReal(zPoles, polePairs, poleReals);
    splitConjAndReal(zZeros, zeroPairs, zeroReals);

    std::vector<Biquad> sos;
    size_t zpIdx = 0;
    size_t zrIdx = 0;

    for (const cdbl &pp : polePairs) {
        Biquad sec;
        sec.a1 = -2.0 * pp.real();
        sec.a2 = std::norm(pp);
        if (zpIdx < zeroPairs.size()) {
            const cdbl &zq = zeroPairs[zpIdx++];
            sec.b0 = 1.0;
            sec.b1 = -2.0 * zq.real();
            sec.b2 = std::norm(zq);
        } else if (zrIdx + 1 < zeroReals.size()) {
            const double q1 = zeroReals[zrIdx++];
            const double q2 = zeroReals[zrIdx++];
            sec.b0 = 1.0;
            sec.b1 = -(q1 + q2);
            sec.b2 = q1 * q2;
        } else if (zrIdx < zeroReals.size()) {
            const double q1 = zeroReals[zrIdx++];
            sec.b0 = 1.0;
            sec.b1 = -q1;
            sec.b2 = 0.0;
        }
        sos.push_back(sec);
    }

    for (size_t prIdx = 0; prIdx < poleReals.size(); prIdx += 2) {
        Biquad sec;
        if (prIdx + 1 < poleReals.size()) {
            const double p1 = poleReals[prIdx];
            const double p2 = poleReals[prIdx + 1];
            sec.a1 = -(p1 + p2);
            sec.a2 = p1 * p2;
            if (zpIdx < zeroPairs.size()) {
                const cdbl &zq = zeroPairs[zpIdx++];
                sec.b0 = 1.0;
                sec.b1 = -2.0 * zq.real();
                sec.b2 = std::norm(zq);
            } else if (zrIdx + 1 < zeroReals.size()) {
                const double q1 = zeroReals[zrIdx++];
                const double q2 = zeroReals[zrIdx++];
                sec.b0 = 1.0;
                sec.b1 = -(q1 + q2);
                sec.b2 = q1 * q2;
            }
        } else {
            // Single real pole -> 1st order section embedded as biquad
            sec.a1 = -poleReals[prIdx];
            sec.a2 = 0.0;
            if (zrIdx < zeroReals.size()) {
                sec.b0 = 1.0;
                sec.b1 = -zeroReals[zrIdx++];
                sec.b2 = 0.0;
            }
        }
        sos.push_back(sec);
    }

    if (sos.empty())
        return sos;

    // Normalize cascade passband gain at reference frequency
    double wRef = 0.0;
    if (pass == AdvancedSignalProc::HighPass) {
        wRef = M_PI;
    } else if (pass == AdvancedSignalProc::BandPass) {
        wRef = 2.0 * std::atan(0.5 * W0);
    }

    const cdbl zInv1 = std::exp(cdbl(0.0, -wRef));
    const cdbl zInv2 = std::exp(cdbl(0.0, -2.0 * wRef));
    cdbl H(1.0, 0.0);
    for (const Biquad &s : sos) {
        const cdbl num = s.b0 + s.b1 * zInv1 + s.b2 * zInv2;
        const cdbl den = 1.0 + s.a1 * zInv1 + s.a2 * zInv2;
        H *= (num / den);
    }

    double targetGain = 1.0;
    if (design == AdvancedSignalProc::ChebyshevI && (M % 2 == 0)) {
        const double rip = std::max(0.01, rippleDb);
        const double eps = std::sqrt(std::max(1e-8, std::pow(10.0, rip / 10.0) - 1.0));
        targetGain = 1.0 / std::sqrt(1.0 + eps * eps);
    }

    const double magH = std::abs(H);
    if (magH > 1e-30) {
        const double scale = targetGain / magH;
        // Distribute gain across sections to prevent internal overflow
        const double perSec = std::pow(scale, 1.0 / static_cast<double>(sos.size()));
        for (Biquad &s : sos) {
            s.b0 *= perSec;
            s.b1 *= perSec;
            s.b2 *= perSec;
        }
    }

    return sos;
}

void applyCascadeForward(const std::vector<Biquad> &sos, std::vector<double> &sig)
{
    if (sig.empty() || sos.empty())
        return;

    for (const Biquad &s : sos) {
        const double x0 = sig.front();
        const double denDc = 1.0 + s.a1 + s.a2;
        const double gDc = (std::fabs(denDc) > 1e-12) ? ((s.b0 + s.b1 + s.b2) / denDc) : 0.0;
        double z2 = (s.b2 - s.a2 * gDc) * x0;
        double z1 = (s.b1 - s.a1 * gDc) * x0 + z2;

        for (double &sample : sig) {
            const double x = sample;
            const double y = s.b0 * x + z1;
            z1 = s.b1 * x - s.a1 * y + z2;
            z2 = s.b2 * x - s.a2 * y;
            sample = y;
        }
    }
}

// ============================================================================
// Orthogonal Pyramid DWT / IDWT Helpers (Haar & Daubechies-4 db4)
// ============================================================================
const std::vector<double> &waveletLowpassFilter(AdvancedSignalProc::WaveletType wtype)
{
    static const std::vector<double> haarH = {
        0.70710678118654752440,
        0.70710678118654752440
    };
    static const std::vector<double> db4H = {
        -0.010597401785069032,
         0.032883011666885200,
         0.030841381835560764,
        -0.187034811719093090,
        -0.027983769416859854,
         0.630880767929858900,
         0.714846570552915400,
         0.230377813308896500
    };
    return (wtype == AdvancedSignalProc::Haar) ? haarH : db4H;
}

double computeMedian(std::vector<double> vals)
{
    if (vals.empty())
        return 0.0;
    const size_t n = vals.size();
    const size_t mid = n / 2;
    std::nth_element(vals.begin(), vals.begin() + static_cast<ptrdiff_t>(mid), vals.end());
    double med = vals[mid];
    if (n % 2 == 0) {
        const double lowerMax =
                *std::max_element(vals.begin(), vals.begin() + static_cast<ptrdiff_t>(mid));
        med = 0.5 * (lowerMax + med);
    }
    return med;
}

double besselI0(double z)
{
    const double halfZ = 0.5 * z;
    const double t2 = halfZ * halfZ;
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k <= 40; ++k) {
        term *= t2 / (static_cast<double>(k) * static_cast<double>(k));
        sum += term;
        if (term <= 1e-16 * sum)
            break;
    }
    return sum;
}

std::vector<double> computeSplineUpperEnvelope(const std::vector<double> &xs,
                                               const std::vector<double> &ys,
                                               int minPeakDistPts)
{
    const int n = static_cast<int>(xs.size());
    std::vector<double> env(static_cast<size_t>(n), 0.0);
    if (n == 0)
        return env;
    if (n == 1) {
        env[0] = ys[0];
        return env;
    }

    const int minDist = std::max(1, minPeakDistPts);

    // Detect candidate local maxima (including flat-topped crests)
    std::vector<int> candPeaks;
    int idx = 1;
    while (idx < n - 1) {
        if (ys[static_cast<size_t>(idx)] >= ys[static_cast<size_t>(idx - 1)]) {
            int r = idx;
            while (r + 1 < n - 1
                   && std::fabs(ys[static_cast<size_t>(r + 1)] - ys[static_cast<size_t>(idx)])
                           <= 1e-15 * std::max(1.0, std::fabs(ys[static_cast<size_t>(idx)]))) {
                ++r;
            }
            if (r < n - 1 && ys[static_cast<size_t>(r)] >= ys[static_cast<size_t>(r + 1)]
                && (ys[static_cast<size_t>(idx)] > ys[static_cast<size_t>(idx - 1)]
                    || ys[static_cast<size_t>(r)] > ys[static_cast<size_t>(r + 1)])) {
                candPeaks.push_back((idx + r) / 2);
            }
            idx = r + 1;
        } else {
            ++idx;
        }
    }

    // Prominence/height-ordered minimum index separation
    std::sort(candPeaks.begin(), candPeaks.end(), [&](int a, int b) {
        if (ys[static_cast<size_t>(a)] != ys[static_cast<size_t>(b)])
            return ys[static_cast<size_t>(a)] > ys[static_cast<size_t>(b)];
        return a < b;
    });

    std::vector<int> accepted;
    accepted.reserve(candPeaks.size());
    for (int pk : candPeaks) {
        bool tooClose = false;
        for (int kept : accepted) {
            if (std::abs(pk - kept) < minDist) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose)
            accepted.push_back(pk);
    }
    std::sort(accepted.begin(), accepted.end());

    // Build knot vectors including left and right boundary anchors
    std::vector<double> knotX;
    std::vector<double> knotY;
    knotX.reserve(accepted.size() + 2);
    knotY.reserve(accepted.size() + 2);

    int medianStep = n;
    if (accepted.size() >= 2) {
        std::vector<double> steps;
        steps.reserve(accepted.size() - 1);
        for (size_t k = 1; k < accepted.size(); ++k)
            steps.push_back(static_cast<double>(accepted[k] - accepted[k - 1]));
        medianStep = std::max(1, static_cast<int>(std::round(computeMedian(steps))));
    }

    // Left boundary anchor at xs.front()
    double yLeftAnchor = ys.front();
    if (accepted.size() >= 2 && accepted.front() > 0 && accepted.front() <= 2 * medianStep) {
        const int i1 = accepted[0];
        const int i2 = accepted[1];
        const double dx = xs[static_cast<size_t>(i2)] - xs[static_cast<size_t>(i1)];
        const double y1 = ys[static_cast<size_t>(i1)];
        const double y2 = ys[static_cast<size_t>(i2)];
        double extrap = y1;
        if (dx > 1e-300)
            extrap = y1 + ((y2 - y1) / dx) * (xs.front() - xs[static_cast<size_t>(i1)]);
        double maxPrefix = ys.front();
        for (int k = 0; k <= i1; ++k)
            maxPrefix = std::max(maxPrefix, ys[static_cast<size_t>(k)]);
        const double maxJump = std::fabs(y2 - y1);
        extrap = std::clamp(extrap, std::min(y1, y2) - maxJump, std::max(y1, y2) + maxJump);
        yLeftAnchor = std::max(ys.front(), std::min(maxPrefix + maxJump, extrap));
        for (int k = 0; k < i1; ++k)
            yLeftAnchor = std::max(yLeftAnchor, ys[static_cast<size_t>(k)]);
    }
    knotX.push_back(xs.front());
    knotY.push_back(yLeftAnchor);

    for (int pk : accepted) {
        if (xs[static_cast<size_t>(pk)] > knotX.back()) {
            knotX.push_back(xs[static_cast<size_t>(pk)]);
            knotY.push_back(ys[static_cast<size_t>(pk)]);
        } else {
            knotY.back() = std::max(knotY.back(), ys[static_cast<size_t>(pk)]);
        }
    }

    // Right boundary anchor at xs.back()
    double yRightAnchor = ys.back();
    if (accepted.size() >= 2 && accepted.back() < n - 1
        && (n - 1 - accepted.back()) <= 2 * medianStep) {
        const int iLast = accepted.back();
        const int iPrev = accepted[accepted.size() - 2];
        const double dx = xs[static_cast<size_t>(iLast)] - xs[static_cast<size_t>(iPrev)];
        const double yPrev = ys[static_cast<size_t>(iPrev)];
        const double yLast = ys[static_cast<size_t>(iLast)];
        double extrap = yLast;
        if (dx > 1e-300)
            extrap = yLast + ((yLast - yPrev) / dx) * (xs.back() - xs[static_cast<size_t>(iLast)]);
        double maxSuffix = ys.back();
        for (int k = iLast; k < n; ++k)
            maxSuffix = std::max(maxSuffix, ys[static_cast<size_t>(k)]);
        const double maxJump = std::fabs(yLast - yPrev);
        extrap = std::clamp(extrap, std::min(yPrev, yLast) - maxJump,
                            std::max(yPrev, yLast) + maxJump);
        yRightAnchor = std::max(ys.back(), std::min(maxSuffix + maxJump, extrap));
        for (int k = iLast + 1; k < n; ++k)
            yRightAnchor = std::max(yRightAnchor, ys[static_cast<size_t>(k)]);
    }
    if (xs.back() > knotX.back()) {
        knotX.push_back(xs.back());
        knotY.push_back(yRightAnchor);
    } else {
        knotY.back() = std::max(knotY.back(), yRightAnchor);
    }

    const size_t K = knotX.size();
    if (K < 2) {
        std::fill(env.begin(), env.end(), knotY.empty() ? ys[0] : knotY[0]);
        return env;
    }

    const gsl_interp_type *interpType = gsl_interp_linear;
    if (K >= 5)
        interpType = gsl_interp_akima;
    else if (K >= 4)
        interpType = gsl_interp_cspline;

    gsl_interp_accel *acc = gsl_interp_accel_alloc();
    gsl_spline *spl = gsl_spline_alloc(interpType, K);
    if (spl && acc && gsl_spline_init(spl, knotX.data(), knotY.data(), K) == 0) {
        for (int i = 0; i < n; ++i) {
            const double xq = std::clamp(xs[static_cast<size_t>(i)], knotX.front(), knotX.back());
            env[static_cast<size_t>(i)] = gsl_spline_eval(spl, xq, acc);
        }
    } else {
        size_t seg = 0;
        for (int i = 0; i < n; ++i) {
            const double xq = xs[static_cast<size_t>(i)];
            while (seg + 1 < K && knotX[seg + 1] < xq)
                ++seg;
            if (seg + 1 >= K) {
                env[static_cast<size_t>(i)] = knotY.back();
            } else {
                const double dx = knotX[seg + 1] - knotX[seg];
                const double a = (dx > 1e-300) ? ((xq - knotX[seg]) / dx) : 0.0;
                env[static_cast<size_t>(i)] = knotY[seg] + a * (knotY[seg + 1] - knotY[seg]);
            }
        }
    }
    if (spl)
        gsl_spline_free(spl);
    if (acc)
        gsl_interp_accel_free(acc);
    return env;
}

bool computeIirFilteredVector(Graph *g, const QString &curveTitle,
                              AdvancedSignalProc::IirDesign design,
                              AdvancedSignalProc::PassType pass, int order,
                              double cutoffLowHz, double cutoffHighHz,
                              bool zeroPhase, double rippleDb,
                              std::vector<double> &tOut, std::vector<double> &filteredOut,
                              double &fsOut)
{
    tOut.clear();
    filteredOut.clear();
    fsOut = 1.0;
    std::vector<double> y;
    if (!extractUniformSignal(g, curveTitle, tOut, y, fsOut) || y.size() < 4)
        return false;

    const std::vector<Biquad> sos =
            designDigitalIirSOS(design, pass, order, fsOut, cutoffLowHz, cutoffHighHz, rippleDb);
    if (sos.empty())
        return false;

    const int n = static_cast<int>(y.size());
    if (zeroPhase && n >= 4) {
        const int padLen = std::min(n - 1, std::max(6, 6 * static_cast<int>(sos.size())));
        std::vector<double> work(static_cast<size_t>(n + 2 * padLen), 0.0);
        for (int k = 0; k < padLen; ++k)
            work[static_cast<size_t>(k)] = 2.0 * y[0] - y[static_cast<size_t>(padLen - k)];
        for (int k = 0; k < n; ++k)
            work[static_cast<size_t>(padLen + k)] = y[static_cast<size_t>(k)];
        for (int k = 0; k < padLen; ++k)
            work[static_cast<size_t>(padLen + n + k)] =
                    2.0 * y[static_cast<size_t>(n - 1)] - y[static_cast<size_t>(n - 2 - k)];

        applyCascadeForward(sos, work);
        std::reverse(work.begin(), work.end());
        applyCascadeForward(sos, work);
        std::reverse(work.begin(), work.end());
        filteredOut.assign(work.begin() + padLen, work.begin() + padLen + n);
    } else {
        filteredOut = y;
        applyCascadeForward(sos, filteredOut);
    }
    return true;
}

bool computeSavGolDerivVector(Graph *g, const QString &curveTitle,
                              int derivOrder, int windowPoints, int polyOrder,
                              std::vector<double> &xsOut, std::vector<double> &derivOut,
                              int *clampedOrderOut = nullptr)
{
    xsOut.clear();
    derivOut.clear();
    std::vector<double> ys;
    if (!extractSortedSignalXY(g, curveTitle, xsOut, ys))
        return false;

    const int n = static_cast<int>(xsOut.size());
    const int d = std::clamp(derivOrder, 1, 4);
    if (clampedOrderOut)
        *clampedOrderOut = d;
    if (n < d + 1)
        return false;

    const int p = std::clamp(std::max(d, polyOrder), d, std::min(8, n - 1));
    int W = std::max({ 3, p + 1, windowPoints });
    if (W % 2 == 0)
        ++W;
    if (W > n) {
        W = (n % 2 == 1) ? n : (n - 1);
        if (W < p + 1)
            W = n;
    }

    double factD = 1.0;
    for (int k = 2; k <= d; ++k)
        factD *= static_cast<double>(k);

    const int nColsV = p + 1;
    gsl_matrix *V = gsl_matrix_alloc(static_cast<size_t>(W), static_cast<size_t>(nColsV));
    gsl_vector *rhs = gsl_vector_alloc(static_cast<size_t>(W));
    gsl_vector *coeffs = gsl_vector_alloc(static_cast<size_t>(nColsV));
    gsl_matrix *cov = gsl_matrix_alloc(static_cast<size_t>(nColsV), static_cast<size_t>(nColsV));
    gsl_multifit_linear_workspace *ws =
            gsl_multifit_linear_alloc(static_cast<size_t>(W), static_cast<size_t>(nColsV));

    if (!V || !rhs || !coeffs || !cov || !ws) {
        if (ws)
            gsl_multifit_linear_free(ws);
        if (cov)
            gsl_matrix_free(cov);
        if (coeffs)
            gsl_vector_free(coeffs);
        if (rhs)
            gsl_vector_free(rhs);
        if (V)
            gsl_matrix_free(V);
        return false;
    }

    derivOut.assign(static_cast<size_t>(n), 0.0);
    const int halfW = W / 2;

    for (int i = 0; i < n; ++i) {
        int start = i - halfW;
        if (start < 0)
            start = 0;
        if (start + W > n)
            start = n - W;

        const double xi = xsOut[static_cast<size_t>(i)];
        double hScale = 0.0;
        for (int m = 0; m < W; ++m)
            hScale = std::max(hScale, std::fabs(xsOut[static_cast<size_t>(start + m)] - xi));
        if (hScale <= 1e-300)
            hScale = 1.0;

        for (int m = 0; m < W; ++m) {
            const double tau = (xsOut[static_cast<size_t>(start + m)] - xi) / hScale;
            double pwr = 1.0;
            for (int k = 0; k <= p; ++k) {
                gsl_matrix_set(V, static_cast<size_t>(m), static_cast<size_t>(k), pwr);
                pwr *= tau;
            }
            gsl_vector_set(rhs, static_cast<size_t>(m), ys[static_cast<size_t>(start + m)]);
        }

        double chisq = 0.0;
        if (gsl_multifit_linear(V, rhs, coeffs, cov, &chisq, ws) == 0) {
            const double cScaled = gsl_vector_get(coeffs, static_cast<size_t>(d));
            const double hPow = std::pow(hScale, d);
            derivOut[static_cast<size_t>(i)] = (hPow > 1e-300) ? (factD * cScaled / hPow) : 0.0;
        }
    }

    gsl_multifit_linear_free(ws);
    gsl_matrix_free(cov);
    gsl_vector_free(coeffs);
    gsl_vector_free(rhs);
    gsl_matrix_free(V);
    return true;
}

} // namespace

// ============================================================================
// Public Static Methods
// ============================================================================

QwtPlotCurve *AdvancedSignalProc::iirFilter(ApplicationWindow *app, Graph *g,
                                            const QString &curveTitle,
                                            IirDesign design, PassType pass, int order,
                                            double cutoffLowHz, double cutoffHighHz,
                                            bool zeroPhase, double rippleDb)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> t, filtered;
    double fs = 1.0;
    if (!computeIirFilteredVector(g, curveTitle, design, pass, order, cutoffLowHz, cutoffHighHz,
                                  zeroPhase, rippleDb, t, filtered, fs))
        return nullptr;
    reportSignalPrep(app, trSig("IIR Digital Filter"), curveTitle, s_lastSignalPrepNote);
    const int n = static_cast<int>(filtered.size());

    Table *tbl = buildSignalTable(app, "IIRFilter",
                                  QStringList() << "Time" << "FilteredY",
                                  QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                                  { t, filtered });
    if (!tbl)
        return nullptr;

    if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, n - 1))
        return nullptr;

    QwtPlotCurve *resCurve = g->curve(g->curves() - 1);
    if (resCurve)
        resCurve->setPen(QPen(QColor(0, 114, 178), 2.0, Qt::SolidLine));
    g->updatePlot();

    if (resCurve) {
        if (RecalcManager *mgr = RecalcManager::instance(app)) {
            if (!mgr->isRecalculating() && mgr->recalcMode() != RecalcManager::Disabled) {
                QVariantMap params;
                params.insert(QStringLiteral("design"), static_cast<int>(design));
                params.insert(QStringLiteral("pass"), static_cast<int>(pass));
                params.insert(QStringLiteral("order"), order);
                params.insert(QStringLiteral("cutoffLowHz"), cutoffLowHz);
                params.insert(QStringLiteral("cutoffHighHz"), cutoffHighHz);
                params.insert(QStringLiteral("zeroPhase"), zeroPhase);
                params.insert(QStringLiteral("rippleDb"), rippleDb);
                mgr->registerLink(app, g, nullptr, curveTitle, resCurve->title().text(),
                                  RecalcManager::OpIIRFilter, params, nullptr);
            }
        }
    }

    app->updateLog(QString("[%1] IIR Digital Filter (order=%2, zeroPhase=%3, fs=%4 Hz) -> %5\n")
                           .arg(QLocale().toString(QDateTime::currentDateTime()))
                           .arg(order)
                           .arg(zeroPhase ? "true" : "false")
                           .arg(fs, 0, 'g', 6)
                           .arg(tbl->name()));
    return resCurve;
}

Matrix *AdvancedSignalProc::computeSTFT(ApplicationWindow *app, Graph *g,
                                        const QString &curveTitle,
                                        int windowSize, int hopSize, bool plotSpectrogram)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 4)
        return nullptr;
    reportSignalPrep(app, QStringLiteral("STFT"), curveTitle, s_lastSignalPrepNote);

    const int n = static_cast<int>(y.size());
    const int W = std::max(8, windowSize);
    const int H = std::max(1, hopSize);

    // Hann window
    std::vector<double> win(static_cast<size_t>(W), 1.0);
    double winSum = 0.0;
    for (int m = 0; m < W; ++m) {
        win[static_cast<size_t>(m)] =
                0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(m) / static_cast<double>(W - 1)));
        winSum += win[static_cast<size_t>(m)];
    }
    const double normFactor = (winSum > 1e-12) ? (2.0 / winSum) : (1.0 / static_cast<double>(W));

    int nFrames = 1;
    if (n > W)
        nFrames = (n - W) / H + 1;
    const int nFreqs = W / 2 + 1;

    gsl_fft_real_wavetable *realWt = gsl_fft_real_wavetable_alloc(static_cast<size_t>(W));
    gsl_fft_real_workspace *realWs = gsl_fft_real_workspace_alloc(static_cast<size_t>(W));
    if (!realWt || !realWs) {
        if (realWt)
            gsl_fft_real_wavetable_free(realWt);
        if (realWs)
            gsl_fft_real_workspace_free(realWs);
        return nullptr;
    }

    Matrix *mat = app->newMatrix(uniqueWinName(app, "STFT"), nFreqs, nFrames);
    if (!mat) {
        gsl_fft_real_wavetable_free(realWt);
        gsl_fft_real_workspace_free(realWs);
        return nullptr;
    }

    const double dt = 1.0 / fs;
    const double tStart = t.front() + 0.5 * std::min(n - 1, W - 1) * dt;
    const double tEnd = (nFrames > 1) ? (tStart + static_cast<double>(nFrames - 1) * H * dt)
                                      : t.back();
    mat->setCoordinates(tStart, std::max(tStart + dt, tEnd), 0.0, 0.5 * fs);

    std::vector<double> frameBuf(static_cast<size_t>(W), 0.0);
    std::vector<double> unpacked(static_cast<size_t>(2 * W), 0.0);

    mat->d_future_matrix->beginMacro(trSig("Compute STFT"));
    for (int f = 0; f < nFrames; ++f) {
        const int startIdx = f * H;
        for (int m = 0; m < W; ++m) {
            const int srcIdx = startIdx + m;
            const double sample = (srcIdx < n) ? y[static_cast<size_t>(srcIdx)] : 0.0;
            frameBuf[static_cast<size_t>(m)] = sample * win[static_cast<size_t>(m)];
        }

        gsl_fft_real_transform(frameBuf.data(), 1, static_cast<size_t>(W), realWt, realWs);
        gsl_fft_halfcomplex_unpack(frameBuf.data(), unpacked.data(), 1, static_cast<size_t>(W));

        for (int k = 0; k < nFreqs; ++k) {
            const double re = unpacked[static_cast<size_t>(2 * k)];
            const double im = unpacked[static_cast<size_t>(2 * k + 1)];
            double mag = std::hypot(re, im) * normFactor;
            if (k == 0 || (W % 2 == 0 && k == W / 2))
                mag *= 0.5;
            mat->setCell(k, f, mag);
        }
    }
    mat->d_future_matrix->endMacro();

    gsl_fft_real_wavetable_free(realWt);
    gsl_fft_real_workspace_free(realWs);

    if (plotSpectrogram) {
        MultiLayer *ml = app->plotSpectrogram(mat, Graph::ColorMap);
        if (ml && ml->activeGraph()) {
            ml->activeGraph()->setXAxisTitle(trSig("Time"));
            ml->activeGraph()->setYAxisTitle(trSig("Frequency (Hz)"));
            ml->activeGraph()->replot();
        }
    }

    return mat;
}

QwtPlotCurve *AdvancedSignalProc::hilbertEnvelope(ApplicationWindow *app, Graph *g,
                                                  const QString &curveTitle,
                                                  bool plotLowerEnvelope, Table **outTable)
{
    if (outTable)
        *outTable = nullptr;
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 2)
        return nullptr;
    reportSignalPrep(app, QStringLiteral("Hilbert"), curveTitle, s_lastSignalPrepNote);

    const size_t n = y.size();
    std::vector<double> z(2 * n, 0.0);
    for (size_t i = 0; i < n; ++i)
        z[2 * i] = y[i];

    // Forward FFT -> X[k]
    runComplexFft1D(z, n, false);

    // Exact Marple (1999) discrete analytic spectrum:
    // Z[0] = X[0]; Z[k] = 2*X[k] for 0 < k < (N+1)/2; Z[N/2] = X[N/2] (if N even); Z[k] = 0 for k > N/2
    const size_t posLimit = (n + 1) / 2;
    for (size_t k = 1; k < posLimit; ++k) {
        z[2 * k] *= 2.0;
        z[2 * k + 1] *= 2.0;
    }
    const size_t zeroStart = (n / 2) + 1;
    for (size_t k = zeroStart; k < n; ++k) {
        z[2 * k] = 0.0;
        z[2 * k + 1] = 0.0;
    }

    // Inverse FFT -> z(t) = y(t) + j * H{y}(t)
    runComplexFft1D(z, n, true);

    std::vector<double> upperEnv(n), lowerEnv(n), imagPart(n), instPhase(n), instFreq(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        const double re = z[2 * i];
        const double im = z[2 * i + 1];
        const double env = std::hypot(re, im);
        upperEnv[i] = env;
        lowerEnv[i] = -env;
        imagPart[i] = im;
        instPhase[i] = std::atan2(im, re);
    }

    // Unwrap phase
    for (size_t i = 1; i < n; ++i) {
        double diff = instPhase[i] - instPhase[i - 1];
        while (diff > M_PI) {
            instPhase[i] -= 2.0 * M_PI;
            diff -= 2.0 * M_PI;
        }
        while (diff < -M_PI) {
            instPhase[i] += 2.0 * M_PI;
            diff += 2.0 * M_PI;
        }
    }

    // Instantaneous frequency f(t) = (1 / 2pi) * d(phi)/dt
    const double dt = 1.0 / fs;
    if (n >= 2) {
        instFreq[0] = (instPhase[1] - instPhase[0]) / (2.0 * M_PI * dt);
        for (size_t i = 1; i + 1 < n; ++i)
            instFreq[i] = (instPhase[i + 1] - instPhase[i - 1]) / (4.0 * M_PI * dt);
        instFreq[n - 1] = (instPhase[n - 1] - instPhase[n - 2]) / (2.0 * M_PI * dt);
    }

    QStringList cols;
    cols << "Time" << "UpperEnvelope" << "LowerEnvelope" << "HilbertImag" << "InstPhaseRad"
         << "InstFreqHz";
    QList<SciDAVis::PlotDesignation> pds;
    pds << SciDAVis::X << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y << SciDAVis::Y;

    Table *tbl = buildSignalTable(app, "HilbertEnv", cols, pds,
                                  { t, upperEnv, lowerEnv, imagPart, instPhase, instFreq });
    if (!tbl)
        return nullptr;

    if (outTable)
        *outTable = tbl;

    const int nRows = static_cast<int>(n);
    if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, nRows - 1))
        return nullptr;
    QwtPlotCurve *upperCurve = g->curve(g->curves() - 1);
    if (upperCurve)
        upperCurve->setPen(QPen(QColor(213, 94, 0), 2.0, Qt::SolidLine));

    if (plotLowerEnvelope) {
        if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(2), Graph::Line, 0, nRows - 1)) {
            if (QwtPlotCurve *lowCurve = g->curve(g->curves() - 1))
                lowCurve->setPen(QPen(QColor(213, 94, 0, 200), 1.6, Qt::DashLine));
        }
    }
    g->updatePlot();
    return upperCurve;
}

Matrix *AdvancedSignalProc::waveletCWT(ApplicationWindow *app, Graph *g,
                                       const QString &curveTitle,
                                       int numScales, WaveletType wtype, bool plotScalogram)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 4)
        return nullptr;
    reportSignalPrep(app, QStringLiteral("CWT"), curveTitle, s_lastSignalPrepNote);

    const size_t n = y.size();
    const int nScales = std::clamp(numScales, 2, 512);

    // Zero-mean detrend
    const double meanY = std::accumulate(y.begin(), y.end(), 0.0) / static_cast<double>(n);

    // Zero-pad to power-of-2 >= 2*N for wrap-free FFT wavelet convolution
    size_t nFft = 1;
    while (nFft < 2 * n)
        nFft <<= 1;

    std::vector<double> sigFft(2 * nFft, 0.0);
    for (size_t i = 0; i < n; ++i)
        sigFft[2 * i] = y[i] - meanY;

    gsl_fft_complex_wavetable *wt = gsl_fft_complex_wavetable_alloc(nFft);
    gsl_fft_complex_workspace *ws = gsl_fft_complex_workspace_alloc(nFft);
    if (!wt || !ws) {
        if (wt)
            gsl_fft_complex_wavetable_free(wt);
        if (ws)
            gsl_fft_complex_workspace_free(ws);
        return nullptr;
    }

    gsl_fft_complex_forward(sigFft.data(), 1, nFft, wt, ws);

    Matrix *mat = app->newMatrix(uniqueWinName(app, "WaveletCWT"), nScales, static_cast<int>(n));
    if (!mat) {
        gsl_fft_complex_wavetable_free(wt);
        gsl_fft_complex_workspace_free(ws);
        return nullptr;
    }

    const double sMin = 1.0;
    const double sMax = std::max(4.0, 0.30 * static_cast<double>(n));
    mat->setCoordinates(t.front(), t.back(), sMin, sMax);

    std::vector<double> omega(nFft, 0.0);
    for (size_t m = 0; m < nFft; ++m) {
        if (m <= nFft / 2)
            omega[m] = 2.0 * M_PI * static_cast<double>(m) / static_cast<double>(nFft);
        else
            omega[m] = -2.0 * M_PI * static_cast<double>(nFft - m) / static_cast<double>(nFft);
    }

    const double piPowNegQuarter = std::pow(M_PI, -0.25);
    const double omega0 = 6.0;
    std::vector<double> prodBuf(2 * nFft, 0.0);

    mat->d_future_matrix->beginMacro(trSig("Compute Wavelet CWT"));
    for (int r = 0; r < nScales; ++r) {
        const double frac = static_cast<double>(r) / static_cast<double>(std::max(1, nScales - 1));
        const double scale = sMin * std::pow(sMax / sMin, frac);

        for (size_t m = 0; m < nFft; ++m) {
            const double sw = scale * omega[m];
            cdbl psiHatConj(0.0, 0.0);

            if (wtype == Morlet) {
                if (omega[m] > 0.0) {
                    const double diff = sw - omega0;
                    const double gVal = std::sqrt(2.0 * M_PI * scale) * piPowNegQuarter
                            * std::exp(-0.5 * diff * diff);
                    psiHatConj = cdbl(gVal, 0.0);
                }
            } else if (wtype == RickerMexicanHat) {
                const double gVal = std::sqrt(2.0 * M_PI * scale) * (2.0 / std::sqrt(3.0))
                        * piPowNegQuarter * (sw * sw) * std::exp(-0.5 * sw * sw);
                psiHatConj = cdbl(gVal, 0.0);
            } else {
                // Continuous Haar wavelet in Fourier domain
                if (std::fabs(sw) > 1e-12) {
                    const double q = 0.25 * sw;
                    const double val = std::sqrt(scale) * (std::sin(q) * std::sin(q) / q);
                    psiHatConj = cdbl(0.0, val);
                }
            }

            const cdbl Ym(sigFft[2 * m], sigFft[2 * m + 1]);
            const cdbl Wm = Ym * psiHatConj;
            prodBuf[2 * m] = Wm.real();
            prodBuf[2 * m + 1] = Wm.imag();
        }

        gsl_fft_complex_inverse(prodBuf.data(), 1, nFft, wt, ws);

        for (size_t c = 0; c < n; ++c) {
            const double mag = std::hypot(prodBuf[2 * c], prodBuf[2 * c + 1]);
            mat->setCell(r, static_cast<int>(c), mag);
        }
    }
    mat->d_future_matrix->endMacro();

    gsl_fft_complex_wavetable_free(wt);
    gsl_fft_complex_workspace_free(ws);

    if (plotScalogram) {
        MultiLayer *ml = app->plotSpectrogram(mat, Graph::ColorMap);
        if (ml && ml->activeGraph()) {
            ml->activeGraph()->setXAxisTitle(trSig("Time"));
            ml->activeGraph()->setYAxisTitle(trSig("Wavelet Scale"));
            ml->activeGraph()->replot();
        }
    }

    return mat;
}

QwtPlotCurve *AdvancedSignalProc::waveletDenoiseDWT(ApplicationWindow *app, Graph *g,
                                                    const QString &curveTitle,
                                                    WaveletType wtype, int levels,
                                                    bool softThreshold, double thresholdScale)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 4)
        return nullptr;
    reportSignalPrep(app, QStringLiteral("DWT"), curveTitle, s_lastSignalPrepNote);

    const int n = static_cast<int>(y.size());
    const std::vector<double> &h = waveletLowpassFilter(wtype);
    const int K = static_cast<int>(h.size());
    std::vector<double> gHigh(static_cast<size_t>(K), 0.0);
    for (int k = 0; k < K; ++k) {
        const double sign = (k % 2 == 0) ? 1.0 : -1.0;
        gHigh[static_cast<size_t>(k)] = sign * h[static_cast<size_t>(K - 1 - k)];
    }

    // Symmetric boundary extension + pad to multiple of 2^L
    const int L = std::clamp(levels, 1, 10);
    const int padLeft = std::min(n - 1, std::max(K, 16));
    const int block = 1 << L;
    const int minPadded = std::max(block * std::max(1, K), n + 2 * padLeft);
    const int P = ((minPadded + block - 1) / block) * block;

    std::vector<double> approx(static_cast<size_t>(P), 0.0);
    auto mirrorSample = [&](int idx) -> double {
        if (n == 1)
            return y[0];
        const int period = 2 * (n - 1);
        int m = idx % period;
        if (m < 0)
            m += period;
        if (m >= n)
            m = period - m;
        return y[static_cast<size_t>(m)];
    };

    for (int i = 0; i < P; ++i)
        approx[static_cast<size_t>(i)] = mirrorSample(i - padLeft);

    // Forward pyramid DWT
    std::vector<std::vector<double>> details(static_cast<size_t>(L));
    int actualLevels = 0;
    for (int lev = 0; lev < L; ++lev) {
        const int M = static_cast<int>(approx.size());
        if (M < 2 || M < K || (M % 2 != 0))
            break;
        const int half = M / 2;
        std::vector<double> nextApprox(static_cast<size_t>(half), 0.0);
        std::vector<double> det(static_cast<size_t>(half), 0.0);

        for (int m = 0; m < half; ++m) {
            double sa = 0.0, sd = 0.0;
            for (int k = 0; k < K; ++k) {
                const double val = approx[static_cast<size_t>((2 * m + k) % M)];
                sa += h[static_cast<size_t>(k)] * val;
                sd += gHigh[static_cast<size_t>(k)] * val;
            }
            nextApprox[static_cast<size_t>(m)] = sa;
            det[static_cast<size_t>(m)] = sd;
        }
        approx.swap(nextApprox);
        details[static_cast<size_t>(lev)] = std::move(det);
        ++actualLevels;
    }

    if (actualLevels > 0 && !details[0].empty()) {
        // Estimate noise sigma via MAD on finest-level central detail coefficients
        const int d1Start = std::clamp(padLeft / 2, 0, static_cast<int>(details[0].size()) - 1);
        const int d1Count = std::clamp(n / 2, 1, static_cast<int>(details[0].size()) - d1Start);
        std::vector<double> coreD1(details[0].begin() + d1Start,
                                   details[0].begin() + d1Start + d1Count);
        const double medD1 = computeMedian(coreD1);
        for (double &v : coreD1)
            v = std::fabs(v - medD1);
        const double mad = computeMedian(coreD1);
        const double sigmaHat = mad / 0.6744897501960817;
        const double scaleMult = std::max(0.0, thresholdScale);
        const double lambda = scaleMult * sigmaHat
                * std::sqrt(2.0 * std::log(static_cast<double>(std::max(2, n))));

        for (int lev = 0; lev < actualLevels; ++lev) {
            for (double &d : details[static_cast<size_t>(lev)]) {
                const double ad = std::fabs(d);
                if (softThreshold) {
                    d = (ad > lambda) ? std::copysign(ad - lambda, d) : 0.0;
                } else {
                    if (ad < lambda)
                        d = 0.0;
                }
            }
        }
    }

    // Exact orthogonal transpose inverse pyramid IDWT
    for (int lev = actualLevels - 1; lev >= 0; --lev) {
        const std::vector<double> &det = details[static_cast<size_t>(lev)];
        const int half = static_cast<int>(approx.size());
        const int M = 2 * half;
        std::vector<double> recon(static_cast<size_t>(M), 0.0);

        for (int m = 0; m < half; ++m) {
            const double ca = approx[static_cast<size_t>(m)];
            const double cd = det[static_cast<size_t>(m)];
            for (int k = 0; k < K; ++k) {
                const size_t dst = static_cast<size_t>((2 * m + k) % M);
                recon[dst] += h[static_cast<size_t>(k)] * ca + gHigh[static_cast<size_t>(k)] * cd;
            }
        }
        approx.swap(recon);
    }

    std::vector<double> denoised(approx.begin() + padLeft, approx.begin() + padLeft + n);

    Table *tbl = buildSignalTable(app, "WaveletDenoise",
                                  QStringList() << "Time" << "DenoisedY",
                                  QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                                  { t, denoised });
    if (!tbl)
        return nullptr;

    if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, n - 1))
        return nullptr;

    QwtPlotCurve *resCurve = g->curve(g->curves() - 1);
    if (resCurve)
        resCurve->setPen(QPen(QColor(0, 158, 115), 2.0, Qt::SolidLine));
    g->updatePlot();

    if (resCurve) {
        if (RecalcManager *mgr = RecalcManager::instance(app)) {
            if (!mgr->isRecalculating() && mgr->recalcMode() != RecalcManager::Disabled) {
                QVariantMap params;
                params.insert(QStringLiteral("wavelet"), static_cast<int>(wtype));
                params.insert(QStringLiteral("levels"), levels);
                params.insert(QStringLiteral("softThreshold"), softThreshold);
                params.insert(QStringLiteral("thresholdScale"), thresholdScale);
                mgr->registerLink(app, g, nullptr, curveTitle, resCurve->title().text(),
                                  RecalcManager::OpWaveletDenoise, params, nullptr);
            }
        }
    }
    return resCurve;
}

Matrix *AdvancedSignalProc::fft2D(ApplicationWindow *app, Matrix *srcMatrix,
                                  bool inverse, bool shiftCenter)
{
    if (!app || !srcMatrix)
        return nullptr;

    const int rows = srcMatrix->numRows();
    const int cols = srcMatrix->numCols();
    if (rows <= 0 || cols <= 0)
        return nullptr;

    const size_t R = static_cast<size_t>(rows);
    const size_t C = static_cast<size_t>(cols);

    std::vector<double> grid(2 * R * C, 0.0);
    for (size_t r = 0; r < R; ++r) {
        for (size_t c = 0; c < C; ++c) {
            const double val = srcMatrix->cell(static_cast<int>(r), static_cast<int>(c));
            grid[2 * (r * C + c)] = std::isfinite(val) ? val : 0.0;
        }
    }

    // If inverse and input was centered, undo shift prior to 2D IFFT
    if (inverse && shiftCenter) {
        std::vector<double> unshifted(2 * R * C, 0.0);
        const size_t sr = (R + 1) / 2;
        const size_t sc = (C + 1) / 2;
        for (size_t r = 0; r < R; ++r) {
            for (size_t c = 0; c < C; ++c) {
                const size_t srcIdx = 2 * (((r + sr) % R) * C + ((c + sc) % C));
                const size_t dstIdx = 2 * (r * C + c);
                unshifted[dstIdx] = grid[srcIdx];
                unshifted[dstIdx + 1] = grid[srcIdx + 1];
            }
        }
        grid.swap(unshifted);
    }

    // 1D complex FFT along each row
    if (C > 1) {
        gsl_fft_complex_wavetable *wtC = gsl_fft_complex_wavetable_alloc(C);
        gsl_fft_complex_workspace *wsC = gsl_fft_complex_workspace_alloc(C);
        if (wtC && wsC) {
            for (size_t r = 0; r < R; ++r) {
                double *rowPtr = grid.data() + 2 * r * C;
                if (inverse)
                    gsl_fft_complex_inverse(rowPtr, 1, C, wtC, wsC);
                else
                    gsl_fft_complex_forward(rowPtr, 1, C, wtC, wsC);
            }
        }
        if (wtC)
            gsl_fft_complex_wavetable_free(wtC);
        if (wsC)
            gsl_fft_complex_workspace_free(wsC);
    }

    // 1D complex FFT along each column (stride = C complex elements)
    if (R > 1) {
        gsl_fft_complex_wavetable *wtR = gsl_fft_complex_wavetable_alloc(R);
        gsl_fft_complex_workspace *wsR = gsl_fft_complex_workspace_alloc(R);
        if (wtR && wsR) {
            for (size_t c = 0; c < C; ++c) {
                double *colPtr = grid.data() + 2 * c;
                if (inverse)
                    gsl_fft_complex_inverse(colPtr, C, R, wtR, wsR);
                else
                    gsl_fft_complex_forward(colPtr, C, R, wtR, wsR);
            }
        }
        if (wtR)
            gsl_fft_complex_wavetable_free(wtR);
        if (wsR)
            gsl_fft_complex_workspace_free(wsR);
    }

    // Quadrant fftshift on forward FFT when requested
    if (!inverse && shiftCenter) {
        std::vector<double> shifted(2 * R * C, 0.0);
        const size_t sr = R / 2;
        const size_t sc = C / 2;
        for (size_t r = 0; r < R; ++r) {
            for (size_t c = 0; c < C; ++c) {
                const size_t dstR = (r + sr) % R;
                const size_t dstC = (c + sc) % C;
                const size_t srcIdx = 2 * (r * C + c);
                const size_t dstIdx = 2 * (dstR * C + dstC);
                shifted[dstIdx] = grid[srcIdx];
                shifted[dstIdx + 1] = grid[srcIdx + 1];
            }
        }
        grid.swap(shifted);
    }

    Matrix *outMat = app->newMatrix(uniqueWinName(app, inverse ? "IFFT2D" : "FFT2D"), rows, cols);
    if (!outMat)
        return nullptr;

    const double dx = (cols > 1) ? std::fabs(srcMatrix->xEnd() - srcMatrix->xStart())
                    / static_cast<double>(cols - 1)
                                 : 1.0;
    const double dy = (rows > 1) ? std::fabs(srcMatrix->yEnd() - srcMatrix->yStart())
                    / static_cast<double>(rows - 1)
                                 : 1.0;
    const double fxMax = (dx > 1e-15) ? (0.5 / dx) : 0.5;
    const double fyMax = (dy > 1e-15) ? (0.5 / dy) : 0.5;
    if (!inverse) {
        if (shiftCenter)
            outMat->setCoordinates(-fxMax, fxMax, -fyMax, fyMax);
        else
            outMat->setCoordinates(0.0, 2.0 * fxMax, 0.0, 2.0 * fyMax);
    } else {
        outMat->setCoordinates(srcMatrix->xStart(), srcMatrix->xEnd(),
                               srcMatrix->yStart(), srcMatrix->yEnd());
    }

    outMat->d_future_matrix->beginMacro(inverse ? trSig("2D IFFT") : trSig("2D FFT"));
    for (size_t r = 0; r < R; ++r) {
        for (size_t c = 0; c < C; ++c) {
            const size_t idx = 2 * (r * C + c);
            const double val = std::hypot(grid[idx], grid[idx + 1]);
            outMat->setCell(static_cast<int>(r), static_cast<int>(c), val);
        }
    }
    outMat->d_future_matrix->endMacro();

    return outMat;
}

Table *AdvancedSignalProc::welchPSD(ApplicationWindow *app, Graph *g,
                                    const QString &curveTitle,
                                    int segmentLen, bool plotResult)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> t, y;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, t, y, fs) || y.size() < 4)
        return nullptr;
    reportSignalPrep(app, QStringLiteral("Welch PSD"), curveTitle, s_lastSignalPrepNote);

    const int n = static_cast<int>(y.size());
    const int L = std::clamp(segmentLen, 4, n);
    const int H = std::max(1, L / 2);

    std::vector<double> win(static_cast<size_t>(L), 1.0);
    double winEnergy = 0.0;
    for (int m = 0; m < L; ++m) {
        win[static_cast<size_t>(m)] = (L > 1)
                ? (0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(m)
                                         / static_cast<double>(L - 1))))
                : 1.0;
        winEnergy += win[static_cast<size_t>(m)] * win[static_cast<size_t>(m)];
    }
    if (winEnergy <= 1e-30)
        winEnergy = static_cast<double>(L);

    const int nBins = L / 2 + 1;
    std::vector<double> psdAccum(static_cast<size_t>(nBins), 0.0);

    gsl_fft_real_wavetable *wt = gsl_fft_real_wavetable_alloc(static_cast<size_t>(L));
    gsl_fft_real_workspace *ws = gsl_fft_real_workspace_alloc(static_cast<size_t>(L));
    if (!wt || !ws) {
        if (wt)
            gsl_fft_real_wavetable_free(wt);
        if (ws)
            gsl_fft_real_workspace_free(ws);
        return nullptr;
    }

    std::vector<double> segBuf(static_cast<size_t>(L), 0.0);
    std::vector<double> unpacked(static_cast<size_t>(2 * L), 0.0);
    int nSegs = 0;

    for (int start = 0; start + L <= n; start += H) {
        double segMean = 0.0;
        for (int m = 0; m < L; ++m)
            segMean += y[static_cast<size_t>(start + m)];
        segMean /= static_cast<double>(L);

        for (int m = 0; m < L; ++m) {
            segBuf[static_cast<size_t>(m)] =
                    (y[static_cast<size_t>(start + m)] - segMean) * win[static_cast<size_t>(m)];
        }

        gsl_fft_real_transform(segBuf.data(), 1, static_cast<size_t>(L), wt, ws);
        gsl_fft_halfcomplex_unpack(segBuf.data(), unpacked.data(), 1, static_cast<size_t>(L));

        for (int k = 0; k < nBins; ++k) {
            const double re = unpacked[static_cast<size_t>(2 * k)];
            const double im = unpacked[static_cast<size_t>(2 * k + 1)];
            double p = (re * re + im * im) / (fs * winEnergy);
            if (k > 0 && !(L % 2 == 0 && k == L / 2))
                p *= 2.0;
            psdAccum[static_cast<size_t>(k)] += p;
        }
        ++nSegs;
    }

    gsl_fft_real_wavetable_free(wt);
    gsl_fft_real_workspace_free(ws);

    if (nSegs == 0)
        return nullptr;

    std::vector<double> freqs(static_cast<size_t>(nBins));
    std::vector<double> psd(static_cast<size_t>(nBins));
    std::vector<double> psdDb(static_cast<size_t>(nBins));

    for (int k = 0; k < nBins; ++k) {
        freqs[static_cast<size_t>(k)] = static_cast<double>(k) * fs / static_cast<double>(L);
        const double val = psdAccum[static_cast<size_t>(k)] / static_cast<double>(nSegs);
        psd[static_cast<size_t>(k)] = val;
        psdDb[static_cast<size_t>(k)] = 10.0 * std::log10(std::max(val, 1e-300));
    }

    Table *tbl = buildSignalTable(
            app, "WelchPSD", QStringList() << "FrequencyHz" << "PSD" << "PSDdB",
            QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y << SciDAVis::Y,
            { freqs, psd, psdDb });
    if (!tbl)
        return nullptr;

    if (plotResult) {
        MultiLayer *ml = app->multilayerPlot(tbl, QStringList() << tbl->colName(1), Graph::Line, 0,
                                             nBins - 1);
        if (ml && ml->activeGraph()) {
            ml->activeGraph()->setXAxisTitle(trSig("Frequency (Hz)"));
            ml->activeGraph()->setYAxisTitle(trSig("Power Spectral Density"));
            ml->activeGraph()->replot();
        }
    }

    return tbl;
}

Table *AdvancedSignalProc::crossCoherence(ApplicationWindow *app, Graph *g,
                                          const QString &curveA, const QString &curveB,
                                          int segmentLen, bool plotResult)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> tA, yA, tB, yB;
    double fsA = 1.0, fsB = 1.0;
    if (!extractUniformSignal(g, curveA, tA, yA, fsA))
        return nullptr;
    const QString prepNoteA = s_lastSignalPrepNote;
    if (!extractUniformSignal(g, curveB, tB, yB, fsB))
        return nullptr;
    reportSignalPrep(app, QStringLiteral("Coherence"), curveA, prepNoteA);
    reportSignalPrep(app, QStringLiteral("Coherence"), curveB, s_lastSignalPrepNote);

    const double tLo = std::max(tA.front(), tB.front());
    const double tHi = std::min(tA.back(), tB.back());
    if (!(tHi > tLo))
        return nullptr;

    const int nGrid = std::max(static_cast<int>(tA.size()), static_cast<int>(tB.size()));
    if (nGrid < 8)
        return nullptr;

    const double dt = (tHi - tLo) / static_cast<double>(nGrid - 1);
    const double fs = 1.0 / dt;

    auto resampleAt = [](const std::vector<double> &ts, const std::vector<double> &ys,
                         double tq) -> double {
        if (tq <= ts.front())
            return ys.front();
        if (tq >= ts.back())
            return ys.back();
        auto it = std::lower_bound(ts.begin(), ts.end(), tq);
        if (it == ts.begin())
            return ys.front();
        const size_t hi = static_cast<size_t>(it - ts.begin());
        const size_t lo = hi - 1;
        const double d = ts[hi] - ts[lo];
        return (d > 1e-300) ? (ys[lo] + ((tq - ts[lo]) / d) * (ys[hi] - ys[lo])) : ys[lo];
    };

    std::vector<double> sigA(static_cast<size_t>(nGrid)), sigB(static_cast<size_t>(nGrid));
    for (int i = 0; i < nGrid; ++i) {
        const double tq = (i == nGrid - 1) ? tHi : (tLo + static_cast<double>(i) * dt);
        sigA[static_cast<size_t>(i)] = resampleAt(tA, yA, tq);
        sigB[static_cast<size_t>(i)] = resampleAt(tB, yB, tq);
    }

    // Ensure at least 2 overlapped segments when nGrid >= 16 so coherence is non-degenerate
    int maxSeg = (nGrid >= 16) ? std::max(8, (2 * nGrid) / 3) : nGrid;
    const int L = std::clamp(segmentLen, 4, maxSeg);
    const int H = std::max(1, L / 2);

    std::vector<double> win(static_cast<size_t>(L), 1.0);
    for (int m = 0; m < L; ++m) {
        win[static_cast<size_t>(m)] = (L > 1)
                ? (0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(m)
                                         / static_cast<double>(L - 1))))
                : 1.0;
    }

    const int nBins = L / 2 + 1;
    std::vector<double> pxx(static_cast<size_t>(nBins), 0.0);
    std::vector<double> pyy(static_cast<size_t>(nBins), 0.0);
    std::vector<cdbl> pxy(static_cast<size_t>(nBins), cdbl(0.0, 0.0));

    gsl_fft_real_wavetable *wt = gsl_fft_real_wavetable_alloc(static_cast<size_t>(L));
    gsl_fft_real_workspace *ws = gsl_fft_real_workspace_alloc(static_cast<size_t>(L));
    if (!wt || !ws) {
        if (wt)
            gsl_fft_real_wavetable_free(wt);
        if (ws)
            gsl_fft_real_workspace_free(ws);
        return nullptr;
    }

    std::vector<double> bufA(static_cast<size_t>(L)), bufB(static_cast<size_t>(L));
    std::vector<double> unpA(static_cast<size_t>(2 * L)), unpB(static_cast<size_t>(2 * L));
    int nSegs = 0;

    for (int start = 0; start + L <= nGrid; start += H) {
        double meanA = 0.0, meanB = 0.0;
        for (int m = 0; m < L; ++m) {
            meanA += sigA[static_cast<size_t>(start + m)];
            meanB += sigB[static_cast<size_t>(start + m)];
        }
        meanA /= static_cast<double>(L);
        meanB /= static_cast<double>(L);

        for (int m = 0; m < L; ++m) {
            bufA[static_cast<size_t>(m)] =
                    (sigA[static_cast<size_t>(start + m)] - meanA) * win[static_cast<size_t>(m)];
            bufB[static_cast<size_t>(m)] =
                    (sigB[static_cast<size_t>(start + m)] - meanB) * win[static_cast<size_t>(m)];
        }

        gsl_fft_real_transform(bufA.data(), 1, static_cast<size_t>(L), wt, ws);
        gsl_fft_halfcomplex_unpack(bufA.data(), unpA.data(), 1, static_cast<size_t>(L));

        gsl_fft_real_transform(bufB.data(), 1, static_cast<size_t>(L), wt, ws);
        gsl_fft_halfcomplex_unpack(bufB.data(), unpB.data(), 1, static_cast<size_t>(L));

        for (int k = 0; k < nBins; ++k) {
            const cdbl Xk(unpA[static_cast<size_t>(2 * k)], unpA[static_cast<size_t>(2 * k + 1)]);
            const cdbl Yk(unpB[static_cast<size_t>(2 * k)], unpB[static_cast<size_t>(2 * k + 1)]);
            pxx[static_cast<size_t>(k)] += std::norm(Xk);
            pyy[static_cast<size_t>(k)] += std::norm(Yk);
            pxy[static_cast<size_t>(k)] += Xk * std::conj(Yk);
        }
        ++nSegs;
    }

    gsl_fft_real_wavetable_free(wt);
    gsl_fft_real_workspace_free(ws);

    if (nSegs == 0)
        return nullptr;

    std::vector<double> freqs(static_cast<size_t>(nBins));
    std::vector<double> coh(static_cast<size_t>(nBins), 0.0);
    std::vector<double> phaseDeg(static_cast<size_t>(nBins), 0.0);

    for (int k = 0; k < nBins; ++k) {
        freqs[static_cast<size_t>(k)] = static_cast<double>(k) * fs / static_cast<double>(L);
        const double denom = pxx[static_cast<size_t>(k)] * pyy[static_cast<size_t>(k)];
        if (denom > 1e-300) {
            coh[static_cast<size_t>(k)] =
                    std::clamp(std::norm(pxy[static_cast<size_t>(k)]) / denom, 0.0, 1.0);
        }
        phaseDeg[static_cast<size_t>(k)] = std::arg(pxy[static_cast<size_t>(k)]) * (180.0 / M_PI);
    }

    Table *tbl = buildSignalTable(
            app, "Coherence", QStringList() << "FrequencyHz" << "Coherence" << "PhaseDeg",
            QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y << SciDAVis::Y,
            { freqs, coh, phaseDeg });
    if (!tbl)
        return nullptr;

    if (plotResult) {
        MultiLayer *ml = app->multilayerPlot(tbl, QStringList() << tbl->colName(1), Graph::Line, 0,
                                             nBins - 1);
        if (ml && ml->activeGraph()) {
            ml->activeGraph()->setXAxisTitle(trSig("Frequency (Hz)"));
            ml->activeGraph()->setYAxisTitle(trSig("Magnitude-Squared Coherence"));
            ml->activeGraph()->setScale(QwtPlot::yLeft, 0.0, 1.05);
            ml->activeGraph()->replot();
        }
    }

    return tbl;
}

// ============================================================================
// OriginPro Signal Processing Dialogs
// ============================================================================

void AdvancedSignalProc::showIirFilterDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    registerSignalProcTranslationsOnce();
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trSig("IIR Digital Filter"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);

    QComboBox *cbDesign = new QComboBox(&dlg);
    cbDesign->addItem(trSig("Butterworth"), static_cast<int>(Butterworth));
    cbDesign->addItem(trSig("Chebyshev Type I"), static_cast<int>(ChebyshevI));
    cbDesign->addItem(trSig("Bessel"), static_cast<int>(Bessel));

    QComboBox *cbPass = new QComboBox(&dlg);
    cbPass->addItem(trSig("Low Pass"), static_cast<int>(LowPass));
    cbPass->addItem(trSig("High Pass"), static_cast<int>(HighPass));
    cbPass->addItem(trSig("Band Pass"), static_cast<int>(BandPass));
    cbPass->addItem(trSig("Band Stop (Notch)"), static_cast<int>(BandStop));

    QSpinBox *sbOrder = new QSpinBox(&dlg);
    sbOrder->setRange(1, 12);
    sbOrder->setValue(4);

    std::vector<double> t, y;
    double currentFs = 100.0;
    extractUniformSignal(g, curves.first(), t, y, currentFs);
    if (currentFs <= 0.0 || !std::isfinite(currentFs))
        currentFs = 100.0;

    QLabel *lblFs = new QLabel(
            QObject::tr("%1 Hz (Nyquist: %2 Hz)")
                    .arg(currentFs, 0, 'g', 5)
                    .arg(0.5 * currentFs, 0, 'g', 5),
            &dlg);

    QDoubleSpinBox *sbLow = new QDoubleSpinBox(&dlg);
    sbLow->setRange(1e-6, 1e12);
    sbLow->setDecimals(4);
    sbLow->setValue(std::max(1e-5, 0.10 * currentFs));

    QDoubleSpinBox *sbHigh = new QDoubleSpinBox(&dlg);
    sbHigh->setRange(1e-6, 1e12);
    sbHigh->setDecimals(4);
    sbHigh->setValue(std::max(2e-5, 0.25 * currentFs));
    sbHigh->setEnabled(false);

    QDoubleSpinBox *sbRipple = new QDoubleSpinBox(&dlg);
    sbRipple->setRange(0.01, 20.0);
    sbRipple->setDecimals(2);
    sbRipple->setValue(1.0);
    sbRipple->setEnabled(false);

    QCheckBox *chkZeroPhase = new QCheckBox(trSig("Zero-Phase Forward-Backward (filtfilt)"), &dlg);
    chkZeroPhase->setChecked(true);

    QCheckBox *chkPreview = new QCheckBox(QObject::tr("Live Preview"), &dlg);
    chkPreview->setChecked(true);

    SignalPreviewGuard previewGuard(g);
    auto refreshPreview = [&]() {
        if (!chkPreview->isChecked()) {
            previewGuard.clear();
            return;
        }
        std::vector<double> px, py;
        double pfs = 1.0;
        if (computeIirFilteredVector(g, cbCurve->currentText(),
                                     static_cast<IirDesign>(cbDesign->currentData().toInt()),
                                     static_cast<PassType>(cbPass->currentData().toInt()),
                                     sbOrder->value(), sbLow->value(), sbHigh->value(),
                                     chkZeroPhase->isChecked(), sbRipple->value(), px, py, pfs)) {
            previewGuard.setSingle(px, py, QPen(QColor(0, 114, 178), 2.0, Qt::DashLine));
        } else {
            previewGuard.clear();
        }
    };

    QObject::connect(cbCurve, &QComboBox::currentTextChanged, &dlg,
                     [&](const QString &curveName) {
                         std::vector<double> tmpT, tmpY;
                         double newFs = currentFs;
                         if (extractUniformSignal(g, curveName, tmpT, tmpY, newFs)
                             && newFs > 0.0 && std::isfinite(newFs)) {
                             const double ratioLow = (currentFs > 0.0)
                                     ? std::clamp(sbLow->value() / currentFs, 0.001, 0.48)
                                     : 0.10;
                             const double ratioHigh = (currentFs > 0.0)
                                     ? std::clamp(sbHigh->value() / currentFs,
                                                  std::min(0.49, ratioLow + 0.01), 0.495)
                                     : 0.25;
                             currentFs = newFs;
                             lblFs->setText(QObject::tr("%1 Hz (Nyquist: %2 Hz)")
                                                    .arg(currentFs, 0, 'g', 5)
                                                    .arg(0.5 * currentFs, 0, 'g', 5));
                             sbLow->blockSignals(true);
                             sbHigh->blockSignals(true);
                             sbLow->setValue(std::max(1e-6, ratioLow * currentFs));
                             sbHigh->setValue(std::max(2e-6, ratioHigh * currentFs));
                             sbLow->blockSignals(false);
                             sbHigh->blockSignals(false);
                         }
                         refreshPreview();
                     });
    QObject::connect(cbPass, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int idx) {
                         const int p = cbPass->itemData(idx).toInt();
                         sbHigh->setEnabled(p == BandPass || p == BandStop);
                         refreshPreview();
                     });
    QObject::connect(cbDesign, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int idx) {
                         sbRipple->setEnabled(cbDesign->itemData(idx).toInt() == ChebyshevI);
                         refreshPreview();
                     });
    QObject::connect(sbOrder, QOverload<int>::of(&QSpinBox::valueChanged), &dlg,
                     [&](int) { refreshPreview(); });
    QObject::connect(sbLow, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                     [&](double) { refreshPreview(); });
    QObject::connect(sbHigh, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                     [&](double) { refreshPreview(); });
    QObject::connect(sbRipple, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                     [&](double) { refreshPreview(); });
    QObject::connect(chkZeroPhase, &QCheckBox::toggled, &dlg, [&](bool) { refreshPreview(); });
    QObject::connect(chkPreview, &QCheckBox::toggled, &dlg, [&](bool) { refreshPreview(); });

    form->addRow(trSig("Curve:"), cbCurve);
    form->addRow(QObject::tr("Detected Sampling Rate:"), lblFs);
    form->addRow(trSig("Prototype Design:"), cbDesign);
    form->addRow(trSig("Pass Type:"), cbPass);
    form->addRow(trSig("Filter Order:"), sbOrder);
    form->addRow(trSig("Cutoff / Low Frequency (Hz):"), sbLow);
    form->addRow(trSig("Upper Cutoff Frequency (Hz):"), sbHigh);
    form->addRow(trSig("Passband Ripple (dB):"), sbRipple);
    form->addRow(QString(), chkZeroPhase);
    form->addRow(QString(), chkPreview);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    refreshPreview();

    if (dlg.exec() == QDialog::Accepted) {
        previewGuard.clear();
        iirFilter(app, g, cbCurve->currentText(),
                  static_cast<IirDesign>(cbDesign->currentData().toInt()),
                  static_cast<PassType>(cbPass->currentData().toInt()), sbOrder->value(),
                  sbLow->value(), sbHigh->value(), chkZeroPhase->isChecked(), sbRipple->value());
    }
}

void AdvancedSignalProc::showStftDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trSig("Short-Time Fourier Transform (STFT)"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);
    QSpinBox *sbWin = new QSpinBox(&dlg);
    sbWin->setRange(8, 16384);
    sbWin->setSingleStep(16);
    sbWin->setValue(64);
    QSpinBox *sbHop = new QSpinBox(&dlg);
    sbHop->setRange(1, 8192);
    sbHop->setSingleStep(8);
    sbHop->setValue(16);
    QCheckBox *chkPlot = new QCheckBox(trSig("Plot Time-Frequency Spectrogram"), &dlg);
    chkPlot->setChecked(true);

    form->addRow(trSig("Curve:"), cbCurve);
    form->addRow(trSig("Window Length (Samples):"), sbWin);
    form->addRow(trSig("Hop Size (Samples):"), sbHop);
    form->addRow(QString(), chkPlot);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted)
        computeSTFT(app, g, cbCurve->currentText(), sbWin->value(), sbHop->value(),
                    chkPlot->isChecked());
}

void AdvancedSignalProc::showHilbertDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trSig("Hilbert Transform & Envelope"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);
    QCheckBox *chkLower = new QCheckBox(trSig("Also Plot Lower Envelope (-|z(t)|)"), &dlg);
    chkLower->setChecked(true);

    form->addRow(trSig("Curve:"), cbCurve);
    form->addRow(QString(), chkLower);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted)
        hilbertEnvelope(app, g, cbCurve->currentText(), chkLower->isChecked(), nullptr);
}

void AdvancedSignalProc::showWaveletDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trSig("Wavelet Analysis (CWT & DWT Denoising)"));
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    QTabWidget *tabs = new QTabWidget(&dlg);

    // Tab 1: Continuous Wavelet Transform
    QWidget *tabCwt = new QWidget(&dlg);
    QFormLayout *fCwt = new QFormLayout(tabCwt);
    QComboBox *cbCwtCurve = new QComboBox(tabCwt);
    cbCwtCurve->addItems(curves);
    QComboBox *cbCwtType = new QComboBox(tabCwt);
    cbCwtType->addItem(trSig("Morlet (omega0 = 6)"), static_cast<int>(Morlet));
    cbCwtType->addItem(trSig("Ricker / Mexican Hat"), static_cast<int>(RickerMexicanHat));
    cbCwtType->addItem(trSig("Haar"), static_cast<int>(Haar));
    QSpinBox *sbScales = new QSpinBox(tabCwt);
    sbScales->setRange(4, 512);
    sbScales->setValue(32);
    QCheckBox *chkScalogram = new QCheckBox(trSig("Plot CWT Scalogram"), tabCwt);
    chkScalogram->setChecked(true);
    fCwt->addRow(trSig("Curve:"), cbCwtCurve);
    fCwt->addRow(trSig("Mother Wavelet:"), cbCwtType);
    fCwt->addRow(trSig("Number of Scales:"), sbScales);
    fCwt->addRow(QString(), chkScalogram);
    tabs->addTab(tabCwt, trSig("Continuous Wavelet (CWT)"));

    // Tab 2: DWT VisuShrink Denoise
    QWidget *tabDwt = new QWidget(&dlg);
    QFormLayout *fDwt = new QFormLayout(tabDwt);
    QComboBox *cbDwtCurve = new QComboBox(tabDwt);
    cbDwtCurve->addItems(curves);
    QComboBox *cbDwtType = new QComboBox(tabDwt);
    cbDwtType->addItem(trSig("Daubechies-4 (db4)"), static_cast<int>(Daubechies4));
    cbDwtType->addItem(trSig("Haar (db1)"), static_cast<int>(Haar));
    QSpinBox *sbLevels = new QSpinBox(tabDwt);
    sbLevels->setRange(1, 10);
    sbLevels->setValue(4);
    QCheckBox *chkSoft = new QCheckBox(trSig("Soft Thresholding (VisuShrink)"), tabDwt);
    chkSoft->setChecked(true);
    QDoubleSpinBox *sbThreshScale = new QDoubleSpinBox(tabDwt);
    sbThreshScale->setRange(0.05, 10.0);
    sbThreshScale->setDecimals(2);
    sbThreshScale->setSingleStep(0.1);
    sbThreshScale->setValue(1.0);
    fDwt->addRow(trSig("Curve:"), cbDwtCurve);
    fDwt->addRow(trSig("Wavelet Family:"), cbDwtType);
    fDwt->addRow(trSig("Decomposition Levels:"), sbLevels);
    fDwt->addRow(trSig("Threshold Scale:"), sbThreshScale);
    fDwt->addRow(QString(), chkSoft);
    tabs->addTab(tabDwt, trSig("DWT Denoise"));

    vbox->addWidget(tabs);
    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        if (tabs->currentIndex() == 0) {
            waveletCWT(app, g, cbCwtCurve->currentText(), sbScales->value(),
                       static_cast<WaveletType>(cbCwtType->currentData().toInt()),
                       chkScalogram->isChecked());
        } else {
            waveletDenoiseDWT(app, g, cbDwtCurve->currentText(),
                              static_cast<WaveletType>(cbDwtType->currentData().toInt()),
                              sbLevels->value(), chkSoft->isChecked(), sbThreshScale->value());
        }
    }
}

void AdvancedSignalProc::showPsdCoherenceDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(trSig("Welch PSD & Cross-Coherence"));
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    QTabWidget *tabs = new QTabWidget(&dlg);

    QWidget *tabPsd = new QWidget(&dlg);
    QFormLayout *fPsd = new QFormLayout(tabPsd);
    QComboBox *cbPsdCurve = new QComboBox(tabPsd);
    cbPsdCurve->addItems(curves);
    QSpinBox *sbPsdSeg = new QSpinBox(tabPsd);
    sbPsdSeg->setRange(8, 65536);
    sbPsdSeg->setSingleStep(32);
    sbPsdSeg->setValue(128);
    QCheckBox *chkPsdPlot = new QCheckBox(trSig("Plot PSD Result"), tabPsd);
    chkPsdPlot->setChecked(true);
    fPsd->addRow(trSig("Curve:"), cbPsdCurve);
    fPsd->addRow(trSig("Segment Length:"), sbPsdSeg);
    fPsd->addRow(QString(), chkPsdPlot);
    tabs->addTab(tabPsd, trSig("Welch PSD"));

    QWidget *tabCoh = new QWidget(&dlg);
    QFormLayout *fCoh = new QFormLayout(tabCoh);
    QComboBox *cbCohA = new QComboBox(tabCoh);
    cbCohA->addItems(curves);
    QComboBox *cbCohB = new QComboBox(tabCoh);
    cbCohB->addItems(curves);
    if (curves.size() >= 2)
        cbCohB->setCurrentIndex(1);
    QSpinBox *sbCohSeg = new QSpinBox(tabCoh);
    sbCohSeg->setRange(8, 65536);
    sbCohSeg->setSingleStep(32);
    sbCohSeg->setValue(128);
    QCheckBox *chkCohPlot = new QCheckBox(trSig("Plot Coherence Result"), tabCoh);
    chkCohPlot->setChecked(true);
    fCoh->addRow(trSig("Signal A:"), cbCohA);
    fCoh->addRow(trSig("Signal B:"), cbCohB);
    fCoh->addRow(trSig("Segment Length:"), sbCohSeg);
    fCoh->addRow(QString(), chkCohPlot);
    tabs->addTab(tabCoh, trSig("Cross-Coherence"));

    vbox->addWidget(tabs);
    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        if (tabs->currentIndex() == 0) {
            welchPSD(app, g, cbPsdCurve->currentText(), sbPsdSeg->value(), chkPsdPlot->isChecked());
        } else {
            crossCoherence(app, g, cbCohA->currentText(), cbCohB->currentText(), sbCohSeg->value(),
                           chkCohPlot->isChecked());
        }
    }
}

QwtPlotCurve *AdvancedSignalProc::savitzkyGolayDerivative(ApplicationWindow *app, Graph *g,
                                                          const QString &curveTitle,
                                                          int derivOrder, int windowPoints,
                                                          int polyOrder)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> xs, deriv;
    int d = 1;
    if (!computeSavGolDerivVector(g, curveTitle, derivOrder, windowPoints, polyOrder, xs, deriv, &d))
        return nullptr;
    const int n = static_cast<int>(xs.size());

    const QString colLabel = (d == 1) ? QStringLiteral("dYdX")
                                      : QStringLiteral("d%1YdX%1").arg(d);
    Table *tbl = buildSignalTable(app, QStringLiteral("SavGolDeriv%1").arg(d),
                                  QStringList() << QStringLiteral("X") << colLabel,
                                  QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                                  { xs, deriv });
    if (!tbl)
        return nullptr;

    if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, n - 1))
        return nullptr;

    QwtPlotCurve *resCurve = g->curve(g->curves() - 1);
    if (resCurve)
        resCurve->setPen(QPen(QColor(180, 35, 110), 2.0, Qt::SolidLine));
    g->updatePlot();

    if (resCurve) {
        if (RecalcManager *mgr = RecalcManager::instance(app)) {
            if (!mgr->isRecalculating() && mgr->recalcMode() != RecalcManager::Disabled) {
                QVariantMap params;
                params.insert(QStringLiteral("derivOrder"), derivOrder);
                params.insert(QStringLiteral("windowPoints"), windowPoints);
                params.insert(QStringLiteral("polyOrder"), polyOrder);
                mgr->registerLink(app, g, nullptr, curveTitle, resCurve->title().text(),
                                  RecalcManager::OpSavGolDeriv, params, nullptr);
            }
        }
    }
    return resCurve;
}

QwtPlotCurve *AdvancedSignalProc::computePeakEnvelope(ApplicationWindow *app, Graph *g,
                                                      const QString &curveTitle,
                                                      EnvelopeType envType, int minPeakDistPts,
                                                      QwtPlotCurve **lowerCurveOut)
{
    if (lowerCurveOut)
        *lowerCurveOut = nullptr;
    if (!app || !g)
        return nullptr;

    std::vector<double> xs, ys;
    if (!extractSortedSignalXY(g, curveTitle, xs, ys) || xs.size() < 2)
        return nullptr;

    const int n = static_cast<int>(xs.size());
    std::vector<double> upperEnv;
    std::vector<double> lowerEnv;

    if (envType == EnvelopeUpper || envType == EnvelopeBoth)
        upperEnv = computeSplineUpperEnvelope(xs, ys, minPeakDistPts);

    if (envType == EnvelopeLower || envType == EnvelopeBoth) {
        std::vector<double> negY(static_cast<size_t>(n), 0.0);
        for (int i = 0; i < n; ++i)
            negY[static_cast<size_t>(i)] = -ys[static_cast<size_t>(i)];
        std::vector<double> negEnv = computeSplineUpperEnvelope(xs, negY, minPeakDistPts);
        lowerEnv.resize(static_cast<size_t>(n), 0.0);
        for (int i = 0; i < n; ++i)
            lowerEnv[static_cast<size_t>(i)] = -negEnv[static_cast<size_t>(i)];
    }

    if (envType == EnvelopeUpper) {
        Table *tbl = buildSignalTable(
                app, QStringLiteral("PeakEnvelope"),
                QStringList() << QStringLiteral("X") << QStringLiteral("UpperEnvelope"),
                QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                { xs, upperEnv });
        if (!tbl)
            return nullptr;
        if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, n - 1))
            return nullptr;
        QwtPlotCurve *upperCurve = g->curve(g->curves() - 1);
        if (upperCurve)
            upperCurve->setPen(QPen(QColor(213, 62, 20), 2.0, Qt::SolidLine));
        g->updatePlot();
        return upperCurve;
    }

    if (envType == EnvelopeLower) {
        Table *tbl = buildSignalTable(
                app, QStringLiteral("PeakEnvelope"),
                QStringList() << QStringLiteral("X") << QStringLiteral("LowerEnvelope"),
                QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
                { xs, lowerEnv });
        if (!tbl)
            return nullptr;
        if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, n - 1))
            return nullptr;
        QwtPlotCurve *lowCurve = g->curve(g->curves() - 1);
        if (lowCurve)
            lowCurve->setPen(QPen(QColor(20, 110, 190), 2.0, Qt::DashLine));
        if (lowerCurveOut)
            *lowerCurveOut = lowCurve;
        g->updatePlot();
        return lowCurve;
    }

    // EnvelopeBoth
    Table *tbl = buildSignalTable(
            app, QStringLiteral("PeakEnvelope"),
            QStringList() << QStringLiteral("X") << QStringLiteral("UpperEnvelope")
                          << QStringLiteral("LowerEnvelope"),
            QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y << SciDAVis::Y,
            { xs, upperEnv, lowerEnv });
    if (!tbl)
        return nullptr;

    QwtPlotCurve *upperCurve = nullptr;
    QwtPlotCurve *lowCurve = nullptr;
    if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, n - 1)) {
        upperCurve = g->curve(g->curves() - 1);
        if (upperCurve)
            upperCurve->setPen(QPen(QColor(213, 62, 20), 2.0, Qt::SolidLine));
    }
    if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(2), Graph::Line, 0, n - 1)) {
        lowCurve = g->curve(g->curves() - 1);
        if (lowCurve)
            lowCurve->setPen(QPen(QColor(20, 110, 190), 2.0, Qt::DashLine));
    }
    if (lowerCurveOut)
        *lowerCurveOut = lowCurve;
    g->updatePlot();
    return upperCurve;
}

QwtPlotCurve *AdvancedSignalProc::decimateCurve(ApplicationWindow *app, Graph *g,
                                                const QString &curveTitle,
                                                int factor, bool firAntiAlias)
{
    if (!app || !g)
        return nullptr;

    std::vector<double> xs, ys;
    if (!extractSortedSignalXY(g, curveTitle, xs, ys) || xs.size() < 2)
        return nullptr;

    const int n = static_cast<int>(ys.size());
    if (n >= 3) {
        const double meanDx = (xs.back() - xs.front()) / static_cast<double>(n - 1);
        if (meanDx > 1e-300) {
            double maxDev = 0.0;
            for (int i = 0; i + 1 < n; ++i) {
                const double dxi = xs[static_cast<size_t>(i + 1)] - xs[static_cast<size_t>(i)];
                maxDev = std::max(maxDev, std::fabs(dxi - meanDx));
            }
            if (maxDev > 0.05 * meanDx) {
                // Non-uniform spacing (>5% step variation): resample onto uniform grid before FIR/decimation
                std::vector<double> uniX(static_cast<size_t>(n), 0.0);
                std::vector<double> uniY(static_cast<size_t>(n), 0.0);
                uniX[0] = xs.front();
                uniY[0] = ys.front();
                uniX[static_cast<size_t>(n - 1)] = xs.back();
                uniY[static_cast<size_t>(n - 1)] = ys.back();
                size_t seg = 0;
                for (int i = 1; i < n - 1; ++i) {
                    const double xq = xs.front() + static_cast<double>(i) * meanDx;
                    uniX[static_cast<size_t>(i)] = xq;
                    while (seg + 1 < static_cast<size_t>(n - 1) && xs[seg + 1] < xq)
                        ++seg;
                    const double dxSeg = xs[seg + 1] - xs[seg];
                    const double alpha = (dxSeg > 1e-300) ? ((xq - xs[seg]) / dxSeg) : 0.0;
                    uniY[static_cast<size_t>(i)] = ys[seg] + alpha * (ys[seg + 1] - ys[seg]);
                }
                xs.swap(uniX);
                ys.swap(uniY);
            }
        }
    }

    const int M = std::clamp(factor, 2, std::max(2, n));

    std::vector<double> xDec;
    std::vector<double> yDec;
    xDec.reserve(static_cast<size_t>((n + M - 1) / M));
    yDec.reserve(static_cast<size_t>((n + M - 1) / M));

    if (firAntiAlias) {
        // Symmetric Kaiser-windowed sinc lowpass anti-aliasing FIR filter (cutoff = 0.8 / M Nyquist)
        const int halfTap = std::clamp(8 * M, 16, 256);
        const double fcNorm = 0.8 / static_cast<double>(M); // fraction of Nyquist
        const double beta = 5.658; // ~60 dB stopband attenuation
        const double invI0Beta = 1.0 / besselI0(beta);

        std::vector<double> h(static_cast<size_t>(2 * halfTap + 1), 0.0);
        double hSum = 0.0;
        for (int k = -halfTap; k <= halfTap; ++k) {
            const double sincVal = (k == 0)
                    ? fcNorm
                    : (std::sin(M_PI * fcNorm * static_cast<double>(k))
                       / (M_PI * static_cast<double>(k)));
            const double u = static_cast<double>(k) / static_cast<double>(halfTap);
            const double winVal = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - u * u))) * invI0Beta;
            const double tap = sincVal * winVal;
            h[static_cast<size_t>(k + halfTap)] = tap;
            hSum += tap;
        }
        if (std::fabs(hSum) > 1e-30) {
            for (double &tap : h)
                tap /= hSum;
        }

        auto sampleOddReflect = [&](int idx) -> double {
            if (n == 1)
                return ys[0];
            if (idx >= 0 && idx < n)
                return ys[static_cast<size_t>(idx)];
            if (idx < 0) {
                const int m = std::min(n - 1, -idx);
                return 2.0 * ys[0] - ys[static_cast<size_t>(m)];
            }
            const int m = std::max(0, 2 * (n - 1) - idx);
            return 2.0 * ys[static_cast<size_t>(n - 1)] - ys[static_cast<size_t>(m)];
        };

        for (int i = 0; i < n; i += M) {
            double acc = 0.0;
            for (int k = -halfTap; k <= halfTap; ++k)
                acc += h[static_cast<size_t>(k + halfTap)] * sampleOddReflect(i - k);
            xDec.push_back(xs[static_cast<size_t>(i)]);
            yDec.push_back(acc);
        }
    } else {
        // Block moving average downsampling
        const int halfLeft = (M - 1) / 2;
        const int halfRight = M - 1 - halfLeft;
        for (int i = 0; i < n; i += M) {
            const int lo = std::max(0, i - halfLeft);
            const int hi = std::min(n - 1, i + halfRight);
            double sumY = 0.0;
            for (int k = lo; k <= hi; ++k)
                sumY += ys[static_cast<size_t>(k)];
            xDec.push_back(xs[static_cast<size_t>(i)]);
            yDec.push_back(sumY / static_cast<double>(hi - lo + 1));
        }
    }

    if (xDec.size() < 2 && n >= 2) {
        xDec.push_back(xs.back());
        yDec.push_back(ys.back());
    }

    const int nOut = static_cast<int>(xDec.size());
    Table *tbl = buildSignalTable(
            app, QStringLiteral("Decimated"),
            QStringList() << QStringLiteral("X") << QStringLiteral("DecimatedY"),
            QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
            { xDec, yDec });
    if (!tbl)
        return nullptr;

    if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, nOut - 1))
        return nullptr;

    QwtPlotCurve *resCurve = g->curve(g->curves() - 1);
    if (resCurve)
        resCurve->setPen(QPen(QColor(15, 135, 95), 2.0, Qt::SolidLine));
    g->updatePlot();
    return resCurve;
}

void AdvancedSignalProc::showSavGolDerivativeDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    registerSignalProcTranslationsOnce();
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Savitzky-Golay Derivative"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);

    QComboBox *cbOrder = new QComboBox(&dlg);
    cbOrder->addItem(QObject::tr("1st Derivative (dy/dx)"), 1);
    cbOrder->addItem(QObject::tr("2nd Derivative (d2y/dx2)"), 2);
    cbOrder->addItem(QObject::tr("3rd Derivative (d3y/dx3)"), 3);
    cbOrder->addItem(QObject::tr("4th Derivative (d4y/dx4)"), 4);

    QSpinBox *sbPoly = new QSpinBox(&dlg);
    sbPoly->setRange(1, 8);
    sbPoly->setValue(3);

    QSpinBox *sbWin = new QSpinBox(&dlg);
    sbWin->setRange(3, 501);
    sbWin->setSingleStep(2);
    sbWin->setValue(9);

    QCheckBox *chkPreview = new QCheckBox(QObject::tr("Live Preview"), &dlg);
    chkPreview->setChecked(true);

    SignalPreviewGuard previewGuard(g);
    auto refreshPreview = [&]() {
        if (!chkPreview->isChecked()) {
            previewGuard.clear();
            return;
        }
        std::vector<double> px, pd;
        if (computeSavGolDerivVector(g, cbCurve->currentText(), cbOrder->currentData().toInt(),
                                     sbWin->value(), sbPoly->value(), px, pd)) {
            previewGuard.setSingle(px, pd, QPen(QColor(180, 35, 110), 2.0, Qt::DashLine));
        } else {
            previewGuard.clear();
        }
    };

    QObject::connect(cbOrder, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int idx) {
                         const int ord = cbOrder->itemData(idx).toInt();
                         if (sbPoly->value() < ord)
                             sbPoly->setValue(ord);
                         if (sbWin->value() < sbPoly->value() + 1) {
                             int w = sbPoly->value() + 1;
                             if (w % 2 == 0)
                                 ++w;
                             sbWin->setValue(w);
                         }
                         refreshPreview();
                     });
    QObject::connect(cbCurve, &QComboBox::currentTextChanged, &dlg,
                     [&](const QString &) { refreshPreview(); });
    QObject::connect(sbPoly, QOverload<int>::of(&QSpinBox::valueChanged), &dlg,
                     [&](int) { refreshPreview(); });
    QObject::connect(sbWin, QOverload<int>::of(&QSpinBox::valueChanged), &dlg,
                     [&](int) { refreshPreview(); });
    QObject::connect(chkPreview, &QCheckBox::toggled, &dlg, [&](bool) { refreshPreview(); });

    form->addRow(QObject::tr("Curve:"), cbCurve);
    form->addRow(QObject::tr("Derivative Order:"), cbOrder);
    form->addRow(QObject::tr("Polynomial Order:"), sbPoly);
    form->addRow(QObject::tr("Window Points (Odd):"), sbWin);
    form->addRow(QString(), chkPreview);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    refreshPreview();

    if (dlg.exec() == QDialog::Accepted) {
        previewGuard.clear();
        savitzkyGolayDerivative(app, g, cbCurve->currentText(),
                                cbOrder->currentData().toInt(), sbWin->value(), sbPoly->value());
    }
}

void AdvancedSignalProc::showPeakEnvelopeDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    registerSignalProcTranslationsOnce();
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Smooth Peak Envelope"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);

    QComboBox *cbType = new QComboBox(&dlg);
    cbType->addItem(QObject::tr("Both Envelopes (Upper & Lower)"), static_cast<int>(EnvelopeBoth));
    cbType->addItem(QObject::tr("Upper Envelope Only"), static_cast<int>(EnvelopeUpper));
    cbType->addItem(QObject::tr("Lower Envelope Only"), static_cast<int>(EnvelopeLower));

    QSpinBox *sbMinDist = new QSpinBox(&dlg);
    sbMinDist->setRange(1, 5000);
    sbMinDist->setValue(3);

    QCheckBox *chkPreview = new QCheckBox(QObject::tr("Live Preview"), &dlg);
    chkPreview->setChecked(true);

    SignalPreviewGuard previewGuard(g);
    auto refreshPreview = [&]() {
        if (!chkPreview->isChecked()) {
            previewGuard.clear();
            return;
        }
        std::vector<double> xs, ys;
        if (!extractSortedSignalXY(g, cbCurve->currentText(), xs, ys) || xs.size() < 2) {
            previewGuard.clear();
            return;
        }
        const EnvelopeType envType = static_cast<EnvelopeType>(cbType->currentData().toInt());
        const int minDist = sbMinDist->value();
        std::vector<double> upperEnv;
        std::vector<double> lowerEnv;
        const bool wantUpper = (envType == EnvelopeUpper || envType == EnvelopeBoth);
        const bool wantLower = (envType == EnvelopeLower || envType == EnvelopeBoth);
        if (wantUpper)
            upperEnv = computeSplineUpperEnvelope(xs, ys, minDist);
        if (wantLower) {
            std::vector<double> negY(ys.size(), 0.0);
            for (size_t i = 0; i < ys.size(); ++i)
                negY[i] = -ys[i];
            std::vector<double> negEnv = computeSplineUpperEnvelope(xs, negY, minDist);
            lowerEnv.resize(ys.size(), 0.0);
            for (size_t i = 0; i < ys.size(); ++i)
                lowerEnv[i] = -negEnv[i];
        }
        previewGuard.setDual(xs, wantUpper ? &upperEnv : nullptr,
                             QPen(QColor(213, 62, 20), 2.0, Qt::DashLine),
                             wantLower ? &lowerEnv : nullptr,
                             QPen(QColor(20, 110, 190), 2.0, Qt::DotLine));
    };

    QObject::connect(cbCurve, &QComboBox::currentTextChanged, &dlg,
                     [&](const QString &) { refreshPreview(); });
    QObject::connect(cbType, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refreshPreview(); });
    QObject::connect(sbMinDist, QOverload<int>::of(&QSpinBox::valueChanged), &dlg,
                     [&](int) { refreshPreview(); });
    QObject::connect(chkPreview, &QCheckBox::toggled, &dlg, [&](bool) { refreshPreview(); });

    form->addRow(QObject::tr("Curve:"), cbCurve);
    form->addRow(QObject::tr("Envelope Type:"), cbType);
    form->addRow(QObject::tr("Min Peak Separation (Points):"), sbMinDist);
    form->addRow(QString(), chkPreview);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    refreshPreview();

    if (dlg.exec() == QDialog::Accepted) {
        previewGuard.clear();
        computePeakEnvelope(app, g, cbCurve->currentText(),
                            static_cast<EnvelopeType>(cbType->currentData().toInt()),
                            sbMinDist->value(), nullptr);
    }
}

void AdvancedSignalProc::showDecimateDialog(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    registerSignalProcTranslationsOnce();
    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Signal Decimation / Resampling"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);

    QSpinBox *sbFactor = new QSpinBox(&dlg);
    sbFactor->setRange(2, 1000);
    sbFactor->setValue(4);

    QCheckBox *chkFir =
            new QCheckBox(QObject::tr("Apply Kaiser-Sinc FIR Anti-Aliasing Filter"), &dlg);
    chkFir->setChecked(true);

    form->addRow(QObject::tr("Curve:"), cbCurve);
    form->addRow(QObject::tr("Decimation Factor:"), sbFactor);
    form->addRow(QString(), chkFir);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        decimateCurve(app, g, cbCurve->currentText(), sbFactor->value(), chkFir->isChecked());
    }
}

namespace {

bool computeFsdSpectrum(const std::vector<double> &xs, const std::vector<double> &ys,
                        double lorentzFwhmGamma, double sharpeningFactorK, int apodization,
                        std::vector<double> &yOut)
{
    yOut.clear();
    const int n = static_cast<int>(xs.size());
    if (n < 4 || static_cast<int>(ys.size()) != n || !(lorentzFwhmGamma > 0.0)
        || !std::isfinite(lorentzFwhmGamma))
        return false;

    const double dx = (xs.back() - xs.front()) / static_cast<double>(n - 1);
    if (!(dx > 0.0) || !std::isfinite(dx))
        return false;

    const double kClamped = std::clamp(std::isfinite(sharpeningFactorK) ? sharpeningFactorK : 1.8,
                                       1.0, 4.0);
    const double tNyq = 0.5 / dx;
    const double tMax = std::min(kClamped / lorentzFwhmGamma, 0.98 * tNyq);
    if (!(tMax > 0.0) || !std::isfinite(tMax))
        return false;

    // Linear endpoint baseline subtraction so periodic/padded FFT has zero step jump
    const int nEdge = std::max(1, std::min(4, n / 8));
    double yLeft = 0.0, xLeft = 0.0, yRight = 0.0, xRight = 0.0;
    for (int i = 0; i < nEdge; ++i) {
        xLeft += xs[static_cast<size_t>(i)];
        yLeft += ys[static_cast<size_t>(i)];
        xRight += xs[static_cast<size_t>(n - 1 - i)];
        yRight += ys[static_cast<size_t>(n - 1 - i)];
    }
    xLeft /= static_cast<double>(nEdge);
    yLeft /= static_cast<double>(nEdge);
    xRight /= static_cast<double>(nEdge);
    yRight /= static_cast<double>(nEdge);
    const double baseSlope = (std::fabs(xRight - xLeft) > 1e-30)
            ? ((yRight - yLeft) / (xRight - xLeft))
            : 0.0;
    auto evalBase = [&](double xv) { return yLeft + baseSlope * (xv - xLeft); };

    std::vector<double> detrended(static_cast<size_t>(n), 0.0);
    double origNetArea = 0.0;
    for (int i = 0; i < n; ++i)
        detrended[static_cast<size_t>(i)] = ys[static_cast<size_t>(i)] - evalBase(xs[static_cast<size_t>(i)]);
    for (int i = 0; i + 1 < n; ++i) {
        origNetArea += 0.5 * (detrended[static_cast<size_t>(i)] + detrended[static_cast<size_t>(i + 1)])
                * (xs[static_cast<size_t>(i + 1)] - xs[static_cast<size_t>(i)]);
    }

    const int pad = std::max(32, n / 2);
    size_t nFft = 64;
    while (nFft < static_cast<size_t>(n + 2 * pad))
        nFft <<= 1;

    std::vector<double> buf(2 * nFft, 0.0);
    for (int i = 0; i < n; ++i)
        buf[2 * static_cast<size_t>(pad + i)] = detrended[static_cast<size_t>(i)];

    // Smooth raised-cosine taper of boundary values over outer pad/2 samples
    const int taperLen = std::max(4, std::min(pad, 24));
    const double v0 = detrended.front();
    const double vN = detrended.back();
    for (int k = 1; k <= taperLen && pad - k >= 0; ++k) {
        const double w = 0.5 * (1.0 + std::cos(M_PI * static_cast<double>(k) / taperLen));
        buf[2 * static_cast<size_t>(pad - k)] = v0 * w;
        if (static_cast<size_t>(pad + n - 1 + k) < nFft)
            buf[2 * static_cast<size_t>(pad + n - 1 + k)] = vN * w;
    }

    gsl_fft_complex_radix2_forward(buf.data(), 1, nFft);

    const double expFloorGauss = std::exp(-4.0);
    const double invDenomGauss = 1.0 / (1.0 - expFloorGauss);

    for (size_t k = 0; k < nFft; ++k) {
        const size_t m = (k <= nFft / 2) ? k : (nFft - k);
        const double absT = static_cast<double>(m) / (static_cast<double>(nFft) * dx);
        double weight = 0.0;
        if (absT < tMax) {
            const double u = absT / tMax;
            double apod = 1.0;
            if (apodization == 1) {
                apod = std::max(0.0, (std::exp(-4.0 * u * u) - expFloorGauss) * invDenomGauss);
            } else {
                const double oneMinusU = 1.0 - u;
                apod = oneMinusU * oneMinusU;
            }
            const double exponent = std::min(M_PI * kClamped, M_PI * lorentzFwhmGamma * absT);
            weight = std::exp(exponent) * apod;
        }
        buf[2 * k] *= weight;
        buf[2 * k + 1] *= weight;
    }

    gsl_fft_complex_radix2_inverse(buf.data(), 1, nFft);

    std::vector<double> fsdNet(static_cast<size_t>(n), 0.0);
    for (int i = 0; i < n; ++i)
        fsdNet[static_cast<size_t>(i)] = buf[2 * static_cast<size_t>(pad + i)];

    double fsdArea = 0.0;
    for (int i = 0; i + 1 < n; ++i) {
        fsdArea += 0.5 * (fsdNet[static_cast<size_t>(i)] + fsdNet[static_cast<size_t>(i + 1)])
                * (xs[static_cast<size_t>(i + 1)] - xs[static_cast<size_t>(i)]);
    }
    if (std::fabs(origNetArea) > 1e-12 && std::fabs(fsdArea) > 1e-12
        && (origNetArea * fsdArea > 0.0)) {
        const double ratio = origNetArea / fsdArea;
        if (ratio >= 0.80 && ratio <= 1.25) {
            for (double &v : fsdNet)
                v *= ratio;
        }
    }

    yOut.resize(static_cast<size_t>(n), 0.0);
    for (int i = 0; i < n; ++i)
        yOut[static_cast<size_t>(i)] = fsdNet[static_cast<size_t>(i)] + evalBase(xs[static_cast<size_t>(i)]);
    return true;
}

QList<Matrix *> collectProjectMatrices(ApplicationWindow *app)
{
    QList<Matrix *> list;
    if (!app)
        return list;
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("Matrix"))
            list.append(static_cast<Matrix *>(w));
    }
    return list;
}

Matrix *findActiveOrFirstMatrix(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    if (QMdiSubWindow *sub = app->d_workspace.activeSubWindow()) {
        if (sub->inherits("Matrix"))
            return static_cast<Matrix *>(sub);
        if (QWidget *w = sub->widget()) {
            if (w->inherits("Matrix"))
                return static_cast<Matrix *>(w);
        }
    }
    const QList<Matrix *> mats = collectProjectMatrices(app);
    return mats.isEmpty() ? nullptr : mats.first();
}

Graph *findActiveOrFirstSignalGraph(ApplicationWindow *app, Graph *explicitGraph)
{
    if (explicitGraph)
        return explicitGraph;
    if (!app)
        return nullptr;
    if (QMdiSubWindow *sub = app->d_workspace.activeSubWindow()) {
        if (sub->inherits("MultiLayer"))
            return static_cast<MultiLayer *>(sub)->activeGraph();
        if (QWidget *w = sub->widget()) {
            if (w->inherits("MultiLayer"))
                return static_cast<MultiLayer *>(w)->activeGraph();
        }
    }
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer")) {
            if (Graph *g = static_cast<MultiLayer *>(w)->activeGraph())
                return g;
        }
    }
    return nullptr;
}

} // namespace

Matrix *AdvancedSignalProc::filterMatrix2D(ApplicationWindow *app, Matrix *srcMatrix,
                                           MatrixFilter2DMethod method, double param,
                                           int radius, bool inPlace, bool plotHeatmap)
{
    registerSignalProcTranslationsOnce();
    if (!srcMatrix)
        return nullptr;
    if (!inPlace && !app)
        return nullptr;

    const int rows = srcMatrix->numRows();
    const int cols = srcMatrix->numCols();
    if (rows <= 0 || cols <= 0)
        return nullptr;

    std::vector<double> src(static_cast<size_t>(rows * cols), 0.0);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c)
            src[static_cast<size_t>(r * cols + c)] = srcMatrix->cell(r, c);
    }

    auto cellClamped = [&](const std::vector<double> &buf, int r, int c) -> double {
        const int rr = std::clamp(r, 0, rows - 1);
        const int cc = std::clamp(c, 0, cols - 1);
        return buf[static_cast<size_t>(rr * cols + cc)];
    };

    std::vector<double> dst(src.size(), 0.0);
    QString suffix = QStringLiteral("Filtered2D");

    switch (method) {
    case FilterGaussian2D: {
        suffix = QStringLiteral("Gauss2D");
        const double sigma = std::max(0.05, std::isfinite(param) ? param : 1.5);
        const int autoRad = std::max(1, static_cast<int>(std::ceil(3.0 * sigma)));
        const int rad = std::clamp((radius > 1) ? std::max(radius, autoRad) : autoRad, 1, 64);

        std::vector<double> kw(static_cast<size_t>(2 * rad + 1), 0.0);
        const double inv2s2 = 0.5 / (sigma * sigma);
        for (int d = -rad; d <= rad; ++d)
            kw[static_cast<size_t>(d + rad)] = std::exp(-static_cast<double>(d * d) * inv2s2);

        std::vector<double> tmp(src.size(), 0.0);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                double wSum = 0.0, vSum = 0.0;
                for (int d = -rad; d <= rad; ++d) {
                    const double v = cellClamped(src, r, c + d);
                    if (std::isfinite(v)) {
                        const double w = kw[static_cast<size_t>(d + rad)];
                        wSum += w;
                        vSum += w * v;
                    }
                }
                tmp[static_cast<size_t>(r * cols + c)] =
                        (wSum > 0.0) ? (vSum / wSum) : src[static_cast<size_t>(r * cols + c)];
            }
        }
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                double wSum = 0.0, vSum = 0.0;
                for (int d = -rad; d <= rad; ++d) {
                    const double v = cellClamped(tmp, r + d, c);
                    if (std::isfinite(v)) {
                        const double w = kw[static_cast<size_t>(d + rad)];
                        wSum += w;
                        vSum += w * v;
                    }
                }
                dst[static_cast<size_t>(r * cols + c)] =
                        (wSum > 0.0) ? (vSum / wSum) : tmp[static_cast<size_t>(r * cols + c)];
            }
        }
        break;
    }
    case FilterMedian2D: {
        suffix = QStringLiteral("Median2D");
        const int rad = std::clamp(radius, 1, 32);
        std::vector<double> win;
        win.reserve(static_cast<size_t>((2 * rad + 1) * (2 * rad + 1)));
        for (int r = 0; r < rows; ++r) {
            const int r0 = std::max(0, r - rad);
            const int r1 = std::min(rows - 1, r + rad);
            for (int c = 0; c < cols; ++c) {
                const int c0 = std::max(0, c - rad);
                const int c1 = std::min(cols - 1, c + rad);
                win.clear();
                for (int rr = r0; rr <= r1; ++rr) {
                    for (int cc = c0; cc <= c1; ++cc) {
                        const double v = src[static_cast<size_t>(rr * cols + cc)];
                        if (std::isfinite(v))
                            win.push_back(v);
                    }
                }
                if (win.empty()) {
                    dst[static_cast<size_t>(r * cols + c)] = src[static_cast<size_t>(r * cols + c)];
                } else {
                    const size_t mid = win.size() / 2;
                    std::nth_element(win.begin(), win.begin() + static_cast<ptrdiff_t>(mid),
                                     win.end());
                    double med = win[mid];
                    if ((win.size() & 1U) == 0U) {
                        const double lowerMax = *std::max_element(
                                win.begin(), win.begin() + static_cast<ptrdiff_t>(mid));
                        med = 0.5 * (lowerMax + med);
                    }
                    dst[static_cast<size_t>(r * cols + c)] = med;
                }
            }
        }
        break;
    }
    case FilterLaplacianSharpen2D: {
        suffix = QStringLiteral("Sharpen2D");
        const double alpha = std::max(0.0, std::isfinite(param) ? param : 1.0);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const double z0 = cellClamped(src, r, c);
                if (!std::isfinite(z0)) {
                    dst[static_cast<size_t>(r * cols + c)] = z0;
                    continue;
                }
                double nSum = 0.0;
                int nCnt = 0;
                const double nb[4] = {
                    cellClamped(src, r - 1, c), cellClamped(src, r + 1, c),
                    cellClamped(src, r, c - 1), cellClamped(src, r, c + 1)
                };
                for (double v : nb) {
                    if (std::isfinite(v)) {
                        nSum += v;
                        ++nCnt;
                    }
                }
                const double nAvg = (nCnt > 0) ? (nSum / static_cast<double>(nCnt)) : z0;
                dst[static_cast<size_t>(r * cols + c)] = z0 + alpha * (z0 - nAvg);
            }
        }
        break;
    }
    case FilterSobelGradient2D: {
        suffix = QStringLiteral("SobelGrad2D");
        const double spanX = std::fabs(srcMatrix->xEnd() - srcMatrix->xStart());
        const double spanY = std::fabs(srcMatrix->yEnd() - srcMatrix->yStart());
        const double dx = (cols > 1 && spanX > 1e-15) ? (spanX / static_cast<double>(cols - 1)) : 1.0;
        const double dy = (rows > 1 && spanY > 1e-15) ? (spanY / static_cast<double>(rows - 1)) : 1.0;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                auto safeVal = [&](int rr, int cc, double fallback) {
                    const double v = cellClamped(src, rr, cc);
                    return std::isfinite(v) ? v : fallback;
                };
                const double cVal = std::isfinite(cellClamped(src, r, c)) ? cellClamped(src, r, c) : 0.0;
                const double z00 = safeVal(r - 1, c - 1, cVal);
                const double z01 = safeVal(r - 1, c, cVal);
                const double z02 = safeVal(r - 1, c + 1, cVal);
                const double z10 = safeVal(r, c - 1, cVal);
                const double z12 = safeVal(r, c + 1, cVal);
                const double z20 = safeVal(r + 1, c - 1, cVal);
                const double z21 = safeVal(r + 1, c, cVal);
                const double z22 = safeVal(r + 1, c + 1, cVal);
                const double gx = ((z02 + 2.0 * z12 + z22) - (z00 + 2.0 * z10 + z20)) / (8.0 * dx);
                const double gy = ((z20 + 2.0 * z21 + z22) - (z00 + 2.0 * z01 + z02)) / (8.0 * dy);
                dst[static_cast<size_t>(r * cols + c)] = std::hypot(gx, gy);
            }
        }
        break;
    }
    case FilterSubtractPlanarTilt2D: {
        suffix = QStringLiteral("LevelPlane2D");
        const double cMid = 0.5 * static_cast<double>(cols - 1);
        const double rMid = 0.5 * static_cast<double>(rows - 1);
        double A[3][4] = {};
        int validCnt = 0;
        for (int r = 0; r < rows; ++r) {
            const double v = static_cast<double>(r) - rMid;
            for (int c = 0; c < cols; ++c) {
                const double z = src[static_cast<size_t>(r * cols + c)];
                if (!std::isfinite(z))
                    continue;
                const double u = static_cast<double>(c) - cMid;
                ++validCnt;
                A[0][0] += u * u;
                A[0][1] += u * v;
                A[0][2] += u;
                A[0][3] += u * z;
                A[1][1] += v * v;
                A[1][2] += v;
                A[1][3] += v * z;
                A[2][2] += 1.0;
                A[2][3] += z;
            }
        }
        A[1][0] = A[0][1];
        A[2][0] = A[0][2];
        A[2][1] = A[1][2];

        double coef[3] = { 0.0, 0.0, 0.0 };
        if (validCnt >= 3) {
            // Solve 3x3 normal equations (with degenerate-dimension fallback)
            bool solved = true;
            for (int col = 0; col < 3; ++col) {
                int piv = col;
                for (int row = col + 1; row < 3; ++row) {
                    if (std::fabs(A[row][col]) > std::fabs(A[piv][col]))
                        piv = row;
                }
                if (std::fabs(A[piv][col]) <= 1e-24) {
                    solved = false;
                    break;
                }
                if (piv != col) {
                    for (int k = col; k < 4; ++k)
                        std::swap(A[col][k], A[piv][k]);
                }
                const double invD = 1.0 / A[col][col];
                for (int k = col; k < 4; ++k)
                    A[col][k] *= invD;
                for (int row = 0; row < 3; ++row) {
                    if (row == col)
                        continue;
                    const double f = A[row][col];
                    for (int k = col; k < 4; ++k)
                        A[row][k] -= f * A[col][k];
                }
            }
            if (solved) {
                coef[0] = A[0][3];
                coef[1] = A[1][3];
                coef[2] = A[2][3];
            } else if (A[2][2] > 0.0) {
                coef[2] = A[2][3] / A[2][2];
            }
        }

        for (int r = 0; r < rows; ++r) {
            const double v = static_cast<double>(r) - rMid;
            for (int c = 0; c < cols; ++c) {
                const double z = src[static_cast<size_t>(r * cols + c)];
                if (!std::isfinite(z)) {
                    dst[static_cast<size_t>(r * cols + c)] = z;
                } else {
                    const double u = static_cast<double>(c) - cMid;
                    dst[static_cast<size_t>(r * cols + c)] = z - (coef[0] * u + coef[1] * v + coef[2]);
                }
            }
        }
        break;
    }
    }

    Matrix *targetMat = srcMatrix;
    if (!inPlace) {
        targetMat = app->newMatrix(uniqueWinName(app, srcMatrix->name() + suffix), rows, cols);
        if (!targetMat)
            return nullptr;
        targetMat->setCoordinates(srcMatrix->xStart(), srcMatrix->xEnd(),
                                  srcMatrix->yStart(), srcMatrix->yEnd());
    }

    targetMat->d_future_matrix->beginMacro(QObject::tr("2D Matrix Filter"));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c)
            targetMat->setCell(r, c, dst[static_cast<size_t>(r * cols + c)]);
    }
    targetMat->d_future_matrix->endMacro();

    if (inPlace && app) {
        for (MyWidget *w : app->windowsList()) {
            if (!w || !w->inherits("MultiLayer"))
                continue;
            MultiLayer *ml = static_cast<MultiLayer *>(w);
            for (Graph *gr : ml->layersList()) {
                if (!gr || !gr->plotWidget())
                    continue;
                for (QwtPlotItem *it : gr->plotWidget()->itemList()) {
                    if (auto *sp = dynamic_cast<Spectrogram *>(it)) {
                        if (sp->matrix() == targetMat)
                            sp->updateData(targetMat);
                    }
                }
            }
        }
    }

    if (plotHeatmap && app) {
        if (MultiLayer *ml = app->plotSpectrogram(targetMat, Graph::ColorMap)) {
            if (Graph *gr = ml->activeGraph()) {
                const QFont titleFnt(QStringLiteral("Arial"), 11, QFont::Bold);
                const QFont tickFnt(QStringLiteral("Arial"), 10);
                gr->initFonts(titleFnt, tickFnt);
                gr->setTitleFont(QFont(QStringLiteral("Arial"), 12, QFont::Bold));
                gr->setRightAxisTitleFont(titleFnt);
                gr->setAxisFont(QwtPlot::yRight, tickFnt);
                gr->replot();
            }
        }
    }

    return targetMat;
}

QwtPlotCurve *AdvancedSignalProc::fourierSelfDeconvolve(
        ApplicationWindow *app, Graph *g, const QString &curveTitle,
        double lorentzFwhmGamma, double sharpeningFactorK, int apodization)
{
    registerSignalProcTranslationsOnce();
    if (!app || !g)
        return nullptr;

    QwtPlotCurve *srcCurve = resolveSignalCurve(g, curveTitle);
    std::vector<double> xs, ys;
    double fs = 1.0;
    if (!extractUniformSignal(g, curveTitle, xs, ys, fs) || xs.size() < 4)
        return nullptr;
    reportSignalPrep(app, QStringLiteral("FSD"), curveTitle, s_lastSignalPrepNote);

    std::vector<double> fsdY;
    if (!computeFsdSpectrum(xs, ys, lorentzFwhmGamma, sharpeningFactorK, apodization, fsdY))
        return nullptr;

    const int n = static_cast<int>(xs.size());
    Table *tbl = buildSignalTable(
            app, QStringLiteral("FSDSharp"),
            QStringList() << QStringLiteral("X") << QStringLiteral("FSDSharpened"),
            QList<SciDAVis::PlotDesignation>() << SciDAVis::X << SciDAVis::Y,
            { xs, fsdY });
    if (!tbl)
        return nullptr;

    if (!g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::Line, 0, n - 1))
        return nullptr;

    QwtPlotCurve *resCurve = g->curve(g->curves() - 1);
    if (resCurve) {
        resCurve->setPen(QPen(QColor(195, 30, 65), 2.0, Qt::SolidLine));
        if (srcCurve)
            resCurve->setAxis(srcCurve->xAxis(), srcCurve->yAxis());
    }
    g->updatePlot();
    return resCurve;
}

void AdvancedSignalProc::showMatrixFilter2DDialog(ApplicationWindow *app)
{
    if (!app)
        return;
    registerSignalProcTranslationsOnce();
    const QList<Matrix *> matrices = collectProjectMatrices(app);
    if (matrices.isEmpty()) {
        QMessageBox::information(app, QObject::tr("2D Spatial Filter & Plane Leveling"),
                                 QObject::tr("Please open a Matrix window first."));
        return;
    }

    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("2D Spatial Filter & Plane Leveling"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbMatrix = new QComboBox(&dlg);
    Matrix *activeMat = findActiveOrFirstMatrix(app);
    int initIdx = 0;
    for (int i = 0; i < matrices.size(); ++i) {
        cbMatrix->addItem(matrices[i]->name());
        if (matrices[i] == activeMat)
            initIdx = i;
    }
    cbMatrix->setCurrentIndex(initIdx);

    QComboBox *cbMethod = new QComboBox(&dlg);
    cbMethod->addItem(QObject::tr("Gaussian Smoothing (2D)"),
                      static_cast<int>(FilterGaussian2D));
    cbMethod->addItem(QObject::tr("Median Despike Filter (Cosmic-Ray Removal)"),
                      static_cast<int>(FilterMedian2D));
    cbMethod->addItem(QObject::tr("Laplacian Sharpening (2D)"),
                      static_cast<int>(FilterLaplacianSharpen2D));
    cbMethod->addItem(QObject::tr("Sobel Gradient Magnitude (|∇Z|)"),
                      static_cast<int>(FilterSobelGradient2D));
    cbMethod->addItem(QObject::tr("Subtract Least-Squares Planar Tilt (AFM/STM Leveling)"),
                      static_cast<int>(FilterSubtractPlanarTilt2D));

    QLabel *lblParam = new QLabel(QObject::tr("Gaussian Sigma (pixels):"), &dlg);
    QDoubleSpinBox *sbParam = new QDoubleSpinBox(&dlg);
    sbParam->setRange(0.05, 100.0);
    sbParam->setDecimals(3);
    sbParam->setSingleStep(0.25);
    sbParam->setValue(1.5);

    QSpinBox *sbRadius = new QSpinBox(&dlg);
    sbRadius->setRange(1, 32);
    sbRadius->setValue(2);

    QCheckBox *chkInPlace =
            new QCheckBox(QObject::tr("Modify Source Matrix In-Place (Undoable)"), &dlg);
    chkInPlace->setChecked(false);
    QCheckBox *chkPlot = new QCheckBox(QObject::tr("Plot Filtered 2D Heatmap"), &dlg);
    chkPlot->setChecked(true);

    auto syncUi = [&](int idx) {
        const int m = cbMethod->itemData(idx).toInt();
        if (m == static_cast<int>(FilterGaussian2D)) {
            lblParam->setText(QObject::tr("Gaussian Sigma (pixels):"));
            sbParam->setEnabled(true);
            sbRadius->setEnabled(true);
        } else if (m == static_cast<int>(FilterMedian2D)) {
            lblParam->setText(QObject::tr("Parameter:"));
            sbParam->setEnabled(false);
            sbRadius->setEnabled(true);
        } else if (m == static_cast<int>(FilterLaplacianSharpen2D)) {
            lblParam->setText(QObject::tr("Sharpening Strength:"));
            sbParam->setEnabled(true);
            sbRadius->setEnabled(false);
        } else {
            lblParam->setText(QObject::tr("Parameter:"));
            sbParam->setEnabled(false);
            sbRadius->setEnabled(false);
        }
    };
    syncUi(0);
    QObject::connect(cbMethod, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, syncUi);

    form->addRow(QObject::tr("Source Matrix:"), cbMatrix);
    form->addRow(QObject::tr("2D Filter Method:"), cbMethod);
    form->addRow(lblParam, sbParam);
    form->addRow(QObject::tr("Kernel Radius (pixels):"), sbRadius);
    form->addRow(QString(), chkInPlace);
    form->addRow(QString(), chkPlot);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const int matIdx = cbMatrix->currentIndex();
        if (matIdx >= 0 && matIdx < matrices.size()) {
            filterMatrix2D(app, matrices[matIdx],
                           static_cast<MatrixFilter2DMethod>(cbMethod->currentData().toInt()),
                           sbParam->value(), sbRadius->value(),
                           chkInPlace->isChecked(), chkPlot->isChecked());
        }
    }
}

void AdvancedSignalProc::showFourierSelfDeconvolveDialog(ApplicationWindow *app, Graph *g)
{
    if (!app)
        return;
    registerSignalProcTranslationsOnce();
    Graph *targetGraph = findActiveOrFirstSignalGraph(app, g);
    if (!targetGraph || targetGraph->analysableCurvesList().isEmpty()) {
        QMessageBox::information(
                app, QObject::tr("Fourier Self-Deconvolution (Peak Sharpening)"),
                QObject::tr("Please open a graph with at least one curve first."));
        return;
    }

    const QStringList curves = targetGraph->analysableCurvesList();
    QDialog dlg(app);
    dlg.setWindowTitle(QObject::tr("Fourier Self-Deconvolution (Peak Sharpening)"));
    QFormLayout *form = new QFormLayout();

    QComboBox *cbCurve = new QComboBox(&dlg);
    cbCurve->addItems(curves);

    auto estimateDefaultGamma = [&](const QString &cname) {
        std::vector<double> xs, ys;
        double fs = 1.0;
        if (!extractUniformSignal(targetGraph, cname, xs, ys, fs) || xs.size() < 4)
            return 10.0;
        const double span = std::fabs(xs.back() - xs.front());
        const double dx = span / static_cast<double>(xs.size() - 1);
        return std::max(4.0 * dx, 0.04 * span);
    };

    QDoubleSpinBox *sbGamma = new QDoubleSpinBox(&dlg);
    sbGamma->setRange(1e-9, 1e12);
    sbGamma->setDecimals(6);
    sbGamma->setValue(estimateDefaultGamma(curves.first()));

    QDoubleSpinBox *sbK = new QDoubleSpinBox(&dlg);
    sbK->setRange(1.05, 3.50);
    sbK->setDecimals(2);
    sbK->setSingleStep(0.10);
    sbK->setValue(1.80);

    QComboBox *cbApod = new QComboBox(&dlg);
    cbApod->addItem(QObject::tr("Bessel / Kauppinen (1 - |t|/t_max)^2"), 0);
    cbApod->addItem(QObject::tr("Gaussian Apodization"), 1);

    QCheckBox *chkPreview = new QCheckBox(QObject::tr("Live Preview"), &dlg);
    chkPreview->setChecked(true);

    SignalPreviewGuard previewGuard(targetGraph);
    auto refreshPreview = [&]() {
        if (!chkPreview->isChecked()) {
            previewGuard.clear();
            return;
        }
        std::vector<double> xs, ys, fsdY;
        double fs = 1.0;
        if (!extractUniformSignal(targetGraph, cbCurve->currentText(), xs, ys, fs)
            || !computeFsdSpectrum(xs, ys, sbGamma->value(), sbK->value(),
                                   cbApod->currentData().toInt(), fsdY)) {
            previewGuard.clear();
            return;
        }
        previewGuard.setSingle(xs, fsdY, QPen(QColor(195, 30, 65), 1.9, Qt::DashLine));
    };

    QObject::connect(cbCurve, &QComboBox::currentTextChanged, &dlg, [&](const QString &cn) {
        sbGamma->setValue(estimateDefaultGamma(cn));
        refreshPreview();
    });
    QObject::connect(sbGamma, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                     [&](double) { refreshPreview(); });
    QObject::connect(sbK, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                     [&](double) { refreshPreview(); });
    QObject::connect(cbApod, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refreshPreview(); });
    QObject::connect(chkPreview, &QCheckBox::toggled, &dlg, [&](bool) { refreshPreview(); });

    form->addRow(QObject::tr("Curve:"), cbCurve);
    form->addRow(QObject::tr("Lorentzian FWHM γ (X units):"), sbGamma);
    form->addRow(QObject::tr("Sharpening Factor K (t_max = K / γ):"), sbK);
    form->addRow(QObject::tr("Apodization Function:"), cbApod);
    form->addRow(QString(), chkPreview);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    refreshPreview();

    if (dlg.exec() == QDialog::Accepted) {
        previewGuard.clear();
        fourierSelfDeconvolve(app, targetGraph, cbCurve->currentText(),
                              sbGamma->value(), sbK->value(), cbApod->currentData().toInt());
    }
}
